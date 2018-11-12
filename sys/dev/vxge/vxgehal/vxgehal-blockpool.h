/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_HAL_BLOCKPOOL_H
#define	VXGE_HAL_BLOCKPOOL_H

__EXTERN_BEGIN_DECLS

/*
 * struct __hal_blockpool_entry_t - Block private data structure
 * @item: List header used to link.
 * @length: Length of the block
 * @memblock: Virtual address block
 * @dma_addr: DMA Address of the block.
 * @dma_handle: DMA handle of the block.
 * @acc_handle: DMA acc handle
 *
 * Block is allocated with a header to put the blocks into list.
 *
 */
typedef struct __hal_blockpool_entry_t {
	vxge_list_t item;
	u32	length;
	void   *memblock;
	dma_addr_t dma_addr;
	pci_dma_h dma_handle;
	pci_dma_acc_h acc_handle;
} __hal_blockpool_entry_t;

/*
 * struct __hal_blockpool_t - Block Pool
 * @hldev: HAL device
 * @block_size: size of each block.
 * @Pool_size: Number of blocks in the pool
 * @pool_incr: Number of blocks to be requested/freed at a time from OS
 * @pool_min: Minimum number of block below which to request additional blocks
 * @pool_max: Maximum number of blocks above which to free additional blocks
 * @req_out: Number of block requests with OS out standing
 * @dma_flags: DMA flags
 * @free_block_list: List of free blocks
 * @pool_lock: Spin lock for the pool
 *
 * Block pool contains the DMA blocks preallocated.
 *
 */
typedef struct __hal_blockpool_t {
	vxge_hal_device_h hldev;
	u32	block_size;
	u32	pool_size;
	u32	pool_incr;
	u32	pool_min;
	u32	pool_max;
	u32	req_out;
	u32	dma_flags;
	vxge_list_t free_block_list;
	vxge_list_t free_entry_list;

#if defined(VXGE_HAL_BP_POST) || defined(VXGE_HAL_BP_POST_IRQ)
	spinlock_t pool_lock;
#endif

} __hal_blockpool_t;

vxge_hal_status_e
__hal_blockpool_create(vxge_hal_device_h hldev,
    __hal_blockpool_t *blockpool,
    u32 pool_size,
    u32 pool_incr,
    u32 pool_min,
    u32 pool_max);

void
__hal_blockpool_destroy(__hal_blockpool_t *blockpool);

__hal_blockpool_entry_t *
__hal_blockpool_block_allocate(vxge_hal_device_h hldev,
    u32 size);

void
__hal_blockpool_block_free(vxge_hal_device_h hldev,
    __hal_blockpool_entry_t *entry);

void *
__hal_blockpool_malloc(vxge_hal_device_h hldev,
    u32 size,
    dma_addr_t *dma_addr,
    pci_dma_h *dma_handle,
    pci_dma_acc_h *acc_handle);

void
__hal_blockpool_free(vxge_hal_device_h hldev,
    void *memblock,
    u32 size,
    dma_addr_t *dma_addr,
    pci_dma_h *dma_handle,
    pci_dma_acc_h *acc_handle);

vxge_hal_status_e
__hal_blockpool_list_allocate(vxge_hal_device_h hldev,
    vxge_list_t *blocklist, u32 count);

void
__hal_blockpool_list_free(vxge_hal_device_h hldev,
    vxge_list_t *blocklist);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_BLOCKPOOL_H */
