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
 * NETLOGIC_BSD */
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
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <sys/pciio.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/intr_machdep.h>
#include <machine/cpuregs.h>

#include <mips/nlm/hal/haldefs.h>
#include <mips/nlm/interrupt.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/hal/pic.h>
#include <mips/nlm/hal/pcibus.h>
#include <mips/nlm/hal/uart.h>
#include <mips/nlm/xlp.h>

#include "pcib_if.h"

struct xlp_pcib_softc {
	bus_dma_tag_t	sc_pci_dmat;	/* PCI DMA tag pointer */
};

static devclass_t pcib_devclass;
static struct rman irq_rman, port_rman, mem_rman, emul_rman;

static void
xlp_pci_init_resources(void)
{

	irq_rman.rm_start = 0;
	irq_rman.rm_end = 255;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "PCI Mapped Interrupts";
	if (rman_init(&irq_rman)
	    || rman_manage_region(&irq_rman, 0, 255))
		panic("pci_init_resources irq_rman");

	port_rman.rm_start = 0;
	port_rman.rm_end = ~0ul;
	port_rman.rm_type = RMAN_ARRAY;
	port_rman.rm_descr = "I/O ports";
	if (rman_init(&port_rman)
	    || rman_manage_region(&port_rman, 0x14000000UL, 0x15ffffffUL))
		panic("pci_init_resources port_rman");

	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0ul;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory";
	if (rman_init(&mem_rman)
	    || rman_manage_region(&mem_rman, 0xd0000000ULL, 0xdfffffffULL))
		panic("pci_init_resources mem_rman");

	emul_rman.rm_start = 0;
	emul_rman.rm_end = ~0ul;
	emul_rman.rm_type = RMAN_ARRAY;
	emul_rman.rm_descr = "Emulated MEMIO";
	if (rman_init(&emul_rman)
	    || rman_manage_region(&emul_rman, 0x18000000ULL, 0x18ffffffULL))
		panic("pci_init_resources emul_rman");

}

static int
xlp_pcib_probe(device_t dev)
{

	device_set_desc(dev, "XLP PCI bus");

	xlp_pci_init_resources();
	return (0);
}

static int
xlp_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
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
xlp_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t result)
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
xlp_pcib_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static u_int32_t
xlp_pcib_read_config(device_t dev, u_int b, u_int s, u_int f,
    u_int reg, int width)
{
	uint32_t data = 0;
	uint64_t cfgaddr;
	int	regindex = reg/sizeof(uint32_t);

	cfgaddr = nlm_pcicfg_base(XLP_HDR_OFFSET(0, b, s, f));
	if ((width == 2) && (reg & 1))
		return 0xFFFFFFFF;
	else if ((width == 4) && (reg & 3))
		return 0xFFFFFFFF;

	data = nlm_read_pci_reg(cfgaddr, regindex);

	/* 
	 * Fix up read data in some SoC devices 
	 * to emulate complete PCIe header
	 */
	if (b == 0) {
		int dev = s % 8;

		/* Fake intpin on config read for UART/I2C, USB, SD/Flash */
		if (regindex == 0xf && 
		    (dev == 6 || dev == 2 || dev == 7))
			data |= 0x1 << 8;	/* Fake int pin */
	}

	if (width == 1)
		return ((data >> ((reg & 3) << 3)) & 0xff);
	else if (width == 2)
		return ((data >> ((reg & 3) << 3)) & 0xffff);
	else
		return (data);
}

static void
xlp_pcib_write_config(device_t dev, u_int b, u_int s, u_int f,
    u_int reg, u_int32_t val, int width)
{
	uint64_t cfgaddr;
	uint32_t data = 0;
	int	regindex = reg / sizeof(uint32_t);

	cfgaddr = nlm_pcicfg_base(XLP_HDR_OFFSET(0, b, s, f));
	if ((width == 2) && (reg & 1))
		return;
	else if ((width == 4) && (reg & 3))
		return;

	if (width == 1) {
		data = nlm_read_pci_reg(cfgaddr, regindex);
		data = (data & ~(0xff << ((reg & 3) << 3))) |
		    (val << ((reg & 3) << 3));
	} else if (width == 2) {
		data = nlm_read_pci_reg(cfgaddr, regindex);
		data = (data & ~(0xffff << ((reg & 3) << 3))) |
		    (val << ((reg & 3) << 3));
	} else {
		data = val;
	}

	nlm_write_pci_reg(cfgaddr, regindex, data);

	return;
}

static int 
xlp_pcib_attach(device_t dev)
{
	struct xlp_pcib_softc *sc;
	sc = device_get_softc(dev);

	device_add_child(dev, "pci", 0);
	bus_generic_attach(dev);

	return (0);
}

static void
xlp_pcib_identify(driver_t * driver, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "pcib", 0);
}

/*
 * XLS PCIe can have upto 4 links, and each link has its on IRQ
 * Find the link on which the device is on 
 */
static int
xlp_pcie_link(device_t pcib, device_t dev)
{
	device_t parent, tmp;

	/* find the lane on which the slot is connected to */
	printf("xlp_pcie_link : bus %s dev %s\n", device_get_nameunit(pcib),
		device_get_nameunit(dev));
	tmp = dev;
	while (1) {
		parent = device_get_parent(tmp);
		if (parent == NULL || parent == pcib) {
			device_printf(dev, "Cannot find parent bus\n");
			return (-1);
		}
		if (strcmp(device_get_nameunit(parent), "pci0") == 0)
			break;
		tmp = parent;
	}
	return (pci_get_function(tmp));
}

/*
 * Find the IRQ for the link, each link has a different interrupt 
 * at the XLP pic
 */
static int
xlp_pcie_link_irt(int link)
{

	if( (link < 0) || (link > 3))
		return (-1);

	return PIC_IRT_PCIE_LINK_INDEX(link);
}

static int
xlp_alloc_msi(device_t pcib, device_t dev, int count, int maxcount, int *irqs)
{
	int i, link;

	/*
	 * Each link has 32 MSIs that can be allocated, but for now
	 * we only support one device per link.
	 * msi_alloc() equivalent is needed when we start supporting 
	 * bridges on the PCIe link.
	 */
	link = xlp_pcie_link(pcib, dev);
	if (link == -1)
		return (ENXIO);

	/*
	 * encode the irq so that we know it is a MSI interrupt when we
	 * setup interrupts
	 */
	for (i = 0; i < count; i++)
		irqs[i] = 64 + link * 32 + i;

	return (0);
}

static int
xlp_release_msi(device_t pcib, device_t dev, int count, int *irqs)
{
	device_printf(dev, "%s: msi release %d\n", device_get_nameunit(pcib),
	    count);
	return (0);
}

static int
xlp_map_msi(device_t pcib, device_t dev, int irq, uint64_t *addr,
    uint32_t *data)
{
	int msi, irt;

	if (irq >= 64) {
		msi = irq - 64;
		*addr = MIPS_MSI_ADDR(0);

		irt = xlp_pcie_link_irt(msi/32);
		if (irt != -1)
			*data = MIPS_MSI_DATA(xlp_irt_to_irq(irt));
		return (0);
	} else {
		device_printf(dev, "%s: map_msi for irq %d  - ignored", 
		    device_get_nameunit(pcib), irq);
		return (ENXIO);
	}
}

static void
bridge_pcie_ack(int irq)
{
	uint32_t node,reg;
	uint64_t base;

	node = nlm_nodeid();
	reg = PCIE_MSI_STATUS;

	switch(irq) {
		case PIC_PCIE_0_IRQ:
			base = nlm_pcicfg_base(XLP_IO_PCIE0_OFFSET(node));
			break;
		case PIC_PCIE_1_IRQ:
			base = nlm_pcicfg_base(XLP_IO_PCIE1_OFFSET(node));
			break;
		case PIC_PCIE_2_IRQ:
			base = nlm_pcicfg_base(XLP_IO_PCIE2_OFFSET(node));
			break;
		case PIC_PCIE_3_IRQ:
			base = nlm_pcicfg_base(XLP_IO_PCIE3_OFFSET(node));
			break;
		default:
			return;
	}

	nlm_write_pci_reg(base, reg, 0xFFFFFFFF);

	return;
}

static int
mips_platform_pci_setup_intr(device_t dev, device_t child,
    struct resource *irq, int flags, driver_filter_t *filt,
    driver_intr_t *intr, void *arg, void **cookiep)
{
	int error = 0;
	int xlpirq;
	int node,base,val,link;
	void *extra_ack;

	error = rman_activate_resource(irq);
	if (error)
		return error;
	if (rman_get_start(irq) != rman_get_end(irq)) {
		device_printf(dev, "Interrupt allocation %lu != %lu\n",
		    rman_get_start(irq), rman_get_end(irq));
		return (EINVAL);
	}
	xlpirq = rman_get_start(irq);
	device_printf(dev, "setup intr %d\n", xlpirq);

	if (strcmp(device_get_name(dev), "pcib") != 0) {
		device_printf(dev, "ret 0 on dev\n");
		return (0);
	}

	/* 
	 * temporary hack for MSI, we support just one device per
	 * link, and assign the link interrupt to the device interrupt
	 */
	if (xlpirq >= 64) {
		xlpirq -= 64;
		if (xlpirq % 32 != 0)
			return (0);

		node = nlm_nodeid();
		link = (xlpirq / 32);
		base = nlm_pcicfg_base(XLP_IO_PCIE_OFFSET(node,link));

		/* MSI Interrupt Vector enable at bridge's configuration */
		nlm_write_pci_reg(base, PCIE_MSI_EN, PCIE_MSI_VECTOR_INT_EN);

		val = nlm_read_pci_reg(base, PCIE_INT_EN0);
		/* MSI Interrupt enable at bridge's configuration */
		nlm_write_pci_reg(base, PCIE_INT_EN0,
				(val | PCIE_MSI_INT_EN));

		/* legacy interrupt disable at bridge */
		val = nlm_read_pci_reg(base, PCIE_BRIDGE_CMD);
		nlm_write_pci_reg(base, PCIE_BRIDGE_CMD,
				(val | PCIM_CMD_INTxDIS));

		/* MSI address update at bridge */
		val = nlm_read_pci_reg(base, PCIE_BRIDGE_MSI_ADDRL);
		nlm_write_pci_reg(base, PCIE_BRIDGE_MSI_ADDRL,
				(val | MSI_MIPS_ADDR_BASE));

		val = nlm_read_pci_reg(base, PCIE_BRIDGE_MSI_CAP);
		/* MSI capability enable at bridge */
		nlm_write_pci_reg(base, PCIE_BRIDGE_MSI_CAP, 
				(val |
				(PCIM_MSICTRL_MSI_ENABLE << 16) |
				(PCIM_MSICTRL_MMC_32 << 16)));

		xlpirq = xlp_pcie_link_irt(xlpirq / 32);
		if (xlpirq == -1)
			return (EINVAL);
		xlpirq = xlp_irt_to_irq(xlpirq);
	}
	/* Set all irqs to CPU 0 for now */
	nlm_pic_write_irt_direct(xlp_pic_base, xlp_irq_to_irt(xlpirq), 1, 0,
				 PIC_LOCAL_SCHEDULING, xlpirq, 0);
	extra_ack = NULL;
	if (xlpirq >= PIC_PCIE_0_IRQ &&
	    xlpirq <= PIC_PCIE_3_IRQ)
		extra_ack = bridge_pcie_ack;
	xlp_establish_intr(device_get_name(child), filt,
	    intr, arg, xlpirq, flags, cookiep, extra_ack);

	return (0);
}

static int
mips_platform_pci_teardown_intr(device_t dev, device_t child,
    struct resource *irq, void *cookie)
{
	if (strcmp(device_get_name(child), "pci") == 0) {
		/* if needed reprogram the pic to clear pcix related entry */
		device_printf(dev, "teardown intr\n");
	}
	return (bus_generic_teardown_intr(dev, child, irq, cookie));
}

static void
assign_soc_resource(device_t child, int type, u_long *startp, u_long *endp,
    u_long *countp, struct rman **rm, bus_space_tag_t *bst, vm_offset_t *va)
{
	int devid = pci_get_device(child);
	int inst = pci_get_function(child);
	int node = pci_get_slot(child) / 8;

	*rm = NULL;
	*va = 0;
	*bst = 0;
	switch (devid) {
	case PCI_DEVICE_ID_NLM_UART:
		switch (type) {
		case SYS_RES_IRQ:
			*startp = *endp = PIC_UART_0_IRQ + inst;
			*countp = 1;
			break;
		case SYS_RES_MEMORY: 
			*va = nlm_get_uart_regbase(node, inst);
			*startp = MIPS_KSEG1_TO_PHYS(va);
			*countp = 0x100;
			*rm = &emul_rman;
			*bst = uart_bus_space_mem;
			break;
		};
		break;

	case PCI_DEVICE_ID_NLM_EHCI:
		if (type == SYS_RES_IRQ) {
			if (inst == 0)
				*startp = *endp = PIC_EHCI_0_IRQ;
			else if (inst == 3)
				*startp = *endp = PIC_EHCI_1_IRQ;
			else
				device_printf(child, "bad instance %d\n", inst);

			*countp = 1; 
		}
		break;
	}

	/* default to rmi_bus_space for SoC resources */
	if (type == SYS_RES_MEMORY && *bst == 0)
		*bst = rmi_bus_space;
}

static struct resource *
xlp_pci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct rman *rm = NULL;
	struct resource *rv;
	vm_offset_t va = 0;
	int needactivate = flags & RF_ACTIVE;
	bus_space_tag_t bst = 0;

	/*
	 * For SoC PCI devices, we have to assign resources correctly
	 * since the IRQ and MEM resources depend on the block.
	 * If the address is not from BAR0, then we use emul_rman
	 */
	if (pci_get_bus(child) == 0 &&
	    pci_get_vendor(child) == PCI_VENDOR_NETLOGIC)
      		assign_soc_resource(child, type, &start, &end,
		    &count, &rm, &bst, &va);
	if (rm == NULL) {
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
			return (0);
		}
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == 0)
		return (0);

	rman_set_rid(rv, *rid);

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		if (va == 0)
			va = (vm_offset_t)pmap_mapdev(start, count);
		if (bst == 0)
			bst = rmi_pci_bus_space;

		rman_set_bushandle(rv, va);
		rman_set_virtual(rv, (void *)va);
		rman_set_bustag(rv, bst);
	}

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
xlp_pci_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_release_resource(r));
}

static bus_dma_tag_t
xlp_pci_get_dma_tag(device_t bus, device_t child)
{
	struct xlp_pcib_softc *sc;

	sc = device_get_softc(bus);
	return (sc->sc_pci_dmat);
}

static int
xlp_pci_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_activate_resource(r));
}

static int
xlp_pci_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_deactivate_resource(r));
}

static int
mips_pci_route_interrupt(device_t bus, device_t dev, int pin)
{
	int irt, link;

	/*
	 * Validate requested pin number.
	 */
	device_printf(bus, "route  %s %d", device_get_nameunit(dev), pin);
	if ((pin < 1) || (pin > 4))
		return (255);

	link = xlp_pcie_link(bus, dev);
	irt = xlp_pcie_link_irt(link);
	if (irt != -1)
		return (xlp_irt_to_irq(irt));

	return (255);
}

static device_method_t xlp_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify, xlp_pcib_identify),
	DEVMETHOD(device_probe, xlp_pcib_probe),
	DEVMETHOD(device_attach, xlp_pcib_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_read_ivar, xlp_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar, xlp_pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource, xlp_pci_alloc_resource),
	DEVMETHOD(bus_release_resource, xlp_pci_release_resource),
	DEVMETHOD(bus_get_dma_tag, xlp_pci_get_dma_tag),
	DEVMETHOD(bus_activate_resource, xlp_pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, xlp_pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr, mips_platform_pci_setup_intr),
	DEVMETHOD(bus_teardown_intr, mips_platform_pci_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots, xlp_pcib_maxslots),
	DEVMETHOD(pcib_read_config, xlp_pcib_read_config),
	DEVMETHOD(pcib_write_config, xlp_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt, mips_pci_route_interrupt),

	DEVMETHOD(pcib_alloc_msi, xlp_alloc_msi),
	DEVMETHOD(pcib_release_msi, xlp_release_msi),
	DEVMETHOD(pcib_map_msi, xlp_map_msi),

	{0, 0}
};

static driver_t xlp_pcib_driver = {
	"pcib",
	xlp_pcib_methods,
	sizeof(struct xlp_pcib_softc),
};

DRIVER_MODULE(pcib, nexus, xlp_pcib_driver, pcib_devclass, 0, 0);
