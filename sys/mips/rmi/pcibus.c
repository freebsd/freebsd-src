/*-
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_isa.h"

#define __RMAN_RESOURCE_VISIBLE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/pmap.h>
#include <sys/interrupt.h>
#include <sys/sysctl.h>
#include <mips/rmi/iomap.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/shared_structs.h>
#include <mips/rmi/board.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <machine/resource.h>
#include <machine/md_var.h>
#include <machine/intr_machdep.h>
#include <mips/rmi/pcibus.h>
/*
static void bridge_pcix_ack(void *);
static void bridge_pcie_ack(void *);
static void pic_pcix_ack(void *);
static void pic_pcie_ack(void *);
*/

extern vm_map_t kernel_map;
vm_offset_t kmem_alloc_nofault(vm_map_t map, vm_size_t size);


int
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

static struct rman irq_rman, port_rman, mem_rman;

/*
static void bridge_pcix_ack(void *arg)
{
	xlr_read_reg(xlr_io_mmio(XLR_IO_PCIX_OFFSET), 0x140 >> 2);
	}
*/
/*
static void bridge_pcie_ack(void *arg)
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
*/
/*
static void pic_pcix_ack(void *none)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	
	mtx_lock_spin(&xlr_pic_lock);
	xlr_write_reg(mmio, PIC_INT_ACK, (1 << PIC_IRT_PCIX_INDEX));
	mtx_unlock_spin(&xlr_pic_lock);
}
*/
/*
static void pic_pcie_ack(void *arg)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	int irq = (int) arg;

	mtx_lock_spin(&xlr_pic_lock);
	xlr_write_reg(mmio, PIC_INT_ACK, (1 << (irq - PIC_IRQ_BASE)));
	mtx_unlock_spin(&xlr_pic_lock);
}

*/

int
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
	
		if (rmi_spin_mutex_safe) mtx_lock_spin(&xlr_pic_lock);
		level = PIC_IRQ_IS_EDGE_TRIGGERED(PIC_IRT_PCIX_INDEX);
		xlr_write_reg(mmio, PIC_IRT_0_PCIX, 0x01);
		xlr_write_reg(mmio, PIC_IRT_1_PCIX, ((1 << 31) | (level << 30) |
		    (1 << 6) | (PIC_PCIX_IRQ)));
		if (rmi_spin_mutex_safe) mtx_unlock_spin(&xlr_pic_lock);
		cpu_establish_hardintr(device_get_name(child), filt,
		    (driver_intr_t *) intr, (void *)arg, PIC_PCIX_IRQ, flags, cookiep);

	} else {
		if (rmi_spin_mutex_safe) mtx_lock_spin(&xlr_pic_lock);
		xlr_write_reg(mmio, PIC_IRT_0_BASE + xlrirq - PIC_IRQ_BASE, 0x01);
		xlr_write_reg(mmio, PIC_IRT_1_BASE + xlrirq - PIC_IRQ_BASE,
		    ((1 << 31) | (1 << 30) | (1 << 6) | xlrirq));
		if (rmi_spin_mutex_safe) mtx_unlock_spin(&xlr_pic_lock);

		if (flags & INTR_FAST)
			cpu_establish_hardintr(device_get_name(child), filt,
			    (driver_intr_t *) intr, (void *)arg, xlrirq, flags, cookiep);
		else
			cpu_establish_hardintr(device_get_name(child), filt,
			    (driver_intr_t *) intr, (void *)arg, xlrirq, flags, cookiep);


	}
	return bus_generic_setup_intr(dev, child, irq, flags, filt, intr,
	    arg, cookiep);
}


int
mips_platform_pci_teardown_intr(device_t dev, device_t child,
    struct resource *irq, void *cookie);
int
mips_platform_pci_teardown_intr(device_t dev, device_t child,
    struct resource *irq, void *cookie)
{
	if (strcmp(device_get_name(child), "pci") == 0) {
		/* if needed reprogram the pic to clear pcix related entry */
	}
	return bus_generic_teardown_intr(dev, child, irq, cookie);
}

void
pci_init_resources(void)
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

/* hack from bus.h in mips/include/bus.h */
#ifndef MIPS_BUS_SPACE_PCI
#define MIPS_BUS_SPACE_PCI 10
#endif

struct resource *
xlr_pci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct rman *rm;
	struct resource *rv;
	vm_offset_t va;
	int needactivate = flags & RF_ACTIVE;

#if 0
	device_printf(bus, "xlr_pci_alloc_resource : child %s, type %d, start %lx end %lx, count %lx, flags %x\n",
	    device_get_nameunit(child), type, start, end, count, flags);
#endif

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

	rman_set_bustag(rv, (bus_space_tag_t) MIPS_BUS_SPACE_PCI);
	rman_set_rid(rv, *rid);

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		/*
		 * if ((start + count) > (2 << 28)) { va_start =
		 * kmem_alloc_nofault(kernel_map, count); }
		 */
		/*
		 * This called for pmap_map_uncached, but the pmap_map calls
		 * pmap_kenter which does a is_cacheable_mem() check and
		 * thus sets the PTE_UNCACHED bit. Hopefully this will work
		 * for this guy... RRS
		 */
		/* va = pmap_map(&va_start, start, start + count, 0); */
		va = (vm_offset_t)pmap_mapdev(start, start + count);
		rman_set_bushandle(rv, va);
		/* bushandle is same as virtual addr */
		rman_set_virtual(rv, (void *)va);
		rman_set_bustag(rv, (bus_space_tag_t) MIPS_BUS_SPACE_PCI);
	}
	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}
	return rv;
}


int
pci_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	return (rman_deactivate_resource(r));
}

/* now in pci.c
int
pci_activate_resource(device_t bus, device_t child, int type, int rid,
		      struct resource *r)
{
	return (rman_activate_resource(r));
}

int
pci_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	return (rman_release_resource(r));
}
*/

struct rman *
pci_get_rman(device_t dev, int type)
{
	switch (type) {
		case SYS_RES_IOPORT:
		return &port_rman;

	case SYS_RES_MEMORY:
		return &mem_rman;

	case SYS_RES_IRQ:
		return &irq_rman;
	}

	return 0;
}
