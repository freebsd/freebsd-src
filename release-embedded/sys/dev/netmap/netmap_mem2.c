/*
 * Copyright (C) 2012-2013 Matteo Landi, Luigi Rizzo, Giuseppe Lettieri. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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

#ifdef linux
#include "bsd_glue.h"
#endif /* linux */

#ifdef __APPLE__
#include "osx_glue.h"
#endif /* __APPLE__ */

#ifdef __FreeBSD__
#include <sys/cdefs.h> /* prerequisite */
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <vm/vm.h>	/* vtophys */
#include <vm/pmap.h>	/* vtophys */
#include <sys/socket.h> /* sockaddrs */
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>
#include <machine/bus.h>	/* bus_dmamap_* */

#endif /* __FreeBSD__ */

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include "netmap_mem2.h"

#ifdef linux
#define NMA_LOCK_INIT(n)	sema_init(&(n)->nm_mtx, 1)
#define NMA_LOCK_DESTROY(n)
#define NMA_LOCK(n)		down(&(n)->nm_mtx)
#define NMA_UNLOCK(n)		up(&(n)->nm_mtx)
#else /* !linux */
#define NMA_LOCK_INIT(n)	mtx_init(&(n)->nm_mtx, "netmap memory allocator lock", NULL, MTX_DEF)
#define NMA_LOCK_DESTROY(n)	mtx_destroy(&(n)->nm_mtx)
#define NMA_LOCK(n)		mtx_lock(&(n)->nm_mtx)
#define NMA_UNLOCK(n)		mtx_unlock(&(n)->nm_mtx)
#endif /* linux */


struct netmap_obj_params netmap_params[NETMAP_POOLS_NR] = {
	[NETMAP_IF_POOL] = {
		.size = 1024,
		.num  = 100,
	},
	[NETMAP_RING_POOL] = {
		.size = 9*PAGE_SIZE,
		.num  = 200,
	},
	[NETMAP_BUF_POOL] = {
		.size = 2048,
		.num  = NETMAP_BUF_MAX_NUM,
	},
};


/*
 * nm_mem is the memory allocator used for all physical interfaces
 * running in netmap mode.
 * Virtual (VALE) ports will have each its own allocator.
 */
static int netmap_mem_global_config(struct netmap_mem_d *nmd);
static int netmap_mem_global_finalize(struct netmap_mem_d *nmd);
static void netmap_mem_global_deref(struct netmap_mem_d *nmd);
struct netmap_mem_d nm_mem = {	/* Our memory allocator. */
	.pools = {
		[NETMAP_IF_POOL] = {
			.name 	= "netmap_if",
			.objminsize = sizeof(struct netmap_if),
			.objmaxsize = 4096,
			.nummin     = 10,	/* don't be stingy */
			.nummax	    = 10000,	/* XXX very large */
		},
		[NETMAP_RING_POOL] = {
			.name 	= "netmap_ring",
			.objminsize = sizeof(struct netmap_ring),
			.objmaxsize = 32*PAGE_SIZE,
			.nummin     = 2,
			.nummax	    = 1024,
		},
		[NETMAP_BUF_POOL] = {
			.name	= "netmap_buf",
			.objminsize = 64,
			.objmaxsize = 65536,
			.nummin     = 4,
			.nummax	    = 1000000, /* one million! */
		},
	},
	.config   = netmap_mem_global_config,
	.finalize = netmap_mem_global_finalize,
	.deref    = netmap_mem_global_deref,
};


// XXX logically belongs to nm_mem
struct lut_entry *netmap_buffer_lut;	/* exported */

/* blueprint for the private memory allocators */
static int netmap_mem_private_config(struct netmap_mem_d *nmd);
static int netmap_mem_private_finalize(struct netmap_mem_d *nmd);
static void netmap_mem_private_deref(struct netmap_mem_d *nmd);
const struct netmap_mem_d nm_blueprint = {
	.pools = {
		[NETMAP_IF_POOL] = {
			.name 	= "%s_if",
			.objminsize = sizeof(struct netmap_if),
			.objmaxsize = 4096,
			.nummin     = 1,
			.nummax	    = 10,
		},
		[NETMAP_RING_POOL] = {
			.name 	= "%s_ring",
			.objminsize = sizeof(struct netmap_ring),
			.objmaxsize = 32*PAGE_SIZE,
			.nummin     = 2,
			.nummax	    = 1024,
		},
		[NETMAP_BUF_POOL] = {
			.name	= "%s_buf",
			.objminsize = 64,
			.objmaxsize = 65536,
			.nummin     = 4,
			.nummax	    = 1000000, /* one million! */
		},
	},
	.config   = netmap_mem_private_config,
	.finalize = netmap_mem_private_finalize,
	.deref    = netmap_mem_private_deref,

	.flags = NETMAP_MEM_PRIVATE,
};

/* memory allocator related sysctls */

#define STRINGIFY(x) #x


#define DECLARE_SYSCTLS(id, name) \
	SYSCTL_INT(_dev_netmap, OID_AUTO, name##_size, \
	    CTLFLAG_RW, &netmap_params[id].size, 0, "Requested size of netmap " STRINGIFY(name) "s"); \
	SYSCTL_INT(_dev_netmap, OID_AUTO, name##_curr_size, \
	    CTLFLAG_RD, &nm_mem.pools[id]._objsize, 0, "Current size of netmap " STRINGIFY(name) "s"); \
	SYSCTL_INT(_dev_netmap, OID_AUTO, name##_num, \
	    CTLFLAG_RW, &netmap_params[id].num, 0, "Requested number of netmap " STRINGIFY(name) "s"); \
	SYSCTL_INT(_dev_netmap, OID_AUTO, name##_curr_num, \
	    CTLFLAG_RD, &nm_mem.pools[id].objtotal, 0, "Current number of netmap " STRINGIFY(name) "s")

SYSCTL_DECL(_dev_netmap);
DECLARE_SYSCTLS(NETMAP_IF_POOL, if);
DECLARE_SYSCTLS(NETMAP_RING_POOL, ring);
DECLARE_SYSCTLS(NETMAP_BUF_POOL, buf);

/*
 * First, find the allocator that contains the requested offset,
 * then locate the cluster through a lookup table.
 */
vm_paddr_t
netmap_mem_ofstophys(struct netmap_mem_d* nmd, vm_ooffset_t offset)
{
	int i;
	vm_ooffset_t o = offset;
	vm_paddr_t pa;
	struct netmap_obj_pool *p;

	NMA_LOCK(nmd);
	p = nmd->pools;

	for (i = 0; i < NETMAP_POOLS_NR; offset -= p[i].memtotal, i++) {
		if (offset >= p[i].memtotal)
			continue;
		// now lookup the cluster's address
		pa = p[i].lut[offset / p[i]._objsize].paddr +
			offset % p[i]._objsize;
		NMA_UNLOCK(nmd);
		return pa;
	}
	/* this is only in case of errors */
	D("invalid ofs 0x%x out of 0x%x 0x%x 0x%x", (u_int)o,
		p[NETMAP_IF_POOL].memtotal,
		p[NETMAP_IF_POOL].memtotal
			+ p[NETMAP_RING_POOL].memtotal,
		p[NETMAP_IF_POOL].memtotal
			+ p[NETMAP_RING_POOL].memtotal
			+ p[NETMAP_BUF_POOL].memtotal);
	NMA_UNLOCK(nmd);
	return 0;	// XXX bad address
}

int
netmap_mem_get_info(struct netmap_mem_d* nmd, u_int* size, u_int *memflags)
{
	int error = 0;
	NMA_LOCK(nmd);
	error = nmd->config(nmd);
	if (error)
		goto out;
	if (nmd->flags & NETMAP_MEM_FINALIZED) {
		*size = nmd->nm_totalsize;
	} else {
		int i;
		*size = 0;
		for (i = 0; i < NETMAP_POOLS_NR; i++) {
			struct netmap_obj_pool *p = nmd->pools + i;
			*size += (p->_numclusters * p->_clustsize);
		}
	}
	*memflags = nmd->flags;
out:
	NMA_UNLOCK(nmd);
	return error;
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
	int i, k = p->_clustentries, n = p->objtotal;
	ssize_t ofs = 0;

	for (i = 0; i < n; i += k, ofs += p->_clustsize) {
		const char *base = p->lut[i].vaddr;
		ssize_t relofs = (const char *) vaddr - base;

		if (relofs < 0 || relofs >= p->_clustsize)
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
#define netmap_if_offset(n, v)					\
	netmap_obj_offset(&(n)->pools[NETMAP_IF_POOL], (v))

#define netmap_ring_offset(n, v)				\
    ((n)->pools[NETMAP_IF_POOL].memtotal + 			\
	netmap_obj_offset(&(n)->pools[NETMAP_RING_POOL], (v)))

#define netmap_buf_offset(n, v)					\
    ((n)->pools[NETMAP_IF_POOL].memtotal +			\
	(n)->pools[NETMAP_RING_POOL].memtotal +		\
	netmap_obj_offset(&(n)->pools[NETMAP_BUF_POOL], (v)))


ssize_t
netmap_mem_if_offset(struct netmap_mem_d *nmd, const void *addr)
{
	ssize_t v;
	NMA_LOCK(nmd);
	v = netmap_if_offset(nmd, addr);
	NMA_UNLOCK(nmd);
	return v;
}

/*
 * report the index, and use start position as a hint,
 * otherwise buffer allocation becomes terribly expensive.
 */
static void *
netmap_obj_malloc(struct netmap_obj_pool *p, u_int len, uint32_t *start, uint32_t *index)
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
		D("no more %s objects", p->name);
		return NULL;
	}
	if (start)
		i = *start;

	/* termination is guaranteed by p->free, but better check bounds on i */
	while (vaddr == NULL && i < p->bitmap_slots)  {
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
		if (index)
			*index = i * 32 + j;
	}
	ND("%s allocator: allocated object @ [%d][%d]: vaddr %p", i, j, vaddr);

	if (start)
		*start = i;
	return vaddr;
}


/*
 * free by index, not by address. This is slow, but is only used
 * for a small number of objects (rings, nifp)
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
	u_int i, j, n = p->numclusters;

	for (i = 0, j = 0; i < n; i++, j += p->_clustentries) {
		void *base = p->lut[i * p->_clustentries].vaddr;
		ssize_t relofs = (ssize_t) vaddr - (ssize_t) base;

		/* Given address, is out of the scope of the current cluster.*/
		if (vaddr < base || relofs >= p->_clustsize)
			continue;

		j = j + relofs / p->_objsize;
		/* KASSERT(j != 0, ("Cannot free object 0")); */
		netmap_obj_free(p, j);
		return;
	}
	D("address %p is not contained inside any cluster (%s)",
	    vaddr, p->name);
}

#define netmap_if_malloc(n, len)	netmap_obj_malloc(&(n)->pools[NETMAP_IF_POOL], len, NULL, NULL)
#define netmap_if_free(n, v)		netmap_obj_free_va(&(n)->pools[NETMAP_IF_POOL], (v))
#define netmap_ring_malloc(n, len)	netmap_obj_malloc(&(n)->pools[NETMAP_RING_POOL], len, NULL, NULL)
#define netmap_ring_free(n, v)		netmap_obj_free_va(&(n)->pools[NETMAP_RING_POOL], (v))
#define netmap_buf_malloc(n, _pos, _index)			\
	netmap_obj_malloc(&(n)->pools[NETMAP_BUF_POOL], NETMAP_BDG_BUF_SIZE(n), _pos, _index)


/* Return the index associated to the given packet buffer */
#define netmap_buf_index(n, v)						\
    (netmap_obj_offset(&(n)->pools[NETMAP_BUF_POOL], (v)) / NETMAP_BDG_BUF_SIZE(n))


/* Return nonzero on error */
static int
netmap_new_bufs(struct netmap_mem_d *nmd, struct netmap_slot *slot, u_int n)
{
	struct netmap_obj_pool *p = &nmd->pools[NETMAP_BUF_POOL];
	u_int i = 0;	/* slot counter */
	uint32_t pos = 0;	/* slot in p->bitmap */
	uint32_t index = 0;	/* buffer index */

	for (i = 0; i < n; i++) {
		void *vaddr = netmap_buf_malloc(nmd, &pos, &index);
		if (vaddr == NULL) {
			D("no more buffers after %d of %d", i, n);
			goto cleanup;
		}
		slot[i].buf_idx = index;
		slot[i].len = p->_objsize;
		slot[i].flags = 0;
	}

	ND("allocated %d buffers, %d available, first at %d", n, p->objfree, pos);
	return (0);

cleanup:
	while (i > 0) {
		i--;
		netmap_obj_free(p, slot[i].buf_idx);
	}
	bzero(slot, n * sizeof(slot[0]));
	return (ENOMEM);
}


static void
netmap_free_buf(struct netmap_mem_d *nmd, uint32_t i)
{
	struct netmap_obj_pool *p = &nmd->pools[NETMAP_BUF_POOL];

	if (i < 2 || i >= p->objtotal) {
		D("Cannot free buf#%d: should be in [2, %d[", i, p->objtotal);
		return;
	}
	netmap_obj_free(p, i);
}

static void
netmap_reset_obj_allocator(struct netmap_obj_pool *p)
{

	if (p == NULL)
		return;
	if (p->bitmap)
		free(p->bitmap, M_NETMAP);
	p->bitmap = NULL;
	if (p->lut) {
		u_int i;
		size_t sz = p->_clustsize;

		for (i = 0; i < p->objtotal; i += p->_clustentries) {
			if (p->lut[i].vaddr)
				contigfree(p->lut[i].vaddr, sz, M_NETMAP);
		}
		bzero(p->lut, sizeof(struct lut_entry) * p->objtotal);
#ifdef linux
		vfree(p->lut);
#else
		free(p->lut, M_NETMAP);
#endif
	}
	p->lut = NULL;
	p->objtotal = 0;
	p->memtotal = 0;
	p->numclusters = 0;
	p->objfree = 0;
}

/*
 * Free all resources related to an allocator.
 */
static void
netmap_destroy_obj_allocator(struct netmap_obj_pool *p)
{
	if (p == NULL)
		return;
	netmap_reset_obj_allocator(p);
}

/*
 * We receive a request for objtotal objects, of size objsize each.
 * Internally we may round up both numbers, as we allocate objects
 * in small clusters multiple of the page size.
 * We need to keep track of objtotal and clustentries,
 * as they are needed when freeing memory.
 *
 * XXX note -- userspace needs the buffers to be contiguous,
 *	so we cannot afford gaps at the end of a cluster.
 */


/* call with NMA_LOCK held */
static int
netmap_config_obj_allocator(struct netmap_obj_pool *p, u_int objtotal, u_int objsize)
{
	int i;
	u_int clustsize;	/* the cluster size, multiple of page size */
	u_int clustentries;	/* how many objects per entry */

	/* we store the current request, so we can
	 * detect configuration changes later */
	p->r_objtotal = objtotal;
	p->r_objsize = objsize;

#define MAX_CLUSTSIZE	(1<<17)
#define LINE_ROUND	64
	if (objsize >= MAX_CLUSTSIZE) {
		/* we could do it but there is no point */
		D("unsupported allocation for %d bytes", objsize);
		return EINVAL;
	}
	/* make sure objsize is a multiple of LINE_ROUND */
	i = (objsize & (LINE_ROUND - 1));
	if (i) {
		D("XXX aligning object by %d bytes", LINE_ROUND - i);
		objsize += LINE_ROUND - i;
	}
	if (objsize < p->objminsize || objsize > p->objmaxsize) {
		D("requested objsize %d out of range [%d, %d]",
			objsize, p->objminsize, p->objmaxsize);
		return EINVAL;
	}
	if (objtotal < p->nummin || objtotal > p->nummax) {
		D("requested objtotal %d out of range [%d, %d]",
			objtotal, p->nummin, p->nummax);
		return EINVAL;
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
	if (netmap_verbose)
		D("objsize %d clustsize %d objects %d",
			objsize, clustsize, clustentries);

	/*
	 * The number of clusters is n = ceil(objtotal/clustentries)
	 * objtotal' = n * clustentries
	 */
	p->_clustentries = clustentries;
	p->_clustsize = clustsize;
	p->_numclusters = (objtotal + clustentries - 1) / clustentries;

	/* actual values (may be larger than requested) */
	p->_objsize = objsize;
	p->_objtotal = p->_numclusters * clustentries;

	return 0;
}


/* call with NMA_LOCK held */
static int
netmap_finalize_obj_allocator(struct netmap_obj_pool *p)
{
	int i; /* must be signed */
	size_t n;

	/* optimistically assume we have enough memory */
	p->numclusters = p->_numclusters;
	p->objtotal = p->_objtotal;

	n = sizeof(struct lut_entry) * p->objtotal;
#ifdef linux
	p->lut = vmalloc(n);
#else
	p->lut = malloc(n, M_NETMAP, M_NOWAIT | M_ZERO);
#endif
	if (p->lut == NULL) {
		D("Unable to create lookup table (%d bytes) for '%s'", (int)n, p->name);
		goto clean;
	}

	/* Allocate the bitmap */
	n = (p->objtotal + 31) / 32;
	p->bitmap = malloc(sizeof(uint32_t) * n, M_NETMAP, M_NOWAIT | M_ZERO);
	if (p->bitmap == NULL) {
		D("Unable to create bitmap (%d entries) for allocator '%s'", (int)n,
		    p->name);
		goto clean;
	}
	p->bitmap_slots = n;

	/*
	 * Allocate clusters, init pointers and bitmap
	 */

	n = p->_clustsize;
	for (i = 0; i < (int)p->objtotal;) {
		int lim = i + p->_clustentries;
		char *clust;

		clust = contigmalloc(n, M_NETMAP, M_NOWAIT | M_ZERO,
		    (size_t)0, -1UL, PAGE_SIZE, 0);
		if (clust == NULL) {
			/*
			 * If we get here, there is a severe memory shortage,
			 * so halve the allocated memory to reclaim some.
			 */
			D("Unable to create cluster at %d for '%s' allocator",
			    i, p->name);
			if (i < 2) /* nothing to halve */
				goto out;
			lim = i / 2;
			for (i--; i >= lim; i--) {
				p->bitmap[ (i>>5) ] &=  ~( 1 << (i & 31) );
				if (i % p->_clustentries == 0 && p->lut[i].vaddr)
					contigfree(p->lut[i].vaddr,
						n, M_NETMAP);
			}
		out:
			p->objtotal = i;
			/* we may have stopped in the middle of a cluster */
			p->numclusters = (i + p->_clustentries - 1) / p->_clustentries;
			break;
		}
		for (; i < lim; i++, clust += p->_objsize) {
			p->bitmap[ (i>>5) ] |=  ( 1 << (i & 31) );
			p->lut[i].vaddr = clust;
			p->lut[i].paddr = vtophys(clust);
		}
	}
	p->objfree = p->objtotal;
	p->memtotal = p->numclusters * p->_clustsize;
	if (p->objfree == 0)
		goto clean;
	if (netmap_verbose)
		D("Pre-allocated %d clusters (%d/%dKB) for '%s'",
		    p->numclusters, p->_clustsize >> 10,
		    p->memtotal >> 10, p->name);

	return 0;

clean:
	netmap_reset_obj_allocator(p);
	return ENOMEM;
}

/* call with lock held */
static int
netmap_memory_config_changed(struct netmap_mem_d *nmd)
{
	int i;

	for (i = 0; i < NETMAP_POOLS_NR; i++) {
		if (nmd->pools[i].r_objsize != netmap_params[i].size ||
		    nmd->pools[i].r_objtotal != netmap_params[i].num)
		    return 1;
	}
	return 0;
}

static void
netmap_mem_reset_all(struct netmap_mem_d *nmd)
{
	int i;
	D("resetting %p", nmd);
	for (i = 0; i < NETMAP_POOLS_NR; i++) {
		netmap_reset_obj_allocator(&nmd->pools[i]);
	}
	nmd->flags  &= ~NETMAP_MEM_FINALIZED;
}

static int
netmap_mem_finalize_all(struct netmap_mem_d *nmd)
{
	int i;
	if (nmd->flags & NETMAP_MEM_FINALIZED)
		return 0;
	nmd->lasterr = 0;
	nmd->nm_totalsize = 0;
	for (i = 0; i < NETMAP_POOLS_NR; i++) {
		nmd->lasterr = netmap_finalize_obj_allocator(&nmd->pools[i]);
		if (nmd->lasterr)
			goto error;
		nmd->nm_totalsize += nmd->pools[i].memtotal;
	}
	/* buffers 0 and 1 are reserved */
	nmd->pools[NETMAP_BUF_POOL].objfree -= 2;
	nmd->pools[NETMAP_BUF_POOL].bitmap[0] = ~3;
	nmd->flags |= NETMAP_MEM_FINALIZED;

	D("Have %d KB for interfaces, %d KB for rings and %d MB for buffers",
	    nmd->pools[NETMAP_IF_POOL].memtotal >> 10,
	    nmd->pools[NETMAP_RING_POOL].memtotal >> 10,
	    nmd->pools[NETMAP_BUF_POOL].memtotal >> 20);

	D("Free buffers: %d", nmd->pools[NETMAP_BUF_POOL].objfree);


	return 0;
error:
	netmap_mem_reset_all(nmd);
	return nmd->lasterr;
}



void
netmap_mem_private_delete(struct netmap_mem_d *nmd)
{
	if (nmd == NULL)
		return;
	D("deleting %p", nmd);
	if (nmd->refcount > 0)
		D("bug: deleting mem allocator with refcount=%d!", nmd->refcount);
	D("done deleting %p", nmd);
	NMA_LOCK_DESTROY(nmd);
	free(nmd, M_DEVBUF);
}

static int
netmap_mem_private_config(struct netmap_mem_d *nmd)
{
	/* nothing to do, we are configured on creation
 	 * and configuration never changes thereafter
 	 */
	return 0;
}

static int
netmap_mem_private_finalize(struct netmap_mem_d *nmd)
{
	int err;
	NMA_LOCK(nmd);
	nmd->refcount++;
	err = netmap_mem_finalize_all(nmd);
	NMA_UNLOCK(nmd);
	return err;

}

static void
netmap_mem_private_deref(struct netmap_mem_d *nmd)
{
	NMA_LOCK(nmd);
	if (--nmd->refcount <= 0)
		netmap_mem_reset_all(nmd);
	NMA_UNLOCK(nmd);
}

struct netmap_mem_d *
netmap_mem_private_new(const char *name, u_int txr, u_int txd, u_int rxr, u_int rxd)
{
	struct netmap_mem_d *d = NULL;
	struct netmap_obj_params p[NETMAP_POOLS_NR];
	int i;
	u_int maxd;

	d = malloc(sizeof(struct netmap_mem_d),
			M_DEVBUF, M_NOWAIT | M_ZERO);
	if (d == NULL)
		return NULL;

	*d = nm_blueprint;

	/* XXX the rest of the code assumes the stack rings are alwasy present */
	txr++;
	rxr++;
	p[NETMAP_IF_POOL].size = sizeof(struct netmap_if) +
		sizeof(ssize_t) * (txr + rxr);
	p[NETMAP_IF_POOL].num = 2;
	maxd = (txd > rxd) ? txd : rxd;
	p[NETMAP_RING_POOL].size = sizeof(struct netmap_ring) +
		sizeof(struct netmap_slot) * maxd;
	p[NETMAP_RING_POOL].num = txr + rxr;
	p[NETMAP_BUF_POOL].size = 2048; /* XXX find a way to let the user choose this */
	p[NETMAP_BUF_POOL].num = rxr * (rxd + 2) + txr * (txd + 2);

	D("req if %d*%d ring %d*%d buf %d*%d",
			p[NETMAP_IF_POOL].num,
			p[NETMAP_IF_POOL].size,
			p[NETMAP_RING_POOL].num,
			p[NETMAP_RING_POOL].size,
			p[NETMAP_BUF_POOL].num,
			p[NETMAP_BUF_POOL].size);

	for (i = 0; i < NETMAP_POOLS_NR; i++) {
		snprintf(d->pools[i].name, NETMAP_POOL_MAX_NAMSZ,
				nm_blueprint.pools[i].name,
				name);
		if (netmap_config_obj_allocator(&d->pools[i],
				p[i].num, p[i].size))
			goto error;
	}

	d->flags &= ~NETMAP_MEM_FINALIZED;

	NMA_LOCK_INIT(d);

	return d;
error:
	netmap_mem_private_delete(d);
	return NULL;
}


/* call with lock held */
static int
netmap_mem_global_config(struct netmap_mem_d *nmd)
{
	int i;

	if (nmd->refcount)
		/* already in use, we cannot change the configuration */
		goto out;

	if (!netmap_memory_config_changed(nmd))
		goto out;

	D("reconfiguring");

	if (nmd->flags & NETMAP_MEM_FINALIZED) {
		/* reset previous allocation */
		for (i = 0; i < NETMAP_POOLS_NR; i++) {
			netmap_reset_obj_allocator(&nmd->pools[i]);
		}
		nmd->flags &= ~NETMAP_MEM_FINALIZED;
	}

	for (i = 0; i < NETMAP_POOLS_NR; i++) {
		nmd->lasterr = netmap_config_obj_allocator(&nmd->pools[i],
				netmap_params[i].num, netmap_params[i].size);
		if (nmd->lasterr)
			goto out;
	}

out:

	return nmd->lasterr;
}

static int
netmap_mem_global_finalize(struct netmap_mem_d *nmd)
{
	int err;

	NMA_LOCK(nmd);


	/* update configuration if changed */
	if (netmap_mem_global_config(nmd))
		goto out;

	nmd->refcount++;

	if (nmd->flags & NETMAP_MEM_FINALIZED) {
		/* may happen if config is not changed */
		ND("nothing to do");
		goto out;
	}

	if (netmap_mem_finalize_all(nmd))
		goto out;

	/* backward compatibility */
	netmap_buf_size = nmd->pools[NETMAP_BUF_POOL]._objsize;
	netmap_total_buffers = nmd->pools[NETMAP_BUF_POOL].objtotal;

	netmap_buffer_lut = nmd->pools[NETMAP_BUF_POOL].lut;
	netmap_buffer_base = nmd->pools[NETMAP_BUF_POOL].lut[0].vaddr;

	nmd->lasterr = 0;

out:
	if (nmd->lasterr)
		nmd->refcount--;
	err = nmd->lasterr;

	NMA_UNLOCK(nmd);

	return err;

}

int
netmap_mem_init(void)
{
	NMA_LOCK_INIT(&nm_mem);
	return (0);
}

void
netmap_mem_fini(void)
{
	int i;

	for (i = 0; i < NETMAP_POOLS_NR; i++) {
	    netmap_destroy_obj_allocator(&nm_mem.pools[i]);
	}
	NMA_LOCK_DESTROY(&nm_mem);
}

static void
netmap_free_rings(struct netmap_adapter *na)
{
	u_int i;
	if (!na->tx_rings)
		return;
	for (i = 0; i < na->num_tx_rings + 1; i++) {
		if (na->tx_rings[i].ring) {
			netmap_ring_free(na->nm_mem, na->tx_rings[i].ring);
			na->tx_rings[i].ring = NULL;
		}
	}
	for (i = 0; i < na->num_rx_rings + 1; i++) {
		if (na->rx_rings[i].ring) {
			netmap_ring_free(na->nm_mem, na->rx_rings[i].ring);
			na->rx_rings[i].ring = NULL;
		}
	}
}

/* call with NMA_LOCK held *
 *
 * Allocate netmap rings and buffers for this card
 * The rings are contiguous, but have variable size.
 */
int
netmap_mem_rings_create(struct netmap_adapter *na)
{
	struct netmap_ring *ring;
	u_int len, ndesc;
	struct netmap_kring *kring;

	NMA_LOCK(na->nm_mem);

	for (kring = na->tx_rings; kring != na->rx_rings; kring++) { /* Transmit rings */
		ndesc = kring->nkr_num_slots;
		len = sizeof(struct netmap_ring) +
			  ndesc * sizeof(struct netmap_slot);
		ring = netmap_ring_malloc(na->nm_mem, len);
		if (ring == NULL) {
			D("Cannot allocate tx_ring");
			goto cleanup;
		}
		ND("txring[%d] at %p ofs %d", i, ring);
		kring->ring = ring;
		*(uint32_t *)(uintptr_t)&ring->num_slots = ndesc;
		*(ssize_t *)(uintptr_t)&ring->buf_ofs =
		    (na->nm_mem->pools[NETMAP_IF_POOL].memtotal +
			na->nm_mem->pools[NETMAP_RING_POOL].memtotal) -
			netmap_ring_offset(na->nm_mem, ring);

		ring->avail = kring->nr_hwavail;
		ring->cur = kring->nr_hwcur;
		*(uint16_t *)(uintptr_t)&ring->nr_buf_size =
			NETMAP_BDG_BUF_SIZE(na->nm_mem);
		ND("initializing slots for txring");
		if (netmap_new_bufs(na->nm_mem, ring->slot, ndesc)) {
			D("Cannot allocate buffers for tx_ring");
			goto cleanup;
		}
	}

	for ( ; kring != na->tailroom; kring++) { /* Receive rings */
		ndesc = kring->nkr_num_slots;
		len = sizeof(struct netmap_ring) +
			  ndesc * sizeof(struct netmap_slot);
		ring = netmap_ring_malloc(na->nm_mem, len);
		if (ring == NULL) {
			D("Cannot allocate rx_ring");
			goto cleanup;
		}
		ND("rxring at %p ofs %d", ring);

		kring->ring = ring;
		*(uint32_t *)(uintptr_t)&ring->num_slots = ndesc;
		*(ssize_t *)(uintptr_t)&ring->buf_ofs =
		    (na->nm_mem->pools[NETMAP_IF_POOL].memtotal +
		        na->nm_mem->pools[NETMAP_RING_POOL].memtotal) -
			netmap_ring_offset(na->nm_mem, ring);

		ring->cur = kring->nr_hwcur;
		ring->avail = kring->nr_hwavail;
		*(int *)(uintptr_t)&ring->nr_buf_size =
			NETMAP_BDG_BUF_SIZE(na->nm_mem);
		ND("initializing slots for rxring[%d]", i);
		if (netmap_new_bufs(na->nm_mem, ring->slot, ndesc)) {
			D("Cannot allocate buffers for rx_ring");
			goto cleanup;
		}
	}

	NMA_UNLOCK(na->nm_mem);

	return 0;

cleanup:
	netmap_free_rings(na);

	NMA_UNLOCK(na->nm_mem);

	return ENOMEM;
}

void
netmap_mem_rings_delete(struct netmap_adapter *na)
{
	/* last instance, release bufs and rings */
	u_int i, lim;
	struct netmap_kring *kring;
	struct netmap_ring *ring;

	NMA_LOCK(na->nm_mem);

	for (kring = na->tx_rings; kring != na->tailroom; kring++) {
		ring = kring->ring;
		if (ring == NULL)
			continue;
		lim = kring->nkr_num_slots;
		for (i = 0; i < lim; i++)
			netmap_free_buf(na->nm_mem, ring->slot[i].buf_idx);
	}
	netmap_free_rings(na);

	NMA_UNLOCK(na->nm_mem);
}


/* call with NMA_LOCK held */
/*
 * Allocate the per-fd structure netmap_if.
 *
 * We assume that the configuration stored in na
 * (number of tx/rx rings and descs) does not change while
 * the interface is in netmap mode.
 */
struct netmap_if *
netmap_mem_if_new(const char *ifname, struct netmap_adapter *na)
{
	struct netmap_if *nifp;
	ssize_t base; /* handy for relative offsets between rings and nifp */
	u_int i, len, ntx, nrx;

	/*
	 * verify whether virtual port need the stack ring
	 */
	ntx = na->num_tx_rings + 1; /* shorthand, include stack ring */
	nrx = na->num_rx_rings + 1; /* shorthand, include stack ring */
	/*
	 * the descriptor is followed inline by an array of offsets
	 * to the tx and rx rings in the shared memory region.
	 * For virtual rx rings we also allocate an array of
	 * pointers to assign to nkr_leases.
	 */

	NMA_LOCK(na->nm_mem);

	len = sizeof(struct netmap_if) + (nrx + ntx) * sizeof(ssize_t);
	nifp = netmap_if_malloc(na->nm_mem, len);
	if (nifp == NULL) {
		NMA_UNLOCK(na->nm_mem);
		return NULL;
	}

	/* initialize base fields -- override const */
	*(u_int *)(uintptr_t)&nifp->ni_tx_rings = na->num_tx_rings;
	*(u_int *)(uintptr_t)&nifp->ni_rx_rings = na->num_rx_rings;
	strncpy(nifp->ni_name, ifname, (size_t)IFNAMSIZ);

	/*
	 * fill the slots for the rx and tx rings. They contain the offset
	 * between the ring and nifp, so the information is usable in
	 * userspace to reach the ring from the nifp.
	 */
	base = netmap_if_offset(na->nm_mem, nifp);
	for (i = 0; i < ntx; i++) {
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i] =
			netmap_ring_offset(na->nm_mem, na->tx_rings[i].ring) - base;
	}
	for (i = 0; i < nrx; i++) {
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i+ntx] =
			netmap_ring_offset(na->nm_mem, na->rx_rings[i].ring) - base;
	}

	NMA_UNLOCK(na->nm_mem);

	return (nifp);
}

void
netmap_mem_if_delete(struct netmap_adapter *na, struct netmap_if *nifp)
{
	if (nifp == NULL)
		/* nothing to do */
		return;
	NMA_LOCK(na->nm_mem);

	netmap_if_free(na->nm_mem, nifp);

	NMA_UNLOCK(na->nm_mem);
}

static void
netmap_mem_global_deref(struct netmap_mem_d *nmd)
{
	NMA_LOCK(nmd);

	nmd->refcount--;
	if (netmap_verbose)
		D("refcount = %d", nmd->refcount);

	NMA_UNLOCK(nmd);
}

int
netmap_mem_finalize(struct netmap_mem_d *nmd)
{
	return nmd->finalize(nmd);
}

void
netmap_mem_deref(struct netmap_mem_d *nmd)
{
	return nmd->deref(nmd);
}
