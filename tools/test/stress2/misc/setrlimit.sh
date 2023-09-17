#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# Test setrlimit() max file size and ftruncate()

# Problem seen:
# Testing UFS -O1
# setrlimit: ftruncate(5413) did not fail. limit = 2791
# Testing FFS -U
# setrlimit: ftruncate(9956) did not fail. limit = 7880
# Testing msdosfs
# setrlimit: ftruncate(9033) did not fail. limit = 5884
# Testing tmpfs
# setrlimit: ftruncate(123) did not fail. limit = 86

. ../default.cfg

cat > /tmp/setrlimit.c <<EOF
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int signals;

static void
handler(int sig __unused)
{
#if defined(DEBUG)
	fprintf(stderr, "Got signal SIGXFSZ\n");
#endif
	signals++;
}

void
test(int argc, char *argv[])
{
	struct rlimit rlim;
	rlim_t limit, sz;
	struct sigaction act;
	long pos;
	int e, expected, fd;
	char file[] = "setrlimit.file";

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <FS size>\n", argv[0]);
		exit(1);
	}
	expected = signals = 0;
	sz = atol(argv[1]);
	arc4random_buf(&limit, sizeof(limit));
	if (limit < 0)
		limit = -limit;
	limit = limit % sz + 1;
	rlim.rlim_cur = rlim.rlim_max = limit;
	if (setrlimit(RLIMIT_FSIZE, &rlim) == -1)
		err(1, "setrlimit(%ld)", limit);

	act.sa_handler = handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	if (sigaction(SIGXFSZ, &act, NULL) != 0)
		err(1, "sigaction");

	if ((fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE)) == -1)
		err(1, "open(%s)", file);

	e  = 0;
	arc4random_buf(&pos, sizeof(pos));
	if (pos < 0)
		pos = -pos;
	pos = pos % (limit * 2);
	if (pos > limit)
		expected = 1;
	if (ftruncate(fd, pos) == -1) {
		e = errno;
		if (pos <= limit)
			errc(1, e, "ftruncate(%ld), limit = %ld", pos, limit);
	} else {
		if (pos > limit)
			errx(1, "ftruncate(%ld) did not fail. limit = %ld", pos, limit);
	}

	if (lseek(fd, limit - 1, SEEK_SET) == -1)
		err(1, "lseek(limit - 1)");
	if (write(fd, "a", 1) != 1)
		err(1, "write() at limit - 1. limit = %ld", limit);

	if (write(fd, "b", 1) != -1)
		err(1, "write() at limit. limit = %ld", limit);
	expected++;

	/* Partial write test. No signal is expected */
	if (lseek(fd, limit - 1, SEEK_SET) == -1)
		err(1, "lseek(limit - 1)");
	if (write(fd, "12", 2) != 1)
		err(1, "write() at limit - 1. limit = %ld", limit);

	if (signals != expected)
		errx(1, "Expected %d signals, got %d", expected, signals);

	close(fd);
	unlink(file);
}

int
main(int argc, char *argv[])
{
	int i;

	for (i = 0; i < 100; i++)
		test(argc, argv);

}
EOF

here=`pwd`
s=0
cc -o /tmp/setrlimit -Wall -Wextra -O0 -g /tmp/setrlimit.c || exit 1

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart

echo "Testing UFS -O1"
mdconfig -t swap -s 1g -u $mdstart
newfs -O1 /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
cd $mntpoint; /tmp/setrlimit 10000 ||  s=1
cd $here
umount $mntpoint
mdconfig -d -u $mdstart

echo "Testing FFS -U"
mdconfig -t swap -s 1g -u $mdstart
newfs -U /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
cd $mntpoint; /tmp/setrlimit 10000 || s=$((s + 2))
cd $here
umount $mntpoint
mdconfig -d -u $mdstart

echo "Testing msdosfs"
mdconfig -t swap -s 1g -u $mdstart
newfs_msdos -F 32 -b 8192 /dev/md$mdstart > /dev/null 2>&1
mount -t msdosfs /dev/md$mdstart $mntpoint
cd $mntpoint; /tmp/setrlimit 10000 || s=$((s + 4))
cd $here
umount $mntpoint
mdconfig -d -u $mdstart

echo "Testing tmpfs"
mount -o size=20000 -t tmpfs dummy $mntpoint
cd $mntpoint; /tmp/setrlimit 10000 || s=$((s + 8))
cd $here
umount $mntpoint

rm -f /tmp/setrlimit /tmp/setrlimit.c
exit $s
