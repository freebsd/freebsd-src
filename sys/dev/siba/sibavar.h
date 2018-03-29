/*-
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

#ifndef _SIBA_SIBAVAR_H_
#define _SIBA_SIBAVAR_H_

#include <sys/rman.h>

struct siba_softc;
struct siba_dev_softc;

enum siba_type {
	SIBA_TYPE_SSB,
	SIBA_TYPE_PCI,
	SIBA_TYPE_PCMCIA,
};

enum siba_device_ivars {
	SIBA_IVAR_VENDOR,
	SIBA_IVAR_DEVICE,
	SIBA_IVAR_REVID,
	SIBA_IVAR_CORE_INDEX,
	SIBA_IVAR_PCI_VENDOR,
	SIBA_IVAR_PCI_DEVICE,
	SIBA_IVAR_PCI_SUBVENDOR,
	SIBA_IVAR_PCI_SUBDEVICE,
	SIBA_IVAR_PCI_REVID,
	SIBA_IVAR_CHIPID,
	SIBA_IVAR_CHIPREV,
	SIBA_IVAR_CHIPPKG,
	SIBA_IVAR_TYPE,
	SIBA_IVAR_CC_PMUFREQ,
	SIBA_IVAR_CC_CAPS,
	SIBA_IVAR_CC_POWERDELAY,
	SIBA_IVAR_PCICORE_REVID
};

#define	SIBA_ACCESSOR(var, ivar, type)				\
	__BUS_ACCESSOR(siba, var, SIBA, ivar, type)

SIBA_ACCESSOR(vendor,		VENDOR,		uint16_t)
SIBA_ACCESSOR(device,		DEVICE,		uint16_t)
SIBA_ACCESSOR(revid,		REVID,		uint8_t)
SIBA_ACCESSOR(core_index,	CORE_INDEX,	uint8_t)
SIBA_ACCESSOR(pci_vendor,	PCI_VENDOR,	uint16_t)
SIBA_ACCESSOR(pci_device,	PCI_DEVICE,	uint16_t)
SIBA_ACCESSOR(pci_subvendor,	PCI_SUBVENDOR,	uint16_t)
SIBA_ACCESSOR(pci_subdevice,	PCI_SUBDEVICE,	uint16_t)
SIBA_ACCESSOR(pci_revid,	PCI_REVID,	uint8_t)
SIBA_ACCESSOR(chipid,		CHIPID,		uint16_t)
SIBA_ACCESSOR(chiprev,		CHIPREV,	uint16_t)
SIBA_ACCESSOR(chippkg,		CHIPPKG,	uint8_t)
SIBA_ACCESSOR(type,		TYPE,		enum siba_type)
SIBA_ACCESSOR(cc_pmufreq,	CC_PMUFREQ,	uint32_t)
SIBA_ACCESSOR(cc_caps,		CC_CAPS,	uint32_t)
SIBA_ACCESSOR(cc_powerdelay,	CC_POWERDELAY,	uint16_t)
SIBA_ACCESSOR(pcicore_revid,	PCICORE_REVID,	uint8_t)

#undef SIBA_ACCESSOR

/* XXX just for SPROM1? */
enum {
	SIBA_CCODE_WORLD,
	SIBA_CCODE_THAILAND,
	SIBA_CCODE_ISRAEL,
	SIBA_CCODE_JORDAN,
	SIBA_CCODE_CHINA,
	SIBA_CCODE_JAPAN,
	SIBA_CCODE_USA_CANADA_ANZ,
	SIBA_CCODE_EUROPE,
	SIBA_CCODE_USA_LOW,
	SIBA_CCODE_JAPAN_HIGH,
	SIBA_CCODE_ALL,
	SIBA_CCODE_NONE,
};

#define siba_mips_read_2(sc, core, reg)				\
	bus_space_read_2((sc)->siba_mem_bt, (sc)->siba_mem_bh,	\
			 (core * SIBA_CORE_LEN) + (reg))

#define siba_mips_read_4(sc, core, reg)				\
	bus_space_read_4((sc)->siba_mem_bt, (sc)->siba_mem_bh,	\
			 (core * SIBA_CORE_LEN) + (reg))

#define siba_mips_write_2(sc, core, reg, val)			\
	bus_space_write_2((sc)->siba_mem_bt, (sc)->siba_mem_bh,	\
			 (core * SIBA_CORE_LEN) + (reg), (val))

#define siba_mips_write_4(sc, core, reg, val)			\
	bus_space_write_4((sc)->siba_mem_bt, (sc)->siba_mem_bh,	\
			 (core * SIBA_CORE_LEN) + (reg), (val))

#define	SIBA_READ_4(siba, reg)		\
	bus_space_read_4((siba)->siba_mem_bt, (siba)->siba_mem_bh, (reg))
#define	SIBA_READ_2(siba, reg)		\
	bus_space_read_2((siba)->siba_mem_bt, (siba)->siba_mem_bh, (reg))
#define	SIBA_READ_MULTI_1(siba, reg, addr, count)			\
	bus_space_read_multi_1((siba)->siba_mem_bt, (siba)->siba_mem_bh,\
	    (reg), (addr), (count))
#define	SIBA_READ_MULTI_2(siba, reg, addr, count)			\
	bus_space_read_multi_2((siba)->siba_mem_bt, (siba)->siba_mem_bh,\
	    (reg), (addr), (count))
#define	SIBA_READ_MULTI_4(siba, reg, addr, count)			\
	bus_space_read_multi_4((siba)->siba_mem_bt, (siba)->siba_mem_bh,\
	    (reg), (addr), (count))

#define	SIBA_WRITE_4(siba, reg, val)	\
	bus_space_write_4((siba)->siba_mem_bt, (siba)->siba_mem_bh,	\
	    (reg), (val))
#define	SIBA_WRITE_2(siba, reg, val)	\
	bus_space_write_2((siba)->siba_mem_bt, (siba)->siba_mem_bh,	\
	    (reg), (val))
#define	SIBA_WRITE_MULTI_1(siba, reg, addr, count)			\
	bus_space_write_multi_1((siba)->siba_mem_bt, (siba)->siba_mem_bh,\
	    (reg), (addr), (count))
#define	SIBA_WRITE_MULTI_2(siba, reg, addr, count)			\
	bus_space_write_multi_2((siba)->siba_mem_bt, (siba)->siba_mem_bh,\
	    (reg), (addr), (count))
#define	SIBA_WRITE_MULTI_4(siba, reg, addr, count)			\
	bus_space_write_multi_4((siba)->siba_mem_bt, (siba)->siba_mem_bh,\
	    (reg), (addr), (count))

#define	SIBA_BARRIER(siba, flags)					\
	bus_space_barrier((siba)->siba_mem_bt, (siba)->siba_mem_bh, (0),\
	    (0), (flags))

#define	SIBA_SETBITS_4(siba, reg, bits)	\
	SIBA_WRITE_4((siba), (reg), SIBA_READ_4((siba), (reg)) | (bits))
#define	SIBA_SETBITS_2(siba, reg, bits)	\
	SIBA_WRITE_2((siba), (reg), SIBA_READ_2((siba), (reg)) | (bits))

#define	SIBA_FILT_SETBITS_4(siba, reg, filt, bits) \
	SIBA_WRITE_4((siba), (reg), (SIBA_READ_4((siba),	\
	    (reg)) & (filt)) | (bits))
#define	SIBA_FILT_SETBITS_2(siba, reg, filt, bits)	\
	SIBA_WRITE_2((siba), (reg), (SIBA_READ_2((siba),	\
	    (reg)) & (filt)) | (bits))

#define	SIBA_CLRBITS_4(siba, reg, bits)	\
	SIBA_WRITE_4((siba), (reg), SIBA_READ_4((siba), (reg)) & ~(bits))
#define	SIBA_CLRBITS_2(siba, reg, bits)	\
	SIBA_WRITE_2((siba), (reg), SIBA_READ_2((siba), (reg)) & ~(bits))

#define	SIBA_CC_READ32(scc, offset) \
	siba_read_4_sub((scc)->scc_dev, offset)
#define	SIBA_CC_WRITE32(scc, offset, val) \
	siba_write_4_sub((scc)->scc_dev, offset, val)
#define	SIBA_CC_MASK32(scc, offset, mask) \
	SIBA_CC_WRITE32(scc, offset, SIBA_CC_READ32(scc, offset) & (mask))
#define	SIBA_CC_SET32(scc, offset, set) \
	SIBA_CC_WRITE32(scc, offset, SIBA_CC_READ32(scc, offset) | (set))
#define	SIBA_CC_MASKSET32(scc, offset, mask, set)	\
	SIBA_CC_WRITE32(scc, offset,			\
	    (SIBA_CC_READ32(scc, offset) & (mask)) | (set))

enum siba_clock {
	SIBA_CLOCK_DYNAMIC,
	SIBA_CLOCK_SLOW,
	SIBA_CLOCK_FAST,
};

enum siba_clksrc {
	SIBA_CC_CLKSRC_PCI,
	SIBA_CC_CLKSRC_CRYSTAL,
	SIBA_CC_CLKSRC_LOWPW,
};

struct siba_cc_pmu0_plltab {
	uint16_t		freq;	/* in kHz.*/
	uint8_t			xf;	/* crystal frequency */
	uint8_t			wb_int;
	uint32_t		wb_frac;
};

struct siba_cc_pmu1_plltab {
	uint16_t		freq;
	uint8_t			xf;
	uint8_t			p1div;
	uint8_t			p2div;
	uint8_t			ndiv_int;
	uint32_t		ndiv_frac;
};

struct siba_cc_pmu_res_updown {
	uint8_t			res;
	uint16_t		updown;
};

#define	SIBA_CC_PMU_DEP_SET	1
#define	SIBA_CC_PMU_DEP_ADD	2
#define	SIBA_CC_PMU_DEP_REMOVE	3

struct siba_cc_pmu_res_depend {
	uint8_t			res;
	uint8_t			task;
	uint32_t		depend;
};

enum siba_sprom_vars {
	SIBA_SPROMVAR_REV,
	SIBA_SPROMVAR_MAC_80211BG,
	SIBA_SPROMVAR_MAC_ETH,
	SIBA_SPROMVAR_MAC_80211A,
	SIBA_SPROMVAR_MII_ETH0,
	SIBA_SPROMVAR_MII_ETH1,
	SIBA_SPROMVAR_MDIO_ETH0,
	SIBA_SPROMVAR_MDIO_ETH1,
	SIBA_SPROMVAR_BREV,
	SIBA_SPROMVAR_CCODE,
	SIBA_SPROMVAR_ANT_A,
	SIBA_SPROMVAR_ANT_BG,
	SIBA_SPROMVAR_PA0B0,
	SIBA_SPROMVAR_PA0B1,
	SIBA_SPROMVAR_PA0B2,
	SIBA_SPROMVAR_PA1B0,
	SIBA_SPROMVAR_PA1B1,
	SIBA_SPROMVAR_PA1B2,
	SIBA_SPROMVAR_PA1LOB0,
	SIBA_SPROMVAR_PA1LOB1,
	SIBA_SPROMVAR_PA1LOB2,
	SIBA_SPROMVAR_PA1HIB0,
	SIBA_SPROMVAR_PA1HIB1,
	SIBA_SPROMVAR_PA1HIB2,
	SIBA_SPROMVAR_GPIO0,
	SIBA_SPROMVAR_GPIO1,
	SIBA_SPROMVAR_GPIO2,
	SIBA_SPROMVAR_GPIO3,
	SIBA_SPROMVAR_MAXPWR_AL,
	SIBA_SPROMVAR_MAXPWR_A,
	SIBA_SPROMVAR_MAXPWR_AH,
	SIBA_SPROMVAR_MAXPWR_BG,
	SIBA_SPROMVAR_RXPO2G,
	SIBA_SPROMVAR_RXPO5G,
	SIBA_SPROMVAR_TSSI_A,
	SIBA_SPROMVAR_TSSI_BG,
	SIBA_SPROMVAR_TRI2G,
	SIBA_SPROMVAR_TRI5GL,
	SIBA_SPROMVAR_TRI5G,
	SIBA_SPROMVAR_TRI5GH,
	SIBA_SPROMVAR_RSSISAV2G,
	SIBA_SPROMVAR_RSSISMC2G,
	SIBA_SPROMVAR_RSSISMF2G,
	SIBA_SPROMVAR_BXA2G,
	SIBA_SPROMVAR_RSSISAV5G,
	SIBA_SPROMVAR_RSSISMC5G,
	SIBA_SPROMVAR_RSSISMF5G,
	SIBA_SPROMVAR_BXA5G,
	SIBA_SPROMVAR_CCK2GPO,
	SIBA_SPROMVAR_OFDM2GPO,
	SIBA_SPROMVAR_OFDM5GLPO,
	SIBA_SPROMVAR_OFDM5GPO,
	SIBA_SPROMVAR_OFDM5GHPO,
	SIBA_SPROMVAR_BF_LO,
	SIBA_SPROMVAR_BF_HI,
	SIBA_SPROMVAR_BF2_LO,
	SIBA_SPROMVAR_BF2_HI,
	SIBA_SPROMVAR_FEM_2GHZ_TSSIPOS,
	SIBA_SPROMVAR_FEM_2GHZ_EXTPAGAIN,
	SIBA_SPROMVAR_FEM_2GHZ_PDET_RANGE,
	SIBA_SPROMVAR_FEM_2GHZ_TR_ISO,
	SIBA_SPROMVAR_FEM_2GHZ_ANTSWLUT,
	SIBA_SPROMVAR_FEM_5GHZ_TSSIPOS,
	SIBA_SPROMVAR_FEM_5GHZ_EXTPAGAIN,
	SIBA_SPROMVAR_FEM_5GHZ_PDET_RANGE,
	SIBA_SPROMVAR_FEM_5GHZ_TR_ISO,
	SIBA_SPROMVAR_FEM_5GHZ_ANTSWLUT,
	SIBA_SPROMVAR_TXPID_2G_0,
	SIBA_SPROMVAR_TXPID_2G_1,
	SIBA_SPROMVAR_TXPID_2G_2,
	SIBA_SPROMVAR_TXPID_2G_3,
	SIBA_SPROMVAR_TXPID_5GL_0,
	SIBA_SPROMVAR_TXPID_5GL_1,
	SIBA_SPROMVAR_TXPID_5GL_2,
	SIBA_SPROMVAR_TXPID_5GL_3,
	SIBA_SPROMVAR_TXPID_5G_0,
	SIBA_SPROMVAR_TXPID_5G_1,
	SIBA_SPROMVAR_TXPID_5G_2,
	SIBA_SPROMVAR_TXPID_5G_3,
	SIBA_SPROMVAR_TXPID_5GH_0,
	SIBA_SPROMVAR_TXPID_5GH_1,
	SIBA_SPROMVAR_TXPID_5GH_2,
	SIBA_SPROMVAR_TXPID_5GH_3,
	SIBA_SPROMVAR_STBCPO,
	SIBA_SPROMVAR_CDDPO,
};

int		siba_read_sprom(device_t, device_t, int, uintptr_t *);
int		siba_write_sprom(device_t, device_t, int, uintptr_t);

/**
 * Generic sprom accessor generation macros for siba(4) drivers
 */
#define __SPROM_ACCESSOR(varp, var, ivarp, ivar, type)			\
									\
static __inline type varp ## _get_ ## var(device_t dev)			\
{									\
	uintptr_t v;							\
	siba_read_sprom(device_get_parent(dev), dev,			\
	    ivarp ## _SPROMVAR_ ## ivar, &v);				\
	return ((type) v);						\
}									\
									\
static __inline void varp ## _set_ ## var(device_t dev, type t)		\
{									\
	uintptr_t v = (uintptr_t) t;					\
	siba_write_sprom(device_get_parent(dev), dev,			\
	    ivarp ## _SPROMVAR_ ## ivar, v);				\
}

#define	SIBA_SPROM_ACCESSOR(var, ivar, type)				\
	__SPROM_ACCESSOR(siba_sprom, var, SIBA, ivar, type)

SIBA_SPROM_ACCESSOR(rev,	REV,		uint8_t);
SIBA_SPROM_ACCESSOR(mac_80211bg,	MAC_80211BG,	uint8_t *);
SIBA_SPROM_ACCESSOR(mac_eth,	MAC_ETH,	uint8_t *);
SIBA_SPROM_ACCESSOR(mac_80211a,	MAC_80211A,	uint8_t *);
SIBA_SPROM_ACCESSOR(mii_eth0,	MII_ETH0,	uint8_t);
SIBA_SPROM_ACCESSOR(mii_eth1,	MII_ETH1,	uint8_t);
SIBA_SPROM_ACCESSOR(mdio_eth0,	MDIO_ETH0,	uint8_t);
SIBA_SPROM_ACCESSOR(mdio_eth1,	MDIO_ETH1,	uint8_t);
SIBA_SPROM_ACCESSOR(brev,	BREV,		uint8_t);
SIBA_SPROM_ACCESSOR(ccode,	CCODE,		uint8_t);
SIBA_SPROM_ACCESSOR(ant_a,	ANT_A,		uint8_t);
SIBA_SPROM_ACCESSOR(ant_bg,	ANT_BG,		uint8_t);
SIBA_SPROM_ACCESSOR(pa0b0,	PA0B0,		uint16_t);
SIBA_SPROM_ACCESSOR(pa0b1,	PA0B1,		uint16_t);
SIBA_SPROM_ACCESSOR(pa0b2,	PA0B2,		uint16_t);
SIBA_SPROM_ACCESSOR(pa1b0,	PA1B0,		uint16_t);
SIBA_SPROM_ACCESSOR(pa1b1,	PA1B1,		uint16_t);
SIBA_SPROM_ACCESSOR(pa1b2,	PA1B2,		uint16_t);
SIBA_SPROM_ACCESSOR(pa1lob0,	PA1LOB0,	uint16_t);
SIBA_SPROM_ACCESSOR(pa1lob1,	PA1LOB1,	uint16_t);
SIBA_SPROM_ACCESSOR(pa1lob2,	PA1LOB2,	uint16_t);
SIBA_SPROM_ACCESSOR(pa1hib0,	PA1HIB0,	uint16_t);
SIBA_SPROM_ACCESSOR(pa1hib1,	PA1HIB1,	uint16_t);
SIBA_SPROM_ACCESSOR(pa1hib2,	PA1HIB2,	uint16_t);
SIBA_SPROM_ACCESSOR(gpio0,	GPIO0,		uint8_t);
SIBA_SPROM_ACCESSOR(gpio1,	GPIO1,		uint8_t);
SIBA_SPROM_ACCESSOR(gpio2,	GPIO2,		uint8_t);
SIBA_SPROM_ACCESSOR(gpio3,	GPIO3,		uint8_t);
SIBA_SPROM_ACCESSOR(maxpwr_al,	MAXPWR_AL,	uint16_t);
SIBA_SPROM_ACCESSOR(maxpwr_a,	MAXPWR_A,	uint16_t);
SIBA_SPROM_ACCESSOR(maxpwr_ah,	MAXPWR_AH,	uint16_t);
SIBA_SPROM_ACCESSOR(maxpwr_bg,	MAXPWR_BG,	uint16_t);
SIBA_SPROM_ACCESSOR(rxpo2g,	RXPO2G,		uint8_t);
SIBA_SPROM_ACCESSOR(rxpo5g,	RXPO5G,		uint8_t);
SIBA_SPROM_ACCESSOR(tssi_a,	TSSI_A,		uint8_t);
SIBA_SPROM_ACCESSOR(tssi_bg,	TSSI_BG,	uint8_t);
SIBA_SPROM_ACCESSOR(tri2g,	TRI2G,		uint8_t);
SIBA_SPROM_ACCESSOR(tri5gl,	TRI5GL,		uint8_t);
SIBA_SPROM_ACCESSOR(tri5g,	TRI5G,		uint8_t);
SIBA_SPROM_ACCESSOR(tri5gh,	TRI5GH,		uint8_t);
SIBA_SPROM_ACCESSOR(rssisav2g,	RSSISAV2G,	uint8_t);
SIBA_SPROM_ACCESSOR(rssismc2g,	RSSISMC2G,	uint8_t);
SIBA_SPROM_ACCESSOR(rssismf2g,	RSSISMF2G,	uint8_t);
SIBA_SPROM_ACCESSOR(bxa2g,	BXA2G,		uint8_t);
SIBA_SPROM_ACCESSOR(rssisav5g,	RSSISAV5G,	uint8_t);
SIBA_SPROM_ACCESSOR(rssismc5g,	RSSISMC5G,	uint8_t);
SIBA_SPROM_ACCESSOR(rssismf5g,	RSSISMF5G,	uint8_t);
SIBA_SPROM_ACCESSOR(bxa5g,	BXA5G,		uint8_t);
SIBA_SPROM_ACCESSOR(cck2gpo,	CCK2GPO,	uint16_t);
SIBA_SPROM_ACCESSOR(ofdm2gpo,	OFDM2GPO,	uint32_t);
SIBA_SPROM_ACCESSOR(ofdm5glpo,	OFDM5GLPO,	uint32_t);
SIBA_SPROM_ACCESSOR(ofdm5gpo,	OFDM5GPO,	uint32_t);
SIBA_SPROM_ACCESSOR(ofdm5ghpo,	OFDM5GHPO,	uint32_t);
SIBA_SPROM_ACCESSOR(bf_lo,	BF_LO,		uint16_t);
SIBA_SPROM_ACCESSOR(bf_hi,	BF_HI,		uint16_t);
SIBA_SPROM_ACCESSOR(bf2_lo,	BF2_LO,		uint16_t);
SIBA_SPROM_ACCESSOR(bf2_hi,	BF2_HI,		uint16_t);
/* 2GHz FEM */
SIBA_SPROM_ACCESSOR(fem_2ghz_tssipos, FEM_2GHZ_TSSIPOS, uint8_t);
SIBA_SPROM_ACCESSOR(fem_2ghz_extpa_gain, FEM_2GHZ_EXTPAGAIN, uint8_t);
SIBA_SPROM_ACCESSOR(fem_2ghz_pdet_range, FEM_2GHZ_PDET_RANGE, uint8_t);
SIBA_SPROM_ACCESSOR(fem_2ghz_tr_iso, FEM_2GHZ_TR_ISO, uint8_t);
SIBA_SPROM_ACCESSOR(fem_2ghz_antswlut, FEM_2GHZ_ANTSWLUT, uint8_t);
/* 5GHz FEM */
SIBA_SPROM_ACCESSOR(fem_5ghz_tssipos, FEM_5GHZ_TSSIPOS, uint8_t);
SIBA_SPROM_ACCESSOR(fem_5ghz_extpa_gain, FEM_5GHZ_EXTPAGAIN, uint8_t);
SIBA_SPROM_ACCESSOR(fem_5ghz_pdet_range, FEM_5GHZ_PDET_RANGE, uint8_t);
SIBA_SPROM_ACCESSOR(fem_5ghz_tr_iso, FEM_5GHZ_TR_ISO, uint8_t);
SIBA_SPROM_ACCESSOR(fem_5ghz_antswlut, FEM_5GHZ_ANTSWLUT, uint8_t);
/* TX power index */
SIBA_SPROM_ACCESSOR(txpid_2g_0, TXPID_2G_0, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_2g_1, TXPID_2G_1, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_2g_2, TXPID_2G_2, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_2g_3, TXPID_2G_3, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5gl_0, TXPID_5GL_0, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5gl_1, TXPID_5GL_1, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5gl_2, TXPID_5GL_2, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5gl_3, TXPID_5GL_3, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5g_0, TXPID_5G_0, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5g_1, TXPID_5G_1, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5g_2, TXPID_5G_2, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5g_3, TXPID_5G_3, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5gh_0, TXPID_5GH_0, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5gh_1, TXPID_5GH_1, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5gh_2, TXPID_5GH_2, uint8_t);
SIBA_SPROM_ACCESSOR(txpid_5gh_3, TXPID_5GH_3, uint8_t);
SIBA_SPROM_ACCESSOR(stbcpo, STBCPO, uint16_t);
SIBA_SPROM_ACCESSOR(cddpo, CDDPO, uint16_t);

#undef SIBA_SPROM_ACCESSOR

struct siba_sprom_core_pwr_info {
	uint8_t itssi_2g, itssi_5g;
	uint8_t maxpwr_2g, maxpwr_5gl, maxpwr_5g, maxpwr_5gh;
	uint8_t pa_2g[4], pa_5gl[4], pa_5g[4], pa_5gh[4];
};

struct siba_sprom {
	uint8_t			rev;		/* revision */
	uint8_t			mac_80211bg[6];	/* address for 802.11b/g */
	uint8_t			mac_eth[6];	/* address for Ethernet */
	uint8_t			mac_80211a[6];	/* address for 802.11a */
	uint8_t			mii_eth0;	/* MII address for eth0 */
	uint8_t			mii_eth1;	/* MII address for eth1 */
	uint8_t			mdio_eth0;	/* MDIO for eth0 */
	uint8_t			mdio_eth1;	/* MDIO for eth1 */
	uint8_t			brev;		/* board revision */
	uint8_t			ccode;		/* Country Code */
	uint8_t			ant_a;		/* A-PHY antenna */
	uint8_t			ant_bg;		/* B/G-PHY antenna */
	uint16_t		pa0b0;
	uint16_t		pa0b1;
	uint16_t		pa0b2;
	uint16_t		pa1b0;
	uint16_t		pa1b1;
	uint16_t		pa1b2;
	uint16_t		pa1lob0;
	uint16_t		pa1lob1;
	uint16_t		pa1lob2;
	uint16_t		pa1hib0;
	uint16_t		pa1hib1;
	uint16_t		pa1hib2;
	uint8_t			gpio0;
	uint8_t			gpio1;
	uint8_t			gpio2;
	uint8_t			gpio3;
	uint16_t		maxpwr_al;
	uint16_t		maxpwr_a;	/* A-PHY Max Power */
	uint16_t		maxpwr_ah;
	uint16_t		maxpwr_bg;	/* BG-PHY Max Power */
	uint8_t			rxpo2g;
	uint8_t			rxpo5g;
	uint8_t			tssi_a;		/* Idle TSSI */
	uint8_t			tssi_bg;	/* Idle TSSI */
	uint8_t			tri2g;
	uint8_t			tri5gl;
	uint8_t			tri5g;
	uint8_t			tri5gh;
	uint8_t			txpid2g[4];	/* 2GHz TX power index */
	uint8_t			txpid5gl[4];	/* 4.9 - 5.1GHz TX power index */
	uint8_t			txpid5g[4];	/* 5.1 - 5.5GHz TX power index */
	uint8_t			txpid5gh[4];	/* 5.5 - 5.9GHz TX power index */
	uint8_t			rssisav2g;
	uint8_t			rssismc2g;
	uint8_t			rssismf2g;
	uint8_t			bxa2g;
	uint8_t			rssisav5g;
	uint8_t			rssismc5g;
	uint8_t			rssismf5g;
	uint8_t			bxa5g;
	uint16_t		cck2gpo;
	uint32_t		ofdm2gpo;
	uint32_t		ofdm5glpo;
	uint32_t		ofdm5gpo;
	uint32_t		ofdm5ghpo;
	uint16_t		bf_lo;		/* boardflags */
	uint16_t		bf_hi;		/* boardflags */
	uint16_t		bf2_lo;
	uint16_t		bf2_hi;

	struct siba_sprom_core_pwr_info core_pwr_info[4];

	struct {
		struct {
			int8_t a0, a1, a2, a3;
		} ghz24;
		struct {
			int8_t a0, a1, a2, a3;
		} ghz5;
	} again;	/* antenna gain */

	struct {
		struct {
			uint8_t tssipos, extpa_gain, pdet_range, tr_iso;
			uint8_t antswlut;
		} ghz2;
		struct {
			uint8_t tssipos, extpa_gain, pdet_range, tr_iso;
			uint8_t antswlut;
		} ghz5;
	} fem;

	uint16_t mcs2gpo[8];
	uint16_t mcs5gpo[8];
	uint16_t mcs5glpo[8];
	uint16_t mcs5ghpo[8];

	uint16_t cddpo;
	uint16_t stbcpo;
};

#define	SIBA_LDO_PAREF			0
#define	SIBA_LDO_VOLT1			1
#define	SIBA_LDO_VOLT2			2
#define	SIBA_LDO_VOLT3			3

struct siba_cc_pmu {
	uint8_t				rev;	/* PMU rev */
	uint32_t			freq;	/* crystal freq in kHz */
};

struct siba_cc {
	struct siba_dev_softc		*scc_dev;
	uint32_t			scc_caps;
	struct siba_cc_pmu		scc_pmu;
	uint16_t			scc_powerup_delay;
};

struct siba_pci {
	struct siba_dev_softc		*spc_dev;
	uint8_t				spc_inited;
	uint8_t				spc_hostmode;
};

struct siba_bus_ops {
	uint16_t		(*read_2)(struct siba_dev_softc *,
				    uint16_t);
	uint32_t		(*read_4)(struct siba_dev_softc *,
				    uint16_t);
	void			(*write_2)(struct siba_dev_softc *,
				    uint16_t, uint16_t);
	void			(*write_4)(struct siba_dev_softc *,
				    uint16_t, uint32_t);
	void			(*read_multi_1)(struct siba_dev_softc *,
				    void *, size_t, uint16_t);
	void			(*read_multi_2)(struct siba_dev_softc *,
				    void *, size_t, uint16_t);
	void			(*read_multi_4)(struct siba_dev_softc *,
				    void *, size_t, uint16_t);
	void			(*write_multi_1)(struct siba_dev_softc *,
				    const void *, size_t, uint16_t);
	void			(*write_multi_2)(struct siba_dev_softc *,
				    const void *, size_t, uint16_t);
	void			(*write_multi_4)(struct siba_dev_softc *,
				    const void *, size_t, uint16_t);
};

struct siba_dev_softc {
	struct siba_softc		*sd_bus;
	struct siba_devid		sd_id;
	const struct siba_bus_ops	*sd_ops;

	uint8_t				sd_coreidx;
};

struct siba_devinfo {
	struct resource_list		 sdi_rl;
	/*devhandle_t			 sdi_devhandle; XXX*/
	/*struct rman sdi_intr_rman;*/

	/* Accessors are needed for ivars below. */
	uint16_t			 sdi_vid;
	uint16_t			 sdi_devid;
	uint8_t				 sdi_rev;
	uint8_t				 sdi_idx;	/* core index on bus */
	uint8_t				 sdi_irq;	/* TODO */
};

struct siba_softc {
	/*
	 * common variables which used for siba(4) bus and siba_bwn bridge.
	 */
	device_t			siba_dev;	/* Device ID */
	struct resource			*siba_mem_res;
	bus_space_tag_t			siba_mem_bt;
	bus_space_handle_t		siba_mem_bh;
	bus_addr_t			siba_maddr;
	bus_size_t			siba_msize;
	uint8_t				siba_ncores;
	uint32_t			siba_debug;

	/*
	 * the following variables are only used for siba_bwn bridge.
	 */

	enum siba_type			siba_type;
	int				siba_invalid;

	struct siba_dev_softc		*siba_curdev;	/* only for PCI */
	struct siba_dev_softc		siba_devs[SIBA_MAX_CORES];
	int				siba_ndevs;

	uint16_t			siba_pci_vid;
	uint16_t			siba_pci_did;
	uint16_t			siba_pci_subvid;
	uint16_t			siba_pci_subdid;
	uint8_t				siba_pci_revid;
	int				siba_mem_rid;

	uint16_t			siba_chipid;	/* for CORE 0 */
	uint16_t			siba_chiprev;
	uint8_t				siba_chippkg;

	struct siba_cc			siba_cc;		/* ChipCommon */
	struct siba_pci			siba_pci;	/* PCI-core */
	const struct siba_bus_ops	*siba_ops;

	struct siba_sprom		siba_sprom;	/* SPROM */
	uint16_t			siba_spromsize;	/* in word size */
};

void		siba_powerup(device_t, int);
int		siba_powerdown(device_t);
uint16_t	siba_read_2(device_t, uint16_t);
void		siba_write_2(device_t, uint16_t, uint16_t);
uint32_t	siba_read_4(device_t, uint16_t);
void		siba_write_4(device_t, uint16_t, uint32_t);
void		siba_dev_up(device_t, uint32_t);
void		siba_dev_down(device_t, uint32_t);
int		siba_dev_isup(device_t);
void		siba_pcicore_intr(device_t);
uint32_t	siba_dma_translation(device_t);
void		siba_read_multi_1(device_t, void *, size_t, uint16_t);
void		siba_read_multi_2(device_t, void *, size_t, uint16_t);
void		siba_read_multi_4(device_t, void *, size_t, uint16_t);
void		siba_write_multi_1(device_t, const void *, size_t, uint16_t);
void		siba_write_multi_2(device_t, const void *, size_t, uint16_t);
void		siba_write_multi_4(device_t, const void *, size_t, uint16_t);
void		siba_barrier(device_t, int);
void		siba_cc_pmu_set_ldovolt(device_t, int, uint32_t);
void		siba_cc_pmu_set_ldoparef(device_t, uint8_t);
void		siba_gpio_set(device_t, uint32_t);
uint32_t	siba_gpio_get(device_t);
void		siba_fix_imcfglobug(device_t);
int		siba_sprom_get_core_power_info(device_t, int,
		    struct siba_sprom_core_pwr_info *);
int		siba_sprom_get_mcs2gpo(device_t, uint16_t *);
int		siba_sprom_get_mcs5glpo(device_t, uint16_t *);
int		siba_sprom_get_mcs5gpo(device_t, uint16_t *);
int		siba_sprom_get_mcs5ghpo(device_t, uint16_t *);
void		siba_pmu_spuravoid_pllupdate(device_t, int);
void		siba_cc_set32(device_t dev, uint32_t, uint32_t);
void		siba_cc_mask32(device_t dev, uint32_t, uint32_t);
uint32_t	siba_cc_read32(device_t dev, uint32_t);
void		siba_cc_write32(device_t dev, uint32_t, uint32_t);

#endif /* _SIBA_SIBAVAR_H_ */
