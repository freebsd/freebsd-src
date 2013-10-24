/*-
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
 *
 * $FreeBSD: head/sys/dev/cheri/compositor/cheri_compositor_internal.h 245380 2013-04-28 22:18:00Z pwithnall $
 */

#include <dev/cheri/compositor/cheri_compositor.h>
#include <sys/queue.h>

#ifndef _DEV_CHERI_COMPOSITOR_INTERNAL_H_
#define	_DEV_CHERI_COMPOSITOR_INTERNAL_H_

/* Kernel-only definitions for the compositor. */
#ifdef _KERNEL

/* Set to 1 if you want extra debugging output. (This will noticeably slow down
 * various operations such as tile cache entry updates.) */
#define CHERI_COMPOSITOR_DEBUG_VERBOSE 0

/* Message output macros. */
#if defined(CHERI_COMPOSITOR_DEBUG_VERBOSE) && CHERI_COMPOSITOR_DEBUG_VERBOSE
	#define CHERI_COMPOSITOR_DEBUG(SC, M, ...) \
	    device_printf((SC)->compositor_dev, "%s: " M "\n", __func__, \
	        ##__VA_ARGS__)
#else /* if !CHERI_COMPOSITOR_DEBUG_VERBOSE */
	#define CHERI_COMPOSITOR_DEBUG(SC, M, ...)
#endif

#define CHERI_COMPOSITOR_ERROR(SC, M, ...) \
    device_printf((SC)->compositor_dev, "%s: Error: " M "\n", \
        __func__, ##__VA_ARGS__)


/**
 * Type representing an index or size, in tiles (not in bytes). Indexes of this
 * type are typically relative to a CFB pool base, rather than the base of
 * compositor memory.
 */
typedef uint64_t compositor_tile_offset_t;

/**
 * Kernel-space identifier for a CFB.
 *
 * pool: The pool containing this CFB.
 * offset: Zero-based offset of the first tile in the CFB from the base of the
 *         pool.
 * length: Positive length of the CFB in tiles.
 *
 * This is in contrast to the user-space identifier for a CFB, which is a CHERI
 * capability using the virtual translations of offset and length, and implicit
 * reference to the pool by means of its memory mapping into the process'
 * address space.
 *
 * Invariants:
 *  - offset and length must specify a region inside pool.
 *  - pool must be non-null.
 *  - length must be positive (notably, non-zero).
 */
struct compositor_cfb_id {
	const struct compositor_cfb_pool *pool;
	compositor_tile_offset_t offset;
	compositor_tile_offset_t length;
};

/**
 * Utility tuple of a device and CFB pool. This associates the given CFB pool
 * with the device owning the device node that pool has been mmap()ped from.
 *
 * dev: Compositor device structure. This must be referenced by the constructor
 *      of the cfb_vm_object.
 * pool: CFB pool allocated to that device. This must be referenced by the
 *       constructor of the cfb_vm_object.
 *
 * Invariants:
 *  - dev and pool must be non-null.
 *  - Zero or more cfb_vm_objects may exist which reference the same dev.
 *  - At most one cfb_vm_object may exist which references the same pool (though
 *    duplicate dev–pool pairs are permitted).
 */
struct cfb_vm_object {
	struct cdev *dev;
	struct compositor_cfb_pool *pool;
};

/**
 * CFB allocation pool. The entirety of compositor memory is split into
 * fixed-size pools from which CFBs are allocated. Pools are mapped to user
 * processes by FD: each time a user process calls mmap() on a given FD, they
 * get a mapping to the same CFB pool. Calling mmap() on a different FD (e.g. by
 * open()ing the compositor device node again) results in mapping a different
 * CFB pool.
 *
 * Allocation state for CFBs is held on a per-pool basis; allocations of CFBs
 * from one pool are completely independent of those from another.
 *
 * ref_count: As the same FD may be mmap()ped several times (potentially by
 *            several processes), pools are reference counted, and only
 *            destroyed when the reference count reaches 0.
 * mapped_fd: A pointer to the FD associated with this pool, for lookup
 *            purposes. No reference is held, as the compositor_cfb_pool is
 *            destroyed when the FD is destroyed.
 * vm_obj: A pointer to the VM object associated with this pool (i.e. the pool's
 *         region of compositor memory). This must be referenced by the
 *         constructor of the compositor_cfb_pool.
 * next_free_tile: Documented below.
 *
 * Invariants:
 *  - ref_count is always positive (specifically, greater than 0).
 *  - mapped_fd and vm_obj are always non-null.
 *  - At most one compositor_cfb_pool may exist which references the same
 *    mapped_fd.
 *  - At most one compositor_cfb_pool may exist which references the same
 *    vm_obj.
 */
struct compositor_cfb_pool {
	unsigned int ref_count;

	const struct file *mapped_fd;
	struct vm_object *vm_obj;

	/*
	 * Allocator state for handing out compositor memory. Currently this is
	 * a simple counter allocator.
	 *
	 * This offset always points to the first unallocated tile. It is in
	 * units of tiles, as an offset from the base of the pool.
	 */
	compositor_tile_offset_t next_free_tile;
};

/**
 * Client frame buffer (CFB). This represents a single pixel buffer as exposed
 * to the hardware. Each CFB may have one rectangular ‘opaque region’ which is
 * used to optimise rendering by specifying a region of the CFB which contains
 * only fully opaque pixels. If this region covers pixels which are not fully
 * opaque, rendering artifacts may result. If no opaque region is required, set
 * opaque_width and opaque_height to 0.
 *
 * z_ordering: Linked list across all CFBs, giving the Z ordering of the CFBs.
 *             The top-most CFB is at the head of the list. CFBs remain in the
 *             z_ordering even if their update_in_progress field is set.
 * x: X coordinate of the top-left corner of the CFB, in pixels relative to the
 *    top-left of the screen.
 * y: Y coordinate of the top-left corner of the CFB, in pixels relative to the
 *    top-left of the screen.
 * z: Z coodinate of the CFB, with increasing Z bringing CFBs closer to the
 *    user (i.e. highest Z coordinate is top-most during compositing).
 * allocated_tiles_base: Address of the first tile in the CFB, in tiles relative
 *                       to the base of compositor memory. (Note: not an offset
 *                       from a compositor pool.)
 * width: Width of the CFB, in tiles.
 * height: Height of the CFB, in tiles.
 * update_in_progress: True if the CFB's pixel data is being written to, and
 *                     hence it shouldn't be drawn to the screen. This allows
 *                     for double-buffering to be implemented in software.
 * opaque_x: X coordinate of the top-left corner of the opaque region of the
 *           CFB, in pixels relative to the top-left of the CFB.
 * opaque_y: Y coordinate of the top-left corner of the opaque region of the
 *           CFB, in pixels relative to the top-left of the CFB.
 * opaque_width: Width of the opaque region of the CFB, in pixels. If the CFB
 *               has no opaque region, this must be 0.
 * opaque_height: Height of the opaque region of the CFB, in pixels. If the CFB
 *                has no opaque region, this must be 0.
 *
 * Invariants:
 *  - x is in (0, max_screen_width].
 *  - y is in (0, max_screen_height].
 *  - z is in (0, max_layers].
 *  - allocated_tiles_base is in (0, compositor_mem_length / tile_size_bytes].
 *  - width is in (1, ceil(max_screen_width / tile_width)).
 *  - height is in (1, ceil(max_screen_height / tile_height)).
 *  - width * tile_width is greater than or equal to the CFB's width in pixels.
 *  - height * tile_height is greater than or equal to the CFB's height in
 *    pixels.
 *  - opaque_x is in [0, width * tile_width).
 *  - opaque_y is in [0, height * tile_height).
 *  - opaque_width is in [0, width * tile_width].
 *  - opaque_height is in [0, height * tile_height].
 */
struct compositor_cfb {
	LIST_ENTRY(compositor_cfb) z_ordering;

	unsigned int x;
	unsigned int y;
	unsigned int z;
	compositor_tile_offset_t allocated_tiles_base;
	unsigned int width;
	unsigned int height;
	boolean_t update_in_progress;
	uint32_t opaque_x; /* in pixels */
	uint32_t opaque_y; /* in pixels */
	uint32_t opaque_width; /* in pixels */
	uint32_t opaque_height; /* in pixels */
};

/**
 * Main device structure for the compositor.
 *
 * Locking is coarse-grained, requiring compositor_lock for any access to locked
 * fields (marked with an (L) in comments below).
 */
struct cheri_compositor_softc {
	/*
	 * Bus-related fields. Read-only once set.
	 */
	device_t	 compositor_dev;
	int		 compositor_unit;

	/*
	 * The compositor driver doesn't require a lot of synchronisation;
	 * however, the lock is used to protect read-modify-write operations on
	 * registers, and the memory management state.
	 */
	struct mtx	 compositor_lock;

	/*
	 * Control register device. This is only accessed by the kernel, which
	 * writes to the device's command buffer. It is not mappable by user
	 * space.
	 */
	struct resource	*compositor_reg_res; /* (L) */
	int		 compositor_reg_rid; /* (L) */

	/*
	 * Avalon sampler register device. This is only accessed by the kernel,
	 * which reads Avalon bus usage samples from it for performance
	 * profiling. It is not mappable by user space.
	 */
	struct resource	*compositor_sampler_res; /* (L) */
	int		 compositor_sampler_rid; /* (L) */

	/*
	 * Client frame buffer memory region. This is indirectly mappable from
	 * user space in the form of fixed CFB pools, each mappable separately.
	 */
	struct cdev	*compositor_cfb_cdev; /* (L) */
	struct resource	*compositor_cfb_res; /* (L) */
	int		 compositor_cfb_rid; /* (L) */

	/*
	 * In-memory CFB store. This can be removed once it's implemented
	 * in hardware. This must be kept sorted by Z-index (with the top-most
	 * layers at the head).
	 *
	 * This contains all CFBs (regardless of whether they're marked as
	 * update_in_progress).
	 */
	LIST_HEAD(, compositor_cfb) cfbs; /* (L) */

	/*
	 * Current compositor configuration (such as resolution). This should
	 * always be an up-to-date copy of the configuration in hardware.
	 */
	struct cheri_compositor_configuration configuration; /* (L) */

	/*
	 * Current command sequence number for communication with the hardware.
	 */
	uint8_t seq_num; /* (L) */

	/*
	 * Array of allocated compositor CFB pool mappings. The indices
	 * correspond to the pool indices/offsets. Entries with either their fd
	 * or vm_mapping equal to NULL correspond to pools which are currently
	 * unallocated.
	 */
	struct compositor_cfb_pool mem_pools[CHERI_COMPOSITOR_MEM_NUM_POOLS];

	/*
	 * Whether statistical sampling of compositor memory traffic is
	 * currently paused. This always reflects the hardware state.
	 */
	boolean_t is_sampler_paused; /* (L) */
};


/* Various address conversion and validation utilities. */

/**
 * Convert an offset or address from units of tiles to units of bytes.
 */
static inline size_t
tile_offset_to_byte_offset(compositor_tile_offset_t tile)
{
	return tile * (TILE_SIZE * TILE_SIZE * PIXEL_SIZE);
}

/**
 * Convert an offset or address from units of bytes to units of tiles.
 *
 * Note that this will truncate to the next lowest multiple of TILE_SIZE.
 */
static inline compositor_tile_offset_t
byte_offset_to_tile_offset(size_t byte)
{
	return byte / (TILE_SIZE * TILE_SIZE * PIXEL_SIZE);
}

/* Convert a pointer to a CFB pool (which must be an entry in sc->mem_pools) to
 * a byte index relative to the base of compositor memory. This performs no
 * validation. */
static inline size_t
compositor_cfb_pool_to_byte_offset(struct cheri_compositor_softc *sc,
    const struct compositor_cfb_pool *cfb_pool)
{
	return ((((uintptr_t) cfb_pool - (uintptr_t) sc->mem_pools) /
	    sizeof(*sc->mem_pools)) *
	    CHERI_COMPOSITOR_MEM_POOL_LENGTH);
}

/* Convert a pointer to a CFB pool (which must be an entry in sc->mem_pools) to
 * a tile index relative to the base of compositor memory. This performs no
 * validation. */
static inline compositor_tile_offset_t
compositor_cfb_pool_to_tile_offset(struct cheri_compositor_softc *sc,
    const struct compositor_cfb_pool *cfb_pool)
{
	return byte_offset_to_tile_offset(
	    compositor_cfb_pool_to_byte_offset(sc, cfb_pool));
}

/**
 * Check whether a CFB pool is valid. This validates all the pool's fields, but
 * does not perform any validation on external factors (such as whether the
 * current thread should be allowed access to the CFB).
 */
static inline boolean_t
cfb_pool_is_valid(struct cheri_compositor_softc *sc,
    const struct compositor_cfb_pool *pool)
{
	return (
	    sc != NULL &&
	    pool != NULL &&
	    pool->mapped_fd != NULL &&
	    pool->vm_obj != NULL &&
	    pool->next_free_tile <= CHERI_COMPOSITOR_MEM_POOL_LENGTH_TILES &&
	    (uintptr_t) pool >= (uintptr_t) sc->mem_pools &&
	    (uintptr_t) pool - (uintptr_t) sc->mem_pools <
	        sizeof(sc->mem_pools));
}

/**
 * Check whether an internal kernel CFB ID (not a capability) is valid. This
 * validates all the ID's fields, but does not perform any validation on
 * external factors (such as whether the current thread should be allowed access
 * to the CFB).
 */
static inline boolean_t
cfb_id_is_valid(struct cheri_compositor_softc *sc,
    const struct compositor_cfb_id *cfb_id)
{
	return (
	    sc != NULL &&
	    cfb_id != NULL &&
	    cfb_pool_is_valid(sc, cfb_id->pool) &&
	    cfb_id->offset < CHERI_COMPOSITOR_MEM_POOL_LENGTH_TILES &&
	    cfb_id->length > 0 &&
	    cfb_id->length <= CHERI_COMPOSITOR_MEM_POOL_LENGTH_TILES &&
	    cfb_id->offset + cfb_id->length <=
	        CHERI_COMPOSITOR_MEM_POOL_LENGTH_TILES);
}

/**
 * Check whether the given offset (in bytes, relative to the base of compositor
 * memory) lies within compositor memory.
 */
static inline boolean_t
tile_offset_is_in_compositor_mem(struct cheri_compositor_softc *sc,
    uint64_t offset)
{
	return (offset <
	    byte_offset_to_tile_offset(rman_get_size(sc->compositor_cfb_res)));
}

/**
 * Assert that the given CFB ID is valid (by calling cfb_id_is_valid()). If the
 * ID is not valid, this will cause a kernel panic with a helpful error message.
 * This is designed to be used to enforce security properties.
 */
#define KASSERT_CFB_ID_IS_VALID(SC, ID) \
	KASSERT(cfb_id_is_valid(SC, ID), \
	    ("invalid CFB ID %p: ([%p, %p), [%lu, %lu)", (ID), \
	     (void *) (rman_get_start((SC)->compositor_cfb_res) + \
	         compositor_cfb_pool_to_byte_offset(SC, (ID)->pool)), \
	     (void *) ((rman_get_start((SC)->compositor_cfb_res) + \
	         compositor_cfb_pool_to_byte_offset(SC, (ID)->pool) + \
	         CHERI_COMPOSITOR_MEM_POOL_LENGTH)), \
	     (ID)->offset, (ID)->offset + (ID)->length));


/* Locking macros. */
#define CHERI_COMPOSITOR_LOCK(sc) \
	mtx_lock(&(sc)->compositor_lock)
#define CHERI_COMPOSITOR_LOCK_ASSERT(sc) \
	mtx_assert(&(sc)->compositor_lock, MA_OWNED)
#define CHERI_COMPOSITOR_LOCK_DESTROY(sc) \
	mtx_destroy(&(sc)->compositor_lock)
#define CHERI_COMPOSITOR_LOCK_INIT(sc) \
	mtx_init(&(sc)->compositor_lock, "cheri_compositor", NULL, MTX_DEF)
#define CHERI_COMPOSITOR_UNLOCK(sc) \
	mtx_unlock(&(sc)->compositor_lock)
#define CHERI_COMPOSITOR_ASSERT_LOCKED(sc) \
	mtx_assert_(&(sc)->compositor_lock, MA_OWNED, __FILE__, __LINE__)


/*
 * Driver setup routines from the bus attachment/teardown.
 */
int	cheri_compositor_attach(struct cheri_compositor_softc *sc);
void	cheri_compositor_detach(struct cheri_compositor_softc *sc);

extern devclass_t	cheri_compositor_devclass;

/*
 * Sub-driver setup routines.
 */
int	cheri_compositor_cfb_attach(struct cheri_compositor_softc *sc);
void	cheri_compositor_cfb_detach(struct cheri_compositor_softc *sc);

/*
 * Internal utility functions.
 */
d_ioctl_t	cheri_compositor_reg_ioctl;
int		dup_or_allocate_cfb_pool_for_cdev_fd(
		    struct cheri_compositor_softc *sc,
		    const struct file *cdev_fd, struct vm_object *vm_obj,
		    struct compositor_cfb_pool **cfb_pool_out);
void		unref_cfb_pool(struct cheri_compositor_softc *sc,
		    struct compositor_cfb_pool *cfb_pool);

#endif /* _KERNEL */

#endif /* _DEV_CHERI_COMPOSITOR_INTERNAL_H_ */
