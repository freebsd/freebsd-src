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

# Vnode reference leak test scenario by kib@.
# Will fail with  "umount: unmount of /mnt5 failed: Device busy"
# vnode leak not seen on HEAD.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg
here=`pwd`
mounts=2		# Number of parallel scripts
D=$diskimage

[ -d "$RUNDIR" ] || mkdir $RUNDIR
cd $RUNDIR

if [ $# -eq 0 ]; then
	sed '1,/^EOF/d' < $here/$0 > vunref.c
	mycc -o /tmp/vunref -Wall -Wextra -O2 vunref.c
	rm -f vunref.c
	cd $here

	rm -f $RUNDIR/active.*
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		[ ! -d ${mntpoint}$m ] && mkdir ${mntpoint}$m
		mount | grep "$mntpoint" | grep -q md$m && umount -f ${mntpoint}$m
		mdconfig -l | grep -q md$m && mdconfig -d -u $m

		dd if=/dev/zero of=$D.$m bs=1m count=10 status=none
		mdconfig -a -t vnode -f $D.$m -u $m
		newfs md${m} > /dev/null 2>&1
	done

	# start the parallel tests
	for i in `jot $mounts`; do
		m=$((i + mdstart - 1))
		$0 mmap $m &
		sleep 0.2
		$0 $m &
	done

	sleep 2

	while [ ! -z "`ls $RUNDIR/active.* 2>/dev/null`" ] ; do
		../testcases/swap/swap -t 2m -i 20
	done
	wait

	for i in `jot $mounts`; do
		m=$((i + mdstart - 1))
		mdconfig -d -u $m
		rm -f $D$m
	done
	rm -f /tmp/vunref $RUNDIR/active.* $diskimage.* ${mntpoint}*/p*
else
	if [ $1 = mmap ]; then
		touch $RUNDIR/active.$2
		for i in `jot 500`; do
			cd ${mntpoint}$2
			/tmp/vunref > /dev/null 2>&1
			cd /
			[ -f $RUNDIR/active.$2 ] || exit
			sleep 0.1
		done
		rm -f $RUNDIR/active.$2
	else
		# The test: Parallel mount and unmounts
		m=$1
		mount $opt /dev/md${m} ${mntpoint}$m
		while [ -f $RUNDIR/active.$m ] ; do
			sleep 0.1
			n=0
			while mount | grep -qw $mntpoint$m; do
				umount ${mntpoint}$m > /dev/null 2>&1 && n=0
				if [ $((n += 1)) -gt 600 ]; then
					echo "*** Leak detected ***"
					fstat $mntpoint$m
					rm -f $RUNDIR/active.*
					exit 1
				fi
				sleep 0.1
			done
			mount $opt /dev/md${m} ${mntpoint}$m
		done
		mount | grep "$mntpoint" | grep -q md$m && umount ${mntpoint}$m
	fi
fi
exit
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/mount.h>
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

int
test(void)
{
	int i;
	pid_t pid;
	char file[128];
	int fdin, fdout;
	char *src, *dst;
	struct stat statbuf;

	pid = getpid();
	for (i = 0; i < 100; i++) {
		sprintf(file,"p%05d.%05d", pid, i);

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

	return (0);
}

int
main()
{
	int i;
	char path[MAXPATHLEN+1];
	struct statfs buf;

	if (getcwd(path, sizeof(path)) == NULL)
		err(1, "getcwd()");

	if (statfs(path, &buf) < 0)
		err(1, "statfs(%s)", path);

	if (!strcmp(buf.f_mntonname, "/"))
			return (1);

        for (i = 0; i < 2; i++) {
                if (fork() == 0)
                        test();
        }
        for (i = 0; i < 2; i++)
                wait(NULL);

        return (0);
}
