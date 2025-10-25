#include "cli_parser.h"

#include "util/env.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace retort
{
namespace
{
bool is_flag(const std::string &arg) {
    return arg.size() > 2 && arg.starts_with("--");
}

std::size_t parse_size(std::string_view value) {
    std::size_t number = 0U;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("invalid numeric value: " + std::string{value});
    }
    return number;
}

int parse_int(std::string_view value) {
    int number = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("invalid numeric value: " + std::string{value});
    }
    return number;
}

std::optional<std::string> read_env_optional(std::string_view name) {
    const auto value = get_env(name);
    if (value.has_value() && value->empty()) {
        return std::nullopt;
    }
    return value;
}

std::size_t read_env_size(std::string_view name, std::size_t fallback) {
    const auto value = get_env(name);
    if (!value.has_value()) {
        return fallback;
    }
    return parse_size(*value);
}

std::string take_value(int &index, int argc, char **argv) {
    const int target = index + 1;
    if (target >= argc) {
        throw std::runtime_error("missing value for option: " + std::string{argv[index]});
    }
    index = target;
    return std::string{argv[target]};
}
}

cli_result parse_cli(int argc, char **argv) {
    if (argc < 2) {
        throw std::runtime_error("command is required");
    }

    const std::string command_name{argv[1]};

    if (command_name == "serve") {
        serve_config config{};
        config.listen_address = get_env_or("RETORT_LISTEN", config.listen_address);
        config.index_path = get_env_or("RETORT_INDEX_PATH", config.index_path);
        const auto env_token = read_env_optional("RETORT_ADMIN_TOKEN");
        if (env_token.has_value()) {
            config.admin_token = env_token;
        }
        const std::size_t default_threads = std::thread::hardware_concurrency();
        config.thread_count = read_env_size("RETORT_THREADS", default_threads == 0U ? 1U : default_threads);
        config.min_query_length = read_env_size("RETORT_MIN_Q", config.min_query_length);
        config.default_limit = read_env_size("RETORT_DEFAULT_LIMIT", config.default_limit);
        config.max_query_length = read_env_size("RETORT_MAX_Q_LEN", config.max_query_length);
        config.log_level = get_env_or("RETORT_LOG_LEVEL", config.log_level);

        for (int i = 2; i < argc; ++i) {
            const std::string arg{argv[i]};
            if (!is_flag(arg)) {
                throw std::runtime_error("unexpected positional argument: " + arg);
            }

            if (arg == "--listen") {
                config.listen_address = take_value(i, argc, argv);
            }
            else if (arg == "--index_path") {
                config.index_path = take_value(i, argc, argv);
            }
            else if (arg == "--admin_token") {
                config.admin_token = take_value(i, argc, argv);
            }
            else if (arg == "--threads") {
                config.thread_count = parse_size(take_value(i, argc, argv));
            }
            else if (arg == "--min_q") {
                config.min_query_length = parse_size(take_value(i, argc, argv));
            }
            else if (arg == "--limit") {
                config.default_limit = parse_size(take_value(i, argc, argv));
            }
            else if (arg == "--max_q_len") {
                config.max_query_length = parse_size(take_value(i, argc, argv));
            }
            else if (arg == "--log_level") {
                config.log_level = take_value(i, argc, argv);
            }
            else {
                throw std::runtime_error("unknown option: " + arg);
            }
        }

        if (config.index_path.empty()) {
            throw std::runtime_error("--index_path or RETORT_INDEX_PATH is required");
        }

        if (config.default_limit == 0U || config.default_limit > config.max_limit) {
            config.default_limit = std::min<std::size_t>(20U, config.max_limit);
        }

        cli_result result{};
        result.command = command_type::serve;
        result.serve = config;
        return result;
    }

    if (command_name == "write") {
        write_config config{};
        for (int i = 2; i < argc; ++i) {
            const std::string arg{argv[i]};
            if (!is_flag(arg)) {
                throw std::runtime_error("unexpected positional argument: " + arg);
            }

            if (arg == "--src_dir") {
                config.source_directory = take_value(i, argc, argv);
            }
            else if (arg == "--repo") {
                config.repository_root = take_value(i, argc, argv);
            }
            else if (arg == "--out") {
                config.output_path = take_value(i, argc, argv);
            }
            else if (arg == "--include-code") {
                config.include_code_blocks = true;
            }
            else if (arg == "--ngram") {
                config.ngram_size = parse_int(take_value(i, argc, argv));
            }
            else if (arg == "--max-bytes") {
                config.max_bytes = parse_size(take_value(i, argc, argv));
            }
            else {
                throw std::runtime_error("unknown option: " + arg);
            }
        }

        if (config.source_directory.empty() && config.repository_root.empty()) {
            throw std::runtime_error("--src_dir or --repo is required");
        }

        cli_result result{};
        result.command = command_type::write;
        result.write = config;
        return result;
    }

    throw std::runtime_error("unknown command: " + command_name);
}
}
