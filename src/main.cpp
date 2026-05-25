#include "server.h"
#include "utils.h"

#include <iostream>
#include <stdexcept>
#include <cstring>

#include <getopt.h>

// ── ANSI colour helpers ──────────────────────────────────────────────────────
#define RESET  "\033[0m"
#define BOLD   "\033[1m"
#define CYAN   "\033[36m"

// ── Help text ────────────────────────────────────────────────────────────────
static void printHelp(const char* prog) {
    std::cout
        << BOLD CYAN "http-server/1.0" RESET
        << " — A multi-threaded HTTP/1.1 server in C++17\n\n"
        << BOLD "Usage:\n" RESET
        << "  " << prog << " [options]\n\n"
        << BOLD "Options:\n" RESET
        << "  " BOLD "-p, --port" RESET "    PORT   Listening port          (default: 8080)\n"
        << "  " BOLD "-H, --host" RESET "    HOST   Bind address            (default: 0.0.0.0)\n"
        << "  " BOLD "-d, --docroot" RESET " DIR    Document root directory (default: ./www)\n"
        << "  " BOLD "-t, --threads" RESET " N      Thread pool size        (default: 4)\n"
        << "  " BOLD "-i, --index" RESET "   FILE   Directory index file    (default: index.html)\n"
        << "  " BOLD "-n, --no-listing" RESET "     Disable directory listing\n"
        << "  " BOLD "-q, --quiet" RESET "          Suppress all log output\n"
        << "  " BOLD "-h, --help" RESET "           Show this help message\n\n"
        << BOLD "Examples:\n" RESET
        << "  " << prog << "\n"
        << "  " << prog << " -p 3000 -d /var/www/html\n"
        << "  " << prog << " -p 8888 -H 127.0.0.1 -t 8 -n\n"
        << "  " << prog << " -p 9000 -d ./public -i home.html -q\n";
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    ServerConfig config;

    static const struct option longOpts[] = {
        { "port",       required_argument, nullptr, 'p' },
        { "host",       required_argument, nullptr, 'H' },
        { "docroot",    required_argument, nullptr, 'd' },
        { "threads",    required_argument, nullptr, 't' },
        { "index",      required_argument, nullptr, 'i' },
        { "no-listing", no_argument,       nullptr, 'n' },
        { "quiet",      no_argument,       nullptr, 'q' },
        { "help",       no_argument,       nullptr, 'h' },
        { nullptr,      0,                 nullptr,  0  }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "p:H:d:t:i:nqh", longOpts, nullptr)) != -1) {
        switch (opt) {
            case 'p': {
                try {
                    const int port = std::stoi(optarg);
                    if (port < 1 || port > 65535) throw std::out_of_range("");
                    config.port = port;
                } catch (...) {
                    std::cerr << "Error: invalid port '" << optarg
                              << "' (must be 1-65535)\n";
                    return 1;
                }
                break;
            }
            case 'H': config.host    = optarg; break;
            case 'd': config.docRoot = optarg; break;
            case 't': {
                try {
                    const int t = std::stoi(optarg);
                    if (t < 1) throw std::out_of_range("");
                    config.numThreads = static_cast<size_t>(t);
                } catch (...) {
                    std::cerr << "Error: invalid thread count '" << optarg
                              << "' (must be >= 1)\n";
                    return 1;
                }
                break;
            }
            case 'i': config.indexFile        = optarg; break;
            case 'n': config.enableDirListing = false;  break;
            case 'q': config.enableLogging    = false;  break;
            case 'h': printHelp(argv[0]);                return 0;
            default:  printHelp(argv[0]);                return 1;
        }
    }

    try {
        HttpServer server(config);
        server.start();
    } catch (const std::exception& e) {
        utils::log("ERROR", e.what());
        return 1;
    }
    return 0;
}
