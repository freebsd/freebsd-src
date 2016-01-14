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

#if EFSYS_OPT_LICENSING

#if EFSYS_OPT_SIENA

static	__checkReturn	efx_rc_t
efx_mcdi_fc_license_update_license(
	__in		efx_nic_t *enp);

static	__checkReturn	efx_rc_t
efx_mcdi_fc_license_get_key_stats(
	__in		efx_nic_t *enp,
	__out		efx_key_stats_t *eksp);

static efx_lic_ops_t	__efx_lic_v1_ops = {
	efx_mcdi_fc_license_update_license,	/* elo_update_licenses */
	efx_mcdi_fc_license_get_key_stats,	/* elo_get_key_stats */
	NULL,					/* elo_app_state */
	NULL,					/* elo_get_id */
};

#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_update_licenses(
	__in		efx_nic_t *enp);

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_get_key_stats(
	__in		efx_nic_t *enp,
	__out		efx_key_stats_t *eksp);

static	__checkReturn	efx_rc_t
efx_mcdi_licensed_app_state(
	__in		efx_nic_t *enp,
	__in		uint64_t app_id,
	__out		boolean_t *licensedp);

static efx_lic_ops_t	__efx_lic_v2_ops = {
	efx_mcdi_licensing_update_licenses,	/* elo_update_licenses */
	efx_mcdi_licensing_get_key_stats,	/* elo_get_key_stats */
	efx_mcdi_licensed_app_state,		/* elo_app_state */
	NULL,					/* elo_get_id */
};

#endif	/* EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_v3_update_licenses(
	__in		efx_nic_t *enp);

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_v3_report_license(
	__in		efx_nic_t *enp,
	__out		efx_key_stats_t *eksp);

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_v3_app_state(
	__in		efx_nic_t *enp,
	__in		uint64_t app_id,
	__out		boolean_t *licensedp);

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_v3_get_id(
	__in		efx_nic_t *enp,
	__in		size_t buffer_size,
	__out		uint32_t *typep,
	__out		size_t *lengthp,
	__out_bcount_part_opt(buffer_size, *lengthp)
			uint8_t *bufferp);

static efx_lic_ops_t	__efx_lic_v3_ops = {
	efx_mcdi_licensing_v3_update_licenses,	/* elo_update_licenses */
	efx_mcdi_licensing_v3_report_license,	/* elo_get_key_stats */
	efx_mcdi_licensing_v3_app_state,	/* elo_app_state */
	efx_mcdi_licensing_v3_get_id,		/* elo_get_id */
};

#endif	/* EFSYS_OPT_MEDFORD */


/* V1 Licensing - used in Siena Modena only */

#if EFSYS_OPT_SIENA

static	__checkReturn	efx_rc_t
efx_mcdi_fc_license_update_license(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_FC_IN_LICENSE_LEN];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_FC_OP_LICENSE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FC_IN_LICENSE_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, FC_IN_LICENSE_OP,
	    MC_CMD_FC_IN_LICENSE_UPDATE_LICENSE);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used != 0) {
		rc = EIO;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_fc_license_get_key_stats(
	__in		efx_nic_t *enp,
	__out		efx_key_stats_t *eksp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_FC_IN_LICENSE_LEN,
			    MC_CMD_FC_OUT_LICENSE_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_FC_OP_LICENSE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FC_IN_LICENSE_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FC_OUT_LICENSE_LEN;

	MCDI_IN_SET_DWORD(req, FC_IN_LICENSE_OP,
	    MC_CMD_FC_IN_LICENSE_GET_KEY_STATS);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_FC_OUT_LICENSE_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	eksp->eks_valid =
		MCDI_OUT_DWORD(req, FC_OUT_LICENSE_VALID_KEYS);
	eksp->eks_invalid =
		MCDI_OUT_DWORD(req, FC_OUT_LICENSE_INVALID_KEYS);
	eksp->eks_blacklisted =
		MCDI_OUT_DWORD(req, FC_OUT_LICENSE_BLACKLISTED_KEYS);
	eksp->eks_unverifiable = 0;
	eksp->eks_wrong_node = 0;
	eksp->eks_licensed_apps_lo = 0;
	eksp->eks_licensed_apps_hi = 0;
	eksp->eks_licensed_features_lo = 0;
	eksp->eks_licensed_features_hi = 0;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_SIENA */

/* V2 Licensing - used by Huntington family only. See SF-113611-TC */

#if EFSYS_OPT_HUNTINGTON

static	__checkReturn	efx_rc_t
efx_mcdi_licensed_app_state(
	__in		efx_nic_t *enp,
	__in		uint64_t app_id,
	__out		boolean_t *licensedp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_LICENSED_APP_STATE_IN_LEN,
			    MC_CMD_GET_LICENSED_APP_STATE_OUT_LEN)];
	uint32_t app_state;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	/* V2 licensing supports 32bit app id only */
	if ((app_id >> 32) != 0) {
		rc = EINVAL;
		goto fail1;
	}

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_LICENSED_APP_STATE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_LICENSED_APP_STATE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_LICENSED_APP_STATE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, GET_LICENSED_APP_STATE_IN_APP_ID,
		    app_id & 0xffffffff);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_GET_LICENSED_APP_STATE_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	app_state = (MCDI_OUT_DWORD(req, GET_LICENSED_APP_STATE_OUT_STATE));
	if (app_state != MC_CMD_GET_LICENSED_APP_STATE_OUT_NOT_LICENSED) {
		*licensedp = B_TRUE;
	} else {
		*licensedp = B_FALSE;
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
efx_mcdi_licensing_update_licenses(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_LICENSING_IN_LEN];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_LICENSING;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_LICENSING_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, LICENSING_IN_OP,
	    MC_CMD_LICENSING_IN_OP_UPDATE_LICENSE);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used != 0) {
		rc = EIO;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_get_key_stats(
	__in		efx_nic_t *enp,
	__out		efx_key_stats_t *eksp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_LICENSING_IN_LEN,
			    MC_CMD_LICENSING_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_LICENSING;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_LICENSING_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_LICENSING_OUT_LEN;

	MCDI_IN_SET_DWORD(req, LICENSING_IN_OP,
	    MC_CMD_LICENSING_IN_OP_GET_KEY_STATS);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_LICENSING_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	eksp->eks_valid =
		MCDI_OUT_DWORD(req, LICENSING_OUT_VALID_APP_KEYS);
	eksp->eks_invalid =
		MCDI_OUT_DWORD(req, LICENSING_OUT_INVALID_APP_KEYS);
	eksp->eks_blacklisted =
		MCDI_OUT_DWORD(req, LICENSING_OUT_BLACKLISTED_APP_KEYS);
	eksp->eks_unverifiable =
		MCDI_OUT_DWORD(req, LICENSING_OUT_UNVERIFIABLE_APP_KEYS);
	eksp->eks_wrong_node =
		MCDI_OUT_DWORD(req, LICENSING_OUT_WRONG_NODE_APP_KEYS);
	eksp->eks_licensed_apps_lo = 0;
	eksp->eks_licensed_apps_hi = 0;
	eksp->eks_licensed_features_lo = 0;
	eksp->eks_licensed_features_hi = 0;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_HUNTINGTON */

/* V3 Licensing - used starting from Medford family. See SF-114884-SW */

#if EFSYS_OPT_MEDFORD

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_v3_update_licenses(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_LICENSING_V3_IN_LEN];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_MEDFORD);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_LICENSING_V3;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_LICENSING_V3_IN_LEN;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, LICENSING_V3_IN_OP,
	    MC_CMD_LICENSING_V3_IN_OP_UPDATE_LICENSE);

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
efx_mcdi_licensing_v3_report_license(
	__in		efx_nic_t *enp,
	__out		efx_key_stats_t *eksp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_LICENSING_V3_IN_LEN,
			    MC_CMD_LICENSING_V3_OUT_LEN)];
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_MEDFORD);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_LICENSING_V3;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_LICENSING_V3_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_LICENSING_V3_OUT_LEN;

	MCDI_IN_SET_DWORD(req, LICENSING_V3_IN_OP,
	    MC_CMD_LICENSING_V3_IN_OP_REPORT_LICENSE);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_LICENSING_V3_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	eksp->eks_valid =
		MCDI_OUT_DWORD(req, LICENSING_V3_OUT_VALID_KEYS);
	eksp->eks_invalid =
		MCDI_OUT_DWORD(req, LICENSING_V3_OUT_INVALID_KEYS);
	eksp->eks_blacklisted = 0;
	eksp->eks_unverifiable =
		MCDI_OUT_DWORD(req, LICENSING_V3_OUT_UNVERIFIABLE_KEYS);
	eksp->eks_wrong_node =
		MCDI_OUT_DWORD(req, LICENSING_V3_OUT_WRONG_NODE_KEYS);
	eksp->eks_licensed_apps_lo =
		MCDI_OUT_DWORD(req, LICENSING_V3_OUT_LICENSED_APPS_LO);
	eksp->eks_licensed_apps_hi =
		MCDI_OUT_DWORD(req, LICENSING_V3_OUT_LICENSED_APPS_HI);
	eksp->eks_licensed_features_lo =
		MCDI_OUT_DWORD(req, LICENSING_V3_OUT_LICENSED_FEATURES_LO);
	eksp->eks_licensed_features_hi =
		MCDI_OUT_DWORD(req, LICENSING_V3_OUT_LICENSED_FEATURES_HI);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_v3_app_state(
	__in		efx_nic_t *enp,
	__in		uint64_t app_id,
	__out		boolean_t *licensedp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_LICENSED_V3_APP_STATE_IN_LEN,
			    MC_CMD_GET_LICENSED_V3_APP_STATE_OUT_LEN)];
	uint32_t app_state;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_MEDFORD);

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_LICENSED_V3_APP_STATE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_LICENSED_V3_APP_STATE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_LICENSED_V3_APP_STATE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, GET_LICENSED_V3_APP_STATE_IN_APP_ID_LO,
		    app_id & 0xffffffff);
	MCDI_IN_SET_DWORD(req, GET_LICENSED_V3_APP_STATE_IN_APP_ID_HI,
		    app_id >> 32);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_LICENSED_V3_APP_STATE_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	app_state = (MCDI_OUT_DWORD(req, GET_LICENSED_V3_APP_STATE_OUT_STATE));
	if (app_state != MC_CMD_GET_LICENSED_V3_APP_STATE_OUT_NOT_LICENSED) {
		*licensedp = B_TRUE;
	} else {
		*licensedp = B_FALSE;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_licensing_v3_get_id(
	__in		efx_nic_t *enp,
	__in		size_t buffer_size,
	__out		uint32_t *typep,
	__out		size_t *lengthp,
	__out_bcount_part_opt(buffer_size, *lengthp)
			uint8_t *bufferp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_LICENSING_GET_ID_V3_IN_LEN,
			    MC_CMD_LICENSING_GET_ID_V3_OUT_LENMIN)];
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_LICENSING_GET_ID_V3;

	if (bufferp == NULL) {
		/* Request id type and length only */
		req.emr_in_buf = bufferp;
		req.emr_in_length = MC_CMD_LICENSING_GET_ID_V3_IN_LEN;
		req.emr_out_buf = bufferp;
		req.emr_out_length = MC_CMD_LICENSING_GET_ID_V3_OUT_LENMIN;
		(void) memset(payload, 0, sizeof (payload));
	} else {
		/* Request full buffer */
		req.emr_in_buf = bufferp;
		req.emr_in_length = MC_CMD_LICENSING_GET_ID_V3_IN_LEN;
		req.emr_out_buf = bufferp;
		req.emr_out_length = MIN(buffer_size, MC_CMD_LICENSING_GET_ID_V3_OUT_LENMIN);
		(void) memset(bufferp, 0, req.emr_out_length);
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_LICENSING_GET_ID_V3_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*typep = MCDI_OUT_DWORD(req, LICENSING_GET_ID_V3_OUT_LICENSE_TYPE);
	*lengthp = MCDI_OUT_DWORD(req, LICENSING_GET_ID_V3_OUT_LICENSE_ID_LENGTH);

	if (bufferp == NULL) {
		/* modify length requirements to indicate to caller the extra buffering
		** needed to read the complete output.
		*/
		*lengthp += MC_CMD_LICENSING_GET_ID_V3_OUT_LENMIN;
	} else {
		/* Shift ID down to start of buffer */
		memmove(bufferp,
		  bufferp+MC_CMD_LICENSING_GET_ID_V3_OUT_LICENSE_ID_OFST,
		  *lengthp);
		memset(bufferp+(*lengthp), 0, MC_CMD_LICENSING_GET_ID_V3_OUT_LICENSE_ID_OFST);
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


#endif	/* EFSYS_OPT_MEDFORD */

	__checkReturn		efx_rc_t
efx_lic_init(
	__in			efx_nic_t *enp)
{
	efx_lic_ops_t *elop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_LIC));

	switch (enp->en_family) {

#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		elop = (efx_lic_ops_t *)&__efx_lic_v1_ops;
		break;
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		elop = (efx_lic_ops_t *)&__efx_lic_v2_ops;
		break;
#endif	/* EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD:
		elop = (efx_lic_ops_t *)&__efx_lic_v3_ops;
		break;
#endif	/* EFSYS_OPT_MEDFORD */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	enp->en_elop = elop;
	enp->en_mod_flags |= EFX_MOD_LIC;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

				void
efx_lic_fini(
	__in			efx_nic_t *enp)
{
	efx_lic_ops_t *elop = enp->en_elop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_LIC);

	enp->en_elop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_LIC;
}


	__checkReturn	efx_rc_t
efx_lic_update_licenses(
	__in		efx_nic_t *enp)
{
	efx_lic_ops_t *elop = enp->en_elop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_LIC);

	if ((rc = elop->elo_update_licenses(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_lic_get_key_stats(
	__in		efx_nic_t *enp,
	__out		efx_key_stats_t *eksp)
{
	efx_lic_ops_t *elop = enp->en_elop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_LIC);

	if ((rc = elop->elo_get_key_stats(enp, eksp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_lic_app_state(
	__in		efx_nic_t *enp,
	__in		uint64_t app_id,
	__out		boolean_t *licensedp)
{
	efx_lic_ops_t *elop = enp->en_elop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_LIC);

	if (elop->elo_app_state == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}
	if ((rc = elop->elo_app_state(enp, app_id, licensedp)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_lic_get_id(
	__in		efx_nic_t *enp,
	__in		size_t buffer_size,
	__out		uint32_t *typep,
	__out		size_t *lengthp,
	__out_opt	uint8_t *bufferp
	)
{
	efx_lic_ops_t *elop = enp->en_elop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_LIC);

	if (elop->elo_get_id == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = elop->elo_get_id(enp, buffer_size, typep,
				    lengthp, bufferp)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_LICENSING */
