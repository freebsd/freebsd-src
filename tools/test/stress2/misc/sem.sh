#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Regression test for panic in semexit_myhook
# Test scenario by kib@

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > sem.c
mycc -o sem -Wall sem.c
rm -f sem.c

cd $RUNDIR/..
for i in `jot 5`; do
	/tmp/sem&
done
for i in `jot 5`; do
	wait
done

rm -f /tmp/sem

exit
EOF
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/wait.h>
#include <sched.h>
#include <errno.h>
#include <err.h>

int	semid = -1;
key_t	semkey;
struct	sembuf sop[2];

size_t	pgsize;
pid_t	pid;

void
random_work(void)
{
	int i, n;

	n = (arc4random() % 5000) + 200;
	for (i = 0; i < n; i++)
		(void) getpid();
}

int
main()
{
	int i, j, seed, status;

	seed = getpid();
	semkey = ftok("/", seed);

	for (i = 0; i < 5000; i++) {

		pid = fork();
		if (pid == -1) {
			perror("fork");
			exit(2);
		}

		if (pid == 0) {	/* child */
			j = 0;
			/* get sem */
			do {
				sched_yield();
				semid = semget(semkey, 0, 0);
			} while (semid == -1 && j++ < 10000);
			if (semid == -1)
				exit(1);

			/*
			 * Attempt to acquire the semaphore.
			 */
			sop[0].sem_num = 0;
			sop[0].sem_op  = -1;
			sop[0].sem_flg = SEM_UNDO;

			semop(semid, sop, 1);
			random_work();
			_exit(0);

		} else {	/* parent */
		/* create sem */
			if ((semid = semget(semkey, 1, IPC_CREAT | 010640)) == -1)
				err(1, "semget (%s:%d)", __FILE__, __LINE__);
			usleep(2000);

			sop[0].sem_num = 0;
			sop[0].sem_op  = 1;
			sop[0].sem_flg = 0;
			if (semop(semid, sop, 1) == -1)
				warn("init: semop (%s:%d)", __FILE__, __LINE__);

			random_work();
			if (semctl(semid, 0, IPC_RMID, 0) == -1 && errno != EINVAL)
				warn("shmctl IPC_RMID (%s:%d)", __FILE__, __LINE__);

		}
		waitpid(pid, &status, 0);
	}
        return (0);
}
