#!/bin/sh

#
# Copyright (c) 2016 Dell EMC Isilon
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

# Regression test for r310849. Hang in "vmpfw" was seen.

# Test scenario suggestion by kib.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

RUNTIME=60
old=`sysctl -n kern.ipc.shm_use_phys`
trap "sysctl kern.ipc.shm_use_phys=$old" EXIT INT
sysctl kern.ipc.shm_use_phys=1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/shm.c
mycc -o shm -Wall -Wextra -O0 -g shm.c -pthread || exit 1
rm -f shm.c

/tmp/shm &

s=0
sleep $RUNTIME
if pgrep -q shm; then
	if pgrep shm | xargs ps -lHp | grep -q vmpfw; then
		s=1
		echo FAIL
		pgrep shm | xargs ps -lHp
		pkill -9 shm
	fi
fi
wait

rm -rf /tmp/shm
exit $s

EOF
#include <sys/param.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static void *shmp;
static size_t len;
static int cont, shmid;

#define PAGES 64
#define RUNTIME (1 * 60)
#define STOP 1
#define SYNC 0

static void
cleanup(void)
{
	if (shmp != MAP_FAILED)
		shmdt(shmp);
	if (shmid != 0)
		shmctl(shmid, IPC_RMID, NULL);
}

static void *
t1(void *arg __unused)
{
	time_t start;
	char *cp;

	pthread_set_name_np(pthread_self(), __func__);
	start = time(NULL);
	while (cont == 1 && (time(NULL) - start) < RUNTIME) {
		if ((cp = mmap(NULL, len, PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
			err(1, "mmap");
		usleep(arc4random() % 400);
		if (munmap(cp, len) == -1)
			warn("unmap(%p, %zu)", shmp, len);
	}

	return (NULL);
}

static void *
t2(void *arg __unused)
{
	key_t shmkey;
	size_t i;
	time_t start;
	char *cp;

	pthread_set_name_np(pthread_self(), __func__);
	shmkey = ftok("/tmp", getpid());
	start = time(NULL);
	atexit(cleanup);
	while ((time(NULL) - start) < RUNTIME) {
		if ((shmid = shmget(shmkey, len, IPC_CREAT | IPC_EXCL |
		    0640)) == -1) {
			if (errno != ENOSPC && errno != EEXIST)
				err(1, "shmget (%s:%d)", __FILE__, __LINE__);
			break;
		}
		if ((shmp = shmat(shmid, NULL, 0)) == (void *) -1)
			break;
		cp = (char *)(shmp);
		for (i = 0; i < len; i = i + PAGE_SIZE)
			cp[i] = 1;
		if (shmdt(shmp) == -1) {
			if (errno != EINVAL)
				warn("shmdt(%p)", shmp);
		}
		if (shmctl(shmid, IPC_RMID, NULL) == -1)
			warn("shmctl IPC_RMID");
		usleep(50);

		shmp = MAP_FAILED;
		shmid = 0;
	}
	cont = 0;

	return (NULL);
}

static int
test(void)
{
	pthread_t tid[2];
	int i, rc;

	shmp = MAP_FAILED;

	cont = 1;
	if ((rc = pthread_create(&tid[0], NULL, t1, NULL)) != 0)
		errc(1, rc, "pthread_create()");
	if ((rc = pthread_create(&tid[1], NULL, t2, NULL)) != 0)
		errc(1, rc, "pthread_create()");

	for (i = 0; i < 2; i++)
		if ((rc = pthread_join(tid[i], NULL)) != 0)
			errc(1, rc, "pthread_join(%d)", i);

	return (0);
}

int
main(void)
{
	pid_t pid;
	time_t start;

	len = PAGES * PAGE_SIZE;
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		if ((pid = fork()) == 0) {
			test();
			exit(0);	/* calls cleanup() */
		}
		if (waitpid(pid, NULL,0) != pid)
			err(1, "waitpid(%d)", pid);
	}

	return(0);
}
