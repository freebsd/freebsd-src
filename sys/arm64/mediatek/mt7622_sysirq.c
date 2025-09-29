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
#include <sys/kernel.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/interrupt-controller/irq.h>

#include "pic_if.h"

static struct ofw_compat_data compat_data[] = {
        {"mediatek,mt7622-sysirq", 	1},
        {"mediatek,mt6577-sysirq", 1},
        {NULL,				0}
};

struct mt7622_sysirq_sc {
    device_t		dev;
    device_t		parent;
    int             rid;
    int			    nirq;
    struct resource		*res;
    struct intr_map_data_fdt *parent_map_data;
};

static struct intr_map_data *
mt7622_sysirq_convert_map_data(struct mt7622_sysirq_sc *sc,
                               struct intr_map_data *data)
{
    struct intr_map_data_fdt *daf;
    int irq;

    daf = (struct intr_map_data_fdt *)data;

    /* We only support GIC forward for now */
    if (daf->ncells != 3)
        return (NULL);

    /* Check if this is a GIC_SPI type */
    if (daf->cells[0] != 0)
        return (NULL);

    irq = daf->cells[1];

    if (daf->ncells != 3) {
        device_printf(sc->dev, "sysirq: %s: bad ncells=%d\n",
                      __func__, daf->ncells);
        return (NULL);
    }

    if (daf->cells[0] != 0) { /* 0 == GIC_SPI */
        device_printf(sc->dev, "sysirq: %s: non-SPI parent (%d) not supported\n",
                      __func__, daf->cells[0]);
        return (NULL);
    }

    if (irq < 0 || irq >= sc->nirq) {
        device_printf(sc->dev, "sysirq: %s: irq %d out of range [0..%d)\n",
                      __func__, irq, sc->nirq);
        return (NULL);
    }

    sc->parent_map_data->ncells = 3;
    sc->parent_map_data->cells[0] = 0;
    sc->parent_map_data->cells[1] = daf->cells[1];
    /* sysirq support controllable irq inverter for each GIC SPI interrupt. */
    sc->parent_map_data->cells[2] = IRQ_TYPE_LEVEL_HIGH;

    return ((struct intr_map_data *)sc->parent_map_data);
}

static int
mt7622_sysirq_activate_intr(device_t dev, struct intr_irqsrc *isrc,
                            struct resource *res, struct intr_map_data *data)
{
    struct mt7622_sysirq_sc *sc;

    sc = device_get_softc(dev);
    data = mt7622_sysirq_convert_map_data(sc, data);
    if (data == NULL)
        return (EINVAL);

    return (PIC_ACTIVATE_INTR(sc->parent, isrc, res, data));
}

static void
mt7622_sysirq_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
    struct mt7622_sysirq_sc *sc;
    sc = device_get_softc(dev);
    PIC_ENABLE_INTR(sc->parent, isrc);
}

static void
mt7622_sysirq_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
    struct mt7622_sysirq_sc *sc;
    sc = device_get_softc(dev);
    PIC_DISABLE_INTR(sc->parent, isrc);
}

static int
mt7622_sysirq_map_intr(device_t dev, struct intr_map_data *data,
                       struct intr_irqsrc **isrcp)
{
    struct mt7622_sysirq_sc *sc;
    int ret;

    sc = device_get_softc(dev);

    if (data->type != INTR_MAP_DATA_FDT)
        return (ENOTSUP);

    data = mt7622_sysirq_convert_map_data(sc, data);
    if (data == NULL)
        return (EINVAL);

    ret = PIC_MAP_INTR(sc->parent, data, isrcp);
    (*isrcp)->isrc_dev = sc->dev;
    return(ret);
}

static int
mt7622_sysirq_deactivate_intr(device_t dev, struct intr_irqsrc *isrc,
                              struct resource *res, struct intr_map_data *data)
{
    struct mt7622_sysirq_sc *sc;

    sc = device_get_softc(dev);

    data = mt7622_sysirq_convert_map_data(sc, data);
    if (data == NULL)
        return (EINVAL);

    return (PIC_DEACTIVATE_INTR(sc->parent, isrc, res, data));
}

static int
mt7622_sysirq_setup_intr(device_t dev, struct intr_irqsrc *isrc,
                         struct resource *res, struct intr_map_data *data)
{
    struct mt7622_sysirq_sc *sc;
    struct intr_map_data_fdt *daf;
    uint32_t reg, val;
    int irq, bit;

    sc = device_get_softc(dev);
    data = mt7622_sysirq_convert_map_data(sc, data);
    if (data == NULL)
        return (EINVAL);

    daf = (struct intr_map_data_fdt *)data;
    irq = daf->cells[1];
    reg = (irq / 32) * 4;
    bit = irq % 32;
    val = bus_read_4(sc->res, reg);
    val |= (1U << bit);
    bus_write_4(sc->res, reg, val);

    return (PIC_SETUP_INTR(sc->parent, isrc, res, data));
}

static int
mt7622_sysirq_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
                            struct resource *res, struct intr_map_data *data)
{
    struct mt7622_sysirq_sc *sc;
    struct intr_map_data_fdt *daf;
    uint32_t reg, val;
    int irq, bit;

    sc = device_get_softc(dev);
    data = mt7622_sysirq_convert_map_data(sc, data);
    if (data == NULL)
        return (EINVAL);

    daf = (struct intr_map_data_fdt *)data;
    irq = daf->cells[1];
    reg = (irq / 32) * 4;
    bit = irq % 32;
    val = bus_read_4(sc->res, reg);
    val &= ~(1U << bit);
    bus_write_4(sc->res, reg, val);

    return (PIC_TEARDOWN_INTR(sc->parent, isrc, res, data));
}

static void
mt7622_sysirq_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
    struct mt7622_sysirq_sc *sc;

    sc = device_get_softc(dev);

    PIC_PRE_ITHREAD(sc->parent, isrc);
}

static void
mt7622_sysirq_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
    struct mt7622_sysirq_sc *sc;

    sc = device_get_softc(dev);

    PIC_POST_ITHREAD(sc->parent, isrc);
}

static void
mt7622_sysirq_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
    struct mt7622_sysirq_sc *sc;

    sc = device_get_softc(dev);

    PIC_POST_FILTER(sc->parent, isrc);
}

static int
mt7622_sysirq_probe(device_t dev)
{
    if (!ofw_bus_status_okay(dev))
        return (ENXIO);

    if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
        return (ENXIO);

    device_set_desc(dev, "Mediatek 7622 sysirq controler");

    return (BUS_PROBE_DEFAULT);
}

static int
mt7622_sysirq_attach(device_t dev)
{
    struct mt7622_sysirq_sc *sc;
    phandle_t node;
    phandle_t xref;
    phandle_t intr_parent;
    int rv = 0;

    sc = device_get_softc(dev);
    sc->dev = dev;
    node = ofw_bus_get_node(dev);

    sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid , RF_ACTIVE);
    if (sc->res == NULL) {
        device_printf(dev, "cannot allocate memory resource\n");
        return (ENXIO);
    }

    sc->nirq = rman_get_size(sc->res) * 8;

    rv = OF_getencprop(node, "interrupt-parent", &intr_parent, sizeof(intr_parent));
    if (rv <= 0) {
        device_printf(dev, "Cannot read interrupt-parent\n");
        if (sc->res) {
            bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
        }
        return (ENXIO);
    }

    sc->parent = OF_device_from_xref(intr_parent);
    if (sc->parent == NULL) {
        device_printf(dev, "Cannot find parent controller\n");
        if (sc->res) {
            bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
        }
        return (ENXIO);
    }

    /* Register ourself as a interrupt controller */
    xref = OF_xref_from_node(node);
    if (intr_pic_register(dev, xref) == NULL) {
        device_printf(dev, "Cannot register GIC\n");
        if (sc->res) {
            bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
        }
        return (ENXIO);
    }

    device_printf(dev, "Registered as PIC, nirq %d, parent %s\n",
                  sc->nirq, device_get_nameunit(sc->parent));

    /* Allocate GIC compatible mapping */
    sc->parent_map_data = (struct intr_map_data_fdt *)intr_alloc_map_data(
            INTR_MAP_DATA_FDT, sizeof(struct intr_map_data_fdt) +
                               + 3 * sizeof(phandle_t), M_WAITOK | M_ZERO);

    /* Register ourself to device can find us */
    OF_device_register_xref(xref, dev);

    return (0);
}

static int
mt7622_sysirq_detach(device_t dev)
{
    struct mt7622_sysirq_sc *sc;
    sc = device_get_softc(dev);
    if (sc->res != NULL) {
        bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
    }

    return (EBUSY);
}

static device_method_t mt7622_sysirq_methods[] = {
        DEVMETHOD(device_probe,		mt7622_sysirq_probe),
        DEVMETHOD(device_attach,	mt7622_sysirq_attach),
        DEVMETHOD(device_detach,	mt7622_sysirq_detach),

        /* Interrupt controller interface */
        DEVMETHOD(pic_activate_intr,	mt7622_sysirq_activate_intr),
        DEVMETHOD(pic_disable_intr,	mt7622_sysirq_disable_intr),
        DEVMETHOD(pic_enable_intr,	mt7622_sysirq_enable_intr),
        DEVMETHOD(pic_map_intr,		mt7622_sysirq_map_intr),
        DEVMETHOD(pic_deactivate_intr,	mt7622_sysirq_deactivate_intr),
        DEVMETHOD(pic_setup_intr,	mt7622_sysirq_setup_intr),
        DEVMETHOD(pic_teardown_intr,	mt7622_sysirq_teardown_intr),
        DEVMETHOD(pic_pre_ithread,	mt7622_sysirq_pre_ithread),
        DEVMETHOD(pic_post_ithread,	mt7622_sysirq_post_ithread),
        DEVMETHOD(pic_post_filter,	mt7622_sysirq_post_filter),

        DEVMETHOD_END
};

static DEFINE_CLASS_0(mt7622_sysirq, mt7622_sysirq_driver, mt7622_sysirq_methods,
sizeof(struct mt7622_sysirq_sc));
EARLY_DRIVER_MODULE(mt7622_sysirq, simplebus, mt7622_sysirq_driver, NULL, NULL,
        BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE + 1);