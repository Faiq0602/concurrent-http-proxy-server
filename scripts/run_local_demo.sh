#!/usr/bin/env sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN=python3
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN=python
else
    echo "python3 or python is required to run the local demo." >&2
    echo "Install Python, then rerun scripts/run_local_demo.sh." >&2
    exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "curl is required to run the local demo." >&2
    echo "Install curl, then rerun scripts/run_local_demo.sh." >&2
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

ORIGIN_PORT=${ORIGIN_PORT:-18080}
PROXY_PORT=${PROXY_PORT:-19090}
WORKERS=${WORKERS:-2}
CACHE_SIZE=${CACHE_SIZE:-1048576}
LOG_LEVEL=${LOG_LEVEL:-info}

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t proxy-demo)
ORIGIN_DIR="$TMP_DIR/origin"
PROXY_LOG="$TMP_DIR/proxy.log"
ORIGIN_LOG="$TMP_DIR/origin.log"
mkdir -p "$ORIGIN_DIR"

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

cat >"$ORIGIN_DIR/index.html" <<EOF
<!doctype html>
<html>
<body>
<h1>concurrent-http-proxy-server</h1>
<p>local demo content</p>
</body>
</html>
EOF

"$PYTHON_BIN" -m http.server "$ORIGIN_PORT" --bind 127.0.0.1 --directory "$ORIGIN_DIR" >"$ORIGIN_LOG" 2>&1 &
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

DEMO_URL="http://127.0.0.1:$ORIGIN_PORT/"
PROXY_URL="http://127.0.0.1:$PROXY_PORT"

echo "origin: $DEMO_URL"
echo "proxy:  $PROXY_URL"
echo "sending first request through the proxy"
curl -sS --proxy "$PROXY_URL" "$DEMO_URL" >/dev/null

echo "sending second request through the proxy"
curl -sS --proxy "$PROXY_URL" "$DEMO_URL" >/dev/null

echo "demo complete"
echo "proxy log:"
cat "$PROXY_LOG"
