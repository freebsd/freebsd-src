/*-
 * Copyright 2009 Solarflare Communications Inc.  All rights reserved.
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
#include "efx_impl.h"

#if EFSYS_OPT_SIENA

static	__checkReturn		int
siena_nic_get_partn_mask(
	__in			efx_nic_t *enp,
	__out			unsigned int *maskp)
{
	efx_mcdi_req_t req;
	uint8_t outbuf[MC_CMD_NVRAM_TYPES_OUT_LEN];
	int rc;

	req.emr_cmd = MC_CMD_NVRAM_TYPES;
	EFX_STATIC_ASSERT(MC_CMD_NVRAM_TYPES_IN_LEN == 0);
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof (outbuf);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_NVRAM_TYPES_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*maskp = MCDI_OUT_DWORD(req, NVRAM_TYPES_OUT_TYPES);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
siena_nic_exit_assertion_handler(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_REBOOT_IN_LEN];
	int rc;

	req.emr_cmd = MC_CMD_REBOOT;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_REBOOT_IN_LEN;
	EFX_STATIC_ASSERT(MC_CMD_REBOOT_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, REBOOT_IN_FLAGS,
			    MC_CMD_REBOOT_FLAGS_AFTER_ASSERTION);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0 && req.emr_rc != EIO) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
siena_nic_read_assertion(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_ASSERTS_IN_LEN,
			    MC_CMD_GET_ASSERTS_OUT_LEN)];
	const char *reason;
	unsigned int flags;
	unsigned int index;
	unsigned int ofst;
	int retry;
	int rc;

	/*
	 * Before we attempt to chat to the MC, we should verify that the MC
	 * isn't in it's assertion handler, either due to a previous reboot,
	 * or because we're reinitializing due to an eec_exception().
	 *
	 * Use GET_ASSERTS to read any assertion state that may be present.
	 * Retry this command twice. Once because a boot-time assertion failure
	 * might cause the 1st MCDI request to fail. And once again because
	 * we might race with siena_nic_exit_assertion_handler() running on the
	 * other port.
	 */
	retry = 2;
	do {
		req.emr_cmd = MC_CMD_GET_ASSERTS;
		req.emr_in_buf = payload;
		req.emr_in_length = MC_CMD_GET_ASSERTS_IN_LEN;
		req.emr_out_buf = payload;
		req.emr_out_length = MC_CMD_GET_ASSERTS_OUT_LEN;

		MCDI_IN_SET_DWORD(req, GET_ASSERTS_IN_CLEAR, 1);
		efx_mcdi_execute(enp, &req);

	} while ((req.emr_rc == EINTR || req.emr_rc == EIO) && retry-- > 0);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_ASSERTS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	/* Print out any assertion state recorded */
	flags = MCDI_OUT_DWORD(req, GET_ASSERTS_OUT_GLOBAL_FLAGS);
	if (flags == MC_CMD_GET_ASSERTS_FLAGS_NO_FAILS)
		return (0);

	reason = (flags == MC_CMD_GET_ASSERTS_FLAGS_SYS_FAIL)
		? "system-level assertion"
		: (flags == MC_CMD_GET_ASSERTS_FLAGS_THR_FAIL)
		? "thread-level assertion"
		: (flags == MC_CMD_GET_ASSERTS_FLAGS_WDOG_FIRED)
		? "watchdog reset"
		: "unknown assertion";
	EFSYS_PROBE3(mcpu_assertion,
	    const char *, reason, unsigned int,
	    MCDI_OUT_DWORD(req, GET_ASSERTS_OUT_SAVED_PC_OFFS),
	    unsigned int,
	    MCDI_OUT_DWORD(req, GET_ASSERTS_OUT_THREAD_OFFS));

	/* Print out the registers */
	ofst = MC_CMD_GET_ASSERTS_OUT_GP_REGS_OFFS_OFST;
	for (index = 1; index < 32; index++) {
		EFSYS_PROBE2(mcpu_register, unsigned int, index, unsigned int,
			    EFX_DWORD_FIELD(*MCDI_OUT(req, efx_dword_t, ofst),
					    EFX_DWORD_0));
		ofst += sizeof (efx_dword_t);
	}
	EFSYS_ASSERT(ofst <= MC_CMD_GET_ASSERTS_OUT_LEN);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
siena_nic_attach(
	__in		efx_nic_t *enp,
	__in		boolean_t attach)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_DRV_ATTACH_IN_LEN];
	int rc;

	req.emr_cmd = MC_CMD_DRV_ATTACH;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_DRV_ATTACH_IN_LEN;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, DRV_ATTACH_IN_NEW_STATE, attach ? 1 : 0);
	MCDI_IN_SET_DWORD(req, DRV_ATTACH_IN_UPDATE, 1);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_DRV_ATTACH_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_PCIE_TUNE

	__checkReturn	int
siena_nic_pcie_extended_sync(
	__in		efx_nic_t *enp)
{
	uint8_t inbuf[MC_CMD_WORKAROUND_IN_LEN];
	efx_mcdi_req_t req;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	req.emr_cmd = MC_CMD_WORKAROUND;
	req.emr_in_buf = inbuf;
	req.emr_in_length = sizeof (inbuf);
	EFX_STATIC_ASSERT(MC_CMD_WORKAROUND_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, WORKAROUND_IN_TYPE, MC_CMD_WORKAROUND_BUG17230);
	MCDI_IN_SET_DWORD(req, WORKAROUND_IN_ENABLED, 1);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_PCIE_TUNE */

static	__checkReturn	int
siena_board_cfg(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	uint8_t outbuf[MAX(MC_CMD_GET_BOARD_CFG_OUT_LEN,
		    MC_CMD_GET_RESOURCE_LIMITS_OUT_LEN)];
	efx_mcdi_req_t req;
	uint8_t *src;
	int rc;

	/* Board configuration */
	req.emr_cmd = MC_CMD_GET_BOARD_CFG;
	EFX_STATIC_ASSERT(MC_CMD_GET_BOARD_CFG_IN_LEN == 0);
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = MC_CMD_GET_BOARD_CFG_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_BOARD_CFG_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (emip->emi_port == 1)
		src = MCDI_OUT2(req, uint8_t,
			    GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT0);
	else
		src = MCDI_OUT2(req, uint8_t,
			    GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT1);
	EFX_MAC_ADDR_COPY(encp->enc_mac_addr, src);

	encp->enc_board_type = MCDI_OUT_DWORD(req,
				    GET_BOARD_CFG_OUT_BOARD_TYPE);

	/* Resource limits */
	req.emr_cmd = MC_CMD_GET_RESOURCE_LIMITS;
	EFX_STATIC_ASSERT(MC_CMD_GET_RESOURCE_LIMITS_IN_LEN == 0);
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = MC_CMD_GET_RESOURCE_LIMITS_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc == 0) {
		if (req.emr_out_length_used < MC_CMD_GET_RESOURCE_LIMITS_OUT_LEN) {
			rc = EMSGSIZE;
			goto fail3;
		}

		encp->enc_evq_limit = MCDI_OUT_DWORD(req,
		    GET_RESOURCE_LIMITS_OUT_EVQ);
		encp->enc_txq_limit = MIN(EFX_TXQ_LIMIT_TARGET,
		    MCDI_OUT_DWORD(req, GET_RESOURCE_LIMITS_OUT_TXQ));
		encp->enc_rxq_limit = MIN(EFX_RXQ_LIMIT_TARGET,
		    MCDI_OUT_DWORD(req, GET_RESOURCE_LIMITS_OUT_RXQ));
	} else if (req.emr_rc == ENOTSUP) {
		encp->enc_evq_limit = 1024;
		encp->enc_txq_limit = EFX_TXQ_LIMIT_TARGET;
		encp->enc_rxq_limit = EFX_RXQ_LIMIT_TARGET;
	} else {
		rc = req.emr_rc;
		goto fail4;
	}

	encp->enc_buftbl_limit = SIENA_SRAM_ROWS -
	    (encp->enc_txq_limit * 16) - (encp->enc_rxq_limit * 64);

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

static	__checkReturn	int
siena_phy_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_req_t req;
	uint8_t outbuf[MC_CMD_GET_PHY_CFG_OUT_LEN];
	int rc;

	req.emr_cmd = MC_CMD_GET_PHY_CFG;
	EFX_STATIC_ASSERT(MC_CMD_GET_PHY_CFG_IN_LEN == 0);
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof (outbuf);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_PHY_CFG_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	encp->enc_phy_type = MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_TYPE);
#if EFSYS_OPT_NAMES
	(void) strncpy(encp->enc_phy_name,
		MCDI_OUT2(req, char, GET_PHY_CFG_OUT_NAME),
		MIN(sizeof (encp->enc_phy_name) - 1,
		    MC_CMD_GET_PHY_CFG_OUT_NAME_LEN));
#endif	/* EFSYS_OPT_NAMES */
	(void) memset(encp->enc_phy_revision, 0,
	    sizeof (encp->enc_phy_revision));
	memcpy(encp->enc_phy_revision,
		MCDI_OUT2(req, char, GET_PHY_CFG_OUT_REVISION),
		MIN(sizeof (encp->enc_phy_revision) - 1,
		    MC_CMD_GET_PHY_CFG_OUT_REVISION_LEN));
#if EFSYS_OPT_PHY_LED_CONTROL
	encp->enc_led_mask = ((1 << EFX_PHY_LED_DEFAULT) |
			    (1 << EFX_PHY_LED_OFF) |
			    (1 << EFX_PHY_LED_ON));
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

#if EFSYS_OPT_PHY_PROPS
	encp->enc_phy_nprops  = 0;
#endif	/* EFSYS_OPT_PHY_PROPS */

	/* Get the media type of the fixed port, if recognised. */
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_XAUI == EFX_PHY_MEDIA_XAUI);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_CX4 == EFX_PHY_MEDIA_CX4);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_KX4 == EFX_PHY_MEDIA_KX4);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_XFP == EFX_PHY_MEDIA_XFP);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_SFP_PLUS == EFX_PHY_MEDIA_SFP_PLUS);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_BASE_T == EFX_PHY_MEDIA_BASE_T);
	epp->ep_fixed_port_type =
		MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_MEDIA_TYPE);
	if (epp->ep_fixed_port_type >= EFX_PHY_MEDIA_NTYPES)
		epp->ep_fixed_port_type = EFX_PHY_MEDIA_INVALID;

	epp->ep_phy_cap_mask =
		MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_SUPPORTED_CAP);
#if EFSYS_OPT_PHY_FLAGS
	encp->enc_phy_flags_mask = MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_FLAGS);
#endif	/* EFSYS_OPT_PHY_FLAGS */

	encp->enc_port = (uint8_t)MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_PRT);

	/* Populate internal state */
	encp->enc_siena_channel =
		(uint8_t)MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_CHANNEL);

#if EFSYS_OPT_PHY_STATS
	encp->enc_siena_phy_stat_mask =
		MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_STATS_MASK);

	/* Convert the MCDI statistic mask into the EFX_PHY_STAT mask */
	siena_phy_decode_stats(enp, encp->enc_siena_phy_stat_mask,
			    NULL, &encp->enc_phy_stat_mask, NULL);
#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_BIST
	encp->enc_bist_mask = 0;
	if (MCDI_OUT_DWORD_FIELD(req, GET_PHY_CFG_OUT_FLAGS,
	    GET_PHY_CFG_OUT_BIST_CABLE_SHORT))
		encp->enc_bist_mask |= (1 << EFX_PHY_BIST_TYPE_CABLE_SHORT);
	if (MCDI_OUT_DWORD_FIELD(req, GET_PHY_CFG_OUT_FLAGS,
	    GET_PHY_CFG_OUT_BIST_CABLE_LONG))
		encp->enc_bist_mask |= (1 << EFX_PHY_BIST_TYPE_CABLE_LONG);
	if (MCDI_OUT_DWORD_FIELD(req, GET_PHY_CFG_OUT_FLAGS,
	    GET_PHY_CFG_OUT_BIST))
		encp->enc_bist_mask |= (1 << EFX_PHY_BIST_TYPE_NORMAL);
#endif	/* EFSYS_OPT_BIST */

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_LOOPBACK

static	__checkReturn	int
siena_loopback_cfg(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_req_t req;
	uint8_t outbuf[MC_CMD_GET_LOOPBACK_MODES_OUT_LEN];
	int rc;

	req.emr_cmd = MC_CMD_GET_LOOPBACK_MODES;
	EFX_STATIC_ASSERT(MC_CMD_GET_LOOPBACK_MODES_IN_LEN == 0);
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof (outbuf);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_LOOPBACK_MODES_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	/*
	 * We assert the MC_CMD_LOOPBACK and EFX_LOOPBACK namespaces agree
	 * in siena_phy.c:siena_phy_get_link()
	 */
	encp->enc_loopback_types[EFX_LINK_100FDX] = EFX_LOOPBACK_MASK &
	    MCDI_OUT_DWORD(req, GET_LOOPBACK_MODES_OUT_100M) &
	    MCDI_OUT_DWORD(req, GET_LOOPBACK_MODES_OUT_SUGGESTED);
	encp->enc_loopback_types[EFX_LINK_1000FDX] = EFX_LOOPBACK_MASK &
	    MCDI_OUT_DWORD(req, GET_LOOPBACK_MODES_OUT_1G) &
	    MCDI_OUT_DWORD(req, GET_LOOPBACK_MODES_OUT_SUGGESTED);
	encp->enc_loopback_types[EFX_LINK_10000FDX] = EFX_LOOPBACK_MASK &
	    MCDI_OUT_DWORD(req, GET_LOOPBACK_MODES_OUT_10G) &
	    MCDI_OUT_DWORD(req, GET_LOOPBACK_MODES_OUT_SUGGESTED);
	encp->enc_loopback_types[EFX_LINK_UNKNOWN] =
	    (1 << EFX_LOOPBACK_OFF) |
	    encp->enc_loopback_types[EFX_LINK_100FDX] |
	    encp->enc_loopback_types[EFX_LINK_1000FDX] |
	    encp->enc_loopback_types[EFX_LINK_10000FDX];

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_LOOPBACK */

#if EFSYS_OPT_MON_STATS

static	__checkReturn	int
siena_monitor_cfg(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_req_t req;
	uint8_t outbuf[MCDI_CTL_SDU_LEN_MAX];
	int rc;

	req.emr_cmd = MC_CMD_SENSOR_INFO;
	EFX_STATIC_ASSERT(MC_CMD_SENSOR_INFO_IN_LEN == 0);
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof (outbuf);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_SENSOR_INFO_OUT_MASK_OFST + 4) {
		rc = EMSGSIZE;
		goto fail2;
	}

	encp->enc_siena_mon_stat_mask =
		MCDI_OUT_DWORD(req, SENSOR_INFO_OUT_MASK);
	encp->enc_mon_type = EFX_MON_SFC90X0;

	siena_mon_decode_stats(enp, encp->enc_siena_mon_stat_mask,
			    NULL, &(encp->enc_mon_stat_mask), NULL);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_MON_STATS */

	__checkReturn	int
siena_nic_probe(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	siena_link_state_t sls;
	unsigned int mask;
	int rc;

	mask = 0;	/* XXX: pacify gcc */
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	/* Read clear any assertion state */
	if ((rc = siena_nic_read_assertion(enp)) != 0)
		goto fail1;

	/* Exit the assertion handler */
	if ((rc = siena_nic_exit_assertion_handler(enp)) != 0)
		goto fail2;

	/* Wrestle control from the BMC */
	if ((rc = siena_nic_attach(enp, B_TRUE)) != 0)
		goto fail3;

	if ((rc = siena_board_cfg(enp)) != 0)
		goto fail4;

	encp->enc_evq_moderation_max =
		EFX_EV_TIMER_QUANTUM << FRF_CZ_TIMER_VAL_WIDTH;

	if ((rc = siena_phy_cfg(enp)) != 0)
		goto fail5;

	/* Obtain the default PHY advertised capabilities */
	if ((rc = siena_nic_reset(enp)) != 0)
		goto fail6;
	if ((rc = siena_phy_get_link(enp, &sls)) != 0)
		goto fail7;
	epp->ep_default_adv_cap_mask = sls.sls_adv_cap_mask;
	epp->ep_adv_cap_mask = sls.sls_adv_cap_mask;

#if EFSYS_OPT_VPD || EFSYS_OPT_NVRAM
	if ((rc = siena_nic_get_partn_mask(enp, &mask)) != 0)
		goto fail8;
	enp->en_u.siena.enu_partn_mask = mask;
#endif

#if EFSYS_OPT_MAC_STATS
	/* Wipe the MAC statistics */
	if ((rc = siena_mac_stats_clear(enp)) != 0)
		goto fail9;
#endif

#if EFSYS_OPT_LOOPBACK
	if ((rc = siena_loopback_cfg(enp)) != 0)
		goto fail10;
#endif

#if EFSYS_OPT_MON_STATS
	if ((rc = siena_monitor_cfg(enp)) != 0)
		goto fail11;
#endif

	encp->enc_features = enp->en_features;

	return (0);

#if EFSYS_OPT_MON_STATS
fail11:
	EFSYS_PROBE(fail11);
#endif
#if EFSYS_OPT_LOOPBACK
fail10:
	EFSYS_PROBE(fail10);
#endif
#if EFSYS_OPT_MAC_STATS
fail9:
	EFSYS_PROBE(fail9);
#endif
#if EFSYS_OPT_VPD || EFSYS_OPT_NVRAM
fail8:
	EFSYS_PROBE(fail8);
#endif
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
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
siena_nic_reset(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	/* siena_nic_reset() is called to recover from BADASSERT failures. */
	if ((rc = siena_nic_read_assertion(enp)) != 0)
		goto fail1;
	if ((rc = siena_nic_exit_assertion_handler(enp)) != 0)
		goto fail2;

	req.emr_cmd = MC_CMD_PORT_RESET;
	EFX_STATIC_ASSERT(MC_CMD_PORT_RESET_IN_LEN == 0);
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	EFX_STATIC_ASSERT(MC_CMD_PORT_RESET_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (0);
}

static	__checkReturn	int
siena_nic_logging(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_LOG_CTRL_IN_LEN];
	int rc;

	req.emr_cmd = MC_CMD_LOG_CTRL;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_LOG_CTRL_IN_LEN;
	EFX_STATIC_ASSERT(MC_CMD_LOG_CTRL_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, LOG_CTRL_IN_LOG_DEST,
		    MC_CMD_LOG_CTRL_IN_LOG_DEST_EVQ);
	MCDI_IN_SET_DWORD(req, LOG_CTRL_IN_LOG_DEST_EVQ, 0);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static			void
siena_nic_rx_cfg(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;

	/*
	 * RX_INGR_EN is always enabled on Siena, because we rely on
	 * the RX parser to be resiliant to missing SOP/EOP.
	 */
	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_INGR_EN, 1);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);

	/* Disable parsing of additional 802.1Q in Q packets */
	EFX_BAR_READO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_CZ_RX_FILTER_ALL_VLAN_ETHERTYPES, 0);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
}

static			void
siena_nic_usrev_dis(
	__in		efx_nic_t *enp)
{
	efx_oword_t	oword;

	EFX_POPULATE_OWORD_1(oword, FRF_CZ_USREV_DIS, 1);
	EFX_BAR_WRITEO(enp, FR_CZ_USR_EV_CFG, &oword);
}

	__checkReturn	int
siena_nic_init(
	__in		efx_nic_t *enp)
{
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	if ((rc = siena_nic_logging(enp)) != 0)
		goto fail1;

	siena_sram_init(enp);

	/* Configure Siena's RX block */
	siena_nic_rx_cfg(enp);

	/* Disable USR_EVents for now */
	siena_nic_usrev_dis(enp);

	/* bug17057: Ensure set_link is called */
	if ((rc = siena_phy_reconfigure(enp)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

			void
siena_nic_fini(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

			void
siena_nic_unprobe(
	__in		efx_nic_t *enp)
{
	(void) siena_nic_attach(enp, B_FALSE);
}

#if EFSYS_OPT_DIAG

static efx_register_set_t __cs	__siena_registers[] = {
	{ FR_AZ_ADR_REGION_REG_OFST, 0, 1 },
	{ FR_CZ_USR_EV_CFG_OFST, 0, 1 },
	{ FR_AZ_RX_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_TX_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_TX_RESERVED_REG_OFST, 0, 1 },
	{ FR_AZ_SRM_TX_DC_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_RX_DC_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_RX_DC_PF_WM_REG_OFST, 0, 1 },
	{ FR_AZ_DP_CTRL_REG_OFST, 0, 1 },
	{ FR_BZ_RX_RSS_TKEY_REG_OFST, 0, 1},
	{ FR_CZ_RX_RSS_IPV6_REG1_OFST, 0, 1},
	{ FR_CZ_RX_RSS_IPV6_REG2_OFST, 0, 1},
	{ FR_CZ_RX_RSS_IPV6_REG3_OFST, 0, 1}
};

static const uint32_t __cs	__siena_register_masks[] = {
	0x0003FFFF, 0x0003FFFF, 0x0003FFFF, 0x0003FFFF,
	0x000103FF, 0x00000000, 0x00000000, 0x00000000,
	0xFFFFFFFE, 0xFFFFFFFF, 0x0003FFFF, 0x00000000,
	0x7FFF0037, 0xFFFF8000, 0xFFFFFFFF, 0x03FFFFFF,
	0xFFFEFE80, 0x1FFFFFFF, 0x020000FE, 0x007FFFFF,
	0x001FFFFF, 0x00000000, 0x00000000, 0x00000000,
	0x00000003, 0x00000000, 0x00000000, 0x00000000,
	0x000003FF, 0x00000000, 0x00000000, 0x00000000,
	0x00000FFF, 0x00000000, 0x00000000, 0x00000000,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0x00000007, 0x00000000
};

static efx_register_set_t __cs	__siena_tables[] = {
	{ FR_AZ_RX_FILTER_TBL0_OFST, FR_AZ_RX_FILTER_TBL0_STEP,
	    FR_AZ_RX_FILTER_TBL0_ROWS },
	{ FR_CZ_RX_MAC_FILTER_TBL0_OFST, FR_CZ_RX_MAC_FILTER_TBL0_STEP,
	    FR_CZ_RX_MAC_FILTER_TBL0_ROWS },
	{ FR_AZ_RX_DESC_PTR_TBL_OFST,
	    FR_AZ_RX_DESC_PTR_TBL_STEP, FR_CZ_RX_DESC_PTR_TBL_ROWS },
	{ FR_AZ_TX_DESC_PTR_TBL_OFST,
	    FR_AZ_TX_DESC_PTR_TBL_STEP, FR_CZ_TX_DESC_PTR_TBL_ROWS },
	{ FR_AZ_TIMER_TBL_OFST, FR_AZ_TIMER_TBL_STEP, FR_CZ_TIMER_TBL_ROWS },
	{ FR_CZ_TX_FILTER_TBL0_OFST,
	    FR_CZ_TX_FILTER_TBL0_STEP, FR_CZ_TX_FILTER_TBL0_ROWS },
	{ FR_CZ_TX_MAC_FILTER_TBL0_OFST,
	    FR_CZ_TX_MAC_FILTER_TBL0_STEP, FR_CZ_TX_MAC_FILTER_TBL0_ROWS }
};

static const uint32_t __cs	__siena_table_masks[] = {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x000003FF,
	0xFFFF0FFF, 0xFFFFFFFF, 0x00000E7F, 0x00000000,
	0xFFFFFFFF, 0x0FFFFFFF, 0x01800000, 0x00000000,
	0xFFFFFFFE, 0x0FFFFFFF, 0x0C000000, 0x00000000,
	0x3FFFFFFF, 0x00000000, 0x00000000, 0x00000000,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x000013FF,
	0xFFFF07FF, 0xFFFFFFFF, 0x0000007F, 0x00000000,
};

	__checkReturn	int
siena_nic_register_test(
	__in		efx_nic_t *enp)
{
	efx_register_set_t *rsp;
	const uint32_t *dwordp;
	unsigned int nitems;
	unsigned int count;
	int rc;

	/* Fill out the register mask entries */
	EFX_STATIC_ASSERT(EFX_ARRAY_SIZE(__siena_register_masks)
		    == EFX_ARRAY_SIZE(__siena_registers) * 4);

	nitems = EFX_ARRAY_SIZE(__siena_registers);
	dwordp = __siena_register_masks;
	for (count = 0; count < nitems; ++count) {
		rsp = __siena_registers + count;
		rsp->mask.eo_u32[0] = *dwordp++;
		rsp->mask.eo_u32[1] = *dwordp++;
		rsp->mask.eo_u32[2] = *dwordp++;
		rsp->mask.eo_u32[3] = *dwordp++;
	}

	/* Fill out the register table entries */
	EFX_STATIC_ASSERT(EFX_ARRAY_SIZE(__siena_table_masks)
		    == EFX_ARRAY_SIZE(__siena_tables) * 4);

	nitems = EFX_ARRAY_SIZE(__siena_tables);
	dwordp = __siena_table_masks;
	for (count = 0; count < nitems; ++count) {
		rsp = __siena_tables + count;
		rsp->mask.eo_u32[0] = *dwordp++;
		rsp->mask.eo_u32[1] = *dwordp++;
		rsp->mask.eo_u32[2] = *dwordp++;
		rsp->mask.eo_u32[3] = *dwordp++;
	}

	if ((rc = efx_nic_test_registers(enp, __siena_registers,
	    EFX_ARRAY_SIZE(__siena_registers))) != 0)
		goto fail1;

	if ((rc = efx_nic_test_tables(enp, __siena_tables,
	    EFX_PATTERN_BYTE_ALTERNATE,
	    EFX_ARRAY_SIZE(__siena_tables))) != 0)
		goto fail2;

	if ((rc = efx_nic_test_tables(enp, __siena_tables,
	    EFX_PATTERN_BYTE_CHANGING,
	    EFX_ARRAY_SIZE(__siena_tables))) != 0)
		goto fail3;

	if ((rc = efx_nic_test_tables(enp, __siena_tables,
	    EFX_PATTERN_BIT_SWEEP, EFX_ARRAY_SIZE(__siena_tables))) != 0)
		goto fail4;

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

#endif	/* EFSYS_OPT_DIAG */

#endif	/* EFSYS_OPT_SIENA */
