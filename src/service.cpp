#include "seat/service.hpp"
#include "seat/database.hpp"
#include "seat/password.hpp"
#include <chrono>
namespace seat {
using db::Statement;
namespace {
Id unix_now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
void validate_name(const std::string &n) {
    if (n.empty() || n.size() > 80 || n.find('\0') != std::string::npos)
        throw Error(Code::InvalidInput, "name length must be 1-80 bytes");
}
void validate_capacity(int c) {
    if (c < 0 || c > 1000000)
        throw Error(Code::InvalidInput, "capacity must be 0-1000000");
}
Reservation reservation(Statement &q) {
    return {q.integer(0),
            q.integer(1),
            q.integer(2),
            q.string(3),
            static_cast<Slot>(q.integer(4)),
            static_cast<Status>(q.integer(5))};
}
} // namespace
struct Service::Impl {
    db::Connection db;
    Clock clock;
    Impl(const std::string &path, Clock now) : db(path), clock(std::move(now)) { db::migrate(db); }
    User actor(const std::string &token) {
        if (token.size() != 64)
            throw Error(Code::Unauthorized, "login required");
        Statement q(db, "SELECT u.id,u.name,u.role FROM sessions s JOIN users u ON "
                        "s.user_id=u.id WHERE s.token_hash=? AND s.expires_at>?");
        q.bind(1, auth::digest(token)).bind(2, unix_now());
        if (!q.row())
            throw Error(Code::Unauthorized, "invalid or expired session");
        return {q.integer(0), q.string(1), static_cast<Role>(q.integer(2))};
    }
    User require(const std::string &t, Role role) {
        auto u = actor(t);
        if (u.role != role)
            throw Error(Code::Forbidden, "role not permitted");
        return u;
    }
    Room room(Id id) {
        Statement q(db, "SELECT id,name,capacity FROM rooms WHERE id=?");
        q.bind(1, id);
        if (!q.row())
            throw Error(Code::NotFound, "room not found");
        return {q.integer(0), q.string(1), static_cast<int>(q.integer(2))};
    }
    void ensure_slot(Id id, const std::string &d, Slot s) {
        auto r = room(id);
        Statement q(db, "INSERT INTO slots(room_id,date,slot,capacity) VALUES(?,?,?,?) "
                        "ON CONFLICT(room_id,date,slot) DO NOTHING");
        q.bind(1, id).bind(2, d).bind(3, static_cast<int>(s)).bind(4, r.capacity).done();
    }
    Reservation get(Id id) {
        Statement q(db, "SELECT id,student_id,room_id,date,slot,status FROM "
                        "reservations WHERE id=?");
        q.bind(1, id);
        if (!q.row())
            throw Error(Code::NotFound, "reservation not found");
        return reservation(q);
    }
    void audit(Id actor, Id id, const std::string &action) {
        Statement q(db, "INSERT INTO audit(actor_id,reservation_id,action) VALUES(?,?,?)");
        q.bind(1, actor).bind(2, id).bind(3, action).done();
    }
    void admin_audit(Id actor, const std::string &action) {
        Statement q(db, "INSERT INTO audit(actor_id,action) VALUES(?,?)");
        q.bind(1, actor).bind(2, action).done();
    }
};
Service::Service(const std::string &path, Clock clock)
    : impl_(std::make_unique<Impl>(path, std::move(clock))) {}
Service::~Service() = default;
void Service::bootstrap(const std::string &password) {
    // Hashing deliberately precedes the write transaction to keep lock time
    // short.
    const auto hash = auth::hash_password(password);
    db::Transaction tx(impl_->db);
    Statement count(impl_->db, "SELECT count(*) FROM users");
    count.row();
    if (count.integer(0) != 0)
        throw Error(Code::Conflict, "already initialized");
    Statement q(impl_->db, "INSERT INTO users(name,password_hash,role) VALUES('admin',?,2)");
    q.bind(1, hash).done();
    impl_->admin_audit(impl_->db.last_id(), "bootstrap");
    tx.commit();
}
std::string Service::login(const std::string &n, const std::string &p) {
    Id id = 0;
    std::string hash;
    {
        Statement q(impl_->db, "SELECT id,password_hash FROM users WHERE name=?");
        q.bind(1, n);
        if (!q.row())
            throw Error(Code::Unauthorized, "invalid credentials");
        id = q.integer(0);
        hash = q.string(1);
    }
    if (!auth::verify_password(p, hash))
        throw Error(Code::Unauthorized, "invalid credentials");
    const auto token = auth::random_token();
    db::Transaction tx(impl_->db);
    Statement clean(impl_->db, "DELETE FROM sessions WHERE expires_at<=?");
    clean.bind(1, unix_now()).done();
    Statement q(impl_->db, "INSERT INTO sessions(token_hash,user_id,expires_at) VALUES(?,?,?)");
    q.bind(1, auth::digest(token)).bind(2, id).bind(3, unix_now() + 8 * 60 * 60).done();
    tx.commit();
    return token;
}
void Service::logout(const std::string &t) {
    Statement q(impl_->db, "DELETE FROM sessions WHERE token_hash=?");
    q.bind(1, auth::digest(t)).done();
}
User Service::identity(const std::string &t) { return impl_->actor(t); }
User Service::add_user(const std::string &t, const std::string &n, const std::string &p,
                       Role role) {
    impl_->require(t, Role::Admin);
    validate_name(n);
    if (role != Role::Student && role != Role::Teacher && role != Role::Admin)
        throw Error(Code::InvalidInput, "invalid role");
    const auto hash = auth::hash_password(p);
    db::Transaction tx(impl_->db);
    auto actor = impl_->require(t, Role::Admin);
    Statement q(impl_->db, "INSERT INTO users(name,password_hash,role) VALUES(?,?,?)");
    q.bind(1, n).bind(2, hash).bind(3, static_cast<int>(role)).done();
    User u{impl_->db.last_id(), n, role};
    impl_->admin_audit(actor.id, "create_user:" + std::to_string(u.id));
    tx.commit();
    return u;
}
std::vector<User> Service::users(const std::string &t) {
    impl_->require(t, Role::Admin);
    Statement q(impl_->db, "SELECT id,name,role FROM users ORDER BY id");
    std::vector<User> out;
    while (q.row())
        out.push_back({q.integer(0), q.string(1), static_cast<Role>(q.integer(2))});
    return out;
}
Room Service::add_room(const std::string &t, const std::string &n, int c) {
    validate_name(n);
    validate_capacity(c);
    db::Transaction tx(impl_->db);
    auto actor = impl_->require(t, Role::Admin);
    Statement q(impl_->db, "INSERT INTO rooms(name,capacity) VALUES(?,?)");
    q.bind(1, n).bind(2, c).done();
    Room r{impl_->db.last_id(), n, c};
    impl_->admin_audit(actor.id, "create_room:" + std::to_string(r.id));
    tx.commit();
    return r;
}
void Service::set_capacity(const std::string &t, Id r, const std::string &d, Slot s, int c) {
    validate_capacity(c);
    validate_slot(d, s, impl_->clock());
    db::Transaction tx(impl_->db);
    auto actor = impl_->require(t, Role::Admin);
    impl_->ensure_slot(r, d, s);
    Statement q(impl_->db, "UPDATE slots SET capacity=? WHERE room_id=? AND "
                           "date=? AND slot=? AND occupied<=?");
    q.bind(1, c).bind(2, r).bind(3, d).bind(4, static_cast<int>(s)).bind(5, c).done();
    if (impl_->db.changes() != 1)
        throw Error(Code::Conflict, "capacity below occupancy");
    impl_->admin_audit(actor.id, "set_capacity:" + std::to_string(r) + ":" + d + ":" +
                                     std::to_string(static_cast<int>(s)) + ":" + std::to_string(c));
    tx.commit();
}
std::vector<Availability> Service::availability(const std::string &t, const std::string &d,
                                                Slot s) {
    impl_->actor(t);
    validate_slot(d, s, impl_->clock());
    Statement q(impl_->db, "SELECT "
                           "r.id,r.name,r.capacity,coalesce(s.capacity,r.capacity),coalesce("
                           "s.occupied,0) FROM rooms r LEFT JOIN slots s ON s.room_id=r.id "
                           "AND s.date=? AND s.slot=? ORDER BY r.id");
    q.bind(1, d).bind(2, static_cast<int>(s));
    std::vector<Availability> out;
    while (q.row())
        out.push_back({{q.integer(0), q.string(1), static_cast<int>(q.integer(2))},
                       static_cast<int>(q.integer(3)),
                       static_cast<int>(q.integer(4))});
    return out;
}
Reservation Service::apply(const std::string &t, const std::string &key, Id room,
                           const std::string &d, Slot s) {
    if (key.empty() || key.size() > 128 || key.find('\0') != std::string::npos)
        throw Error(Code::InvalidInput, "request id length must be 1-128 bytes");
    db::Transaction tx(impl_->db);
    auto u = impl_->require(t, Role::Student);
    {
        Statement q(impl_->db, "SELECT reservation_id,room_id,date,slot FROM "
                               "requests WHERE student_id=? AND request_id=?");
        q.bind(1, u.id).bind(2, key);
        if (q.row()) {
            if (q.integer(1) != room || q.string(2) != d || q.integer(3) != static_cast<int>(s))
                throw Error(Code::Conflict, "request id reused with different payload");
            Reservation original{q.integer(0), u.id, room, d, s, Status::Pending};
            tx.commit();
            return original;
        }
    }
    validate_slot(d, s, impl_->clock());
    {
        Statement q(impl_->db, "SELECT 1 FROM reservations WHERE student_id=? AND "
                               "date=? AND slot=? AND status IN(0,1)");
        q.bind(1, u.id).bind(2, d).bind(3, static_cast<int>(s));
        if (q.row())
            throw Error(Code::Conflict, "active reservation exists in this time slot");
    }
    impl_->ensure_slot(room, d, s);
    // This conditional UPDATE, not an unprotected read, decides admission.
    Statement decrement(impl_->db, "UPDATE slots SET occupied=occupied+1 WHERE room_id=? "
                                   "AND date=? AND slot=? AND occupied<capacity");
    decrement.bind(1, room).bind(2, d).bind(3, static_cast<int>(s)).done();
    if (impl_->db.changes() != 1)
        throw Error(Code::Full, "no remaining capacity");
    Statement insert(impl_->db, "INSERT INTO reservations(student_id,room_id,date,slot,status) "
                                "VALUES(?,?,?,?,0)");
    insert.bind(1, u.id).bind(2, room).bind(3, d).bind(4, static_cast<int>(s)).done();
    Reservation r{impl_->db.last_id(), u.id, room, d, s, Status::Pending};
    Statement request(impl_->db, "INSERT INTO "
                                 "requests(student_id,request_id,room_id,date,"
                                 "slot,reservation_id) VALUES(?,?,?,?,?,?)");
    request.bind(1, u.id)
        .bind(2, key)
        .bind(3, room)
        .bind(4, d)
        .bind(5, static_cast<int>(s))
        .bind(6, r.id)
        .done();
    impl_->audit(u.id, r.id, "apply");
    tx.commit();
    return r;
}
Reservation Service::get(const std::string &t, Id id) {
    auto u = impl_->actor(t);
    if (u.role == Role::Admin)
        throw Error(Code::Forbidden, "student or teacher required");
    auto r = impl_->get(id);
    if (u.role == Role::Student && r.student_id != u.id)
        throw Error(Code::Forbidden, "not your reservation");
    return r;
}
std::vector<Reservation> Service::reservations(const std::string &t) {
    auto u = impl_->actor(t);
    if (u.role == Role::Admin)
        throw Error(Code::Forbidden, "student or teacher required");
    Statement q(impl_->db, "SELECT id,student_id,room_id,date,slot,status FROM reservations "
                           "WHERE (?=0 OR student_id=?) ORDER BY id");
    q.bind(1, u.role == Role::Student ? 1 : 0).bind(2, u.id);
    std::vector<Reservation> out;
    while (q.row())
        out.push_back(reservation(q));
    return out;
}
Reservation Service::change(const std::string &t, Id id, Status expected, Status target) {
    db::Transaction tx(impl_->db);
    auto u = impl_->actor(t);
    auto r = impl_->get(id);
    if (target == Status::Cancelled) {
        if (u.role != Role::Student || u.id != r.student_id)
            throw Error(Code::Forbidden, "only owner can cancel");
    } else if (u.role != Role::Teacher)
        throw Error(Code::Forbidden, "teacher required");
    if (!can_transition(expected, target))
        throw Error(Code::Conflict, "illegal state transition");
    Statement update(impl_->db, "UPDATE reservations SET status=? WHERE id=? AND status=?");
    update.bind(1, static_cast<int>(target)).bind(2, id).bind(3, static_cast<int>(expected)).done();
    if (impl_->db.changes() != 1)
        throw Error(Code::Conflict, "stale expected state");
    if (target == Status::Rejected || target == Status::Cancelled) {
        Statement release(impl_->db, "UPDATE slots SET occupied=occupied-1 WHERE room_id=? "
                                     "AND date=? AND slot=? AND occupied>0");
        release.bind(1, r.room_id).bind(2, r.date).bind(3, static_cast<int>(r.slot)).done();
        if (impl_->db.changes() != 1)
            throw Error(Code::Storage, "occupancy invariant broken");
    }
    impl_->audit(u.id, id, std::string(name(expected)) + "->" + name(target));
    tx.commit();
    r.status = target;
    return r;
}
} // namespace seat
