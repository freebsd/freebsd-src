#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2023 Gavin D. Howard and contributors.
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
	printf 'usage: %s dir test problematic_tests [exec args...]\n' "$script"
	exit 1
}

# Command-line processing.
if [ "$#" -lt 3 ]; then
	usage "Not enough arguments"
else

	d="$1"
	shift
	check_d_arg "$d"

	t="$1"
	shift

	problematic="$1"
	shift
	check_bool_arg "$problematic"

fi

testfile="$testdir/$d/errors/$t"
check_file_arg "$testfile"

if [ "$#" -lt 1 ]; then
	exe="$testdir/../bin/$d"
else
	exe="$1"
	shift
fi

# Just skip tests that are problematic on FreeBSD. These tests can cause FreeBSD
# to kill bc from memory exhaustion because of overcommit.
if [ "$d" = "bc" ] && [ "$problematic" -eq 0 ]; then
	if [ "$t" = "33.txt" ]; then
		printf 'Skipping problematic %s error file %s...\n' "$d" "$t"
		exit 0
	fi
fi

# I use these, so unset them to make the tests work.
unset BC_ENV_ARGS
unset BC_LINE_LENGTH
unset DC_ENV_ARGS
unset DC_LINE_LENGTH

out="$outputdir/${d}_outputs/error_results_${t}"
outdir=$(dirname "$out")

# Make sure the directory exists.
if [ ! -d "$outdir" ]; then
	mkdir -p "$outdir"
fi

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

printf 'Running %s error file %s with clamping...' "$d" "$t"

printf '%s\n' "$halt" | "$exe" "$@" $opts -c "$testfile" 2> "$out" > /dev/null
err="$?"

checkerrtest "$d" "$err" "$testfile" "$out" "$exebase" > /dev/null

printf 'pass\n'

printf 'Running %s error file %s without clamping...' "$d" "$t"

printf '%s\n' "$halt" | "$exe" "$@" $opts -C "$testfile" 2> "$out" > /dev/null
err="$?"

checkerrtest "$d" "$err" "$testfile" "$out" "$exebase" > /dev/null

printf 'pass\n'

printf 'Running %s error file %s through cat with clamping...' "$d" "$t"

cat "$testfile" | "$exe" "$@" $opts -c 2> "$out" > /dev/null
err="$?"

checkerrtest "$d" "$err" "$testfile" "$out" "$exebase"

printf 'pass\n'

printf 'Running %s error file %s through cat without clamping...' "$d" "$t"

cat "$testfile" | "$exe" "$@" $opts -C 2> "$out" > /dev/null
err="$?"

checkerrtest "$d" "$err" "$testfile" "$out" "$exebase"

printf 'pass\n'
