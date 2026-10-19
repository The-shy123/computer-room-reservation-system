#pragma once
#include "seat/domain.hpp"
#include <memory>
#include <vector>
namespace seat {
// One Service belongs to one executing thread. Its implementation owns storage.
class Service {
  public:
    explicit Service(const std::string &database, Clock clock = shanghai_now);
    ~Service();
    Service(const Service &) = delete;
    Service &operator=(const Service &) = delete;
    void bootstrap(const std::string &admin_password);
    std::string login(const std::string &name, const std::string &password);
    void logout(const std::string &token);
    User identity(const std::string &token);
    User add_user(const std::string &token, const std::string &name, const std::string &password,
                  Role role);
    std::vector<User> users(const std::string &token);
    Room add_room(const std::string &token, const std::string &name, int capacity);
    void set_capacity(const std::string &token, Id room, const std::string &date, Slot slot,
                      int capacity);
    std::vector<Availability> availability(const std::string &token, const std::string &date,
                                           Slot slot);
    Reservation apply(const std::string &token, const std::string &request_id, Id room,
                      const std::string &date, Slot slot);
    Reservation change(const std::string &token, Id id, Status expected, Status target);
    Reservation get(const std::string &token, Id id);
    std::vector<Reservation> reservations(const std::string &token);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace seat
