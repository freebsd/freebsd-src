/*-
 * Copyright (c) 2017 Ian Lepore <ian@freebsd.org>
 * All rights reserved.
 *
 * Development sponsored by Microsemi, Inc.
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
/*
 * Utility functions for PHY drivers on systems configured using FDT data.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_fdt.h>

/*
 * Table to translate MII_CONTYPE_xxxx constants to/from devicetree strings.
 * We explicitly associate the enum values with the strings in a table to avoid
 * relying on this list being sorted in the same order as the enum in miivar.h,
 * and to avoid problems if the enum gains new types that aren't in the FDT
 * data.  However, the "unknown" entry must be first because it is referenced
 * using subscript 0 in mii_fdt_contype_to_name().
 */
static struct contype_names {
	mii_contype_t type;
	const char   *name;
} fdt_contype_names[] = {
	{MII_CONTYPE_UNKNOWN,		"unknown"},
	{MII_CONTYPE_MII,		"mii"},
	{MII_CONTYPE_GMII,		"gmii"},
	{MII_CONTYPE_SGMII,		"sgmii"},
	{MII_CONTYPE_QSGMII,		"qsgmii"},
	{MII_CONTYPE_TBI,		"tbi"},
	{MII_CONTYPE_REVMII,		"rev-mii"},
	{MII_CONTYPE_RMII,		"rmii"},
	{MII_CONTYPE_RGMII,		"rgmii"},
	{MII_CONTYPE_RGMII_ID,		"rgmii-id"},
	{MII_CONTYPE_RGMII_RXID,	"rgmii-rxid"},
	{MII_CONTYPE_RGMII_TXID,	"rgmii-txid"},
	{MII_CONTYPE_RTBI,		"rtbi"},
	{MII_CONTYPE_SMII,		"smii"},
	{MII_CONTYPE_XGMII,		"xgmii"},
	{MII_CONTYPE_TRGMII,		"trgmii"},
	{MII_CONTYPE_2000BX,		"2000base-x"},
	{MII_CONTYPE_2500BX,		"2500base-x"},
	{MII_CONTYPE_RXAUI,		"rxaui"},
};                                                           

static phandle_t
mii_fdt_get_phynode(phandle_t macnode)
{
	static const char *props[] = {
	    "phy-handle", "phy", "phy-device"
	};
	pcell_t xref;
	u_int i;

	for (i = 0; i < nitems(props); ++i) {
		if (OF_getencprop(macnode, props[i], &xref, sizeof(xref)) > 0)
			return (OF_node_from_xref(xref));
	}
	return (-1);
}

static phandle_t
mii_fdt_lookup_phy(phandle_t node, int addr)
{
	phandle_t ports, phynode, child;
	int reg;

	/* First try to see if we have a direct xref pointing to a PHY. */
	phynode = mii_fdt_get_phynode(node);
	if (phynode != -1)
		return (phynode);

	/*
	 * Now handle the "switch" case.
	 * Search "ports" subnode for nodes that describe a switch port
	 * including a PHY xref.
	 * Since we have multiple candidates select one based on PHY address.
	 */
	ports = ofw_bus_find_child(node, "ports");
	if (ports <= 0)
		ports = ofw_bus_find_child(node, "ethernet-ports");
	if (ports <= 0)
		return (-1);

	for (child = OF_child(ports); child != 0; child = OF_peer(child)) {
		if (ofw_bus_node_status_okay(child) == 0)
			continue;

		phynode = mii_fdt_get_phynode(child);
		if (phynode <= 0)
			continue;

		if (OF_getencprop(phynode, "reg", &reg, sizeof(reg)) <= 0)
			continue;

		if (reg == addr)
			return (phynode);
	}
	return (-1);
}

mii_contype_t
mii_fdt_contype_from_name(const char *name)
{
	u_int i;

	for (i = 0; i < nitems(fdt_contype_names); ++i) {
		if (strcmp(name, fdt_contype_names[i].name) == 0)
			return (fdt_contype_names[i].type);
	}
	return (MII_CONTYPE_UNKNOWN);
}

const char *
mii_fdt_contype_to_name(mii_contype_t contype)
{
	u_int i;

	for (i = 0; i < nitems(fdt_contype_names); ++i) {
		if (contype == fdt_contype_names[i].type)
			return (fdt_contype_names[i].name);
	}
	return (fdt_contype_names[0].name);
}

mii_contype_t
mii_fdt_get_contype(phandle_t macnode)
{
	char val[32];

	if (OF_getprop(macnode, "phy-mode", val, sizeof(val)) <= 0 &&
	    OF_getprop(macnode, "phy-connection-type", val, sizeof(val)) <= 0) {
                return (MII_CONTYPE_UNKNOWN);
	}
	return (mii_fdt_contype_from_name(val));
}

void
mii_fdt_free_config(struct mii_fdt_phy_config *cfg)
{

	free(cfg, M_OFWPROP);
}

mii_fdt_phy_config_t *
mii_fdt_get_config(device_t phydev)
{
	struct mii_attach_args *ma;
	mii_fdt_phy_config_t *cfg;
	device_t miibus, macdev;
	pcell_t val;

	ma = device_get_ivars(phydev);
	miibus = device_get_parent(phydev);
	macdev = device_get_parent(miibus);

	cfg = malloc(sizeof(*cfg), M_OFWPROP, M_ZERO | M_WAITOK);

	/*
	 * If we can't find our parent MAC's node, there's nothing more we can
	 * fill in; cfg is already full of zero/default values, return it.
	 */
	if ((cfg->macnode = ofw_bus_get_node(macdev)) == -1)
		return (cfg);

	cfg->con_type = mii_fdt_get_contype(cfg->macnode);

	/*
	 * If we can't find our own PHY node, there's nothing more we can fill
	 * in, just return what we've got.
	 */
	cfg->phynode = mii_fdt_lookup_phy(cfg->macnode, ma->mii_phyno);
	if (cfg->phynode == -1)
		return (cfg);

	if (OF_getencprop(cfg->phynode, "max-speed", &val, sizeof(val)) > 0)
		cfg->max_speed = val;

	if (ofw_bus_node_is_compatible(cfg->phynode,
	    "ethernet-phy-ieee802.3-c45"))
		cfg->flags |= MIIF_FDT_COMPAT_CLAUSE45;

	if (OF_hasprop(cfg->phynode, "broken-turn-around"))
		cfg->flags |= MIIF_FDT_BROKEN_TURNAROUND;
	if (OF_hasprop(cfg->phynode, "enet-phy-lane-swap"))
		cfg->flags |= MIIF_FDT_LANE_SWAP;
	if (OF_hasprop(cfg->phynode, "enet-phy-lane-no-swap"))
		cfg->flags |= MIIF_FDT_NO_LANE_SWAP;
	if (OF_hasprop(cfg->phynode, "eee-broken-100tx"))
		cfg->flags |= MIIF_FDT_EEE_BROKEN_100TX;
	if (OF_hasprop(cfg->phynode, "eee-broken-1000t"))
		cfg->flags |= MIIF_FDT_EEE_BROKEN_1000T;
	if (OF_hasprop(cfg->phynode, "eee-broken-10gt"))
		cfg->flags |= MIIF_FDT_EEE_BROKEN_10GT;
	if (OF_hasprop(cfg->phynode, "eee-broken-1000kx"))
		cfg->flags |= MIIF_FDT_EEE_BROKEN_1000KX;
	if (OF_hasprop(cfg->phynode, "eee-broken-10gkx4"))
		cfg->flags |= MIIF_FDT_EEE_BROKEN_10GKX4;
	if (OF_hasprop(cfg->phynode, "eee-broken-10gkr"))
		cfg->flags |= MIIF_FDT_EEE_BROKEN_10GKR;

	return (cfg);
}

static int
miibus_fdt_probe(device_t dev)
{
	device_t parent;

	parent = device_get_parent(dev);
	if (ofw_bus_get_node(parent) == -1)
		return (ENXIO);

	device_set_desc(dev, "OFW MII bus");
	return (BUS_PROBE_DEFAULT);
}

static int
miibus_fdt_attach(device_t dev)
{
	struct mii_attach_args *ma;
	int i, error, nchildren;
	device_t parent, *children;
	phandle_t phy_node;

	parent = device_get_parent(dev);

	error = device_get_children(dev, &children, &nchildren);
	if (error != 0 || nchildren == 0)
		return (ENXIO);

	for (i = 0; i < nchildren; i++) {
		ma = device_get_ivars(children[i]);
		bzero(&ma->obd, sizeof(ma->obd));
		phy_node = mii_fdt_lookup_phy(ofw_bus_get_node(parent),
		    ma->mii_phyno);
		if (phy_node == -1) {
			device_printf(dev,
			    "Warning: failed to find OFW node for PHY%d\n",
			    ma->mii_phyno);
			continue;
		}
		error = ofw_bus_gen_setup_devinfo(&ma->obd, phy_node);
		if (error != 0) {
			device_printf(dev,
			    "Warning: failed to setup OFW devinfo for PHY%d\n",
			    ma->mii_phyno);
			continue;
		}
		/*
		 * Setup interrupt resources.
		 * Only a handful of PHYs support those,
		 * so it's fine if we fail here.
		 */
		resource_list_init(&ma->rl);
		(void)ofw_bus_intr_to_rl(children[i], phy_node, &ma->rl, NULL);
	}

	free(children, M_TEMP);
	return (miibus_attach(dev));
}

static struct resource_list *
miibus_fdt_get_resource_list(device_t bus, device_t child)
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(child);

	if (ma->obd.obd_node == 0)
		return (NULL);

	return (&ma->rl);
}

static const struct ofw_bus_devinfo*
miibus_fdt_get_devinfo(device_t bus, device_t child)
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(child);

	if (ma->obd.obd_node == 0)
		return (NULL);

	return (&ma->obd);
}

static device_method_t miibus_fdt_methods[] = {
	DEVMETHOD(device_probe,		miibus_fdt_probe),
	DEVMETHOD(device_attach,	miibus_fdt_attach),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	miibus_fdt_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),
	DEVMETHOD(bus_release_resource,		bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,		bus_generic_adjust_resource),
	DEVMETHOD(bus_alloc_resource,		bus_generic_rl_alloc_resource),
	DEVMETHOD(bus_get_resource,		bus_generic_rl_get_resource),
	DEVMETHOD(bus_set_resource,		bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource_list,	miibus_fdt_get_resource_list),

	DEVMETHOD_END
};

DEFINE_CLASS_1(miibus, miibus_fdt_driver, miibus_fdt_methods,
    sizeof(struct mii_data), miibus_driver);
