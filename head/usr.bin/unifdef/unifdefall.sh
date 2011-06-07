#!/bin/sh
#
# unifdefall: remove all the #if's from a source file
#
# Copyright (c) 2002 - 2010 Tony Finch <dot@dotat.at>
# Copyright (c) 2009 - 2010 Jonathan Nieder <jrnieder@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

set -e

unifdef="$(dirname "$0")/unifdef"
if [ ! -e "$unifdef" ]
then
	unifdef=unifdef
fi

case "$@" in
"-d "*)	echo DEBUGGING 1>&2
	debug=-d
	shift
esac

basename=$(basename "$0")
tmp=$(mktemp -d "${TMPDIR:-/tmp}/$basename.XXXXXXXXXX") || exit 2
trap 'rm -r "$tmp" || exit 2' EXIT

export LC_ALL=C

# list of all controlling macros
"$unifdef" $debug -s "$@" | sort | uniq >"$tmp/ctrl"
# list of all macro definitions
cpp -dM "$@" | sort | sed 's/^#define //' >"$tmp/hashdefs"
# list of defined macro names
sed 's/[^A-Za-z0-9_].*$//' <"$tmp/hashdefs" >"$tmp/alldef"
# list of undefined and defined controlling macros
comm -23 "$tmp/ctrl" "$tmp/alldef" >"$tmp/undef"
comm -12 "$tmp/ctrl" "$tmp/alldef" >"$tmp/def"
# create a sed script that extracts the controlling macro definitions
# and converts them to unifdef command-line arguments
sed 's|.*|s/^&\\(([^)]*)\\)\\{0,1\\} /-D&=/p|' <"$tmp/def" >"$tmp/script"
# create the final unifdef command
{	echo "$unifdef" $debug -k '\'
	# convert the controlling undefined macros to -U arguments
	sed 's/.*/-U& \\/' <"$tmp/undef"
	# convert the controlling defined macros to quoted -D arguments
	sed -nf "$tmp/script" <"$tmp/hashdefs" |
		sed "s/'/'\\\\''/g;s/.*/'&' \\\\/"
	echo '"$@"'
} >"$tmp/cmd"
case $debug in
-d)	for i in ctrl hashdefs alldef undef def script cmd
	do	echo ==== $i
		cat "$tmp/$i"
	done 1>&2
esac
# run the command we just created
sh "$tmp/cmd" "$@"
