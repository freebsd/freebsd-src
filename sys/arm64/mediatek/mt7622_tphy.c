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
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/clk/clk.h>
#include <dev/phy/phy_usb.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "phynode_if.h"
#include "phynode_usb_if.h"

#define U3P_SPLLC_XTALCTL3		    0x018
#define XC3_RG_U3_XTAL_RX_PWD		(1u << 9)
#define XC3_RG_U3_FRC_XTAL_RX_PWD	(1u << 8)

#define U3P_U3_PHYA_DA_REG0	        0x100
//#define P3A_RG_XTAL_EXT_PE2H        __BITS(17, 16)
#define P3A_RG_XTAL_EXT_PE1H        ((1u << 13) | (1u << 12))
#define P3A_RG_XTAL_EXT_EN_U3       ((1u << 11) | (1u << 10))

#define U3P_U3_PHYD_CDR1		    0x05c
#define P3D_RG_CDR_BIR_LTD1		    ((1u << 28) | (1u << 24))
#define P3D_RG_CDR_BIR_LTD0         ((1u << 12) | (1u << 8))

#define U3P_U3_PHYA_REG9	        0x024
#define P3A_RG_RX_DAC_MUX		    ((1u << 5) | (1u << 2))

#define U3P_U3_PHYD_LFPS1		    0x00c
#define P3D_RG_FWAKE_TH		        ((1u << 21) | (1u << 10))

static struct ofw_compat_data compat_data[] = {
        {"mediatek,mt7622-tphy",     1},
        {"mediatek,generic-tphy-v1", 1},
        {NULL,                       0}
};

struct mt7622_tphy_softc {
    device_t dev;
    struct resource	*mem_res;
};

struct mt_tphy_phynode_sc {
    struct phynode_usb_sc usb_sc;
    struct resource	*mem_res;
    int mode;
    clk_t clk_ref;
    uint32_t base;
};


static int mt_phynode_phy_enable(struct phynode *phy, bool enable);
static int mt_phynode_get_mode(struct phynode *phynode, int *mode);
static int mt_phynode_set_mode(struct phynode *phynode, int mode);

/* Phy controller class and methods. */
static phynode_method_t mt_phynode_methods[] = {
        PHYNODEUSBMETHOD(phynode_enable,	mt_phynode_phy_enable),
        PHYNODEUSBMETHOD(phynode_usb_get_mode,	mt_phynode_get_mode),
        PHYNODEUSBMETHOD(phynode_usb_set_mode,	mt_phynode_set_mode),
        PHYNODEUSBMETHOD_END
};
DEFINE_CLASS_1(mt_phynode, mt_phynode_class, mt_phynode_methods,
sizeof(struct mt_tphy_phynode_sc), phynode_usb_class);

static int
mt_phynode_phy_enable(struct phynode *phy, bool enable)
{
    struct mt_tphy_phynode_sc *sc;
    device_t dev;
    uint32_t tmp;
    dev = phynode_get_device(phy);
    sc = phynode_get_softc(phy);

    device_printf(dev, "Enable phy %d\n", enable);

    tmp = bus_read_4(sc->mem_res, U3P_SPLLC_XTALCTL3);
    tmp |= (XC3_RG_U3_XTAL_RX_PWD | XC3_RG_U3_FRC_XTAL_RX_PWD);
    bus_write_4(sc->mem_res, U3P_SPLLC_XTALCTL3, tmp);

    /*tmp = bus_read_4(sc->mem_res, U3P_U3_PHYA_DA_REG0);
    tmp &= ~0x00000c00;
    tmp |= ((2 << __builtin_ffs(0x00000c00) - 1) & 0x00000c00);
    bus_write_4(sc->mem_res, U3P_U3_PHYA_DA_REG0, tmp);

    tmp = bus_read_4(sc->mem_res, U3P_U3_PHYA_REG9);
    tmp = (tmp & ~P3A_RG_TX_EIDLE_CM) |
          ((0xe << __builtin_ffs(P3A_RG_TX_EIDLE_CM) - 1) & P3A_RG_TX_EIDLE_CM);
    bus_write_4(sc->mem_res, U3P_U3_PHYA_REG9, tmp);*/

    tmp = bus_read_4(sc->mem_res, U3P_U3_PHYD_CDR1);
    tmp &= ~(P3D_RG_CDR_BIR_LTD0 | P3D_RG_CDR_BIR_LTD1);
    tmp |= ((0xc << (__builtin_ffs(P3D_RG_CDR_BIR_LTD0)-1)) & P3D_RG_CDR_BIR_LTD0) |
           ((0x3 << (__builtin_ffs(P3D_RG_CDR_BIR_LTD1)-1)) & P3D_RG_CDR_BIR_LTD1);
    bus_write_4(sc->mem_res, U3P_U3_PHYD_CDR1, tmp);

    tmp = bus_read_4(sc->mem_res, U3P_U3_PHYD_LFPS1);
    tmp = (tmp & ~P3D_RG_FWAKE_TH) |
          ((0x34 << (__builtin_ffs(P3D_RG_FWAKE_TH)-1)) & P3D_RG_FWAKE_TH);
    bus_write_4(sc->mem_res, U3P_U3_PHYD_LFPS1, tmp);


    device_printf(dev, "Mode phy %d\n", sc->mode);

    return (0);
}

static int
mt_phynode_get_mode(struct phynode *phynode, int *mode)
{
    struct mt_tphy_phynode_sc *sc;

    sc = phynode_get_softc(phynode);
    *mode = sc->mode;
    return (0);
}

static int
mt_phynode_set_mode(struct phynode *phynode, int mode)
{
    struct mt_tphy_phynode_sc *sc;

    sc = phynode_get_softc(phynode);
    sc->mode = mode;

    return (0);
}

static int
mt7622_init_tphy(struct mt7622_tphy_softc *sc,
                               phandle_t node)
{
    struct phynode *phynode;
    struct phynode_init_def phy_init;
    struct mt_tphy_phynode_sc *phy_sc;
    intptr_t phy;
    uint32_t base;
    int phy_id = 0;
    char *name = NULL;
    uint64_t freq;
    clk_t clk;

    /* we find out usb phy node */
    for (phandle_t child = OF_child(node); child != 0 && child != -1; child = OF_peer(child)) {
        if (OF_getprop_alloc(child, "name", (void **) &name) > 0) {
            if (strncmp(name, "usb-phy", 7) == 0) {
                device_printf(sc->dev, "USB PHY found %s \n", name);

                /* Create and register phy. */
                bzero(&phy_init, sizeof(phy_init));
                phy_id++;
                phy_init.id = phy_id;
                phy_init.ofw_node = child;
                phynode = phynode_create(sc->dev, &mt_phynode_class, &phy_init);
                if (phynode == NULL) {
                    device_printf(sc->dev, "Cannot create phy.\n");
                    OF_prop_free(name);
                    return (ENXIO);
                }

                if(OF_getencprop(child, "reg", &base, sizeof(base)) <= 0) {
                    device_printf(sc->dev, "Cannot get 'reg' property.\n");
                    OF_prop_free(name);
                    return (ENXIO);
                }

                if(clk_get_by_ofw_name(sc->dev, child, "ref", &clk) == 0) {
                    if (clk) {
                        if (clk_enable(clk) != 0) {
                            device_printf(sc->dev, "Couldn't enable clock %s\n",
                                          clk_get_name(clk));
                            OF_prop_free(name);
                            return (ENXIO);
                        }

                        if (bootverbose) {
                            clk_get_freq(clk, &freq);
                            device_printf(sc->dev,"Tphy clock %s frequency: %lu\n", clk_get_name(clk), freq);
                        }

                    }
                }
                else {
                    device_printf(sc->dev, "Could not find clock %s\n", clk_get_name(clk));
                }

                phy_sc = phynode_get_softc(phynode);
                phy_sc->mem_res = sc->mem_res;
                phy_sc->base = base;
                phy_sc->clk_ref = clk;
                if (phynode_register(phynode) == NULL) {
                    device_printf(sc->dev, "Failed to register USB PHY\n");
                    OF_prop_free(name);
                    if (clk != NULL) {
                        clk_release(clk);
                    }
                    return (ENXIO);
                }
                else {
                    device_printf(sc->dev, "Register USB PHY\n");
                    mt_phynode_phy_enable(phynode, true);
                }


                if (bootverbose) {
                    phy = phynode_get_id(phynode);
                    device_printf(sc->dev, "Attached phy id: %ld\n", phy);
                }
            } else {
                device_printf(sc->dev, "Unknown PHY found %s \n", name);
            }
        }
        OF_prop_free(name);
    }

    return (0);
}

static int
mt7622_tphy_probe(device_t dev)
{
    if (!ofw_bus_status_okay(dev)) {
        return (ENXIO);
    }

    if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0) {
        return (ENXIO);
    }

    device_set_desc(dev, "Meditak T-PHY driver");
    return (BUS_PROBE_DEFAULT);
}

static int
mt7622_tphy_attach(device_t dev)
{
    struct mt7622_tphy_softc *sc;
    phandle_t node;
    int rid, rv;

    sc = device_get_softc(dev);
    sc->dev = dev;
    node = ofw_bus_get_node(sc->dev);

    rid = 0;
    sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
                                         RF_ACTIVE | RF_SHAREABLE);
    if (sc->mem_res == NULL) {
        device_printf(dev, "Cannot allocate memory resources\n");
        return (ENXIO);
    }

    rv = mt7622_init_tphy(sc, node);
    if (rv != 0) {
        return (ENXIO);
    }

    bus_attach_children(dev);
    return (0);
}

static int
mt7622_tphy_detach(device_t dev)
{
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
sizeof(struct mt7622_tphy_softc));
EARLY_DRIVER_MODULE(mt7622_tphy, simplebus, mt7622_tphy_driver, NULL, NULL,
79);
