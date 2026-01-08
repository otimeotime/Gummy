#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

SERVER_BIN="$ROOT_DIR/ingame_server_demo"
VIEWER_BIN="$ROOT_DIR/net_game_client"

IP="${IP:-127.0.0.1}"
# Track whether the caller explicitly chose a port (env PORT or --port).
PORT_EXPLICIT=0
if [[ -n "${PORT+x}" ]]; then
  PORT_EXPLICIT=1
fi
if [[ "$PORT_EXPLICIT" -eq 0 ]]; then
  # Pick a random port to avoid collisions with the default Service port (9090)
  # range 10000-15000
  PORT=$(( 10000 + RANDOM % 5000 ))
fi
PORT="${PORT:-9090}" # Fallback if random fails or blocked (unlikely)
MAP_PATH_DEFAULT="assets/maps/flatmap.txt"

usage() {
  cat <<'USAGE'
Usage:
  ./run_replay_viewer.sh <replay.grpl> [--ip IP] [--port PORT] [--username NAME] [--no-build] [--map MAP_PATH]

Starts:
  - ./ingame_server_demo --replay <replay.grpl>
  - ./net_game_client <ip> <port> <mapPath> <username>

Notes:
  - The viewer connects as a spectator (replay server assigns playerId=UINT32_MAX).
  - Map is for client-side visuals. With replay header v2, the server sends the recorded mapPath and the client auto-reloads it.
  - If you want to override or if the replay is an older v1 file, you can still pass --map. Default: assets/maps/flatmap.txt
  - Logs written to ./logs/
USAGE
}

DO_BUILD=1
REPLAY_PATH=""
MAP_PATH="$MAP_PATH_DEFAULT"
USERNAME=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ip)
      IP="${2:-}"; shift 2;;
    --port)
      PORT_EXPLICIT=1
      PORT="${2:-}"; shift 2;;
    --map)
      MAP_PATH="${2:-}"; shift 2;;
    --username)
      USERNAME="${2:-}"; shift 2;;
    --no-build)
      DO_BUILD=0; shift;;
    -h|--help)
      usage; exit 0;;
    *)
      if [[ -z "$REPLAY_PATH" ]]; then
        REPLAY_PATH="$1"; shift
      else
        echo "[Error] Unknown arg: $1" >&2
        usage >&2
        exit 2
      fi
      ;;
  esac
done

# If the user didn't explicitly pick a port (and no PORT env override), avoid
# colliding with an active ingame server by picking a free port in 9090-9099.
if [[ "${PORT}" == "9090" && "$PORT_EXPLICIT" -eq 0 ]]; then
  # Best-effort check: if something is already accepting on 9090, pick another.
  if (exec 3<>"/dev/tcp/$IP/$PORT") 2>/dev/null; then
    exec 3>&-; exec 3<&-;
    for p in 9091 9092 9093 9094 9095 9096 9097 9098 9099; do
      if (exec 3<>"/dev/tcp/$IP/$p") 2>/dev/null; then
        exec 3>&-; exec 3<&-;
        continue
      fi
      PORT="$p"
      break
    done
  fi
fi

if [[ -z "$REPLAY_PATH" ]]; then
  echo "[Error] replay file path required" >&2
  usage >&2
  exit 2
fi

LOG_DIR="$ROOT_DIR/logs"
mkdir -p "$LOG_DIR"
TS="$(date +"%Y%m%d_%H%M%S")"
SERVER_LOG="$LOG_DIR/replay_server_${TS}.log"
VIEWER_LOG="$LOG_DIR/replay_viewer_${TS}.log"

if [[ $DO_BUILD -eq 1 ]]; then
  if [[ ! -x "$SERVER_BIN" || ! -x "$VIEWER_BIN" ]]; then
    echo "[Info] Building server + viewer..."
    make server client -j
  fi
fi

if [[ ! -x "$SERVER_BIN" ]]; then
  echo "[Error] Missing executable: $SERVER_BIN" >&2
  echo "        Run: make server" >&2
  exit 1
fi

if [[ ! -x "$VIEWER_BIN" ]]; then
  echo "[Error] Missing executable: $VIEWER_BIN" >&2
  echo "        Run: make client" >&2
  exit 1
fi

cleanup() {
  set +e
  echo "[Info] Shutting down replay server..."
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill -INT "$SERVER_PID" 2>/dev/null || true
    sleep 0.3 || true
    kill "$SERVER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

echo "[Info] Starting replay server on $IP:$PORT from: $REPLAY_PATH"
echo "[Info] Server log: $SERVER_LOG"
"$SERVER_BIN" --port "$PORT" --replay "$REPLAY_PATH" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!

# Wait for TCP port to accept connections.
echo -n "[Info] Waiting for replay server to accept connections"
for i in {1..50}; do
  if (exec 3<>"/dev/tcp/$IP/$PORT") 2>/dev/null; then
    exec 3>&-; exec 3<&-;
    echo " OK"
    break
  fi
  echo -n "."
  sleep 0.1
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo
    echo "[Error] Replay server exited early. Check: $SERVER_LOG" >&2
    exit 1
  fi
  if [[ $i -eq 50 ]]; then
    echo
    echo "[Error] Timed out waiting for $IP:$PORT. Check: $SERVER_LOG" >&2
    exit 1
  fi
done

echo "[Info] Starting SDL replay viewer... (log: $VIEWER_LOG)"
if [[ -n "$USERNAME" ]]; then
  "$VIEWER_BIN" "$IP" "$PORT" "$MAP_PATH" "$USERNAME" >"$VIEWER_LOG" 2>&1
else
  "$VIEWER_BIN" "$IP" "$PORT" "$MAP_PATH" >"$VIEWER_LOG" 2>&1
fi
