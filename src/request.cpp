#include "request.h"
#include "utils.h"

#include <sstream>

// ── Method string → enum ──────────────────────────────────────────────────────
static HttpMethod toMethod(const std::string& m) {
    if (m == "GET")     return HttpMethod::GET;
    if (m == "POST")    return HttpMethod::POST;
    if (m == "HEAD")    return HttpMethod::HEAD;
    if (m == "PUT")     return HttpMethod::PUT;
    if (m == "DELETE")  return HttpMethod::DELETE;
    if (m == "OPTIONS") return HttpMethod::OPTIONS;
    return HttpMethod::UNKNOWN;
}

// ── HttpRequest::parse ────────────────────────────────────────────────────────
HttpRequest HttpRequest::parse(const std::string& raw) {
    HttpRequest req;

    // ── Locate header / body boundary ─────────────────────────────────────
    static const std::string SEP = "\r\n\r\n";
    const auto bodyPos = raw.find(SEP);
    if (bodyPos == std::string::npos) {
        req.errorMsg = "missing header/body separator (\\r\\n\\r\\n)";
        return req;
    }

    const std::string headerSection = raw.substr(0, bodyPos);
    req.body = raw.substr(bodyPos + SEP.size());

    // ── Request line ──────────────────────────────────────────────────────
    std::istringstream ss(headerSection);
    std::string requestLine;
    if (!std::getline(ss, requestLine) || requestLine.empty()) {
        req.errorMsg = "empty request line";
        return req;
    }
    if (!requestLine.empty() && requestLine.back() == '\r')
        requestLine.pop_back();

    std::istringstream rl(requestLine);
    std::string target;
    if (!(rl >> req.methodStr >> target >> req.version)) {
        req.errorMsg = "malformed request line: " + requestLine;
        return req;
    }
    req.method = toMethod(req.methodStr);

    // ── Decode and split target into path + query ─────────────────────────
    const auto qPos = target.find('?');
    if (qPos != std::string::npos) {
        req.path  = utils::sanitizePath(utils::urlDecode(target.substr(0, qPos)));
        req.query = target.substr(qPos + 1);   // keep query raw; decode per-param
    } else {
        req.path = utils::sanitizePath(utils::urlDecode(target));
    }

    // ── Header lines ──────────────────────────────────────────────────────
    std::string headerLine;
    while (std::getline(ss, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r')
            headerLine.pop_back();
        if (headerLine.empty()) break;

        const auto colon = headerLine.find(':');
        if (colon != std::string::npos) {
            const std::string key   = utils::toLower(utils::trim(headerLine.substr(0, colon)));
            const std::string value = utils::trim(headerLine.substr(colon + 1));
            req.headers[key] = value;
        }
    }

    req.valid = true;
    return req;
}

// ── HttpRequest::getHeader ────────────────────────────────────────────────────
std::string HttpRequest::getHeader(const std::string& name) const {
    const auto it = headers.find(utils::toLower(name));
    return it != headers.end() ? it->second : std::string{};
}
