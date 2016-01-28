/*-
 * Copyright (c) 2007-2015 Solarflare Communications Inc.
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
#if EFSYS_OPT_FALCON
#include "falcon_nvram.h"
#endif

#if EFSYS_OPT_MAC_FALCON_XMAC
#include "falcon_xmac.h"
#endif

#if EFSYS_OPT_MAC_FALCON_GMAC
#include "falcon_gmac.h"
#endif

#if EFSYS_OPT_PHY_NULL
#include "nullphy.h"
#endif

#if EFSYS_OPT_PHY_QT2022C2
#include "qt2022c2.h"
#endif

#if EFSYS_OPT_PHY_SFX7101
#include "sfx7101.h"
#endif

#if EFSYS_OPT_PHY_TXC43128
#include "txc43128.h"
#endif

#if EFSYS_OPT_PHY_SFT9001
#include "sft9001.h"
#endif

#if EFSYS_OPT_PHY_QT2025C
#include "qt2025c.h"
#endif

#if EFSYS_OPT_PHY_NULL
static efx_phy_ops_t	__efx_phy_null_ops = {
	NULL,				/* epo_power */
	nullphy_reset,			/* epo_reset */
	nullphy_reconfigure,		/* epo_reconfigure */
	nullphy_verify,			/* epo_verify */
	NULL,				/* epo_uplink_check */
	nullphy_downlink_check,		/* epo_downlink_check */
	nullphy_oui_get,		/* epo_oui_get */
#if EFSYS_OPT_PHY_STATS
	nullphy_stats_update,		/* epo_stats_update */
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
#if EFSYS_OPT_NAMES
	nullphy_prop_name,		/* epo_prop_name */
#endif
	nullphy_prop_get,		/* epo_prop_get */
	nullphy_prop_set,		/* epo_prop_set */
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_BIST
	NULL,				/* epo_bist_enable_offline */
	NULL,				/* epo_bist_start */
	NULL,				/* epo_bist_poll */
	NULL,				/* epo_bist_stop */
#endif	/* EFSYS_OPT_BIST */
};
#endif	/* EFSYS_OPT_PHY_NULL */

#if EFSYS_OPT_PHY_QT2022C2
static efx_phy_ops_t	__efx_phy_qt2022c2_ops = {
	NULL,				/* epo_power */
	qt2022c2_reset,			/* epo_reset */
	qt2022c2_reconfigure,		/* epo_reconfigure */
	qt2022c2_verify,		/* epo_verify */
	qt2022c2_uplink_check,		/* epo_uplink_check */
	qt2022c2_downlink_check,	/* epo_downlink_check */
	qt2022c2_oui_get,		/* epo_oui_get */
#if EFSYS_OPT_PHY_STATS
	qt2022c2_stats_update,		/* epo_stats_update */
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
#if EFSYS_OPT_NAMES
	qt2022c2_prop_name,		/* epo_prop_name */
#endif
	qt2022c2_prop_get,		/* epo_prop_get */
	qt2022c2_prop_set,		/* epo_prop_set */
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_BIST
	NULL,				/* epo_bist_enable_offline */
	NULL,				/* epo_bist_start */
	NULL,				/* epo_bist_poll */
	NULL,				/* epo_bist_stop */
#endif	/* EFSYS_OPT_BIST */
};
#endif	/* EFSYS_OPT_PHY_QT2022C2 */

#if EFSYS_OPT_PHY_SFX7101
static efx_phy_ops_t	__efx_phy_sfx7101_ops = {
	sfx7101_power,			/* epo_power */
	sfx7101_reset,			/* epo_reset */
	sfx7101_reconfigure,		/* epo_reconfigure */
	sfx7101_verify,			/* epo_verify */
	sfx7101_uplink_check,		/* epo_uplink_check */
	sfx7101_downlink_check,		/* epo_downlink_check */
	sfx7101_oui_get,		/* epo_oui_get */
#if EFSYS_OPT_PHY_STATS
	sfx7101_stats_update,		/* epo_stats_update */
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
#if EFSYS_OPT_NAMES
	sfx7101_prop_name,		/* epo_prop_name */
#endif
	sfx7101_prop_get,		/* epo_prop_get */
	sfx7101_prop_set,		/* epo_prop_set */
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_BIST
	NULL,				/* epo_bist_enable_offline */
	NULL,				/* epo_bist_start */
	NULL,				/* epo_bist_poll */
	NULL,				/* epo_bist_stop */
#endif	/* EFSYS_OPT_BIST */
};
#endif	/* EFSYS_OPT_PHY_SFX7101 */

#if EFSYS_OPT_PHY_TXC43128
static efx_phy_ops_t	__efx_phy_txc43128_ops = {
	NULL,				/* epo_power */
	txc43128_reset,			/* epo_reset */
	txc43128_reconfigure,		/* epo_reconfigure */
	txc43128_verify,		/* epo_verify */
	txc43128_uplink_check,		/* epo_uplink_check */
	txc43128_downlink_check,	/* epo_downlink_check */
	txc43128_oui_get,		/* epo_oui_get */
#if EFSYS_OPT_PHY_STATS
	txc43128_stats_update,		/* epo_stats_update */
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
#if EFSYS_OPT_NAMES
	txc43128_prop_name,		/* epo_prop_name */
#endif
	txc43128_prop_get,		/* epo_prop_get */
	txc43128_prop_set,		/* epo_prop_set */
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_BIST
	NULL,				/* epo_bist_enable_offline */
	NULL,				/* epo_bist_start */
	NULL,				/* epo_bist_poll */
	NULL,				/* epo_bist_stop */
#endif	/* EFSYS_OPT_BIST */
};
#endif	/* EFSYS_OPT_PHY_TXC43128 */

#if EFSYS_OPT_PHY_SFT9001
static efx_phy_ops_t	__efx_phy_sft9001_ops = {
	NULL,				/* epo_power */
	sft9001_reset,			/* epo_reset */
	sft9001_reconfigure,		/* epo_reconfigure */
	sft9001_verify,			/* epo_verify */
	sft9001_uplink_check,		/* epo_uplink_check */
	sft9001_downlink_check,		/* epo_downlink_check */
	sft9001_oui_get,		/* epo_oui_get */
#if EFSYS_OPT_PHY_STATS
	sft9001_stats_update,		/* epo_stats_update */
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
#if EFSYS_OPT_NAMES
	sft9001_prop_name,		/* epo_prop_name */
#endif
	sft9001_prop_get,		/* epo_prop_get */
	sft9001_prop_set,		/* epo_prop_set */
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_BIST
	NULL,				/* epo_bist_enable_offline */
	sft9001_bist_start,		/* epo_bist_start */
	sft9001_bist_poll,		/* epo_bist_poll */
	sft9001_bist_stop,		/* epo_bist_stop */
#endif	/* EFSYS_OPT_BIST */
};
#endif	/* EFSYS_OPT_PHY_SFT9001 */

#if EFSYS_OPT_PHY_QT2025C
static efx_phy_ops_t	__efx_phy_qt2025c_ops = {
	NULL,				/* epo_power */
	qt2025c_reset,			/* epo_reset */
	qt2025c_reconfigure,		/* epo_reconfigure */
	qt2025c_verify,			/* epo_verify */
	qt2025c_uplink_check,		/* epo_uplink_check */
	qt2025c_downlink_check,		/* epo_downlink_check */
	qt2025c_oui_get,		/* epo_oui_get */
#if EFSYS_OPT_PHY_STATS
	qt2025c_stats_update,		/* epo_stats_update */
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
#if EFSYS_OPT_NAMES
	qt2025c_prop_name,		/* epo_prop_name */
#endif
	qt2025c_prop_get,		/* epo_prop_get */
	qt2025c_prop_set,		/* epo_prop_set */
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_BIST
	NULL,				/* epo_bist_enable_offline */
	NULL,				/* epo_bist_start */
	NULL,				/* epo_bist_poll */
	NULL,				/* epo_bist_stop */
#endif	/* EFSYS_OPT_BIST */
};
#endif	/* EFSYS_OPT_PHY_QT2025C */

#if EFSYS_OPT_SIENA
static efx_phy_ops_t	__efx_phy_siena_ops = {
	siena_phy_power,		/* epo_power */
	NULL,				/* epo_reset */
	siena_phy_reconfigure,		/* epo_reconfigure */
	siena_phy_verify,		/* epo_verify */
	NULL,				/* epo_uplink_check */
	NULL,				/* epo_downlink_check */
	siena_phy_oui_get,		/* epo_oui_get */
#if EFSYS_OPT_PHY_STATS
	siena_phy_stats_update,		/* epo_stats_update */
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
#if EFSYS_OPT_NAMES
	siena_phy_prop_name,		/* epo_prop_name */
#endif
	siena_phy_prop_get,		/* epo_prop_get */
	siena_phy_prop_set,		/* epo_prop_set */
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_BIST
	NULL,				/* epo_bist_enable_offline */
	siena_phy_bist_start, 		/* epo_bist_start */
	siena_phy_bist_poll,		/* epo_bist_poll */
	siena_phy_bist_stop,		/* epo_bist_stop */
#endif	/* EFSYS_OPT_BIST */
};
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD
static efx_phy_ops_t	__efx_phy_ef10_ops = {
	ef10_phy_power,			/* epo_power */
	NULL,				/* epo_reset */
	ef10_phy_reconfigure,		/* epo_reconfigure */
	ef10_phy_verify,		/* epo_verify */
	NULL,				/* epo_uplink_check */
	NULL,				/* epo_downlink_check */
	ef10_phy_oui_get,		/* epo_oui_get */
#if EFSYS_OPT_PHY_STATS
	ef10_phy_stats_update,		/* epo_stats_update */
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
#if EFSYS_OPT_NAMES
	ef10_phy_prop_name,		/* epo_prop_name */
#endif
	ef10_phy_prop_get,		/* epo_prop_get */
	ef10_phy_prop_set,		/* epo_prop_set */
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_BIST
	/* FIXME: Are these BIST methods appropriate for Medford? */
	hunt_bist_enable_offline,	/* epo_bist_enable_offline */
	hunt_bist_start,		/* epo_bist_start */
	hunt_bist_poll,			/* epo_bist_poll */
	hunt_bist_stop,			/* epo_bist_stop */
#endif	/* EFSYS_OPT_BIST */
};
#endif	/* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD */

	__checkReturn	efx_rc_t
efx_phy_probe(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_phy_ops_t *epop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	epp->ep_port = encp->enc_port;
	epp->ep_phy_type = encp->enc_phy_type;

	/* Hook in operations structure */
	switch (enp->en_family) {
#if EFSYS_OPT_FALCON
	case EFX_FAMILY_FALCON:
		switch (epp->ep_phy_type) {
#if EFSYS_OPT_PHY_NULL
		case PHY_TYPE_NONE_DECODE:
			epop = (efx_phy_ops_t *)&__efx_phy_null_ops;
			break;
#endif
#if EFSYS_OPT_PHY_QT2022C2
		case PHY_TYPE_QT2022C2_DECODE:
			epop = (efx_phy_ops_t *)&__efx_phy_qt2022c2_ops;
			break;
#endif
#if EFSYS_OPT_PHY_SFX7101
		case PHY_TYPE_SFX7101_DECODE:
			epop = (efx_phy_ops_t *)&__efx_phy_sfx7101_ops;
			break;
#endif
#if EFSYS_OPT_PHY_TXC43128
		case PHY_TYPE_TXC43128_DECODE:
			epop = (efx_phy_ops_t *)&__efx_phy_txc43128_ops;
			break;
#endif
#if EFSYS_OPT_PHY_SFT9001
		case PHY_TYPE_SFT9001A_DECODE:
		case PHY_TYPE_SFT9001B_DECODE:
			epop = (efx_phy_ops_t *)&__efx_phy_sft9001_ops;
			break;
#endif
#if EFSYS_OPT_PHY_QT2025C
		case EFX_PHY_QT2025C:
			epop = (efx_phy_ops_t *)&__efx_phy_qt2025c_ops;
			break;
#endif
		default:
			rc = ENOTSUP;
			goto fail1;
		}
		break;
#endif	/* EFSYS_OPT_FALCON */
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		epop = (efx_phy_ops_t *)&__efx_phy_siena_ops;
		break;
#endif	/* EFSYS_OPT_SIENA */
#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		epop = (efx_phy_ops_t *)&__efx_phy_ef10_ops;
		break;
#endif	/* EFSYS_OPT_HUNTINGTON */
#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD:
		epop = (efx_phy_ops_t *)&__efx_phy_ef10_ops;
		break;
#endif	/* EFSYS_OPT_MEDFORD */
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	epp->ep_epop = epop;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	epp->ep_port = 0;
	epp->ep_phy_type = 0;

	return (rc);
}

	__checkReturn	efx_rc_t
efx_phy_verify(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	return (epop->epo_verify(enp));
}

#if EFSYS_OPT_PHY_LED_CONTROL

	__checkReturn	efx_rc_t
efx_phy_led_set(
	__in		efx_nic_t *enp,
	__in		efx_phy_led_mode_t mode)
{
	efx_nic_cfg_t *encp = (&enp->en_nic_cfg);
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;
	uint32_t mask;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	if (epp->ep_phy_led_mode == mode)
		goto done;

	mask = (1 << EFX_PHY_LED_DEFAULT);
	mask |= encp->enc_led_mask;

	if (!((1 << mode) & mask)) {
		rc = ENOTSUP;
		goto fail1;
	}

	EFSYS_ASSERT3U(mode, <, EFX_PHY_LED_NMODES);
	epp->ep_phy_led_mode = mode;

	if ((rc = epop->epo_reconfigure(enp)) != 0)
		goto fail2;

done:
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

			void
efx_phy_adv_cap_get(
	__in		efx_nic_t *enp,
	__in		uint32_t flag,
	__out		uint32_t *maskp)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	switch (flag) {
	case EFX_PHY_CAP_CURRENT:
		*maskp = epp->ep_adv_cap_mask;
		break;
	case EFX_PHY_CAP_DEFAULT:
		*maskp = epp->ep_default_adv_cap_mask;
		break;
	case EFX_PHY_CAP_PERM:
		*maskp = epp->ep_phy_cap_mask;
		break;
	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}
}

	__checkReturn	efx_rc_t
efx_phy_adv_cap_set(
	__in		efx_nic_t *enp,
	__in		uint32_t mask)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;
	uint32_t old_mask;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	if ((mask & ~epp->ep_phy_cap_mask) != 0) {
		rc = ENOTSUP;
		goto fail1;
	}

	if (epp->ep_adv_cap_mask == mask)
		goto done;

	old_mask = epp->ep_adv_cap_mask;
	epp->ep_adv_cap_mask = mask;

	if ((rc = epop->epo_reconfigure(enp)) != 0)
		goto fail2;

done:
	return (0);

fail2:
	EFSYS_PROBE(fail2);

	epp->ep_adv_cap_mask = old_mask;
	/* Reconfigure for robustness */
	if (epop->epo_reconfigure(enp) != 0) {
		/*
		 * We may have an inconsistent view of our advertised speed
		 * capabilities.
		 */
		EFSYS_ASSERT(0);
	}

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	void
efx_phy_lp_cap_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *maskp)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	*maskp = epp->ep_lp_cap_mask;
}

	__checkReturn	efx_rc_t
efx_phy_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	return (epop->epo_oui_get(enp, ouip));
}

			void
efx_phy_media_type_get(
	__in		efx_nic_t *enp,
	__out		efx_phy_media_type_t *typep)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	if (epp->ep_module_type != EFX_PHY_MEDIA_INVALID)
		*typep = epp->ep_module_type;
	else
		*typep = epp->ep_fixed_port_type;
}

#if EFSYS_OPT_PHY_STATS

#if EFSYS_OPT_NAMES

/* START MKCONFIG GENERATED PhyStatNamesBlock d5f79b4bc2c050fe */
static const char 	*__efx_phy_stat_name[] = {
	"oui",
	"pma_pmd_link_up",
	"pma_pmd_rx_fault",
	"pma_pmd_tx_fault",
	"pma_pmd_rev_a",
	"pma_pmd_rev_b",
	"pma_pmd_rev_c",
	"pma_pmd_rev_d",
	"pcs_link_up",
	"pcs_rx_fault",
	"pcs_tx_fault",
	"pcs_ber",
	"pcs_block_errors",
	"phy_xs_link_up",
	"phy_xs_rx_fault",
	"phy_xs_tx_fault",
	"phy_xs_align",
	"phy_xs_sync_a",
	"phy_xs_sync_b",
	"phy_xs_sync_c",
	"phy_xs_sync_d",
	"an_link_up",
	"an_master",
	"an_local_rx_ok",
	"an_remote_rx_ok",
	"cl22ext_link_up",
	"snr_a",
	"snr_b",
	"snr_c",
	"snr_d",
	"pma_pmd_signal_a",
	"pma_pmd_signal_b",
	"pma_pmd_signal_c",
	"pma_pmd_signal_d",
	"an_complete",
	"pma_pmd_rev_major",
	"pma_pmd_rev_minor",
	"pma_pmd_rev_micro",
	"pcs_fw_version_0",
	"pcs_fw_version_1",
	"pcs_fw_version_2",
	"pcs_fw_version_3",
	"pcs_fw_build_yy",
	"pcs_fw_build_mm",
	"pcs_fw_build_dd",
	"pcs_op_mode",
};

/* END MKCONFIG GENERATED PhyStatNamesBlock */

					const char *
efx_phy_stat_name(
	__in				efx_nic_t *enp,
	__in				efx_phy_stat_t type)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(type, <, EFX_PHY_NSTATS);

	return (__efx_phy_stat_name[type]);
}

#endif	/* EFSYS_OPT_NAMES */

	__checkReturn			efx_rc_t
efx_phy_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	return (epop->epo_stats_update(enp, esmp, stat));
}

#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES
		const char *
efx_phy_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	return (epop->epo_prop_name(enp, id));
}
#endif	/* EFSYS_OPT_NAMES */

	__checkReturn	efx_rc_t
efx_phy_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	return (epop->epo_prop_get(enp, id, flags, valp));
}

	__checkReturn	efx_rc_t
efx_phy_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	return (epop->epo_prop_set(enp, id, val));
}
#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_BIST

	__checkReturn		efx_rc_t
efx_bist_enable_offline(
	__in			efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	if (epop->epo_bist_enable_offline == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = epop->epo_bist_enable_offline(enp)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);

}

	__checkReturn		efx_rc_t
efx_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	EFSYS_ASSERT3U(type, !=, EFX_BIST_TYPE_UNKNOWN);
	EFSYS_ASSERT3U(type, <, EFX_BIST_TYPE_NTYPES);
	EFSYS_ASSERT3U(epp->ep_current_bist, ==, EFX_BIST_TYPE_UNKNOWN);

	if (epop->epo_bist_start == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = epop->epo_bist_start(enp, type)) != 0)
		goto fail2;

	epp->ep_current_bist = type;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_bist_poll(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type,
	__out			efx_bist_result_t *resultp,
	__out_opt		uint32_t *value_maskp,
	__out_ecount_opt(count)	unsigned long *valuesp,
	__in			size_t count)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	EFSYS_ASSERT3U(type, !=, EFX_BIST_TYPE_UNKNOWN);
	EFSYS_ASSERT3U(type, <, EFX_BIST_TYPE_NTYPES);
	EFSYS_ASSERT3U(epp->ep_current_bist, ==, type);

	EFSYS_ASSERT(epop->epo_bist_poll != NULL);
	if (epop->epo_bist_poll == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = epop->epo_bist_poll(enp, type, resultp, value_maskp,
	    valuesp, count)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
efx_bist_stop(
	__in		efx_nic_t *enp,
	__in		efx_bist_type_t type)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	EFSYS_ASSERT3U(type, !=, EFX_BIST_TYPE_UNKNOWN);
	EFSYS_ASSERT3U(type, <, EFX_BIST_TYPE_NTYPES);
	EFSYS_ASSERT3U(epp->ep_current_bist, ==, type);

	EFSYS_ASSERT(epop->epo_bist_stop != NULL);

	if (epop->epo_bist_stop != NULL)
		epop->epo_bist_stop(enp, type);

	epp->ep_current_bist = EFX_BIST_TYPE_UNKNOWN;
}

#endif	/* EFSYS_OPT_BIST */
			void
efx_phy_unprobe(
	__in	efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	epp->ep_epop = NULL;

	epp->ep_adv_cap_mask = 0;

	epp->ep_port = 0;
	epp->ep_phy_type = 0;
}
