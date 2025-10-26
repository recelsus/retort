#include "index_builder.h"

#include "index/document.h"
#include "index/schema_migration.h"
#include "index/sqlite_database.h"
#include "writer/markdown_loader.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace retort
{
namespace
{
std::filesystem::path resolve_root(const write_config &config) {
    if (!config.source_directory.empty()) {
        const std::filesystem::path src_path{config.source_directory};
        if (src_path.is_absolute() || config.repository_root.empty()) {
            return src_path;
        }
        return std::filesystem::path{config.repository_root} / src_path;
    }
    if (!config.repository_root.empty()) {
        return std::filesystem::path{config.repository_root};
    }
    throw std::runtime_error("unable to resolve source root");
}

std::string iso8601_now() {
    const auto now = std::chrono::system_clock::now();
    const auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&now_time_t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::optional<std::string> read_repo_commit(const std::string &repo_root) {
    if (repo_root.empty()) {
        return std::nullopt;
    }
    std::string command = "git -C '" + repo_root + "' rev-parse HEAD 2>/dev/null";
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return std::nullopt;
    }
    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output.append(buffer);
    }
    const int status = pclose(pipe);
    if (status != 0) {
        return std::nullopt;
    }
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    if (output.empty()) {
        return std::nullopt;
    }
    return output;
}

std::filesystem::path resolve_output_file(const write_config &config) {
    std::filesystem::path raw_path{config.output_path.empty() ? std::string{"."} : config.output_path};
    std::error_code ec;
    bool treat_as_directory = false;

    const auto as_string = raw_path.generic_string();
    if (as_string.empty() || raw_path == "." || raw_path == "./") {
        treat_as_directory = true;
    } else {
        const char tail = as_string.back();
        if (tail == '/' || tail == '\\') {
            treat_as_directory = true;
        }
    }

    if (std::filesystem::exists(raw_path, ec) && std::filesystem::is_directory(raw_path, ec)) {
        treat_as_directory = true;
    }

    if (treat_as_directory) {
        std::filesystem::create_directories(raw_path, ec);
        if (ec) {
            throw std::runtime_error("failed to create output directory: " + raw_path.string());
        }
        raw_path /= "retort_index.sqlite";
    } else {
        const auto parent = raw_path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                throw std::runtime_error("failed to create output directory: " + parent.string());
            }
        }
    }

    return raw_path;
}

void write_meta(sqlite3 *db, const std::string &key, const std::string &value) {
    sqlite3_stmt *stmt = nullptr;
    const char *sql = "INSERT INTO meta(key, value) VALUES(?, ?) ON CONFLICT(key) DO UPDATE SET value=excluded.value";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("failed to prepare meta statement");
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("failed to write meta");
    }
    sqlite3_finalize(stmt);
}

void reset_table(sqlite3 *db, std::string_view table) {
    std::string query = "DELETE FROM ";
    query.append(table);
    if (sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error("failed to reset table");
    }
}
}

void build_index(const write_config &config) {
    const auto root_path = resolve_root(config);
    const auto output_path = resolve_output_file(config);
    markdown_options options{};
    options.include_code_blocks = config.include_code_blocks;
    options.ngram_size = config.ngram_size;
    options.max_bytes = config.max_bytes;

    const auto files = collect_markdown_files(root_path);
    if (files.empty()) {
        throw std::runtime_error("no markdown files found under " + root_path.string());
    }

    std::vector<document_row> documents;
    documents.reserve(files.size());

    for (const auto &file : files) {
        try {
            const auto converted = convert_markdown(root_path, file, options);
            if (converted.has_value()) {
                documents.push_back(*converted);
            }
        }
        catch (const std::exception &ex) {
            std::cerr << "skip file " << file << ": " << ex.what() << '\n';
        }
    }

    if (documents.empty()) {
        throw std::runtime_error("no documents indexed (status=publish only)");
    }

    std::error_code ec;
    std::filesystem::remove(output_path, ec);

    sqlite_database database{output_path};
    ensure_schema(database);
    auto *db = database.handle();

    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error("failed to begin transaction");
    }

    try {
        reset_table(db, "docs");
        reset_table(db, "docs_fts");

        sqlite3_stmt *docs_stmt = nullptr;
        const char *docs_sql =
            "INSERT INTO docs(doc_id, url, format, title, tags, lang, updated_at, sha1)"
            " VALUES(?, ?, ?, ?, ?, ?, ?, ?)"
            " ON CONFLICT(doc_id) DO UPDATE SET"
            " url=excluded.url, format=excluded.format, title=excluded.title,"
            " tags=excluded.tags, lang=excluded.lang, updated_at=excluded.updated_at, sha1=excluded.sha1";
        if (sqlite3_prepare_v2(db, docs_sql, -1, &docs_stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("failed to prepare docs statement");
        }

        sqlite3_stmt *fts_delete = nullptr;
        const char *fts_delete_sql = "DELETE FROM docs_fts WHERE doc_id = ?";
        if (sqlite3_prepare_v2(db, fts_delete_sql, -1, &fts_delete, nullptr) != SQLITE_OK) {
            sqlite3_finalize(docs_stmt);
            throw std::runtime_error("failed to prepare fts delete");
        }

        sqlite3_stmt *fts_insert = nullptr;
        const char *fts_insert_sql = "INSERT INTO docs_fts(doc_id, title, body_tokens) VALUES(?, ?, ?)";
        if (sqlite3_prepare_v2(db, fts_insert_sql, -1, &fts_insert, nullptr) != SQLITE_OK) {
            sqlite3_finalize(docs_stmt);
            sqlite3_finalize(fts_delete);
            throw std::runtime_error("failed to prepare fts insert");
        }

        for (const auto &doc : documents) {
            sqlite3_reset(docs_stmt);
            sqlite3_clear_bindings(docs_stmt);
            sqlite3_bind_text(docs_stmt, 1, doc.doc_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(docs_stmt, 2, doc.url.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(docs_stmt, 3, doc.format.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(docs_stmt, 4, doc.title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(docs_stmt, 5, doc.tags_json.c_str(), -1, SQLITE_TRANSIENT);
            if (doc.lang.empty()) {
                sqlite3_bind_null(docs_stmt, 6);
            }
            else {
                sqlite3_bind_text(docs_stmt, 6, doc.lang.c_str(), -1, SQLITE_TRANSIENT);
            }
            sqlite3_bind_int64(docs_stmt, 7, doc.updated_at);
            sqlite3_bind_text(docs_stmt, 8, doc.sha1.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(docs_stmt) != SQLITE_DONE) {
                throw std::runtime_error("failed to insert docs row");
            }

            sqlite3_reset(fts_delete);
            sqlite3_clear_bindings(fts_delete);
            sqlite3_bind_text(fts_delete, 1, doc.doc_id.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(fts_delete) != SQLITE_DONE) {
                throw std::runtime_error("failed to delete existing fts row");
            }

            sqlite3_reset(fts_insert);
            sqlite3_clear_bindings(fts_insert);
            sqlite3_bind_text(fts_insert, 1, doc.doc_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(fts_insert, 2, doc.title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(fts_insert, 3, doc.body_tokens.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(fts_insert) != SQLITE_DONE) {
                throw std::runtime_error("failed to insert fts row");
            }
        }

        sqlite3_finalize(docs_stmt);
        sqlite3_finalize(fts_delete);
        sqlite3_finalize(fts_insert);

        write_meta(db, "schema_version", "1");
        write_meta(db, "doc_count", std::to_string(documents.size()));
        write_meta(db, "built_at", iso8601_now());
        const auto commit_hash = read_repo_commit(config.repository_root);
        write_meta(db, "repo_commit", commit_hash.value_or("unknown"));

        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
            throw std::runtime_error("failed to commit transaction");
        }
    }
    catch (...) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }
}
}
