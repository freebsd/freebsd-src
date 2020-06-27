#! /bin/sh
#
# Copyright (c) 2018-2020 Gavin D. Howard and contributors.
#
# All rights reserved.
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

if [ $# -lt 4 ]; then
	echo "usage: $progname input output name header [label [define [remove_tabs]]]"
	exit 1
fi

input="$1"
output="$2"
name="$3"
header="$4"
label="$5"
define="$6"
remove_tabs="$7"

exec < "$input"
exec > "$output"

if [ -n "$label" ]; then
	nameline="const char *${label} = \"${input}\";"
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
// Licensed under the 2-clause BSD license.
// *** AUTOMATICALLY GENERATED FROM ${input}. DO NOT MODIFY. ***

${condstart}
#include <${header}>

$nameline

const char ${name}[] =
$(sed -e "$remtabsexpr " -e '1,/^$/d; s:\\n:\\\\n:g; s:":\\":g; s:^:":; s:$:\\n":')
;
${condend}
EOF
