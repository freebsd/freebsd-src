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

# "panic: kmem_malloc(4096): kmem_map too small" seen.

. ../default.cfg
[ $((`sysctl -n vm.kmem_size` / 1024 / 1024 / 1024)) -eq 0 ] && exit 0

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/shm_open.c
mycc -o shm_open -Wall -Wextra -O2 -g shm_open.c || exit 1
rm -f shm_open.c
cd $odir

v1=`vmstat -m | awk '/shmfd/{gsub("K", ""); print $3}'`
su $testuser -c /tmp/shm_open
v2=`vmstat -m | awk '/shmfd/{gsub("K", ""); print $3}'`
v2=$((v2 - v1))
[ $v2 -gt 10 ] && { echo "shmfd leaked ${v2}K"; s=1; } || s=0

rm -f /tmp/shm_open
exit $s

EOF
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int
main(void)
{
	pid_t pid;
	off_t len;
	int fd, i;
	char name[80];

	len = 1024 * 1024;
	pid = getpid();
	for (i = 0; i < 500000; i++) {
		sprintf(name, "/testing.%06d.%06d", pid, i);

		if ((fd = shm_open(name, O_RDWR | O_CREAT, 0644)) == -1)
			err(1, "shm_open()");
		if (ftruncate(fd, len) == -1)
			err(1, "ftruncate");
		close(fd);
	}
	for (i = 0; i < 500000; i++) {
		sprintf(name, "/testing.%06d.%06d", pid, i);
		if (shm_unlink(name) == -1)
			err(1, "shm_unlink(%s)", name);
	}

	return(0);
}
