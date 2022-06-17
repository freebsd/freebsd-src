#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# tunefs -j enable is not aware of the indirect blocks.
# Problem fixed in r247399.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > suj32.c
mycc -o suj32 -Wall -Wextra -O2 suj32.c || exit 1
rm -f suj32.c
cd $here

mount | grep "on $mntpoint " | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 5g -u $mdstart
newfs -U md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint || exit 1

# fill the root directory to become larger than NIDIR * blksize
(cd $mntpoint; /tmp/suj32)
while mount | grep -q $mntpoint; do
	umount $mntpoint || sleep 1
done

tunefs -j enable /dev/md$mdstart

if ! mount /dev/md$mdstart $mntpoint; then
	echo FAIL
	fsck_ffs -y /dev/md$mdstart
else
	umount $mntpoint
fi

mdconfig -d -u $mdstart
rm -f /tmp/suj32
exit 0
EOF
#include <sys/stat.h>
#include <err.h>
#include <stdio.h>

int
main(void)
{
	int i;
	char name[800];
	char *filler =
		"fillerfillerfillerfillerfillerfillerfillerfillerfiller"
		"fillerfillerfillerfillerfillerfillerfillerfillerfiller"
		"fillerfillerfillerfillerfillerfillerfillerfillerfiller"
		"fillerfillerfillerfillerfillerfillerfillerfillerfiller";

	for (i = 0; i < 2000; i++) {
		snprintf(name, sizeof(name), "%s.%d", filler, i);
		if (mkdir(name, 00700) == -1)
			err(1, "mkdir(%s)", name);
	}

	return (0);
}
