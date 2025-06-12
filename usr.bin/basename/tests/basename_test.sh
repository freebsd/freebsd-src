#!/bin/sh -e
#
# Copyright (c) 2023 The FreeBSD Foundation
#
# SPDX-License-Identifier: BSD-2-Clause
#
# MAINTAINER: user
#
# Test cases for the basename(1) utility, including the -d option.

# Helper function to get the path to the basename program
# This might need adjustment based on the actual test environment.
# For now, assume 'basename' is in the PATH or in a location
# discoverable by the test runner.
BASENAME_CMD="basename"

# Test cases for the -d option

atf_test_case d_option_basic
d_option_basic_head() {
	atf_set "descr" "Test basename -d /usr/bin/foo"
}
d_option_basic_body() {
	atf_check -o inline:"/usr/bin\n" ${BASENAME_CMD} -d /usr/bin/foo
}

atf_test_case d_option_trailing_slash
d_option_trailing_slash_head() {
	atf_set "descr" "Test basename -d /usr/bin/foo/"
}
d_option_trailing_slash_body() {
	atf_check -o inline:"/usr/bin\n" ${BASENAME_CMD} -d /usr/bin/foo/
}

atf_test_case d_option_no_path
d_option_no_path_head() {
	atf_set "descr" "Test basename -d foo"
}
d_option_no_path_body() {
	atf_check -o inline:".\n" ${BASENAME_CMD} -d foo
}

atf_test_case d_option_dot
d_option_dot_head() {
	atf_set "descr" "Test basename -d /usr/bin/."
}
d_option_dot_body() {
	atf_check -o inline:"/usr/bin\n" ${BASENAME_CMD} -d /usr/bin/.
}

atf_test_case d_option_root
d_option_root_head() {
	atf_set "descr" "Test basename -d /"
}
d_option_root_body() {
	atf_check -o inline:"/\n" ${BASENAME_CMD} -d /
}

atf_test_case d_option_double_root
d_option_double_root_head() {
	atf_set "descr" "Test basename -d //"
}
d_option_double_root_body() {
	atf_check -o inline:"/\n" ${BASENAME_CMD} -d //
}

atf_test_case d_option_empty_string
d_option_empty_string_head() {
	atf_set "descr" "Test basename -d \"\""
}
d_option_empty_string_body() {
	atf_check -o inline:".\n" ${BASENAME_CMD} -d ""
}

# Test cases for mutual exclusivity of -d

atf_test_case d_option_conflict_a
d_option_conflict_a_head() {
	atf_set "descr" "Test basename -d -a /usr/bin/foo (should fail)"
}
d_option_conflict_a_body() {
	atf_check -s exit:1 -e ignore ${BASENAME_CMD} -d -a /usr/bin/foo
}

atf_test_case d_option_conflict_s
d_option_conflict_s_head() {
	atf_set "descr" "Test basename -d -s .c /usr/bin/foo.c (should fail)"
}
d_option_conflict_s_body() {
	atf_check -s exit:1 -e ignore ${BASENAME_CMD} -d -s .c /usr/bin/foo.c
}

atf_test_case d_option_multiple_paths
d_option_multiple_paths_head() {
	atf_set "descr" "Test basename -d /usr/bin/foo /usr/local (should fail)"
}
d_option_multiple_paths_body() {
	atf_check -s exit:1 -e ignore ${BASENAME_CMD} -d /usr/bin/foo /usr/local
}

# Test cases for existing functionality

atf_test_case existing_basic
existing_basic_head() {
	atf_set "descr" "Test basename /usr/bin/foo"
}
existing_basic_body() {
	atf_check -o inline:"foo\n" ${BASENAME_CMD} /usr/bin/foo
}

atf_test_case existing_suffix
existing_suffix_head() {
	atf_set "descr" "Test basename /usr/bin/foo.c .c"
}
existing_suffix_body() {
	atf_check -o inline:"foo\n" ${BASENAME_CMD} /usr/bin/foo.c .c
}

atf_test_case existing_suffix_no_match
existing_suffix_no_match_head() {
	atf_set "descr" "Test basename /usr/bin/foo.c .h"
}
existing_suffix_no_match_body() {
	atf_check -o inline:"foo.c\n" ${BASENAME_CMD} /usr/bin/foo.c .h
}

atf_test_case existing_s_option
existing_s_option_head() {
	atf_set "descr" "Test basename -s .c /usr/bin/foo.c"
}
existing_s_option_body() {
	atf_check -o inline:"foo\n" ${BASENAME_CMD} -s .c /usr/bin/foo.c
}

atf_test_case existing_a_option
existing_a_option_head() {
	atf_set "descr" "Test basename -a /usr/bin/foo /usr/local/bar"
}
existing_a_option_body() {
	atf_check -o inline:"foo\nbar\n" ${BASENAME_CMD} -a /usr/bin/foo /usr/local/bar
}

atf_test_case existing_a_s_option
existing_a_s_option_head() {
	atf_set "descr" "Test basename -a -s .c /usr/bin/foo.c /usr/local/bar.c"
}
existing_a_s_option_body() {
	atf_check -o inline:"foo\nbar\n" ${BASENAME_CMD} -a -s .c /usr/bin/foo.c /usr/local/bar.c
}

atf_test_case existing_empty_string
existing_empty_string_head() {
	atf_set "descr" "Test basename \"\""
}
existing_empty_string_body() {
	# Expecting a single newline character for an empty string input,
	# which is what printf("\n") in the C code does.
	atf_check -o inline:"\n" ${BASENAME_CMD} ""
}

atf_init_test_cases() {
	atf_add_test_case d_option_basic
	atf_add_test_case d_option_trailing_slash
	atf_add_test_case d_option_no_path
	atf_add_test_case d_option_dot
	atf_add_test_case d_option_root
	atf_add_test_case d_option_double_root
	atf_add_test_case d_option_empty_string
	atf_add_test_case d_option_conflict_a
	atf_add_test_case d_option_conflict_s
	atf_add_test_case d_option_multiple_paths
	atf_add_test_case existing_basic
	atf_add_test_case existing_suffix
	atf_add_test_case existing_suffix_no_match
	atf_add_test_case existing_s_option
	atf_add_test_case existing_a_option
	atf_add_test_case existing_a_s_option
	atf_add_test_case existing_empty_string
}

# Ensure this script is executable: chmod +x basename_test.sh
# To run these tests (assuming ATF is installed and configured):
# kyua test -k /path/to/basename_test.sh
# or directly if using atf-run:
# atf-run | atf-report
