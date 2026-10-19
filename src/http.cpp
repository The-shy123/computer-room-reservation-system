#include "seat/service.hpp"
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <drogon/drogon.h>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <set>
#include <trantor/utils/ConcurrentTaskQueue.h>
using namespace seat;
using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
namespace {
using Callback = std::function<void(const HttpResponsePtr &)>;
using Operation = std::function<Json::Value(Service &, const HttpRequestPtr &)>;
HttpResponsePtr response(const Json::Value &value, int status = 200) {
    auto r = drogon::HttpResponse::newHttpJsonResponse(value);
    r->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
    r->addHeader("Cache-Control", "no-store");
    return r;
}
HttpResponsePtr failure(int status, const std::string &code, const std::string &message) {
    Json::Value j;
    j["error"]["code"] = code;
    j["error"]["message"] = message;
    auto r = response(j, status);
    if (status == 503)
        r->addHeader("Retry-After", "1");
    return r;
}
int http_status(Code c) {
    switch (c) {
    case Code::InvalidInput:
        return 400;
    case Code::Unauthorized:
        return 401;
    case Code::Forbidden:
        return 403;
    case Code::NotFound:
        return 404;
    case Code::Full:
    case Code::Conflict:
        return 409;
    case Code::Busy:
        return 503;
    case Code::Storage:
        return 500;
    }
    return 500;
}
std::string token(const HttpRequestPtr &req) {
    const auto h = req->getHeader("authorization");
    if (h.substr(0, 7) != "Bearer ")
        throw Error(Code::Unauthorized, "Bearer session required");
    return h.substr(7);
}
Json::Value body(const HttpRequestPtr &req, std::initializer_list<const char *> allowed) {
    auto value = req->getJsonObject();
    if (!value || !value->isObject())
        throw Error(Code::InvalidInput, "JSON object required");
    std::set<std::string> keys;
    for (auto key : allowed)
        keys.insert(key);
    for (const auto &key : value->getMemberNames())
        if (!keys.count(key))
            throw Error(Code::InvalidInput, "unknown JSON field");
    return *value;
}
std::string text_field(const Json::Value &j, const char *key) {
    if (!j[key].isString())
        throw Error(Code::InvalidInput, std::string(key) + " must be a string");
    return j[key].asString();
}
Id integer(const Json::Value &j, const char *key) {
    if (!j[key].isInt64())
        throw Error(Code::InvalidInput, std::string(key) + " must be an integer");
    return j[key].asInt64();
}
int small_integer(const Json::Value &j, const char *key) {
    if (!j[key].isInt())
        throw Error(Code::InvalidInput, std::string(key) + " integer out of range");
    return j[key].asInt();
}
Id number(const std::string &s) {
    Id n = 0;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), n);
    if (s.empty() || result.ec != std::errc{} || result.ptr != s.data() + s.size() || n < 1)
        throw Error(Code::InvalidInput, "positive integer required");
    return n;
}
Slot slot(const Json::Value &j) {
    const auto n = integer(j, "slot");
    if (n != 1 && n != 2)
        throw Error(Code::InvalidInput, "slot must be 1 or 2");
    return static_cast<Slot>(n);
}
Status status(const std::string &s) {
    for (auto v : {Status::Pending, Status::Approved, Status::Rejected, Status::Cancelled})
        if (s == name(v))
            return v;
    throw Error(Code::InvalidInput, "unknown status");
}
Json::Value json(const Reservation &r) {
    Json::Value j;
    j["id"] = Json::Int64(r.id);
    j["student_id"] = Json::Int64(r.student_id);
    j["room_id"] = Json::Int64(r.room_id);
    j["date"] = r.date;
    j["slot"] = static_cast<int>(r.slot);
    j["status"] = name(r.status);
    return j;
}
Json::Value json(const User &u) {
    Json::Value j;
    j["id"] = Json::Int64(u.id);
    j["name"] = u.name;
    j["role"] = static_cast<int>(u.role);
    return j;
}
struct Runtime : std::enable_shared_from_this<Runtime> {
    std::string database;
    trantor::ConcurrentTaskQueue workers{4, "seat-db"};
    std::atomic<int> pending{0};
    std::mutex mutex;
    std::condition_variable drained;
    explicit Runtime(std::string path) : database(std::move(path)) {}
    void finished() {
        std::lock_guard<std::mutex> lock(mutex);
        --pending;
        drained.notify_all();
    }
    void dispatch(HttpRequestPtr req, Callback cb, Operation op) {
        // Bound both running and queued requests. Never block an HTTP event loop.
        if (pending.fetch_add(1) >= 128) {
            finished();
            cb(failure(503, "overloaded", "request queue full"));
            return;
        }
        auto self = shared_from_this();
        try {
            workers.runTaskInQueue([self, req = std::move(req), cb = std::move(cb),
                                    op = std::move(op)] {
                struct Completion {
                    std::shared_ptr<Runtime> runtime;
                    ~Completion() { runtime->finished(); }
                } completion{self};
                HttpResponsePtr result;
                try {
                    Service service(self->database);
                    result = response(op(service, req));
                } catch (const Error &e) {
                    result =
                        failure(http_status(e.code), name(e.code),
                                e.code == Code::Storage ? "storage operation failed" : e.what());
                } catch (const std::exception &) {
                    result = failure(500, "internal_error", "request failed");
                }
                // Callback and request are owned values. Drogon dispatches the response
                // to its IO loop.
                cb(result);
            });
        } catch (...) {
            finished();
            throw;
        }
    }
    void drain() {
        std::unique_lock<std::mutex> lock(mutex);
        drained.wait(lock, [&] { return pending.load() == 0; });
        lock.unlock();
        workers.stop();
    }
};
void route(const std::shared_ptr<Runtime> &runtime, const std::string &path,
           drogon::HttpMethod method, Operation op) {
    drogon::app().registerHandler(
        path,
        [runtime, op = std::move(op)](const HttpRequestPtr &req, Callback &&cb) {
            runtime->dispatch(req, std::move(cb), op);
        },
        {method});
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::string database, host = "127.0.0.1";
        int port = 8080;
        for (int i = 1; i < argc; ++i) {
            if (i + 1 >= argc)
                throw std::runtime_error("missing option value");
            std::string key = argv[i], value = argv[++i];
            if (key == "--db")
                database = value;
            else if (key == "--host")
                host = value;
            else if (key == "--port") {
                auto n = number(value);
                if (n > 65535)
                    throw std::runtime_error("invalid port");
                port = static_cast<int>(n);
            } else
                throw std::runtime_error("unknown option");
        }
        if (database.empty() || !std::filesystem::path(database).is_absolute())
            throw std::runtime_error("--db /absolute/path/seat.db required");
        {
            Service initialize(database);
        }
        auto runtime = std::make_shared<Runtime>(database);
        route(runtime, "/api/login", drogon::Post, [](Service &svc, const HttpRequestPtr &req) {
            auto j = body(req, {"name", "password"});
            Json::Value out;
            out["token"] = svc.login(text_field(j, "name"), text_field(j, "password"));
            out["expires_in"] = 28800;
            return out;
        });
        route(runtime, "/api/logout", drogon::Post, [](Service &svc, const HttpRequestPtr &req) {
            svc.logout(token(req));
            Json::Value out;
            out["ok"] = true;
            return out;
        });
        route(runtime, "/api/me", drogon::Get, [](Service &svc, const HttpRequestPtr &req) {
            return json(svc.identity(token(req)));
        });
        route(runtime, "/api/rooms", drogon::Get, [](Service &svc, const HttpRequestPtr &req) {
            auto n = number(req->getParameter("slot"));
            if (n > 2)
                throw Error(Code::InvalidInput, "slot must be 1 or 2");
            Json::Value out(Json::arrayValue);
            for (auto a :
                 svc.availability(token(req), req->getParameter("date"), static_cast<Slot>(n))) {
                Json::Value j;
                j["id"] = Json::Int64(a.room.id);
                j["name"] = a.room.name;
                j["capacity"] = a.capacity;
                j["occupied"] = a.occupied;
                j["remaining"] = a.capacity - a.occupied;
                out.append(j);
            }
            return out;
        });
        route(runtime, "/api/reservations", drogon::Get,
              [](Service &svc, const HttpRequestPtr &req) {
                  Json::Value out(Json::arrayValue);
                  for (auto r : svc.reservations(token(req)))
                      out.append(json(r));
                  return out;
              });
        route(runtime, "/api/reservations/detail", drogon::Get,
              [](Service &svc, const HttpRequestPtr &req) {
                  return json(svc.get(token(req), number(req->getParameter("id"))));
              });
        route(runtime, "/api/reservations", drogon::Post,
              [](Service &svc, const HttpRequestPtr &req) {
                  auto j = body(req, {"request_id", "room_id", "date", "slot"});
                  return json(svc.apply(token(req), text_field(j, "request_id"),
                                        integer(j, "room_id"), text_field(j, "date"), slot(j)));
              });
        route(runtime, "/api/reservations/cancel", drogon::Post,
              [](Service &svc, const HttpRequestPtr &req) {
                  auto j = body(req, {"id", "expected"});
                  return json(svc.change(token(req), integer(j, "id"),
                                         status(text_field(j, "expected")), Status::Cancelled));
              });
        route(runtime, "/api/reservations/review", drogon::Post,
              [](Service &svc, const HttpRequestPtr &req) {
                  auto j = body(req, {"id", "expected", "target"});
                  auto target = status(text_field(j, "target"));
                  if (target != Status::Approved && target != Status::Rejected)
                      throw Error(Code::InvalidInput, "review target must be Approved or Rejected");
                  return json(svc.change(token(req), integer(j, "id"),
                                         status(text_field(j, "expected")), target));
              });
        route(runtime, "/api/users", drogon::Get, [](Service &svc, const HttpRequestPtr &req) {
            Json::Value out(Json::arrayValue);
            for (auto u : svc.users(token(req)))
                out.append(json(u));
            return out;
        });
        route(runtime, "/api/users", drogon::Post, [](Service &svc, const HttpRequestPtr &req) {
            auto j = body(req, {"name", "password", "role"});
            const auto r = integer(j, "role");
            if (r < 0 || r > 2)
                throw Error(Code::InvalidInput, "role must be 0, 1, or 2");
            return json(svc.add_user(token(req), text_field(j, "name"), text_field(j, "password"),
                                     static_cast<Role>(r)));
        });
        route(runtime, "/api/rooms", drogon::Post, [](Service &svc, const HttpRequestPtr &req) {
            auto j = body(req, {"name", "capacity"});
            auto r = svc.add_room(token(req), text_field(j, "name"), small_integer(j, "capacity"));
            Json::Value out;
            out["id"] = Json::Int64(r.id);
            out["name"] = r.name;
            out["capacity"] = r.capacity;
            return out;
        });
        route(runtime, "/api/rooms/capacity", drogon::Post,
              [](Service &svc, const HttpRequestPtr &req) {
                  auto j = body(req, {"room_id", "date", "slot", "capacity"});
                  svc.set_capacity(token(req), integer(j, "room_id"), text_field(j, "date"),
                                   slot(j), small_integer(j, "capacity"));
                  Json::Value out;
                  out["ok"] = true;
                  return out;
              });
        drogon::app().registerHandler("/health",
                                      [](const HttpRequestPtr &, Callback &&cb) {
                                          Json::Value j;
                                          j["ok"] = true;
                                          cb(response(j));
                                      },
                                      {drogon::Get});
        drogon::app().setCustomErrorHandler([](drogon::HttpStatusCode c, const HttpRequestPtr &) {
            return failure(static_cast<int>(c), "http_error", "HTTP request rejected");
        });
        drogon::app()
            .setLogLevel(trantor::Logger::kWarn)
            .setThreadNum(2)
            .setClientMaxBodySize(16384)
            .setIdleConnectionTimeout(30)
            .addListener(host, static_cast<std::uint16_t>(port));
        std::cout << "Listening on " << host << ":" << port << " (local demonstration; no TLS)"
                  << std::endl;
        drogon::app().run();
        runtime->drain();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
