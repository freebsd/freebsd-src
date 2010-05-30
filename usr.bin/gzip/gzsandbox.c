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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libcapsicum.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gzip.h"

#define	LC_USR_BIN_GZIP_SANDBOX	"/usr/bin/gzip"

#ifndef NO_SANDBOX_SUPPORT

int	gzsandbox(void);

static char *lc_sandbox_argv[] = { __DECONST(char *, LC_USR_BIN_GZIP_SANDBOX),
				    NULL };

#define	PROXIED_GZ_COMPRESS	1
#define	PROXIED_GZ_UNCOMPRESS	2
#define	PROXIED_UNBZIP2		3

static struct lc_sandbox	*lcsp;
static int			 gzsandbox_initialized;
static int			 gzsandbox_enabled;

static void
gzsandbox_initialize(void)
{

	if (gzsandbox_initialized)
		return;
	gzsandbox_enabled = lch_autosandbox_isenabled("gzip");
	gzsandbox_initialized = 1;
	if (!gzsandbox_enabled)
		return;

	if (lch_start(LC_USR_BIN_GZIP_SANDBOX, lc_sandbox_argv,
	    LCH_PERMIT_STDERR, NULL, &lcsp) < 0)
		err(-1, "lch_start %s", LC_USR_BIN_GZIP_SANDBOX);
}

struct host_gz_compress_req {
	char		hgc_req_origname[PATH_MAX];
	int		hgc_req_numflag;
	uint32_t	hgc_req_mtime;
} __packed;

struct host_gz_compress_rep {
	off_t	hgc_rep_gsize;
	off_t	hgc_rep_retval;
} __packed;

static off_t
gz_compress_insandbox(int in, int out, off_t *gsizep, const char *origname,
    uint32_t mtime)
{
	struct host_gz_compress_req req;
	struct host_gz_compress_rep rep;
	struct iovec iov_req, iov_rep;
	int fdarray[2];
	size_t len;

	bzero(&req, sizeof(req));
	strlcpy(req.hgc_req_origname, origname,
	    sizeof(req.hgc_req_origname));
	req.hgc_req_numflag = numflag;
	req.hgc_req_mtime = mtime;
	iov_req.iov_base = &req;
	iov_req.iov_len = sizeof(req);
	iov_rep.iov_base = &rep;
	iov_rep.iov_len = sizeof(rep);
	fdarray[0] = cap_new(in, CAP_FSTAT | CAP_READ | CAP_SEEK);
	fdarray[1] = cap_new(out, CAP_FSTAT | CAP_WRITE | CAP_SEEK);
	if (fdarray[0] == -1 || fdarray[1] == -1)
		err(-1, "cap_new");
	if (lch_rpc_rights(lcsp, PROXIED_GZ_COMPRESS, &iov_req, 1,
	    fdarray, 2, &iov_rep, 1, &len, NULL, NULL) < 0)
		err(-1, "lch_rpc_rights");
	if (len != sizeof(rep))
		errx(-1, "lch_rpc_rights len %zu", len);
	if (gsizep != NULL)
		*gsizep = rep.hgc_rep_gsize;
	close(fdarray[0]);
	close(fdarray[1]);
	return (rep.hgc_rep_retval);
}

static void
sandbox_gz_compress_buffer(struct lc_host *lchp, uint32_t opno,
    uint32_t seqno, char *buffer, size_t len, int fd_in, int fd_out)
{
	struct host_gz_compress_req req;
	struct host_gz_compress_rep rep;
	struct iovec iov;

	if (len != sizeof(req))
		err(-1, "sandbox_gz_compress_buffer: len %zu", len);

	bcopy(buffer, &req, sizeof(req));
	bzero(&rep, sizeof(rep));
	numflag = req.hgc_req_numflag;
	rep.hgc_rep_retval = gz_compress(fd_in, fd_out, &rep.hgc_rep_gsize,
	    req.hgc_req_origname, req.hgc_req_mtime);
	iov.iov_base = &rep;
	iov.iov_len = sizeof(rep);
	if (lcs_sendrpc(lchp, opno, seqno, &iov, 1) < 0)
		err(-1, "lcs_sendrpc");
}

off_t
gz_compress_wrapper(int in, int out, off_t *gsizep, const char *origname,
    uint32_t mtime)
{

	gzsandbox_initialize();
	if (gzsandbox_enabled)
		return (gz_compress_insandbox(in, out, gsizep, origname,
		    mtime));
	else
		return (gz_compress(in, out, gsizep, origname, mtime));
}

struct host_gz_uncompress_req {
	size_t	hgu_req_prelen;
	char	hgu_req_filename[PATH_MAX];
	/* ... followed by data ... */
};

struct host_gz_uncompress_rep {
	off_t	hgu_rep_gsize;
	off_t	hgu_rep_retval;
};

static off_t
gz_uncompress_insandbox(int in, int out, char *pre, size_t prelen,
    off_t *gsizep, const char *filename)
{
	struct host_gz_uncompress_req req;
	struct host_gz_uncompress_rep rep;
	struct iovec iov_req[2], iov_rep;
	int fdarray[2];
	size_t len;

	bzero(&req, sizeof(req));
	req.hgu_req_prelen = prelen;
	strlcpy(req.hgu_req_filename, filename,
	    sizeof(req.hgu_req_filename));
	iov_req[0].iov_base = &req;
	iov_req[0].iov_len = sizeof(req);
	iov_req[1].iov_base = pre;
	iov_req[1].iov_len = prelen;
	iov_rep.iov_base = &rep;
	iov_rep.iov_len = sizeof(rep);
	fdarray[0] = cap_new(in, CAP_FSTAT | CAP_READ | CAP_SEEK);
	fdarray[1] = cap_new(out, CAP_FSTAT | CAP_WRITE | CAP_SEEK);
	if (fdarray[0] == -1 || fdarray[1] == -1)
		err(-1, "cap_new");
	if (lch_rpc_rights(lcsp, PROXIED_GZ_UNCOMPRESS, iov_req, 1,
	    fdarray, 2, &iov_rep, 1, &len, NULL, NULL) < 0)
		err(-1, "lch_rpc_rights");
	if (len != sizeof(rep))
		errx(-1, "lch_rpc_rights len %zu", len);
	if (gsizep != NULL)
		*gsizep = rep.hgu_rep_gsize;
	close(fdarray[0]);
	close(fdarray[1]);
	return (rep.hgu_rep_retval);
}

static void
sandbox_gz_uncompress_buffer(struct lc_host *lchp, uint32_t opno,
    uint32_t seqno, char *buffer, size_t len, int fd_in, int fd_out)
{
	struct host_gz_uncompress_req req;
	struct host_gz_uncompress_rep rep;
	struct iovec iov;
	char *pre;

	if (len != sizeof(req))
		err(-1, "sandbox_gz_uncompress_buffer: len %zu", len);

	bcopy(buffer, &req, sizeof(req));
	pre = buffer + sizeof(req);
	bzero(&rep, sizeof(rep));
	rep.hgu_rep_retval = gz_uncompress(fd_in, fd_out, pre,
	    req.hgu_req_prelen, &rep.hgu_rep_gsize, req.hgu_req_filename);
	iov.iov_base = &rep;
	iov.iov_len = sizeof(rep);
	if (lcs_sendrpc(lchp, opno, seqno, &iov, 1) < 0)
		err(-1, "lcs_sendrpc");
}

off_t
gz_uncompress_wrapper(int in, int out, char *pre, size_t prelen,
    off_t *gsizep, const char *filename)
{

	gzsandbox_initialize();
	if (gzsandbox_enabled)
		return (gz_uncompress_insandbox(in, out,  pre, prelen,
		    gsizep, filename));
	else
		return (gz_uncompress(in, out, pre, prelen, gsizep,
		    filename));
}

struct host_unbzip2_req {
	size_t	hub_req_prelen;
	/* ... followed by data ... */
};

struct host_unbzip2_rep {
	off_t	hub_rep_bytes_in;
	off_t	hub_rep_retval;
};

static off_t
unbzip2_insandbox(int in, int out, char *pre, size_t prelen, off_t *bytes_in)
{
	struct host_unbzip2_req req;
	struct host_unbzip2_rep rep;
	struct iovec iov_req[2], iov_rep;
	int fdarray[2];
	size_t len;

	bzero(&req, sizeof(req));
	req.hub_req_prelen = prelen;
	iov_req[0].iov_base = &req;
	iov_req[0].iov_len = sizeof(req);
	iov_req[1].iov_base = pre;
	iov_req[1].iov_len = prelen;
	iov_rep.iov_base = &rep;
	iov_rep.iov_len = sizeof(rep);
	fdarray[0] = cap_new(in, CAP_FSTAT | CAP_READ | CAP_SEEK);
	fdarray[1] = cap_new(out, CAP_FSTAT | CAP_WRITE | CAP_SEEK);
	if (fdarray[0] == -1 || fdarray[1] == -1)
		err(-1, "cap_new");
	if (lch_rpc_rights(lcsp, PROXIED_UNBZIP2, iov_req, 1,
	    fdarray, 2, &iov_rep, 1, &len, NULL, NULL) < 0)
		err(-1, "lch_rpc_rights");
	if (len != sizeof(rep))
		errx(-1, "lch_rpc_rights len %zu", len);
	if (bytes_in != NULL)
		*bytes_in = rep.hub_rep_bytes_in;
	close(fdarray[0]);
	close(fdarray[1]);
	return (rep.hub_rep_retval);
}

static void
sandbox_unbzip2_buffer(struct lc_host *lchp, uint32_t opno,
    uint32_t seqno, char *buffer, size_t len, int fd_in, int fd_out)
{
	struct host_unbzip2_req req;
	struct host_unbzip2_rep rep;
	struct iovec iov;
	char *pre;

	if (len != sizeof(req))
		err(-1, "sandbox_gz_uncompress_buffer: len %zu", len);

	bcopy(buffer, &req, sizeof(req));
	pre = buffer + sizeof(req);
	bzero(&rep, sizeof(rep));
	rep.hub_rep_retval = unbzip2(fd_in, fd_out, pre, req.hub_req_prelen,
	    &rep.hub_rep_bytes_in);
	iov.iov_base = &rep;
	iov.iov_len = sizeof(rep);
	if (lcs_sendrpc(lchp, opno, seqno, &iov, 1) < 0)
		err(-1, "lcs_sendrpc");
}

off_t
unbzip2_wrapper(int in, int out, char *pre, size_t prelen, off_t *bytes_in)
{

	gzsandbox_initialize();
	if (gzsandbox_enabled)
		return (unbzip2_insandbox(in, out, pre, prelen, bytes_in));
	else
		return (unbzip2(in, out, pre, prelen, bytes_in));
}

/*
 * Main entry point for capability-mode 
 */
int gzsandbox(void)
{
	int fdarray[2], fdcount;
	struct lc_host *lchp;
	uint32_t opno, seqno;
	u_char *buffer;
	size_t len;

	if (lcs_get(&lchp) < 0)
		errx(-1, "libcapsicum sandbox binary");

	while (1) {
		fdcount = 2;
		if (lcs_recvrpc_rights(lchp, &opno, &seqno, &buffer, &len,
		    fdarray, &fdcount) < 0) {
			if (errno == EPIPE)
				exit(-1);
			else
				err(-1, "lcs_recvrpc_rights");
		}
		switch (opno) {
		case PROXIED_GZ_COMPRESS:
			if (fdcount != 2)
				errx(-1, "sandbox_workloop: %d fds", fdcount);
			sandbox_gz_compress_buffer(lchp, opno, seqno, buffer,
			    len, fdarray[0], fdarray[1]);
			close(fdarray[0]);
			close(fdarray[1]);
			break;

		case PROXIED_GZ_UNCOMPRESS:
			if (fdcount != 2)
				errx(-1, "sandbox_workloop: %d fds", fdcount);
			sandbox_gz_uncompress_buffer(lchp, opno, seqno,
			    buffer, len, fdarray[0], fdarray[1]);
			close(fdarray[0]);
			close(fdarray[1]);
			break;

		case PROXIED_UNBZIP2:
			if (fdcount != 2)
				errx(-1, "sandbox_workloop: %d fds", fdcount);
			sandbox_unbzip2_buffer(lchp, opno, seqno, buffer, len,
			    fdarray[0], fdarray[1]);
			close(fdarray[0]);
			close(fdarray[1]);
			break;

		default:
			errx(-1, "sandbox_workloop: unknown op %d", opno);
		}
		free(buffer);
	}
}

#else /* NO_SANDBOX_SUPPORT */

off_t
gz_compress_wrapper(int in, int out, off_t *gsizep, const char *origname,
    uint32_t mtime)
{

	return (gz_compress(in, out, gsizep, origname, mtime));
}

off_t
gz_uncompress_wrapper(int in, int out, char *pre, size_t prelen,
    off_t *gsizep, const char *filename)
{

	return (gz_uncompress(in, out, pre, prelen, gsizep, filename));
}

off_t
unbzip2_wrapper(int in, int out, char *pre, size_t prelen, off_t *bytes_in)
{

	return (unbzip2(in, out, pre, prelen, bytes_in));
}

#endif /* !NO_SANDBOX_SUPPORT */
