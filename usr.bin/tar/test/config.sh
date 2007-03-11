#
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

THISDIR=`cd \`dirname $0\`;/bin/pwd`

# TESTDIR defaults to /tmp/bsdtar- + the name of the script
if [ -z "$TESTDIR" ]; then
    TESTDIR=/tmp/bsdtar-`echo $0 | sed -e 's|.*/||' -e 's|\.sh||' -e 's/[^a-z0-9_-]/_/g'`
fi
# Use bsdtar we just built by default
if [ -z "$BSDTAR" ]; then
    BSDTAR=$THISDIR/../../bsdtar
fi
# Try 'gtar', then 'tar', then '/usr/local/bin/gtar' to find GNU tar
if [ -z "$GTAR" ]; then
    if ( /bin/sh -c 'gtar --version' | grep 'GNU tar' ) > /dev/null 2>&1; then
	GTAR=gtar
    fi
fi
if [ -z "$GTAR" ]; then
    if ( /bin/sh -c 'tar --version' | grep 'GNU tar' ) > /dev/null 2>&1; then
	GTAR=tar
    fi
fi
if [ -z "$GTAR" ]; then
    if ( /bin/sh -c '/usr/local/bin/gtar --version' | grep 'GNU tar' ) > /dev/null 2>&1; then
	GTAR=/usr/local/bin/gtar
    fi
fi
if [ -z "$CPIO" ]; then
    CPIO=cpio
fi

# Remove and recreate the directory we'll use for these tests
rm -rf $TESTDIR
mkdir -p $TESTDIR || exit 1
cd $TESTDIR || exit 1

