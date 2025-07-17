#! /bin/sh
# Copyright 2010 The Kyua Authors.
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

Prog_Name=${0##*/}

if [ ! -f ./main.cpp ]; then
    echo "${Prog_Name}: must be run from the source top directory" 1>&2
    exit 1
fi

if [ ! -f configure ]; then
    echo "${Prog_Name}: configure not found; nothing to clean?" 1>&2
    exit 1
fi

[ -f Makefile ] || ./configure
make distclean

# Top-level directory.
rm -f Makefile.in
rm -f aclocal.m4
rm -rf autom4te.cache
rm -f config.h.in
rm -f configure
rm -f mkinstalldirs
rm -f kyua-*.tar.gz

# admin directory.
rm -f admin/ar-lib
rm -f admin/compile
rm -f admin/config.guess
rm -f admin/config.sub
rm -f admin/depcomp
rm -f admin/install-sh
rm -f admin/ltmain.sh
rm -f admin/mdate-sh
rm -f admin/missing

# bootstrap directory.
rm -f bootstrap/package.m4
rm -f bootstrap/testsuite

# doc directory.
rm -f doc/*.info
rm -f doc/stamp-vti
rm -f doc/version.texi

# m4 directory.
rm -f m4/libtool.m4
rm -f m4/lt*.m4

# Files and directories spread all around the tree.
find . -name '#*' | xargs rm -rf
find . -name '*~' | xargs rm -rf
find . -name .deps | xargs rm -rf
find . -name .gdb_history | xargs rm -rf
find . -name .libs | xargs rm -rf
find . -name .tmp | xargs rm -rf

# Show remaining files.
if [ -n "${GIT}" ]; then
    echo ">>> untracked and ignored files"
    "${GIT}" status --porcelain --ignored | grep -E '^(\?\?|!!)' || true
fi
