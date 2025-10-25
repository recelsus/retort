#pragma once

#include "index/document.h"
#include "config/app_config.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace retort
{
struct markdown_options
{
    bool include_code_blocks = false;
    std::optional<int> ngram_size;
    std::size_t max_bytes = 1024U * 1024U;
};

std::optional<document_row> convert_markdown(const std::filesystem::path &root_path,
                                             const std::filesystem::path &file_path,
                                             const markdown_options &options);

std::vector<std::filesystem::path> collect_markdown_files(const std::filesystem::path &root_path);
}
