/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2025 Martin Filla
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/rman.h>

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
#include <dev/ofw/ofw_bus_subr.h>
#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

#define MT7531_SWITCH_MAX_PORTS 7
#define MT7531_SWITCH_MAX_PHYS  5
#define	MTKSWITCH_CPU_PORT	6
#define MT7531_SWITCH_HDR_LEN   4
#define MT7530_SWITCH_NUM_FDB_RECORDS  2048
#define MT7530_SWITCH_ALL_MEMBERS      0xff
#define	MTKSWITCH_LINK_UP	(1<<0)
#define	MTKSWITCH_SPEED_MASK	(3<<1)
#define	MTKSWITCH_SPEED_10	(0<<1)
#define MTKSWITCH_SPEED_100	(1<<1)
#define	MTKSWITCH_SPEED_1000	(2<<1)
#define	MTKSWITCH_DUPLEX	(1<<3)
#define MTKSWITCH_TXFLOW	(1<<4)
#define MTKSWITCH_RXFLOW	(1<<5)

#define	MTKSWITCH_ATC	0x0080
#define		ATC_BUSY		(1u<<15)
#define		ATC_AC_MAT_NON_STATIC_MACS	(4u<<8)
#define		ATC_AC_CMD_CLEAN	(2u<<0)

#define	MTKSWITCH_VTCR	0x0090
#define		VTCR_BUSY		(1u<<31)
#define		VTCR_FUNC_VID_READ	(0u<<12)
#define		VTCR_FUNC_VID_WRITE	(1u<<12)
#define		VTCR_FUNC_VID_INVALID	(2u<<12)
#define		VTCR_FUNC_VID_VALID	(3u<<12)
#define		VTCR_IDX_INVALID	(1u<<16)
#define		VTCR_VID_MASK		0xfff

#define	MTKSWITCH_VAWD1	0x0094
#define		VAWD1_IVL_MAC		(1u<<30)
#define		VAWD1_VTAG_EN		(1u<<28)
#define		VAWD1_PORT_MEMBER(p)	((1u<<16)<<(p))
#define		VAWD1_MEMBER_OFF	16
#define		VAWD1_MEMBER_MASK	0xff
#define		VAWD1_FID_OFFSET	1
#define		VAWD1_VALID		(1u<<0)

#define	MTKSWITCH_VAWD2	0x0098
#define		VAWD2_PORT_UNTAGGED(p)	(0u<<((p)*2))
#define		VAWD2_PORT_TAGGED(p)	(2u<<((p)*2))
#define		VAWD2_PORT_MASK(p)	(3u<<((p)*2))

#define	MTKSWITCH_VTIM(v)	((((v) >> 1) * 4) + 0x100)
#define		VTIM_OFF(v)	(((v) & 1) ? 12 : 0)
#define		VTIM_MASK	0xfff

#define	MTKSWITCH_PIAC	0x7004
#define		PIAC_PHY_ACS_ST		(1u<<31)
#define		PIAC_MDIO_REG_ADDR_OFF	25
#define		PIAC_MDIO_PHY_ADDR_OFF	20
#define		PIAC_MDIO_CMD_WRITE	(1u<<18)
#define		PIAC_MDIO_CMD_READ	(2u<<18)
#define		PIAC_MDIO_ST		(1u<<16)
#define		PIAC_MDIO_RW_DATA_MASK	0xffff

#define	MTKSWITCH_PORTREG(r, p)	((r) + ((p) * 0x100))

#define	MTKSWITCH_PCR(x)	MTKSWITCH_PORTREG(0x2004, (x))
#define		PCR_PORT_VLAN_SECURE	(3u<<0)

#define	MTKSWITCH_PVC(x)	MTKSWITCH_PORTREG(0x2010, (x))
#define		PVC_VLAN_ATTR_MASK	(3u<<6)

#define	MTKSWITCH_PPBV1(x)	MTKSWITCH_PORTREG(0x2014, (x))
#define	MTKSWITCH_PPBV2(x)	MTKSWITCH_PORTREG(0x2018, (x))
#define		PPBV_VID(v)		(((v)<<16) | (v))
#define		PPBV_VID_FROM_REG(x)	((x) & 0xfff)
#define		PPBV_VID_MASK		0xfff

#define	MTKSWITCH_PMCR(x)	MTKSWITCH_PORTREG(0x3000, (x))
#define		PMCR_FORCE_LINK		(1u<<0)
#define		PMCR_FORCE_DPX		(1u<<1)
#define		PMCR_FORCE_SPD_1000	(2u<<2)
#define		PMCR_FORCE_TX_FC	(1u<<4)
#define		PMCR_FORCE_RX_FC	(1u<<5)
#define		PMCR_BACKPR_EN		(1u<<8)
#define		PMCR_BKOFF_EN		(1u<<9)
#define		PMCR_MAC_RX_EN		(1u<<13)
#define		PMCR_MAC_TX_EN		(1u<<14)
#define		PMCR_FORCE_MODE		(1u<<15)
#define		PMCR_RES_1		(1u<<16)
#define		PMCR_IPG_CFG_RND	(1u<<18)
#define		PMCR_CFG_DEFAULT	(PMCR_BACKPR_EN | PMCR_BKOFF_EN | \
		    PMCR_MAC_RX_EN | PMCR_MAC_TX_EN | PMCR_IPG_CFG_RND |  \
		    PMCR_FORCE_RX_FC | PMCR_FORCE_TX_FC | PMCR_RES_1)

#define	MTKSWITCH_PMSR(x)	MTKSWITCH_PORTREG(0x3008, (x))
#define		PMSR_MAC_LINK_STS	(1u<<0)
#define		PMSR_MAC_DPX_STS	(1u<<1)
#define		PMSR_MAC_SPD_STS	(3u<<2)
#define		PMSR_MAC_SPD(x)		(((x)>>2) & 0x3)
#define		PMSR_MAC_SPD_10		0
#define		PMSR_MAC_SPD_100	1
#define		PMSR_MAC_SPD_1000	2
#define		PMSR_TX_FC_STS		(1u<<4)
#define		PMSR_RX_FC_STS		(1u<<5)

#define	MTKSWITCH_READ(_sc, _reg)		\
	    bus_read_4((_sc)->res, (_reg))
#define MTKSWITCH_WRITE(_sc, _reg, _val)	\
	    bus_write_4((_sc)->res, (_reg), (_val))
#define	MTKSWITCH_MOD(_sc, _reg, _clr, _set)	\
	    MTKSWITCH_WRITE((_sc), (_reg),	\
	        ((MTKSWITCH_READ((_sc), (_reg)) & ~(_clr)) | (_set))

#define	MTKSWITCH_REG32(addr)	((addr) & ~(0x3))
#define	MTKSWITCH_IS_HI16(addr)	(((addr) & 0x3) > 0x1)
#define	MTKSWITCH_HI16(x)	(((x) >> 16) & 0xffff)
#define	MTKSWITCH_LO16(x)	((x) & 0xffff)
#define	MTKSWITCH_TO_HI16(x)	(((x) & 0xffff) << 16)
#define	MTKSWITCH_TO_LO16(x)	((x) & 0xffff)
#define	MTKSWITCH_HI16_MSK	0xffff0000
#define MTKSWITCH_LO16_MSK	0x0000ffff

#define	MTKSWITCH_LAN_VID	0x001
#define	MTKSWITCH_WAN_VID	0x002
#define	MTKSWITCH_INVALID_VID	0xfff

#define	MTKSWITCH_LAN_FID	1
#define	MTKSWITCH_WAN_FID	2

#define	MTKSWITCH_REG_ADDR(r)	(((r) >> 6) & 0x3ff)
#define	MTKSWITCH_REG_LO(r)	(((r) >> 2) & 0xf)
#define	MTKSWITCH_REG_HI(r)	(1 << 4)
#define MTKSWITCH_VAL_LO(v)	((v) & 0xffff)
#define MTKSWITCH_VAL_HI(v)	(((v) >> 16) & 0xffff)
#define MTKSWITCH_GLOBAL_PHY	31
#define	MTKSWITCH_GLOBAL_REG	31


struct mt7531_switch_softc {
    struct mtx	mtx;
    device_t	dev;
    struct resource *res;
    int		numphys;
    uint32_t	phymap;
    int		numports;
    uint32_t	portmap;
    int		cpuport;
    uint32_t	valid_vlans;
    //mtk_switch_type	sc_switchtype;
    char		*ifname[MT7531_SWITCH_MAX_PHYS];
    device_t	miibus[MT7531_SWITCH_MAX_PHYS];
    if_t ifp[MT7531_SWITCH_MAX_PHYS];
    struct callout	callout_tick;
    etherswitch_info_t info;
    uint32_t	vlan_mode;

    struct {
        /* Global setup */
        int (* mt7531_switch_reset) (struct mt7531_switch_softc *);
        int (* mt7531_switch_hw_setup) (struct mt7531_switch_softc *);
        int (* mt7531_switch_hw_global_setup) (struct mt7531_switch_softc *);

        /* Port functions */
        void (* mt7531_switchport_init) (struct mt7531_switch_softc *, int);
        uint32_t (* mt7531_switch_get_port_status) (struct mt7531_switch_softc *, int);

        /* ATU functions */
        int (* mt7531_switch_atu_flush) (struct mt7531_switch_softc *);

        /* VLAN functions */
        int (* mt7531_switch_port_vlan_setup) (struct mt7531_switch_softc *,
                                               etherswitch_port_t *);
        int (* mt7531_switch_port_vlan_get) (struct mt7531_switch_softc *,
                                             etherswitch_port_t *);
        void (* mt7531_switch_vlan_init_hw) (struct mt7531_switch_softc *);
        int (* mt7531_switch_vlan_getvgroup) (struct mt7531_switch_softc *,
                                              etherswitch_vlangroup_t *);
        int (* mt7531_switch_vlan_setvgroup) (struct mt7531_switch_softc *,
                                              etherswitch_vlangroup_t *);
        int (* mt7531_switch_vlan_get_pvid) (struct mt7531_switch_softc *,
                                             int, int *);
        int (* mt7531_switch_vlan_set_pvid) (struct mt7531_switch_softc *,
                                             int, int);

        /* PHY functions */
        int (* mt7531_switch_phy_read) (device_t, int, int);
        int (* mt7531_switch_phy_write) (device_t, int, int, int);

        /* Register functions */
        int (* mt7531_switch_reg_read) (device_t, int);
        int (* mt7531_switch_reg_write) (device_t, int, int);

        /* Internal register access functions */
        uint32_t (* mt7531_switch_read) (struct mt7531_switch_softc *, int);
        uint32_t (* mt7531_switch_write) (struct mt7531_switch_softc *, int,
                                         uint32_t);
    } hal;
};

static struct ofw_compat_data compat_data[] = {
        { "mediatek,mt7531",	1 },
        { NULL, 0 }
};

static void
mtk_attach_switch_mt7531(struct mt7531_switch_softc *sc);

/* PHY <-> port mapping is currently 1:1 */
static inline int
mt7531_portforphy(int phy)
{

    return (phy);
}

static inline int
mt7531_phyforport(int port)
{

    return (port);
}

/*
 * Convert port status to ifmedia.
 */
static void
mt7531_update_ifmedia(uint32_t portstatus, u_int *media_status,
                         u_int *media_active)
{
    *media_active = IFM_ETHER;
    *media_status = IFM_AVALID;

    if ((portstatus & MTKSWITCH_LINK_UP) != 0)
        *media_status |= IFM_ACTIVE;
    else {
        *media_active |= IFM_NONE;
        return;
    }

    switch (portstatus & MTKSWITCH_SPEED_MASK) {
        case MTKSWITCH_SPEED_10:
            *media_active |= IFM_10_T;
            break;
        case MTKSWITCH_SPEED_100:
            *media_active |= IFM_100_TX;
            break;
        case MTKSWITCH_SPEED_1000:
            *media_active |= IFM_1000_T;
            break;
    }

    if ((portstatus & MTKSWITCH_DUPLEX) != 0)
        *media_active |= IFM_FDX;
    else
        *media_active |= IFM_HDX;

    if ((portstatus & MTKSWITCH_TXFLOW) != 0)
        *media_active |= IFM_ETH_TXPAUSE;
    if ((portstatus & MTKSWITCH_RXFLOW) != 0)
        *media_active |= IFM_ETH_RXPAUSE;
}

static void
mt7531_miipollstat(struct mt7531_switch_softc *sc)
{
    struct mii_data *mii;
    struct mii_softc *miisc;
    uint32_t portstatus;
    int i, port_flap = 0;

    mtx_assert(&sc->mtx, MA_OWNED);

    for (i = 0; i < sc->numphys; i++) {
        if (sc->miibus[i] == NULL)
            continue;
        mii = device_get_softc(sc->miibus[i]);
        portstatus = sc->hal.mt7531_switch_get_port_status(sc,
                                                           mt7531_portforphy(i));

        /* If a port has flapped - mark it so we can flush the ATU */
        if (((mii->mii_media_status & IFM_ACTIVE) == 0 &&
             (portstatus & MTKSWITCH_LINK_UP) != 0) ||
            ((mii->mii_media_status & IFM_ACTIVE) != 0 &&
             (portstatus & MTKSWITCH_LINK_UP) == 0)) {
            port_flap = 1;
        }

        mt7531_update_ifmedia(portstatus, &mii->mii_media_status,
                                 &mii->mii_media_active);
        LIST_FOREACH(miisc, &mii->mii_phys, mii_list) {
            if (IFM_INST(mii->mii_media.ifm_cur->ifm_media) !=
                miisc->mii_inst)
                continue;
            mii_phy_update(miisc, MII_POLLSTAT);
        }
    }

    if (port_flap) {
        sc->hal.mt7531_switch_atu_flush(sc);
    }
}

static void
mt7531_tick(void *arg)
{
    struct mt7531_switch_softc *sc = arg;

    mt7531_miipollstat(sc);
    callout_reset(&sc->callout_tick, hz, mt7531_tick, sc);
}

static etherswitch_info_t *
mt7531_getinfo(device_t dev)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);
    return (&sc->info);
}

static int
mt7531_getport(device_t dev, etherswitch_port_t *p)
{
    /*struct mt7531_switch_softc *sc = device_get_softc(dev);
    uint32_t pmsr = 0;
    int port = p->es_port;

    if (port < 0 || port > 6)
        return (EINVAL);

    p->es_pvid = 1;
    p->es_flags = 0;*/

    /*p->es_link = (pmsr & PMSR_LINK) != 0;
    p->es_duplex = (pmsr & PMSR_FDX) ? 1 : 0;

    switch ((pmsr & PMSR_SPEED_MASK) >> 2) {
        case 0: p->es_speed = 10; break;
        case 1: p->es_speed = 100; break;
        case 2: p->es_speed = 1000; break;
        default: p->es_speed = 0; break;
    }*/
    return (0);
}

static int
mt7531_setport(device_t dev, etherswitch_port_t *p)
{
    /*struct mt7531_switch_softc *sc = device_get_softc(dev);
    int port = p->es_port;
    uint32_t pmcr = 0;

    if (port < 0 || port > 6)
        return (EINVAL);

    pmcr = PMCR_FORCE_MODE | PMCR_TX_EN | PMCR_RX_EN | PMCR_FORCE_FDX | PMCR_FORCE_SPEED_1000;
    */
    return (0);
}

static int
mt7531_getvlangroup(device_t dev, etherswitch_vlangroup_t *vg)
{
    vg->es_vid = vg->es_vlangroup + 1;
    vg->es_member_ports = 0;
    vg->es_untagged_ports = 0;
    return (0);
}

static int
mt7531_setvlangroup(device_t dev, etherswitch_vlangroup_t *vg)
{
    return (0);
}

static void
mt7531_reset(struct mt7531_switch_softc *sc)
{
}

static int
mt7531_readphy(device_t dev, int phy, int reg)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);

    return (sc->hal.mt7531_switch_phy_read(dev, phy, reg));
}

static int
mt7531_writephy(device_t dev, int phy, int reg, int val)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);

    return (sc->hal.mt7531_switch_phy_write(dev, phy, reg, val));
}

static int
mt7531_readreg(device_t dev, int addr)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);

    return (sc->hal.mt7531_switch_reg_read(dev, addr));
}

static int
mt7531_writereg(device_t dev, int addr, int value)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);

    return (sc->hal.mt7531_switch_reg_write(dev, addr, value));
}

static void
mt7531_statchg(device_t dev)
{
    device_printf(dev, "func %s\n", __func__ );
}

static inline struct mii_data *
mt7531_miiforport(struct mt7531_switch_softc *sc, int port)
{
    int phy = mt7531_phyforport(port);

    if (phy < 0 || phy >= MT7531_SWITCH_MAX_PHYS || sc->miibus[phy] == NULL)
        return (NULL);

    return (device_get_softc(sc->miibus[phy]));
}

static int
mt7531_ifmedia_upd(if_t ifp)
{
    struct mt7531_switch_softc *sc = if_getsoftc(ifp);
    struct mii_data *mii = mt7531_miiforport(sc, if_getdunit(ifp));

    if (mii == NULL)
        return (ENXIO);
    mii_mediachg(mii);
    return (0);
}

static void
mt7531_ifmedia_sts(if_t ifp, struct ifmediareq *ifmr)
{
    struct mt7531_switch_softc *sc = if_getsoftc(ifp);
    struct mii_data *mii = mt7531_miiforport(sc, if_getdunit(ifp));

    device_printf(sc->dev, "%s\n", __func__);

    if (mii == NULL)
        return;
    mii_pollstat(mii);
    ifmr->ifm_active = mii->mii_media_active;
    ifmr->ifm_status = mii->mii_media_status;
}

static int
mt7531_attach_phys(struct mt7531_switch_softc *sc)
{
    int phy, err = 0;
    char name[IFNAMSIZ];

    /* PHYs need an interface, so we generate a dummy one */
    snprintf(name, IFNAMSIZ, "%sport", device_get_nameunit(sc->dev));
    for (phy = 0; phy < sc->numphys; phy++) {
        if ((sc->phymap & (1u << phy)) == 0) {
            sc->ifp[phy] = NULL;
            sc->ifname[phy] = NULL;
            sc->miibus[phy] = NULL;
            continue;
        }
        sc->ifp[phy] = if_alloc(IFT_ETHER);
        if_setsoftc(sc->ifp[phy], sc);
        if_setflagbits(sc->ifp[phy], IFF_UP | IFF_BROADCAST |
                                     IFF_DRV_RUNNING | IFF_SIMPLEX, 0);
        sc->ifname[phy] = malloc(strlen(name) + 1, M_DEVBUF, M_WAITOK);
        bcopy(name, sc->ifname[phy], strlen(name) + 1);
        if_initname(sc->ifp[phy], sc->ifname[phy],
                    mt7531_portforphy(phy));
        err = mii_attach(sc->dev, &sc->miibus[phy], sc->ifp[phy],
                         mt7531_ifmedia_upd, mt7531_ifmedia_sts,
                         BMSR_DEFCAPMASK, phy, MII_OFFSET_ANY, 0);
        if (err != 0) {
            device_printf(sc->dev,
                          "attaching PHY %d failed\n",
                          phy);
        } else {
            device_printf(sc->dev, "%s attached to pseudo interface "
                                "%s\n", device_get_nameunit(sc->miibus[phy]),
                    if_name(sc->ifp[phy]));
        }
    }
    return (err);
}

static int
mt7531_set_vlan_mode(struct mt7531_switch_softc *sc, uint32_t mode)
{
    /* Check for invalid modes. */
    if ((mode & sc->info.es_vlan_caps) != mode)
        return (EINVAL);

    sc->vlan_mode = mode;

    /* Reset VLANs. */
    sc->hal.mt7531_switch_vlan_init_hw(sc);

    return (0);
}

static int
mt7531_probe(device_t dev)
{
    //if (!ofw_bus_status_okay(dev))
    //    return (ENXIO);

    if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
        return (ENXIO);

    device_printf(dev, "%s", __func__);
    device_set_desc(dev, "MediaTek MT7531 Gigabit Switch");
    return (BUS_PROBE_DEFAULT);
}

static int
mt7531_attach(device_t dev)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);
    int err, rid;

    sc->numports = MT7531_SWITCH_MAX_PORTS;
    sc->numphys = MT7531_SWITCH_MAX_PHYS;
    sc->cpuport = MTKSWITCH_CPU_PORT;
    sc->dev = dev;

    mtk_attach_switch_mt7531(sc);

    rid = 0;
    sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
    if (sc->res == NULL) {
        device_printf(dev, "Could not map memory\n");
        return (ENXIO);
    }

    mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

    if (sc->hal.mt7531_switch_reset(sc)) {
        device_printf(dev, "%s: mtkswitch_reset: failed\n", __func__);
        return (ENXIO);
    }

    err = sc->hal.mt7531_switch_hw_setup(sc);
    device_printf(dev, "%s: hw_setup: err=%d\n", __func__, err);
    if (err != 0) {
        return (err);
    }

    err = sc->hal.mt7531_switch_hw_global_setup(sc);
    device_printf(dev, "%s: hw_global_setup: err=%d\n", __func__, err);
    if (err != 0) {
        return (err);
    }

    /* Initialize the switch ports */
    for (int port = 0; port < sc->numports; port++) {
        sc->hal.mt7531_switchport_init(sc, port);
    }

    /* Attach the PHYs and complete the bus enumeration */
    err = mt7531_attach_phys(sc);
    device_printf(dev, "%s: attach_phys: err=%d\n", __func__, err);
    if (err != 0)
        return (err);

    /* Default to ingress filters off. */
    err = mt7531_set_vlan_mode(sc, ETHERSWITCH_VLAN_DOT1Q);
    device_printf(dev, "%s: set_vlan_mode: err=%d\n", __func__, err);
    if (err != 0)
        return (err);

    bus_identify_children(dev);
    bus_enumerate_hinted_children(dev);
    bus_attach_children(dev);
    device_printf(dev, "%s: bus_generic_attach: err=%d\n", __func__, err);
    if (err != 0)
        return (err);

    callout_init_mtx(&sc->callout_tick, &sc->mtx, 0);

    mtx_lock(&sc->mtx);
    mt7531_tick(sc);
    mtx_unlock(&sc->mtx);

    device_printf(dev, "Inicialize device....");

    return (0);
}

static int
mt7531_detach(device_t dev)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);
    mtx_destroy(&sc->mtx);
    return (0);
}

static device_method_t mt7531_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,     mt7531_probe),
        DEVMETHOD(device_attach,    mt7531_attach),
        DEVMETHOD(device_detach,    mt7531_detach),

        /* bus interface */
        DEVMETHOD(bus_add_child,	device_add_child_ordered),

        /* etherswitch interface */
        DEVMETHOD(etherswitch_getinfo,      mt7531_getinfo),
        DEVMETHOD(etherswitch_readreg,      mt7531_readreg),
        DEVMETHOD(etherswitch_writereg,     mt7531_writereg),
        DEVMETHOD(etherswitch_getport,      mt7531_getport),
        DEVMETHOD(etherswitch_setport,      mt7531_setport),
        DEVMETHOD(etherswitch_getvgroup, mt7531_getvlangroup),
        DEVMETHOD(etherswitch_setvgroup, mt7531_setvlangroup),

        /* MII interface */
        DEVMETHOD(miibus_readreg,	mt7531_readphy),
        DEVMETHOD(miibus_writereg,	mt7531_writephy),
        DEVMETHOD(miibus_statchg,	mt7531_statchg),

        /* MDIO interface */
        DEVMETHOD(mdio_readreg,		mt7531_readphy),
        DEVMETHOD(mdio_writereg,	mt7531_writephy),

        DEVMETHOD_END
};


DEFINE_CLASS_0(mt7531_switch, mt7531_switch_driver, mt7531_methods, sizeof(struct mt7531_switch_softc));
DRIVER_MODULE(mt7531_switch, simplebus, mt7531_switch_driver, 0, 0);
DRIVER_MODULE(miibus, mt7531_switch, miibus_driver, 0, 0);
DRIVER_MODULE(mdio, mt7531_switch, mdio_driver, 0, 0);
DRIVER_MODULE(etherswitch, mt7531_switch, etherswitch_driver, 0, 0);
MODULE_VERSION(mt7531_switch, 1);
MODULE_DEPEND(mt7531_switch, miibus, 1, 1, 1);
MODULE_DEPEND(mt7531_switch, etherswitch, 1, 1, 1);


static int
mtkswitch_phy_read_locked(struct mt7531_switch_softc *sc, int phy, int reg)
{
    uint32_t data;

    MTKSWITCH_WRITE(sc, MTKSWITCH_PIAC, PIAC_PHY_ACS_ST | PIAC_MDIO_ST |
                                        (reg << PIAC_MDIO_REG_ADDR_OFF) | (phy << PIAC_MDIO_PHY_ADDR_OFF) |
                                        PIAC_MDIO_CMD_READ);
    while ((data = MTKSWITCH_READ(sc, MTKSWITCH_PIAC)) & PIAC_PHY_ACS_ST);

    return ((int)(data & PIAC_MDIO_RW_DATA_MASK));
}

static int
mtkswitch_phy_read(device_t dev, int phy, int reg)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);
    int data;

    if ((phy < 0 || phy >= 32) || (reg < 0 || reg >= 32))
        return (ENXIO);

    mtx_assert(&sc->mtx, MA_NOTOWNED);
    mtx_lock(&sc->mtx);
    data = mtkswitch_phy_read_locked(sc, phy, reg);
    mtx_unlock(&sc->mtx);

    return (data);
}

static int
mtkswitch_phy_write_locked(struct mt7531_switch_softc *sc, int phy, int reg,
                           int val)
{

    MTKSWITCH_WRITE(sc, MTKSWITCH_PIAC, PIAC_PHY_ACS_ST | PIAC_MDIO_ST |
                                        (reg << PIAC_MDIO_REG_ADDR_OFF) | (phy << PIAC_MDIO_PHY_ADDR_OFF) |
                                        (val & PIAC_MDIO_RW_DATA_MASK) | PIAC_MDIO_CMD_WRITE);
    while (MTKSWITCH_READ(sc, MTKSWITCH_PIAC) & PIAC_PHY_ACS_ST);

    return (0);
}

static int
mtkswitch_phy_write(device_t dev, int phy, int reg, int val)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);
    int res;

    if ((phy < 0 || phy >= 32) || (reg < 0 || reg >= 32))
        return (ENXIO);

    mtx_assert(&sc->mtx, MA_NOTOWNED);
    mtx_lock(&sc->mtx);
    res = mtkswitch_phy_write_locked(sc, phy, reg, val);
    mtx_unlock(&sc->mtx);

    return (res);
}

static uint32_t
mtkswitch_reg_read32(struct mt7531_switch_softc *sc, int reg)
{

    return (MTKSWITCH_READ(sc, reg));
}

static uint32_t
mtkswitch_reg_write32(struct mt7531_switch_softc *sc, int reg, uint32_t val)
{

    MTKSWITCH_WRITE(sc, reg, val);
    return (0);
}

static uint32_t
mtkswitch_reg_read32_mt7621(struct mt7531_switch_softc *sc, int reg)
{
    uint32_t low, hi;

    mtkswitch_phy_write_locked(sc, MTKSWITCH_GLOBAL_PHY,
                               MTKSWITCH_GLOBAL_REG, MTKSWITCH_REG_ADDR(reg));
    low = mtkswitch_phy_read_locked(sc, MTKSWITCH_GLOBAL_PHY,
                                    MTKSWITCH_REG_LO(reg));
    hi = mtkswitch_phy_read_locked(sc, MTKSWITCH_GLOBAL_PHY,
                                   MTKSWITCH_REG_HI(reg));
    return (low | (hi << 16));
}

static uint32_t
mtkswitch_reg_write32_mt7621(struct mt7531_switch_softc *sc, int reg, uint32_t val)
{

    mtkswitch_phy_write_locked(sc, MTKSWITCH_GLOBAL_PHY,
                               MTKSWITCH_GLOBAL_REG, MTKSWITCH_REG_ADDR(reg));
    mtkswitch_phy_write_locked(sc, MTKSWITCH_GLOBAL_PHY,
                               MTKSWITCH_REG_LO(reg), MTKSWITCH_VAL_LO(val));
    mtkswitch_phy_write_locked(sc, MTKSWITCH_GLOBAL_PHY,
                               MTKSWITCH_REG_HI(reg), MTKSWITCH_VAL_HI(val));
    return (0);
}

static int
mtkswitch_reg_read(device_t dev, int reg)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);
    uint32_t val;

    val = sc->hal.mt7531_switch_read(sc, MTKSWITCH_REG32(reg));
    if (MTKSWITCH_IS_HI16(reg))
        return (MTKSWITCH_HI16(val));
    return (MTKSWITCH_LO16(val));
}

static int
mtkswitch_reg_write(device_t dev, int reg, int val)
{
    struct mt7531_switch_softc *sc = device_get_softc(dev);
    uint32_t tmp;

    tmp = sc->hal.mt7531_switch_read(sc, MTKSWITCH_REG32(reg));
    if (MTKSWITCH_IS_HI16(reg)) {
        tmp &= MTKSWITCH_LO16_MSK;
        tmp |= MTKSWITCH_TO_HI16(val);
    } else {
        tmp &= MTKSWITCH_HI16_MSK;
        tmp |= MTKSWITCH_TO_LO16(val);
    }
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_REG32(reg), tmp);

    return (0);
}

static int
mtkswitch_reset(struct mt7531_switch_softc *sc)
{

    /* We don't reset the switch for now */
    return (0);
}

static int
mtkswitch_hw_setup(struct mt7531_switch_softc *sc)
{

    /*
     * TODO: parse the device tree and see if we need to configure
     *       ports, etc. differently. For now we fallback to defaults.
     */

    /* Called early and hence unlocked */
    return (0);
}

static int
mtkswitch_hw_global_setup(struct mt7531_switch_softc *sc)
{
    /* Currently does nothing */

    /* Called early and hence unlocked */
    return (0);
}

static void
mtkswitch_port_init(struct mt7531_switch_softc *sc, int port)
{
    uint32_t val;

    /* Called early and hence unlocked */

    /* Set the port to secure mode */
    val = sc->hal.mt7531_switch_read(sc, MTKSWITCH_PCR(port));
    val |= PCR_PORT_VLAN_SECURE;
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_PCR(port), val);

    /* Set port's vlan_attr to user port */
    val = sc->hal.mt7531_switch_read(sc, MTKSWITCH_PVC(port));
    val &= ~PVC_VLAN_ATTR_MASK;
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_PVC(port), val);

    val = PMCR_CFG_DEFAULT;
    if (port == sc->cpuport)
        val |= PMCR_FORCE_LINK | PMCR_FORCE_DPX | PMCR_FORCE_SPD_1000 |
               PMCR_FORCE_MODE;
    /* Set port's MAC to default settings */
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_PMCR(port), val);
}

static uint32_t
mtkswitch_get_port_status(struct mt7531_switch_softc *sc, int port)
{
    uint32_t val, res, tmp;

    mtx_assert(&sc->mtx, MA_OWNED);
    res = 0;
    val = sc->hal.mt7531_switch_read(sc, MTKSWITCH_PMSR(port));

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
mtkswitch_atu_flush(struct mt7531_switch_softc *sc)
{

    mtx_assert(&sc->mtx, MA_OWNED);

    /* Flush all non-static MAC addresses */
    while (sc->hal.mt7531_switch_read(sc, MTKSWITCH_ATC) & ATC_BUSY);
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_ATC, ATC_BUSY |
                                               ATC_AC_MAT_NON_STATIC_MACS | ATC_AC_CMD_CLEAN);
    while (sc->hal.mt7531_switch_read(sc, MTKSWITCH_ATC) & ATC_BUSY);

    return (0);
}

static int
mtkswitch_port_vlan_setup(struct mt7531_switch_softc *sc, etherswitch_port_t *p)
{
    int err;

    /*
     * Port behaviour wrt tag/untag/stack is currently defined per-VLAN.
     * So we say we don't support it here.
     */
    if ((p->es_flags & (ETHERSWITCH_PORT_DOUBLE_TAG |
                        ETHERSWITCH_PORT_ADDTAG | ETHERSWITCH_PORT_STRIPTAG)) != 0)
        return (ENOTSUP);

    mtx_assert(&sc->mtx, MA_NOTOWNED);
    mtx_lock(&sc->mtx);

    /* Set the PVID */
    if (p->es_pvid != 0) {
        err = sc->hal.mt7531_switch_vlan_set_pvid(sc, p->es_port,
                                              p->es_pvid);
        if (err != 0) {
            mtx_unlock(&sc->mtx);
            return (err);
        }
    }

    mtx_unlock(&sc->mtx);

    return (0);
}

static int
mtkswitch_port_vlan_get(struct mt7531_switch_softc *sc, etherswitch_port_t *p)
{

    mtx_assert(&sc->mtx, MA_NOTOWNED);
    mtx_lock(&sc->mtx);

    /* Retrieve the PVID */
    sc->hal.mt7531_switch_vlan_get_pvid(sc, p->es_port, &p->es_pvid);

    /*
     * Port flags are not supported at the moment.
     * Port's tag/untag/stack behaviour is defined per-VLAN.
     */
    p->es_flags = 0;

    mtx_unlock(&sc->mtx);

    return (0);
}

static void
mtkswitch_invalidate_vlan(struct mt7531_switch_softc *sc, uint32_t vid)
{

    while (sc->hal.mt7531_switch_read(sc, MTKSWITCH_VTCR) & VTCR_BUSY);
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_VTCR, VTCR_BUSY |
                                                VTCR_FUNC_VID_INVALID | (vid & VTCR_VID_MASK));
    while (sc->hal.mt7531_switch_read(sc, MTKSWITCH_VTCR) & VTCR_BUSY);
}

static void
mtkswitch_vlan_init_hw(struct mt7531_switch_softc *sc)
{
    uint32_t val, vid, i;

    mtx_assert(&sc->mtx, MA_NOTOWNED);
    mtx_lock(&sc->mtx);
    /* Reset all VLANs to defaults first */
    for (i = 0; i < sc->info.es_nvlangroups; i++) {
        mtkswitch_invalidate_vlan(sc, i);
        //
    }

    /* Now, add all ports as untagged members of VLAN 1 */
    vid = 1;
    val = VAWD1_IVL_MAC | VAWD1_VTAG_EN | VAWD1_VALID;
    for (i = 0; i < sc->info.es_nports; i++)
        val |= VAWD1_PORT_MEMBER(i);
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_VAWD1, val);
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_VAWD2, 0);
    val = VTCR_BUSY | VTCR_FUNC_VID_WRITE | vid;
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_VTCR, val);

    /* Set all port PVIDs to 1 */
    for (i = 0; i < sc->info.es_nports; i++) {
        sc->hal.mt7531_switch_vlan_set_pvid(sc, i, 1);
    }

    mtx_unlock(&sc->mtx);
}

static int
mtkswitch_vlan_getvgroup(struct mt7531_switch_softc *sc, etherswitch_vlangroup_t *v)
{
    uint32_t val, i;

    mtx_assert(&sc->mtx, MA_NOTOWNED);

    if ((sc->vlan_mode != ETHERSWITCH_VLAN_DOT1Q) ||
        (v->es_vlangroup > sc->info.es_nvlangroups))
        return (EINVAL);

    /* Reset the member ports. */
    v->es_untagged_ports = 0;
    v->es_member_ports = 0;

    /* Not supported for now */
    v->es_fid = 0;

    mtx_lock(&sc->mtx);;

    v->es_vid = v->es_vlangroup;

    while (sc->hal.mt7531_switch_read(sc, MTKSWITCH_VTCR) & VTCR_BUSY);
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_VTCR, VTCR_BUSY |
                                                VTCR_FUNC_VID_READ | (v->es_vlangroup & VTCR_VID_MASK));
    while ((val = sc->hal.mt7531_switch_read(sc, MTKSWITCH_VTCR)) & VTCR_BUSY);
    if (val & VTCR_IDX_INVALID) {
        mtx_unlock(&sc->mtx);
        return (0);
    }

    val = sc->hal.mt7531_switch_read(sc, MTKSWITCH_VAWD1);
    if (val & VAWD1_VALID)
        v->es_vid |= ETHERSWITCH_VID_VALID;
    else {
        mtx_unlock(&sc->mtx);
        return (0);
    }
    v->es_member_ports = (val >> VAWD1_MEMBER_OFF) & VAWD1_MEMBER_MASK;

    val = sc->hal.mt7531_switch_read(sc, MTKSWITCH_VAWD2);
    for (i = 0; i < sc->info.es_nports; i++) {
        if ((val & VAWD2_PORT_MASK(i)) == VAWD2_PORT_UNTAGGED(i))
            v->es_untagged_ports |= (1<<i);
    }

    mtx_unlock(&sc->mtx);;
    return (0);
}

static int
mtkswitch_vlan_setvgroup(struct mt7531_switch_softc *sc, etherswitch_vlangroup_t *v)
{
    uint32_t val, i, vid;

    mtx_assert(&sc->mtx, MA_NOTOWNED);
    if ((sc->vlan_mode != ETHERSWITCH_VLAN_DOT1Q) ||
        (v->es_vlangroup > sc->info.es_nvlangroups))
        return (EINVAL);

    /* We currently don't support FID */
    if (v->es_fid != 0)
        return (EINVAL);

    mtx_lock(&sc->mtx);;
    while (sc->hal.mt7531_switch_read(sc, MTKSWITCH_VTCR) & VTCR_BUSY);

    vid = v->es_vid;

    /* We use FID 0 */
    val = VAWD1_IVL_MAC | VAWD1_VTAG_EN | VAWD1_VALID;
    val |= ((v->es_member_ports & VAWD1_MEMBER_MASK) << VAWD1_MEMBER_OFF);
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_VAWD1, val);

    /* Set tagged ports */
    val = 0;
    for (i = 0; i < sc->info.es_nports; i++)
        if (((1<<i) & v->es_untagged_ports) == 0)
            val |= VAWD2_PORT_TAGGED(i);
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_VAWD2, val);

    /* Write the VLAN entry */
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_VTCR, VTCR_BUSY |
                                                VTCR_FUNC_VID_WRITE | (vid & VTCR_VID_MASK));
    while ((val = sc->hal.mt7531_switch_read(sc, MTKSWITCH_VTCR)) & VTCR_BUSY);

    mtx_unlock(&sc->mtx);

    if (val & VTCR_IDX_INVALID)
        return (EINVAL);

    return (0);
}

static int
mtkswitch_vlan_get_pvid(struct mt7531_switch_softc *sc, int port, int *pvid)
{

    mtx_assert(&sc->mtx, MA_OWNED);

    *pvid = sc->hal.mt7531_switch_read(sc, MTKSWITCH_PPBV1(port));
    *pvid = PPBV_VID_FROM_REG(*pvid);

    return (0);
}

static int
mtkswitch_vlan_set_pvid(struct mt7531_switch_softc *sc, int port, int pvid)
{
    uint32_t val;

    mtx_assert(&sc->mtx, MA_OWNED);
    val = PPBV_VID(pvid & PPBV_VID_MASK);
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_PPBV1(port), val);
    sc->hal.mt7531_switch_write(sc, MTKSWITCH_PPBV2(port), val);

    return (0);
}

static void
mtk_attach_switch_mt7531(struct mt7531_switch_softc *sc)
{

    sc->portmap = 0x7f;
    sc->phymap = 0x1f;

    sc->info.es_nports = 7;
    sc->info.es_vlan_caps = ETHERSWITCH_VLAN_DOT1Q;
    sc->info.es_nvlangroups = 16;
    sprintf(sc->info.es_name, "Mediatek GSW");

    sc->hal.mt7531_switch_read = mtkswitch_reg_read32_mt7621;
    sc->hal.mt7531_switch_write = mtkswitch_reg_write32_mt7621;
    sc->info.es_nvlangroups = 4096;
    
    sc->hal.mt7531_switch_reset = mtkswitch_reset;
    sc->hal.mt7531_switch_hw_setup = mtkswitch_hw_setup;
    sc->hal.mt7531_switch_hw_global_setup = mtkswitch_hw_global_setup;
    sc->hal.mt7531_switchport_init = mtkswitch_port_init;
    sc->hal.mt7531_switch_get_port_status = mtkswitch_get_port_status;
    sc->hal.mt7531_switch_atu_flush = mtkswitch_atu_flush;
    sc->hal.mt7531_switch_port_vlan_setup = mtkswitch_port_vlan_setup;
    sc->hal.mt7531_switch_port_vlan_get = mtkswitch_port_vlan_get;
    sc->hal.mt7531_switch_vlan_init_hw = mtkswitch_vlan_init_hw;
    sc->hal.mt7531_switch_vlan_getvgroup = mtkswitch_vlan_getvgroup;
    sc->hal.mt7531_switch_vlan_setvgroup = mtkswitch_vlan_setvgroup;
    sc->hal.mt7531_switch_vlan_get_pvid = mtkswitch_vlan_get_pvid;
    sc->hal.mt7531_switch_vlan_set_pvid = mtkswitch_vlan_set_pvid;
    sc->hal.mt7531_switch_phy_read = mtkswitch_phy_read;
    sc->hal.mt7531_switch_phy_write = mtkswitch_phy_write;
    sc->hal.mt7531_switch_reg_read = mtkswitch_reg_read;
    sc->hal.mt7531_switch_reg_write = mtkswitch_reg_write;
}