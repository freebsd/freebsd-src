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
 * $FreeBSD: src/sys/pci/pci_compat.c,v 1.35 2000/02/28 08:12:24 bde Exp $
 *
 */

#include "opt_bus.h"

/* for compatibility to FreeBSD-2.2 version of PCI code */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <sys/interrupt.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#ifdef APIC_IO
#include <machine/smp.h>
#endif

#ifdef __i386__
#include <i386/isa/intr_machdep.h>
#endif


#ifdef PCI_COMPAT

/* ------------------------------------------------------------------------- */

u_long
pci_conf_read(pcici_t cfg, u_long reg)
{
	return (pci_read_config(cfg->dev, reg, 4));
}

void
pci_conf_write(pcici_t cfg, u_long reg, u_long data)
{
	pci_write_config(cfg->dev, reg, data, 4);
}

int
pci_map_port(pcici_t cfg, u_long reg, pci_port_t* pa)
{
	int rid;
	struct resource *res;

	rid = reg;
	res = bus_alloc_resource(cfg->dev, SYS_RES_IOPORT, &rid,
				 0, ~0, 1, RF_ACTIVE);
	if (res) {
		*pa = rman_get_start(res);
		return (1);
	}
	return (0);
}

int
pci_map_mem(pcici_t cfg, u_long reg, vm_offset_t* va, vm_offset_t* pa)
{
	int rid;
	struct resource *res;

	rid = reg;
	res = bus_alloc_resource(cfg->dev, SYS_RES_MEMORY, &rid,
				 0, ~0, 1, RF_ACTIVE);
	if (res) {
		*pa = rman_get_start(res);
		*va = (vm_offset_t) rman_get_virtual(res);
		return (1);
	}
	return (0);
}

int
pci_map_int(pcici_t cfg, pci_inthand_t *handler, void *arg, intrmask_t *maskptr)
{
	return (pci_map_int_right(cfg, handler, arg, maskptr, 0));
}

int
pci_map_int_right(pcici_t cfg, pci_inthand_t *handler, void *arg,
		  intrmask_t *maskptr, u_int intflags)
{
	int error;
#ifdef APIC_IO
	int nextpin, muxcnt;
#endif
	if (cfg->intpin != 0) {
		int irq = cfg->intline;
		int rid = 0;
		struct resource *res;
		int flags = 0;
		int resflags = RF_SHAREABLE|RF_ACTIVE;
		void *ih;

#ifdef INTR_FAST
		if (intflags & INTR_FAST)
			flags |= INTR_TYPE_FAST;
		if (intflags & INTR_EXCL)
			resflags &= ~RF_SHAREABLE;
#endif

		res = bus_alloc_resource(cfg->dev, SYS_RES_IRQ, &rid,
					 irq, irq, 1, resflags);
		if (!res) {
			printf("pci_map_int: can't allocate interrupt\n");
			return 0;
		}

		/*
		 * This is ugly. Translate the mask into an interrupt type.
		 */
		if (maskptr == &tty_imask)
			flags |= INTR_TYPE_TTY;
		else if (maskptr == &bio_imask)
			flags |= INTR_TYPE_BIO;
		else if (maskptr == &net_imask)
			flags |= INTR_TYPE_NET;
		else if (maskptr == &cam_imask)
			flags |= INTR_TYPE_CAM;

		error = BUS_SETUP_INTR(device_get_parent(cfg->dev), cfg->dev,
				       res, flags, handler, arg, &ih);
		if (error != 0)
			return 0;

#ifdef NEW_BUS_PCI
		/*
		 * XXX this apic stuff looks totally busted.  It should
		 * move to the nexus code which actually registers the
		 * interrupt.
		 */
#endif

#ifdef APIC_IO
		nextpin = next_apic_irq(irq);
		
		if (nextpin < 0)
			return 1;

		/* 
		 * Attempt handling of some broken mp tables.
		 *
		 * It's OK to yell (since the mp tables are broken).
		 * 
		 * Hanging in the boot is not OK
		 */

		muxcnt = 2;
		nextpin = next_apic_irq(nextpin);
		while (muxcnt < 5 && nextpin >= 0) {
			muxcnt++;
			nextpin = next_apic_irq(nextpin);
		}
		if (muxcnt >= 5) {
			printf("bogus MP table, more than 4 IO APIC pins connected to the same PCI device or ISA/EISA interrupt\n");
			return 0;
		}
		
		printf("bogus MP table, %d IO APIC pins connected to the same PCI device or ISA/EISA interrupt\n", muxcnt);

		nextpin = next_apic_irq(irq);
		while (nextpin >= 0) {
			rid = 0;
			res = bus_alloc_resource(cfg->dev, SYS_RES_IRQ, &rid,
						 nextpin, nextpin, 1,
						 resflags);
			if (!res) {
				printf("pci_map_int: can't allocate extra interrupt\n");
				return 0;
			}
			error = BUS_SETUP_INTR(device_get_parent(cfg->dev),
					       cfg->dev, res, flags,
					       handler, arg, &ih);
			if (error != 0) {
				printf("pci_map_int: BUS_SETUP_INTR failed\n");
				return 0;
			}
			printf("Registered extra interrupt handler for int %d (in addition to int %d)\n", nextpin, irq);
			nextpin = next_apic_irq(nextpin);
		}
#endif
	}
	return (1);
}

int
pci_unmap_int(pcici_t cfg)
{
	return (0); /* not supported, yet, since cfg doesn't know about idesc */
}

pcici_t
pci_get_parent_from_tag(pcici_t tag)
{
	return (pcici_t)pci_devlist_get_parent(tag);
}

int
pci_get_bus_from_tag(pcici_t tag)
{
	return tag->bus;
}

#endif /* PCI_COMPAT */
