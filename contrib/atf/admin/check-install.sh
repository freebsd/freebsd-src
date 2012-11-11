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
# A utility to ensure that INSTALL lists the correct versions of the
# tools used to generate the distfile.
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
# check_tool readme_file prog_name verbose_name
#
#   Executes 'prog_name' to determine its version and checks if the
#   given 'readme_file' contains 'verbose_name <version>' in it.
#
check_tool() {
    readme=${1}
    prog=${2}
    name=${3}

    ver=$(${prog} --version | head -n 1 | cut -d ' ' -f 4)

    if grep "\\* ${name} ${ver}" ${readme} >/dev/null; then
        true
    else
        warn "Incorrect version of ${name}"
        false
    fi
}

#
# main readme_file
#
# Entry point.
#
main() {
    readme=${1}
    ret=0

    check_tool ${readme} autoconf "GNU autoconf" || ret=1
    check_tool ${readme} automake "GNU automake" || ret=1
    check_tool ${readme} libtool "GNU libtool" || ret=1

    return ${ret}
}

main "${@}"

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
