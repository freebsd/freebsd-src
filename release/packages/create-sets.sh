#! /bin/sh

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

for pkg in "$repodir"/*.pkg; do
	# If the package name doesn't containing a '-', then it's
	# probably data.pkg or packagesite.pkg, which are not real
	# packages.
	{ echo "$pkg" | grep -q '-'; } || continue

	set -- $(pkg query -F "$pkg" '%At %n %Av' | grep '^set ')
	pkgname="$2"
	set="$3"
	SETS="$SETS $set"
	setvar="$(echo "$set" | tr - _)"
	eval PKGS_${setvar}=\"\$PKGS_${setvar} $pkgname\"
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
