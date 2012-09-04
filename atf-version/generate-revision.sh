#! /bin/sh
#
# Automated Testing Framework (atf)
#
# Copyright (c) 2007 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# Generates a header file with information about the revision used to
# build ATF.
#

set -e

Prog_Name=${0##*/}

GIT=
ROOT=

#
# err message
#
err() {
    echo "${Prog_Name}: ${@}" 1>&2
    exit 1
}

#
# call_git args
#
call_git() {
    ( cd "${ROOT}" && "${GIT}" "${@}" )
}

#
# generate_from_dist revfile version
#
generate_from_dist() {
    revfile=${1}; shift
    version=${1}; shift

    >${revfile}

    echo "#define PACKAGE_REVISION_TYPE_DIST" >>${revfile}
}

#
# generate_from_git revfile
#
generate_from_git() {
    revfile=${1}

    rev_base_id=$(call_git rev-parse HEAD)
    rev_branch=$(call_git branch | grep '^\* ' | cut -d ' ' -f 2-)
    rev_date=$(call_git log -1 | grep '^Date:' | sed -e 's,^Date:[ \t]*,,')
    if [ -z "$(call_git status -s)" ]; then
        rev_modified=false
    else
        rev_modified=true
    fi

    >${revfile}

    echo "#define PACKAGE_REVISION_TYPE_GIT" >>${revfile}

    echo "#define PACKAGE_REVISION_BRANCH \"${rev_branch}\"" >>${revfile}
    echo "#define PACKAGE_REVISION_BASE \"${rev_base_id}\"" >>${revfile}

    if [ ${rev_modified} = true ]; then
        echo "#define PACKAGE_REVISION_MODIFIED 1" >>${revfile}
    fi

    echo "#define PACKAGE_REVISION_DATE \"${rev_date}\"" >>${revfile}
}

#
# main
#
# Entry point.
#
main() {
    outfile=
    version=
    while getopts :g:r:o:v: arg; do
        case ${arg} in
            g)
                GIT=${OPTARG}
                ;;
            o)
                outfile=${OPTARG}
                ;;
            r)
                ROOT=${OPTARG}
                ;;
            v)
                version=${OPTARG}
                ;;
            *)
                err "Unknown option ${arg}"
                ;;
        esac
    done
    [ -n "${ROOT}" ] || \
        err "Must specify the top-level source directory with -r"
    [ -n "${outfile}" ] || \
        err "Must specify an output file with -o"
    [ -n "${version}" ] || \
        err "Must specify a version number with -v"

    if [ -n "${GIT}" -a -d ${ROOT}/.git ]; then
        generate_from_git ${outfile}
    else
        generate_from_dist ${outfile} ${version}
    fi
}

main "${@}"

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
