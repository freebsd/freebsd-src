#!/bin/sh -e

git svn fetch -A scripts/authors.svn

git checkout FreeBSD

case "$(git merge --no-commit git-svn)" in
"Already up-to-date.")
	exit 0
esac

git commit -m 'Merge back from svn'
