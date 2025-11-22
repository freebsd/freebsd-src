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
#include "syscon_if.h"

struct mt7622_pciecfg_softc {
    device_t dev;
    struct resource *mem_res;
    struct mtx mtx;
    struct syscon *syscon;
};

static struct ofw_compat_data compat_data[] = {
        {"mediatek,generic-pciecfg", 1},
        {NULL, 0},
};

static int
mt7622_pcicfg_syscon_write_4(struct syscon *syscon, bus_size_t offset, uint32_t val)
{
    struct mt7622_pciecfg_softc *sc;

    sc = device_get_softc(syscon->pdev);
    device_printf(sc->dev, "offset=%lx write %x\n", offset, val);
    mtx_lock(&sc->mtx);
    bus_write_4(sc->mem_res, offset, val);
    mtx_unlock(&sc->mtx);
    return (0);
}

static uint32_t
mt7622_pcicfg_syscon_read_4(struct syscon *syscon, bus_size_t offset)
{
    struct mt7622_pciecfg_softc *sc;
    uint32_t val;

    sc = device_get_softc(syscon->pdev);

    mtx_lock(&sc->mtx);
    val = bus_read_4(sc->mem_res, offset);
    mtx_unlock(&sc->mtx);
    device_printf(sc->dev, "offset=%lx Read %x\n", offset, val);
    return (val);
}

static int
mt7622_pcicfg_syscon_modify_4(struct syscon *syscon, bus_size_t offset, uint32_t clr, uint32_t set)
{
    struct mt7622_pciecfg_softc *sc;
    uint32_t reg;

    sc = device_get_softc(syscon->pdev);

    mtx_lock(&sc->mtx);
    reg = bus_read_4(sc->mem_res, offset);
    reg &= ~clr;
    reg |= set;
    bus_write_4(sc->mem_res, offset, reg);
    mtx_unlock(&sc->mtx);
    device_printf(sc->dev, "offset=%lx reg: %x (clr %x set %x)\n", offset, reg, clr, set);

    return (0);
}

static syscon_method_t mt7622_pcicfg_syscon_reg_methods[] = {
        SYSCONMETHOD(syscon_read_4,	mt7622_pcicfg_syscon_read_4),
        SYSCONMETHOD(syscon_write_4, mt7622_pcicfg_syscon_write_4),
        SYSCONMETHOD(syscon_modify_4, mt7622_pcicfg_syscon_modify_4),

        SYSCONMETHOD_END
};

DEFINE_CLASS_1(mt7622_pcicfg_syscon_reg, mt7622_pcicfg_syscon_reg_class, mt7622_pcicfg_syscon_reg_methods,
0, syscon_class);

static int
mt7622_pciecfg_probe(device_t dev)
{
    if (!ofw_bus_status_okay(dev)) {
        return (ENXIO);
    }

    if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
        device_set_desc(dev, "Mediatek pciecfg driver");
        return (BUS_PROBE_DEFAULT);
    }

    return (ENXIO);
}

static int
mt7622_pciecfg_attach(device_t dev)
{
    struct mt7622_pciecfg_softc *sc = device_get_softc(dev);
    int rid = 0;
    sc->dev = dev;

    mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

    if (ofw_bus_is_compatible(dev, "syscon")) {
        sc->mem_res = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY, &rid,
                               RF_ACTIVE | RF_SHAREABLE);
        if(sc->mem_res == NULL) {
            device_printf(sc->dev, "Cant allocate resources\n");
            return (ENXIO);
        }

        sc->syscon = syscon_create_ofw_node(sc->dev,
                                            &mt7622_pcicfg_syscon_reg_class, ofw_bus_get_node(sc->dev));
        if (sc->syscon == NULL) {
            device_printf(dev,
                          "Failed to create/register syscon\n");
            return (ENXIO);
        }
    }

    return (0);
}

static int
mt7622_pciecfg_detach(device_t dev)
{
    device_printf(dev, "Error: pciecfg driver cannot be detached\n");
    return (EBUSY);
}

static int
mt7622_pciecfg_syscon_get_handle(device_t dev, struct syscon **syscon)
{
    struct mt7622_pciecfg_softc *sc;

    sc = device_get_softc(dev);
    *syscon = sc->syscon;
    if (*syscon == NULL) {
        return (ENODEV);
    }

    return (0);
}

static void
mt7622_pciecfg_syscon_lock(device_t dev)
{
    struct mt7622_pciecfg_softc *sc;

    sc = device_get_softc(dev);
    mtx_lock(&sc->mtx);
}

static void
mt7622_pciecfg_syscon_unlock(device_t dev)
{
    struct mt7622_pciecfg_softc *sc;

    sc = device_get_softc(dev);
    mtx_unlock(&sc->mtx);
}

static device_method_t mt7622_pciecfg_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,		 mt7622_pciecfg_probe),
        DEVMETHOD(device_attach,	 mt7622_pciecfg_attach),
        DEVMETHOD(device_detach, 	 mt7622_pciecfg_detach),

        /* Syscon interface */
        DEVMETHOD(syscon_get_handle,    mt7622_pciecfg_syscon_get_handle),
        DEVMETHOD(syscon_device_lock,   mt7622_pciecfg_syscon_lock),
        DEVMETHOD(syscon_device_unlock, mt7622_pciecfg_syscon_unlock),

        DEVMETHOD_END
};

DEFINE_CLASS_1(mt7622_pciecfg, mt7622_pciecfg_driver, mt7622_pciecfg_methods,
sizeof(struct mt7622_pciecfg_softc), mt7622_pcicfg_syscon_reg_class);
DRIVER_MODULE(mt7622_pciecfg, simplebus, mt7622_pciecfg_driver, NULL, NULL);