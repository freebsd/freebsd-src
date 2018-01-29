/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define	BWN_USE_SIBA	1
#include "if_bwn_siba.h"

/** Legacy siba(4) bus operations */

static int
bwn_siba_bus_ops_init(device_t dev)
{
	return (0);
}

static void
bwn_siba_bus_ops_fini(device_t dev)
{
}

const struct bwn_bus_ops bwn_siba_bus_ops = {
	.init				= bwn_siba_bus_ops_init,
	.fini				= bwn_siba_bus_ops_fini,
	.pci_find_cap			= pci_find_cap,
	.pci_alloc_msi			= pci_alloc_msi,
	.pci_release_msi		= pci_release_msi,
	.pci_msi_count			= pci_msi_count,
	.get_vendor			= siba_get_vendor,
	.get_device			= siba_get_device,
	.get_revid			= siba_get_revid,
	.get_pci_vendor			= siba_get_pci_vendor,
	.get_pci_device			= siba_get_pci_device,
	.get_pci_subvendor		= siba_get_pci_subvendor,
	.get_pci_subdevice		= siba_get_pci_subdevice,
	.get_pci_revid			= siba_get_pci_revid,
	.get_chipid			= siba_get_chipid,
	.get_chiprev			= siba_get_chiprev,
	.get_chippkg			= siba_get_chippkg,
	.get_type			= siba_get_type,
	.get_cc_pmufreq			= siba_get_cc_pmufreq,
	.get_cc_caps			= siba_get_cc_caps,
	.get_cc_powerdelay		= siba_get_cc_powerdelay,
	.get_pcicore_revid		= siba_get_pcicore_revid,
	.sprom_get_rev			= siba_sprom_get_rev,
	.sprom_get_mac_80211bg		= siba_sprom_get_mac_80211bg,
	.sprom_get_mac_80211a		= siba_sprom_get_mac_80211a,
	.sprom_get_brev			= siba_sprom_get_brev,
	.sprom_get_ccode		= siba_sprom_get_ccode,
	.sprom_get_ant_a		= siba_sprom_get_ant_a,
	.sprom_get_ant_bg		= siba_sprom_get_ant_bg,
	.sprom_get_pa0b0		= siba_sprom_get_pa0b0,
	.sprom_get_pa0b1		= siba_sprom_get_pa0b1,
	.sprom_get_pa0b2		= siba_sprom_get_pa0b2,
	.sprom_get_gpio0		= siba_sprom_get_gpio0,
	.sprom_get_gpio1		= siba_sprom_get_gpio1,
	.sprom_get_gpio2		= siba_sprom_get_gpio2,
	.sprom_get_gpio3		= siba_sprom_get_gpio3,
	.sprom_get_maxpwr_bg		= siba_sprom_get_maxpwr_bg,
	.sprom_set_maxpwr_bg		= siba_sprom_set_maxpwr_bg,
	.sprom_get_rxpo2g		= siba_sprom_get_rxpo2g,
	.sprom_get_rxpo5g		= siba_sprom_get_rxpo5g,
	.sprom_get_tssi_bg		= siba_sprom_get_tssi_bg,
	.sprom_get_tri2g		= siba_sprom_get_tri2g,
	.sprom_get_tri5gl		= siba_sprom_get_tri5gl,
	.sprom_get_tri5g		= siba_sprom_get_tri5g,
	.sprom_get_tri5gh		= siba_sprom_get_tri5gh,
	.sprom_get_rssisav2g		= siba_sprom_get_rssisav2g,
	.sprom_get_rssismc2g		= siba_sprom_get_rssismc2g,
	.sprom_get_rssismf2g		= siba_sprom_get_rssismf2g,
	.sprom_get_bxa2g		= siba_sprom_get_bxa2g,
	.sprom_get_rssisav5g		= siba_sprom_get_rssisav5g,
	.sprom_get_rssismc5g		= siba_sprom_get_rssismc5g,
	.sprom_get_rssismf5g		= siba_sprom_get_rssismf5g,
	.sprom_get_bxa5g		= siba_sprom_get_bxa5g,
	.sprom_get_cck2gpo		= siba_sprom_get_cck2gpo,
	.sprom_get_ofdm2gpo		= siba_sprom_get_ofdm2gpo,
	.sprom_get_ofdm5glpo		= siba_sprom_get_ofdm5glpo,
	.sprom_get_ofdm5gpo		= siba_sprom_get_ofdm5gpo,
	.sprom_get_ofdm5ghpo		= siba_sprom_get_ofdm5ghpo,
	.sprom_get_bf_lo		= siba_sprom_get_bf_lo,
	.sprom_set_bf_lo		= siba_sprom_set_bf_lo,
	.sprom_get_bf_hi		= siba_sprom_get_bf_hi,
	.sprom_get_bf2_lo		= siba_sprom_get_bf2_lo,
	.sprom_get_bf2_hi		= siba_sprom_get_bf2_hi,
	.sprom_get_fem_2ghz_tssipos	= siba_sprom_get_fem_2ghz_tssipos,
	.sprom_get_fem_2ghz_extpa_gain	= siba_sprom_get_fem_2ghz_extpa_gain,
	.sprom_get_fem_2ghz_pdet_range	= siba_sprom_get_fem_2ghz_pdet_range,
	.sprom_get_fem_2ghz_tr_iso	= siba_sprom_get_fem_2ghz_tr_iso,
	.sprom_get_fem_2ghz_antswlut	= siba_sprom_get_fem_2ghz_antswlut,
	.sprom_get_fem_5ghz_extpa_gain	= siba_sprom_get_fem_5ghz_extpa_gain,
	.sprom_get_fem_5ghz_pdet_range	= siba_sprom_get_fem_5ghz_pdet_range,
	.sprom_get_fem_5ghz_antswlut	= siba_sprom_get_fem_5ghz_antswlut,
	.sprom_get_txpid_2g_0		= siba_sprom_get_txpid_2g_0,
	.sprom_get_txpid_2g_1		= siba_sprom_get_txpid_2g_1,
	.sprom_get_txpid_5gl_0		= siba_sprom_get_txpid_5gl_0,
	.sprom_get_txpid_5gl_1		= siba_sprom_get_txpid_5gl_1,
	.sprom_get_txpid_5g_0		= siba_sprom_get_txpid_5g_0,
	.sprom_get_txpid_5g_1		= siba_sprom_get_txpid_5g_1,
	.sprom_get_txpid_5gh_0		= siba_sprom_get_txpid_5gh_0,
	.sprom_get_txpid_5gh_1		= siba_sprom_get_txpid_5gh_1,
	.sprom_get_stbcpo		= siba_sprom_get_stbcpo,
	.sprom_get_cddpo		= siba_sprom_get_cddpo,
	.powerup			= siba_powerup,
	.powerdown			= siba_powerdown,
	.read_2				= siba_read_2,
	.write_2			= siba_write_2,
	.read_4				= siba_read_4,
	.write_4			= siba_write_4,
	.dev_up				= siba_dev_up,
	.dev_down			= siba_dev_down,
	.dev_isup			= siba_dev_isup,
	.pcicore_intr			= siba_pcicore_intr,
	.dma_translation		= siba_dma_translation,
	.read_multi_2			= siba_read_multi_2,
	.read_multi_4			= siba_read_multi_4,
	.write_multi_2			= siba_write_multi_2,
	.write_multi_4			= siba_write_multi_4,
	.barrier			= siba_barrier,
	.cc_pmu_set_ldovolt		= siba_cc_pmu_set_ldovolt,
	.cc_pmu_set_ldoparef		= siba_cc_pmu_set_ldoparef,
	.gpio_set			= siba_gpio_set,
	.gpio_get			= siba_gpio_get,
	.fix_imcfglobug			= siba_fix_imcfglobug,
	.sprom_get_core_power_info	= siba_sprom_get_core_power_info,
	.sprom_get_mcs2gpo		= siba_sprom_get_mcs2gpo,
	.sprom_get_mcs5glpo		= siba_sprom_get_mcs5glpo,
	.sprom_get_mcs5gpo		= siba_sprom_get_mcs5gpo,
	.sprom_get_mcs5ghpo		= siba_sprom_get_mcs5ghpo,
	.pmu_spuravoid_pllupdate	= siba_pmu_spuravoid_pllupdate,
	.cc_set32			= siba_cc_set32,
	.cc_mask32			= siba_cc_mask32,
	.cc_write32			= siba_cc_write32,
};
