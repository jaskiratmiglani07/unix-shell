#!/usr/bin/env bash
# test_phase6.sh - Signal handling tests for AegisShell
# Note: Uses fixed fd numbers (5,6) for FIFO write ends (bash 3.2 compat)

SHELL_BIN="./bin/aegissh"
PASSED=0
FAILED=0

assert_eq() {
    local test_name="$1" expected="$2" actual="$3"
    if [ "$expected" = "$actual" ]; then
        echo "  [PASS] $test_name"; PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $test_name"
        echo "    Expected: '$expected'"
        echo "    Actual:   '$actual'"
        FAILED=$((FAILED + 1))
    fi
}

assert_contains() {
    local test_name="$1" expected_sub="$2" actual="$3"
    if [[ "$actual" == *"$expected_sub"* ]]; then
        echo "  [PASS] $test_name"; PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $test_name"
        echo "    Expected to contain: '$expected_sub'"
        echo "    Actual:             '$actual'"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Running Phase 6 Test Suite for AegisShell (Signals) ==="

# ---------------------------------------------------------------
# Tests 1-3: Shell-level signal ignoring
# Strategy: FIFO with write end held open on fd 5 so the shell
#           never receives EOF while we test signal delivery.
# ---------------------------------------------------------------
FIFO1=$(mktemp -u /tmp/aegissh_sig_XXXXXX)
OUT1=$(mktemp /tmp/aegissh_out_XXXXXX)
mkfifo "$FIFO1"

# Launch shell in background (it will block in open(FIFO1,O_RDONLY) until
# we open the write end below)
$SHELL_BIN <"$FIFO1" >"$OUT1" 2>&1 &
SHELL_PID=$!

# Open write end on fd 5 — this unblocks the shell's FIFO open
exec 5>"$FIFO1"

sleep 0.2   # Let the REPL spin up

# Test 1: Shell survives SIGINT
kill -INT "$SHELL_PID" 2>/dev/null || true
sleep 0.15
if kill -0 "$SHELL_PID" 2>/dev/null; then
    echo "  [PASS] shell survives SIGINT"; PASSED=$((PASSED + 1))
else
    echo "  [FAIL] shell survives SIGINT (process exited)"; FAILED=$((FAILED + 1))
fi

# Test 2: Shell ignores SIGQUIT
kill -QUIT "$SHELL_PID" 2>/dev/null || true
sleep 0.15
if kill -0 "$SHELL_PID" 2>/dev/null; then
    echo "  [PASS] shell ignores SIGQUIT"; PASSED=$((PASSED + 1))
else
    echo "  [FAIL] shell ignores SIGQUIT (process exited)"; FAILED=$((FAILED + 1))
fi

# Test 3: Shell ignores SIGTSTP
kill -TSTP "$SHELL_PID" 2>/dev/null || true
sleep 0.15
if kill -0 "$SHELL_PID" 2>/dev/null; then
    echo "  [PASS] shell ignores SIGTSTP"; PASSED=$((PASSED + 1))
else
    echo "  [FAIL] shell ignores SIGTSTP (process exited)"; FAILED=$((FAILED + 1))
fi

# Test 4: Shell still functional after signals (output check)
echo "echo sig_check_ok" >&5
sleep 0.2
assert_contains "shell still executes commands after signals" "sig_check_ok" "$(cat "$OUT1")"

# Teardown shell 1
echo "exit" >&5
exec 5>&-
wait "$SHELL_PID" 2>/dev/null || true
rm -f "$FIFO1" "$OUT1"

# ---------------------------------------------------------------
# Test 5: Child process is terminated by SIGINT
# ---------------------------------------------------------------
FIFO2=$(mktemp -u /tmp/aegissh_child_XXXXXX)
OUT2=$(mktemp /tmp/aegissh_child_out_XXXXXX)
mkfifo "$FIFO2"

$SHELL_BIN <"$FIFO2" >"$OUT2" 2>&1 &
SHELL_PID2=$!
exec 6>"$FIFO2"

sleep 0.2

# Start a long-running child via the shell
echo "sleep 10" >&6
sleep 0.4

CHILD_PID=$(pgrep -P "$SHELL_PID2" sleep 2>/dev/null || true)

CHILD_FOUND="false"
if [ -n "$CHILD_PID" ]; then
    CHILD_FOUND="true"
    kill -INT "$CHILD_PID" 2>/dev/null || true
    sleep 0.2
    if kill -0 "$CHILD_PID" 2>/dev/null; then
        assert_eq "child terminated by SIGINT" "dead" "alive"
    else
        echo "  [PASS] child terminated by SIGINT"; PASSED=$((PASSED + 1))
    fi
else
    echo "  [FAIL] child process (sleep) not found via pgrep"; FAILED=$((FAILED + 1))
fi

echo "exit" >&6
exec 6>&-
wait "$SHELL_PID2" 2>/dev/null || true
rm -f "$FIFO2" "$OUT2"

echo "==========================================================="
echo "Phase 6 Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
