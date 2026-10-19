#include "seat/service.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <termios.h>
#include <unistd.h>
using namespace seat;
namespace {
std::string secret() {
    struct EchoGuard {
        termios original{};
        bool changed = false;
        EchoGuard() {
            if (isatty(STDIN_FILENO)) {
                if (tcgetattr(STDIN_FILENO, &original) != 0)
                    throw Error(Code::Storage, "terminal read failed");
                auto hidden = original;
                hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
                if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden) != 0)
                    throw Error(Code::Storage, "terminal echo control failed");
                changed = true;
            }
        }
        ~EchoGuard() noexcept {
            if (changed)
                tcsetattr(STDIN_FILENO, TCSANOW, &original);
        }
    } guard;
    std::cout << "Password: " << std::flush;
    std::string password;
    if (!std::getline(std::cin, password))
        throw Error(Code::InvalidInput, "password input ended");
    std::cout << '\n';
    return password;
}
void print(const Reservation &r) {
    std::cout << r.id << " student=" << r.student_id << " room=" << r.room_id << " " << r.date
              << " slot=" << static_cast<int>(r.slot) << " " << name(r.status) << '\n';
}
Status status(const std::string &s) {
    for (auto v : {Status::Pending, Status::Approved, Status::Rejected, Status::Cancelled})
        if (s == name(v))
            return v;
    throw Error(Code::InvalidInput, "unknown status");
}
void end(std::istringstream &in) {
    std::string extra;
    if (!in || in >> extra)
        throw Error(Code::InvalidInput, "invalid command arguments");
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 3 || std::string(argv[1]) != "--db" ||
        !std::filesystem::path(argv[2]).is_absolute()) {
        std::cerr << "Usage: seat_cli --db /absolute/path/seat.db\n";
        return 2;
    }
    try {
        Service service(argv[2]);
        std::string token, line;
        std::cout << "Seat CLI (SQLite). Commands: bootstrap, login NAME, user NAME "
                     "ROLE(0 student/1 teacher/2 admin), room NAME CAPACITY, capacity "
                     "ROOM DATE SLOT N, rooms DATE SLOT, apply KEY ROOM DATE SLOT, list, "
                     "get ID, change ID EXPECTED TARGET, logout, quit\n";
        while (std::cout << "> " && std::getline(std::cin, line)) {
            try {
                std::istringstream in(line);
                std::string cmd;
                in >> cmd;
                if (cmd.empty())
                    continue;
                if (cmd == "quit") {
                    end(in);
                    break;
                }
                if (cmd == "bootstrap") {
                    end(in);
                    service.bootstrap(secret());
                } else if (cmd == "login") {
                    std::string n;
                    in >> n;
                    end(in);
                    service.logout(token);
                    token.clear();
                    token = service.login(n, secret());
                    std::cout << "logged in\n";
                } else if (cmd == "logout") {
                    end(in);
                    service.logout(token);
                    token.clear();
                } else if (cmd == "user") {
                    std::string n;
                    int r = 0;
                    in >> n >> r;
                    end(in);
                    std::cout << service.add_user(token, n, secret(), static_cast<Role>(r)).id
                              << '\n';
                } else if (cmd == "users") {
                    end(in);
                    for (auto u : service.users(token))
                        std::cout << u.id << " " << u.name << " role=" << static_cast<int>(u.role)
                                  << '\n';
                } else if (cmd == "room") {
                    std::string n;
                    int c = 0;
                    in >> n >> c;
                    end(in);
                    std::cout << service.add_room(token, n, c).id << '\n';
                } else if (cmd == "rooms") {
                    std::string d;
                    int s = 0;
                    in >> d >> s;
                    end(in);
                    for (auto a : service.availability(token, d, static_cast<Slot>(s)))
                        std::cout << a.room.id << " " << a.room.name
                                  << " remaining=" << a.capacity - a.occupied << '\n';
                } else if (cmd == "capacity") {
                    Id r = 0;
                    std::string d;
                    int s = 0, c = 0;
                    in >> r >> d >> s >> c;
                    end(in);
                    service.set_capacity(token, r, d, static_cast<Slot>(s), c);
                } else if (cmd == "apply") {
                    std::string k, d;
                    Id r = 0;
                    int s = 0;
                    in >> k >> r >> d >> s;
                    end(in);
                    print(service.apply(token, k, r, d, static_cast<Slot>(s)));
                } else if (cmd == "list") {
                    end(in);
                    for (auto r : service.reservations(token))
                        print(r);
                } else if (cmd == "get") {
                    Id id = 0;
                    in >> id;
                    end(in);
                    print(service.get(token, id));
                } else if (cmd == "change") {
                    Id id = 0;
                    std::string e, t;
                    in >> id >> e >> t;
                    end(in);
                    print(service.change(token, id, status(e), status(t)));
                } else
                    throw Error(Code::InvalidInput, "unknown command");
            } catch (const Error &e) {
                std::cout << name(e.code) << ": " << e.what() << '\n';
            } catch (const std::exception &) {
                std::cout << "storage_error\n";
            }
        }
        if (!token.empty())
            service.logout(token);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
