#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2020 Peter Holm
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

# A tmpfs "nomtime" mount option test.

# kib@ wrote:
# A test program should do something along the lines:
# 1. open tmpfs file, ftruncate it, mmap
# 2. write to the mmaped file area periodically, for long time
#    e.g. 1 write in 10 secs, for 120 secs
# 3. the mtime of the file should be the same as after the truncation
#    if the nomtime flag is set.  Otherwise, it should be around +-30
#    secs of the last write to the area.
#
# Really the current (unpatched) situation is some not well defined
# mix between nomtime and its absence.

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > tmpfs22.c
mycc -o tmpfs22 -Wall -Wextra -O2 -g tmpfs22.c || exit 1
rm -f tmpfs22.c
cd $odir

mount | grep "$mntpoint" | grep -q tmpfs && umount $mntpoint
mount -o size=2g,nomtime -t tmpfs tmpfs  $mntpoint || exit 1

(cd $mntpoint; /tmp/tmpfs22); s=$?

for i in `jot 6`; do
	umount $mntpoint && break
	sleep 2
done
rm -f /tmp/tmpfs22
exit $s

EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static char *file = "test";

int
main(void)
{
	struct stat st1, st2;
	size_t len;
	int fd, i, s;
	char *p;

	if ((fd = open(file, O_RDWR | O_CREAT, 0644)) == -1)
		err(1, "open(%s)", file);
	len = 2LL * 1024 * 1024;
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");
	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM)
			return (1);
		err(1, "mmap()");
	}
	if (fstat(fd, &st1) == -1)
		err(1, "fstat 1");

	for (i = 0; i < 12; i++) {
		sleep(10);
		p[arc4random() % len] = 1;
	}
	if (fstat(fd, &st2) == -1)
		err(1, "fstat 2");
	s = 0;
	if (st1.st_mtime == st2.st_mtime) {
		fprintf(stderr, "mtime is unchanged: %ld %ld\n",
		    (long)st1.st_mtime, (long)st2.st_mtime);
		s=1;
	}
	munmap(p, len);
	close(fd);

	return (s);
}
