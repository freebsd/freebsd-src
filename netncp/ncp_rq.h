/*-
 * Copyright (c) 1999, 2000, 2001 Boris Popov
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
 * $FreeBSD$
 */
#ifndef _NETNCP_NCP_RQ_H_
#define _NETNCP_NCP_RQ_H_

#include <sys/endian.h>

#define getb(buf,ofs) 		(((const u_int8_t *)(buf))[ofs])
#define setb(buf,ofs,val)	(((u_int8_t*)(buf))[ofs])=val
#define getbw(buf,ofs)		((u_int16_t)(getb(buf,ofs)))

#define	getwle(buf,ofs) (le16toh(*((u_int16_t*)(&((u_int8_t*)(buf))[ofs]))))
#define	getdle(buf,ofs) (le32toh(*((u_int32_t*)(&((u_int8_t*)(buf))[ofs]))))
#define	getwbe(buf,ofs) (be16toh(*((u_int16_t*)(&((u_int8_t*)(buf))[ofs]))))
#define	getdbe(buf,ofs) (be32toh(*((u_int32_t*)(&((u_int8_t*)(buf))[ofs]))))

#define	setwle(buf,ofs,val) \
	(*((u_int16_t*)(&((u_int8_t*)(buf))[ofs])))=htole16(val)
#define	setdle(buf,ofs,val) \
	(*((u_int32_t*)(&((u_int8_t*)(buf))[ofs])))=htole32(val)
#define	setwbe(buf,ofs,val) \
	(*((u_int16_t*)(&((u_int8_t*)(buf))[ofs])))=htobe16(val)
#define	setdbe(buf,ofs,val) \
	(*((u_int32_t*)(&((u_int8_t*)(buf))[ofs])))=htobe32(val)

#ifdef _KERNEL

#include <sys/mchain.h>

#define	NCPR_ALLOCED		0x0001	/* request structure was allocated */
#define	NCPR_DONTFREEONERR	0x0002	/* do not free structure on error */

/* 
 * Structure to prepare ncp request and receive reply 
 */
struct ncp_rq {
	int		nr_flags;
	struct mbchain	rq;
	struct mdchain	rp;
	int		nr_minrplen;	/* minimal rp size (-1 if not known) */
	int		nr_rpsize;	/* reply size minus ncp header */
	int		nr_cc;		/* completion code */
	int		nr_cs;		/* connection state */
	struct thread *	nr_td;		/* thread that did rq */
	struct ucred *	nr_cred;	/* user that did rq */
	int		rexmit;
	struct ncp_conn*nr_conn;	/* back link */
};

int  ncp_rq_alloc(u_int8_t fn, struct ncp_conn *ncp, struct thread *td,
	struct ucred *cred, struct ncp_rq **rqpp);
int  ncp_rq_alloc_any(u_int32_t ptype, u_int8_t fn, struct ncp_conn *ncp,
	struct thread *td,	struct ucred *cred, struct ncp_rq **rqpp);
int  ncp_rq_alloc_subfn(u_int8_t fn, u_int8_t subfn, struct ncp_conn *ncp,
	struct thread *td,	struct ucred *cred, struct ncp_rq **rqpp);
int  ncp_rq_init_any(struct ncp_rq *rqp, u_int32_t ptype, u_int8_t fn,
	struct ncp_conn *ncp, 
	struct thread *td, struct ucred *cred);
void ncp_rq_done(struct ncp_rq *rqp);
int  ncp_request(struct ncp_rq *rqp);
int  ncp_request_int(struct ncp_rq *rqp);

struct ncp_nlstables;

int  ncp_rq_pathstring(struct ncp_rq *rqp, int size, const char *name, struct ncp_nlstables*);
int  ncp_rq_dbase_path(struct ncp_rq *, u_int8_t vol_num,
		    u_int32_t dir_base, int namelen, u_char *name, struct ncp_nlstables *nt);
int  ncp_rq_pstring(struct ncp_rq *rqp, const char *s);

void ncp_sign_init(const char *logindata, char *sign_root);

#else /* ifdef _KERNEL */

#define	DECLARE_RQ	struct ncp_buf conn1, *conn=&conn1

#define	ncp_add_byte(conn,x) (conn)->packet[(conn)->rqsize++]=x

struct ncp_buf;

__BEGIN_DECLS

void ncp_init_request(struct ncp_buf *);
void ncp_init_request_s(struct ncp_buf *, int);
void ncp_add_word_lh(struct ncp_buf *, u_int16_t);
void ncp_add_dword_lh(struct ncp_buf *, u_int32_t);
void ncp_add_word_hl(struct ncp_buf *, u_int16_t);
void ncp_add_dword_hl(struct ncp_buf *, u_int32_t);
void ncp_add_mem(struct ncp_buf *, const void *, int);
void ncp_add_mem_nls(struct ncp_buf *, const void *, int);
void ncp_add_pstring(struct ncp_buf *, const char *);
void ncp_add_handle_path(struct ncp_buf *, nuint32, nuint32, int, const char *);

#define ncp_reply_data(conn,offset) ((conn)->packet+offset)
#define ncp_reply_byte(conn,offset) (*(u_int8_t*)(ncp_reply_data(conn, offset)))

u_int16_t ncp_reply_word_hl(struct ncp_buf *, int);
u_int16_t ncp_reply_word_lh(struct ncp_buf *, int);
u_int32_t ncp_reply_dword_hl(struct ncp_buf *, int);
u_int32_t ncp_reply_dword_lh(struct ncp_buf *, int);

static __inline void
ConvertToNWfromDWORD(u_int32_t sfd, ncp_fh *fh) {
	fh->val1 = (fh->val.val32 = sfd);
	return;
}

__END_DECLS

#endif /* ifdef _KERNEL */

#endif	/* !_NETNCP_NCP_RQ_H_ */
