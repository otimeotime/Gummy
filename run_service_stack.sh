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
  ./run_service_stack.sh <replay.grpl> [--no-build]

Starts:
  - 1x ./service_server_app
  - 2x ./service_client

Replay mode:
  - If you pass a .grpl path, this script starts the replay viewer in the background
    and exits immediately.

Notes:
  - Clients are SDL GUI apps; they will open two windows.
  - Logs are written into ./logs/.
USAGE
}

DO_BUILD=1
DELAY_SECONDS=1
REPLAY_PATH=""

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
      if [[ -z "$REPLAY_PATH" && -f "$1" && "$1" == *.grpl ]]; then
        REPLAY_PATH="$1"
        shift
      else
        echo "[Error] Unknown arg: $1" >&2
        usage >&2
        exit 2
      fi
      ;;
  esac
done

if [[ -n "$REPLAY_PATH" ]]; then
  # Replay-only helper mode: launch the viewer in background and exit.
  if [[ ! -f "$REPLAY_PATH" ]]; then
    echo "[Error] Replay file not found: $REPLAY_PATH" >&2
    exit 1
  fi

  if [[ $DO_BUILD -eq 1 ]]; then
    if [[ ! -x "$SCRIPT_DIR/ingame_server_demo" || ! -x "$SCRIPT_DIR/net_game_client" ]]; then
      echo "[Info] Building server + viewer via make..."
      make server client -j
    fi
  fi

  # Pick a free replay port to avoid conflicts (e.g., service ingame server uses 9090).
  PICKED_PORT=""
  for p in 9090 9091 9092 9093 9094 9095 9096 9097 9098 9099; do
    if (exec 3<>"/dev/tcp/127.0.0.1/$p") 2>/dev/null; then
      exec 3>&-; exec 3<&-;
      continue
    fi
    PICKED_PORT="$p"
    break
  done
  if [[ -z "$PICKED_PORT" ]]; then
    echo "[Error] Could not find a free port in 9090-9099 for replay." >&2
    exit 1
  fi

  LAUNCH_LOG="$LOG_DIR/replay_launch_${TS}.log"
  echo "[Info] Starting replay in background: $REPLAY_PATH"
  echo "[Info] Using port: $PICKED_PORT"
  echo "[Info] Launch log: $LAUNCH_LOG"

  if [[ $DO_BUILD -eq 1 ]]; then
    ( PORT="$PICKED_PORT" ./run_replay_viewer.sh "$REPLAY_PATH" ) >"$LAUNCH_LOG" 2>&1 &
  else
    ( PORT="$PICKED_PORT" ./run_replay_viewer.sh --no-build "$REPLAY_PATH" ) >"$LAUNCH_LOG" 2>&1 &
  fi

  echo "[Info] Replay launcher PID: $!"
  # Best-effort detach so Ctrl+C in this shell doesn't affect the replay.
  disown || true
  exit 0
fi

if [[ $DO_BUILD -eq 1 ]]; then
  if [[ ! -x "$SERVER_BIN" || ! -x "$CLIENT_BIN" ]]; then
    echo "[Info] Building service server + client via make..."
    make service_server service_client server client
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
( 
  # If something is already listening on 127.0.0.1:8080, starting will fail.
  # We avoid auto-killing to prevent terminating unrelated services.
  if (exec 3<>"/dev/tcp/127.0.0.1/8080") 2>/dev/null; then
    exec 3>&-; exec 3<&-;
    echo "[Error] Port 8080 is already in use. Stop the existing process and retry." >&2
    echo "        Hint: ss -ltnp '( sport = :8080 )'" >&2
    exit 1
  fi
)
( REPLAY_SESSION_TS="$TS" REPLAY_DIR="$LOG_DIR" stdbuf -oL -eL "$SERVER_BIN" ) >"$SERVER_LOG" 2>&1 &
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
