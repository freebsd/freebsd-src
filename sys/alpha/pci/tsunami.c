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
 *	$Id $
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
#include <alpha/isa/isavar.h>
#include <alpha/pci/tsunamireg.h>
#include <alpha/pci/tsunamivar.h>
#include <alpha/pci/pcibus.h>
#include <machine/bwx.h>
#include <machine/intr.h>
#include <machine/intrcnt.h>
#include <machine/cpuconf.h>
#include <machine/rpb.h>
#include <machine/resource.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	tsunami_devclass;
static device_t		tsunami0;		/* XXX only one for now */
extern vm_offset_t      alpha_XXX_dmamap_or;
struct tsunami_softc {
	int		junk;		/* no softc */
};

#define TSUNAMI_SOFTC(dev)	(struct tsunami_softc*) device_get_softc(dev)

static alpha_chipset_inb_t	tsunami_inb;
static alpha_chipset_inw_t	tsunami_inw;
static alpha_chipset_inl_t	tsunami_inl;
static alpha_chipset_outb_t	tsunami_outb;
static alpha_chipset_outw_t	tsunami_outw;
static alpha_chipset_outl_t	tsunami_outl;
static alpha_chipset_readb_t	tsunami_readb;
static alpha_chipset_readw_t	tsunami_readw;
static alpha_chipset_readl_t	tsunami_readl;
static alpha_chipset_writeb_t	tsunami_writeb;
static alpha_chipset_writew_t	tsunami_writew;
static alpha_chipset_writel_t	tsunami_writel;
static alpha_chipset_maxdevs_t	tsunami_maxdevs;
static alpha_chipset_cfgreadb_t	tsunami_cfgreadb;
static alpha_chipset_cfgreadw_t	tsunami_cfgreadw;
static alpha_chipset_cfgreadl_t	tsunami_cfgreadl;
static alpha_chipset_cfgwriteb_t tsunami_cfgwriteb;
static alpha_chipset_cfgwritew_t tsunami_cfgwritew;
static alpha_chipset_cfgwritel_t tsunami_cfgwritel;
static alpha_chipset_addrcvt_t   tsunami_cvt_dense, tsunami_cvt_bwx;

static alpha_chipset_read_hae_t	tsunami_read_hae;
static alpha_chipset_write_hae_t tsunami_write_hae;

static alpha_chipset_t tsunami_chipset = {
	tsunami_inb,
	tsunami_inw,
	tsunami_inl,
	tsunami_outb,
	tsunami_outw,
	tsunami_outl,
	tsunami_readb,
	tsunami_readw,
	tsunami_readl,
	tsunami_writeb,
	tsunami_writew,
	tsunami_writel,
	tsunami_maxdevs,
	tsunami_cfgreadb,
	tsunami_cfgreadw,
	tsunami_cfgreadl,
	tsunami_cfgwriteb,
	tsunami_cfgwritew,
	tsunami_cfgwritel,
	tsunami_cvt_dense,
	tsunami_cvt_bwx,
	tsunami_read_hae,
	tsunami_write_hae,
};

/*
 * This setup will only allow for one additional hose
 */

#define ADDR_TO_HOSE(x)    ((x)  >> 31)
#define STRIP_HOSE(x)      ((x) & 0x7fffffff)

static void tsunami_intr_enable  __P((int));
static void tsunami_intr_disable __P((int));

static u_int8_t
tsunami_inb(u_int32_t port)
{
	int hose = ADDR_TO_HOSE(port);
	port = STRIP_HOSE(port);
	alpha_mb();
	return ldbu(KV(TSUNAMI_IO(hose) + port));
}

static u_int16_t
tsunami_inw(u_int32_t port)
{
	int hose = ADDR_TO_HOSE(port);
	port = STRIP_HOSE(port);
	alpha_mb();
	return ldwu(KV(TSUNAMI_IO(hose) + port));
}

static u_int32_t
tsunami_inl(u_int32_t port)
{
	int hose = ADDR_TO_HOSE(port);
	port = STRIP_HOSE(port);
	alpha_mb();
	return ldl(KV(TSUNAMI_IO(hose) + port));
}

static void
tsunami_outb(u_int32_t port, u_int8_t data)
{
	int hose = ADDR_TO_HOSE(port);
	port = STRIP_HOSE(port);
	stb(KV(TSUNAMI_IO(hose) + port), data);
	alpha_mb();
}

static void
tsunami_outw(u_int32_t port, u_int16_t data)
{
	int hose = ADDR_TO_HOSE(port);
	port = STRIP_HOSE(port);
	stw(KV(TSUNAMI_IO(hose) + port), data);
	alpha_mb();
}

static void
tsunami_outl(u_int32_t port, u_int32_t data)
{
	int hose = ADDR_TO_HOSE(port);
	port = STRIP_HOSE(port);
	stl(KV(TSUNAMI_IO(hose) + port), data);
	alpha_mb();
}

static u_int8_t
tsunami_readb(u_int32_t pa)
{
	int hose = ADDR_TO_HOSE(pa);
	pa = STRIP_HOSE(pa);
	alpha_mb();
	return ldbu(KV(TSUNAMI_MEM(hose) + pa));
}

static u_int16_t
tsunami_readw(u_int32_t pa)
{
	int hose = ADDR_TO_HOSE(pa);
	pa = STRIP_HOSE(pa);
	alpha_mb();
	return ldwu(KV(TSUNAMI_MEM(hose) + pa));
}

static u_int32_t
tsunami_readl(u_int32_t pa)
{
	int hose = ADDR_TO_HOSE(pa);
	pa = STRIP_HOSE(pa);
	alpha_mb();
	return ldl(KV(TSUNAMI_MEM(hose) + pa));
}

static void
tsunami_writeb(u_int32_t pa, u_int8_t data)
{
	int hose = ADDR_TO_HOSE(pa);
	pa = STRIP_HOSE(pa);
	stb(KV(TSUNAMI_MEM(hose) + pa), data);
	alpha_mb();
}

static void
tsunami_writew(u_int32_t pa, u_int16_t data)
{
	int hose = ADDR_TO_HOSE(pa);
	pa = STRIP_HOSE(pa);
	stw(KV(TSUNAMI_MEM(hose) + pa), data);
	alpha_mb();
}

static void
tsunami_writel(u_int32_t pa, u_int32_t data)
{
	int hose = ADDR_TO_HOSE(pa);
	pa = STRIP_HOSE(pa);
	stl(KV(TSUNAMI_MEM(hose) + pa), data);
	alpha_mb();
}

static int
tsunami_maxdevs(u_int b)
{
	return 12;		/* XXX */
}

static void
tsunami_clear_abort(void)
{
	alpha_mb();
	alpha_pal_draina();	
}

static int
tsunami_check_abort(void)
{
/*	u_int32_t errbits;*/
	int ba = 0;

	alpha_pal_draina();	
	alpha_mb();
#if 0
	errbits = REGVAL(TSUNAMI_CSR_TSUNAMI_ERR);
	if (errbits & (TSUNAMI_ERR_RCVD_MAS_ABT|TSUNAMI_ERR_RCVD_TAR_ABT))
		ba = 1;

	if (errbits) {
		REGVAL(TSUNAMI_CSR_TSUNAMI_ERR) = errbits;
		alpha_mb();
		alpha_pal_draina();
	}
#endif
	return ba;
}

#define TSUNAMI_CFGADDR(b, s, f, r, h)				\
	KV(TSUNAMI_CONF(h) | ((b) << 16) | ((s) << 11) | ((f) << 8) | (r))

#define CFGREAD(h, b, s, f, r, op, width, type)			\
	int bus = tsunami_bus_within_hose(h, b);		\
	vm_offset_t va = TSUNAMI_CFGADDR(bus, s, f, r, h);	\
	type data;						\
	tsunami_clear_abort();					\
	if (badaddr((caddr_t)va, width)) {			\
		tsunami_check_abort();				\
		return ~0;					\
	}							\
	data = ##op##(va);					\
	if (tsunami_check_abort())				\
		return ~0;					\
	return data;			

#define CFWRITE(h, b, s, f, r, data, op, width)			\
	int bus = tsunami_bus_within_hose(h, b);		\
	vm_offset_t va = TSUNAMI_CFGADDR(bus, s, f, r, h);	\
	tsunami_clear_abort();					\
	if (badaddr((caddr_t)va, width)) 			\
		return;						\
	##op##(va, data);					\
	tsunami_check_abort();




static u_int8_t
tsunami_cfgreadb(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(h, b, s, f, r, ldbu, 1, u_int8_t)
}

static u_int16_t
tsunami_cfgreadw(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(h, b, s, f, r, ldwu, 2, u_int16_t)
}

static u_int32_t
tsunami_cfgreadl(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(h, b, s, f, r, ldl, 4, u_int32_t)
}

static void
tsunami_cfgwriteb(u_int h, u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	CFWRITE(h, b, s, f, r, data, stb, 1)	
}

static void
tsunami_cfgwritew(u_int h, u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{	
	CFWRITE(h, b, s, f, r, data, stw, 2)
}

static void
tsunami_cfgwritel(u_int h, u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	CFWRITE(h, b, s, f, r, data, stl, 4)
}


vm_offset_t
tsunami_cvt_bwx(vm_offset_t addr)
{
	int hose;
	vm_offset_t laddr;
	laddr = addr & 0xffffffffUL;
	hose = ADDR_TO_HOSE(laddr);
	laddr = STRIP_HOSE(addr);
        laddr |=  TSUNAMI_MEM(hose);
	return (KV(laddr));
}

vm_offset_t
tsunami_cvt_dense(vm_offset_t addr)
{
	return tsunami_cvt_bwx(addr);
}


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
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	pci_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
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
pchip_init(tsunami_pchip *pchip, int index)
{
#if 0

	/* 
	 *  The code below, if active, would attempt to
	 *  setup the DMA base and size registers of Window 0 
	 *  to emulate the  placement of the direct-mapped window 
         *  on previous chipsets.
	 *
	 *  HOWEVER: doing this means that a 64-bit card at device 11
	 *  would not be able to be setup for DMA.
	 * 
	 *  For now, we just trust the SRM console to set things up
	 *  properly.  This works on the xp1000, but may need to be
	 *  to be revisited for other systems.
 	 */ 

	printf("initializing pchip%d\n", index);	
	pchip->wsba[0].reg = 1L | (1024*1024*1024U & 0xfff00000U);
	pchip->wsm[0].reg = (1024*1024*1024U - 1) & 0xfff00000UL;
	pchip->tba[0].reg = 0;
	/* 
	 * disable windows 1, 2 and 3 
	 */


	pchip->wsba[1].reg = 0;
	pchip->wsba[2].reg = 0;
	pchip->wsba[3].reg = 0;

	alpha_mb();
#endif	

}	


void
tsunami_init()
{
	static int initted = 0;

	if (initted) return;
	initted = 1;

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
	int *hose;
	int i;
	if (tsunami0)
		return ENXIO;
	tsunami0 = dev;
	device_set_desc(dev, "21271 Core Logic chipset"); 

	pci_init_resources();
	isa_init_intr();

	for(i = 0; i < 2; i++) {
		hose = malloc(sizeof(int), M_DEVBUF, M_NOWAIT);
		*hose = i;
		device_add_child(dev, "pcib", i, hose);
	}
	pchip_init(pchip0, 0);
	pchip_init(pchip1, 1);
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
	
	return 0;
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

	error = alpha_setup_intr(0x900 + (irq->r_start << 4),
			intr, arg, cookiep,
			&intrcnt[INTRCNT_EB164_IRQ + irq->r_start]);
	if (error)
		return error;

	/* Enable PCI interrupt */
	platform.pci_intr_enable(irq->r_start);

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

