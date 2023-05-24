/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

/**
 * @file ice_osdep.c
 * @brief Functions used to implement OS compatibility layer
 *
 * Contains functions used by ice_osdep.h to implement the OS compatibility
 * layer used by some of the hardware files. Specifically, it is for the bits
 * of OS compatibility which don't make sense as macros or inline functions.
 */

#include "ice_common.h"
#include "ice_iflib.h"
#include <machine/stdarg.h>
#include <sys/time.h>

/**
 * @var M_ICE_OSDEP
 * @brief OS compatibility layer allocation type
 *
 * malloc(9) allocation type used by the OS compatibility layer for
 * distinguishing allocations by this layer from those of the rest of the
 * driver.
 */
MALLOC_DEFINE(M_ICE_OSDEP, "ice-osdep", "Intel(R) 100Gb Network Driver osdep allocations");

/**
 * @var ice_lock_count
 * @brief Global count of # of ice_lock mutexes initialized
 *
 * A global count of the total number of times that ice_init_lock has been
 * called. This is used to generate unique lock names for each ice_lock, to
 * aid in witness lock checking.
 */
u16 ice_lock_count = 0;

static void ice_dmamap_cb(void *arg, bus_dma_segment_t * segs, int __unused nseg, int error);

/**
 * ice_hw_to_dev - Given a hw private struct, find the associated device_t
 * @hw: the hardware private structure
 *
 * Given a hw structure pointer, lookup the softc and extract the device
 * pointer. Assumes that hw is embedded within the ice_softc, instead of being
 * allocated separately, so that __containerof math will work.
 *
 * This can't be defined in ice_osdep.h as it depends on the complete
 * definition of struct ice_softc. That can't be easily included in
 * ice_osdep.h without creating circular header dependencies.
 */
device_t
ice_hw_to_dev(struct ice_hw *hw) {
	struct ice_softc *sc = __containerof(hw, struct ice_softc, hw);

	return sc->dev;
}

/**
 * ice_debug - Log a debug message if the type is enabled
 * @hw: device private hardware structure
 * @mask: the debug message type
 * @fmt: printf format specifier
 *
 * Check if hw->debug_mask has enabled the given message type. If so, log the
 * message to the console using vprintf. Mimic the output of device_printf by
 * using device_print_prettyname().
 */
void
ice_debug(struct ice_hw *hw, uint64_t mask, char *fmt, ...)
{
	device_t dev = ice_hw_to_dev(hw);
	va_list args;

	if (!(mask & hw->debug_mask))
		return;

	device_print_prettyname(dev);
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

/**
 * ice_debug_array - Format and print an array of values to the console
 * @hw: private hardware structure
 * @mask: the debug message type
 * @rowsize: preferred number of rows to use
 * @groupsize: preferred size in bytes to print each chunk
 * @buf: the array buffer to print
 * @len: size of the array buffer
 *
 * Format the given array as a series of uint8_t values with hexadecimal
 * notation and log the contents to the console log.
 *
 * TODO: Currently only supports a group size of 1, due to the way hexdump is
 * implemented.
 */
void
ice_debug_array(struct ice_hw *hw, uint64_t mask, uint32_t rowsize,
		uint32_t __unused groupsize, uint8_t *buf, size_t len)
{
	device_t dev = ice_hw_to_dev(hw);
	char prettyname[20];

	if (!(mask & hw->debug_mask))
		return;

	/* Format the device header to a string */
	snprintf(prettyname, sizeof(prettyname), "%s: ", device_get_nameunit(dev));

	/* Make sure the row-size isn't too large */
	if (rowsize > 0xFF)
		rowsize = 0xFF;

	hexdump(buf, len, prettyname, HD_OMIT_CHARS | rowsize);
}

/**
 * ice_info_fwlog - Format and print an array of values to the console
 * @hw: private hardware structure
 * @rowsize: preferred number of rows to use
 * @groupsize: preferred size in bytes to print each chunk
 * @buf: the array buffer to print
 * @len: size of the array buffer
 *
 * Format the given array as a series of uint8_t values with hexadecimal
 * notation and log the contents to the console log.  This variation is
 * specific to firmware logging.
 *
 * TODO: Currently only supports a group size of 1, due to the way hexdump is
 * implemented.
 */
void
ice_info_fwlog(struct ice_hw *hw, uint32_t rowsize, uint32_t __unused groupsize,
	       uint8_t *buf, size_t len)
{
	device_t dev = ice_hw_to_dev(hw);
	char prettyname[20];

	if (!ice_fwlog_supported(hw))
		return;

	/* Format the device header to a string */
	snprintf(prettyname, sizeof(prettyname), "%s: FWLOG: ",
	    device_get_nameunit(dev));

	/* Make sure the row-size isn't too large */
	if (rowsize > 0xFF)
		rowsize = 0xFF;

	hexdump(buf, len, prettyname, HD_OMIT_CHARS | rowsize);
}

/**
 * rd32 - Read a 32bit hardware register value
 * @hw: the private hardware structure
 * @reg: register address to read
 *
 * Read the specified 32bit register value from BAR0 and return its contents.
 */
uint32_t
rd32(struct ice_hw *hw, uint32_t reg)
{
	struct ice_softc *sc = __containerof(hw, struct ice_softc, hw);

	return bus_space_read_4(sc->bar0.tag, sc->bar0.handle, reg);
}

/**
 * rd64 - Read a 64bit hardware register value
 * @hw: the private hardware structure
 * @reg: register address to read
 *
 * Read the specified 64bit register value from BAR0 and return its contents.
 *
 * @pre For 32-bit builds, assumes that the 64bit register read can be
 * safely broken up into two 32-bit register reads.
 */
uint64_t
rd64(struct ice_hw *hw, uint32_t reg)
{
	struct ice_softc *sc = __containerof(hw, struct ice_softc, hw);
	uint64_t data;

#ifdef __amd64__
	data = bus_space_read_8(sc->bar0.tag, sc->bar0.handle, reg);
#else
	/*
	 * bus_space_read_8 isn't supported on 32bit platforms, so we fall
	 * back to using two bus_space_read_4 calls.
	 */
	data = bus_space_read_4(sc->bar0.tag, sc->bar0.handle, reg);
	data |= ((uint64_t)bus_space_read_4(sc->bar0.tag, sc->bar0.handle, reg + 4)) << 32;
#endif

	return data;
}

/**
 * wr32 - Write a 32bit hardware register
 * @hw: the private hardware structure
 * @reg: the register address to write to
 * @val: the 32bit value to write
 *
 * Write the specified 32bit value to a register address in BAR0.
 */
void
wr32(struct ice_hw *hw, uint32_t reg, uint32_t val)
{
	struct ice_softc *sc = __containerof(hw, struct ice_softc, hw);

	bus_space_write_4(sc->bar0.tag, sc->bar0.handle, reg, val);
}

/**
 * wr64 - Write a 64bit hardware register
 * @hw: the private hardware structure
 * @reg: the register address to write to
 * @val: the 64bit value to write
 *
 * Write the specified 64bit value to a register address in BAR0.
 *
 * @pre For 32-bit builds, assumes that the 64bit register write can be safely
 * broken up into two 32-bit register writes.
 */
void
wr64(struct ice_hw *hw, uint32_t reg, uint64_t val)
{
	struct ice_softc *sc = __containerof(hw, struct ice_softc, hw);

#ifdef __amd64__
	bus_space_write_8(sc->bar0.tag, sc->bar0.handle, reg, val);
#else
	uint32_t lo_val, hi_val;

	/*
	 * bus_space_write_8 isn't supported on 32bit platforms, so we fall
	 * back to using two bus_space_write_4 calls.
	 */
	lo_val = (uint32_t)val;
	hi_val = (uint32_t)(val >> 32);
	bus_space_write_4(sc->bar0.tag, sc->bar0.handle, reg, lo_val);
	bus_space_write_4(sc->bar0.tag, sc->bar0.handle, reg + 4, hi_val);
#endif
}

/**
 * ice_usec_delay - Delay for the specified number of microseconds
 * @time: microseconds to delay
 * @sleep: if true, sleep where possible
 *
 * If sleep is true, and if the current thread is allowed to sleep, pause so
 * that another thread can execute. Otherwise, use DELAY to spin the thread
 * instead.
 */
void
ice_usec_delay(uint32_t time, bool sleep)
{
	if (sleep && THREAD_CAN_SLEEP())
		pause("ice_usec_delay", USEC_2_TICKS(time));
	else
		DELAY(time);
}

/**
 * ice_msec_delay - Delay for the specified number of milliseconds
 * @time: milliseconds to delay
 * @sleep: if true, sleep where possible
 *
 * If sleep is true, and if the current thread is allowed to sleep, pause so
 * that another thread can execute. Otherwise, use DELAY to spin the thread
 * instead.
 */
void
ice_msec_delay(uint32_t time, bool sleep)
{
	if (sleep && THREAD_CAN_SLEEP())
		pause("ice_msec_delay", MSEC_2_TICKS(time));
	else
		DELAY(time * 1000);
}

/**
 * ice_msec_pause - pause (sleep) the thread for a time in milliseconds
 * @time: milliseconds to sleep
 *
 * Wrapper for ice_msec_delay with sleep set to true.
 */
void
ice_msec_pause(uint32_t time)
{
	ice_msec_delay(time, true);
}

/**
 * ice_msec_spin - Spin the thread for a time in milliseconds
 * @time: milliseconds to delay
 *
 * Wrapper for ice_msec_delay with sleep sent to false.
 */
void
ice_msec_spin(uint32_t time)
{
	ice_msec_delay(time, false);
}

/********************************************************************
 * Manage DMA'able memory.
 *******************************************************************/

/**
 * ice_dmamap_cb - Callback function DMA maps
 * @arg: pointer to return the segment address
 * @segs: the segments array
 * @nseg: number of segments in the array
 * @error: error code
 *
 * Callback used by the bus DMA code to obtain the segment address.
 */
static void
ice_dmamap_cb(void *arg, bus_dma_segment_t * segs, int __unused nseg, int error)
{
	if (error)
		return;
	*(bus_addr_t *) arg = segs->ds_addr;
	return;
}

/**
 * ice_alloc_dma_mem - Request OS to allocate DMA memory
 * @hw: private hardware structure
 * @mem: structure defining the DMA memory request
 * @size: the allocation size
 *
 * Allocates some memory for DMA use. Use the FreeBSD bus DMA interface to
 * track this memory using a bus DMA tag and map.
 *
 * Returns a pointer to the DMA memory address.
 */
void *
ice_alloc_dma_mem(struct ice_hw *hw, struct ice_dma_mem *mem, u64 size)
{
	device_t dev = ice_hw_to_dev(hw);
	int err;

	err = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
				 1, 0,			/* alignment, boundary */
				 BUS_SPACE_MAXADDR,	/* lowaddr */
				 BUS_SPACE_MAXADDR,	/* highaddr */
				 NULL, NULL,		/* filtfunc, filtfuncarg */
				 size,			/* maxsize */
				 1,			/* nsegments */
				 size,			/* maxsegsz */
				 BUS_DMA_ALLOCNOW,	/* flags */
				 NULL,			/* lockfunc */
				 NULL,			/* lockfuncarg */
				 &mem->tag);
	if (err != 0) {
		device_printf(dev,
		    "ice_alloc_dma: bus_dma_tag_create failed, "
		    "error %s\n", ice_err_str(err));
		goto fail_0;
	}
	err = bus_dmamem_alloc(mem->tag, (void **)&mem->va,
			     BUS_DMA_NOWAIT | BUS_DMA_ZERO, &mem->map);
	if (err != 0) {
		device_printf(dev,
		    "ice_alloc_dma: bus_dmamem_alloc failed, "
		    "error %s\n", ice_err_str(err));
		goto fail_1;
	}
	err = bus_dmamap_load(mem->tag, mem->map, mem->va,
			    size,
			    ice_dmamap_cb,
			    &mem->pa,
			    BUS_DMA_NOWAIT);
	if (err != 0) {
		device_printf(dev,
		    "ice_alloc_dma: bus_dmamap_load failed, "
		    "error %s\n", ice_err_str(err));
		goto fail_2;
	}
	mem->size = size;
	bus_dmamap_sync(mem->tag, mem->map,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	return (mem->va);
fail_2:
	bus_dmamem_free(mem->tag, mem->va, mem->map);
fail_1:
	bus_dma_tag_destroy(mem->tag);
fail_0:
	mem->map = NULL;
	mem->tag = NULL;
	return (NULL);
}

/**
 * ice_free_dma_mem - Free DMA memory allocated by ice_alloc_dma_mem
 * @hw: the hardware private structure
 * @mem: DMA memory to free
 *
 * Release the bus DMA tag and map, and free the DMA memory associated with
 * it.
 */
void
ice_free_dma_mem(struct ice_hw __unused *hw, struct ice_dma_mem *mem)
{
	bus_dmamap_sync(mem->tag, mem->map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(mem->tag, mem->map);
	bus_dmamem_free(mem->tag, mem->va, mem->map);
	bus_dma_tag_destroy(mem->tag);
	mem->map = NULL;
	mem->tag = NULL;
}
