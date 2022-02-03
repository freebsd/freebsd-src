#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# Test scenario inspired by alc@
# Threaded version in order to "use the same pmap", as pointed out by kib@

# https://people.freebsd.org/~pho/stress/log/kostik601.txt
# Fixed by r255396

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/wire_no_page.c
mycc -o mmap7 -Wall -Wextra -O0 wire_no_page.c -lpthread || exit 1
rm -f wire_no_page.c
cd $odir

(cd ../testcases/swap; ./swap -t 1m -i 2) &
sleep 1
cp /tmp/mmap7 /tmp/mmap7.inputfile
/tmp/mmap7 /tmp/mmap7.inputfile
while pkill -9 swap; do :; done
wait
rm -f /tmp/mmap7 /tmp/mmap7.inputfile
exit

EOF
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char *p1, *p2, *p3;
char c;
const char *file;
int fd;
size_t len;
struct stat st;

void *
test2(void *arg __unused)
{
	int error, i __unused;

	p1[arc4random() % len] = 1;
	p2[arc4random() % len] = 1;

	if (arc4random() % 100 < 30)
		i = p3[arc4random() % len];

	if (arc4random() % 100 < 50)
		if ((error = mlock(p1, len)) == -1)
			err(1, "mlock");
	if (arc4random() % 100 < 50)
		if ((error = msync(p1, len, MS_SYNC | MS_INVALIDATE)) == -1)
			if (errno != EBUSY)
				err(1, "msync");
	return (0);
}

void
test(void)
{
	pthread_t cp[3];
	int e, error, i;

	if ((fd = open(file, O_RDWR)) == -1)
		err(1, "open %s", file);
	if ((error = fstat(fd, &st)) == -1)
		err(1, "stat(%s)", file);
	len = round_page(st.st_size);
	if ((p1 = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		err(1, "mmap");
	if ((p2 = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		err(1, "mmap");
	if ((p3 = mmap(NULL, len, PROT_READ             , MAP_SHARED, fd, 0)) == MAP_FAILED)
		err(1, "mmap");

	for (i = 0; i < 3; i++)
		if ((e = pthread_create(&cp[i], NULL, test2, NULL)) != 0)
			errc(1, e, "pthread_create");
	for (i = 0; i < 3; i++)
		pthread_join(cp[i], NULL);

	if (munmap(p3, len) == -1)
		err(1, "unmap()");
	if (munmap(p2, len) == -1)
		err(1, "unmap()");
	if (munmap(p1, len) == -1)
		err(1, "unmap()");
	close(fd);

	_exit(0);
}

int
main(int argc, char *argv[])
{
	int i;

	if (argc != 2)
		errx(1, "Usage: %s <file>", argv[0]);
	file = argv[1];

	for (i = 0; i < 30000; i++) {
		if (fork() == 0)
			test();
		wait(NULL);
	}

	return (0);
}
