/*-
 * Copyright (c) 1998 Doug Rabson
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
 *
 * $FreeBSD$
 */
/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * Additional Copyright (c) 1998 by Andrew Gallatin for Duke University 
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>
#include <alpha/pci/pcibus.h>
#include <alpha/isa/isavar.h>
#include <machine/intr.h>
#include <machine/resource.h>
#include <machine/intrcnt.h>
#include <machine/cpuconf.h>
#include <machine/swiz.h>
#include <machine/rpb.h>
#include <machine/sgmap.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	apecs_devclass;
static device_t		apecs0;		/* XXX only one for now */

struct apecs_softc {
	vm_offset_t	dmem_base;	/* dense memory */
	vm_offset_t	smem_base;	/* sparse memory */
	vm_offset_t	io_base;	/* dense i/o */
	vm_offset_t	cfg0_base;	/* dense pci0 config */
	vm_offset_t	cfg1_base;	/* dense pci1 config */
};

#define APECS_SOFTC(dev)	(struct apecs_softc*) device_get_softc(dev)

static alpha_chipset_read_hae_t	apecs_read_hae;
static alpha_chipset_write_hae_t apecs_write_hae;

static alpha_chipset_t apecs_swiz_chipset = {
	apecs_read_hae,
	apecs_write_hae,
};

/*
 * Memory functions.
 * 
 * XXX linux does 32-bit reads/writes via dense space.  This doesn't
 *     appear to work for devices behind a ppb.  I'm using sparse
 *     accesses & they appear to work just fine everywhere.
 */

static u_int32_t	apecs_hae_mem;

#define REG1 (1UL << 24)
static u_int32_t
apecs_set_hae_mem(void *arg, u_int32_t pa)
{
	int s; 
	u_int32_t msb;
	if (pa >= REG1){
		msb = pa & 0xf8000000;
		pa -= msb;
		s = splhigh();
                if (msb != apecs_hae_mem) {
			apecs_hae_mem = msb;
			REGVAL(EPIC_HAXR1) = apecs_hae_mem;
			alpha_mb();
			apecs_hae_mem = REGVAL(EPIC_HAXR1);
		}
		splx(s);
	}
	return pa;
}

static u_int64_t
apecs_read_hae(void)
{
	return apecs_hae_mem & 0xf8000000;
}

static void
apecs_write_hae(u_int64_t hae)
{
	u_int32_t pa = hae;
	apecs_set_hae_mem(0, pa);
}

static int apecs_probe(device_t dev);
static int apecs_attach(device_t dev);
static struct resource *apecs_alloc_resource(device_t bus, device_t child,
					     int type, int *rid, u_long start,
					     u_long end, u_long count,
					     u_int flags);
static int apecs_release_resource(device_t bus, device_t child,
				  int type, int rid, struct resource *r);
static int apecs_setup_intr(device_t dev, device_t child,
			    struct resource *irq, int flags,
			    driver_intr_t *intr, void *arg, void **cookiep);
static int apecs_teardown_intr(device_t dev, device_t child,
			     struct resource *irq, void *cookie);

static device_method_t apecs_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		apecs_probe),
	DEVMETHOD(device_attach,	apecs_attach),

	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,	apecs_alloc_resource),
	DEVMETHOD(bus_release_resource,	apecs_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	apecs_setup_intr),
	DEVMETHOD(bus_teardown_intr,	apecs_teardown_intr),

	{ 0, 0 }
};

static driver_t apecs_driver = {
	"apecs",
	apecs_methods,
	sizeof(struct apecs_softc),
};

#define APECS_SGMAP_BASE		(8*1024*1024)
#define APECS_SGMAP_SIZE		(8*1024*1024)

static void
apecs_sgmap_invalidate(void)
{
	alpha_mb();
	REGVAL(EPIC_TBIA) = 0;
	alpha_mb();
}

static void
apecs_sgmap_map(void *arg, bus_addr_t ba, vm_offset_t pa)
{
	u_int64_t *sgtable = arg;
	int index = alpha_btop(ba - APECS_SGMAP_BASE);

	if (pa) {
		if (pa > (1L<<32))
			panic("apecs_sgmap_map: can't map address 0x%lx", pa);
		sgtable[index] = ((pa >> 13) << 1) | 1;
	} else {
		sgtable[index] = 0;
	}
	alpha_mb();
	apecs_sgmap_invalidate();
}

static void
apecs_init_sgmap(void)
{
	void *sgtable;

	/*
	 * First setup Window 0 to map 8Mb to 16Mb with an
	 * sgmap. Allocate the map aligned to a 32 boundary.
	 */
	REGVAL(EPIC_PCI_BASE_1) = APECS_SGMAP_BASE |
		EPIC_PCI_BASE_SGEN | EPIC_PCI_BASE_WENB;
	alpha_mb();

	REGVAL(EPIC_PCI_MASK_1) = EPIC_PCI_MASK_8M;
	alpha_mb();

	sgtable = contigmalloc(8192, M_DEVBUF, M_NOWAIT,
			       0, (1L<<34),
			       32*1024, (1L<<34));
	if (!sgtable)
		panic("apecs_init_sgmap: can't allocate page table");
	REGVAL(EPIC_TBASE_1) =
		(pmap_kextract((vm_offset_t) sgtable) >> EPIC_TBASE_SHIFT);

	chipset.sgmap = sgmap_map_create(APECS_SGMAP_BASE,
					 APECS_SGMAP_BASE + APECS_SGMAP_SIZE,
					 apecs_sgmap_map, sgtable);
}

void
apecs_init()
{
	static int initted = 0;
	static struct swiz_space io_space, mem_space;

	if (initted) return;
	initted = 1;

	swiz_init_space(&io_space, KV(APECS_PCI_SIO));
	swiz_init_space_hae(&mem_space, KV(APECS_PCI_SPARSE),
			    apecs_set_hae_mem, 0);

	busspace_isa_io = (struct alpha_busspace *) &io_space;
	busspace_isa_mem = (struct alpha_busspace *) &mem_space;

	chipset = apecs_swiz_chipset;

	if (platform.pci_intr_init)
		platform.pci_intr_init();

}

static int
apecs_probe(device_t dev)
{
	int memwidth;
	if (apecs0)
		return ENXIO;
	apecs0 = dev;
	memwidth = (REGVAL(COMANCHE_GCR) & COMANCHE_GCR_WIDEMEM) != 0 ? 128 : 64;
	if(memwidth == 64){
		device_set_desc(dev, "DECchip 21071 Core Logic chipset");
	} else {
		device_set_desc(dev, "DECchip 21072 Core Logic chipset");
	}
	apecs_hae_mem = REGVAL(EPIC_HAXR1);

	pci_init_resources();
	isa_init_intr();
	apecs_init_sgmap();

	device_add_child(dev, "pcib", 0);

	return 0;
}

static int
apecs_attach(device_t dev)
{
	struct apecs_softc* sc = APECS_SOFTC(dev);
	apecs_init();

	sc->dmem_base = APECS_PCI_DENSE;
	sc->smem_base = APECS_PCI_SPARSE;
	sc->io_base = APECS_PCI_SIO;
	sc->cfg0_base = KV(APECS_PCI_CONF);
	sc->cfg1_base = NULL;

	set_iointr(alpha_dispatch_intr);

	snprintf(chipset_type, sizeof(chipset_type), "apecs");
	chipset_bwx = 0;
	chipset_ports = APECS_PCI_SIO;
	chipset_memory = APECS_PCI_SPARSE;
	chipset_dense = APECS_PCI_DENSE;
	chipset_hae_mask = EPIC_HAXR1_EADDR;

	bus_generic_attach(dev);
	return 0;
}

static struct resource *
apecs_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     u_long start, u_long end, u_long count, u_int flags)
{
	if ((hwrpb->rpb_type == ST_DEC_2100_A50) &&
	    (type == SYS_RES_IRQ))
		return isa_alloc_intr(bus, child, start);
	else
		return pci_alloc_resource(bus, child, type, rid,
					  start, end, count, flags);
}

static int
apecs_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	if ((hwrpb->rpb_type == ST_DEC_2100_A50) &&
	    (type == SYS_RES_IRQ))
		return isa_release_intr(bus, child, r);
	else
		return pci_release_resource(bus, child, type, rid, r);
}

static void
apecs_disable_intr(int vector)
{
	int irq = (vector - 0x900) >> 4;
	platform.pci_intr_disable(irq);
}

static void
apecs_enable_intr(int vector)
{
	int irq = (vector - 0x900) >> 4;
	platform.pci_intr_enable(irq);
}

static int
apecs_setup_intr(device_t dev, device_t child,
	       struct resource *irq, int flags,
	       driver_intr_t *intr, void *arg, void **cookiep)
{
	int error;
	
	/* 
	 *  the avanti routes interrupts through the isa interrupt
	 *  controller, so we need to special case it 
	 */
	if(hwrpb->rpb_type == ST_DEC_2100_A50)
		return isa_setup_intr(dev, child, irq, flags,
				      intr, arg, cookiep);

	error = rman_activate_resource(irq);
	if (error)
		return error;

	error = alpha_setup_intr(device_get_nameunit(child ? child : dev),
			0x900 + (irq->r_start << 4), intr, arg, flags, cookiep,
			&intrcnt[INTRCNT_EB64PLUS_IRQ + irq->r_start],
			apecs_disable_intr, apecs_enable_intr);
	if (error)
		return error;

	/* Enable PCI interrupt */
	platform.pci_intr_enable(irq->r_start);

	device_printf(child, "interrupting at APECS irq %d\n",
		      (int) irq->r_start);


	return 0;
}

static int
apecs_teardown_intr(device_t dev, device_t child,
		  struct resource *irq, void *cookie)
{
	/* 
	 *  the avanti routes interrupts through the isa interrupt
	 *  controller, so we need to special case it 
	 */
	if(hwrpb->rpb_type == ST_DEC_2100_A50)
		return isa_teardown_intr(dev, child, irq, cookie);

	alpha_teardown_intr(cookie);
	return rman_deactivate_resource(irq);
}

DRIVER_MODULE(apecs, root, apecs_driver, apecs_devclass, 0, 0);

