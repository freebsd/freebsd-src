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


#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD

#if EFSYS_OPT_MCDI

#ifndef WITH_MCDI_V2
#error "WITH_MCDI_V2 required for EF10 MCDIv2 commands."
#endif

typedef enum efx_mcdi_header_type_e {
	EFX_MCDI_HEADER_TYPE_V1, /* MCDIv0 (BootROM), MCDIv1 commands */
	EFX_MCDI_HEADER_TYPE_V2, /* MCDIv2 commands */
} efx_mcdi_header_type_t;

/*
 * Return the header format to use for sending an MCDI request.
 *
 * An MCDIv1 (Siena compatible) command should use MCDIv2 encapsulation if the
 * request input buffer or response output buffer are too large for the MCDIv1
 * format. An MCDIv2 command must always be sent using MCDIv2 encapsulation.
 */
#define	EFX_MCDI_HEADER_TYPE(_cmd, _length)				\
	((((_cmd) & ~EFX_MASK32(MCDI_HEADER_CODE)) ||			\
	((_length) & ~EFX_MASK32(MCDI_HEADER_DATALEN)))	?		\
	EFX_MCDI_HEADER_TYPE_V2	: EFX_MCDI_HEADER_TYPE_V1)


/*
 * MCDI Header NOT_EPOCH flag
 * ==========================
 * A new epoch begins at initial startup or after an MC reboot, and defines when
 * the MC should reject stale MCDI requests.
 *
 * The first MCDI request sent by the host should contain NOT_EPOCH=0, and all
 * subsequent requests (until the next MC reboot) should contain NOT_EPOCH=1.
 *
 * After rebooting the MC will fail all requests with NOT_EPOCH=1 by writing a
 * response with ERROR=1 and DATALEN=0 until a request is seen with NOT_EPOCH=0.
 */


	__checkReturn	efx_rc_t
ef10_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *emtp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efsys_mem_t *esmp = emtp->emt_dma_mem;
	efx_dword_t dword;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);
	EFSYS_ASSERT(enp->en_features & EFX_FEATURE_MCDI_DMA);

	/*
	 * All EF10 firmware supports MCDIv2 and MCDIv1.
	 * Medford BootROM supports MCDIv2 and MCDIv1.
	 * Huntington BootROM supports MCDIv1 only.
	 */
	emip->emi_max_version = 2;

	/* A host DMA buffer is required for EF10 MCDI */
	if (esmp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	/*
	 * Ensure that the MC doorbell is in a known state before issuing MCDI
	 * commands. The recovery algorithm requires that the MC command buffer
	 * must be 256 byte aligned. See bug24769.
	 */
	if ((EFSYS_MEM_ADDR(esmp) & 0xFF) != 0) {
		rc = EINVAL;
		goto fail2;
	}
	EFX_POPULATE_DWORD_1(dword, EFX_DWORD_0, 1);
	EFX_BAR_WRITED(enp, ER_DZ_MC_DB_HWRD_REG, &dword, B_FALSE);

	/* Save initial MC reboot status */
	(void) ef10_mcdi_poll_reboot(enp);

	/* Start a new epoch (allow fresh MCDI requests to succeed) */
	efx_mcdi_new_epoch(enp);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
ef10_mcdi_fini(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);

	emip->emi_new_epoch = B_FALSE;
}

static			void
ef10_mcdi_send_request(
	__in		efx_nic_t *enp,
	__in		void *hdrp,
	__in		size_t hdr_len,
	__in		void *sdup,
	__in		size_t sdu_len)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efsys_mem_t *esmp = emtp->emt_dma_mem;
	efx_dword_t dword;
	unsigned int pos;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	/* Write the header */
	for (pos = 0; pos < hdr_len; pos += sizeof (efx_dword_t)) {
		dword = *(efx_dword_t *)((uint8_t *)hdrp + pos);
		EFSYS_MEM_WRITED(esmp, pos, &dword);
	}

	/* Write the payload */
	for (pos = 0; pos < sdu_len; pos += sizeof (efx_dword_t)) {
		dword = *(efx_dword_t *)((uint8_t *)sdup + pos);
		EFSYS_MEM_WRITED(esmp, hdr_len + pos, &dword);
	}

	/* Guarantee ordering of memory (MCDI request) and PIO (MC doorbell) */
	EFSYS_DMA_SYNC_FOR_DEVICE(esmp, 0, hdr_len + sdu_len);
	EFSYS_PIO_WRITE_BARRIER();

	/* Ring the doorbell to post the command DMA address to the MC */
	EFX_POPULATE_DWORD_1(dword, EFX_DWORD_0,
	    EFSYS_MEM_ADDR(esmp) >> 32);
	EFX_BAR_WRITED(enp, ER_DZ_MC_DB_LWRD_REG, &dword, B_FALSE);

	EFX_POPULATE_DWORD_1(dword, EFX_DWORD_0,
	    EFSYS_MEM_ADDR(esmp) & 0xffffffff);
	EFX_BAR_WRITED(enp, ER_DZ_MC_DB_HWRD_REG, &dword, B_FALSE);
}

			void
ef10_mcdi_request_copyin(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__in		unsigned int seq,
	__in		boolean_t ev_cpl,
	__in		boolean_t new_epoch)
{
#if EFSYS_OPT_MCDI_LOGGING
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
#endif /* EFSYS_OPT_MCDI_LOGGING */
	efx_mcdi_header_type_t hdr_type;
	efx_dword_t hdr[2];
	size_t hdr_len;
	unsigned int xflags;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	xflags = 0;
	if (ev_cpl)
		xflags |= MCDI_HEADER_XFLAGS_EVREQ;

	hdr_type = EFX_MCDI_HEADER_TYPE(emrp->emr_cmd,
	    MAX(emrp->emr_in_length, emrp->emr_out_length));

	if (hdr_type == EFX_MCDI_HEADER_TYPE_V2) {
		/* Construct MCDI v2 header */
		hdr_len = sizeof (hdr);
		EFX_POPULATE_DWORD_8(hdr[0],
		    MCDI_HEADER_CODE, MC_CMD_V2_EXTN,
		    MCDI_HEADER_RESYNC, 1,
		    MCDI_HEADER_DATALEN, 0,
		    MCDI_HEADER_SEQ, seq,
		    MCDI_HEADER_NOT_EPOCH, new_epoch ? 0 : 1,
		    MCDI_HEADER_ERROR, 0,
		    MCDI_HEADER_RESPONSE, 0,
		    MCDI_HEADER_XFLAGS, xflags);

		EFX_POPULATE_DWORD_2(hdr[1],
		    MC_CMD_V2_EXTN_IN_EXTENDED_CMD, emrp->emr_cmd,
		    MC_CMD_V2_EXTN_IN_ACTUAL_LEN, emrp->emr_in_length);
	} else {
		/* Construct MCDI v1 header */
		hdr_len = sizeof (hdr[0]);
		EFX_POPULATE_DWORD_8(hdr[0],
		    MCDI_HEADER_CODE, emrp->emr_cmd,
		    MCDI_HEADER_RESYNC, 1,
		    MCDI_HEADER_DATALEN, emrp->emr_in_length,
		    MCDI_HEADER_SEQ, seq,
		    MCDI_HEADER_NOT_EPOCH, new_epoch ? 0 : 1,
		    MCDI_HEADER_ERROR, 0,
		    MCDI_HEADER_RESPONSE, 0,
		    MCDI_HEADER_XFLAGS, xflags);
	}

#if EFSYS_OPT_MCDI_LOGGING
	if (emtp->emt_logger != NULL) {
		emtp->emt_logger(emtp->emt_context, EFX_LOG_MCDI_REQUEST,
		    &hdr, hdr_len,
		    emrp->emr_in_buf, emrp->emr_in_length);
	}
#endif /* EFSYS_OPT_MCDI_LOGGING */

	ef10_mcdi_send_request(enp, &hdr[0], hdr_len,
	    emrp->emr_in_buf, emrp->emr_in_length);
}

			void
ef10_mcdi_request_copyout(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp)
{
#if EFSYS_OPT_MCDI_LOGGING
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
#endif /* EFSYS_OPT_MCDI_LOGGING */
	efx_dword_t hdr[2];
	unsigned int hdr_len;
	size_t bytes;

	if (emrp->emr_out_buf == NULL)
		return;

	/* Read the command header to detect MCDI response format */
	hdr_len = sizeof (hdr[0]);
	ef10_mcdi_read_response(enp, &hdr[0], 0, hdr_len);
	if (EFX_DWORD_FIELD(hdr[0], MCDI_HEADER_CODE) == MC_CMD_V2_EXTN) {
		/*
		 * Read the actual payload length. The length given in the event
		 * is only correct for responses with the V1 format.
		 */
		ef10_mcdi_read_response(enp, &hdr[1], hdr_len, sizeof (hdr[1]));
		hdr_len += sizeof (hdr[1]);

		emrp->emr_out_length_used = EFX_DWORD_FIELD(hdr[1],
					    MC_CMD_V2_EXTN_IN_ACTUAL_LEN);
	}

	/* Copy payload out into caller supplied buffer */
	bytes = MIN(emrp->emr_out_length_used, emrp->emr_out_length);
	ef10_mcdi_read_response(enp, emrp->emr_out_buf, hdr_len, bytes);

#if EFSYS_OPT_MCDI_LOGGING
	if (emtp->emt_logger != NULL) {
		emtp->emt_logger(emtp->emt_context,
		    EFX_LOG_MCDI_RESPONSE,
		    &hdr, hdr_len,
		    emrp->emr_out_buf, bytes);
	}
#endif /* EFSYS_OPT_MCDI_LOGGING */
}

	__checkReturn	boolean_t
ef10_mcdi_poll_response(
	__in		efx_nic_t *enp)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efsys_mem_t *esmp = emtp->emt_dma_mem;
	efx_dword_t hdr;

	EFSYS_MEM_READD(esmp, 0, &hdr);
	return (EFX_DWORD_FIELD(hdr, MCDI_HEADER_RESPONSE) ? B_TRUE : B_FALSE);
}

			void
ef10_mcdi_read_response(
	__in			efx_nic_t *enp,
	__out_bcount(length)	void *bufferp,
	__in			size_t offset,
	__in			size_t length)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efsys_mem_t *esmp = emtp->emt_dma_mem;
	unsigned int pos;
	efx_dword_t data;

	for (pos = 0; pos < length; pos += sizeof (efx_dword_t)) {
		EFSYS_MEM_READD(esmp, offset + pos, &data);
		memcpy((uint8_t *)bufferp + pos, &data,
		    MIN(sizeof (data), length - pos));
	}
}

			efx_rc_t
ef10_mcdi_poll_reboot(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_dword_t dword;
	uint32_t old_status;
	uint32_t new_status;
	efx_rc_t rc;

	old_status = emip->emi_mc_reboot_status;

	/* Update MC reboot status word */
	EFX_BAR_TBL_READD(enp, ER_DZ_BIU_MC_SFT_STATUS_REG, 0, &dword, B_FALSE);
	new_status = dword.ed_u32[0];

	/* MC has rebooted if the value has changed */
	if (new_status != old_status) {
		emip->emi_mc_reboot_status = new_status;

		/*
		 * FIXME: Ignore detected MC REBOOT for now.
		 *
		 * The Siena support for checking for MC reboot from status
		 * flags is broken - see comments in siena_mcdi_poll_reboot().
		 * As the generic MCDI code is shared the EF10 reboot
		 * detection suffers similar problems.
		 *
		 * Do not report an error when the boot status changes until
		 * this can be handled by common code drivers (and reworked to
		 * support Siena too).
		 */
		if (B_FALSE) {
			rc = EIO;
			goto fail1;
		}
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_mcdi_feature_supported(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_feature_id_t id,
	__out		boolean_t *supportedp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t privilege_mask = encp->enc_privilege_mask;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	/*
	 * Use privilege mask state at MCDI attach.
	 */

	switch (id) {
	case EFX_MCDI_FEATURE_FW_UPDATE:
		/*
		 * Admin privilege must be used prior to introduction of
		 * specific flag.
		 */
		*supportedp =
		    EFX_MCDI_HAVE_PRIVILEGE(privilege_mask, ADMIN);
		break;
	case EFX_MCDI_FEATURE_LINK_CONTROL:
		/*
		 * Admin privilege used prior to introduction of
		 * specific flag.
		 */
		*supportedp =
		    EFX_MCDI_HAVE_PRIVILEGE(privilege_mask, LINK) ||
		    EFX_MCDI_HAVE_PRIVILEGE(privilege_mask, ADMIN);
		break;
	case EFX_MCDI_FEATURE_MACADDR_CHANGE:
		/*
		 * Admin privilege must be used prior to introduction of
		 * mac spoofing privilege (at v4.6), which is used up to
		 * introduction of change mac spoofing privilege (at v4.7)
		 */
		*supportedp =
		    EFX_MCDI_HAVE_PRIVILEGE(privilege_mask, CHANGE_MAC) ||
		    EFX_MCDI_HAVE_PRIVILEGE(privilege_mask, MAC_SPOOFING) ||
		    EFX_MCDI_HAVE_PRIVILEGE(privilege_mask, ADMIN);
		break;
	case EFX_MCDI_FEATURE_MAC_SPOOFING:
		/*
		 * Admin privilege must be used prior to introduction of
		 * mac spoofing privilege (at v4.6), which is used up to
		 * introduction of mac spoofing TX privilege (at v4.7)
		 */
		*supportedp =
		    EFX_MCDI_HAVE_PRIVILEGE(privilege_mask, MAC_SPOOFING_TX) ||
		    EFX_MCDI_HAVE_PRIVILEGE(privilege_mask, MAC_SPOOFING) ||
		    EFX_MCDI_HAVE_PRIVILEGE(privilege_mask, ADMIN);
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

#endif	/* EFSYS_OPT_MCDI */

#endif	/* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD */
