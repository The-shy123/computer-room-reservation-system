#include "seat/domain.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
namespace seat {
namespace {
int days(const std::string &date) {
    validate_date(date);
    int y = std::stoi(date.substr(0, 4));
    const int m = std::stoi(date.substr(5, 2));
    const int d = std::stoi(date.substr(8, 2));
    y -= m <= 2;
    const int era = y / 400;
    const int yo = y - era * 400;
    return era * 146097 + yo * 365 + yo / 4 - yo / 100 + (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 +
           d - 1 - 719468;
}
} // namespace
void validate_date(const std::string &s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-')
        throw Error(Code::InvalidInput, "date must be YYYY-MM-DD");
    for (std::size_t i = 0; i < s.size(); ++i)
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9'))
            throw Error(Code::InvalidInput, "invalid date");
    const int y = std::stoi(s.substr(0, 4)), m = std::stoi(s.substr(5, 2)),
              d = std::stoi(s.substr(8, 2));
    const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y < 2000 || y > 2099 || m < 1 || m > 12)
        throw Error(Code::InvalidInput, "date outside supported calendar 2000-2099");
    const int limit =
        lengths[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0) ? 1 : 0);
    if (d < 1 || d > limit)
        throw Error(Code::InvalidInput, "invalid day");
}
void validate_slot(const std::string &date, Slot slot, const LocalTime &now) {
    const int day = days(date), today = days(now.date);
    if (slot != Slot::Morning && slot != Slot::Afternoon)
        throw Error(Code::InvalidInput, "slot must be 1 or 2");
    const int weekday = (day + 4) % 7;
    if (weekday == 0 || weekday == 6)
        throw Error(Code::InvalidInput, "weekdays only");
    const int start = slot == Slot::Morning ? 8 * 60 : 14 * 60;
    if (day < today || (day == today && now.minute >= start))
        throw Error(Code::InvalidInput, "slot has started");
}
LocalTime shanghai_now() {
    const auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() +
                                                        std::chrono::hours(8));
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char date[16]{};
    std::strftime(date, sizeof(date), "%Y-%m-%d", &tm);
    return {date, tm.tm_hour * 60 + tm.tm_min};
}
bool can_transition(Status from, Status to) {
    return (from == Status::Pending &&
            (to == Status::Approved || to == Status::Rejected || to == Status::Cancelled)) ||
           (from == Status::Approved && to == Status::Cancelled);
}
const char *name(Status s) {
    switch (s) {
    case Status::Pending:
        return "Pending";
    case Status::Approved:
        return "Approved";
    case Status::Rejected:
        return "Rejected";
    case Status::Cancelled:
        return "Cancelled";
    }
    return "Invalid";
}
const char *name(Code c) {
    switch (c) {
    case Code::InvalidInput:
        return "invalid_input";
    case Code::NotFound:
        return "not_found";
    case Code::Unauthorized:
        return "unauthorized";
    case Code::Forbidden:
        return "forbidden";
    case Code::Full:
        return "capacity_full";
    case Code::Conflict:
        return "conflict";
    case Code::Busy:
        return "database_busy";
    case Code::Storage:
        return "storage_error";
    }
    return "internal_error";
}
} // namespace seat
