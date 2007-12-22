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
# $FreeBSD$

echo "Test overwrite avoidance"
. `dirname $0`/config.sh

# Create a file with some data.
# This ensures that test.tar actually has some data in it
# by the time tar tries to add it to itself.
dd if=/dev/urandom of=a bs=1k count=100 >/dev/null 2>&1

# Now try to implicitly add archive to itself
${BSDTAR} -cf test.tar . || echo XXX FAILED XXX

# Create another file
dd if=/dev/urandom of=b bs=1k count=100 >/dev/null 2>&1

# Try again.
${BSDTAR} -cf test.tar . || echo XXX FAILED XXX

# Extract the archive and check that the two files got archived, despite the warning
mkdir compare
cd compare
${BSDTAR} -xf ../test.tar
cmp a ../a || echo XXX a didn't archive correctly XXX
cmp b ../b || echo XXX b didn't archive correctly XXX

# TODO: Test overwrite avoidance on extract
