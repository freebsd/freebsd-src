/*
 * Copyright (c) 2001
 * 	Bosko Milekic <bmilekic@FreeBSD.org>. All rights reserved.
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

#include "opt_param.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
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

/*
 * Maximum number of PCPU containers. If you know what you're doing you could
 * explicitly define MBALLOC_NCPU to be exactly the number of CPUs on your
 * system during compilation, and thus prevent kernel structure bloat.
 *
 * SMP and non-SMP kernels clearly have a different number of possible cpus,
 * but because we cannot assume a dense array of CPUs, we always allocate
 * and traverse PCPU containers up to NCPU amount and merely check for
 * CPU availability.
 */
#ifdef	MBALLOC_NCPU
#define	NCPU	MBALLOC_NCPU
#else
#define	NCPU	MAXCPU
#endif

/*
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
 * is protected by a mutex lock in order to ensure consistency. The mutex lock
 * itself is allocated seperately and attached to the container at boot time,
 * thus allowing for certain containers to share the same mutex lock. Per-CPU
 * containers for mbufs and mbuf clusters all share the same per-CPU
 * lock whereas the "general system" containers (i.e. the "main lists") for
 * these objects share one global lock.
 *
 */
struct mb_bucket {
	SLIST_ENTRY(mb_bucket)	mb_blist;
	int 			mb_owner;
	int			mb_numfree;
	void 			*mb_free[0];
};

struct mb_container {
	SLIST_HEAD(mc_buckethd, mb_bucket)	mc_bhead;
	struct	mtx				*mc_lock;
	int					mc_numowner;
	u_int					mc_starved;
	long					*mc_types;
	u_long					*mc_objcount;
	u_long					*mc_numpgs;
};

struct mb_gen_list {
	struct	mb_container	mb_cont;
	struct	cv		mgl_mstarved;
};

struct mb_pcpu_list {
	struct	mb_container	mb_cont;
};

/*
 * Boot-time configurable object counts that will determine the maximum
 * number of permitted objects in the mbuf and mcluster cases. In the
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
#ifndef	NMBUFS
#define	NMBUFS		(nmbclusters * 2)
#endif
#ifndef	NSFBUFS
#define	NSFBUFS		(512 + maxusers * 16)
#endif
#ifndef	NMBCNTS
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

	return;
}
SYSINIT(tunable_mbinit, SI_SUB_TUNABLES, SI_ORDER_ANY, tunable_mbinit, NULL);

/*
 * The freelist structures and mutex locks. The number statically declared
 * here depends on the number of CPUs.
 *
 * We setup in such a way that all the objects (mbufs, clusters)
 * share the same mutex lock. It has been established that we do not benefit
 * from different locks for different objects, so we use the same lock,
 * regardless of object type.
 */
struct mb_lstmngr {
	struct	mb_gen_list	*ml_genlist;
	struct	mb_pcpu_list	*ml_cntlst[NCPU];
	struct	mb_bucket	**ml_btable;
	vm_map_t		ml_map;
	vm_offset_t		ml_mapbase;
	vm_offset_t		ml_maptop;
	int			ml_mapfull;
	u_int			ml_objsize;
	u_int			*ml_wmhigh;
};
struct	mb_lstmngr	mb_list_mbuf, mb_list_clust;
struct	mtx		mbuf_gen, mbuf_pcpu[NCPU];

/*
 * Local macros for internal allocator structure manipulations.
 */
#ifdef SMP
#define	MB_GET_PCPU_LIST(mb_lst)	  (mb_lst)->ml_cntlst[PCPU_GET(cpuid)]
#else
#define	MB_GET_PCPU_LIST(mb_lst)	  (mb_lst)->ml_cntlst[0]
#endif

#define	MB_GET_PCPU_LIST_NUM(mb_lst, num) (mb_lst)->ml_cntlst[(num)]

#define	MB_GET_GEN_LIST(mb_lst)		  (mb_lst)->ml_genlist

#define	MB_LOCK_CONT(mb_cnt)	 	  mtx_lock((mb_cnt)->mb_cont.mc_lock)

#define	MB_UNLOCK_CONT(mb_cnt)		  mtx_unlock((mb_cnt)->mb_cont.mc_lock)

#define	MB_BUCKET_INDX(mb_obj, mb_lst)					\
    (int)(((caddr_t)(mb_obj) - (caddr_t)(mb_lst)->ml_mapbase) / PAGE_SIZE)

#define	MB_GET_OBJECT(mb_objp, mb_bckt, mb_lst)				\
{									\
	struct	mc_buckethd	*_mchd = &((mb_lst)->mb_cont.mc_bhead);	\
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
 * Ownership of buckets/containers is represented by integers. The PCPU
 * lists range from 0 to NCPU-1. We need a free numerical id for the general
 * list (we use NCPU). We also need a non-conflicting free bit to indicate
 * that the bucket is free and removed from a container, while not losing
 * the bucket's originating container id. We use the highest bit
 * for the free marker.
 */
#define	MB_GENLIST_OWNER	(NCPU)
#define	MB_BUCKET_FREE		(1 << (sizeof(int) * 8 - 1))

/*
 * sysctl(8) exported objects
 */
struct	mbstat	mbstat;			/* General stats + infos. */
struct	mbpstat	mb_statpcpu[NCPU+1];	/* PCPU + Gen. container alloc stats */
int		mbuf_wait = 	64;	/* Sleep time for wait code (ticks) */
u_int		mbuf_limit = 	512;	/* Upper lim. on # of mbufs per CPU */
u_int		clust_limit = 	128;	/* Upper lim. on # of clusts per CPU */
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
static __inline	void	*mb_alloc(struct mb_lstmngr *, int, short);
void			*mb_alloc_wait(struct mb_lstmngr *, short);
static __inline	void	 mb_free(struct mb_lstmngr *, void *, short);
static	void		 mbuf_init(void *);
struct	mb_bucket	*mb_pop_cont(struct mb_lstmngr *, int,
			    struct mb_pcpu_list *);
void			 mb_reclaim(void);

/*
 * Initial allocation numbers. Each parameter represents the number of buckets
 * of each object that will be placed initially in each PCPU container for
 * said object.
 */
#define	NMB_MBUF_INIT	4
#define	NMB_CLUST_INIT	16

/*
 * Initialize the mbuf subsystem.
 *
 * We sub-divide the kmem_map into several submaps; this way, we don't have
 * to worry about artificially limiting the number of mbuf or mbuf cluster
 * allocations, due to fear of one type of allocation "stealing" address
 * space initially reserved for another.
 *
 * Setup both the general containers and all the PCPU containers. Populate
 * the PCPU containers with initial numbers.
 */
MALLOC_DEFINE(M_MBUF, "mbufmgr", "mbuf subsystem management structures");
SYSINIT(mbuf, SI_SUB_MBUF, SI_ORDER_FIRST, mbuf_init, NULL)
void
mbuf_init(void *dummy)
{
	struct	mb_pcpu_list	*pcpu_cnt;
	vm_size_t		mb_map_size;
	int			i, j;

	/*
	 * Setup all the submaps, for each type of object that we deal
	 * with in this allocator.
	 */
	mb_map_size = (vm_size_t)(nmbufs * MSIZE);
	mb_map_size = rounddown(mb_map_size, PAGE_SIZE);
	mb_list_mbuf.ml_btable = malloc((unsigned long)mb_map_size / PAGE_SIZE *
	    sizeof(struct mb_bucket *), M_MBUF, M_NOWAIT);
	if (mb_list_mbuf.ml_btable == NULL)
		goto bad;
	mb_list_mbuf.ml_map = kmem_suballoc(kmem_map,&(mb_list_mbuf.ml_mapbase),
	    &(mb_list_mbuf.ml_maptop), mb_map_size);
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
	mb_list_clust.ml_mapfull = 0;
	mb_list_clust.ml_objsize = MCLBYTES;
	mb_list_clust.ml_wmhigh = &clust_limit;

	/* XXX XXX XXX: mbuf_map->system_map = clust_map->system_map = 1 */

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
	mtx_init(&mbuf_gen, "mbuf subsystem general lists lock", 0);
	cv_init(&(mb_list_mbuf.ml_genlist->mgl_mstarved), "mbuf pool starved");
	cv_init(&(mb_list_clust.ml_genlist->mgl_mstarved),
	    "mcluster pool starved");
	mb_list_mbuf.ml_genlist->mb_cont.mc_lock =
	    mb_list_clust.ml_genlist->mb_cont.mc_lock = &mbuf_gen;

	/*
	 * Setup the general containers for each object.
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
	 * Initialize general mbuf statistics
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

		mtx_init(&mbuf_pcpu[i], "mbuf PCPU list lock", 0);
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
 * buffers. Return a pointer to the new bucket (already in the container if
 * successful), or return NULL on failure.
 *
 * LOCKING NOTES:
 * PCPU container lock must be held when this is called.
 * The lock is dropped here so that we can cleanly call the underlying VM
 * code. If we fail, we return with no locks held. If we succeed (i.e. return
 * non-NULL), we return with the PCPU lock held, ready for allocation from
 * the returned bucket.
 */
struct mb_bucket *
mb_pop_cont(struct mb_lstmngr *mb_list, int how, struct mb_pcpu_list *cnt_lst)
{
	struct	mb_bucket	*bucket;
	caddr_t			p;
	int			i;

	MB_UNLOCK_CONT(cnt_lst);
	/*
	 * If our object's (finite) map is starved now (i.e. no more address
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
 * The general case is very easy. Complications only arise if our PCPU
 * container is empty. Things get worse if the PCPU container is empty,
 * the general container is empty, and we've run out of address space
 * in our map; then we try to block if we're willing to (M_TRYWAIT).
 */
static __inline
void *
mb_alloc(struct mb_lstmngr *mb_list, int how, short type)
{
	struct	mb_pcpu_list	*cnt_lst;
	struct	mb_bucket 	*bucket;
	void			*m;

	m = NULL;
	cnt_lst = MB_GET_PCPU_LIST(mb_list);
	MB_LOCK_CONT(cnt_lst);

	if ((bucket = SLIST_FIRST(&(cnt_lst->mb_cont.mc_bhead))) != NULL) {
		/*
		 * This is the easy allocation case. We just grab an object
		 * from a bucket in the PCPU container. At worst, we
		 * have just emptied the bucket and so we remove it
		 * from the container.
		 */
		MB_GET_OBJECT(m, bucket, cnt_lst);
		MB_MBTYPES_INC(cnt_lst, type, 1); 
		MB_UNLOCK_CONT(cnt_lst);
	} else {
		struct	mb_gen_list *gen_list;

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
			MB_UNLOCK_CONT(cnt_lst);
		} else {
			/*
			 * We'll have to allocate a new page.
			 */
			MB_UNLOCK_CONT(gen_list);
			bucket = mb_pop_cont(mb_list, how, cnt_lst);
			if (bucket != NULL) {
				bucket->mb_numfree--;
				m = bucket->mb_free[(bucket->mb_numfree)];
				(*(cnt_lst->mb_cont.mc_objcount))--;
				MB_MBTYPES_INC(cnt_lst, type, 1);
				MB_UNLOCK_CONT(cnt_lst);
			} else {
				if (how == M_TRYWAIT) {
				  /*
			 	   * Absolute worst-case scenario. We block if
			 	   * we're willing to, but only after trying to
				   * steal from other lists.
				   */
					m = mb_alloc_wait(mb_list, type);
				} else {
					/*
					 * no way to indent this code decently
					 * with 8-space tabs.
					 */
					static int last_report;
					/* XXX: No consistency. */
					mbstat.m_drops++;
					if (ticks < last_report ||
					   (ticks - last_report) >= hz) {
						last_report = ticks;
						printf(
"mb_alloc for type %d failed, consider increase mbuf value.\n", type);
					}

				}
			}
		}
	}

	return (m);
}

/*
 * This is the worst-case scenario called only if we're allocating with
 * M_TRYWAIT. We first drain all the protocols, then try to find an mbuf
 * by looking in every PCPU container. If we're still unsuccesful, we
 * try the general container one last time and possibly block on our
 * starved cv.
 */
void *
mb_alloc_wait(struct mb_lstmngr *mb_list, short type)
{
	struct	mb_pcpu_list	*cnt_lst;
	struct	mb_gen_list 	*gen_list;
	struct	mb_bucket 	*bucket;
	void			*m;
	int			i, cv_ret;

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

/*
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
mb_free(struct mb_lstmngr *mb_list, void *m, short type)
{
	struct	mb_pcpu_list	*cnt_lst;
	struct	mb_gen_list 	*gen_list;
	struct	mb_bucket 	*bucket;
	u_int			owner;

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
		MB_LOCK_CONT(gen_list);
		if (owner != (bucket->mb_owner & ~MB_BUCKET_FREE)) {
			MB_UNLOCK_CONT(gen_list);
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
		MB_UNLOCK_CONT(gen_list);
		break;

	default:
		cnt_lst = MB_GET_PCPU_LIST_NUM(mb_list, owner);
		MB_LOCK_CONT(cnt_lst);
		if (owner != (bucket->mb_owner & ~MB_BUCKET_FREE)) {
			MB_UNLOCK_CONT(cnt_lst);
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
			MB_UNLOCK_CONT(cnt_lst);
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
			MB_UNLOCK_CONT(cnt_lst);
			break;
		}

		if (bucket->mb_owner & MB_BUCKET_FREE) {
			SLIST_INSERT_HEAD(&(cnt_lst->mb_cont.mc_bhead),
			    bucket, mb_blist);
			bucket->mb_owner = cnt_lst->mb_cont.mc_numowner;
		}

		MB_UNLOCK_CONT(cnt_lst);
		break;
	}

	return;
}

/*
 * Drain protocols in hopes to free up some resources.
 *
 * LOCKING NOTES:
 * No locks should be held when this is called. The drain routines have to
 * presently acquire some locks which raises the possibility of lock order
 * violation if we're holding any mutex if that mutex is acquired in reverse
 * order relative to one of the locks in the drain routines.
 */
void
mb_reclaim(void)
{
	struct	domain	*dp;
	struct	protosw	*pr;

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

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_drain)
				(*pr->pr_drain)();

}

/*
 * Local mbuf & cluster alloc macros and routines.
 * Local macro and function names begin with an underscore ("_").
 */
void	_mclfree(struct mbuf *);

#define	_m_get(m, how, type) do {					\
	(m) = (struct mbuf *)mb_alloc(&mb_list_mbuf, (how), (type));	\
	if ((m) != NULL) {						\
		(m)->m_type = (type);					\
		(m)->m_next = NULL;					\
		(m)->m_nextpkt = NULL;					\
		(m)->m_data = (m)->m_dat;				\
		(m)->m_flags = 0;					\
	}								\
} while (0)

#define	_m_gethdr(m, how, type) do {					\
	(m) = (struct mbuf *)mb_alloc(&mb_list_mbuf, (how), (type));	\
	if ((m) != NULL) {						\
		(m)->m_type = (type);					\
		(m)->m_next = NULL;					\
		(m)->m_nextpkt = NULL;					\
		(m)->m_data = (m)->m_pktdat;				\
		(m)->m_flags = M_PKTHDR;				\
		(m)->m_pkthdr.rcvif = NULL;				\
		(m)->m_pkthdr.csum_flags = 0;				\
		(m)->m_pkthdr.aux = NULL;				\
	}								\
} while (0)

/* XXX: Check for M_PKTHDR && m_pkthdr.aux is bogus... please fix (see KAME) */
#define	_m_free(m, n) do {						\
	(n) = (m)->m_next;						\
	if ((m)->m_flags & M_EXT)					\
		MEXTFREE((m));						\
	if (((m)->m_flags & M_PKTHDR) != 0 && (m)->m_pkthdr.aux) {	\
		m_freem((m)->m_pkthdr.aux);				\
		(m)->m_pkthdr.aux = NULL;				\
	}								\
	mb_free(&mb_list_mbuf, (m), (m)->m_type);			\
} while (0)

#define	_mext_init_ref(m) do {						\
	(m)->m_ext.ref_cnt = malloc(sizeof(u_int), M_MBUF, M_NOWAIT);	\
	if ((m)->m_ext.ref_cnt != NULL) {				\
		*((m)->m_ext.ref_cnt) = 0;				\
		MEXT_ADD_REF((m));					\
	}								\
} while (0)

#define	_mext_dealloc_ref(m)						\
	free((m)->m_ext.ref_cnt, M_MBUF)

void
_mext_free(struct mbuf *mb)
{

	if (mb->m_ext.ext_type == EXT_CLUSTER)
		mb_free(&mb_list_clust, (caddr_t)mb->m_ext.ext_buf, MT_NOTMBUF);
	else
		(*(mb->m_ext.ext_free))(mb->m_ext.ext_buf, mb->m_ext.ext_args);

	_mext_dealloc_ref(mb);
	return;
}

/* We only include this here to avoid making m_clget() excessively large
 * due to too much inlined code. */
void
_mclfree(struct mbuf *mb)
{

	mb_free(&mb_list_clust, (caddr_t)mb->m_ext.ext_buf, MT_NOTMBUF);
	mb->m_ext.ext_buf = NULL;
	return;
}

/*
 * Exported space allocation and de-allocation routines.
 */
struct mbuf *
m_get(int how, int type)
{
	struct	mbuf *mb;

	_m_get(mb, how, type);
	return (mb);
}

struct mbuf *
m_gethdr(int how, int type)
{
	struct	mbuf *mb;

	_m_gethdr(mb, how, type);
	return (mb);
}

struct mbuf *
m_get_clrd(int how, int type)
{
	struct	mbuf *mb;

	_m_get(mb, how, type);

	if (mb != NULL)
		bzero(mtod(mb, caddr_t), MLEN);

	return (mb);
}

struct mbuf *
m_gethdr_clrd(int how, int type)
{
	struct	mbuf *mb;

	_m_gethdr(mb, how, type);

	if (mb != NULL)
		bzero(mtod(mb, caddr_t), MHLEN);

	return (mb);
}

struct mbuf *
m_free(struct mbuf *mb)
{
	struct	mbuf *nb;

	_m_free(mb, nb);
	return (nb);
}

void
m_clget(struct mbuf *mb, int how)
{

	mb->m_ext.ext_buf = (caddr_t)mb_alloc(&mb_list_clust, how, MT_NOTMBUF);
	if (mb->m_ext.ext_buf != NULL) {
		_mext_init_ref(mb);
		if (mb->m_ext.ref_cnt == NULL)
			_mclfree(mb);
		else {
			mb->m_data = mb->m_ext.ext_buf;
			mb->m_flags |= M_EXT;
			mb->m_ext.ext_free = NULL;
			mb->m_ext.ext_args = NULL;
			mb->m_ext.ext_size = MCLBYTES;
			mb->m_ext.ext_type = EXT_CLUSTER;
		}
	}
	return;
}

void
m_extadd(struct mbuf *mb, caddr_t buf, u_int size,
	 void (*freef)(caddr_t, void *), void *args, short flags, int type)
{

	_mext_init_ref(mb);
	if (mb->m_ext.ref_cnt != NULL) {
		mb->m_flags |= (M_EXT | flags);
		mb->m_ext.ext_buf = buf;
		mb->m_data = mb->m_ext.ext_buf;
		mb->m_ext.ext_size = size;
		mb->m_ext.ext_free = freef;
		mb->m_ext.ext_args = args;
		mb->m_ext.ext_type = type;
	}
	return;
}

/*
 * Change type for mbuf `mb'; this is a relatively expensive operation and
 * should be avoided.
 */
void
m_chtype(struct mbuf *mb, short new_type)
{
	struct	mb_gen_list *gen_list;

	gen_list = MB_GET_GEN_LIST(&mb_list_mbuf);
	MB_LOCK_CONT(gen_list);
	MB_MBTYPES_DEC(gen_list, mb->m_type, 1);
	MB_MBTYPES_INC(gen_list, new_type, 1);
	MB_UNLOCK_CONT(gen_list);
	mb->m_type = new_type;
	return;
}
