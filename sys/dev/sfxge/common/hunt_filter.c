/*-
 * Copyright (c) 2007-2015 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_HUNTINGTON

#if EFSYS_OPT_FILTER

#define	EFE_SPEC(eftp, index)	((eftp)->eft_entry[(index)].efe_spec)

static			efx_filter_spec_t *
ef10_filter_entry_spec(
	__in		const ef10_filter_table_t *eftp,
	__in		unsigned int index)
{
	return ((efx_filter_spec_t *)(EFE_SPEC(eftp, index) &
		~(uintptr_t)EFX_EF10_FILTER_FLAGS));
}

static			boolean_t
ef10_filter_entry_is_busy(
	__in		const ef10_filter_table_t *eftp,
	__in		unsigned int index)
{
	if (EFE_SPEC(eftp, index) & EFX_EF10_FILTER_FLAG_BUSY)
		return (B_TRUE);
	else
		return (B_FALSE);
}

static			boolean_t
ef10_filter_entry_is_auto_old(
	__in		const ef10_filter_table_t *eftp,
	__in		unsigned int index)
{
	if (EFE_SPEC(eftp, index) & EFX_EF10_FILTER_FLAG_AUTO_OLD)
		return (B_TRUE);
	else
		return (B_FALSE);
}

static			void
ef10_filter_set_entry(
	__inout		ef10_filter_table_t *eftp,
	__in		unsigned int index,
	__in_opt	const efx_filter_spec_t *efsp)
{
	EFE_SPEC(eftp, index) = (uintptr_t)efsp;
}

static			void
ef10_filter_set_entry_busy(
	__inout		ef10_filter_table_t *eftp,
	__in		unsigned int index)
{
	EFE_SPEC(eftp, index) |= (uintptr_t)EFX_EF10_FILTER_FLAG_BUSY;
}

static			void
ef10_filter_set_entry_not_busy(
	__inout		ef10_filter_table_t *eftp,
	__in		unsigned int index)
{
	EFE_SPEC(eftp, index) &= ~(uintptr_t)EFX_EF10_FILTER_FLAG_BUSY;
}

static			void
ef10_filter_set_entry_auto_old(
	__inout		ef10_filter_table_t *eftp,
	__in		unsigned int index)
{
	EFSYS_ASSERT(ef10_filter_entry_spec(eftp, index) != NULL);
	EFE_SPEC(eftp, index) |= (uintptr_t)EFX_EF10_FILTER_FLAG_AUTO_OLD;
}

static			void
ef10_filter_set_entry_not_auto_old(
	__inout		ef10_filter_table_t *eftp,
	__in		unsigned int index)
{
	EFE_SPEC(eftp, index) &= ~(uintptr_t)EFX_EF10_FILTER_FLAG_AUTO_OLD;
	EFSYS_ASSERT(ef10_filter_entry_spec(eftp, index) != NULL);
}

	__checkReturn	efx_rc_t
ef10_filter_init(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;
	ef10_filter_table_t *eftp;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

#define	MATCH_MASK(match) (EFX_MASK32(match) << EFX_LOW_BIT(match))
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_REM_HOST ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_SRC_IP));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_LOC_HOST ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_DST_IP));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_REM_MAC ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_SRC_MAC));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_REM_PORT ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_SRC_PORT));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_LOC_MAC ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_DST_MAC));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_LOC_PORT ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_DST_PORT));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_ETHER_TYPE ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_ETHER_TYPE));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_INNER_VID ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_INNER_VLAN));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_OUTER_VID ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_OUTER_VLAN));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_IP_PROTO ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_IP_PROTO));
#undef MATCH_MASK

	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (ef10_filter_table_t), eftp);

	if (!eftp) {
		rc = ENOMEM;
		goto fail1;
	}

	enp->en_filter.ef_ef10_filter_table = eftp;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
ef10_filter_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	if (enp->en_filter.ef_ef10_filter_table != NULL) {
		EFSYS_KMEM_FREE(enp->en_esip, sizeof (ef10_filter_table_t),
		    enp->en_filter.ef_ef10_filter_table);
	}
}

static	__checkReturn	efx_rc_t
efx_mcdi_filter_op_add(
	__in		efx_nic_t *enp,
	__in		efx_filter_spec_t *spec,
	__in		unsigned int filter_op,
	__inout		ef10_filter_handle_t *handle)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_FILTER_OP_IN_LEN,
			    MC_CMD_FILTER_OP_OUT_LEN)];
	uint32_t match_fields = 0;
	efx_rc_t rc;

	memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_FILTER_OP;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FILTER_OP_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FILTER_OP_OUT_LEN;

	switch (filter_op) {
	case MC_CMD_FILTER_OP_IN_OP_REPLACE:
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_HANDLE_LO,
		    handle->efh_lo);
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_HANDLE_HI,
		    handle->efh_hi);
		/* Fall through */
	case MC_CMD_FILTER_OP_IN_OP_INSERT:
	case MC_CMD_FILTER_OP_IN_OP_SUBSCRIBE:
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_OP, filter_op);
		break;
	default:
		EFSYS_ASSERT(0);
		rc = EINVAL;
		goto fail1;
	}

	if (spec->efs_match_flags & EFX_FILTER_MATCH_LOC_MAC_IG) {
		/*
		 * The LOC_MAC_IG match flag can represent unknown unicast
		 *  or multicast filters - use the MAC address to distinguish
		 *  them.
		 */
		if (EFX_MAC_ADDR_IS_MULTICAST(spec->efs_loc_mac))
			match_fields |= 1U <<
				MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_MCAST_DST_LBN;
		else
			match_fields |= 1U <<
				MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_UCAST_DST_LBN;
	}

	match_fields |= spec->efs_match_flags & (~EFX_FILTER_MATCH_LOC_MAC_IG);

	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_PORT_ID,
	    EVB_PORT_ID_ASSIGNED);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_MATCH_FIELDS,
	    match_fields);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_RX_DEST,
	    MC_CMD_FILTER_OP_IN_RX_DEST_HOST);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_RX_QUEUE,
	    spec->efs_dmaq_id);
	if (spec->efs_flags & EFX_FILTER_FLAG_RX_RSS) {
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_RX_CONTEXT,
		    spec->efs_rss_context);
	}
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_RX_MODE,
	    spec->efs_flags & EFX_FILTER_FLAG_RX_RSS ?
	    MC_CMD_FILTER_OP_IN_RX_MODE_RSS :
	    MC_CMD_FILTER_OP_IN_RX_MODE_SIMPLE);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_TX_DEST,
	    MC_CMD_FILTER_OP_IN_TX_DEST_DEFAULT);

	if (filter_op != MC_CMD_FILTER_OP_IN_OP_REPLACE) {
		/*
		 * NOTE: Unlike most MCDI requests, the filter fields
		 * are presented in network (big endian) byte order.
		 */
		memcpy(MCDI_IN2(req, uint8_t, FILTER_OP_IN_SRC_MAC),
		    spec->efs_rem_mac, EFX_MAC_ADDR_LEN);
		memcpy(MCDI_IN2(req, uint8_t, FILTER_OP_IN_DST_MAC),
		    spec->efs_loc_mac, EFX_MAC_ADDR_LEN);

		MCDI_IN_SET_WORD(req, FILTER_OP_IN_SRC_PORT,
		    __CPU_TO_BE_16(spec->efs_rem_port));
		MCDI_IN_SET_WORD(req, FILTER_OP_IN_DST_PORT,
		    __CPU_TO_BE_16(spec->efs_loc_port));

		MCDI_IN_SET_WORD(req, FILTER_OP_IN_ETHER_TYPE,
		    __CPU_TO_BE_16(spec->efs_ether_type));

		MCDI_IN_SET_WORD(req, FILTER_OP_IN_INNER_VLAN,
		    __CPU_TO_BE_16(spec->efs_inner_vid));
		MCDI_IN_SET_WORD(req, FILTER_OP_IN_OUTER_VLAN,
		    __CPU_TO_BE_16(spec->efs_outer_vid));

		/* IP protocol (in low byte, high byte is zero) */
		MCDI_IN_SET_BYTE(req, FILTER_OP_IN_IP_PROTO,
		    spec->efs_ip_proto);

		EFX_STATIC_ASSERT(sizeof (spec->efs_rem_host) ==
		    MC_CMD_FILTER_OP_IN_SRC_IP_LEN);
		EFX_STATIC_ASSERT(sizeof (spec->efs_loc_host) ==
		    MC_CMD_FILTER_OP_IN_DST_IP_LEN);

		memcpy(MCDI_IN2(req, uint8_t, FILTER_OP_IN_SRC_IP),
		    &spec->efs_rem_host.eo_byte[0],
		    MC_CMD_FILTER_OP_IN_SRC_IP_LEN);
		memcpy(MCDI_IN2(req, uint8_t, FILTER_OP_IN_DST_IP),
		    &spec->efs_loc_host.eo_byte[0],
		    MC_CMD_FILTER_OP_IN_DST_IP_LEN);
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_FILTER_OP_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	handle->efh_lo = MCDI_OUT_DWORD(req, FILTER_OP_OUT_HANDLE_LO);
	handle->efh_hi = MCDI_OUT_DWORD(req, FILTER_OP_OUT_HANDLE_HI);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);

}

static	__checkReturn	efx_rc_t
efx_mcdi_filter_op_delete(
	__in		efx_nic_t *enp,
	__in		unsigned int filter_op,
	__inout		ef10_filter_handle_t *handle)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_FILTER_OP_IN_LEN,
			    MC_CMD_FILTER_OP_OUT_LEN)];
	efx_rc_t rc;

	memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_FILTER_OP;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FILTER_OP_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FILTER_OP_OUT_LEN;

	switch (filter_op) {
	case MC_CMD_FILTER_OP_IN_OP_REMOVE:
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_OP,
		    MC_CMD_FILTER_OP_IN_OP_REMOVE);
		break;
	case MC_CMD_FILTER_OP_IN_OP_UNSUBSCRIBE:
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_OP,
		    MC_CMD_FILTER_OP_IN_OP_UNSUBSCRIBE);
		break;
	default:
		EFSYS_ASSERT(0);
		rc = EINVAL;
		goto fail1;
	}

	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_HANDLE_LO, handle->efh_lo);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_HANDLE_HI, handle->efh_hi);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_FILTER_OP_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	boolean_t
ef10_filter_equal(
	__in		const efx_filter_spec_t *left,
	__in		const efx_filter_spec_t *right)
{
	/* FIXME: Consider rx vs tx filters (look at efs_flags) */
	if (left->efs_match_flags != right->efs_match_flags)
		return (B_FALSE);
	if (!EFX_OWORD_IS_EQUAL(left->efs_rem_host, right->efs_rem_host))
		return (B_FALSE);
	if (!EFX_OWORD_IS_EQUAL(left->efs_loc_host, right->efs_loc_host))
		return (B_FALSE);
	if (memcmp(left->efs_rem_mac, right->efs_rem_mac, EFX_MAC_ADDR_LEN))
		return (B_FALSE);
	if (memcmp(left->efs_loc_mac, right->efs_loc_mac, EFX_MAC_ADDR_LEN))
		return (B_FALSE);
	if (left->efs_rem_port != right->efs_rem_port)
		return (B_FALSE);
	if (left->efs_loc_port != right->efs_loc_port)
		return (B_FALSE);
	if (left->efs_inner_vid != right->efs_inner_vid)
		return (B_FALSE);
	if (left->efs_outer_vid != right->efs_outer_vid)
		return (B_FALSE);
	if (left->efs_ether_type != right->efs_ether_type)
		return (B_FALSE);
	if (left->efs_ip_proto != right->efs_ip_proto)
		return (B_FALSE);

	return (B_TRUE);

}

static	__checkReturn	boolean_t
ef10_filter_same_dest(
	__in		const efx_filter_spec_t *left,
	__in		const efx_filter_spec_t *right)
{
	if ((left->efs_flags & EFX_FILTER_FLAG_RX_RSS) &&
	    (right->efs_flags & EFX_FILTER_FLAG_RX_RSS)) {
		if (left->efs_rss_context == right->efs_rss_context)
			return (B_TRUE);
	} else if ((~(left->efs_flags) & EFX_FILTER_FLAG_RX_RSS) &&
	    (~(right->efs_flags) & EFX_FILTER_FLAG_RX_RSS)) {
		if (left->efs_dmaq_id == right->efs_dmaq_id)
			return (B_TRUE);
	}
	return (B_FALSE);
}

static	__checkReturn	uint32_t
ef10_filter_hash(
	__in		efx_filter_spec_t *spec)
{
	EFX_STATIC_ASSERT((sizeof (efx_filter_spec_t) % sizeof (uint32_t))
			    == 0);
	EFX_STATIC_ASSERT((EFX_FIELD_OFFSET(efx_filter_spec_t, efs_outer_vid) %
			    sizeof (uint32_t)) == 0);

	/*
	 * As the area of the efx_filter_spec_t we need to hash is DWORD
	 * aligned and an exact number of DWORDs in size we can use the
	 * optimised efx_hash_dwords() rather than efx_hash_bytes()
	 */
	return (efx_hash_dwords((const uint32_t *)&spec->efs_outer_vid,
			(sizeof (efx_filter_spec_t) -
			EFX_FIELD_OFFSET(efx_filter_spec_t, efs_outer_vid)) /
			sizeof (uint32_t), 0));
}

/*
 * Decide whether a filter should be exclusive or else should allow
 * delivery to additional recipients.  Currently we decide that
 * filters for specific local unicast MAC and IP addresses are
 * exclusive.
 */
static	__checkReturn	boolean_t
ef10_filter_is_exclusive(
	__in		efx_filter_spec_t *spec)
{
	if ((spec->efs_match_flags & EFX_FILTER_MATCH_LOC_MAC) &&
	    !EFX_MAC_ADDR_IS_MULTICAST(spec->efs_loc_mac))
		return (B_TRUE);

	if ((spec->efs_match_flags &
		(EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_LOC_HOST)) ==
	    (EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_LOC_HOST)) {
		if ((spec->efs_ether_type == EFX_ETHER_TYPE_IPV4) &&
		    ((spec->efs_loc_host.eo_u8[0] & 0xf) != 0xe))
			return (B_TRUE);
		if ((spec->efs_ether_type == EFX_ETHER_TYPE_IPV6) &&
		    (spec->efs_loc_host.eo_u8[0] != 0xff))
			return (B_TRUE);
	}

	return (B_FALSE);
}

	__checkReturn	efx_rc_t
ef10_filter_restore(
	__in		efx_nic_t *enp)
{
	int tbl_id;
	efx_filter_spec_t *spec;
	ef10_filter_table_t *eftp = enp->en_filter.ef_ef10_filter_table;
	boolean_t restoring;
	int state;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	for (tbl_id = 0; tbl_id < EFX_EF10_FILTER_TBL_ROWS; tbl_id++) {

		EFSYS_LOCK(enp->en_eslp, state);

		spec = ef10_filter_entry_spec(eftp, tbl_id);
		if (spec == NULL) {
			restoring = B_FALSE;
		} else if (ef10_filter_entry_is_busy(eftp, tbl_id)) {
			/* Ignore busy entries. */
			restoring = B_FALSE;
		} else {
			ef10_filter_set_entry_busy(eftp, tbl_id);
			restoring = B_TRUE;
		}

		EFSYS_UNLOCK(enp->en_eslp, state);

		if (restoring == B_FALSE)
			continue;

		if (ef10_filter_is_exclusive(spec)) {
			rc = efx_mcdi_filter_op_add(enp, spec,
			    MC_CMD_FILTER_OP_IN_OP_INSERT,
			    &eftp->eft_entry[tbl_id].efe_handle);
		} else {
			rc = efx_mcdi_filter_op_add(enp, spec,
			    MC_CMD_FILTER_OP_IN_OP_SUBSCRIBE,
			    &eftp->eft_entry[tbl_id].efe_handle);
		}

		if (rc != 0)
			goto fail1;

		EFSYS_LOCK(enp->en_eslp, state);

		ef10_filter_set_entry_not_busy(eftp, tbl_id);

		EFSYS_UNLOCK(enp->en_eslp, state);
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

/*
 * An arbitrary search limit for the software hash table. As per the linux net
 * driver.
 */
#define	EF10_FILTER_SEARCH_LIMIT 200

static	__checkReturn	efx_rc_t
ef10_filter_add_internal(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec,
	__in		boolean_t may_replace,
	__out_opt	uint32_t *filter_id)
{
	efx_rc_t rc;
	ef10_filter_table_t *eftp = enp->en_filter.ef_ef10_filter_table;
	efx_filter_spec_t *saved_spec;
	uint32_t hash;
	unsigned int depth;
	int ins_index;
	boolean_t replacing = B_FALSE;
	unsigned int i;
	int state;
	boolean_t locked = B_FALSE;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

#if EFSYS_OPT_RX_SCALE
	spec->efs_rss_context = enp->en_rss_context;
#endif

	hash = ef10_filter_hash(spec);

	/*
	 * FIXME: Add support for inserting filters of different priorities
	 * and removing lower priority multicast filters (bug 42378)
	 */

	/*
	 * Find any existing filters with the same match tuple or
	 * else a free slot to insert at.  If any of them are busy,
	 * we have to wait and retry.
	 */
	for (;;) {
		ins_index = -1;
		depth = 1;
		EFSYS_LOCK(enp->en_eslp, state);
		locked = B_TRUE;

		for (;;) {
			i = (hash + depth) & (EFX_EF10_FILTER_TBL_ROWS - 1);
			saved_spec = ef10_filter_entry_spec(eftp, i);

			if (!saved_spec) {
				if (ins_index < 0) {
					ins_index = i;
				}
			} else if (ef10_filter_equal(spec, saved_spec)) {
				if (ef10_filter_entry_is_busy(eftp, i))
					break;
				if (saved_spec->efs_priority
					    == EFX_FILTER_PRI_AUTO) {
					ins_index = i;
					goto found;
				} else if (ef10_filter_is_exclusive(spec)) {
					if (may_replace) {
						ins_index = i;
						goto found;
					} else {
						rc = EEXIST;
						goto fail1;
					}
				}

				/* Leave existing */
			}

			/*
			 * Once we reach the maximum search depth, use
			 * the first suitable slot or return EBUSY if
			 * there was none.
			 */
			if (depth == EF10_FILTER_SEARCH_LIMIT) {
				if (ins_index < 0) {
					rc = EBUSY;
					goto fail2;
				}
				goto found;
			}
			depth++;
		}
		EFSYS_UNLOCK(enp->en_eslp, state);
		locked = B_FALSE;
	}

found:
	/*
	 * Create a software table entry if necessary, and mark it
	 * busy.  We might yet fail to insert, but any attempt to
	 * insert a conflicting filter while we're waiting for the
	 * firmware must find the busy entry.
	 */
	saved_spec = ef10_filter_entry_spec(eftp, ins_index);
	if (saved_spec) {
		if (saved_spec->efs_priority == EFX_FILTER_PRI_AUTO) {
			/* This is a filter we are refreshing */
			ef10_filter_set_entry_not_auto_old(eftp, ins_index);
			goto out_unlock;

		}
		replacing = B_TRUE;
	} else {
		EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (*spec), saved_spec);
		if (!saved_spec) {
			rc = ENOMEM;
			goto fail3;
		}
		*saved_spec = *spec;
		ef10_filter_set_entry(eftp, ins_index, saved_spec);
	}
	ef10_filter_set_entry_busy(eftp, ins_index);

	EFSYS_UNLOCK(enp->en_eslp, state);
	locked = B_FALSE;

	/*
	 * On replacing the filter handle may change after after a successful
	 * replace operation.
	 */
	if (replacing) {
		rc = efx_mcdi_filter_op_add(enp, spec,
		    MC_CMD_FILTER_OP_IN_OP_REPLACE,
		    &eftp->eft_entry[ins_index].efe_handle);
	} else if (ef10_filter_is_exclusive(spec)) {
		rc = efx_mcdi_filter_op_add(enp, spec,
		    MC_CMD_FILTER_OP_IN_OP_INSERT,
		    &eftp->eft_entry[ins_index].efe_handle);
	} else {
		rc = efx_mcdi_filter_op_add(enp, spec,
		    MC_CMD_FILTER_OP_IN_OP_SUBSCRIBE,
		    &eftp->eft_entry[ins_index].efe_handle);
	}

	if (rc != 0)
		goto fail4;

	EFSYS_LOCK(enp->en_eslp, state);
	locked = B_TRUE;

	if (replacing) {
		/* Update the fields that may differ */
		saved_spec->efs_priority = spec->efs_priority;
		saved_spec->efs_flags = spec->efs_flags;
		saved_spec->efs_rss_context = spec->efs_rss_context;
		saved_spec->efs_dmaq_id = spec->efs_dmaq_id;
	}

	ef10_filter_set_entry_not_busy(eftp, ins_index);

out_unlock:

	EFSYS_UNLOCK(enp->en_eslp, state);
	locked = B_FALSE;

	if (filter_id)
		*filter_id = ins_index;

	return (0);

fail4:
	EFSYS_PROBE(fail4);

	if (!replacing) {
		EFSYS_KMEM_FREE(enp->en_esip, sizeof (*spec), saved_spec);
		saved_spec = NULL;
	}
	ef10_filter_set_entry_not_busy(eftp, ins_index);
	ef10_filter_set_entry(eftp, ins_index, NULL);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	if (locked)
		EFSYS_UNLOCK(enp->en_eslp, state);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_filter_add(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec,
	__in		boolean_t may_replace)
{
	efx_rc_t rc;

	rc = ef10_filter_add_internal(enp, spec, may_replace, NULL);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


static	__checkReturn	efx_rc_t
ef10_filter_delete_internal(
	__in		efx_nic_t *enp,
	__in		uint32_t filter_id)
{
	efx_rc_t rc;
	ef10_filter_table_t *table = enp->en_filter.ef_ef10_filter_table;
	efx_filter_spec_t *spec;
	int state;
	uint32_t filter_idx = filter_id % EFX_EF10_FILTER_TBL_ROWS;

	/*
	 * Find the software table entry and mark it busy.  Don't
	 * remove it yet; any attempt to update while we're waiting
	 * for the firmware must find the busy entry.
	 *
	 * FIXME: What if the busy flag is never cleared?
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	while (ef10_filter_entry_is_busy(table, filter_idx)) {
		EFSYS_UNLOCK(enp->en_eslp, state);
		EFSYS_SPIN(1);
		EFSYS_LOCK(enp->en_eslp, state);
	}
	if ((spec = ef10_filter_entry_spec(table, filter_idx)) != NULL) {
		ef10_filter_set_entry_busy(table, filter_idx);
	}
	EFSYS_UNLOCK(enp->en_eslp, state);

	if (spec == NULL) {
		rc = ENOENT;
		goto fail1;
	}

	/*
	 * Try to remove the hardware filter. This may fail if the MC has
	 * rebooted (which frees all hardware filter resources).
	 */
	if (ef10_filter_is_exclusive(spec)) {
		rc = efx_mcdi_filter_op_delete(enp,
		    MC_CMD_FILTER_OP_IN_OP_REMOVE,
		    &table->eft_entry[filter_idx].efe_handle);
	} else {
		rc = efx_mcdi_filter_op_delete(enp,
		    MC_CMD_FILTER_OP_IN_OP_UNSUBSCRIBE,
		    &table->eft_entry[filter_idx].efe_handle);
	}

	/* Free the software table entry */
	EFSYS_LOCK(enp->en_eslp, state);
	ef10_filter_set_entry_not_busy(table, filter_idx);
	ef10_filter_set_entry(table, filter_idx, NULL);
	EFSYS_UNLOCK(enp->en_eslp, state);

	EFSYS_KMEM_FREE(enp->en_esip, sizeof (*spec), spec);

	/* Check result of hardware filter removal */
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_filter_delete(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec)
{
	efx_rc_t rc;
	ef10_filter_table_t *table = enp->en_filter.ef_ef10_filter_table;
	efx_filter_spec_t *saved_spec;
	unsigned int hash;
	unsigned int depth;
	unsigned int i;
	int state;
	boolean_t locked = B_FALSE;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	hash = ef10_filter_hash(spec);

	EFSYS_LOCK(enp->en_eslp, state);
	locked = B_TRUE;

	depth = 1;
	for (;;) {
		i = (hash + depth) & (EFX_EF10_FILTER_TBL_ROWS - 1);
		saved_spec = ef10_filter_entry_spec(table, i);
		if (saved_spec && ef10_filter_equal(spec, saved_spec) &&
		    ef10_filter_same_dest(spec, saved_spec)) {
			break;
		}
		if (depth == EF10_FILTER_SEARCH_LIMIT) {
			rc = ENOENT;
			goto fail1;
		}
		depth++;
	}

	EFSYS_UNLOCK(enp->en_eslp, state);
	locked = B_FALSE;

	rc = ef10_filter_delete_internal(enp, i);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	if (locked)
		EFSYS_UNLOCK(enp->en_eslp, state);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_get_parser_disp_info(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_PARSER_DISP_INFO_IN_LEN,
			    MC_CMD_GET_PARSER_DISP_INFO_OUT_LENMAX)];
	efx_rc_t rc;
	uint32_t i;
	boolean_t support_unknown_ucast = B_FALSE;
	boolean_t support_unknown_mcast = B_FALSE;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_PARSER_DISP_INFO;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_PARSER_DISP_INFO_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_PARSER_DISP_INFO_OUT_LENMAX;

	MCDI_IN_SET_DWORD(req, GET_PARSER_DISP_INFO_OUT_OP,
	    MC_CMD_GET_PARSER_DISP_INFO_IN_OP_GET_SUPPORTED_RX_MATCHES);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	*length = MCDI_OUT_DWORD(req,
	    GET_PARSER_DISP_INFO_OUT_NUM_SUPPORTED_MATCHES);

	if (req.emr_out_length_used <
	    MC_CMD_GET_PARSER_DISP_INFO_OUT_LEN(*length)) {
		rc = EMSGSIZE;
		goto fail2;
	}

	memcpy(list,
	    MCDI_OUT2(req,
	    uint32_t,
	    GET_PARSER_DISP_INFO_OUT_SUPPORTED_MATCHES),
	    (*length) * sizeof (uint32_t));
	EFX_STATIC_ASSERT(sizeof (uint32_t) ==
	    MC_CMD_GET_PARSER_DISP_INFO_OUT_SUPPORTED_MATCHES_LEN);

	/*
	 * Remove UNKNOWN UCAST and MCAST flags, and if both are present, change
	 * the lower priority one to LOC_MAC_IG.
	 */
	for (i = 0; i < *length; i++) {
		if (list[i] & MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_UCAST_DST_LBN) {
			list[i] &=
			(~MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_UCAST_DST_LBN);
			support_unknown_ucast = B_TRUE;
		}
		if (list[i] & MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_MCAST_DST_LBN) {
			list[i] &=
			(~MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_MCAST_DST_LBN);
			support_unknown_mcast = B_TRUE;
		}

		if (support_unknown_ucast && support_unknown_mcast) {
			list[i] &= EFX_FILTER_MATCH_LOC_MAC_IG;
			break;
		}
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_filter_supported_filters(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length)
{
	efx_rc_t rc;

	if ((rc = efx_mcdi_get_parser_disp_info(enp, list, length) != 0))
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
ef10_filter_insert_unicast(
	__in				efx_nic_t *enp,
	__in_ecount(6)			uint8_t const *addr,
	__in				efx_filter_flag_t filter_flags)
{
	ef10_filter_table_t *eftp = enp->en_filter.ef_ef10_filter_table;
	efx_filter_spec_t spec;
	efx_rc_t rc;

	/* Insert the filter for the local station address */
	efx_filter_spec_init_rx(&spec, EFX_FILTER_PRI_AUTO,
	    filter_flags,
	    eftp->eft_default_rxq);
	efx_filter_spec_set_eth_local(&spec, EFX_FILTER_SPEC_VID_UNSPEC, addr);

	rc = ef10_filter_add_internal(enp, &spec, B_TRUE,
	    &eftp->eft_unicst_filter_indexes[eftp->eft_unicst_filter_count]);
	if (rc != 0)
		goto fail1;

	eftp->eft_unicst_filter_count++;
	EFSYS_ASSERT(eftp->eft_unicst_filter_count <=
		    EFX_EF10_FILTER_UNICAST_FILTERS_MAX);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn	efx_rc_t
ef10_filter_insert_all_unicast(
	__in				efx_nic_t *enp,
	__in				efx_filter_flag_t filter_flags)
{
	ef10_filter_table_t *eftp = enp->en_filter.ef_ef10_filter_table;
	efx_filter_spec_t spec;
	efx_rc_t rc;

	/* Insert the unknown unicast filter */
	efx_filter_spec_init_rx(&spec, EFX_FILTER_PRI_AUTO,
	    filter_flags,
	    eftp->eft_default_rxq);
	efx_filter_spec_set_uc_def(&spec);
	rc = ef10_filter_add_internal(enp, &spec, B_TRUE,
	    &eftp->eft_unicst_filter_indexes[eftp->eft_unicst_filter_count]);
	if (rc != 0)
		goto fail1;

	eftp->eft_unicst_filter_count++;
	EFSYS_ASSERT(eftp->eft_unicst_filter_count <=
		    EFX_EF10_FILTER_UNICAST_FILTERS_MAX);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn	efx_rc_t
ef10_filter_insert_multicast_list(
	__in				efx_nic_t *enp,
	__in				boolean_t mulcst,
	__in				boolean_t brdcst,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				uint32_t count,
	__in				efx_filter_flag_t filter_flags,
	__in				boolean_t rollback)
{
	ef10_filter_table_t *eftp = enp->en_filter.ef_ef10_filter_table;
	efx_filter_spec_t spec;
	uint8_t addr[6];
	uint32_t i;
	uint32_t filter_index;
	uint32_t filter_count;
	efx_rc_t rc;

	if (mulcst == B_FALSE)
		count = 0;

	if (count + (brdcst ? 1 : 0) >
	    EFX_ARRAY_SIZE(eftp->eft_mulcst_filter_indexes)) {
		/* Too many MAC addresses */
		rc = EINVAL;
		goto fail1;
	}

	/* Insert/renew multicast address list filters */
	filter_count = 0;
	for (i = 0; i < count; i++) {
		efx_filter_spec_init_rx(&spec,
		    EFX_FILTER_PRI_AUTO,
		    filter_flags,
		    eftp->eft_default_rxq);

		efx_filter_spec_set_eth_local(&spec,
		    EFX_FILTER_SPEC_VID_UNSPEC,
		    &addrs[i * EFX_MAC_ADDR_LEN]);

		rc = ef10_filter_add_internal(enp, &spec, B_TRUE,
					    &filter_index);

		if (rc == 0) {
			eftp->eft_mulcst_filter_indexes[filter_count] =
				filter_index;
			filter_count++;
		} else if (rollback == B_TRUE) {
			/* Only stop upon failure if told to rollback */
			goto rollback;
		}

	}

	if (brdcst == B_TRUE) {
		/* Insert/renew broadcast address filter */
		efx_filter_spec_init_rx(&spec, EFX_FILTER_PRI_AUTO,
		    filter_flags,
		    eftp->eft_default_rxq);

		EFX_MAC_BROADCAST_ADDR_SET(addr);
		efx_filter_spec_set_eth_local(&spec, EFX_FILTER_SPEC_VID_UNSPEC,
		    addr);

		rc = ef10_filter_add_internal(enp, &spec, B_TRUE,
					    &filter_index);

		if (rc == 0) {
			eftp->eft_mulcst_filter_indexes[filter_count] =
				filter_index;
			filter_count++;
		} else if (rollback == B_TRUE) {
			/* Only stop upon failure if told to rollback */
			goto rollback;
		}
	}

	eftp->eft_mulcst_filter_count = filter_count;
	eftp->eft_using_all_mulcst = B_FALSE;

	return (0);

rollback:
	/* Remove any filters we have inserted */
	i = filter_count;
	while (i--) {
		(void) ef10_filter_delete_internal(enp,
		    eftp->eft_mulcst_filter_indexes[i]);
	}
	eftp->eft_mulcst_filter_count = 0;

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
ef10_filter_insert_all_multicast(
	__in				efx_nic_t *enp,
	__in				efx_filter_flag_t filter_flags)
{
	ef10_filter_table_t *eftp = enp->en_filter.ef_ef10_filter_table;
	efx_filter_spec_t spec;
	efx_rc_t rc;

	/* Insert the unknown multicast filter */
	efx_filter_spec_init_rx(&spec, EFX_FILTER_PRI_AUTO,
	    filter_flags,
	    eftp->eft_default_rxq);
	efx_filter_spec_set_mc_def(&spec);

	rc = ef10_filter_add_internal(enp, &spec, B_TRUE,
	    &eftp->eft_mulcst_filter_indexes[0]);
	if (rc != 0)
		goto fail1;

	eftp->eft_mulcst_filter_count = 1;
	eftp->eft_using_all_mulcst = B_TRUE;

	/*
	 * FIXME: If brdcst == B_FALSE, add a filter to drop broadcast traffic.
	 */

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static			void
ef10_filter_remove_old(
	__in		efx_nic_t *enp)
{
	ef10_filter_table_t *table = enp->en_filter.ef_ef10_filter_table;
	uint32_t i;

	for (i = 0; i < EFX_ARRAY_SIZE(table->eft_entry); i++) {
		if (ef10_filter_entry_is_auto_old(table, i)) {
			(void) ef10_filter_delete_internal(enp, i);
		}
	}
}


static	__checkReturn	efx_rc_t
ef10_filter_get_workarounds(
	__in				efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	uint32_t implemented = 0;
	uint32_t enabled = 0;
	efx_rc_t rc;

	rc = efx_mcdi_get_workarounds(enp, &implemented, &enabled);
	if (rc == 0) {
		/* Check if chained multicast filter support is enabled */
		if (implemented & enabled & MC_CMD_GET_WORKAROUNDS_OUT_BUG26807)
			encp->enc_bug26807_workaround = B_TRUE;
		else
			encp->enc_bug26807_workaround = B_FALSE;
	} else if (rc == ENOTSUP) {
		/*
		 * Firmware is too old to support GET_WORKAROUNDS, and support
		 * for this workaround was implemented later.
		 */
		encp->enc_bug26807_workaround = B_FALSE;
	} else {
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);

}


/*
 * Reconfigure all filters.
 * If all_unicst and/or all mulcst filters cannot be applied then
 * return ENOTSUP (Note the filters for the specified addresses are
 * still applied in this case).
 */
	__checkReturn	efx_rc_t
ef10_filter_reconfigure(
	__in				efx_nic_t *enp,
	__in_ecount(6)			uint8_t const *mac_addr,
	__in				boolean_t all_unicst,
	__in				boolean_t mulcst,
	__in				boolean_t all_mulcst,
	__in				boolean_t brdcst,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				uint32_t count)
{
	efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	ef10_filter_table_t *table = enp->en_filter.ef_ef10_filter_table;
	efx_filter_flag_t filter_flags;
	unsigned i;
	efx_rc_t all_unicst_rc = 0;
	efx_rc_t all_mulcst_rc = 0;
	efx_rc_t rc;

	if (table->eft_default_rxq == NULL) {
		/*
		 * Filters direct traffic to the default RXQ, and so cannot be
		 * inserted until it is available. Any currently configured
		 * filters must be removed (ignore errors in case the MC
		 * has rebooted, which removes hardware filters).
		 */
		for (i = 0; i < table->eft_unicst_filter_count; i++) {
			(void) ef10_filter_delete_internal(enp,
					table->eft_unicst_filter_indexes[i]);
		}
		table->eft_unicst_filter_count = 0;

		for (i = 0; i < table->eft_mulcst_filter_count; i++) {
			(void) ef10_filter_delete_internal(enp,
					table->eft_mulcst_filter_indexes[i]);
		}
		table->eft_mulcst_filter_count = 0;

		return (0);
	}

	if (table->eft_using_rss)
		filter_flags = EFX_FILTER_FLAG_RX_RSS;
	else
		filter_flags = 0;

	/* Mark old filters which may need to be removed */
	for (i = 0; i < table->eft_unicst_filter_count; i++) {
		ef10_filter_set_entry_auto_old(table,
					table->eft_unicst_filter_indexes[i]);
	}
	for (i = 0; i < table->eft_mulcst_filter_count; i++) {
		ef10_filter_set_entry_auto_old(table,
					table->eft_mulcst_filter_indexes[i]);
	}

	/*
	 * Insert or renew unicast filters.
	 *
	 * Frimware does not perform chaining on unicast filters. As traffic is
	 * therefore only delivered to the first matching filter, we should
	 * always insert the specific filter for our MAC address, to try and
	 * ensure we get that traffic.
	 *
	 * (If the filter for our MAC address has already been inserted by
	 * another function, we won't receive traffic sent to us, even if we
	 * insert a unicast mismatch filter. To prevent traffic stealing, this
	 * therefore relies on the privilege model only allowing functions to
	 * insert filters for their own MAC address unless explicitly given
	 * additional privileges by the user. This also means that, even on a
	 * priviliged function, inserting a unicast mismatch filter may not
	 * catch all traffic in multi PCI function scenarios.)
	 */
	table->eft_unicst_filter_count = 0;
	rc = ef10_filter_insert_unicast(enp, mac_addr, filter_flags);
	if (all_unicst || (rc != 0)) {
		all_unicst_rc = ef10_filter_insert_all_unicast(enp,
						    filter_flags);
		if ((rc != 0) && (all_unicst_rc != 0))
			goto fail1;
	}

	/*
	 * WORKAROUND_BUG26807 controls firmware support for chained multicast
	 * filters, and can only be enabled or disabled when the hardware filter
	 * table is empty.
	 *
	 * Chained multicast filters require support from the datapath firmware,
	 * and may not be available (e.g. low-latency variants or old Huntington
	 * firmware).
	 *
	 * Firmware will reset (FLR) functions which have inserted filters in
	 * the hardware filter table when the workaround is enabled/disabled.
	 * Functions without any hardware filters are not reset.
	 *
	 * Re-check if the workaround is enabled after adding unicast hardware
	 * filters. This ensures that encp->enc_bug26807_workaround matches the
	 * firmware state, and that later changes to enable/disable the
	 * workaround will result in this function seeing a reset (FLR).
	 *
	 * In common-code drivers, we only support multiple PCI function
	 * scenarios with firmware that supports multicast chaining, so we can
	 * assume it is enabled for such cases and hence simplify the filter
	 * insertion logic. Firmware that does not support multicast chaining
	 * does not support multiple PCI function configurations either, so
	 * filter insertion is much simpler and the same strategies can still be
	 * used.
	 */
	if ((rc = ef10_filter_get_workarounds(enp)) != 0)
		goto fail2;

	if ((table->eft_using_all_mulcst != all_mulcst) &&
	    (encp->enc_bug26807_workaround == B_TRUE)) {
		/*
		 * Multicast filter chaining is enabled, so traffic that matches
		 * more than one multicast filter will be replicated and
		 * delivered to multiple recipients.  To avoid this duplicate
		 * delivery, remove old multicast filters before inserting new
		 * multicast filters.
		 */
		ef10_filter_remove_old(enp);
	}

	/* Insert or renew multicast filters */
	if (all_mulcst == B_TRUE) {
		/*
		 * Insert the all multicast filter. If that fails, try to insert
		 * all of our multicast filters (but without rollback on
		 * failure).
		 */
		all_mulcst_rc = ef10_filter_insert_all_multicast(enp,
							    filter_flags);
		if (all_mulcst_rc != 0) {
			rc = ef10_filter_insert_multicast_list(enp, B_TRUE,
			    brdcst, addrs, count, filter_flags, B_FALSE);
			if (rc != 0)
				goto fail3;
		}
	} else {
		/*
		 * Insert filters for multicast addresses.
		 * If any insertion fails, then rollback and try to insert the
		 * all multicast filter instead.
		 * If that also fails, try to insert all of the multicast
		 * filters (but without rollback on failure).
		 */
		rc = ef10_filter_insert_multicast_list(enp, mulcst, brdcst,
			    addrs, count, filter_flags, B_TRUE);
		if (rc != 0) {
			if ((table->eft_using_all_mulcst == B_FALSE) &&
			    (encp->enc_bug26807_workaround == B_TRUE)) {
				/*
				 * Multicast filter chaining is on, so remove
				 * old filters before inserting the multicast
				 * all filter to avoid duplicate delivery caused
				 * by packets matching multiple filters.
				 */
				ef10_filter_remove_old(enp);
			}

			rc = ef10_filter_insert_all_multicast(enp,
							    filter_flags);
			if (rc != 0) {
				rc = ef10_filter_insert_multicast_list(enp,
				    mulcst, brdcst,
				    addrs, count, filter_flags, B_FALSE);
				if (rc != 0)
					goto fail4;
			}
		}
	}

	/* Remove old filters which were not renewed */
	ef10_filter_remove_old(enp);

	/* report if any optional flags were rejected */
	if (((all_unicst != B_FALSE) && (all_unicst_rc != 0)) ||
	    ((all_mulcst != B_FALSE) && (all_mulcst_rc != 0))) {
		rc = ENOTSUP;
	}

	return (rc);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	/* Clear auto old flags */
	for (i = 0; i < EFX_ARRAY_SIZE(table->eft_entry); i++) {
		if (ef10_filter_entry_is_auto_old(table, i)) {
			ef10_filter_set_entry_not_auto_old(table, i);
		}
	}

	return (rc);
}

		void
ef10_filter_get_default_rxq(
	__in		efx_nic_t *enp,
	__out		efx_rxq_t **erpp,
	__out		boolean_t *using_rss)
{
	ef10_filter_table_t *table = enp->en_filter.ef_ef10_filter_table;

	*erpp = table->eft_default_rxq;
	*using_rss = table->eft_using_rss;
}


		void
ef10_filter_default_rxq_set(
	__in		efx_nic_t *enp,
	__in		efx_rxq_t *erp,
	__in		boolean_t using_rss)
{
	ef10_filter_table_t *table = enp->en_filter.ef_ef10_filter_table;

#if EFSYS_OPT_RX_SCALE
	EFSYS_ASSERT((using_rss == B_FALSE) ||
	    (enp->en_rss_context != EF10_RSS_CONTEXT_INVALID));
	table->eft_using_rss = using_rss;
#else
	EFSYS_ASSERT(using_rss == B_FALSE);
	table->eft_using_rss = B_FALSE;
#endif
	table->eft_default_rxq = erp;
}

		void
ef10_filter_default_rxq_clear(
	__in		efx_nic_t *enp)
{
	ef10_filter_table_t *table = enp->en_filter.ef_ef10_filter_table;

	table->eft_default_rxq = NULL;
	table->eft_using_rss = B_FALSE;
}


#endif /* EFSYS_OPT_FILTER */

#endif /* EFSYS_OPT_HUNTINGTON */
