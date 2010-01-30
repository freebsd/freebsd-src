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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "libcapsicum.h"
#include "libcapsicum_internal.h"
#include "libcapsicum_sandbox_api.h"

ssize_t
lcs_recv(struct lc_host *lchp, void *buf, size_t len, int flags)
{

	return (_lc_recv(lchp->lch_fd_sock, buf, len, flags, 0));
}

ssize_t
lcs_recv_rights(struct lc_host *lchp, void *buf, size_t len, int flags,
    int *fdp, int *fdcountp)
{

	return (_lc_recv_rights(lchp->lch_fd_sock, buf, len, flags, 0, fdp,
	    fdcountp));
}

ssize_t
lcs_send(struct lc_host *lchp, const void *msg, size_t len, int flags)
{

	return (_lc_send(lchp->lch_fd_sock, msg, len, flags, 0));
}

ssize_t
lcs_send_rights(struct lc_host *lchp, const void *msg, size_t len,
    int flags, int *fdp, int fdcount)
{

	return (_lc_send_rights(lchp->lch_fd_sock, msg, len, flags, 0, fdp,
	    fdcount));
}

/*
 * libcapsicum RPC facility (lcrpc) sandbox routines.  Since arguments are
 * variable size, space is allocated by the RPC code rather than the caller,
 * who is expected to free it with free(3) if desired.
 */
static int
lcs_recvrpc_internal(struct lc_host *lchp, u_int32_t *opnop,
    u_int32_t *seqnop, u_char **bufferp, size_t *lenp, int *fdp,
    int *fdcountp)
{
	struct lcrpc_request_hdr req_hdr;
	size_t totlen;
	ssize_t len;
	u_char *buffer;
	int error;

	if (fdp != NULL)
		len = _lc_recv_rights(lchp->lch_fd_sock, &req_hdr,
		    sizeof(req_hdr), MSG_WAITALL, LC_IGNOREEINTR, fdp,
			    fdcountp);
	else
		len = _lc_recv(lchp->lch_fd_sock, &req_hdr, sizeof(req_hdr),
		    MSG_WAITALL, LC_IGNOREEINTR);
	if (len < 0)
		return (-1);
	if (len == 0) {
		if (fdp != NULL)
			_lc_dispose_rights(fdp, *fdcountp);
		errno = EPIPE;
		return (-1);
	}
	if (len != sizeof(req_hdr)) {
		if (fdp != NULL)
			_lc_dispose_rights(fdp, *fdcountp);
		errno = EBADMSG;
		return (-1);
	}

	if (req_hdr.lcrpc_reqhdr_magic != LCRPC_REQUEST_HDR_MAGIC) {
		if (fdp != NULL)
			_lc_dispose_rights(fdp, *fdcountp);
		errno = EBADMSG;
		return (-1);
	}

	/*
	 * XXXRW: Should we check that the receive data fits in the address
	 * space of the sandbox?
	 *
	 * XXXRW: If malloc() fails, we should drain the right amount of data
	 * from the socket so that the next RPC will succeed.  Possibly we
	 * should also reply with an error from this layer to the sender?
	 * What about if there are other socket errors, such as EINTR?
	 */
	buffer = malloc(req_hdr.lcrpc_reqhdr_datalen);
	if (buffer == NULL) {
		error = errno;
		if (fdp != NULL)
			_lc_dispose_rights(fdp, *fdcountp);
		errno = error;
		return (-1);
	}

	/*
	 * XXXRW: Likewise, how to handle failure at this stage?
	 */
	totlen = 0;
	while (totlen < req_hdr.lcrpc_reqhdr_datalen) {
		len = _lc_recv(lchp->lch_fd_sock, buffer + totlen,
		    req_hdr.lcrpc_reqhdr_datalen - totlen, MSG_WAITALL,
		    LC_IGNOREEINTR);
		if (len < 0) {
			error = errno;
			if (fdp != NULL)
				_lc_dispose_rights(fdp, *fdcountp);
			free(buffer);
			return (-1);
		}
		if (len == 0) {
			errno = EPIPE;
			if (fdp != NULL)
				_lc_dispose_rights(fdp, *fdcountp);
			free(buffer);
			return (-1);
		}
		totlen += len;
	}
	*bufferp = buffer;
	*lenp = totlen;
	*opnop = req_hdr.lcrpc_reqhdr_opno;
	*seqnop = req_hdr.lcrpc_reqhdr_seqno;
	return (0);
}

int
lcs_recvrpc(struct lc_host *lchp, u_int32_t *opnop, u_int32_t *seqnop,
    u_char **bufferp, size_t *lenp)
{

	return (lcs_recvrpc_internal(lchp, opnop, seqnop, bufferp, lenp,
	    NULL, NULL));
}

int
lcs_recvrpc_rights(struct lc_host *lchp, u_int32_t *opnop, u_int32_t *seqnop,
    u_char **bufferp, size_t *lenp, int *fdp, int *fdcountp)
{

	return (lcs_recvrpc_internal(lchp, opnop, seqnop, bufferp, lenp,
	    fdp, fdcountp));
}

static int
lcs_sendrpc_internal(struct lc_host *lchp, u_int32_t opno, u_int32_t seqno,
    struct iovec *rep, int repcount, int *fdp, int fdcount)
{
	struct lcrpc_reply_hdr rep_hdr;
	ssize_t len;
	int i;

	bzero(&rep_hdr, sizeof(rep_hdr));
	rep_hdr.lcrpc_rephdr_magic = LCRPC_REPLY_HDR_MAGIC;
	rep_hdr.lcrpc_rephdr_seqno = seqno;
	rep_hdr.lcrpc_rephdr_opno = opno;
	rep_hdr.lcrpc_rephdr_datalen = 0;
	for (i = 0; i < repcount; i++)
		rep_hdr.lcrpc_rephdr_datalen += rep[i].iov_len;

	/*
	 * Send our header.
	 */
	if (fdp != NULL)
		len = _lc_send_rights(lchp->lch_fd_sock, &rep_hdr,
		    sizeof(rep_hdr), 0, LC_IGNOREEINTR, fdp, fdcount);
	else
		len = _lc_send(lchp->lch_fd_sock, &rep_hdr, sizeof(rep_hdr),
		    0, LC_IGNOREEINTR);
	if (len < 0)
		return (-1);
	if (len != sizeof(rep_hdr)) {
		errno = EPIPE;
		return (-1);
	}

	/*
	 * Send user data.
	 */
	for (i = 0; i < repcount; i++) {
		len = _lc_send(lchp->lch_fd_sock, rep[i].iov_base,
		    rep[i].iov_len, 0, LC_IGNOREEINTR);
		if (len < 0)
			return (-1);
		if ((size_t)len != rep[i].iov_len) {
			errno = EPIPE;
			return (-1);
		}
	}
	return (0);
}

int
lcs_sendrpc(struct lc_host *lchp, u_int32_t opno, u_int32_t seqno,
    struct iovec *rep, int repcount)
{

	return (lcs_sendrpc_internal(lchp, opno, seqno, rep, repcount, NULL,
	    0));
}

int
lcs_sendrpc_rights(struct lc_host *lchp, u_int32_t opno, u_int32_t seqno,
    struct iovec *rep, int repcount, int *fdp, int fdcount)
{

	return (lcs_sendrpc_internal(lchp, opno, seqno, rep, repcount, fdp,
	    fdcount));
}
