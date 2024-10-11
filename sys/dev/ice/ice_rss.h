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
 * @file ice_rss.h
 * @brief default RSS values if kernel RSS is not enabled
 *
 * This header includes default definitions for RSS functionality if the
 * kernel RSS interface is not enabled. This allows main driver code to avoid
 * having to check the RSS ifdef throughout, but instead just use the RSS
 * definitions, as they will fall back to these defaults when the kernel
 * interface is disabled.
 */
#ifndef _ICE_RSS_H_
#define _ICE_RSS_H_

#ifdef RSS
// We have the kernel RSS interface available
#include <net/rss_config.h>

/* Make sure our key size buffer has enough space to store the kernel RSS key */
CTASSERT(ICE_AQC_GET_SET_RSS_KEY_DATA_RSS_KEY_SIZE >= RSS_KEYSIZE);
#else
/* The kernel RSS interface is not enabled. Use suitable defaults for the RSS
 * configuration functions.
 *
 * The RSS hash key will be a pre-generated random key.
 * The number of buckets will just match the number of CPUs.
 * The lookup table will be assigned using round-robin with no indirection.
 * The RSS hash configuration will be set to suitable defaults.
 */

#define	RSS_HASHTYPE_RSS_IPV4		(1 << 1)	/* IPv4 2-tuple */
#define	RSS_HASHTYPE_RSS_TCP_IPV4	(1 << 2)	/* TCPv4 4-tuple */
#define	RSS_HASHTYPE_RSS_IPV6		(1 << 3)	/* IPv6 2-tuple */
#define	RSS_HASHTYPE_RSS_TCP_IPV6	(1 << 4)	/* TCPv6 4-tuple */
#define	RSS_HASHTYPE_RSS_IPV6_EX	(1 << 5)	/* IPv6 2-tuple + ext hdrs */
#define	RSS_HASHTYPE_RSS_TCP_IPV6_EX	(1 << 6)	/* TCPv6 4-tiple + ext hdrs */
#define	RSS_HASHTYPE_RSS_UDP_IPV4	(1 << 7)	/* IPv4 UDP 4-tuple */
#define	RSS_HASHTYPE_RSS_UDP_IPV6	(1 << 9)	/* IPv6 UDP 4-tuple */
#define	RSS_HASHTYPE_RSS_UDP_IPV6_EX	(1 << 10)	/* IPv6 UDP 4-tuple + ext hdrs */

#define rss_getkey(key) ice_get_default_rss_key(key)
#define rss_getnumbuckets() (mp_ncpus)
#define rss_get_indirection_to_bucket(index) (index)
#define rss_gethashconfig() (ICE_DEFAULT_RSS_HASH_CONFIG)

/**
 * rss_hash2bucket - Determine the bucket for a given hash value
 * @hash_val: the hash value to use
 * @hash_type: the type of the hash
 * @bucket_id: on success, updated with the bucket
 *
 * This function simply verifies that the hash type is known. If it is, then
 * we forward the hash value directly as the bucket id. If the hash type is
 * unknown, we return -1.
 *
 * This is the simplest mechanism for converting a hash value into a bucket,
 * and does not support any form of indirection table.
 */
static inline int
rss_hash2bucket(uint32_t hash_val, uint32_t hash_type, uint32_t *bucket_id)
{
	switch (hash_type) {
	case M_HASHTYPE_RSS_IPV4:
	case M_HASHTYPE_RSS_TCP_IPV4:
	case M_HASHTYPE_RSS_UDP_IPV4:
	case M_HASHTYPE_RSS_IPV6:
	case M_HASHTYPE_RSS_TCP_IPV6:
	case M_HASHTYPE_RSS_UDP_IPV6:
		*bucket_id = hash_val;
		return (0);
	default:
		return (-1);
	}
}

#endif /* !RSS */

#define ICE_DEFAULT_RSS_HASH_CONFIG \
	((u_int)(RSS_HASHTYPE_RSS_IPV4 | \
		 RSS_HASHTYPE_RSS_TCP_IPV4 | \
		 RSS_HASHTYPE_RSS_UDP_IPV4 | \
		 RSS_HASHTYPE_RSS_IPV6 | \
		 RSS_HASHTYPE_RSS_TCP_IPV6 | \
		 RSS_HASHTYPE_RSS_UDP_IPV6))

#endif /* _ICE_COMMON_COMPAT_H_ */
