/*-
 * Copyright (c) 2009 Neelkanth Natu
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
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/pcpu.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>

#include <machine/resource.h>
#include <machine/bus.h>

#include "pcib_if.h"

#include "sb_bus_space.h"
#include "sb_scd.h"

__FBSDID("$FreeBSD$");

static struct {
	vm_offset_t vaddr;
	vm_paddr_t  paddr;
} zbpci_config_space[MAXCPU];

static const vm_paddr_t CFG_PADDR_BASE = 0xFE000000;
static const u_long PCI_IOSPACE_ADDR = 0xFC000000;
static const u_long PCI_IOSPACE_SIZE = 0x02000000;

#define	PCI_MATCH_BYTE_LANES_START	0x40000000
#define	PCI_MATCH_BYTE_LANES_END	0x5FFFFFFF
#define	PCI_MATCH_BYTE_LANES_SIZE	0x20000000

#define	PCI_MATCH_BIT_LANES_MASK	(1 << 29)
#define	PCI_MATCH_BIT_LANES_START	0x60000000
#define	PCI_MATCH_BIT_LANES_END		0x7FFFFFFF
#define	PCI_MATCH_BIT_LANES_SIZE	0x20000000

static struct rman port_rman;

static int
zbpci_probe(device_t dev)
{
	
	device_set_desc(dev, "Broadcom/Sibyte PCI I/O Bridge");
	return (0);
}

static int
zbpci_attach(device_t dev)
{
	int n, rid, size;
	vm_offset_t va;
	struct resource *res;
	
	/*
	 * Reserve the physical memory window used to map PCI I/O space.
	 */
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				 PCI_IOSPACE_ADDR,
				 PCI_IOSPACE_ADDR + PCI_IOSPACE_SIZE - 1,
				 PCI_IOSPACE_SIZE, 0);
	if (res == NULL)
		panic("Cannot allocate resource for PCI I/O space mapping.");

	port_rman.rm_start = 0;
	port_rman.rm_end = PCI_IOSPACE_SIZE - 1;
	port_rman.rm_type = RMAN_ARRAY;
	port_rman.rm_descr = "PCI I/O ports";
	if (rman_init(&port_rman) != 0 ||
	    rman_manage_region(&port_rman, 0, PCI_IOSPACE_SIZE - 1) != 0)
		panic("%s: port_rman", __func__);

	/*
	 * Reserve the physical memory that is used to read/write to the
	 * pci config space but don't activate it. We are using a page worth
	 * of KVA as a window over this region.
	 */
	rid = 1;
	size = (PCI_BUSMAX + 1) * (PCI_SLOTMAX + 1) * (PCI_FUNCMAX + 1) * 256;
	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, CFG_PADDR_BASE,
				 CFG_PADDR_BASE + size - 1, size, 0);
	if (res == NULL)
		panic("Cannot allocate resource for config space accesses.");

	/*
	 * Allocate the entire "match bit lanes" address space.
	 */
#if _BYTE_ORDER == _BIG_ENDIAN
	rid = 2;
	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 
				 PCI_MATCH_BIT_LANES_START,
				 PCI_MATCH_BIT_LANES_END,
				 PCI_MATCH_BIT_LANES_SIZE, 0);
	if (res == NULL)
		panic("Cannot allocate resource for pci match bit lanes.");
#endif	/* _BYTE_ORDER ==_BIG_ENDIAN */

	/*
	 * Allocate KVA for accessing PCI config space.
	 */
	va = kva_alloc(PAGE_SIZE * mp_ncpus);
	if (va == 0) {
		device_printf(dev, "Cannot allocate virtual addresses for "
				   "config space access.\n");
		return (ENOMEM);
	}

	for (n = 0; n < mp_ncpus; ++n)
		zbpci_config_space[n].vaddr = va + n * PAGE_SIZE;

	/*
	 * Sibyte has the PCI bus hierarchy rooted at bus 0 and HT-PCI
	 * hierarchy rooted at bus 1.
	 */
	if (device_add_child(dev, "pci", 0) == NULL)
		panic("zbpci_attach: could not add pci bus 0.\n");

	if (device_add_child(dev, "pci", 1) == NULL)
		panic("zbpci_attach: could not add pci bus 1.\n");

	if (bootverbose)
		device_printf(dev, "attached.\n");

	return (bus_generic_attach(dev));
}

static struct resource *
zbpci_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource *res;

	/*
	 * Handle PCI I/O port resources here and pass everything else to nexus.
	 */
	if (type != SYS_RES_IOPORT) {
		res = bus_generic_alloc_resource(bus, child, type, rid,
						 start, end, count, flags);
		return (res);
	}

	res = rman_reserve_resource(&port_rman, start, end, count,
				    flags, child);
	if (res == NULL)
		return (NULL);

	rman_set_rid(res, *rid);

	/* Activate the resource is requested */
	if (flags & RF_ACTIVE) {
		if (bus_activate_resource(child, type, *rid, res) != 0) {
			rman_release_resource(res);
			return (NULL);
		}
	}

	return (res);
}

static int
zbpci_activate_resource(device_t bus, device_t child, int type, int rid,
			struct resource *res)
{
	int error;
	void *vaddr;
	u_long orig_paddr, paddr, psize;

	paddr = rman_get_start(res);
	psize = rman_get_size(res);
	orig_paddr = paddr;

#if _BYTE_ORDER == _BIG_ENDIAN
	/*
	 * The CFE allocates PCI memory resources that map to the
	 * "match byte lanes" address space. This address space works
	 * best for DMA transfers because it does not do any automatic
	 * byte swaps when data crosses the pci-cpu interface.
	 *
	 * This also makes it sub-optimal for accesses to PCI device
	 * registers because it exposes the little-endian nature of
	 * the PCI bus to the big-endian CPU. The Sibyte has another
	 * address window called the "match bit lanes" window which
	 * automatically swaps bytes when data crosses the pci-cpu
	 * interface.
	 *
	 * We "assume" that any bus_space memory accesses done by the
	 * CPU to a PCI device are register/configuration accesses and
	 * are done through the "match bit lanes" window. Any DMA
	 * transfers will continue to be through the "match byte lanes"
	 * window because the PCI BAR registers will not be changed.
	 */
	if (type == SYS_RES_MEMORY) {
		if (paddr >= PCI_MATCH_BYTE_LANES_START &&
		    paddr + psize - 1 <= PCI_MATCH_BYTE_LANES_END) {
			paddr |= PCI_MATCH_BIT_LANES_MASK;
			rman_set_start(res, paddr);
			rman_set_end(res, paddr + psize - 1);
		}
	}
#endif

	if (type != SYS_RES_IOPORT) {
		error = bus_generic_activate_resource(bus, child, type,
						      rid, res);
#if _BYTE_ORDER == _BIG_ENDIAN
		if (type == SYS_RES_MEMORY) {
			rman_set_start(res, orig_paddr);
			rman_set_end(res, orig_paddr + psize - 1);
		}
#endif
		return (error);
	}

	/*
	 * Map the I/O space resource through the memory window starting
	 * at PCI_IOSPACE_ADDR.
	 */
	vaddr = pmap_mapdev(paddr + PCI_IOSPACE_ADDR, psize);

	rman_set_virtual(res, vaddr);
	rman_set_bustag(res, mips_bus_space_generic);
	rman_set_bushandle(res, (bus_space_handle_t)vaddr);

	return (rman_activate_resource(res));
}

static int
zbpci_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	int error;

	if (type != SYS_RES_IOPORT)
		return (bus_generic_release_resource(bus, child, type, rid, r));

	if (rman_get_flags(r) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return (error);
	}

	return (rman_release_resource(r));
}

static int
zbpci_deactivate_resource(device_t bus, device_t child, int type, int rid,
			  struct resource *r)
{
	vm_offset_t va;

	if (type != SYS_RES_IOPORT) {
		return (bus_generic_deactivate_resource(bus, child, type,
							rid, r));
	}
	
	va = (vm_offset_t)rman_get_virtual(r);
	pmap_unmapdev(va, rman_get_size(r));

	return (rman_deactivate_resource(r));
}

static int
zbpci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = 0;				/* single PCI domain */
		return (0);
	case PCIB_IVAR_BUS:
		*result = device_get_unit(child);	/* PCI bus 0 or 1 */
		return (0);
	default:
		return (ENOENT);
	}
}

/*
 * We rely on the CFE to have configured the intline correctly to point to
 * one of PCI-A/PCI-B/PCI-C/PCI-D in the interupt mapper.
 */
static int
zbpci_route_interrupt(device_t pcib, device_t dev, int pin)
{

	return (PCI_INVALID_IRQ);
}

/*
 * This function is expected to be called in a critical section since it
 * changes the per-cpu pci config space va-to-pa mappings.
 */
static vm_offset_t
zbpci_config_space_va(int bus, int slot, int func, int reg, int bytes)
{
	int cpu;
	vm_offset_t va_page;
	vm_paddr_t pa, pa_page;

	if (bus <= PCI_BUSMAX && slot <= PCI_SLOTMAX && func <= PCI_FUNCMAX &&
	    reg <= PCI_REGMAX && (bytes == 1 || bytes == 2 || bytes == 4) &&
	    ((reg & (bytes - 1)) == 0)) {
		cpu = PCPU_GET(cpuid);
		va_page = zbpci_config_space[cpu].vaddr;
		pa = CFG_PADDR_BASE |
		     (bus << 16) | (slot << 11) | (func << 8) | reg;
#if _BYTE_ORDER == _BIG_ENDIAN
		pa = pa ^ (4 - bytes);
#endif
		pa_page = rounddown2(pa, PAGE_SIZE);
		if (zbpci_config_space[cpu].paddr != pa_page) {
			pmap_kremove(va_page);
			pmap_kenter_attr(va_page, pa_page, PTE_C_UNCACHED);
			zbpci_config_space[cpu].paddr = pa_page;
		}
		return (va_page + (pa - pa_page));
	} else {
		return (0);
	}
}

static uint32_t
zbpci_read_config(device_t dev, u_int b, u_int s, u_int f, u_int r, int w)
{
	uint32_t data;
	vm_offset_t va;

	critical_enter();

	va = zbpci_config_space_va(b, s, f, r, w);
	if (va == 0) {
		panic("zbpci_read_config: invalid %d/%d/%d[%d] %d\n",
		      b, s, f, r, w);
	}

	switch (w) {
	case 4:
		data = *(uint32_t *)va;
		break;
	case 2:
		data = *(uint16_t *)va;
		break;
	case 1:
		data = *(uint8_t *)va;
		break;
	default:
		panic("zbpci_read_config: invalid width %d\n", w);
	}

	critical_exit();

	return (data);
}

static void
zbpci_write_config(device_t d, u_int b, u_int s, u_int f, u_int r,
		   uint32_t data, int w)
{
	vm_offset_t va;

	critical_enter();

	va = zbpci_config_space_va(b, s, f, r, w);
	if (va == 0) {
		panic("zbpci_write_config: invalid %d/%d/%d[%d] %d/%d\n",
		      b, s, f, r, data, w);
	}

	switch (w) {
	case 4:
		*(uint32_t *)va = data;
		break;
	case 2:
		*(uint16_t *)va = data;
		break;
	case 1:
		*(uint8_t *)va = data;
		break;
	default:
		panic("zbpci_write_config: invalid width %d\n", w);
	}

	critical_exit();
}

static device_method_t zbpci_methods[] ={
	/* Device interface */
	DEVMETHOD(device_probe,		zbpci_probe),
	DEVMETHOD(device_attach,	zbpci_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	zbpci_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_alloc_resource,	zbpci_alloc_resource),
	DEVMETHOD(bus_activate_resource, zbpci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, zbpci_deactivate_resource),
	DEVMETHOD(bus_release_resource,	zbpci_release_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_add_child,	bus_generic_add_child),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	pcib_maxslots),
	DEVMETHOD(pcib_read_config,	zbpci_read_config),
	DEVMETHOD(pcib_write_config,	zbpci_write_config),
	DEVMETHOD(pcib_route_interrupt,	zbpci_route_interrupt),
	
	{ 0, 0 }
};

/*
 * The "zbpci" class inherits from the "pcib" base class. Therefore in
 * addition to drivers that belong to the "zbpci" class we will also
 * consider drivers belonging to the "pcib" when probing children of
 * "zbpci".
 */
DEFINE_CLASS_1(zbpci, zbpci_driver, zbpci_methods, 0, pcib_driver);

static devclass_t zbpci_devclass;

DRIVER_MODULE(zbpci, zbbus, zbpci_driver, zbpci_devclass, 0, 0);

/*
 * Big endian bus space routines
 */
#if _BYTE_ORDER == _BIG_ENDIAN

/*
 * The CPU correctly deals with the big-endian to little-endian swap if
 * we are accessing 4 bytes at a time. However if we want to read 1 or 2
 * bytes then we need to fudge the address generated by the CPU such that
 * it generates the right byte enables on the PCI bus.
 */
static bus_addr_t
sb_match_bit_lane_addr(bus_addr_t addr, int bytes)
{
	vm_offset_t pa;

	pa = vtophys(addr);
	
	if (pa >= PCI_MATCH_BIT_LANES_START && pa <= PCI_MATCH_BIT_LANES_END)
		return (addr ^ (4 - bytes));
	else
		return (addr);
}

uint8_t
sb_big_endian_read8(bus_addr_t addr)
{
	bus_addr_t addr2;

	addr2 = sb_match_bit_lane_addr(addr, 1);
	return (readb(addr2));
}

uint16_t
sb_big_endian_read16(bus_addr_t addr)
{
	bus_addr_t addr2;

	addr2 = sb_match_bit_lane_addr(addr, 2);
	return (readw(addr2));
}

uint32_t
sb_big_endian_read32(bus_addr_t addr)
{
	bus_addr_t addr2;

	addr2 = sb_match_bit_lane_addr(addr, 4);
	return (readl(addr2));
}

void
sb_big_endian_write8(bus_addr_t addr, uint8_t val)
{
	bus_addr_t addr2;

	addr2 = sb_match_bit_lane_addr(addr, 1);
	writeb(addr2, val);
}

void
sb_big_endian_write16(bus_addr_t addr, uint16_t val)
{
	bus_addr_t addr2;

	addr2 = sb_match_bit_lane_addr(addr, 2);
	writew(addr2, val);
}

void
sb_big_endian_write32(bus_addr_t addr, uint32_t val)
{
	bus_addr_t addr2;

	addr2 = sb_match_bit_lane_addr(addr, 4);
	writel(addr2, val);
}
#endif	/* _BIG_ENDIAN */
