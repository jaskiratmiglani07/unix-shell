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

echo "=== Running Phase 9 Test Suite for AegisShell (Job Control) ==="

# Test 1: jobs command shows background jobs
OUTPUT=$(printf "sleep 1 &\njobs\nexit\n" | $SHELL_BIN 2>&1 | tr -d '\r')
assert_contains "jobs shows background job" "Running" "$OUTPUT"
assert_contains "jobs shows background job" "sleep 1" "$OUTPUT"

# Test 2: fg brings job to foreground
OUTPUT=$(printf "sleep 2 &\nfg\nexit\n" | $SHELL_BIN 2>&1 | tr -d '\r')
assert_contains "fg brings job to foreground" "sleep 2" "$OUTPUT"

# Test 3: bg continues stopped job
# This is harder to test without interactive terminal, skip for now

# Test 4: Ctrl+Z stops foreground job (requires interactive terminal, skip)

# Test 5: jobs builtin lists all jobs
OUTPUT=$(printf "sleep 1 &\nsleep 2 &\njobs\nexit\n" | $SHELL_BIN 2>&1 | tr -d '\r')
assert_contains "jobs shows multiple jobs" "sleep 1" "$OUTPUT"
assert_contains "jobs shows multiple jobs" "sleep 2" "$OUTPUT"

# Test 6: fg with job number
OUTPUT=$(printf 'sleep 2 &\nsleep 3 &\nfg %%1\nexit\n' | $SHELL_BIN 2>&1 | tr -d '\r')
assert_contains "fg with job number" "sleep 2" "$OUTPUT"

echo "================================================================="
echo "Phase 9 Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi