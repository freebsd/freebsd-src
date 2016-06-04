/*-
 * Copyright (c) 2012-2015 Solarflare Communications Inc.
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
#if EFSYS_OPT_MON_MCDI
#include "mcdi_mon.h"
#endif

#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD

#include "ef10_tlv_layout.h"

	__checkReturn	efx_rc_t
efx_mcdi_get_port_assignment(
	__in		efx_nic_t *enp,
	__out		uint32_t *portp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_PORT_ASSIGNMENT_IN_LEN,
			    MC_CMD_GET_PORT_ASSIGNMENT_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_PORT_ASSIGNMENT;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_PORT_ASSIGNMENT_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_PORT_ASSIGNMENT_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_PORT_ASSIGNMENT_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*portp = MCDI_OUT_DWORD(req, GET_PORT_ASSIGNMENT_OUT_PORT);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_get_port_modes(
	__in		efx_nic_t *enp,
	__out		uint32_t *modesp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_PORT_MODES_IN_LEN,
			    MC_CMD_GET_PORT_MODES_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_PORT_MODES;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_PORT_MODES_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_PORT_MODES_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	/*
	 * Require only Modes and DefaultMode fields.
	 * (CurrentMode field was added for Medford)
	 */
	if (req.emr_out_length_used <
	    MC_CMD_GET_PORT_MODES_OUT_CURRENT_MODE_OFST) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*modesp = MCDI_OUT_DWORD(req, GET_PORT_MODES_OUT_MODES);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


static	__checkReturn		efx_rc_t
efx_mcdi_vadaptor_alloc(
	__in			efx_nic_t *enp,
	__in			uint32_t port_id)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_VADAPTOR_ALLOC_IN_LEN,
			    MC_CMD_VADAPTOR_ALLOC_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_vport_id, ==, EVB_PORT_ID_NULL);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_VADAPTOR_ALLOC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VADAPTOR_ALLOC_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VADAPTOR_ALLOC_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VADAPTOR_ALLOC_IN_UPSTREAM_PORT_ID, port_id);
	MCDI_IN_POPULATE_DWORD_1(req, VADAPTOR_ALLOC_IN_FLAGS,
	    VADAPTOR_ALLOC_IN_FLAG_PERMIT_SET_MAC_WHEN_FILTERS_INSTALLED,
	    enp->en_nic_cfg.enc_allow_set_mac_with_installed_filters ? 1 : 0);

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

static	__checkReturn		efx_rc_t
efx_mcdi_vadaptor_free(
	__in			efx_nic_t *enp,
	__in			uint32_t port_id)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_VADAPTOR_FREE_IN_LEN,
			    MC_CMD_VADAPTOR_FREE_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_VADAPTOR_FREE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VADAPTOR_FREE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VADAPTOR_FREE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VADAPTOR_FREE_IN_UPSTREAM_PORT_ID, port_id);

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
efx_mcdi_get_mac_address_pf(
	__in			efx_nic_t *enp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6])
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_MAC_ADDRESSES_IN_LEN,
			    MC_CMD_GET_MAC_ADDRESSES_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_MAC_ADDRESSES;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_MAC_ADDRESSES_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_MAC_ADDRESSES_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_MAC_ADDRESSES_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (MCDI_OUT_DWORD(req, GET_MAC_ADDRESSES_OUT_MAC_COUNT) < 1) {
		rc = ENOENT;
		goto fail3;
	}

	if (mac_addrp != NULL) {
		uint8_t *addrp;

		addrp = MCDI_OUT2(req, uint8_t,
		    GET_MAC_ADDRESSES_OUT_MAC_ADDR_BASE);

		EFX_MAC_ADDR_COPY(mac_addrp, addrp);
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

	__checkReturn	efx_rc_t
efx_mcdi_get_mac_address_vf(
	__in			efx_nic_t *enp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6])
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_VPORT_GET_MAC_ADDRESSES_IN_LEN,
			    MC_CMD_VPORT_GET_MAC_ADDRESSES_OUT_LENMAX)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_VPORT_GET_MAC_ADDRESSES;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VPORT_GET_MAC_ADDRESSES_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VPORT_GET_MAC_ADDRESSES_OUT_LENMAX;

	MCDI_IN_SET_DWORD(req, VPORT_GET_MAC_ADDRESSES_IN_VPORT_ID,
	    EVB_PORT_ID_ASSIGNED);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used <
	    MC_CMD_VPORT_GET_MAC_ADDRESSES_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (MCDI_OUT_DWORD(req,
		VPORT_GET_MAC_ADDRESSES_OUT_MACADDR_COUNT) < 1) {
		rc = ENOENT;
		goto fail3;
	}

	if (mac_addrp != NULL) {
		uint8_t *addrp;

		addrp = MCDI_OUT2(req, uint8_t,
		    VPORT_GET_MAC_ADDRESSES_OUT_MACADDR);

		EFX_MAC_ADDR_COPY(mac_addrp, addrp);
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

	__checkReturn	efx_rc_t
efx_mcdi_get_clock(
	__in		efx_nic_t *enp,
	__out		uint32_t *sys_freqp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_CLOCK_IN_LEN,
			    MC_CMD_GET_CLOCK_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_CLOCK;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_CLOCK_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_CLOCK_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_CLOCK_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*sys_freqp = MCDI_OUT_DWORD(req, GET_CLOCK_OUT_SYS_FREQ);
	if (*sys_freqp == 0) {
		rc = EINVAL;
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

	__checkReturn	efx_rc_t
efx_mcdi_get_vector_cfg(
	__in		efx_nic_t *enp,
	__out_opt	uint32_t *vec_basep,
	__out_opt	uint32_t *pf_nvecp,
	__out_opt	uint32_t *vf_nvecp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_VECTOR_CFG_IN_LEN,
			    MC_CMD_GET_VECTOR_CFG_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_VECTOR_CFG;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_VECTOR_CFG_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_VECTOR_CFG_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_VECTOR_CFG_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (vec_basep != NULL)
		*vec_basep = MCDI_OUT_DWORD(req, GET_VECTOR_CFG_OUT_VEC_BASE);
	if (pf_nvecp != NULL)
		*pf_nvecp = MCDI_OUT_DWORD(req, GET_VECTOR_CFG_OUT_VECS_PER_PF);
	if (vf_nvecp != NULL)
		*vf_nvecp = MCDI_OUT_DWORD(req, GET_VECTOR_CFG_OUT_VECS_PER_VF);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_get_capabilities(
	__in		efx_nic_t *enp,
	__out		uint32_t *flagsp,
	__out		uint32_t *flags2p)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_CAPABILITIES_IN_LEN,
			    MC_CMD_GET_CAPABILITIES_V2_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_CAPABILITIES;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_CAPABILITIES_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_CAPABILITIES_V2_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_CAPABILITIES_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*flagsp = MCDI_OUT_DWORD(req, GET_CAPABILITIES_OUT_FLAGS1);

	if (req.emr_out_length_used < MC_CMD_GET_CAPABILITIES_V2_OUT_LEN)
		*flags2p = 0;
	else
		*flags2p = MCDI_OUT_DWORD(req, GET_CAPABILITIES_V2_OUT_FLAGS2);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


static	__checkReturn	efx_rc_t
efx_mcdi_alloc_vis(
	__in		efx_nic_t *enp,
	__in		uint32_t min_vi_count,
	__in		uint32_t max_vi_count,
	__out		uint32_t *vi_basep,
	__out		uint32_t *vi_countp,
	__out		uint32_t *vi_shiftp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_ALLOC_VIS_IN_LEN,
			    MC_CMD_ALLOC_VIS_OUT_LEN)];
	efx_rc_t rc;

	if (vi_countp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_ALLOC_VIS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_ALLOC_VIS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_ALLOC_VIS_OUT_LEN;

	MCDI_IN_SET_DWORD(req, ALLOC_VIS_IN_MIN_VI_COUNT, min_vi_count);
	MCDI_IN_SET_DWORD(req, ALLOC_VIS_IN_MAX_VI_COUNT, max_vi_count);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_ALLOC_VIS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	*vi_basep = MCDI_OUT_DWORD(req, ALLOC_VIS_OUT_VI_BASE);
	*vi_countp = MCDI_OUT_DWORD(req, ALLOC_VIS_OUT_VI_COUNT);

	/* Report VI_SHIFT if available (always zero for Huntington) */
	if (req.emr_out_length_used < MC_CMD_ALLOC_VIS_EXT_OUT_LEN)
		*vi_shiftp = 0;
	else
		*vi_shiftp = MCDI_OUT_DWORD(req, ALLOC_VIS_EXT_OUT_VI_SHIFT);

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
efx_mcdi_free_vis(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	efx_rc_t rc;

	EFX_STATIC_ASSERT(MC_CMD_FREE_VIS_IN_LEN == 0);
	EFX_STATIC_ASSERT(MC_CMD_FREE_VIS_OUT_LEN == 0);

	req.emr_cmd = MC_CMD_FREE_VIS;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	efx_mcdi_execute_quiet(enp, &req);

	/* Ignore ELREADY (no allocated VIs, so nothing to free) */
	if ((req.emr_rc != 0) && (req.emr_rc != EALREADY)) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


static	__checkReturn	efx_rc_t
efx_mcdi_alloc_piobuf(
	__in		efx_nic_t *enp,
	__out		efx_piobuf_handle_t *handlep)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_ALLOC_PIOBUF_IN_LEN,
			    MC_CMD_ALLOC_PIOBUF_OUT_LEN)];
	efx_rc_t rc;

	if (handlep == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_ALLOC_PIOBUF;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_ALLOC_PIOBUF_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_ALLOC_PIOBUF_OUT_LEN;

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_ALLOC_PIOBUF_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	*handlep = MCDI_OUT_DWORD(req, ALLOC_PIOBUF_OUT_PIOBUF_HANDLE);

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
efx_mcdi_free_piobuf(
	__in		efx_nic_t *enp,
	__in		efx_piobuf_handle_t handle)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_FREE_PIOBUF_IN_LEN,
			    MC_CMD_FREE_PIOBUF_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_FREE_PIOBUF;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FREE_PIOBUF_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FREE_PIOBUF_OUT_LEN;

	MCDI_IN_SET_DWORD(req, FREE_PIOBUF_IN_PIOBUF_HANDLE, handle);

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_link_piobuf(
	__in		efx_nic_t *enp,
	__in		uint32_t vi_index,
	__in		efx_piobuf_handle_t handle)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_LINK_PIOBUF_IN_LEN,
			    MC_CMD_LINK_PIOBUF_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_LINK_PIOBUF;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_LINK_PIOBUF_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_LINK_PIOBUF_OUT_LEN;

	MCDI_IN_SET_DWORD(req, LINK_PIOBUF_IN_PIOBUF_HANDLE, handle);
	MCDI_IN_SET_DWORD(req, LINK_PIOBUF_IN_TXQ_INSTANCE, vi_index);

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

static	__checkReturn	efx_rc_t
efx_mcdi_unlink_piobuf(
	__in		efx_nic_t *enp,
	__in		uint32_t vi_index)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_UNLINK_PIOBUF_IN_LEN,
			    MC_CMD_UNLINK_PIOBUF_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_UNLINK_PIOBUF;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_UNLINK_PIOBUF_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_UNLINK_PIOBUF_OUT_LEN;

	MCDI_IN_SET_DWORD(req, UNLINK_PIOBUF_IN_TXQ_INSTANCE, vi_index);

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

static			void
ef10_nic_alloc_piobufs(
	__in		efx_nic_t *enp,
	__in		uint32_t max_piobuf_count)
{
	efx_piobuf_handle_t *handlep;
	unsigned int i;
	efx_rc_t rc;

	EFSYS_ASSERT3U(max_piobuf_count, <=,
	    EFX_ARRAY_SIZE(enp->en_arch.ef10.ena_piobuf_handle));

	enp->en_arch.ef10.ena_piobuf_count = 0;

	for (i = 0; i < max_piobuf_count; i++) {
		handlep = &enp->en_arch.ef10.ena_piobuf_handle[i];

		if ((rc = efx_mcdi_alloc_piobuf(enp, handlep)) != 0)
			goto fail1;

		enp->en_arch.ef10.ena_pio_alloc_map[i] = 0;
		enp->en_arch.ef10.ena_piobuf_count++;
	}

	return;

fail1:
	for (i = 0; i < enp->en_arch.ef10.ena_piobuf_count; i++) {
		handlep = &enp->en_arch.ef10.ena_piobuf_handle[i];

		efx_mcdi_free_piobuf(enp, *handlep);
		*handlep = EFX_PIOBUF_HANDLE_INVALID;
	}
	enp->en_arch.ef10.ena_piobuf_count = 0;
}


static			void
ef10_nic_free_piobufs(
	__in		efx_nic_t *enp)
{
	efx_piobuf_handle_t *handlep;
	unsigned int i;

	for (i = 0; i < enp->en_arch.ef10.ena_piobuf_count; i++) {
		handlep = &enp->en_arch.ef10.ena_piobuf_handle[i];

		efx_mcdi_free_piobuf(enp, *handlep);
		*handlep = EFX_PIOBUF_HANDLE_INVALID;
	}
	enp->en_arch.ef10.ena_piobuf_count = 0;
}

/* Sub-allocate a block from a piobuf */
	__checkReturn	efx_rc_t
ef10_nic_pio_alloc(
	__inout		efx_nic_t *enp,
	__out		uint32_t *bufnump,
	__out		efx_piobuf_handle_t *handlep,
	__out		uint32_t *blknump,
	__out		uint32_t *offsetp,
	__out		size_t *sizep)
{
	efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	efx_drv_cfg_t *edcp = &enp->en_drv_cfg;
	uint32_t blk_per_buf;
	uint32_t buf, blk;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);
	EFSYS_ASSERT(bufnump);
	EFSYS_ASSERT(handlep);
	EFSYS_ASSERT(blknump);
	EFSYS_ASSERT(offsetp);
	EFSYS_ASSERT(sizep);

	if ((edcp->edc_pio_alloc_size == 0) ||
	    (enp->en_arch.ef10.ena_piobuf_count == 0)) {
		rc = ENOMEM;
		goto fail1;
	}
	blk_per_buf = encp->enc_piobuf_size / edcp->edc_pio_alloc_size;

	for (buf = 0; buf < enp->en_arch.ef10.ena_piobuf_count; buf++) {
		uint32_t *map = &enp->en_arch.ef10.ena_pio_alloc_map[buf];

		if (~(*map) == 0)
			continue;

		EFSYS_ASSERT3U(blk_per_buf, <=, (8 * sizeof (*map)));
		for (blk = 0; blk < blk_per_buf; blk++) {
			if ((*map & (1u << blk)) == 0) {
				*map |= (1u << blk);
				goto done;
			}
		}
	}
	rc = ENOMEM;
	goto fail2;

done:
	*handlep = enp->en_arch.ef10.ena_piobuf_handle[buf];
	*bufnump = buf;
	*blknump = blk;
	*sizep = edcp->edc_pio_alloc_size;
	*offsetp = blk * (*sizep);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

/* Free a piobuf sub-allocated block */
	__checkReturn	efx_rc_t
ef10_nic_pio_free(
	__inout		efx_nic_t *enp,
	__in		uint32_t bufnum,
	__in		uint32_t blknum)
{
	uint32_t *map;
	efx_rc_t rc;

	if ((bufnum >= enp->en_arch.ef10.ena_piobuf_count) ||
	    (blknum >= (8 * sizeof (*map)))) {
		rc = EINVAL;
		goto fail1;
	}

	map = &enp->en_arch.ef10.ena_pio_alloc_map[bufnum];
	if ((*map & (1u << blknum)) == 0) {
		rc = ENOENT;
		goto fail2;
	}
	*map &= ~(1u << blknum);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_nic_pio_link(
	__inout		efx_nic_t *enp,
	__in		uint32_t vi_index,
	__in		efx_piobuf_handle_t handle)
{
	return (efx_mcdi_link_piobuf(enp, vi_index, handle));
}

	__checkReturn	efx_rc_t
ef10_nic_pio_unlink(
	__inout		efx_nic_t *enp,
	__in		uint32_t vi_index)
{
	return (efx_mcdi_unlink_piobuf(enp, vi_index));
}

	__checkReturn	efx_rc_t
ef10_get_datapath_caps(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t flags;
	uint32_t flags2;
	efx_rc_t rc;

	if ((rc = efx_mcdi_get_capabilities(enp, &flags, &flags2)) != 0)
		goto fail1;

#define	CAP_FLAG(flags1, field)		\
	((flags1) & (1 << (MC_CMD_GET_CAPABILITIES_V2_OUT_ ## field ## _LBN)))

#define	CAP_FLAG2(flags2, field)	\
	((flags2) & (1 << (MC_CMD_GET_CAPABILITIES_V2_OUT_ ## field ## _LBN)))

	/*
	 * Huntington RXDP firmware inserts a 0 or 14 byte prefix.
	 * We only support the 14 byte prefix here.
	 */
	if (CAP_FLAG(flags, RX_PREFIX_LEN_14) == 0) {
		rc = ENOTSUP;
		goto fail2;
	}
	encp->enc_rx_prefix_size = 14;

	/* Check if the firmware supports TSO */
	encp->enc_fw_assisted_tso_enabled =
	    CAP_FLAG(flags, TX_TSO) ? B_TRUE : B_FALSE;

	/* Check if the firmware supports FATSOv2 */
	encp->enc_fw_assisted_tso_v2_enabled =
	    CAP_FLAG2(flags2, TX_TSO_V2) ? B_TRUE : B_FALSE;

	/* Check if the firmware has vadapter/vport/vswitch support */
	encp->enc_datapath_cap_evb =
	    CAP_FLAG(flags, EVB) ? B_TRUE : B_FALSE;

	/* Check if the firmware supports VLAN insertion */
	encp->enc_hw_tx_insert_vlan_enabled =
	    CAP_FLAG(flags, TX_VLAN_INSERTION) ? B_TRUE : B_FALSE;

	/* Check if the firmware supports RX event batching */
	encp->enc_rx_batching_enabled =
	    CAP_FLAG(flags, RX_BATCHING) ? B_TRUE : B_FALSE;

	if (encp->enc_rx_batching_enabled)
		encp->enc_rx_batch_max = 16;

	/* Check if the firmware supports disabling scatter on RXQs */
	encp->enc_rx_disable_scatter_supported =
	    CAP_FLAG(flags, RX_DISABLE_SCATTER) ? B_TRUE : B_FALSE;

	/* Check if the firmware supports set mac with running filters */
	encp->enc_allow_set_mac_with_installed_filters =
	    CAP_FLAG(flags, VADAPTOR_PERMIT_SET_MAC_WHEN_FILTERS_INSTALLED) ?
	    B_TRUE : B_FALSE;

	/*
	 * Check if firmware supports the extended MC_CMD_SET_MAC, which allows
	 * specifying which parameters to configure.
	 */
	encp->enc_enhanced_set_mac_supported =
		CAP_FLAG(flags, SET_MAC_ENHANCED) ? B_TRUE : B_FALSE;

#undef CAP_FLAG
#undef CAP_FLAG2

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


#define	EF10_LEGACY_PF_PRIVILEGE_MASK					\
	(MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_LINK			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_ONLOAD			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_PTP			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_INSECURE_FILTERS		|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_MAC_SPOOFING		|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_UNICAST			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_MULTICAST			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_BROADCAST			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_ALL_MULTICAST		|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_PROMISCUOUS)

#define	EF10_LEGACY_VF_PRIVILEGE_MASK	0


	__checkReturn		efx_rc_t
ef10_get_privilege_mask(
	__in			efx_nic_t *enp,
	__out			uint32_t *maskp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t mask;
	efx_rc_t rc;

	if ((rc = efx_mcdi_privilege_mask(enp, encp->enc_pf, encp->enc_vf,
					    &mask)) != 0) {
		if (rc != ENOTSUP)
			goto fail1;

		/* Fallback for old firmware without privilege mask support */
		if (EFX_PCI_FUNCTION_IS_PF(encp)) {
			/* Assume PF has admin privilege */
			mask = EF10_LEGACY_PF_PRIVILEGE_MASK;
		} else {
			/* VF is always unprivileged by default */
			mask = EF10_LEGACY_VF_PRIVILEGE_MASK;
		}
	}

	*maskp = mask;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


/*
 * The external port mapping is a one-based numbering of the external
 * connectors on the board. It does not distinguish off-board separated
 * outputs such as multi-headed cables.
 * The number of ports that map to each external port connector
 * on the board is determined by the chip family and the port modes to
 * which the NIC can be configured. The mapping table lists modes with
 * port numbering requirements in increasing order.
 */
static struct {
	efx_family_t	family;
	uint32_t	modes_mask;
	uint32_t	stride;
}	__ef10_external_port_mappings[] = {
	/* Supported modes requiring 1 output per port */
	{
		EFX_FAMILY_HUNTINGTON,
		(1 << TLV_PORT_MODE_10G) |
		(1 << TLV_PORT_MODE_10G_10G) |
		(1 << TLV_PORT_MODE_10G_10G_10G_10G),
		1
	},
	{
		EFX_FAMILY_MEDFORD,
		(1 << TLV_PORT_MODE_10G) |
		(1 << TLV_PORT_MODE_10G_10G) |
		(1 << TLV_PORT_MODE_10G_10G_10G_10G),
		1
	},
	/* Supported modes requiring 2 outputs per port */
	{
		EFX_FAMILY_HUNTINGTON,
		(1 << TLV_PORT_MODE_40G) |
		(1 << TLV_PORT_MODE_40G_40G) |
		(1 << TLV_PORT_MODE_40G_10G_10G) |
		(1 << TLV_PORT_MODE_10G_10G_40G),
		2
	},
	{
		EFX_FAMILY_MEDFORD,
		(1 << TLV_PORT_MODE_40G) |
		(1 << TLV_PORT_MODE_40G_40G) |
		(1 << TLV_PORT_MODE_40G_10G_10G) |
		(1 << TLV_PORT_MODE_10G_10G_40G),
		2
	},
	/* Supported modes requiring 4 outputs per port */
	{
		EFX_FAMILY_MEDFORD,
		(1 << TLV_PORT_MODE_10G_10G_10G_10G_Q) |
		(1 << TLV_PORT_MODE_10G_10G_10G_10G_Q2),
		4
	},
};

	__checkReturn	efx_rc_t
ef10_external_port_mapping(
	__in		efx_nic_t *enp,
	__in		uint32_t port,
	__out		uint8_t *external_portp)
{
	efx_rc_t rc;
	int i;
	uint32_t port_modes;
	uint32_t matches;
	uint32_t stride = 1; /* default 1-1 mapping */

	if ((rc = efx_mcdi_get_port_modes(enp, &port_modes)) != 0) {
		/* No port mode information available - use default mapping */
		goto out;
	}

	/*
	 * Infer the internal port -> external port mapping from
	 * the possible port modes for this NIC.
	 */
	for (i = 0; i < EFX_ARRAY_SIZE(__ef10_external_port_mappings); ++i) {
		if (__ef10_external_port_mappings[i].family !=
		    enp->en_family)
			continue;
		matches = (__ef10_external_port_mappings[i].modes_mask &
		    port_modes);
		if (matches != 0) {
			stride = __ef10_external_port_mappings[i].stride;
			port_modes &= ~matches;
		}
	}

	if (port_modes != 0) {
		/* Some advertised modes are not supported */
		rc = ENOTSUP;
		goto fail1;
	}

out:
	/*
	 * Scale as required by last matched mode and then convert to
	 * one-based numbering
	 */
	*external_portp = (uint8_t)(port / stride) + 1;
	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
ef10_nic_probe(
	__in		efx_nic_t *enp)
{
	const efx_nic_ops_t *enop = enp->en_enop;
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_drv_cfg_t *edcp = &(enp->en_drv_cfg);
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	/* Read and clear any assertion state */
	if ((rc = efx_mcdi_read_assertion(enp)) != 0)
		goto fail1;

	/* Exit the assertion handler */
	if ((rc = efx_mcdi_exit_assertion_handler(enp)) != 0)
		if (rc != EACCES)
			goto fail2;

	if ((rc = efx_mcdi_drv_attach(enp, B_TRUE)) != 0)
		goto fail3;

	if ((rc = enop->eno_board_cfg(enp)) != 0)
		if (rc != EACCES)
			goto fail4;

	/*
	 * Set default driver config limits (based on board config).
	 *
	 * FIXME: For now allocate a fixed number of VIs which is likely to be
	 * sufficient and small enough to allow multiple functions on the same
	 * port.
	 */
	edcp->edc_min_vi_count = edcp->edc_max_vi_count =
	    MIN(128, MAX(encp->enc_rxq_limit, encp->enc_txq_limit));

	/* The client driver must configure and enable PIO buffer support */
	edcp->edc_max_piobuf_count = 0;
	edcp->edc_pio_alloc_size = 0;

#if EFSYS_OPT_MAC_STATS
	/* Wipe the MAC statistics */
	if ((rc = efx_mcdi_mac_stats_clear(enp)) != 0)
		goto fail5;
#endif

#if EFSYS_OPT_LOOPBACK
	if ((rc = efx_mcdi_get_loopback_modes(enp)) != 0)
		goto fail6;
#endif

#if EFSYS_OPT_MON_STATS
	if ((rc = mcdi_mon_cfg_build(enp)) != 0) {
		/* Unprivileged functions do not have access to sensors */
		if (rc != EACCES)
			goto fail7;
	}
#endif

	encp->enc_features = enp->en_features;

	return (0);

#if EFSYS_OPT_MON_STATS
fail7:
	EFSYS_PROBE(fail7);
#endif
#if EFSYS_OPT_LOOPBACK
fail6:
	EFSYS_PROBE(fail6);
#endif
#if EFSYS_OPT_MAC_STATS
fail5:
	EFSYS_PROBE(fail5);
#endif
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_nic_set_drv_limits(
	__inout		efx_nic_t *enp,
	__in		efx_drv_limits_t *edlp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_drv_cfg_t *edcp = &(enp->en_drv_cfg);
	uint32_t min_evq_count, max_evq_count;
	uint32_t min_rxq_count, max_rxq_count;
	uint32_t min_txq_count, max_txq_count;
	efx_rc_t rc;

	if (edlp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	/* Get minimum required and maximum usable VI limits */
	min_evq_count = MIN(edlp->edl_min_evq_count, encp->enc_evq_limit);
	min_rxq_count = MIN(edlp->edl_min_rxq_count, encp->enc_rxq_limit);
	min_txq_count = MIN(edlp->edl_min_txq_count, encp->enc_txq_limit);

	edcp->edc_min_vi_count =
	    MAX(min_evq_count, MAX(min_rxq_count, min_txq_count));

	max_evq_count = MIN(edlp->edl_max_evq_count, encp->enc_evq_limit);
	max_rxq_count = MIN(edlp->edl_max_rxq_count, encp->enc_rxq_limit);
	max_txq_count = MIN(edlp->edl_max_txq_count, encp->enc_txq_limit);

	edcp->edc_max_vi_count =
	    MAX(max_evq_count, MAX(max_rxq_count, max_txq_count));

	/*
	 * Check limits for sub-allocated piobuf blocks.
	 * PIO is optional, so don't fail if the limits are incorrect.
	 */
	if ((encp->enc_piobuf_size == 0) ||
	    (encp->enc_piobuf_limit == 0) ||
	    (edlp->edl_min_pio_alloc_size == 0) ||
	    (edlp->edl_min_pio_alloc_size > encp->enc_piobuf_size)) {
		/* Disable PIO */
		edcp->edc_max_piobuf_count = 0;
		edcp->edc_pio_alloc_size = 0;
	} else {
		uint32_t blk_size, blk_count, blks_per_piobuf;

		blk_size =
		    MAX(edlp->edl_min_pio_alloc_size,
			    encp->enc_piobuf_min_alloc_size);

		blks_per_piobuf = encp->enc_piobuf_size / blk_size;
		EFSYS_ASSERT3U(blks_per_piobuf, <=, 32);

		blk_count = (encp->enc_piobuf_limit * blks_per_piobuf);

		/* A zero max pio alloc count means unlimited */
		if ((edlp->edl_max_pio_alloc_count > 0) &&
		    (edlp->edl_max_pio_alloc_count < blk_count)) {
			blk_count = edlp->edl_max_pio_alloc_count;
		}

		edcp->edc_pio_alloc_size = blk_size;
		edcp->edc_max_piobuf_count =
		    (blk_count + (blks_per_piobuf - 1)) / blks_per_piobuf;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
ef10_nic_reset(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_ENTITY_RESET_IN_LEN,
			    MC_CMD_ENTITY_RESET_OUT_LEN)];
	efx_rc_t rc;

	/* ef10_nic_reset() is called to recover from BADASSERT failures. */
	if ((rc = efx_mcdi_read_assertion(enp)) != 0)
		goto fail1;
	if ((rc = efx_mcdi_exit_assertion_handler(enp)) != 0)
		goto fail2;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_ENTITY_RESET;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_ENTITY_RESET_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_ENTITY_RESET_OUT_LEN;

	MCDI_IN_POPULATE_DWORD_1(req, ENTITY_RESET_IN_FLAG,
	    ENTITY_RESET_IN_FUNCTION_RESOURCE_RESET, 1);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	/* Clear RX/TX DMA queue errors */
	enp->en_reset_flags &= ~(EFX_RESET_RXQ_ERR | EFX_RESET_TXQ_ERR);

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
ef10_nic_init(
	__in		efx_nic_t *enp)
{
	efx_drv_cfg_t *edcp = &(enp->en_drv_cfg);
	uint32_t min_vi_count, max_vi_count;
	uint32_t vi_count, vi_base, vi_shift;
	uint32_t i;
	uint32_t retry;
	uint32_t delay_us;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	/* Enable reporting of some events (e.g. link change) */
	if ((rc = efx_mcdi_log_ctrl(enp)) != 0)
		goto fail1;

	/* Allocate (optional) on-chip PIO buffers */
	ef10_nic_alloc_piobufs(enp, edcp->edc_max_piobuf_count);

	/*
	 * For best performance, PIO writes should use a write-combined
	 * (WC) memory mapping. Using a separate WC mapping for the PIO
	 * aperture of each VI would be a burden to drivers (and not
	 * possible if the host page size is >4Kbyte).
	 *
	 * To avoid this we use a single uncached (UC) mapping for VI
	 * register access, and a single WC mapping for extra VIs used
	 * for PIO writes.
	 *
	 * Each piobuf must be linked to a VI in the WC mapping, and to
	 * each VI that is using a sub-allocated block from the piobuf.
	 */
	min_vi_count = edcp->edc_min_vi_count;
	max_vi_count =
	    edcp->edc_max_vi_count + enp->en_arch.ef10.ena_piobuf_count;

	/* Ensure that the previously attached driver's VIs are freed */
	if ((rc = efx_mcdi_free_vis(enp)) != 0)
		goto fail2;

	/*
	 * Reserve VI resources (EVQ+RXQ+TXQ) for this PCIe function. If this
	 * fails then retrying the request for fewer VI resources may succeed.
	 */
	vi_count = 0;
	if ((rc = efx_mcdi_alloc_vis(enp, min_vi_count, max_vi_count,
		    &vi_base, &vi_count, &vi_shift)) != 0)
		goto fail3;

	EFSYS_PROBE2(vi_alloc, uint32_t, vi_base, uint32_t, vi_count);

	if (vi_count < min_vi_count) {
		rc = ENOMEM;
		goto fail4;
	}

	enp->en_arch.ef10.ena_vi_base = vi_base;
	enp->en_arch.ef10.ena_vi_count = vi_count;
	enp->en_arch.ef10.ena_vi_shift = vi_shift;

	if (vi_count < min_vi_count + enp->en_arch.ef10.ena_piobuf_count) {
		/* Not enough extra VIs to map piobufs */
		ef10_nic_free_piobufs(enp);
	}

	enp->en_arch.ef10.ena_pio_write_vi_base =
	    vi_count - enp->en_arch.ef10.ena_piobuf_count;

	/* Save UC memory mapping details */
	enp->en_arch.ef10.ena_uc_mem_map_offset = 0;
	if (enp->en_arch.ef10.ena_piobuf_count > 0) {
		enp->en_arch.ef10.ena_uc_mem_map_size =
		    (ER_DZ_TX_PIOBUF_STEP *
		    enp->en_arch.ef10.ena_pio_write_vi_base);
	} else {
		enp->en_arch.ef10.ena_uc_mem_map_size =
		    (ER_DZ_TX_PIOBUF_STEP *
		    enp->en_arch.ef10.ena_vi_count);
	}

	/* Save WC memory mapping details */
	enp->en_arch.ef10.ena_wc_mem_map_offset =
	    enp->en_arch.ef10.ena_uc_mem_map_offset +
	    enp->en_arch.ef10.ena_uc_mem_map_size;

	enp->en_arch.ef10.ena_wc_mem_map_size =
	    (ER_DZ_TX_PIOBUF_STEP *
	    enp->en_arch.ef10.ena_piobuf_count);

	/* Link piobufs to extra VIs in WC mapping */
	if (enp->en_arch.ef10.ena_piobuf_count > 0) {
		for (i = 0; i < enp->en_arch.ef10.ena_piobuf_count; i++) {
			rc = efx_mcdi_link_piobuf(enp,
			    enp->en_arch.ef10.ena_pio_write_vi_base + i,
			    enp->en_arch.ef10.ena_piobuf_handle[i]);
			if (rc != 0)
				break;
		}
	}

	/*
	 * Allocate a vAdaptor attached to our upstream vPort/pPort.
	 *
	 * On a VF, this may fail with MC_CMD_ERR_NO_EVB_PORT (ENOENT) if the PF
	 * driver has yet to bring up the EVB port. See bug 56147. In this case,
	 * retry the request several times after waiting a while. The wait time
	 * between retries starts small (10ms) and exponentially increases.
	 * Total wait time is a little over two seconds. Retry logic in the
	 * client driver may mean this whole loop is repeated if it continues to
	 * fail.
	 */
	retry = 0;
	delay_us = 10000;
	while ((rc = efx_mcdi_vadaptor_alloc(enp, EVB_PORT_ID_ASSIGNED)) != 0) {
		if (EFX_PCI_FUNCTION_IS_PF(&enp->en_nic_cfg) ||
		    (rc != ENOENT)) {
			/*
			 * Do not retry alloc for PF, or for other errors on
			 * a VF.
			 */
			goto fail5;
		}

		/* VF startup before PF is ready. Retry allocation. */
		if (retry > 5) {
			/* Too many attempts */
			rc = EINVAL;
			goto fail6;
		}
		EFSYS_PROBE1(mcdi_no_evb_port_retry, int, retry);
		EFSYS_SLEEP(delay_us);
		retry++;
		if (delay_us < 500000)
			delay_us <<= 2;
	}

	enp->en_vport_id = EVB_PORT_ID_ASSIGNED;
	enp->en_nic_cfg.enc_mcdi_max_payload_length = MCDI_CTL_SDU_LEN_MAX_V2;

	return (0);

fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);

	ef10_nic_free_piobufs(enp);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *vi_countp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	/*
	 * Report VIs that the client driver can use.
	 * Do not include VIs used for PIO buffer writes.
	 */
	*vi_countp = enp->en_arch.ef10.ena_pio_write_vi_base;

	return (0);
}

	__checkReturn	efx_rc_t
ef10_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep)
{
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	/*
	 * TODO: Specify host memory mapping alignment and granularity
	 * in efx_drv_limits_t so that they can be taken into account
	 * when allocating extra VIs for PIO writes.
	 */
	switch (region) {
	case EFX_REGION_VI:
		/* UC mapped memory BAR region for VI registers */
		*offsetp = enp->en_arch.ef10.ena_uc_mem_map_offset;
		*sizep = enp->en_arch.ef10.ena_uc_mem_map_size;
		break;

	case EFX_REGION_PIO_WRITE_VI:
		/* WC mapped memory BAR region for piobuf writes */
		*offsetp = enp->en_arch.ef10.ena_wc_mem_map_offset;
		*sizep = enp->en_arch.ef10.ena_wc_mem_map_size;
		break;

	default:
		rc = EINVAL;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
ef10_nic_fini(
	__in		efx_nic_t *enp)
{
	uint32_t i;
	efx_rc_t rc;

	(void) efx_mcdi_vadaptor_free(enp, enp->en_vport_id);
	enp->en_vport_id = 0;

	/* Unlink piobufs from extra VIs in WC mapping */
	if (enp->en_arch.ef10.ena_piobuf_count > 0) {
		for (i = 0; i < enp->en_arch.ef10.ena_piobuf_count; i++) {
			rc = efx_mcdi_unlink_piobuf(enp,
			    enp->en_arch.ef10.ena_pio_write_vi_base + i);
			if (rc != 0)
				break;
		}
	}

	ef10_nic_free_piobufs(enp);

	(void) efx_mcdi_free_vis(enp);
	enp->en_arch.ef10.ena_vi_count = 0;
}

			void
ef10_nic_unprobe(
	__in		efx_nic_t *enp)
{
#if EFSYS_OPT_MON_STATS
	mcdi_mon_cfg_free(enp);
#endif /* EFSYS_OPT_MON_STATS */
	(void) efx_mcdi_drv_attach(enp, B_FALSE);
}

#if EFSYS_OPT_DIAG

	__checkReturn	efx_rc_t
ef10_nic_register_test(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	/* FIXME */
	_NOTE(ARGUNUSED(enp))
	if (B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}
	/* FIXME */

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */


#endif	/* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD */
