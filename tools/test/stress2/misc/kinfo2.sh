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

# Test scenario by marcus@freebsd.org

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kinfo2.c
mycc -o kinfo2 -Wall -Wextra kinfo2.c -lutil || exit 1
rm -f kinfo2.c

mount | grep -q procfs || mount -t procfs procfs /proc
s=0
for i in `jot 15`; do
	pids=""
	for j in `jot 5`; do
		/tmp/kinfo2 &
		pids="$pids $!"
	done
	for p in $pids; do
		wait $p
		[ $? -ne 0 ] && s=1
	done
done

rm -f /tmp/kinfo2
exit $s
EOF

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

static char buf[8096];

static void
handler(int i __unused) {
	_exit(0);
}

/* Stir /dev/proc */
static void
churning(void) {
	pid_t r;
	int fd, status;

	for (;;) {
		r = fork();
		if (r == 0) {
			if ((fd = open("/proc/curproc/mem", O_RDONLY)) == -1)
				err(1, "open(/proc/curproc/mem)");
			bzero(buf, sizeof(buf));
			_exit(0);
		}
		if (r < 0) {
			perror("fork");
			exit(2);
		}
		wait(&status);
	}
}

/* Get files for each proc */
void
list(void)
{
	struct dirent *dp;
        struct kinfo_file *freep;
	struct kinfo_vmentry *freep_vm;
	struct stat sb;
	pid_t pid;
	off_t base;
	long l;
	int cnt, fd, n;
	int space = sizeof(buf);
	char *bp = buf;
	char *dummy;

	if ((fd = open("/proc", O_RDONLY)) == -1)
		err(1, "open(%s)", "/proc");

	if (fstat(fd, &sb) == -1)
		err(1, "fstat()");
	do {
		if ((n = getdirentries(fd, bp, space, &base)) == -1)
			err(1, "getdirentries");
		space = space - n;
		if (space < sb.st_blksize)
			break;
		bp   = bp + n;
	} while (n != 0);
	close(fd);

	bp = buf;
	dp = (struct dirent *)bp;
	for (;;) {
#if defined(DEBUG)
		printf("name: %-10s, inode %7ju, type %2d, namelen %d, "
		    "d_reclen %d\n",
		    dp->d_name, (uintmax_t)dp->d_fileno, dp->d_type,
		    dp->d_namlen, dp->d_reclen); fflush(stdout);
#endif

		if (dp->d_type == DT_DIR &&
		    (dp->d_name[0] >= '0' && dp->d_name[0] <= '9')) {
			l = strtol(dp->d_name, &dummy, 10);
			pid = l;

			/* The tests start here */
			freep = kinfo_getfile(pid, &cnt);
			free(freep);

			freep_vm = kinfo_getvmmap(pid, &cnt);
			free(freep_vm);
			/* End test */
		}

		bp = bp + dp->d_reclen;
		dp = (struct dirent *)bp;
		if (dp->d_reclen <= 0)
			break;
	}
}

int
main(void)
{
	pid_t r;

	signal(SIGALRM, handler);
	alarm(60);

	if ((r = fork()) == 0) {
		alarm(60);
		for (;;)
			churning();
	}
	if (r < 0) {
		perror("fork");
		exit(2);
	}

	for (;;)
		list();

	return (0);
}
