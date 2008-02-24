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
# $FreeBSD: src/usr.bin/tar/test/test-flags.sh,v 1.1 2007/03/11 10:36:42 kientzle Exp $

# Exercise copying of file flags
echo "File Flag handling"
# Basic test configuration
. `dirname $0`/config.sh

# Create some files with various flags set
mkdir original
FLAGS='uchg opaque nodump uappnd'
for f in $FLAGS; do
    touch original/test.$f
    chflags $f original/test.$f
done
#ls -ol ${TESTDIR}/original

# Copy the dir with -p
echo "  -p preserves flags"
mkdir copy
(cd original && ${BSDTAR} -cf - .) | (cd copy; ${BSDTAR} -xpf -)
# Verify that the flags are set
for f in $FLAGS; do
    [ "$f" = `ls -ol copy/test.$f | awk '{print $5}'` ]		\
	|| echo XXX FAIL: $f not preserved with -p XXX
done
#ls -ol ${TESTDIR}/copy

# Copy the dir without -p
echo "  flags omitted without -p"
mkdir copy2
(cd original && ${BSDTAR} -cf - .) | (cd copy2; ${BSDTAR} -xf -)
# Verify that the flags are not set
for f in $FLAGS; do
    [ "$f" = `ls -ol copy2/test.$f | awk '{print $5}'` ]	\
	&& echo XXX FAIL: $f copied without -p XXX
done
#ls -ol ${TESTDIR}/copy2

# Strip off the flags so we can clean this directory on the next test
for f in $FLAGS; do
    if [ $f = 'nodump' ]; then
	chflags dump original/test.$f
	chflags dump copy/test.$f
    else
	chflags no$f original/test.$f
	chflags no$f copy/test.$f
    fi
done
cd ..

