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

echo "=== Running Phase 1 Test Suite for AegisShell ==="

# Test 1: pwd builtin
OUTPUT=$(printf "pwd\nexit\n" | $SHELL_BIN | tr -d '\r')
EXPECTED_PWD="$(pwd)"
assert_eq "pwd builtin output" "$EXPECTED_PWD" "$OUTPUT"

# Test 2: exit with code 0
set +e
printf "exit 0\n" | $SHELL_BIN > /dev/null 2>&1
STATUS=$?
set -e
assert_eq "exit status 0" "0" "$STATUS"

# Test 3: exit with specific code
set +e
printf "exit 42\n" | $SHELL_BIN > /dev/null 2>&1
STATUS=$?
set -e
assert_eq "exit status 42" "42" "$STATUS"

# Test 4: exit with invalid numeric arg
set +e
ERR_OUT=$(printf "exit notanumber\n" | $SHELL_BIN 2>&1 > /dev/null)
STATUS=$?
set -e
assert_eq "exit invalid arg status" "2" "$STATUS"

# Test 5: external command execution
OUTPUT=$(printf "echo hello world\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "external echo execution" "hello world" "$OUTPUT"

# Test 6: external command with flags
OUTPUT=$(printf "uname -s\nexit\n" | $SHELL_BIN | tr -d '\r')
EXPECTED_UNAME="$(uname -s)"
assert_eq "external uname execution" "$EXPECTED_UNAME" "$OUTPUT"

# Test 7: non-existent command
set +e
ERR_OUT=$(printf "foobar_command_does_not_exist_987\nexit\n" | $SHELL_BIN 2>&1 > /dev/null)
STATUS=$?
set -e
assert_eq "command not found error status" "127" "$STATUS"

# Test 8: comments and empty lines
OUTPUT=$(printf "\n# this is a comment\n    \necho alive\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "comments and empty lines handling" "alive" "$OUTPUT"

echo "==============================================="
echo "Phase 1 Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
