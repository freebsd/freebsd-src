#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Show scheduler fairness for ULE vs. 4BSD.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > sched.c
mycc -o sched -Wall -Wextra -O0 sched.c || exit 1
rm -f sched.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

cpus=`sysctl hw.ncpu | sed 's/.*: //'`
uname -v
(cd $mntpoint; /tmp/sched $((cpus + 1))) > /dev/null 2>&1 &
sleep 30
export LANG=C
top -U nobody -d 1 | grep nobody | awk '{print $11}' | sed 's/%//' |
    ministat -A -w 73 | tail -1 | awk '{if ($NF > 1.0) exit 1}' ||
{ echo Broken; top -U nobody -d 1 | grep nobody; }
killall sched
wait

for i in `jot 3`; do
	echo "run #$i"
	(cd $mntpoint; /tmp/sched $((cpus + 1)))
done

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
rm -f /tmp/sched
mdconfig -d -u $mdstart
exit
EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libutil.h>
#include <pwd.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N 100 * 1024 * 1024

double r;
int parallel;

void
work(void)
{
	struct passwd *pw;
	struct timespec start, finish;
	double d1, d2;
	int i, j;
	volatile char *cp;

	while (access("rendezvous", R_OK) != 0)
		usleep(1);

	if ((pw = getpwnam("nobody")) == NULL)
		err(1, "no such user: nobody");
	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		err(1, "Can't drop privileges to \"nobody\"");
	endpwent();

	d1 = d2 = 0;
	cp = malloc(N);
	clock_gettime(CLOCK_REALTIME_PRECISE, &start);
	for (i = 0; i < 1; i++) {
		for (j = 0; j < INT_MAX; j++) {
			d1 = d1 + 1.0 / j;
			d2 = d1 + 0.8 / j;
			if (j % 1000 == 0) {
				cp[arc4random() % N] = j % 255;
			}
		}
	}
	r = d1 + d2;
	clock_gettime(CLOCK_REALTIME_PRECISE, &finish);
	timespecsub(&finish, &start, &finish);
#if defined(DEBUG)
	fprintf(stderr, "Elapsed time for pid %d: %.4f\n", getpid(),
	    finish.tv_sec + (double)finish.tv_nsec / 1e9);
#endif

	_exit(0);
}

int
main(int argc, char **argv)
{
	int fd, i;

	if (argc == 2)
		parallel = atoi(argv[1]);
	else
		errx(1, "Usage: %s <cpus>", argv[0]);

	for (i = 0; i < parallel; i++) {
		if (fork() == 0)
			work();
	}
	if ((fd = open("rendezvous", O_CREAT, 0644)) == -1)
		err(1, "open()");
	close(fd);
	for (i = 0; i < parallel; i++)
		wait(NULL);

	return (0);
}
