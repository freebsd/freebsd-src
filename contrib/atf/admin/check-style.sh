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
# A utility to sanity check the coding style of all source files in the
# project tree.
#

Prog_Name=${0##*/}

#
# err message
#
err() {
    echo "${Prog_Name}: ${@}" 1>&2
    exit 1
}

#
# warn message
#
warn() {
    echo "${Prog_Name}: ${@}" 1>&2
}

#
# guess_topdir
#
# Locates the project's top directory and prints its path.  The caller
# must verify if the result is correct or not.
#
guess_topdir() {
    olddir=$(pwd)
    topdir=$(pwd)
    while [ ${topdir} != / ]; do
        if [ -f ./atf-c.h ]; then
            break
        else
            cd ..
            topdir=$(pwd)
        fi
    done
    cd ${olddir}
    echo ${topdir}
}

#
# find_sources
#
# Locates all source files within the project, relative to the current
# directory, and prints their paths.
#
find_sources() {
    find . \( -name "AUTHORS" -o \
              -name "COPYING" -o \
              -name "ChangeLog" -o \
              -name "NEWS" -o \
              -name "README" -o \
              -name "TODO" -o \
              -name "*.[0-9]" -o \
              -name "*.ac" -o \
              -name "*.at" -o \
              -name "*.awk" -o \
              -name "*.c" -o \
              -name "*.cpp" -o \
              -name "*.h" -o \
              -name "*.h.in" -o \
              -name "*.hpp" -o \
              -name "*.m4" -o \
              -name "*.sh" \
           \) -a \( \
              \! -path "*/atf-[0-9]*" -a \
              \! -path "*autom4te*" -a \
              -type f -a \
              \! -name "aclocal.m4" \
              \! -name "bconfig.h" \
              \! -name "defs.h" \
              \! -name "libtool.m4" \
              \! -name "ltoptions.m4" \
              \! -name "ltsugar.m4" \
              \! -name "lt~obsolete.m4" \
              \! -name "*.so.*" \
           \)
}

#
# guess_formats file
#
# Guesses the formats applicable to the given file and prints the resulting
# list.
#
guess_formats() {
    case ${1} in
        */ltmain.sh)
            ;;
        *.[0-9])
            echo common man
            ;;
        *.c|*.h)
            echo common c
            ;;
        *.cpp|*.hpp)
            echo common cpp
            ;;
        *.sh)
            echo common shell
            ;;
        *)
            echo common
            ;;
    esac
}

#
# check_file file
#
# Checks the validity of the given file.
#
check_file() {
    err=0
    for format in $(guess_formats ${1}); do
        awk -f ${topdir}/admin/check-style-${format}.awk ${1} || err=1
    done

    return ${err}
}

#
# main [file list]
#
# Entry point.
#
main() {
    topdir=$(guess_topdir)
    if [ ! -f ${topdir}/atf-c.h ]; then
        err "Could not locate the project's top directory"
    fi

    if [ ${#} -gt 0 ]; then
        sources=${@}
    else
        cd ${topdir}
        sources=$(find_sources)
    fi

    ok=0
    for file in ${sources}; do
        file=$(echo ${file} | sed -e "s,\\./,,")

        if [ ! -f ${file} ]; then
            err "Could not open ${file}"
        else
            check_file ${file} || ok=1
        fi
    done

    return ${ok}
}

main "${@}"

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
