/*-
 * Copyright 2016 Michal Meloun <mmel@FreeBSD.org>
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
#include "opt_platform.h"
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include  <dev/extres/phy/phy.h>

#include "phy_if.h"

struct phy {
	device_t	consumer_dev;	/* consumer device*/
	device_t	provider_dev;	/* provider device*/
	uintptr_t	phy_id;		/* phy id */
};

MALLOC_DEFINE(M_PHY, "phy", "Phy framework");

int
phy_init(device_t consumer, phy_t phy)
{

	return (PHY_INIT(phy->provider_dev, phy->phy_id, true));
}

int
phy_deinit(device_t consumer, phy_t phy)
{

	return (PHY_INIT(phy->provider_dev, phy->phy_id, false));
}


int
phy_enable(device_t consumer, phy_t phy)
{

	return (PHY_ENABLE(phy->provider_dev, phy->phy_id, true));
}

int
phy_disable(device_t consumer, phy_t phy)
{

	return (PHY_ENABLE(phy->provider_dev, phy->phy_id, false));
}

int
phy_status(device_t consumer, phy_t phy, int *value)
{

	return (PHY_STATUS(phy->provider_dev, phy->phy_id, value));
}

int
phy_get_by_id(device_t consumer_dev, device_t provider_dev, intptr_t id,
    phy_t *phy_out)
{
	phy_t phy;

	/* Create handle */
	phy = malloc(sizeof(struct phy), M_PHY,
	    M_WAITOK | M_ZERO);
	phy->consumer_dev = consumer_dev;
	phy->provider_dev = provider_dev;
	phy->phy_id = id;
	*phy_out = phy;
	return (0);
}

void
phy_release(phy_t phy)
{
	free(phy, M_PHY);
}


#ifdef FDT
int phy_default_map(device_t provider, phandle_t xref, int ncells,
    pcell_t *cells, intptr_t *id)
{

	if (ncells == 0)
		*id = 1;
	else if (ncells == 1)
		*id = cells[0];
	else
		return  (ERANGE);

	return (0);
}

int
phy_get_by_ofw_idx(device_t consumer_dev, phandle_t cnode, int idx, phy_t *phy)
{
	phandle_t xnode;
	pcell_t *cells;
	device_t phydev;
	int ncells, rv;
	intptr_t id;

	if (cnode <= 0)
		cnode = ofw_bus_get_node(consumer_dev);
	if (cnode <= 0) {
		device_printf(consumer_dev,
		    "%s called on not ofw based device\n", __func__);
		return (ENXIO);
	}
	rv = ofw_bus_parse_xref_list_alloc(cnode, "phys", "#phy-cells", idx,
	    &xnode, &ncells, &cells);
	if (rv != 0)
		return (rv);

	/* Tranlate provider to device. */
	phydev = OF_device_from_xref(xnode);
	if (phydev == NULL) {
		OF_prop_free(cells);
		return (ENODEV);
	}
	/* Map phy to number. */
	rv = PHY_MAP(phydev, xnode, ncells, cells, &id);
	OF_prop_free(cells);
	if (rv != 0)
		return (rv);

	return (phy_get_by_id(consumer_dev, phydev, id, phy));
}

int
phy_get_by_ofw_name(device_t consumer_dev, phandle_t cnode, char *name,
    phy_t *phy)
{
	int rv, idx;

	if (cnode <= 0)
		cnode = ofw_bus_get_node(consumer_dev);
	if (cnode <= 0) {
		device_printf(consumer_dev,
		    "%s called on not ofw based device\n",  __func__);
		return (ENXIO);
	}
	rv = ofw_bus_find_string_index(cnode, "phy-names", name, &idx);
	if (rv != 0)
		return (rv);
	return (phy_get_by_ofw_idx(consumer_dev, cnode, idx, phy));
}

int
phy_get_by_ofw_property(device_t consumer_dev, phandle_t cnode, char *name,
    phy_t *phy)
{
	pcell_t *cells;
	device_t phydev;
	int ncells, rv;
	intptr_t id;

	if (cnode <= 0)
		cnode = ofw_bus_get_node(consumer_dev);
	if (cnode <= 0) {
		device_printf(consumer_dev,
		    "%s called on not ofw based device\n", __func__);
		return (ENXIO);
	}
	ncells = OF_getencprop_alloc(cnode, name, sizeof(pcell_t),
	    (void **)&cells);
	if (ncells < 1)
		return (ENXIO);

	/* Tranlate provider to device. */
	phydev = OF_device_from_xref(cells[0]);
	if (phydev == NULL) {
		OF_prop_free(cells);
		return (ENODEV);
	}
	/* Map phy to number. */
	rv = PHY_MAP(phydev, cells[0], ncells - 1 , cells + 1, &id);
	OF_prop_free(cells);
	if (rv != 0)
		return (rv);

	return (phy_get_by_id(consumer_dev, phydev, id, phy));
}

void
phy_register_provider(device_t provider_dev)
{
	phandle_t xref, node;

	node = ofw_bus_get_node(provider_dev);
	if (node <= 0)
		panic("%s called on not ofw based device.\n", __func__);

	xref = OF_xref_from_node(node);
	OF_device_register_xref(xref, provider_dev);
}

void
phy_unregister_provider(device_t provider_dev)
{
	phandle_t xref;

	xref = OF_xref_from_device(provider_dev);
	OF_device_register_xref(xref, NULL);
}
#endif
