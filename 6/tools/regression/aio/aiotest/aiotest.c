/*-
 * Copyright (c) 2004 Robert N. M. Watson
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

/*
 * Regression test to do some very basic AIO exercising on several types of
 * file descriptors.  Currently, the tests consist of initializing a fixed
 * size buffer with pseudo-random data, writing it to one fd using AIO, then
 * reading it from a second descriptor using AIO.  For some targets, the same
 * fd is used for write and read (i.e., file, md device), but for others the
 * operation is performed on a peer (pty, socket, fifo, etc).  A timeout is
 * initiated to detect undo blocking.  This test does not attempt to exercise
 * error cases or more subtle asynchronous behavior, just make sure that the
 * basic operations work on some basic object types.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mdioctl.h>

#include <aio.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define	PATH_TEMPLATE	"/tmp/aio.XXXXXXXXXX"

/*
 * GLOBAL_MAX sets the largest usable buffer size to be read and written, as
 * it sizes ac_buffer in the aio_context structure.  It is also the default
 * size for file I/O.  For other types, we use smaller blocks or we risk
 * blocking (and we run in a single process/thread so that would be bad).
 */
#define	GLOBAL_MAX	16384

#define	BUFFER_MAX	GLOBAL_MAX
struct aio_context {
	const char	*ac_test;
	int		 ac_read_fd, ac_write_fd;
	long		 ac_seed;
	char		 ac_buffer[GLOBAL_MAX];
	int		 ac_buflen;
	int		 ac_seconds;
	void		 (*ac_cleanup)(void *arg);
	void		*ac_cleanup_arg;
};

static int	aio_timedout;
static int	aio_notpresent;

/*
 * Attempt to provide a cleaner failure mode in the event AIO support is not
 * present by catching and reporting SIGSYS.
 */
static void
aio_sigsys(int sig)
{

	aio_notpresent = 1;
}

static void
aio_sigsys_setup(void)
{

	if (signal(SIGSYS, aio_sigsys) == SIG_ERR)
		errx(-1, "FAIL: signal(SIGSYS): %s", strerror(errno));
}

/*
 * Each test run specifies a timeout in seconds.  Use the somewhat obsoleted
 * signal(3) and alarm(3) APIs to set this up.
 */
static void
aio_timeout_signal(int sig)
{

	aio_timedout = 1;
}

static void
aio_timeout_start(const char *string1, const char *string2, int seconds)
{

	aio_timedout = 0;
	if (signal(SIGALRM, aio_timeout_signal) == SIG_ERR)
		errx(-1, "FAIL: %s: %s: aio_timeout_set: signal(SIGALRM): %s",
		    string1, string2, strerror(errno));
	alarm(seconds);
}

static void
aio_timeout_stop(const char *string1, const char *string2)
{

	if (signal(SIGALRM, NULL) == SIG_ERR)
		errx(-1, "FAIL: %s: %s: aio_timeout_stop: signal(NULL): %s",
		    string1, string2, strerror(errno));
	alarm(0);
}

/*
 * Fill a buffer given a seed that can be fed into srandom() to initialize
 * the PRNG in a repeatable manner.
 */
static void
aio_fill_buffer(char *buffer, int len, long seed)
{
	char ch;
	int i;

	srandom(seed);
	for (i = 0; i < len; i++) {
		ch = random() & 0xff;
		buffer[i] = ch;
	}
}

/*
 * Test that a buffer matches a given seed.  See aio_fill_buffer().  Return
 * (1) on a match, (0) on a mismatch.
 */
static int
aio_test_buffer(char *buffer, int len, long seed)
{
	char ch;
	int i;

	srandom(seed);
	for (i = 0; i < len; i++) {
		ch = random() & 0xff;
		if (buffer[i] != ch)
			return (0);
	}
	return (1);
}

/*
 * Initialize a testing context given the file descriptors provided by the
 * test setup.
 */
static void
aio_context_init(struct aio_context *ac, const char *test, int read_fd,
    int write_fd, int buflen, int seconds, void (*cleanup)(void *),
    void *cleanup_arg)
{

	if (buflen > BUFFER_MAX)
		errx(-1, "FAIL: %s: aio_context_init: buffer too large",
		    test);
	bzero(ac, sizeof(*ac));
	ac->ac_test = test;
	ac->ac_read_fd = read_fd;
	ac->ac_write_fd = write_fd;
	ac->ac_buflen = buflen;
	srandomdev();
	ac->ac_seed = random();
	aio_fill_buffer(ac->ac_buffer, buflen, ac->ac_seed);
	if (aio_test_buffer(ac->ac_buffer, buflen, ac->ac_seed) == 0)
		errx(-1, "%s: aio_context_init: aio_test_buffer: internal "
		    "error", test);
	ac->ac_seconds = seconds;
	ac->ac_cleanup = cleanup;
	ac->ac_cleanup_arg = cleanup_arg;
}

/*
 * Each tester can register a callback to clean up in the event the test
 * fails.  Preserve the value of errno so that subsequent calls to errx()
 * work properly.
 */
static void
aio_cleanup(struct aio_context *ac)
{
	int error;

	if (ac->ac_cleanup == NULL)
		return;
	error = errno;
	(ac->ac_cleanup)(ac->ac_cleanup_arg);
	errno = error;
}

/*
 * Perform a simple write test of our initialized data buffer to the provided
 * file descriptor.
 */
static void
aio_write_test(struct aio_context *ac)
{
	struct aiocb aio, *aiop;
	ssize_t len;
	int error;

	bzero(&aio, sizeof(aio));
	aio.aio_buf = ac->ac_buffer;
	aio.aio_nbytes = ac->ac_buflen;
	aio.aio_fildes = ac->ac_write_fd;
	aio.aio_offset = 0;

	aio_timeout_start(ac->ac_test, "aio_write_test", ac->ac_seconds);

	if (aio_write(&aio) < 0) {
		if (errno == EINTR) {
			if (aio_notpresent)
				errno = EOPNOTSUPP;
			if (aio_timedout) {
				aio_cleanup(ac);
				errx(-1, "FAIL: %s: aio_write_test: "
				    "aio_write: timed out", ac->ac_test);
			}
		}
		aio_cleanup(ac);
		errx(-1, "FAIL: %s: aio_write_test: aio_write: %s",
		    ac->ac_test, strerror(errno));
	}

	len = aio_waitcomplete(&aiop, NULL);
	if (len < 0) {
		if (errno == EINTR) {
			if (aio_notpresent)
				errno = EOPNOTSUPP;
			if (aio_timedout) {
				aio_cleanup(ac);
				errx(-1, "FAIL: %s: aio_write_test: "
				    "aio_waitcomplete: timed out",
				    ac->ac_test);
			}
		}
		aio_cleanup(ac);
		errx(-1, "FAIL: %s: aio_write_test: aio_waitcomplete: %s",
		    ac->ac_test, strerror(errno));
	}

	aio_timeout_stop(ac->ac_test, "aio_write_test");

	if (len != ac->ac_buflen) {
		aio_cleanup(ac);
		errx(-1, "FAIL: %s: aio_write_test: aio_waitcomplete: short "
		    "write (%d)", ac->ac_test, len);
	}
}

/*
 * Perform a simple read test of our initialized data buffer from the
 * provided file descriptor.
 */
static void
aio_read_test(struct aio_context *ac)
{
	struct aiocb aio, *aiop;
	ssize_t len;

	bzero(ac->ac_buffer, ac->ac_buflen);
	bzero(&aio, sizeof(aio));
	aio.aio_buf = ac->ac_buffer;
	aio.aio_nbytes = ac->ac_buflen;
	aio.aio_fildes = ac->ac_read_fd;
	aio.aio_offset = 0;

	aio_timeout_start(ac->ac_test, "aio_read_test", ac->ac_seconds);

	if (aio_read(&aio) < 0) {
		if (errno == EINTR) {
			if (aio_notpresent)
				errno = EOPNOTSUPP;
			if (aio_timedout) {
				aio_cleanup(ac);
				errx(-1, "FAIL: %s: aio_read_test: "
				    "aio_read: timed out", ac->ac_test);
			}
		}
		aio_cleanup(ac);
		errx(-1, "FAIL: %s: aio_read_test: aio_read %s", ac->ac_test,
		    strerror(errno));
	}

	len = aio_waitcomplete(&aiop, NULL);
	if (len < 0) {
		if (errno == EINTR) {
			if (aio_notpresent)
				errno = EOPNOTSUPP;
			if (aio_timedout) {
				aio_cleanup(ac);
				errx(-1, "FAIL: %s: aio_read_test: "
				    "aio_waitcomplete: timed out",
				    ac->ac_test);
			}
		}
		aio_cleanup(ac);
		errx(-1, "FAIL: %s: aio_read_test: aio_waitcomplete: %s",
		    ac->ac_test, strerror(errno));
	}

	aio_timeout_stop(ac->ac_test, "aio_read_test");

	if (len != ac->ac_buflen) {
		aio_cleanup(ac);
		errx(-1, "FAIL: %s: aio_read_test: aio_waitcomplete: short "
		    "read (%d)", ac->ac_test, len);
	}

	if (aio_test_buffer(ac->ac_buffer, ac->ac_buflen, ac->ac_seed) == 0) {
		aio_cleanup(ac);
		errx(-1, "FAIL: %s: aio_read_test: buffer mismatch",
		    ac->ac_test);
	}
}

/*
 * Series of type-specific tests for AIO.  For now, we just make sure we can
 * issue a write and then a read to each type.  We assume that once a write
 * is issued, a read can follow.
 */

/*
 * Test with a classic file.  Assumes we can create a moderate size temporary
 * file.
 */
struct aio_file_arg {
	int	 afa_fd;
	char	*afa_pathname;
};

static void
aio_file_cleanup(void *arg)
{
	struct aio_file_arg *afa;

	afa = arg;
	close(afa->afa_fd);
	unlink(afa->afa_pathname);
}

#define	FILE_LEN	GLOBAL_MAX
#define	FILE_TIMEOUT	30
static int
aio_file_test(void)
{
	char pathname[PATH_MAX];
	struct aio_file_arg arg;
	struct aio_context ac;
	int fd;

	strcpy(pathname, PATH_TEMPLATE);
	fd = mkstemp(pathname);
	if (fd == -1)
		errx(-1, "FAIL: aio_file_test: mkstemp: %s",
		    strerror(errno));

	arg.afa_fd = fd;
	arg.afa_pathname = pathname;

	aio_context_init(&ac, "aio_file_test", fd, fd, FILE_LEN,
	    FILE_TIMEOUT, aio_file_cleanup, &arg);
	aio_write_test(&ac);
	aio_read_test(&ac);

	aio_file_cleanup(&arg);
	
	fprintf(stderr, "PASS: aio_file_test\n");
}

struct aio_fifo_arg {
	int	 afa_read_fd;
	int	 afa_write_fd;
	char	*afa_pathname;
};

static void
aio_fifo_cleanup(void *arg)
{
	struct aio_fifo_arg *afa;

	afa = arg;
	if (afa->afa_read_fd != -1)
		close(afa->afa_read_fd);
	if (afa->afa_write_fd != -1)
		close(afa->afa_write_fd);
	unlink(afa->afa_pathname);
}

#define	FIFO_LEN	256
#define	FIFO_TIMEOUT	30
static int
aio_fifo_test(void)
{
	int error, read_fd = -1, write_fd = -1;
	struct aio_fifo_arg arg;
	char pathname[PATH_MAX];
	struct aio_context ac;

	/*
	 * In theory, mktemp() can return a name that is then collided with.
	 * Because this is a regression test, we treat that as a test failure
	 * rather than retrying.
	 */
	strcpy(pathname, PATH_TEMPLATE);
	mktemp(pathname);
	if (mkfifo(pathname, 0600) == -1)
		errx(-1, "FAIL: aio_fifo_test: mkfifo: %s", strerror(errno));
	arg.afa_pathname = pathname;
	arg.afa_read_fd = -1;
	arg.afa_write_fd = -1;

	read_fd = open(pathname, O_RDONLY | O_NONBLOCK);
	if (read_fd == -1) {
		error = errno;
		aio_fifo_cleanup(&arg);
		errno = error;
		errx(-1, "FAIL: aio_fifo_test: read_fd open: %s",
		    strerror(errno));
	}
	arg.afa_read_fd = read_fd;

	write_fd = open(pathname, O_WRONLY);
	if (write_fd == -1) {
		error = errno;
		aio_fifo_cleanup(&arg);
		errno = error;
		errx(-1, "FAIL: aio_fifo_test: write_fd open: %s",
		    strerror(errno));
	}
	arg.afa_write_fd = write_fd;

	aio_context_init(&ac, "aio_fifo_test", read_fd, write_fd, FIFO_LEN,
	    FIFO_TIMEOUT, aio_fifo_cleanup, &arg);
	aio_write_test(&ac);
	aio_read_test(&ac);

	aio_fifo_cleanup(&arg);

	fprintf(stderr, "PASS: aio_fifo_test\n");
}

struct aio_unix_socketpair_arg {
	int	asa_sockets[2];
};

static void
aio_unix_socketpair_cleanup(void *arg)
{
	struct aio_unix_socketpair_arg *asa;

	asa = arg;
	close(asa->asa_sockets[0]);
	close(asa->asa_sockets[1]);
}

#define	UNIX_SOCKETPAIR_LEN	256
#define	UNIX_SOCKETPAIR_TIMEOUT	30
static int
aio_unix_socketpair_test(void)
{
	struct aio_unix_socketpair_arg arg;
	struct aio_context ac;
	int sockets[2];

	if (socketpair(PF_UNIX, SOCK_STREAM, 0, sockets) < 0)
		errx(-1, "FAIL: aio_socketpair_test: socketpair: %s",
		    strerror(errno));

	arg.asa_sockets[0] = sockets[0];
	arg.asa_sockets[1] = sockets[1];
	aio_context_init(&ac, "aio_unix_socketpair_test", sockets[0],
	    sockets[1], UNIX_SOCKETPAIR_LEN, UNIX_SOCKETPAIR_TIMEOUT,
	    aio_unix_socketpair_cleanup, &arg);
	aio_write_test(&ac);
	aio_read_test(&ac);

	aio_unix_socketpair_cleanup(&arg);

	fprintf(stderr, "PASS: aio_unix_socketpair_test\n");
}

struct aio_pty_arg {
	int	apa_read_fd;
	int	apa_write_fd;
};

static void
aio_pty_cleanup(void *arg)
{
	struct aio_pty_arg *apa;

	close(apa->apa_read_fd);
	close(apa->apa_write_fd);
};

#define	PTY_LEN		256
#define	PTY_TIMEOUT	30
static int
aio_pty_test(void)
{
	struct aio_pty_arg arg;
	struct aio_context ac;
	int read_fd, write_fd;
	struct termios ts;
	int error;

	if (openpty(&read_fd, &write_fd, NULL, NULL, NULL) < 0)
		errx(-1, "FAIL: aio_pty_test: openpty: %s", strerror(errno));

	arg.apa_read_fd = read_fd;
	arg.apa_write_fd = write_fd;

	if (tcgetattr(write_fd, &ts) < 0) {
		error = errno;
		aio_pty_cleanup(&arg);
		errno = error;
		errx(-1, "FAIL: aio_pty_test: tcgetattr: %s",
		    strerror(errno));
	}
	cfmakeraw(&ts);
	if (tcsetattr(write_fd, TCSANOW, &ts) < 0) {
		error = errno;
		aio_pty_cleanup(&arg);
		errno = error;
		errx(-1, "FAIL: aio_pty_test: tcsetattr: %s",
		    strerror(errno));
	}

	aio_context_init(&ac, "aio_pty_test", read_fd, write_fd, PTY_LEN,
	    PTY_TIMEOUT, aio_pty_cleanup, &arg);
	aio_write_test(&ac);
	aio_read_test(&ac);

	aio_pty_cleanup(&arg);

	fprintf(stderr, "PASS: aio_pty_test\n");
}

static void
aio_pipe_cleanup(void *arg)
{
	int *pipes = arg;

	close(pipes[0]);
	close(pipes[1]);
}

#define	PIPE_LEN	256
#define	PIPE_TIMEOUT	30
static int
aio_pipe_test(void)
{	
	struct aio_context ac;
	int pipes[2];

	if (pipe(pipes) < 0)
		errx(-1, "FAIL: aio_pipe_test: pipe: %s", strerror(errno));

	aio_context_init(&ac, "aio_file_test", pipes[0], pipes[1], PIPE_LEN,
	    PIPE_TIMEOUT, aio_pipe_cleanup, pipes);
	aio_write_test(&ac);
	aio_read_test(&ac);

	aio_pipe_cleanup(pipes);

	fprintf(stderr, "PASS: aio_pipe_test\n");
}

struct aio_md_arg {
	int	ama_mdctl_fd;
	int	ama_unit;
	int	ama_fd;
};

static void
aio_md_cleanup(void *arg)
{
	struct aio_md_arg *ama;
	struct md_ioctl mdio;
	int error;

	ama = arg;

	if (ama->ama_fd != -1)
		close(ama->ama_fd);

	if (ama->ama_unit != -1) {
		bzero(&mdio, sizeof(mdio));
		mdio.md_version = MDIOVERSION;
		mdio.md_unit = ama->ama_unit;
		if (ioctl(ama->ama_mdctl_fd, MDIOCDETACH, &mdio) < 0) {
			error = errno;
			close(ama->ama_mdctl_fd);
			errno = error;
			warnx("FAIL: aio_md_test: MDIOCDETACH: %s",
			    strerror(errno));
		}
	}

	close(ama->ama_mdctl_fd);
}

#define	MD_LEN		GLOBAL_MAX
#define	MD_TIMEOUT	30
static int
aio_md_test(void)
{
	int error, fd, i, mdctl_fd, unit;
	char pathname[PATH_MAX];
	struct aio_md_arg arg;
	struct aio_context ac;
	struct md_ioctl mdio;

	mdctl_fd = open("/dev/" MDCTL_NAME, O_RDWR, 0);
	if (mdctl_fd < 0)
		errx(-1, "FAIL: aio_md_test: open(/dev/%s): %s", MDCTL_NAME,
		    strerror(errno));

	bzero(&mdio, sizeof(mdio));
	mdio.md_version = MDIOVERSION;
	mdio.md_type = MD_MALLOC;
	mdio.md_options = MD_AUTOUNIT | MD_COMPRESS;
	mdio.md_mediasize = GLOBAL_MAX;
	mdio.md_sectorsize = 512;

	arg.ama_mdctl_fd = mdctl_fd;
	arg.ama_unit = -1;
	arg.ama_fd = -1;
	if (ioctl(mdctl_fd, MDIOCATTACH, &mdio) < 0) {
		error = errno;
		aio_md_cleanup(&arg);
		errno = error;
		errx(-1, "FAIL: aio_md_test: MDIOCATTACH: %s",
		    strerror(errno));
	}

	arg.ama_unit = unit = mdio.md_unit;
	snprintf(pathname, PATH_MAX, "/dev/md%d", unit);
	fd = open(pathname, O_RDWR);
	if (fd < 0) {
		error = errno;
		aio_md_cleanup(&arg);
		errno = error;
		errx(-1, "FAIL: aio_md_test: open(%s): %s", pathname,
		    strerror(errno));
	}
	arg.ama_fd = fd;

	aio_context_init(&ac, "aio_md_test", fd, fd, MD_LEN, MD_TIMEOUT,
	    aio_md_cleanup, &arg);
	aio_write_test(&ac);
	aio_read_test(&ac);

	aio_md_cleanup(&arg);

	fprintf(stderr, "PASS: aio_md_test\n");
}

int
main(int argc, char *argv[])
{

	aio_sigsys_setup();
	aio_file_test();
	aio_fifo_test();
	aio_unix_socketpair_test();
	aio_pty_test();
	aio_pipe_test();
	if (geteuid() == 0)
		aio_md_test();
	else
		fprintf(stderr, "WARNING: aio_md_test: skipped as euid "
		    "!= 0\n");
}
