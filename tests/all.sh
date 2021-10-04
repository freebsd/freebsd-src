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

script="$0"
testdir=$(dirname "$script")

. "$testdir/../scripts/functions.sh"

# We need to figure out if we should run stuff in parallel.
pll=1

while getopts "n" opt; do

	case "$opt" in
		n) pll=0 ; shift ; set -e ;;
		?) usage "Invalid option: $opt" ;;
	esac

done

# Command-line processing.
if [ "$#" -ge 1 ]; then
	d="$1"
	shift
else
	err_exit "usage: $script [-n] dir [run_extra_tests] [run_stack_tests] [gen_tests] [time_tests] [exec args...]" 1
fi

if [ "$#" -lt 1 ]; then
	extra=1
else
	extra="$1"
	shift
fi

if [ "$#" -lt 1 ]; then
	run_stack_tests=1
else
	run_stack_tests="$1"
	shift
fi

if [ "$#" -lt 1 ]; then
	generate_tests=1
else
	generate_tests="$1"
	shift
fi

if [ "$#" -lt 1 ]; then
	time_tests=0
else
	time_tests="$1"
	shift
fi

if [ "$#" -lt 1 ]; then
	exe="$testdir/../bin/$d"
else
	exe="$1"
	shift
fi

stars="***********************************************************************"
printf '%s\n' "$stars"

# Set stuff for the correct calculator.
if [ "$d" = "bc" ]; then
	halt="quit"
else
	halt="q"
fi

# I use these, so unset them to make the tests work.
unset BC_ENV_ARGS
unset BC_LINE_LENGTH
unset DC_ENV_ARGS
unset DC_LINE_LENGTH

# Get the list of tests that require extra math.
extra_required=$(cat "$testdir/extra_required.txt")

pids=""

printf '\nRunning %s tests...\n\n' "$d"

# Run the tests one at a time.
while read t; do

	# If it requires extra, then skip if we don't have it.
	if [ "$extra" -eq 0 ]; then
		if [ -z "${extra_required##*$t*}" ]; then
			printf 'Skipping %s %s\n' "$d" "$t"
			continue
		fi
	fi

	if [ "$pll" -ne 0 ]; then
		sh "$testdir/test.sh" "$d" "$t" "$generate_tests" "$time_tests" "$exe" "$@" &
		pids="$pids $!"
	else
		sh "$testdir/test.sh" "$d" "$t" "$generate_tests" "$time_tests" "$exe" "$@"
	fi

done < "$testdir/$d/all.txt"

# stdin tests.
if [ "$pll" -ne 0 ]; then
	sh "$testdir/stdin.sh" "$d" "$exe" "$@" &
	pids="$pids $!"
else
	sh "$testdir/stdin.sh" "$d" "$exe" "$@"
fi

# Script tests.
if [ "$pll" -ne 0 ]; then
	sh "$testdir/scripts.sh" "$d" "$extra" "$run_stack_tests" "$generate_tests" \
		"$time_tests" "$exe" "$@" &
	pids="$pids $!"
else
	sh "$testdir/scripts.sh" -n "$d" "$extra" "$run_stack_tests" "$generate_tests" \
		"$time_tests" "$exe" "$@"
fi

# Read tests.
if [ "$pll" -ne 0 ]; then
	sh "$testdir/read.sh" "$d" "$exe" "$@" &
	pids="$pids $!"
else
	sh "$testdir/read.sh" "$d" "$exe" "$@"
fi

# Error tests.
if [ "$pll" -ne 0 ]; then
	sh "$testdir/errors.sh" "$d" "$exe" "$@" &
	pids="$pids $!"
else
	sh "$testdir/errors.sh" "$d" "$exe" "$@"
fi

# Test all the files in the errors directory. While the other error test (in
# tests/errors.sh) does a test for every line, this does one test per file, but
# it runs the file through stdin and as a file on the command-line.
for testfile in $testdir/$d/errors/*.txt; do

	b=$(basename "$testfile")

	if [ "$pll" -ne 0 ]; then
		sh "$testdir/error.sh" "$d" "$b" "$@" &
		pids="$pids $!"
	else
		sh "$testdir/error.sh" "$d" "$b" "$@"
	fi

done

# Other tests.
if [ "$pll" -ne 0 ]; then
	sh "$testdir/other.sh" "$d" "$extra" "$exe" "$@" &
	pids="$pids $!"
else
	sh "$testdir/other.sh" "$d" "$extra" "$exe" "$@"
fi

if [ "$pll" -ne 0 ]; then

	exit_err=0

	for p in $pids; do

		wait "$p"
		err="$?"

		if [ "$err" -ne 0 ]; then
			printf 'A test failed!\n'
			exit_err=1
		fi
	done

	if [ "$exit_err" -ne 0 ]; then
		exit 1
	fi

fi

printf '\nAll %s tests passed.\n' "$d"

printf '\n%s\n' "$stars"
