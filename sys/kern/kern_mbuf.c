/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2004, 2005,
 *	Bosko Milekic <bmilekic@FreeBSD.org>.  All rights reserved.
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

#include "opt_param.h"
#include "opt_kern_tls.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/domainset.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/ktls.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/refcount.h>
#include <sys/sf_buf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/uma.h>
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
 *        |	                         [ Mbuf Keg   ]
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
 * When the Keg's are overfull objects get decommissioned with
 * _zfini_ functions and free'd back to the global memory pool.
 *
 */

int nmbufs;			/* limits number of mbufs */
int nmbclusters;		/* limits number of mbuf clusters */
int nmbjumbop;			/* limits number of page size jumbo clusters */
int nmbjumbo9;			/* limits number of 9k jumbo clusters */
int nmbjumbo16;			/* limits number of 16k jumbo clusters */

bool mb_use_ext_pgs;		/* use EXT_PGS mbufs for sendfile & TLS */
SYSCTL_BOOL(_kern_ipc, OID_AUTO, mb_use_ext_pgs, CTLFLAG_RWTUN,
    &mb_use_ext_pgs, 0,
    "Use unmapped mbufs for sendfile(2) and TLS offload");

static quad_t maxmbufmem;	/* overall real memory limit for all mbufs */

SYSCTL_QUAD(_kern_ipc, OID_AUTO, maxmbufmem, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &maxmbufmem, 0,
    "Maximum real memory allocatable to various mbuf types");

static counter_u64_t snd_tag_count;
SYSCTL_COUNTER_U64(_kern_ipc, OID_AUTO, num_snd_tags, CTLFLAG_RW,
    &snd_tag_count, "# of active mbuf send tags");

/*
 * tunable_mbinit() has to be run before any mbuf allocations are done.
 */
static void
tunable_mbinit(void *dummy)
{
	quad_t realmem;

	/*
	 * The default limit for all mbuf related memory is 1/2 of all
	 * available kernel memory (physical or kmem).
	 * At most it can be 3/4 of available kernel memory.
	 */
	realmem = qmin((quad_t)physmem * PAGE_SIZE, vm_kmem_size);
	maxmbufmem = realmem / 2;
	TUNABLE_QUAD_FETCH("kern.ipc.maxmbufmem", &maxmbufmem);
	if (maxmbufmem > realmem / 4 * 3)
		maxmbufmem = realmem / 4 * 3;

	TUNABLE_INT_FETCH("kern.ipc.nmbclusters", &nmbclusters);
	if (nmbclusters == 0)
		nmbclusters = maxmbufmem / MCLBYTES / 4;

	TUNABLE_INT_FETCH("kern.ipc.nmbjumbop", &nmbjumbop);
	if (nmbjumbop == 0)
		nmbjumbop = maxmbufmem / MJUMPAGESIZE / 4;

	TUNABLE_INT_FETCH("kern.ipc.nmbjumbo9", &nmbjumbo9);
	if (nmbjumbo9 == 0)
		nmbjumbo9 = maxmbufmem / MJUM9BYTES / 6;

	TUNABLE_INT_FETCH("kern.ipc.nmbjumbo16", &nmbjumbo16);
	if (nmbjumbo16 == 0)
		nmbjumbo16 = maxmbufmem / MJUM16BYTES / 6;

	/*
	 * We need at least as many mbufs as we have clusters of
	 * the various types added together.
	 */
	TUNABLE_INT_FETCH("kern.ipc.nmbufs", &nmbufs);
	if (nmbufs < nmbclusters + nmbjumbop + nmbjumbo9 + nmbjumbo16)
		nmbufs = lmax(maxmbufmem / MSIZE / 5,
		    nmbclusters + nmbjumbop + nmbjumbo9 + nmbjumbo16);
}
SYSINIT(tunable_mbinit, SI_SUB_KMEM, SI_ORDER_MIDDLE, tunable_mbinit, NULL);

static int
sysctl_nmbclusters(SYSCTL_HANDLER_ARGS)
{
	int error, newnmbclusters;

	newnmbclusters = nmbclusters;
	error = sysctl_handle_int(oidp, &newnmbclusters, 0, req);
	if (error == 0 && req->newptr && newnmbclusters != nmbclusters) {
		if (newnmbclusters > nmbclusters &&
		    nmbufs >= nmbclusters + nmbjumbop + nmbjumbo9 + nmbjumbo16) {
			nmbclusters = newnmbclusters;
			nmbclusters = uma_zone_set_max(zone_clust, nmbclusters);
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
	if (error == 0 && req->newptr && newnmbjumbop != nmbjumbop) {
		if (newnmbjumbop > nmbjumbop &&
		    nmbufs >= nmbclusters + nmbjumbop + nmbjumbo9 + nmbjumbo16) {
			nmbjumbop = newnmbjumbop;
			nmbjumbop = uma_zone_set_max(zone_jumbop, nmbjumbop);
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
	if (error == 0 && req->newptr && newnmbjumbo9 != nmbjumbo9) {
		if (newnmbjumbo9 > nmbjumbo9 &&
		    nmbufs >= nmbclusters + nmbjumbop + nmbjumbo9 + nmbjumbo16) {
			nmbjumbo9 = newnmbjumbo9;
			nmbjumbo9 = uma_zone_set_max(zone_jumbo9, nmbjumbo9);
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
	if (error == 0 && req->newptr && newnmbjumbo16 != nmbjumbo16) {
		if (newnmbjumbo16 > nmbjumbo16 &&
		    nmbufs >= nmbclusters + nmbjumbop + nmbjumbo9 + nmbjumbo16) {
			nmbjumbo16 = newnmbjumbo16;
			nmbjumbo16 = uma_zone_set_max(zone_jumbo16, nmbjumbo16);
		} else
			error = EINVAL;
	}
	return (error);
}
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbjumbo16, CTLTYPE_INT|CTLFLAG_RW,
&nmbjumbo16, 0, sysctl_nmbjumbo16, "IU",
    "Maximum number of mbuf 16k jumbo clusters allowed");

static int
sysctl_nmbufs(SYSCTL_HANDLER_ARGS)
{
	int error, newnmbufs;

	newnmbufs = nmbufs;
	error = sysctl_handle_int(oidp, &newnmbufs, 0, req);
	if (error == 0 && req->newptr && newnmbufs != nmbufs) {
		if (newnmbufs > nmbufs) {
			nmbufs = newnmbufs;
			nmbufs = uma_zone_set_max(zone_mbuf, nmbufs);
			EVENTHANDLER_INVOKE(nmbufs_change);
		} else
			error = EINVAL;
	}
	return (error);
}
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbufs, CTLTYPE_INT|CTLFLAG_RW,
&nmbufs, 0, sysctl_nmbufs, "IU",
    "Maximum number of mbufs allowed");

/*
 * Zones from which we allocate.
 */
uma_zone_t	zone_mbuf;
uma_zone_t	zone_clust;
uma_zone_t	zone_pack;
uma_zone_t	zone_jumbop;
uma_zone_t	zone_jumbo9;
uma_zone_t	zone_jumbo16;
uma_zone_t	zone_extpgs;

/*
 * Local prototypes.
 */
static int	mb_ctor_mbuf(void *, int, void *, int);
static int	mb_ctor_clust(void *, int, void *, int);
static int	mb_ctor_pack(void *, int, void *, int);
static void	mb_dtor_mbuf(void *, int, void *);
static void	mb_dtor_pack(void *, int, void *);
static int	mb_zinit_pack(void *, int, int);
static void	mb_zfini_pack(void *, int);
static void	mb_reclaim(uma_zone_t, int);
static void    *mbuf_jumbo_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);

/* Ensure that MSIZE is a power of 2. */
CTASSERT((((MSIZE - 1) ^ MSIZE) + 1) >> 1 == MSIZE);

_Static_assert(sizeof(struct mbuf_ext_pgs) == 256,
    "mbuf_ext_pgs size mismatch");

/*
 * Initialize FreeBSD Network buffer allocation.
 */
static void
mbuf_init(void *dummy)
{

	/*
	 * Configure UMA zones for Mbufs, Clusters, and Packets.
	 */
	zone_mbuf = uma_zcreate(MBUF_MEM_NAME, MSIZE,
	    mb_ctor_mbuf, mb_dtor_mbuf, NULL, NULL,
	    MSIZE - 1, UMA_ZONE_MAXBUCKET);
	if (nmbufs > 0)
		nmbufs = uma_zone_set_max(zone_mbuf, nmbufs);
	uma_zone_set_warning(zone_mbuf, "kern.ipc.nmbufs limit reached");
	uma_zone_set_maxaction(zone_mbuf, mb_reclaim);

	zone_clust = uma_zcreate(MBUF_CLUSTER_MEM_NAME, MCLBYTES,
	    mb_ctor_clust, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	if (nmbclusters > 0)
		nmbclusters = uma_zone_set_max(zone_clust, nmbclusters);
	uma_zone_set_warning(zone_clust, "kern.ipc.nmbclusters limit reached");
	uma_zone_set_maxaction(zone_clust, mb_reclaim);

	zone_pack = uma_zsecond_create(MBUF_PACKET_MEM_NAME, mb_ctor_pack,
	    mb_dtor_pack, mb_zinit_pack, mb_zfini_pack, zone_mbuf);

	/* Make jumbo frame zone too. Page size, 9k and 16k. */
	zone_jumbop = uma_zcreate(MBUF_JUMBOP_MEM_NAME, MJUMPAGESIZE,
	    mb_ctor_clust, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	if (nmbjumbop > 0)
		nmbjumbop = uma_zone_set_max(zone_jumbop, nmbjumbop);
	uma_zone_set_warning(zone_jumbop, "kern.ipc.nmbjumbop limit reached");
	uma_zone_set_maxaction(zone_jumbop, mb_reclaim);

	zone_jumbo9 = uma_zcreate(MBUF_JUMBO9_MEM_NAME, MJUM9BYTES,
	    mb_ctor_clust, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	uma_zone_set_allocf(zone_jumbo9, mbuf_jumbo_alloc);
	if (nmbjumbo9 > 0)
		nmbjumbo9 = uma_zone_set_max(zone_jumbo9, nmbjumbo9);
	uma_zone_set_warning(zone_jumbo9, "kern.ipc.nmbjumbo9 limit reached");
	uma_zone_set_maxaction(zone_jumbo9, mb_reclaim);

	zone_jumbo16 = uma_zcreate(MBUF_JUMBO16_MEM_NAME, MJUM16BYTES,
	    mb_ctor_clust, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	uma_zone_set_allocf(zone_jumbo16, mbuf_jumbo_alloc);
	if (nmbjumbo16 > 0)
		nmbjumbo16 = uma_zone_set_max(zone_jumbo16, nmbjumbo16);
	uma_zone_set_warning(zone_jumbo16, "kern.ipc.nmbjumbo16 limit reached");
	uma_zone_set_maxaction(zone_jumbo16, mb_reclaim);

	zone_extpgs = uma_zcreate(MBUF_EXTPGS_MEM_NAME,
	    sizeof(struct mbuf_ext_pgs),
	    NULL, NULL, NULL, NULL,
	    UMA_ALIGN_CACHE, 0);

	/*
	 * Hook event handler for low-memory situation, used to
	 * drain protocols and push data back to the caches (UMA
	 * later pushes it back to VM).
	 */
	EVENTHANDLER_REGISTER(vm_lowmem, mb_reclaim, NULL,
	    EVENTHANDLER_PRI_FIRST);

	snd_tag_count = counter_u64_alloc(M_WAITOK);
}
SYSINIT(mbuf, SI_SUB_MBUF, SI_ORDER_FIRST, mbuf_init, NULL);

#ifdef DEBUGNET
/*
 * debugnet makes use of a pre-allocated pool of mbufs and clusters.  When
 * debugnet is configured, we initialize a set of UMA cache zones which return
 * items from this pool.  At panic-time, the regular UMA zone pointers are
 * overwritten with those of the cache zones so that drivers may allocate and
 * free mbufs and clusters without attempting to allocate physical memory.
 *
 * We keep mbufs and clusters in a pair of mbuf queues.  In particular, for
 * the purpose of caching clusters, we treat them as mbufs.
 */
static struct mbufq dn_mbufq =
    { STAILQ_HEAD_INITIALIZER(dn_mbufq.mq_head), 0, INT_MAX };
static struct mbufq dn_clustq =
    { STAILQ_HEAD_INITIALIZER(dn_clustq.mq_head), 0, INT_MAX };

static int dn_clsize;
static uma_zone_t dn_zone_mbuf;
static uma_zone_t dn_zone_clust;
static uma_zone_t dn_zone_pack;

static struct debugnet_saved_zones {
	uma_zone_t dsz_mbuf;
	uma_zone_t dsz_clust;
	uma_zone_t dsz_pack;
	uma_zone_t dsz_jumbop;
	uma_zone_t dsz_jumbo9;
	uma_zone_t dsz_jumbo16;
	bool dsz_debugnet_zones_enabled;
} dn_saved_zones;

static int
dn_buf_import(void *arg, void **store, int count, int domain __unused,
    int flags)
{
	struct mbufq *q;
	struct mbuf *m;
	int i;

	q = arg;

	for (i = 0; i < count; i++) {
		m = mbufq_dequeue(q);
		if (m == NULL)
			break;
		trash_init(m, q == &dn_mbufq ? MSIZE : dn_clsize, flags);
		store[i] = m;
	}
	KASSERT((flags & M_WAITOK) == 0 || i == count,
	    ("%s: ran out of pre-allocated mbufs", __func__));
	return (i);
}

static void
dn_buf_release(void *arg, void **store, int count)
{
	struct mbufq *q;
	struct mbuf *m;
	int i;

	q = arg;

	for (i = 0; i < count; i++) {
		m = store[i];
		(void)mbufq_enqueue(q, m);
	}
}

static int
dn_pack_import(void *arg __unused, void **store, int count, int domain __unused,
    int flags __unused)
{
	struct mbuf *m;
	void *clust;
	int i;

	for (i = 0; i < count; i++) {
		m = m_get(MT_DATA, M_NOWAIT);
		if (m == NULL)
			break;
		clust = uma_zalloc(dn_zone_clust, M_NOWAIT);
		if (clust == NULL) {
			m_free(m);
			break;
		}
		mb_ctor_clust(clust, dn_clsize, m, 0);
		store[i] = m;
	}
	KASSERT((flags & M_WAITOK) == 0 || i == count,
	    ("%s: ran out of pre-allocated mbufs", __func__));
	return (i);
}

static void
dn_pack_release(void *arg __unused, void **store, int count)
{
	struct mbuf *m;
	void *clust;
	int i;

	for (i = 0; i < count; i++) {
		m = store[i];
		clust = m->m_ext.ext_buf;
		uma_zfree(dn_zone_clust, clust);
		uma_zfree(dn_zone_mbuf, m);
	}
}

/*
 * Free the pre-allocated mbufs and clusters reserved for debugnet, and destroy
 * the corresponding UMA cache zones.
 */
void
debugnet_mbuf_drain(void)
{
	struct mbuf *m;
	void *item;

	if (dn_zone_mbuf != NULL) {
		uma_zdestroy(dn_zone_mbuf);
		dn_zone_mbuf = NULL;
	}
	if (dn_zone_clust != NULL) {
		uma_zdestroy(dn_zone_clust);
		dn_zone_clust = NULL;
	}
	if (dn_zone_pack != NULL) {
		uma_zdestroy(dn_zone_pack);
		dn_zone_pack = NULL;
	}

	while ((m = mbufq_dequeue(&dn_mbufq)) != NULL)
		m_free(m);
	while ((item = mbufq_dequeue(&dn_clustq)) != NULL)
		uma_zfree(m_getzone(dn_clsize), item);
}

/*
 * Callback invoked immediately prior to starting a debugnet connection.
 */
void
debugnet_mbuf_start(void)
{

	MPASS(!dn_saved_zones.dsz_debugnet_zones_enabled);

	/* Save the old zone pointers to restore when debugnet is closed. */
	dn_saved_zones = (struct debugnet_saved_zones) {
		.dsz_debugnet_zones_enabled = true,
		.dsz_mbuf = zone_mbuf,
		.dsz_clust = zone_clust,
		.dsz_pack = zone_pack,
		.dsz_jumbop = zone_jumbop,
		.dsz_jumbo9 = zone_jumbo9,
		.dsz_jumbo16 = zone_jumbo16,
	};

	/*
	 * All cluster zones return buffers of the size requested by the
	 * drivers.  It's up to the driver to reinitialize the zones if the
	 * MTU of a debugnet-enabled interface changes.
	 */
	printf("debugnet: overwriting mbuf zone pointers\n");
	zone_mbuf = dn_zone_mbuf;
	zone_clust = dn_zone_clust;
	zone_pack = dn_zone_pack;
	zone_jumbop = dn_zone_clust;
	zone_jumbo9 = dn_zone_clust;
	zone_jumbo16 = dn_zone_clust;
}

/*
 * Callback invoked when a debugnet connection is closed/finished.
 */
void
debugnet_mbuf_finish(void)
{

	MPASS(dn_saved_zones.dsz_debugnet_zones_enabled);

	printf("debugnet: restoring mbuf zone pointers\n");
	zone_mbuf = dn_saved_zones.dsz_mbuf;
	zone_clust = dn_saved_zones.dsz_clust;
	zone_pack = dn_saved_zones.dsz_pack;
	zone_jumbop = dn_saved_zones.dsz_jumbop;
	zone_jumbo9 = dn_saved_zones.dsz_jumbo9;
	zone_jumbo16 = dn_saved_zones.dsz_jumbo16;

	memset(&dn_saved_zones, 0, sizeof(dn_saved_zones));
}

/*
 * Reinitialize the debugnet mbuf+cluster pool and cache zones.
 */
void
debugnet_mbuf_reinit(int nmbuf, int nclust, int clsize)
{
	struct mbuf *m;
	void *item;

	debugnet_mbuf_drain();

	dn_clsize = clsize;

	dn_zone_mbuf = uma_zcache_create("debugnet_" MBUF_MEM_NAME,
	    MSIZE, mb_ctor_mbuf, mb_dtor_mbuf, NULL, NULL,
	    dn_buf_import, dn_buf_release,
	    &dn_mbufq, UMA_ZONE_NOBUCKET);

	dn_zone_clust = uma_zcache_create("debugnet_" MBUF_CLUSTER_MEM_NAME,
	    clsize, mb_ctor_clust, NULL, NULL, NULL,
	    dn_buf_import, dn_buf_release,
	    &dn_clustq, UMA_ZONE_NOBUCKET);

	dn_zone_pack = uma_zcache_create("debugnet_" MBUF_PACKET_MEM_NAME,
	    MCLBYTES, mb_ctor_pack, mb_dtor_pack, NULL, NULL,
	    dn_pack_import, dn_pack_release,
	    NULL, UMA_ZONE_NOBUCKET);

	while (nmbuf-- > 0) {
		m = m_get(MT_DATA, M_WAITOK);
		uma_zfree(dn_zone_mbuf, m);
	}
	while (nclust-- > 0) {
		item = uma_zalloc(m_getzone(dn_clsize), M_WAITOK);
		uma_zfree(dn_zone_clust, item);
	}
}
#endif /* DEBUGNET */

/*
 * UMA backend page allocator for the jumbo frame zones.
 *
 * Allocates kernel virtual memory that is backed by contiguous physical
 * pages.
 */
static void *
mbuf_jumbo_alloc(uma_zone_t zone, vm_size_t bytes, int domain, uint8_t *flags,
    int wait)
{

	/* Inform UMA that this allocator uses kernel_map/object. */
	*flags = UMA_SLAB_KERNEL;
	return ((void *)kmem_alloc_contig_domainset(DOMAINSET_FIXED(domain),
	    bytes, wait, (vm_paddr_t)0, ~(vm_paddr_t)0, 1, 0,
	    VM_MEMATTR_DEFAULT));
}

/*
 * Constructor for Mbuf master zone.
 *
 * The 'arg' pointer points to a mb_args structure which
 * contains call-specific information required to support the
 * mbuf allocation API.  See mbuf.h.
 */
static int
mb_ctor_mbuf(void *mem, int size, void *arg, int how)
{
	struct mbuf *m;
	struct mb_args *args;
	int error;
	int flags;
	short type;

	args = (struct mb_args *)arg;
	type = args->type;

	/*
	 * The mbuf is initialized later.  The caller has the
	 * responsibility to set up any MAC labels too.
	 */
	if (type == MT_NOINIT)
		return (0);

	m = (struct mbuf *)mem;
	flags = args->flags;
	MPASS((flags & M_NOFREE) == 0);

	error = m_init(m, how, type, flags);

	return (error);
}

/*
 * The Mbuf master zone destructor.
 */
static void
mb_dtor_mbuf(void *mem, int size, void *arg)
{
	struct mbuf *m;
	unsigned long flags;

	m = (struct mbuf *)mem;
	flags = (unsigned long)arg;

	KASSERT((m->m_flags & M_NOFREE) == 0, ("%s: M_NOFREE set", __func__));
	if (!(flags & MB_DTOR_SKIP) && (m->m_flags & M_PKTHDR) && !SLIST_EMPTY(&m->m_pkthdr.tags))
		m_tag_delete_chain(m, NULL);
}

/*
 * The Mbuf Packet zone destructor.
 */
static void
mb_dtor_pack(void *mem, int size, void *arg)
{
	struct mbuf *m;

	m = (struct mbuf *)mem;
	if ((m->m_flags & M_PKTHDR) != 0)
		m_tag_delete_chain(m, NULL);

	/* Make sure we've got a clean cluster back. */
	KASSERT((m->m_flags & M_EXT) == M_EXT, ("%s: M_EXT not set", __func__));
	KASSERT(m->m_ext.ext_buf != NULL, ("%s: ext_buf == NULL", __func__));
	KASSERT(m->m_ext.ext_free == NULL, ("%s: ext_free != NULL", __func__));
	KASSERT(m->m_ext.ext_arg1 == NULL, ("%s: ext_arg1 != NULL", __func__));
	KASSERT(m->m_ext.ext_arg2 == NULL, ("%s: ext_arg2 != NULL", __func__));
	KASSERT(m->m_ext.ext_size == MCLBYTES, ("%s: ext_size != MCLBYTES", __func__));
	KASSERT(m->m_ext.ext_type == EXT_PACKET, ("%s: ext_type != EXT_PACKET", __func__));
#ifdef INVARIANTS
	trash_dtor(m->m_ext.ext_buf, MCLBYTES, arg);
#endif
	/*
	 * If there are processes blocked on zone_clust, waiting for pages
	 * to be freed up, cause them to be woken up by draining the
	 * packet zone.  We are exposed to a race here (in the check for
	 * the UMA_ZFLAG_FULL) where we might miss the flag set, but that
	 * is deliberate. We don't want to acquire the zone lock for every
	 * mbuf free.
	 */
	if (uma_zone_exhausted(zone_clust))
		uma_zone_reclaim(zone_pack, UMA_RECLAIM_DRAIN);
}

/*
 * The Cluster and Jumbo[PAGESIZE|9|16] zone constructor.
 *
 * Here the 'arg' pointer points to the Mbuf which we
 * are configuring cluster storage for.  If 'arg' is
 * empty we allocate just the cluster without setting
 * the mbuf to it.  See mbuf.h.
 */
static int
mb_ctor_clust(void *mem, int size, void *arg, int how)
{
	struct mbuf *m;

	m = (struct mbuf *)arg;
	if (m != NULL) {
		m->m_ext.ext_buf = (char *)mem;
		m->m_data = m->m_ext.ext_buf;
		m->m_flags |= M_EXT;
		m->m_ext.ext_free = NULL;
		m->m_ext.ext_arg1 = NULL;
		m->m_ext.ext_arg2 = NULL;
		m->m_ext.ext_size = size;
		m->m_ext.ext_type = m_gettype(size);
		m->m_ext.ext_flags = EXT_FLAG_EMBREF;
		m->m_ext.ext_count = 1;
	}

	return (0);
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
	if (uma_zalloc_arg(zone_clust, m, how) == NULL ||
	    m->m_ext.ext_buf == NULL)
		return (ENOMEM);
	m->m_ext.ext_type = EXT_PACKET;	/* Override. */
#ifdef INVARIANTS
	trash_init(m->m_ext.ext_buf, MCLBYTES, how);
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

/*
 * The "packet" keg constructor.
 */
static int
mb_ctor_pack(void *mem, int size, void *arg, int how)
{
	struct mbuf *m;
	struct mb_args *args;
	int error, flags;
	short type;

	m = (struct mbuf *)mem;
	args = (struct mb_args *)arg;
	flags = args->flags;
	type = args->type;
	MPASS((flags & M_NOFREE) == 0);

#ifdef INVARIANTS
	trash_ctor(m->m_ext.ext_buf, MCLBYTES, arg, how);
#endif

	error = m_init(m, how, type, flags);

	/* m_ext is already initialized. */
	m->m_data = m->m_ext.ext_buf;
 	m->m_flags = (flags | M_EXT);

	return (error);
}

/*
 * This is the protocol drain routine.  Called by UMA whenever any of the
 * mbuf zones is closed to its limit.
 *
 * No locks should be held when this is called.  The drain routines have to
 * presently acquire some locks which raises the possibility of lock order
 * reversal.
 */
static void
mb_reclaim(uma_zone_t zone __unused, int pending __unused)
{
	struct domain *dp;
	struct protosw *pr;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK | WARN_PANIC, NULL, __func__);

	for (dp = domains; dp != NULL; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_drain != NULL)
				(*pr->pr_drain)();
}

/*
 * Free "count" units of I/O from an mbuf chain.  They could be held
 * in EXT_PGS or just as a normal mbuf.  This code is intended to be
 * called in an error path (I/O error, closed connection, etc).
 */
void
mb_free_notready(struct mbuf *m, int count)
{
	int i;

	for (i = 0; i < count && m != NULL; i++) {
		if ((m->m_flags & M_EXT) != 0 &&
		    m->m_ext.ext_type == EXT_PGS) {
			m->m_ext.ext_pgs->nrdy--;
			if (m->m_ext.ext_pgs->nrdy != 0)
				continue;
		}
		m = m_free(m);
	}
	KASSERT(i == count, ("Removed only %d items from %p", i, m));
}

/*
 * Compress an unmapped mbuf into a simple mbuf when it holds a small
 * amount of data.  This is used as a DOS defense to avoid having
 * small packets tie up wired pages, an ext_pgs structure, and an
 * mbuf.  Since this converts the existing mbuf in place, it can only
 * be used if there are no other references to 'm'.
 */
int
mb_unmapped_compress(struct mbuf *m)
{
	volatile u_int *refcnt;
	struct mbuf m_temp;

	/*
	 * Assert that 'm' does not have a packet header.  If 'm' had
	 * a packet header, it would only be able to hold MHLEN bytes
	 * and m_data would have to be initialized differently.
	 */
	KASSERT((m->m_flags & M_PKTHDR) == 0 && (m->m_flags & M_EXT) &&
	    m->m_ext.ext_type == EXT_PGS,
            ("%s: m %p !M_EXT or !EXT_PGS or M_PKTHDR", __func__, m));
	KASSERT(m->m_len <= MLEN, ("m_len too large %p", m));

	if (m->m_ext.ext_flags & EXT_FLAG_EMBREF) {
		refcnt = &m->m_ext.ext_count;
	} else {
		KASSERT(m->m_ext.ext_cnt != NULL,
		    ("%s: no refcounting pointer on %p", __func__, m));
		refcnt = m->m_ext.ext_cnt;
	}

	if (*refcnt != 1)
		return (EBUSY);

	/*
	 * Copy mbuf header and m_ext portion of 'm' to 'm_temp' to
	 * create a "fake" EXT_PGS mbuf that can be used with
	 * m_copydata() as well as the ext_free callback.
	 */
	memcpy(&m_temp, m, offsetof(struct mbuf, m_ext) + sizeof (m->m_ext));
	m_temp.m_next = NULL;
	m_temp.m_nextpkt = NULL;

	/* Turn 'm' into a "normal" mbuf. */
	m->m_flags &= ~(M_EXT | M_RDONLY | M_NOMAP);
	m->m_data = m->m_dat;

	/* Copy data from template's ext_pgs. */
	m_copydata(&m_temp, 0, m_temp.m_len, mtod(m, caddr_t));

	/* Free the backing pages. */
	m_temp.m_ext.ext_free(&m_temp);

	/* Finally, free the ext_pgs struct. */
	uma_zfree(zone_extpgs, m_temp.m_ext.ext_pgs);
	return (0);
}

/*
 * These next few routines are used to permit downgrading an unmapped
 * mbuf to a chain of mapped mbufs.  This is used when an interface
 * doesn't supported unmapped mbufs or if checksums need to be
 * computed in software.
 *
 * Each unmapped mbuf is converted to a chain of mbufs.  First, any
 * TLS header data is stored in a regular mbuf.  Second, each page of
 * unmapped data is stored in an mbuf with an EXT_SFBUF external
 * cluster.  These mbufs use an sf_buf to provide a valid KVA for the
 * associated physical page.  They also hold a reference on the
 * original EXT_PGS mbuf to ensure the physical page doesn't go away.
 * Finally, any TLS trailer data is stored in a regular mbuf.
 *
 * mb_unmapped_free_mext() is the ext_free handler for the EXT_SFBUF
 * mbufs.  It frees the associated sf_buf and releases its reference
 * on the original EXT_PGS mbuf.
 *
 * _mb_unmapped_to_ext() is a helper function that converts a single
 * unmapped mbuf into a chain of mbufs.
 *
 * mb_unmapped_to_ext() is the public function that walks an mbuf
 * chain converting any unmapped mbufs to mapped mbufs.  It returns
 * the new chain of unmapped mbufs on success.  On failure it frees
 * the original mbuf chain and returns NULL.
 */
static void
mb_unmapped_free_mext(struct mbuf *m)
{
	struct sf_buf *sf;
	struct mbuf *old_m;

	sf = m->m_ext.ext_arg1;
	sf_buf_free(sf);

	/* Drop the reference on the backing EXT_PGS mbuf. */
	old_m = m->m_ext.ext_arg2;
	mb_free_ext(old_m);
}

static struct mbuf *
_mb_unmapped_to_ext(struct mbuf *m)
{
	struct mbuf_ext_pgs *ext_pgs;
	struct mbuf *m_new, *top, *prev, *mref;
	struct sf_buf *sf;
	vm_page_t pg;
	int i, len, off, pglen, pgoff, seglen, segoff;
	volatile u_int *refcnt;
	u_int ref_inc = 0;

	MBUF_EXT_PGS_ASSERT(m);
	ext_pgs = m->m_ext.ext_pgs;
	len = m->m_len;
	KASSERT(ext_pgs->tls == NULL, ("%s: can't convert TLS mbuf %p",
	    __func__, m));

	/* See if this is the mbuf that holds the embedded refcount. */
	if (m->m_ext.ext_flags & EXT_FLAG_EMBREF) {
		refcnt = &m->m_ext.ext_count;
		mref = m;
	} else {
		KASSERT(m->m_ext.ext_cnt != NULL,
		    ("%s: no refcounting pointer on %p", __func__, m));
		refcnt = m->m_ext.ext_cnt;
		mref = __containerof(refcnt, struct mbuf, m_ext.ext_count);
	}

	/* Skip over any data removed from the front. */
	off = mtod(m, vm_offset_t);

	top = NULL;
	if (ext_pgs->hdr_len != 0) {
		if (off >= ext_pgs->hdr_len) {
			off -= ext_pgs->hdr_len;
		} else {
			seglen = ext_pgs->hdr_len - off;
			segoff = off;
			seglen = min(seglen, len);
			off = 0;
			len -= seglen;
			m_new = m_get(M_NOWAIT, MT_DATA);
			if (m_new == NULL)
				goto fail;
			m_new->m_len = seglen;
			prev = top = m_new;
			memcpy(mtod(m_new, void *), &ext_pgs->hdr[segoff],
			    seglen);
		}
	}
	pgoff = ext_pgs->first_pg_off;
	for (i = 0; i < ext_pgs->npgs && len > 0; i++) {
		pglen = mbuf_ext_pg_len(ext_pgs, i, pgoff);
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

		pg = PHYS_TO_VM_PAGE(ext_pgs->pa[i]);
		m_new = m_get(M_NOWAIT, MT_DATA);
		if (m_new == NULL)
			goto fail;
		if (top == NULL) {
			top = prev = m_new;
		} else {
			prev->m_next = m_new;
			prev = m_new;
		}
		sf = sf_buf_alloc(pg, SFB_NOWAIT);
		if (sf == NULL)
			goto fail;

		ref_inc++;
		m_extadd(m_new, (char *)sf_buf_kva(sf), PAGE_SIZE,
		    mb_unmapped_free_mext, sf, mref, M_RDONLY, EXT_SFBUF);
		m_new->m_data += segoff;
		m_new->m_len = seglen;

		pgoff = 0;
	};
	if (len != 0) {
		KASSERT((off + len) <= ext_pgs->trail_len,
		    ("off + len > trail (%d + %d > %d)", off, len,
		    ext_pgs->trail_len));
		m_new = m_get(M_NOWAIT, MT_DATA);
		if (m_new == NULL)
			goto fail;
		if (top == NULL)
			top = m_new;
		else
			prev->m_next = m_new;
		m_new->m_len = len;
		memcpy(mtod(m_new, void *), &ext_pgs->trail[off], len);
	}

	if (ref_inc != 0) {
		/*
		 * Obtain an additional reference on the old mbuf for
		 * each created EXT_SFBUF mbuf.  They will be dropped
		 * in mb_unmapped_free_mext().
		 */
		if (*refcnt == 1)
			*refcnt += ref_inc;
		else
			atomic_add_int(refcnt, ref_inc);
	}
	m_free(m);
	return (top);

fail:
	if (ref_inc != 0) {
		/*
		 * Obtain an additional reference on the old mbuf for
		 * each created EXT_SFBUF mbuf.  They will be
		 * immediately dropped when these mbufs are freed
		 * below.
		 */
		if (*refcnt == 1)
			*refcnt += ref_inc;
		else
			atomic_add_int(refcnt, ref_inc);
	}
	m_free(m);
	m_freem(top);
	return (NULL);
}

struct mbuf *
mb_unmapped_to_ext(struct mbuf *top)
{
	struct mbuf *m, *next, *prev = NULL;

	prev = NULL;
	for (m = top; m != NULL; m = next) {
		/* m might be freed, so cache the next pointer. */
		next = m->m_next;
		if (m->m_flags & M_NOMAP) {
			if (prev != NULL) {
				/*
				 * Remove 'm' from the new chain so
				 * that the 'top' chain terminates
				 * before 'm' in case 'top' is freed
				 * due to an error.
				 */
				prev->m_next = NULL;
			}
			m = _mb_unmapped_to_ext(m);
			if (m == NULL) {
				m_freem(top);
				m_freem(next);
				return (NULL);
			}
			if (prev == NULL) {
				top = m;
			} else {
				prev->m_next = m;
			}

			/*
			 * Replaced one mbuf with a chain, so we must
			 * find the end of chain.
			 */
			prev = m_last(m);
		} else {
			if (prev != NULL) {
				prev->m_next = m;
			}
			prev = m;
		}
	}
	return (top);
}

/*
 * Allocate an empty EXT_PGS mbuf.  The ext_free routine is
 * responsible for freeing any pages backing this mbuf when it is
 * freed.
 */
struct mbuf *
mb_alloc_ext_pgs(int how, bool pkthdr, m_ext_free_t ext_free)
{
	struct mbuf *m;
	struct mbuf_ext_pgs *ext_pgs;

	if (pkthdr)
		m = m_gethdr(how, MT_DATA);
	else
		m = m_get(how, MT_DATA);
	if (m == NULL)
		return (NULL);

	ext_pgs = uma_zalloc(zone_extpgs, how);
	if (ext_pgs == NULL) {
		m_free(m);
		return (NULL);
	}
	ext_pgs->npgs = 0;
	ext_pgs->nrdy = 0;
	ext_pgs->first_pg_off = 0;
	ext_pgs->last_pg_len = 0;
	ext_pgs->flags = 0;
	ext_pgs->hdr_len = 0;
	ext_pgs->trail_len = 0;
	ext_pgs->tls = NULL;
	ext_pgs->so = NULL;
	m->m_data = NULL;
	m->m_flags |= (M_EXT | M_RDONLY | M_NOMAP);
	m->m_ext.ext_type = EXT_PGS;
	m->m_ext.ext_flags = EXT_FLAG_EMBREF;
	m->m_ext.ext_count = 1;
	m->m_ext.ext_pgs = ext_pgs;
	m->m_ext.ext_size = 0;
	m->m_ext.ext_free = ext_free;
	return (m);
}

#ifdef INVARIANT_SUPPORT
void
mb_ext_pgs_check(struct mbuf_ext_pgs *ext_pgs)
{

	/*
	 * NB: This expects a non-empty buffer (npgs > 0 and
	 * last_pg_len > 0).
	 */
	KASSERT(ext_pgs->npgs > 0,
	    ("ext_pgs with no valid pages: %p", ext_pgs));
	KASSERT(ext_pgs->npgs <= nitems(ext_pgs->pa),
	    ("ext_pgs with too many pages: %p", ext_pgs));
	KASSERT(ext_pgs->nrdy <= ext_pgs->npgs,
	    ("ext_pgs with too many ready pages: %p", ext_pgs));
	KASSERT(ext_pgs->first_pg_off < PAGE_SIZE,
	    ("ext_pgs with too large page offset: %p", ext_pgs));
	KASSERT(ext_pgs->last_pg_len > 0,
	    ("ext_pgs with zero last page length: %p", ext_pgs));
	KASSERT(ext_pgs->last_pg_len <= PAGE_SIZE,
	    ("ext_pgs with too large last page length: %p", ext_pgs));
	if (ext_pgs->npgs == 1) {
		KASSERT(ext_pgs->first_pg_off + ext_pgs->last_pg_len <=
		    PAGE_SIZE, ("ext_pgs with single page too large: %p",
		    ext_pgs));
	}
	KASSERT(ext_pgs->hdr_len <= sizeof(ext_pgs->hdr),
	    ("ext_pgs with too large header length: %p", ext_pgs));
	KASSERT(ext_pgs->trail_len <= sizeof(ext_pgs->trail),
	    ("ext_pgs with too large header length: %p", ext_pgs));
}
#endif

/*
 * Clean up after mbufs with M_EXT storage attached to them if the
 * reference count hits 1.
 */
void
mb_free_ext(struct mbuf *m)
{
	volatile u_int *refcnt;
	struct mbuf *mref;
	int freembuf;

	KASSERT(m->m_flags & M_EXT, ("%s: M_EXT not set on %p", __func__, m));

	/* See if this is the mbuf that holds the embedded refcount. */
	if (m->m_ext.ext_flags & EXT_FLAG_EMBREF) {
		refcnt = &m->m_ext.ext_count;
		mref = m;
	} else {
		KASSERT(m->m_ext.ext_cnt != NULL,
		    ("%s: no refcounting pointer on %p", __func__, m));
		refcnt = m->m_ext.ext_cnt;
		mref = __containerof(refcnt, struct mbuf, m_ext.ext_count);
	}

	/*
	 * Check if the header is embedded in the cluster.  It is
	 * important that we can't touch any of the mbuf fields
	 * after we have freed the external storage, since mbuf
	 * could have been embedded in it.  For now, the mbufs
	 * embedded into the cluster are always of type EXT_EXTREF,
	 * and for this type we won't free the mref.
	 */
	if (m->m_flags & M_NOFREE) {
		freembuf = 0;
		KASSERT(m->m_ext.ext_type == EXT_EXTREF ||
		    m->m_ext.ext_type == EXT_RXRING,
		    ("%s: no-free mbuf %p has wrong type", __func__, m));
	} else
		freembuf = 1;

	/* Free attached storage if this mbuf is the only reference to it. */
	if (*refcnt == 1 || atomic_fetchadd_int(refcnt, -1) == 1) {
		switch (m->m_ext.ext_type) {
		case EXT_PACKET:
			/* The packet zone is special. */
			if (*refcnt == 0)
				*refcnt = 1;
			uma_zfree(zone_pack, mref);
			break;
		case EXT_CLUSTER:
			uma_zfree(zone_clust, m->m_ext.ext_buf);
			uma_zfree(zone_mbuf, mref);
			break;
		case EXT_JUMBOP:
			uma_zfree(zone_jumbop, m->m_ext.ext_buf);
			uma_zfree(zone_mbuf, mref);
			break;
		case EXT_JUMBO9:
			uma_zfree(zone_jumbo9, m->m_ext.ext_buf);
			uma_zfree(zone_mbuf, mref);
			break;
		case EXT_JUMBO16:
			uma_zfree(zone_jumbo16, m->m_ext.ext_buf);
			uma_zfree(zone_mbuf, mref);
			break;
		case EXT_PGS: {
#ifdef KERN_TLS
			struct mbuf_ext_pgs *pgs;
			struct ktls_session *tls;
#endif

			KASSERT(mref->m_ext.ext_free != NULL,
			    ("%s: ext_free not set", __func__));
			mref->m_ext.ext_free(mref);
#ifdef KERN_TLS
			pgs = mref->m_ext.ext_pgs;
			tls = pgs->tls;
			if (tls != NULL &&
			    !refcount_release_if_not_last(&tls->refcount))
				ktls_enqueue_to_free(pgs);
			else
#endif
				uma_zfree(zone_extpgs, mref->m_ext.ext_pgs);
			uma_zfree(zone_mbuf, mref);
			break;
		}
		case EXT_SFBUF:
		case EXT_NET_DRV:
		case EXT_MOD_TYPE:
		case EXT_DISPOSABLE:
			KASSERT(mref->m_ext.ext_free != NULL,
			    ("%s: ext_free not set", __func__));
			mref->m_ext.ext_free(mref);
			uma_zfree(zone_mbuf, mref);
			break;
		case EXT_EXTREF:
			KASSERT(m->m_ext.ext_free != NULL,
			    ("%s: ext_free not set", __func__));
			m->m_ext.ext_free(m);
			break;
		case EXT_RXRING:
			KASSERT(m->m_ext.ext_free == NULL,
			    ("%s: ext_free is set", __func__));
			break;
		default:
			KASSERT(m->m_ext.ext_type == 0,
			    ("%s: unknown ext_type", __func__));
		}
	}

	if (freembuf && m != mref)
		uma_zfree(zone_mbuf, m);
}

/*
 * Official mbuf(9) allocation KPI for stack and drivers:
 *
 * m_get()	- a single mbuf without any attachments, sys/mbuf.h.
 * m_gethdr()	- a single mbuf initialized as M_PKTHDR, sys/mbuf.h.
 * m_getcl()	- an mbuf + 2k cluster, sys/mbuf.h.
 * m_clget()	- attach cluster to already allocated mbuf.
 * m_cljget()	- attach jumbo cluster to already allocated mbuf.
 * m_get2()	- allocate minimum mbuf that would fit size argument.
 * m_getm2()	- allocate a chain of mbufs/clusters.
 * m_extadd()	- attach external cluster to mbuf.
 *
 * m_free()	- free single mbuf with its tags and ext, sys/mbuf.h.
 * m_freem()	- free chain of mbufs.
 */

int
m_clget(struct mbuf *m, int how)
{

	KASSERT((m->m_flags & M_EXT) == 0, ("%s: mbuf %p has M_EXT",
	    __func__, m));
	m->m_ext.ext_buf = (char *)NULL;
	uma_zalloc_arg(zone_clust, m, how);
	/*
	 * On a cluster allocation failure, drain the packet zone and retry,
	 * we might be able to loosen a few clusters up on the drain.
	 */
	if ((how & M_NOWAIT) && (m->m_ext.ext_buf == NULL)) {
		uma_zone_reclaim(zone_pack, UMA_RECLAIM_DRAIN);
		uma_zalloc_arg(zone_clust, m, how);
	}
	MBUF_PROBE2(m__clget, m, how);
	return (m->m_flags & M_EXT);
}

/*
 * m_cljget() is different from m_clget() as it can allocate clusters without
 * attaching them to an mbuf.  In that case the return value is the pointer
 * to the cluster of the requested size.  If an mbuf was specified, it gets
 * the cluster attached to it and the return value can be safely ignored.
 * For size it takes MCLBYTES, MJUMPAGESIZE, MJUM9BYTES, MJUM16BYTES.
 */
void *
m_cljget(struct mbuf *m, int how, int size)
{
	uma_zone_t zone;
	void *retval;

	if (m != NULL) {
		KASSERT((m->m_flags & M_EXT) == 0, ("%s: mbuf %p has M_EXT",
		    __func__, m));
		m->m_ext.ext_buf = NULL;
	}

	zone = m_getzone(size);
	retval = uma_zalloc_arg(zone, m, how);

	MBUF_PROBE4(m__cljget, m, how, size, retval);

	return (retval);
}

/*
 * m_get2() allocates minimum mbuf that would fit "size" argument.
 */
struct mbuf *
m_get2(int size, int how, short type, int flags)
{
	struct mb_args args;
	struct mbuf *m, *n;

	args.flags = flags;
	args.type = type;

	if (size <= MHLEN || (size <= MLEN && (flags & M_PKTHDR) == 0))
		return (uma_zalloc_arg(zone_mbuf, &args, how));
	if (size <= MCLBYTES)
		return (uma_zalloc_arg(zone_pack, &args, how));

	if (size > MJUMPAGESIZE)
		return (NULL);

	m = uma_zalloc_arg(zone_mbuf, &args, how);
	if (m == NULL)
		return (NULL);

	n = uma_zalloc_arg(zone_jumbop, m, how);
	if (n == NULL) {
		uma_zfree(zone_mbuf, m);
		return (NULL);
	}

	return (m);
}

/*
 * m_getjcl() returns an mbuf with a cluster of the specified size attached.
 * For size it takes MCLBYTES, MJUMPAGESIZE, MJUM9BYTES, MJUM16BYTES.
 */
struct mbuf *
m_getjcl(int how, short type, int flags, int size)
{
	struct mb_args args;
	struct mbuf *m, *n;
	uma_zone_t zone;

	if (size == MCLBYTES)
		return m_getcl(how, type, flags);

	args.flags = flags;
	args.type = type;

	m = uma_zalloc_arg(zone_mbuf, &args, how);
	if (m == NULL)
		return (NULL);

	zone = m_getzone(size);
	n = uma_zalloc_arg(zone, m, how);
	if (n == NULL) {
		uma_zfree(zone_mbuf, m);
		return (NULL);
	}
	return (m);
}

/*
 * Allocate a given length worth of mbufs and/or clusters (whatever fits
 * best) and return a pointer to the top of the allocated chain.  If an
 * existing mbuf chain is provided, then we will append the new chain
 * to the existing one and return a pointer to the provided mbuf.
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
		len -= M_SIZE(mb);
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

/*-
 * Configure a provided mbuf to refer to the provided external storage
 * buffer and setup a reference count for said buffer.
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
m_extadd(struct mbuf *mb, char *buf, u_int size, m_ext_free_t freef,
    void *arg1, void *arg2, int flags, int type)
{

	KASSERT(type != EXT_CLUSTER, ("%s: EXT_CLUSTER not allowed", __func__));

	mb->m_flags |= (M_EXT | flags);
	mb->m_ext.ext_buf = buf;
	mb->m_data = mb->m_ext.ext_buf;
	mb->m_ext.ext_size = size;
	mb->m_ext.ext_free = freef;
	mb->m_ext.ext_arg1 = arg1;
	mb->m_ext.ext_arg2 = arg2;
	mb->m_ext.ext_type = type;

	if (type != EXT_EXTREF) {
		mb->m_ext.ext_count = 1;
		mb->m_ext.ext_flags = EXT_FLAG_EMBREF;
	} else
		mb->m_ext.ext_flags = 0;
}

/*
 * Free an entire chain of mbufs and associated external buffers, if
 * applicable.
 */
void
m_freem(struct mbuf *mb)
{

	MBUF_PROBE1(m__freem, mb);
	while (mb != NULL)
		mb = m_free(mb);
}

void
m_snd_tag_init(struct m_snd_tag *mst, struct ifnet *ifp)
{

	if_ref(ifp);
	mst->ifp = ifp;
	refcount_init(&mst->refcount, 1);
	counter_u64_add(snd_tag_count, 1);
}

void
m_snd_tag_destroy(struct m_snd_tag *mst)
{
	struct ifnet *ifp;

	ifp = mst->ifp;
	ifp->if_snd_tag_free(mst);
	if_rele(ifp);
	counter_u64_add(snd_tag_count, -1);
}
