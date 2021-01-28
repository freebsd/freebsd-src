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

if [ -d /usr/local/share/aclocal ]; then
    autoreconf -isv -I/usr/local/share/aclocal
else
    autoreconf -isv
fi

ret=0
./configure || ret=${?}
if [ ${ret} -ne 0 ]; then
    cat config.log || true
    exit ${ret}
fi

archflags=
[ "${ARCH?}" != i386 ] || archflags=-m32

f=

if [ -n "${archflags}" ]; then
    CC=${CC-"cc"}
    CXX=${CXX-"c++"}

    f="${f} ATF_BUILD_CC='${CC} ${archflags}'"
    f="${f} ATF_BUILD_CXX='${CXX} ${archflags}'"
    f="${f} CFLAGS='${archflags}'"
    f="${f} CXXFLAGS='${archflags}'"
    f="${f} LDFLAGS='${archflags}'"
fi

if [ "${AS_ROOT:-no}" = yes ]; then
    cat >root-kyua.conf <<EOF
syntax(2)
unprivileged_user = 'nobody'
EOF
    ret=0
    sudo -H PATH="${PATH}" make distcheck DISTCHECK_CONFIGURE_FLAGS="${f}" \
        KYUA_TEST_CONFIG_FILE="$(pwd)/root-kyua.conf" || ret=${?}
else
    ret=0
    make distcheck DISTCHECK_CONFIGURE_FLAGS="${f}" || ret=${?}
fi
if [ ${ret} -ne 0 ]; then
    cat atf-*/_build/sub/config.log || true
    exit ${ret}
fi

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
