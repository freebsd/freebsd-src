#!/bin/sh

set -e

die() {
    echo $*
    exit 1
}

staging=staging

# Iteratively try to push all the branches, then push upstream. Repeat until the upstream
# push works...
while true; do
      for pr in $(git config --get-all branch.${staging}.opabinia.prs); do
	  upstream=$(git config --get branch.${staging}.opabinia.${pr}.upstream)
	  upstream_branch=$(git config --get branch.${staging}.opabinia.${pr}.upstream-branch)

	  git push $upstream HEAD:$upstream_branch --force || true 	# bare git push gives cut and paste line
      done

      if ! git push  --push-option=confirm-author freebsd HEAD:main; then
	  git fetch freebsd
	  git rebase freebsd/main ${stagig}
	  continue
      fi
      break
done

# OK, pull and rebase to catchup to these changes...
git checkout main;
git pull --rebase

# try to cleanup
for pr in $(git config --get-all branch.${staging}.opabinia.prs); do
    git branch -D PR-${pr}
    git config --remove-section branch.${staging}.opabinia.${pr}
done
git config --remove-section branch.${staging}.opabinia
