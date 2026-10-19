#pragma once
#include "seat/database.hpp"
#include "seat/password.hpp"
#include "seat/service.hpp"
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>
struct TempDatabase {
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() / ("seat-test-" + seat::auth::random_token());
    std::string path;
    TempDatabase() {
        std::filesystem::create_directory(directory);
        path = (directory / "test.db").string();
    }
    ~TempDatabase() {
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
    }
};
class Gate {
    std::mutex mutex_;
    std::condition_variable cv_;
    int remaining_;

  public:
    explicit Gate(int count) : remaining_(count) {}
    void arrive() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (--remaining_ == 0)
            cv_.notify_all();
        else
            cv_.wait(lock, [&] { return remaining_ == 0; });
    }
};
inline seat::Id scalar(seat::db::Connection &db, const char *sql) {
    seat::db::Statement q(db, sql);
    if (!q.row())
        throw std::runtime_error("missing scalar row");
    return q.integer(0);
}
