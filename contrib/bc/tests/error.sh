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

outputdir=${BC_TEST_OUTPUT_DIR:-$testdir}

# Command-line processing.
if [ "$#" -lt 2 ]; then
	printf 'usage: %s dir test [exec args...]\n' "$script"
	exit 1
else
	d="$1"
	shift

	t="$1"
	shift
fi

if [ "$#" -lt 1 ]; then
	exe="$testdir/../bin/$d"
else
	exe="$1"
	shift
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

testfile="$testdir/$d/errors/$t"

printf 'Running %s error file %s...' "$d" "$t"

printf '%s\n' "$halt" | "$exe" "$@" $opts "$testfile" 2> "$out" > /dev/null
err="$?"

checkerrtest "$d" "$err" "$testfile" "$out" "$exebase" > /dev/null

printf 'pass\n'

printf 'Running %s error file %s through cat...' "$d" "$t"

cat "$testfile" | "$exe" "$@" $opts 2> "$out" > /dev/null
err="$?"

checkcrash "$d" "$err" "$testfile"

printf 'pass\n'
