/*-
 * Copyright (c) 2003-2004 Robert N. M. Watson
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define timespecsub(vvp, uvp)                                           \
        do {                                                            \
                (vvp)->tv_sec -= (uvp)->tv_sec;                         \
                (vvp)->tv_nsec -= (uvp)->tv_nsec;                       \
                if ((vvp)->tv_nsec < 0) {                               \
                        (vvp)->tv_sec--;                                \
                        (vvp)->tv_nsec += 1000000000;                   \
                }                                                       \
        } while (0)

inline void
test_getuid(int num)
{
	int i;

	/*
	 * Thread-local data should require no locking if system
	 * call is MPSAFE.
	 */
	for (i = 0; i < num; i++)
		getuid();
}

inline void
test_getppid(int num)
{
	int i;

	/*
	 * This is process-local, but can change, so will require a
	 * lock.
	 */
	for (i = 0; i < num; i++)
		getppid();
}

inline void
test_clock_gettime(int num)
{
	struct timespec ts;
	int i;

	for (i = 0; i < num; i++) {
		if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
			perror("clock_gettime");
			exit(-1);
		}
	}
}

inline void
test_pipe(int num)
{
	int i;

	/*
	 * pipe creation is expensive, as it will allocate a new file
	 * descriptor, allocate a new pipe, hook it all up, and return.
	 * Destroying is also expensive, as we now have to free up
	 * the file descriptors and return the pipe.
	 */
	for (i = 0; i < num; i++) {
		int fd[2];
		if (pipe(fd) == -1) {
			perror("pipe");
			exit(-1);
		}

		close(fd[0]);
		close(fd[1]);
	}
}

inline void
test_socket(int num)
{
	int i, so;

	/*
	 * Sockets are also expensive, but unlike pipes, currently
	 * require Giant.
	 */
	for (i = 0; i < num; i++) {
		so = socket(PF_LOCAL, SOCK_STREAM, 0);
		if (so == -1) {
			perror("socket");
			exit(-1);
		}
		close(so);
	}
}

static void
usage(void)
{

	fprintf(stderr, "syscall_timing [iterations] [test]\n");
	fprintf(stderr, "supported tests: getuid getppid clock_gettime "
	    "pipe socket\n");
	exit(-1);
}

int
main(int argc, char *argv[])
{
	struct timespec ts_start, ts_end, ts_res;
	int count;

	if (argc != 3)
		usage();
	count = atoi(argv[1]);

	assert(clock_getres(CLOCK_REALTIME, &ts_res) == 0);
	printf("Clock resolution: %d.%09lu\n", ts_res.tv_sec, ts_res.tv_nsec);

	if (strcmp(argv[2], "getuid") == 0) {
		assert(clock_gettime(CLOCK_REALTIME, &ts_start) == 0);
		test_getuid(count);
		assert(clock_gettime(CLOCK_REALTIME, &ts_end) == 0);
	} else if (strcmp(argv[2], "getppid") == 0) {
		assert(clock_gettime(CLOCK_REALTIME, &ts_start) == 0);
		test_getppid(count);
		assert(clock_gettime(CLOCK_REALTIME, &ts_end) == 0);
	} else if (strcmp(argv[2], "clock_gettime") == 0) {
		assert(clock_gettime(CLOCK_REALTIME, &ts_start) == 0);
		test_clock_gettime(count);
		assert(clock_gettime(CLOCK_REALTIME, &ts_end) == 0);
	} else if (strcmp(argv[2], "pipe") == 0) {
		assert(clock_gettime(CLOCK_REALTIME, &ts_start) == 0);
		test_pipe(count);
		assert(clock_gettime(CLOCK_REALTIME, &ts_end) == 0);
	} else if (strcmp(argv[2], "socket") == 0) {
		assert(clock_gettime(CLOCK_REALTIME, &ts_start) == 0);
		test_socket(count);
		assert(clock_gettime(CLOCK_REALTIME, &ts_end) == 0);
	 } else
		usage();

	timespecsub(&ts_end, &ts_start);

	printf("test: %s\n", argv[2]);

	printf("%d.%09lu for %d iterations\n", ts_end.tv_sec,
	    ts_end.tv_nsec, count);

	/*
	 * Note.  This assumes that each iteration takes less than
	 * a second, and that our total nanoseconds doesn't exceed
	 * the room in our arithmetic unit.  Fine for system calls,
	 * but not for long things.
	 */
	ts_end.tv_sec *= 1000000000 / count;
	printf("0.%09lu per/iteration\n", 
	    ts_end.tv_sec + ts_end.tv_nsec / count);
	return (0);
}
