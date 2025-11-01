#include "markdown_loader.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace retort
{
namespace
{
using string_map = std::unordered_map<std::string, std::string>;

std::string trim_copy(std::string_view view) {
    const auto begin = view.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }
    const auto end = view.find_last_not_of(" \t\r\n");
    return std::string{view.substr(begin, end - begin + 1U)};
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string strip_quotes(std::string value) {
    if (value.size() >= 2U && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1U, value.size() - 2U);
    }
    return value;
}

string_map parse_frontmatter_lines(const std::vector<std::string> &lines) {
    string_map result;
    for (const auto &line : lines) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const auto key = trim_copy(std::string_view{line}.substr(0U, colon));
        const auto value = trim_copy(std::string_view{line}.substr(colon + 1U));
        if (!key.empty()) {
            result[key] = strip_quotes(value);
        }
    }
    return result;
}

std::vector<std::string> parse_tags(const std::string &value) {
    std::string working = value;
    if (working.starts_with('[') && working.ends_with(']')) {
        working = working.substr(1U, working.size() - 2U);
    }
    std::vector<std::string> tags;
    std::stringstream stream{working};
    std::string item;
    while (std::getline(stream, item, ',')) {
        const auto trimmed = trim_copy(item);
        if (!trimmed.empty()) {
            tags.push_back(strip_quotes(trimmed));
        }
    }
    return tags;
}

std::string tags_to_json(const std::vector<std::string> &tags) {
    if (tags.empty()) {
        return "[]";
    }
    std::ostringstream oss;
    oss << '[';
    for (std::size_t i = 0U; i < tags.size(); ++i) {
        if (i > 0U) {
            oss << ',';
        }
        std::string value = tags[i];
        oss << '"';
        for (const char ch : value) {
            if (ch == '"') {
                oss << "\\\"";
            }
            else {
                oss << ch;
            }
        }
        oss << '"';
    }
    oss << ']';
    return oss.str();
}

std::string read_file(const std::filesystem::path &path, std::size_t max_bytes) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error("failed to open file: " + path.string());
    }
    stream.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(stream.tellg());
    if (size > max_bytes) {
        throw std::runtime_error("file exceeds max bytes: " + path.string());
    }
    stream.seekg(0, std::ios::beg);
    std::string contents(size, '\0');
    if (size > 0U) {
        stream.read(contents.data(), static_cast<std::streamsize>(size));
    }
    return contents;
}

std::pair<string_map, std::string> split_frontmatter(const std::string &contents) {
    if (!contents.starts_with("---")) {
        return {string_map{}, contents};
    }
    const auto end = contents.find("\n---", 3U);
    if (end == std::string::npos) {
        return {string_map{}, contents};
    }
    const auto after = contents.find("\n", end + 4U);
    const auto fm_block = contents.substr(3U, end - 3U);
    std::vector<std::string> lines;
    std::stringstream ss{fm_block};
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    return {parse_frontmatter_lines(lines), contents.substr(after == std::string::npos ? contents.size() : after + 1U)};
}

std::string find_first_heading(const std::string &body) {
    std::stringstream stream{body};
    std::string line;
    while (std::getline(stream, line)) {
        auto trimmed = trim_copy(line);
        if (trimmed.starts_with('#')) {
            trimmed.erase(0U, trimmed.find_first_not_of('#'));
            trimmed = trim_copy(trimmed);
            if (!trimmed.empty()) {
                return trimmed;
            }
        }
    }
    return {};
}

std::string fallback_title(const std::filesystem::path &path) {
    std::string base = path.stem().string();
    if (base == "index") {
        const auto parent = path.parent_path().filename().string();
        if (!parent.empty()) {
            base = parent;
        }
    }
    std::replace(base.begin(), base.end(), '-', ' ');
    std::replace(base.begin(), base.end(), '_', ' ');
    if (!base.empty()) {
        base[0] = static_cast<char>(std::toupper(base[0]));
    }
    return base;
}

std::string build_doc_id(const std::filesystem::path &root_path, const std::filesystem::path &file_path) {
    const auto relative = std::filesystem::relative(file_path, root_path);
    return relative.generic_string();
}

std::string build_url(const std::filesystem::path &root_path,
    const std::filesystem::path &file_path,
    const string_map &frontmatter) {
    const auto it_url = frontmatter.find("url");
    if (it_url != frontmatter.end() && !it_url->second.empty()) {
        if (it_url->second.starts_with('/')) {
            return it_url->second;
        }
        return '/' + it_url->second;
    }
    const auto it_slug = frontmatter.find("slug");
    if (it_slug != frontmatter.end() && !it_slug->second.empty()) {
        std::string slug = it_slug->second;
        if (!slug.starts_with('/')) {
            slug = '/' + slug;
        }
        if (!slug.ends_with('/')) {
            slug.push_back('/');
        }
        return slug;
    }

    auto relative = std::filesystem::relative(file_path, root_path);
    relative.replace_extension();
    std::string url = '/' + relative.generic_string();
    if (file_path.filename() == "index.md" || file_path.filename() == "index.mdx") {
        const auto parent = relative.parent_path().generic_string();
        url = parent.empty() ? std::string{"/"} : '/' + parent;
    }
    if (!url.ends_with('/')) {
        url.push_back('/');
    }
    std::string collapsed;
    collapsed.reserve(url.size());
    bool previous_slash = false;
    for (const char ch : url) {
        if (ch == '/') {
            if (!previous_slash) {
                collapsed.push_back(ch);
            }
            previous_slash = true;
        }
        else {
            collapsed.push_back(ch);
            previous_slash = false;
        }
    }
    return collapsed;
}

std::string remove_code_blocks(const std::string &body, bool include_code_blocks) {
    if (include_code_blocks) {
        return body;
    }
    std::stringstream input{body};
    std::ostringstream output;
    std::string line;
    bool skipping = false;
    while (std::getline(input, line)) {
        if (line.starts_with("```") || line.starts_with("~~~")) {
            skipping = !skipping;
            continue;
        }
        if (!skipping) {
            output << line << '\n';
        }
    }
    return output.str();
}

std::string strip_mdx(const std::string &body) {
    std::ostringstream output;
    bool inside_tag = false;
    for (std::size_t i = 0U; i < body.size(); ++i) {
        const char ch = body[i];
        if (ch == '<') {
            inside_tag = true;
            continue;
        }
        if (ch == '>') {
            inside_tag = false;
            continue;
        }
        if (!inside_tag) {
            output << ch;
        }
    }
    return output.str();
}

std::string collapse_punctuation(const std::string &input) {
    std::string output;
    output.reserve(input.size());
    bool last_space = false;
    for (unsigned char uc : input) {
        if (std::isalnum(uc)) {
            output.push_back(static_cast<char>(std::tolower(uc)));
            last_space = false;
        }
        else if (static_cast<unsigned char>(uc) >= 128U) {
            output.push_back(static_cast<char>(uc));
            last_space = false;
        }
        else {
            if (!last_space) {
                output.push_back(' ');
                last_space = true;
            }
        }
    }
    return output;
}

std::string collapse_spaces(const std::string &input) {
    std::string output;
    output.reserve(input.size());
    bool last_space = true;
    for (char ch : input) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!last_space) {
                output.push_back(' ');
                last_space = true;
            }
        }
        else {
            output.push_back(ch);
            last_space = false;
        }
    }
    if (!output.empty() && output.back() == ' ') {
        output.pop_back();
    }
    return output;
}

std::string build_tokens(const std::string &input, const std::optional<int> &ngram_size) {
    const auto collapsed = collapse_spaces(collapse_punctuation(input));
    if (!ngram_size.has_value() || *ngram_size <= 1) {
        return collapsed;
    }
    std::string compact;
    compact.reserve(collapsed.size());
    for (char ch : collapsed) {
        if (ch != ' ') {
            compact.push_back(ch);
        }
    }
    std::ostringstream tokens;
    tokens << collapsed;
    if (!collapsed.empty()) {
        tokens << ' ';
    }
    const int n = *ngram_size;
    if (n > 0) {
        for (std::size_t i = 0U; i + static_cast<std::size_t>(n) <= compact.size(); ++i) {
            if (i > 0U) {
                tokens << ' ';
            }
            tokens << compact.substr(i, static_cast<std::size_t>(n));
        }
    }
    return tokens.str();
}

std::int64_t file_timestamp(const std::filesystem::path &path) {
    const auto time_point = std::filesystem::last_write_time(path);
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    const auto system_time = std::chrono::clock_cast<std::chrono::system_clock>(time_point);
#else
    // Fallback conversion when clock_cast is unavailable (older libstdc++/libc++).
    const auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        time_point - decltype(time_point)::clock::now() + std::chrono::system_clock::now());
#endif
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(system_time.time_since_epoch());
    return seconds.count();
}

std::string compute_digest(const document_row &row) {
    std::string buffer;
    buffer.reserve(row.title.size() + row.body_tokens.size());
    buffer.append(row.title);
    buffer.push_back('\n');
    buffer.append(row.body_tokens);
    const auto value = std::hash<std::string>{}(buffer);
    std::ostringstream oss;
    oss << std::hex << value;
    return oss.str();
}
}

std::optional<document_row> convert_markdown(const std::filesystem::path &root_path,
    const std::filesystem::path &file_path,
    const markdown_options &options) {
    const auto contents = read_file(file_path, options.max_bytes);
    const auto [frontmatter, body_raw] = split_frontmatter(contents);
    const auto it_draft = frontmatter.find("draft");
    if (it_draft != frontmatter.end()) {
        const auto value = trim_copy(it_draft->second);
        if (value == "true" || value == "True" || value == "1") {
            return std::nullopt;
        }
    }

    const auto it_status = frontmatter.find("status");
    if (it_status != frontmatter.end()) {
        const auto status_value = to_lower_ascii(trim_copy(it_status->second));
        if (status_value != "publish") {
            return std::nullopt;
        }
    }

    const bool is_mdx = file_path.extension() == ".mdx";
    std::string body = body_raw;
    body = remove_code_blocks(body, options.include_code_blocks);
    if (is_mdx) {
        body = strip_mdx(body);
    }

    document_row row;
    row.doc_id = build_doc_id(root_path, file_path);
    row.format = is_mdx ? "mdx" : "md";
    row.url = build_url(root_path, file_path, frontmatter);

    std::string title;
    const auto it_title = frontmatter.find("title");
    if (it_title != frontmatter.end()) {
        title = it_title->second;
    }
    else {
        title = find_first_heading(body);
        if (title.empty()) {
            title = fallback_title(file_path);
        }
    }
    row.title = title;

    const auto it_lang = frontmatter.find("lang");
    if (it_lang != frontmatter.end()) {
        row.lang = it_lang->second;
    }

    const auto it_tags = frontmatter.find("tags");
    if (it_tags != frontmatter.end()) {
        const auto tags = parse_tags(it_tags->second);
        row.tags_json = tags_to_json(tags);
    }
    else {
        row.tags_json = "[]";
    }

    row.updated_at = file_timestamp(file_path);
    row.body_tokens = build_tokens(body, options.ngram_size);
    row.sha1 = compute_digest(row);
    return row;
}

std::vector<std::filesystem::path> collect_markdown_files(const std::filesystem::path &root_path) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(root_path)) {
        throw std::runtime_error("source path does not exist: " + root_path.string());
    }
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto ext = entry.path().extension().string();
        if (ext == ".md" || ext == ".mdx") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}
}
