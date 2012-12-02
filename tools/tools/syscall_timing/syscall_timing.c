/*-
 * Copyright (c) 2003-2004, 2010 Robert N. M. Watson
 * All rights reserved.
 *
 * Portions of this software were developed at the University of Cambridge
 * Computer Laboratory with support from a grant from Google, Inc.
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
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct timespec ts_start, ts_end;
static int alarm_timeout;
static volatile int alarm_fired;

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
alarm_handler(int signum)
{

	alarm_fired = 1;
}

static void
benchmark_start(void)
{
	int error;

	alarm_fired = 0;
	if (alarm_timeout) {
		signal(SIGALRM, alarm_handler);
		alarm(alarm_timeout);
	}
	error = clock_gettime(CLOCK_REALTIME, &ts_start);
	assert(error == 0);
}

static void
benchmark_stop(void)
{
	int error;

	error = clock_gettime(CLOCK_REALTIME, &ts_end);
	assert(error == 0);
}
  
uintmax_t
test_getuid(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i;

	/*
	 * Thread-local data should require no locking if system
	 * call is MPSAFE.
	 */
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		getuid();
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_getppid(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i;

	/*
	 * This is process-local, but can change, so will require a
	 * lock.
	 */
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		getppid();
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_clock_gettime(uintmax_t num, uintmax_t int_arg, const char *path)
{
	struct timespec ts;
	uintmax_t i;

	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		(void)clock_gettime(CLOCK_REALTIME, &ts);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_gettimeofday(uintmax_t num, uintmax_t int_arg, const char *path)
{
	struct timeval tv;
	uintmax_t i;

	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		(void)gettimeofday(&tv, NULL);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_pipe(uintmax_t num, uintmax_t int_arg, const char *path)
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
		if (alarm_fired)
			break;
		if (pipe(fd) == -1)
			err(-1, "test_pipe: pipe");
		close(fd[0]);
		close(fd[1]);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_socket_stream(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i, so;

	so = socket(int_arg, SOCK_STREAM, 0);
	if (so < 0)
		err(-1, "test_socket_stream: socket");
	close(so);
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		so = socket(int_arg, SOCK_STREAM, 0);
		if (so == -1)
			err(-1, "test_socket_stream: socket");
		close(so);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_socket_dgram(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i, so;

	so = socket(int_arg, SOCK_DGRAM, 0);
	if (so < 0)
		err(-1, "test_socket_dgram: socket");
	close(so);
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		so = socket(int_arg, SOCK_DGRAM, 0);
		if (so == -1)
			err(-1, "test_socket_dgram: socket");
		close(so);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_socketpair_stream(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i;
	int so[2];

	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, so) == -1)
		err(-1, "test_socketpair_stream: socketpair");
	close(so[0]);
	close(so[1]);
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		if (socketpair(PF_LOCAL, SOCK_STREAM, 0, so) == -1)
			err(-1, "test_socketpair_stream: socketpair");
		close(so[0]);
		close(so[1]);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_socketpair_dgram(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i;
	int so[2];

	if (socketpair(PF_LOCAL, SOCK_DGRAM, 0, so) == -1)
		err(-1, "test_socketpair_dgram: socketpair");
	close(so[0]);
	close(so[1]);
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		if (socketpair(PF_LOCAL, SOCK_DGRAM, 0, so) == -1)
			err(-1, "test_socketpair_dgram: socketpair");
		close(so[0]);
		close(so[1]);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_create_unlink(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i;
	int fd;

	(void)unlink(path);
	fd = open(path, O_RDWR | O_CREAT, 0600);
	if (fd < 0)
		err(-1, "test_create_unlink: create: %s", path);
	close(fd);
	if (unlink(path) < 0)
		err(-1, "test_create_unlink: unlink: %s", path);
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		fd = open(path, O_RDWR | O_CREAT, 0600);
		if (fd < 0)
			err(-1, "test_create_unlink: create: %s", path);
		close(fd);
		if (unlink(path) < 0)
			err(-1, "test_create_unlink: unlink: %s", path);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_open_close(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		err(-1, "test_open_close: %s", path);
	close(fd);

	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		fd = open(path, O_RDONLY);
		if (fd < 0)
			err(-1, "test_open_close: %s", path);
		close(fd);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_read(uintmax_t num, uintmax_t int_arg, const char *path)
{
	char buf[int_arg];
	uintmax_t i;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		err(-1, "test_open_read: %s", path);
	(void)pread(fd, buf, int_arg, 0);

	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		(void)pread(fd, buf, int_arg, 0);
	}
	benchmark_stop();
	close(fd);
	return (i);
}

uintmax_t
test_open_read_close(uintmax_t num, uintmax_t int_arg, const char *path)
{
	char buf[int_arg];
	uintmax_t i;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		err(-1, "test_open_read_close: %s", path);
	(void)read(fd, buf, int_arg);
	close(fd);

	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		fd = open(path, O_RDONLY);
		if (fd < 0)
			err(-1, "test_open_read_close: %s", path);
		(void)read(fd, buf, int_arg);
		close(fd);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_dup(uintmax_t num, uintmax_t int_arg, const char *path)
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
		if (alarm_fired)
			break;
		fd = dup(shmfd);
		if (fd >= 0)
			close(fd);
	}
	benchmark_stop();
	close(shmfd);
	return (i);
}

uintmax_t
test_shmfd(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i, shmfd;

	shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
	if (shmfd < 0)
		err(-1, "test_shmfd: shm_open");
	close(shmfd);
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
		if (shmfd < 0)
			err(-1, "test_shmfd: shm_open");
		close(shmfd);
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_fstat_shmfd(uintmax_t num, uintmax_t int_arg, const char *path)
{
	struct stat sb;
	uintmax_t i, shmfd;

	shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
	if (shmfd < 0)
		err(-1, "test_fstat_shmfd: shm_open");
	if (fstat(shmfd, &sb) < 0)
		err(-1, "test_fstat_shmfd: fstat");
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		(void)fstat(shmfd, &sb);
	}
	benchmark_stop();
	close(shmfd);
	return (i);
}

uintmax_t
test_fork(uintmax_t num, uintmax_t int_arg, const char *path)
{
	pid_t pid;
	uintmax_t i;

	pid = fork();
	if (pid < 0)
		err(-1, "test_fork: fork");
	if (pid == 0)
		_exit(0);
	if (waitpid(pid, NULL, 0) < 0)
		err(-1, "test_fork: waitpid");
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		pid = fork();
		if (pid < 0)
			err(-1, "test_fork: fork");
		if (pid == 0)
			_exit(0);
		if (waitpid(pid, NULL, 0) < 0)
			err(-1, "test_fork: waitpid");
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_vfork(uintmax_t num, uintmax_t int_arg, const char *path)
{
	pid_t pid;
	uintmax_t i;

	pid = vfork();
	if (pid < 0)
		err(-1, "test_vfork: vfork");
	if (pid == 0)
		_exit(0);
	if (waitpid(pid, NULL, 0) < 0)
		err(-1, "test_vfork: waitpid");
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		pid = vfork();
		if (pid < 0)
			err(-1, "test_vfork: vfork");
		if (pid == 0)
			_exit(0);
		if (waitpid(pid, NULL, 0) < 0)
			err(-1, "test_vfork: waitpid");
	}
	benchmark_stop();
	return (i);
}

#define	USR_BIN_TRUE	"/usr/bin/true"
static char *execve_args[] = { USR_BIN_TRUE, NULL};
extern char **environ;

uintmax_t
test_fork_exec(uintmax_t num, uintmax_t int_arg, const char *path)
{
	pid_t pid;
	uintmax_t i;

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
		if (alarm_fired)
			break;
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
	return (i);
}

uintmax_t
test_vfork_exec(uintmax_t num, uintmax_t int_arg, const char *path)
{
	pid_t pid;
	uintmax_t i;

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
		if (alarm_fired)
			break;
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
	return (i);
}

uintmax_t
test_chroot(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uintmax_t i;

	if (chroot("/") < 0)
		err(-1, "test_chroot: chroot");
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		if (chroot("/") < 0)
			err(-1, "test_chroot: chroot");
	}
	benchmark_stop();
	return (i);
}

uintmax_t
test_setuid(uintmax_t num, uintmax_t int_arg, const char *path)
{
	uid_t uid;
	uintmax_t i;

	uid = getuid();
	if (setuid(uid) < 0)
		err(-1, "test_setuid: setuid");
	benchmark_start();
	for (i = 0; i < num; i++) {
		if (alarm_fired)
			break;
		if (setuid(uid) < 0)
			err(-1, "test_setuid: setuid");
	}
	benchmark_stop();
	return (i);
}

struct test {
	const char	*t_name;
	uintmax_t	(*t_func)(uintmax_t, uintmax_t, const char *);
	int		 t_flags;
	uintmax_t	 t_int;
};

#define	FLAG_PATH	0x00000001

static const struct test tests[] = {
	{ "getuid", test_getuid },
	{ "getppid", test_getppid },
	{ "clock_gettime", test_clock_gettime },
	{ "gettimeofday", test_gettimeofday },
	{ "pipe", test_pipe },
	{ "socket_local_stream", test_socket_stream, .t_int = PF_LOCAL },
	{ "socket_local_dgram", test_socket_dgram, .t_int = PF_LOCAL },
	{ "socketpair_stream", test_socketpair_stream },
	{ "socketpair_dgram", test_socketpair_dgram },
	{ "socket_tcp", test_socket_stream, .t_int = PF_INET },
	{ "socket_udp", test_socket_dgram, .t_int = PF_INET },
	{ "create_unlink", test_create_unlink, .t_flags = FLAG_PATH },
	{ "open_close", test_open_close, .t_flags = FLAG_PATH },
	{ "open_read_close_1", test_open_read_close, .t_flags = FLAG_PATH,
	    .t_int = 1 },
	{ "open_read_close_10", test_open_read_close, .t_flags = FLAG_PATH,
	    .t_int = 10 },
	{ "open_read_close_100", test_open_read_close, .t_flags = FLAG_PATH,
	    .t_int = 100 },
	{ "open_read_close_1000", test_open_read_close, .t_flags = FLAG_PATH,
	    .t_int = 1000 },
	{ "open_read_close_10000", test_open_read_close,
	    .t_flags = FLAG_PATH, .t_int = 10000 },
	{ "open_read_close_100000", test_open_read_close,
	    .t_flags = FLAG_PATH, .t_int = 100000 },
	{ "open_read_close_1000000", test_open_read_close,
	    .t_flags = FLAG_PATH, .t_int = 1000000 },
	{ "read_1", test_read, .t_flags = FLAG_PATH, .t_int = 1 },
	{ "read_10", test_read, .t_flags = FLAG_PATH, .t_int = 10 },
	{ "read_100", test_read, .t_flags = FLAG_PATH, .t_int = 100 },
	{ "read_1000", test_read, .t_flags = FLAG_PATH, .t_int = 1000 },
	{ "read_10000", test_read, .t_flags = FLAG_PATH, .t_int = 10000 },
	{ "read_100000", test_read, .t_flags = FLAG_PATH, .t_int = 100000 },
	{ "read_1000000", test_read, .t_flags = FLAG_PATH, .t_int = 1000000 },
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

	fprintf(stderr, "syscall_timing [-i iterations] [-l loops] "
	    "[-p path] [-s seconds] test\n");
	for (i = 0; i < tests_count; i++)
		fprintf(stderr, "  %s\n", tests[i].t_name);
	exit(-1);
}

int
main(int argc, char *argv[])
{
	struct timespec ts_res;
	const struct test *the_test;
	const char *path;
	long long ll;
	char *endp;
	int ch, error, i, j, k;
	uintmax_t iterations, loops;

	alarm_timeout = 1;
	iterations = 0;
	loops = 10;
	path = NULL;
	while ((ch = getopt(argc, argv, "i:l:p:s:")) != -1) {
		switch (ch) {
		case 'i':
			ll = strtol(optarg, &endp, 10);
			if (*endp != 0 || ll < 1 || ll > 100000)
				usage();
			iterations = ll;
			break;

		case 'l':
			ll = strtol(optarg, &endp, 10);
			if (*endp != 0 || ll < 1 || ll > 100000)
				usage();
			loops = ll;
			break;

		case 'p':
			path = optarg;
			break;

		case 's':
			ll = strtol(optarg, &endp, 10);
			if (*endp != 0 || ll < 1 || ll > 60*60)
				usage();
			alarm_timeout = ll;
			break;

		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (iterations < 1 && alarm_timeout < 1)
		usage();
	if (iterations < 1)
		iterations = UINT64_MAX;
	if (loops < 1)
		loops = 1;

	if (argc < 1)
		usage();

	/*
	 * Validate test list and that, if a path is required, it is
	 * defined.
	 */
	for (j = 0; j < argc; j++) {
		the_test = NULL;
		for (i = 0; i < tests_count; i++) {
			if (strcmp(argv[j], tests[i].t_name) == 0)
				the_test = &tests[i];
		}
		if (the_test == NULL)
			usage();
		if ((the_test->t_flags & FLAG_PATH) && (path == NULL)) {
			errx(-1, "%s requires -p", the_test->t_name);
		}
	}

	error = clock_getres(CLOCK_REALTIME, &ts_res);
	assert(error == 0);
	printf("Clock resolution: %ju.%09ju\n", (uintmax_t)ts_res.tv_sec,
	    (uintmax_t)ts_res.tv_nsec);
	printf("test\tloop\ttime\titerations\tperiteration\n");

	for (j = 0; j < argc; j++) {
		uintmax_t calls, nsecsperit;

		the_test = NULL;
		for (i = 0; i < tests_count; i++) {
			if (strcmp(argv[j], tests[i].t_name) == 0)
				the_test = &tests[i];
		}

		/*
		 * Run one warmup, then do the real thing (loops) times.
		 */
		the_test->t_func(iterations, the_test->t_int, path);
		calls = 0;
		for (k = 0; k < loops; k++) {
			calls = the_test->t_func(iterations, the_test->t_int,
			    path);
			timespecsub(&ts_end, &ts_start);
			printf("%s\t%d\t", the_test->t_name, k);
			printf("%ju.%09ju\t%d\t", (uintmax_t)ts_end.tv_sec,
			    (uintmax_t)ts_end.tv_nsec, calls);

		/*
		 * Note.  This assumes that each iteration takes less than
		 * a second, and that our total nanoseconds doesn't exceed
		 * the room in our arithmetic unit.  Fine for system calls,
		 * but not for long things.
		 */
			nsecsperit = ts_end.tv_sec * 1000000000;
			nsecsperit += ts_end.tv_nsec;
			nsecsperit /= calls;
			printf("0.%09ju\n", (uintmax_t)nsecsperit);
		}
	}
	return (0);
}
