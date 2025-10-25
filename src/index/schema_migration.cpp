#include "schema_migration.h"

namespace retort
{
void ensure_schema(sqlite_database &db) {
    db.exec("PRAGMA journal_mode=OFF;");
    db.exec("PRAGMA synchronous=OFF;");
    db.exec("PRAGMA temp_store=MEMORY;");
    db.exec("PRAGMA cache_size=-20000;");
    db.exec("PRAGMA mmap_size=268435456;");

    db.exec(
        "CREATE TABLE IF NOT EXISTS docs ("
        " doc_id TEXT PRIMARY KEY,"
        " url TEXT NOT NULL,"
        " format TEXT NOT NULL,"
        " title TEXT NOT NULL,"
        " tags TEXT,"
        " lang TEXT,"
        " updated_at INTEGER NOT NULL,"
        " sha1 TEXT NOT NULL"
        ");");

    db.exec(
        "CREATE TABLE IF NOT EXISTS meta ("
        " key TEXT PRIMARY KEY,"
        " value TEXT NOT NULL"
        ");");

    db.exec(
        "CREATE VIRTUAL TABLE IF NOT EXISTS docs_fts"
        " USING fts5(doc_id UNINDEXED, title, body_tokens, tokenize='unicode61');");

    db.exec(
        "CREATE VIEW IF NOT EXISTS v_search AS"
        " SELECT d.url, d.title, d.format, d.tags, d.lang, d.updated_at, d.doc_id"
        " FROM docs d;");
}
}
