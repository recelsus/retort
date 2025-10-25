#include "sqlite_database.h"

namespace retort
{
sqlite_database::sqlite_database(const std::string &path, int flags) {
    if (sqlite3_open_v2(path.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
        throw std::runtime_error("failed to open sqlite database: " + path);
    }
}

sqlite_database::~sqlite_database() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

sqlite3 *sqlite_database::handle() const noexcept
{
    return db_;
}

void sqlite_database::exec(const std::string &sql) {
    char *error_message = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_message) != SQLITE_OK) {
        std::string message = "sqlite exec failed";
        if (error_message != nullptr) {
            message.append(": ").append(error_message);
            sqlite3_free(error_message);
        }
        throw std::runtime_error(message);
    }
}
}
