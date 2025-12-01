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

cleanup()
{
    rc=$?
    rm -fr "$GITARC_TMPDIR"
    trap - EXIT
    exit $rc
}

err_usage()
{
    cat >&2 <<__EOF__
Usage: git arc [-vy] <command> <arguments>

Commands:
  create [-l] [-r <reviewer1>[,<reviewer2>...]] [-s subscriber[,...]] [<commit>|<commit range>]
  list <commit>|<commit range>
  patch [-bc] <diff1> [<diff2> ...]
  stage [-b branch] [<commit>|<commit range>]
  update [-l] [-m message] [<commit>|<commit range>]

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
    patch  -- Apply patches from Differential revisions.  By default, patches
              are applied to the currently checked-out tree, unless -b is
              supplied, in which case a new branch is first created.  The -c
              option commits the applied patch using the review's metadata.
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

    arc.list [bool]    -- Always use "list mode" (-l) with create and update.
                          In this mode, the list of git revisions to use
                          is listed with a single prompt before creating or
                          updating reviews.  The diffs for individual commits
                          are not shown.

    arc.verbose [bool] -- Verbose output.  Equivalent to the -v flag.

Examples:
  Create a Phabricator review using the contents of the most recent commit in
  your git checkout.  The commit title is used as the review title, the commit
  log message is used as the review description, markj@FreeBSD.org is added as
  a reviewer. Also, the "Jails" reviewer group is added using its hashtag.

  $ git arc create -r markj,#jails HEAD

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

  Apply the patch in review D12345 to the currently checked-out tree, and
  commit it using the review's title, summary and author.

  $ git arc patch -c D12345

  Apply the patches in reviews D12345 and D12346 in a new branch, and commit
  them using the review titles, summaries and authors.

  $ git arc patch -bc D12345 D12346

  List the status of reviews for all the commits in the branch "feature":

  $ git arc list main..feature

__EOF__

    exit 1
}

# Use xmktemp instead of mktemp when creating temporary files.
xmktemp()
{
    mktemp "${GITARC_TMPDIR:?}/tmp.XXXXXXXXXX" || exit 1
}

#
# Fetch the value of a boolean config variable ($1) and return true
# (0) if the variable is true.  The default value to use if the
# variable is not set is passed in $2.
#
get_bool_config()
{
    test "$(git config --bool --get $1 2>/dev/null || echo $2)" != "false"
}

#
# Invoke the actual arc command.  This allows us to only rely on the
# devel/arcanist-lib port, which installs the actual script, rather than
# the devel/arcanist-port, which installs a symlink in ${LOCALBASE}/bin
# but conflicts with the archivers/arc port.
#
: ${LOCALBASE:=$(sysctl -n user.localbase)}
: ${LOCALBASE:=/usr/local}
: ${ARC_CMD:=${LOCALBASE}/lib/php/arcanist/bin/arc}
arc()
{
    ${ARC_CMD} "$@"
}

#
# Filter the output of call-conduit to remove the warnings that are generated
# for some installations where openssl module is mysteriously installed twice so
# a warning is generated. It's likely a local config error, but we should work
# in the face of that.
#
arc_call_conduit()
{
    arc call-conduit "$@" | grep -v '^Warning: '
}

#
# Filter the output of arc list to remove the warnings as above, as well as
# the bolding sequence (the color sequence remains intact).
#
arc_list()
{
    arc list "$@" | grep -v '^Warning: ' | sed -E 's/\x1b\[1m//g;s/\x1b\[m//g'
}

diff2phid()
{
    local diff

    diff=$1
    if ! expr "$diff" : 'D[1-9][0-9]*$' >/dev/null; then
        err "invalid diff ID $diff"
    fi

    echo '{"names":["'"$diff"'"]}' |
        arc_call_conduit -- phid.lookup |
        jq -r "select(.response != []) | .response.${diff}.phid"
}

diff2status()
{
    local diff tmp status summary

    diff=$1
    if ! expr "$diff" : 'D[1-9][0-9]*$' >/dev/null; then
        err "invalid diff ID $diff"
    fi

    tmp=$(xmktemp)
    echo '{"names":["'"$diff"'"]}' |
        arc_call_conduit -- phid.lookup > "$tmp"
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

    title=$(echo "$1" | sed 's/"/\\"/g')
    arc_list --no-ansi |
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

    msg=$(xmktemp)
    git show -s --format='%B' "$commit" > "$msg"
    printf "\nTest Plan:\n" >> "$msg"
    printf "\nReviewers:\n" >> "$msg"
    printf "%s\n" "${reviewers}" >> "$msg"
    printf "\nSubscribers:\n" >> "$msg"
    printf "%s\n" "${subscribers}" >> "$msg"

    yes | EDITOR=true \
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
            arc_call_conduit -- differential.revision.edit >&3
    fi
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
        arc_call_conduit -- differential.revision.search |
        jq '.response.data[0].attachments.reviewers.reviewers[] | select(.status == "accepted").reviewerPHID')
    if [ -n "$userids" ]; then
        echo '{
        "constraints": {"phids": ['"$(echo $userids | tr '[:blank:]' ',')"']}
        }' |
        arc_call_conduit -- user.search |
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
            _commits=$(git rev-list --reverse $_commits)
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
    if get_bool_config arc.list false; then
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
    openrevs=$(arc_list --ansi)

    for commit in $commits; do
        chash=$(git show -s --format='%C(auto)%h' "$commit")
        printf "%s" "${chash} "

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
            '{if ($2 == "'"$(echo "$title" | sed 's/"/\\"/g')"'") print $0}')
        if [ -z "$diff" ]; then
            echo "No Review            : $title"
        elif [ "$(echo "$diff" | wc -l)" -ne 1 ]; then
            printf "%s" "Ambiguous Reviews: "
            echo "$diff" | grep -E -o 'D[1-9][0-9]*:' | tr -d ':' \
                | paste -sd ',' - | sed 's/,/, /g'
        else
            echo "$diff" | sed -e 's/^[^ ]* *//'
        fi
    done
}

# Try to guess our way to a good author name. The DWIM is strong in this
# function, but these heuristics seem to generally produce the right results, in
# the sample of src commits I checked out.
find_author()
{
    local addr name email author_addr author_name

    addr="$1"
    name="$2"
    author_addr="$3"
    author_name="$4"

    # The Phabricator interface doesn't have a simple way to get author name and
    # address, so we have to try a number of heuristics to get the right result.

    # Choice 1: It's a FreeBSD committer. These folks have no '.' in their phab
    # username/addr. Sampled data in phab suggests that there's a high rate of
    # these people having their local config pointing at something other than
    # freebsd.org (which isn't surprising for ports committers getting src
    # commits reviewed).
    case "${addr}" in
    *.*) ;;             # external user
    *)
        echo "${name} <${addr}@FreeBSD.org>"
        return
        ;;
    esac

    # Choice 2: author_addr and author_name were set in the bundle, so use
    # that. We may need to filter some known bogus ones, should they crop up.
    if [ -n "$author_name" -a -n "$author_addr" ]; then
        echo "${author_name} <${author_addr}>"
        return
    fi

    # Choice 3: We can find this user in the FreeBSD repo. They've submited
    # something before, and they happened to use an email that's somewhat
    # similar to their phab username.
    email=$(git log -1 --author "$(echo ${addr} | tr _ .)" --pretty="%aN <%aE>")
    if [ -n "${email}" ]; then
        echo "${email}"
        return
    fi

    # Choice 4: We know this user. They've committed before, and they happened
    # to use the same name, unless the name has the word 'user' in it. This
    # might not be a good idea, since names can be somewhat common (there
    # are two Andrew Turners that have contributed to FreeBSD, for example).
    if ! (echo "${name}" | grep -w "[Uu]ser" -q); then
        email=$(git log -1 --author "${name}" --pretty="%aN <%aE>")
        if [ -n "$email" ]; then
            echo "$email"
            return
        fi
    fi

    # Choice 5: Wing it as best we can. In this scenario, we replace the last _
    # with a @, and call it the email address...
    # Annoying fun fact: Phab replaces all non alpha-numerics with _, so we
    # don't know if the prior _ are _ or + or any number of other characters.
    # Since there's issues here, prompt
    a=$(printf "%s <%s>\n" "${name}" $(echo "$addr" | sed -e 's/\(.*\)_/\1@/'))
    echo "Making best guess: Turning ${addr} to ${a}" >&2
    if ! prompt; then
        echo "ABORT"
        return
    fi
    echo "${a}"
}

patch_branch()
{
    local base new suffix

    if [ $# -eq 1 ]; then
        base="gitarc-$1"
    else
        base="gitarc-$(printf "%s-" "$@" | sed 's/-$//')"
    fi

    new="$base"
    suffix=1
    while git show-ref --quiet --branches "$new"; do
        new="${base}_$suffix"
        suffix=$((suffix + 1))
    done

    git checkout -b "$new"
}

patch_commit()
{
    local diff reviewid review_data authorid user_data user_addr user_name
    local diff_data author_addr author_name author tmp

    diff=$1
    reviewid=$(diff2phid "$diff")
    # Get the author phid for this patch
    review_data=$(xmktemp)
    echo '{"constraints": {"phids": ["'"$reviewid"'"]}}' | \
        arc_call_conduit -- differential.revision.search > "$review_data"
    authorid=$(jq -r '.response.data[].fields.authorPHID' "$review_data")
    # Get metadata about the user that submitted this patch
    user_data=$(xmktemp)
    echo '{"constraints": {"phids": ["'"$authorid"'"]}}' | \
        arc_call_conduit -- user.search | \
        jq -r '.response.data[].fields' > "$user_data"
    user_addr=$(jq -r '.username' "$user_data")
    user_name=$(jq -r '.realName' "$user_data")
    # Dig the data out of querydiffs api endpoint, although it's deprecated,
    # since it's one of the few places we can get email addresses. It's unclear
    # if we can expect multiple difference ones of these. Some records don't
    # have this data, so we remove all the 'null's. We sort the results and
    # remove duplicates 'just to be sure' since we've not seen multiple
    # records that match.
    diff_data=$(xmktemp)
    echo '{"revisionIDs": [ '"${diff#D}"' ]}' | \
        arc_call_conduit -- differential.querydiffs |
        jq -r '.response | flatten | .[]' > "$diff_data"
    # If the differential revision has multiple revisions, just take the first
    # non-null value we get.
    author_addr=$(jq -r ".authorEmail?" "$diff_data" | grep -v '^null$' | head -n 1)
    author_name=$(jq -r ".authorName?" "$diff_data" | grep -v '^null$' | head -n 1)

    author=$(find_author "$user_addr" "$user_name" "$author_addr" "$author_name")

    # If we had to guess, and the user didn't want to guess, abort
    if [ "${author}" = "ABORT" ]; then
        warn "Not committing due to uncertainty over author name"
        exit 1
    fi

    tmp=$(xmktemp)
    jq -r '.response.data[].fields.title' "$review_data" > "$tmp"
    echo >> "$tmp"
    jq -r '.response.data[].fields.summary' "$review_data" >> "$tmp"
    echo >> "$tmp"
    # XXX this leaves an extra newline in some cases.
    reviewers=$(diff2reviewers "$diff" | sed '/^$/d' | paste -sd ',' - | sed 's/,/, /g')
    if [ -n "$reviewers" ]; then
        printf "Reviewed by:\t%s\n" "${reviewers}" >> "$tmp"
    fi
    # XXX TODO refactor with gitarc__stage maybe?
    printf "Differential Revision:\thttps://reviews.freebsd.org/%s\n" "${diff}" >> "$tmp"
    git commit --author "${author}" --file "$tmp"
}

gitarc__patch()
{
    local branch commit rev

    branch=false
    commit=false
    while getopts bc o; do
        case "$o" in
        b)
            require_clean_work_tree "patch -b"
            branch=true
            ;;
        c)
            require_clean_work_tree "patch -c"
            commit=true
            ;;
        *)
            err_usage
            ;;
        esac
    done
    shift $((OPTIND-1))

    if [ $# -eq 0 ]; then
        err_usage
    fi

    if ${branch}; then
        patch_branch "$@"
    fi
    for rev in "$@"; do
        if ! arc patch --skip-dependencies --nobranch --nocommit --force "$rev"; then
            break
        fi
        echo "Applying ${rev}..."
        if ${commit}; then
            patch_commit $rev
        fi
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

    tmp=$(xmktemp)
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
    local commit commits diff doprompt have_msg list o msg

    list=
    if get_bool_config arc.list false; then
        list=1
    fi
    doprompt=1
    while getopts lm: o; do
        case "$o" in
        l)
            list=1
            ;;
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
        diff=$(commit2diff "$commit")

        if [ "$doprompt" ] && ! show_and_prompt "$commit"; then
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
if get_bool_config arc.assume-yes false; then
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

[ -x "${ARC_CMD}" ] || err "arc is required, install devel/arcanist-lib"
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

if get_bool_config arc.browse false; then
    BROWSE=--browse
fi

GITARC_TMPDIR=$(mktemp -d) || exit 1
trap cleanup EXIT HUP INT QUIT TRAP USR1 TERM

gitarc__"${verb}" "$@"
