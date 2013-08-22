/*-
 * Copyright (c) 2003-2012 Broadcom Corporation
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY BROADCOM ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL BROADCOM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include <sys/pciio.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>

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
#include <mips/nlm/hal/bridge.h>
#include <mips/nlm/hal/gbu.h>
#include <mips/nlm/hal/pcibus.h>
#include <mips/nlm/hal/uart.h>
#include <mips/nlm/xlp.h>

#include "pcib_if.h"
#include "pci_if.h"

#define	EMUL_MEM_START	0x16000000UL
#define	EMUL_MEM_END	0x18ffffffUL

/* SoC device qurik handling */
static int irt_irq_map[4 * 256];
static int irq_irt_map[64];

static void
xlp_add_irq(int node, int irt, int irq)
{
	int nodeirt = node * 256 + irt;

	irt_irq_map[nodeirt] = irq;
	irq_irt_map[irq] = nodeirt;
}

int
xlp_irq_to_irt(int irq)
{
	return irq_irt_map[irq];
}

int
xlp_irt_to_irq(int nodeirt)
{
	return irt_irq_map[nodeirt];
}

/* Override PCI a bit for SoC devices */

enum {
	INTERNAL_DEV	= 0x1,	/* internal device, skip on enumeration */
	MEM_RES_EMUL	= 0x2,	/* no MEM or IO bar, custom res alloc */
	SHARED_IRQ	= 0x4,
	DEV_MMIO32	= 0x8,	/* byte access not allowed to mmio */
};

struct soc_dev_desc {
	u_int	devid;		/* device ID */
	int	irqbase;	/* start IRQ */
	u_int	flags;		/* flags */
	int	ndevs;		/* to keep track of number of devices */
};

struct soc_dev_desc xlp_dev_desc[] = {
	{ PCI_DEVICE_ID_NLM_ICI,               0, INTERNAL_DEV },
	{ PCI_DEVICE_ID_NLM_PIC,               0, INTERNAL_DEV },
	{ PCI_DEVICE_ID_NLM_FMN,               0, INTERNAL_DEV },
	{ PCI_DEVICE_ID_NLM_UART, PIC_UART_0_IRQ, MEM_RES_EMUL | DEV_MMIO32},
	{ PCI_DEVICE_ID_NLM_I2C,               0, MEM_RES_EMUL | DEV_MMIO32 },
	{ PCI_DEVICE_ID_NLM_NOR,               0, MEM_RES_EMUL },
	{ PCI_DEVICE_ID_NLM_MMC,     PIC_MMC_IRQ, MEM_RES_EMUL },
	{ PCI_DEVICE_ID_NLM_EHCI, PIC_EHCI_0_IRQ, 0 }
};

struct  xlp_devinfo {
	struct pci_devinfo pcidev;
	int	irq;
	int	flags;
	u_long	mem_res_start;
};

static __inline struct soc_dev_desc *
xlp_find_soc_desc(int devid)
{
	struct soc_dev_desc *p;
	int i, n;

	n = sizeof(xlp_dev_desc) / sizeof(xlp_dev_desc[0]);
	for (i = 0, p = xlp_dev_desc; i < n; i++, p++)
		if (p->devid == devid)
			return (p);
	return (NULL);
}

static struct resource *
xlp_pci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct resource *r;
	struct xlp_devinfo *xlp_devinfo;
	int busno;

	/*
	 * Do custom allocation for MEMORY resource for SoC device if 
	 * MEM_RES_EMUL flag is set
	 */
	busno = pci_get_bus(child);
	if ((type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) && busno == 0) {
		xlp_devinfo = (struct xlp_devinfo *)device_get_ivars(child);
		if ((xlp_devinfo->flags & MEM_RES_EMUL) != 0) {
			/* no emulation for IO ports */
			if (type == SYS_RES_IOPORT)
				return (NULL);

			start = xlp_devinfo->mem_res_start;
			count = XLP_PCIE_CFG_SIZE - XLP_IO_PCI_HDRSZ;

			/* MMC needs to 2 slots with rids 16 and 20 and a
			 * fixup for size */
			if (pci_get_device(child) == PCI_DEVICE_ID_NLM_MMC) {
				count = 0x100;
				if (*rid == 16)
					; /* first slot already setup */
				else if (*rid == 20)
					start += 0x100; /* second slot */
				else
					return (NULL);
			}

			end = start + count - 1;
			r = BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
			    type, rid, start, end, count, flags);
			if (r == NULL)
				return (NULL);
			if ((xlp_devinfo->flags & DEV_MMIO32) != 0)
				rman_set_bustag(r, rmi_uart_bus_space);
			return (r);
		}
	}

	/* Not custom alloc, use PCI code */
	return (pci_alloc_resource(bus, child, type, rid, start, end, count,
	    flags));
}

static int
xlp_pci_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	u_long start;

	/* If custom alloc, handle that */
	start = rman_get_start(r);
	if (type == SYS_RES_MEMORY && pci_get_bus(child) == 0 &&
	    start >= EMUL_MEM_START && start <= EMUL_MEM_END)
		return (BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r));

	/* use default PCI function */
	return (bus_generic_rl_release_resource(bus, child, type, rid, r));
}

static void
xlp_add_soc_child(device_t pcib, device_t dev, int b, int s, int f)
{
	struct pci_devinfo *dinfo;
	struct xlp_devinfo *xlp_dinfo;
	struct soc_dev_desc *si;
	uint64_t pcibase;
	int domain, node, irt, irq, flags, devoffset, num;
	uint16_t devid;

	domain = pcib_get_domain(dev);
	node = s / 8;
	devoffset = XLP_HDR_OFFSET(node, 0, s % 8, f);
	if (!nlm_dev_exists(devoffset))
		return;

	/* Find if there is a desc for the SoC device */
	devid = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_DEVICE, 2);
	si = xlp_find_soc_desc(devid);

	/* update flags and irq from desc if available */
	irq = 0;
	flags = 0;
	if (si != NULL) {
		if (si->irqbase != 0)
			irq = si->irqbase + si->ndevs;
		flags = si->flags;
		si->ndevs++;
	}

	/* skip internal devices */
	if ((flags & INTERNAL_DEV) != 0)
		return;

	/* PCIe interfaces are special, bug in Ax */
	if (devid == PCI_DEVICE_ID_NLM_PCIE) {
		xlp_add_irq(node, xlp_pcie_link_irt(f), PIC_PCIE_0_IRQ + f);
	} else {
		/* Stash intline and pin in shadow reg for devices */
		pcibase = nlm_pcicfg_base(devoffset);
		irt = nlm_irtstart(pcibase);
		num = nlm_irtnum(pcibase);
		if (irq != 0 && num > 0) {
			xlp_add_irq(node, irt, irq);
			nlm_write_reg(pcibase, XLP_PCI_DEVSCRATCH_REG0,
			    (1 << 8) | irq);
		}
	}
	dinfo = pci_read_device(pcib, domain, b, s, f, sizeof(*xlp_dinfo));
	if (dinfo == NULL)
		return;
	xlp_dinfo = (struct xlp_devinfo *)dinfo;
	xlp_dinfo->irq = irq;
	xlp_dinfo->flags = flags;

	/* memory resource from ecfg space, if MEM_RES_EMUL is set */
	if ((flags & MEM_RES_EMUL) != 0)
		xlp_dinfo->mem_res_start = XLP_DEFAULT_IO_BASE + devoffset +
		    XLP_IO_PCI_HDRSZ;
	pci_add_child(dev, dinfo);
}

static int
xlp_pci_attach(device_t dev)
{
	device_t pcib = device_get_parent(dev);
	int maxslots, s, f, pcifunchigh;
	int busno;
	uint8_t hdrtype;

	/*
	 * The on-chip devices are on a bus that is almost, but not
	 * quite, completely like PCI. Add those things by hand.
	 */
	busno = pcib_get_bus(dev);
	maxslots = PCIB_MAXSLOTS(pcib);
	for (s = 0; s <= maxslots; s++) {
		pcifunchigh = 0;
		f = 0;
		hdrtype = PCIB_READ_CONFIG(pcib, busno, s, f, PCIR_HDRTYPE, 1);
		if ((hdrtype & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
			continue;
		if (hdrtype & PCIM_MFDEV)
			pcifunchigh = PCI_FUNCMAX;
		for (f = 0; f <= pcifunchigh; f++)
			xlp_add_soc_child(pcib, dev, busno, s, f);
	}
	return (bus_generic_attach(dev));
}

static int
xlp_pci_probe(device_t dev)
{
	device_t pcib;

	pcib = device_get_parent(dev);
	/*
	 * Only the top level bus has SoC devices, leave the rest to
	 * Generic PCI code
	 */
	if (strcmp(device_get_nameunit(pcib), "pcib0") != 0)
		return (ENXIO);
	device_set_desc(dev, "XLP SoCbus");
	return (BUS_PROBE_DEFAULT);
}

static devclass_t pci_devclass;
static device_method_t xlp_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xlp_pci_probe),
	DEVMETHOD(device_attach,	xlp_pci_attach),
	DEVMETHOD(bus_alloc_resource,	xlp_pci_alloc_resource),
	DEVMETHOD(bus_release_resource, xlp_pci_release_resource),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pci, xlp_pci_driver, xlp_pci_methods, sizeof(struct pci_softc),
    pci_driver);
DRIVER_MODULE(xlp_pci, pcib, xlp_pci_driver, pci_devclass, 0, 0);

static devclass_t pcib_devclass;
static struct rman irq_rman, port_rman, mem_rman, emul_rman;

static void
xlp_pcib_init_resources(void)
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
	    || rman_manage_region(&port_rman, PCIE_IO_BASE, PCIE_IO_LIMIT))
		panic("pci_init_resources port_rman");

	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0ul;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory";
	if (rman_init(&mem_rman)
	    || rman_manage_region(&mem_rman, PCIE_MEM_BASE, PCIE_MEM_LIMIT))
		panic("pci_init_resources mem_rman");

	/*
	 * This includes the GBU (nor flash) memory range and the PCIe
	 * memory area. 
	 */
	emul_rman.rm_start = 0;
	emul_rman.rm_end = ~0ul;
	emul_rman.rm_type = RMAN_ARRAY;
	emul_rman.rm_descr = "Emulated MEMIO";
	if (rman_init(&emul_rman)
	    || rman_manage_region(&emul_rman, EMUL_MEM_START, EMUL_MEM_END))
		panic("pci_init_resources emul_rman");
}

static int
xlp_pcib_probe(device_t dev)
{

	device_set_desc(dev, "XLP PCI bus");
	xlp_pcib_init_resources();
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

	/* 
	 * The intline and int pin of SoC devices are DOA, except
	 * for bridges (slot %8 == 1).
	 * use the values we stashed in a writable PCI scratch reg.
	 */
	if (b == 0 && regindex == 0xf && s % 8 > 1)
		regindex = XLP_PCI_DEVSCRATCH_REG0;

	data = nlm_read_pci_reg(cfgaddr, regindex);
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

	/*
	 * use shadow reg for intpin/intline which are dead
	 */
	if (b == 0 && regindex == 0xf && s % 8 > 1)
		regindex = XLP_PCI_DEVSCRATCH_REG0;
	nlm_write_pci_reg(cfgaddr, regindex, data);
}

/*
 * Enable byte swap in hardware when compiled big-endian.
 * Programs a link's PCIe SWAP regions from the link's IO and MEM address
 * ranges.
 */
static void
xlp_pcib_hardware_swap_enable(int node, int link)
{
#if BYTE_ORDER == BIG_ENDIAN
	uint64_t bbase, linkpcibase;
	uint32_t bar;
	int pcieoffset;

	pcieoffset = XLP_IO_PCIE_OFFSET(node, link);
	if (!nlm_dev_exists(pcieoffset))
		return;

	bbase = nlm_get_bridge_regbase(node);
	linkpcibase = nlm_pcicfg_base(pcieoffset);
	bar = nlm_read_bridge_reg(bbase, BRIDGE_PCIEMEM_BASE0 + link);
	nlm_write_pci_reg(linkpcibase, PCIE_BYTE_SWAP_MEM_BASE, bar);

	bar = nlm_read_bridge_reg(bbase, BRIDGE_PCIEMEM_LIMIT0 + link);
	nlm_write_pci_reg(linkpcibase, PCIE_BYTE_SWAP_MEM_LIM, bar | 0xFFF);

	bar = nlm_read_bridge_reg(bbase, BRIDGE_PCIEIO_BASE0 + link);
	nlm_write_pci_reg(linkpcibase, PCIE_BYTE_SWAP_IO_BASE, bar);

	bar = nlm_read_bridge_reg(bbase, BRIDGE_PCIEIO_LIMIT0 + link);
	nlm_write_pci_reg(linkpcibase, PCIE_BYTE_SWAP_IO_LIM, bar | 0xFFF);
#endif
}

static int 
xlp_pcib_attach(device_t dev)
{
	int node, link;

	/* enable hardware swap on all nodes/links */
	for (node = 0; node < XLP_MAX_NODES; node++)
		for (link = 0; link < 4; link++)
			xlp_pcib_hardware_swap_enable(node, link);

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

	switch (irq) {
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
mips_platform_pcib_setup_intr(device_t dev, device_t child,
    struct resource *irq, int flags, driver_filter_t *filt,
    driver_intr_t *intr, void *arg, void **cookiep)
{
	int error = 0;
	int xlpirq;
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
	if (xlpirq == 0)
		return (0);

	if (strcmp(device_get_name(dev), "pcib") != 0)
		return (0);

	/* 
	 * temporary hack for MSI, we support just one device per
	 * link, and assign the link interrupt to the device interrupt
	 */
	if (xlpirq >= 64) {
		int node, val, link;
		uint64_t base;

		xlpirq -= 64;
		if (xlpirq % 32 != 0)
			return (0);

		node = nlm_nodeid();
		link = xlpirq / 32;
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
		nlm_write_pci_reg(base, PCIE_BRIDGE_MSI_ADDRL,
		    MSI_MIPS_ADDR_BASE);
		nlm_write_pci_reg(base, PCIE_BRIDGE_MSI_ADDRH, 0);

		val = nlm_read_pci_reg(base, PCIE_BRIDGE_MSI_CAP);
		/* MSI capability enable at bridge */
		nlm_write_pci_reg(base, PCIE_BRIDGE_MSI_CAP, 
		    (val | (PCIM_MSICTRL_MSI_ENABLE << 16) |
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
	if (xlpirq >= PIC_PCIE_0_IRQ && xlpirq <= PIC_PCIE_3_IRQ)
		extra_ack = bridge_pcie_ack;
	xlp_establish_intr(device_get_name(child), filt,
	    intr, arg, xlpirq, flags, cookiep, extra_ack);

	return (0);
}

static int
mips_platform_pcib_teardown_intr(device_t dev, device_t child,
    struct resource *irq, void *cookie)
{
	if (strcmp(device_get_name(child), "pci") == 0) {
		/* if needed reprogram the pic to clear pcix related entry */
		device_printf(dev, "teardown intr\n");
	}
	return (bus_generic_teardown_intr(dev, child, irq, cookie));
}

static struct resource *
xlp_pcib_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct rman *rm = NULL;
	struct resource *rv;
	void *va;
	int needactivate = flags & RF_ACTIVE;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &irq_rman;
		break;
	
	case SYS_RES_IOPORT:
		rm = &port_rman;
		break;

	case SYS_RES_MEMORY:
		if (start >= EMUL_MEM_START && start <= EMUL_MEM_END)
			rm = &emul_rman;
		else
			rm = &mem_rman;
			break;

	default:
		return (0);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);

	rman_set_rid(rv, *rid);

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		va = pmap_mapdev(start, count);
		rman_set_bushandle(rv, (bus_space_handle_t)va);
		rman_set_bustag(rv, rmi_bus_space);
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
xlp_pcib_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_release_resource(r));
}

static int
xlp_pcib_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_activate_resource(r));
}

static int
xlp_pcib_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_deactivate_resource(r));
}

static int
mips_pcib_route_interrupt(device_t bus, device_t dev, int pin)
{
	int irt, link;

	/*
	 * Validate requested pin number.
	 */
	if ((pin < 1) || (pin > 4))
		return (255);

	if (pci_get_bus(dev) == 0 &&
	    pci_get_vendor(dev) == PCI_VENDOR_NETLOGIC) {
		/* SoC devices */
		uint64_t pcibase;
		int f, n, d, num;

		f = pci_get_function(dev);
		n = pci_get_slot(dev) / 8;
		d = pci_get_slot(dev) % 8;

		/*
		 * For PCIe links, return link IRT, for other SoC devices
		 * get the IRT from its PCIe header
		 */
		if (d == 1) {
			irt = xlp_pcie_link_irt(f);
		} else {
			pcibase = nlm_pcicfg_base(XLP_HDR_OFFSET(n, 0, d, f));
			irt = nlm_irtstart(pcibase);
			num = nlm_irtnum(pcibase);
			if (num != 1)
				device_printf(bus, "[%d:%d:%d] Error %d IRQs\n",
				    n, d, f, num);
		}
	} else {
		/* Regular PCI devices */
		link = xlp_pcie_link(bus, dev);
		irt = xlp_pcie_link_irt(link);
	}

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
	DEVMETHOD(bus_read_ivar, xlp_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar, xlp_pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource, xlp_pcib_alloc_resource),
	DEVMETHOD(bus_release_resource, xlp_pcib_release_resource),
	DEVMETHOD(bus_activate_resource, xlp_pcib_activate_resource),
	DEVMETHOD(bus_deactivate_resource, xlp_pcib_deactivate_resource),
	DEVMETHOD(bus_setup_intr, mips_platform_pcib_setup_intr),
	DEVMETHOD(bus_teardown_intr, mips_platform_pcib_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots, xlp_pcib_maxslots),
	DEVMETHOD(pcib_read_config, xlp_pcib_read_config),
	DEVMETHOD(pcib_write_config, xlp_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt, mips_pcib_route_interrupt),

	DEVMETHOD(pcib_alloc_msi, xlp_alloc_msi),
	DEVMETHOD(pcib_release_msi, xlp_release_msi),
	DEVMETHOD(pcib_map_msi, xlp_map_msi),

	DEVMETHOD_END
};

static driver_t xlp_pcib_driver = {
	"pcib",
	xlp_pcib_methods,
	1, /* no softc */
};

DRIVER_MODULE(pcib, nexus, xlp_pcib_driver, pcib_devclass, 0, 0);
