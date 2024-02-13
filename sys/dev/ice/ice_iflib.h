/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
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
 * @file ice_iflib.h
 * @brief main header for the iflib driver implementation
 *
 * Contains the definitions for various structures used by the iflib driver
 * implementation, including the Tx and Rx queue structures and the ice_softc
 * structure.
 */

#ifndef _ICE_IFLIB_H_
#define _ICE_IFLIB_H_

/* include kernel options first */
#include "ice_opts.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/iflib.h>
#include "ifdi_if.h"

#include "ice_lib.h"
#include "ice_osdep.h"
#include "ice_resmgr.h"
#include "ice_type.h"
#include "ice_features.h"

/**
 * ASSERT_CTX_LOCKED - Assert that the iflib context lock is held
 * @sc: ice softc pointer
 *
 * Macro to trigger an assertion if the iflib context lock is not
 * currently held.
 */
#define ASSERT_CTX_LOCKED(sc) sx_assert((sc)->iflib_ctx_lock, SA_XLOCKED)

/**
 * IFLIB_CTX_LOCK - lock the iflib context lock
 * @sc: ice softc pointer
 *
 * Macro used to unlock the iflib context lock.
 */
#define IFLIB_CTX_LOCK(sc) sx_xlock((sc)->iflib_ctx_lock)

/**
 * IFLIB_CTX_UNLOCK - unlock the iflib context lock
 * @sc: ice softc pointer
 *
 * Macro used to unlock the iflib context lock.
 */
#define IFLIB_CTX_UNLOCK(sc) sx_xunlock((sc)->iflib_ctx_lock)

/**
 * ASSERT_CFG_LOCKED - Assert that a configuration lock is held
 * @sc: ice softc pointer
 *
 * Macro used by ice_lib.c to verify that certain functions are called while
 * holding a configuration lock. For the iflib implementation, this will be
 * the iflib context lock.
 */
#define ASSERT_CFG_LOCKED(sc) ASSERT_CTX_LOCKED(sc)

/**
 * ICE_IFLIB_MAX_DESC_COUNT - Maximum ring size for iflib
 *
 * The iflib stack currently requires that the ring size, or number of
 * descriptors, be a power of 2. The ice hardware is limited to a maximum of
 * 8160 descriptors, which is not quite 2^13. Limit the maximum ring size for
 * iflib to just 2^12 (4096).
 */
#define ICE_IFLIB_MAX_DESC_COUNT	4096

/**
 * @struct ice_irq_vector
 * @brief Driver irq vector structure
 *
 * ice_lib.c requires the following parameters
 * @me: the vector number
 *
 * Other parameters may be iflib driver specific
 *
 * The iflib driver uses a single hardware interrupt per Rx queue, and uses
 * software interrupts for the Tx queues.
 */
struct ice_irq_vector {
	u32			me;

	struct if_irq		irq;
};

/**
 * @struct ice_tx_queue
 * @brief Driver Tx queue structure
 *
 * ice_lib.c requires the following parameters:
 * @vsi: backpointer the VSI structure
 * @me: this queue's index into the queue array
 * @irqv: always NULL for iflib
 * @desc_count: the number of descriptors
 * @tx_paddr: the physical address for this queue
 * @q_teid: the Tx queue TEID returned from firmware
 * @stats: queue statistics
 * @tc: traffic class queue belongs to
 * @q_handle: qidx in tc; used in TXQ enable functions
 *
 * Other parameters may be iflib driver specific
 */
struct ice_tx_queue {
	struct ice_vsi		*vsi;
	struct ice_tx_desc	*tx_base;
	bus_addr_t		tx_paddr;
	struct tx_stats		stats;
	u64			tso;
	u16			desc_count;
	u32			tail;
	struct ice_irq_vector	*irqv;
	u32			q_teid;
	u32			me;
	u16			q_handle;
	u8			tc;

	/* descriptor writeback status */
	qidx_t			*tx_rsq;
	qidx_t			tx_rs_cidx;
	qidx_t			tx_rs_pidx;
	qidx_t			tx_cidx_processed;
};

/**
 * @struct ice_rx_queue
 * @brief Driver Rx queue structure
 *
 * ice_lib.c requires the following parameters:
 * @vsi: backpointer the VSI structure
 * @me: this queue's index into the queue array
 * @irqv: pointer to vector structure associated with this queue
 * @desc_count: the number of descriptors
 * @rx_paddr: the physical address for this queue
 * @tail: the tail register address for this queue
 * @stats: queue statistics
 * @tc: traffic class queue belongs to
 *
 * Other parameters may be iflib driver specific
 */
struct ice_rx_queue {
	struct ice_vsi			*vsi;
	union ice_32b_rx_flex_desc	*rx_base;
	bus_addr_t			rx_paddr;
	struct rx_stats			stats;
	u16				desc_count;
	u32				tail;
	struct ice_irq_vector		*irqv;
	u32				me;
	u8				tc;

	struct if_irq			que_irq;
};

/**
 * @struct ice_mirr_if
 * @brief structure representing a mirroring interface
 */
struct ice_mirr_if {
	struct ice_softc *back;
	struct ifnet *ifp;
	struct ice_vsi *vsi;

	device_t subdev;
	if_ctx_t subctx;
	if_softc_ctx_t subscctx;

	u16 num_irq_vectors;
	u16 *if_imap;
	u16 *os_imap;
	struct ice_irq_vector *rx_irqvs;

	u32 state;

	bool if_attached;
};

/**
 * @struct ice_softc
 * @brief main structure representing one device
 *
 * ice_lib.c requires the following parameters
 * @all_vsi: the array of all allocated VSIs
 * @debug_sysctls: sysctl node for debug sysctls
 * @dev: device_t pointer
 * @feat_en: bitmap of enabled driver features
 * @hw: embedded ice_hw structure
 * @ifp: pointer to the ifnet structure
 * @link_up: boolean indicating if link is up
 * @num_available_vsi: size of the VSI array
 * @pf_vsi: embedded VSI structure for the main PF VSI
 * @rx_qmgr: queue manager for Rx queues
 * @soft_stats: software statistics for this device
 * @state: driver state flags
 * @stats: hardware statistics for this device
 * @tx_qmgr: queue manager for Tx queues
 * @vsi_sysctls: sysctl node for all VSI sysctls
 * @enable_tx_fc_filter: boolean indicating if the Tx FC filter is enabled
 * @enable_tx_lldp_filter: boolean indicating if the Tx LLDP filter is enabled
 * @rebuild_ticks: indicates when a post-reset rebuild started
 * @imgr: resource manager for interrupt allocations
 * @pf_imap: interrupt mapping for PF LAN interrupts
 * @lan_vectors: # of vectors used by LAN driver (length of pf_imap)
 * @ldo_tlv: LAN Default Override settings from NVM
 *
 * ice_iov.c requires the following parameters (when PCI_IOV is defined):
 * @vfs: array of VF context structures
 * @num_vfs: number of VFs to use for SR-IOV
 *
 * The main representation for a single OS device, used to represent a single
 * physical function.
 */
struct ice_softc {
	struct ice_hw hw;
	struct ice_vsi pf_vsi;		/* Main PF VSI */

	char admin_mtx_name[16]; /* name of the admin mutex */
	struct mtx admin_mtx; /* mutex to protect the admin timer */
	struct callout admin_timer; /* timer to trigger admin task */

	/* iRDMA peer interface */
	struct ice_rdma_entry rdma_entry;
	int irdma_vectors;
	u16 *rdma_imap;

	struct ice_vsi **all_vsi;	/* Array of VSI pointers */
	u16 num_available_vsi;		/* Size of VSI array */

	struct sysctl_oid *vsi_sysctls;	/* Sysctl node for VSI sysctls */
	struct sysctl_oid *debug_sysctls; /* Sysctl node for debug sysctls */

	device_t dev;
	if_ctx_t ctx;
	if_shared_ctx_t sctx;
	if_softc_ctx_t scctx;
	struct ifmedia *media;
	struct ifnet *ifp;

	/* device statistics */
	struct ice_pf_hw_stats stats;
	struct ice_pf_sw_stats soft_stats;

	/* Tx/Rx queue managers */
	struct ice_resmgr tx_qmgr;
	struct ice_resmgr rx_qmgr;

	/* Interrupt allocation manager */
	struct ice_resmgr dev_imgr;
	u16 *pf_imap;
	int lan_vectors;

	/* iflib Tx/Rx queue count sysctl values */
	int ifc_sysctl_ntxqs;
	int ifc_sysctl_nrxqs;

	/* IRQ Vector data */
	struct resource *msix_table;
	int num_irq_vectors;
	struct ice_irq_vector *irqvs;

	/* BAR info */
	struct ice_bar_info bar0;

	/* link status */
	bool link_up;

	/* Ethertype filters enabled */
	bool enable_tx_fc_filter;
	bool enable_tx_lldp_filter;

	/* Other tunable flags */
	bool enable_health_events;

	/* 5-layer scheduler topology enabled */
	bool tx_balance_en;

	/* Allow additional non-standard FEC mode */
	bool allow_no_fec_mod_in_auto;

	int rebuild_ticks;

	/* driver state flags, only access using atomic functions */
	u32 state;

	/* NVM link override settings */
	struct ice_link_default_override_tlv ldo_tlv;

	u32 fw_debug_dump_cluster_mask;

	struct sx *iflib_ctx_lock;

	/* Tri-state feature flags (capable/enabled) */
	ice_declare_bitmap(feat_cap, ICE_FEATURE_COUNT);
	ice_declare_bitmap(feat_en, ICE_FEATURE_COUNT);

	struct ice_resmgr os_imgr;
	/* For mirror interface */
	struct ice_mirr_if *mirr_if;
	int extra_vectors;
	int last_rid;
};

#endif /* _ICE_IFLIB_H_ */
