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

run_autoreconf() {
    if [ -d /usr/local/share/aclocal ]; then
        autoreconf -isv -I/usr/local/share/aclocal
    else
        autoreconf -isv
    fi
}

do_apidocs() {
    run_autoreconf || return 1
    ./configure --with-doxygen || return 1
    make check-api-docs
}

do_distcheck() {
    run_autoreconf || return 1
    ./configure || return 1

    sudo sysctl -w "kernel.core_pattern=core.%p"

    local archflags=
    [ "${ARCH?}" != i386 ] || archflags=-m32

    cat >kyua.conf <<EOF
syntax(2)

-- We do not know how many CPUs the test machine has.  However, parallelizing
-- the execution of our tests to _any_ degree speeds up the time it takes to
-- complete a test run because many of our tests are blocking.
parallelism = 4
EOF
    [ "${UNPRIVILEGED_USER:-no}" = no ] || \
        echo "unprivileged_user = 'nobody'" >>kyua.conf

    local f=
    f="${f} CFLAGS='${archflags}'"
    f="${f} CPPFLAGS='-I/usr/local/include'"
    f="${f} CXXFLAGS='${archflags}'"
    f="${f} LDFLAGS='-L/usr/local/lib -Wl,-R/usr/local/lib'"
    f="${f} PKG_CONFIG_PATH='/usr/local/lib/pkgconfig'"
    f="${f} KYUA_CONFIG_FILE_FOR_CHECK=$(pwd)/kyua.conf"
    if [ "${AS_ROOT:-no}" = yes ]; then
        sudo -H PATH="${PATH}" make distcheck DISTCHECK_CONFIGURE_FLAGS="${f}"
    else
        make distcheck DISTCHECK_CONFIGURE_FLAGS="${f}"
    fi
}

do_style() {
    run_autoreconf || return 1
    mkdir build
    cd build
    ../configure || return 1
    make check-style
}

main() {
    if [ -z "${DO}" ]; then
        echo "DO must be defined" 1>&2
        exit 1
    fi
    for step in ${DO}; do
        "do_${DO}" || exit 1
    done
}

main "${@}"
