#pragma once

#include <cstdint>
#include <string>

namespace retort
{
struct document_row
{
    std::string doc_id;
    std::string url;
    std::string format;
    std::string title;
    std::string tags_json;
    std::string lang;
    std::int64_t updated_at = 0;
    std::string sha1;
    std::string body_tokens;
};
}
