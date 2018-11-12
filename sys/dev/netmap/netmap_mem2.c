/*
 * Copyright (C) 2012-2014 Matteo Landi, Luigi Rizzo, Giuseppe Lettieri. All rights reserved.
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

#define NETMAP_BUF_MAX_NUM	20*4096*2	/* large machine */

#define NETMAP_POOL_MAX_NAMSZ	32


enum {
	NETMAP_IF_POOL   = 0,
	NETMAP_RING_POOL,
	NETMAP_BUF_POOL,
	NETMAP_POOLS_NR
};


struct netmap_obj_params {
	u_int size;
	u_int num;
};

struct netmap_obj_pool {
	char name[NETMAP_POOL_MAX_NAMSZ];	/* name of the allocator */

	/* ---------------------------------------------------*/
	/* these are only meaningful if the pool is finalized */
	/* (see 'finalized' field in netmap_mem_d)            */
	u_int objtotal;         /* actual total number of objects. */
	u_int memtotal;		/* actual total memory space */
	u_int numclusters;	/* actual number of clusters */

	u_int objfree;          /* number of free objects. */

	struct lut_entry *lut;  /* virt,phys addresses, objtotal entries */
	uint32_t *bitmap;       /* one bit per buffer, 1 means free */
	uint32_t bitmap_slots;	/* number of uint32 entries in bitmap */
	/* ---------------------------------------------------*/

	/* limits */
	u_int objminsize;	/* minimum object size */
	u_int objmaxsize;	/* maximum object size */
	u_int nummin;		/* minimum number of objects */
	u_int nummax;		/* maximum number of objects */

	/* these are changed only by config */
	u_int _objtotal;	/* total number of objects */
	u_int _objsize;		/* object size */
	u_int _clustsize;       /* cluster size */
	u_int _clustentries;    /* objects per cluster */
	u_int _numclusters;	/* number of clusters */

	/* requested values */
	u_int r_objtotal;
	u_int r_objsize;
};

#define NMA_LOCK_T		NM_MTX_T


struct netmap_mem_ops {
	void (*nmd_get_lut)(struct netmap_mem_d *, struct netmap_lut*);
	int  (*nmd_get_info)(struct netmap_mem_d *, u_int *size,
			u_int *memflags, uint16_t *id);

	vm_paddr_t (*nmd_ofstophys)(struct netmap_mem_d *, vm_ooffset_t);
	int (*nmd_config)(struct netmap_mem_d *);
	int (*nmd_finalize)(struct netmap_mem_d *);
	void (*nmd_deref)(struct netmap_mem_d *);
	ssize_t  (*nmd_if_offset)(struct netmap_mem_d *, const void *vaddr);
	void (*nmd_delete)(struct netmap_mem_d *);

	struct netmap_if * (*nmd_if_new)(struct netmap_adapter *);
	void (*nmd_if_delete)(struct netmap_adapter *, struct netmap_if *);
	int  (*nmd_rings_create)(struct netmap_adapter *);
	void (*nmd_rings_delete)(struct netmap_adapter *);
};

typedef uint16_t nm_memid_t;

struct netmap_mem_d {
	NMA_LOCK_T nm_mtx;  /* protect the allocator */
	u_int nm_totalsize; /* shorthand */

	u_int flags;
#define NETMAP_MEM_FINALIZED	0x1	/* preallocation done */
	int lasterr;		/* last error for curr config */
	int active;		/* active users */
	int refcount;
	/* the three allocators */
	struct netmap_obj_pool pools[NETMAP_POOLS_NR];

	nm_memid_t nm_id;	/* allocator identifier */
	int nm_grp;	/* iommu groupd id */

	/* list of all existing allocators, sorted by nm_id */
	struct netmap_mem_d *prev, *next;

	struct netmap_mem_ops *ops;
};

#define NMD_DEFCB(t0, name) \
t0 \
netmap_mem_##name(struct netmap_mem_d *nmd) \
{ \
	return nmd->ops->nmd_##name(nmd); \
}

#define NMD_DEFCB1(t0, name, t1) \
t0 \
netmap_mem_##name(struct netmap_mem_d *nmd, t1 a1) \
{ \
	return nmd->ops->nmd_##name(nmd, a1); \
}

#define NMD_DEFCB3(t0, name, t1, t2, t3) \
t0 \
netmap_mem_##name(struct netmap_mem_d *nmd, t1 a1, t2 a2, t3 a3) \
{ \
	return nmd->ops->nmd_##name(nmd, a1, a2, a3); \
}

#define NMD_DEFNACB(t0, name) \
t0 \
netmap_mem_##name(struct netmap_adapter *na) \
{ \
	return na->nm_mem->ops->nmd_##name(na); \
}

#define NMD_DEFNACB1(t0, name, t1) \
t0 \
netmap_mem_##name(struct netmap_adapter *na, t1 a1) \
{ \
	return na->nm_mem->ops->nmd_##name(na, a1); \
}

NMD_DEFCB1(void, get_lut, struct netmap_lut *);
NMD_DEFCB3(int, get_info, u_int *, u_int *, uint16_t *);
NMD_DEFCB1(vm_paddr_t, ofstophys, vm_ooffset_t);
static int netmap_mem_config(struct netmap_mem_d *);
NMD_DEFCB(int, config);
NMD_DEFCB1(ssize_t, if_offset, const void *);
NMD_DEFCB(void, delete);

NMD_DEFNACB(struct netmap_if *, if_new);
NMD_DEFNACB1(void, if_delete, struct netmap_if *);
NMD_DEFNACB(int, rings_create);
NMD_DEFNACB(void, rings_delete);

static int netmap_mem_map(struct netmap_obj_pool *, struct netmap_adapter *);
static int netmap_mem_unmap(struct netmap_obj_pool *, struct netmap_adapter *);
static int nm_mem_assign_group(struct netmap_mem_d *, struct device *);

#define NMA_LOCK_INIT(n)	NM_MTX_INIT((n)->nm_mtx)
#define NMA_LOCK_DESTROY(n)	NM_MTX_DESTROY((n)->nm_mtx)
#define NMA_LOCK(n)		NM_MTX_LOCK((n)->nm_mtx)
#define NMA_UNLOCK(n)		NM_MTX_UNLOCK((n)->nm_mtx)

#ifdef NM_DEBUG_MEM_PUTGET
#define NM_DBG_REFC(nmd, func, line)	\
	printf("%s:%d mem[%d] -> %d\n", func, line, (nmd)->nm_id, (nmd)->refcount);
#else
#define NM_DBG_REFC(nmd, func, line)
#endif

#ifdef NM_DEBUG_MEM_PUTGET
void __netmap_mem_get(struct netmap_mem_d *nmd, const char *func, int line)
#else
void netmap_mem_get(struct netmap_mem_d *nmd)
#endif
{
	NMA_LOCK(nmd);
	nmd->refcount++;
	NM_DBG_REFC(nmd, func, line);
	NMA_UNLOCK(nmd);
}

#ifdef NM_DEBUG_MEM_PUTGET
void __netmap_mem_put(struct netmap_mem_d *nmd, const char *func, int line)
#else
void netmap_mem_put(struct netmap_mem_d *nmd)
#endif
{
	int last;
	NMA_LOCK(nmd);
	last = (--nmd->refcount == 0);
	NM_DBG_REFC(nmd, func, line);
	NMA_UNLOCK(nmd);
	if (last)
		netmap_mem_delete(nmd);
}

int
netmap_mem_finalize(struct netmap_mem_d *nmd, struct netmap_adapter *na)
{
	if (nm_mem_assign_group(nmd, na->pdev) < 0) {
		return ENOMEM;
	} else {
		nmd->ops->nmd_finalize(nmd);
	}

	if (!nmd->lasterr && na->pdev)
		netmap_mem_map(&nmd->pools[NETMAP_BUF_POOL], na);

	return nmd->lasterr;
}

void
netmap_mem_deref(struct netmap_mem_d *nmd, struct netmap_adapter *na)
{
	NMA_LOCK(nmd);
	netmap_mem_unmap(&nmd->pools[NETMAP_BUF_POOL], na);
	NMA_UNLOCK(nmd);
	return nmd->ops->nmd_deref(nmd);
}


/* accessor functions */
static void
netmap_mem2_get_lut(struct netmap_mem_d *nmd, struct netmap_lut *lut)
{
	lut->lut = nmd->pools[NETMAP_BUF_POOL].lut;
	lut->objtotal = nmd->pools[NETMAP_BUF_POOL].objtotal;
	lut->objsize = nmd->pools[NETMAP_BUF_POOL]._objsize;
}

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

struct netmap_obj_params netmap_min_priv_params[NETMAP_POOLS_NR] = {
	[NETMAP_IF_POOL] = {
		.size = 1024,
		.num  = 1,
	},
	[NETMAP_RING_POOL] = {
		.size = 5*PAGE_SIZE,
		.num  = 4,
	},
	[NETMAP_BUF_POOL] = {
		.size = 2048,
		.num  = 4098,
	},
};


/*
 * nm_mem is the memory allocator used for all physical interfaces
 * running in netmap mode.
 * Virtual (VALE) ports will have each its own allocator.
 */
extern struct netmap_mem_ops netmap_mem_global_ops; /* forward */
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

	.nm_id = 1,
	.nm_grp = -1,

	.prev = &nm_mem,
	.next = &nm_mem,

	.ops = &netmap_mem_global_ops
};


struct netmap_mem_d *netmap_last_mem_d = &nm_mem;

/* blueprint for the private memory allocators */
extern struct netmap_mem_ops netmap_mem_private_ops; /* forward */
const struct netmap_mem_d nm_blueprint = {
	.pools = {
		[NETMAP_IF_POOL] = {
			.name 	= "%s_if",
			.objminsize = sizeof(struct netmap_if),
			.objmaxsize = 4096,
			.nummin     = 1,
			.nummax	    = 100,
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

	.flags = NETMAP_MEM_PRIVATE,

	.ops = &netmap_mem_private_ops
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
	    CTLFLAG_RD, &nm_mem.pools[id].objtotal, 0, "Current number of netmap " STRINGIFY(name) "s"); \
	SYSCTL_INT(_dev_netmap, OID_AUTO, priv_##name##_size, \
	    CTLFLAG_RW, &netmap_min_priv_params[id].size, 0, \
	    "Default size of private netmap " STRINGIFY(name) "s"); \
	SYSCTL_INT(_dev_netmap, OID_AUTO, priv_##name##_num, \
	    CTLFLAG_RW, &netmap_min_priv_params[id].num, 0, \
	    "Default number of private netmap " STRINGIFY(name) "s")

SYSCTL_DECL(_dev_netmap);
DECLARE_SYSCTLS(NETMAP_IF_POOL, if);
DECLARE_SYSCTLS(NETMAP_RING_POOL, ring);
DECLARE_SYSCTLS(NETMAP_BUF_POOL, buf);

static int
nm_mem_assign_id(struct netmap_mem_d *nmd)
{
	nm_memid_t id;
	struct netmap_mem_d *scan = netmap_last_mem_d;
	int error = ENOMEM;

	NMA_LOCK(&nm_mem);

	do {
		/* we rely on unsigned wrap around */
		id = scan->nm_id + 1;
		if (id == 0) /* reserve 0 as error value */
			id = 1;
		scan = scan->next;
		if (id != scan->nm_id) {
			nmd->nm_id = id;
			nmd->prev = scan->prev;
			nmd->next = scan;
			scan->prev->next = nmd;
			scan->prev = nmd;
			netmap_last_mem_d = nmd;
			error = 0;
			break;
		}
	} while (scan != netmap_last_mem_d);

	NMA_UNLOCK(&nm_mem);
	return error;
}

static void
nm_mem_release_id(struct netmap_mem_d *nmd)
{
	NMA_LOCK(&nm_mem);

	nmd->prev->next = nmd->next;
	nmd->next->prev = nmd->prev;

	if (netmap_last_mem_d == nmd)
		netmap_last_mem_d = nmd->prev;

	nmd->prev = nmd->next = NULL;

	NMA_UNLOCK(&nm_mem);
}

static int
nm_mem_assign_group(struct netmap_mem_d *nmd, struct device *dev)
{
	int err = 0, id;
	id = nm_iommu_group_id(dev);
	if (netmap_verbose)
		D("iommu_group %d", id);

	NMA_LOCK(nmd);

	if (nmd->nm_grp < 0)
		nmd->nm_grp = id;

	if (nmd->nm_grp != id)
		nmd->lasterr = err = ENOMEM;

	NMA_UNLOCK(nmd);
	return err;
}

/*
 * First, find the allocator that contains the requested offset,
 * then locate the cluster through a lookup table.
 */
static vm_paddr_t
netmap_mem2_ofstophys(struct netmap_mem_d* nmd, vm_ooffset_t offset)
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
		pa = vtophys(p[i].lut[offset / p[i]._objsize].vaddr) +
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

static int
netmap_mem2_get_info(struct netmap_mem_d* nmd, u_int* size, u_int *memflags,
	nm_memid_t *id)
{
	int error = 0;
	NMA_LOCK(nmd);
	error = netmap_mem_config(nmd);
	if (error)
		goto out;
	if (size) {
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
	}
	if (memflags)
		*memflags = nmd->flags;
	if (id)
		*id = nmd->nm_id;
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


static ssize_t
netmap_mem2_if_offset(struct netmap_mem_d *nmd, const void *addr)
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
 * free by index, not by address.
 * XXX should we also cleanup the content ?
 */
static int
netmap_obj_free(struct netmap_obj_pool *p, uint32_t j)
{
	uint32_t *ptr, mask;

	if (j >= p->objtotal) {
		D("invalid index %u, max %u", j, p->objtotal);
		return 1;
	}
	ptr = &p->bitmap[j / 32];
	mask = (1 << (j % 32));
	if (*ptr & mask) {
		D("ouch, double free on buffer %d", j);
		return 1;
	} else {
		*ptr |= mask;
		p->objfree++;
		return 0;
	}
}

/*
 * free by address. This is slow but is only used for a few
 * objects (rings, nifp)
 */
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

#define netmap_mem_bufsize(n)	\
	((n)->pools[NETMAP_BUF_POOL]._objsize)

#define netmap_if_malloc(n, len)	netmap_obj_malloc(&(n)->pools[NETMAP_IF_POOL], len, NULL, NULL)
#define netmap_if_free(n, v)		netmap_obj_free_va(&(n)->pools[NETMAP_IF_POOL], (v))
#define netmap_ring_malloc(n, len)	netmap_obj_malloc(&(n)->pools[NETMAP_RING_POOL], len, NULL, NULL)
#define netmap_ring_free(n, v)		netmap_obj_free_va(&(n)->pools[NETMAP_RING_POOL], (v))
#define netmap_buf_malloc(n, _pos, _index)			\
	netmap_obj_malloc(&(n)->pools[NETMAP_BUF_POOL], netmap_mem_bufsize(n), _pos, _index)


#if 0 // XXX unused
/* Return the index associated to the given packet buffer */
#define netmap_buf_index(n, v)						\
    (netmap_obj_offset(&(n)->pools[NETMAP_BUF_POOL], (v)) / NETMAP_BDG_BUF_SIZE(n))
#endif

/*
 * allocate extra buffers in a linked list.
 * returns the actual number.
 */
uint32_t
netmap_extra_alloc(struct netmap_adapter *na, uint32_t *head, uint32_t n)
{
	struct netmap_mem_d *nmd = na->nm_mem;
	uint32_t i, pos = 0; /* opaque, scan position in the bitmap */

	NMA_LOCK(nmd);

	*head = 0;	/* default, 'null' index ie empty list */
	for (i = 0 ; i < n; i++) {
		uint32_t cur = *head;	/* save current head */
		uint32_t *p = netmap_buf_malloc(nmd, &pos, head);
		if (p == NULL) {
			D("no more buffers after %d of %d", i, n);
			*head = cur; /* restore */
			break;
		}
		RD(5, "allocate buffer %d -> %d", *head, cur);
		*p = cur; /* link to previous head */
	}

	NMA_UNLOCK(nmd);

	return i;
}

static void
netmap_extra_free(struct netmap_adapter *na, uint32_t head)
{
        struct lut_entry *lut = na->na_lut.lut;
	struct netmap_mem_d *nmd = na->nm_mem;
	struct netmap_obj_pool *p = &nmd->pools[NETMAP_BUF_POOL];
	uint32_t i, cur, *buf;

	D("freeing the extra list");
	for (i = 0; head >=2 && head < p->objtotal; i++) {
		cur = head;
		buf = lut[head].vaddr;
		head = *buf;
		*buf = 0;
		if (netmap_obj_free(p, cur))
			break;
	}
	if (head != 0)
		D("breaking with head %d", head);
	D("freed %d buffers", i);
}


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
netmap_mem_set_ring(struct netmap_mem_d *nmd, struct netmap_slot *slot, u_int n, uint32_t index)
{
	struct netmap_obj_pool *p = &nmd->pools[NETMAP_BUF_POOL];
	u_int i;

	for (i = 0; i < n; i++) {
		slot[i].buf_idx = index;
		slot[i].len = p->_objsize;
		slot[i].flags = 0;
	}
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
netmap_free_bufs(struct netmap_mem_d *nmd, struct netmap_slot *slot, u_int n)
{
	u_int i;

	for (i = 0; i < n; i++) {
		if (slot[i].buf_idx > 2)
			netmap_free_buf(nmd, slot[i].buf_idx);
	}
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

		/*
		 * Free each cluster allocated in
		 * netmap_finalize_obj_allocator().  The cluster start
		 * addresses are stored at multiples of p->_clusterentries
		 * in the lut.
		 */
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

#define MAX_CLUSTSIZE	(1<<22)		// 4 MB
#define LINE_ROUND	NM_CACHE_ALIGN	// 64
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
	}
	/* exact solution not found */
	if (clustentries == 0) {
		D("unsupported allocation for %d bytes", objsize);
		return EINVAL;
	}
	/* compute clustsize */
	clustsize = clustentries * objsize;
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
				p->lut[i].vaddr = NULL;
			}
		out:
			p->objtotal = i;
			/* we may have stopped in the middle of a cluster */
			p->numclusters = (i + p->_clustentries - 1) / p->_clustentries;
			break;
		}
		/*
		 * Set bitmap and lut state for all buffers in the current
		 * cluster.
		 *
		 * [i, lim) is the set of buffer indexes that cover the
		 * current cluster.
		 *
		 * 'clust' is really the address of the current buffer in
		 * the current cluster as we index through it with a stride
		 * of p->_objsize.
		 */
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

	if (netmap_verbose)
		D("resetting %p", nmd);
	for (i = 0; i < NETMAP_POOLS_NR; i++) {
		netmap_reset_obj_allocator(&nmd->pools[i]);
	}
	nmd->flags  &= ~NETMAP_MEM_FINALIZED;
}

static int
netmap_mem_unmap(struct netmap_obj_pool *p, struct netmap_adapter *na)
{
	int i, lim = p->_objtotal;

	if (na->pdev == NULL)
		return 0;

#ifdef __FreeBSD__
	(void)i;
	(void)lim;
	D("unsupported on FreeBSD");
#else /* linux */
	for (i = 2; i < lim; i++) {
		netmap_unload_map(na, (bus_dma_tag_t) na->pdev, &p->lut[i].paddr);
	}
#endif /* linux */

	return 0;
}

static int
netmap_mem_map(struct netmap_obj_pool *p, struct netmap_adapter *na)
{
#ifdef __FreeBSD__
	D("unsupported on FreeBSD");
#else /* linux */
	int i, lim = p->_objtotal;

	if (na->pdev == NULL)
		return 0;

	for (i = 2; i < lim; i++) {
		netmap_load_map(na, (bus_dma_tag_t) na->pdev, &p->lut[i].paddr,
				p->lut[i].vaddr);
	}
#endif /* linux */

	return 0;
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

	if (netmap_verbose)
		D("interfaces %d KB, rings %d KB, buffers %d MB",
		    nmd->pools[NETMAP_IF_POOL].memtotal >> 10,
		    nmd->pools[NETMAP_RING_POOL].memtotal >> 10,
		    nmd->pools[NETMAP_BUF_POOL].memtotal >> 20);

	if (netmap_verbose)
		D("Free buffers: %d", nmd->pools[NETMAP_BUF_POOL].objfree);


	return 0;
error:
	netmap_mem_reset_all(nmd);
	return nmd->lasterr;
}



static void
netmap_mem_private_delete(struct netmap_mem_d *nmd)
{
	if (nmd == NULL)
		return;
	if (netmap_verbose)
		D("deleting %p", nmd);
	if (nmd->active > 0)
		D("bug: deleting mem allocator with active=%d!", nmd->active);
	nm_mem_release_id(nmd);
	if (netmap_verbose)
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
	nmd->active++;
	err = netmap_mem_finalize_all(nmd);
	NMA_UNLOCK(nmd);
	return err;

}

static void
netmap_mem_private_deref(struct netmap_mem_d *nmd)
{
	NMA_LOCK(nmd);
	if (--nmd->active <= 0)
		netmap_mem_reset_all(nmd);
	NMA_UNLOCK(nmd);
}


/*
 * allocator for private memory
 */
struct netmap_mem_d *
netmap_mem_private_new(const char *name, u_int txr, u_int txd,
	u_int rxr, u_int rxd, u_int extra_bufs, u_int npipes, int *perr)
{
	struct netmap_mem_d *d = NULL;
	struct netmap_obj_params p[NETMAP_POOLS_NR];
	int i, err;
	u_int v, maxd;

	d = malloc(sizeof(struct netmap_mem_d),
			M_DEVBUF, M_NOWAIT | M_ZERO);
	if (d == NULL) {
		err = ENOMEM;
		goto error;
	}

	*d = nm_blueprint;

	err = nm_mem_assign_id(d);
	if (err)
		goto error;

	/* account for the fake host rings */
	txr++;
	rxr++;

	/* copy the min values */
	for (i = 0; i < NETMAP_POOLS_NR; i++) {
		p[i] = netmap_min_priv_params[i];
	}

	/* possibly increase them to fit user request */
	v = sizeof(struct netmap_if) + sizeof(ssize_t) * (txr + rxr);
	if (p[NETMAP_IF_POOL].size < v)
		p[NETMAP_IF_POOL].size = v;
	v = 2 + 4 * npipes;
	if (p[NETMAP_IF_POOL].num < v)
		p[NETMAP_IF_POOL].num = v;
	maxd = (txd > rxd) ? txd : rxd;
	v = sizeof(struct netmap_ring) + sizeof(struct netmap_slot) * maxd;
	if (p[NETMAP_RING_POOL].size < v)
		p[NETMAP_RING_POOL].size = v;
	/* each pipe endpoint needs two tx rings (1 normal + 1 host, fake)
         * and two rx rings (again, 1 normal and 1 fake host)
         */
	v = txr + rxr + 8 * npipes;
	if (p[NETMAP_RING_POOL].num < v)
		p[NETMAP_RING_POOL].num = v;
	/* for each pipe we only need the buffers for the 4 "real" rings.
         * On the other end, the pipe ring dimension may be different from
         * the parent port ring dimension. As a compromise, we allocate twice the
         * space actually needed if the pipe rings were the same size as the parent rings
         */
	v = (4 * npipes + rxr) * rxd + (4 * npipes + txr) * txd + 2 + extra_bufs;
		/* the +2 is for the tx and rx fake buffers (indices 0 and 1) */
	if (p[NETMAP_BUF_POOL].num < v)
		p[NETMAP_BUF_POOL].num = v;

	if (netmap_verbose)
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
		err = netmap_config_obj_allocator(&d->pools[i],
				p[i].num, p[i].size);
		if (err)
			goto error;
	}

	d->flags &= ~NETMAP_MEM_FINALIZED;

	NMA_LOCK_INIT(d);

	return d;
error:
	netmap_mem_private_delete(d);
	if (perr)
		*perr = err;
	return NULL;
}


/* call with lock held */
static int
netmap_mem_global_config(struct netmap_mem_d *nmd)
{
	int i;

	if (nmd->active)
		/* already in use, we cannot change the configuration */
		goto out;

	if (!netmap_memory_config_changed(nmd))
		goto out;

	ND("reconfiguring");

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
		
	/* update configuration if changed */
	if (netmap_mem_global_config(nmd))
		goto out;

	nmd->active++;

	if (nmd->flags & NETMAP_MEM_FINALIZED) {
		/* may happen if config is not changed */
		ND("nothing to do");
		goto out;
	}

	if (netmap_mem_finalize_all(nmd))
		goto out;

	nmd->lasterr = 0;

out:
	if (nmd->lasterr)
		nmd->active--;
	err = nmd->lasterr;

	return err;

}

static void
netmap_mem_global_delete(struct netmap_mem_d *nmd)
{
	int i;

	for (i = 0; i < NETMAP_POOLS_NR; i++) {
	    netmap_destroy_obj_allocator(&nm_mem.pools[i]);
	}

	NMA_LOCK_DESTROY(&nm_mem);
}

int
netmap_mem_init(void)
{
	NMA_LOCK_INIT(&nm_mem);
	netmap_mem_get(&nm_mem);
	return (0);
}

void
netmap_mem_fini(void)
{
	netmap_mem_put(&nm_mem);
}

static void
netmap_free_rings(struct netmap_adapter *na)
{
	enum txrx t;

	for_rx_tx(t) {
		u_int i;
		for (i = 0; i < netmap_real_rings(na, t); i++) {
			struct netmap_kring *kring = &NMR(na, t)[i];
			struct netmap_ring *ring = kring->ring;

			if (ring == NULL)
				continue;
			netmap_free_bufs(na->nm_mem, ring->slot, kring->nkr_num_slots);
			netmap_ring_free(na->nm_mem, ring);
			kring->ring = NULL;
		}
	}
}

/* call with NMA_LOCK held *
 *
 * Allocate netmap rings and buffers for this card
 * The rings are contiguous, but have variable size.
 * The kring array must follow the layout described
 * in netmap_krings_create().
 */
static int
netmap_mem2_rings_create(struct netmap_adapter *na)
{
	enum txrx t;

	NMA_LOCK(na->nm_mem);

	for_rx_tx(t) {
		u_int i;

		for (i = 0; i <= nma_get_nrings(na, t); i++) {
			struct netmap_kring *kring = &NMR(na, t)[i];
			struct netmap_ring *ring = kring->ring;
			u_int len, ndesc;

			if (ring) {
				ND("%s already created", kring->name);
				continue; /* already created by somebody else */
			}
			ndesc = kring->nkr_num_slots;
			len = sizeof(struct netmap_ring) +
				  ndesc * sizeof(struct netmap_slot);
			ring = netmap_ring_malloc(na->nm_mem, len);
			if (ring == NULL) {
				D("Cannot allocate %s_ring", nm_txrx2str(t));
				goto cleanup;
			}
			ND("txring at %p", ring);
			kring->ring = ring;
			*(uint32_t *)(uintptr_t)&ring->num_slots = ndesc;
			*(int64_t *)(uintptr_t)&ring->buf_ofs =
			    (na->nm_mem->pools[NETMAP_IF_POOL].memtotal +
				na->nm_mem->pools[NETMAP_RING_POOL].memtotal) -
				netmap_ring_offset(na->nm_mem, ring);

			/* copy values from kring */
			ring->head = kring->rhead;
			ring->cur = kring->rcur;
			ring->tail = kring->rtail;
			*(uint16_t *)(uintptr_t)&ring->nr_buf_size =
				netmap_mem_bufsize(na->nm_mem);
			ND("%s h %d c %d t %d", kring->name,
				ring->head, ring->cur, ring->tail);
			ND("initializing slots for %s_ring", nm_txrx2str(txrx));
			if (i != nma_get_nrings(na, t) || (na->na_flags & NAF_HOST_RINGS)) {
				/* this is a real ring */
				if (netmap_new_bufs(na->nm_mem, ring->slot, ndesc)) {
					D("Cannot allocate buffers for %s_ring", nm_txrx2str(t));
					goto cleanup;
				}
			} else {
				/* this is a fake ring, set all indices to 0 */
				netmap_mem_set_ring(na->nm_mem, ring->slot, ndesc, 0);
			}
		        /* ring info */
		        *(uint16_t *)(uintptr_t)&ring->ringid = kring->ring_id;
		        *(uint16_t *)(uintptr_t)&ring->dir = kring->tx;
		}
	}

	NMA_UNLOCK(na->nm_mem);

	return 0;

cleanup:
	netmap_free_rings(na);

	NMA_UNLOCK(na->nm_mem);

	return ENOMEM;
}

static void
netmap_mem2_rings_delete(struct netmap_adapter *na)
{
	/* last instance, release bufs and rings */
	NMA_LOCK(na->nm_mem);

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
static struct netmap_if *
netmap_mem2_if_new(struct netmap_adapter *na)
{
	struct netmap_if *nifp;
	ssize_t base; /* handy for relative offsets between rings and nifp */
	u_int i, len, n[NR_TXRX], ntot;
	enum txrx t;

	ntot = 0;
	for_rx_tx(t) {
		/* account for the (eventually fake) host rings */
		n[t] = nma_get_nrings(na, t) + 1;
		ntot += n[t];
	}
	/*
	 * the descriptor is followed inline by an array of offsets
	 * to the tx and rx rings in the shared memory region.
	 */

	NMA_LOCK(na->nm_mem);

	len = sizeof(struct netmap_if) + (ntot * sizeof(ssize_t));
	nifp = netmap_if_malloc(na->nm_mem, len);
	if (nifp == NULL) {
		NMA_UNLOCK(na->nm_mem);
		return NULL;
	}

	/* initialize base fields -- override const */
	*(u_int *)(uintptr_t)&nifp->ni_tx_rings = na->num_tx_rings;
	*(u_int *)(uintptr_t)&nifp->ni_rx_rings = na->num_rx_rings;
	strncpy(nifp->ni_name, na->name, (size_t)IFNAMSIZ);

	/*
	 * fill the slots for the rx and tx rings. They contain the offset
	 * between the ring and nifp, so the information is usable in
	 * userspace to reach the ring from the nifp.
	 */
	base = netmap_if_offset(na->nm_mem, nifp);
	for (i = 0; i < n[NR_TX]; i++) {
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i] =
			netmap_ring_offset(na->nm_mem, na->tx_rings[i].ring) - base;
	}
	for (i = 0; i < n[NR_RX]; i++) {
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i+n[NR_TX]] =
			netmap_ring_offset(na->nm_mem, na->rx_rings[i].ring) - base;
	}

	NMA_UNLOCK(na->nm_mem);

	return (nifp);
}

static void
netmap_mem2_if_delete(struct netmap_adapter *na, struct netmap_if *nifp)
{
	if (nifp == NULL)
		/* nothing to do */
		return;
	NMA_LOCK(na->nm_mem);
	if (nifp->ni_bufs_head)
		netmap_extra_free(na, nifp->ni_bufs_head);
	netmap_if_free(na->nm_mem, nifp);

	NMA_UNLOCK(na->nm_mem);
}

static void
netmap_mem_global_deref(struct netmap_mem_d *nmd)
{

	nmd->active--;
	if (!nmd->active)
		nmd->nm_grp = -1;
	if (netmap_verbose)
		D("active = %d", nmd->active);

}

struct netmap_mem_ops netmap_mem_global_ops = {
	.nmd_get_lut = netmap_mem2_get_lut,
	.nmd_get_info = netmap_mem2_get_info,
	.nmd_ofstophys = netmap_mem2_ofstophys,
	.nmd_config = netmap_mem_global_config,
	.nmd_finalize = netmap_mem_global_finalize,
	.nmd_deref = netmap_mem_global_deref,
	.nmd_delete = netmap_mem_global_delete,
	.nmd_if_offset = netmap_mem2_if_offset,
	.nmd_if_new = netmap_mem2_if_new,
	.nmd_if_delete = netmap_mem2_if_delete,
	.nmd_rings_create = netmap_mem2_rings_create,
	.nmd_rings_delete = netmap_mem2_rings_delete
};
struct netmap_mem_ops netmap_mem_private_ops = {
	.nmd_get_lut = netmap_mem2_get_lut,
	.nmd_get_info = netmap_mem2_get_info,
	.nmd_ofstophys = netmap_mem2_ofstophys,
	.nmd_config = netmap_mem_private_config,
	.nmd_finalize = netmap_mem_private_finalize,
	.nmd_deref = netmap_mem_private_deref,
	.nmd_if_offset = netmap_mem2_if_offset,
	.nmd_delete = netmap_mem_private_delete,
	.nmd_if_new = netmap_mem2_if_new,
	.nmd_if_delete = netmap_mem2_if_delete,
	.nmd_rings_create = netmap_mem2_rings_create,
	.nmd_rings_delete = netmap_mem2_rings_delete
};
