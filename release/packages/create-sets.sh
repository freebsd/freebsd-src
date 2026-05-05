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

: ${PKG_CMD:="pkg"}

srcdir="$1"; shift
wstagedir="$1"; shift
repodir="$1"; shift
# Everything after the first three arguments is UCL variables we pass to
# generate-set-ucl.lua.
UCL_VARS="$@"

# Extract PKG_NAME_PREFIX so we can use it later.
PKG_NAME_PREFIX=""
set -- $UCL_VARS
while [ -n "$1" ]; do
	case "$1" in
	PKG_NAME_PREFIX)
		shift
		PKG_NAME_PREFIX="$1"
		break;;
	*)
		shift; shift;;
	esac
done
if [ -z "$PKG_NAME_PREFIX" ]; then
	printf >&2 '%s: PKG_NAME_PREFIX must be specified\n' "$0"
	exit 1
fi

# Nothing is explicitly added to set-base, so it wouldn't get built unless
# we list it here.
SETS="base base-dbg base-jail base-jail-dbg"

for pkg in "$repodir"/*.pkg; do
	# Check if we should process this package.
	case "${pkg##*/}" in

	# When building release, we add a 'pkg' package to the repository,
	# but this isn't a base package and doesn't have a set.  To avoid
	# this causing an error, skip it.
	pkg-*)	continue;;

	# Any existing set packages may also have no sets (and even if they
	# do, they shouldn't be included here).
	${PKG_NAME_PREFIX}-set-*)
		continue;;

	# If the package name contains a '-', process it.  All "real"
	# packages contain a '-', because the package filename format
	# is <pkgname>-<version>.pkg, so this skips files which aren't
	# really packages, like data.pkg or packagesite.pkg.
	#
	*-*)	;;
	*)	continue;;
	esac

	# Print a useful error message instead of failing silently if
	# grep doesn't find any sets here.
	set +e
	_tmp="$(${PKG_CMD} query -F "$pkg" '%At %n %Av' | grep '^set ')"
	if [ -z "$_tmp" ]; then
		printf >&2 '%s: package has no sets: %s\n' "$0" "$pkg"
		exit 1
	fi
	set -e

	set -- $_tmp
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

	flua "${srcdir}/release/packages/generate-set-ucl.lua" \
		"${srcdir}/release/packages/set-template.ucl" \
		PKGNAME "$set" \
		SET_DEPENDS "$deps" \
		UCLFILES "${srcdir}/release/packages/sets" \
		$UCL_VARS \
		> "${wstagedir}/set-${set}.ucl"
done
