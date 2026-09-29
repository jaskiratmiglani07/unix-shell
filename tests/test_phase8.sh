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

echo "=== Running Phase 8 Test Suite for AegisShell (Command History) ==="

# Use a temp history file to avoid polluting user's history
export HISTFILE=$(mktemp /tmp/aegissh_history_XXXXXX)
trap "rm -f $HISTFILE" EXIT

# Test 1: history command shows executed commands
OUTPUT=$(printf "echo hello\necho world\nhistory\nexit\n" | HISTFILE=$HISTFILE $SHELL_BIN | tr -d '\r')
assert_contains "history shows commands" "echo hello" "$OUTPUT"
assert_contains "history shows commands" "echo world" "$OUTPUT"
assert_contains "history shows numbered entries" " 1  " "$OUTPUT"
assert_contains "history shows numbered entries" " 2  " "$OUTPUT"

# Test 2: history persists across sessions
# First session
printf "echo persistent\nexit\n" | HISTFILE=$HISTFILE $SHELL_BIN > /dev/null
# Second session
OUTPUT=$(printf "history\nexit\n" | HISTFILE=$HISTFILE $SHELL_BIN | tr -d '\r')
assert_contains "history persists across sessions" "echo persistent" "$OUTPUT"

# Test 3: history doesn't show the history command itself
OUTPUT=$(printf "echo test\nhistory\nexit\n" | HISTFILE=$HISTFILE $SHELL_BIN | tr -d '\r')
# Should not contain "history" command in history output
if [[ "$OUTPUT" == *"history"* ]]; then
    # Check if it's the history builtin or the history output
    # The history output will show the previous commands but not "history" itself
    if [[ "$OUTPUT" == *" 3  history"* ]]; then
        echo "  [FAIL] history command appears in history"
        FAILED=$((FAILED + 1))
    else
        echo "  [PASS] history command not recorded"
        PASSED=$((PASSED + 1))
    fi
else
    echo "  [PASS] history command not recorded"
    PASSED=$((PASSED + 1))
fi

# Test 4: empty history
rm -f "$HISTFILE"
OUTPUT=$(printf "history\nexit\n" | HISTFILE=$HISTFILE $SHELL_BIN | tr -d '\r')
assert_eq "empty history produces no output" "" "$OUTPUT"

# Test 5: history with pipeline commands
OUTPUT=$(printf "echo a | cat\necho b | grep b\nhistory\nexit\n" | HISTFILE=$HISTFILE $SHELL_BIN | tr -d '\r')
assert_contains "pipeline commands in history" "echo a | cat" "$OUTPUT"
assert_contains "pipeline commands in history" "echo b | grep b" "$OUTPUT"

echo "================================================================="
echo "Phase 8 Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi