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

# Exercise copying of ACLs
echo "ACL handling"
# Basic test configuration
TESTDIR=/mnt/da0/acl-test
. `dirname $0`/config.sh

# Create some files with ACLs
mkdir original
cd original
touch a
chmod 664 a
setfacl -m user:bin:rw- -m group:78:r-x a \
    || echo XXX failed to set access ACL on a XXX
mkdir d
chmod 775 d
setfacl -m user:daemon:rw- -m group:78:r-x d \
    || echo XXX failed to set access ACL on d XXX
setfacl -d   -m user::rw- -m group::rw- -m other::rw- -m group:79:r-- d \
    || echo XXX failed to set default ACL on d XXX
cd ..

# Copy the dir with -p
echo "  -p preserves ACLs"
mkdir copy
(cd original && ${BSDTAR} -cf - .) | (cd copy; ${BSDTAR} -xpf -)

# Verify the ACLs
cd copy
if [ "user::rw- user:bin:rw- group::rw- group:78:r-x mask::rwx other::r--" \
    = "`echo \`getfacl -q a\``" ]; then
    # It matches!!
else
    echo XXX a has wrong ACL XXX `getfacl -q a`
fi

if [ "user::rwx user:daemon:rw- group::rwx group:78:r-x mask::rwx other::r-x" \
    = "`echo \`getfacl -q d\``" ]; then
    # It matches!!
else
    echo XXX d has wrong ACL XXX `getfacl -q d`
fi


if [ "user::rw- group::rw- group:79:r-- mask::rw- other::rw-" \
    = "`echo \`getfacl -q -d d\``" ]; then
    # It matches!!
else
    echo XXX d has wrong ACL XXX `getfacl -q -d d`
fi

