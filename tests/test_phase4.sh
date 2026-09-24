#!/usr/bin/env bash
set -euo pipefail

SHELL_BIN="./bin/aegissh"
PASSED=0
FAILED=0
TMP_DIR=$(mktemp -d /tmp/aegissh_pipe_XXXXXX)

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

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

echo "=== Running Phase 4 Test Suite for AegisShell (Pipelines) ==="

# Test 1: Simple 2-stage pipeline
OUTPUT=$(printf "echo hello world | grep world\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "2-stage pipeline echo | grep" "hello world" "$OUTPUT"

# Test 2: 3-stage pipeline
OUTPUT=$(printf "seq 1 5 | grep -v 3 | wc -l\nexit\n" | $SHELL_BIN | tr -d ' \r\n')
assert_eq "3-stage pipeline seq | grep | wc" "4" "$OUTPUT"

# Test 3: Multi-stage pipeline with builtins
OUTPUT=$(printf "echo test_aegis_flow | cat | tr a-z A-Z\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "multi-stage pipeline with builtin echo" "TEST_AEGIS_FLOW" "$OUTPUT"

# Test 4: Pipeline combined with redirection
echo "pipeline source data" > "$TMP_DIR/source.txt"
printf "cat < $TMP_DIR/source.txt | tr a-z A-Z > $TMP_DIR/dest.txt\nexit\n" | $SHELL_BIN > /dev/null
RESULT=$(cat "$TMP_DIR/dest.txt" | tr -d '\r')
assert_eq "pipeline with input and output redirection" "PIPELINE SOURCE DATA" "$RESULT"

# Test 5: Pipeline throughput stress test (10,000 lines through 4 stages)
OUTPUT=$(printf "seq 1 10000 | cat | cat | wc -l\nexit\n" | $SHELL_BIN | tr -d ' \r\n')
assert_eq "pipeline high throughput (10k items)" "10000" "$OUTPUT"

# Test 6: Exit status of pipeline corresponds to the LAST command
set +e
printf "echo hello | false\nexit\n" | $SHELL_BIN > /dev/null 2>&1
STATUS=$?
set -e
assert_eq "pipeline status reflects last stage failure" "1" "$STATUS"

set +e
printf "false | echo hello\nexit\n" | $SHELL_BIN > /dev/null 2>&1
STATUS=$?
set -e
assert_eq "pipeline status reflects last stage success" "0" "$STATUS"

# Test 7: Syntax error on empty stage
set +e
ERR_OUT=$(printf "| ls\nexit\n" | $SHELL_BIN 2>&1 > /dev/null)
set -e
assert_eq "syntax error on leading pipe" "true" "$([[ "$ERR_OUT" == *"syntax error"* ]] && echo true || echo false)"

echo "============================================================="
echo "Phase 4 Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
