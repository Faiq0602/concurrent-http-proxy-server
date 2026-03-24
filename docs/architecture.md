# Architecture

## System Overview

The proxy has five main runtime stages:

1. startup and configuration
2. listener accept loop
3. worker-thread request handling
4. cache lookup or origin forwarding
5. metrics and lifecycle logging

The implementation stays intentionally modular so each stage is visible in a small number of files.

## Request Lifecycle

1. `main.c` parses CLI configuration and starts the proxy server.
2. `proxy_server.c` initializes the socket layer, shared cache, metrics, and thread pool.
3. The main listener thread accepts client sockets and submits them to the worker queue.
4. A worker thread reads request bytes with size limits and timeouts.
5. `request_parser.c` parses the proxy-style absolute `GET` request.
6. The worker builds a normalized cache key from method, host, port, and path.
7. On cache hit, the cached response bytes are written directly back to the client.
8. On cache miss, `response_forwarder.c` connects to the origin, rewrites the request into origin form, relays the response, and captures eligible bytes for caching.
9. If the origin response fits cache policy, it is inserted into the shared cache.
10. The worker closes the client socket and updates metrics.

## Concurrency Model

The server uses a bounded thread pool rather than creating one thread per connection.

- the accept loop is single-threaded
- accepted clients are pushed into a synchronized bounded queue
- worker threads pull jobs from the queue and process requests independently
- the shared cache and metrics module each protect internal state with a mutex

This keeps concurrency explicit and contained:

- `thread_pool.c` owns queue coordination
- `proxy_server.c` owns request lifecycle logic
- `cache.c` owns shared cache synchronization
- `metrics.c` owns counter synchronization

## Cache Design

The shared response cache combines:

- a hash table for lookup by normalized request key
- a doubly linked LRU list for eviction order

Current cache key:

```text
METHOD|HOST|PORT|PATH
```

Current cache policy:

- only the current plain HTTP `GET` flow is eligible
- only fully captured responses are stored
- oversized responses are not cached
- eviction removes least-recently-used objects first

## Module Boundaries

### Runtime control

- `main.c`
- `proxy_server.c`

### Networking

- `socket_utils.c`
- `response_forwarder.c`

### Concurrency

- `thread_pool.c`

### Parsing

- `request_parser.c`

### Caching

- `cache.c`
- `lru.c`

### Observability

- `logger.c`
- `metrics.c`

### Validation

- `tests/test_request_parser.c`
- `tests/test_lru.c`
- `tests/test_cache.c`
- `tests/test_metrics.c`

## Failure Handling

The request path currently returns explicit HTTP error responses for common failure cases:

- malformed request
- oversized request headers
- request timeout
- origin fetch failure

The proxy also emits structured lifecycle logs and a metrics summary on graceful shutdown.
