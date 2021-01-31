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

usage() {
	printf "usage: %s manpage\n" "$0" 1>&2
	exit 1
}

print_manpage() {

	_print_manpage_md="$1"
	shift

	_print_manpage_out="$1"
	shift

	cat "$manualsdir/header.txt" > "$_print_manpage_out"
	cat "$manualsdir/header_${manpage}.txt" >> "$_print_manpage_out"

	pandoc -f markdown -t man "$_print_manpage_md" >> "$_print_manpage_out"

}

gen_manpage() {

	_gen_manpage_args="$1"
	shift

	_gen_manpage_status="$ALL"
	_gen_manpage_out="$manualsdir/$manpage/$_gen_manpage_args.1"
	_gen_manpage_md="$manualsdir/$manpage/$_gen_manpage_args.1.md"
	_gen_manpage_temp="$manualsdir/temp.1.md"
	_gen_manpage_ifs="$IFS"

	rm -rf "$_gen_manpage_out" "$_gen_manpage_md"

	while IFS= read -r line; do

		if [ "$line" = "{{ end }}" ]; then

			if [ "$_gen_manpage_status" -eq "$ALL" ]; then
				err_exit "{{ end }} tag without corresponding start tag" 2
			fi

			_gen_manpage_status="$ALL"

		elif [ "${line#\{\{* $_gen_manpage_args *\}\}}" != "$line" ]; then

			if [ "$_gen_manpage_status" -ne "$ALL" ]; then
				err_exit "start tag nested in start tag" 3
			fi

			_gen_manpage_status="$NOSKIP"

		elif [ "${line#\{\{*\}\}}" != "$line" ]; then

			if [ "$_gen_manpage_status" -ne "$ALL" ]; then
				err_exit "start tag nested in start tag" 3
			fi

			_gen_manpage_status="$SKIP"

		else
			if [ "$_gen_manpage_status" -ne "$SKIP" ]; then
				printf '%s\n' "$line" >> "$_gen_manpage_temp"
			fi
		fi

	done < "$manualsdir/${manpage}.1.md.in"

	uniq "$_gen_manpage_temp" "$_gen_manpage_md"
	rm -rf "$_gen_manpage_temp"

	IFS="$_gen_manpage_ifs"

	print_manpage "$_gen_manpage_md" "$_gen_manpage_out"
}

set -e

script="$0"
scriptdir=$(dirname "$script")
manualsdir="$scriptdir/manuals"

. "$scriptdir/functions.sh"

ARGS="A E H N P EH EN EP HN HP NP EHN EHP ENP HNP EHNP"
ALL=0
NOSKIP=1
SKIP=2

test "$#" -eq 1 || usage

manpage="$1"
shift

if [ "$manpage" != "bcl" ]; then

	for a in $ARGS; do
		gen_manpage "$a"
	done

else
	print_manpage "$manualsdir/${manpage}.3.md" "$manualsdir/${manpage}.3"
fi
