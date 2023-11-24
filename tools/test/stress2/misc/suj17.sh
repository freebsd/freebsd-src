#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# Truncate scenario for suj.
# "panic: worklist_insert: 0xc8bc5b00 freework(0x8009) already on list"
# seen.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > suj17.c
mycc -o suj17 -Wall -Wextra -O2 suj17.c
rm -f suj17.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1
[ $# -eq 1 ] && opt="$1"
[ $# -eq 0 ] && opt="-j"
echo "newfs $opt md$mdstart"
newfs $opt md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "cd $mntpoint; /tmp/suj17"
s=$?

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
checkfs /dev/md$mdstart || s=$?
mdconfig -d -u $mdstart
rm -f /tmp/suj17
exit $s
EOF
#include <fcntl.h>
#include <err.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define SIZ (1024 * 1024 - 1)
char buf[SIZ];

void
test()
{
	int fd, i;
	char name[128];
	off_t len = 104857600LL; /* 100 Mb */
	off_t pos;

	sprintf(name, "%06d", getpid());
	if ((fd = open(name, O_WRONLY | O_CREAT, 0666)) == -1)
			err(1, "open(%s)", name);
	for (i = 0; i < 100; i++) {
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write");
	}

	for (;;) {
		if (access("rendezvous", R_OK) == 0)
			break;
		sched_yield();
	}

	srand48(getpid());
	for (i = 0; i < 50000; i++) {
		pos = lrand48() % (len - sizeof(buf));
		if (ftruncate(fd, pos) == -1)
			err(1, "ftruncate");
		pos = lrand48() % (len - sizeof(buf));
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek");
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write");
	}
	close(fd);
	unlink(name);
	_exit(0);
}

int
main()
{
	int fd, i, j, status;

	for (i = 0; i < 1; i++) {
		for (j = 0; j < 6; j++) {
			if (fork() == 0)
				test();
		}
		if ((fd = open("rendezvous", O_CREAT, 0644)) == -1)
			err(1, "open()");
		close(fd);

		for (j = 0; j < 6; j++)
			wait(&status);
		unlink("rendezvous");
	}

	return (0);
}
