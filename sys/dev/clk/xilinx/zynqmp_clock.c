/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <machine/bus.h>
#include <sys/queue.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_fixed.h>

#include <dev/clk/xilinx/zynqmp_clk_mux.h>
#include <dev/clk/xilinx/zynqmp_clk_pll.h>
#include <dev/clk/xilinx/zynqmp_clk_fixed.h>
#include <dev/clk/xilinx/zynqmp_clk_div.h>
#include <dev/clk/xilinx/zynqmp_clk_gate.h>

#include <dev/firmware/xilinx/pm_defs.h>

#include "clkdev_if.h"
#include "zynqmp_firmware_if.h"

#define	ZYNQMP_MAX_NAME_LEN	16
#define	ZYNQMP_MAX_NODES	6
#define	ZYNQMP_MAX_PARENTS	100

#define	ZYNQMP_CLK_IS_VALID	(1 << 0)
#define	ZYNQMP_CLK_IS_EXT	(1 << 2)

#define	ZYNQMP_GET_NODE_TYPE(x)		(x & 0x7)
#define	ZYNQMP_GET_NODE_CLKFLAGS(x)	((x >> 8) & 0xFF)
#define	ZYNQMP_GET_NODE_TYPEFLAGS(x)	((x >> 24) & 0xF)

enum ZYNQMP_NODE_TYPE {
	CLK_NODE_TYPE_NULL = 0,
	CLK_NODE_TYPE_MUX,
	CLK_NODE_TYPE_PLL,
	CLK_NODE_TYPE_FIXED,
	CLK_NODE_TYPE_DIV0,
	CLK_NODE_TYPE_DIV1,
	CLK_NODE_TYPE_GATE,
};

/*
 * Clock IDs in the firmware starts at 0 but
 * exported clocks (and so clock exposed by the clock framework)
 * starts at 1
 */
#define	ZYNQMP_ID_TO_CLK(x)	((x) + 1)
#define	CLK_ID_TO_ZYNQMP(x)	((x) - 1)

struct zynqmp_clk {
	TAILQ_ENTRY(zynqmp_clk)	next;
	struct clknode_init_def	clkdef;
	uint32_t		id;
	uint32_t		parentids[ZYNQMP_MAX_PARENTS];
	uint32_t		topology[ZYNQMP_MAX_NODES];
	uint32_t		attributes;
};

struct zynqmp_clock_softc {
	device_t			dev;
	device_t			parent;
	phandle_t			node;
	clk_t				clk_pss_ref;
	clk_t				clk_video;
	clk_t				clk_pss_alt_ref;
	clk_t				clk_aux_ref;
	clk_t				clk_gt_crx_ref;
	struct clkdom			*clkdom;
};

struct name_resp {
	char name[16];
};

struct zynqmp_clk_softc {
	struct zynqmp_clk	*clk;
	device_t		firmware;
	uint32_t		id;
};

static int
zynqmp_clk_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);
	return (0);
}

static clknode_method_t zynqmp_clk_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		zynqmp_clk_init),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(zynqmp_clk_clknode, zynqmp_clk_clknode_class,
    zynqmp_clk_clknode_methods, sizeof(struct zynqmp_clk_softc), clknode_class);

static int
zynqmp_clk_register(struct clkdom *clkdom, device_t fw, struct zynqmp_clk *clkdef)
{
	struct clknode *clknode;
	struct zynqmp_clk_softc *sc;
	char *prev_clock_name = NULL;
	char *clkname, *parent_name;
	struct clknode_init_def *zynqclk;
	int i;

	for (i = 0; i < ZYNQMP_MAX_NODES; i++) {
		/* Bail early if we have no node */
		if (ZYNQMP_GET_NODE_TYPE(clkdef->topology[i]) == CLK_NODE_TYPE_NULL)
			break;
		zynqclk = malloc(sizeof(*zynqclk), M_DEVBUF, M_WAITOK | M_ZERO);
		zynqclk->id = clkdef->clkdef.id;
		/* For the first node in the topology we use the main clock parents */
		if (i == 0) {
			zynqclk->parent_cnt = clkdef->clkdef.parent_cnt;
			zynqclk->parent_names = clkdef->clkdef.parent_names;
		} else {
			zynqclk->parent_cnt = 1;
			zynqclk->parent_names = malloc(sizeof(char *) * zynqclk->parent_cnt, M_DEVBUF, M_ZERO | M_WAITOK);
			parent_name = strdup(prev_clock_name, M_DEVBUF);
			zynqclk->parent_names[0] = (const char *)parent_name;
		}
		/* Register the clock node based on the topology type */
		switch (ZYNQMP_GET_NODE_TYPE(clkdef->topology[i])) {
		case CLK_NODE_TYPE_MUX:
			asprintf(&clkname, M_DEVBUF, "%s_mux", clkdef->clkdef.name);
			zynqclk->name = (const char *)clkname;
			zynqmp_clk_mux_register(clkdom, fw, zynqclk);
			break;
		case CLK_NODE_TYPE_PLL:
			asprintf(&clkname, M_DEVBUF, "%s_pll", clkdef->clkdef.name);
			zynqclk->name = (const char *)clkname;
			zynqmp_clk_pll_register(clkdom, fw, zynqclk);
			break;
		case CLK_NODE_TYPE_FIXED:
			asprintf(&clkname, M_DEVBUF, "%s_fixed", clkdef->clkdef.name);
			zynqclk->name = (const char *)clkname;
			zynqmp_clk_fixed_register(clkdom, fw, zynqclk);
			break;
		case CLK_NODE_TYPE_DIV0:
			asprintf(&clkname, M_DEVBUF, "%s_div0", clkdef->clkdef.name);
			zynqclk->name = (const char *)clkname;
			zynqmp_clk_div_register(clkdom, fw, zynqclk, CLK_DIV_TYPE_DIV0);
			break;
		case CLK_NODE_TYPE_DIV1:
			asprintf(&clkname, M_DEVBUF, "%s_div1", clkdef->clkdef.name);
			zynqclk->name = (const char *)clkname;
			zynqmp_clk_div_register(clkdom, fw, zynqclk, CLK_DIV_TYPE_DIV1);
			break;
		case CLK_NODE_TYPE_GATE:
			asprintf(&clkname, M_DEVBUF, "%s_gate", clkdef->clkdef.name);
			zynqclk->name = (const char *)clkname;
			zynqmp_clk_gate_register(clkdom, fw, zynqclk);
			break;
		case CLK_NODE_TYPE_NULL:
		default:
			clkname = NULL;
			break;
		}
		if (i != 0) {
			free(parent_name, M_DEVBUF);
			free(zynqclk->parent_names, M_DEVBUF);
		}
		if (clkname != NULL)
			prev_clock_name = strdup(clkname, M_DEVBUF);
		free(clkname, M_DEVBUF);
		free(zynqclk, M_DEVBUF);
	}

	/* Register main clock */
	clkdef->clkdef.name = clkdef->clkdef.name;
	clkdef->clkdef.parent_cnt = 1;
	clkdef->clkdef.parent_names = malloc(sizeof(char *) * clkdef->clkdef.parent_cnt, M_DEVBUF, M_ZERO | M_WAITOK);
	clkdef->clkdef.parent_names[0] = strdup(prev_clock_name, M_DEVBUF);
	clknode = clknode_create(clkdom, &zynqmp_clk_clknode_class, &clkdef->clkdef);
	if (clknode == NULL)
		return (1);
	sc = clknode_get_softc(clknode);
	sc->id = clkdef->clkdef.id - 1;
	sc->firmware = fw;
	sc->clk = clkdef;
	clknode_register(clkdom, clknode);
	return (0);
}

static int
zynqmp_fw_clk_get_name(struct zynqmp_clock_softc *sc, struct zynqmp_clk *clk, uint32_t id)
{
	char *clkname;
	uint32_t query_data[4];
	int rv;

	rv = ZYNQMP_FIRMWARE_QUERY_DATA(sc->parent, PM_QID_CLOCK_GET_NAME, id, 0, 0, query_data);
	if (rv != 0)
		return (rv);
	if (query_data[0] == '\0')
		return (EINVAL);
	clkname = malloc(ZYNQMP_MAX_NAME_LEN, M_DEVBUF, M_ZERO | M_WAITOK);
	memcpy(clkname, query_data, ZYNQMP_MAX_NAME_LEN);
	clk->clkdef.name = clkname;
	return (0);
}

static int
zynqmp_fw_clk_get_attributes(struct zynqmp_clock_softc *sc, struct zynqmp_clk *clk, uint32_t id)
{
	uint32_t query_data[4];
	int rv;

	rv = ZYNQMP_FIRMWARE_QUERY_DATA(sc->parent, PM_QID_CLOCK_GET_ATTRIBUTES, id, 0, 0, query_data);
	if (rv != 0)
		return (rv);
	clk->attributes = query_data[1];
	return (0);
}

static int
zynqmp_fw_clk_get_parents(struct zynqmp_clock_softc *sc, struct zynqmp_clk *clk, uint32_t id)
{
	int rv, i;
	uint32_t query_data[4];

	for (i = 0; i < ZYNQMP_MAX_PARENTS; i += 3) {
		clk->parentids[i] = -1;
		clk->parentids[i + 1] = -1;
		clk->parentids[i + 2] = -1;
		rv = ZYNQMP_FIRMWARE_QUERY_DATA(sc->parent, PM_QID_CLOCK_GET_PARENTS, id, i, 0, query_data);
		clk->parentids[i] = query_data[1] & 0xFFFF;
		clk->parentids[i + 1] = query_data[2] & 0xFFFF;
		clk->parentids[i + 2] = query_data[3] & 0xFFFF;
		if ((int32_t)query_data[1] == -1) {
			clk->parentids[i] = -1;
			break;
		}
		clk->parentids[i] += 1;
		clk->clkdef.parent_cnt++;
		if ((int32_t)query_data[2] == -1) {
			clk->parentids[i + 1] = -1;
			break;
		}
		clk->parentids[i + 1] += 1;
		clk->clkdef.parent_cnt++;
		if ((int32_t)query_data[3] == -1) {
			clk->parentids[i + 2] = -1;
			break;
		}
		clk->parentids[i + 2] += 1;
		clk->clkdef.parent_cnt++;
		if ((int32_t)query_data[1] == -2)
			clk->parentids[i] = -2;
		if ((int32_t)query_data[2] == -2)
			clk->parentids[i + 1] = -2;
		if ((int32_t)query_data[3] == -2)
			clk->parentids[i + 2] = -2;
		if (rv != 0)
			break;
	}
	return (0);
}

static int
zynqmp_fw_clk_get_topology(struct zynqmp_clock_softc *sc, struct zynqmp_clk *clk, uint32_t id)
{
	uint32_t query_data[4];
	int rv;

	rv = ZYNQMP_FIRMWARE_QUERY_DATA(sc->parent, PM_QID_CLOCK_GET_TOPOLOGY, id, 0, 0, query_data);
	if (rv != 0)
		return (rv);
	clk->topology[0] = query_data[1];
	clk->topology[1] = query_data[2];
	clk->topology[2] = query_data[3];
	if (query_data[3] == '\0')
		goto out;
	rv = ZYNQMP_FIRMWARE_QUERY_DATA(sc->parent, PM_QID_CLOCK_GET_TOPOLOGY, id, 3, 0, query_data);
	if (rv != 0)
		return (rv);
	clk->topology[3] = query_data[1];
	clk->topology[4] = query_data[2];
	clk->topology[5] = query_data[3];

out:
	return (0);
}

static int
zynqmp_clock_ofw_map(struct clkdom *clkdom, uint32_t ncells,
    phandle_t *cells, struct clknode **clk)
{

	if (ncells != 1)
		return (ERANGE);
	*clk = clknode_find_by_id(clkdom, ZYNQMP_ID_TO_CLK(cells[0]));
	if (*clk == NULL)
		return (ENXIO);
	return (0);
}

static int
zynqmp_fw_clk_get_all(struct zynqmp_clock_softc *sc)
{
	TAILQ_HEAD(tailhead, zynqmp_clk)	clk_list;
	struct zynqmp_clk *clk, *tmp, *tmp2;
	char *clkname;
	int rv, i;
	uint32_t query_data[4], num_clock;

	TAILQ_INIT(&clk_list);
	rv = ZYNQMP_FIRMWARE_QUERY_DATA(sc->parent,
	    PM_QID_CLOCK_GET_NUM_CLOCKS,
	    0,
	    0,
	    0,
	    query_data);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get clock details from the firmware\n");
		return (ENXIO);
	}

	num_clock = query_data[1];
	for (i = 0; i < num_clock; i++) {
		clk = malloc(sizeof(*clk), M_DEVBUF, M_WAITOK | M_ZERO);
		clk->clkdef.id = ZYNQMP_ID_TO_CLK(i);
		zynqmp_fw_clk_get_name(sc, clk, i);
		zynqmp_fw_clk_get_attributes(sc, clk, i);
		if ((clk->attributes & ZYNQMP_CLK_IS_VALID) == 0) {
			free(clk, M_DEVBUF);
			continue;
		}
		if (clk->attributes & ZYNQMP_CLK_IS_EXT)
			goto skip_ext;
		/* Get parents id */
		rv = zynqmp_fw_clk_get_parents(sc, clk, i);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot get parent for %s\n", clk->clkdef.name);
			free(clk, M_DEVBUF);
			continue;
		}
		/* Get topology */
		rv = zynqmp_fw_clk_get_topology(sc, clk, i);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot get topology for %s\n", clk->clkdef.name);
			free(clk, M_DEVBUF);
			continue;
		}
	skip_ext:
		TAILQ_INSERT_TAIL(&clk_list, clk, next);
	}

	/* Add a dummy clock */
	clk = malloc(sizeof(*clk), M_DEVBUF, M_WAITOK | M_ZERO);
	clkname = strdup("dummy", M_DEVBUF);
	clk->clkdef.name = (const char *)clkname;
	clk->clkdef.id = i;
	clk->attributes = ZYNQMP_CLK_IS_EXT;
	TAILQ_INSERT_TAIL(&clk_list, clk, next);

	/* Map parents id to name */
	TAILQ_FOREACH_SAFE(clk, &clk_list, next, tmp) {
		if (clk->attributes & ZYNQMP_CLK_IS_EXT)
			continue;
		clk->clkdef.parent_names = malloc(sizeof(char *) * clk->clkdef.parent_cnt, M_DEVBUF, M_ZERO | M_WAITOK);
		for (i = 0; i < ZYNQMP_MAX_PARENTS; i++) {
			if (clk->parentids[i] == -1)
				break;
			if (clk->parentids[i] == -2) {
				clk->clkdef.parent_names[i] = strdup("dummy", M_DEVBUF);
				continue;
			}
			TAILQ_FOREACH(tmp2, &clk_list, next) {
				if (tmp2->clkdef.id == clk->parentids[i]) {
					if (tmp2->attributes & ZYNQMP_CLK_IS_EXT) {
						int idx;

						if (ofw_bus_find_string_index( sc->node,
						    "clock-names", tmp2->clkdef.name, &idx) == ENOENT)
							clk->clkdef.parent_names[i] = strdup("dummy", M_DEVBUF);
						else
							clk->clkdef.parent_names[i] = strdup(tmp2->clkdef.name, M_DEVBUF);
					}
					else
						clk->clkdef.parent_names[i] = strdup(tmp2->clkdef.name, M_DEVBUF);
					break;
				}
			}
		}
	}

	sc->clkdom = clkdom_create(sc->dev);
	if (sc->clkdom == NULL)
		panic("Cannot create clkdom\n");
	clkdom_set_ofw_mapper(sc->clkdom, zynqmp_clock_ofw_map);

	/* Register the clocks */
	TAILQ_FOREACH_SAFE(clk, &clk_list, next, tmp) {
		if (clk->attributes & ZYNQMP_CLK_IS_EXT) {
			if (strcmp(clk->clkdef.name, "dummy") == 0) {
				struct clk_fixed_def dummy;

				bzero(&dummy, sizeof(dummy));
				dummy.clkdef.id = clk->clkdef.id;
				dummy.clkdef.name = strdup("dummy", M_DEVBUF);
				clknode_fixed_register(sc->clkdom, &dummy);
				free(__DECONST(char *, dummy.clkdef.name), M_DEVBUF);
			}
		} else
			zynqmp_clk_register(sc->clkdom, sc->parent, clk);

		TAILQ_REMOVE(&clk_list, clk, next);
		for (i = 0; i < clk->clkdef.parent_cnt; i++)
			free(__DECONST(char *, clk->clkdef.parent_names[i]), M_DEVBUF);
		free(clk->clkdef.parent_names, M_DEVBUF);
		free(__DECONST(char *, clk->clkdef.name), M_DEVBUF);
		free(clk, M_DEVBUF);
	}

	if (clkdom_finit(sc->clkdom) != 0)
		panic("cannot finalize clkdom initialization\n");

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	return (0);
}

static int
zynqmp_clock_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (!ofw_bus_is_compatible(dev, "xlnx,zynqmp-clk"))
		return (ENXIO);
	device_set_desc(dev, "ZynqMP Clock Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
zynqmp_clock_attach(device_t dev)
{
	struct zynqmp_clock_softc *sc;
	int rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->parent = device_get_parent(dev);
	sc->node = ofw_bus_get_node(dev);

	/* Enable all clocks */
	if (clk_get_by_ofw_name(dev, 0, "pss_ref_clk", &sc->clk_pss_ref) != 0) {
		device_printf(dev, "Cannot get pss_ref_clk clock\n");
		return (ENXIO);
	}
	rv = clk_enable(sc->clk_pss_ref);
	if (rv != 0) {
		device_printf(dev, "Could not enable clock pss_ref_clk\n");
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(dev, 0, "video_clk", &sc->clk_video) != 0) {
		device_printf(dev, "Cannot get video_clk clock\n");
		return (ENXIO);
	}
	rv = clk_enable(sc->clk_video);
	if (rv != 0) {
		device_printf(dev, "Could not enable clock video_clk\n");
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(dev, 0, "pss_alt_ref_clk", &sc->clk_pss_alt_ref) != 0) {
		device_printf(dev, "Cannot get pss_alt_ref_clk clock\n");
		return (ENXIO);
	}
	rv = clk_enable(sc->clk_pss_alt_ref);
	if (rv != 0) {
		device_printf(dev, "Could not enable clock pss_alt_ref_clk\n");
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(dev, 0, "aux_ref_clk", &sc->clk_aux_ref) != 0) {
		device_printf(dev, "Cannot get pss_aux_clk clock\n");
		return (ENXIO);
	}
	rv = clk_enable(sc->clk_aux_ref);
	if (rv != 0) {
		device_printf(dev, "Could not enable clock pss_aux_clk\n");
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(dev, 0, "gt_crx_ref_clk", &sc->clk_gt_crx_ref) != 0) {
		device_printf(dev, "Cannot get gt_crx_ref_clk clock\n");
		return (ENXIO);
	}
	rv = clk_enable(sc->clk_gt_crx_ref);
	if (rv != 0) {
		device_printf(dev, "Could not enable clock gt_crx_ref_clk\n");
		return (ENXIO);
	}

	rv = zynqmp_fw_clk_get_all(sc);
	if (rv != 0) {
		clk_disable(sc->clk_gt_crx_ref);
		clk_disable(sc->clk_aux_ref);
		clk_disable(sc->clk_pss_alt_ref);
		clk_disable(sc->clk_video);
		clk_disable(sc->clk_pss_ref);
		return (rv);
	}
	return (0);
}

static device_method_t zynqmp_clock_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, 	zynqmp_clock_probe),
	DEVMETHOD(device_attach, 	zynqmp_clock_attach),

	DEVMETHOD_END
};

static driver_t zynqmp_clock_driver = {
	"zynqmp_clock",
	zynqmp_clock_methods,
	sizeof(struct zynqmp_clock_softc),
};

EARLY_DRIVER_MODULE(zynqmp_clock, simplebus, zynqmp_clock_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LAST);
