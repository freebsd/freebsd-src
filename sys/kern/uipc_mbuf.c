/*-
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)uipc_mbuf.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_param.h"
#include "opt_mbuf_stress_test.h"
#include "opt_mbuf_profiling.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/uio.h>

int	max_linkhdr;
int	max_protohdr;
int	max_hdr;
int	max_datalen;
#ifdef MBUF_STRESS_TEST
int	m_defragpackets;
int	m_defragbytes;
int	m_defraguseless;
int	m_defragfailure;
int	m_defragrandomfailures;
#endif

/*
 * sysctl(8) exported objects
 */
SYSCTL_INT(_kern_ipc, KIPC_MAX_LINKHDR, max_linkhdr, CTLFLAG_RD,
	   &max_linkhdr, 0, "Size of largest link layer header");
SYSCTL_INT(_kern_ipc, KIPC_MAX_PROTOHDR, max_protohdr, CTLFLAG_RD,
	   &max_protohdr, 0, "Size of largest protocol layer header");
SYSCTL_INT(_kern_ipc, KIPC_MAX_HDR, max_hdr, CTLFLAG_RD,
	   &max_hdr, 0, "Size of largest link plus protocol header");
SYSCTL_INT(_kern_ipc, KIPC_MAX_DATALEN, max_datalen, CTLFLAG_RD,
	   &max_datalen, 0, "Minimum space left in mbuf after max_hdr");
#ifdef MBUF_STRESS_TEST
SYSCTL_INT(_kern_ipc, OID_AUTO, m_defragpackets, CTLFLAG_RD,
	   &m_defragpackets, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, m_defragbytes, CTLFLAG_RD,
	   &m_defragbytes, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, m_defraguseless, CTLFLAG_RD,
	   &m_defraguseless, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, m_defragfailure, CTLFLAG_RD,
	   &m_defragfailure, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, m_defragrandomfailures, CTLFLAG_RW,
	   &m_defragrandomfailures, 0, "");
#endif

/*
 * Allocate a given length worth of mbufs and/or clusters (whatever fits
 * best) and return a pointer to the top of the allocated chain.  If an
 * existing mbuf chain is provided, then we will append the new chain
 * to the existing one but still return the top of the newly allocated
 * chain.
 */
struct mbuf *
m_getm2(struct mbuf *m, int len, int how, short type, int flags)
{
	struct mbuf *mb, *nm = NULL, *mtail = NULL;

	KASSERT(len >= 0, ("%s: len is < 0", __func__));

	/* Validate flags. */
	flags &= (M_PKTHDR | M_EOR);

	/* Packet header mbuf must be first in chain. */
	if ((flags & M_PKTHDR) && m != NULL)
		flags &= ~M_PKTHDR;

	/* Loop and append maximum sized mbufs to the chain tail. */
	while (len > 0) {
		if (len > MCLBYTES)
			mb = m_getjcl(how, type, (flags & M_PKTHDR),
			    MJUMPAGESIZE);
		else if (len >= MINCLSIZE)
			mb = m_getcl(how, type, (flags & M_PKTHDR));
		else if (flags & M_PKTHDR)
			mb = m_gethdr(how, type);
		else
			mb = m_get(how, type);

		/* Fail the whole operation if one mbuf can't be allocated. */
		if (mb == NULL) {
			if (nm != NULL)
				m_freem(nm);
			return (NULL);
		}

		/* Book keeping. */
		len -= (mb->m_flags & M_EXT) ? mb->m_ext.ext_size :
			((mb->m_flags & M_PKTHDR) ? MHLEN : MLEN);
		if (mtail != NULL)
			mtail->m_next = mb;
		else
			nm = mb;
		mtail = mb;
		flags &= ~M_PKTHDR;	/* Only valid on the first mbuf. */
	}
	if (flags & M_EOR)
		mtail->m_flags |= M_EOR;  /* Only valid on the last mbuf. */

	/* If mbuf was supplied, append new chain to the end of it. */
	if (m != NULL) {
		for (mtail = m; mtail->m_next != NULL; mtail = mtail->m_next)
			;
		mtail->m_next = nm;
		mtail->m_flags &= ~M_EOR;
	} else
		m = nm;

	return (m);
}

/*
 * Free an entire chain of mbufs and associated external buffers, if
 * applicable.
 */
void
m_freem(struct mbuf *mb)
{

	while (mb != NULL)
		mb = m_free(mb);
}

/*-
 * Configure a provided mbuf to refer to the provided external storage
 * buffer and setup a reference count for said buffer.  If the setting
 * up of the reference count fails, the M_EXT bit will not be set.  If
 * successfull, the M_EXT bit is set in the mbuf's flags.
 *
 * Arguments:
 *    mb     The existing mbuf to which to attach the provided buffer.
 *    buf    The address of the provided external storage buffer.
 *    size   The size of the provided buffer.
 *    freef  A pointer to a routine that is responsible for freeing the
 *           provided external storage buffer.
 *    args   A pointer to an argument structure (of any type) to be passed
 *           to the provided freef routine (may be NULL).
 *    flags  Any other flags to be passed to the provided mbuf.
 *    type   The type that the external storage buffer should be
 *           labeled with.
 *
 * Returns:
 *    Nothing.
 */
void
m_extadd(struct mbuf *mb, caddr_t buf, u_int size,
    void (*freef)(void *, void *), void *arg1, void *arg2, int flags, int type)
{
	KASSERT(type != EXT_CLUSTER, ("%s: EXT_CLUSTER not allowed", __func__));

	if (type != EXT_EXTREF)
		mb->m_ext.ref_cnt = (u_int *)uma_zalloc(zone_ext_refcnt, M_NOWAIT);
	if (mb->m_ext.ref_cnt != NULL) {
		*(mb->m_ext.ref_cnt) = 1;
		mb->m_flags |= (M_EXT | flags);
		mb->m_ext.ext_buf = buf;
		mb->m_data = mb->m_ext.ext_buf;
		mb->m_ext.ext_size = size;
		mb->m_ext.ext_free = freef;
		mb->m_ext.ext_arg1 = arg1;
		mb->m_ext.ext_arg2 = arg2;
		mb->m_ext.ext_type = type;
        }
}

/*
 * Non-directly-exported function to clean up after mbufs with M_EXT
 * storage attached to them if the reference count hits 1.
 */
void
mb_free_ext(struct mbuf *m)
{
	int skipmbuf;
	
	KASSERT((m->m_flags & M_EXT) == M_EXT, ("%s: M_EXT not set", __func__));
	KASSERT(m->m_ext.ref_cnt != NULL, ("%s: ref_cnt not set", __func__));


	/*
	 * check if the header is embedded in the cluster
	 */     
	skipmbuf = (m->m_flags & M_NOFREE);
	
	/* Free attached storage if this mbuf is the only reference to it. */
	if (*(m->m_ext.ref_cnt) == 1 ||
	    atomic_fetchadd_int(m->m_ext.ref_cnt, -1) == 1) {
		switch (m->m_ext.ext_type) {
		case EXT_PACKET:	/* The packet zone is special. */
			if (*(m->m_ext.ref_cnt) == 0)
				*(m->m_ext.ref_cnt) = 1;
			uma_zfree(zone_pack, m);
			return;		/* Job done. */
		case EXT_CLUSTER:
			uma_zfree(zone_clust, m->m_ext.ext_buf);
			break;
		case EXT_JUMBOP:
			uma_zfree(zone_jumbop, m->m_ext.ext_buf);
			break;
		case EXT_JUMBO9:
			uma_zfree(zone_jumbo9, m->m_ext.ext_buf);
			break;
		case EXT_JUMBO16:
			uma_zfree(zone_jumbo16, m->m_ext.ext_buf);
			break;
		case EXT_SFBUF:
		case EXT_NET_DRV:
		case EXT_MOD_TYPE:
		case EXT_DISPOSABLE:
			*(m->m_ext.ref_cnt) = 0;
			uma_zfree(zone_ext_refcnt, __DEVOLATILE(u_int *,
				m->m_ext.ref_cnt));
			/* FALLTHROUGH */
		case EXT_EXTREF:
			KASSERT(m->m_ext.ext_free != NULL,
				("%s: ext_free not set", __func__));
			(*(m->m_ext.ext_free))(m->m_ext.ext_arg1,
			    m->m_ext.ext_arg2);
			break;
		default:
			KASSERT(m->m_ext.ext_type == 0,
				("%s: unknown ext_type", __func__));
		}
	}
	if (skipmbuf)
		return;
	
	/*
	 * Free this mbuf back to the mbuf zone with all m_ext
	 * information purged.
	 */
	m->m_ext.ext_buf = NULL;
	m->m_ext.ext_free = NULL;
	m->m_ext.ext_arg1 = NULL;
	m->m_ext.ext_arg2 = NULL;
	m->m_ext.ref_cnt = NULL;
	m->m_ext.ext_size = 0;
	m->m_ext.ext_type = 0;
	m->m_flags &= ~M_EXT;
	uma_zfree(zone_mbuf, m);
}

/*
 * Attach the the cluster from *m to *n, set up m_ext in *n
 * and bump the refcount of the cluster.
 */
static void
mb_dupcl(struct mbuf *n, struct mbuf *m)
{
	KASSERT((m->m_flags & M_EXT) == M_EXT, ("%s: M_EXT not set", __func__));
	KASSERT(m->m_ext.ref_cnt != NULL, ("%s: ref_cnt not set", __func__));
	KASSERT((n->m_flags & M_EXT) == 0, ("%s: M_EXT set", __func__));

	if (*(m->m_ext.ref_cnt) == 1)
		*(m->m_ext.ref_cnt) += 1;
	else
		atomic_add_int(m->m_ext.ref_cnt, 1);
	n->m_ext.ext_buf = m->m_ext.ext_buf;
	n->m_ext.ext_free = m->m_ext.ext_free;
	n->m_ext.ext_arg1 = m->m_ext.ext_arg1;
	n->m_ext.ext_arg2 = m->m_ext.ext_arg2;
	n->m_ext.ext_size = m->m_ext.ext_size;
	n->m_ext.ref_cnt = m->m_ext.ref_cnt;
	n->m_ext.ext_type = m->m_ext.ext_type;
	n->m_flags |= M_EXT;
}

/*
 * Clean up mbuf (chain) from any tags and packet headers.
 * If "all" is set then the first mbuf in the chain will be
 * cleaned too.
 */
void
m_demote(struct mbuf *m0, int all)
{
	struct mbuf *m;

	for (m = all ? m0 : m0->m_next; m != NULL; m = m->m_next) {
		if (m->m_flags & M_PKTHDR) {
			m_tag_delete_chain(m, NULL);
			m->m_flags &= ~M_PKTHDR;
			bzero(&m->m_pkthdr, sizeof(struct pkthdr));
		}
		if (m != m0 && m->m_nextpkt != NULL) {
			KASSERT(m->m_nextpkt == NULL,
			    ("%s: m_nextpkt not NULL", __func__));
			m_freem(m->m_nextpkt);
			m->m_nextpkt = NULL;
		}
		m->m_flags = m->m_flags & (M_EXT|M_RDONLY|M_FREELIST|M_NOFREE);
	}
}

/*
 * Sanity checks on mbuf (chain) for use in KASSERT() and general
 * debugging.
 * Returns 0 or panics when bad and 1 on all tests passed.
 * Sanitize, 0 to run M_SANITY_ACTION, 1 to garble things so they
 * blow up later.
 */
int
m_sanity(struct mbuf *m0, int sanitize)
{
	struct mbuf *m;
	caddr_t a, b;
	int pktlen = 0;

#ifdef INVARIANTS
#define	M_SANITY_ACTION(s)	panic("mbuf %p: " s, m)
#else 
#define	M_SANITY_ACTION(s)	printf("mbuf %p: " s, m)
#endif

	for (m = m0; m != NULL; m = m->m_next) {
		/*
		 * Basic pointer checks.  If any of these fails then some
		 * unrelated kernel memory before or after us is trashed.
		 * No way to recover from that.
		 */
		a = ((m->m_flags & M_EXT) ? m->m_ext.ext_buf :
			((m->m_flags & M_PKTHDR) ? (caddr_t)(&m->m_pktdat) :
			 (caddr_t)(&m->m_dat)) );
		b = (caddr_t)(a + (m->m_flags & M_EXT ? m->m_ext.ext_size :
			((m->m_flags & M_PKTHDR) ? MHLEN : MLEN)));
		if ((caddr_t)m->m_data < a)
			M_SANITY_ACTION("m_data outside mbuf data range left");
		if ((caddr_t)m->m_data > b)
			M_SANITY_ACTION("m_data outside mbuf data range right");
		if ((caddr_t)m->m_data + m->m_len > b)
			M_SANITY_ACTION("m_data + m_len exeeds mbuf space");
		if ((m->m_flags & M_PKTHDR) && m->m_pkthdr.header) {
			if ((caddr_t)m->m_pkthdr.header < a ||
			    (caddr_t)m->m_pkthdr.header > b)
				M_SANITY_ACTION("m_pkthdr.header outside mbuf data range");
		}

		/* m->m_nextpkt may only be set on first mbuf in chain. */
		if (m != m0 && m->m_nextpkt != NULL) {
			if (sanitize) {
				m_freem(m->m_nextpkt);
				m->m_nextpkt = (struct mbuf *)0xDEADC0DE;
			} else
				M_SANITY_ACTION("m->m_nextpkt on in-chain mbuf");
		}

		/* packet length (not mbuf length!) calculation */
		if (m0->m_flags & M_PKTHDR)
			pktlen += m->m_len;

		/* m_tags may only be attached to first mbuf in chain. */
		if (m != m0 && m->m_flags & M_PKTHDR &&
		    !SLIST_EMPTY(&m->m_pkthdr.tags)) {
			if (sanitize) {
				m_tag_delete_chain(m, NULL);
				/* put in 0xDEADC0DE perhaps? */
			} else
				M_SANITY_ACTION("m_tags on in-chain mbuf");
		}

		/* M_PKTHDR may only be set on first mbuf in chain */
		if (m != m0 && m->m_flags & M_PKTHDR) {
			if (sanitize) {
				bzero(&m->m_pkthdr, sizeof(m->m_pkthdr));
				m->m_flags &= ~M_PKTHDR;
				/* put in 0xDEADCODE and leave hdr flag in */
			} else
				M_SANITY_ACTION("M_PKTHDR on in-chain mbuf");
		}
	}
	m = m0;
	if (pktlen && pktlen != m->m_pkthdr.len) {
		if (sanitize)
			m->m_pkthdr.len = 0;
		else
			M_SANITY_ACTION("m_pkthdr.len != mbuf chain length");
	}
	return 1;

#undef	M_SANITY_ACTION
}


/*
 * "Move" mbuf pkthdr from "from" to "to".
 * "from" must have M_PKTHDR set, and "to" must be empty.
 */
void
m_move_pkthdr(struct mbuf *to, struct mbuf *from)
{

#if 0
	/* see below for why these are not enabled */
	M_ASSERTPKTHDR(to);
	/* Note: with MAC, this may not be a good assertion. */
	KASSERT(SLIST_EMPTY(&to->m_pkthdr.tags),
	    ("m_move_pkthdr: to has tags"));
#endif
#ifdef MAC
	/*
	 * XXXMAC: It could be this should also occur for non-MAC?
	 */
	if (to->m_flags & M_PKTHDR)
		m_tag_delete_chain(to, NULL);
#endif
	to->m_flags = (from->m_flags & M_COPYFLAGS) | (to->m_flags & M_EXT);
	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;
	to->m_pkthdr = from->m_pkthdr;		/* especially tags */
	SLIST_INIT(&from->m_pkthdr.tags);	/* purge tags from src */
	from->m_flags &= ~M_PKTHDR;
}

/*
 * Duplicate "from"'s mbuf pkthdr in "to".
 * "from" must have M_PKTHDR set, and "to" must be empty.
 * In particular, this does a deep copy of the packet tags.
 */
int
m_dup_pkthdr(struct mbuf *to, struct mbuf *from, int how)
{

#if 0
	/*
	 * The mbuf allocator only initializes the pkthdr
	 * when the mbuf is allocated with MGETHDR. Many users
	 * (e.g. m_copy*, m_prepend) use MGET and then
	 * smash the pkthdr as needed causing these
	 * assertions to trip.  For now just disable them.
	 */
	M_ASSERTPKTHDR(to);
	/* Note: with MAC, this may not be a good assertion. */
	KASSERT(SLIST_EMPTY(&to->m_pkthdr.tags), ("m_dup_pkthdr: to has tags"));
#endif
	MBUF_CHECKSLEEP(how);
#ifdef MAC
	if (to->m_flags & M_PKTHDR)
		m_tag_delete_chain(to, NULL);
#endif
	to->m_flags = (from->m_flags & M_COPYFLAGS) | (to->m_flags & M_EXT);
	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;
	to->m_pkthdr = from->m_pkthdr;
	SLIST_INIT(&to->m_pkthdr.tags);
	return (m_tag_copy_chain(to, from, MBTOM(how)));
}

/*
 * Lesser-used path for M_PREPEND:
 * allocate new mbuf to prepend to chain,
 * copy junk along.
 */
struct mbuf *
m_prepend(struct mbuf *m, int len, int how)
{
	struct mbuf *mn;

	if (m->m_flags & M_PKTHDR)
		MGETHDR(mn, how, m->m_type);
	else
		MGET(mn, how, m->m_type);
	if (mn == NULL) {
		m_freem(m);
		return (NULL);
	}
	if (m->m_flags & M_PKTHDR)
		M_MOVE_PKTHDR(mn, m);
	mn->m_next = m;
	m = mn;
	if(m->m_flags & M_PKTHDR) {
		if (len < MHLEN)
			MH_ALIGN(m, len);
	} else {
		if (len < MLEN) 
			M_ALIGN(m, len);
	}
	m->m_len = len;
	return (m);
}

/*
 * Make a copy of an mbuf chain starting "off0" bytes from the beginning,
 * continuing for "len" bytes.  If len is M_COPYALL, copy to end of mbuf.
 * The wait parameter is a choice of M_WAIT/M_DONTWAIT from caller.
 * Note that the copy is read-only, because clusters are not copied,
 * only their reference counts are incremented.
 */
struct mbuf *
m_copym(struct mbuf *m, int off0, int len, int wait)
{
	struct mbuf *n, **np;
	int off = off0;
	struct mbuf *top;
	int copyhdr = 0;

	KASSERT(off >= 0, ("m_copym, negative off %d", off));
	KASSERT(len >= 0, ("m_copym, negative len %d", len));
	MBUF_CHECKSLEEP(wait);
	if (off == 0 && m->m_flags & M_PKTHDR)
		copyhdr = 1;
	while (off > 0) {
		KASSERT(m != NULL, ("m_copym, offset > size of mbuf chain"));
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	np = &top;
	top = 0;
	while (len > 0) {
		if (m == NULL) {
			KASSERT(len == M_COPYALL, 
			    ("m_copym, length > size of mbuf chain"));
			break;
		}
		if (copyhdr)
			MGETHDR(n, wait, m->m_type);
		else
			MGET(n, wait, m->m_type);
		*np = n;
		if (n == NULL)
			goto nospace;
		if (copyhdr) {
			if (!m_dup_pkthdr(n, m, wait))
				goto nospace;
			if (len == M_COPYALL)
				n->m_pkthdr.len -= off0;
			else
				n->m_pkthdr.len = len;
			copyhdr = 0;
		}
		n->m_len = min(len, m->m_len - off);
		if (m->m_flags & M_EXT) {
			n->m_data = m->m_data + off;
			mb_dupcl(n, m);
		} else
			bcopy(mtod(m, caddr_t)+off, mtod(n, caddr_t),
			    (u_int)n->m_len);
		if (len != M_COPYALL)
			len -= n->m_len;
		off = 0;
		m = m->m_next;
		np = &n->m_next;
	}
	if (top == NULL)
		mbstat.m_mcfail++;	/* XXX: No consistency. */

	return (top);
nospace:
	m_freem(top);
	mbstat.m_mcfail++;	/* XXX: No consistency. */
	return (NULL);
}

/*
 * Returns mbuf chain with new head for the prepending case.
 * Copies from mbuf (chain) n from off for len to mbuf (chain) m
 * either prepending or appending the data.
 * The resulting mbuf (chain) m is fully writeable.
 * m is destination (is made writeable)
 * n is source, off is offset in source, len is len from offset
 * dir, 0 append, 1 prepend
 * how, wait or nowait
 */

static int
m_bcopyxxx(void *s, void *t, u_int len)
{
	bcopy(s, t, (size_t)len);
	return 0;
}

struct mbuf *
m_copymdata(struct mbuf *m, struct mbuf *n, int off, int len,
    int prep, int how)
{
	struct mbuf *mm, *x, *z, *prev = NULL;
	caddr_t p;
	int i, nlen = 0;
	caddr_t buf[MLEN];

	KASSERT(m != NULL && n != NULL, ("m_copymdata, no target or source"));
	KASSERT(off >= 0, ("m_copymdata, negative off %d", off));
	KASSERT(len >= 0, ("m_copymdata, negative len %d", len));
	KASSERT(prep == 0 || prep == 1, ("m_copymdata, unknown direction %d", prep));

	mm = m;
	if (!prep) {
		while(mm->m_next) {
			prev = mm;
			mm = mm->m_next;
		}
	}
	for (z = n; z != NULL; z = z->m_next)
		nlen += z->m_len;
	if (len == M_COPYALL)
		len = nlen - off;
	if (off + len > nlen || len < 1)
		return NULL;

	if (!M_WRITABLE(mm)) {
		/* XXX: Use proper m_xxx function instead. */
		x = m_getcl(how, MT_DATA, mm->m_flags);
		if (x == NULL)
			return NULL;
		bcopy(mm->m_ext.ext_buf, x->m_ext.ext_buf, x->m_ext.ext_size);
		p = x->m_ext.ext_buf + (mm->m_data - mm->m_ext.ext_buf);
		x->m_data = p;
		mm->m_next = NULL;
		if (mm != m)
			prev->m_next = x;
		m_free(mm);
		mm = x;
	}

	/*
	 * Append/prepend the data.  Allocating mbufs as necessary.
	 */
	/* Shortcut if enough free space in first/last mbuf. */
	if (!prep && M_TRAILINGSPACE(mm) >= len) {
		m_apply(n, off, len, m_bcopyxxx, mtod(mm, caddr_t) +
			 mm->m_len);
		mm->m_len += len;
		mm->m_pkthdr.len += len;
		return m;
	}
	if (prep && M_LEADINGSPACE(mm) >= len) {
		mm->m_data = mtod(mm, caddr_t) - len;
		m_apply(n, off, len, m_bcopyxxx, mtod(mm, caddr_t));
		mm->m_len += len;
		mm->m_pkthdr.len += len;
		return mm;
	}

	/* Expand first/last mbuf to cluster if possible. */
	if (!prep && !(mm->m_flags & M_EXT) && len > M_TRAILINGSPACE(mm)) {
		bcopy(mm->m_data, &buf, mm->m_len);
		m_clget(mm, how);
		if (!(mm->m_flags & M_EXT))
			return NULL;
		bcopy(&buf, mm->m_ext.ext_buf, mm->m_len);
		mm->m_data = mm->m_ext.ext_buf;
		mm->m_pkthdr.header = NULL;
	}
	if (prep && !(mm->m_flags & M_EXT) && len > M_LEADINGSPACE(mm)) {
		bcopy(mm->m_data, &buf, mm->m_len);
		m_clget(mm, how);
		if (!(mm->m_flags & M_EXT))
			return NULL;
		bcopy(&buf, (caddr_t *)mm->m_ext.ext_buf +
		       mm->m_ext.ext_size - mm->m_len, mm->m_len);
		mm->m_data = (caddr_t)mm->m_ext.ext_buf +
			      mm->m_ext.ext_size - mm->m_len;
		mm->m_pkthdr.header = NULL;
	}

	/* Append/prepend as many mbuf (clusters) as necessary to fit len. */
	if (!prep && len > M_TRAILINGSPACE(mm)) {
		if (!m_getm(mm, len - M_TRAILINGSPACE(mm), how, MT_DATA))
			return NULL;
	}
	if (prep && len > M_LEADINGSPACE(mm)) {
		if (!(z = m_getm(NULL, len - M_LEADINGSPACE(mm), how, MT_DATA)))
			return NULL;
		i = 0;
		for (x = z; x != NULL; x = x->m_next) {
			i += x->m_flags & M_EXT ? x->m_ext.ext_size :
			      (x->m_flags & M_PKTHDR ? MHLEN : MLEN);
			if (!x->m_next)
				break;
		}
		z->m_data += i - len;
		m_move_pkthdr(mm, z);
		x->m_next = mm;
		mm = z;
	}

	/* Seek to start position in source mbuf. Optimization for long chains. */
	while (off > 0) {
		if (off < n->m_len)
			break;
		off -= n->m_len;
		n = n->m_next;
	}

	/* Copy data into target mbuf. */
	z = mm;
	while (len > 0) {
		KASSERT(z != NULL, ("m_copymdata, falling off target edge"));
		i = M_TRAILINGSPACE(z);
		m_apply(n, off, i, m_bcopyxxx, mtod(z, caddr_t) + z->m_len);
		z->m_len += i;
		/* fixup pkthdr.len if necessary */
		if ((prep ? mm : m)->m_flags & M_PKTHDR)
			(prep ? mm : m)->m_pkthdr.len += i;
		off += i;
		len -= i;
		z = z->m_next;
	}
	return (prep ? mm : m);
}

/*
 * Copy an entire packet, including header (which must be present).
 * An optimization of the common case `m_copym(m, 0, M_COPYALL, how)'.
 * Note that the copy is read-only, because clusters are not copied,
 * only their reference counts are incremented.
 * Preserve alignment of the first mbuf so if the creator has left
 * some room at the beginning (e.g. for inserting protocol headers)
 * the copies still have the room available.
 */
struct mbuf *
m_copypacket(struct mbuf *m, int how)
{
	struct mbuf *top, *n, *o;

	MBUF_CHECKSLEEP(how);
	MGET(n, how, m->m_type);
	top = n;
	if (n == NULL)
		goto nospace;

	if (!m_dup_pkthdr(n, m, how))
		goto nospace;
	n->m_len = m->m_len;
	if (m->m_flags & M_EXT) {
		n->m_data = m->m_data;
		mb_dupcl(n, m);
	} else {
		n->m_data = n->m_pktdat + (m->m_data - m->m_pktdat );
		bcopy(mtod(m, char *), mtod(n, char *), n->m_len);
	}

	m = m->m_next;
	while (m) {
		MGET(o, how, m->m_type);
		if (o == NULL)
			goto nospace;

		n->m_next = o;
		n = n->m_next;

		n->m_len = m->m_len;
		if (m->m_flags & M_EXT) {
			n->m_data = m->m_data;
			mb_dupcl(n, m);
		} else {
			bcopy(mtod(m, char *), mtod(n, char *), n->m_len);
		}

		m = m->m_next;
	}
	return top;
nospace:
	m_freem(top);
	mbstat.m_mcfail++;	/* XXX: No consistency. */ 
	return (NULL);
}

/*
 * Copy data from an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes, into the indicated buffer.
 */
void
m_copydata(const struct mbuf *m, int off, int len, caddr_t cp)
{
	u_int count;

	KASSERT(off >= 0, ("m_copydata, negative off %d", off));
	KASSERT(len >= 0, ("m_copydata, negative len %d", len));
	while (off > 0) {
		KASSERT(m != NULL, ("m_copydata, offset > size of mbuf chain"));
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		KASSERT(m != NULL, ("m_copydata, length > size of mbuf chain"));
		count = min(m->m_len - off, len);
		bcopy(mtod(m, caddr_t) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		m = m->m_next;
	}
}

/*
 * Copy a packet header mbuf chain into a completely new chain, including
 * copying any mbuf clusters.  Use this instead of m_copypacket() when
 * you need a writable copy of an mbuf chain.
 */
struct mbuf *
m_dup(struct mbuf *m, int how)
{
	struct mbuf **p, *top = NULL;
	int remain, moff, nsize;

	MBUF_CHECKSLEEP(how);
	/* Sanity check */
	if (m == NULL)
		return (NULL);
	M_ASSERTPKTHDR(m);

	/* While there's more data, get a new mbuf, tack it on, and fill it */
	remain = m->m_pkthdr.len;
	moff = 0;
	p = &top;
	while (remain > 0 || top == NULL) {	/* allow m->m_pkthdr.len == 0 */
		struct mbuf *n;

		/* Get the next new mbuf */
		if (remain >= MINCLSIZE) {
			n = m_getcl(how, m->m_type, 0);
			nsize = MCLBYTES;
		} else {
			n = m_get(how, m->m_type);
			nsize = MLEN;
		}
		if (n == NULL)
			goto nospace;

		if (top == NULL) {		/* First one, must be PKTHDR */
			if (!m_dup_pkthdr(n, m, how)) {
				m_free(n);
				goto nospace;
			}
			if ((n->m_flags & M_EXT) == 0)
				nsize = MHLEN;
		}
		n->m_len = 0;

		/* Link it into the new chain */
		*p = n;
		p = &n->m_next;

		/* Copy data from original mbuf(s) into new mbuf */
		while (n->m_len < nsize && m != NULL) {
			int chunk = min(nsize - n->m_len, m->m_len - moff);

			bcopy(m->m_data + moff, n->m_data + n->m_len, chunk);
			moff += chunk;
			n->m_len += chunk;
			remain -= chunk;
			if (moff == m->m_len) {
				m = m->m_next;
				moff = 0;
			}
		}

		/* Check correct total mbuf length */
		KASSERT((remain > 0 && m != NULL) || (remain == 0 && m == NULL),
		    	("%s: bogus m_pkthdr.len", __func__));
	}
	return (top);

nospace:
	m_freem(top);
	mbstat.m_mcfail++;	/* XXX: No consistency. */
	return (NULL);
}

/*
 * Concatenate mbuf chain n to m.
 * Both chains must be of the same type (e.g. MT_DATA).
 * Any m_pkthdr is not updated.
 */
void
m_cat(struct mbuf *m, struct mbuf *n)
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
m_adj(struct mbuf *mp, int req_len)
{
	int len = req_len;
	struct mbuf *m;
	int count;

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
		if (mp->m_flags & M_PKTHDR)
			mp->m_pkthdr.len -= (req_len - len);
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
			if (mp->m_flags & M_PKTHDR)
				mp->m_pkthdr.len -= len;
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
				if (m->m_next != NULL) {
					m_freem(m->m_next);
					m->m_next = NULL;
				}
				break;
			}
			count -= m->m_len;
		}
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
struct mbuf *
m_pullup(struct mbuf *n, int len)
{
	struct mbuf *m;
	int count;
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
		if (m == NULL)
			goto bad;
		m->m_len = 0;
		if (n->m_flags & M_PKTHDR)
			M_MOVE_PKTHDR(m, n);
	}
	space = &m->m_dat[MLEN] - (m->m_data + m->m_len);
	do {
		count = min(min(max(len, max_protohdr), space), n->m_len);
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t) + m->m_len,
		  (u_int)count);
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
	mbstat.m_mpfail++;	/* XXX: No consistency. */
	return (NULL);
}

/*
 * Like m_pullup(), except a new mbuf is always allocated, and we allow
 * the amount of empty space before the data in the new mbuf to be specified
 * (in the event that the caller expects to prepend later).
 */
int MSFail;

struct mbuf *
m_copyup(struct mbuf *n, int len, int dstoff)
{
	struct mbuf *m;
	int count, space;

	if (len > (MHLEN - dstoff))
		goto bad;
	MGET(m, M_DONTWAIT, n->m_type);
	if (m == NULL)
		goto bad;
	m->m_len = 0;
	if (n->m_flags & M_PKTHDR)
		M_MOVE_PKTHDR(m, n);
	m->m_data += dstoff;
	space = &m->m_dat[MLEN] - (m->m_data + m->m_len);
	do {
		count = min(min(max(len, max_protohdr), space), n->m_len);
		memcpy(mtod(m, caddr_t) + m->m_len, mtod(n, caddr_t),
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
	MSFail++;
	return (NULL);
}

/*
 * Partition an mbuf chain in two pieces, returning the tail --
 * all but the first len0 bytes.  In case of failure, it returns NULL and
 * attempts to restore the chain to its original state.
 *
 * Note that the resulting mbufs might be read-only, because the new
 * mbuf can end up sharing an mbuf cluster with the original mbuf if
 * the "breaking point" happens to lie within a cluster mbuf. Use the
 * M_WRITABLE() macro to check for this case.
 */
struct mbuf *
m_split(struct mbuf *m0, int len0, int wait)
{
	struct mbuf *m, *n;
	u_int len = len0, remain;

	MBUF_CHECKSLEEP(wait);
	for (m = m0; m && len > m->m_len; m = m->m_next)
		len -= m->m_len;
	if (m == NULL)
		return (NULL);
	remain = m->m_len - len;
	if (m0->m_flags & M_PKTHDR) {
		MGETHDR(n, wait, m0->m_type);
		if (n == NULL)
			return (NULL);
		n->m_pkthdr.rcvif = m0->m_pkthdr.rcvif;
		n->m_pkthdr.len = m0->m_pkthdr.len - len0;
		m0->m_pkthdr.len = len0;
		if (m->m_flags & M_EXT)
			goto extpacket;
		if (remain > MHLEN) {
			/* m can't be the lead packet */
			MH_ALIGN(n, 0);
			n->m_next = m_split(m, len, wait);
			if (n->m_next == NULL) {
				(void) m_free(n);
				return (NULL);
			} else {
				n->m_len = 0;
				return (n);
			}
		} else
			MH_ALIGN(n, remain);
	} else if (remain == 0) {
		n = m->m_next;
		m->m_next = NULL;
		return (n);
	} else {
		MGET(n, wait, m->m_type);
		if (n == NULL)
			return (NULL);
		M_ALIGN(n, remain);
	}
extpacket:
	if (m->m_flags & M_EXT) {
		n->m_data = m->m_data + len;
		mb_dupcl(n, m);
	} else {
		bcopy(mtod(m, caddr_t) + len, mtod(n, caddr_t), remain);
	}
	n->m_len = remain;
	m->m_len = len;
	n->m_next = m->m_next;
	m->m_next = NULL;
	return (n);
}
/*
 * Routine to copy from device local memory into mbufs.
 * Note that `off' argument is offset into first mbuf of target chain from
 * which to begin copying the data to.
 */
struct mbuf *
m_devget(char *buf, int totlen, int off, struct ifnet *ifp,
    void (*copy)(char *from, caddr_t to, u_int len))
{
	struct mbuf *m;
	struct mbuf *top = NULL, **mp = &top;
	int len;

	if (off < 0 || off > MHLEN)
		return (NULL);

	while (totlen > 0) {
		if (top == NULL) {	/* First one, must be PKTHDR */
			if (totlen + off >= MINCLSIZE) {
				m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
				len = MCLBYTES;
			} else {
				m = m_gethdr(M_DONTWAIT, MT_DATA);
				len = MHLEN;

				/* Place initial small packet/header at end of mbuf */
				if (m && totlen + off + max_linkhdr <= MLEN) {
					m->m_data += max_linkhdr;
					len -= max_linkhdr;
				}
			}
			if (m == NULL)
				return NULL;
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = totlen;
		} else {
			if (totlen + off >= MINCLSIZE) {
				m = m_getcl(M_DONTWAIT, MT_DATA, 0);
				len = MCLBYTES;
			} else {
				m = m_get(M_DONTWAIT, MT_DATA);
				len = MLEN;
			}
			if (m == NULL) {
				m_freem(top);
				return NULL;
			}
		}
		if (off) {
			m->m_data += off;
			len -= off;
			off = 0;
		}
		m->m_len = len = min(totlen, len);
		if (copy)
			copy(buf, mtod(m, caddr_t), (u_int)len);
		else
			bcopy(buf, mtod(m, caddr_t), (u_int)len);
		buf += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
	}
	return (top);
}

/*
 * Copy data from a buffer back into the indicated mbuf chain,
 * starting "off" bytes from the beginning, extending the mbuf
 * chain if necessary.
 */
void
m_copyback(struct mbuf *m0, int off, int len, c_caddr_t cp)
{
	int mlen;
	struct mbuf *m = m0, *n;
	int totlen = 0;

	if (m0 == NULL)
		return;
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == NULL) {
			n = m_get(M_DONTWAIT, m->m_type);
			if (n == NULL)
				goto out;
			bzero(mtod(n, caddr_t), MLEN);
			n->m_len = min(MLEN, len + off);
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		if (m->m_next == NULL && (len > m->m_len - off)) {
			m->m_len += min(len - (m->m_len - off),
			    M_TRAILINGSPACE(m));
		}
		mlen = min (m->m_len - off, len);
		bcopy(cp, off + mtod(m, caddr_t), (u_int)mlen);
		cp += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->m_next == NULL) {
			n = m_get(M_DONTWAIT, m->m_type);
			if (n == NULL)
				break;
			n->m_len = min(MLEN, len);
			m->m_next = n;
		}
		m = m->m_next;
	}
out:	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;
}

/*
 * Append the specified data to the indicated mbuf chain,
 * Extend the mbuf chain if the new data does not fit in
 * existing space.
 *
 * Return 1 if able to complete the job; otherwise 0.
 */
int
m_append(struct mbuf *m0, int len, c_caddr_t cp)
{
	struct mbuf *m, *n;
	int remainder, space;

	for (m = m0; m->m_next != NULL; m = m->m_next)
		;
	remainder = len;
	space = M_TRAILINGSPACE(m);
	if (space > 0) {
		/*
		 * Copy into available space.
		 */
		if (space > remainder)
			space = remainder;
		bcopy(cp, mtod(m, caddr_t) + m->m_len, space);
		m->m_len += space;
		cp += space, remainder -= space;
	}
	while (remainder > 0) {
		/*
		 * Allocate a new mbuf; could check space
		 * and allocate a cluster instead.
		 */
		n = m_get(M_DONTWAIT, m->m_type);
		if (n == NULL)
			break;
		n->m_len = min(MLEN, remainder);
		bcopy(cp, mtod(n, caddr_t), n->m_len);
		cp += n->m_len, remainder -= n->m_len;
		m->m_next = n;
		m = n;
	}
	if (m0->m_flags & M_PKTHDR)
		m0->m_pkthdr.len += len - remainder;
	return (remainder == 0);
}

/*
 * Apply function f to the data in an mbuf chain starting "off" bytes from
 * the beginning, continuing for "len" bytes.
 */
int
m_apply(struct mbuf *m, int off, int len,
    int (*f)(void *, void *, u_int), void *arg)
{
	u_int count;
	int rval;

	KASSERT(off >= 0, ("m_apply, negative off %d", off));
	KASSERT(len >= 0, ("m_apply, negative len %d", len));
	while (off > 0) {
		KASSERT(m != NULL, ("m_apply, offset > size of mbuf chain"));
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		KASSERT(m != NULL, ("m_apply, offset > size of mbuf chain"));
		count = min(m->m_len - off, len);
		rval = (*f)(arg, mtod(m, caddr_t) + off, count);
		if (rval)
			return (rval);
		len -= count;
		off = 0;
		m = m->m_next;
	}
	return (0);
}

/*
 * Return a pointer to mbuf/offset of location in mbuf chain.
 */
struct mbuf *
m_getptr(struct mbuf *m, int loc, int *off)
{

	while (loc >= 0) {
		/* Normal end of search. */
		if (m->m_len > loc) {
			*off = loc;
			return (m);
		} else {
			loc -= m->m_len;
			if (m->m_next == NULL) {
				if (loc == 0) {
					/* Point at the end of valid data. */
					*off = m->m_len;
					return (m);
				}
				return (NULL);
			}
			m = m->m_next;
		}
	}
	return (NULL);
}

void
m_print(const struct mbuf *m, int maxlen)
{
	int len;
	int pdata;
	const struct mbuf *m2;

	if (m->m_flags & M_PKTHDR)
		len = m->m_pkthdr.len;
	else
		len = -1;
	m2 = m;
	while (m2 != NULL && (len == -1 || len)) {
		pdata = m2->m_len;
		if (maxlen != -1 && pdata > maxlen)
			pdata = maxlen;
		printf("mbuf: %p len: %d, next: %p, %b%s", m2, m2->m_len,
		    m2->m_next, m2->m_flags, "\20\20freelist\17skipfw"
		    "\11proto5\10proto4\7proto3\6proto2\5proto1\4rdonly"
		    "\3eor\2pkthdr\1ext", pdata ? "" : "\n");
		if (pdata)
			printf(", %*D\n", pdata, (u_char *)m2->m_data, "-");
		if (len != -1)
			len -= m2->m_len;
		m2 = m2->m_next;
	}
	if (len > 0)
		printf("%d bytes unaccounted for.\n", len);
	return;
}

u_int
m_fixhdr(struct mbuf *m0)
{
	u_int len;

	len = m_length(m0, NULL);
	m0->m_pkthdr.len = len;
	return (len);
}

u_int
m_length(struct mbuf *m0, struct mbuf **last)
{
	struct mbuf *m;
	u_int len;

	len = 0;
	for (m = m0; m != NULL; m = m->m_next) {
		len += m->m_len;
		if (m->m_next == NULL)
			break;
	}
	if (last != NULL)
		*last = m;
	return (len);
}

/*
 * Defragment a mbuf chain, returning the shortest possible
 * chain of mbufs and clusters.  If allocation fails and
 * this cannot be completed, NULL will be returned, but
 * the passed in chain will be unchanged.  Upon success,
 * the original chain will be freed, and the new chain
 * will be returned.
 *
 * If a non-packet header is passed in, the original
 * mbuf (chain?) will be returned unharmed.
 */
struct mbuf *
m_defrag(struct mbuf *m0, int how)
{
	struct mbuf *m_new = NULL, *m_final = NULL;
	int progress = 0, length;

	MBUF_CHECKSLEEP(how);
	if (!(m0->m_flags & M_PKTHDR))
		return (m0);

	m_fixhdr(m0); /* Needed sanity check */

#ifdef MBUF_STRESS_TEST
	if (m_defragrandomfailures) {
		int temp = arc4random() & 0xff;
		if (temp == 0xba)
			goto nospace;
	}
#endif
	
	if (m0->m_pkthdr.len > MHLEN)
		m_final = m_getcl(how, MT_DATA, M_PKTHDR);
	else
		m_final = m_gethdr(how, MT_DATA);

	if (m_final == NULL)
		goto nospace;

	if (m_dup_pkthdr(m_final, m0, how) == 0)
		goto nospace;

	m_new = m_final;

	while (progress < m0->m_pkthdr.len) {
		length = m0->m_pkthdr.len - progress;
		if (length > MCLBYTES)
			length = MCLBYTES;

		if (m_new == NULL) {
			if (length > MLEN)
				m_new = m_getcl(how, MT_DATA, 0);
			else
				m_new = m_get(how, MT_DATA);
			if (m_new == NULL)
				goto nospace;
		}

		m_copydata(m0, progress, length, mtod(m_new, caddr_t));
		progress += length;
		m_new->m_len = length;
		if (m_new != m_final)
			m_cat(m_final, m_new);
		m_new = NULL;
	}
#ifdef MBUF_STRESS_TEST
	if (m0->m_next == NULL)
		m_defraguseless++;
#endif
	m_freem(m0);
	m0 = m_final;
#ifdef MBUF_STRESS_TEST
	m_defragpackets++;
	m_defragbytes += m0->m_pkthdr.len;
#endif
	return (m0);
nospace:
#ifdef MBUF_STRESS_TEST
	m_defragfailure++;
#endif
	if (m_final)
		m_freem(m_final);
	return (NULL);
}

/*
 * Defragment an mbuf chain, returning at most maxfrags separate
 * mbufs+clusters.  If this is not possible NULL is returned and
 * the original mbuf chain is left in it's present (potentially
 * modified) state.  We use two techniques: collapsing consecutive
 * mbufs and replacing consecutive mbufs by a cluster.
 *
 * NB: this should really be named m_defrag but that name is taken
 */
struct mbuf *
m_collapse(struct mbuf *m0, int how, int maxfrags)
{
	struct mbuf *m, *n, *n2, **prev;
	u_int curfrags;

	/*
	 * Calculate the current number of frags.
	 */
	curfrags = 0;
	for (m = m0; m != NULL; m = m->m_next)
		curfrags++;
	/*
	 * First, try to collapse mbufs.  Note that we always collapse
	 * towards the front so we don't need to deal with moving the
	 * pkthdr.  This may be suboptimal if the first mbuf has much
	 * less data than the following.
	 */
	m = m0;
again:
	for (;;) {
		n = m->m_next;
		if (n == NULL)
			break;
		if ((m->m_flags & M_RDONLY) == 0 &&
		    n->m_len < M_TRAILINGSPACE(m)) {
			bcopy(mtod(n, void *), mtod(m, char *) + m->m_len,
				n->m_len);
			m->m_len += n->m_len;
			m->m_next = n->m_next;
			m_free(n);
			if (--curfrags <= maxfrags)
				return m0;
		} else
			m = n;
	}
	KASSERT(maxfrags > 1,
		("maxfrags %u, but normal collapse failed", maxfrags));
	/*
	 * Collapse consecutive mbufs to a cluster.
	 */
	prev = &m0->m_next;		/* NB: not the first mbuf */
	while ((n = *prev) != NULL) {
		if ((n2 = n->m_next) != NULL &&
		    n->m_len + n2->m_len < MCLBYTES) {
			m = m_getcl(how, MT_DATA, 0);
			if (m == NULL)
				goto bad;
			bcopy(mtod(n, void *), mtod(m, void *), n->m_len);
			bcopy(mtod(n2, void *), mtod(m, char *) + n->m_len,
				n2->m_len);
			m->m_len = n->m_len + n2->m_len;
			m->m_next = n2->m_next;
			*prev = m;
			m_free(n);
			m_free(n2);
			if (--curfrags <= maxfrags)	/* +1 cl -2 mbufs */
				return m0;
			/*
			 * Still not there, try the normal collapse
			 * again before we allocate another cluster.
			 */
			goto again;
		}
		prev = &n->m_next;
	}
	/*
	 * No place where we can collapse to a cluster; punt.
	 * This can occur if, for example, you request 2 frags
	 * but the packet requires that both be clusters (we
	 * never reallocate the first mbuf to avoid moving the
	 * packet header).
	 */
bad:
	return NULL;
}

#ifdef MBUF_STRESS_TEST

/*
 * Fragment an mbuf chain.  There's no reason you'd ever want to do
 * this in normal usage, but it's great for stress testing various
 * mbuf consumers.
 *
 * If fragmentation is not possible, the original chain will be
 * returned.
 *
 * Possible length values:
 * 0	 no fragmentation will occur
 * > 0	each fragment will be of the specified length
 * -1	each fragment will be the same random value in length
 * -2	each fragment's length will be entirely random
 * (Random values range from 1 to 256)
 */
struct mbuf *
m_fragment(struct mbuf *m0, int how, int length)
{
	struct mbuf *m_new = NULL, *m_final = NULL;
	int progress = 0;

	if (!(m0->m_flags & M_PKTHDR))
		return (m0);
	
	if ((length == 0) || (length < -2))
		return (m0);

	m_fixhdr(m0); /* Needed sanity check */

	m_final = m_getcl(how, MT_DATA, M_PKTHDR);

	if (m_final == NULL)
		goto nospace;

	if (m_dup_pkthdr(m_final, m0, how) == 0)
		goto nospace;

	m_new = m_final;

	if (length == -1)
		length = 1 + (arc4random() & 255);

	while (progress < m0->m_pkthdr.len) {
		int fraglen;

		if (length > 0)
			fraglen = length;
		else
			fraglen = 1 + (arc4random() & 255);
		if (fraglen > m0->m_pkthdr.len - progress)
			fraglen = m0->m_pkthdr.len - progress;

		if (fraglen > MCLBYTES)
			fraglen = MCLBYTES;

		if (m_new == NULL) {
			m_new = m_getcl(how, MT_DATA, 0);
			if (m_new == NULL)
				goto nospace;
		}

		m_copydata(m0, progress, fraglen, mtod(m_new, caddr_t));
		progress += fraglen;
		m_new->m_len = fraglen;
		if (m_new != m_final)
			m_cat(m_final, m_new);
		m_new = NULL;
	}
	m_freem(m0);
	m0 = m_final;
	return (m0);
nospace:
	if (m_final)
		m_freem(m_final);
	/* Return the original chain on failure */
	return (m0);
}

#endif

/*
 * Copy the contents of uio into a properly sized mbuf chain.
 */
struct mbuf *
m_uiotombuf(struct uio *uio, int how, int len, int align, int flags)
{
	struct mbuf *m, *mb;
	int error, length, total;
	int progress = 0;

	/*
	 * len can be zero or an arbitrary large value bound by
	 * the total data supplied by the uio.
	 */
	if (len > 0)
		total = min(uio->uio_resid, len);
	else
		total = uio->uio_resid;

	/*
	 * The smallest unit returned by m_getm2() is a single mbuf
	 * with pkthdr.  We can't align past it.
	 */
	if (align >= MHLEN)
		return (NULL);

	/*
	 * Give us the full allocation or nothing.
	 * If len is zero return the smallest empty mbuf.
	 */
	m = m_getm2(NULL, max(total + align, 1), how, MT_DATA, flags);
	if (m == NULL)
		return (NULL);
	m->m_data += align;

	/* Fill all mbufs with uio data and update header information. */
	for (mb = m; mb != NULL; mb = mb->m_next) {
		length = min(M_TRAILINGSPACE(mb), total - progress);

		error = uiomove(mtod(mb, void *), length, uio);
		if (error) {
			m_freem(m);
			return (NULL);
		}

		mb->m_len = length;
		progress += length;
		if (flags & M_PKTHDR)
			m->m_pkthdr.len += length;
	}
	KASSERT(progress == total, ("%s: progress != total", __func__));

	return (m);
}

/*
 * Copy an mbuf chain into a uio limited by len if set.
 */
int
m_mbuftouio(struct uio *uio, struct mbuf *m, int len)
{
	int error, length, total;
	int progress = 0;

	if (len > 0)
		total = min(uio->uio_resid, len);
	else
		total = uio->uio_resid;

	/* Fill the uio with data from the mbufs. */
	for (; m != NULL; m = m->m_next) {
		length = min(m->m_len, total - progress);

		error = uiomove(mtod(m, void *), length, uio);
		if (error)
			return (error);

		progress += length;
	}

	return (0);
}

/*
 * Set the m_data pointer of a newly-allocated mbuf
 * to place an object of the specified size at the
 * end of the mbuf, longword aligned.
 */
void
m_align(struct mbuf *m, int len)
{
	int adjust;

	if (m->m_flags & M_EXT)
		adjust = m->m_ext.ext_size - len;
	else if (m->m_flags & M_PKTHDR)
		adjust = MHLEN - len;
	else
		adjust = MLEN - len;
	m->m_data += adjust &~ (sizeof(long)-1);
}

/*
 * Create a writable copy of the mbuf chain.  While doing this
 * we compact the chain with a goal of producing a chain with
 * at most two mbufs.  The second mbuf in this chain is likely
 * to be a cluster.  The primary purpose of this work is to create
 * a writable packet for encryption, compression, etc.  The
 * secondary goal is to linearize the data so the data can be
 * passed to crypto hardware in the most efficient manner possible.
 */
struct mbuf *
m_unshare(struct mbuf *m0, int how)
{
	struct mbuf *m, *mprev;
	struct mbuf *n, *mfirst, *mlast;
	int len, off;

	mprev = NULL;
	for (m = m0; m != NULL; m = mprev->m_next) {
		/*
		 * Regular mbufs are ignored unless there's a cluster
		 * in front of it that we can use to coalesce.  We do
		 * the latter mainly so later clusters can be coalesced
		 * also w/o having to handle them specially (i.e. convert
		 * mbuf+cluster -> cluster).  This optimization is heavily
		 * influenced by the assumption that we're running over
		 * Ethernet where MCLBYTES is large enough that the max
		 * packet size will permit lots of coalescing into a
		 * single cluster.  This in turn permits efficient
		 * crypto operations, especially when using hardware.
		 */
		if ((m->m_flags & M_EXT) == 0) {
			if (mprev && (mprev->m_flags & M_EXT) &&
			    m->m_len <= M_TRAILINGSPACE(mprev)) {
				/* XXX: this ignores mbuf types */
				memcpy(mtod(mprev, caddr_t) + mprev->m_len,
				       mtod(m, caddr_t), m->m_len);
				mprev->m_len += m->m_len;
				mprev->m_next = m->m_next;	/* unlink from chain */
				m_free(m);			/* reclaim mbuf */
#if 0
				newipsecstat.ips_mbcoalesced++;
#endif
			} else {
				mprev = m;
			}
			continue;
		}
		/*
		 * Writable mbufs are left alone (for now).
		 */
		if (M_WRITABLE(m)) {
			mprev = m;
			continue;
		}

		/*
		 * Not writable, replace with a copy or coalesce with
		 * the previous mbuf if possible (since we have to copy
		 * it anyway, we try to reduce the number of mbufs and
		 * clusters so that future work is easier).
		 */
		KASSERT(m->m_flags & M_EXT, ("m_flags 0x%x", m->m_flags));
		/* NB: we only coalesce into a cluster or larger */
		if (mprev != NULL && (mprev->m_flags & M_EXT) &&
		    m->m_len <= M_TRAILINGSPACE(mprev)) {
			/* XXX: this ignores mbuf types */
			memcpy(mtod(mprev, caddr_t) + mprev->m_len,
			       mtod(m, caddr_t), m->m_len);
			mprev->m_len += m->m_len;
			mprev->m_next = m->m_next;	/* unlink from chain */
			m_free(m);			/* reclaim mbuf */
#if 0
			newipsecstat.ips_clcoalesced++;
#endif
			continue;
		}

		/*
		 * Allocate new space to hold the copy...
		 */
		/* XXX why can M_PKTHDR be set past the first mbuf? */
		if (mprev == NULL && (m->m_flags & M_PKTHDR)) {
			/*
			 * NB: if a packet header is present we must
			 * allocate the mbuf separately from any cluster
			 * because M_MOVE_PKTHDR will smash the data
			 * pointer and drop the M_EXT marker.
			 */
			MGETHDR(n, how, m->m_type);
			if (n == NULL) {
				m_freem(m0);
				return (NULL);
			}
			M_MOVE_PKTHDR(n, m);
			MCLGET(n, how);
			if ((n->m_flags & M_EXT) == 0) {
				m_free(n);
				m_freem(m0);
				return (NULL);
			}
		} else {
			n = m_getcl(how, m->m_type, m->m_flags);
			if (n == NULL) {
				m_freem(m0);
				return (NULL);
			}
		}
		/*
		 * ... and copy the data.  We deal with jumbo mbufs
		 * (i.e. m_len > MCLBYTES) by splitting them into
		 * clusters.  We could just malloc a buffer and make
		 * it external but too many device drivers don't know
		 * how to break up the non-contiguous memory when
		 * doing DMA.
		 */
		len = m->m_len;
		off = 0;
		mfirst = n;
		mlast = NULL;
		for (;;) {
			int cc = min(len, MCLBYTES);
			memcpy(mtod(n, caddr_t), mtod(m, caddr_t) + off, cc);
			n->m_len = cc;
			if (mlast != NULL)
				mlast->m_next = n;
			mlast = n;	
#if 0
			newipsecstat.ips_clcopied++;
#endif

			len -= cc;
			if (len <= 0)
				break;
			off += cc;

			n = m_getcl(how, m->m_type, m->m_flags);
			if (n == NULL) {
				m_freem(mfirst);
				m_freem(m0);
				return (NULL);
			}
		}
		n->m_next = m->m_next; 
		if (mprev == NULL)
			m0 = mfirst;		/* new head of chain */
		else
			mprev->m_next = mfirst;	/* replace old mbuf */
		m_free(m);			/* release old mbuf */
		mprev = mfirst;
	}
	return (m0);
}

#ifdef MBUF_PROFILING

#define MP_BUCKETS 32 /* don't just change this as things may overflow.*/
struct mbufprofile {
	uintmax_t wasted[MP_BUCKETS];
	uintmax_t used[MP_BUCKETS];
	uintmax_t segments[MP_BUCKETS];
} mbprof;

#define MP_MAXDIGITS 21	/* strlen("16,000,000,000,000,000,000") == 21 */
#define MP_NUMLINES 6
#define MP_NUMSPERLINE 16
#define MP_EXTRABYTES 64	/* > strlen("used:\nwasted:\nsegments:\n") */
/* work out max space needed and add a bit of spare space too */
#define MP_MAXLINE ((MP_MAXDIGITS+1) * MP_NUMSPERLINE)
#define MP_BUFSIZE ((MP_MAXLINE * MP_NUMLINES) + 1 + MP_EXTRABYTES)

char mbprofbuf[MP_BUFSIZE];

void
m_profile(struct mbuf *m)
{
	int segments = 0;
	int used = 0;
	int wasted = 0;
	
	while (m) {
		segments++;
		used += m->m_len;
		if (m->m_flags & M_EXT) {
			wasted += MHLEN - sizeof(m->m_ext) +
			    m->m_ext.ext_size - m->m_len;
		} else {
			if (m->m_flags & M_PKTHDR)
				wasted += MHLEN - m->m_len;
			else
				wasted += MLEN - m->m_len;
		}
		m = m->m_next;
	}
	/* be paranoid.. it helps */
	if (segments > MP_BUCKETS - 1)
		segments = MP_BUCKETS - 1;
	if (used > 100000)
		used = 100000;
	if (wasted > 100000)
		wasted = 100000;
	/* store in the appropriate bucket */
	/* don't bother locking. if it's slightly off, so what? */
	mbprof.segments[segments]++;
	mbprof.used[fls(used)]++;
	mbprof.wasted[fls(wasted)]++;
}

static void
mbprof_textify(void)
{
	int offset;
	char *c;
	uint64_t *p;
	

	p = &mbprof.wasted[0];
	c = mbprofbuf;
	offset = snprintf(c, MP_MAXLINE + 10, 
	    "wasted:\n"
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#ifdef BIG_ARRAY
	p = &mbprof.wasted[16];
	c += offset;
	offset = snprintf(c, MP_MAXLINE, 
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#endif
	p = &mbprof.used[0];
	c += offset;
	offset = snprintf(c, MP_MAXLINE + 10, 
	    "used:\n"
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#ifdef BIG_ARRAY
	p = &mbprof.used[16];
	c += offset;
	offset = snprintf(c, MP_MAXLINE, 
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#endif
	p = &mbprof.segments[0];
	c += offset;
	offset = snprintf(c, MP_MAXLINE + 10, 
	    "segments:\n"
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#ifdef BIG_ARRAY
	p = &mbprof.segments[16];
	c += offset;
	offset = snprintf(c, MP_MAXLINE, 
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %jju",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#endif
}

static int
mbprof_handler(SYSCTL_HANDLER_ARGS)
{
	int error;

	mbprof_textify();
	error = SYSCTL_OUT(req, mbprofbuf, strlen(mbprofbuf) + 1);
	return (error);
}

static int
mbprof_clr_handler(SYSCTL_HANDLER_ARGS)
{
	int clear, error;
 
	clear = 0;
	error = sysctl_handle_int(oidp, &clear, 0, req);
	if (error || !req->newptr)
		return (error);
 
	if (clear) {
		bzero(&mbprof, sizeof(mbprof));
	}
 
	return (error);
}


SYSCTL_PROC(_kern_ipc, OID_AUTO, mbufprofile, CTLTYPE_STRING|CTLFLAG_RD,
	    NULL, 0, mbprof_handler, "A", "mbuf profiling statistics");

SYSCTL_PROC(_kern_ipc, OID_AUTO, mbufprofileclr, CTLTYPE_INT|CTLFLAG_RW,
	    NULL, 0, mbprof_clr_handler, "I", "clear mbuf profiling statistics");
#endif

