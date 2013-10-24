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
 * $FreeBSD: head/sys/dev/cheri/compositor/cheri_compositor.h 245380 2013-05-02 00:09:00Z pwithnall $
 */

#ifndef _DEV_CHERI_COMPOSITOR_H_
#define	_DEV_CHERI_COMPOSITOR_H_

/**
 * Driver for the CHERI compositor device, which is a simple 2D graphics
 * compositor. It maintains a set of pixel buffers (‘client frame buffers’ or
 * CFBs), each of which has an associated (X, Y) position and Z stacking order;
 * and the hardware alpha composites them using the ‘over’ operator to form an
 * output frame buffer.
 *
 * Much of the functionality of this driver is dedicated to managing CFB memory,
 * and the security thereof. CFB memory is exposed as a series of ‘pools’ which
 * are mapped into user process’ VM spaces by mmap()ping the compositor’s device
 * node. Each unique FD for the device node gives access to a unique (and
 * deterministic) CFB pool; hence processes can share access to a CFB pool by
 * explicitly sharing its FD.
 *
 * Each CFB is identified by a capability (‘CFB ID’). This ID is only valid with
 * respect to a given process VM space, as capabilities are virtually addressed.
 * The capability gives the base address of the CFB in the process’ address
 * space, the CFB’s length, and its access permissions. Processes don’t have to
 * access CFB pixel data through this capability, as typically the entire CFB
 * pool will be accessible under the process’ ambient authority. However,
 * sandboxed processes or those sharing a CFB pool from another process will
 * need to access pixel data through the CFB ID.
 *
 * In order to share CFBs with other processes, and to perform metadata
 * operations on them, the driver has a pair of ioctl()s to translate CFB IDs
 * into ‘CFB tokens’, which are sealed capabilities. A CFB token is sealed so
 * that only this driver can unseal it (or seal it up in the first place), and
 * refers to the CFB by its physical address, so is independent of any process’
 * virtual address space.
 *
 * The pixel data for a CFB is 32-bit RGBA format, with 8 bits per component.
 * It’s stored linearly, with a row stride which is a multiple of 8 pixels to
 * allow for aligned memory accesses. Each group of 8 contiguous pixels is
 * called a ‘slice’.
 *
 * The metadata for a CFB can specify an ‘opaque region’, which is used as an
 * optimisation by the driver to reduce the number of hardware tile cache
 * updates necessary by assuming it is entirely opaque, and thus obscures any
 * CFBs below it in the Z ordering. Use of the opaque region is not necessary
 * for correct operation, but incorrectly specifying an opaque region may result
 * in rendering artefacts.
 *
 * Each CFB also has a ‘update in progress’ metadata bit, which specifies
 * whether the CFB should be drawn. When set, the CFB is not drawn by the
 * compositor hardware; this may be used to implement double buffering.
 *
 * Remaining FIXMEs:
 *  • Ensure the ioctl()s can be used from 32- or 64-bit user space.
 *  • Implement cleverer checks to avoid recalculating the tile cache.
 *  • Implement the tile cache calculations in hardware and remove them from the
 *    driver.
 *  • Implement the cheri_compositor_swap_cfbs ioctl().
 *  • Re-add is_opaque and height to TILE_CACHE_ENTRY hardware packets.
 *  • Tidy up endianness when communicating with hardware.
 *  • Use interrupts in communications with hardware, rather than polled I/O.
 *  • Implement sensible memory management within CFB pools.
 */


#ifndef _KERNEL
#include <stdint.h>
#include <sys/ioccom.h>
#include <sys/types.h>

#ifndef boolean_t
typedef int boolean_t;
#endif
#endif /* !_KERNEL */


/* Configuration defines. Many of these are hardware limitations, so can't be
 * changed without updating the Bluespec. */
#define MAX_CFBS 16
#define MAX_X_RES 2560 /* pixels */
#define MAX_Y_RES 1600 /* pixels */
#define MAX_Z (MAX_TILES / 2) /* layers */
#define DEFAULT_X_RES 800 /* pixels */
#define DEFAULT_Y_RES 480 /* pixels */
#define TILE_SIZE 32 /* pixels on one side */
#define SLICE_SIZE 8 /* pixels */
#define PIXEL_SIZE 4 /* bytes per pixel */
#define SEQ_NUM_MAX 16
#define MAX_LAYERS 7
#define TILES_PER_LAYER ((MAX_X_RES * MAX_Y_RES) / (TILE_SIZE * TILE_SIZE))
#define MAX_TILES (TILES_PER_LAYER * MAX_LAYERS)


/**
 * Configuration for the compositor hardware. This may be changed at runtime by
 * calling compositor_set_configuration().
 *
 * x_resolution: Output X resolution, in pixels.
 * y_resolution: Output Y resolution, in pixels.
 */
struct cheri_compositor_configuration {
	unsigned int x_resolution;
	unsigned int y_resolution;
};


/* FIXME: Can't this be grabbed from FDT or something? */
#define	CHERI_COMPOSITOR_MEM_LENGTH	0x10000000

/**
 * Size of a CFB pool, in bytes. This is fixed, and the same for all CFB pools.
 * It's decided by the driver, and is not subject to hardware limitations other
 * than the size of compositor memory.
 *
 * This must be a multiple of PAGE_SIZE, and must be at most 0x10000000, which
 * is the upper size limit (in bytes) for an mmap() in FreeBSD.
 *
 * The present value was chosen arbitrarily as 2560×1600 pixels (4 bytes each)
 * plus a little extra.
 */
#define CHERI_COMPOSITOR_MEM_POOL_LENGTH 0x1000000
#define CHERI_COMPOSITOR_MEM_NUM_POOLS \
    (CHERI_COMPOSITOR_MEM_LENGTH / CHERI_COMPOSITOR_MEM_POOL_LENGTH)
#define CHERI_COMPOSITOR_MEM_POOL_LENGTH_TILES \
    (byte_offset_to_tile_offset(CHERI_COMPOSITOR_MEM_POOL_LENGTH))


/**
 * CFB ID capability. This is an unsealed, virtually addressed capability which
 * gives the extents of a CFB in a given user space process' virtual address
 * space. It may only be used within that process (and in communications between
 * that process and the kernel). It must not be shared with other processes.
 */
typedef struct chericap *cheri_compositor_cfb_id;

/**
 * CFB token capability. This is a sealed, physically addressed data capability
 * which is used to prove to the kernel that a process has delegate authority
 * over the given CFB whose physical extents are given as the capability's base
 * and length. This is used between processes, and when one process delegates to
 * another to update a CFB. For example, if a client program wants a compositor
 * process to update a CFB for it without granting the compositor access to the
 * CFB's pixel data.
 */
typedef struct chericap *cheri_compositor_cfb_token;


/* Structures for ioctl()s. Documentation for the ioctl()s themselves is given
 * with the documentation for the relevant struct. */

/**
 * Allocate a new CFB. The width and height, in pixels, are taken as input. Both
 * must be multiples of TILE_SIZE (to ensure a stride which is divisible by
 * TILE_SIZE). The width and height must be positive (specifically, non-zero)
 * and must be at most MAX_X_RES or MAX_Y_RES.
 *
 * The compositor device node's file descriptor must have been mmap()ped before
 * this may be called.
 *
 * On success, a CFB ID capability (of type cheri_compositor_cfb_id) is returned
 * in cfb_id_out.
 *
 * On failure, cfb_id_out is undefined, and one of the following error codes is
 * returned:
 *  - EFAULT: If the compositor device node's FD hasn't been mmap()ped.
 *  - EINVAL: If cfb_id_out is NULL.
 *  - ENOMEM: If the kernel or compositor ran out of memory.
 *  - EDOM: If the width or height are zero, larger than the maximum X or Y
 *          resolution, or not a multiple of TILE_SIZE.
 *  - EIO: If there was an error communicating with the hardware.
 */
struct cheri_compositor_allocate_cfb {
	uint32_t width;
	uint32_t height;
	caddr_t cfb_id_out; /* becomes an iov[] inside the kernel */
};

/**
 * Free a CFB. The CFB's ID capability is taken as input, uniquely identifying
 * the CFB.
 *
 * The compositor device node's file descriptor must have been mmap()ped before
 * this may be called.
 *
 * On failure, one of the following error codes is returned:
 *  - EFAULT: If the compositor device node's FD hasn't been mmap()ped.
 *  - EINVAL: If the given CFB ID isn't valid.
 *  - EIO: If there was an error communicating with the hardware.
 */
struct cheri_compositor_free_cfb {
	cheri_compositor_cfb_id cfb_id;
};

/**
 * Update a CFB's metadata. The CFB's token capability (not its ID capability)
 * and new metadata are taken as input.
 *
 * The compositor device node's file descriptor must have been mmap()ped before
 * this may be called.
 *
 * The calling process must have (at least) PRIV_DRIVER privileges, as the
 * ability to modify the metadata of a CFB is security critical, and normal user
 * space programs should not ordinarily be permitted to do so, even for their
 * own CFBs (or they could hijack the entire screen, for example).
 *
 * The X and Y coordinates set the position of the top-left corner of the CFB,
 * relative to the top-left corner of the screen (with axes increasing
 * left-to-right and top-to-bottom). They are given in pixels.
 *
 * The Z coordinate sets the stacking order of the CFB, relative to other CFBs,
 * with a higher Z coordinate meaning the CFB is closer to the screen.
 *
 * The opaque X and Y coordinates set the top-left corner of the CFB's opaque
 * region, relative to the top-left corner of the CFB. The opaque width and
 * height set the size of this region, which may be 0. The opaque region is used
 * to optimise rendering, hinting to the compositor that the pixel data in that
 * region is entirely opaque. Setting the opaque region to cover pixel data
 * which is not opaque may result in rendering artifacts.
 *
 * The update_in_progress field specifies whether the CFB is to be displayed. If
 * true, the CFB is considered to be a back buffer, and is not rendered. If
 * false, the CFB is rendered.
 *
 * On failure, one of the following error codes is returned:
 *  - EFAULT: If the compositor device node's FD hasn't been mmap()ped.
 *  - EPERM: If the calling process doesn't have PRIV_DRIVER privileges.
 *  - ENOMEM: If the kernel ran out of memory.
 *  - EINVAL: If the given CFB token isn't valid.
 *  - EDOM: If the X coordinate is greater than or equal to the maximum X
 *          resolution, the Y coordinate is great than or equal to the maximum Y
 *          resolution, or the Z coordinate is greater than the maximum.
 *  - EIO: If there was an error communicating with the hardware.
 */
struct cheri_compositor_update_cfb {
	cheri_compositor_cfb_token cfb_token;
	uint32_t x_position; /* in pixels */
	uint32_t y_position; /* in pixels */
	uint32_t z_position; /* in pixels */
	uint32_t opaque_x; /* in pixels */
	uint32_t opaque_y; /* in pixels */
	uint32_t opaque_width; /* in pixels */
	uint32_t opaque_height; /* in pixels */
	boolean_t update_in_progress;
};

/**
 * Invert the update_in_progress fields for the specified CFBs. This is a
 * reduced version of the functionality offered by cheri_compositor_update_cfb,
 * intended for swapping CFBs between back and front buffers when doing double
 * buffering.
 *
 * Note: This ioctl() currently isn't implemented, so will always return ENODEV.
 */
struct cheri_compositor_swap_cfbs {
	/* FIXME: This could take a single array of CFBs and just invert the
	 * update_in_progress bits for them to reduce ioctl() calls. */
	cheri_compositor_cfb_token cfb_to_update;
	cheri_compositor_cfb_token cfb_updated;
};

/**
 * Get the current configuration of the compositor hardware. This is its mutable
 * state, such as the current output resolution. Immutable state is fetched
 * using an as-yet unimplemented ioctl() (FIXME).
 *
 * The configuration will be returned in the address specified by
 * configuration_out, which must be at least as big as struct
 * cheri_compositor_configuration.
 *
 * The calling process must have (at least) PRIV_DRIVER privileges, as hardware
 * state may be sensitive, and normal user space programs should not ordinarily
 * be permitted to know it.
 *
 * On failure, one of the following error codes is returned:
 *  - EPERM: If the calling process doesn't have PRIV_DRIVER privileges.
 *  - EINVAL: If the given configuration return address isn't valid.
 *  - EIO: If there was an error communicating with the hardware.
 */
struct cheri_compositor_get_configuration {
	caddr_t /* cheri_compositor_configuration */ configuration_out;
		/* becomes an iov[] inside the kernel */
};

/**
 * Set the current configuration of the compositor hardware. This is its mutable
 * state, such as the current output resolution. Changes will take effect at the
 * next suitable point (such as the next vertical blank period), but this is not
 * guaranteed.
 *
 * The calling process must have (at least) PRIV_DRIVER privileges, as hardware
 * state may be sensitive, and normal user space programs should not ordinarily
 * be permitted to set it.
 *
 * On failure, one of the following error codes is returned:
 *  - EPERM: If the calling process doesn't have PRIV_DRIVER privileges.
 *  - EINVAL: If one of the X and Y resolution is zero but the other is not.
 *  - EDOM: If the X or Y resolution are greater than the maximum X or Y
 *          resolution.
 *  - EIO: If there was an error communicating with the hardware.
 */
struct cheri_compositor_set_configuration {
	struct cheri_compositor_configuration configuration;
};

/**
 * Convert a CFB ID capability to a CFB token capability. The former is unsealed
 * and virtually addressed; the latter is sealed and physically addressed. This
 * is intended to be used when passing CFB IDs from one process to another, when
 * coupled with cheri_compositor_cfb_token_to_id. It can also be used to create
 * CFB tokens to pass to privileged processes which may use them to call
 * cheri_compositor_update_cfb.
 *
 * The compositor device node's file descriptor must have been mmap()ped before
 * this may be called.
 *
 * The CFB ID is inputted as cfb_id, and the resulting token is outputted as
 * cfb_token on success.
 *
 * On failure, cfb_token is undefined, and one of the following error codes is
 * returned:
 *  - EFAULT: If the compositor device node's FD hasn't been mmap()ped.
 *  - EINVAL: If cfb_id or cfb_token is NULL.
 *  - ENOMEM: If the kernel ran out of memory.
 *  - EINVAL: If the given CFB ID isn't valid.
 */
struct cheri_compositor_cfb_id_to_token {
	/* FIXME: This could take arrays of tuples to reduce ioctl() calls. */
	cheri_compositor_cfb_id cfb_id;
	caddr_t /* cheri_compositor_cfb_token */ cfb_token;
};

/**
 * Convert a CFB token capability to a CFB ID capability. The former is sealed
 * and physically addressed; the latter is unsealed and virtually addressed.
 * This is intended to be used when passing CFB IDs from one process to another,
 * when coupled with cheri_compositor_cfb_id_to_token.
 *
 * The compositor device node's file descriptor must have been mmap()ped before
 * this may be called.
 *
 * The CFB token is inputted as cfb_token, and the resulting ID is outputted as
 * cfb_id on success.
 *
 * On failure, cfb_id is undefined, and one of the following error codes is
 * returned:
 *  - EFAULT: If the compositor device node's FD hasn't been mmap()ped.
 *  - EINVAL: If cfb_token or cfb_id is NULL.
 *  - ENOMEM: If the kernel ran out of memory.
 *  - EINVAL: If the given CFB token isn't valid.
 */
struct cheri_compositor_cfb_token_to_id {
	/* FIXME: This could take arrays of tuples to reduce ioctl() calls. */
	cheri_compositor_cfb_token cfb_token;
	caddr_t /* cheri_compositor_cfb_id */ cfb_id;
};

/**
 * Performance statistics for the compositor hardware and memory bus. This may
 * be populated at runtime with the latest sample values by calling
 * compositor_get_statistics(). All sample values are counted since the last
 * statistics reset (when CHERI_COMPOSITOR_RESET_STATISTICS was called).
 *
 * num_read_requests: Number of memory read requests sent by the compositor.
 * num_write_requests: Number of memory write requests sent by the compositor.
 * num_read_bursts: Number of memory read requests with a burst length greater
 *                  than 1 sent by the compositor.
 * num_write_bursts: Number of memory write requests with a burst length greater
 *                   than 1 sent by the compositor.
 * num_latency_bins: Number of latency histogram bins which are valid in
 *                   latency_bins.
 * latency_bin_lower_bound: Inclusive lower bound of latency represented by the
 *                          lowest index in latency_bins, in cycles.
 * latency_bin_upper_bound: Exclusive upper bound of latency represented by the
 *                          highest index in latency_bins, in cycles.
 * latency_bins: Array of latency bins, each containing the number of memory
 *               read request–response pairs received by the compositor with the
 *               given latency, in cycles.
 * num_compositor_cycles: Number of clock cycles the compositor has run for.
 * pipeline_stage_cycles: Array of counters for the number of packets sent
 *                        between each pair of adjacent pipeline stages in the
 *                        compositor. The 0th entry gives the number of packets
 *                        received at the head of the pipeline, and the final
 *                        entry gives the number of packets sent between the
 *                        penultimate and ultimate pipeline stages.
 * num_tce_requests: Number of requests sent by the compositor to a tile cache.
 * num_memory_requests: Number of requests sent by the compositor to external
 *                      memory.
 * num_frames: Number of whole frames rendered by the compositor.
 */
struct cheri_compositor_statistics {
	/* Memory bus statistics. */
	uint32_t num_read_requests;
	uint32_t num_write_requests;
	uint32_t num_read_bursts;
	uint32_t num_write_bursts;

	unsigned int num_latency_bins;
	uint8_t latency_bin_lower_bound;
	uint8_t latency_bin_upper_bound;

#define CHERI_COMPOSITOR_STATISTICS_NUM_LATENCY_BINS 16
	uint32_t latency_bins[CHERI_COMPOSITOR_STATISTICS_NUM_LATENCY_BINS];

	/* Compositor statistics. */
	uint32_t num_compositor_cycles;
#define CHERI_COMPOSITOR_STATISTICS_NUM_PIPELINE_STAGES 5
	uint32_t pipeline_stage_cycles
		[CHERI_COMPOSITOR_STATISTICS_NUM_PIPELINE_STAGES + 1];
	uint32_t num_tce_requests;
	uint32_t num_memory_requests;
	uint32_t num_frames;
};

/**
 * Get the current values of performance sampling counters in the compositor
 * hardware. These values will only be fetched atomically with respect to each
 * other if performance sampling is currently paused (using
 * CHERI_COMPOSITOR_PAUSE_STATISTICS). The counter values are unchanged by
 * CHERI_COMPOSITOR_GET_STATISTICS; use CHERI_COMPOSITOR_RESET_STATISTICS to
 * reset them to 0.
 *
 * The statistics will be returned in the address specified by stats_out, which
 * must be at least as big as struct cheri_compositor_statistics.
 *
 * The calling process must have (at least) PRIV_DRIVER privileges, as hardware
 * statistics may be sensitive, and normal user space programs should not
 * ordinarily be permitted to know them.
 *
 * On failure, one of the following error codes is returned:
 *  - EPERM: If the calling process doesn't have PRIV_DRIVER privileges.
 *  - EINVAL: If the given statistics return address isn't valid.
 *  - EIO: If there was an error communicating with the hardware.
 *  - ENOMEM: If there weren't enough entries in stats_out’s latency_bins field
 *            to accommodate all the latency bins exposed by hardware, as
 *            returned in stats_out’s num_latency_bins field (which is valid if
 *            ENOMEM is returned).
 */
struct cheri_compositor_get_statistics {
	caddr_t /* struct cheri_compositor_statistics */ stats_out;
		/* becomes an iov[] inside the kernel */
};


/* ioctl() definitions. */
#define CHERI_COMPOSITOR_ALLOCATE_CFB \
	_IOWR(0, 0x01, struct cheri_compositor_allocate_cfb)
#define CHERI_COMPOSITOR_FREE_CFB \
	_IOW(0, 0x02, struct cheri_compositor_free_cfb)
#define CHERI_COMPOSITOR_UPDATE_CFB \
	_IOW(0, 0x03, struct cheri_compositor_update_cfb)
#define CHERI_COMPOSITOR_SWAP_CFBS \
	_IOW(0, 0x04, struct cheri_compositor_swap_cfbs)
#define CHERI_COMPOSITOR_GET_CONFIGURATION \
	_IOR(0, 0x05, struct cheri_compositor_get_configuration)
#define CHERI_COMPOSITOR_SET_CONFIGURATION \
	_IOW(0, 0x06, struct cheri_compositor_set_configuration)
#define CHERI_COMPOSITOR_CFB_ID_TO_TOKEN \
	_IOWR(0, 0x07, struct cheri_compositor_cfb_id_to_token)
#define CHERI_COMPOSITOR_CFB_TOKEN_TO_ID \
	_IOWR(0, 0x08, struct cheri_compositor_cfb_token_to_id)
#define CHERI_COMPOSITOR_GET_STATISTICS \
	_IOWR(0, 0x09, struct cheri_compositor_get_statistics)
#define CHERI_COMPOSITOR_PAUSE_STATISTICS \
	_IO(0, 0x0A)
#define CHERI_COMPOSITOR_UNPAUSE_STATISTICS \
	_IO(0, 0x0B)
#define CHERI_COMPOSITOR_RESET_STATISTICS \
	_IO(0, 0x0C)

#endif /* _DEV_CHERI_COMPOSITOR_H_ */
