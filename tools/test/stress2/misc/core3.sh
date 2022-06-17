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

# Test multiple (parallel) core dumps and mount / umount.
# mount(8) stuck in "ufs" or "tmpfs".
# http://people.freebsd.org/~pho/stress/log/kostik724.txt
# Fixed by r272535.
# On i386 pgrep(1) loops. Fixed by r272566.

# "Sleeping on "pmapdi" with the following non-sleepable locks held:"
# https://people.freebsd.org/~pho/stress/log/kostik883.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > core3.c
mycc -o core3 -Wall -Wextra -O2 core3.c || exit 1
rm -f core3.c
cd $odir

mount | grep -q "on $mntpoint " && umount $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
mkdir $mntpoint/d
chmod 777 $mntpoint/d

su $testuser -c "/tmp/core3 $mntpoint/d" &
pid=$!
sleep 1

while pgrep -q core3; do
	[ -d $mntpoint/d ] &&
	   umount -f $mntpoint
done > /dev/null 2>&1  &
while pgrep -q core3; do
	[ -d $mntpoint/d ] ||
	   mount /dev/md$mdstart $mntpoint
done > /dev/null 2>&1
wait $pid
status=$?
mount | grep -q "on $mntpoint " &&
	    umount -f $mntpoint
mdconfig -d -u $mdstart
[ $status -ne 0 ] && exit $status

# tmpfs
mount -o size=1g -t tmpfs tmpfs $mntpoint
su $testuser -c "/tmp/core3 $mntpoint/d" &
pid=$!
sleep 1

while pgrep -q core3; do
	[ -d $mntpoint/d ] &&
	   umount -f $mntpoint
done > /dev/null &
while pgrep -q core3; do
	if [ ! -d $mntpoint/d ]; then
		mount -t tmpfs tmpfs $mntpoint
		mkdir $mntpoint/d
	fi
done
wait $pid
status=$?
for i in `jot 5` ; do
	mount | grep -q "on $mntpoint " || break
	umount -f $mntpoint
	sleep 1
done
rm -f /tmp/core3
exit $status
EOF
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 64
#define SIZ (4 * 1024 * 1024)
#define TIMEDOUT 22

void *p;

static void
hand(int i __unused) {	/* handler */
	_exit(TIMEDOUT);
}

void
test(char *argv[])
{
	size_t len;

	len = SIZ;
	p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);

	/*
	 * This loop caused mount to wait in "ufs".
	 * Adding a usleep(200) would remove the hang.
	 */
	signal(SIGALRM, hand);
	alarm(600);
	while (chdir(argv[1]) == -1)
		;

	raise(SIGSEGV);

	_exit(0);
}

int
main(int argc, char *argv[])
{
	time_t start;
	int i, s, status;

	if (argc != 2)
		errx(1, "Usage: %s <path>", argv[0]);

	status = 0;
	start = time(NULL);
	while (time(NULL) - start < 600 && status == 0) {
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				test(argv);
		}
		for (i = 0; i < PARALLEL; i++) {
			wait(&s);
			if (WEXITSTATUS(s) == TIMEDOUT)
				status = 1;
		}
	}

	return (status);
}
