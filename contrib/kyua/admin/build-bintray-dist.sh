#! /bin/sh
# Copyright 2017 The Kyua Authors.
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

# \file admin/build-bintray-dist.sh
# Builds a full Kyua installation under /usr/local for Ubuntu.
#
# This script is used to create the bintray distribution packages in lieu
# of real Debian packages for Kyua.  The result of this script is a
# tarball that provides the contents of /usr/local for Kyua.

set -e -x

err() {
    echo "${@}" 1>&2
    exit 1
}

install_deps() {
    sudo apt-get update -qq

    local pkgsuffix=
    local packages=
    packages="${packages} autoconf"
    packages="${packages} automake"
    packages="${packages} clang"
    packages="${packages} g++"
    packages="${packages} gdb"
    packages="${packages} git"
    packages="${packages} libtool"
    packages="${packages} make"
    if [ "${ARCH?}" = i386 ]; then
         pkgsuffix=:i386
         packages="${packages} gcc-multilib"
         packages="${packages} g++-multilib"
    fi
    packages="${packages} liblua5.2-0${pkgsuffix}"
    packages="${packages} liblua5.2-dev${pkgsuffix}"
    packages="${packages} libsqlite3-0${pkgsuffix}"
    packages="${packages} libsqlite3-dev${pkgsuffix}"
    packages="${packages} pkg-config${pkgsuffix}"
    packages="${packages} sqlite3"
    sudo apt-get install -y ${packages}
}

install_from_github() {
    local name="${1}"; shift
    local release="${1}"; shift

    local distname="${name}-${release}"

    local baseurl="https://github.com/jmmv/${name}"
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
        CC="${CC?}" \
        CFLAGS="${archflags}" \
        CPPFLAGS="-I/usr/local/include" \
        CXX="${CXX?}" \
        CXXFLAGS="${archflags}" \
        LDFLAGS="-L/usr/local/lib -Wl,-R/usr/local/lib" \
        PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
    make
    sudo make install
    cd -

    rm -rf "${distname}" "${distname}.tar.gz"
}

main() {
    [ "${ARCH+set}" = set ] || err "ARCH must be set in the environment"
    [ "${CC+set}" = set ] || err "CC must be set in the environment"
    [ "${CXX+set}" = set ] || err "CXX must be set in the environment"

    [ ! -f /root/local.tgz ] || err "/root/local.tgz already exists"
    tar -czf /root/local.tgz /usr/local
    restore() {
        rm -rf /usr/local
        tar -xz -C / -f /root/local.tgz
        rm /root/local.tgz
    }
    trap restore EXIT
    rm -rf /usr/local
    mkdir /usr/local

    install_deps
    install_from_github atf 0.21
    install_from_github lutok 0.4
    install_from_github kyua 0.13

    local version="$(lsb_release -rs | cut -d . -f 1-2 | tr . -)"
    local name="$(date +%Y%m%d)-usr-local-kyua"
    name="${name}-ubuntu-${version}-${ARCH?}-${CC?}.tar.gz"
    tar -czf "${name}" /usr/local
}

main "${@}"
