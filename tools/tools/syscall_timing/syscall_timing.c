/*-
 * Copyright (c) 2003-2004, 2010 Robert N. M. Watson
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
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct timespec ts_start, ts_end;

#define timespecsub(vvp, uvp)						\
	do {								\
		(vvp)->tv_sec -= (uvp)->tv_sec;				\
		(vvp)->tv_nsec -= (uvp)->tv_nsec;			\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)

static void
benchmark_start(void)
{

	assert(clock_gettime(CLOCK_REALTIME, &ts_start) == 0);
}

static void
benchmark_stop(void)
{

	assert(clock_gettime(CLOCK_REALTIME, &ts_end) == 0);
}
  
void
test_getuid(int num)
{
	int i;

	/*
	 * Thread-local data should require no locking if system
	 * call is MPSAFE.
	 */
	benchmark_start();
	for (i = 0; i < num; i++)
		getuid();
	benchmark_stop();
}

void
test_getppid(int num)
{
	int i;

	/*
	 * This is process-local, but can change, so will require a
	 * lock.
	 */
	benchmark_start();
	for (i = 0; i < num; i++)
		getppid();
	benchmark_stop();
}

void
test_clock_gettime(int num)
{
	struct timespec ts;
	int i;

	benchmark_start();
	for (i = 0; i < num; i++)
		(void)clock_gettime(CLOCK_REALTIME, &ts);
	benchmark_stop();
}

void
test_pipe(int num)
{
	int fd[2], i;

	/*
	 * pipe creation is expensive, as it will allocate a new file
	 * descriptor, allocate a new pipe, hook it all up, and return.
	 * Destroying is also expensive, as we now have to free up
	 * the file descriptors and return the pipe.
	 */
	if (pipe(fd) < 0)
		err(-1, "test_pipe: pipe");
	close(fd[0]);
	close(fd[1]);
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (pipe(fd) == -1)
			err(-1, "test_pipe: pipe");
		close(fd[0]);
		close(fd[1]);
	}
	benchmark_stop();
}

void
test_socket_stream(int num)
{
	int i, so;

	so = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (so < 0)
		err(-1, "test_socket_stream: socket");
	close(so);
	benchmark_start();
	for (i = 0; i < num; i++) {
		so = socket(PF_LOCAL, SOCK_STREAM, 0);
		if (so == -1)
			err(-1, "test_socket_stream: socket");
		close(so);
	}
	benchmark_stop();
}

void
test_socket_dgram(int num)
{
	int i, so;

	so = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (so < 0)
		err(-1, "test_socket_dgram: socket");
	close(so);
	benchmark_start();
	for (i = 0; i < num; i++) {
		so = socket(PF_LOCAL, SOCK_DGRAM, 0);
		if (so == -1)
			err(-1, "test_socket_dgram: socket");
		close(so);
	}
	benchmark_stop();
}

void
test_socketpair_stream(int num)
{
	int i, so[2];

	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, so) == -1)
		err(-1, "test_socketpair_stream: socketpair");
	close(so[0]);
	close(so[1]);
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (socketpair(PF_LOCAL, SOCK_STREAM, 0, so) == -1)
			err(-1, "test_socketpair_stream: socketpair");
		close(so[0]);
		close(so[1]);
	}
	benchmark_stop();
}

void
test_socketpair_dgram(int num)
{
	int i, so[2];

	if (socketpair(PF_LOCAL, SOCK_DGRAM, 0, so) == -1)
		err(-1, "test_socketpair_dgram: socketpair");
	close(so[0]);
	close(so[1]);
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (socketpair(PF_LOCAL, SOCK_DGRAM, 0, so) == -1)
			err(-1, "test_socketpair_dgram: socketpair");
		close(so[0]);
		close(so[1]);
	}
	benchmark_stop();
}

void
test_dup(int num)
{
	int fd, i, shmfd;

	shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
	if (shmfd < 0)
		err(-1, "test_dup: shm_open");
	fd = dup(shmfd);
	if (fd >= 0)
		close(fd);
	benchmark_start();
	for (i = 0; i < num; i++) {
		fd = dup(shmfd);
		if (fd >= 0)
			close(fd);
	}
	benchmark_stop();
	close(shmfd);
}

void
test_shmfd(int num)
{
	int i, shmfd;

	shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
	if (shmfd < 0)
		err(-1, "test_shmfd: shm_open");
	close(shmfd);
	benchmark_start();
	for (i = 0; i < num; i++) {
		shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
		if (shmfd < 0)
			err(-1, "test_shmfd: shm_open");
		close(shmfd);
	}
	benchmark_stop();
}

void
test_fstat_shmfd(int num)
{
	struct stat sb;
	int i, shmfd;

	shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
	if (shmfd < 0)
		err(-1, "test_fstat_shmfd: shm_open");
	if (fstat(shmfd, &sb) < 0)
		err(-1, "test_fstat_shmfd: fstat");
	benchmark_start();
	for (i = 0; i < num; i++)
		(void)fstat(shmfd, &sb);
	benchmark_stop();
	close(shmfd);
}

void
test_fork(int num)
{
	pid_t pid;
	int i;

	pid = fork();
	if (pid < 0)
		err(-1, "test_fork: fork");
	if (pid == 0)
		_exit(0);
	if (waitpid(pid, NULL, 0) < 0)
		err(-1, "test_fork: waitpid");
	benchmark_start();
	for (i = 0; i < num; i++) {
		pid = fork();
		if (pid < 0)
			err(-1, "test_fork: fork");
		if (pid == 0)
			_exit(0);
		if (waitpid(pid, NULL, 0) < 0)
			err(-1, "test_fork: waitpid");
	}
	benchmark_stop();
}

void
test_vfork(int num)
{
	pid_t pid;
	int i;

	pid = vfork();
	if (pid < 0)
		err(-1, "test_vfork: vfork");
	if (pid == 0)
		_exit(0);
	if (waitpid(pid, NULL, 0) < 0)
		err(-1, "test_vfork: waitpid");
	benchmark_start();
	for (i = 0; i < num; i++) {
		pid = vfork();
		if (pid < 0)
			err(-1, "test_vfork: vfork");
		if (pid == 0)
			_exit(0);
		if (waitpid(pid, NULL, 0) < 0)
			err(-1, "test_vfork: waitpid");
	}
	benchmark_stop();
}

#define	USR_BIN_TRUE	"/usr/bin/true"
static char *execve_args[] = { USR_BIN_TRUE, NULL};
extern char **environ;

void
test_fork_exec(int num)
{
	pid_t pid;
	int i;

	pid = fork();
	if (pid < 0)
		err(-1, "test_fork_exec: fork");
	if (pid == 0) {
		(void)execve(USR_BIN_TRUE, execve_args, environ);
		err(-1, "execve");
	}
	if (waitpid(pid, NULL, 0) < 0)
		err(-1, "test_fork: waitpid");
	benchmark_start();
	for (i = 0; i < num; i++) {
		pid = fork();
		if (pid < 0)
			err(-1, "test_fork_exec: fork");
		if (pid == 0) {
			(void)execve(USR_BIN_TRUE, execve_args, environ);
			err(-1, "test_fork_exec: execve");
		}
		if (waitpid(pid, NULL, 0) < 0)
			err(-1, "test_fork_exec: waitpid");
	}
	benchmark_stop();
}

void
test_vfork_exec(int num)
{
	pid_t pid;
	int i;

	pid = vfork();
	if (pid < 0)
		err(-1, "test_vfork_exec: vfork");
	if (pid == 0) {
		(void)execve(USR_BIN_TRUE, execve_args, environ);
		err(-1, "test_vfork_exec: execve");
	}
	if (waitpid(pid, NULL, 0) < 0)
		err(-1, "test_vfork_exec: waitpid");
	benchmark_start();
	for (i = 0; i < num; i++) {
		pid = vfork();
		if (pid < 0)
			err(-1, "test_vfork_exec: vfork");
		if (pid == 0) {
			(void)execve(USR_BIN_TRUE, execve_args, environ);
			err(-1, "execve");
		}
		if (waitpid(pid, NULL, 0) < 0)
			err(-1, "test_vfork_exec: waitpid");
	}
	benchmark_stop();
}

void
test_chroot(int num)
{
	int i;

	if (chroot("/") < 0)
		err(-1, "test_chroot: chroot");
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (chroot("/") < 0)
			err(-1, "test_chroot: chroot");
	}
	benchmark_stop();
}

void
test_setuid(int num)
{
	uid_t uid;
	int i;

	uid = getuid();
	if (setuid(uid) < 0)
		err(-1, "test_setuid: setuid");
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (setuid(uid) < 0)
			err(-1, "test_setuid: setuid");
	}
	benchmark_stop();
}

struct test {
	const char	*t_name;
	void		(*t_func)(int);
};

static const struct test tests[] = {
	{ "getuid", test_getuid },
	{ "getppid", test_getppid },
	{ "clock_gettime", test_clock_gettime },
	{ "pipe", test_pipe },
	{ "socket_stream", test_socket_stream },
	{ "socket_dgram", test_socket_dgram },
	{ "socketpair_stream", test_socketpair_stream },
	{ "socketpair_dgram", test_socketpair_dgram },
	{ "dup", test_dup },
	{ "shmfd", test_shmfd },
	{ "fstat_shmfd", test_fstat_shmfd },
	{ "fork", test_fork },
	{ "vfork", test_vfork },
	{ "fork_exec", test_fork_exec },
	{ "vfork_exec", test_vfork_exec },
	{ "chroot", test_chroot },
	{ "setuid", test_setuid },
};
static const int tests_count = sizeof(tests) / sizeof(tests[0]);

static void
usage(void)
{
	int i;

	fprintf(stderr, "syscall_timing [iterations] [loops] [test]\n");
	for (i = 0; i < tests_count; i++)
		fprintf(stderr, "  %s\n", tests[i].t_name);
	exit(-1);
}

int
main(int argc, char *argv[])
{
	struct timespec ts_res;
	const struct test *the_test;
	long long ll;
	char *endp;
	int i, j, k;
	int iterations, loops;

	if (argc < 4)
		usage();

	ll = strtoll(argv[1], &endp, 10);
	if (*endp != 0 || ll < 0 || ll > 100000)
		usage();
	iterations = ll;

	ll = strtoll(argv[2], &endp, 10);
	if (*endp != 0 || ll < 0 || ll > 100000)
		usage();
	loops = ll;

	assert(clock_getres(CLOCK_REALTIME, &ts_res) == 0);
	printf("Clock resolution: %ju.%ju\n", (uintmax_t)ts_res.tv_sec,
	    (uintmax_t)ts_res.tv_nsec);
	printf("test\tloop\ttotal\titerations\tperiteration\n");

	for (j = 3; j < argc; j++) {
		the_test = NULL;
		for (i = 0; i < tests_count; i++) {
			if (strcmp(argv[j], tests[i].t_name) == 0)
				the_test = &tests[i];
		}
		if (the_test == NULL)
			usage();

		/*
		 * Run one warmup, then do the real thing (loops) times.
		 */
		the_test->t_func(iterations);
		for (k = 0; k < loops; k++) {
			the_test->t_func(iterations);
			timespecsub(&ts_end, &ts_start);
			printf("%s\t%d\t", the_test->t_name, k);
			printf("%ju.%09ju\t%d\t", (uintmax_t)ts_end.tv_sec,
			    (uintmax_t)ts_end.tv_nsec, iterations);

		/*
		 * Note.  This assumes that each iteration takes less than
		 * a second, and that our total nanoseconds doesn't exceed
		 * the room in our arithmetic unit.  Fine for system calls,
		 * but not for long things.
		 */
			ts_end.tv_sec *= 1000000000 / iterations;
			printf("0.%09ju\n", (uintmax_t)(ts_end.tv_sec +
			    ts_end.tv_nsec / iterations));
		}
	}
	return (0);
}
