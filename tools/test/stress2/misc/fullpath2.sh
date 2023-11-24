#!/bin/sh

#
# Copyright (c) 2016 Dell EMC Isilon
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

# fullpath NULL reference problem hunt.

# From the commit log of r308407:
#   vn_fullpath1() checked VV_ROOT and then unreferenced
#   vp->v_mount->mnt_vnodecovered unlocked.  This allowed unmount to race.
#   Lock vnode after we noticed the VV_ROOT flag.  See comments for
#   explanation why unlocked check for the flag is considered safe.

# 'panic: namei: garbage in ni_resflags: 1':
# https://people.freebsd.org/~pho/stress/log/fullpath2.txt
# Fixed by r367130

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

cont=/tmp/fullpath2.continue
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/fullpath2.c
mycc -o fullpath2 -Wall -Wextra -O2 -g fullpath2.c -lprocstat || exit 1
rm -f fullpath2.c
cd $odir

mount | grep -q "on $mntpoint " && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
gpart create -s GPT md$mdstart > /dev/null || exit 1
gpart add -t freebsd-ufs md$mdstart > /dev/null || exit 1
newfs -n $newfs_flags md${mdstart}p1 > /dev/null || exit 1
mount /dev/md${mdstart}p1 $mntpoint
touch $mntpoint/marker $cont
trap "rm -f $cont" EXIT INT

daemon sh -c "(cd $odir/../testcases/swap; ./swap -t 4m -i 10 -l 100)" > \
    /dev/null 2>&1

for i in `jot $(jot -r 1 2 10)`; do
	/tmp/fullpath2 $mntpoint &
	pids="$pids $!"
done

for i in `jot $(jot -r 1 2 5)`; do
	while [ -e $cont ]; do find $mntpoint -ls > /dev/null 2>&1; done &
	pidf="$pidf $!"
done

umounts=0
while pgrep -q fullpath2; do
	for i in `jot 30`; do
		umount -f $mntpoint && umounts=$((umounts+1)) &&
		    mount /dev/md${mdstart}p1 $mntpoint
		sleep 2
	done
done
echo "$umounts umounts"
rm -f $cont
while mount | grep -q "on $mntpoint "; do
	umount -f $mntpoint
done
for i in $pids; do
	wait $i
done
while pgrep -q swap; do
	pkill -9 swap
done

kill $pidp $pidf > /dev/null 2>&1
wait

rm -f $mntpoint/file.* /tmp/fullpath2 fullpath2.core
mdconfig -d -u $mdstart

exit 0
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <fcntl.h>
#include <libprocstat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *share;

#define NB 1024
#define RUNTIME 300

/* dtrace -w -n 'fbt::*vn_fullpath1:entry {@rw[execname,probefunc] = count(); }' */

static void
getfiles(pid_t pid)
{
	struct filestat_list *head;
	struct kinfo_proc *p;
	struct procstat *prstat;
	unsigned int cnt;

	if ((prstat = procstat_open_sysctl()) == NULL)
		err(1, "procstat_open_sysctl");

	if ((p = procstat_getprocs(prstat, KERN_PROC_PID,
				    pid, &cnt)) == NULL)
		err(1, "procstat_getprocs");

	if ((head = procstat_getfiles(prstat, p, 0)) == NULL)
		err(1, "procstat_getfiles");

	procstat_freefiles(prstat, head);
	procstat_freeprocs(prstat, p);
	procstat_close(prstat);
}

int
main(int argc, char *argv[])
{
	size_t len;
	time_t start;
	int fd[NB], i, n;
	pid_t pid;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	if ((pid = fork()) == 0) {
		setproctitle("getfiles");
		while (share[0] == 0)
			getfiles(pid);
		_exit(0);
	}

	char file[MAXPATHLEN + 1];
	char marker[MAXPATHLEN + 1];

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file path>\n", argv[0]);
		exit(1);
	}

	memset(fd, 0, sizeof(fd));
	snprintf(marker, sizeof(marker), "%s/marker", argv[1]);
	i = n = 0;
	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		snprintf(file, sizeof(file), "%s/file.%06d.%02d", argv[1], getpid(), i);
		if (access(marker, R_OK) == -1)
			continue;
		if (fd[i] > 0)
			close(fd[i]);
		if ((fd[i] = open(file, O_RDWR | O_CREAT | O_APPEND,
		    DEFFILEMODE)) == -1) {
			if (errno != ENOENT && errno != EBUSY)
				warn("open(%s)", file);
			continue;
		}
		n++;
		write(fd[i], "a", 1);
		usleep(arc4random() % 400);
		if (arc4random() % 100 < 10) {
			close(fd[i]);
			unlink(file);
			fd[i] = 0;
		}
		i++;
		i = i % NB;
	}
	share[0] = 1;

	if (waitpid(pid, NULL, 0) != pid)
		err(1, "waitpid");
	if (n < 100)
		errx(1, "Short run: %d", n);

	return (0);
}
