/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016 Landon J. Fuller <landonf@FreeBSD.org>.
 * Copyright (c) 2007 Bruce M. Simpson.
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
 *
 * $FreeBSD$
 */

#ifndef _IF_BWN_SIBA_H_
#define _IF_BWN_SIBA_H_

/** If true, expose legacy siba_pci headers directly. Otherwise,
  * we expose our siba/bhnd compatibility shims. */
#ifndef	BWN_USE_SIBA
#define	BWN_USE_SIBA	0
#endif

struct bwn_softc;
struct siba_sprom_core_pwr_info;

/*
 * Legacy siba(4) bus API compatibility shims.
 */
struct bwn_bus_ops {
	/* bus-specific initialization/finalization */
	int		(*init)(device_t);
	void		(*fini)(device_t);

	/* compatibility shims */
	int		(*pci_find_cap)(device_t, int, int *);
	int		(*pci_alloc_msi)(device_t, int *);
	int		(*pci_release_msi)(device_t);
	int		(*pci_msi_count)(device_t);
	uint16_t	(*get_vendor)(device_t);
	uint16_t	(*get_device)(device_t);
	uint8_t		(*get_revid)(device_t);
	uint16_t	(*get_pci_vendor)(device_t);
	uint16_t	(*get_pci_device)(device_t);
	uint16_t	(*get_pci_subvendor)(device_t);
	uint16_t	(*get_pci_subdevice)(device_t);
	uint8_t		(*get_pci_revid)(device_t);
	uint16_t	(*get_chipid)(device_t);
	uint16_t	(*get_chiprev)(device_t);
	uint8_t		(*get_chippkg)(device_t);
	enum siba_type	(*get_type)(device_t);
	uint32_t	(*get_cc_pmufreq)(device_t);
	uint32_t	(*get_cc_caps)(device_t);
	uint16_t	(*get_cc_powerdelay)(device_t);
	uint8_t		(*get_pcicore_revid)(device_t);
	uint8_t		(*sprom_get_rev)(device_t);
	uint8_t		*(*sprom_get_mac_80211bg)(device_t);
	uint8_t		*(*sprom_get_mac_80211a)(device_t);
	uint8_t		(*sprom_get_brev)(device_t);
	uint8_t		(*sprom_get_ccode)(device_t);
	uint8_t		(*sprom_get_ant_a)(device_t);
	uint8_t		(*sprom_get_ant_bg)(device_t);
	uint16_t	(*sprom_get_pa0b0)(device_t);
	uint16_t	(*sprom_get_pa0b1)(device_t);
	uint16_t	(*sprom_get_pa0b2)(device_t);
	uint8_t		(*sprom_get_gpio0)(device_t);
	uint8_t		(*sprom_get_gpio1)(device_t);
	uint8_t		(*sprom_get_gpio2)(device_t);
	uint8_t		(*sprom_get_gpio3)(device_t);
	uint16_t	(*sprom_get_maxpwr_bg)(device_t);
	void		(*sprom_set_maxpwr_bg)(device_t, uint16_t);
	uint8_t		(*sprom_get_rxpo2g)(device_t);
	uint8_t		(*sprom_get_rxpo5g)(device_t);
	uint8_t		(*sprom_get_tssi_bg)(device_t);
	uint8_t		(*sprom_get_tri2g)(device_t);
	uint8_t		(*sprom_get_tri5gl)(device_t);
	uint8_t		(*sprom_get_tri5g)(device_t);
	uint8_t		(*sprom_get_tri5gh)(device_t);
	uint8_t		(*sprom_get_rssisav2g)(device_t);
	uint8_t		(*sprom_get_rssismc2g)(device_t);
	uint8_t		(*sprom_get_rssismf2g)(device_t);
	uint8_t		(*sprom_get_bxa2g)(device_t);
	uint8_t		(*sprom_get_rssisav5g)(device_t);
	uint8_t		(*sprom_get_rssismc5g)(device_t);
	uint8_t		(*sprom_get_rssismf5g)(device_t);
	uint8_t		(*sprom_get_bxa5g)(device_t);
	uint16_t	(*sprom_get_cck2gpo)(device_t);
	uint32_t	(*sprom_get_ofdm2gpo)(device_t);
	uint32_t	(*sprom_get_ofdm5glpo)(device_t);
	uint32_t	(*sprom_get_ofdm5gpo)(device_t);
	uint32_t	(*sprom_get_ofdm5ghpo)(device_t);
	uint16_t	(*sprom_get_bf_lo)(device_t);
	void		(*sprom_set_bf_lo)(device_t, uint16_t);
	uint16_t	(*sprom_get_bf_hi)(device_t);
	uint16_t	(*sprom_get_bf2_lo)(device_t);
	uint16_t	(*sprom_get_bf2_hi)(device_t);
	uint8_t		(*sprom_get_fem_2ghz_tssipos)(device_t);
	uint8_t		(*sprom_get_fem_2ghz_extpa_gain)(device_t);
	uint8_t		(*sprom_get_fem_2ghz_pdet_range)(device_t);
	uint8_t		(*sprom_get_fem_2ghz_tr_iso)(device_t);
	uint8_t		(*sprom_get_fem_2ghz_antswlut)(device_t);
	uint8_t		(*sprom_get_fem_5ghz_extpa_gain)(device_t);
	uint8_t		(*sprom_get_fem_5ghz_pdet_range)(device_t);
	uint8_t		(*sprom_get_fem_5ghz_antswlut)(device_t);
	uint8_t		(*sprom_get_txpid_2g_0)(device_t);
	uint8_t		(*sprom_get_txpid_2g_1)(device_t);
	uint8_t		(*sprom_get_txpid_5gl_0)(device_t);
	uint8_t		(*sprom_get_txpid_5gl_1)(device_t);
	uint8_t		(*sprom_get_txpid_5g_0)(device_t);
	uint8_t		(*sprom_get_txpid_5g_1)(device_t);
	uint8_t		(*sprom_get_txpid_5gh_0)(device_t);
	uint8_t		(*sprom_get_txpid_5gh_1)(device_t);
	uint16_t	(*sprom_get_stbcpo)(device_t);
	uint16_t	(*sprom_get_cddpo)(device_t);
	void		(*powerup)(device_t, int);
	int		(*powerdown)(device_t);
	uint16_t	(*read_2)(device_t, uint16_t);
	void		(*write_2)(device_t, uint16_t, uint16_t);
	uint32_t	(*read_4)(device_t, uint16_t);
	void		(*write_4)(device_t, uint16_t, uint32_t);
	void		(*dev_up)(device_t, uint32_t);
	void		(*dev_down)(device_t, uint32_t);
	int		(*dev_isup)(device_t);
	void		(*pcicore_intr)(device_t);
	uint32_t	(*dma_translation)(device_t);
	void		(*read_multi_2)(device_t, void *, size_t, uint16_t);
	void		(*read_multi_4)(device_t, void *, size_t, uint16_t);
	void		(*write_multi_2)(device_t, const void *, size_t, uint16_t);
	void		(*write_multi_4)(device_t, const void *, size_t, uint16_t);
	void		(*barrier)(device_t, int);
	void		(*cc_pmu_set_ldovolt)(device_t, int, uint32_t);
	void		(*cc_pmu_set_ldoparef)(device_t, uint8_t);
	void		(*gpio_set)(device_t, uint32_t);
	uint32_t	(*gpio_get)(device_t);
	void		(*fix_imcfglobug)(device_t);
	int		(*sprom_get_core_power_info)(device_t, int, struct siba_sprom_core_pwr_info *);
	int		(*sprom_get_mcs2gpo)(device_t, uint16_t *);
	int		(*sprom_get_mcs5glpo)(device_t, uint16_t *);
	int		(*sprom_get_mcs5gpo)(device_t, uint16_t *);
	int		(*sprom_get_mcs5ghpo)(device_t, uint16_t *);
	void		(*pmu_spuravoid_pllupdate)(device_t, int);
	void		(*cc_set32)(device_t, uint32_t, uint32_t);
	void		(*cc_mask32)(device_t, uint32_t, uint32_t);
	void		(*cc_write32)(device_t, uint32_t, uint32_t);
};

#if BWN_USE_SIBA

#include <dev/siba/siba_ids.h>
#include <dev/siba/sibareg.h>
#include <dev/siba/sibavar.h>

#define	BWN_BUS_OPS_ATTACH(_dev)	(0)
#define	BWN_BUS_OPS_DETACH(_dev)

#else /* !BWN_USE_SIBA */

struct bwn_bus_ops;

extern const struct bwn_bus_ops bwn_siba_bus_ops;
extern const struct bwn_bus_ops bwn_bhnd_bus_ops;

/*
 * Declared in:
 *    /usr/home/landonf/Documents/Code/FreeBSD/svn/head/sys/dev/siba/siba_ids.h
 */

struct siba_devid {
    uint16_t sd_vendor;
    uint16_t sd_device;
    uint8_t sd_rev;
    char *sd_desc;
};

#define	SIBA_DEV(_vendor, _cid, _rev, _msg)	\
	{ SIBA_VID_##_vendor, SIBA_DEVID_##_cid, _rev, _msg }

#define	SIBA_DEVID_80211		0x812
#define	SIBA_VID_BROADCOM		0x4243

/*
 * Declared in:
 *    /usr/home/landonf/Documents/Code/FreeBSD/svn/head/sys/dev/siba/sibareg.h
 */

#define	SIBA_CC_CAPS_PMU		0x10000000
#define	SIBA_CC_CHIPCTL			0x0028
#define	SIBA_CC_CHIPCTL_ADDR		0x0650
#define	SIBA_CC_CHIPCTL_DATA		0x0654

#define	SIBA_DMA_TRANSLATION_MASK	0xc0000000

#define	SIBA_TGSLOW			0x0f98
#define	SIBA_TGSLOW_FGC			0x00020000

#define	SIBA_TGSHIGH			0x0f9c
#define	SIBA_TGSHIGH_DMA64		0x10000000

#define	SIBA_BOARDVENDOR_DELL		0x1028
#define	SIBA_BOARDVENDOR_BCM		0x14e4

#define	SIBA_BOARD_BCM4309G		0x0421
#define	SIBA_BOARD_BU4306		0x0416
#define	SIBA_BOARD_BCM4321		0x046d

#define	SIBA_CHIPPACK_BCM4712S		1


/*
 * Declared in:
 *    /usr/home/landonf/Documents/Code/FreeBSD/svn/head/sys/dev/siba/sibavar.h
 */

enum siba_type {
	SIBA_TYPE_SSB			/* unused */,
	SIBA_TYPE_PCI,
	SIBA_TYPE_PCMCIA
};

/* TODO: need a real country code table */
enum {
	SIBA_CCODE_JAPAN,
	SIBA_CCODE_UNKNOWN
};

struct siba_sprom_core_pwr_info {
    uint8_t itssi_2g;
    uint8_t itssi_5g;
    uint8_t maxpwr_2g;
    uint8_t maxpwr_5gl;
    uint8_t maxpwr_5g;
    uint8_t maxpwr_5gh;
    int16_t pa_2g[3];
    int16_t pa_5gl[4];
    int16_t pa_5g[4];
    int16_t pa_5gh[4];
};

#define	SIBA_LDO_PAREF	0

#define	BWN_BUS_OPS_SC(_sc)	\
	((_sc)->sc_bus_ops)

#define	BWN_BUS_OPS(_dev)	\
	BWN_BUS_OPS_SC((struct bwn_softc *)device_get_softc(_dev))

#define	BWN_BUS_OPS_ATTACH(_dev)	\
	BWN_BUS_OPS(_dev)->init(_dev)
#define	BWN_BUS_OPS_DETACH(_dev)	\
	BWN_BUS_OPS(_dev)->fini(_dev)

#define	pci_find_cap(_dev, capability, capreg)	\
	BWN_BUS_OPS(_dev)->pci_find_cap(_dev, capability, capreg)
#define	pci_alloc_msi(_dev, count)	\
	BWN_BUS_OPS(_dev)->pci_alloc_msi(_dev, count)
#define	pci_release_msi(_dev)	\
	BWN_BUS_OPS(_dev)->pci_release_msi(_dev)
#define	pci_msi_count(_dev)	\
	BWN_BUS_OPS(_dev)->pci_msi_count(_dev)

#define	siba_get_vendor(_dev)	\
	BWN_BUS_OPS(_dev)->get_vendor(_dev)
#define	siba_get_device(_dev)	\
	BWN_BUS_OPS(_dev)->get_device(_dev)
#define	siba_get_revid(_dev)	\
	BWN_BUS_OPS(_dev)->get_revid(_dev)
#define	siba_get_pci_vendor(_dev)	\
	BWN_BUS_OPS(_dev)->get_pci_vendor(_dev)
#define	siba_get_pci_device(_dev)	\
	BWN_BUS_OPS(_dev)->get_pci_device(_dev)
#define	siba_get_pci_subvendor(_dev)	\
	BWN_BUS_OPS(_dev)->get_pci_subvendor(_dev)
#define	siba_get_pci_subdevice(_dev)	\
	BWN_BUS_OPS(_dev)->get_pci_subdevice(_dev)
#define	siba_get_pci_revid(_dev)	\
	BWN_BUS_OPS(_dev)->get_pci_revid(_dev)
#define	siba_get_chipid(_dev)	\
	BWN_BUS_OPS(_dev)->get_chipid(_dev)
#define	siba_get_chiprev(_dev)	\
	BWN_BUS_OPS(_dev)->get_chiprev(_dev)
#define	siba_get_chippkg(_dev)	\
	BWN_BUS_OPS(_dev)->get_chippkg(_dev)
#define	siba_get_type(_dev)	\
	BWN_BUS_OPS(_dev)->get_type(_dev)
#define	siba_get_cc_pmufreq(_dev)	\
	BWN_BUS_OPS(_dev)->get_cc_pmufreq(_dev)
#define	siba_get_cc_caps(_dev)	\
	BWN_BUS_OPS(_dev)->get_cc_caps(_dev)
#define	siba_get_cc_powerdelay(_dev)	\
	BWN_BUS_OPS(_dev)->get_cc_powerdelay(_dev)
#define	siba_get_pcicore_revid(_dev)	\
	BWN_BUS_OPS(_dev)->get_pcicore_revid(_dev)
#define	siba_sprom_get_rev(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_rev(_dev)
#define	siba_sprom_get_mac_80211bg(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_mac_80211bg(_dev)
#define	siba_sprom_get_mac_80211a(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_mac_80211a(_dev)
#define	siba_sprom_get_brev(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_brev(_dev)
#define	siba_sprom_get_ccode(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_ccode(_dev)
#define	siba_sprom_get_ant_a(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_ant_a(_dev)
#define	siba_sprom_get_ant_bg(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_ant_bg(_dev)
#define	siba_sprom_get_pa0b0(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_pa0b0(_dev)
#define	siba_sprom_get_pa0b1(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_pa0b1(_dev)
#define	siba_sprom_get_pa0b2(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_pa0b2(_dev)
#define	siba_sprom_get_gpio0(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_gpio0(_dev)
#define	siba_sprom_get_gpio1(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_gpio1(_dev)
#define	siba_sprom_get_gpio2(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_gpio2(_dev)
#define	siba_sprom_get_gpio3(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_gpio3(_dev)
#define	siba_sprom_get_maxpwr_bg(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_maxpwr_bg(_dev)
#define	siba_sprom_set_maxpwr_bg(_dev, t)	\
	BWN_BUS_OPS(_dev)->sprom_set_maxpwr_bg(_dev, t)
#define	siba_sprom_get_rxpo2g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_rxpo2g(_dev)
#define	siba_sprom_get_rxpo5g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_rxpo5g(_dev)
#define	siba_sprom_get_tssi_bg(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_tssi_bg(_dev)
#define	siba_sprom_get_tri2g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_tri2g(_dev)
#define	siba_sprom_get_tri5gl(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_tri5gl(_dev)
#define	siba_sprom_get_tri5g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_tri5g(_dev)
#define	siba_sprom_get_tri5gh(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_tri5gh(_dev)
#define	siba_sprom_get_rssisav2g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_rssisav2g(_dev)
#define	siba_sprom_get_rssismc2g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_rssismc2g(_dev)
#define	siba_sprom_get_rssismf2g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_rssismf2g(_dev)
#define	siba_sprom_get_bxa2g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_bxa2g(_dev)
#define	siba_sprom_get_rssisav5g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_rssisav5g(_dev)
#define	siba_sprom_get_rssismc5g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_rssismc5g(_dev)
#define	siba_sprom_get_rssismf5g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_rssismf5g(_dev)
#define	siba_sprom_get_bxa5g(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_bxa5g(_dev)
#define	siba_sprom_get_cck2gpo(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_cck2gpo(_dev)
#define	siba_sprom_get_ofdm2gpo(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_ofdm2gpo(_dev)
#define	siba_sprom_get_ofdm5glpo(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_ofdm5glpo(_dev)
#define	siba_sprom_get_ofdm5gpo(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_ofdm5gpo(_dev)
#define	siba_sprom_get_ofdm5ghpo(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_ofdm5ghpo(_dev)
#define	siba_sprom_get_bf_lo(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_bf_lo(_dev)
#define	siba_sprom_set_bf_lo(_dev, t)	\
	BWN_BUS_OPS(_dev)->sprom_set_bf_lo(_dev, t)
#define	siba_sprom_get_bf_hi(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_bf_hi(_dev)
#define	siba_sprom_get_bf2_lo(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_bf2_lo(_dev)
#define	siba_sprom_get_bf2_hi(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_bf2_hi(_dev)
#define	siba_sprom_get_fem_2ghz_tssipos(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_fem_2ghz_tssipos(_dev)
#define	siba_sprom_get_fem_2ghz_extpa_gain(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_fem_2ghz_extpa_gain(_dev)
#define	siba_sprom_get_fem_2ghz_pdet_range(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_fem_2ghz_pdet_range(_dev)
#define	siba_sprom_get_fem_2ghz_tr_iso(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_fem_2ghz_tr_iso(_dev)
#define	siba_sprom_get_fem_2ghz_antswlut(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_fem_2ghz_antswlut(_dev)
#define	siba_sprom_get_fem_5ghz_extpa_gain(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_fem_5ghz_extpa_gain(_dev)
#define	siba_sprom_get_fem_5ghz_pdet_range(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_fem_5ghz_pdet_range(_dev)
#define	siba_sprom_get_fem_5ghz_antswlut(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_fem_5ghz_antswlut(_dev)
#define	siba_sprom_get_txpid_2g_0(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_txpid_2g_0(_dev)
#define	siba_sprom_get_txpid_2g_1(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_txpid_2g_1(_dev)
#define	siba_sprom_get_txpid_5gl_0(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_txpid_5gl_0(_dev)
#define	siba_sprom_get_txpid_5gl_1(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_txpid_5gl_1(_dev)
#define	siba_sprom_get_txpid_5g_0(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_txpid_5g_0(_dev)
#define	siba_sprom_get_txpid_5g_1(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_txpid_5g_1(_dev)
#define	siba_sprom_get_txpid_5gh_0(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_txpid_5gh_0(_dev)
#define	siba_sprom_get_txpid_5gh_1(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_txpid_5gh_1(_dev)
#define	siba_sprom_get_stbcpo(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_stbcpo(_dev)
#define	siba_sprom_get_cddpo(_dev)	\
	BWN_BUS_OPS(_dev)->sprom_get_cddpo(_dev)
#define	siba_powerup(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->powerup(_dev, _arg1)
#define	siba_powerdown(_dev)	\
	BWN_BUS_OPS(_dev)->powerdown(_dev)
#define	siba_read_2(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->read_2(_dev, _arg1)
#define	siba_write_2(_dev, _arg1, _arg2)	\
	BWN_BUS_OPS(_dev)->write_2(_dev, _arg1, _arg2)
#define	siba_read_4(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->read_4(_dev, _arg1)
#define	siba_write_4(_dev, _arg1, _arg2)	\
	BWN_BUS_OPS(_dev)->write_4(_dev, _arg1, _arg2)
#define	siba_dev_up(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->dev_up(_dev, _arg1)
#define	siba_dev_down(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->dev_down(_dev, _arg1)
#define	siba_dev_isup(_dev)	\
	BWN_BUS_OPS(_dev)->dev_isup(_dev)
#define	siba_pcicore_intr(_dev)	\
	BWN_BUS_OPS(_dev)->pcicore_intr(_dev)
#define	siba_dma_translation(_dev)	\
	BWN_BUS_OPS(_dev)->dma_translation(_dev)
#define	siba_read_multi_2(_dev, _arg1, _arg2, _arg3)	\
	BWN_BUS_OPS(_dev)->read_multi_2(_dev, _arg1, _arg2, _arg3)
#define	siba_read_multi_4(_dev, _arg1, _arg2, _arg3)	\
	BWN_BUS_OPS(_dev)->read_multi_4(_dev, _arg1, _arg2, _arg3)
#define	siba_write_multi_2(_dev, _arg1, _arg2, _arg3)	\
	BWN_BUS_OPS(_dev)->write_multi_2(_dev, _arg1, _arg2, _arg3)
#define	siba_write_multi_4(_dev, _arg1, _arg2, _arg3)	\
	BWN_BUS_OPS(_dev)->write_multi_4(_dev, _arg1, _arg2, _arg3)
#define	siba_barrier(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->barrier(_dev, _arg1)
#define	siba_cc_pmu_set_ldovolt(_dev, _arg1, _arg2)	\
	BWN_BUS_OPS(_dev)->cc_pmu_set_ldovolt(_dev, _arg1, _arg2)
#define	siba_cc_pmu_set_ldoparef(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->cc_pmu_set_ldoparef(_dev, _arg1)
#define	siba_gpio_set(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->gpio_set(_dev, _arg1)
#define	siba_gpio_get(_dev)	\
	BWN_BUS_OPS(_dev)->gpio_get(_dev)
#define	siba_fix_imcfglobug(_dev)	\
	BWN_BUS_OPS(_dev)->fix_imcfglobug(_dev)
#define	siba_sprom_get_core_power_info(_dev, _arg1, _arg2)	\
	BWN_BUS_OPS(_dev)->sprom_get_core_power_info(_dev, _arg1, _arg2)
#define	siba_sprom_get_mcs2gpo(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->sprom_get_mcs2gpo(_dev, _arg1)
#define	siba_sprom_get_mcs5glpo(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->sprom_get_mcs5glpo(_dev, _arg1)
#define	siba_sprom_get_mcs5gpo(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->sprom_get_mcs5gpo(_dev, _arg1)
#define	siba_sprom_get_mcs5ghpo(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->sprom_get_mcs5ghpo(_dev, _arg1)
#define	siba_pmu_spuravoid_pllupdate(_dev, _arg1)	\
	BWN_BUS_OPS(_dev)->pmu_spuravoid_pllupdate(_dev, _arg1)
#define	siba_cc_set32(_dev, _arg1, _arg2)	\
	BWN_BUS_OPS(_dev)->cc_set32(_dev, _arg1, _arg2)
#define	siba_cc_mask32(_dev, _arg1, _arg2)	\
	BWN_BUS_OPS(_dev)->cc_mask32(_dev, _arg1, _arg2)
#define	siba_cc_write32(_dev, _arg1, _arg2)	\
	BWN_BUS_OPS(_dev)->cc_write32(_dev, _arg1, _arg2)

#endif /* BWN_USE_SIBA */

#endif /* _IF_BWN_SIBA_H_ */
