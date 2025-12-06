#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2025 Gavin D. Howard and contributors.
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

testdir=$(dirname "${script}")

. "$testdir/../scripts/functions.sh"

# Just print the usage and exit with an error. This can receive a message to
# print.
# @param 1  A message to print.
usage() {
	if [ $# -eq 1 ]; then
		printf '%s\n\n' "$1"
	fi
	printf 'usage: %s [-n] dir [run_extra_tests] [run_stack_tests] [generate_tests] [exec args...]\n' "$script"
	exit 1
}

pids=""

# We need to figure out if we should run stuff in parallel.
pll=1

while getopts "n" opt; do

	case "$opt" in
		n) pll=0 ; set -e ;;
		?) usage "Invalid option: $opt" ;;
	esac

done
shift $(($OPTIND - 1))

# Command-line processing.
if [ "$#" -eq 0 ]; then
	usage "Need at least 1 argument"
else
	d="$1"
	shift
	check_d_arg "$d"
fi

if [ "$#" -gt 0 ]; then
	run_extra_tests="$1"
	shift
	check_bool_arg "$run_extra_tests"
else
	run_extra_tests=1
	check_bool_arg "$run_extra_tests"
fi

if [ "$#" -gt 0 ]; then
	run_stack_tests="$1"
	shift
	check_bool_arg "$run_stack_tests"
else
	run_stack_tests=1
	check_bool_arg "$run_stack_tests"
fi

if [ "$#" -gt 0 ]; then
	generate="$1"
	shift
	check_bool_arg "$generate"
else
	generate=1
	check_bool_arg "$generate"
fi

if [ "$#" -gt 0 ]; then
	exe="$1"
	shift
	check_exec_arg "$exe"
else
	exe="$testdir/../bin/$d"
	check_exec_arg "$exe"
fi

scriptdir="$testdir/$d/scripts"

scripts=$(cat "$scriptdir/all.txt")

# Run each script test individually.
for s in $scripts; do

	f=$(basename "$s")

	if [ "$pll" -ne 0 ]; then
		sh "$testdir/script.sh" "$d" "$f" "$run_extra_tests" "$run_stack_tests" \
			"$generate" "$exe" "$@" &
		pids="$pids $!"
	else
		sh "$testdir/script.sh" "$d" "$f" "$run_extra_tests" "$run_stack_tests" \
			"$generate" "$exe" "$@"
	fi

done

if [ "$pll" -ne 0 ]; then

	exit_err=0

	for p in $pids; do

		wait "$p"
		err="$?"

		if [ "$err" -ne 0 ]; then
			printf 'A script failed!\n'
			exit_err=1
		fi

	done

	if [ "$exit_err" -ne 0 ]; then
		exit 1
	fi

fi
