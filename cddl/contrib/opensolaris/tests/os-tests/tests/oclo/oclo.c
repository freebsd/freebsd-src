/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2024 Oxide Computer Company
 */

/*
 * Verify the behavior of the various O_CLOFORK and O_CLOEXEC variants. In
 * particular getting this via:
 *
 *  - open(2): O_CLOFORK/O_CLOEXEC
 *  - fcntl(2): F_SETFD FD_CLOFORK/FD_CLOEXEC
 *  - fcntl(2): F_DUPFD_CLOFORK/F_DUPFD_CLOEXEC
 *  - fcntl(2): F_DUP2FD_CLOFORK/F_DUP2FD_CLOEXEC
 *  - dup2(3C)
 *  - dup3(3C): argument translation
 *  - pipe2(2)
 *  - socket(2): SOCK_CLOEXEC/SOCK_CLOFORK
 *  - accept(2): flags on the listen socket aren't inherited on accept
 *  - socketpair(3SOCKET)
 *  - accept4(2): SOCK_CLOEXEC/SOCK_CLOFORK
 *  - recvmsg(2): SCM_RIGHTS MSG_CMSG_CLOFORK/MSG_CMSG_CLOEXEC
 *
 * The test is designed such that we have an array of functions that are used to
 * create file descriptors with different rules. This is found in the
 * oclo_create array. Each file descriptor that is created is then registered
 * with information about what is expected about it. A given creation function
 * can create more than one file descriptor; however, our expectation is that
 * every file descriptor is accounted for (ignoring stdin, stdout, and stderr).
 *
 * We pass a record of each file descriptor that was recorded to a verification
 * program that will verify everything is correctly honored after an exec.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sysmacros.h>
#include <sys/fork.h>
#include <wait.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <sys/socket.h>

/*
 * Verification program name.
 */
#define	OCLO_VERIFY	"ocloexec_verify"

/*
 * This structure represents a table of ways we expect to create file
 * descriptors that should have the resulting flags set when done. The table is
 * ordered and subsequent iterations are allowed to assume that the ones that
 * have gone ahead of them have run and are therefore allowed to access them.
 * The create function is expected to return the created fd.
 */
typedef struct clo_create clo_create_t;
struct clo_create {
	const char *clo_desc;
	int clo_flags;
	void (*clo_func)(const clo_create_t *);
};

/*
 * This is our run-time data. We expect all file descriptors to be registered by
 * our calling functions through oclo_record().
 */
typedef struct clo_rtdata {
	const clo_create_t *crt_data;
	size_t crt_idx;
	int crt_fd;
	int crt_flags;
	const char *crt_desc;
} clo_rtdata_t;

static clo_rtdata_t *oclo_rtdata;
size_t oclo_rtdata_nents = 0;
size_t oclo_rtdata_next = 0;
static int oclo_nextfd = STDERR_FILENO + 1;

static bool
oclo_flags_match(const clo_rtdata_t *rt, bool child)
{
	const char *pass = child ? "post-fork" : "pre-fork";
	bool fail = child && (rt->crt_flags & FD_CLOFORK) != 0;
	int flags = fcntl(rt->crt_fd, F_GETFD, NULL);

	if (flags < 0) {
		int e = errno;

		if (fail) {
			if (e == EBADF) {
				(void) printf("TEST PASSED: %s (%s): fd %d: "
				    "correctly closed\n",
				    rt->crt_data->clo_desc, pass, rt->crt_fd);
				return (true);
			}

			warn("TEST FAILED: %s (%s): fd %d: expected fcntl to "
			    "fail with EBADF, but found %s",
			    rt->crt_data->clo_desc, pass, rt->crt_fd,
			    strerrorname_np(e));
			return (false);
		}

		warnx("TEST FAILED: %s (%s): fd %d: fcntl(F_GETFD) "
		    "unexpectedly failed", rt->crt_data->clo_desc, pass,
		    rt->crt_fd);
		return (false);
	}

	if (fail) {
		warnx("TEST FAILED: %s (%s): fd %d: received flags %d, but "
		    "expected to fail based on flags %d",
		    rt->crt_data->clo_desc, pass, rt->crt_fd, flags,
		    rt->crt_fd);
		return (false);
	}

	if (flags != rt->crt_flags) {
		warnx("TEST FAILED: %s (%s): fd %d: discovered flags 0x%x do "
		    "not match expected flags 0x%x", rt->crt_data->clo_desc,
		    pass, rt->crt_fd, flags, rt->crt_fd);
		return (false);
	}

	(void) printf("TEST PASSED: %s (%s): fd %d discovered flags match "
	    "(0x%x)\n", rt->crt_data->clo_desc, pass, rt->crt_fd, flags);
	return (true);
}


static void
oclo_record(const clo_create_t *c, int fd, int exp_flags, const char *desc)
{
	if (oclo_rtdata_next == oclo_rtdata_nents) {
		size_t newrt = oclo_rtdata_nents + 8;
		clo_rtdata_t *rt;
		rt = recallocarray(oclo_rtdata, oclo_rtdata_nents, newrt,
		    sizeof (clo_rtdata_t));
		if (rt == NULL) {
			err(EXIT_FAILURE, "TEST_FAILED: internal error "
			    "expanding fd records to %zu entries", newrt);
		}

		oclo_rtdata_nents = newrt;
		oclo_rtdata = rt;
	}

	if (fd != oclo_nextfd) {
		errx(EXIT_FAILURE, "TEST FAILED: internal test error: expected "
		    "to record next fd %d, given %d", oclo_nextfd, fd);
	}

	oclo_rtdata[oclo_rtdata_next].crt_data = c;
	oclo_rtdata[oclo_rtdata_next].crt_fd = fd;
	oclo_rtdata[oclo_rtdata_next].crt_flags = exp_flags;
	oclo_rtdata[oclo_rtdata_next].crt_desc = desc;

	/*
	 * Matching errors at this phase are fatal as it means we screwed up the
	 * program pretty badly.
	 */
	if (!oclo_flags_match(&oclo_rtdata[oclo_rtdata_next], false)) {
		exit(EXIT_FAILURE);
	}

	oclo_rtdata_next++;
	oclo_nextfd++;
}

static int
oclo_file(const clo_create_t *c)
{
	int flags = O_RDWR, fd;

	if ((c->clo_flags & FD_CLOEXEC) != 0)
		flags |= O_CLOEXEC;
	if ((c->clo_flags & FD_CLOFORK) != 0)
		flags |= O_CLOFORK;
	fd = open("/dev/null", flags);
	if (fd < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to open /dev/null",
		    c->clo_desc);
	}

	return (fd);
}

static void
oclo_open(const clo_create_t *c)
{
	oclo_record(c, oclo_file(c), c->clo_flags, NULL);
}

static void
oclo_setfd_common(const clo_create_t *c, int targ_flags)
{
	int fd = oclo_file(c);
	if (fcntl(fd, F_SETFD, targ_flags) < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: F_SETFD failed to set "
		    "flags to %d", c->clo_desc, targ_flags);
	}

	oclo_record(c, fd, targ_flags, NULL);
}

static void
oclo_setfd_none(const clo_create_t *c)
{
	oclo_setfd_common(c, 0);
}

static void
oclo_setfd_exec(const clo_create_t *c)
{
	oclo_setfd_common(c, FD_CLOEXEC);
}

static void
oclo_setfd_fork(const clo_create_t *c)
{
	oclo_setfd_common(c, FD_CLOFORK);
}

static void
oclo_setfd_both(const clo_create_t *c)
{
	oclo_setfd_common(c, FD_CLOFORK | FD_CLOEXEC);
}

/*
 * Open an fd with flags in a certain form and then use one of the F_DUPFD or
 * F_DUP2FD variants and ensure that flags are properly propagated as expected.
 */
static void
oclo_fdup_common(const clo_create_t *c, int targ_flags, int cmd)
{
	int dup, fd;

	fd = oclo_file(c);
	oclo_record(c, fd, c->clo_flags, "base");
	switch (cmd) {
	case F_DUPFD:
	case F_DUPFD_CLOEXEC:
	case F_DUPFD_CLOFORK:
		dup = fcntl(fd, cmd, fd);
		break;
	case F_DUP2FD:
	case F_DUP2FD_CLOEXEC:
	case F_DUP2FD_CLOFORK:
		dup = fcntl(fd, cmd, fd + 1);
		break;
	case F_DUP3FD:
		dup = fcntl(fd, cmd, fd + 1, targ_flags);
		break;
	default:
		errx(EXIT_FAILURE, "TEST FAILURE: %s: internal error: "
		    "unexpected fcntl cmd: 0x%x", c->clo_desc, cmd);
	}

	if (dup < 0) {
		err(EXIT_FAILURE, "TEST FAILURE: %s: failed to dup fd with "
		    "fcntl command 0x%x", c->clo_desc, cmd);
	}

	oclo_record(c, dup, targ_flags, "dup");
}

static void
oclo_fdupfd(const clo_create_t *c)
{
	oclo_fdup_common(c, 0, F_DUPFD);
}

static void
oclo_fdupfd_fork(const clo_create_t *c)
{
	oclo_fdup_common(c, FD_CLOFORK, F_DUPFD_CLOFORK);
}

static void
oclo_fdupfd_exec(const clo_create_t *c)
{
	oclo_fdup_common(c, FD_CLOEXEC, F_DUPFD_CLOEXEC);
}

static void
oclo_fdup2fd(const clo_create_t *c)
{
	oclo_fdup_common(c, 0, F_DUP2FD);
}

static void
oclo_fdup2fd_fork(const clo_create_t *c)
{
	oclo_fdup_common(c, FD_CLOFORK, F_DUP2FD_CLOFORK);
}

static void
oclo_fdup2fd_exec(const clo_create_t *c)
{
	oclo_fdup_common(c, FD_CLOEXEC, F_DUP2FD_CLOEXEC);
}

static void
oclo_fdup3fd_none(const clo_create_t *c)
{
	oclo_fdup_common(c, 0, F_DUP3FD);
}

static void
oclo_fdup3fd_exec(const clo_create_t *c)
{
	oclo_fdup_common(c, FD_CLOEXEC, F_DUP3FD);
}

static void
oclo_fdup3fd_fork(const clo_create_t *c)
{
	oclo_fdup_common(c, FD_CLOFORK, F_DUP3FD);
}

static void
oclo_fdup3fd_both(const clo_create_t *c)
{
	oclo_fdup_common(c, FD_CLOEXEC | FD_CLOFORK, F_DUP3FD);
}

static void
oclo_dup_common(const clo_create_t *c, int targ_flags, bool v3)
{
	int dup, fd;
	fd = oclo_file(c);
	oclo_record(c, fd, c->clo_flags, "base");
	if (v3) {
		int dflags = 0;
		if ((targ_flags & FD_CLOEXEC) != 0)
			dflags |= O_CLOEXEC;
		if ((targ_flags & FD_CLOFORK) != 0)
			dflags |= O_CLOFORK;
		dup = dup3(fd, fd + 1, dflags);
	} else {
		dup = dup2(fd, fd + 1);
	}

	oclo_record(c, dup, targ_flags, "dup");
}

static void
oclo_dup2(const clo_create_t *c)
{
	oclo_dup_common(c, 0, false);
}

static void
oclo_dup3_none(const clo_create_t *c)
{
	oclo_dup_common(c, 0, true);
}

static void
oclo_dup3_exec(const clo_create_t *c)
{
	oclo_dup_common(c, FD_CLOEXEC, true);
}

static void
oclo_dup3_fork(const clo_create_t *c)
{
	oclo_dup_common(c, FD_CLOFORK, true);
}

static void
oclo_dup3_both(const clo_create_t *c)
{
	oclo_dup_common(c, FD_CLOEXEC | FD_CLOFORK, true);
}

static void
oclo_pipe(const clo_create_t *c)
{
	int flags = 0, fds[2];

	if ((c->clo_flags & FD_CLOEXEC) != 0)
		flags |= O_CLOEXEC;
	if ((c->clo_flags & FD_CLOFORK) != 0)
		flags |= O_CLOFORK;

	if (pipe2(fds, flags) < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: pipe2() with flags %d "
		    "failed", c->clo_desc, flags);
	}

	oclo_record(c, fds[0], c->clo_flags, "pipe[0]");
	oclo_record(c, fds[1], c->clo_flags, "pipe[1]");
}

static void
oclo_socket(const clo_create_t *c)
{
	int type = SOCK_DGRAM, fd;

	if ((c->clo_flags & FD_CLOEXEC) != 0)
		type |= SOCK_CLOEXEC;
	if ((c->clo_flags & FD_CLOFORK) != 0)
		type |= SOCK_CLOFORK;
	fd = socket(PF_INET, type, 0);
	if (fd < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to create socket "
		    "with flags: 0x%x\n", c->clo_desc, c->clo_flags);
	}

	oclo_record(c, fd, c->clo_flags, NULL);
}

static void
oclo_accept_common(const clo_create_t *c, int targ_flags, bool a4)
{
	int lsock, csock, asock;
	int ltype = SOCK_STREAM, atype = 0;
	struct sockaddr_in in;
	socklen_t slen;

	if ((c->clo_flags & FD_CLOEXEC) != 0)
		ltype |= SOCK_CLOEXEC;
	if ((c->clo_flags & FD_CLOFORK) != 0)
		ltype |= SOCK_CLOFORK;

	if ((targ_flags & FD_CLOEXEC) != 0)
		atype |= SOCK_CLOEXEC;
	if ((targ_flags & FD_CLOFORK) != 0)
		atype |= SOCK_CLOFORK;

	lsock = socket(PF_INET, ltype, 0);
	if (lsock < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to create listen "
		    "socket with flags: 0x%x\n", c->clo_desc, c->clo_flags);
	}

	oclo_record(c, lsock, c->clo_flags, "listen");
	(void) memset(&in, 0, sizeof (in));
	in.sin_family = AF_INET;
	in.sin_port = 0;
	in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(lsock, (struct sockaddr *)&in, sizeof (in)) != 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to bind socket",
		    c->clo_desc);
	}

	slen = sizeof (struct sockaddr_in);
	if (getsockname(lsock, (struct sockaddr *)&in, &slen) != 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to discover bound "
		    "socket address", c->clo_desc);
	}

	if (listen(lsock, 5) < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to listen on socket",
		    c->clo_desc);
	}

	csock = socket(PF_INET, SOCK_STREAM, 0);
	if (csock < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to create client "
		    "socket", c->clo_desc);
	}
	oclo_record(c, csock, 0, "connect");

	if (connect(csock, (struct sockaddr *)&in, sizeof (in)) != 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to connect to "
		    "server socket", c->clo_desc);
	}

	if (a4) {
		asock = accept4(lsock, NULL, NULL, atype);
	} else {
		asock = accept(lsock, NULL, NULL);
	}
	if (asock < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to accept client "
		    "connection", c->clo_desc);
	}
	oclo_record(c, asock, targ_flags, "accept");
}

static void
oclo_accept(const clo_create_t *c)
{
	oclo_accept_common(c, 0, false);
}

static void
oclo_accept4_none(const clo_create_t *c)
{
	oclo_accept_common(c, 0, true);
}

static void
oclo_accept4_fork(const clo_create_t *c)
{
	oclo_accept_common(c, FD_CLOFORK, true);
}

static void
oclo_accept4_exec(const clo_create_t *c)
{
	oclo_accept_common(c, FD_CLOEXEC, true);
}

static void
oclo_accept4_both(const clo_create_t *c)
{
	oclo_accept_common(c, FD_CLOEXEC | FD_CLOFORK, true);
}

/*
 * Go through the process of sending ourselves a file descriptor.
 */
static void
oclo_rights_common(const clo_create_t *c, int targ_flags)
{
	int pair[2], type = SOCK_DGRAM, sflags = 0;
	int tosend = oclo_file(c), recvfd;
	uint32_t data = 0x7777;
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cm;

	if ((c->clo_flags & FD_CLOEXEC) != 0)
		type |= SOCK_CLOEXEC;
	if ((c->clo_flags & FD_CLOFORK) != 0)
		type |= SOCK_CLOFORK;

	if (socketpair(PF_UNIX, type, 0, pair) < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to create socket "
		    "pair", c->clo_desc);
	}

	oclo_record(c, tosend, c->clo_flags, "send fd");
	oclo_record(c, pair[0], c->clo_flags, "pair[0]");
	oclo_record(c, pair[1], c->clo_flags, "pair[1]");

	iov.iov_base = (void *)&data;
	iov.iov_len = sizeof (data);

	(void) memset(&msg, 0, sizeof (msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_controllen = CMSG_SPACE(sizeof (int));

	msg.msg_control = calloc(1, msg.msg_controllen);
	if (msg.msg_control == NULL) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to allocate %u "
		    "bytes for SCM_RIGHTS control message", c->clo_desc,
		    msg.msg_controllen);
	}

	cm = CMSG_FIRSTHDR(&msg);
	cm->cmsg_len = CMSG_LEN(sizeof (int));
	cm->cmsg_level = SOL_SOCKET;
	cm->cmsg_type = SCM_RIGHTS;
	(void) memcpy(CMSG_DATA(cm), &tosend, sizeof (tosend));

	if ((targ_flags & FD_CLOEXEC) != 0)
		sflags |= MSG_CMSG_CLOEXEC;
	if ((targ_flags & FD_CLOFORK) != 0)
		sflags |= MSG_CMSG_CLOFORK;

	if (sendmsg(pair[0], &msg, 0) < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to send fd",
		    c->clo_desc);
	}

	data = 0;
	if (recvmsg(pair[1], &msg, sflags) < 0) {
		err(EXIT_FAILURE, "TEST FAILED: %s: failed to get fd",
		    c->clo_desc);
	}

	if (data != 0x7777) {
		errx(EXIT_FAILURE, "TEST FAILED: %s: did not receive correct "
		    "data: expected 0x7777, found 0x%x", c->clo_desc, data);
	}

	if (msg.msg_controllen < CMSG_SPACE(sizeof (int))) {
		errx(EXIT_FAILURE, "TEST FAILED: %s: found insufficient "
		    "message control length: expected at least 0x%x, found "
		    "0x%x", c->clo_desc, CMSG_SPACE(sizeof (int)),
		    msg.msg_controllen);
	}

	cm = CMSG_FIRSTHDR(&msg);
	if (cm->cmsg_level != SOL_SOCKET || cm->cmsg_type != SCM_RIGHTS) {
		errx(EXIT_FAILURE, "TEST FAILED: %s: found surprising cmsg "
		    "0x%x/0x%x, expected 0x%x/0x%x", c->clo_desc,
		    cm->cmsg_level, cm->cmsg_type, SOL_SOCKET, SCM_RIGHTS);
	}

	if (cm->cmsg_len != CMSG_LEN(sizeof (int))) {
		errx(EXIT_FAILURE, "TEST FAILED: %s: found unexpected "
		    "SCM_RIGHTS length 0x%x: expected 0x%zx", c->clo_desc,
		    cm->cmsg_len, CMSG_LEN(sizeof (int)));
	}

	(void) memcpy(&recvfd, CMSG_DATA(cm), sizeof (recvfd));
	oclo_record(c, recvfd, targ_flags, "SCM_RIGHTS");
}

static void
oclo_rights_none(const clo_create_t *c)
{
	oclo_rights_common(c, 0);
}

static void
oclo_rights_exec(const clo_create_t *c)
{
	oclo_rights_common(c, FD_CLOEXEC);
}

static void
oclo_rights_fork(const clo_create_t *c)
{
	oclo_rights_common(c, FD_CLOFORK);
}

static void
oclo_rights_both(const clo_create_t *c)
{
	oclo_rights_common(c, FD_CLOEXEC | FD_CLOFORK);
}

static const clo_create_t oclo_create[] = { {
	.clo_desc = "open(2), no flags",
	.clo_flags = 0,
	.clo_func = oclo_open
}, {
	.clo_desc = "open(2), O_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_open
}, {
	.clo_desc = "open(2), O_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_open
}, {
	.clo_desc = "open(2), O_CLOEXEC|O_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_open
}, {
	.clo_desc = "fcntl(F_SETFD) no flags->no flags",
	.clo_flags = 0,
	.clo_func = oclo_setfd_none
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOFORK|O_CLOEXEC->no flags",
	.clo_flags = O_CLOFORK | O_CLOEXEC,
	.clo_func = oclo_setfd_none
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOEXEC->no flags",
	.clo_flags = O_CLOEXEC,
	.clo_func = oclo_setfd_none
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOFORK->no flags",
	.clo_flags = O_CLOFORK,
	.clo_func = oclo_setfd_none
}, {
	.clo_desc = "fcntl(F_SETFD) no flags->O_CLOEXEC",
	.clo_flags = 0,
	.clo_func = oclo_setfd_exec
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOFORK|O_CLOEXEC->O_CLOEXEC",
	.clo_flags = O_CLOFORK | O_CLOEXEC,
	.clo_func = oclo_setfd_exec
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOEXEC->O_CLOEXEC",
	.clo_flags = O_CLOEXEC,
	.clo_func = oclo_setfd_exec
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOFORK->O_CLOEXEC",
	.clo_flags = O_CLOFORK,
	.clo_func = oclo_setfd_exec
}, {
	.clo_desc = "fcntl(F_SETFD) no flags->O_CLOFORK",
	.clo_flags = 0,
	.clo_func = oclo_setfd_fork
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOFORK|O_CLOEXEC->O_CLOFORK",
	.clo_flags = O_CLOFORK | O_CLOEXEC,
	.clo_func = oclo_setfd_fork
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOEXEC->O_CLOFORK",
	.clo_flags = O_CLOEXEC,
	.clo_func = oclo_setfd_fork
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOFORK->O_CLOFORK",
	.clo_flags = O_CLOFORK,
	.clo_func = oclo_setfd_fork
}, {
	.clo_desc = "fcntl(F_SETFD) no flags->O_CLOFORK|O_CLOEXEC",
	.clo_flags = 0,
	.clo_func = oclo_setfd_both
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOFORK|O_CLOEXEC->O_CLOFORK|O_CLOEXEC",
	.clo_flags = O_CLOFORK | O_CLOEXEC,
	.clo_func = oclo_setfd_both
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOEXEC->O_CLOFORK|O_CLOEXEC",
	.clo_flags = O_CLOEXEC,
	.clo_func = oclo_setfd_both
}, {
	.clo_desc = "fcntl(F_SETFD) O_CLOFORK->O_CLOFORK|O_CLOEXEC",
	.clo_flags = O_CLOFORK,
	.clo_func = oclo_setfd_both
}, {
	.clo_desc = "fcntl(F_DUPFD) none->none",
	.clo_flags = 0,
	.clo_func = oclo_fdupfd
}, {
	.clo_desc = "fcntl(F_DUPFD) FD_CLOEXEC->none",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdupfd
}, {
	.clo_desc = "fcntl(F_DUPFD) FD_CLOFORK->none",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdupfd
}, {
	.clo_desc = "fcntl(F_DUPFD) FD_CLOEXEC|FD_CLOFORK->none",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdupfd
}, {
	.clo_desc = "fcntl(F_DUPFD_CLOFORK) none",
	.clo_flags = 0,
	.clo_func = oclo_fdupfd_fork
}, {
	.clo_desc = "fcntl(F_DUPFD_CLOFORK) FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdupfd_fork
}, {
	.clo_desc = "fcntl(F_DUPFD_CLOFORK) FD_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdupfd_fork
}, {
	.clo_desc = "fcntl(F_DUPFD_CLOFORK) FD_CLOEXEC|FD_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdupfd_fork
}, {
	.clo_desc = "fcntl(F_DUPFD_CLOEXEC) none",
	.clo_flags = 0,
	.clo_func = oclo_fdupfd_exec
}, {
	.clo_desc = "fcntl(F_DUPFD_CLOEXEC) FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdupfd_exec
}, {
	.clo_desc = "fcntl(F_DUPFD_CLOEXEC) FD_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdupfd_exec
}, {
	.clo_desc = "fcntl(F_DUPFD_CLOEXEC) FD_CLOEXEC|FD_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdupfd_exec
}, {
	.clo_desc = "fcntl(F_DUP2FD) none->none",
	.clo_flags = 0,
	.clo_func = oclo_fdup2fd
}, {
	.clo_desc = "fcntl(F_DUP2FD) FD_CLOEXEC->none",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdup2fd
}, {
	.clo_desc = "fcntl(F_DUP2FD) FD_CLOFORK->none",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdup2fd
}, {
	.clo_desc = "fcntl(F_DUP2FD) FD_CLOEXEC|FD_CLOFORK->none",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdup2fd
}, {
	.clo_desc = "fcntl(F_DUP2FD_CLOFORK) none",
	.clo_flags = 0,
	.clo_func = oclo_fdup2fd_fork
}, {
	.clo_desc = "fcntl(F_DUP2FD_CLOFORK) FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdup2fd_fork
}, {
	.clo_desc = "fcntl(F_DUP2FD_CLOFORK) FD_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdup2fd_fork
}, {
	.clo_desc = "fcntl(F_DUP2FD_CLOFORK) FD_CLOEXEC|FD_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdup2fd_fork
}, {
	.clo_desc = "fcntl(F_DUP2FD_CLOEXEC) none",
	.clo_flags = 0,
	.clo_func = oclo_fdup2fd_exec
}, {
	.clo_desc = "fcntl(F_DUP2FD_CLOEXEC) FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdup2fd_exec
}, {
	.clo_desc = "fcntl(F_DUP2FD_CLOEXEC) FD_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdup2fd_exec
}, {
	.clo_desc = "fcntl(F_DUP2FD_CLOEXEC) FD_CLOEXEC|FD_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdup2fd_exec
}, {
	.clo_desc = "fcntl(F_DUP3FD) none->none",
	.clo_flags = 0,
	.clo_func = oclo_fdup3fd_none
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOEXEC->none",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdup3fd_none
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOFORK->none",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdup3fd_none
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOEXEC|FD_CLOFORK->none",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdup3fd_none
}, {
	.clo_desc = "fcntl(F_DUP3FD) none->FD_CLOEXEC",
	.clo_flags = 0,
	.clo_func = oclo_fdup3fd_exec
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOEXEC->FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdup3fd_exec
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOFORK->FD_CLOEXEC",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdup3fd_exec
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOEXEC|FD_CLOFORK->FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdup3fd_exec
}, {
	.clo_desc = "fcntl(F_DUP3FD) none->FD_CLOFORK|FD_CLOEXEC",
	.clo_flags = 0,
	.clo_func = oclo_fdup3fd_both
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOEXEC->FD_CLOFORK|FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdup3fd_both
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOFORK->FD_CLOFORK|FD_CLOEXEC",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdup3fd_both
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOEXEC|FD_CLOFORK->"
	    "FD_CLOFORK|FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdup3fd_both
}, {
	.clo_desc = "fcntl(F_DUP3FD) none->FD_CLOFORK",
	.clo_flags = 0,
	.clo_func = oclo_fdup3fd_fork
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOEXEC->FD_CLOFORK",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_fdup3fd_fork
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOFORK->FD_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_fdup3fd_fork
}, {
	.clo_desc = "fcntl(F_DUP3FD) FD_CLOEXEC|FD_CLOFORK->FD_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_fdup3fd_fork
}, {
	.clo_desc = "dup2() none->none",
	.clo_flags = 0,
	.clo_func = oclo_dup2
}, {
	.clo_desc = "dup2() FD_CLOEXEC->none",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_dup2
}, {
	.clo_desc = "dup2() FD_CLOFORK->none",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_dup2
}, {
	.clo_desc = "dup2() FD_CLOEXEC|FD_CLOFORK->none",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_dup2
}, {
	.clo_desc = "dup3() none->none",
	.clo_flags = 0,
	.clo_func = oclo_dup3_none
}, {
	.clo_desc = "dup3() FD_CLOEXEC->none",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_dup3_none
}, {
	.clo_desc = "dup3() FD_CLOFORK->none",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_dup3_none
}, {
	.clo_desc = "dup3() FD_CLOEXEC|FD_CLOFORK->none",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_dup3_none
}, {
	.clo_desc = "dup3() none->FD_CLOEXEC",
	.clo_flags = 0,
	.clo_func = oclo_dup3_exec
}, {
	.clo_desc = "dup3() FD_CLOEXEC->FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_dup3_exec
}, {
	.clo_desc = "dup3() FD_CLOFORK->FD_CLOEXEC",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_dup3_exec
}, {
	.clo_desc = "dup3() FD_CLOEXEC|FD_CLOFORK->FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_dup3_exec
}, {
	.clo_desc = "dup3() none->FD_CLOFORK|FD_CLOEXEC",
	.clo_flags = 0,
	.clo_func = oclo_dup3_both
}, {
	.clo_desc = "dup3() FD_CLOEXEC->FD_CLOFORK|FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_dup3_both
}, {
	.clo_desc = "dup3() FD_CLOFORK->FD_CLOFORK|FD_CLOEXEC",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_dup3_both
}, {
	.clo_desc = "dup3() FD_CLOEXEC|FD_CLOFORK->FD_CLOFORK|FD_CLOEXEC",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_dup3_both
}, {
	.clo_desc = "dup3() none->FD_CLOFORK",
	.clo_flags = 0,
	.clo_func = oclo_dup3_fork
}, {
	.clo_desc = "dup3() FD_CLOEXEC->FD_CLOFORK",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_dup3_fork
}, {
	.clo_desc = "dup3() FD_CLOFORK->FD_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_dup3_fork
}, {
	.clo_desc = "dup3() FD_CLOEXEC|FD_CLOFORK->FD_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_dup3_fork
}, {
	.clo_desc = "pipe(2), no flags",
	.clo_flags = 0,
	.clo_func = oclo_pipe
}, {
	.clo_desc = "pipe(2), O_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_pipe
}, {
	.clo_desc = "pipe(2), O_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_pipe
}, {
	.clo_desc = "pipe(2), O_CLOEXEC|O_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_pipe
}, {
	.clo_desc = "socket(2), no flags",
	.clo_flags = 0,
	.clo_func = oclo_socket
}, {
	.clo_desc = "socket(2), O_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_socket
}, {
	.clo_desc = "socket(2), O_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_socket
}, {
	.clo_desc = "socket(2), O_CLOEXEC|O_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_socket
}, {
	.clo_desc = "socket(2), no flags->accept() none",
	.clo_flags = 0,
	.clo_func = oclo_accept
}, {
	.clo_desc = "socket(2), O_CLOEXEC->accept() none",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_accept
}, {
	.clo_desc = "socket(2), O_CLOFORK->accept() none",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_accept
}, {
	.clo_desc = "socket(2), O_CLOEXEC|O_CLOFORK->accept() none",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_accept
}, {
	.clo_desc = "socket(2), no flags->accept4() none",
	.clo_flags = 0,
	.clo_func = oclo_accept4_none
}, {
	.clo_desc = "socket(2), O_CLOEXEC->accept4() none",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_accept4_none
}, {
	.clo_desc = "socket(2), O_CLOFORK->accept4() none",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_accept4_none
}, {
	.clo_desc = "socket(2), O_CLOEXEC|O_CLOFORK->accept4() none",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_accept4_none
}, {
	.clo_desc = "socket(2), no flags->accept4() SOCK_CLOFORK|SOCK_CLOEXEC",
	.clo_flags = 0,
	.clo_func = oclo_accept4_both
}, {
	.clo_desc = "socket(2), O_CLOEXEC->accept4() SOCK_CLOFORK|SOCK_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_accept4_both
}, {
	.clo_desc = "socket(2), O_CLOFORK->accept4() SOCK_CLOFORK|SOCK_CLOEXEC",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_accept4_both
}, {
	.clo_desc = "socket(2), O_CLOEXEC|O_CLOFORK->accept4() "
	    "SOCK_CLOFORK|SOCK_CLOEXEC",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_accept4_both
}, {
	.clo_desc = "socket(2), no flags->accept4() SOCK_CLOFORK",
	.clo_flags = 0,
	.clo_func = oclo_accept4_fork
}, {
	.clo_desc = "socket(2), O_CLOEXEC->accept4() SOCK_CLOFORK",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_accept4_fork
}, {
	.clo_desc = "socket(2), O_CLOFORK->accept4() SOCK_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_accept4_fork
}, {
	.clo_desc = "socket(2), O_CLOEXEC|O_CLOFORK->accept4() SOCK_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_accept4_fork
}, {
	.clo_desc = "socket(2), no flags->accept4() SOCK_CLOEXEC",
	.clo_flags = 0,
	.clo_func = oclo_accept4_exec
}, {
	.clo_desc = "socket(2), O_CLOEXEC->accept4() SOCK_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_accept4_exec
}, {
	.clo_desc = "socket(2), O_CLOFORK->accept4() SOCK_CLOEXEC",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_accept4_exec
}, {
	.clo_desc = "socket(2), O_CLOEXEC|O_CLOFORK->accept4() SOCK_CLOEXEC",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_accept4_exec
}, {
	.clo_desc = "SCM_RIGHTS none->none",
	.clo_flags = 0,
	.clo_func = oclo_rights_none
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOFORK->none",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_rights_none
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOEXEC->none",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_rights_none
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOEXEC|FD_CLOFORK->none",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_rights_none
}, {
	.clo_desc = "SCM_RIGHTS none->MSG_CMSG_CLOEXEC",
	.clo_flags = 0,
	.clo_func = oclo_rights_exec
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOFORK->MSG_CMSG_CLOEXEC",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_rights_exec
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOEXEC->MSG_CMSG_CLOEXEC",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_rights_exec
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOEXEC|FD_CLOFORK->MSG_CMSG_CLOEXEC",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_rights_exec
}, {
	.clo_desc = "SCM_RIGHTS MSG_CMSG_CLOFORK->nMSG_CMSG_CLOFORK",
	.clo_flags = 0,
	.clo_func = oclo_rights_fork
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOFORK->MSG_CMSG_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_rights_fork
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOEXEC->MSG_CMSG_CLOFORK",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_rights_fork
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOEXEC|FD_CLOFORK->MSG_CMSG_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_rights_fork
}, {
	.clo_desc = "SCM_RIGHTS none->MSG_CMSG_CLOEXEC|MSG_CMSG_CLOFORK",
	.clo_flags = 0,
	.clo_func = oclo_rights_both
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOFORK->MSG_CMSG_CLOEXEC|MSG_CMSG_CLOFORK",
	.clo_flags = FD_CLOFORK,
	.clo_func = oclo_rights_both
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOEXEC->MSG_CMSG_CLOEXEC|MSG_CMSG_CLOFORK",
	.clo_flags = FD_CLOEXEC,
	.clo_func = oclo_rights_both
}, {
	.clo_desc = "SCM_RIGHTS FD_CLOEXEC|FD_CLOFORK->"
	    "MSG_CMSG_CLOEXEC|MSG_CMSG_CLOFORK",
	.clo_flags = FD_CLOEXEC | FD_CLOFORK,
	.clo_func = oclo_rights_both
} };

static bool
oclo_verify_fork(void)
{
	bool ret = true;

	for (size_t i = 0; i < oclo_rtdata_next; i++) {
		if (!oclo_flags_match(&oclo_rtdata[i], true)) {
			ret = false;
		}
	}

	return (ret);
}

/*
 * Here we proceed to re-open any fd that was closed due to O_CLOFORK again to
 * make sure it makes it to our child verifier. This also serves as a test to
 * make sure that our opening of the lowest fd is correct. While this doesn't
 * actually use the same method as was done previously, While it might be ideal
 * to use the method as originally, this should get us most of the way there.
 */
static void
oclo_child_reopen(void)
{
	for (size_t i = 0; i < oclo_rtdata_next; i++) {
		int fd;
		int flags = O_RDWR | O_CLOFORK;

		if ((oclo_rtdata[i].crt_flags & FD_CLOFORK) == 0)
			continue;

		if ((oclo_rtdata[i].crt_flags & FD_CLOEXEC) != 0)
			flags |= O_CLOEXEC;

		fd = open("/dev/zero", flags);
		if (fd < 0) {
			err(EXIT_FAILURE, "TEST FAILED: failed to re-open fd "
			    "%d with flags %d", oclo_rtdata[i].crt_fd, flags);
		}

		if (fd != oclo_rtdata[i].crt_fd) {
			errx(EXIT_FAILURE, "TEST FAILED: re-opening fd %d "
			    "returned fd %d: test design issue or lowest fd "
			    "algorithm is broken", oclo_rtdata[i].crt_fd, fd);
		}
	}

	(void) printf("TEST PASSED: successfully reopened fds post-fork");
}

/*
 * Look for the verification program in the same directory that this program is
 * found in. Note, that isn't the same thing as the current working directory.
 */
static void
oclo_exec(void)
{
	ssize_t ret;
	char dir[PATH_MAX], file[PATH_MAX];
	char **argv;

	ret = readlink("/proc/self/path/a.out", dir, sizeof (dir));
	if (ret < 0) {
		err(EXIT_FAILURE, "TEST FAILED: failed to read our a.out path "
		    "from /proc");
	} else if (ret == 0) {
		errx(EXIT_FAILURE, "TEST FAILED: reading /proc/self/path/a.out "
		    "returned 0 bytes");
	} else if (ret == sizeof (dir)) {
		errx(EXIT_FAILURE, "TEST FAILED: Using /proc/self/path/a.out "
		    "requires truncation");
	}

	if (snprintf(file, sizeof (file), "%s/%s", dirname(dir), OCLO_VERIFY) >=
	    sizeof (file)) {
		errx(EXIT_FAILURE, "TEST FAILED: cannot assemble exec path "
		    "name: internal buffer overflow");
	}

	/* We need an extra for both the NULL terminator and the program name */
	argv = calloc(oclo_rtdata_next + 2, sizeof (char *));
	if (argv == NULL) {
		err(EXIT_FAILURE, "TEST FAILED: failed to allocate exec "
		    "argument array");
	}

	argv[0] = file;
	for (size_t i = 0; i < oclo_rtdata_next; i++) {
		if (asprintf(&argv[i + 1], "0x%x", oclo_rtdata[i].crt_flags) ==
		    -1) {
			err(EXIT_FAILURE, "TEST FAILED: failed to assemble "
			    "exec argument %zu", i + 1);
		}
	}

	(void) execv(file, argv);
	warn("TEST FAILED: failed to exec verifier %s", file);
}

int
main(void)
{
	int ret = EXIT_SUCCESS;
	siginfo_t cret;

	/*
	 * Before we do anything else close all FDs that aren't standard. We
	 * don't want anything the test suite environment may have left behind.
	 */
	(void) closefrom(STDERR_FILENO + 1);

	/*
	 * Treat failure during this set up phase as a hard failure. There's no
	 * reason to continue if we can't successfully create the FDs we expect.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(oclo_create); i++) {
		oclo_create[i].clo_func(&oclo_create[i]);
	}

	pid_t child = forkx(FORK_NOSIGCHLD | FORK_WAITPID);
	if (child == 0) {
		if (!oclo_verify_fork()) {
			ret = EXIT_FAILURE;
		}

		oclo_child_reopen();

		oclo_exec();
		ret = EXIT_FAILURE;
		_exit(ret);
	}

	if (waitid(P_PID, child, &cret, WEXITED) < 0) {
		err(EXIT_FAILURE, "TEST FAILED: internal test failure waiting "
		    "for forked child to report");
	}

	if (cret.si_code != CLD_EXITED) {
		warnx("TEST FAILED: child process did not successfully exit: "
		    "found si_code: %d", cret.si_code);
		ret = EXIT_FAILURE;
	} else if (cret.si_status != 0) {
		warnx("TEST FAILED: child process did not exit with code 0: "
		    "found %d", cret.si_status);
		ret = EXIT_FAILURE;
	}

	if (ret == EXIT_SUCCESS) {
		(void) printf("All tests passed successfully\n");
	}

	return (ret);
}
