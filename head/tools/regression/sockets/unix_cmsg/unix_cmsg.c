/*-
 * Copyright (c) 2005 Andrey Simonenko
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

/*
 * There are tables with tests descriptions and pointers to test
 * functions.  Each t_*() function returns 0 if its test passed,
 * -1 if its test failed (something wrong was found in local domain
 * control messages), -2 if some system error occurred.  If test
 * function returns -2, then a program exits.
 *
 * Each test function completely control what to do (eg. fork or
 * do not fork a client process).  If a test function forks a client
 * process, then it waits for its termination.  If a return code of a
 * client process is not equal to zero, or if a client process was
 * terminated by a signal, then test function returns -2.
 *
 * Each test function and complete program are not optimized
 * a lot to allow easy to modify tests.
 *
 * Each function which can block, is run under TIMEOUT, if timeout
 * occurs, then test function returns -2 or a client process exits
 * with nonzero return code.
 */

#ifndef LISTENQ
# define LISTENQ	1
#endif

#ifndef TIMEOUT
# define TIMEOUT	60
#endif

#define EXTRA_CMSG_SPACE 512	/* Memory for not expected control data. */

static int	t_cmsgcred(void), t_sockcred_stream1(void);
static int	t_sockcred_stream2(void), t_cmsgcred_sockcred(void);
static int	t_sockcred_dgram(void), t_timestamp(void);

struct test_func {
	int	(*func)(void);	/* Pointer to function.	*/
	const char *desc;	/* Test description.	*/
};

static struct test_func test_stream_tbl[] = {
	{ NULL,			" 0: All tests" },
	{ t_cmsgcred,		" 1: Sending, receiving cmsgcred" },
	{ t_sockcred_stream1,	" 2: Receiving sockcred (listening socket has LOCAL_CREDS)" },
	{ t_sockcred_stream2,	" 3: Receiving sockcred (accepted socket has LOCAL_CREDS)" },
	{ t_cmsgcred_sockcred,	" 4: Sending cmsgcred, receiving sockcred" },
	{ t_timestamp,		" 5: Sending, receiving timestamp" },
	{ NULL, NULL }
};

static struct test_func test_dgram_tbl[] = {
	{ NULL,			" 0: All tests" },
	{ t_cmsgcred,		" 1: Sending, receiving cmsgcred" },
	{ t_sockcred_dgram,	" 2: Receiving sockcred" },
	{ t_cmsgcred_sockcred,	" 3: Sending cmsgcred, receiving sockcred" },
	{ t_timestamp,		" 4: Sending, receiving timestamp" },
	{ NULL, NULL }
};

#define TEST_STREAM_NO_MAX	(sizeof(test_stream_tbl) / sizeof(struct test_func) - 2)
#define TEST_DGRAM_NO_MAX	(sizeof(test_dgram_tbl) / sizeof(struct test_func) - 2)

static const char *myname = "SERVER";	/* "SERVER" or "CLIENT" */

static int	debug = 0;		/* 1, if -d. */
static int	no_control_data = 0;	/* 1, if -z. */

static u_int	nfailed = 0;		/* Number of failed tests. */

static int	sock_type;		/* SOCK_STREAM or SOCK_DGRAM */
static const char *sock_type_str;	/* "SOCK_STREAM" or "SOCK_DGRAN" */

static char	tempdir[] = "/tmp/unix_cmsg.XXXXXXX";
static char	serv_sock_path[PATH_MAX];

static char	ipc_message[] = "hello";

#define IPC_MESSAGE_SIZE	(sizeof(ipc_message))

static struct sockaddr_un servaddr;	/* Server address. */

static sigjmp_buf env_alrm;

static uid_t	my_uid;
static uid_t	my_euid;
static gid_t	my_gid;
static gid_t	my_egid;

/*
 * my_gids[0] is EGID, next items are supplementary GIDs,
 * my_ngids determines valid items in my_gids array.
 */
static gid_t	my_gids[NGROUPS_MAX];
static int	my_ngids;

static pid_t	client_pid;		/* PID of forked client. */

#define dbgmsg(x)	do {			\
	if (debug)				\
	       logmsgx x ;			\
} while (/* CONSTCOND */0)

static void	logmsg(const char *, ...) __printflike(1, 2);
static void	logmsgx(const char *, ...) __printflike(1, 2);
static void	output(const char *, ...) __printflike(1, 2);

extern char	*__progname;		/* The name of program. */

/*
 * Output the help message (-h switch).
 */
static void
usage(int quick)
{
	const struct test_func *test_func;

	fprintf(stderr, "Usage: %s [-dhz] [-t <socktype>] [testno]\n",
	    __progname);
	if (quick)
		return;
	fprintf(stderr, "\n Options are:\n\
  -d\t\t\tOutput debugging information\n\
  -h\t\t\tOutput this help message and exit\n\
  -t <socktype>\t\tRun test only for the given socket type:\n\
\t\t\tstream or dgram\n\
  -z\t\t\tDo not send real control data if possible\n\n");
	fprintf(stderr, " Available tests for stream sockets:\n");
	for (test_func = test_stream_tbl; test_func->desc != NULL; ++test_func)
		fprintf(stderr, "  %s\n", test_func->desc);
	fprintf(stderr, "\n Available tests for datagram sockets:\n");
	for (test_func = test_dgram_tbl; test_func->desc != NULL; ++test_func)
		fprintf(stderr, "  %s\n", test_func->desc);
}

/*
 * printf-like function for outputting to STDOUT_FILENO.
 */
static void
output(const char *format, ...)
{
	char buf[128];
	va_list ap;

	va_start(ap, format);
	if (vsnprintf(buf, sizeof(buf), format, ap) < 0)
		err(EX_SOFTWARE, "output: vsnprintf failed");
	write(STDOUT_FILENO, buf, strlen(buf));
	va_end(ap);
}

/*
 * printf-like function for logging, also outputs message for errno.
 */
static void
logmsg(const char *format, ...)
{
	char buf[128];
	va_list ap;
	int errno_save;

	errno_save = errno;		/* Save errno. */

	va_start(ap, format);
	if (vsnprintf(buf, sizeof(buf), format, ap) < 0)
		err(EX_SOFTWARE, "logmsg: vsnprintf failed");
	if (errno_save == 0)
		output("%s: %s\n", myname, buf);
	else
		output("%s: %s: %s\n", myname, buf, strerror(errno_save));
	va_end(ap);

	errno = errno_save;		/* Restore errno. */
}

/*
 * printf-like function for logging, do not output message for errno.
 */
static void
logmsgx(const char *format, ...)
{
	char buf[128];
	va_list ap;

	va_start(ap, format);
	if (vsnprintf(buf, sizeof(buf), format, ap) < 0)
		err(EX_SOFTWARE, "logmsgx: vsnprintf failed");
	output("%s: %s\n", myname, buf);
	va_end(ap);
}

/*
 * Run tests from testno1 to testno2.
 */
static int
run_tests(u_int testno1, u_int testno2)
{
	const struct test_func *test_func;
	u_int i, nfailed1;

	output("Running tests for %s sockets:\n", sock_type_str);
	test_func = (sock_type == SOCK_STREAM ?
	    test_stream_tbl : test_dgram_tbl) + testno1;

	nfailed1 = 0;
	for (i = testno1; i <= testno2; ++test_func, ++i) {
		output(" %s\n", test_func->desc);
		switch (test_func->func()) {
		case -1:
			++nfailed1;
			break;
		case -2:
			logmsgx("some system error occurred, exiting");
			return (-1);
		}
	}

	nfailed += nfailed1;

	if (testno1 != testno2) {
		if (nfailed1 == 0)
			output("-- all tests were passed!\n");
		else
			output("-- %u test%s failed!\n", nfailed1,
			    nfailed1 == 1 ? "" : "s");
	} else {
		if (nfailed == 0)
			output("-- test was passed!\n");
		else
			output("-- test failed!\n");
	}

	return (0);
}

/* ARGSUSED */
static void
sig_alrm(int signo __unused)
{
	siglongjmp(env_alrm, 1);
}

/*
 * Initialize signals handlers.
 */
static void
sig_init(void)
{
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, (struct sigaction *)NULL) < 0) 
		err(EX_OSERR, "sigaction(SIGPIPE)");

	sa.sa_handler = sig_alrm;
	if (sigaction(SIGALRM, &sa, (struct sigaction *)NULL) < 0)
		err(EX_OSERR, "sigaction(SIGALRM)");
}

int
main(int argc, char *argv[])
{
	const char *errstr;
	int opt, dgramflag, streamflag;
	u_int testno1, testno2;

	dgramflag = streamflag = 0;
	while ((opt = getopt(argc, argv, "dht:z")) != -1)
		switch (opt) {
		case 'd':
			debug = 1;
			break;
		case 'h':
			usage(0);
			return (EX_OK);
		case 't':
			if (strcmp(optarg, "stream") == 0)
				streamflag = 1;
			else if (strcmp(optarg, "dgram") == 0)
				dgramflag = 1;
			else
				errx(EX_USAGE, "wrong socket type in -t option");
			break;
		case 'z':
			no_control_data = 1;
			break;
		case '?':
		default:
			usage(1);
			return (EX_USAGE);
		}

	if (optind < argc) {
		if (optind + 1 != argc)
			errx(EX_USAGE, "too many arguments");
		testno1 = strtonum(argv[optind], 0, UINT_MAX, &errstr);
		if (errstr != NULL)
			errx(EX_USAGE, "wrong test number: %s", errstr);
	} else
		testno1 = 0;

	if (dgramflag == 0 && streamflag == 0)
		dgramflag = streamflag = 1;

	if (dgramflag && streamflag && testno1 != 0)
		errx(EX_USAGE, "you can use particular test, only with datagram or stream sockets");

	if (streamflag) {
		if (testno1 > TEST_STREAM_NO_MAX)
			errx(EX_USAGE, "given test %u for stream sockets does not exist",
			    testno1);
	} else {
		if (testno1 > TEST_DGRAM_NO_MAX)
			errx(EX_USAGE, "given test %u for datagram sockets does not exist",
			    testno1);
	}

	my_uid = getuid();
	my_euid = geteuid();
	my_gid = getgid();
	my_egid = getegid();
	switch (my_ngids = getgroups(sizeof(my_gids) / sizeof(my_gids[0]), my_gids)) {
	case -1:
		err(EX_SOFTWARE, "getgroups");
		/* NOTREACHED */
	case 0:
		errx(EX_OSERR, "getgroups returned 0 groups");
	}

	sig_init();

	if (mkdtemp(tempdir) == NULL)
		err(EX_OSERR, "mkdtemp");

	if (streamflag) {
		sock_type = SOCK_STREAM;
		sock_type_str = "SOCK_STREAM";
		if (testno1 == 0) {
			testno1 = 1;
			testno2 = TEST_STREAM_NO_MAX;
		} else
			testno2 = testno1;
		if (run_tests(testno1, testno2) < 0)
			goto failed;
		testno1 = 0;
	}

	if (dgramflag) {
		sock_type = SOCK_DGRAM;
		sock_type_str = "SOCK_DGRAM";
		if (testno1 == 0) {
			testno1 = 1;
			testno2 = TEST_DGRAM_NO_MAX;
		} else
			testno2 = testno1;
		if (run_tests(testno1, testno2) < 0)
			goto failed;
	}

	if (rmdir(tempdir) < 0) {
		logmsg("rmdir(%s)", tempdir);
		return (EX_OSERR);
	}

	return (nfailed ? EX_OSERR : EX_OK);

failed:
	if (rmdir(tempdir) < 0)
		logmsg("rmdir(%s)", tempdir);
	return (EX_OSERR);
}

/*
 * Create PF_LOCAL socket, if sock_path is not equal to NULL, then
 * bind() it.  Return socket address in addr.  Return file descriptor
 * or -1 if some error occurred.
 */
static int
create_socket(char *sock_path, size_t sock_path_len, struct sockaddr_un *addr)
{
	int rv, fd;

	if ((fd = socket(PF_LOCAL, sock_type, 0)) < 0) {
		logmsg("create_socket: socket(PF_LOCAL, %s, 0)", sock_type_str);
		return (-1);
	}

	if (sock_path != NULL) {
		if ((rv = snprintf(sock_path, sock_path_len, "%s/%s",
		    tempdir, myname)) < 0) {
			logmsg("create_socket: snprintf failed");
			goto failed;
		}
		if ((size_t)rv >= sock_path_len) {
			logmsgx("create_socket: too long path name for given buffer");
			goto failed;
		}

		memset(addr, 0, sizeof(addr));
		addr->sun_family = AF_LOCAL;
		if (strlen(sock_path) >= sizeof(addr->sun_path)) {
			logmsgx("create_socket: too long path name (>= %lu) for local domain socket",
			    (u_long)sizeof(addr->sun_path));
			goto failed;
		}
		strcpy(addr->sun_path, sock_path);

		if (bind(fd, (struct sockaddr *)addr, SUN_LEN(addr)) < 0) {
			logmsg("create_socket: bind(%s)", sock_path);
			goto failed;
		}
	}

	return (fd);

failed:
	if (close(fd) < 0)
		logmsg("create_socket: close");
	return (-1);
}

/*
 * Call create_socket() for server listening socket.
 * Return socket descriptor or -1 if some error occurred.
 */
static int
create_server_socket(void)
{
	return (create_socket(serv_sock_path, sizeof(serv_sock_path), &servaddr));
}

/*
 * Create unbound socket.
 */
static int
create_unbound_socket(void)
{
	return (create_socket((char *)NULL, 0, (struct sockaddr_un *)NULL));
}

/*
 * Close socket descriptor, if sock_path is not equal to NULL,
 * then unlink the given path.
 */
static int
close_socket(const char *sock_path, int fd)
{
	int error = 0;

	if (close(fd) < 0) {
		logmsg("close_socket: close");
		error = -1;
	}
	if (sock_path != NULL)
		if (unlink(sock_path) < 0) {
			logmsg("close_socket: unlink(%s)", sock_path);
			error = -1;
		}
	return (error);
}

/*
 * Connect to server (socket address in servaddr).
 */
static int
connect_server(int fd)
{
	dbgmsg(("connecting to %s", serv_sock_path));

	/*
	 * If PF_LOCAL listening socket's queue is full, then connect()
	 * returns ECONNREFUSED immediately, do not need timeout.
	 */
	if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		logmsg("connect_server: connect(%s)", serv_sock_path);
		return (-1);
	}

	return (0);
}

/*
 * sendmsg() with timeout.
 */
static int
sendmsg_timeout(int fd, struct msghdr *msg, size_t n)
{
	ssize_t nsent;

	dbgmsg(("sending %lu bytes", (u_long)n));

	if (sigsetjmp(env_alrm, 1) != 0) {
		logmsgx("sendmsg_timeout: cannot send message to %s (timeout)", serv_sock_path);
		return (-1);
	}

	(void)alarm(TIMEOUT);

	nsent = sendmsg(fd, msg, 0);

	(void)alarm(0);

	if (nsent < 0) {
		logmsg("sendmsg_timeout: sendmsg");
		return (-1);
	}

	if ((size_t)nsent != n) {
		logmsgx("sendmsg_timeout: sendmsg: short send: %ld of %lu bytes",
		    (long)nsent, (u_long)n);
		return (-1);
	}

	return (0);
}

/*
 * accept() with timeout.
 */
static int
accept_timeout(int listenfd)
{
	int fd;

	dbgmsg(("accepting connection"));

	if (sigsetjmp(env_alrm, 1) != 0) {
		logmsgx("accept_timeout: cannot accept connection (timeout)");
		return (-1);
	}

	(void)alarm(TIMEOUT);

	fd = accept(listenfd, (struct sockaddr *)NULL, (socklen_t *)NULL);

	(void)alarm(0);

	if (fd < 0) {
		logmsg("accept_timeout: accept");
		return (-1);
	}

	return (fd);
}

/*
 * recvmsg() with timeout.
 */
static int
recvmsg_timeout(int fd, struct msghdr *msg, size_t n)
{
	ssize_t nread;

	dbgmsg(("receiving %lu bytes", (u_long)n));

	if (sigsetjmp(env_alrm, 1) != 0) {
		logmsgx("recvmsg_timeout: cannot receive message (timeout)");
		return (-1);
	}

	(void)alarm(TIMEOUT);

	nread = recvmsg(fd, msg, MSG_WAITALL);

	(void)alarm(0);

	if (nread < 0) {
		logmsg("recvmsg_timeout: recvmsg");
		return (-1);
	}

	if ((size_t)nread != n) {
		logmsgx("recvmsg_timeout: recvmsg: short read: %ld of %lu bytes",
		    (long)nread, (u_long)n);
		return (-1);
	}

	return (0);
}

/*
 * Wait for synchronization message (1 byte) with timeout.
 */
static int
sync_recv(int fd)
{
	ssize_t nread;
	char buf;

	dbgmsg(("waiting for sync message"));

	if (sigsetjmp(env_alrm, 1) != 0) {
		logmsgx("sync_recv: cannot receive sync message (timeout)");
		return (-1);
	}

	(void)alarm(TIMEOUT);

	nread = read(fd, &buf, 1);

	(void)alarm(0);

	if (nread < 0) {
		logmsg("sync_recv: read");
		return (-1);
	}

	if (nread != 1) {
		logmsgx("sync_recv: read: short read: %ld of 1 byte",
		    (long)nread);
		return (-1);
	}

	return (0);
}

/*
 * Send synchronization message (1 byte) with timeout.
 */
static int
sync_send(int fd)
{
	ssize_t nsent;

	dbgmsg(("sending sync message"));

	if (sigsetjmp(env_alrm, 1) != 0) {
		logmsgx("sync_send: cannot send sync message (timeout)");
		return (-1);
	}

	(void)alarm(TIMEOUT);

	nsent = write(fd, "", 1);

	(void)alarm(0);

	if (nsent < 0) {
		logmsg("sync_send: write");
		return (-1);
	}

	if (nsent != 1) {
		logmsgx("sync_send: write: short write: %ld of 1 byte",
		    (long)nsent);
		return (-1);
	}

	return (0);
}

/*
 * waitpid() for client with timeout.
 */
static int
wait_client(void)
{
	int status;
	pid_t pid;

	if (sigsetjmp(env_alrm, 1) != 0) {
		logmsgx("wait_client: cannot get exit status of client PID %ld (timeout)",
		    (long)client_pid);
		return (-1);
	}

	(void)alarm(TIMEOUT);

	pid = waitpid(client_pid, &status, 0);

	(void)alarm(0);

	if (pid == (pid_t)-1) {
		logmsg("wait_client: waitpid");
		return (-1);
	}

	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) != 0) {
			logmsgx("wait_client: exit status of client PID %ld is %d",
			    (long)client_pid, WEXITSTATUS(status));
			return (-1);
		}
	} else {
		if (WIFSIGNALED(status))
			logmsgx("wait_client: abnormal termination of client PID %ld, signal %d%s",
			    (long)client_pid, WTERMSIG(status), WCOREDUMP(status) ? " (core file generated)" : "");
		else
			logmsgx("wait_client: termination of client PID %ld, unknown status",
			    (long)client_pid);
		return (-1);
	}

	return (0);
}

/*
 * Check if n supplementary GIDs in gids are correct.  (my_gids + 1)
 * has (my_ngids - 1) supplementary GIDs of current process.
 */
static int
check_groups(const gid_t *gids, int n)
{
	char match[NGROUPS_MAX] = { 0 };
	int error, i, j;

	if (n != my_ngids - 1) {
		logmsgx("wrong number of groups %d != %d (returned from getgroups() - 1)",
		    n, my_ngids - 1);
		error = -1;
	} else
		error = 0;
	for (i = 0; i < n; ++i) {
		for (j = 1; j < my_ngids; ++j) {
			if (gids[i] == my_gids[j]) {
				if (match[j]) {
					logmsgx("duplicated GID %lu",
					    (u_long)gids[i]);
					error = -1;
				} else
					match[j] = 1;
				break;
			}
		}
		if (j == my_ngids) {
			logmsgx("unexpected GID %lu", (u_long)gids[i]);
			error = -1;
		}
	}
	for (j = 1; j < my_ngids; ++j)
		if (match[j] == 0) {
			logmsgx("did not receive supplementary GID %u", my_gids[j]);
			error = -1;
		}
	return (error);
}

/*
 * Send n messages with data and control message with SCM_CREDS type
 * to server and exit.
 */
static void
t_cmsgcred_client(u_int n)
{
	union {
		struct cmsghdr	cm;
		char	control[CMSG_SPACE(sizeof(struct cmsgcred))];
	} control_un;
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr *cmptr;
	int fd;
	u_int i;

	assert(n == 1 || n == 2);

	if ((fd = create_unbound_socket()) < 0)
		goto failed;

	if (connect_server(fd) < 0)
		goto failed_close;

	iov[0].iov_base = ipc_message;
	iov[0].iov_len = IPC_MESSAGE_SIZE;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control_un.control;
	msg.msg_controllen = no_control_data ?
	    sizeof(struct cmsghdr) : sizeof(control_un.control);
	msg.msg_flags = 0;

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(no_control_data ?
	    0 : sizeof(struct cmsgcred));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_CREDS;

	for (i = 0; i < n; ++i) {
		dbgmsg(("#%u msg_controllen = %u, cmsg_len = %u", i,
		    (u_int)msg.msg_controllen, (u_int)cmptr->cmsg_len));
		if (sendmsg_timeout(fd, &msg, IPC_MESSAGE_SIZE) < 0)
			goto failed_close;
	}

	if (close_socket((const char *)NULL, fd) < 0)
		goto failed;

	_exit(0);

failed_close:
	(void)close_socket((const char *)NULL, fd);

failed:
	_exit(1);
}

/*
 * Receive two messages with data and control message with SCM_CREDS
 * type followed by struct cmsgcred{} from client.  fd1 is a listen
 * socket for stream sockets or simply socket for datagram sockets.
 */
static int
t_cmsgcred_server(int fd1)
{
	char buf[IPC_MESSAGE_SIZE];
	union {
		struct cmsghdr	cm;
		char	control[CMSG_SPACE(sizeof(struct cmsgcred)) + EXTRA_CMSG_SPACE];
	} control_un;
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr *cmptr;
	const struct cmsgcred *cmcredptr;
	socklen_t controllen;
	int error, error2, fd2;
	u_int i;

	if (sock_type == SOCK_STREAM) {
		if ((fd2 = accept_timeout(fd1)) < 0)
			return (-2);
	} else
		fd2 = fd1;

	error = 0;

	controllen = sizeof(control_un.control);

	for (i = 0; i < 2; ++i) {
		iov[0].iov_base = buf;
		iov[0].iov_len = sizeof(buf);

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = 1;
		msg.msg_control = control_un.control;
		msg.msg_controllen = controllen;
		msg.msg_flags = 0;

		controllen = CMSG_SPACE(sizeof(struct cmsgcred));

		if (recvmsg_timeout(fd2, &msg, sizeof(buf)) < 0)
			goto failed;

		if (msg.msg_flags & MSG_CTRUNC) {
			logmsgx("#%u control data was truncated, MSG_CTRUNC flag is on",
			    i);
			goto next_error;
		}

		if (msg.msg_controllen < sizeof(struct cmsghdr)) {
			logmsgx("#%u msg_controllen %u < %lu (sizeof(struct cmsghdr))",
			    i, (u_int)msg.msg_controllen, (u_long)sizeof(struct cmsghdr));
			goto next_error;
		}

		if ((cmptr = CMSG_FIRSTHDR(&msg)) == NULL) {
			logmsgx("CMSG_FIRSTHDR is NULL");
			goto next_error;
		}

		dbgmsg(("#%u msg_controllen = %u, cmsg_len = %u", i,
		    (u_int)msg.msg_controllen, (u_int)cmptr->cmsg_len));

		if (cmptr->cmsg_level != SOL_SOCKET) {
			logmsgx("#%u cmsg_level %d != SOL_SOCKET", i,
			    cmptr->cmsg_level);
			goto next_error;
		}

		if (cmptr->cmsg_type != SCM_CREDS) {
			logmsgx("#%u cmsg_type %d != SCM_CREDS", i,
			    cmptr->cmsg_type);
			goto next_error;
		}

		if (cmptr->cmsg_len != CMSG_LEN(sizeof(struct cmsgcred))) {
			logmsgx("#%u cmsg_len %u != %lu (CMSG_LEN(sizeof(struct cmsgcred))",
			    i, (u_int)cmptr->cmsg_len, (u_long)CMSG_LEN(sizeof(struct cmsgcred)));
			goto next_error;
		}

		cmcredptr = (const struct cmsgcred *)CMSG_DATA(cmptr);

		error2 = 0;
		if (cmcredptr->cmcred_pid != client_pid) {
			logmsgx("#%u cmcred_pid %ld != %ld (PID of client)",
			    i, (long)cmcredptr->cmcred_pid, (long)client_pid);
			error2 = 1;
		}
		if (cmcredptr->cmcred_uid != my_uid) {
			logmsgx("#%u cmcred_uid %lu != %lu (UID of current process)",
			    i, (u_long)cmcredptr->cmcred_uid, (u_long)my_uid);
			error2 = 1;
		}
		if (cmcredptr->cmcred_euid != my_euid) {
			logmsgx("#%u cmcred_euid %lu != %lu (EUID of current process)",
			    i, (u_long)cmcredptr->cmcred_euid, (u_long)my_euid);
			error2 = 1;
		}
		if (cmcredptr->cmcred_gid != my_gid) {
			logmsgx("#%u cmcred_gid %lu != %lu (GID of current process)",
			    i, (u_long)cmcredptr->cmcred_gid, (u_long)my_gid);
			error2 = 1;
		}
		if (cmcredptr->cmcred_ngroups == 0) {
			logmsgx("#%u cmcred_ngroups = 0, this is wrong", i);
			error2 = 1;
		} else {
			if (cmcredptr->cmcred_ngroups > NGROUPS_MAX) {
				logmsgx("#%u cmcred_ngroups %d > %u (NGROUPS_MAX)",
				    i, cmcredptr->cmcred_ngroups, NGROUPS_MAX);
				error2 = 1;
			} else if (cmcredptr->cmcred_ngroups < 0) {
				logmsgx("#%u cmcred_ngroups %d < 0",
				    i, cmcredptr->cmcred_ngroups);
				error2 = 1;
			} else {
				dbgmsg(("#%u cmcred_ngroups = %d", i,
				    cmcredptr->cmcred_ngroups));
				if (cmcredptr->cmcred_groups[0] != my_egid) {
					logmsgx("#%u cmcred_groups[0] %lu != %lu (EGID of current process)",
					    i, (u_long)cmcredptr->cmcred_groups[0], (u_long)my_egid);
					error2 = 1;
				}
				if (check_groups(cmcredptr->cmcred_groups + 1, cmcredptr->cmcred_ngroups - 1) < 0) {
					logmsgx("#%u cmcred_groups has wrong GIDs", i);
					error2 = 1;
				}
			}
		}

		if (error2)
			goto next_error;

		if ((cmptr = CMSG_NXTHDR(&msg, cmptr)) != NULL) {
			logmsgx("#%u control data has extra header", i);
			goto next_error;
		}

		continue;
next_error:
		error = -1;
	}

	if (sock_type == SOCK_STREAM)
		if (close(fd2) < 0) {
			logmsg("close");
			return (-2);
		}
	return (error);

failed:
	if (sock_type == SOCK_STREAM)
		if (close(fd2) < 0)
			logmsg("close");
	return (-2);
}

static int
t_cmsgcred(void)
{
	int error, fd;

	if ((fd = create_server_socket()) < 0)
		return (-2);

	if (sock_type == SOCK_STREAM)
		if (listen(fd, LISTENQ) < 0) {
			logmsg("listen");
			goto failed;
		}

	if ((client_pid = fork()) == (pid_t)-1) {
		logmsg("fork");
		goto failed;
	}

	if (client_pid == 0) {
		myname = "CLIENT";
		if (close_socket((const char *)NULL, fd) < 0)
			_exit(1);
		t_cmsgcred_client(2);
	}

	if ((error = t_cmsgcred_server(fd)) == -2) {
		(void)wait_client();
		goto failed;
	}

	if (wait_client() < 0)
		goto failed;

	if (close_socket(serv_sock_path, fd) < 0) {
		logmsgx("close_socket failed");
		return (-2);
	}
	return (error);

failed:
	if (close_socket(serv_sock_path, fd) < 0)
		logmsgx("close_socket failed");
	return (-2);
}

/*
 * Send two messages with data to server and exit.
 */
static void
t_sockcred_client(int type)
{
	struct msghdr msg;
	struct iovec iov[1];
	int fd;
	u_int i;

	assert(type == 0 || type == 1);

	if ((fd = create_unbound_socket()) < 0)
		goto failed;

	if (connect_server(fd) < 0)
		goto failed_close;

	if (type == 1)
		if (sync_recv(fd) < 0)
			goto failed_close;

	iov[0].iov_base = ipc_message;
	iov[0].iov_len = IPC_MESSAGE_SIZE;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	for (i = 0; i < 2; ++i)
		if (sendmsg_timeout(fd, &msg, IPC_MESSAGE_SIZE) < 0)
			goto failed_close;

	if (close_socket((const char *)NULL, fd) < 0)
		goto failed;

	_exit(0);

failed_close:
	(void)close_socket((const char *)NULL, fd);

failed:
	_exit(1);
}

/*
 * Receive one message with data and control message with SCM_CREDS
 * type followed by struct sockcred{} and if n is not equal 1, then
 * receive another one message with data.  fd1 is a listen socket for
 * stream sockets or simply socket for datagram sockets.  If type is
 * 1, then set LOCAL_CREDS option for accepted stream socket.
 */
static int
t_sockcred_server(int type, int fd1, u_int n)
{
	char buf[IPC_MESSAGE_SIZE];
	union {
		struct cmsghdr	cm;
		char	control[CMSG_SPACE(SOCKCREDSIZE(NGROUPS_MAX)) + EXTRA_CMSG_SPACE];
	} control_un;
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr *cmptr;
	const struct sockcred *sockcred;
	int error, error2, fd2, optval;
	u_int i;

	assert(n == 1 || n == 2);
	assert(type == 0 || type == 1);

	if (sock_type == SOCK_STREAM) {
		if ((fd2 = accept_timeout(fd1)) < 0)
			return (-2);
		if (type == 1) {
			optval = 1;
			if (setsockopt(fd2, 0, LOCAL_CREDS, &optval, sizeof optval) < 0) {
				logmsg("setsockopt(LOCAL_CREDS) for accepted socket");
				if (errno == ENOPROTOOPT) {
					error = -1;
					goto done_close;
				}
				goto failed;
			}
			if (sync_send(fd2) < 0)
				goto failed;
		}
	} else
		fd2 = fd1;

	error = 0;

	for (i = 0; i < n; ++i) {
		iov[0].iov_base = buf;
		iov[0].iov_len = sizeof buf;

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = 1;
		msg.msg_control = control_un.control;
		msg.msg_controllen = sizeof control_un.control;
		msg.msg_flags = 0;

		if (recvmsg_timeout(fd2, &msg, sizeof buf) < 0)
			goto failed;

		if (msg.msg_flags & MSG_CTRUNC) {
			logmsgx("control data was truncated, MSG_CTRUNC flag is on");
			goto next_error;
		}

		if (i != 0 && sock_type == SOCK_STREAM) {
			if (msg.msg_controllen != 0) {
				logmsgx("second message has control data, this is wrong for stream sockets");
				goto next_error;
			}
			dbgmsg(("#%u msg_controllen = %u", i,
			    (u_int)msg.msg_controllen));
			continue;
		}

		if (msg.msg_controllen < sizeof(struct cmsghdr)) {
			logmsgx("#%u msg_controllen %u < %lu (sizeof(struct cmsghdr))",
			    i, (u_int)msg.msg_controllen, (u_long)sizeof(struct cmsghdr));
			goto next_error;
		}

		if ((cmptr = CMSG_FIRSTHDR(&msg)) == NULL) {
			logmsgx("CMSG_FIRSTHDR is NULL");
			goto next_error;
		}

		dbgmsg(("#%u msg_controllen = %u, cmsg_len = %u", i,
		    (u_int)msg.msg_controllen, (u_int)cmptr->cmsg_len));

		if (cmptr->cmsg_level != SOL_SOCKET) {
			logmsgx("#%u cmsg_level %d != SOL_SOCKET", i,
			    cmptr->cmsg_level);
			goto next_error;
		}

		if (cmptr->cmsg_type != SCM_CREDS) {
			logmsgx("#%u cmsg_type %d != SCM_CREDS", i,
			    cmptr->cmsg_type);
			goto next_error;
		}

		if (cmptr->cmsg_len < CMSG_LEN(SOCKCREDSIZE(1))) {
			logmsgx("#%u cmsg_len %u != %lu (CMSG_LEN(SOCKCREDSIZE(1)))",
			    i, (u_int)cmptr->cmsg_len, (u_long)CMSG_LEN(SOCKCREDSIZE(1)));
			goto next_error;
		}

		sockcred = (const struct sockcred *)CMSG_DATA(cmptr);

		error2 = 0;
		if (sockcred->sc_uid != my_uid) {
			logmsgx("#%u sc_uid %lu != %lu (UID of current process)",
			    i, (u_long)sockcred->sc_uid, (u_long)my_uid);
			error2 = 1;
		}
		if (sockcred->sc_euid != my_euid) {
			logmsgx("#%u sc_euid %lu != %lu (EUID of current process)",
			    i, (u_long)sockcred->sc_euid, (u_long)my_euid);
			error2 = 1;
		}
		if (sockcred->sc_gid != my_gid) {
			logmsgx("#%u sc_gid %lu != %lu (GID of current process)",
			    i, (u_long)sockcred->sc_gid, (u_long)my_gid);
			error2 = 1;
		}
		if (sockcred->sc_egid != my_egid) {
			logmsgx("#%u sc_egid %lu != %lu (EGID of current process)",
			    i, (u_long)sockcred->sc_gid, (u_long)my_egid);
			error2 = 1;
		}
		if (sockcred->sc_ngroups > NGROUPS_MAX) {
			logmsgx("#%u sc_ngroups %d > %u (NGROUPS_MAX)",
			    i, sockcred->sc_ngroups, NGROUPS_MAX);
			error2 = 1;
		} else if (sockcred->sc_ngroups < 0) {
			logmsgx("#%u sc_ngroups %d < 0",
			    i, sockcred->sc_ngroups);
			error2 = 1;
		} else {
			dbgmsg(("#%u sc_ngroups = %d", i, sockcred->sc_ngroups));
			if (check_groups(sockcred->sc_groups, sockcred->sc_ngroups) < 0) {
				logmsgx("#%u sc_groups has wrong GIDs", i);
				error2 = 1;
			}
		}

		if (error2)
			goto next_error;

		if ((cmptr = CMSG_NXTHDR(&msg, cmptr)) != NULL) {
			logmsgx("#%u control data has extra header, this is wrong",
			    i);
			goto next_error;
		}

		continue;
next_error:
		error = -1;
	}

done_close:
	if (sock_type == SOCK_STREAM)
		if (close(fd2) < 0) {
			logmsg("close");
			return (-2);
		}
	return (error);

failed:
	if (sock_type == SOCK_STREAM)
		if (close(fd2) < 0)
			logmsg("close");
	return (-2);
}

static int
t_sockcred(int type)
{
	int error, fd, optval;

	assert(type == 0 || type == 1);

	if ((fd = create_server_socket()) < 0)
		return (-2);

	if (sock_type == SOCK_STREAM)
		if (listen(fd, LISTENQ) < 0) {
			logmsg("listen");
			goto failed;
		}

	if (type == 0) {
		optval = 1;
		if (setsockopt(fd, 0, LOCAL_CREDS, &optval, sizeof optval) < 0) {
			logmsg("setsockopt(LOCAL_CREDS) for %s socket",
			    sock_type == SOCK_STREAM ? "stream listening" : "datagram");
			if (errno == ENOPROTOOPT) {
				error = -1;
				goto done_close;
			}
			goto failed;
		}
	}

	if ((client_pid = fork()) == (pid_t)-1) {
		logmsg("fork");
		goto failed;
	}

	if (client_pid == 0) {
		myname = "CLIENT";
		if (close_socket((const char *)NULL, fd) < 0)
			_exit(1);
		t_sockcred_client(type);
	}

	if ((error = t_sockcred_server(type, fd, 2)) == -2) {
		(void)wait_client();
		goto failed;
	}

	if (wait_client() < 0)
		goto failed;

done_close:
	if (close_socket(serv_sock_path, fd) < 0) {
		logmsgx("close_socket failed");
		return (-2);
	}
	return (error);

failed:
	if (close_socket(serv_sock_path, fd) < 0)
		logmsgx("close_socket failed");
	return (-2);
}

static int
t_sockcred_stream1(void)
{
	return (t_sockcred(0));
}

static int
t_sockcred_stream2(void)
{
	return (t_sockcred(1));
}

static int
t_sockcred_dgram(void)
{
	return (t_sockcred(0));
}

static int
t_cmsgcred_sockcred(void)
{
	int error, fd, optval;

	if ((fd = create_server_socket()) < 0)
		return (-2);

	if (sock_type == SOCK_STREAM)
		if (listen(fd, LISTENQ) < 0) {
			logmsg("listen");
			goto failed;
		}

	optval = 1;
	if (setsockopt(fd, 0, LOCAL_CREDS, &optval, sizeof optval) < 0) {
		logmsg("setsockopt(LOCAL_CREDS) for %s socket",
		    sock_type == SOCK_STREAM ? "stream listening" : "datagram");
		if (errno == ENOPROTOOPT) {
			error = -1;
			goto done_close;
		}
		goto failed;
	}

	if ((client_pid = fork()) == (pid_t)-1) {
		logmsg("fork");
		goto failed;
	}

	if (client_pid == 0) {
		myname = "CLIENT";
		if (close_socket((const char *)NULL, fd) < 0)
			_exit(1);
		t_cmsgcred_client(1);
	}

	if ((error = t_sockcred_server(0, fd, 1)) == -2) {
		(void)wait_client();
		goto failed;
	}

	if (wait_client() < 0)
		goto failed;

done_close:
	if (close_socket(serv_sock_path, fd) < 0) {
		logmsgx("close_socket failed");
		return (-2);
	}
	return (error);

failed:
	if (close_socket(serv_sock_path, fd) < 0)
		logmsgx("close_socket failed");
	return (-2);
}

/*
 * Send one message with data and control message with SCM_TIMESTAMP
 * type to server and exit.
 */
static void
t_timestamp_client(void)
{
	union {
		struct cmsghdr	cm;
		char	control[CMSG_SPACE(sizeof(struct timeval))];
	} control_un;
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr *cmptr;
	int fd;

	if ((fd = create_unbound_socket()) < 0)
		goto failed;

	if (connect_server(fd) < 0)
		goto failed_close;

	iov[0].iov_base = ipc_message;
	iov[0].iov_len = IPC_MESSAGE_SIZE;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control_un.control;
	msg.msg_controllen = no_control_data ?
	    sizeof(struct cmsghdr) :sizeof control_un.control;
	msg.msg_flags = 0;

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(no_control_data ?
	    0 : sizeof(struct timeval));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_TIMESTAMP;

	dbgmsg(("msg_controllen = %u, cmsg_len = %u",
	    (u_int)msg.msg_controllen, (u_int)cmptr->cmsg_len));

	if (sendmsg_timeout(fd, &msg, IPC_MESSAGE_SIZE) < 0)
		goto failed_close;

	if (close_socket((const char *)NULL, fd) < 0)
		goto failed;

	_exit(0);

failed_close:
	(void)close_socket((const char *)NULL, fd);

failed:
	_exit(1);
}

/*
 * Receive one message with data and control message with SCM_TIMESTAMP
 * type followed by struct timeval{} from client.
 */
static int
t_timestamp_server(int fd1)
{
	union {
		struct cmsghdr	cm;
		char	control[CMSG_SPACE(sizeof(struct timeval)) + EXTRA_CMSG_SPACE];
	} control_un;
	char buf[IPC_MESSAGE_SIZE];
	int error, fd2;
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr *cmptr;
	const struct timeval *timeval;

	if (sock_type == SOCK_STREAM) {
		if ((fd2 = accept_timeout(fd1)) < 0)
			return (-2);
	} else
		fd2 = fd1;

	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof buf;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof control_un.control;;
	msg.msg_flags = 0;

	if (recvmsg_timeout(fd2, &msg, sizeof buf) < 0)
		goto failed;

	error = -1;

	if (msg.msg_flags & MSG_CTRUNC) {
		logmsgx("control data was truncated, MSG_CTRUNC flag is on");
		goto done;
	}

	if (msg.msg_controllen < sizeof(struct cmsghdr)) {
		logmsgx("msg_controllen %u < %lu (sizeof(struct cmsghdr))",
		    (u_int)msg.msg_controllen, (u_long)sizeof(struct cmsghdr));
		goto done;
	}

	if ((cmptr = CMSG_FIRSTHDR(&msg)) == NULL) {
		logmsgx("CMSG_FIRSTHDR is NULL");
		goto done;
	}

	dbgmsg(("msg_controllen = %u, cmsg_len = %u",
	    (u_int)msg.msg_controllen, (u_int)cmptr->cmsg_len));

	if (cmptr->cmsg_level != SOL_SOCKET) {
		logmsgx("cmsg_level %d != SOL_SOCKET", cmptr->cmsg_level);
		goto done;
	}

	if (cmptr->cmsg_type != SCM_TIMESTAMP) {
		logmsgx("cmsg_type %d != SCM_TIMESTAMP", cmptr->cmsg_type);
		goto done;
	}

	if (cmptr->cmsg_len != CMSG_LEN(sizeof(struct timeval))) {
		logmsgx("cmsg_len %u != %lu (CMSG_LEN(sizeof(struct timeval))",
		    (u_int)cmptr->cmsg_len, (u_long)CMSG_LEN(sizeof(struct timeval)));
		goto done;
	}

	timeval = (const struct timeval *)CMSG_DATA(cmptr);

	dbgmsg(("timeval tv_sec %jd, tv_usec %jd",
	    (intmax_t)timeval->tv_sec, (intmax_t)timeval->tv_usec));

	if ((cmptr = CMSG_NXTHDR(&msg, cmptr)) != NULL) {
		logmsgx("control data has extra header");
		goto done;
	}

	error = 0;

done:
	if (sock_type == SOCK_STREAM)
		if (close(fd2) < 0) {
			logmsg("close");
			return (-2);
		}
	return (error);

failed:
	if (sock_type == SOCK_STREAM)
		if (close(fd2) < 0)
			logmsg("close");
	return (-2);
}

static int
t_timestamp(void)
{
	int error, fd;

	if ((fd = create_server_socket()) < 0)
		return (-2);

	if (sock_type == SOCK_STREAM)
		if (listen(fd, LISTENQ) < 0) {
			logmsg("listen");
			goto failed;
		}

	if ((client_pid = fork()) == (pid_t)-1) {
		logmsg("fork");
		goto failed;
	}

	if (client_pid == 0) {
		myname = "CLIENT";
		if (close_socket((const char *)NULL, fd) < 0)
			_exit(1);
		t_timestamp_client();
	}

	if ((error = t_timestamp_server(fd)) == -2) {
		(void)wait_client();
		goto failed;
	}

	if (wait_client() < 0)
		goto failed;

	if (close_socket(serv_sock_path, fd) < 0) {
		logmsgx("close_socket failed");
		return (-2);
	}
	return (error);

failed:
	if (close_socket(serv_sock_path, fd) < 0)
		logmsgx("close_socket failed");
	return (-2);
}
