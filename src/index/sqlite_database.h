#pragma once

#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace retort
{
class sqlite_database
{
public:
    explicit sqlite_database(const std::string &path, int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    ~sqlite_database();

    sqlite_database(const sqlite_database &) = delete;
    sqlite_database &operator=(const sqlite_database &) = delete;

    sqlite_database(sqlite_database &&) = delete;
    sqlite_database &operator=(sqlite_database &&) = delete;

    sqlite3 *handle() const noexcept;
    void exec(const std::string &sql);

private:
    sqlite3 *db_ = nullptr;
};
}
