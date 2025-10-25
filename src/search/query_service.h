#pragma once

#include "config/app_config.h"
#include "index/sqlite_database.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace retort
{
struct search_hit
{
    std::string url;
    std::string title;
    std::string format;
    std::string tags_json;
    std::string lang;
    std::int64_t updated_at = 0;
    double score = 0.0;
    std::string snippet;
};

struct meta_info
{
    std::string schema_version;
    std::string repo_commit;
    std::string built_at;
    std::size_t doc_count = 0U;
};

class query_service
{
public:
    explicit query_service(sqlite_database &database);

    std::vector<search_hit> search(const std::string &query,
                                   std::size_t limit,
                                   std::size_t offset) const;

    meta_info load_meta() const;

private:
    sqlite_database &database_;
};
}
