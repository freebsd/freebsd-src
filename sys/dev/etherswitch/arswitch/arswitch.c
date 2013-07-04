/*-
 * Copyright (c) 2011-2012 Stefan Bethke.
 * Copyright (c) 2012 Adrian Chadd.
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
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/etherswitch/mdio.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/arswitch/arswitchreg.h>
#include <dev/etherswitch/arswitch/arswitchvar.h>
#include <dev/etherswitch/arswitch/arswitch_reg.h>
#include <dev/etherswitch/arswitch/arswitch_phy.h>

#include <dev/etherswitch/arswitch/arswitch_7240.h>
#include <dev/etherswitch/arswitch/arswitch_8216.h>
#include <dev/etherswitch/arswitch/arswitch_8226.h>
#include <dev/etherswitch/arswitch/arswitch_8316.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

#if	defined(DEBUG)
static SYSCTL_NODE(_debug, OID_AUTO, arswitch, CTLFLAG_RD, 0, "arswitch");
#endif

static inline int arswitch_portforphy(int phy);
static void arswitch_tick(void *arg);
static int arswitch_ifmedia_upd(struct ifnet *);
static void arswitch_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static int
arswitch_probe(device_t dev)
{
	struct arswitch_softc *sc;
	uint32_t id;
	char *chipname, desc[256];

	sc = device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->page = -1;

	/* AR7240 probe */
	if (ar7240_probe(dev) == 0) {
		chipname = "AR7240";
		sc->sc_switchtype = AR8X16_SWITCH_AR7240;
		id = 0;
		goto done;
	}

	/* AR8xxx probe */
	id = arswitch_readreg(dev, AR8X16_REG_MASK_CTRL);
	switch ((id & AR8X16_MASK_CTRL_VER_MASK) >>
	    AR8X16_MASK_CTRL_VER_SHIFT) {
	case 1:
		chipname = "AR8216";
		sc->sc_switchtype = AR8X16_SWITCH_AR8216;
		break;
	case 2:
		chipname = "AR8226";
		sc->sc_switchtype = AR8X16_SWITCH_AR8226;
		break;
	case 16:
		chipname = "AR8316";
		sc->sc_switchtype = AR8X16_SWITCH_AR8316;
		break;
	default:
		chipname = NULL;
	}

done:
	DPRINTF(dev, "chipname=%s, rev=%02x\n", chipname,
	    id & AR8X16_MASK_CTRL_REV_MASK);
	if (chipname != NULL) {
		snprintf(desc, sizeof(desc),
		    "Atheros %s Ethernet Switch",
		    chipname);
		device_set_desc_copy(dev, desc);
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
arswitch_attach_phys(struct arswitch_softc *sc)
{
	int phy, err = 0;
	char name[IFNAMSIZ];

	/* PHYs need an interface, so we generate a dummy one */
	snprintf(name, IFNAMSIZ, "%sport", device_get_nameunit(sc->sc_dev));
	for (phy = 0; phy < sc->numphys; phy++) {
		sc->ifp[phy] = if_alloc(IFT_ETHER);
		sc->ifp[phy]->if_softc = sc;
		sc->ifp[phy]->if_flags |= IFF_UP | IFF_BROADCAST |
		    IFF_DRV_RUNNING | IFF_SIMPLEX;
		sc->ifname[phy] = malloc(strlen(name)+1, M_DEVBUF, M_WAITOK);
		bcopy(name, sc->ifname[phy], strlen(name)+1);
		if_initname(sc->ifp[phy], sc->ifname[phy],
		    arswitch_portforphy(phy));
		err = mii_attach(sc->sc_dev, &sc->miibus[phy], sc->ifp[phy],
		    arswitch_ifmedia_upd, arswitch_ifmedia_sts, \
		    BMSR_DEFCAPMASK, phy, MII_OFFSET_ANY, 0);
		DPRINTF(sc->sc_dev, "%s attached to pseudo interface %s\n",
		    device_get_nameunit(sc->miibus[phy]),
		    sc->ifp[phy]->if_xname);
		if (err != 0) {
			device_printf(sc->sc_dev,
			    "attaching PHY %d failed\n",
			    phy);
		}
	}
	return (err);
}

static int
arswitch_attach(device_t dev)
{
	struct arswitch_softc *sc;
	int err = 0;

	sc = device_get_softc(dev);

	/* sc->sc_switchtype is already decided in arswitch_probe() */
	sc->sc_dev = dev;
	mtx_init(&sc->sc_mtx, "arswitch", NULL, MTX_DEF);
	sc->page = -1;
	strlcpy(sc->info.es_name, device_get_desc(dev),
	    sizeof(sc->info.es_name));

	/*
	 * Attach switch related functions
	 */
	if (AR8X16_IS_SWITCH(sc, AR7240))
		ar7240_attach(sc);
	else if (AR8X16_IS_SWITCH(sc, AR8216))
		ar8216_attach(sc);
	else if (AR8X16_IS_SWITCH(sc, AR8226))
		ar8226_attach(sc);
	else if (AR8X16_IS_SWITCH(sc, AR8316))
		ar8316_attach(sc);
	else
		return (ENXIO);

	/*
	 * XXX these two should be part of the switch attach function
	 */
	sc->info.es_nports = 5; /* XXX technically 6, but 6th not used */
	sc->info.es_nvlangroups = 16;

	/* XXX Defaults for externally connected AR8316 */
	sc->numphys = 4;
	sc->phy4cpu = 1;
	sc->is_rgmii = 1;
	sc->is_gmii = 0;

	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "numphys", &sc->numphys);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "phy4cpu", &sc->phy4cpu);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "is_rgmii", &sc->is_rgmii);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "is_gmii", &sc->is_gmii);

	/*
	 * This requires much more setup depending upon each chip, including:
	 *
	 * + Proper reinitialisation of the PHYs;
	 * + Initialising the VLAN table;
	 * + Initialising the port access table and CPU flood/broadcast
	 *   configuration;
	 * + Other things I haven't yet thought of.
	 */
#ifdef NOTYET
	arswitch_writereg(dev, AR8X16_REG_MASK_CTRL,
	    AR8X16_MASK_CTRL_SOFT_RESET);
	DELAY(1000);
	if (arswitch_readreg(dev, AR8X16_REG_MASK_CTRL) &
	    AR8X16_MASK_CTRL_SOFT_RESET) {
		device_printf(dev, "unable to reset switch\n");
		return (ENXIO);
	}
#endif

	err = sc->hal.arswitch_hw_setup(sc);
	if (err != 0)
		return (err);

	err = sc->hal.arswitch_hw_global_setup(sc);
	if (err != 0)
		return (err);

	/*
	 * Attach the PHYs and complete the bus enumeration.
	 */
	err = arswitch_attach_phys(sc);
	if (err != 0)
		return (err);

	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);
	err = bus_generic_attach(dev);
	if (err != 0)
		return (err);
	
	callout_init_mtx(&sc->callout_tick, &sc->sc_mtx, 0);

	ARSWITCH_LOCK(sc);
	arswitch_tick(sc);
	ARSWITCH_UNLOCK(sc);
	
	return (err);
}

static int
arswitch_detach(device_t dev)
{
	struct arswitch_softc *sc = device_get_softc(dev);
	int i;

	callout_drain(&sc->callout_tick);

	for (i=0; i < sc->numphys; i++) {
		if (sc->miibus[i] != NULL)
			device_delete_child(dev, sc->miibus[i]);
		if (sc->ifp[i] != NULL)
			if_free(sc->ifp[i]);
		free(sc->ifname[i], M_DEVBUF);
	}

	bus_generic_detach(dev);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

/*
 * Convert PHY number to port number. PHY0 is connected to port 1, PHY1 to
 * port 2, etc.
 */
static inline int
arswitch_portforphy(int phy)
{
	return (phy+1);
}

static inline struct mii_data *
arswitch_miiforport(struct arswitch_softc *sc, int port)
{
	int phy = port-1;

	if (phy < 0 || phy >= sc->numphys)
		return (NULL);
	return (device_get_softc(sc->miibus[phy]));
}

static inline struct ifnet *
arswitch_ifpforport(struct arswitch_softc *sc, int port)
{
	int phy = port-1;

	if (phy < 0 || phy >= sc->numphys)
		return (NULL);
	return (sc->ifp[phy]);
}

/*
 * Convert port status to ifmedia.
 */
static void
arswitch_update_ifmedia(int portstatus, u_int *media_status, u_int *media_active)
{
	*media_active = IFM_ETHER;
	*media_status = IFM_AVALID;

	if ((portstatus & AR8X16_PORT_STS_LINK_UP) != 0)
		*media_status |= IFM_ACTIVE;
	else {
		*media_active |= IFM_NONE;
		return;
	}
	switch (portstatus & AR8X16_PORT_STS_SPEED_MASK) {
	case AR8X16_PORT_STS_SPEED_10:
		*media_active |= IFM_10_T;
		break;
	case AR8X16_PORT_STS_SPEED_100:
		*media_active |= IFM_100_TX;
		break;
	case AR8X16_PORT_STS_SPEED_1000:
		*media_active |= IFM_1000_T;
		break;
	}
	if ((portstatus & AR8X16_PORT_STS_DUPLEX) == 0)
		*media_active |= IFM_FDX;
	else
		*media_active |= IFM_HDX;
	if ((portstatus & AR8X16_PORT_STS_TXFLOW) != 0)
		*media_active |= IFM_ETH_TXPAUSE;
	if ((portstatus & AR8X16_PORT_STS_RXFLOW) != 0)
		*media_active |= IFM_ETH_RXPAUSE;
}

/*
 * Poll the status for all PHYs.  We're using the switch port status because
 * thats a lot quicker to read than talking to all the PHYs.  Care must be
 * taken that the resulting ifmedia_active is identical to what the PHY will
 * compute, or gratuitous link status changes will occur whenever the PHYs
 * update function is called.
 */
static void
arswitch_miipollstat(struct arswitch_softc *sc)
{
	int i;
	struct mii_data *mii;
	struct mii_softc *miisc;
	int portstatus;

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);

	for (i = 0; i < sc->numphys; i++) {
		if (sc->miibus[i] == NULL)
			continue;
		mii = device_get_softc(sc->miibus[i]);
		portstatus = arswitch_readreg(sc->sc_dev,
		    AR8X16_REG_PORT_STS(arswitch_portforphy(i)));
#if 0
		DPRINTF(sc->sc_dev, "p[%d]=%b\n",
		    arge_portforphy(i),
		    portstatus,
		    "\20\3TXMAC\4RXMAC\5TXFLOW\6RXFLOW\7"
		    "DUPLEX\11LINK_UP\12LINK_AUTO\13LINK_PAUSE");
#endif
		arswitch_update_ifmedia(portstatus, &mii->mii_media_status,
		    &mii->mii_media_active);
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list) {
			if (IFM_INST(mii->mii_media.ifm_cur->ifm_media) !=
			    miisc->mii_inst)
				continue;
			mii_phy_update(miisc, MII_POLLSTAT);
		}
	}
}

static void
arswitch_tick(void *arg)
{
	struct arswitch_softc *sc = arg;

	arswitch_miipollstat(sc);
	callout_reset(&sc->callout_tick, hz, arswitch_tick, sc);
}

static void
arswitch_lock(device_t dev)
{
	struct arswitch_softc *sc = device_get_softc(dev);

	ARSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);
	ARSWITCH_LOCK(sc);
}

static void
arswitch_unlock(device_t dev)
{
	struct arswitch_softc *sc = device_get_softc(dev);

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	ARSWITCH_UNLOCK(sc);
}

static etherswitch_info_t *
arswitch_getinfo(device_t dev)
{
	struct arswitch_softc *sc = device_get_softc(dev);
	
	return (&sc->info);
}

static int
arswitch_getport(device_t dev, etherswitch_port_t *p)
{
	struct arswitch_softc *sc = device_get_softc(dev);
	struct mii_data *mii;
	struct ifmediareq *ifmr = &p->es_ifmr;
	int err;
	
	if (p->es_port < 0 || p->es_port >= AR8X16_NUM_PORTS)
		return (ENXIO);
	p->es_pvid = 0;

	mii = arswitch_miiforport(sc, p->es_port);
	if (p->es_port == 0) {
		/* fill in fixed values for CPU port */
		p->es_flags |= ETHERSWITCH_PORT_CPU;
		ifmr->ifm_count = 0;
		ifmr->ifm_current = ifmr->ifm_active =
		    IFM_ETHER | IFM_1000_T | IFM_FDX;
		ifmr->ifm_mask = 0;
		ifmr->ifm_status = IFM_ACTIVE | IFM_AVALID;
	} else if (mii != NULL) {
		err = ifmedia_ioctl(mii->mii_ifp, &p->es_ifr,
		    &mii->mii_media, SIOCGIFMEDIA);
		if (err)
			return (err);
	} else {
		return (ENXIO);
	}
	return (0);
}

/*
 * XXX doesn't yet work?
 */
static int
arswitch_setport(device_t dev, etherswitch_port_t *p)
{
	int err;
	struct arswitch_softc *sc;
	struct ifmedia *ifm;
	struct mii_data *mii;
	struct ifnet *ifp;

	/*
	 * XXX check the sc numphys, or the #define ?
	 */
	if (p->es_port < 0 || p->es_port >= AR8X16_NUM_PHYS)
		return (ENXIO);

	sc = device_get_softc(dev);

	/*
	 * XXX TODO: don't set the CPU port?
	 */

	mii = arswitch_miiforport(sc, p->es_port);
	if (mii == NULL)
		return (ENXIO);

	ifp = arswitch_ifpforport(sc, p->es_port);

	ifm = &mii->mii_media;
	err = ifmedia_ioctl(ifp, &p->es_ifr, ifm, SIOCSIFMEDIA);
	return (err);
}

static int
arswitch_getvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{

	/* XXX not implemented yet */
	vg->es_vid = 0;
	vg->es_member_ports = 0;
	vg->es_untagged_ports = 0;
	vg->es_fid = 0;
	return (0);
}

static int
arswitch_setvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{

	/* XXX not implemented yet */
	return (0);
}

static void
arswitch_statchg(device_t dev)
{

	DPRINTF(dev, "%s\n", __func__);
}

static int
arswitch_ifmedia_upd(struct ifnet *ifp)
{
	struct arswitch_softc *sc = ifp->if_softc;
	struct mii_data *mii = arswitch_miiforport(sc, ifp->if_dunit);

	if (mii == NULL)
		return (ENXIO);
	mii_mediachg(mii);
	return (0);
}

static void
arswitch_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct arswitch_softc *sc = ifp->if_softc;
	struct mii_data *mii = arswitch_miiforport(sc, ifp->if_dunit);

	DPRINTF(sc->sc_dev, "%s\n", __func__);

	if (mii == NULL)
		return;
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static device_method_t arswitch_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		arswitch_probe),
	DEVMETHOD(device_attach,	arswitch_attach),
	DEVMETHOD(device_detach,	arswitch_detach),
	
	/* bus interface */
	DEVMETHOD(bus_add_child,	device_add_child_ordered),
	
	/* MII interface */
	DEVMETHOD(miibus_readreg,	arswitch_readphy),
	DEVMETHOD(miibus_writereg,	arswitch_writephy),
	DEVMETHOD(miibus_statchg,	arswitch_statchg),

	/* MDIO interface */
	DEVMETHOD(mdio_readreg,		arswitch_readphy),
	DEVMETHOD(mdio_writereg,	arswitch_writephy),

	/* etherswitch interface */
	DEVMETHOD(etherswitch_lock,	arswitch_lock),
	DEVMETHOD(etherswitch_unlock,	arswitch_unlock),
	DEVMETHOD(etherswitch_getinfo,	arswitch_getinfo),
	DEVMETHOD(etherswitch_readreg,	arswitch_readreg),
	DEVMETHOD(etherswitch_writereg,	arswitch_writereg),
	DEVMETHOD(etherswitch_readphyreg,	arswitch_readphy),
	DEVMETHOD(etherswitch_writephyreg,	arswitch_writephy),
	DEVMETHOD(etherswitch_getport,	arswitch_getport),
	DEVMETHOD(etherswitch_setport,	arswitch_setport),
	DEVMETHOD(etherswitch_getvgroup,	arswitch_getvgroup),
	DEVMETHOD(etherswitch_setvgroup,	arswitch_setvgroup),

	DEVMETHOD_END
};

DEFINE_CLASS_0(arswitch, arswitch_driver, arswitch_methods,
    sizeof(struct arswitch_softc));
static devclass_t arswitch_devclass;

DRIVER_MODULE(arswitch, mdio, arswitch_driver, arswitch_devclass, 0, 0);
DRIVER_MODULE(miibus, arswitch, miibus_driver, miibus_devclass, 0, 0);
DRIVER_MODULE(mdio, arswitch, mdio_driver, mdio_devclass, 0, 0);
DRIVER_MODULE(etherswitch, arswitch, etherswitch_driver, etherswitch_devclass, 0, 0);
MODULE_VERSION(arswitch, 1);
MODULE_DEPEND(arswitch, miibus, 1, 1, 1); /* XXX which versions? */
MODULE_DEPEND(arswitch, etherswitch, 1, 1, 1); /* XXX which versions? */
