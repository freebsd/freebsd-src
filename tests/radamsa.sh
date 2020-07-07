#! /bin/bash
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

getentry() {

	if [ $# -gt 0 ]; then
		entnum="$1"
	else
		entnum=0
	fi

	e=$(cat -)
	num=$(printf '%s\n' "$e" | wc -l)

	if [ "$entnum" -eq 0 ]; then
		rand=$(printf 'irand(%s) + 1\n' "$num" | "$bcdir/bc")
	else
		rand="$entnum"
	fi

	ent=$(printf '%s\n' "$e" | tail -n +$rand | head -n 1)

	printf '%s\n' "$ent"
}

script="$0"

if [ "$#" -lt 1 ]; then
	printf 'usage: %s dir\n' "$0"
	exit 1
fi

d="$1"
shift

dir=$(dirname "$script")

. "$dir/../functions.sh"

bcdir="$dir/../bin"

if [ "$d" = "bc" ]; then
	inputs="$dir/../../inputs"
	opts="-lq"
elif [ "$d" = "dc" ]; then
	inputs="$dir/../../inputs_dc"
	opts="-x"
else
	err_exit "wrong type of executable" 1
fi

export ASAN_OPTIONS="abort_on_error=1"

entries=$(cat "$dir/radamsa.txt")

IFS=$'\n'

go=1

while [ "$go" -ne 0 ]; do

	if [ "$d" = "bc" ]; then

		entry=$(cat -- "$dir/radamsa.txt" | getentry)
		items=$(printf '%s\n' "$entry" | radamsa -n 10)

		printf '%s\n' "$items"

		for i in `seq 1 10`; do

			item=$(printf '%s\n' "$items" | getentry "$i")

			export BC_ENV_ARGS="$item"
			echo 'halt' | "$bcdir/$d"
			err=$?

			checkcrash "$d" "$err" "radamsa env args: \"$item\""
		done

	fi

	f=$(ls "$inputs" | getentry)
	l=$(cat "$inputs/$f" | wc -l)
	ll=$(printf '%s^2\n' "$l" | bc)

	for i in $(seq 1 2); do
		data=$(cat "$inputs/$f" | radamsa -n 1)
		printf '%s\n' "$data" > "$dir/../.log_${d}_test.txt"
		printf '%s\n' "$data" | timeout -s SIGTERM 5 "$bcdir/$d" "$opts" > /dev/null
		err=$?
		checkcrash "$d" "$err" "radamsa stdin"
	done

done
