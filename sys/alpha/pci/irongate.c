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

static alpha_chipset_inb_t	irongate_inb;
static alpha_chipset_inw_t	irongate_inw;
static alpha_chipset_inl_t	irongate_inl;
static alpha_chipset_outb_t	irongate_outb;
static alpha_chipset_outw_t	irongate_outw;
static alpha_chipset_outl_t	irongate_outl;
static alpha_chipset_readb_t	irongate_readb;
static alpha_chipset_readw_t	irongate_readw;
static alpha_chipset_readl_t	irongate_readl;
static alpha_chipset_writeb_t	irongate_writeb;
static alpha_chipset_writew_t	irongate_writew;
static alpha_chipset_writel_t	irongate_writel;
static alpha_chipset_maxdevs_t	irongate_maxdevs;
static alpha_chipset_cfgreadb_t	irongate_cfgreadb;
static alpha_chipset_cfgreadw_t	irongate_cfgreadw;
static alpha_chipset_cfgreadl_t	irongate_cfgreadl;
static alpha_chipset_cfgwriteb_t irongate_cfgwriteb;
static alpha_chipset_cfgwritew_t irongate_cfgwritew;
static alpha_chipset_cfgwritel_t irongate_cfgwritel;
static alpha_chipset_addrcvt_t   irongate_cvt_dense, irongate_cvt_bwx;

static alpha_chipset_read_hae_t	irongate_read_hae;
static alpha_chipset_write_hae_t irongate_write_hae;

static alpha_chipset_t irongate_chipset = {
	irongate_inb,
	irongate_inw,
	irongate_inl,
	irongate_outb,
	irongate_outw,
	irongate_outl,
	irongate_readb,
	irongate_readw,
	irongate_readl,
	irongate_writeb,
	irongate_writew,
	irongate_writel,
	irongate_maxdevs,
	irongate_cfgreadb,
	irongate_cfgreadw,
	irongate_cfgreadl,
	irongate_cfgwriteb,
	irongate_cfgwritew,
	irongate_cfgwritel,
	irongate_cvt_dense,
	irongate_cvt_bwx,
	irongate_read_hae,
	irongate_write_hae,
};

static u_int8_t
irongate_inb(u_int32_t port)
{
	alpha_mb();
	return ldbu(KV(IRONGATE_IO + port));
}

static u_int16_t
irongate_inw(u_int32_t port)
{
	alpha_mb();
	return ldwu(KV(IRONGATE_IO + port));
}

static u_int32_t
irongate_inl(u_int32_t port)
{
	alpha_mb();
	return ldl(KV(IRONGATE_IO + port));
}

static void
irongate_outb(u_int32_t port, u_int8_t data)
{
	stb(KV(IRONGATE_IO + port), data);
	alpha_mb();
}

static void
irongate_outw(u_int32_t port, u_int16_t data)
{
	stw(KV(IRONGATE_IO + port), data);
	alpha_mb();
}

static void
irongate_outl(u_int32_t port, u_int32_t data)
{
	stl(KV(IRONGATE_IO + port), data);
	alpha_mb();
}

static u_int8_t
irongate_readb(u_int32_t pa)
{
	alpha_mb();
	return ldbu(KV(IRONGATE_MEM + pa));
}

static u_int16_t
irongate_readw(u_int32_t pa)
{
	alpha_mb();
	return ldwu(KV(IRONGATE_MEM + pa));
}

static u_int32_t
irongate_readl(u_int32_t pa)
{
	alpha_mb();
	return ldl(KV(IRONGATE_MEM + pa));
}

static void
irongate_writeb(u_int32_t pa, u_int8_t data)
{
	stb(KV(IRONGATE_MEM + pa), data);
	alpha_mb();
}

static void
irongate_writew(u_int32_t pa, u_int16_t data)
{
	stw(KV(IRONGATE_MEM + pa), data);
	alpha_mb();
}

static void
irongate_writel(u_int32_t pa, u_int32_t data)
{
	stl(KV(IRONGATE_MEM + pa), data);
	alpha_mb();
}

static int
irongate_maxdevs(u_int b)
{
	return 12;		/* XXX */
}

static void
irongate_clear_abort(void)
{
	alpha_mb();
	alpha_pal_draina();	
}

static int
irongate_check_abort(void)
{
	alpha_pal_draina();	
	alpha_mb();

	return 0;
}

#define IRONGATE_CFGADDR(b, s, f, r)				\
	KV(IRONGATE_CONF | ((b) << 16) | ((s) << 11) | ((f) << 8) | (r))

#define CFGREAD(h, b, s, f, r, op, width, type)			\
        vm_offset_t va;						\
	type data;						\
	va = IRONGATE_CFGADDR(b, s, f, r);			\
	irongate_clear_abort();					\
	if (badaddr((caddr_t)va, width)) {			\
		irongate_check_abort();				\
		return ~0;					\
	}							\
	data = ##op##(va);					\
	if (irongate_check_abort())				\
		return ~0;					\
	return data;			

#define CFWRITE(h, b, s, f, r, data, op, width)			\
        vm_offset_t va;						\
	va = IRONGATE_CFGADDR(b, s, f, r);			\
	irongate_clear_abort();					\
	if (badaddr((caddr_t)va, width)) 			\
		return;						\
	##op##(va, data);					\
	irongate_check_abort();




static u_int8_t
irongate_cfgreadb(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(h, b, s, f, r, ldbu, 1, u_int8_t)
}

static u_int16_t
irongate_cfgreadw(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(h, b, s, f, r, ldwu, 2, u_int16_t)
}

static u_int32_t
irongate_cfgreadl(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(h, b, s, f, r, ldl, 4, u_int32_t)
}

static void
irongate_cfgwriteb(u_int h, u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	CFWRITE(h, b, s, f, r, data, stb, 1)	
}

static void
irongate_cfgwritew(u_int h, u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{	
	CFWRITE(h, b, s, f, r, data, stw, 2)
}

static void
irongate_cfgwritel(u_int h, u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	CFWRITE(h, b, s, f, r, data, stl, 4)
}


vm_offset_t
irongate_cvt_bwx(vm_offset_t addr)
{
        addr &= 0xffffffffUL;
	return (KV(addr | IRONGATE_MEM));
}

vm_offset_t
irongate_cvt_dense(vm_offset_t addr)
{
	return irongate_cvt_bwx(addr);
}


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

	if (initted) return;
	initted = 1;

	chipset = irongate_chipset;
	alpha_XXX_dmamap_or = 0UL;

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
	u_int8_t value;
	pcicfgregs southbridge;


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

	/*  
	 * XXX  -- The SRM console doesn't properly initialize
	 * the AcerLabs M1533C southbridge.  We must turn off 32-bit 
	 * DMA support.
	 */

	southbridge.hose = 0;
	southbridge.bus  = 0;
	southbridge.slot = 7;
	southbridge.func = 0;
	if ((0x153310b9 == pci_cfgread(&southbridge, PCIR_DEVVENDOR, 4))) {
		value = (u_int8_t)pci_cfgread(&southbridge, 0x42, 1);	
		value &= ~0x40;
		pci_cfgwrite(&southbridge, 0x42, 0, 1);
	}

	bus_generic_attach(dev);

	return 0;
}

DRIVER_MODULE(irongate, root, irongate_driver, irongate_devclass, 0, 0);

