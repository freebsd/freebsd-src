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
 * $FreeBSD$
 */

#ifndef XGE_HAL_RING_H
#define XGE_HAL_RING_H

#include <dev/nxge/include/xgehal-channel.h>
#include <dev/nxge/include/xgehal-config.h>
#include <dev/nxge/include/xgehal-mm.h>

__EXTERN_BEGIN_DECLS

/* HW ring configuration */
#define XGE_HAL_RING_RXDBLOCK_SIZE  0x1000

#define XGE_HAL_RXD_T_CODE_OK       0x0
#define XGE_HAL_RXD_T_CODE_PARITY   0x1
#define XGE_HAL_RXD_T_CODE_ABORT    0x2
#define XGE_HAL_RXD_T_CODE_PARITY_ABORT 0x3
#define XGE_HAL_RXD_T_CODE_RDA_FAILURE  0x4
#define XGE_HAL_RXD_T_CODE_UNKNOWN_PROTO 0x5
#define XGE_HAL_RXD_T_CODE_BAD_FCS  0x6
#define XGE_HAL_RXD_T_CODE_BUFF_SIZE    0x7
#define XGE_HAL_RXD_T_CODE_BAD_ECC  0x8
#define XGE_HAL_RXD_T_CODE_UNUSED_C 0xC
#define XGE_HAL_RXD_T_CODE_UNKNOWN  0xF

#define XGE_HAL_RING_USE_MTU        -1

/* control_1 and control_2 formatting - same for all buffer modes */
#define XGE_HAL_RXD_GET_L3_CKSUM(control_1) ((u16)(control_1>>16) & 0xFFFF)
#define XGE_HAL_RXD_GET_L4_CKSUM(control_1) ((u16)(control_1 & 0xFFFF))

#define XGE_HAL_RXD_MASK_VLAN_TAG       vBIT(0xFFFF,48,16)
#define XGE_HAL_RXD_SET_VLAN_TAG(control_2, val) control_2 |= (u16)val
#define XGE_HAL_RXD_GET_VLAN_TAG(control_2) ((u16)(control_2 & 0xFFFF))

#define XGE_HAL_RXD_POSTED_4_XFRAME     BIT(7)  /* control_1 */
#define XGE_HAL_RXD_NOT_COMPLETED               BIT(0)  /* control_2 */
#define XGE_HAL_RXD_T_CODE      (BIT(12)|BIT(13)|BIT(14)|BIT(15))
#define XGE_HAL_RXD_GET_T_CODE(control_1)   \
	            ((control_1 & XGE_HAL_RXD_T_CODE)>>48)
#define XGE_HAL_RXD_SET_T_CODE(control_1, val) \
	            (control_1 |= (((u64)val & 0xF) << 48))

#define XGE_HAL_RXD_MASK_FRAME_TYPE     vBIT(0x3,25,2)
#define XGE_HAL_RXD_MASK_FRAME_PROTO        vBIT(0xFFFF,24,8)
#define XGE_HAL_RXD_GET_FRAME_TYPE(control_1)   \
	    (u8)(0x3 & ((control_1 & XGE_HAL_RXD_MASK_FRAME_TYPE) >> 37))
#define XGE_HAL_RXD_GET_FRAME_PROTO(control_1)  \
	        (u8)((control_1 & XGE_HAL_RXD_MASK_FRAME_PROTO) >> 32)
#define XGE_HAL_RXD_FRAME_PROTO_VLAN_TAGGED BIT(24)
#define XGE_HAL_RXD_FRAME_PROTO_IPV4        BIT(27)
#define XGE_HAL_RXD_FRAME_PROTO_IPV6        BIT(28)
#define XGE_HAL_RXD_FRAME_PROTO_IP_FRAGMENTED   BIT(29)
#define XGE_HAL_RXD_FRAME_PROTO_TCP     BIT(30)
#define XGE_HAL_RXD_FRAME_PROTO_UDP     BIT(31)
#define XGE_HAL_RXD_FRAME_TCP_OR_UDP (XGE_HAL_RXD_FRAME_PROTO_TCP | \
	            XGE_HAL_RXD_FRAME_PROTO_UDP)

/**
 * enum xge_hal_frame_type_e - Ethernet frame format.
 * @XGE_HAL_FRAME_TYPE_DIX: DIX (Ethernet II) format.
 * @XGE_HAL_FRAME_TYPE_LLC: LLC format.
 * @XGE_HAL_FRAME_TYPE_SNAP: SNAP format.
 * @XGE_HAL_FRAME_TYPE_IPX: IPX format.
 *
 * Ethernet frame format.
 */
typedef enum xge_hal_frame_type_e {
	XGE_HAL_FRAME_TYPE_DIX          = 0x0,
	XGE_HAL_FRAME_TYPE_LLC          = 0x1,
	XGE_HAL_FRAME_TYPE_SNAP         = 0x2,
	XGE_HAL_FRAME_TYPE_IPX          = 0x3,
} xge_hal_frame_type_e;

/**
 * enum xge_hal_frame_proto_e - Higher-layer ethernet protocols.
 * @XGE_HAL_FRAME_PROTO_VLAN_TAGGED: VLAN.
 * @XGE_HAL_FRAME_PROTO_IPV4: IPv4.
 * @XGE_HAL_FRAME_PROTO_IPV6: IPv6.
 * @XGE_HAL_FRAME_PROTO_IP_FRAGMENTED: IP fragmented.
 * @XGE_HAL_FRAME_PROTO_TCP: TCP.
 * @XGE_HAL_FRAME_PROTO_UDP: UDP.
 * @XGE_HAL_FRAME_PROTO_TCP_OR_UDP: TCP or UDP.
 *
 * Higher layer ethernet protocols and options.
 */
typedef enum xge_hal_frame_proto_e {
	XGE_HAL_FRAME_PROTO_VLAN_TAGGED     = 0x80,
	XGE_HAL_FRAME_PROTO_IPV4        = 0x10,
	XGE_HAL_FRAME_PROTO_IPV6        = 0x08,
	XGE_HAL_FRAME_PROTO_IP_FRAGMENTED   = 0x04,
	XGE_HAL_FRAME_PROTO_TCP         = 0x02,
	XGE_HAL_FRAME_PROTO_UDP         = 0x01,
	XGE_HAL_FRAME_PROTO_TCP_OR_UDP      = (XGE_HAL_FRAME_PROTO_TCP | \
	                       XGE_HAL_FRAME_PROTO_UDP)
} xge_hal_frame_proto_e;

/*
 * xge_hal_ring_rxd_1_t
 */
typedef struct {
	u64 host_control;
	u64 control_1;
	u64 control_2;
#define XGE_HAL_RXD_1_MASK_BUFFER0_SIZE     vBIT(0xFFFF,0,16)
#define XGE_HAL_RXD_1_SET_BUFFER0_SIZE(val) vBIT(val,0,16)
#define XGE_HAL_RXD_1_GET_BUFFER0_SIZE(Control_2) \
	        (int)((Control_2 & vBIT(0xFFFF,0,16))>>48)
#define XGE_HAL_RXD_1_GET_RTH_VALUE(Control_2) \
	        (u32)((Control_2 & vBIT(0xFFFFFFFF,16,32))>>16)
	u64 buffer0_ptr;
} xge_hal_ring_rxd_1_t;

/*
 * xge_hal_ring_rxd_3_t
 */
typedef struct {
	u64 host_control;
	u64 control_1;

	u64 control_2;
#define XGE_HAL_RXD_3_MASK_BUFFER0_SIZE     vBIT(0xFF,8,8)
#define XGE_HAL_RXD_3_SET_BUFFER0_SIZE(val) vBIT(val,8,8)
#define XGE_HAL_RXD_3_MASK_BUFFER1_SIZE     vBIT(0xFFFF,16,16)
#define XGE_HAL_RXD_3_SET_BUFFER1_SIZE(val) vBIT(val,16,16)
#define XGE_HAL_RXD_3_MASK_BUFFER2_SIZE     vBIT(0xFFFF,32,16)
#define XGE_HAL_RXD_3_SET_BUFFER2_SIZE(val) vBIT(val,32,16)


#define XGE_HAL_RXD_3_GET_BUFFER0_SIZE(Control_2) \
	            (int)((Control_2 & vBIT(0xFF,8,8))>>48)
#define XGE_HAL_RXD_3_GET_BUFFER1_SIZE(Control_2) \
	            (int)((Control_2 & vBIT(0xFFFF,16,16))>>32)
#define XGE_HAL_RXD_3_GET_BUFFER2_SIZE(Control_2) \
	            (int)((Control_2 & vBIT(0xFFFF,32,16))>>16)

	u64 buffer0_ptr;
	u64 buffer1_ptr;
	u64 buffer2_ptr;
} xge_hal_ring_rxd_3_t;

/*
 * xge_hal_ring_rxd_5_t
 */
typedef struct {
#ifdef XGE_OS_HOST_BIG_ENDIAN
	u32 host_control;
	u32 control_3;
#else
	u32 control_3;
	u32 host_control;
#endif


#define XGE_HAL_RXD_5_MASK_BUFFER3_SIZE     vBIT(0xFFFF,32,16)
#define XGE_HAL_RXD_5_SET_BUFFER3_SIZE(val) vBIT(val,32,16)
#define XGE_HAL_RXD_5_MASK_BUFFER4_SIZE     vBIT(0xFFFF,48,16)
#define XGE_HAL_RXD_5_SET_BUFFER4_SIZE(val) vBIT(val,48,16)

#define XGE_HAL_RXD_5_GET_BUFFER3_SIZE(Control_3) \
	            (int)((Control_3 & vBIT(0xFFFF,32,16))>>16)
#define XGE_HAL_RXD_5_GET_BUFFER4_SIZE(Control_3) \
	            (int)((Control_3 & vBIT(0xFFFF,48,16)))

	u64 control_1;
	u64 control_2;

#define XGE_HAL_RXD_5_MASK_BUFFER0_SIZE     vBIT(0xFFFF,0,16)
#define XGE_HAL_RXD_5_SET_BUFFER0_SIZE(val) vBIT(val,0,16)
#define XGE_HAL_RXD_5_MASK_BUFFER1_SIZE     vBIT(0xFFFF,16,16)
#define XGE_HAL_RXD_5_SET_BUFFER1_SIZE(val) vBIT(val,16,16)
#define XGE_HAL_RXD_5_MASK_BUFFER2_SIZE     vBIT(0xFFFF,32,16)
#define XGE_HAL_RXD_5_SET_BUFFER2_SIZE(val) vBIT(val,32,16)


#define XGE_HAL_RXD_5_GET_BUFFER0_SIZE(Control_2) \
	        (int)((Control_2 & vBIT(0xFFFF,0,16))>>48)
#define XGE_HAL_RXD_5_GET_BUFFER1_SIZE(Control_2) \
	        (int)((Control_2 & vBIT(0xFFFF,16,16))>>32)
#define XGE_HAL_RXD_5_GET_BUFFER2_SIZE(Control_2) \
	        (int)((Control_2 & vBIT(0xFFFF,32,16))>>16)
	u64 buffer0_ptr;
	u64 buffer1_ptr;
	u64 buffer2_ptr;
	u64 buffer3_ptr;
	u64 buffer4_ptr;
} xge_hal_ring_rxd_5_t;

#define XGE_HAL_RXD_GET_RTH_SPDM_HIT(Control_1) \
	    (u8)((Control_1 & BIT(18))>>45)
#define XGE_HAL_RXD_GET_RTH_IT_HIT(Control_1) \
	    (u8)((Control_1 & BIT(19))>>44)
#define XGE_HAL_RXD_GET_RTH_HASH_TYPE(Control_1) \
	    (u8)((Control_1 & vBIT(0xF,20,4))>>40)

#define XGE_HAL_RXD_HASH_TYPE_NONE              0x0
#define XGE_HAL_RXD_HASH_TYPE_TCP_IPV4          0x1
#define XGE_HAL_RXD_HASH_TYPE_UDP_IPV4          0x2
#define XGE_HAL_RXD_HASH_TYPE_IPV4              0x3
#define XGE_HAL_RXD_HASH_TYPE_TCP_IPV6          0x4
#define XGE_HAL_RXD_HASH_TYPE_UDP_IPV6          0x5
#define XGE_HAL_RXD_HASH_TYPE_IPV6              0x6
#define XGE_HAL_RXD_HASH_TYPE_TCP_IPV6_EX       0x7
#define XGE_HAL_RXD_HASH_TYPE_UDP_IPV6_EX       0x8
#define XGE_HAL_RXD_HASH_TYPE_IPV6_EX           0x9

typedef u8 xge_hal_ring_block_t[XGE_HAL_RING_RXDBLOCK_SIZE];

#define XGE_HAL_RING_NEXT_BLOCK_POINTER_OFFSET  0xFF8
#define XGE_HAL_RING_MEMBLOCK_IDX_OFFSET    0xFF0

#define XGE_HAL_RING_RXD_SIZEOF(n) \
	(n==1 ? sizeof(xge_hal_ring_rxd_1_t) : \
	    (n==3 ? sizeof(xge_hal_ring_rxd_3_t) : \
	        sizeof(xge_hal_ring_rxd_5_t)))

#define XGE_HAL_RING_RXDS_PER_BLOCK(n) \
	(n==1 ? 127 : (n==3 ? 85 : 63))

/**
 * struct xge_hal_ring_rxd_priv_t - Receive descriptor HAL-private data.
 * @dma_addr: DMA (mapped) address of _this_ descriptor.
 * @dma_handle: DMA handle used to map the descriptor onto device.
 * @dma_offset: Descriptor's offset in the memory block. HAL allocates
 *              descriptors in memory blocks of
 *              %XGE_HAL_RING_RXDBLOCK_SIZE
 *              bytes. Each memblock is contiguous DMA-able memory. Each
 *              memblock contains 1 or more 4KB RxD blocks visible to the
 *              Xframe hardware.
 * @dma_object: DMA address and handle of the memory block that contains
 *              the descriptor. This member is used only in the "checked"
 *              version of the HAL (to enforce certain assertions);
 *              otherwise it gets compiled out.
 * @allocated: True if the descriptor is reserved, 0 otherwise. Internal usage.
 *
 * Per-receive decsriptor HAL-private data. HAL uses the space to keep DMA
 * information associated with the descriptor. Note that ULD can ask HAL
 * to allocate additional per-descriptor space for its own (ULD-specific)
 * purposes.
 */
typedef struct xge_hal_ring_rxd_priv_t {
	dma_addr_t      dma_addr;
	pci_dma_h       dma_handle;
	ptrdiff_t       dma_offset;
#ifdef XGE_DEBUG_ASSERT
	xge_hal_mempool_dma_t   *dma_object;
#endif
#ifdef XGE_OS_MEMORY_CHECK
	int         allocated;
#endif
} xge_hal_ring_rxd_priv_t;

/**
 * struct xge_hal_ring_t - Ring channel.
 * @channel: Channel "base" of this ring, the common part of all HAL
 *           channels.
 * @buffer_mode: 1, 3, or 5. The value specifies a receive buffer mode,
 *          as per Xframe User Guide.
 * @indicate_max_pkts: Maximum number of packets processed within a single
 *          interrupt. Can be used to limit the time spent inside hw
 *          interrupt.
 * @config: Ring configuration, part of device configuration
 *          (see xge_hal_device_config_t{}).
 * @rxd_size: RxD sizes for 1-, 3- or 5- buffer modes. As per Xframe spec,
 *            1-buffer mode descriptor is 32 byte long, etc.
 * @rxd_priv_size: Per RxD size reserved (by HAL) for ULD to keep per-descriptor
 *                 data (e.g., DMA handle for Solaris)
 * @rxds_per_block: Number of descriptors per hardware-defined RxD
 *                  block. Depends on the (1-,3-,5-) buffer mode.
 * @mempool: Memory pool, the pool from which descriptors get allocated.
 *           (See xge_hal_mm.h).
 * @rxdblock_priv_size: Reserved at the end of each RxD block. HAL internal
 *                      usage. Not to confuse with @rxd_priv_size.
 * @reserved_rxds_arr: Array of RxD pointers. At any point in time each
 *                     entry in this array is available for allocation
 *                     (via xge_hal_ring_dtr_reserve()) and posting.
 * @cmpl_cnt: Completion counter. Is reset to zero upon entering the ISR.
 *            Used in conjunction with @indicate_max_pkts. 
 * Ring channel.
 *
 * Note: The structure is cache line aligned to better utilize
 *       CPU cache performance.
 */
typedef struct xge_hal_ring_t {
	xge_hal_channel_t       channel;
	int             buffer_mode;
	int             indicate_max_pkts;
	xge_hal_ring_config_t       *config;
	int             rxd_size;
	int             rxd_priv_size;
	int             rxds_per_block;
	xge_hal_mempool_t       *mempool;
	int             rxdblock_priv_size;
	void                **reserved_rxds_arr;
	int             cmpl_cnt;
} __xge_os_attr_cacheline_aligned xge_hal_ring_t;

/**
 * struct xge_hal_dtr_info_t - Extended information associated with a
 * completed ring descriptor.
 * @l3_cksum: Result of IP checksum check (by Xframe hardware).
 *            This field containing XGE_HAL_L3_CKSUM_OK would mean that
 *            the checksum is correct, otherwise - the datagram is
 *            corrupted.
 * @l4_cksum: Result of TCP/UDP checksum check (by Xframe hardware).
 *            This field containing XGE_HAL_L4_CKSUM_OK would mean that
 *            the checksum is correct. Otherwise - the packet is
 *            corrupted.
 * @frame: See xge_hal_frame_type_e{}.
 * @proto:    Reporting bits for various higher-layer protocols, including (but
 *        note restricted to) TCP and UDP. See xge_hal_frame_proto_e{}.
 * @vlan:     VLAN tag extracted from the received frame.
 * @rth_value: Receive Traffic Hashing(RTH) hash value. Produced by Xframe II
 *             hardware if RTH is enabled.
 * @rth_it_hit: Set, If RTH hash value calculated by the Xframe II hardware
 *             has a matching entry in the Indirection table.
 * @rth_spdm_hit: Set, If RTH hash value calculated by the Xframe II hardware
 *             has a matching entry in the Socket Pair Direct Match table.
 * @rth_hash_type: RTH hash code of the function used to calculate the hash.
 * @reserved_pad: Unused byte.
 */
typedef struct xge_hal_dtr_info_t {
	int l3_cksum;
	int l4_cksum;
	int frame; /* zero or more of xge_hal_frame_type_e flags */
	int proto; /* zero or more of xge_hal_frame_proto_e flags */
	int vlan;
	u32 rth_value;
	u8  rth_it_hit;
	u8  rth_spdm_hit;
	u8  rth_hash_type;
	u8  reserved_pad;
} xge_hal_dtr_info_t;

/* ========================== RING PRIVATE API ============================ */

xge_hal_status_e __hal_ring_open(xge_hal_channel_h channelh,
	        xge_hal_channel_attr_t  *attr);

void __hal_ring_close(xge_hal_channel_h channelh);

void __hal_ring_hw_initialize(xge_hal_device_h devh);

void __hal_ring_mtu_set(xge_hal_device_h devh, int new_mtu);

void __hal_ring_prc_enable(xge_hal_channel_h channelh);

void __hal_ring_prc_disable(xge_hal_channel_h channelh);

xge_hal_status_e __hal_ring_initial_replenish(xge_hal_channel_t *channel,
	                      xge_hal_channel_reopen_e reopen);

#if defined(XGE_DEBUG_FP) && (XGE_DEBUG_FP & XGE_DEBUG_FP_RING)
#define __HAL_STATIC_RING
#define __HAL_INLINE_RING

__HAL_STATIC_RING __HAL_INLINE_RING int
__hal_ring_block_memblock_idx(xge_hal_ring_block_t *block);

__HAL_STATIC_RING __HAL_INLINE_RING void
__hal_ring_block_memblock_idx_set(xge_hal_ring_block_t*block, int memblock_idx);

__HAL_STATIC_RING __HAL_INLINE_RING dma_addr_t
__hal_ring_block_next_pointer(xge_hal_ring_block_t *block);

__HAL_STATIC_RING __HAL_INLINE_RING void
__hal_ring_block_next_pointer_set(xge_hal_ring_block_t*block,
	        dma_addr_t dma_next);

__HAL_STATIC_RING __HAL_INLINE_RING xge_hal_ring_rxd_priv_t*
__hal_ring_rxd_priv(xge_hal_ring_t *ring, xge_hal_dtr_h dtrh);

/* =========================== RING PUBLIC API ============================ */

__HAL_STATIC_RING __HAL_INLINE_RING xge_hal_status_e
xge_hal_ring_dtr_reserve(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh);

__HAL_STATIC_RING __HAL_INLINE_RING void*
xge_hal_ring_dtr_private(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_1b_set(xge_hal_dtr_h dtrh, dma_addr_t dma_pointer, int size);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_info_get(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	        xge_hal_dtr_info_t *ext_info);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_1b_get(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	        dma_addr_t *dma_pointer, int *pkt_length);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_3b_set(xge_hal_dtr_h dtrh, dma_addr_t dma_pointers[],
	        int sizes[]);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_3b_get(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	        dma_addr_t dma_pointers[], int sizes[]);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_5b_set(xge_hal_dtr_h dtrh, dma_addr_t dma_pointers[],
	        int sizes[]);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_5b_get(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	        dma_addr_t dma_pointer[], int sizes[]);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_post(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_pre_post(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_post_post(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_post_post_wmb(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_RING __HAL_INLINE_RING xge_hal_status_e
xge_hal_ring_dtr_next_completed(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh,
	        u8 *t_code);

__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_free(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_RING __HAL_INLINE_RING xge_hal_status_e
xge_hal_ring_is_next_dtr_completed(xge_hal_channel_h channelh);

#else /* XGE_FASTPATH_EXTERN */
#define __HAL_STATIC_RING static
#define __HAL_INLINE_RING inline
#include <dev/nxge/xgehal/xgehal-ring-fp.c>
#endif /* XGE_FASTPATH_INLINE */

__EXTERN_END_DECLS

#endif /* XGE_HAL_RING_H */
