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
 * @file iavf_lib.h
 * @brief header for structures and functions common to legacy and iflib
 *
 * Contains definitions and function declarations which are shared between the
 * legacy and iflib driver implementation.
 */
#ifndef _IAVF_LIB_H_
#define _IAVF_LIB_H_

#include <sys/malloc.h>
#include <sys/stdarg.h>
#include <sys/sysctl.h>
#ifdef RSS
#include <net/rss_config.h>
#endif

#include "iavf_debug.h"
#include "iavf_osdep.h"
#include "iavf_type.h"
#include "iavf_prototype.h"

MALLOC_DECLARE(M_IAVF);

/*
 * Ring Descriptors Valid Range: 32-4096 Default Value: 1024 This value is the
 * number of tx/rx descriptors allocated by the driver. Increasing this
 * value allows the driver to queue more operations.
 *
 * Tx descriptors are always 16 bytes, but Rx descriptors can be 32 bytes.
 * The driver currently always uses 32 byte Rx descriptors.
 */
#define IAVF_DEFAULT_RING	1024
#define IAVF_MAX_RING		4096
#define IAVF_MIN_RING		64
#define IAVF_RING_INCREMENT	32

#define IAVF_AQ_LEN		256
#define IAVF_AQ_LEN_MAX		1024

/*
** Default number of entries in Tx queue buf_ring.
*/
#define DEFAULT_TXBRSZ		4096

/* Alignment for rings */
#define DBA_ALIGN		128

/*
 * Max number of multicast MAC addrs added to the driver's
 * internal lists before converting to promiscuous mode
 */
#define MAX_MULTICAST_ADDR	128

/* Byte alignment for Tx/Rx descriptor rings */
#define DBA_ALIGN		128

#define IAVF_MSIX_BAR		3
#define IAVF_ADM_LIMIT		2
#define IAVF_TSO_SIZE		((255*1024)-1)
#define IAVF_AQ_BUF_SZ		((u32) 4096)
#define IAVF_RX_HDR		128
#define IAVF_RX_LIMIT		512
#define IAVF_RX_ITR		0
#define IAVF_TX_ITR		1
/**
 * The maximum packet length allowed to be sent or received by the adapter.
 */
#define IAVF_MAX_FRAME		9728
/**
 * The minimum packet length allowed to be sent by the adapter.
 */
#define IAVF_MIN_FRAME		17
#define IAVF_MAX_TX_SEGS	8
#define IAVF_MAX_RX_SEGS	5
#define IAVF_MAX_TSO_SEGS	128
#define IAVF_SPARSE_CHAIN	7
#define IAVF_MIN_TSO_MSS	64
#define IAVF_MAX_TSO_MSS	9668
#define IAVF_MAX_DMA_SEG_SIZE	((16 * 1024) - 1)
#define IAVF_AQ_MAX_ERR		30
#define IAVF_MAX_INIT_WAIT	120
#define IAVF_AQ_TIMEOUT		(1 * hz)
#define IAVF_ADV_LINK_SPEED_SCALE	((u64)1000000)
#define IAVF_MAX_DIS_Q_RETRY	10

#define IAVF_RSS_KEY_SIZE_REG		13
#define IAVF_RSS_KEY_SIZE		(IAVF_RSS_KEY_SIZE_REG * 4)
#define IAVF_RSS_VSI_LUT_SIZE		64	/* X722 -> VSI, X710 -> VF */
#define IAVF_RSS_VSI_LUT_ENTRY_MASK	0x3F
#define IAVF_RSS_VF_LUT_ENTRY_MASK	0xF

/* Maximum MTU size */
#define IAVF_MAX_MTU (IAVF_MAX_FRAME - \
		     ETHER_HDR_LEN - ETHER_CRC_LEN - ETHER_VLAN_ENCAP_LEN)

/*
 * Hardware requires that TSO packets have an segment size of at least 64
 * bytes. To avoid sending bad frames to the hardware, the driver forces the
 * MSS for all TSO packets to have a segment size of at least 64 bytes.
 *
 * However, if the MTU is reduced below a certain size, then the resulting
 * larger MSS can result in transmitting segmented frames with a packet size
 * larger than the MTU.
 *
 * Avoid this by preventing the MTU from being lowered below this limit.
 * Alternative solutions require changing the TCP stack to disable offloading
 * the segmentation when the requested segment size goes below 64 bytes.
 */
#define IAVF_MIN_MTU 112

/*
 * Interrupt Moderation parameters
 * Multiply ITR values by 2 for real ITR value
 */
#define IAVF_MAX_ITR		0x0FF0
#define IAVF_ITR_100K		0x0005
#define IAVF_ITR_20K		0x0019
#define IAVF_ITR_8K		0x003E
#define IAVF_ITR_4K		0x007A
#define IAVF_ITR_1K		0x01F4
#define IAVF_ITR_DYNAMIC	0x8000
#define IAVF_LOW_LATENCY	0
#define IAVF_AVE_LATENCY	1
#define IAVF_BULK_LATENCY	2

/* MacVlan Flags */
#define IAVF_FILTER_USED	(u16)(1 << 0)
#define IAVF_FILTER_VLAN	(u16)(1 << 1)
#define IAVF_FILTER_ADD		(u16)(1 << 2)
#define IAVF_FILTER_DEL		(u16)(1 << 3)
#define IAVF_FILTER_MC		(u16)(1 << 4)
/* used in the vlan field of the filter when not a vlan */
#define IAVF_VLAN_ANY		-1

#define CSUM_OFFLOAD_IPV4	(CSUM_IP|CSUM_TCP|CSUM_UDP|CSUM_SCTP)
#define CSUM_OFFLOAD_IPV6	(CSUM_TCP_IPV6|CSUM_UDP_IPV6|CSUM_SCTP_IPV6)
#define CSUM_OFFLOAD		(CSUM_OFFLOAD_IPV4|CSUM_OFFLOAD_IPV6|CSUM_TSO)

/* Misc flags for iavf_vsi.flags */
#define IAVF_FLAGS_KEEP_TSO4	(1 << 0)
#define IAVF_FLAGS_KEEP_TSO6	(1 << 1)

#define IAVF_DEFAULT_RSS_HENA_BASE (\
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_UDP) |	\
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_TCP) |	\
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_SCTP) |	\
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_OTHER) |	\
	BIT_ULL(IAVF_FILTER_PCTYPE_FRAG_IPV4) |		\
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_UDP) |	\
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_TCP) |	\
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_SCTP) |	\
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_OTHER) |	\
	BIT_ULL(IAVF_FILTER_PCTYPE_FRAG_IPV6))

#define IAVF_DEFAULT_ADV_RSS_HENA (\
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK))

#define IAVF_DEFAULT_RSS_HENA_XL710 (\
	IAVF_DEFAULT_RSS_HENA_BASE |			\
	BIT_ULL(IAVF_FILTER_PCTYPE_L2_PAYLOAD))

#define IAVF_DEFAULT_RSS_HENA_X722 (\
	IAVF_DEFAULT_RSS_HENA_XL710 |			\
	IAVF_DEFAULT_ADV_RSS_HENA)

#define IAVF_DEFAULT_RSS_HENA_AVF (\
	IAVF_DEFAULT_RSS_HENA_BASE |			\
	IAVF_DEFAULT_ADV_RSS_HENA)

/* For stats sysctl naming */
#define IAVF_QUEUE_NAME_LEN 32

#define IAVF_FLAG_AQ_ENABLE_QUEUES            (u32)(1 << 0)
#define IAVF_FLAG_AQ_DISABLE_QUEUES           (u32)(1 << 1)
#define IAVF_FLAG_AQ_ADD_MAC_FILTER           (u32)(1 << 2)
#define IAVF_FLAG_AQ_ADD_VLAN_FILTER          (u32)(1 << 3)
#define IAVF_FLAG_AQ_DEL_MAC_FILTER           (u32)(1 << 4)
#define IAVF_FLAG_AQ_DEL_VLAN_FILTER          (u32)(1 << 5)
#define IAVF_FLAG_AQ_CONFIGURE_QUEUES         (u32)(1 << 6)
#define IAVF_FLAG_AQ_MAP_VECTORS              (u32)(1 << 7)
#define IAVF_FLAG_AQ_HANDLE_RESET             (u32)(1 << 8)
#define IAVF_FLAG_AQ_CONFIGURE_PROMISC        (u32)(1 << 9)
#define IAVF_FLAG_AQ_GET_STATS                (u32)(1 << 10)
#define IAVF_FLAG_AQ_CONFIG_RSS_KEY           (u32)(1 << 11)
#define IAVF_FLAG_AQ_SET_RSS_HENA             (u32)(1 << 12)
#define IAVF_FLAG_AQ_GET_RSS_HENA_CAPS        (u32)(1 << 13)
#define IAVF_FLAG_AQ_CONFIG_RSS_LUT           (u32)(1 << 14)

#define IAVF_CAP_ADV_LINK_SPEED(_sc) \
    ((_sc)->vf_res->vf_cap_flags & VIRTCHNL_VF_CAP_ADV_LINK_SPEED)

#define IAVF_NRXQS(_vsi) ((_vsi)->num_rx_queues)
#define IAVF_NTXQS(_vsi) ((_vsi)->num_tx_queues)

/**
 * printf %b flag args
 */
#define IAVF_FLAGS \
    "\20\1ENABLE_QUEUES\2DISABLE_QUEUES\3ADD_MAC_FILTER" \
    "\4ADD_VLAN_FILTER\5DEL_MAC_FILTER\6DEL_VLAN_FILTER" \
    "\7CONFIGURE_QUEUES\10MAP_VECTORS\11HANDLE_RESET" \
    "\12CONFIGURE_PROMISC\13GET_STATS\14CONFIG_RSS_KEY" \
    "\15SET_RSS_HENA\16GET_RSS_HENA_CAPS\17CONFIG_RSS_LUT"
/**
 * printf %b flag args for offloads from virtchnl.h
 */
#define IAVF_PRINTF_VF_OFFLOAD_FLAGS \
    "\20\1L2" \
    "\2IWARP" \
    "\3FCOE" \
    "\4RSS_AQ" \
    "\5RSS_REG" \
    "\6WB_ON_ITR" \
    "\7REQ_QUEUES" \
    "\10ADV_LINK_SPEED" \
    "\21VLAN" \
    "\22RX_POLLING" \
    "\23RSS_PCTYPE_V2" \
    "\24RSS_PF" \
    "\25ENCAP" \
    "\26ENCAP_CSUM" \
    "\27RX_ENCAP_CSUM" \
    "\30ADQ"

/**
 * @enum iavf_ext_link_speed
 * @brief Extended link speed enumeration
 *
 * Enumeration of possible link speeds that the device could be operating in.
 * Contains an extended list compared to the virtchnl_link_speed, including
 * additional higher speeds such as 50GB, and 100GB.
 *
 * The enumeration is used to convert between the old virtchnl_link_speed, the
 * newer advanced speed reporting value specified in Mb/s, and the ifmedia
 * link speeds reported to the operating system.
 */
enum iavf_ext_link_speed {
	IAVF_EXT_LINK_SPEED_UNKNOWN,
	IAVF_EXT_LINK_SPEED_10MB,
	IAVF_EXT_LINK_SPEED_100MB,
	IAVF_EXT_LINK_SPEED_1000MB,
	IAVF_EXT_LINK_SPEED_2500MB,
	IAVF_EXT_LINK_SPEED_5GB,
	IAVF_EXT_LINK_SPEED_10GB,
	IAVF_EXT_LINK_SPEED_20GB,
	IAVF_EXT_LINK_SPEED_25GB,
	IAVF_EXT_LINK_SPEED_40GB,
	IAVF_EXT_LINK_SPEED_50GB,
	IAVF_EXT_LINK_SPEED_100GB,
};

/**
 * @struct iavf_sysctl_info
 * @brief sysctl statistic info
 *
 * Structure describing a single statistics sysctl, used for reporting
 * specific hardware and software statistics via the sysctl interface.
 */
struct iavf_sysctl_info {
	u64	*stat;
	char	*name;
	char	*description;
};

/* Forward struct declarations */
struct iavf_sc;
struct iavf_vsi;

/**
 * @enum iavf_state
 * @brief Driver state flags
 *
 * Used to indicate the status of various driver events. Intended to be
 * modified only using atomic operations, so that we can use it even in places
 * which aren't locked.
 */
enum iavf_state {
	IAVF_STATE_INITIALIZED,
	IAVF_STATE_RESET_REQUIRED,
	IAVF_STATE_RESET_PENDING,
	IAVF_STATE_RUNNING,
	/* This entry must be last */
	IAVF_STATE_LAST,
};

/* Functions for setting and checking driver state. Note the functions take
 * bit positions, not bitmasks. The atomic_testandset_32 and
 * atomic_testandclear_32 operations require bit positions, while the
 * atomic_set_32 and atomic_clear_32 require bitmasks. This can easily lead to
 * programming error, so we provide wrapper functions to avoid this.
 */

/**
 * iavf_set_state - Set the specified state
 * @s: the state bitmap
 * @bit: the state to set
 *
 * Atomically update the state bitmap with the specified bit set.
 */
static inline void
iavf_set_state(volatile u32 *s, enum iavf_state bit)
{
	/* atomic_set_32 expects a bitmask */
	atomic_set_32(s, BIT(bit));
}

/**
 * iavf_clear_state - Clear the specified state
 * @s: the state bitmap
 * @bit: the state to clear
 *
 * Atomically update the state bitmap with the specified bit cleared.
 */
static inline void
iavf_clear_state(volatile u32 *s, enum iavf_state bit)
{
	/* atomic_clear_32 expects a bitmask */
	atomic_clear_32(s, BIT(bit));
}

/**
 * iavf_testandset_state - Test and set the specified state
 * @s: the state bitmap
 * @bit: the bit to test
 *
 * Atomically update the state bitmap, setting the specified bit.
 *
 * @returns the previous value of the bit.
 */
static inline u32
iavf_testandset_state(volatile u32 *s, enum iavf_state bit)
{
	/* atomic_testandset_32 expects a bit position */
	return atomic_testandset_32(s, bit);
}

/**
 * iavf_testandclear_state - Test and clear the specified state
 * @s: the state bitmap
 * @bit: the bit to test
 *
 * Atomically update the state bitmap, clearing the specified bit.
 *
 * @returns the previous value of the bit.
 */
static inline u32
iavf_testandclear_state(volatile u32 *s, enum iavf_state bit)
{
	/* atomic_testandclear_32 expects a bit position */
	return atomic_testandclear_32(s, bit);
}

/**
 * iavf_test_state - Test the specified state
 * @s: the state bitmap
 * @bit: the bit to test
 *
 * @returns true if the state is set, false otherwise.
 *
 * @remark Use this only if the flow does not need to update the state. If you
 * must update the state as well, prefer iavf_testandset_state or
 * iavf_testandclear_state.
 */
static inline u32
iavf_test_state(volatile u32 *s, enum iavf_state bit)
{
	return (*s & BIT(bit)) ? true : false;
}

/**
 * cmp_etheraddr - Compare two ethernet addresses
 * @ea1: first ethernet address
 * @ea2: second ethernet address
 *
 * Compares two ethernet addresses.
 *
 * @returns true if the addresses are equal, false otherwise.
 */
static inline bool
cmp_etheraddr(const u8 *ea1, const u8 *ea2)
{
	bool cmp = FALSE;

	if ((ea1[0] == ea2[0]) && (ea1[1] == ea2[1]) &&
	    (ea1[2] == ea2[2]) && (ea1[3] == ea2[3]) &&
	    (ea1[4] == ea2[4]) && (ea1[5] == ea2[5]))
		cmp = TRUE;

	return (cmp);
}

int iavf_send_vc_msg(struct iavf_sc *sc, u32 op);
int iavf_send_vc_msg_sleep(struct iavf_sc *sc, u32 op);
void iavf_update_link_status(struct iavf_sc *);
bool iavf_driver_is_detaching(struct iavf_sc *sc);
void iavf_msec_pause(int msecs);
void iavf_get_default_rss_key(u32 *key);
int iavf_allocate_pci_resources_common(struct iavf_sc *sc);
int iavf_reset_complete(struct iavf_hw *hw);
int iavf_setup_vc(struct iavf_sc *sc);
int iavf_reset(struct iavf_sc *sc);
void iavf_enable_adminq_irq(struct iavf_hw *hw);
void iavf_disable_adminq_irq(struct iavf_hw *hw);
int iavf_vf_config(struct iavf_sc *sc);
void iavf_print_device_info(struct iavf_sc *sc);
int iavf_get_vsi_res_from_vf_res(struct iavf_sc *sc);
void iavf_set_mac_addresses(struct iavf_sc *sc);
void iavf_init_filters(struct iavf_sc *sc);
void iavf_free_filters(struct iavf_sc *sc);
void iavf_add_device_sysctls_common(struct iavf_sc *sc);
void iavf_configure_tx_itr(struct iavf_sc *sc);
void iavf_configure_rx_itr(struct iavf_sc *sc);
struct sysctl_oid_list *
    iavf_create_debug_sysctl_tree(struct iavf_sc *sc);
void iavf_add_debug_sysctls_common(struct iavf_sc *sc,
    struct sysctl_oid_list *debug_list);
void iavf_add_vsi_sysctls(device_t dev, struct iavf_vsi *vsi,
    struct sysctl_ctx_list *ctx, const char *sysctl_name);
void iavf_add_sysctls_eth_stats(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct iavf_eth_stats *eth_stats);
void iavf_media_status_common(struct iavf_sc *sc,
    struct ifmediareq *ifmr);
int iavf_media_change_common(if_t ifp);
void iavf_set_initial_baudrate(if_t ifp);
u64 iavf_max_vc_speed_to_value(u8 link_speeds);
void iavf_config_rss_reg(struct iavf_sc *sc);
void iavf_config_rss_pf(struct iavf_sc *sc);
void iavf_config_rss(struct iavf_sc *sc);
int iavf_config_promisc(struct iavf_sc *sc, int flags);
void iavf_init_multi(struct iavf_sc *sc);
void iavf_multi_set(struct iavf_sc *sc);
int iavf_add_mac_filter(struct iavf_sc *sc, u8 *macaddr, u16 flags);
struct iavf_mac_filter *
    iavf_find_mac_filter(struct iavf_sc *sc, u8 *macaddr);
struct iavf_mac_filter *
    iavf_get_mac_filter(struct iavf_sc *sc);
u64 iavf_baudrate_from_link_speed(struct iavf_sc *sc);
void iavf_add_vlan_filter(struct iavf_sc *sc, u16 vtag);
int iavf_mark_del_vlan_filter(struct iavf_sc *sc, u16 vtag);
void iavf_disable_queues_with_retries(struct iavf_sc *);

int iavf_sysctl_current_speed(SYSCTL_HANDLER_ARGS);
int iavf_sysctl_tx_itr(SYSCTL_HANDLER_ARGS);
int iavf_sysctl_rx_itr(SYSCTL_HANDLER_ARGS);
int iavf_sysctl_sw_filter_list(SYSCTL_HANDLER_ARGS);

#endif /* _IAVF_LIB_H_ */
