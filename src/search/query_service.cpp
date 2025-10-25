#include "query_service.h"

#include <stdexcept>
#include <utility>

namespace retort
{
namespace
{
void check_sqlite(int code) {
    if (code != SQLITE_OK && code != SQLITE_DONE && code != SQLITE_ROW) {
        throw std::runtime_error("sqlite operation failed");
    }
}
}

query_service::query_service(sqlite_database &database)
    : database_{database}
{
}

std::vector<search_hit> query_service::search(const std::string &query,
                                              std::size_t limit,
                                              std::size_t offset) const
{
    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "SELECT v.url, v.title, v.format, v.tags, v.lang, v.updated_at,"
        " bm25(docs_fts) AS score,"
        " snippet(docs_fts, 2, '<mark>', '</mark>', '...', 24) AS snippet"
        " FROM docs_fts"
        " JOIN v_search v ON v.doc_id = docs_fts.doc_id"
        " WHERE docs_fts MATCH ?"
        " ORDER BY score"
        " LIMIT ? OFFSET ?";
    check_sqlite(sqlite3_prepare_v2(database_.handle(), sql, -1, &stmt, nullptr));
    sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, static_cast<int>(limit));
    sqlite3_bind_int(stmt, 3, static_cast<int>(offset));

    std::vector<search_hit> hits;
    while (true) {
        const int step = sqlite3_step(stmt);
        if (step == SQLITE_ROW) {
            search_hit hit;
            hit.url = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            hit.title = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            hit.format = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            hit.tags_json = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            const auto lang_text = sqlite3_column_text(stmt, 4);
            hit.lang = lang_text ? reinterpret_cast<const char *>(lang_text) : std::string{};
            hit.updated_at = sqlite3_column_int64(stmt, 5);
            hit.score = sqlite3_column_double(stmt, 6);
            const auto snippet_text = sqlite3_column_text(stmt, 7);
            hit.snippet = snippet_text ? reinterpret_cast<const char *>(snippet_text) : std::string{};
            hits.push_back(std::move(hit));
            continue;
        }
        if (step == SQLITE_DONE) {
            break;
        }
        sqlite3_finalize(stmt);
        throw std::runtime_error("failed to read search result");
    }

    sqlite3_finalize(stmt);
    return hits;
}

meta_info query_service::load_meta() const
{
    sqlite3_stmt *stmt = nullptr;
    const char *sql = "SELECT key, value FROM meta";
    check_sqlite(sqlite3_prepare_v2(database_.handle(), sql, -1, &stmt, nullptr));
    meta_info info;
    while (true) {
        const int step = sqlite3_step(stmt);
        if (step == SQLITE_ROW) {
            const std::string key = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            const std::string value = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            if (key == "schema_version") {
                info.schema_version = value;
            }
            else if (key == "repo_commit") {
                info.repo_commit = value;
            }
            else if (key == "built_at") {
                info.built_at = value;
            }
            else if (key == "doc_count") {
                info.doc_count = static_cast<std::size_t>(std::stoull(value));
            }
            continue;
        }
        if (step == SQLITE_DONE) {
            break;
        }
        sqlite3_finalize(stmt);
        throw std::runtime_error("failed to read meta rows");
    }
    sqlite3_finalize(stmt);
    return info;
}
}
