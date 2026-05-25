# http-server

> A production-quality, multi-threaded HTTP/1.1 server written from scratch in **C++17**.  
> Raw POSIX sockets · hand-rolled thread pool · zero external dependencies.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## Features

| Feature | Detail |
|---|---|
| **Thread pool** | Fixed-size worker pool; configurable via `-t` |
| **Static file serving** | Any file under the document root, correct MIME types |
| **MIME detection** | 35+ extensions: HTML, CSS, JS, JSON, images, fonts, video, audio, archives, source code |
| **Directory listing** | Auto-generated HTML table (sorted: dirs first, then files) |
| **Index file** | Serves `index.html` (or custom) instead of listing when present |
| **Directory redirect** | `/foo` → `301 /foo/` so relative links resolve correctly |
| **HEAD support** | Full headers, zero body bytes |
| **Graceful shutdown** | `SIGINT`/`SIGTERM` drain the pool and close the socket cleanly |
| **Slow-client timeout** | 30 s `SO_RCVTIMEO` prevents thread starvation |
| **URL decoding** | `%XX` and `+`-as-space in request paths |
| **Path sanitization** | `../`-traversal and `./` segments are removed before mapping to the filesystem |
| **Colour access log** | Timestamped, ANSI-coloured log lines to `stdout` |

---

## Requirements

- Linux / macOS (any POSIX system with `dirent.h`)
- GCC ≥ 9 or Clang ≥ 10 with C++17 support
- `make` or `cmake` ≥ 3.14
- No external libraries required

---

## Build

### With Make (recommended)

```bash
git clone https://github.com/rraghu08-covin/http-server
cd http-server
make          # release build  → ./http-server
make debug    # ASan + UBSan   → ./http-server
make clean    # remove build/
```

### With CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## Run

```
./http-server [options]

Options:
  -p, --port     PORT   Listening port          (default: 8080)
  -H, --host     HOST   Bind address            (default: 0.0.0.0)
  -d, --docroot  DIR    Document root directory (default: ./www)
  -t, --threads  N      Thread pool size        (default: 4)
  -i, --index    FILE   Directory index file    (default: index.html)
  -n, --no-listing      Disable directory listing
  -q, --quiet           Suppress all log output
  -h, --help            Show this help message
```

### Examples

```bash
# Serve the bundled demo page on port 8080
./http-server

# Custom port, document root, thread count
./http-server -p 3000 -d /var/www/html -t 8

# Localhost only, disable directory listing, quiet
./http-server -p 9000 -H 127.0.0.1 -n -q

# Custom index file
./http-server -i home.html
```

---

## Terminal Demo

```
╔══════════════════════════════════════╗
║       http-server / 1.0  ready       ║
╚══════════════════════════════════════╝

[INFO]  Address  http://0.0.0.0:8080
[INFO]  DocRoot  ./www
[INFO]  Threads  4 workers
[INFO]  Listing  enabled
[INFO]  Press Ctrl-C to stop

[INFO]  127.0.0.1  GET  /               200 OK
[INFO]  127.0.0.1  GET  /style.css      200 OK
[INFO]  127.0.0.1  GET  /favicon.ico    404 Not Found
```

---

## Project Structure

```
http-server/
├── include/
│   ├── utils.h           # MIME, URL decode, path sanitize, log
│   ├── request.h         # HttpRequest struct + parse()
│   ├── response.h        # HttpResponse struct + factories
│   ├── thread_pool.h     # Fixed-size ThreadPool
│   ├── server.h          # ServerConfig + HttpServer
│   └── connection.h      # handleConnection() declaration
├── src/
│   ├── utils.cpp         # MIME table, URL decode, sanitizePath, log
│   ├── request.cpp       # HTTP/1.1 request parser
│   ├── response.cpp      # Response factories + serializer
│   ├── thread_pool.cpp   # Producer-consumer thread pool
│   ├── server.cpp        # Socket lifecycle + accept loop
│   ├── connection.cpp    # Request routing + access log
│   └── main.cpp          # getopt_long CLI entry point
├── www/
│   ├── index.html        # Dark-themed demo landing page
│   └── style.css         # Supplemental stylesheet
├── Makefile
├── CMakeLists.txt
└── .gitignore
```

---

## Architecture

```
main()                   parse CLI flags → HttpServer::start()
  └─ HttpServer
       ├─ createSocket()  bind + listen on TCP port
       ├─ sigaction()     SIGINT / SIGTERM → graceful stop
       ├─ ThreadPool      N worker threads waiting on condition_variable
       └─ acceptLoop()    accept() → enqueue λ → handleConnection(fd)

handleConnection(fd)
  ├─ readRequest()        recv loop with 30s SO_RCVTIMEO
  ├─ HttpRequest::parse() split headers / body; decode URL
  ├─ route:
  │    isDirectory → index file? → listing? → redirect
  │    fileExists  → makeFile()
  │    else        → makeError(404)
  ├─ HEAD → clear body, keep Content-Length
  ├─ access log
  └─ sendAll() + close(fd)
```

---

## Key Concepts Demonstrated

- **POSIX TCP sockets** — `socket`, `bind`, `listen`, `accept`, `recv`, `send`, `close`
- **`SO_REUSEADDR`** — immediate port reuse after server restart
- **`SO_RCVTIMEO`** — socket-level receive timeout to protect worker threads
- **`MSG_NOSIGNAL`** — suppress `SIGPIPE` on broken connections
- **`sigaction`** — async-signal-safe shutdown sequence
- **C++17 structured bindings** — `for (const auto& [k, v] : headers)`
- **`std::atomic<bool>`** — lock-free running flag shared between threads
- **RAII** — `unique_ptr<ThreadPool>`, file handles, socket descriptors
- **HTTP/1.1** — request parsing, status codes, `Content-Length`, `Connection: close`

---

## License

MIT © 2024 rraghu08-covin
