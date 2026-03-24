# concurrent-http-proxy-server

A C systems project that implements a concurrent HTTP proxy server with a shared thread-safe LRU cache.

## What It Does

- accepts plain HTTP proxy requests
- parses absolute-form `GET` requests
- forwards misses to origin servers
- caches eligible origin responses
- serves repeated requests from a shared in-memory cache
- handles multiple clients through a bounded worker thread pool
- tracks basic operational metrics and logs request lifecycle events

## Why This Project Exists

The repository is designed to be easy to understand and maintain:

- modular C code with clear boundaries
- strict compiler flags
- incremental PR-sized history
- unit tests for core modules
- concise architecture and design notes

## Build

```cmd
mingw32-make
```

## Run Tests

```cmd
mingw32-make test
```

## Run Locally

Start a local origin server:

```cmd
python -m http.server 18080
```

Start the proxy:

```cmd
proxy_server.exe --port 9093 --workers 2 --cache-size 1048576 --log-level info
```

Send a request through the proxy:

```cmd
curl -v --proxy http://127.0.0.1:9093 http://127.0.0.1:18080/
```

Repeat the same request to exercise the cache.

For a scripted local walkthrough or benchmark, see [`docs/benchmarks.md`](docs/benchmarks.md).

## Repository Layout

```text
.
├── include/
├── src/
├── tests/
├── docs/
├── scripts/
├── Makefile
└── README.md
```

## Key Modules

- `request_parser.*`: parses proxy-style HTTP requests
- `socket_utils.*`: listener, connect, timeout, and socket I/O helpers
- `thread_pool.*`: bounded worker pool and synchronized queue
- `response_forwarder.*`: origin request rewrite and response relay
- `cache.*` and `lru.*`: shared cache storage and eviction ordering
- `metrics.*`: thread-safe operational counters and summary logging
- `proxy_server.*`: request lifecycle orchestration

## Current Scope

Implemented:

- plain HTTP proxying
- bounded concurrency
- shared in-memory caching for eligible responses
- metrics summary logging
- unit tests for parser, LRU, cache, and metrics
- local demo and benchmark scripts for reproducible performance checks

Not implemented yet:

- HTTPS CONNECT tunneling
- advanced cache policy controls

## Docs

- [Architecture](docs/architecture.md)
- [Design Decisions](docs/design-decisions.md)
- [Benchmarks](docs/benchmarks.md)
- [Test Notes](tests/test_notes.md)
