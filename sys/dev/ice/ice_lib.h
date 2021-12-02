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
/*$FreeBSD$*/

/**
 * @file ice_lib.h
 * @brief header for generic device and sysctl functions
 *
 * Contains definitions and function declarations for the ice_lib.c file. It
 * does not depend on the iflib networking stack.
 */

#ifndef _ICE_LIB_H_
#define _ICE_LIB_H_

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/module.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <sys/bitstring.h>

#include "ice_dcb.h"
#include "ice_type.h"
#include "ice_common.h"
#include "ice_flow.h"
#include "ice_sched.h"
#include "ice_resmgr.h"

#include "ice_rdma_internal.h"

#include "ice_rss.h"

/* Hide debug sysctls unless INVARIANTS is enabled */
#ifdef INVARIANTS
#define ICE_CTLFLAG_DEBUG 0
#else
#define ICE_CTLFLAG_DEBUG CTLFLAG_SKIP
#endif

/**
 * for_each_set_bit - For loop over each set bit in a bit string
 * @bit: storage for the bit index
 * @data: address of data block to loop over
 * @nbits: maximum number of bits to loop over
 *
 * macro to create a for loop over a bit string, which runs the body once for
 * each bit that is set in the string. The bit variable will be set to the
 * index of each set bit in the string, with zero representing the first bit.
 */
#define for_each_set_bit(bit, data, nbits) \
	for (bit_ffs((bitstr_t *)(data), (nbits), &(bit)); \
	     (bit) != -1; \
	     bit_ffs_at((bitstr_t *)(data), (bit) + 1, (nbits), &(bit)))

/**
 * @var broadcastaddr
 * @brief broadcast MAC address
 *
 * constant defining the broadcast MAC address, used for programming the
 * broadcast address as a MAC filter for the PF VSI.
 */
static const u8 broadcastaddr[ETHER_ADDR_LEN] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

MALLOC_DECLARE(M_ICE);

extern const char ice_driver_version[];
extern const uint8_t ice_major_version;
extern const uint8_t ice_minor_version;
extern const uint8_t ice_patch_version;
extern const uint8_t ice_rc_version;

/* global sysctl indicating whether the Tx FC filter should be enabled */
extern bool ice_enable_tx_fc_filter;

/* global sysctl indicating whether the Tx LLDP filter should be enabled */
extern bool ice_enable_tx_lldp_filter;

/* global sysctl indicating whether FW health status events should be enabled */
extern bool ice_enable_health_events;

/**
 * @struct ice_bar_info
 * @brief PCI BAR mapping information
 *
 * Contains data about a PCI BAR that the driver has mapped for use.
 */
struct ice_bar_info {
	struct resource		*res;
	bus_space_tag_t		tag;
	bus_space_handle_t	handle;
	bus_size_t		size;
	int			rid;
};

/* Alignment for queues */
#define DBA_ALIGN		128

/* Maximum TSO size is (256K)-1 */
#define ICE_TSO_SIZE		((256*1024) - 1)

/* Minimum size for TSO MSS */
#define ICE_MIN_TSO_MSS		64

#define ICE_MAX_TX_SEGS		8
#define ICE_MAX_TSO_SEGS	128

#define ICE_MAX_DMA_SEG_SIZE	((16*1024) - 1)

#define ICE_MAX_RX_SEGS		5

#define ICE_MAX_TSO_HDR_SEGS	3

#define ICE_MSIX_BAR		3

#define ICE_MAX_DCB_TCS		8

#define ICE_DEFAULT_DESC_COUNT	1024
#define ICE_MAX_DESC_COUNT	8160
#define ICE_MIN_DESC_COUNT	64
#define ICE_DESC_COUNT_INCR	32

/* List of hardware offloads we support */
#define ICE_CSUM_OFFLOAD (CSUM_IP | CSUM_IP_TCP | CSUM_IP_UDP | CSUM_IP_SCTP |	\
			  CSUM_IP6_TCP| CSUM_IP6_UDP | CSUM_IP6_SCTP |		\
			  CSUM_IP_TSO | CSUM_IP6_TSO)

/* Macros to decide what kind of hardware offload to enable */
#define ICE_CSUM_TCP (CSUM_IP_TCP|CSUM_IP_TSO|CSUM_IP6_TSO|CSUM_IP6_TCP)
#define ICE_CSUM_UDP (CSUM_IP_UDP|CSUM_IP6_UDP)
#define ICE_CSUM_SCTP (CSUM_IP_SCTP|CSUM_IP6_SCTP)
#define ICE_CSUM_IP (CSUM_IP|CSUM_IP_TSO)

/* List of known RX CSUM offload flags */
#define ICE_RX_CSUM_FLAGS (CSUM_L3_CALC | CSUM_L3_VALID | CSUM_L4_CALC | \
			   CSUM_L4_VALID | CSUM_L5_CALC | CSUM_L5_VALID | \
			   CSUM_COALESCED)

/* List of interface capabilities supported by ice hardware */
#define ICE_FULL_CAPS \
	(IFCAP_TSO4 | IFCAP_TSO6 | \
	 IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6 | \
	 IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6 | \
	 IFCAP_VLAN_HWFILTER | IFCAP_VLAN_HWTSO | \
	 IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWCSUM | IFCAP_VLAN_HWTSO | \
	 IFCAP_VLAN_MTU | IFCAP_JUMBO_MTU | IFCAP_LRO)

/* Safe mode disables support for hardware checksums and TSO */
#define ICE_SAFE_CAPS \
	(ICE_FULL_CAPS & ~(IFCAP_HWCSUM | IFCAP_TSO | \
			   IFCAP_VLAN_HWTSO | IFCAP_VLAN_HWCSUM))

#define ICE_CAPS(sc) \
	(ice_is_bit_set(sc->feat_en, ICE_FEATURE_SAFE_MODE) ? ICE_SAFE_CAPS : ICE_FULL_CAPS)

/**
 * ICE_NVM_ACCESS
 * @brief Private ioctl command number for NVM access ioctls
 *
 * The ioctl command number used by NVM update for accessing the driver for
 * NVM access commands.
 */
#define ICE_NVM_ACCESS \
	(((((((('E' << 4) + '1') << 4) + 'K') << 4) + 'G') << 4) | 5)

#define ICE_AQ_LEN		1023
#define ICE_MBXQ_LEN		512
#define ICE_SBQ_LEN		512

#define ICE_CTRLQ_WORK_LIMIT 256

#define ICE_DFLT_TRAFFIC_CLASS BIT(0)

/* wait up to 50 microseconds for queue state change */
#define ICE_Q_WAIT_RETRY_LIMIT	5

#define ICE_UP_TABLE_TRANSLATE(val, i) \
		(((val) << ICE_AQ_VSI_UP_TABLE_UP##i##_S) & \
		ICE_AQ_VSI_UP_TABLE_UP##i##_M)

/*
 * For now, set this to the hardware maximum. Each function gets a smaller
 * number assigned to it in hw->func_caps.guar_num_vsi, though there
 * appears to be no guarantee that is the maximum number that a function
 * can use.
 */
#define ICE_MAX_VSI_AVAILABLE	768

/* Maximum size of a single frame (for Tx and Rx) */
#define ICE_MAX_FRAME_SIZE ICE_AQ_SET_MAC_FRAME_SIZE_MAX

/* Maximum MTU size */
#define ICE_MAX_MTU (ICE_MAX_FRAME_SIZE - \
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
#define ICE_MIN_MTU 112

#define ICE_DEFAULT_VF_QUEUES	4

/*
 * The maximum number of RX queues allowed per TC in a VSI.
 */
#define ICE_MAX_RXQS_PER_TC	256

/*
 * There are three settings that can be updated independently or
 * altogether: Link speed, FEC, and Flow Control.  These macros allow
 * the caller to specify which setting(s) to update.
 */
#define ICE_APPLY_LS        BIT(0)
#define ICE_APPLY_FEC       BIT(1)
#define ICE_APPLY_FC        BIT(2)
#define ICE_APPLY_LS_FEC    (ICE_APPLY_LS | ICE_APPLY_FEC)
#define ICE_APPLY_LS_FC     (ICE_APPLY_LS | ICE_APPLY_FC)
#define ICE_APPLY_FEC_FC    (ICE_APPLY_FEC | ICE_APPLY_FC)
#define ICE_APPLY_LS_FEC_FC (ICE_APPLY_LS_FEC | ICE_APPLY_FC)

/**
 * @enum ice_dyn_idx_t
 * @brief Dynamic Control ITR indexes
 *
 * This enum matches hardware bits and is meant to be used by DYN_CTLN
 * registers and QINT registers or more generally anywhere in the manual
 * mentioning ITR_INDX, ITR_NONE cannot be used as an index 'n' into any
 * register but instead is a special value meaning "don't update" ITR0/1/2.
 */
enum ice_dyn_idx_t {
	ICE_IDX_ITR0 = 0,
	ICE_IDX_ITR1 = 1,
	ICE_IDX_ITR2 = 2,
	ICE_ITR_NONE = 3	/* ITR_NONE must not be used as an index */
};

/* By convenction ITR0 is used for RX, and ITR1 is used for TX */
#define ICE_RX_ITR ICE_IDX_ITR0
#define ICE_TX_ITR ICE_IDX_ITR1

#define ICE_ITR_MAX		8160

/* Define the default Tx and Rx ITR as 50us (translates to ~20k int/sec max) */
#define ICE_DFLT_TX_ITR		50
#define ICE_DFLT_RX_ITR		50

/**
 * ice_itr_to_reg - Convert an ITR setting into its register equivalent
 * @hw: The device HW structure
 * @itr_setting: the ITR setting to convert
 *
 * Based on the hardware ITR granularity, convert an ITR setting into the
 * correct value to prepare programming to the HW.
 */
static inline u16 ice_itr_to_reg(struct ice_hw *hw, u16 itr_setting)
{
	return itr_setting / hw->itr_gran;
}

/**
 * @enum ice_rx_dtype
 * @brief DTYPE header split options
 *
 * This enum matches the Rx context bits to define whether header split is
 * enabled or not.
 */
enum ice_rx_dtype {
	ICE_RX_DTYPE_NO_SPLIT		= 0,
	ICE_RX_DTYPE_HEADER_SPLIT	= 1,
	ICE_RX_DTYPE_SPLIT_ALWAYS	= 2,
};

/* Strings used for displaying FEC mode
 *
 * Use ice_fec_str() to get these unless these need to be embedded in a
 * string constant.
 */
#define ICE_FEC_STRING_AUTO	"Auto"
#define ICE_FEC_STRING_RS	"RS-FEC"
#define ICE_FEC_STRING_BASER	"FC-FEC/BASE-R"
#define ICE_FEC_STRING_NONE	"None"

/* Strings used for displaying Flow Control mode
 *
 * Use ice_fc_str() to get these unless these need to be embedded in a
 * string constant.
 */
#define ICE_FC_STRING_FULL	"Full"
#define ICE_FC_STRING_TX	"Tx"
#define ICE_FC_STRING_RX	"Rx"
#define ICE_FC_STRING_NONE	"None"

/*
 * The number of times the ice_handle_i2c_req function will retry reading
 * I2C data via the Admin Queue before returning EBUSY.
 */
#define ICE_I2C_MAX_RETRIES		10

/*
 * The Start LLDP Agent AQ command will fail if it's sent too soon after
 * the LLDP agent is stopped. The period between the stop and start
 * commands must currently be at least 2 seconds.
 */
#define ICE_START_LLDP_RETRY_WAIT	(2 * hz)

/*
 * The ice_(set|clear)_vsi_promisc() function expects a mask of promiscuous
 * modes to operate on. This mask is the default one for the driver, where
 * promiscuous is enabled/disabled for all types of non-VLAN-tagged/VLAN 0
 * traffic.
 */
#define ICE_VSI_PROMISC_MASK		(ICE_PROMISC_UCAST_TX | \
					 ICE_PROMISC_UCAST_RX | \
					 ICE_PROMISC_MCAST_TX | \
					 ICE_PROMISC_MCAST_RX)

struct ice_softc;

/**
 * @enum ice_rx_cso_stat
 * @brief software checksum offload statistics
 *
 * Enumeration of possible checksum offload statistics captured by software
 * during the Rx path.
 */
enum ice_rx_cso_stat {
	ICE_CSO_STAT_RX_IP4_ERR,
	ICE_CSO_STAT_RX_IP6_ERR,
	ICE_CSO_STAT_RX_L3_ERR,
	ICE_CSO_STAT_RX_TCP_ERR,
	ICE_CSO_STAT_RX_UDP_ERR,
	ICE_CSO_STAT_RX_SCTP_ERR,
	ICE_CSO_STAT_RX_L4_ERR,
	ICE_CSO_STAT_RX_COUNT
};

/**
 * @enum ice_tx_cso_stat
 * @brief software checksum offload statistics
 *
 * Enumeration of possible checksum offload statistics captured by software
 * during the Tx path.
 */
enum ice_tx_cso_stat {
	ICE_CSO_STAT_TX_TCP,
	ICE_CSO_STAT_TX_UDP,
	ICE_CSO_STAT_TX_SCTP,
	ICE_CSO_STAT_TX_IP4,
	ICE_CSO_STAT_TX_IP6,
	ICE_CSO_STAT_TX_L3_ERR,
	ICE_CSO_STAT_TX_L4_ERR,
	ICE_CSO_STAT_TX_COUNT
};

/**
 * @struct tx_stats
 * @brief software Tx statistics
 *
 * Contains software counted Tx statistics for a single queue
 */
struct tx_stats {
	/* Soft Stats */
	u64			tx_bytes;
	u64			tx_packets;
	u64			mss_too_small;
	u64			cso[ICE_CSO_STAT_TX_COUNT];
};

/**
 * @struct rx_stats
 * @brief software Rx statistics
 *
 * Contains software counted Rx statistics for a single queue
 */
struct rx_stats {
	/* Soft Stats */
	u64			rx_packets;
	u64			rx_bytes;
	u64			desc_errs;
	u64			cso[ICE_CSO_STAT_RX_COUNT];
};

/**
 * @struct ice_vsi_hw_stats
 * @brief hardware statistics for a VSI
 *
 * Stores statistics that are generated by hardware for a VSI.
 */
struct ice_vsi_hw_stats {
	struct ice_eth_stats prev;
	struct ice_eth_stats cur;
	bool offsets_loaded;
};

/**
 * @struct ice_pf_hw_stats
 * @brief hardware statistics for a PF
 *
 * Stores statistics that are generated by hardware for each PF.
 */
struct ice_pf_hw_stats {
	struct ice_hw_port_stats prev;
	struct ice_hw_port_stats cur;
	bool offsets_loaded;
};

/**
 * @struct ice_pf_sw_stats
 * @brief software statistics for a PF
 *
 * Contains software generated statistics relevant to a PF.
 */
struct ice_pf_sw_stats {
	/* # of reset events handled, by type */
	u32 corer_count;
	u32 globr_count;
	u32 empr_count;
	u32 pfr_count;

	/* # of detected MDD events for Tx and Rx */
	u32 tx_mdd_count;
	u32 rx_mdd_count;
};

/**
 * @struct ice_tc_info
 * @brief Traffic class information for a VSI
 *
 * Stores traffic class information used in configuring
 * a VSI.
 */
struct ice_tc_info {
	u16 qoffset;	/* Offset in VSI queue space */
	u16 qcount_tx;	/* TX queues for this Traffic Class */
	u16 qcount_rx;	/* RX queues */
};

/**
 * @struct ice_vsi
 * @brief VSI structure
 *
 * Contains data relevant to a single VSI
 */
struct ice_vsi {
	/* back pointer to the softc */
	struct ice_softc	*sc;

	bool dynamic;		/* if true, dynamically allocated */

	enum ice_vsi_type type;	/* type of this VSI */
	u16 idx;		/* software index to sc->all_vsi[] */

	u16 *tx_qmap; /* Tx VSI to PF queue mapping */
	u16 *rx_qmap; /* Rx VSI to PF queue mapping */

	bitstr_t *vmap; /* Vector(s) assigned to VSI */

	enum ice_resmgr_alloc_type qmap_type;

	struct ice_tx_queue *tx_queues;	/* Tx queue array */
	struct ice_rx_queue *rx_queues;	/* Rx queue array */

	int num_tx_queues;
	int num_rx_queues;
	int num_vectors;

	int16_t rx_itr;
	int16_t tx_itr;

	/* RSS configuration */
	u16 rss_table_size; /* HW RSS table size */
	u8 rss_lut_type; /* Used to configure Get/Set RSS LUT AQ call */

	int max_frame_size;
	u16 mbuf_sz;

	struct ice_aqc_vsi_props info;

	/* DCB configuration */
	u8 num_tcs;	/* Total number of enabled TCs */
	u16 tc_map;	/* bitmap of enabled Traffic Classes */
	/* Information for each traffic class */
	struct ice_tc_info tc_info[ICE_MAX_TRAFFIC_CLASS];

	/* context for per-VSI sysctls */
	struct sysctl_ctx_list ctx;
	struct sysctl_oid *vsi_node;

	/* context for per-txq sysctls */
	struct sysctl_ctx_list txqs_ctx;
	struct sysctl_oid *txqs_node;

	/* context for per-rxq sysctls */
	struct sysctl_ctx_list rxqs_ctx;
	struct sysctl_oid *rxqs_node;

	/* VSI-level stats */
	struct ice_vsi_hw_stats hw_stats;
};

/**
 * @enum ice_state
 * @brief Driver state flags
 *
 * Used to indicate the status of various driver events. Intended to be
 * modified only using atomic operations, so that we can use it even in places
 * which aren't locked.
 */
enum ice_state {
	ICE_STATE_CONTROLQ_EVENT_PENDING,
	ICE_STATE_VFLR_PENDING,
	ICE_STATE_MDD_PENDING,
	ICE_STATE_RESET_OICR_RECV,
	ICE_STATE_RESET_PFR_REQ,
	ICE_STATE_PREPARED_FOR_RESET,
	ICE_STATE_RESET_FAILED,
	ICE_STATE_DRIVER_INITIALIZED,
	ICE_STATE_NO_MEDIA,
	ICE_STATE_RECOVERY_MODE,
	ICE_STATE_ROLLBACK_MODE,
	ICE_STATE_LINK_STATUS_REPORTED,
	ICE_STATE_ATTACHING,
	ICE_STATE_DETACHING,
	ICE_STATE_LINK_DEFAULT_OVERRIDE_PENDING,
	ICE_STATE_LLDP_RX_FLTR_FROM_DRIVER,
	ICE_STATE_MULTIPLE_TCS,
	/* This entry must be last */
	ICE_STATE_LAST,
};

/* Functions for setting and checking driver state. Note the functions take
 * bit positions, not bitmasks. The atomic_testandset_32 and
 * atomic_testandclear_32 operations require bit positions, while the
 * atomic_set_32 and atomic_clear_32 require bitmasks. This can easily lead to
 * programming error, so we provide wrapper functions to avoid this.
 */

/**
 * ice_set_state - Set the specified state
 * @s: the state bitmap
 * @bit: the state to set
 *
 * Atomically update the state bitmap with the specified bit set.
 */
static inline void
ice_set_state(volatile u32 *s, enum ice_state bit)
{
	/* atomic_set_32 expects a bitmask */
	atomic_set_32(s, BIT(bit));
}

/**
 * ice_clear_state - Clear the specified state
 * @s: the state bitmap
 * @bit: the state to clear
 *
 * Atomically update the state bitmap with the specified bit cleared.
 */
static inline void
ice_clear_state(volatile u32 *s, enum ice_state bit)
{
	/* atomic_clear_32 expects a bitmask */
	atomic_clear_32(s, BIT(bit));
}

/**
 * ice_testandset_state - Test and set the specified state
 * @s: the state bitmap
 * @bit: the bit to test
 *
 * Atomically update the state bitmap, setting the specified bit. Returns the
 * previous value of the bit.
 */
static inline u32
ice_testandset_state(volatile u32 *s, enum ice_state bit)
{
	/* atomic_testandset_32 expects a bit position */
	return atomic_testandset_32(s, bit);
}

/**
 * ice_testandclear_state - Test and clear the specified state
 * @s: the state bitmap
 * @bit: the bit to test
 *
 * Atomically update the state bitmap, clearing the specified bit. Returns the
 * previous value of the bit.
 */
static inline u32
ice_testandclear_state(volatile u32 *s, enum ice_state bit)
{
	/* atomic_testandclear_32 expects a bit position */
	return atomic_testandclear_32(s, bit);
}

/**
 * ice_test_state - Test the specified state
 * @s: the state bitmap
 * @bit: the bit to test
 *
 * Return true if the state is set, false otherwise. Use this only if the flow
 * does not need to update the state. If you must update the state as well,
 * prefer ice_testandset_state or ice_testandclear_state.
 */
static inline u32
ice_test_state(volatile u32 *s, enum ice_state bit)
{
	return (*s & BIT(bit)) ? true : false;
}

/**
 * @struct ice_str_buf
 * @brief static length buffer for string returning
 *
 * Structure containing a fixed size string buffer, used to implement
 * numeric->string conversion functions that may want to return non-constant
 * strings.
 *
 * This allows returning a fixed size string that is generated by a conversion
 * function, and then copied to the used location without needing to use an
 * explicit local variable passed by reference.
 */
struct ice_str_buf {
	char str[ICE_STR_BUF_LEN];
};

struct ice_str_buf _ice_aq_str(enum ice_aq_err aq_err);
struct ice_str_buf _ice_status_str(enum ice_status status);
struct ice_str_buf _ice_err_str(int err);
struct ice_str_buf _ice_fltr_flag_str(u16 flag);
struct ice_str_buf _ice_log_sev_str(u8 log_level);
struct ice_str_buf _ice_mdd_tx_tclan_str(u8 event);
struct ice_str_buf _ice_mdd_tx_pqm_str(u8 event);
struct ice_str_buf _ice_mdd_rx_str(u8 event);
struct ice_str_buf _ice_fw_lldp_status(u32 lldp_status);

#define ice_aq_str(err)		_ice_aq_str(err).str
#define ice_status_str(err)	_ice_status_str(err).str
#define ice_err_str(err)	_ice_err_str(err).str
#define ice_fltr_flag_str(flag)	_ice_fltr_flag_str(flag).str

#define ice_mdd_tx_tclan_str(event)	_ice_mdd_tx_tclan_str(event).str
#define ice_mdd_tx_pqm_str(event)	_ice_mdd_tx_pqm_str(event).str
#define ice_mdd_rx_str(event)		_ice_mdd_rx_str(event).str

#define ice_log_sev_str(log_level)	_ice_log_sev_str(log_level).str
#define ice_fw_lldp_status(lldp_status) _ice_fw_lldp_status(lldp_status).str

/**
 * ice_enable_intr - Enable interrupts for given vector
 * @hw: the device private HW structure
 * @vector: the interrupt index in PF space
 *
 * In MSI or Legacy interrupt mode, interrupt 0 is the only valid index.
 */
static inline void
ice_enable_intr(struct ice_hw *hw, int vector)
{
	u32 dyn_ctl;

	/* Use ITR_NONE so that ITR configuration is not changed. */
	dyn_ctl = GLINT_DYN_CTL_INTENA_M | GLINT_DYN_CTL_CLEARPBA_M |
		  (ICE_ITR_NONE << GLINT_DYN_CTL_ITR_INDX_S);
	wr32(hw, GLINT_DYN_CTL(vector), dyn_ctl);
}

/**
 * ice_disable_intr - Disable interrupts for given vector
 * @hw: the device private HW structure
 * @vector: the interrupt index in PF space
 *
 * In MSI or Legacy interrupt mode, interrupt 0 is the only valid index.
 */
static inline void
ice_disable_intr(struct ice_hw *hw, int vector)
{
	u32 dyn_ctl;

	/* Use ITR_NONE so that ITR configuration is not changed. */
	dyn_ctl = ICE_ITR_NONE << GLINT_DYN_CTL_ITR_INDX_S;
	wr32(hw, GLINT_DYN_CTL(vector), dyn_ctl);
}

/**
 * ice_is_tx_desc_done - determine if a Tx descriptor is done
 * @txd: the Tx descriptor to check
 *
 * Returns true if hardware is done with a Tx descriptor and software is
 * capable of re-using it.
 */
static inline bool
ice_is_tx_desc_done(struct ice_tx_desc *txd)
{
	return (((txd->cmd_type_offset_bsz & ICE_TXD_QW1_DTYPE_M)
		 >> ICE_TXD_QW1_DTYPE_S) == ICE_TX_DESC_DTYPE_DESC_DONE);
}

/**
 * ice_get_pf_id - Get the PF id from the hardware registers
 * @hw: the ice hardware structure
 *
 * Reads the PF_FUNC_RID register and extracts the function number from it.
 * Intended to be used in cases where hw->pf_id hasn't yet been assigned by
 * ice_init_hw.
 *
 * @pre this function should be called only after PCI register access has been
 * setup, and prior to ice_init_hw. After hardware has been initialized, the
 * cached hw->pf_id value can be used.
 */
static inline u8
ice_get_pf_id(struct ice_hw *hw)
{
	return (u8)((rd32(hw, PF_FUNC_RID) & PF_FUNC_RID_FUNCTION_NUMBER_M) >>
		    PF_FUNC_RID_FUNCTION_NUMBER_S);
}

/* Details of how to re-initialize depend on the networking stack */
void ice_request_stack_reinit(struct ice_softc *sc);

/* Details of how to check if the network stack is detaching us */
bool ice_driver_is_detaching(struct ice_softc *sc);

const char * ice_fw_module_str(enum ice_aqc_fw_logging_mod module);
void ice_add_fw_logging_tunables(struct ice_softc *sc,
				 struct sysctl_oid *parent);
void ice_handle_fw_log_event(struct ice_softc *sc, struct ice_aq_desc *desc,
			     void *buf);

int  ice_process_ctrlq(struct ice_softc *sc, enum ice_ctl_q q_type, u16 *pending);
int  ice_map_bar(device_t dev, struct ice_bar_info *bar, int bar_num);
void ice_free_bar(device_t dev, struct ice_bar_info *bar);
void ice_set_ctrlq_len(struct ice_hw *hw);
void ice_release_vsi(struct ice_vsi *vsi);
struct ice_vsi *ice_alloc_vsi(struct ice_softc *sc, enum ice_vsi_type type);
int  ice_alloc_vsi_qmap(struct ice_vsi *vsi, const int max_tx_queues,
		       const int max_rx_queues);
void ice_free_vsi_qmaps(struct ice_vsi *vsi);
int  ice_initialize_vsi(struct ice_vsi *vsi);
void ice_deinit_vsi(struct ice_vsi *vsi);
uint64_t ice_aq_speed_to_rate(struct ice_port_info *pi);
int  ice_get_phy_type_low(uint64_t phy_type_low);
int  ice_get_phy_type_high(uint64_t phy_type_high);
enum ice_status ice_add_media_types(struct ice_softc *sc, struct ifmedia *media);
void ice_configure_rxq_interrupts(struct ice_vsi *vsi);
void ice_configure_txq_interrupts(struct ice_vsi *vsi);
void ice_flush_rxq_interrupts(struct ice_vsi *vsi);
void ice_flush_txq_interrupts(struct ice_vsi *vsi);
int  ice_cfg_vsi_for_tx(struct ice_vsi *vsi);
int  ice_cfg_vsi_for_rx(struct ice_vsi *vsi);
int  ice_control_rx_queues(struct ice_vsi *vsi, bool enable);
int  ice_cfg_pf_default_mac_filters(struct ice_softc *sc);
int  ice_rm_pf_default_mac_filters(struct ice_softc *sc);
void ice_print_nvm_version(struct ice_softc *sc);
void ice_update_vsi_hw_stats(struct ice_vsi *vsi);
void ice_reset_vsi_stats(struct ice_vsi *vsi);
void ice_update_pf_stats(struct ice_softc *sc);
void ice_reset_pf_stats(struct ice_softc *sc);
void ice_add_device_sysctls(struct ice_softc *sc);
void ice_log_hmc_error(struct ice_hw *hw, device_t dev);
void ice_add_sysctls_eth_stats(struct sysctl_ctx_list *ctx,
			       struct sysctl_oid *parent,
			       struct ice_eth_stats *stats);
void ice_add_vsi_sysctls(struct ice_vsi *vsi);
void ice_add_sysctls_mac_stats(struct sysctl_ctx_list *ctx,
			       struct sysctl_oid *parent,
			       struct ice_hw_port_stats *stats);
void ice_configure_misc_interrupts(struct ice_softc *sc);
int  ice_sync_multicast_filters(struct ice_softc *sc);
enum ice_status ice_add_vlan_hw_filter(struct ice_vsi *vsi, u16 vid);
enum ice_status ice_remove_vlan_hw_filter(struct ice_vsi *vsi, u16 vid);
void ice_add_vsi_tunables(struct ice_vsi *vsi, struct sysctl_oid *parent);
void ice_del_vsi_sysctl_ctx(struct ice_vsi *vsi);
void ice_add_device_tunables(struct ice_softc *sc);
int  ice_add_vsi_mac_filter(struct ice_vsi *vsi, const u8 *addr);
int  ice_remove_vsi_mac_filter(struct ice_vsi *vsi, const u8 *addr);
int  ice_vsi_disable_tx(struct ice_vsi *vsi);
void ice_vsi_add_txqs_ctx(struct ice_vsi *vsi);
void ice_vsi_add_rxqs_ctx(struct ice_vsi *vsi);
void ice_vsi_del_txqs_ctx(struct ice_vsi *vsi);
void ice_vsi_del_rxqs_ctx(struct ice_vsi *vsi);
void ice_add_txq_sysctls(struct ice_tx_queue *txq);
void ice_add_rxq_sysctls(struct ice_rx_queue *rxq);
int  ice_config_rss(struct ice_vsi *vsi);
void ice_clean_all_vsi_rss_cfg(struct ice_softc *sc);
void ice_load_pkg_file(struct ice_softc *sc);
void ice_log_pkg_init(struct ice_softc *sc, enum ice_status *pkg_status);
uint64_t ice_get_ifnet_counter(struct ice_vsi *vsi, ift_counter counter);
void ice_save_pci_info(struct ice_hw *hw, device_t dev);
int  ice_replay_all_vsi_cfg(struct ice_softc *sc);
void ice_link_up_msg(struct ice_softc *sc);
int  ice_update_laa_mac(struct ice_softc *sc);
void ice_get_and_print_bus_info(struct ice_softc *sc);
const char *ice_fec_str(enum ice_fec_mode mode);
const char *ice_fc_str(enum ice_fc_mode mode);
const char *ice_fwd_act_str(enum ice_sw_fwd_act_type action);
const char *ice_state_to_str(enum ice_state state);
int  ice_init_link_events(struct ice_softc *sc);
void ice_configure_rx_itr(struct ice_vsi *vsi);
void ice_configure_tx_itr(struct ice_vsi *vsi);
void ice_setup_pf_vsi(struct ice_softc *sc);
void ice_handle_mdd_event(struct ice_softc *sc);
void ice_init_dcb_setup(struct ice_softc *sc);
int  ice_send_version(struct ice_softc *sc);
int  ice_cfg_pf_ethertype_filters(struct ice_softc *sc);
void ice_init_link_configuration(struct ice_softc *sc);
void ice_init_saved_phy_cfg(struct ice_softc *sc);
int  ice_apply_saved_phy_cfg(struct ice_softc *sc, u8 settings);
void ice_set_link_management_mode(struct ice_softc *sc);
int  ice_module_event_handler(module_t mod, int what, void *arg);
int  ice_handle_nvm_access_ioctl(struct ice_softc *sc, struct ifdrv *ifd);
int  ice_handle_i2c_req(struct ice_softc *sc, struct ifi2creq *req);
int  ice_read_sff_eeprom(struct ice_softc *sc, u16 dev_addr, u16 offset, u8* data, u16 length);
int  ice_alloc_intr_tracking(struct ice_softc *sc);
void ice_free_intr_tracking(struct ice_softc *sc);
void ice_set_default_local_lldp_mib(struct ice_softc *sc);
void ice_init_health_events(struct ice_softc *sc);
void ice_cfg_pba_num(struct ice_softc *sc);

#endif /* _ICE_LIB_H_ */
