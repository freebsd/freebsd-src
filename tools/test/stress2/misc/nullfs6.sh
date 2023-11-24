#!/bin/sh

#
# Copyright (c) 2010 Peter Holm <pho@FreeBSD.org>
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Lock violation panic regression test
# Test scenario by Mikolaj Golub <to my trociny gmail com>
# Fixed in r208773

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

nullfs_srcdir=${nullfs_srcdir:-/tmp}
mount | grep nullfs | grep -q $nullfs_srcdir/1 && umount $nullfs_srcdir/1

rm -rf $nullfs_srcdir/1 $nullfs_srcdir/2
mkdir $nullfs_srcdir/1 $nullfs_srcdir/2
touch $nullfs_srcdir/1/test.file

mount -t nullfs $nullfs_srcdir/1 $nullfs_srcdir/2

cp $nullfs_srcdir/1/test.file $nullfs_srcdir/2/test.file	# scenario by kib
mv $nullfs_srcdir/1/test.file $nullfs_srcdir/2/	# panics with lock violation

umount $nullfs_srcdir/1
rm -rf $nullfs_srcdir/1 $nullfs_srcdir/2
