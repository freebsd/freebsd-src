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
 * @file ice_resmgr.c
 * @brief Resource allocation manager
 *
 * Manage device resource allocations for a PF, including assigning queues to
 * VSIs, or managing interrupt allocations across the PF.
 *
 * It can handle contiguous and scattered resource allocations, and upon
 * assigning them, will fill in the mapping array with a map of
 * resource IDs to PF-space resource indices.
 */

#include "ice_resmgr.h"

/**
 * @var M_ICE_RESMGR
 * @brief PF resource manager allocation type
 *
 * malloc(9) allocation type used by the resource manager code.
 */
MALLOC_DEFINE(M_ICE_RESMGR, "ice-resmgr", "Intel(R) 100Gb Network Driver resmgr allocations");

/*
 * Public resource manager allocation functions
 */

/**
 * ice_resmgr_init - Initialize a resource manager structure
 * @resmgr: structure to track the resource manager state
 * @num_res: the maximum number of resources it can assign
 *
 * Initialize the state of a resource manager structure, allocating space to
 * assign up to the requested number of resources. Uses bit strings to track
 * which resources have been assigned. This type of resmgr is intended to be
 * used for tracking LAN queue assignments between VSIs.
 */
int
ice_resmgr_init(struct ice_resmgr *resmgr, u16 num_res)
{
	resmgr->resources = bit_alloc(num_res, M_ICE_RESMGR, M_NOWAIT);
	if (resmgr->resources == NULL)
		return (ENOMEM);

	resmgr->num_res = num_res;
	resmgr->contig_only = false;
	return (0);
}

/**
 * ice_resmgr_init_contig_only - Initialize a resource manager structure
 * @resmgr: structure to track the resource manager state
 * @num_res: the maximum number of resources it can assign
 *
 * Functions similarly to ice_resmgr_init(), but the resulting resmgr structure
 * will only allow contiguous allocations. This type of resmgr is intended to
 * be used with tracking device MSI-X interrupt allocations.
 */
int
ice_resmgr_init_contig_only(struct ice_resmgr *resmgr, u16 num_res)
{
	int error;

	error = ice_resmgr_init(resmgr, num_res);
	if (error)
		return (error);

	resmgr->contig_only = true;
	return (0);
}

/**
 * ice_resmgr_destroy - Deallocate memory associated with a resource manager
 * @resmgr: resource manager structure
 *
 * De-allocates the bit string associated with this resource manager. It is
 * expected that this function will not be called until all of the assigned
 * resources have been released.
 */
void
ice_resmgr_destroy(struct ice_resmgr *resmgr)
{
	if (resmgr->resources != NULL) {
#ifdef INVARIANTS
		int set;

		bit_count(resmgr->resources, 0, resmgr->num_res, &set);
		MPASS(set == 0);
#endif

		free(resmgr->resources, M_ICE_RESMGR);
		resmgr->resources = NULL;
	}
	resmgr->num_res = 0;
}

/*
 * Resource allocation functions
 */

/**
 * ice_resmgr_assign_contiguous - Assign contiguous mapping of resources
 * @resmgr: resource manager structure
 * @idx: memory to store mapping, at least num_res wide
 * @num_res: the number of resources to assign
 *
 * Assign num_res number of contiguous resources into the idx mapping. On
 * success, idx will be updated to map each index to a PF resource.
 *
 * This function guarantees that the resource mapping will be contiguous, and
 * will fail if that is not possible.
 */
int
ice_resmgr_assign_contiguous(struct ice_resmgr *resmgr, u16 *idx, u16 num_res)
{
	int start, i;

	bit_ffc_area(resmgr->resources, resmgr->num_res, num_res, &start);
	if (start < 0)
		return (ENOSPC);

	/* Set each bit and update the index array */
	for (i = 0; i < num_res; i++) {
		bit_set(resmgr->resources, start + i);
		idx[i] = start + i;
	}

	return (0);
}

/**
 * ice_resmgr_assign_scattered - Assign possibly scattered resources
 * @resmgr: the resource manager structure
 * @idx: memory to store associated resource mapping, at least num_res wide
 * @num_res: the number of resources to assign
 *
 * Assign num_res number of resources into the idx_mapping. On success, idx
 * will be updated to map each index to a PF-space resource.
 *
 * Queues may be allocated non-contiguously, and this function requires that
 * num_res be less than the ICE_MAX_SCATTERED_QUEUES due to hardware
 * limitations on scattered queue assignment.
 */
int
ice_resmgr_assign_scattered(struct ice_resmgr *resmgr, u16 *idx, u16 num_res)
{
	int index = 0, i;

	/* Scattered allocations won't work if they weren't allowed at resmgr
	 * creation time.
	 */
	if (resmgr->contig_only)
		return (EPERM);

	/* Hardware can only support a limited total of scattered queues for
	 * a single VSI
	 */
	if (num_res > ICE_MAX_SCATTERED_QUEUES)
		return (EOPNOTSUPP);

	for (i = 0; i < num_res; i++) {
		bit_ffc_at(resmgr->resources, index, resmgr->num_res, &index);
		if (index < 0)
			goto err_no_space;

		bit_set(resmgr->resources, index);
		idx[i] = index;
	}
	return (0);

err_no_space:
	/* Release any resources we did assign up to this point. */
	ice_resmgr_release_map(resmgr, idx, i);
	return (ENOSPC);
}

/**
 * ice_resmgr_release_map - Release previously assigned resource mapping
 * @resmgr: the resource manager structure
 * @idx: previously assigned resource mapping
 * @num_res: number of resources in the mapping
 *
 * Clears the assignment of each resource in the provided resource index. Updates
 * the idx to indicate that each of the virtual indexes have invalid resource
 * mappings by assigning them to ICE_INVALID_RES_IDX.
 */
void
ice_resmgr_release_map(struct ice_resmgr *resmgr, u16 *idx, u16 num_res)
{
	int i;

	for (i = 0; i < num_res; i++) {
		if (idx[i] < resmgr->num_res)
			bit_clear(resmgr->resources, idx[i]);
		idx[i] = ICE_INVALID_RES_IDX;
	}
}
