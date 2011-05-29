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

#include <dev/vxge/vxgehal/vxgehal.h>

/*
 * __hal_blockpool_create - Create block pool
 * @devh: Pointer to HAL Device object.
 * @blockpool: Block pool to be created.
 * @pool_size: Number of blocks in the pool.
 * @pool_incr: Number of blocks to be request from OS at a time
 * @pool_min: Number of blocks below which new blocks to be requested.
 * @pool_max: Number of blocks above which block to be freed.
 *
 * This function creates block pool
 */

vxge_hal_status_e
__hal_blockpool_create(vxge_hal_device_h devh,
    __hal_blockpool_t *blockpool,
    u32 pool_size,
    u32 pool_incr,
    u32 pool_min,
    u32 pool_max)
{
	u32 i;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_blockpool_entry_t *entry;
	void *memblock;
	dma_addr_t dma_addr;
	pci_dma_h dma_handle;
	pci_dma_acc_h acc_handle;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(devh != NULL);
	vxge_os_memzero(&dma_handle, sizeof(pci_dma_h));
	vxge_os_memzero(&acc_handle, sizeof(pci_dma_acc_h));

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool("devh = 0x"VXGE_OS_STXFMT", "
	    "blockpool = 0x"VXGE_OS_STXFMT", pool_size = %d, pool_incr = %d, "
	    "pool_min = %d, pool_max = %d", (ptr_t) devh, (ptr_t) blockpool,
	    pool_size, pool_incr, pool_min, pool_max);

	if (blockpool == NULL) {
		vxge_hal_err_log_pool(
		    "%s:%d null pointer passed. blockpool is null",
		    __FILE__, __LINE__);
		vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	blockpool->hldev = devh;
	blockpool->block_size = VXGE_OS_HOST_PAGE_SIZE;
	blockpool->pool_size = 0;
	blockpool->pool_incr = pool_incr;
	blockpool->pool_min = pool_min;
	blockpool->pool_max = pool_max;
	blockpool->req_out = 0;

#if defined(VXGE_HAL_DMA_CONSISTENT)
	blockpool->dma_flags = VXGE_OS_DMA_CONSISTENT;
#else
	blockpool->dma_flags = VXGE_OS_DMA_STREAMING;
#endif

	vxge_list_init(&blockpool->free_block_list);

	vxge_list_init(&blockpool->free_entry_list);

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_lock_init(&blockpool->pool_lock, hldev->header.pdev);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_lock_init_irq(&blockpool->pool_lock, hldev->header.irqh);
#endif

	for (i = 0; i < pool_size + pool_max; i++) {

		entry = (__hal_blockpool_entry_t *) vxge_os_malloc(
		    hldev->header.pdev,
		    sizeof(__hal_blockpool_entry_t));
		if (entry == NULL) {
			__hal_blockpool_destroy(blockpool);

			vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_OUT_OF_MEMORY);

			return (VXGE_HAL_ERR_OUT_OF_MEMORY);
		}

		vxge_list_insert(&entry->item, &blockpool->free_entry_list);
	}

	for (i = 0; i < pool_size; i++) {

		memblock = vxge_os_dma_malloc(
		    hldev->header.pdev,
		    VXGE_OS_HOST_PAGE_SIZE,
		    blockpool->dma_flags,
		    &dma_handle,
		    &acc_handle);

		if (memblock == NULL) {
			__hal_blockpool_destroy(blockpool);
			vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_OUT_OF_MEMORY);

			return (VXGE_HAL_ERR_OUT_OF_MEMORY);
		}

		dma_addr = vxge_os_dma_map(
		    hldev->header.pdev,
		    dma_handle,
		    memblock,
		    VXGE_OS_HOST_PAGE_SIZE,
		    VXGE_OS_DMA_DIR_BIDIRECTIONAL,
		    blockpool->dma_flags);

		if (dma_addr == VXGE_OS_INVALID_DMA_ADDR) {
			vxge_os_dma_free(hldev->header.pdev,
			    memblock,
			    VXGE_OS_HOST_PAGE_SIZE,
			    blockpool->dma_flags,
			    &dma_handle,
			    &acc_handle);
			__hal_blockpool_destroy(blockpool);

			vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_OUT_OF_MEMORY);

			return (VXGE_HAL_ERR_OUT_OF_MEMORY);
		}

		entry = (__hal_blockpool_entry_t *)
		    vxge_list_first_get(&blockpool->free_entry_list);

		if (entry == NULL) {
			entry = (__hal_blockpool_entry_t *) vxge_os_malloc(
			    hldev->header.pdev,
			    sizeof(__hal_blockpool_entry_t));
		}

		if (entry != NULL) {
			vxge_list_remove(&entry->item);
			entry->length = VXGE_OS_HOST_PAGE_SIZE;
			entry->memblock = memblock;
			entry->dma_addr = dma_addr;
			entry->acc_handle = acc_handle;
			entry->dma_handle = dma_handle;
			vxge_list_insert(&entry->item,
			    &blockpool->free_block_list);
			blockpool->pool_size++;
		} else {
			__hal_blockpool_destroy(blockpool);
			vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_OUT_OF_MEMORY);

			return (VXGE_HAL_ERR_OUT_OF_MEMORY);
		}
	}

	vxge_hal_info_log_pool(
	    "Blockpool  block size:%d block pool size: %d",
	    blockpool->block_size, blockpool->pool_size);

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_blockpool_destroy - Deallocates the block pool
 * @blockpool: blockpool to be deallocated
 *
 * This function freeup the memory pool and removes the
 * block pool.
 */

void
__hal_blockpool_destroy(
    __hal_blockpool_t *blockpool)
{
	__hal_device_t *hldev;
	vxge_list_t *p, *n;

	vxge_assert(blockpool != NULL);

	hldev = (__hal_device_t *) blockpool->hldev;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool("blockpool = 0x"VXGE_OS_STXFMT,
	    (ptr_t) blockpool);

	if (blockpool == NULL) {
		vxge_hal_err_log_pool(
		    "%s:%d null pointer passed blockpool = null",
		    __FILE__, __LINE__);
		vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 1",
		    __FILE__, __func__, __LINE__);
		return;
	}

	vxge_list_for_each_safe(p, n, &blockpool->free_block_list) {

		vxge_os_dma_unmap(hldev->header.pdev,
		    ((__hal_blockpool_entry_t *) p)->dma_handle,
		    ((__hal_blockpool_entry_t *) p)->dma_addr,
		    ((__hal_blockpool_entry_t *) p)->length,
		    VXGE_OS_DMA_DIR_BIDIRECTIONAL);

		vxge_os_dma_free(hldev->header.pdev,
		    ((__hal_blockpool_entry_t *) p)->memblock,
		    ((__hal_blockpool_entry_t *) p)->length,
		    blockpool->dma_flags,
		    &((__hal_blockpool_entry_t *) p)->dma_handle,
		    &((__hal_blockpool_entry_t *) p)->acc_handle);

		vxge_list_remove(&((__hal_blockpool_entry_t *) p)->item);

		vxge_os_free(hldev->header.pdev,
		    (void *)p, sizeof(__hal_blockpool_entry_t));

		blockpool->pool_size--;
	}

	vxge_list_for_each_safe(p, n, &blockpool->free_entry_list) {

		vxge_list_remove(&((__hal_blockpool_entry_t *) p)->item);

		vxge_os_free(hldev->header.pdev,
		    (void *)p, sizeof(__hal_blockpool_entry_t));

	}

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_lock_destroy(&blockpool->pool_lock,
	    hldev->header.pdev);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_lock_destroy_irq(&blockpool->pool_lock,
	    hldev->header.pdev);
#endif

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_blockpool_blocks_add - Request additional blocks
 * @blockpool: Block pool.
 *
 * Requests additional blocks to block pool
 */
static inline void
__hal_blockpool_blocks_add(
    __hal_blockpool_t * blockpool)
{
	u32 nreq = 0, i;
	__hal_device_t *hldev;

	vxge_assert(blockpool != NULL);

	hldev = (__hal_device_t *) blockpool->hldev;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool("blockpool = 0x"VXGE_OS_STXFMT,
	    (ptr_t) blockpool);

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif
	if ((blockpool->pool_size + blockpool->req_out) <
	    blockpool->pool_min) {
		nreq = blockpool->pool_incr;
		blockpool->req_out += nreq;
	}

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif

	for (i = 0; i < nreq; i++) {
		vxge_os_dma_malloc_async(
		    ((__hal_device_t *) blockpool->hldev)->header.pdev,
		    blockpool->hldev,
		    VXGE_OS_HOST_PAGE_SIZE,
		    blockpool->dma_flags);
	}

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_blockpool_blocks_remove - Free additional blocks
 * @blockpool: Block pool.
 *
 * Frees additional blocks over maximum from the block pool
 */
static inline void
__hal_blockpool_blocks_remove(
    __hal_blockpool_t * blockpool)
{
	vxge_list_t *p, *n;
	__hal_device_t *hldev;

	vxge_assert(blockpool != NULL);

	hldev = (__hal_device_t *) blockpool->hldev;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool("blockpool = 0x"VXGE_OS_STXFMT,
	    (ptr_t) blockpool);

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif
	vxge_list_for_each_safe(p, n, &blockpool->free_block_list) {

		if (blockpool->pool_size < blockpool->pool_max)
			break;

		vxge_os_dma_unmap(
		    ((__hal_device_t *) blockpool->hldev)->header.pdev,
		    ((__hal_blockpool_entry_t *) p)->dma_handle,
		    ((__hal_blockpool_entry_t *) p)->dma_addr,
		    ((__hal_blockpool_entry_t *) p)->length,
		    VXGE_OS_DMA_DIR_BIDIRECTIONAL);

		vxge_os_dma_free(
		    ((__hal_device_t *) blockpool->hldev)->header.pdev,
		    ((__hal_blockpool_entry_t *) p)->memblock,
		    ((__hal_blockpool_entry_t *) p)->length,
		    blockpool->dma_flags,
		    &((__hal_blockpool_entry_t *) p)->dma_handle,
		    &((__hal_blockpool_entry_t *) p)->acc_handle);

		vxge_list_remove(&((__hal_blockpool_entry_t *) p)->item);

		vxge_list_insert(p, &blockpool->free_entry_list);

		blockpool->pool_size--;

	}

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_blockpool_block_add - callback for vxge_os_dma_malloc_async
 * @devh: HAL device handle.
 * @block_addr: Virtual address of the block
 * @length: Length of the block.
 * @p_dma_h: Physical address of the block
 * @acc_handle: DMA acc handle
 *
 * Adds a block to block pool
 */
void
vxge_hal_blockpool_block_add(
    vxge_hal_device_h devh,
    void *block_addr,
    u32 length,
    pci_dma_h * dma_h,
    pci_dma_acc_h * acc_handle)
{
	__hal_blockpool_t *blockpool;
	__hal_blockpool_entry_t *entry;
	__hal_device_t *hldev;
	dma_addr_t dma_addr;
	vxge_hal_status_e status;
	u32 req_out;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool("devh = 0x"VXGE_OS_STXFMT", length = %d, "
	    "block_addr = 0x"VXGE_OS_STXFMT",  dma_h = 0x"VXGE_OS_STXFMT", "
	    "acc_handle = 0x"VXGE_OS_STXFMT, (ptr_t) devh, length,
	    (ptr_t) block_addr, (ptr_t) dma_h, (ptr_t) acc_handle);

	blockpool = &hldev->block_pool;

	if (block_addr == NULL) {
#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif
		blockpool->req_out--;

#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif
		vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 1",
		    __FILE__, __func__, __LINE__);
		return;
	}

	dma_addr = vxge_os_dma_map(hldev->header.pdev,
	    *dma_h,
	    block_addr,
	    length,
	    VXGE_OS_DMA_DIR_BIDIRECTIONAL,
	    blockpool->dma_flags);

	if (dma_addr == VXGE_OS_INVALID_DMA_ADDR) {
		vxge_os_dma_free(hldev->header.pdev,
		    block_addr,
		    length,
		    blockpool->dma_flags,
		    dma_h,
		    acc_handle);
#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif
		blockpool->req_out--;

#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif
		vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 1",
		    __FILE__, __func__, __LINE__);
		return;
	}

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif

	entry = (__hal_blockpool_entry_t *)
	    vxge_list_first_get(&blockpool->free_entry_list);

	if (entry == NULL) {
		entry = (__hal_blockpool_entry_t *) vxge_os_malloc(
		    hldev->header.pdev,
		    sizeof(__hal_blockpool_entry_t));
	} else {
		vxge_list_remove(&entry->item);
	}

	if (entry != NULL) {
		entry->length = length;
		entry->memblock = block_addr;
		entry->dma_addr = dma_addr;
		entry->acc_handle = *acc_handle;
		entry->dma_handle = *dma_h;
		vxge_list_insert(&entry->item, &blockpool->free_block_list);
		blockpool->pool_size++;
		status = VXGE_HAL_OK;
	} else {
		status = VXGE_HAL_ERR_OUT_OF_MEMORY;
	}

	blockpool->req_out--;

	req_out = blockpool->req_out;

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif

	if (req_out == 0)
		__hal_channel_process_pending_list(devh);

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
}

/*
 * __hal_blockpool_malloc - Allocate a memory block from pool
 * @devh: HAL device handle.
 * @size: Length of the block.
 * @dma_addr: Buffer to return DMA Address of the block.
 * @dma_handle: Buffer to return DMA handle of the block.
 * @acc_handle: Buffer to return DMA acc handle
 *
 *
 * Allocates a block of memory of given size, either from block pool
 * or by calling vxge_os_dma_malloc()
 */
void *
__hal_blockpool_malloc(vxge_hal_device_h devh,
    u32 size,
    dma_addr_t *dma_addr,
    pci_dma_h *dma_handle,
    pci_dma_acc_h *acc_handle)
{
	__hal_blockpool_entry_t *entry;
	__hal_blockpool_t *blockpool;
	__hal_device_t *hldev;
	void *memblock = NULL;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool(
	    "devh = 0x"VXGE_OS_STXFMT", size = %d, "
	    "dma_addr = 0x"VXGE_OS_STXFMT", dma_handle = 0x"VXGE_OS_STXFMT", "
	    "acc_handle = 0x"VXGE_OS_STXFMT, (ptr_t) devh, size,
	    (ptr_t) dma_addr, (ptr_t) dma_handle, (ptr_t) acc_handle);

	blockpool = &((__hal_device_t *) devh)->block_pool;

	if (size != blockpool->block_size) {

		memblock = vxge_os_dma_malloc(
		    ((__hal_device_t *) devh)->header.pdev,
		    size,
		    blockpool->dma_flags |
		    VXGE_OS_DMA_CACHELINE_ALIGNED,
		    dma_handle,
		    acc_handle);

		if (memblock == NULL) {
			vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_OUT_OF_MEMORY);
			return (NULL);
		}

		*dma_addr = vxge_os_dma_map(
		    ((__hal_device_t *) devh)->header.pdev,
		    *dma_handle,
		    memblock,
		    size,
		    VXGE_OS_DMA_DIR_BIDIRECTIONAL,
		    blockpool->dma_flags);

		if (*dma_addr == VXGE_OS_INVALID_DMA_ADDR) {
			vxge_os_dma_free(((__hal_device_t *) devh)->header.pdev,
			    memblock,
			    size,
			    blockpool->dma_flags |
			    VXGE_OS_DMA_CACHELINE_ALIGNED,
			    dma_handle,
			    acc_handle);

			vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_OUT_OF_MEMORY);

			return (NULL);
		}

	} else {

#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif

		entry = (__hal_blockpool_entry_t *)
		    vxge_list_first_get(&blockpool->free_block_list);

		if (entry != NULL) {
			vxge_list_remove(&entry->item);
			*dma_addr = entry->dma_addr;
			*dma_handle = entry->dma_handle;
			*acc_handle = entry->acc_handle;
			memblock = entry->memblock;

			vxge_list_insert(&entry->item,
			    &blockpool->free_entry_list);
			blockpool->pool_size--;
		}

#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif

		if (memblock != NULL)
			__hal_blockpool_blocks_add(blockpool);

	}

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, !memblock);

	return (memblock);

}

/*
 * __hal_blockpool_free - Frees the memory allcoated with __hal_blockpool_malloc
 * @devh: HAL device handle.
 * @memblock: Virtual address block
 * @size: Length of the block.
 * @dma_addr: DMA Address of the block.
 * @dma_handle: DMA handle of the block.
 * @acc_handle: DMA acc handle
 *
 *
 * Frees the memory allocated with __hal_blockpool_malloc to blockpool or system
 */
void
__hal_blockpool_free(vxge_hal_device_h devh,
    void *memblock,
    u32 size,
    dma_addr_t *dma_addr,
    pci_dma_h *dma_handle,
    pci_dma_acc_h *acc_handle)
{
	__hal_blockpool_entry_t *entry;
	__hal_blockpool_t *blockpool;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool("devh = 0x"VXGE_OS_STXFMT", size = %d, "
	    "dma_addr = 0x"VXGE_OS_STXFMT", dma_handle = 0x"VXGE_OS_STXFMT", "
	    "acc_handle = 0x"VXGE_OS_STXFMT, (ptr_t) devh, size,
	    (ptr_t) dma_addr, (ptr_t) dma_handle, (ptr_t) acc_handle);

	blockpool = &((__hal_device_t *) devh)->block_pool;

	if (size != blockpool->block_size) {

		vxge_os_dma_unmap(((__hal_device_t *) devh)->header.pdev,
		    *dma_handle,
		    *dma_addr,
		    size,
		    VXGE_OS_DMA_DIR_BIDIRECTIONAL);

		vxge_os_dma_free(((__hal_device_t *) devh)->header.pdev,
		    memblock,
		    size,
		    blockpool->dma_flags |
		    VXGE_OS_DMA_CACHELINE_ALIGNED,
		    dma_handle,
		    acc_handle);
	} else {
#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif

		entry = (__hal_blockpool_entry_t *)
		    vxge_list_first_get(&blockpool->free_entry_list);

		if (entry == NULL) {
			entry = (__hal_blockpool_entry_t *) vxge_os_malloc(
			    ((__hal_device_t *) devh)->header.pdev,
			    sizeof(__hal_blockpool_entry_t));
		} else {
			vxge_list_remove(&entry->item);
		}

		if (entry != NULL) {
			entry->length = size;
			entry->memblock = memblock;
			entry->dma_addr = *dma_addr;
			entry->acc_handle = *acc_handle;
			entry->dma_handle = *dma_handle;
			vxge_list_insert(&entry->item,
			    &blockpool->free_block_list);
			blockpool->pool_size++;
			status = VXGE_HAL_OK;
		} else {
			status = VXGE_HAL_ERR_OUT_OF_MEMORY;
		}

#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif
		if (status == VXGE_HAL_OK)
			__hal_blockpool_blocks_remove(blockpool);

	}

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
}

/*
 * __hal_blockpool_block_allocate - Allocates a block from block pool
 * @hldev: Hal device
 * @size: Size of the block to be allocated
 *
 * This function allocates a block from block pool or from the system
 */
__hal_blockpool_entry_t *
__hal_blockpool_block_allocate(vxge_hal_device_h devh,
    u32 size)
{
	__hal_blockpool_entry_t *entry = NULL;
	__hal_device_t *hldev;
	__hal_blockpool_t *blockpool;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool("devh = 0x"VXGE_OS_STXFMT", size = %d",
	    (ptr_t) devh, size);

	blockpool = &((__hal_device_t *) devh)->block_pool;

	if (size == blockpool->block_size) {
#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif

		entry = (__hal_blockpool_entry_t *)
		    vxge_list_first_get(&blockpool->free_block_list);

		if (entry != NULL) {
			vxge_list_remove(&entry->item);
			blockpool->pool_size--;
		}

#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif
	}

	if (entry != NULL)
		__hal_blockpool_blocks_add(blockpool);


	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, !entry);

	return (entry);
}

/*
 * __hal_blockpool_block_free - Frees a block from block pool
 * @devh: Hal device
 * @entry: Entry of block to be freed
 *
 * This function frees a block from block pool
 */
void
__hal_blockpool_block_free(vxge_hal_device_h devh,
    __hal_blockpool_entry_t *entry)
{
	__hal_device_t *hldev;
	__hal_blockpool_t *blockpool;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool(
	    "devh = 0x"VXGE_OS_STXFMT", entry = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) entry);

	blockpool = &((__hal_device_t *) devh)->block_pool;

	if (entry->length == blockpool->block_size) {
#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif

		vxge_list_insert(&entry->item, &blockpool->free_block_list);
		blockpool->pool_size++;

#if defined(VXGE_HAL_BP_POST)
		vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
		vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif
	}

	__hal_blockpool_blocks_remove(blockpool);

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}


/*
 * __hal_blockpool_list_allocate - Allocate blocks from block pool
 * @devh: Hal device
 * @blocklist: List into which the allocated blocks to be inserted
 * @count: Number of blocks to be allocated
 *
 * This function allocates a register from the register pool
 */
vxge_hal_status_e
__hal_blockpool_list_allocate(
    vxge_hal_device_h devh,
    vxge_list_t *blocklist,
    u32 count)
{
	u32 i;
	__hal_device_t *hldev;
	__hal_blockpool_t *blockpool;
	__hal_blockpool_entry_t *block_entry;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool("devh = 0x"VXGE_OS_STXFMT", blocklist = "
	    "0x"VXGE_OS_STXFMT", count = %d", (ptr_t) devh,
	    (ptr_t) blocklist, count);

	blockpool = &((__hal_device_t *) devh)->block_pool;

	if (blocklist == NULL) {
		vxge_hal_err_log_pool(
		    "null pointer passed blockpool = 0x"VXGE_OS_STXFMT", "
		    "blocklist = 0x"VXGE_OS_STXFMT, (ptr_t) blockpool,
		    (ptr_t) blocklist);
		vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 1",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_FAIL);
	}

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif

	vxge_list_init(blocklist);

	for (i = 0; i < count; i++) {

		block_entry = (__hal_blockpool_entry_t *)
		    vxge_list_first_get(&blockpool->free_block_list);

		if (block_entry == NULL)
			break;

		vxge_list_remove(&block_entry->item);

		vxge_os_memzero(block_entry->memblock, blockpool->block_size);

		vxge_list_insert(&block_entry->item, blocklist);

		blockpool->pool_size++;
	}

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif

	if (i < count) {

		vxge_hal_err_log_pool("%s:%d Blockpool out of blocks",
		    __FILE__, __LINE__);

		vxge_assert(FALSE);

		__hal_blockpool_list_free(blockpool, blocklist);

		status = VXGE_HAL_ERR_OUT_OF_MEMORY;

	}

	__hal_blockpool_blocks_add(blockpool);

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_blockpool_list_free - Free a list of blocks from block pool
 * @devh: Hal device
 * @blocklist: List of blocks to be freed
 *
 * This function frees a list of blocks to the block pool
 */
void
__hal_blockpool_list_free(
    vxge_hal_device_h devh,
    vxge_list_t *blocklist)
{
	__hal_device_t *hldev;
	__hal_blockpool_t *blockpool;
	__hal_blockpool_entry_t *block_entry;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool(
	    "devh = 0x"VXGE_OS_STXFMT", blocklist = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) blocklist);

	blockpool = &((__hal_device_t *) devh)->block_pool;

	if (blocklist == NULL) {
		vxge_hal_err_log_pool(
		    "null pointer passed blockpool = 0x"VXGE_OS_STXFMT", "
		    "blocklist = 0x"VXGE_OS_STXFMT, (ptr_t) blockpool,
		    (ptr_t) blocklist);
		vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 1",
		    __FILE__, __func__, __LINE__);
		return;
	}

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_lock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_lock_irq(&blockpool->pool_lock, flags);
#endif

	while ((block_entry = (__hal_blockpool_entry_t *)
	    vxge_list_first_get(blocklist)) != NULL) {

		vxge_list_remove(&block_entry->item);

		vxge_list_insert(&block_entry->item,
		    &blockpool->free_block_list);

		blockpool->pool_size++;
	}

#if defined(VXGE_HAL_BP_POST)
	vxge_os_spin_unlock(&blockpool->pool_lock);
#elif defined(VXGE_HAL_BP_POST_IRQ)
	vxge_os_spin_unlock_irq(&blockpool->pool_lock, flags);
#endif

	__hal_blockpool_blocks_remove(blockpool);

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}
