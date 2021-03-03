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

# "umount -f" test scenario (distill of nfs4.sh)
# "panic: vputx: missed vn_close" seen.
# Fixed in r248815

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > nfs12.c
mycc -o nfs12 -Wall -Wextra -O2 -g nfs12.c
rm -f nfs12.c
cd $here

mount | grep "on $mntpoint " | grep nfs > /dev/null && umount $mntpoint
version="-o nfsv3"	# The default
[ $# -eq 1 ] &&  [ "$1" -eq 4 ] && version="-o nfsv4"
for i in `jot 10`; do
	mount -t nfs $version -o tcp -o retrycnt=3 -o intr,soft -o rw \
	    $nfs_export $mntpoint
	sleep 2

	if [ $i -eq 10 ]; then
		rm -f $mntpoint/nfs12.p*
	else
		(cd $mntpoint; /tmp/nfs12 > /dev/null 2>&1) &
		sleep 2
	fi

	while mount | grep "on $mntpoint " | grep -q nfs; do
		umount -f $mntpoint
	done
	kill -9 $! > /dev/null 2>/dev/null && kill $!
	wait
done

rm -f /tmp/nfs12
exit
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
#include <string.h>
#include <unistd.h>

#define INPUTFILE "/bin/date"
#define PARALLEL 5

static int
tmmap(void)
{
	struct stat statbuf;
	pid_t pid;
	char *src, *dst;
	int i;
	int fdin, fdout;
	char file[128];

	pid = getpid();
	setproctitle("mmap");
	for (i = 0; i < 50000; i++) {
		sprintf(file,"nfs12.p%05d.%05d", pid, i);

		if ((fdin = open(INPUTFILE, O_RDONLY)) < 0)
			err(1, INPUTFILE);

		if ((fdout = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0)
			err(1, "%s", file);

		if (fstat(fdin, &statbuf) < 0)
			err(1, "fstat error");

		if (lseek(fdout, statbuf.st_size - 1, SEEK_SET) == -1)
			err(1, "lseek error");

		/* write a dummy byte at the last location */
		if (write(fdout, "", 1) != 1)
			err(1, "write error");

		if ((src = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fdin, 0)) ==
			(caddr_t) - 1)
			err(1, "mmap error for input");

		if ((dst = mmap(0, statbuf.st_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fdout, 0)) == (caddr_t) - 1)
			err(1, "mmap error for output");

		memcpy(dst, src, statbuf.st_size);

		if (munmap(src, statbuf.st_size) == -1)
			err(1, "munmap");
		close(fdin);

		if (munmap(dst, statbuf.st_size) == -1)
			err(1, "munmap");
		close(fdout);

		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);
	}

	_exit(0);
}

int
main(void)
{
        int i;

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			tmmap();
	}

	for (i = 0; i < PARALLEL; i++) {
		wait(NULL);
	}

	return (0);
}
