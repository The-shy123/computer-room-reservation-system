#pragma once
#include "seat/domain.hpp"
#include <sqlite3.h>
namespace seat::db {
// Connections/statements/transactions are non-copyable, with nonthrowing
// cleanup.
class Connection {
  public:
    explicit Connection(const std::string &path, int busy_ms = 250);
    ~Connection() noexcept;
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
    void exec(const char *sql);
    sqlite3 *handle() const { return db_; }
    Id last_id() const { return sqlite3_last_insert_rowid(db_); }
    int changes() const { return sqlite3_changes(db_); }

  private:
    sqlite3 *db_ = nullptr;
};
class Statement {
  public:
    Statement(Connection &db, const char *sql);
    ~Statement() noexcept;
    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;
    Statement &bind(int index, Id value);
    Statement &bind(int index, const std::string &value);
    bool row();
    void done();
    Id integer(int column) const;
    std::string string(int column) const;

  private:
    sqlite3_stmt *stmt_ = nullptr;
};
class Transaction {
  public:
    explicit Transaction(Connection &db) : db_(db) { db_.exec("BEGIN IMMEDIATE"); }
    ~Transaction() noexcept;
    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;
    void commit();

  private:
    Connection &db_;
    bool committed_ = false;
};
void migrate(Connection &db);
} // namespace seat::db
