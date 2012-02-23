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
 *
 * $FreeBSD$
 */

#ifndef _SYS_SIENA_IMPL_H
#define	_SYS_SIENA_IMPL_H

#include "efx.h"
#include "efx_regs.h"
#include "efx_mcdi.h"
#include "siena_flash.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_PHY_PROPS

/* START MKCONFIG GENERATED SienaPhyHeaderPropsBlock a8db1f8eb5106efd */
typedef enum siena_phy_prop_e {
	SIENA_PHY_NPROPS
} siena_phy_prop_t;

/* END MKCONFIG GENERATED SienaPhyHeaderPropsBlock */

#endif  /* EFSYS_OPT_PHY_PROPS */

#define	SIENA_NVRAM_CHUNK 0x80

extern	__checkReturn	int
siena_nic_probe(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_PCIE_TUNE

extern	__checkReturn	int
siena_nic_pcie_extended_sync(
	__in		efx_nic_t *enp);

#endif

extern	__checkReturn	int
siena_nic_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
siena_nic_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	int
siena_nic_register_test(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern			void
siena_nic_fini(
	__in		efx_nic_t *enp);

extern			void
siena_nic_unprobe(
	__in		efx_nic_t *enp);

#define	SIENA_SRAM_ROWS	0x12000

extern			void
siena_sram_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	int
siena_sram_test(
	__in		efx_nic_t *enp,
	__in		efx_sram_pattern_fn_t func);

#endif	/* EFSYS_OPT_DIAG */


#if EFSYS_OPT_NVRAM || EFSYS_OPT_VPD

extern	__checkReturn		int
siena_nvram_partn_size(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__out			size_t *sizep);

extern	__checkReturn		int
siena_nvram_partn_lock(
	__in			efx_nic_t *enp,
	__in			unsigned int partn);

extern	__checkReturn		int
siena_nvram_partn_read(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
siena_nvram_partn_erase(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__in			size_t size);

extern	__checkReturn		int
siena_nvram_partn_write(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
siena_nvram_partn_unlock(
	__in			efx_nic_t *enp,
	__in			unsigned int partn);

extern	__checkReturn		int
siena_nvram_get_dynamic_cfg(
	__in			efx_nic_t *enp,
	__in			unsigned int index,
	__in			boolean_t vpd,
	__out			siena_mc_dynamic_config_hdr_t **dcfgp,
	__out			size_t *sizep);

#endif	/* EFSYS_OPT_VPD || EFSYS_OPT_NVRAM */

#if EFSYS_OPT_NVRAM

#if EFSYS_OPT_DIAG

extern	__checkReturn		int
siena_nvram_test(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern	__checkReturn		int
siena_nvram_size(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *sizep);

extern	__checkReturn		int
siena_nvram_get_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4]);

extern	__checkReturn		int
siena_nvram_rw_start(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *pref_chunkp);

extern	__checkReturn		int
siena_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	 __checkReturn		int
siena_nvram_erase(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type);

extern	__checkReturn		int
siena_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
siena_nvram_rw_finish(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type);

extern	__checkReturn		int
siena_nvram_set_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint16_t version[4]);

#endif	/* EFSYS_OPT_NVRAM */

#if EFSYS_OPT_VPD

extern	__checkReturn		int
siena_vpd_init(
	__in			efx_nic_t *enp);

extern	__checkReturn		int
siena_vpd_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep);

extern	__checkReturn		int
siena_vpd_read(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
siena_vpd_verify(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
siena_vpd_reinit(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
siena_vpd_get(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__inout			efx_vpd_value_t *evvp);

extern	__checkReturn		int
siena_vpd_set(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_value_t *evvp);

extern	__checkReturn		int
siena_vpd_next(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			efx_vpd_value_t *evvp,
	__inout			unsigned int *contp);

extern __checkReturn		int
siena_vpd_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
siena_vpd_fini(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_VPD */

typedef struct siena_link_state_s {
	uint32_t		sls_adv_cap_mask;
	uint32_t		sls_lp_cap_mask;
	unsigned int 		sls_fcntl;
	efx_link_mode_t		sls_link_mode;
#if EFSYS_OPT_LOOPBACK
	efx_loopback_type_t	sls_loopback;
#endif
	boolean_t		sls_mac_up;
} siena_link_state_t;

extern			void
siena_phy_link_ev(
	__in		efx_nic_t *enp,
	__in		efx_qword_t *eqp,
	__out		efx_link_mode_t *link_modep);

extern	__checkReturn	int
siena_phy_get_link(
	__in		efx_nic_t *enp,
	__out		siena_link_state_t *slsp);

extern	__checkReturn	int
siena_phy_power(
	__in		efx_nic_t *enp,
	__in		boolean_t on);

extern	__checkReturn	int
siena_phy_reconfigure(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
siena_phy_verify(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
siena_phy_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip);

#if EFSYS_OPT_PHY_STATS

extern					void
siena_phy_decode_stats(
	__in				efx_nic_t *enp,
	__in				uint32_t vmask,
	__in_opt			efsys_mem_t *esmp,
	__out_opt			uint64_t *smaskp,
	__out_ecount_opt(EFX_PHY_NSTATS)	uint32_t *stat);

extern	__checkReturn			int
siena_phy_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_PHY_NSTATS)	uint32_t *stat);

#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES

extern		const char __cs *
siena_phy_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

extern	__checkReturn	int
siena_phy_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp);

extern	__checkReturn	int
siena_phy_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val);

#endif	/* EFSYS_OPT_PHY_PROPS */

#if EFSYS_OPT_PHY_BIST

extern	__checkReturn		int
siena_phy_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type);

extern	__checkReturn		int
siena_phy_bist_poll(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type,
	__out			efx_phy_bist_result_t *resultp,
	__out_opt __drv_when(count > 0, __notnull)
	uint32_t 	*value_maskp,
	__out_ecount_opt(count)	__drv_when(count > 0, __notnull)
	unsigned long	*valuesp,
	__in			size_t count);

extern				void
siena_phy_bist_stop(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type);

#endif	/* EFSYS_OPT_PHY_BIST */

extern	__checkReturn	int
siena_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *link_modep);

extern	__checkReturn	int
siena_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp);

extern	__checkReturn	int
siena_mac_reconfigure(
	__in	efx_nic_t *enp);

#if EFSYS_OPT_LOOPBACK

extern	__checkReturn	int
siena_mac_loopback_set(
	__in		efx_nic_t *enp,
	__in		efx_link_mode_t link_mode,
	__in		efx_loopback_type_t loopback_type);

#endif	/* EFSYS_OPT_LOOPBACK */

#if EFSYS_OPT_MAC_STATS

extern	__checkReturn			int
siena_mac_stats_clear(
	__in				efx_nic_t *enp);

extern	__checkReturn			int
siena_mac_stats_upload(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp);

extern	__checkReturn			int
siena_mac_stats_periodic(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__in				uint16_t period_ms,
	__in				boolean_t events);

extern	__checkReturn			int
siena_mac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stat,
	__out_opt			uint32_t *generationp);

#endif	/* EFSYS_OPT_MAC_STATS */

extern	__checkReturn	int
siena_mon_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
siena_mon_reconfigure(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_MON_STATS

extern					void
siena_mon_decode_stats(
	__in				efx_nic_t *enp,
	__in				uint32_t dmask,
	__in_opt			efsys_mem_t *esmp,
	__out_opt				uint32_t *vmaskp,
	__out_ecount_opt(EFX_MON_NSTATS)	efx_mon_stat_value_t *value);

extern	__checkReturn			int
siena_mon_ev(
	__in				efx_nic_t *enp,
	__in				efx_qword_t *eqp,
	__out				efx_mon_stat_t *idp,
	__out				efx_mon_stat_value_t *valuep);

extern	__checkReturn			int
siena_mon_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values);

#endif	/* EFSYS_OPT_MON_STATS */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SIENA_IMPL_H */
