#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Regression test for statfs problems with deleting a large number of files

# $ ./statfs.sh
# Filesystem  1K-blocks    Used Avail Capacity iused  ifree %iused  Mounted on
# /dev/ad0s1e   1982798 1782134 42042    98%    4965 254105    2%   /tmp
# Free inodes on /tmp: 254105
# Creating 100000 files...
# Deleting 100000 files...
# Filesystem  1K-blocks    Used   Avail Capacity iused  ifree %iused  Mounted on
# /dev/ad0s1e   1982798 -284096 2108272   -16%    4965 254105    2%   /tmp
# $ umount -f /tmp; mount /tmp
# $ df -i /tmp
# Filesystem  1K-blocks    Used Avail Capacity iused  ifree %iused  Mounted on
# /dev/ad0s1e   1982798 1784528 39648    98%    4965 254105    2%   /tmp

. ../default.cfg

odir=`pwd`
dir=/tmp

cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/statfs.c
mycc -o statfs -Wall statfs.c
rm -f statfs.c

df -i /tmp
./statfs
df -i /tmp

exit
EOF
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <errno.h>
#include <err.h>

int64_t
inodes(void)
{
	char path[MAXPATHLEN+1];
	struct statfs buf;

	sync();
	if (getcwd(path, sizeof(path)) == NULL)
		err(1, "getcwd()");

	if (statfs(path, &buf) < 0)
		err(1, "statfs(%s)", path);
	printf("Free inodes on %s: %jd\n", path, buf.f_ffree);
	return (buf.f_ffree);
}

int
main()
{
	int fd, i, j;
	int64_t size;
	pid_t pid;
	char file[128];

	size = inodes() - 1000;

	if (size > 100000)
		size = 100000;

	printf("Creating %jd files...", size); fflush(stdout);
	pid = getpid();
	for (j = 0; j < size; j++) {
		sprintf(file,"p%06d.%05d", pid, j);
		if ((fd = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0666)) == -1) {
			if (errno != EINTR) {
				warn("creat(%s)", file);
				printf("break out at %d, errno %d\n", j, errno);
				break;
			}
		}
		if (fd != -1 && close(fd) == -1)
			err(2, "close(%d)", j);

	}

	printf("\nDeleting %jd files...", size); fflush(stdout);
	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%06d.%05d", pid, i);
		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);

	}
	printf("\n");
	return (0);
}
