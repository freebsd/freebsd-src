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

set -e

script="$0"
testdir=$(dirname "$script")

. "$testdir/../scripts/functions.sh"

# Command-line processing.
if [ "$#" -ge 1 ]; then
	d="$1"
	shift
else
	err_exit "usage: $script dir [run_extra_tests] [run_stack_tests] [gen_tests] [time_tests] [exec args...]" 1
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

	sh "$testdir/test.sh" "$d" "$t" "$generate_tests" "$time_tests" "$exe" "$@"

done < "$testdir/$d/all.txt"

# stdin tests.
sh "$testdir/stdin.sh" "$d" "$exe" "$@"

# Script tests.
sh "$testdir/scripts.sh" "$d" "$extra" "$run_stack_tests" "$generate_tests" \
	"$time_tests" "$exe" "$@"

# Read tests.
sh "$testdir/read.sh" "$d" "$exe" "$@"

# Error tests.
sh "$testdir/errors.sh" "$d" "$exe" "$@"

# Other tests.
sh "$testdir/other.sh" "$d" "$extra" "$exe" "$@"

# History tests.
sh "$testdir/history.sh" "$d" -a

printf '\nAll %s tests passed.\n' "$d"

printf '\n%s\n' "$stars"
