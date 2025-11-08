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
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/clk/clk.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofwpci.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/syscon/syscon.h>

#include "pcib_if.h"
#include "syscon_if.h"

#define MASK(n, m)  ((((1U) << ((n) - (m) + 1)) - 1) << (m))

#define PCIE_SYS_CFG_V2          0x000
#define PCIE_CSR_LTSSM_EN(p)     (1U << (0 + (p) * 8))
#define PCIE_CSR_ASPM_L1_EN(p)   (1U << (1 + (p) * 8))

#define PCIE_RST_CTRL            0x510
#define PCIE_PHY_RSTB            (1U << 0)
#define PCIE_PIPE_SRSTB          (1U << 1)
#define PCIE_MAC_SRSTB           (1U << 2)
#define PCIE_CRSTB               (1U << 3)
#define PCIE_PERSTB              (1U << 8)
#define PCIE_LINKDOWN_RST_EN     MASK(15, 13)
#define PCIE_LINK_STATUS_V2      0x804
#define PCIE_PORT_LINKUP_V2      (1U << 10)

#define PCIE_CONF_VEND_ID	    0x100
#define PCIE_CONF_DEVICE_ID	    0x102
#define PCIE_CONF_CLASS_ID	    0x106
#define PCI_VENDOR_ID_MEDIATEK		0x14c3
#define PCI_DEVICE_ID_MT7622        0x5396
#define PCI_CLASS_BRIDGE_PCI		0x0604

#define PCIE_INT_MASK		0x420
#define INTX_MASK    ((0xF << 16))
#define INTX_SHIFT		16
#define PCIE_INT_STATUS		0x424

// enable 0 - 4 bits
#define AHB2PCIE_SIZE(x)   ((uint32_t)((x) & 0x1F))
#define PCIE_AHB_TRANS_BASE0_L	0x438
#define PCIE_AHB_TRANS_BASE0_H	0x43c
#define PCIE_AXI_WINDOW0	0x448
#define WIN_ENABLE    (1U << 7)
#define PCIE2AHB_SIZE	0x21

/* CFG_HEADER_0 fields (DW0) */
#define	CFG_DW0_LENGTH(length)	((length) & MASK(9, 0))
#define CFG_DW0_TYPE(type)	    (((type) << 24) & MASK(28, 24))
#define	CFG_DW0_FMT(fmt)		(((fmt) << 29) & MASK(31, 29))
#define CFG_HEADER_TLP_DW0(type, fmt) \
(CFG_DW0_LENGTH(1) | CFG_DW0_TYPE(type) | CFG_DW0_FMT(fmt))

/* CFG_HEADER_1 fields (DW1) */
#define CFG_HEADER_TLP_DW1(where, size) \
    (MASK(((size) - 1), 0) << ((where) & 0x3))

/* CFG_HEADER_2 fields (DW1) */
#define CFG_DW2_REGN(regn)	((regn) & MASK(11, 2))
#define CFG_DW2_FUN(fun)	(((fun) << 16) & MASK(18, 16))
#define CFG_DW2_DEV(dev)	(((dev) << 19) & MASK(23, 19))
#define CFG_DW2_BUS(bus)	(((bus) << 24) & MASK(31, 24))

#define CFG_HEADER_TLP_DW2(regn, fun, dev, bus) \
    (CFG_DW2_REGN(regn) | CFG_DW2_FUN(fun) | \
     CFG_DW2_DEV(dev) | CFG_DW2_BUS(bus))

#define PCIE_CFG_HEADER0	0x460
#define PCIE_CFG_HEADER1	0x464
#define PCIE_CFG_HEADER2	0x468

#define PCIE_CFG_WDATA		0x470
#define PCIE_APP_TLP_REQ	0x488
#define PCIE_CFG_RDATA		0x48c
#define APP_CFG_REQ         (1U << 0)
#define APP_CPL_STATUS      ((0x7U << 5))

#define CFG_WRRD_TYPE_0		4
#define CFG_WR_FMT		2
#define CFG_RD_FMT		0

#define UPPER_32_BITS(x) ((uint32_t)(((x) >> 16) >> 16))

struct mt7622_pcie_softc {
    struct ofw_pci_softc ofw_pci;
    device_t dev;
    struct resource *res_mem;
    int rid;
    struct resource *pcie_irq_res;
    int irq_rid;
    void *pcie_irq_cookie;
    phandle_t node;
    clk_t sys_ck0, ahb_ck0, aux_ck0, axi_ck0, obff_ck0, pipe_ck0;
    struct ofw_pci_range pref_mem_range;
    struct ofw_pci_range io_range;
    struct ofw_pci_range mem_range;
    int	num_mem_ranges;
    struct syscon *syscon;
};

static struct ofw_compat_data compat_data[] = {
        {"mediatek,mt7622-pcie", 1},
        {NULL,                   0}
};

static int
mt7622_pcib_maxslots(device_t dev) {
    return (1);   /* slot 0 */
}

static int
mt7622_pcie_sys_irq(void *arg) {
    return (FILTER_HANDLED);
}

static int
mt7622_pcib_route_interrupt(device_t bus, device_t dev, int pin)
{
    struct mt7622_pcie_softc *sc;
    u_int irq;

    sc = device_get_softc(bus);
    irq = intr_map_clone_irq(rman_get_start(sc->pcie_irq_res));
    device_printf(bus, "route pin %d for device %d.%d to %u\n",
                  pin, pci_get_slot(dev), pci_get_function(dev),
                  irq);

    return (irq);
}

static int
mt7622_pcie_port_start(device_t dev, int port)
{
    struct mt7622_pcie_softc *sc = device_get_softc(dev);
    int timeout;
    uint32_t val;

    /* Assert all reset signals */
    bus_write_4(sc->res_mem, PCIE_RST_CTRL, 0x0);

    /* Enable PCIe link down reset */
    bus_write_4(sc->res_mem, PCIE_RST_CTRL, PCIE_LINKDOWN_RST_EN);

    /*Described in PCIe CEM specification sections 2.2 (PERST# Signal) and
    * 2.2.1 (Initial Power-Up (G3 to S0))*/
    DELAY(100000);

    /* De-assert PHY, PE, PIPE, MAC and configuration reset	*/
    val = bus_read_4(sc->res_mem, PCIE_RST_CTRL);
    val |= PCIE_PHY_RSTB | PCIE_PERSTB | PCIE_PIPE_SRSTB |
           PCIE_MAC_SRSTB | PCIE_CRSTB;
    bus_write_4(sc->res_mem, PCIE_RST_CTRL, val);
    DELAY(100000); /* 100 ms after PERST */

    /* set right vendor id and device id */
    val = PCI_VENDOR_ID_MEDIATEK | (PCI_DEVICE_ID_MT7622 << 16);
    bus_write_4(sc->res_mem, PCIE_CONF_VEND_ID, val);
    bus_write_2(sc->res_mem, PCIE_CONF_CLASS_ID, PCI_CLASS_BRIDGE_PCI);

    /* Wait for link up/stable */
    timeout = 100000;
    while (timeout-- > 0) {
        val = bus_read_4(sc->res_mem, PCIE_LINK_STATUS_V2);
        if (val & PCIE_PORT_LINKUP_V2) {
            device_printf(sc->dev, "PCIe port %d: link is UP\n", port);
            break;
        }
        DELAY(100);
    }

    if (timeout <= 0) {
        device_printf(sc->dev, "PCIe port%d: link-up timeout (status=0x%08x)\n",
                      port, val);
        return (ETIMEDOUT);
    }

    /* Set INTx mask */
    val = bus_read_4(sc->res_mem, PCIE_INT_MASK);
    val &= ~INTX_MASK;
    bus_write_4(sc->res_mem, PCIE_INT_MASK, val);

    bus_addr_t base = sc->mem_range.pci;
    bus_size_t size = sc->mem_range.size;

    val = (uint32_t)(base & 0xffffffff);
    val |= AHB2PCIE_SIZE(fls(size) - 1);
    bus_write_4(sc->res_mem, PCIE_AHB_TRANS_BASE0_L, val);

    val = UPPER_32_BITS(base);
    bus_write_4(sc->res_mem, PCIE_AHB_TRANS_BASE0_H, val);

    /* Set PCIe to AXI translation memory space.*/
    val = PCIE2AHB_SIZE | WIN_ENABLE;
    bus_write_4(sc->res_mem, PCIE_AXI_WINDOW0, val);

    return (0);
}

static int
mt7622_pcie_decode_ranges(struct mt7622_pcie_softc *sc, struct ofw_pci_range *ranges, int nranges)
{
    int i;
    for (i = 0; i < nranges; i++) {
        switch (ranges[i].pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) {
            case OFW_PCI_PHYS_HI_SPACE_IO:
                if (sc->io_range.size != 0) {
                    device_printf(sc->dev,
                                  "Duplicated IO range found in DT\n");
                    return (ENXIO);
                }
                sc->io_range = ranges[i];
                break;
            case OFW_PCI_PHYS_HI_SPACE_MEM32:
            case OFW_PCI_PHYS_HI_SPACE_MEM64:
                if (ranges[i].pci_hi & OFW_PCI_PHYS_HI_PREFETCHABLE) {
                    if (sc->pref_mem_range.size != 0) {
                        device_printf(sc->dev,
                                      "Duplicated memory range found "
                                      "in DT\n");
                        return (ENXIO);
                    }
                    sc->pref_mem_range = ranges[i];
                } else {
                    if (sc->mem_range.size != 0) {
                        device_printf(sc->dev,
                                      "Duplicated memory range found "
                                      "in DT\n");
                        return (ENXIO);
                    }
                    sc->mem_range = ranges[i];
                }
                break;
            default:
                device_printf(sc->dev,
                              "Unknown PCI space type in range #%d: 0x%08x\n",
                              i, ranges[i].pci_hi);
                break;
        }
    }

    if (sc->mem_range.size == 0) {
        device_printf(sc->dev,
                      "At least one memory range must be defined in DT for MT7622\n");
        return (ENXIO);
    }

    return (0);
}

static uint32_t
mt7622_pcib_read_config(device_t dev, u_int bus, u_int slot, u_int func,
                        u_int reg, int bytes)
{
    struct mt7622_pcie_softc *sc = device_get_softc(dev);
    uint32_t val, timeout;

    /* Write PCIe configuration transaction header for read */
    bus_write_4(sc->res_mem, PCIE_CFG_HEADER0,
                CFG_HEADER_TLP_DW0(CFG_WRRD_TYPE_0, CFG_RD_FMT));
    bus_write_4(sc->res_mem, PCIE_CFG_HEADER1, CFG_HEADER_TLP_DW1(reg, bytes));
    bus_write_4(sc->res_mem, PCIE_CFG_HEADER2, CFG_HEADER_TLP_DW2(reg, func, slot, bus));

    /* Trigger hardware to transmit Cfgrd TLP */
    val = bus_read_4(sc->res_mem, PCIE_APP_TLP_REQ);
    val |= APP_CFG_REQ;
    bus_write_4(sc->res_mem, PCIE_APP_TLP_REQ, val);

    timeout = 10000;
    while (timeout-- > 0) {
        val = bus_read_4(sc->res_mem, PCIE_APP_TLP_REQ);
        if ((val & APP_CFG_REQ) == 0) {
            break;
        }
        DELAY(100);

        if (val & APP_CPL_STATUS) {
            device_printf(sc->dev,
                          "PCIe cfg read completion error: APP_CPL_STATUS set\n");
            return (~0U);
        }
    }

    /* Read payload of config read */
    val = bus_read_4(sc->res_mem, PCIE_CFG_RDATA);
    switch (bytes) {
        case 1:
            val = (val >> (8 * (reg & 3))) & 0xff;
            break;
        case 2:
            val = (val >> (8 * (reg & 3))) & 0xffff;
            break;
        default:
            return (~0U);
            break;
    }

    return (val);
}

static void
mt7622_pcib_write_config(device_t dev, u_int bus, u_int slot, u_int func,
                         u_int reg, uint32_t val, int bytes)
{
    struct mt7622_pcie_softc *sc = device_get_softc(dev);

    bus_write_4(sc->res_mem, PCIE_CFG_HEADER0,
                CFG_HEADER_TLP_DW0(CFG_WRRD_TYPE_0, CFG_WR_FMT));
    bus_write_4(sc->res_mem, PCIE_CFG_HEADER1,
                CFG_HEADER_TLP_DW1(reg, bytes));
    bus_write_4(sc->res_mem, PCIE_CFG_HEADER2, CFG_HEADER_TLP_DW2(reg, func, slot, bus));

    // /* Write Cfgwr data */
    val = val << 8 * (reg & 3);
    bus_write_4(sc->res_mem, PCIE_CFG_WDATA, val);

    // /* Trigger h/w to transmit Cfgwr TLP */
    val = bus_read_4(sc->res_mem, PCIE_APP_TLP_REQ);
    val |= APP_CFG_REQ;
    bus_write_4(sc->res_mem, PCIE_APP_TLP_REQ, val);
}

static void
mt7622_pcie_port_enable(struct mt7622_pcie_softc *sc, int port)
{
    uint32_t val;
    val = SYSCON_READ_4(sc->syscon, PCIE_SYS_CFG_V2);
    val |= PCIE_CSR_LTSSM_EN(port);
    val |= PCIE_CSR_ASPM_L1_EN(port);
    SYSCON_WRITE_4(sc->syscon, PCIE_SYS_CFG_V2, val);
    device_printf(sc->dev,
                  "enabled LTSSM+ASPM L1 for port %u (0x%08x)\n",
                  port, val);
}

static int
mt7622_pcie_detach(device_t dev)
{
    struct mt7622_pcie_softc *sc = device_get_softc(dev);

    if (sc->sys_ck0) {
        clk_disable(sc->sys_ck0);
        clk_release(sc->sys_ck0);
    }

    if (sc->ahb_ck0) {
        clk_disable(sc->ahb_ck0);
        clk_release(sc->ahb_ck0);
    }

    if (sc->aux_ck0) {
        clk_disable(sc->aux_ck0);
        clk_release(sc->aux_ck0);
    }

    if (sc->axi_ck0) {
        clk_disable(sc->axi_ck0);
        clk_release(sc->axi_ck0);
    }

    if (sc->obff_ck0) {
        clk_disable(sc->obff_ck0);
        clk_release(sc->obff_ck0);
    }

    if (sc->pcie_irq_cookie != NULL) {
        bus_teardown_intr(dev, sc->pcie_irq_res, sc->pcie_irq_cookie);
        sc->pcie_irq_cookie = NULL;
    }

    if (sc->res_mem) {
        bus_release_resource(dev, SYS_RES_MEMORY, sc->rid,
                             sc->res_mem);
    }

    if (sc->pcie_irq_res) {
        bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid,
                             sc->pcie_irq_res);
    }

    ofw_pcib_fini(sc->dev);

    return (0);
}

static int
mt7622_pcie_attach(device_t dev) {
    struct mt7622_pcie_softc *sc = device_get_softc(dev);
    int error = 0;
    int port = 0;
    phandle_t nodecfg, root;

    sc->dev = dev;
    sc->node = ofw_bus_get_node(dev);

    root = OF_finddevice("/");
    if (root == -1) {
        device_printf(sc->dev, "No FDT root\n");
        return (ENXIO);
    }

    nodecfg = ofw_bus_find_compatible(root, "mediatek,generic-pciecfg");
    if (nodecfg == 0) {
        device_printf(sc->dev,
                      "Cannot mediatek,generic-pciecfg syscon node found\n");
        return (ENXIO);
    }

    error = syscon_get_by_ofw_node(sc->dev, nodecfg, &sc->syscon);
    if (error != 0) {
        device_printf(sc->dev,
                      "Cannot get syscon handle for pciecfg: %d\n", error);
        return (error);
    }

    if (ofw_bus_find_string_index(sc->node, "reg-names",
                                  "port0", &sc->rid) == 0) {
        port = 0;

        sc->res_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
                                             RF_ACTIVE);
        if (sc->res_mem == NULL) {
            device_printf(dev, "Cannot allocate resource\n");
            return (ENXIO);
        }
    }

    if (ofw_bus_find_string_index(sc->node, "reg-names",
                                       "port1", &sc->rid) == 0) {
        port = 1;

        sc->res_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
                                             RF_ACTIVE);
        if (sc->res_mem == NULL) {
            device_printf(dev, "Cannot allocate resource\n");
            return (ENXIO);
        }
    }

    if (port == 0) {
        if (clk_get_by_ofw_name(dev, 0, "sys_ck0", &sc->sys_ck0)) {
            device_printf(dev, "Can not get sys_ck0 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->sys_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable sys_ck0 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "ahb_ck0", &sc->ahb_ck0)) {
            device_printf(dev, "Can not get ahb_ck0 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->ahb_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable ahb_ck0 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "aux_ck0", &sc->aux_ck0)) {
            device_printf(dev, "Can not get aux_ck0 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->aux_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable aux_ck0 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "axi_ck0", &sc->axi_ck0)) {
            device_printf(dev, "Can not get axi_ck0 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->axi_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable axi_ck0 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "obff_ck0", &sc->obff_ck0)) {
            device_printf(dev, "Can not get obff_ck0 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->obff_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable obff_ck0 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "pipe_ck0", &sc->pipe_ck0)) {
            device_printf(dev, "Can not get pipe_ck0 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->pipe_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable pipe_ck0 clock\n");
            return (ENXIO);
        }

        mt7622_pcie_port_enable(sc, port);
    }
    else if (port == 1) {
        if (clk_get_by_ofw_name(dev, 0, "sys_ck1", &sc->sys_ck0)) {
            device_printf(dev, "Can not get sys_ck1 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->sys_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable sys_ck1 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "ahb_ck1", &sc->ahb_ck0)) {
            device_printf(dev, "Can not get ahb_ck1 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->ahb_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable ahb_ck1 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "aux_ck1", &sc->aux_ck0)) {
            device_printf(dev, "Can not get aux_ck1 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->aux_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable aux_ck1 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "axi_ck1", &sc->axi_ck0)) {
            device_printf(dev, "Can not get axi_ck1 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->axi_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable axi_ck1 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "obff_ck1", &sc->obff_ck0)) {
            device_printf(dev, "Can not get obff_ck0 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->obff_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable obff_ck1 clock\n");
            return (ENXIO);
        }

        if (clk_get_by_ofw_name(dev, 0, "pipe_ck1", &sc->pipe_ck0)) {
            device_printf(dev, "Can not get pipe_ck0 clk\n");
            return (ENXIO);
        }

        error = clk_enable(sc->pipe_ck0);
        if (error != 0) {
            device_printf(sc->dev, "could not enable pipe_ck1 clock\n");
            return (ENXIO);
        }

        mt7622_pcie_port_enable(sc, port);

    } else {
        device_printf(dev, "CLocks not found.\n");
        return (ENXIO);
    }

    error = ofw_bus_find_string_index(sc->node, "interrupt-names",
                                      "pcie_irq", &sc->irq_rid);
    if (error != 0) {
        device_printf(dev, "Cannot get 'pcie_irq' IRQ\n");
        return (ENXIO);
    }
    sc->pcie_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
                                              RF_ACTIVE | RF_SHAREABLE);
    if (sc->pcie_irq_res == NULL) {
        device_printf(dev, "Cannot allocate 'pcie' IRQ resource\n");
        return (ENXIO);
    }

    /* 2. wait for refclk stable */
    DELAY(100000); /* 100 ms */
    
    error = ofw_pcib_init(dev);
    if (error != 0) {
        device_printf(dev, "ofw_pcib_init() fails\n");
        return (ENXIO);
    }

    error = mt7622_pcie_decode_ranges(sc, sc->ofw_pci.sc_range,
                                      sc->ofw_pci.sc_nrange);
    if (error != 0) {
        return (ENXIO);
    }

    error = mt7622_pcie_port_start(dev, port);
    if (error != 0) {
        device_printf(dev, "port%d: link bring-up failed: %d\n", port, error);
        return (ENXIO);
    }

    error = bus_setup_intr(dev, sc->pcie_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
                           mt7622_pcie_sys_irq, NULL, sc, &sc->pcie_irq_cookie);
    if (error != 0) {
        device_printf(dev, "cannot setup client interrupt handler\n");
        return (ENXIO);
    }

    device_add_child(dev, "pci", DEVICE_UNIT_ANY);

    error = ofw_pcib_attach(dev);
    if (error != 0) {
        return (ENXIO);
    }

    return (0);
}

static int
mt7622_pcie_probe(device_t dev) {
    if (!ofw_bus_status_okay(dev)) {
        return (ENXIO);
    }
    if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data) {
        return (ENXIO);
    }
    device_set_desc(dev, "Mediatek 7622 PCIe controller");
    return (BUS_PROBE_DEFAULT);
}

static device_method_t mt7622_pcie_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe, mt7622_pcie_probe),
        DEVMETHOD(device_attach, mt7622_pcie_attach),
        DEVMETHOD(device_detach, mt7622_pcie_detach),

        /* Bus interface */
        DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
        DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),

        /* pcib interface */
        DEVMETHOD(pcib_maxslots,		mt7622_pcib_maxslots),
        DEVMETHOD(pcib_read_config,		mt7622_pcib_read_config),
        DEVMETHOD(pcib_write_config,    mt7622_pcib_write_config),
        DEVMETHOD(pcib_route_interrupt, mt7622_pcib_route_interrupt),
        DEVMETHOD(pcib_request_feature,		pcib_request_feature_allow),

        /* OFW bus interface */
        DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
        DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
        DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
        DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
        DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

        DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, mt7622_pcie_driver, mt7622_pcie_methods, sizeof(struct
        mt7622_pcie_softc), ofw_pcib_driver); DRIVER_MODULE(mt7622_pcie, simplebus,
        mt7622_pcie_driver, NULL, NULL);
