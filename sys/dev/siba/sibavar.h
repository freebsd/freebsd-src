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

enum siba_device_ivars {
	SIBA_IVAR_VENDOR,
	SIBA_IVAR_DEVICE,
	SIBA_IVAR_REVID,
	SIBA_IVAR_CORE_INDEX
};

#define	SIBA_ACCESSOR(var, ivar, type)				\
	__BUS_ACCESSOR(siba, var, SIBA, ivar, type)

SIBA_ACCESSOR(vendor,		VENDOR,		uint16_t)
SIBA_ACCESSOR(device,		DEVICE,		uint16_t)
SIBA_ACCESSOR(revid,		REVID,		uint8_t)
SIBA_ACCESSOR(core_index,	CORE_INDEX,	uint8_t)

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
	siba_read_4((scc)->scc_dev, offset)
#define	SIBA_CC_WRITE32(scc, offset, val) \
	siba_write_4((scc)->scc_dev, offset, val)
#define	SIBA_CC_MASK32(scc, offset, mask) \
	SIBA_CC_WRITE32(scc, offset, SIBA_CC_READ32(scc, offset) & (mask))
#define	SIBA_CC_SET32(scc, offset, set) \
	SIBA_CC_WRITE32(scc, offset, SIBA_CC_READ32(scc, offset) | (set))
#define	SIBA_CC_MASKSET32(scc, offset, mask, set)	\
	SIBA_CC_WRITE32(scc, offset,			\
	    (SIBA_CC_READ32(scc, offset) & (mask)) | (set))

enum siba_type {
	SIBA_TYPE_SSB,
	SIBA_TYPE_PCI,
	SIBA_TYPE_PCMCIA,
};

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
	uint8_t			gpio0;
	uint8_t			gpio1;
	uint8_t			gpio2;
	uint8_t			gpio3;
	uint16_t		maxpwr_a;	/* A-PHY Max Power */
	uint16_t		maxpwr_bg;	/* BG-PHY Max Power */
	uint8_t			tssi_a;		/* Idle TSSI */
	uint8_t			tssi_bg;	/* Idle TSSI */
	uint16_t		bf_lo;		/* boardflags */
	uint16_t		bf_hi;		/* boardflags */
	struct {
		struct {
			int8_t a0, a1, a2, a3;
		} ghz24;
		struct {
			int8_t a0, a1, a2, a3;
		} ghz5;
	} again;	/* antenna gain */
};

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
	int				siba_mem_rid;

	uint16_t			siba_chipid;	/* for CORE 0 */
	uint16_t			siba_chiprev;
	uint8_t				siba_chippkg;

	struct siba_cc			siba_cc;		/* ChipCommon */
	struct siba_pci			siba_pci;	/* PCI-core */
	const struct siba_bus_ops	*siba_ops;

	/* board informations */
	uint16_t			siba_board_vendor;
	uint16_t			siba_board_type;
	uint16_t			siba_board_rev;
	struct siba_sprom		siba_sprom;	/* SPROM */
	uint16_t			siba_spromsize;	/* in word size */
};

void		siba_powerup(struct siba_softc *, int);
uint16_t	siba_read_2(struct siba_dev_softc *, uint16_t);
void		siba_write_2(struct siba_dev_softc *, uint16_t, uint16_t);
uint32_t	siba_read_4(struct siba_dev_softc *, uint16_t);
void		siba_write_4(struct siba_dev_softc *, uint16_t, uint32_t);
void		siba_dev_up(struct siba_dev_softc *, uint32_t);
void		siba_dev_down(struct siba_dev_softc *, uint32_t);
int		siba_powerdown(struct siba_softc *);
int		siba_dev_isup(struct siba_dev_softc *);
void		siba_pcicore_intr(struct siba_pci *, struct siba_dev_softc *);
uint32_t	siba_dma_translation(struct siba_dev_softc *);
void		*siba_dma_alloc_consistent(struct siba_dev_softc *, size_t,
		    bus_addr_t *);
void		siba_read_multi_1(struct siba_dev_softc *, void *, size_t,
		    uint16_t);
void		siba_read_multi_2(struct siba_dev_softc *, void *, size_t,
		    uint16_t);
void		siba_read_multi_4(struct siba_dev_softc *, void *, size_t,
		    uint16_t);
void		siba_write_multi_1(struct siba_dev_softc *, const void *,
		    size_t, uint16_t);
void		siba_write_multi_2(struct siba_dev_softc *, const void *,
		    size_t, uint16_t);
void		siba_write_multi_4(struct siba_dev_softc *, const void *,
		    size_t, uint16_t);
void		siba_barrier(struct siba_dev_softc *, int);

#endif /* _SIBA_SIBAVAR_H_ */
