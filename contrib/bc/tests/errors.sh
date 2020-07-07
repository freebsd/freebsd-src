#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2020 Gavin D. Howard and contributors.
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

# WARNING: Test files cannot have empty lines!

script="$0"
testdir=$(dirname "$script")

. "$testdir/../functions.sh"

if [ "$#" -eq 0 ]; then
	printf 'usage: %s dir [exec args...]\n' "$script"
	exit 1
else
	d="$1"
	shift
fi

if [ "$#" -lt 1 ]; then
	exe="$testdir/../bin/$d"
else
	exe="$1"
	shift
fi

out="$testdir/../.log_${d}_test.txt"

exebase=$(basename "$exe")

posix="posix_errors"
read_errors="read_errors"

if [ "$d" = "bc" ]; then
	opts="-l"
	halt="halt"
	read_call="read()"
	read_expr="${read_call}\n5+5;"
else
	opts="-x"
	halt="q"
fi

for testfile in $testdir/$d/*errors.txt; do

	if [ -z "${testfile##*$read_errors*}" ]; then
		# We don't test read errors here. Skip.
		continue
	fi

	if [ -z "${testfile##*$posix*}" ]; then

		line="last"
		printf '%s\n' "$line" | "$exe" "$@" "-lw"  2> "$out" > /dev/null
		err="$?"

		if [ "$err" -ne 0 ]; then
			die "$d" "returned an error ($err)" "POSIX warning" 1
		fi

		checktest "$d" "1" "$line" "$out" "$exebase"

		options="-ls"
	else
		options="$opts"
	fi

	base=$(basename "$testfile")
	base="${base%.*}"
	printf 'Running %s %s...' "$d" "$base"

	while read -r line; do

		rm -f "$out"

		printf '%s\n' "$line" | "$exe" "$@" "$options" 2> "$out" > /dev/null
		err="$?"

		checktest "$d" "$err" "$line" "$out" "$exebase"

	done < "$testfile"

	printf 'pass\n'

done

for testfile in $testdir/$d/errors/*.txt; do

	printf 'Running %s error file %s...' "$d" "$testfile"

	printf '%s\n' "$halt" | "$exe" "$@" $opts "$testfile" 2> "$out" > /dev/null
	err="$?"

	checktest "$d" "$err" "$testfile" "$out" "$exebase"

	printf 'pass\n'

	printf 'Running %s error file %s through cat...' "$d" "$testfile"

	cat "$testfile" | "$exe" "$@" $opts 2> "$out" > /dev/null
	err="$?"

	checkcrash "$d" "$err" "$testfile"

	printf 'pass\n'

done
