/*-
 * Copyright 2007-2009 Solarflare Communications Inc.  All rights reserved.
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
#include "efx_impl.h"

#if EFSYS_OPT_MAC_FALCON_GMAC
#include "falcon_gmac.h"
#endif

#if EFSYS_OPT_MAC_FALCON_XMAC
#include "falcon_xmac.h"
#endif

#if EFSYS_OPT_MAC_FALCON_GMAC
static efx_mac_ops_t	__cs __efx_falcon_gmac_ops = {
	falcon_gmac_reset,		/* emo_reset */
	falcon_mac_poll,		/* emo_poll */
	falcon_mac_up,			/* emo_up */
	falcon_gmac_reconfigure,	/* emo_reconfigure */
#if EFSYS_OPT_LOOPBACK
	falcon_mac_loopback_set,	/* emo_loopback_set */
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_MAC_STATS
	falcon_mac_stats_upload,	/* emo_stats_upload */
	NULL,				/* emo_stats_periodic */
	falcon_gmac_stats_update	/* emo_stats_update */
#endif	/* EFSYS_OPT_MAC_STATS */
};
#endif	/* EFSYS_OPT_MAC_FALCON_GMAC */

#if EFSYS_OPT_MAC_FALCON_XMAC
static efx_mac_ops_t	__cs __efx_falcon_xmac_ops = {
	falcon_xmac_reset,		/* emo_reset */
	falcon_mac_poll,		/* emo_poll */
	falcon_mac_up,			/* emo_up */
	falcon_xmac_reconfigure,	/* emo_reconfigure */
#if EFSYS_OPT_LOOPBACK
	falcon_mac_loopback_set,	/* emo_loopback_set */
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_MAC_STATS
	falcon_mac_stats_upload,	/* emo_stats_upload */
	NULL,				/* emo_stats_periodic */
	falcon_xmac_stats_update	/* emo_stats_update */
#endif	/* EFSYS_OPT_MAC_STATS */
};
#endif	/* EFSYS_OPT_MAC_FALCON_XMAC */

#if EFSYS_OPT_SIENA
static efx_mac_ops_t	__cs __efx_siena_mac_ops = {
	NULL,				/* emo_reset */
	siena_mac_poll,			/* emo_poll */
	siena_mac_up,			/* emo_up */
	siena_mac_reconfigure,		/* emo_reconfigure */
#if EFSYS_OPT_LOOPBACK
	siena_mac_loopback_set,		/* emo_loopback_set */
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_MAC_STATS
	siena_mac_stats_upload,		/* emo_stats_upload */
	siena_mac_stats_periodic,	/* emo_stats_periodic */
	siena_mac_stats_update		/* emo_stats_update */
#endif	/* EFSYS_OPT_MAC_STATS */
};
#endif	/* EFSYS_OPT_SIENA */

static efx_mac_ops_t	__cs * __cs __efx_mac_ops[] = {
	NULL,
#if EFSYS_OPT_MAC_FALCON_GMAC
	&__efx_falcon_gmac_ops,
#else
	NULL,
#endif	/* EFSYS_OPT_MAC_FALCON_GMAC */
#if EFSYS_OPT_MAC_FALCON_XMAC
	&__efx_falcon_xmac_ops,
#else
	NULL,
#endif	/* EFSYS_OPT_MAC_FALCON_XMAC */
#if EFSYS_OPT_SIENA
	&__efx_siena_mac_ops,
#else
	NULL,
#endif	/* EFSYS_OPT_SIENA */
};

	__checkReturn			int
efx_mac_pdu_set(
	__in				efx_nic_t *enp,
	__in				size_t pdu)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	uint32_t old_pdu;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);
	EFSYS_ASSERT(emop != NULL);

	if (pdu < EFX_MAC_PDU_MIN) {
		rc = EINVAL;
		goto fail1;
	}

	if (pdu > EFX_MAC_PDU_MAX) {
		rc = EINVAL;
		goto fail2;
	}

	old_pdu = epp->ep_mac_pdu;
	epp->ep_mac_pdu = (uint32_t)pdu;
	if ((rc = emop->emo_reconfigure(enp)) != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);

	epp->ep_mac_pdu = old_pdu;

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn			int
efx_mac_addr_set(
	__in				efx_nic_t *enp,
	__in				uint8_t *addr)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	uint8_t old_addr[6];
	uint32_t oui;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	if (addr[0] & 0x01) {
		rc = EINVAL;
		goto fail1;
	}

	oui = addr[0] << 16 | addr[1] << 8 | addr[2];
	if (oui == 0x000000) {
		rc = EINVAL;
		goto fail2;
	}

	EFX_MAC_ADDR_COPY(old_addr, epp->ep_mac_addr);
	EFX_MAC_ADDR_COPY(epp->ep_mac_addr, addr);
	if ((rc = emop->emo_reconfigure(enp)) != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);

	EFX_MAC_ADDR_COPY(epp->ep_mac_addr, old_addr);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn			int
efx_mac_filter_set(
	__in				efx_nic_t *enp,
	__in				boolean_t unicst,
	__in				boolean_t brdcst)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	boolean_t old_unicst;
	boolean_t old_brdcst;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	old_unicst = unicst;
	old_brdcst = brdcst;

	epp->ep_unicst = unicst;
	epp->ep_brdcst = brdcst;

	if ((rc = emop->emo_reconfigure(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	epp->ep_unicst = old_unicst;
	epp->ep_brdcst = old_brdcst;

	return (rc);
}

	__checkReturn			int
efx_mac_drain(
	__in				efx_nic_t *enp,
	__in				boolean_t enabled)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);
	EFSYS_ASSERT(emop != NULL);

	if (epp->ep_mac_drain == enabled)
		return (0);

	epp->ep_mac_drain = enabled;

	if (enabled && emop->emo_reset != NULL) {
		if ((rc = emop->emo_reset(enp)) != 0)
			goto fail1;

		EFSYS_ASSERT(enp->en_reset_flags & EFX_RESET_MAC);
		enp->en_reset_flags &= ~EFX_RESET_PHY;
	}

	if ((rc = emop->emo_reconfigure(enp)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
efx_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	if ((rc = emop->emo_up(enp, mac_upp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn			int
efx_mac_fcntl_set(
	__in				efx_nic_t *enp,
	__in				unsigned int fcntl,
	__in				boolean_t autoneg)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	efx_phy_ops_t *epop = epp->ep_epop;
	unsigned int old_fcntl;
	boolean_t old_autoneg;
	unsigned int old_adv_cap;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	if ((fcntl & ~(EFX_FCNTL_RESPOND | EFX_FCNTL_GENERATE)) != 0) {
		rc = EINVAL;
		goto fail1;
	}

	/*
	 * Ignore a request to set flow control autonegotiation
	 * if the PHY doesn't support it.
	 */
	if (~epp->ep_phy_cap_mask & (1 << EFX_PHY_CAP_AN))
		autoneg = B_FALSE;

	old_fcntl = epp->ep_fcntl;
	old_autoneg = autoneg;
	old_adv_cap = epp->ep_adv_cap_mask;

	epp->ep_fcntl = fcntl;
	epp->ep_fcntl_autoneg = autoneg;

	/*
	 * If the PHY supports autonegotiation, then encode the flow control
	 * settings in the advertised capabilities, and restart AN. Otherwise,
	 * just push the new settings directly to the MAC.
	 */
	if (epp->ep_phy_cap_mask & (1 << EFX_PHY_CAP_AN)) {
		if (fcntl & EFX_FCNTL_RESPOND)
			epp->ep_adv_cap_mask |=    (1 << EFX_PHY_CAP_PAUSE |
						    1 << EFX_PHY_CAP_ASYM);
		else
			epp->ep_adv_cap_mask &=   ~(1 << EFX_PHY_CAP_PAUSE |
						    1 << EFX_PHY_CAP_ASYM);

		if (fcntl & EFX_FCNTL_GENERATE)
			epp->ep_adv_cap_mask ^= (1 << EFX_PHY_CAP_ASYM);

		if ((rc = epop->epo_reconfigure(enp)) != 0)
			goto fail2;

	} else {
		if ((rc = emop->emo_reconfigure(enp)) != 0)
			goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);

	epp->ep_fcntl = old_fcntl;
	epp->ep_fcntl_autoneg = old_autoneg;
	epp->ep_adv_cap_mask = old_adv_cap;

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

			void
efx_mac_fcntl_get(
	__in		efx_nic_t *enp,
	__out		unsigned int *fcntl_wantedp,
	__out		unsigned int *fcntl_linkp)
{
	efx_port_t *epp = &(enp->en_port);
	unsigned int wanted;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	/*
	 * If the PHY supports auto negotiation, then the requested flow
	 * control settings are encoded in the advertised capabilities.
	 */
	if (epp->ep_phy_cap_mask & (1 << EFX_PHY_CAP_AN)) {
		wanted = 0;

		if (epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_PAUSE))
			wanted = EFX_FCNTL_RESPOND | EFX_FCNTL_GENERATE;
		if (epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_ASYM))
			wanted ^= EFX_FCNTL_GENERATE;
	} else
		wanted = epp->ep_fcntl;

	*fcntl_linkp = epp->ep_fcntl;
	*fcntl_wantedp = wanted;
}

	__checkReturn			int
efx_mac_hash_set(
	__in				efx_nic_t *enp,
	__in_ecount(EFX_MAC_HASH_BITS)	unsigned int const *bucket)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	efx_oword_t old_hash[2];
	unsigned int index;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	memcpy(old_hash, epp->ep_multicst_hash, sizeof (old_hash));

	/* Set the lower 128 bits of the hash */
	EFX_ZERO_OWORD(epp->ep_multicst_hash[0]);
	for (index = 0; index < 128; index++) {
		if (bucket[index] != 0)
			EFX_SET_OWORD_BIT(epp->ep_multicst_hash[0], index);
	}

	/* Set the upper 128 bits of the hash */
	EFX_ZERO_OWORD(epp->ep_multicst_hash[1]);
	for (index = 0; index < 128; index++) {
		if (bucket[index + 128] != 0)
			EFX_SET_OWORD_BIT(epp->ep_multicst_hash[1], index);
	}

	if ((rc = emop->emo_reconfigure(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	memcpy(epp->ep_multicst_hash, old_hash, sizeof (old_hash));

	return (rc);
}

#if EFSYS_OPT_MAC_STATS

#if EFSYS_OPT_NAMES

/* START MKCONFIG GENERATED EfxMacStatNamesBlock adf707adba80813e */
static const char 	__cs * __cs __efx_mac_stat_name[] = {
	"rx_octets",
	"rx_pkts",
	"rx_unicst_pkts",
	"rx_multicst_pkts",
	"rx_brdcst_pkts",
	"rx_pause_pkts",
	"rx_le_64_pkts",
	"rx_65_to_127_pkts",
	"rx_128_to_255_pkts",
	"rx_256_to_511_pkts",
	"rx_512_to_1023_pkts",
	"rx_1024_to_15xx_pkts",
	"rx_ge_15xx_pkts",
	"rx_errors",
	"rx_fcs_errors",
	"rx_drop_events",
	"rx_false_carrier_errors",
	"rx_symbol_errors",
	"rx_align_errors",
	"rx_internal_errors",
	"rx_jabber_pkts",
	"rx_lane0_char_err",
	"rx_lane1_char_err",
	"rx_lane2_char_err",
	"rx_lane3_char_err",
	"rx_lane0_disp_err",
	"rx_lane1_disp_err",
	"rx_lane2_disp_err",
	"rx_lane3_disp_err",
	"rx_match_fault",
	"rx_nodesc_drop_cnt",
	"tx_octets",
	"tx_pkts",
	"tx_unicst_pkts",
	"tx_multicst_pkts",
	"tx_brdcst_pkts",
	"tx_pause_pkts",
	"tx_le_64_pkts",
	"tx_65_to_127_pkts",
	"tx_128_to_255_pkts",
	"tx_256_to_511_pkts",
	"tx_512_to_1023_pkts",
	"tx_1024_to_15xx_pkts",
	"tx_ge_15xx_pkts",
	"tx_errors",
	"tx_sgl_col_pkts",
	"tx_mult_col_pkts",
	"tx_ex_col_pkts",
	"tx_late_col_pkts",
	"tx_def_pkts",
	"tx_ex_def_pkts",
};
/* END MKCONFIG GENERATED EfxMacStatNamesBlock */

	__checkReturn			const char __cs *
efx_mac_stat_name(
	__in				efx_nic_t *enp,
	__in				unsigned int id)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	EFSYS_ASSERT3U(id, <, EFX_MAC_NSTATS);
	return (__efx_mac_stat_name[id]);
}

#endif	/* EFSYS_OPT_STAT_NAME */

	__checkReturn			int
efx_mac_stats_upload(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);
	EFSYS_ASSERT(emop != NULL);

	/*
	 * Don't assert !ep_mac_stats_pending, because the client might
	 * have failed to finalise statistics when previously stopping
	 * the port.
	 */
	if ((rc = emop->emo_stats_upload(enp, esmp)) != 0)
		goto fail1;

	epp->ep_mac_stats_pending = B_TRUE;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn			int
efx_mac_stats_periodic(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__in				uint16_t period_ms,
	__in				boolean_t events)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	EFSYS_ASSERT(emop != NULL);

	if (emop->emo_stats_periodic == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = emop->emo_stats_periodic(enp, esmp, period_ms, events)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}


	__checkReturn			int
efx_mac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MAC_NSTATS)	efsys_stat_t *essp,
	__in				uint32_t *generationp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_ops_t *emop = epp->ep_emop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);
	EFSYS_ASSERT(emop != NULL);

	rc = emop->emo_stats_update(enp, esmp, essp, generationp);
	if (rc == 0)
		epp->ep_mac_stats_pending = B_FALSE;

	return (rc);
}

#endif	/* EFSYS_OPT_MAC_STATS */

	__checkReturn			int
efx_mac_select(
	__in				efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mac_type_t type = EFX_MAC_INVALID;
	efx_mac_ops_t *emop;
	int rc = EINVAL;

#if EFSYS_OPT_SIENA
	if (enp->en_family == EFX_FAMILY_SIENA) {
		type = EFX_MAC_SIENA;
		goto chosen;
	}
#endif

#if EFSYS_OPT_FALCON
	switch (epp->ep_link_mode) {
#if EFSYS_OPT_MAC_FALCON_GMAC
	case EFX_LINK_100HDX:
	case EFX_LINK_100FDX:
	case EFX_LINK_1000HDX:
	case EFX_LINK_1000FDX:
		type = EFX_MAC_FALCON_GMAC;
		goto chosen;
#endif	/* EFSYS_OPT_FALCON_GMAC */

#if EFSYS_OPT_MAC_FALCON_XMAC
	case EFX_LINK_10000FDX:
		type = EFX_MAC_FALCON_XMAC;
		goto chosen;
#endif	/* EFSYS_OPT_FALCON_XMAC */

	default:
#if EFSYS_OPT_MAC_FALCON_GMAC && EFSYS_OPT_MAC_FALCON_XMAC
		/* Only initialise a MAC supported by the PHY */
		if (epp->ep_phy_cap_mask &
		    ((1 << EFX_PHY_CAP_1000FDX) |
		    (1 << EFX_PHY_CAP_1000HDX) |
		    (1 << EFX_PHY_CAP_100FDX) |
		    (1 << EFX_PHY_CAP_100HDX) |
		    (1 << EFX_PHY_CAP_10FDX) |
		    (1 << EFX_PHY_CAP_10FDX)))
			type = EFX_MAC_FALCON_GMAC;
		else
			type = EFX_MAC_FALCON_XMAC;
#elif EFSYS_OPT_MAC_FALCON_GMAC
		type = EFX_MAC_FALCON_GMAC;
#else
		type = EFX_MAC_FALCON_XMAC;
#endif
		goto chosen;
	}
#endif	/* EFSYS_OPT_FALCON */

chosen:
	EFSYS_ASSERT(type != EFX_MAC_INVALID);
	EFSYS_ASSERT3U(type, <, EFX_MAC_NTYPES);
	emop = epp->ep_emop = (efx_mac_ops_t *)__efx_mac_ops[type];
	EFSYS_ASSERT(emop != NULL);

	epp->ep_mac_type = type;
	
	if (emop->emo_reset != NULL) {
		if ((rc = emop->emo_reset(enp)) != 0)
			goto fail1;
		
		EFSYS_ASSERT(enp->en_reset_flags & EFX_RESET_MAC);
		enp->en_reset_flags &= ~EFX_RESET_MAC;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}
