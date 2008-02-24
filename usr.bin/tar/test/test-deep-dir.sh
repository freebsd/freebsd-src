#!/bin/sh
# Copyright (c) 2007 Tim Kientzle
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD: src/usr.bin/tar/test/test-deep-dir.sh,v 1.1 2007/03/11 10:36:42 kientzle Exp $

# Stress the deep directory logic; the actual depth here seems to
# be limited by the shell.  This should be restructured to get around
# that limit (possibly by using perl to build the deep tree?)
echo Deep directory tests

# Basic test configuration
. `dirname $0`/config.sh

# Create a deep dir (shell seems to be limited by PATH_MAX)
mkdir original
cd original
I=0
while [ $I -lt 200 ]
do
    mkdir a$I
    cd a$I
    I=$(($I + 1))
done
while [ $I -gt 0 ] ; do cd ..; I=$(($I - 1)); done
cd ..

# Copy this using bsdtar
echo "  tar -c | tar -x"
mkdir copy
(cd original; ${BSDTAR} -cf - .) | (cd copy; ${BSDTAR} -xf -)
diff -r original copy || echo XXX FAILURE XXX

# Copy gtar->bsdtar
echo "  gtar -c | tar -x"
mkdir copy-gtar
(cd original; ${GTAR} -cf - .) | (cd copy-gtar; ${BSDTAR} -xf -)
diff -r original copy-gtar || echo XXX FAILURE XXX
cd ..

