/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2007-2016 Solarflare Communications Inc.
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

#if EFSYS_OPT_MON_MCDI
#include "mcdi_mon.h"
#endif

#if EFSYS_OPT_NAMES

static const char * const __efx_mon_name[] = {
	"",
	"sfx90x0",
	"sfx91x0",
	"sfx92x0"
};

		const char *
efx_mon_name(
	__in	efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	EFSYS_ASSERT(encp->enc_mon_type != EFX_MON_INVALID);
	EFSYS_ASSERT3U(encp->enc_mon_type, <, EFX_MON_NTYPES);
	return (__efx_mon_name[encp->enc_mon_type]);
}

#endif	/* EFSYS_OPT_NAMES */

#if EFSYS_OPT_MON_MCDI
static const efx_mon_ops_t	__efx_mon_mcdi_ops = {
#if EFSYS_OPT_MON_STATS
	mcdi_mon_stats_update		/* emo_stats_update */
#endif	/* EFSYS_OPT_MON_STATS */
};
#endif


	__checkReturn	efx_rc_t
efx_mon_init(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mon_t *emp = &(enp->en_mon);
	const efx_mon_ops_t *emop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	if (enp->en_mod_flags & EFX_MOD_MON) {
		rc = EINVAL;
		goto fail1;
	}

	enp->en_mod_flags |= EFX_MOD_MON;

	emp->em_type = encp->enc_mon_type;

	EFSYS_ASSERT(encp->enc_mon_type != EFX_MON_INVALID);
	switch (emp->em_type) {
#if EFSYS_OPT_MON_MCDI
	case EFX_MON_SFC90X0:
	case EFX_MON_SFC91X0:
	case EFX_MON_SFC92X0:
		emop = &__efx_mon_mcdi_ops;
		break;
#endif
	default:
		rc = ENOTSUP;
		goto fail2;
	}

	emp->em_emop = emop;
	return (0);

fail2:
	EFSYS_PROBE(fail2);

	emp->em_type = EFX_MON_INVALID;

	enp->en_mod_flags &= ~EFX_MOD_MON;

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#if EFSYS_OPT_MON_STATS

#if EFSYS_OPT_NAMES

/* START MKCONFIG GENERATED MonitorStatNamesBlock 5daa2a5725ba734b */
static const char * const __mon_stat_name[] = {
	"value_2_5v",
	"value_vccp1",
	"value_vcc",
	"value_5v",
	"value_12v",
	"value_vccp2",
	"value_ext_temp",
	"value_int_temp",
	"value_ain1",
	"value_ain2",
	"controller_cooling",
	"ext_cooling",
	"1v",
	"1_2v",
	"1_8v",
	"3_3v",
	"1_2va",
	"vref",
	"vaoe",
	"aoe_temperature",
	"psu_aoe_temperature",
	"psu_temperature",
	"fan0",
	"fan1",
	"fan2",
	"fan3",
	"fan4",
	"vaoe_in",
	"iaoe",
	"iaoe_in",
	"nic_power",
	"0_9v",
	"i0_9v",
	"i1_2v",
	"0_9v_adc",
	"controller_temperature2",
	"vreg_temperature",
	"vreg_0_9v_temperature",
	"vreg_1_2v_temperature",
	"int_vptat",
	"controller_internal_adc_temperature",
	"ext_vptat",
	"controller_external_adc_temperature",
	"ambient_temperature",
	"airflow",
	"vdd08d_vss08d_csr",
	"vdd08d_vss08d_csr_extadc",
	"hotpoint_temperature",
	"phy_power_switch_port0",
	"phy_power_switch_port1",
	"mum_vcc",
	"0v9_a",
	"i0v9_a",
	"0v9_a_temp",
	"0v9_b",
	"i0v9_b",
	"0v9_b_temp",
	"ccom_avreg_1v2_supply",
	"ccom_avreg_1v2_supply_ext_adc",
	"ccom_avreg_1v8_supply",
	"ccom_avreg_1v8_supply_ext_adc",
	"controller_master_vptat",
	"controller_master_internal_temp",
	"controller_master_vptat_ext_adc",
	"controller_master_internal_temp_ext_adc",
	"controller_slave_vptat",
	"controller_slave_internal_temp",
	"controller_slave_vptat_ext_adc",
	"controller_slave_internal_temp_ext_adc",
	"sodimm_vout",
	"sodimm_0_temp",
	"sodimm_1_temp",
	"phy0_vcc",
	"phy1_vcc",
	"controller_tdiode_temp",
	"board_front_temp",
	"board_back_temp",
};

/* END MKCONFIG GENERATED MonitorStatNamesBlock */

extern					const char *
efx_mon_stat_name(
	__in				efx_nic_t *enp,
	__in				efx_mon_stat_t id)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	EFSYS_ASSERT3U(id, <, EFX_MON_NSTATS);
	return (__mon_stat_name[id]);
}

#endif	/* EFSYS_OPT_NAMES */

	__checkReturn			efx_rc_t
efx_mon_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values)
{
	efx_mon_t *emp = &(enp->en_mon);
	const efx_mon_ops_t *emop = emp->em_emop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MON);

	return (emop->emo_stats_update(enp, esmp, values));
}

#endif	/* EFSYS_OPT_MON_STATS */

		void
efx_mon_fini(
	__in	efx_nic_t *enp)
{
	efx_mon_t *emp = &(enp->en_mon);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MON);

	emp->em_emop = NULL;

	emp->em_type = EFX_MON_INVALID;

	enp->en_mod_flags &= ~EFX_MOD_MON;
}
