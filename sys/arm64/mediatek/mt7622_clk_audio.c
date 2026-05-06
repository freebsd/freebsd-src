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
#include <dev/syscon/syscon.h>
#include <dt-bindings/clock/mt7622-clk.h>
#include <dev/clk/clk_gate.h>
#include <dev/hwreset/hwreset.h>
#include "syscon_if.h"
#include "clkdev_if.h"
#include "hwreset_if.h"
#include "mdtk_clk.h"


static struct ofw_compat_data compat_data[] = {
        {"mediatek,mt7622-audsys", 1},
        {NULL, 0},
};

static struct clk_gate_def gates_clk[] = {
        /* AUDIO0 */
        GATE(CLK_AUDIO_AFE, "audio_afe", "rtc", 0x0, 2),
        GATE(CLK_AUDIO_HDMI, "audio_hdmi", "apll1_ck_sel", 0x0, 20),
        GATE(CLK_AUDIO_SPDF, "audio_spdf", "apll1_ck_sel", 0x0, 21),
        GATE(CLK_AUDIO_APLL, "audio_apll", "apll1_ck_sel", 0x0, 23),
        /* AUDIO1 */
        GATE(CLK_AUDIO_I2SIN1, "audio_i2sin1", "a1sys_hp_sel", 0x10, 0),
        GATE(CLK_AUDIO_I2SIN2, "audio_i2sin2", "a1sys_hp_sel", 0x10, 1),
        GATE(CLK_AUDIO_I2SIN3, "audio_i2sin3", "a1sys_hp_sel", 0x10, 2),
        GATE(CLK_AUDIO_I2SIN4, "audio_i2sin4", "a1sys_hp_sel", 0x10, 3),
        GATE(CLK_AUDIO_I2SO1, "audio_i2so1", "a1sys_hp_sel", 0x10, 6),
        GATE(CLK_AUDIO_I2SO2, "audio_i2so2", "a1sys_hp_sel", 0x10, 7),
        GATE(CLK_AUDIO_I2SO3, "audio_i2so3", "a1sys_hp_sel", 0x10, 8),
        GATE(CLK_AUDIO_I2SO4, "audio_i2so4", "a1sys_hp_sel", 0x10, 9),
        GATE(CLK_AUDIO_ASRCI1, "audio_asrci1", "asm_h_sel", 0x10, 12),
        GATE(CLK_AUDIO_ASRCI2, "audio_asrci2", "asm_h_sel", 0x10, 13),
        GATE(CLK_AUDIO_ASRCO1, "audio_asrco1", "asm_h_sel", 0x10, 14),
        GATE(CLK_AUDIO_ASRCO2, "audio_asrco2", "asm_h_sel", 0x10, 15),
        GATE(CLK_AUDIO_INTDIR, "audio_intdir", "intdir_sel", 0x10, 20),
        GATE(CLK_AUDIO_A1SYS, "audio_a1sys", "a1sys_hp_sel", 0x10, 21),
        GATE(CLK_AUDIO_A2SYS, "audio_a2sys", "a2sys_hp_sel", 0x10, 22),
        GATE(CLK_AUDIO_AFE_CONN, "audio_afe_conn", "a1sys_hp_sel", 0x10, 23),
        /* AUDIO2 */
        GATE(CLK_AUDIO_UL1, "audio_ul1", "a1sys_hp_sel", 0x14, 0),
        GATE(CLK_AUDIO_UL2, "audio_ul2", "a1sys_hp_sel", 0x14, 1),
        GATE(CLK_AUDIO_UL3, "audio_ul3", "a1sys_hp_sel", 0x14, 2),
        GATE(CLK_AUDIO_UL4, "audio_ul4", "a1sys_hp_sel", 0x14, 3),
        GATE(CLK_AUDIO_UL5, "audio_ul5", "a1sys_hp_sel", 0x14, 4),
        GATE(CLK_AUDIO_UL6, "audio_ul6", "a1sys_hp_sel", 0x14, 5),
        GATE(CLK_AUDIO_DL1, "audio_dl1", "a1sys_hp_sel", 0x14, 6),
        GATE(CLK_AUDIO_DL2, "audio_dl2", "a1sys_hp_sel", 0x14, 7),
        GATE(CLK_AUDIO_DL3, "audio_dl3", "a1sys_hp_sel", 0x14, 8),
        GATE(CLK_AUDIO_DL4, "audio_dl4", "a1sys_hp_sel", 0x14, 9),
        GATE(CLK_AUDIO_DL5, "audio_dl5", "a1sys_hp_sel", 0x14, 10),
        GATE(CLK_AUDIO_DL6, "audio_dl6", "a1sys_hp_sel", 0x14, 11),
        GATE(CLK_AUDIO_DLMCH, "audio_dlmch", "a1sys_hp_sel", 0x14, 12),
        GATE(CLK_AUDIO_ARB1, "audio_arb1", "a1sys_hp_sel", 0x14, 13),
        GATE(CLK_AUDIO_AWB, "audio_awb", "a1sys_hp_sel", 0x14, 14),
        GATE(CLK_AUDIO_AWB2, "audio_awb2", "a1sys_hp_sel", 0x14, 15),
        GATE(CLK_AUDIO_DAI, "audio_dai", "a1sys_hp_sel", 0x14, 16),
        GATE(CLK_AUDIO_MOD, "audio_mod", "a1sys_hp_sel", 0x14, 17),
        /* AUDIO3 */
        GATE(CLK_AUDIO_ASRCI3, "audio_asrci3", "asm_h_sel", 0x634, 2),
        GATE(CLK_AUDIO_ASRCI4, "audio_asrci4", "asm_h_sel", 0x634, 3),
        GATE(CLK_AUDIO_ASRCO3, "audio_asrco3", "asm_h_sel", 0x634, 6),
        GATE(CLK_AUDIO_ASRCO4, "audio_asrco4", "asm_h_sel", 0x634, 7),
        GATE(CLK_AUDIO_MEM_ASRC1, "audio_mem_asrc1", "asm_h_sel", 0x634, 10),
        GATE(CLK_AUDIO_MEM_ASRC2, "audio_mem_asrc2", "asm_h_sel", 0x634, 11),
        GATE(CLK_AUDIO_MEM_ASRC3, "audio_mem_asrc3", "asm_h_sel", 0x634, 12),
        GATE(CLK_AUDIO_MEM_ASRC4, "audio_mem_asrc4", "asm_h_sel", 0x634, 13),
        GATE(CLK_AUDIO_MEM_ASRC5, "audio_mem_asrc5", "asm_h_sel", 0x634, 14),
};

static struct mdtk_clk_def clk_def = {
        .gates_def = gates_clk,
        .num_gates = nitems(gates_clk),
};

static int
audio_clk_detach(device_t dev)
{
    device_printf(dev, "Error: Clock driver cannot be detached\n");
    return (EBUSY);
}

static int
audio_clk_probe(device_t dev)
{
    if (!ofw_bus_status_okay(dev))
        return (ENXIO);

    if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
        device_set_desc(dev, "Mediatek mt7622 audio clocks");
        return (BUS_PROBE_DEFAULT);
    }

    return (ENXIO);
}

static int
audio_clk_attach(device_t dev) {
    struct mdtk_clk_softc *sc = device_get_softc(dev);
    int rid = 0;

    sc->dev = dev;

    mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

    if (ofw_bus_is_compatible(dev, "syscon")) {
        sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
                                             RF_ACTIVE);
        if (sc->mem_res == NULL) {
            device_printf(dev,
                          "Cannot allocate memory resource\n");
            return (ENXIO);
        }

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
audio_clk_hwreset_assert(device_t dev, intptr_t idx, bool value)
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
audio_clk_syscon_get_handle(device_t dev, struct syscon **syscon)
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
audio_clk_syscon_lock(device_t dev)
{
    struct mdtk_clk_softc *sc;

    sc = device_get_softc(dev);
    mtx_lock(&sc->mtx);
}

static void
audio_clk_syscon_unlock(device_t dev)
{
    struct mdtk_clk_softc *sc;

    sc = device_get_softc(dev);
    mtx_unlock(&sc->mtx);
}

static device_method_t mt7622_audio_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,		 audio_clk_probe),
        DEVMETHOD(device_attach,	 audio_clk_attach),
        DEVMETHOD(device_detach, 	 audio_clk_detach),

        /* Clkdev interface*/
        DEVMETHOD(clkdev_read_4,        mdtk_clkdev_read_4),
        DEVMETHOD(clkdev_write_4,	    mdtk_clkdev_write_4),
        DEVMETHOD(clkdev_modify_4,	    mdtk_clkdev_modify_4),
        DEVMETHOD(clkdev_device_lock,	mdtk_clkdev_device_lock),
        DEVMETHOD(clkdev_device_unlock,	mdtk_clkdev_device_unlock),

        DEVMETHOD(hwreset_assert,	audio_clk_hwreset_assert),

        /* Syscon interface */
        DEVMETHOD(syscon_get_handle,    audio_clk_syscon_get_handle),
        DEVMETHOD(syscon_device_lock,   audio_clk_syscon_lock),
        DEVMETHOD(syscon_device_unlock, audio_clk_syscon_unlock),

        DEVMETHOD_END
};

DEFINE_CLASS_1(mt7622_audio, mt7622_audio_driver, mt7622_audio_methods,
sizeof(struct mdtk_clk_softc), syscon_class);

EARLY_DRIVER_MODULE(mt7622_audio, simplebus, mt7622_audio_driver, NULL, NULL,
        BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 4);