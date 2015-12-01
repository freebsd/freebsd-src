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

#if EFSYS_OPT_SIENA && EFSYS_OPT_MCDI

#define	SIENA_MCDI_PDU(_emip)			\
	(((emip)->emi_port == 1)		\
	? MC_SMEM_P0_PDU_OFST >> 2		\
	: MC_SMEM_P1_PDU_OFST >> 2)

#define	SIENA_MCDI_DOORBELL(_emip)		\
	(((emip)->emi_port == 1)		\
	? MC_SMEM_P0_DOORBELL_OFST >> 2		\
	: MC_SMEM_P1_DOORBELL_OFST >> 2)

#define	SIENA_MCDI_STATUS(_emip)		\
	(((emip)->emi_port == 1)		\
	? MC_SMEM_P0_STATUS_OFST >> 2		\
	: MC_SMEM_P1_STATUS_OFST >> 2)


			void
siena_mcdi_request_copyin(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__in		unsigned int seq,
	__in		boolean_t ev_cpl,
	__in		boolean_t new_epoch)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_dword_t dword;
	unsigned int xflags;
	unsigned int pdur;
	unsigned int dbr;
	unsigned int pos;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);
	_NOTE(ARGUNUSED(new_epoch))

	EFSYS_ASSERT(emip->emi_port == 1 || emip->emi_port == 2);
	pdur = SIENA_MCDI_PDU(emip);
	dbr = SIENA_MCDI_DOORBELL(emip);

	xflags = 0;
	if (ev_cpl)
		xflags |= MCDI_HEADER_XFLAGS_EVREQ;

	/* Construct the header in shared memory */
	EFX_POPULATE_DWORD_6(dword,
			    MCDI_HEADER_CODE, emrp->emr_cmd,
			    MCDI_HEADER_RESYNC, 1,
			    MCDI_HEADER_DATALEN, emrp->emr_in_length,
			    MCDI_HEADER_SEQ, seq,
			    MCDI_HEADER_RESPONSE, 0,
			    MCDI_HEADER_XFLAGS, xflags);
	EFX_BAR_TBL_WRITED(enp, FR_CZ_MC_TREG_SMEM, pdur, &dword, B_TRUE);

	for (pos = 0; pos < emrp->emr_in_length; pos += sizeof (efx_dword_t)) {
		memcpy(&dword, MCDI_IN(*emrp, efx_dword_t, pos),
		    MIN(sizeof (dword), emrp->emr_in_length - pos));
		EFX_BAR_TBL_WRITED(enp, FR_CZ_MC_TREG_SMEM,
		    pdur + 1 + (pos >> 2), &dword, B_FALSE);
	}

	/* Ring the doorbell */
	EFX_POPULATE_DWORD_1(dword, EFX_DWORD_0, 0xd004be11);
	EFX_BAR_TBL_WRITED(enp, FR_CZ_MC_TREG_SMEM, dbr, &dword, B_FALSE);
}

			void
siena_mcdi_request_copyout(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	unsigned int pos;
	unsigned int pdur;
	efx_dword_t data;

	pdur = SIENA_MCDI_PDU(emip);

	/* Copy payload out if caller supplied buffer */
	if (emrp->emr_out_buf != NULL) {
		size_t bytes = MIN(emrp->emr_out_length_used,
				    emrp->emr_out_length);
		for (pos = 0; pos < bytes; pos += sizeof (efx_dword_t)) {
			EFX_BAR_TBL_READD(enp, FR_CZ_MC_TREG_SMEM,
			    pdur + 1 + (pos >> 2), &data, B_FALSE);
			memcpy(MCDI_OUT(*emrp, efx_dword_t, pos), &data,
			    MIN(sizeof (data), bytes - pos));
		}
	}
}

			efx_rc_t
siena_mcdi_poll_reboot(
	__in		efx_nic_t *enp)
{
#ifndef EFX_GRACEFUL_MC_REBOOT
 	/*
	 * This function is not being used properly.
	 * Until its callers are fixed, it should always return 0.
	 */
	_NOTE(ARGUNUSED(enp))
	return (0);
#else
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	unsigned int rebootr;
	efx_dword_t dword;
	uint32_t value;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);
	EFSYS_ASSERT(emip->emi_port == 1 || emip->emi_port == 2);
	rebootr = SIENA_MCDI_STATUS(emip);

	EFX_BAR_TBL_READD(enp, FR_CZ_MC_TREG_SMEM, rebootr, &dword, B_FALSE);
	value = EFX_DWORD_FIELD(dword, EFX_DWORD_0);

	if (value == 0)
		return (0);

	EFX_ZERO_DWORD(dword);
	EFX_BAR_TBL_WRITED(enp, FR_CZ_MC_TREG_SMEM, rebootr, &dword, B_FALSE);

	if (value == MC_STATUS_DWORD_ASSERT)
		return (EINTR);
	else
		return (EIO);
#endif
}

	__checkReturn	boolean_t
siena_mcdi_request_poll(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_mcdi_req_t *emrp;
	efx_dword_t dword;
	unsigned int pdur;
	unsigned int seq;
	unsigned int length;
	int state;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	/* Serialise against post-watchdog efx_mcdi_ev* */
	EFSYS_LOCK(enp->en_eslp, state);

	EFSYS_ASSERT(emip->emi_pending_req != NULL);
	EFSYS_ASSERT(!emip->emi_ev_cpl);
	emrp = emip->emi_pending_req;

	/* Check for reboot atomically w.r.t efx_mcdi_request_start */
	if (emip->emi_poll_cnt++ == 0) {
		if ((rc = siena_mcdi_poll_reboot(enp)) != 0) {
			emip->emi_pending_req = NULL;
			EFSYS_UNLOCK(enp->en_eslp, state);

			goto fail1;
		}
	}

	EFSYS_ASSERT(emip->emi_port == 1 || emip->emi_port == 2);
	pdur = SIENA_MCDI_PDU(emip);

	/* Read the command header */
	EFX_BAR_TBL_READD(enp, FR_CZ_MC_TREG_SMEM, pdur, &dword, B_FALSE);
	if (EFX_DWORD_FIELD(dword, MCDI_HEADER_RESPONSE) == 0) {
		EFSYS_UNLOCK(enp->en_eslp, state);
		return (B_FALSE);
	}

	/* Request complete */
	emip->emi_pending_req = NULL;
	seq = (emip->emi_seq - 1) & EFX_MASK32(MCDI_HEADER_SEQ);

	/* Check for synchronous reboot */
	if (EFX_DWORD_FIELD(dword, MCDI_HEADER_ERROR) != 0 &&
	    EFX_DWORD_FIELD(dword, MCDI_HEADER_DATALEN) == 0) {
		/* Consume status word */
		EFSYS_SPIN(EFX_MCDI_STATUS_SLEEP_US);
		siena_mcdi_poll_reboot(enp);
		EFSYS_UNLOCK(enp->en_eslp, state);
		rc = EIO;
		goto fail2;
	}

	EFSYS_UNLOCK(enp->en_eslp, state);

	/* Check that the returned data is consistent */
	if (EFX_DWORD_FIELD(dword, MCDI_HEADER_CODE) != emrp->emr_cmd ||
	    EFX_DWORD_FIELD(dword, MCDI_HEADER_SEQ) != seq) {
		/* Response is for a different request */
		rc = EIO;
		goto fail3;
	}

	length = EFX_DWORD_FIELD(dword, MCDI_HEADER_DATALEN);
	if (EFX_DWORD_FIELD(dword, MCDI_HEADER_ERROR)) {
		efx_dword_t errdword;
		int errcode;

		EFSYS_ASSERT3U(length, ==, 4);
		EFX_BAR_TBL_READD(enp, FR_CZ_MC_TREG_SMEM,
		    pdur + 1 + (MC_CMD_ERR_CODE_OFST >> 2),
		    &errdword, B_FALSE);
		errcode = EFX_DWORD_FIELD(errdword, EFX_DWORD_0);
		rc = efx_mcdi_request_errcode(errcode);
		if (!emrp->emr_quiet) {
			EFSYS_PROBE2(mcdi_err, int, emrp->emr_cmd,
			    int, errcode);
		}
		goto fail4;

	} else {
		emrp->emr_out_length_used = length;
		emrp->emr_rc = 0;
		siena_mcdi_request_copyout(enp, emrp);
	}

	goto out;

fail4:
	if (!emrp->emr_quiet)
		EFSYS_PROBE(fail4);
fail3:
	if (!emrp->emr_quiet)
		EFSYS_PROBE(fail3);
fail2:
	if (!emrp->emr_quiet)
		EFSYS_PROBE(fail2);
fail1:
	if (!emrp->emr_quiet)
		EFSYS_PROBE1(fail1, efx_rc_t, rc);

	/* Fill out error state */
	emrp->emr_rc = rc;
	emrp->emr_out_length_used = 0;

	/* Reboot/Assertion */
	if (rc == EIO || rc == EINTR)
		efx_mcdi_raise_exception(enp, emrp, rc);

out:
	return (B_TRUE);
}

	__checkReturn	efx_rc_t
siena_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *mtp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_oword_t oword;
	unsigned int portnum;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);

	/* Determine the port number to use for MCDI */
	EFX_BAR_READO(enp, FR_AZ_CS_DEBUG_REG, &oword);
	portnum = EFX_OWORD_FIELD(oword, FRF_CZ_CS_PORT_NUM);

	if (portnum == 0) {
		/* Presumably booted from ROM; only MCDI port 1 will work */
		emip->emi_port = 1;
	} else if (portnum <= 2) {
		emip->emi_port = portnum;
	} else {
		rc = EINVAL;
		goto fail1;
	}

	/*
	 * Wipe the atomic reboot status so subsequent MCDI requests succeed.
	 * BOOT_STATUS is preserved so eno_nic_probe() can boot out of the
	 * assertion handler.
	 */
	(void) siena_mcdi_poll_reboot(enp);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
siena_mcdi_fini(
	__in		efx_nic_t *enp)
{
}

	__checkReturn	efx_rc_t
siena_mcdi_fw_update_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp)
{
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	*supportedp = B_TRUE;

	return (0);
}

	__checkReturn	efx_rc_t
siena_mcdi_macaddr_change_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp)
{
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	*supportedp = B_TRUE;

	return (0);
}

	__checkReturn	efx_rc_t
siena_mcdi_link_control_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp)
{
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	*supportedp = B_TRUE;

	return (0);
}

#endif	/* EFSYS_OPT_SIENA && EFSYS_OPT_MCDI */
