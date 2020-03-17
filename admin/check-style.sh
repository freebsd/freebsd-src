#! /bin/sh
# Copyright 2011 The Kyua Authors.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# \file admin/check-style.sh
#
# Sanity checks the coding style of all source files in the project tree.

ProgName="${0##*/}"


# Prints an error message and exits.
#
# \param ... Parts of the error message; concatenated using a space as the
#     separator.
err() {
    echo "${ProgName}:" "${@}" 1>&2
    exit 1
}


# Locates all source files within the project directory.
#
# We require the project to have been configured in a directory that is separate
# from the source tree.   This is to allow us to easily filter out build
# artifacts from our search.
#
# \param srcdir Absolute path to the source directory.
# \param builddir Absolute path to the build directory.
# \param tarname Basename of the project's tar file, to skip possible distfile
#     directories.
find_sources() {
    local srcdir="${1}"; shift
    local builddir="${1}"; shift
    local tarname="${1}"; shift

    (
        cd "${srcdir}"
        find . -type f -a \
                  \! -path "*/.git/*" \
                  \! -path "*/.deps/*" \
                  \! -path "*/autom4te.cache/*" \
                  \! -path "*/${tarname}-[0-9]*/*" \
                  \! -path "*/${builddir##*/}/*" \
                  \! -name "Makefile.in" \
                  \! -name "aclocal.m4" \
                  \! -name "config.h.in" \
                  \! -name "configure" \
                  \! -name "testsuite"
    )
}


# Prints the style rules applicable to a given file.
#
# \param file Path to the source file.
guess_rules() {
    local file="${1}"; shift

    case "${file}" in
        */ax_cxx_compile_stdcxx.m4) ;;
        */ltmain.sh) ;;
        *Makefile*) echo common make ;;
        *.[0-9]) echo common man ;;
        *.cpp|*.hpp) echo common cpp ;;
        *.sh) echo common shell ;;
        *) echo common ;;
    esac
}


# Validates a given file against the rules that apply to it.
#
# \param srcdir Absolute path to the source directory.
# \param file Name of the file to validate relative to srcdir.
#
# \return 0 if the file is valid; 1 otherwise, in which case the style
# violations are printed to the output.
check_file() {
    local srcdir="${1}"; shift
    local file="${1}"; shift

    local err=0
    for rule in $(guess_rules "${file}"); do
        awk -f "${srcdir}/admin/check-style-${rule}.awk" \
            "${srcdir}/${file}" || err=1
    done

    return ${err}
}


# Entry point.
main() {
    local builddir=.
    local srcdir=.
    local tarname=UNKNOWN

    local arg
    while getopts :b:s:t: arg; do
        case "${arg}" in
            b)
                builddir="${OPTARG}"
                ;;

            s)
                srcdir="${OPTARG}"
                ;;

            t)
                tarname="${OPTARG}"
                ;;

            \?)
                err "Unknown option -${OPTARG}"
                ;;
        esac
    done
    shift $(expr ${OPTIND} - 1)

    srcdir="$(cd "${srcdir}" && pwd -P)"
    builddir="$(cd "${builddir}" && pwd -P)"
    [ "${srcdir}" != "${builddir}" ] || \
        err "srcdir and builddir cannot match; reconfigure the package" \
            "in a separate directory"

    local sources
    if [ ${#} -gt 0 ]; then
        sources="${@}"
    else
        sources="$(find_sources "${srcdir}" "${builddir}" "${tarname}")"
    fi

    local ok=0
    for file in ${sources}; do
        local file="$(echo ${file} | sed -e "s,\\./,,")"

        check_file "${srcdir}" "${file}" || ok=1
    done

    return "${ok}"
}


main "${@}"
