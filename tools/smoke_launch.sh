#!/usr/bin/env bash
# Launch moth_sprite in a temporary directory, let it run, close it the way a user would, and check the result.
#
# Usage: tools/smoke_launch.sh [seconds to run, default 5]
#
# Passes (exit 0) when the app exits with code 0, the log has no [warning] or [error] lines, and
# moth_sprite.json was written, which shows Shutdown() ran to the end. On failure the log is kept and its
# path is printed.
#
# Needs an X11 display, xdotool and python3.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/Debug/moth_sprite"
RUN_SECONDS="${1:-5}"

fail_setup() {
    echo "SMOKE LAUNCH CANNOT RUN: $1"
    exit 2
}

[ -x "$BIN" ] || fail_setup "$BIN not found. Build first."
[ -n "${DISPLAY:-}" ] || fail_setup "DISPLAY is not set."
command -v xdotool > /dev/null || fail_setup "xdotool is not installed."
command -v python3 > /dev/null || fail_setup "python3 is not installed."

# The app reads and writes moth_sprite.json and imgui.ini in the current directory.
# Run it somewhere else so the user's files are not changed.
DIR="$(mktemp -d -t moth_sprite_smoke.XXXXXX)"
LOG="$DIR/run.log"
cd "$DIR" || fail_setup "cannot enter $DIR"

"$BIN" > "$LOG" 2>&1 &
PID=$!

FAILURES=()

WID="$(timeout 15 xdotool search --sync --pid "$PID" --name "Moth Sprite" 2> /dev/null | head -n 1)"
if [ -z "$WID" ]; then
    FAILURES+=("window did not appear within 15 seconds")
else
    sleep "$RUN_SECONDS"
    if ! kill -0 "$PID" 2> /dev/null; then
        FAILURES+=("app exited before it was closed")
    elif ! python3 "$ROOT/tools/close_window.py" "$WID"; then
        FAILURES+=("could not send the close request")
    fi
fi

for _ in $(seq 1 150); do
    kill -0 "$PID" 2> /dev/null || break
    sleep 0.1
done
if kill -0 "$PID" 2> /dev/null; then
    FAILURES+=("app did not exit within 15 seconds of the close request")
    kill -9 "$PID" 2> /dev/null
fi
wait "$PID"
EXIT_CODE=$?

[ "$EXIT_CODE" -eq 0 ] || FAILURES+=("exit code $EXIT_CODE")
[ -f "$DIR/moth_sprite.json" ] || FAILURES+=("moth_sprite.json not written, so Shutdown() did not finish")
if grep -qE '\[(warning|error)\]' "$LOG"; then
    FAILURES+=("log has warning or error lines")
fi

if [ "${#FAILURES[@]}" -eq 0 ]; then
    echo "SMOKE LAUNCH PASSED"
    rm -rf "$DIR"
    exit 0
fi

echo "SMOKE LAUNCH FAILED"
for reason in "${FAILURES[@]}"; do
    echo "  - $reason"
done
echo "Warning and error lines:"
grep -E '\[(warning|error)\]' "$LOG" | sed 's/^/  /' || echo "  (none)"
echo "Full log: $LOG"
exit 1
