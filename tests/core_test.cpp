#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "seat/service.hpp"
#include "test_support.hpp"
#include <fstream>
using namespace seat;
static bool code(const std::function<void()> &f, Code c) {
    try {
        f();
    } catch (const Error &e) {
        return e.code == c;
    }
    return false;
}
struct Fixture {
    TempDatabase database;
    Service service{database.path, [] { return LocalTime{"2026-10-19", 7 * 60}; }};
    std::string admin, student, other, teacher;
    Fixture() {
        service.bootstrap("adminpass");
        admin = service.login("admin", "adminpass");
        service.add_user(admin, "s", "studentpass", Role::Student);
        service.add_user(admin, "o", "otherpass", Role::Student);
        service.add_user(admin, "t", "teacherpass", Role::Teacher);
        student = service.login("s", "studentpass");
        other = service.login("o", "otherpass");
        teacher = service.login("t", "teacherpass");
        service.add_room(admin, "lab", 1);
        service.add_room(admin, "lab2", 2);
    }
};
TEST_CASE("Gregorian date, weekday, actual weeks, exact start boundary") {
    CHECK_NOTHROW(validate_date("2028-02-29"));
    CHECK_THROWS(validate_date("2026-02-29"));
    CHECK_THROWS(validate_date("2026-13-01"));
    CHECK_THROWS(validate_date("2026-1-01"));
    CHECK_THROWS(validate_date("2026-04-31"));
    CHECK_NOTHROW(validate_slot("2026-10-19", Slot::Morning, {"2026-10-19", 479}));
    CHECK_THROWS(validate_slot("2026-10-19", Slot::Morning, {"2026-10-19", 480}));
    CHECK_THROWS(validate_slot("2026-10-18", Slot::Morning, {"2026-10-19", 0}));
    CHECK_THROWS(validate_slot("2026-10-24", Slot::Morning, {"2026-10-19", 0}));
    CHECK_THROWS(validate_slot("2026-10-19", static_cast<Slot>(99), {"2026-10-19", 0}));
    CHECK_NOTHROW(validate_slot("2026-10-26", Slot::Morning, {"2026-10-19", 0}));
}
TEST_CASE("all sixteen state transitions") {
    for (auto f : {Status::Pending, Status::Approved, Status::Rejected, Status::Cancelled})
        for (auto t : {Status::Pending, Status::Approved, Status::Rejected, Status::Cancelled}) {
            const bool legal =
                (f == Status::Pending &&
                 (t == Status::Approved || t == Status::Rejected || t == Status::Cancelled)) ||
                (f == Status::Approved && t == Status::Cancelled);
            CHECK(can_transition(f, t) == legal);
        }
}
TEST_CASE_FIXTURE(Fixture, "capacity, cross-room duplication, ownership, release exactly once") {
    auto r = service.apply(student, "a", 1, "2026-10-19", Slot::Morning);
    CHECK(r.status == Status::Pending);
    CHECK(code([&] { service.apply(other, "b", 1, r.date, r.slot); }, Code::Full));
    CHECK(code([&] { service.apply(student, "b", 2, r.date, r.slot); }, Code::Conflict));
    CHECK(code([&] { service.get(other, r.id); }, Code::Forbidden));
    CHECK(service.reservations(other).empty());
    CHECK(code([&] { service.get(admin, r.id); }, Code::Forbidden));
    CHECK(code([&] { service.reservations(admin); }, Code::Forbidden));
    CHECK(code([&] { service.change(student, r.id, Status::Pending, Status::Approved); },
               Code::Forbidden));
    CHECK(code([&] { service.set_capacity(admin, 1, r.date, r.slot, 0); }, Code::Conflict));
    CHECK(service.change(teacher, r.id, Status::Pending, Status::Approved).status ==
          Status::Approved);
    CHECK(code([&] { service.change(teacher, r.id, Status::Pending, Status::Approved); },
               Code::Conflict));
    CHECK(service.change(student, r.id, Status::Approved, Status::Cancelled).status ==
          Status::Cancelled);
    CHECK(code([&] { service.change(student, r.id, Status::Approved, Status::Cancelled); },
               Code::Conflict));
    CHECK(service.apply(other, "b", 1, r.date, r.slot).id > r.id);
}
TEST_CASE_FIXTURE(Fixture, "idempotency and service permissions") {
    auto r = service.apply(student, "a", 1, "2026-10-19", Slot::Morning);
    CHECK(service.apply(student, "a", 1, r.date, r.slot).id == r.id);
    CHECK(code([&] { service.apply(student, "a", 2, r.date, r.slot); }, Code::Conflict));
    CHECK(code([&] { service.apply(teacher, "a", 2, r.date, r.slot); }, Code::Forbidden));
    CHECK(code([&] { service.add_room(student, "x", 2); }, Code::Forbidden));
    CHECK(code([&] { service.users(student); }, Code::Forbidden));
    CHECK(code([&] { service.identity("forged"); }, Code::Unauthorized));
    CHECK(code([&] { service.apply(other, "x", 99, r.date, r.slot); }, Code::NotFound));
    CHECK(service.change(teacher, r.id, Status::Pending, Status::Rejected).status ==
          Status::Rejected);
    CHECK(code([&] { service.change(student, r.id, Status::Rejected, Status::Cancelled); },
               Code::Conflict));
    CHECK(service.availability(other, r.date, r.slot).front().occupied == 0);
}
TEST_CASE_FIXTURE(Fixture, "SQLite schema foreign keys constraints reopen and password storage") {
    db::Connection db(database.path);
    CHECK(scalar(db, "PRAGMA foreign_keys") == 1);
    CHECK(sqlite3_libversion_number() == SQLITE_VERSION_NUMBER);
    CHECK(scalar(db, "PRAGMA user_version") == 1);
    db::migrate(db);
    CHECK(scalar(db, "PRAGMA user_version") == 1);
    CHECK_THROWS(db.exec("INSERT INTO reservations(student_id,room_id,date,slot,status) "
                         "VALUES(999,999,'2026-10-19',1,0)"));
    CHECK_THROWS(db.exec("UPDATE rooms SET capacity=-1"));
    db::Statement q(db, "SELECT password_hash FROM users WHERE name='s'");
    REQUIRE(q.row());
    CHECK(q.string(0) != "studentpass");
    CHECK(auth::verify_password("studentpass", q.string(0)));
    CHECK_FALSE(auth::verify_password("bad", q.string(0)));
    auto r = service.apply(student, "persistent", 1, "2026-10-19", Slot::Morning);
    Service reopened(database.path, [] { return LocalTime{"2026-10-19", 420}; });
    CHECK(reopened.get(student, r.id).student_id == r.student_id);
    CHECK(reopened.apply(student, "persistent", 1, r.date, r.slot).id == r.id);
    reopened.logout(student);
    CHECK(code([&] { service.identity(student); }, Code::Unauthorized));
    CHECK(code([&] { service.bootstrap("adminpass"); }, Code::Conflict));
}
TEST_CASE_FIXTURE(Fixture, "failure after decrement and after request "
                           "insertion rolls back entire operation") {
    db::Connection db(database.path);
    for (const auto *table : {"reservations", "audit"}) {
        // Fault injection stays in the test database; production has no failure
        // switch.
        const auto sql = std::string("CREATE TRIGGER inject_failure BEFORE INSERT ON ") + table +
                         " BEGIN SELECT RAISE(ABORT,'injected'); END";
        db.exec(sql.c_str());
        CHECK(code([&] { service.apply(student, "rollback", 1, "2026-10-19", Slot::Morning); },
                   Code::Conflict));
        CHECK(scalar(db, "SELECT count(*) FROM reservations") == 0);
        CHECK(scalar(db, "SELECT count(*) FROM requests") == 0);
        CHECK(scalar(db, "SELECT coalesce(sum(occupied),0) FROM slots") == 0);
        db.exec("DROP TRIGGER inject_failure");
    }
    auto r = service.apply(student, "rollback", 1, "2026-10-19", Slot::Morning);
    CHECK(r.status == Status::Pending);
    db.exec("CREATE TRIGGER inject_failure BEFORE INSERT ON audit BEGIN SELECT "
            "RAISE(ABORT,'injected'); END");
    CHECK_THROWS(service.change(student, r.id, Status::Pending, Status::Cancelled));
    CHECK(service.get(student, r.id).status == Status::Pending);
    CHECK(scalar(db, "SELECT sum(occupied) FROM slots") == 1);
    db.exec("DROP TRIGGER inject_failure");
}
TEST_CASE("commit failure RAII rollback and corrupt database error") {
    TempDatabase temp;
    db::Connection db(temp.path);
    db::migrate(db);
    sqlite3_commit_hook(db.handle(), [](void *) -> int { return 1; }, nullptr);
    CHECK_THROWS([&] {
        db::Transaction tx(db);
        db.exec("INSERT INTO rooms(name,capacity) VALUES('never committed',10)");
        tx.commit();
    }());
    sqlite3_commit_hook(db.handle(), nullptr, nullptr);
    CHECK(scalar(db, "SELECT count(*) FROM rooms") == 0);
    CHECK(sqlite3_get_autocommit(db.handle()) == 1);
    CHECK(code([&] { Service missing((temp.directory / "missing" / "x.db").string()); },
               Code::Storage));
}
TEST_CASE("corrupt database is reported as storage failure") {
    TempDatabase temp;
    {
        std::ofstream file(temp.path);
        file << "not a database";
    }
    CHECK(code([&] { Service corrupt(temp.path); }, Code::Storage));
}
TEST_CASE_FIXTURE(Fixture, "lock timeout is busy not full and retry works") {
    db::Connection blocker(database.path);
    db::Transaction tx(blocker);
    auto start = std::chrono::steady_clock::now();
    CHECK(
        code([&] { service.apply(student, "busy", 1, "2026-10-19", Slot::Morning); }, Code::Busy));
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
    tx.commit();
    CHECK_NOTHROW(service.apply(student, "busy", 1, "2026-10-19", Slot::Morning));
}
TEST_CASE_FIXTURE(Fixture, "twenty independent connections compete for exactly ten seats") {
    service.set_capacity(admin, 1, "2026-10-19", Slot::Morning, 10);
    std::vector<std::string> tokens;
    for (int i = 0; i < 20; ++i) {
        const auto n = "racer" + std::to_string(i);
        service.add_user(admin, n, "racerpass", Role::Student);
        tokens.push_back(service.login(n, "racerpass"));
    }
    struct Result {
        int success = 0, full = 0, busy = 0, conflict = 0, internal = 0, busy_attempts = 0;
    };
    std::vector<Result> results(20);
    std::vector<std::thread> threads;
    Gate gate(20);
    for (int i = 0; i < 20; ++i) {
        auto worker =
            std::make_unique<Service>(database.path, [] { return LocalTime{"2026-10-19", 420}; });
        threads.emplace_back([&, i, worker = std::move(worker)] {
            gate.arrive();
            auto &out = results[static_cast<std::size_t>(i)];
            for (int attempt = 0; attempt < 4; ++attempt) {
                try {
                    worker->apply(tokens[static_cast<std::size_t>(i)], "race", 1, "2026-10-19",
                                  Slot::Morning);
                    out.success = 1;
                    return;
                } catch (const Error &e) {
                    if (e.code == Code::Busy) {
                        ++out.busy_attempts;
                        if (attempt < 3) {
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(10 * (1 << attempt)));
                            continue;
                        }
                        out.busy = 1;
                    } else if (e.code == Code::Full)
                        out.full = 1;
                    else if (e.code == Code::Conflict)
                        out.conflict = 1;
                    else
                        out.internal = 1;
                    return;
                } catch (...) {
                    out.internal = 1;
                    return;
                }
            }
        });
    }
    for (auto &t : threads)
        t.join();
    Result total;
    for (auto r : results) {
        total.success += r.success;
        total.full += r.full;
        total.busy += r.busy;
        total.conflict += r.conflict;
        total.internal += r.internal;
        total.busy_attempts += r.busy_attempts;
    }
    std::cout << "CONCURRENCY total=20 success=" << total.success << " full=" << total.full
              << " busy_final=" << total.busy << " busy_attempts=" << total.busy_attempts
              << " conflict=" << total.conflict << " internal=" << total.internal
              << " timeout=0 (joined)\n";
    CHECK(total.success == 10);
    CHECK(total.full == 10);
    CHECK(total.busy == 0);
    CHECK(total.conflict == 0);
    CHECK(total.internal == 0);
    db::Connection db(database.path);
    CHECK(scalar(db, "SELECT sum(occupied) FROM slots") == 10);
    CHECK(scalar(db, "SELECT count(*) FROM reservations WHERE status IN(0,1)") == 10);
    CHECK(scalar(db, "SELECT count(*) FROM requests") == 10);
    CHECK(scalar(db, "SELECT count(*) FROM audit WHERE action='apply'") == 10);
}
TEST_CASE_FIXTURE(Fixture, "concurrent idempotency and business duplicate across rooms") {
    for (bool same_request : {true, false}) {
        const auto date = same_request ? "2026-10-19" : "2026-10-20";
        Service second(database.path, [] { return LocalTime{"2026-10-19", 420}; });
        Gate gate(2);
        Id ids[2] = {0, 0};
        bool conflict[2] = {false, false};
        bool unexpected[2] = {false, false};
        auto run = [&](int i, Service &svc) {
            gate.arrive();
            try {
                ids[i] = svc.apply(student, same_request ? "key-a" : (i == 0 ? "new-a" : "new-b"),
                                   same_request ? 1 : i + 1, date, Slot::Morning)
                             .id;
            } catch (const Error &e) {
                conflict[i] = e.code == Code::Conflict;
                unexpected[i] = !conflict[i];
            } catch (...) {
                unexpected[i] = true;
            }
        };
        std::thread one(run, 0, std::ref(service)), two(run, 1, std::ref(second));
        one.join();
        two.join();
        CHECK_FALSE(unexpected[0]);
        CHECK_FALSE(unexpected[1]);
        if (same_request) {
            CHECK(ids[0] > 0);
            CHECK(ids[0] == ids[1]);
        } else {
            CHECK(static_cast<int>(conflict[0]) + static_cast<int>(conflict[1]) == 1);
            CHECK(static_cast<int>(ids[0] > 0) + static_cast<int>(ids[1] > 0) == 1);
        }
    }
}
TEST_CASE_FIXTURE(Fixture, "teacher and student race with expected Pending state") {
    for (int iteration = 0; iteration < 12; ++iteration) {
        auto r = service.apply(student, "state-" + std::to_string(iteration), 1, "2026-10-19",
                               Slot::Morning);
        Service second(database.path, [] { return LocalTime{"2026-10-19", 420}; });
        Gate gate(2);
        int ok[2] = {0, 0}, conflict[2] = {0, 0}, unexpected[2] = {0, 0};
        auto change = [&](int i, Service &svc) {
            gate.arrive();
            try {
                svc.change(i == 0 ? teacher : student, r.id, Status::Pending,
                           i == 0 ? Status::Approved : Status::Cancelled);
                ok[i] = 1;
            } catch (const Error &e) {
                if (e.code == Code::Conflict)
                    conflict[i] = 1;
                else
                    unexpected[i] = 1;
            } catch (...) {
                unexpected[i] = 1;
            }
        };
        std::thread a(change, 0, std::ref(service)), b(change, 1, std::ref(second));
        a.join();
        b.join();
        CHECK(ok[0] + ok[1] == 1);
        CHECK(conflict[0] + conflict[1] == 1);
        CHECK(unexpected[0] + unexpected[1] == 0);
        const auto final = service.get(student, r.id);
        CHECK((final.status == Status::Approved || final.status == Status::Cancelled));
        CHECK(service.availability(student, r.date, r.slot)[0].occupied ==
              (final.status == Status::Approved ? 1 : 0));
        if (final.status == Status::Approved)
            service.change(student, r.id, Status::Approved, Status::Cancelled);
    }
}
TEST_CASE_FIXTURE(Fixture,
                  "bound SQL input, expired sessions, unique index and invalid state constraints") {
    const std::string attack = "quoted'; DROP TABLE users; --";
    CHECK_NOTHROW(service.add_user(admin, attack, "boundpass", Role::Student));
    CHECK(service.identity(service.login(attack, "boundpass")).name == attack);
    CHECK(code([&] { service.add_user(admin, "bad", "boundpass", static_cast<Role>(99)); },
               Code::InvalidInput));
    auto r = service.apply(student, "constraints", 1, "2026-10-19", Slot::Morning);
    db::Connection db(database.path);
    db::Statement duplicate(
        db, "INSERT INTO reservations(student_id,room_id,date,slot,status) VALUES(?,?,?,?,0)");
    CHECK_THROWS(duplicate.bind(1, r.student_id)
                     .bind(2, r.room_id)
                     .bind(3, r.date)
                     .bind(4, static_cast<int>(r.slot))
                     .done());
    CHECK_THROWS(db.exec("UPDATE reservations SET status=99"));
    CHECK_THROWS(db.exec("UPDATE slots SET occupied=capacity+1"));
    CHECK_THROWS(db.exec("UPDATE reservations SET student_id=999999"));
    CHECK(scalar(db, "SELECT count(*) FROM reservations") == 1);
    db::Statement expire(db, "UPDATE sessions SET expires_at=0 WHERE token_hash=?");
    expire.bind(1, auth::digest(student)).done();
    CHECK(code([&] { service.get(student, r.id); }, Code::Unauthorized));
}
TEST_CASE_FIXTURE(Fixture, "same-state cancellation race releases only once") {
    auto r = service.apply(student, "cancel-race", 1, "2026-10-19", Slot::Morning);
    Service second(database.path, [] { return LocalTime{"2026-10-19", 420}; });
    Gate gate(2);
    bool success[2] = {false, false}, conflict[2] = {false, false}, unexpected[2] = {false, false};
    auto run = [&](int i, Service &svc) {
        gate.arrive();
        try {
            svc.change(student, r.id, Status::Pending, Status::Cancelled);
            success[i] = true;
        } catch (const Error &e) {
            conflict[i] = e.code == Code::Conflict;
            unexpected[i] = !conflict[i];
        } catch (...) {
            unexpected[i] = true;
        }
    };
    std::thread a(run, 0, std::ref(service)), b(run, 1, std::ref(second));
    a.join();
    b.join();
    CHECK(static_cast<int>(success[0]) + static_cast<int>(success[1]) == 1);
    CHECK(static_cast<int>(conflict[0]) + static_cast<int>(conflict[1]) == 1);
    CHECK_FALSE(unexpected[0]);
    CHECK_FALSE(unexpected[1]);
    CHECK(service.availability(student, r.date, r.slot)[0].occupied == 0);
    db::Connection db(database.path);
    CHECK(scalar(db, "SELECT count(*) FROM audit WHERE action='Pending->Cancelled'") == 1);
}
