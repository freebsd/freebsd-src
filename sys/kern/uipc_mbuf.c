/*
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
 *	@(#)uipc_mbuf.c	8.2 (Berkeley) 1/4/94
 * $FreeBSD$
 */

#include "opt_param.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/domain.h>
#include <sys/protosw.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#ifdef INVARIANTS
#include <machine/cpu.h>
#endif

static void mbinit __P((void *));
SYSINIT(mbuf, SI_SUB_MBUF, SI_ORDER_FIRST, mbinit, NULL)

struct mbuf *mbutl;
char	*mclrefcnt;
struct mbstat mbstat;
u_long	mbtypes[MT_NTYPES];
struct mbuf *mmbfree;
union mcluster *mclfree;
int	max_linkhdr;
int	max_protohdr;
int	max_hdr;
int	max_datalen;
int	nmbclusters;
int	nmbufs;
u_int	m_mballoc_wid = 0;
u_int	m_clalloc_wid = 0;

SYSCTL_DECL(_kern_ipc);
SYSCTL_INT(_kern_ipc, KIPC_MAX_LINKHDR, max_linkhdr, CTLFLAG_RW,
	   &max_linkhdr, 0, "");
SYSCTL_INT(_kern_ipc, KIPC_MAX_PROTOHDR, max_protohdr, CTLFLAG_RW,
	   &max_protohdr, 0, "");
SYSCTL_INT(_kern_ipc, KIPC_MAX_HDR, max_hdr, CTLFLAG_RW, &max_hdr, 0, "");
SYSCTL_INT(_kern_ipc, KIPC_MAX_DATALEN, max_datalen, CTLFLAG_RW,
	   &max_datalen, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, mbuf_wait, CTLFLAG_RW,
	   &mbuf_wait, 0, "");
SYSCTL_STRUCT(_kern_ipc, KIPC_MBSTAT, mbstat, CTLFLAG_RW, &mbstat, mbstat, "");
SYSCTL_OPAQUE(_kern_ipc, OID_AUTO, mbtypes, CTLFLAG_RD, mbtypes,
	   sizeof(mbtypes), "LU", "");
SYSCTL_INT(_kern_ipc, KIPC_NMBCLUSTERS, nmbclusters, CTLFLAG_RD, 
	   &nmbclusters, 0, "Maximum number of mbuf clusters available");
SYSCTL_INT(_kern_ipc, OID_AUTO, nmbufs, CTLFLAG_RD, &nmbufs, 0,
	   "Maximum number of mbufs available"); 

static void	m_reclaim __P((void));

#ifndef NMBCLUSTERS
#define NMBCLUSTERS	(512 + maxusers * 16)
#endif
#ifndef NMBUFS
#define NMBUFS		(nmbclusters * 4)
#endif

/*
 * Perform sanity checks of tunables declared above.
 */
static void
tunable_mbinit(void *dummy)
{

	/*
	 * This has to be done before VM init.
	 */
	nmbclusters = NMBCLUSTERS;
	TUNABLE_INT_FETCH("kern.ipc.nmbclusters", &nmbclusters);
	nmbufs = NMBUFS;
	TUNABLE_INT_FETCH("kern.ipc.nmbufs", &nmbufs);
	/* Sanity checks */
	if (nmbufs < nmbclusters * 2)
		nmbufs = nmbclusters * 2;

	return;
}
SYSINIT(tunable_mbinit, SI_SUB_TUNABLES, SI_ORDER_ANY, tunable_mbinit, NULL);

/* "number of clusters of pages" */
#define NCL_INIT	1

#define NMB_INIT	16

/* ARGSUSED*/
static void
mbinit(dummy)
	void *dummy;
{
	int s;

	mmbfree = NULL; mclfree = NULL;
	mbstat.m_msize = MSIZE;
	mbstat.m_mclbytes = MCLBYTES;
	mbstat.m_minclsize = MINCLSIZE;
	mbstat.m_mlen = MLEN;
	mbstat.m_mhlen = MHLEN;

	s = splimp();
	if (m_mballoc(NMB_INIT, M_DONTWAIT) == 0)
		goto bad;
#if MCLBYTES <= PAGE_SIZE
	if (m_clalloc(NCL_INIT, M_DONTWAIT) == 0)
		goto bad;
#else
	/* It's OK to call contigmalloc in this context. */
	if (m_clalloc(16, M_WAIT) == 0)
		goto bad;
#endif
	splx(s);
	return;
bad:
	panic("mbinit");
}

/*
 * Allocate at least nmb mbufs and place on mbuf free list.
 * Must be called at splimp.
 */
/* ARGSUSED */
int
m_mballoc(nmb, how)
	register int nmb;
	int how;
{
	register caddr_t p;
	register int i;
	int nbytes;

	/*
	 * If we've hit the mbuf limit, stop allocating from mb_map,
	 * (or trying to) in order to avoid dipping into the section of
	 * mb_map which we've "reserved" for clusters.
	 */
	if ((nmb + mbstat.m_mbufs) > nmbufs)
		return (0);

	/*
	 * Once we run out of map space, it will be impossible to get
	 * any more (nothing is ever freed back to the map)
	 * -- however you are not dead as m_reclaim might
	 * still be able to free a substantial amount of space.
	 *
	 * XXX Furthermore, we can also work with "recycled" mbufs (when
	 * we're calling with M_WAIT the sleep procedure will be woken
	 * up when an mbuf is freed. See m_mballoc_wait()).
	 */
	if (mb_map_full)
		return (0);

	nbytes = round_page(nmb * MSIZE);
	p = (caddr_t)kmem_malloc(mb_map, nbytes, M_NOWAIT);
	if (p == 0 && how == M_WAIT) {
		mbstat.m_wait++;
		p = (caddr_t)kmem_malloc(mb_map, nbytes, M_WAITOK);
	}

	/*
	 * Either the map is now full, or `how' is M_NOWAIT and there
	 * are no pages left.
	 */
	if (p == NULL)
		return (0);

	nmb = nbytes / MSIZE;
	for (i = 0; i < nmb; i++) {
		((struct mbuf *)p)->m_next = mmbfree;
		mmbfree = (struct mbuf *)p;
		p += MSIZE;
	}
	mbstat.m_mbufs += nmb;
	mbtypes[MT_FREE] += nmb;
	return (1);
}

/*
 * Once the mb_map has been exhausted and if the call to the allocation macros
 * (or, in some cases, functions) is with M_WAIT, then it is necessary to rely
 * solely on reclaimed mbufs. Here we wait for an mbuf to be freed for a 
 * designated (mbuf_wait) time. 
 */
struct mbuf *
m_mballoc_wait(int caller, int type)
{
	struct mbuf *p;
	int s;

	s = splimp();
	m_mballoc_wid++;
	if ((tsleep(&m_mballoc_wid, PVM, "mballc", mbuf_wait)) == EWOULDBLOCK)
		m_mballoc_wid--;
	splx(s);

	/*
	 * Now that we (think) that we've got something, we will redo an
	 * MGET, but avoid getting into another instance of m_mballoc_wait()
	 * XXX: We retry to fetch _even_ if the sleep timed out. This is left
	 *      this way, purposely, in the [unlikely] case that an mbuf was
	 *      freed but the sleep was not awakened in time. 
	 */
	p = NULL;
	switch (caller) {
	case MGET_C:
		MGET(p, M_DONTWAIT, type);
		break;
	case MGETHDR_C:
		MGETHDR(p, M_DONTWAIT, type);
		break;
	default:
		panic("m_mballoc_wait: invalid caller (%d)", caller);
	}

	s = splimp();
	if (p != NULL) {		/* We waited and got something... */
		mbstat.m_wait++;
		/* Wake up another if we have more free. */
		if (mmbfree != NULL)
			MMBWAKEUP();
	}
	splx(s);
	return (p);
}

#if MCLBYTES > PAGE_SIZE
static int i_want_my_mcl;

static void
kproc_mclalloc(void)
{
	int status;

	while (1) {
		tsleep(&i_want_my_mcl, PVM, "mclalloc", 0);

		for (; i_want_my_mcl; i_want_my_mcl--) {
			if (m_clalloc(1, M_WAIT) == 0)
				printf("m_clalloc failed even in process context!\n");
		}
	}
}

static struct proc *mclallocproc;
static struct kproc_desc mclalloc_kp = {
	"mclalloc",
	kproc_mclalloc,
	&mclallocproc
};
SYSINIT(mclallocproc, SI_SUB_KTHREAD_UPDATE, SI_ORDER_ANY, kproc_start,
	   &mclalloc_kp);
#endif

/*
 * Allocate some number of mbuf clusters
 * and place on cluster free list.
 * Must be called at splimp.
 */
/* ARGSUSED */
int
m_clalloc(ncl, how)
	register int ncl;
	int how;
{
	register caddr_t p;
	register int i;
	int npg;

	/*
	 * If we've hit the mcluster number limit, stop allocating from
	 * mb_map, (or trying to) in order to avoid dipping into the section
	 * of mb_map which we've "reserved" for mbufs.
	 */
	if ((ncl + mbstat.m_clusters) > nmbclusters) {
		mbstat.m_drops++;
		return (0);
	}

	/*
	 * Once we run out of map space, it will be impossible
	 * to get any more (nothing is ever freed back to the
	 * map). From this point on, we solely rely on freed 
	 * mclusters.
	 */
	if (mb_map_full) {
		mbstat.m_drops++;
		return (0);
	}

#if MCLBYTES > PAGE_SIZE
	if (how != M_WAIT) {
		i_want_my_mcl += ncl;
		wakeup(&i_want_my_mcl);
		mbstat.m_wait++;
		p = 0;
	} else {
		p = contigmalloc1(MCLBYTES * ncl, M_DEVBUF, M_WAITOK, 0ul,
				  ~0ul, PAGE_SIZE, 0, mb_map);
	}
#else
	npg = ncl;
	p = (caddr_t)kmem_malloc(mb_map, ctob(npg),
				 how != M_WAIT ? M_NOWAIT : M_WAITOK);
	ncl = ncl * PAGE_SIZE / MCLBYTES;
#endif
	/*
	 * Either the map is now full, or `how' is M_NOWAIT and there
	 * are no pages left.
	 */
	if (p == NULL) {
		mbstat.m_drops++;
		return (0);
	}

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
 * Once the mb_map submap has been exhausted and the allocation is called with
 * M_WAIT, we rely on the mclfree union pointers. If nothing is free, we will
 * sleep for a designated amount of time (mbuf_wait) or until we're woken up
 * due to sudden mcluster availability.
 */
caddr_t
m_clalloc_wait(void)
{
	caddr_t p;
	int s;

#ifdef __i386__
	/* If in interrupt context, and INVARIANTS, maintain sanity and die. */
	KASSERT(intr_nesting_level == 0, ("CLALLOC: CANNOT WAIT IN INTERRUPT"));
#endif

	/* Sleep until something's available or until we expire. */
	m_clalloc_wid++;
	if ((tsleep(&m_clalloc_wid, PVM, "mclalc", mbuf_wait)) == EWOULDBLOCK)
		m_clalloc_wid--;

	/*
	 * Now that we (think) that we've got something, we will redo and
	 * MGET, but avoid getting into another instance of m_clalloc_wait()
	 */
	p = NULL;
	MCLALLOC(p, M_DONTWAIT);

	s = splimp();
	if (p != NULL) {	/* We waited and got something... */
		mbstat.m_wait++;
		/* Wake up another if we have more free. */
		if (mclfree != NULL)
			MCLWAKEUP();
	}

	splx(s);
	return (p);
}

/*
 * When MGET fails, ask protocols to free space when short of memory,
 * then re-attempt to allocate an mbuf.
 */
struct mbuf *
m_retry(i, t)
	int i, t;
{
	register struct mbuf *m;

	/*
	 * Must only do the reclaim if not in an interrupt context.
	 */
	if (i == M_WAIT) {
#ifdef __i386__
		KASSERT(intr_nesting_level == 0,
		    ("MBALLOC: CANNOT WAIT IN INTERRUPT"));
#endif
		m_reclaim();
	}

	/*
	 * Both m_mballoc_wait and m_retry must be nulled because
	 * when the MGET macro is run from here, we deffinately do _not_
	 * want to enter an instance of m_mballoc_wait() or m_retry() (again!)
	 */
#define m_mballoc_wait(caller,type)    (struct mbuf *)0
#define m_retry(i, t)	(struct mbuf *)0
	MGET(m, i, t);
#undef m_retry
#undef m_mballoc_wait

	if (m != NULL)
		mbstat.m_wait++;
	else
		mbstat.m_drops++;

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

	/*
	 * Must only do the reclaim if not in an interrupt context.
	 */
	if (i == M_WAIT) {
#ifdef __i386__
		KASSERT(intr_nesting_level == 0,
		    ("MBALLOC: CANNOT WAIT IN INTERRUPT"));
#endif
		m_reclaim();
	}

#define m_mballoc_wait(caller,type)    (struct mbuf *)0
#define m_retryhdr(i, t) (struct mbuf *)0
	MGETHDR(m, i, t);
#undef m_retryhdr
#undef m_mballoc_wait

	if (m != NULL)  
		mbstat.m_wait++;
	else    
		mbstat.m_drops++;
	
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
m_get(how, type)
	int how, type;
{
	register struct mbuf *m;

	MGET(m, how, type);
	return (m);
}

struct mbuf *
m_gethdr(how, type)
	int how, type;
{
	register struct mbuf *m;

	MGETHDR(m, how, type);
	return (m);
}

struct mbuf *
m_getclr(how, type)
	int how, type;
{
	register struct mbuf *m;

	MGET(m, how, type);
	if (m == 0)
		return (0);
	bzero(mtod(m, caddr_t), MLEN);
	return (m);
}

/*
 * struct mbuf *
 * m_getm(m, len, how, type)
 *
 * This will allocate len-worth of mbufs and/or mbuf clusters (whatever fits
 * best) and return a pointer to the top of the allocated chain. If m is
 * non-null, then we assume that it is a single mbuf or an mbuf chain to
 * which we want len bytes worth of mbufs and/or clusters attached, and so
 * if we succeed in allocating it, we will just return a pointer to m.
 *
 * If we happen to fail at any point during the allocation, we will free
 * up everything we have already allocated and return NULL.
 *
 */
struct mbuf *
m_getm(struct mbuf *m, int len, int how, int type)
{
	struct mbuf *top, *tail, *mp, *mtail = NULL;

	KASSERT(len >= 0, ("len is < 0 in m_getm"));

	MGET(mp, how, type);
	if (mp == NULL)
		return (NULL);
	else if (len > MINCLSIZE) {
		MCLGET(mp, how);
		if ((mp->m_flags & M_EXT) == 0) {
			m_free(mp);
			return (NULL);
		}
	}
	mp->m_len = 0;
	len -= M_TRAILINGSPACE(mp);

	if (m != NULL)
		for (mtail = m; mtail->m_next != NULL; mtail = mtail->m_next);
	else
		m = mp;

	top = tail = mp;
	while (len > 0) {
		MGET(mp, how, type);
		if (mp == NULL)
			goto failed;

		tail->m_next = mp;
		tail = mp;
		if (len > MINCLSIZE) {
			MCLGET(mp, how);
			if ((mp->m_flags & M_EXT) == 0)
				goto failed;
		}

		mp->m_len = 0;
		len -= M_TRAILINGSPACE(mp);
	}

	if (mtail != NULL)
		mtail->m_next = top;
	return (m);

failed:
	m_freem(top);
	return (NULL);
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
		m = n;
	} while (m);
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
 * Note that the copy is read-only, because clusters are not copied,
 * only their reference counts are incremented.
 */
#define MCFail (mbstat.m_mcfail)

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

	KASSERT(off >= 0, ("m_copym, negative off %d", off));
	KASSERT(len >= 0, ("m_copym, negative len %d", len));
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
		if (m == 0) {
			KASSERT(len == M_COPYALL, 
			    ("m_copym, length > size of mbuf chain"));
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
		n->m_len = min(len, m->m_len - off);
		if (m->m_flags & M_EXT) {
			n->m_data = m->m_data + off;
			if(!m->m_ext.ext_ref)
				mclrefcnt[mtocl(m->m_ext.ext_buf)]++;
			else
				(*(m->m_ext.ext_ref))(m->m_ext.ext_buf,
							m->m_ext.ext_size);
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
 * Copy an entire packet, including header (which must be present).
 * An optimization of the common case `m_copym(m, 0, M_COPYALL, how)'.
 * Note that the copy is read-only, because clusters are not copied,
 * only their reference counts are incremented.
 * Preserve alignment of the first mbuf so if the creator has left
 * some room at the beginning (e.g. for inserting protocol headers)
 * the copies also have the room available.
 */
struct mbuf *
m_copypacket(m, how)
	struct mbuf *m;
	int how;
{
	struct mbuf *top, *n, *o;

	MGET(n, how, m->m_type);
	top = n;
	if (!n)
		goto nospace;

	M_COPY_PKTHDR(n, m);
	n->m_len = m->m_len;
	if (m->m_flags & M_EXT) {
		n->m_data = m->m_data;
		if(!m->m_ext.ext_ref)
			mclrefcnt[mtocl(m->m_ext.ext_buf)]++;
		else
			(*(m->m_ext.ext_ref))(m->m_ext.ext_buf,
						m->m_ext.ext_size);
		n->m_ext = m->m_ext;
		n->m_flags |= M_EXT;
	} else {
		bcopy(mtod(m, char *), mtod(n, char *), n->m_len);
	}

	m = m->m_next;
	while (m) {
		MGET(o, how, m->m_type);
		if (!o)
			goto nospace;

		n->m_next = o;
		n = n->m_next;

		n->m_len = m->m_len;
		if (m->m_flags & M_EXT) {
			n->m_data = m->m_data;
			if(!m->m_ext.ext_ref)
				mclrefcnt[mtocl(m->m_ext.ext_buf)]++;
			else
				(*(m->m_ext.ext_ref))(m->m_ext.ext_buf,
							m->m_ext.ext_size);
			n->m_ext = m->m_ext;
			n->m_flags |= M_EXT;
		} else {
			n->m_data = n->m_pktdat + (m->m_data - m->m_pktdat );
			bcopy(mtod(m, char *), mtod(n, char *), n->m_len);
		}

		m = m->m_next;
	}
	return top;
nospace:
	m_freem(top);
	MCFail++;
	return 0;
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
m_dup(m, how)
	struct mbuf *m;
	int how;
{
	struct mbuf **p, *top = NULL;
	int remain, moff, nsize;

	/* Sanity check */
	if (m == NULL)
		return (0);
	KASSERT((m->m_flags & M_PKTHDR) != 0, ("%s: !PKTHDR", __FUNCTION__));

	/* While there's more data, get a new mbuf, tack it on, and fill it */
	remain = m->m_pkthdr.len;
	moff = 0;
	p = &top;
	while (remain > 0 || top == NULL) {	/* allow m->m_pkthdr.len == 0 */
		struct mbuf *n;

		/* Get the next new mbuf */
		MGET(n, how, m->m_type);
		if (n == NULL)
			goto nospace;
		if (top == NULL) {		/* first one, must be PKTHDR */
			M_COPY_PKTHDR(n, m);
			nsize = MHLEN;
		} else				/* not the first one */
			nsize = MLEN;
		if (remain >= MINCLSIZE) {
			MCLGET(n, how);
			if ((n->m_flags & M_EXT) == 0) {
				(void)m_free(n);
				goto nospace;
			}
			nsize = MCLBYTES;
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
		    	("%s: bogus m_pkthdr.len", __FUNCTION__));
	}
	return (top);

nospace:
	m_freem(top);
	MCFail++;
	return (0);
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
	register int count;

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
				break;
			}
			count -= m->m_len;
		}
		while (m->m_next)
			(m = m->m_next) ->m_len = 0;
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
#define MPFail (mbstat.m_mpfail)

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
 * Partition an mbuf chain in two pieces, returning the tail --
 * all but the first len0 bytes.  In case of failure, it returns NULL and
 * attempts to restore the chain to its original state.
 */
struct mbuf *
m_split(m0, len0, wait)
	register struct mbuf *m0;
	int len0, wait;
{
	register struct mbuf *m, *n;
	unsigned len = len0, remain;

	for (m = m0; m && len > m->m_len; m = m->m_next)
		len -= m->m_len;
	if (m == 0)
		return (0);
	remain = m->m_len - len;
	if (m0->m_flags & M_PKTHDR) {
		MGETHDR(n, wait, m0->m_type);
		if (n == 0)
			return (0);
		n->m_pkthdr.rcvif = m0->m_pkthdr.rcvif;
		n->m_pkthdr.len = m0->m_pkthdr.len - len0;
		m0->m_pkthdr.len = len0;
		if (m->m_flags & M_EXT)
			goto extpacket;
		if (remain > MHLEN) {
			/* m can't be the lead packet */
			MH_ALIGN(n, 0);
			n->m_next = m_split(m, len, wait);
			if (n->m_next == 0) {
				(void) m_free(n);
				return (0);
			} else
				return (n);
		} else
			MH_ALIGN(n, remain);
	} else if (remain == 0) {
		n = m->m_next;
		m->m_next = 0;
		return (n);
	} else {
		MGET(n, wait, m->m_type);
		if (n == 0)
			return (0);
		M_ALIGN(n, remain);
	}
extpacket:
	if (m->m_flags & M_EXT) {
		n->m_flags |= M_EXT;
		n->m_ext = m->m_ext;
		if(!m->m_ext.ext_ref)
			mclrefcnt[mtocl(m->m_ext.ext_buf)]++;
		else
			(*(m->m_ext.ext_ref))(m->m_ext.ext_buf,
						m->m_ext.ext_size);
		m->m_ext.ext_size = 0; /* For Accounting XXXXXX danger */
		n->m_data = m->m_data + len;
	} else {
		bcopy(mtod(m, caddr_t) + len, mtod(n, caddr_t), remain);
	}
	n->m_len = remain;
	m->m_len = len;
	n->m_next = m->m_next;
	m->m_next = 0;
	return (n);
}
/*
 * Routine to copy from device local memory into mbufs.
 */
struct mbuf *
m_devget(buf, totlen, off0, ifp, copy)
	char *buf;
	int totlen, off0;
	struct ifnet *ifp;
	void (*copy) __P((char *from, caddr_t to, u_int len));
{
	register struct mbuf *m;
	struct mbuf *top = 0, **mp = &top;
	register int off = off0, len;
	register char *cp;
	char *epkt;

	cp = buf;
	epkt = cp + totlen;
	if (off) {
		cp += off + 2 * sizeof(u_short);
		totlen -= 2 * sizeof(u_short);
	}
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	m->m_len = MHLEN;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			m->m_len = MLEN;
		}
		len = min(totlen, epkt - cp);
		if (len >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				m->m_len = len = min(len, MCLBYTES);
			else
				len = m->m_len;
		} else {
			/*
			 * Place initial small packet/header at end of mbuf.
			 */
			if (len < m->m_len) {
				if (top == 0 && len + max_linkhdr <= m->m_len)
					m->m_data += max_linkhdr;
				m->m_len = len;
			} else
				len = m->m_len;
		}
		if (copy)
			copy(cp, mtod(m, caddr_t), (unsigned)len);
		else
			bcopy(cp, mtod(m, caddr_t), (unsigned)len);
		cp += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
		if (cp == epkt)
			cp = buf;
	}
	return (top);
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

void
m_print(const struct mbuf *m)
{
	int len;
	const struct mbuf *m2;

	len = m->m_pkthdr.len;
	m2 = m;
	while (len) {
		printf("%p %*D\n", m2, m2->m_len, (u_char *)m2->m_data, "-");
		len -= m2->m_len;
		m2 = m2->m_next;
	}
	return;
}
