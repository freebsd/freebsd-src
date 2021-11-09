/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <dev/enetc/enetc_mdio.h>

#include <dev/etherswitch/etherswitch.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/etherswitch/felix/felix_var.h>
#include <dev/etherswitch/felix/felix_reg.h>

#include "etherswitch_if.h"
#include "miibus_if.h"

MALLOC_DECLARE(M_FELIX);
MALLOC_DEFINE(M_FELIX, "felix", "felix switch");

static device_probe_t felix_probe;
static device_attach_t felix_attach;
static device_detach_t felix_detach;

static etherswitch_info_t* felix_getinfo(device_t);
static int felix_getconf(device_t, etherswitch_conf_t *);
static int felix_setconf(device_t, etherswitch_conf_t *);
static void felix_lock(device_t);
static void felix_unlock(device_t);
static int felix_getport(device_t, etherswitch_port_t *);
static int felix_setport(device_t, etherswitch_port_t *);
static int felix_readreg_wrapper(device_t, int);
static int felix_writereg_wrapper(device_t, int, int);
static int felix_readphy(device_t, int, int);
static int felix_writephy(device_t, int, int, int);
static int felix_setvgroup(device_t, etherswitch_vlangroup_t *);
static int felix_getvgroup(device_t, etherswitch_vlangroup_t *);

static int felix_parse_port_fdt(felix_softc_t, phandle_t, int *);
static int felix_setup(felix_softc_t);
static void felix_setup_port(felix_softc_t, int);

static void felix_tick(void *);
static int felix_ifmedia_upd(struct ifnet *);
static void felix_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static void felix_get_port_cfg(felix_softc_t, etherswitch_port_t *);
static void felix_set_port_cfg(felix_softc_t, etherswitch_port_t *);

static bool felix_is_phyport(felix_softc_t, int);
static struct mii_data *felix_miiforport(felix_softc_t, unsigned int);

static struct felix_pci_id felix_pci_ids[] = {
	{PCI_VENDOR_FREESCALE, FELIX_DEV_ID, FELIX_DEV_NAME},
	{0, 0, NULL}
};

static device_method_t felix_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,			felix_probe),
	DEVMETHOD(device_attach,		felix_attach),
	DEVMETHOD(device_detach,		felix_detach),

	/* bus interface */
	DEVMETHOD(bus_add_child,		device_add_child_ordered),

	/* etherswitch interface */
	DEVMETHOD(etherswitch_getinfo,		felix_getinfo),
	DEVMETHOD(etherswitch_getconf,		felix_getconf),
	DEVMETHOD(etherswitch_setconf,		felix_setconf),
	DEVMETHOD(etherswitch_lock,		felix_lock),
	DEVMETHOD(etherswitch_unlock,		felix_unlock),
	DEVMETHOD(etherswitch_getport,		felix_getport),
	DEVMETHOD(etherswitch_setport,		felix_setport),
	DEVMETHOD(etherswitch_readreg,		felix_readreg_wrapper),
	DEVMETHOD(etherswitch_writereg,		felix_writereg_wrapper),
	DEVMETHOD(etherswitch_readphyreg,	felix_readphy),
	DEVMETHOD(etherswitch_writephyreg,	felix_writephy),
	DEVMETHOD(etherswitch_setvgroup,	felix_setvgroup),
	DEVMETHOD(etherswitch_getvgroup,	felix_getvgroup),

	/* miibus interface */
	DEVMETHOD(miibus_readreg,		felix_readphy),
	DEVMETHOD(miibus_writereg,		felix_writephy),

	DEVMETHOD_END
};

static devclass_t felix_devclass;
DEFINE_CLASS_0(felix, felix_driver, felix_methods,
    sizeof(struct felix_softc));

DRIVER_MODULE_ORDERED(felix, pci, felix_driver, felix_devclass,
    NULL, NULL, SI_ORDER_ANY);
DRIVER_MODULE(miibus, felix, miibus_driver, miibus_devclass,
    NULL, NULL);
DRIVER_MODULE(etherswitch, felix, etherswitch_driver, etherswitch_devclass,
    NULL, NULL);
MODULE_VERSION(felix, 1);
MODULE_PNP_INFO("U16:vendor;U16:device;D:#", pci, felix,
    felix_pci_ids, nitems(felix_pci_ids) - 1);

static int
felix_probe(device_t dev)
{
	struct felix_pci_id *id;
	felix_softc_t sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	for (id = felix_pci_ids; id->vendor != 0; ++id) {
		if (pci_get_device(dev) != id->device ||
		    pci_get_vendor(dev) != id->vendor)
			continue;

		device_set_desc(dev, id->desc);

		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
felix_parse_port_fdt(felix_softc_t sc, phandle_t child, int *pport)
{
	uint32_t port, status;
	phandle_t node;

	if (OF_getencprop(child, "reg", (void *)&port, sizeof(port)) < 0) {
		device_printf(sc->dev, "Port node doesn't have reg property\n");
		return (ENXIO);
	}

	*pport = port;

	node = OF_getproplen(child, "ethernet");
	if (node <= 0)
		sc->ports[port].cpu_port = false;
	else
		sc->ports[port].cpu_port = true;

	node = ofw_bus_find_child(child, "fixed-link");
	if (node <= 0) {
		sc->ports[port].fixed_port = false;
		return (0);
	}

	sc->ports[port].fixed_port = true;;

	if (OF_getencprop(node, "speed", &status, sizeof(status)) <= 0) {
		device_printf(sc->dev,
		    "Port has fixed-link node without link speed specified\n");
		return (ENXIO);
        }

	switch (status) {
	case 2500:
		status = IFM_2500_T;
		break;
	case 1000:
		status = IFM_1000_T;
		break;
	case 100:
		status = IFM_100_T;
		break;
	case 10:
		status = IFM_10_T;
		break;
	default:
		device_printf(sc->dev,
		    "Unsupported link speed value of %d\n",
		    status);
		return (ENXIO);
	}

	if (OF_hasprop(node, "full-duplex"))
		status |= IFM_FDX;
	else
		status |= IFM_HDX;

	status |= IFM_ETHER;
	sc->ports[port].fixed_link_status = status;
	return (0);
}

static int
felix_init_interface(felix_softc_t sc, int port)
{
	char name[IFNAMSIZ];

	snprintf(name, IFNAMSIZ, "%sport", device_get_nameunit(sc->dev));

	sc->ports[port].ifp = if_alloc(IFT_ETHER);
	if (sc->ports[port].ifp == NULL)
		return (ENOMEM);

	sc->ports[port].ifp->if_softc = sc;
	sc->ports[port].ifp->if_flags = IFF_UP | IFF_BROADCAST | IFF_MULTICAST |
	    IFF_DRV_RUNNING | IFF_SIMPLEX;
	sc->ports[port].ifname = malloc(strlen(name) + 1, M_FELIX, M_NOWAIT);
	if (sc->ports[port].ifname == NULL) {
		if_free(sc->ports[port].ifp);
		return (ENOMEM);
	}

	memcpy(sc->ports[port].ifname, name, strlen(name) + 1);
	if_initname(sc->ports[port].ifp, sc->ports[port].ifname, port);
	return (0);
}

static void
felix_setup_port(felix_softc_t sc, int port)
{

	/* Link speed has to be always set to 1000 in the clock register. */
	FELIX_DEVGMII_PORT_WR4(sc, port, FELIX_DEVGMII_CLK_CFG,
	    FELIX_DEVGMII_CLK_CFG_SPEED_1000);
	FELIX_DEVGMII_PORT_WR4(sc, port, FELIX_DEVGMII_MAC_CFG,
	    FELIX_DEVGMII_MAC_CFG_TX_ENA | FELIX_DEVGMII_MAC_CFG_RX_ENA);
	FELIX_WR4(sc, FELIX_QSYS_PORT_MODE(port),
	    FELIX_QSYS_PORT_MODE_PORT_ENA);

	/*
	 * Enable "VLANMTU". Each port has a configurable MTU.
	 * Accept frames that are 8 and 4 bytes longer than it
	 * for double and single tagged frames respectively.
	 * Since etherswitch API doesn't provide an option to change
	 * MTU don't touch it for now.
	 */
	FELIX_DEVGMII_PORT_WR4(sc, port, FELIX_DEVGMII_VLAN_CFG,
	    FELIX_DEVGMII_VLAN_CFG_ENA |
	    FELIX_DEVGMII_VLAN_CFG_LEN_ENA |
	    FELIX_DEVGMII_VLAN_CFG_DOUBLE_ENA);
}

static int
felix_setup(felix_softc_t sc)
{
	int timeout, i;
	uint32_t reg;

	/* Trigger soft reset, bit is self-clearing, with 5s timeout. */
	FELIX_WR4(sc, FELIX_DEVCPU_GCB_RST, FELIX_DEVCPU_GCB_RST_EN);
	timeout = FELIX_INIT_TIMEOUT;
	do {
		DELAY(1000);
		reg = FELIX_RD4(sc, FELIX_DEVCPU_GCB_RST);
		if ((reg & FELIX_DEVCPU_GCB_RST_EN) == 0)
			break;
	} while (timeout-- > 0);
	if (timeout == 0) {
		device_printf(sc->dev,
		    "Timeout while waiting for switch to reset\n");
		return (ETIMEDOUT);
	}

	FELIX_WR4(sc, FELIX_SYS_RAM_CTRL, FELIX_SYS_RAM_CTRL_INIT);
	timeout = FELIX_INIT_TIMEOUT;
	do {
		DELAY(1000);
		reg = FELIX_RD4(sc, FELIX_SYS_RAM_CTRL);
		if ((reg & FELIX_SYS_RAM_CTRL_INIT) == 0)
			break;
	} while (timeout-- > 0);
	if (timeout == 0) {
		device_printf(sc->dev,
		    "Timeout while waiting for switch RAM init.\n");
		return (ETIMEDOUT);
	}

	FELIX_WR4(sc, FELIX_SYS_CFG, FELIX_SYS_CFG_CORE_EN);

	for (i = 0; i < sc->info.es_nports; i++)
		felix_setup_port(sc, i);

	return (0);
}

static int
felix_timer_rate(SYSCTL_HANDLER_ARGS)
{
	felix_softc_t sc;
	int error, value, old;

	sc = arg1;

	old = value = sc->timer_ticks;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (value < 0)
		return (EINVAL);

	if (value == old)
		return (0);

	FELIX_LOCK(sc);
	sc->timer_ticks = value;
	callout_reset(&sc->tick_callout, sc->timer_ticks, felix_tick, sc);
	FELIX_UNLOCK(sc);

	return (0);
}

static int
felix_attach(device_t dev)
{
	phandle_t child, ports, node;
	int error, port, rid;
	felix_softc_t sc;
	uint32_t phy_addr;
	ssize_t size;

	sc = device_get_softc(dev);
	sc->info.es_nports = 0;
	sc->info.es_vlan_caps = ETHERSWITCH_VLAN_DOT1Q;
	strlcpy(sc->info.es_name, "Felix TSN Switch", sizeof(sc->info.es_name));

	rid = PCIR_BAR(FELIX_BAR_MDIO);
	sc->mdio = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mdio == NULL) {
		device_printf(dev, "Failed to allocate MDIO registers.\n");
		return (ENXIO);
	}

	rid = PCIR_BAR(FELIX_BAR_REGS);
	sc->regs = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->regs == NULL) {
		device_printf(dev, "Failed to allocate registers BAR.\n");
		error = ENXIO;
		goto out_fail;
	}

	mtx_init(&sc->mtx, "felix lock",  NULL, MTX_DEF);
	callout_init_mtx(&sc->tick_callout, &sc->mtx, 0);

	node = ofw_bus_get_node(dev);
	if (node <= 0) {
		error = ENXIO;
		goto out_fail;
	}

	ports = ofw_bus_find_child(node, "ports");
	if (ports == 0) {
		device_printf(dev,
		    "Failed to find \"ports\" property in DTS.\n");
		error = ENXIO;
		goto out_fail;
	}

	for (child = OF_child(ports); child != 0; child = OF_peer(child)) {
		/* Do not parse disabled ports. */
		if (ofw_bus_node_status_okay(child) == 0)
			continue;

		error = felix_parse_port_fdt(sc, child, &port);
		if (error != 0)
			goto out_fail;

		error = felix_init_interface(sc, port);
		if (error != 0) {
			device_printf(sc->dev,
			    "Failed to initialize interface.\n");
			goto out_fail;
		}

		if (sc->ports[port].fixed_port) {
			sc->info.es_nports++;
			continue;
		}

		size = OF_getencprop(child, "phy-handle", &node, sizeof(node));
		if (size <= 0) {
			device_printf(sc->dev,
			    "Failed to acquire PHY handle from FDT.\n");
			error = ENXIO;
			goto out_fail;
		}

		node = OF_node_from_xref(node);
		size = OF_getencprop(node, "reg", &phy_addr, sizeof(phy_addr));
		if (size <= 0) {
			device_printf(sc->dev,
			    "Failed to obtain PHY address.\n");
			error = ENXIO;
			goto out_fail;
		}

		sc->ports[port].phyaddr = phy_addr;
		sc->ports[port].miibus = NULL;
		error = mii_attach(dev, &sc->ports[port].miibus, sc->ports[port].ifp,
		    felix_ifmedia_upd, felix_ifmedia_sts, BMSR_DEFCAPMASK,
		    phy_addr, MII_OFFSET_ANY, 0);
		if (error != 0)
			goto out_fail;

		sc->info.es_nports++;
	}

	error = felix_setup(sc);
	if (error != 0)
		goto out_fail;

	sc->timer_ticks = hz;	/* Default to 1s. */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "timer_ticks", CTLTYPE_INT | CTLFLAG_RW,
	    sc, 0, felix_timer_rate, "I",
	    "Number of ticks between timer invocations");

	/* The tick routine has to be called with the lock held. */
	FELIX_LOCK(sc);
	felix_tick(sc);
	FELIX_UNLOCK(sc);

	/* Allow etherswitch to attach as our child. */
	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);

out_fail:
	felix_detach(dev);
	return (error);
}

static int
felix_detach(device_t dev)
{
	felix_softc_t sc;
	int error;
	int i;

	error = 0;
	sc = device_get_softc(dev);
	bus_generic_detach(dev);

	mtx_lock(&sc->mtx);
	callout_stop(&sc->tick_callout);
	mtx_unlock(&sc->mtx);
	mtx_destroy(&sc->mtx);

	/*
	 * If we have been fully attached do a soft reset.
	 * This way after when driver is unloaded switch is left in unmanaged mode.
	 */
	if (device_is_attached(dev))
		felix_setup(sc);

	for (i = 0; i < sc->info.es_nports; i++) {
		if (sc->ports[i].miibus != NULL)
			device_delete_child(dev, sc->ports[i].miibus);
		if (sc->ports[i].ifp != NULL)
			if_free(sc->ports[i].ifp);
		if (sc->ports[i].ifname != NULL)
			free(sc->ports[i].ifname, M_FELIX);
	}

	if (sc->regs != NULL)
		error = bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->regs), sc->regs);

	if (sc->mdio != NULL)
		error = bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mdio), sc->mdio);

	return (error);
}

static etherswitch_info_t*
felix_getinfo(device_t dev)
{
	felix_softc_t sc;

	sc = device_get_softc(dev);
	return (&sc->info);
}

static int
felix_getconf(device_t dev, etherswitch_conf_t *conf)
{
	felix_softc_t sc;

	sc = device_get_softc(dev);

	/* Return the VLAN mode. */
	conf->cmd = ETHERSWITCH_CONF_VLAN_MODE;
	conf->vlan_mode = sc->vlan_mode;
	return (0);
}

static int
felix_init_vlan(felix_softc_t sc)
{
	int timeout = FELIX_INIT_TIMEOUT;
	uint32_t reg;
	int i;

	/* Flush VLAN table in hardware. */
	FELIX_WR4(sc, FELIX_ANA_VT, FELIX_ANA_VT_RESET);
	do {
		DELAY(1000);
		reg = FELIX_RD4(sc, FELIX_ANA_VT);
		if ((reg & FELIX_ANA_VT_STS) == FELIX_ANA_VT_IDLE)
			break;
	} while (timeout-- > 0);
	if (timeout == 0) {
		device_printf(sc->dev,
		    "Timeout during VLAN table reset.\n");
		return (ETIMEDOUT);
	}

	/* Flush VLAN table in sc. */
	for (i = 0; i < sc->info.es_nvlangroups; i++)
		sc->vlans[i] = 0;

	/*
	 * Make all ports VLAN aware.
	 * Read VID from incoming frames and use it for port grouping
	 * purposes.
	 * Don't set this if pvid is set.
	 */
	for (i = 0; i < sc->info.es_nports; i++) {
		reg = FELIX_ANA_PORT_RD4(sc, i, FELIX_ANA_PORT_VLAN_CFG);
		if ((reg & FELIX_ANA_PORT_VLAN_CFG_VID_MASK) != 0)
			continue;

		reg |= FELIX_ANA_PORT_VLAN_CFG_VID_AWARE;
		FELIX_ANA_PORT_WR4(sc, i, FELIX_ANA_PORT_VLAN_CFG, reg);
	}
	return (0);
}

static int
felix_setconf(device_t dev, etherswitch_conf_t *conf)
{
	felix_softc_t sc;
	int error;

	error = 0;
	/* Set the VLAN mode. */
	sc = device_get_softc(dev);
	FELIX_LOCK(sc);
	if (conf->cmd & ETHERSWITCH_CONF_VLAN_MODE) {
		switch (conf->vlan_mode) {
		case ETHERSWITCH_VLAN_DOT1Q:
			sc->vlan_mode = ETHERSWITCH_VLAN_DOT1Q;
			sc->info.es_nvlangroups = FELIX_NUM_VLANS;
			error = felix_init_vlan(sc);
			break;
		default:
			error = EINVAL;
		}
	}
	FELIX_UNLOCK(sc);
	return (error);
}

static void
felix_lock(device_t dev)
{
	felix_softc_t sc;

	sc = device_get_softc(dev);
	FELIX_LOCK_ASSERT(sc, MA_NOTOWNED);
	FELIX_LOCK(sc);
}

static void
felix_unlock(device_t dev)
{
	felix_softc_t sc;

	sc = device_get_softc(dev);
	FELIX_LOCK_ASSERT(sc, MA_OWNED);
	FELIX_UNLOCK(sc);
}

static void
felix_get_port_cfg(felix_softc_t sc, etherswitch_port_t *p)
{
	uint32_t reg;

	p->es_flags = 0;

	reg = FELIX_ANA_PORT_RD4(sc, p->es_port, FELIX_ANA_PORT_DROP_CFG);
	if (reg & FELIX_ANA_PORT_DROP_CFG_TAGGED)
		p->es_flags |= ETHERSWITCH_PORT_DROPTAGGED;

	if (reg & FELIX_ANA_PORT_DROP_CFG_UNTAGGED)
		p->es_flags |= ETHERSWITCH_PORT_DROPUNTAGGED;

	reg = FELIX_DEVGMII_PORT_RD4(sc, p->es_port, FELIX_DEVGMII_VLAN_CFG);
	if (reg & FELIX_DEVGMII_VLAN_CFG_DOUBLE_ENA)
		p->es_flags |= ETHERSWITCH_PORT_DOUBLE_TAG;

	reg = FELIX_REW_PORT_RD4(sc, p->es_port, FELIX_REW_PORT_TAG_CFG);
	if (reg & FELIX_REW_PORT_TAG_CFG_ALL)
		p->es_flags |= ETHERSWITCH_PORT_ADDTAG;

	reg = FELIX_ANA_PORT_RD4(sc, p->es_port, FELIX_ANA_PORT_VLAN_CFG);
	if (reg & FELIX_ANA_PORT_VLAN_CFG_POP)
		p->es_flags |= ETHERSWITCH_PORT_STRIPTAGINGRESS;

	p->es_pvid = reg & FELIX_ANA_PORT_VLAN_CFG_VID_MASK;
}

static int
felix_getport(device_t dev, etherswitch_port_t *p)
{
	struct ifmediareq *ifmr;
	struct mii_data *mii;
	felix_softc_t sc;
	int error;

	error = 0;
	sc = device_get_softc(dev);
	FELIX_LOCK_ASSERT(sc, MA_NOTOWNED);

	if (p->es_port >= sc->info.es_nports || p->es_port < 0)
		return (EINVAL);

	FELIX_LOCK(sc);
	felix_get_port_cfg(sc, p);
	if (sc->ports[p->es_port].fixed_port) {
		ifmr = &p->es_ifmr;
		ifmr->ifm_status = IFM_ACTIVE | IFM_AVALID;
		ifmr->ifm_count = 0;
		ifmr->ifm_active = sc->ports[p->es_port].fixed_link_status;
		ifmr->ifm_current = ifmr->ifm_active;
		ifmr->ifm_mask = 0;
	} else {
		mii = felix_miiforport(sc, p->es_port);
		error = ifmedia_ioctl(mii->mii_ifp, &p->es_ifr,
		    &mii->mii_media, SIOCGIFMEDIA);
	}
	FELIX_UNLOCK(sc);
	return (error);
}

static void
felix_set_port_cfg(felix_softc_t sc, etherswitch_port_t *p)
{
	uint32_t reg;

	reg = FELIX_ANA_PORT_RD4(sc, p->es_port, FELIX_ANA_PORT_DROP_CFG);
	if (p->es_flags & ETHERSWITCH_PORT_DROPTAGGED)
		reg |= FELIX_ANA_PORT_DROP_CFG_TAGGED;
	else
		reg &= ~FELIX_ANA_PORT_DROP_CFG_TAGGED;

	if (p->es_flags & ETHERSWITCH_PORT_DROPUNTAGGED)
		reg |= FELIX_ANA_PORT_DROP_CFG_UNTAGGED;
	else
		reg &= ~FELIX_ANA_PORT_DROP_CFG_UNTAGGED;

	FELIX_ANA_PORT_WR4(sc, p->es_port, FELIX_ANA_PORT_DROP_CFG, reg);

	reg = FELIX_REW_PORT_RD4(sc, p->es_port, FELIX_REW_PORT_TAG_CFG);
	if (p->es_flags & ETHERSWITCH_PORT_ADDTAG)
		reg |= FELIX_REW_PORT_TAG_CFG_ALL;
	else
		reg &= ~FELIX_REW_PORT_TAG_CFG_ALL;

	FELIX_REW_PORT_WR4(sc, p->es_port, FELIX_REW_PORT_TAG_CFG, reg);

	reg = FELIX_ANA_PORT_RD4(sc, p->es_port, FELIX_ANA_PORT_VLAN_CFG);
	if (p->es_flags & ETHERSWITCH_PORT_STRIPTAGINGRESS)
		reg |= FELIX_ANA_PORT_VLAN_CFG_POP;
	else
		reg &= ~FELIX_ANA_PORT_VLAN_CFG_POP;

	reg &= ~FELIX_ANA_PORT_VLAN_CFG_VID_MASK;
	reg |= p->es_pvid & FELIX_ANA_PORT_VLAN_CFG_VID_MASK;
	/*
	 * If port VID is set use it for VLAN classification,
	 * instead of frame VID.
	 * By default the frame tag takes precedence.
	 * Force the switch to ignore it.
	 */
	if (p->es_pvid != 0)
		reg &= ~FELIX_ANA_PORT_VLAN_CFG_VID_AWARE;
	else
		reg |= FELIX_ANA_PORT_VLAN_CFG_VID_AWARE;

	FELIX_ANA_PORT_WR4(sc, p->es_port, FELIX_ANA_PORT_VLAN_CFG, reg);
}

static int
felix_setport(device_t dev, etherswitch_port_t *p)
{
	felix_softc_t sc;
	struct mii_data *mii;
	int error;

	error = 0;
	sc = device_get_softc(dev);
	FELIX_LOCK_ASSERT(sc, MA_NOTOWNED);

	if (p->es_port >= sc->info.es_nports || p->es_port < 0)
		return (EINVAL);

	FELIX_LOCK(sc);
	felix_set_port_cfg(sc, p);
	if (felix_is_phyport(sc, p->es_port)) {
		mii = felix_miiforport(sc, p->es_port);
		error = ifmedia_ioctl(mii->mii_ifp, &p->es_ifr, &mii->mii_media,
		    SIOCSIFMEDIA);
	}
	FELIX_UNLOCK(sc);

	return (error);
}

static int
felix_readreg_wrapper(device_t dev, int addr_reg)
{
	felix_softc_t sc;

	sc = device_get_softc(dev);
	if (addr_reg > rman_get_size(sc->regs))
		return (UINT32_MAX);	/* Can't return errors here. */

	return (FELIX_RD4(sc, addr_reg));
}

static int
felix_writereg_wrapper(device_t dev, int addr_reg, int val)
{
	felix_softc_t sc;

	sc = device_get_softc(dev);
	if (addr_reg > rman_get_size(sc->regs))
		return (EINVAL);

	FELIX_WR4(sc, addr_reg, val);
	return (0);
}

static int
felix_readphy(device_t dev, int phy, int reg)
{
	felix_softc_t sc;

	sc = device_get_softc(dev);

	return (enetc_mdio_read(sc->mdio, FELIX_MDIO_BASE, phy, reg));
}

static int
felix_writephy(device_t dev, int phy, int reg, int data)
{
	felix_softc_t sc;

	sc = device_get_softc(dev);

	return (enetc_mdio_write(sc->mdio, FELIX_MDIO_BASE, phy, reg, data));
}

static int
felix_set_dot1q_vlan(felix_softc_t sc, etherswitch_vlangroup_t *vg)
{
	uint32_t reg;
	int i, vid;

	vid = vg->es_vid & ETHERSWITCH_VID_MASK;

	/* Tagged mode is not supported. */
	if (vg->es_member_ports != vg->es_untagged_ports)
		return (EINVAL);

	/*
	 * Hardware support 4096 groups, but we can't do group_id == vid.
	 * Note that hw_group_id == vid.
	 */
	if (vid == 0) {
		/* Clear VLAN table entry using old VID. */
		FELIX_WR4(sc, FELIX_ANA_VTIDX, sc->vlans[vg->es_vlangroup]);
		FELIX_WR4(sc, FELIX_ANA_VT, FELIX_ANA_VT_WRITE);
		sc->vlans[vg->es_vlangroup] = 0;
		return (0);
	}

	/* The VID is already used in a different group. */
	for (i = 0; i < sc->info.es_nvlangroups; i++)
		if (i != vg->es_vlangroup && vid == sc->vlans[i])
			return (EINVAL);

	/* This group already uses a different VID. */
	if (sc->vlans[vg->es_vlangroup] != 0 &&
	    sc->vlans[vg->es_vlangroup] != vid)
		return (EINVAL);

	sc->vlans[vg->es_vlangroup] = vid;

	/* Assign members to the given group. */
	reg = vg->es_member_ports & FELIX_ANA_VT_PORTMASK_MASK;
	reg <<= FELIX_ANA_VT_PORTMASK_SHIFT;
	reg |= FELIX_ANA_VT_WRITE;

	FELIX_WR4(sc, FELIX_ANA_VTIDX, vid);
	FELIX_WR4(sc, FELIX_ANA_VT, reg);

	/*
	 * According to documentation read and write commands
	 * are instant.
	 * Add a small delay just to be safe.
	 */
	mb();
	DELAY(100);
	reg = FELIX_RD4(sc, FELIX_ANA_VT);
	if ((reg & FELIX_ANA_VT_STS) != FELIX_ANA_VT_IDLE)
		return (ENXIO);

	return (0);
}

static int
felix_setvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{
	felix_softc_t sc;
	int error;

	sc = device_get_softc(dev);

	FELIX_LOCK(sc);
	if (sc->vlan_mode == ETHERSWITCH_VLAN_DOT1Q)
		error = felix_set_dot1q_vlan(sc, vg);
	else
		error = EINVAL;

	FELIX_UNLOCK(sc);
	return (error);
}

static int
felix_get_dot1q_vlan(felix_softc_t sc, etherswitch_vlangroup_t *vg)
{
	uint32_t reg;
	int vid;

	vid = sc->vlans[vg->es_vlangroup];

	if (vid == 0)
		return (0);

	FELIX_WR4(sc, FELIX_ANA_VTIDX, vid);
	FELIX_WR4(sc, FELIX_ANA_VT, FELIX_ANA_VT_READ);

	/*
	 * According to documentation read and write commands
	 * are instant.
	 * Add a small delay just to be safe.
	 */
	mb();
	DELAY(100);
	reg = FELIX_RD4(sc, FELIX_ANA_VT);
	if ((reg & FELIX_ANA_VT_STS) != FELIX_ANA_VT_IDLE)
		return (ENXIO);

	reg >>= FELIX_ANA_VT_PORTMASK_SHIFT;
	reg &= FELIX_ANA_VT_PORTMASK_MASK;

	vg->es_untagged_ports = vg->es_member_ports = reg;
	vg->es_fid = 0;
	vg->es_vid = vid | ETHERSWITCH_VID_VALID;
	return (0);
}

static int
felix_getvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{
	felix_softc_t sc;
	int error;

	sc = device_get_softc(dev);

	FELIX_LOCK(sc);
	if (sc->vlan_mode == ETHERSWITCH_VLAN_DOT1Q)
		error = felix_get_dot1q_vlan(sc, vg);
	else
		error = EINVAL;

	FELIX_UNLOCK(sc);
	return (error);
}

static void
felix_tick(void *arg)
{
	struct mii_data *mii;
	felix_softc_t sc;
	int port;

	sc = arg;

	FELIX_LOCK_ASSERT(sc, MA_OWNED);

	for (port = 0; port < sc->info.es_nports; port++) {
		if (!felix_is_phyport(sc, port))
			continue;

		mii = felix_miiforport(sc, port);
		MPASS(mii != NULL);
		mii_tick(mii);
	}
	if (sc->timer_ticks != 0)
		callout_reset(&sc->tick_callout, sc->timer_ticks, felix_tick, sc);
}

static int
felix_ifmedia_upd(struct ifnet *ifp)
{
	struct mii_data *mii;
	felix_softc_t sc;

	sc = ifp->if_softc;
	mii = felix_miiforport(sc, ifp->if_dunit);
	if (mii == NULL)
		return (ENXIO);

	mii_mediachg(mii);
	return (0);
}

static void
felix_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	felix_softc_t sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
	mii = felix_miiforport(sc, ifp->if_dunit);
	if (mii == NULL)
		return;

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static  bool
felix_is_phyport(felix_softc_t sc, int port)
{

	return (!sc->ports[port].fixed_port);
}

static  struct mii_data*
felix_miiforport(felix_softc_t sc, unsigned int port)
{

	if (!felix_is_phyport(sc, port))
		return (NULL);

	return (device_get_softc(sc->ports[port].miibus));
}
