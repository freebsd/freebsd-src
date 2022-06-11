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

export LANG=C
export LC_CTYPE=C

progname=${0##*/}

script="$0"
scriptdir=$(dirname "$script")
. "$scriptdir/../scripts/functions.sh"

# See strgen.c comment on main() for what these mean. Note, however, that this
# script generates a string literal, not a char array. To understand the
# consequences of that, see manuals/development.md#strgenc.
if [ $# -lt 3 ]; then
	echo "usage: $progname input output exclude name [label [define [remove_tabs]]]"
	exit 1
fi

input="$1"
output="$2"
exclude="$3"
name="$4"
label="$5"
define="$6"
remove_tabs="$7"

tmpinput=$(mktemp -t "${input##*/}")

if [ "$exclude" -ne 0 ]; then
	filter_text "$input" "$tmpinput" "E"
else
	filter_text "$input" "$tmpinput" "A"
fi

exec < "$tmpinput"
exec > "$output"

rm -f "$tmpinput"

if [ -n "$label" ]; then
	nameline="const char *${label} = \"${input}\";"
	labelexternline="extern const char *${label};"
fi

if [ -n "$define" ]; then
	condstart="#if ${define}"
	condend="#endif"
fi

if [ -n "$remove_tabs" ]; then
	if [ "$remove_tabs" -ne 0 ]; then
		remtabsexpr='s:	::g;'
	fi
fi

cat<<EOF
// Copyright (c) 2018-2021 Gavin D. Howard and contributors.
// Licensed under the 2-clause BSD license.
// *** AUTOMATICALLY GENERATED FROM ${input}. DO NOT MODIFY. ***

${condstart}
$labelexternline

extern const char $name[];

$nameline

const char ${name}[] =
$(sed -e "$remtabsexpr " -e '1,/^$/d; s:\\n:\\\\n:g; s:":\\":g; s:^:":; s:$:\\n":')
;
${condend}
EOF

#if [ "$exclude" -ne 0 ]; then
	#rm -rf "$input"
#fi
