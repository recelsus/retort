#include "config/app_config.h"
#include "index/sqlite_database.h"
#include "search/query_service.h"
#include "util/json.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace retort
{
namespace
{
struct http_request
{
    std::string method;
    std::string target_path;
    std::string query_string;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct meta_runtime
{
    std::unique_ptr<sqlite_database> database;
    std::unique_ptr<query_service> queries;
    meta_info meta;
};

std::pair<std::string, std::string> split_listen_address(const std::string &address) {
    const auto pos = address.rfind(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("listen address must include port");
    }
    std::string host = address.substr(0U, pos);
    std::string port = address.substr(pos + 1U);
    if (port.empty()) {
        throw std::runtime_error("port is empty");
    }
    if (host.empty()) {
        host = "0.0.0.0";
    }
    return {host, port};
}

int create_listen_socket(const std::string &host, const std::string &port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo *result = nullptr;
    const int res = getaddrinfo(host.empty() ? nullptr : host.c_str(), port.c_str(), &hints, &result);
    if (res != 0) {
        throw std::runtime_error("getaddrinfo failed: " + std::string{gai_strerror(res)});
    }

    int listen_fd = -1;
    for (addrinfo *ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        listen_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (listen_fd == -1) {
            continue;
        }
        int enable = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
        if (bind(listen_fd, ptr->ai_addr, ptr->ai_addrlen) == 0) {
            if (listen(listen_fd, 64) == 0) {
                break;
            }
        }
        close(listen_fd);
        listen_fd = -1;
    }

    freeaddrinfo(result);

    if (listen_fd == -1) {
        throw std::runtime_error("failed to bind listen socket");
    }
    return listen_fd;
}

std::string read_until(int fd, std::size_t expected) {
    std::string data;
    data.reserve(expected);
    while (data.size() < expected) {
        char buffer[4096];
        const auto remaining = expected - data.size();
        const ssize_t chunk = recv(fd, buffer, std::min<std::size_t>(sizeof(buffer), remaining), 0);
        if (chunk <= 0) {
            break;
        }
        data.append(buffer, buffer + chunk);
    }
    return data;
}

std::string read_request_buffer(int fd) {
    std::string buffer;
    char temp[4096];
    while (true) {
        const ssize_t bytes = recv(fd, temp, sizeof(temp), 0);
        if (bytes <= 0) {
            break;
        }
        buffer.append(temp, temp + bytes);
        if (buffer.find("\r\n\r\n") != std::string::npos) {
            break;
        }
        if (buffer.size() > 1'048'576U) {
            break;
        }
    }
    return buffer;
}

std::optional<http_request> parse_http_request(int fd) {
    std::string buffer = read_request_buffer(fd);
    if (buffer.empty()) {
        return std::nullopt;
    }
    const auto header_end = buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return std::nullopt;
    }
    std::string header_part = buffer.substr(0U, header_end);
    std::string remaining = buffer.substr(header_end + 4U);

    std::stringstream stream{header_part};
    std::string request_line;
    if (!std::getline(stream, request_line)) {
        return std::nullopt;
    }
    if (request_line.ends_with('\r')) {
        request_line.pop_back();
    }
    std::stringstream line_stream{request_line};
    http_request request;
    if (!(line_stream >> request.method)) {
        return std::nullopt;
    }
    std::string target;
    if (!(line_stream >> target)) {
        return std::nullopt;
    }
    std::string version;
    if (!(line_stream >> version)) {
        return std::nullopt;
    }
    if (version != "HTTP/1.1") {
        return std::nullopt;
    }
    const auto query_pos = target.find('?');
    if (query_pos == std::string::npos) {
        request.target_path = target;
    }
    else {
        request.target_path = target.substr(0U, query_pos);
        request.query_string = target.substr(query_pos + 1U);
    }

    std::string header_line;
    while (std::getline(stream, header_line)) {
        if (header_line.ends_with('\r')) {
            header_line.pop_back();
        }
        const auto colon = header_line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string key = header_line.substr(0U, colon);
        key.erase(std::remove_if(key.begin(), key.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }), key.end());
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::string value = header_line.substr(colon + 1U);
        const auto begin = value.find_first_not_of(" \t");
        if (begin == std::string::npos) {
            value.clear();
        }
        else {
            const auto end = value.find_last_not_of(" \t");
            value = value.substr(begin, end - begin + 1U);
        }
        request.headers[key] = value;
    }

    const auto it_length = request.headers.find("content-length");
    if (it_length != request.headers.end()) {
        std::size_t length = 0U;
        try {
            length = static_cast<std::size_t>(std::stoul(it_length->second));
        }
        catch (...) {
            length = 0U;
        }
        if (remaining.size() < length) {
            const auto rest = read_until(fd, length - remaining.size());
            remaining.append(rest);
        }
        request.body = remaining.substr(0U, length);
    }
    else {
        request.body = std::move(remaining);
    }
    return request;
}

std::string url_decode(const std::string &value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0U; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '+') {
            result.push_back(' ');
            continue;
        }
        if (ch == '%' && i + 2U < value.size()) {
            const auto hex = value.substr(i + 1U, 2U);
            int code = 0;
            std::stringstream ss;
            ss << std::hex << hex;
            ss >> code;
            result.push_back(static_cast<char>(code));
            i += 2U;
            continue;
        }
        result.push_back(ch);
    }
    return result;
}

std::unordered_map<std::string, std::string> parse_query_map(const std::string &query) {
    std::unordered_map<std::string, std::string> map;
    std::size_t start = 0U;
    while (start < query.size()) {
        const auto amp = query.find('&', start);
        const auto segment = query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        const auto eq = segment.find('=');
        if (eq != std::string::npos) {
            const auto key = url_decode(segment.substr(0U, eq));
            const auto value = url_decode(segment.substr(eq + 1U));
            map[key] = value;
        }
        else if (!segment.empty()) {
            map[url_decode(segment)] = "";
        }
        if (amp == std::string::npos) {
            break;
        }
        start = amp + 1U;
    }
    return map;
}

std::string build_response_body(const std::vector<search_hit> &hits, const meta_info &meta) {
    std::ostringstream oss;
    oss << "{\"hits\":[";
    for (std::size_t i = 0U; i < hits.size(); ++i) {
        if (i > 0U) {
            oss << ',';
        }
        const auto &hit = hits[i];
        oss << '{'
            << "\"url\":\"" << json_escape(hit.url) << '\"'
            << ",\"title\":\"" << json_escape(hit.title) << '\"'
            << ",\"format\":\"" << json_escape(hit.format) << '\"'
            << ",\"tags\":" << hit.tags_json
            << ",\"lang\":\"" << json_escape(hit.lang) << '\"'
            << ",\"updated_at\":" << hit.updated_at
            << ",\"score\":" << hit.score
            << ",\"snippet\":\"" << json_escape(hit.snippet) << '\"'
            << '}';
    }
    oss << "],\"count\":" << hits.size()
        << ",\"repo_commit\":\"" << json_escape(meta.repo_commit) << '\"'
        << '}';
    return oss.str();
}

std::string build_meta_body(const meta_info &meta) {
    std::ostringstream oss;
    oss << "{"
        << "\"schema_version\":\"" << json_escape(meta.schema_version) << '\"'
        << ",\"repo_commit\":\"" << json_escape(meta.repo_commit) << '\"'
        << ",\"built_at\":\"" << json_escape(meta.built_at) << '\"'
        << ",\"doc_count\":" << meta.doc_count
        << '}';
    return oss.str();
}

std::string http_status_reason(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 413:
        return "Payload Too Large";
    case 500:
        return "Internal Server Error";
    default:
        return "OK";
    }
}

void send_response(int fd, int status, const std::vector<std::pair<std::string, std::string>> &headers, const std::string &body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << ' ' << http_status_reason(status) << "\r\n";
    for (const auto &header : headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    const auto payload = oss.str();
    const char *data = payload.data();
    std::size_t remaining = payload.size();
    while (remaining > 0U) {
        const ssize_t sent = send(fd, data, remaining, 0);
        if (sent <= 0) {
            break;
        }
        data += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
}

bool verify_admin(const serve_config &config, const http_request &request) {
    if (!config.admin_token.has_value() || config.admin_token->empty()) {
        return false;
    }
    const auto it = request.headers.find("authorization");
    if (it == request.headers.end()) {
        return false;
    }
    const std::string expected = "Bearer " + *config.admin_token;
    return it->second == expected;
}

meta_runtime open_runtime(const serve_config &config) {
    meta_runtime data;
    data.database = std::make_unique<sqlite_database>(config.index_path, SQLITE_OPEN_READONLY);
    data.queries = std::make_unique<query_service>(*data.database);
    data.meta = data.queries->load_meta();
    return data;
}

void handle_search(int fd,
                   const serve_config &config,
                   meta_runtime &runtime,
                   const http_request &request,
                   const std::unordered_map<std::string, std::string> &params) {
    const auto it_query = params.find("q");
    if (it_query == params.end()) {
        send_response(fd, 400, {{"Content-Type", "application/json"}}, "{\"error\":\"missing q\"}");
        return;
    }

    std::string query = it_query->second;
    const auto begin = query.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        send_response(fd, 400, {{"Content-Type", "application/json"}}, "{\"error\":\"empty query\"}");
        return;
    }
    const auto end = query.find_last_not_of(" \t\r\n");
    query = query.substr(begin, end - begin + 1U);

    if (query.size() < config.min_query_length) {
        send_response(fd, 400, {{"Content-Type", "application/json"}}, "{\"error\":\"query too short\"}");
        return;
    }
    if (query.size() > config.max_query_length) {
        send_response(fd, 413, {{"Content-Type", "application/json"}}, "{\"error\":\"query too long\"}");
        return;
    }

    std::size_t limit = config.default_limit;
    const auto it_limit = params.find("limit");
    if (it_limit != params.end()) {
        try {
            limit = static_cast<std::size_t>(std::stoul(it_limit->second));
        }
        catch (...) {
            limit = config.default_limit;
        }
    }
    limit = std::min(limit, config.max_limit);
    if (limit == 0U) {
        limit = 1U;
    }

    std::size_t offset = 0U;
    const auto it_offset = params.find("offset");
    if (it_offset != params.end()) {
        try {
            offset = static_cast<std::size_t>(std::stoul(it_offset->second));
        }
        catch (...) {
            offset = 0U;
        }
    }

    std::vector<search_hit> hits;
    try {
        hits = runtime.queries->search(query, limit, offset);
    }
    catch (const std::exception &ex) {
        send_response(fd, 500, {{"Content-Type", "application/json"}}, "{\"error\":\"search failed\"}");
        std::cerr << "search error: " << ex.what() << '\n';
        return;
    }

    const auto body = build_response_body(hits, runtime.meta);
    send_response(fd,
                  200,
                  {{"Content-Type", "application/json"}, {"Cache-Control", "no-store"}, {"X-Index-Version", runtime.meta.repo_commit}},
                  body);
}

void handle_meta(int fd, const meta_runtime &runtime) {
    const auto body = build_meta_body(runtime.meta);
    send_response(fd,
                  200,
                  {{"Content-Type", "application/json"}, {"Cache-Control", "no-store"}, {"X-Index-Version", runtime.meta.repo_commit}},
                  body);
}

void handle_health(int fd) {
    send_response(fd, 200, {{"Content-Type", "text/plain"}}, "ok");
}

void handle_reopen(int fd, const serve_config &config, meta_runtime &runtime, const http_request &request) {
    if (!verify_admin(config, request)) {
        send_response(fd, 401, {{"Content-Type", "application/json"}}, "{\"error\":\"unauthorized\"}");
        return;
    }
    try {
        runtime = open_runtime(config);
    }
    catch (const std::exception &ex) {
        send_response(fd, 500, {{"Content-Type", "application/json"}}, "{\"error\":\"reload failed\"}");
        std::cerr << "reload error: " << ex.what() << '\n';
        return;
    }
    send_response(fd, 204, {{"X-Index-Version", runtime.meta.repo_commit}}, "");
}

void route_request(int fd, const serve_config &config, meta_runtime &runtime, const http_request &request) {
    if (request.method == "GET" && request.target_path == "/search") {
        const auto params = parse_query_map(request.query_string);
        handle_search(fd, config, runtime, request, params);
        return;
    }

    if (request.method == "GET" && request.target_path == "/meta") {
        handle_meta(fd, runtime);
        return;
    }

    if (request.method == "GET" && request.target_path == "/healthz") {
        handle_health(fd);
        return;
    }

    if (request.method == "POST" && request.target_path == "/admin/reopen") {
        handle_reopen(fd, config, runtime, request);
        return;
    }

    send_response(fd, 404, {{"Content-Type", "application/json"}}, "{\"error\":\"not found\"}");
}
}

int run_server(const serve_config &config) {
    const auto [host, port] = split_listen_address(config.listen_address);
    meta_runtime runtime;
    try {
        runtime = open_runtime(config);
    }
    catch (const std::exception &ex) {
        std::cerr << "failed to open index: " << ex.what() << '\n';
        return 1;
    }

    int listen_fd = -1;
    try {
        listen_fd = create_listen_socket(host, port);
    }
    catch (const std::exception &ex) {
        std::cerr << "listen error: " << ex.what() << '\n';
        return 1;
    }

    std::cout << "retort serve listening on " << host << ':' << port << '\n';

    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        const int client_fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "accept failed: " << std::strerror(errno) << '\n';
            break;
        }

        const auto request = parse_http_request(client_fd);
        if (!request.has_value()) {
            send_response(client_fd, 400, {{"Content-Type", "application/json"}}, "{\"error\":\"bad request\"}");
            close(client_fd);
            continue;
        }

        try {
            route_request(client_fd, config, runtime, *request);
        }
        catch (const std::exception &ex) {
            send_response(client_fd, 500, {{"Content-Type", "application/json"}}, "{\"error\":\"internal server error\"}");
            std::cerr << "handler error: " << ex.what() << '\n';
        }
        close(client_fd);
    }

    close(listen_fd);
    return 0;
}
}
