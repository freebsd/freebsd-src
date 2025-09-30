/*-
 * Copyright (c) 2023 Priit Trees
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mdio/mdio.h>

#include <dev/etherswitch/etherswitch.h>
#include <dev/etherswitch/mtkswitch/mtkswitchvar.h>
#include <dev/etherswitch/mtkswitch/mtkswitch_mt7531.h>

#include "etherswitch_if.h"
#include "miibus_if.h"
#include "mdio_if.h"

#define MDIO_READ(dev, addr, reg)                                       \
    MDIO_READREG(device_get_parent(dev), (addr), (reg))
#define MDIO_WRITE(dev, addr, reg, val)                                 \
    MDIO_WRITEREG(device_get_parent(dev), (addr), (reg), (val))

static int
mtkswitch_phy_read_locked(struct mtkswitch_softc *sc, int phy, int reg)
{
	uint32_t data;

	while (sc->hal.mtkswitch_read(sc, MTKSWITCH_PIAC) & PIAC_PHY_ACS_ST);

	sc->hal.mtkswitch_write(sc, MTKSWITCH_PIAC,
	    PIAC_PHY_ACS_ST | PIAC_MDIO_ST | (reg << PIAC_MDIO_REG_ADDR_OFF) |
	    (phy << PIAC_MDIO_PHY_ADDR_OFF) | PIAC_MDIO_CMD_READ);

	while ((data = sc->hal.mtkswitch_read(sc,MTKSWITCH_PIAC)) & PIAC_PHY_ACS_ST);

	return ((int)(data & PIAC_MDIO_RW_DATA_MASK));

}

static int
mtkswitch_phy_read(device_t dev, int phy, int reg)
{
	struct mtkswitch_softc *sc = device_get_softc(dev);
	int data;

	if ((phy < 0 || phy >= 32) || (reg < 0 || reg >= 32))
		return (ENXIO);

	MTKSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);
	MTKSWITCH_LOCK(sc);
	data = mtkswitch_phy_read_locked(sc, phy, reg);
	MTKSWITCH_UNLOCK(sc);

	return (data);
}

static int
mtkswitch_phy_write_locked(struct mtkswitch_softc *sc, int phy, int reg,
    int val)
{
	sc->hal.mtkswitch_write(sc, MTKSWITCH_PIAC,
	    PIAC_PHY_ACS_ST | PIAC_MDIO_ST | (reg << PIAC_MDIO_REG_ADDR_OFF) |
	    (phy << PIAC_MDIO_PHY_ADDR_OFF) | PIAC_MDIO_CMD_WRITE |
	    (val & PIAC_MDIO_RW_DATA_MASK));
	while (sc->hal.mtkswitch_read(sc, MTKSWITCH_PIAC) & PIAC_PHY_ACS_ST);

	return (0);
}

static int
mtkswitch_phy_write(device_t dev, int phy, int reg, int val)
{
	struct mtkswitch_softc *sc = device_get_softc(dev);
	int res;

	if ((phy < 0 || phy >= 32) || (reg < 0 || reg >= 32))
		return (ENXIO);

	MTKSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);
	MTKSWITCH_LOCK(sc);
	res = mtkswitch_phy_write_locked(sc, phy, reg, val);
	MTKSWITCH_UNLOCK(sc);

	return (res);
}

static uint32_t
mtkswitch_reg_read32(struct mtkswitch_softc *sc, int reg)
{
	uint32_t low, hi;

	MDIO_WRITE(sc->sc_dev, MTKSWITCH_GLOBAL_PHY,
	    MTKSWITCH_GLOBAL_REG, MTKSWITCH_REG_ADDR(reg));
	low = MDIO_READ(sc->sc_dev, MTKSWITCH_GLOBAL_PHY,
	    MTKSWITCH_REG_LO(reg));
	hi = MDIO_READ(sc->sc_dev, MTKSWITCH_GLOBAL_PHY,
	    MTKSWITCH_REG_HI(reg));
	return (low | (hi << 16));
}

static uint32_t
mtkswitch_reg_write32(struct mtkswitch_softc *sc, int reg, uint32_t val)
{

	MDIO_WRITE(sc->sc_dev, MTKSWITCH_GLOBAL_PHY,
	    MTKSWITCH_GLOBAL_REG, MTKSWITCH_REG_ADDR(reg));
	MDIO_WRITE(sc->sc_dev, MTKSWITCH_GLOBAL_PHY,
	    MTKSWITCH_REG_LO(reg), MTKSWITCH_VAL_LO(val));
	MDIO_WRITE(sc->sc_dev, MTKSWITCH_GLOBAL_PHY,
	    MTKSWITCH_REG_HI(reg), MTKSWITCH_VAL_HI(val));
	return (0);
}

static int
mtkswitch_reg_read(device_t dev, int reg)
{

	struct mtkswitch_softc *sc = device_get_softc(dev);
	uint32_t val;

	val = sc->hal.mtkswitch_read(sc, MTKSWITCH_REG32(reg));
	return val;
}

static int
mtkswitch_reg_write(device_t dev, int reg, int val)
{
	struct mtkswitch_softc *sc = device_get_softc(dev);
	sc->hal.mtkswitch_write(sc, MTKSWITCH_REG32(reg), val);

	return (0);
}

static int
mtkswitch_reset(struct mtkswitch_softc *sc)
{

	/* We don't reset the switch for now */
	return (0);
}

static void
mtkswitch_setup_xtal(struct mtkswitch_softc *sc, uint32_t xtal)
{
	uint32_t val;

	/* Step 1 : Disable MT7531 COREPLL */
	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP);
	val &= ~PLLGP_COREPLL;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP, val);

	/* Step 2: switch to XTAL output */
	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP);
	val |= PLLGP_SW_CLKSW;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP, val);

	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP_CR0);
	val &= ~PLLGP_RG_COREPLL;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP_CR0, val);

	/* Step 3: disable PLLGP and enable program PLLGP */
	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP);
	val |= PLLGP_SW_PLLGP;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP, val);

	/* Step 4: program COREPLL output frequency to 500MHz */
	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP_CR0);
	val &= ~PLLGP_RG_COREPLL_POSDIV_M;
	val |= 2 << PLLGP_RG_COREPLL_POSDIV_S;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP_CR0, val);
	DELAY(35);

	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP_CR0);
	val &= ~PLLGP_RG_COREPLL_SDM_PCW_M;
	val |= xtal << PLLGP_RG_COREPLL_SDM_PCW_S;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP_CR0, val);

	/* Set feedback divide ratio update signal to high */
	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP_CR0);
	val |= PLLGP_RG_COREPLL_SDM_PCW_CHG;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP_CR0, val);
	/* Wait for at least 16 XTAL clocks */
	DELAY(20);

	/* Step 5: set feedback divide ratio update signal to low */
	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP_CR0);
	val &= ~PLLGP_RG_COREPLL_SDM_PCW_CHG;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP_CR0, val);

	/* Enable 325M clock for SGMII */
	sc->hal.mtkswitch_write(sc, MT7531_ANA_PLLGP_CR5, 0xad0000);

	/* Enable 250SSC clock for RGMII */
	sc->hal.mtkswitch_write(sc, MT7531_ANA_PLLGP_CR2, 0x4f40000);

	/* Step 6: Enable MT7531 PLL */
	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP_CR0);
	val |= PLLGP_RG_COREPLL;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP_CR0, val);

	val = sc->hal.mtkswitch_read(sc, MT7531_PLLGP);
	val |= PLLGP_COREPLL;
	sc->hal.mtkswitch_write(sc, MT7531_PLLGP, val);
	DELAY(35);
}

static int
mtkswitch_hw_setup(struct mtkswitch_softc *sc)
{

	/*
	 * TODO: parse the device tree and see if we need to configure
	 *       ports, etc. differently. For now we fallback to defaults.
	 */

	uint32_t crev, topsig, val, xtal;

	crev = sc->hal.mtkswitch_read(sc, MTKSWITCH_CREV);
	topsig  = sc->hal.mtkswitch_read(sc, MT7531_TOP_SIG_SR);

	/* Print chip name and revision. In future need
	 * to move other place and mayby can use it.
	 */
	device_printf(sc->sc_dev, "chip %s rev 0x%x\n",
		topsig & PAD_DUAL_SGMII ? "MT7531AE" : "MT7531BE",
		CREV_CHIP_REV(crev));

	/* MT7531AE has got two SGMII units. One for port 5, one for port 6.
	 * MT7531BE has got only one SGMII unit which is for port 6.
	 */
	if (topsig & PAD_DUAL_SGMII)
		return (0);

	/* on linux rev is checked. if it bigger than 0
	 * and SMI is enabled then 40MHz other 25MHz
	 * else hwstrap is set then 25MHz other 40MHz
	 */

	val = sc->hal.mtkswitch_read(sc, MTKSWITCH_STRAP);
	if (val & STRAP_XTAL)	/* starp is set 25MHz */
		xtal = MT7531_XTAL_25MHZ;
	else 	/* starp is not set. Then is used 40MHz */
		xtal = MT7531_XTAL_40MHZ;

	mtkswitch_setup_xtal(sc, xtal);

	/* Called early and hence unlocked */
	return (0);
}

static int
mtkswitch_hw_global_setup(struct mtkswitch_softc *sc)
{
	/* Currently does nothing */

	/* Called early and hence unlocked */
	return (0);
}

static void
mtkswitch_port_init(struct mtkswitch_softc *sc, int port)
{
	uint32_t val;

	/* Called early and hence unlocked */

	/* Set the port to secure mode */
	val = sc->hal.mtkswitch_read(sc, MTKSWITCH_PCR(port));
	val |= PCR_PORT_VLAN_SECURE;
	sc->hal.mtkswitch_write(sc, MTKSWITCH_PCR(port), val);

	/* Set port's vlan_attr to user port */
	val = sc->hal.mtkswitch_read(sc, MTKSWITCH_PVC(port));
	val &= ~PVC_VLAN_ATTR_MASK;
	sc->hal.mtkswitch_write(sc, MTKSWITCH_PVC(port), val);

	val = PMCR_CFG_DEFAULT;
	if (port == sc->cpuport){
		val |= PMCR_FORCE_LINK | PMCR_FORCE_DPX | PMCR_FORCE_SPD_1000 |
		    MT7631_PMCR_FORCE_MODE | PMCR_MAC_MODE;
	}
	/* Set port's MAC to default settings */
	sc->hal.mtkswitch_write(sc, MTKSWITCH_PMCR(port), val);
}

static uint32_t
mtkswitch_get_port_status(struct mtkswitch_softc *sc, int port)
{
	uint32_t val, res, tmp;

	MTKSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	res = 0;
	val = sc->hal.mtkswitch_read(sc, MTKSWITCH_PMSR(port));

	if (val & PMSR_MAC_LINK_STS)
		res |= MTKSWITCH_LINK_UP;
	if (val & PMSR_MAC_DPX_STS)
		res |= MTKSWITCH_DUPLEX;
	tmp = PMSR_MAC_SPD(val);
	if (tmp == 0)
		res |= MTKSWITCH_SPEED_10;
	else if (tmp == 1)
		res |= MTKSWITCH_SPEED_100;
	else if (tmp == 2)
		res |= MTKSWITCH_SPEED_1000;
	if (val & PMSR_TX_FC_STS)
		res |= MTKSWITCH_TXFLOW;
	if (val & PMSR_RX_FC_STS)
		res |= MTKSWITCH_RXFLOW;

	return (res);
}

static int
mtkswitch_atu_flush(struct mtkswitch_softc *sc)
{

	MTKSWITCH_LOCK_ASSERT(sc, MA_OWNED);

	/* Flush all non-static MAC addresses */
	while (sc->hal.mtkswitch_read(sc, MTKSWITCH_ATC) & ATC_BUSY);
	sc->hal.mtkswitch_write(sc, MTKSWITCH_ATC, ATC_BUSY |
	    ATC_AC_MAT_NON_STATIC_MACS | ATC_AC_CMD_CLEAN);
	while (sc->hal.mtkswitch_read(sc, MTKSWITCH_ATC) & ATC_BUSY);

	return (0);
}

static int
mtkswitch_port_vlan_setup(struct mtkswitch_softc *sc, etherswitch_port_t *p)
{
	int err;

	/*
	 * Port behaviour wrt tag/untag/stack is currently defined per-VLAN.
	 * So we say we don't support it here.
	 */
	if ((p->es_flags & (ETHERSWITCH_PORT_DOUBLE_TAG |
	    ETHERSWITCH_PORT_ADDTAG | ETHERSWITCH_PORT_STRIPTAG)) != 0)
		return (ENOTSUP);

	MTKSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);
	MTKSWITCH_LOCK(sc);

	/* Set the PVID */
	if (p->es_pvid != 0) {
		err = sc->hal.mtkswitch_vlan_set_pvid(sc, p->es_port,
		    p->es_pvid);
		if (err != 0) {
			MTKSWITCH_UNLOCK(sc);
			return (err);
		}
	}

	MTKSWITCH_UNLOCK(sc);

	return (0);
}

static int
mtkswitch_port_vlan_get(struct mtkswitch_softc *sc, etherswitch_port_t *p)
{

	MTKSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);
	MTKSWITCH_LOCK(sc);

	/* Retrieve the PVID */
	sc->hal.mtkswitch_vlan_get_pvid(sc, p->es_port, &p->es_pvid);

	/*
	 * Port flags are not supported at the moment.
	 * Port's tag/untag/stack behaviour is defined per-VLAN.
	 */
	p->es_flags = 0;

	MTKSWITCH_UNLOCK(sc);

	return (0);
}

static void
mtkswitch_invalidate_vlan(struct mtkswitch_softc *sc, uint32_t vid)
{

	while (sc->hal.mtkswitch_read(sc, MTKSWITCH_VTCR) & VTCR_BUSY);
	sc->hal.mtkswitch_write(sc, MTKSWITCH_VTCR, VTCR_BUSY |
	    VTCR_FUNC_VID_INVALID | (vid & VTCR_VID_MASK));
	while (sc->hal.mtkswitch_read(sc, MTKSWITCH_VTCR) & VTCR_BUSY);
}

static int
mtkswitch_update_vlan_entry(struct mtkswitch_softc *sc, uint16_t vid,
    uint8_t members, uint16_t untag)
{
	uint32_t val;

	while (sc->hal.mtkswitch_read(sc, MTKSWITCH_VTCR) & VTCR_BUSY);

	/* We use FID 0 */
	val = VAWD1_IVL_MAC | VAWD1_VTAG_EN | VAWD1_VALID |
	    ((members & VAWD1_MEMBER_MASK) << VAWD1_MEMBER_OFF);
	sc->hal.mtkswitch_write(sc, MTKSWITCH_VAWD1, val);

	/* Set tagged ports */
	sc->hal.mtkswitch_write(sc, MTKSWITCH_VAWD2, (untag &0xFFFF));

	/* Write the VLAN entry */
	sc->hal.mtkswitch_write(sc, MTKSWITCH_VTCR, VTCR_BUSY |
	    VTCR_FUNC_VID_WRITE | (vid & VTCR_VID_MASK));
	while ((val = sc->hal.mtkswitch_read(sc, MTKSWITCH_VTCR)) & VTCR_BUSY);
	return (val);
}

static void
mtkswitch_vlan_init_hw(struct mtkswitch_softc *sc)
{
	uint8_t members = 0;
	uint32_t i;

	MTKSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);
	MTKSWITCH_LOCK(sc);
	/* Reset all VLANs to defaults first */
	for (i = 0; i < sc->info.es_nvlangroups; i++) {
		mtkswitch_invalidate_vlan(sc, i);
	}

	/* Now, add all ports as untagged members of VLAN 1 */
	for (i = 0; i < sc->info.es_nports; i++)
		members |= ((1u)<<(i));

	mtkswitch_update_vlan_entry(sc, 1, members, 0);

	/* Reset internal VLAN table. */
	for (i = 0; i < nitems(sc->vlans); i++)
		sc->vlans[i] = 0;

	sc->vlans[0] = 1;

	/* Set all port PVIDs to 1 */
	for (i = 0; i < sc->info.es_nports; i++) {
		sc->hal.mtkswitch_vlan_set_pvid(sc, i, 1);
	}

	MTKSWITCH_UNLOCK(sc);
}

static int
mtkswitch_vlan_getvgroup(struct mtkswitch_softc *sc, etherswitch_vlangroup_t *v)
{
	uint32_t val, i;
	MTKSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);

	/* Reset the member ports. */
	v->es_untagged_ports = 0;
	v->es_member_ports = 0;

	/* Not supported for now */
	v->es_fid = 0;

	MTKSWITCH_LOCK(sc);
	v->es_vid = sc->vlans[v->es_vlangroup];

	if (v->es_vid == 0)
	{
		MTKSWITCH_UNLOCK(sc);
		return (0);
	}
	while (sc->hal.mtkswitch_read(sc, MTKSWITCH_VTCR) & VTCR_BUSY);
	sc->hal.mtkswitch_write(sc, MTKSWITCH_VTCR, VTCR_BUSY |
	    VTCR_FUNC_VID_READ | (v->es_vid & VTCR_VID_MASK));
	while ((val = sc->hal.mtkswitch_read(sc, MTKSWITCH_VTCR)) & VTCR_BUSY);
	if (val & VTCR_IDX_INVALID) {
		MTKSWITCH_UNLOCK(sc);
		return (0);
	}

	val = sc->hal.mtkswitch_read(sc, MTKSWITCH_VAWD1);
	if (val & VAWD1_VALID)
		v->es_vid |= ETHERSWITCH_VID_VALID;
	else {
		MTKSWITCH_UNLOCK(sc);
		return (0);
	}
	v->es_member_ports = (val >> VAWD1_MEMBER_OFF) & VAWD1_MEMBER_MASK;

	val = sc->hal.mtkswitch_read(sc, MTKSWITCH_VAWD2);
	for (i = 0; i < sc->info.es_nports; i++) {
		if ((val & VAWD2_PORT_MASK(i)) == VAWD2_PORT_UNTAGGED(i))
			v->es_untagged_ports |= (1<<i);
	}

	MTKSWITCH_UNLOCK(sc);
	return (0);
}

static int
mtkswitch_vlan_setvgroup(struct mtkswitch_softc *sc, etherswitch_vlangroup_t *v)
{
	uint16_t untagged_ports = 0;
	uint32_t val;
	int i, vlan;

	MTKSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);

	vlan = v->es_vid & ETHERSWITCH_VID_MASK;
	if (vlan == 0)
	{
		mtkswitch_invalidate_vlan(sc, sc->vlans[v->es_vlangroup]);
		sc->vlans[v->es_vlangroup] = 0;
		return (0);
	}

       /* Is this VLAN already in table ? */
	for (i = 0; i < sc->info.es_nvlangroups; i++)
		if (i != v->es_vlangroup && vlan == sc->vlans[i])
			return (EINVAL);

	sc->vlans[v->es_vlangroup] = vlan;

	/* We currently don't support FID */
	if (v->es_fid != 0)
		return (EINVAL);

	MTKSWITCH_LOCK(sc);
	/* Set tagged ports and Write the VLAN entry*/
	for (i = 0; i < sc->info.es_nports; i++)
		if (((1<<i) & v->es_untagged_ports) == 0)
			untagged_ports |= VAWD2_PORT_TAGGED(i);

	val = mtkswitch_update_vlan_entry(sc, v->es_vid, v->es_member_ports,
	     untagged_ports);
	MTKSWITCH_UNLOCK(sc);

	if (val & VTCR_IDX_INVALID)
		return (EINVAL);

	return (0);
}

static int
mtkswitch_vlan_get_pvid(struct mtkswitch_softc *sc, int port, int *pvid)
{

	MTKSWITCH_LOCK_ASSERT(sc, MA_OWNED);

	*pvid = sc->hal.mtkswitch_read(sc, MTKSWITCH_PPBV1(port));
	*pvid = PPBV_VID_FROM_REG(*pvid);

	return (0);
}

static int
mtkswitch_vlan_set_pvid(struct mtkswitch_softc *sc, int port, int pvid)
{
	uint32_t val;

	MTKSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	val = PPBV_VID(pvid & PPBV_VID_MASK);
	sc->hal.mtkswitch_write(sc, MTKSWITCH_PPBV1(port), val);

	return (0);
}

extern void
mtk_attach_switch_mt7631(struct mtkswitch_softc *sc)
{

	sc->portmap = 0x7f;
	sc->phymap = 0x1e;

	sc->info.es_nports = 7;
	sc->info.es_vlan_caps = ETHERSWITCH_VLAN_DOT1Q;
	sprintf(sc->info.es_name, "Mediatek GSW");
	sc->hal.mtkswitch_read = mtkswitch_reg_read32;
	sc->hal.mtkswitch_write = mtkswitch_reg_write32;
	sc->info.es_nvlangroups = 4096;
	sc->hal.mtkswitch_reset = mtkswitch_reset;
	sc->hal.mtkswitch_hw_setup = mtkswitch_hw_setup;
	sc->hal.mtkswitch_hw_global_setup = mtkswitch_hw_global_setup;
	sc->hal.mtkswitch_port_init = mtkswitch_port_init;
	sc->hal.mtkswitch_get_port_status = mtkswitch_get_port_status;
	sc->hal.mtkswitch_atu_flush = mtkswitch_atu_flush;
	sc->hal.mtkswitch_port_vlan_setup = mtkswitch_port_vlan_setup;
	sc->hal.mtkswitch_port_vlan_get = mtkswitch_port_vlan_get;
	sc->hal.mtkswitch_vlan_init_hw = mtkswitch_vlan_init_hw;
	sc->hal.mtkswitch_vlan_getvgroup = mtkswitch_vlan_getvgroup;
	sc->hal.mtkswitch_vlan_setvgroup = mtkswitch_vlan_setvgroup;
	sc->hal.mtkswitch_vlan_get_pvid = mtkswitch_vlan_get_pvid;
	sc->hal.mtkswitch_vlan_set_pvid = mtkswitch_vlan_set_pvid;
	sc->hal.mtkswitch_phy_read = mtkswitch_phy_read;
	sc->hal.mtkswitch_phy_write = mtkswitch_phy_write;
	sc->hal.mtkswitch_reg_read = mtkswitch_reg_read;
	sc->hal.mtkswitch_reg_write = mtkswitch_reg_write;
}

#define MT7530_PORT_MIB_COUNTER(x)	(0x4000 + (x) * 0x100)

struct mt7530_mib_desc {
	unsigned int size;
	unsigned int offset;
	const char *name;
	const char *desc;
};

#define MIB_DESC(_s, _o, _n, _d)\
{				\
	.size = (_s),		\
	.offset = (_o),		\
	.name = (_n),		\
	.desc = (_d),		\
}

// vaata üle
static const
struct mt7530_mib_desc mt7530_mib[] = {
	MIB_DESC(1, 0x00, "tx_drop",		"Transmit droped frames"),
	MIB_DESC(1, 0x04, "tx_crcerrs",		"Transmit CRC errors"),
	MIB_DESC(1, 0x08, "tx_ucast_frames",	"Transmit good unicast frames"),
	MIB_DESC(1, 0x0c, "tx_mcast_frames",	"Transmit good multicast frames"),
	MIB_DESC(1, 0x10, "tx_bcast_frames",	"Transmit good broadcast frames"),
	MIB_DESC(1, 0x14, "tx_colls",		"Transmit collisions"),
	MIB_DESC(1, 0x18, "tx_single_colls",	"Transmit single collisions"),
	MIB_DESC(1, 0x1c, "tx_multi_colls",	"Transmit multiple collisions"),
	MIB_DESC(1, 0x20, "tx_deferred",	"Transmit deferred frames"),
	MIB_DESC(1, 0x24, "tx_late_colls",	"Transmit late collisions"),
	MIB_DESC(1, 0x28, "tx_excess_colls",	"Transmit excessive collisions"),
	MIB_DESC(1, 0x2c, "tx_pause_frames",	"Transmit pause frames"),
	MIB_DESC(1, 0x30, "tx_frames_64",	"Transmit 64 bytes frames"),
	MIB_DESC(1, 0x34, "tx_frames_65_127",	"Transmit 65 to 127 bytes frames"),
	MIB_DESC(1, 0x38, "tx_frames_128_255",	"Transmit 128 to 255 bytes frames"),
	MIB_DESC(1, 0x3c, "tx_frames_256_511",	"Transmit 256 to 511 bytes frames"),
	MIB_DESC(1, 0x40, "tx_frames_512_1023",	"Transmit 512 to 1023 bytes frames"),
	MIB_DESC(1, 0x44, "tx_frame_1024_max",	"Transmit 1024 to max bytes frames"),
	MIB_DESC(2, 0x48, "tx_bytes",		"Transmit good bytes"),
	MIB_DESC(1, 0x60, "rx_drop",		"Receive droped frames"),
	MIB_DESC(1, 0x64, "rx_pkts_filtered",	"Receive frames is filtered"),
	MIB_DESC(1, 0x68, "rx_ucast_frames",	"Receive unicast frames"),
	MIB_DESC(1, 0x6c, "rx_mcast_frames",	"Receive multicast frames"),
	MIB_DESC(1, 0x70, "rx_bcast_frames",	"Receive broadcast frames"),
	MIB_DESC(1, 0x74, "rx_align_errs",	"Receive alignment errors"),
	MIB_DESC(1, 0x78, "rx_crcerrs",		"Receive CRC errors"),
	MIB_DESC(1, 0x7c, "rx_runts",		"Receive undersized frames"),
	MIB_DESC(1, 0x80, "rx_fragments",	"Receive fragmented frames"),
	MIB_DESC(1, 0x84, "rx_oversize_frames",	"Receive oversize frames"),
	MIB_DESC(1, 0x88, "rx_jabbers",		"Receive jabbers frames"),
	MIB_DESC(1, 0x8c, "rx_pause_frames",	"Receive pause control frames"),
	MIB_DESC(1, 0x90, "rx_frames_64",	"Receive 64 bytes frames"),
	MIB_DESC(1, 0x94, "rx_frames_65_127",	"Receive 65 to 127 bytes frames"),
	MIB_DESC(1, 0x98, "rx_frames_128_255",	"Receive 128 to 255 bytes frames"),
	MIB_DESC(1, 0x9c, "rx_frames_256_511",	"Receive 256 to 511 bytes frames"),
	MIB_DESC(1, 0xa0, "rx_frames_512_1023",	"Receive 512 to 1023 bytes frames"),
	MIB_DESC(1, 0xa4, "rx_frame_1024_max",	"Receive 1024 to max bytes frames"),
	MIB_DESC(2, 0xa8, "rx_bytes",		"Receive good bytes"),
	MIB_DESC(1, 0xb0, "rx_ctrl_drop",	"Receive droped frames"),
	MIB_DESC(1, 0xb4, "rx_ingress_drop",	"Receive droped by ingress rate limited"),
	MIB_DESC(1, 0xb8, "rx_arl_drop",	"Receive droped by ACL"),
};

static int64_t
mt7531_hw_port_mib_read_count(struct mtkswitch_softc *sc, int port, int index)
{
	const struct mt7530_mib_desc *mib;
	uint64_t val;
	uint32_t reg, hi;

	MTKSWITCH_LOCK_ASSERT(sc, MA_OWNED);

	mib = &mt7530_mib[index];
	reg = MT7530_PORT_MIB_COUNTER(port) + mib->offset;

	val = sc->hal.mtkswitch_read(sc, reg);
	if (mib->size == 2) {
		hi = sc->hal.mtkswitch_read(sc, reg + 4);
		val |= ((uint64_t) hi << 32);
	}

	return val;
}

static int
mt7531_hw_port_mib_clear(struct mtkswitch_softc *sc, int port,
     int index, uint32_t val)
{
	uint32_t reg;

	MTKSWITCH_LOCK_ASSERT(sc, MA_OWNED);

	reg = MT7530_PORT_MIB_COUNTER(port) + index;

	return sc->hal.mtkswitch_write(sc, reg, val);
}

struct mt7531_sysctl_mib {
        struct mtkswitch_softc  *sc;
        int                     index;
        int                     port;
};

static int
mt7531_sysctl_port_mib_read_count(SYSCTL_HANDLER_ARGS)
{
	struct mtkswitch_softc *sc;
	uint64_t val = 0;
	uint32_t reg = (uint32_t)arg2;
	uint32_t port  = ((reg >> 16) & 0xffff);
	uint32_t index = (reg & 0xffff);

	sc = (struct mtkswitch_softc *)arg1;
	if (sc == NULL)
		return (EINVAL);

	if (index < 0 || index > nitems(mt7530_mib))
		return (EINVAL);

	if (port < 0 || port > MTKSWITCH_MAX_PORTS)
		return (EINVAL);

	MTKSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	MTKSWITCH_LOCK(sc);
	val = mt7531_hw_port_mib_read_count(sc, port, index);
	MTKSWITCH_UNLOCK(sc);

	return sysctl_handle_64(oidp, &val, 0, req);
}

static int
mt7531_sysctl_port_mib_clear_count(SYSCTL_HANDLER_ARGS)
{
	struct mtkswitch_softc *sc;
	uint32_t val = 0;
	uint32_t reg = (uint32_t)arg2;
	uint32_t port  = ((reg >> 16) & 0xffff);
	uint32_t index = (reg & 0xffff);
	int error;

	sc = (struct mtkswitch_softc *)arg1;
	if (sc == NULL)
		return (EINVAL);

	//if (index < 0 || index > nitems(mt7530_mib))
	//	return (EINVAL);

	if (port < 0 || port > MTKSWITCH_MAX_PORTS)
		return (EINVAL);

	error = sysctl_handle_32(oidp, &val, 0, req);

	if (error || !req->newptr)
		return (error);

	MTKSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	MTKSWITCH_LOCK(sc);
	mt7531_hw_port_mib_clear(sc, port, index, val);
	MTKSWITCH_UNLOCK(sc);

	return (0);
}

int
mt7531_sysctl_attach(struct mtkswitch_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid *ptree;
	struct sysctl_oid_list *children;
	struct sysctl_oid_list *pchildren;
	struct sysctl_oid_list *ichildren;

	char port_num_buf[32];
	uint32_t reg;
	int index, port;

	ctx = device_get_sysctl_ctx(sc->sc_dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->sc_dev));

	tree = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "port",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
	     "ethernet stndard mib counters of ports");
	pchildren = SYSCTL_CHILDREN(tree);

	for (port = 0; port < MTKSWITCH_MAX_PORTS; port++) {
		snprintf(port_num_buf, sizeof(port_num_buf), "%d", port);
		ptree = SYSCTL_ADD_NODE(ctx, pchildren, port,
		    port_num_buf, CTLFLAG_RD | CTLFLAG_MPSAFE,
		    NULL, "port mib counters");
		ichildren = SYSCTL_CHILDREN(ptree);
		for (index = 0; index <  nitems(mt7530_mib); index++) {
			reg = ((port << 16) | index);

			SYSCTL_ADD_PROC(ctx, ichildren, index,
			    mt7530_mib[index].name,
			    CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    sc, reg, mt7531_sysctl_port_mib_read_count,
			    "LU", mt7530_mib[index].desc);
		}
		reg = ((port << 16) | 0xD0);
		SYSCTL_ADD_PROC(ctx, ichildren, OID_AUTO,
		    "clear_tx", CTLTYPE_UINT | CTLFLAG_WR | CTLFLAG_MPSAFE,
		     sc, reg, mt7531_sysctl_port_mib_clear_count,
		     "IU", "Clear TX Counters");
		reg = ((port << 16) | 0xD4);
		SYSCTL_ADD_PROC(ctx, ichildren, OID_AUTO,
		    "clear_rx", CTLTYPE_UINT | CTLFLAG_WR | CTLFLAG_MPSAFE,
		     sc, reg, mt7531_sysctl_port_mib_clear_count,
		     "IU", "Clear RX Counters");
	}
	return (0);
}

