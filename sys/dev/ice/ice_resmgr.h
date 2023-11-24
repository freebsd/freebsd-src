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
 * @file ice_resmgr.h
 * @brief Resource manager interface
 *
 * Defines an interface for managing PF hardware queues and interrupts for assigning them to
 * hardware VSIs and VFs.
 *
 * For queue management:
 * The total number of available Tx and Rx queues is not equal, so it is
 * expected that each PF will allocate two ice_resmgr structures, one for Tx
 * and one for Rx. These should be allocated in attach() prior to initializing
 * VSIs, and destroyed in detach().
 *
 * For interrupt management:
 * The PF allocates an ice_resmgr structure that does not allow scattered
 * allocations since interrupt allocations must be contiguous.
 */

#ifndef _ICE_RESMGR_H_
#define _ICE_RESMGR_H_
#include <sys/param.h>
#include "ice_osdep.h"

#include <sys/bitstring.h>

/*
 * For managing VSI queue allocations
 */
/* Hardware only supports a limited number of resources in scattered mode */
#define ICE_MAX_SCATTERED_QUEUES	16
/* Use highest value to indicate invalid resource mapping */
#define ICE_INVALID_RES_IDX		0xFFFF

/*
 * Structures
 */

/**
 * @struct ice_resmgr
 * @brief Resource manager
 *
 * Represent resource allocations using a bitstring, where bit zero represents
 * the first resource. If a particular bit is set this indicates that the
 * resource has been allocated and is not free.
 */
struct ice_resmgr {
	bitstr_t	*resources;
	u16		num_res;
	bool		contig_only;
};

/**
 * @enum ice_resmgr_alloc_type
 * @brief resource manager allocation types
 *
 * Enumeration of possible allocation types that can be used when
 * assigning resources. For now, SCATTERED is only used with
 * managing queue allocations.
 */
enum ice_resmgr_alloc_type {
	ICE_RESMGR_ALLOC_INVALID = 0,
	ICE_RESMGR_ALLOC_CONTIGUOUS,
	ICE_RESMGR_ALLOC_SCATTERED
};

/* Public resource manager allocation functions */
int	ice_resmgr_init(struct ice_resmgr *resmgr, u16 num_res);
int	ice_resmgr_init_contig_only(struct ice_resmgr *resmgr, u16 num_res);
void	ice_resmgr_destroy(struct ice_resmgr *resmgr);

/* Public resource assignment functions */
int	ice_resmgr_assign_contiguous(struct ice_resmgr *resmgr, u16 *idx, u16 num_res);
int	ice_resmgr_assign_scattered(struct ice_resmgr *resmgr, u16 *idx, u16 num_res);

/* Release resources */
void	ice_resmgr_release_map(struct ice_resmgr *resmgr, u16 *idx, u16 num_res);

#endif /* _ICE_RESMGR_H_ */

