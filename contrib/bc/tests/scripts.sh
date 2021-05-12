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

testdir=$(dirname "${script}")

pids=""

# We need to figure out if we should run stuff in parallel.
pll=1

while getopts "n" opt; do

	case "$opt" in
		n) pll=0 ; shift ; set -e ;;
		?) usage "Invalid option: $opt" ;;
	esac

done

# Command-line processing.
if [ "$#" -eq 0 ]; then
	printf 'usage: %s [-n] dir [run_extra_tests] [run_stack_tests] [generate_tests] [time_tests] [exec args...]\n' "$script"
	exit 1
else
	d="$1"
	shift
fi

if [ "$#" -gt 0 ]; then
	run_extra_tests="$1"
	shift
else
	run_extra_tests=1
fi

if [ "$#" -gt 0 ]; then
	run_stack_tests="$1"
	shift
else
	run_stack_tests=1
fi

if [ "$#" -gt 0 ]; then
	generate="$1"
	shift
else
	generate=1
fi

if [ "$#" -gt 0 ]; then
	time_tests="$1"
	shift
else
	time_tests=0
fi

if [ "$#" -gt 0 ]; then
	exe="$1"
	shift
else
	exe="$testdir/../bin/$d"
fi

scriptdir="$testdir/$d/scripts"

scripts=$(cat "$scriptdir/all.txt")

# Run each script test individually.
for s in $scripts; do

	f=$(basename "$s")

	if [ "$pll" -ne 0 ]; then
		sh "$testdir/script.sh" "$d" "$f" "$run_extra_tests" "$run_stack_tests" \
			"$generate" "$time_tests" "$exe" "$@" &
		pids="$pids $!"
	else
		sh "$testdir/script.sh" "$d" "$f" "$run_extra_tests" "$run_stack_tests" \
			"$generate" "$time_tests" "$exe" "$@"
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
