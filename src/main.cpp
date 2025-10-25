#include "cli/cli_parser.h"
#include "config/app_config.h"
#include "writer/index_builder.h"

#include <exception>
#include <iostream>
#include <string_view>

namespace
{
constexpr std::string_view retort_version{"0.1.0"};

void print_usage()
{
    std::cout << R"(retort <command> [options]

Commands
  serve    Start HTTP search server
    --listen <addr>        Override listen host:port (default: 127.0.0.1:9000)
    --index_path <path>    SQLite database path (required)
    --threads <n>          Worker thread count (default: HW cores)
    --min_q <n>            Minimum query length (default: 2)
    --limit <n>            Default search limit (default: 20)
    --max_q_len <n>        Maximum allowed query length (default: 1024)
    --log_level <level>    Log level: silent | error | info | debug

  write    Build SQLite FTS index
    --src_dir <path>       Astro content directory
    --repo <path>          Repository root (for git metadata)
    --out <path>           Output SQLite file (required)
    --include-code         Include fenced code blocks in body
    --ngram <n>            Emit n-gram tokens (default: disabled)
    --max-bytes <n>        Per-file size limit (default: 1048576)

Environment variables override serve options (e.g. RETORT_LISTEN).

Global options
  -h, --help        Show this help
  -v, --version     Show version
)";
}

void print_version()
{
    std::cout << "retort " << retort_version << '\n';
}
}

namespace retort
{
int run_server(const serve_config &config);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        const std::string_view first{argv[1]};
        if (first == "--help" || first == "-h") {
            print_usage();
            return 0;
        }
        if (first == "--version" || first == "-v") {
            print_version();
            return 0;
        }
    }

    try {
        const auto parsed = retort::parse_cli(argc, argv);
        switch (parsed.command) {
        case retort::command_type::serve:
            return retort::run_server(parsed.serve);
        case retort::command_type::write:
            retort::build_index(parsed.write);
            return 0;
        }
    }
    catch (const std::exception &ex) {
        std::cerr << "retort error: " << ex.what() << '\n';
    }
    return 1;
}
