#!/usr/bin/env bash
set -euo pipefail

SHELL_BIN="./bin/aegissh"
PASSED=0
FAILED=0
TMP_DIR=$(mktemp -d /tmp/aegissh_redir_XXXXXX)

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

echo "=== Running Phase 3 Test Suite for AegisShell (Redirection) ==="

# Test 1: Output truncation (>)
printf "echo 'hello truncation' > $TMP_DIR/out1.txt\nexit\n" | $SHELL_BIN > /dev/null
CONTENT=$(cat "$TMP_DIR/out1.txt")
assert_eq "stdout redirection >" "'hello truncation'" "$CONTENT"

# Overwrite out1.txt to verify truncation
printf "echo 'second line' > $TMP_DIR/out1.txt\nexit\n" | $SHELL_BIN > /dev/null
CONTENT=$(cat "$TMP_DIR/out1.txt")
assert_eq "stdout truncation overwrites file" "'second line'" "$CONTENT"

# Test 2: Output append (>>)
printf "echo 'line 1' > $TMP_DIR/out2.txt\necho 'line 2' >> $TMP_DIR/out2.txt\nexit\n" | $SHELL_BIN > /dev/null
CONTENT=$(cat "$TMP_DIR/out2.txt")
EXPECTED=$(printf "'line 1'\n'line 2'")
assert_eq "stdout append >>" "$EXPECTED" "$CONTENT"

# Test 3: Input redirection (<)
echo "sample input line" > "$TMP_DIR/in1.txt"
OUTPUT=$(printf "cat < $TMP_DIR/in1.txt\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "stdin redirection <" "sample input line" "$OUTPUT"

# Test 4: Stderr redirection (2>)
# Triggering an external command that writes to stderr
set +e
printf "ls /path/definitely/does_not_exist_for_sure 2> $TMP_DIR/err1.txt\nexit\n" | $SHELL_BIN > /dev/null 2>&1
set -e
assert_eq "stderr redirection 2> created file" "true" "$([ -s "$TMP_DIR/err1.txt" ] && echo true || echo false)"

# Test 5: Stderr append redirection (2>>)
set +e
printf "ls /fake_path_1 2> $TMP_DIR/err2.txt\nls /fake_path_2 2>> $TMP_DIR/err2.txt\nexit\n" | $SHELL_BIN > /dev/null 2>&1
set -e
LINE_COUNT=$(wc -l < "$TMP_DIR/err2.txt" | tr -d ' ')
assert_eq "stderr append 2>> has multiple lines" "2" "$LINE_COUNT"

# Test 6: Builtin redirection (echo and pwd into file)
printf "pwd > $TMP_DIR/pwd.txt\nexit\n" | $SHELL_BIN > /dev/null
CONTENT=$(cat "$TMP_DIR/pwd.txt")
EXPECTED_PWD="$(pwd)"
assert_eq "builtin pwd redirection" "$EXPECTED_PWD" "$CONTENT"

# Test 7: Combined stdout and stderr redirection
printf "echo stdout_data > $TMP_DIR/stdout.txt 2> $TMP_DIR/stderr.txt\nexit\n" | $SHELL_BIN > /dev/null
assert_eq "combined stdout written" "stdout_data" "$(cat "$TMP_DIR/stdout.txt")"
assert_eq "combined stderr empty" "0" "$(wc -c < "$TMP_DIR/stderr.txt" | tr -d ' ')"

# Test 8: Nonexistent input file error handling
set +e
ERR_OUT=$(printf "cat < $TMP_DIR/nonexistent_file_9999.txt\nexit\n" | $SHELL_BIN 2>&1 > /dev/null)
STATUS=$?
set -e
assert_eq "input redirection error returns non-zero" "1" "$STATUS"

echo "================================================================"
echo "Phase 3 Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
