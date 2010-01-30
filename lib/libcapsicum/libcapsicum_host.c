/*-
 * Copyright (c) 2009 Robert N. M. Watson
 * All rights reserved.
 *
 * WARNING: THIS IS EXPERIMENTAL SECURITY SOFTWARE THAT MUST NOT BE RELIED
 * ON IN PRODUCTION SYSTEMS.  IT WILL BREAK YOUR SOFTWARE IN NEW AND
 * UNEXPECTED WAYS.
 * 
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc. 
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
 * $P4: //depot/projects/trustedbsd/capabilities/src/lib/libcapsicum/libcapsicum_host.c#1 $
 */

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/procdesc.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libcapsicum.h"
#include "libcapsicum_internal.h"
#include "libcapsicum_sandbox_api.h"

#define	LIBCAPABILITY_CAPMASK_DEVNULL	(CAP_EVENT | CAP_READ | CAP_WRITE)
#define	LIBCAPABILITY_CAPMASK_SOCK	(CAP_EVENT | CAP_READ | CAP_WRITE)
#define	LIBCAPABILITY_CAPMASK_BIN	(CAP_READ | CAP_EVENT | CAP_FSTAT | \
					    CAP_FSTATFS | \
					    CAP_FEXECVE | CAP_MMAP | \
					    CAP_MAPEXEC)
#define	LIBCAPABILITY_CAPMASK_SANDBOX	LIBCAPABILITY_CAPMASK_BIN
#define	LIBCAPABILITY_CAPMASK_LDSO	LIBCAPABILITY_CAPMASK_BIN
#define	LIBCAPABILITY_CAPMASK_LIB	LIBCAPABILITY_CAPMASK_BIN

#define	_PATH_LIB	"/lib"
#define	_PATH_USR_LIB	"/usr/lib"
#define	LIBC_SO	"libc.so.7"
#define	LIBCAPABILITY_SO	"libcapsicum.so.1"
#define	LIBSBUF_SO	"libsbuf.so.5"

extern char **environ;

#define LD_ELF_CAP_SO		"ld-elf-cap.so.1"
#define	PATH_LD_ELF_CAP_SO	"/libexec"
char *ldso_argv[] = {
	__DECONST(char *, PATH_LD_ELF_CAP_SO "/" LD_ELF_CAP_SO),
	NULL,
};

int
lch_autosandbox_isenabled(__unused const char *servicename)
{

	if (getenv("LIBCAPABILITY_NOAUTOSANDBOX") != NULL)
		return (0);
	return (1);
}

/*
 * Install an array of file descriptors using the array index of each
 * descriptor in the array as its destination file descriptor number.  All
 * other existing file descriptors will be closed when this function returns,
 * leaving a pristine vector.  If calls fail, then we return (-1), but there
 * are no guarantees about the state of the file descriptor array for the
 * process, so it's a throw-away.
 *
 * It would be nice not to shuffle descriptors that already have the right
 * number.
 */
static int
lch_installfds(u_int fd_count, int *fds)
{
	u_int i;
	int highestfd;

	if (fd_count == 0)
		return (0);

	/*
	 * Identify the highest source file descriptor we care about so that
	 * when we play the dup2() rearranging game, we don't overwrite any
	 * we care about.
	 */
	highestfd = fds[0];
	for (i = 1; i < fd_count; i++) {
		if (fds[i] > highestfd)
			highestfd = fds[i];
	}
	highestfd++;	/* Don't tread on the highest */

	/*
	 * First, move all our descriptors up the range.
	 */
	for (i = 0; i < fd_count; i++) {
		if (dup2(fds[i], highestfd + i) < 0)
			return (-1);
	}

	/*
	 * Now put them back.
	 */
	for (i = 0; i < fd_count; i++) {
		if (dup2(highestfd + i, i) < 0)
			return (-1);
	}

	/*
	 * Close the descriptors that we moved, as well as any others that
	 * were left open by the caller.
	 */
	closefrom(fd_count);
	return (0);
}

static void
lch_sandbox(int fd_sock, int fd_sandbox, int fd_ldso, int fd_libc,
    int fd_libcapsicum, int fd_libsbuf, int fd_devnull, u_int flags,
    struct lc_library *lclp, u_int lcl_count, const char *binname,
    char *const argv[])
{
	int *fd_array, fdcount;
	struct sbuf *sbufp;
	u_int i;

	if (lc_limitfd(fd_devnull, LIBCAPABILITY_CAPMASK_DEVNULL) < 0)
		return;
	if (lc_limitfd(fd_sandbox, LIBCAPABILITY_CAPMASK_SANDBOX) < 0)
		return;
	if (lc_limitfd(fd_sock, LIBCAPABILITY_CAPMASK_SOCK) < 0)
		return;
	if (lc_limitfd(fd_ldso, LIBCAPABILITY_CAPMASK_LDSO) < 0)
		return;
	if (lc_limitfd(fd_libc, LIBCAPABILITY_CAPMASK_LIB) < 0)
		return;
	if (lc_limitfd(fd_libcapsicum, LIBCAPABILITY_CAPMASK_LIB) < 0)
		return;
	if (lc_limitfd(fd_libsbuf, LIBCAPABILITY_CAPMASK_LIB) < 0)
		return;

	fdcount = 10 + lcl_count;
	fd_array = malloc(fdcount * sizeof(int));
	if (fd_array == NULL)
		return;

	fd_array[0] = fd_devnull;
	if (flags & LCH_PERMIT_STDOUT) {
		if (lc_limitfd(STDOUT_FILENO, CAP_SEEK | CAP_WRITE) < 0)
			return;
		fd_array[1] = STDOUT_FILENO;
	} else
		fd_array[1] = fd_devnull;
	if (flags & LCH_PERMIT_STDERR) {
		if (lc_limitfd(STDERR_FILENO, CAP_SEEK | CAP_WRITE) < 0)
			return;
		fd_array[2] = STDERR_FILENO;
	} else
		fd_array[2] = fd_devnull;
	fd_array[3] = fd_sandbox;
	fd_array[4] = fd_sock;
	fd_array[5] = fd_ldso;
	fd_array[6] = fd_libc;
	fd_array[7] = fd_libcapsicum;
	fd_array[8] = fd_libsbuf;
	fd_array[9] = fd_devnull;
	for (i = 0; i < lcl_count; i++) {
		if (lc_limitfd(lclp->lcl_fd, LIBCAPABILITY_CAPMASK_LIB) < 0)
			return;
		fd_array[i + 10] = lclp[i].lcl_fd;
	}

	if (lch_installfds(fdcount, fd_array) < 0)
		return;

	sbufp = sbuf_new_auto();
	if (sbufp == NULL)
		return;
	(void)sbuf_printf(sbufp, "%d:%s,%d:%s,%d:%s,%d:%s,%d:%s,%d:%s",
	    3, binname, 5, LD_ELF_CAP_SO, 6, LIBC_SO, 7, LIBCAPABILITY_SO,
	    8, LIBSBUF_SO, 9, _PATH_DEVNULL);
	for (i = 0; i < lcl_count; i++)
		(void)sbuf_printf(sbufp, ",%d:%s", i + 10,
		    lclp[i].lcl_libname);
	sbuf_finish(sbufp);
	if (sbuf_overflowed(sbufp))
		return;
	if (setenv("LD_LIBCACHE", sbuf_data(sbufp), 1) == -1)
		return;
	sbuf_delete(sbufp);

	sbufp = sbuf_new_auto();
	if (sbufp == NULL)
		return;
	(void)sbuf_printf(sbufp, "%s:%d", LIBCAPABILITY_SANDBOX_API_SOCK, 4);
	sbuf_finish(sbufp);
	if (sbuf_overflowed(sbufp))
		return;
	if (setenv(LIBCAPABILITY_SANDBOX_API_ENV, sbuf_data(sbufp), 1) == -1)
		return;
	sbuf_delete(sbufp);

	if (cap_enter() < 0)
		return;

	(void)fexecve(5, argv, environ);
}

int
lch_startfd_libs(int fd_sandbox, const char *binname, char *const argv[],
    u_int flags, struct lc_library *lclp, u_int lcl_count,
    struct lc_sandbox **lcspp)
{
	struct lc_sandbox *lcsp;
	int fd_devnull, fd_ldso, fd_libc, fd_libcapsicum, fd_libsbuf;
	int fd_procdesc, fd_sockpair[2];
	int error, val;
	pid_t pid;

	fd_devnull = fd_ldso = fd_libc = fd_libcapsicum = fd_libsbuf =
	    fd_procdesc = fd_sockpair[0] = fd_sockpair[1] = -1;

	lcsp = malloc(sizeof(*lcsp));
	if (lcsp == NULL)
		return (-1);
	bzero(lcsp, sizeof(*lcsp));

	if (ld_insandbox()) {
		if (ld_libcache_lookup(LD_ELF_CAP_SO, &fd_ldso) < 0)
			goto out_error;
		if (ld_libcache_lookup(LIBC_SO, &fd_libc) < 0)
			goto out_error;
		if (ld_libcache_lookup(LIBCAPABILITY_SO,
		    &fd_libcapsicum) < 0)
			goto out_error;
		if (ld_libcache_lookup(LIBSBUF_SO, &fd_libsbuf) < 0)
			goto out_error;
		if (ld_libcache_lookup(_PATH_DEVNULL, &fd_devnull) < 0)
			goto out_error;
	} else {
		fd_ldso = open(PATH_LD_ELF_CAP_SO "/" LD_ELF_CAP_SO,
		    O_RDONLY);
		if (fd_ldso < 0)
			goto out_error;
		fd_libc = open(_PATH_LIB "/" LIBC_SO, O_RDONLY);
		if (fd_libc < 0)
			goto out_error;
		fd_libsbuf = open(_PATH_LIB "/" LIBSBUF_SO, O_RDONLY);
		if (fd_libsbuf < 0)
			goto out_error;
		fd_libcapsicum = open(_PATH_USR_LIB "/" LIBCAPABILITY_SO,
		    O_RDONLY);
		if (fd_libcapsicum < 0)
			goto out_error;
		fd_devnull = open(_PATH_DEVNULL, O_RDWR);
		if (fd_devnull < 0)
			goto out_error;
	}

	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd_sockpair) < 0)
		goto out_error;

	val = 1;
	if (setsockopt(fd_sockpair[0], SOL_SOCKET, SO_NOSIGPIPE, &val,
	    sizeof(val)) < 0) {
		fd_sockpair[0] = fd_sockpair[1] = -1;
		goto out_error;
	}

	pid = pdfork(&fd_procdesc);
	if (pid < 0) {
		fd_procdesc = -1;
		goto out_error;
	}
	if (pid == 0) {
		lch_sandbox(fd_sockpair[1], fd_sandbox, fd_ldso, fd_libc,
		    fd_libcapsicum, fd_libsbuf, fd_devnull, flags, lclp,
		    lcl_count, binname, argv);
		exit(-1);
	}
#ifndef IN_CAP_MODE
	close(fd_devnull);
	close(fd_libsbuf);
	close(fd_libcapsicum);
	close(fd_libc);
	close(fd_ldso);
#endif
	close(fd_sockpair[1]);

	lcsp->lcs_fd_procdesc = fd_procdesc;
	lcsp->lcs_fd_sock = fd_sockpair[0];
	lcsp->lcs_pid = pid;
	*lcspp = lcsp;

	return (0);

out_error:
	error = errno;
	if (fd_sockpair[0] != -1)
		close(fd_sockpair[0]);
	if (fd_sockpair[1] != -1)
		close(fd_sockpair[1]);
#ifndef IN_CAP_MODE
	if (fd_devnull != -1)
		close(fd_devnull);
	if (fd_libsbuf != -1)
		close(fd_libsbuf);
	if (fd_libcapsicum != -1)
		close(fd_libcapsicum);
	if (fd_libc != -1)
		close(fd_libc);
	if (fd_ldso != -1)
		close(fd_ldso);
#endif
	if (lcsp != NULL)
		free(lcsp);
	errno = error;
	return (-1);
}

int
lch_startfd(int fd_sandbox, const char *binname, char *const argv[],
    u_int flags, struct lc_sandbox **lcspp)
{

	return (lch_startfd_libs(fd_sandbox, binname, argv, flags, NULL, 0,
	    lcspp));
}

int
lch_start_libs(const char *sandbox, char *const argv[], u_int flags,
    struct lc_library *lclp, u_int lcl_count, struct lc_sandbox **lcspp)
{
	char binname[MAXPATHLEN];
	int error, fd_sandbox, ret;

	if (basename_r(sandbox, binname) == NULL)
		return (-1);

	fd_sandbox = open(sandbox, O_RDONLY);
	if (fd_sandbox < 0)
		return (-1);

	ret = lch_startfd_libs(fd_sandbox, binname, argv, flags, lclp,
	    lcl_count, lcspp);
	error = errno;
	close(fd_sandbox);
	errno = error;
	return (ret);
}

int
lch_start(const char *sandbox, char *const argv[], u_int flags,
    struct lc_sandbox **lcspp)
{

	return (lch_start_libs(sandbox, argv, flags, NULL, 0, lcspp));
}

void
lch_stop(struct lc_sandbox *lcsp)
{

	close(lcsp->lcs_fd_sock);
	close(lcsp->lcs_fd_procdesc);
	lcsp->lcs_fd_sock = -1;
	lcsp->lcs_fd_procdesc = -1;
	lcsp->lcs_pid = -1;
}

int
lch_getsock(struct lc_sandbox *lcsp, int *fdp)
{

	*fdp = lcsp->lcs_fd_sock;
	return (0);
}

int
lch_getpid(struct lc_sandbox *lcsp, pid_t *pidp)
{

	*pidp = lcsp->lcs_pid;
	return (0);
}

int
lch_getprocdesc(struct lc_sandbox *lcsp, int *fdp)
{

	*fdp = lcsp->lcs_fd_procdesc;
	return (0);
}
