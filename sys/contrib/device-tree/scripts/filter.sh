#!/bin/sh

# git branch -D upstream/rewritten-prev upstream/master upstream/rewritten filter-state      

set -e

export SCRIPTS=$(dirname $(readlink -f $0))

export FILTER_BRANCH_SQUELCH_WARNING=1

UPSTREAM_MASTER=upstream/master
UPSTREAM_REWRITTEN=upstream/dts

LAST=$(git show-ref -s refs/heads/$UPSTREAM_MASTER||true)
if [ -n "$LAST" ] ; then
    RANGE="$LAST..$UPSTREAM_REWRITTEN"
else
    # This must be a new conversion...
    RANGE="$UPSTREAM_REWRITTEN"
fi

FETCH_HEAD=$(git rev-parse FETCH_HEAD)
if [ "$LAST" = "$FETCH_HEAD" ] ; then
	echo "Nothing new in FETCH_HEAD: $FETCH_HEAD"
	exit 0
fi

rm -f .git/refs/original/refs/heads/${UPSTREAM_REWRITTEN}

git branch -f $UPSTREAM_REWRITTEN FETCH_HEAD

git filter-branch --force \
	--index-filter ${SCRIPTS}/index-filter.sh \
	--msg-filter 'cat && /bin/echo -e "\n[ upstream commit: $GIT_COMMIT ]"' \
	--tag-name-filter 'while read t ; do /bin/echo -n $t-dts-raw ; done' \
	--parent-filter 'sed "s/-p //g" | xargs -r git show-branch --independent | sed "s/\</-p /g"' \
	--prune-empty --state-branch refs/heads/filter-state \
	-- $RANGE

git branch -f $UPSTREAM_MASTER FETCH_HEAD
