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

#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_div.h>
#include <dev/clk/clk_mux.h>
#include <dev/clk/clk_gate.h>
#include <dev/clk/clk_link.h>
#include <arm64/mediatek/mdtk_clk.h>
#include "clkdev_if.h"
#include "hwreset_if.h"
#include "mdtk_clk.h"

static struct ofw_compat_data compat_data[] = {
        {"mediatek,mt7622-ssusbsys", 2},
        {NULL, 0},
};

static struct clk_gate_def gates_ssusb_clk[] = {
        GATE(CLK_SSUSB_U2_PHY_1P_EN, "ssusb_u2_phy_1p","to_u2_phy_1p", 0x30, 0),
        GATE(CLK_SSUSB_U2_PHY_EN, "ssusb_u2_phy_en", "to_u2_phy", 0x30, 1),
        GATE(CLK_SSUSB_REF_EN, "ssusb_ref_en", "to_usb3_ref", 0x30, 5),
        GATE(CLK_SSUSB_SYS_EN, "ssusb_sys_en", "to_usb3_sys", 0x30, 6),
        GATE(CLK_SSUSB_MCU_EN, "ssusb_mcu_en", "axi_sel", 0x30, 7),
        GATE(CLK_SSUSB_DMA_EN, "ssusb_dma_en", "hif_sel", 0x30, 8),
};

static struct mdtk_clk_def clk_ssusb_def = {
        .linked_def = NULL,
        .num_linked = 0,
        .fixed_def = NULL,
        .num_fixed = 0,
        .gates_def = gates_ssusb_clk,
        .num_gates = nitems(gates_ssusb_clk),
        .muxes_def = NULL,
        .num_muxes = 0,
};

static int
mt7622_ssusbsys_clk_detach(device_t dev)
{
    device_printf(dev, "Error: Clock driver cannot be detached\n");
    return (EBUSY);
}

static int
mt7622_ssusbsys_clk_probe(device_t dev)
{
    if (!ofw_bus_status_okay(dev))
        return (ENXIO);

    if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
        device_set_desc(dev, "Mediatek mt7622 ssusbsys clocks");
        return (BUS_PROBE_DEFAULT);
    }

    return (ENXIO);
}

static int
mt7622_ssusbsys_clk_attach(device_t dev) {
    struct mdtk_clk_softc *sc = device_get_softc(dev);
    int rid;

    sc->dev = dev;

    mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

    rid = 0;
    sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
                                         RF_ACTIVE);
    if (!sc->mem_res) {
        device_printf(dev, "cannot allocate memory resource\n");
        return ENXIO;
    }

    mdtk_register_clocks(dev, &clk_ssusb_def);

    return (0);
}

static device_method_t mt7622_ssusbsys_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,		 mt7622_ssusbsys_clk_probe),
        DEVMETHOD(device_attach,	 mt7622_ssusbsys_clk_attach),
        DEVMETHOD(device_detach, 	 mt7622_ssusbsys_clk_detach),

        /* Clkdev interface*/
        DEVMETHOD(clkdev_read_4,        mdtk_clkdev_read_4),
        DEVMETHOD(clkdev_write_4,	    mdtk_clkdev_write_4),
        DEVMETHOD(clkdev_modify_4,	    mdtk_clkdev_modify_4),
        DEVMETHOD(clkdev_device_lock,	mdtk_clkdev_device_lock),
        DEVMETHOD(clkdev_device_unlock,	mdtk_clkdev_device_unlock),
        DEVMETHOD_END
};

DEFINE_CLASS_0(mt7622_ssusbsys, mt7622_ssusbsys_driver, mt7622_ssusbsys_methods,
sizeof(struct mdtk_clk_softc));

EARLY_DRIVER_MODULE(mt7622_ssusbsys, simplebus, mt7622_ssusbsys_driver, NULL, NULL,
        BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 5);
