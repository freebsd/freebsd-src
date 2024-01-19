#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2023 Gavin D. Howard and contributors.
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

scriptdir=$(dirname "$0")

gnu=/usr/bin/bc
gdh=/usr/local/bin/bc

if [ "$#" -lt 1 ]; then
	printf 'err: must provide path to new bc\n'
	exit 1
fi

new="$1"
shift

unset BC_LINE_LENGTH && unset BC_ENV_ARGS

gdh_fail_file="sqrt_fails.bc"
new_fail_file="new_sqrt_fails.bc"

rm -rf "$gdh_fail_file"
rm -rf "$new_fail_file"

while [ true ]; do

	tst=$("$gdh" -l "$scriptdir/sqrt_random.bc")
	err=$?

	if [ "$err" -ne 0 ]; then
		printf 'err: failed to create test\n'
		exit 2
	fi

	good=$(printf '%s\n' "$tst" | "$gnu" -l)

	gdh_out=$(printf '%s\n' "$tst" | "$gdh" -l)
	new_out=$(printf '%s\n' "$tst" | "$new" -l)

	gdh_good=$(printf '%s == %s\n' "$good" "$gdh_out" | "$gnu")
	new_good=$(printf '%s == %s\n' "$good" "$new_out" | "$gnu")

	if [ "$gdh_good" -eq 0 ]; then
		printf '%s\n' "$tst" >> "$gdh_fail_file"
	fi

	if [ "$new_good" -eq 0 ]; then
		printf '%s\n' "$tst" >> "$new_fail_file"
	fi

done
