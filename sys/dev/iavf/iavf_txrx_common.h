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
 * @file iavf_txrx_common.h
 * @brief Tx/Rx hotpath functions common to legacy and iflib
 *
 * Contains implementations for functions used in the hotpath for both the
 * legacy and iflib driver implementations.
 */
#ifndef _IAVF_TXRX_COMMON_H_
#define _IAVF_TXRX_COMMON_H_

#include "iavf_iflib.h"

static inline int iavf_ptype_to_hash(u8 ptype);

/**
 * iavf_ptype_to_hash - parse the packet type
 * @ptype: packet type
 *
 * Determine the appropriate hash for a given packet type
 *
 * @returns the M_HASHTYPE_* value for the given packet type.
 */
static inline int
iavf_ptype_to_hash(u8 ptype)
{
        struct iavf_rx_ptype_decoded decoded;

	decoded = decode_rx_desc_ptype(ptype);

	if (!decoded.known)
		return M_HASHTYPE_OPAQUE;

	if (decoded.outer_ip == IAVF_RX_PTYPE_OUTER_L2)
		return M_HASHTYPE_OPAQUE;

	/* Note: anything that gets to this point is IP */
        if (decoded.outer_ip_ver == IAVF_RX_PTYPE_OUTER_IPV6) {
		switch (decoded.inner_prot) {
		case IAVF_RX_PTYPE_INNER_PROT_TCP:
			return M_HASHTYPE_RSS_TCP_IPV6;
		case IAVF_RX_PTYPE_INNER_PROT_UDP:
			return M_HASHTYPE_RSS_UDP_IPV6;
		default:
			return M_HASHTYPE_RSS_IPV6;
		}
	}
        if (decoded.outer_ip_ver == IAVF_RX_PTYPE_OUTER_IPV4) {
		switch (decoded.inner_prot) {
		case IAVF_RX_PTYPE_INNER_PROT_TCP:
			return M_HASHTYPE_RSS_TCP_IPV4;
		case IAVF_RX_PTYPE_INNER_PROT_UDP:
			return M_HASHTYPE_RSS_UDP_IPV4;
		default:
			return M_HASHTYPE_RSS_IPV4;
		}
	}
	/* We should never get here! */
	return M_HASHTYPE_OPAQUE;
}

#endif /* _IAVF_TXRX_COMMON_H_ */
