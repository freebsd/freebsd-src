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
 *	$Id: cia.c,v 1.5 1998/08/07 08:18:44 dfr Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>
#include <machine/bwx.h>
#include <machine/swiz.h>
#include <machine/intr.h>
#include <machine/cpuconf.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	cia_devclass;
static device_t		cia0;		/* XXX only one for now */
static u_int32_t	cia_hae_mem;

struct cia_softc {
	int		junk;		/* no softc */
};

#define CIA_SOFTC(dev)	(struct cia_softc*) device_get_softc(dev)

static alpha_chipset_inb_t	cia_bwx_inb, cia_swiz_inb;
static alpha_chipset_inw_t	cia_bwx_inw, cia_swiz_inw;
static alpha_chipset_inl_t	cia_bwx_inl, cia_swiz_inl;
static alpha_chipset_outb_t	cia_bwx_outb, cia_swiz_outb;
static alpha_chipset_outw_t	cia_bwx_outw, cia_swiz_outw;
static alpha_chipset_outl_t	cia_bwx_outl, cia_swiz_outl;
static alpha_chipset_readb_t	cia_bwx_readb, cia_swiz_readb;
static alpha_chipset_readw_t	cia_bwx_readw, cia_swiz_readw;
static alpha_chipset_readl_t	cia_bwx_readl, cia_swiz_readl;
static alpha_chipset_writeb_t	cia_bwx_writeb, cia_swiz_writeb;
static alpha_chipset_writew_t	cia_bwx_writew, cia_swiz_writew;
static alpha_chipset_writel_t	cia_bwx_writel, cia_swiz_writel;
static alpha_chipset_maxdevs_t	cia_bwx_maxdevs, cia_swiz_maxdevs;
static alpha_chipset_cfgreadb_t	cia_bwx_cfgreadb, cia_swiz_cfgreadb;
static alpha_chipset_cfgreadw_t	cia_bwx_cfgreadw, cia_swiz_cfgreadw;
static alpha_chipset_cfgreadl_t	cia_bwx_cfgreadl, cia_swiz_cfgreadl;
static alpha_chipset_cfgwriteb_t cia_bwx_cfgwriteb, cia_swiz_cfgwriteb;
static alpha_chipset_cfgwritew_t cia_bwx_cfgwritew, cia_swiz_cfgwritew;
static alpha_chipset_cfgwritel_t cia_bwx_cfgwritel, cia_swiz_cfgwritel;

static alpha_chipset_t cia_bwx_chipset = {
	cia_bwx_inb,
	cia_bwx_inw,
	cia_bwx_inl,
	cia_bwx_outb,
	cia_bwx_outw,
	cia_bwx_outl,
	cia_bwx_readb,
	cia_bwx_readw,
	cia_bwx_readl,
	cia_bwx_writeb,
	cia_bwx_writew,
	cia_bwx_writel,
	cia_bwx_maxdevs,
	cia_bwx_cfgreadb,
	cia_bwx_cfgreadw,
	cia_bwx_cfgreadl,
	cia_bwx_cfgwriteb,
	cia_bwx_cfgwritew,
	cia_bwx_cfgwritel,
};
static alpha_chipset_t cia_swiz_chipset = {
	cia_swiz_inb,
	cia_swiz_inw,
	cia_swiz_inl,
	cia_swiz_outb,
	cia_swiz_outw,
	cia_swiz_outl,
	cia_swiz_readb,
	cia_swiz_readw,
	cia_swiz_readl,
	cia_swiz_writeb,
	cia_swiz_writew,
	cia_swiz_writel,
	cia_swiz_maxdevs,
	cia_swiz_cfgreadb,
	cia_swiz_cfgreadw,
	cia_swiz_cfgreadl,
	cia_swiz_cfgwriteb,
	cia_swiz_cfgwritew,
	cia_swiz_cfgwritel,
};

static u_int8_t
cia_bwx_inb(u_int32_t port)
{
	alpha_mb();
	return ldbu(KV(CIA_EV56_BWIO+BWX_EV56_INT1 + port));
}

static u_int16_t
cia_bwx_inw(u_int32_t port)
{
	alpha_mb();
	return ldwu(KV(CIA_EV56_BWIO+BWX_EV56_INT2 + port));
}

static u_int32_t
cia_bwx_inl(u_int32_t port)
{
	alpha_mb();
	return ldl(KV(CIA_EV56_BWIO+BWX_EV56_INT4 + port));
}

static void
cia_bwx_outb(u_int32_t port, u_int8_t data)
{
	stb(KV(CIA_EV56_BWIO+BWX_EV56_INT1 + port), data);
	alpha_wmb();
}

static void
cia_bwx_outw(u_int32_t port, u_int16_t data)
{
	stw(KV(CIA_EV56_BWIO+BWX_EV56_INT2 + port), data);
	alpha_wmb();
}

static void
cia_bwx_outl(u_int32_t port, u_int32_t data)
{
	stl(KV(CIA_EV56_BWIO+BWX_EV56_INT4 + port), data);
	alpha_wmb();
}

static u_int8_t
cia_bwx_readb(u_int32_t pa)
{
	alpha_mb();
	return ldbu(KV(CIA_EV56_BWMEM+BWX_EV56_INT1 + pa));
}

static u_int16_t
cia_bwx_readw(u_int32_t pa)
{
	alpha_mb();
	return ldwu(KV(CIA_EV56_BWMEM+BWX_EV56_INT2 + pa));
}

static u_int32_t
cia_bwx_readl(u_int32_t pa)
{
	alpha_mb();
	return ldl(KV(CIA_EV56_BWMEM+BWX_EV56_INT4 + pa));
}

static void
cia_bwx_writeb(u_int32_t pa, u_int8_t data)
{
	stb(KV(CIA_EV56_BWMEM+BWX_EV56_INT1 + pa), data);
	alpha_wmb();
}

static void
cia_bwx_writew(u_int32_t pa, u_int16_t data)
{
	stw(KV(CIA_EV56_BWMEM+BWX_EV56_INT2 + pa), data);
	alpha_wmb();
}

static void
cia_bwx_writel(u_int32_t pa, u_int32_t data)
{
	stl(KV(CIA_EV56_BWMEM+BWX_EV56_INT4 + pa), data);
	alpha_wmb();
}

static int
cia_bwx_maxdevs(u_int b)
{
	return 12;		/* XXX */
}

#define CIA_BWX_CFGADDR(b, s, f, r)				\
	KV(((b) ? CIA_EV56_BWCONF1 : CIA_EV56_BWCONF0)		\
	   | ((b) << 16) | ((s) << 11) | ((f) << 8) | (r))

static u_int8_t
cia_bwx_cfgreadb(u_int b, u_int s, u_int f, u_int r)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	alpha_mb();
	if (badaddr((caddr_t)va, 1)) return ~0;
	return ldbu(va+BWX_EV56_INT1);
}

static u_int16_t
cia_bwx_cfgreadw(u_int b, u_int s, u_int f, u_int r)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	alpha_mb();
	if (badaddr((caddr_t)va, 2)) return ~0;
	return ldwu(va+BWX_EV56_INT2);
}

static u_int32_t
cia_bwx_cfgreadl(u_int b, u_int s, u_int f, u_int r)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	alpha_mb();
	if (badaddr((caddr_t)va, 4)) return ~0;
	return ldl(va+BWX_EV56_INT4);
}

static void
cia_bwx_cfgwriteb(u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 1)) return;
	stb(va+BWX_EV56_INT1, data);
	alpha_wmb();
}

static void
cia_bwx_cfgwritew(u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 2)) return;
	stw(va+BWX_EV56_INT2, data);
	alpha_wmb();
}

static void
cia_bwx_cfgwritel(u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 4)) return;
	stl(va+BWX_EV56_INT4, data);
	alpha_wmb();
}

static u_int8_t
cia_swiz_inb(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_BYTE(KV(CIA_PCI_SIO1), port);
}

static u_int16_t
cia_swiz_inw(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_WORD(KV(CIA_PCI_SIO1), port);
}

static u_int32_t
cia_swiz_inl(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_LONG(KV(CIA_PCI_SIO1), port);
}

static void
cia_swiz_outb(u_int32_t port, u_int8_t data)
{
	SPARSE_WRITE_BYTE(KV(CIA_PCI_SIO1), port, data);
	alpha_wmb();
}

static void
cia_swiz_outw(u_int32_t port, u_int16_t data)
{
	SPARSE_WRITE_WORD(KV(CIA_PCI_SIO1), port, data);
	alpha_wmb();
}

static void
cia_swiz_outl(u_int32_t port, u_int32_t data)
{
	SPARSE_WRITE_LONG(KV(CIA_PCI_SIO1), port, data);
	alpha_wmb();
}

static __inline void
cia_swiz_set_hae_mem(u_int32_t pa)
{
    /* Only bother with region 1 */
#define REG1 (7 << 29)
    if ((cia_hae_mem & REG1) != (pa & REG1)) {
	/*
	 * Seems fairly paranoid but this is what Linux does...
	 */
	int s = splhigh();
	cia_hae_mem = (cia_hae_mem & ~REG1) | (pa & REG1);
	REGVAL(CIA_CSR_HAE_MEM) = cia_hae_mem;
	alpha_mb();
	cia_hae_mem = REGVAL(CIA_CSR_HAE_MEM);
	splx(s);
    }
}

static u_int8_t
cia_swiz_readb(u_int32_t pa)
{
	alpha_mb();
	cia_swiz_set_hae_mem(pa);
	return SPARSE_READ_BYTE(KV(CIA_PCI_SMEM1), pa);
}

static u_int16_t
cia_swiz_readw(u_int32_t pa)
{
	alpha_mb();
	cia_swiz_set_hae_mem(pa);
	return SPARSE_READ_WORD(KV(CIA_PCI_SMEM1), pa);
}

static u_int32_t
cia_swiz_readl(u_int32_t pa)
{
	alpha_mb();
	cia_swiz_set_hae_mem(pa);
	return SPARSE_READ_LONG(KV(CIA_PCI_SMEM1), pa);
}

static void
cia_swiz_writeb(u_int32_t pa, u_int8_t data)
{
	cia_swiz_set_hae_mem(pa);
	SPARSE_WRITE_BYTE(KV(CIA_PCI_SMEM1), pa, data);
	alpha_wmb();
}

static void
cia_swiz_writew(u_int32_t pa, u_int16_t data)
{
	cia_swiz_set_hae_mem(pa);
	SPARSE_WRITE_WORD(KV(CIA_PCI_SMEM1), pa, data);
	alpha_wmb();
}

static void
cia_swiz_writel(u_int32_t pa, u_int32_t data)
{
	cia_swiz_set_hae_mem(pa);
	SPARSE_WRITE_LONG(KV(CIA_PCI_SMEM1), pa, data);
	alpha_wmb();
}

static int
cia_swiz_maxdevs(u_int b)
{
	return 12;		/* XXX */
}

#define CIA_SWIZ_CFGOFF(b, s, f, r) \
	(((b) << 16) | ((s) << 11) | ((f) << 8) | (r))

/*  when doing a type 1 pci configuration space access, we
 *  must set a bit in the CIA_CSR_CFG register & clear it 
 *  when we're done 
*/

#define CIA_TYPE1_SETUP(b,s,old_cfg) if((b)) {		\
        do {						\
		(s) = splhigh();			\
		(old_cfg) = REGVAL(CIA_CSR_CFG);	\
		alpha_mb();				\
		REGVAL(CIA_CSR_CFG) = (old_cfg) | 0x1;	\
		alpha_mb();				\
        } while(0);					\
}

#define CIA_TYPE1_TEARDOWN(b,s,old_cfg) if((b)) {	\
        do {						\
		alpha_mb();				\
		REGVAL(CIA_CSR_CFG) = (old_cfg);	\
		alpha_mb();				\
		splx((s));				\
        } while(0);					\
}

#define SWIZ_CFGREAD(b, s, f, r, width, type)				\
	type val = ~0;							\
	int ipl = 0;							\
	u_int32_t old_cfg = 0;						\
	vm_offset_t off = CIA_SWIZ_CFGOFF(b, s, f, r);			\
	vm_offset_t kv = SPARSE_##width##_ADDRESS(CIA_PCI_CONF, off);	\
	alpha_mb();							\
	CIA_TYPE1_SETUP(b,ipl,old_cfg);					\
	if (!badaddr((caddr_t)kv, sizeof(type))) {			\
		val = SPARSE_##width##_EXTRACT(off, SPARSE_READ(kv));	\
	}								\
        CIA_TYPE1_TEARDOWN(b,ipl,old_cfg);				\
	return val;							

#define SWIZ_CFGWRITE(b, s, f, r, data, width, type)			\
	int ipl = 0;							\
	u_int32_t old_cfg = 0;						\
	vm_offset_t off = CIA_SWIZ_CFGOFF(b, s, f, r);			\
	vm_offset_t kv = SPARSE_##width##_ADDRESS(CIA_PCI_CONF, off);	\
	alpha_mb();							\
	CIA_TYPE1_SETUP(b,ipl,old_cfg);					\
	if (!badaddr((caddr_t)kv, sizeof(type))) {			\
                SPARSE_WRITE(kv, SPARSE_##width##_INSERT(off, data));	\
		alpha_wmb();						\
	}								\
        CIA_TYPE1_TEARDOWN(b,ipl,old_cfg);				\
	return;							

static u_int8_t
cia_swiz_cfgreadb(u_int b, u_int s, u_int f, u_int r)
{
	SWIZ_CFGREAD(b, s, f, r, BYTE, u_int8_t);
}

static u_int16_t
cia_swiz_cfgreadw(u_int b, u_int s, u_int f, u_int r)
{
	SWIZ_CFGREAD(b, s, f, r, WORD, u_int16_t);
}

static u_int32_t
cia_swiz_cfgreadl(u_int b, u_int s, u_int f, u_int r)
{
	SWIZ_CFGREAD(b, s, f, r, LONG, u_int32_t);
}

static void
cia_swiz_cfgwriteb(u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	SWIZ_CFGWRITE(b, s, f, r, data, BYTE, u_int8_t);
}

static void
cia_swiz_cfgwritew(u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	SWIZ_CFGWRITE(b, s, f, r, data, WORD, u_int16_t);
}

static void
cia_swiz_cfgwritel(u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	SWIZ_CFGWRITE(b, s, f, r, data, LONG, u_int32_t);
}

static int cia_probe(device_t dev);
static int cia_attach(device_t dev);
static void *cia_create_intr(device_t dev, device_t child, int irq, driver_intr_t *intr, void *arg);
static int cia_connect_intr(device_t dev, void* ih);

static device_method_t cia_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cia_probe),
	DEVMETHOD(device_attach,	cia_attach),

	/* Bus interface */
	DEVMETHOD(bus_create_intr,	cia_create_intr),
	DEVMETHOD(bus_connect_intr,	cia_connect_intr),

	{ 0, 0 }
};

static driver_t cia_driver = {
	"cia",
	cia_methods,
	DRIVER_TYPE_MISC,
	sizeof(struct cia_softc),
};

void
cia_init()
{
	static int initted = 0;

	if (initted) return;
	initted = 1;

	if (alpha_implver() != ALPHA_IMPLVER_EV5
	    || alpha_amask(ALPHA_AMASK_BWX))
		chipset = cia_swiz_chipset;
	else
		chipset = cia_bwx_chipset;
	cia_hae_mem = REGVAL(CIA_CSR_HAE_MEM);

#if 0
	chipset = cia_swiz_chipset; /* XXX */
#endif

	if (platform.pci_intr_init)
		platform.pci_intr_init();
}

static int
cia_probe(device_t dev)
{
	if (cia0)
		return ENXIO;
	cia0 = dev;
	device_set_desc(dev, "2117x PCI adapter"); /* XXX */

	device_add_child(dev, "isa", 0, 0);

	return 0;
}

static int
cia_attach(device_t dev)
{
	struct cia_softc* sc = CIA_SOFTC(dev);

	cia_init();
	chipset.intrdev = dev;

	if (!platform.iointr)	/* XXX */
		set_iointr(alpha_dispatch_intr);

	bus_generic_attach(dev);
	return 0;
}

static void *
cia_create_intr(device_t dev, device_t child,
		int irq, driver_intr_t *intr, void *arg)
{
	return alpha_create_intr(0x900 + (irq << 4), intr, arg);
}

static int
cia_connect_intr(device_t dev, void* ih)
{
	struct alpha_intr *i = ih;
	int s = splhigh();
	int error = alpha_connect_intr(i);
	if (!error) {
		/* Enable PCI interrupt */
		platform.pci_intr_enable((i->vector - 0x900) >> 4);
	}
	splx(s);
	return error;
}

DRIVER_MODULE(cia, root, cia_driver, cia_devclass, 0, 0);

