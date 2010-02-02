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
 * $P4: //depot/projects/trustedbsd/capabilities/src/lib/libcapsicum/libcapsicum_sandbox_api.h#3 $
 */

#ifndef _LIBCAPSICUM_SANDBOX_API_H_
#define	_LIBCAPSICUM_SANDBOX_API_H_

/*
 * This include file captures the assumptions libcapsicum sandboxs will
 * make about the runtime environment set up by libcapsicum hosts.
 */
#define	LIBCAPSICUM_SANDBOX_API_ENV	"LIBCAPSICUM_SANDBOX"
#define LIBCAPSICUM_SANDBOX_FDLIST	"LIBCAPSICUM_FDLIST"
#define	LIBCAPSICUM_SANDBOX_API_SOCK	"sock"

/*
 * Maximum number of file descriptor rights we will ever send as part of an
 * RPC.
 */
#define	LIBCAPSICUM_SANDBOX_API_MAXRIGHTS	16

/*
 * Simple libcapsicum RPC facility (lcrpc) definitions.
 */
#define	LCRPC_REQUEST_HDR_MAGIC	0x29ee2d7eb9143d98
struct lcrpc_request_hdr {
	u_int64_t	lcrpc_reqhdr_magic;
	u_int32_t	lcrpc_reqhdr_seqno;
	u_int32_t	lcrpc_reqhdr_opno;
	u_int64_t	lcrpc_reqhdr_datalen;
	u_int64_t	lcrpc_reqhdr_maxrepdatalen;
	u_int64_t	_lcrpc_reqhdr_spare3;
	u_int64_t	_lcrpc_reqhdr_spare2;
	u_int64_t	_lcrpc_reqhdr_spare1;
	u_int64_t	_lcrpc_reqhdr_spare0;
} __packed;

#define	LCRPC_REPLY_HDR_MAGIC	0x37cc2e29f5cce29b
struct lcrpc_reply_hdr {
	u_int64_t	lcrpc_rephdr_magic;
	u_int32_t	lcrpc_rephdr_seqno;
	u_int32_t	lcrpc_rephdr_opno;
	u_int64_t	lcrpc_rephdr_datalen;
	u_int64_t	_lcrpc_rephdr_spare4;
	u_int64_t	_lcrpc_rephdr_spare3;
	u_int64_t	_lcrpc_rephdr_spare2;
	u_int64_t	_lcrpc_rephdr_spare1;
	u_int64_t	_lcrpc_rephdr_spare0;
} __packed;

#endif /* !_LIBCAPSICUM_H_ */
