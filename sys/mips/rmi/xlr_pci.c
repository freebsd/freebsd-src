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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/intr_machdep.h>
#include <mips/rmi/rmi_mips_exts.h>
#include <mips/rmi/interrupt.h>
#include <machine/cpuregs.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <mips/rmi/iomap.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/shared_structs.h>
#include <mips/rmi/board.h>
#include <mips/rmi/pcibus.h>

#include "pcib_if.h"

#define pci_cfg_offset(bus,slot,devfn,where) (((bus)<<16) + ((slot) << 11)+((devfn)<<8)+(where))
#define PCIE_LINK_STATE    0x4000

#define LSU_CFG0_REGID       0
#define LSU_CERRLOG_REGID    9
#define LSU_CERROVF_REGID    10
#define LSU_CERRINT_REGID    11

/* MSI support */
#define MSI_MIPS_ADDR_DEST		0x000ff000
#define MSI_MIPS_ADDR_RH		0x00000008
#define MSI_MIPS_ADDR_RH_OFF		0x00000000
#define MSI_MIPS_ADDR_RH_ON		0x00000008
#define MSI_MIPS_ADDR_DM		0x00000004
#define MSI_MIPS_ADDR_DM_PHYSICAL	0x00000000
#define MSI_MIPS_ADDR_DM_LOGICAL	0x00000004

/* Fields in data for Intel MSI messages. */
#define MSI_MIPS_DATA_TRGRMOD		0x00008000	/* Trigger mode */
#define MSI_MIPS_DATA_TRGREDG		0x00000000	/* edge */
#define MSI_MIPS_DATA_TRGRLVL		0x00008000	/* level */

#define MSI_MIPS_DATA_LEVEL		0x00004000	/* Polarity. */
#define MSI_MIPS_DATA_DEASSERT		0x00000000
#define MSI_MIPS_DATA_ASSERT		0x00004000

#define MSI_MIPS_DATA_DELMOD		0x00000700	/* Delivery Mode */
#define MSI_MIPS_DATA_DELFIXED		0x00000000	/* fixed */
#define MSI_MIPS_DATA_DELLOPRI		0x00000100	/* lowest priority */

#define MSI_MIPS_DATA_INTVEC		0x000000ff

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

#define DEBUG
#ifdef DEBUG
#define dbg_devprintf	device_printf
#else
#define dbg_devprintf(dev, fmt, ...)
#endif

struct xlr_pcib_softc {
	int junk;		/* no softc */
};

static devclass_t pcib_devclass;
static void *xlr_pci_config_base;
static struct rman irq_rman, port_rman, mem_rman;

static void
xlr_pci_init_resources(void)
{
	irq_rman.rm_start = 0;
	irq_rman.rm_end = 255;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "PCI Mapped Interrupts";
	if (rman_init(&irq_rman)
	    || rman_manage_region(&irq_rman, 0, 255))
		panic("pci_init_resources irq_rman");

	port_rman.rm_start = 0;
	port_rman.rm_end = ~0u;
	port_rman.rm_type = RMAN_ARRAY;
	port_rman.rm_descr = "I/O ports";
	if (rman_init(&port_rman)
	    || rman_manage_region(&port_rman, 0x10000000, 0x1fffffff))
		panic("pci_init_resources port_rman");

	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0u;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory";
	if (rman_init(&mem_rman)
	    || rman_manage_region(&mem_rman, 0xd0000000, 0xdfffffff))
		panic("pci_init_resources mem_rman");
}

static int
xlr_pcib_probe(device_t dev)
{
	if (xlr_board_info.is_xls)
		device_set_desc(dev, "XLS PCIe bus");
	else
		device_set_desc(dev, "XLR PCI bus");

	xlr_pci_init_resources();
	xlr_pci_config_base = (void *)MIPS_PHYS_TO_KSEG1(DEFAULT_PCI_CONFIG_BASE);

	return 0;
}

static int
xlr_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = 0;
		return (0);
	case PCIB_IVAR_BUS:
		*result = 0;
		return (0);
	}
	return (ENOENT);
}

static int
xlr_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t result)
{
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		return (EINVAL);
	case PCIB_IVAR_BUS:
		return (EINVAL);
	}
	return (ENOENT);
}

static int
xlr_pcib_maxslots(device_t dev)
{
	return (PCI_SLOTMAX);
}

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
pci_cfg_read_32bit(uint32_t addr)
{
	uint32_t temp = 0;
	uint32_t *p = (uint32_t *) ((uint32_t) xlr_pci_config_base + (addr & ~3));
	uint64_t cerr_cpu_log = 0;

	disable_and_clear_cache_error();

	temp = bswap32(*p);

	/* Read cache err log */
	cerr_cpu_log = read_64bit_phnx_ctrl_reg(CPU_BLOCKID_LSU, LSU_CERRLOG_REGID);
	if (cerr_cpu_log) {
		/* Device don't exist. */
		temp = ~0x0;
	}
	clear_and_enable_cache_error();
	return temp;
}

static u_int32_t
xlr_pcib_read_config(device_t dev, u_int b, u_int s, u_int f,
			u_int reg, int width)
{
	uint32_t data = 0;

	if ((width == 2) && (reg & 1))
		return 0xFFFFFFFF;
	else if ((width == 4) && (reg & 3))
		return 0xFFFFFFFF;

	data = pci_cfg_read_32bit(pci_cfg_offset(b, s, f, reg));

	if (width == 1)
		return ((data >> ((reg & 3) << 3)) & 0xff);
	else if (width == 2)
		return ((data >> ((reg & 3) << 3)) & 0xffff);
	else
		return data;
}

static void
xlr_pcib_write_config(device_t dev, u_int b, u_int s, u_int f,
		u_int reg, u_int32_t val, int width)
{
	uint32_t cfgaddr = pci_cfg_offset(b, s, f, reg);
	uint32_t data = 0, *p;

	if ((width == 2) && (reg & 1))
		return;
	else if ((width == 4) && (reg & 3))
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

	p = (uint32_t *)((uint32_t) xlr_pci_config_base + (cfgaddr & ~3));
	*p = bswap32(data);

	return;
}

static int 
xlr_pcib_attach(device_t dev)
{
	device_add_child(dev, "pci", 0);
	bus_generic_attach(dev);
	return 0;
}

static void
xlr_pcib_identify(driver_t * driver, device_t parent)
{
	if (xlr_board_info.is_xls) {
		xlr_reg_t *pcie_mmio_le = xlr_io_mmio(XLR_IO_PCIE_1_OFFSET);
		xlr_reg_t reg_link0 = xlr_read_reg(pcie_mmio_le, (0x80 >> 2));
		xlr_reg_t reg_link1 = xlr_read_reg(pcie_mmio_le, (0x84 >> 2));

		if ((uint16_t) reg_link0 & PCIE_LINK_STATE) {
			device_printf(parent, "Link 0 up\n");
		}
		if ((uint16_t) reg_link1 & PCIE_LINK_STATE) {
			device_printf(parent, "Link 1 up\n");
		}
	}

	BUS_ADD_CHILD(parent, 0, "pcib", 0);
}

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

static void
bridge_pcix_ack(void *arg)
{
	xlr_read_reg(xlr_io_mmio(XLR_IO_PCIX_OFFSET), 0x140 >> 2);
}

static void
bridge_pcix_mask_ack(void *arg)
{
	xlr_mask_hard_irq(arg);
	bridge_pcix_ack(arg);
}
	
static void
bridge_pcie_ack(void *arg)
{
	int irq = (int)arg;
	uint32_t reg;
	xlr_reg_t *pcie_mmio_le = xlr_io_mmio(XLR_IO_PCIE_1_OFFSET);

	switch (irq) {
	case PIC_PCIE_LINK0_IRQ : reg = PCIE_LINK0_MSI_STATUS; break;
	case PIC_PCIE_LINK1_IRQ : reg = PCIE_LINK1_MSI_STATUS; break;
	case PIC_PCIE_LINK2_IRQ : reg = PCIE_LINK2_MSI_STATUS; break;
	case PIC_PCIE_LINK3_IRQ : reg = PCIE_LINK3_MSI_STATUS; break;
	default:
		return;
	}

	xlr_write_reg(pcie_mmio_le, reg>>2, 0xffffffff);
}

static void
bridge_pcie_mask_ack(void *arg)
{
	xlr_mask_hard_irq(arg);
	bridge_pcie_ack(arg);
}

static int
mips_platform_pci_setup_intr(device_t dev, device_t child,
    struct resource *irq, int flags,
    driver_filter_t * filt,
    driver_intr_t * intr, void *arg,
    void **cookiep)
{
	int level;
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	int error = 0;
	int xlrirq;

	error = rman_activate_resource(irq);
	if (error)
		return error;
	if (rman_get_start(irq) != rman_get_end(irq)) {
		device_printf(dev, "Interrupt allocation %lu != %lu\n",
		    rman_get_start(irq), rman_get_end(irq));
		return EINVAL;
	}
	xlrirq = rman_get_start(irq);

	if (strcmp(device_get_name(dev), "pcib") != 0)
		return 0;

	if (xlr_board_info.is_xls == 0) {
		if (rmi_spin_mutex_safe)
			 mtx_lock_spin(&xlr_pic_lock);
		level = PIC_IRQ_IS_EDGE_TRIGGERED(PIC_IRT_PCIX_INDEX);
		xlr_write_reg(mmio, PIC_IRT_0_PCIX, 0x01);
		xlr_write_reg(mmio, PIC_IRT_1_PCIX, ((1 << 31) | (level << 30) |
		    (1 << 6) | (PIC_PCIX_IRQ)));
		if (rmi_spin_mutex_safe)
			 mtx_unlock_spin(&xlr_pic_lock);
		xlr_cpu_establish_hardintr(device_get_name(child), filt,
		    intr, arg, PIC_PCIX_IRQ, flags, cookiep,
		    bridge_pcix_mask_ack, xlr_unmask_hard_irq,
		    bridge_pcix_ack, NULL);
	} else {
		if (rmi_spin_mutex_safe)
			 mtx_lock_spin(&xlr_pic_lock);
		xlr_write_reg(mmio, PIC_IRT_0_BASE + xlrirq - PIC_IRQ_BASE, 0x01);
		xlr_write_reg(mmio, PIC_IRT_1_BASE + xlrirq - PIC_IRQ_BASE,
		    ((1 << 31) | (1 << 30) | (1 << 6) | xlrirq));
		if (rmi_spin_mutex_safe)
			 mtx_unlock_spin(&xlr_pic_lock);

		xlr_cpu_establish_hardintr(device_get_name(child), filt,
		    intr, arg, xlrirq, flags, cookiep,
	    	    bridge_pcie_mask_ack, xlr_unmask_hard_irq,
		    bridge_pcie_ack, NULL);
	}

	return bus_generic_setup_intr(dev, child, irq, flags, filt, intr,
	    arg, cookiep);
}

static int
mips_platform_pci_teardown_intr(device_t dev, device_t child,
    struct resource *irq, void *cookie)
{
	if (strcmp(device_get_name(child), "pci") == 0) {
		/* if needed reprogram the pic to clear pcix related entry */
		device_printf(dev, "teardown intr\n");
	}
	return bus_generic_teardown_intr(dev, child, irq, cookie);
}

static struct resource *
xlr_pci_alloc_resource(device_t bus, device_t child, int type, int *rid,
	u_long start, u_long end, u_long count, u_int flags)
{
	struct rman *rm;
	struct resource *rv;
	vm_offset_t va;
	int needactivate = flags & RF_ACTIVE;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &irq_rman;
		break;

	case SYS_RES_IOPORT:
		rm = &port_rman;
		break;

	case SYS_RES_MEMORY:
		rm = &mem_rman;
		break;

	default:
		return 0;
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == 0)
		return 0;

	rman_set_rid(rv, *rid);

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		va = (vm_offset_t)pmap_mapdev(start, count);
		rman_set_bushandle(rv, va);
		/* bushandle is same as virtual addr */
		rman_set_virtual(rv, (void *)va);
		rman_set_bustag(rv, rmi_pci_bus_space);
	}

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}
	return rv;
}

static int
xlr_pci_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	return (rman_release_resource(r));
}

static int
xlr_pci_activate_resource(device_t bus, device_t child, int type, int rid,
                      struct resource *r)
{
	return (rman_activate_resource(r));
}

static int
xlr_pci_deactivate_resource(device_t bus, device_t child, int type, int rid,
                          struct resource *r)
{
	return (rman_deactivate_resource(r));
}

static int
mips_pci_route_interrupt(device_t bus, device_t dev, int pin)
{
	/*
	 * Validate requested pin number.
	 */
	if ((pin < 1) || (pin > 4))
		return (255);

	if (xlr_board_info.is_xls) {
		switch (pin) {
		case 1:
			return PIC_PCIE_LINK0_IRQ;
		case 2:
			return PIC_PCIE_LINK1_IRQ;
		case 3:
			return PIC_PCIE_LINK2_IRQ;
		case 4:
			return PIC_PCIE_LINK3_IRQ;
		}
	} else {
		if (pin == 1) {
			return (16);
		}
	}

	return (255);
}

static device_method_t xlr_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify, xlr_pcib_identify),
	DEVMETHOD(device_probe, xlr_pcib_probe),
	DEVMETHOD(device_attach, xlr_pcib_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_read_ivar, xlr_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar, xlr_pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource, xlr_pci_alloc_resource),
	DEVMETHOD(bus_release_resource, xlr_pci_release_resource),
	DEVMETHOD(bus_activate_resource, xlr_pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, xlr_pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr, mips_platform_pci_setup_intr),
	DEVMETHOD(bus_teardown_intr, mips_platform_pci_teardown_intr),

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
	sizeof(struct xlr_pcib_softc),
};

DRIVER_MODULE(pcib, nexus, xlr_pcib_driver, pcib_devclass, 0, 0);
