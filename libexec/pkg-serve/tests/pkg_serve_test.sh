#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Baptiste Daroussin <bapt@FreeBSD.org>

PKG_SERVE="${PKG_SERVE:-/usr/libexec/pkg-serve}"

serve()
{
	printf "$1" | "${PKG_SERVE}" "$2"
}

check_output()
{
	local pattern="$1" ; shift
	output=$(serve "$@")
	case "$output" in
	*${pattern}*)
		return 0
		;;
	*)
		echo "Expected pattern: ${pattern}"
		echo "Got: ${output}"
		return 1
		;;
	esac
}

atf_test_case greeting
greeting_head()
{
	atf_set "descr" "Server sends greeting on connect"
}
greeting_body()
{
	mkdir repo
	check_output "ok: pkg-serve " "quit\n" repo ||
	    atf_fail "greeting not found"
}

atf_test_case unknown_command
unknown_command_head()
{
	atf_set "descr" "Unknown commands get ko response"
}
unknown_command_body()
{
	mkdir repo
	check_output "ko: unknown command 'plop'" "plop\nquit\n" repo ||
	    atf_fail "expected ko for unknown command"
}

atf_test_case get_missing_file
get_missing_file_head()
{
	atf_set "descr" "Requesting a missing file returns ko"
}
get_missing_file_body()
{
	mkdir repo
	check_output "ko: file not found" "get nonexistent.pkg 0\nquit\n" repo ||
	    atf_fail "expected file not found"
}

atf_test_case get_file
get_file_head()
{
	atf_set "descr" "Requesting an existing file returns its content"
}
get_file_body()
{
	mkdir repo
	echo "testcontent" > repo/test.pkg
	output=$(serve "get test.pkg 0\nquit\n" repo)
	echo "$output" | grep -q "ok: 12" ||
	    atf_fail "expected ok: 12, got: ${output}"
	echo "$output" | grep -q "testcontent" ||
	    atf_fail "expected testcontent in output"
}

atf_test_case get_file_leading_slash
get_file_leading_slash_head()
{
	atf_set "descr" "Leading slash in path is stripped"
}
get_file_leading_slash_body()
{
	mkdir repo
	echo "testcontent" > repo/test.pkg
	check_output "ok: 12" "get /test.pkg 0\nquit\n" repo ||
	    atf_fail "leading slash not stripped"
}

atf_test_case get_file_uptodate
get_file_uptodate_head()
{
	atf_set "descr" "File with old mtime returns ok: 0"
}
get_file_uptodate_body()
{
	mkdir repo
	echo "testcontent" > repo/test.pkg
	check_output "ok: 0" "get test.pkg 9999999999\nquit\n" repo ||
	    atf_fail "expected ok: 0 for up-to-date file"
}

atf_test_case get_directory
get_directory_head()
{
	atf_set "descr" "Requesting a directory returns ko"
}
get_directory_body()
{
	mkdir -p repo/subdir
	check_output "ko: file not found" "get subdir 0\nquit\n" repo ||
	    atf_fail "expected file not found"
}

atf_test_case get_missing_age
get_missing_age_head()
{
	atf_set "descr" "get without age argument returns error"
}
get_missing_age_body()
{
	mkdir repo
	check_output "ko: bad command get" "get test.pkg\nquit\n" repo ||
	    atf_fail "expected bad command get"
}

atf_test_case get_bad_age
get_bad_age_head()
{
	atf_set "descr" "get with non-numeric age returns error"
}
get_bad_age_body()
{
	mkdir repo
	check_output "ko: bad number" "get test.pkg notanumber\nquit\n" repo ||
	    atf_fail "expected bad number"
}

atf_test_case get_empty_arg
get_empty_arg_head()
{
	atf_set "descr" "get with no arguments returns error"
}
get_empty_arg_body()
{
	mkdir repo
	check_output "ko: bad command get" "get \nquit\n" repo ||
	    atf_fail "expected bad command get"
}

atf_test_case get_fifo
get_fifo_head()
{
	atf_set "descr" "Requesting a FIFO returns ko without blocking"
}
get_fifo_body()
{
	mkdir repo
	mkfifo repo/fifo
	output=$(printf 'get fifo 0\nquit\n' | timeout 10 "${PKG_SERVE}" repo)
	case "$output" in
	*"ko: file not found"*)
		;;
	*)
		atf_fail "expected 'file not found', got: ${output}"
		;;
	esac
}

atf_test_case no_existence_oracle
no_existence_oracle_head()
{
	atf_set "descr" "Missing and non-regular files are indistinguishable"
}
no_existence_oracle_body()
{
	mkdir repo
	mkdir repo/dir
	missing=$(serve "get missing.pkg 0\nquit\n" repo)
	nonreg=$(serve "get dir 0\nquit\n" repo)
	[ "${missing}" = "${nonreg}" ] ||
	    atf_fail "responses differ: '${missing}' vs '${nonreg}'"
}

atf_test_case path_traversal
path_traversal_head()
{
	atf_set "descr" "Path traversal with .. is rejected"
}
path_traversal_body()
{
	mkdir repo
	check_output "ko: file not found" \
	    "get ../etc/passwd 0\nquit\n" repo ||
	    atf_fail "path traversal not rejected"
}

atf_test_case get_subdir_file
get_subdir_file_head()
{
	atf_set "descr" "Files in subdirectories are served"
}
get_subdir_file_body()
{
	mkdir -p repo/sub
	echo "subcontent" > repo/sub/file.pkg
	output=$(serve "get sub/file.pkg 0\nquit\n" repo)
	echo "$output" | grep -q "ok: 11" ||
	    atf_fail "expected ok: 11, got: ${output}"
	echo "$output" | grep -q "subcontent" ||
	    atf_fail "expected subcontent in output"
}

atf_test_case multiple_gets
multiple_gets_head()
{
	atf_set "descr" "Multiple get commands in one session"
}
multiple_gets_body()
{
	mkdir repo
	echo "aaa" > repo/a.pkg
	echo "bbb" > repo/b.pkg
	output=$(serve "get a.pkg 0\nget b.pkg 0\nquit\n" repo)
	echo "$output" | grep -q "ok: 4" ||
	    atf_fail "expected ok: 4 for a.pkg"
	echo "$output" | grep -q "aaa" ||
	    atf_fail "expected content of a.pkg"
	echo "$output" | grep -q "bbb" ||
	    atf_fail "expected content of b.pkg"
}

atf_test_case bad_basedir
bad_basedir_head()
{
	atf_set "descr" "Non-existent basedir causes exit failure"
}
bad_basedir_body()
{
	atf_check -s not-exit:0 -e match:"open" \
	    "${PKG_SERVE}" /nonexistent/path
}

atf_test_case command_too_long
command_too_long_head()
{
	atf_set "descr" "Overlong command line is rejected and does not desync the stream"
}
command_too_long_body()
{
	mkdir repo
	echo "testcontent" > repo/test.pkg
	# A line far beyond the per-command limit must be discarded, and the
	# commands that follow it on the same stream must still be processed.
	output=$(awk 'BEGIN {
	    for (i = 0; i < 100000; i++)
		printf "A"
	    printf "\nget test.pkg 0\nquit\n"
	}' | "${PKG_SERVE}" repo)
	case "$output" in
	*"ko: command too long"*)
		;;
	*)
		atf_fail "expected 'command too long', got: ${output}"
		;;
	esac
	case "$output" in
	*"ok: 12"*"testcontent"*)
		;;
	*)
		atf_fail "server did not recover after overlong line: ${output}"
		;;
	esac
}

atf_init_test_cases()
{
	atf_add_test_case greeting
	atf_add_test_case unknown_command
	atf_add_test_case get_missing_file
	atf_add_test_case get_file
	atf_add_test_case get_file_leading_slash
	atf_add_test_case get_file_uptodate
	atf_add_test_case get_directory
	atf_add_test_case no_existence_oracle
	atf_add_test_case get_missing_age
	atf_add_test_case get_bad_age
	atf_add_test_case get_empty_arg
	atf_add_test_case command_too_long
	atf_add_test_case get_fifo
	atf_add_test_case path_traversal
	atf_add_test_case get_subdir_file
	atf_add_test_case multiple_gets
	atf_add_test_case bad_basedir
}
