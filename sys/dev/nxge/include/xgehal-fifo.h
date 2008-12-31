/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 *
 * $FreeBSD: src/sys/dev/nxge/include/xgehal-fifo.h,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef XGE_HAL_FIFO_H
#define XGE_HAL_FIFO_H

#include <dev/nxge/include/xgehal-channel.h>
#include <dev/nxge/include/xgehal-config.h>
#include <dev/nxge/include/xgehal-mm.h>

__EXTERN_BEGIN_DECLS

/* HW fifo configuration */
#define XGE_HAL_FIFO_INT_PER_LIST_THRESHOLD 65
#define XGE_HAL_FIFO_MAX_WRR            5
#define XGE_HAL_FIFO_MAX_PARTITION      4
#define XGE_HAL_FIFO_MAX_WRR_STATE      36
#define XGE_HAL_FIFO_HW_PAIR_OFFSET     0x20000

/* HW FIFO Weight Calender */
#define XGE_HAL_FIFO_WRR_0      0x0706050407030602ULL
#define XGE_HAL_FIFO_WRR_1      0x0507040601070503ULL
#define XGE_HAL_FIFO_WRR_2      0x0604070205060700ULL
#define XGE_HAL_FIFO_WRR_3      0x0403060705010207ULL
#define XGE_HAL_FIFO_WRR_4      0x0604050300000000ULL
/*
 * xge_hal_fifo_hw_pair_t
 *
 * Represent a single fifo in the BAR1 memory space.
 */
typedef struct {
	u64 txdl_pointer; /* offset 0x0 */

	u64 reserved[2];

	u64 list_control; /* offset 0x18 */
#define XGE_HAL_TX_FIFO_LAST_TXD_NUM( val)     vBIT(val,0,8)
#define XGE_HAL_TX_FIFO_FIRST_LIST             BIT(14)
#define XGE_HAL_TX_FIFO_LAST_LIST              BIT(15)
#define XGE_HAL_TX_FIFO_FIRSTNLAST_LIST        vBIT(3,14,2)
#define XGE_HAL_TX_FIFO_SPECIAL_FUNC           BIT(23)
#define XGE_HAL_TX_FIFO_NO_SNOOP(n)            vBIT(n,30,2)
} xge_hal_fifo_hw_pair_t;


/* Bad TxDL transfer codes */
#define XGE_HAL_TXD_T_CODE_OK               0x0
#define XGE_HAL_TXD_T_CODE_UNUSED_1     0x1
#define XGE_HAL_TXD_T_CODE_ABORT_BUFFER     0x2
#define XGE_HAL_TXD_T_CODE_ABORT_DTOR       0x3
#define XGE_HAL_TXD_T_CODE_UNUSED_5     0x5
#define XGE_HAL_TXD_T_CODE_PARITY       0x7
#define XGE_HAL_TXD_T_CODE_LOSS_OF_LINK     0xA
#define XGE_HAL_TXD_T_CODE_GENERAL_ERR      0xF


/**
 * struct xge_hal_fifo_txd_t - TxD.
 * @control_1: Control_1.
 * @control_2: Control_2.
 * @buffer_pointer: Buffer_Address.
 * @host_control: Host_Control.Opaque 64bit data stored by ULD inside the Xframe
 *            descriptor prior to posting the latter on the channel
 *            via xge_hal_fifo_dtr_post() or xge_hal_ring_dtr_post().
 *            The %host_control is returned as is to the ULD with each
 *            completed descriptor.
 *
 * Transmit descriptor (TxD).Fifo descriptor contains configured number
 * (list) of TxDs. * For more details please refer to Xframe User Guide,
 * Section 5.4.2 "Transmit Descriptor (TxD) Format".
 */
typedef struct xge_hal_fifo_txd_t {
	u64 control_1;
#define XGE_HAL_TXD_LIST_OWN_XENA       BIT(7)
#define XGE_HAL_TXD_T_CODE      (BIT(12)|BIT(13)|BIT(14)|BIT(15))
#define XGE_HAL_GET_TXD_T_CODE(val)     ((val & XGE_HAL_TXD_T_CODE)>>48)
#define XGE_HAL_SET_TXD_T_CODE(x, val)  (x |= (((u64)val & 0xF) << 48))
#define XGE_HAL_TXD_GATHER_CODE         (BIT(22) | BIT(23))
#define XGE_HAL_TXD_GATHER_CODE_FIRST   BIT(22)
#define XGE_HAL_TXD_GATHER_CODE_LAST    BIT(23)
#define XGE_HAL_TXD_NO_LSO              0
#define XGE_HAL_TXD_UDF_COF             1
#define XGE_HAL_TXD_TCP_LSO             2
#define XGE_HAL_TXD_UDP_LSO             3
#define XGE_HAL_TXD_LSO_COF_CTRL(val)   vBIT(val,30,2)
#define XGE_HAL_TXD_TCP_LSO_MSS(val)    vBIT(val,34,14)
#define XGE_HAL_TXD_BUFFER0_SIZE(val)   vBIT(val,48,16)
#define XGE_HAL_TXD_GET_LSO_BYTES_SENT(val) ((val & vBIT(0xFFFF,16,16))>>32)
	u64 control_2;
#define XGE_HAL_TXD_TX_CKO_CONTROL      (BIT(5)|BIT(6)|BIT(7))
#define XGE_HAL_TXD_TX_CKO_IPV4_EN      BIT(5)
#define XGE_HAL_TXD_TX_CKO_TCP_EN       BIT(6)
#define XGE_HAL_TXD_TX_CKO_UDP_EN       BIT(7)
#define XGE_HAL_TXD_VLAN_ENABLE         BIT(15)
#define XGE_HAL_TXD_VLAN_TAG(val)       vBIT(val,16,16)
#define XGE_HAL_TXD_INT_NUMBER(val)     vBIT(val,34,6)
#define XGE_HAL_TXD_INT_TYPE_PER_LIST   BIT(47)
#define XGE_HAL_TXD_INT_TYPE_UTILZ      BIT(46)
#define XGE_HAL_TXD_SET_MARKER          vBIT(0x6,0,4)

	u64 buffer_pointer;

	u64 host_control;

} xge_hal_fifo_txd_t;

typedef xge_hal_fifo_txd_t* xge_hal_fifo_txdl_t;

/**
 * struct xge_hal_fifo_t - Fifo channel.
 * @channel: Channel "base" of this fifo, the common part of all HAL
 *           channels.
 * @post_lock_ptr: Points to a lock that serializes (pointer, control) PIOs.
 *           Note that for Xena the serialization is done across all device
 *           fifos.
 * @hw_pair: Per-fifo (Pointer, Control) pair used to send descriptors to the
 *           Xframe hardware (for details see Xframe user guide).
 * @config: Fifo configuration, part of device configuration
 *          (see xge_hal_device_config_t{}).
 * @no_snoop_bits: See xge_hal_fifo_config_t{}.
 * @txdl_per_memblock: Number of TxDLs (TxD lists) per memblock.
 * on TxDL please refer to Xframe UG.
 * @interrupt_type: FIXME: to-be-defined.
 * @txdl_size: Configured TxDL size (i.e., number of TxDs in a list), plus
 *             per-TxDL HAL private space (xge_hal_fifo_txdl_priv_t).
 * @priv_size: Per-Tx descriptor space reserved for upper-layer driver
 *             usage.
 * @mempool: Memory pool, from which descriptors get allocated.
 * @align_size: TBD
 *
 * Fifo channel.
 * Note: The structure is cache line aligned.
 */
typedef struct xge_hal_fifo_t {
	xge_hal_channel_t   channel;
	spinlock_t      *post_lock_ptr;
	xge_hal_fifo_hw_pair_t  *hw_pair;
	xge_hal_fifo_config_t   *config;
	int         no_snoop_bits;
	int         txdl_per_memblock;
	u64         interrupt_type;
	int         txdl_size;
	int         priv_size;
	xge_hal_mempool_t   *mempool;
	int         align_size;
} __xge_os_attr_cacheline_aligned xge_hal_fifo_t;

/**
 * struct xge_hal_fifo_txdl_priv_t - Transmit descriptor HAL-private
 * data.
 * @dma_addr: DMA (mapped) address of _this_ descriptor.
 * @dma_handle: DMA handle used to map the descriptor onto device.
 * @dma_offset: Descriptor's offset in the memory block. HAL allocates
 * descriptors in memory blocks (see
 * xge_hal_fifo_config_t{})
 * Each memblock is a contiguous block of DMA-able memory.
 * @frags: Total number of fragments (that is, contiguous data buffers)
 * carried by this TxDL.
 * @align_vaddr_start: (TODO).
 * @align_vaddr: Virtual address of the per-TxDL area in memory used for
 * alignement. Used to place one or more mis-aligned fragments
 * (the maximum defined by configration variable
 * @max_aligned_frags).
 * @align_dma_addr: DMA address translated from the @align_vaddr.
 * @align_dma_handle: DMA handle that corresponds to @align_dma_addr.
 * @align_dma_acch: DMA access handle corresponds to @align_dma_addr.
 * @align_dma_offset: The current offset into the @align_vaddr area.
 * Grows while filling the descriptor, gets reset.
 * @align_used_frags: (TODO).
 * @alloc_frags: Total number of fragments allocated.
 * @dang_frags: Number of fragments kept from release until this TxDL is freed.
 * @bytes_sent: TODO
 * @unused: TODO
 * @dang_txdl: (TODO).
 * @next_txdl_priv: (TODO).
 * @first_txdp: (TODO).
 * @dang_dtrh: Pointer to TxDL (list) kept from release until this TxDL
 * is freed.
 * @linked_txdl_priv: Pointer to any linked TxDL for creating contiguous
 * TxDL list.
 * @dtrh: Corresponding dtrh to this TxDL.
 * @memblock: Pointer to the TxDL memory block or memory page.
 * on the next send operation.
 * @dma_object: DMA address and handle of the memory block that contains
 * the descriptor. This member is used only in the "checked"
 * version of the HAL (to enforce certain assertions);
 * otherwise it gets compiled out.
 * @allocated: True if the descriptor is reserved, 0 otherwise. Internal usage.
 *
 * Per-transmit decsriptor HAL-private data. HAL uses the space to keep DMA
 * information associated with the descriptor. Note that ULD can ask HAL
 * to allocate additional per-descriptor space for its own (ULD-specific)
 * purposes.
 *
 * See also: xge_hal_ring_rxd_priv_t{}.
 */
typedef struct xge_hal_fifo_txdl_priv_t {
	dma_addr_t              dma_addr;
	pci_dma_h               dma_handle;
	ptrdiff_t               dma_offset;
	int                 frags;
	char                    *align_vaddr_start;
	char                    *align_vaddr;
	dma_addr_t              align_dma_addr;
	pci_dma_h               align_dma_handle;
	pci_dma_acc_h               align_dma_acch;
	ptrdiff_t               align_dma_offset;
	int                 align_used_frags;
	int                 alloc_frags;
	int                 dang_frags;
	unsigned int                bytes_sent;
	int                 unused;
	xge_hal_fifo_txd_t          *dang_txdl;
	struct xge_hal_fifo_txdl_priv_t     *next_txdl_priv;
	xge_hal_fifo_txd_t          *first_txdp;
	void                    *memblock;
#ifdef XGE_DEBUG_ASSERT
	xge_hal_mempool_dma_t           *dma_object;
#endif
#ifdef XGE_OS_MEMORY_CHECK
	int                 allocated;
#endif
} xge_hal_fifo_txdl_priv_t;

/**
 * xge_hal_fifo_get_max_frags_cnt - Return the max fragments allocated
 * for the fifo.
 * @channelh: Channel handle.
 */
static inline int
xge_hal_fifo_get_max_frags_cnt(xge_hal_channel_h channelh)
{
	return ((xge_hal_fifo_t *)channelh)->config->max_frags;
}
/* ========================= FIFO PRIVATE API ============================= */

xge_hal_status_e __hal_fifo_open(xge_hal_channel_h channelh,
	        xge_hal_channel_attr_t *attr);

void __hal_fifo_close(xge_hal_channel_h channelh);

void __hal_fifo_hw_initialize(xge_hal_device_h hldev);

xge_hal_status_e
__hal_fifo_dtr_align_alloc_map(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

void
__hal_fifo_dtr_align_free_unmap(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

#if defined(XGE_DEBUG_FP) && (XGE_DEBUG_FP & XGE_DEBUG_FP_FIFO)
#define __HAL_STATIC_FIFO
#define __HAL_INLINE_FIFO

__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_fifo_txdl_priv_t*
__hal_fifo_txdl_priv(xge_hal_dtr_h dtrh);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
__hal_fifo_dtr_post_single(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	        u64 ctrl_1);
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
__hal_fifo_txdl_restore_many(xge_hal_channel_h channelh,
	          xge_hal_fifo_txd_t *txdp, int txdl_count);

/* ========================= FIFO PUBLIC API ============================== */

__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_reserve(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_reserve_many(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh,
	                          const int frags);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void*
xge_hal_fifo_dtr_private(xge_hal_dtr_h dtrh);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO int
xge_hal_fifo_dtr_buffer_cnt(xge_hal_dtr_h dtrh);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_reserve_sp(xge_hal_channel_h channel, int dtr_sp_size,
	        xge_hal_dtr_h dtr_sp);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_post(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_post_many(xge_hal_channel_h channelh, int num,
	        xge_hal_dtr_h dtrs[]);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_next_completed(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh,
	        u8 *t_code);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_free(xge_hal_channel_h channelh, xge_hal_dtr_h dtr);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_buffer_set(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	        int frag_idx, dma_addr_t dma_pointer, int size);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_buffer_set_aligned(xge_hal_channel_h channelh,
	        xge_hal_dtr_h dtrh, int frag_idx, void *vaddr,
	        dma_addr_t dma_pointer, int size, int misaligned_size);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_buffer_append(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	    void *vaddr, int size);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_buffer_finalize(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	    int frag_idx);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_mss_set(xge_hal_dtr_h dtrh, int mss);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_cksum_set_bits(xge_hal_dtr_h dtrh, u64 cksum_bits);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_vlan_set(xge_hal_dtr_h dtrh, u16 vlan_tag);

__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_is_next_dtr_completed(xge_hal_channel_h channelh);

#else /* XGE_FASTPATH_EXTERN */
#define __HAL_STATIC_FIFO static
#define __HAL_INLINE_FIFO inline
#include <dev/nxge/xgehal/xgehal-fifo-fp.c>
#endif /* XGE_FASTPATH_INLINE */

__EXTERN_END_DECLS

#endif /* XGE_HAL_FIFO_H */
