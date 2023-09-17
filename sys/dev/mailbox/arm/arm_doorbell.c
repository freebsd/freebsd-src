/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "arm_doorbell.h"

#define	MHU_CHAN_RX_LP		0x000	/* Low priority channel */
#define	MHU_CHAN_RX_HP		0x020	/* High priority channel */
#define	MHU_CHAN_RX_SEC		0x200	/* Secure channel */
#define	 MHU_INTR_STAT		0x00
#define	 MHU_INTR_SET		0x08
#define	 MHU_INTR_CLEAR		0x10

#define	MHU_TX_REG_OFFSET	0x100

#define	DOORBELL_N_CHANNELS	3
#define	DOORBELL_N_DOORBELLS	(DOORBELL_N_CHANNELS * 32)

struct arm_doorbell dbells[DOORBELL_N_DOORBELLS];

static struct resource_spec arm_doorbell_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ -1, 0 }
};

struct arm_doorbell_softc {
	struct resource		*res[3];
	void			*lp_intr_cookie;
	void			*hp_intr_cookie;
	device_t		dev;
};

static void
arm_doorbell_lp_intr(void *arg)
{
	struct arm_doorbell_softc *sc;
	struct arm_doorbell *db;
	uint32_t reg;
	int i;

	sc = arg;

	reg = bus_read_4(sc->res[0], MHU_CHAN_RX_LP + MHU_INTR_STAT);
	for (i = 0; i < 32; i++) {
		if (reg & (1 << i)) {
			db = &dbells[i];
			bus_write_4(sc->res[0], MHU_CHAN_RX_LP + MHU_INTR_CLEAR,
			    (1 << i));
			if (db->func != NULL)
				db->func(db->arg);
		}
	}
}

static void
arm_doorbell_hp_intr(void *arg)
{
	struct arm_doorbell_softc *sc;
	struct arm_doorbell *db;
	uint32_t reg;
	int i;

	sc = arg;

	reg = bus_read_4(sc->res[0], MHU_CHAN_RX_HP + MHU_INTR_STAT);
	for (i = 0; i < 32; i++) {
		if (reg & (1 << i)) {
			db = &dbells[i];
			bus_write_4(sc->res[0], MHU_CHAN_RX_HP + MHU_INTR_CLEAR,
			    (1 << i));
			if (db->func != NULL)
				db->func(db->arg);
		}
	}
}

static int
arm_doorbell_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "arm,mhu-doorbell"))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM MHU Doorbell");

	return (BUS_PROBE_DEFAULT);
}

static int
arm_doorbell_attach(device_t dev)
{
	struct arm_doorbell_softc *sc;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);

	if (bus_alloc_resources(dev, arm_doorbell_spec, sc->res) != 0) {
		device_printf(dev, "Can't allocate resources for device.\n");
		return (ENXIO);
	}

	/* Setup interrupt handlers. */
	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, arm_doorbell_lp_intr, sc, &sc->lp_intr_cookie);
	if (error != 0) {
		device_printf(dev, "Can't setup LP interrupt handler.\n");
		bus_release_resources(dev, arm_doorbell_spec, sc->res);
		return (ENXIO);
	}

	error = bus_setup_intr(dev, sc->res[2], INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, arm_doorbell_hp_intr, sc, &sc->hp_intr_cookie);
	if (error != 0) {
		device_printf(dev, "Can't setup HP interrupt handler.\n");
		bus_release_resources(dev, arm_doorbell_spec, sc->res);
		return (ENXIO);
	}

	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);
}

static int
arm_doorbell_detach(device_t dev)
{

	return (EBUSY);
}

struct arm_doorbell *
arm_doorbell_ofw_get(device_t dev, const char *name)
{
	phandle_t node, parent;
	struct arm_doorbell *db;
	device_t db_dev;
	pcell_t *cells;
	int nmboxes;
	int ncells;
	int idx;
	int db_id;
	int error;
	int chan;

	node = ofw_bus_get_node(dev);

	error = ofw_bus_parse_xref_list_get_length(node, "mboxes",
	    "#mbox-cells", &nmboxes);
	if (error) {
		device_printf(dev, "%s can't get mboxes list.\n", __func__);
		return (NULL);
	}

	if (nmboxes == 0) {
		device_printf(dev, "%s mbox list is empty.\n", __func__);
		return (NULL);
	}

	error = ofw_bus_find_string_index(node, "mbox-names", name, &idx);
	if (error != 0) {
		device_printf(dev, "%s can't find string index.\n",
		    __func__);
		return (NULL);
	}

	error = ofw_bus_parse_xref_list_alloc(node, "mboxes", "#mbox-cells",
	    idx, &parent, &ncells, &cells);
	if (error != 0) {
		device_printf(dev, "%s can't get mbox device xref\n",
		    __func__);
		return (NULL);
	}

	if (ncells != 2) {
		device_printf(dev, "Unexpected data size.\n");
		OF_prop_free(cells);
		return (NULL);
	}

	db_dev = OF_device_from_xref(parent);
	if (db_dev == NULL) {
		device_printf(dev, "%s: Can't get arm_doorbell device\n",
		    __func__);
		OF_prop_free(cells);
		return (NULL);
	}

	chan = cells[0];
	if (chan >= DOORBELL_N_CHANNELS) {
		device_printf(dev, "Unexpected channel number.\n");
		OF_prop_free(cells);
		return (NULL);
	}

	db_id = cells[1];
	if (db_id >= 32) {
		device_printf(dev, "Unexpected channel bit.\n");
		OF_prop_free(cells);
		return (NULL);
	}

	db = &dbells[chan * db_id];
	db->dev = dev;
	db->db_dev = db_dev;
	db->chan = chan;
	db->db = db_id;

	OF_prop_free(cells);

	return (db);
}

void
arm_doorbell_set(struct arm_doorbell *db)
{
	struct arm_doorbell_softc *sc;
	uint32_t offset;

	sc = device_get_softc(db->db_dev);

	switch (db->chan) {
	case 0:
		offset = MHU_CHAN_RX_LP;
		break;
	case 1:
		offset = MHU_CHAN_RX_HP;
		break;
	case 2:
		offset = MHU_CHAN_RX_SEC;
		break;
	default:
		panic("not reached");
	};

	offset |= MHU_TX_REG_OFFSET;

	bus_write_4(sc->res[0], offset + MHU_INTR_SET, (1 << db->db));
}

int
arm_doorbell_get(struct arm_doorbell *db)
{
	struct arm_doorbell_softc *sc;
	uint32_t offset;
	uint32_t reg;

	sc = device_get_softc(db->db_dev);

	switch (db->chan) {
	case 0:
		offset = MHU_CHAN_RX_LP;
		break;
	case 1:
		offset = MHU_CHAN_RX_HP;
		break;
	case 2:
		offset = MHU_CHAN_RX_SEC;
		break;
	default:
		panic("not reached");
	};

	reg = bus_read_4(sc->res[0], offset + MHU_INTR_STAT);
	if (reg & (1 << db->db)) {
		bus_write_4(sc->res[0], offset + MHU_INTR_CLEAR,
		    (1 << db->db));
		return (1);
	}

	return (0);
}

void
arm_doorbell_set_handler(struct arm_doorbell *db, void (*func)(void *),
    void *arg)
{

	db->func = func;
	db->arg = arg;
}

static device_method_t arm_doorbell_methods[] = {
	DEVMETHOD(device_probe,		arm_doorbell_probe),
	DEVMETHOD(device_attach,	arm_doorbell_attach),
	DEVMETHOD(device_detach,	arm_doorbell_detach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(arm_doorbell, arm_doorbell_driver, arm_doorbell_methods,
    sizeof(struct arm_doorbell_softc), simplebus_driver);

EARLY_DRIVER_MODULE(arm_doorbell, simplebus, arm_doorbell_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(arm_doorbell, 1);
