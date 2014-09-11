/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * Copyright (c) 2013 Philip Withnall
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
__FBSDID("$FreeBSD: head/sys/dev/cheri/compositor/cheri_compositor_reg.c 239691 2013-05-04 12:16:00Z pwithnall $");

/*
 * XXXRW: Direct access to capability internals via memory is discouraged;
 * this code should be updated to use CHERI builtins.
 */
#define	CHERICAP_INTERNAL

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/consio.h>				/* struct vt_mode */
#include <sys/endian.h>
#include <sys/fbio.h>				/* video_adapter_t */
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/rwlock.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/vm.h>

#include <dev/cheri/compositor/cheri_compositor_internal.h>
#include <mips/include/cheri.h>

/* Based on sys/dev/terasic/mtl/terasic_mtl_reg.c by Robert N. M. Watson. */

MALLOC_DEFINE(M_CHERI_COMPOSITOR, "compositor_reg",
    "CHERI Compositor registers");

#define DIVIDE_ROUND(N, D) (((N) + (D) - 1) / (D))
#define SEQ_NUM_INC(S) S = (S == SEQ_NUM_MAX - 1) ? 1 : S + 1

/* A tile position, in tiles relative to the top-left corner of the screen. */
struct tile_position {
	unsigned int x;
	unsigned int y;
};

/* A rectangular region of tiles, with all coordinates in terms of tiles.
 * top_left gives the top-left corner of the region; bottom_right gives the
 * bottom-right corner of the region (which must be greater than or equal to
 * top_left in both dimensions). The minimum size is one tile (when top_left and
 * bottom_right are equal); the maximum is the entire screen. */
struct tile_region {
	struct tile_position top_left;
	struct tile_position bottom_right;
};

/* Permissions mask giving the maximum set of permissions a CFB ID or token can
 * have. */
#define CFB_PERMS_MASK (CHERI_PERM_STORE | CHERI_PERM_LOAD)

/* Register offsets (in bytes). */
#define CHERI_COMPOSITOR_COMMAND_HEADER		0
#define CHERI_COMPOSITOR_COMMAND_PAYLOAD	8
#define CHERI_COMPOSITOR_RESPONSE_HEADER	16
#define CHERI_COMPOSITOR_RESPONSE_PAYLOAD	24

/* Sampler register offsets (in bytes). */
#define CHERI_COMPOSITOR_SAMPLER_CONTROL			0
#define CHERI_COMPOSITOR_SAMPLER_NUM_READ_REQUESTS		4
#define CHERI_COMPOSITOR_SAMPLER_NUM_WRITE_REQUESTS		8
#define CHERI_COMPOSITOR_SAMPLER_NUM_READ_BURSTS		12
#define CHERI_COMPOSITOR_SAMPLER_NUM_WRITE_BURSTS		16
#define CHERI_COMPOSITOR_SAMPLER_LATENCY_BIN_CONFIGURATION	28
#define CHERI_COMPOSITOR_SAMPLER_LATENCY_BINS			64

/* Bit fields for the CHERI_COMPOSITOR_SAMPLER_CONTROL register. These are
 * messed up because of endianness somewhere. (FIXME.) */
#define CHERI_COMPOSITOR_SAMPLER_CONTROL_HAS_OVERFLOWED_POS	24
#define CHERI_COMPOSITOR_SAMPLER_CONTROL_IS_PAUSED_POS		25
#define CHERI_COMPOSITOR_SAMPLER_CONTROL_RESET_POS		26

/*
 * Various structures and macros for interacting with the hardware. These define
 * the interface between the driver and the hardware, so don't change them
 * unless you're also changing the Bluespec.
 */
typedef enum {
	OPCODE_NOOP = 0,
	OPCODE_ALLOCATE_CFB = 1,
	OPCODE_FREE_CFB = 2,
	OPCODE_UPDATE_CFB = 3,
	OPCODE_SWAP_CFBS = 4,
	OPCODE_GET_CONFIGURATION = 5,
	OPCODE_SET_CONFIGURATION = 6,
	OPCODE_GET_BASE_ADDRESS = 7,
	OPCODE_SET_BASE_ADDRESS = 8,
	OPCODE_UPDATE_TILE_CACHE_ENTRY = 9,
	OPCODE_GET_PARAMETERS = 10,
	OPCODE_GET_STATISTICS = 11,
	OPCODE_CONTROL_STATISTICS = 12,
} compositor_opcode;

/* Page selectors for the GetStatistics command. */
typedef enum {
	STATISTICS_PAGE_PIPELINE1 = 0,
	STATISTICS_PAGE_PIPELINE2 = 1,
	STATISTICS_PAGE_PIPELINE3 = 2,
	STATISTICS_PAGE_MEMORIES = 3,
	STATISTICS_PAGE_FRAME_RATE = 4,
} compositor_statistics_page;

struct compositor_command {
	struct {
		unsigned int padding0 : 15;
		unsigned int fence : 1;
		unsigned int padding1 : 4;
		unsigned int seq_num : 4;
		unsigned int padding2 : 4;
		compositor_opcode opcode : 4;
	} header;
	uint64_t payload;
} __packed;

/* Macro to construct a tile cache entry for use in command payloads.
 * FIXME: Due to payload space restrictions, we have to drop is_opaque and
 * height; they're made up by the hardware. This should be fixed by implementing
 * UpdateCfb in hardware; then UpdateTileCacheEntry can go away. */
#define TILE_CACHE_ENTRY(is_opaque, x, y, allocated_tiles_base, width, height) \
	(((uint64_t) (x) & 0xfff) << 34 | \
	 ((uint64_t) (y) & 0x7ff) << 23 | \
	 ((uint64_t) (allocated_tiles_base) & 0xffff) << 7 | \
	 ((uint64_t) (width) & 0x7f))

/* Macros to construct 64-bit payloads for commands being sent to the
 * hardware. */
#define PAYLOAD_NOOP (0)
#define PAYLOAD_ALLOCATE_CFB(widthTiles, heightTiles, allocatedTilesBase) \
	(((uint64_t) (allocatedTilesBase) & 0xffff) << 16 | \
	 ((uint64_t) (heightTiles) & 0x3f) << 8 | \
	 ((uint64_t) (widthTiles) & 0x7f))
#define PAYLOAD_SET_CONFIGURATION(x_resolution, y_resolution) \
	(((uint64_t) (y_resolution) & 0x7ff) << 16 | \
	 ((uint64_t) (x_resolution) & 0xfff))
#define PAYLOAD_UPDATE_TILE_CACHE_ENTRY(layer, address, entry) \
	(((uint64_t) (layer) & 0x7) << 58 | \
	 ((uint64_t) (address) & 0xfff) << 46 | \
	 ((uint64_t) (entry) & 0x3fffffffffff))
#define PAYLOAD_GET_STATISTICS(page) \
	((uint64_t) (page) & 0x7)
#define PAYLOAD_CONTROL_STATISTICS(reset, isPaused) \
	(((uint64_t) (reset) & 0x1) << 1 | \
	 ((uint64_t) (isPaused) & 0x1))

/* Maximum of 4 bits wide. */
typedef enum {
	STATUS_FAILURE = 0,
	STATUS_SUCCESS = 1,
} compositor_response_status;

struct compositor_response {
	struct {
		unsigned int padding0 : 20;
		unsigned int seq_num : 4;
		compositor_opcode opcode : 4;
		compositor_response_status status : 4;
	} header;
	uint64_t payload;
} __packed;

static inline void
compositor_send_command(struct cheri_compositor_softc *sc,
    const struct compositor_command *command,
    volatile struct compositor_response *response,
    boolean_t expecting_response_payload)
{
	const uint32_t *command_raw = (const uint32_t *) command;
	volatile uint32_t *response_raw = (volatile uint32_t *) response;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);

#if defined(CHERI_COMPOSITOR_DEBUG_VERBOSE) && CHERI_COMPOSITOR_DEBUG_VERBOSE
	/* Warn if our sequence number is zero because it'll cause a timeout. */
	if (command->header.seq_num == 0) {
		CHERI_COMPOSITOR_ERROR(sc, "Zero sequence number used.");
	}

	CHERI_COMPOSITOR_DEBUG(sc,
	    "Sending command %u (seq_num: %u): %08x %016lx…",
	    command->header.opcode, command->header.seq_num, command_raw[0],
	    command->payload);
#endif

	/* Send the payload. */
	bus_write_8(sc->compositor_reg_res, CHERI_COMPOSITOR_COMMAND_PAYLOAD,
	    command->payload);

	/* Send the command. */
	bus_write_8(sc->compositor_reg_res, CHERI_COMPOSITOR_COMMAND_HEADER,
	    command_raw[0]);

	/* FIXME: Tidy up endianness; h64tole, le64toh etc. */

	/* Read the response. The hardware guarantees that this is always
	 * available on the next clock cycle. */
	CHERI_COMPOSITOR_DEBUG(sc, "Checking for response…");

	response_raw[0] =
	    bus_read_8(sc->compositor_reg_res,
	        CHERI_COMPOSITOR_RESPONSE_HEADER) & 0xffffffff;

	if (expecting_response_payload) {
		/* This should get optimised if !expecting_response_payload. */
		response->payload =
		    bus_read_8(sc->compositor_reg_res,
		        CHERI_COMPOSITOR_RESPONSE_PAYLOAD);
	}

#if defined(CHERI_COMPOSITOR_DEBUG_VERBOSE) && CHERI_COMPOSITOR_DEBUG_VERBOSE
	/* Error? */
	if (response->header.seq_num == 0 ||
	    response->header.seq_num != command->header.seq_num) {
		CHERI_COMPOSITOR_ERROR(sc,
		    "Aborted sending compositor command due to error.");
	}

	CHERI_COMPOSITOR_DEBUG(sc, "Acknowledging response.");
#endif

	/* Acknowledge the response. */
	bus_write_8(sc->compositor_reg_res,
	    CHERI_COMPOSITOR_RESPONSE_HEADER, 0);
}

#if defined(CHERI_COMPOSITOR_DEBUG_VERBOSE) && CHERI_COMPOSITOR_DEBUG_VERBOSE
/* Debugging aide. */
static void
compositor_print_cfbs(struct cheri_compositor_softc *sc)
{
	const struct compositor_cfb *cfb;

	device_printf(sc->compositor_dev, "%s: CFB IDs:\n", __func__);

	LIST_FOREACH(cfb, &sc->cfbs, z_ordering) {
		device_printf(sc->compositor_dev, "\t%p [%p, %p)\n",
		    cfb,
		    (void *) (rman_get_start(sc->compositor_cfb_res) +
		        tile_offset_to_byte_offset(cfb->allocated_tiles_base)),
		    (void *) (rman_get_start(sc->compositor_cfb_res) +
		        tile_offset_to_byte_offset(cfb->allocated_tiles_base +
		            cfb->width * cfb->height)));
	}
}
#endif

/* qsort()-style. */
static int
compositor_sort_cfbs_callback(const struct compositor_cfb *a,
    const struct compositor_cfb *b)
{
	/* NULL sorts after everything else. */
	if (a == NULL || b == NULL)
		return (intptr_t) b - (intptr_t) a;

	/* Sort by Z order. Higher Z values mean the CFB is closer to the
	 * screen, and hence needs to be nearer the top of the Z-ordering of
	 * sc->cfbs. */
	if (a->z != b->z)
		return (int) b->z - (int) a->z;

	/* Break ties by address. */
	return (intptr_t) a - (intptr_t) b;
}

static void
compositor_sort_cfbs(struct cheri_compositor_softc *sc)
{
	struct compositor_cfb *prev_cfb, *cfb;
	boolean_t swapped = true;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "Sorting CFBs.");

	/* Simple bubble sort. FIXME: Use something better. */
	while (!LIST_EMPTY(&sc->cfbs) && swapped) {
		swapped = false;
		prev_cfb = LIST_FIRST(&sc->cfbs);
		cfb = NULL;

		for (cfb = LIST_NEXT(prev_cfb, z_ordering);
		     cfb != NULL;
		     prev_cfb = cfb, cfb = LIST_NEXT(cfb, z_ordering)) {
			if (compositor_sort_cfbs_callback (prev_cfb, cfb) > 0) {
				struct compositor_cfb *tmp;

				LIST_REMOVE(cfb, z_ordering);
				LIST_INSERT_BEFORE(prev_cfb, cfb, z_ordering);
				tmp = prev_cfb;
				prev_cfb = cfb;
				cfb = tmp;
				swapped = true;
			}
		}
	}

#if defined(CHERI_COMPOSITOR_DEBUG_VERBOSE) && CHERI_COMPOSITOR_DEBUG_VERBOSE
	compositor_print_cfbs(sc);
#endif
}

/**
 * Calculate the region of tiles which are covered by this CFB, and hence are
 * invalidated if the CFB has just changed position.
 */
static void
calculate_invalidation_region_for_cfb(struct cheri_compositor_softc *sc,
    const struct compositor_cfb *cfb, struct tile_region *invalidation_region)
{
	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);

	invalidation_region->top_left.x = cfb->x / TILE_SIZE /* truncated */;
	invalidation_region->top_left.y = cfb->y / TILE_SIZE /* truncated */;
	invalidation_region->bottom_right.x =
		MIN(DIVIDE_ROUND(sc->configuration.x_resolution, TILE_SIZE) - 1,
		    DIVIDE_ROUND(cfb->x, TILE_SIZE) + cfb->width);
	invalidation_region->bottom_right.y =
		MIN(DIVIDE_ROUND(sc->configuration.y_resolution, TILE_SIZE) - 1,
		    DIVIDE_ROUND(cfb->y, TILE_SIZE) + cfb->height);
}

/**
 * Calculate the union of two tile regions. This gives another rectangular
 * region (convex regions are not supported).
 *
 * Note that region1, region2 and region_out may all alias.
 */
static void
calculate_invalidation_region_union(const struct tile_region *region1,
    const struct tile_region *region2, struct tile_region *region_out)
{
	region_out->top_left.x = MIN(region1->top_left.x, region2->top_left.x);
	region_out->top_left.y = MIN(region1->top_left.y, region2->top_left.y);
	region_out->bottom_right.x =
		MAX(region1->bottom_right.x, region2->bottom_right.x);
	region_out->bottom_right.y =
		MAX(region1->bottom_right.y, region2->bottom_right.y);
}

/**
 * Free a CFB, deallocating its space in its CFB pool, and removing it from the
 * Z ordering.
 */
static void
free_cfb(struct cheri_compositor_softc *sc, struct compositor_cfb *cfb)
{
	CHERI_COMPOSITOR_DEBUG(sc, "Freeing CFB %p.", cfb);

	/* Remove the CFB from the cfbs list. */
	LIST_REMOVE(cfb, z_ordering);
	free(cfb, M_CHERI_COMPOSITOR);
	cfb = NULL;

	/* FIXME: If this is the most recently-allocated CFB, decrement
	 * next_free_tile. Also need to call FreeCfb in the hardware. */
}

/**
 * Construct a capability for a region inside a given CFB pool. This assumes
 * we're executing with full ambient authority, and clobbers $c1. It assumes
 * that the region has previously been validated to lie inside the CFB pool.
 *
 * Note that this performs *no* validation. It is up to the caller to validate
 * the input.
 *
 * vm_map_entry: VM mapping for the pool the region is inside.
 * allocation_offset: Offset from the base of the pool to the first byte of the
 * region, in bytes.
 * allocation_length: Length of the region, in bytes.
 */
static void
construct_capability_for_cfb_pool_and_offset(struct cheri_compositor_softc *sc,
    const struct vm_map_entry *entry, size_t allocation_offset,
    size_t allocation_length, struct chericap *cfb_cap_out)
{
	vm_offset_t virtual_base_address;

	CHERI_COMPOSITOR_DEBUG(sc,
	    "entry: %p, allocation_offset: %lu, allocation_length: %lu",
	    entry, allocation_offset, allocation_length);

	/* Clear $c1 so that we return an invalid capability in cfb_cap_out in
	 * error conditions. */
	CHERI_CCLEARTAG(1, 1);

	/* entry->start gives the base of the CFB pool in the process' VM space,
	 * and entry->offset gives the start of the mmap() from the base of the
	 * pool; hence (entry->start - entry->offset) gives the virtual address
	 * of the base of the pool. allocation_offset gives the offset of the
	 * region from the base of the pool. Everything is in bytes. */
	virtual_base_address = entry->start - entry->offset + allocation_offset;

	CHERI_COMPOSITOR_DEBUG(sc, "virtual_base_address: %p",
	    (void *) virtual_base_address);

	CHERI_CINCBASE(1, CHERI_CR_KDC, virtual_base_address);
	CHERI_CSETLEN(1, 1, allocation_length);
	CHERI_CANDPERM(1, 1, CFB_PERMS_MASK);

	/* Return. */
	cheri_capability_store(1, cfb_cap_out);

	/* Don't leak the capability. */
	CHERI_CCLEARTAG(1, 1);
}

/**
 * Convert a physical address in compositor memory into a compositor_cfb_id,
 * giving the CFB pool, CFB and allocation which contains that physical address.
 *
 * Note that this performs *no* validation. It is up to the caller to validate
 * the returned compositor_cfb_id.
 *
 * The physical_base_address must be in PHYS, not XPHYS.
 */
static void
extract_cfb_id_from_physical_address(
    struct cheri_compositor_softc *sc, vm_paddr_t physical_base_address,
    size_t length, struct compositor_cfb_id *cfb_id_out)
{
	unsigned int cfb_pool_index; /* into sc->mem_pools */
	size_t compositor_offset; /* bytes from the base of compositor memory */
	struct compositor_cfb_pool *cfb_pool;
	compositor_tile_offset_t allocation_offset; /* tiles */
	compositor_tile_offset_t allocation_length; /* tiles */

	/* Translate to be relative to the base of compositor memory. Note that
	 * rman returns the address in XKPHYS, but the VM subsystem returns it
	 * in PHYS. */
	compositor_offset =
	    physical_base_address -
	        MIPS_XKPHYS_TO_PHYS(rman_get_start(sc->compositor_cfb_res));
	/* Note: this deliberately truncates: */
	cfb_pool_index = compositor_offset / CHERI_COMPOSITOR_MEM_POOL_LENGTH;
	cfb_pool = &sc->mem_pools[cfb_pool_index];

	allocation_offset =
	    byte_offset_to_tile_offset(
	        compositor_offset % CHERI_COMPOSITOR_MEM_POOL_LENGTH);
	allocation_length = byte_offset_to_tile_offset(length);

	CHERI_COMPOSITOR_DEBUG(sc,
	    "compositor_offset: %lu, cfb_pool_index: %u, cfb_pool: %p "
	    "(mapped FD: %p, VM object: %p, next free tile: %lu), "
	    "allocation_offset: %lu, allocation_length: %lu",
	    compositor_offset, cfb_pool_index, cfb_pool, cfb_pool->mapped_fd,
	    cfb_pool->vm_obj, cfb_pool->next_free_tile, allocation_offset,
	    allocation_length);

	/* Output. */
	cfb_id_out->pool = cfb_pool;
	cfb_id_out->offset = allocation_offset;
	cfb_id_out->length = allocation_length;
}

/* See below for documentation. */
static vm_paddr_t
convert_virtual_to_physical(struct cheri_compositor_softc *sc,
    vm_ooffset_t virtual_base_address, const struct vm_map_entry *entry,
    const struct compositor_cfb_pool *cfb_pool)
{
	size_t allocation_offset;

	/* Construct the token out of the CFB's physical address, input length
	 * and permissions.
	 *
	 * entry->start gives the base of the CFB pool in the process' VM space,
	 * and entry->offset gives the start of the mmap() from the base of the
	 * pool; hence (entry->start - entry->offset) gives the virtual address
	 * of the base of the pool. allocation_offset gives the offset of the
	 * region from the base of the pool. Everything is in bytes.
	 *
	 * This is equivalent to:
	 *     return vtophys(virtual_base_address);
	 * but works even before the page mapping has been faulted in. */
	allocation_offset =
	    virtual_base_address - (entry->start - entry->offset);
	return rman_get_start(sc->compositor_cfb_res) +
	    compositor_cfb_pool_to_byte_offset(sc, cfb_pool) +
	    allocation_offset;
}

/**
 * Convert a CFB ID capability into a compositor_cfb_id, giving the CFB pool,
 * CFB and length of the CFB identified by the capability.
 *
 * Note that this performs *no* validation. It is up to the caller to validate
 * the returned compositor_cfb_id.
 */
static void
extract_cfb_id_from_cfb_cap_id(struct cheri_compositor_softc *sc,
    const struct compositor_cfb_pool *cfb_pool,
    const struct vm_map_entry *entry,
    const struct chericap *cfb_cap, struct compositor_cfb_id *cfb_id_out)
{
	vm_paddr_t physical_base_address;

	CHERI_COMPOSITOR_DEBUG(sc, "cfb_cap: %p (%p, %p]", cfb_cap,
	    (void *) cfb_cap->c_base,
	    (void *) (cfb_cap->c_base + cfb_cap->c_length));

	/* Convert to a physical address. */
	physical_base_address =
	    convert_virtual_to_physical(sc, cfb_cap->c_base, entry, cfb_pool);

	CHERI_COMPOSITOR_DEBUG(sc, "physical_base_address: %p",
	    (void *) physical_base_address);

	extract_cfb_id_from_physical_address(sc,
	    physical_base_address, cfb_cap->c_length, cfb_id_out);
}

/**
 * Convert a CFB token capability into a compositor_cfb_id, giving the CFB pool,
 * CFB and length of the CFB identified by the capability.
 *
 * Note that this performs *no* validation. It is up to the caller to validate
 * the returned compositor_cfb_id.
 */
static void
extract_cfb_id_from_cfb_cap_token(struct cheri_compositor_softc *sc,
    const struct chericap *cfb_cap, struct compositor_cfb_id *cfb_id_out)
{
	CHERI_COMPOSITOR_DEBUG(sc, "cfb_cap: %p (%p, %p]", cfb_cap,
	    (void *) cfb_cap->c_base,
	    (void *) (cfb_cap->c_base + cfb_cap->c_length));
	CHERI_COMPOSITOR_DEBUG(sc, "physical_base_address: %p",
	    (void *) cfb_cap->c_base);

	extract_cfb_id_from_physical_address(sc, cfb_cap->c_base,
	    cfb_cap->c_length, cfb_id_out);
}

/**
 * Find the VM map entry for the given vm_object (representing a single CFB
 * pool) in the given thread, if such a map entry exists.
 *
 * On success, entry_out is set to the map entry, and 0 is returned.
 * If no such map entry exists, entry_out is set to NULL, and -1 is returned.
 */
static int
get_mapping_for_cfb_pool_and_thread(struct cheri_compositor_softc *sc,
    const struct vm_object *vm_object, const struct thread *td,
    struct vm_map_entry **entry_out)
{
	struct vm_map *map;
	struct vm_map_entry *cur, *entry;
	int retval;

	entry = NULL;
	retval = -1;

	map = &td->td_proc->p_vmspace->vm_map;

	CHERI_COMPOSITOR_DEBUG(sc,
	    "vm_object: %p, td: %p, map: %p", vm_object, td, map);

	vm_map_lock_read(map);

	/* Based on vm_map_lookup_entry() in vm_map.c. */
	cur = map->root;
	if (cur == NULL) {
		/* The map is empty. */
		entry = NULL;
		retval = -1;
		goto done;
	} else {
		/* Search through the linked list of entries for our vm_object.
		 * This can't be a binary search or splay tree search, since
		 * the tree is ordered by virtual start and end address. */
		do {
			if (((cur->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) &&
			    cur->object.vm_object == vm_object) {
				/* Found it. */
				entry = cur;
				retval = 0;
				goto done;
			}

			cur = cur->next;
		} while (cur != map->root);
	}

	/* Failure. */
	retval = -1;

done:
	vm_map_unlock_read(map);

	CHERI_COMPOSITOR_DEBUG(sc,
	    "entry: %p (%p, %p), offset: %lu, retval: %i",
	    entry, entry ? (void *) entry->start : 0,
	    entry ? (void *) entry->end : 0, entry ? entry->offset : 0,
	    retval);

	*entry_out = entry;

	return (retval);
}

/**
 * Find the CFB pool corresponding to the given file descriptor for the
 * compositor's device node.
 *
 * On success, cfb_pool_out will be set to the CFB pool, and 0 will be returned.
 * Otherwise, cfb_pool_out will be set to NULL, and EINVAL will be returned.
 */
static int
get_cfb_pool_for_cdev_fd(struct cheri_compositor_softc *sc,
    const struct file *cdev_fd, struct compositor_cfb_pool **cfb_pool_out)
{
	unsigned int i;

	CHERI_COMPOSITOR_DEBUG(sc, "cdev_fd: %p", cdev_fd);

	for (i = 0; i < sizeof(sc->mem_pools) / sizeof(*sc->mem_pools); i++) {
		if (sc->mem_pools[i].mapped_fd == cdev_fd) {
			/* Found it. */
			*cfb_pool_out = &sc->mem_pools[i];

			CHERI_COMPOSITOR_DEBUG(sc,
			    "found CFB pool: %p, mapped FD: %p, "
			    "VM object: %p, next free tile: %lu",
			    *cfb_pool_out, (*cfb_pool_out)->mapped_fd,
			    (*cfb_pool_out)->vm_obj,
			    (*cfb_pool_out)->next_free_tile);

			return 0;
		}
	}

	/* Failed to find a mapping. */
	*cfb_pool_out = NULL;

	CHERI_COMPOSITOR_DEBUG(sc, "failed to find a mapping");

	return EINVAL;
}

/**
 * Find the CFB pool corresponding to the given file descriptor for the
 * compositor's device node, or allocate a new CFB pool to that FD if no
 * allocation exists already.
 *
 * NOTE: This must only be called with the compositor's lock already held.
 *
 * On success, cfb_pool_out will be set to the CFB pool, its reference count
 * will be incremented, and 0 will be returned. Otherwise, if there are no free
 * CFB pools, cfb_pool_out will be set to NULL, and ENOMEM will be returned.
 */
int
dup_or_allocate_cfb_pool_for_cdev_fd(struct cheri_compositor_softc *sc,
    const struct file *cdev_fd, struct vm_object *vm_obj,
    struct compositor_cfb_pool **cfb_pool_out)
{
	unsigned int i;
	int first_free = -1;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "cdev_fd: %p, vm_obj: %p", cdev_fd, vm_obj);

	/* Find the mapping, or the first free mapping. */
	for (i = 0; i < sizeof(sc->mem_pools) / sizeof(*sc->mem_pools); i++) {
		if (sc->mem_pools[i].mapped_fd == cdev_fd) {
			/* Found it. */
			*cfb_pool_out = &sc->mem_pools[i];
			sc->mem_pools[i].ref_count++;

			CHERI_COMPOSITOR_DEBUG(sc,
			    "found pool: %p (mapped FD: %p, VM object: %p, "
			    "next free tile: %lu)",
			    *cfb_pool_out, (*cfb_pool_out)->mapped_fd,
			    (*cfb_pool_out)->vm_obj,
			    (*cfb_pool_out)->next_free_tile);

			return 0;
		} else if (first_free == -1 &&
		    sc->mem_pools[i].mapped_fd == NULL) {
			/* Found a free mapping. */
			first_free = i;
		}
	}

	/* Didn't find the requested mapping, so try and allocate a new one. */
	if (first_free != -1) {
		*cfb_pool_out = &sc->mem_pools[first_free];
		sc->mem_pools[first_free].mapped_fd = cdev_fd;
		sc->mem_pools[first_free].vm_obj = vm_obj;
		sc->mem_pools[first_free].next_free_tile = 0;
		sc->mem_pools[first_free].ref_count = 1;

		if (vm_obj != NULL) {
			vm_object_reference(vm_obj);
		}

		CHERI_COMPOSITOR_DEBUG(sc, "allocating pool: %p",
		    *cfb_pool_out);

		return 0;
	}

	/* Failed to find a free mapping. */
	*cfb_pool_out = NULL;

	CHERI_COMPOSITOR_ERROR(sc, "failed to find mapping");

	return ENOMEM;
}

static void recalculate_tile_cache(struct cheri_compositor_softc *sc,
    const struct tile_region *invalidated_region);

/**
 * Unreference a CFB pool. If the pool's reference count reaches 0, it will be
 * destroyed along with all its CFBs. In this case, the tile caches will be
 * updated, so any CFBs in the pool which were still visible on-screen will be
 * hidden.
 *
 * A CFB pool should only end up being destroyed if all memory mappings of it
 * are unmapped.
 *
 * NOTE: This must only be called with the compositor's lock already held.
 */
void
unref_cfb_pool(struct cheri_compositor_softc *sc,
    struct compositor_cfb_pool *cfb_pool)
{
	struct compositor_cfb *cfb, *cfb_temp;
	unsigned int num_cfbs_destroyed = 0;
	struct tile_region invalidation_region = { { 0, }, { 0, } };

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);

	KASSERT(cfb_pool->ref_count > 0,
	    ("invalid ref count (%u) for pool %p\n",
	     cfb_pool->ref_count, cfb_pool));
	cfb_pool->ref_count--;

	CHERI_COMPOSITOR_DEBUG(sc, "decremented ref count of pool %p to %u",
	    cfb_pool, cfb_pool->ref_count);

	if (cfb_pool->ref_count > 0) {
		/* CFB pool is still alive. */
		return;
	}

	/* Destroy the pool's CFBs. This has to use a safe iterator as it
	 * potentially removes elements from the list. */
	LIST_FOREACH_SAFE(cfb, &sc->cfbs, z_ordering, cfb_temp) {
		if (cfb->allocated_tiles_base >=
		        compositor_cfb_pool_to_tile_offset(sc, cfb_pool) &&
		    cfb->allocated_tiles_base <
		        compositor_cfb_pool_to_tile_offset(sc, cfb_pool) +
		            CHERI_COMPOSITOR_MEM_POOL_LENGTH_TILES) {
			struct tile_region r;

			/* This CFB is in the pool being destroyed. */
			calculate_invalidation_region_for_cfb(sc, cfb, &r);
			calculate_invalidation_region_union(
			    &invalidation_region, &r, &invalidation_region);

			/* Note: free_cfb() removes the CFB from the list. */
			free_cfb(sc, cfb);
			num_cfbs_destroyed++;
		}
	}

	if (num_cfbs_destroyed > 0) {
		/* Resort the CFBs and recalculate the tile cache entries. */
		compositor_sort_cfbs(sc);
		recalculate_tile_cache(sc, &invalidation_region);
	}

	/* Destroy the pool itself. Note: No need to unref vm_obj here, as
	 * that's done by the VM subsystem when munmap() (or equivalent) is
	 * called. */
	cfb_pool->mapped_fd = NULL;
	cfb_pool->vm_obj = NULL;
	cfb_pool->next_free_tile = 0;
}

/* NOTE: This must only be called with the compositor's lock already held. */
static int
compositor_allocate_cfb(struct cheri_compositor_softc *sc,
    const struct vm_object *vm_obj, const struct thread *td,
    unsigned int width, unsigned int height, struct chericap *cfb_cap_out)
{
	struct compositor_command command = { { 0, }, };
	struct compositor_response response = { { 0, }, };
	unsigned int width_tiles, height_tiles;
	struct compositor_cfb *new_cfb = NULL;
	size_t allocation_offset = 0, allocation_length = 0; /* bytes */
	int retval = 0;
	compositor_tile_offset_t allocated_tiles_base;
	unsigned int allocation_order;
	boolean_t width_is_odd, height_is_odd;
	struct cfb_vm_object *cfb_vm_obj;
	struct compositor_cfb_pool *cfb_pool;
	struct vm_map_entry *entry;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "Allocating CFB (width: %u, height: %u)…",
	    width, height);

	/* Input checking. */
	if (vm_obj == NULL || cfb_cap_out == NULL) {
		retval = EINVAL;
		goto done;
	}
	if (width == 0 || width > MAX_X_RES || (width % TILE_SIZE) != 0 ||
	    height == 0 || height > MAX_Y_RES || (height % TILE_SIZE) != 0) {
		retval = EDOM;
		goto done;
	}

	cfb_vm_obj = vm_obj->handle;
	cfb_pool = cfb_vm_obj->pool;

	CHERI_COMPOSITOR_DEBUG(sc,
	    "Using cfb_vm_obj: %p and cfb_pool: %p", cfb_vm_obj, cfb_pool);

	/* Convert the width and height to tiles. */
	width_tiles = DIVIDE_ROUND(width, TILE_SIZE);
	height_tiles = DIVIDE_ROUND(height, TILE_SIZE);

	KASSERT(width_tiles > 0 && height_tiles > 0,
	    ("invalid width (%u) or height (%u)", width_tiles, height_tiles));

	/* Allocate the CFB in compositor memory. FIXME: For the moment this is
	 * a simple counting allocator, but it should eventually be something
	 * slightly more normal (e.g. buddy allocator). */

	/* Calculate the order of the allocation, in tiles. Note that
	 * width_tiles and height_tiles are given in tiles. */
	allocation_order = (fls(width_tiles) - 1) + (fls(height_tiles) - 1);

	/* If either of the width/height is not a power of two, increment the
	 * allocation order. */
	width_is_odd = (bitcount32(width_tiles) != 1);
	height_is_odd = (bitcount32(height_tiles) != 1);

	if (width_is_odd && height_is_odd) {
		allocation_order += 2;
	} else if (width_is_odd || height_is_odd) {
		allocation_order += 1;
	}

	/* Debug output. */
	CHERI_COMPOSITOR_DEBUG(sc,
	    "next free tile: %lu, allocation_order: %u, "
	    "width_is_odd: %u, height_is_odd: %u…",
	    cfb_pool->next_free_tile, allocation_order, width_is_odd,
	    height_is_odd);

	/* Check if there's enough space in the pool to satisfy the request. */
	if (CHERI_COMPOSITOR_MEM_POOL_LENGTH_TILES - cfb_pool->next_free_tile <
	    (1 << allocation_order)) {
		/* Failure. */
		retval = ENOMEM;
		goto done;
	}

	/* Allocate space for the CFB's tile data. The returned allocation is in
	 * bytes, but the stored next_free_tile is in tiles (updated
	 * below). allocated_tiles_base is relative to the start of *compositor*
	 * memory, not the pool. This is because the hardware doesn't know about
	 * pools. Conversely, the next_free_tile is stored per-pool and hence
	 * is relative to the pool. */
	allocated_tiles_base =
	    compositor_cfb_pool_to_tile_offset(sc, cfb_pool) +
	        cfb_pool->next_free_tile;
	allocation_offset =
	    tile_offset_to_byte_offset(cfb_pool->next_free_tile);
	allocation_length =
	    tile_offset_to_byte_offset(width_tiles * height_tiles);

	CHERI_COMPOSITOR_DEBUG(sc,
	    "allocated_tiles_base: %lu, allocation_offset: %lu, "
	    "allocation_length: %lu", allocated_tiles_base,
	    allocation_offset, allocation_length);

	/* Inform the hardware of the allocation. */
	SEQ_NUM_INC(sc->seq_num);
	command.header.seq_num = sc->seq_num;
	command.header.opcode = OPCODE_ALLOCATE_CFB;
	command.payload =
	    PAYLOAD_ALLOCATE_CFB(width_tiles, height_tiles,
	        allocated_tiles_base);
	compositor_send_command(sc, &command, &response, false);

	if (response.header.status != STATUS_SUCCESS) {
		retval = EIO;
		goto done;
	}

	CHERI_COMPOSITOR_DEBUG(sc,
	    "Got allocation [%p, %p) in pool %p [%p, %p).",
	    (void *) allocation_offset,
	    (void *) (allocation_offset + allocation_length), cfb_pool,
	    (void *) compositor_cfb_pool_to_byte_offset(sc, cfb_pool),
	    (void *) (compositor_cfb_pool_to_byte_offset(sc, cfb_pool) +
	              CHERI_COMPOSITOR_MEM_POOL_LENGTH));

	/* Get the mapping for the CFB pool. */
	if (get_mapping_for_cfb_pool_and_thread(sc, vm_obj, td, &entry) != 0) {
		retval = EFAULT;
		goto done;
	}

	/* Commit the allocation. */
	cfb_pool->next_free_tile += (1 << allocation_order);

	/* Construct a capability to identify the CFB. */
	construct_capability_for_cfb_pool_and_offset(sc, entry,
	    allocation_offset, allocation_length, cfb_cap_out);

	new_cfb = malloc(sizeof(*new_cfb), M_CHERI_COMPOSITOR, M_WAITOK);
	CHERI_COMPOSITOR_DEBUG(sc, "malloc()ed new CFB: %p", new_cfb);
	if (new_cfb == NULL) {
		retval = ENOMEM;
		goto done;
	}

	LIST_INSERT_HEAD(&sc->cfbs, new_cfb, z_ordering);

	new_cfb->x = 0;
	new_cfb->y = 0;
	new_cfb->z = 0; /* 0 means bottom-most */
	new_cfb->allocated_tiles_base = allocated_tiles_base;
	new_cfb->width = width_tiles;
	new_cfb->height = height_tiles;
	new_cfb->update_in_progress = true;
	new_cfb->opaque_x = 0;
	new_cfb->opaque_y = 0;
	new_cfb->opaque_width = 0;
	new_cfb->opaque_height = 0;

	CHERI_COMPOSITOR_DEBUG(sc, "new_cfb: %p", new_cfb);

	/* Re-sort CFBs. No need to update the cache yet because we don't know
	 * the CFB's final position, and it's still marked as being updated. */
	compositor_sort_cfbs(sc);

done:
	CHERI_COMPOSITOR_DEBUG(sc, "Finished allocating (retval: %u).", retval);

	return retval;
}

/* x and y specify the tile to update are in tiles relative to the top-left of
 * the screen. tile_x_offset and tile_y_offset specify the position of the
 * top-left corner of the CFB at this layer on this tile, are in pixels relative
 * to the top-left of the screen. allocated_tiles_base is an offset from the
 * start of compositor memory, in tiles.
 *
 * NOTE: This is an internal function, and thus only performs input validation
 * if debugging is enabled. It should not be called from an ioctl() without
 * extra validation. */
static int
compositor_update_tile_cache_entry_internal(struct cheri_compositor_softc *sc,
    unsigned int layer, unsigned int x, unsigned int y,
    unsigned int is_opaque, unsigned int tile_x_offset,
    unsigned int tile_y_offset, uint64_t allocated_tiles_base,
    unsigned int width, unsigned int height)
{
	int retval = 0;

	struct compositor_command command = { { 0, }, };
	struct compositor_response response = { { 0, }, };

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);

#if defined(CHERI_COMPOSITOR_DEBUG_VERBOSE) && CHERI_COMPOSITOR_DEBUG_VERBOSE
	/* Debug output. */
	CHERI_COMPOSITOR_DEBUG(sc, "Updating tile cache entry (layer: %u, "
	    "x: %u, y: %u, is_opaque: %u, tile_x_offset: %u, "
	    "tile_y_offset: %u, allocated_tiles_base: %lu, width: %u, "
	    "height: %u)…", layer, x, y, is_opaque, tile_x_offset,
	    tile_y_offset, allocated_tiles_base, width, height);

	/* Input validation. */
	if (layer >= MAX_LAYERS ||
	    x >= DIVIDE_ROUND(MAX_X_RES, TILE_SIZE) ||
	    y >= DIVIDE_ROUND(MAX_Y_RES, TILE_SIZE) ||
	    tile_x_offset >= MAX_X_RES || tile_y_offset >= MAX_Y_RES ||
	    width > DIVIDE_ROUND(MAX_X_RES, TILE_SIZE) ||
	    height > DIVIDE_ROUND(MAX_Y_RES, TILE_SIZE)) {
		retval = EDOM;
		goto done;
	}
	if (!tile_offset_is_in_compositor_mem(sc, allocated_tiles_base)) {
		retval = EINVAL;
		goto done;
	}
#endif

	/* Send an UpdateTileCacheEntry command to the hardware. */
	SEQ_NUM_INC(sc->seq_num);
	command.header.seq_num = sc->seq_num;
	command.header.opcode = OPCODE_UPDATE_TILE_CACHE_ENTRY;
	command.payload =
	    PAYLOAD_UPDATE_TILE_CACHE_ENTRY(layer,
	        y * (MAX_X_RES / TILE_SIZE) + x /* address */,
	        TILE_CACHE_ENTRY(is_opaque, tile_x_offset, tile_y_offset,
	            allocated_tiles_base, width, height));
	compositor_send_command(sc, &command, &response, false);

	retval = (response.header.status == STATUS_SUCCESS) ? 0 : EIO;

#if defined(CHERI_COMPOSITOR_DEBUG_VERBOSE) && CHERI_COMPOSITOR_DEBUG_VERBOSE
done:
	CHERI_COMPOSITOR_DEBUG(sc, "Finished updating tile cache entry "
	    "(retval: %u).", retval);
#endif

	return retval;
}

/* A dummy CFB used to set tile cache entries if not enough CFBs intersect that
 * output tile. */
static const struct compositor_cfb dummy_cfb = {
	.z_ordering = { 0, },
	.x = 0,
	.y = 0,
	.z = 0, /* 0 means bottom-most */
	.allocated_tiles_base = 666,
	.width = 0,
	.height = 0,
	.update_in_progress = true,
	.opaque_x = 0,
	.opaque_y = 0,
	.opaque_width = 0,
	.opaque_height = 0,
};

/* Calculate whether the given tile in a CFB is opaque. Note that x and y are
 * given in tiles relative to the top-left of the screen, not the CFB. */
static boolean_t
is_tile_opaque(unsigned int x, unsigned int y, const struct compositor_cfb *cfb)
{
	/* Work everything out in pixels. */
	x *= TILE_SIZE;
	y *= TILE_SIZE;

	return (x >= cfb->opaque_x && x < cfb->opaque_x + cfb->opaque_width &&
	        y >= cfb->opaque_y && y < cfb->opaque_y + cfb->opaque_height);
}

/* FIXME: This is nasty and inefficient and should be moved into hardware
 * because parallelisation.
 * NOTE: This must only be called with the compositor's lock already held. */
static void
recalculate_tile_cache(struct cheri_compositor_softc *sc,
    const struct tile_region *invalidated_region)
{
	unsigned int x, y, i;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "Over region (%u, %u), (%u, %u)",
	    invalidated_region->top_left.x, invalidated_region->top_left.y,
	    invalidated_region->bottom_right.x,
	    invalidated_region->bottom_right.y);

	/* Iterate over all the output tiles, calculating which CFBs are visible
	 * on each tile, and their relative Z orderings. */
	for (y = invalidated_region->top_left.y;
	     y <= invalidated_region->bottom_right.y;
	     y++) {
		for (x = invalidated_region->top_left.x;
		     x <= invalidated_region->bottom_right.x;
		     x++) {
			/* An array of the final Z ordering for this output
			 * tile. It is populated from the top-most layer down to
			 * the bottom-most layer by iterating over the
			 * z_ordered sorting of the cfbs array (which is kept
			 * sorted by increasing depth) and selecting CFBs which
			 * intersect this output tile. */
			const struct compositor_cfb *cfb;
			const struct compositor_cfb *z_ordering[MAX_LAYERS] =
			    { 0, };
			unsigned int z_ordering_index = 0;

			LIST_FOREACH(cfb, &sc->cfbs, z_ordering) {
				/* If the CFB is being updated, discard it. */
				if (cfb->update_in_progress) {
					/* Run away! (But come back later!) */
					continue;
				}

				/* If the CFB is odd (e.g. only just allocated),
				 * discard it. */
				if (cfb->width == 0 || cfb->height == 0) {
					continue;
				}

				/* If the CFB doesn't intersect this output
				 * tile, discard it. */
				if (cfb->x + cfb->width * TILE_SIZE - 1 < x * TILE_SIZE ||
				    cfb->x >= (x + 1) * TILE_SIZE ||
				    cfb->y + cfb->height * TILE_SIZE - 1 < y * TILE_SIZE ||
				    cfb->y >= (y + 1) * TILE_SIZE) {
					/* Run away! */
					continue;
				}

				/* This CFB intersects the output tile, so
				 * insert an entry into the ordering. */
				z_ordering[z_ordering_index++] = cfb;
				if (z_ordering_index >= MAX_LAYERS) {
					/* Finished? */
					break;
				}
			}

			/* Fill the rest of the cache entries for this output
			 * tile with dummy values. */
			for (i = z_ordering_index; i < MAX_LAYERS; i++) {
				z_ordering[i] = &dummy_cfb;
			}

			/* Update the TileCacheEntrys for this output tile. */
			for (i = 0; i < MAX_LAYERS; i++) {
				boolean_t is_opaque;
				boolean_t is_tile_aligned_x, is_tile_aligned_y;
				boolean_t is_left_tile, is_right_tile;
				boolean_t is_top_tile, is_bottom_tile;

				is_opaque = is_tile_opaque(x, y, z_ordering[i]);
				is_tile_aligned_x =
				    ((z_ordering[i]->x % TILE_SIZE) == 0);
				is_tile_aligned_y =
				    ((z_ordering[i]->y % TILE_SIZE) == 0);

				is_left_tile =
				    !is_tile_aligned_x &&
				    x * TILE_SIZE < z_ordering[i]->x;
				is_right_tile =
				    !is_tile_aligned_x &&
				    (x + 1) * TILE_SIZE >=
				        z_ordering[i]->x +
				            z_ordering[i]->width * TILE_SIZE;
				is_top_tile =
				    !is_tile_aligned_y &&
				    y * TILE_SIZE < z_ordering[i]->y;
				is_bottom_tile =
				    !is_tile_aligned_y &&
				    (y + 1) * TILE_SIZE >=
				        z_ordering[i]->y +
				            z_ordering[i]->height * TILE_SIZE;

				if (compositor_update_tile_cache_entry_internal(
				        sc, i /* layer */, x, y, is_opaque,
				        z_ordering[i]->x, z_ordering[i]->y,
				        z_ordering[i]->allocated_tiles_base,
				        z_ordering[i]->width,
				        z_ordering[i]->height) != 0) {
					/* Error! */
					CHERI_COMPOSITOR_ERROR(sc,
					    "Expected success response to "
					    "UpdateTileCacheEntry command.");
				}

				/* Bail as soon as we descend to an opaque
				 * layer, except if this is the a border
				 * tile of a non-tile-aligned CFB, in which
				 * case we need to update the tile cache entry
				 * for the layer below to allow for it to show
				 * through. */
				if (is_opaque &&
				    !(is_left_tile || is_right_tile ||
				      is_top_tile || is_bottom_tile)) {
					break;
				}
			}
		}
	}
}

/* NOTE: This must only be called with the compositor's lock already held. */
static struct compositor_cfb *
compositor_find_cfb(struct cheri_compositor_softc *sc,
    const struct compositor_cfb_id *cfb_id)
{
	compositor_tile_offset_t expected_allocated_tiles_base /* in tiles */;
	struct compositor_cfb *cfb;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);

	/* Bounds checking. */
	KASSERT_CFB_ID_IS_VALID(sc, cfb_id);

	/* Find the given CFB in the cfbs array. FIXME: This could be more
	 * efficient. */
	expected_allocated_tiles_base =
	    compositor_cfb_pool_to_tile_offset(sc, cfb_id->pool) +
	        cfb_id->offset;

	CHERI_COMPOSITOR_DEBUG(sc, "expected_allocated_tiles_base: %p",
	    (void *) (rman_get_start(sc->compositor_cfb_res) +
	        tile_offset_to_byte_offset(expected_allocated_tiles_base)));

	LIST_FOREACH(cfb, &sc->cfbs, z_ordering) {
		if (cfb->allocated_tiles_base ==
		    expected_allocated_tiles_base) {
			/* Found it. */
			return cfb;
		}
	}

	return NULL;
}

/* Validate the given cfb_cap, and return EINVAL if invalid. If it's valid,
 * search for the corresponding cfb in the cfbs array. If found, return 0 and
 * return the CFB in cfb_out. Otherwise, return EFAULT.
 *
 * NOTE: This must only be called with the compositor's lock already held. */
static int
is_valid_cfb_cap(struct cheri_compositor_softc *sc, struct chericap *cfb_cap,
    const struct compositor_cfb_pool *expected_cfb_pool,
    const struct vm_map_entry *entry, struct compositor_cfb **cfb_out)
{
	int retval;
	unsigned int tag;
	uint32_t perms;
	struct compositor_cfb *cfb;
	struct compositor_cfb_id cfb_id;

	/* Clear the output in case of error. */
	retval = EINVAL;
	cfb = NULL;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "cfb_cap: %p, expected_cfb_pool: %p",
	    cfb_cap, expected_cfb_pool);

	/* To be a valid CFB ID, it must be a valid capability, must lie in
	 * the interior of an appropriate CFB pool, must be readable and
	 * writeable, and must exist in the cfbs array. */
	cheri_capability_load(1, cfb_cap);

	/* Check the tag. */
	CHERI_CGETTAG(tag, 1);
	if (tag != 1) {
		goto done;
	}

	/* Check it fits into an appropriate CFB pool. */
	extract_cfb_id_from_cfb_cap_id(sc, expected_cfb_pool, entry, cfb_cap,
	    &cfb_id);

	CHERI_COMPOSITOR_DEBUG(sc, "cfb_id: %p (%p: %lu, %lu), "
	    "sc->mem_pools: [%p, %p)", &cfb_id, cfb_id.pool, cfb_id.offset,
	    cfb_id.length, sc->mem_pools,
	    (void *) ((uintptr_t) sc->mem_pools + sizeof(sc->mem_pools)));

	if (!cfb_id_is_valid(sc, &cfb_id) || cfb_id.pool != expected_cfb_pool) {
		goto done;
	}

	/* Check the permissions. */
	CHERI_CGETPERM(perms, 1);

	if ((perms & CHERI_PERM_LOAD) == 0 ||
	    (perms & CHERI_PERM_STORE) == 0) {
		goto done;
	}

	/* Check it exists in the CFBs array. */
	cfb = compositor_find_cfb(sc, &cfb_id);
	if (cfb == NULL) {
		retval = EFAULT;
		goto done;
	}

	/* Success! */
	retval = 0;

done:
	/* Tidy up. */
	CHERI_CCLEARTAG(1, 1);

	*cfb_out = cfb;
	return retval;
}

/**
 * Construct a capability to use to seal CFB tokens. This acts as an identifier
 * or type for CFB tokens, to differentiate them from other sealed capabilities.
 * User-space processes may not forge this seal, as they do not have access to
 * kernel code in memory (and so cannot construct a capability with otype/eaddr
 * of this function address). This is an important security property, as it
 * prevents user-space processes unsealing a CFB token and gaining a capability
 * to write directly to physically addressed compositor memory. (Though this may
 * not actually be useful to them.)
 */
static void
get_kernel_seal_capability(struct cheri_compositor_softc *sc,
    const struct compositor_cfb_pool *cfb_pool, struct chericap *cap_out)
{
	CHERI_COMPOSITOR_DEBUG(sc, "cfb_pool: %p, cap_out: %p", cfb_pool,
	    cap_out);

#ifdef BROKEN
	/*
	 * XXXRW: CHERI ISAv3 doesn't allow setting the type without
	 * simultaneously sealing.
	 */
	/* FIXME: Use pool as well? */
	CHERI_CINCBASE(1, CHERI_CR_KDC, 0);
	CHERI_CSETLEN(1, 1, 0);
	CHERI_CSETTYPE(1, CHERI_CR_KCC, get_kernel_seal_capability);
	CHERI_CANDPERM(1, 1, CHERI_PERM_SEAL);
#endif

	cheri_capability_store(1, cap_out);
	CHERI_CCLEARTAG(1, 1);
}

/* Validate the given sealed cfb_cap, and return EINVAL if invalid. If it's
 * valid, search for the corresponding cfb in the cfbs array. If found, return 0
 * and return the CFB in cfb_out. Otherwise, return EFAULT.
 *
 * NOTE: This must only be called with the compositor's lock already held. */
static int
is_valid_cfb_cap_token(struct cheri_compositor_softc *sc,
    struct chericap *cfb_cap,
    struct compositor_cfb **cfb_out, struct compositor_cfb_id *cfb_id_out)
{
	int retval;
	unsigned int tag, sealed;
	struct compositor_cfb *cfb;
	struct chericap *seal_cap;

	/* Clear the output in case of error. */
	retval = EINVAL;
	cfb = NULL;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);	
	CHERI_COMPOSITOR_DEBUG(sc, "cfb_cap: %p", cfb_cap);

	/* To be a valid CFB token, it must be a valid and sealed capability.
	 * Its base/length must lie in the interior of the CFB pool specified in
	 * its otype/addr, and must exist in the CFBs array. Its otype/addr must
	 * be as constructed by the kernel. */
	cheri_capability_load(1, cfb_cap);

	/* Check the tag. */
	CHERI_CGETTAG(tag, 1);
	CHERI_COMPOSITOR_DEBUG(sc, "tag: %u", tag);
	if (tag != 1) {
		goto done;
	}

	/* Check it's sealed. */
	CHERI_CGETSEALED(sealed, 1);
	CHERI_COMPOSITOR_DEBUG(sc, "sealed: %u", sealed);
	if (sealed == 0) {
		goto done;
	}

	/* FIXME: seal_cap doesn't seem to be properly aligned when
	 * allocated on the stack. */
	seal_cap = malloc(sizeof(*seal_cap), M_CHERI_COMPOSITOR,
	    M_WAITOK | M_ZERO);

	if (seal_cap == NULL) {
		retval = ENOMEM;
		goto done;
	}

	/* Check it fits into an appropriate CFB pool. */
	extract_cfb_id_from_cfb_cap_token(sc, cfb_cap, cfb_id_out);

	CHERI_COMPOSITOR_DEBUG(sc, "cfb_id: %p (%p: %lu, %lu), "
	    "sc->mem_pools: [%p, %p)", cfb_id_out, cfb_id_out->pool,
	    cfb_id_out->offset, cfb_id_out->length, sc->mem_pools,
	    (void *) ((uintptr_t) sc->mem_pools + sizeof(sc->mem_pools)));

	if (!cfb_id_is_valid(sc, cfb_id_out)) {
		goto done;
	}

	/* Check its seal is correct. */
	get_kernel_seal_capability(sc, cfb_id_out->pool, seal_cap);
	cheri_capability_load(2, seal_cap);

	/* Unseal the token, effectively using the hardware to verify its
	 * security rather than emulating the checks in software. Unseal $c1
	 * with seal $c2 and put the result in $c3. */
	cheri_capability_load(1, cfb_cap);
	CHERI_CUNSEAL(3, 1, 2);

	/* Check it exists in the CFBs array. */
	cfb = compositor_find_cfb(sc, cfb_id_out);
	if (cfb == NULL) {
		retval = EFAULT;
		goto done;
	}

	/* Success! */
	retval = 0;

done:
	/* Tidy up. */
	CHERI_CCLEARTAG(1, 1);
	CHERI_CCLEARTAG(2, 2);
	CHERI_CCLEARTAG(3, 3);

	*cfb_out = cfb;

	return retval;
}

/* NOTE: This must only be called with the compositor's lock already held.
 * cfb_cap is a capability giving access to the CFB's pixel data. */
static int
compositor_free_cfb(struct cheri_compositor_softc *sc, const struct thread *td,
    const struct compositor_cfb_pool *cfb_pool, struct chericap *cfb_cap)
{
	struct compositor_cfb *cfb;
	struct tile_region invalidation_region;
	int retval = 0;
	struct vm_map_entry *entry;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "cfb_cap: %p", cfb_cap);

	/* Find the VM mapping for the FD. */
	if (get_mapping_for_cfb_pool_and_thread(sc, cfb_pool->vm_obj, td,
	    &entry) != 0) {
		retval = EFAULT;
		goto done;
	}

	/* Check inputs, and if they're valid, return the given CFB in the cfbs
	 * array. */
	retval = is_valid_cfb_cap(sc, cfb_cap, cfb_pool, entry, &cfb);
	if (retval != 0) {
		goto done;
	}

	/* Calculate the maximum region of tiles invalidated by the removal of
	 * the CFB. */
	calculate_invalidation_region_for_cfb(sc, cfb, &invalidation_region);

	/* Free the CFB and remove it from the cfbs list. */
	free_cfb(sc, cfb);
	cfb = NULL;

	/* Resort the CFBs and recalculate tile cache entries.
	 * FIXME: Could also only update if nothing's changed, or if
	 * update_in_progress has changed. */
	compositor_sort_cfbs(sc);
	recalculate_tile_cache(sc, &invalidation_region);

done:
	CHERI_COMPOSITOR_DEBUG(sc, "Finished freeing CFB (retval: %u).",
	    retval);

	return retval;
}

/* NOTE: This must only be called with the compositor's lock already held.
 * cfb_cap is a capability giving access to the CFB's pixel data. x_position and
 * y_position are in pixels from the top-left of the screen.
 * opaque_[x|y|width|height] describe the (potentially empty) region of the CFB
 * which is opaque, in pixels relative to the top-left of the CFB. */
static int
compositor_update_cfb(struct cheri_compositor_softc *sc,
    const struct compositor_cfb_pool *cfb_pool, struct chericap *cfb_cap,
    unsigned int x_position, unsigned int y_position, unsigned int z_position,
    unsigned int opaque_x, unsigned int opaque_y,
    unsigned int opaque_width, unsigned int opaque_height,
    boolean_t update_in_progress)
{
	struct compositor_cfb_id cfb_id;
	struct compositor_cfb *cfb, old_cfb;
	struct tile_region old_invalidation_region, new_invalidation_region,
	   combined_invalidation_region;
	int retval = 0;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "cfb_cap: %p, x_position: %u, "
	    "y_position: %u, z_position: %u, update_in_progress: %u\n", cfb_cap,
	    x_position, y_position, z_position, update_in_progress);

	/* Check inputs and return the given CFB in the cfbs array if valid. */
	retval = is_valid_cfb_cap_token(sc, cfb_cap, &cfb, &cfb_id);
	if (retval != 0) {
		goto done;
	}
	if (x_position >= MAX_X_RES || y_position >= MAX_Y_RES ||
	    z_position > MAX_Z) {
		retval = EDOM;
		goto done;
	}

	CHERI_COMPOSITOR_DEBUG(sc, "cfb: %p", cfb);

	/* Save the CFB's old details for use in calculating the invalidation
	 * region, below. */
	memcpy(&old_cfb, cfb, sizeof(*cfb));

	/* Update the CFB's details. */
	cfb->x = x_position;
	cfb->y = y_position;
	cfb->z = z_position;
	cfb->opaque_x = opaque_x;
	cfb->opaque_y = opaque_y;
	cfb->opaque_width = opaque_width;
	cfb->opaque_height = opaque_height;
	cfb->update_in_progress = update_in_progress;

	/* Resort the CFBs. We only need to resort if the Z coordinate or
	 * update_in_progress bit has changed. */
	if (old_cfb.z != cfb->z ||
	    old_cfb.update_in_progress != cfb->update_in_progress) {
		compositor_sort_cfbs(sc);
	}

	/* Calculate the maximum region of tiles invalidated by moving the
	 * CFB. One region for the CFB's old position, one for its new position,
	 * and one as the union of the two. The region used in the
	 * recalculate_tile_cache() call depends on whether the CFB was visible
	 * before/now. */
	calculate_invalidation_region_for_cfb(sc, &old_cfb,
	    &old_invalidation_region);
	calculate_invalidation_region_for_cfb(sc, cfb,
	    &new_invalidation_region);
	calculate_invalidation_region_union(&old_invalidation_region,
	    &new_invalidation_region, &combined_invalidation_region);

	/* FIXME: Also factor opaque region into invalidation region. */

	/* Recalculate tile cache entries. Don't do so if the CFB wasn't visible
	 * before or now.
	 * FIXME: Could take (lack of) changes in X and Y into account. */
	if (old_cfb.update_in_progress && !cfb->update_in_progress) {
		/* Wasn't visible before && visible now => invalidate the new
		 * location only. */
		recalculate_tile_cache(sc, &new_invalidation_region);
	} else if (!old_cfb.update_in_progress && cfb->update_in_progress) {
		/* Was visible before && not visible now => invalidate the old
		 * location only. */
		recalculate_tile_cache(sc, &old_invalidation_region);
	} else if (!old_cfb.update_in_progress && !cfb->update_in_progress) {
		/* Visible before && now => invalidate both locations. */
		recalculate_tile_cache(sc, &combined_invalidation_region);
	}

done:
	CHERI_COMPOSITOR_DEBUG(sc, "Finished updating CFB (retval: %u)",
	    retval);

	return retval;
}

/* NOTE: This must only be called with the compositor's lock already held. */
static int
compositor_set_configuration(struct cheri_compositor_softc *sc,
    const struct cheri_compositor_configuration *configuration)
{
	struct compositor_command command = { { 0, }, };
	struct compositor_response response = { { 0, }, };
	int retval = 0;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc,
	    "Setting configuration (x_resolution: %u, y_resolution: %u).",
	    configuration->x_resolution, configuration->y_resolution);

	/* Check inputs. */
	if (configuration == NULL) {
		retval = EINVAL;
		goto done;
	}
	if (configuration->x_resolution > MAX_X_RES ||
	    configuration->y_resolution > MAX_Y_RES) {
		retval = EDOM;
		goto done;
	}
	if ((configuration->x_resolution == 0) !=
	    (configuration->y_resolution == 0)) {
		retval = EINVAL;
		goto done;
	}

	/* Send a SetConfiguration command to the hardware. */
	SEQ_NUM_INC(sc->seq_num);
	command.header.seq_num = sc->seq_num;
	command.header.opcode = OPCODE_SET_CONFIGURATION;
	command.payload =
	    PAYLOAD_SET_CONFIGURATION(configuration->x_resolution,
	       configuration->y_resolution);
	compositor_send_command(sc, &command, &response, false);

	if (response.header.status == STATUS_SUCCESS) {
		/* Update our copies in memory. */
		memcpy(&sc->configuration, configuration,
		    sizeof(sc->configuration));
	} else {
		/* Debug. */
		CHERI_COMPOSITOR_ERROR(sc,
		    "Expected success response to SetConfiguration command.");
	}

	retval = (response.header.status == STATUS_SUCCESS) ? 0 : EIO;

done:
	CHERI_COMPOSITOR_DEBUG(sc,
	    "Finished setting configuration (retval: %u).", retval);

	return retval;
}

/**
 * Convert a CFB ID capability to a CFB token capability. The former is
 * virtually addressed and unsealed; the latter is physically addressed and
 * sealed.
 *
 * NOTE: This must only be called with the compositor's lock already held.
 *
 * This performs all necessary validation and returns error codes as
 * appropriate. On success, 0 is returned.
 */
static int
compositor_cfb_id_to_token(struct cheri_compositor_softc *sc,
    const struct vm_object *vm_obj, const struct thread *td,
    struct compositor_cfb_pool *cfb_pool, struct chericap *cfb_cap_in,
    struct chericap *cfb_cap_out)
{
	int retval = 0;
	struct compositor_cfb *cfb;
	struct chericap *seal_cap;
	vm_paddr_t physical_base_address;
	struct vm_map_entry *entry;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "vm_obj: %p, td: %p, cfb_pool: %p, "
	    "cfb_cap_in: %p, cfb_cap_out: %p", vm_obj, td, cfb_pool, cfb_cap_in,
	    cfb_cap_out);

	/* Find the VM mapping for the FD, so we can translate the CFB ID's
	 * virtual address to its physical address, below. */
	if (get_mapping_for_cfb_pool_and_thread(sc, vm_obj, td, &entry) != 0) {
		retval = EFAULT;
		goto done;
	}

	/* Check the process actually has permission to use cfb_cap_in. */
	retval = is_valid_cfb_cap(sc, cfb_cap_in, cfb_pool, entry, &cfb);
	if (retval != 0) {
		goto done;
	}

	/* Get a capability to seal the CFB's token with. */

	/* FIXME: seal_cap doesn't seem to be properly aligned when
	 * allocated on the stack. */
	seal_cap = malloc(sizeof(*seal_cap), M_CHERI_COMPOSITOR,
	    M_WAITOK | M_ZERO);

	if (seal_cap == NULL) {
		retval = ENOMEM;
		goto done;
	}

	get_kernel_seal_capability(sc, cfb_pool, seal_cap);
	cheri_capability_load(1, seal_cap);

	/* Convert virtual to physical. */
	physical_base_address =
	    convert_virtual_to_physical(sc, cfb_cap_in->c_base, entry,
	        cfb_pool);

	CHERI_CINCBASE(2, CHERI_CR_KDC, physical_base_address);
	CHERI_CSETLEN(2, 2, cfb_cap_in->c_length);
	CHERI_CANDPERM(2, 2, cfb_cap_in->c_perms & CFB_PERMS_MASK);

	/* Seal the token. Construct sealed capability $c3 from base/offset in
	 * $c2 and seal otype/addr in $c1. */
	CHERI_CSEAL(3, 2, 1);

	/* Return. */
	cheri_capability_store(3, cfb_cap_out);

done:
	CHERI_COMPOSITOR_DEBUG(sc,
	    "Converted cfb_cap %p to cfb_token %p (retval: %i).", cfb_cap_in,
	    cfb_cap_out, retval);

	/* Don't leak the capabilities. */
	CHERI_CCLEARTAG(1, 1);
	CHERI_CCLEARTAG(2, 2);
	CHERI_CCLEARTAG(3, 3);

	return retval;
}

/**
 * Convert a CFB token capability to a CFB ID capability. The former is
 * physically addressed and sealed; the latter is virtually addressed and
 * unsealed.
 *
 * NOTE: This must only be called with the compositor's lock already held.
 *
 * This performs all necessary validation and returns error codes as
 * appropriate. On success, 0 is returned.
 */
static int
compositor_cfb_token_to_id(struct cheri_compositor_softc *sc,
    const struct vm_object *vm_obj, const struct thread *td,
    const struct compositor_cfb_pool *cfb_pool, struct chericap *cfb_cap_in,
    struct chericap *cfb_cap_out)
{
	int retval = 0;
	struct compositor_cfb *cfb;
	struct vm_map_entry *entry;
	struct compositor_cfb_id cfb_id;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "vm_obj: %p, td: %p, cfb_pool: %p, "
	    "cfb_cap_in: %p, cfb_cap_out: %p", vm_obj, td, cfb_pool, cfb_cap_in,
	    cfb_cap_out);

	/* Check the process actually has permission to use cfb_cap_in. This
	 * validates the physical address in the capability. */
	retval = is_valid_cfb_cap_token(sc, cfb_cap_in, &cfb, &cfb_id);
	if (retval != 0) {
		goto done;
	}

	/* Convert it to a virtually addressed CFB ID capability. This includes
	 * checking whether the process (td) has access to the pool. */
	if (get_mapping_for_cfb_pool_and_thread(sc, vm_obj, td, &entry) != 0) {
		retval = EFAULT;
		goto done;
	}

	construct_capability_for_cfb_pool_and_offset(sc, entry,
	    tile_offset_to_byte_offset(cfb_id.offset),
	    tile_offset_to_byte_offset(cfb_id.length), cfb_cap_out);

done:
	CHERI_COMPOSITOR_DEBUG(sc,
	    "Converted cfb_token %p to cfb_cap %p (retval: %i).", cfb_cap_in,
	    cfb_cap_out, retval);

	return retval;
}

/**
 * Pause sampling of hardware performance. This will stop incrementing hardware
 * performance counters without resetting their values. It controls both the
 * memory bus sampler (AvalonSampler peripheral) and the compositor hardware’s
 * internal sampling counters, though the two are not paused atomically. This is
 * the inverse of compositor_sampler_unpause_statistics().
 *
 * NOTE: This must only be called with the compositor's lock already held.
 *
 * On success, 0 is returned. On failure, EIO is returned if communication with
 * the compositor failed.
 */
static int
compositor_sampler_pause_statistics(struct cheri_compositor_softc *sc)
{
	struct compositor_command command = { { 0, }, };
	struct compositor_response response = { { 0, }, };
	int retval = 0;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "Pausing statistics");

	/* Pause the compositor itself. */
	SEQ_NUM_INC(sc->seq_num);
	command.header.seq_num = sc->seq_num;
	command.header.opcode = OPCODE_CONTROL_STATISTICS;
	command.payload = PAYLOAD_CONTROL_STATISTICS(0, 1);
	compositor_send_command(sc, &command, &response, false);

	if (response.header.status != STATUS_SUCCESS) {
		retval = EIO;
		goto done;
	}

	/* Pause the sampler peripheral. */
	bus_write_4(sc->compositor_sampler_res,
	    CHERI_COMPOSITOR_SAMPLER_CONTROL,
	    1 << CHERI_COMPOSITOR_SAMPLER_CONTROL_IS_PAUSED_POS);

	sc->is_sampler_paused = 1;

done:
	return retval;
}

/**
 * Unpause sampling of hardware performance. This will re-start incrementing
 * hardware performance counters without resetting their values. This is the
 * inverse of compositor_sampler_pause_statistics().
 *
 * NOTE: This must only be called with the compositor's lock already held.
 *
 * On success, 0 is returned. On failure, EIO is returned if communication with
 * the compositor failed.
 */
static int
compositor_sampler_unpause_statistics(struct cheri_compositor_softc *sc)
{
	struct compositor_command command = { { 0, }, };
	struct compositor_response response = { { 0, }, };
	int retval = 0;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "Unpausing statistics");

	/* Unpause the compositor itself. */
	SEQ_NUM_INC(sc->seq_num);
	command.header.seq_num = sc->seq_num;
	command.header.opcode = OPCODE_CONTROL_STATISTICS;
	command.payload = PAYLOAD_CONTROL_STATISTICS(0, 0);
	compositor_send_command(sc, &command, &response, false);

	if (response.header.status != STATUS_SUCCESS) {
		retval = EIO;
		goto done;
	}

	/* Unpause the sampler peripheral. */
	bus_write_4(sc->compositor_sampler_res,
	    CHERI_COMPOSITOR_SAMPLER_CONTROL,
	    0 << CHERI_COMPOSITOR_SAMPLER_CONTROL_IS_PAUSED_POS);

	sc->is_sampler_paused = 0;

done:
	return retval;
}

/**
 * Reset hardware performance counters to 0. It resets both the memory bus
 * sampler (AvalonSampler peripheral) and the compositor hardware’s internal
 * sampling counters. It will preserve the paused state of both devices (see
 * compositor_sampler_pause_statistics()).
 *
 * NOTE: This must only be called with the compositor's lock already held.
 *
 * On success, 0 is returned. On failure, EIO is returned if communication with
 * the compositor failed.
 */
static int
compositor_sampler_reset_statistics(struct cheri_compositor_softc *sc)
{
	struct compositor_command command = { { 0, }, };
	struct compositor_response response = { { 0, }, };
	int retval = 0;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "Resetting statistics");

	/* Reset the compositor. */
	SEQ_NUM_INC(sc->seq_num);
	command.header.seq_num = sc->seq_num;
	command.header.opcode = OPCODE_CONTROL_STATISTICS;
	command.payload = PAYLOAD_CONTROL_STATISTICS(1, sc->is_sampler_paused);
	compositor_send_command(sc, &command, &response, false);

	if (response.header.status != STATUS_SUCCESS) {
		retval = EIO;
		goto done;
	}

	/* Reset the sampler peripheral. */
	bus_write_4(sc->compositor_sampler_res,
	    CHERI_COMPOSITOR_SAMPLER_CONTROL,
	    (1 << CHERI_COMPOSITOR_SAMPLER_CONTROL_RESET_POS) |
	    (sc->is_sampler_paused <<
	        CHERI_COMPOSITOR_SAMPLER_CONTROL_IS_PAUSED_POS));

done:
	return retval;
}

/**
 * Fetch a page of sample counters from the compositor hardware. Each page
 * contains two 32-bit counters, the most-significant of which is returned in
 * val1_out, and the least-significant is returned in val2_out. The two counters
 * are fetched atomically with respect to each other, and the hardware counters
 * are not reset or paused.
 *
 * FIXME: The use of val1_out and val2_out is ugly and not type-safe.
 *
 * NOTE: This must only be called with the compositor's lock already held.
 *
 * On success, 0 is returned. On failure, EIO is returned if communication with
 * the compositor failed.
 */
static int
compositor_sampler_get_statistics_page(struct cheri_compositor_softc *sc,
    compositor_statistics_page page, uint32_t *val1_out, uint32_t *val2_out)
{
	struct compositor_command command = { { 0, }, };
	struct compositor_response response = { { 0, }, };
	int retval = 0;
	uint32_t val1 = 0, val2 = 0;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "Fetching statistics page %u", page);

	/* Fetch a statistics page. */
	SEQ_NUM_INC(sc->seq_num);
	command.header.seq_num = sc->seq_num;
	command.header.opcode = OPCODE_GET_STATISTICS;
	command.payload = PAYLOAD_GET_STATISTICS(page);
	compositor_send_command(sc, &command, &response, true);

	if (response.header.status != STATUS_SUCCESS) {
		retval = EIO;
		goto done;
	}

	/* Copy out the values. Each is 29b long. */
	val1 = (response.payload >> 32) & 0x1fffffff;
	val2 = (response.payload >> 0) & 0x1fffffff;

done:
	*val1_out = val1;
	*val2_out = val2;

	return retval;
}

/**
 * Fetch sample counters from the compositor hardware and memory bus sampler
 * peripheral. The sampled values are returned in stats. Samples are fetched
 * atomically with respect to each other only if sampling is currently paused
 * (see compositor_sampler_pause_statistics()). Fetching statistics does not
 * affect or reset the value of hardware counters, and the current pause state
 * of the counters will remain unchanged.
 *
 * NOTE: This must only be called with the compositor's lock already held.
 *
 * On success, 0 is returned. On failure, EIO is returned if communication with
 * the compositor failed; or ENOMEM is returned if stats didn't have enough
 * slots in its latency_bins field for the number of latency bins exposed by
 * hardware (the num_latency_bins field, which is guaranteed to be set if ENOMEM
 * is returned).
 */
static int
compositor_sampler_get_statistics(struct cheri_compositor_softc *sc,
    struct cheri_compositor_statistics *stats)
{
	int retval = 0;
	unsigned int i;
	uint32_t latency_bin_configuration;
	boolean_t was_paused;

	CHERI_COMPOSITOR_ASSERT_LOCKED(sc);
	CHERI_COMPOSITOR_DEBUG(sc, "stats: %p", stats);

	/* Pause memory and compositor statistic collection so that they're read
	 * atomically. */
	was_paused = sc->is_sampler_paused;
	if (!was_paused)
		compositor_sampler_pause_statistics(sc);

	/* Read the statistics from the AvalonSampler attached to the
	 * compositor. */
	/* FIXME: The le32toh() calls are because of an endianness problem
	 * somewhere. Potentially in the hardware or bus configuration. Fixing
	 * it here is the quickest (dirtiest) way, and should be tidied up
	 * later. */
	stats->num_read_requests =
	    le32toh(bus_read_4(sc->compositor_sampler_res,
	        CHERI_COMPOSITOR_SAMPLER_NUM_READ_REQUESTS));
	stats->num_write_requests =
	    le32toh(bus_read_4(sc->compositor_sampler_res,
	        CHERI_COMPOSITOR_SAMPLER_NUM_WRITE_REQUESTS));
	stats->num_read_bursts =
	    le32toh(bus_read_4(sc->compositor_sampler_res,
	        CHERI_COMPOSITOR_SAMPLER_NUM_READ_BURSTS));
	stats->num_write_bursts =
	    le32toh(bus_read_4(sc->compositor_sampler_res,
	        CHERI_COMPOSITOR_SAMPLER_NUM_WRITE_BURSTS));

	latency_bin_configuration = bus_read_4(sc->compositor_sampler_res,
	    CHERI_COMPOSITOR_SAMPLER_LATENCY_BIN_CONFIGURATION);
	stats->num_latency_bins = (latency_bin_configuration >> 8) & 0xff;
	stats->latency_bin_upper_bound =
	    (latency_bin_configuration >> 16) & 0xff;
	stats->latency_bin_lower_bound =
	    (latency_bin_configuration >> 24) & 0xff;

	/* Sanity check num_latency_bins. */
	if (stats->num_latency_bins >
	    CHERI_COMPOSITOR_STATISTICS_NUM_LATENCY_BINS) {
		retval = ENOMEM;
		goto done;
	}

	for (i = 0; i < stats->num_latency_bins; i++) {
		stats->latency_bins[i] =
		    le32toh(bus_read_4(sc->compositor_sampler_res,
		        CHERI_COMPOSITOR_SAMPLER_LATENCY_BINS + 4 * i));
	}

	/* Read the statistics from the compositor hardware itself. */
		/* Compositor statistics. */
	if (compositor_sampler_get_statistics_page(sc,
	        STATISTICS_PAGE_PIPELINE1, &stats->pipeline_stage_cycles[0],
	        &stats->pipeline_stage_cycles[1]) != 0 ||
	    compositor_sampler_get_statistics_page(sc,
	        STATISTICS_PAGE_PIPELINE2, &stats->pipeline_stage_cycles[2],
	        &stats->pipeline_stage_cycles[3]) != 0 ||
	    compositor_sampler_get_statistics_page(sc,
	        STATISTICS_PAGE_PIPELINE3, &stats->pipeline_stage_cycles[4],
	        &stats->pipeline_stage_cycles[5]) != 0 ||
	    compositor_sampler_get_statistics_page(sc,
	        STATISTICS_PAGE_MEMORIES, &stats->num_tce_requests,
	        &stats->num_memory_requests) != 0 ||
	    compositor_sampler_get_statistics_page(sc,
	        STATISTICS_PAGE_FRAME_RATE, &stats->num_compositor_cycles,
	        &stats->num_frames) != 0) {
		/* Error. */
		retval = EIO;
		goto done;
	}

done:
	/* Unpause again. */
	if (!was_paused)
		compositor_sampler_unpause_statistics(sc);

	return retval;
}

int
cheri_compositor_reg_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	struct cheri_compositor_softc *sc;
	int error = 0;
	const struct file *cdev_fd;

	sc = dev->si_drv1;

	CHERI_COMPOSITOR_DEBUG(sc, "Received ioctl %lu", cmd);

	/* Work out which FD the ioctl() is being called on, as that determines
	 * which compositor memory pool we use. */
	cdev_fd = td->td_fpop;
	KASSERT(cdev_fd != NULL,
	    ("ioctl called on null FD from thread %p", td));

	switch (cmd) {
	case CHERI_COMPOSITOR_ALLOCATE_CFB: {
		struct cheri_compositor_allocate_cfb *params;
		struct chericap *cfb_cap;
		struct compositor_cfb_pool *cfb_pool;

		params = (struct cheri_compositor_allocate_cfb *) data;

		/* FD has to be mmap()ped before AllocateCfb can be called. */
		if (get_cfb_pool_for_cdev_fd(sc, cdev_fd, &cfb_pool) != 0) {
			return (EFAULT);
		}

		/* Check things. Width and height are checked by
		 * compositor_allocate_cfb(). */
		if (params->cfb_id_out == NULL)
			return (EINVAL);

		/* FIXME: cfb_cap doesn't seem to be properly aligned when
		 * allocated on the stack. */
		cfb_cap = malloc(sizeof(*cfb_cap), M_CHERI_COMPOSITOR,
		    M_WAITOK | M_ZERO);

		if (cfb_cap == NULL) {
			error = ENOMEM;
			goto done;
		}

		/* Send an AllocateCfb command to the hardware. */
		CHERI_COMPOSITOR_LOCK(sc);
		error =
		    compositor_allocate_cfb(sc, cfb_pool->vm_obj, td,
		        params->width, params->height, cfb_cap);
		CHERI_COMPOSITOR_UNLOCK(sc);

		if (error != 0) {
			free(cfb_cap, M_CHERI_COMPOSITOR);
			goto done;
		}

		/* Copy out the allocated CFB's capability. This would normally
		 * use copyout(), but it needs to be capability-aware. */
		error =
		    copyoutcap(cfb_cap, params->cfb_id_out,
		       sizeof(struct chericap));
		free(cfb_cap, M_CHERI_COMPOSITOR);

		if (error != 0) {
			compositor_free_cfb(sc, td, cfb_pool,
			    (struct chericap *) params->cfb_id_out);
			goto done;
		}

		break;
	}
	case CHERI_COMPOSITOR_FREE_CFB: {
		struct cheri_compositor_free_cfb *params;
		struct compositor_cfb_pool *cfb_pool;

		params = (struct cheri_compositor_free_cfb *) data;

		/* FD has to be mmap()ped before FreeCfb can be called. */
		if (get_cfb_pool_for_cdev_fd(sc, cdev_fd, &cfb_pool) != 0) {
			return (EFAULT);
		}

		/* All the parameters are validated by
		 * compositor_free_cfb(). */

		/* Send a FreeCfb command to the hardware. */
		CHERI_COMPOSITOR_LOCK(sc);
		error = compositor_free_cfb(sc, td, cfb_pool, params->cfb_id);
		CHERI_COMPOSITOR_UNLOCK(sc);

		if (error != 0)
			goto done;

		break;
	}
	case CHERI_COMPOSITOR_UPDATE_CFB: {
		struct cheri_compositor_update_cfb *params;
		struct chericap *cfb_cap;
		struct compositor_cfb_pool *cfb_pool;

		params = (struct cheri_compositor_update_cfb *) data;

		/* FD has to be mmap()ped before UpdateCfb can be called. */
		if (get_cfb_pool_for_cdev_fd(sc, cdev_fd, &cfb_pool) != 0) {
			return (EFAULT);
		}

		/* Require strict privileges. */
		if (priv_check(td, PRIV_DRIVER) != 0) {
			return (EPERM);
		}

		/* FIXME: cfb_cap doesn't seem to be properly aligned when
		 * allocated on the stack. */
		cfb_cap = malloc(sizeof(*cfb_cap), M_CHERI_COMPOSITOR,
		    M_WAITOK | M_ZERO);

		if (cfb_cap == NULL) {
			error = ENOMEM;
			goto done;
		}

		/* Copy in the CFB's capability. */
		error =
		    copyincap(params->cfb_token, cfb_cap,
		        sizeof(struct chericap));

		if (error != 0) {
			free(cfb_cap, M_CHERI_COMPOSITOR);
			goto done;
		}

		/* All the parameters are validated by
		 * compositor_update_cfb(). */

		/* Send an UpdateCfb command to the hardware. */
		CHERI_COMPOSITOR_LOCK(sc);
		error =
		    compositor_update_cfb(sc, cfb_pool, cfb_cap,
		        params->x_position, params->y_position,
		        params->z_position, params->opaque_x, params->opaque_y,
		        params->opaque_width, params->opaque_height,
		        params->update_in_progress);
		CHERI_COMPOSITOR_UNLOCK(sc);

		free(cfb_cap, M_CHERI_COMPOSITOR);

		if (error != 0)
			goto done;

		break;
	}
	case CHERI_COMPOSITOR_SWAP_CFBS: {
		struct compositor_cfb_pool *cfb_pool;

		/* Require strict privileges. */
		if (priv_check(td, PRIV_DRIVER) != 0) {
			return (EPERM);
		}

		/* FD has to be mmap()ped before SwapCfbs can be called. */
		if (get_cfb_pool_for_cdev_fd(sc, cdev_fd, &cfb_pool) != 0) {
			return (EFAULT);
		}

		CHERI_COMPOSITOR_ERROR(sc, "FIXME: ioctl() unimplemented.");
		error = ENODEV;

		break;
	}
	case CHERI_COMPOSITOR_GET_CONFIGURATION: {
		struct cheri_compositor_get_configuration *params;

		params = (struct cheri_compositor_get_configuration *) data;

		/* Require strict capabilities. */
		if (priv_check(td, PRIV_DRIVER) != 0) {
			return (EPERM);
		}

		/* Check things. */
		if (params->configuration_out == NULL)
			return (EINVAL);

		/* Copy out the configuration. */
		error =
		    copyout(&sc->configuration, params->configuration_out,
		       sizeof(struct cheri_compositor_configuration));
		if (error != 0)
			goto done;

		break;
	}
	case CHERI_COMPOSITOR_SET_CONFIGURATION: {
		struct cheri_compositor_set_configuration *params;

		params = (struct cheri_compositor_set_configuration *) data;

		/* Require strict capabilities. */
		if (priv_check(td, PRIV_DRIVER) != 0) {
			return (EPERM);
		}

		/* Send a SetConfiguration command to the hardware. */
		CHERI_COMPOSITOR_LOCK(sc);
		error =
		    compositor_set_configuration(sc, &params->configuration);
		CHERI_COMPOSITOR_UNLOCK(sc);

		if (error != 0)
			goto done;

		break;
	}
	case CHERI_COMPOSITOR_CFB_ID_TO_TOKEN: {
		struct cheri_compositor_cfb_id_to_token *params;
		struct chericap *cfb_cap_in, *cfb_cap_out;
		struct compositor_cfb_pool *cfb_pool;

		params = (struct cheri_compositor_cfb_id_to_token *) data;

		/* FD has to be mmap()ped before we can check capabilities
		 * against it. */
		if (get_cfb_pool_for_cdev_fd(sc, cdev_fd, &cfb_pool) != 0) {
			return (EFAULT);
		}

		/* Check things. */
		if (params->cfb_id == NULL || params->cfb_token == NULL)
			return (EINVAL);

		/* FIXME: cfb_cap_* doesn't seem to be properly aligned when
		 * allocated on the stack. */
		cfb_cap_in = malloc(sizeof(*cfb_cap_in), M_CHERI_COMPOSITOR,
		    M_WAITOK | M_ZERO);

		if (cfb_cap_in == NULL) {
			error = ENOMEM;
			goto done;
		}

		cfb_cap_out = malloc(sizeof(*cfb_cap_out), M_CHERI_COMPOSITOR,
		    M_WAITOK | M_ZERO);

		if (cfb_cap_out == NULL) {
			error = ENOMEM;
			goto done;
		}

		/* Copy in the CFB's capability. */
		error =
		    copyincap(params->cfb_id, cfb_cap_in,
		        sizeof(struct chericap));

		if (error != 0) {
			free(cfb_cap_in, M_CHERI_COMPOSITOR);
			free(cfb_cap_out, M_CHERI_COMPOSITOR);
			goto done;
		}

		/* Convert the capability ID to a sealed token. */
		CHERI_COMPOSITOR_LOCK(sc);
		error =
		    compositor_cfb_id_to_token(sc, cfb_pool->vm_obj, td,
		        cfb_pool, cfb_cap_in, cfb_cap_out);
		CHERI_COMPOSITOR_UNLOCK(sc);

		if (error != 0) {
			free(cfb_cap_in, M_CHERI_COMPOSITOR);
			free(cfb_cap_out, M_CHERI_COMPOSITOR);
			goto done;
		}

		/* Copy out the newly produced CFB token capability. This would
		 * normally use copyout(), but it needs to be
		 * capability-aware. */
		error =
		    copyoutcap(cfb_cap_out, params->cfb_token,
		       sizeof(struct chericap));

		free(cfb_cap_in, M_CHERI_COMPOSITOR);
		free(cfb_cap_out, M_CHERI_COMPOSITOR);

		if (error != 0) {
			goto done;
		}

		break;
	}
	case CHERI_COMPOSITOR_CFB_TOKEN_TO_ID: {
		struct cheri_compositor_cfb_token_to_id *params;
		struct chericap *cfb_cap_in, *cfb_cap_out;
		struct compositor_cfb_pool *cfb_pool;

		params = (struct cheri_compositor_cfb_token_to_id *) data;

		/* FD has to be mmap()ped before we can create capabilities for
		 * it. */
		if (get_cfb_pool_for_cdev_fd(sc, cdev_fd, &cfb_pool) != 0) {
			return (EFAULT);
		}

		/* Check things. */
		if (params->cfb_token == NULL || params->cfb_id == NULL)
			return (EINVAL);

		/* FIXME: cfb_cap_* doesn't seem to be properly aligned when
		 * allocated on the stack. */
		cfb_cap_in = malloc(sizeof(*cfb_cap_in), M_CHERI_COMPOSITOR,
		    M_WAITOK | M_ZERO);

		if (cfb_cap_in == NULL) {
			error = ENOMEM;
			goto done;
		}

		cfb_cap_out = malloc(sizeof(*cfb_cap_out), M_CHERI_COMPOSITOR,
		    M_WAITOK | M_ZERO);

		if (cfb_cap_out == NULL) {
			error = ENOMEM;
			goto done;
		}

		/* Copy in the CFB's capability. */
		error =
		    copyincap(params->cfb_token, cfb_cap_in,
		        sizeof(struct chericap));

		if (error != 0) {
			free(cfb_cap_in, M_CHERI_COMPOSITOR);
			free(cfb_cap_out, M_CHERI_COMPOSITOR);
			goto done;
		}

		/* Convert the sealed capability token to an unsealed ID. */
		CHERI_COMPOSITOR_LOCK(sc);
		error =
		    compositor_cfb_token_to_id(sc, cfb_pool->vm_obj, td,
		        cfb_pool, cfb_cap_in, cfb_cap_out);
		CHERI_COMPOSITOR_UNLOCK(sc);

		if (error != 0) {
			free(cfb_cap_in, M_CHERI_COMPOSITOR);
			free(cfb_cap_out, M_CHERI_COMPOSITOR);
			goto done;
		}

		/* Copy out the newly produced CFB ID capability. This would
		 * normally use copyout(), but it needs to be
		 * capability-aware. */
		error =
		    copyoutcap(cfb_cap_out, params->cfb_id,
		       sizeof(struct chericap));

		free(cfb_cap_in, M_CHERI_COMPOSITOR);
		free(cfb_cap_out, M_CHERI_COMPOSITOR);

		if (error != 0) {
			goto done;
		}

		break;
	}
	case CHERI_COMPOSITOR_GET_STATISTICS: {
		struct cheri_compositor_get_statistics *params;
		struct cheri_compositor_statistics stats;

		params = (struct cheri_compositor_get_statistics *) data;

		/* Require strict capabilities. */
		if (priv_check(td, PRIV_DRIVER) != 0) {
			return (EPERM);
		}

		/* Check things. */
		if (params->stats_out == NULL)
			return (EINVAL);

		/* Get statistics from the AvalonSampler. */
		CHERI_COMPOSITOR_LOCK(sc);
		error =
		    compositor_sampler_get_statistics(sc, &stats);
		CHERI_COMPOSITOR_UNLOCK(sc);

		/* Copy out the statistics. */
		error =
		    copyout(&stats, params->stats_out,
		       sizeof(struct cheri_compositor_statistics));
		if (error != 0)
			goto done;

		break;
	}
	case CHERI_COMPOSITOR_PAUSE_STATISTICS: {
		/* Require strict privileges. */
		if (priv_check(td, PRIV_DRIVER) != 0) {
			return (EPERM);
		}

		/* Pause statistical sampling. */
		CHERI_COMPOSITOR_LOCK(sc);
		error = compositor_sampler_pause_statistics(sc);
		CHERI_COMPOSITOR_UNLOCK(sc);

		break;
	}
	case CHERI_COMPOSITOR_UNPAUSE_STATISTICS: {
		/* Require strict privileges. */
		if (priv_check(td, PRIV_DRIVER) != 0) {
			return (EPERM);
		}

		/* Unpause statistical sampling. */
		CHERI_COMPOSITOR_LOCK(sc);
		error = compositor_sampler_unpause_statistics(sc);
		CHERI_COMPOSITOR_UNLOCK(sc);

		break;
	}
	case CHERI_COMPOSITOR_RESET_STATISTICS: {
		/* Require strict privileges. */
		if (priv_check(td, PRIV_DRIVER) != 0) {
			return (EPERM);
		}

		/* Reset statistical sample counters. */
		CHERI_COMPOSITOR_LOCK(sc);
		error = compositor_sampler_reset_statistics(sc);
		CHERI_COMPOSITOR_UNLOCK(sc);

		break;
	}
	default:
		error = EINVAL;
		break;
	}

done:
	return (error);
}
