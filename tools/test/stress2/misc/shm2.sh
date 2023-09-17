#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# No problems seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/shm2.c
mycc -o shm2 -Wall -Wextra -O0 -g shm2.c || exit 1
rm -f shm2.c

(cd $odir/../testcases/swap; ./swap -t 2m -i 20 -h -l 100) &
/tmp/shm2
while pkill swap; do sleep 1; done
wait

rm -rf /tmp/shm2
exit $s

EOF
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <vm/vm_param.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define INCARNATIONS 32

static unsigned long size, original;
static int runtime, utime;

static void
setup(void)
{
	struct rlimit rlp;

	size = size / INCARNATIONS;
	original = size;
	if (size == 0)
		errx(1, "Argument too small");

	if (getrlimit(RLIMIT_DATA, &rlp) < 0)
		err(1,"getrlimit");
	rlp.rlim_cur -= 1024 * 1024;

	if (size > (unsigned long)rlp.rlim_cur)
		size = rlp.rlim_cur;

#if 0
	printf("setup: pid %d. Total %luMb\n",
		getpid(), size / 1024 / 1024 * INCARNATIONS);
#endif

	if (size == 0)
		errx(1, "Argument too small");

	return;
}

static int
test(void)
{
	key_t shmkey;
	volatile char *c;
	int page, shmid;
	unsigned long i, j;
	time_t start;

	shmkey = ftok("/tmp", getpid());
	shmid = -1;
	while (shmid == -1) {
		if ((shmid = shmget(shmkey, size, IPC_CREAT | IPC_EXCL | 0640)) == -1) {
			if (errno != ENOSPC && errno != EEXIST)
				err(1, "shmget (%s:%d)", __FILE__, __LINE__);
		} else
			break;
		size -=  1024;
	}
	if ((c = shmat(shmid, NULL, 0)) == (void *) -1)
		err(1, "shmat");
	if (size != original)
		printf("shm size changed from %ld kb to %ld kb\n",
		    original / 1024, size / 1024);
	page = getpagesize();
	start = time(NULL);
	while ((time(NULL) - start) < runtime) {
		i = j = 0;
		while (i < size) {
			c[i] = 1;
			i += page;
			if ((time(NULL) - start) >= runtime)
				break;
//			usleep(utime);
		}
	}
	if (shmdt((void *)c) == -1) {
		if (errno != EINVAL)
			warn("shmdt(%p)", c);
	}
	if (shmctl(shmid, IPC_RMID, NULL) == -1)
		warn("shmctl IPC_RMID");

	_exit(0);
}

int
main(void)
{
	pid_t pids[INCARNATIONS];
	int i;

	runtime = 120;	/* 2 minutes */
	utime = 1000;	/* 0.001 sec */
	size = 512 * 1024 * 1024;
	setup();

	for (i = 0; i < INCARNATIONS; i++)
		if ((pids[i] = fork()) == 0)
			test();

	for (i = 0; i < INCARNATIONS; i++)
		if (waitpid(pids[i], NULL, 0) != pids[i])
			err(1, "waitpid(%d)", pids[i]);

	return (0);
}
