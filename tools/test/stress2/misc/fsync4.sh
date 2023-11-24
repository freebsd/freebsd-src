#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
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

# Regression test for D38114: Handle ERELOOKUP from VOP_FSYNC()

# "fsync4: msync(0x82d3cc000), file d1/d2/d3/d4/d5/../file.92660:
# Input/output error" seen

# Fixed by: 6189672e6008 - main - Handle ERELOOKUP from VOP_FSYNC() in
# several other places

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
pids=""
prog=$(basename "$0" .sh)
s=0
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/$prog.c
mycc -o $prog -Wall -Wextra -O0 -g $prog.c || exit 1
rm -f $prog.c
cd $odir

set -eu
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs -U md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $odir
../testcases/swap/swap -t 1m -i 20 -l 100 > /dev/null &
sleep .5
cd $mntpoint
mkdir -p d1/d2/d3/d4/d5
for i in `jot 8`; do
	$dir/$prog $i &
	pids="$pids $!"
done
cd $odir
for pid in $pids; do
	wait $pid
	[ $? -ne 0 ] && s=1
done

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/$prog
exit $s
EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>


#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RUNTIME 60
#define SIZ (1024 * 1024)

static time_t start;

int
main(void)
{
	size_t len;
	int fd, i, ps;
	char *cp;
	char *path = "d1/d2/d3/d4/d5";
	char d1[1024], d2[1024], file[1024];

	snprintf(d1, sizeof(d1), "%s/dir.%d", path, getpid());
	snprintf(d2, sizeof(d2), "%s/new.%d", path, getpid());
	snprintf(file, sizeof(file), "%s/../file.%d", path, getpid());

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		if (mkdir(d1, 0740) == -1)
			err(1, "mkdir(%s)", d1);
		if (rename(d1, d2) == -1)
			err(1, "rename(%s, %s)", d1, d2);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) == -1)
			err(1, "open%s()", file);
		len = SIZ;
		if (ftruncate(fd, len) == -1)
			err(1, "ftruncate");
		cp = mmap(NULL, len, PROT_READ | PROT_WRITE,
		    MAP_SHARED, fd, 0);
		    if (cp == MAP_FAILED)
		    	err(1, "mmap()");
		ps = getpagesize();
		for (i = 0; i < SIZ; i += ps)
			cp[i] = 1;
		if (msync(cp, 0, MS_SYNC) == -1)
			err(1, "msync(%p), file %s", cp, file);
		if (munmap(cp, len) == -1)
			err(1, "unmap()");
		close(fd);
		if (rename(d2, d1) == -1)
			err(1, "rename(%s, %s)", d2, d1);
		if (rmdir(d1) == -1)
			err(1, "rmdir(%s)", d1);
	}

	return (0);
}
