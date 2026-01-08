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
    kill -INT "$SERVER_PID" 2>/dev/null || true
    # Wait briefly; if it doesn't exit, force kill.
    for _ in {1..10}; do
      if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        break
      fi
      sleep 0.1
    done
    if kill -0 "$SERVER_PID" 2>/dev/null; then
      kill "$SERVER_PID" 2>/dev/null || true
    fi
  fi
}
trap cleanup EXIT INT TERM

if [[ ! -x "$SERVER_BIN" || ! -x "$CLIENT_BIN" ]]; then
  echo "ERROR: missing binaries. Expected $SERVER_BIN and $CLIENT_BIN" >&2
  echo "Build once with: make server client -j" >&2
  exit 1
fi

echo "Starting server on port $PORT (recording to $REPLAY_OUT)..."

# If something is already listening on the desired port, it will cause a false-positive
# readiness check and clients will connect to the wrong server. Try to stop it first.
if command -v ss >/dev/null 2>&1; then
  EXISTING_PIDS="$(ss -ltnpH "sport = :$PORT" 2>/dev/null | sed -n 's/.*pid=\([0-9]\+\).*/\1/p' | sort -u)"
  if [[ -n "$EXISTING_PIDS" ]]; then
    echo "Port $PORT is already in use by PID(s): $EXISTING_PIDS. Stopping them..."
    for pid in $EXISTING_PIDS; do
      kill -INT "$pid" 2>/dev/null || true
    done
    for _ in {1..30}; do
      if ss -ltnH "sport = :$PORT" 2>/dev/null | grep -q ":$PORT"; then
        sleep 0.1
      else
        break
      fi
    done
  fi
fi

# Start server in a separate process group so terminal Ctrl+C doesn't also
# send SIGINT directly to it (shutdown is handled by this script's trap).
if command -v setsid >/dev/null 2>&1; then
  setsid "$SERVER_BIN" --port "$PORT" --record "$REPLAY_OUT" >"$SERVER_LOG" 2>&1 &
else
  "$SERVER_BIN" --port "$PORT" --record "$REPLAY_OUT" >"$SERVER_LOG" 2>&1 &
fi
SERVER_PID=$!

# Wait for server readiness.
# IMPORTANT: do not just check LISTEN on the port, because an unrelated process
# could be listening (including a leftover replay server), causing clients to
# connect to the wrong server.
echo -n "Waiting for server to start listening"
for i in {1..80}; do
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo
    echo "ERROR: server exited early. Last 60 lines of $SERVER_LOG:" >&2
    tail -n 60 "$SERVER_LOG" >&2 || true
    exit 1
  fi

  if grep -q "GameServer listening on port $PORT" "$SERVER_LOG" 2>/dev/null; then
    echo " OK"
    break
  fi

  echo -n "."
  sleep 0.1

  if [[ $i -eq 80 ]]; then
    echo
    echo "ERROR: timed out waiting for server readiness. Last 60 lines of $SERVER_LOG:" >&2
    tail -n 60 "$SERVER_LOG" >&2 || true
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
