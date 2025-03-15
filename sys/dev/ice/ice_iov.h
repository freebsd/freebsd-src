/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2025, Intel Corporation
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
 * @file ice_iov.h
 * @brief header for IOV functionality
 *
 * This header includes definitions used to implement device Virtual Functions
 * for the ice driver.
 */

#ifndef _ICE_IOV_H_
#define _ICE_IOV_H_

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/nv.h>
#include <sys/iov_schema.h>
#include <sys/param.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/pci/pci_iov.h>

#include "ice_iflib.h"
#include "ice_vf_mbx.h"

/**
 * @enum ice_vf_flags
 * @brief VF state flags
 *
 * Used to indicate the status of a PF's VF, as well as indicating what each VF
 * is capabile of. Intended to be modified only using atomic operations, so
 * they can be read and modified in places that aren't locked.
 *
 * Used in struct ice_vf's vf_flags field.
 */
enum ice_vf_flags {
	VF_FLAG_ENABLED			= BIT(0),
	VF_FLAG_SET_MAC_CAP		= BIT(1),
	VF_FLAG_VLAN_CAP		= BIT(2),
	VF_FLAG_PROMISC_CAP		= BIT(3),
	VF_FLAG_MAC_ANTI_SPOOF		= BIT(4),
};

/**
 * @struct ice_vf
 * @brief PF's VF software context
 *
 * Represents the state and options for a VF spawned from a PF.
 */
struct ice_vf {
	struct ice_vsi *vsi;
	u32 vf_flags;

	u8 mac[ETHER_ADDR_LEN];
	u16 vf_num;
	struct virtchnl_version_info version;

	u16 mac_filter_limit;
	u16 mac_filter_cnt;
	u16 vlan_limit;
	u16 vlan_cnt;

	u16 num_irq_vectors;
	u16 *vf_imap;
	struct ice_irq_vector *tx_irqvs;
	struct ice_irq_vector *rx_irqvs;
};

#define ICE_PCIE_DEV_STATUS			0xAA

#define ICE_PCI_CIAD_WAIT_COUNT			100
#define ICE_PCI_CIAD_WAIT_DELAY_US		1
#define ICE_VPGEN_VFRSTAT_WAIT_COUNT		100
#define ICE_VPGEN_VFRSTAT_WAIT_DELAY_US		20

#define ICE_VIRTCHNL_VALID_PROMISC_FLAGS	(FLAG_VF_UNICAST_PROMISC | \
						 FLAG_VF_MULTICAST_PROMISC)

#define ICE_DEFAULT_VF_VLAN_LIMIT			64
#define ICE_DEFAULT_VF_FILTER_LIMIT			16

int ice_iov_attach(struct ice_softc *sc);
int ice_iov_detach(struct ice_softc *sc);

int ice_iov_init(struct ice_softc *sc, uint16_t num_vfs, const nvlist_t *params);
int ice_iov_add_vf(struct ice_softc *sc, uint16_t vfnum, const nvlist_t *params);
void ice_iov_uninit(struct ice_softc *sc);

void ice_iov_handle_vflr(struct ice_softc *sc);

void ice_vc_handle_vf_msg(struct ice_softc *sc, struct ice_rq_event_info *event);
void ice_vc_notify_all_vfs_link_state(struct ice_softc *sc);

#endif /* _ICE_IOV_H_ */

