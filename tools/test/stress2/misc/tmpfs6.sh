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

# panic: deadlkres: possible deadlock detected for 0xc8576a00, blocked for 1801792 ticks

# Scenario by kib@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > tmpfs6.c
mycc -o tmpfs6 -Wall -Wextra -O2  tmpfs6.c || exit 1
rm -f tmpfs6.c

mount | grep $mntpoint | grep -q tmpfs && umount $mntpoint
mount -t tmpfs tmpfs  $mntpoint

(cd $mntpoint; /tmp/tmpfs6)
rm -f /tmp/tmpfs6

while mount | grep $mntpoint | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
exit
EOF
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <err.h>

int pagesize;

#define FILESIZE (32 * 1024)
char wbuffer[FILESIZE];

void
test(void)
{
	int fd;
	int len;
	void *addr;
	char filename[80];

	snprintf(filename, sizeof(filename), "file.%07d", getpid());
	if ((fd = open(filename, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) == -1)
		err(1, "open(%s)", filename);

	if ((len = write(fd, wbuffer, FILESIZE)) != FILESIZE)
		err(1, "write()");

	fsync(fd);

	if ((addr = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE , MAP_SHARED, fd, 0)) == MAP_FAILED)
		err(1, "mmap()");

	if (lseek(fd, 0, SEEK_SET) != 0)
		err(1, "lseek()");

	if ((len = write(fd, addr, FILESIZE)) != FILESIZE)
		err(1, "write() 2");

	if (munmap(addr, FILESIZE) == -1)
		err(1, "munmap()");
	close(fd);
	unlink(filename);

}

int
main(void)
{
	int i;

	for (i = 0; i < 10000; i++)
		test();

	return (0);
}
