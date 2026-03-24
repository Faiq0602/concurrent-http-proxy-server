#!/usr/bin/env sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN=python3
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN=python
else
    echo "python3 or python is required for the local benchmark." >&2
    echo "Install Python, then rerun scripts/benchmark.sh." >&2
    exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "curl is required for the local benchmark." >&2
    echo "Install curl, then rerun scripts/benchmark.sh." >&2
    exit 1
fi

if [ -x "$REPO_ROOT/proxy_server" ]; then
    PROXY_BIN="$REPO_ROOT/proxy_server"
elif [ -f "$REPO_ROOT/proxy_server.exe" ]; then
    PROXY_BIN="$REPO_ROOT/proxy_server.exe"
else
    echo "proxy_server binary not found." >&2
    echo "Build the project first with 'make' or 'mingw32-make'." >&2
    exit 1
fi

ORIGIN_PORT=${ORIGIN_PORT:-18081}
PROXY_PORT=${PROXY_PORT:-19091}
WORKERS=${WORKERS:-2}
CACHE_SIZE=${CACHE_SIZE:-1048576}
LOG_LEVEL=${LOG_LEVEL:-warn}
DELAY_MS=${DELAY_MS:-250}
REQUEST_COUNT=${REQUEST_COUNT:-6}
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t proxy-benchmark)
ORIGIN_SCRIPT="$TMP_DIR/delayed_origin.py"
ORIGIN_LOG="$TMP_DIR/origin.log"
PROXY_LOG="$TMP_DIR/proxy.log"

cleanup() {
    if [ "${PROXY_PID:-}" ]; then
        kill "$PROXY_PID" >/dev/null 2>&1 || true
        wait "$PROXY_PID" 2>/dev/null || true
    fi

    if [ "${ORIGIN_PID:-}" ]; then
        kill "$ORIGIN_PID" >/dev/null 2>&1 || true
        wait "$ORIGIN_PID" 2>/dev/null || true
    fi

    rm -rf "$TMP_DIR"
}

trap cleanup EXIT INT TERM

cat >"$ORIGIN_SCRIPT" <<'EOF'
import argparse
import http.server
import socketserver
import time


class ThreadingHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


def build_handler(delay_ms: int):
    class DelayedHandler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            time.sleep(delay_ms / 1000.0)
            body = f"delay_ms={delay_ms} path={self.path}\n".encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, format, *args):
            return

    return DelayedHandler


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--delay-ms", type=int, required=True)
    args = parser.parse_args()

    server = ThreadingHTTPServer(("127.0.0.1", args.port), build_handler(args.delay_ms))
    server.serve_forever()


if __name__ == "__main__":
    main()
EOF

"$PYTHON_BIN" "$ORIGIN_SCRIPT" --port "$ORIGIN_PORT" --delay-ms "$DELAY_MS" >"$ORIGIN_LOG" 2>&1 &
ORIGIN_PID=$!

"$PROXY_BIN" --port "$PROXY_PORT" --workers "$WORKERS" --cache-size "$CACHE_SIZE" --log-level "$LOG_LEVEL" >"$PROXY_LOG" 2>&1 &
PROXY_PID=$!

sleep 1

if ! kill -0 "$ORIGIN_PID" >/dev/null 2>&1; then
    echo "failed to start local origin server" >&2
    echo "origin log: $ORIGIN_LOG" >&2
    exit 1
fi

if ! kill -0 "$PROXY_PID" >/dev/null 2>&1; then
    echo "failed to start proxy server" >&2
    echo "proxy log: $PROXY_LOG" >&2
    exit 1
fi

cache_url="http://127.0.0.1:$ORIGIN_PORT/cache-target"
proxy_url="http://127.0.0.1:$PROXY_PORT"

cold_time=$(curl -sS -o /dev/null -w "%{time_total}" --proxy "$proxy_url" "$cache_url")
hot_time=$(curl -sS -o /dev/null -w "%{time_total}" --proxy "$proxy_url" "$cache_url")

start_time=$("$PYTHON_BIN" -c "import time; print(time.perf_counter())")
i=1
while [ "$i" -le "$REQUEST_COUNT" ]; do
    curl -sS -o /dev/null --proxy "$proxy_url" "http://127.0.0.1:$ORIGIN_PORT/concurrency-$i" &
    eval "curl_pid_$i=$!"
    i=$((i + 1))
done
i=1
while [ "$i" -le "$REQUEST_COUNT" ]; do
    eval "wait \$curl_pid_$i"
    i=$((i + 1))
done
end_time=$("$PYTHON_BIN" -c "import time; print(time.perf_counter())")

concurrency_time=$("$PYTHON_BIN" -c "print(round(float('$end_time') - float('$start_time'), 4))")
speedup_ratio=$("$PYTHON_BIN" -c "cold=float('$cold_time'); hot=float('$hot_time'); print(round(cold / hot, 2) if hot > 0 else 'inf')")
expected_serial=$("$PYTHON_BIN" -c "print(round(($REQUEST_COUNT * $DELAY_MS) / 1000.0, 4))")
expected_batched=$("$PYTHON_BIN" -c "requests=$REQUEST_COUNT; workers=$WORKERS; delay=$DELAY_MS / 1000.0; batches=(requests + workers - 1) // workers; print(round(batches * delay, 4))")

echo "benchmark configuration"
echo "  proxy_port: $PROXY_PORT"
echo "  origin_port: $ORIGIN_PORT"
echo "  workers: $WORKERS"
echo "  cache_size_bytes: $CACHE_SIZE"
echo "  delay_ms: $DELAY_MS"
echo "  request_count: $REQUEST_COUNT"
echo
echo "cache benchmark"
echo "  cold_request_seconds: $cold_time"
echo "  hot_request_seconds:  $hot_time"
echo "  cold_to_hot_ratio:    $speedup_ratio"
echo
echo "concurrency benchmark"
echo "  elapsed_seconds:              $concurrency_time"
echo "  serial_delay_baseline:        $expected_serial"
echo "  worker_batching_expectation:  $expected_batched"
echo
echo "proxy log: $PROXY_LOG"
