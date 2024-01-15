#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019-2021 Mark Johnston <markj@FreeBSD.org>
# Copyright (c) 2021 John Baldwin <jhb@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the distribution.
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
#

# TODO:
# - roll back after errors or SIGINT
#   - created revs
#   - main (for git arc stage)

warn()
{
    echo "$(basename "$0"): $1" >&2
}

err()
{
    warn "$1"
    exit 1
}

err_usage()
{
    cat >&2 <<__EOF__
Usage: git arc [-vy] <command> <arguments>

Commands:
  create [-l] [-r <reviewer1>[,<reviewer2>...]] [-s subscriber[,...]] [<commit>|<commit range>]
  list <commit>|<commit range>
  patch <diff1> [<diff2> ...]
  stage [-b branch] [<commit>|<commit range>]
  update [-m message] [<commit>|<commit range>]

Description:
  Create or manage FreeBSD Phabricator reviews based on git commits.  There
  is a one-to one relationship between git commits and Differential revisions,
  and the Differential revision title must match the summary line of the
  corresponding commit.  In particular, commit summaries must be unique across
  all open Differential revisions authored by you.

  The first parameter must be a verb.  The available verbs are:

    create -- Create new Differential revisions from the specified commits.
    list   -- Print the associated Differential revisions for the specified
              commits.
    patch  -- Try to apply a patch from a Differential revision to the
              currently checked out tree.
    stage  -- Prepare a series of commits to be pushed to the upstream FreeBSD
              repository.  The commits are cherry-picked to a branch (main by
              default), review tags are added to the commit log message, and
              the log message is opened in an editor for any last-minute
              updates.  The commits need not have associated Differential
              revisions.
    update -- Synchronize the Differential revisions associated with the
              specified commits.  Currently only the diff is updated; the
              review description and other metadata is not synchronized.

  The typical end-to-end usage looks something like this:

    $ git commit -m "kern: Rewrite in Rust"
    $ git arc create HEAD
    <Make changes to the diff based on reviewer feedback.>
    $ git commit --amend
    $ git arc update HEAD
    <Now that all reviewers are happy, it's time to push.>
    $ git arc stage HEAD
    $ git push freebsd HEAD:main

Config Variables:
  These are manipulated by git-config(1).

    arc.assume_yes [bool]
                       -- Assume a "yes" answer to all prompts instead of
                          prompting the user.  Equivalent to the -y flag.

    arc.browse [bool]  -- Try to open newly created reviews in a browser tab.
                          Defaults to false.

    arc.list [bool]    -- Always use "list mode" (-l) with create.  In this
                          mode, the list of git revisions to create reviews for
                          is listed with a single prompt before creating
                          reviews.  The diffs for individual commits are not
                          shown.

    arc.verbose [bool] -- Verbose output.  Equivalent to the -v flag.

Examples:
  Create a Phabricator review using the contents of the most recent commit in
  your git checkout.  The commit title is used as the review title, the commit
  log message is used as the review description, markj@FreeBSD.org is added as
  a reviewer.

  $ git arc create -r markj HEAD

  Create a series of Phabricator reviews for each of HEAD~2, HEAD~ and HEAD.
  Pairs of consecutive commits are linked into a patch stack.  Note that the
  first commit in the specified range is excluded.

  $ git arc create HEAD~3..HEAD

  Update the review corresponding to commit b409afcfedcdda.  The title of the
  commit must be the same as it was when the review was created.  The review
  description is not automatically updated.

  $ git arc update b409afcfedcdda

  Apply the patch in review D12345 to the currently checked-out tree, and stage
  it.

  $ git arc patch D12345

  List the status of reviews for all the commits in the branch "feature":

  $ git arc list main..feature

__EOF__

    exit 1
}

#
# Filter the output of arc list to remove the warnings as above, as well as any
# stray escape sequences that are in the list (it interferes with the parsing)
#
arc_list()
{
    arc list "$@" | sed 's/\x1b\[[0-9;]*m//g' | grep -v '^Warning: '
}

diff2phid()
{
    local diff

    diff=$1
    if ! expr "$diff" : 'D[1-9][0-9]*$' >/dev/null; then
        err "invalid diff ID $diff"
    fi

    echo '{"names":["'"$diff"'"]}' |
        arc call-conduit -- phid.lookup |
        jq -r "select(.response != []) | .response.${diff}.phid"
}

diff2status()
{
    local diff tmp status summary

    diff=$1
    if ! expr "$diff" : 'D[1-9][0-9]*$' >/dev/null; then
        err "invalid diff ID $diff"
    fi

    tmp=$(mktemp)
    echo '{"names":["'"$diff"'"]}' |
        arc call-conduit -- phid.lookup > "$tmp"
    status=$(jq -r "select(.response != []) | .response.${diff}.status" < "$tmp")
    summary=$(jq -r "select(.response != []) |
         .response.${diff}.fullName" < "$tmp")
    printf "%-14s %s\n" "${status}" "${summary}"
}

log2diff()
{
    local diff

    diff=$(git show -s --format=%B "$commit" |
        sed -nE '/^Differential Revision:[[:space:]]+(https:\/\/reviews.freebsd.org\/)?(D[0-9]+)$/{s//\2/;p;}')
    if [ -n "$diff" ] && [ "$(echo "$diff" | wc -l)" -eq 1 ]; then
        echo "$diff"
    else
        echo
    fi
}

# Look for an open revision with a title equal to the input string.  Return
# a possibly empty list of Differential revision IDs.
title2diff()
{
    local title

    title=$(echo $1 | sed 's/"/\\"/g')
    arc_list |
        awk -F': ' '{
            if (substr($0, index($0, FS) + length(FS)) == "'"$title"'") {
                print substr($1, match($1, "D[1-9][0-9]*"))
            }
        }'
}

commit2diff()
{
    local commit diff title

    commit=$1

    # First, look for a valid differential reference in the commit
    # log.
    diff=$(log2diff "$commit")
    if [ -n "$diff" ]; then
        echo "$diff"
        return
    fi

    # Second, search the open reviews returned by 'arc list' looking
    # for a subject match.
    title=$(git show -s --format=%s "$commit")
    diff=$(title2diff "$title")
    if [ -z "$diff" ]; then
        err "could not find review for '${title}'"
    elif [ "$(echo "$diff" | wc -l)" -ne 1 ]; then
        err "found multiple reviews with the same title"
    fi

    echo "$diff"
}

create_one_review()
{
    local childphid commit doprompt msg parent parentphid reviewers
    local subscribers

    commit=$1
    reviewers=$2
    subscribers=$3
    parent=$4
    doprompt=$5

    if [ "$doprompt" ] && ! show_and_prompt "$commit"; then
        return 1
    fi

    msg=$(mktemp)
    git show -s --format='%B' "$commit" > "$msg"
    printf "\nTest Plan:\n" >> "$msg"
    printf "\nReviewers:\n" >> "$msg"
    printf "%s\n" "${reviewers}" >> "$msg"
    printf "\nSubscribers:\n" >> "$msg"
    printf "%s\n" "${subscribers}" >> "$msg"

    yes | env EDITOR=true \
        arc diff --message-file "$msg" --never-apply-patches --create \
        --allow-untracked $BROWSE --head "$commit" "${commit}~"
    [ $? -eq 0 ] || err "could not create Phabricator diff"

    if [ -n "$parent" ]; then
        diff=$(commit2diff "$commit")
        [ -n "$diff" ] || err "failed to look up review ID for $commit"

        childphid=$(diff2phid "$diff")
        parentphid=$(diff2phid "$parent")
        echo '{
            "objectIdentifier": "'"${childphid}"'",
            "transactions": [
                {
                    "type": "parents.add",
                    "value": ["'"${parentphid}"'"]
                }
             ]}' |
            arc call-conduit -- differential.revision.edit >&3
    fi
    rm -f "$msg"
    return 0
}

# Get a list of reviewers who accepted the specified diff.
diff2reviewers()
{
    local diff reviewid userids

    diff=$1
    reviewid=$(diff2phid "$diff")
    userids=$( \
        echo '{
                  "constraints": {"phids": ["'"$reviewid"'"]},
                  "attachments": {"reviewers": true}
              }' |
        arc call-conduit -- differential.revision.search |
        jq '.response.data[0].attachments.reviewers.reviewers[] | select(.status == "accepted").reviewerPHID')
    if [ -n "$userids" ]; then
        echo '{
                  "constraints": {"phids": ['"$(echo -n "$userids" | tr '[:space:]' ',')"']}
              }' |
            arc call-conduit -- user.search |
            jq -r '.response.data[].fields.username'
    fi
}

prompt()
{
    local resp

    if [ "$ASSUME_YES" ]; then
        return 0
    fi

    printf "\nDoes this look OK? [y/N] "
    read -r resp

    case $resp in
    [Yy])
        return 0
        ;;
    *)
        return 1
        ;;
    esac
}

show_and_prompt()
{
    local commit

    commit=$1

    git show "$commit"
    prompt
}

build_commit_list()
{
    local chash _commits commits

    for chash in "$@"; do
        _commits=$(git rev-parse "${chash}")
        if ! git cat-file -e "${chash}"'^{commit}' >/dev/null 2>&1; then
            # shellcheck disable=SC2086
            _commits=$(git rev-list $_commits | tail -r)
        fi
        [ -n "$_commits" ] || err "invalid commit ID ${chash}"
        commits="$commits $_commits"
    done
    echo "$commits"
}

gitarc__create()
{
    local commit commits doprompt list o prev reviewers subscribers

    list=
    prev=""
    if [ "$(git config --bool --get arc.list 2>/dev/null || echo false)" != "false" ]; then
        list=1
    fi
    doprompt=1
    while getopts lp:r:s: o; do
        case "$o" in
        l)
            list=1
            ;;
        p)
            prev="$OPTARG"
            ;;
        r)
            reviewers="$OPTARG"
            ;;
        s)
            subscribers="$OPTARG"
            ;;
        *)
            err_usage
            ;;
        esac
    done
    shift $((OPTIND-1))

    commits=$(build_commit_list "$@")

    if [ "$list" ]; then
        for commit in ${commits}; do
            git --no-pager show --oneline --no-patch "$commit"
        done | git_pager
        if ! prompt; then
            return
        fi
        doprompt=
    fi

    for commit in ${commits}; do
        if create_one_review "$commit" "$reviewers" "$subscribers" "$prev" \
                             "$doprompt"; then
            prev=$(commit2diff "$commit")
        else
            prev=""
        fi
    done
}

gitarc__list()
{
    local chash commit commits diff openrevs title

    commits=$(build_commit_list "$@")
    openrevs=$(arc_list)

    for commit in $commits; do
        chash=$(git show -s --format='%C(auto)%h' "$commit")
        echo -n "${chash} "

        diff=$(log2diff "$commit")
        if [ -n "$diff" ]; then
                diff2status "$diff"
                continue
        fi

        # This does not use commit2diff as it needs to handle errors
        # differently and keep the entire status.
        title=$(git show -s --format=%s "$commit")
        diff=$(echo "$openrevs" | \
            awk -F'D[1-9][0-9]*: ' \
                '{if ($2 == "'"$(echo $title | sed 's/"/\\"/g')"'") print $0}')
        if [ -z "$diff" ]; then
            echo "No Review      : $title"
        elif [ "$(echo "$diff" | wc -l)" -ne 1 ]; then
            echo -n "Ambiguous Reviews: "
            echo "$diff" | grep -E -o 'D[1-9][0-9]*:' | tr -d ':' \
                | paste -sd ',' - | sed 's/,/, /g'
        else
            echo "$diff" | sed -e 's/^[^ ]* *//'
        fi
    done
}

gitarc__patch()
{
    local rev

    if [ $# -eq 0 ]; then
        err_usage
    fi

    for rev in "$@"; do
        arc patch --skip-dependencies --nocommit --nobranch --force "$rev"
        echo "Applying ${rev}..."
        [ $? -eq 0 ] || break
    done
}

gitarc__stage()
{
    local author branch commit commits diff reviewers title tmp

    branch=main
    while getopts b: o; do
        case "$o" in
        b)
            branch="$OPTARG"
            ;;
        *)
            err_usage
            ;;
        esac
    done
    shift $((OPTIND-1))

    commits=$(build_commit_list "$@")

    if [ "$branch" = "main" ]; then
        git checkout -q main
    else
        git checkout -q -b "${branch}" main
    fi

    tmp=$(mktemp)
    for commit in $commits; do
        git show -s --format=%B "$commit" > "$tmp"
        title=$(git show -s --format=%s "$commit")
        diff=$(title2diff "$title")
        if [ -n "$diff" ]; then
            # XXX this leaves an extra newline in some cases.
            reviewers=$(diff2reviewers "$diff" | sed '/^$/d' | paste -sd ',' - | sed 's/,/, /g')
            if [ -n "$reviewers" ]; then
                printf "Reviewed by:\t%s\n" "${reviewers}" >> "$tmp"
            fi
            printf "Differential Revision:\thttps://reviews.freebsd.org/%s" "${diff}" >> "$tmp"
        fi
        author=$(git show -s --format='%an <%ae>' "${commit}")
        if ! git cherry-pick --no-commit "${commit}"; then
            warn "Failed to apply $(git rev-parse --short "${commit}").  Are you staging patches in the wrong order?"
            git checkout -f
            break
        fi
        git commit --edit --file "$tmp" --author "${author}"
    done
}

gitarc__update()
{
    local commit commits diff have_msg msg

    while getopts m: o; do
        case "$o" in
        m)
            msg="$OPTARG"
            have_msg=1
            ;;
        *)
            err_usage
            ;;
        esac
    done
    shift $((OPTIND-1))

    commits=$(build_commit_list "$@")
    for commit in ${commits}; do
        diff=$(commit2diff "$commit")

        if ! show_and_prompt "$commit"; then
            break
        fi

        # The linter is stupid and applies patches to the working copy.
        # This would be tolerable if it didn't try to correct "misspelled" variable
        # names.
        if [ -n "$have_msg" ]; then
            arc diff --message "$msg" --allow-untracked --never-apply-patches \
                --update "$diff" --head "$commit" "${commit}~"
        else
            arc diff --allow-untracked --never-apply-patches --update "$diff" \
                --head "$commit" "${commit}~"
        fi
    done
}

set -e

ASSUME_YES=
if [ "$(git config --bool --get arc.assume-yes 2>/dev/null || echo false)" != "false" ]; then
    ASSUME_YES=1
fi

VERBOSE=
while getopts vy o; do
    case "$o" in
    v)
        VERBOSE=1
        ;;
    y)
        ASSUME_YES=1
        ;;
    *)
        err_usage
        ;;
    esac
done
shift $((OPTIND-1))

[ $# -ge 1 ] || err_usage

which arc >/dev/null 2>&1 || err "arc is required, install devel/arcanist"
which jq >/dev/null 2>&1 || err "jq is required, install textproc/jq"

if [ "$VERBOSE" ]; then
    exec 3>&1
else
    exec 3> /dev/null
fi

case "$1" in
create|list|patch|stage|update)
    ;;
*)
    err_usage
    ;;
esac
verb=$1
shift

# All subcommands require at least one parameter.
if [ $# -eq 0 ]; then
    err_usage
fi

# Pull in some git helper functions.
git_sh_setup=$(git --exec-path)/git-sh-setup
[ -f "$git_sh_setup" ] || err "cannot find git-sh-setup"
SUBDIRECTORY_OK=y
USAGE=
# shellcheck disable=SC1090
. "$git_sh_setup"

# git commands use GIT_EDITOR instead of EDITOR, so try to provide consistent
# behaviour.  Ditto for PAGER.  This makes git-arc play nicer with editor
# plugins like vim-fugitive.
if [ -n "$GIT_EDITOR" ]; then
    EDITOR=$GIT_EDITOR
fi
if [ -n "$GIT_PAGER" ]; then
    PAGER=$GIT_PAGER
fi

# Bail if the working tree is unclean, except for "list" and "patch"
# operations.
case $verb in
list|patch)
    ;;
*)
    require_clean_work_tree "$verb"
    ;;
esac

if [ "$(git config --bool --get arc.browse 2>/dev/null || echo false)" != "false" ]; then
    BROWSE=--browse
fi

gitarc__"${verb}" "$@"
