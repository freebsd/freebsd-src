#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# Test scenario by kib@

# "panic: vm_page_set_invalid: page 0xc3dfe8c0 is busy" seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/wire_no_page.c
mycc -o wire_no_page -Wall -Wextra wire_no_page.c -lpthread || exit 1
rm -f wire_no_page.c
cd $odir

cp /tmp/wire_no_page /tmp/wire_no_page2
start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 300 ]; do
	for i in `jot 50`; do
		/tmp/wire_no_page /tmp/wire_no_page2 &
	done
	wait
done

rm -f /tmp/wire_no_page /tmp/wire_no_page2
exit

EOF
/* $Id: wire_no_page.c,v 1.1 2013/05/17 07:50:30 kostik Exp kostik $ */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	struct stat st;
	char *p1, *p2;
	size_t len;
	pid_t child;
	int error, fd;

	if (argc < 2)
		errx(1, "file name ?");
	fd = open(argv[1], O_RDWR);
	if (fd == -1)
		err(1, "open %s", argv[1]);
	error = fstat(fd, &st);
	if (error == -1)
		err(1, "stat");
	len = round_page(st.st_size);
	p1 = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if ((void *)p1 == MAP_FAILED)
		err(1, "mmap");
	error = mlock(p1, len);
	if (error == -1)
		err(1, "mlock");
	p2 = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if ((void *)p2 == MAP_FAILED)
		err(1, "mmap");
	error = msync(p2, len, MS_SYNC | MS_INVALIDATE);
	if (error == -1)
		err(1, "msync");
	child = fork();
	if (child == -1)
		err(1, "fork");
	else if (child == 0)
		_exit(0);

	return (0);
}
