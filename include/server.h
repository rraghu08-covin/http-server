#pragma once
#include "thread_pool.h"
#include <string>
#include <memory>
#include <atomic>

// ── ServerConfig ──────────────────────────────────────────────────────────────
// All runtime options; populated by main() via getopt_long.
struct ServerConfig {
    std::string host             = "0.0.0.0";
    int         port             = 8080;
    std::string docRoot          = "./www";
    size_t      numThreads       = 4;
    int         backlog          = 128;
    bool        enableDirListing = true;
    bool        enableLogging    = true;
    std::string indexFile        = "index.html";
};

// ── HttpServer ────────────────────────────────────────────────────────────────
class HttpServer {
public:
    explicit HttpServer(const ServerConfig& config);
    ~HttpServer();

    // Bind socket, install signal handlers, start accept loop (blocks).
    void start();

    // Signal the accept loop to exit; clean up resources.
    // Idempotent — safe to call more than once.
    void stop();

    // SIGINT / SIGTERM handler — calls instance->stop().
    static void signalHandler(int signum);

    // Pointer to the active server instance (for signal-handler access).
    static HttpServer* instance;

private:
    void acceptLoop();
    int  createSocket();

    ServerConfig                config_;
    int                         serverFd_ = -1;
    std::unique_ptr<ThreadPool> pool_;
    std::atomic<bool>           running_{false};
};
