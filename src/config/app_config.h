#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace retort
{
enum class command_type
{
    serve,
    write
};

struct serve_config
{
    std::string listen_address = "127.0.0.1:9000";
    std::string index_path;
    std::optional<std::string> admin_token;
    std::size_t thread_count = 0U;
    std::size_t min_query_length = 2U;
    std::size_t default_limit = 20U;
    std::size_t max_limit = 100U;
    std::size_t max_query_length = 1024U;
    std::string log_level = "info";
};

struct write_config
{
    std::string source_directory;
    std::string repository_root;
    std::string output_path = ".";
    bool include_code_blocks = false;
    std::optional<int> ngram_size;
    std::size_t max_bytes = 1024U * 1024U;
};

struct cli_result
{
    command_type command;
    serve_config serve;
    write_config write;
};
}
