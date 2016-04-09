/*-
 * Copyright (C) 2012 Intel Corporation
 * All rights reserved.
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

__FBSDID("$FreeBSD$");

#ifndef __IOAT_H__
#define __IOAT_H__

#include <sys/param.h>
#include <machine/bus.h>

/*
 * This file defines the public interface to the IOAT driver.
 */

/*
 * Enables an interrupt for this operation. Typically, you would only enable
 * this on the last operation in a group
 */
#define	DMA_INT_EN	0x1
/*
 * Like M_NOWAIT.  Operations will return NULL if they cannot allocate a
 * descriptor without blocking.
 */
#define	DMA_NO_WAIT	0x2
/*
 * Disallow prefetching the source of the following operation.  Ordinarily, DMA
 * operations can be pipelined on some hardware.  E.g., operation 2's source
 * may be prefetched before operation 1 completes.
 */
#define	DMA_FENCE	0x4
#define	DMA_ALL_FLAGS	(DMA_INT_EN | DMA_NO_WAIT | DMA_FENCE)

/*
 * Hardware revision number.  Different hardware revisions support different
 * features.  For example, 3.2 cannot read from MMIO space, while 3.3 can.
 */
#define	IOAT_VER_3_0			0x30
#define	IOAT_VER_3_2			0x32
#define	IOAT_VER_3_3			0x33

typedef void *bus_dmaengine_t;
struct bus_dmadesc;
typedef void (*bus_dmaengine_callback_t)(void *arg, int error);

/*
 * Called first to acquire a reference to the DMA channel
 *
 * Flags may be M_WAITOK or M_NOWAIT.
 */
bus_dmaengine_t ioat_get_dmaengine(uint32_t channel_index, int flags);

/* Release the DMA channel */
void ioat_put_dmaengine(bus_dmaengine_t dmaengine);

/* Check the DMA engine's HW version */
int ioat_get_hwversion(bus_dmaengine_t dmaengine);
size_t ioat_get_max_io_size(bus_dmaengine_t dmaengine);

/*
 * Set interrupt coalescing on a DMA channel.
 *
 * The argument is in microseconds.  A zero value disables coalescing.  Any
 * other value delays interrupt generation for N microseconds to provide
 * opportunity to coalesce multiple operations into a single interrupt.
 *
 * Returns an error status, or zero on success.
 *
 * - ERANGE if the given value exceeds the delay supported by the hardware.
 *   (All current hardware supports a maximum of 0x3fff microseconds delay.)
 * - ENODEV if the hardware does not support interrupt coalescing.
 */
int ioat_set_interrupt_coalesce(bus_dmaengine_t dmaengine, uint16_t delay);

/*
 * Return the maximum supported coalescing period, for use in
 * ioat_set_interrupt_coalesce().  If the hardware does not support coalescing,
 * returns zero.
 */
uint16_t ioat_get_max_coalesce_period(bus_dmaengine_t dmaengine);

/*
 * Acquire must be called before issuing an operation to perform. Release is
 * called after.  Multiple operations can be issued within the context of one
 * acquire and release
 */
void ioat_acquire(bus_dmaengine_t dmaengine);
void ioat_release(bus_dmaengine_t dmaengine);

/*
 * Acquire_reserve can be called to ensure there is room for N descriptors.  If
 * it succeeds, the next N valid operations will successfully enqueue.
 *
 * It may fail with:
 *   - ENXIO if the channel is in an errored state, or the driver is being
 *     unloaded
 *   - EAGAIN if mflags included M_NOWAIT
 *
 * On failure, the caller does not hold the dmaengine.
 */
int ioat_acquire_reserve(bus_dmaengine_t dmaengine, unsigned n, int mflags);

/*
 * Issue a blockfill operation.  The 64-bit pattern 'fillpattern' is written to
 * 'len' physically contiguous bytes at 'dst'.
 *
 * Only supported on devices with the BFILL capability.
 */
struct bus_dmadesc *ioat_blockfill(bus_dmaengine_t dmaengine, bus_addr_t dst,
    uint64_t fillpattern, bus_size_t len, bus_dmaengine_callback_t callback_fn,
    void *callback_arg, uint32_t flags);

/* Issues the copy data operation */
struct bus_dmadesc *ioat_copy(bus_dmaengine_t dmaengine, bus_addr_t dst,
    bus_addr_t src, bus_size_t len, bus_dmaengine_callback_t callback_fn,
    void *callback_arg, uint32_t flags);

/*
 * Issue a copy data operation, with constraints:
 *  - src1, src2, dst1, dst2 are all page-aligned addresses
 *  - The quantity to copy is exactly 2 pages;
 *  - src1 -> dst1, src2 -> dst2
 *
 * Why use this instead of normal _copy()?  You can copy two non-contiguous
 * pages (src, dst, or both) with one descriptor.
 */
struct bus_dmadesc *ioat_copy_8k_aligned(bus_dmaengine_t dmaengine,
    bus_addr_t dst1, bus_addr_t dst2, bus_addr_t src1, bus_addr_t src2,
    bus_dmaengine_callback_t callback_fn, void *callback_arg, uint32_t flags);

/*
 * Issues a null operation. This issues the operation to the hardware, but the
 * hardware doesn't do anything with it.
 */
struct bus_dmadesc *ioat_null(bus_dmaengine_t dmaengine,
    bus_dmaengine_callback_t callback_fn, void *callback_arg, uint32_t flags);


#endif /* __IOAT_H__ */

