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
 * $FreeBSD: src/sys/alpha/pci/lca.c,v 1.13 1999/12/03 08:40:53 mdodd Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <alpha/pci/lcareg.h>
#include <alpha/pci/lcavar.h>
#include <alpha/pci/pcibus.h>
#include <alpha/isa/isavar.h>
#include <machine/intr.h>
#include <machine/resource.h>
#include <machine/cpuconf.h>
#include <machine/swiz.h>
#include <machine/sgmap.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	lca_devclass;
static device_t		lca0;		/* XXX only one for now */

struct lca_softc {
	int		junk;
};

#define LCA_SOFTC(dev)	(struct lca_softc*) device_get_softc(dev)

static alpha_chipset_inb_t	lca_inb;
static alpha_chipset_inw_t	lca_inw;
static alpha_chipset_inl_t	lca_inl;
static alpha_chipset_outb_t	lca_outb;
static alpha_chipset_outw_t	lca_outw;
static alpha_chipset_outl_t	lca_outl;
static alpha_chipset_readb_t	lca_readb;
static alpha_chipset_readw_t	lca_readw;
static alpha_chipset_readl_t	lca_readl;
static alpha_chipset_writeb_t	lca_writeb;
static alpha_chipset_writew_t	lca_writew;
static alpha_chipset_writel_t	lca_writel;
static alpha_chipset_maxdevs_t	lca_maxdevs;
static alpha_chipset_cfgreadb_t	lca_cfgreadb;
static alpha_chipset_cfgreadw_t	lca_cfgreadw;
static alpha_chipset_cfgreadl_t	lca_cfgreadl;
static alpha_chipset_cfgwriteb_t lca_cfgwriteb;
static alpha_chipset_cfgwritew_t lca_cfgwritew;
static alpha_chipset_cfgwritel_t lca_cfgwritel;
static alpha_chipset_addrcvt_t   lca_cvt_dense;
static alpha_chipset_read_hae_t	lca_read_hae;
static alpha_chipset_write_hae_t lca_write_hae;

static alpha_chipset_t lca_chipset = {
	lca_inb,
	lca_inw,
	lca_inl,
	lca_outb,
	lca_outw,
	lca_outl,
	lca_readb,
	lca_readw,
	lca_readl,
	lca_writeb,
	lca_writew,
	lca_writel,
	lca_maxdevs,
	lca_cfgreadb,
	lca_cfgreadw,
	lca_cfgreadl,
	lca_cfgwriteb,
	lca_cfgwritew,
	lca_cfgwritel,
	lca_cvt_dense,
	NULL,
	lca_read_hae,
	lca_write_hae,
};

static u_int8_t
lca_inb(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_BYTE(KV(LCA_PCI_SIO), port);
}

static u_int16_t
lca_inw(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_WORD(KV(LCA_PCI_SIO), port);
}

static u_int32_t
lca_inl(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_LONG(KV(LCA_PCI_SIO), port);
}

static void
lca_outb(u_int32_t port, u_int8_t data)
{
	SPARSE_WRITE_BYTE(KV(LCA_PCI_SIO), port, data);
	alpha_wmb();
}

static void
lca_outw(u_int32_t port, u_int16_t data)
{
	SPARSE_WRITE_WORD(KV(LCA_PCI_SIO), port, data);
	alpha_wmb();
}

static void
lca_outl(u_int32_t port, u_int32_t data)
{
	SPARSE_WRITE_LONG(KV(LCA_PCI_SIO), port, data);
	alpha_wmb();
}

/*
 * The LCA HAE is write-only.  According to NetBSD, this is where it starts.
 */
static u_int32_t	lca_hae_mem = 0x80000000;

/*
 * The first 16Mb ignores the HAE.  The next 112Mb uses the HAE to set
 * the high bits of the PCI address.
 */
#define REG1 (1UL << 24)

static __inline  void
lca_set_hae_mem(u_int32_t *pa)
{
	int s; 
	u_int32_t msb;
	if(*pa >= REG1){
		msb = *pa & 0xf8000000;
		*pa -= msb;
		s = splhigh();
                if (msb != lca_hae_mem) {
			lca_hae_mem = msb;
			REGVAL(LCA_IOC_HAE) = lca_hae_mem;
			alpha_mb();
			alpha_mb();
		}
		splx(s);
	}
}

static u_int8_t
lca_readb(u_int32_t pa)
{
	alpha_mb();
	lca_set_hae_mem(&pa);
	return SPARSE_READ_BYTE(KV(LCA_PCI_SPARSE), pa);
}

static u_int16_t
lca_readw(u_int32_t pa)
{
	alpha_mb();
	lca_set_hae_mem(&pa);
	return SPARSE_READ_WORD(KV(LCA_PCI_SPARSE), pa);
}

static u_int32_t
lca_readl(u_int32_t pa)
{
	alpha_mb();
	lca_set_hae_mem(&pa);
	return SPARSE_READ_LONG(KV(LCA_PCI_SPARSE), pa);
}

static void
lca_writeb(u_int32_t pa, u_int8_t data)
{
	lca_set_hae_mem(&pa);
	SPARSE_WRITE_BYTE(KV(LCA_PCI_SPARSE), pa, data);
	alpha_wmb();
}

static void
lca_writew(u_int32_t pa, u_int16_t data)
{
	lca_set_hae_mem(&pa);
	SPARSE_WRITE_WORD(KV(LCA_PCI_SPARSE), pa, data);
	alpha_wmb();
}

static void
lca_writel(u_int32_t pa, u_int32_t data)
{
	lca_set_hae_mem(&pa);
	SPARSE_WRITE_LONG(KV(LCA_PCI_SPARSE), pa, data);
	alpha_wmb();
}

static int
lca_maxdevs(u_int b)
{
	return 12;		/* XXX */
}

#define LCA_CFGOFF(b, s, f, r) \
	((b) ? (((b) << 16) | ((s) << 11) | ((f) << 8) | (r)) \
	 : ((1 << ((s) + 11)) | ((f) << 8) | (r)))

#define LCA_TYPE1_SETUP(b,s) if ((b)) {		\
        do {					\
		(s) = splhigh();		\
		alpha_mb();			\
		REGVAL(LCA_IOC_CONF) = 1;	\
		alpha_mb();			\
        } while(0);				\
}

#define LCA_TYPE1_TEARDOWN(b,s) if ((b)) {	\
        do {					\
		alpha_mb();			\
		REGVAL(LCA_IOC_CONF) = 0;	\
		alpha_mb();			\
		splx((s));			\
        } while(0);				\
}

#define CFGREAD(b, s, f, r, width, type)				 \
	type val = ~0;							 \
	int ipl = 0;							 \
	vm_offset_t off = LCA_CFGOFF(b, s, f, r);			 \
	vm_offset_t kv = SPARSE_##width##_ADDRESS(KV(LCA_PCI_CONF), off); \
	alpha_mb();							 \
	LCA_TYPE1_SETUP(b,ipl);						 \
	if (!badaddr((caddr_t)kv, sizeof(type))) {			 \
		val = SPARSE_##width##_EXTRACT(off, SPARSE_READ(kv));	 \
	}								 \
        LCA_TYPE1_TEARDOWN(b,ipl);					 \
	return val							

#define CFGWRITE(b, s, f, r, data, width, type)				\
	int ipl = 0;							\
	vm_offset_t off = LCA_CFGOFF(b, s, f, r);			\
	vm_offset_t kv = SPARSE_##width##_ADDRESS(KV(LCA_PCI_CONF), off); \
	alpha_mb();							\
	LCA_TYPE1_SETUP(b,ipl);						\
	if (!badaddr((caddr_t)kv, sizeof(type))) {			\
                SPARSE_WRITE(kv, SPARSE_##width##_INSERT(off, data));	\
		alpha_wmb();						\
	}								\
        LCA_TYPE1_TEARDOWN(b,ipl);					\
	return

static u_int8_t
lca_cfgreadb(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(b, s, f, r, BYTE, u_int8_t);
}

static u_int16_t
lca_cfgreadw(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(b, s, f, r, WORD, u_int16_t);
}

static u_int32_t
lca_cfgreadl(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(b, s, f, r, LONG, u_int32_t);
}

static void
lca_cfgwriteb(u_int h, u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	CFGWRITE(b, s, f, r, data, BYTE, u_int8_t);
}

static void
lca_cfgwritew(u_int h, u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	CFGWRITE(b, s, f, r, data, WORD, u_int16_t);
}

static void
lca_cfgwritel(u_int h, u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	CFGWRITE(b, s, f, r, data, LONG, u_int16_t);
}

static vm_offset_t
lca_cvt_dense(vm_offset_t addr)
{
	addr &= 0xffffffffUL;
	return (addr | LCA_PCI_DENSE);
	
}

static u_int64_t
lca_read_hae(void)
{
	return lca_hae_mem & 0xf8000000;
}

static void
lca_write_hae(u_int64_t hae)
{
	u_int32_t pa = hae;
	lca_set_hae_mem(&pa);
}

static int lca_probe(device_t dev);
static int lca_attach(device_t dev);
static struct resource *lca_alloc_resource(device_t bus, device_t child,
					   int type, int *rid, u_long start,
					   u_long end, u_long count,
					   u_int flags);
static int lca_release_resource(device_t bus, device_t child,
				int type, int rid, struct resource *r);

static device_method_t lca_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lca_probe),
	DEVMETHOD(device_attach,	lca_attach),

	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,	lca_alloc_resource),
	DEVMETHOD(bus_release_resource,	lca_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	isa_setup_intr),
	DEVMETHOD(bus_teardown_intr,	isa_teardown_intr),

	{ 0, 0 }
};

static driver_t lca_driver = {
	"lca",
	lca_methods,
	sizeof(struct lca_softc),
};

#define LCA_SGMAP_BASE		(8*1024*1024)
#define LCA_SGMAP_SIZE		(8*1024*1024)

static void
lca_sgmap_invalidate(void)
{
	alpha_mb();
	REGVAL(LCA_IOC_TBIA) = 0;
	alpha_mb();
}

static void
lca_sgmap_map(void *arg, vm_offset_t ba, vm_offset_t pa)
{
	u_int64_t *sgtable = arg;
	int index = alpha_btop(ba - LCA_SGMAP_BASE);

	if (pa) {
		if (pa > (1L<<32))
			panic("lca_sgmap_map: can't map address 0x%lx", pa);
		sgtable[index] = ((pa >> 13) << 1) | 1;
	} else {
		sgtable[index] = 0;
	}
	alpha_mb();
	lca_sgmap_invalidate();
}

static void
lca_init_sgmap(void)
{
	void *sgtable;

	/*
	 * First setup Window 0 to map 8Mb to 16Mb with an
	 * sgmap. Allocate the map aligned to a 32 boundary.
	 */
	REGVAL64(LCA_IOC_W_BASE0) = LCA_SGMAP_BASE |
		IOC_W_BASE_SG | IOC_W_BASE_WEN;
	alpha_mb();

	REGVAL64(LCA_IOC_W_MASK0) = IOC_W_MASK_8M;
	alpha_mb();

	sgtable = contigmalloc(8192, M_DEVBUF, M_NOWAIT,
			       0, (1L<<34),
			       32*1024, (1L<<34));
	if (!sgtable)
		panic("lca_init_sgmap: can't allocate page table");
	chipset.sgmap = sgmap_map_create(LCA_SGMAP_BASE,
					 LCA_SGMAP_BASE + LCA_SGMAP_SIZE,
					 lca_sgmap_map, sgtable);

	
	REGVAL64(LCA_IOC_W_T_BASE0) = pmap_kextract((vm_offset_t) sgtable);
	alpha_mb();
	REGVAL64(LCA_IOC_TB_ENA) = IOC_TB_ENA_TEN;
	alpha_mb();
	lca_sgmap_invalidate();
}

void
lca_init()
{
	static int initted = 0;

	if (initted) return;
	initted = 1;

	/* Type 0 PCI conf access. */
	REGVAL64(LCA_IOC_CONF) = 0;

	if (platform.pci_intr_init)
		platform.pci_intr_init();

	chipset = lca_chipset;
}

static int
lca_probe(device_t dev)
{
	if (lca0)
		return ENXIO;
	lca0 = dev;
	device_set_desc(dev, "21066 Core Logic chipset"); /* XXX */

	pci_init_resources();
	isa_init_intr();
	lca_init_sgmap();

	device_add_child(dev, "pcib", 0);

	return 0;
}

static int
lca_attach(device_t dev)
{
	lca_init();

	set_iointr(alpha_dispatch_intr);

	snprintf(chipset_type, sizeof(chipset_type), "lca");
	chipset_bwx = 0;
	chipset_ports = LCA_PCI_SIO;
	chipset_memory = LCA_PCI_SPARSE;
	chipset_dense = LCA_PCI_DENSE;
	chipset_hae_mask = IOC_HAE_ADDREXT;

	bus_generic_attach(dev);
	return 0;
}

static struct resource *
lca_alloc_resource(device_t bus, device_t child, int type, int *rid,
		   u_long start, u_long end, u_long count, u_int flags)
{
	if (type == SYS_RES_IRQ)
		return isa_alloc_intr(bus, child, start);
	else
		return pci_alloc_resource(bus, child, type, rid,
					  start, end, count, flags);
}

static int
lca_release_resource(device_t bus, device_t child, int type, int rid,
		     struct resource *r)
{
	if (type == SYS_RES_IRQ)
		return isa_release_intr(bus, child, r);
	else
		return pci_release_resource(bus, child, type, rid, r);
}

DRIVER_MODULE(lca, root, lca_driver, lca_devclass, 0, 0);

