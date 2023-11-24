#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Regression test of extattrctl and ACLs on UFS1 FS
# Kernel must be compiled with options UFS_EXTATTR and UFS_EXTATTR_AUTOSTART

# Scenario by rwatson@ from:
#
# Newsgroups: lucky.freebsd.current
# Subject: Re: setfacl requirements?
# Date: Thu, 5 Dec 2002 15:50:02 +0000 (UTC)

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ "`sysctl -in kern.features.ufs_extattr`" != "1" ] &&
	{ echo "Kernel not build with UFS_EXTATTR"; exit 0; }
[ -z "`which setfacl`" ] && exit 0

mount | grep "$mntpoint" | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 20m -u $mdstart

newfs -O 1 md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

mkdir -p $mntpoint/.attribute/system
cd $mntpoint/.attribute/system

extattrctl initattr -p . 388 posix1e.acl_access
extattrctl initattr -p . 388 posix1e.acl_default
cd /
umount $mntpoint
tunefs -a enable /dev/md$mdstart
mount /dev/md$mdstart $mntpoint
mount | grep md$mdstart

touch $mntpoint/acl-test
setfacl -b $mntpoint/acl-test
setfacl -m user:nobody:rw-,group:wheel:rw- $mntpoint/acl-test
getfacl $mntpoint/acl-test
ls -l $mntpoint/acl-test

umount $mntpoint
mdconfig -d -u $mdstart
