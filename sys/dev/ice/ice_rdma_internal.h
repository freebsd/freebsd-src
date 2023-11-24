/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
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
 * @file ice_rdma_internal.h
 * @brief internal header for the RMDA driver interface setup
 *
 * Contains the definitions and functions used by the ice driver to setup the
 * RDMA driver interface. Functions and definitions in this file are not
 * shared with the RDMA client driver.
 */
#ifndef _ICE_RDMA_INTERNAL_H_
#define _ICE_RDMA_INTERNAL_H_

#include "ice_rdma.h"

/* Forward declare the softc structure */
struct ice_softc;

/* Global sysctl variable indicating if the RDMA client interface is enabled */
extern bool ice_enable_irdma;

/**
 * @struct ice_rdma_entry
 * @brief RDMA peer list node
 *
 * Structure used to store peer entries for each PF in a linked list.
 * @var ice_rdma_entry::attached
 * 	check for irdma driver attached
 * @var ice_rdma_entry::initiated
 * 	check for irdma driver ready to use
 * @var ice_rdma_entry::node
 * 	list node of the RDMA entry
 * @var ice_rdma_entry::peer
 * 	pointer to peer
 */
struct ice_rdma_entry {
	LIST_ENTRY(ice_rdma_entry) node;
	struct ice_rdma_peer peer;
	bool attached;
	bool initiated;
};

#define ice_rdma_peer_to_entry(p) __containerof(p, struct ice_rdma_entry, peer)
#define ice_rdma_entry_to_sc(e) __containerof(e, struct ice_softc, rdma_entry)
#define ice_rdma_peer_to_sc(p) ice_rdma_entry_to_sc(ice_rdma_peer_to_entry(p))

/**
 * @struct ice_rdma_peers
 * @brief Head list structure for the RDMA entry list
 *
 * Type defining the head of the linked list of RDMA entries.
 */
LIST_HEAD(ice_rdma_peers, ice_rdma_entry);

/**
 * @struct ice_rdma_state
 * @brief global driver state for RDMA
 *
 * Contains global state shared across all PFs by the device driver, such as
 * the kobject class of the currently connected peer driver, and the linked
 * list of peer entries for each PF.
 *
 * @var ice_rdma_state::registered
 * 	check forr irdma driver registered
 * @var ice_rdma_state::peer_class
 * 	kobject class for irdma driver
 * @var ice_rdma_state::mtx
 * 	mutex for protecting irdma operations
 * @var ice_rdma_state::peers
 * 	list of RDMA entries
 */
struct ice_rdma_state {
	bool registered;
	kobj_class_t peer_class;
	struct sx mtx;
	struct ice_rdma_peers peers;
};

void ice_rdma_init(void);
void ice_rdma_exit(void);

int  ice_rdma_pf_attach(struct ice_softc *sc);
void ice_rdma_pf_detach(struct ice_softc *sc);
int  ice_rdma_pf_init(struct ice_softc *sc);
int  ice_rdma_pf_stop(struct ice_softc *sc);
void ice_rdma_link_change(struct ice_softc *sc, int linkstate, uint64_t baudrate);
void ice_rdma_notify_dcb_qos_change(struct ice_softc *sc);
void ice_rdma_dcb_qos_update(struct ice_softc *sc, struct ice_port_info *pi);
void ice_rdma_notify_pe_intr(struct ice_softc *sc, uint32_t oicr);
void ice_rdma_notify_reset(struct ice_softc *sc);
#endif
