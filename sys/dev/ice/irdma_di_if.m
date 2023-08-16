# SPDX-License-Identifier: BSD-3-Clause 
#  Copyright (c) 2023, Intel Corporation
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

/**
 * @file irdma_di_if.m
 * @brief RDMA client kobject driver interface
 *
 * KObject methods implemented by the ice driver. These functions are called
 * by the RDMA client driver to connect with the ice driver and request
 * operations or notify the driver of RDMA events.
 */
#include "ice_rdma.h"

INTERFACE irdma_di;

/**
 * reset - Request the ice driver to perform a reset
 * @peer: the RDMA peer structure
 *
 * Called by the RDMA client driver to request a reset of the ice device.
 */
METHOD int reset {
	struct ice_rdma_peer *peer;
};

/**
 * msix_init - Initialize MSI-X resources for the RDMA driver
 * @peer: the RDMA peer structure
 * @msix_info: the requested MSI-X mapping
 *
 * Called by the RDMA client driver to request initialization of the MSI-X
 * resources used for RDMA functionality.
 */
METHOD int msix_init {
	struct ice_rdma_peer *peer;
	struct ice_rdma_msix_mapping *msix_info;
};

/**
 * qset_register_request - RDMA client interface request qset
 *                         registration or deregistration
 * @peer: the RDMA peer client structure
 * @res: resources to be registered or unregistered
 */
METHOD int qset_register_request {
	struct ice_rdma_peer *peer;
	struct ice_rdma_qset_update *res;
};

/**
 * vsi_filter_update - configure vsi information
 *                     when opening or closing rdma driver
 * @peer: the RDMA peer client structure
 * @enable: enable or disable the rdma filter
 */
METHOD int vsi_filter_update {
	struct ice_rdma_peer *peer;
	bool enable;
};

/**
 * req_handler - handle requests incoming from RDMA driver
 * @peer: the RDMA peer client structure
 * @req: structure containing request
 */
METHOD void req_handler {
	struct ice_rdma_peer *peer;
	struct ice_rdma_request *req;
};
