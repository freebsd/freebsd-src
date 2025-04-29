/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include "opt_param.h"
#include "opt_kern_tls.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/domainset.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/ktls.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
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
#include <vm/vm_pageout.h>
#include <vm/vm_map.h>
#include <vm/uma.h>
#include <vm/uma_dbg.h>

_Static_assert(MJUMPAGESIZE > MCLBYTES,
    "Cluster must be smaller than a jumbo page");

/*
 * In FreeBSD, Mbufs and Mbuf Clusters are allocated from UMA
 * Zones.
 *
 * Mbuf Clusters (2K, contiguous) are allocated from the Cluster
 * Zone.  The Zone can be capped at kern.ipc.nmbclusters, if the
 * administrator so desires.
 *
 * Mbufs are allocated from a UMA Primary Zone called the Mbuf
 * Zone.
 *
 * Additionally, FreeBSD provides a Packet Zone, which it
 * configures as a Secondary Zone to the Mbuf Primary Zone,
 * thus sharing backend Slab kegs with the Mbuf Primary Zone.
 *
 * Thus common-case allocations and locking are simplified:
 *
 *  m_clget()                m_getcl()
 *    |                         |
 *    |   .------------>[(Packet Cache)]    m_get(), m_gethdr()
 *    |   |             [     Packet   ]            |
 *  [(Cluster Cache)]   [    Secondary ]   [ (Mbuf Cache)     ]
 *  [ Cluster Zone  ]   [     Zone     ]   [ Mbuf Primary Zone ]
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
 * Caches are per-CPU and are filled from the Primary Zone.
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

bool mb_use_ext_pgs = false;	/* use M_EXTPG mbufs for sendfile & TLS */

static int
sysctl_mb_use_ext_pgs(SYSCTL_HANDLER_ARGS)
{
	int error, extpg;

	extpg = mb_use_ext_pgs;
	error = sysctl_handle_int(oidp, &extpg, 0, req);
	if (error == 0 && req->newptr != NULL) {
		if (extpg != 0 && !PMAP_HAS_DMAP)
			error = EOPNOTSUPP;
		else
			mb_use_ext_pgs = extpg != 0;
	}
	return (error);
}
SYSCTL_PROC(_kern_ipc, OID_AUTO, mb_use_ext_pgs,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_NOFETCH,
    &mb_use_ext_pgs, 0, sysctl_mb_use_ext_pgs, "IU",
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
	int extpg;

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

	/*
	 * Unmapped mbufs can only safely be used on platforms with a direct
	 * map.
	 */
	if (PMAP_HAS_DMAP) {
		extpg = 1;
		TUNABLE_INT_FETCH("kern.ipc.mb_use_ext_pgs", &extpg);
		mb_use_ext_pgs = extpg != 0;
	}
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
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbclusters,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE,
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
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbjumbop,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE,
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
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbjumbo9,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE,
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
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbjumbo16,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE,
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
SYSCTL_PROC(_kern_ipc, OID_AUTO, nmbufs,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE,
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

/* Ensure that MSIZE is a power of 2. */
CTASSERT((((MSIZE - 1) ^ MSIZE) + 1) >> 1 == MSIZE);

_Static_assert(sizeof(struct mbuf) <= MSIZE,
    "size of mbuf exceeds MSIZE");
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
	    MSIZE - 1, UMA_ZONE_CONTIG | UMA_ZONE_MAXBUCKET);
	if (nmbufs > 0)
		nmbufs = uma_zone_set_max(zone_mbuf, nmbufs);
	uma_zone_set_warning(zone_mbuf, "kern.ipc.nmbufs limit reached");
	uma_zone_set_maxaction(zone_mbuf, mb_reclaim);

	zone_clust = uma_zcreate(MBUF_CLUSTER_MEM_NAME, MCLBYTES,
	    mb_ctor_clust, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_CONTIG);
	if (nmbclusters > 0)
		nmbclusters = uma_zone_set_max(zone_clust, nmbclusters);
	uma_zone_set_warning(zone_clust, "kern.ipc.nmbclusters limit reached");
	uma_zone_set_maxaction(zone_clust, mb_reclaim);

	zone_pack = uma_zsecond_create(MBUF_PACKET_MEM_NAME, mb_ctor_pack,
	    mb_dtor_pack, mb_zinit_pack, mb_zfini_pack, zone_mbuf);

	/* Make jumbo frame zone too. Page size, 9k and 16k. */
	zone_jumbop = uma_zcreate(MBUF_JUMBOP_MEM_NAME, MJUMPAGESIZE,
	    mb_ctor_clust, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_CONTIG);
	if (nmbjumbop > 0)
		nmbjumbop = uma_zone_set_max(zone_jumbop, nmbjumbop);
	uma_zone_set_warning(zone_jumbop, "kern.ipc.nmbjumbop limit reached");
	uma_zone_set_maxaction(zone_jumbop, mb_reclaim);

	zone_jumbo9 = uma_zcreate(MBUF_JUMBO9_MEM_NAME, MJUM9BYTES,
	    mb_ctor_clust, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_CONTIG);
	if (nmbjumbo9 > 0)
		nmbjumbo9 = uma_zone_set_max(zone_jumbo9, nmbjumbo9);
	uma_zone_set_warning(zone_jumbo9, "kern.ipc.nmbjumbo9 limit reached");
	uma_zone_set_maxaction(zone_jumbo9, mb_reclaim);

	zone_jumbo16 = uma_zcreate(MBUF_JUMBO16_MEM_NAME, MJUM16BYTES,
	    mb_ctor_clust, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_CONTIG);
	if (nmbjumbo16 > 0)
		nmbjumbo16 = uma_zone_set_max(zone_jumbo16, nmbjumbo16);
	uma_zone_set_warning(zone_jumbo16, "kern.ipc.nmbjumbo16 limit reached");
	uma_zone_set_maxaction(zone_jumbo16, mb_reclaim);

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
		m = m_get(M_NOWAIT, MT_DATA);
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
		m = m_get(M_WAITOK, MT_DATA);
		uma_zfree(dn_zone_mbuf, m);
	}
	while (nclust-- > 0) {
		item = uma_zalloc(m_getzone(dn_clsize), M_WAITOK);
		uma_zfree(dn_zone_clust, item);
	}
}
#endif /* DEBUGNET */

/*
 * Constructor for Mbuf primary zone.
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
 * The Mbuf primary zone destructor.
 */
static void
mb_dtor_mbuf(void *mem, int size, void *arg)
{
	struct mbuf *m;
	unsigned long flags __diagused;

	m = (struct mbuf *)mem;
	flags = (unsigned long)arg;

	KASSERT((m->m_flags & M_NOFREE) == 0, ("%s: M_NOFREE set", __func__));
	KASSERT((flags & 0x1) == 0, ("%s: obsolete MB_DTOR_SKIP passed", __func__));
	if ((m->m_flags & M_PKTHDR) && !SLIST_EMPTY(&m->m_pkthdr.tags))
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
#if defined(INVARIANTS) && !defined(KMSAN)
	trash_dtor(m->m_ext.ext_buf, MCLBYTES, zone_clust);
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
#if defined(INVARIANTS) && !defined(KMSAN)
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
#if defined(INVARIANTS) && !defined(KMSAN)
	trash_fini(m->m_ext.ext_buf, MCLBYTES);
#endif
	uma_zfree_arg(zone_clust, m->m_ext.ext_buf, NULL);
#if defined(INVARIANTS) && !defined(KMSAN)
	trash_dtor(mem, size, zone_clust);
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

#if defined(INVARIANTS) && !defined(KMSAN)
	trash_ctor(m->m_ext.ext_buf, MCLBYTES, zone_clust, how);
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
 */
static void
mb_reclaim(uma_zone_t zone __unused, int pending __unused)
{

	EVENTHANDLER_INVOKE(mbuf_lowmem, VM_LOW_MBUFS);
}

/*
 * Free "count" units of I/O from an mbuf chain.  They could be held
 * in M_EXTPG or just as a normal mbuf.  This code is intended to be
 * called in an error path (I/O error, closed connection, etc).
 */
void
mb_free_notready(struct mbuf *m, int count)
{
	int i;

	for (i = 0; i < count && m != NULL; i++) {
		if ((m->m_flags & M_EXTPG) != 0) {
			m->m_epg_nrdy--;
			if (m->m_epg_nrdy != 0)
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
	char buf[MLEN];

	/*
	 * Assert that 'm' does not have a packet header.  If 'm' had
	 * a packet header, it would only be able to hold MHLEN bytes
	 * and m_data would have to be initialized differently.
	 */
	KASSERT((m->m_flags & M_PKTHDR) == 0 && (m->m_flags & M_EXTPG),
            ("%s: m %p !M_EXTPG or M_PKTHDR", __func__, m));
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

	m_copydata(m, 0, m->m_len, buf);

	/* Free the backing pages. */
	m->m_ext.ext_free(m);

	/* Turn 'm' into a "normal" mbuf. */
	m->m_flags &= ~(M_EXT | M_RDONLY | M_EXTPG);
	m->m_data = m->m_dat;

	/* Copy data back into m. */
	bcopy(buf, mtod(m, char *), m->m_len);

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
 * original M_EXTPG mbuf to ensure the physical page doesn't go away.
 * Finally, any TLS trailer data is stored in a regular mbuf.
 *
 * mb_unmapped_free_mext() is the ext_free handler for the EXT_SFBUF
 * mbufs.  It frees the associated sf_buf and releases its reference
 * on the original M_EXTPG mbuf.
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

	/* Drop the reference on the backing M_EXTPG mbuf. */
	old_m = m->m_ext.ext_arg2;
	mb_free_extpg(old_m);
}

static int
_mb_unmapped_to_ext(struct mbuf *m, struct mbuf **mres)
{
	struct mbuf *m_new, *top, *prev, *mref;
	struct sf_buf *sf;
	vm_page_t pg;
	int i, len, off, pglen, pgoff, seglen, segoff;
	volatile u_int *refcnt;
	u_int ref_inc = 0;

	M_ASSERTEXTPG(m);

	if (m->m_epg_tls != NULL) {
		/* can't convert TLS mbuf */
		m_free(m);
		*mres = NULL;
		return (EINVAL);
	}

	len = m->m_len;

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
	if (m->m_epg_hdrlen != 0) {
		if (off >= m->m_epg_hdrlen) {
			off -= m->m_epg_hdrlen;
		} else {
			seglen = m->m_epg_hdrlen - off;
			segoff = off;
			seglen = min(seglen, len);
			off = 0;
			len -= seglen;
			m_new = m_get(M_NOWAIT, MT_DATA);
			if (m_new == NULL)
				goto fail;
			m_new->m_len = seglen;
			prev = top = m_new;
			memcpy(mtod(m_new, void *), &m->m_epg_hdr[segoff],
			    seglen);
		}
	}
	pgoff = m->m_epg_1st_off;
	for (i = 0; i < m->m_epg_npgs && len > 0; i++) {
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
		    mb_unmapped_free_mext, sf, mref, m->m_flags & M_RDONLY,
		    EXT_SFBUF);
		m_new->m_data += segoff;
		m_new->m_len = seglen;

		pgoff = 0;
	};
	if (len != 0) {
		KASSERT((off + len) <= m->m_epg_trllen,
		    ("off + len > trail (%d + %d > %d)", off, len,
		    m->m_epg_trllen));
		m_new = m_get(M_NOWAIT, MT_DATA);
		if (m_new == NULL)
			goto fail;
		if (top == NULL)
			top = m_new;
		else
			prev->m_next = m_new;
		m_new->m_len = len;
		memcpy(mtod(m_new, void *), &m->m_epg_trail[off], len);
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
	*mres = top;
	return (0);

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
	*mres = NULL;
	return (ENOMEM);
}

int
mb_unmapped_to_ext(struct mbuf *top, struct mbuf **mres)
{
	struct mbuf *m, *m1, *next, *prev = NULL;
	int error;

	prev = NULL;
	for (m = top; m != NULL; m = next) {
		/* m might be freed, so cache the next pointer. */
		next = m->m_next;
		if (m->m_flags & M_EXTPG) {
			if (prev != NULL) {
				/*
				 * Remove 'm' from the new chain so
				 * that the 'top' chain terminates
				 * before 'm' in case 'top' is freed
				 * due to an error.
				 */
				prev->m_next = NULL;
			}
			error = _mb_unmapped_to_ext(m, &m1);
			if (error != 0) {
				if (top != m)
					m_freem(top);
				m_freem(next);
				*mres = NULL;
				return (error);
			}
			m = m1;
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
	*mres = top;
	return (0);
}

/*
 * Allocate an empty M_EXTPG mbuf.  The ext_free routine is
 * responsible for freeing any pages backing this mbuf when it is
 * freed.
 */
struct mbuf *
mb_alloc_ext_pgs(int how, m_ext_free_t ext_free, int flags)
{
	struct mbuf *m;

	m = m_get(how, MT_DATA);
	if (m == NULL)
		return (NULL);

	m->m_epg_npgs = 0;
	m->m_epg_nrdy = 0;
	m->m_epg_1st_off = 0;
	m->m_epg_last_len = 0;
	m->m_epg_flags = 0;
	m->m_epg_hdrlen = 0;
	m->m_epg_trllen = 0;
	m->m_epg_tls = NULL;
	m->m_epg_so = NULL;
	m->m_data = NULL;
	m->m_flags |= M_EXT | M_EXTPG | flags;
	m->m_ext.ext_flags = EXT_FLAG_EMBREF;
	m->m_ext.ext_count = 1;
	m->m_ext.ext_size = 0;
	m->m_ext.ext_free = ext_free;
	return (m);
}

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
			m_free_raw(mref);
			break;
		case EXT_JUMBOP:
			uma_zfree(zone_jumbop, m->m_ext.ext_buf);
			m_free_raw(mref);
			break;
		case EXT_JUMBO9:
			uma_zfree(zone_jumbo9, m->m_ext.ext_buf);
			m_free_raw(mref);
			break;
		case EXT_JUMBO16:
			uma_zfree(zone_jumbo16, m->m_ext.ext_buf);
			m_free_raw(mref);
			break;
		case EXT_SFBUF:
		case EXT_NET_DRV:
		case EXT_CTL:
		case EXT_MOD_TYPE:
		case EXT_DISPOSABLE:
			KASSERT(mref->m_ext.ext_free != NULL,
			    ("%s: ext_free not set", __func__));
			mref->m_ext.ext_free(mref);
			m_free_raw(mref);
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
		m_free_raw(m);
}

/*
 * Clean up after mbufs with M_EXTPG storage attached to them if the
 * reference count hits 1.
 */
void
mb_free_extpg(struct mbuf *m)
{
	volatile u_int *refcnt;
	struct mbuf *mref;

	M_ASSERTEXTPG(m);

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

	/* Free attached storage if this mbuf is the only reference to it. */
	if (*refcnt == 1 || atomic_fetchadd_int(refcnt, -1) == 1) {
		KASSERT(mref->m_ext.ext_free != NULL,
		    ("%s: ext_free not set", __func__));

		mref->m_ext.ext_free(mref);
#ifdef KERN_TLS
		if (mref->m_epg_tls != NULL &&
		    !refcount_release_if_not_last(&mref->m_epg_tls->refcount))
			ktls_enqueue_to_free(mref);
		else
#endif
			m_free_raw(mref);
	}

	if (m != mref)
		m_free_raw(m);
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
		m_free_raw(m);
		return (NULL);
	}

	return (m);
}

/*
 * m_get3() allocates minimum mbuf that would fit "size" argument.
 * Unlike m_get2() it can allocate clusters up to MJUM16BYTES.
 */
struct mbuf *
m_get3(int size, int how, short type, int flags)
{
	struct mb_args args;
	struct mbuf *m, *n;
	uma_zone_t zone;

	if (size <= MJUMPAGESIZE)
		return (m_get2(size, how, type, flags));

	if (size > MJUM16BYTES)
		return (NULL);

	args.flags = flags;
	args.type = type;

	m = uma_zalloc_arg(zone_mbuf, &args, how);
	if (m == NULL)
		return (NULL);

	if (size <= MJUM9BYTES)
		zone = zone_jumbo9;
	else
		zone = zone_jumbo16;

	n = uma_zalloc_arg(zone, m, how);
	if (n == NULL) {
		m_free_raw(m);
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
		m_free_raw(m);
		return (NULL);
	}
	MBUF_PROBE5(m__getjcl, how, type, flags, size, m);
	return (m);
}

/*
 * Allocate mchain of a given length of mbufs and/or clusters (whatever fits
 * best).  May fail due to ENOMEM.  In case of failure state of mchain is
 * inconsistent.
 */
int
mc_get(struct mchain *mc, u_int length, int how, short type, int flags)
{
	struct mbuf *mb;
	u_int progress;

	MPASS(length >= 0);

	*mc = MCHAIN_INITIALIZER(mc);
	flags &= (M_PKTHDR | M_EOR);
	progress = 0;

	/* Loop and append maximum sized mbufs to the chain tail. */
	do {
		if (length - progress > MCLBYTES) {
			/*
			 * M_NOWAIT here is intentional, it avoids blocking if
			 * the jumbop zone is exhausted. See 796d4eb89e2c and
			 * D26150 for more detail.
			 */
			mb = m_getjcl(M_NOWAIT, type, (flags & M_PKTHDR),
			    MJUMPAGESIZE);
		} else
			mb = NULL;
		if (mb == NULL) {
			if (length - progress >= MINCLSIZE)
				mb = m_getcl(how, type, (flags & M_PKTHDR));
			else if (flags & M_PKTHDR)
				mb = m_gethdr(how, type);
			else
				mb = m_get(how, type);

			/*
			 * Fail the whole operation if one mbuf can't be
			 * allocated.
			 */
			if (mb == NULL) {
				m_freem(mc_first(mc));
				return (ENOMEM);
			}
		}

		progress += M_SIZE(mb);
		mc_append(mc, mb);
		/* Only valid on the first mbuf. */
		flags &= ~M_PKTHDR;
	} while (progress < length);
	if (flags & M_EOR)
		/* Only valid on the last mbuf. */
		mc_last(mc)->m_flags |= M_EOR;

	return (0);
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
	struct mchain mc;

	/* Packet header mbuf must be first in chain. */
	if (m != NULL && (flags & M_PKTHDR))
		flags &= ~M_PKTHDR;

	if (__predict_false(mc_get(&mc, len, how, type, flags) != 0))
		return (NULL);

	/* If mbuf was supplied, append new chain to the end of it. */
	if (m != NULL) {
		struct mbuf *mtail;

		mtail = m_last(m);
		mtail->m_next = mc_first(&mc);
		mtail->m_flags &= ~M_EOR;
	} else
		m = mc_first(&mc);

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

/*
 * Free an entire chain of mbufs and associated external buffers, following
 * both m_next and m_nextpkt linkage.
 * Note: doesn't support NULL argument.
 */
void
m_freemp(struct mbuf *m)
{
	struct mbuf *n;

	MBUF_PROBE1(m__freemp, m);
	do {
		n = m->m_nextpkt;
		while (m != NULL)
			m = m_free(m);
		m = n;
	} while (m != NULL);
}

/*
 * Temporary primitive to allow freeing without going through m_free.
 */
void
m_free_raw(struct mbuf *mb)
{

	uma_zfree(zone_mbuf, mb);
}

int
m_snd_tag_alloc(struct ifnet *ifp, union if_snd_tag_alloc_params *params,
    struct m_snd_tag **mstp)
{

	return (if_snd_tag_alloc(ifp, params, mstp));
}

void
m_snd_tag_init(struct m_snd_tag *mst, struct ifnet *ifp,
    const struct if_snd_tag_sw *sw)
{

	if_ref(ifp);
	mst->ifp = ifp;
	refcount_init(&mst->refcount, 1);
	mst->sw = sw;
	counter_u64_add(snd_tag_count, 1);
}

void
m_snd_tag_destroy(struct m_snd_tag *mst)
{
	struct ifnet *ifp;

	ifp = mst->ifp;
	mst->sw->snd_tag_free(mst);
	if_rele(ifp);
	counter_u64_add(snd_tag_count, -1);
}

void
m_rcvif_serialize(struct mbuf *m)
{
	u_short idx, gen;

	M_ASSERTPKTHDR(m);
	idx = if_getindex(m->m_pkthdr.rcvif);
	gen = if_getidxgen(m->m_pkthdr.rcvif);
	m->m_pkthdr.rcvidx = idx;
	m->m_pkthdr.rcvgen = gen;
	if (__predict_false(m->m_pkthdr.leaf_rcvif != NULL)) {
		idx = if_getindex(m->m_pkthdr.leaf_rcvif);
		gen = if_getidxgen(m->m_pkthdr.leaf_rcvif);
	} else {
		idx = -1;
		gen = 0;
	}
	m->m_pkthdr.leaf_rcvidx = idx;
	m->m_pkthdr.leaf_rcvgen = gen;
}

struct ifnet *
m_rcvif_restore(struct mbuf *m)
{
	struct ifnet *ifp, *leaf_ifp;

	M_ASSERTPKTHDR(m);
	NET_EPOCH_ASSERT();

	ifp = ifnet_byindexgen(m->m_pkthdr.rcvidx, m->m_pkthdr.rcvgen);
	if (ifp == NULL || (if_getflags(ifp) & IFF_DYING))
		return (NULL);

	if (__predict_true(m->m_pkthdr.leaf_rcvidx == (u_short)-1)) {
		leaf_ifp = NULL;
	} else {
		leaf_ifp = ifnet_byindexgen(m->m_pkthdr.leaf_rcvidx,
		    m->m_pkthdr.leaf_rcvgen);
		if (__predict_false(leaf_ifp != NULL && (if_getflags(leaf_ifp) & IFF_DYING)))
			leaf_ifp = NULL;
	}

	m->m_pkthdr.leaf_rcvif = leaf_ifp;
	m->m_pkthdr.rcvif = ifp;

	return (ifp);
}

/*
 * Allocate an mbuf with anonymous external pages.
 */
struct mbuf *
mb_alloc_ext_plus_pages(int len, int how)
{
	struct mbuf *m;
	vm_page_t pg;
	int i, npgs;

	m = mb_alloc_ext_pgs(how, mb_free_mext_pgs, 0);
	if (m == NULL)
		return (NULL);
	m->m_epg_flags |= EPG_FLAG_ANON;
	npgs = howmany(len, PAGE_SIZE);
	for (i = 0; i < npgs; i++) {
		do {
			pg = vm_page_alloc_noobj(VM_ALLOC_NODUMP |
			    VM_ALLOC_WIRED);
			if (pg == NULL) {
				if (how == M_NOWAIT) {
					m->m_epg_npgs = i;
					m_free(m);
					return (NULL);
				}
				vm_wait(NULL);
			}
		} while (pg == NULL);
		m->m_epg_pa[i] = VM_PAGE_TO_PHYS(pg);
	}
	m->m_epg_npgs = npgs;
	return (m);
}

/*
 * Copy the data in the mbuf chain to a chain of mbufs with anonymous external
 * unmapped pages.
 * len is the length of data in the input mbuf chain.
 * mlen is the maximum number of bytes put into each ext_page mbuf.
 */
struct mbuf *
mb_mapped_to_unmapped(struct mbuf *mp, int len, int mlen, int how,
    struct mbuf **mlast)
{
	struct mbuf *m, *mout;
	char *pgpos, *mbpos;
	int i, mblen, mbufsiz, pglen, xfer;

	if (len == 0)
		return (NULL);
	mbufsiz = min(mlen, len);
	m = mout = mb_alloc_ext_plus_pages(mbufsiz, how);
	if (m == NULL)
		return (m);
	pgpos = (char *)(void *)PHYS_TO_DMAP(m->m_epg_pa[0]);
	pglen = PAGE_SIZE;
	mblen = 0;
	i = 0;
	do {
		if (pglen == 0) {
			if (++i == m->m_epg_npgs) {
				m->m_epg_last_len = PAGE_SIZE;
				mbufsiz = min(mlen, len);
				m->m_next = mb_alloc_ext_plus_pages(mbufsiz,
				    how);
				m = m->m_next;
				if (m == NULL) {
					m_freem(mout);
					return (m);
				}
				i = 0;
			}
			pgpos = (char *)(void *)PHYS_TO_DMAP(m->m_epg_pa[i]);
			pglen = PAGE_SIZE;
		}
		while (mblen == 0) {
			if (mp == NULL) {
				m_freem(mout);
				return (NULL);
			}
			KASSERT((mp->m_flags & M_EXTPG) == 0,
			    ("mb_copym_ext_pgs: ext_pgs input mbuf"));
			mbpos = mtod(mp, char *);
			mblen = mp->m_len;
			mp = mp->m_next;
		}
		xfer = min(mblen, pglen);
		memcpy(pgpos, mbpos, xfer);
		pgpos += xfer;
		mbpos += xfer;
		pglen -= xfer;
		mblen -= xfer;
		len -= xfer;
		m->m_len += xfer;
	} while (len > 0);
	m->m_epg_last_len = PAGE_SIZE - pglen;
	if (mlast != NULL)
		*mlast = m;
	return (mout);
}
