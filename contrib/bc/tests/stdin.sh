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
	printf 'usage: %s dir [exe [args...]]\n' "$0"
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

out="$outputdir/${d}_outputs/stdin_results.txt"
outdir=$(dirname "$out")

# Make sure the directory exists.
if [ ! -d "$outdir" ]; then
	mkdir -p "$outdir"
fi

# Set stuff for the correct calculator.
if [ "$d" = "bc" ]; then
	options="-lq"
else
	options="-x"
fi

rm -f "$out"

# I use these, so unset them to make the tests work.
unset BC_ENV_ARGS
unset BC_LINE_LENGTH
unset DC_ENV_ARGS
unset DC_LINE_LENGTH

set +e

printf 'Running %s stdin tests...' "$d"

# Run the file through stdin.
cat "$testdir/$d/stdin.txt" | "$exe" "$@" "$options" > "$out" 2> /dev/null
checktest "$d" "$?" "stdin" "$testdir/$d/stdin_results.txt" "$out"

# bc has some more tests; run those.
if [ "$d" = "bc" ]; then

	cat "$testdir/$d/stdin1.txt" | "$exe" "$@" "$options" > "$out" 2> /dev/null
	checktest "$d" "$?" "stdin1" "$testdir/$d/stdin1_results.txt" "$out"

	cat "$testdir/$d/stdin2.txt" | "$exe" "$@" "$options" > "$out" 2> /dev/null
	checktest "$d" "$?" "stdin2" "$testdir/$d/stdin2_results.txt" "$out"
fi

rm -f "$out"

exec printf 'pass\n'
