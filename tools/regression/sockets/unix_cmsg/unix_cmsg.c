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

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ucred.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "uc_common.h"
#include "t_cmsgcred.h"
#include "t_bintime.h"
#include "t_generic.h"
#include "t_peercred.h"
#include "t_timeval.h"
#include "t_sockcred.h"
#include "t_cmsgcred_sockcred.h"
#include "t_cmsg_len.h"
#include "t_timespec_real.h"
#include "t_timespec_mono.h"

/*
 * There are tables with tests descriptions and pointers to test
 * functions.  Each t_*() function returns 0 if its test passed,
 * -1 if its test failed, -2 if some system error occurred.
 * If a test function returns -2, then a program exits.
 *
 * If a test function forks a client process, then it waits for its
 * termination.  If a return code of a client process is not equal
 * to zero, or if a client process was terminated by a signal, then
 * a test function returns -1 or -2 depending on exit status of
 * a client process.
 *
 * Each function which can block, is run under TIMEOUT.  If timeout
 * occurs, then a test function returns -2 or a client process exits
 * with a non-zero return code.
 */

#ifndef LISTENQ
# define LISTENQ	1
#endif

#ifndef TIMEOUT
# define TIMEOUT	2
#endif

static int	t_cmsgcred(void);
static int	t_sockcred_1(void);
static int	t_sockcred_2(void);
static int	t_cmsgcred_sockcred(void);
static int	t_timeval(void);
static int	t_bintime(void);
/*
 * The testcase fails on 64-bit architectures (amd64), but passes on 32-bit
 * architectures (i386); see bug 206543
 */
#ifndef __LP64__
static int	t_cmsg_len(void);
#endif
static int	t_peercred(void);

struct test_func {
	int		(*func)(void);
	const char	*desc;
};

static const struct test_func test_stream_tbl[] = {
	{
	  .func = NULL,
	  .desc = "All tests"
	},
	{
	  .func = t_cmsgcred,
	  .desc = "Sending, receiving cmsgcred"
	},
	{
	  .func = t_sockcred_1,
	  .desc = "Receiving sockcred (listening socket)"
	},
	{
	  .func = t_sockcred_2,
	  .desc = "Receiving sockcred (accepted socket)"
	},
	{
	  .func = t_cmsgcred_sockcred,
	  .desc = "Sending cmsgcred, receiving sockcred"
	},
	{
	  .func = t_timeval,
	  .desc = "Sending, receiving timeval"
	},
	{
	  .func = t_bintime,
	  .desc = "Sending, receiving bintime"
	},
#ifndef __LP64__
	{
	  .func = t_cmsg_len,
	  .desc = "Check cmsghdr.cmsg_len"
	},
#endif
	{
	  .func = t_peercred,
	  .desc = "Check LOCAL_PEERCRED socket option"
	},
#if defined(SCM_REALTIME)
	{
	  .func = t_timespec_real,
	  .desc = "Sending, receiving realtime"
	},
#endif
#if defined(SCM_MONOTONIC)
	{
	  .func = t_timespec_mono,
	  .desc = "Sending, receiving monotonic time (uptime)"
	}
#endif
};

#define TEST_STREAM_TBL_SIZE \
	(sizeof(test_stream_tbl) / sizeof(test_stream_tbl[0]))

static const struct test_func test_dgram_tbl[] = {
	{
	  .func = NULL,
	  .desc = "All tests"
	},
	{
	  .func = t_cmsgcred,
	  .desc = "Sending, receiving cmsgcred"
	},
	{
	  .func = t_sockcred_2,
	  .desc = "Receiving sockcred"
	},
	{
	  .func = t_cmsgcred_sockcred,
	  .desc = "Sending cmsgcred, receiving sockcred"
	},
	{
	  .func = t_timeval,
	  .desc = "Sending, receiving timeval"
	},
	{
	  .func = t_bintime,
	  .desc = "Sending, receiving bintime"
	},
#ifndef __LP64__
	{
	  .func = t_cmsg_len,
	  .desc = "Check cmsghdr.cmsg_len"
	},
#endif
#if defined(SCM_REALTIME)
	{
	  .func = t_timespec_real,
	  .desc = "Sending, receiving realtime"
	},
#endif
#if defined(SCM_MONOTONIC)
	{
	  .func = t_timespec_mono,
	  .desc = "Sending, receiving monotonic time (uptime)"
	}
#endif
};

#define TEST_DGRAM_TBL_SIZE \
	(sizeof(test_dgram_tbl) / sizeof(test_dgram_tbl[0]))

static bool	debug = false;
static bool	server_flag = true;
static bool	send_data_flag = true;
static bool	send_array_flag = true;	
static bool	failed_flag = false;

static int	sock_type;
static const char *sock_type_str;

static const char *proc_name;

static char	work_dir[] = _PATH_TMP "unix_cmsg.XXXXXXX";
static int	serv_sock_fd;
static struct sockaddr_un serv_addr_sun;

static struct {
	char		*buf_send;
	char		*buf_recv;
	size_t		buf_size;
	u_int		msg_num;
}		ipc_msg;

#define IPC_MSG_NUM_DEF		5
#define IPC_MSG_NUM_MAX		10
#define IPC_MSG_SIZE_DEF	7
#define IPC_MSG_SIZE_MAX	128

static struct {
	uid_t		uid;
	uid_t		euid;
	gid_t		gid;
	gid_t		egid;
	gid_t		*gid_arr;
	int		gid_num;
}		proc_cred;

static pid_t	client_pid;

#define SYNC_SERVER	0
#define SYNC_CLIENT	1
#define SYNC_RECV	0
#define SYNC_SEND	1

static int	sync_fd[2][2];

#define LOGMSG_SIZE	128

static void	logmsg(const char *, ...) __printflike(1, 2);
static void	logmsgx(const char *, ...) __printflike(1, 2);
static void	dbgmsg(const char *, ...) __printflike(1, 2);
static void	output(const char *, ...) __printflike(1, 2);

static void
usage(bool verbose)
{
	u_int i;

	printf("usage: %s [-dh] [-n num] [-s size] [-t type] "
	    "[-z value] [testno]\n", getprogname());
	if (!verbose)
		return;
	printf("\n Options are:\n\
  -d            Output debugging information\n\
  -h            Output the help message and exit\n\
  -n num        Number of messages to send\n\
  -s size       Specify size of data for IPC\n\
  -t type       Specify socket type (stream, dgram) for tests\n\
  -z value      Do not send data in a message (bit 0x1), do not send\n\
                data array associated with a cmsghdr structure (bit 0x2)\n\
  testno        Run one test by its number (require the -t option)\n\n");
	printf(" Available tests for stream sockets:\n");
	for (i = 0; i < TEST_STREAM_TBL_SIZE; ++i)
		printf("   %u: %s\n", i, test_stream_tbl[i].desc);
	printf("\n Available tests for datagram sockets:\n");
	for (i = 0; i < TEST_DGRAM_TBL_SIZE; ++i)
		printf("   %u: %s\n", i, test_dgram_tbl[i].desc);
}

static void
output(const char *format, ...)
{
	char buf[LOGMSG_SIZE];
	va_list ap;

	va_start(ap, format);
	if (vsnprintf(buf, sizeof(buf), format, ap) < 0)
		err(EXIT_FAILURE, "output: vsnprintf failed");
	write(STDOUT_FILENO, buf, strlen(buf));
	va_end(ap);
}

static void
logmsg(const char *format, ...)
{
	char buf[LOGMSG_SIZE];
	va_list ap;
	int errno_save;

	errno_save = errno;
	va_start(ap, format);
	if (vsnprintf(buf, sizeof(buf), format, ap) < 0)
		err(EXIT_FAILURE, "logmsg: vsnprintf failed");
	if (errno_save == 0)
		output("%s: %s\n", proc_name, buf);
	else
		output("%s: %s: %s\n", proc_name, buf, strerror(errno_save));
	va_end(ap);
	errno = errno_save;
}

static void
vlogmsgx(const char *format, va_list ap)
{
	char buf[LOGMSG_SIZE];

	if (vsnprintf(buf, sizeof(buf), format, ap) < 0)
		err(EXIT_FAILURE, "logmsgx: vsnprintf failed");
	output("%s: %s\n", proc_name, buf);

}

static void
logmsgx(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vlogmsgx(format, ap);
	va_end(ap);
}

static void
dbgmsg(const char *format, ...)
{
	va_list ap;

	if (debug) {
		va_start(ap, format);
		vlogmsgx(format, ap);
		va_end(ap);
	}
}

static int
run_tests(int type, u_int testno1)
{
	const struct test_func *tf;
	u_int i, testno2, failed_num;

	sock_type = type;
	if (type == SOCK_STREAM) {
		sock_type_str = "SOCK_STREAM";
		tf = test_stream_tbl;
		i = TEST_STREAM_TBL_SIZE - 1;
	} else {
		sock_type_str = "SOCK_DGRAM";
		tf = test_dgram_tbl;
		i = TEST_DGRAM_TBL_SIZE - 1;
	}
	if (testno1 == 0) {
		testno1 = 1;
		testno2 = i;
	} else
		testno2 = testno1;

	output("Running tests for %s sockets:\n", sock_type_str);
	failed_num = 0;
	for (i = testno1, tf += testno1; i <= testno2; ++tf, ++i) {
		output("  %u: %s\n", i, tf->desc);
		switch (tf->func()) {
		case -1:
			++failed_num;
			break;
		case -2:
			logmsgx("some system error or timeout occurred");
			return (-1);
		}
	}

	if (failed_num != 0)
		failed_flag = true;

	if (testno1 != testno2) {
		if (failed_num == 0)
			output("-- all tests passed!\n");
		else
			output("-- %u test%s failed!\n",
			    failed_num, failed_num == 1 ? "" : "s");
	} else {
		if (failed_num == 0)
			output("-- test passed!\n");
		else
			output("-- test failed!\n");
	}

	return (0);
}

static int
init(void)
{
	struct sigaction sigact;
	size_t idx;
	int rv;

	proc_name = "SERVER";

	sigact.sa_handler = SIG_IGN;
	sigact.sa_flags = 0;
	sigemptyset(&sigact.sa_mask);
	if (sigaction(SIGPIPE, &sigact, (struct sigaction *)NULL) < 0) {
		logmsg("init: sigaction");
		return (-1);
	}

	if (ipc_msg.buf_size == 0)
		ipc_msg.buf_send = ipc_msg.buf_recv = NULL;
	else {
		ipc_msg.buf_send = malloc(ipc_msg.buf_size);
		ipc_msg.buf_recv = malloc(ipc_msg.buf_size);
		if (ipc_msg.buf_send == NULL || ipc_msg.buf_recv == NULL) {
			logmsg("init: malloc");
			return (-1);
		}
		for (idx = 0; idx < ipc_msg.buf_size; ++idx)
			ipc_msg.buf_send[idx] = (char)idx;
	}

	proc_cred.uid = getuid();
	proc_cred.euid = geteuid();
	proc_cred.gid = getgid();
	proc_cred.egid = getegid();
	proc_cred.gid_num = getgroups(0, (gid_t *)NULL);
	if (proc_cred.gid_num < 0) {
		logmsg("init: getgroups");
		return (-1);
	}
	proc_cred.gid_arr = malloc(proc_cred.gid_num *
	    sizeof(*proc_cred.gid_arr));
	if (proc_cred.gid_arr == NULL) {
		logmsg("init: malloc");
		return (-1);
	}
	if (getgroups(proc_cred.gid_num, proc_cred.gid_arr) < 0) {
		logmsg("init: getgroups");
		return (-1);
	}

	memset(&serv_addr_sun, 0, sizeof(serv_addr_sun));
	rv = snprintf(serv_addr_sun.sun_path, sizeof(serv_addr_sun.sun_path),
	    "%s/%s", work_dir, proc_name);
	if (rv < 0) {
		logmsg("init: snprintf");
		return (-1);
	}
	if ((size_t)rv >= sizeof(serv_addr_sun.sun_path)) {
		logmsgx("init: not enough space for socket pathname");
		return (-1);
	}
	serv_addr_sun.sun_family = PF_LOCAL;
	serv_addr_sun.sun_len = SUN_LEN(&serv_addr_sun);

	return (0);
}

static int
client_fork(void)
{
	int fd1, fd2;

	if (pipe(sync_fd[SYNC_SERVER]) < 0 ||
	    pipe(sync_fd[SYNC_CLIENT]) < 0) {
		logmsg("client_fork: pipe");
		return (-1);
	}
	client_pid = fork();
	if (client_pid == (pid_t)-1) {
		logmsg("client_fork: fork");
		return (-1);
	}
	if (client_pid == 0) {
		proc_name = "CLIENT";
		server_flag = false;
		fd1 = sync_fd[SYNC_SERVER][SYNC_RECV];
		fd2 = sync_fd[SYNC_CLIENT][SYNC_SEND];
	} else {
		fd1 = sync_fd[SYNC_SERVER][SYNC_SEND];
		fd2 = sync_fd[SYNC_CLIENT][SYNC_RECV];
	}
	if (close(fd1) < 0 || close(fd2) < 0) {
		logmsg("client_fork: close");
		return (-1);
	}
	return (client_pid != 0);
}

static void
client_exit(int rv)
{
	if (close(sync_fd[SYNC_SERVER][SYNC_SEND]) < 0 ||
	    close(sync_fd[SYNC_CLIENT][SYNC_RECV]) < 0) {
		logmsg("client_exit: close");
		rv = -1;
	}
	rv = rv == 0 ? EXIT_SUCCESS : -rv;
	dbgmsg("exit: code %d", rv);
	_exit(rv);
}

static int
client_wait(void)
{
	int status;
	pid_t pid;

	dbgmsg("waiting for client");

	if (close(sync_fd[SYNC_SERVER][SYNC_RECV]) < 0 ||
	    close(sync_fd[SYNC_CLIENT][SYNC_SEND]) < 0) {
		logmsg("client_wait: close");
		return (-1);
	}

	pid = waitpid(client_pid, &status, 0);
	if (pid == (pid_t)-1) {
		logmsg("client_wait: waitpid");
		return (-1);
	}

	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) != EXIT_SUCCESS) {
			logmsgx("client exit status is %d",
			    WEXITSTATUS(status));
			return (-WEXITSTATUS(status));
		}
	} else {
		if (WIFSIGNALED(status))
			logmsgx("abnormal termination of client, signal %d%s",
			    WTERMSIG(status), WCOREDUMP(status) ?
			    " (core file generated)" : "");
		else
			logmsgx("termination of client, unknown status");
		return (-1);
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	const char *errstr;
	u_int testno, zvalue;
	int opt, rv;
	bool dgram_flag, stream_flag;

	ipc_msg.buf_size = IPC_MSG_SIZE_DEF;
	ipc_msg.msg_num = IPC_MSG_NUM_DEF;
	dgram_flag = stream_flag = false;
	while ((opt = getopt(argc, argv, "dhn:s:t:z:")) != -1)
		switch (opt) {
		case 'd':
			debug = true;
			break;
		case 'h':
			usage(true);
			return (EXIT_SUCCESS);
		case 'n':
			ipc_msg.msg_num = strtonum(optarg, 1,
			    IPC_MSG_NUM_MAX, &errstr);
			if (errstr != NULL)
				errx(EXIT_FAILURE, "option -n: number is %s",
				    errstr);
			break;
		case 's':
			ipc_msg.buf_size = strtonum(optarg, 0,
			    IPC_MSG_SIZE_MAX, &errstr);
			if (errstr != NULL)
				errx(EXIT_FAILURE, "option -s: number is %s",
				    errstr);
			break;
		case 't':
			if (strcmp(optarg, "stream") == 0)
				stream_flag = true;
			else if (strcmp(optarg, "dgram") == 0)
				dgram_flag = true;
			else
				errx(EXIT_FAILURE, "option -t: "
				    "wrong socket type");
			break;
		case 'z':
			zvalue = strtonum(optarg, 0, 3, &errstr);
			if (errstr != NULL)
				errx(EXIT_FAILURE, "option -z: number is %s",
				    errstr);
			if (zvalue & 0x1)
				send_data_flag = false;
			if (zvalue & 0x2)
				send_array_flag = false;
			break;
		default:
			usage(false);
			return (EXIT_FAILURE);
		}

	if (optind < argc) {
		if (optind + 1 != argc)
			errx(EXIT_FAILURE, "too many arguments");
		testno = strtonum(argv[optind], 0, UINT_MAX, &errstr);
		if (errstr != NULL)
			errx(EXIT_FAILURE, "test number is %s", errstr);
		if (stream_flag && testno >= TEST_STREAM_TBL_SIZE)
			errx(EXIT_FAILURE, "given test %u for stream "
			    "sockets does not exist", testno);
		if (dgram_flag && testno >= TEST_DGRAM_TBL_SIZE)
			errx(EXIT_FAILURE, "given test %u for datagram "
			    "sockets does not exist", testno);
	} else
		testno = 0;

	if (!dgram_flag && !stream_flag) {
		if (testno != 0)
			errx(EXIT_FAILURE, "particular test number "
			    "can be used with the -t option only");
		dgram_flag = stream_flag = true;
	}

	if (mkdtemp(work_dir) == NULL)
		err(EXIT_FAILURE, "mkdtemp(%s)", work_dir);

	rv = EXIT_FAILURE;
	if (init() < 0)
		goto done;

	if (stream_flag)
		if (run_tests(SOCK_STREAM, testno) < 0)
			goto done;
	if (dgram_flag)
		if (run_tests(SOCK_DGRAM, testno) < 0)
			goto done;

	rv = EXIT_SUCCESS;
done:
	if (rmdir(work_dir) < 0) {
		logmsg("rmdir(%s)", work_dir);
		rv = EXIT_FAILURE;
	}
	return (failed_flag ? EXIT_FAILURE : rv);
}

static int
socket_close(int fd)
{
	int rv;

	rv = 0;
	if (close(fd) < 0) {
		logmsg("socket_close: close");
		rv = -1;
	}
	if (server_flag && fd == serv_sock_fd)
		if (unlink(serv_addr_sun.sun_path) < 0) {
			logmsg("socket_close: unlink(%s)",
			    serv_addr_sun.sun_path);
			rv = -1;
		}
	return (rv);
}

static int
socket_create(void)
{
	struct timeval tv;
	int fd;

	fd = socket(PF_LOCAL, sock_type, 0);
	if (fd < 0) {
		logmsg("socket_create: socket(PF_LOCAL, %s, 0)", sock_type_str);
		return (-1);
	}
	if (server_flag)
		serv_sock_fd = fd;

	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0 ||
	    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
		logmsg("socket_create: setsockopt(SO_RCVTIMEO/SO_SNDTIMEO)");
		goto failed;
	}

	if (server_flag) {
		if (bind(fd, (struct sockaddr *)&serv_addr_sun,
		    serv_addr_sun.sun_len) < 0) {
			logmsg("socket_create: bind(%s)",
			    serv_addr_sun.sun_path);
			goto failed;
		}
		if (sock_type == SOCK_STREAM) {
			int val;

			if (listen(fd, LISTENQ) < 0) {
				logmsg("socket_create: listen");
				goto failed;
			}
			val = fcntl(fd, F_GETFL, 0);
			if (val < 0) {
				logmsg("socket_create: fcntl(F_GETFL)");
				goto failed;
			}
			if (fcntl(fd, F_SETFL, val | O_NONBLOCK) < 0) {
				logmsg("socket_create: fcntl(F_SETFL)");
				goto failed;
			}
		}
	}

	return (fd);

failed:
	if (close(fd) < 0)
		logmsg("socket_create: close");
	if (server_flag)
		if (unlink(serv_addr_sun.sun_path) < 0)
			logmsg("socket_close: unlink(%s)",
			    serv_addr_sun.sun_path);
	return (-1);
}

static int
socket_connect(int fd)
{
	dbgmsg("connect");

	if (connect(fd, (struct sockaddr *)&serv_addr_sun,
	    serv_addr_sun.sun_len) < 0) {
		logmsg("socket_connect: connect(%s)", serv_addr_sun.sun_path);
		return (-1);
	}
	return (0);
}

static int
sync_recv(void)
{
	ssize_t ssize;
	int fd;
	char buf;

	dbgmsg("sync: wait");

	fd = sync_fd[server_flag ? SYNC_SERVER : SYNC_CLIENT][SYNC_RECV];

	ssize = read(fd, &buf, 1);
	if (ssize < 0) {
		logmsg("sync_recv: read");
		return (-1);
	}
	if (ssize < 1) {
		logmsgx("sync_recv: read %zd of 1 byte", ssize);
		return (-1);
	}

	dbgmsg("sync: received");

	return (0);
}

static int
sync_send(void)
{
	ssize_t ssize;
	int fd;

	dbgmsg("sync: send");

	fd = sync_fd[server_flag ? SYNC_CLIENT : SYNC_SERVER][SYNC_SEND];

	ssize = write(fd, "", 1);
	if (ssize < 0) {
		logmsg("sync_send: write");
		return (-1);
	}
	if (ssize < 1) {
		logmsgx("sync_send: sent %zd of 1 byte", ssize);
		return (-1);
	}

	return (0);
}

static int
message_send(int fd, const struct msghdr *msghdr)
{
	const struct cmsghdr *cmsghdr;
	size_t size;
	ssize_t ssize;

	size = msghdr->msg_iov != 0 ? msghdr->msg_iov->iov_len : 0;
	dbgmsg("send: data size %zu", size);
	dbgmsg("send: msghdr.msg_controllen %u",
	    (u_int)msghdr->msg_controllen);
	cmsghdr = CMSG_FIRSTHDR(msghdr);
	if (cmsghdr != NULL)
		dbgmsg("send: cmsghdr.cmsg_len %u",
		    (u_int)cmsghdr->cmsg_len);

	ssize = sendmsg(fd, msghdr, 0);
	if (ssize < 0) {
		logmsg("message_send: sendmsg");
		return (-1);
	}
	if ((size_t)ssize != size) {
		logmsgx("message_send: sendmsg: sent %zd of %zu bytes",
		    ssize, size);
		return (-1);
	}

	if (!send_data_flag)
		if (sync_send() < 0)
			return (-1);

	return (0);
}

static int
message_sendn(int fd, struct msghdr *msghdr)
{
	u_int i;

	for (i = 1; i <= ipc_msg.msg_num; ++i) {
		dbgmsg("message #%u", i);
		if (message_send(fd, msghdr) < 0)
			return (-1);
	}
	return (0);
}

static int
message_recv(int fd, struct msghdr *msghdr)
{
	const struct cmsghdr *cmsghdr;
	size_t size;
	ssize_t ssize;

	if (!send_data_flag)
		if (sync_recv() < 0)
			return (-1);

	size = msghdr->msg_iov != NULL ? msghdr->msg_iov->iov_len : 0;
	ssize = recvmsg(fd, msghdr, MSG_WAITALL);
	if (ssize < 0) {
		logmsg("message_recv: recvmsg");
		return (-1);
	}
	if ((size_t)ssize != size) {
		logmsgx("message_recv: recvmsg: received %zd of %zu bytes",
		    ssize, size);
		return (-1);
	}

	dbgmsg("recv: data size %zd", ssize);
	dbgmsg("recv: msghdr.msg_controllen %u",
	    (u_int)msghdr->msg_controllen);
	cmsghdr = CMSG_FIRSTHDR(msghdr);
	if (cmsghdr != NULL)
		dbgmsg("recv: cmsghdr.cmsg_len %u",
		    (u_int)cmsghdr->cmsg_len);

	if (memcmp(ipc_msg.buf_recv, ipc_msg.buf_send, size) != 0) {
		logmsgx("message_recv: received message has wrong content");
		return (-1);
	}

	return (0);
}

static int
socket_accept(int listenfd)
{
	fd_set rset;
	struct timeval tv;
	int fd, rv, val;

	dbgmsg("accept");

	FD_ZERO(&rset);
	FD_SET(listenfd, &rset);
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	rv = select(listenfd + 1, &rset, (fd_set *)NULL, (fd_set *)NULL, &tv);
	if (rv < 0) {
		logmsg("socket_accept: select");
		return (-1);
	}
	if (rv == 0) {
		logmsgx("socket_accept: select timeout");
		return (-1);
	}

	fd = accept(listenfd, (struct sockaddr *)NULL, (socklen_t *)NULL);
	if (fd < 0) {
		logmsg("socket_accept: accept");
		return (-1);
	}

	val = fcntl(fd, F_GETFL, 0);
	if (val < 0) {
		logmsg("socket_accept: fcntl(F_GETFL)");
		goto failed;
	}
	if (fcntl(fd, F_SETFL, val & ~O_NONBLOCK) < 0) {
		logmsg("socket_accept: fcntl(F_SETFL)");
		goto failed;
	}

	return (fd);

failed:
	if (close(fd) < 0)
		logmsg("socket_accept: close");
	return (-1);
}

static int
check_msghdr(const struct msghdr *msghdr, size_t size)
{
	if (msghdr->msg_flags & MSG_TRUNC) {
		logmsgx("msghdr.msg_flags has MSG_TRUNC");
		return (-1);
	}
	if (msghdr->msg_flags & MSG_CTRUNC) {
		logmsgx("msghdr.msg_flags has MSG_CTRUNC");
		return (-1);
	}
	if (msghdr->msg_controllen < size) {
		logmsgx("msghdr.msg_controllen %u < %zu",
		    (u_int)msghdr->msg_controllen, size);
		return (-1);
	}
	if (msghdr->msg_controllen > 0 && size == 0) {
		logmsgx("msghdr.msg_controllen %u > 0",
		    (u_int)msghdr->msg_controllen);
		return (-1);
	}
	return (0);
}

static int
check_cmsghdr(const struct cmsghdr *cmsghdr, int type, size_t size)
{
	if (cmsghdr == NULL) {
		logmsgx("cmsghdr is NULL");
		return (-1);
	}
	if (cmsghdr->cmsg_level != SOL_SOCKET) {
		logmsgx("cmsghdr.cmsg_level %d != SOL_SOCKET",
		    cmsghdr->cmsg_level);
		return (-1);
	}
	if (cmsghdr->cmsg_type != type) {
		logmsgx("cmsghdr.cmsg_type %d != %d",
		    cmsghdr->cmsg_type, type);
		return (-1);
	}
	if (cmsghdr->cmsg_len != CMSG_LEN(size)) {
		logmsgx("cmsghdr.cmsg_len %u != %zu",
		    (u_int)cmsghdr->cmsg_len, CMSG_LEN(size));
		return (-1);
	}
	return (0);
}

static int
check_groups(const char *gid_arr_str, const gid_t *gid_arr,
    const char *gid_num_str, int gid_num, bool all_gids)
{
	int i;

	for (i = 0; i < gid_num; ++i)
		dbgmsg("%s[%d] %lu", gid_arr_str, i, (u_long)gid_arr[i]);

	if (all_gids) {
		if (gid_num != proc_cred.gid_num) {
			logmsgx("%s %d != %d", gid_num_str, gid_num,
			    proc_cred.gid_num);
			return (-1);
		}
	} else {
		if (gid_num > proc_cred.gid_num) {
			logmsgx("%s %d > %d", gid_num_str, gid_num,
			    proc_cred.gid_num);
			return (-1);
		}
	}
	if (memcmp(gid_arr, proc_cred.gid_arr,
	    gid_num * sizeof(*gid_arr)) != 0) {
		logmsgx("%s content is wrong", gid_arr_str);
		for (i = 0; i < gid_num; ++i)
			if (gid_arr[i] != proc_cred.gid_arr[i]) {
				logmsgx("%s[%d] %lu != %lu",
				    gid_arr_str, i, (u_long)gid_arr[i],
				    (u_long)proc_cred.gid_arr[i]);
				break;
			}
		return (-1);
	}
	return (0);
}

static int
check_xucred(const struct xucred *xucred, socklen_t len)
{
	int rc;

	if (len != sizeof(*xucred)) {
		logmsgx("option value size %zu != %zu",
		    (size_t)len, sizeof(*xucred));
		return (-1);
	}

	dbgmsg("xucred.cr_version %u", xucred->cr_version);
	dbgmsg("xucred.cr_uid %lu", (u_long)xucred->cr_uid);
	dbgmsg("xucred.cr_ngroups %d", xucred->cr_ngroups);

	rc = 0;

	if (xucred->cr_version != XUCRED_VERSION) {
		logmsgx("xucred.cr_version %u != %d",
		    xucred->cr_version, XUCRED_VERSION);
		rc = -1;
	}
	if (xucred->cr_uid != proc_cred.euid) {
		logmsgx("xucred.cr_uid %lu != %lu (EUID)",
		   (u_long)xucred->cr_uid, (u_long)proc_cred.euid);
		rc = -1;
	}
	if (xucred->cr_ngroups == 0) {
		logmsgx("xucred.cr_ngroups == 0");
		rc = -1;
	}
	if (xucred->cr_ngroups < 0) {
		logmsgx("xucred.cr_ngroups < 0");
		rc = -1;
	}
	if (xucred->cr_ngroups > XU_NGROUPS) {
		logmsgx("xucred.cr_ngroups %hu > %u (max)",
		    xucred->cr_ngroups, XU_NGROUPS);
		rc = -1;
	}
	if (xucred->cr_groups[0] != proc_cred.egid) {
		logmsgx("xucred.cr_groups[0] %lu != %lu (EGID)",
		    (u_long)xucred->cr_groups[0], (u_long)proc_cred.egid);
		rc = -1;
	}
	if (check_groups("xucred.cr_groups", xucred->cr_groups,
	    "xucred.cr_ngroups", xucred->cr_ngroups, false) < 0)
		rc = -1;
	return (rc);
}

static int
check_scm_creds_cmsgcred(struct cmsghdr *cmsghdr)
{
	const struct cmsgcred *cmcred;
	int rc;

	if (check_cmsghdr(cmsghdr, SCM_CREDS, sizeof(struct cmsgcred)) < 0)
		return (-1);

	cmcred = (struct cmsgcred *)CMSG_DATA(cmsghdr);

	dbgmsg("cmsgcred.cmcred_pid %ld", (long)cmcred->cmcred_pid);
	dbgmsg("cmsgcred.cmcred_uid %lu", (u_long)cmcred->cmcred_uid);
	dbgmsg("cmsgcred.cmcred_euid %lu", (u_long)cmcred->cmcred_euid);
	dbgmsg("cmsgcred.cmcred_gid %lu", (u_long)cmcred->cmcred_gid);
	dbgmsg("cmsgcred.cmcred_ngroups %d", cmcred->cmcred_ngroups);

	rc = 0;

	if (cmcred->cmcred_pid != client_pid) {
		logmsgx("cmsgcred.cmcred_pid %ld != %ld",
		    (long)cmcred->cmcred_pid, (long)client_pid);
		rc = -1;
	}
	if (cmcred->cmcred_uid != proc_cred.uid) {
		logmsgx("cmsgcred.cmcred_uid %lu != %lu",
		    (u_long)cmcred->cmcred_uid, (u_long)proc_cred.uid);
		rc = -1;
	}
	if (cmcred->cmcred_euid != proc_cred.euid) {
		logmsgx("cmsgcred.cmcred_euid %lu != %lu",
		    (u_long)cmcred->cmcred_euid, (u_long)proc_cred.euid);
		rc = -1;
	}
	if (cmcred->cmcred_gid != proc_cred.gid) {
		logmsgx("cmsgcred.cmcred_gid %lu != %lu",
		    (u_long)cmcred->cmcred_gid, (u_long)proc_cred.gid);
		rc = -1;
	}
	if (cmcred->cmcred_ngroups == 0) {
		logmsgx("cmsgcred.cmcred_ngroups == 0");
		rc = -1;
	}
	if (cmcred->cmcred_ngroups < 0) {
		logmsgx("cmsgcred.cmcred_ngroups %d < 0",
		    cmcred->cmcred_ngroups);
		rc = -1;
	}
	if (cmcred->cmcred_ngroups > CMGROUP_MAX) {
		logmsgx("cmsgcred.cmcred_ngroups %d > %d",
		    cmcred->cmcred_ngroups, CMGROUP_MAX);
		rc = -1;
	}
	if (cmcred->cmcred_groups[0] != proc_cred.egid) {
		logmsgx("cmsgcred.cmcred_groups[0] %lu != %lu (EGID)",
		    (u_long)cmcred->cmcred_groups[0], (u_long)proc_cred.egid);
		rc = -1;
	}
	if (check_groups("cmsgcred.cmcred_groups", cmcred->cmcred_groups,
	    "cmsgcred.cmcred_ngroups", cmcred->cmcred_ngroups, false) < 0)
		rc = -1;
	return (rc);
}

static int
check_scm_creds_sockcred(struct cmsghdr *cmsghdr)
{
	const struct sockcred *sc;
	int rc;

	if (check_cmsghdr(cmsghdr, SCM_CREDS,
	    SOCKCREDSIZE(proc_cred.gid_num)) < 0)
		return (-1);

	sc = (struct sockcred *)CMSG_DATA(cmsghdr);

	rc = 0;

	dbgmsg("sockcred.sc_uid %lu", (u_long)sc->sc_uid);
	dbgmsg("sockcred.sc_euid %lu", (u_long)sc->sc_euid);
	dbgmsg("sockcred.sc_gid %lu", (u_long)sc->sc_gid);
	dbgmsg("sockcred.sc_egid %lu", (u_long)sc->sc_egid);
	dbgmsg("sockcred.sc_ngroups %d", sc->sc_ngroups);

	if (sc->sc_uid != proc_cred.uid) {
		logmsgx("sockcred.sc_uid %lu != %lu",
		    (u_long)sc->sc_uid, (u_long)proc_cred.uid);
		rc = -1;
	}
	if (sc->sc_euid != proc_cred.euid) {
		logmsgx("sockcred.sc_euid %lu != %lu",
		    (u_long)sc->sc_euid, (u_long)proc_cred.euid);
		rc = -1;
	}
	if (sc->sc_gid != proc_cred.gid) {
		logmsgx("sockcred.sc_gid %lu != %lu",
		    (u_long)sc->sc_gid, (u_long)proc_cred.gid);
		rc = -1;
	}
	if (sc->sc_egid != proc_cred.egid) {
		logmsgx("sockcred.sc_egid %lu != %lu",
		    (u_long)sc->sc_egid, (u_long)proc_cred.egid);
		rc = -1;
	}
	if (sc->sc_ngroups == 0) {
		logmsgx("sockcred.sc_ngroups == 0");
		rc = -1;
	}
	if (sc->sc_ngroups < 0) {
		logmsgx("sockcred.sc_ngroups %d < 0",
		    sc->sc_ngroups);
		rc = -1;
	}
	if (sc->sc_ngroups != proc_cred.gid_num) {
		logmsgx("sockcred.sc_ngroups %d != %u",
		    sc->sc_ngroups, proc_cred.gid_num);
		rc = -1;
	}
	if (check_groups("sockcred.sc_groups", sc->sc_groups,
	    "sockcred.sc_ngroups", sc->sc_ngroups, true) < 0)
		rc = -1;
	return (rc);
}

static int
check_scm_timestamp(struct cmsghdr *cmsghdr)
{
	const struct timeval *tv;

	if (check_cmsghdr(cmsghdr, SCM_TIMESTAMP, sizeof(struct timeval)) < 0)
		return (-1);

	tv = (struct timeval *)CMSG_DATA(cmsghdr);

	dbgmsg("timeval.tv_sec %"PRIdMAX", timeval.tv_usec %"PRIdMAX,
	    (intmax_t)tv->tv_sec, (intmax_t)tv->tv_usec);

	return (0);
}

static int
check_scm_bintime(struct cmsghdr *cmsghdr)
{
	const struct bintime *bt;

	if (check_cmsghdr(cmsghdr, SCM_BINTIME, sizeof(struct bintime)) < 0)
		return (-1);

	bt = (struct bintime *)CMSG_DATA(cmsghdr);

	dbgmsg("bintime.sec %"PRIdMAX", bintime.frac %"PRIu64,
	    (intmax_t)bt->sec, bt->frac);

	return (0);
}

static void
msghdr_init_generic(struct msghdr *msghdr, struct iovec *iov, void *cmsg_data)
{
	msghdr->msg_name = NULL;
	msghdr->msg_namelen = 0;
	if (send_data_flag) {
		iov->iov_base = server_flag ?
		    ipc_msg.buf_recv : ipc_msg.buf_send;
		iov->iov_len = ipc_msg.buf_size;
		msghdr->msg_iov = iov;
		msghdr->msg_iovlen = 1;
	} else {
		msghdr->msg_iov = NULL;
		msghdr->msg_iovlen = 0;
	}
	msghdr->msg_control = cmsg_data;
	msghdr->msg_flags = 0;
}

static void
msghdr_init_server(struct msghdr *msghdr, struct iovec *iov,
    void *cmsg_data, size_t cmsg_size)
{
	msghdr_init_generic(msghdr, iov, cmsg_data);
	msghdr->msg_controllen = cmsg_size;
	dbgmsg("init: data size %zu", msghdr->msg_iov != NULL ?
	    msghdr->msg_iov->iov_len : (size_t)0);
	dbgmsg("init: msghdr.msg_controllen %u",
	    (u_int)msghdr->msg_controllen);
}

static void
msghdr_init_client(struct msghdr *msghdr, struct iovec *iov,
    void *cmsg_data, size_t cmsg_size, int type, size_t arr_size)
{
	struct cmsghdr *cmsghdr;

	msghdr_init_generic(msghdr, iov, cmsg_data);
	if (cmsg_data != NULL) {
		if (send_array_flag)
			dbgmsg("sending an array");
		else
			dbgmsg("sending a scalar");
		msghdr->msg_controllen = send_array_flag ?
		    cmsg_size : CMSG_SPACE(0);
		cmsghdr = CMSG_FIRSTHDR(msghdr);
		cmsghdr->cmsg_level = SOL_SOCKET;
		cmsghdr->cmsg_type = type;
		cmsghdr->cmsg_len = CMSG_LEN(send_array_flag ? arr_size : 0);
	} else
		msghdr->msg_controllen = 0;
}

static int
t_generic(int (*client_func)(int), int (*server_func)(int))
{
	int fd, rv, rv_client;

	switch (client_fork()) {
	case 0:
		fd = socket_create();
		if (fd < 0)
			rv = -2;
		else {
			rv = client_func(fd);
			if (socket_close(fd) < 0)
				rv = -2;
		}
		client_exit(rv);
		break;
	case 1:
		fd = socket_create();
		if (fd < 0)
			rv = -2;
		else {
			rv = server_func(fd);
			rv_client = client_wait();
			if (rv == 0 || (rv == -2 && rv_client != 0))
				rv = rv_client;
			if (socket_close(fd) < 0)
				rv = -2;
		}
		break;
	default:
		rv = -2;
	}
	return (rv);
}

static int
t_cmsgcred_client(int fd)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	void *cmsg_data;
	size_t cmsg_size;
	int rv;

	if (sync_recv() < 0)
		return (-2);

	rv = -2;

	cmsg_size = CMSG_SPACE(sizeof(struct cmsgcred));
	cmsg_data = malloc(cmsg_size);
	if (cmsg_data == NULL) {
		logmsg("malloc");
		goto done;
	}
	msghdr_init_client(&msghdr, iov, cmsg_data, cmsg_size,
	    SCM_CREDS, sizeof(struct cmsgcred));

	if (socket_connect(fd) < 0)
		goto done;

	if (message_sendn(fd, &msghdr) < 0)
		goto done;

	rv = 0;
done:
	free(cmsg_data);
	return (rv);
}

static int
t_cmsgcred_server(int fd1)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	struct cmsghdr *cmsghdr;
	void *cmsg_data;
	size_t cmsg_size;
	u_int i;
	int fd2, rv;

	if (sync_send() < 0)
		return (-2);

	fd2 = -1;
	rv = -2;

	cmsg_size = CMSG_SPACE(sizeof(struct cmsgcred));
	cmsg_data = malloc(cmsg_size);
	if (cmsg_data == NULL) {
		logmsg("malloc");
		goto done;
	}

	if (sock_type == SOCK_STREAM) {
		fd2 = socket_accept(fd1);
		if (fd2 < 0)
			goto done;
	} else
		fd2 = fd1;

	rv = -1;
	for (i = 1; i <= ipc_msg.msg_num; ++i) {
		dbgmsg("message #%u", i);

		msghdr_init_server(&msghdr, iov, cmsg_data, cmsg_size);
		if (message_recv(fd2, &msghdr) < 0) {
			rv = -2;
			break;
		}

		if (check_msghdr(&msghdr, sizeof(*cmsghdr)) < 0)
			break;

		cmsghdr = CMSG_FIRSTHDR(&msghdr);
		if (check_scm_creds_cmsgcred(cmsghdr) < 0)
			break;
	}
	if (i > ipc_msg.msg_num)
		rv = 0;
done:
	free(cmsg_data);
	if (sock_type == SOCK_STREAM && fd2 >= 0)
		if (socket_close(fd2) < 0)
			rv = -2;
	return (rv);
}

static int
t_cmsgcred(void)
{
	return (t_generic(t_cmsgcred_client, t_cmsgcred_server));
}

static int
t_sockcred_client(int type, int fd)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	int rv;

	if (sync_recv() < 0)
		return (-2);

	rv = -2;

	msghdr_init_client(&msghdr, iov, NULL, 0, 0, 0);

	if (socket_connect(fd) < 0)
		goto done;

	if (type == 2)
		if (sync_recv() < 0)
			goto done;

	if (message_sendn(fd, &msghdr) < 0)
		goto done;

	rv = 0;
done:
	return (rv);
}

static int
t_sockcred_server(int type, int fd1)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	struct cmsghdr *cmsghdr;
	void *cmsg_data;
	size_t cmsg_size;
	u_int i;
	int fd2, rv, val;

	fd2 = -1;
	rv = -2;

	cmsg_size = CMSG_SPACE(SOCKCREDSIZE(proc_cred.gid_num));
	cmsg_data = malloc(cmsg_size);
	if (cmsg_data == NULL) {
		logmsg("malloc");
		goto done;
	}

	if (type == 1) {
		dbgmsg("setting LOCAL_CREDS");
		val = 1;
		if (setsockopt(fd1, 0, LOCAL_CREDS, &val, sizeof(val)) < 0) {
			logmsg("setsockopt(LOCAL_CREDS)");
			goto done;
		}
	}

	if (sync_send() < 0)
		goto done;

	if (sock_type == SOCK_STREAM) {
		fd2 = socket_accept(fd1);
		if (fd2 < 0)
			goto done;
	} else
		fd2 = fd1;

	if (type == 2) {
		dbgmsg("setting LOCAL_CREDS");
		val = 1;
		if (setsockopt(fd2, 0, LOCAL_CREDS, &val, sizeof(val)) < 0) {
			logmsg("setsockopt(LOCAL_CREDS)");
			goto done;
		}
		if (sync_send() < 0)
			goto done;
	}

	rv = -1;
	for (i = 1; i <= ipc_msg.msg_num; ++i) {
		dbgmsg("message #%u", i);

		msghdr_init_server(&msghdr, iov, cmsg_data, cmsg_size);
		if (message_recv(fd2, &msghdr) < 0) {
			rv = -2;
			break;
		}

		if (i > 1 && sock_type == SOCK_STREAM) {
			if (check_msghdr(&msghdr, 0) < 0)
				break;
		} else {
			if (check_msghdr(&msghdr, sizeof(*cmsghdr)) < 0)
				break;

			cmsghdr = CMSG_FIRSTHDR(&msghdr);
			if (check_scm_creds_sockcred(cmsghdr) < 0)
				break;
		}
	}
	if (i > ipc_msg.msg_num)
		rv = 0;
done:
	free(cmsg_data);
	if (sock_type == SOCK_STREAM && fd2 >= 0)
		if (socket_close(fd2) < 0)
			rv = -2;
	return (rv);
}

static int
t_sockcred_1(void)
{
	u_int i;
	int fd, rv, rv_client;

	switch (client_fork()) {
	case 0:
		for (i = 1; i <= 2; ++i) {
			dbgmsg("client #%u", i);
			fd = socket_create();
			if (fd < 0)
				rv = -2;
			else {
				rv = t_sockcred_client(1, fd);
				if (socket_close(fd) < 0)
					rv = -2;
			}
			if (rv != 0)
				break;
		}
		client_exit(rv);
		break;
	case 1:
		fd = socket_create();
		if (fd < 0)
			rv = -2;
		else {
			rv = t_sockcred_server(1, fd);
			if (rv == 0)
				rv = t_sockcred_server(3, fd);
			rv_client = client_wait();
			if (rv == 0 || (rv == -2 && rv_client != 0))
				rv = rv_client;
			if (socket_close(fd) < 0)
				rv = -2;
		}
		break;
	default:
		rv = -2;
	}

	return (rv);
}

static int
t_sockcred_2_client(int fd)
{
	return (t_sockcred_client(2, fd));
}

static int
t_sockcred_2_server(int fd)
{
	return (t_sockcred_server(2, fd));
}

static int
t_sockcred_2(void)
{
	return (t_generic(t_sockcred_2_client, t_sockcred_2_server));
}

static int
t_cmsgcred_sockcred_server(int fd1)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	struct cmsghdr *cmsghdr;
	void *cmsg_data, *cmsg1_data, *cmsg2_data;
	size_t cmsg_size, cmsg1_size, cmsg2_size;
	u_int i;
	int fd2, rv, val;

	fd2 = -1;
	rv = -2;

	cmsg1_size = CMSG_SPACE(SOCKCREDSIZE(proc_cred.gid_num));
	cmsg2_size = CMSG_SPACE(sizeof(struct cmsgcred));
	cmsg1_data = malloc(cmsg1_size);
	cmsg2_data = malloc(cmsg2_size);
	if (cmsg1_data == NULL || cmsg2_data == NULL) {
		logmsg("malloc");
		goto done;
	}

	dbgmsg("setting LOCAL_CREDS");
	val = 1;
	if (setsockopt(fd1, 0, LOCAL_CREDS, &val, sizeof(val)) < 0) {
		logmsg("setsockopt(LOCAL_CREDS)");
		goto done;
	}

	if (sync_send() < 0)
		goto done;

	if (sock_type == SOCK_STREAM) {
		fd2 = socket_accept(fd1);
		if (fd2 < 0)
			goto done;
	} else
		fd2 = fd1;

	cmsg_data = cmsg1_data;
	cmsg_size = cmsg1_size;
	rv = -1;
	for (i = 1; i <= ipc_msg.msg_num; ++i) {
		dbgmsg("message #%u", i);

		msghdr_init_server(&msghdr, iov, cmsg_data, cmsg_size);
		if (message_recv(fd2, &msghdr) < 0) {
			rv = -2;
			break;
		}

		if (check_msghdr(&msghdr, sizeof(*cmsghdr)) < 0)
			break;

		cmsghdr = CMSG_FIRSTHDR(&msghdr);
		if (i == 1 || sock_type == SOCK_DGRAM) {
			if (check_scm_creds_sockcred(cmsghdr) < 0)
				break;
		} else {
			if (check_scm_creds_cmsgcred(cmsghdr) < 0)
				break;
		}

		cmsg_data = cmsg2_data;
		cmsg_size = cmsg2_size;
	}
	if (i > ipc_msg.msg_num)
		rv = 0;
done:
	free(cmsg1_data);
	free(cmsg2_data);
	if (sock_type == SOCK_STREAM && fd2 >= 0)
		if (socket_close(fd2) < 0)
			rv = -2;
	return (rv);
}

static int
t_cmsgcred_sockcred(void)
{
	return (t_generic(t_cmsgcred_client, t_cmsgcred_sockcred_server));
}

static int
t_timeval_client(int fd)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	void *cmsg_data;
	size_t cmsg_size;
	int rv;

	if (sync_recv() < 0)
		return (-2);

	rv = -2;

	cmsg_size = CMSG_SPACE(sizeof(struct timeval));
	cmsg_data = malloc(cmsg_size);
	if (cmsg_data == NULL) {
		logmsg("malloc");
		goto done;
	}
	msghdr_init_client(&msghdr, iov, cmsg_data, cmsg_size,
	    SCM_TIMESTAMP, sizeof(struct timeval));

	if (socket_connect(fd) < 0)
		goto done;

	if (message_sendn(fd, &msghdr) < 0)
		goto done;

	rv = 0;
done:
	free(cmsg_data);
	return (rv);
}

static int
t_timeval_server(int fd1)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	struct cmsghdr *cmsghdr;
	void *cmsg_data;
	size_t cmsg_size;
	u_int i;
	int fd2, rv;

	if (sync_send() < 0)
		return (-2);

	fd2 = -1;
	rv = -2;

	cmsg_size = CMSG_SPACE(sizeof(struct timeval));
	cmsg_data = malloc(cmsg_size);
	if (cmsg_data == NULL) {
		logmsg("malloc");
		goto done;
	}

	if (sock_type == SOCK_STREAM) {
		fd2 = socket_accept(fd1);
		if (fd2 < 0)
			goto done;
	} else
		fd2 = fd1;

	rv = -1;
	for (i = 1; i <= ipc_msg.msg_num; ++i) {
		dbgmsg("message #%u", i);

		msghdr_init_server(&msghdr, iov, cmsg_data, cmsg_size);
		if (message_recv(fd2, &msghdr) < 0) {
			rv = -2;
			break;
		}

		if (check_msghdr(&msghdr, sizeof(*cmsghdr)) < 0)
			break;

		cmsghdr = CMSG_FIRSTHDR(&msghdr);
		if (check_scm_timestamp(cmsghdr) < 0)
			break;
	}
	if (i > ipc_msg.msg_num)
		rv = 0;
done:
	free(cmsg_data);
	if (sock_type == SOCK_STREAM && fd2 >= 0)
		if (socket_close(fd2) < 0)
			rv = -2;
	return (rv);
}

static int
t_timeval(void)
{
	return (t_generic(t_timeval_client, t_timeval_server));
}

static int
t_bintime_client(int fd)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	void *cmsg_data;
	size_t cmsg_size;
	int rv;

	if (sync_recv() < 0)
		return (-2);

	rv = -2;

	cmsg_size = CMSG_SPACE(sizeof(struct bintime));
	cmsg_data = malloc(cmsg_size);
	if (cmsg_data == NULL) {
		logmsg("malloc");
		goto done;
	}
	msghdr_init_client(&msghdr, iov, cmsg_data, cmsg_size,
	    SCM_BINTIME, sizeof(struct bintime));

	if (socket_connect(fd) < 0)
		goto done;

	if (message_sendn(fd, &msghdr) < 0)
		goto done;

	rv = 0;
done:
	free(cmsg_data);
	return (rv);
}

static int
t_bintime_server(int fd1)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	struct cmsghdr *cmsghdr;
	void *cmsg_data;
	size_t cmsg_size;
	u_int i;
	int fd2, rv;

	if (sync_send() < 0)
		return (-2);

	fd2 = -1;
	rv = -2;

	cmsg_size = CMSG_SPACE(sizeof(struct bintime));
	cmsg_data = malloc(cmsg_size);
	if (cmsg_data == NULL) {
		logmsg("malloc");
		goto done;
	}

	if (sock_type == SOCK_STREAM) {
		fd2 = socket_accept(fd1);
		if (fd2 < 0)
			goto done;
	} else
		fd2 = fd1;

	rv = -1;
	for (i = 1; i <= ipc_msg.msg_num; ++i) {
		dbgmsg("message #%u", i);

		msghdr_init_server(&msghdr, iov, cmsg_data, cmsg_size);
		if (message_recv(fd2, &msghdr) < 0) {
			rv = -2;
			break;
		}

		if (check_msghdr(&msghdr, sizeof(*cmsghdr)) < 0)
			break;

		cmsghdr = CMSG_FIRSTHDR(&msghdr);
		if (check_scm_bintime(cmsghdr) < 0)
			break;
	}
	if (i > ipc_msg.msg_num)
		rv = 0;
done:
	free(cmsg_data);
	if (sock_type == SOCK_STREAM && fd2 >= 0)
		if (socket_close(fd2) < 0)
			rv = -2;
	return (rv);
}

static int
t_bintime(void)
{
	return (t_generic(t_bintime_client, t_bintime_server));
}

#ifndef __LP64__
static int
t_cmsg_len_client(int fd)
{
	struct msghdr msghdr;
	struct iovec iov[1];
	struct cmsghdr *cmsghdr;
	void *cmsg_data;
	size_t size, cmsg_size;
	socklen_t socklen;
	int rv;

	if (sync_recv() < 0)
		return (-2);

	rv = -2;

	cmsg_size = CMSG_SPACE(sizeof(struct cmsgcred));
	cmsg_data = malloc(cmsg_size);
	if (cmsg_data == NULL) {
		logmsg("malloc");
		goto done;
	}
	msghdr_init_client(&msghdr, iov, cmsg_data, cmsg_size,
	    SCM_CREDS, sizeof(struct cmsgcred));
	cmsghdr = CMSG_FIRSTHDR(&msghdr);

	if (socket_connect(fd) < 0)
		goto done;

	size = msghdr.msg_iov != NULL ? msghdr.msg_iov->iov_len : 0;
	rv = -1;
	for (socklen = 0; socklen < CMSG_LEN(0); ++socklen) {
		cmsghdr->cmsg_len = socklen;
		dbgmsg("send: data size %zu", size);
		dbgmsg("send: msghdr.msg_controllen %u",
		    (u_int)msghdr.msg_controllen);
		dbgmsg("send: cmsghdr.cmsg_len %u",
		    (u_int)cmsghdr->cmsg_len);
		if (sendmsg(fd, &msghdr, 0) < 0) {
			dbgmsg("sendmsg(2) failed: %s; retrying",
			    strerror(errno));
			continue;
		}
		logmsgx("sent message with cmsghdr.cmsg_len %u < %u",
		    (u_int)cmsghdr->cmsg_len, (u_int)CMSG_LEN(0));
		break;
	}
	if (socklen == CMSG_LEN(0))
		rv = 0;

	if (sync_send() < 0) {
		rv = -2;
		goto done;
	}
done:
	free(cmsg_data);
	return (rv);
}

static int
t_cmsg_len_server(int fd1)
{
	int fd2, rv;

	if (sync_send() < 0)
		return (-2);

	rv = -2;

	if (sock_type == SOCK_STREAM) {
		fd2 = socket_accept(fd1);
		if (fd2 < 0)
			goto done;
	} else
		fd2 = fd1;

	if (sync_recv() < 0)
		goto done;

	rv = 0;
done:
	if (sock_type == SOCK_STREAM && fd2 >= 0)
		if (socket_close(fd2) < 0)
			rv = -2;
	return (rv);
}

static int
t_cmsg_len(void)
{
	return (t_generic(t_cmsg_len_client, t_cmsg_len_server));
}
#endif

static int
t_peercred_client(int fd)
{
	struct xucred xucred;
	socklen_t len;

	if (sync_recv() < 0)
		return (-1);

	if (socket_connect(fd) < 0)
		return (-1);

	len = sizeof(xucred);
	if (getsockopt(fd, 0, LOCAL_PEERCRED, &xucred, &len) < 0) {
		logmsg("getsockopt(LOCAL_PEERCRED)");
		return (-1);
	}

	if (check_xucred(&xucred, len) < 0)
		return (-1);

	return (0);
}

static int
t_peercred_server(int fd1)
{
	struct xucred xucred;
	socklen_t len;
	int fd2, rv;

	if (sync_send() < 0)
		return (-2);

	fd2 = socket_accept(fd1);
	if (fd2 < 0)
		return (-2);

	len = sizeof(xucred);
	if (getsockopt(fd2, 0, LOCAL_PEERCRED, &xucred, &len) < 0) {
		logmsg("getsockopt(LOCAL_PEERCRED)");
		rv = -2;
		goto done;
	}

	if (check_xucred(&xucred, len) < 0) {
		rv = -1;
		goto done;
	}

	rv = 0;
done:
	if (socket_close(fd2) < 0)
		rv = -2;
	return (rv);
}

static int
t_peercred(void)
{
	return (t_generic(t_peercred_client, t_peercred_server));
}
