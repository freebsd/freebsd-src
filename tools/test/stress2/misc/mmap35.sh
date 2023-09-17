#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# mmap(2) should fail with "Invalid argument" on i386
# Fixed by r348843

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
[ "`uname -p`" = "i386" ] || exit 0

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap35.c
mycc -o mmap35 -Wall -Wextra -O0 -g mmap35.c || exit 1
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 4g -u $mdstart
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
echo "Expect: mmap35: mmap: Cannot allocate memory"
$dir/mmap35
s=$?
[ -f mmap35.core ] &&
    { ls -l mmap35.core; mv mmap35.core $dir; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -f $dir/mmap35
[ $s -eq 0 ] && rm -f mmap35.c
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SIZ 0xfffff0fcULL

int
main(void)
{
	off_t offset;
	int fd;
	char *c, file[80];

	snprintf(file, sizeof(file), "file.%d", getpid());
	if ((fd = open(file, O_RDWR | O_CREAT, DEFFILEMODE)) == -1)
		err(1, "open(%s)", file);

	offset = SIZ;
	if (ftruncate(fd, offset) == -1)
		err(1, "ftruncate(%jd)", offset);
	write(fd, "b", 1);

	c = mmap(NULL, offset, PROT_READ | PROT_WRITE, MAP_SHARED,
	    fd, 0);
	if (c == MAP_FAILED)
		warn("mmap");
	else
		c[offset / 2] = 1;

	unlink(file);

	return (0);
}
