/*
 * Copyright (C) 2012 Matteo Landi, Luigi Rizzo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
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

/*
 * $FreeBSD$
 * $Id: netmap_mem2.c 10830 2012-03-22 18:06:01Z luigi $
 *
 * New memory allocator for netmap
 */

/*
 * The new version allocates three regions:
 *	nm_if_pool      for the struct netmap_if
 *	nm_ring_pool    for the struct netmap_ring
 *	nm_buf_pool    for the packet buffers.
 *
 * All regions need to be page-sized as we export them to
 * userspace through mmap. Only the latter need to be dma-able,
 * but for convenience use the same type of allocator for all.
 *
 * Once mapped, the three regions are exported to userspace
 * as a contiguous block, starting from nm_if_pool. Each
 * cluster (and pool) is an integral number of pages.
 *   [ . . . ][ . . . . . .][ . . . . . . . . . .]
 *    nm_if     nm_ring            nm_buf
 *
 * The userspace areas contain offsets of the objects in userspace.
 * When (at init time) we write these offsets, we find out the index
 * of the object, and from there locate the offset from the beginning
 * of the region.
 *
 * Allocator for a pool of memory objects of the same size.
 * The pool is split into smaller clusters, whose size is a
 * multiple of the page size. The cluster size is chosen
 * to minimize the waste for a given max cluster size
 * (we do it by brute force, as we have relatively few object
 * per cluster).
 *
 * To be polite with the cache, objects are aligned to
 * the cache line, or 64 bytes. Sizes are rounded to multiple of 64.
 * For each object we have
 * one entry in the bitmap to signal the state. Allocation scans
 * the bitmap, but since this is done only on attach, we are not
 * too worried about performance
 */

/*
 *	MEMORY SIZES:
 *
 * (all the parameters below will become tunables)
 *
 * struct netmap_if is variable size but small.
 * Assuming each NIC has 8+2 rings, (4+1 tx, 4+1 rx) the netmap_if
 * uses 120 bytes on a 64-bit machine.
 * We allocate NETMAP_IF_MAX_SIZE  (1024) which should work even for
 * cards with 48 ring pairs.
 * The total number of 'struct netmap_if' could be slightly larger
 * that the total number of rings on all interfaces on the system.
 */
#define NETMAP_IF_MAX_SIZE      1024
#define NETMAP_IF_MAX_NUM       512

/*
 * netmap rings are up to 2..4k descriptors, 8 bytes each,
 * plus some glue at the beginning (32 bytes).
 * We set the default ring size to 9 pages (36K) and enable
 * a few hundreds of them.
 */
#define NETMAP_RING_MAX_SIZE    (9*PAGE_SIZE)
#define NETMAP_RING_MAX_NUM     200	/* approx 8MB */

/*
 * Buffers: the more the better. Buffer size is NETMAP_BUF_SIZE,
 * 2k or slightly less, aligned to 64 bytes.
 * A large 10G interface can have 2k*18 = 36k buffers per interface,
 * or about 72MB of memory. Up to us to use more.
 */
#ifndef CONSERVATIVE
#define NETMAP_BUF_MAX_NUM      100000  /* 200MB */
#else /* CONSERVATIVE */
#define NETMAP_BUF_MAX_NUM      20000   /* 40MB */
#endif


struct netmap_obj_pool {
	char name[16];		/* name of the allocator */
	u_int objtotal;         /* actual total number of objects. */
	u_int objfree;          /* number of free objects. */
	u_int clustentries;	/* actual objects per cluster */

	/* the total memory space is _numclusters*_clustsize */
	u_int _numclusters;	/* how many clusters */
	u_int _clustsize;        /* cluster size */
	u_int _objsize;		/* actual object size */

	u_int _memtotal;	/* _numclusters*_clustsize */
	struct lut_entry *lut;  /* virt,phys addresses, objtotal entries */
	uint32_t *bitmap;       /* one bit per buffer, 1 means free */
};

struct netmap_mem_d {
	NM_LOCK_T nm_mtx; /* protect the allocator ? */
	u_int nm_totalsize; /* shorthand */

	/* pointers to the three allocators */
	struct netmap_obj_pool *nm_if_pool;
	struct netmap_obj_pool *nm_ring_pool;
	struct netmap_obj_pool *nm_buf_pool;
};

struct lut_entry *netmap_buffer_lut;	/* exported */


/*
 * Convert a userspace offset to a phisical address.
 * XXX re-do in a simpler way.
 *
 * The idea here is to hide userspace applications the fact that pre-allocated
 * memory is not contiguous, but fragmented across different clusters and
 * smaller memory allocators. Consequently, first of all we need to find which
 * allocator is owning provided offset, then we need to find out the physical
 * address associated to target page (this is done using the look-up table.
 */
static inline vm_paddr_t
netmap_ofstophys(vm_offset_t offset)
{
	const struct netmap_obj_pool *p[] = {
		nm_mem->nm_if_pool,
		nm_mem->nm_ring_pool,
		nm_mem->nm_buf_pool };
	int i;
	vm_offset_t o = offset;


	for (i = 0; i < 3; offset -= p[i]->_memtotal, i++) {
		if (offset >= p[i]->_memtotal)
			continue;
		// XXX now scan the clusters
		return p[i]->lut[offset / p[i]->_objsize].paddr +
			offset % p[i]->_objsize;
	}
	D("invalid ofs 0x%x out of 0x%x 0x%x 0x%x", o,
		(u_int)p[0]->_memtotal, p[0]->_memtotal + p[1]->_memtotal,
		p[0]->_memtotal + p[1]->_memtotal + p[2]->_memtotal);
	return 0;	// XXX bad address
}

/*
 * we store objects by kernel address, need to find the offset
 * within the pool to export the value to userspace.
 * Algorithm: scan until we find the cluster, then add the
 * actual offset in the cluster
 */
static ssize_t
netmap_obj_offset(struct netmap_obj_pool *p, const void *vaddr)
{
	int i, k = p->clustentries, n = p->objtotal;
	ssize_t ofs = 0;

	for (i = 0; i < n; i += k, ofs += p->_clustsize) {
		const char *base = p->lut[i].vaddr;
		ssize_t relofs = (const char *) vaddr - base;

		if (relofs < 0 || relofs > p->_clustsize)
			continue;

		ofs = ofs + relofs;
		ND("%s: return offset %d (cluster %d) for pointer %p",
		    p->name, ofs, i, vaddr);
		return ofs;
	}
	D("address %p is not contained inside any cluster (%s)",
	    vaddr, p->name);
	return 0; /* An error occurred */
}

/* Helper functions which convert virtual addresses to offsets */
#define netmap_if_offset(v)					\
	netmap_obj_offset(nm_mem->nm_if_pool, (v))

#define netmap_ring_offset(v)					\
    (nm_mem->nm_if_pool->_memtotal + 				\
	netmap_obj_offset(nm_mem->nm_ring_pool, (v)))

#define netmap_buf_offset(v)					\
    (nm_mem->nm_if_pool->_memtotal +				\
	nm_mem->nm_ring_pool->_memtotal +			\
	netmap_obj_offset(nm_mem->nm_buf_pool, (v)))


static void *
netmap_obj_malloc(struct netmap_obj_pool *p, int len)
{
	uint32_t i = 0;			/* index in the bitmap */
	uint32_t mask, j;		/* slot counter */
	void *vaddr = NULL;

	if (len > p->_objsize) {
		D("%s request size %d too large", p->name, len);
		// XXX cannot reduce the size
		return NULL;
	}

	if (p->objfree == 0) {
		D("%s allocator: run out of memory", p->name);
		return NULL;
	}

	/* termination is guaranteed by p->free */
	while (vaddr == NULL) {
		uint32_t cur = p->bitmap[i];
		if (cur == 0) { /* bitmask is fully used */
			i++;
			continue;
		}
		/* locate a slot */
		for (j = 0, mask = 1; (cur & mask) == 0; j++, mask <<= 1)
			;

		p->bitmap[i] &= ~mask; /* mark object as in use */
		p->objfree--;

		vaddr = p->lut[i * 32 + j].vaddr;
	}
	ND("%s allocator: allocated object @ [%d][%d]: vaddr %p", i, j, vaddr);

	return vaddr;
}


/*
 * free by index, not by address
 */
static void
netmap_obj_free(struct netmap_obj_pool *p, uint32_t j)
{
	if (j >= p->objtotal) {
		D("invalid index %u, max %u", j, p->objtotal);
		return;
	}
	p->bitmap[j / 32] |= (1 << (j % 32));
	p->objfree++;
	return;
}

static void
netmap_obj_free_va(struct netmap_obj_pool *p, void *vaddr)
{
	int i, j, n = p->_memtotal / p->_clustsize;

	for (i = 0, j = 0; i < n; i++, j += p->clustentries) {
		void *base = p->lut[i * p->clustentries].vaddr;
		ssize_t relofs = (ssize_t) vaddr - (ssize_t) base;

		/* Given address, is out of the scope of the current cluster.*/
		if (vaddr < base || relofs > p->_clustsize)
			continue;

		j = j + relofs / p->_objsize;
		KASSERT(j != 0, ("Cannot free object 0"));
		netmap_obj_free(p, j);
		return;
	}
	ND("address %p is not contained inside any cluster (%s)",
	    vaddr, p->name);
}

#define netmap_if_malloc(len)	netmap_obj_malloc(nm_mem->nm_if_pool, len)
#define netmap_if_free(v)	netmap_obj_free_va(nm_mem->nm_if_pool, (v))
#define netmap_ring_malloc(len)	netmap_obj_malloc(nm_mem->nm_ring_pool, len)
#define netmap_buf_malloc()			\
	netmap_obj_malloc(nm_mem->nm_buf_pool, NETMAP_BUF_SIZE)


/* Return the index associated to the given packet buffer */
#define netmap_buf_index(v)						\
    (netmap_obj_offset(nm_mem->nm_buf_pool, (v)) / nm_mem->nm_buf_pool->_objsize)


static void
netmap_new_bufs(struct netmap_if *nifp __unused,
                struct netmap_slot *slot, u_int n)
{
	struct netmap_obj_pool *p = nm_mem->nm_buf_pool;
	uint32_t i = 0;	/* slot counter */

	for (i = 0; i < n; i++) {
		void *vaddr = netmap_buf_malloc();
		if (vaddr == NULL) {
			D("unable to locate empty packet buffer");
			goto cleanup;
		}

		slot[i].buf_idx = netmap_buf_index(vaddr);
		KASSERT(slot[i].buf_idx != 0,
		    ("Assigning buf_idx=0 to just created slot"));
		slot[i].len = p->_objsize;
		slot[i].flags = NS_BUF_CHANGED; // XXX GAETANO hack
	}

	ND("allocated %d buffers, %d available", n, p->objfree);
	return;

cleanup:
	for (i--; i >= 0; i--) {
		netmap_obj_free(nm_mem->nm_buf_pool, slot[i].buf_idx);
	}
}


static void
netmap_free_buf(struct netmap_if *nifp, uint32_t i)
{
	struct netmap_obj_pool *p = nm_mem->nm_buf_pool;
	if (i < 2 || i >= p->objtotal) {
		D("Cannot free buf#%d: should be in [2, %d[", i, p->objtotal);
		return;
	}
	netmap_obj_free(nm_mem->nm_buf_pool, i);
}


/*
 * Free all resources related to an allocator.
 */
static void
netmap_destroy_obj_allocator(struct netmap_obj_pool *p)
{
	if (p == NULL)
		return;
	if (p->bitmap)
		free(p->bitmap, M_NETMAP);
	if (p->lut) {
		int i;
		for (i = 0; i < p->objtotal; i += p->clustentries) {
			if (p->lut[i].vaddr)
				contigfree(p->lut[i].vaddr, p->_clustsize, M_NETMAP);
		}
		bzero(p->lut, sizeof(struct lut_entry) * p->objtotal);
		free(p->lut, M_NETMAP);
	}
	bzero(p, sizeof(*p));
	free(p, M_NETMAP);
}

/*
 * We receive a request for objtotal objects, of size objsize each.
 * Internally we may round up both numbers, as we allocate objects
 * in small clusters multiple of the page size.
 * In the allocator we don't need to store the objsize,
 * but we do need to keep track of objtotal' and clustentries,
 * as they are needed when freeing memory.
 *
 * XXX note -- userspace needs the buffers to be contiguous,
 *	so we cannot afford gaps at the end of a cluster.
 */
static struct netmap_obj_pool *
netmap_new_obj_allocator(const char *name, u_int objtotal, u_int objsize)
{
	struct netmap_obj_pool *p;
	int i, n;
	u_int clustsize;	/* the cluster size, multiple of page size */
	u_int clustentries;	/* how many objects per entry */

#define MAX_CLUSTSIZE	(1<<17)
#define LINE_ROUND	64
	if (objsize >= MAX_CLUSTSIZE) {
		/* we could do it but there is no point */
		D("unsupported allocation for %d bytes", objsize);
		return NULL;
	}
	/* make sure objsize is a multiple of LINE_ROUND */
	i = (objsize & (LINE_ROUND - 1));
	if (i) {
		D("XXX aligning object by %d bytes", LINE_ROUND - i);
		objsize += LINE_ROUND - i;
	}
	/*
	 * Compute number of objects using a brute-force approach:
	 * given a max cluster size,
	 * we try to fill it with objects keeping track of the
	 * wasted space to the next page boundary.
	 */
	for (clustentries = 0, i = 1;; i++) {
		u_int delta, used = i * objsize;
		if (used > MAX_CLUSTSIZE)
			break;
		delta = used % PAGE_SIZE;
		if (delta == 0) { // exact solution
			clustentries = i;
			break;
		}
		if (delta > ( (clustentries*objsize) % PAGE_SIZE) )
			clustentries = i;
	}
	// D("XXX --- ouch, delta %d (bad for buffers)", delta);
	/* compute clustsize and round to the next page */
	clustsize = clustentries * objsize;
	i =  (clustsize & (PAGE_SIZE - 1));
	if (i)
		clustsize += PAGE_SIZE - i;
	D("objsize %d clustsize %d objects %d",
		objsize, clustsize, clustentries);

	p = malloc(sizeof(struct netmap_obj_pool), M_NETMAP,
	    M_WAITOK | M_ZERO);
	if (p == NULL) {
		D("Unable to create '%s' allocator", name);
		return NULL;
	}
	/*
	 * Allocate and initialize the lookup table.
	 *
	 * The number of clusters is n = ceil(objtotal/clustentries)
	 * objtotal' = n * clustentries
	 */
	strncpy(p->name, name, sizeof(p->name));
	p->clustentries = clustentries;
	p->_clustsize = clustsize;
	n = (objtotal + clustentries - 1) / clustentries;
	p->_numclusters = n;
	p->objtotal = n * clustentries;
	p->objfree = p->objtotal - 2; /* obj 0 and 1 are reserved */
	p->_objsize = objsize;
	p->_memtotal = p->_numclusters * p->_clustsize;

	p->lut = malloc(sizeof(struct lut_entry) * p->objtotal,
	    M_NETMAP, M_WAITOK | M_ZERO);
	if (p->lut == NULL) {
		D("Unable to create lookup table for '%s' allocator", name);
		goto clean;
	}

	/* Allocate the bitmap */
	n = (p->objtotal + 31) / 32;
	p->bitmap = malloc(sizeof(uint32_t) * n, M_NETMAP, M_WAITOK | M_ZERO);
	if (p->bitmap == NULL) {
		D("Unable to create bitmap (%d entries) for allocator '%s'", n,
		    name);
		goto clean;
	}

	/*
	 * Allocate clusters, init pointers and bitmap
	 */
	for (i = 0; i < p->objtotal;) {
		int lim = i + clustentries;
		char *clust;

		clust = contigmalloc(clustsize, M_NETMAP, M_WAITOK | M_ZERO,
		    0, -1UL, PAGE_SIZE, 0);
		if (clust == NULL) {
			/*
			 * If we get here, there is a severe memory shortage,
			 * so halve the allocated memory to reclaim some.
			 */
			D("Unable to create cluster at %d for '%s' allocator",
			    i, name);
			lim = i / 2;
			for (; i >= lim; i--) {
				p->bitmap[ (i>>5) ] &=  ~( 1 << (i & 31) );
				if (i % clustentries == 0 && p->lut[i].vaddr)
					contigfree(p->lut[i].vaddr,
						p->_clustsize, M_NETMAP);
			}
			p->objtotal = i;
			p->objfree = p->objtotal - 2;
			p->_numclusters = i / clustentries;
			p->_memtotal = p->_numclusters * p->_clustsize;
			break;
		}
		for (; i < lim; i++, clust += objsize) {
			p->bitmap[ (i>>5) ] |=  ( 1 << (i & 31) );
			p->lut[i].vaddr = clust;
			p->lut[i].paddr = vtophys(clust);
		}
	}
	p->bitmap[0] = ~3; /* objs 0 and 1 is always busy */
	D("Pre-allocated %d clusters (%d/%dKB) for '%s'",
	    p->_numclusters, p->_clustsize >> 10,
	    p->_memtotal >> 10, name);

	return p;

clean:
	netmap_destroy_obj_allocator(p);
	return NULL;
}

static int
netmap_memory_init(void)
{
	struct netmap_obj_pool *p;

	nm_mem = malloc(sizeof(struct netmap_mem_d), M_NETMAP,
			      M_WAITOK | M_ZERO);
	if (nm_mem == NULL)
		goto clean;

	p = netmap_new_obj_allocator("netmap_if",
	    NETMAP_IF_MAX_NUM, NETMAP_IF_MAX_SIZE);
	if (p == NULL)
		goto clean;
	nm_mem->nm_if_pool = p;

	p = netmap_new_obj_allocator("netmap_ring",
	    NETMAP_RING_MAX_NUM, NETMAP_RING_MAX_SIZE);
	if (p == NULL)
		goto clean;
	nm_mem->nm_ring_pool = p;

	p = netmap_new_obj_allocator("netmap_buf",
	    NETMAP_BUF_MAX_NUM, NETMAP_BUF_SIZE);
	if (p == NULL)
		goto clean;
	netmap_total_buffers = p->objtotal;
	netmap_buffer_lut = p->lut;
	nm_mem->nm_buf_pool = p;
	netmap_buffer_base = p->lut[0].vaddr;

	mtx_init(&nm_mem->nm_mtx, "netmap memory allocator lock", NULL,
		 MTX_DEF);
	nm_mem->nm_totalsize =
	    nm_mem->nm_if_pool->_memtotal +
	    nm_mem->nm_ring_pool->_memtotal +
	    nm_mem->nm_buf_pool->_memtotal;

	D("Have %d KB for interfaces, %d KB for rings and %d MB for buffers",
	    nm_mem->nm_if_pool->_memtotal >> 10,
	    nm_mem->nm_ring_pool->_memtotal >> 10,
	    nm_mem->nm_buf_pool->_memtotal >> 20);
	return 0;

clean:
	if (nm_mem) {
		netmap_destroy_obj_allocator(nm_mem->nm_ring_pool);
		netmap_destroy_obj_allocator(nm_mem->nm_if_pool);
		free(nm_mem, M_NETMAP);
	}
	return ENOMEM;
}


static void
netmap_memory_fini(void)
{
	if (!nm_mem)
		return;
	netmap_destroy_obj_allocator(nm_mem->nm_if_pool);
	netmap_destroy_obj_allocator(nm_mem->nm_ring_pool);
	netmap_destroy_obj_allocator(nm_mem->nm_buf_pool);
	mtx_destroy(&nm_mem->nm_mtx);
	free(nm_mem, M_NETMAP);
}



static void *
netmap_if_new(const char *ifname, struct netmap_adapter *na)
{
	struct netmap_if *nifp;
	struct netmap_ring *ring;
	ssize_t base; /* handy for relative offsets between rings and nifp */
	u_int i, len, ndesc;
	u_int ntx = na->num_tx_rings + 1; /* shorthand, include stack ring */
	u_int nrx = na->num_rx_rings + 1; /* shorthand, include stack ring */
	struct netmap_kring *kring;

	NMA_LOCK();
	/*
	 * the descriptor is followed inline by an array of offsets
	 * to the tx and rx rings in the shared memory region.
	 */
	len = sizeof(struct netmap_if) + (nrx + ntx) * sizeof(ssize_t);
	nifp = netmap_if_malloc(len);
	if (nifp == NULL) {
		NMA_UNLOCK();
		return NULL;
	}

	/* initialize base fields -- override const */
	*(int *)(uintptr_t)&nifp->ni_tx_rings = na->num_tx_rings;
	*(int *)(uintptr_t)&nifp->ni_rx_rings = na->num_rx_rings;
	strncpy(nifp->ni_name, ifname, IFNAMSIZ);

	(na->refcount)++;	/* XXX atomic ? we are under lock */
	if (na->refcount > 1) { /* already setup, we are done */
		NMA_UNLOCK();
		goto final;
	}

	/*
	 * First instance, allocate netmap rings and buffers for this card
	 * The rings are contiguous, but have variable size.
	 */
	for (i = 0; i < ntx; i++) { /* Transmit rings */
		kring = &na->tx_rings[i];
		ndesc = na->num_tx_desc;
		bzero(kring, sizeof(*kring));
		len = sizeof(struct netmap_ring) +
			  ndesc * sizeof(struct netmap_slot);
		ring = netmap_ring_malloc(len);
		if (ring == NULL) {
			D("Cannot allocate tx_ring[%d] for %s", i, ifname);
			goto cleanup;
		}
		ND("txring[%d] at %p ofs %d", i, ring);
		kring->na = na;
		kring->ring = ring;
		*(int *)(uintptr_t)&ring->num_slots = kring->nkr_num_slots = ndesc;
		*(ssize_t *)(uintptr_t)&ring->buf_ofs =
		    (nm_mem->nm_if_pool->_memtotal +
			nm_mem->nm_ring_pool->_memtotal) -
			netmap_ring_offset(ring);

		/*
		 * IMPORTANT:
		 * Always keep one slot empty, so we can detect new
		 * transmissions comparing cur and nr_hwcur (they are
		 * the same only if there are no new transmissions).
		 */
		ring->avail = kring->nr_hwavail = ndesc - 1;
		ring->cur = kring->nr_hwcur = 0;
		*(int *)(uintptr_t)&ring->nr_buf_size = NETMAP_BUF_SIZE;
		ND("initializing slots for txring[%d]", i);
		netmap_new_bufs(nifp, ring->slot, ndesc);
	}

	for (i = 0; i < nrx; i++) { /* Receive rings */
		kring = &na->rx_rings[i];
		ndesc = na->num_rx_desc;
		bzero(kring, sizeof(*kring));
		len = sizeof(struct netmap_ring) +
			  ndesc * sizeof(struct netmap_slot);
		ring = netmap_ring_malloc(len);
		if (ring == NULL) {
			D("Cannot allocate rx_ring[%d] for %s", i, ifname);
			goto cleanup;
		}
		ND("rxring[%d] at %p ofs %d", i, ring);

		kring->na = na;
		kring->ring = ring;
		*(int *)(uintptr_t)&ring->num_slots = kring->nkr_num_slots = ndesc;
		*(ssize_t *)(uintptr_t)&ring->buf_ofs =
		    (nm_mem->nm_if_pool->_memtotal +
		        nm_mem->nm_ring_pool->_memtotal) -
			netmap_ring_offset(ring);

		ring->cur = kring->nr_hwcur = 0;
		ring->avail = kring->nr_hwavail = 0; /* empty */
		*(int *)(uintptr_t)&ring->nr_buf_size = NETMAP_BUF_SIZE;
		ND("initializing slots for rxring[%d]", i);
		netmap_new_bufs(nifp, ring->slot, ndesc);
	}
	NMA_UNLOCK();
#ifdef linux
	// XXX initialize the selrecord structs.
	for (i = 0; i < ntx; i++)
		init_waitqueue_head(&na->rx_rings[i].si);
	for (i = 0; i < nrx; i++)
		init_waitqueue_head(&na->tx_rings[i].si);
	init_waitqueue_head(&na->rx_si);
	init_waitqueue_head(&na->tx_si);
#endif
final:
	/*
	 * fill the slots for the rx and tx rings. They contain the offset
	 * between the ring and nifp, so the information is usable in
	 * userspace to reach the ring from the nifp.
	 */
	base = netmap_if_offset(nifp);
	for (i = 0; i < ntx; i++) {
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i] =
			netmap_ring_offset(na->tx_rings[i].ring) - base;
	}
	for (i = 0; i < nrx; i++) {
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i+ntx] =
			netmap_ring_offset(na->rx_rings[i].ring) - base;
	}
	return (nifp);
cleanup:
	// XXX missing
	NMA_UNLOCK();
	return NULL;
}

static void
netmap_free_rings(struct netmap_adapter *na)
{
	int i;
	for (i = 0; i < na->num_tx_rings + 1; i++)
		netmap_obj_free_va(nm_mem->nm_ring_pool,
			na->tx_rings[i].ring);
	for (i = 0; i < na->num_rx_rings + 1; i++)
		netmap_obj_free_va(nm_mem->nm_ring_pool,
			na->rx_rings[i].ring);
}
