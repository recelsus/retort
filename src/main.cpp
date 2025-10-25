#include "cli/cli_parser.h"
#include "config/app_config.h"
#include "writer/index_builder.h"

#include <exception>
#include <iostream>

namespace retort
{
int run_server(const serve_config &config);
}

int main(int argc, char **argv) {
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
