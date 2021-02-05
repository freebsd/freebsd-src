#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2020 Gavin D. Howard and contributors.
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

err_exit() {

	if [ "$#" -ne 2 ]; then
		printf 'Invalid number of args to err_exit\n'
		exit 1
	fi

	printf '%s\n' "$1"
	printf '\nexiting...\n'
	exit "$2"
}

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

checktest()
{
	_checktest_d="$1"
	shift

	_checktest_error="$1"
	shift

	_checktest_name="$1"
	shift

	_checktest_out="$1"
	shift

	_checktest_exebase="$1"
	shift

	checkcrash "$_checktest_d" "$_checktest_error" "$_checktest_name"

	if [ "$_checktest_error" -eq 0 ]; then
		die "$_checktest_d" "returned no error" "$_checktest_name" 127
	fi

	if [ "$_checktest_error" -eq 100 ]; then

		_checktest_output=$(cat "$_checktest_out")
		_checktest_fatal_error="Fatal error"

		if [ "${_checktest_output##*$_checktest_fatal_error*}" ]; then
			printf "%s\n" "$_checktest_output"
			die "$_checktest_d" "had memory errors on a non-fatal error" \
				"$_checktest_name" "$_checktest_error"
		fi
	fi

	if [ ! -s "$_checktest_out" ]; then
		die "$_checktest_d" "produced no error message" "$_checktest_name" "$_checktest_error"
	fi

	# Display the error messages if not directly running exe.
	# This allows the script to print valgrind output.
	if [ "$_checktest_exebase" != "bc" -a "$_checktest_exebase" != "dc" ]; then
		cat "$_checktest_out"
	fi
}

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

gen_nlspath() {

	_gen_nlspath_nlspath="$1"
	shift

	_gen_nlspath_locale="$1"
	shift

	_gen_nlspath_execname="$1"
	shift

	_gen_nlspath_char="@"
	_gen_nlspath_modifier="${_gen_nlspath_locale#*$_gen_nlspath_char}"
	_gen_nlspath_tmplocale="${_gen_nlspath_locale%%$_gen_nlspath_char*}"

	_gen_nlspath_char="."
	_gen_nlspath_charset="${_gen_nlspath_tmplocale#*$_gen_nlspath_char}"
	_gen_nlspath_tmplocale="${_gen_nlspath_tmplocale%%$_gen_nlspath_char*}"

	if [ "$_gen_nlspath_charset" = "$_gen_nlspath_tmplocale" ]; then
		_gen_nlspath_charset=""
	fi

	_gen_nlspath_char="_"
	_gen_nlspath_territory="${_gen_nlspath_tmplocale#*$_gen_nlspath_char}"
	_gen_nlspath_language="${_gen_nlspath_tmplocale%%$_gen_nlspath_char*}"

	if [ "$_gen_nlspath_territory" = "$_gen_nlspath_tmplocale" ]; then
		_gen_nlspath_territory=""
	fi

	if [ "$_gen_nlspath_language" = "$_gen_nlspath_tmplocale" ]; then
		_gen_nlspath_language=""
	fi

	_gen_nlspath_needles="%%:%L:%N:%l:%t:%c"

	_gen_nlspath_needles=$(printf '%s' "$_gen_nlspath_needles" | tr ':' '\n')

	for _gen_nlspath_i in $_gen_nlspath_needles; do
		_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "$_gen_nlspath_i" "|$_gen_nlspath_i|")
	done

	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%%" "%")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%L" "$_gen_nlspath_locale")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%N" "$_gen_nlspath_execname")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%l" "$_gen_nlspath_language")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%t" "$_gen_nlspath_territory")
	_gen_nlspath_nlspath=$(substring_replace "$_gen_nlspath_nlspath" "%c" "$_gen_nlspath_charset")

	_gen_nlspath_nlspath=$(printf '%s' "$_gen_nlspath_nlspath" | tr -d '|')

	printf '%s' "$_gen_nlspath_nlspath"
}
