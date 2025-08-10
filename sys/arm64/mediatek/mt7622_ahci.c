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
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <dev/ahci/ahci.h>
#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/phy/phy.h>
#include <dev/fdt/fdt_pinctrl.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

struct mt7622_ahci_sc {
    struct ahci_controller	ctlr;
    device_t		dev;
    struct ahci_soc		*soc;
    struct resource		*sata_mem;
    clk_t			clk_ahb;
    clk_t			clk_axi;
    clk_t			clk_asic;
    clk_t			clk_rbc;
    clk_t           clk_pm;
    hwreset_t		hwreset_axi;
    hwreset_t		hwreset_sw;
    hwreset_t		hwreset_reg;
    phy_t			phy;
};

static struct ofw_compat_data compat_data[] = {
        {"mediatek,mt7622-ahci", 1},
        {"mediatek,mtk-ahci", 1},
        {NULL,			0}
};

static int
get_fdt_resources(struct mt7622_ahci_sc *sc, phandle_t node)
{
    int rv;

    /* Resets. */
    rv = hwreset_get_by_ofw_name(sc->dev, 0, "axi", &sc->hwreset_sw );
    if (rv != 0) {
        device_printf(sc->dev, "Cannot get 'axi' reset\n");
        return (ENXIO);
    }
    rv = hwreset_get_by_ofw_name(sc->dev, 0, "sw",
                                 &sc->hwreset_sw);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot get 'sw' reset\n");
        return (ENXIO);
    }
    rv = hwreset_get_by_ofw_name(sc->dev, 0, "reg",
                                 &sc->hwreset_reg);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot get 'reg' reset\n");
        return (ENXIO);
    }

    /* Phy */
    rv = phy_get_by_ofw_name(sc->dev, 0, "sata-phy", &sc->phy);
    if (rv != 0) {
        rv = phy_get_by_ofw_idx(sc->dev, 0, 0, &sc->phy);
        if (rv != 0) {
            device_printf(sc->dev, "Cannot get 'sata-phy' phy\n");
            return (ENXIO);
        }
    }

    /* Clocks. */
    rv = clk_get_by_ofw_name(sc->dev, 0, "ahb", &sc->clk_ahb);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot get 'ahb' clock\n");
        return (ENXIO);
    }

    rv = clk_get_by_ofw_name(sc->dev, 0, "axi", &sc->clk_axi);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot get 'axi' clock\n");
        return (ENXIO);
    }

    rv = clk_get_by_ofw_name(sc->dev, 0, "asic", &sc->clk_asic);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot get 'asic' clock\n");
        return (ENXIO);
    }

    rv = clk_get_by_ofw_name(sc->dev, 0, "rbc", &sc->clk_rbc);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot get 'rbc' clock\n");
        return (ENXIO);
    }

    rv = clk_get_by_ofw_name(sc->dev, 0, "pm", &sc->clk_pm);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot get 'pm' clock\n");
        return (ENXIO);
    }
    return (0);
}

static int
enable_fdt_resources(struct mt7622_ahci_sc *sc)
{
    int rv;

    /* Stop clocks */
    clk_stop(sc->clk_ahb);
    clk_stop(sc->clk_axi);
    clk_stop(sc->clk_asic);
    clk_stop(sc->clk_rbc);
    clk_stop(sc->clk_pm);

    rv = hwreset_assert(sc->hwreset_axi);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot assert 'axi' reset\n");
        return (rv);
    }

    rv = hwreset_assert(sc->hwreset_sw);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot assert 'sw' reset\n");
        return (rv);
    }

    rv = hwreset_assert(sc->hwreset_reg);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot assert 'reg' reset\n");
        return (rv);
    }

    rv = clk_enable(sc->clk_ahb);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot enable 'sata ahb' clock\n");
        return (rv);
    }

    rv = clk_enable(sc->clk_axi);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot enable 'sata axi' clock\n");
        return (rv);
    }

    rv = clk_enable(sc->clk_asic);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot enable 'sata asic' clock\n");
        return (rv);
    }

    rv = clk_enable(sc->clk_rbc);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot enable 'sata rhc' clock\n");
        return (rv);
    }

    rv = clk_enable(sc->clk_pm);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot enable 'sata pm' clock\n");
        return (rv);
    }


    rv = hwreset_deassert(sc->hwreset_axi);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot unreset 'sata axi' reset\n");
        return (rv);
    }
    rv = hwreset_deassert(sc->hwreset_sw);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot unreset 'sata sw' reset\n");
        return (rv);
    }
    rv = hwreset_deassert(sc->hwreset_reg);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot unreset 'sata reg' reset\n");
        return (rv);
    }

    rv = phy_enable(sc->phy);
    if (rv != 0) {
        device_printf(sc->dev, "Cannot enable SATA phy\n");
        return (rv);
    }

    return (0);
}

static int
mt7622_ahci_probe(device_t dev)
{

    if (!ofw_bus_status_okay(dev))
        return (ENXIO);

    if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
        return (ENXIO);

    device_set_desc(dev, "Mediatek mt7622 AHCI SATA controller");
    return (BUS_PROBE_DEFAULT);
}

static int
mt7622_ahci_attach(device_t dev)
{
    struct mt7622_ahci_sc *sc;
    struct ahci_controller *ctlr;
    phandle_t node;
    int rv, rid;

    sc = device_get_softc(dev);
    sc->dev = dev;
    ctlr = &sc->ctlr;
    node = ofw_bus_get_node(dev);
    sc->soc = (struct ahci_soc *)ofw_bus_search_compatible(dev,
                                                           compat_data)->ocd_data;

    ctlr->r_rid = 0;
    ctlr->r_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
                                         &ctlr->r_rid, RF_ACTIVE);
    if (ctlr->r_mem == NULL)
        return (ENXIO);

    rid = 1;
    sc->sata_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
                                          &rid, RF_ACTIVE);
    if (sc->sata_mem == NULL) {
        rv = ENXIO;
        goto fail;
    }

    rv = get_fdt_resources(sc, node);
    if (rv != 0) {
        device_printf(sc->dev, "Failed to allocate FDT resource(s)\n");
        goto fail;
    }

    rv = enable_fdt_resources(sc);
    if (rv != 0) {
        device_printf(sc->dev, "Failed to enable FDT resource(s)\n");
        goto fail;
    }
    /*rv = tegra_ahci_ctrl_init(sc);
    if (rv != 0) {
        device_printf(sc->dev, "Failed to initialize controller)\n");
        goto fail;
    }*/

    /* Setup controller defaults. */
    ctlr->msi = 0;
    ctlr->numirqs = 1;
    ctlr->ccc = 0;

    /* Reset controller. */
    /*rv = tegra_ahci_ctlr_reset(dev);
    if (rv != 0)
        goto fail;*/
    rv = ahci_attach(dev);
    device_printf(sc->dev, "return code %d\n", rv);
    return (rv);

    fail:
    if (sc->sata_mem != NULL) {
        bus_release_resource(dev, SYS_RES_MEMORY, 1, sc->sata_mem);
    }
    if (ctlr->r_mem != NULL) {
        bus_release_resource(dev, SYS_RES_MEMORY, ctlr->r_rid,
                             ctlr->r_mem);
    }

    return (rv);
}

static int
mt7622_ahci_detach(device_t dev)
{

    ahci_detach(dev);
    return (0);
}

static int
mt7622_ahci_suspend(device_t dev)
{
    struct mt7622_ahci_sc *sc = device_get_softc(dev);

    bus_generic_suspend(dev);
    /* Disable interupts, so the state change(s) doesn't trigger. */
    ATA_OUTL(sc->ctlr.r_mem, AHCI_GHC,
             ATA_INL(sc->ctlr.r_mem, AHCI_GHC) & (~AHCI_GHC_IE));
    return (0);
}
/*
static int
mt7622_ahci_resume(device_t dev)
{
    int res;

    if ((res = tegra_ahci_ctlr_reset(dev)) != 0)
        return (res);
    ahci_ctlr_setup(dev);
    return (bus_generic_resume(dev));
}*/

static device_method_t mt7622_ahci_methods[] = {
        DEVMETHOD(device_probe,		mt7622_ahci_probe),
        DEVMETHOD(device_attach,	mt7622_ahci_attach),
        DEVMETHOD(device_detach,	mt7622_ahci_detach),
        DEVMETHOD(device_suspend,	mt7622_ahci_suspend),
        //DEVMETHOD(device_resume,	mt7622_ahci_resume),
        DEVMETHOD(bus_print_child,	ahci_print_child),
        DEVMETHOD(bus_alloc_resource,	ahci_alloc_resource),
        DEVMETHOD(bus_release_resource,	ahci_release_resource),
        DEVMETHOD(bus_setup_intr,	ahci_setup_intr),
        DEVMETHOD(bus_teardown_intr,	ahci_teardown_intr),
        DEVMETHOD(bus_child_location,	ahci_child_location),
        DEVMETHOD(bus_get_dma_tag,	ahci_get_dma_tag),

        DEVMETHOD_END
};

static DEFINE_CLASS_0(ahci, mt7622_ahci_driver, mt7622_ahci_methods,
sizeof(struct mt7622_ahci_sc));
DRIVER_MODULE(mt7622_ahci, simplebus, mt7622_ahci_driver, NULL, NULL);





