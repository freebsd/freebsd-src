#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# Hunt for https://people.freebsd.org/~pho/stress/log/log0018.txt

# Test scenario idea by kib@
# Scenario should be the following (without paging load, so that pages stay
# resident):
# - create some large file, say 2G
# - map it and touch every page to ensure that they all allocated
# - unmap, and map again, so while the pages are resident, they are not
#   installed into page table
# Do mincore(2) on the mapped region.
#

# The problem was not reproduced.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
[ `sysctl -n vm.swap_total` -eq 0 ] && exit 0
path=`dirname $diskimage`
[ `df -k $path | tail -1 | awk '{print int($4 / 1024 / 1024)}'` -lt 4 ] && exit 0

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mincore.c
mycc -o mincore -Wall -Wextra -O0 -g mincore.c || exit 1
rm -f mincore.c
cd $odir

(cd ../testcases/swap; ./swap -t 10m -i 20 -l 100) &
for i in `jot 20`; do
        [ `swapinfo | tail -1 | awk '{gsub("%", ""); print $5}'` -gt 0 ] && break
        sleep 5
	pgrep -q swap || break
done
echo "`date +%T` Go"

$dir/mincore $path; s=$?

while pkill swap; do :; done
wait
rm -rf $dir/mincore
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 2
#define RUNTIME 300
#define SIZE (2LL * 1024 * 1024 * 1024)

static void
test(char *dir)
{
	size_t i, len;
	int fd;
	char file[128], *p, *vec;

	len = SIZE;
	snprintf(file, sizeof(file), "%s/mincore.%d.file", dir, getpid());
	if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE))
	    == -1)
		err(1, "open(%s)", file);
	if (unlink(file) == -1)
		err(1, "unlink(%s)", file);
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncete()");
	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
	    MAP_FAILED)
		err(1, "mmap()");
	for (i = 0; i < len; i += PAGE_SIZE)
		p[i] = 1;
	if (munmap(p, len) == -1)
		err(1, "munmap()");
	if ((vec = malloc(len / PAGE_SIZE)) == NULL)
		err(1, "malloc");
	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
		MAP_FAILED)
			err(1, "mmap()");
	if (mincore(p, len, vec) == -1)
		err(1, "mincore()");
	if (munmap(p, len) == -1)
		err(1, "munmap()");
	close(fd);

	_exit(0);
}

int
main(int argc __unused, char *argv[])
{
	pid_t pids[PARALLEL];
	time_t start;
	int i;

	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			test(argv[1]);
	}

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], NULL, WNOHANG) == pids[i]) {
				if ((pids[i] = fork()) == 0)
					test(argv[1]);
			}
		}
	}

	for (i = 0; i < PARALLEL; i++) {
		if (waitpid(pids[i], NULL, 0) != pids[i])
			err(1, "waitpid");
	}

	return (0);
}
