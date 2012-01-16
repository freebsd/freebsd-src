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
#include "efx_regs.h"
#include "efx_impl.h"

#if EFSYS_OPT_MON_NULL
#include "nullmon.h"
#endif

#if EFSYS_OPT_MON_LM87
#include "lm87.h"
#endif

#if EFSYS_OPT_MON_MAX6647
#include "max6647.h"
#endif

#if EFSYS_OPT_NAMES

static const char	__cs * __cs __efx_mon_name[] = {
	"",
	"nullmon",
	"lm87",
	"max6647",
	"sfx90x0"
};

		const char __cs *
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

#if EFSYS_OPT_MON_NULL
static efx_mon_ops_t	__cs __efx_mon_null_ops = {
	nullmon_reset,			/* emo_reset */
	nullmon_reconfigure,		/* emo_reconfigure */
#if EFSYS_OPT_MON_STATS
	nullmon_stats_update		/* emo_stat_update */
#endif	/* EFSYS_OPT_MON_STATS */
};
#endif

#if EFSYS_OPT_MON_LM87
static efx_mon_ops_t	__cs __efx_mon_lm87_ops = {
	lm87_reset,			/* emo_reset */
	lm87_reconfigure,		/* emo_reconfigure */
#if EFSYS_OPT_MON_STATS
	lm87_stats_update		/* emo_stat_update */
#endif	/* EFSYS_OPT_MON_STATS */
};
#endif

#if EFSYS_OPT_MON_MAX6647
static efx_mon_ops_t	__cs __efx_mon_max6647_ops = {
	max6647_reset,			/* emo_reset */
	max6647_reconfigure,		/* emo_reconfigure */
#if EFSYS_OPT_MON_STATS
	max6647_stats_update		/* emo_stat_update */
#endif	/* EFSYS_OPT_MON_STATS */
};
#endif

#if EFSYS_OPT_MON_SIENA
static efx_mon_ops_t	__cs __efx_mon_siena_ops = {
	siena_mon_reset,		/* emo_reset */
	siena_mon_reconfigure,		/* emo_reconfigure */
#if EFSYS_OPT_MON_STATS
	siena_mon_stats_update		/* emo_stat_update */
#endif	/* EFSYS_OPT_MON_STATS */
};
#endif


static efx_mon_ops_t	__cs * __cs __efx_mon_ops[] = {
	NULL,
#if EFSYS_OPT_MON_NULL
	&__efx_mon_null_ops,
#else
	NULL,
#endif
#if EFSYS_OPT_MON_LM87
	&__efx_mon_lm87_ops,
#else
	NULL,
#endif
#if EFSYS_OPT_MON_MAX6647
	&__efx_mon_max6647_ops,
#else
	NULL,
#endif
#if EFSYS_OPT_MON_SIENA
	&__efx_mon_siena_ops
#else
	NULL
#endif
};

	__checkReturn	int
efx_mon_init(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mon_t *emp = &(enp->en_mon);
	efx_mon_ops_t *emop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	if (enp->en_mod_flags & EFX_MOD_MON) {
		rc = EINVAL;
		goto fail1;
	}

	enp->en_mod_flags |= EFX_MOD_MON;

	emp->em_type = encp->enc_mon_type;

	EFSYS_ASSERT(encp->enc_mon_type != EFX_MON_INVALID);
	EFSYS_ASSERT3U(emp->em_type, <, EFX_MON_NTYPES);
	if ((emop = (efx_mon_ops_t *)__efx_mon_ops[emp->em_type]) == NULL) {
		rc = ENOTSUP;
		goto fail2;
	}

	if ((rc = emop->emo_reset(enp)) != 0)
		goto fail3;

	if ((rc = emop->emo_reconfigure(enp)) != 0)
		goto fail4;

	emp->em_emop = emop;
	return (0);

fail4:
	EFSYS_PROBE(fail5);

	(void) emop->emo_reset(enp);

fail3:
	EFSYS_PROBE(fail4);
fail2:
	EFSYS_PROBE(fail3);

	emp->em_type = EFX_MON_INVALID;

	enp->en_mod_flags &= ~EFX_MOD_MON;

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_MON_STATS

#if EFSYS_OPT_NAMES

/* START MKCONFIG GENERATED MonitorStatNamesBlock 08518fd1fb4e2612 */
static const char 	__cs * __cs __mon_stat_name[] = {
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
};

/* END MKCONFIG GENERATED MonitorStatNamesBlock */

extern					const char __cs *
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

	__checkReturn			int
efx_mon_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values)
{
	efx_mon_t *emp = &(enp->en_mon);
	efx_mon_ops_t *emop = emp->em_emop;

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
	efx_mon_ops_t *emop = emp->em_emop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MON);

	emp->em_emop = NULL;

	rc = emop->emo_reset(enp);
	if (rc != 0)
		EFSYS_PROBE1(fail1, int, rc);

	emp->em_type = EFX_MON_INVALID;

	enp->en_mod_flags &= ~EFX_MOD_MON;
}
