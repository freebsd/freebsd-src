# SPDX-License-Identifier: BSD-3-Clause 
#  Copyright (c) 2022, Intel Corporation
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#   1. Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#
#   3. Neither the name of the Intel Corporation nor the names of its
#      contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
# $FreeBSD$

/**
 * @file irdma_if.m
 * @brief RDMA client kobject interface
 *
 * KOBject methods implemented by the RDMA client driver. These functions will
 * be called from the ice driver to notify the RDMA client driver of device
 * driver events.
 */
#include "ice_rdma.h"

INTERFACE irdma;

/**
 * probe - Notify the RDMA client driver that a peer device has been created
 * @peer: the RDMA peer structure
 *
 * Called by the ice driver during attach to notify the RDMA client driver
 * that a new PF has been initialized.
 */
METHOD int probe {
	struct ice_rdma_peer *peer;
};

/**
 * open - Notify the RDMA client driver that a peer device has been opened
 * @peer: the RDMA peer structure
 *
 * Called by the ice driver during the if_init routine to notify the RDMA
 * client driver that a PF has been activated.
 */
METHOD int open {
	struct ice_rdma_peer *peer;
};

/**
 * close - Notify the RDMA client driver that a peer device has closed
 * @peer: the RDMA peer structure
 *
 * Called by the ice driver during the if_stop routine to notify the RDMA
 * client driver that a PF has been deactivated.
 */
METHOD int close {
	struct ice_rdma_peer *peer;
};

/**
 * remove - Notify the RDMA client driver that a peer device has been removed
 * @peer: the RDMA peer structure
 *
 * Called by the ice driver during detach to notify the RDMA client driver
 * that a PF has been removed.
 */
METHOD int remove {
	struct ice_rdma_peer *peer;
}

/**
 * link_change - Notify the RDMA client driver that link status has changed
 * @peer: the RDMA peer structure
 * @linkstate: link status
 * @baudrate: link rate in bits per second
 *
 * Called by the ice driver when link status changes to notify the RDMA client
 * driver of the new status.
 */
METHOD void link_change {
	struct ice_rdma_peer *peer;
	int linkstate;
	uint64_t baudrate;
}

METHOD void event_handler {
	struct ice_rdma_peer *peer;
	struct ice_rdma_event *event;
}
