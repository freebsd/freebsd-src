/*-
 * Copyright (c) 2004, 2005,
 * 	Bosko Milekic <bmilekic@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"
#include "opt_param.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/protosw.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <vm/uma_dbg.h>

/*
 * In FreeBSD, Mbufs and Mbuf Clusters are allocated from UMA
 * Zones.
 *
 * Mbuf Clusters (2K, contiguous) are allocated from the Cluster
 * Zone.  The Zone can be capped at kern.ipc.nmbclusters, if the
 * administrator so desires.
 *
 * Mbufs are allocated from a UMA Master Zone called the Mbuf
 * Zone.
 *
 * Additionally, FreeBSD provides a Packet Zone, which it
 * configures as a Secondary Zone to the Mbuf Master Zone,
 * thus sharing backend Slab kegs with the Mbuf Master Zone.
 *
 * Thus common-case allocations and locking are simplified:
 *
 *  m_clget()                m_getcl()
 *    |                         |
 *    |   .------------>[(Packet Cache)]    m_get(), m_gethdr()
 *    |   |             [     Packet   ]            |
 *  [(Cluster Cache)]   [    Secondary ]   [ (Mbuf Cache)     ]
 *  [ Cluster Zone  ]   [     Zone     ]   [ Mbuf Master Zone ]
 *        |                       \________         |
 *  [ Cluster Keg   ]                      \       /
 *        |    	                         [ Mbuf Keg   ]
 *  [ Cluster Slabs ]                         |
 *        |                              [ Mbuf Slabs ]
 *         \____________(VM)_________________/
 *
 *
 * Whenever an object is allocated with uma_zalloc() out of
 * one of the Zones its _ctor_ function is executed.  The same
 * for any deallocation through uma_zfree() the _dtor_ function
 * is executed.
 *
 * Caches are per-CPU and are filled from the Master Zone.
 *
 * Whenever an object is allocated from the underlying global
 * memory pool it gets pre-initialized with the _zinit_ functions.
 * When the Keg's are overfull objects get decomissioned with
 * _zfini_ functions and free'd back to the global memory pool.
 *
 */

int nmbclusters;		/* limits number of mbuf clusters */
int nmbjumbop;			/* limits number of page size jumbo clusters */
int nmbjumbo9;			/* limits number of 9k jumbo clusters */
int nmbjumbo16;			/* limits number of 16k jumbo clusters */
struct mbstat mbstat;

/*
 * tunable_mbinit() has to be run before init_maxsockets() thus
 * the SYSINIT order below is SI_ORDER_MIDDLE while init_maxsockets()
 * runs at SI_ORDER_ANY.
 */
static void
tunable_mbinit(void *dummy)
{
	TUNABLE_INT_FETCH("kern.ipc.nmbclusters", &nmbclusters);

	/* This has to be done before VM init. */
	if (nmbclusters == 0)
		nmbclusters = 1024 + maxusers * 64;
	nmbjumbop = nmbclusters / 2;
	nmbjumbo9 = nmbjumbop / 2;
	nmbjumbo16 = nmbjumbo9 / 2;
}
SYSINIT(tunable_mbinit, SI_SUB_TUNABLES, SI_ORDER_MIDDLE, tunable_mbinit, NULL);

static int
sysctl_nmbclusters(SYSCTL_HANDLER_ARGS)
{
	int error, newnmbclusters;

	newnmbclusters = nmbclusters;
	error = sysctl_handle_int(oidp, &newnmbclusters, 0, req); 
	if (error == 0 && req->newptr) {
		if (newnmbclusters > nmbclusters) {
			nmbclusters = newnmbclusters;
			uma_zone_set_max(zone_clust, nmbclusters);
			EVENTHANDLER_INVOKE(nmbclusters_change);
		} else
			error = EINVAL;
	}
	return (error);
}
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbclusters, CTLTYPE_INT|CTLFLAG_RW,
&nmbclusters, 0, sysctl_nmbclusters, "IU",
    "Maximum number of mbuf clusters allowed");

static int
sysctl_nmbjumbop(SYSCTL_HANDLER_ARGS)
{
	int error, newnmbjumbop;

	newnmbjumbop = nmbjumbop;
	error = sysctl_handle_int(oidp, &newnmbjumbop, 0, req); 
	if (error == 0 && req->newptr) {
		if (newnmbjumbop> nmbjumbop) {
			nmbjumbop = newnmbjumbop;
			uma_zone_set_max(zone_jumbop, nmbjumbop);
		} else
			error = EINVAL;
	}
	return (error);
}
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbjumbop, CTLTYPE_INT|CTLFLAG_RW,
&nmbjumbop, 0, sysctl_nmbjumbop, "IU",
	 "Maximum number of mbuf page size jumbo clusters allowed");


static int
sysctl_nmbjumbo9(SYSCTL_HANDLER_ARGS)
{
	int error, newnmbjumbo9;

	newnmbjumbo9 = nmbjumbo9;
	error = sysctl_handle_int(oidp, &newnmbjumbo9, 0, req); 
	if (error == 0 && req->newptr) {
		if (newnmbjumbo9> nmbjumbo9) {
			nmbjumbo9 = newnmbjumbo9;
			uma_zone_set_max(zone_jumbo9, nmbjumbo9);
		} else
			error = EINVAL;
	}
	return (error);
}
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbjumbo9, CTLTYPE_INT|CTLFLAG_RW,
&nmbjumbo9, 0, sysctl_nmbjumbo9, "IU",
	"Maximum number of mbuf 9k jumbo clusters allowed"); 

static int
sysctl_nmbjumbo16(SYSCTL_HANDLER_ARGS)
{
	int error, newnmbjumbo16;

	newnmbjumbo16 = nmbjumbo16;
	error = sysctl_handle_int(oidp, &newnmbjumbo16, 0, req); 
	if (error == 0 && req->newptr) {
		if (newnmbjumbo16> nmbjumbo16) {
			nmbjumbo16 = newnmbjumbo16;
			uma_zone_set_max(zone_jumbo16, nmbjumbo16);
		} else
			error = EINVAL;
	}
	return (error);
}
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbjumbo16, CTLTYPE_INT|CTLFLAG_RW,
&nmbjumbo16, 0, sysctl_nmbjumbo16, "IU",
    "Maximum number of mbuf 16k jumbo clusters allowed");



SYSCTL_STRUCT(_kern_ipc, OID_AUTO, mbstat, CTLFLAG_RD, &mbstat, mbstat,
    "Mbuf general information and statistics");

/*
 * Zones from which we allocate.
 */
uma_zone_t	zone_clust;
uma_zone_t	zone_jumbop;
uma_zone_t	zone_jumbo9;
uma_zone_t	zone_jumbo16;
uma_zone_t	zone_mbuf;
uma_zone_t	zone_pack;

/*
 * Local prototypes.
 */
#ifdef INVARIANTS 
static int	mb_ctor_pack(void *mem, int size, void *arg, int how);
#endif
static void	mb_dtor_pack(void *mem, int size, void *arg);
static void	mb_reclaim(void *);
static int	mb_zinit_pack(void *mem, int size, int how);
static void	mb_zfini_pack(void *mem, int size);

static void	mbuf_init(void *);
static void    *mbuf_jumbo_alloc(uma_zone_t, int, u_int8_t *, int);
static void	mbuf_jumbo_free(void *, int, u_int8_t);

static MALLOC_DEFINE(M_JUMBOFRAME, "jumboframes", "mbuf jumbo frame buffers");

/* Ensure that MSIZE doesn't break dtom() - it must be a power of 2 */
CTASSERT((((MSIZE - 1) ^ MSIZE) + 1) >> 1 == MSIZE);

/*
 * Initialize FreeBSD Network buffer allocation.
 */
SYSINIT(mbuf, SI_SUB_MBUF, SI_ORDER_FIRST, mbuf_init, NULL);
static void
mbuf_init(void *dummy)
{

	/*
	 * Configure UMA zones for Mbufs, Clusters, and Packets.
	 */
	zone_mbuf = uma_zcreate(MBUF_MEM_NAME, MSIZE,
#ifdef INVARIANTS
	    trash_ctor, trash_dtor, trash_init, trash_fini,
#else
	    NULL, NULL, NULL, NULL,
#endif
	    MSIZE - 1, UMA_ZONE_MAXBUCKET);

	zone_clust = uma_zcreate(MBUF_CLUSTER_MEM_NAME, MCLBYTES,
#ifdef INVARIANTS
	    trash_ctor, trash_dtor, trash_init, trash_fini,
#else
	    NULL, NULL, NULL, NULL,
#endif
	    UMA_ALIGN_PTR, UMA_ZONE_MAXBUCKET);
	if (nmbclusters > 0)
		uma_zone_set_max(zone_clust, nmbclusters);

	zone_pack = uma_zsecond_create(MBUF_PACKET_MEM_NAME,
#ifdef INVARIANTS
	    mb_ctor_pack,
#else
	    NULL,
#endif
	    mb_dtor_pack, mb_zinit_pack, mb_zfini_pack, zone_mbuf);

	/* Make jumbo frame zone too. Page size, 9k and 16k. */
	zone_jumbop = uma_zcreate(MBUF_JUMBOP_MEM_NAME, MJUMPAGESIZE,
#ifdef INVARIANTS
	    trash_ctor, trash_dtor, trash_init, trash_fini,
#else
	    NULL, NULL, NULL, NULL,
#endif
	    UMA_ALIGN_PTR, 0);
	if (nmbjumbop > 0)
		uma_zone_set_max(zone_jumbop, nmbjumbop);

	zone_jumbo9 = uma_zcreate(MBUF_JUMBO9_MEM_NAME, MJUM9BYTES,
#ifdef INVARIANTS
	    trash_ctor, trash_dtor, trash_init, trash_fini,
#else
	    NULL, NULL, NULL, NULL,
#endif
	    UMA_ALIGN_PTR, 0);
	if (nmbjumbo9 > 0)
		uma_zone_set_max(zone_jumbo9, nmbjumbo9);
	uma_zone_set_allocf(zone_jumbo9, mbuf_jumbo_alloc);
	uma_zone_set_freef(zone_jumbo9, mbuf_jumbo_free);

	zone_jumbo16 = uma_zcreate(MBUF_JUMBO16_MEM_NAME, MJUM16BYTES,
#ifdef INVARIANTS
	    trash_ctor, trash_dtor, trash_init, trash_fini,
#else
	    NULL, NULL, NULL, NULL,
#endif
	    UMA_ALIGN_PTR, 0);
	if (nmbjumbo16 > 0)
		uma_zone_set_max(zone_jumbo16, nmbjumbo16);
	uma_zone_set_allocf(zone_jumbo16, mbuf_jumbo_alloc);
	uma_zone_set_freef(zone_jumbo16, mbuf_jumbo_free);

	/*
	 * Hook event handler for low-memory situation, used to
	 * drain protocols and push data back to the caches (UMA
	 * later pushes it back to VM).
	 */
	EVENTHANDLER_REGISTER(vm_lowmem, mb_reclaim, NULL,
	    EVENTHANDLER_PRI_FIRST);

	/*
	 * [Re]set counters and local statistics knobs.
	 * XXX Some of these should go and be replaced, but UMA stat
	 * gathering needs to be revised.
	 */
	mbstat.m_mbufs = 0;
	mbstat.m_mclusts = 0;
	mbstat.m_drain = 0;
	mbstat.m_msize = MSIZE;
	mbstat.m_mclbytes = MCLBYTES;
	mbstat.m_minclsize = MINCLSIZE;
	mbstat.m_mlen = MLEN;
	mbstat.m_mhlen = MHLEN;
	mbstat.m_numtypes = MT_NTYPES;

	mbstat.m_mcfail = mbstat.m_mpfail = 0;
	mbstat.sf_iocnt = 0;
	mbstat.sf_allocwait = mbstat.sf_allocfail = 0;
}

/*
 * UMA backend page allocator for the jumbo frame zones.
 *
 * Allocates kernel virtual memory that is backed by contiguous physical
 * pages.
 */
static void *
mbuf_jumbo_alloc(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{

	/* Inform UMA that this allocator uses kernel_map/object. */
	*flags = UMA_SLAB_KERNEL;
	return (contigmalloc(bytes, M_JUMBOFRAME, wait, (vm_paddr_t)0,
	    ~(vm_paddr_t)0, 1, 0));
}

/*
 * UMA backend page deallocator for the jumbo frame zones.
 */
static void
mbuf_jumbo_free(void *mem, int size, u_int8_t flags)
{

	contigfree(mem, size, M_JUMBOFRAME);
}

#ifdef INVARIANTS
static int
mb_ctor_pack(void *mem, int size, void *arg, int how)
{
	struct mbuf *m;

	m = (struct mbuf *)mem;
	trash_ctor(m->m_ext.ext_buf, MCLBYTES, arg, how);

	return (0);
}
#endif

/*
 * The Mbuf Packet zone destructor.
 */
static void
mb_dtor_pack(void *mem, int size, void *arg)
{
	struct mbuf *m;

	m = (struct mbuf *)mem;
	/* Make sure we've got a clean cluster back. */
	KASSERT((m->m_flags & M_EXT) == M_EXT, ("%s: M_EXT not set", __func__));
	KASSERT(m->m_ext.ext_buf != NULL, ("%s: ext_buf == NULL", __func__));
	KASSERT(m->m_ext.ext_free == m_ext_free_nop,
	    ("%s: ext_free != m_ext_free_nop", __func__));
	KASSERT(m->m_ext.ext_arg1 == NULL, ("%s: ext_arg1 != NULL", __func__));
	KASSERT(m->m_ext.ext_arg2 == NULL, ("%s: ext_arg2 != NULL", __func__));
	KASSERT(m->m_ext.ext_size == MCLBYTES, ("%s: ext_size != MCLBYTES",
	    __func__));
	KASSERT(m->m_ext.ext_type == EXT_PACKET, ("%s: ext_type != EXT_PACKET",
	    __func__));
#ifdef INVARIANTS 
	trash_dtor(m->m_ext.ext_buf, MCLBYTES, arg);
#endif

	/*
	 * If there are processes blocked on zone_clust, waiting for pages
	 * to be freed up, * cause them to be woken up by draining the
	 * packet zone.  We are exposed to a race here * (in the check for
	 * the UMA_ZFLAG_FULL) where we might miss the flag set, but that
	 * is deliberate. We don't want to acquire the zone lock for every
	 * mbuf free.
	 */
	if (uma_zone_exhausted_nolock(zone_clust))
		zone_drain(zone_pack);
}

/*
 * The Packet secondary zone's init routine, executed on the
 * object's transition from mbuf keg slab to zone cache.
 */
static int
mb_zinit_pack(void *mem, int size, int how)
{
	struct mbuf *m;

	m = (struct mbuf *)mem;		/* m is virgin. */
	/*
	 * Allocate and attach the cluster to the ext.
	 */
	if ((mem = uma_zalloc(zone_clust, how)) == NULL)
		return (ENOMEM);
	m_extadd(m, mem, MCLBYTES, m_ext_free_nop, NULL, NULL, 0, EXT_PACKET);
#ifdef INVARIANTS
	return trash_init(m->m_ext.ext_buf, MCLBYTES, how);
#endif

	return (0);
}

/*
 * The Packet secondary zone's fini routine, executed on the
 * object's transition from zone cache to keg slab.
 */
static void
mb_zfini_pack(void *mem, int size)
{
	struct mbuf *m;

	m = (struct mbuf *)mem;
#ifdef INVARIANTS
	trash_fini(m->m_ext.ext_buf, MCLBYTES);
#endif
	uma_zfree_arg(zone_clust, m->m_ext.ext_buf, NULL);
#ifdef INVARIANTS
	trash_dtor(mem, size, NULL);
#endif
}

int
m_pkthdr_init(struct mbuf *m, int how)
{
#ifdef MAC
	int error;
#endif

	m->m_data = m->m_pktdat;
	SLIST_INIT(&m->m_pkthdr.tags);
	m->m_pkthdr.rcvif = NULL;
	m->m_pkthdr.header = NULL;
	m->m_pkthdr.len = 0;
	m->m_pkthdr.flowid = 0;
	m->m_pkthdr.csum_flags = 0;
	m->m_pkthdr.csum_data = 0;
	m->m_pkthdr.tso_segsz = 0;
	m->m_pkthdr.ether_vtag = 0;
#ifdef MAC
	/* If the label init fails, fail the alloc */
	error = mac_mbuf_init(m, how);
	if (error)
		return (error);
#endif

	return (0);
}


/*
 * This is the protocol drain routine.
 *
 * No locks should be held when this is called.  The drain routines have to
 * presently acquire some locks which raises the possibility of lock order
 * reversal.
 */
static void
mb_reclaim(void *junk)
{
	struct domain *dp;
	struct protosw *pr;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK | WARN_PANIC, NULL,
	    "mb_reclaim()");

	for (dp = domains; dp != NULL; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_drain != NULL)
				(*pr->pr_drain)();
}

struct mbuf *
_m_getjcl(int how, short type, int flags, int size, uma_zone_t zone,
    int exttype)
{
	struct mbuf *m;
	void *mem;

	if (size == MCLBYTES)
		return m_getcl(how, type, flags);
	/*
	 * Allocate the memory and header seperate for these sizes.
	 */
	mem = uma_zalloc(zone, how);
	if (mem == NULL)
		return (NULL);
	m = m_alloc(zone_mbuf, 0, how, type, flags);
	if (m == NULL) {
		uma_zfree(zone, mem);
		return (NULL);
	}
	m_extadd(m, mem, size, m_ext_free_zone, zone, mem, flags, exttype);

	return (m);
}

void *
_m_cljget(struct mbuf *m, int how, int size, uma_zone_t zone, int exttype)
{
	void *mem;

	if (m && m->m_flags & M_EXT)
		printf("%s: %p mbuf already has cluster\n", __func__, m);
	if (m != NULL)
		m->m_ext.ext_buf = NULL;
	mem = uma_zalloc(zone, how);
	if (mem == NULL)
		return (NULL);
	if (m)
		m_extadd(m, mem, size, m_ext_free_zone, zone, mem, 0, exttype);
	return (mem);
}
