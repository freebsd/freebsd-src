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

# "panic: vm_pageout_flush: partially invalid page xx index 0/1" seen.
# Fixed in r255566.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/msync2.c
mycc -o msync2  -Wall -Wextra msync2.c || exit 1
rm -f msync2.c
cd $odir

dd if=/dev/zero bs=$((4096 + 1)) of=/tmp/msync2.inputfile count=1 \
    status=none
/tmp/msync2  /tmp/msync2.inputfile
rm -f /tmp/msync2  /tmp/msync2.inputfile
exit

EOF
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define RUNTIME 400

const char *file;
char c;

void
wr(void)
{
	struct stat st;
	char *p1;
	size_t len;
	int error, fd;

	if ((fd = open(file, O_RDWR)) == -1)
		err(1, "open %s", file);
	if ((error = fstat(fd, &st)) == -1)
		err(1, "stat(%s)", file);
	len = round_page(st.st_size);
	if ((p1 = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
	    MAP_FAILED)
		err(1, "mmap");
//	p1[len - 1] = 1;		/* No panic with this */
	p1[arc4random() % len] = 1;	/* Need this for the panic */

	if ((error = msync(p1, len, MS_SYNC | MS_INVALIDATE)) == -1)
		if (errno != EBUSY)
			err(1, "msync");

	if (munmap(p1, len) == -1)
		err(1, "unmap()");
	close(fd);

	_exit(0);
}

void
test(void)
{
#if 1
	int i;

	for (i = 0; i < 3; i++)
		if (fork() == 0)
			wr();
	for (i = 0; i < 3; i++)
		wait(NULL);
#else
	wr();	/* No problem here */
#endif

	_exit(0);
}

int
main(int argc, char *argv[])
{
	time_t start;

	if (argc != 2)
		errx(1, "Usage: %s <file>", argv[0]);
	file = argv[1];

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		if (fork() == 0)
			test();
		wait(NULL);
	}

	return (0);
}
