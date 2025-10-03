#! /bin/sh
#
# SPDX-License-Identifier: ISC
#
# Copyright (c) 2025 Lexi Winter <ivy@FreeBSD.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# Generate metapackage sets.  We do this by examining the annotations field
# of the packages we previously built.

set -e

if [ $# -lt 3 ]; then
	printf >&2 'usage: %s <srcdir> <wstagedir> <repodir>\n' "$0"
	exit 1
fi

srcdir="$1"; shift
wstagedir="$1"; shift
repodir="$1"; shift
# Everything after the first three arguments is UCL variables we pass to
# generate-set-ucl.lua.
UCL_VARS="$@"

# Nothing is explicitly added to set-base, so it wouldn't get built unless
# we list it here.
SETS="base base-dbg base-jail base-jail-dbg"

for pkg in "$repodir"/*.pkg; do
	# If the package name doesn't containing a '-', then it's
	# probably data.pkg or packagesite.pkg, which are not real
	# packages.
	{ echo "$pkg" | grep -q '-'; } || continue

	set -- $(pkg query -F "$pkg" '%At %n %Av' | grep '^set ')
	pkgname="$2"
	sets="$(echo "$3" | tr , ' ')"
	for set in $sets; do
		SETS="$SETS $set"
		setvar="$(echo "$set" | tr - _)"
		eval PKGS_${setvar}=\"\$PKGS_${setvar} $pkgname\"
	done
done

for set in $(echo $SETS | tr ' ' '\n' | sort | uniq); do
	setvar="$(echo "$set" | tr - _)"
	eval deps=\"\$PKGS_${setvar}\"

	"${srcdir}/release/packages/generate-set-ucl.lua" \
		"${srcdir}/release/packages/set-template.ucl" \
		PKGNAME "$set" \
		SET_DEPENDS "$deps" \
		UCLFILES "${srcdir}/release/packages/sets" \
		$UCL_VARS \
		> "${wstagedir}/set-${set}.ucl"
done
