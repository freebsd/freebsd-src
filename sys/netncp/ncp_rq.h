/*
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
 * $FreeBSD$
 */
#ifndef _NETNCP_NCP_RQ_H_
#define _NETNCP_NCP_RQ_H_

#include <machine/endian.h>

#define getb(buf,ofs) 		(((const u_int8_t *)(buf))[ofs])
#define setb(buf,ofs,val)	(((u_int8_t*)(buf))[ofs])=val
#define getbw(buf,ofs)		((u_int16_t)(getb(buf,ofs)))

#if (BYTE_ORDER == LITTLE_ENDIAN)

#define getwle(buf,ofs) (*((u_int16_t*)(&((u_int8_t*)(buf))[ofs])))
#define getdle(buf,ofs) (*((u_int32_t*)(&((u_int8_t*)(buf))[ofs])))
#define getwbe(buf,ofs) (ntohs(getwle(buf,ofs)))
#define getdbe(buf,ofs) (ntohl(getdle(buf,ofs)))

#define setwle(buf,ofs,val) getwle(buf,ofs)=val
#define setwbe(buf,ofs,val) getwle(buf,ofs)=htons(val)
#define setdle(buf,ofs,val) getdle(buf,ofs)=val
#define setdbe(buf,ofs,val) getdle(buf,ofs)=htonl(val)

#define htoles(x)	((u_int16_t)(x))
#define letohs(x)	((u_int16_t)(x))
#define	htolel(x)	((u_int32_t)(x))
#define	letohl(x)	((u_int32_t)(x))

#else
#error "Macros for Big-Endians are incomplete"
#define getwle(buf,ofs) ((u_int16_t)(getb(buf, ofs) | (getb(buf, ofs + 1) << 8)))
#define getdle(buf,ofs) ((u_int32_t)(getb(buf, ofs) | \
				    (getb(buf, ofs + 1) << 8) | \
				    (getb(buf, ofs + 2) << 16) | \
				    (getb(buf, ofs + 3) << 24)))
#define getwbe(buf,ofs) (*((u_int16_t*)(&((u_int8_t*)(buf))[ofs])))
#define getdbe(buf,ofs) (*((u_int32_t*)(&((u_int8_t*)(buf))[ofs])))
/*
#define setwle(buf,ofs,val) getwle(buf,ofs)=val
#define setdle(buf,ofs,val) getdle(buf,ofs)=val
*/
#define setwbe(buf,ofs,val) getwle(buf,ofs)=val
#define setdbe(buf,ofs,val) getdle(buf,ofs)=val
/*
#define htoles(x)	((u_int16_t)(x))
#define letohs(x)	((u_int16_t)(x))
#define	htolel(x)	((u_int32_t)(x))
#define	letohl(x)	((u_int32_t)(x))
*/
#endif


#ifdef _KERNEL
struct ncp_nlstables;
/* 
 * Structure to prepare ncp request and receive reply 
 */
struct ncp_rq {
	struct ncp_conn	*conn;		/* back link */
	struct mbuf	*rq;
	struct mbuf	*mrq;
	struct mbuf	*rp;
	struct mbuf	*mrp;
	caddr_t		bpos;
/*	int		rqsize;*/		/* request size without ncp header */
	int		rpsize;		/* reply size minus ncp header */
	int		cc;		/* completion code */
	int		cs;		/* connection state */
	struct proc	*p;		/* proc that did rq */
	struct ucred	*cred;		/* user that did rq */
	int		rexmit;
};

#define DECLARE_RQ	struct ncp_rq rq;struct ncp_rq *rqp=&rq


int  ncp_rq_head(struct ncp_rq *rqp,u_int32_t ptype, u_int8_t fn,struct proc *p,
    struct ucred *cred);
int  ncp_rq_done(struct ncp_rq *rqp);

/* common case for normal request */
#define	ncp_rq_init(rqp,fn,p,c)	ncp_rq_head((rqp),NCP_REQUEST,(fn),(p),(c))
#define	ncp_rq_close(rqp)	ncp_rq_done((rqp))

#define NCP_RQ_HEAD(fn,p,c)	ncp_rq_init(rqp,fn,p,c)
#define	NCP_RQ_HEAD_S(fn,sfn,p,c)	NCP_RQ_HEAD(fn,p,c);ncp_rq_word(rqp,0);ncp_rq_byte(rqp,(sfn))
#define NCP_RQ_EXIT	bad: ncp_rq_close(rqp)
#define NCP_RQ_EXIT_NB	ncp_rq_close(rqp)
#define ncp_rq_word	ncp_rq_word_lh
#define ncp_rq_dword	ncp_rq_dword_lh

/*void ncp_init_request(struct ncp_rq *rqp, int fn);
void ncp_close_request(struct ncp_rq *rqp);*/
void ncp_rq_byte(struct ncp_rq *rqp, u_int8_t x);
void ncp_rq_word_hl(struct ncp_rq *rqp, u_int16_t x);
void ncp_rq_word_lh(struct ncp_rq *rqp, u_int16_t x);
void ncp_rq_dword_lh(struct ncp_rq *rqp, u_int32_t x);
static void ncp_rq_mem(struct ncp_rq *rqp, caddr_t source, int size);
static int  ncp_rq_usermem(struct ncp_rq *rqp, caddr_t source, int size);
int  ncp_rq_mbuf(struct ncp_rq *rqp, struct mbuf *m, int size);
int  ncp_rq_putanymem(struct ncp_rq *rqp, caddr_t source, int size,int type);
void ncp_rq_pathstring(struct ncp_rq *rqp, int size, char *name, struct ncp_nlstables*);
void ncp_rq_dbase_path(struct ncp_rq *, u_int8_t vol_num,
		    u_int32_t dir_base, int namelen, u_char *name, struct ncp_nlstables *nt);
void ncp_rq_pstring(struct ncp_rq *rqp, char *s);

u_int8_t  ncp_rp_byte(struct ncp_rq *rqp);
u_int16_t ncp_rp_word_hl(struct ncp_rq *rqp);
u_int16_t ncp_rp_word_lh(struct ncp_rq *rqp);
u_int32_t ncp_rp_dword_hl(struct ncp_rq *rqp);
u_int32_t ncp_rp_dword_lh(struct ncp_rq *rqp);
void      ncp_rp_mem(struct ncp_rq *rqp,caddr_t target, int size);
int       ncp_rp_usermem(struct ncp_rq *rqp,caddr_t target, int size);
int nwfs_mbuftouio(struct mbuf **mrep, struct uio *uiop, int siz, caddr_t *dpos);
int nwfs_uiotombuf(struct uio *uiop, struct mbuf **mq, int siz, caddr_t *bpos);
struct mbuf* ncp_rp_mbuf(struct ncp_rq *rqp, int size);

static void __inline ncp_rq_mem(struct ncp_rq *rqp, caddr_t source, int size) {
	ncp_rq_putanymem(rqp,source,size,0);
}
static int __inline ncp_rq_usermem(struct ncp_rq *rqp, caddr_t source, int size) {
	return ncp_rq_putanymem(rqp,source,size,1);
}
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
