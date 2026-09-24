#!/usr/bin/env bash
set -euo pipefail

SHELL_BIN="./bin/aegissh"
PASSED=0
FAILED=0

assert_eq() {
    local test_name="$1"
    local expected="$2"
    local actual="$3"

    if [ "$expected" = "$actual" ]; then
        echo "  [PASS] $test_name"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $test_name"
        echo "    Expected: '$expected'"
        echo "    Actual:   '$actual'"
        FAILED=$((FAILED + 1))
    fi
}

assert_contains() {
    local test_name="$1"
    local expected_sub="$2"
    local actual="$3"

    if [[ "$actual" == *"$expected_sub"* ]]; then
        echo "  [PASS] $test_name"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $test_name"
        echo "    Expected to contain: '$expected_sub'"
        echo "    Actual:             '$actual'"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Running Phase 5 Test Suite for AegisShell (Background Processes) ==="

# Test 1: Background execution syntax [1] <pid> and immediate return
START_TIME=$(date +%s)
OUTPUT=$(printf "sleep 1 &\necho immediate\nexit\n" | $SHELL_BIN | tr -d '\r')
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

assert_contains "background output prints job id [1]" "[1]" "$OUTPUT"
assert_contains "immediate command executed" "immediate" "$OUTPUT"
assert_eq "non-blocking background execution" "true" "$([ $ELAPSED -lt 2 ] && echo true || echo false)"

# Test 2: Background process is actually alive while shell is active
FIFO_IN=$(mktemp -u /tmp/aegissh_fifo_XXXXXX)
OUT_LOG=$(mktemp /tmp/aegissh_out_XXXXXX)
mkfifo "$FIFO_IN"

$SHELL_BIN < "$FIFO_IN" > "$OUT_LOG" 2>&1 &
SHELL_BG_PID=$!

echo "sleep 2 &" > "$FIFO_IN"
sleep 0.2
PID=$(grep -o '\[1\] [0-9]*' "$OUT_LOG" | tail -n 1 | awk '{print $2}')

STILL_ALIVE="false"
if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
    STILL_ALIVE="true"
    kill "$PID" 2>/dev/null || true
fi
assert_eq "background PID is alive after launch" "true" "$STILL_ALIVE"

echo "exit" > "$FIFO_IN"
wait "$SHELL_BG_PID" 2>/dev/null || true
rm -f "$FIFO_IN" "$OUT_LOG"

# Test 3: Background process completion and reaping notification
OUTPUT=$(printf "sleep 0.2 &\nsleep 0.4\necho tick\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_contains "completion notification contains Done" "Done" "$OUTPUT"

# Test 4: Multiple background jobs receive incrementing IDs
OUTPUT=$(printf "sleep 0.3 &\nsleep 0.3 &\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_contains "first job is [1]" "[1]" "$OUTPUT"
assert_contains "second job is [2]" "[2]" "$OUTPUT"

# Test 5: Background pipeline
OUTPUT=$(printf "sleep 0.1 | cat &\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_contains "background pipeline prints job id" "[1]" "$OUTPUT"

# Test 6: Zombie check - no defunct processes remain
ps -ef | grep "$SHELL_BIN" | grep -v grep | grep defunct && HAS_ZOMBIES="true" || HAS_ZOMBIES="false"
assert_eq "no defunct zombie processes left" "false" "$HAS_ZOMBIES"

echo "========================================================================="
echo "Phase 5 Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
