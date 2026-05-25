#pragma once
#include "server.h"
#include <string>

// ── handleConnection ──────────────────────────────────────────────────────────
// Runs inside a thread-pool worker.
// Reads, parses and routes one HTTP request; sends the full response;
// closes the file descriptor when done.
void handleConnection(int fd, const std::string& clientIp,
                      const ServerConfig& config);
