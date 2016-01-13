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


static			void
siena_mcdi_send_request(
	__in		efx_nic_t *enp,
	__in		void *hdrp,
	__in		size_t hdr_len,
	__in		void *sdup,
	__in		size_t sdu_len)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_dword_t dword;
	unsigned int pdur;
	unsigned int dbr;
	unsigned int pos;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);

	EFSYS_ASSERT(emip->emi_port == 1 || emip->emi_port == 2);
	pdur = SIENA_MCDI_PDU(emip);
	dbr = SIENA_MCDI_DOORBELL(emip);

	/* Write the header */
	EFSYS_ASSERT3U(hdr_len, ==, sizeof (efx_dword_t));
	dword = *(efx_dword_t *)hdrp;
	EFX_BAR_TBL_WRITED(enp, FR_CZ_MC_TREG_SMEM, pdur, &dword, B_TRUE);

	/* Write the payload */
	for (pos = 0; pos < sdu_len; pos += sizeof (efx_dword_t)) {
		dword = *(efx_dword_t *)((uint8_t *)sdup + pos);
		EFX_BAR_TBL_WRITED(enp, FR_CZ_MC_TREG_SMEM,
		    pdur + 1 + (pos >> 2), &dword, B_FALSE);
	}

	/* Ring the doorbell */
	EFX_POPULATE_DWORD_1(dword, EFX_DWORD_0, 0xd004be11);
	EFX_BAR_TBL_WRITED(enp, FR_CZ_MC_TREG_SMEM, dbr, &dword, B_FALSE);
}

			void
siena_mcdi_request_copyin(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__in		unsigned int seq,
	__in		boolean_t ev_cpl,
	__in		boolean_t new_epoch)
{
#if EFSYS_OPT_MCDI_LOGGING
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
#endif
	efx_dword_t hdr;
	size_t hdr_len;
	unsigned int xflags;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);
	_NOTE(ARGUNUSED(new_epoch))

	xflags = 0;
	if (ev_cpl)
		xflags |= MCDI_HEADER_XFLAGS_EVREQ;

	/* Construct the header */
	hdr_len = sizeof (hdr);
	EFX_POPULATE_DWORD_6(hdr,
			    MCDI_HEADER_CODE, emrp->emr_cmd,
			    MCDI_HEADER_RESYNC, 1,
			    MCDI_HEADER_DATALEN, emrp->emr_in_length,
			    MCDI_HEADER_SEQ, seq,
			    MCDI_HEADER_RESPONSE, 0,
			    MCDI_HEADER_XFLAGS, xflags);

#if EFSYS_OPT_MCDI_LOGGING
	if (emtp->emt_logger != NULL) {
		emtp->emt_logger(emtp->emt_context, EFX_LOG_MCDI_REQUEST,
		    &hdr, sizeof (hdr),
		    emrp->emr_in_buf, emrp->emr_in_length);
	}
#endif /* EFSYS_OPT_MCDI_LOGGING */

	siena_mcdi_send_request(enp, &hdr, hdr_len,
	    emrp->emr_in_buf, emrp->emr_in_length);
}

			void
siena_mcdi_request_copyout(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp)
{
#if EFSYS_OPT_MCDI_LOGGING
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efx_dword_t hdr;
#endif
	size_t bytes = MIN(emrp->emr_out_length_used, emrp->emr_out_length);

	/* Copy payload out if caller supplied buffer */
	if (emrp->emr_out_buf != NULL) {
		siena_mcdi_read_response(enp, emrp->emr_out_buf,
		    sizeof (efx_dword_t), bytes);
	}

#if EFSYS_OPT_MCDI_LOGGING
	if (emtp->emt_logger != NULL) {
		siena_mcdi_read_response(enp, &hdr, 0, sizeof (hdr));

		emtp->emt_logger(emtp->emt_context,
		    EFX_LOG_MCDI_RESPONSE,
		    &hdr, sizeof (hdr),
		    emrp->emr_out_buf, bytes);
	}
#endif /* EFSYS_OPT_MCDI_LOGGING */
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

extern	__checkReturn	boolean_t
siena_mcdi_poll_response(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_dword_t hdr;
	unsigned int pdur;

	EFSYS_ASSERT(emip->emi_port == 1 || emip->emi_port == 2);
	pdur = SIENA_MCDI_PDU(emip);

	EFX_BAR_TBL_READD(enp, FR_CZ_MC_TREG_SMEM, pdur, &hdr, B_FALSE);
	return (EFX_DWORD_FIELD(hdr, MCDI_HEADER_RESPONSE) ? B_TRUE : B_FALSE);
}

			void
siena_mcdi_read_response(
	__in			efx_nic_t *enp,
	__out_bcount(length)	void *bufferp,
	__in			size_t offset,
	__in			size_t length)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	unsigned int pdur;
	unsigned int pos;
	efx_dword_t data;

	EFSYS_ASSERT(emip->emi_port == 1 || emip->emi_port == 2);
	pdur = SIENA_MCDI_PDU(emip);

	for (pos = 0; pos < length; pos += sizeof (efx_dword_t)) {
		EFX_BAR_TBL_READD(enp, FR_CZ_MC_TREG_SMEM,
		    pdur + ((offset + pos) >> 2), &data, B_FALSE);
		memcpy((uint8_t *)bufferp + pos, &data,
		    MIN(sizeof (data), length - pos));
	}
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

	/* Siena BootROM and firmware only support MCDIv1 */
	emip->emi_max_version = 1;

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
siena_mcdi_feature_supported(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_feature_id_t id,
	__out		boolean_t *supportedp)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	switch (id) {
	case EFX_MCDI_FEATURE_FW_UPDATE:
	case EFX_MCDI_FEATURE_LINK_CONTROL:
	case EFX_MCDI_FEATURE_MACADDR_CHANGE:
	case EFX_MCDI_FEATURE_MAC_SPOOFING:
		*supportedp = B_TRUE;
		break;
	default:
		rc = ENOTSUP;
		goto fail1;
		break;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_SIENA && EFSYS_OPT_MCDI */
