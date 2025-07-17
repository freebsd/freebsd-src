#! /bin/sh
# Copyright 2014 The Kyua Authors.
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

set -e -x

install_deps() {
    local pkgsuffix=
    local packages=
    if [ "${ARCH?}" = i386 ]; then
         pkgsuffix=:i386
         packages="${packages} gcc-multilib"
         packages="${packages} g++-multilib"
         sudo dpkg --add-architecture i386
    fi
    packages="${packages} gdb"
    packages="${packages} liblua5.2-0${pkgsuffix}"
    packages="${packages} liblua5.2-dev${pkgsuffix}"
    packages="${packages} libsqlite3-0${pkgsuffix}"
    packages="${packages} libsqlite3-dev${pkgsuffix}"
    packages="${packages} pkg-config${pkgsuffix}"
    packages="${packages} sqlite3"
    sudo apt-get update -qq
    sudo apt-get install -y ${packages}
}

install_kyua() {
    local name="20190321-usr-local-kyua-ubuntu-16-04-${ARCH?}-${CC?}.tar.gz"
    wget -O "${name}" "http://dl.bintray.com/ngie-eign/kyua/${name}" || return 1
    sudo tar -xzvp -C / -f "${name}"
    rm -f "${name}"
}

do_apidocs() {
    sudo apt-get install -y doxygen
}

do_distcheck() {
    :
}

do_style() {
    :
}

main() {
    if [ -z "${DO}" ]; then
        echo "DO must be defined" 1>&2
        exit 1
    fi
    install_deps
    install_kyua
    for step in ${DO}; do
        "do_${DO}" || exit 1
    done
}

main "${@}"
