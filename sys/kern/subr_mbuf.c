/*-
 * Copyright (c) 2001, 2002
 * 	Bosko Milekic <bmilekic@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
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

#include "opt_mac.h"
#include "opt_param.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mac.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/smp.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/domain.h>
#include <sys/protosw.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

/******************************************************************************
 * mb_alloc mbuf and cluster allocator.
 *
 * Maximum number of PCPU containers. If you know what you're doing you could
 * explicitly define MBALLOC_NCPU to be exactly the number of CPUs on your
 * system during compilation, and thus prevent kernel structure bloat.
 *
 * SMP and non-SMP kernels clearly have a different number of possible CPUs,
 * but because we cannot assume a dense array of CPUs, we always allocate
 * and traverse PCPU containers up to NCPU amount and merely check for
 * CPU availability.
 */
#ifdef MBALLOC_NCPU
#define	NCPU	MBALLOC_NCPU
#else
#define	NCPU	MAXCPU
#endif

/*-
 * The mbuf allocator is heavily based on Alfred Perlstein's
 * (alfred@FreeBSD.org) "memcache" allocator which is itself based
 * on concepts from several per-CPU memory allocators. The difference
 * between this allocator and memcache is that, among other things:
 *
 * (i) We don't free back to the map from the free() routine - we leave the
 *     option of implementing lazy freeing (from a kproc) in the future. 
 *
 * (ii) We allocate from separate sub-maps of kmem_map, thus limiting the
 *	maximum number of allocatable objects of a given type. Further,
 *	we handle blocking on a cv in the case that the map is starved and
 *	we have to rely solely on cached (circulating) objects.
 *
 * The mbuf allocator keeps all objects that it allocates in mb_buckets.
 * The buckets keep a page worth of objects (an object can be an mbuf or an
 * mbuf cluster) and facilitate moving larger sets of contiguous objects
 * from the per-CPU lists to the main list for the given object. The buckets
 * also have an added advantage in that after several moves from a per-CPU
 * list to the main list and back to the per-CPU list, contiguous objects
 * are kept together, thus trying to put the TLB cache to good use.
 *
 * The buckets are kept on singly-linked lists called "containers." A container
 * is protected by a mutex lock in order to ensure consistency.  The mutex lock
 * itself is allocated separately and attached to the container at boot time,
 * thus allowing for certain containers to share the same mutex lock.  Per-CPU
 * containers for mbufs and mbuf clusters all share the same per-CPU
 * lock whereas the "general system" containers (i.e., the "main lists") for
 * these objects share one global lock.
 */
struct mb_bucket {
	SLIST_ENTRY(mb_bucket) mb_blist;
	int 	mb_owner;
	int	mb_numfree;
	void 	*mb_free[0];
};

struct mb_container {
	SLIST_HEAD(mc_buckethd, mb_bucket) mc_bhead;
	struct	mtx *mc_lock;
	int	mc_numowner;
	u_int	mc_starved;
	long	*mc_types;
	u_long	*mc_objcount;
	u_long	*mc_numpgs;
};

struct mb_gen_list {
	struct	mb_container mb_cont;
	struct	cv mgl_mstarved;
};

struct mb_pcpu_list {
	struct	mb_container mb_cont;
};

/*
 * Boot-time configurable object counts that will determine the maximum
 * number of permitted objects in the mbuf and mcluster cases.  In the
 * ext counter (nmbcnt) case, it's just an indicator serving to scale
 * kmem_map size properly - in other words, we may be allowed to allocate
 * more than nmbcnt counters, whereas we will never be allowed to allocate
 * more than nmbufs mbufs or nmbclusters mclusters.
 * As for nsfbufs, it is used to indicate how many sendfile(2) buffers will be
 * allocatable by the sfbuf allocator (found in uipc_syscalls.c)
 */
#ifndef NMBCLUSTERS
#define	NMBCLUSTERS	(1024 + maxusers * 64)
#endif
#ifndef NMBUFS
#define	NMBUFS		(nmbclusters * 2)
#endif
#ifndef NSFBUFS
#define	NSFBUFS		(512 + maxusers * 16)
#endif
#ifndef NMBCNTS
#define	NMBCNTS		(nmbclusters + nsfbufs)
#endif
int	nmbufs;
int	nmbclusters;
int	nmbcnt;
int	nsfbufs;

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
	nsfbufs = NSFBUFS;
	TUNABLE_INT_FETCH("kern.ipc.nsfbufs", &nsfbufs);
	nmbcnt = NMBCNTS;
	TUNABLE_INT_FETCH("kern.ipc.nmbcnt", &nmbcnt);
	/* Sanity checks */
	if (nmbufs < nmbclusters * 2)
		nmbufs = nmbclusters * 2;
	if (nmbcnt < nmbclusters + nsfbufs)
		nmbcnt = nmbclusters + nsfbufs;
}
SYSINIT(tunable_mbinit, SI_SUB_TUNABLES, SI_ORDER_ANY, tunable_mbinit, NULL);

/*
 * The freelist structures and mutex locks.  The number statically declared
 * here depends on the number of CPUs.
 *
 * We set up in such a way that all the objects (mbufs, clusters)
 * share the same mutex lock.  It has been established that we do not benefit
 * from different locks for different objects, so we use the same lock,
 * regardless of object type.  This also allows us to do optimised
 * multi-object allocations without dropping the lock in between.
 */
struct mb_lstmngr {
	struct mb_gen_list *ml_genlist;
	struct mb_pcpu_list *ml_cntlst[NCPU];
	struct mb_bucket **ml_btable;
	vm_map_t	ml_map;
	vm_offset_t	ml_mapbase;
	vm_offset_t	ml_maptop;
	int		ml_mapfull;
	u_int		ml_objsize;
	u_int		*ml_wmhigh;
};
static struct mb_lstmngr mb_list_mbuf, mb_list_clust;
static struct mtx mbuf_gen, mbuf_pcpu[NCPU];
u_int *cl_refcntmap;

/*
 * Local macros for internal allocator structure manipulations.
 */
#ifdef SMP
#define	MB_GET_PCPU_LIST(mb_lst)	(mb_lst)->ml_cntlst[PCPU_GET(cpuid)]
#else
#define	MB_GET_PCPU_LIST(mb_lst)	(mb_lst)->ml_cntlst[0]
#endif

#define	MB_GET_GEN_LIST(mb_lst)		(mb_lst)->ml_genlist

#define	MB_LOCK_CONT(mb_cnt)		mtx_lock((mb_cnt)->mb_cont.mc_lock)

#define	MB_UNLOCK_CONT(mb_cnt)		mtx_unlock((mb_cnt)->mb_cont.mc_lock)

#define	MB_GET_PCPU_LIST_NUM(mb_lst, num)				\
    (mb_lst)->ml_cntlst[(num)]

#define	MB_BUCKET_INDX(mb_obj, mb_lst)					\
    (int)(((caddr_t)(mb_obj) - (caddr_t)(mb_lst)->ml_mapbase) / PAGE_SIZE)

#define	MB_GET_OBJECT(mb_objp, mb_bckt, mb_lst)				\
{									\
	struct mc_buckethd *_mchd = &((mb_lst)->mb_cont.mc_bhead);	\
									\
	(mb_bckt)->mb_numfree--;					\
	(mb_objp) = (mb_bckt)->mb_free[((mb_bckt)->mb_numfree)];	\
	(*((mb_lst)->mb_cont.mc_objcount))--;				\
	if ((mb_bckt)->mb_numfree == 0) {				\
		SLIST_REMOVE_HEAD(_mchd, mb_blist);			\
		SLIST_NEXT((mb_bckt), mb_blist) = NULL;			\
		(mb_bckt)->mb_owner |= MB_BUCKET_FREE;			\
	}								\
}

#define	MB_PUT_OBJECT(mb_objp, mb_bckt, mb_lst)				\
	(mb_bckt)->mb_free[((mb_bckt)->mb_numfree)] = (mb_objp);	\
	(mb_bckt)->mb_numfree++;					\
	(*((mb_lst)->mb_cont.mc_objcount))++;

#define	MB_MBTYPES_INC(mb_cnt, mb_type, mb_num)				\
	if ((mb_type) != MT_NOTMBUF)					\
	    (*((mb_cnt)->mb_cont.mc_types + (mb_type))) += (mb_num)

#define	MB_MBTYPES_DEC(mb_cnt, mb_type, mb_num)				\
	if ((mb_type) != MT_NOTMBUF)					\
	    (*((mb_cnt)->mb_cont.mc_types + (mb_type))) -= (mb_num)

/*
 * Ownership of buckets/containers is represented by integers.  The PCPU
 * lists range from 0 to NCPU-1.  We need a free numerical id for the general
 * list (we use NCPU).  We also need a non-conflicting free bit to indicate
 * that the bucket is free and removed from a container, while not losing
 * the bucket's originating container id.  We use the highest bit
 * for the free marker.
 */
#define	MB_GENLIST_OWNER	(NCPU)
#define	MB_BUCKET_FREE		(1 << (sizeof(int) * 8 - 1))

/* Statistics structures for allocator (per-CPU and general). */
static struct mbpstat mb_statpcpu[NCPU + 1];
struct mbstat mbstat;

/* Sleep time for wait code (in ticks). */
static int mbuf_wait = 64;

static u_int mbuf_limit = 512;	/* Upper limit on # of mbufs per CPU. */
static u_int clust_limit = 128;	/* Upper limit on # of clusters per CPU. */

/*
 * Objects exported by sysctl(8).
 */
SYSCTL_DECL(_kern_ipc);
SYSCTL_INT(_kern_ipc, OID_AUTO, nmbclusters, CTLFLAG_RD, &nmbclusters, 0, 
    "Maximum number of mbuf clusters available");
SYSCTL_INT(_kern_ipc, OID_AUTO, nmbufs, CTLFLAG_RD, &nmbufs, 0,
    "Maximum number of mbufs available"); 
SYSCTL_INT(_kern_ipc, OID_AUTO, nmbcnt, CTLFLAG_RD, &nmbcnt, 0,
    "Number used to scale kmem_map to ensure sufficient space for counters");
SYSCTL_INT(_kern_ipc, OID_AUTO, nsfbufs, CTLFLAG_RD, &nsfbufs, 0,
    "Maximum number of sendfile(2) sf_bufs available");
SYSCTL_INT(_kern_ipc, OID_AUTO, mbuf_wait, CTLFLAG_RW, &mbuf_wait, 0,
    "Sleep time of mbuf subsystem wait allocations during exhaustion");
SYSCTL_UINT(_kern_ipc, OID_AUTO, mbuf_limit, CTLFLAG_RW, &mbuf_limit, 0,
    "Upper limit of number of mbufs allowed on each PCPU list");
SYSCTL_UINT(_kern_ipc, OID_AUTO, clust_limit, CTLFLAG_RW, &clust_limit, 0,
    "Upper limit of number of mbuf clusters allowed on each PCPU list");
SYSCTL_STRUCT(_kern_ipc, OID_AUTO, mbstat, CTLFLAG_RD, &mbstat, mbstat,
    "Mbuf general information and statistics");
SYSCTL_OPAQUE(_kern_ipc, OID_AUTO, mb_statpcpu, CTLFLAG_RD, mb_statpcpu,
    sizeof(mb_statpcpu), "S,", "Mbuf allocator per CPU statistics");

/*
 * Prototypes of local allocator routines.
 */
static void		*mb_alloc_wait(struct mb_lstmngr *, short);
static struct mb_bucket	*mb_pop_cont(struct mb_lstmngr *, int,
			    struct mb_pcpu_list *);
static void		 mb_reclaim(void);
static void		 mbuf_init(void *);

/*
 * Initial allocation numbers.  Each parameter represents the number of buckets
 * of each object that will be placed initially in each PCPU container for
 * said object.
 */
#define	NMB_MBUF_INIT	4
#define	NMB_CLUST_INIT	16

/*
 * Internal flags that allow for cache locks to remain "persistent" across
 * allocation and free calls.  They may be used in combination.
 */
#define	MBP_PERSIST	0x1	/* Return with lock still held. */
#define	MBP_PERSISTENT	0x2	/* Cache lock is already held coming in. */

/*
 * Initialize the mbuf subsystem.
 *
 * We sub-divide the kmem_map into several submaps; this way, we don't have
 * to worry about artificially limiting the number of mbuf or mbuf cluster
 * allocations, due to fear of one type of allocation "stealing" address
 * space initially reserved for another.
 *
 * Set up both the general containers and all the PCPU containers.  Populate
 * the PCPU containers with initial numbers.
 */
MALLOC_DEFINE(M_MBUF, "mbufmgr", "mbuf subsystem management structures");
SYSINIT(mbuf, SI_SUB_MBUF, SI_ORDER_FIRST, mbuf_init, NULL)
void
mbuf_init(void *dummy)
{
	struct mb_pcpu_list *pcpu_cnt;
	vm_size_t mb_map_size;
	int i, j;

	/*
	 * Set up all the submaps, for each type of object that we deal
	 * with in this allocator.  We also allocate space for the cluster
	 * ref. counts in the mbuf map (and not the cluster map) in order to
	 * give clusters a nice contiguous address space without any holes.
	 */
	mb_map_size = (vm_size_t)(nmbufs * MSIZE + nmbclusters * sizeof(u_int));
	mb_map_size = rounddown(mb_map_size, PAGE_SIZE);
	mb_list_mbuf.ml_btable = malloc((unsigned long)mb_map_size / PAGE_SIZE *
	    sizeof(struct mb_bucket *), M_MBUF, M_NOWAIT);
	if (mb_list_mbuf.ml_btable == NULL)
		goto bad;
	mb_list_mbuf.ml_map = kmem_suballoc(kmem_map,&(mb_list_mbuf.ml_mapbase),
	    &(mb_list_mbuf.ml_maptop), mb_map_size);
	mb_list_mbuf.ml_map->system_map = 1;
	mb_list_mbuf.ml_mapfull = 0;
	mb_list_mbuf.ml_objsize = MSIZE;
	mb_list_mbuf.ml_wmhigh = &mbuf_limit;

	mb_map_size = (vm_size_t)(nmbclusters * MCLBYTES);
	mb_map_size = rounddown(mb_map_size, PAGE_SIZE);
	mb_list_clust.ml_btable = malloc((unsigned long)mb_map_size / PAGE_SIZE
	    * sizeof(struct mb_bucket *), M_MBUF, M_NOWAIT);
	if (mb_list_clust.ml_btable == NULL)
		goto bad;
	mb_list_clust.ml_map = kmem_suballoc(kmem_map,
	    &(mb_list_clust.ml_mapbase), &(mb_list_clust.ml_maptop),
	    mb_map_size);
	mb_list_clust.ml_map->system_map = 1;
	mb_list_clust.ml_mapfull = 0;
	mb_list_clust.ml_objsize = MCLBYTES;
	mb_list_clust.ml_wmhigh = &clust_limit;

	/*
	 * Allocate required general (global) containers for each object type.
	 */
	mb_list_mbuf.ml_genlist = malloc(sizeof(struct mb_gen_list), M_MBUF,
	    M_NOWAIT);
	mb_list_clust.ml_genlist = malloc(sizeof(struct mb_gen_list), M_MBUF,
	    M_NOWAIT);
	if ((mb_list_mbuf.ml_genlist == NULL) ||
	    (mb_list_clust.ml_genlist == NULL))
		goto bad;

	/*
	 * Initialize condition variables and general container mutex locks.
	 */
	mtx_init(&mbuf_gen, "mbuf subsystem general lists lock", NULL, 0);
	cv_init(&(mb_list_mbuf.ml_genlist->mgl_mstarved), "mbuf pool starved");
	cv_init(&(mb_list_clust.ml_genlist->mgl_mstarved),
	    "mcluster pool starved");
	mb_list_mbuf.ml_genlist->mb_cont.mc_lock =
	    mb_list_clust.ml_genlist->mb_cont.mc_lock = &mbuf_gen;

	/*
	 * Set up the general containers for each object.
	 */
	mb_list_mbuf.ml_genlist->mb_cont.mc_numowner =
	    mb_list_clust.ml_genlist->mb_cont.mc_numowner = MB_GENLIST_OWNER;
	mb_list_mbuf.ml_genlist->mb_cont.mc_starved =
	    mb_list_clust.ml_genlist->mb_cont.mc_starved = 0;
	mb_list_mbuf.ml_genlist->mb_cont.mc_objcount =
	    &(mb_statpcpu[MB_GENLIST_OWNER].mb_mbfree);
	mb_list_clust.ml_genlist->mb_cont.mc_objcount =
	    &(mb_statpcpu[MB_GENLIST_OWNER].mb_clfree);
	mb_list_mbuf.ml_genlist->mb_cont.mc_numpgs =
	    &(mb_statpcpu[MB_GENLIST_OWNER].mb_mbpgs);
	mb_list_clust.ml_genlist->mb_cont.mc_numpgs =
	    &(mb_statpcpu[MB_GENLIST_OWNER].mb_clpgs);
	mb_list_mbuf.ml_genlist->mb_cont.mc_types =
	    &(mb_statpcpu[MB_GENLIST_OWNER].mb_mbtypes[0]);
	mb_list_clust.ml_genlist->mb_cont.mc_types = NULL;
	SLIST_INIT(&(mb_list_mbuf.ml_genlist->mb_cont.mc_bhead));
	SLIST_INIT(&(mb_list_clust.ml_genlist->mb_cont.mc_bhead));

	/*
	 * Allocate all the required counters for clusters.  This makes
	 * cluster allocations/deallocations much faster.
	 */
	cl_refcntmap = (u_int *)kmem_malloc(mb_list_clust.ml_map,
	    roundup(nmbclusters * sizeof(u_int), MSIZE), M_NOWAIT);
	if (cl_refcntmap == NULL)
		goto bad;

	/*
	 * Initialize general mbuf statistics.
	 */
	mbstat.m_msize = MSIZE;
	mbstat.m_mclbytes = MCLBYTES;
	mbstat.m_minclsize = MINCLSIZE;
	mbstat.m_mlen = MLEN;
	mbstat.m_mhlen = MHLEN;
	mbstat.m_numtypes = MT_NTYPES;

	/*
	 * Allocate and initialize PCPU containers.
	 */
	for (i = 0; i < NCPU; i++) {
		if (CPU_ABSENT(i))
			continue;

		mb_list_mbuf.ml_cntlst[i] = malloc(sizeof(struct mb_pcpu_list),
		    M_MBUF, M_NOWAIT);
		mb_list_clust.ml_cntlst[i] = malloc(sizeof(struct mb_pcpu_list),
		    M_MBUF, M_NOWAIT);
		if ((mb_list_mbuf.ml_cntlst[i] == NULL) ||
		    (mb_list_clust.ml_cntlst[i] == NULL))
			goto bad;

		mtx_init(&mbuf_pcpu[i], "mbuf PCPU list lock", NULL, 0);
		mb_list_mbuf.ml_cntlst[i]->mb_cont.mc_lock =
		    mb_list_clust.ml_cntlst[i]->mb_cont.mc_lock = &mbuf_pcpu[i];

		mb_statpcpu[i].mb_active = 1;
		mb_list_mbuf.ml_cntlst[i]->mb_cont.mc_numowner =
		    mb_list_clust.ml_cntlst[i]->mb_cont.mc_numowner = i;
		mb_list_mbuf.ml_cntlst[i]->mb_cont.mc_starved =
		    mb_list_clust.ml_cntlst[i]->mb_cont.mc_starved = 0;
		mb_list_mbuf.ml_cntlst[i]->mb_cont.mc_objcount =
		    &(mb_statpcpu[i].mb_mbfree);
		mb_list_clust.ml_cntlst[i]->mb_cont.mc_objcount =
		    &(mb_statpcpu[i].mb_clfree);
		mb_list_mbuf.ml_cntlst[i]->mb_cont.mc_numpgs =
		    &(mb_statpcpu[i].mb_mbpgs);
		mb_list_clust.ml_cntlst[i]->mb_cont.mc_numpgs =
		    &(mb_statpcpu[i].mb_clpgs);
		mb_list_mbuf.ml_cntlst[i]->mb_cont.mc_types =
		    &(mb_statpcpu[i].mb_mbtypes[0]);
		mb_list_clust.ml_cntlst[i]->mb_cont.mc_types = NULL;

		SLIST_INIT(&(mb_list_mbuf.ml_cntlst[i]->mb_cont.mc_bhead));
		SLIST_INIT(&(mb_list_clust.ml_cntlst[i]->mb_cont.mc_bhead));

		/*
		 * Perform initial allocations.
		 */
		pcpu_cnt = MB_GET_PCPU_LIST_NUM(&mb_list_mbuf, i);
		MB_LOCK_CONT(pcpu_cnt);
		for (j = 0; j < NMB_MBUF_INIT; j++) {
			if (mb_pop_cont(&mb_list_mbuf, M_DONTWAIT, pcpu_cnt)
			    == NULL)
				goto bad;
		}
		MB_UNLOCK_CONT(pcpu_cnt);

		pcpu_cnt = MB_GET_PCPU_LIST_NUM(&mb_list_clust, i);
		MB_LOCK_CONT(pcpu_cnt);
		for (j = 0; j < NMB_CLUST_INIT; j++) {
			if (mb_pop_cont(&mb_list_clust, M_DONTWAIT, pcpu_cnt)
			    == NULL)
				goto bad;
		}
		MB_UNLOCK_CONT(pcpu_cnt);
	}

	return;
bad:
	panic("mbuf_init(): failed to initialize mbuf subsystem!");
}

/*
 * Populate a given mbuf PCPU container with a bucket full of fresh new
 * buffers.  Return a pointer to the new bucket (already in the container if
 * successful), or return NULL on failure.
 *
 * LOCKING NOTES:
 * PCPU container lock must be held when this is called.
 * The lock is dropped here so that we can cleanly call the underlying VM
 * code.  If we fail, we return with no locks held. If we succeed (i.e., return
 * non-NULL), we return with the PCPU lock held, ready for allocation from
 * the returned bucket.
 */
static struct mb_bucket *
mb_pop_cont(struct mb_lstmngr *mb_list, int how, struct mb_pcpu_list *cnt_lst)
{
	struct mb_bucket *bucket;
	caddr_t p;
	int i;

	MB_UNLOCK_CONT(cnt_lst);
	/*
	 * If our object's (finite) map is starved now (i.e., no more address
	 * space), bail out now.
	 */
	if (mb_list->ml_mapfull)
		return (NULL);

	bucket = malloc(sizeof(struct mb_bucket) +
	    PAGE_SIZE / mb_list->ml_objsize * sizeof(void *), M_MBUF,
	    how == M_TRYWAIT ? M_WAITOK : M_NOWAIT);
	if (bucket == NULL)
		return (NULL);

	p = (caddr_t)kmem_malloc(mb_list->ml_map, PAGE_SIZE,
	    how == M_TRYWAIT ? M_WAITOK : M_NOWAIT);
	if (p == NULL) {
		free(bucket, M_MBUF);
		if (how == M_TRYWAIT)
			mb_list->ml_mapfull = 1;
		return (NULL);
	}

	bucket->mb_numfree = 0;
	mb_list->ml_btable[MB_BUCKET_INDX(p, mb_list)] = bucket;
	for (i = 0; i < (PAGE_SIZE / mb_list->ml_objsize); i++) {
		bucket->mb_free[i] = p;
		bucket->mb_numfree++;
		p += mb_list->ml_objsize;
	}

	MB_LOCK_CONT(cnt_lst);
	bucket->mb_owner = cnt_lst->mb_cont.mc_numowner;
	SLIST_INSERT_HEAD(&(cnt_lst->mb_cont.mc_bhead), bucket, mb_blist);
	(*(cnt_lst->mb_cont.mc_numpgs))++;
	*(cnt_lst->mb_cont.mc_objcount) += bucket->mb_numfree;

	return (bucket);
}

/*
 * Allocate an mbuf-subsystem type object.
 * The general case is very easy.  Complications only arise if our PCPU
 * container is empty.  Things get worse if the PCPU container is empty,
 * the general container is empty, and we've run out of address space
 * in our map; then we try to block if we're willing to (M_TRYWAIT).
 */
static __inline
void *
mb_alloc(struct mb_lstmngr *mb_list, int how, short type, short persist, 
	 int *pers_list)
{
	static int last_report;
	struct mb_pcpu_list *cnt_lst;
	struct mb_bucket *bucket;
	void *m;

	m = NULL;
	if ((persist & MBP_PERSISTENT) != 0) {
		/*
		 * If we're a "persistent" call, then the per-CPU #(pers_list)
		 * cache lock is already held, and we just need to refer to
		 * the correct cache descriptor.
		 */
		cnt_lst = MB_GET_PCPU_LIST_NUM(mb_list, *pers_list);
	} else {
		cnt_lst = MB_GET_PCPU_LIST(mb_list);
		MB_LOCK_CONT(cnt_lst);
	}

	if ((bucket = SLIST_FIRST(&(cnt_lst->mb_cont.mc_bhead))) != NULL) {
		/*
		 * This is the easy allocation case. We just grab an object
		 * from a bucket in the PCPU container. At worst, we
		 * have just emptied the bucket and so we remove it
		 * from the container.
		 */
		MB_GET_OBJECT(m, bucket, cnt_lst);
		MB_MBTYPES_INC(cnt_lst, type, 1);

		/* If asked to persist, do not drop the lock. */
		if ((persist & MBP_PERSIST) == 0)
			MB_UNLOCK_CONT(cnt_lst);
		else
			*pers_list = cnt_lst->mb_cont.mc_numowner;
	} else {
		struct mb_gen_list *gen_list;

		/*
		 * This is the less-common more difficult case. We must
		 * first verify if the general list has anything for us
		 * and if that also fails, we must allocate a page from
		 * the map and create a new bucket to place in our PCPU
		 * container (already locked). If the map is starved then
		 * we're really in for trouble, as we have to wait on
		 * the general container's condition variable.
		 */
		gen_list = MB_GET_GEN_LIST(mb_list);
		MB_LOCK_CONT(gen_list);

		if ((bucket = SLIST_FIRST(&(gen_list->mb_cont.mc_bhead)))
		    != NULL) {
			/*
			 * Give ownership of the bucket to our CPU's
			 * container, but only actually put the bucket
			 * in the container if it doesn't become free
			 * upon removing an mbuf from it.
			 */
			SLIST_REMOVE_HEAD(&(gen_list->mb_cont.mc_bhead),
			    mb_blist);
			bucket->mb_owner = cnt_lst->mb_cont.mc_numowner;
			(*(gen_list->mb_cont.mc_numpgs))--;
			(*(cnt_lst->mb_cont.mc_numpgs))++;
			*(gen_list->mb_cont.mc_objcount) -= bucket->mb_numfree;
			bucket->mb_numfree--;
			m = bucket->mb_free[(bucket->mb_numfree)];
			if (bucket->mb_numfree == 0) {
				SLIST_NEXT(bucket, mb_blist) = NULL;
				bucket->mb_owner |= MB_BUCKET_FREE;
			} else {
				SLIST_INSERT_HEAD(&(cnt_lst->mb_cont.mc_bhead),
				     bucket, mb_blist);
				*(cnt_lst->mb_cont.mc_objcount) +=
				    bucket->mb_numfree;
			}
			MB_UNLOCK_CONT(gen_list);
			MB_MBTYPES_INC(cnt_lst, type, 1);

			/* If asked to persist, do not drop the lock. */
			if ((persist & MBP_PERSIST) == 0)
				MB_UNLOCK_CONT(cnt_lst);
			else
				*pers_list = cnt_lst->mb_cont.mc_numowner;
		} else {
			/*
			 * We'll have to allocate a new page.
			 */
			MB_UNLOCK_CONT(gen_list);
			bucket = mb_pop_cont(mb_list, how, cnt_lst);
			if (bucket != NULL) {
				MB_GET_OBJECT(m, bucket, cnt_lst);
				MB_MBTYPES_INC(cnt_lst, type, 1);

				/* If asked to persist, do not drop the lock. */
				if ((persist & MBP_PERSIST) == 0)
					MB_UNLOCK_CONT(cnt_lst);
				else
					*pers_list=cnt_lst->mb_cont.mc_numowner;
			} else {
				if (how == M_TRYWAIT) {
					/*
				 	 * Absolute worst-case scenario.
					 * We block if we're willing to, but
					 * only after trying to steal from
					 * other lists.
					 */
					m = mb_alloc_wait(mb_list, type);
				} else {
					/* XXX: No consistency. */
					mbstat.m_drops++;

					if (ticks < last_report ||
					   (ticks - last_report) >= hz) {
						last_report = ticks;
						printf(
"All mbufs or mbuf clusters exhausted, please see tuning(7).\n");
					}

				}
				if (m != NULL && (persist & MBP_PERSIST) != 0) {
					cnt_lst = MB_GET_PCPU_LIST(mb_list);
					MB_LOCK_CONT(cnt_lst);
					*pers_list=cnt_lst->mb_cont.mc_numowner;
				}
			}
		}
	}

	return (m);
}

/*
 * This is the worst-case scenario called only if we're allocating with
 * M_TRYWAIT.  We first drain all the protocols, then try to find an mbuf
 * by looking in every PCPU container.  If we're still unsuccesful, we
 * try the general container one last time and possibly block on our
 * starved cv.
 */
static void *
mb_alloc_wait(struct mb_lstmngr *mb_list, short type)
{
	struct mb_pcpu_list *cnt_lst;
	struct mb_gen_list *gen_list;
	struct mb_bucket *bucket;
	void *m;
	int i, cv_ret;

	/*
	 * Try to reclaim mbuf-related objects (mbufs, clusters).
	 */
	mb_reclaim();

	/*
	 * Cycle all the PCPU containers. Increment starved counts if found
	 * empty.
	 */
	for (i = 0; i < NCPU; i++) {
		if (CPU_ABSENT(i))
			continue;
		cnt_lst = MB_GET_PCPU_LIST_NUM(mb_list, i);
		MB_LOCK_CONT(cnt_lst);

		/*
		 * If container is non-empty, get a single object from it.
		 * If empty, increment starved count.
		 */
		if ((bucket = SLIST_FIRST(&(cnt_lst->mb_cont.mc_bhead))) !=
		    NULL) {
			MB_GET_OBJECT(m, bucket, cnt_lst);
			MB_MBTYPES_INC(cnt_lst, type, 1);
			MB_UNLOCK_CONT(cnt_lst);
			mbstat.m_wait++;	/* XXX: No consistency. */
			return (m);
		} else
			cnt_lst->mb_cont.mc_starved++;

		MB_UNLOCK_CONT(cnt_lst);
	}

	/*
	 * We're still here, so that means it's time to get the general
	 * container lock, check it one more time (now that mb_reclaim()
	 * has been called) and if we still get nothing, block on the cv.
	 */
	gen_list = MB_GET_GEN_LIST(mb_list);
	MB_LOCK_CONT(gen_list);
	if ((bucket = SLIST_FIRST(&(gen_list->mb_cont.mc_bhead))) != NULL) {
		MB_GET_OBJECT(m, bucket, gen_list);
		MB_MBTYPES_INC(gen_list, type, 1);
		MB_UNLOCK_CONT(gen_list);
		mbstat.m_wait++;	/* XXX: No consistency. */
		return (m);
	}

	gen_list->mb_cont.mc_starved++;
	cv_ret = cv_timedwait(&(gen_list->mgl_mstarved),
	    gen_list->mb_cont.mc_lock, mbuf_wait);
	gen_list->mb_cont.mc_starved--;

	if ((cv_ret == 0) &&
	    ((bucket = SLIST_FIRST(&(gen_list->mb_cont.mc_bhead))) != NULL)) {
		MB_GET_OBJECT(m, bucket, gen_list);
		MB_MBTYPES_INC(gen_list, type, 1);
		mbstat.m_wait++;	/* XXX: No consistency. */
	} else {
		mbstat.m_drops++;	/* XXX: No consistency. */
		m = NULL;
	}

	MB_UNLOCK_CONT(gen_list);

	return (m);
}

/*-
 * Free an object to its rightful container.
 * In the very general case, this operation is really very easy.
 * Complications arise primarily if:
 *	(a) We've hit the high limit on number of free objects allowed in
 *	    our PCPU container.
 *	(b) We're in a critical situation where our container has been
 *	    marked 'starved' and we need to issue wakeups on the starved
 *	    condition variable.
 *	(c) Minor (odd) cases: our bucket has migrated while we were
 *	    waiting for the lock; our bucket is in the general container;
 *	    our bucket is empty.
 */
static __inline
void
mb_free(struct mb_lstmngr *mb_list, void *m, short type, short persist,
	int *pers_list)
{
	struct mb_pcpu_list *cnt_lst;
	struct mb_gen_list *gen_list;
	struct mb_bucket *bucket;
	u_int owner;

	bucket = mb_list->ml_btable[MB_BUCKET_INDX(m, mb_list)];

	/*
	 * Make sure that if after we lock the bucket's present container the
	 * bucket has migrated, that we drop the lock and get the new one.
	 */
retry_lock:
	owner = bucket->mb_owner & ~MB_BUCKET_FREE;
	switch (owner) {
	case MB_GENLIST_OWNER:
		gen_list = MB_GET_GEN_LIST(mb_list);
		if (((persist & MBP_PERSISTENT) != 0) && (*pers_list >= 0)) {
			if (*pers_list != MB_GENLIST_OWNER) {
				cnt_lst = MB_GET_PCPU_LIST_NUM(mb_list,
				    *pers_list);
				MB_UNLOCK_CONT(cnt_lst);
				MB_LOCK_CONT(gen_list);
			}
		} else {
			MB_LOCK_CONT(gen_list);
		}
		if (owner != (bucket->mb_owner & ~MB_BUCKET_FREE)) {
			MB_UNLOCK_CONT(gen_list);
			*pers_list = -1;
			goto retry_lock;
		}

		/*
		 * If we're intended for the general container, this is
		 * real easy: no migrating required. The only `bogon'
		 * is that we're now contending with all the threads
		 * dealing with the general list, but this is expected.
		 */
		MB_PUT_OBJECT(m, bucket, gen_list);
		MB_MBTYPES_DEC(gen_list, type, 1);
		if (gen_list->mb_cont.mc_starved > 0)
			cv_signal(&(gen_list->mgl_mstarved));
		if ((persist & MBP_PERSIST) == 0)
			MB_UNLOCK_CONT(gen_list);
		else
			*pers_list = MB_GENLIST_OWNER;
		break;

	default:
		cnt_lst = MB_GET_PCPU_LIST_NUM(mb_list, owner);
		if (((persist & MBP_PERSISTENT) != 0) && (*pers_list >= 0)) {
			if (*pers_list == MB_GENLIST_OWNER) {
				gen_list = MB_GET_GEN_LIST(mb_list);
				MB_UNLOCK_CONT(gen_list);
				MB_LOCK_CONT(cnt_lst);
			} else {
				cnt_lst = MB_GET_PCPU_LIST_NUM(mb_list,
				    *pers_list);
				owner = *pers_list;
			}
		} else {
			MB_LOCK_CONT(cnt_lst);
		}
		if (owner != (bucket->mb_owner & ~MB_BUCKET_FREE)) {
			MB_UNLOCK_CONT(cnt_lst);
			*pers_list = -1;
			goto retry_lock;
		}

		MB_PUT_OBJECT(m, bucket, cnt_lst);
		MB_MBTYPES_DEC(cnt_lst, type, 1);

		if (cnt_lst->mb_cont.mc_starved > 0) {
			/*
			 * This is a tough case. It means that we've
			 * been flagged at least once to indicate that
			 * we're empty, and that the system is in a critical
			 * situation, so we ought to migrate at least one
			 * bucket over to the general container.
			 * There may or may not be a thread blocking on
			 * the starved condition variable, but chances
			 * are that one will eventually come up soon so
			 * it's better to migrate now than never.
			 */
			gen_list = MB_GET_GEN_LIST(mb_list);
			MB_LOCK_CONT(gen_list);
			KASSERT((bucket->mb_owner & MB_BUCKET_FREE) != 0,
			    ("mb_free: corrupt bucket %p\n", bucket));
			SLIST_INSERT_HEAD(&(gen_list->mb_cont.mc_bhead),
			    bucket, mb_blist);
			bucket->mb_owner = MB_GENLIST_OWNER;
			(*(cnt_lst->mb_cont.mc_objcount))--;
			(*(gen_list->mb_cont.mc_objcount))++;
			(*(cnt_lst->mb_cont.mc_numpgs))--;
			(*(gen_list->mb_cont.mc_numpgs))++;

			/*
			 * Determine whether or not to keep transferring
			 * buckets to the general list or whether we've
			 * transferred enough already.
			 * We realize that although we may flag another
			 * bucket to be migrated to the general container
			 * that in the meantime, the thread that was
			 * blocked on the cv is already woken up and
			 * long gone. But in that case, the worst
			 * consequence is that we will end up migrating
			 * one bucket too many, which is really not a big
			 * deal, especially if we're close to a critical
			 * situation.
			 */
			if (gen_list->mb_cont.mc_starved > 0) {
				cnt_lst->mb_cont.mc_starved--;
				cv_signal(&(gen_list->mgl_mstarved));
			} else
				cnt_lst->mb_cont.mc_starved = 0;

			MB_UNLOCK_CONT(gen_list);
			if ((persist & MBP_PERSIST) == 0)
				MB_UNLOCK_CONT(cnt_lst);
			else
				*pers_list = owner;
			break;
		}

		if (*(cnt_lst->mb_cont.mc_objcount) > *(mb_list->ml_wmhigh)) {
			/*
			 * We've hit the high limit of allowed numbers of mbufs
			 * on this PCPU list. We must now migrate a bucket
			 * over to the general container.
			 */
			gen_list = MB_GET_GEN_LIST(mb_list);
			MB_LOCK_CONT(gen_list);
			if ((bucket->mb_owner & MB_BUCKET_FREE) == 0) {
				bucket =
				    SLIST_FIRST(&(cnt_lst->mb_cont.mc_bhead));
				SLIST_REMOVE_HEAD(&(cnt_lst->mb_cont.mc_bhead),
				    mb_blist);
			}
			SLIST_INSERT_HEAD(&(gen_list->mb_cont.mc_bhead),
			    bucket, mb_blist);
			bucket->mb_owner = MB_GENLIST_OWNER;
			*(cnt_lst->mb_cont.mc_objcount) -= bucket->mb_numfree;
			*(gen_list->mb_cont.mc_objcount) += bucket->mb_numfree;
			(*(cnt_lst->mb_cont.mc_numpgs))--;
			(*(gen_list->mb_cont.mc_numpgs))++;

			/*
			 * While we're at it, transfer some of the mbtypes
			 * "count load" onto the general list's mbtypes
			 * array, seeing as how we're moving the bucket
			 * there now, meaning that the freeing of objects
			 * there will now decrement the _general list's_
			 * mbtypes counters, and no longer our PCPU list's
			 * mbtypes counters. We do this for the type presently
			 * being freed in an effort to keep the mbtypes
			 * counters approximately balanced across all lists.
			 */ 
			MB_MBTYPES_DEC(cnt_lst, type, (PAGE_SIZE /
			    mb_list->ml_objsize) - bucket->mb_numfree);
			MB_MBTYPES_INC(gen_list, type, (PAGE_SIZE /
			    mb_list->ml_objsize) - bucket->mb_numfree);
 
			MB_UNLOCK_CONT(gen_list);
			if ((persist & MBP_PERSIST) == 0)
				MB_UNLOCK_CONT(cnt_lst);
			else
				*pers_list = owner;
			break;
		}

		if (bucket->mb_owner & MB_BUCKET_FREE) {
			SLIST_INSERT_HEAD(&(cnt_lst->mb_cont.mc_bhead),
			    bucket, mb_blist);
			bucket->mb_owner = cnt_lst->mb_cont.mc_numowner;
		}

		if ((persist & MBP_PERSIST) == 0)
			MB_UNLOCK_CONT(cnt_lst);
		else
			*pers_list = owner;
		break;
	}
}

/*
 * Drain protocols in hopes to free up some resources.
 *
 * LOCKING NOTES:
 * No locks should be held when this is called.  The drain routines have to
 * presently acquire some locks which raises the possibility of lock order
 * violation if we're holding any mutex if that mutex is acquired in reverse
 * order relative to one of the locks in the drain routines.
 */
static void
mb_reclaim(void)
{
	struct domain *dp;
	struct protosw *pr;

/*
 * XXX: Argh, we almost always trip here with witness turned on now-a-days
 * XXX: because we often come in with Giant held. For now, there's no way
 * XXX: to avoid this.
 */
#ifdef WITNESS
	KASSERT(witness_list(curthread) == 0,
	    ("mb_reclaim() called with locks held"));
#endif

	mbstat.m_drain++;	/* XXX: No consistency. */

	for (dp = domains; dp != NULL; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_drain != NULL)
				(*pr->pr_drain)();
}

/******************************************************************************
 * Internal setup macros.
 */

#define	_mb_setup(m, type) do {						\
	(m)->m_type = (type);						\
	(m)->m_next = NULL;						\
	(m)->m_nextpkt = NULL;						\
	(m)->m_data = (m)->m_dat;					\
	(m)->m_flags = 0;						\
} while (0)

#define	_mbhdr_setup(m, type) do {					\
	(m)->m_type = (type);						\
	(m)->m_next = NULL;						\
	(m)->m_nextpkt = NULL;						\
	(m)->m_data = (m)->m_pktdat;					\
	(m)->m_flags = M_PKTHDR;					\
	(m)->m_pkthdr.rcvif = NULL;					\
	(m)->m_pkthdr.csum_flags = 0;					\
	(m)->m_pkthdr.aux = NULL;					\
} while (0)

#define _mcl_setup(m) do {						\
	(m)->m_data = (m)->m_ext.ext_buf;				\
	(m)->m_flags |= M_EXT;						\
	(m)->m_ext.ext_free = NULL;					\
	(m)->m_ext.ext_args = NULL;					\
	(m)->m_ext.ext_size = MCLBYTES;					\
	(m)->m_ext.ext_type = EXT_CLUSTER;				\
} while (0)

#define	_mext_init_ref(m, ref) do {					\
	(m)->m_ext.ref_cnt = ((ref) == NULL) ?				\
	    malloc(sizeof(u_int), M_MBUF, M_NOWAIT) : (u_int *)(ref);	\
	if ((m)->m_ext.ref_cnt != NULL) {				\
		*((m)->m_ext.ref_cnt) = 0;				\
		MEXT_ADD_REF((m));					\
	}								\
} while (0)

#define	cl2ref(cl)							\
    (((uintptr_t)(cl) - (uintptr_t)cl_refcntmap) >> MCLSHIFT)

#define	_mext_dealloc_ref(m)						\
	free((m)->m_ext.ref_cnt, M_MBUF)

/******************************************************************************
 * Internal routines.
 * 
 * Because mb_alloc() and mb_free() are inlines (to keep the common
 * cases down to a maximum of one function call), below are a few
 * routines used only internally for the sole purpose of making certain
 * functions smaller.
 *
 * - _mext_free(): frees associated storage when the ref. count is
 *   exactly one and we're freeing.
 *
 * - _mgetm_internal(): common "persistent-lock" routine that allocates
 *   an mbuf and a cluster in one shot, but where the lock is already
 *   held coming in (which is what makes it different from the exported
 *   m_getcl()).  The lock is dropped when done.  This is used by m_getm()
 *   and, therefore, is very m_getm()-specific.
 */
static struct mbuf *_mgetm_internal(int, short, short, int);

void
_mext_free(struct mbuf *mb)
{

	if (mb->m_ext.ext_type == EXT_CLUSTER) {
		mb_free(&mb_list_clust, (caddr_t)mb->m_ext.ext_buf, MT_NOTMBUF,
		    0, NULL);
	} else {
		(*(mb->m_ext.ext_free))(mb->m_ext.ext_buf, mb->m_ext.ext_args);
		_mext_dealloc_ref(mb);
	}
}

static struct mbuf *
_mgetm_internal(int how, short type, short persist, int cchnum)
{
	struct mbuf *mb;

	mb = (struct mbuf *)mb_alloc(&mb_list_mbuf, how, type, persist,&cchnum);
	if (mb == NULL)
		return NULL;
	_mb_setup(mb, type);

	if ((persist & MBP_PERSIST) != 0) {
		mb->m_ext.ext_buf = (caddr_t)mb_alloc(&mb_list_clust,
		    how, MT_NOTMBUF, MBP_PERSISTENT, &cchnum);
		if (mb->m_ext.ext_buf == NULL) {
			(void)m_free(mb);
			mb = NULL;
		}
		_mcl_setup(mb);
		_mext_init_ref(mb, &cl_refcntmap[cl2ref(mb->m_ext.ext_buf)]);
	}
	return (mb);
}

/******************************************************************************
 * Exported buffer allocation and de-allocation routines.
 */

/*
 * Allocate and return a single (normal) mbuf.  NULL is returned on failure.
 *
 * Arguments:
 *  - how: M_TRYWAIT to try to block for kern.ipc.mbuf_wait number of ticks
 *    if really starved for memory.  M_DONTWAIT to never block.
 *  - type: the type of the mbuf being allocated.
 */
struct mbuf *
m_get(int how, short type)
{
	struct mbuf *mb;

	mb = (struct mbuf *)mb_alloc(&mb_list_mbuf, how, type, 0, NULL);
	if (mb != NULL)
		_mb_setup(mb, type);
	return (mb);
}

/*
 * Allocate a given length worth of mbufs and/or clusters (whatever fits
 * best) and return a pointer to the top of the allocated chain.  If an
 * existing mbuf chain is provided, then we will append the new chain
 * to the existing one and return the top of the provided (existing)
 * chain.  NULL is returned on failure, in which case the [optional]
 * provided chain is left untouched, and any memory already allocated
 * is freed.
 *
 * Arguments:
 *  - m: existing chain to which to append new chain (optional).
 *  - len: total length of data to append, either in mbufs or clusters
 *    (we allocate whatever combination yields the best fit).
 *  - how: M_TRYWAIT to try to block for kern.ipc.mbuf_wait number of ticks
 *    if really starved for memory.  M_DONTWAIT to never block.
 *  - type: the type of the mbuf being allocated.
 */
struct mbuf *
m_getm(struct mbuf *m, int len, int how, short type)
{
	struct mbuf *mb, *top, *cur, *mtail;
	int num, rem, cchnum;
	short persist;
	int i;

	KASSERT(len >= 0, ("m_getm(): len is < 0"));

	/* If m != NULL, we will append to the end of that chain. */
	if (m != NULL)
		for (mtail = m; mtail->m_next != NULL; mtail = mtail->m_next);
	else
		mtail = NULL;

	/*
	 * In the best-case scenario (which should be the common case
	 * unless we're in a starvation situation), we will be able to
	 * go through the allocation of all the desired mbufs and clusters
	 * here without dropping our per-CPU cache lock in between.
	 */
	num = len / MCLBYTES;
	rem = len % MCLBYTES;
	persist = 0;
	cchnum = -1;
	top = cur = NULL;
	for (i = 0; i < num; i++) {
		mb = (struct mbuf *)mb_alloc(&mb_list_mbuf, how, type,
		    MBP_PERSIST | persist, &cchnum);
		if (mb == NULL)
			goto failed;
		_mb_setup(mb, type);

		persist = (i != (num - 1) || rem > 0) ? MBP_PERSIST : 0;
		mb->m_ext.ext_buf = (caddr_t)mb_alloc(&mb_list_clust,
		    how, MT_NOTMBUF, persist | MBP_PERSISTENT, &cchnum);
		if (mb->m_ext.ext_buf == NULL) {
			(void)m_free(mb);
			goto failed;
		}
		_mcl_setup(mb);
		_mext_init_ref(mb, &cl_refcntmap[cl2ref(mb->m_ext.ext_buf)]);
		persist = MBP_PERSISTENT;

		if (cur == NULL)
			top = cur = mb;
		else
			cur->m_next = mb;
	}
	if (rem > 0) {
		if (cchnum >= 0) {
			persist = MBP_PERSISTENT;
			persist |= (rem > MINCLSIZE) ? MBP_PERSIST : 0;
			mb = _mgetm_internal(how, type, persist, cchnum);
			if (mb == NULL)
				goto failed;
		} else if (rem > MINCLSIZE) {
			mb = m_getcl(how, type, 0);
		} else {
			mb = m_get(how, type);
		}
		if (mb != NULL) {
			if (cur == NULL)
				top = mb;
			else
				cur->m_next = mb;
		} else
			goto failed;
	}

	if (mtail != NULL)
		mtail->m_next = top;
	else
		mtail = top;
	return mtail;
failed:
	if (top != NULL)
		m_freem(top);
	return NULL;
}

/*
 * Allocate and return a single M_PKTHDR mbuf.  NULL is returned on failure.
 *
 * Arguments:
 *  - how: M_TRYWAIT to try to block for kern.ipc.mbuf_wait number of ticks
 *    if really starved for memory.  M_DONTWAIT to never block.
 *  - type: the type of the mbuf being allocated.
 */
struct mbuf *
m_gethdr(int how, short type)
{
	struct mbuf *mb;

	mb = (struct mbuf *)mb_alloc(&mb_list_mbuf, how, type, 0, NULL);
	if (mb != NULL) {
		_mbhdr_setup(mb, type);
#ifdef MAC
		if (mac_init_mbuf(mb, how) != 0) {
			m_free(mb);
			return NULL;
		}
#endif
	}
	return (mb);
}

/*
 * Allocate and return a single (normal) pre-zero'd mbuf.  NULL is
 * returned on failure.
 *
 * Arguments:
 *  - how: M_TRYWAIT to try to block for kern.ipc.mbuf_wait number of ticks
 *    if really starved for memory.  M_DONTWAIT to never block.
 *  - type: the type of the mbuf being allocated.
 */
struct mbuf *
m_get_clrd(int how, short type)
{
	struct mbuf *mb;

	mb = (struct mbuf *)mb_alloc(&mb_list_mbuf, how, type, 0, NULL);
	if (mb != NULL) {
		_mb_setup(mb, type);
		bzero(mtod(mb, caddr_t), MLEN);
	}
	return (mb);
}

/*
 * Allocate and return a single M_PKTHDR pre-zero'd mbuf.  NULL is
 * returned on failure.
 *
 * Arguments:
 *  - how: M_TRYWAIT to try to block for kern.ipc.mbuf_wait number of ticks
 *    if really starved for memory.  M_DONTWAIT to never block.
 *  - type: the type of the mbuf being allocated.
 */
struct mbuf *
m_gethdr_clrd(int how, short type)
{
	struct mbuf *mb;

	mb = (struct mbuf *)mb_alloc(&mb_list_mbuf, how, type, 0, NULL);
	if (mb != NULL) {
		_mbhdr_setup(mb, type);
#ifdef MAC
		if (mac_init_mbuf(mb, how) != 0) {
			m_free(mb);
			return NULL;
		}
#endif
		bzero(mtod(mb, caddr_t), MHLEN);
	}
	return (mb);
}

/*
 * Free a single mbuf and any associated storage that it may have attached
 * to it.  The associated storage may not be immediately freed if its
 * reference count is above 1.  Returns the next mbuf in the chain following
 * the mbuf being freed.
 *
 * Arguments:
 *  - mb: the mbuf to free.
 */
struct mbuf *
m_free(struct mbuf *mb)
{
	struct mbuf *nb;
	int cchnum;
	short persist = 0;

	/* XXX: This check is bogus... please fix (see KAME). */
	if ((mb->m_flags & M_PKTHDR) != 0 && mb->m_pkthdr.aux) {
		m_freem(mb->m_pkthdr.aux);
		mb->m_pkthdr.aux = NULL;
	}
#ifdef MAC
	if ((mb->m_flags & M_PKTHDR) &&
	    (mb->m_pkthdr.label.l_flags & MAC_FLAG_INITIALIZED))
		mac_destroy_mbuf(mb);
#endif
	nb = mb->m_next;
	if ((mb->m_flags & M_EXT) != 0) {
		MEXT_REM_REF(mb);
		if (atomic_cmpset_int(mb->m_ext.ref_cnt, 0, 1)) {
			if (mb->m_ext.ext_type == EXT_CLUSTER) {
				mb_free(&mb_list_clust,
				    (caddr_t)mb->m_ext.ext_buf, MT_NOTMBUF,
				    MBP_PERSIST, &cchnum);
				persist = MBP_PERSISTENT;
			} else {
				(*(mb->m_ext.ext_free))(mb->m_ext.ext_buf,
				    mb->m_ext.ext_args);
				_mext_dealloc_ref(mb);
				persist = 0;
			}
		}
	}
	mb_free(&mb_list_mbuf, mb, mb->m_type, persist, &cchnum);
	return (nb);
}

/*
 * Free an entire chain of mbufs and associated external buffers, if
 * applicable.  Right now, we only optimize a little so that the cache
 * lock may be held across a single mbuf+cluster free.  Hopefully,
 * we'll eventually be holding the lock across more than merely two
 * consecutive frees but right now this is hard to implement because of
 * things like _mext_dealloc_ref (may do a free()) and atomic ops in the
 * loop, as well as the fact that we may recurse on m_freem() in
 * m_pkthdr.aux != NULL cases.
 *
 *  - mb: the mbuf chain to free.
 */
void
m_freem(struct mbuf *mb)
{
	struct mbuf *m;
	int cchnum;
	short persist;

	while (mb != NULL) {
		/* XXX: This check is bogus... please fix (see KAME). */
		if ((mb->m_flags & M_PKTHDR) != 0 && mb->m_pkthdr.aux) {
			m_freem(mb->m_pkthdr.aux);
			mb->m_pkthdr.aux = NULL;
		}
#ifdef MAC
		if ((mb->m_flags & M_PKTHDR) &&
		    (mb->m_pkthdr.label.l_flags & MAC_FLAG_INITIALIZED))
			mac_destroy_mbuf(mb);
#endif
		persist = 0;
		m = mb;
		mb = mb->m_next;
		if ((m->m_flags & M_EXT) != 0) {
			MEXT_REM_REF(m);
			if (atomic_cmpset_int(m->m_ext.ref_cnt, 0, 1)) {
				if (m->m_ext.ext_type == EXT_CLUSTER) {
					mb_free(&mb_list_clust,
					    (caddr_t)m->m_ext.ext_buf,
					    MT_NOTMBUF, MBP_PERSIST, &cchnum);
					persist = MBP_PERSISTENT;
				} else {
					(*(m->m_ext.ext_free))(m->m_ext.ext_buf,
					    m->m_ext.ext_args);
					_mext_dealloc_ref(m);
					persist = 0;
				}
			}
		}
		mb_free(&mb_list_mbuf, m, m->m_type, persist, &cchnum);
	}
}

/*
 * Fetch an mbuf with a cluster attached to it.  If one of the
 * allocations fails, the entire allocation fails.  This routine is
 * the preferred way of fetching both the mbuf and cluster together,
 * as it avoids having to unlock/relock between allocations.  Returns
 * NULL on failure. 
 *
 * Arguments:
 *  - how: M_TRYWAIT to try to block for kern.ipc.mbuf_wait number of ticks
 *    if really starved for memory.  M_DONTWAIT to never block.
 *  - type: the type of the mbuf being allocated.
 *  - flags: any flags to pass to the mbuf being allocated; if this includes
 *    the M_PKTHDR bit, then the mbuf is configured as a M_PKTHDR mbuf.
 */
struct mbuf *
m_getcl(int how, short type, int flags)
{
	struct mbuf *mb;
	int cchnum;

	mb = (struct mbuf *)mb_alloc(&mb_list_mbuf, how, type,
	    MBP_PERSIST, &cchnum);
	if (mb == NULL)
		return NULL;
	mb->m_type = type;
	mb->m_next = NULL;
	mb->m_flags = flags;
	if ((flags & M_PKTHDR) != 0) {
		mb->m_nextpkt = NULL;
		mb->m_pkthdr.rcvif = NULL;
		mb->m_pkthdr.csum_flags = 0;
		mb->m_pkthdr.aux = NULL;
	}

	mb->m_ext.ext_buf = (caddr_t)mb_alloc(&mb_list_clust, how,
	    MT_NOTMBUF, MBP_PERSISTENT, &cchnum);
	if (mb->m_ext.ext_buf == NULL) {
		(void)m_free(mb);
		mb = NULL;
	} else {
		_mcl_setup(mb);
		_mext_init_ref(mb, &cl_refcntmap[cl2ref(mb->m_ext.ext_buf)]);
	}
#ifdef MAC
	if ((flags & M_PKTHDR) && (mac_init_mbuf(mb, how) != 0)) {
		m_free(mb);
		return NULL;
	}
#endif
	return (mb);
}

/*
 * Fetch a single mbuf cluster and attach it to an existing mbuf.  If
 * successfull, configures the provided mbuf to have mbuf->m_ext.ext_buf
 * pointing to the cluster, and sets the M_EXT bit in the mbuf's flags.
 * The M_EXT bit is not set on failure.
 *
 * Arguments:
 *  - mb: the existing mbuf to which to attach the allocated cluster.
 *  - how: M_TRYWAIT to try to block for kern.ipc.mbuf_wait number of ticks
 *    if really starved for memory.  M_DONTWAIT to never block.
 */
void
m_clget(struct mbuf *mb, int how)
{

	mb->m_ext.ext_buf= (caddr_t)mb_alloc(&mb_list_clust,how,MT_NOTMBUF,
	    0, NULL);
	if (mb->m_ext.ext_buf != NULL) {
		_mcl_setup(mb);
		_mext_init_ref(mb, &cl_refcntmap[cl2ref(mb->m_ext.ext_buf)]);
	}
}

/*
 * Configure a provided mbuf to refer to the provided external storage
 * buffer and setup a reference count for said buffer.  If the setting
 * up of the reference count fails, the M_EXT bit will not be set.  If
 * successfull, the M_EXT bit is set in the mbuf's flags.
 *
 * Arguments:
 *  - mb: the existing mbuf to which to attach the provided buffer.
 *  - buf: the address of the provided external storage buffer.
 *  - size: the size of the provided buffer.
 *  - freef: a pointer to a routine that is responsible for freeing the
 *    provided external storage buffer.
 *  - args: a pointer to an argument structure (of any type) to be passed
 *    to the provided freef routine (may be NULL).
 *  - flags: any other flags to be passed to the provided mbuf.
 *  - type: the type that the external storage buffer should be labeled with.
 */
void
m_extadd(struct mbuf *mb, caddr_t buf, u_int size,
    void (*freef)(void *, void *), void *args, int flags, int type)
{

	_mext_init_ref(mb, ((type != EXT_CLUSTER) ?
	    NULL : &cl_refcntmap[cl2ref(mb->m_ext.ext_buf)]));
	if (mb->m_ext.ref_cnt != NULL) {
		mb->m_flags |= (M_EXT | flags);
		mb->m_ext.ext_buf = buf;
		mb->m_data = mb->m_ext.ext_buf;
		mb->m_ext.ext_size = size;
		mb->m_ext.ext_free = freef;
		mb->m_ext.ext_args = args;
		mb->m_ext.ext_type = type;
	}
}

/*
 * Change type of provided mbuf.  This is a relatively expensive operation
 * (due to the cost of statistics manipulations) and should be avoided, where
 * possible.
 *
 * Arguments:
 *  - mb: the provided mbuf for which the type needs to be changed.
 *  - new_type: the new type to change the mbuf to.
 */
void
m_chtype(struct mbuf *mb, short new_type)
{
	struct mb_gen_list *gen_list;

	gen_list = MB_GET_GEN_LIST(&mb_list_mbuf);
	MB_LOCK_CONT(gen_list);
	MB_MBTYPES_DEC(gen_list, mb->m_type, 1);
	MB_MBTYPES_INC(gen_list, new_type, 1);
	MB_UNLOCK_CONT(gen_list);
	mb->m_type = new_type;
}
