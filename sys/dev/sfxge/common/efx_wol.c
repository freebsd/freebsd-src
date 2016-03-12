/*-
 * Copyright (c) 2009-2015 Solarflare Communications Inc.
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

#if EFSYS_OPT_WOL

	__checkReturn	efx_rc_t
efx_wol_init(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_WOL));

	if (~(encp->enc_features) & EFX_FEATURE_WOL) {
		rc = ENOTSUP;
		goto fail1;
	}

	/* Current implementation is Siena specific */
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	enp->en_mod_flags |= EFX_MOD_WOL;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_wol_filter_clear(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_WOL_FILTER_RESET_IN_LEN,
			    MC_CMD_WOL_FILTER_RESET_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_WOL);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_WOL_FILTER_RESET;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_WOL_FILTER_RESET_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_WOL_FILTER_RESET_OUT_LEN;

	MCDI_IN_SET_DWORD(req, WOL_FILTER_RESET_IN_MASK,
			    MC_CMD_WOL_FILTER_RESET_IN_WAKE_FILTERS |
			    MC_CMD_WOL_FILTER_RESET_IN_LIGHTSOUT_OFFLOADS);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_wol_filter_add(
	__in		efx_nic_t *enp,
	__in		efx_wol_type_t type,
	__in		efx_wol_param_t *paramp,
	__out		uint32_t *filter_idp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_WOL_FILTER_SET_IN_LEN,
			    MC_CMD_WOL_FILTER_SET_OUT_LEN)];
	efx_byte_t link_mask;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_WOL);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_WOL_FILTER_SET;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_WOL_FILTER_SET_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_WOL_FILTER_SET_OUT_LEN;

	switch (type) {
	case EFX_WOL_TYPE_MAGIC:
		MCDI_IN_SET_DWORD(req, WOL_FILTER_SET_IN_FILTER_MODE,
				    MC_CMD_FILTER_MODE_SIMPLE);
		MCDI_IN_SET_DWORD(req, WOL_FILTER_SET_IN_WOL_TYPE,
				    MC_CMD_WOL_TYPE_MAGIC);
		EFX_MAC_ADDR_COPY(
			MCDI_IN2(req, uint8_t, WOL_FILTER_SET_IN_MAGIC_MAC),
			paramp->ewp_magic.mac_addr);
		break;

	case EFX_WOL_TYPE_BITMAP: {
		uint32_t swapped = 0;
		efx_dword_t *dwordp;
		unsigned int pos, bit;

		MCDI_IN_SET_DWORD(req, WOL_FILTER_SET_IN_FILTER_MODE,
				    MC_CMD_FILTER_MODE_SIMPLE);
		MCDI_IN_SET_DWORD(req, WOL_FILTER_SET_IN_WOL_TYPE,
				    MC_CMD_WOL_TYPE_BITMAP);

		/*
		 * MC bitmask is supposed to be bit swapped
		 * amongst 32 bit words(!)
		 */

		dwordp = MCDI_IN2(req, efx_dword_t,
				    WOL_FILTER_SET_IN_BITMAP_MASK);

		EFSYS_ASSERT3U(EFX_WOL_BITMAP_MASK_SIZE % 4, ==, 0);

		for (pos = 0; pos < EFX_WOL_BITMAP_MASK_SIZE; ++pos) {
			uint8_t native = paramp->ewp_bitmap.mask[pos];

			for (bit = 0; bit < 8; ++bit) {
				swapped <<= 1;
				swapped |= (native & 0x1);
				native >>= 1;
			}

			if ((pos & 3) == 3) {
				EFX_POPULATE_DWORD_1(dwordp[pos >> 2],
				    EFX_DWORD_0, swapped);
				swapped = 0;
			}
		}

		memcpy(MCDI_IN2(req, uint8_t, WOL_FILTER_SET_IN_BITMAP_BITMAP),
		    paramp->ewp_bitmap.value,
		    sizeof (paramp->ewp_bitmap.value));

		EFSYS_ASSERT3U(paramp->ewp_bitmap.value_len, <=,
				    sizeof (paramp->ewp_bitmap.value));
		MCDI_IN_SET_DWORD(req, WOL_FILTER_SET_IN_BITMAP_LEN,
				    paramp->ewp_bitmap.value_len);
		}
		break;

	case EFX_WOL_TYPE_LINK:
		MCDI_IN_SET_DWORD(req, WOL_FILTER_SET_IN_FILTER_MODE,
				    MC_CMD_FILTER_MODE_SIMPLE);
		MCDI_IN_SET_DWORD(req, WOL_FILTER_SET_IN_WOL_TYPE,
				    MC_CMD_WOL_TYPE_LINK);

		EFX_ZERO_BYTE(link_mask);
		EFX_SET_BYTE_FIELD(link_mask, MC_CMD_WOL_FILTER_SET_IN_LINK_UP,
		    1);
		MCDI_IN_SET_BYTE(req, WOL_FILTER_SET_IN_LINK_MASK,
		    link_mask.eb_u8[0]);
		break;

	default:
		EFSYS_ASSERT3U(type, !=, type);
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_WOL_FILTER_SET_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*filter_idp = MCDI_OUT_DWORD(req, WOL_FILTER_SET_OUT_FILTER_ID);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_wol_filter_remove(
	__in		efx_nic_t *enp,
	__in		uint32_t filter_id)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_WOL_FILTER_REMOVE_IN_LEN,
			    MC_CMD_WOL_FILTER_REMOVE_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_WOL);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_WOL_FILTER_REMOVE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_WOL_FILTER_REMOVE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_WOL_FILTER_REMOVE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, WOL_FILTER_REMOVE_IN_FILTER_ID, filter_id);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
efx_lightsout_offload_add(
	__in		efx_nic_t *enp,
	__in		efx_lightsout_offload_type_t type,
	__in		efx_lightsout_offload_param_t *paramp,
	__out		uint32_t *filter_idp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MAX(MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_ARP_LEN,
				MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS_LEN),
			    MC_CMD_ADD_LIGHTSOUT_OFFLOAD_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_WOL);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_ADD_LIGHTSOUT_OFFLOAD;
	req.emr_in_buf = payload;
	req.emr_in_length = sizeof (type);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_ADD_LIGHTSOUT_OFFLOAD_OUT_LEN;

	switch (type) {
	case EFX_LIGHTSOUT_OFFLOAD_TYPE_ARP:
		req.emr_in_length = MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_ARP_LEN;

		MCDI_IN_SET_DWORD(req, ADD_LIGHTSOUT_OFFLOAD_IN_PROTOCOL,
				    MC_CMD_LIGHTSOUT_OFFLOAD_PROTOCOL_ARP);
		EFX_MAC_ADDR_COPY(MCDI_IN2(req, uint8_t,
					    ADD_LIGHTSOUT_OFFLOAD_IN_ARP_MAC),
				    paramp->elop_arp.mac_addr);
		MCDI_IN_SET_DWORD(req, ADD_LIGHTSOUT_OFFLOAD_IN_ARP_IP,
				    paramp->elop_arp.ip);
		break;
	case EFX_LIGHTSOUT_OFFLOAD_TYPE_NS:
		req.emr_in_length = MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS_LEN;

		MCDI_IN_SET_DWORD(req, ADD_LIGHTSOUT_OFFLOAD_IN_PROTOCOL,
				    MC_CMD_LIGHTSOUT_OFFLOAD_PROTOCOL_NS);
		EFX_MAC_ADDR_COPY(MCDI_IN2(req, uint8_t,
					    ADD_LIGHTSOUT_OFFLOAD_IN_NS_MAC),
				    paramp->elop_ns.mac_addr);
		memcpy(MCDI_IN2(req, uint8_t,
		    ADD_LIGHTSOUT_OFFLOAD_IN_NS_SNIPV6),
		    paramp->elop_ns.solicited_node,
		    sizeof (paramp->elop_ns.solicited_node));
		memcpy(MCDI_IN2(req, uint8_t, ADD_LIGHTSOUT_OFFLOAD_IN_NS_IPV6),
		    paramp->elop_ns.ip, sizeof (paramp->elop_ns.ip));
		break;
	default:
		rc = EINVAL;
		goto fail1;
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_ADD_LIGHTSOUT_OFFLOAD_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	*filter_idp = MCDI_OUT_DWORD(req, ADD_LIGHTSOUT_OFFLOAD_OUT_FILTER_ID);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
efx_lightsout_offload_remove(
	__in		efx_nic_t *enp,
	__in		efx_lightsout_offload_type_t type,
	__in		uint32_t filter_id)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_IN_LEN,
			    MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_WOL);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_OUT_LEN;

	switch (type) {
	case EFX_LIGHTSOUT_OFFLOAD_TYPE_ARP:
		MCDI_IN_SET_DWORD(req, REMOVE_LIGHTSOUT_OFFLOAD_IN_PROTOCOL,
				    MC_CMD_LIGHTSOUT_OFFLOAD_PROTOCOL_ARP);
		break;
	case EFX_LIGHTSOUT_OFFLOAD_TYPE_NS:
		MCDI_IN_SET_DWORD(req, REMOVE_LIGHTSOUT_OFFLOAD_IN_PROTOCOL,
				    MC_CMD_LIGHTSOUT_OFFLOAD_PROTOCOL_NS);
		break;
	default:
		rc = EINVAL;
		goto fail1;
	}

	MCDI_IN_SET_DWORD(req, REMOVE_LIGHTSOUT_OFFLOAD_IN_FILTER_ID,
			    filter_id);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


			void
efx_wol_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_WOL);

	enp->en_mod_flags &= ~EFX_MOD_WOL;
}

#endif	/* EFSYS_OPT_WOL */
