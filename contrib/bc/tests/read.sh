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

. "$testdir/../functions.sh"

if [ "$#" -lt 1 ]; then
	printf 'usage: %s dir [exe [args...]]\n' "$0"
	printf 'valid dirs are:\n'
	printf '\n'
	cat "$testdir/all.txt"
	printf '\n'
	exit 1
fi

d="$1"
shift

if [ "$#" -gt 0 ]; then
	exe="$1"
	shift
else
	exe="$testdir/../bin/$d"
fi

name="$testdir/$d/read.txt"
results="$testdir/$d/read_results.txt"
errors="$testdir/$d/read_errors.txt"

out="$testdir/${d}_outputs/read_results.txt"
outdir=$(dirname "$out")

if [ ! -d "$outdir" ]; then
	mkdir -p "$outdir"
fi

exebase=$(basename "$exe")

if [ "$d" = "bc" ]; then
	options="-lq"
	halt="halt"
else
	options="-x"
	halt="q"
fi

if [ "$d" = "bc" ]; then
	read_call="read()"
	read_expr="${read_call}\n5+5;"
else
	read_call="?"
	read_expr="${read_call}"
fi

printf 'Running %s read...' "$d"

set +e

while read line; do

	printf '%s\n%s\n' "$read_call" "$line" | "$exe" "$@" "$options" > "$out"
	checktest "$d" "$?" 'read' "$results" "$out"

done < "$name"

printf 'pass\n'

printf 'Running %s read errors...' "$d"

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
