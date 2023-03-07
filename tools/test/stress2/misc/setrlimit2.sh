#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

# Demonstrate that a mapped SHARED file can be updated past LIMIT_FSIZE

# Kostik wrote:
# This one should be reproducible when you
# - have file larger than e.g. RLIMIT_FSIZE
# - mmaped it without closing the file descriptor
# - dirty its pages beyond the limit
# - then unmap
# - then close the file descriptor.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

cat > /tmp/setrlimit2.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>
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
	signals++;
}

int
main(int argc, char *argv[])
{
	struct rlimit rlim;
	struct sigaction act;
	struct stat st;
	size_t len;
	int error, fd, ps;
	char *file, *p;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <data file>\n", argv[0]);
		exit(1);
	}
	act.sa_handler = handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	if (sigaction(SIGXFSZ, &act, NULL) != 0)
		err(1, "sigaction");

	file = argv[1];
	ps = getpagesize();
	if ((fd = open(file, O_RDWR)) == -1)
		err(1, "open(%s)", file);
	if ((error = fstat(fd, &st)) == -1)
		err(1, "stat(%s)", file);
	len = round_page(st.st_size);
	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
			err(1, "mmap");
	rlim.rlim_cur = rlim.rlim_max = len / 2;;
	if (setrlimit(RLIMIT_FSIZE, &rlim) == -1)
		err(1, "setrlimit(%ld)", len / 2);

	p[len / 2 + ps] = 'a';

	if (munmap(p, len) == -1)
		err(1, "unmap()");
	close(fd);

}
EOF
here=`pwd`
cd /tmp
mycc -o setrlimit2 -Wall -Wextra -O0 -g setrlimit2.c || exit 1
data=/tmp/setrlimit2.data
dd if=/dev/zero of=$data bs=1m count=1 status=none
h1=`md5 < $data`

./setrlimit2 $data

h2=`md5 < $data`
rm -f /tmp/setrlimit2 /tmp/setrlimit2.c
[ $h1 = $h2 ] && exit 1 || exit 0
