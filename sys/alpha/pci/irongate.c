/*-
 * Copyright (c) 2000 Andrew Gallatin
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
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <alpha/isa/isavar.h>
#include <alpha/pci/irongatereg.h>
#include <alpha/pci/irongatevar.h>
#include <alpha/pci/pcibus.h>
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

static devclass_t	irongate_devclass;
static device_t		irongate0;		/* XXX only one for now */

struct irongate_softc {
	int		junk;		/* no softc */
};

#define IRONGATE_SOFTC(dev)	(struct irongate_softc*) device_get_softc(dev)

static alpha_chipset_read_hae_t	irongate_read_hae;
static alpha_chipset_write_hae_t irongate_write_hae;

static alpha_chipset_t irongate_chipset = {
	irongate_read_hae,
	irongate_write_hae,
};

/* 
 * There doesn't appear to be an hae on this platform
 */


static u_int64_t
irongate_read_hae(void)
{
	return 0;  
}

static void
irongate_write_hae(u_int64_t hae)
{
}

static int irongate_probe(device_t dev);
static int irongate_attach(device_t dev);

static device_method_t irongate_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		irongate_probe),
	DEVMETHOD(device_attach,	irongate_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,      bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	pci_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	isa_setup_intr),
	DEVMETHOD(bus_teardown_intr,	isa_teardown_intr),

	{ 0, 0 }
};

static driver_t irongate_driver = {
	"irongate",
	irongate_methods,
	sizeof(struct irongate_softc),
};

void
irongate_init()
{
	static int initted = 0;
	static struct bwx_space io_space, mem_space;

	if (initted) return;
	initted = 1;

	chipset = irongate_chipset;
	alpha_XXX_dmamap_or = 0UL;

	bwx_init_space(&io_space, KV(IRONGATE_IO));
	bwx_init_space(&mem_space, KV(IRONGATE_MEM));

	busspace_isa_io = (kobj_t) &io_space;
	busspace_isa_mem = (kobj_t) &mem_space;

	if (platform.pci_intr_init)
		platform.pci_intr_init();
}

static int
irongate_probe(device_t dev)
{

	if (irongate0)
		return ENXIO;
	irongate0 = dev;
	device_set_desc(dev, "AMD 751 Core Logic chipset"); 
	pci_init_resources();
	isa_init_intr();
	device_add_child(dev, "pcib", 0);
	return 0;
}

	

static int
irongate_attach(device_t dev)
{
	irongate_init();

	if (!platform.iointr)	/* XXX */
		set_iointr(alpha_dispatch_intr);

	snprintf(chipset_type, sizeof(chipset_type), "irongate");
	chipset_bwx = 1;

	chipset_ports = IRONGATE_IO;
	chipset_memory = IRONGATE_MEM;
	chipset_dense = IRONGATE_MEM;
	/* no s/g support in this chipset, must use bounce-buffers */
	chipset.sgmap = NULL;	

	bus_generic_attach(dev);

	return 0;
}

DRIVER_MODULE(irongate, root, irongate_driver, irongate_devclass, 0, 0);

