#! /bin/sh
# Copyright 2014 Google Inc.
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
    sudo apt-get update -qq

    local pkgsuffix=
    local packages=
    if [ "${ARCH?}" = i386 ]; then
         pkgsuffix=:i386
         packages="${packages} gcc-multilib"
         packages="${packages} g++-multilib"
    fi
    packages="${packages} doxygen"
    packages="${packages} gdb"
    packages="${packages} liblua5.2-0${pkgsuffix}"
    packages="${packages} liblua5.2-dev${pkgsuffix}"
    packages="${packages} libsqlite3-0${pkgsuffix}"
    packages="${packages} libsqlite3-dev${pkgsuffix}"
    packages="${packages} pkg-config${pkgsuffix}"
    packages="${packages} sqlite3"
    sudo apt-get install -y ${packages}
}

install_from_github() {
    local project="${1}"; shift
    local name="${1}"; shift
    local release="${1}"; shift

    local distname="${name}-${release}"

    local baseurl="https://github.com/jmmv/${project}"
    wget --no-check-certificate \
        "${baseurl}/releases/download/${distname}/${distname}.tar.gz"
    tar -xzvf "${distname}.tar.gz"

    local archflags=
    [ "${ARCH?}" != i386 ] || archflags=-m32

    cd "${distname}"
    ./configure \
        --disable-developer \
        --without-atf \
        --without-doxygen \
        CFLAGS="${archflags}" \
        CPPFLAGS="-I/usr/local/include" \
        CXXFLAGS="${archflags}" \
        LDFLAGS="-L/usr/local/lib -Wl,-R/usr/local/lib" \
        PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
    make
    sudo make install
    cd -

    rm -rf "${distname}" "${distname}.tar.gz"
}

install_from_bintray() {
    case "${ARCH?}" in
        amd64)
            name="20160204-usr-local-kyua-ubuntu-12-04-amd64-${CC?}.tar.gz"
            ;;
        i386)
            name="20160714-usr-local-kyua-ubuntu-12-04-i386-${CC?}.tar.gz"
            ;;
        *)
            echo "ERROR: Unknown ARCH value ${ARCH}" 1>&2
            exit 1
            ;;
    esac
    wget "http://dl.bintray.com/jmmv/kyua/${name}" || return 1
    sudo tar -xzvp -C / -f "${name}"
    rm -f "${name}"
}

install_deps
if ! install_from_bintray; then
    install_from_github atf atf 0.20
    install_from_github lutok lutok 0.4
    install_from_github kyua kyua-testers 0.2
    install_from_github kyua kyua-cli 0.8
fi
