# Test Notes

This repository keeps automated checks intentionally simple:

- `mingw32-make test` builds and runs the core unit tests
- parser behavior is covered in `tests/test_request_parser.c`
- LRU ordering is covered in `tests/test_lru.c`
- cache insertion, eviction, and overwrite behavior are covered in `tests/test_cache.c`
- operational counter behavior is covered in `tests/test_metrics.c`

## Recommended local validation flow

1. Run `mingw32-make clean`
2. Run `mingw32-make`
3. Run `mingw32-make test`

## Manual integration checks

### Plain proxy forwarding

1. Start a local HTTP origin server:
   `python -m http.server 18080`
2. Start the proxy:
   `proxy_server.exe --port 9093 --workers 2 --log-level info`
3. Send a request through the proxy:
   `curl -v --proxy http://127.0.0.1:9093 http://127.0.0.1:18080/`
4. Verify `200 OK` is returned and the proxy logs the request lifecycle.

### Cache miss then hit

1. Start the proxy with caching enabled:
   `proxy_server.exe --port 9097 --workers 2 --cache-size 1048576 --log-level info`
2. Send the same request twice:
   `curl -v --proxy http://127.0.0.1:9097 http://127.0.0.1:18080/cache-demo`
   `curl -v --proxy http://127.0.0.1:9097 http://127.0.0.1:18080/cache-demo`
3. Verify logs show one `cache miss` and one `cache hit`.

### Malformed request handling

1. Start the proxy:
   `proxy_server.exe --port 9095 --log-level info`
2. Send an unsupported proxy request with a raw TCP client:
   `POST http://example.com/submit HTTP/1.1`
3. Verify the proxy returns `HTTP/1.1 400 Bad Request`.

### Bounded concurrency

1. Start the proxy with a small worker count:
   `proxy_server.exe --port 9096 --workers 2 --log-level debug`
2. Launch multiple proxied `curl` requests in parallel against a delayed origin.
3. Verify all requests succeed and that total elapsed time reflects batching through the worker pool rather than strict serial execution.

### Metrics summary

1. Start the proxy and send a small mix of miss/hit traffic.
2. Stop the proxy gracefully.
3. Verify the final logs include a metrics summary line with request, cache, and byte counters.
