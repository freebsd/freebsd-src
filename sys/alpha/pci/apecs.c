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
 *	$Id: cia.c,v 1.3 1998/07/22 08:32:17 dfr Exp $
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
#include <sys/bus.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>
#include <machine/intr.h>
#include <machine/cpuconf.h>
#include <machine/swiz.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	apecs_devclass;
static device_t		apecs0;		/* XXX only one for now */
static device_t		isa0;

struct apecs_softc {
	vm_offset_t	dmem_base;	/* dense memory */
	vm_offset_t	smem_base;	/* sparse memory */
	vm_offset_t	io_base;	/* dense i/o */
	vm_offset_t	cfg0_base;	/* dense pci0 config */
	vm_offset_t	cfg1_base;	/* dense pci1 config */
};

#define APECS_SOFTC(dev)	(struct apecs_softc*) device_get_softc(dev)

static alpha_chipset_inb_t	apecs_swiz_inb;
static alpha_chipset_inw_t	apecs_swiz_inw;
static alpha_chipset_inl_t	apecs_swiz_inl;
static alpha_chipset_outb_t	apecs_swiz_outb;
static alpha_chipset_outw_t	apecs_swiz_outw;
static alpha_chipset_outl_t	apecs_swiz_outl;
static alpha_chipset_readb_t	apecs_swiz_readb;
static alpha_chipset_readw_t	apecs_swiz_readw;
static alpha_chipset_readl_t	apecs_swiz_readl;
static alpha_chipset_writeb_t	apecs_swiz_writeb;
static alpha_chipset_writew_t	apecs_swiz_writew;
static alpha_chipset_writel_t	apecs_swiz_writel;
static alpha_chipset_maxdevs_t	apecs_swiz_maxdevs;
static alpha_chipset_cfgreadb_t	apecs_swiz_cfgreadb;
static alpha_chipset_cfgreadw_t	apecs_swiz_cfgreadw;
static alpha_chipset_cfgreadl_t	apecs_swiz_cfgreadl;
static alpha_chipset_cfgwriteb_t  apecs_swiz_cfgwriteb;
static alpha_chipset_cfgwritew_t  apecs_swiz_cfgwritew;
static alpha_chipset_cfgwritel_t  apecs_swiz_cfgwritel;

static alpha_chipset_t apecs_swiz_chipset = {
	apecs_swiz_inb,
	apecs_swiz_inw,
	apecs_swiz_inl,
	apecs_swiz_outb,
	apecs_swiz_outw,
	apecs_swiz_outl,
	apecs_swiz_readb,
	apecs_swiz_readw,
	apecs_swiz_readl,
	apecs_swiz_writeb,
	apecs_swiz_writew,
	apecs_swiz_writel,
	apecs_swiz_maxdevs,
	apecs_swiz_cfgreadb,
	apecs_swiz_cfgreadw,
	apecs_swiz_cfgreadl,
	apecs_swiz_cfgwriteb,
	apecs_swiz_cfgwritew,
	apecs_swiz_cfgwritel,
};

static int
apecs_swiz_maxdevs(u_int b)
{
	return 12;		/* XXX */
}



static u_int8_t
apecs_swiz_inb(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_BYTE(KV(APECS_PCI_SIO), port);
}

static u_int16_t
apecs_swiz_inw(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_WORD(KV(APECS_PCI_SIO), port);
}

static u_int32_t
apecs_swiz_inl(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_LONG(KV(APECS_PCI_SIO), port);
}

static void
apecs_swiz_outb(u_int32_t port, u_int8_t data)
{
	SPARSE_WRITE_BYTE(KV(APECS_PCI_SIO), port, data);
	alpha_wmb();
}

static void
apecs_swiz_outw(u_int32_t port, u_int16_t data)
{
	SPARSE_WRITE_WORD(KV(APECS_PCI_SIO), port, data);
	alpha_wmb();
}

static void
apecs_swiz_outl(u_int32_t port, u_int32_t data)
{
	SPARSE_WRITE_LONG(KV(APECS_PCI_SIO), port, data);
	alpha_wmb();
}

/*
 * Memory functions.
 * 
 * XXX linux does 32-bit reads/writes via dense space.  This doesn't
 *     appear to work for devices behind a ppb.  I'm using sparse
 *     accesses & they appear to work just fine everywhere.
 */

static u_int32_t	apecs_hae_mem;

#define REG1 (1UL << 24)
static __inline  void
apecs_swiz_set_hae_mem(u_int32_t *pa)
{
	int s; 
	u_int32_t msb;
	if(*pa >= REG1){
		msb = *pa & 0xf8000000;
		*pa -= msb;
		s = splhigh();
                if (msb != apecs_hae_mem) {
			apecs_hae_mem = msb;
			REGVAL(EPIC_HAXR1) = apecs_hae_mem;
			alpha_mb();
			apecs_hae_mem = REGVAL(EPIC_HAXR1);
		}
		splx(s);
	}
}

static u_int8_t
apecs_swiz_readb(u_int32_t pa)
{
	alpha_mb();
	apecs_swiz_set_hae_mem(&pa);
	return SPARSE_READ_BYTE(KV(APECS_PCI_SPARSE), pa);
}

static u_int16_t
apecs_swiz_readw(u_int32_t pa)
{
	alpha_mb();
	apecs_swiz_set_hae_mem(&pa);
	return SPARSE_READ_WORD(KV(APECS_PCI_SPARSE), pa);
}

static u_int32_t
apecs_swiz_readl(u_int32_t pa)
{
	alpha_mb();
	apecs_swiz_set_hae_mem(&pa);
	return SPARSE_READ_LONG(KV(APECS_PCI_SPARSE), pa);
}

static void
apecs_swiz_writeb(u_int32_t pa, u_int8_t data)
{
	apecs_swiz_set_hae_mem(&pa);
	SPARSE_WRITE_BYTE(KV(APECS_PCI_SPARSE), pa, data);
	alpha_wmb();
}

static void
apecs_swiz_writew(u_int32_t pa, u_int16_t data)
{
	apecs_swiz_set_hae_mem(&pa);
	SPARSE_WRITE_WORD(KV(APECS_PCI_SPARSE), pa, data);
	alpha_wmb();
}


static void
apecs_swiz_writel(u_int32_t pa, u_int32_t data)
{
	apecs_swiz_set_hae_mem(&pa);
	SPARSE_WRITE_LONG(KV(APECS_PCI_SPARSE), pa, data);
	alpha_wmb();

}


#define APECS_SWIZ_CFGOFF(b, s, f, r) \
	(((b) << 16) | ((s) << 11) | ((f) << 8) | (r))

#define APECS_TYPE1_SETUP(b,s,old_haxr2) if((b)) {		\
        do {						\
		(s) = splhigh();			\
		(old_haxr2) = REGVAL(EPIC_HAXR2);	\
		alpha_mb();				\
		REGVAL(EPIC_HAXR2) = (old_haxr2) | 0x1;	\
		alpha_mb();				\
        } while(0);					\
}

#define APECS_TYPE1_TEARDOWN(b,s,old_haxr2) if((b)) {	\
        do {						\
		alpha_mb();				\
		REGVAL(EPIC_HAXR2) = (old_haxr2);	\
		alpha_mb();				\
		splx((s));				\
        } while(0);					\
}

#define SWIZ_CFGREAD(b, s, f, r, width, type)				\
	type val = ~0;							\
	int ipl = 0;							\
	u_int32_t old_haxr2 = 0;					\
	struct apecs_softc* sc = APECS_SOFTC(apecs0);			\
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);		\
	vm_offset_t kv = SPARSE_##width##_ADDRESS(sc->cfg0_base, off);	\
	alpha_mb();							\
	APECS_TYPE1_SETUP(b,ipl,old_haxr2);				\
	if (!badaddr((caddr_t)kv, sizeof(type))) {			\
		val = SPARSE_##width##_EXTRACT(off, SPARSE_READ(kv));	\
	}								\
        APECS_TYPE1_TEARDOWN(b,ipl,old_haxr2);				\
	return val;							

#define SWIZ_CFGWRITE(b, s, f, r, data, width, type)			\
	int ipl = 0;							\
	u_int32_t old_haxr2 = 0;					\
	struct apecs_softc* sc = APECS_SOFTC(apecs0);			\
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);		\
	vm_offset_t kv = SPARSE_##width##_ADDRESS(sc->cfg0_base, off);	\
	alpha_mb();							\
	APECS_TYPE1_SETUP(b,ipl,old_haxr2);				\
	if (!badaddr((caddr_t)kv, sizeof(type))) {			\
                SPARSE_WRITE(kv, SPARSE_##width##_INSERT(off, data));	\
		alpha_wmb();						\
	}								\
        APECS_TYPE1_TEARDOWN(b,ipl,old_haxr2);				\
	return;							

#if 1
static u_int8_t
apecs_swiz_cfgreadb(u_int b, u_int s, u_int f, u_int r)
{
	SWIZ_CFGREAD(b, s, f, r, BYTE, u_int8_t);
}

static u_int16_t
apecs_swiz_cfgreadw(u_int b, u_int s, u_int f, u_int r)
{
	SWIZ_CFGREAD(b, s, f, r, WORD, u_int16_t);
}

static u_int32_t
apecs_swiz_cfgreadl(u_int b, u_int s, u_int f, u_int r)
{
	SWIZ_CFGREAD(b, s, f, r, LONG, u_int32_t);
}

static void
apecs_swiz_cfgwriteb(u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	SWIZ_CFGWRITE(b, s, f, r, data, BYTE, u_int8_t);
}

static void
apecs_swiz_cfgwritew(u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	SWIZ_CFGWRITE(b, s, f, r, data, WORD, u_int16_t);
}

static void
apecs_swiz_cfgwritel(u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	SWIZ_CFGWRITE(b, s, f, r, data, LONG, u_int32_t);
}

#else
static u_int8_t
apecs_swiz_cfgreadb(u_int b, u_int s, u_int f, u_int r)
{
	struct apecs_softc* sc = APECS_SOFTC(apecs0);
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);
	alpha_mb();
	if (badaddr((caddr_t)(sc->cfg0_base + SPARSE_BYTE_OFFSET(off)), 1)) return ~0;
	return SPARSE_READ_BYTE(sc->cfg0_base, off);
}

static u_int16_t
apecs_swiz_cfgreadw(u_int b, u_int s, u_int f, u_int r)
{
	struct apecs_softc* sc = APECS_SOFTC(apecs0);
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);
	alpha_mb();
	if (badaddr((caddr_t)(sc->cfg0_base + SPARSE_WORD_OFFSET(off)), 2)) return ~0;
	return SPARSE_READ_WORD(sc->cfg0_base, off);
}

static u_int32_t
apecs_swiz_cfgreadl(u_int b, u_int s, u_int f, u_int r)
{
	struct apecs_softc* sc = APECS_SOFTC(apecs0);
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);
	alpha_mb();
	if (badaddr((caddr_t)(sc->cfg0_base + SPARSE_LONG_OFFSET(off)), 4)) return ~0;
	return SPARSE_READ_LONG(sc->cfg0_base, off);
}

static void
apecs_swiz_cfgwriteb(u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	struct apecs_softc* sc = APECS_SOFTC(apecs0);
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);
	if (badaddr((caddr_t)(sc->cfg0_base + SPARSE_BYTE_OFFSET(off)), 1)) return;
	SPARSE_WRITE_BYTE(sc->cfg0_base, off, data);
	alpha_wmb();
}

static void
apecs_swiz_cfgwritew(u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	struct apecs_softc* sc = APECS_SOFTC(apecs0);
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);
	if (badaddr((caddr_t)(sc->cfg0_base + SPARSE_WORD_OFFSET(off)), 2)) return;
	SPARSE_WRITE_WORD(sc->cfg0_base, off, data);
	alpha_wmb();
}

static void
apecs_swiz_cfgwritel(u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	struct apecs_softc* sc = APECS_SOFTC(apecs0);
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);
	if (badaddr((caddr_t)(sc->cfg0_base + SPARSE_LONG_OFFSET(off)), 4)) return;
	SPARSE_WRITE_LONG(sc->cfg0_base, off, data);
	alpha_wmb();
}
#endif

static int apecs_probe(device_t dev);
static int apecs_attach(device_t dev);
static void *apecs_create_intr(device_t dev, device_t child, int irq, driver_intr_t *intr, void *arg);
static int apecs_connect_intr(device_t dev, void* ih);

static device_method_t apecs_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		apecs_probe),
	DEVMETHOD(device_attach,	apecs_attach),

	/* Bus interface */

	{ 0, 0 }
};

static driver_t apecs_driver = {
	"apecs",
	apecs_methods,
	DRIVER_TYPE_MISC,
	sizeof(struct apecs_softc),
};

void
apecs_init()
{
	static int initted = 0;

	if (initted) return;
	initted = 1;

	if (platform.pci_intr_init)
		platform.pci_intr_init();

	chipset = apecs_swiz_chipset;
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

	isa0 = device_add_child(dev, "isa", 0, 0);

	return 0;
}

extern void isa_intr(void* frame, u_long vector);

static int
apecs_attach(device_t dev)
{
	struct apecs_softc* sc = APECS_SOFTC(dev);
	apecs_init();
	chipset.intrdev = isa0;

	sc->dmem_base = APECS_PCI_DENSE;
	sc->smem_base = APECS_PCI_SPARSE;
	sc->io_base = APECS_PCI_SIO;
	sc->cfg0_base = KV(APECS_PCI_CONF);
	sc->cfg1_base = NULL;

	set_iointr(alpha_dispatch_intr);

	bus_generic_attach(dev);
	return 0;
}

DRIVER_MODULE(apecs, root, apecs_driver, apecs_devclass, 0, 0);

