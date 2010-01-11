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

#include <machine/pmap.h>
#include <machine/resource.h>

#include "pcib_if.h"

#include "sb_scd.h"

__FBSDID("$FreeBSD$");

static struct {
	vm_offset_t vaddr;
	vm_paddr_t  paddr;
} zbpci_config_space[MAXCPU];

static const vm_paddr_t CFG_PADDR_BASE = 0xFE000000;
	
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
	 * Reserve the the physical memory that is used to read/write to the
	 * pci config space but don't activate it. We are using a page worth
	 * of KVA as a window over this region.
	 */
	rid = 0;
	size = (PCI_BUSMAX + 1) * (PCI_SLOTMAX + 1) * (PCI_FUNCMAX + 1) * 256;
	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, CFG_PADDR_BASE,
				 CFG_PADDR_BASE + size - 1, size, 0);
	if (res == NULL) {
		panic("Cannot allocate resource for config space accesses.");
	}

	/*
	 * Allocate KVA for accessing PCI config space.
	 */
	va = kmem_alloc_nofault(kernel_map, PAGE_SIZE * mp_ncpus);
	if (va == 0) {
		device_printf(dev, "Cannot allocate virtual addresses for "
				   "config space access.\n");
		return (ENOMEM);
	}

	for (n = 0; n < mp_ncpus; ++n) {
		zbpci_config_space[n].vaddr = va + n * PAGE_SIZE;
	}

	/*
	 * Sibyte has the PCI bus hierarchy rooted at bus 0 and HT-PCI
	 * hierarchy rooted at bus 1.
	 */
	if (device_add_child(dev, "pci", 0) == NULL) {
		panic("zbpci_attach: could not add pci bus 0.\n");
	}

	if (device_add_child(dev, "pci", 1) == NULL) {
		panic("zbpci_attach: could not add pci bus 1.\n");
	}

	if (bootverbose) {
		device_printf(dev, "attached.\n");
	}

	return (bus_generic_attach(dev));
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
		pa_page = pa & ~(PAGE_SIZE - 1);
		if (zbpci_config_space[cpu].paddr != pa_page) {
			pmap_kremove(va_page);
			pmap_kenter(va_page, pa_page);
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
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
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
DECLARE_CLASS(pcib_driver);
DEFINE_CLASS_1(zbpci, zbpci_driver, zbpci_methods, 0, pcib_driver);

static devclass_t zbpci_devclass;

DRIVER_MODULE(zbpci, zbbus, zbpci_driver, zbpci_devclass, 0, 0);
