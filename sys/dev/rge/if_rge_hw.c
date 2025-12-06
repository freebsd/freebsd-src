/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019, 2020, 2023-2025 Kevin Lo <kevlo@openbsd.org>
 * Copyright (c) 2025 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Hardware programming portions from Realtek Semiconductor.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*	$OpenBSD: if_rge.c,v 1.38 2025/09/19 00:41:14 kevlo Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_media.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/mii/mii.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "if_rge_vendor.h"
#include "if_rgereg.h"
#include "if_rgevar.h"
#include "if_rge_microcode.h"
#include "if_rge_debug.h"

#include "if_rge_hw.h"

static	int rge_reset(struct rge_softc *sc);
static	void rge_set_phy_power(struct rge_softc *sc, int on);
static	uint64_t rge_mcu_get_bin_version(uint16_t entries);
static	void rge_mcu_set_version(struct rge_softc *sc, uint64_t mcodever);
static	void rge_ephy_config_mac_r25(struct rge_softc *sc);
static	void rge_ephy_config_mac_r25b(struct rge_softc *sc);
static	void rge_ephy_config_mac_r27(struct rge_softc *sc);
static	void rge_phy_config_mac_r27(struct rge_softc *sc);
static	void rge_phy_config_mac_r26(struct rge_softc *sc);
static	void rge_phy_config_mac_r25(struct rge_softc *sc);
static	void rge_phy_config_mac_r25b(struct rge_softc *sc);
static	void rge_phy_config_mac_r25d(struct rge_softc *sc);
static	void rge_phy_config_mcu(struct rge_softc *sc, uint16_t rcodever);
static	void rge_hw_init(struct rge_softc *sc);
static	void rge_disable_phy_ocp_pwrsave(struct rge_softc *sc);
static	void rge_patch_phy_mcu(struct rge_softc *sc, int set);
static	void rge_disable_hw_im(struct rge_softc *sc);
static	void rge_disable_sim_im(struct rge_softc *sc);
static	void rge_setup_sim_im(struct rge_softc *sc);
static	void rge_switch_mcu_ram_page(struct rge_softc *sc, int page);
static	int rge_exit_oob(struct rge_softc *sc);
static	void rge_write_ephy(struct rge_softc *sc, uint16_t reg, uint16_t val);
static	uint16_t rge_read_ephy(struct rge_softc *sc, uint16_t reg);
static	uint16_t rge_check_ephy_ext_add(struct rge_softc *sc, uint16_t reg);
static	void rge_r27_write_ephy(struct rge_softc *sc, uint16_t reg, uint16_t val);

static int
rge_reset(struct rge_softc *sc)
{
	int i;

	RGE_CLRBIT_4(sc, RGE_RXCFG, RGE_RXCFG_ALLPHYS | RGE_RXCFG_INDIV |
	    RGE_RXCFG_MULTI | RGE_RXCFG_BROAD | RGE_RXCFG_RUNT |
	    RGE_RXCFG_ERRPKT);

	/* Enable RXDV gate. */
	RGE_SETBIT_1(sc, RGE_PPSW, 0x08);

	RGE_SETBIT_1(sc, RGE_CMD, RGE_CMD_STOPREQ);
	if (sc->rge_type == MAC_R25) {
		for (i = 0; i < 20; i++) {
			DELAY(10);
			if (!(RGE_READ_1(sc, RGE_CMD) & RGE_CMD_STOPREQ))
				break;
		}
		if (i == 20) {
			RGE_PRINT_ERROR(sc, "failed to stop all requests\n");
			return ETIMEDOUT;
		}
	} else
		DELAY(200);

	for (i = 0; i < 3000; i++) {
		DELAY(50);
		if ((RGE_READ_1(sc, RGE_MCUCMD) & (RGE_MCUCMD_RXFIFO_EMPTY |
		    RGE_MCUCMD_TXFIFO_EMPTY)) == (RGE_MCUCMD_RXFIFO_EMPTY |
		    RGE_MCUCMD_TXFIFO_EMPTY))
			break;
	}
	if (sc->rge_type != MAC_R25) {
		for (i = 0; i < 3000; i++) {
			DELAY(50);
			if ((RGE_READ_2(sc, RGE_IM) & 0x0103) == 0x0103)
				break;
		}
	}

	RGE_WRITE_1(sc, RGE_CMD,
	    RGE_READ_1(sc, RGE_CMD) & (RGE_CMD_TXENB | RGE_CMD_RXENB));

	/* Soft reset. */
	RGE_WRITE_1(sc, RGE_CMD, RGE_CMD_RESET);

	for (i = 0; i < RGE_TIMEOUT; i++) {
		DELAY(100);
		if (!(RGE_READ_1(sc, RGE_CMD) & RGE_CMD_RESET))
			break;
	}
	if (i == RGE_TIMEOUT) {
		RGE_PRINT_ERROR(sc, "reset never completed!\n");
		return ETIMEDOUT;
	}

	return 0;
}

/**
 * @brief Do initial chip power-on and setup.
 *
 * Must be called with the driver lock held.
 */
int
rge_chipinit(struct rge_softc *sc)
{
	int error;

	RGE_ASSERT_LOCKED(sc);

	if ((error = rge_exit_oob(sc)) != 0)
		return error;
	rge_set_phy_power(sc, 1);
	rge_hw_init(sc);
	rge_hw_reset(sc);

	return 0;
}

static void
rge_set_phy_power(struct rge_softc *sc, int on)
{
	int i;

	if (on) {
		RGE_SETBIT_1(sc, RGE_PMCH, 0xc0);

		rge_write_phy(sc, 0, MII_BMCR, BMCR_AUTOEN);

		for (i = 0; i < RGE_TIMEOUT; i++) {
			if ((rge_read_phy_ocp(sc, 0xa420) & 0x0007) == 3)
				break;
			DELAY(1000);
		}
	} else {
		rge_write_phy(sc, 0, MII_BMCR, BMCR_AUTOEN | BMCR_PDOWN);
		RGE_CLRBIT_1(sc, RGE_PMCH, 0x80);
		RGE_CLRBIT_1(sc, RGE_PPSW, 0x40);
	}
}

void
rge_mac_config_mcu(struct rge_softc *sc, enum rge_mac_type type)
{
	uint64_t mcodever;
	uint16_t reg;
	int i, npages;

	if (type == MAC_R25) {
		for (npages = 0; npages < 3; npages++) {
			rge_switch_mcu_ram_page(sc, npages);
			for (i = 0; i < nitems(rtl8125_mac_bps); i++) {
				if (npages == 0)
					rge_write_mac_ocp(sc,
					    rtl8125_mac_bps[i].reg,
					    rtl8125_mac_bps[i].val);
				else if (npages == 1)
					rge_write_mac_ocp(sc,
					    rtl8125_mac_bps[i].reg, 0);
				else {
					if (rtl8125_mac_bps[i].reg < 0xf9f8)
						rge_write_mac_ocp(sc,
						    rtl8125_mac_bps[i].reg, 0);
				}
			}
			if (npages == 2) {
				rge_write_mac_ocp(sc, 0xf9f8, 0x6486);
				rge_write_mac_ocp(sc, 0xf9fa, 0x0b15);
				rge_write_mac_ocp(sc, 0xf9fc, 0x090e);
				rge_write_mac_ocp(sc, 0xf9fe, 0x1139);
			}
		}
		rge_write_mac_ocp(sc, 0xfc26, 0x8000);
		rge_write_mac_ocp(sc, 0xfc2a, 0x0540);
		rge_write_mac_ocp(sc, 0xfc2e, 0x0a06);
		rge_write_mac_ocp(sc, 0xfc30, 0x0eb8);
		rge_write_mac_ocp(sc, 0xfc32, 0x3a5c);
		rge_write_mac_ocp(sc, 0xfc34, 0x10a8);
		rge_write_mac_ocp(sc, 0xfc40, 0x0d54);
		rge_write_mac_ocp(sc, 0xfc42, 0x0e24);
		rge_write_mac_ocp(sc, 0xfc48, 0x307a);
	} else if (type == MAC_R25B) {
		rge_switch_mcu_ram_page(sc, 0);
		for (i = 0; i < nitems(rtl8125b_mac_bps); i++) {
			rge_write_mac_ocp(sc, rtl8125b_mac_bps[i].reg,
			    rtl8125b_mac_bps[i].val);
		}
	} else if (type == MAC_R25D) {
		for (npages = 0; npages < 3; npages++) {
			rge_switch_mcu_ram_page(sc, npages);

			rge_write_mac_ocp(sc, 0xf800,
			    (npages == 0) ? 0xe002 : 0);
			rge_write_mac_ocp(sc, 0xf802,
			    (npages == 0) ? 0xe006 : 0);
			rge_write_mac_ocp(sc, 0xf804,
			    (npages == 0) ? 0x4166 : 0);
			rge_write_mac_ocp(sc, 0xf806,
			    (npages == 0) ? 0x9cf6 : 0);
			rge_write_mac_ocp(sc, 0xf808,
			    (npages == 0) ? 0xc002 : 0);
			rge_write_mac_ocp(sc, 0xf80a,
			    (npages == 0) ? 0xb800 : 0);
			rge_write_mac_ocp(sc, 0xf80c,
			    (npages == 0) ? 0x14a4 : 0);
			rge_write_mac_ocp(sc, 0xf80e,
			    (npages == 0) ? 0xc102 : 0);
			rge_write_mac_ocp(sc, 0xf810,
			    (npages == 0) ? 0xb900 : 0);

			for (reg = 0xf812; reg <= 0xf9f6; reg += 2)
				rge_write_mac_ocp(sc, reg, 0);

			rge_write_mac_ocp(sc, 0xf9f8,
			    (npages == 2) ? 0x6938 : 0);
			rge_write_mac_ocp(sc, 0xf9fa,
			    (npages == 2) ? 0x0a18 : 0);
			rge_write_mac_ocp(sc, 0xf9fc,
			    (npages == 2) ? 0x0217 : 0);
			rge_write_mac_ocp(sc, 0xf9fe,
			    (npages == 2) ? 0x0d2a : 0);
		}
		rge_write_mac_ocp(sc, 0xfc26, 0x8000);
		rge_write_mac_ocp(sc, 0xfc28, 0x14a2);
		rge_write_mac_ocp(sc, 0xfc48, 0x0001);
	} else if (type == MAC_R27) {
		mcodever = rge_mcu_get_bin_version(nitems(rtl8127_mac_bps));
		if (sc->rge_mcodever != mcodever) {
		    	/* Switch to page 0. */
			rge_switch_mcu_ram_page(sc, 0);
			for (i = 0; i < 256; i++)
				rge_write_mac_ocp(sc, rtl8127_mac_bps[i].reg,
				    rtl8127_mac_bps[i].val);
		    	/* Switch to page 1. */
			rge_switch_mcu_ram_page(sc, 1);
			for (; i < nitems(rtl8127_mac_bps); i++)
				rge_write_mac_ocp(sc, rtl8127_mac_bps[i].reg,
				    rtl8127_mac_bps[i].val);
		}
		rge_write_mac_ocp(sc, 0xfc26, 0x8000);
		rge_write_mac_ocp(sc, 0xfc28, 0x1520);
		rge_write_mac_ocp(sc, 0xfc2a, 0x41e0);
		rge_write_mac_ocp(sc, 0xfc2c, 0x508c);
		rge_write_mac_ocp(sc, 0xfc2e, 0x50f6);
		rge_write_mac_ocp(sc, 0xfc30, 0x34fa);
		rge_write_mac_ocp(sc, 0xfc32, 0x0166);
		rge_write_mac_ocp(sc, 0xfc34, 0x1a6a);
		rge_write_mac_ocp(sc, 0xfc36, 0x1a2c);
		rge_write_mac_ocp(sc, 0xfc48, 0x00ff);

		/* Write microcode version. */
		rge_mcu_set_version(sc, mcodever);
	}
}

static uint64_t
rge_mcu_get_bin_version(uint16_t entries)
{
	uint64_t binver = 0;
	int i;

	for (i = 0; i < 4; i++) {
		binver <<= 16;
		binver |= rtl8127_mac_bps[entries - 4 + i].val;
	}

	return binver;
}

static void
rge_mcu_set_version(struct rge_softc *sc, uint64_t mcodever)
{
	int i;

	/* Switch to page 2. */
	rge_switch_mcu_ram_page(sc, 2);

	for (i = 0; i < 8; i += 2) {
		rge_write_mac_ocp(sc, 0xf9f8 + 6 - i, (uint16_t)mcodever);
		mcodever >>= 16;
	}

	/* Switch back to page 0. */
	rge_switch_mcu_ram_page(sc, 0);
}

void
rge_ephy_config(struct rge_softc *sc)
{
	switch (sc->rge_type) {
	case MAC_R25:
		rge_ephy_config_mac_r25(sc);
		break;
	case MAC_R25B:
		rge_ephy_config_mac_r25b(sc);
		break;
	case MAC_R27:
		rge_ephy_config_mac_r27(sc);
		break;
	default:
		break;	/* Nothing to do. */
	}
}

static void
rge_ephy_config_mac_r25(struct rge_softc *sc)
{
	uint16_t val;
	int i;

	for (i = 0; i < nitems(mac_r25_ephy); i++)
		rge_write_ephy(sc, mac_r25_ephy[i].reg, mac_r25_ephy[i].val);

	val = rge_read_ephy(sc, 0x002a) & ~0x7000;
	rge_write_ephy(sc, 0x002a, val | 0x3000);
	RGE_EPHY_CLRBIT(sc, 0x0019, 0x0040);
	RGE_EPHY_SETBIT(sc, 0x001b, 0x0e00);
	RGE_EPHY_CLRBIT(sc, 0x001b, 0x7000);
	rge_write_ephy(sc, 0x0002, 0x6042);
	rge_write_ephy(sc, 0x0006, 0x0014);
	val = rge_read_ephy(sc, 0x006a) & ~0x7000;
	rge_write_ephy(sc, 0x006a, val | 0x3000);
	RGE_EPHY_CLRBIT(sc, 0x0059, 0x0040);
	RGE_EPHY_SETBIT(sc, 0x005b, 0x0e00);
	RGE_EPHY_CLRBIT(sc, 0x005b, 0x7000);
	rge_write_ephy(sc, 0x0042, 0x6042);
	rge_write_ephy(sc, 0x0046, 0x0014);
}

static void
rge_ephy_config_mac_r25b(struct rge_softc *sc)
{
	int i;

	for (i = 0; i < nitems(mac_r25b_ephy); i++)
		rge_write_ephy(sc, mac_r25b_ephy[i].reg, mac_r25b_ephy[i].val);
}

static void
rge_ephy_config_mac_r27(struct rge_softc *sc)
{
	int i;

	for (i = 0; i < nitems(mac_r27_ephy); i++)
		rge_r27_write_ephy(sc, mac_r27_ephy[i].reg,
		    mac_r27_ephy[i].val);

	/* Clear extended address. */
	rge_write_ephy(sc, RGE_EPHYAR_EXT_ADDR, 0);
}

int
rge_phy_config(struct rge_softc *sc)
{
	uint16_t val = 0;
	int i;

	rge_ephy_config(sc);

	/* PHY reset. */
	rge_write_phy(sc, 0, MII_ANAR,
	    rge_read_phy(sc, 0, MII_ANAR) &
	    ~(ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10));
	rge_write_phy(sc, 0, MII_100T2CR,
	    rge_read_phy(sc, 0, MII_100T2CR) &
	    ~(GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX));
	switch (sc->rge_type) {
	case MAC_R27:
		val |= RGE_ADV_10000TFDX;
		/* fallthrough */
	case MAC_R26:
		val |= RGE_ADV_5000TFDX;
		/* fallthrough */
	default:
		val |= RGE_ADV_2500TFDX;
		break;
	}
	RGE_PHY_CLRBIT(sc, 0xa5d4, val);
	rge_write_phy(sc, 0, MII_BMCR, BMCR_RESET | BMCR_AUTOEN |
	    BMCR_STARTNEG);
	for (i = 0; i < 2500; i++) {
		if (!(rge_read_phy(sc, 0, MII_BMCR) & BMCR_RESET))
			break;
		DELAY(1000);
	}
	if (i == 2500) {
		RGE_PRINT_ERROR(sc, "PHY reset failed\n");
		return (ETIMEDOUT);
	}

	/* Read ram code version. */
	rge_write_phy_ocp(sc, 0xa436, 0x801e);
	sc->rge_rcodever = rge_read_phy_ocp(sc, 0xa438);

	switch (sc->rge_type) {
	case MAC_R25:
		rge_phy_config_mac_r25(sc);
		break;
	case MAC_R25B:
		rge_phy_config_mac_r25b(sc);
		break;
	case MAC_R25D:
		rge_phy_config_mac_r25d(sc);
		break;
	case MAC_R26:
		rge_phy_config_mac_r26(sc);
		break;
	case MAC_R27:
		rge_phy_config_mac_r27(sc);
		break;
	default:
		break;	/* Can't happen. */
	}

	RGE_PHY_CLRBIT(sc, 0xa5b4, 0x8000);

	/* Disable EEE. */
	RGE_MAC_CLRBIT(sc, 0xe040, 0x0003);
	if (sc->rge_type == MAC_R25) {
		RGE_MAC_CLRBIT(sc, 0xeb62, 0x0006);
		RGE_PHY_CLRBIT(sc, 0xa432, 0x0010);
	} else if (sc->rge_type == MAC_R25B || sc->rge_type == MAC_R25D)
		RGE_PHY_SETBIT(sc, 0xa432, 0x0010);

	RGE_PHY_CLRBIT(sc, 0xa5d0, (sc->rge_type == MAC_R27) ? 0x000e : 0x0006);
	RGE_PHY_CLRBIT(sc, 0xa6d4, 0x0001);
	if (sc->rge_type == MAC_R26 || sc->rge_type == MAC_R27)
		RGE_PHY_CLRBIT(sc, 0xa6d4, 0x0002);
	RGE_PHY_CLRBIT(sc, 0xa6d8, 0x0010);
	RGE_PHY_CLRBIT(sc, 0xa428, 0x0080);
	RGE_PHY_CLRBIT(sc, 0xa4a2, 0x0200);

	/* Disable advanced EEE. */
	RGE_MAC_CLRBIT(sc, 0xe052, 0x0001);
	RGE_PHY_CLRBIT(sc, 0xa442, 0x3000);
	RGE_PHY_CLRBIT(sc, 0xa430, 0x8000);

	return (0);
}

static void
rge_phy_config_mac_r27(struct rge_softc *sc)
{
	uint16_t val;
	int i;
	static const uint16_t mac_cfg_value[] =
	    { 0x815a, 0x0150, 0x81f4, 0x0150, 0x828e, 0x0150, 0x81b1, 0x0000,
	      0x824b, 0x0000, 0x82e5, 0x0000 };

	static const uint16_t mac_cfg2_value[] =
	    { 0x88d7, 0x01a0, 0x88d9, 0x01a0, 0x8ffa, 0x002a, 0x8fee, 0xffdf,
	      0x8ff0, 0xffff, 0x8ff2, 0x0a4a, 0x8ff4, 0xaa5a, 0x8ff6, 0x0a4a,
	      0x8ff8, 0xaa5a };

	static const uint16_t mac_cfg_a438_value[] =
	    { 0x003b, 0x0086, 0x00b7, 0x00db, 0x00fe, 0x00fe, 0x00fe, 0x00fe,
	      0x00c3, 0x0078, 0x0047, 0x0023 };

	rge_phy_config_mcu(sc, RGE_MAC_R27_RCODE_VER);

	rge_write_phy_ocp(sc, 0xa4d2, 0x0000);
	rge_read_phy_ocp(sc, 0xa4d4);

	RGE_PHY_CLRBIT(sc, 0xa442, 0x0800);
	rge_write_phy_ocp(sc, 0xa436, 0x8415);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x9300);
	rge_write_phy_ocp(sc, 0xa436, 0x81a3);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0f00);
	rge_write_phy_ocp(sc, 0xa436, 0x81ae);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0f00);
	rge_write_phy_ocp(sc, 0xa436, 0x81b9);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xb900);
	rge_write_phy_ocp(sc, 0xb87c, 0x83b0);
	RGE_PHY_CLRBIT(sc,0xb87e, 0x0e00);
	rge_write_phy_ocp(sc, 0xb87c, 0x83c5);
	RGE_PHY_CLRBIT(sc, 0xb87e, 0x0e00);
	rge_write_phy_ocp(sc, 0xb87c, 0x83da);
	RGE_PHY_CLRBIT(sc, 0xb87e, 0x0e00);
	rge_write_phy_ocp(sc, 0xb87c, 0x83ef);
	RGE_PHY_CLRBIT(sc, 0xb87e, 0x0e00);
	val = rge_read_phy_ocp(sc, 0xbf38) & ~0x01f0;
	rge_write_phy_ocp(sc, 0xbf38, val | 0x0160);
	val = rge_read_phy_ocp(sc, 0xbf3a) & ~0x001f;
	rge_write_phy_ocp(sc, 0xbf3a, val | 0x0014);
	RGE_PHY_CLRBIT(sc, 0xbf28, 0x6000);
	RGE_PHY_CLRBIT(sc, 0xbf2c, 0xc000);
	val = rge_read_phy_ocp(sc, 0xbf28) & ~0x1fff;
	rge_write_phy_ocp(sc, 0xbf28, val | 0x0187);
	val = rge_read_phy_ocp(sc, 0xbf2a) & ~0x003f;
	rge_write_phy_ocp(sc, 0xbf2a, val | 0x0003);
	rge_write_phy_ocp(sc, 0xa436, 0x8173);
	rge_write_phy_ocp(sc, 0xa438, 0x8620);
	rge_write_phy_ocp(sc, 0xa436, 0x8175);
	rge_write_phy_ocp(sc, 0xa438, 0x8671);
	rge_write_phy_ocp(sc, 0xa436, 0x817c);
	RGE_PHY_SETBIT(sc, 0xa438, 0x2000);
	rge_write_phy_ocp(sc, 0xa436, 0x8187);
	RGE_PHY_SETBIT(sc, 0xa438, 0x2000);
	rge_write_phy_ocp(sc, 0xA436, 0x8192);
	RGE_PHY_SETBIT(sc, 0xA438, 0x2000);
	rge_write_phy_ocp(sc, 0xA436, 0x819D);
	RGE_PHY_SETBIT(sc, 0xA438, 0x2000);
	rge_write_phy_ocp(sc, 0xA436, 0x81A8);
	RGE_PHY_CLRBIT(sc, 0xA438, 0x2000);
	rge_write_phy_ocp(sc, 0xA436, 0x81B3);
	RGE_PHY_CLRBIT(sc, 0xA438, 0x2000);
	rge_write_phy_ocp(sc, 0xA436, 0x81BE);
	RGE_PHY_SETBIT(sc, 0xA438, 0x2000);
	rge_write_phy_ocp(sc, 0xa436, 0x817d);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xa600);
	rge_write_phy_ocp(sc, 0xa436, 0x8188);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xa600);
	rge_write_phy_ocp(sc, 0xa436, 0x8193);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xa600);
	rge_write_phy_ocp(sc, 0xa436, 0x819e);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xa600);
	rge_write_phy_ocp(sc, 0xa436, 0x81a9);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1400);
	rge_write_phy_ocp(sc, 0xa436, 0x81b4);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1400);
	rge_write_phy_ocp(sc, 0xa436, 0x81bf);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xa600);
	RGE_PHY_CLRBIT(sc, 0xaeaa, 0x0028);
	rge_write_phy_ocp(sc, 0xb87c, 0x84f0);
	rge_write_phy_ocp(sc, 0xb87e, 0x201c);
	rge_write_phy_ocp(sc, 0xb87c, 0x84f2);
	rge_write_phy_ocp(sc, 0xb87e, 0x3117);
	rge_write_phy_ocp(sc, 0xaec6, 0x0000);
	rge_write_phy_ocp(sc, 0xae20, 0xffff);
	rge_write_phy_ocp(sc, 0xaece, 0xffff);
	rge_write_phy_ocp(sc, 0xaed2, 0xffff);
	rge_write_phy_ocp(sc, 0xaec8, 0x0000);
	RGE_PHY_CLRBIT(sc, 0xaed0, 0x0001);
	rge_write_phy_ocp(sc, 0xadb8, 0x0150);
	rge_write_phy_ocp(sc, 0xb87c, 0x8197);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x5000);
	rge_write_phy_ocp(sc, 0xb87c, 0x8231);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x5000);
	rge_write_phy_ocp(sc, 0xb87c, 0x82cb);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x5000);
	rge_write_phy_ocp(sc, 0xb87c, 0x82cd);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x5700);
	rge_write_phy_ocp(sc, 0xb87c, 0x8233);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x5700);
	rge_write_phy_ocp(sc, 0xb87c, 0x8199);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x5700);
	for (i = 0; i < nitems(mac_cfg_value); i+=2) {
		rge_write_phy_ocp(sc, 0xb87c, mac_cfg_value[i]);
		rge_write_phy_ocp(sc, 0xb87e, mac_cfg_value[i + 1]);
	}
	rge_write_phy_ocp(sc, 0xb87c, 0x84f7);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x2800);
	RGE_PHY_SETBIT(sc, 0xaec2, 0x1000);
	rge_write_phy_ocp(sc, 0xb87c, 0x81b3);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0xad00);
	rge_write_phy_ocp(sc, 0xb87c, 0x824d);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0xad00);
	rge_write_phy_ocp(sc, 0xb87c, 0x82e7);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0xad00);
	val = rge_read_phy_ocp(sc, 0xae4e) & ~0x000f;
	rge_write_phy_ocp(sc, 0xae4e, val | 0x0001);
	rge_write_phy_ocp(sc, 0xb87c, 0x82ce);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xf000;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x4000);
	rge_write_phy_ocp(sc, 0xb87c, 0x84ac);
	rge_write_phy_ocp(sc, 0xb87e, 0x0000);
	rge_write_phy_ocp(sc, 0xb87c, 0x84ae);
	rge_write_phy_ocp(sc, 0xb87e, 0x0000);
	rge_write_phy_ocp(sc, 0xb87c, 0x84b0);
	rge_write_phy_ocp(sc, 0xb87e, 0xf818);
	rge_write_phy_ocp(sc, 0xb87c, 0x84b2);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x6000);
	rge_write_phy_ocp(sc, 0xb87c, 0x8ffc);
	rge_write_phy_ocp(sc, 0xb87e, 0x6008);
	rge_write_phy_ocp(sc, 0xb87c, 0x8ffe);
	rge_write_phy_ocp(sc, 0xb87e, 0xf450);
	rge_write_phy_ocp(sc, 0xb87c, 0x8015);
	RGE_PHY_SETBIT(sc, 0xb87e, 0x0200);
	rge_write_phy_ocp(sc, 0xb87c, 0x8016);
	RGE_PHY_CLRBIT(sc, 0xb87e, 0x0800);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fe6);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0800);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fe4);
	rge_write_phy_ocp(sc, 0xb87e, 0x2114);
	rge_write_phy_ocp(sc, 0xb87c, 0x8647);
	rge_write_phy_ocp(sc, 0xb87e, 0xa7B1);
	rge_write_phy_ocp(sc, 0xb87c, 0x8649);
	rge_write_phy_ocp(sc, 0xb87e, 0xbbca);
	rge_write_phy_ocp(sc, 0xb87c, 0x864b);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0xdc00);
	rge_write_phy_ocp(sc, 0xb87c, 0x8154);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xc000;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x4000);
	rge_write_phy_ocp(sc, 0xb87c, 0x8158);
	RGE_PHY_CLRBIT(sc, 0xb87e, 0xc000);
	rge_write_phy_ocp(sc, 0xb87c, 0x826c);
	rge_write_phy_ocp(sc, 0xb87e, 0xffff);
	rge_write_phy_ocp(sc, 0xb87c, 0x826e);
	rge_write_phy_ocp(sc, 0xb87e, 0xffff);
	rge_write_phy_ocp(sc, 0xb87c, 0x8872);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0e00);
	rge_write_phy_ocp(sc, 0xa436, 0x8012);
	RGE_PHY_SETBIT(sc, 0xa438, 0x0800);
	rge_write_phy_ocp(sc, 0xa436, 0x8012);
	RGE_PHY_SETBIT(sc, 0xa438, 0x4000);
	RGE_PHY_SETBIT(sc, 0xb576, 0x0001);
	rge_write_phy_ocp(sc, 0xa436, 0x834a);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0700);
	rge_write_phy_ocp(sc, 0xb87c, 0x8217);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0x3f00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x2a00);
	rge_write_phy_ocp(sc, 0xa436, 0x81b1);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0b00);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fed);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x4e00);
	rge_write_phy_ocp(sc, 0xb87c, 0x88ac);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x2300);
	RGE_PHY_SETBIT(sc, 0xbf0c, 0x3800);
	rge_write_phy_ocp(sc, 0xb87c, 0x88de);
	RGE_PHY_CLRBIT(sc, 0xb87e, 0xFF00);
	rge_write_phy_ocp(sc, 0xb87c, 0x80B4);
	rge_write_phy_ocp(sc, 0xb87e, 0x5195);
	rge_write_phy_ocp(sc, 0xa436, 0x8370);
	rge_write_phy_ocp(sc, 0xa438, 0x8671);
	rge_write_phy_ocp(sc, 0xa436, 0x8372);
	rge_write_phy_ocp(sc, 0xa438, 0x86c8);
	rge_write_phy_ocp(sc, 0xa436, 0x8401);
	rge_write_phy_ocp(sc, 0xa438, 0x86c8);
	rge_write_phy_ocp(sc, 0xa436, 0x8403);
	rge_write_phy_ocp(sc, 0xa438, 0x86da);
	rge_write_phy_ocp(sc, 0xa436, 0x8406);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x1800;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x8408);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x1800;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x840a);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x1800;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x840c);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x1800;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x840e);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x1800;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x8410);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x1800;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x8412);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x1800;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x8414);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x1800;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x8416);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x1800;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x82bd);
	rge_write_phy_ocp(sc, 0xa438, 0x1f40);
	val = rge_read_phy_ocp(sc, 0xbfb4) & ~0x07ff;
	rge_write_phy_ocp(sc, 0xbfb4, val | 0x0328);
	rge_write_phy_ocp(sc, 0xbfb6, 0x3e14);
	rge_write_phy_ocp(sc, 0xa436, 0x81c4);
	for (i = 0; i < nitems(mac_cfg_a438_value); i++)
		rge_write_phy_ocp(sc, 0xa438, mac_cfg_a438_value[i]);
	for (i = 0; i < nitems(mac_cfg2_value); i+=2) {
		rge_write_phy_ocp(sc, 0xb87c, mac_cfg2_value[i]);
		rge_write_phy_ocp(sc, 0xb87e, mac_cfg2_value[i + 1]);
	}
	rge_write_phy_ocp(sc, 0xb87c, 0x88d5);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0200);
	rge_write_phy_ocp(sc, 0xa436, 0x84bb);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0a00);
	rge_write_phy_ocp(sc, 0xa436, 0x84c0);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1600);
	RGE_PHY_SETBIT(sc, 0xa430, 0x0003);
}

static void
rge_phy_config_mac_r26(struct rge_softc *sc)
{
	uint16_t val;
	int i;
	static const uint16_t mac_cfg2_a438_value[] =
	    { 0x0044, 0x00a8, 0x00d6, 0x00ec, 0x00f6, 0x00fc, 0x00fe,
	      0x00fe, 0x00bc, 0x0058, 0x002a, 0x003f, 0x3f02, 0x023c,
	      0x3b0a, 0x1c00, 0x0000, 0x0000, 0x0000, 0x0000 };

	static const uint16_t mac_cfg2_b87e_value[] =
	    { 0x03ed, 0x03ff, 0x0009, 0x03fe, 0x000b, 0x0021, 0x03f7,
	      0x03b8, 0x03e0, 0x0049, 0x0049, 0x03e0, 0x03b8, 0x03f7,
	      0x0021, 0x000b, 0x03fe, 0x0009, 0x03ff, 0x03ed, 0x000e,
	      0x03fe, 0x03ed, 0x0006, 0x001a, 0x03f1, 0x03d8, 0x0023,
	      0x0054, 0x0322, 0x00dd, 0x03ab, 0x03dc, 0x0027, 0x000e,
	      0x03e5, 0x03f9, 0x0012, 0x0001, 0x03f1 };

	rge_phy_config_mcu(sc, RGE_MAC_R26_RCODE_VER);

	RGE_PHY_SETBIT(sc, 0xa442, 0x0800);
	rge_write_phy_ocp(sc, 0xa436, 0x80bf);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xed00);
	rge_write_phy_ocp(sc, 0xa436, 0x80cd);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1000);
	rge_write_phy_ocp(sc, 0xa436, 0x80d1);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xc800);
	rge_write_phy_ocp(sc, 0xa436, 0x80d4);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xc800);
	rge_write_phy_ocp(sc, 0xa436, 0x80e1);
	rge_write_phy_ocp(sc, 0xa438, 0x10cc);
	rge_write_phy_ocp(sc, 0xa436, 0x80e5);
	rge_write_phy_ocp(sc, 0xa438, 0x4f0c);
	rge_write_phy_ocp(sc, 0xa436, 0x8387);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x4700);
	val = rge_read_phy_ocp(sc, 0xa80c) & ~0x00c0;
	rge_write_phy_ocp(sc, 0xa80c, val | 0x0080);
	RGE_PHY_CLRBIT(sc, 0xac90, 0x0010);
	RGE_PHY_CLRBIT(sc, 0xad2c, 0x8000);
	rge_write_phy_ocp(sc, 0xb87c, 0x8321);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x1100);
	RGE_PHY_SETBIT(sc, 0xacf8, 0x000c);
	rge_write_phy_ocp(sc, 0xa436, 0x8183);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x5900);
	RGE_PHY_SETBIT(sc, 0xad94, 0x0020);
	RGE_PHY_CLRBIT(sc, 0xa654, 0x0800);
	RGE_PHY_SETBIT(sc, 0xb648, 0x4000);
	rge_write_phy_ocp(sc, 0xb87c, 0x839e);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x2f00);
	rge_write_phy_ocp(sc, 0xb87c, 0x83f2);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0800);
	RGE_PHY_SETBIT(sc, 0xada0, 0x0002);
	rge_write_phy_ocp(sc, 0xb87c, 0x80f3);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x9900);
	rge_write_phy_ocp(sc, 0xb87c, 0x8126);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0xc100);
	rge_write_phy_ocp(sc, 0xb87c, 0x893a);
	rge_write_phy_ocp(sc, 0xb87e, 0x8080);
	rge_write_phy_ocp(sc, 0xb87c, 0x8647);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0xe600);
	rge_write_phy_ocp(sc, 0xb87c, 0x862c);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x1200);
	rge_write_phy_ocp(sc, 0xb87c, 0x864a);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0xe600);
	rge_write_phy_ocp(sc, 0xb87c, 0x80a0);
	rge_write_phy_ocp(sc, 0xb87e, 0xbcbc);
	rge_write_phy_ocp(sc, 0xb87c, 0x805e);
	rge_write_phy_ocp(sc, 0xb87e, 0xbcbc);
	rge_write_phy_ocp(sc, 0xb87c, 0x8056);
	rge_write_phy_ocp(sc, 0xb87e, 0x3077);
	rge_write_phy_ocp(sc, 0xb87c, 0x8058);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x5a00);
	rge_write_phy_ocp(sc, 0xb87c, 0x8098);
	rge_write_phy_ocp(sc, 0xb87e, 0x3077);
	rge_write_phy_ocp(sc, 0xb87c, 0x809a);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x5a00);
	rge_write_phy_ocp(sc, 0xb87c, 0x8052);
	rge_write_phy_ocp(sc, 0xb87e, 0x3733);
	rge_write_phy_ocp(sc, 0xb87c, 0x8094);
	rge_write_phy_ocp(sc, 0xb87e, 0x3733);
	rge_write_phy_ocp(sc, 0xb87c, 0x807f);
	rge_write_phy_ocp(sc, 0xb87e, 0x7c75);
	rge_write_phy_ocp(sc, 0xb87c, 0x803d);
	rge_write_phy_ocp(sc, 0xb87e, 0x7c75);
	rge_write_phy_ocp(sc, 0xb87c, 0x8036);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x3000);
	rge_write_phy_ocp(sc, 0xb87c, 0x8078);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x3000);
	rge_write_phy_ocp(sc, 0xb87c, 0x8031);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x3300);
	rge_write_phy_ocp(sc, 0xb87c, 0x8073);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x3300);
	val = rge_read_phy_ocp(sc, 0xae06) & ~0xfc00;
	rge_write_phy_ocp(sc, 0xae06, val | 0x7c00);
	rge_write_phy_ocp(sc, 0xb87c, 0x89D1);
	rge_write_phy_ocp(sc, 0xb87e, 0x0004);
	rge_write_phy_ocp(sc, 0xa436, 0x8fbd);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0a00);
	rge_write_phy_ocp(sc, 0xa436, 0x8fbe);
	rge_write_phy_ocp(sc, 0xa438, 0x0d09);
	rge_write_phy_ocp(sc, 0xb87c, 0x89cd);
	rge_write_phy_ocp(sc, 0xb87e, 0x0f0f);
	rge_write_phy_ocp(sc, 0xb87c, 0x89cf);
	rge_write_phy_ocp(sc, 0xb87e, 0x0f0f);
	rge_write_phy_ocp(sc, 0xb87c, 0x83a4);
	rge_write_phy_ocp(sc, 0xb87e, 0x6600);
	rge_write_phy_ocp(sc, 0xb87c, 0x83a6);
	rge_write_phy_ocp(sc, 0xb87e, 0x6601);
	rge_write_phy_ocp(sc, 0xb87c, 0x83c0);
	rge_write_phy_ocp(sc, 0xb87e, 0x6600);
	rge_write_phy_ocp(sc, 0xb87c, 0x83c2);
	rge_write_phy_ocp(sc, 0xb87e, 0x6601);
	rge_write_phy_ocp(sc, 0xb87c, 0x8414);
	rge_write_phy_ocp(sc, 0xb87e, 0x6600);
	rge_write_phy_ocp(sc, 0xb87c, 0x8416);
	rge_write_phy_ocp(sc, 0xb87e, 0x6601);
	rge_write_phy_ocp(sc, 0xb87c, 0x83f8);
	rge_write_phy_ocp(sc, 0xb87e, 0x6600);
	rge_write_phy_ocp(sc, 0xb87c, 0x83fa);
	rge_write_phy_ocp(sc, 0xb87e, 0x6601);

	rge_patch_phy_mcu(sc, 1);
	val = rge_read_phy_ocp(sc, 0xbd96) & ~0x1f00;
	rge_write_phy_ocp(sc, 0xbd96, val | 0x1000);
	val = rge_read_phy_ocp(sc, 0xbf1c) & ~0x0007;
	rge_write_phy_ocp(sc, 0xbf1c, val | 0x0007);
	RGE_PHY_CLRBIT(sc, 0xbfbe, 0x8000);
	val = rge_read_phy_ocp(sc, 0xbf40) & ~0x0380;
	rge_write_phy_ocp(sc, 0xbf40, val | 0x0280);
	val = rge_read_phy_ocp(sc, 0xbf90) & ~0x0080;
	rge_write_phy_ocp(sc, 0xbf90, val | 0x0060);
	val = rge_read_phy_ocp(sc, 0xbf90) & ~0x0010;
	rge_write_phy_ocp(sc, 0xbf90, val | 0x000c);
	rge_patch_phy_mcu(sc, 0);

	rge_write_phy_ocp(sc, 0xa436, 0x843b);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x2000);
	rge_write_phy_ocp(sc, 0xa436, 0x843d);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x2000);
	RGE_PHY_CLRBIT(sc, 0xb516, 0x007f);
	RGE_PHY_CLRBIT(sc, 0xbf80, 0x0030);

	rge_write_phy_ocp(sc, 0xa436, 0x8188);
	for (i = 0; i < 11; i++)
		rge_write_phy_ocp(sc, 0xa438, mac_cfg2_a438_value[i]);

	rge_write_phy_ocp(sc, 0xb87c, 0x8015);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0800);
	rge_write_phy_ocp(sc, 0xb87c, 0x8ffd);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fff);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x7f00);
	rge_write_phy_ocp(sc, 0xb87c, 0x8ffb);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0100);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fe9);
	rge_write_phy_ocp(sc, 0xb87e, 0x0002);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fef);
	rge_write_phy_ocp(sc, 0xb87e, 0x00a5);
	rge_write_phy_ocp(sc, 0xb87c, 0x8ff1);
	rge_write_phy_ocp(sc, 0xb87e, 0x0106);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fe1);
	rge_write_phy_ocp(sc, 0xb87e, 0x0102);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fe3);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0400);
	RGE_PHY_SETBIT(sc, 0xa654, 0x0800);
	RGE_PHY_CLRBIT(sc, 0xa654, 0x0003);
	rge_write_phy_ocp(sc, 0xac3a, 0x5851);
	val = rge_read_phy_ocp(sc, 0xac3c) & ~0xd000;
	rge_write_phy_ocp(sc, 0xac3c, val | 0x2000);
	val = rge_read_phy_ocp(sc, 0xac42) & ~0x0200;
	rge_write_phy_ocp(sc, 0xac42, val | 0x01c0);
	RGE_PHY_CLRBIT(sc, 0xac3e, 0xe000);
	RGE_PHY_CLRBIT(sc, 0xac42, 0x0038);
	val = rge_read_phy_ocp(sc, 0xac42) & ~0x0002;
	rge_write_phy_ocp(sc, 0xac42, val | 0x0005);
	rge_write_phy_ocp(sc, 0xac1a, 0x00db);
	rge_write_phy_ocp(sc, 0xade4, 0x01b5);
	RGE_PHY_CLRBIT(sc, 0xad9c, 0x0c00);
	rge_write_phy_ocp(sc, 0xb87c, 0x814b);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x1100);
	rge_write_phy_ocp(sc, 0xb87c, 0x814d);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x1100);
	rge_write_phy_ocp(sc, 0xb87c, 0x814f);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0b00);
	rge_write_phy_ocp(sc, 0xb87c, 0x8142);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0100);
	rge_write_phy_ocp(sc, 0xb87c, 0x8144);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0100);
	rge_write_phy_ocp(sc, 0xb87c, 0x8150);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0100);
	rge_write_phy_ocp(sc, 0xb87c, 0x8118);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0700);
	rge_write_phy_ocp(sc, 0xb87c, 0x811a);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0700);
	rge_write_phy_ocp(sc, 0xb87c, 0x811c);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0500);
	rge_write_phy_ocp(sc, 0xb87c, 0x810f);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0100);
	rge_write_phy_ocp(sc, 0xb87c, 0x8111);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0100);
	rge_write_phy_ocp(sc, 0xb87c, 0x811d);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0100);
	RGE_PHY_SETBIT(sc, 0xac36, 0x1000);
	RGE_PHY_CLRBIT(sc, 0xad1c, 0x0100);
	val = rge_read_phy_ocp(sc, 0xade8) & ~0xffc0;
	rge_write_phy_ocp(sc, 0xade8, val | 0x1400);
	rge_write_phy_ocp(sc, 0xb87c, 0x864b);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x9d00);

	rge_write_phy_ocp(sc, 0xa436, 0x8f97);
	for (; i < nitems(mac_cfg2_a438_value); i++)
		rge_write_phy_ocp(sc, 0xa438, mac_cfg2_a438_value[i]);

	RGE_PHY_SETBIT(sc, 0xad9c, 0x0020);
	rge_write_phy_ocp(sc, 0xb87c, 0x8122);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0c00);

	rge_write_phy_ocp(sc, 0xb87c, 0x82c8);
	for (i = 0; i < 20; i++)
		rge_write_phy_ocp(sc, 0xb87e, mac_cfg2_b87e_value[i]);

	rge_write_phy_ocp(sc, 0xb87c, 0x80ef);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0c00);

	rge_write_phy_ocp(sc, 0xb87c, 0x82a0);
	for (; i < nitems(mac_cfg2_b87e_value); i++)
		rge_write_phy_ocp(sc, 0xb87e, mac_cfg2_b87e_value[i]);

	rge_write_phy_ocp(sc, 0xa436, 0x8018);
	RGE_PHY_SETBIT(sc, 0xa438, 0x2000);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fe4);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0);
	val = rge_read_phy_ocp(sc, 0xb54c) & ~0xffc0;
	rge_write_phy_ocp(sc, 0xb54c, val | 0x3700);
}

static void
rge_phy_config_mac_r25(struct rge_softc *sc)
{
	uint16_t val;
	int i;
	static const uint16_t mac_cfg3_a438_value[] =
	    { 0x0043, 0x00a7, 0x00d6, 0x00ec, 0x00f6, 0x00fb, 0x00fd, 0x00ff,
	      0x00bb, 0x0058, 0x0029, 0x0013, 0x0009, 0x0004, 0x0002 };

	static const uint16_t mac_cfg3_b88e_value[] =
	    { 0xc091, 0x6e12, 0xc092, 0x1214, 0xc094, 0x1516, 0xc096, 0x171b,
	      0xc098, 0x1b1c, 0xc09a, 0x1f1f, 0xc09c, 0x2021, 0xc09e, 0x2224,
	      0xc0a0, 0x2424, 0xc0a2, 0x2424, 0xc0a4, 0x2424, 0xc018, 0x0af2,
	      0xc01a, 0x0d4a, 0xc01c, 0x0f26, 0xc01e, 0x118d, 0xc020, 0x14f3,
	      0xc022, 0x175a, 0xc024, 0x19c0, 0xc026, 0x1c26, 0xc089, 0x6050,
	      0xc08a, 0x5f6e, 0xc08c, 0x6e6e, 0xc08e, 0x6e6e, 0xc090, 0x6e12 };

	rge_phy_config_mcu(sc, RGE_MAC_R25_RCODE_VER);

	RGE_PHY_SETBIT(sc, 0xad4e, 0x0010);
	val = rge_read_phy_ocp(sc, 0xad16) & ~0x03ff;
	rge_write_phy_ocp(sc, 0xad16, val | 0x03ff);
	val = rge_read_phy_ocp(sc, 0xad32) & ~0x003f;
	rge_write_phy_ocp(sc, 0xad32, val | 0x0006);
	RGE_PHY_CLRBIT(sc, 0xac08, 0x1000);
	RGE_PHY_CLRBIT(sc, 0xac08, 0x0100);
	val = rge_read_phy_ocp(sc, 0xacc0) & ~0x0003;
	rge_write_phy_ocp(sc, 0xacc0, val | 0x0002);
	val = rge_read_phy_ocp(sc, 0xad40) & ~0x00e0;
	rge_write_phy_ocp(sc, 0xad40, val | 0x0040);
	val = rge_read_phy_ocp(sc, 0xad40) & ~0x0007;
	rge_write_phy_ocp(sc, 0xad40, val | 0x0004);
	RGE_PHY_CLRBIT(sc, 0xac14, 0x0080);
	RGE_PHY_CLRBIT(sc, 0xac80, 0x0300);
	val = rge_read_phy_ocp(sc, 0xac5e) & ~0x0007;
	rge_write_phy_ocp(sc, 0xac5e, val | 0x0002);
	rge_write_phy_ocp(sc, 0xad4c, 0x00a8);
	rge_write_phy_ocp(sc, 0xac5c, 0x01ff);
	val = rge_read_phy_ocp(sc, 0xac8a) & ~0x00f0;
	rge_write_phy_ocp(sc, 0xac8a, val | 0x0030);
	rge_write_phy_ocp(sc, 0xb87c, 0x8157);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0500);
	rge_write_phy_ocp(sc, 0xb87c, 0x8159);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0700);
	rge_write_phy_ocp(sc, 0xb87c, 0x80a2);
	rge_write_phy_ocp(sc, 0xb87e, 0x0153);
	rge_write_phy_ocp(sc, 0xb87c, 0x809c);
	rge_write_phy_ocp(sc, 0xb87e, 0x0153);

	rge_write_phy_ocp(sc, 0xa436, 0x81b3);
	for (i = 0; i < nitems(mac_cfg3_a438_value); i++)
		rge_write_phy_ocp(sc, 0xa438, mac_cfg3_a438_value[i]);
	for (i = 0; i < 26; i++)
		rge_write_phy_ocp(sc, 0xa438, 0);
	rge_write_phy_ocp(sc, 0xa436, 0x8257);
	rge_write_phy_ocp(sc, 0xa438, 0x020f);
	rge_write_phy_ocp(sc, 0xa436, 0x80ea);
	rge_write_phy_ocp(sc, 0xa438, 0x7843);

	rge_patch_phy_mcu(sc, 1);
	RGE_PHY_CLRBIT(sc, 0xb896, 0x0001);
	RGE_PHY_CLRBIT(sc, 0xb892, 0xff00);
	for (i = 0; i < nitems(mac_cfg3_b88e_value); i += 2) {
		rge_write_phy_ocp(sc, 0xb88e, mac_cfg3_b88e_value[i]);
		rge_write_phy_ocp(sc, 0xb890, mac_cfg3_b88e_value[i + 1]);
	}
	RGE_PHY_SETBIT(sc, 0xb896, 0x0001);
	rge_patch_phy_mcu(sc, 0);

	RGE_PHY_SETBIT(sc, 0xd068, 0x2000);
	rge_write_phy_ocp(sc, 0xa436, 0x81a2);
	RGE_PHY_SETBIT(sc, 0xa438, 0x0100);
	val = rge_read_phy_ocp(sc, 0xb54c) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb54c, val | 0xdb00);
	RGE_PHY_CLRBIT(sc, 0xa454, 0x0001);
	RGE_PHY_SETBIT(sc, 0xa5d4, 0x0020);
	RGE_PHY_CLRBIT(sc, 0xad4e, 0x0010);
	RGE_PHY_CLRBIT(sc, 0xa86a, 0x0001);
	RGE_PHY_SETBIT(sc, 0xa442, 0x0800);
	RGE_PHY_SETBIT(sc, 0xa424, 0x0008);
}

static void
rge_phy_config_mac_r25b(struct rge_softc *sc)
{
	uint16_t val;
	int i;

	rge_phy_config_mcu(sc, RGE_MAC_R25B_RCODE_VER);

	RGE_PHY_SETBIT(sc, 0xa442, 0x0800);
	val = rge_read_phy_ocp(sc, 0xac46) & ~0x00f0;
	rge_write_phy_ocp(sc, 0xac46, val | 0x0090);
	val = rge_read_phy_ocp(sc, 0xad30) & ~0x0003;
	rge_write_phy_ocp(sc, 0xad30, val | 0x0001);
	rge_write_phy_ocp(sc, 0xb87c, 0x80f5);
	rge_write_phy_ocp(sc, 0xb87e, 0x760e);
	rge_write_phy_ocp(sc, 0xb87c, 0x8107);
	rge_write_phy_ocp(sc, 0xb87e, 0x360e);
	rge_write_phy_ocp(sc, 0xb87c, 0x8551);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0800);
	val = rge_read_phy_ocp(sc, 0xbf00) & ~0xe000;
	rge_write_phy_ocp(sc, 0xbf00, val | 0xa000);
	val = rge_read_phy_ocp(sc, 0xbf46) & ~0x0f00;
	rge_write_phy_ocp(sc, 0xbf46, val | 0x0300);
	for (i = 0; i < 10; i++) {
		rge_write_phy_ocp(sc, 0xa436, 0x8044 + i * 6);
		rge_write_phy_ocp(sc, 0xa438, 0x2417);
	}
	RGE_PHY_SETBIT(sc, 0xa4ca, 0x0040);
	val = rge_read_phy_ocp(sc, 0xbf84) & ~0xe000;
	rge_write_phy_ocp(sc, 0xbf84, val | 0xa000);
	rge_write_phy_ocp(sc, 0xa436, 0x8170);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x2700;
	rge_write_phy_ocp(sc, 0xa438, val | 0xd800);
	RGE_PHY_SETBIT(sc, 0xa424, 0x0008);
}

static void
rge_phy_config_mac_r25d(struct rge_softc *sc)
{
	uint16_t val;
	int i;

	rge_phy_config_mcu(sc, RGE_MAC_R25D_RCODE_VER);

	RGE_PHY_SETBIT(sc, 0xa442, 0x0800);

	rge_patch_phy_mcu(sc, 1);
	RGE_PHY_SETBIT(sc, 0xbf96, 0x8000);
	val = rge_read_phy_ocp(sc, 0xbf94) & ~0x0007;
	rge_write_phy_ocp(sc, 0xbf94, val | 0x0005);
	val = rge_read_phy_ocp(sc, 0xbf8e) & ~0x3c00;
	rge_write_phy_ocp(sc, 0xbf8e, val | 0x2800);
	val = rge_read_phy_ocp(sc, 0xbcd8) & ~0xc000;
	rge_write_phy_ocp(sc, 0xbcd8, val | 0x4000);
	RGE_PHY_SETBIT(sc, 0xbcd8, 0xc000);
	val = rge_read_phy_ocp(sc, 0xbcd8) & ~0xc000;
	rge_write_phy_ocp(sc, 0xbcd8, val | 0x4000);
	val = rge_read_phy_ocp(sc, 0xbc80) & ~0x001f;
	rge_write_phy_ocp(sc, 0xbc80, val | 0x0004);
	RGE_PHY_SETBIT(sc, 0xbc82, 0xe000);
	RGE_PHY_SETBIT(sc, 0xbc82, 0x1c00);
	val = rge_read_phy_ocp(sc, 0xbc80) & ~0x001f;
	rge_write_phy_ocp(sc, 0xbc80, val | 0x0005);
	val = rge_read_phy_ocp(sc, 0xbc82) & ~0x00e0;
	rge_write_phy_ocp(sc, 0xbc82, val | 0x0040);
	RGE_PHY_SETBIT(sc, 0xbc82, 0x001c);
	RGE_PHY_CLRBIT(sc, 0xbcd8, 0xc000);
	val = rge_read_phy_ocp(sc, 0xbcd8) & ~0xc000;
	rge_write_phy_ocp(sc, 0xbcd8, val | 0x8000);
	RGE_PHY_CLRBIT(sc, 0xbcd8, 0xc000);
	RGE_PHY_CLRBIT(sc, 0xbd70, 0x0100);
	RGE_PHY_SETBIT(sc, 0xa466, 0x0002);
	rge_write_phy_ocp(sc, 0xa436, 0x836a);
	RGE_PHY_CLRBIT(sc, 0xa438, 0xff00);
	rge_patch_phy_mcu(sc, 0);

	rge_write_phy_ocp(sc, 0xb87c, 0x832c);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0500);
	val = rge_read_phy_ocp(sc, 0xb106) & ~0x0700;
	rge_write_phy_ocp(sc, 0xb106, val | 0x0100);
	val = rge_read_phy_ocp(sc, 0xb206) & ~0x0700;
	rge_write_phy_ocp(sc, 0xb206, val | 0x0200);
	val = rge_read_phy_ocp(sc, 0xb306) & ~0x0700;
	rge_write_phy_ocp(sc, 0xb306, val | 0x0300);
	rge_write_phy_ocp(sc, 0xb87c, 0x80cb);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0300);
	rge_write_phy_ocp(sc, 0xbcf4, 0x0000);
	rge_write_phy_ocp(sc, 0xbcf6, 0x0000);
	rge_write_phy_ocp(sc, 0xbc12, 0x0000);
	rge_write_phy_ocp(sc, 0xb87c, 0x844d);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0200);

	rge_write_phy_ocp(sc, 0xb87c, 0x8feb);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0100);
	rge_write_phy_ocp(sc, 0xb87c, 0x8fe9);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x0600);

	val = rge_read_phy_ocp(sc, 0xac7e) & ~0x01fc;
	rge_write_phy_ocp(sc, 0xac7e, val | 0x00B4);
	rge_write_phy_ocp(sc, 0xb87c, 0x8105);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x7a00);
	rge_write_phy_ocp(sc, 0xb87c, 0x8117);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x3a00);
	rge_write_phy_ocp(sc, 0xb87c, 0x8103);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x7400);
	rge_write_phy_ocp(sc, 0xb87c, 0x8115);
	val = rge_read_phy_ocp(sc, 0xb87e) & ~0xff00;
	rge_write_phy_ocp(sc, 0xb87e, val | 0x3400);
	RGE_PHY_CLRBIT(sc, 0xad40, 0x0030);
	val = rge_read_phy_ocp(sc, 0xad66) & ~0x000f;
	rge_write_phy_ocp(sc, 0xad66, val | 0x0007);
	val = rge_read_phy_ocp(sc, 0xad68) & ~0xf000;
	rge_write_phy_ocp(sc, 0xad68, val | 0x8000);
	val = rge_read_phy_ocp(sc, 0xad68) & ~0x0f00;
	rge_write_phy_ocp(sc, 0xad68, val | 0x0500);
	val = rge_read_phy_ocp(sc, 0xad68) & ~0x000f;
	rge_write_phy_ocp(sc, 0xad68, val | 0x0002);
	val = rge_read_phy_ocp(sc, 0xad6a) & ~0xf000;
	rge_write_phy_ocp(sc, 0xad6a, val | 0x7000);
	rge_write_phy_ocp(sc, 0xac50, 0x01e8);
	rge_write_phy_ocp(sc, 0xa436, 0x81fa);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x5400);
	val = rge_read_phy_ocp(sc, 0xa864) & ~0x00f0;
	rge_write_phy_ocp(sc, 0xa864, val | 0x00c0);
	val = rge_read_phy_ocp(sc, 0xa42c) & ~0x00ff;
	rge_write_phy_ocp(sc, 0xa42c, val | 0x0002);
	rge_write_phy_ocp(sc, 0xa436, 0x80e1);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0f00);
	rge_write_phy_ocp(sc, 0xa436, 0x80de);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xf000;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0700);
	RGE_PHY_SETBIT(sc, 0xa846, 0x0080);
	rge_write_phy_ocp(sc, 0xa436, 0x80ba);
	rge_write_phy_ocp(sc, 0xa438, 0x8a04);
	rge_write_phy_ocp(sc, 0xa436, 0x80bd);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xca00);
	rge_write_phy_ocp(sc, 0xa436, 0x80b7);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xb300);
	rge_write_phy_ocp(sc, 0xa436, 0x80ce);
	rge_write_phy_ocp(sc, 0xa438, 0x8a04);
	rge_write_phy_ocp(sc, 0xa436, 0x80d1);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xca00);
	rge_write_phy_ocp(sc, 0xa436, 0x80cb);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0xbb00);
	rge_write_phy_ocp(sc, 0xa436, 0x80a6);
	rge_write_phy_ocp(sc, 0xa438, 0x4909);
	rge_write_phy_ocp(sc, 0xa436, 0x80a8);
	rge_write_phy_ocp(sc, 0xa438, 0x05b8);
	rge_write_phy_ocp(sc, 0xa436, 0x8200);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x5800);
	rge_write_phy_ocp(sc, 0xa436, 0x8ff1);
	rge_write_phy_ocp(sc, 0xa438, 0x7078);
	rge_write_phy_ocp(sc, 0xa436, 0x8ff3);
	rge_write_phy_ocp(sc, 0xa438, 0x5d78);
	rge_write_phy_ocp(sc, 0xa436, 0x8ff5);
	rge_write_phy_ocp(sc, 0xa438, 0x7862);
	rge_write_phy_ocp(sc, 0xa436, 0x8ff7);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1400);

	rge_write_phy_ocp(sc, 0xa436, 0x814c);
	rge_write_phy_ocp(sc, 0xa438, 0x8455);
	rge_write_phy_ocp(sc, 0xa436, 0x814e);
	rge_write_phy_ocp(sc, 0xa438, 0x84a6);
	rge_write_phy_ocp(sc, 0xa436, 0x8163);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0600);
	rge_write_phy_ocp(sc, 0xa436, 0x816a);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0500);
	rge_write_phy_ocp(sc, 0xa436, 0x8171);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1f00);

	val = rge_read_phy_ocp(sc, 0xbc3a) & ~0x000f;
	rge_write_phy_ocp(sc, 0xbc3a, val | 0x0006);
	for (i = 0; i < 10; i++) {
		rge_write_phy_ocp(sc, 0xa436, 0x8064 + i * 3);
		RGE_PHY_CLRBIT(sc, 0xa438, 0x0700);
	}
	val = rge_read_phy_ocp(sc, 0xbfa0) & ~0xff70;
	rge_write_phy_ocp(sc, 0xbfa0, val | 0x5500);
	rge_write_phy_ocp(sc, 0xbfa2, 0x9d00);
	rge_write_phy_ocp(sc, 0xa436, 0x8165);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0x0700;
	rge_write_phy_ocp(sc, 0xa438, val | 0x0200);

	rge_write_phy_ocp(sc, 0xa436, 0x8019);
	RGE_PHY_SETBIT(sc, 0xa438, 0x0100);
	rge_write_phy_ocp(sc, 0xa436, 0x8fe3);
	rge_write_phy_ocp(sc, 0xa438, 0x0005);
	rge_write_phy_ocp(sc, 0xa438, 0x0000);
	rge_write_phy_ocp(sc, 0xa438, 0x00ed);
	rge_write_phy_ocp(sc, 0xa438, 0x0502);
	rge_write_phy_ocp(sc, 0xa438, 0x0b00);
	rge_write_phy_ocp(sc, 0xa438, 0xd401);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x2900);

	rge_write_phy_ocp(sc, 0xa436, 0x8018);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1700);

	rge_write_phy_ocp(sc, 0xa436, 0x815b);
	val = rge_read_phy_ocp(sc, 0xa438) & ~0xff00;
	rge_write_phy_ocp(sc, 0xa438, val | 0x1700);

	RGE_PHY_CLRBIT(sc, 0xa4e0, 0x8000);
	RGE_PHY_CLRBIT(sc, 0xa5d4, 0x0020);
	RGE_PHY_CLRBIT(sc, 0xa654, 0x0800);
	RGE_PHY_SETBIT(sc, 0xa430, 0x1001);
	RGE_PHY_SETBIT(sc, 0xa442, 0x0080);
}

static void
rge_phy_config_mcu(struct rge_softc *sc, uint16_t rcodever)
{
	if (sc->rge_rcodever != rcodever) {
		int i;

		rge_patch_phy_mcu(sc, 1);

		if (sc->rge_type == MAC_R25) {
			rge_write_phy_ocp(sc, 0xa436, 0x8024);
			rge_write_phy_ocp(sc, 0xa438, 0x8601);
			rge_write_phy_ocp(sc, 0xa436, 0xb82e);
			rge_write_phy_ocp(sc, 0xa438, 0x0001);

			RGE_PHY_SETBIT(sc, 0xb820, 0x0080);

			for (i = 0; i < nitems(mac_r25_mcu); i++)
				rge_write_phy_ocp(sc,
				    mac_r25_mcu[i].reg, mac_r25_mcu[i].val);

			RGE_PHY_CLRBIT(sc, 0xb820, 0x0080);

			rge_write_phy_ocp(sc, 0xa436, 0);
			rge_write_phy_ocp(sc, 0xa438, 0);
			RGE_PHY_CLRBIT(sc, 0xb82e, 0x0001);
			rge_write_phy_ocp(sc, 0xa436, 0x8024);
			rge_write_phy_ocp(sc, 0xa438, 0);
		} else if (sc->rge_type == MAC_R25B) {
			for (i = 0; i < nitems(mac_r25b_mcu); i++)
				rge_write_phy_ocp(sc,
				    mac_r25b_mcu[i].reg, mac_r25b_mcu[i].val);
		} else if (sc->rge_type == MAC_R25D) {
			for (i = 0; i < 2403; i++)
				rge_write_phy_ocp(sc,
				    mac_r25d_mcu[i].reg, mac_r25d_mcu[i].val);
			rge_patch_phy_mcu(sc, 0);

			rge_patch_phy_mcu(sc, 1);
			for (; i < 2528; i++)
				rge_write_phy_ocp(sc,
				    mac_r25d_mcu[i].reg, mac_r25d_mcu[i].val);
			rge_patch_phy_mcu(sc, 0);

			rge_patch_phy_mcu(sc, 1);
			for (; i < nitems(mac_r25d_mcu); i++)
				rge_write_phy_ocp(sc,
				    mac_r25d_mcu[i].reg, mac_r25d_mcu[i].val);
		} else if (sc->rge_type == MAC_R26) {
			for (i = 0; i < nitems(mac_r26_mcu); i++)
				rge_write_phy_ocp(sc,
				    mac_r26_mcu[i].reg, mac_r26_mcu[i].val);
		} else if (sc->rge_type == MAC_R27) {
			for (i = 0; i < 1887; i++)
				rge_write_phy_ocp(sc,
				    mac_r27_mcu[i].reg, mac_r27_mcu[i].val);
			rge_patch_phy_mcu(sc, 0);

			rge_patch_phy_mcu(sc, 1);
			for (; i < nitems(mac_r27_mcu); i++)
				rge_write_phy_ocp(sc,
				    mac_r27_mcu[i].reg, mac_r27_mcu[i].val);
		}

		rge_patch_phy_mcu(sc, 0);

		/* Write ram code version. */
		rge_write_phy_ocp(sc, 0xa436, 0x801e);
		rge_write_phy_ocp(sc, 0xa438, rcodever);
	}
}

void
rge_set_macaddr(struct rge_softc *sc, const uint8_t *addr)
{
	RGE_SETBIT_1(sc, RGE_EECMD, RGE_EECMD_WRITECFG);
	RGE_WRITE_4(sc, RGE_MAC0,
	    addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0]);
	RGE_WRITE_4(sc, RGE_MAC4,
	    addr[5] <<  8 | addr[4]);
	RGE_CLRBIT_1(sc, RGE_EECMD, RGE_EECMD_WRITECFG);
}

/**
 * @brief Read the mac address from the NIC EEPROM.
 *
 * Note this also calls rge_set_macaddr() which programs
 * it into the PPROM; I'm not sure why.
 *
 * Must be called with the driver lock held.
 */
void
rge_get_macaddr(struct rge_softc *sc, uint8_t *addr)
{
	int i;

	RGE_ASSERT_LOCKED(sc);

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		addr[i] = RGE_READ_1(sc, RGE_MAC0 + i);

	*(uint32_t *)&addr[0] = RGE_READ_4(sc, RGE_ADDR0);
	*(uint16_t *)&addr[4] = RGE_READ_2(sc, RGE_ADDR1);

	rge_set_macaddr(sc, addr);
}

/**
 * @brief MAC hardware initialisation
 *
 * Must be called with the driver lock held.
 */
static void
rge_hw_init(struct rge_softc *sc)
{
	uint16_t reg;
	int i;

	RGE_ASSERT_LOCKED(sc);

	rge_disable_aspm_clkreq(sc);
	RGE_CLRBIT_1(sc, 0xf1, 0x80);

	/* Disable UPS. */
	RGE_MAC_CLRBIT(sc, 0xd40a, 0x0010);

	/* Disable MAC MCU. */
	rge_disable_aspm_clkreq(sc);
	rge_write_mac_ocp(sc, 0xfc48, 0);
	for (reg = 0xfc28; reg < 0xfc48; reg += 2)
		rge_write_mac_ocp(sc, reg, 0);
	DELAY(3000);
	rge_write_mac_ocp(sc, 0xfc26, 0);

	/* Read microcode version. */
	rge_switch_mcu_ram_page(sc, 2);
	sc->rge_mcodever = 0;
	for (i = 0; i < 8; i += 2) {
		sc->rge_mcodever <<= 16;
		sc->rge_mcodever |= rge_read_mac_ocp(sc, 0xf9f8 + i);
	}
	rge_switch_mcu_ram_page(sc, 0);

	rge_mac_config_mcu(sc, sc->rge_type);

	/* Disable PHY power saving. */
	if (sc->rge_type == MAC_R25)
		rge_disable_phy_ocp_pwrsave(sc);

	/* Set PCIe uncorrectable error status. */
	rge_write_csi(sc, 0x108,
	    rge_read_csi(sc, 0x108) | 0x00100000);
}

void
rge_hw_reset(struct rge_softc *sc)
{
	/* Disable interrupts */
	RGE_WRITE_4(sc, RGE_IMR, 0);
	RGE_WRITE_4(sc, RGE_ISR, RGE_READ_4(sc, RGE_ISR));

	/* Clear timer interrupts. */
	RGE_WRITE_4(sc, RGE_TIMERINT0, 0);
	RGE_WRITE_4(sc, RGE_TIMERINT1, 0);
	RGE_WRITE_4(sc, RGE_TIMERINT2, 0);
	RGE_WRITE_4(sc, RGE_TIMERINT3, 0);

	rge_reset(sc);
}

static void
rge_disable_phy_ocp_pwrsave(struct rge_softc *sc)
{
	if (rge_read_phy_ocp(sc, 0xc416) != 0x0500) {
		rge_patch_phy_mcu(sc, 1);
		rge_write_phy_ocp(sc, 0xc416, 0);
		rge_write_phy_ocp(sc, 0xc416, 0x0500);
		rge_patch_phy_mcu(sc, 0);
	}
}

static void
rge_patch_phy_mcu(struct rge_softc *sc, int set)
{
	int i;

	if (set)
		RGE_PHY_SETBIT(sc, 0xb820, 0x0010);
	else
		RGE_PHY_CLRBIT(sc, 0xb820, 0x0010);

	for (i = 0; i < 1000; i++) {
		if (set) {
			if ((rge_read_phy_ocp(sc, 0xb800) & 0x0040) != 0)
				break;
		} else {
			if (!(rge_read_phy_ocp(sc, 0xb800) & 0x0040))
				break;
		}
		DELAY(100);
	}
	if (i == 1000)
		RGE_PRINT_ERROR(sc, "timeout waiting to patch phy mcu\n");
}

void
rge_config_imtype(struct rge_softc *sc, int imtype)
{
	switch (imtype) {
	case RGE_IMTYPE_NONE:
		sc->rge_intrs = RGE_INTRS;
		break;
	case RGE_IMTYPE_SIM:
		sc->rge_intrs = RGE_INTRS_TIMER;
		break;
	default:
		RGE_PRINT_ERROR(sc, "unknown imtype %d", imtype);
	}
}

void
rge_disable_aspm_clkreq(struct rge_softc *sc)
{
	int unlock = 1;

	if ((RGE_READ_1(sc, RGE_EECMD) & RGE_EECMD_WRITECFG) ==
	    RGE_EECMD_WRITECFG)
		unlock = 0;

	if (unlock)
		RGE_SETBIT_1(sc, RGE_EECMD, RGE_EECMD_WRITECFG);

	if (sc->rge_type == MAC_R26 || sc->rge_type == MAC_R27)
		RGE_CLRBIT_1(sc, RGE_INT_CFG0, 0x08);
	else
		RGE_CLRBIT_1(sc, RGE_CFG2, RGE_CFG2_CLKREQ_EN);
	RGE_CLRBIT_1(sc, RGE_CFG5, RGE_CFG5_PME_STS);

	if (unlock)
		RGE_CLRBIT_1(sc, RGE_EECMD, RGE_EECMD_WRITECFG);
}

static void
rge_disable_hw_im(struct rge_softc *sc)
{
	RGE_WRITE_2(sc, RGE_IM, 0);
}

static void
rge_disable_sim_im(struct rge_softc *sc)
{
	RGE_WRITE_4(sc, RGE_TIMERINT0, 0);
	sc->rge_timerintr = 0;
}

static void
rge_setup_sim_im(struct rge_softc *sc)
{
	RGE_WRITE_4(sc, RGE_TIMERINT0, 0x2600);
	RGE_WRITE_4(sc, RGE_TIMERCNT, 1);
	sc->rge_timerintr = 1;
}

void
rge_setup_intr(struct rge_softc *sc, int imtype)
{
	rge_config_imtype(sc, imtype);

	/* Enable interrupts. */
	RGE_WRITE_4(sc, RGE_IMR, sc->rge_intrs);

	switch (imtype) {
	case RGE_IMTYPE_NONE:
		rge_disable_sim_im(sc);
		rge_disable_hw_im(sc);
		break;
	case RGE_IMTYPE_SIM:
		rge_disable_hw_im(sc);
		rge_setup_sim_im(sc);
		break;
	default:
		RGE_PRINT_ERROR(sc, "unknown imtype %d", imtype);
	}
}

static void
rge_switch_mcu_ram_page(struct rge_softc *sc, int page)
{
	uint16_t val;

	val = rge_read_mac_ocp(sc, 0xe446) & ~0x0003;
	val |= page;
	rge_write_mac_ocp(sc, 0xe446, val);
}

static int
rge_exit_oob(struct rge_softc *sc)
{
	int error, i;

	/* Disable RealWoW. */
	rge_write_mac_ocp(sc, 0xc0bc, 0x00ff);

	if ((error = rge_reset(sc)) != 0)
		return error;

	/* Disable OOB. */
	RGE_CLRBIT_1(sc, RGE_MCUCMD, RGE_MCUCMD_IS_OOB);

	RGE_MAC_CLRBIT(sc, 0xe8de, 0x4000);

	for (i = 0; i < 10; i++) {
		DELAY(100);
		if (RGE_READ_2(sc, RGE_TWICMD) & 0x0200)
			break;
	}

	rge_write_mac_ocp(sc, 0xc0aa, 0x07d0);
	rge_write_mac_ocp(sc, 0xc0a6, 0x01b5);
	rge_write_mac_ocp(sc, 0xc01e, 0x5555);

	for (i = 0; i < 10; i++) {
		DELAY(100);
		if (RGE_READ_2(sc, RGE_TWICMD) & 0x0200)
			break;
	}

	if (rge_read_mac_ocp(sc, 0xd42c) & 0x0100) {
		for (i = 0; i < RGE_TIMEOUT; i++) {
			if ((rge_read_phy_ocp(sc, 0xa420) & 0x0007) == 2)
				break;
			DELAY(1000);
		}
		RGE_MAC_CLRBIT(sc, 0xd42c, 0x0100);
		if (sc->rge_type != MAC_R25)
			RGE_PHY_CLRBIT(sc, 0xa466, 0x0001);
		RGE_PHY_CLRBIT(sc, 0xa468, 0x000a);
	}

	return 0;
}

void
rge_write_csi(struct rge_softc *sc, uint32_t reg, uint32_t val)
{
	int i;

	RGE_WRITE_4(sc, RGE_CSIDR, val);
	RGE_WRITE_4(sc, RGE_CSIAR, (reg & RGE_CSIAR_ADDR_MASK) |
	    (RGE_CSIAR_BYTE_EN << RGE_CSIAR_BYTE_EN_SHIFT) | RGE_CSIAR_BUSY);

	for (i = 0; i < 20000; i++) {
		 DELAY(1);
		 if (!(RGE_READ_4(sc, RGE_CSIAR) & RGE_CSIAR_BUSY))
			break;
	}

	DELAY(20);
}

uint32_t
rge_read_csi(struct rge_softc *sc, uint32_t reg)
{
	int i;

	RGE_WRITE_4(sc, RGE_CSIAR, (reg & RGE_CSIAR_ADDR_MASK) |
	    (RGE_CSIAR_BYTE_EN << RGE_CSIAR_BYTE_EN_SHIFT));

	for (i = 0; i < 20000; i++) {
		 DELAY(1);
		 if (RGE_READ_4(sc, RGE_CSIAR) & RGE_CSIAR_BUSY)
			break;
	}

	DELAY(20);

	return (RGE_READ_4(sc, RGE_CSIDR));
}

void
rge_write_mac_ocp(struct rge_softc *sc, uint16_t reg, uint16_t val)
{
	uint32_t tmp;

	tmp = (reg >> 1) << RGE_MACOCP_ADDR_SHIFT;
	tmp += val;
	tmp |= RGE_MACOCP_BUSY;
	RGE_WRITE_4(sc, RGE_MACOCP, tmp);
}

uint16_t
rge_read_mac_ocp(struct rge_softc *sc, uint16_t reg)
{
	uint32_t val;

	val = (reg >> 1) << RGE_MACOCP_ADDR_SHIFT;
	RGE_WRITE_4(sc, RGE_MACOCP, val);

	return (RGE_READ_4(sc, RGE_MACOCP) & RGE_MACOCP_DATA_MASK);
}

static void
rge_write_ephy(struct rge_softc *sc, uint16_t reg, uint16_t val)
{
	uint32_t tmp;
	int i;

	tmp = (reg & RGE_EPHYAR_ADDR_MASK) << RGE_EPHYAR_ADDR_SHIFT;
	tmp |= RGE_EPHYAR_BUSY | (val & RGE_EPHYAR_DATA_MASK);
	RGE_WRITE_4(sc, RGE_EPHYAR, tmp);

	for (i = 0; i < 20000; i++) {
		DELAY(1);
		if (!(RGE_READ_4(sc, RGE_EPHYAR) & RGE_EPHYAR_BUSY))
			break;
	}

	DELAY(20);
}

static uint16_t
rge_read_ephy(struct rge_softc *sc, uint16_t reg)
{
	uint32_t val;
	int i;

	val = (reg & RGE_EPHYAR_ADDR_MASK) << RGE_EPHYAR_ADDR_SHIFT;
	RGE_WRITE_4(sc, RGE_EPHYAR, val);

	for (i = 0; i < 20000; i++) {
		DELAY(1);
		val = RGE_READ_4(sc, RGE_EPHYAR);
		if (val & RGE_EPHYAR_BUSY)
			break;
	}

	DELAY(20);

	return (val & RGE_EPHYAR_DATA_MASK);
}

static uint16_t
rge_check_ephy_ext_add(struct rge_softc *sc, uint16_t reg)
{
	uint16_t val;

	val = (reg >> 12);
	rge_write_ephy(sc, RGE_EPHYAR_EXT_ADDR, val);

	return reg & 0x0fff;
}

static void
rge_r27_write_ephy(struct rge_softc *sc, uint16_t reg, uint16_t val)
{
	rge_write_ephy(sc, rge_check_ephy_ext_add(sc, reg), val);
}

void
rge_write_phy(struct rge_softc *sc, uint16_t addr, uint16_t reg, uint16_t val)
{
	uint16_t off, phyaddr;

	phyaddr = addr ? addr : RGE_PHYBASE + (reg / 8);
	phyaddr <<= 4;

	off = addr ? reg : 0x10 + (reg % 8);

	phyaddr += (off - 16) << 1;

	rge_write_phy_ocp(sc, phyaddr, val);
}

uint16_t
rge_read_phy(struct rge_softc *sc, uint16_t addr, uint16_t reg)
{
	uint16_t off, phyaddr;

	phyaddr = addr ? addr : RGE_PHYBASE + (reg / 8);
	phyaddr <<= 4;

	off = addr ? reg : 0x10 + (reg % 8);

	phyaddr += (off - 16) << 1;

	return (rge_read_phy_ocp(sc, phyaddr));
}

void
rge_write_phy_ocp(struct rge_softc *sc, uint16_t reg, uint16_t val)
{
	uint32_t tmp;
	int i;

	tmp = (reg >> 1) << RGE_PHYOCP_ADDR_SHIFT;
	tmp |= RGE_PHYOCP_BUSY | val;
	RGE_WRITE_4(sc, RGE_PHYOCP, tmp);

	for (i = 0; i < 20000; i++) {
		DELAY(1);
		if (!(RGE_READ_4(sc, RGE_PHYOCP) & RGE_PHYOCP_BUSY))
			break;
	}
}

uint16_t
rge_read_phy_ocp(struct rge_softc *sc, uint16_t reg)
{
	uint32_t val;
	int i;

	val = (reg >> 1) << RGE_PHYOCP_ADDR_SHIFT;
	RGE_WRITE_4(sc, RGE_PHYOCP, val);

	for (i = 0; i < 20000; i++) {
		DELAY(1);
		val = RGE_READ_4(sc, RGE_PHYOCP);
		if (val & RGE_PHYOCP_BUSY)
			break;
	}

	return (val & RGE_PHYOCP_DATA_MASK);
}

int
rge_get_link_status(struct rge_softc *sc)
{
	return ((RGE_READ_2(sc, RGE_PHYSTAT) & RGE_PHYSTAT_LINK) ? 1 : 0);
}

#if 0
#ifndef SMALL_KERNEL
int
rge_wol(struct ifnet *ifp, int enable)
{
	struct rge_softc *sc = ifp->if_softc;

	if (enable) {
		if (!(RGE_READ_1(sc, RGE_CFG1) & RGE_CFG1_PM_EN)) {
			printf("%s: power management is disabled, "
			    "cannot do WOL\n", sc->sc_dev.dv_xname);
			return (ENOTSUP);
		}

	}

	rge_iff(sc);

	if (enable)
		RGE_MAC_SETBIT(sc, 0xc0b6, 0x0001);
	else
		RGE_MAC_CLRBIT(sc, 0xc0b6, 0x0001);

	RGE_SETBIT_1(sc, RGE_EECMD, RGE_EECMD_WRITECFG);
	RGE_CLRBIT_1(sc, RGE_CFG5, RGE_CFG5_WOL_LANWAKE | RGE_CFG5_WOL_UCAST |
	    RGE_CFG5_WOL_MCAST | RGE_CFG5_WOL_BCAST);
	RGE_CLRBIT_1(sc, RGE_CFG3, RGE_CFG3_WOL_LINK | RGE_CFG3_WOL_MAGIC);
	if (enable)
		RGE_SETBIT_1(sc, RGE_CFG5, RGE_CFG5_WOL_LANWAKE);
	RGE_CLRBIT_1(sc, RGE_EECMD, RGE_EECMD_WRITECFG);

	return (0);
}

void
rge_wol_power(struct rge_softc *sc)
{
	/* Disable RXDV gate. */
	RGE_CLRBIT_1(sc, RGE_PPSW, 0x08);
	DELAY(2000);

	RGE_SETBIT_1(sc, RGE_CFG1, RGE_CFG1_PM_EN);
	RGE_SETBIT_1(sc, RGE_CFG2, RGE_CFG2_PMSTS_EN);
}
#endif

#endif
