#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# "panic: kmem_malloc(2069012480): kmem_map too small" seen.
# Fixed in r237366.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg
[ -z "`which setfacl`" ] && exit 0

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > extattr_set_fd.c
mycc -o extattr_set_fd -Wall -Wextra -O2 extattr_set_fd.c
rm -f extattr_set_fd.c

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

(cd $mntpoint; /tmp/extattr_set_fd)

while mount | grep -q $mntpoint; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/extattr_set_fd
exit 0
EOF
#include <sys/types.h>
#include <sys/extattr.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>

char buf[4096];

int
main(void)
{
	int fd;

	if ((fd = open("theFile", O_RDWR | O_CREAT, 0622)) == -1)
		err(1, "open(%s)", "theFile");

	(void) extattr_set_fd(fd, 1, "test", buf, 0x7b5294a6);

	return (0);
}
