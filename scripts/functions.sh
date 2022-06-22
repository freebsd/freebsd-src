#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2021 Gavin D. Howard and contributors.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

# This script is NOT meant to be run! It is meant to be sourced by other
# scripts.

# Reads and follows a link until it finds a real file. This is here because the
# readlink utility is not part of the POSIX standard. Sigh...
# @param f  The link to find the original file for.
readlink() {

	_readlink_f="$1"
	shift

	_readlink_arrow="-> "
	_readlink_d=$(dirname "$_readlink_f")

	_readlink_lsout=""
	_readlink_link=""

	_readlink_lsout=$(ls -dl "$_readlink_f")
	_readlink_link=$(printf '%s' "${_readlink_lsout#*$_readlink_arrow}")

	while [ -z "${_readlink_lsout##*$_readlink_arrow*}" ]; do
		_readlink_f="$_readlink_d/$_readlink_link"
		_readlink_d=$(dirname "$_readlink_f")
		_readlink_lsout=$(ls -dl "$_readlink_f")
		_readlink_link=$(printf '%s' "${_readlink_lsout#*$_readlink_arrow}")
	done

	printf '%s' "${_readlink_f##*$_readlink_d/}"
}

# Quick function for exiting with an error.
# @param 1  A message to print.
# @param 2  The exit code to use.
err_exit() {

	if [ "$#" -ne 2 ]; then
		printf 'Invalid number of args to err_exit\n'
		exit 1
	fi

	printf '%s\n' "$1"
	exit "$2"
}

# Check the return code on a test and exit with a fail if it's non-zero.
# @param d     The calculator under test.
# @param err   The return code.
# @param name  The name of the test.
checktest_retcode() {

	_checktest_retcode_d="$1"
	shift

	_checktest_retcode_err="$1"
	shift

	_checktest_retcode_name="$1"
	shift

	if [ "$_checktest_retcode_err" -ne 0 ]; then
		printf 'FAIL!!!\n'
		err_exit "$_checktest_retcode_d failed test '$_checktest_retcode_name' with error code $_checktest_retcode_err" 1
	fi
}

# Check the result of a test. First, it checks the error code using
# checktest_retcode(). Then it checks the output against the expected output
# and fails if it doesn't match.
# @param d             The calculator under test.
# @param err           The error code.
# @param name          The name of the test.
# @param test_path     The path to the test.
# @param results_name  The path to the file with the expected result.
checktest() {

	_checktest_d="$1"
	shift

	_checktest_err="$1"
	shift

	_checktest_name="$1"
	shift

	_checktest_test_path="$1"
	shift

	_checktest_results_name="$1"
	shift

	checktest_retcode "$_checktest_d" "$_checktest_err" "$_checktest_name"

	_checktest_diff=$(diff "$_checktest_test_path" "$_checktest_results_name")

	_checktest_err="$?"

	if [ "$_checktest_err" -ne 0 ]; then
		printf 'FAIL!!!\n'
		printf '%s\n' "$_checktest_diff"
		err_exit "$_checktest_d failed test $_checktest_name" 1
	fi
}

# Die. With a message.
# @param d     The calculator under test.
# @param msg   The message to print.
# @param name  The name of the test.
# @param err   The return code from the test.
die() {

	_die_d="$1"
	shift

	_die_msg="$1"
	shift

	_die_name="$1"
	shift

	_die_err="$1"
	shift

	_die_str=$(printf '\n%s %s on test:\n\n    %s\n' "$_die_d" "$_die_msg" "$_die_name")

	err_exit "$_die_str" "$_die_err"
}

# Check that a test did not crash and die if it did.
# @param d      The calculator under test.
# @param error  The error code.
# @param name   The name of the test.
checkcrash() {

	_checkcrash_d="$1"
	shift

	_checkcrash_error="$1"
	shift

	_checkcrash_name="$1"
	shift


	if [ "$_checkcrash_error" -gt 127 ]; then
		die "$_checkcrash_d" "crashed ($_checkcrash_error)" \
			"$_checkcrash_name" "$_checkcrash_error"
	fi
}

# Check that a test had an error or crash.
# @param d        The calculator under test.
# @param error    The error code.
# @param name     The name of the test.
# @param out      The file that the test results were output to.
# @param exebase  The name of the executable.
checkerrtest()
{
	_checkerrtest_d="$1"
	shift

	_checkerrtest_error="$1"
	shift

	_checkerrtest_name="$1"
	shift

	_checkerrtest_out="$1"
	shift

	_checkerrtest_exebase="$1"
	shift

	checkcrash "$_checkerrtest_d" "$_checkerrtest_error" "$_checkerrtest_name"

	if [ "$_checkerrtest_error" -eq 0 ]; then
		die "$_checkerrtest_d" "returned no error" "$_checkerrtest_name" 127
	fi

	# This is to check for memory errors with Valgrind, which is told to return
	# 100 on memory errors.
	if [ "$_checkerrtest_error" -eq 100 ]; then

		_checkerrtest_output=$(cat "$_checkerrtest_out")
		_checkerrtest_fatal_error="Fatal error"

		if [ "${_checkerrtest_output##*$_checkerrtest_fatal_error*}" ]; then
			printf "%s\n" "$_checkerrtest_output"
			die "$_checkerrtest_d" "had memory errors on a non-fatal error" \
				"$_checkerrtest_name" "$_checkerrtest_error"
		fi
	fi

	if [ ! -s "$_checkerrtest_out" ]; then
		die "$_checkerrtest_d" "produced no error message" "$_checkerrtest_name" "$_checkerrtest_error"
	fi

	# To display error messages, uncomment this line. This is useful when
	# debugging.
	#cat "$_checkerrtest_out"
}

# Replace a substring in a string with another. This function is the *real*
# workhorse behind configure.sh's generation of a Makefile.
#
# This function uses a sed call that uses exclamation points `!` as delimiters.
# As a result, needle can never contain an exclamation point. Oh well.
#
# @param str          The string that will have any of the needle replaced by
#                     replacement.
# @param needle       The needle to replace in str with replacement.
# @param replacement  The replacement for needle in str.
substring_replace() {

	_substring_replace_str="$1"
	shift

	_substring_replace_needle="$1"
	shift

	_substring_replace_replacement="$1"
	shift

	_substring_replace_result=$(printf '%s\n' "$_substring_replace_str" | \
		sed -e "s!$_substring_replace_needle!$_substring_replace_replacement!g")

	printf '%s' "$_substring_replace_result"
}

# Generates an NLS path based on the locale and executable name.
#
# This is a monstrosity for a reason.
#
# @param nlspath   The $NLSPATH
# @param locale    The locale.
# @param execname  The name of the executable.
gen_nlspath() {

	_gen_nlspath_nlspath="$1"
	shift

	_gen_nlspath_locale="$1"
	shift

	_gen_nlspath_execname="$1"
	shift

	# Split the locale into its modifier and other parts.
	_gen_nlspath_char="@"
	_gen_nlspath_modifier="${_gen_nlspath_locale#*$_gen_nlspath_char}"
	_gen_nlspath_tmplocale="${_gen_nlspath_locale%%$_gen_nlspath_char*}"

	# Split the locale into charset and other parts.
	_gen_nlspath_char="."
	_gen_nlspath_charset="${_gen_nlspath_tmplocale#*$_gen_nlspath_char}"
	_gen_nlspath_tmplocale="${_gen_nlspath_tmplocale%%$_gen_nlspath_char*}"

	# Check for an empty charset.
	if [ "$_gen_nlspath_charset" = "$_gen_nlspath_tmplocale" ]; then
		_gen_nlspath_charset=""
	fi

	# Split the locale into territory and language.
	_gen_nlspath_char="_"
	_gen_nlspath_territory="${_gen_nlspath_tmplocale#*$_gen_nlspath_char}"
	_gen_nlspath_language="${_gen_nlspath_tmplocale%%$_gen_nlspath_char*}"

	# Check for empty territory and language.
	if [ "$_gen_nlspath_territory" = "$_gen_nlspath_tmplocale" ]; then
		_gen_nlspath_territory=""
	fi

	if [ "$_gen_nlspath_language" = "$_gen_nlspath_tmplocale" ]; then
		_gen_nlspath_language=""
	fi

	# Prepare to replace the format specifiers. This is done by wrapping the in
	# pipe characters. It just makes it easier to split them later.
	_gen_nlspath_needles="%%:%L:%N:%l:%t:%c"

	_gen_nlspath_needles=$(printf '%s' "$_gen_nlspath_needles" | tr ':' '\n')

	for _gen_nlspath_i in $_gen_nlspath_needles; do
		_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "$_gen_nlspath_i" "|$_gen_nlspath_i|")
	done

	# Replace all the format specifiers.
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%%" "%")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%L" "$_gen_nlspath_locale")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%N" "$_gen_nlspath_execname")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%l" "$_gen_nlspath_language")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%t" "$_gen_nlspath_territory")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%c" "$_gen_nlspath_charset")

	# Get rid of pipe characters.
	_gen_nlspath_nlspath=$(printf '%s' "$_gen_nlspath_nlspath" | tr -d '|')

	# Return the result.
	printf '%s' "$_gen_nlspath_nlspath"
}

ALL=0
NOSKIP=1
SKIP=2

# Filters text out of a file according to the build type.
# @param in    File to filter.
# @param out   File to write the filtered output to.
# @param type  Build type.
filter_text() {

	_filter_text_in="$1"
	shift

	_filter_text_out="$1"
	shift

	_filter_text_buildtype="$1"
	shift

	# Set up some local variables.
	_filter_text_status="$ALL"
	_filter_text_last_line=""

	# We need to set IFS, so we store it here for restoration later.
	_filter_text_ifs="$IFS"

	# Remove the file- that will be generated.
	rm -rf "$_filter_text_out"

	# Here is the magic. This loop reads the template line-by-line, and based on
	# _filter_text_status, either prints it to the markdown manual or not.
	#
	# Here is how the template is set up: it is a normal markdown file except
	# that there are sections surrounded tags that look like this:
	#
	# {{ <build_type_list> }}
	# ...
	# {{ end }}
	#
	# Those tags mean that whatever build types are found in the
	# <build_type_list> get to keep that section. Otherwise, skip.
	#
	# Obviously, the tag itself and its end are not printed to the markdown
	# manual.
	while IFS= read -r _filter_text_line; do

		# If we have found an end, reset the status.
		if [ "$_filter_text_line" = "{{ end }}" ]; then

			# Some error checking. This helps when editing the templates.
			if [ "$_filter_text_status" -eq "$ALL" ]; then
				err_exit "{{ end }} tag without corresponding start tag" 2
			fi

			_filter_text_status="$ALL"

		# We have found a tag that allows our build type to use it.
		elif [ "${_filter_text_line#\{\{* $_filter_text_buildtype *\}\}}" != "$_filter_text_line" ]; then

			# More error checking. We don't want tags nested.
			if [ "$_filter_text_status" -ne "$ALL" ]; then
				err_exit "start tag nested in start tag" 3
			fi

			_filter_text_status="$NOSKIP"

		# We have found a tag that is *not* allowed for our build type.
		elif [ "${_filter_text_line#\{\{*\}\}}" != "$_filter_text_line" ]; then

			if [ "$_filter_text_status" -ne "$ALL" ]; then
				err_exit "start tag nested in start tag" 3
			fi

			_filter_text_status="$SKIP"

		# This is for normal lines. If we are not skipping, print.
		else
			if [ "$_filter_text_status" -ne "$SKIP" ]; then
				if [ "$_filter_text_line" != "$_filter_text_last_line" ]; then
					printf '%s\n' "$_filter_text_line" >> "$_filter_text_out"
				fi
				_filter_text_last_line="$_filter_text_line"
			fi
		fi

	done < "$_filter_text_in"

	# Reset IFS.
	IFS="$_filter_text_ifs"
}
