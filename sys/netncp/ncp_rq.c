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
 * Routines to prepare request and fetch reply
 *
 * $FreeBSD$
 */ 
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/uio.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_rq.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_nls.h>

int
ncp_rq_head(struct ncp_rq *rqp, u_int32_t ptype, u_int8_t fn,struct proc *p,
    struct ucred *cred)
{
	struct mbuf *m;
	struct ncp_rqhdr *rq;
	struct ncp_bursthdr *brq;
	caddr_t pstart;

	bzero(rqp, sizeof(*rqp));
	rqp->p = p;
	rqp->cred = cred;
	m = m_gethdr(M_WAIT, MT_DATA);
	if (m == NULL) 
		return ENOBUFS;		/* if M_WAIT ? */
	m->m_pkthdr.rcvif = NULL;
	rqp->rq = rqp->mrq = m;
	rqp->rp = NULL;
	switch(ptype) {
	    case NCP_PACKET_BURST:
		MH_ALIGN(m, sizeof(*brq) + 24);
		m->m_len = sizeof(*brq);
		brq = mtod(m, struct ncp_bursthdr *);
		brq->bh_type = ptype;
		brq->bh_streamtype = 0x2;
		pstart = (caddr_t)brq;
		break;
	    default:
		MH_ALIGN(m, sizeof(*rq) + 2);	/* possible len field in some functions */
		m->m_len = sizeof(*rq);
		rq = mtod(m, struct ncp_rqhdr *);
		rq->type = ptype;
		rq->seq = 0;	/* filled later */
		rq->fn = fn;
		pstart = (caddr_t)rq;
		break;
	}
	rqp->bpos = pstart + m->m_len;
	return 0;
}

int
ncp_rq_done(struct ncp_rq *rqp) {
	m_freem(rqp->rq);
	rqp->rq=NULL;
	if (rqp->rp) m_freem(rqp->rp);
	rqp->rp=NULL;
	return (0);
}

/*
 * Routines to fill the request
 */
static caddr_t ncp_mchecksize(struct ncp_rq *rqp, int size);
#define	NCP_RQADD(t)	((t*)(ncp_mchecksize(rqp,sizeof(t))))

caddr_t
ncp_mchecksize(struct ncp_rq *rqp, int size) {
	caddr_t bpos1;

	if (size>MLEN)
		panic("ncp_mchecksize\n");
	if (M_TRAILINGSPACE(rqp->mrq)<(size)) {
		struct mbuf *m;
		m = m_get(M_WAIT, MT_DATA);
		m->m_len = 0;
		rqp->bpos = mtod(m, caddr_t);
		rqp->mrq->m_next = m;
		rqp->mrq = m;
	}
	rqp->mrq->m_len += size;
	bpos1 = rqp->bpos;
	rqp->bpos += size;
	return bpos1;
}

void
ncp_rq_byte(struct ncp_rq *rqp,u_int8_t x) {
	*NCP_RQADD(u_int8_t)=x;
}

void
ncp_rq_word_hl(struct ncp_rq *rqp, u_int16_t x) {
	setwbe(NCP_RQADD(u_int16_t), 0, x);
}

void
ncp_rq_word_lh(struct ncp_rq *rqp, u_int16_t x) {
	setwle(NCP_RQADD(u_int16_t), 0, x);
}

void
ncp_rq_dword_lh(struct ncp_rq *rqp, u_int32_t x) {
	setdle(NCP_RQADD(u_int32_t), 0, x);
}

void
ncp_rq_pathstring(struct ncp_rq *rqp, int size, char *name, struct ncp_nlstables *nt) {
	struct mbuf *m;
	int cplen;

	ncp_rq_byte(rqp, size);
	m = rqp->mrq;
	cplen = min(size, M_TRAILINGSPACE(m));
	if (cplen) {
		ncp_pathcopy(name, rqp->bpos, cplen, nt);
		size -= cplen;
		name += cplen;
		m->m_len += cplen;
	}
	if (size) {
		m = m_getm(m, size, MT_DATA, M_WAIT);
		while (size > 0){
			m = m->m_next;
			cplen = min(size, M_TRAILINGSPACE(m));
			ncp_pathcopy(name, mtod(m, caddr_t) + m->m_len, cplen, nt);
			size -= cplen;
			name += cplen;
			m->m_len += cplen;
		}
	}
	rqp->bpos = mtod(m,caddr_t) + m->m_len;
	rqp->mrq = m;
	return;
}

int
ncp_rq_putanymem(struct ncp_rq *rqp, caddr_t source, int size, int type) {
	struct mbuf *m;
	int cplen, error;

	m = rqp->mrq;
	cplen = min(size, M_TRAILINGSPACE(m));
	if (cplen) {
		if (type==1) {
			error = copyin(source, rqp->bpos, cplen);
			if (error) return error;
		} else
			bcopy(source, rqp->bpos, cplen);
		size -= cplen;
		source += cplen;
		m->m_len += cplen;
	}
	if (size) {
		m = m_getm(m, size, MT_DATA, M_WAIT);
		while (size > 0){
			m = m->m_next;
			cplen = min(size, M_TRAILINGSPACE(m));
			if (type==1) {
				error = copyin(source, mtod(m, caddr_t) + m->m_len, cplen);
				if (error) return error;
			} else
				bcopy(source, mtod(m, caddr_t) + m->m_len, cplen);
			size -= cplen;
			source += cplen;
			m->m_len += cplen;
		}
	}
	rqp->bpos = mtod(m,caddr_t) + m->m_len;
	rqp->mrq = m;
	return 0;
}

int
ncp_rq_mbuf(struct ncp_rq *rqp, struct mbuf *m, int size) {

	rqp->mrq->m_next = m;
	m->m_next = NULL;
	if (size != M_COPYALL) m->m_len = size;
	rqp->bpos = mtod(m,caddr_t) + m->m_len;
	rqp->mrq = m;
	return 0;
}

void 
ncp_rq_pstring(struct ncp_rq *rqp, char *s) {
	int len = strlen(s);
	if (len > 255) {
		nwfs_printf("string too long: %s\n", s);
		len = 255;
	}
	ncp_rq_byte(rqp, len);
	ncp_rq_mem(rqp, s, len);
	return;
}

void 
ncp_rq_dbase_path(struct ncp_rq *rqp, u_int8_t vol_num, u_int32_t dir_base,
                    int namelen, u_char *path, struct ncp_nlstables *nt)
{
	int complen;

	ncp_rq_byte(rqp, vol_num);
	ncp_rq_dword(rqp, dir_base);
	ncp_rq_byte(rqp, 1);	/* with dirbase */
	if (path != NULL && path[0]) {
		if (namelen < 0) {
			namelen = *path++;
			ncp_rq_byte(rqp, namelen);
			for(; namelen; namelen--) {
				complen = *path++;
				ncp_rq_byte(rqp, complen);
				ncp_rq_mem(rqp, path, complen);
				path += complen;
			}
		} else {
			ncp_rq_byte(rqp, 1);	/* 1 component */
			ncp_rq_pathstring(rqp, namelen, path, nt);
		}
	} else {
		ncp_rq_byte(rqp, 0);
		ncp_rq_byte(rqp, 0);
	}
}
/*
 * fetch reply routines
 */
#define ncp_mspaceleft	(mtod(rqp->mrp,caddr_t)+rqp->mrp->m_len-rqp->bpos)

u_int8_t
ncp_rp_byte(struct ncp_rq *rqp) {
	if (rqp->mrp == NULL) return 0;
	if (ncp_mspaceleft < 1) {
		rqp->mrp = rqp->mrp->m_next;
		if (rqp->mrp == NULL) return 0;
		rqp->bpos = mtod(rqp->mrp, caddr_t);
	}
	rqp->bpos += 1;
	return rqp->bpos[-1];
}

u_int16_t
ncp_rp_word_lh(struct ncp_rq *rqp) {
	caddr_t prev = rqp->bpos;
	u_int16_t t;

	if (rqp->mrp == NULL) return 0;
	if (ncp_mspaceleft >= 2) {
		rqp->bpos += 2;
		return getwle(prev,0);
	}
	t = *((u_int8_t*)(rqp->bpos));
	rqp->mrp = rqp->mrp->m_next;
	if (rqp->mrp == NULL) return 0;
	((u_int8_t *)&t)[1] = *((u_int8_t*)(rqp->bpos = mtod(rqp->mrp, caddr_t)));
	rqp->bpos += 2;
	return t;
}

u_int16_t
ncp_rp_word_hl(struct ncp_rq *rqp) {
	return (ntohs(ncp_rp_word_lh(rqp)));
}

u_int32_t
ncp_rp_dword_hl(struct ncp_rq *rqp) {
	int togo, rest;
	caddr_t prev = rqp->bpos;
	u_int32_t t;

	if (rqp->mrp == NULL) return 0;
	rest = ncp_mspaceleft;
	if (rest >= 4) {
		rqp->bpos += 4;
		return getdbe(prev,0);
	}
	togo = 0;
	while (rest--) {
		((u_int8_t *)&t)[togo++] = *((u_int8_t*)(prev++));
	}
	rqp->mrp = rqp->mrp->m_next;
	if (rqp->mrp == NULL) return 0;
	prev = mtod(rqp->mrp, caddr_t);
	rqp->bpos = prev + 4 - togo;	/* XXX possible low than togo bytes in next mbuf */
	while (togo < 4) {
		((u_int8_t *)&t)[togo++] = *((u_int8_t*)(prev++));
	}
	return getdbe(&t,0);
}

u_int32_t
ncp_rp_dword_lh(struct ncp_rq *rqp) {
	int rest, togo;
	caddr_t prev = rqp->bpos;
	u_int32_t t;

	if (rqp->mrp == NULL) return 0;
	rest = ncp_mspaceleft;
	if (rest >= 4) {
		rqp->bpos += 4;
		return getdle(prev,0);
	}
	togo = 0;
	while (rest--) {
		((u_int8_t *)&t)[togo++] = *((u_int8_t*)(prev++));
	}
	rqp->mrp = rqp->mrp->m_next;
	if (rqp->mrp == NULL) return 0;
	prev = mtod(rqp->mrp, caddr_t);
	rqp->bpos = prev + 4 - togo;	/* XXX possible low than togo bytes in next mbuf */
	while (togo < 4) {
		((u_int8_t *)&t)[togo++] = *((u_int8_t*)(prev++));
	}
	return getdle(&t,0);
}
void
ncp_rp_mem(struct ncp_rq *rqp,caddr_t target, int size) {
	register struct mbuf *m=rqp->mrp;
	register unsigned count;
	
	while (size > 0) {
		if (m==0) {	/* should be panic */
			printf("ncp_rp_mem: incomplete copy\n");
			return;
		}
		count = mtod(m,caddr_t)+m->m_len-rqp->bpos;
		if (count == 0) {
			m=m->m_next;
			rqp->bpos=mtod(m,caddr_t);
			continue;
		}
		count = min(count,size);
		bcopy(rqp->bpos, target, count);
		size -= count;
		target += count;
		rqp->bpos += count;
	}
	rqp->mrp=m;
	return;
}

int
ncp_rp_usermem(struct ncp_rq *rqp,caddr_t target, int size) {
	register struct mbuf *m=rqp->mrp;
	register unsigned count;
	int error;
	
	while (size>0) {
		if (m==0) {	/* should be panic */
			printf("ncp_rp_mem: incomplete copy\n");
			return EFAULT;
		}
		count = mtod(m,caddr_t)+m->m_len-rqp->bpos;
		if (count == 0) {
			m=m->m_next;
			rqp->bpos=mtod(m,caddr_t);
			continue;
		}
		count = min(count,size);
		error=copyout(rqp->bpos, target, count);
		if (error) return error;
		size -= count;
		target += count;
		rqp->bpos += count;
	}
	rqp->mrp=m;
	return 0;
}

struct mbuf*
ncp_rp_mbuf(struct ncp_rq *rqp, int size) {
	register struct mbuf *m=rqp->mrp, *rm;
	register unsigned count;
	
	rm = m_copym(m, rqp->bpos - mtod(m,caddr_t), size, M_WAIT);
	while (size > 0) {
		if (m == 0) {
			printf("ncp_rp_mbuf: can't advance\n");
			return rm;
		}
		count = mtod(m,caddr_t)+ m->m_len - rqp->bpos;
		if (count == 0) {
			m = m->m_next;
			rqp->bpos = mtod(m, caddr_t);
			continue;
		}
		count = min(count, size);
		size -= count;
		rqp->bpos += count;
	}
	rqp->mrp=m;
	return rm;
}

int
nwfs_mbuftouio(mrep, uiop, siz, dpos)
	struct mbuf **mrep;
	register struct uio *uiop;
	int siz;
	caddr_t *dpos;
{
	register char *mbufcp, *uiocp;
	register int xfer, left, len;
	register struct mbuf *mp;
	long uiosiz;
	int error = 0;

	mp = *mrep;
	if (!mp) return 0;
	mbufcp = *dpos;
	len = mtod(mp, caddr_t)+mp->m_len-mbufcp;
	while (siz > 0) {
		if (uiop->uio_iovcnt <= 0 || uiop->uio_iov == NULL)
			return (EFBIG);
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		while (left > 0) {
			while (len == 0) {
				mp = mp->m_next;
				if (mp == NULL)
					return (EBADRPC);
				mbufcp = mtod(mp, caddr_t);
				len = mp->m_len;
			}
			xfer = (left > len) ? len : left;
#ifdef notdef
			/* Not Yet.. */
			if (uiop->uio_iov->iov_op != NULL)
				(*(uiop->uio_iov->iov_op))
				(mbufcp, uiocp, xfer);
			else
#endif
			if (uiop->uio_segflg == UIO_SYSSPACE)
				bcopy(mbufcp, uiocp, xfer);
			else
				copyout(mbufcp, uiocp, xfer);
			left -= xfer;
			len -= xfer;
			mbufcp += xfer;
			uiocp += xfer;
			uiop->uio_offset += xfer;
			uiop->uio_resid -= xfer;
		}
		if (uiop->uio_iov->iov_len <= siz) {
			uiop->uio_iovcnt--;
			uiop->uio_iov++;
		} else {
			uiop->uio_iov->iov_base += uiosiz;
			uiop->uio_iov->iov_len -= uiosiz;
		}
		siz -= uiosiz;
	}
	*dpos = mbufcp;
	*mrep = mp;
	return (error);
}
/*
 * copies a uio scatter/gather list to an mbuf chain.
 * NOTE: can ony handle iovcnt == 1
 */
int
nwfs_uiotombuf(uiop, mq, siz, bpos)
	register struct uio *uiop;
	struct mbuf **mq;
	int siz;
	caddr_t *bpos;
{
	register char *uiocp;
	register struct mbuf *mp, *mp2;
	register int xfer, left, mlen;
	int uiosiz, clflg;

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1)
		panic("nfsm_uiotombuf: iovcnt != 1");
#endif

	if (siz > MLEN)		/* or should it >= MCLBYTES ?? */
		clflg = 1;
	else
		clflg = 0;
	mp = mp2 = *mq;
	while (siz > 0) {
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		while (left > 0) {
			mlen = M_TRAILINGSPACE(mp);
			if (mlen == 0) {
				MGET(mp, M_WAIT, MT_DATA);
				if (clflg)
					MCLGET(mp, M_WAIT);
				mp->m_len = 0;
				mp2->m_next = mp;
				mp2 = mp;
				mlen = M_TRAILINGSPACE(mp);
			}
			xfer = (left > mlen) ? mlen : left;
#ifdef notdef
			/* Not Yet.. */
			if (uiop->uio_iov->iov_op != NULL)
				(*(uiop->uio_iov->iov_op))
				(uiocp, mtod(mp, caddr_t)+mp->m_len, xfer);
			else
#endif
			if (uiop->uio_segflg == UIO_SYSSPACE)
				bcopy(uiocp, mtod(mp, caddr_t)+mp->m_len, xfer);
			else
				copyin(uiocp, mtod(mp, caddr_t)+mp->m_len, xfer);
			mp->m_len += xfer;
			left -= xfer;
			uiocp += xfer;
			uiop->uio_offset += xfer;
			uiop->uio_resid -= xfer;
		}
		uiop->uio_iov->iov_base += uiosiz;
		uiop->uio_iov->iov_len -= uiosiz;
		siz -= uiosiz;
	}
	*bpos = mtod(mp, caddr_t)+mp->m_len;
	*mq = mp;
	return (0);
}
