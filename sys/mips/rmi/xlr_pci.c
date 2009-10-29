/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 * RMI_BSD */
#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <mips/rmi/rmi_mips_exts.h>
#include <machine/cpuregs.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include <mips/rmi/iomap.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/shared_structs.h>
#include <mips/rmi/board.h>
#include <mips/rmi/pcibus.h>
#include "pcib_if.h"

#define LSU_CFG0_REGID       0
#define LSU_CERRLOG_REGID    9
#define LSU_CERROVF_REGID    10
#define LSU_CERRINT_REGID    11
#define SWAP32(x)\
        (((x) & 0xff000000) >> 24) | \
        (((x) & 0x000000ff) << 24) | \
        (((x) & 0x0000ff00) << 8)  | \
        (((x) & 0x00ff0000) >> 8)


/* MSI support */

#define MSI_MIPS_ADDR_DEST             0x000ff000
#define MSI_MIPS_ADDR_RH               0x00000008
#define MSI_MIPS_ADDR_RH_OFF          0x00000000
#define MSI_MIPS_ADDR_RH_ON           0x00000008
#define MSI_MIPS_ADDR_DM               0x00000004
#define MSI_MIPS_ADDR_DM_PHYSICAL     0x00000000
#define MSI_MIPS_ADDR_DM_LOGICAL      0x00000004

/* Fields in data for Intel MSI messages. */
#define MSI_MIPS_DATA_TRGRMOD          0x00008000	/* Trigger mode */
#define MSI_MIPS_DATA_TRGREDG         0x00000000	/* edge */
#define MSI_MIPS_DATA_TRGRLVL         0x00008000	/* level */

#define MSI_MIPS_DATA_LEVEL            0x00004000	/* Polarity. */
#define MSI_MIPS_DATA_DEASSERT        0x00000000
#define MSI_MIPS_DATA_ASSERT          0x00004000

#define MSI_MIPS_DATA_DELMOD           0x00000700	/* Delivery Mode */
#define MSI_MIPS_DATA_DELFIXED	       0x00000000	/* fixed */
#define MSI_MIPS_DATA_DELLOPRI        0x00000100	/* lowest priority */

#define MSI_MIPS_DATA_INTVEC           0x000000ff

/*
 * Build Intel MSI message and data values from a source.  AMD64 systems
 * seem to be compatible, so we use the same function for both.
 */
#define MIPS_MSI_ADDR(cpu)					       \
        (MSI_MIPS_ADDR_BASE | (cpu) << 12 |			       \
	 MSI_MIPS_ADDR_RH_OFF | MSI_MIPS_ADDR_DM_PHYSICAL)

#define MIPS_MSI_DATA(irq)					       \
        (MSI_MIPS_DATA_TRGRLVL | MSI_MIPS_DATA_DELFIXED |	       \
	 MSI_MIPS_DATA_ASSERT | (irq))

struct xlr_hose_softc {
	int junk;		/* no softc */
};
static devclass_t pcib_devclass;
static int pci_bus_status = 0;
static void *pci_config_base;

static uint32_t pci_cfg_read_32bit(uint32_t addr);
static void pci_cfg_write_32bit(uint32_t addr, uint32_t data);

static int
xlr_pcib_probe(device_t dev)
{
	device_set_desc(dev, "xlr system bridge controller");

	pci_init_resources();
	pci_config_base = (void *)MIPS_PHYS_TO_KSEG1(DEFAULT_PCI_CONFIG_BASE);
	pci_bus_status = 1;

	return 0;
}

static int
xlr_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t * result)
{
#if 0
	device_printf(dev, "xlr_pcib_read_ivar : read ivar %d for child %s\n", which, device_get_nameunit(child));
#endif
	switch (which) {
	case PCIB_IVAR_BUS:
		*result = 0;
		return 0;
	}
	return ENOENT;
}

static int
xlr_pcib_maxslots(device_t dev)
{
	if (xlr_board_info.is_xls)
		return 4;
	else
		return 32;
}

#define pci_cfg_offset(bus,slot,devfn,where) (((bus)<<16) + ((slot) << 11)+((devfn)<<8)+(where))

static __inline__ void 
disable_and_clear_cache_error(void)
{
	uint64_t lsu_cfg0 = read_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CFG0_REGID);

	lsu_cfg0 = lsu_cfg0 & ~0x2e;
	write_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CFG0_REGID, lsu_cfg0);
	/* Clear cache error log */
	write_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CERRLOG_REGID, 0);
}

static __inline__ void 
clear_and_enable_cache_error(void)
{
	uint64_t lsu_cfg0 = 0;

	/* first clear the cache error logging register */
	write_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CERRLOG_REGID, 0);
	write_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CERROVF_REGID, 0);
	write_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CERRINT_REGID, 0);

	lsu_cfg0 = read_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CFG0_REGID);
	lsu_cfg0 = lsu_cfg0 | 0x2e;
	write_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CFG0_REGID, lsu_cfg0);
}

static uint32_t 
phoenix_pciread(u_int b, u_int s, u_int f,
    u_int reg, int width)
{
	uint32_t data = 0;

	if ((width == 2) && (reg & 1))
		return 0xFFFFFFFF;
	else if ((width == 4) && (reg & 3))
		return 0xFFFFFFFF;

	if (pci_bus_status)
		data = pci_cfg_read_32bit(pci_cfg_offset(b, s, f, reg));
	else
		data = 0xFFFFFFFF;

	if (width == 1)
		return ((data >> ((reg & 3) << 3)) & 0xff);
	else if (width == 2)
		return ((data >> ((reg & 3) << 3)) & 0xffff);
	else
		return data;
}

static void 
phoenix_pciwrite(u_int b, u_int s, u_int f,
    u_int reg, u_int val, int width)
{
	uint32_t cfgaddr = pci_cfg_offset(b, s, f, reg);
	uint32_t data = 0;

	if ((width == 2) && (reg & 1))
		return;
	else if ((width == 4) && (reg & 3))
		return;

	if (!pci_bus_status)
		return;

	if (width == 1) {
		data = pci_cfg_read_32bit(cfgaddr);
		data = (data & ~(0xff << ((reg & 3) << 3))) |
		    (val << ((reg & 3) << 3));
	} else if (width == 2) {
		data = pci_cfg_read_32bit(cfgaddr);
		data = (data & ~(0xffff << ((reg & 3) << 3))) |
		    (val << ((reg & 3) << 3));
	} else {
		data = val;
	}

	pci_cfg_write_32bit(cfgaddr, data);

	return;
}

static uint32_t 
pci_cfg_read_32bit(uint32_t addr)
{
	uint32_t temp = 0;
	uint32_t *p = (uint32_t *) ((uint32_t) pci_config_base + (addr & ~3));
	uint64_t cerr_cpu_log = 0;

	disable_and_clear_cache_error();

	temp = SWAP32(*p);

	/* Read cache err log */
	cerr_cpu_log = read_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CERRLOG_REGID);

	if (cerr_cpu_log) {
		/* Device don't exist. */
		temp = ~0x0;
	}
	clear_and_enable_cache_error();
	return temp;
}


static void 
pci_cfg_write_32bit(uint32_t addr, uint32_t data)
{
	unsigned int *p = (unsigned int *)((uint32_t) pci_config_base + (addr & ~3));

	*p = SWAP32(data);
}

static u_int32_t
xlr_pcib_read_config(device_t dev, u_int b, u_int s, u_int f,
    u_int reg, int width)
{
	return phoenix_pciread(b, s, f, reg, width);
}

static void
xlr_pcib_write_config(device_t dev, u_int b, u_int s, u_int f,
    u_int reg, u_int32_t val, int width)
{
	phoenix_pciwrite(b, s, f, reg, val, width);
}

static int 
xlr_pcib_attach(device_t dev)
{
	device_add_child(dev, "pci", 0);
	bus_generic_attach(dev);
	return 0;
}

#define  PCIE_LINK_STATE    0x4000

static void
xlr_pcib_identify(driver_t * driver, device_t parent)
{
	xlr_reg_t *pcie_mmio_le = xlr_io_mmio(XLR_IO_PCIE_1_OFFSET);
	xlr_reg_t reg_link0 = xlr_read_reg(pcie_mmio_le, (0x80 >> 2));
	xlr_reg_t reg_link1 = xlr_read_reg(pcie_mmio_le, (0x84 >> 2));

	if ((uint16_t) reg_link0 & PCIE_LINK_STATE) {
		device_printf(parent, "Link 0 up\n");
	}
	if ((uint16_t) reg_link1 & PCIE_LINK_STATE) {
		device_printf(parent, "Link 1 up\n");
	}
	BUS_ADD_CHILD(parent, 0, "pcib", 0);

}
static int
    xlr_alloc_msi(device_t pcib, device_t dev, int count, int maxcount, int *irqs);
static int
    xlr_release_msi(device_t pcib, device_t dev, int count, int *irqs);

static int
xlr_alloc_msi(device_t pcib, device_t dev, int count, int maxcount, int *irqs)
{
	int pciirq;
	int i;
	device_t parent, tmp;


	/* find the lane on which the slot is connected to */
	tmp = dev;
	while (1) {
		parent = device_get_parent(tmp);
		if (parent == NULL || parent == pcib) {
			device_printf(dev, "Cannot find parent bus\n");
			return ENXIO;
		}
		if (strcmp(device_get_nameunit(parent), "pci0") == 0)
			break;
		tmp = parent;
	}

	switch (pci_get_slot(tmp)) {
	case 0:
		pciirq = PIC_PCIE_LINK0_IRQ;
		break;
	case 1:
		pciirq = PIC_PCIE_LINK1_IRQ;
		break;
	case 2:
		pciirq = PIC_PCIE_LINK2_IRQ;
		break;
	case 3:
		pciirq = PIC_PCIE_LINK3_IRQ;
		break;
	default:
		return ENXIO;
	}

	irqs[0] = pciirq;
	/*
	 * For now put in some fixed values for the other requested MSI,
	 * TODO handle multiple messages
	 */
	for (i = 1; i < count; i++)
		irqs[i] = pciirq + 64 * i;

	return 0;
}

static int
xlr_release_msi(device_t pcib, device_t dev, int count, int *irqs)
{
	device_printf(dev, "%s: msi release %d\n", device_get_nameunit(pcib), count);
	return 0;
}
static int
    xlr_map_msi(device_t pcib, device_t dev, int irq, uint64_t * addr, uint32_t * data);

static int
xlr_map_msi(device_t pcib, device_t dev, int irq, uint64_t * addr, uint32_t * data)
{
	switch (irq) {
		case PIC_PCIE_LINK0_IRQ:
		case PIC_PCIE_LINK1_IRQ:
		case PIC_PCIE_LINK2_IRQ:
		case PIC_PCIE_LINK3_IRQ:
		*addr = MIPS_MSI_ADDR(0);
		*data = MIPS_MSI_DATA(irq);
		return 0;

	default:
		device_printf(dev, "%s: map_msi for irq %d  - ignored", device_get_nameunit(pcib),
		    irq);
		return (ENXIO);
	}

}

static device_method_t xlr_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify, xlr_pcib_identify),
	DEVMETHOD(device_probe, xlr_pcib_probe),
	DEVMETHOD(device_attach, xlr_pcib_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_read_ivar, xlr_pcib_read_ivar),
	DEVMETHOD(bus_alloc_resource, xlr_pci_alloc_resource),
	DEVMETHOD(bus_release_resource, pci_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr, mips_platform_pci_setup_intr),
	DEVMETHOD(bus_teardown_intr, bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots, xlr_pcib_maxslots),
	DEVMETHOD(pcib_read_config, xlr_pcib_read_config),
	DEVMETHOD(pcib_write_config, xlr_pcib_write_config),

	DEVMETHOD(pcib_route_interrupt, mips_pci_route_interrupt),

	DEVMETHOD(pcib_alloc_msi, xlr_alloc_msi),
	DEVMETHOD(pcib_release_msi, xlr_release_msi),
	DEVMETHOD(pcib_map_msi, xlr_map_msi),

	{0, 0}
};

static driver_t xlr_pcib_driver = {
	"pcib",
	xlr_pcib_methods,
	sizeof(struct xlr_hose_softc),
};

DRIVER_MODULE(pcib, nexus, xlr_pcib_driver, pcib_devclass, 0, 0);
