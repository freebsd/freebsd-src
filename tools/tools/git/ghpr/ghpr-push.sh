#!/bin/sh

set -e

die() {
    echo $*
    exit 1
}

staging=staging
do_pr_branch_push=false


# Iteratively try to push all the branches, then push upstream. Repeat until the upstream
# push works...
while true; do
    # We'll likely drop pushing to the pull request branches, but that's not
    # final, so keep the code, but if false'd out. We'll make it a proper option
    # or remove it once the discussion settles down. Only Warner can use it at
    # the moment anyway.
    if $do_pr_branch_push; then
	for pr in $(git config --get-all branch.${staging}.opabinia.prs); do
	    upstream=$(git config --get branch.${staging}.opabinia.${pr}.upstream)
	    upstream_branch=$(git config --get branch.${staging}.opabinia.${pr}.upstream-branch)
	    git push $upstream HEAD:$upstream_branch --force || true 	# bare git push gives cut and paste line
	done
    fi

      if ! git push  --push-option=confirm-author freebsd HEAD:main; then
	  git fetch freebsd
	  git rebase freebsd/main ${staging}
	  continue
      fi
      break
done

# OK, pull and rebase to catchup to these changes...
git checkout main;
git pull --rebase

# try to cleanup
for pr in $(git config --get-all branch.${staging}.opabinia.prs); do
    if ! $do_pr_branch_push; then
	gh pr edit $pr --add-label merged
	gh pr close $pr --comment "Automated message from ghpr: Thank you for your submission. This PR has been merged to FreeBSD's `main` branch. These changes will appear shortly on our GitHub mirror."
    fi
    git branch -D PR-${pr}
    git config --remove-section branch.${staging}.opabinia.${pr}
done
git config --remove-section branch.${staging}.opabinia
git branch -D ${staging}
