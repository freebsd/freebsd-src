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
 * __hal_fifo_mempool_item_alloc - Allocate List blocks for TxD list callback
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
 * pool for TxD list
 */
static vxge_hal_status_e
__hal_fifo_mempool_item_alloc(
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
	void *block_priv;
	u32 memblock_item_idx;

	__hal_fifo_t *fifo = (__hal_fifo_t *) userdata;

	vxge_assert(fifo != NULL);
	vxge_assert(item);

#if (VXGE_COMPONENT_HAL_POOL & VXGE_DEBUG_MODULE_MASK)
	{
		__hal_device_t *hldev = (__hal_device_t *) fifo->channel.devh;

		vxge_hal_trace_log_pool("==> %s:%s:%d",
		    __FILE__, __func__, __LINE__);

		vxge_hal_trace_log_pool(
		    "mempoolh = 0x"VXGE_OS_STXFMT", "
		    "memblock = 0x"VXGE_OS_STXFMT", memblock_index = %d, "
		    "dma_object = 0x"VXGE_OS_STXFMT", \
		    item = 0x"VXGE_OS_STXFMT", "
		    "item_index = %d, is_last = %d, userdata = 0x"VXGE_OS_STXFMT,
		    (ptr_t) mempoolh, (ptr_t) memblock, memblock_index,
		    (ptr_t) dma_object, (ptr_t) item, item_index, is_last,
		    (ptr_t) userdata);
	}
#endif

	block_priv = __hal_mempool_item_priv((vxge_hal_mempool_t *) mempoolh,
	    memblock_index, item, &memblock_item_idx);

	vxge_assert(block_priv != NULL);

	for (i = 0; i < fifo->txdl_per_memblock; i++) {

		__hal_fifo_txdl_priv_t *txdl_priv;
		vxge_hal_fifo_txd_t *txdp;

		int dtr_index = item_index * fifo->txdl_per_memblock + i;

		txdp = (vxge_hal_fifo_txd_t *) ((void *)
		    ((char *) item + i * fifo->txdl_size));

		txdp->host_control = dtr_index;

		fifo->channel.dtr_arr[dtr_index].dtr = txdp;

		fifo->channel.dtr_arr[dtr_index].uld_priv = (void *)
		    ((char *) block_priv + fifo->txdl_priv_size * i);

		fifo->channel.dtr_arr[dtr_index].hal_priv = (void *)
		    (((char *) fifo->channel.dtr_arr[dtr_index].uld_priv) +
		    fifo->per_txdl_space);

		txdl_priv = (__hal_fifo_txdl_priv_t *)
		    fifo->channel.dtr_arr[dtr_index].hal_priv;

		vxge_assert(txdl_priv);

		/* pre-format HAL's TxDL's private */
		/* LINTED */
		txdl_priv->dma_offset = (char *) txdp - (char *) memblock;
		txdl_priv->dma_addr = dma_object->addr + txdl_priv->dma_offset;
		txdl_priv->dma_handle = dma_object->handle;
		txdl_priv->memblock = memblock;
		txdl_priv->first_txdp = (vxge_hal_fifo_txd_t *) txdp;
		txdl_priv->next_txdl_priv = NULL;
		txdl_priv->dang_txdl = NULL;
		txdl_priv->dang_frags = 0;
		txdl_priv->alloc_frags = 0;

#if defined(VXGE_DEBUG_ASSERT)
		txdl_priv->dma_object = dma_object;
#endif

#if defined(VXGE_HAL_ALIGN_XMIT)
		txdl_priv->align_vaddr = NULL;
		txdl_priv->align_dma_addr = (dma_addr_t) 0;

#ifndef	VXGE_HAL_ALIGN_XMIT_ALLOC_RT
		/* CONSTCOND */
		if (TRUE) {
			vxge_hal_status_e status;

			if (fifo->config->alignment_size) {
				status = __hal_fifo_txdl_align_alloc_map(fifo,
				    txdp);
				if (status != VXGE_HAL_OK) {

#if (VXGE_COMPONENT_HAL_POOL & VXGE_DEBUG_MODULE_MASK)
					__hal_device_t *hldev;
					hldev = (__hal_device_t *)
					    fifo->channel.devh;

					vxge_hal_err_log_pool(
					    "align buffer[%d] %d bytes, \
					    status %d",
					    (item_index * fifo->txdl_per_memblock + i),
					    fifo->align_size, status);

					vxge_hal_trace_log_pool(
					    "<== %s:%s:%d  Result: 0",
					    __FILE__, __func__, __LINE__);
#endif
					return (status);
				}
			}
		}
#endif
#endif
		if (fifo->txdl_init) {
			fifo->txdl_init(fifo->channel.vph,
			    (vxge_hal_txdl_h) txdp,
			    VXGE_HAL_FIFO_ULD_PRIV(fifo, txdp),
			    VXGE_HAL_FIFO_TXDL_INDEX(txdp),
			    fifo->channel.userdata, VXGE_HAL_OPEN_NORMAL);
		}
	}

#if (VXGE_COMPONENT_HAL_POOL & VXGE_DEBUG_MODULE_MASK)
	{
		__hal_device_t *hldev = (__hal_device_t *) fifo->channel.devh;

		vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 0",
		    __FILE__, __func__, __LINE__);
	}
#endif

	return (VXGE_HAL_OK);
}


/*
 * __hal_fifo_mempool_item_free - Free List blocks for TxD list callback
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
 * pool for TxD list
 */
static vxge_hal_status_e
__hal_fifo_mempool_item_free(
    vxge_hal_mempool_h mempoolh,
    void *memblock,
    u32 memblock_index,
    vxge_hal_mempool_dma_t *dma_object,
    void *item,
    u32 item_index,
    u32 is_last,
    void *userdata)
{
	vxge_assert(item);

#if (VXGE_COMPONENT_HAL_POOL & VXGE_DEBUG_MODULE_MASK)
	{
		__hal_fifo_t *fifo = (__hal_fifo_t *) userdata;

		vxge_assert(fifo != NULL);

		__hal_device_t *hldev = (__hal_device_t *) fifo->channel.devh;

		vxge_hal_trace_log_pool("==> %s:%s:%d",
		    __FILE__, __func__, __LINE__);

		vxge_hal_trace_log_pool("mempoolh = 0x"VXGE_OS_STXFMT", "
		    "memblock = 0x"VXGE_OS_STXFMT", memblock_index = %d, "
		    "dma_object = 0x"VXGE_OS_STXFMT", \
		    item = 0x"VXGE_OS_STXFMT", "
		    "item_index = %d, is_last = %d, userdata = 0x"VXGE_OS_STXFMT,
		    (ptr_t) mempoolh, (ptr_t) memblock, memblock_index,
		    (ptr_t) dma_object, (ptr_t) item, item_index, is_last,
		    (ptr_t) userdata);
	}
#endif

#if defined(VXGE_HAL_ALIGN_XMIT)
	{
		__hal_fifo_t *fifo = (__hal_fifo_t *) userdata;

		vxge_assert(fifo != NULL);
		if (fifo->config->alignment_size) {

			int i;
			vxge_hal_fifo_txd_t *txdp;

			for (i = 0; i < fifo->txdl_per_memblock; i++) {
				txdp = (void *)
				    ((char *) item + i * fifo->txdl_size);
				__hal_fifo_txdl_align_free_unmap(fifo, txdp);
			}
		}
	}
#endif

#if (VXGE_COMPONENT_HAL_POOL & VXGE_DEBUG_MODULE_MASK)
	{
		__hal_fifo_t *fifo = (__hal_fifo_t *) userdata;

		vxge_assert(fifo != NULL);

		__hal_device_t *hldev = (__hal_device_t *) fifo->channel.devh;

		vxge_hal_trace_log_pool("<== %s:%s:%d  Result: 0",
		    __FILE__, __func__, __LINE__);
	}
#endif

	return (VXGE_HAL_OK);
}

/*
 * __hal_fifo_create - Create a FIFO
 * @vpath_handle: Handle returned by virtual path open
 * @attr: FIFO configuration parameters structure
 *
 * This function creates FIFO and initializes it.
 *
 */
vxge_hal_status_e
__hal_fifo_create(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_fifo_attr_t *attr)
{
	vxge_hal_status_e status;
	__hal_fifo_t *fifo;
	vxge_hal_fifo_config_t *config;
	u32 txdl_size, memblock_size, txdl_per_memblock;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;

	vxge_assert((vpath_handle != NULL) && (attr != NULL));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", attr = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) attr);

	if ((vpath_handle == NULL) || (attr == NULL)) {
		vxge_hal_err_log_fifo("null pointer passed == > %s : %d",
		    __func__, __LINE__);
		vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	config =
	    &vp->vpath->hldev->header.config.vp_config[vp->vpath->vp_id].fifo;

	txdl_size = config->max_frags * sizeof(vxge_hal_fifo_txd_t);

	if (txdl_size <= VXGE_OS_HOST_PAGE_SIZE)
		memblock_size = VXGE_OS_HOST_PAGE_SIZE;
	else
		memblock_size = txdl_size;

	txdl_per_memblock = memblock_size / txdl_size;

	config->fifo_length = ((config->fifo_length + txdl_per_memblock - 1) /
	    txdl_per_memblock) * txdl_per_memblock;

	fifo = (__hal_fifo_t *) vxge_hal_channel_allocate(
	    (vxge_hal_device_h) vp->vpath->hldev,
	    vpath_handle,
	    VXGE_HAL_CHANNEL_TYPE_FIFO,
	    config->fifo_length,
	    attr->per_txdl_space,
	    attr->userdata);

	if (fifo == NULL) {
		vxge_hal_err_log_fifo("Memory allocation failed == > %s : %d",
		    __func__, __LINE__);
		vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	vp->vpath->fifoh = fifo;

	fifo->stats = &vp->vpath->sw_stats->fifo_stats;

	fifo->config = config;

	fifo->memblock_size = memblock_size;

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_lock_init(&fifo->channel.post_lock,
	    vp->vpath->hldev->header.pdev);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_lock_init_irq(&fifo->channel.post_lock,
	    vp->vpath->hldev->header.irqh);
#endif

	fifo->align_size =
	    fifo->config->alignment_size * fifo->config->max_aligned_frags;

	/* apply "interrupts per txdl" attribute */
	fifo->interrupt_type = VXGE_HAL_FIFO_TXD_INT_TYPE_UTILZ;
	if (fifo->config->intr) {
		fifo->interrupt_type = VXGE_HAL_FIFO_TXD_INT_TYPE_PER_LIST;
	}

	fifo->no_snoop_bits = config->no_snoop_bits;

	/*
	 * FIFO memory management strategy:
	 *
	 * TxDL splitted into three independent parts:
	 *	- set of TxD's
	 *	- TxD HAL private part
	 *	- upper layer private part
	 *
	 * Adaptative memory allocation used. i.e. Memory allocated on
	 * demand with the size which will fit into one memory block.
	 * One memory block may contain more than one TxDL. In simple case
	 * memory block size can be equal to CPU page size. On more
	 * sophisticated OS's memory block can be contigious across
	 * several pages.
	 *
	 * During "reserve" operations more memory can be allocated on demand
	 * for example due to FIFO full condition.
	 *
	 * Pool of memory memblocks never shrinks except __hal_fifo_close
	 * routine which will essentially stop channel and free the resources.
	 */

	/* TxDL common private size == TxDL private + ULD private */
	fifo->txdl_priv_size =
	    sizeof(__hal_fifo_txdl_priv_t) + attr->per_txdl_space;
	fifo->txdl_priv_size =
	    ((fifo->txdl_priv_size + __vxge_os_cacheline_size - 1) /
	    __vxge_os_cacheline_size) * __vxge_os_cacheline_size;

	fifo->per_txdl_space = attr->per_txdl_space;

	/* recompute txdl size to be cacheline aligned */
	fifo->txdl_size = txdl_size;
	fifo->txdl_per_memblock = txdl_per_memblock;

	/*
	 * since txdl_init() callback will be called from item_alloc(),
	 * the same way channels userdata might be used prior to
	 * channel_initialize()
	 */
	fifo->txdl_init = attr->txdl_init;
	fifo->txdl_term = attr->txdl_term;
	fifo->callback = attr->callback;

	if (fifo->txdl_per_memblock == 0) {
		__hal_fifo_delete(vpath_handle);
		vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_BLOCK_SIZE);
		return (VXGE_HAL_ERR_INVALID_BLOCK_SIZE);
	}

	/* calculate actual TxDL block private size */
	fifo->txdlblock_priv_size =
	    fifo->txdl_priv_size * fifo->txdl_per_memblock;

	fifo->mempool =
	    vxge_hal_mempool_create((vxge_hal_device_h) vp->vpath->hldev,
	    fifo->memblock_size,
	    fifo->memblock_size,
	    fifo->txdlblock_priv_size,
	    fifo->config->fifo_length /
	    fifo->txdl_per_memblock,
	    fifo->config->fifo_length /
	    fifo->txdl_per_memblock,
	    __hal_fifo_mempool_item_alloc,
	    __hal_fifo_mempool_item_free,
	    fifo);

	if (fifo->mempool == NULL) {
		__hal_fifo_delete(vpath_handle);
		vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	status = vxge_hal_channel_initialize(&fifo->channel);
	if (status != VXGE_HAL_OK) {
		__hal_fifo_delete(vpath_handle);
		vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * __hal_fifo_abort - Returns the TxD
 * @fifoh: Fifo to be reset
 * @reopen: See  vxge_hal_reopen_e {}.
 *
 * This function terminates the TxDs of fifo
 */
void
__hal_fifo_abort(
    vxge_hal_fifo_h fifoh,
    vxge_hal_reopen_e reopen)
{
	u32 i = 0;
	__hal_fifo_t *fifo = (__hal_fifo_t *) fifoh;
	__hal_device_t *hldev;
	vxge_hal_txdl_h txdlh;

	vxge_assert(fifoh != NULL);

	hldev = (__hal_device_t *) fifo->channel.devh;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("fifo = 0x"VXGE_OS_STXFMT", reopen = %d",
	    (ptr_t) fifoh, reopen);

	if (fifo->txdl_term) {
		__hal_channel_for_each_dtr(&fifo->channel, txdlh, i) {
			if (!__hal_channel_is_posted_dtr(&fifo->channel,
			    i)) {
				fifo->txdl_term(fifo->channel.vph, txdlh,
				    VXGE_HAL_FIFO_ULD_PRIV(fifo, txdlh),
				    VXGE_HAL_TXDL_STATE_FREED,
				    fifo->channel.userdata,
				    reopen);
			}
		}
	}

	for (;;) {
		__hal_channel_dtr_try_complete(&fifo->channel, &txdlh);

		if (txdlh == NULL)
			break;

		__hal_channel_dtr_complete(&fifo->channel);

		if (fifo->txdl_term) {
			fifo->txdl_term(fifo->channel.vph, txdlh,
			    VXGE_HAL_FIFO_ULD_PRIV(fifo, txdlh),
			    VXGE_HAL_TXDL_STATE_POSTED,
			    fifo->channel.userdata,
			    reopen);
		}

		__hal_channel_dtr_free(&fifo->channel,
		    VXGE_HAL_FIFO_TXDL_INDEX(txdlh));
	}

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_fifo_reset - Resets the fifo
 * @fifoh: Fifo to be reset
 *
 * This function resets the fifo during vpath reset operation
 */
vxge_hal_status_e
__hal_fifo_reset(
    vxge_hal_fifo_h fifoh)
{
	vxge_hal_status_e status;
	__hal_device_t *hldev;
	__hal_fifo_t *fifo = (__hal_fifo_t *) fifoh;

	vxge_assert(fifoh != NULL);

	hldev = (__hal_device_t *) fifo->channel.devh;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("fifo = 0x"VXGE_OS_STXFMT,
	    (ptr_t) fifoh);

	__hal_fifo_abort(fifoh, VXGE_HAL_RESET_ONLY);

	status = __hal_channel_reset(&fifo->channel);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);

	}

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_fifo_doorbell_reset - Resets the doorbell fifo
 * @vapth_handle: Vpath Handle
 *
 * This function resets the doorbell fifo during if fifo error occurs
 */
vxge_hal_status_e
vxge_hal_fifo_doorbell_reset(
    vxge_hal_vpath_h vpath_handle)
{
	u32 i;
	vxge_hal_txdl_h txdlh;
	__hal_fifo_t *fifo;
	__hal_virtualpath_t *vpath;
	__hal_fifo_txdl_priv_t *txdl_priv;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vpath = ((__hal_vpath_handle_t *) fifo->channel.vph)->vpath;

	status = __hal_non_offload_db_reset(fifo->channel.vph);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
		    __FILE__, __func__, __LINE__);
		return (status);
	}

	__hal_channel_for_each_posted_dtr(&fifo->channel, txdlh, i) {

		txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdlh);

		__hal_non_offload_db_post(fifo->channel.vph,
		    ((VXGE_HAL_FIFO_TXD_NO_BW_LIMIT_GET(
		    ((vxge_hal_fifo_txd_t *) txdlh)->control_1)) ?
		    (((u64) txdl_priv->dma_addr) | 0x1) :
		    (u64) txdl_priv->dma_addr),
		    txdl_priv->frags - 1,
		    vpath->vp_config->fifo.no_snoop_bits);
	}

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (status);
}

/*
 * __hal_fifo_delete - Removes the FIFO
 * @vpath_handle: Virtual path handle to which this queue belongs
 *
 * This function freeup the memory pool and removes the FIFO
 */
void
__hal_fifo_delete(
    vxge_hal_vpath_h vpath_handle)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

	if (fifo->mempool) {
		__hal_fifo_abort(vp->vpath->fifoh, VXGE_HAL_OPEN_NORMAL);
		vxge_hal_mempool_destroy(fifo->mempool);
	}

	vxge_hal_channel_terminate(&fifo->channel);

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_lock_destroy(&fifo->channel.post_lock,
	    vp->vpath->hldev->header.pdev);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_lock_destroy_irq(&fifo->channel.post_lock,
	    vp->vpath->hldev->header.pdev);
#endif

	vxge_hal_channel_free(&fifo->channel);

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

#if defined(VXGE_HAL_ALIGN_XMIT)
/*
 * __hal_fifo_txdl_align_free_unmap - Unmap the alignement buffers
 * @fifo: Fifo
 * @txdp: txdl
 *
 * This function unmaps dma memory for the alignment buffers
 */
void
__hal_fifo_txdl_align_free_unmap(
    __hal_fifo_t *fifo,
    vxge_hal_fifo_txd_t *txdp)
{
	__hal_device_t *hldev;
	__hal_fifo_txdl_priv_t *txdl_priv;

	vxge_assert((fifo != NULL) && (txdp != NULL));

	hldev = (__hal_device_t *) fifo->channel.devh;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo(
	    "fifo = 0x"VXGE_OS_STXFMT",  txdp = 0x"VXGE_OS_STXFMT,
	    (ptr_t) fifo, (ptr_t) txdp);

	txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdp);

	if (txdl_priv->align_vaddr != NULL) {
		__hal_blockpool_free(fifo->channel.devh,
		    txdl_priv->align_vaddr,
		    fifo->align_size,
		    &txdl_priv->align_dma_addr,
		    &txdl_priv->align_dma_handle,
		    &txdl_priv->align_dma_acch);

		txdl_priv->align_vaddr = NULL;
		txdl_priv->align_dma_addr = 0;
	}

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_fifo_txdl_align_alloc_map - Maps the alignement buffers
 * @fifo: Fifo
 * @txdp: txdl
 *
 * This function maps dma memory for the alignment buffers
 */
vxge_hal_status_e
__hal_fifo_txdl_align_alloc_map(
    __hal_fifo_t *fifo,
    vxge_hal_fifo_txd_t *txdp)
{
	__hal_device_t *hldev;
	__hal_fifo_txdl_priv_t *txdl_priv;

	vxge_assert((fifo != NULL) && (txdp != NULL));

	hldev = (__hal_device_t *) fifo->channel.devh;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo(
	    "fifo = 0x"VXGE_OS_STXFMT",  txdp = 0x"VXGE_OS_STXFMT,
	    (ptr_t) fifo, (ptr_t) txdp);

	txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdp);

	/* allocate alignment DMA-buffer */
	txdl_priv->align_vaddr =
	    (u8 *) __hal_blockpool_malloc(fifo->channel.devh,
	    fifo->align_size,
	    &txdl_priv->align_dma_addr,
	    &txdl_priv->align_dma_handle,
	    &txdl_priv->align_dma_acch);
	if (txdl_priv->align_vaddr == NULL) {
		vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}
#endif
/*
 * vxge_hal_fifo_free_txdl_count_get - returns the number of txdls
 *                               available in the fifo
 * @vpath_handle: Virtual path handle.
 */
u32
vxge_hal_fifo_free_txdl_count_get(vxge_hal_vpath_h vpath_handle)
{
	return __hal_channel_free_dtr_count(&((__hal_fifo_t *)
	    ((__hal_vpath_handle_t *) vpath_handle)->vpath->fifoh)->channel);
}

/*
 * vxge_hal_fifo_txdl_private_get - Retrieve per-descriptor private data.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 *
 * Retrieve per-descriptor private data.
 * Note that ULD requests per-descriptor space via
 * vxge_hal_fifo_attr_t passed to
 * vxge_hal_vpath_open().
 *
 * Returns: private ULD data associated with the descriptor.
 */
void *
vxge_hal_fifo_txdl_private_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh)
{
	return (VXGE_HAL_FIFO_ULD_PRIV(((__hal_fifo_t *)
	    ((__hal_vpath_handle_t *) vpath_handle)->vpath->fifoh), txdlh));
}

/*
 * vxge_hal_fifo_txdl_reserve - Reserve fifo descriptor.
 * @vapth_handle: virtual path handle.
 * @txdlh: Reserved descriptor. On success HAL fills this "out" parameter
 *	with a valid handle.
 * @txdl_priv: Buffer to return the pointer to per txdl space
 *
 * Reserve a single TxDL (that is, fifo descriptor)
 * for the subsequent filling-in by upper layerdriver (ULD))
 * and posting on the corresponding channel (@channelh)
 * via vxge_hal_fifo_txdl_post().
 *
 * Note: it is the responsibility of ULD to reserve multiple descriptors
 * for lengthy (e.g., LSO) transmit operation. A single fifo descriptor
 * carries up to configured number (fifo.max_frags) of contiguous buffers.
 *
 * Returns: VXGE_HAL_OK - success;
 * VXGE_HAL_INF_OUT_OF_DESCRIPTORS - Currently no descriptors available
 *
 */
vxge_hal_status_e
vxge_hal_fifo_txdl_reserve(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h *txdlh,
    void **txdl_priv)
{
	u32 i;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	__hal_fifo_t *fifo;
	vxge_hal_status_e status;

#if defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	unsigned long flags = 0;

#endif

	vxge_assert((vpath_handle != NULL) && (txdlh != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo(
	    "vpath_handle = 0x"VXGE_OS_STXFMT",  txdlh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) txdlh);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_lock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&fifo->channel.post_lock, flags);
#endif

	status = __hal_channel_dtr_reserve(&fifo->channel, txdlh);

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_unlock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&fifo->channel.post_lock, flags);
#endif

	if (status == VXGE_HAL_OK) {
		vxge_hal_fifo_txd_t *txdp = (vxge_hal_fifo_txd_t *)*txdlh;
		__hal_fifo_txdl_priv_t *priv;

		priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdp);

		/* reset the TxDL's private */
		priv->align_dma_offset = 0;
		priv->align_vaddr_start = priv->align_vaddr;
		priv->align_used_frags = 0;
		priv->frags = 0;
		priv->alloc_frags = fifo->config->max_frags;
		priv->dang_txdl = NULL;
		priv->dang_frags = 0;
		priv->next_txdl_priv = NULL;
		priv->bytes_sent = 0;

		*txdl_priv = VXGE_HAL_FIFO_ULD_PRIV(fifo, txdp);

		for (i = 0; i < fifo->config->max_frags; i++) {
			txdp = ((vxge_hal_fifo_txd_t *)*txdlh) + i;
			txdp->control_0 = txdp->control_1 = 0;
		}

#if defined(VXGE_OS_MEMORY_CHECK)
		priv->allocated = 1;
#endif
	}

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (status);
}

/*
 * vxge_hal_fifo_txdl_buffer_set - Set transmit buffer pointer in the
 * descriptor.
 * @vpath_handle: virtual path handle.
 * @txdlh: Descriptor handle.
 * @frag_idx: Index of the data buffer in the caller's scatter-gather list
 *	   (of buffers).
 * @dma_pointer: DMA address of the data buffer referenced by @frag_idx.
 * @size: Size of the data buffer (in bytes).
 *
 * This API is part of the preparation of the transmit descriptor for posting
 * (via vxge_hal_fifo_txdl_post()). The related "preparation" APIs include
 * vxge_hal_fifo_txdl_mss_set() and vxge_hal_fifo_txdl_cksum_set_bits().
 * All three APIs fill in the fields of the fifo descriptor,
 * in accordance with the X3100 specification.
 *
 */
void
vxge_hal_fifo_txdl_buffer_set(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 frag_idx,
    dma_addr_t dma_pointer,
    unsigned long size)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	__hal_fifo_txdl_priv_t *txdl_priv;
	vxge_hal_fifo_txd_t *txdp;

	vxge_assert((vpath_handle != NULL) && (txdlh != NULL) &&
	    (dma_pointer != 0) && (size != 0));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "txdlh = 0x"VXGE_OS_STXFMT", frag_idx = %d, "
	    "dma_pointer = 0x"VXGE_OS_LLXFMT", size = %lu",
	    (ptr_t) vpath_handle, (ptr_t) txdlh,
	    frag_idx, (u64) dma_pointer, size);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

	txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdlh);

	txdp = (vxge_hal_fifo_txd_t *) txdlh + txdl_priv->frags;

	/*
	 * Note:
	 * it is the responsibility of upper layers and not HAL
	 * detect it and skip zero-size fragment
	 */
	vxge_assert(size > 0);
	vxge_assert(frag_idx < txdl_priv->alloc_frags);

	txdp->buffer_pointer = (u64) dma_pointer;
	txdp->control_0 |= VXGE_HAL_FIFO_TXD_BUFFER_SIZE(size);
	txdl_priv->bytes_sent += size;
	fifo->stats->total_buffers++;
	txdl_priv->frags++;

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_fifo_txdl_buffer_set_aligned - Align transmit buffer and fill
 * in fifo descriptor.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 * @frag_idx: Index of the data buffer in the caller's scatter-gather list
 *	   (of buffers).
 * @vaddr: Virtual address of the data buffer.
 * @dma_pointer: DMA address of the data buffer referenced by @frag_idx.
 * @size: Size of the data buffer (in bytes).
 * @misaligned_size: Size (in bytes) of the misaligned portion of the
 * data buffer. Calculated by the caller, based on the platform/OS/other
 * specific criteria, which is outside of HAL's domain. See notes below.
 *
 * This API is part of the transmit descriptor preparation for posting
 * (via vxge_hal_fifo_txdl_post()). The related "preparation" APIs include
 * vxge_hal_fifo_txdl_mss_set() and vxge_hal_fifo_txdl_cksum_set_bits().
 * All three APIs fill in the fields of the fifo descriptor,
 * in accordance with the X3100 specification.
 * On the PCI-X based systems aligning transmit data typically provides better
 * transmit performance. The typical alignment granularity: L2 cacheline size.
 * However, HAL does not make assumptions in terms of the alignment granularity;
 * this is specified via additional @misaligned_size parameter described above.
 * Prior to calling vxge_hal_fifo_txdl_buffer_set_aligned(),
 * ULD is supposed to check alignment of a given fragment/buffer. For this HAL
 * provides a separate vxge_hal_check_alignment() API sufficient to cover
 * most (but not all) possible alignment criteria.
 * If the buffer appears to be aligned, the ULD calls
 * vxge_hal_fifo_txdl_buffer_set().
 * Otherwise, ULD calls vxge_hal_fifo_txdl_buffer_set_aligned().
 *
 * Note; This API is a "superset" of vxge_hal_fifo_txdl_buffer_set(). In
 * addition to filling in the specified descriptor it aligns transmit data on
 * the specified boundary.
 * Note: Decision on whether to align or not to align a given contiguous
 * transmit buffer is outside of HAL's domain. To this end ULD can use any
 * programmable criteria, which can help to 1) boost transmit performance,
 * and/or 2) provide a workaround for PCI bridge bugs, if any.
 *
 */
vxge_hal_status_e
vxge_hal_fifo_txdl_buffer_set_aligned(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 frag_idx,
    void *vaddr,
    dma_addr_t dma_pointer,
    u32 size,
    u32 misaligned_size)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	__hal_fifo_txdl_priv_t *txdl_priv;
	vxge_hal_fifo_txd_t *txdp;
	int remaining_size;
	ptrdiff_t prev_boff;

	vxge_assert((vpath_handle != NULL) && (txdlh != NULL) &&
	    (vaddr != NULL) && (dma_pointer != 0) &&
	    (size != 0) && (misaligned_size != 0));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", txdlh = 0x"VXGE_OS_STXFMT", "
	    "frag_idx = %d, vaddr = 0x"VXGE_OS_STXFMT", "
	    "dma_pointer = 0x"VXGE_OS_LLXFMT", size = %d, "
	    "misaligned_size = %d", (ptr_t) vpath_handle,
	    (ptr_t) txdlh, frag_idx, (ptr_t) vaddr, (u64) dma_pointer, size,
	    misaligned_size);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

	txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdlh);

	txdp = (vxge_hal_fifo_txd_t *) txdlh + txdl_priv->frags;

	/*
	 * On some systems buffer size could be zero.
	 * It is the responsibility of ULD and *not HAL* to
	 * detect it and skip it.
	 */
	vxge_assert(size > 0);
	vxge_assert(frag_idx < txdl_priv->alloc_frags);
	vxge_assert(misaligned_size != 0 &&
	    misaligned_size <= fifo->config->alignment_size);

	remaining_size = size - misaligned_size;
	vxge_assert(remaining_size >= 0);

	vxge_os_memcpy((char *) txdl_priv->align_vaddr_start,
	    vaddr, misaligned_size);

	if (txdl_priv->align_used_frags >= fifo->config->max_aligned_frags) {
		return (VXGE_HAL_ERR_OUT_ALIGNED_FRAGS);
	}

	/* setup new buffer */
	/* LINTED */
	prev_boff = txdl_priv->align_vaddr_start - txdl_priv->align_vaddr;
	txdp->buffer_pointer = (u64) txdl_priv->align_dma_addr + prev_boff;
	txdp->control_0 |= VXGE_HAL_FIFO_TXD_BUFFER_SIZE(misaligned_size);
	txdl_priv->bytes_sent += misaligned_size;
	fifo->stats->total_buffers++;
	txdl_priv->frags++;
	txdl_priv->align_used_frags++;
	txdl_priv->align_vaddr_start += fifo->config->alignment_size;
	txdl_priv->align_dma_offset = 0;

#if defined(VXGE_OS_DMA_REQUIRES_SYNC)
	/* sync new buffer */
	vxge_os_dma_sync(fifo->channel.pdev,
	    txdl_priv->align_dma_handle,
	    txdp->buffer_pointer,
	    0,
	    misaligned_size,
	    VXGE_OS_DMA_DIR_TODEVICE);
#endif

	if (remaining_size) {
		vxge_assert(frag_idx < txdl_priv->alloc_frags);
		txdp++;
		txdp->buffer_pointer = (u64) dma_pointer + misaligned_size;
		txdp->control_0 |=
		    VXGE_HAL_FIFO_TXD_BUFFER_SIZE(remaining_size);
		txdl_priv->bytes_sent += remaining_size;
		fifo->stats->total_buffers++;
		txdl_priv->frags++;
	}

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_fifo_txdl_buffer_append - Append the contents of virtually
 *		contiguous data buffer to a single physically contiguous buffer.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 * @vaddr: Virtual address of the data buffer.
 * @size: Size of the data buffer (in bytes).
 *
 * This API is part of the transmit descriptor preparation for posting
 * (via vxge_hal_fifo_txdl_post()).
 * The main difference of this API wrt to the APIs
 * vxge_hal_fifo_txdl_buffer_set_aligned() is that this API appends the
 * contents of virtually contiguous data buffers received from
 * upper layer into a single physically contiguous data buffer and the
 * device will do a DMA from this buffer.
 *
 * See Also: vxge_hal_fifo_txdl_buffer_finalize(),
 * vxge_hal_fifo_txdl_buffer_set(),
 * vxge_hal_fifo_txdl_buffer_set_aligned().
 */
vxge_hal_status_e
vxge_hal_fifo_txdl_buffer_append(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    void *vaddr,
    u32 size)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	__hal_fifo_txdl_priv_t *txdl_priv;
	ptrdiff_t used;

	vxge_assert((vpath_handle != NULL) && (txdlh != NULL) &&
	    (vaddr != NULL) && (size == 0));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "txdlh = 0x"VXGE_OS_STXFMT", vaddr = 0x"VXGE_OS_STXFMT", "
	    "size = %d", (ptr_t) vpath_handle, (ptr_t) txdlh,
	    (ptr_t) vaddr, size);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

	txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdlh);

	/* LINTED */
	used = txdl_priv->align_vaddr_start - txdl_priv->align_vaddr;
	used += txdl_priv->align_dma_offset;

	if (used + (unsigned int)size > (unsigned int)fifo->align_size)
		return (VXGE_HAL_ERR_OUT_ALIGNED_FRAGS);

	vxge_os_memcpy((char *) txdl_priv->align_vaddr_start +
	    txdl_priv->align_dma_offset, vaddr, size);

	fifo->stats->copied_frags++;

	txdl_priv->align_dma_offset += size;

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_fifo_txdl_buffer_finalize - Prepares a descriptor that contains the
 * single physically contiguous buffer.
 *
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 * @frag_idx: Index of the data buffer in the Txdl list.
 *
 * This API in conjuction with vxge_hal_fifo_txdl_buffer_append() prepares
 * a descriptor that consists of a single physically contiguous buffer
 * which inturn contains the contents of one or more virtually contiguous
 * buffers received from the upper layer.
 *
 * See Also: vxge_hal_fifo_txdl_buffer_append().
 */
void
vxge_hal_fifo_txdl_buffer_finalize(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 frag_idx)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	__hal_fifo_txdl_priv_t *txdl_priv;
	vxge_hal_fifo_txd_t *txdp;
	ptrdiff_t prev_boff;

	vxge_assert((vpath_handle != NULL) &&
	    (txdlh != NULL) && (frag_idx != 0));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "txdlh = 0x"VXGE_OS_STXFMT", frag_idx = %d", (ptr_t) vpath_handle,
	    (ptr_t) txdlh, frag_idx);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

	txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdlh);
	txdp = (vxge_hal_fifo_txd_t *) txdlh + txdl_priv->frags;

	/* LINTED */
	prev_boff = txdl_priv->align_vaddr_start - txdl_priv->align_vaddr;
	txdp->buffer_pointer = (u64) txdl_priv->align_dma_addr + prev_boff;
	txdp->control_0 |=
	    VXGE_HAL_FIFO_TXD_BUFFER_SIZE(txdl_priv->align_dma_offset);
	txdl_priv->bytes_sent += (unsigned int)txdl_priv->align_dma_offset;
	fifo->stats->total_buffers++;
	fifo->stats->copied_buffers++;
	txdl_priv->frags++;
	txdl_priv->align_used_frags++;

#if defined(VXGE_OS_DMA_REQUIRES_SYNC)
	/* sync pre-mapped buffer */
	vxge_os_dma_sync(fifo->channel.pdev,
	    txdl_priv->align_dma_handle,
	    txdp->buffer_pointer,
	    0,
	    txdl_priv->align_dma_offset,
	    VXGE_OS_DMA_DIR_TODEVICE);
#endif

	/* increment vaddr_start for the next buffer_append() iteration */
	txdl_priv->align_vaddr_start += txdl_priv->align_dma_offset;
	txdl_priv->align_dma_offset = 0;

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_fifo_txdl_new_frame_set - Start the new packet by setting TXDL flags
 * @vpath_handle: virtual path handle.
 * @txdlh: Descriptor handle.
 * @tagged: Is the frame tagged
 *
 * This API is part of the preparation of the transmit descriptor for posting
 * (via vxge_hal_fifo_txdl_post()). This api is used to mark the end of previous
 * frame and start of a new frame.
 *
 */
void
vxge_hal_fifo_txdl_new_frame_set(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 tagged)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	__hal_fifo_txdl_priv_t *txdl_priv;
	vxge_hal_fifo_txd_t *txdp;

	vxge_assert((vpath_handle != NULL) && (txdlh != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "txdlh = 0x"VXGE_OS_STXFMT", tagged = %d",
	    (ptr_t) vpath_handle, (ptr_t) txdlh, tagged);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

	txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdlh);

	txdp = (vxge_hal_fifo_txd_t *) txdlh + txdl_priv->frags;

	txdp->control_0 |=
	    VXGE_HAL_FIFO_TXD_HOST_STEER(vp->vpath->vp_config->wire_port);
	txdp->control_0 |= VXGE_HAL_FIFO_TXD_GATHER_CODE(
	    VXGE_HAL_FIFO_TXD_GATHER_CODE_FIRST);
	txdp->control_1 |= fifo->interrupt_type;
	txdp->control_1 |= VXGE_HAL_FIFO_TXD_INT_NUMBER(
	    vp->vpath->tx_intr_num);
	if (tagged)
		txdp->control_1 |= VXGE_HAL_FIFO_TXD_NO_BW_LIMIT;
	if (txdl_priv->frags) {

		txdp = (vxge_hal_fifo_txd_t *) txdlh + (txdl_priv->frags - 1);

		txdp->control_0 |= VXGE_HAL_FIFO_TXD_GATHER_CODE(
		    VXGE_HAL_FIFO_TXD_GATHER_CODE_LAST);

	}

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_fifo_txdl_post - Post descriptor on the fifo channel.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor obtained via vxge_hal_fifo_txdl_reserve()
 * @tagged: Is the frame tagged
 *
 * Post descriptor on the 'fifo' type channel for transmission.
 * Prior to posting the descriptor should be filled in accordance with
 * Host/X3100 interface specification for a given service (LL, etc.).
 *
 */
void
vxge_hal_fifo_txdl_post(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 tagged)
{
	u64 list_ptr;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	__hal_fifo_txdl_priv_t *txdl_priv;
	vxge_hal_fifo_txd_t *txdp_last;
	vxge_hal_fifo_txd_t *txdp_first;

#if defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	unsigned long flags = 0;

#endif

	vxge_assert((vpath_handle != NULL) && (txdlh != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "txdlh = 0x"VXGE_OS_STXFMT", tagged = %d",
	    (ptr_t) vpath_handle, (ptr_t) txdlh, tagged);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

	txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdlh);

	txdp_first = (vxge_hal_fifo_txd_t *) txdlh;
	txdp_first->control_0 |=
	    VXGE_HAL_FIFO_TXD_HOST_STEER(vp->vpath->vp_config->wire_port);
	txdp_first->control_0 |=
	    VXGE_HAL_FIFO_TXD_GATHER_CODE(VXGE_HAL_FIFO_TXD_GATHER_CODE_FIRST);
	txdp_first->control_1 |=
	    VXGE_HAL_FIFO_TXD_INT_NUMBER(vp->vpath->tx_intr_num);
	txdp_first->control_1 |= fifo->interrupt_type;
	list_ptr = (u64) txdl_priv->dma_addr;
	if (tagged) {
		txdp_first->control_1 |= VXGE_HAL_FIFO_TXD_NO_BW_LIMIT;
		list_ptr |= 0x1;
	}

	txdp_last =
	    (vxge_hal_fifo_txd_t *) txdlh + (txdl_priv->frags - 1);
	txdp_last->control_0 |=
	    VXGE_HAL_FIFO_TXD_GATHER_CODE(VXGE_HAL_FIFO_TXD_GATHER_CODE_LAST);

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_lock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&fifo->channel.post_lock, flags);
#endif

	txdp_first->control_0 |= VXGE_HAL_FIFO_TXD_LIST_OWN_ADAPTER;

#if defined(VXGE_DEBUG_ASSERT)
	/* make sure device overwrites the t_code value on completion */
	txdp_first->control_0 |=
	    VXGE_HAL_FIFO_TXD_T_CODE(VXGE_HAL_FIFO_TXD_T_CODE_UNUSED);
#endif

#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_TXDL_STREAMING)
	/* sync the TxDL to device */
	vxge_os_dma_sync(fifo->channel.pdev,
	    txdl_priv->dma_handle,
	    txdl_priv->dma_addr,
	    txdl_priv->dma_offset,
	    txdl_priv->frags << 5, /* sizeof(vxge_hal_fifo_txd_t) */
	    VXGE_OS_DMA_DIR_TODEVICE);
#endif
	/*
	 * we want touch dtr_arr in order with ownership bit set to HW
	 */
	__hal_channel_dtr_post(&fifo->channel, VXGE_HAL_FIFO_TXDL_INDEX(txdlh));

	__hal_non_offload_db_post(vpath_handle,
	    list_ptr,
	    txdl_priv->frags - 1,
	    vp->vpath->vp_config->fifo.no_snoop_bits);

#if defined(VXGE_HAL_FIFO_DUMP_TXD)
	vxge_hal_info_log_fifo(
	    ""VXGE_OS_LLXFMT":"VXGE_OS_LLXFMT":"VXGE_OS_LLXFMT":"
	    VXGE_OS_LLXFMT" dma "VXGE_OS_LLXFMT,
	    txdp_first->control_0, txdp_first->control_1,
	    txdp_first->buffer_pointer, VXGE_HAL_FIFO_TXDL_INDEX(txdp_first),
	    txdl_priv->dma_addr);
#endif

	fifo->stats->total_posts++;
	fifo->stats->common_stats.usage_cnt++;
	if (fifo->stats->common_stats.usage_max <
	    fifo->stats->common_stats.usage_cnt)
		fifo->stats->common_stats.usage_max =
		    fifo->stats->common_stats.usage_cnt;

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_unlock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&fifo->channel.post_lock, flags);
#endif

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_fifo_is_next_txdl_completed - Checks if the next txdl is completed
 * @vpath_handle: Virtual path handle.
 */
vxge_hal_status_e
vxge_hal_fifo_is_next_txdl_completed(vxge_hal_vpath_h vpath_handle)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	vxge_hal_fifo_txd_t *txdp;
	vxge_hal_txdl_h txdlh;
	vxge_hal_status_e status = VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;

#if defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	unsigned long flags = 0;

#endif


	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_lock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&fifo->channel.post_lock, flags);
#endif

	__hal_channel_dtr_try_complete(&fifo->channel, &txdlh);

	txdp = (vxge_hal_fifo_txd_t *) txdlh;
	if ((txdp != NULL) &&
	    (!(txdp->control_0 & VXGE_HAL_FIFO_TXD_LIST_OWN_ADAPTER))) {
		status = VXGE_HAL_OK;
	}

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_unlock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&fifo->channel.post_lock, flags);
#endif

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	/* no more completions */
	return (status);
}

/*
 * vxge_hal_fifo_txdl_next_completed - Retrieve next completed descriptor.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle. Returned by HAL.
 * @txdl_priv: Buffer to return the pointer to per txdl space
 * @t_code: Transfer code, as per X3100 User Guide,
 *	 Transmit Descriptor Format.
 *	 Returned by HAL.
 *
 * Retrieve the _next_ completed descriptor.
 * HAL uses channel callback (*vxge_hal_channel_callback_f) to notifiy
 * upper-layer driver (ULD) of new completed descriptors. After that
 * the ULD can use vxge_hal_fifo_txdl_next_completed to retrieve the rest
 * completions (the very first completion is passed by HAL via
 * vxge_hal_channel_callback_f).
 *
 * Implementation-wise, the upper-layer driver is free to call
 * vxge_hal_fifo_txdl_next_completed either immediately from inside the
 * channel callback, or in a deferred fashion and separate (from HAL)
 * context.
 *
 * Non-zero @t_code means failure to process the descriptor.
 * The failure could happen, for instance, when the link is
 * down, in which case X3100 completes the descriptor because it
 * is not able to send the data out.
 *
 * For details please refer to X3100 User Guide.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS - No completed descriptors
 * are currently available for processing.
 *
 */
vxge_hal_status_e
vxge_hal_fifo_txdl_next_completed(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h * txdlh,
    void **txdl_priv,
    vxge_hal_fifo_tcode_e * t_code)
{
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	vxge_hal_fifo_txd_t *txdp;

#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_TXDL_STREAMING)
	__hal_fifo_txdl_priv_t *priv;

#endif
#if defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	unsigned long flags = 0;

#endif

	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	vxge_hal_status_e status = VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;

	vxge_assert((vpath_handle != NULL) &&
	    (txdlh != NULL) && (t_code != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "txdlh = 0x"VXGE_OS_STXFMT", t_code = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) txdlh, (ptr_t) t_code);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

	*txdlh = 0;

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_lock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&fifo->channel.post_lock, flags);
#endif

	__hal_channel_dtr_try_complete(&fifo->channel, txdlh);

	txdp = (vxge_hal_fifo_txd_t *) * txdlh;
	if (txdp != NULL) {

#if defined(VXGE_OS_DMA_REQUIRES_SYNC) && defined(VXGE_HAL_DMA_TXDL_STREAMING)
		priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdp);

		/*
		 * sync TxDL to read the ownership
		 *
		 * Note: 16bytes means Control_1 & Control_2
		 */
		vxge_os_dma_sync(fifo->channel.pdev,
		    priv->dma_handle,
		    priv->dma_addr,
		    priv->dma_offset,
		    16,
		    VXGE_OS_DMA_DIR_FROMDEVICE);
#endif

		/* check whether host owns it */
		if (!(txdp->control_0 & VXGE_HAL_FIFO_TXD_LIST_OWN_ADAPTER)) {

			__hal_channel_dtr_complete(&fifo->channel);

			*txdl_priv = VXGE_HAL_FIFO_ULD_PRIV(fifo, txdp);

			*t_code = (vxge_hal_fifo_tcode_e)
			    VXGE_HAL_FIFO_TXD_T_CODE_GET(txdp->control_0);

			if (fifo->stats->common_stats.usage_cnt > 0)
				fifo->stats->common_stats.usage_cnt--;

			status = VXGE_HAL_OK;
		}
	}

	/* no more completions */
#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_unlock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&fifo->channel.post_lock, flags);
#endif

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_fifo_handle_tcode - Handle transfer code.
 * @vpath_handle: Virtual Path handle.
 * @txdlh: Descriptor handle.
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
vxge_hal_fifo_handle_tcode(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    vxge_hal_fifo_tcode_e t_code)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;

	vxge_assert((vpath_handle != NULL) && (txdlh != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "txdlh = 0x"VXGE_OS_STXFMT", t_code = 0x%d",
	    (ptr_t) vpath_handle, (ptr_t) txdlh, t_code);

	switch ((t_code & 0x7)) {
	case 0:
		/* 000: Transfer operation completed successfully. */
		break;
	case 1:
		/*
		 * 001: a PCI read transaction (either TxD or frame data)
		 *	returned with corrupt data.
		 */
		break;
	case 2:
		/* 010: a PCI read transaction was returned with no data. */
		break;
	case 3:
		/*
		 * 011: The host attempted to send either a frame or LSO
		 *	MSS that was too long (>9800B).
		 */
		break;
	case 4:
		/*
		 * 100: Error detected during TCP/UDP Large Send
		 *	Offload operation, due to improper header template,
		 *	unsupported protocol, etc.
		 */
		break;
	default:
		vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_TCODE);
		return (VXGE_HAL_ERR_INVALID_TCODE);
	}

	vp->vpath->sw_stats->fifo_stats.txd_t_code_err_cnt[t_code]++;

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, VXGE_HAL_OK);
	return (VXGE_HAL_OK);
}

/*
 * __hal_fifo_txdl_free_many - Free the fragments
 * @fifo: FIFO
 * @txdp: Poniter to a TxD
 * @list_size: List size
 * @frags: Number of fragments
 *
 * This routinf frees the fragments in a txdl
 */
void
__hal_fifo_txdl_free_many(
    __hal_fifo_t *fifo,
    vxge_hal_fifo_txd_t * txdp,
    u32 list_size,
    u32 frags)
{
	__hal_fifo_txdl_priv_t *current_txdl_priv;
	__hal_fifo_txdl_priv_t *next_txdl_priv;
	u32 invalid_frags = frags % list_size;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) fifo->channel.vph;
	__hal_device_t *hldev;

	vxge_assert((fifo != NULL) && (txdp != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo(
	    "fifo = 0x"VXGE_OS_STXFMT", txdp = 0x"VXGE_OS_STXFMT", "
	    "list_size = %d, frags = %d", (ptr_t) fifo, (ptr_t) txdp,
	    list_size, frags);

	if (invalid_frags) {
		vxge_hal_trace_log_fifo(
		    "freeing corrupt txdlh 0x"VXGE_OS_STXFMT", "
		    "fragments %d list size %d",
		    (ptr_t) txdp, frags, list_size);
		vxge_assert(invalid_frags == 0);
	}
	while (txdp) {
		vxge_hal_trace_log_fifo("freeing linked txdlh 0x"VXGE_OS_STXFMT
		    ", " "fragments %d list size %d",
		    (ptr_t) txdp, frags, list_size);
		current_txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdp);
#if defined(VXGE_DEBUG_ASSERT) && defined(VXGE_OS_MEMORY_CHECK)
		current_txdl_priv->allocated = 0;
#endif
		__hal_channel_dtr_free(&fifo->channel,
		    VXGE_HAL_FIFO_TXDL_INDEX(txdp));
		next_txdl_priv = current_txdl_priv->next_txdl_priv;
		vxge_assert(frags);
		frags -= list_size;
		if (next_txdl_priv) {
			current_txdl_priv->next_txdl_priv = NULL;
			txdp = next_txdl_priv->first_txdp;
		} else {
			vxge_hal_trace_log_fifo(
			    "freed linked txdlh fragments %d list size %d",
			    frags, list_size);
			break;
		}
	}

	vxge_assert(frags == 0);

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_fifo_txdl_free - Free descriptor.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 *
 * Free the reserved descriptor. This operation is "symmetrical" to
 * vxge_hal_fifo_txdl_reserve. The "free-ing" completes the descriptor's
 * lifecycle.
 *
 * After free-ing (see vxge_hal_fifo_txdl_free()) the descriptor again can
 * be:
 *
 * - reserved (vxge_hal_fifo_txdl_reserve);
 *
 * - posted (vxge_hal_fifo_txdl_post);
 *
 * - completed (vxge_hal_fifo_txdl_next_completed);
 *
 * - and recycled again (vxge_hal_fifo_txdl_free).
 *
 * For alternative state transitions and more details please refer to
 * the design doc.
 *
 */
void
vxge_hal_fifo_txdl_free(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	__hal_fifo_txdl_priv_t *txdl_priv;
	u32 max_frags;

#if defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	u32 flags = 0;

#endif
	vxge_assert((vpath_handle != NULL) && (txdlh != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "txdlh = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle, (ptr_t) txdlh);

	fifo = (__hal_fifo_t *) vp->vpath->fifoh;

	vxge_assert(fifo != NULL);

	txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdlh);

	max_frags = fifo->config->max_frags;

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_lock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_lock_irq(&fifo->channel.post_lock, flags);
#endif

	if (txdl_priv->alloc_frags > max_frags) {
		vxge_hal_fifo_txd_t *dang_txdp = (vxge_hal_fifo_txd_t *)
		txdl_priv->dang_txdl;
		u32 dang_frags = txdl_priv->dang_frags;
		u32 alloc_frags = txdl_priv->alloc_frags;
		txdl_priv->dang_txdl = NULL;
		txdl_priv->dang_frags = 0;
		txdl_priv->alloc_frags = 0;
		/* txdlh must have a linked list of txdlh */
		vxge_assert(txdl_priv->next_txdl_priv);

		/* free any dangling txdlh first */
		if (dang_txdp) {
			vxge_hal_info_log_fifo(
			    "freeing dangled txdlh 0x"VXGE_OS_STXFMT" for %d "
			    "fragments", (ptr_t) dang_txdp, dang_frags);
			__hal_fifo_txdl_free_many(fifo, dang_txdp,
			    max_frags, dang_frags);
		}

		/* now free the reserved txdlh list */
		vxge_hal_info_log_fifo(
		    "freeing txdlh 0x"VXGE_OS_STXFMT" list of %d fragments",
		    (ptr_t) txdlh, alloc_frags);
		__hal_fifo_txdl_free_many(fifo,
		    (vxge_hal_fifo_txd_t *) txdlh, max_frags,
		    alloc_frags);
	} else {
		__hal_channel_dtr_free(&fifo->channel,
		    VXGE_HAL_FIFO_TXDL_INDEX(txdlh));
	}

	fifo->channel.poll_bytes += txdl_priv->bytes_sent;

#if defined(VXGE_DEBUG_ASSERT) && defined(VXGE_OS_MEMORY_CHECK)
	txdl_priv->allocated = 0;
#endif

#if defined(VXGE_HAL_TX_MULTI_POST)
	vxge_os_spin_unlock(&fifo->channel.post_lock);
#elif defined(VXGE_HAL_TX_MULTI_POST_IRQ)
	vxge_os_spin_unlock_irq(&fifo->channel.post_lock, flags);
#endif

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}
