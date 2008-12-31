/*-
 * Copyright (c) 1999, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $FreeBSD: src/sys/netncp/ncp_ncp.h,v 1.8.18.1 2008/11/25 02:59:29 kensmith Exp $
 */
#ifndef _NETNCP_NCP_NCP_H_
#define _NETNCP_NCP_NCP_H_

#define NCP_ALLOC_SLOT		0x1111
#define NCP_REQUEST		0x2222
#define NCP_REPLY		0x3333
#define NCP_FREE_SLOT		0x5555
#define	NCP_PACKET_BURST	0x7777
#define NCP_POSITIVE_ACK	0x9999

/*
 * Bits for connection state field in ncp_rphdr
 */
#define	NCP_CS_BAD_CONN		0x01	/* no such connection */
#define	NCP_CS_NO_SLOTS		0x04	/* no connection slots available */
#define	NCP_CS_SERVER_DOWN	0x10	/* server in down state */
#define	NCP_CS_HAVE_BROADCAST	0x40	/* server holds broadcast for us */

#define NCP_RETRY_COUNT		5
#define	NCP_RETRY_TIMEOUT	10
#define	NCP_RESTORE_COUNT	2	/* how many times try to restore per 
					 * single request, should be an _even_ */

struct ncp_rqhdr {
	u_int16_t type;
	u_int8_t  seq;
	u_int8_t  conn_low;
	u_int8_t  task;
	u_int8_t  conn_high;
	u_int8_t  fn;
	u_int8_t  data[0];
} __packed;


struct ncp_rphdr {
	u_int16_t	type;
	u_int8_t	seq;
	u_int8_t	conn_low;
	u_int8_t	task;
	u_int8_t	conn_high;
	u_int8_t	completion_code;
	u_int8_t	connection_state;
	u_int8_t	data[0];
}__packed;

#define	BFL_ABT		0x04
#define	BFL_EOB		0x10
#define	BFL_SYS		0x80

#define	BOP_READ	1L
#define	BOP_WRITE	2L

#define	BERR_NONE	0
#define	BERR_INIT	1
#define	BERR_IO		2
#define	BERR_NODATA	3
#define	BERR_WRITE	4

struct ncp_bursthdr {
	u_short	bh_type;
	u_char	bh_flags;
	u_char	bh_streamtype;
	u_long	bh_srcid;
	u_long	bh_dstid;
	u_long	bh_seq;			/* HL */
	u_long	bh_send_delay;		/* HL */
	u_short	bh_bseq;		/* HL */
	u_short	bh_aseq;		/* HL */
	u_long	bh_blen;		/* HL */
	u_long	bh_dofs;		/* HL */
	u_short	bh_dlen;		/* HL */
	u_short	bh_misfrags;		/* HL */
} __packed;

struct ncp_conn;
struct ncp_conn_args;
struct ncp_rq;
struct ucred;

int  ncp_ncp_connect(struct ncp_conn *conn);
int  ncp_ncp_disconnect(struct ncp_conn *conn);
int  ncp_negotiate_buffersize(struct ncp_conn *conn, int size, int *target);
int  ncp_renegotiate_connparam(struct ncp_conn *conn, int buffsize,
	u_int8_t in_options);
int  ncp_get_bindery_object_id(struct ncp_conn *conn, 
		u_int16_t object_type, char *object_name, 
		struct ncp_bindery_object *target,
		struct thread *td,struct ucred *cred);
int  ncp_get_encryption_key(struct ncp_conn *conn, char *target);
int  ncp_login_encrypted(struct ncp_conn *conn,
	struct ncp_bindery_object *object,
	const u_char *key, const u_char *passwd,
	struct thread *td, struct ucred *cred);
int ncp_login_unencrypted(struct ncp_conn *conn, u_int16_t object_type, 
	const char *object_name, const u_char *passwd,
	struct thread *td, struct ucred *cred);
int  ncp_read(struct ncp_conn *conn, ncp_fh *file, struct uio *uiop, struct ucred *cred);
int  ncp_write(struct ncp_conn *conn, ncp_fh *file, struct uio *uiop, struct ucred *cred);

#endif /* _NCP_NCP_H_ */
