#include "seat/database.hpp"
namespace seat::db {
namespace {
void check(int rc) {
    if (rc == SQLITE_OK || rc == SQLITE_ROW || rc == SQLITE_DONE)
        return;
    const int base = rc & 0xff;
    if (base == SQLITE_BUSY || base == SQLITE_LOCKED)
        throw Error(Code::Busy, "database busy; retry with same request id");
    if (base == SQLITE_CONSTRAINT)
        throw Error(Code::Conflict, "database constraint conflict");
    throw Error(Code::Storage,
                "database operation failed (SQLite code " + std::to_string(rc) + ")");
}
} // namespace
Connection::Connection(const std::string &path, int busy_ms) {
    const int rc =
        sqlite3_open_v2(path.c_str(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db_);
        db_ = nullptr;
        check(rc);
    }
    try {
        check(sqlite3_extended_result_codes(db_, 1));
        check(sqlite3_busy_timeout(db_, busy_ms));
        exec("PRAGMA foreign_keys=ON");
        Statement q(*this, "PRAGMA foreign_keys");
        if (!q.row() || q.integer(0) != 1)
            throw Error(Code::Storage, "foreign keys unavailable");
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}
Connection::~Connection() noexcept { sqlite3_close(db_); }
void Connection::exec(const char *sql) { check(sqlite3_exec(db_, sql, nullptr, nullptr, nullptr)); }
Statement::Statement(Connection &db, const char *sql) {
    check(sqlite3_prepare_v2(db.handle(), sql, -1, &stmt_, nullptr));
}
Statement::~Statement() noexcept { sqlite3_finalize(stmt_); }
Statement &Statement::bind(int i, Id v) {
    check(sqlite3_bind_int64(stmt_, i, v));
    return *this;
}
Statement &Statement::bind(int i, const std::string &v) {
    check(sqlite3_bind_text(stmt_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT));
    return *this;
}
bool Statement::row() {
    const int rc = sqlite3_step(stmt_);
    check(rc);
    return rc == SQLITE_ROW;
}
void Statement::done() {
    if (row())
        throw Error(Code::Storage, "unexpected query row");
}
Id Statement::integer(int i) const { return sqlite3_column_int64(stmt_, i); }
std::string Statement::string(int i) const {
    const auto *p = sqlite3_column_text(stmt_, i);
    return p ? std::string(reinterpret_cast<const char *>(p),
                           static_cast<std::size_t>(sqlite3_column_bytes(stmt_, i)))
             : std::string{};
}
Transaction::~Transaction() noexcept {
    if (!committed_)
        sqlite3_exec(db_.handle(), "ROLLBACK", nullptr, nullptr, nullptr);
}
void Transaction::commit() {
    db_.exec("COMMIT");
    committed_ = true;
}
void migrate(Connection &db) {
    auto version = [&] {
        Statement q(db, "PRAGMA user_version");
        q.row();
        return q.integer(0);
    };
    const auto v = version();
    if (v == 1)
        return;
    if (v != 0)
        throw Error(Code::Storage, "unsupported schema version");
    db.exec("PRAGMA journal_mode=WAL");
    Transaction tx(db);
    if (version() == 1) {
        tx.commit();
        return;
    }
    db.exec(R"SQL(
CREATE TABLE users(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL UNIQUE CHECK(length(name) BETWEEN 1 AND 80),password_hash TEXT NOT NULL,role INTEGER NOT NULL CHECK(role IN(0,1,2)));
CREATE TABLE rooms(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL UNIQUE CHECK(length(name) BETWEEN 1 AND 80),capacity INTEGER NOT NULL CHECK(capacity BETWEEN 0 AND 1000000));
CREATE TABLE slots(room_id INTEGER NOT NULL REFERENCES rooms(id),date TEXT NOT NULL CHECK(length(date)=10 AND date BETWEEN '2000-01-01' AND '2099-12-31'),slot INTEGER NOT NULL CHECK(slot IN(1,2)),capacity INTEGER NOT NULL CHECK(capacity BETWEEN 0 AND 1000000),occupied INTEGER NOT NULL DEFAULT 0 CHECK(occupied>=0 AND occupied<=capacity),PRIMARY KEY(room_id,date,slot));
CREATE TABLE reservations(id INTEGER PRIMARY KEY AUTOINCREMENT,student_id INTEGER NOT NULL REFERENCES users(id),room_id INTEGER NOT NULL,date TEXT NOT NULL,slot INTEGER NOT NULL,status INTEGER NOT NULL CHECK(status IN(0,1,2,3)),FOREIGN KEY(room_id,date,slot) REFERENCES slots(room_id,date,slot));
CREATE UNIQUE INDEX one_active_per_student ON reservations(student_id,date,slot) WHERE status IN(0,1);
CREATE INDEX reservations_slot ON reservations(room_id,date,slot,status);
CREATE TABLE requests(student_id INTEGER NOT NULL REFERENCES users(id),request_id TEXT NOT NULL CHECK(length(request_id) BETWEEN 1 AND 128),room_id INTEGER NOT NULL,date TEXT NOT NULL,slot INTEGER NOT NULL,reservation_id INTEGER NOT NULL UNIQUE REFERENCES reservations(id),PRIMARY KEY(student_id,request_id));
CREATE TABLE audit(id INTEGER PRIMARY KEY AUTOINCREMENT,actor_id INTEGER NOT NULL REFERENCES users(id),reservation_id INTEGER REFERENCES reservations(id),action TEXT NOT NULL,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE sessions(token_hash TEXT PRIMARY KEY,user_id INTEGER NOT NULL REFERENCES users(id),expires_at INTEGER NOT NULL);
CREATE INDEX sessions_expiry ON sessions(expires_at);
PRAGMA user_version=1;
)SQL");
    tx.commit();
}
} // namespace seat::db
