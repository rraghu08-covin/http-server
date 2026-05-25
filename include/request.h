#pragma once
#include <string>
#include <map>

// ── HTTP method enum ──────────────────────────────────────────────────────────
enum class HttpMethod { GET, POST, HEAD, PUT, DELETE, OPTIONS, UNKNOWN };

// ── HttpRequest ───────────────────────────────────────────────────────────────
struct HttpRequest {
    HttpMethod  method    = HttpMethod::UNKNOWN;
    std::string methodStr;
    std::string path;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    bool        valid    = false;
    std::string errorMsg;

    // Parse a raw HTTP/1.1 request string.
    // Sets valid=false and populates errorMsg on failure.
    static HttpRequest parse(const std::string& raw);

    // Case-insensitive header lookup. Returns "" if not found.
    std::string getHeader(const std::string& name) const;
};
