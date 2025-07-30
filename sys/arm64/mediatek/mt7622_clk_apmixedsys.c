/*-
 * Copyright (c) 2025 Martin Filla
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/clock/mt7622-clk.h>
#include <dev/clk/clk_gate.h>
#include "clkdev_if.h"
#include "hwreset_if.h"
#include "mdtk_clk.h"
#include "mt7622_clk_pll.h"

static struct ofw_compat_data compat_data[] = {
        {"mediatek,mt7622-apmixedsys", 1},
        {NULL, 0},
};

static struct clk_pll_def pll_clk[] = {
        PLL(CLK_APMIXED_ARMPLL, "armpll", "clkxtal", 0x0200, 0x020C, 0x00000001,
            2, 21, 0x0204, 24, 0, 0x0204, 0),
        PLL(CLK_APMIXED_MAINPLL, "mainpll", "clkxtal", 0x0210, 0x021C, 0x00000001,
            1, 21, 0x0214, 24, 0, 0x0214, 0),
        PLL(CLK_APMIXED_UNIV2PLL, "univ2pll", "clkxtal", 0x0220, 0x022C, 0x00000001,
            1, 7, 0x0224, 24, 0, 0x0224, 14),
        PLL(CLK_APMIXED_ETH1PLL, "eth1pll", "clkxtal", 0x0300, 0x0310, 0x00000001,
            0, 21, 0x0300, 1, 0, 0x0304, 0),
        PLL(CLK_APMIXED_ETH2PLL, "eth2pll", "clkxtal", 0x0314, 0x0320, 0x00000001,
            0, 21, 0x0314, 1, 0, 0x0318, 0),
        PLL(CLK_APMIXED_AUD1PLL, "aud1pll", "clkxtal", 0x0324, 0x0330, 0x00000001,
            0, 31, 0x0324, 1, 0, 0x0328, 0),
        PLL(CLK_APMIXED_AUD2PLL, "aud2pll", "clkxtal", 0x0334, 0x0340, 0x00000001,
            0, 31, 0x0334, 1, 0, 0x0338, 0),
        PLL(CLK_APMIXED_TRGPLL, "trgpll", "clkxtal", 0x0344, 0x0354, 0x00000001,
            0, 21, 0x0344, 1, 0, 0x0348, 0),
        PLL(CLK_APMIXED_SGMIPLL, "sgmipll", "clkxtal",  0x0358, 0x0368, 0x00000001,
            0, 21, 0x0358, 1, 0, 0x035C, 0),
};

static struct clk_gate_def gates_clk[] = {
        GATE(CLK_APMIXED_MAIN_CORE_EN, "main_core_en", "mainpll", 0x8, 5),
};

static struct mdtk_clk_def clk_def = {
        .pll_def = pll_clk,
        .num_pll = nitems(pll_clk),
        .num_gates = nitems(gates_clk),
        .gates_def = gates_clk,
};

static int
apmixedsys_clk_detach(device_t dev)
{
    device_printf(dev, "Error: Clock driver cannot be detached\n");
    return (EBUSY);
}

static int
apmixedsys_clk_probe(device_t dev)
{
    if (!ofw_bus_status_okay(dev))
        return (ENXIO);

    if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
        device_set_desc(dev, "Mediatek mt7622 apmixedsys clocks");
        return (BUS_PROBE_DEFAULT);
    }

    return (ENXIO);
}

static int
apmixedsys_clk_attach(device_t dev) {
    struct mdtk_clk_softc *sc = device_get_softc(dev);
    int rid, rv;

    sc->dev = dev;

    mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

    /* Resource setup. */
    rid = 0;
    sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
                                         RF_ACTIVE);
    if (!sc->mem_res) {
        device_printf(dev, "cannot allocate memory resource\n");
        rv = ENXIO;
        goto fail;
    }

    mdtk_register_clocks(dev,  &clk_def);
    return (0);

    fail:
    if (sc->mem_res)
        bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

    return (rv);
}

static device_method_t mt7622_apmixedsys_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,		 apmixedsys_clk_probe),
        DEVMETHOD(device_attach,	 apmixedsys_clk_attach),
        DEVMETHOD(device_detach, 	 apmixedsys_clk_detach),

        /* Clkdev interface*/
        DEVMETHOD(clkdev_read_4,        mdtk_clkdev_read_4),
        DEVMETHOD(clkdev_write_4,	    mdtk_clkdev_write_4),
        DEVMETHOD(clkdev_modify_4,	    mdtk_clkdev_modify_4),
        DEVMETHOD(clkdev_device_lock,	mdtk_clkdev_device_lock),
        DEVMETHOD(clkdev_device_unlock,	mdtk_clkdev_device_unlock),
        DEVMETHOD_END
};

DEFINE_CLASS_0(mt7622_apmixedsys, mt7622_apmixedsys_driver, mt7622_apmixedsys_methods,
sizeof(struct mdtk_clk_softc));
EARLY_DRIVER_MODULE(mt7622_apmixedsys, simplebus, mt7622_apmixedsys_driver, NULL, NULL,
        BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 1);