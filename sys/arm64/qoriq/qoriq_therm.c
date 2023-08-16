/*-
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright  2020 Michal Meloun <mmel@FreeBSD.org>
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
 * Thermometer driver for QorIQ  SoCs.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "qoriq_therm_if.h"

#define	TMU_TMR		0x00
#define	TMU_TSR		0x04
#define TMUV1_TMTMIR	0x08
#define TMUV2_TMSR	0x08
#define TMUV2_TMTMIR	0x0C
#define	TMU_TIER	0x20
#define	TMU_TTCFGR	0x80
#define	TMU_TSCFGR	0x84
#define	TMU_TRITSR(x)	(0x100 + (16 * (x)))
#define	 TMU_TRITSR_VALID	(1U << 31)
#define	TMUV2_TMSAR(x)	(0x304 + (16 * (x)))
#define	TMU_VERSION	0xBF8			/* not in TRM */
#define	TMUV2_TEUMR(x)	(0xF00 + (4 * (x)))
#define	TMU_TTRCR(x)	(0xF10 + (4 * (x)))


struct tsensor {
	int			site_id;
	char 			*name;
	int			id;
};

struct qoriq_therm_softc {
	device_t		dev;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void			*irq_ih;
	int			ntsensors;
	struct tsensor		*tsensors;
	bool			little_endian;
	clk_t			clk;
	int			ver;
};

static struct sysctl_ctx_list qoriq_therm_sysctl_ctx;

struct tsensor default_sensors[] =
{
	{ 0,	"site0",		0 },
	{ 1,	"site1",		1 },
	{ 2,	"site2",		2 },
	{ 3,	"site3",		3 },
	{ 4,	"site4",		4 },
	{ 5,	"site5",		5 },
	{ 6,	"site6",		6 },
	{ 7,	"site7",		7 },
	{ 8,	"site8",		8 },
	{ 9,	"site9",		9 },
	{ 10,	"site10",		10 },
	{ 11,	"site11",		11 },
	{ 12,	"site12",		12 },
	{ 13,	"site13",		13 },
	{ 14,	"site14",		14 },
	{ 15,	"site15",		15 },
};

static struct tsensor imx8mq_sensors[] =
{
	{ 0,	"cpu",			0 },
	{ 1,	"gpu",			1 },
	{ 2,	"vpu",			2 },
};

static struct tsensor ls1012_sensors[] =
{
	{ 0,	"cpu-thermal",		0 },
};

static struct tsensor ls1028_sensors[] =
{
	{ 0,	"ddr-controller",	0 },
	{ 1,	"core-cluster",		1 },
};

static struct tsensor ls1043_sensors[] =
{
	{ 0,	"ddr-controller",	0 },
	{ 1,	"serdes",		1 },
	{ 2,	"fman",			2 },
	{ 3,	"core-cluster",		3 },
};

static struct tsensor ls1046_sensors[] =
{
	{ 0,	"ddr-controller",	0 },
	{ 1,	"serdes",		1 },
	{ 2,	"fman",			2 },
	{ 3,	"core-cluster",		3 },
	{ 4,	"sec",			4 },
};

static struct tsensor ls1088_sensors[] =
{
	{ 0,	"core-cluster",		0 },
	{ 1,	"soc",			1 },
};

/* Note: tmu[1..7] not [0..6]. */
static struct tsensor lx2080_sensors[] =
{
	{ 1,	"ddr-controller1",	0 },
	{ 2,	"ddr-controller2",	1 },
	{ 3,	"ddr-controller3",	2 },
	{ 4,	"core-cluster1",	3 },
	{ 5,	"core-cluster2",	4 },
	{ 6,	"core-cluster3",	5 },
	{ 7,	"core-cluster4",	6 },
};

static struct tsensor lx2160_sensors[] =
{
	{ 0,	"cluster6-7",		0 },
	{ 1,	"ddr-cluster5",		1 },
	{ 2,	"wriop",		2 },
	{ 3,	"dce-qbman-hsio2",	3 },
	{ 4,	"ccn-dpaa-tbu",		4 },
	{ 5,	"cluster4-hsio3",	5 },
	{ 6,	"cluster2-3",		6 },
};

struct qoriq_therm_socs {
	const char		*name;
	struct tsensor		*tsensors;
	int			ntsensors;
} qoriq_therm_socs[] = {
#define	_SOC(_n, _a)	{ _n, _a, nitems(_a) }
	_SOC("fsl,imx8mq",	imx8mq_sensors),
	_SOC("fsl,ls1012a",	ls1012_sensors),
	_SOC("fsl,ls1028a",	ls1028_sensors),
	_SOC("fsl,ls1043a",	ls1043_sensors),
	_SOC("fsl,ls1046a",	ls1046_sensors),
	_SOC("fsl,ls1088a",	ls1088_sensors),
	_SOC("fsl,ls2080a",	lx2080_sensors),
	_SOC("fsl,lx2160a",	lx2160_sensors),
	{ NULL,	NULL, 0 }
#undef _SOC
};

static struct ofw_compat_data compat_data[] = {
	{"fsl,qoriq-tmu",	1},
	{"fsl,imx8mq-tmu",	1},
	{NULL,			0},
};

static inline void
WR4(struct qoriq_therm_softc *sc, bus_size_t addr, uint32_t val)
{

	val = sc->little_endian ? htole32(val): htobe32(val);
	bus_write_4(sc->mem_res, addr, val);
}

static inline uint32_t
RD4(struct qoriq_therm_softc *sc, bus_size_t addr)
{
	uint32_t val;

	val = bus_read_4(sc->mem_res, addr);
	return (sc->little_endian ? le32toh(val): be32toh(val));
}

static int
qoriq_therm_read_temp(struct qoriq_therm_softc *sc, struct tsensor *sensor,
    int *temp)
{
	int timeout;
	uint32_t val;

	/* wait for valid sample */
	for (timeout = 1000; timeout > 0; timeout--) {
		val = RD4(sc, TMU_TRITSR(sensor->site_id));
		if (val & TMU_TRITSR_VALID)
			break;
		DELAY(100);
	}
	if (timeout <= 0)
		device_printf(sc->dev, "Sensor %s timeouted\n", sensor->name);

	*temp = (int)(val & 0x1FF) * 1000;
	if (sc->ver == 1)
		*temp = (int)(val & 0xFF) * 1000;
	else
		*temp = (int)(val & 0x1FF) * 1000 - 273100;

	return (0);
}

static int
qoriq_therm_get_temp(device_t dev, device_t cdev, uintptr_t id, int *val)
{
	struct qoriq_therm_softc *sc;

	sc = device_get_softc(dev);
	if (id >= sc->ntsensors)
		return (ERANGE);
	return(qoriq_therm_read_temp(sc, sc->tsensors + id, val));
}

static int
qoriq_therm_sysctl_temperature(SYSCTL_HANDLER_ARGS)
{
	struct qoriq_therm_softc *sc;
	int val;
	int rv;
	int id;

	/* Write request */
	if (req->newptr != NULL)
		return (EINVAL);

	sc = arg1;
	id = arg2;

	if (id >= sc->ntsensors)
		return (ERANGE);
	rv =  qoriq_therm_read_temp(sc, sc->tsensors + id, &val);
	if (rv != 0)
		return (rv);

	val = val / 100;
	val +=  2731;
	rv = sysctl_handle_int(oidp, &val, 0, req);
	return (rv);
}

static int
qoriq_therm_init_sysctl(struct qoriq_therm_softc *sc)
{
	int i;
	struct sysctl_oid *oid, *tmp;

	/* create node for hw.temp */
	oid = SYSCTL_ADD_NODE(&qoriq_therm_sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO, "temperature",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
	if (oid == NULL)
		return (ENXIO);

	/* add sensors */
	for (i = sc->ntsensors  - 1; i >= 0; i--) {
		tmp = SYSCTL_ADD_PROC(&qoriq_therm_sysctl_ctx,
		    SYSCTL_CHILDREN(oid), OID_AUTO, sc->tsensors[i].name,
		    CTLTYPE_INT | CTLFLAG_RD , sc, i,
		    qoriq_therm_sysctl_temperature, "IK", "SoC Temperature");
		if (tmp == NULL)
			return (ENXIO);
	}
	return (0);
}

static int
qoriq_therm_fdt_calib(struct qoriq_therm_softc *sc, phandle_t node)
{
	int 	nranges, ncalibs, i;
	int	*ranges, *calibs;

	/* initialize temperature range control registes */
	nranges = OF_getencprop_alloc_multi(node, "fsl,tmu-range",
	    sizeof(*ranges), (void **)&ranges);
	if (nranges < 2 || nranges > 4) {
		device_printf(sc->dev, "Invalid 'tmu-range' property\n");
		return (ERANGE);
	}
	for (i = 0; i < nranges; i++) {
		WR4(sc, TMU_TTRCR(i), ranges[i]);
	}

	/* initialize calibration data for above ranges */
	ncalibs = OF_getencprop_alloc_multi(node, "fsl,tmu-calibration",
	    sizeof(*calibs),(void **)&calibs);
	if (ncalibs <= 0 || (ncalibs % 2) != 0) {
		device_printf(sc->dev, "Invalid 'tmu-calibration' property\n");
		return (ERANGE);
	}
	for (i = 0; i < ncalibs; i +=2) {
		WR4(sc, TMU_TTCFGR, calibs[i]);
		WR4(sc, TMU_TSCFGR, calibs[i + 1]);
	}

	return (0);
}

static int
qoriq_therm_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "QorIQ temperature sensors");
	return (BUS_PROBE_DEFAULT);
}

static int
qoriq_therm_attach(device_t dev)
{
	struct qoriq_therm_softc *sc;
	struct qoriq_therm_socs *soc;
	phandle_t node, root;
	uint32_t sites;
	int rid, rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(sc->dev);
	sc->little_endian = OF_hasprop(node, "little-endian");

	sysctl_ctx_init(&qoriq_therm_sysctl_ctx);

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		goto fail;
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate IRQ resources\n");
		goto fail;
	}

/*
	if ((bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    qoriq_therm_intr, NULL, sc, &sc->irq_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		goto fail;
	}
*/
	rv = clk_get_by_ofw_index(dev, 0, 0, &sc->clk);
	if (rv != 0 && rv != ENOENT) {
		device_printf(dev, "Cannot get clock: %d %d\n", rv, ENOENT);
		goto fail;
	}
	if (sc->clk != NULL) {
		rv = clk_enable(sc->clk);
		if (rv != 0) {
			device_printf(dev, "Cannot enable clock: %d\n", rv);
			goto fail;
		}
	}

	sc->ver = (RD4(sc, TMU_VERSION) >> 8) & 0xFF;

	/* Select per SoC configuration. */
	root = OF_finddevice("/");
	if (root < 0) {
		device_printf(dev, "Cannot get root node: %d\n", root);
		goto fail;
	}
	soc = qoriq_therm_socs;
	while (soc != NULL && soc->name != NULL) {
		if (ofw_bus_node_is_compatible(root, soc->name))
			break;
		soc++;
	}
	if (soc == NULL) {
		device_printf(dev, "Unsupported SoC, using default sites.\n");
		sc->tsensors = default_sensors;
		sc->ntsensors = nitems(default_sensors);
	} else {
		sc->tsensors = soc->tsensors;
		sc->ntsensors = soc->ntsensors;
	}

	/* stop monitoring */
	WR4(sc, TMU_TMR, 0);
	RD4(sc, TMU_TMR);

	/* disable all interrupts */
	WR4(sc, TMU_TIER, 0);

	/* setup measurement interval */
	if (sc->ver == 1) {
		WR4(sc, TMUV1_TMTMIR, 0x0F);
	} else {
		WR4(sc, TMUV2_TMTMIR, 0x0F);	/* disable */
		/* these registers are not of settings is not in TRM */
		WR4(sc, TMUV2_TEUMR(0), 0x51009c00);
		for (int i = 0; i < sc->ntsensors; i++)
			WR4(sc, TMUV2_TMSAR(sc->tsensors[i].site_id), 0xE);
	}

	/* prepare calibration tables */
	rv = qoriq_therm_fdt_calib(sc, node);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot initialize calibration tables\n");
		goto fail;
	}
	/* start monitoring */
	sites = 0;
	if (sc->ver == 1) {
		for (int i = 0; i < sc->ntsensors; i++)
			sites |= 1 << (15 - sc->tsensors[i].site_id);
		WR4(sc, TMU_TMR, 0x8C000000 | sites);
	} else {
		for (int i = 0; i < sc->ntsensors; i++)
			sites |= 1 << sc->tsensors[i].site_id;
		WR4(sc, TMUV2_TMSR, sites);
		WR4(sc, TMU_TMR, 0x83000000);
	}

	rv = qoriq_therm_init_sysctl(sc);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot initialize sysctls\n");
		goto fail;
	}

	OF_device_register_xref(OF_xref_from_node(node), dev);
	return (bus_generic_attach(dev));

fail:
	if (sc->irq_ih != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_ih);
	sysctl_ctx_free(&qoriq_therm_sysctl_ctx);
	if (sc->clk != NULL)
		clk_release(sc->clk);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	return (ENXIO);
}

static int
qoriq_therm_detach(device_t dev)
{
	struct qoriq_therm_softc *sc;
	sc = device_get_softc(dev);

	if (sc->irq_ih != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_ih);
	sysctl_ctx_free(&qoriq_therm_sysctl_ctx);
	if (sc->clk != NULL)
		clk_release(sc->clk);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	return (0);
}

static device_method_t qoriq_qoriq_therm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			qoriq_therm_probe),
	DEVMETHOD(device_attach,		qoriq_therm_attach),
	DEVMETHOD(device_detach,		qoriq_therm_detach),

	/* SOCTHERM interface */
	DEVMETHOD(qoriq_therm_get_temperature,	qoriq_therm_get_temp),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(soctherm, qoriq_qoriq_therm_driver, qoriq_qoriq_therm_methods,
    sizeof(struct qoriq_therm_softc));
DRIVER_MODULE(qoriq_soctherm, simplebus, qoriq_qoriq_therm_driver, NULL, NULL);
