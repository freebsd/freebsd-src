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

testdir=$(dirname "${script}")

. "$testdir/../scripts/functions.sh"

outputdir=${BC_TEST_OUTPUT_DIR:-$testdir}

# Command-line processing.
if [ "$#" -lt 2 ]; then
	printf 'usage: %s dir script [run_extra_tests] [run_stack_tests] [generate_tests] [time_tests] [exec args...]\n' "$script"
	exit 1
fi

d="$1"
shift

f="$1"
shift

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

# Set stuff for the correct calculator.
if [ "$d" = "bc" ]; then

	if [ "$run_stack_tests" -ne 0 ]; then
		options="-lgq"
	else
		options="-lq"
	fi

	halt="halt"

else
	options="-x"
	halt="q"
fi

scriptdir="$testdir/$d/scripts"

name="${f%.*}"

# We specifically want to skip this because it is handled specially.
if [ "$f" = "timeconst.bc" ]; then
	exit 0
fi

# Skip the tests that require extra math if we don't have it.
if [ "$run_extra_tests" -eq 0 ]; then
	if [ "$f" = "rand.bc" ]; then
		printf 'Skipping %s script: %s\n' "$d" "$f"
		exit 0
	fi
fi

# Skip the tests that require global stacks flag if we are not allowed to run
# them.
if [ "$run_stack_tests" -eq 0 ]; then

	if [ "$f" = "globals.bc" ] || [ "$f" = "references.bc" ] || [ "$f" = "rand.bc" ]; then
		printf 'Skipping %s script: %s\n' "$d" "$f"
		exit 0
	fi

fi

out="$outputdir/${d}_outputs/${name}_script_results.txt"
outdir=$(dirname "$out")

# Make sure the directory exists.
if [ ! -d "$outdir" ]; then
	mkdir -p "$outdir"
fi

# I use these, so unset them to make the tests work.
unset BC_ENV_ARGS
unset BC_LINE_LENGTH
unset DC_ENV_ARGS
unset DC_LINE_LENGTH

s="$scriptdir/$f"
orig="$testdir/$name.txt"
results="$scriptdir/$name.txt"

if [ -f "$orig" ]; then
	res="$orig"
elif [ -f "$results" ]; then
	res="$results"
elif [ "$generate" -eq 0 ]; then
	printf 'Skipping %s script %s\n' "$d" "$f"
	exit 0
else

	set +e

	# This is to check that the command exists. If not, we should not try to
	# generate the test. Instead, we should just skip.
	command -v "$d"
	err="$?"

	set -e

	if [ "$err" -ne 0 ]; then
		printf 'Could not find %s to generate results; skipping %s script %s\n' "$d" "$d" "$f"
		exit 0
	fi

	# This sed, and the script, are to remove an incompatibility with GNU bc,
	# where GNU bc is wrong. See the development manual
	# (manuals/development.md#script-tests) for more information.
	printf 'Generating %s results...' "$f"
	printf '%s\n' "$halt" | "$d" "$s" | sed -n -f "$testdir/script.sed" > "$results"
	printf 'done\n'
	res="$results"
fi

set +e

printf 'Running %s script %s...' "$d" "$f"

# Yes this is poor timing, but it works.
if [ "$time_tests" -ne 0 ]; then
	printf '\n'
	printf '%s\n' "$halt" | /usr/bin/time -p "$exe" "$@" $options "$s" > "$out"
	err="$?"
	printf '\n'
else
	printf '%s\n' "$halt" | "$exe" "$@" $options "$s" > "$out"
	err="$?"
fi

checktest "$d" "$err" "script $f" "$res" "$out"

rm -f "$out"

exec printf 'pass\n'
