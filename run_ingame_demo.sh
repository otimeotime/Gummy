#!/usr/bin/env bash
set -euo pipefail

PORT="${PORT:-9090}"
IP="${IP:-127.0.0.1}"

# Usage: ./run_ingame_demo.sh <map name>
# Example: ./run_ingame_demo.sh map1.txt
MAP_ARG="${1:-flatmap.txt}"
if [[ "$MAP_ARG" == */* ]]; then
  MAP_PATH="$MAP_ARG"
else
  MAP_PATH="assets/maps/$MAP_ARG"
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

SERVER_BIN="./ingame_server_demo"
CLIENT_BIN="./net_game_client"

LOG_DIR="$ROOT_DIR/logs"
mkdir -p "$LOG_DIR"

TS="$(date +"%Y%m%d_%H%M%S")"
SERVER_LOG="$LOG_DIR/ingame_server_${TS}.log"
CLIENT1_LOG="$LOG_DIR/ingame_client1_${TS}.log"
CLIENT2_LOG="$LOG_DIR/ingame_client2_${TS}.log"
REPLAY_OUT="$LOG_DIR/replay_ingame_${TS}.grpl"

cleanup() {
  set +e
  echo "Stopping clients/server..."

  if [[ -n "${CLIENT2_PID:-}" ]] && kill -0 "$CLIENT2_PID" 2>/dev/null; then
    kill "$CLIENT2_PID" 2>/dev/null
  fi
  if [[ -n "${CLIENT1_PID:-}" ]] && kill -0 "$CLIENT1_PID" 2>/dev/null; then
    kill "$CLIENT1_PID" 2>/dev/null
  fi

  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    # Let the server handle SIGINT cleanly.
    kill -INT "$SERVER_PID" 2>/dev/null
    sleep 0.5
    kill "$SERVER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

if [[ ! -x "$SERVER_BIN" || ! -x "$CLIENT_BIN" ]]; then
  echo "ERROR: missing binaries. Expected $SERVER_BIN and $CLIENT_BIN" >&2
  echo "Build once with: make server client -j" >&2
  exit 1
fi

echo "Starting server on port $PORT (recording to $REPLAY_OUT)..."
"$SERVER_BIN" --port "$PORT" --record "$REPLAY_OUT" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!

# Wait for TCP port to accept connections.
# Uses bash's /dev/tcp; retries for up to ~5 seconds.
echo -n "Waiting for server to accept connections"
for _ in {1..50}; do
  if (exec 3<>"/dev/tcp/$IP/$PORT") 2>/dev/null; then
    exec 3>&-
    exec 3<&-
    echo " OK"
    break
  fi
  echo -n "."
  sleep 0.1
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo
    echo "ERROR: server exited early. Check $LOG_DIR/ingame_server_demo.log" >&2
    exit 1
  fi
  if [[ $_ -eq 50 ]]; then
    echo
    echo "ERROR: timed out waiting for $IP:$PORT. Check $LOG_DIR/ingame_server_demo.log" >&2
    exit 1
  fi
done

echo "Launching 2 clients (close a window or Ctrl+C here to stop all)..."
"$CLIENT_BIN" "$IP" "$PORT" "$MAP_PATH" >"$CLIENT1_LOG" 2>&1 &
CLIENT1_PID=$!

sleep 0.3

"$CLIENT_BIN" "$IP" "$PORT" "$MAP_PATH" >"$CLIENT2_LOG" 2>&1 &
CLIENT2_PID=$!

# Wait until any client exits, then cleanup via trap.
wait -n "$CLIENT1_PID" "$CLIENT2_PID"
