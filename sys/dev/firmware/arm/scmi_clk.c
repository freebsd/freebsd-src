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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/clk/clk.h>
#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "scmi.h"
#include "scmi_protocols.h"
#include "scmi_clk.h"

struct scmi_clk_softc {
	device_t	dev;
	device_t	scmi;
	struct clkdom	*clkdom;
};

struct scmi_clknode_softc {
	device_t	dev;
	int		clock_id;
};

static int
scmi_clk_get_rate(struct scmi_clk_softc *sc, int clk_id, uint64_t *rate)
{
	struct scmi_clk_rate_get_out out;
	struct scmi_clk_rate_get_in in;
	struct scmi_req req;
	int error;

	req.protocol_id = SCMI_PROTOCOL_ID_CLOCK;
	req.message_id = SCMI_CLOCK_RATE_GET;
	req.in_buf = &in;
	req.in_size = sizeof(struct scmi_clk_rate_get_in);
	req.out_buf = &out;
	req.out_size = sizeof(struct scmi_clk_rate_get_out);

	in.clock_id = clk_id;

	error = scmi_request(sc->scmi, &req);
	if (error != 0)
		return (error);

	if (out.status != 0)
		return (ENXIO);

	*rate = out.rate_lsb | ((uint64_t)out.rate_msb << 32);

	return (0);
}

static int
scmi_clk_set_rate(struct scmi_clk_softc *sc, int clk_id, uint64_t rate)
{
	struct scmi_clk_rate_set_out out;
	struct scmi_clk_rate_set_in in;
	struct scmi_req req;
	int error;

	req.protocol_id = SCMI_PROTOCOL_ID_CLOCK;
	req.message_id = SCMI_CLOCK_RATE_SET;
	req.in_buf = &in;
	req.in_size = sizeof(struct scmi_clk_rate_set_in);
	req.out_buf = &out;
	req.out_size = sizeof(struct scmi_clk_rate_set_out);

	in.clock_id = clk_id;
	in.flags = SCMI_CLK_RATE_ROUND_CLOSEST;
	in.rate_lsb = (uint32_t)rate;
	in.rate_msb = (uint32_t)(rate >> 32);

	error = scmi_request(sc->scmi, &req);
	if (error != 0)
		return (error);

	if (out.status != 0)
		return (ENXIO);

	return (0);
}

static int __unused
scmi_clk_gate(struct scmi_clk_softc *sc, int clk_id, int enable)
{
	struct scmi_clk_state_out out;
	struct scmi_clk_state_in in;
	struct scmi_req req;
	int error;

	req.protocol_id = SCMI_PROTOCOL_ID_CLOCK;
	req.message_id = SCMI_CLOCK_CONFIG_SET;
	req.in_buf = &in;
	req.in_size = sizeof(struct scmi_clk_state_in);
	req.out_buf = &out;
	req.out_size = sizeof(struct scmi_clk_state_out);

	in.clock_id = clk_id;
	in.attributes = enable;

	error = scmi_request(sc->scmi, &req);
	if (error != 0)
		return (error);

	if (out.status != 0)
		return (ENXIO);

	return (0);
}

static int
scmi_clknode_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);

	return (0);
}

static int
scmi_clknode_recalc_freq(struct clknode *clk, uint64_t *freq)
{
	struct scmi_clknode_softc *clk_sc;
	struct scmi_clk_softc *sc;
	uint64_t rate;
	int ret;

	clk_sc = clknode_get_softc(clk);
	sc = device_get_softc(clk_sc->dev);
	ret = scmi_clk_get_rate(sc, clk_sc->clock_id, &rate);
	if (ret == 0)
		*freq = rate;

	return (ret);
}

static int
scmi_clknode_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{
	struct scmi_clknode_softc *clk_sc;
	struct scmi_clk_softc *sc;

	clk_sc = clknode_get_softc(clk);
	sc = device_get_softc(clk_sc->dev);

	dprintf("%s: %ld\n", __func__, *fout);

	scmi_clk_set_rate(sc, clk_sc->clock_id, *fout);

	*stop = 1;

	return (0);
}

static clknode_method_t scmi_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		scmi_clknode_init),
	CLKNODEMETHOD(clknode_recalc_freq,	scmi_clknode_recalc_freq),
	CLKNODEMETHOD(clknode_set_freq,		scmi_clknode_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(scmi_clknode, scmi_clknode_class, scmi_clknode_methods,
    sizeof(struct scmi_clknode_softc), clknode_class);

static int
scmi_clk_add_node(struct scmi_clk_softc *sc, int index, char *clock_name)
{
	struct scmi_clknode_softc *clk_sc;
	struct clknode_init_def def;
	struct clknode *clk;

	memset(&def, 0, sizeof(def));
	def.id = index;
	def.name = clock_name;
	def.parent_names = NULL;
	def.parent_cnt = 0;

	clk = clknode_create(sc->clkdom, &scmi_clknode_class, &def);
	if (clk == NULL) {
		device_printf(sc->dev, "Cannot create clknode.\n");
		return (ENXIO);
	}

	clk_sc = clknode_get_softc(clk);
	clk_sc->dev = sc->dev;
	clk_sc->clock_id = index;

	if (clknode_register(sc->clkdom, clk) == NULL) {
		device_printf(sc->dev, "Could not register clock '%s'.\n",
		    def.name);
		return (ENXIO);
	}

	device_printf(sc->dev, "Clock '%s' registered.\n", def.name);

	return (0);
}

static int
scmi_clk_get_name(struct scmi_clk_softc *sc, int index, char **result)
{
	struct scmi_clk_name_get_out out;
	struct scmi_clk_name_get_in in;
	struct scmi_req req;
	char *clock_name;
	int error;

	req.protocol_id = SCMI_PROTOCOL_ID_CLOCK;
	req.message_id = SCMI_CLOCK_NAME_GET;
	req.in_buf = &in;
	req.in_size = sizeof(struct scmi_clk_name_get_in);
	req.out_buf = &out;
	req.out_size = sizeof(struct scmi_clk_name_get_out);

	in.clock_id = index;

	error = scmi_request(sc->scmi, &req);
	if (error != 0)
		return (error);

	if (out.status != 0)
		return (ENXIO);

	clock_name = malloc(sizeof(out.name), M_DEVBUF, M_WAITOK);
	strncpy(clock_name, out.name, sizeof(out.name));

	*result = clock_name;

	return (0);
}

static int
scmi_clk_attrs(struct scmi_clk_softc *sc, int index)
{
	struct scmi_clk_attrs_out out;
	struct scmi_clk_attrs_in in;
	struct scmi_req req;
	int error;
	char *clock_name;

	req.protocol_id = SCMI_PROTOCOL_ID_CLOCK;
	req.message_id = SCMI_CLOCK_ATTRIBUTES;
	req.in_buf = &in;
	req.in_size = sizeof(struct scmi_clk_attrs_in);
	req.out_buf = &out;
	req.out_size = sizeof(struct scmi_clk_attrs_out);

	in.clock_id = index;

	error = scmi_request(sc->scmi, &req);
	if (error != 0)
		return (error);

	if (out.status != 0)
		return (ENXIO);

	if (out.attributes & CLK_ATTRS_EXT_CLK_NAME) {
		error = scmi_clk_get_name(sc, index, &clock_name);
		if (error)
			return (error);
	} else {
		clock_name = malloc(sizeof(out.clock_name), M_DEVBUF, M_WAITOK);
		strncpy(clock_name, out.clock_name, sizeof(out.clock_name));
	}

	error = scmi_clk_add_node(sc, index, clock_name);

	return (error);
}

static int
scmi_clk_discover(struct scmi_clk_softc *sc)
{
	struct scmi_clk_protocol_attrs_out out;
	struct scmi_req req;
	int nclocks;
	int failing;
	int error;
	int i;

	req.protocol_id = SCMI_PROTOCOL_ID_CLOCK;
	req.message_id = SCMI_PROTOCOL_ATTRIBUTES;
	req.in_buf = NULL;
	req.in_size = 0;
	req.out_buf = &out;
	req.out_size = sizeof(struct scmi_clk_protocol_attrs_out);

	error = scmi_request(sc->scmi, &req);
	if (error != 0)
		return (error);

	if (out.status != 0)
		return (ENXIO);

	nclocks = (out.attributes & CLK_ATTRS_NCLOCKS_M) >>
	    CLK_ATTRS_NCLOCKS_S;

	device_printf(sc->dev, "Found %d clocks.\n", nclocks);

	failing = 0;

	for (i = 0; i < nclocks; i++) {
		error = scmi_clk_attrs(sc, i);
		if (error) {
			device_printf(sc->dev,
			    "Could not process clock index %d.\n", i);
			failing++;
		}
	}

	if (failing == nclocks)
		return (ENXIO);

	return (0);
}

static int
scmi_clk_init(struct scmi_clk_softc *sc)
{
	int error;

	/* Create clock domain */
	sc->clkdom = clkdom_create(sc->dev);
	if (sc->clkdom == NULL)
		return (ENXIO);

	error = scmi_clk_discover(sc);
	if (error) {
		device_printf(sc->dev, "Could not discover clocks.\n");
		return (ENXIO);
	}

	error = clkdom_finit(sc->clkdom);
	if (error) {
		device_printf(sc->dev, "Failed to init clock domain.\n");
		return (ENXIO);
	}

	return (0);
}

static int
scmi_clk_probe(device_t dev)
{
	phandle_t node;
	uint32_t reg;
	int error;

	node = ofw_bus_get_node(dev);

	error = OF_getencprop(node, "reg", &reg, sizeof(uint32_t));
	if (error < 0)
		return (ENXIO);

	if (reg != SCMI_PROTOCOL_ID_CLOCK)
		return (ENXIO);

	device_set_desc(dev, "SCMI Clock Management Unit");

	return (BUS_PROBE_DEFAULT);
}

static int
scmi_clk_attach(device_t dev)
{
	struct scmi_clk_softc *sc;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->scmi = device_get_parent(dev);

	node = ofw_bus_get_node(sc->dev);

	OF_device_register_xref(OF_xref_from_node(node), sc->dev);

	scmi_clk_init(sc);

	return (0);
}

static int
scmi_clk_detach(device_t dev)
{

	return (0);
}

static device_method_t scmi_clk_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		scmi_clk_probe),
	DEVMETHOD(device_attach,	scmi_clk_attach),
	DEVMETHOD(device_detach,	scmi_clk_detach),
	DEVMETHOD_END
};

static driver_t scmi_clk_driver = {
	"scmi_clk",
	scmi_clk_methods,
	sizeof(struct scmi_clk_softc),
};

EARLY_DRIVER_MODULE(scmi_clk, scmi, scmi_clk_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(scmi_clk, 1);
