/*-
 * Copyright 2007-2009 Solarflare Communications Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"


#if EFSYS_OPT_FILTER

/* "Fudge factors" - difference between programmed value and actual depth.
 * Due to pipelined implementation we need to program H/W with a value that
 * is larger than the hop limit we want.
 */
#define FILTER_CTL_SRCH_FUDGE_WILD 3
#define FILTER_CTL_SRCH_FUDGE_FULL 1

/* Hard maximum hop limit.  Hardware will time-out beyond 200-something.
 * We also need to avoid infinite loops in efx_filter_search() when the
 * table is full.
 */
#define FILTER_CTL_SRCH_MAX 200

/* The filter hash function is LFSR polynomial x^16 + x^3 + 1 of a 32-bit
 * key derived from the n-tuple. */
static			uint16_t
efx_filter_tbl_hash(
	__in 		uint32_t key)
{
	uint16_t tmp;

	/* First 16 rounds */
	tmp = 0x1fff ^ (uint16_t)(key >> 16);
	tmp = tmp ^ tmp >> 3 ^ tmp >> 6;
	tmp = tmp ^ tmp >> 9;

	/* Last 16 rounds */
	tmp = tmp ^ tmp << 13 ^ (uint16_t)(key & 0xffff);
	tmp = tmp ^ tmp >> 3 ^ tmp >> 6;
	tmp = tmp ^ tmp >> 9;

	return (tmp);
}


/* To allow for hash collisions, filter search continues at these
 * increments from the first possible entry selected by the hash. */
static			uint16_t
efx_filter_tbl_increment(
	__in		uint32_t key)
{
	return ((uint16_t)(key * 2 - 1));
}

static	__checkReturn	boolean_t
efx_filter_test_used(
	__in		efx_filter_tbl_t *eftp,
	__in		unsigned int index)
{
	EFSYS_ASSERT3P(eftp->eft_bitmap, !=, NULL);
	return ((eftp->eft_bitmap[index / 32] & (1 << (index % 32))) != 0);
}

static			void
efx_filter_set_used(
	__in		efx_filter_tbl_t *eftp,
	__in		unsigned int index)
{
	EFSYS_ASSERT3P(eftp->eft_bitmap, !=, NULL);
	eftp->eft_bitmap[index / 32] |= (1 << (index % 32));
	++eftp->eft_used;
}

static			void
efx_filter_clear_used(
	__in		efx_filter_tbl_t *eftp,
	__in		unsigned int index)
{
	EFSYS_ASSERT3P(eftp->eft_bitmap, !=, NULL);
	eftp->eft_bitmap[index / 32] &= ~(1 << (index % 32));

	--eftp->eft_used;
	EFSYS_ASSERT3U(eftp->eft_used, >=, 0);
}


static 			efx_filter_tbl_id_t
efx_filter_tbl_id(
	__in		efx_filter_type_t type)
{
	efx_filter_tbl_id_t tbl_id;

	switch (type)
	{
	case EFX_FILTER_RX_TCP_FULL:
	case EFX_FILTER_RX_TCP_WILD:
	case EFX_FILTER_RX_UDP_FULL:
	case EFX_FILTER_RX_UDP_WILD:
		tbl_id = EFX_FILTER_TBL_RX_IP;
		break;

#if EFSYS_OPT_SIENA
	case EFX_FILTER_RX_MAC_FULL:
	case EFX_FILTER_RX_MAC_WILD:
		tbl_id = EFX_FILTER_TBL_RX_MAC;
		break;

	case EFX_FILTER_TX_TCP_FULL:
	case EFX_FILTER_TX_TCP_WILD:
	case EFX_FILTER_TX_UDP_FULL:
	case EFX_FILTER_TX_UDP_WILD:
		tbl_id = EFX_FILTER_TBL_TX_IP;
		break;

	case EFX_FILTER_TX_MAC_FULL:
	case EFX_FILTER_TX_MAC_WILD:
		tbl_id = EFX_FILTER_TBL_RX_MAC;
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}
	return (tbl_id);
}

static			void
efx_filter_reset_search_depth(
	__inout		efx_filter_t *efp,
	__in		efx_filter_tbl_id_t tbl_id)
{
	switch (tbl_id)
	{
	case EFX_FILTER_TBL_RX_IP:
		efp->ef_depth[EFX_FILTER_RX_TCP_FULL] = 0;
		efp->ef_depth[EFX_FILTER_RX_TCP_WILD] = 0;
		efp->ef_depth[EFX_FILTER_RX_UDP_FULL] = 0;
		efp->ef_depth[EFX_FILTER_RX_UDP_WILD] = 0;
		break;

#if EFSYS_OPT_SIENA
	case EFX_FILTER_TBL_RX_MAC:
		efp->ef_depth[EFX_FILTER_RX_MAC_FULL] = 0;
		efp->ef_depth[EFX_FILTER_RX_MAC_WILD] = 0;
		break;

	case EFX_FILTER_TBL_TX_IP:
		efp->ef_depth[EFX_FILTER_TX_TCP_FULL] = 0;
		efp->ef_depth[EFX_FILTER_TX_TCP_WILD] = 0;
		efp->ef_depth[EFX_FILTER_TX_UDP_FULL] = 0;
		efp->ef_depth[EFX_FILTER_TX_UDP_WILD] = 0;
		break;

	case EFX_FILTER_TBL_TX_MAC:
		efp->ef_depth[EFX_FILTER_TX_MAC_FULL] = 0;
		efp->ef_depth[EFX_FILTER_TX_MAC_WILD] = 0;
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}
}

static			void
efx_filter_push_rx_limits(
	__in		efx_nic_t *enp)
{
	efx_filter_t *efp = &enp->en_filter;
	efx_oword_t oword;

	EFX_BAR_READO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);

	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TCP_FULL_SRCH_LIMIT,
	    efp->ef_depth[EFX_FILTER_RX_TCP_FULL] +
	    FILTER_CTL_SRCH_FUDGE_FULL);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TCP_WILD_SRCH_LIMIT,
	    efp->ef_depth[EFX_FILTER_RX_TCP_WILD] +
	    FILTER_CTL_SRCH_FUDGE_WILD);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_UDP_FULL_SRCH_LIMIT,
	    efp->ef_depth[EFX_FILTER_RX_UDP_FULL] +
	    FILTER_CTL_SRCH_FUDGE_FULL);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_UDP_WILD_SRCH_LIMIT,
	    efp->ef_depth[EFX_FILTER_RX_UDP_WILD] +
	    FILTER_CTL_SRCH_FUDGE_WILD);

#if EFSYS_OPT_SIENA
	if (efp->ef_tbl[EFX_FILTER_TBL_RX_MAC].eft_size) {
		EFX_SET_OWORD_FIELD(oword,
		    FRF_CZ_ETHERNET_FULL_SEARCH_LIMIT,
		    efp->ef_depth[EFX_FILTER_RX_MAC_FULL] +
		    FILTER_CTL_SRCH_FUDGE_FULL);
		EFX_SET_OWORD_FIELD(oword,
		    FRF_CZ_ETHERNET_WILDCARD_SEARCH_LIMIT,
		    efp->ef_depth[EFX_FILTER_RX_MAC_WILD] +
		    FILTER_CTL_SRCH_FUDGE_WILD);
	}
#endif /* EFSYS_OPT_SIENA */

	EFX_BAR_WRITEO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
}

static			void
efx_filter_push_tx_limits(
	__in		efx_nic_t *enp)
{
	efx_filter_t *efp = &enp->en_filter;
	efx_oword_t oword;

	if (efp->ef_tbl[EFX_FILTER_TBL_TX_IP].eft_size == 0)
		return;

	EFX_BAR_READO(enp, FR_AZ_TX_CFG_REG, &oword);

	EFX_SET_OWORD_FIELD(oword, FRF_CZ_TX_TCPIP_FILTER_FULL_SEARCH_RANGE,
	    efp->ef_depth[EFX_FILTER_TX_TCP_FULL] +
	    FILTER_CTL_SRCH_FUDGE_FULL);
	EFX_SET_OWORD_FIELD(oword, FRF_CZ_TX_TCPIP_FILTER_WILD_SEARCH_RANGE,
	    efp->ef_depth[EFX_FILTER_TX_TCP_WILD] +
	    FILTER_CTL_SRCH_FUDGE_WILD);
	EFX_SET_OWORD_FIELD(oword, FRF_CZ_TX_UDPIP_FILTER_FULL_SEARCH_RANGE,
	    efp->ef_depth[EFX_FILTER_TX_UDP_FULL] +
	    FILTER_CTL_SRCH_FUDGE_FULL);
	EFX_SET_OWORD_FIELD(oword, FRF_CZ_TX_UDPIP_FILTER_WILD_SEARCH_RANGE,
	    efp->ef_depth[EFX_FILTER_TX_UDP_WILD] +
	    FILTER_CTL_SRCH_FUDGE_WILD);

	EFX_BAR_WRITEO(enp, FR_AZ_TX_CFG_REG, &oword);
}

/* Build a filter entry and return its n-tuple key. */
static	__checkReturn	uint32_t
efx_filter_build(
	__out		efx_oword_t *filter,
	__in		efx_filter_spec_t *spec)
{
	uint32_t dword3;
	uint32_t key;
	uint8_t  type  = spec->efs_type;
	uint8_t  flags = spec->efs_flags;

	switch (efx_filter_tbl_id(type)) {
	case EFX_FILTER_TBL_RX_IP: {
		boolean_t is_udp = (type == EFX_FILTER_RX_UDP_FULL ||
		    type == EFX_FILTER_RX_UDP_WILD);
		EFX_POPULATE_OWORD_7(*filter,
		    FRF_BZ_RSS_EN,     (flags & EFX_FILTER_FLAG_RX_RSS) ? 1 : 0,
		    FRF_BZ_SCATTER_EN, (flags & EFX_FILTER_FLAG_RX_SCATTER) ? 1 : 0,
		    FRF_AZ_TCP_UDP, is_udp,
		    FRF_AZ_RXQ_ID, spec->efs_dmaq_id,
		    EFX_DWORD_2, spec->efs_dword[2],
		    EFX_DWORD_1, spec->efs_dword[1],
		    EFX_DWORD_0, spec->efs_dword[0]);
		dword3 = is_udp;
		break;
	}

#if EFSYS_OPT_SIENA
	case EFX_FILTER_TBL_RX_MAC: {
		boolean_t is_wild = (type == EFX_FILTER_RX_MAC_WILD);
		EFX_POPULATE_OWORD_8(*filter,
		    FRF_CZ_RMFT_RSS_EN, (flags & EFX_FILTER_FLAG_RX_RSS) ? 1 : 0,
		    FRF_CZ_RMFT_SCATTER_EN, (flags & EFX_FILTER_FLAG_RX_SCATTER) ? 1 : 0,
		    FRF_CZ_RMFT_IP_OVERRIDE, (flags & EFX_FILTER_FLAG_RX_OVERRIDE_IP) ? 1 : 0,
		    FRF_CZ_RMFT_RXQ_ID, spec->efs_dmaq_id,
		    FRF_CZ_RMFT_WILDCARD_MATCH, is_wild,
		    FRF_CZ_RMFT_DEST_MAC_DW1, spec->efs_dword[2],
		    FRF_CZ_RMFT_DEST_MAC_DW0, spec->efs_dword[1],
		    FRF_CZ_RMFT_VLAN_ID, spec->efs_dword[0]);
		dword3 = is_wild;
		break;
	}
#endif /* EFSYS_OPT_SIENA */

	case EFX_FILTER_TBL_TX_IP: {
		boolean_t is_udp = (type == EFX_FILTER_TX_UDP_FULL ||
		    type == EFX_FILTER_TX_UDP_WILD);
		EFX_POPULATE_OWORD_5(*filter,
		    FRF_CZ_TIFT_TCP_UDP, is_udp,
		    FRF_CZ_TIFT_TXQ_ID, spec->efs_dmaq_id,
		    EFX_DWORD_2, spec->efs_dword[2],
		    EFX_DWORD_1, spec->efs_dword[1],
		    EFX_DWORD_0, spec->efs_dword[0]);
		dword3 = is_udp | spec->efs_dmaq_id << 1;
		break;
	}

#if EFSYS_OPT_SIENA
	case EFX_FILTER_TBL_TX_MAC: {
		boolean_t is_wild = (type == EFX_FILTER_TX_MAC_WILD);
		EFX_POPULATE_OWORD_5(*filter,
		    FRF_CZ_TMFT_TXQ_ID, spec->efs_dmaq_id,
		    FRF_CZ_TMFT_WILDCARD_MATCH, is_wild,
		    FRF_CZ_TMFT_SRC_MAC_DW1, spec->efs_dword[2],
		    FRF_CZ_TMFT_SRC_MAC_DW0, spec->efs_dword[1],
		    FRF_CZ_TMFT_VLAN_ID, spec->efs_dword[0]);
		dword3 = is_wild | spec->efs_dmaq_id << 1;
		break;
	}
#endif /* EFSYS_OPT_SIENA */

	default:
		EFSYS_ASSERT(B_FALSE);
	}

	key = spec->efs_dword[0] ^ spec->efs_dword[1] ^ spec->efs_dword[2] ^ dword3;
	return (key);
}

static	__checkReturn		int
efx_filter_push_entry(
	__inout			efx_nic_t *enp,
	__in			efx_filter_type_t type,
	__in			int index,
	__in			efx_oword_t *eop)
{
	int rc;

	switch (type)
	{
	case EFX_FILTER_RX_TCP_FULL:
	case EFX_FILTER_RX_TCP_WILD:
	case EFX_FILTER_RX_UDP_FULL:
	case EFX_FILTER_RX_UDP_WILD:
		EFX_BAR_TBL_WRITEO(enp, FR_AZ_RX_FILTER_TBL0, index, eop);
		break;

#if EFSYS_OPT_SIENA
	case EFX_FILTER_RX_MAC_FULL:
	case EFX_FILTER_RX_MAC_WILD:
		EFX_BAR_TBL_WRITEO(enp, FR_CZ_RX_MAC_FILTER_TBL0, index, eop);
		break;

	case EFX_FILTER_TX_TCP_FULL:
	case EFX_FILTER_TX_TCP_WILD:
	case EFX_FILTER_TX_UDP_FULL:
	case EFX_FILTER_TX_UDP_WILD:
		EFX_BAR_TBL_WRITEO(enp, FR_CZ_TX_FILTER_TBL0, index, eop);
		break;

	case EFX_FILTER_TX_MAC_FULL:
	case EFX_FILTER_TX_MAC_WILD:
		EFX_BAR_TBL_WRITEO(enp, FR_CZ_TX_MAC_FILTER_TBL0, index, eop);
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		rc = ENOTSUP;
		goto fail1;
	}
	return (0);

fail1:
	return (rc);
}


static	__checkReturn	boolean_t
efx_filter_equal(
	__in		const efx_filter_spec_t *left,
	__in		const efx_filter_spec_t *right)
{
	efx_filter_tbl_id_t tbl_id = efx_filter_tbl_id(left->efs_type);

	if (left->efs_type != right->efs_type)
		return (B_FALSE);

	if (memcmp(left->efs_dword, right->efs_dword, sizeof(left->efs_dword)))
		return (B_FALSE);

	if ((tbl_id == EFX_FILTER_TBL_TX_IP ||
	     tbl_id == EFX_FILTER_TBL_TX_MAC) &&
	    left->efs_dmaq_id != right->efs_dmaq_id)
		return (B_FALSE);

	return (B_TRUE);
}

static	__checkReturn	int
efx_filter_search(
	__in		efx_filter_tbl_t *eftp,
	__in		efx_filter_spec_t *spec,
	__in		uint32_t key,
	__in		boolean_t for_insert,
	__out		int *filter_index,
	__out		int *depth_required)
{
	unsigned hash, incr, filter_idx, depth;

	hash = efx_filter_tbl_hash(key);
	incr = efx_filter_tbl_increment(key);

	filter_idx = hash & (eftp->eft_size - 1);
	depth = 1;

	for (;;) {
		/* Return success if entry is used and matches this spec
		 * or entry is unused and we are trying to insert.
		 */
		if (efx_filter_test_used(eftp, filter_idx) ?
		    efx_filter_equal(spec, &eftp->eft_spec[filter_idx]) :
		    for_insert) {
			*filter_index = filter_idx;
			*depth_required = depth;
			return (0);
		}

		/* Return failure if we reached the maximum search depth */
		if (depth == FILTER_CTL_SRCH_MAX)
			return for_insert ? EBUSY : ENOENT;

		filter_idx = (filter_idx + incr) & (eftp->eft_size - 1);
		++depth;
	}
}

	__checkReturn	int
efx_filter_insert_filter(
	__in		efx_nic_t *enp,
	__in		efx_filter_spec_t *spec,
	__in		boolean_t replace)
{
	efx_filter_t *efp = &enp->en_filter;
	efx_filter_tbl_id_t tbl_id = efx_filter_tbl_id(spec->efs_type);
	efx_filter_tbl_t *eftp = &efp->ef_tbl[tbl_id];
	efx_filter_spec_t *saved_spec;
	efx_oword_t filter;
	int filter_idx;
	unsigned int depth;
	int state;
	uint32_t key;
	int rc;

	if (eftp->eft_size == 0)
		return (EINVAL);

	key = efx_filter_build(&filter, spec);

	EFSYS_LOCK(enp->en_eslp, state);

	rc = efx_filter_search(eftp, spec, key, B_TRUE, &filter_idx, &depth);
	if (rc != 0)
		goto done;

	EFSYS_ASSERT3U(filter_idx, <, eftp->eft_size);
	saved_spec = &eftp->eft_spec[filter_idx];

	if (efx_filter_test_used(eftp, filter_idx)) {
		if (replace == B_FALSE) {
			rc = EEXIST;
			goto done;
		}
	}
	efx_filter_set_used(eftp, filter_idx);
	*saved_spec = *spec;

	if (efp->ef_depth[spec->efs_type] < depth) {
		efp->ef_depth[spec->efs_type] = depth;
		if (tbl_id == EFX_FILTER_TBL_TX_IP ||
		    tbl_id == EFX_FILTER_TBL_TX_MAC)
			efx_filter_push_tx_limits(enp);
		else
			efx_filter_push_rx_limits(enp);
	}

	efx_filter_push_entry(enp, spec->efs_type, filter_idx, &filter);

done:
	EFSYS_UNLOCK(enp->en_eslp, state);
	return (rc);
}

static			void
efx_filter_clear_entry(
	__in		efx_nic_t *enp,
	__in		efx_filter_tbl_t *eftp,
	__in		int index)
{
	efx_oword_t filter;

	if (efx_filter_test_used(eftp, index)) {
		efx_filter_clear_used(eftp, index);

		EFX_ZERO_OWORD(filter);
		efx_filter_push_entry(enp, eftp->eft_spec[index].efs_type,
		    index, &filter);

		memset(&eftp->eft_spec[index], 0, sizeof(eftp->eft_spec[0]));
	}
}

	__checkReturn	int
efx_filter_remove_filter(
	__in		efx_nic_t *enp,
	__in		efx_filter_spec_t *spec)
{
	efx_filter_t *efp = &enp->en_filter;
	efx_filter_tbl_id_t tbl_id = efx_filter_tbl_id(spec->efs_type);
	efx_filter_tbl_t *eftp = &efp->ef_tbl[tbl_id];
	efx_filter_spec_t *saved_spec;
	efx_oword_t filter;
	int filter_idx, depth;
	int state;
	uint32_t key;
	int rc;

	key = efx_filter_build(&filter, spec);

	EFSYS_LOCK(enp->en_eslp, state);

	rc = efx_filter_search(eftp, spec, key, B_FALSE, &filter_idx, &depth);
	if (rc != 0)
		goto out;

	saved_spec = &eftp->eft_spec[filter_idx];

	efx_filter_clear_entry(enp, eftp, filter_idx);
	if (eftp->eft_used == 0)
		efx_filter_reset_search_depth(efp, tbl_id);

	rc = 0;

out:
	EFSYS_UNLOCK(enp->en_eslp, state);
	return (rc);
}

			void
efx_filter_remove_index(
	__inout		efx_nic_t *enp,
	__in		efx_filter_type_t type,
	__in		int index)
{
	efx_filter_t *efp = &enp->en_filter;
	enum efx_filter_tbl_id tbl_id = efx_filter_tbl_id(type);
	efx_filter_tbl_t *eftp = &efp->ef_tbl[tbl_id];
	int state;

	if (index < 0)
		return;

	EFSYS_LOCK(enp->en_eslp, state);

	efx_filter_clear_entry(enp, eftp, index);
	if (eftp->eft_used == 0)
		efx_filter_reset_search_depth(efp, tbl_id);

	EFSYS_UNLOCK(enp->en_eslp, state);
}

			void
efx_filter_tbl_clear(
	__inout		efx_nic_t *enp,
	__in		efx_filter_tbl_id_t tbl_id)
{
	efx_filter_t *efp = &enp->en_filter;
	efx_filter_tbl_t *eftp = &efp->ef_tbl[tbl_id];
	int index;
	int state;

	EFSYS_LOCK(enp->en_eslp, state);

	for (index = 0; index < eftp->eft_size; ++index) {
		efx_filter_clear_entry(enp, eftp, index);
	}

	if (eftp->eft_used == 0)
		efx_filter_reset_search_depth(efp, tbl_id);

	EFSYS_UNLOCK(enp->en_eslp, state);
}

/* Restore filter state after a reset */
			void
efx_filter_restore(
	__in		efx_nic_t *enp)
{
	efx_filter_t *efp = &enp->en_filter;
	efx_filter_tbl_id_t tbl_id;
	efx_filter_tbl_t *eftp;
	efx_filter_spec_t *spec;
	efx_oword_t filter;
	int filter_idx;
	int state;

	EFSYS_LOCK(enp->en_eslp, state);

	for (tbl_id = 0; tbl_id < EFX_FILTER_NTBLS; tbl_id++) {
		eftp = &efp->ef_tbl[tbl_id];
		for (filter_idx = 0; filter_idx < eftp->eft_size; filter_idx++) {
			if (!efx_filter_test_used(eftp, filter_idx))
				continue;

			spec = &eftp->eft_spec[filter_idx];
			efx_filter_build(&filter, spec);
			efx_filter_push_entry(enp, spec->efs_type,
			    filter_idx, &filter);
		}
	}

	efx_filter_push_rx_limits(enp);
	efx_filter_push_tx_limits(enp);

	EFSYS_UNLOCK(enp->en_eslp, state);
}

			void
efx_filter_redirect_index(
	__inout		efx_nic_t *enp,
	__in		efx_filter_type_t type,
	__in		int filter_index,
	__in		int rxq_index)
{
	efx_filter_t *efp = &enp->en_filter;
	efx_filter_tbl_t *eftp =
		&efp->ef_tbl[efx_filter_tbl_id(type)];
	efx_filter_spec_t *spec;
	efx_oword_t filter;
	int state;

	EFSYS_LOCK(enp->en_eslp, state);

	spec = &eftp->eft_spec[filter_index];
	spec->efs_dmaq_id = (uint16_t)rxq_index;

	efx_filter_build(&filter, spec);
	efx_filter_push_entry(enp, spec->efs_type, filter_index, &filter);

	EFSYS_UNLOCK(enp->en_eslp, state);
}

	__checkReturn	int
efx_filter_init(
	__in		efx_nic_t *enp)
{
	efx_filter_t *efp = &enp->en_filter;
	efx_filter_tbl_t *eftp;
	int tbl_id;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_FILTER));

	switch (enp->en_family)
	{
#if EFSYS_OPT_FALCON
	case EFX_FAMILY_FALCON:
		eftp = &efp->ef_tbl[EFX_FILTER_TBL_RX_IP];
		eftp->eft_size = FR_AZ_RX_FILTER_TBL0_ROWS;
		break;
#endif	/* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		eftp = &efp->ef_tbl[EFX_FILTER_TBL_RX_IP];
		eftp->eft_size = FR_AZ_RX_FILTER_TBL0_ROWS;

		eftp = &efp->ef_tbl[EFX_FILTER_TBL_RX_MAC];
		eftp->eft_size = FR_CZ_RX_MAC_FILTER_TBL0_ROWS;

		eftp = &efp->ef_tbl[EFX_FILTER_TBL_TX_IP];
		eftp->eft_size = FR_CZ_TX_FILTER_TBL0_ROWS;

		eftp = &efp->ef_tbl[EFX_FILTER_TBL_TX_MAC];
		eftp->eft_size = FR_CZ_TX_MAC_FILTER_TBL0_ROWS;
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		rc = ENOTSUP;
		goto fail1;
	}

	for (tbl_id = 0; tbl_id < EFX_FILTER_NTBLS; tbl_id++) {
		unsigned int bitmap_size;

		eftp = &efp->ef_tbl[tbl_id];
		if (eftp->eft_size == 0)
			continue;

		EFX_STATIC_ASSERT(sizeof(eftp->eft_bitmap[0]) == sizeof(uint32_t));
		bitmap_size = (eftp->eft_size + (sizeof(uint32_t) * 8) - 1) / 8;

		EFSYS_KMEM_ALLOC(enp->en_esip, bitmap_size, eftp->eft_bitmap);
		if (!eftp->eft_bitmap) {
			rc = ENOMEM;
			goto fail2;
		}

		EFSYS_KMEM_ALLOC(enp->en_esip, eftp->eft_size * sizeof(*eftp->eft_spec),
		    eftp->eft_spec);
		if (!eftp->eft_spec) {
			rc = ENOMEM;
			goto fail2;
		}
		memset(eftp->eft_spec, 0, eftp->eft_size * sizeof(*eftp->eft_spec));
	}
	enp->en_mod_flags |= EFX_MOD_FILTER;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
	efx_filter_fini(enp);

fail1:
	EFSYS_PROBE1(fail1, int, rc);
	return (rc);
}

			void
efx_filter_fini(
	__in		efx_nic_t *enp)
{
	efx_filter_t *efp = &enp->en_filter;
	efx_filter_tbl_id_t tbl_id;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	for (tbl_id = 0; tbl_id < EFX_FILTER_NTBLS; tbl_id++) {
		efx_filter_tbl_t *eftp = &efp->ef_tbl[tbl_id];
		unsigned int bitmap_size;

		EFX_STATIC_ASSERT(sizeof(eftp->eft_bitmap[0]) == sizeof(uint32_t));
		bitmap_size = (eftp->eft_size + (sizeof(uint32_t) * 8) - 1) / 8;

		EFSYS_KMEM_FREE(enp->en_esip, bitmap_size, eftp->eft_bitmap);
		eftp->eft_bitmap = NULL;

		EFSYS_KMEM_FREE(enp->en_esip, eftp->eft_size * sizeof(*eftp->eft_spec),
		    eftp->eft_spec);
		eftp->eft_spec = NULL;
	}

	enp->en_mod_flags &= ~EFX_MOD_FILTER;
}

extern			void
efx_filter_spec_rx_ipv4_tcp_full(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint32_t src_ip,
	__in		uint16_t src_tcp,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_tcp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT((flags & ~(EFX_FILTER_FLAG_RX_RSS |
		    EFX_FILTER_FLAG_RX_SCATTER)) == 0);

	spec->efs_type = EFX_FILTER_RX_TCP_FULL;
	spec->efs_flags = (uint8_t)flags;
	spec->efs_dword[0] = src_tcp | src_ip << 16;
	spec->efs_dword[1] = dest_tcp << 16 | src_ip >> 16;
	spec->efs_dword[2] = dest_ip;
}

extern			void
efx_filter_spec_rx_ipv4_tcp_wild(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_tcp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT((flags & ~(EFX_FILTER_FLAG_RX_RSS |
		    EFX_FILTER_FLAG_RX_SCATTER)) == 0);

	spec->efs_type = EFX_FILTER_RX_TCP_WILD;
	spec->efs_flags = (uint8_t)flags;
	spec->efs_dword[0] = 0;
	spec->efs_dword[1] = dest_tcp << 16;
	spec->efs_dword[2] = dest_ip;
}

extern			void
efx_filter_spec_rx_ipv4_udp_full(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint32_t src_ip,
	__in		uint16_t src_udp,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_udp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT((flags & ~(EFX_FILTER_FLAG_RX_RSS |
		    EFX_FILTER_FLAG_RX_SCATTER)) == 0);

	spec->efs_type = EFX_FILTER_RX_UDP_FULL;
	spec->efs_flags = (uint8_t)flags;
	spec->efs_dword[0] = src_udp | src_ip << 16;
	spec->efs_dword[1] = dest_udp << 16 | src_ip >> 16;
	spec->efs_dword[2] = dest_ip;
}

extern			void
efx_filter_spec_rx_ipv4_udp_wild(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_udp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT((flags & ~(EFX_FILTER_FLAG_RX_RSS |
		    EFX_FILTER_FLAG_RX_SCATTER)) == 0);

	spec->efs_type = EFX_FILTER_RX_UDP_WILD;
	spec->efs_flags = (uint8_t)flags;
	spec->efs_dword[0] = dest_udp;
	spec->efs_dword[1] = 0;
	spec->efs_dword[2] = dest_ip;
}

#if EFSYS_OPT_SIENA
extern			void
efx_filter_spec_rx_mac_full(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint16_t vlan_id,
	__in		uint8_t *dest_mac)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(dest_mac, !=, NULL);
	EFSYS_ASSERT((flags & ~(EFX_FILTER_FLAG_RX_RSS |
		    EFX_FILTER_FLAG_RX_SCATTER |
		    EFX_FILTER_FLAG_RX_OVERRIDE_IP)) == 0);

	spec->efs_type = EFX_FILTER_RX_MAC_FULL;
	spec->efs_flags = (uint8_t)flags;
	spec->efs_dword[0] = vlan_id;
	spec->efs_dword[1] =
	    dest_mac[2] << 24 |
	    dest_mac[3] << 16 |
	    dest_mac[4] <<  8 |
	    dest_mac[5];
 	spec->efs_dword[2] =
	    dest_mac[0] <<  8 |
	    dest_mac[1];
}
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_SIENA
extern			void
efx_filter_spec_rx_mac_wild(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint8_t *dest_mac)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(dest_mac, !=, NULL);
	EFSYS_ASSERT((flags & ~(EFX_FILTER_FLAG_RX_RSS |
		    EFX_FILTER_FLAG_RX_SCATTER |
		    EFX_FILTER_FLAG_RX_OVERRIDE_IP)) == 0);

	spec->efs_type = EFX_FILTER_RX_MAC_WILD;
	spec->efs_flags = (uint8_t)flags;
	spec->efs_dword[0] = 0;
	spec->efs_dword[1] =
	    dest_mac[2] << 24 |
	    dest_mac[3] << 16 |
	    dest_mac[4] <<  8 |
	    dest_mac[5];
 	spec->efs_dword[2] =
	    dest_mac[0] <<  8 |
	    dest_mac[1];
}
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_SIENA
extern			void
efx_filter_spec_tx_ipv4_tcp_full(
	__inout		efx_filter_spec_t *spec,
	__in		uint32_t src_ip,
	__in		uint16_t src_tcp,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_tcp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_type = EFX_FILTER_TX_TCP_FULL;
	spec->efs_flags = 0;
	spec->efs_dword[0] = src_tcp | src_ip << 16;
	spec->efs_dword[1] = dest_tcp << 16 | src_ip >> 16;
	spec->efs_dword[2] = dest_ip;
}
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_SIENA
extern			void
efx_filter_spec_tx_ipv4_tcp_wild(
	__inout		efx_filter_spec_t *spec,
	__in		uint32_t src_ip,
	__in		uint16_t src_tcp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_type = EFX_FILTER_TX_TCP_WILD;
	spec->efs_flags = 0;
	spec->efs_dword[0] = 0;
	spec->efs_dword[1] = src_tcp << 16;
	spec->efs_dword[2] = src_ip;
}
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_SIENA
extern			void
efx_filter_spec_tx_ipv4_udp_full(
	__inout		efx_filter_spec_t *spec,
	__in		uint32_t src_ip,
	__in		uint16_t src_udp,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_udp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_type = EFX_FILTER_TX_UDP_FULL;
	spec->efs_flags = 0;
	spec->efs_dword[0] = src_udp | src_ip << 16;
	spec->efs_dword[1] = dest_udp << 16 | src_ip >> 16;
	spec->efs_dword[2] = dest_ip;
}
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_SIENA
extern			void
efx_filter_spec_tx_ipv4_udp_wild(
	__inout		efx_filter_spec_t *spec,
	__in		uint32_t src_ip,
	__in		uint16_t src_udp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_type = EFX_FILTER_TX_UDP_WILD;
	spec->efs_flags = 0;
	spec->efs_dword[0] = src_udp;
	spec->efs_dword[1] = 0;
	spec->efs_dword[2] = src_ip;
}
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_SIENA
extern			void
efx_filter_spec_tx_mac_full(
	__inout		efx_filter_spec_t *spec,
	__in		uint16_t vlan_id,
	__in		uint8_t *src_mac)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(src_mac, !=, NULL);

	spec->efs_type = EFX_FILTER_TX_MAC_FULL;
	spec->efs_flags = 0;
	spec->efs_dword[0] = vlan_id;
	spec->efs_dword[1] =
	    src_mac[2] << 24 |
	    src_mac[3] << 16 |
	    src_mac[4] <<  8 |
	    src_mac[5];
 	spec->efs_dword[2] =
	    src_mac[0] <<  8 |
	    src_mac[1];
}
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_SIENA
extern			void
efx_filter_spec_tx_mac_wild(
	__inout		efx_filter_spec_t *spec,
	__in		uint8_t *src_mac)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(src_mac, !=, NULL);

	spec->efs_type = EFX_FILTER_TX_MAC_WILD;
	spec->efs_flags = 0;
	spec->efs_dword[0] = 0;
	spec->efs_dword[1] =
	    src_mac[2] << 24 |
	    src_mac[3] << 16 |
	    src_mac[4] <<  8 |
	    src_mac[5];
 	spec->efs_dword[2] =
	    src_mac[0] <<  8 |
	    src_mac[1];
}
#endif	/* EFSYS_OPT_SIENA */


#endif /* EFSYS_OPT_FILTER */
