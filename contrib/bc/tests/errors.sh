#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2024 Gavin D. Howard and contributors.
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

# WARNING: Test files cannot have empty lines!

script="$0"
testdir=$(dirname "$script")

. "$testdir/../scripts/functions.sh"

outputdir=${BC_TEST_OUTPUT_DIR:-$testdir}

# Just print the usage and exit with an error. This can receive a message to
# print.
# @param 1  A message to print.
usage() {
	if [ $# -eq 1 ]; then
		printf '%s\n\n' "$1"
	fi
	printf 'usage: %s dir [exec args...]\n' "$script"
	exit 1
}

# Command-line processing.
if [ "$#" -eq 0 ]; then
	usage "Not enough arguments"
else
	d="$1"
	shift
	check_d_arg "$d"
fi

if [ "$#" -lt 1 ]; then
	exe="$testdir/../bin/$d"
	check_exec_arg "$exe"
else
	exe="$1"
	shift
	check_exec_arg "$exe"
fi

# I use these, so unset them to make the tests work.
unset BC_ENV_ARGS
unset BC_LINE_LENGTH
unset DC_ENV_ARGS
unset DC_LINE_LENGTH

out="$outputdir/${d}_outputs/errors_results.txt"
outdir=$(dirname "$out")

# Make sure the directory exists.
if [ ! -d "$outdir" ]; then
	mkdir -p "$outdir"
fi

exebase=$(basename "$exe")

# These are the filenames for the extra tests.
posix="posix_errors"
read_errors="read_errors"

# Set stuff for the correct calculator.
if [ "$d" = "bc" ]; then
	opts="-l"
	halt="halt"
	read_call="read()"
	read_expr="${read_call}\n5+5;"
else
	opts="-x"
	halt="q"
fi

printf 'Running %s command-line error tests...' "$d"

printf '%s\n' "$halt" 2> /dev/null | "$exe" "$@" -e "1+1" -f- -e "2+2" 2> "$out" > /dev/null
err="$?"

checkerrtest "$d" "$err" "command-line -e test" "$out" "$exebase"

printf '%s\n' "$halt" 2> /dev/null | "$exe" "$@" -e "1+1" -f- -f "$testdir/$d/decimal.txt" 2> "$out" > /dev/null
err="$?"

checkerrtest "$d" "$err" "command-line -f test" "$out" "$exebase"

printf 'pass\n'

# Now test the error files in the standard tests directory.
for testfile in $testdir/$d/*errors.txt; do

	if [ -z "${testfile##*$read_errors*}" ]; then
		# We don't test read errors here. Skip.
		continue
	fi

	# Test bc POSIX errors and warnings.
	if [ -z "${testfile##*$posix*}" ]; then

		# Just test warnings.
		line="last"
		printf '%s\n' "$line" 2> /dev/null | "$exe" "$@" "-lw"  2> "$out" > /dev/null
		err="$?"

		if [ "$err" -ne 0 ]; then
			die "$d" "returned an error ($err)" "POSIX warning" 1
		fi

		checkerrtest "$d" "1" "$line" "$out" "$exebase"

		# Set the options for standard mode.
		options="-ls"

	else
		options="$opts"
	fi

	# Output something pretty.
	base=$(basename "$testfile")
	base="${base%.*}"
	printf 'Running %s %s...' "$d" "$base"

	# Test errors on each line of the file. Yes, each line has a separate error
	# case.
	while read -r line; do

		rm -f "$out"

		printf '%s\n' "$line" 2> /dev/null | "$exe" "$@" "$options" 2> "$out" > /dev/null
		err="$?"

		checkerrtest "$d" "$err" "$line" "$out" "$exebase"

	done < "$testfile"

	printf 'pass\n'

done
