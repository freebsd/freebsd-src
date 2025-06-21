#!/bin/sh

# ldconfig_test.sh - Tests for ldconfig -S functionality

# Exit immediately if a command exits with a non-zero status.
set -e

# Configuration
LDCONFIG_CMD="../../ldconfig" # Corrected relative path
TEST_DIR_BASENAME="ldconfig_S_test_run"
TEST_COUNT=0
FAIL_COUNT=0

# Helper functions for testing
assert_grep() {
    local pattern="$1"
    local input="$2"
    local test_desc="$3"
    TEST_COUNT=$((TEST_COUNT + 1))

    echo "ASSERT GREP: $test_desc"
    echo "Pattern: $pattern"
    # echo "Input for grep:"
    # echo "$input"
    if echo "$input" | grep -qE -- "$pattern"; then
        echo "PASS: Found pattern."
    else
        echo "FAIL: Did not find pattern."
        echo "--- Full Input ---"
        echo "$input"
        echo "--- End Full Input ---"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

assert_not_grep() {
    local pattern="$1"
    local input="$2"
    local test_desc="$3"
    TEST_COUNT=$((TEST_COUNT + 1))

    echo "ASSERT NOT GREP: $test_desc"
    echo "Pattern: $pattern"
    if echo "$input" | grep -qE -- "$pattern"; then
        echo "FAIL: Unexpectedly found pattern."
        echo "--- Full Input ---"
        echo "$input"
        echo "--- End Full Input ---"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    else
        echo "PASS: Did not find pattern (as expected)."
    fi
}

# Create a temporary directory for testing
TEST_WORK_DIR=$(mktemp -d -t "${TEST_DIR_BASENAME}.XXXXXX")
if [ -z "$TEST_WORK_DIR" ]; then
    echo "Failed to create temporary working directory."
    exit 1
fi
echo "Test working directory: $TEST_WORK_DIR"

# Cleanup function to remove the temporary directory on exit
cleanup() {
    if [ -n "$TEST_WORK_DIR" ] && [ -d "$TEST_WORK_DIR" ]; then
        echo "Cleaning up $TEST_WORK_DIR"
        rm -rf "$TEST_WORK_DIR"
    fi
}
trap cleanup EXIT INT TERM

# --- Test Setup ---
cd "$TEST_WORK_DIR"

# Create dummy library directories and files
mkdir dir1 dir2 dir_empty
touch dir1/libone.so.1
touch dir1/libtwo.so.1.2.3
touch dir2/libthree.so.3

# Get absolute paths for hints and validation
ABS_DIR1=$(pwd)/dir1
ABS_DIR2=$(pwd)/dir2
ABS_DIR_EMPTY=$(pwd)/dir_empty

# Create a temporary hints file (ldconfig will process this relative to its own CWD if not absolute)
# For ldconfig -f, it takes this file as the hints file to operate on.
# The paths *inside* this temp_conf_file should be the directories ldconfig needs to scan.
TEMP_CONF_FILE="ldconfig.conf.temp"
echo "$ABS_DIR1" > "$TEMP_CONF_FILE"
echo "$ABS_DIR2" >> "$TEMP_CONF_FILE"
echo "$ABS_DIR_EMPTY" >> "$TEMP_CONF_FILE"

TEMP_HINTS_FILE="hints.temp"

echo "Temporary configuration file ($TEMP_CONF_FILE) created with content:"
cat "$TEMP_CONF_FILE"

echo "Running ldconfig to generate initial hints file ($TEMP_HINTS_FILE)..."
# We pass the *directories* to ldconfig, not the conf file with -f for generation.
# -f is used to specify *which* hints file to update/read.
# The command line arguments are directories to scan.
# If we want to control exactly what goes into the hints dirlist without
# relying on command-line args for paths, we'd need a way to pre-populate the hints.
# For this test, we'll have ldconfig create the hints file based on scanning our test dirs.
# The paths listed in the conf file are what ldconfig will scan if it were reading a conf.
# Here, we're building the hints file directly from specified dirs.
$LDCONFIG_CMD -f "$TEMP_HINTS_FILE" "$ABS_DIR1" "$ABS_DIR2" "$ABS_DIR_EMPTY"
echo "Initial hints file generated."

# --- Test Cases ---

# Test Case 1: ldconfig -r -S output
echo "\n--- Test Case 1: ldconfig -r -S output ---"
output_rs=$($LDCONFIG_CMD -r -S -f "$TEMP_HINTS_FILE")
echo "Output for ldconfig -rS:"
echo "$output_rs"

# Validations for Test Case 1
# Note: The library index (e.g., 0:, 1:) might vary depending on processing order.
# We grep for the library name and its (from ...) part.
# The path in "=> /path/to/lib" is also absolute.
# ldconfig -r lists libraries found by scanning the directories in the hints file.
# The "=> path" is constructed by ldconfig itself.
# The "(from path)" is the new part, where "path" is the directory from the hints file.

assert_grep "libone\.so\.1 => $ABS_DIR1/libone\.so\.1 (from $ABS_DIR1)" "$output_rs" "libone.so.1 shows source dir1"
assert_grep "libtwo\.so\.1\.2\.3 => $ABS_DIR1/libtwo\.so\.1\.2\.3 (from $ABS_DIR1)" "$output_rs" "libtwo.so.1.2.3 shows source dir1"
assert_grep "libthree\.so\.3 => $ABS_DIR2/libthree\.so\.3 (from $ABS_DIR2)" "$output_rs" "libthree.so.3 shows source dir2"

# Check that dir_empty is listed in search paths but has no libs under it
# The header for dir_empty should be present.
# The search directories line should contain ABS_DIR_EMPTY
assert_grep "search directories:.*$ABS_DIR_EMPTY" "$output_rs" "dir_empty in search directories list"
# Check that no libraries are listed immediately under a dir_empty section if it were to print one (it usually doesn't print headers for empty dirs for libs)
# This is harder to assert directly, absence of positive matches for dir_empty libs is implicit.


# Test Case 2: ldconfig -r (without -S) output
echo "\n--- Test Case 2: ldconfig -r (without -S) output ---"
output_r=$($LDCONFIG_CMD -r -f "$TEMP_HINTS_FILE")
echo "Output for ldconfig -r:"
echo "$output_r"

# Validations for Test Case 2
assert_not_grep "(from $ABS_DIR1)" "$output_r" "libone.so.1 does not show source dir1"
assert_not_grep "(from $ABS_DIR2)" "$output_r" "libthree.so.3 does not show source dir2"
# Check that the base information is still there
assert_grep "libone\.so\.1 => $ABS_DIR1/libone\.so\.1" "$output_r" "libone.so.1 basic info present"
assert_grep "libthree\.so\.3 => $ABS_DIR2/libthree\.so\.3" "$output_r" "libthree.so.3 basic info present"


# Test Case 3: ldconfig -S (without -r) warning
echo "\n--- Test Case 3: ldconfig -S (without -r) warning ---"
# Redirect stderr to stdout to capture it
output_S_stderr=$($LDCONFIG_CMD -S -f "$TEMP_HINTS_FILE" 2>&1) || true # Allow non-zero exit if ldconfig exits due to misuse
echo "Output for ldconfig -S (stderr normally):"
echo "$output_S_stderr"

# Validations for Test Case 3
assert_grep "ldconfig: -S is only effective with -r" "$output_S_stderr" "Warning message for -S without -r"


# --- Test Summary ---
cd .. # Go back to original directory before cleanup (important for relative LDCONFIG_CMD path if used differently later)
echo "\n--- Summary ---"
if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "All $TEST_COUNT tests passed."
    exit 0
else
    echo "$FAIL_COUNT out of $TEST_COUNT tests failed."
    exit 1
fi
