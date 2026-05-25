#pragma once
#include <string>
#include <vector>

// ── utils namespace ──────────────────────────────────────────────────────────
// Utility functions shared across all server modules.

namespace utils {

    // MIME type detection by file extension
    // Returns "application/octet-stream" for unknown extensions.
    std::string mimeType(const std::string& path);

    // Percent-decode a URL string (%XX → char, + → space).
    std::string urlDecode(const std::string& str);

    // Strip leading/trailing whitespace (space, tab, CR, LF).
    std::string trim(const std::string& str);

    // Convert string to ASCII lowercase.
    std::string toLower(const std::string& str);

    // RFC 7231 HTTP date string: "Tue, 15 Nov 1994 08:12:31 GMT"
    std::string httpDate();

    // POSIX filesystem helpers
    bool        fileExists  (const std::string& path);
    bool        isDirectory (const std::string& path);
    long        fileSize    (const std::string& path);

    // Resolve . and .. path segments; prevent directory traversal.
    // The returned path always begins with /.
    std::string sanitizePath(const std::string& path);

    // Timestamped, coloured log line written to stdout.
    // level: "INFO" | "WARN" | "ERROR"
    void log(const std::string& level, const std::string& msg);

} // namespace utils
