#!/bin/bash

case $1 in
    v*-dts) ;;
    '')
	echo >&2 "No version given"
	exit 1
	;;
    *)
	echo >&2 "Unexpected version: $1"
	exit 1
	;;
esac

v=$1

set -e

# Use the date of Linus' originally tagged commit for the merge. This might
# differ from what the commit that the rewritten tag points to, since the
# orignal commit may have been discarded.
export GIT_AUTHOR_DATE=$(git log -1 --format=%ad "${v%-dts}")
if [ ! "${GIT_AUTHOR_DATE}" ] ; then
    echo >&2 "Unable to determine commit date for merge"
    exit 1
fi
if [ "${v}" = "v2.6.12-rc2-dts" ] ; then
    auh="--allow-unrelated-histories"
fi
git merge $auh --no-edit "${v}-raw"
git clean -fdqx
# Use the date of Linus' original tag for the tag.
case "${v%-dts}" in
    v2.6.12*|v2.6.13-rc[123])
        # Commits from v2.6.12-rc2..v2.6.13-rc3 lacked the date. So use the commit's
        # date.
        export GIT_COMMITTER_DATE="${GIT_AUTHOR_DATE}"
        ;;
    *)
        export GIT_COMMITTER_DATE="$(git for-each-ref --format='%(taggerdate)' "refs/tags/${v%-dts}")"
        ;;
esac
if [ ! "${GIT_COMMITTER_DATE}" ] ; then
    echo >&2 "Unable to determine date for tag"
    exit 1
fi
git tag -s -m "Tagging ${v}" -u 695A46C6 "${v}"
make -k -j12 -s
