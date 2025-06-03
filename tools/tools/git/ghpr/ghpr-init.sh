#!/bin/sh

set -e

die() {
    echo $*
    exit 1
}

# Create a fresh branch for the staging tree.
BRANCH=${1:-staging}
base=main

if [ "$(git config branch.${BRANCH}.opabinia)" = "true" ]; then
    echo "Branch ${BRANCH} has already been initialized"
    # Bail if the branch already exists
else
    if git rev-parse --verify ${BRANCH} > /dev/null 2>&1; then
	echo "Branch ${BRANCH} already exists, skipping creation, but rebasing to ${base}"
	git rebase ${base} ${BRANCH}
    else
	# Create the branch and tag it as the one we're using for opabinia merging.
	echo "Creating ${BRANCH} from ${base} to land changes"
	git checkout -b ${BRANCH} ${base} || die "Can't create ${BRANCH}"
    fi
fi

git config set --type bool --all branch.${BRANCH}.opabinia true || die "Can't annotate"
git config set --all branch.${BRANCH}.opabinia.base "${base}" || die "Can't annotate"
