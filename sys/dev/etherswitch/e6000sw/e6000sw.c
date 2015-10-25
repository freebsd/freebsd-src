/*-
 * Copyright (c) 2015 Semihalf
 * Copyright (c) 2015 Stormshield
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/fcntl.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <arm/mv/mvwin.h>
#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

#include <dev/etherswitch/etherswitch.h>
#include <dev/etherswitch/mdio.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mge/if_mgevar.h>

#include "e6000swreg.h"
#include "etherswitch_if.h"
#include "miibus_if.h"
#include "mdio_if.h"

MALLOC_DECLARE(M_E6000SW);
MALLOC_DEFINE(M_E6000SW, "e6000sw", "e6000sw switch");

#define E6000SW_LOCK(_sc)			\
	    sx_xlock(&(_sc)->sx)
#define E6000SW_UNLOCK(_sc)			\
	    sx_unlock(&(_sc)->sx)
#define E6000SW_LOCK_ASSERT(_sc, _what)		\
	    sx_assert(&(_sc)->sx, (_what))
#define E6000SW_TRYLOCK(_sc)			\
	    sx_tryxlock(&(_sc)->sx)

typedef struct e6000sw_softc {
	device_t		dev;

	struct sx		sx;
	struct ifnet		*ifp[E6000SW_NUM_PHYS];
	char			*ifname[E6000SW_NUM_PHYS];
	device_t		miibus[E6000SW_NUM_PHYS];
	struct mii_data		*mii[E6000SW_NUM_PHYS];
	struct callout		tick_callout;

	uint32_t		cpuports_mask;

	int			vid[E6000SW_NUM_VGROUPS];
	int			members[E6000SW_NUM_VGROUPS];
	int			vgroup[E6000SW_NUM_PORTS];
} e6000sw_softc_t;

static etherswitch_info_t etherswitch_info = {
	.es_nports =		E6000SW_NUM_PORTS,
	.es_nvlangroups =	E6000SW_NUM_VGROUPS,
	.es_name =		"Marvell 6000 series switch"
};

static void e6000sw_identify(driver_t *driver, device_t parent);
static int e6000sw_probe(device_t dev);
static int e6000sw_attach(device_t dev);
static int e6000sw_detach(device_t dev);
static int e6000sw_readphy(device_t dev, int phy, int reg);
static int e6000sw_writephy(device_t dev, int phy, int reg, int data);
static etherswitch_info_t* e6000sw_getinfo(device_t dev);
static void e6000sw_lock(device_t dev);
static void e6000sw_unlock(device_t dev);
static int e6000sw_getport(device_t dev, etherswitch_port_t *p);
static int e6000sw_setport(device_t dev, etherswitch_port_t *p);
static int e6000sw_readreg_wrapper(device_t dev, int addr_reg);
static int e6000sw_writereg_wrapper(device_t dev, int addr_reg, int val);
static int e6000sw_readphy_wrapper(device_t dev, int phy, int reg);
static int e6000sw_writephy_wrapper(device_t dev, int phy, int reg, int data);
static int e6000sw_getvgroup_wrapper(device_t dev, etherswitch_vlangroup_t *vg);
static int e6000sw_setvgroup_wrapper(device_t dev, etherswitch_vlangroup_t *vg);
static int e6000sw_setvgroup(device_t dev, etherswitch_vlangroup_t *vg);
static int e6000sw_getvgroup(device_t dev, etherswitch_vlangroup_t *vg);
static void e6000sw_setup(device_t dev, e6000sw_softc_t *sc);
static void e6000sw_port_vlan_conf(e6000sw_softc_t *sc);
static void e6000sw_tick(void *arg);
static void e6000sw_set_atustat(device_t dev, e6000sw_softc_t *sc, int bin,
    int flag);
static int e6000sw_atu_flush(device_t dev, e6000sw_softc_t *sc, int flag);
static __inline void e6000sw_writereg(e6000sw_softc_t *sc, int addr, int reg,
    int val);
static __inline uint32_t e6000sw_readreg(e6000sw_softc_t *sc, int addr,
    int reg);
static int e6000sw_ifmedia_upd(struct ifnet *ifp);
static void e6000sw_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
static int e6000sw_atu_mac_table(device_t dev, e6000sw_softc_t *sc, struct
    atu_opt *atu, int flag);
static int e6000sw_get_pvid(e6000sw_softc_t *sc, int port, int *pvid);
static int e6000sw_set_pvid(e6000sw_softc_t *sc, int port, int pvid);
static __inline int e6000sw_cpuport(e6000sw_softc_t *sc, int port);
static __inline struct mii_data *e6000sw_miiforphy(e6000sw_softc_t *sc,
    unsigned int phy);

static struct proc *e6000sw_kproc;

static device_method_t e6000sw_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,		e6000sw_identify),
	DEVMETHOD(device_probe,			e6000sw_probe),
	DEVMETHOD(device_attach,		e6000sw_attach),
	DEVMETHOD(device_detach,		e6000sw_detach),

	/* bus interface */
	DEVMETHOD(bus_add_child,		device_add_child_ordered),

	/* mii interface */
	DEVMETHOD(miibus_readreg,		e6000sw_readphy),
	DEVMETHOD(miibus_writereg,		e6000sw_writephy),

	/* etherswitch interface */
	DEVMETHOD(etherswitch_getinfo,		e6000sw_getinfo),
	DEVMETHOD(etherswitch_lock,		e6000sw_lock),
	DEVMETHOD(etherswitch_unlock,		e6000sw_unlock),
	DEVMETHOD(etherswitch_getport,		e6000sw_getport),
	DEVMETHOD(etherswitch_setport,		e6000sw_setport),
	DEVMETHOD(etherswitch_readreg,		e6000sw_readreg_wrapper),
	DEVMETHOD(etherswitch_writereg,		e6000sw_writereg_wrapper),
	DEVMETHOD(etherswitch_readphyreg,	e6000sw_readphy_wrapper),
	DEVMETHOD(etherswitch_writephyreg,	e6000sw_writephy_wrapper),
	DEVMETHOD(etherswitch_setvgroup,	e6000sw_setvgroup_wrapper),
	DEVMETHOD(etherswitch_getvgroup,	e6000sw_getvgroup_wrapper),

	DEVMETHOD_END
};

static devclass_t e6000sw_devclass;

DEFINE_CLASS_0(e6000sw, e6000sw_driver, e6000sw_methods,
    sizeof(e6000sw_softc_t));

DRIVER_MODULE(e6000sw, mdio, e6000sw_driver, e6000sw_devclass, 0, 0);
DRIVER_MODULE(etherswitch, e6000sw, etherswitch_driver, etherswitch_devclass, 0,
    0);
DRIVER_MODULE(miibus, e6000sw, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(e6000sw, mdio, 1, 1, 1);

static void
e6000sw_identify(driver_t *driver, device_t parent)
{

	if (device_find_child(parent, "e6000sw", -1) == NULL)
		BUS_ADD_CHILD(parent, 0, "e6000sw", -1);
}

static int
e6000sw_probe(device_t dev)
{
	e6000sw_softc_t *sc;
	const char *description;
	unsigned int id;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(e6000sw_softc_t));
	sc->dev = dev;
	/* Lock is necessary due to assertions. */
	sx_init(&sc->sx, "e6000sw");
	E6000SW_LOCK(sc);

	id = e6000sw_readreg(sc, REG_PORT(0), SWITCH_ID);

	switch (id & 0xfff0) {
	case 0x3520:
		description = "Marvell 88E6352";
		break;
	case 0x1720:
		description = "Marvell 88E6172";
		break;
	case 0x1760:
		description = "Marvell 88E6176";
		break;
	default:
		E6000SW_UNLOCK(sc);
		sx_destroy(&sc->sx);
		device_printf(dev, "Unrecognized device.\n");
		return (ENXIO);
	}

	device_set_desc(dev, description);

	E6000SW_UNLOCK(sc);

	return (BUS_PROBE_DEFAULT);
}

static int
e6000sw_attach(device_t dev)
{
	e6000sw_softc_t *sc;
	int phy, err, port;
	char name[IFNAMSIZ];

	err = 0;
	sc = device_get_softc(dev);
	E6000SW_LOCK(sc);
	sc->cpuports_mask = E6000SW_CPUPORTS_MASK;
	for (port = 0; port < E6000SW_NUM_PORTS; port++)
		sc->vgroup[port] = E6000SW_PORT_NO_VGROUP;
	e6000sw_setup(dev, sc);

	snprintf(name, IFNAMSIZ, "%sport", device_get_nameunit(sc->dev));
	for (phy = 0; phy < E6000SW_NUM_PHYS; phy++) {
		sc->ifp[phy] = if_alloc(IFT_ETHER);
		if (sc->ifp[phy] == NULL)
			goto out_fail;
		sc->ifp[phy]->if_softc = sc;
		sc->ifp[phy]->if_flags |= IFF_UP | IFF_BROADCAST |
		    IFF_DRV_RUNNING | IFF_SIMPLEX;
		sc->ifname[phy] = malloc(strlen(name) + 1, M_E6000SW, M_WAITOK);
		if (sc->ifname[phy] == NULL)
			goto out_fail;
		bcopy(name, sc->ifname[phy], strlen(name) + 1);
		if_initname(sc->ifp[phy], sc->ifname[phy], phy);
		err = mii_attach(sc->dev, &sc->miibus[phy], sc->ifp[phy],
		    e6000sw_ifmedia_upd, e6000sw_ifmedia_sts, BMSR_DEFCAPMASK,
		    phy, MII_OFFSET_ANY, 0);
		if (err != 0) {
			device_printf(sc->dev,
			    "attaching PHY %d failed\n",
			    phy);
			goto out_fail;
		}
		sc->mii[phy] = device_get_softc(sc->miibus[phy]);
	}
	E6000SW_UNLOCK(sc);

	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);
	bus_generic_attach(dev);

	kproc_create(e6000sw_tick, sc, &e6000sw_kproc, 0, 0,
	    "e6000sw tick kproc");

	return (0);

out_fail:
	e6000sw_detach(dev);

	return (ENXIO);
}

static __inline void
e6000sw_poll_done(e6000sw_softc_t *sc)
{

	while (e6000sw_readreg(sc, REG_GLOBAL2, PHY_CMD) &
	    (1 << PHY_CMD_SMI_BUSY))
		continue;
}


/*
 * PHY registers are paged. Put page index in reg 22 (accessible from every
 * page), then access specific register.
 */
static int
e6000sw_readphy(device_t dev, int phy, int reg)
{
	e6000sw_softc_t *sc;
	uint32_t val;

	sc = device_get_softc(dev);
	val = 0;

	if (phy >= E6000SW_NUM_PHYS || reg >= E6000SW_NUM_PHY_REGS) {
		device_printf(dev, "Wrong register address.\n");
		return (EINVAL);
	}

	E6000SW_LOCK_ASSERT(sc, SA_XLOCKED);

	e6000sw_poll_done(sc);
	val |= 1 << PHY_CMD_SMI_BUSY;
	val |= PHY_CMD_MODE_MDIO << PHY_CMD_MODE;
	val |= PHY_CMD_OPCODE_READ << PHY_CMD_OPCODE;
	val |= (reg << PHY_CMD_REG_ADDR) & PHY_CMD_REG_ADDR_MASK;
	val |= (phy << PHY_CMD_DEV_ADDR) & PHY_CMD_DEV_ADDR_MASK;
	e6000sw_writereg(sc, REG_GLOBAL2, SMI_PHY_CMD_REG, val);
	e6000sw_poll_done(sc);
	val = e6000sw_readreg(sc, REG_GLOBAL2, SMI_PHY_DATA_REG)
		& PHY_DATA_MASK;

	return (val);
}

static int
e6000sw_writephy(device_t dev, int phy, int reg, int data)
{
	e6000sw_softc_t *sc;
	uint32_t val;

	sc = device_get_softc(dev);
	val = 0;

	if (phy >= E6000SW_NUM_PHYS || reg >= E6000SW_NUM_PHY_REGS) {
		device_printf(dev, "Wrong register address.\n");
		return (EINVAL);
	}

	E6000SW_LOCK_ASSERT(sc, SA_XLOCKED);

	e6000sw_poll_done(sc);
	val |= PHY_CMD_MODE_MDIO << PHY_CMD_MODE;
	val |= 1 << PHY_CMD_SMI_BUSY;
	val |= PHY_CMD_OPCODE_WRITE << PHY_CMD_OPCODE;
	val |= (reg << PHY_CMD_REG_ADDR) & PHY_CMD_REG_ADDR_MASK;
	val |= (phy << PHY_CMD_DEV_ADDR) & PHY_CMD_DEV_ADDR_MASK;
	e6000sw_writereg(sc, REG_GLOBAL2, SMI_PHY_DATA_REG,
			 data & PHY_DATA_MASK);
	e6000sw_writereg(sc, REG_GLOBAL2, SMI_PHY_CMD_REG, val);
	e6000sw_poll_done(sc);

	return (0);
}

static int
e6000sw_detach(device_t dev)
{
	int phy;
	e6000sw_softc_t *sc;

	sc = device_get_softc(dev);
	bus_generic_detach(dev);
	sx_destroy(&sc->sx);
	for (phy = 0; phy < E6000SW_NUM_PHYS; phy++) {
		if (sc->miibus[phy] != NULL)
			device_delete_child(dev, sc->miibus[phy]);
		if (sc->ifp[phy] != NULL)
			if_free(sc->ifp[phy]);
		if (sc->ifname[phy] != NULL)
			free(sc->ifname[phy], M_E6000SW);
	}

	return (0);
}

static etherswitch_info_t*
e6000sw_getinfo(device_t dev)
{

	return (&etherswitch_info);
}

static void
e6000sw_lock(device_t dev)
{
	struct e6000sw_softc *sc;

	sc = device_get_softc(dev);

	E6000SW_LOCK_ASSERT(sc, SA_UNLOCKED);
	E6000SW_LOCK(sc);
}

static void
e6000sw_unlock(device_t dev)
{
	struct e6000sw_softc *sc;

	sc = device_get_softc(dev);

	E6000SW_LOCK_ASSERT(sc, SA_XLOCKED);
	E6000SW_UNLOCK(sc);
}

static int
e6000sw_getport(device_t dev, etherswitch_port_t *p)
{
	struct mii_data *mii;
	int err;
	struct ifmediareq *ifmr;

	err = 0;
	e6000sw_softc_t *sc = device_get_softc(dev);
	E6000SW_LOCK_ASSERT(sc, SA_UNLOCKED);

	E6000SW_LOCK(sc);

	if (p->es_port >= E6000SW_NUM_PORTS ||
	    p->es_port < 0) {
		err = EINVAL;
		goto out;
	}

	e6000sw_get_pvid(sc, p->es_port, &p->es_pvid);

	if (e6000sw_cpuport(sc, p->es_port)) {
		p->es_flags |= ETHERSWITCH_PORT_CPU;
		ifmr = &p->es_ifmr;
		ifmr->ifm_status = IFM_ACTIVE | IFM_AVALID;
		ifmr->ifm_count = 0;
		ifmr->ifm_current = ifmr->ifm_active =
		    IFM_ETHER | IFM_1000_T | IFM_FDX;
		ifmr->ifm_mask = 0;
	} else {
		mii = e6000sw_miiforphy(sc, p->es_port);
		err = ifmedia_ioctl(mii->mii_ifp, &p->es_ifr,
		    &mii->mii_media, SIOCGIFMEDIA);
	}

out:
	E6000SW_UNLOCK(sc);
	return (err);
}

static int
e6000sw_setport(device_t dev, etherswitch_port_t *p)
{
	e6000sw_softc_t *sc;
	int err;
	struct mii_data *mii;

	err = 0;
	sc = device_get_softc(dev);
	E6000SW_LOCK_ASSERT(sc, SA_UNLOCKED);

	E6000SW_LOCK(sc);

	if (p->es_port >= E6000SW_NUM_PORTS ||
	    p->es_port < 0) {
		err = EINVAL;
		goto out;
	}

	if (p->es_pvid != 0)
		e6000sw_set_pvid(sc, p->es_port, p->es_pvid);
	if (!e6000sw_cpuport(sc, p->es_port)) {
		mii = e6000sw_miiforphy(sc, p->es_port);
		err = ifmedia_ioctl(mii->mii_ifp, &p->es_ifr, &mii->mii_media,
		    SIOCSIFMEDIA);
	}

out:
	E6000SW_UNLOCK(sc);
	return (err);
}

/*
 * Registers in this switch are divided into sections, specified in
 * documentation. So as to access any of them, section index and reg index
 * is necessary. etherswitchcfg uses only one variable, so indexes were
 * compressed into addr_reg: 32 * section_index + reg_index.
 */
static int
e6000sw_readreg_wrapper(device_t dev, int addr_reg)
{

	if ((addr_reg > (REG_GLOBAL2 * 32 + REG_NUM_MAX)) ||
	    (addr_reg < (REG_PORT(0) * 32))) {
		device_printf(dev, "Wrong register address.\n");
		return (EINVAL);
	}

	return (e6000sw_readreg(device_get_softc(dev), addr_reg / 32,
	    addr_reg % 32));
}

static int
e6000sw_writereg_wrapper(device_t dev, int addr_reg, int val)
{

	if ((addr_reg > (REG_GLOBAL2 * 32 + REG_NUM_MAX)) ||
	    (addr_reg < (REG_PORT(0) * 32))) {
		device_printf(dev, "Wrong register address.\n");
		return (EINVAL);
	}
	e6000sw_writereg(device_get_softc(dev), addr_reg / 5,
	    addr_reg % 32, val);

	return (0);
}

/*
 * These wrappers are necessary because PHY accesses from etherswitchcfg
 * need to be synchronized with locks, while miibus PHY accesses do not.
 */
static int
e6000sw_readphy_wrapper(device_t dev, int phy, int reg)
{
	e6000sw_softc_t *sc;
	int ret;

	sc = device_get_softc(dev);
	E6000SW_LOCK_ASSERT(sc, SA_UNLOCKED);

	E6000SW_LOCK(sc);
	ret = e6000sw_readphy(dev, phy, reg);
	E6000SW_UNLOCK(sc);

	return (ret);
}

static int
e6000sw_writephy_wrapper(device_t dev, int phy, int reg, int data)
{
	e6000sw_softc_t *sc;
	int ret;

	sc = device_get_softc(dev);
	E6000SW_LOCK_ASSERT(sc, SA_UNLOCKED);

	E6000SW_LOCK(sc);
	ret = e6000sw_writephy(dev, phy, reg, data);
	E6000SW_UNLOCK(sc);

	return (ret);
}

/*
 * setvgroup/getvgroup called from etherswitchfcg need to be locked,
 * while internal calls do not.
 */
static int
e6000sw_setvgroup_wrapper(device_t dev, etherswitch_vlangroup_t *vg)
{
	e6000sw_softc_t *sc;
	int ret;

	sc = device_get_softc(dev);
	E6000SW_LOCK_ASSERT(sc, SA_UNLOCKED);

	E6000SW_LOCK(sc);
	ret = e6000sw_setvgroup(dev, vg);
	E6000SW_UNLOCK(sc);

	return (ret);
}

static int
e6000sw_getvgroup_wrapper(device_t dev, etherswitch_vlangroup_t *vg)
{
	e6000sw_softc_t *sc;
	int ret;

	sc = device_get_softc(dev);
	E6000SW_LOCK_ASSERT(sc, SA_UNLOCKED);

	E6000SW_LOCK(sc);
	ret = e6000sw_getvgroup(dev, vg);
	E6000SW_UNLOCK(sc);

	return (ret);
}

static __inline void
e6000sw_flush_port(e6000sw_softc_t *sc, int port)
{
	uint32_t reg;

	reg = e6000sw_readreg(sc, REG_PORT(port),
	    PORT_VLAN_MAP);
	reg &= ~PORT_VLAN_MAP_TABLE_MASK;
	reg &= ~PORT_VLAN_MAP_FID_MASK;
	e6000sw_writereg(sc, REG_PORT(port),
	    PORT_VLAN_MAP, reg);
	if (sc->vgroup[port] != E6000SW_PORT_NO_VGROUP) {
		/*
		 * If port belonged somewhere, owner-group
		 * should have its entry removed.
		 */
		sc->members[sc->vgroup[port]] &= ~(1 << port);
		sc->vgroup[port] = E6000SW_PORT_NO_VGROUP;
	}
}

static __inline void
e6000sw_port_assign_vgroup(e6000sw_softc_t *sc, int port, int fid, int vgroup,
    int members)
{
	uint32_t reg;

	reg = e6000sw_readreg(sc, REG_PORT(port),
	    PORT_VLAN_MAP);
	reg &= ~PORT_VLAN_MAP_TABLE_MASK;
	reg &= ~PORT_VLAN_MAP_FID_MASK;
	reg |= members & ~(1 << port);
	reg |= (fid << PORT_VLAN_MAP_FID) & PORT_VLAN_MAP_FID_MASK;
	e6000sw_writereg(sc, REG_PORT(port), PORT_VLAN_MAP,
	    reg);
	sc->vgroup[port] = vgroup;
}

static int
e6000sw_setvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{
	e6000sw_softc_t *sc;
	int port, fid;

	sc = device_get_softc(dev);
	E6000SW_LOCK_ASSERT(sc, SA_XLOCKED);

	if (vg->es_vlangroup >= E6000SW_NUM_VGROUPS)
		return (EINVAL);
	if (vg->es_member_ports != vg->es_untagged_ports) {
		device_printf(dev, "Tagged ports not supported.\n");
		return (EINVAL);
	}

	vg->es_untagged_ports &= PORT_VLAN_MAP_TABLE_MASK;
	fid = vg->es_vlangroup + 1;
	for (port = 0; port < E6000SW_NUM_PORTS; port++) {
		if ((sc->members[vg->es_vlangroup] & (1 << port)) ||
		    (vg->es_untagged_ports & (1 << port)))
			e6000sw_flush_port(sc, port);
		if (vg->es_untagged_ports & (1 << port))
			e6000sw_port_assign_vgroup(sc, port, fid,
			    vg->es_vlangroup, vg->es_untagged_ports);
	}
	sc->vid[vg->es_vlangroup] = vg->es_vid;
	sc->members[vg->es_vlangroup] = vg->es_untagged_ports;

	return (0);
}

static int
e6000sw_getvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{
	e6000sw_softc_t *sc;

	sc = device_get_softc(dev);
	E6000SW_LOCK_ASSERT(sc, SA_XLOCKED);

	if (vg->es_vlangroup >= E6000SW_NUM_VGROUPS)
		return (EINVAL);
	vg->es_untagged_ports = vg->es_member_ports =
	    sc->members[vg->es_vlangroup];
	vg->es_vid = ETHERSWITCH_VID_VALID;

	return (0);
}

static __inline struct mii_data*
e6000sw_miiforphy(e6000sw_softc_t *sc, unsigned int phy)
{

	if (phy >= E6000SW_NUM_PHYS)
		return (NULL);

	return (device_get_softc(sc->miibus[phy]));
}

static int
e6000sw_ifmedia_upd(struct ifnet *ifp)
{
	e6000sw_softc_t *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
	mii = e6000sw_miiforphy(sc, ifp->if_dunit);
	if (mii == NULL)
		return (ENXIO);
	mii_mediachg(mii);

	return (0);
}

static void
e6000sw_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	e6000sw_softc_t *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
	mii = e6000sw_miiforphy(sc, ifp->if_dunit);

	if (mii == NULL)
		return;

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static __inline uint32_t
e6000sw_readreg(e6000sw_softc_t *sc, int addr, int reg)
{

	E6000SW_LOCK_ASSERT(sc, SA_XLOCKED);

	return (MDIO_READREG(device_get_parent(sc->dev), addr, reg));
}

static __inline void
e6000sw_writereg(e6000sw_softc_t *sc, int addr, int reg, int val)
{

	E6000SW_LOCK_ASSERT(sc, SA_XLOCKED);

	MDIO_WRITEREG(device_get_parent(sc->dev), addr, reg, val);
}

static __inline int
e6000sw_cpuport(e6000sw_softc_t *sc, int port)
{

	return (sc->cpuports_mask & (1 << port));
}

static __inline int
e6000sw_set_pvid(e6000sw_softc_t *sc, int port, int pvid)
{

	e6000sw_writereg(sc, REG_PORT(port), PORT_VID, pvid &
	    PORT_VID_DEF_VID_MASK);

	return (0);
}

static __inline int
e6000sw_get_pvid(e6000sw_softc_t *sc, int port, int *pvid)
{

	if (pvid == NULL)
		return (ENXIO);

	*pvid = e6000sw_readreg(sc, REG_PORT(port), PORT_VID) &
	    PORT_VID_DEF_VID_MASK;

	return (0);
}

static void
e6000sw_tick (void *arg)
{
	e6000sw_softc_t *sc;
	struct mii_softc *miisc;
	int i;

	sc = arg;

	E6000SW_LOCK_ASSERT(sc, SA_UNLOCKED);
	for (;;) {
		E6000SW_LOCK(sc);
		for (i = 0; i < E6000SW_NUM_PHYS; i++) {
			mii_tick(sc->mii[i]);
			LIST_FOREACH(miisc, &sc->mii[i]->mii_phys, mii_list) {
				if (IFM_INST(sc->mii[i]->mii_media.ifm_cur->ifm_media)
				    != miisc->mii_inst)
					continue;
				mii_phy_update(miisc, MII_POLLSTAT);
			}
		}
		E6000SW_UNLOCK(sc);
		pause("e6000sw tick", 1000);
	}
}

static void
e6000sw_setup(device_t dev, e6000sw_softc_t *sc)
{
	uint16_t atu_ctrl, atu_age;

	/* Set aging time */
	e6000sw_writereg(sc, REG_GLOBAL, ATU_CONTROL,
	    (E6000SW_DEFAULT_AGETIME << ATU_CONTROL_AGETIME) |
	    (1 << ATU_CONTROL_LEARN2ALL));

	/* Send all with specific mac address to cpu port */
	e6000sw_writereg(sc, REG_GLOBAL2, MGMT_EN_2x, MGMT_EN_ALL);
	e6000sw_writereg(sc, REG_GLOBAL2, MGMT_EN_0x, MGMT_EN_ALL);

	/* Disable Remote Managment */
	e6000sw_writereg(sc, REG_GLOBAL, SWITCH_GLOBAL_CONTROL2, 0);

	/* Disable loopback filter and flow control messages */
	e6000sw_writereg(sc, REG_GLOBAL2, SWITCH_MGMT,
	    SWITCH_MGMT_PRI_MASK |
	    (1 << SWITCH_MGMT_RSVD2CPU) |
	    SWITCH_MGMT_FC_PRI_MASK |
	    (1 << SWITCH_MGMT_FORCEFLOW));

	/* Set VLAN configuration */
	e6000sw_port_vlan_conf(sc);

	e6000sw_atu_flush(dev, sc, NO_OPERATION);
	e6000sw_atu_mac_table(dev, sc, NULL, NO_OPERATION);
	e6000sw_set_atustat(dev, sc, 0, COUNT_ALL);

	/* Set ATU AgeTime to 15 seconds */
	atu_age = 1;

	atu_ctrl = e6000sw_readreg(sc, REG_GLOBAL, ATU_CONTROL);

	/* Set new AgeTime field */
	atu_ctrl &= ~ATU_CONTROL_AGETIME_MASK;
	e6000sw_writereg(sc, REG_GLOBAL, ATU_CONTROL, atu_ctrl |
	    (atu_age << ATU_CONTROL_AGETIME));
}

static void
e6000sw_port_vlan_conf(e6000sw_softc_t *sc)
{
	int port, ret;
	etherswitch_vlangroup_t vg;
	device_t dev;

	dev = sc->dev;
	/* Disable all ports */
	for (port = 0; port < E6000SW_NUM_PORTS; port++) {
		ret = e6000sw_readreg(sc, REG_PORT(port), PORT_CONTROL);
		e6000sw_writereg(sc, REG_PORT(port), PORT_CONTROL,
		    (ret & ~PORT_CONTROL_ENABLE));
	}

	/* Set port priority */
	for (port = 0; port < E6000SW_NUM_PORTS; port++) {
		ret = e6000sw_readreg(sc, REG_PORT(port), PORT_VID);
		ret &= ~PORT_VID_PRIORITY_MASK;
		e6000sw_writereg(sc, REG_PORT(port), PORT_VID, ret);
	}

	vg.es_vlangroup = 0;
	vg.es_vid = 0;
	vg.es_member_ports = vg.es_untagged_ports = E6000SW_DEF_VLANGROUP0;
	e6000sw_setvgroup(dev, &vg);
	vg.es_vlangroup = 1;
	vg.es_vid = 1;
	vg.es_member_ports = vg.es_untagged_ports = E6000SW_DEF_VLANGROUP1;
	e6000sw_setvgroup(dev, &vg);

	device_printf(dev, "Default vlangroups set.\n");
	/* Set VID map */
	for (port = 0; port < E6000SW_NUM_PORTS; port++) {
		ret = e6000sw_readreg(sc, REG_PORT(port), PORT_VID);
		ret &= ~PORT_VID_DEF_VID_MASK;
		ret |= (port + 1);
		e6000sw_writereg(sc, REG_PORT(port), PORT_VID, ret);
	}

	/* Enable all ports */
	for (port = 0; port < E6000SW_NUM_PORTS; port++) {
		ret = e6000sw_readreg(sc, REG_PORT(port), PORT_CONTROL);
		e6000sw_writereg(sc, REG_PORT(port), PORT_CONTROL, (ret |
		    PORT_CONTROL_ENABLE));
	}
}

static void
e6000sw_set_atustat(device_t dev, e6000sw_softc_t *sc, int bin, int flag)
{
	uint16_t ret;

	ret = e6000sw_readreg(sc, REG_GLOBAL2, ATU_STATS);
	e6000sw_writereg(sc, REG_GLOBAL2, ATU_STATS, (bin << ATU_STATS_BIN ) |
	    (flag << ATU_STATS_FLAG));
}

static int
e6000sw_atu_mac_table(device_t dev, e6000sw_softc_t *sc, struct atu_opt *atu,
    int flag)
{
	uint16_t ret_opt;
	uint16_t ret_data;
	int retries;

	if (flag == NO_OPERATION)
		return (0);
	else if ((flag & (LOAD_FROM_FIB | PURGE_FROM_FIB | GET_NEXT_IN_FIB |
	    GET_VIOLATION_DATA | CLEAR_VIOLATION_DATA)) == 0) {
		device_printf(dev, "Wrong Opcode for ATU operation\n");
		return (EINVAL);
	}

	ret_opt = e6000sw_readreg(sc, REG_GLOBAL, ATU_OPERATION);

	if (ret_opt & ATU_UNIT_BUSY) {
		device_printf(dev, "ATU unit is busy, cannot access"
		    "register\n");
		return (EBUSY);
	} else {
		if(flag & LOAD_FROM_FIB) {
			ret_data = e6000sw_readreg(sc, REG_GLOBAL, ATU_DATA);
			e6000sw_writereg(sc, REG_GLOBAL2, ATU_DATA, (ret_data &
			    ~ENTRY_STATE));
		}
		e6000sw_writereg(sc, REG_GLOBAL, ATU_MAC_ADDR01, atu->mac_01);
		e6000sw_writereg(sc, REG_GLOBAL, ATU_MAC_ADDR23, atu->mac_23);
		e6000sw_writereg(sc, REG_GLOBAL, ATU_MAC_ADDR45, atu->mac_45);
		e6000sw_writereg(sc, REG_GLOBAL, ATU_FID, atu->fid);

		e6000sw_writereg(sc, REG_GLOBAL, ATU_OPERATION, (ret_opt |
		    ATU_UNIT_BUSY | flag));

		retries = E6000SW_RETRIES;
		while (--retries & (e6000sw_readreg(sc, REG_GLOBAL,
		    ATU_OPERATION) & ATU_UNIT_BUSY))
			DELAY(1);

		if (retries == 0)
			device_printf(dev, "Timeout while flushing\n");
		else if (flag & GET_NEXT_IN_FIB) {
			atu->mac_01 = e6000sw_readreg(sc, REG_GLOBAL,
			    ATU_MAC_ADDR01);
			atu->mac_23 = e6000sw_readreg(sc, REG_GLOBAL,
			    ATU_MAC_ADDR23);
			atu->mac_45 = e6000sw_readreg(sc, REG_GLOBAL,
			    ATU_MAC_ADDR45);
		}
	}

	return (0);
}

static int
e6000sw_atu_flush(device_t dev, e6000sw_softc_t *sc, int flag)
{
	uint16_t ret;
	int retries;

	if (flag == NO_OPERATION)
		return (0);

	ret = e6000sw_readreg(sc, REG_GLOBAL, ATU_OPERATION);
	if (ret & ATU_UNIT_BUSY) {
		device_printf(dev, "Atu unit is busy, cannot flush\n");
		return (EBUSY);
	} else {
		e6000sw_writereg(sc, REG_GLOBAL, ATU_OPERATION, (ret |
		    ATU_UNIT_BUSY | flag));
		retries = E6000SW_RETRIES;
		while (--retries & (e6000sw_readreg(sc, REG_GLOBAL,
		    ATU_OPERATION) & ATU_UNIT_BUSY))
			DELAY(1);

		if (retries == 0)
			device_printf(dev, "Timeout while flushing\n");
	}

	return (0);
}
