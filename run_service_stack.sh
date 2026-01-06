#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

SERVER_BIN="$SCRIPT_DIR/service_server_app"
CLIENT_BIN="$SCRIPT_DIR/service_client"

LOG_DIR="$SCRIPT_DIR/logs"
mkdir -p "$LOG_DIR"

TS="$(date +"%Y%m%d_%H%M%S")"
SERVER_LOG="$LOG_DIR/service_server_${TS}.log"
CLIENT1_LOG="$LOG_DIR/service_client1_${TS}.log"
CLIENT2_LOG="$LOG_DIR/service_client2_${TS}.log"

usage() {
  cat <<'USAGE'
Usage:
  ./run_service_stack.sh [--no-build] [--delay SECONDS]

Starts:
  - 1x ./service_server_app
  - 2x ./service_client

Notes:
  - Clients are SDL GUI apps; they will open two windows.
  - Logs are written into ./logs/.
USAGE
}

DO_BUILD=1
DELAY_SECONDS=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build)
      DO_BUILD=0
      shift
      ;;
    --delay)
      DELAY_SECONDS="${2:-}"
      if [[ -z "$DELAY_SECONDS" ]]; then
        echo "[Error] --delay requires a value" >&2
        exit 2
      fi
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[Error] Unknown arg: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ $DO_BUILD -eq 1 ]]; then
  if [[ ! -x "$SERVER_BIN" || ! -x "$CLIENT_BIN" ]]; then
    echo "[Info] Building service server + client via make..."
    make service_server service_client
  fi
fi

if [[ ! -x "$SERVER_BIN" ]]; then
  echo "[Error] Missing executable: $SERVER_BIN" >&2
  echo "        Run: make service_server" >&2
  exit 1
fi

if [[ ! -x "$CLIENT_BIN" ]]; then
  echo "[Error] Missing executable: $CLIENT_BIN" >&2
  echo "        Run: make service_client" >&2
  exit 1
fi

PIDS=()
cleanup() {
  echo "[Info] Shutting down..."
  # kill in reverse order (clients first, then server)
  for (( idx=${#PIDS[@]}-1; idx>=0; idx-- )); do
    pid="${PIDS[$idx]}"
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid" 2>/dev/null || true
    fi
  done

  # Give processes a moment to exit, then force kill if needed
  sleep 0.5 || true
  for pid in "${PIDS[@]}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill -9 "$pid" 2>/dev/null || true
    fi
  done
}
trap cleanup INT TERM EXIT

echo "[Info] Starting service server..."
( stdbuf -oL -eL "$SERVER_BIN" ) >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
PIDS+=("$SERVER_PID")
echo "[Info] Server PID: $SERVER_PID (log: $SERVER_LOG)"

sleep "$DELAY_SECONDS"

echo "[Info] Starting service client #1..."
( stdbuf -oL -eL "$CLIENT_BIN" ) >"$CLIENT1_LOG" 2>&1 &
CLIENT1_PID=$!
PIDS+=("$CLIENT1_PID")
echo "[Info] Client1 PID: $CLIENT1_PID (log: $CLIENT1_LOG)"

echo "[Info] Starting service client #2..."
( stdbuf -oL -eL "$CLIENT_BIN" ) >"$CLIENT2_LOG" 2>&1 &
CLIENT2_PID=$!
PIDS+=("$CLIENT2_PID")
echo "[Info] Client2 PID: $CLIENT2_PID (log: $CLIENT2_LOG)"

echo "[Info] Running. Press Ctrl+C to stop all."

# Wait until server exits (or until Ctrl+C triggers trap)
wait "$SERVER_PID"
