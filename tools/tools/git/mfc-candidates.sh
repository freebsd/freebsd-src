#!/bin/sh

#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2022 The FreeBSD Foundation
#
# This software was developed by Ed Maste
# under sponsorship from the FreeBSD Foundation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted providing that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

from_branch=freebsd/main
author="${USER}"

# Get the FreeBSD repository
repo=$(basename "$(git remote get-url freebsd 2>/dev/null)" 2>/dev/null)

if [ "${repo}" = "ports.git" ]; then
	year=$(date '+%Y')
	month=$(date '+%m')
	qtr=$(((month-1) / 3 + 1))
	to_branch="freebsd/${year}Q${qtr}"
elif [ "${repo}" = "src.git" ]; then
	to_branch=freebsd/stable/13
	# If pwd is a stable or release branch tree, default to it.
	cur_branch=$(git symbolic-ref --short HEAD 2>/dev/null)
	case $cur_branch in
	stable/*)
		to_branch=$cur_branch
		;;
	releng/*)
		to_branch=$cur_branch
		major=${cur_branch#releng/}
		major=${major%.*}
		from_branch=freebsd/stable/$major
	esac
else
	echo "pwd is not under a ports or src repository."
	exit 0
fi

params()
{
	echo "from:             $from_branch"
	echo "to:               $to_branch"
	if [ -n "$author" ]; then
		echo "author/committer: $author"
	else
		echo "author/committer: <all>"
	fi
}

usage()
{
	echo "usage: $(basename $0) [-ah] [-f from_branch] [-t to_branch] [-u user] [-X exclude_file] [path ...]"
	echo
	params
	exit 0
}

while getopts "af:ht:u:vX:" opt; do
	case $opt in
	a)
		# All authors/committers
		author=
		;;
	f)
		from_branch=$OPTARG
		;;
	h)
		usage
		;;
	t)
		to_branch=$OPTARG
		;;
	u)
		author=$OPTARG
		;;
	v)
		verbose=1
		;;
	X)
		if [ ! -r "$OPTARG" ]; then
			echo "Exclude file $OPTARG not readable" >&2
			exit 1
		fi
		exclude_file=$OPTARG
		;;
	esac
done
shift $(($OPTIND - 1))

if [ $verbose ]; then
	params
	echo
fi

authorarg=
if [ -n "$author" ]; then
	# Match user ID in the email portion of author or committer
	authorarg="--author <${author}@ --committer <${author}@"
fi

# Commits in from_branch after branch point
commits_from()
{
	git rev-list --first-parent $authorarg $to_branch..$from_branch "$@" |\
	    sort
}

# "cherry picked from" hashes from commits in to_branch after branch point
commits_to()
{
	git log $from_branch..$to_branch --grep 'cherry picked from' "$@" |\
	    sed -E -n 's/^[[:space:]]*\(cherry picked from commit ([0-9a-f]+)\)[[:space:]]*$/\1/p' |\
	    sort
}

# Turn a list of short hashes (and optional descriptions) into a list of full
# hashes.
canonicalize_hashes()
{
	while read hash rest; do
		if ! git show --pretty=%H --no-patch $hash; then
			echo "error parsing hash list" >&2
			exit 1
		fi
	done | sort
}

workdir=$(mktemp -d /tmp/find-mfc.XXXXXXXXXX)
from_list=$workdir/commits-from
to_list=$workdir/commits-to
candidate_list=$workdir/candidates

if [ -n "$exclude_file" ]; then
	exclude_list=$workdir/commits-exclude
	canonicalize_hashes < $exclude_file > $exclude_list
fi

commits_from "$@" > $from_list
commits_to "$@" > $to_list

comm -23 $from_list $to_list > $candidate_list

if [ -n "$exclude_file" ]; then
	mv $candidate_list $candidate_list.bak
	comm -23 $candidate_list.bak $exclude_list > $candidate_list
fi

# Sort by (but do not print) commit time
while read hash; do
	git show --pretty='%ct %h %s' --no-patch $hash
done < $candidate_list | sort -n | cut -d ' ' -f 2-

rm -rf "$workdir"
