#!/bin/sh

set -e

die() {
    echo $*
    exit 1
}

update_to_upstream() (
    local staging=$1
    local base=$2

    git checkout ${base}
    git pull --rebase
    git rebase -i ${base} ${staging}
)

PR=$1
staging=staging

[ -n "${PR}" ] || die "Need a pr"

if [ "$(git config branch.${staging}.opabinia)" != "true" ]; then
    die "Branch ${staging} has not been initialized"
fi

base=$(git config branch.${staging}.opabinia.base)
[ -n "${base}" ] || die "No base set on ${staging}"

if [ -z "$(git config --get-all branch.${staging}.opabinia.prs)" ]; then
    # Update ${base} if prs list is empty
    update_to_upstream ${staging} ${base}
else
    # Otherwise checkout staging as is
    git checkout ${staging}
fi

# OK. We always have to create a new branch for the PR. We do this into
# ${base} first (since that's what gh pr checkout does). We then then rebase
# this branch onto the staging branch, doing the rebase rewriting along the
# way. This rather convoluted setup was selected over cherry-picking and/or
# pulling a formatted patch to apply because it always applies it correctly
# and then we use git's tools to merge, pushing the problems there rather than
# trying to deal with them ourselves here.

# In the future, PR may be a list
# Also, error handling is annoying at best.

git branch -D PR-$PR || true
gh pr checkout $PR -b PR-$PR

upstream=$(git config branch.PR-$PR.pushRemote)
upstream_branch=$(git config branch.PR-$PR.merge | sed -e s=refs/heads/==)

git rebase -i ${base} --onto ${staging} --exec 'env EDITOR=$HOME/bin/git-fixup-editor git commit --amend --trailer "Reviewed-by: imp" --trailer "Pull-Request: https://github.com/freebsd/freebsd-src/pull/'"$PR"'"'
# Save the upstream data
git config --add branch.${staging}.opabinia.prs ${PR}
git config --add branch.${staging}.opabinia.${PR}.upstream ${upstream}
git config --add branch.${staging}.opabinia.${PR}.upstream-branch ${upstream_branch}
# Move the staging branch to the new tip of the tree.
git checkout -B ${staging} HEAD

# XXX need to somehow scrape the PR for approvals, translate that to FreeBSD's name
# and add that in the Reviewed-by stuff... that's done by hand...

# Sanity check things... not 100% right, since it checks everything we're queued up so far...
tools/build/checkstyle9.pl ${base}..${staging}

# Bump .Dd dates?
# run before/after igor?
# Anything else?
