#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# Test with read/write length of INT_MAX (i386) or INT_MAX+1 (amd64)

# Seen:
# rdwr:  readv(). Expected 2147483648 (80000000), got -2147483648 (ffffffff80000000) bytes: No error: 0
# rdwr: writev(). Expected 2147483648 (80000000), got -2147483648 (ffffffff80000000) bytes: No error: 0
# Fixed by: 78101d437a92

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > rdwr.c
mycc -o rdwr -Wall rdwr.c || exit
rm -f rdwr.c

oldclamp=`sysctl debug.devfs_iosize_max_clamp 2>/dev/null |
    awk '{print $NF}'`
if [ `uname -m` = amd64 ]; then
	[ "$oldclamp" = "1" ] && sysctl debug.devfs_iosize_max_clamp=0
fi
for j in `jot 10`; do
	/tmp/rdwr || { echo FAIL; break; }
done
if [ `uname -m` = amd64 ]; then
	[ "$oldclamp" = "1" ] && sysctl debug.devfs_iosize_max_clamp=1
fi

rm -f /tmp/rdwr
exit
EOF
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>

static int s;

static void
er(char * syscall, size_t len, ssize_t ret)
{
	warn("%8s. Expected %zu (%zx), got %zd (%zx) bytes",
	    syscall, len, len, ret, ret);
	s=1;
}

int
main(int argc, char **argv)
{
	int fd1, fd2;
	size_t len;
	ssize_t r;
	void *p;
	struct iovec iov;

	alarm(120);
	if ((fd1 = open("/dev/null", O_RDWR, 0)) == -1)
		err(1, "open /dev/null");

	if ((fd2 = open("/dev/zero", O_RDWR)) == -1)
		err(1, "open /dev/zero");

	if (sizeof(size_t) == sizeof(int32_t))
		len = (size_t)INT_MAX;		/* i386  */
	else
		len = (size_t)INT_MAX + 1;	/* amd64 */

	if ((p = mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd2, 0)) ==
                        MAP_FAILED)
		err(1, "mmap");

	if ((r = read(fd2, p, len)) != len)
		er("read()", len, r);

	if ((r = write(fd1, p, len)) != len)
		er("write()", len, r);

	if ((r = pread(fd2, p, len, 0)) != len)
		er("pread()", len, r);

	if ((r = pwrite(fd1, p, len, 0)) != len)
		er("pwrite()", len, r);

	iov.iov_base = p;
	iov.iov_len = len;
	if ((r = readv(fd2, &iov, 1)) != len)
		er("readv()", len, r);

	if ((r = writev(fd1, &iov, 1)) != len)
		er("writev()", len, r);

	if ((r = preadv(fd2, &iov, 1, 0)) != len)
		er("preadv()", len, r);

	if ((r = pwritev(fd1, &iov, 1, 0)) != len)
		er("pwritev()", len, r);

	close(fd1);
	close(fd2);

	return (s);
}
