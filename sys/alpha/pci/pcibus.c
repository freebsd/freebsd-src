/*
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
 *
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/interrupt.h>
#include <sys/sysctl.h>
#include <sys/rman.h>

#include <pci/pcivar.h>
#include <machine/chipset.h>
#include <machine/cpuconf.h>
#include <machine/resource.h>
#include <machine/md_var.h>
#include <alpha/pci/pcibus.h>
#include <alpha/isa/isavar.h>

#include "alphapci_if.h"
#include "isa.h"

#define	ISA_IRQ_OFFSET	0xe0
#define	ISA_IRQ_LEN	0x10

struct alpha_busspace *busspace_isa_io;
struct alpha_busspace *busspace_isa_mem;

char chipset_type[10];
int chipset_bwx = 0;
long chipset_ports = 0;
long chipset_memory = 0;
long chipset_dense = 0;
long chipset_hae_mask = 0;

SYSCTL_NODE(_hw, OID_AUTO, chipset, CTLFLAG_RW, 0, "PCI chipset information");
SYSCTL_STRING(_hw_chipset, OID_AUTO, type, CTLFLAG_RD, chipset_type, 0,
	      "PCI chipset type");
SYSCTL_INT(_hw_chipset, OID_AUTO, bwx, CTLFLAG_RD, &chipset_bwx, 0,
	   "PCI chipset supports BWX access");
SYSCTL_LONG(_hw_chipset, OID_AUTO, ports, CTLFLAG_RD, &chipset_ports, 0,
	    "PCI chipset port address");
SYSCTL_LONG(_hw_chipset, OID_AUTO, memory, CTLFLAG_RD, &chipset_memory, 0,
	    "PCI chipset memory address");
SYSCTL_LONG(_hw_chipset, OID_AUTO, dense, CTLFLAG_RD, &chipset_dense, 0,
	    "PCI chipset dense memory address");
SYSCTL_LONG(_hw_chipset, OID_AUTO, hae_mask, CTLFLAG_RD, &chipset_hae_mask, 0,
	    "PCI chipset mask for HAE register");

void
alpha_platform_assign_pciintr(pcicfgregs *cfg)
{
	if(platform.pci_intr_map)
		platform.pci_intr_map((void *)cfg);
}

#if	NISA > 0
struct resource *
alpha_platform_alloc_ide_intr(int chan)
{
	int irqs[2] = { 14, 15 };
	return isa_alloc_intr(0, 0, irqs[chan]);
}

int
alpha_platform_release_ide_intr(int chan, struct resource *res)
{
	return isa_release_intr(0, 0, res);
}

int
alpha_platform_setup_ide_intr(device_t dev,
			      struct resource *res,
			      driver_intr_t *fn, void *arg,
			      void **cookiep)
{
	return isa_setup_intr(0, dev, res, INTR_TYPE_BIO, fn, arg, cookiep);
}

int
alpha_platform_teardown_ide_intr(device_t dev,
				 struct resource *res, void *cookie)
{
	return isa_teardown_intr(0, dev, res, cookie);
}
#else
struct resource *
alpha_platform_alloc_ide_intr(int chan)
{
	return (NULL);
}
int
alpha_platform_release_ide_intr(int chan, struct resource *res)
{
	return (ENXIO);
}

int
alpha_platform_setup_ide_intr(struct resource *res,
    driver_intr_t *fn, void *arg, void **cookiep)
{
	return (ENXIO);
}

int
alpha_platform_teardown_ide_intr(struct resource *res, void *cookie)
{
	return (ENXIO);
}
#endif

static struct rman irq_rman, port_rman, mem_rman;

int
alpha_platform_pci_setup_intr(device_t dev, device_t child,
			      struct resource *irq,  int flags,
			      driver_intr_t *intr, void *arg,
			      void **cookiep)
{
#if	NISA > 0
	/*
	 * XXX - If we aren't the resource manager for this IRQ, assume that
	 * it is actually handled by the ISA PIC.
	 */
	if(irq->r_rm != &irq_rman)
		return isa_setup_intr(dev, child, irq, flags, intr, arg,
				      cookiep);
	else
#endif
		return bus_generic_setup_intr(dev, child, irq, flags, intr,
					      arg, cookiep);
}

int
alpha_platform_pci_teardown_intr(device_t dev, device_t child,
				 struct resource *irq, void *cookie)
{
#if	NISA > 0
	/*
	 * XXX - If we aren't the resource manager for this IRQ, assume that
	 * it is actually handled by the ISA PIC.
	 */
	if(irq->r_rm != &irq_rman)
		return isa_teardown_intr(dev, child, irq, cookie);
	else
#endif
		return bus_generic_teardown_intr(dev, child, irq, cookie);
}

void 
pci_init_resources(void)
{
	irq_rman.rm_start = 0;
	irq_rman.rm_end = 65536;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "PCI Mapped Interrupts";
	if (rman_init(&irq_rman)
	    || rman_manage_region(&irq_rman, 0, 65536))
		panic("pci_init_resources irq_rman");

	port_rman.rm_start = 0;
	port_rman.rm_end = ~0u;
	port_rman.rm_type = RMAN_ARRAY;
	port_rman.rm_descr = "I/O ports";
	if (rman_init(&port_rman)
	    || rman_manage_region(&port_rman, 0x0, (1L << 32)))
		panic("pci_init_resources port_rman");

	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0u;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory";
	if (rman_init(&mem_rman)
	    || rman_manage_region(&mem_rman, 0x0, (1L << 32)))
		panic("pci_init_resources mem_rman");
}

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of the pci chipset.
 */
struct resource *
pci_alloc_resource(device_t bus, device_t child, int type, int *rid,
		   u_long start, u_long end, u_long count, u_int flags)
{
	struct	rman *rm;
	struct	resource *rv;
	void *va;

	switch (type) {
	case SYS_RES_IRQ:
#if NISA > 0
		if((start >= ISA_IRQ_OFFSET) &&
		   (end < ISA_IRQ_OFFSET + ISA_IRQ_LEN)) {
		  	return isa_alloc_intrs(bus, child,
					       start - ISA_IRQ_OFFSET,
					       end - ISA_IRQ_OFFSET);
		}
		else
#endif
			rm = &irq_rman;
		break;

	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		rm = ALPHAPCI_GET_RMAN(bus, type);
		break;

	default:
		return 0;
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == 0)
		return 0;

	rman_set_bustag(rv, ALPHAPCI_GET_BUSTAG(bus, type));
	rman_set_bushandle(rv, rv->r_start);
	switch (type) {
	case SYS_RES_MEMORY:
		va = 0;
		if (flags & PCI_RF_DENSE)
			va = ALPHAPCI_CVT_DENSE(bus, rv->r_start);
		else if (flags & PCI_RF_BWX)
			va = ALPHAPCI_CVT_BWX(bus, rv->r_start);
		else
			va = (void *) rv->r_start; /* maybe NULL? */
		rman_set_virtual(rv, va);

		break;
	}

	return rv;
}

int
pci_activate_resource(device_t bus, device_t child, int type, int rid,
		      struct resource *r)
{
	return (rman_activate_resource(r));
}

int
pci_deactivate_resource(device_t bus, device_t child, int type, int rid,
			  struct resource *r)
{
	return (rman_deactivate_resource(r));
}

int
pci_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	return (rman_release_resource(r));
}

struct alpha_busspace *
pci_get_bustag(device_t dev, int type)
{
	switch (type) {
	case SYS_RES_IOPORT:
		return busspace_isa_io;

	case SYS_RES_MEMORY:
		return busspace_isa_mem;
	}

	return 0;
}

struct rman *
pci_get_rman(device_t dev, int type)
{
	switch (type) {
	case SYS_RES_IOPORT:
		return &port_rman;

	case SYS_RES_MEMORY:
		return &mem_rman;
	}

	return 0;
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_COMMAND(in, db_in)
{
    int c;
    int size;

    if (!have_addr)
	return;

    size = -1;
    while ((c = *modif++) != '\0') {
	switch (c) {
	case 'b':
	    size = 1;
	    break;
	case 'w':
	    size = 2;
	    break;
	case 'l':
	    size = 4;
	    break;
	}
    }

    if (size < 0) {
	db_printf("bad size\n");
	return;
    }

    if (count <= 0) count = 1;
    while (--count >= 0) {
	db_printf("%08lx:\t", addr);
	switch (size) {
	case 1:
	    db_printf("%02x\n", inb(addr));
	    break;
	case 2:
	    db_printf("%04x\n", inw(addr));
	    break;
	case 4:
	    db_printf("%08x\n", inl(addr));
	    break;
	}
    }
}

#endif
