/*-
 * SPDX-License-Identifier: BSD-2-Clause
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_var.h>
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
#include <dev/mdio/mdio.h>
#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/ar40xx/ar40xx_var.h>
#include <dev/etherswitch/ar40xx/ar40xx_reg.h>
#include <dev/etherswitch/ar40xx/ar40xx_phy.h>
#include <dev/etherswitch/ar40xx/ar40xx_debug.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_psgmii.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_port.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_mib.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_vtu.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_atu.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

static struct ofw_compat_data compat_data[] = {
	{ "qcom,ess-switch",		1 },
	{ NULL,				0 },
};

static int
ar40xx_probe(device_t dev)
{

	if (! ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "IPQ4018 ESS Switch fabric / PSGMII PHY");
	return (BUS_PROBE_DEFAULT);
}

static void
ar40xx_tick(void *arg)
{
	struct ar40xx_softc *sc = arg;

	(void) ar40xx_phy_tick(sc);
	callout_reset(&sc->sc_phy_callout, hz, ar40xx_tick, sc);
}

static void
ar40xx_statchg(device_t dev)
{
	struct ar40xx_softc *sc = device_get_softc(dev);

	AR40XX_DPRINTF(sc, AR40XX_DBG_PORT_STATUS, "%s\n", __func__);
}

static int
ar40xx_readphy(device_t dev, int phy, int reg)
{
	struct ar40xx_softc *sc = device_get_softc(dev);

	return MDIO_READREG(sc->sc_mdio_dev, phy, reg);
}

static int
ar40xx_writephy(device_t dev, int phy, int reg, int val)
{
	struct ar40xx_softc *sc = device_get_softc(dev);

	return MDIO_WRITEREG(sc->sc_mdio_dev, phy, reg, val);
}

/*
 * Do the initial switch configuration.
 */
static int
ar40xx_reset_switch(struct ar40xx_softc *sc)
{
	int ret, i;

	AR40XX_DPRINTF(sc, AR40XX_DBG_HW_INIT, "%s: called\n", __func__);

	/* blank the VLAN config */
	memset(&sc->sc_vlan, 0, sizeof(sc->sc_vlan));

	/* initial vlan port mapping */
	for (i = 0; i < AR40XX_NUM_VTU_ENTRIES; i++)
		sc->sc_vlan.vlan_id[i] = 0;

	/* init vlan config */
	ret = ar40xx_hw_vlan_init(sc);

	/* init monitor config */
	sc->sc_monitor.mirror_tx = false;
	sc->sc_monitor.mirror_rx = false;
	sc->sc_monitor.source_port = 0;
	sc->sc_monitor.monitor_port = 0;

	/* apply switch config */
	ret = ar40xx_hw_sw_hw_apply(sc);

	return (ret);
}

static int
ar40xx_sysctl_dump_port_state(SYSCTL_HANDLER_ARGS)
{
	struct ar40xx_softc *sc = arg1;
	int val = 0;
	int error;
	int i;

	(void) i; (void) sc;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	if (val < 0 || val > 5) {
		return (EINVAL);
	}

	AR40XX_LOCK(sc);

	device_printf(sc->sc_dev, "port %d: PORT_STATUS=0x%08x\n", val,
	    AR40XX_REG_READ(sc, AR40XX_REG_PORT_STATUS(val)));
	device_printf(sc->sc_dev, "port %d: PORT_HEADER=0x%08x\n", val,
	    AR40XX_REG_READ(sc, AR40XX_REG_PORT_HEADER(val)));
	device_printf(sc->sc_dev, "port %d: PORT_VLAN0=0x%08x\n", val,
	    AR40XX_REG_READ(sc, AR40XX_REG_PORT_VLAN0(val)));
	device_printf(sc->sc_dev, "port %d: PORT_VLAN1=0x%08x\n", val,
	    AR40XX_REG_READ(sc, AR40XX_REG_PORT_VLAN1(val)));
	device_printf(sc->sc_dev, "port %d: PORT_LOOKUP=0x%08x\n", val,
	    AR40XX_REG_READ(sc, AR40XX_REG_PORT_LOOKUP(val)));
	device_printf(sc->sc_dev, "port %d: PORT_HOL_CTRL1=0x%08x\n", val,
	    AR40XX_REG_READ(sc, AR40XX_REG_PORT_HOL_CTRL1(val)));
	device_printf(sc->sc_dev, "port %d: PORT_FLOWCTRL_THRESH=0x%08x\n",
	    val, AR40XX_REG_READ(sc, AR40XX_REG_PORT_FLOWCTRL_THRESH(val)));

	AR40XX_UNLOCK(sc);

	return (0);
}

static int
ar40xx_sysctl_dump_port_mibstats(SYSCTL_HANDLER_ARGS)
{
	struct ar40xx_softc *sc = arg1;
	int val = 0;
	int error;
	int i;

	(void) i; (void) sc;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	if (val < 0 || val > 5) {
		return (EINVAL);
	}

	AR40XX_LOCK(sc);

	/* Yes, this snapshots all ports */
	(void) ar40xx_hw_mib_capture(sc);
	(void) ar40xx_hw_mib_fetch(sc, val);

	AR40XX_UNLOCK(sc);

	return (0);
}


static int
ar40xx_sysctl_attach(struct ar40xx_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->sc_debug, 0,
	    "debugging flags");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "port_state", CTLTYPE_INT | CTLFLAG_RW, sc,
	    0, ar40xx_sysctl_dump_port_state, "I", "");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "port_mibstats", CTLTYPE_INT | CTLFLAG_RW, sc,
	    0, ar40xx_sysctl_dump_port_mibstats, "I", "");

	return (0);
}

static int
ar40xx_detach(device_t dev)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	int i;

	device_printf(sc->sc_dev, "%s: called\n", __func__);

	callout_drain(&sc->sc_phy_callout);

	/* Free PHYs */
	for (i = 0; i < AR40XX_NUM_PHYS; i++) {
		if (sc->sc_phys.miibus[i] != NULL)
			device_delete_child(dev, sc->sc_phys.miibus[i]);
		if (sc->sc_phys.ifp[i] != NULL)
			if_free(sc->sc_phys.ifp[i]);
		free(sc->sc_phys.ifname[i], M_DEVBUF);
	}

	bus_generic_detach(dev);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static int
ar40xx_attach(device_t dev)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	phandle_t psgmii_p, root_p, mdio_p;
	int ret, i;

	sc->sc_dev = dev;
	mtx_init(&sc->sc_mtx, "ar40xx_switch", NULL, MTX_DEF);

	psgmii_p = OF_finddevice("/soc/ess-psgmii");
	if (psgmii_p == -1) {
		device_printf(dev,
		    "%s: couldn't find /soc/ess-psgmii DT node\n",
		    __func__);
		goto error;
	}

	/*
	 * Get the ipq4019-mdio node here, to talk to our local PHYs
	 * if needed
	 */
	root_p = OF_finddevice("/soc");
	mdio_p = ofw_bus_find_compatible(root_p, "qcom,ipq4019-mdio");
	if (mdio_p == -1) {
		device_printf(dev, "%s: couldn't find ipq4019-mdio DT node\n",
		    __func__);
		goto error;
	}
	sc->sc_mdio_phandle = mdio_p;
	sc->sc_mdio_dev = OF_device_from_xref(OF_xref_from_node(mdio_p));
	if (sc->sc_mdio_dev == NULL) {
		device_printf(dev,
		    "%s: couldn't get mdio device (mdio_p=%u)\n",
		    __func__, mdio_p);
		goto error;
	}

	/* get psgmii base address from psgmii node */
	ret = OF_decode_addr(psgmii_p, 0, &sc->sc_psgmii_mem_tag,
	    &sc->sc_psgmii_mem_handle,
	    &sc->sc_psgmii_mem_size);
	if (ret != 0) {
		device_printf(dev, "%s: couldn't map psgmii mem (%d)\n",
		    __func__, ret);
		goto error;
	}

	/* get switch base address */
	sc->sc_ess_mem_rid = 0;
	sc->sc_ess_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_ess_mem_rid, RF_ACTIVE);
	if (sc->sc_ess_mem_res == NULL) {
		device_printf(dev, "%s: failed to find memory resource\n",
		    __func__);
		goto error;
	}
	sc->sc_ess_mem_size = (size_t) bus_get_resource_count(dev,
	    SYS_RES_MEMORY, sc->sc_ess_mem_rid);
	if (sc->sc_ess_mem_size == 0) {
		device_printf(dev, "%s: failed to get device memory size\n",
		    __func__);
		goto error;
	}

	ret = OF_getencprop(ofw_bus_get_node(dev), "switch_mac_mode",
	    &sc->sc_config.switch_mac_mode,
	    sizeof(sc->sc_config.switch_mac_mode));
	if (ret < 0) {
		device_printf(dev, "%s: missing switch_mac_mode property\n",
		    __func__);
		goto error;
	}

	ret = OF_getencprop(ofw_bus_get_node(dev), "switch_cpu_bmp",
	    &sc->sc_config.switch_cpu_bmp,
	    sizeof(sc->sc_config.switch_cpu_bmp));
	if (ret < 0) {
		device_printf(dev, "%s: missing switch_cpu_bmp property\n",
		    __func__);
		goto error;
	}

	ret = OF_getencprop(ofw_bus_get_node(dev), "switch_lan_bmp",
	    &sc->sc_config.switch_lan_bmp,
	    sizeof(sc->sc_config.switch_lan_bmp));
	if (ret < 0) {
		device_printf(dev, "%s: missing switch_lan_bmp property\n",
		    __func__);
		goto error;
	}

	ret = OF_getencprop(ofw_bus_get_node(dev), "switch_wan_bmp",
	    &sc->sc_config.switch_wan_bmp,
	    sizeof(sc->sc_config.switch_wan_bmp));
	if (ret < 0) {
		device_printf(dev, "%s: missing switch_wan_bmp property\n",
		    __func__);
		goto error;
	}

	ret = clk_get_by_ofw_name(dev, 0, "ess_clk", &sc->sc_ess_clk);
	if (ret != 0) {
		device_printf(dev, "%s: failed to find ess_clk (%d)\n",
		    __func__, ret);
		goto error;
	}
	ret = clk_enable(sc->sc_ess_clk);
	if (ret != 0) {
		device_printf(dev, "%s: failed to enable clock (%d)\n",
		    __func__, ret);
		goto error;
	}

	ret = hwreset_get_by_ofw_name(dev, 0, "ess_rst", &sc->sc_ess_rst);
	if (ret != 0) {
		device_printf(dev, "%s: failed to find ess_rst (%d)\n",
		    __func__, ret);
		goto error;
	}

	/*
	 * Ok, at this point we have enough resources to do an initial
	 * reset and configuration.
	 */

	AR40XX_LOCK(sc);

	/* Initial PSGMII/RGMII port configuration */
	ret = ar40xx_hw_psgmii_init_config(sc);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to init PSGMII (%d)\n", ret);
		goto error_locked;
	}

	/*
	 * ESS reset - this resets both the ethernet switch
	 * AND the ethernet block.
	 */
	ret = ar40xx_hw_ess_reset(sc);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to reset ESS block (%d)\n", ret);
		goto error_locked;
	}

	/*
	 * Check the PHY IDs for each of the PHYs from 0..4;
	 * this is useful to make sure that we can SEE the external
	 * PHY(s).
	 */
	if (bootverbose) {
		ret = ar40xx_hw_phy_get_ids(sc);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: failed to check PHY IDs (%d)\n", ret);
			goto error_locked;
		}
	}

	/*
	 * Do PSGMII PHY self-test; work-around issues.
	 */
	ret = ar40xx_hw_psgmii_self_test(sc);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to do PSGMII self-test (%d)\n", ret);
		goto error_locked;
	}

	/* Return port config to runtime state */
	ret = ar40xx_hw_psgmii_self_test_clean(sc);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to do PSGMII runtime config (%d)\n", ret);
		goto error_locked;
	}

	/* mac_mode_init */
	ret = ar40xx_hw_psgmii_set_mac_mode(sc,
	    sc->sc_config.switch_mac_mode);

	/* Initialise each hardware port */
	for (i = 0; i < AR40XX_NUM_PORTS; i++) {
		ret = ar40xx_hw_port_init(sc, i);
	}

	/* initialise the global switch configuration */
	ret = ar40xx_hw_init_globals(sc);

	/* reset the switch vlan/port learning config */
	ret = ar40xx_reset_switch(sc);

	/* cpuport setup */
	ret = ar40xx_hw_port_cpuport_setup(sc);

	AR40XX_UNLOCK(sc);

#if 0
	/* We may end up needing the QM workaround code here.. */
	device_printf(dev, "%s: TODO: QM error check\n", __func__);
#endif

	/* Attach PHYs */
	ret = ar40xx_attach_phys(sc);

	ret = bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);
	ret = bus_generic_attach(dev);

	/* Start timer */
	callout_init_mtx(&sc->sc_phy_callout, &sc->sc_mtx, 0);

	/*
	 * Setup the etherswitch info block.
	 */
	strlcpy(sc->sc_info.es_name, device_get_desc(dev),
	    sizeof(sc->sc_info.es_name));
	sc->sc_info.es_nports = AR40XX_NUM_PORTS;
	sc->sc_info.es_vlan_caps = ETHERSWITCH_VLAN_DOT1Q;
	/* XXX TODO: double-tag / 802.1ad */
	sc->sc_info.es_nvlangroups = AR40XX_NUM_VTU_ENTRIES;

	/*
	 * Fetch the initial port configuration.
	 */
	AR40XX_LOCK(sc);
	ar40xx_tick(sc);
	AR40XX_UNLOCK(sc);

	ar40xx_sysctl_attach(sc);

	return (0);
error_locked:
	AR40XX_UNLOCK(sc);
error:
	ar40xx_detach(dev);
	return (ENXIO);
}

static void
ar40xx_lock(device_t dev)
{
	struct ar40xx_softc *sc = device_get_softc(dev);

	AR40XX_LOCK(sc);
}

static void
ar40xx_unlock(device_t dev)
{
	struct ar40xx_softc *sc = device_get_softc(dev);

	AR40XX_LOCK_ASSERT(sc);
	AR40XX_UNLOCK(sc);
}

static etherswitch_info_t *
ar40xx_getinfo(device_t dev)
{
	struct ar40xx_softc *sc = device_get_softc(dev);

	return (&sc->sc_info);
}

static int
ar40xx_readreg(device_t dev, int addr)
{
	struct ar40xx_softc *sc = device_get_softc(dev);

	if (addr >= sc->sc_ess_mem_size - 1)
		return (-1);

	AR40XX_REG_BARRIER_READ(sc);

	return AR40XX_REG_READ(sc, addr);
}

static int
ar40xx_writereg(device_t dev, int addr, int value)
{
	struct ar40xx_softc *sc = device_get_softc(dev);

	if (addr >= sc->sc_ess_mem_size - 1)
		return (-1);

	AR40XX_REG_WRITE(sc, addr, value);
	AR40XX_REG_BARRIER_WRITE(sc);
	return (0);
}

/*
 * Get the port configuration and status.
 */
static int
ar40xx_getport(device_t dev, etherswitch_port_t *p)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	struct mii_data *mii = NULL;
	struct ifmediareq *ifmr;
	int err;

	if (p->es_port < 0 || p->es_port > sc->sc_info.es_nports)
		return (ENXIO);

	AR40XX_LOCK(sc);
	/* Fetch the current VLAN configuration for this port */
	/* PVID */
	ar40xx_hw_get_port_pvid(sc, p->es_port, &p->es_pvid);

	/*
	 * The VLAN egress aren't appropriate to the ports;
	 * instead it's part of the VLAN group config.
	 */

	/* Get MII config */
	mii = ar40xx_phy_miiforport(sc, p->es_port);

	AR40XX_UNLOCK(sc);

	if (p->es_port == 0) {
		/* CPU port */
		p->es_flags |= ETHERSWITCH_PORT_CPU;
		ifmr = &p->es_ifmr;
		ifmr->ifm_count = 0;
		ifmr->ifm_current = ifmr->ifm_active =
		     IFM_ETHER | IFM_1000_T | IFM_FDX;
		ifmr->ifm_mask = 0;
		ifmr->ifm_status = IFM_ACTIVE | IFM_AVALID;
	} else if (mii != NULL) {
		/* non-CPU port */
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
 * Set the port configuration and status.
 */
static int
ar40xx_setport(device_t dev, etherswitch_port_t *p)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	struct ifmedia *ifm;
	struct mii_data *mii;
	if_t ifp;
	int ret;

	if (p->es_port < 0 || p->es_port > sc->sc_info.es_nports)
		return (EINVAL);

	/* Port flags */
	AR40XX_LOCK(sc);
	ret = ar40xx_hw_set_port_pvid(sc, p->es_port, p->es_pvid);
	if (ret != 0) {
		AR40XX_UNLOCK(sc);
		return (ret);
	}
	/* XXX TODO: tag strip/unstrip, double-tag, etc */
	AR40XX_UNLOCK(sc);

	/* Don't change media config on CPU port */
	if (p->es_port == 0)
		return (0);

	mii = ar40xx_phy_miiforport(sc, p->es_port);
	if (mii == NULL)
		return (ENXIO);

	ifp = ar40xx_phy_ifpforport(sc, p->es_port);

	ifm = &mii->mii_media;
	return (ifmedia_ioctl(ifp, &p->es_ifr, ifm, SIOCSIFMEDIA));

	return (0);
}

/*
 * Get the current VLAN group (per-port, ISL, dot1q) configuration.
 *
 * For now the only supported operating mode is dot1q.
 */
static int
ar40xx_getvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	int vid, ret;

	if (vg->es_vlangroup > sc->sc_info.es_nvlangroups)
		return (EINVAL);

	vg->es_untagged_ports = 0;
	vg->es_member_ports = 0;
	vg->es_fid = 0;

	AR40XX_LOCK(sc);

	/* Note: only supporting 802.1q VLAN config for now */
	if (sc->sc_vlan.vlan != 1) {
		vg->es_member_ports = 0;
		vg->es_untagged_ports = 0;
		AR40XX_UNLOCK(sc);
		return (-1);
	}

	/* Get vlangroup mapping to VLAN id */
	vid = sc->sc_vlan.vlan_id[vg->es_vlangroup];
	if ((vid & ETHERSWITCH_VID_VALID) == 0) {
		/* Not an active vgroup; bail */
		AR40XX_UNLOCK(sc);
		return (0);
	}
	vg->es_vid = vid;

	ret = ar40xx_hw_vtu_get_vlan(sc, vid, &vg->es_member_ports,
	    &vg->es_untagged_ports);

	AR40XX_UNLOCK(sc);

	if (ret == 0) {
		vg->es_vid |= ETHERSWITCH_VID_VALID;
	}

	return (ret);
}

/*
 * Set the current VLAN group (per-port, ISL, dot1q) configuration.
 *
 * For now the only supported operating mode is dot1q.
 */
static int
ar40xx_setvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	int err, vid;

	/* For now we only support 802.1q mode */
	if (sc->sc_vlan.vlan == 0)
		return (EINVAL);

	AR40XX_LOCK(sc);
	vid = sc->sc_vlan.vlan_id[vg->es_vlangroup];
	/*
	 * If we have an 802.1q VID and it's different to the current one,
	 * purge the current VTU entry.
	 */
	if ((vid != 0) &&
	    ((vid & ETHERSWITCH_VID_VALID) != 0) &&
	    ((vid & ETHERSWITCH_VID_MASK) !=
	     (vg->es_vid & ETHERSWITCH_VID_MASK))) {
		AR40XX_DPRINTF(sc, AR40XX_DBG_VTU_OP,
		    "%s: purging VID %d first\n", __func__, vid);
		err = ar40xx_hw_vtu_flush(sc);
		if (err != 0) {
			AR40XX_UNLOCK(sc);
			return (err);
		}
	}

	/* Update VLAN ID */
	vid = vg->es_vid & ETHERSWITCH_VID_MASK;
	sc->sc_vlan.vlan_id[vg->es_vlangroup] = vid;
	if (vid == 0) {
		/* Setting it to 0 disables the group */
		AR40XX_UNLOCK(sc);
		return (0);
	}
	/* Add valid bit for this entry */
	sc->sc_vlan.vlan_id[vg->es_vlangroup] = vid | ETHERSWITCH_VID_VALID;

	/* Update hardware */
	err = ar40xx_hw_vtu_load_vlan(sc, vid, vg->es_member_ports,
	    vg->es_untagged_ports);
	if (err != 0) {
		AR40XX_UNLOCK(sc);
		return (err);
	}

	/* Update the config for the given entry */
	sc->sc_vlan.vlan_ports[vg->es_vlangroup] = vg->es_member_ports;
	sc->sc_vlan.vlan_untagged[vg->es_vlangroup] = vg->es_untagged_ports;

	AR40XX_UNLOCK(sc);

	return (0);
}

/*
 * Get the current configuration mode.
 */
static int
ar40xx_getconf(device_t dev, etherswitch_conf_t *conf)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	int ret;

	AR40XX_LOCK(sc);

	/* Only support dot1q VLAN for now */
	conf->cmd = ETHERSWITCH_CONF_VLAN_MODE;
	conf->vlan_mode = ETHERSWITCH_VLAN_DOT1Q;

	/* Switch MAC address */
	ret = ar40xx_hw_read_switch_mac_address(sc, &conf->switch_macaddr);
	if (ret == 0)
		conf->cmd |= ETHERSWITCH_CONF_SWITCH_MACADDR;

	AR40XX_UNLOCK(sc);

	return (0);
}

/*
 * Set the current configuration and do a switch reset.
 *
 * For now the only supported operating mode is dot1q, don't
 * allow it to be set to non-dot1q.
 */
static int
ar40xx_setconf(device_t dev, etherswitch_conf_t *conf)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	int ret = 0;

	if (conf->cmd & ETHERSWITCH_CONF_VLAN_MODE) {
		/* Only support dot1q VLAN for now */
		if (conf->vlan_mode != ETHERSWITCH_VLAN_DOT1Q)
			return (EINVAL);
	}

	if (conf->cmd & ETHERSWITCH_CONF_SWITCH_MACADDR) {
		AR40XX_LOCK(sc);
		ret = ar40xx_hw_read_switch_mac_address(sc,
		    &conf->switch_macaddr);
		AR40XX_UNLOCK(sc);
	}

	return (ret);
}

/*
 * Flush all ATU entries.
 */
static int
ar40xx_atu_flush_all(device_t dev)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	int ret;

	AR40XX_LOCK(sc);
	ret = ar40xx_hw_atu_flush_all(sc);
	AR40XX_UNLOCK(sc);
	return (ret);
}

/*
 * Flush all ATU entries for the given port.
 */
static int
ar40xx_atu_flush_port(device_t dev, int port)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	int ret;

	AR40XX_LOCK(sc);
	ret = ar40xx_hw_atu_flush_port(sc, port);
	AR40XX_UNLOCK(sc);
	return (ret);
}

/*
 * Load the ATU table into local storage so it can be iterated
 * over.
 */
static int
ar40xx_atu_fetch_table(device_t dev, etherswitch_atu_table_t *table)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	int err, nitems;

	memset(&sc->atu.entries, 0, sizeof(sc->atu.entries));

	table->es_nitems = 0;
	nitems = 0;

	AR40XX_LOCK(sc);
	sc->atu.count = 0;
	err = ar40xx_hw_atu_fetch_entry(sc, NULL, 0);
	if (err != 0)
		goto done;

	while (nitems < AR40XX_NUM_ATU_ENTRIES) {
		err = ar40xx_hw_atu_fetch_entry(sc,
		    &sc->atu.entries[nitems], 1);
		if (err != 0)
			goto done;
		sc->atu.entries[nitems].id = nitems;
		nitems++;
	}
done:
	sc->atu.count = nitems;
	table->es_nitems = nitems;
	AR40XX_UNLOCK(sc);

	return (0);
}

/*
 * Iterate over the ATU table entries that have been previously
 * fetched.
 */
static int
ar40xx_atu_fetch_table_entry(device_t dev, etherswitch_atu_entry_t *e)
{
	struct ar40xx_softc *sc = device_get_softc(dev);
	int id, err = 0;

	id = e->id;
	AR40XX_LOCK(sc);
	if (id > sc->atu.count) {
		err = ENOENT;
		goto done;
	}
	memcpy(e, &sc->atu.entries[id], sizeof(*e));
done:
	AR40XX_UNLOCK(sc);
	return (err);
}

static device_method_t ar40xx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			ar40xx_probe),
	DEVMETHOD(device_attach,		ar40xx_attach),
	DEVMETHOD(device_detach,		ar40xx_detach),

	/* bus interface */
	DEVMETHOD(bus_add_child,		device_add_child_ordered),

	/* MII interface */
	DEVMETHOD(miibus_readreg,		ar40xx_readphy),
	DEVMETHOD(miibus_writereg,		ar40xx_writephy),
	DEVMETHOD(miibus_statchg,		ar40xx_statchg),

	/* MDIO interface */
	DEVMETHOD(mdio_readreg,			ar40xx_readphy),
	DEVMETHOD(mdio_writereg,		ar40xx_writephy),

	/* etherswitch interface */
	DEVMETHOD(etherswitch_lock,		ar40xx_lock),
	DEVMETHOD(etherswitch_unlock,		ar40xx_unlock),
	DEVMETHOD(etherswitch_getinfo,		ar40xx_getinfo),
	DEVMETHOD(etherswitch_readreg,		ar40xx_readreg),
	DEVMETHOD(etherswitch_writereg,		ar40xx_writereg),
	DEVMETHOD(etherswitch_readphyreg,	ar40xx_readphy),
	DEVMETHOD(etherswitch_writephyreg,	ar40xx_writephy),
	DEVMETHOD(etherswitch_getport,		ar40xx_getport),
	DEVMETHOD(etherswitch_setport,		ar40xx_setport),
	DEVMETHOD(etherswitch_getvgroup,	ar40xx_getvgroup),
	DEVMETHOD(etherswitch_setvgroup,	ar40xx_setvgroup),
	DEVMETHOD(etherswitch_getconf,		ar40xx_getconf),
	DEVMETHOD(etherswitch_setconf,		ar40xx_setconf),
	DEVMETHOD(etherswitch_flush_all,	ar40xx_atu_flush_all),
	DEVMETHOD(etherswitch_flush_port,	ar40xx_atu_flush_port),
	DEVMETHOD(etherswitch_fetch_table,	ar40xx_atu_fetch_table),
	DEVMETHOD(etherswitch_fetch_table_entry,
					     ar40xx_atu_fetch_table_entry),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ar40xx, ar40xx_driver, ar40xx_methods,
    sizeof(struct ar40xx_softc));

DRIVER_MODULE(ar40xx, simplebus, ar40xx_driver, 0, 0);
DRIVER_MODULE(ar40xx, ofwbus, ar40xx_driver, 0, 0);
DRIVER_MODULE(miibus, ar40xx, miibus_driver, 0, 0);
DRIVER_MODULE(mdio, ar40xx, mdio_driver, 0, 0);
DRIVER_MODULE(etherswitch, ar40xx, etherswitch_driver, 0, 0);
MODULE_DEPEND(ar40xx, mdio, 1, 1, 1);
MODULE_DEPEND(ar40xx, miibus, 1, 1, 1);
MODULE_DEPEND(ar40xx, etherswitch, 1, 1, 1);
MODULE_VERSION(ar40xx, 1);
