/*
 * sdp-int.h
 *
 * Copyright (c) 2001-2003 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: sdp-int.h,v 1.1 2003/09/01 23:01:07 max Exp $
 * $FreeBSD$
 */

#ifndef _SDP_INT_H_
#define _SDP_INT_H_

__BEGIN_DECLS

#define	SDP_UNSOCK_PATH		"/var/run/sdp"
#define SDP_UNSOCK_MTU		2048

/*
 * SDP session
 */

struct sdp_session {
	u_int16_t	 flags;
#define SDP_SESSION_LOCAL	(1 << 0)
	u_int16_t	 tid;   /* current session transaction ID (tid) */
	u_int16_t	 omtu;  /* outgoing MTU (req buffer size) */
	u_int16_t	 imtu;  /* incoming MTU (rsp buffer size) */
	u_int8_t	*req;	/* request buffer (start) */
	u_int8_t	*req_e;	/* request buffer (end) */
	u_int8_t	*rsp;	/* response buffer (start) */
	u_int8_t	*rsp_e;	/* response buffer (end) */
	u_int32_t	 cslen; /* continuation state length */
	u_int8_t	 cs[16];/* continuation state */
	int32_t		 s;     /* L2CAP socket */
	int32_t		 error;	/* last error code */
};
typedef struct sdp_session	sdp_session_t;
typedef struct sdp_session *	sdp_session_p;

__END_DECLS

#endif /* ndef _SDP_INT_H_ */

