#include "utils.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <cstring>
#include <unordered_map>
#include <vector>

#include <sys/stat.h>

// ── ANSI colour helpers ──────────────────────────────────────────────────────
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"

namespace utils {

// ── MIME type table ───────────────────────────────────────────────────────────
std::string mimeType(const std::string& path) {
    static const std::unordered_map<std::string, std::string> table = {
        // ── Web ───────────────────────────────────────────────────────────────
        { ".html",  "text/html; charset=utf-8"        },
        { ".htm",   "text/html; charset=utf-8"        },
        { ".css",   "text/css; charset=utf-8"         },
        { ".js",    "application/javascript"          },
        { ".mjs",   "application/javascript"          },
        { ".json",  "application/json"                },
        { ".xml",   "application/xml"                 },
        { ".txt",   "text/plain; charset=utf-8"       },
        // ── Images ────────────────────────────────────────────────────────────
        { ".png",   "image/png"                       },
        { ".jpg",   "image/jpeg"                      },
        { ".jpeg",  "image/jpeg"                      },
        { ".gif",   "image/gif"                       },
        { ".ico",   "image/x-icon"                    },
        { ".svg",   "image/svg+xml"                   },
        { ".webp",  "image/webp"                      },
        // ── Fonts ─────────────────────────────────────────────────────────────
        { ".woff",  "font/woff"                       },
        { ".woff2", "font/woff2"                      },
        { ".ttf",   "font/ttf"                        },
        // ── Media ─────────────────────────────────────────────────────────────
        { ".mp4",   "video/mp4"                       },
        { ".webm",  "video/webm"                      },
        { ".mp3",   "audio/mpeg"                      },
        { ".wav",   "audio/wav"                       },
        { ".ogg",   "audio/ogg"                       },
        // ── Documents / Archives ──────────────────────────────────────────────
        { ".pdf",   "application/pdf"                 },
        { ".zip",   "application/zip"                 },
        { ".gz",    "application/gzip"                },
        { ".tar",   "application/x-tar"               },
        // ── Source code (served as plain text) ────────────────────────────────
        { ".c",     "text/plain; charset=utf-8"       },
        { ".cpp",   "text/plain; charset=utf-8"       },
        { ".cc",    "text/plain; charset=utf-8"       },
        { ".h",     "text/plain; charset=utf-8"       },
        { ".hpp",   "text/plain; charset=utf-8"       },
        { ".py",    "text/plain; charset=utf-8"       },
        { ".sh",    "text/plain; charset=utf-8"       },
        { ".java",  "text/plain; charset=utf-8"       },
        { ".go",    "text/plain; charset=utf-8"       },
        { ".rs",    "text/plain; charset=utf-8"       },
        { ".md",    "text/markdown; charset=utf-8"    },
    };

    const auto dot = path.rfind('.');
    if (dot == std::string::npos) return "application/octet-stream";
    const std::string ext = toLower(path.substr(dot));
    const auto it = table.find(ext);
    return it != table.end() ? it->second : "application/octet-stream";
}

// ── URL decoding ──────────────────────────────────────────────────────────────
std::string urlDecode(const std::string& str) {
    std::string out;
    out.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '+') {
            out += ' ';
        } else if (str[i] == '%' && i + 2 < str.size()) {
            const char hex[3] = { str[i + 1], str[i + 2], '\0' };
            char* endPtr = nullptr;
            const long val = std::strtol(hex, &endPtr, 16);
            if (endPtr == hex + 2) {
                out += static_cast<char>(val);
                i += 2;
            } else {
                out += str[i];   // invalid escape — keep literal '%'
            }
        } else {
            out += str[i];
        }
    }
    return out;
}

// ── String utilities ──────────────────────────────────────────────────────────
std::string trim(const std::string& str) {
    static constexpr const char* ws = " \t\r\n";
    const auto start = str.find_first_not_of(ws);
    if (start == std::string::npos) return {};
    const auto end = str.find_last_not_of(ws);
    return str.substr(start, end - start + 1);
}

std::string toLower(const std::string& str) {
    std::string out;
    out.reserve(str.size());
    for (unsigned char c : str)
        out += static_cast<char>(std::tolower(c));
    return out;
}

// ── HTTP date ─────────────────────────────────────────────────────────────────
std::string httpDate() {
    const std::time_t now = std::time(nullptr);
    const std::tm* gmt    = std::gmtime(&now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return std::string(buf);
}

// ── Filesystem helpers ────────────────────────────────────────────────────────
bool fileExists(const std::string& path) {
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool isDirectory(const std::string& path) {
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

long fileSize(const std::string& path) {
    struct stat st{};
    return (::stat(path.c_str(), &st) == 0) ? static_cast<long>(st.st_size) : -1L;
}

// ── Path sanitization ─────────────────────────────────────────────────────────
std::string sanitizePath(const std::string& path) {
    // Preserve a trailing slash to distinguish "directory" URLs from "file" URLs,
    // but only when the path is more than just "/".
    const bool trailingSlash = path.size() > 1 && path.back() == '/';

    std::vector<std::string> parts;
    std::istringstream ss(path);
    std::string seg;
    while (std::getline(ss, seg, '/')) {
        if (seg.empty() || seg == ".") continue;
        if (seg == "..") {
            if (!parts.empty()) parts.pop_back();
            // Extra ".." at root are silently discarded — traversal prevented.
        } else {
            parts.push_back(seg);
        }
    }

    std::string result = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += '/';
        result += parts[i];
    }
    if (trailingSlash && result != "/") result += '/';
    return result;
}

// ── Logging ───────────────────────────────────────────────────────────────────
void log(const std::string& level, const std::string& msg) {
    const std::time_t now = std::time(nullptr);
    const std::tm* lt     = std::localtime(&now);
    char ts[24];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", lt);

    const char* color = "";
    if      (level == "INFO")  color = GREEN;
    else if (level == "WARN")  color = YELLOW;
    else if (level == "ERROR") color = RED;

    std::cout << DIM "[" << ts << "] " RESET
              << color << BOLD "[" << level << "]" RESET " "
              << msg << "\n" << std::flush;
}

} // namespace utils
