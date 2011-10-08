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
 * __hal_ring_block_memblock_idx - Return the memblock index
 * @block: Virtual address of memory block
 *
 * This function returns the index of memory block
 */
static inline u32
__hal_ring_block_memblock_idx(
    vxge_hal_ring_block_t block)
{
	return (u32)*((u64 *) ((void *)((u8 *) block +
	    VXGE_HAL_RING_MEMBLOCK_IDX_OFFSET)));
}

/*
 * __hal_ring_block_memblock_idx_set - Sets the memblock index
 * @block: Virtual address of memory block
 * @memblock_idx: Index of memory block
 *
 * This function sets index to a memory block
 */
static inline void
__hal_ring_block_memblock_idx_set(
    vxge_hal_ring_block_t block,
    u32 memblock_idx)
{
	*((u64 *) ((void *)((u8 *) block +
	    VXGE_HAL_RING_MEMBLOCK_IDX_OFFSET))) = memblock_idx;
}

/*
 * __hal_ring_block_next_pointer - Returns the dma address of next block
 * @block: RxD block
 *
 * Returns the dma address of next block stored in the RxD block
 */
static inline dma_addr_t
/* LINTED */
__hal_ring_block_next_pointer(
    vxge_hal_ring_block_t *block)
{
	return (dma_addr_t)*((u64 *) ((void *)((u8 *) block +
	    VXGE_HAL_RING_NEXT_BLOCK_POINTER_OFFSET)));
}

/*
 * __hal_ring_block_next_pointer_set - Sets the next block pointer in RxD block
 * @block: RxD block
 * @dma_next: dma address of next block
 *
 * Sets the next block pointer in RxD block
 */
static inline void
__hal_ring_block_next_pointer_set(
    vxge_hal_ring_block_t *block,
    dma_addr_t dma_next)
{
	*((u64 *) ((void *)((u8 *) block +
	    VXGE_HAL_RING_NEXT_BLOCK_POINTER_OFFSET))) = dma_next;
}

/*
 * __hal_ring_first_block_address_get - Returns the dma address of the
 *		first block
 * @ringh: Handle to the ring
 *
 * Returns the dma address of the first RxD block
 */
u64
__hal_ring_first_block_address_get(
    vxge_hal_ring_h ringh)
{
	__hal_ring_t *ring = (__hal_ring_t *) ringh;
	vxge_hal_mempool_dma_t *dma_object;

	dma_object = __hal_mempool_memblock_dma(ring->mempool, 0);

	vxge_assert(dma_object != NULL);

	return (dma_object->addr);
}


#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_RXD_STREAMING)
/*
 * __hal_ring_item_dma_offset - Return the dma offset of an item
 * @mempoolh: Handle to the memory pool of the ring
 * @item: Item for which to get the dma offset
 *
 * This function returns the dma offset of a given item
 */
static ptrdiff_t
__hal_ring_item_dma_offset(
    vxge_hal_mempool_h mempoolh,
    void *item)
{
	u32 memblock_idx;
	void *memblock;
	vxge_hal_mempool_t *mempool = (vxge_hal_mempool_t *) mempoolh;
	__hal_device_t *hldev;

	vxge_assert((mempoolh != NULL) && (item != NULL) &&
	    (dma_handle != NULL));

	hldev = (__hal_device_t *) mempool->devh;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "mempoolh = 0x"VXGE_OS_STXFMT", item = 0x"VXGE_OS_STXFMT,
	    (ptr_t) mempoolh, (ptr_t) item);

	/* get owner memblock index */
	memblock_idx = __hal_ring_block_memblock_idx(item);

	/* get owner memblock by memblock index */
	memblock = __hal_mempool_memblock(mempoolh, memblock_idx);

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return ((u8 *) item - (u8 *) memblock);
}
#endif

/*
 * __hal_ring_item_dma_addr - Return the dma address of an item
 * @mempoolh: Handle to the memory pool of the ring
 * @item: Item for which to get the dma offset
 * @dma_handle: dma handle
 *
 * This function returns the dma address of a given item
 */
static dma_addr_t
__hal_ring_item_dma_addr(
    vxge_hal_mempool_h mempoolh,
    void *item,
    pci_dma_h *dma_handle)
{
	u32 memblock_idx;
	void *memblock;
	vxge_hal_mempool_dma_t *memblock_dma_object;
	vxge_hal_mempool_t *mempool = (vxge_hal_mempool_t *) mempoolh;
	__hal_device_t *hldev;
	ptrdiff_t dma_item_offset;

	vxge_assert((mempoolh != NULL) && (item != NULL) &&
	    (dma_handle != NULL));

	hldev = (__hal_device_t *) mempool->devh;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "mempoolh = 0x"VXGE_OS_STXFMT", item = 0x"VXGE_OS_STXFMT", "
	    "dma_handle = 0x"VXGE_OS_STXFMT, (ptr_t) mempoolh,
	    (ptr_t) item, (ptr_t) dma_handle);

	/* get owner memblock index */
	memblock_idx = __hal_ring_block_memblock_idx((u8 *) item);

	/* get owner memblock by memblock index */
	memblock = __hal_mempool_memblock(
	    (vxge_hal_mempool_t *) mempoolh, memblock_idx);

	/* get memblock DMA object by memblock index */
	memblock_dma_object = __hal_mempool_memblock_dma(
	    (vxge_hal_mempool_t *) mempoolh, memblock_idx);

	/* calculate offset in the memblock of this item */
	/* LINTED */
	dma_item_offset = (u8 *) item - (u8 *) memblock;

	*dma_handle = memblock_dma_object->handle;

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (memblock_dma_object->addr + dma_item_offset);
}

/*
 * __hal_ring_rxdblock_link - Link the RxD blocks
 * @mempoolh: Handle to the memory pool of the ring
 * @ring: ring
 * @from: RxD block from which to link
 * @to: RxD block to which to link to
 *
 * This function returns the dma address of a given item
 */
static void
__hal_ring_rxdblock_link(
    vxge_hal_mempool_h mempoolh,
    __hal_ring_t *ring,
    u32 from,
    u32 to)
{
	vxge_hal_ring_block_t *to_item, *from_item;
	dma_addr_t to_dma, from_dma;
	pci_dma_h to_dma_handle, from_dma_handle;
	__hal_device_t *hldev;

	vxge_assert((mempoolh != NULL) && (ring != NULL));

	hldev = (__hal_device_t *) ring->channel.devh;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "mempoolh = 0x"VXGE_OS_STXFMT", ring = 0x"VXGE_OS_STXFMT", "
	    "from = %d, to = %d", (ptr_t) mempoolh, (ptr_t) ring, from, to);

	/* get "from" RxD block */
	from_item = (vxge_hal_ring_block_t *) __hal_mempool_item(
	    (vxge_hal_mempool_t *) mempoolh, from);
	vxge_assert(from_item);

	/* get "to" RxD block */
	to_item = (vxge_hal_ring_block_t *) __hal_mempool_item(
	    (vxge_hal_mempool_t *) mempoolh, to);
	vxge_assert(to_item);

	/* return address of the beginning of previous RxD block */
	to_dma = __hal_ring_item_dma_addr(mempoolh, to_item, &to_dma_handle);

	/*
	 * set next pointer for this RxD block to point on
	 * previous item's DMA start address
	 */
	__hal_ring_block_next_pointer_set(from_item, to_dma);

	/* return "from" RxD block's DMA start address */
	from_dma = __hal_ring_item_dma_addr(
	    mempoolh, from_item, &from_dma_handle);

#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_RXD_STREAMING)
	/* we must sync "from" RxD block, so hardware will see it */
	vxge_os_dma_sync(ring->channel.pdev,
	    from_dma_handle,
	    from_dma + VXGE_HAL_RING_NEXT_BLOCK_POINTER_OFFSET,
	    __hal_ring_item_dma_offset(mempoolh, from_item) +
	    VXGE_HAL_RING_NEXT_BLOCK_POINTER_OFFSET,
	    sizeof(u64),
	    VXGE_OS_DMA_DIR_TODEVICE);
#endif

	vxge_hal_info_log_ring(
	    "block%d:0x"VXGE_OS_STXFMT" => block%d:0x"VXGE_OS_STXFMT,
	    from, (ptr_t) from_dma, to, (ptr_t) to_dma);

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

}

/*
 * __hal_ring_mempool_item_alloc - Allocate List blocks for RxD block callback
 * @mempoolh: Handle to memory pool
 * @memblock: Address of this memory block
 * @memblock_index: Index of this memory block
 * @dma_object: dma object for this block
 * @item: Pointer to this item
 * @index: Index of this item in memory block
 * @is_last: If this is last item in the block
 * @userdata: Specific data of user
 *
 * This function is callback passed to __hal_mempool_create to create memory
 * pool for RxD block
 */
static vxge_hal_status_e
__hal_ring_mempool_item_alloc(
    vxge_hal_mempool_h mempoolh,
    void *memblock,
    u32 memblock_index,
    vxge_hal_mempool_dma_t *dma_object,
    void *item,
    u32 item_index,
    u32 is_last,
    void *userdata)
{
	u32 i;
	__hal_ring_t *ring = (__hal_ring_t *) userdata;
	__hal_device_t *hldev;

	vxge_assert((item != NULL) && (ring != NULL));

	hldev = (__hal_device_t *) ring->channel.devh;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool(
	    "mempoolh = 0x"VXGE_OS_STXFMT", memblock = 0x"VXGE_OS_STXFMT", "
	    "memblock_index = %d, dma_object = 0x"VXGE_OS_STXFMT", "
	    "item = 0x"VXGE_OS_STXFMT", item_index = %d, is_last = %d, "
	    "userdata = 0x"VXGE_OS_STXFMT, (ptr_t) mempoolh, (ptr_t) memblock,
	    memblock_index, (ptr_t) dma_object, (ptr_t) item, item_index, is_last,
	    (ptr_t) userdata);

	/* format rxds array */
	for (i = 0; i < ring->rxds_per_block; i++) {

		void *uld_priv;
		void *rxdblock_priv;
		__hal_ring_rxd_priv_t *rxd_priv;
		vxge_hal_ring_rxd_1_t *rxdp;
		u32 memblock_item_idx;
		u32 dtr_index = item_index * ring->rxds_per_block + i;

		ring->channel.dtr_arr[dtr_index].dtr =
		    ((u8 *) item) + i * ring->rxd_size;

		/*
		 * Note: memblock_item_idx is index of the item within
		 * the memblock. For instance, in case of three RxD-blocks
		 * per memblock this value can be 0, 1 or 2.
		 */
		rxdblock_priv = __hal_mempool_item_priv(
		    (vxge_hal_mempool_t *) mempoolh,
		    memblock_index,
		    item,
		    &memblock_item_idx);

		rxdp = (vxge_hal_ring_rxd_1_t *)
		    ring->channel.dtr_arr[dtr_index].dtr;

		uld_priv = ((u8 *) rxdblock_priv + ring->rxd_priv_size * i);
		rxd_priv =
		    (__hal_ring_rxd_priv_t *) ((void *)(((char *) uld_priv) +
		    ring->per_rxd_space));

		((vxge_hal_ring_rxd_5_t *) rxdp)->host_control = dtr_index;

		ring->channel.dtr_arr[dtr_index].uld_priv = (void *)uld_priv;
		ring->channel.dtr_arr[dtr_index].hal_priv = (void *)rxd_priv;

		/* pre-format per-RxD Ring's private */
		/* LINTED */
		rxd_priv->dma_offset = (u8 *) rxdp - (u8 *) memblock;
		rxd_priv->dma_addr = dma_object->addr + rxd_priv->dma_offset;
		rxd_priv->dma_handle = dma_object->handle;
#if defined(VXGE_DEBUG_ASSERT)
		rxd_priv->dma_object = dma_object;
#endif
		rxd_priv->db_bytes = ring->rxd_size;

		if (i == (ring->rxds_per_block - 1)) {
			rxd_priv->db_bytes +=
			    (((vxge_hal_mempool_t *) mempoolh)->memblock_size -
			    (ring->rxds_per_block * ring->rxd_size));
		}
	}

	__hal_ring_block_memblock_idx_set((u8 *) item, memblock_index);
	if (is_last) {
		/* link last one with first one */
		__hal_ring_rxdblock_link(mempoolh, ring, item_index, 0);
	}

	if (item_index > 0) {
		/* link this RxD block with previous one */
		__hal_ring_rxdblock_link(mempoolh, ring, item_index - 1, item_index);
	}

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * __hal_ring_mempool_item_free - Free RxD blockt callback
 * @mempoolh: Handle to memory pool
 * @memblock: Address of this memory block
 * @memblock_index: Index of this memory block
 * @dma_object: dma object for this block
 * @item: Pointer to this item
 * @index: Index of this item in memory block
 * @is_last: If this is last item in the block
 * @userdata: Specific data of user
 *
 * This function is callback passed to __hal_mempool_free to destroy memory
 * pool for RxD block
 */
static vxge_hal_status_e
__hal_ring_mempool_item_free(
    vxge_hal_mempool_h mempoolh,
    void *memblock,
    u32 memblock_index,
    vxge_hal_mempool_dma_t *dma_object,
    void *item,
    u32 item_index,
    u32 is_last,
    void *userdata)
{
	__hal_ring_t *ring = (__hal_ring_t *) userdata;
	__hal_device_t *hldev;

	vxge_assert((item != NULL) && (ring != NULL));

	hldev = (__hal_device_t *) ring->channel.devh;

	vxge_hal_trace_log_pool("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_pool(
	    "mempoolh = 0x"VXGE_OS_STXFMT", memblock = 0x"VXGE_OS_STXFMT", "
	    "memblock_index = %d, dma_object = 0x"VXGE_OS_STXFMT", "
	    "item = 0x"VXGE_OS_STXFMT", item_index = %d, is_last = %d, "
	    "userdata = 0x"VXGE_OS_STXFMT, (ptr_t) mempoolh, (ptr_t) memblock,
	    memblock_index, (ptr_t) dma_object, (ptr_t) item, item_index, is_last,
	    (ptr_t) userdata);

	vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * __hal_ring_initial_replenish - Initial replenish of RxDs
 * @ring: ring
 * @reopen: Flag to denote if it is open or repopen
 *
 * This function replenishes the RxDs from reserve array to work array
 */
static vxge_hal_status_e
__hal_ring_initial_replenish(
    __hal_ring_t *ring,
    vxge_hal_reopen_e reopen)
{
	vxge_hal_rxd_h rxd;
	void *uld_priv;
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert(ring != NULL);

	hldev = (__hal_device_t *) ring->channel.devh;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring("ring = 0x"VXGE_OS_STXFMT", reopen = %d",
	    (ptr_t) ring, reopen);

	while (vxge_hal_ring_rxd_reserve(ring->channel.vph, &rxd, &uld_priv) ==
	    VXGE_HAL_OK) {

		if (ring->rxd_init) {
			status = ring->rxd_init(ring->channel.vph,
			    rxd,
			    uld_priv,
			    VXGE_HAL_RING_RXD_INDEX(rxd),
			    ring->channel.userdata,
			    reopen);
			if (status != VXGE_HAL_OK) {
				vxge_hal_ring_rxd_free(ring->channel.vph, rxd);
				vxge_hal_trace_log_ring("<== %s:%s:%d \
				    Result: %d",
				    __FILE__, __func__, __LINE__, status);
				return (status);
			}
		}

		vxge_hal_ring_rxd_post(ring->channel.vph, rxd);
	}

	vxge_hal_trace_log_ring("<== %s:%s:%d Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * __hal_ring_create - Create a Ring
 * @vpath_handle: Handle returned by virtual path open
 * @attr: Ring configuration parameters structure
 *
 * This function creates Ring and initializes it.
 *
 */
vxge_hal_status_e
__hal_ring_create(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_ring_attr_t *attr)
{
	vxge_hal_status_e status;
	__hal_ring_t *ring;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	vxge_hal_ring_config_t *config;
	__hal_device_t *hldev;

	vxge_assert((vpath_handle != NULL) && (attr != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", attr = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) attr);

	if ((vpath_handle == NULL) || (attr == NULL)) {
		vxge_hal_err_log_ring("null pointer passed == > %s : %d",
		    __func__, __LINE__);
		vxge_hal_trace_log_ring("<== %s:%s:%d  Result:1",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_FAIL);
	}

	config =
	    &vp->vpath->hldev->header.config.vp_config[vp->vpath->vp_id].ring;

	config->ring_length = ((config->ring_length +
	    vxge_hal_ring_rxds_per_block_get(config->buffer_mode) - 1) /
	    vxge_hal_ring_rxds_per_block_get(config->buffer_mode)) *
	    vxge_hal_ring_rxds_per_block_get(config->buffer_mode);

	ring = (__hal_ring_t *) vxge_hal_channel_allocate(
	    (vxge_hal_device_h) vp->vpath->hldev,
	    vpath_handle,
	    VXGE_HAL_CHANNEL_TYPE_RING,
	    config->ring_length,
	    attr->per_rxd_space,
	    attr->userdata);

	if (ring == NULL) {
		vxge_hal_err_log_ring("Memory allocation failed == > %s : %d",
		    __func__, __LINE__);
		vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	vp->vpath->ringh = (vxge_hal_ring_h) ring;

	ring->stats = &vp->vpath->sw_stats->ring_stats;

	ring->config = config;
	ring->callback = attr->callback;
	ring->rxd_init = attr->rxd_init;
	ring->rxd_term = attr->rxd_term;

	ring->indicate_max_pkts = config->indicate_max_pkts;
	ring->buffer_mode = config->buffer_mode;

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_lock_init(&ring->channel.post_lock, hldev->pdev);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_lock_init_irq(&ring->channel.post_lock, hldev->irqh);
#endif

	ring->rxd_size = vxge_hal_ring_rxd_size_get(config->buffer_mode);
	ring->rxd_priv_size =
	    sizeof(__hal_ring_rxd_priv_t) + attr->per_rxd_space;
	ring->per_rxd_space = attr->per_rxd_space;

	ring->rxd_priv_size =
	    ((ring->rxd_priv_size + __vxge_os_cacheline_size - 1) /
	    __vxge_os_cacheline_size) * __vxge_os_cacheline_size;

	/*
	 * how many RxDs can fit into one block. Depends on configured
	 * buffer_mode.
	 */
	ring->rxds_per_block =
	    vxge_hal_ring_rxds_per_block_get(config->buffer_mode);

	/* calculate actual RxD block private size */
	ring->rxdblock_priv_size = ring->rxd_priv_size * ring->rxds_per_block;

	ring->rxd_mem_avail =
	    ((__hal_vpath_handle_t *) ring->channel.vph)->vpath->rxd_mem_size;

	ring->db_byte_count = 0;

	ring->mempool = vxge_hal_mempool_create(
	    (vxge_hal_device_h) vp->vpath->hldev,
	    VXGE_OS_HOST_PAGE_SIZE,
	    VXGE_OS_HOST_PAGE_SIZE,
	    ring->rxdblock_priv_size,
	    ring->config->ring_length / ring->rxds_per_block,
	    ring->config->ring_length / ring->rxds_per_block,
	    __hal_ring_mempool_item_alloc,
	    __hal_ring_mempool_item_free,
	    ring);

	if (ring->mempool == NULL) {
		__hal_ring_delete(vpath_handle);
		vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	status = vxge_hal_channel_initialize(&ring->channel);
	if (status != VXGE_HAL_OK) {
		__hal_ring_delete(vpath_handle);
		vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}


	/*
	 * Note:
	 * Specifying rxd_init callback means two things:
	 * 1) rxds need to be initialized by ULD at channel-open time;
	 * 2) rxds need to be posted at channel-open time
	 *	(that's what the initial_replenish() below does)
	 * Currently we don't have a case when the 1) is done without the 2).
	 */
	if (ring->rxd_init) {
		if ((status = __hal_ring_initial_replenish(
		    ring,
		    VXGE_HAL_OPEN_NORMAL))
		    != VXGE_HAL_OK) {
			__hal_ring_delete(vpath_handle);
			vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}
	}

	/*
	 * initial replenish will increment the counter in its post() routine,
	 * we have to reset it
	 */
	ring->stats->common_stats.usage_cnt = 0;

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * __hal_ring_abort - Returns the RxD
 * @ringh: Ring to be reset
 * @reopen: See  vxge_hal_reopen_e {}.
 *
 * This function terminates the RxDs of ring
 */
void
__hal_ring_abort(
    vxge_hal_ring_h ringh,
    vxge_hal_reopen_e reopen)
{
	u32 i = 0;
	vxge_hal_rxd_h rxdh;

	__hal_device_t *hldev;
	__hal_ring_t *ring = (__hal_ring_t *) ringh;

	vxge_assert(ringh != NULL);

	hldev = (__hal_device_t *) ring->channel.devh;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring("ring = 0x"VXGE_OS_STXFMT", reopen = %d",
	    (ptr_t) ringh, reopen);

	if (ring->rxd_term) {
		__hal_channel_for_each_dtr(&ring->channel, rxdh, i) {
			if (!__hal_channel_is_posted_dtr(&ring->channel, i)) {
				ring->rxd_term(ring->channel.vph, rxdh,
				    VXGE_HAL_RING_ULD_PRIV(ring, rxdh),
				    VXGE_HAL_RXD_STATE_FREED,
				    ring->channel.userdata,
				    reopen);
			}
		}
	}

	for (;;) {
		__hal_channel_dtr_try_complete(&ring->channel, &rxdh);
		if (rxdh == NULL)
			break;

		__hal_channel_dtr_complete(&ring->channel);
		if (ring->rxd_term) {
			ring->rxd_term(ring->channel.vph, rxdh,
			    VXGE_HAL_RING_ULD_PRIV(ring, rxdh),
			    VXGE_HAL_RXD_STATE_POSTED,
			    ring->channel.userdata,
			    reopen);
		}
		__hal_channel_dtr_free(&ring->channel,
		    VXGE_HAL_RING_RXD_INDEX(rxdh));
	}

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_ring_reset - Resets the ring
 * @ringh: Ring to be reset
 *
 * This function resets the ring during vpath reset operation
 */
vxge_hal_status_e
__hal_ring_reset(
    vxge_hal_ring_h ringh)
{
	__hal_ring_t *ring = (__hal_ring_t *) ringh;
	__hal_device_t *hldev;
	vxge_hal_status_e status;
	__hal_vpath_handle_t *vph = (__hal_vpath_handle_t *) ring->channel.vph;

	vxge_assert(ringh != NULL);

	hldev = (__hal_device_t *) ring->channel.devh;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring("ring = 0x"VXGE_OS_STXFMT,
	    (ptr_t) ringh);

	__hal_ring_abort(ringh, VXGE_HAL_RESET_ONLY);

	status = __hal_channel_reset(&ring->channel);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);

	}
	ring->rxd_mem_avail = vph->vpath->rxd_mem_size;
	ring->db_byte_count = 0;


	if (ring->rxd_init) {
		if ((status = __hal_ring_initial_replenish(
		    ring,
		    VXGE_HAL_RESET_ONLY))
		    != VXGE_HAL_OK) {
			vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}
	}

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * __hal_ring_delete - Removes the ring
 * @vpath_handle: Virtual path handle to which this queue belongs
 *
 * This function freeup the memory pool and removes the ring
 */
void
__hal_ring_delete(
    vxge_hal_vpath_h vpath_handle)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	__hal_ring_t *ring;

	vxge_assert(vpath_handle != NULL);

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	ring = (__hal_ring_t *) vp->vpath->ringh;

	vxge_assert(ring != NULL);

	vxge_assert(ring->channel.pdev);

	__hal_ring_abort(vp->vpath->ringh, VXGE_HAL_OPEN_NORMAL);


	if (ring->mempool) {
		vxge_hal_mempool_destroy(ring->mempool);
	}

	vxge_hal_channel_terminate(&ring->channel);

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_lock_destroy(&ring->channel.post_lock, hldev->pdev);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_lock_destroy_irq(&ring->channel.post_lock, hldev->pdev);
#endif

	vxge_hal_channel_free(&ring->channel);

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

}

/*
 * __hal_ring_frame_length_set	- Set the maximum frame length of recv frames.
 * @vpath: virtual Path
 * @new_frmlen: New frame length
 *
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_OUT_OF_DESCRIPTORS - Currently no descriptors available.
 *
 */
vxge_hal_status_e
__hal_ring_frame_length_set(
    __hal_virtualpath_t *vpath,
    u32 new_frmlen)
{
	u64 val64;
	__hal_device_t *hldev;

	vxge_assert(vpath != NULL);

	hldev = (__hal_device_t *) vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath = 0x"VXGE_OS_STXFMT", new_frmlen = %d",
	    (ptr_t) vpath, new_frmlen);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {

		vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);

	}

	val64 = vxge_os_pio_mem_read64(
	    vpath->hldev->header.pdev,
	    vpath->hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 &= ~VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(0x3fff);

	if (vpath->vp_config->ring.max_frm_len !=
	    VXGE_HAL_MAX_RING_FRM_LEN_USE_MTU) {

		val64 |= VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(
		    vpath->vp_config->ring.max_frm_len +
		    VXGE_HAL_MAC_HEADER_MAX_SIZE);

	} else {

		val64 |= VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(new_frmlen +
		    VXGE_HAL_MAC_HEADER_MAX_SIZE);
	}

	vxge_os_pio_mem_write64(
	    vpath->hldev->header.pdev,
	    vpath->hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_ring_rxd_reserve - Reserve ring descriptor.
 * @vpath_handle: virtual Path handle.
 * @rxdh: Reserved descriptor. On success HAL fills this "out" parameter
 *		with a valid handle.
 * @rxd_priv: Buffer to return pointer to per rxd private space
 *
 * Reserve Rx descriptor for the subsequent filling-in (by upper layer
 * driver (ULD)) and posting on the corresponding channel (@channelh)
 * via vxge_hal_ring_rxd_post().
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_OUT_OF_DESCRIPTORS - Currently no descriptors available.
 *
 */
vxge_hal_status_e
vxge_hal_ring_rxd_reserve(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h * rxdh,
    void **rxd_priv)
{
	vxge_hal_status_e status;
#if defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	unsigned long flags;
#endif
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	__hal_ring_t *ring;

	vxge_assert((vpath_handle != NULL) && (rxdh != NULL) &&
	    (rxd_priv != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", rxdh = 0x"VXGE_OS_STXFMT", "
	    "rxd_priv = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle,
	    (ptr_t) rxdh, (ptr_t) rxd_priv);

	ring = (__hal_ring_t *) vp->vpath->ringh;

	vxge_assert(ring != NULL);

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_lock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&ring->channel.post_lock, flags);
#endif

	status = __hal_channel_dtr_reserve(&ring->channel, rxdh);

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_unlock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&ring->channel.post_lock, flags);
#endif

	if (status == VXGE_HAL_OK) {
		vxge_hal_ring_rxd_1_t *rxdp = (vxge_hal_ring_rxd_1_t *)*rxdh;

		/* instead of memset: reset	this RxD */
		rxdp->control_0 = rxdp->control_1 = 0;

		*rxd_priv = VXGE_HAL_RING_ULD_PRIV(ring, rxdp);

#if defined(VXGE_OS_MEMORY_CHECK)
		VXGE_HAL_RING_HAL_PRIV(ring, rxdp)->allocated = 1;
#endif
	}

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (status);
}

/*
 * vxge_hal_ring_rxd_pre_post - Prepare rxd and post
 * @vpath_handle: virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * This routine prepares a rxd and posts
 */
void
vxge_hal_ring_rxd_pre_post(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh)
{

#if defined(VXGE_DEBUG_ASSERT)
	vxge_hal_ring_rxd_1_t *rxdp = (vxge_hal_ring_rxd_1_t *) rxdh;

#endif

#if defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	unsigned long flags;

#endif
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	__hal_ring_t *ring;

	vxge_assert((vpath_handle != NULL) && (rxdh != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", rxdh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) rxdh);

	ring = (__hal_ring_t *) vp->vpath->ringh;

	vxge_assert(ring != NULL);

#if defined(VXGE_DEBUG_ASSERT)
	/* make	sure device overwrites the (illegal) t_code on completion */
	rxdp->control_0 |=
	    VXGE_HAL_RING_RXD_T_CODE(VXGE_HAL_RING_RXD_T_CODE_UNUSED);
#endif

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_lock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&ring->channel.post_lock, flags);
#endif

#if defined(VXGE_DEBUG_ASSERT) && defined(VXGE_HAL_RING_ENFORCE_ORDER)
	if (TRUE) {
		if (VXGE_HAL_RING_RXD_INDEX(rxdp) != 0) {
			vxge_hal_rxd_h prev_rxdh;
			__hal_ring_rxd_priv_t *rxdp_priv;
			u32 index;

			rxdp_priv = VXGE_HAL_RING_HAL_PRIV(ring, rxdp);

			if (VXGE_HAL_RING_RXD_INDEX(rxdp) == 0)
				index = ring->channel.length;
			else
				index = VXGE_HAL_RING_RXD_INDEX(rxdp) - 1;

			prev_rxdh = ring->channel.dtr_arr[index].dtr;

			if (prev_rxdh != NULL &&
			    (rxdp_priv->dma_offset & (~0xFFF)) !=
			    rxdp_priv->dma_offset) {
				vxge_assert((char *) prev_rxdh +
				    ring->rxd_size == rxdh);
			}
		}
	}
#endif

	__hal_channel_dtr_post(&ring->channel, VXGE_HAL_RING_RXD_INDEX(rxdh));

	ring->db_byte_count +=
	    VXGE_HAL_RING_HAL_PRIV(ring, rxdh)->db_bytes;

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_unlock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&ring->channel.post_lock, flags);
#endif

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_ring_rxd_post_post - Process rxd after post.
 * @vpath_handle: virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * Processes rxd after post
 */
void
vxge_hal_ring_rxd_post_post(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh)
{
	vxge_hal_ring_rxd_1_t *rxdp = (vxge_hal_ring_rxd_1_t *) rxdh;

#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_RXD_STREAMING)
	__hal_ring_rxd_priv_t *priv;

#endif
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	__hal_ring_t *ring;

	vxge_assert((vpath_handle != NULL) && (rxdh != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", rxdh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) rxdh);

	ring = (__hal_ring_t *) vp->vpath->ringh;

	vxge_assert(ring != NULL);

	/* do POST */
	rxdp->control_0 |= VXGE_HAL_RING_RXD_LIST_OWN_ADAPTER;

	rxdp->control_1 |= VXGE_HAL_RING_RXD_LIST_TAIL_OWN_ADAPTER;

#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_RXD_STREAMING)
	priv = __hal_ring_rxd_priv(ring, rxdp);
	vxge_os_dma_sync(ring->channel.pdev,
	    priv->dma_handle,
	    priv->dma_addr,
	    priv->dma_offset,
	    ring->rxd_size,
	    VXGE_OS_DMA_DIR_TODEVICE);
#endif
	if (ring->stats->common_stats.usage_cnt > 0)
		ring->stats->common_stats.usage_cnt--;

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_ring_rxd_post - Post descriptor on the ring.
 * @vpath_handle: virtual Path handle.
 * @rxdh: Descriptor obtained via vxge_hal_ring_rxd_reserve().
 *
 * Post	descriptor on the ring.
 * Prior to posting the	descriptor should be filled in accordance with
 * Host/X3100 interface specification for a given service (LL, etc.).
 *
 */
void
vxge_hal_ring_rxd_post(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh)
{
	vxge_hal_ring_rxd_1_t *rxdp = (vxge_hal_ring_rxd_1_t *) rxdh;

#if defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	unsigned long flags;
#endif

	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	__hal_ring_t *ring;

	vxge_assert((vpath_handle != NULL) && (rxdh != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", rxdh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) rxdh);

	ring = (__hal_ring_t *) vp->vpath->ringh;

	vxge_assert(ring != NULL);

	/* Based on Titan HW bugzilla # 3039, we need to reset the tcode */
	rxdp->control_0 = 0;

#if defined(VXGE_DEBUG_ASSERT)
	/* make	sure device overwrites the (illegal) t_code on completion */
	rxdp->control_0 |=
	    VXGE_HAL_RING_RXD_T_CODE(VXGE_HAL_RING_RXD_T_CODE_UNUSED);
#endif

	rxdp->control_1 |= VXGE_HAL_RING_RXD_LIST_TAIL_OWN_ADAPTER;
	rxdp->control_0 |= VXGE_HAL_RING_RXD_LIST_OWN_ADAPTER;

#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_RXD_STREAMING)
	{
		__hal_ring_rxd_priv_t *rxdp_temp1;
		rxdp_temp1 = VXGE_HAL_RING_HAL_PRIV(ring, rxdp);
		vxge_os_dma_sync(ring->channel.pdev,
		    rxdp_temp1->dma_handle,
		    rxdp_temp1->dma_addr,
		    rxdp_temp1->dma_offset,
		    ring->rxd_size,
		    VXGE_OS_DMA_DIR_TODEVICE);
	}
#endif

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_lock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&ring->channel.post_lock, flags);
#endif

#if defined(VXGE_DEBUG_ASSERT) && defined(VXGE_HAL_RING_ENFORCE_ORDER)
	if (TRUE) {
		if (VXGE_HAL_RING_RXD_INDEX(rxdp) != 0) {

			vxge_hal_rxd_h prev_rxdh;
			__hal_ring_rxd_priv_t *rxdp_temp2;

			rxdp_temp2 = VXGE_HAL_RING_HAL_PRIV(ring, rxdp);
			prev_rxdh =
			    ring->channel.dtr_arr[VXGE_HAL_RING_RXD_INDEX(rxdp) - 1].dtr;

			if (prev_rxdh != NULL &&
			    (rxdp_temp2->dma_offset & (~0xFFF)) != rxdp_temp2->dma_offset)
				vxge_assert((char *) prev_rxdh + ring->rxd_size == rxdh);
		}
	}
#endif

	__hal_channel_dtr_post(&ring->channel, VXGE_HAL_RING_RXD_INDEX(rxdh));

	ring->db_byte_count +=
	    VXGE_HAL_RING_HAL_PRIV(ring, rxdp)->db_bytes;

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_unlock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&ring->channel.post_lock, flags);
#endif

	if (ring->stats->common_stats.usage_cnt > 0)
		ring->stats->common_stats.usage_cnt--;

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_ring_rxd_post_post_wmb - Process rxd after post with memory barrier
 * @vpath_handle: virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * Processes rxd after post with memory barrier.
 */
void
vxge_hal_ring_rxd_post_post_wmb(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;

	vxge_assert((vpath_handle != NULL) && (rxdh != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", rxdh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) rxdh);

	/* Do memory barrier before changing the ownership */
	vxge_os_wmb();

	vxge_hal_ring_rxd_post_post(vpath_handle, rxdh);

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_ring_rxd_post_post_db - Post Doorbell after posting the rxd(s).
 * @vpath_handle: virtual Path handle.
 *
 * Post Doorbell after posting the rxd(s).
 */
void
vxge_hal_ring_rxd_post_post_db(
    vxge_hal_vpath_h vpath_handle)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	__hal_ring_t *ring;

	vxge_assert(vpath_handle != NULL);

	hldev = (__hal_device_t *) vp->vpath->hldev;

	ring = (__hal_ring_t *) vp->vpath->ringh;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_lock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&ring->channel.post_lock, flags);
#endif

	if (ring->db_byte_count <= ring->rxd_mem_avail) {
		__hal_rxd_db_post(vpath_handle, ring->db_byte_count);
		ring->rxd_mem_avail -= ring->db_byte_count;
		ring->db_byte_count = 0;
	} else {
		__hal_rxd_db_post(vpath_handle, ring->rxd_mem_avail);
		ring->db_byte_count -= ring->rxd_mem_avail;
		ring->rxd_mem_avail = 0;
	}

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_unlock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&ring->channel.post_lock, flags);
#endif

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_ring_is_next_rxd_completed - Check if the next rxd is completed
 * @vpath_handle: Virtual Path handle.
 *
 * Checks if the the _next_	completed descriptor is	in host	memory
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS - No completed	descriptors
 * are currently available for processing.
 */
vxge_hal_status_e
vxge_hal_ring_is_next_rxd_completed(
    vxge_hal_vpath_h vpath_handle)
{
	__hal_ring_t *ring;
	vxge_hal_rxd_h rxdh;
	vxge_hal_ring_rxd_1_t *rxdp;	/* doesn't matter 1, 3 or 5... */
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	ring = (__hal_ring_t *) vp->vpath->ringh;

	vxge_assert(ring != NULL);

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_lock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&ring->channel.post_lock, flags);
#endif

	__hal_channel_dtr_try_complete(&ring->channel, &rxdh);

	rxdp = (vxge_hal_ring_rxd_1_t *) rxdh;

	if (rxdp != NULL) {

		/* check whether it is not the end */
		if ((!(rxdp->control_0 & VXGE_HAL_RING_RXD_LIST_OWN_ADAPTER)) &&
		    (!(rxdp->control_1 &
		    VXGE_HAL_RING_RXD_LIST_TAIL_OWN_ADAPTER))) {

			status = VXGE_HAL_OK;
		}
	}

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_unlock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&ring->channel.post_lock, flags);
#endif

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_ring_rxd_next_completed - Get the _next_ completed descriptor.
 * @channelh: Channel handle.
 * @rxdh: Descriptor handle. Returned by HAL.
 * @rxd_priv: Buffer to return a pointer to the per rxd space allocated
 * @t_code:	Transfer code, as per X3100 User Guide,
 *			Receive	Descriptor Format. Returned	by HAL.
 *
 * Retrieve the	_next_ completed descriptor.
 * HAL uses ring callback (*vxge_hal_ring_callback_f) to notifiy
 * upper-layer driver (ULD) of new completed descriptors. After that
 * the ULD can use vxge_hal_ring_rxd_next_completed to retrieve the rest
 * completions (the very first completion is passed by HAL via
 * vxge_hal_ring_callback_f).
 *
 * Implementation-wise,	the upper-layer	driver is free to call
 * vxge_hal_ring_rxd_next_completed either immediately from inside the
 * ring callback, or in a deferred fashion and separate (from HAL)
 * context.
 *
 * Non-zero @t_code means failure to fill-in receive buffer(s)
 * of the descriptor.
 * For instance, parity	error detected during the data transfer.
 * In this case	X3100 will	complete the descriptor	and	indicate
 * for the host	that the received data is not to be	used.
 * For details please refer	to X3100 User Guide.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS - No completed	descriptors
 * are currently available for processing.
 *
 * See also: vxge_hal_ring_callback_f {},
 * vxge_hal_fifo_rxd_next_completed(), vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_ring_rxd_next_completed(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h *rxdh,
    void **rxd_priv,
    u8 *t_code)
{
	__hal_ring_t *ring;
	vxge_hal_ring_rxd_5_t *rxdp;	/* doesn't matter 1, 3 or 5... */
#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_RXD_STREAMING)
	__hal_ring_rxd_priv_t *priv;
#endif
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	u64 own, control_0, control_1;

	vxge_assert((vpath_handle != NULL) && (rxdh != NULL) &&
	    (rxd_priv != NULL) && (t_code != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", rxdh = 0x"VXGE_OS_STXFMT", "
	    "rxd_priv = 0x"VXGE_OS_STXFMT", t_code = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) rxdh, (ptr_t) rxd_priv,
	    (ptr_t) t_code);

	ring = (__hal_ring_t *) vp->vpath->ringh;

	vxge_assert(ring != NULL);

	*rxdh = 0;
	*rxd_priv = NULL;

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_lock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&ring->channel.post_lock, flags);
#endif

	__hal_channel_dtr_try_complete(&ring->channel, rxdh);

	rxdp = (vxge_hal_ring_rxd_5_t *)*rxdh;
	if (rxdp != NULL) {

#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_RXD_STREAMING)
		/*
		 * Note: 24 bytes at most means:
		 *	- Control_3 in case of 5-buffer	mode
		 *	- Control_1 and	Control_2
		 *
		 * This is the only length needs to be invalidated
		 * type of channels.
		 */
		priv = __hal_ring_rxd_priv(ring, rxdp);
		vxge_os_dma_sync(ring->channel.pdev,
		    priv->dma_handle,
		    priv->dma_addr,
		    priv->dma_offset,
		    24,
		    VXGE_OS_DMA_DIR_FROMDEVICE);
#endif
		*t_code = (u8) VXGE_HAL_RING_RXD_T_CODE_GET(rxdp->control_0);

		control_0 = rxdp->control_0;
		control_1 = rxdp->control_1;
		own = control_0 & VXGE_HAL_RING_RXD_LIST_OWN_ADAPTER;

		/* check whether it is not the end */
		if ((!own && !(control_1 & VXGE_HAL_RING_RXD_LIST_TAIL_OWN_ADAPTER)) ||
		    (*t_code == VXGE_HAL_RING_RXD_T_CODE_FRM_DROP)) {

#ifndef	VXGE_HAL_IRQ_POLLING
			if (++ring->cmpl_cnt > ring->indicate_max_pkts) {
				/*
				 * reset it. since we don't want to return
				 * garbage to the ULD
				 */
				*rxdh = 0;
				status = VXGE_HAL_COMPLETIONS_REMAIN;
			} else {
#endif
				__hal_channel_dtr_complete(&ring->channel);

				*rxd_priv = VXGE_HAL_RING_ULD_PRIV(ring, rxdp);

				ring->rxd_mem_avail +=
				    (VXGE_HAL_RING_HAL_PRIV(ring, rxdp))->db_bytes;

				ring->stats->common_stats.usage_cnt++;
				if (ring->stats->common_stats.usage_max <
				    ring->stats->common_stats.usage_cnt)
					ring->stats->common_stats.usage_max =
					    ring->stats->common_stats.usage_cnt;

				switch (ring->buffer_mode) {
				case VXGE_HAL_RING_RXD_BUFFER_MODE_1:
					ring->channel.poll_bytes +=
					    (u32) VXGE_HAL_RING_RXD_1_BUFFER0_SIZE_GET(
					    rxdp->control_1);
					break;
				case VXGE_HAL_RING_RXD_BUFFER_MODE_3:
					ring->channel.poll_bytes +=
					    (u32) VXGE_HAL_RING_RXD_3_BUFFER0_SIZE_GET(
					    rxdp->control_1) +
					    (u32) VXGE_HAL_RING_RXD_3_BUFFER1_SIZE_GET(
					    rxdp->control_1) +
					    (u32) VXGE_HAL_RING_RXD_3_BUFFER2_SIZE_GET(
					    rxdp->control_1);
					break;
				case VXGE_HAL_RING_RXD_BUFFER_MODE_5:
					ring->channel.poll_bytes +=
					    (u32) VXGE_HAL_RING_RXD_5_BUFFER0_SIZE_GET(
					    rxdp->control_1) +
					    (u32) VXGE_HAL_RING_RXD_5_BUFFER1_SIZE_GET(
					    rxdp->control_1) +
					    (u32) VXGE_HAL_RING_RXD_5_BUFFER2_SIZE_GET(
					    rxdp->control_1) +
					    (u32) VXGE_HAL_RING_RXD_5_BUFFER3_SIZE_GET(
					    rxdp->control_2) +
					    (u32) VXGE_HAL_RING_RXD_5_BUFFER4_SIZE_GET(
					    rxdp->control_2);
					break;
				}

				status = VXGE_HAL_OK;
#ifndef	VXGE_HAL_IRQ_POLLING
			}
#endif
		}
	}

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_unlock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&ring->channel.post_lock, flags);
#endif

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}


/*
 * vxge_hal_ring_handle_tcode - Handle transfer code.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 * @t_code: One of the enumerated (and documented in the X3100 user guide)
 *	 "transfer codes".
 *
 * Handle descriptor's transfer code. The latter comes with each completed
 * descriptor.
 *
 * Returns: one of the vxge_hal_status_e {} enumerated types.
 * VXGE_HAL_OK			- for success.
 * VXGE_HAL_ERR_CRITICAL	- when encounters critical error.
 */
vxge_hal_status_e
vxge_hal_ring_handle_tcode(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    u8 t_code)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (rxdh != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "rxdh = 0x"VXGE_OS_STXFMT", t_code = 0x%d",
	    (ptr_t) vpath_handle, (ptr_t) rxdh, t_code);

	switch (t_code) {
	case 0x0:
		/* 0x0: Transfer ok. */
		break;
	case 0x1:
		/*
		 * 0x1: Layer 3 checksum presentation
		 *	configuration mismatch.
		 */
		break;
	case 0x2:
		/*
		 * 0x2: Layer 4 checksum presentation
		 *	configuration mismatch.
		 */
		break;
	case 0x3:
		/*
		 * 0x3: Layer 3 and Layer 4 checksum
		 *	presentation configuration mismatch.
		 */
		break;
	case 0x4:
		/* 0x4: Reserved. */
		break;
	case 0x5:
		/*
		 * 0x5: Layer 3 error unparseable packet,
		 *	such as unknown IPv6 header.
		 */
		break;
	case 0x6:
		/*
		 * 0x6: Layer 2 error frame integrity
		 *	error, such as FCS or ECC).
		 */
		break;
	case 0x7:
		/*
		 * 0x7: Buffer size error the RxD buffer(s)
		 *	were not appropriately sized and
		 *	data loss occurred.
		 */
		break;
	case 0x8:
		/* 0x8: Internal ECC error RxD corrupted. */
		__hal_device_handle_error(vp->vpath->hldev,
		    vp->vpath->vp_id, VXGE_HAL_EVENT_ECCERR);
		break;
	case 0x9:
		/*
		 * 0x9: Benign overflow the contents of
		 *	Segment1 exceeded the capacity of
		 *	Buffer1 and the remainder was placed
		 *	in Buffer2. Segment2 now starts in
		 *	Buffer3. No data loss or errors occurred.
		 */
		break;
	case 0xA:
		/*
		 * 0xA: Buffer size 0 one of the RxDs
		 *	assigned buffers has a size of 0 bytes.
		 */
		break;
	case 0xB:
		/* 0xB: Reserved. */
		break;
	case 0xC:
		/*
		 * 0xC: Frame dropped either due to
		 *	VPath Reset or because of a VPIN mismatch.
		 */
		break;
	case 0xD:
		/* 0xD: Reserved. */
		break;
	case 0xE:
		/* 0xE: Reserved. */
		break;
	case 0xF:
		/*
		 * 0xF: Multiple errors more than one
		 *	transfer code condition occurred.
		 */
		break;
	default:
		vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_TCODE);
		return (VXGE_HAL_ERR_INVALID_TCODE);
	}

	vp->vpath->sw_stats->ring_stats.rxd_t_code_err_cnt[t_code]++;

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, VXGE_HAL_OK);
	return (VXGE_HAL_OK);
}


/*
 * vxge_hal_ring_rxd_private_get - Get ULD private per-descriptor data.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * Returns: private ULD	info associated	with the descriptor.
 * ULD requests	per-descriptor space via vxge_hal_ring_attr.
 *
 */
void *
vxge_hal_ring_rxd_private_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	return (VXGE_HAL_RING_ULD_PRIV(
	    ((__hal_ring_t *) vp->vpath->ringh), rxdh));

}

/*
 * vxge_hal_ring_rxd_free - Free descriptor.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * Free	the reserved descriptor. This operation is "symmetrical" to
 * vxge_hal_ring_rxd_reserve. The "free-ing" completes the descriptor's
 * lifecycle.
 *
 * After free-ing (see vxge_hal_ring_rxd_free()) the descriptor again can
 * be:
 *
 * - reserved (vxge_hal_ring_rxd_reserve);
 *
 * - posted	(vxge_hal_ring_rxd_post);
 *
 * - completed (vxge_hal_ring_rxd_next_completed);
 *
 * - and recycled again	(vxge_hal_ring_rxd_free).
 *
 * For alternative state transitions and more details please refer to
 * the design doc.
 *
 */
void
vxge_hal_ring_rxd_free(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh)
{
#if defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	unsigned long flags;

#endif
	__hal_ring_t *ring;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (rxdh != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_ring(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", rxdh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) rxdh);

	ring = (__hal_ring_t *) vp->vpath->ringh;

	vxge_assert(ring != NULL);

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_lock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&ring->channel.post_lock, flags);
#endif

	__hal_channel_dtr_free(&ring->channel, VXGE_HAL_RING_RXD_INDEX(rxdh));
#if defined(VXGE_OS_MEMORY_CHECK)
	VXGE_HAL_RING_HAL_PRIV(ring, rxdh)->allocated = 0;
#endif

#if defined(VXGE_HAL_RX_MULTI_POST)
	vxge_os_spin_unlock(&ring->channel.post_lock);
#elif defined(VXGE_HAL_RX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&ring->channel.post_lock, flags);
#endif

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}
