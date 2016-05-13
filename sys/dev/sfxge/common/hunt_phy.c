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

#if EFSYS_OPT_HUNTINGTON

#if EFSYS_OPT_BIST

	__checkReturn		efx_rc_t
hunt_bist_enable_offline(
	__in			efx_nic_t *enp)
{
	efx_rc_t rc;

	if ((rc = efx_mcdi_bist_enable_offline(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
hunt_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type)
{
	efx_rc_t rc;

	if ((rc = efx_mcdi_bist_start(enp, type)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
hunt_bist_poll(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type,
	__out			efx_bist_result_t *resultp,
	__out_opt __drv_when(count > 0, __notnull)
	uint32_t *value_maskp,
	__out_ecount_opt(count)	__drv_when(count > 0, __notnull)
	unsigned long *valuesp,
	__in			size_t count)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_POLL_BIST_IN_LEN,
			    MCDI_CTL_SDU_LEN_MAX)];
	uint32_t value_mask = 0;
	uint32_t result;
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_POLL_BIST;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_POLL_BIST_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MCDI_CTL_SDU_LEN_MAX;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_POLL_BIST_OUT_RESULT_OFST + 4) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (count > 0)
		(void) memset(valuesp, '\0', count * sizeof (unsigned long));

	result = MCDI_OUT_DWORD(req, POLL_BIST_OUT_RESULT);

	if (result == MC_CMD_POLL_BIST_FAILED &&
	    req.emr_out_length >= MC_CMD_POLL_BIST_OUT_MEM_LEN &&
	    count > EFX_BIST_MEM_ECC_FATAL) {
		if (valuesp != NULL) {
			valuesp[EFX_BIST_MEM_TEST] =
			    MCDI_OUT_DWORD(req, POLL_BIST_OUT_MEM_TEST);
			valuesp[EFX_BIST_MEM_ADDR] =
			    MCDI_OUT_DWORD(req, POLL_BIST_OUT_MEM_ADDR);
			valuesp[EFX_BIST_MEM_BUS] =
			    MCDI_OUT_DWORD(req, POLL_BIST_OUT_MEM_BUS);
			valuesp[EFX_BIST_MEM_EXPECT] =
			    MCDI_OUT_DWORD(req, POLL_BIST_OUT_MEM_EXPECT);
			valuesp[EFX_BIST_MEM_ACTUAL] =
			    MCDI_OUT_DWORD(req, POLL_BIST_OUT_MEM_ACTUAL);
			valuesp[EFX_BIST_MEM_ECC] =
			    MCDI_OUT_DWORD(req, POLL_BIST_OUT_MEM_ECC);
			valuesp[EFX_BIST_MEM_ECC_PARITY] =
			    MCDI_OUT_DWORD(req, POLL_BIST_OUT_MEM_ECC_PARITY);
			valuesp[EFX_BIST_MEM_ECC_FATAL] =
			    MCDI_OUT_DWORD(req, POLL_BIST_OUT_MEM_ECC_FATAL);
		}
		value_mask |= (1 << EFX_BIST_MEM_TEST) |
		    (1 << EFX_BIST_MEM_ADDR) |
		    (1 << EFX_BIST_MEM_BUS) |
		    (1 << EFX_BIST_MEM_EXPECT) |
		    (1 << EFX_BIST_MEM_ACTUAL) |
		    (1 << EFX_BIST_MEM_ECC) |
		    (1 << EFX_BIST_MEM_ECC_PARITY) |
		    (1 << EFX_BIST_MEM_ECC_FATAL);
	} else if (result == MC_CMD_POLL_BIST_FAILED &&
	    encp->enc_phy_type == EFX_PHY_XFI_FARMI &&
	    req.emr_out_length >= MC_CMD_POLL_BIST_OUT_MRSFP_LEN &&
	    count > EFX_BIST_FAULT_CODE) {
		if (valuesp != NULL)
			valuesp[EFX_BIST_FAULT_CODE] =
			    MCDI_OUT_DWORD(req, POLL_BIST_OUT_MRSFP_TEST);
		value_mask |= 1 << EFX_BIST_FAULT_CODE;
	}

	if (value_maskp != NULL)
		*value_maskp = value_mask;

	EFSYS_ASSERT(resultp != NULL);
	if (result == MC_CMD_POLL_BIST_RUNNING)
		*resultp = EFX_BIST_RESULT_RUNNING;
	else if (result == MC_CMD_POLL_BIST_PASSED)
		*resultp = EFX_BIST_RESULT_PASSED;
	else
		*resultp = EFX_BIST_RESULT_FAILED;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
hunt_bist_stop(
	__in		efx_nic_t *enp,
	__in		efx_bist_type_t type)
{
	/* There is no way to stop BIST on Huntinton. */
	_NOTE(ARGUNUSED(enp, type))
}

#endif	/* EFSYS_OPT_BIST */

#endif	/* EFSYS_OPT_HUNTINGTON */
