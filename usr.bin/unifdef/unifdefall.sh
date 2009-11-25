#!/bin/sh
#
# unifdefall: remove all the #if's from a source file
#
# Copyright (c) 2002 - 2009 Tony Finch <dot@dotat.at>.  All rights reserved.
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
#	$dotat: unifdef/unifdefall.sh,v 1.21 2009/11/25 19:54:34 fanf2 Exp $
# $FreeBSD$

set -e

basename=$(basename $0)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/$basename.XXXXXXXXXX") || exit 2
trap 'rm -r "$tmp" || exit 1' EXIT

export LC_ALL=C

./unifdef -s "$@" | sort | uniq >"$tmp/ctrl"
cpp -dM "$@" | sort | sed 's/^#define //' >"$tmp/hashdefs"
sed 's/[^A-Za-z0-9_].*$//' "$tmp/hashdefs" >"$tmp/alldef"
comm -23 "$tmp/ctrl" "$tmp/alldef" >"$tmp/undef"
comm -12 "$tmp/ctrl" "$tmp/alldef" >"$tmp/def"
(
	echo ./unifdef -k \\
	sed 's/.*/-U& \\/' "$tmp/undef"
	while read sym
	do sed -n 's/^'$sym'\(([^)]*)\)\{0,1\} /-D'$sym'=/p' "$tmp/hashdefs"
	done <"$tmp/def" |
		sed "s/'/'\\\\''/g;s/.*/'&' \\\\/"
	echo '"$@"'
) >"$tmp/cmd"
sh "$tmp/cmd" "$@"
