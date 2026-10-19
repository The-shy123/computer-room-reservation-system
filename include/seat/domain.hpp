#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
namespace seat {
using Id = std::int64_t;
enum class Role { Student, Teacher, Admin };
enum class Slot { Morning = 1, Afternoon = 2 };
enum class Status { Pending, Approved, Rejected, Cancelled };
enum class Code { InvalidInput, NotFound, Unauthorized, Forbidden, Full, Conflict, Busy, Storage };
class Error : public std::runtime_error {
  public:
    Error(Code value, const std::string &message) : std::runtime_error(message), code(value) {}
    Code code;
};
struct User {
    Id id;
    std::string name;
    Role role;
};
struct Room {
    Id id;
    std::string name;
    int capacity;
};
struct Reservation {
    Id id;
    Id student_id;
    Id room_id;
    std::string date;
    Slot slot;
    Status status;
};
struct Availability {
    Room room;
    int capacity;
    int occupied;
};
struct LocalTime {
    std::string date;
    int minute;
};
using Clock = std::function<LocalTime()>;
LocalTime shanghai_now();
void validate_date(const std::string &date);
void validate_slot(const std::string &date, Slot slot, const LocalTime &now);
bool can_transition(Status from, Status to);
const char *name(Status status);
const char *name(Code code);
} // namespace seat
