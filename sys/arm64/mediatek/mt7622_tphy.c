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
#include <sys/bitset.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/fdt/fdt_pinctrl.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/phy/phy.h>
#include <dev/phy/phy_usb.h>

#include "phynode_if.h"
#include "phynode_usb_if.h"

#define MT_U3P_SPLLC_XTALCTL3           0x018
#define MT_XC3_RG_U3_XTAL_RX_PWD        (1u << (9))
#define MT_XC3_RG_U3_FRC_XTAL_RX_PWD    (1u << (8))
#define U3P_U3_PHYA_DA_REG0             0x100
#define GEN_BIT_MASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (32 - 1 - (h))))
#define P3A_RG_XTAL_EXT_PE2H            GEN_BIT_MASK(17, 16)
#define P3A_RG_XTAL_EXT_PE1H            GEN_BIT_MASK(13, 12)
#define P3A_RG_XTAL_EXT_EN_U3           GEN_BIT_MASK(11, 10)

struct mt_phynode_sc {
    device_t dev;
    uint32_t base;
    int mode;
};

static int
mt_phynode_phy_enable(struct phynode *phy, bool enable) {
    struct mt_phynode_sc *sc;
    sc = phynode_get_softc(phy);
    device_printf(sc->dev, "GENERIC PHY enabled (off=0x%zx)\n",
                  (size_t) sc->base);
    return (0);
}

static int
mt_phynode_get_mode(struct phynode *phynode, int *mode) {
    struct mt_phynode_sc *sc;

    sc = phynode_get_softc(phynode);
    *mode = sc->mode;
    return (0);
}

static int
mt_phynode_set_mode(struct phynode *phynode, int mode) {
    struct mt_phynode_sc *sc;

    sc = phynode_get_softc(phynode);
    sc->mode = mode;

    return (0);
}

/* Phy controller class and methods. */
static phynode_method_t mt_phynode_methods[] = {
        PHYNODEMETHOD(phynode_enable, mt_phynode_phy_enable),
        PHYNODEMETHOD_END
};
DEFINE_CLASS_1(mt_phynode, mt_phynode_class, mt_phynode_methods,
sizeof(struct mt_phynode_sc), phynode_class);


struct mt_phynode_usb_sc {
    device_t dev;
    uint32_t base;
    int mode;
};

static int
mt_phynode_phy_usb_enable(struct phynode *phy, bool enable) {
    struct mt_phynode_usb_sc *sc;
    sc = phynode_get_softc(phy);
    device_printf(sc->dev, "USB PHY enabled (off=0x%zx)\n",
                  (size_t) sc->base);
    return (0);
}

static int
mt_phynode_get_usb_mode(struct phynode *phynode, int *mode) {
    struct mt_phynode_usb_sc *sc;

    sc = phynode_get_softc(phynode);
    *mode = sc->mode;
    return (0);
}

static int
mt_phynode_set_usb_mode(struct phynode *phynode, int mode) {
    struct mt_phynode_usb_sc *sc;

    sc = phynode_get_softc(phynode);
    sc->mode = mode;

    return (0);
}

/* Phy controller class and methods. */
static phynode_method_t mt_phynode_usb_methods[] = {
        PHYNODEUSBMETHOD(phynode_enable, mt_phynode_phy_usb_enable),
        PHYNODEMETHOD(phynode_usb_get_mode, mt_phynode_get_usb_mode),
        PHYNODEMETHOD(phynode_usb_set_mode, mt_phynode_set_usb_mode),
        PHYNODEUSBMETHOD_END
};
DEFINE_CLASS_1(mt_phynode_usb, mt_phynode_class_usb, mt_phynode_usb_methods,
sizeof(struct mt_phynode_usb_sc), phynode_usb_class);

struct mt7622_tphy_sc {
    device_t dev;
    struct resource *res_mem;
    int rid;
    clk_t clk;
    hwreset_t rst;
};

static struct ofw_compat_data compat_data_phy[] = {
        {"mediatek,mt7622-tphy",     1},
        {"mediatek,generic-tphy-v1", 1},
        {NULL,                       0}
};

static int
mt7622_tphy_probe(device_t dev) {
    if (!ofw_bus_search_compatible(dev, compat_data_phy)->ocd_data) {
        return (ENXIO);
    }

    if (!ofw_bus_status_okay(dev)) {
        return (ENXIO);
    }

    device_set_desc(dev, "Mediatek mt7622 T-Phy");
    return (BUS_PROBE_DEFAULT);
}

static int
mt7622_tphy_attach(device_t dev) {
    struct phynode *phynode;
    struct phynode_init_def phy_init;
    struct mt7622_tphy_sc *sc;

    phandle_t node;
    intptr_t phy;
    char *name = NULL;
    int phy_id = 0;

    sc = device_get_softc(dev);
    node = ofw_bus_get_node(dev);
    sc->dev = dev;
    sc->rid = 0;

    sc->res_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
                                         RF_ACTIVE);
    if (sc->res_mem == NULL) {
        device_printf(dev, "Cannot allocate resource\n");
        return (ENXIO);
    }

    uint32_t val = bus_read_4(sc->res_mem, MT_U3P_SPLLC_XTALCTL3);
    val |= (MT_XC3_RG_U3_XTAL_RX_PWD | MT_XC3_RG_U3_FRC_XTAL_RX_PWD);
    bus_write_4(sc->res_mem, MT_U3P_SPLLC_XTALCTL3, val);

    device_printf(sc->dev, "Test read register 0x%x\n", bus_read_4(sc->res_mem, MT_U3P_SPLLC_XTALCTL3));
    device_printf(sc->dev, "Test read val %d\n", val);

    val = bus_read_4(sc->res_mem, U3P_U3_PHYA_DA_REG0);
    val &=  ~P3A_RG_XTAL_EXT_EN_U3;
    bus_write_4(sc->res_mem, U3P_U3_PHYA_DA_REG0, val);

    device_printf(sc->dev, "Test read register 0x%x\n", bus_read_4(sc->res_mem, U3P_U3_PHYA_DA_REG0));
    device_printf(sc->dev, "Test read val %d\n", val);




    for (phandle_t child = OF_child(node); child != 0 && child != -1; child = OF_peer(child)) {
        if (OF_getprop_alloc(child, "name", (void **) &name) > 0) {
            if (strncmp(name, "usb-phy", 7) == 0) {
                device_printf(sc->dev, "USB PHY found %s \n", name);

                /* Create and register phy. */
                bzero(&phy_init, sizeof(phy_init));
                phy_id++;
                phy_init.id = phy_id;
                phy_init.ofw_node = node;
                phynode = phynode_create(sc->dev, &mt_phynode_class_usb, &phy_init);
                if (phynode == NULL) {
                    device_printf(sc->dev, "Cannot create phy.\n");
                    goto fail;
                }

                if (phynode_register(phynode) == NULL) {
                    device_printf(dev, "failed to register USB PHY\n");
                    goto fail;
                }

                if (bootverbose) {
                    phy = phynode_get_id(phynode);
                    device_printf(dev, "Attached phy id: %ld\n", phy);
                }
            } else {
                device_printf(sc->dev, "Unknown PHY found %s \n", name);
            }
        }
        OF_prop_free(name);
    }

    bus_attach_children(dev);
    return (0);

    fail:
    if (sc->res_mem) {
        bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res_mem);
    }

    if (name != NULL) {
        OF_prop_free(name);
    }

    return (ENXIO);
}

static int
mt7622_tphy_detach(device_t dev) {
    struct mt7622_tphy_sc *sc = device_get_softc(dev);
    if (sc->res_mem) {
        bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res_mem);
    }

    return (0);
}

static device_method_t mt7622_tphy_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe, mt7622_tphy_probe),
        DEVMETHOD(device_attach, mt7622_tphy_attach),
        DEVMETHOD(device_detach, mt7622_tphy_detach),
        DEVMETHOD_END
};

static DEFINE_CLASS_0(mt7622_tphy, mt7622_tphy_driver, mt7622_tphy_methods,
sizeof(struct mt7622_tphy_sc));
DRIVER_MODULE(mt7622_tphy, simplebus, mt7622_tphy_driver, NULL, NULL);
MODULE_DEPEND(mt7622_tphy, ofwbus,1, 1, 1);
