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
 * $P4: //depot/projects/trustedbsd/capabilities/src/lib/libcapsicum/libcapsicum_host_io.c#2 $
 */

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/procdesc.h>
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

/*
 * Simple I/O wrappers for capability sockets.  Possibly more keeping an eye
 * on the worker should take place here.
 */
ssize_t
lch_send(struct lc_sandbox *lcsp, const void *msg, size_t len, int flags)
{

	return (_lc_send(lcsp->lcs_fd_sock, msg, len, flags, 0));
}

ssize_t
lch_send_rights(struct lc_sandbox *lcsp, const void *msg, size_t len,
    int flags, int *fdp, int fdcount)
{

	return (_lc_send_rights(lcsp->lcs_fd_sock, msg, len, flags, 0, fdp,
	    fdcount));
}

ssize_t
lch_recv(struct lc_sandbox *lcsp, void *buf, size_t len, int flags)
{

	return (_lc_recv(lcsp->lcs_fd_sock, buf, len, flags, 0));
}

ssize_t
lch_recv_rights(struct lc_sandbox *lcsp, void *buf, size_t len, int flags,
    int *fdp, int *fdcountp)
{

	return (_lc_recv_rights(lcsp->lcs_fd_sock, buf, len, flags, 0, fdp,
	    fdcountp));
}

/*
 * Simple libcapsicum RPC facility (lcrpc): send a request, get back a
 * reply (up to the size bound of the buffers passed in).  The caller is
 * responsible for retransmitting if the sandbox fails.
 *
 * Right now sequence numbers are unimplemented -- that's fine because we
 * don't need retransmission, and are synchronous.  However, it might not be
 * a bad idea to use them anyway.
 */
static int
lch_rpc_internal(struct lc_sandbox *lcsp, u_int32_t opno, struct iovec *req,
    int reqcount, int *req_fdp, int req_fdcount, struct iovec *rep,
    int repcount, size_t *replenp, int *rep_fdp, int *rep_fdcountp)
{
	struct lcrpc_request_hdr req_hdr;
	struct lcrpc_reply_hdr rep_hdr;
	size_t left, off, space, totlen, want;
	ssize_t len;
	int i;

	bzero(&req_hdr, sizeof(req_hdr));
	req_hdr.lcrpc_reqhdr_magic = LCRPC_REQUEST_HDR_MAGIC;
	req_hdr.lcrpc_reqhdr_seqno = 0;
	req_hdr.lcrpc_reqhdr_opno = opno;
	for (i = 0; i < reqcount; i++)
		req_hdr.lcrpc_reqhdr_datalen += req[i].iov_len;
	for (i = 0; i < repcount; i++)
		req_hdr.lcrpc_reqhdr_maxrepdatalen += rep[i].iov_len;

	/*
	 * Send our header.
	 */
	if (req_fdp != NULL)
		len = _lc_send_rights(lcsp->lcs_fd_sock, &req_hdr,
		    sizeof(req_hdr), 0, LC_IGNOREEINTR, req_fdp,
		    req_fdcount);
	else
		len = _lc_send(lcsp->lcs_fd_sock, &req_hdr, sizeof(req_hdr),
		    0, LC_IGNOREEINTR);
	if (len < 0)
		return (-1);
	if (len != sizeof(req_hdr)) {
		errno = ECHILD;
		return (-1);
	}

	/*
	 * Send the user request.
	 */
	for (i = 0; i < reqcount; i++) {
		len = _lc_send(lcsp->lcs_fd_sock, req[i].iov_base,
		    req[i].iov_len, 0, LC_IGNOREEINTR);
		if (len < 0)
			return (-1);
		if ((size_t)len != req[i].iov_len) {
			errno = ECHILD;
			return (-1);
		}
	}

	/*
	 * Receive our header and validate.
	 */
	if (rep_fdp != NULL)
		len = _lc_recv_rights(lcsp->lcs_fd_sock, &rep_hdr,
		    sizeof(rep_hdr), MSG_WAITALL, LC_IGNOREEINTR, rep_fdp,
		    rep_fdcountp);
	else
		len = _lc_recv(lcsp->lcs_fd_sock, &rep_hdr, sizeof(rep_hdr),
		    MSG_WAITALL, LC_IGNOREEINTR);
	if (len < 0)
		return (-1);
	if (len != sizeof(rep_hdr)) {
		if (rep_fdp != NULL)
			_lc_dispose_rights(rep_fdp, *rep_fdcountp);
		errno = ECHILD;
		return (-1);
	}

	if (rep_hdr.lcrpc_rephdr_magic != LCRPC_REPLY_HDR_MAGIC ||
	    rep_hdr.lcrpc_rephdr_seqno != 0 ||
	    rep_hdr.lcrpc_rephdr_opno != opno ||
	    rep_hdr.lcrpc_rephdr_datalen > req_hdr.lcrpc_reqhdr_maxrepdatalen) {
		if (rep_fdp != NULL)
			_lc_dispose_rights(rep_fdp, *rep_fdcountp);
		errno = EBADRPC;
		return (-1);
	}

	/*
	 * Receive the user data.  Notice that we can partially overwrite the
	 * user buffer but still receive an error.
	 */
	totlen = 0;
	for (i = 0; i < repcount; i++) {
		off = 0;
		while (totlen < rep_hdr.lcrpc_rephdr_datalen) {
			space = rep[i].iov_len - off;
			left = rep_hdr.lcrpc_rephdr_datalen - totlen;
			want = (space > left) ? space : left;
			len = _lc_recv(lcsp->lcs_fd_sock,
			    (u_char *)((uintptr_t)rep[i].iov_base + off),
			    want, MSG_WAITALL, LC_IGNOREEINTR);
			if (len < 0)
				return (-1);
			if ((size_t)len != want) {
				if (rep_fdp != NULL)
					_lc_dispose_rights(rep_fdp,
					    *rep_fdcountp);
				errno = ECHILD;
				return (-1);
			}
			off += len;
			totlen += len;
			if (rep[i].iov_len == off)
				break;
		}
		if (totlen == rep_hdr.lcrpc_rephdr_datalen)
			break;
	}
	*replenp = totlen;
	return (0);
}

int
lch_rpc(struct lc_sandbox *lcsp, u_int32_t opno, struct iovec *req,
    int reqcount, struct iovec *rep, int repcount, size_t *replenp)
{

	return (lch_rpc_internal(lcsp, opno, req, reqcount, NULL, 0,
	    rep, repcount, replenp, NULL, NULL));
}

int
lch_rpc_rights(struct lc_sandbox *lcsp, u_int32_t opno, struct iovec *req,
    int reqcount, int *req_fdp, int req_fdcount, struct iovec *rep,
    int repcount, size_t *replenp, int *rep_fdp, int *rep_fdcountp)
{

	return (lch_rpc_internal(lcsp, opno, req, reqcount, req_fdp,
	    req_fdcount, rep, repcount, replenp, rep_fdp, rep_fdcountp));
}
