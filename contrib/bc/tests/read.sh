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

set -e

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
	printf 'usage: %s dir [exe [args...]]\n' "$script"
	printf 'valid dirs are:\n'
	printf '\n'
	cat "$testdir/all.txt"
	printf '\n'
	exit 1
}

# Command-line processing.
if [ "$#" -lt 1 ]; then
	usage "Not enough arguments"
fi

d="$1"
shift
check_d_arg "$d"

if [ "$#" -gt 0 ]; then
	exe="$1"
	shift
	check_exec_arg "$exe"
else
	exe="$testdir/../bin/$d"
	check_exec_arg "$exe"
fi

name="$testdir/$d/read.txt"
results="$testdir/$d/read_results.txt"
errors="$testdir/$d/read_errors.txt"

out="$outputdir/${d}_outputs/read_results.txt"
multiple_res="$outputdir/${d}_outputs/read_multiple_results.txt"
outdir=$(dirname "$out")

# Make sure the directory exists.
if [ ! -d "$outdir" ]; then
	mkdir -p "$outdir"
fi

exebase=$(basename "$exe")

# Set stuff for the correct calculator.
if [ "$d" = "bc" ]; then
	options="-lq"
	halt="halt"
	read_call="read()"
	read_expr="${read_call}\n5+5;"
	read_multiple=$(printf '%s\n%s\n%s\n' "3" "2" "1")
else
	options="-x"
	halt="q"
	read_call="?"
	read_expr="${read_call}"
	read_multiple=$(printf '%spR\n%spR\n%spR\n' "3" "2" "1")
fi

# I use these, so unset them to make the tests work.
unset BC_ENV_ARGS
unset BC_LINE_LENGTH
unset DC_ENV_ARGS
unset DC_LINE_LENGTH

printf 'Running %s read...' "$d"

set +e

# Run read() on every line.
while read line; do

	printf '%s\n%s\n' "$read_call" "$line" | "$exe" "$@" "$options" > "$out"
	checktest "$d" "$?" 'read' "$results" "$out"

done < "$name"

printf 'pass\n'

printf 'Running %s read multiple...' "$d"

printf '3\n2\n1\n' > "$multiple_res"

# Run multiple read() calls.
printf '%s\n' "$read_multiple" | "$exe" "$@" "$options" -e "$read_call" -e "$read_call" -e "$read_call" > "$out"
checktest "$d" "$?" 'read multiple' "$multiple_res" "$out"

printf 'pass\n'

printf 'Running %s read errors...' "$d"

# Run read on every line.
while read line; do

	printf '%s\n%s\n' "$read_call" "$line" | "$exe" "$@" "$options" 2> "$out" > /dev/null
	err="$?"

	checkerrtest "$d" "$err" "$line" "$out" "$exebase"

done < "$errors"

printf 'pass\n'

printf 'Running %s empty read...' "$d"

read_test=$(printf '%s\n' "$read_call")

printf '%s\n' "$read_test" | "$exe" "$@" "$opts" 2> "$out" > /dev/null
err="$?"

checkerrtest "$d" "$err" "$read_test" "$out" "$exebase"

printf 'pass\n'

printf 'Running %s read EOF...' "$d"

read_test=$(printf '%s' "$read_call")

printf '%s' "$read_test" | "$exe" "$@" "$opts" 2> "$out" > /dev/null
err="$?"

checkerrtest "$d" "$err" "$read_test" "$out" "$exebase"

exec printf 'pass\n'
