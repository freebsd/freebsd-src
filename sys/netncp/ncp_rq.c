/*
 * Copyright (c) 1999-2001 Boris Popov
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
 * Routines to prepare request and fetch reply
 *
 * $FreeBSD$
 */ 
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_rq.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_sock.h>
#include <netncp/ncp_nls.h>

static MALLOC_DEFINE(M_NCPRQ, "NCPRQ", "NCP request");

static int ncp_sign_packet(struct ncp_conn *conn, struct ncp_rq *rqp, int *size);

int
ncp_rq_alloc_any(u_int32_t ptype, u_int8_t fn, struct ncp_conn *ncp,
	struct proc *p, struct ucred *cred,
	struct ncp_rq **rqpp)
{
	struct ncp_rq *rqp;
	int error;

	MALLOC(rqp, struct ncp_rq *, sizeof(*rqp), M_NCPRQ, M_WAITOK);
	error = ncp_rq_init_any(rqp, ptype, fn, ncp, p, cred);
	rqp->nr_flags |= NCPR_ALLOCED;
	if (error) {
		ncp_rq_done(rqp);
		return error;
	}
	*rqpp = rqp;
	return 0;
}

int
ncp_rq_alloc(u_int8_t fn, struct ncp_conn *ncp,
	struct proc *p, struct ucred *cred, struct ncp_rq **rqpp)
{
	return ncp_rq_alloc_any(NCP_REQUEST, fn, ncp, p, cred, rqpp);
}

int
ncp_rq_alloc_subfn(u_int8_t fn, u_int8_t subfn, struct ncp_conn *ncp,
	struct proc *p,	struct ucred *cred, struct ncp_rq **rqpp)
{
	struct ncp_rq *rqp;
	int error;

	error = ncp_rq_alloc_any(NCP_REQUEST, fn, ncp, p, cred, &rqp);
	if (error)
		return error;
	mb_reserve(&rqp->rq, 2);
	mb_put_uint8(&rqp->rq, subfn);
	*rqpp = rqp;
	return 0;
}

int
ncp_rq_init_any(struct ncp_rq *rqp, u_int32_t ptype, u_int8_t fn,
	struct ncp_conn *ncp,
	struct proc *p,	struct ucred *cred)
{
	struct ncp_rqhdr *rq;
	struct ncp_bursthdr *brq;
	struct mbchain *mbp;
	int error;

	bzero(rqp, sizeof(*rqp));
	error = ncp_conn_access(ncp, cred, NCPM_EXECUTE);
	if (error)
		return error;
	rqp->nr_p = p;
	rqp->nr_cred = cred;
	rqp->nr_conn = ncp;
	mbp = &rqp->rq;
	if (mb_init(mbp) != 0)
		return ENOBUFS;
	switch(ptype) {
	    case NCP_PACKET_BURST:
		brq = (struct ncp_bursthdr*)mb_reserve(mbp, sizeof(*brq));
		brq->bh_type = ptype;
		brq->bh_streamtype = 0x2;
		break;
	    default:
		rq = (struct ncp_rqhdr*)mb_reserve(mbp, sizeof(*rq));
		rq->type = ptype;
		rq->seq = 0;	/* filled later */
		rq->fn = fn;
		break;
	}
	rqp->nr_minrplen = -1;
	return 0;
}

void
ncp_rq_done(struct ncp_rq *rqp)
{
	mb_done(&rqp->rq);
	md_done(&rqp->rp);
	if (rqp->nr_flags & NCPR_ALLOCED)
		free(rqp, M_NCPRQ);
	return;
}

/*
 * Routines to fill the request
 */

static int
ncp_rq_pathstrhelp(struct mbchain *mbp, c_caddr_t src, caddr_t dst, int len)
{
	ncp_pathcopy(src, dst, len, mbp->mb_udata);
	return 0;
}

int
ncp_rq_pathstring(struct ncp_rq *rqp, int size, const char *name,
	struct ncp_nlstables *nt)
{
	struct mbchain *mbp = &rqp->rq;

	mb_put_uint8(mbp, size);
	mbp->mb_copy = ncp_rq_pathstrhelp;
	mbp->mb_udata = nt;
	return mb_put_mem(mbp, (c_caddr_t)name, size, MB_MCUSTOM);
}

int 
ncp_rq_pstring(struct ncp_rq *rqp, const char *s)
{
	u_int len = strlen(s);
	int error;

	if (len > 255)
		return EINVAL;
	error = mb_put_uint8(&rqp->rq, len);
	if (error)
		return error;
	return mb_put_mem(&rqp->rq, s, len, MB_MSYSTEM);
}

int 
ncp_rq_dbase_path(struct ncp_rq *rqp, u_int8_t vol_num, u_int32_t dir_base,
                    int namelen, u_char *path, struct ncp_nlstables *nt)
{
	struct mbchain *mbp = &rqp->rq;
	int complen;

	mb_put_uint8(mbp, vol_num);
	mb_put_mem(mbp, (c_caddr_t)&dir_base, sizeof(dir_base), MB_MSYSTEM);
	mb_put_uint8(mbp, 1);	/* with dirbase */
	if (path != NULL && path[0]) {
		if (namelen < 0) {
			namelen = *path++;
			mb_put_uint8(mbp, namelen);
			for(; namelen; namelen--) {
				complen = *path++;
				mb_put_uint8(mbp, complen);
				mb_put_mem(mbp, path, complen, MB_MSYSTEM);
				path += complen;
			}
		} else {
			mb_put_uint8(mbp, 1);	/* 1 component */
			ncp_rq_pathstring(rqp, namelen, path, nt);
		}
	} else {
		mb_put_uint8(mbp, 0);
		mb_put_uint8(mbp, 0);
	}
	return 0;
}

/* 
 * Make a signature for the current packet and add it at the end of the
 * packet.
 */
static int
ncp_sign_packet(struct ncp_conn *conn, struct ncp_rq *rqp, int *size)
{
	u_char data[64];
	int error;

	bzero(data, sizeof(data));
	bcopy(conn->sign_root, data, 8);
	setdle(data, 8, *size);
	m_copydata(rqp->rq.mb_top, sizeof(struct ncp_rqhdr) - 1,
		min((*size) - sizeof(struct ncp_rqhdr)+1, 52), data + 12);
	ncp_sign(conn->sign_state, data, conn->sign_state);
	error = mb_put_mem(&rqp->rq, (caddr_t)conn->sign_state, 8, MB_MSYSTEM);
	if (error)
		return error;
	(*size) += 8;
	return 0;
}

/*
 * Low level send rpc, here we do not attempt to restore any connection,
 * Connection expected to be locked
 */
int 
ncp_request_int(struct ncp_rq *rqp)
{
	struct ncp_conn *conn = rqp->nr_conn;
	struct proc *p = conn->procp;
	struct socket *so = conn->ncp_so;
	struct ncp_rqhdr *rq;
	struct ncp_rphdr *rp=NULL;
	struct timeval tv;
	struct mbuf *m, *mreply = NULL;
	struct mbchain *mbp;
	int error, len, dosend, plen = 0, gotpacket;

	if (so == NULL) {
		printf("%s: ncp_so is NULL !\n",__FUNCTION__);
		ncp_conn_invalidate(conn);
		return ENOTCONN;
	}
	if (p == NULL)
		p = curproc;	/* XXX maybe procpage ? */
	/*
	 * Flush out replies on previous reqs
	 */
	while (ncp_poll(so, POLLIN) != 0) {
		if (ncp_sock_recv(so, &m, &len) != 0)
			break;
		m_freem(m);
	}
	mbp = &rqp->rq;
	len = mb_fixhdr(mbp);
	rq = mtod(mbp->mb_top, struct ncp_rqhdr *);
	rq->seq = conn->seq;
	m = rqp->rq.mb_top;

	switch (rq->fn) {
	    case 0x15: case 0x16: case 0x17: case 0x23:
		*(u_int16_t*)(rq + 1) = htons(len - 2 - sizeof(*rq));
		break;
	}
	if (conn->flags & NCPFL_SIGNACTIVE) {
		error = ncp_sign_packet(conn, rqp, &len);
		if (error)
			return error;
		mbp->mb_top->m_pkthdr.len = len;
	}
	rq->conn_low = conn->connid & 0xff;
	/* rq->task = p->p_pgrp->pg_id & 0xff; */ /*p->p_pid*/
	/* XXX: this is temporary fix till I find a better solution */
	rq->task = rq->conn_low;
	rq->conn_high = conn->connid >> 8;
	rqp->rexmit = conn->li.retry_count;
	error = 0;
	for(dosend = 1;;) {
		if (rqp->rexmit-- == 0) {
			error = ETIMEDOUT;
			break;
		}
		error = 0;
		if (dosend) {
			NCPSDEBUG("send:%04x f=%02x c=%d l=%d s=%d t=%d\n",rq->type, rq->fn, (rq->conn_high << 8) + rq->conn_low,
				mbp->mb_top->m_pkthdr.len, rq->seq, rq->task
			);
			error = ncp_sock_send(so, mbp->mb_top, rqp);
			if (error)
				break;
		}
		tv.tv_sec = conn->li.timeout;
		tv.tv_usec = 0;
		error = ncp_sock_rselect(so, p, &tv, POLLIN);
		if (error == EWOULDBLOCK )	/* timeout expired */
			continue;
		error = ncp_chkintr(conn, p);
		if (error)
			break;
		/*
		 * At this point it is possible to get more than one
		 * reply from server. In general, last reply should be for
		 * current request, but not always. So, we loop through
		 * all replies to find the right answer and flush others.
		 */
		gotpacket = 0;	/* nothing good found */
		dosend = 1;	/* resend rq if error */
		for (;;) {
			error = 0;
			if (ncp_poll(so, POLLIN) == 0)
				break;
/*			if (so->so_rcv.sb_cc == 0) {
				break;
			}*/
			error = ncp_sock_recv(so, &m, &len);
			if (error)
				break; 		/* must be more checks !!! */
			if (m->m_len < sizeof(*rp)) {
				m = m_pullup(m, sizeof(*rp));
				if (m == NULL) {
					printf("%s: reply too short\n",__FUNCTION__);
					continue;
				}
			}
			rp = mtod(m, struct ncp_rphdr*);
			if (len == sizeof(*rp) && rp->type == NCP_POSITIVE_ACK) {
				NCPSDEBUG("got positive acknowledge\n");
				m_freem(m);
				rqp->rexmit = conn->li.retry_count;
				dosend = 0;	/* server just busy and will reply ASAP */
				continue;
			}
			NCPSDEBUG("recv:%04x c=%d l=%d s=%d t=%d cc=%02x cs=%02x\n",rp->type,
			    (rp->conn_high << 8) + rp->conn_low, len, rp->seq, rp->task,
			     rp->completion_code, rp->connection_state);
			NCPDDEBUG(m);
			if ( (rp->type == NCP_REPLY) && 
			    ((rq->type == NCP_ALLOC_SLOT) || 
			    ((rp->conn_low == rq->conn_low) &&
			     (rp->conn_high == rq->conn_high)
			    ))) {
				if (rq->seq > rp->seq || (rq->seq == 0 && rp->seq == 0xff)) {
					dosend = 1;
				}
				if (rp->seq == rq->seq) {
					if (gotpacket) {
						m_freem(m);
					} else {
						gotpacket = 1;
						mreply = m;
						plen = len;
					}
					continue;	/* look up other for other packets */
				}
			}
			m_freem(m);
			NCPSDEBUG("reply mismatch\n");
		} /* for receive */
		if (error || gotpacket)
			break;
		/* try to resend, or just wait */
	}
	conn->seq++;
	if (error) {
		NCPSDEBUG("error=%d\n", error);
		/*
		 * Any error except interruped call means that we have
		 * to reconnect. So, eliminate future timeouts by invalidating
		 * connection now.
		 */
		if (error != EINTR)
			ncp_conn_invalidate(conn);
		return (error);
	}
	if (conn->flags & NCPFL_SIGNACTIVE) {
		/* XXX: check reply signature */
		m_adj(mreply, -8);
		plen -= 8;
	}
	rp = mtod(mreply, struct ncp_rphdr*);
	md_initm(&rqp->rp, mreply);
	rqp->nr_rpsize = plen - sizeof(*rp);
	rqp->nr_cc = error = rp->completion_code;
	if (error)
		error |= 0x8900;	/* server error */
	rqp->nr_cs = rp->connection_state;
	if (rqp->nr_cs & (NCP_CS_BAD_CONN | NCP_CS_SERVER_DOWN)) {
		NCPSDEBUG("server drop us\n");
		ncp_conn_invalidate(conn);
		error = ECONNRESET;
	}
	md_get_mem(&rqp->rp, NULL, sizeof(*rp), MB_MSYSTEM);
	return error;
}

/*
 * Here we will try to restore any loggedin & dropped connection,
 * connection should be locked on entry
 */
static __inline int
ncp_restore_login(struct ncp_conn *conn)
{
	int error;

	printf("ncprq: Restoring connection, flags = %x\n", conn->flags);
	conn->flags |= NCPFL_RESTORING;
	error = ncp_conn_reconnect(conn);
	if (!error && (conn->flags & NCPFL_WASLOGGED))
		error = ncp_conn_login(conn, conn->procp, conn->ucred);
	if (error)
		ncp_ncp_disconnect(conn);
	conn->flags &= ~NCPFL_RESTORING;
	return error;
}

int
ncp_request(struct ncp_rq *rqp)
{
	struct ncp_conn *ncp = rqp->nr_conn;
	int error, rcnt;

	error = ncp_conn_lock(ncp, rqp->nr_p, rqp->nr_cred, NCPM_EXECUTE);
	if (error)
		goto out;
	rcnt = NCP_RESTORE_COUNT;
	for(;;) {
		if (ncp->flags & NCPFL_ATTACHED) {
			error = ncp_request_int(rqp);
			if (ncp->flags & NCPFL_ATTACHED)
				break;
		}
		if (rcnt-- == 0) {
			error = ECONNRESET;
			break;
		}
		/*
		 * Do not attempt to restore connection recursively
		 */
		if (ncp->flags & NCPFL_RESTORING) {
			error = ENOTCONN;
			break;
		}
		error = ncp_restore_login(ncp);
		if (error)
			continue;
	}
	ncp_conn_unlock(ncp, rqp->nr_p);
out:
	if (error && (rqp->nr_flags & NCPR_DONTFREEONERR) == 0)
		ncp_rq_done(rqp);
	return error;
}
