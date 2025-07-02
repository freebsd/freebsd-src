/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
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
#include <sys/vmmeter.h>
#include <sys/sbuf.h>
#include <sys/sdt.h>
#include <vm/vm.h>
#include <vm/vm_pageout.h>
#include <vm/vm_page.h>

SDT_PROBE_DEFINE5_XLATE(sdt, , , m__init,
    "struct mbuf *", "mbufinfo_t *",
    "uint32_t", "uint32_t",
    "uint16_t", "uint16_t",
    "uint32_t", "uint32_t",
    "uint32_t", "uint32_t");

SDT_PROBE_DEFINE3_XLATE(sdt, , , m__gethdr_raw,
    "uint32_t", "uint32_t",
    "uint16_t", "uint16_t",
    "struct mbuf *", "mbufinfo_t *");

SDT_PROBE_DEFINE3_XLATE(sdt, , , m__gethdr,
    "uint32_t", "uint32_t",
    "uint16_t", "uint16_t",
    "struct mbuf *", "mbufinfo_t *");

SDT_PROBE_DEFINE3_XLATE(sdt, , , m__get_raw,
    "uint32_t", "uint32_t",
    "uint16_t", "uint16_t",
    "struct mbuf *", "mbufinfo_t *");

SDT_PROBE_DEFINE3_XLATE(sdt, , , m__get,
    "uint32_t", "uint32_t",
    "uint16_t", "uint16_t",
    "struct mbuf *", "mbufinfo_t *");

SDT_PROBE_DEFINE4_XLATE(sdt, , , m__getcl,
    "uint32_t", "uint32_t",
    "uint16_t", "uint16_t",
    "uint32_t", "uint32_t",
    "struct mbuf *", "mbufinfo_t *");

SDT_PROBE_DEFINE5_XLATE(sdt, , , m__getjcl,
    "uint32_t", "uint32_t",
    "uint16_t", "uint16_t",
    "uint32_t", "uint32_t",
    "uint32_t", "uint32_t",
    "struct mbuf *", "mbufinfo_t *");

SDT_PROBE_DEFINE3_XLATE(sdt, , , m__clget,
    "struct mbuf *", "mbufinfo_t *",
    "uint32_t", "uint32_t",
    "uint32_t", "uint32_t");

SDT_PROBE_DEFINE4_XLATE(sdt, , , m__cljget,
    "struct mbuf *", "mbufinfo_t *",
    "uint32_t", "uint32_t",
    "uint32_t", "uint32_t",
    "void*", "void*");

SDT_PROBE_DEFINE(sdt, , , m__cljset);

SDT_PROBE_DEFINE1_XLATE(sdt, , , m__free,
        "struct mbuf *", "mbufinfo_t *");

SDT_PROBE_DEFINE1_XLATE(sdt, , , m__freem,
    "struct mbuf *", "mbufinfo_t *");

SDT_PROBE_DEFINE1_XLATE(sdt, , , m__freemp,
    "struct mbuf *", "mbufinfo_t *");

#include <security/mac/mac_framework.h>

/*
 * Provide minimum possible defaults for link and protocol header space,
 * assuming IPv4 over Ethernet.  Enabling IPv6, IEEE802.11 or some other
 * protocol may grow these values.
 */
u_int	max_linkhdr = 16;
u_int	max_protohdr = 40;
u_int	max_hdr = 16 + 40;
SYSCTL_INT(_kern_ipc, KIPC_MAX_LINKHDR, max_linkhdr, CTLFLAG_RD,
	   &max_linkhdr, 16, "Size of largest link layer header");
SYSCTL_INT(_kern_ipc, KIPC_MAX_PROTOHDR, max_protohdr, CTLFLAG_RD,
	   &max_protohdr, 40, "Size of largest protocol layer header");
SYSCTL_INT(_kern_ipc, KIPC_MAX_HDR, max_hdr, CTLFLAG_RD,
	   &max_hdr, 16 + 40, "Size of largest link plus protocol header");

static void
max_hdr_grow(void)
{

	max_hdr = max_linkhdr + max_protohdr;
	MPASS(max_hdr <= MHLEN);
}

void
max_linkhdr_grow(u_int new)
{

	if (new > max_linkhdr) {
		max_linkhdr = new;
		max_hdr_grow();
	}
}

void
max_protohdr_grow(u_int new)
{

	if (new > max_protohdr) {
		max_protohdr = new;
		max_hdr_grow();
	}
}

#ifdef MBUF_STRESS_TEST
int	m_defragpackets;
int	m_defragbytes;
int	m_defraguseless;
int	m_defragfailure;
int	m_defragrandomfailures;

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
 * Ensure the correct size of various mbuf parameters.  It could be off due
 * to compiler-induced padding and alignment artifacts.
 */
CTASSERT(MSIZE - offsetof(struct mbuf, m_dat) == MLEN);
CTASSERT(MSIZE - offsetof(struct mbuf, m_pktdat) == MHLEN);

/*
 * mbuf data storage should be 64-bit aligned regardless of architectural
 * pointer size; check this is the case with and without a packet header.
 */
CTASSERT(offsetof(struct mbuf, m_dat) % 8 == 0);
CTASSERT(offsetof(struct mbuf, m_pktdat) % 8 == 0);

/*
 * While the specific values here don't matter too much (i.e., +/- a few
 * words), we do want to ensure that changes to these values are carefully
 * reasoned about and properly documented.  This is especially the case as
 * network-protocol and device-driver modules encode these layouts, and must
 * be recompiled if the structures change.  Check these values at compile time
 * against the ones documented in comments in mbuf.h.
 *
 * NB: Possibly they should be documented there via #define's and not just
 * comments.
 */
#if defined(__LP64__)
CTASSERT(offsetof(struct mbuf, m_dat) == 32);
CTASSERT(sizeof(struct pkthdr) == 64);
CTASSERT(sizeof(struct m_ext) == 160);
#else
CTASSERT(offsetof(struct mbuf, m_dat) == 24);
CTASSERT(sizeof(struct pkthdr) == 56);
#if defined(__powerpc__) && defined(BOOKE)
/* PowerPC booke has 64-bit physical pointers. */
CTASSERT(sizeof(struct m_ext) == 176);
#else
CTASSERT(sizeof(struct m_ext) == 172);
#endif
#endif

/*
 * Assert that the queue(3) macros produce code of the same size as an old
 * plain pointer does.
 */
#ifdef INVARIANTS
static struct mbuf __used m_assertbuf;
CTASSERT(sizeof(m_assertbuf.m_slist) == sizeof(m_assertbuf.m_next));
CTASSERT(sizeof(m_assertbuf.m_stailq) == sizeof(m_assertbuf.m_next));
CTASSERT(sizeof(m_assertbuf.m_slistpkt) == sizeof(m_assertbuf.m_nextpkt));
CTASSERT(sizeof(m_assertbuf.m_stailqpkt) == sizeof(m_assertbuf.m_nextpkt));
#endif

/*
 * Attach the cluster from *m to *n, set up m_ext in *n
 * and bump the refcount of the cluster.
 */
void
mb_dupcl(struct mbuf *n, struct mbuf *m)
{
	volatile u_int *refcnt;

	KASSERT(m->m_flags & (M_EXT | M_EXTPG),
	    ("%s: M_EXT | M_EXTPG not set on %p", __func__, m));
	KASSERT(!(n->m_flags & (M_EXT | M_EXTPG)),
	    ("%s: M_EXT | M_EXTPG set on %p", __func__, n));

	/*
	 * Cache access optimization.
	 *
	 * o Regular M_EXT storage doesn't need full copy of m_ext, since
	 *   the holder of the 'ext_count' is responsible to carry the free
	 *   routine and its arguments.
	 * o M_EXTPG data is split between main part of mbuf and m_ext, the
	 *   main part is copied in full, the m_ext part is similar to M_EXT.
	 * o EXT_EXTREF, where 'ext_cnt' doesn't point into mbuf at all, is
	 *   special - it needs full copy of m_ext into each mbuf, since any
	 *   copy could end up as the last to free.
	 */
	if (m->m_flags & M_EXTPG) {
		bcopy(&m->m_epg_startcopy, &n->m_epg_startcopy,
		    __rangeof(struct mbuf, m_epg_startcopy, m_epg_endcopy));
		bcopy(&m->m_ext, &n->m_ext, m_epg_ext_copylen);
	} else if (m->m_ext.ext_type == EXT_EXTREF)
		bcopy(&m->m_ext, &n->m_ext, sizeof(struct m_ext));
	else
		bcopy(&m->m_ext, &n->m_ext, m_ext_copylen);

	n->m_flags |= m->m_flags & (M_RDONLY | M_EXT | M_EXTPG);

	/* See if this is the mbuf that holds the embedded refcount. */
	if (m->m_ext.ext_flags & EXT_FLAG_EMBREF) {
		refcnt = n->m_ext.ext_cnt = &m->m_ext.ext_count;
		n->m_ext.ext_flags &= ~EXT_FLAG_EMBREF;
	} else {
		KASSERT(m->m_ext.ext_cnt != NULL,
		    ("%s: no refcounting pointer on %p", __func__, m));
		refcnt = m->m_ext.ext_cnt;
	}

	if (*refcnt == 1)
		*refcnt += 1;
	else
		atomic_add_int(refcnt, 1);
}

void
m_demote_pkthdr(struct mbuf *m)
{

	M_ASSERTPKTHDR(m);
	M_ASSERT_NO_SND_TAG(m);

	m_tag_delete_chain(m, NULL);
	m->m_flags &= ~M_PKTHDR;
	bzero(&m->m_pkthdr, sizeof(struct pkthdr));
}

/*
 * Clean up mbuf (chain) from any tags and packet headers.
 * If "all" is set then the first mbuf in the chain will be
 * cleaned too.
 */
void
m_demote(struct mbuf *m0, int all, int flags)
{
	struct mbuf *m;

	flags |= M_DEMOTEFLAGS;

	for (m = all ? m0 : m0->m_next; m != NULL; m = m->m_next) {
		KASSERT(m->m_nextpkt == NULL, ("%s: m_nextpkt in m %p, m0 %p",
		    __func__, m, m0));
		if (m->m_flags & M_PKTHDR)
			m_demote_pkthdr(m);
		m->m_flags &= flags;
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
		a = M_START(m);
		b = a + M_SIZE(m);
		if ((caddr_t)m->m_data < a)
			M_SANITY_ACTION("m_data outside mbuf data range left");
		if ((caddr_t)m->m_data > b)
			M_SANITY_ACTION("m_data outside mbuf data range right");
		if ((caddr_t)m->m_data + m->m_len > b)
			M_SANITY_ACTION("m_data + m_len exeeds mbuf space");

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
 * Non-inlined part of m_init().
 */
int
m_pkthdr_init(struct mbuf *m, int how)
{
#ifdef MAC
	int error;
#endif
	m->m_data = m->m_pktdat;
	bzero(&m->m_pkthdr, sizeof(m->m_pkthdr));
#ifdef NUMA
	m->m_pkthdr.numa_domain = M_NODOM;
#endif
#ifdef MAC
	/* If the label init fails, fail the alloc */
	error = mac_mbuf_init(m, how);
	if (error)
		return (error);
#endif

	return (0);
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
	to->m_flags = (from->m_flags & M_COPYFLAGS) |
	    (to->m_flags & (M_EXT | M_EXTPG));
	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;
	to->m_pkthdr = from->m_pkthdr;		/* especially tags */
	SLIST_INIT(&from->m_pkthdr.tags);	/* purge tags from src */
	from->m_flags &= ~M_PKTHDR;
	if (from->m_pkthdr.csum_flags & CSUM_SND_TAG) {
		from->m_pkthdr.csum_flags &= ~CSUM_SND_TAG;
		from->m_pkthdr.snd_tag = NULL;
	}
}

/*
 * Duplicate "from"'s mbuf pkthdr in "to".
 * "from" must have M_PKTHDR set, and "to" must be empty.
 * In particular, this does a deep copy of the packet tags.
 */
int
m_dup_pkthdr(struct mbuf *to, const struct mbuf *from, int how)
{

#if 0
	/*
	 * The mbuf allocator only initializes the pkthdr
	 * when the mbuf is allocated with m_gethdr(). Many users
	 * (e.g. m_copy*, m_prepend) use m_get() and then
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
	to->m_flags = (from->m_flags & M_COPYFLAGS) |
	    (to->m_flags & (M_EXT | M_EXTPG));
	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;
	to->m_pkthdr = from->m_pkthdr;
	if (from->m_pkthdr.csum_flags & CSUM_SND_TAG)
		m_snd_tag_ref(from->m_pkthdr.snd_tag);
	SLIST_INIT(&to->m_pkthdr.tags);
	return (m_tag_copy_chain(to, from, how));
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
		mn = m_gethdr(how, m->m_type);
	else
		mn = m_get(how, m->m_type);
	if (mn == NULL) {
		m_freem(m);
		return (NULL);
	}
	if (m->m_flags & M_PKTHDR)
		m_move_pkthdr(mn, m);
	mn->m_next = m;
	m = mn;
	if (len < M_SIZE(m))
		M_ALIGN(m, len);
	m->m_len = len;
	return (m);
}

/*
 * Make a copy of an mbuf chain starting "off0" bytes from the beginning,
 * continuing for "len" bytes.  If len is M_COPYALL, copy to end of mbuf.
 * The wait parameter is a choice of M_WAITOK/M_NOWAIT from caller.
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
	top = NULL;
	while (len > 0) {
		if (m == NULL) {
			KASSERT(len == M_COPYALL,
			    ("m_copym, length > size of mbuf chain"));
			break;
		}
		if (copyhdr)
			n = m_gethdr(wait, m->m_type);
		else
			n = m_get(wait, m->m_type);
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
		if (m->m_flags & (M_EXT | M_EXTPG)) {
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

	return (top);
nospace:
	m_freem(top);
	return (NULL);
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
	n = m_get(how, m->m_type);
	top = n;
	if (n == NULL)
		goto nospace;

	if (!m_dup_pkthdr(n, m, how))
		goto nospace;
	n->m_len = m->m_len;
	if (m->m_flags & (M_EXT | M_EXTPG)) {
		n->m_data = m->m_data;
		mb_dupcl(n, m);
	} else {
		n->m_data = n->m_pktdat + (m->m_data - m->m_pktdat );
		bcopy(mtod(m, char *), mtod(n, char *), n->m_len);
	}

	m = m->m_next;
	while (m) {
		o = m_get(how, m->m_type);
		if (o == NULL)
			goto nospace;

		n->m_next = o;
		n = n->m_next;

		n->m_len = m->m_len;
		if (m->m_flags & (M_EXT | M_EXTPG)) {
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
	return (NULL);
}

static void
m_copyfromunmapped(const struct mbuf *m, int off, int len, caddr_t cp)
{
	struct iovec iov;
	struct uio uio;
	int error __diagused;

	KASSERT(off >= 0, ("m_copyfromunmapped: negative off %d", off));
	KASSERT(len >= 0, ("m_copyfromunmapped: negative len %d", len));
	KASSERT(off < m->m_len,
	    ("m_copyfromunmapped: len exceeds mbuf length"));
	iov.iov_base = cp;
	iov.iov_len = len;
	uio.uio_resid = len;
	uio.uio_iov = &iov;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_rw = UIO_READ;
	error = m_unmapped_uiomove(m, off, &uio, len);
	KASSERT(error == 0, ("m_unmapped_uiomove failed: off %d, len %d", off,
	   len));
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
		if ((m->m_flags & M_EXTPG) != 0)
			m_copyfromunmapped(m, off, count, cp);
		else
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
m_dup(const struct mbuf *m, int how)
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
			n->m_flags &= ~M_RDONLY;
		}
		n->m_len = 0;

		/* Link it into the new chain */
		*p = n;
		p = &n->m_next;

		/* Copy data from original mbuf(s) into new mbuf */
		while (n->m_len < nsize && m != NULL) {
			int chunk = min(nsize - n->m_len, m->m_len - moff);

			m_copydata(m, moff, chunk, n->m_data + n->m_len);
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
		if (!M_WRITABLE(m) ||
		    (n->m_flags & M_EXTPG) != 0 ||
		    M_TRAILINGSPACE(m) < n->m_len) {
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

/*
 * Concatenate two pkthdr mbuf chains.
 */
void
m_catpkt(struct mbuf *m, struct mbuf *n)
{

	M_ASSERTPKTHDR(m);
	M_ASSERTPKTHDR(n);

	m->m_pkthdr.len += n->m_pkthdr.len;
	m_demote(n, 1, 0);

	m_cat(m, n);
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

void
m_adj_decap(struct mbuf *mp, int len)
{
	uint8_t rsstype;

	m_adj(mp, len);
	if ((mp->m_flags & M_PKTHDR) != 0) {
		/*
		 * If flowid was calculated by card from the inner
		 * headers, move flowid to the decapsulated mbuf
		 * chain, otherwise clear.  This depends on the
		 * internals of m_adj, which keeps pkthdr as is, in
		 * particular not changing rsstype and flowid.
		 */
		rsstype = mp->m_pkthdr.rsstype;
		if ((rsstype & M_HASHTYPE_INNER) != 0) {
			M_HASHTYPE_SET(mp, rsstype & ~M_HASHTYPE_INNER);
		} else {
			M_HASHTYPE_CLEAR(mp);
		}
	}
}

/*
 * Rearange an mbuf chain so that len bytes are contiguous
 * and in the data area of an mbuf (so that mtod will work
 * for a structure of size len).  Returns the resulting
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

	KASSERT((n->m_flags & M_EXTPG) == 0,
	    ("%s: unmapped mbuf %p", __func__, n));

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
		m = m_get(M_NOWAIT, n->m_type);
		if (m == NULL)
			goto bad;
		if (n->m_flags & M_PKTHDR)
			m_move_pkthdr(m, n);
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
	return (NULL);
}

/*
 * Like m_pullup(), except a new mbuf is always allocated, and we allow
 * the amount of empty space before the data in the new mbuf to be specified
 * (in the event that the caller expects to prepend later).
 */
struct mbuf *
m_copyup(struct mbuf *n, int len, int dstoff)
{
	struct mbuf *m;
	int count, space;

	if (len > (MHLEN - dstoff))
		goto bad;
	m = m_get(M_NOWAIT, n->m_type);
	if (m == NULL)
		goto bad;
	if (n->m_flags & M_PKTHDR)
		m_move_pkthdr(m, n);
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
	if (m0->m_flags & M_PKTHDR && remain == 0) {
		n = m_gethdr(wait, m0->m_type);
		if (n == NULL)
			return (NULL);
		n->m_next = m->m_next;
		m->m_next = NULL;
		if (m0->m_pkthdr.csum_flags & CSUM_SND_TAG) {
			n->m_pkthdr.snd_tag =
			    m_snd_tag_ref(m0->m_pkthdr.snd_tag);
			n->m_pkthdr.csum_flags |= CSUM_SND_TAG;
		} else
			n->m_pkthdr.rcvif = m0->m_pkthdr.rcvif;
		n->m_pkthdr.len = m0->m_pkthdr.len - len0;
		m0->m_pkthdr.len = len0;
		return (n);
	} else if (m0->m_flags & M_PKTHDR) {
		n = m_gethdr(wait, m0->m_type);
		if (n == NULL)
			return (NULL);
		if (m0->m_pkthdr.csum_flags & CSUM_SND_TAG) {
			n->m_pkthdr.snd_tag =
			    m_snd_tag_ref(m0->m_pkthdr.snd_tag);
			n->m_pkthdr.csum_flags |= CSUM_SND_TAG;
		} else
			n->m_pkthdr.rcvif = m0->m_pkthdr.rcvif;
		n->m_pkthdr.len = m0->m_pkthdr.len - len0;
		m0->m_pkthdr.len = len0;
		if (m->m_flags & (M_EXT | M_EXTPG))
			goto extpacket;
		if (remain > MHLEN) {
			/* m can't be the lead packet */
			M_ALIGN(n, 0);
			n->m_next = m_split(m, len, wait);
			if (n->m_next == NULL) {
				(void) m_free(n);
				return (NULL);
			} else {
				n->m_len = 0;
				return (n);
			}
		} else
			M_ALIGN(n, remain);
	} else if (remain == 0) {
		n = m->m_next;
		m->m_next = NULL;
		return (n);
	} else {
		n = m_get(wait, m->m_type);
		if (n == NULL)
			return (NULL);
		M_ALIGN(n, remain);
	}
extpacket:
	if (m->m_flags & (M_EXT | M_EXTPG)) {
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
 * Partition mchain in two pieces, keeping len0 bytes in head and transferring
 * remainder to tail.  In case of failure, both chains to be left untouched.
 * M_EOR is observed correctly.
 * Resulting mbufs might be read-only.
 */
int
mc_split(struct mchain *head, struct mchain *tail, u_int len0, int wait)
{
	struct mbuf *m, *n;
	u_int len, mlen, remain;

	MPASS(!(mc_first(head)->m_flags & M_PKTHDR));
	MBUF_CHECKSLEEP(wait);

	mlen = 0;
	len = len0;
	STAILQ_FOREACH(m, &head->mc_q, m_stailq) {
		mlen += MSIZE;
		if (m->m_flags & M_EXT)
			mlen += m->m_ext.ext_size;
		if (len > m->m_len)
			len -= m->m_len;
		else
			break;
	}
	if (__predict_false(m == NULL)) {
		*tail = MCHAIN_INITIALIZER(tail);
		return (0);
	}
	remain = m->m_len - len;
	if (remain > 0) {
		if (__predict_false((n = m_get(wait, m->m_type)) == NULL))
			return (ENOMEM);
		m_align(n, remain);
		if (m->m_flags & M_EXT) {
			n->m_data = m->m_data + len;
			mb_dupcl(n, m);
		} else
			bcopy(mtod(m, char *) + len, mtod(n, char *), remain);
	}

	/* XXXGL: need STAILQ_SPLIT */
	STAILQ_FIRST(&tail->mc_q) = STAILQ_NEXT(m, m_stailq);
	tail->mc_q.stqh_last = head->mc_q.stqh_last;
	tail->mc_len = head->mc_len - len0;
	tail->mc_mlen = head->mc_mlen - mlen;
	if (remain > 0) {
		MPASS(n->m_len == 0);
		mc_prepend(tail, n);
		n->m_len = remain;
		m->m_len -= remain;
		if (m->m_flags & M_EOR) {
			m->m_flags &= ~M_EOR;
			n->m_flags |= M_EOR;
		}
	}
	head->mc_q.stqh_last = &STAILQ_NEXT(m, m_stailq);
	STAILQ_NEXT(m, m_stailq) = NULL;
	head->mc_len = len0;
	head->mc_mlen = mlen;

	return (0);
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
				m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
				len = MCLBYTES;
			} else {
				m = m_gethdr(M_NOWAIT, MT_DATA);
				len = MHLEN;

				/* Place initial small packet/header at end of mbuf */
				if (m && totlen + off + max_linkhdr <= MHLEN) {
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
				m = m_getcl(M_NOWAIT, MT_DATA, 0);
				len = MCLBYTES;
			} else {
				m = m_get(M_NOWAIT, MT_DATA);
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

static void
m_copytounmapped(const struct mbuf *m, int off, int len, c_caddr_t cp)
{
	struct iovec iov;
	struct uio uio;
	int error __diagused;

	KASSERT(off >= 0, ("m_copytounmapped: negative off %d", off));
	KASSERT(len >= 0, ("m_copytounmapped: negative len %d", len));
	KASSERT(off < m->m_len, ("m_copytounmapped: len exceeds mbuf length"));
	iov.iov_base = __DECONST(caddr_t, cp);
	iov.iov_len = len;
	uio.uio_resid = len;
	uio.uio_iov = &iov;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_rw = UIO_WRITE;
	error = m_unmapped_uiomove(m, off, &uio, len);
	KASSERT(error == 0, ("m_unmapped_uiomove failed: off %d, len %d", off,
	   len));
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
			n = m_get(M_NOWAIT, m->m_type);
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
		if ((m->m_flags & M_EXTPG) != 0)
			m_copytounmapped(m, off, mlen, cp);
		else
			bcopy(cp, off + mtod(m, caddr_t), (u_int)mlen);
		cp += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->m_next == NULL) {
			n = m_get(M_NOWAIT, m->m_type);
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
		n = m_get(M_NOWAIT, m->m_type);
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

static int
m_apply_extpg_one(struct mbuf *m, int off, int len,
    int (*f)(void *, void *, u_int), void *arg)
{
	void *p;
	u_int i, count, pgoff, pglen;
	int rval;

	KASSERT(PMAP_HAS_DMAP,
	    ("m_apply_extpg_one does not support unmapped mbufs"));
	off += mtod(m, vm_offset_t);
	if (off < m->m_epg_hdrlen) {
		count = min(m->m_epg_hdrlen - off, len);
		rval = f(arg, m->m_epg_hdr + off, count);
		if (rval)
			return (rval);
		len -= count;
		off = 0;
	} else
		off -= m->m_epg_hdrlen;
	pgoff = m->m_epg_1st_off;
	for (i = 0; i < m->m_epg_npgs && len > 0; i++) {
		pglen = m_epg_pagelen(m, i, pgoff);
		if (off < pglen) {
			count = min(pglen - off, len);
			p = (void *)PHYS_TO_DMAP(m->m_epg_pa[i] + pgoff + off);
			rval = f(arg, p, count);
			if (rval)
				return (rval);
			len -= count;
			off = 0;
		} else
			off -= pglen;
		pgoff = 0;
	}
	if (len > 0) {
		KASSERT(off < m->m_epg_trllen,
		    ("m_apply_extpg_one: offset beyond trailer"));
		KASSERT(len <= m->m_epg_trllen - off,
		    ("m_apply_extpg_one: length beyond trailer"));
		return (f(arg, m->m_epg_trail + off, len));
	}
	return (0);
}

/* Apply function f to the data in a single mbuf. */
static int
m_apply_one(struct mbuf *m, int off, int len,
    int (*f)(void *, void *, u_int), void *arg)
{
	if ((m->m_flags & M_EXTPG) != 0)
		return (m_apply_extpg_one(m, off, len, f, arg));
	else
		return (f(arg, mtod(m, caddr_t) + off, len));
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
		KASSERT(m != NULL, ("m_apply, offset > size of mbuf chain "
		    "(%d extra)", off));
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		KASSERT(m != NULL, ("m_apply, length > size of mbuf chain "
		    "(%d extra)", len));
		count = min(m->m_len - off, len);
		rval = m_apply_one(m, off, count, f, arg);
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

	if (m == NULL) {
		printf("mbuf: %p\n", m);
		return;
	}

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
 * Return the number of fragments an mbuf will use.  This is usually
 * used as a proxy for the number of scatter/gather elements needed by
 * a DMA engine to access an mbuf.  In general mapped mbufs are
 * assumed to be backed by physically contiguous buffers that only
 * need a single fragment.  Unmapped mbufs, on the other hand, can
 * span disjoint physical pages.
 */
static int
frags_per_mbuf(struct mbuf *m)
{
	int frags;

	if ((m->m_flags & M_EXTPG) == 0)
		return (1);

	/*
	 * The header and trailer are counted as a single fragment
	 * each when present.
	 *
	 * XXX: This overestimates the number of fragments by assuming
	 * all the backing physical pages are disjoint.
	 */
	frags = 0;
	if (m->m_epg_hdrlen != 0)
		frags++;
	frags += m->m_epg_npgs;
	if (m->m_epg_trllen != 0)
		frags++;

	return (frags);
}

/*
 * Defragment an mbuf chain, returning at most maxfrags separate
 * mbufs+clusters.  If this is not possible NULL is returned and
 * the original mbuf chain is left in its present (potentially
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
		curfrags += frags_per_mbuf(m);
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
		if (M_WRITABLE(m) &&
		    n->m_len < M_TRAILINGSPACE(m)) {
			m_copydata(n, 0, n->m_len,
			    mtod(m, char *) + m->m_len);
			m->m_len += n->m_len;
			m->m_next = n->m_next;
			curfrags -= frags_per_mbuf(n);
			m_free(n);
			if (curfrags <= maxfrags)
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
			m_copydata(n, 0,  n->m_len, mtod(m, char *));
			m_copydata(n2, 0,  n2->m_len,
			    mtod(m, char *) + n->m_len);
			m->m_len = n->m_len + n2->m_len;
			m->m_next = n2->m_next;
			*prev = m;
			curfrags += 1;  /* For the new cluster */
			curfrags -= frags_per_mbuf(n);
			curfrags -= frags_per_mbuf(n2);
			m_free(n);
			m_free(n2);
			if (curfrags <= maxfrags)
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
	struct mbuf *m_first, *m_last;
	int divisor = 255, progress = 0, fraglen;

	if (!(m0->m_flags & M_PKTHDR))
		return (m0);

	if (length == 0 || length < -2)
		return (m0);
	if (length > MCLBYTES)
		length = MCLBYTES;
	if (length < 0 && divisor > MCLBYTES)
		divisor = MCLBYTES;
	if (length == -1)
		length = 1 + (arc4random() % divisor);
	if (length > 0)
		fraglen = length;

	m_fixhdr(m0); /* Needed sanity check */

	m_first = m_getcl(how, MT_DATA, M_PKTHDR);
	if (m_first == NULL)
		goto nospace;

	if (m_dup_pkthdr(m_first, m0, how) == 0)
		goto nospace;

	m_last = m_first;

	while (progress < m0->m_pkthdr.len) {
		if (length == -2)
			fraglen = 1 + (arc4random() % divisor);
		if (fraglen > m0->m_pkthdr.len - progress)
			fraglen = m0->m_pkthdr.len - progress;

		if (progress != 0) {
			struct mbuf *m_new = m_getcl(how, MT_DATA, 0);
			if (m_new == NULL)
				goto nospace;

			m_last->m_next = m_new;
			m_last = m_new;
		}

		m_copydata(m0, progress, fraglen, mtod(m_last, caddr_t));
		progress += fraglen;
		m_last->m_len = fraglen;
	}
	m_freem(m0);
	m0 = m_first;
	return (m0);
nospace:
	if (m_first)
		m_freem(m_first);
	/* Return the original chain on failure */
	return (m0);
}

#endif

/*
 * Free pages from mbuf_ext_pgs, assuming they were allocated via
 * vm_page_alloc() and aren't associated with any object.  Complement
 * to allocator from m_uiotombuf_nomap().
 */
void
mb_free_mext_pgs(struct mbuf *m)
{
	vm_page_t pg;

	M_ASSERTEXTPG(m);
	for (int i = 0; i < m->m_epg_npgs; i++) {
		pg = PHYS_TO_VM_PAGE(m->m_epg_pa[i]);
		vm_page_unwire_noq(pg);
		vm_page_free(pg);
	}
}

static struct mbuf *
m_uiotombuf_nomap(struct uio *uio, int how, int len, int maxseg, int flags)
{
	struct mbuf *m, *mb, *prev;
	vm_page_t pg_array[MBUF_PEXT_MAX_PGS];
	int error, length, i, needed;
	ssize_t total;
	int pflags = malloc2vm_flags(how) | VM_ALLOC_NODUMP | VM_ALLOC_WIRED;

	MPASS((flags & M_PKTHDR) == 0);
	MPASS((how & M_ZERO) == 0);

	/*
	 * len can be zero or an arbitrary large value bound by
	 * the total data supplied by the uio.
	 */
	if (len > 0)
		total = MIN(uio->uio_resid, len);
	else
		total = uio->uio_resid;

	if (maxseg == 0)
		maxseg = MBUF_PEXT_MAX_PGS * PAGE_SIZE;

	/*
	 * If total is zero, return an empty mbuf.  This can occur
	 * for TLS 1.0 connections which send empty fragments as
	 * a countermeasure against the known-IV weakness in CBC
	 * ciphersuites.
	 */
	if (__predict_false(total == 0)) {
		mb = mb_alloc_ext_pgs(how, mb_free_mext_pgs, 0);
		if (mb == NULL)
			return (NULL);
		mb->m_epg_flags = EPG_FLAG_ANON;
		return (mb);
	}

	/*
	 * Allocate the pages
	 */
	m = NULL;
	while (total > 0) {
		mb = mb_alloc_ext_pgs(how, mb_free_mext_pgs, 0);
		if (mb == NULL)
			goto failed;
		if (m == NULL)
			m = mb;
		else
			prev->m_next = mb;
		prev = mb;
		mb->m_epg_flags = EPG_FLAG_ANON;
		needed = length = MIN(maxseg, total);
		for (i = 0; needed > 0; i++, needed -= PAGE_SIZE) {
retry_page:
			pg_array[i] = vm_page_alloc_noobj(pflags);
			if (pg_array[i] == NULL) {
				if (how & M_NOWAIT) {
					goto failed;
				} else {
					vm_wait(NULL);
					goto retry_page;
				}
			}
			mb->m_epg_pa[i] = VM_PAGE_TO_PHYS(pg_array[i]);
			mb->m_epg_npgs++;
		}
		mb->m_epg_last_len = length - PAGE_SIZE * (mb->m_epg_npgs - 1);
		MBUF_EXT_PGS_ASSERT_SANITY(mb);
		total -= length;
		error = uiomove_fromphys(pg_array, 0, length, uio);
		if (error != 0)
			goto failed;
		mb->m_len = length;
		mb->m_ext.ext_size += PAGE_SIZE * mb->m_epg_npgs;
		if (flags & M_PKTHDR)
			m->m_pkthdr.len += length;
	}
	return (m);

failed:
	m_freem(m);
	return (NULL);
}

/*
 * Copy the contents of uio into a properly sized mbuf chain.
 * A compat KPI.  Users are recommended to use direct calls to backing
 * functions.
 */
struct mbuf *
m_uiotombuf(struct uio *uio, int how, int len, int lspace, int flags)
{

	if (flags & M_EXTPG) {
		/* XXX: 'lspace' magically becomes maxseg! */
		return (m_uiotombuf_nomap(uio, how, len, lspace, flags));
	} else if (__predict_false(uio->uio_resid == 0)) {
		struct mbuf *m;

		/*
		 * m_uiotombuf() is known to return zero length buffer, keep
		 * this compatibility. mc_uiotomc() won't do that.
		 */
		if (flags & M_PKTHDR) {
			m = m_gethdr(how, MT_DATA);
			m->m_pkthdr.memlen = MSIZE;
		} else
			m = m_get(how, MT_DATA);
		if (m != NULL)
			m->m_data += lspace;
		return (m);
	} else {
		struct mchain mc;
		int error;

		error = mc_uiotomc(&mc, uio, len, lspace, how, flags);
		if (__predict_true(error == 0)) {
			if (flags & M_PKTHDR) {
				mc_first(&mc)->m_pkthdr.len = mc.mc_len;
				mc_first(&mc)->m_pkthdr.memlen = mc.mc_mlen;
			}
			return (mc_first(&mc));
		} else
			return (NULL);
	}
}

/*
 * Copy the contents of uio into a properly sized mbuf chain.
 * @param length Limit copyout length.  If 0 entire uio_resid is copied.
 * @param lspace Provide leading space in the first mbuf in the chain.
 */
int
mc_uiotomc(struct mchain *mc, struct uio *uio, u_int length, u_int lspace,
    int how, int flags)
{
	struct mbuf *mb;
	u_int total;
	int error;

	MPASS(lspace < MHLEN);
	MPASS(UINT_MAX - lspace >= length);
	MPASS(uio->uio_rw == UIO_WRITE);
	MPASS(uio->uio_resid >= 0);

	if (length > 0) {
		if (uio->uio_resid > length) {
			total = length;
			flags &= ~M_EOR;
		} else
			total = uio->uio_resid;
	} else if (__predict_false(uio->uio_resid + lspace > UINT_MAX))
		return (EOVERFLOW);
	else
		total = uio->uio_resid;

	if (__predict_false(total + lspace == 0)) {
		*mc = MCHAIN_INITIALIZER(mc);
		return (0);
	}

	error = mc_get(mc, total + lspace, how, MT_DATA, flags);
	if (__predict_false(error))
		return (error);
	mc_first(mc)->m_data += lspace;

	/* Fill all mbufs with uio data and update header information. */
	STAILQ_FOREACH(mb, &mc->mc_q, m_stailq) {
		u_int mlen;

		mlen = min(M_TRAILINGSPACE(mb), total - mc->mc_len);
		error = uiomove(mtod(mb, void *), mlen, uio);
		if (__predict_false(error)) {
			mc_freem(mc);
			*mc = MCHAIN_INITIALIZER(mc);
			return (error);
		}
		mb->m_len = mlen;
		mc->mc_len += mlen;
	}
	MPASS(mc->mc_len == total);

	return (0);
}

/*
 * Copy data to/from an unmapped mbuf into a uio limited by len if set.
 */
int
m_unmapped_uiomove(const struct mbuf *m, int m_off, struct uio *uio, int len)
{
	vm_page_t pg;
	int error, i, off, pglen, pgoff, seglen, segoff;

	M_ASSERTEXTPG(m);
	error = 0;

	/* Skip over any data removed from the front. */
	off = mtod(m, vm_offset_t);

	off += m_off;
	if (m->m_epg_hdrlen != 0) {
		if (off >= m->m_epg_hdrlen) {
			off -= m->m_epg_hdrlen;
		} else {
			seglen = m->m_epg_hdrlen - off;
			segoff = off;
			seglen = min(seglen, len);
			off = 0;
			len -= seglen;
			error = uiomove(__DECONST(void *,
			    &m->m_epg_hdr[segoff]), seglen, uio);
		}
	}
	pgoff = m->m_epg_1st_off;
	for (i = 0; i < m->m_epg_npgs && error == 0 && len > 0; i++) {
		pglen = m_epg_pagelen(m, i, pgoff);
		if (off >= pglen) {
			off -= pglen;
			pgoff = 0;
			continue;
		}
		seglen = pglen - off;
		segoff = pgoff + off;
		off = 0;
		seglen = min(seglen, len);
		len -= seglen;
		pg = PHYS_TO_VM_PAGE(m->m_epg_pa[i]);
		error = uiomove_fromphys(&pg, segoff, seglen, uio);
		pgoff = 0;
	};
	if (len != 0 && error == 0) {
		KASSERT((off + len) <= m->m_epg_trllen,
		    ("off + len > trail (%d + %d > %d, m_off = %d)", off, len,
		    m->m_epg_trllen, m_off));
		error = uiomove(__DECONST(void *, &m->m_epg_trail[off]),
		    len, uio);
	}
	return (error);
}

/*
 * Copy an mbuf chain into a uio limited by len if set.
 */
int
m_mbuftouio(struct uio *uio, const struct mbuf *m, int len)
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

		if ((m->m_flags & M_EXTPG) != 0)
			error = m_unmapped_uiomove(m, 0, uio, length);
		else
			error = uiomove(mtod(m, void *), length, uio);
		if (error)
			return (error);

		progress += length;
	}

	return (0);
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
			continue;
		}

		/*
		 * Allocate new space to hold the copy and copy the data.
		 * We deal with jumbo mbufs (i.e. m_len > MCLBYTES) by
		 * splitting them into clusters.  We could just malloc a
		 * buffer and make it external but too many device drivers
		 * don't know how to break up the non-contiguous memory when
		 * doing DMA.
		 */
		n = m_getcl(how, m->m_type, m->m_flags & M_COPYFLAGS);
		if (n == NULL) {
			m_freem(m0);
			return (NULL);
		}
		if (m->m_flags & M_PKTHDR) {
			KASSERT(mprev == NULL, ("%s: m0 %p, m %p has M_PKTHDR",
			    __func__, m0, m));
			m_move_pkthdr(n, m);
		}
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

			n = m_getcl(how, m->m_type, m->m_flags & M_COPYFLAGS);
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

static int
mbprof_handler(SYSCTL_HANDLER_ARGS)
{
	char buf[256];
	struct sbuf sb;
	int error;
	uint64_t *p;

	sbuf_new_for_sysctl(&sb, buf, sizeof(buf), req);

	p = &mbprof.wasted[0];
	sbuf_printf(&sb,
	    "wasted:\n"
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#ifdef BIG_ARRAY
	p = &mbprof.wasted[16];
	sbuf_printf(&sb,
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#endif
	p = &mbprof.used[0];
	sbuf_printf(&sb,
	    "used:\n"
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#ifdef BIG_ARRAY
	p = &mbprof.used[16];
	sbuf_printf(&sb,
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#endif
	p = &mbprof.segments[0];
	sbuf_printf(&sb,
	    "segments:\n"
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %ju\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#ifdef BIG_ARRAY
	p = &mbprof.segments[16];
	sbuf_printf(&sb,
	    "%ju %ju %ju %ju %ju %ju %ju %ju "
	    "%ju %ju %ju %ju %ju %ju %ju %jju",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#endif

	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
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

SYSCTL_PROC(_kern_ipc, OID_AUTO, mbufprofile,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    mbprof_handler, "A",
    "mbuf profiling statistics");

SYSCTL_PROC(_kern_ipc, OID_AUTO, mbufprofileclr,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    mbprof_clr_handler, "I",
    "clear mbuf profiling statistics");
#endif
