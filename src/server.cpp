#include "server.h"
#include "connection.h"
#include "utils.h"

#include <iostream>
#include <stdexcept>
#include <cstring>

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>

// ── ANSI colour helpers ──────────────────────────────────────────────────────
#define RESET  "\033[0m"
#define BOLD   "\033[1m"
#define CYAN   "\033[36m"

// ── Static member ─────────────────────────────────────────────────────────────
HttpServer* HttpServer::instance = nullptr;

// ── Constructor / Destructor ──────────────────────────────────────────────────
HttpServer::HttpServer(const ServerConfig& config)
    : config_(config)
    , pool_(std::make_unique<ThreadPool>(config.numThreads))
{
    instance = this;
    // Normalise docRoot: remove trailing slashes
    while (config_.docRoot.size() > 1 && config_.docRoot.back() == '/')
        config_.docRoot.pop_back();
}

HttpServer::~HttpServer() {
    stop();
}

// ── Socket creation ───────────────────────────────────────────────────────────
int HttpServer::createSocket() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error(std::string("socket() failed: ") + strerror(errno));

    // Allow immediate port reuse after restart
    const int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(config_.port));
    if (::inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0)
        addr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("bind() failed: ") + strerror(errno));
    }
    if (::listen(fd, config_.backlog) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("listen() failed: ") + strerror(errno));
    }
    return fd;
}

// ── Signal handler ────────────────────────────────────────────────────────────
void HttpServer::signalHandler(int /*signum*/) {
    if (!instance) return;
    // Set the flag and interrupt accept() — the rest of cleanup happens
    // in start() after acceptLoop() returns.
    instance->running_.store(false, std::memory_order_relaxed);
    if (instance->serverFd_ >= 0)
        ::shutdown(instance->serverFd_, SHUT_RDWR);
}

// ── Accept loop ───────────────────────────────────────────────────────────────
void HttpServer::acceptLoop() {
    while (running_.load()) {
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        const int   clientFd  = ::accept(serverFd_,
                                          reinterpret_cast<sockaddr*>(&clientAddr),
                                          &clientLen);
        if (clientFd < 0) {
            if (!running_.load()) break;        // graceful shutdown
            if (errno == EINTR)   continue;      // interrupted by signal
            if (config_.enableLogging)
                utils::log("WARN", std::string("accept() failed: ") + strerror(errno));
            continue;
        }

        // Reject connections that arrive in the shutdown race window
        if (!running_.load()) { ::close(clientFd); break; }

        char ipBuf[INET_ADDRSTRLEN] = "unknown";
        ::inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));
        const std::string clientIp(ipBuf);

        pool_->enqueue([clientFd, clientIp, this]() {
            handleConnection(clientFd, clientIp, config_);
        });
    }
}

// ── start ─────────────────────────────────────────────────────────────────────
void HttpServer::start() {
    serverFd_ = createSocket();
    running_  = true;

    // Install signal handlers
    struct sigaction sa{};
    sa.sa_handler = HttpServer::signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ::sigaction(SIGINT,  &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
    ::signal(SIGPIPE, SIG_IGN);  // handle broken pipe via send() return value

    if (config_.enableLogging) {
        std::cout
            << BOLD CYAN
            << "\n  \u250c\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2510\n"
            << "  \u2551       http-server / 1.0  ready       \u2551\n"
            << "  \u2514\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2518\n"
            << RESET "\n";
        utils::log("INFO", std::string(BOLD) + "Address  " + RESET
                         + "http://" + config_.host + ":" + std::to_string(config_.port));
        utils::log("INFO", std::string(BOLD) + "DocRoot  " + RESET + config_.docRoot);
        utils::log("INFO", std::string(BOLD) + "Threads  " + RESET
                         + std::to_string(config_.numThreads) + " workers");
        utils::log("INFO", std::string(BOLD) + "Listing  " + RESET
                         + std::string(config_.enableDirListing ? "enabled" : "disabled"));
        utils::log("INFO", "Press Ctrl-C to stop\n");
    }

    acceptLoop();   // blocks until stop() is called

    // ── Post-loop cleanup ─────────────────────────────────────────────────
    if (serverFd_ >= 0) {
        ::close(serverFd_);
        serverFd_ = -1;
    }
    pool_->shutdown();

    if (config_.enableLogging)
        utils::log("INFO", "Server stopped.");
}

// ── stop ──────────────────────────────────────────────────────────────────────
void HttpServer::stop() {
    if (!running_.exchange(false)) return;  // idempotent
    if (serverFd_ >= 0)
        ::shutdown(serverFd_, SHUT_RDWR);   // unblock accept()
}
