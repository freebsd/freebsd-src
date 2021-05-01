/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* Test shared memory */

#include <sys/param.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "stress.h"

static int	shmid = -1;
static key_t	shmkey;
static char	*shm_buf;

static int	semid = -1;
static key_t	semkey;
static struct	sembuf sop[2];

static size_t	pgsize;
static pid_t	pid;

int
setup(int nb __unused)
{
	int seed;

	pgsize = sysconf(_SC_PAGESIZE);

	seed = getpid();
	shmkey = ftok("/tmp", seed);
	if ((shmid = shmget(shmkey, 10 * pgsize, IPC_CREAT | IPC_EXCL | 0640)) == -1) {
		if (errno == ENOSPC) {
			fprintf(stderr, "Max number of semaphores reached.\n");
			exit(1);
		}
		err(1, "shmget (%s:%d)", __FILE__, __LINE__);
	}

	shm_buf = 0;
	if ((shm_buf = shmat(shmid, NULL, 0)) == (void *) -1)
		err(1, "sender: shmat (%s:%d)", __FILE__, __LINE__);

	semkey = ftok("/var", seed);
	if ((semid = semget(semkey, 2, IPC_CREAT | IPC_EXCL | 0640)) == -1) {
		if (errno == ENOSPC) {
			fprintf(stderr, "Max number of semaphores reached.\n");
			exit(1);
		}
		err(1, "semget (%s:%d)", __FILE__, __LINE__);
	}
        /* Initialize the semaphore. */
        sop[0].sem_num = 0;
        sop[0].sem_op  = 0;  /* This is the number of runs without queuing. */
        sop[0].sem_flg = 0;
        sop[1].sem_num = 1;
        sop[1].sem_op  = 0;  /* This is the number of runs without queuing. */
        sop[1].sem_flg = 0;
        if (semop(semid, sop, 2) == -1)
            err(1, "init: semop (%s:%d)", __FILE__, __LINE__);
        return (0);
}

void
cleanup(void)
{
	if (shmid != -1)
		if (shmctl(shmid, IPC_RMID, NULL) == -1 && errno != EINVAL)
			warn("shmctl IPC_RMID (%s:%d)", __FILE__, __LINE__);
	if (semid != -1)
		if (semctl(semid, 0, IPC_RMID, 0) == -1 && errno != EINVAL)
			warn("shmctl IPC_RMID (%s:%d)", __FILE__, __LINE__);
}

static void
Wait(int i) {
		sop[0].sem_num = i;
		sop[0].sem_op = -1;
		if (semop(semid, sop, 1) == -1) {
			if (errno != EINTR && errno != EIDRM && errno != EINVAL)
				warn("Wait: semop (%s:%d)", __FILE__, __LINE__);
			done_testing = 1;
		}
}

static void
Sig(int i) {
		sop[0].sem_num = i;
		sop[0].sem_op = 1;
		if (semop(semid, sop, 1) == -1) {
			if (errno != EINTR && errno != EIDRM && errno != EINVAL)
			warn("Sig: semop (%s:%d)", __FILE__, __LINE__);
			done_testing = 1;
		}
}

int
test(void)
{
	int i = 0;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(2);
	}

	if (pid == 0) {	/* child */
		i = 0;
		for (;;) {
			Wait(1);
			if (done_testing == 1)
				break;
			if (shm_buf[i] != (i % 128)) {
				fprintf(stderr,
					"child %d: expected %d, got %d\n",
					getpid(), i % 128, shm_buf[i]);
				break;
			}
			shm_buf[i] = 0;
			i = (i + 1) % (10 * pgsize);
			shm_buf[i] = (i % 128);
			i = (i + 1) % (10 * pgsize);
			Sig(0);
		}
		_exit(0);

	} else {	/* parent */
		i = 0;
		for (;;) {
			shm_buf[i] = (i % 128);
			Sig(1);
			i = (i + 1) % (10 * pgsize);
			Wait(0);
			if (done_testing == 1)
				break;
			if (shm_buf[i] != (i % 128)) {
				fprintf(stderr,
					"parent(%d): expected %d, got %d\n",
					getpid(), i % 128, shm_buf[i]);
				break;
			}
			shm_buf[i] = 0;
			i = (i + 1) % (10 * pgsize);
		}
		kill(pid, SIGHUP);
		kill(pid, SIGKILL);
	}
        return (0);
}
