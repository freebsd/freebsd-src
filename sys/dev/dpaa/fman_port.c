/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Shared bits of the FMan port drivers.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus_subr.h>
#include <machine/bus.h>

#include "fman.h"
#include "fman_port.h"
#include "fman_port_var.h"
#include "fman_if.h"

int
fman_port_attach_common(device_t dev, struct fman_port_softc *sc,
    int port_speed)
{
	phandle_t node;
	pcell_t cell;

	sc->sc_dev = dev;

	node = ofw_bus_get_node(dev);
	if (OF_getencprop(node, "cell-index", &cell, sizeof(cell)) < 0) {
		device_printf(dev, "No cell-index property\n");
		return (ENXIO);
	}
	sc->sc_port_id = cell;

	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 0,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_mem == NULL) {
		device_printf(dev, "failed to allocate MMIO\n");
		return (ENXIO);
	}

	FMAN_GET_REVISION(device_get_parent(dev),
	    &sc->sc_revision_major, &sc->sc_revision_minor);

	sc->sc_port_speed = port_speed;
	sc->sc_bm_max_pools = MAX_BM_POOLS;
	sc->sc_max_frame_length = PORT_MAX_FRAME_LENGTH;

	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);
}

int
fman_port_detach_common(device_t dev, struct fman_port_softc *sc)
{

	if (sc->sc_mem != NULL)
		bus_release_resource(dev, sc->sc_mem);

	return (0);
}

void
fman_port_config_common(struct fman_port_softc *sc,
    struct fman_port_params *params)
{

	sc->sc_default_fqid = params->dflt_fqid;
	sc->sc_err_fqid = params->err_fqid;

	sc->sc_max_port_fifo_size =
	    FMAN_GET_BMI_MAX_FIFO_SIZE(device_get_parent(sc->sc_dev));

	switch (sc->sc_revision_major) {
	case 2:
	case 3:
		sc->sc_max_ext_portals = 4;
		sc->sc_max_sub_portals = 12;
		break;
	case 6:
		sc->sc_max_ext_portals = 8;
		sc->sc_max_sub_portals = 16;
		break;
	}
}

int
fman_port_get_id(device_t dev)
{
	struct fman_port_softc *sc = device_get_softc(dev);

	return (sc->sc_port_id);
}
