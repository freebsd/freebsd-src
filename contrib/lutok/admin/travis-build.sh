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
./configure

archflags=
[ "${ARCH?}" != i386 ] || archflags=-m32

f=
f="${f} CPPFLAGS='-I/usr/local/include'"
f="${f} CXX='${CXX} ${archflags}'"
f="${f} LDFLAGS='-L/usr/local/lib -Wl,-R/usr/local/lib'"
f="${f} PKG_CONFIG_PATH='/usr/local/lib/pkgconfig'"
if [ "${AS_ROOT:-no}" = yes ]; then
    sudo -H PATH="${PATH}" make distcheck DISTCHECK_CONFIGURE_FLAGS="${f}"
else
    make distcheck DISTCHECK_CONFIGURE_FLAGS="${f}"
fi
