/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * $FreeBSD$
 */

#ifndef _BHND_BHNDB_PRIVATE_H_
#define _BHND_BHNDB_PRIVATE_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include "bhndbvar.h"

/*
 * Private bhndb(4) driver definitions.
 */

struct bhndb_dw_alloc;
struct bhndb_region;
struct bhndb_resources;

struct resource			*bhndb_find_resource_range(
				     struct bhndb_resources *br,
				     rman_res_t start, rman_res_t count);

struct resource			*bhndb_find_regwin_resource(
				     struct bhndb_resources *br,
				     const struct bhndb_regwin *win);

struct bhndb_resources		*bhndb_alloc_resources(device_t dev,
				     device_t parent_dev,
				     const struct bhndb_hwcfg *cfg);

void				 bhndb_free_resources(
				     struct bhndb_resources *br);

int				 bhndb_add_resource_region(
				     struct bhndb_resources *br,
				     bhnd_addr_t addr, bhnd_size_t size,
				     bhndb_priority_t priority,
				     const struct bhndb_regwin *static_regwin);

struct bhndb_region		*bhndb_find_resource_region(
				     struct bhndb_resources *br,
				     bhnd_addr_t addr, bhnd_size_t size);

struct bhndb_dw_alloc		*bhndb_dw_find_resource(
				     struct bhndb_resources *dr,
				     struct resource *r);
				     
struct bhndb_dw_alloc		*bhndb_dw_find_mapping(
				     struct bhndb_resources *br,
				     bhnd_addr_t addr, bhnd_size_t size);

int				 bhndb_dw_retain(
				     struct bhndb_resources *br,
				     struct bhndb_dw_alloc *dwa,
				     struct resource *res);

void				 bhndb_dw_release(
				     struct bhndb_resources *br,
				     struct bhndb_dw_alloc *dwa,
				     struct resource *res);

int				 bhndb_dw_set_addr(device_t dev,
				     struct bhndb_resources *br,
				     struct bhndb_dw_alloc *dwa,
				     bus_addr_t addr, bus_size_t size);

size_t				 bhndb_regwin_count(
				     const struct bhndb_regwin *table,
				     bhndb_regwin_type_t type);

const struct bhndb_regwin	*bhndb_regwin_find_type(
				     const struct bhndb_regwin *table,
				     bhndb_regwin_type_t type,
				     bus_size_t min_size);

const struct bhndb_regwin	*bhndb_regwin_find_core(
				     const struct bhndb_regwin *table,
				     bhnd_devclass_t class, int unit,
				     bhnd_port_type port_type, u_int port,
				     u_int region);


const struct bhndb_regwin	*bhndb_regwin_find_best(
				     const struct bhndb_regwin *table,
				     bhnd_devclass_t class, int unit,
				     bhnd_port_type port_type, u_int port,
				     u_int region, bus_size_t min_size);

bool				 bhndb_regwin_matches_device(
				     const struct bhndb_regwin *regw,
				     device_t dev);

const struct bhndb_hw_priority	*bhndb_hw_priority_find_device(
				     const struct bhndb_hw_priority *table,
				     device_t device);


/**
 * Dynamic register window allocation reference.
 */
struct bhndb_dw_rentry {
	struct resource			*dw_res;		/**< child resource */
	LIST_ENTRY(bhndb_dw_rentry)	 dw_link;
};

/**
 * A dynamic register window allocation record. 
 */
struct bhndb_dw_alloc {
	const struct bhndb_regwin	*win;		/**< window definition */
	struct resource			*parent_res;	/**< enclosing resource */
	u_int				 rnid;		/**< region identifier */
	rman_res_t			 target;	/**< the current window address, or 0x0 if unknown */

	LIST_HEAD(, bhndb_dw_rentry)	 refs;		/**< references */
};

/**
 * A bus address region description.
 */
struct bhndb_region {
	bhnd_addr_t			 addr;		/**< start of mapped range */
	bhnd_size_t			 size;		/**< size of mapped range */
	bhndb_priority_t		 priority;	/**< direct resource allocation priority */
	const struct bhndb_regwin	*static_regwin;	/**< fixed mapping regwin, if any */

	STAILQ_ENTRY(bhndb_region)	 link;
};

/**
 * BHNDB resource allocation state.
 */
struct bhndb_resources {
	device_t			 dev;		/**< bridge device */
	const struct bhndb_hwcfg	*cfg;		/**< hardware configuration */

	device_t			 parent_dev;	/**< parent device */
	struct resource_spec		*res_spec;	/**< parent bus resource specs */
	struct resource			**res;		/**< parent bus resources */
	
	struct rman			 ht_mem_rman;	/**< host memory manager */
	struct rman			 br_mem_rman;	/**< bridged memory manager */

	STAILQ_HEAD(, bhndb_region) 	 bus_regions;	/**< bus region descriptors */

	struct bhndb_dw_alloc		*dw_alloc;	/**< dynamic window allocation records */
	size_t				 dwa_count;	/**< number of dynamic windows available. */
	uint32_t			 dwa_freelist;	/**< dynamic window free list */
	bhndb_priority_t		 min_prio;	/**< minimum resource priority required to
							     allocate a dynamic window */
};

/**
 * Returns true if the all dynamic windows have been exhausted, false
 * otherwise.
 * 
 * @param br The resource state to check.
 */
static inline bool
bhndb_dw_exhausted(struct bhndb_resources *br)
{
	return (br->dwa_freelist == 0);
}

/**
 * Find the next free dynamic window region in @p br.
 * 
 * @param br The resource state to search.
 */
static inline struct bhndb_dw_alloc *
bhndb_dw_next_free(struct bhndb_resources *br)
{
	struct bhndb_dw_alloc *dw_free;

	if (bhndb_dw_exhausted(br))
		return (NULL);

	dw_free = &br->dw_alloc[__builtin_ctz(br->dwa_freelist)];

	KASSERT(LIST_EMPTY(&dw_free->refs),
	    ("free list out of sync with refs"));

	return (dw_free);
}

/**
 * Returns true if a dynamic window allocation is marked as free.
 * 
 * @param br The resource state owning @p dwa.
 * @param dwa The dynamic window allocation record to be checked.
 */
static inline bool
bhndb_dw_is_free(struct bhndb_resources *br, struct bhndb_dw_alloc *dwa)
{
	bool is_free = LIST_EMPTY(&dwa->refs);

	KASSERT(is_free == ((br->dwa_freelist & (1 << dwa->rnid)) != 0),
	    ("refs out of sync with free list"));

	return (is_free);
}


#define	BHNDB_LOCK_INIT(sc) \
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev), \
	    "bhndb resource allocator lock", MTX_DEF)
#define	BHNDB_LOCK(sc)			mtx_lock(&(sc)->sc_mtx)
#define	BHNDB_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define	BHNDB_LOCK_ASSERT(sc, what)	mtx_assert(&(sc)->sc_mtx, what)
#define	BHNDB_LOCK_DESTROY(sc)		mtx_destroy(&(sc)->sc_mtx)

#endif /* _BHND_BHNDB_PRIVATE_H_ */
