#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# Parallel write test.

# fsync: giving up on dirty
# 0xc9172168: tag devfs, type VCHR
#     usecount 1, writecount 0, refcount 31 mountedhere 0xc96bd300
#     flags (VI_ACTIVE)
#     v_object 0xc96e55a0 ref 0 pages 983 cleanbuf 27 dirtybuf 2
#     lock type devfs: EXCL by thread 0xcbedf340 (pid 58752, write, tid 100254)
# #0 0xc0c23f33 at __lockmgr_args+0xae3
# #1 0xc0cff943 at vop_stdlock+0x53
# #2 0xc12027c8 at VOP_LOCK1_APV+0x118
# #3 0xc0d2400a at _vn_lock+0xba
# #4 0xc0f3288b at ffs_sync+0x34b
# #5 0xc0f16cc5 at softdep_ast_cleanup_proc+0x205
# #6 0xc0ca3ac7 at userret+0x37
# #7 0xc11d021e at syscall+0x50e
# #8 0xc11bae3f at Xint0x80_syscall+0x2f
#         dev label/tmp

# Deadlock seen:
# https://people.freebsd.org/~pho/stress/log/write.txt

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/write.c
mycc -o write -Wall -Wextra -O2 -g write.c || exit 1
rm -f write.c
cd $odir

need=$((25 * 1024))
[ `df -k $(dirname $diskimage) | tail -1 | awk '{print int($4 / 1024)}'` \
    -lt $need ] &&
    printf "Need %d MB on %s.\n" $need `dirname $diskimage` && exit 0
wd=`dirname $diskimage`
wd="$wd/write.dir"
rm -rf $wd
mkdir -p $wd

(cd $wd; /tmp/write)
s=$?

rm -rf /tmp/write $wd
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define DONE 1
#define MAXBLK (2 * 1024 * 1024)
#define MAXPROC 32
#define MAXSIZ (9LL * 1024 * 1024 *1024)
#define RUNTIME (15 * 60)
#define SYNC 0

volatile u_int *share;
int parallel;

struct typ {
	off_t blocksize;
	off_t blocks;
	int sequential;
	int mindelay;
	int maxdelay;
} t[MAXPROC];

int
rnd(int mi, int ma)
{
        return (arc4random()  % (ma - mi + 1) + mi);
}

void
test(int indx, int num)
{
	ssize_t i, r;
	time_t start;
	int fd, n;
	char *buf, file[80];

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (unsigned int)parallel)
		;

	if ((buf = malloc(t[indx].blocksize)) == NULL)
		err(1, "malloc");
	snprintf(file, sizeof(file), "file.%06d.%06d", indx, num);
	n = 0;
	start = time(NULL);
	while (share[DONE] != (unsigned int)parallel) {
		setproctitle("test(%d) num %d, n %d", indx, num, n);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE))
		    == -1)
			err(1, "open(%s)", file);

		for (i = 0; i < t[indx].blocks; i++) {
			if (t[indx].sequential == 0)
				if (lseek(fd, 2LL << (arc4random() % 18),
				    SEEK_SET) == -1)
					err(1, "lseek");
			if ((r = write(fd, buf, t[indx].blocksize)) !=
			    t[indx].blocksize) {
				warn("write returned %zd\n", r);
				goto done;
			}
			usleep(rnd(t[indx].mindelay, t[indx].maxdelay));
		}

		close(fd);
		if (n++ == 0)
			atomic_add_int(&share[DONE], 1);
		if (time(NULL) - start >= RUNTIME / 4) {
#if defined(DEBUG)
			fprintf(stderr, "test(%d), %d Timed out\n", indx, num);
#endif
			break;
		}
	}
done:
	if (n++ == 0)
		atomic_add_int(&share[DONE], 1);

	_exit(0);
}

void
setup(void)
{
	int i;

	parallel = arc4random() % MAXPROC + 1;
	for (i = 0; i < parallel; i++) {
		if (arc4random() % 100 < 10)
			t[i].blocksize = (arc4random() + 1) % MAXBLK;
		else
			t[i].blocksize = 2 << (arc4random() % 20);
		t[i].sequential = arc4random() % 2;
		t[i].mindelay = arc4random() % 50;
		t[i].maxdelay = t[i].mindelay + arc4random() % 100;
		t[i].blocks = 2LL << (arc4random() % 18);
		if (t[i].blocks * t[i].blocksize > MAXSIZ)
			t[i].blocks = MAXSIZ / t[i].blocksize;
#if defined(DEBUG)
		fprintf(stderr, "%3d: blocksize %7lld, sequential %d, "
		    "mindelay %3d, maxdelay %3d, blocks %6lld, size %4lld MB\n",
		    i, (long long)t[i].blocksize, t[i].sequential, t[i].mindelay,
		    t[i].maxdelay, (long long)t[i].blocks,
		    (long long)t[i].blocksize * t[i].blocks / 1024 / 1024);
#endif
	}
}

int
main(void)
{
	size_t len;
	time_t start;
	int e, i, n, *pids, status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	n = 0;
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		setup();

		pids = malloc(sizeof(pid_t) * parallel);
		share[SYNC] = share[DONE] = 0;
		for (i = 0; i < parallel; i++) {
			if ((pids[i] = fork()) == 0)
				test(i, n);
		}
		for (i = 0; i < parallel; i++) {
			if (waitpid(pids[i], &status, 0) != pids[i])
				err(1, "waitpid %d", pids[i]);
			e += status == 0 ? 0 : 1;
		}
		n++;
		n = n % 10;
		free(pids);
	}

	return (e);
}
