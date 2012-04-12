/*
 * Copyright (C) 2011 Matteo Landi, Luigi Rizzo. All rights reserved.
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
 *
 * The original netmap memory allocator, using a single large
 * chunk of memory allocated with contigmalloc.
 */

/*
 * Default amount of memory pre-allocated by the module.
 * We start with a large size and then shrink our demand
 * according to what is avalable when the module is loaded.
 */
#define NETMAP_MEMORY_SIZE (64 * 1024 * PAGE_SIZE)
static void * netmap_malloc(size_t size, const char *msg);
static void netmap_free(void *addr, const char *msg);

#define netmap_if_malloc(len)   netmap_malloc(len, "nifp")
#define netmap_if_free(v)	netmap_free((v), "nifp")

#define netmap_ring_malloc(len) netmap_malloc(len, "ring")
#define netmap_free_rings(na)		\
	netmap_free((na)->tx_rings[0].ring, "shadow rings");

/*
 * Allocator for a pool of packet buffers. For each buffer we have
 * one entry in the bitmap to signal the state. Allocation scans
 * the bitmap, but since this is done only on attach, we are not
 * too worried about performance
 * XXX if we need to allocate small blocks, a translation
 * table is used both for kernel virtual address and physical
 * addresses.
 */
struct netmap_buf_pool {
	u_int total_buffers;	/* total buffers. */
	u_int free;
	u_int bufsize;
	char *base;		/* buffer base address */
	uint32_t *bitmap;	/* one bit per buffer, 1 means free */
};
struct netmap_buf_pool nm_buf_pool;
SYSCTL_INT(_dev_netmap, OID_AUTO, total_buffers,
    CTLFLAG_RD, &nm_buf_pool.total_buffers, 0, "total_buffers");
SYSCTL_INT(_dev_netmap, OID_AUTO, free_buffers,
    CTLFLAG_RD, &nm_buf_pool.free, 0, "free_buffers");


/*
 * Allocate n buffers from the ring, and fill the slot.
 * Buffer 0 is the 'junk' buffer.
 */
static void
netmap_new_bufs(struct netmap_if *nifp __unused,
		struct netmap_slot *slot, u_int n)
{
	struct netmap_buf_pool *p = &nm_buf_pool;
	uint32_t bi = 0;		/* index in the bitmap */
	uint32_t mask, j, i = 0;	/* slot counter */

	if (n > p->free) {
		D("only %d out of %d buffers available", i, n);
		return;
	}
	/* termination is guaranteed by p->free */
	while (i < n && p->free > 0) {
		uint32_t cur = p->bitmap[bi];
		if (cur == 0) { /* bitmask is fully used */
			bi++;
			continue;
		}
		/* locate a slot */
		for (j = 0, mask = 1; (cur & mask) == 0; j++, mask <<= 1) ;
		p->bitmap[bi] &= ~mask;		/* slot in use */
		p->free--;
		slot[i].buf_idx = bi*32+j;
		slot[i].len = p->bufsize;
		slot[i].flags = NS_BUF_CHANGED;
		i++;
	}
	ND("allocated %d buffers, %d available", n, p->free);
}


static void
netmap_free_buf(struct netmap_if *nifp __unused, uint32_t i)
{
	struct netmap_buf_pool *p = &nm_buf_pool;

	uint32_t pos, mask;
	if (i >= p->total_buffers) {
		D("invalid free index %d", i);
		return;
	}
	pos = i / 32;
	mask = 1 << (i % 32);
	if (p->bitmap[pos] & mask) {
		D("slot %d already free", i);
		return;
	}
	p->bitmap[pos] |= mask;
	p->free++;
}


/* Descriptor of the memory objects handled by our memory allocator. */
struct netmap_mem_obj {
	TAILQ_ENTRY(netmap_mem_obj) nmo_next; /* next object in the
						 chain. */
	int nmo_used; /* flag set on used memory objects. */
	size_t nmo_size; /* size of the memory area reserved for the
			    object. */
	void *nmo_data; /* pointer to the memory area. */
};

/* Wrap our memory objects to make them ``chainable``. */
TAILQ_HEAD(netmap_mem_obj_h, netmap_mem_obj);


/* Descriptor of our custom memory allocator. */
struct netmap_mem_d {
	struct mtx nm_mtx; /* lock used to handle the chain of memory
			      objects. */
	struct netmap_mem_obj_h nm_molist; /* list of memory objects */
	size_t nm_size; /* total amount of memory used for rings etc. */
	size_t nm_totalsize; /* total amount of allocated memory
		(the difference is used for buffers) */
	size_t nm_buf_start; /* offset of packet buffers.
			This is page-aligned. */
	size_t nm_buf_len; /* total memory for buffers */
	void *nm_buffer; /* pointer to the whole pre-allocated memory
			    area. */
};

/* Shorthand to compute a netmap interface offset. */
#define netmap_if_offset(v)                                     \
    ((char *) (v) - (char *) nm_mem->nm_buffer)
/* .. and get a physical address given a memory offset */
#define netmap_ofstophys(o)                                     \
    (vtophys(nm_mem->nm_buffer) + (o))


/*------ netmap memory allocator -------*/
/*
 * Request for a chunk of memory.
 *
 * Memory objects are arranged into a list, hence we need to walk this
 * list until we find an object with the needed amount of data free.
 * This sounds like a completely inefficient implementation, but given
 * the fact that data allocation is done once, we can handle it
 * flawlessly.
 *
 * Return NULL on failure.
 */
static void *
netmap_malloc(size_t size, __unused const char *msg)
{
	struct netmap_mem_obj *mem_obj, *new_mem_obj;
	void *ret = NULL;

	NMA_LOCK();
	TAILQ_FOREACH(mem_obj, &nm_mem->nm_molist, nmo_next) {
		if (mem_obj->nmo_used != 0 || mem_obj->nmo_size < size)
			continue;

		new_mem_obj = malloc(sizeof(struct netmap_mem_obj), M_NETMAP,
				     M_WAITOK | M_ZERO);
		TAILQ_INSERT_BEFORE(mem_obj, new_mem_obj, nmo_next);

		new_mem_obj->nmo_used = 1;
		new_mem_obj->nmo_size = size;
		new_mem_obj->nmo_data = mem_obj->nmo_data;
		memset(new_mem_obj->nmo_data, 0, new_mem_obj->nmo_size);

		mem_obj->nmo_size -= size;
		mem_obj->nmo_data = (char *) mem_obj->nmo_data + size;
		if (mem_obj->nmo_size == 0) {
			TAILQ_REMOVE(&nm_mem->nm_molist, mem_obj,
				     nmo_next);
			free(mem_obj, M_NETMAP);
		}

		ret = new_mem_obj->nmo_data;

		break;
	}
	NMA_UNLOCK();
	ND("%s: %d bytes at %p", msg, size, ret);

	return (ret);
}

/*
 * Return the memory to the allocator.
 *
 * While freeing a memory object, we try to merge adjacent chunks in
 * order to reduce memory fragmentation.
 */
static void
netmap_free(void *addr, const char *msg)
{
	size_t size;
	struct netmap_mem_obj *cur, *prev, *next;

	if (addr == NULL) {
		D("NULL addr for %s", msg);
		return;
	}

	NMA_LOCK();
	TAILQ_FOREACH(cur, &nm_mem->nm_molist, nmo_next) {
		if (cur->nmo_data == addr && cur->nmo_used)
			break;
	}
	if (cur == NULL) {
		NMA_UNLOCK();
		D("invalid addr %s %p", msg, addr);
		return;
	}

	size = cur->nmo_size;
	cur->nmo_used = 0;

	/* merge current chunk of memory with the previous one,
	   if present. */
	prev = TAILQ_PREV(cur, netmap_mem_obj_h, nmo_next);
	if (prev && prev->nmo_used == 0) {
		TAILQ_REMOVE(&nm_mem->nm_molist, cur, nmo_next);
		prev->nmo_size += cur->nmo_size;
		free(cur, M_NETMAP);
		cur = prev;
	}

	/* merge with the next one */
	next = TAILQ_NEXT(cur, nmo_next);
	if (next && next->nmo_used == 0) {
		TAILQ_REMOVE(&nm_mem->nm_molist, next, nmo_next);
		cur->nmo_size += next->nmo_size;
		free(next, M_NETMAP);
	}
	NMA_UNLOCK();
	ND("freed %s %d bytes at %p", msg, size, addr);
}


/*
 * Create and return a new ``netmap_if`` object, and possibly also
 * rings and packet buffors.
 *
 * Return NULL on failure.
 */
static void *
netmap_if_new(const char *ifname, struct netmap_adapter *na)
{
	struct netmap_if *nifp;
	struct netmap_ring *ring;
	struct netmap_kring *kring;
	char *buff;
	u_int i, len, ofs, numdesc;
	u_int nrx = na->num_rx_queues + 1; /* shorthand, include stack queue */
	u_int ntx = na->num_tx_queues + 1; /* shorthand, include stack queue */

	/*
	 * the descriptor is followed inline by an array of offsets
	 * to the tx and rx rings in the shared memory region.
	 */
	len = sizeof(struct netmap_if) + (nrx + ntx) * sizeof(ssize_t);
	nifp = netmap_if_malloc(len);
	if (nifp == NULL)
		return (NULL);

	/* initialize base fields */
	*(int *)(uintptr_t)&nifp->ni_rx_queues = na->num_rx_queues;
	*(int *)(uintptr_t)&nifp->ni_tx_queues = na->num_tx_queues;
	strncpy(nifp->ni_name, ifname, IFNAMSIZ);

	(na->refcount)++;	/* XXX atomic ? we are under lock */
	if (na->refcount > 1)
		goto final;

	/*
	 * First instance. Allocate the netmap rings
	 * (one for each hw queue, one pair for the host).
	 * The rings are contiguous, but have variable size.
	 * The entire block is reachable at
	 *	na->tx_rings[0]
	 */
	len = (ntx + nrx) * sizeof(struct netmap_ring) +
	      (ntx * na->num_tx_desc + nrx * na->num_rx_desc) *
		   sizeof(struct netmap_slot);
	buff = netmap_ring_malloc(len);
	if (buff == NULL) {
		D("failed to allocate %d bytes for %s shadow ring",
			len, ifname);
error:
		(na->refcount)--;
		netmap_if_free(nifp);
		return (NULL);
	}
	/* Check whether we have enough buffers */
	len = ntx * na->num_tx_desc + nrx * na->num_rx_desc;
	NMA_LOCK();
	if (nm_buf_pool.free < len) {
		NMA_UNLOCK();
		netmap_free(buff, "not enough bufs");
		goto error;
	}
	/*
	 * in the kring, store the pointers to the shared rings
	 * and initialize the rings. We are under NMA_LOCK().
	 */
	ofs = 0;
	for (i = 0; i < ntx; i++) { /* Transmit rings */
		kring = &na->tx_rings[i];
		numdesc = na->num_tx_desc;
		bzero(kring, sizeof(*kring));
		kring->na = na;

		ring = kring->ring = (struct netmap_ring *)(buff + ofs);
		*(ssize_t *)(uintptr_t)&ring->buf_ofs =
			nm_buf_pool.base - (char *)ring;
		ND("txring[%d] at %p ofs %d", i, ring, ring->buf_ofs);
		*(uint32_t *)(uintptr_t)&ring->num_slots =
			kring->nkr_num_slots = numdesc;

		/*
		 * IMPORTANT:
		 * Always keep one slot empty, so we can detect new
		 * transmissions comparing cur and nr_hwcur (they are
		 * the same only if there are no new transmissions).
		 */
		ring->avail = kring->nr_hwavail = numdesc - 1;
		ring->cur = kring->nr_hwcur = 0;
		*(uint16_t *)(uintptr_t)&ring->nr_buf_size = NETMAP_BUF_SIZE;
		netmap_new_bufs(nifp, ring->slot, numdesc);

		ofs += sizeof(struct netmap_ring) +
			numdesc * sizeof(struct netmap_slot);
	}

	for (i = 0; i < nrx; i++) { /* Receive rings */
		kring = &na->rx_rings[i];
		numdesc = na->num_rx_desc;
		bzero(kring, sizeof(*kring));
		kring->na = na;

		ring = kring->ring = (struct netmap_ring *)(buff + ofs);
		*(ssize_t *)(uintptr_t)&ring->buf_ofs =
			nm_buf_pool.base - (char *)ring;
		ND("rxring[%d] at %p offset %d", i, ring, ring->buf_ofs);
		*(uint32_t *)(uintptr_t)&ring->num_slots =
			kring->nkr_num_slots = numdesc;
		ring->cur = kring->nr_hwcur = 0;
		ring->avail = kring->nr_hwavail = 0; /* empty */
		*(uint16_t *)(uintptr_t)&ring->nr_buf_size = NETMAP_BUF_SIZE;
		netmap_new_bufs(nifp, ring->slot, numdesc);
		ofs += sizeof(struct netmap_ring) +
			numdesc * sizeof(struct netmap_slot);
	}
	NMA_UNLOCK();
	// XXX initialize the selrecord structs.

final:
	/*
	 * fill the slots for the rx and tx queues. They contain the offset
	 * between the ring and nifp, so the information is usable in
	 * userspace to reach the ring from the nifp.
	 */
	for (i = 0; i < ntx; i++) {
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i] =
			(char *)na->tx_rings[i].ring - (char *)nifp;
	}
	for (i = 0; i < nrx; i++) {
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i+ntx] =
			(char *)na->rx_rings[i].ring - (char *)nifp;
	}
	return (nifp);
}

/*
 * Initialize the memory allocator.
 *
 * Create the descriptor for the memory , allocate the pool of memory
 * and initialize the list of memory objects with a single chunk
 * containing the whole pre-allocated memory marked as free.
 *
 * Start with a large size, then halve as needed if we fail to
 * allocate the block. While halving, always add one extra page
 * because buffers 0 and 1 are used for special purposes.
 * Return 0 on success, errno otherwise.
 */
static int
netmap_memory_init(void)
{
	struct netmap_mem_obj *mem_obj;
	void *buf = NULL;
	int i, n, sz = NETMAP_MEMORY_SIZE;
	int extra_sz = 0; // space for rings and two spare buffers

	for (; sz >= 1<<20; sz >>=1) {
		extra_sz = sz/200;
		extra_sz = (extra_sz + 2*PAGE_SIZE - 1) & ~(PAGE_SIZE-1);
	        buf = contigmalloc(sz + extra_sz,
			     M_NETMAP,
			     M_WAITOK | M_ZERO,
			     0, /* low address */
			     -1UL, /* high address */
			     PAGE_SIZE, /* alignment */
			     0 /* boundary */
			    );
		if (buf)
			break;
	}
	if (buf == NULL)
		return (ENOMEM);
	sz += extra_sz;
	nm_mem = malloc(sizeof(struct netmap_mem_d), M_NETMAP,
			      M_WAITOK | M_ZERO);
	mtx_init(&nm_mem->nm_mtx, "netmap memory allocator lock", NULL,
		 MTX_DEF);
	TAILQ_INIT(&nm_mem->nm_molist);
	nm_mem->nm_buffer = buf;
	nm_mem->nm_totalsize = sz;

	/*
	 * A buffer takes 2k, a slot takes 8 bytes + ring overhead,
	 * so the ratio is 200:1. In other words, we can use 1/200 of
	 * the memory for the rings, and the rest for the buffers,
	 * and be sure we never run out.
	 */
	nm_mem->nm_size = sz/200;
	nm_mem->nm_buf_start =
		(nm_mem->nm_size + PAGE_SIZE - 1) & ~(PAGE_SIZE-1);
	nm_mem->nm_buf_len = sz - nm_mem->nm_buf_start;

	nm_buf_pool.base = nm_mem->nm_buffer;
	nm_buf_pool.base += nm_mem->nm_buf_start;
	netmap_buffer_base = nm_buf_pool.base;
	D("netmap_buffer_base %p (offset %d)",
		netmap_buffer_base, (int)nm_mem->nm_buf_start);
	/* number of buffers, they all start as free */

	netmap_total_buffers = nm_buf_pool.total_buffers =
		nm_mem->nm_buf_len / NETMAP_BUF_SIZE;
	nm_buf_pool.bufsize = NETMAP_BUF_SIZE;

	D("Have %d MB, use %dKB for rings, %d buffers at %p",
		(sz >> 20), (int)(nm_mem->nm_size >> 10),
		nm_buf_pool.total_buffers, nm_buf_pool.base);

	/* allocate and initialize the bitmap. Entry 0 is considered
	 * always busy (used as default when there are no buffers left).
	 */
	n = (nm_buf_pool.total_buffers + 31) / 32;
	nm_buf_pool.bitmap = malloc(sizeof(uint32_t) * n, M_NETMAP,
			 M_WAITOK | M_ZERO);
	nm_buf_pool.bitmap[0] = ~3; /* slot 0 and 1 always busy */
	for (i = 1; i < n; i++)
		nm_buf_pool.bitmap[i] = ~0;
	nm_buf_pool.free = nm_buf_pool.total_buffers - 2;
	
	mem_obj = malloc(sizeof(struct netmap_mem_obj), M_NETMAP,
			 M_WAITOK | M_ZERO);
	TAILQ_INSERT_HEAD(&nm_mem->nm_molist, mem_obj, nmo_next);
	mem_obj->nmo_used = 0;
	mem_obj->nmo_size = nm_mem->nm_size;
	mem_obj->nmo_data = nm_mem->nm_buffer;

	return (0);
}


/*
 * Finalize the memory allocator.
 *
 * Free all the memory objects contained inside the list, and deallocate
 * the pool of memory; finally free the memory allocator descriptor.
 */
static void
netmap_memory_fini(void)
{
	struct netmap_mem_obj *mem_obj;

	while (!TAILQ_EMPTY(&nm_mem->nm_molist)) {
		mem_obj = TAILQ_FIRST(&nm_mem->nm_molist);
		TAILQ_REMOVE(&nm_mem->nm_molist, mem_obj, nmo_next);
		if (mem_obj->nmo_used == 1) {
			printf("netmap: leaked %d bytes at %p\n",
			       (int)mem_obj->nmo_size,
			       mem_obj->nmo_data);
		}
		free(mem_obj, M_NETMAP);
	}
	contigfree(nm_mem->nm_buffer, nm_mem->nm_totalsize, M_NETMAP);
	// XXX mutex_destroy(nm_mtx);
	free(nm_mem, M_NETMAP);
}
/*------------- end of memory allocator -----------------*/
