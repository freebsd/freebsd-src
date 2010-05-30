/*-
 * Copyright (c) 2009-2010 Robert N. M. Watson
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
 * $P4: //depot/projects/trustedbsd/capabilities/src/lib/libcapsicum/libcapsicum_host.c#17 $
 */

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/procdesc.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <err.h>
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

#define	LIBCAPSICUM_CAPMASK_SOCK	(CAP_EVENT | CAP_READ | CAP_WRITE)
#define	LIBCAPSICUM_CAPMASK_BIN	(CAP_READ | CAP_EVENT | CAP_FSTAT | \
					    CAP_FSTATFS | \
					    CAP_FEXECVE | CAP_MMAP | \
					    CAP_MAPEXEC)
#define	LIBCAPSICUM_CAPMASK_SANDBOX	LIBCAPSICUM_CAPMASK_BIN
#define	LIBCAPSICUM_CAPMASK_LDSO	LIBCAPSICUM_CAPMASK_BIN
#define LIBCAPSICUM_CAPMASK_LIBDIR	LIBCAPSICUM_CAPMASK_BIN \
					 | CAP_LOOKUP | CAP_ATBASE
#define LIBCAPSICUM_CAPMASK_FDLIST	CAP_READ | CAP_WRITE | CAP_FTRUNCATE \
					 | CAP_FSTAT | CAP_MMAP

extern char **environ;

#define LD_ELF_CAP_SO		"ld-elf-cap.so.1"
#define	PATH_LD_ELF_CAP_SO	"/libexec"

int
lch_autosandbox_isenabled(__unused const char *servicename)
{

	if (getenv("LIBCAPSICUM_NOAUTOSANDBOX") != NULL)
		return (0);
	return (1);
}

/*
 * Once in the child process, create the new sandbox.
 *
 * XXX: A number of things happen here that are not safe after fork(),
 * especially calls to err().
 */
static void
lch_sandbox(int fd_sock, int fd_binary, int fd_rtld, u_int flags,
    const char *binname, char *const argv[], struct lc_fdlist *userfds)
{
	struct sbuf *sbufp;
	int shmfd = -1;
	size_t fdlistsize;
	struct lc_fdlist *fds;
	void *shm;

	/*
	 * Inform the run-time linked of the binary's name.
	 */
	if (setenv("LD_BINNAME", binname, 1) == -1)
		err(-1, "Error in setenv(LD_BINNAME)");

	/*
	 * Create an anonymous shared memory segment for the FD list.
	 */
	shmfd = shm_open(SHM_ANON, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (shmfd < 0)
		err(-1, "Error creating shared memory segment");

	/*
	 * Create and fill up the FD list.
	 */
	fds = lc_fdlist_new();
	if (fds == NULL)
		err(-1, "Error in lc_fdlist_new()");

	if (lc_fdlist_addcap(fds, LIBCAPSICUM_FQNAME, "stdin", "",
		STDIN_FILENO, 0) < 0)
		err(-1, "Error in lc_fdlist_addcap(stdin)");

	if (lc_fdlist_addcap(fds, LIBCAPSICUM_FQNAME, "stdout", "",
		STDOUT_FILENO,
		(flags & LCH_PERMIT_STDOUT) ? CAP_WRITE | CAP_SEEK : 0) < 0)
		err(-1, "Error in lc_fdlist_addcap(stdout)");

	if (lc_fdlist_addcap(fds, LIBCAPSICUM_FQNAME, "stderr", "",
		STDERR_FILENO,
		(flags & LCH_PERMIT_STDERR) ? CAP_WRITE | CAP_SEEK : 0) < 0)
		err(-1, "Error in lc_fdlist_addcap(stderr)");

	if (lc_fdlist_addcap(fds, LIBCAPSICUM_FQNAME, "socket", "",
	                     fd_sock, LIBCAPSICUM_CAPMASK_SOCK) < 0)
		err(-1, "Error in lc_fdlist_addcap(fd_sock)");

	if (lc_fdlist_addcap(fds, LIBCAPSICUM_FQNAME, "fdlist", "",
	                     shmfd, LIBCAPSICUM_CAPMASK_FDLIST) < 0)
		err(-1, "Error in lc_fdlist_addcap(shmfd)");

	if (lc_fdlist_addcap(fds, RTLD_CAP_FQNAME, "rtld", "",
	                     fd_rtld, LIBCAPSICUM_CAPMASK_LDSO) < 0)
		err(-1, "Error in lc_fdlist_addcap(fd_rtld)");

	if (lc_fdlist_addcap(fds, RTLD_CAP_FQNAME, "binary", "",
	                     fd_binary, LIBCAPSICUM_CAPMASK_SANDBOX) < 0)
		err(-1, "Error in lc_fdlist_addcap(fd_binary)");

	if (lc_fdlist_append(fds, userfds) < 0)
		err(-1, "Error in lc_fdlist_append()");

	/*
	 * Ask RTLD for library path descriptors.
	 *
	 * NOTE: This is FreeBSD-specific; porting to other operating systems
	 * will require dynamic linkers capable of answering similar queries.
	 */
	int size = 16;
	int *libdirs;

	while (1) {
		libdirs = malloc(size * sizeof(int));
		if (ld_libdirs(libdirs, &size) < 0) {
			free(libdirs);
			if (size > 0)
				continue;
			err(-1, "Error in ld_libdirs()");
		} else
			break;
	}

	for (int j = 0; j < size; j++)
		if (lc_fdlist_addcap(fds, RTLD_CAP_FQNAME, "libdir", "",
		    libdirs[j], LIBCAPSICUM_CAPMASK_LIBDIR) < 0)
			err(-1, "Error in lc_fdlist_addcap(libdirs[%d]: %d)",
			    j, libdirs[j]);

	if (lc_fdlist_reorder(fds) < 0)
		err(-1, "Error in lc_fdlist_reorder()");

	/*
	 * Find the fdlist shared memory segment.
	 */
	int pos = 0;
	if (lc_fdlist_lookup(fds, LIBCAPSICUM_FQNAME, "fdlist", NULL, &shmfd,
	    &pos) < 0)
		err(-1, "Error in lc_fdlist_lookup(fdlist)");

	char tmp[8];
	sprintf(tmp, "%d", shmfd);
	if (setenv(LIBCAPSICUM_SANDBOX_FDLIST, tmp, 1) == -1)
		err(-1, "Error in setenv(LIBCAPSICUM_SANDBOX_FDLIST)");

	/*
	 * Map it and copy the list.
	 */
	fdlistsize = lc_fdlist_size(fds);
	if (ftruncate(shmfd, fdlistsize) < 0)
		err(-1, "Error in ftruncate(shmfd)");

	shm = mmap(NULL, fdlistsize, PROT_READ | PROT_WRITE,
	    MAP_NOSYNC | MAP_SHARED, shmfd, 0);
	if (shm == MAP_FAILED)
		err(-1, "Error mapping fdlist SHM");

	memcpy(shm, _lc_fdlist_getstorage(fds), fdlistsize);
	if (munmap(shm, fdlistsize))
		err(-1, "Error in munmap(shm, fdlistsize)");


	/*
	 * Find RTLD.
	 */
	if (lc_fdlist_lookup(fds, RTLD_CAP_FQNAME, "rtld", NULL, &fd_rtld,
	                     NULL) < 0)
		err(-1, "Error in lc_fdlist_lookup(RTLD)");

	/*
	 * Find the binary for RTLD.
	 */
	if (lc_fdlist_lookup(fds, RTLD_CAP_FQNAME, "binary", NULL,
	    &fd_binary, NULL) < 0)
		err(-1, "Error in lc_fdlist_lookup(RTLD binary)");

	sprintf(tmp, "%d", fd_binary);
	if (setenv("LD_BINARY", tmp, 1) != 0)
		err(-1, "Error in setenv(LD_BINARY)");

	/*
	 * Build LD_LIBRARY_DIRS for RTLD.
	 *
	 * NOTE: This is FreeBSD-specific; porting to other operating systems
	 * will require dynamic linkers capable of operating on file
	 * descriptors.
	 */
	sbufp = sbuf_new_auto();
	if (sbufp == NULL)
		err(-1, "Error in sbuf_new_auto()");

	{
		int fd;
		while (lc_fdlist_lookup(fds, RTLD_CAP_FQNAME, "libdir", NULL,
		    &fd, &pos) >= 0)
			sbuf_printf(sbufp, "%d:", fd);
	}

	sbuf_finish(sbufp);
	if (sbuf_overflowed(sbufp))
		err(-1, "sbuf_overflowed()");
	if (setenv("LD_LIBRARY_DIRS", sbuf_data(sbufp), 1) == -1)
		err(-1, "Error in setenv(LD_LIBRARY_DIRS)");
	sbuf_delete(sbufp);

	if (cap_enter() < 0)
		err(-1, "cap_enter() failed");

	(void)fexecve(fd_rtld, argv, environ);
}

int
lch_startfd(int fd_binary, const char *binname, char *const argv[],
    u_int flags, struct lc_fdlist *fds, struct lc_sandbox **lcspp)
{
	struct lc_sandbox *lcsp;
	int fd_rtld;
	int fd_procdesc, fd_sockpair[2];
	int error, val;
	pid_t pid;

	fd_rtld = fd_procdesc = fd_sockpair[0] = fd_sockpair[1] = -1;

	lcsp = malloc(sizeof(*lcsp));
	if (lcsp == NULL)
		return (-1);
	bzero(lcsp, sizeof(*lcsp));

	if (ld_insandbox()) {
		struct lc_fdlist *globals;
		int pos = 0;

		globals = lc_fdlist_global();
		if (globals == NULL)
			goto out_error;
		if (lc_fdlist_lookup(globals, RTLD_CAP_FQNAME, "rtld", NULL,
		    &fd_rtld, &pos) < 0)
			goto out_error;
	} else {
		fd_rtld = open(PATH_LD_ELF_CAP_SO "/" LD_ELF_CAP_SO,
		    O_RDONLY);
		if (fd_rtld < 0)
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
		lch_sandbox(fd_sockpair[1], fd_binary, fd_rtld, flags,
		    binname, argv, fds);
		exit(-1);
	}
	if (fd_rtld != -1)
		close(fd_rtld);
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
	if (fd_rtld != -1)
		close(fd_rtld);
	if (lcsp != NULL)
		free(lcsp);
	errno = error;
	return (-1);
}

int
lch_start(const char *sandbox, char *const argv[], u_int flags,
    struct lc_fdlist *fds, struct lc_sandbox **lcspp)
{
	char binname[MAXPATHLEN];
	int error, fd_binary, ret;

	if (basename_r(sandbox, binname) == NULL)
		return (-1);

	fd_binary = open(sandbox, O_RDONLY);
	if (fd_binary < 0)
		return (-1);

	ret = lch_startfd(fd_binary, binname, argv, flags, fds, lcspp);
	error = errno;
	close(fd_binary);
	errno = error;
	return (ret);
}

void
lch_stop(struct lc_sandbox *lcsp)
{

	close(lcsp->lcs_fd_sock);
	close(lcsp->lcs_fd_procdesc);
	lcsp->lcs_fd_sock = -1;
	lcsp->lcs_fd_procdesc = -1;
	lcsp->lcs_pid = -1;
	free(lcsp);
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
