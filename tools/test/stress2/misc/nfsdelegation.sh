#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# Test nfsv4 delegations. Scenario suggestion by kib.
# "(nfsdelegation), uid 0, was killed: text file modification" seen.
# Fixed by r316745

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > nfsdelegation.c
mycc -o nfsdelegation -Wall -Wextra -O0 nfsdelegation.c || exit 1
rm -f nfsdelegation.c

[ `sysctl -n sysctl vfs.timestamp_precision` -ne 3 ] &&
    echo "vfs.timestamp_precision must be set to 3"
[ "`sysctl -ni vfs.nfsd.issue_delegations`" != "1" ] &&
    { echo "vfs.nfsd.issue_delegations is not enabled"; exit 0; }
pgrep -q nfscbd || { echo "nfscbd is not running"; exit 0; }

mount | grep "$mntpoint" | grep -q nfs && umount $mntpoint
opt="-o nocto"
opt="$opt -o nolockd -o nfsv4"
mount $opt $nfs_export $mntpoint || exit 1
sleep .2

wdir=$mntpoint/nfsdelegation.`jot -rc 8 a z | tr -d '\n'`/nfsdelegation
mkdir -p $wdir || exit 1

delegs=0
s=0
(cd $wdir; /tmp/nfsdelegation) &
while kill -0 $! 2>/dev/null; do
	r=`nfsstat -ec | grep -A1 Delegs | tail -1 | awk '{print $5}'`
	[ $r -gt $delegs ] && { delegs=$r; break; }
done
wait
[ $delegs -eq 0 ] && { echo "No delegations detected"; s=2; }

rm -rf $wdir
umount $mntpoint
while mount | grep "$mntpoint " | grep -q nfs; do
	umount -f $mntpoint
done
tail -3 /var/log/messages | grep -m1 nfsdelegation: && s=2
rm -f /tmp/nfsdelegation
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
#include <string.h>
#include <unistd.h>

#define LOOPS 100
#define INPUTFILE "/bin/sleep"
#define PARALLEL 3

static volatile u_int *share;

static int
tmmap(int idx)
{
	struct stat statbuf;
	pid_t epid, pid;
	int i;
	int fdout;
	char *cmdline[3], *dst, file[128], help[80];

	pid = getpid();
	cmdline[1] = ".01";
	cmdline[2] = 0;
	for (i = 0; i < LOOPS; i++) {
		sprintf(file,"nfsdelegation.p%05d.%05d", pid, i);
		cmdline[0] = file;

		snprintf(help, sizeof(help), "cp %s %s; chmod 777 %s", INPUTFILE, file, file);
		system(help);
		share[idx] = 0;
		if ((epid = fork()) == 0) {
			alarm(60);
			while (share[idx] == 0)
				usleep(100);
			if (execve(cmdline[0], cmdline, NULL) == -1)
				err(1, "execve");
		}

		if ((fdout = open(file, O_RDWR)) < 0)
			err(1, "open(%s)", file);
		if (fstat(fdout, &statbuf) < 0)
			err(1, "fstat error");

		if ((dst = mmap(0, statbuf.st_size, PROT_READ | PROT_WRITE |
		    MAP_PRIVATE, MAP_SHARED, fdout, 0)) == (caddr_t) - 1)
			err(1, "mmap error for output");

		dst[statbuf.st_size] = 1;

		close(fdout);
		if (munmap(dst, statbuf.st_size) == -1)
			err(1, "munmap");
		share[idx] = 1;
		if (waitpid(epid, NULL, 0) != epid)
			err(1, "waitpid(%d)", epid);
	}

	_exit(0);
}

int
main(void)
{
	size_t len;
        int i;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");
	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			tmmap(i);
	}

	for (i = 0; i < PARALLEL; i++) {
		wait(NULL);
	}

	return (0);
}
