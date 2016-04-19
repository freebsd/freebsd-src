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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>

#include "bhndb_private.h"
#include "bhndbvar.h"

/**
 * Attach a BHND bridge device to @p parent.
 * 
 * @param parent A parent PCI device.
 * @param[out] bhndb On success, the probed and attached bhndb bridge device.
 * @param unit The device unit number, or -1 to select the next available unit
 * number.
 * 
 * @retval 0 success
 * @retval non-zero Failed to attach the bhndb device.
 */
int
bhndb_attach_bridge(device_t parent, device_t *bhndb, int unit)
{
	int error;

	*bhndb = device_add_child(parent, devclass_get_name(bhndb_devclass),
	    unit);
	if (*bhndb == NULL)
		return (ENXIO);

	if (!(error = device_probe_and_attach(*bhndb)))
		return (0);

	if ((device_delete_child(parent, *bhndb)))
		device_printf(parent, "failed to detach bhndb child\n");

	return (error);
}

/*
 * Call BHNDB_SUSPEND_RESOURCE() for all resources in @p rl.
 */
static void
bhndb_do_suspend_resources(device_t dev, struct resource_list *rl)
{
	struct resource_list_entry *rle;

	/* Suspend all child resources. */
	STAILQ_FOREACH(rle, rl, link) {
		/* Skip non-allocated resources */
		if (rle->res == NULL)
			continue;

		BHNDB_SUSPEND_RESOURCE(device_get_parent(dev), dev, rle->type,
		    rle->res);
	}
}

/**
 * Helper function for implementing BUS_RESUME_CHILD() on bridged
 * bhnd(4) buses.
 * 
 * This implementation of BUS_RESUME_CHILD() uses BUS_GET_RESOURCE_LIST()
 * to find the child's resources and call BHNDB_SUSPEND_RESOURCE() for all
 * child resources, ensuring that the device's allocated bridge resources
 * will be available to other devices during bus resumption.
 * 
 * Before suspending any resources, @p child is suspended by 
 * calling bhnd_generic_suspend_child().
 * 
 * If @p child is not a direct child of @p dev, suspension is delegated to
 * the @p dev parent.
 */
int
bhnd_generic_br_suspend_child(device_t dev, device_t child)
{
	struct resource_list		*rl;
	int				 error;

	if (device_get_parent(child) != dev)
		BUS_SUSPEND_CHILD(device_get_parent(dev), child);

	if (device_is_suspended(child))
		return (EBUSY);

	/* Suspend the child device */
	if ((error = bhnd_generic_suspend_child(dev, child)))
		return (error);

	/* Fetch the resource list. If none, there's nothing else to do */
	rl = BUS_GET_RESOURCE_LIST(device_get_parent(child), child);
	if (rl == NULL)
		return (0);

	/* Suspend all child resources. */
	bhndb_do_suspend_resources(dev, rl);

	return (0);
}

/**
 * Helper function for implementing BUS_RESUME_CHILD() on bridged
 * bhnd(4) bus devices.
 * 
 * This implementation of BUS_RESUME_CHILD() uses BUS_GET_RESOURCE_LIST()
 * to find the child's resources and call BHNDB_RESUME_RESOURCE() for all
 * child resources, before delegating to bhnd_generic_resume_child().
 * 
 * If resource resumption fails, @p child will not be resumed.
 * 
 * If @p child is not a direct child of @p dev, suspension is delegated to
 * the @p dev parent.
 */
int
bhnd_generic_br_resume_child(device_t dev, device_t child)
{
	struct resource_list		*rl;
	struct resource_list_entry	*rle;
	int				 error;
	
	if (device_get_parent(child) != dev)
		BUS_RESUME_CHILD(device_get_parent(dev), child);

	if (!device_is_suspended(child))
		return (EBUSY);

	/* Fetch the resource list. If none, there's nothing else to do */
	rl = BUS_GET_RESOURCE_LIST(device_get_parent(child), child);
	if (rl == NULL)
		return (bhnd_generic_resume_child(dev, child));

	/* Resume all resources */
	STAILQ_FOREACH(rle, rl, link) {
		/* Skip non-allocated resources */
		if (rle->res == NULL)
			continue;

		error = BHNDB_RESUME_RESOURCE(device_get_parent(dev), dev,
		    rle->type, rle->res);
		if (error) {
			/* Put all resources back into a suspend state */
			bhndb_do_suspend_resources(dev, rl);
			return (error);
		}
	}

	/* Now that all resources are resumed, resume child */
	if ((error = bhnd_generic_resume_child(dev, child))) {
		/* Put all resources back into a suspend state */
		bhndb_do_suspend_resources(dev, rl);
	}

	return (error);
}

/**
 * Find a SYS_RES_MEMORY resource containing the given address range.
 * 
 * @param br The bhndb resource state to search.
 * @param start The start address of the range to search for.
 * @param count The size of the range to search for.
 * 
 * @retval resource the host resource containing the requested range.
 * @retval NULL if no resource containing the requested range can be found.
 */
struct resource *
bhndb_find_resource_range(struct bhndb_resources *br, rman_res_t start,
     rman_res_t count)
{
	for (u_int i = 0; br->res_spec[i].type != -1; i++) {
		struct resource *r = br->res[i];

		if (br->res_spec->type != SYS_RES_MEMORY)
			continue;

		/* Verify range */
		if (rman_get_start(r) > start)
			continue;
		
		if (rman_get_end(r) < (start + count - 1))
			continue;

		return (r);
	}

	return (NULL);
}

/**
 * Find the resource containing @p win.
 * 
 * @param br The bhndb resource state to search.
 * @param win A register window.
 * 
 * @retval resource the resource containing @p win.
 * @retval NULL if no resource containing @p win can be found.
 */
struct resource *
bhndb_find_regwin_resource(struct bhndb_resources *br,
    const struct bhndb_regwin *win)
{
	const struct resource_spec *rspecs;

	rspecs = br->cfg->resource_specs;
	for (u_int i = 0; rspecs[i].type != -1; i++) {			
		if (win->res.type != rspecs[i].type)
			continue;

		if (win->res.rid != rspecs[i].rid)
			continue;

		/* Found declared resource */
		return (br->res[i]);
	}

	device_printf(br->dev,
	    "missing regwin resource spec (type=%d, rid=%d)\n",
	    win->res.type, win->res.rid);

	return (NULL);
}

/**
 * Allocate and initialize a new resource state structure, allocating
 * bus resources from @p parent_dev according to @p cfg.
 * 
 * @param dev The bridge device.
 * @param parent_dev The parent device from which resources will be allocated.
 * @param cfg The hardware configuration to be used.
 */
struct bhndb_resources *
bhndb_alloc_resources(device_t dev, device_t parent_dev,
    const struct bhndb_hwcfg *cfg)
{
	struct bhndb_resources		*r;
	const struct bhndb_regwin	*win;
	bus_size_t			 last_window_size;
	size_t				 res_num;
	u_int				 rnid;
	int				 error;
	bool				 free_parent_res;
	bool				 free_ht_mem, free_br_mem;

	free_parent_res = false;
	free_ht_mem = false;
	free_br_mem = false;

	r = malloc(sizeof(*r), M_BHND, M_NOWAIT|M_ZERO);
	if (r == NULL)
		return (NULL);

	/* Basic initialization */
	r->dev = dev;
	r->parent_dev = parent_dev;
	r->cfg = cfg;
	r->min_prio = BHNDB_PRIORITY_NONE;
	STAILQ_INIT(&r->bus_regions);
	
	/* Initialize host address space resource manager. */
	r->ht_mem_rman.rm_start = 0;
	r->ht_mem_rman.rm_end = ~0;
	r->ht_mem_rman.rm_type = RMAN_ARRAY;
	r->ht_mem_rman.rm_descr = "BHNDB host memory";
	if ((error = rman_init(&r->ht_mem_rman))) {
		device_printf(r->dev, "could not initialize ht_mem_rman\n");
		goto failed;
	}
	free_ht_mem = true;


	/* Initialize resource manager for the bridged address space. */
	r->br_mem_rman.rm_start = 0;
	r->br_mem_rman.rm_end = BUS_SPACE_MAXADDR_32BIT;
	r->br_mem_rman.rm_type = RMAN_ARRAY;
	r->br_mem_rman.rm_descr = "BHNDB bridged memory";

	if ((error = rman_init(&r->br_mem_rman))) {
		device_printf(r->dev, "could not initialize br_mem_rman\n");
		goto failed;
	}
	free_br_mem = true;

	error = rman_manage_region(&r->br_mem_rman, 0, BUS_SPACE_MAXADDR_32BIT);
	if (error) {
		device_printf(r->dev, "could not configure br_mem_rman\n");
		goto failed;
	}


	/* Determine our bridge resource count from the hardware config. */
	res_num = 0;
	for (size_t i = 0; cfg->resource_specs[i].type != -1; i++)
		res_num++;

	/* Allocate space for a non-const copy of our resource_spec
	 * table; this will be updated with the RIDs assigned by
	 * bus_alloc_resources. */
	r->res_spec = malloc(sizeof(r->res_spec[0]) * (res_num + 1), M_BHND,
	    M_NOWAIT);
	if (r->res_spec == NULL)
		goto failed;

	/* Initialize and terminate the table */
	for (size_t i = 0; i < res_num; i++)
		r->res_spec[i] = cfg->resource_specs[i];
	
	r->res_spec[res_num].type = -1;

	/* Allocate space for our resource references */
	r->res = malloc(sizeof(r->res[0]) * res_num, M_BHND, M_NOWAIT);
	if (r->res == NULL)
		goto failed;

	/* Allocate resources */
	error = bus_alloc_resources(r->parent_dev, r->res_spec, r->res);
	if (error) {
		device_printf(r->dev,
		    "could not allocate bridge resources on %s: %d\n",
		    device_get_nameunit(r->parent_dev), error);
		goto failed;
	} else {
		free_parent_res = true;
	}

	/* Add allocated memory resources to our host memory resource manager */
	for (u_int i = 0; r->res_spec[i].type != -1; i++) {
		struct resource *res;
		
		/* skip non-memory resources */
		if (r->res_spec[i].type != SYS_RES_MEMORY)
			continue;

		/* add host resource to set of managed regions */
		res = r->res[i];
		error = rman_manage_region(&r->ht_mem_rman, rman_get_start(res),
		    rman_get_end(res));
		if (error) {
			device_printf(r->dev,
			    "could not register host memory region with "
			    "ht_mem_rman: %d\n", error);
			goto failed;
		}
	}

	/* Fetch the dynamic regwin count and verify that it does not exceed
	 * what is representable via our freelist bitmask. */
	r->dwa_count = bhndb_regwin_count(cfg->register_windows,
	    BHNDB_REGWIN_T_DYN);
	if (r->dwa_count >= (8 * sizeof(r->dwa_freelist))) {
		device_printf(r->dev, "max dynamic regwin count exceeded\n");
		goto failed;
	}
	
	/* Allocate the dynamic window allocation table. */
	r->dw_alloc = malloc(sizeof(r->dw_alloc[0]) * r->dwa_count, M_BHND,
	    M_NOWAIT);
	if (r->dw_alloc == NULL)
		goto failed;

	/* Initialize the dynamic window table and freelist. */
	r->dwa_freelist = 0;
	rnid = 0;
	last_window_size = 0;
	for (win = cfg->register_windows;
	    win->win_type != BHNDB_REGWIN_T_INVALID; win++)
	{
		struct bhndb_dw_alloc *dwa;

		/* Skip non-DYN windows */
		if (win->win_type != BHNDB_REGWIN_T_DYN)
			continue;

		/* Validate the window size */
		if (win->win_size == 0) {
			device_printf(r->dev, "ignoring zero-length dynamic "
			    "register window\n");
			continue;
		} else if (last_window_size == 0) {
			last_window_size = win->win_size;
		} else if (last_window_size != win->win_size) {
			/* 
			 * No existing hardware should trigger this.
			 * 
			 * If you run into this in the future, the dynamic
			 * window allocator and the resource priority system
			 * will need to be extended to support multiple register
			 * window allocation pools. 
			 */
			device_printf(r->dev, "devices that vend multiple "
			    "dynamic register window sizes are not currently "
			    "supported\n");
			goto failed;
		}

		dwa = &r->dw_alloc[rnid];
		dwa->win = win;
		dwa->parent_res = NULL;
		dwa->rnid = rnid;
		dwa->target = 0x0;
		
		LIST_INIT(&dwa->refs);

		/* Find and validate corresponding resource. */
		dwa->parent_res = bhndb_find_regwin_resource(r, win);
		if (dwa->parent_res == NULL)
			goto failed;

		if (rman_get_size(dwa->parent_res) < win->win_offset +
		    win->win_size)
		{
			device_printf(r->dev, "resource %d too small for "
			    "register window with offset %llx and size %llx\n",
			    rman_get_rid(dwa->parent_res),
			    (unsigned long long) win->win_offset,
			    (unsigned long long) win->win_size);

			error = EINVAL;
			goto failed;
		}

		/* Add to freelist */
		r->dwa_freelist |= (1 << rnid);

		rnid++;
	}

	return (r);

failed:
	if (free_parent_res)
		bus_release_resources(r->parent_dev, r->res_spec, r->res);
	
	if (free_ht_mem)
		rman_fini(&r->ht_mem_rman);

	if (free_br_mem)
		rman_fini(&r->br_mem_rman);

	if (r->res != NULL)
		free(r->res, M_BHND);

	if (r->res_spec != NULL)
		free(r->res_spec, M_BHND);

	if (r->dw_alloc != NULL)
		free(r->dw_alloc, M_BHND);

	free (r, M_BHND);

	return (NULL);
}

/**
 * Deallocate the given bridge resource structure and any associated resources.
 * 
 * @param br Resource state to be deallocated.
 */
void
bhndb_free_resources(struct bhndb_resources *br)
{
	struct bhndb_region	*region, *r_next;
	struct bhndb_dw_alloc	*dwa;
	struct bhndb_dw_rentry	*dwr, *dwr_next;

	/* No window regions may still be held */
	if (__builtin_popcount(br->dwa_freelist) != br->dwa_count) {
		device_printf(br->dev, "leaked %llu dynamic register regions\n",
		    (unsigned long long) br->dwa_count - br->dwa_freelist);
	}

	/* Release resources allocated through our parent. */
	bus_release_resources(br->parent_dev, br->res_spec, br->res);

	/* Clean up resource reservations */
	for (size_t i = 0; i < br->dwa_count; i++) {
		dwa = &br->dw_alloc[i];

		LIST_FOREACH_SAFE(dwr, &dwa->refs, dw_link, dwr_next) {
			LIST_REMOVE(dwr, dw_link);
			free(dwr, M_BHND);
		}
	}
	
	/* Release bus regions */
	STAILQ_FOREACH_SAFE(region, &br->bus_regions, link, r_next) {
		STAILQ_REMOVE(&br->bus_regions, region, bhndb_region, link);
		free(region, M_BHND);
	}

	/* Release our resource managers */
	rman_fini(&br->ht_mem_rman);
	rman_fini(&br->br_mem_rman);

	/* Free backing resource state structures */
	free(br->res, M_BHND);
	free(br->res_spec, M_BHND);
	free(br->dw_alloc, M_BHND);
}

/**
 * Add a bus region entry to @p r for the given base @p addr and @p size.
 * 
 * @param br The resource state to which the bus region entry will be added.
 * @param addr The base address of this region.
 * @param size The size of this region.
 * @param priority The resource priority to be assigned to allocations
 * made within this bus region.
 * @param static_regwin If available, a static register window mapping this
 * bus region entry. If not available, NULL.
 * 
 * @retval 0 success
 * @retval non-zero if adding the bus region fails.
 */
int
bhndb_add_resource_region(struct bhndb_resources *br, bhnd_addr_t addr,
    bhnd_size_t size, bhndb_priority_t priority,
    const struct bhndb_regwin *static_regwin)
{
	struct bhndb_region	*reg;

	/* Insert in the bus resource list */
	reg = malloc(sizeof(*reg), M_BHND, M_NOWAIT);
	if (reg == NULL)
		return (ENOMEM);

	*reg = (struct bhndb_region) {
		.addr = addr,
		.size = size,
		.priority = priority,
		.static_regwin = static_regwin
	};

	STAILQ_INSERT_HEAD(&br->bus_regions, reg, link);

	return (0);
}

/**
 * Find a bus region that maps @p size bytes at @p addr.
 * 
 * @param br The resource state to search.
 * @param addr The requested starting address.
 * @param size The requested size.
 * 
 * @retval bhndb_region A region that fully contains the requested range.
 * @retval NULL If no mapping region can be found.
 */
struct bhndb_region *
bhndb_find_resource_region(struct bhndb_resources *br, bhnd_addr_t addr,
    bhnd_size_t size)
{
	struct bhndb_region *region;

	STAILQ_FOREACH(region, &br->bus_regions, link) {
		/* Request must fit within the region's mapping  */
		if (addr < region->addr)
			continue;

		if (addr + size > region->addr + region->size)
			continue;

		return (region);
	}

	/* Not found */
	return (NULL);
}

/**
 * Find the entry matching @p r in @p dwa's references, if any.
 * 
 * @param dwa The dynamic window allocation to search
 * @param r The resource to search for in @p dwa.
 */
static struct bhndb_dw_rentry *
bhndb_dw_find_resource_entry(struct bhndb_dw_alloc *dwa, struct resource *r)
{
	struct bhndb_dw_rentry	*rentry;

	LIST_FOREACH(rentry, &dwa->refs, dw_link) {
		struct resource *dw_res = rentry->dw_res;

		/* Match dev/rid/addr/size */
		if (rman_get_device(dw_res)	!= rman_get_device(r) ||
			rman_get_rid(dw_res)	!= rman_get_rid(r) ||
			rman_get_start(dw_res)	!= rman_get_start(r) ||
			rman_get_size(dw_res)	!= rman_get_size(r))
		{
			continue;
		}

		/* Matching allocation found */
		return (rentry);
	}

	return (NULL);
}

/**
 * Find the dynamic region allocated for @p r, if any.
 * 
 * @param br The resource state to search.
 * @param r The resource to search for.
 * 
 * @retval bhndb_dw_alloc The allocation record for @p r.
 * @retval NULL if no dynamic window is allocated for @p r.
 */
struct bhndb_dw_alloc *
bhndb_dw_find_resource(struct bhndb_resources *br, struct resource *r)
{
	struct bhndb_dw_alloc	*dwa;

	for (size_t i = 0; i < br->dwa_count; i++) {
		dwa = &br->dw_alloc[i];

		/* Skip free dynamic windows */
		if (bhndb_dw_is_free(br, dwa))
			continue;

		/* Matching allocation found? */
		if (bhndb_dw_find_resource_entry(dwa, r) != NULL)
			return (dwa);
	}

	return (NULL);
}

/**
 * Find an existing dynamic window mapping @p size bytes
 * at @p addr. The window may or may not be free.
 * 
 * @param br The resource state to search.
 * @param addr The requested starting address.
 * @param size The requested size.
 * 
 * @retval bhndb_dw_alloc A window allocation that fully contains the requested
 * range.
 * @retval NULL If no mapping region can be found.
 */
struct bhndb_dw_alloc *
bhndb_dw_find_mapping(struct bhndb_resources *br, bhnd_addr_t addr,
    bhnd_size_t size)
{
	struct bhndb_dw_alloc		*dwr;
	const struct bhndb_regwin	*win;

	/* Search for an existing dynamic mapping of this address range. */
	for (size_t i = 0; i < br->dwa_count; i++) {
		dwr = &br->dw_alloc[i];
		win = dwr->win;

		/* Verify the range */
		if (addr < dwr->target)
			continue;

		if (addr + size > dwr->target + win->win_size)
			continue;

		/* Found a usable mapping */
		return (dwr);
	}

	/* not found */
	return (NULL);
}

/**
 * Retain a reference to @p dwa for use by @p res.
 * 
 * @param br The resource state owning @p dwa.
 * @param dwa The allocation record to be retained.
 * @param res The resource that will own a reference to @p dwa.
 * 
 * @retval 0 success
 * @retval ENOMEM Failed to allocate a new reference structure.
 */
int
bhndb_dw_retain(struct bhndb_resources *br, struct bhndb_dw_alloc *dwa,
    struct resource *res)
{
	struct bhndb_dw_rentry *rentry;

	KASSERT(bhndb_dw_find_resource_entry(dwa, res) == NULL,
	    ("double-retain of dynamic window for same resource"));

	/* Insert a reference entry; we use M_NOWAIT to allow use from
	 * within a non-sleepable lock */
	rentry = malloc(sizeof(*rentry), M_BHND, M_NOWAIT);
	if (rentry == NULL)
		return (ENOMEM);

	rentry->dw_res = res;
	LIST_INSERT_HEAD(&dwa->refs, rentry, dw_link);

	/* Update the free list */
	br->dwa_freelist &= ~(1 << (dwa->rnid));
 
	return (0);
}

/**
 * Release a reference to @p dwa previously retained by @p res. If the
 * reference count of @p dwa reaches zero, it will be added to the
 * free list.
 * 
 * @param br The resource state owning @p dwa.
 * @param dwa The allocation record to be released.
 * @param res The resource that currently owns a reference to @p dwa.
 */
void
bhndb_dw_release(struct bhndb_resources *br, struct bhndb_dw_alloc *dwa,
    struct resource *r)
{
	struct bhndb_dw_rentry	*rentry;

	/* Find the rentry */
	rentry = bhndb_dw_find_resource_entry(dwa, r);
	KASSERT(rentry != NULL, ("over release of resource entry"));

	LIST_REMOVE(rentry, dw_link);
	free(rentry, M_BHND);

	/* If this was the last reference, update the free list */
	if (LIST_EMPTY(&dwa->refs))
		br->dwa_freelist |= (1 << (dwa->rnid));
}

/**
 * Attempt to set (or reset) the target address of @p dwa to map @p size bytes
 * at @p addr.
 * 
 * This will apply any necessary window alignment and verify that
 * the window is capable of mapping the requested range prior to modifying
 * therecord.
 * 
 * @param dev The device on which to issue the BHNDB_SET_WINDOW_ADDR() request.
 * @param br The resource state owning @p dwa.
 * @param dwa The allocation record to be configured.
 * @param addr The address to be mapped via @p dwa.
 * @param size The number of bytes to be mapped at @p addr.
 *
 * @retval 0 success
 * @retval non-zero no usable register window available.
 */
int
bhndb_dw_set_addr(device_t dev, struct bhndb_resources *br,
    struct bhndb_dw_alloc *dwa, bus_addr_t addr, bus_size_t size)
{
	const struct bhndb_regwin	*rw;
	bus_addr_t			 offset;
	int				 error;

	rw = dwa->win;

	KASSERT(bhndb_dw_is_free(br, dwa),
	    ("attempting to set the target address on an in-use window"));

	/* Page-align the target address */
	offset = addr % rw->win_size;
	dwa->target = addr - offset;

	/* Verify that the window is large enough for the full target */
	if (rw->win_size - offset < size)
		return (ENOMEM);
	
	/* Update the window target */
	error = BHNDB_SET_WINDOW_ADDR(dev, dwa->win, dwa->target);
	if (error) {
		dwa->target = 0x0;
		return (error);
	}

	return (0);
}

/**
 * Return the count of @p type register windows in @p table.
 * 
 * @param table The table to search.
 * @param type The required window type, or BHNDB_REGWIN_T_INVALID to
 * count all register window types.
 */
size_t
bhndb_regwin_count(const struct bhndb_regwin *table,
    bhndb_regwin_type_t type)
{
	const struct bhndb_regwin	*rw;
	size_t				 count;

	count = 0;
	for (rw = table; rw->win_type != BHNDB_REGWIN_T_INVALID; rw++) {
		if (type == BHNDB_REGWIN_T_INVALID || rw->win_type == type)
			count++;
	}

	return (count);
}

/**
 * Search @p table for the first window with the given @p type.
 * 
 * @param table The table to search.
 * @param type The required window type.
 * @param min_size The minimum window size.
 * 
 * @retval bhndb_regwin The first matching window.
 * @retval NULL If no window of the requested type could be found. 
 */
const struct bhndb_regwin *
bhndb_regwin_find_type(const struct bhndb_regwin *table,
    bhndb_regwin_type_t type, bus_size_t min_size)
{
	const struct bhndb_regwin *rw;

	for (rw = table; rw->win_type != BHNDB_REGWIN_T_INVALID; rw++)
	{
		if (rw->win_type == type && rw->win_size >= min_size)
			return (rw);
	}

	return (NULL);
}

/**
 * Search @p windows for the first matching core window.
 * 
 * @param table The table to search.
 * @param class The required core class.
 * @param unit The required core unit, or -1.
 * @param port_type The required port type.
 * @param port The required port.
 * @param region The required region.
 *
 * @retval bhndb_regwin The first matching window.
 * @retval NULL If no matching window was found. 
 */
const struct bhndb_regwin *
bhndb_regwin_find_core(const struct bhndb_regwin *table, bhnd_devclass_t class,
    int unit, bhnd_port_type port_type, u_int port, u_int region)
{
	const struct bhndb_regwin *rw;
	
	for (rw = table; rw->win_type != BHNDB_REGWIN_T_INVALID; rw++)
	{
		if (rw->win_type != BHNDB_REGWIN_T_CORE)
			continue;

		if (rw->core.class != class)
			continue;
		
		if (unit != -1 && rw->core.unit != unit)
			continue;

		if (rw->core.port_type != port_type)
			continue;

		if (rw->core.port != port)
			continue;
		
		if (rw->core.region != region)
			continue;

		return (rw);
	}

	return (NULL);
}

/**
 * Search @p windows for the best available window of at least @p min_size.
 * 
 * Search order:
 * - BHND_REGWIN_T_CORE
 * - BHND_REGWIN_T_DYN
 * 
 * @param table The table to search.
 * @param class The required core class.
 * @param unit The required core unit, or -1.
 * @param port_type The required port type.
 * @param port The required port.
 * @param region The required region.
 * @param min_size The minimum window size.
 *
 * @retval bhndb_regwin The first matching window.
 * @retval NULL If no matching window was found. 
 */
const struct bhndb_regwin *
bhndb_regwin_find_best(const struct bhndb_regwin *table,
    bhnd_devclass_t class, int unit, bhnd_port_type port_type, u_int port,
    u_int region, bus_size_t min_size)
{
	const struct bhndb_regwin *rw;

	/* Prefer a fixed core mapping */
	rw = bhndb_regwin_find_core(table, class, unit, port_type,
	    port, region);
	if (rw != NULL)
		return (rw);

	/* Fall back on a generic dynamic window */
	return (bhndb_regwin_find_type(table, BHNDB_REGWIN_T_DYN, min_size));
}

/**
 * Return true if @p regw defines a static port register window, and
 * the mapped port is actually defined on @p dev.
 * 
 * @param regw A register window to match against.
 * @param dev A bhnd(4) bus device.
 */
bool
bhndb_regwin_matches_device(const struct bhndb_regwin *regw, device_t dev)
{
	/* Only core windows are supported */
	if (regw->win_type != BHNDB_REGWIN_T_CORE)
		return (false);

	/* Device class must match */
	if (bhnd_get_class(dev) != regw->core.class)
		return (false);

	/* Device unit must match */
	if (bhnd_get_core_unit(dev) != regw->core.unit)
		return (false);
	
	/* The regwin port/region must be defined. */
	if (!bhnd_is_region_valid(dev, regw->core.port_type, regw->core.port,
	    regw->core.region))
	{
		return (false);
	}

	/* Matches */
	return (true);
}

/**
 * Search for a core resource priority descriptor in @p table that matches
 * @p device.
 * 
 * @param table The table to search.
 * @param device A bhnd(4) bus device.
 */
const struct bhndb_hw_priority *
bhndb_hw_priority_find_device(const struct bhndb_hw_priority *table,
    device_t device)
{
	const struct bhndb_hw_priority *hp;

	for (hp = table; hp->ports != NULL; hp++) {
		if (bhnd_device_matches(device, &hp->match))
			return (hp);
	}

	/* not found */
	return (NULL);
}
