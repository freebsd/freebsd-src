/*-
 * Copyright (c) 2025 Martin Filla, Michal Meloun <mmel@FreeBSD.org>
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
#include <dev/syscon/syscon.h>
#include <dt-bindings/clock/mt7622-clk.h>
#include <dev/clk/clk_mux.h>
#include <dev/clk/clk_gate.h>
#include <dev/hwreset/hwreset.h>
#include "syscon_if.h"
#include "clkdev_if.h"
#include "hwreset_if.h"
#include "mdtk_clk.h"

#define PERICFG_CG0  0x10
#define PERICFG_CG1  0x14

static struct ofw_compat_data compat_data[] = {
    {"mediatek,mt7622-pericfg",	1},
    {NULL,		 	0},
};

PLIST(peribus_ck_parents) = {
        "syspll1_d8",
        "syspll1_d4"
};

static struct clk_gate_def gates_clk[] = {
    GATE(CLK_PERI_THERM_PD, "peri_therm_pd", "axi_sel", PERICFG_CG0, 1),
    GATE(CLK_PERI_PWM1_PD, "peri_pwm1_pd", "clkxtal", PERICFG_CG0, 2),
    GATE(CLK_PERI_PWM2_PD, "peri_pwm2_pd", "clkxtal", PERICFG_CG0, 3),
    GATE(CLK_PERI_PWM3_PD, "peri_pwm3_pd", "clkxtal", PERICFG_CG0, 4),
    GATE(CLK_PERI_PWM4_PD, "peri_pwm4_pd", "clkxtal", PERICFG_CG0, 5),
    GATE(CLK_PERI_PWM5_PD, "peri_pwm5_pd", "clkxtal", PERICFG_CG0, 6),
    GATE(CLK_PERI_PWM6_PD, "peri_pwm6_pd", "clkxtal", PERICFG_CG0, 7),
    GATE(CLK_PERI_PWM7_PD, "peri_pwm7_pd", "clkxtal", PERICFG_CG0, 8),
    GATE(CLK_PERI_PWM_PD, "peri_pwm_pd", "clkxtal", PERICFG_CG0, 9),
    GATE(CLK_PERI_AP_DMA_PD, "peri_ap_dma_pd", "axi_sel", PERICFG_CG0, 12),
    GATE(CLK_PERI_MSDC30_0_PD, "peri_msdc30_0", "msdc30_0_sel", PERICFG_CG0, 13),
    GATE(CLK_PERI_MSDC30_1_PD, "peri_msdc30_1", "msdc30_1_sel", PERICFG_CG0, 14),
    GATE(CLK_PERI_UART0_PD, "peri_uart0_pd", "axi_sel", PERICFG_CG0, 17),
    GATE(CLK_PERI_UART1_PD, "peri_uart1_pd", "axi_sel", PERICFG_CG0, 18),
    GATE(CLK_PERI_UART2_PD, "peri_uart2_pd", "axi_sel", PERICFG_CG0, 19),
    GATE(CLK_PERI_UART3_PD, "peri_uart3_pd", "axi_sel", PERICFG_CG0, 20),
    GATE(CLK_PERI_UART4_PD, "peri_uart4_pd", "axi_sel", PERICFG_CG0, 21),
    GATE(CLK_PERI_BTIF_PD, "peri_btif_pd", "axi_sel", PERICFG_CG0, 22),
    GATE(CLK_PERI_I2C0_PD, "peri_i2c0_pd", "axi_sel", PERICFG_CG0, 23),
    GATE(CLK_PERI_I2C1_PD, "peri_i2c1_pd", "axi_sel", PERICFG_CG0, 24),
    GATE(CLK_PERI_I2C2_PD, "peri_i2c2_pd", "axi_sel", PERICFG_CG0, 25),
    GATE(CLK_PERI_SPI1_PD, "peri_spi1_pd", "spi1_sel", PERICFG_CG0, 26),
    GATE(CLK_PERI_AUXADC_PD, "peri_auxadc_pd", "clkxtal", PERICFG_CG0, 27),
    GATE(CLK_PERI_SPI0_PD, "peri_spi0_pd", "spi0_sel", PERICFG_CG0, 28),
    GATE(CLK_PERI_SNFI_PD, "peri_snfi_pd", "spinfi_infra_bclk_sel", PERICFG_CG0, 29),
    GATE(CLK_PERI_NFI_PD, "peri_nfi_pd", "axi_sel", PERICFG_CG0, 30),
    GATE(CLK_PERI_NFIECC_PD, "peri_nfiecc_pd", "axi_sel", PERICFG_CG0, 31),

    GATE(CLK_PERI_FLASH_PD, "peri_flash_pd", "flash_sel", PERICFG_CG1, 1),
    GATE(CLK_PERI_IRTX_PD, "peri_irtx_pd", "irtx_sel", PERICFG_CG1, 2),
};

static struct clk_mux_def muxes_clk[] = {
    MUX0(0, "peribus_ck_sel_mux", peribus_ck_parents, 0x05C, 0 , 1),
};


static struct mdtk_clk_def clk_def = {
    .gates_def = gates_clk,
    .num_gates = nitems(gates_clk),
    .muxes_def = muxes_clk,
    .num_muxes = nitems(muxes_clk),
};

static int
pericfg_clk_detach(device_t dev)
{
    device_printf(dev, "Error: Clock driver cannot be detached\n");
    return (EBUSY);
}

static int
pericfg_clk_probe(device_t dev)
{
    if (!ofw_bus_status_okay(dev))
        return (ENXIO);

    if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
        device_set_desc(dev, "Mediatek pericfg clocks");
        return (BUS_PROBE_DEFAULT);
    }

    return (ENXIO);
}

static int
pericfg_clk_attach(device_t dev) {
    struct mdtk_clk_softc *sc = device_get_softc(dev);
    int rid = 0;

    sc->dev = dev;

    if (ofw_bus_is_compatible(dev, "syscon")) {
        sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
                                             RF_ACTIVE);
        if (sc->mem_res == NULL) {
            device_printf(dev,
                          "Cannot allocate memory resource\n");
            return (ENXIO);
        }

        mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);
        sc->syscon = syscon_create_ofw_node(dev,
                                            &syscon_class, ofw_bus_get_node(dev));
        if (sc->syscon == NULL) {
            device_printf(dev,
                          "Failed to create/register syscon\n");
            return (ENXIO);
        }
    }

    mdtk_register_clocks(dev, &clk_def);
    return (0);
}

static int
pericfg_clk_hwreset_assert(device_t dev, intptr_t idx, bool value)
{
    struct mdtk_clk_softc *sc = device_get_softc(dev);
    uint32_t mask, reset_reg;

    CLKDEV_DEVICE_LOCK(sc->dev);
    KASSERT((idx > 0 && idx < 32), ("%s: idx out of range",__func__));


    mask = 1 << (idx % 32);
    reset_reg = (idx / 32) * 4;

    CLKDEV_MODIFY_4(sc->dev, reset_reg, mask, value ? mask : 0);
    CLKDEV_DEVICE_UNLOCK(sc->dev);

    return(0);
}

static int
pericfg_clk_syscon_get_handle(device_t dev, struct syscon **syscon)
{
    struct mdtk_clk_softc *sc;

    sc = device_get_softc(dev);
    *syscon = sc->syscon;
    if (*syscon == NULL) {
        return (ENODEV);
    }

    return (0);
}

static void
pericfg_clk_syscon_lock(device_t dev)
{
    struct mdtk_clk_softc *sc;

    sc = device_get_softc(dev);
    mtx_lock(&sc->mtx);
}

static void
pericfg_clk_syscon_unlock(device_t dev)
{
    struct mdtk_clk_softc *sc;

    sc = device_get_softc(dev);
    mtx_unlock(&sc->mtx);
}

static device_method_t mdtk_mt7622_pericfg_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		 pericfg_clk_probe),
    DEVMETHOD(device_attach,	 pericfg_clk_attach),
    DEVMETHOD(device_detach, 	 pericfg_clk_detach),

    /* Clkdev interface*/
    DEVMETHOD(clkdev_read_4,        mdtk_clkdev_read_4),
    DEVMETHOD(clkdev_write_4,	    mdtk_clkdev_write_4),
    DEVMETHOD(clkdev_modify_4,	    mdtk_clkdev_modify_4),
    DEVMETHOD(clkdev_device_lock,	mdtk_clkdev_device_lock),
    DEVMETHOD(clkdev_device_unlock,	mdtk_clkdev_device_unlock),

    DEVMETHOD(hwreset_assert,	pericfg_clk_hwreset_assert),

    /* Syscon interface */
    DEVMETHOD(syscon_get_handle,	pericfg_clk_syscon_get_handle),
    DEVMETHOD(syscon_device_lock,	pericfg_clk_syscon_lock),
    DEVMETHOD(syscon_device_unlock,	pericfg_clk_syscon_unlock),

    DEVMETHOD_END
};

DEFINE_CLASS_1(mdtk_mt7622_pericfg, mdtk_mt7622_pericfg_driver, mdtk_mt7622_pericfg_methods,
               sizeof(struct mdtk_clk_softc), syscon_class);

EARLY_DRIVER_MODULE(mdtk_mt7622_pericfg, simplebus, mdtk_mt7622_pericfg_driver, NULL, NULL,
                    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 3);

