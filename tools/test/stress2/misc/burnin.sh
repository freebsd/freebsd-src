#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# Time creating and deleting a number of files once a minute.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

export LANG=C
dir=/tmp
runtime=1200	# default
[ $# -eq 1 ] && runtime=$1
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/burnin.c
mycc -o burnin -Wall -Wextra -O0 -g burnin.c || exit 1
rm -f burnin.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
d=`date '+%Y%m%dT%H%M%S'`
log=/tmp/burnin.$d.log
mode=`pgrep -q cron && echo "Multi-user" || echo "Single-user"`
echo "# `uname -a` $mode mode `hostname`" > $log

/tmp/burnin -r 10 -d $mntpoint > /dev/null 2>&1
/tmp/burnin -r $runtime -d $mntpoint >> $log

ministat -A -C 2 -w 72 $log | tail -1 | awk '{if ($NF > .1) exit(1)}'
s=$?
[ $s -ne 0 ] && ministat -C 2 -w 72 $log

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -rf /tmp/burnin $log
exit 0
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define DELAY 60
#define SYNC 0

volatile u_int *share;
int bufsize, files, parallel, runtime;
char *buf, *dir;

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-b buf size] [-d directory] [-p parallel] "
	    "[-r runtime]\n",
	    getprogname());
	_exit(1);
}

void
test(void)
{
	pid_t pid;
	int fd, i;
	char path[MAXPATHLEN + 1];

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (volatile u_int)parallel)
		;

	pid =getpid();
	for (i = 0; i < files; i++) {
		snprintf(path, sizeof(path), "%s/f%06d.%06d", dir, pid, i);
		if ((fd = open(path, O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE)) ==
		    -1)
			err(1, "open(%s)", path);
		if (write(fd, buf, bufsize) != bufsize)
			err(1, "write()");
		if (close(fd) == -1)
			err(1, "close(%d)", fd);
		if (unlink(path) == -1)
			err(1, "unlink(%s)", path);
	}

	_exit(0);
}

int
main(int argc, char *argv[])
{
	struct timeval t1, t2, diff;
	struct tm *tp;
	size_t len;
	time_t start, now;
	int ch, e, i, *pids, status;
	char help[80];

	bufsize = 8 * 1024;
	dir = "/tmp";
	files = 5000;
	parallel = 4;
	runtime = 1 * 60 * 60 * 24;

	while ((ch = getopt(argc, argv, "b:d:f:r:")) != -1)
		switch(ch) {
		case 'b':	/* bufsize */
			if (sscanf(optarg, "%d", &bufsize) != 1)
				usage();
			break;
		case 'd':	/* dir */
			dir = optarg;
			break;
		case 'f':	/* files */
			if (sscanf(optarg, "%d", &files) != 1)
				usage();
			break;
		case 'p':	/* parallel */
			if (sscanf(optarg, "%d", &parallel) != 1)
				usage();
			break;
		case 'r':	/* runtime */
			if (sscanf(optarg, "%d", &runtime) != 1)
				usage();
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	printf("# Options used: dir=%s, bufsize=%d, files=%d, parallel=%d, "
	    "runtime=%d\n",
	    dir, bufsize, files, parallel, runtime);
	if ((buf = malloc(bufsize)) == NULL)
		err(1, "malloc(%d)", bufsize);
	if ((pids = malloc(sizeof(pid_t) * parallel)) == NULL)
		err(1, "malloc(%d)", (int)(sizeof(pid_t) * parallel));
	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < runtime && e == 0) {
		share[SYNC] = 0;
		gettimeofday(&t1, NULL);
		for (i = 0; i < parallel; i++) {
			if ((pids[i] = fork()) == 0)
				test();
		}
		for (i = 0; i < parallel; i++) {
			waitpid(pids[i], &status, 0);
			e += status == 0 ? 0 : 1;
		}
		gettimeofday(&t2, NULL);
		timersub(&t2, &t1, &diff);
		now = time(NULL);
		tp = localtime(&now);
		strftime(help, sizeof(help), "%Y%m%d%H%M%S", tp);
		printf("%s %ld.%06ld\n", help, (long)diff.tv_sec,
		    diff.tv_usec);
		fflush(stdout);
		if (runtime > DELAY)
			sleep(DELAY);
	}

	return (e);
}
