/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
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

/**
 * @file iavf_osdep.c
 * @brief OS compatibility layer
 *
 * Contains definitions for various functions used to provide an OS
 * independent layer for sharing code between drivers on different operating
 * systems.
 */
#include <machine/stdarg.h>

#include "iavf_iflib.h"

/********************************************************************
 * Manage DMA'able memory.
 *******************************************************************/

/**
 * iavf_dmamap_cb - DMA mapping callback function
 * @arg: pointer to return the segment address
 * @segs: the segments array
 * @nseg: number of segments in the array
 * @error: error code
 *
 * Callback used by the bus DMA code to obtain the segment address.
 */
static void
iavf_dmamap_cb(void *arg, bus_dma_segment_t * segs, int nseg __unused,
	       int error)
{
        if (error)
                return;
        *(bus_addr_t *) arg = segs->ds_addr;
        return;
}

/**
 * iavf_allocate_virt_mem - Allocate virtual memory
 * @hw: hardware structure
 * @mem: structure describing the memory allocation
 * @size: size of the allocation
 *
 * OS compatibility function to allocate virtual memory.
 *
 * @returns zero on success, or a status code on failure.
 */
enum iavf_status
iavf_allocate_virt_mem(struct iavf_hw *hw __unused, struct iavf_virt_mem *mem,
		       u32 size)
{
	mem->va = malloc(size, M_IAVF, M_NOWAIT | M_ZERO);
	return(mem->va == NULL);
}

/**
 * iavf_free_virt_mem - Free virtual memory
 * @hw: hardware structure
 * @mem: structure describing the memory to free
 *
 * OS compatibility function to free virtual memory
 *
 * @returns zero.
 */
enum iavf_status
iavf_free_virt_mem(struct iavf_hw *hw __unused, struct iavf_virt_mem *mem)
{
	free(mem->va, M_IAVF);
	mem->va = NULL;

	return(0);
}

/**
 * iavf_allocate_dma_mem - Allocate DMA memory
 * @hw: hardware structure
 * @mem: structure describing the memory allocation
 * @type: unused type parameter specifying the type of allocation
 * @size: size of the allocation
 * @alignment: alignment requirements for the allocation
 *
 * Allocates DMA memory by using bus_dma_tag_create to create a DMA tag, and
 * them bus_dmamem_alloc to allocate the associated memory.
 *
 * @returns zero on success, or a status code on failure.
 */
enum iavf_status
iavf_allocate_dma_mem(struct iavf_hw *hw, struct iavf_dma_mem *mem,
	enum iavf_memory_type type __unused, u64 size, u32 alignment)
{
	device_t	dev = ((struct iavf_osdep *)hw->back)->dev;
	int		err;


	err = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
			       alignment, 0,	/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,	/* filter, filterarg */
			       size,	/* maxsize */
			       1,	/* nsegments */
			       size,	/* maxsegsize */
			       BUS_DMA_ALLOCNOW, /* flags */
			       NULL,	/* lockfunc */
			       NULL,	/* lockfuncarg */
			       &mem->tag);
	if (err != 0) {
		device_printf(dev,
		    "iavf_allocate_dma: bus_dma_tag_create failed, "
		    "error %u\n", err);
		goto fail_0;
	}
	err = bus_dmamem_alloc(mem->tag, (void **)&mem->va,
			     BUS_DMA_NOWAIT | BUS_DMA_ZERO, &mem->map);
	if (err != 0) {
		device_printf(dev,
		    "iavf_allocate_dma: bus_dmamem_alloc failed, "
		    "error %u\n", err);
		goto fail_1;
	}
	err = bus_dmamap_load(mem->tag, mem->map, mem->va,
			    size,
			    iavf_dmamap_cb,
			    &mem->pa,
			    BUS_DMA_NOWAIT);
	if (err != 0) {
		device_printf(dev,
		    "iavf_allocate_dma: bus_dmamap_load failed, "
		    "error %u\n", err);
		goto fail_2;
	}
	mem->nseg = 1;
	mem->size = size;
	bus_dmamap_sync(mem->tag, mem->map,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	return (0);
fail_2:
	bus_dmamem_free(mem->tag, mem->va, mem->map);
fail_1:
	bus_dma_tag_destroy(mem->tag);
fail_0:
	mem->map = NULL;
	mem->tag = NULL;
	return (err);
}

/**
 * iavf_free_dma_mem - Free DMA memory allocation
 * @hw: hardware structure
 * @mem: pointer to memory structure previously allocated
 *
 * Releases DMA memory that was previously allocated by iavf_allocate_dma_mem.
 *
 * @returns zero.
 */
enum iavf_status
iavf_free_dma_mem(struct iavf_hw *hw __unused, struct iavf_dma_mem *mem)
{
	bus_dmamap_sync(mem->tag, mem->map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(mem->tag, mem->map);
	bus_dmamem_free(mem->tag, mem->va, mem->map);
	bus_dma_tag_destroy(mem->tag);
	return (0);
}

/**
 * iavf_init_spinlock - Initialize a spinlock
 * @lock: OS compatibility lock structure
 *
 * Use the mutex layer to initialize a spin lock that can be used via the OS
 * compatibility layer accessors.
 *
 * @remark we pass MTX_DUPOK because the mutex name will not be unique. An
 * alternative would be to somehow generate a name, such as by passing in the
 * __file__ and __line__ values from a macro.
 */
void
iavf_init_spinlock(struct iavf_spinlock *lock)
{
	mtx_init(&lock->mutex, "mutex",
	    "iavf spinlock", MTX_DEF | MTX_DUPOK);
}

/**
 * iavf_acquire_spinlock - Acquire a spin lock
 * @lock: OS compatibility lock structure
 *
 * Acquire a spin lock using mtx_lock.
 */
void
iavf_acquire_spinlock(struct iavf_spinlock *lock)
{
	mtx_lock(&lock->mutex);
}

/**
 * iavf_release_spinlock - Release a spin lock
 * @lock: OS compatibility lock structure
 *
 * Release a spin lock using mtx_unlock.
 */
void
iavf_release_spinlock(struct iavf_spinlock *lock)
{
	mtx_unlock(&lock->mutex);
}

/**
 * iavf_destroy_spinlock - Destroy a spin lock
 * @lock: OS compatibility lock structure
 *
 * Destroy (deinitialize) a spin lock by calling mtx_destroy.
 *
 * @remark we only destroy the lock if it was initialized. This means that
 * calling iavf_destroy_spinlock on a lock that was already destroyed or was
 * never initialized is not considered a bug.
 */
void
iavf_destroy_spinlock(struct iavf_spinlock *lock)
{
	if (mtx_initialized(&lock->mutex))
		mtx_destroy(&lock->mutex);
}

/**
 * iavf_debug_shared - Log a debug message if enabled
 * @hw: device hardware structure
 * @mask: bit indicating the type of the message
 * @fmt: printf format string
 *
 * Checks if the mask is enabled in the hw->debug_mask. If so, prints
 * a message to the console using vprintf().
 */
void
iavf_debug_shared(struct iavf_hw *hw, uint64_t mask, char *fmt, ...)
{
	va_list args;
	device_t dev;

	if (!(mask & ((struct iavf_hw *)hw)->debug_mask))
		return;

	dev = ((struct iavf_osdep *)hw->back)->dev;

	/* Re-implement device_printf() */
	device_print_prettyname(dev);
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

/**
 * iavf_read_pci_cfg - Read a PCI config register
 * @hw: device hardware structure
 * @reg: the PCI register to read
 *
 * Calls pci_read_config to read the given PCI register from the PCI config
 * space.
 *
 * @returns the value of the register.
 */
u16
iavf_read_pci_cfg(struct iavf_hw *hw, u32 reg)
{
        u16 value;

        value = pci_read_config(((struct iavf_osdep *)hw->back)->dev,
            reg, 2);

        return (value);
}

/**
 * iavf_write_pci_cfg - Write a PCI config register
 * @hw: device hardware structure
 * @reg: the PCI register to write
 * @value: the value to write
 *
 * Calls pci_write_config to write to a given PCI register in the PCI config
 * space.
 */
void
iavf_write_pci_cfg(struct iavf_hw *hw, u32 reg, u16 value)
{
        pci_write_config(((struct iavf_osdep *)hw->back)->dev,
            reg, value, 2);

        return;
}

/**
 * iavf_rd32 - Read a 32bit hardware register value
 * @hw: the private hardware structure
 * @reg: register address to read
 *
 * Read the specified 32bit register value from BAR0 and return its contents.
 *
 * @returns the value of the 32bit register.
 */
inline uint32_t
iavf_rd32(struct iavf_hw *hw, uint32_t reg)
{
	struct iavf_osdep *osdep = (struct iavf_osdep *)hw->back;

	KASSERT(reg < osdep->mem_bus_space_size,
	    ("iavf: register offset %#jx too large (max is %#jx)",
	    (uintmax_t)reg, (uintmax_t)osdep->mem_bus_space_size));

	return (bus_space_read_4(osdep->mem_bus_space_tag,
	    osdep->mem_bus_space_handle, reg));
}

/**
 * iavf_wr32 - Write a 32bit hardware register
 * @hw: the private hardware structure
 * @reg: the register address to write to
 * @val: the 32bit value to write
 *
 * Write the specified 32bit value to a register address in BAR0.
 */
inline void
iavf_wr32(struct iavf_hw *hw, uint32_t reg, uint32_t val)
{
	struct iavf_osdep *osdep = (struct iavf_osdep *)hw->back;

	KASSERT(reg < osdep->mem_bus_space_size,
	    ("iavf: register offset %#jx too large (max is %#jx)",
	    (uintmax_t)reg, (uintmax_t)osdep->mem_bus_space_size));

	bus_space_write_4(osdep->mem_bus_space_tag,
	    osdep->mem_bus_space_handle, reg, val);
}

/**
 * iavf_flush - Flush register writes
 * @hw: private hardware structure
 *
 * Forces the completion of outstanding PCI register writes by reading from
 * a specific hardware register.
 */
inline void
iavf_flush(struct iavf_hw *hw)
{
	struct iavf_osdep *osdep = (struct iavf_osdep *)hw->back;

	rd32(hw, osdep->flush_reg);
}

/**
 * iavf_debug_core - Debug printf for core driver code
 * @dev: the device_t to log under
 * @enabled_mask: the mask of enabled messages
 * @mask: the mask of the requested message to print
 * @fmt: printf format string
 *
 * If enabled_mask has the bit from the mask set, print a message to the
 * console using the specified format. This is used to conditionally enable
 * log messages at run time by toggling the enabled_mask in the device
 * structure.
 */
void
iavf_debug_core(device_t dev, u32 enabled_mask, u32 mask, char *fmt, ...)
{
	va_list args;

	if (!(mask & enabled_mask))
		return;

	/* Re-implement device_printf() */
	device_print_prettyname(dev);
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}
