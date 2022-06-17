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

# Test that fails on various non FreeBSD file systems.
# 3.13.0-74-generic #118-Ubuntu SMP: Mismatch @ count 214, 0 != 123
# OK on OS X (Darwin Kernel Version 15.5.0)

# Scanario by: Michael Ubell ubell mindspring com.

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

sed '1,/^EOF/d' < $0 > /tmp/overlap.c
mycc -o /tmp/overlap -Wall -Wextra -O2 /tmp/overlap.c -lpthread || exit 1
rm -f /tmp/overlap.c

size="1g"
mdconfig -a -t swap -s $size -u $mdstart || exit 1

newfs -U md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

(cd $mntpoint; /tmp/overlap)
s=$?

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/overlap
exit $s
EOF
#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

char file[128];
int bsiz, siz;

void
handler(int s __unused)
{
	_exit(0);
}

void *
writer(void *arg __unused) {
	int fdes, count;
	ssize_t nwrite;
	int *buf;

	if ((fdes = open(file, O_RDWR|O_CREAT, 0664)) == -1)
		err(1, "open(%s)", file);

	count = 0;
	buf = malloc(bsiz * sizeof(int));
	buf[0] = buf[bsiz - 1] = 0;
	while ((nwrite = pwrite(fdes, buf, siz, 0)) != -1) {
		if (nwrite < siz)
			err(1, "pwrite @ count %d, nwrite %zd", count, nwrite);
		buf[0] = ++buf[bsiz - 1];
		count++;
	}

	err(1, "pwrite()");
}

void *
reader(void *arg __unused) {
	int fdes, count;
	ssize_t nread;
	int *buf;

	if ((fdes = open(file, O_RDWR|O_CREAT, 0664)) == -1)
		err(1, "open(%s)", file);
	count = 0;

	buf = malloc(bsiz * sizeof(int));
	while ((nread = pread(fdes, buf, siz, 0)) == 0)
		continue;

	do {
		if (nread < siz)
			err(1, "pread @ count %d, nread %zd", count, nread);
		if (buf[0] != buf[bsiz - 1]) {
			printf("Mismatch @ count %d, %d != %d\n",
			    count, buf[0], buf[bsiz - 1]);
			abort();
		}
		count++;
	} while ((nread = pread(fdes, buf, siz, 0)) != -1);

	err(1, "pread()");
}

int
main(int argc, char **argv) {
	pthread_t rp, wp;
	int ret;
	void *exitstatus;

	snprintf(file, sizeof(file), "test.%0d5", getpid());
	siz = 65536;
	if (argc == 2)
		siz = atoi(argv[1]);

	bsiz = siz / sizeof(int);

	signal(SIGALRM, handler);
	alarm(300);

	if ((ret = pthread_create(&wp, NULL, writer, NULL)) != 0)
		errc(1, ret, "pthread_create");
	if ((ret = pthread_create(&rp, NULL, reader, NULL)) != 0)
		errc(1, ret, "pthread_create");

	pthread_join(rp, &exitstatus);
	pthread_join(wp, &exitstatus);

	unlink(file);
	return (0);
}
