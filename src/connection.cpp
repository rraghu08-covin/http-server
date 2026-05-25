#include "connection.h"
#include "request.h"
#include "response.h"
#include "utils.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <string>
#include <algorithm>

// ── ANSI colour helpers ──────────────────────────────────────────────────────
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define CYAN    "\033[36m"

// ── Network helpers ───────────────────────────────────────────────────────────
static bool sendAll(int fd, const std::string& data) {
    const char* buf       = data.data();
    size_t      remaining = data.size();
    while (remaining > 0) {
        const ssize_t sent = ::send(fd, buf, remaining, MSG_NOSIGNAL);
        if (sent <= 0) return false;
        buf       += sent;
        remaining -= static_cast<size_t>(sent);
    }
    return true;
}

// ── Request reader ────────────────────────────────────────────────────────────
static std::string readRequest(int fd) {
    // 30-second receive timeout prevents slow-client thread starvation.
    struct timeval tv{ 30, 0 };
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string buf;
    buf.reserve(8192);
    char   tmp[8192];
    static constexpr size_t MAX_REQUEST = 8UL * 1024 * 1024;  // 8 MB cap

    bool   headersComplete = false;
    size_t bodyNeeded      = 0;
    size_t headerEnd       = 0;

    while (buf.size() < MAX_REQUEST) {
        const ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) break;   // connection closed or timeout
        buf.append(tmp, static_cast<size_t>(n));

        if (!headersComplete) {
            const auto sep = buf.find("\r\n\r\n");
            if (sep != std::string::npos) {
                headersComplete = true;
                headerEnd       = sep + 4;

                // Extract Content-Length (case-insensitive search)
                const std::string lower = utils::toLower(buf.substr(0, sep));
                const auto clPos = lower.find("content-length:");
                if (clPos != std::string::npos) {
                    const auto lineEnd = lower.find('\n', clPos);
                    const std::string clStr =
                        utils::trim(lower.substr(clPos + 15,
                                    lineEnd == std::string::npos
                                        ? std::string::npos
                                        : lineEnd - (clPos + 15)));
                    try {
                        bodyNeeded = std::min(
                            static_cast<size_t>(std::stoul(clStr)),
                            MAX_REQUEST);
                    } catch (...) { bodyNeeded = 0; }
                }
            }
        }

        if (headersComplete) {
            const size_t have = buf.size() > headerEnd ? buf.size() - headerEnd : 0;
            if (have >= bodyNeeded) break;
        }
    }
    return buf;
}

// ── Status-code colour ────────────────────────────────────────────────────────
static const char* codeColor(int code) {
    if (code >= 500) return RED;
    if (code >= 400) return YELLOW;
    if (code >= 300) return CYAN;
    return GREEN;
}

// ── Connection handler ────────────────────────────────────────────────────────
void handleConnection(int fd, const std::string& clientIp,
                      const ServerConfig& config) {
    const std::string raw = readRequest(fd);
    const HttpRequest req = HttpRequest::parse(raw);
    HttpResponse      resp;

    // ── Validate request ──────────────────────────────────────────────────
    if (!req.valid) {
        resp = HttpResponse::makeError(400, req.errorMsg);
    } else if (req.method != HttpMethod::GET  &&
               req.method != HttpMethod::HEAD &&
               req.method != HttpMethod::POST) {
        resp = HttpResponse::makeError(405, "Only GET, HEAD and POST are supported");
        resp.setHeader("Allow", "GET, HEAD, POST");
    } else {
        // ── Route to file or directory ────────────────────────────────────
        const std::string fsPath = config.docRoot + req.path;

        if (utils::isDirectory(fsPath)) {
            // Strip trailing slash from fs path for index-file lookup
            std::string fsBase = fsPath;
            while (fsBase.size() > 1 && fsBase.back() == '/')
                fsBase.pop_back();

            const std::string indexPath   = fsBase + "/" + config.indexFile;
            const bool        hasSlash    = !req.path.empty() && req.path.back() == '/';

            if (utils::fileExists(indexPath)) {
                resp = HttpResponse::makeFile(indexPath);
            } else if (!hasSlash) {
                // Redirect /foo -> /foo/ (canonical directory URL)
                resp = HttpResponse::make(301, "", "text/plain");
                resp.setHeader("Location", req.path + "/");
            } else if (config.enableDirListing) {
                resp = HttpResponse::makeDirectoryListing(fsBase, req.path);
            } else {
                resp = HttpResponse::makeError(403, "Directory listing is disabled");
            }
        } else if (utils::fileExists(fsPath)) {
            resp = HttpResponse::makeFile(fsPath);
        } else {
            resp = HttpResponse::makeError(404,
                "The requested resource was not found on this server");
        }
    }

    // ── HEAD: strip body, keep Content-Length ─────────────────────────────
    if (req.valid && req.method == HttpMethod::HEAD)
        resp.body.clear();

    // ── Access log ────────────────────────────────────────────────────────
    if (config.enableLogging) {
        const char* cc = codeColor(resp.statusCode);
        std::string logMsg;
        logMsg += std::string(CYAN);
        logMsg += clientIp;
        logMsg += RESET "  " BOLD;
        logMsg += req.methodStr.empty() ? "-" : req.methodStr;
        logMsg += RESET "  ";
        logMsg += req.path.empty()      ? "/" : req.path;
        logMsg += "  ";
        logMsg += cc;
        logMsg += std::to_string(resp.statusCode);
        logMsg += " ";
        logMsg += resp.statusText;
        logMsg += RESET;
        utils::log("INFO", logMsg);
    }

    // ── Send response and close ────────────────────────────────────────────
    sendAll(fd, resp.serialize());
    ::close(fd);
}
