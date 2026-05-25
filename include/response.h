#pragma once
#include <string>
#include <map>

// ── HttpResponse ──────────────────────────────────────────────────────────────
struct HttpResponse {
    int         statusCode = 200;
    std::string statusText = "OK";
    std::map<std::string, std::string> headers;
    std::string body;

    // Generic response with standard headers (Date, Server, Connection,
    // Content-Type, Content-Length).
    static HttpResponse make(int code, const std::string& body = "",
                             const std::string& contentType = "text/plain");

    // Styled HTML error page.
    static HttpResponse makeError(int code, const std::string& msg = "");

    // Read a file from disk; detect MIME type; return 200 response.
    static HttpResponse makeFile(const std::string& filePath);

    // HTML table directory listing; sorts dirs first, then files.
    static HttpResponse makeDirectoryListing(const std::string& dirPath,
                                              const std::string& urlPath);

    // Serialise to HTTP/1.1 wire format.
    std::string serialize() const;

    // Add or overwrite a response header.
    void setHeader(const std::string& name, const std::string& value);
};
