/*	$OpenBSD: monitor.h,v 1.6 2002/06/11 05:46:20 mpech Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MONITOR_H_
#define _MONITOR_H_

enum monitor_reqtype {
	MONITOR_REQ_MODULI, MONITOR_ANS_MODULI,
	MONITOR_REQ_FREE, MONITOR_REQ_AUTHSERV,
	MONITOR_REQ_SIGN, MONITOR_ANS_SIGN,
	MONITOR_REQ_PWNAM, MONITOR_ANS_PWNAM,
	MONITOR_REQ_AUTH2_READ_BANNER, MONITOR_ANS_AUTH2_READ_BANNER,
	MONITOR_REQ_AUTHPASSWORD, MONITOR_ANS_AUTHPASSWORD,
	MONITOR_REQ_BSDAUTHQUERY, MONITOR_ANS_BSDAUTHQUERY,
	MONITOR_REQ_BSDAUTHRESPOND, MONITOR_ANS_BSDAUTHRESPOND,
	MONITOR_REQ_SKEYQUERY, MONITOR_ANS_SKEYQUERY,
	MONITOR_REQ_SKEYRESPOND, MONITOR_ANS_SKEYRESPOND,
	MONITOR_REQ_KEYALLOWED, MONITOR_ANS_KEYALLOWED,
	MONITOR_REQ_KEYVERIFY, MONITOR_ANS_KEYVERIFY,
	MONITOR_REQ_KEYEXPORT,
	MONITOR_REQ_PTY, MONITOR_ANS_PTY,
	MONITOR_REQ_PTYCLEANUP,
	MONITOR_REQ_SESSKEY, MONITOR_ANS_SESSKEY,
	MONITOR_REQ_SESSID,
	MONITOR_REQ_RSAKEYALLOWED, MONITOR_ANS_RSAKEYALLOWED,
	MONITOR_REQ_RSACHALLENGE, MONITOR_ANS_RSACHALLENGE,
	MONITOR_REQ_RSARESPONSE, MONITOR_ANS_RSARESPONSE,
	MONITOR_REQ_PAM_START,
	MONITOR_REQ_PAM_INIT_CTX, MONITOR_ANS_PAM_INIT_CTX,
	MONITOR_REQ_PAM_QUERY, MONITOR_ANS_PAM_QUERY,
	MONITOR_REQ_PAM_RESPOND, MONITOR_ANS_PAM_RESPOND,
	MONITOR_REQ_PAM_FREE_CTX, MONITOR_ANS_PAM_FREE_CTX,
	MONITOR_REQ_TERM
};

struct mm_master;
struct monitor {
	int			 m_recvfd;
	int			 m_sendfd;
	struct mm_master	*m_zback;
	struct mm_master	*m_zlib;
	struct Kex		**m_pkex;
	pid_t			 m_pid;
};

struct monitor *monitor_init(void);
void monitor_reinit(struct monitor *);
void monitor_sync(struct monitor *);

struct Authctxt;
struct Authctxt *monitor_child_preauth(struct monitor *);
void monitor_child_postauth(struct monitor *);

struct mon_table;
int monitor_read(struct monitor*, struct mon_table *, struct mon_table **);

/* Prototypes for request sending and receiving */
void mm_request_send(int, enum monitor_reqtype, Buffer *);
void mm_request_receive(int, Buffer *);
void mm_request_receive_expect(int, enum monitor_reqtype, Buffer *);

#endif /* _MONITOR_H_ */
