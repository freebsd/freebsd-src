/*-
 * Copyright (c) 1999 Andrew Gallatin
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

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/interrupt.h>
#include <sys/malloc.h>

#include <pci/pcivar.h>
#include <alpha/isa/isavar.h>
#include <alpha/pci/tsunamireg.h>
#include <alpha/pci/tsunamivar.h>
#include <machine/bwx.h>
#include <machine/intr.h>
#include <machine/intrcnt.h>
#include <machine/cpuconf.h>
#include <machine/rpb.h>
#include <machine/resource.h>
#include <machine/sgmap.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	tsunami_devclass;
static device_t		tsunami0;		/* XXX only one for now */

struct tsunami_softc {
	int		junk;		/* no softc */
};

int tsunami_num_pchips = 0;
static volatile tsunami_pchip *pchip[2] = {pchip0, pchip1};

#define TSUNAMI_SOFTC(dev)	(struct tsunami_softc*) device_get_softc(dev)

static alpha_chipset_read_hae_t	tsunami_read_hae;
static alpha_chipset_write_hae_t tsunami_write_hae;

static alpha_chipset_t tsunami_chipset = {
	tsunami_read_hae,
	tsunami_write_hae,
};

static void tsunami_intr_enable(int);
static void tsunami_intr_disable(int);

/* 
 * There doesn't appear to be an hae on this platform
 */


static u_int64_t
tsunami_read_hae(void)
{
	return 0;  
}

static void
tsunami_write_hae(u_int64_t hae)
{
}

static int tsunami_probe(device_t dev);
static int tsunami_attach(device_t dev);
static int tsunami_setup_intr(device_t dev, device_t child, 
			      struct resource *irq, int flags,
			  driver_intr_t *intr, void *arg, void **cookiep);
static int tsunami_teardown_intr(device_t dev, device_t child,
			     struct resource *irq, void *cookie);

static device_method_t tsunami_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tsunami_probe),
	DEVMETHOD(device_attach,	tsunami_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,      bus_generic_print_child),
	DEVMETHOD(bus_setup_intr,	tsunami_setup_intr),
	DEVMETHOD(bus_teardown_intr,	tsunami_teardown_intr),

	{ 0, 0 }
};

static driver_t tsunami_driver = {
	"tsunami",
	tsunami_methods,
	sizeof(struct tsunami_softc),
};

static void
pchip_init(volatile tsunami_pchip *pchip, int index)
{

	int i;
	
	/*
	 * initialize the direct map DMA windows.
	 *
	 * leave window 0 untouched; we'll set that up for S/G DMA for 
	 * isa devices later in the boot process
	 *
	 * window 1 goes at 2GB and has a length of 1 GB. It maps
	 * physical address 0 - 1GB. The SRM console typically sets
	 * this window up here.
	 */
	
        pchip->wsba[1].reg = (2UL*1024*1024*1024) | WINDOW_ENABLE;
        pchip->wsm[1].reg  = (1UL*1024*1024*1024 - 1) & 0xfff00000UL;
        pchip->tba[1].reg  = 0;
	
	/*
	 * window 2 goes at 3GB and has a length of 1 GB.  It maps
	 * physical address 1GB-2GB. 
	 */
	
        pchip->wsba[2].reg = (3UL*1024*1024*1024) | WINDOW_ENABLE;
        pchip->wsm[2].reg  = (1UL*1024*1024*1024 - 1) & 0xfff00000UL;
        pchip->tba[2].reg  = 1UL*1024*1024*1024;
	
	/*
	 * window 3 is disabled.  The SRM console typically leaves it
	 * disabled
	 */
	
        pchip->wsba[3].reg = 0;
        alpha_mb();
	
	if(bootverbose) {
		printf("pchip%d:\n", index);
		for (i = 0; i < 4; i++) {
			printf("\twsba[%d].reg = 0x%lx\n", 
			       i, pchip->wsba[i].reg);
			printf("\t wsm[%d].reg = 0x%lx\n", 
			       i, pchip->wsm[i].reg);
			printf("\t tba[%d].reg = 0x%lx\n", 
			       i, pchip->tba[i].reg);
		}
	}
}	

#define TSUNAMI_SGMAP_BASE		(8*1024*1024)
#define TSUNAMI_SGMAP_SIZE		(8*1024*1024)

static void
tsunami_sgmap_invalidate(void)
{
	alpha_mb();
	switch (tsunami_num_pchips) {
	case 2:
		pchip[1]->tlbia.reg = (u_int64_t)0;
	case 1:
		pchip[0]->tlbia.reg = (u_int64_t)0;
	}
	alpha_mb();
}

static void
tsunami_sgmap_map(void *arg, bus_addr_t ba, vm_offset_t pa)
{
	u_int64_t *sgtable = arg;
	int index = alpha_btop(ba - TSUNAMI_SGMAP_BASE);

	if (pa) {
		if (pa > (1L<<32))
			panic("tsunami_sgmap_map: can't map address 0x%lx", pa);
		sgtable[index] = ((pa >> 13) << 1) | 1;
	} else {
		sgtable[index] = 0;
	}
	alpha_mb();
	tsunami_sgmap_invalidate();
}


static void
tsunami_init_sgmap(void)
{
	void *sgtable;
	int i;

	sgtable = contigmalloc(8192, M_DEVBUF, M_NOWAIT,
			       0, (1L<<34),
			       32*1024, (1L<<34));
	if (!sgtable)
		panic("tsunami_init_sgmap: can't allocate page table");

	for(i=0; i < tsunami_num_pchips; i++){
		pchip[i]->tba[0].reg =
			pmap_kextract((vm_offset_t) sgtable);
		pchip[i]->wsba[0].reg |= WINDOW_ENABLE | WINDOW_SCATTER_GATHER;
	}

	chipset.sgmap = sgmap_map_create(TSUNAMI_SGMAP_BASE,
					 TSUNAMI_SGMAP_BASE + TSUNAMI_SGMAP_SIZE,
					 tsunami_sgmap_map, sgtable);
}

void
tsunami_init()
{
	static int initted = 0;
	static struct bwx_space io_space;
	static struct bwx_space mem_space;

	if (initted) return;
	initted = 1;

	/*
	 * Define two temporary spaces for bootstrap i/o on hose 0.
	 */
	bwx_init_space(&io_space, KV(TSUNAMI_IO(0)));
	bwx_init_space(&mem_space, KV(TSUNAMI_MEM(0)));

	busspace_isa_io = (struct alpha_busspace *) &io_space;
	busspace_isa_mem = (struct alpha_busspace *) &mem_space;

	chipset = tsunami_chipset;
	platform.pci_intr_enable =  tsunami_intr_enable;
	platform.pci_intr_disable = tsunami_intr_disable;
	alpha_XXX_dmamap_or = 2UL * 1024UL * 1024UL * 1024UL;

	if (platform.pci_intr_init)
		platform.pci_intr_init();
}

static int
tsunami_probe(device_t dev)
{
	device_t child;
	int i;
	if (tsunami0)
		return ENXIO;
	tsunami0 = dev;
	device_set_desc(dev, "21271 Core Logic chipset"); 
	if(cchip->csc.reg & CSC_P1P)
		tsunami_num_pchips = 2;
	else
		tsunami_num_pchips = 1;

	isa_init_intr();

	for(i = 0; i < tsunami_num_pchips; i++) {
		child = device_add_child(dev, "pcib", i);
		pchip_init(pchip[i], i);		
	}

	return 0;
}

static int
tsunami_attach(device_t dev)
{
	tsunami_init();

	if (!platform.iointr)	/* XXX */
		set_iointr(alpha_dispatch_intr);

	snprintf(chipset_type, sizeof(chipset_type), "tsunami");
	chipset_bwx = 1;

	chipset_ports = TSUNAMI_IO(0);
	chipset_memory = TSUNAMI_MEM(0);
	chipset_dense = TSUNAMI_MEM(0);
	bus_generic_attach(dev);
	tsunami_init_sgmap();

	return 0;
}

static void
tsunami_disable_intr_vec(int vector)
{
	int irq;

	irq = (vector - 0x900) >> 4;
	mtx_lock_spin(&icu_lock);
	platform.pci_intr_disable(irq);
	mtx_unlock_spin(&icu_lock);
}

static void
tsunami_enable_intr_vec(int vector)
{
	int irq;

	irq = (vector - 0x900) >> 4;
	mtx_lock_spin(&icu_lock);
	platform.pci_intr_enable(irq);
	mtx_unlock_spin(&icu_lock);
}

static int
tsunami_setup_intr(device_t dev, device_t child,
	       struct resource *irq, int flags,
	       driver_intr_t *intr, void *arg, void **cookiep)
{
	int error;

	error = rman_activate_resource(irq);
	if (error)
		return error;

	error = alpha_setup_intr(device_get_nameunit(child ? child : dev),
			0x900 + (irq->r_start << 4), intr, arg, flags, cookiep,
			&intrcnt[INTRCNT_EB164_IRQ + irq->r_start],
			tsunami_disable_intr_vec, tsunami_enable_intr_vec);
	if (error)
		return error;

	/* Enable PCI interrupt */
	mtx_lock_spin(&icu_lock);
	platform.pci_intr_enable(irq->r_start);
	mtx_unlock_spin(&icu_lock);

	device_printf(child, "interrupting at TSUNAMI irq %d\n",
		      (int) irq->r_start);

	return 0;
}

static int
tsunami_teardown_intr(device_t dev, device_t child,
		  struct resource *irq, void *cookie)
{

	alpha_teardown_intr(cookie);
	return rman_deactivate_resource(irq);

}


/*
 * Currently, all interrupts will be funneled through CPU 0
 */

static void
tsunami_intr_enable(int irq)
{
	volatile u_int64_t *mask;
	u_int64_t saved_mask;

	mask = &cchip->dim0.reg;
	saved_mask = *mask;

	saved_mask |= (1UL << (unsigned long)irq);
	*mask = saved_mask;
	alpha_mb();
	alpha_mb();
	saved_mask = *mask;
	alpha_mb();
	alpha_mb();
}

static void
tsunami_intr_disable(int irq)
{
	volatile u_int64_t *mask;
	u_int64_t saved_mask;

	mask = &cchip->dim0.reg;
	saved_mask = *mask;

	saved_mask &= ~(1UL << (unsigned long)irq);
	*mask = saved_mask;
	alpha_mb();
	saved_mask = *mask;
	alpha_mb();
	alpha_mb();
}



DRIVER_MODULE(tsunami, root, tsunami_driver, tsunami_devclass, 0, 0);

