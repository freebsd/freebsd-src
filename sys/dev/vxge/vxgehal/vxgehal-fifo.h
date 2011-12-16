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

#ifndef	VXGE_HAL_FIFO_H
#define	VXGE_HAL_FIFO_H

__EXTERN_BEGIN_DECLS

/*
 * struct __hal_fifo_t - Fifo.
 * @channel: Channel "base" of this fifo, the common part of all HAL
 *		channels.
 * @mempool: Memory pool, from which descriptors get allocated.
 * @config: Fifo configuration, part of device configuration
 *		(see vxge_hal_device_config_t {}).
 * @interrupt_type: Interrupt type to be used
 * @no_snoop_bits: See vxge_hal_fifo_config_t {}.
 * @memblock_size: Fifo descriptors are allocated in blocks of @mem_block_size
 *		bytes. Setting @memblock_size to page size ensures
 *		by-page allocation of descriptors. 128K bytes is the
 *		maximum supported block size.
 * @txdl_per_memblock: Number of TxDLs (TxD lists) per memblock.
 *		on TxDL please refer to X3100 UG.
 * @txdl_size: Configured TxDL size (i.e., number of TxDs in a list), plus
 *		per-TxDL HAL private space (__hal_fifo_txdl_priv_t).
 * @txdl_priv_size: Per-TxDL space reserved for HAL and ULD
 * @per_txdl_space: Per txdl private space for the ULD
 * @txdlblock_priv_size: Total private space per TXDL memory block
 * @align_size: Cache alignment size
 * @callback: Fifo completion callback. HAL invokes the callback when there
 *		are new completions on that fifo. In many implementations
 *		the @callback executes in the hw interrupt context.
 * @txdl_init: Fifo's descriptor-initialize callback.
 *		See vxge_hal_fifo_txdl_init_f {}.
 *		If not NULL, HAL invokes the callback when opening
 *		the fifo via vxge_hal_vpath_open().
 * @txdl_term: Fifo's descriptor-terminate callback. If not NULL,
 *		HAL invokes the callback when closing the corresponding fifo.
 *		See also vxge_hal_fifo_txdl_term_f {}.
 * @stats: Statistics of this fifo
 *
 * Fifo channel.
 * Note: The structure is cache line aligned.
 */
typedef struct __hal_fifo_t {
	__hal_channel_t				channel;
	vxge_hal_mempool_t			*mempool;
	vxge_hal_fifo_config_t			*config;
	u64					interrupt_type;
	u32					no_snoop_bits;
	u32					memblock_size;
	u32					txdl_per_memblock;
	u32					txdl_size;
	u32					txdl_priv_size;
	u32					per_txdl_space;
	u32					txdlblock_priv_size;
	u32					align_size;
	vxge_hal_fifo_callback_f		callback;
	vxge_hal_fifo_txdl_init_f		txdl_init;
	vxge_hal_fifo_txdl_term_f		txdl_term;
	vxge_hal_vpath_stats_sw_fifo_info_t	*stats;
} __vxge_os_attr_cacheline_aligned __hal_fifo_t;

/*
 * struct __hal_fifo_txdl_priv_t - Transmit descriptor HAL-private data.
 * @dma_addr: DMA (mapped) address of _this_ descriptor.
 * @dma_handle: DMA handle used to map the descriptor onto device.
 * @dma_offset: Descriptor's offset in the memory block. HAL allocates
 *		  descriptors in memory blocks (see vxge_hal_fifo_config_t {})
 *		Each memblock is a contiguous block of DMA-able memory.
 * @frags: Total number of fragments (that is, contiguous data buffers)
 * carried by this TxDL.
 * @align_vaddr_start: Aligned virtual address start
 * @align_vaddr: Virtual address of the per-TxDL area in memory used for
 *		alignement. Used to place one or more mis-aligned fragments
 *		(the maximum defined by configration variable
 *		@max_aligned_frags).
 * @align_dma_addr: DMA address translated from the @align_vaddr.
 * @align_dma_handle: DMA handle that corresponds to @align_dma_addr.
 * @align_dma_acch: DMA access handle corresponds to @align_dma_addr.
 * @align_dma_offset: The current offset into the @align_vaddr area.
 * Grows while filling the descriptor, gets reset.
 * @align_used_frags: Number of fragments used.
 * @alloc_frags: Total number of fragments allocated.
 * @dang_frags: Number of fragments kept from release until this TxDL is freed.
 * @bytes_sent:
 * @unused:
 * @dang_txdl:
 * @next_txdl_priv:
 * @first_txdp:
 * @dang_txdlh: Pointer to TxDL (list) kept from release until this TxDL
 *		is freed.
 * @linked_txdl_priv: Pointer to any linked TxDL for creating contiguous
 *		TxDL list.
 * @txdlh: Corresponding txdlh to this TxDL.
 * @memblock: Pointer to the TxDL memory block or memory page.
 *		on the next send operation.
 * @dma_object: DMA address and handle of the memory block that contains
 *		the descriptor. This member is used only in the "checked"
 *		version of the HAL (to enforce certain assertions);
 *		otherwise it gets compiled out.
 * @allocated: True if the descriptor is reserved, 0 otherwise. Internal usage.
 *
 * Per-transmit decsriptor HAL-private data. HAL uses the space to keep DMA
 * information associated with the descriptor. Note that ULD can ask HAL
 * to allocate additional per-descriptor space for its own (ULD-specific)
 * purposes.
 *
 * See also: vxge_hal_ring_rxd_priv_t {}.
 */
typedef struct __hal_fifo_txdl_priv_t {
	dma_addr_t			dma_addr;
	pci_dma_h			dma_handle;
	ptrdiff_t			dma_offset;
	u32				frags;
	u8				*align_vaddr_start;
	u8				*align_vaddr;
	dma_addr_t			align_dma_addr;
	pci_dma_h			align_dma_handle;
	pci_dma_acc_h			align_dma_acch;
	ptrdiff_t			align_dma_offset;
	u32				align_used_frags;
	u32				alloc_frags;
	u32				dang_frags;
	u32				bytes_sent;
	u32				unused;
	vxge_hal_fifo_txd_t		*dang_txdl;
	struct __hal_fifo_txdl_priv_t	*next_txdl_priv;
	vxge_hal_fifo_txd_t		*first_txdp;
	void				*memblock;
#if defined(VXGE_DEBUG_ASSERT)
	vxge_hal_mempool_dma_t		*dma_object;
#endif
#if defined(VXGE_OS_MEMORY_CHECK)
	u32				allocated;
#endif
} __hal_fifo_txdl_priv_t;

#define	VXGE_HAL_FIFO_ULD_PRIV(fifo, txdh)				\
	fifo->channel.dtr_arr[						\
		((vxge_hal_fifo_txd_t *)(txdh))->host_control].uld_priv

#define	VXGE_HAL_FIFO_HAL_PRIV(fifo, txdh)				\
	((__hal_fifo_txdl_priv_t *)(fifo->channel.dtr_arr[		\
		((vxge_hal_fifo_txd_t *)(txdh))->host_control].hal_priv))

#define	VXGE_HAL_FIFO_MAX_FRAG_CNT(fifo) fifo->config->max_frags

#define	VXGE_HAL_FIFO_TXDL_INDEX(txdp)	\
	(u32)((vxge_hal_fifo_txd_t *)txdp)->host_control

/* ========================= FIFO PRIVATE API ============================= */

vxge_hal_status_e
__hal_fifo_create(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_fifo_attr_t *attr);

void
__hal_fifo_abort(
    vxge_hal_fifo_h fifoh,
    vxge_hal_reopen_e reopen);

vxge_hal_status_e
__hal_fifo_reset(
    vxge_hal_fifo_h ringh);

void
__hal_fifo_delete(
    vxge_hal_vpath_h vpath_handle);

void
__hal_fifo_txdl_free_many(
    __hal_fifo_t *fifo,
    vxge_hal_fifo_txd_t *txdp,
    u32 list_size,
    u32 frags);

#if defined(VXGE_HAL_ALIGN_XMIT)
void
__hal_fifo_txdl_align_free_unmap(
    __hal_fifo_t *fifo,
    vxge_hal_fifo_txd_t *txdp);

vxge_hal_status_e
__hal_fifo_txdl_align_alloc_map(
    __hal_fifo_t *fifo,
    vxge_hal_fifo_txd_t *txdp);

#endif

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_FIFO_H */
