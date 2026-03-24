# Benchmarks

## Goal

The benchmark workflow in this repository is intentionally local and reproducible. It avoids external network dependencies by benchmarking the proxy against a local Python origin server.

## Benchmark Scope

The benchmark script measures two behaviors:

- cache benefit: one cold request followed by one hot request for the same URL
- bounded concurrency: multiple delayed origin requests issued in parallel through a fixed-size worker pool

The script does not attempt to be a full load-testing harness. It is meant to provide a fast local signal that the cache and worker pool materially change runtime behavior in the expected direction.

## Prerequisites

- a built proxy binary: `proxy_server` or `proxy_server.exe`
- `curl`
- `python3` or `python`
- a POSIX shell environment such as Linux, macOS, WSL, or Git Bash on Windows

If one of those tools is missing, the scripts exit with a short error message and a suggested next step.

## Local Demo

`scripts/run_local_demo.sh` starts:

- a local static origin server on `127.0.0.1`
- the proxy with configurable worker count and cache size
- two requests through the proxy to exercise the request path and cache path

Run it from the repository root:

```sh
sh scripts/run_local_demo.sh
```

Useful overrides:

```sh
PROXY_PORT=19095 WORKERS=4 LOG_LEVEL=debug sh scripts/run_local_demo.sh
```

## Local Benchmark

`scripts/benchmark.sh` starts a delayed local origin server and the proxy, then prints a small benchmark summary.

Run it from the repository root:

```sh
sh scripts/benchmark.sh
```

Useful overrides:

```sh
WORKERS=2 REQUEST_COUNT=8 DELAY_MS=300 LOG_LEVEL=info sh scripts/benchmark.sh
```

The benchmark summary includes:

- `cold_request_seconds`: first request for a cacheable URL
- `hot_request_seconds`: repeated request for the same URL after the cache is populated
- `cold_to_hot_ratio`: rough cache benefit signal
- `elapsed_seconds`: wall-clock time for multiple delayed requests sent in parallel
- `serial_delay_baseline`: what the request set would cost if handled strictly serially
- `worker_batching_expectation`: the approximate lower bound implied by the configured worker count and origin delay

## How To Interpret Results

For the cache path:

- the hot request should be materially faster than the cold request because the proxy serves the response from local memory instead of waiting on the delayed origin

For the concurrency path:

- elapsed time should be closer to the worker-batching expectation than to the serial-delay baseline
- increasing `WORKERS` should reduce the elapsed time until the origin delay or local machine scheduling becomes the dominant factor

## Notes And Limits

- the benchmark uses loopback networking, so the results are most useful as relative comparisons on the same machine
- the local origin intentionally sleeps before responding to make cache and concurrency behavior easy to observe
- absolute timings will vary by operating system, shell environment, compiler, and machine load
- the benchmark currently exercises plain HTTP only; HTTPS CONNECT is not part of this workflow
