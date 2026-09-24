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

echo "=== Running Phase 2 Test Suite for AegisShell ==="

# Test 1: echo basic
OUTPUT=$(printf "echo hello from aegissh\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "echo basic" "hello from aegissh" "$OUTPUT"

# Test 2: echo -n (no newline)
OUTPUT=$(printf "echo -n no-newline\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "echo -n flag" "no-newline" "$OUTPUT"

# Test 3: cd to explicit path
TMP_TEST_DIR=$(mktemp -d /tmp/aegissh_test_XXXXXX)
# Resolve symlinks in case macOS /tmp -> /private/tmp
RESOLVED_TMP_TEST_DIR=$(cd "$TMP_TEST_DIR" && pwd -P)
OUTPUT=$(printf "cd $TMP_TEST_DIR\npwd\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "cd explicit directory" "$RESOLVED_TMP_TEST_DIR" "$OUTPUT"

# Test 4: cd with no args (goes to HOME)
RESOLVED_HOME=$(cd "$HOME" && pwd -P)
OUTPUT=$(printf "cd $TMP_TEST_DIR\ncd\npwd\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "cd to HOME default" "$RESOLVED_HOME" "$OUTPUT"

# Test 5: cd - (back to OLDPWD)
OUTPUT=$(printf "cd $TMP_TEST_DIR\ncd\ncd -\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "cd - switches back" "$RESOLVED_TMP_TEST_DIR" "$OUTPUT"

# Test 6: cd nonexistent directory
set +e
ERR_OUT=$(printf "cd /path/that/definitely/does/not/exist_12345\nexit\n" | $SHELL_BIN 2>&1 > /dev/null)
STATUS=$?
set -e
assert_eq "cd nonexistent failure status" "1" "$STATUS"

# Test 7: export and env
OUTPUT=$(printf "export AEGIS_TEST_VAR=systems_project_ok\nenv\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_contains "export variable present in env" "AEGIS_TEST_VAR=systems_project_ok" "$OUTPUT"

# Test 8: unset removes variable
OUTPUT=$(printf "export AEGIS_UNSET_ME=temporary\nunset AEGIS_UNSET_ME\nenv\nexit\n" | $SHELL_BIN | tr -d '\r')
if [[ "$OUTPUT" == *"AEGIS_UNSET_ME"* ]]; then
    echo "  [FAIL] unset failed to remove variable"
    FAILED=$((FAILED + 1))
else
    echo "  [PASS] unset successfully removed variable"
    PASSED=$((PASSED + 1))
fi

# Test 9: export invalid identifier
set +e
ERR_OUT=$(printf "export 123invalid=val\nexit\n" | $SHELL_BIN 2>&1 > /dev/null)
STATUS=$?
set -e
assert_eq "export invalid identifier status" "1" "$STATUS"

# Test 10: unset invalid identifier
set +e
ERR_OUT=$(printf "unset 99invalid\nexit\n" | $SHELL_BIN 2>&1 > /dev/null)
STATUS=$?
set -e
assert_eq "unset invalid identifier status" "1" "$STATUS"

# Test 11: PATH handling (custom binary in new PATH)
BIN_TEST_DIR=$(mktemp -d /tmp/aegissh_bin_XXXXXX)
cat << 'EOF' > "$BIN_TEST_DIR/custom_script"
#!/bin/sh
echo "CUSTOM_BIN_CALLED"
EOF
chmod +x "$BIN_TEST_DIR/custom_script"

OUTPUT=$(printf "export PATH=$BIN_TEST_DIR:$PATH\ncustom_script\nexit\n" | $SHELL_BIN | tr -d '\r')
assert_eq "dynamic PATH lookup of new binary" "CUSTOM_BIN_CALLED" "$OUTPUT"

# Clean up temporary test directories
rm -rf "$TMP_TEST_DIR" "$BIN_TEST_DIR"

echo "==============================================="
echo "Phase 2 Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
