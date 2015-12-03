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

#include "efsys.h"
#include "efx.h"
#include "efx_impl.h"
#include "mcdi_mon.h"

#if EFSYS_OPT_HUNTINGTON

#include "ef10_tlv_layout.h"

static	__checkReturn	efx_rc_t
efx_mcdi_get_port_assignment(
	__in		efx_nic_t *enp,
	__out		uint32_t *portp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_PORT_ASSIGNMENT_IN_LEN,
			    MC_CMD_GET_PORT_ASSIGNMENT_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

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

static	__checkReturn	efx_rc_t
efx_mcdi_get_port_modes(
	__in		efx_nic_t *enp,
	__out		uint32_t *modesp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_PORT_MODES_IN_LEN,
			    MC_CMD_GET_PORT_MODES_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

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

	/* Accept pre-Medford size (8 bytes - no CurrentMode field) */
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

static	__checkReturn	efx_rc_t
efx_mcdi_get_mac_address_pf(
	__in			efx_nic_t *enp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6])
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_MAC_ADDRESSES_IN_LEN,
			    MC_CMD_GET_MAC_ADDRESSES_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

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

static	__checkReturn	efx_rc_t
efx_mcdi_get_mac_address_vf(
	__in			efx_nic_t *enp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6])
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_VPORT_GET_MAC_ADDRESSES_IN_LEN,
			    MC_CMD_VPORT_GET_MAC_ADDRESSES_OUT_LENMAX)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

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

static	__checkReturn	efx_rc_t
efx_mcdi_get_clock(
	__in		efx_nic_t *enp,
	__out		uint32_t *sys_freqp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_CLOCK_IN_LEN,
			    MC_CMD_GET_CLOCK_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

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

static 	__checkReturn	efx_rc_t
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
	__out		efx_dword_t *flagsp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_CAPABILITIES_IN_LEN,
			    MC_CMD_GET_CAPABILITIES_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_CAPABILITIES;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_CAPABILITIES_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_CAPABILITIES_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_CAPABILITIES_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*flagsp = *MCDI_OUT2(req, efx_dword_t, GET_CAPABILITIES_OUT_FLAGS1);

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
	__out_opt	uint32_t *vi_basep,
	__out		uint32_t *vi_countp)

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

	if (vi_basep != NULL)
		*vi_basep = MCDI_OUT_DWORD(req, ALLOC_VIS_OUT_VI_BASE);

	if (vi_countp != NULL)
		*vi_countp = MCDI_OUT_DWORD(req, ALLOC_VIS_OUT_VI_COUNT);

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
hunt_nic_alloc_piobufs(
	__in		efx_nic_t *enp,
	__in		uint32_t max_piobuf_count)
{
	efx_piobuf_handle_t *handlep;
	unsigned int i;
	efx_rc_t rc;

	EFSYS_ASSERT3U(max_piobuf_count, <=,
	    EFX_ARRAY_SIZE(enp->en_u.hunt.enu_piobuf_handle));

	enp->en_u.hunt.enu_piobuf_count = 0;

	for (i = 0; i < max_piobuf_count; i++) {
		handlep = &enp->en_u.hunt.enu_piobuf_handle[i];

		if ((rc = efx_mcdi_alloc_piobuf(enp, handlep)) != 0)
			goto fail1;

		enp->en_u.hunt.enu_pio_alloc_map[i] = 0;
		enp->en_u.hunt.enu_piobuf_count++;
	}

	return;

fail1:
	for (i = 0; i < enp->en_u.hunt.enu_piobuf_count; i++) {
		handlep = &enp->en_u.hunt.enu_piobuf_handle[i];

		efx_mcdi_free_piobuf(enp, *handlep);
		*handlep = EFX_PIOBUF_HANDLE_INVALID;
	}
	enp->en_u.hunt.enu_piobuf_count = 0;
}


static			void
hunt_nic_free_piobufs(
	__in		efx_nic_t *enp)
{
	efx_piobuf_handle_t *handlep;
	unsigned int i;

	for (i = 0; i < enp->en_u.hunt.enu_piobuf_count; i++) {
		handlep = &enp->en_u.hunt.enu_piobuf_handle[i];

		efx_mcdi_free_piobuf(enp, *handlep);
		*handlep = EFX_PIOBUF_HANDLE_INVALID;
	}
	enp->en_u.hunt.enu_piobuf_count = 0;
}

/* Sub-allocate a block from a piobuf */
	__checkReturn	efx_rc_t
hunt_nic_pio_alloc(
	__inout		efx_nic_t *enp,
	__out		uint32_t *bufnump,
	__out		efx_piobuf_handle_t *handlep,
	__out		uint32_t *blknump,
	__out		uint32_t *offsetp,
	__out		size_t *sizep)
{
	efx_drv_cfg_t *edcp = &enp->en_drv_cfg;
	uint32_t blk_per_buf;
	uint32_t buf, blk;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);
	EFSYS_ASSERT(bufnump);
	EFSYS_ASSERT(handlep);
	EFSYS_ASSERT(blknump);
	EFSYS_ASSERT(offsetp);
	EFSYS_ASSERT(sizep);

	if ((edcp->edc_pio_alloc_size == 0) ||
	    (enp->en_u.hunt.enu_piobuf_count == 0)) {
		rc = ENOMEM;
		goto fail1;
	}
	blk_per_buf = HUNT_PIOBUF_SIZE / edcp->edc_pio_alloc_size;

	for (buf = 0; buf < enp->en_u.hunt.enu_piobuf_count; buf++) {
		uint32_t *map = &enp->en_u.hunt.enu_pio_alloc_map[buf];

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
	*handlep = enp->en_u.hunt.enu_piobuf_handle[buf];
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
hunt_nic_pio_free(
	__inout		efx_nic_t *enp,
	__in		uint32_t bufnum,
	__in		uint32_t blknum)
{
	uint32_t *map;
	efx_rc_t rc;

	if ((bufnum >= enp->en_u.hunt.enu_piobuf_count) ||
	    (blknum >= (8 * sizeof (*map)))) {
		rc = EINVAL;
		goto fail1;
	}

	map = &enp->en_u.hunt.enu_pio_alloc_map[bufnum];
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
hunt_nic_pio_link(
	__inout		efx_nic_t *enp,
	__in		uint32_t vi_index,
	__in		efx_piobuf_handle_t handle)
{
	return (efx_mcdi_link_piobuf(enp, vi_index, handle));
}

	__checkReturn	efx_rc_t
hunt_nic_pio_unlink(
	__inout		efx_nic_t *enp,
	__in		uint32_t vi_index)
{
	return (efx_mcdi_unlink_piobuf(enp, vi_index));
}

static	__checkReturn	efx_rc_t
hunt_get_datapath_caps(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_dword_t datapath_capabilities;
	efx_rc_t rc;

	if ((rc = efx_mcdi_get_capabilities(enp, &datapath_capabilities)) != 0)
		goto fail1;

	/*
	 * Huntington RXDP firmware inserts a 0 or 14 byte prefix.
	 * We only support the 14 byte prefix here.
	 */
	if (MCDI_CMD_DWORD_FIELD(&datapath_capabilities,
		GET_CAPABILITIES_OUT_RX_PREFIX_LEN_14) != 1) {
		rc = ENOTSUP;
		goto fail2;
	}
	encp->enc_rx_prefix_size = 14;

	/* Check if the firmware supports TSO */
	if (MCDI_CMD_DWORD_FIELD(&datapath_capabilities,
				GET_CAPABILITIES_OUT_TX_TSO) == 1)
		encp->enc_fw_assisted_tso_enabled = B_TRUE;
	else
		encp->enc_fw_assisted_tso_enabled = B_FALSE;

	/* Check if the firmware has vadapter/vport/vswitch support */
	if (MCDI_CMD_DWORD_FIELD(&datapath_capabilities,
				GET_CAPABILITIES_OUT_EVB) == 1)
		encp->enc_datapath_cap_evb = B_TRUE;
	else
		encp->enc_datapath_cap_evb = B_FALSE;

	/* Check if the firmware supports VLAN insertion */
	if (MCDI_CMD_DWORD_FIELD(&datapath_capabilities,
				GET_CAPABILITIES_OUT_TX_VLAN_INSERTION) == 1)
		encp->enc_hw_tx_insert_vlan_enabled = B_TRUE;
	else
		encp->enc_hw_tx_insert_vlan_enabled = B_FALSE;

	/* Check if the firmware supports RX event batching */
	if (MCDI_CMD_DWORD_FIELD(&datapath_capabilities,
		GET_CAPABILITIES_OUT_RX_BATCHING) == 1) {
		encp->enc_rx_batching_enabled = B_TRUE;
		encp->enc_rx_batch_max = 16;
	} else {
		encp->enc_rx_batching_enabled = B_FALSE;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
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
}	__hunt_external_port_mappings[] = {
	/* Supported modes requiring 1 output per port */
	{
		EFX_FAMILY_HUNTINGTON,
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
	}
	/*
	 * NOTE: Medford modes will require 4 outputs per port:
	 *	TLV_PORT_MODE_10G_10G_10G_10G_Q
	 *	TLV_PORT_MODE_10G_10G_10G_10G_Q2
	 * The Q2 mode routes outputs to external port 2. Support for this
	 * will require a new field specifying the number to add after
	 * scaling by stride. This is fixed at 1 currently.
	 */
};

static	__checkReturn	efx_rc_t
hunt_external_port_mapping(
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
	for (i = 0; i < EFX_ARRAY_SIZE(__hunt_external_port_mappings); ++i) {
		if (__hunt_external_port_mappings[i].family !=
		    enp->en_family)
			continue;
		matches = (__hunt_external_port_mappings[i].modes_mask &
		    port_modes);
		if (matches != 0) {
			stride = __hunt_external_port_mappings[i].stride;
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

static	__checkReturn	efx_rc_t
hunt_board_cfg(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint8_t mac_addr[6];
	uint32_t board_type = 0;
	hunt_link_state_t hls;
	efx_port_t *epp = &(enp->en_port);
	uint32_t port;
	uint32_t pf;
	uint32_t vf;
	uint32_t mask;
	uint32_t flags;
	uint32_t sysclk;
	uint32_t base, nvec;
	efx_rc_t rc;

	if ((rc = efx_mcdi_get_port_assignment(enp, &port)) != 0)
		goto fail1;

	/*
	 * NOTE: The MCDI protocol numbers ports from zero.
	 * The common code MCDI interface numbers ports from one.
	 */
	emip->emi_port = port + 1;

	if ((rc = hunt_external_port_mapping(enp, port,
		    &encp->enc_external_port)) != 0)
		goto fail2;

	/*
	 * Get PCIe function number from firmware (used for
	 * per-function privilege and dynamic config info).
	 *  - PCIe PF: pf = PF number, vf = 0xffff.
	 *  - PCIe VF: pf = parent PF, vf = VF number.
	 */
	if ((rc = efx_mcdi_get_function_info(enp, &pf, &vf)) != 0)
		goto fail3;

	encp->enc_pf = pf;
	encp->enc_vf = vf;

	/* MAC address for this function */
	if (EFX_PCI_FUNCTION_IS_PF(encp)) {
		rc = efx_mcdi_get_mac_address_pf(enp, mac_addr);
		if ((rc == 0) && (mac_addr[0] & 0x02)) {
			/*
			 * If the static config does not include a global MAC
			 * address pool then the board may return a locally
			 * administered MAC address (this should only happen on
			 * incorrectly programmed boards).
			 */
			rc = EINVAL;
		}
	} else {
		rc = efx_mcdi_get_mac_address_vf(enp, mac_addr);
	}
	if (rc != 0)
		goto fail4;

	EFX_MAC_ADDR_COPY(encp->enc_mac_addr, mac_addr);

	/* Board configuration */
	rc = efx_mcdi_get_board_cfg(enp, &board_type, NULL, NULL);
	if (rc != 0) {
		/* Unprivileged functions may not be able to read board cfg */
		if (rc == EACCES)
			board_type = 0;
		else
			goto fail5;
	}

	encp->enc_board_type = board_type;
	encp->enc_clk_mult = 1; /* not used for Huntington */

	/* Fill out fields in enp->en_port and enp->en_nic_cfg from MCDI */
	if ((rc = efx_mcdi_get_phy_cfg(enp)) != 0)
		goto fail6;

	/* Obtain the default PHY advertised capabilities */
	if ((rc = hunt_phy_get_link(enp, &hls)) != 0)
		goto fail7;
	epp->ep_default_adv_cap_mask = hls.hls_adv_cap_mask;
	epp->ep_adv_cap_mask = hls.hls_adv_cap_mask;

	/*
	 * Enable firmware workarounds for hardware errata.
	 * Expected responses are:
	 *  - 0 (zero):
	 *	Success: workaround enabled or disabled as requested.
	 *  - MC_CMD_ERR_ENOSYS (reported as ENOTSUP):
	 *	Firmware does not support the MC_CMD_WORKAROUND request.
	 *	(assume that the workaround is not supported).
	 *  - MC_CMD_ERR_ENOENT (reported as ENOENT):
	 *	Firmware does not support the requested workaround.
	 *  - MC_CMD_ERR_EPERM  (reported as EACCES):
	 *	Unprivileged function cannot enable/disable workarounds.
	 *
	 * See efx_mcdi_request_errcode() for MCDI error translations.
	 */

	/*
	 * If the bug35388 workaround is enabled, then use an indirect access
	 * method to avoid unsafe EVQ writes.
	 */
	rc = efx_mcdi_set_workaround(enp, MC_CMD_WORKAROUND_BUG35388, B_TRUE,
	    NULL);
	if ((rc == 0) || (rc == EACCES))
		encp->enc_bug35388_workaround = B_TRUE;
	else if ((rc == ENOTSUP) || (rc == ENOENT))
		encp->enc_bug35388_workaround = B_FALSE;
	else
		goto fail8;

	/*
	 * If the bug41750 workaround is enabled, then do not test interrupts,
	 * as the test will fail (seen with Greenport controllers).
	 */
	rc = efx_mcdi_set_workaround(enp, MC_CMD_WORKAROUND_BUG41750, B_TRUE,
	    NULL);
	if (rc == 0) {
		encp->enc_bug41750_workaround = B_TRUE;
	} else if (rc == EACCES) {
		/* Assume a controller with 40G ports needs the workaround. */
		if (epp->ep_default_adv_cap_mask & EFX_PHY_CAP_40000FDX)
			encp->enc_bug41750_workaround = B_TRUE;
		else
			encp->enc_bug41750_workaround = B_FALSE;
	} else if ((rc == ENOTSUP) || (rc == ENOENT)) {
		encp->enc_bug41750_workaround = B_FALSE;
	} else {
		goto fail9;
	}
	if (EFX_PCI_FUNCTION_IS_VF(encp)) {
		/* Interrupt testing does not work for VFs. See bug50084. */
		encp->enc_bug41750_workaround = B_TRUE;
	}

	/*
	 * If the bug26807 workaround is enabled, then firmware has enabled
	 * support for chained multicast filters. Firmware will reset (FLR)
	 * functions which have filters in the hardware filter table when the
	 * workaround is enabled/disabled.
	 *
	 * We must recheck if the workaround is enabled after inserting the
	 * first hardware filter, in case it has been changed since this check.
	 */
	rc = efx_mcdi_set_workaround(enp, MC_CMD_WORKAROUND_BUG26807,
	    B_TRUE, &flags);
	if (rc == 0) {
		encp->enc_bug26807_workaround = B_TRUE;
		if (flags & (1 << MC_CMD_WORKAROUND_EXT_OUT_FLR_DONE_LBN)) {
			/*
			 * Other functions had installed filters before the
			 * workaround was enabled, and they have been reset
			 * by firmware.
			 */
			EFSYS_PROBE(bug26807_workaround_flr_done);
			/* FIXME: bump MC warm boot count ? */
		}
	} else if (rc == EACCES) {
		/*
		 * Unprivileged functions cannot enable the workaround in older
		 * firmware.
		 */
		encp->enc_bug26807_workaround = B_FALSE;
	} else if ((rc == ENOTSUP) || (rc == ENOENT)) {
		encp->enc_bug26807_workaround = B_FALSE;
	} else {
		goto fail10;
	}

	/* Get sysclk frequency (in MHz). */
	if ((rc = efx_mcdi_get_clock(enp, &sysclk)) != 0)
		goto fail11;

	/*
	 * The timer quantum is 1536 sysclk cycles, documented for the
	 * EV_TMR_VAL field of EV_TIMER_TBL. Scale for MHz and ns units.
	 */
	encp->enc_evq_timer_quantum_ns = 1536000UL / sysclk; /* 1536 cycles */
	if (encp->enc_bug35388_workaround) {
		encp->enc_evq_timer_max_us = (encp->enc_evq_timer_quantum_ns <<
		ERF_DD_EVQ_IND_TIMER_VAL_WIDTH) / 1000;
	} else {
		encp->enc_evq_timer_max_us = (encp->enc_evq_timer_quantum_ns <<
		FRF_CZ_TC_TIMER_VAL_WIDTH) / 1000;
	}

	/* Check capabilities of running datapath firmware */
	if ((rc = hunt_get_datapath_caps(enp)) != 0)
	    goto fail12;

	/* Alignment for receive packet DMA buffers */
	encp->enc_rx_buf_align_start = 1;
	encp->enc_rx_buf_align_end = 64; /* RX DMA end padding */

	/* Alignment for WPTR updates */
	encp->enc_rx_push_align = HUNTINGTON_RX_WPTR_ALIGN;

	/*
	 * Set resource limits for MC_CMD_ALLOC_VIS. Note that we cannot use
	 * MC_CMD_GET_RESOURCE_LIMITS here as that reports the available
	 * resources (allocated to this PCIe function), which is zero until
	 * after we have allocated VIs.
	 */
	encp->enc_evq_limit = 1024;
	encp->enc_rxq_limit = EFX_RXQ_LIMIT_TARGET;
	encp->enc_txq_limit = EFX_TXQ_LIMIT_TARGET;

	encp->enc_buftbl_limit = 0xFFFFFFFF;

	encp->enc_piobuf_limit = HUNT_PIOBUF_NBUFS;
	encp->enc_piobuf_size = HUNT_PIOBUF_SIZE;

	/*
	 * Get the current privilege mask. Note that this may be modified
	 * dynamically, so this value is informational only. DO NOT use
	 * the privilege mask to check for sufficient privileges, as that
	 * can result in time-of-check/time-of-use bugs.
	 */
	if ((rc = efx_mcdi_privilege_mask(enp, pf, vf, &mask)) != 0) {
		if (rc != ENOTSUP)
			goto fail13;

		/* Fallback for old firmware without privilege mask support */
		if (EFX_PCI_FUNCTION_IS_PF(encp)) {
			/* Assume PF has admin privilege */
			mask = HUNT_LEGACY_PF_PRIVILEGE_MASK;
		} else {
			/* VF is always unprivileged by default */
			mask = HUNT_LEGACY_VF_PRIVILEGE_MASK;
		}
	}

	encp->enc_privilege_mask = mask;

	/* Get interrupt vector limits */
	if ((rc = efx_mcdi_get_vector_cfg(enp, &base, &nvec, NULL)) != 0) {
		if (EFX_PCI_FUNCTION_IS_PF(encp))
			goto fail14;

		/* Ignore error (cannot query vector limits from a VF). */
		base = 0;
		nvec = 1024;
	}
	encp->enc_intr_vec_base = base;
	encp->enc_intr_limit = nvec;

	/*
	 * Maximum number of bytes into the frame the TCP header can start for
	 * firmware assisted TSO to work.
	 */
	encp->enc_tx_tso_tcp_header_offset_limit = 208;

	return (0);

fail14:
	EFSYS_PROBE(fail14);
fail13:
	EFSYS_PROBE(fail13);
fail12:
	EFSYS_PROBE(fail12);
fail11:
	EFSYS_PROBE(fail11);
fail10:
	EFSYS_PROBE(fail10);
fail9:
	EFSYS_PROBE(fail9);
fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);
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
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
hunt_nic_probe(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_drv_cfg_t *edcp = &(enp->en_drv_cfg);
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	/* Read and clear any assertion state */
	if ((rc = efx_mcdi_read_assertion(enp)) != 0)
		goto fail1;

	/* Exit the assertion handler */
	if ((rc = efx_mcdi_exit_assertion_handler(enp)) != 0)
		if (rc != EACCES)
			goto fail2;

	if ((rc = efx_mcdi_drv_attach(enp, B_TRUE)) != 0)
		goto fail3;

	if ((rc = hunt_board_cfg(enp)) != 0)
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
hunt_nic_set_drv_limits(
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
		    MAX(edlp->edl_min_pio_alloc_size, HUNT_MIN_PIO_ALLOC_SIZE);

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
hunt_nic_reset(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_ENTITY_RESET_IN_LEN,
			    MC_CMD_ENTITY_RESET_OUT_LEN)];
	efx_rc_t rc;

	/* hunt_nic_reset() is called to recover from BADASSERT failures. */
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
hunt_nic_init(
	__in		efx_nic_t *enp)
{
	efx_drv_cfg_t *edcp = &(enp->en_drv_cfg);
	uint32_t min_vi_count, max_vi_count;
	uint32_t vi_count, vi_base;
	uint32_t i;
	uint32_t retry;
	uint32_t delay_us;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	/* Enable reporting of some events (e.g. link change) */
	if ((rc = efx_mcdi_log_ctrl(enp)) != 0)
		goto fail1;

	/* Allocate (optional) on-chip PIO buffers */
	hunt_nic_alloc_piobufs(enp, edcp->edc_max_piobuf_count);

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
	max_vi_count = edcp->edc_max_vi_count + enp->en_u.hunt.enu_piobuf_count;

	/* Ensure that the previously attached driver's VIs are freed */
	if ((rc = efx_mcdi_free_vis(enp)) != 0)
		goto fail2;

	/*
	 * Reserve VI resources (EVQ+RXQ+TXQ) for this PCIe function. If this
	 * fails then retrying the request for fewer VI resources may succeed.
	 */
	vi_count = 0;
	if ((rc = efx_mcdi_alloc_vis(enp, min_vi_count, max_vi_count,
		    &vi_base, &vi_count)) != 0)
		goto fail3;

	EFSYS_PROBE2(vi_alloc, uint32_t, vi_base, uint32_t, vi_count);

	if (vi_count < min_vi_count) {
		rc = ENOMEM;
		goto fail4;
	}

	enp->en_u.hunt.enu_vi_base = vi_base;
	enp->en_u.hunt.enu_vi_count = vi_count;

	if (vi_count < min_vi_count + enp->en_u.hunt.enu_piobuf_count) {
		/* Not enough extra VIs to map piobufs */
		hunt_nic_free_piobufs(enp);
	}

	enp->en_u.hunt.enu_pio_write_vi_base =
	    vi_count - enp->en_u.hunt.enu_piobuf_count;

	/* Save UC memory mapping details */
	enp->en_u.hunt.enu_uc_mem_map_offset = 0;
	if (enp->en_u.hunt.enu_piobuf_count > 0) {
		enp->en_u.hunt.enu_uc_mem_map_size =
		    (ER_DZ_TX_PIOBUF_STEP *
		    enp->en_u.hunt.enu_pio_write_vi_base);
	} else {
		enp->en_u.hunt.enu_uc_mem_map_size =
		    (ER_DZ_TX_PIOBUF_STEP *
		    enp->en_u.hunt.enu_vi_count);
	}

	/* Save WC memory mapping details */
	enp->en_u.hunt.enu_wc_mem_map_offset =
	    enp->en_u.hunt.enu_uc_mem_map_offset +
	    enp->en_u.hunt.enu_uc_mem_map_size;

	enp->en_u.hunt.enu_wc_mem_map_size =
	    (ER_DZ_TX_PIOBUF_STEP *
	    enp->en_u.hunt.enu_piobuf_count);

	/* Link piobufs to extra VIs in WC mapping */
	if (enp->en_u.hunt.enu_piobuf_count > 0) {
		for (i = 0; i < enp->en_u.hunt.enu_piobuf_count; i++) {
			rc = efx_mcdi_link_piobuf(enp,
			    enp->en_u.hunt.enu_pio_write_vi_base + i,
			    enp->en_u.hunt.enu_piobuf_handle[i]);
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

	hunt_nic_free_piobufs(enp);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
hunt_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *vi_countp)
{
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	/*
	 * Report VIs that the client driver can use.
	 * Do not include VIs used for PIO buffer writes.
	 */
	*vi_countp = enp->en_u.hunt.enu_pio_write_vi_base;

	return (0);
}

	__checkReturn	efx_rc_t
hunt_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	/*
	 * TODO: Specify host memory mapping alignment and granularity
	 * in efx_drv_limits_t so that they can be taken into account
	 * when allocating extra VIs for PIO writes.
	 */
	switch (region) {
	case EFX_REGION_VI:
		/* UC mapped memory BAR region for VI registers */
		*offsetp = enp->en_u.hunt.enu_uc_mem_map_offset;
		*sizep = enp->en_u.hunt.enu_uc_mem_map_size;
		break;

	case EFX_REGION_PIO_WRITE_VI:
		/* WC mapped memory BAR region for piobuf writes */
		*offsetp = enp->en_u.hunt.enu_wc_mem_map_offset;
		*sizep = enp->en_u.hunt.enu_wc_mem_map_size;
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
hunt_nic_fini(
	__in		efx_nic_t *enp)
{
	uint32_t i;
	efx_rc_t rc;

	(void) efx_mcdi_vadaptor_free(enp, enp->en_vport_id);
	enp->en_vport_id = 0;

	/* Unlink piobufs from extra VIs in WC mapping */
	if (enp->en_u.hunt.enu_piobuf_count > 0) {
		for (i = 0; i < enp->en_u.hunt.enu_piobuf_count; i++) {
			rc = efx_mcdi_unlink_piobuf(enp,
			    enp->en_u.hunt.enu_pio_write_vi_base + i);
			if (rc != 0)
				break;
		}
	}

	hunt_nic_free_piobufs(enp);

	(void) efx_mcdi_free_vis(enp);
	enp->en_u.hunt.enu_vi_count = 0;
}

			void
hunt_nic_unprobe(
	__in		efx_nic_t *enp)
{
#if EFSYS_OPT_MON_STATS
	mcdi_mon_cfg_free(enp);
#endif /* EFSYS_OPT_MON_STATS */
	(void) efx_mcdi_drv_attach(enp, B_FALSE);
}

#if EFSYS_OPT_DIAG

	__checkReturn	efx_rc_t
hunt_nic_register_test(
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



#endif	/* EFSYS_OPT_HUNTINGTON */
