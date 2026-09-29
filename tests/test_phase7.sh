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

echo "=== Running Phase 7 Test Suite for AegisShell (Quoting & Expansion) ==="

# Test 1: Single quotes - literal, no expansion
OUTPUT=$(printf "echo 'hello \$HOME world'\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "single quotes prevent expansion" "hello \$HOME world" "$OUTPUT"

# Test 2: Double quotes - expansion happens
export AEGIS_TEST_VAR="double_quoted_value"
OUTPUT=$(printf "echo \"hello \$AEGIS_TEST_VAR world\"\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "double quotes allow expansion" "hello double_quoted_value world" "$OUTPUT"

# Test 3: Unquoted - expansion happens
OUTPUT=$(printf "echo hello \$AEGIS_TEST_VAR world\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "unquoted allows expansion" "hello double_quoted_value world" "$OUTPUT"

# Test 4: Mixed quoting
OUTPUT=$(printf "echo 'single' \"double \$AEGIS_TEST_VAR\" unquoted\$AEGIS_TEST_VAR\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "mixed quoting" "single double double_quoted_value unquoteddouble_quoted_value" "$OUTPUT"

# Test 5: Empty variable
unset EMPTY_VAR
OUTPUT=$(printf "echo 'start'\$EMPTY_VAR'end'\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "empty variable expands to nothing" "startend" "$OUTPUT"

# Test 6: Variable with braces ${VAR}
OUTPUT=$(printf "echo \${AEGIS_TEST_VAR}_suffix\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "brace expansion" "double_quoted_value_suffix" "$OUTPUT"

# Test 7: $? expansion (last exit status)
OUTPUT=$(printf "false\necho \$?\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "exit status expansion" "1" "$OUTPUT"

OUTPUT=$(printf "true\necho \$?\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "exit status expansion 0" "0" "$OUTPUT"

# Test 8: Escaped characters
OUTPUT=$(printf "echo hello\\ world\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "escaped space" "hello world" "$OUTPUT"

OUTPUT=$(printf "echo \\\$HOME\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "escaped dollar" "\$HOME" "$OUTPUT"

# Test 9: Quotes in redirection filenames (using exported variable)
TMP_FILE=$(mktemp /tmp/aegissh_test_XXXXXX)
export TMP_FILE
printf "echo hello > \"\$TMP_FILE\"\nexit\n" | $SHELL_BIN > /dev/null
CONTENT=$(cat "$TMP_FILE")
assert_eq "quoted redirection filename" "hello" "$CONTENT"
rm -f "$TMP_FILE"

# Test 10: Single quotes preserve everything including backslashes
OUTPUT=$(printf 'echo '\''hello\\\\world'\''\nexit\n' | $SHELL_BIN | tr -d '\r')
assert_eq "single quotes preserve backslashes" "hello\\\\world" "$OUTPUT"

# Test 11: Double quotes handle backslash escaping
OUTPUT=$(printf 'echo "hello\\\\world"\nexit\n' | $SHELL_BIN | tr -d '\r')
assert_eq "double quotes escape backslash" "hello\\world" "$OUTPUT"

OUTPUT=$(printf 'echo "hello\\$AEGIS_TEST_VAR"\nexit\n' | $SHELL_BIN | tr -d '\r')
assert_eq "double quotes escape dollar" "hello\$AEGIS_TEST_VAR" "$OUTPUT"

# Test 12: Command substitution not supported yet - just verify $() is treated literally
OUTPUT=$(printf 'echo $(date)\nexit\n' | $SHELL_BIN | tr -d '\r')
# Should treat $(date) as literal since command substitution not implemented
assert_contains "dollar-paren treated literally" '$(date)' "$OUTPUT"

# Test 13: Multiple spaces preserved in quotes
OUTPUT=$(printf "echo \"hello    world\"\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "multiple spaces in double quotes" "hello    world" "$OUTPUT"

OUTPUT=$(printf "echo 'hello    world'\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "multiple spaces in single quotes" "hello    world" "$OUTPUT"

# Test 14: Unquoted multiple spaces collapsed
OUTPUT=$(printf "echo hello    world\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "unquoted multiple spaces collapsed" "hello world" "$OUTPUT"

echo "================================================================="
echo "Phase 7 Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi