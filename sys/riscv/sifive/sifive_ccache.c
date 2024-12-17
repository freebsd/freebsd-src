/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
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
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/fdt/fdt_common.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#define	SIFIVE_CCACHE_CONFIG	0x000
#define	 CCACHE_CONFIG_WAYS_S	8
#define	 CCACHE_CONFIG_WAYS_M	(0xff << CCACHE_CONFIG_WAYS_S)
#define	SIFIVE_CCACHE_WAYENABLE	0x008
#define	SIFIVE_CCACHE_FLUSH64	0x200

#define	SIFIVE_CCACHE_LINE_SIZE	64

#define	RD8(sc, off)		(bus_read_8((sc)->res, (off)))
#define	WR8(sc, off, val)	(bus_write_8((sc)->res, (off), (val)))
#define	CC_WR8(offset, value)	\
    *(volatile uint64_t *)((uintptr_t)ccache_va + (offset)) = (value)

static struct ofw_compat_data compat_data[] = {
	{ "sifive,eic7700",			1 },
	{ NULL,					0 }
};

struct ccache_softc {
	struct resource	*res;
};

static void *ccache_va = NULL;

static struct resource_spec ccache_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

/*
 * Non-standard EIC7700 cache-flushing routine.
 */
static void
ccache_flush_range(vm_offset_t start, size_t len)
{
	vm_offset_t paddr;
	vm_offset_t sva;
	vm_offset_t step;
	uint64_t line;

	if (ccache_va == NULL || len == 0)
		return;

	mb();

	for (sva = start; len > 0;) {
		paddr = pmap_kextract(sva);
		step = min(PAGE_SIZE - (paddr & PAGE_MASK), len);
		for (line = rounddown2(paddr, SIFIVE_CCACHE_LINE_SIZE);
		    line < paddr + step;
		    line += SIFIVE_CCACHE_LINE_SIZE)
			CC_WR8(SIFIVE_CCACHE_FLUSH64, line);
		sva += step;
		len -= step;
	}

	mb();
}

static void
ccache_install_hooks(void)
{
	struct riscv_cache_ops eswin_ops;

	eswin_ops.dcache_wbinv_range = ccache_flush_range;
	eswin_ops.dcache_inv_range = ccache_flush_range;
	eswin_ops.dcache_wb_range = ccache_flush_range;

	riscv_cache_install_hooks(&eswin_ops, SIFIVE_CCACHE_LINE_SIZE);
}

static int
ccache_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	if (device_get_unit(dev) != 0)
		return (ENXIO);

	device_set_desc(dev, "SiFive Cache Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
ccache_attach(device_t dev)
{
	struct ccache_softc *sc;
	size_t config, ways;

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, ccache_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	/* Non-standard EIC7700 cache unit configuration. */
	config = RD8(sc, SIFIVE_CCACHE_CONFIG);
	ways = (config & CCACHE_CONFIG_WAYS_M) >> CCACHE_CONFIG_WAYS_S;
	WR8(sc, SIFIVE_CCACHE_WAYENABLE, (ways - 1));

	ccache_va = rman_get_virtual(sc->res);
	ccache_install_hooks();

	return (0);
}

static device_method_t ccache_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ccache_probe),
	DEVMETHOD(device_attach,	ccache_attach),
	DEVMETHOD_END
};

static driver_t ccache_driver = {
	"ccache",
	ccache_methods,
	sizeof(struct ccache_softc),
};

EARLY_DRIVER_MODULE(ccache, simplebus, ccache_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_FIRST);
MODULE_VERSION(ccache, 1);
