/*
 * Copyright (c) 1982, 1986, 1988, 1991 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)uipc_mbuf.c	7.19 (Berkeley) 4/20/91
 *	$Id: uipc_mbuf.c,v 1.6 1993/12/19 00:51:44 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "malloc.h"
#define MBTYPES
#include "mbuf.h"
#include "kernel.h"
#include "syslog.h"
#include "domain.h"
#include "protosw.h"
#include "vm/vm.h"

/* From sys/mbuf.h */
struct 	mbstat mbstat;
int	nmbclusters;
union	mcluster *mclfree;
int	max_linkhdr;
int	max_protohdr;
int	max_hdr;
int	max_datalen;

extern	vm_map_t mb_map;
struct	mbuf *mbutl;
char	*mclrefcnt;

static void m_reclaim(void);

void
mbinit()
{
	int s;

#if CLBYTES < 4096
#define NCL_INIT	(4096/CLBYTES)
#else
#define NCL_INIT	1
#endif
	s = splimp();
	if (m_clalloc(NCL_INIT, M_DONTWAIT) == 0)
		goto bad;
	splx(s);
	return;
bad:
	panic("mbinit");
}

/*
 * Allocate some number of mbuf clusters
 * and place on cluster free list.
 * Must be called at splimp.
 */
/* ARGSUSED */
int
m_clalloc(ncl, how)				/* 31 Aug 92*/
	register int ncl;
	int how;
{
	int npg, mbx;
	register caddr_t p;
	register int i;
	static int logged;

	npg = ncl * CLSIZE;
	/* 31 Aug 92*/
	p = (caddr_t)kmem_malloc(mb_map, ctob(npg), !(how&M_DONTWAIT));
	if (p == NULL) {
		if (logged == 0) {
			logged++;
			log(LOG_ERR, "mb_map full\n");
		}
		return (0);
	}
	ncl = ncl * CLBYTES / MCLBYTES;
	for (i = 0; i < ncl; i++) {
		((union mcluster *)p)->mcl_next = mclfree;
		mclfree = (union mcluster *)p;
		p += MCLBYTES;
		mbstat.m_clfree++;
	}
	mbstat.m_clusters += ncl;
	return (1);
}

/*
 * When MGET failes, ask protocols to free space when short of memory,
 * then re-attempt to allocate an mbuf.
 */
struct mbuf *
m_retry(i, t)
	int i, t;
{
	register struct mbuf *m;

	m_reclaim();
#define m_retry(i, t)	(struct mbuf *)0
	MGET(m, i, t);
#undef m_retry
	return (m);
}

/*
 * As above; retry an MGETHDR.
 */
struct mbuf *
m_retryhdr(i, t)
	int i, t;
{
	register struct mbuf *m;

	m_reclaim();
#define m_retryhdr(i, t) (struct mbuf *)0
	MGETHDR(m, i, t);
#undef m_retryhdr
	return (m);
}

static void
m_reclaim()
{
	register struct domain *dp;
	register struct protosw *pr;
	int s = splimp();

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_drain)
				(*pr->pr_drain)();
	splx(s);
	mbstat.m_drain++;
}

/*
 * Space allocation routines.
 * These are also available as macros
 * for critical paths.
 */
struct mbuf *
m_get(how, type)				/* 31 Aug 92*/
	int how, type;
{
	register struct mbuf *m;

	MGET(m, how, type);
	return (m);
}

struct mbuf *
m_gethdr(how, type)				/* 31 Aug 92*/
	int how, type;
{
	register struct mbuf *m;

	MGETHDR(m, how, type);
	return (m);
}

struct mbuf *
m_getclr(how, type)				/* 31 Aug 92*/
	int how, type;
{
	register struct mbuf *m;

	MGET(m, how, type);
	if (m == 0)
		return (0);
	bzero(mtod(m, caddr_t), MLEN);
	return (m);
}

struct mbuf *
m_free(m)
	struct mbuf *m;
{
	register struct mbuf *n;

	MFREE(m, n);
	return (n);
}

void
m_freem(m)
	register struct mbuf *m;
{
	register struct mbuf *n;

	if (m == NULL)
		return;
	do {
		MFREE(m, n);
	} while (m = n);
}

/*
 * Mbuffer utility routines.
 */

/*
 * Lesser-used path for M_PREPEND:
 * allocate new mbuf to prepend to chain,
 * copy junk along.
 */
struct mbuf *
m_prepend(m, len, how)
	register struct mbuf *m;
	int len, how;
{
	struct mbuf *mn;

	MGET(mn, how, m->m_type);
	if (mn == (struct mbuf *)NULL) {
		m_freem(m);
		return ((struct mbuf *)NULL);
	}
	if (m->m_flags & M_PKTHDR) {
		M_COPY_PKTHDR(mn, m);
		m->m_flags &= ~M_PKTHDR;
	}
	mn->m_next = m;
	m = mn;
	if (len < MHLEN)
		MH_ALIGN(m, len);
	m->m_len = len;
	return (m);
}

/*
 * Make a copy of an mbuf chain starting "off0" bytes from the beginning,
 * continuing for "len" bytes.  If len is M_COPYALL, copy to end of mbuf.
 * The wait parameter is a choice of M_WAIT/M_DONTWAIT from caller.
 */
int MCFail;

struct mbuf *
m_copym(m, off0, len, wait)
	register struct mbuf *m;
	int off0, wait;
	register int len;
{
	register struct mbuf *n, **np;
	register int off = off0;
	struct mbuf *top;
	int copyhdr = 0;

	if (off < 0 || len < 0)
		panic("m_copym");
	if (off == 0 && m->m_flags & M_PKTHDR)
		copyhdr = 1;
	while (off > 0) {
		if (m == 0)
			panic("m_copym");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	np = &top;
	top = 0;
	while (len > 0) {
		if (m == 0) {
			if (len != M_COPYALL)
				panic("m_copym");
			break;
		}
		MGET(n, wait, m->m_type);
		*np = n;
		if (n == 0)
			goto nospace;
		if (copyhdr) {
			M_COPY_PKTHDR(n, m);
			if (len == M_COPYALL)
				n->m_pkthdr.len -= off0;
			else
				n->m_pkthdr.len = len;
			copyhdr = 0;
		}
		n->m_len = MIN(len, m->m_len - off);
		if (m->m_flags & M_EXT) {
			n->m_data = m->m_data + off;
			mclrefcnt[mtocl(m->m_ext.ext_buf)]++;
			n->m_ext = m->m_ext;
			n->m_flags |= M_EXT;
		} else
			bcopy(mtod(m, caddr_t)+off, mtod(n, caddr_t),
			    (unsigned)n->m_len);
		if (len != M_COPYALL)
			len -= n->m_len;
		off = 0;
		m = m->m_next;
		np = &n->m_next;
	}
	if (top == 0)
		MCFail++;
	return (top);
nospace:
	m_freem(top);
	MCFail++;
	return (0);
}

/*
 * Copy data from an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes, into the indicated buffer.
 */
void
m_copydata(m, off, len, cp)
	register struct mbuf *m;
	register int off;
	register int len;
	caddr_t cp;
{
	register unsigned count;

	if (off < 0 || len < 0)
		panic("m_copydata");
	while (off > 0) {
		if (m == 0)
			panic("m_copydata");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		if (m == 0)
			panic("m_copydata");
		count = MIN(m->m_len - off, len);
		bcopy(mtod(m, caddr_t) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		m = m->m_next;
	}
}

/*
 * Concatenate mbuf chain n to m.
 * Both chains must be of the same type (e.g. MT_DATA).
 * Any m_pkthdr is not updated.
 */
void
m_cat(m, n)
	register struct mbuf *m, *n;
{
	while (m->m_next)
		m = m->m_next;
	while (n) {
		if (m->m_flags & M_EXT ||
		    m->m_data + m->m_len + n->m_len >= &m->m_dat[MLEN]) {
			/* just join the two chains */
			m->m_next = n;
			return;
		}
		/* splat the data from one into the other */
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t) + m->m_len,
		    (u_int)n->m_len);
		m->m_len += n->m_len;
		n = m_free(n);
	}
}

void
m_adj(mp, req_len)
	struct mbuf *mp;
	int req_len;
{
	register int len = req_len;
	register struct mbuf *m;
	register count;

	if ((m = mp) == NULL)
		return;
	if (len >= 0) {
		/*
		 * Trim from head.
		 */
		while (m != NULL && len > 0) {
			if (m->m_len <= len) {
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {
				m->m_len -= len;
				m->m_data += len;
				len = 0;
			}
		}
		m = mp;
		if (mp->m_flags & M_PKTHDR)
			m->m_pkthdr.len -= (req_len - len);
	} else {
		/*
		 * Trim from tail.  Scan the mbuf chain,
		 * calculating its length and finding the last mbuf.
		 * If the adjustment only affects this mbuf, then just
		 * adjust and return.  Otherwise, rescan and truncate
		 * after the remaining size.
		 */
		len = -len;
		count = 0;
		for (;;) {
			count += m->m_len;
			if (m->m_next == (struct mbuf *)0)
				break;
			m = m->m_next;
		}
		if (m->m_len >= len) {
			m->m_len -= len;
			if ((mp = m)->m_flags & M_PKTHDR)
				m->m_pkthdr.len -= len;
			return;
		}
		count -= len;
		if (count < 0)
			count = 0;
		/*
		 * Correct length for chain is "count".
		 * Find the mbuf with last data, adjust its length,
		 * and toss data from remaining mbufs on chain.
		 */
		m = mp;
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len = count;
		for (; m; m = m->m_next) {
			if (m->m_len >= count) {
				m->m_len = count;
				break;
			}
			count -= m->m_len;
		}
		while (m = m->m_next)
			m->m_len = 0;
	}
}

/*
 * Rearange an mbuf chain so that len bytes are contiguous
 * and in the data area of an mbuf (so that mtod and dtom
 * will work for a structure of size len).  Returns the resulting
 * mbuf chain on success, frees it and returns null on failure.
 * If there is room, it will add up to max_protohdr-len extra bytes to the
 * contiguous region in an attempt to avoid being called next time.
 */
int MPFail;

struct mbuf *
m_pullup(n, len)
	register struct mbuf *n;
	int len;
{
	register struct mbuf *m;
	register int count;
	int space;

	/*
	 * If first mbuf has no cluster, and has room for len bytes
	 * without shifting current data, pullup into it,
	 * otherwise allocate a new mbuf to prepend to the chain.
	 */
	if ((n->m_flags & M_EXT) == 0 &&
	    n->m_data + len < &n->m_dat[MLEN] && n->m_next) {
		if (n->m_len >= len)
			return (n);
		m = n;
		n = n->m_next;
		len -= m->m_len;
	} else {
		if (len > MHLEN)
			goto bad;
		MGET(m, M_DONTWAIT, n->m_type);
		if (m == 0)
			goto bad;
		m->m_len = 0;
		if (n->m_flags & M_PKTHDR) {
			M_COPY_PKTHDR(m, n);
			n->m_flags &= ~M_PKTHDR;
		}
	}
	space = &m->m_dat[MLEN] - (m->m_data + m->m_len);
	do {
		count = min(min(max(len, max_protohdr), space), n->m_len);
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t) + m->m_len,
		  (unsigned)count);
		len -= count;
		m->m_len += count;
		n->m_len -= count;
		space -= count;
		if (n->m_len)
			n->m_data += count;
		else
			n = m_free(n);
	} while (len > 0 && n);
	if (len > 0) {
		(void) m_free(m);
		goto bad;
	}
	m->m_next = n;
	return (m);
bad:
	m_freem(n);
	MPFail++;
	return (0);
}

/*
 * Copy data from a buffer back into the indicated mbuf chain,
 * starting "off" bytes from the beginning, extending the mbuf
 * chain if necessary.
 */
void
m_copyback(m0, off, len, cp)
	struct	mbuf *m0;
	register int off;
	register int len;
	caddr_t cp;

{
	register int mlen;
	register struct mbuf *m = m0, *n;
	int totlen = 0;

	if (m0 == 0)
		return;
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == 0) {
			n = m_getclr(M_DONTWAIT, m->m_type);
			if (n == 0)
				goto out;
			n->m_len = min(MLEN, len + off);
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		mlen = min (m->m_len - off, len);
		bcopy(cp, off + mtod(m, caddr_t), (unsigned)mlen);
		cp += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->m_next == 0) {
			n = m_get(M_DONTWAIT, m->m_type);
			if (n == 0)
				break;
			n->m_len = min(MLEN, len);
			m->m_next = n;
		}
		m = m->m_next;
	}
out:	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;
}

struct mbuf *
m_split (m0, len0, wait)
	register struct mbuf *m0;
	int len0;
	int wait;
{
	register struct mbuf *m, *n;
	unsigned len = len0, remain;

	for (m = m0; m && len > m -> m_len; m = m -> m_next)
		len -= m -> m_len;
	if (m == 0)
		return (0);
	remain = m -> m_len - len;
	if (m0 -> m_flags & M_PKTHDR) {
		MGETHDR(n, wait, m0 -> m_type);
		if (n == 0)
			return (0);
		n -> m_pkthdr.rcvif = m0 -> m_pkthdr.rcvif;
		n -> m_pkthdr.len = m0 -> m_pkthdr.len - len0;
		m0 -> m_pkthdr.len = len0;
		if (m -> m_flags & M_EXT)
			goto extpacket;
		if (remain > MHLEN) {
			/* m can't be the lead packet */
			MH_ALIGN(n, 0);
			n -> m_next = m_split (m, len, wait);
			if (n -> m_next == 0) {
				(void) m_free (n);
				return (0);
			} else
				return (n);
		} else
			MH_ALIGN(n, remain);
	} else if (remain == 0) {
		n = m -> m_next;
		m -> m_next = 0;
		return (n);
	} else {
		MGET(n, wait, m -> m_type);
		if (n == 0)
			return (0);
		M_ALIGN(n, remain);
	}
extpacket:
	if (m -> m_flags & M_EXT) {
		n -> m_flags |= M_EXT;
		n -> m_ext = m -> m_ext;
		mclrefcnt[mtocl (m -> m_ext.ext_buf)]++;
		n -> m_data = m -> m_data + len;
	} else {
		bcopy (mtod (m, caddr_t) + len, mtod (n, caddr_t), remain);
	}
	n -> m_len = remain;
	m -> m_len = len;
	n -> m_next = m -> m_next;
	m -> m_next = 0;
	return (n);
}

/* The following taken from netiso/iso_chksum.c */
struct mbuf *
m_append(head, m)		/* XXX */
	struct mbuf *head, *m;
{
	register struct mbuf *n;

	if (m == 0)
		return head;
	if (head == 0)
		return m;
	n = head;
	while (n->m_next)
		n = n->m_next;
	n->m_next = m;
	return head;
}

/*
 * FUNCTION:	m_datalen
 *
 * PURPOSE:		returns length of the mbuf chain.
 * 				used all over the iso code.
 *
 * RETURNS:		integer
 *
 * SIDE EFFECTS: none
 *
 * NOTES:		
 */
int
m_datalen (morig)		/* XXX */
	struct mbuf *morig;
{ 	
	int	s = splimp();
	register struct mbuf *n=morig;
	register int datalen = 0;

	if( morig == (struct mbuf *)0)
		return 0;
	for(;;) {
		datalen += n->m_len;
		if (n->m_next == (struct mbuf *)0 ) {
			break;
		}
		n = n->m_next;
	}
	splx(s);
	return datalen;
}

int
m_compress(in, out)
	register struct mbuf *in, **out;
{
	register 	int datalen = 0;
	int	s = splimp();

	if(!in->m_next) {
		*out = in;
		splx(s);
		return in->m_len;
	}
	MGET((*out), M_DONTWAIT, MT_DATA);
	if(! (*out)) {
		*out = in;
		splx(s);
		return -1; 
	}
	(*out)->m_len = 0;
	(*out)->m_act = 0;

	while (in) {
		if (in->m_flags & M_EXT) {
#ifdef DEBUG
			ASSERT(in->m_len == 0);
#endif
		}
		if ( in->m_len == 0) {
			in = in->m_next;
			continue;
		}
		if (((*out)->m_flags & M_EXT) == 0) {
			int len;

			len = M_TRAILINGSPACE(*out);
			len = MIN(len, in->m_len);
			datalen += len;

			bcopy(mtod(in, caddr_t), mtod((*out), caddr_t) + (*out)->m_len,
						(unsigned)len);

			(*out)->m_len += len;
			in->m_len -= len;
			continue;
		} else {
			/* (*out) is full */
			if( !((*out)->m_next = m_get(M_DONTWAIT, MT_DATA))) {
				m_freem(*out);
				*out = in;
				splx(s);
				return -1;
			}
			(*out)->m_len = 0;
			(*out)->m_act = 0;
			*out = (*out)->m_next;
		}
	}
	m_freem(in);
	splx(s);
	return datalen;
}
