#!/bin/sh

# ls_json_test.sh - Tests for ls -j (JSON output)

# Exit immediately if a command exits with a non-zero status.
set -e

# Check for jq
if ! command -v jq >/dev/null 2>&1; then
	echo "jq is not installed. Skipping JSON tests."
	exit 0 # Exit successfully as per instruction for missing test dependencies
fi

# Test script variables
LS_CMD="../../ls" # Path to the ls command to be tested
TEST_DIR_BASENAME="ls_json_test_run"
TEST_COUNT=0
FAIL_COUNT=0

# Create a temporary directory for testing
TEST_DIR=$(mktemp -d -t "${TEST_DIR_BASENAME}.XXXXXX")
if [ -z "$TEST_DIR" ]; then
    echo "Failed to create temporary directory."
    exit 1 # Critical error, cannot proceed
fi

# Cleanup function to remove the temporary directory on exit
cleanup() {
    if [ -n "$TEST_DIR" ] && [ -d "$TEST_DIR" ]; then
        rm -rf "$TEST_DIR"
    fi
}
trap cleanup EXIT INT TERM

# Function to run a test
# Usage: run_test "test_name" "command_to_run" "jq_filter_for_validation" "expected_jq_output"
run_test() {
    TEST_COUNT=$((TEST_COUNT + 1))
    local test_name="$1"
    local command="$2"
    local jq_filter="$3"
    local expected_output="$4"
    local actual_output
    local result

    echo "Running test: $test_name"

    # Execute the command and capture its output
    actual_output=$(eval "$command")

    # Validate JSON structure first (must be valid JSON)
    if ! echo "$actual_output" | jq . > /dev/null 2>&1; then
        echo "FAIL: $test_name - Output is not valid JSON."
        echo "Command: $command"
        echo "Actual Output:"
        echo "$actual_output"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return
    fi

    # Apply jq filter and compare
    result=$(echo "$actual_output" | jq -c "$jq_filter") # -c for compact output

    if [ "$result" = "$expected_output" ]; then
        echo "PASS: $test_name"
    else
        echo "FAIL: $test_name"
        echo "Command: $command"
        echo "Expected (jq -c '$jq_filter'): $expected_output"
        echo "Actual (jq -c '$jq_filter'): $result"
        echo "Full ls output:"
        echo "$actual_output"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

# --- Test Cases Start Here ---

cd "$TEST_DIR"

# Test Case 1: Basic File Types and Attributes
test_basic_types() {
    echo "\n--- Test Case: Basic File Types and Attributes ---"
    touch testfile.txt
    echo "content" > testfile.txt
    mkdir testdir
    ln -s testfile.txt testlink
    ln -s nonexistent brokenlink
    mkfifo testfifo

    # Test 1.1: ls -j on specific items
    # Validate names and types (simplified for initial setup)
    # Note: Inode, times, etc. will vary. Focus on stable fields first.
    # Order of output from ls can vary, so select by name.
    run_test "basic_types_testfile" \
             "$LS_CMD -j testfile.txt" \
             '.[0].name == "testfile.txt" and .[0].type == "file" and .[0].size == 8' \
             "true" # 8 bytes for "content\n"

    run_test "basic_types_testdir" \
             "$LS_CMD -j testdir" \
             '.[0].name == "testdir" and .[0].type == "directory"' \
             "true"

    run_test "basic_types_testlink" \
             "$LS_CMD -j testlink" \
             '.[0].name == "testlink" and .[0].type == "link" and .[0].target == "testfile.txt"' \
             "true"

    run_test "basic_types_brokenlink" \
             "$LS_CMD -j brokenlink" \
             '.[0].name == "brokenlink" and .[0].type == "symlink" and .[0].target == "nonexistent"' \
             "true" # type is 'symlink' (FTS_SL) because target does not exist

    run_test "basic_types_testfifo" \
             "$LS_CMD -j testfifo" \
             '.[0].name == "testfifo" and .[0].type == "fifo"' \
             "true"

    # Test 1.2: ls -j on current directory
    # Check for multiple entries and their types
    local ls_output
    ls_output=$($LS_CMD -j .)

    run_test "basic_types_current_dir_testfile_type" \
             "echo '$ls_output'" \
             '.[] | select(.name == "testfile.txt") | .type' \
             '"file"'
    run_test "basic_types_current_dir_testdir_type" \
             "echo '$ls_output'" \
             '.[] | select(.name == "testdir") | .type' \
             '"directory"'
    run_test "basic_types_current_dir_testlink_type_target" \
             "echo '$ls_output'" \
             '.[] | select(.name == "testlink") | .type == "link" and .target == "testfile.txt"' \
             "true" # target only for -l or when it's a link argument
    run_test "basic_types_current_dir_brokenlink_type_target" \
             "echo '$ls_output'" \
             '.[] | select(.name == "brokenlink") | .type == "link" and .target == "nonexistent"' \
             "true" # target only for -l or when it's a link argument
    run_test "basic_types_current_dir_testfifo_type" \
             "echo '$ls_output'" \
             '.[] | select(.name == "testfifo") | .type' \
             '"fifo"'

    # Test 1.3: ls -j (previously -jl) on current directory (target is always included for links)
    # ls_output is already from $LS_CMD -j .
    run_test "basic_types_current_dir_testlink_target" \
         "echo '$ls_output'" \
         '.[] | select(.name == "testlink") | .target' \
         '"testfile.txt"'
    run_test "basic_types_current_dir_brokenlink_target" \
         "echo '$ls_output'" \
         '.[] | select(.name == "brokenlink") | .target' \
         '"nonexistent"'

    # Test 1.4: ls -jA (show dotfiles)
    echo "content" > .hiddenfile
    # Check if .hiddenfile is present, and testfile.txt is still there
    # Note: '.' and '..' are not listed by -A by default in fts, unless FTS_SEEDOT is also set.
    # ls -A implies f_listdot = 1. Our ls -A implementation sets f_listdot.
    # We expect .hiddenfile, but not '.' or '..' unless explicitly asked for with -a.
    local ls_output_A
    ls_output_A=$($LS_CMD -jA .)

    run_test "basic_types_dotfile_A_hiddenfile_present" \
        "echo '$ls_output_A'" \
        '.[] | select(.name == ".hiddenfile") | .type' \
        '"file"'
    run_test "basic_types_dotfile_A_testfile_present" \
        "echo '$ls_output_A'" \
        '.[] | select(.name == "testfile.txt") | .type' \
        '"file"'
    run_test "basic_types_dotfile_A_dot_absent" \
        "echo '$ls_output_A'" \
        '.[] | select(.name == ".") | length' \
        "0" # Expecting empty result, so length 0
    run_test "basic_types_dotfile_A_dotdot_absent" \
        "echo '$ls_output_A'" \
        '.[] | select(.name == "..") | length' \
        "0" # Expecting empty result, so length 0

    # The 'error' field at the top level is for when fts_statp itself is NULL,
    # not for broken link targets. The type "symlink" (FTS_SL) indicates a problematic link target.
}

# Test Case 2: Special Characters in Filenames
test_special_chars() {
    echo "\n--- Test Case: Special Characters in Filenames ---"
    touch "file with spaces.txt"
    touch $'file_with_tab\t.txt'
    touch $'file_with_newline\n.txt'
    touch 'file_with_quote".txt'
    touch 'file_with_backslash\\.txt'
    touch 'file_with_*.txt'

    run_test "special_char_spaces" \
             "$LS_CMD -j \"file with spaces.txt\"" \
             '.[0].name' \
             '"file with spaces.txt"'
    run_test "special_char_tab" \
             "$LS_CMD -j $'file_with_tab\t.txt'" \
             '.[0].name' \
             '"file_with_tab\\t.txt"' # Tab should be escaped as \t in JSON string
    run_test "special_char_newline" \
             "$LS_CMD -j $'file_with_newline\n.txt'" \
             '.[0].name' \
             '"file_with_newline\\n.txt"' # Newline should be escaped as \n
    run_test "special_char_quote" \
             "$LS_CMD -j 'file_with_quote\".txt'" \
             '.[0].name' \
             '"file_with_quote\\".txt"' # Quote should be escaped as \"
    run_test "special_char_backslash" \
             "$LS_CMD -j 'file_with_backslash\\\\.txt'" \
             '.[0].name' \
             '"file_with_backslash\\\\.txt"' # Backslash should be escaped as \\
    run_test "special_char_asterisk" \
             "$LS_CMD -j 'file_with_*.txt'" \
             '.[0].name' \
             '"file_with_*.txt"'
}

# Test Case 3: Permissions
test_permissions() {
    echo "\n--- Test Case: Permissions ---"
    touch perm_file.txt
    chmod 0777 perm_file.txt
    run_test "perms_0777_mode_octal" \
             "$LS_CMD -j perm_file.txt" \
             '.[0].mode_octal' \
             '"0777"' # Mode for regular file, not considering S_IFREG
    # Note: mode_octal in ls.c includes file type bits. S_IFREG | 0777
    # Need to get actual mode from stat to predict accurately or mask it.
    # Let's get the actual mode and then check.
    # local actual_mode=$(stat -f "%p" perm_file.txt) # stat syntax varies
    # For now, check against string representation if mode_octal is complex.
    run_test "perms_0777_mode_string" \
             "$LS_CMD -j perm_file.txt" \
             '.[0].mode_string' \
             '"-rwxrwxrwx"' # Assuming umask allows this

    chmod 0600 perm_file.txt
    run_test "perms_0600_mode_string" \
             "$LS_CMD -j perm_file.txt" \
             '.[0].mode_string' \
             '"-rw-------"'

    chmod 0444 perm_file.txt
    run_test "perms_0444_mode_string" \
             "$LS_CMD -j perm_file.txt" \
             '.[0].mode_string' \
             '"-r--r--r--"'
}

# Test Case 4: Timestamps (basic check)
test_timestamps() {
    echo "\n--- Test Case: Timestamps ---"
    touch timestamp_file.txt
    # Get current time (approximate)
    # Validating exact timestamps is tricky due to second-level precision and test execution time.
    # We'll check for presence and that they are numbers.
    run_test "timestamps_exist_are_numbers" \
             "$LS_CMD -j timestamp_file.txt" \
             '.[0].atime | type == "number" and (.[0].mtime | type == "number") and (.[0].ctime | type == "number") and (.[0].birthtime | type == "number")' \
             "true"

    # Specific mtime test using GNU date and touch syntax (common in Ubuntu sandbox)
    # Create a known past date
    past_date_sec=$(date -d "2023-01-15 14:30:00" +%s 2>/dev/null)
    if [ -n "$past_date_sec" ]; then
        touch -m -d "2023-01-15 14:30:00" timestamp_file.txt 2>/dev/null
        if [ $? -eq 0 ]; then
            run_test "timestamp_specific_mtime" \
                     "$LS_CMD -j timestamp_file.txt" \
                     ".[0].mtime == $past_date_sec" \
                     "true"
        else
            echo "SKIP: timestamp_specific_mtime (touch -d failed, GNU touch might not be available)"
        fi
    else
        echo "SKIP: timestamp_specific_mtime (date -d failed, GNU date might not be available)"
    fi
}


# --- Call test functions ---
test_basic_types
test_special_chars
test_permissions
test_timestamps
# More test cases to be added here for -A, -o, -Z (if feasible) etc.

# --- Test Summary ---
cd "$TEST_DIR/.." # Go back to original directory before cleanup
echo "\n--- Summary ---"
if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "All $TEST_COUNT tests passed."
    exit 0
else
    echo "$FAIL_COUNT out of $TEST_COUNT tests failed."
    exit 1
fi
make -C bin/ls/tests/ls_json_test.sh executable
chmod +x bin/ls/tests/ls_json_test.sh
