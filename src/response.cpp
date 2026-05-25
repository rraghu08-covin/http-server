#include "response.h"
#include "utils.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <ctime>

#include <dirent.h>
#include <sys/stat.h>

// ── Status text lookup ────────────────────────────────────────────────────────
static const char* statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
        default:  return "Unknown";
    }
}

// ── setHeader ─────────────────────────────────────────────────────────────────
void HttpResponse::setHeader(const std::string& name, const std::string& value) {
    headers[name] = value;
}

// ── make ──────────────────────────────────────────────────────────────────────
HttpResponse HttpResponse::make(int code, const std::string& body,
                                const std::string& contentType) {
    HttpResponse resp;
    resp.statusCode = code;
    resp.statusText = statusText(code);
    resp.body       = body;
    resp.setHeader("Server",         "http-server/1.0");
    resp.setHeader("Date",           utils::httpDate());
    resp.setHeader("Connection",     "close");
    resp.setHeader("Content-Type",   contentType);
    resp.setHeader("Content-Length", std::to_string(body.size()));
    return resp;
}

// ── makeError ─────────────────────────────────────────────────────────────────
HttpResponse HttpResponse::makeError(int code, const std::string& msg) {
    const std::string text   = statusText(code);
    const std::string detail = msg.empty() ? text : msg;

    std::string body =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\"><head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>" + std::to_string(code) + " " + text + "</title>\n"
        "<style>\n"
        "  * { box-sizing:border-box; margin:0; padding:0; }\n"
        "  body { font-family:system-ui,-apple-system,sans-serif;\n"
        "         background:#f0f2f5; color:#333;\n"
        "         display:flex; align-items:center; justify-content:center;\n"
        "         min-height:100vh; padding:20px; }\n"
        "  .card { background:#fff; border-radius:12px;\n"
        "          box-shadow:0 4px 24px rgba(0,0,0,.08);\n"
        "          padding:48px 56px; text-align:center; max-width:480px; width:100%; }\n"
        "  .code { font-size:5rem; font-weight:800; color:#e74c3c; line-height:1; margin-bottom:.2rem; }\n"
        "  h1   { font-size:1.4rem; color:#555; margin-bottom:1rem; font-weight:600; }\n"
        "  p    { color:#888; line-height:1.7; margin-bottom:2rem; font-size:.95rem; }\n"
        "  a    { display:inline-block; padding:11px 28px; background:#3b82f6;\n"
        "         color:#fff; border-radius:8px; text-decoration:none; font-size:.9rem; }\n"
        "  a:hover { background:#2563eb; }\n"
        "</style></head><body>\n"
        "<div class=\"card\">\n"
        "  <div class=\"code\">" + std::to_string(code) + "</div>\n"
        "  <h1>" + text + "</h1>\n"
        "  <p>" + detail + "</p>\n"
        "  <a href=\"/\">&#8592; Back to Home</a>\n"
        "</div></body></html>\n";

    return make(code, body, "text/html; charset=utf-8");
}

// ── makeFile ──────────────────────────────────────────────────────────────────
HttpResponse HttpResponse::makeFile(const std::string& filePath) {
    // Open binary, seek to end to get size in one pass
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return makeError(403, "Cannot open file: " + filePath);

    const auto fileLen = file.tellg();
    if (fileLen < 0)
        return makeError(500, "Failed to determine file size");

    file.seekg(0, std::ios::beg);
    std::string content(static_cast<size_t>(fileLen), '\0');
    if (!file.read(content.data(), fileLen))
        return makeError(500, "Error reading file");

    return make(200, content, utils::mimeType(filePath));
}

// ── makeDirectoryListing ──────────────────────────────────────────────────────
HttpResponse HttpResponse::makeDirectoryListing(const std::string& dirPath,
                                                 const std::string& urlPath) {
    DIR* dp = opendir(dirPath.c_str());
    if (!dp) return makeError(403, "Cannot open directory");

    struct Entry {
        std::string name;
        bool        isDir;
        long        size;   // -1 for directories
        std::string mtime;
    };
    std::vector<Entry> entries;

    struct dirent* ent;
    while ((ent = readdir(dp)) != nullptr) {
        const std::string name = ent->d_name;
        if (name == ".") continue;   // skip self; keep parent ".."

        const std::string full = dirPath + "/" + name;
        struct stat st{};
        if (::stat(full.c_str(), &st) != 0) continue;

        const bool dir  = S_ISDIR(st.st_mode);
        const long sz   = dir ? -1L : static_cast<long>(st.st_size);
        char       tbuf[20];
        std::tm*   tm   = std::localtime(&st.st_mtime);
        std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", tm);
        entries.push_back({name, dir, sz, tbuf});
    }
    closedir(dp);

    // Sort: ".." always first; directories alpha; files alpha
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.name == "..") return true;
        if (b.name == "..") return false;
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name < b.name;
    });

    // ── Build HTML ────────────────────────────────────────────────────────
    const std::string title = "Index of " + urlPath;
    std::ostringstream html;
    html <<
        "<!DOCTYPE html>\n"
        "<html lang=\"en\"><head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>" << title << "</title>\n"
        "<style>\n"
        "  * { box-sizing:border-box; margin:0; padding:0; }\n"
        "  body  { font-family:system-ui,-apple-system,sans-serif;\n"
        "          background:#f0f2f5; color:#333; padding:32px 16px; }\n"
        "  .wrap { max-width:960px; margin:0 auto; background:#fff;\n"
        "          border-radius:12px; box-shadow:0 2px 20px rgba(0,0,0,.08); overflow:hidden; }\n"
        "  header{ padding:20px 28px; background:#1e293b; color:#e2e8f0; }\n"
        "  header h1 { font-size:1.1rem; font-weight:600; }\n"
        "  header p  { font-size:.78rem; color:#94a3b8; margin-top:3px; }\n"
        "  table { border-collapse:collapse; width:100%; }\n"
        "  thead th { padding:10px 16px; background:#f8fafc; text-align:left;\n"
        "             font-size:.75rem; text-transform:uppercase; letter-spacing:.06em;\n"
        "             color:#64748b; border-bottom:1px solid #e2e8f0; }\n"
        "  td   { padding:10px 16px; border-bottom:1px solid #f1f5f9; font-size:.9rem; }\n"
        "  tr:last-child td { border-bottom:none; }\n"
        "  tbody tr:hover td { background:#f8fafc; }\n"
        "  a    { text-decoration:none; color:#2563eb; }\n"
        "  a:hover { text-decoration:underline; }\n"
        "  .sz  { color:#94a3b8; text-align:right; font-variant-numeric:tabular-nums; }\n"
        "  .ts  { color:#94a3b8; white-space:nowrap; font-size:.82rem; }\n"
        "  footer{ padding:10px 28px; background:#f8fafc; border-top:1px solid #e2e8f0;\n"
        "          font-size:.75rem; color:#94a3b8; }\n"
        "</style></head><body>\n"
        "<div class=\"wrap\">\n"
        "<header><h1>&#128194; " << title << "</h1>\n"
        "<p>http-server/1.0</p></header>\n"
        "<table><thead><tr>"
        "<th>Name</th><th>Last Modified</th><th class=\"sz\">Size</th>"
        "</tr></thead><tbody>\n";

    for (const auto& e : entries) {
        // Build href: strip trailing slash from base, then re-add name
        std::string base = urlPath;
        if (!base.empty() && base.back() == '/') base.pop_back();
        const std::string href = base + "/" + e.name + (e.isDir ? "/" : "");

        const char* icon = e.isDir ? "&#128193;" : "&#128196;";

        // Human-readable file size
        std::string sizeStr;
        if (e.isDir) {
            sizeStr = "&mdash;";
        } else if (e.size < 1024L) {
            sizeStr = std::to_string(e.size) + "&nbsp;B";
        } else if (e.size < 1024L * 1024L) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1)
               << (e.size / 1024.0) << "&nbsp;KB";
            sizeStr = ss.str();
        } else if (e.size < 1024L * 1024L * 1024L) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1)
               << (e.size / (1024.0 * 1024.0)) << "&nbsp;MB";
            sizeStr = ss.str();
        } else {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2)
               << (e.size / (1024.0 * 1024.0 * 1024.0)) << "&nbsp;GB";
            sizeStr = ss.str();
        }

        html << "<tr>"
             << "<td>" << icon << " <a href=\"" << href << "\">" << e.name << "</a></td>"
             << "<td class=\"ts\">" << e.mtime << "</td>"
             << "<td class=\"sz\">" << sizeStr << "</td>"
             << "</tr>\n";
    }

    html << "</tbody></table>\n"
         << "<footer>" << entries.size() << " item(s)</footer>\n"
         << "</div></body></html>\n";

    return make(200, html.str(), "text/html; charset=utf-8");
}

// ── serialize ─────────────────────────────────────────────────────────────────
std::string HttpResponse::serialize() const {
    std::ostringstream out;
    out << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    for (const auto& [k, v] : headers)
        out << k << ": " << v << "\r\n";
    out << "\r\n" << body;
    return out.str();
}
