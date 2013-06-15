/*-
 * Copyright 2008-2009 Solarflare Communications Inc.  All rights reserved.
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
#include "efx_regs_mcdi.h"
#include "efx_impl.h"

#if EFSYS_OPT_MCDI

/* Shared memory layout */

#define	MCDI_P1_DBL_OFST	0x0
#define	MCDI_P2_DBL_OFST	0x1
#define	MCDI_P1_PDU_OFST	0x2
#define	MCDI_P2_PDU_OFST	0x42
#define	MCDI_P1_REBOOT_OFST	0x1fe
#define	MCDI_P2_REBOOT_OFST	0x1ff

/* A reboot/assertion causes the MCDI status word to be set after the
 * command word is set or a REBOOT event is sent. If we notice a reboot
 * via these mechanisms then wait 10ms for the status word to be set.
 */
#define	MCDI_STATUS_SLEEP_US	10000

			void
efx_mcdi_request_start(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__in		boolean_t ev_cpl)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	efx_dword_t dword;
	unsigned int seq;
	unsigned int xflags;
	unsigned int pdur;
	unsigned int dbr;
	unsigned int pos;
	int state;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	switch (emip->emi_port)	{
	case 1:
		pdur = MCDI_P1_PDU_OFST;
		dbr = MCDI_P1_DBL_OFST;
		break;
	case 2:
		pdur = MCDI_P2_PDU_OFST;
		dbr = MCDI_P2_DBL_OFST;
		break;
	default:
		EFSYS_ASSERT(0);
		pdur = dbr = 0;
	};

	/*
	 * efx_mcdi_request_start() is naturally serialised against both
	 * efx_mcdi_request_poll() and efx_mcdi_ev_cpl()/efx_mcdi_ev_death(),
	 * by virtue of there only being one outstanding MCDI request.
	 * Unfortunately, upper layers may also call efx_mcdi_request_abort()
	 * at any time, to timeout a pending mcdi request, That request may
	 * then subsequently complete, meaning efx_mcdi_ev_cpl() or
	 * efx_mcdi_ev_death() may end up running in parallel with
	 * efx_mcdi_request_start(). This race is handled by ensuring that
	 * %emi_pending_req, %emi_ev_cpl and %emi_seq are protected by the
	 * en_eslp lock.
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	EFSYS_ASSERT(emip->emi_pending_req == NULL);
	emip->emi_pending_req = emrp;
	emip->emi_ev_cpl = ev_cpl;
	emip->emi_poll_cnt = 0;
	seq = emip->emi_seq++ & 0xf;
	EFSYS_UNLOCK(enp->en_eslp, state);

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

static			void
efx_mcdi_request_copyout(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	unsigned int pos;
	unsigned int pdur;
	efx_dword_t data;

	pdur = (emip->emi_port == 1) ? MCDI_P1_PDU_OFST : MCDI_P2_PDU_OFST;

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

static			int
efx_mcdi_request_errcode(
	__in		unsigned int err)
{

	switch (err) {
	case MC_CMD_ERR_ENOENT:
		return (ENOENT);
	case MC_CMD_ERR_EINTR:
		return (EINTR);
	case MC_CMD_ERR_EACCES:
		return (EACCES);
	case MC_CMD_ERR_EBUSY:
		return (EBUSY);
	case MC_CMD_ERR_EINVAL:
		return (EINVAL);
	case MC_CMD_ERR_EDEADLK:
		return (EDEADLK);
	case MC_CMD_ERR_ENOSYS:
		return (ENOTSUP);
	case MC_CMD_ERR_ETIME:
		return (ETIMEDOUT);
#ifdef WITH_MCDI_V2
	case MC_CMD_ERR_EAGAIN:
		return (EAGAIN);
	case MC_CMD_ERR_ENOSPC:
		return (ENOSPC);
#endif
	default:
		EFSYS_PROBE1(mc_pcol_error, int, err);
		return (EIO);
	}
}

static			void
efx_mcdi_raise_exception(
	__in		efx_nic_t *enp,
	__in_opt	efx_mcdi_req_t *emrp,
	__in		int rc)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	const efx_mcdi_transport_t *emtp = emip->emi_mtp;
	efx_mcdi_exception_t exception;

	/* Reboot or Assertion failure only */
	EFSYS_ASSERT(rc == EIO || rc == EINTR);

	/*
	 * If MC_CMD_REBOOT causes a reboot (dependent on parameters),
	 * then the EIO is not worthy of an exception.
	 */
	if (emrp != NULL && emrp->emr_cmd == MC_CMD_REBOOT && rc == EIO)
		return;

	exception = (rc == EIO)
		? EFX_MCDI_EXCEPTION_MC_REBOOT
		: EFX_MCDI_EXCEPTION_MC_BADASSERT;

	emtp->emt_exception(emtp->emt_context, exception);
}

static			int
efx_mcdi_poll_reboot(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	unsigned int rebootr;
	efx_dword_t dword;
	uint32_t value;

	EFSYS_ASSERT(emip->emi_port == 1 || emip->emi_port == 2);
	rebootr = ((emip->emi_port == 1)
	    ? MCDI_P1_REBOOT_OFST
	    : MCDI_P2_REBOOT_OFST);

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
}

	__checkReturn	boolean_t
efx_mcdi_request_poll(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	efx_mcdi_req_t *emrp;
	efx_dword_t dword;
	unsigned int pdur;
	unsigned int seq;
	unsigned int length;
	int state;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	/* Serialise against post-watchdog efx_mcdi_ev* */
	EFSYS_LOCK(enp->en_eslp, state);

	EFSYS_ASSERT(emip->emi_pending_req != NULL);
	EFSYS_ASSERT(!emip->emi_ev_cpl);
	emrp = emip->emi_pending_req;

	/* Check for reboot atomically w.r.t efx_mcdi_request_start */
	if (emip->emi_poll_cnt++ == 0) {
		if ((rc = efx_mcdi_poll_reboot(enp)) != 0) {
			emip->emi_pending_req = NULL;
			EFSYS_UNLOCK(enp->en_eslp, state);

			goto fail1;
		}
	}

	EFSYS_ASSERT(emip->emi_port == 1 || emip->emi_port == 2);
	pdur = (emip->emi_port == 1) ? MCDI_P1_PDU_OFST : MCDI_P2_PDU_OFST;

	/* Read the command header */
	EFX_BAR_TBL_READD(enp, FR_CZ_MC_TREG_SMEM, pdur, &dword, B_FALSE);
	if (EFX_DWORD_FIELD(dword, MCDI_HEADER_RESPONSE) == 0) {
		EFSYS_UNLOCK(enp->en_eslp, state);
		return (B_FALSE);
	}

	/* Request complete */
	emip->emi_pending_req = NULL;
	seq = (emip->emi_seq - 1) & 0xf;

	/* Check for synchronous reboot */
	if (EFX_DWORD_FIELD(dword, MCDI_HEADER_ERROR) != 0 &&
	    EFX_DWORD_FIELD(dword, MCDI_HEADER_DATALEN) == 0) {
		/* Consume status word */
		EFSYS_SPIN(MCDI_STATUS_SLEEP_US);
		efx_mcdi_poll_reboot(enp);
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
		EFSYS_PROBE2(mcdi_err, int, emrp->emr_cmd, int, errcode);
		goto fail4;

	} else {
		emrp->emr_out_length_used = length;
		emrp->emr_rc = 0;
		efx_mcdi_request_copyout(enp, emrp);
	}

	goto out;

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	/* Fill out error state */
	emrp->emr_rc = rc;
	emrp->emr_out_length_used = 0;

	/* Reboot/Assertion */
	if (rc == EIO || rc == EINTR)
		efx_mcdi_raise_exception(enp, emrp, rc);

out:
	return (B_TRUE);
}

			void
efx_mcdi_execute(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	const efx_mcdi_transport_t *emtp = emip->emi_mtp;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	emtp->emt_execute(emtp->emt_context, emrp);
}

			void
efx_mcdi_ev_cpl(
	__in		efx_nic_t *enp,
	__in		unsigned int seq,
	__in		unsigned int outlen,
	__in		int errcode)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	const efx_mcdi_transport_t *emtp = emip->emi_mtp;
	efx_mcdi_req_t *emrp;
	int state;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	/*
	 * Serialise against efx_mcdi_request_poll()/efx_mcdi_request_start()
	 * when we're completing an aborted request.
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	if (emip->emi_pending_req == NULL || !emip->emi_ev_cpl ||
	    (seq != ((emip->emi_seq - 1) & 0xf))) {
		EFSYS_ASSERT(emip->emi_aborted > 0);
		if (emip->emi_aborted > 0)
			--emip->emi_aborted;
		EFSYS_UNLOCK(enp->en_eslp, state);
		return;
	}

	emrp = emip->emi_pending_req;
	emip->emi_pending_req = NULL;
	EFSYS_UNLOCK(enp->en_eslp, state);

	/*
	 * Fill out the remaining hdr fields, and copyout the payload
	 * if the user supplied an output buffer.
	 */
	if (errcode != 0) {
		EFSYS_PROBE2(mcdi_err, int, emrp->emr_cmd,
		    int, errcode);
		emrp->emr_out_length_used = 0;
		emrp->emr_rc = efx_mcdi_request_errcode(errcode);
	} else {
		emrp->emr_out_length_used = outlen;
		emrp->emr_rc = 0;
		efx_mcdi_request_copyout(enp, emrp);
	}

	emtp->emt_ev_cpl(emtp->emt_context);
}

			void
efx_mcdi_ev_death(
	__in		efx_nic_t *enp,
	__in		int rc)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	const efx_mcdi_transport_t *emtp = emip->emi_mtp;
	efx_mcdi_req_t *emrp = NULL;
	boolean_t ev_cpl;
	int state;

	/*
	 * The MCDI request (if there is one) has been terminated, either
	 * by a BADASSERT or REBOOT event.
	 *
	 * If there is an outstanding event-completed MCDI operation, then we
	 * will never receive the completion event (because both MCDI
	 * completions and BADASSERT events are sent to the same evq). So
	 * complete this MCDI op.
	 *
	 * This function might run in parallel with efx_mcdi_request_poll()
	 * for poll completed mcdi requests, and also with
	 * efx_mcdi_request_start() for post-watchdog completions.
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	emrp = emip->emi_pending_req;
	ev_cpl = emip->emi_ev_cpl;
	if (emrp != NULL && emip->emi_ev_cpl) {
		emip->emi_pending_req = NULL;

		emrp->emr_out_length_used = 0;
		emrp->emr_rc = rc;
		++emip->emi_aborted;
	}

	/* Since we're running in parallel with a request, consume the
	 * status word before dropping the lock.
	 */
	if (rc == EIO || rc == EINTR) {
		EFSYS_SPIN(MCDI_STATUS_SLEEP_US);
		(void) efx_mcdi_poll_reboot(enp);
	}

	EFSYS_UNLOCK(enp->en_eslp, state);

	efx_mcdi_raise_exception(enp, emrp, rc);

	if (emrp != NULL && ev_cpl)
		emtp->emt_ev_cpl(emtp->emt_context);
}

	__checkReturn		int
efx_mcdi_version(
	__in			efx_nic_t *enp,
	__out_ecount_opt(4)	uint16_t versionp[4],
	__out_opt		uint32_t *buildp,
	__out_opt		efx_mcdi_boot_t *statusp)
{
	uint8_t outbuf[MAX(MC_CMD_GET_VERSION_OUT_LEN,
		    MC_CMD_GET_BOOT_STATUS_OUT_LEN)];
	efx_mcdi_req_t req;
	efx_word_t *ver_words;
	uint16_t version[4];
	uint32_t build;
	efx_mcdi_boot_t status;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	EFX_STATIC_ASSERT(MC_CMD_GET_VERSION_IN_LEN == 0);
	req.emr_cmd = MC_CMD_GET_VERSION;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = MC_CMD_GET_VERSION_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	/* bootrom support */
	if (req.emr_out_length_used == MC_CMD_GET_VERSION_V0_OUT_LEN) {
		version[0] = version[1] = version[2] = version[3] = 0;
		build = MCDI_OUT_DWORD(req, GET_VERSION_OUT_FIRMWARE);

		goto version;
	}

	if (req.emr_out_length_used < MC_CMD_GET_VERSION_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	ver_words = MCDI_OUT2(req, efx_word_t, GET_VERSION_OUT_VERSION);
	version[0] = EFX_WORD_FIELD(ver_words[0], EFX_WORD_0);
	version[1] = EFX_WORD_FIELD(ver_words[1], EFX_WORD_0);
	version[2] = EFX_WORD_FIELD(ver_words[2], EFX_WORD_0);
	version[3] = EFX_WORD_FIELD(ver_words[3], EFX_WORD_0);
	build = MCDI_OUT_DWORD(req, GET_VERSION_OUT_FIRMWARE);

version:
	/* The bootrom doesn't understand BOOT_STATUS */
	if (build == MC_CMD_GET_VERSION_OUT_FIRMWARE_BOOTROM) {
		status = EFX_MCDI_BOOT_ROM;
		goto out;
	}

	req.emr_cmd = MC_CMD_GET_BOOT_STATUS;
	EFX_STATIC_ASSERT(MC_CMD_GET_BOOT_STATUS_IN_LEN == 0);
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = MC_CMD_GET_BOOT_STATUS_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	if (req.emr_out_length_used < MC_CMD_GET_BOOT_STATUS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail4;
	}

	if (MCDI_OUT_DWORD_FIELD(req, GET_BOOT_STATUS_OUT_FLAGS,
	    GET_BOOT_STATUS_OUT_FLAGS_PRIMARY))
		status = EFX_MCDI_BOOT_PRIMARY;
	else
		status = EFX_MCDI_BOOT_SECONDARY;

out:
	if (versionp != NULL)
		memcpy(versionp, version, sizeof (version));
	if (buildp != NULL)
		*buildp = build;
	if (statusp != NULL)
		*statusp = status;

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
efx_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *mtp)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	efx_oword_t oword;
	unsigned int portnum;
	int rc;

	EFSYS_ASSERT3U(enp->en_mod_flags, ==, 0);
	enp->en_mod_flags |= EFX_MOD_MCDI;

	if (enp->en_family == EFX_FAMILY_FALCON)
		return (0);

	emip->emi_mtp = mtp;

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
	(void) efx_mcdi_poll_reboot(enp);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	enp->en_mod_flags &= ~EFX_MOD_MCDI;

	return (rc);
}


	__checkReturn	int
efx_mcdi_reboot(
	__in		efx_nic_t *enp)
{
	uint8_t payload[MC_CMD_REBOOT_IN_LEN];
	efx_mcdi_req_t req;
	int rc;

	/*
	 * We could require the caller to have caused en_mod_flags=0 to
	 * call this function. This doesn't help the other port though,
	 * who's about to get the MC ripped out from underneath them.
	 * Since they have to cope with the subsequent fallout of MCDI
	 * failures, we should as well.
	 */
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	req.emr_cmd = MC_CMD_REBOOT;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_REBOOT_IN_LEN;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, REBOOT_IN_FLAGS, 0);

	efx_mcdi_execute(enp, &req);

	/* Invert EIO */
	if (req.emr_rc != EIO) {
		rc = EIO;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	boolean_t
efx_mcdi_request_abort(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	efx_mcdi_req_t *emrp;
	boolean_t aborted;
	int state;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	/*
	 * efx_mcdi_ev_* may have already completed this event, and be
	 * spinning/blocked on the upper layer lock. So it *is* legitimate
	 * to for emi_pending_req to be NULL. If there is a pending event
	 * completed request, then provide a "credit" to allow
	 * efx_mcdi_ev_cpl() to accept a single spurious completion.
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	emrp = emip->emi_pending_req;
	aborted = (emrp != NULL);
	if (aborted) {
		emip->emi_pending_req = NULL;

		/* Error the request */
		emrp->emr_out_length_used = 0;
		emrp->emr_rc = ETIMEDOUT;

		/* Provide a credit for seqno/emr_pending_req mismatches */
		if (emip->emi_ev_cpl)
			++emip->emi_aborted;

		/*
		 * The upper layer has called us, so we don't
		 * need to complete the request.
		 */
	}
	EFSYS_UNLOCK(enp->en_eslp, state);

	return (aborted);
}

			void
efx_mcdi_fini(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);

	EFSYS_ASSERT3U(enp->en_mod_flags, ==, EFX_MOD_MCDI);
	enp->en_mod_flags &= ~EFX_MOD_MCDI;

	if (~(enp->en_features) & EFX_FEATURE_MCDI)
		return;

	emip->emi_mtp = NULL;
	emip->emi_port = 0;
	emip->emi_aborted = 0;
}

#endif	/* EFSYS_OPT_MCDI */
