/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>.
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
#ifndef	__AR40XX_VAR_H__
#define	__AR40XX_VAR_H__

#define	AR40XX_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	AR40XX_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	AR40XX_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->sc_mtx, MA_OWNED)

/*
 * register space access macros
 */
#define	AR40XX_REG_WRITE(sc, reg, val)		do {		\
	    bus_write_4(sc->sc_ess_mem_res, (reg), (val));	\
	    } while (0)

#define	AR40XX_REG_READ(sc, reg)	bus_read_4(sc->sc_ess_mem_res, (reg))

#define	AR40XX_REG_BARRIER_WRITE(sc)	bus_barrier((sc)->sc_ess_mem_res,	\
	    0, (sc)->sc_ess_mem_size, BUS_SPACE_BARRIER_WRITE)
#define	AR40XX_REG_BARRIER_READ(sc)	bus_barrier((sc)->sc_ess_mem_res,	\
	    0, (sc)->sc_ess_mem_size, BUS_SPACE_BARRIER_READ)
#define	AR40XX_REG_BARRIER_RW(sc)	bus_barrier((sc)->sc_ess_mem_res,	\
	    0, (sc)->sc_ess_mem_size,					\
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE)

/* Size of the VLAN table itself in hardware */
#define	AR40XX_NUM_VTU_ENTRIES		64
#define	AR40XX_NUM_PORTS		6
#define	AR40XX_NUM_PHYS			5
/* Size of the ATU table in hardware */
#define	AR40XX_NUM_ATU_ENTRIES		2048

struct ar40xx_softc {
	struct mtx	sc_mtx;		/* serialize access to softc */
	device_t	sc_dev;
	uint32_t	sc_debug;

	/* ess-switch memory resource */
	struct resource	*sc_ess_mem_res;
	int		sc_ess_mem_rid;
	size_t		sc_ess_mem_size;

	/* ess-switch clock resource */
	clk_t		sc_ess_clk;

	/* ess-switch reset resource */
	hwreset_t	sc_ess_rst;

	/* phy update callout timer */
	struct callout	sc_phy_callout;

	/* memory for the ess-psgmii config interface */
	bus_space_tag_t		sc_psgmii_mem_tag;
	bus_space_handle_t	sc_psgmii_mem_handle;
	bus_size_t		sc_psgmii_mem_size;

	/* reference to the ipq4019-mdio interface */
	phandle_t		sc_mdio_phandle;
	device_t		sc_mdio_dev;

	etherswitch_info_t	sc_info;

	struct {
		uint32_t	phy_t_status;
	} sc_psgmii;

	struct {
		uint32_t switch_mac_mode;
		uint32_t switch_cpu_bmp;
		uint32_t switch_lan_bmp;
		uint32_t switch_wan_bmp;
	} sc_config;

	/* VLAN table configuration */
	struct {
		/* Whether 802.1q VLANs are enabled or not */
		bool vlan;
		/* Map etherswitch vgroup to 802.1q vlan */
		uint16_t vlan_id[AR40XX_NUM_VTU_ENTRIES];
		/* VLAN port membership */
		uint8_t vlan_ports[AR40XX_NUM_VTU_ENTRIES];
		/* VLAN port membership - untagged ports */
		uint16_t vlan_untagged[AR40XX_NUM_VTU_ENTRIES];
		/* PVID for each port - index into vlan_id[] */
		uint16_t pvid[AR40XX_NUM_PORTS];
	} sc_vlan;

	struct {
		bool mirror_rx;
		bool mirror_tx;
		int source_port;
		int monitor_port;
	} sc_monitor;

	struct {
		char *ifname[AR40XX_NUM_PHYS];
		device_t miibus[AR40XX_NUM_PHYS];
		struct ifnet *ifp[AR40XX_NUM_PHYS];
	} sc_phys;

	/* ATU (address table unit) support */
	struct {
		int count;
		int size;
		etherswitch_atu_entry_t entries[AR40XX_NUM_ATU_ENTRIES];
	} atu;
};

#endif	/* __AR40XX_VAR_H__ */

