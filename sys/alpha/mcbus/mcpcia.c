/* $FreeBSD$ */
/*
 * Copyright (c) 2000 Matthew Jacob
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/swiz.h>
#include <machine/intr.h>
#include <machine/intrcnt.h>
#include <machine/resource.h>
#include <machine/sgmap.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
 

#include <alpha/mcbus/mcbusreg.h>
#include <alpha/mcbus/mcbusvar.h>

#include <alpha/mcbus/mcpciareg.h>
#include <alpha/mcbus/mcpciavar.h>
#include <alpha/pci/pcibus.h>
#include <pci/pcivar.h>

static devclass_t	mcpcia_devclass;

/* We're only allowing for one MCBUS right now */
static device_t		mcpcias[MCPCIA_PER_MCBUS];

#define KV(pa)		((void *)ALPHA_PHYS_TO_K0SEG(pa))

struct mcpcia_softc {
	struct mcpcia_softc *next;
	device_t	dev;		/* backpointer */
	u_int64_t	sysbase;	/* shorthand */
	vm_offset_t	dmem_base;	/* dense memory */
	vm_offset_t	smem_base;	/* sparse memory */
	vm_offset_t	io_base;	/* sparse i/o */
	int		mcpcia_inst;	/* our mcpcia instance # */
};
static struct mcpcia_softc *mcpcia_eisa = NULL;
extern void dec_kn300_cons_init(void);


static int mcpcia_probe(device_t dev);
static int mcpcia_attach(device_t dev);

static int mcpcia_setup_intr(device_t, device_t, struct resource *, int,
    driver_intr_t *, void *, void **);
static int
mcpcia_teardown_intr(device_t, device_t, struct resource *, void *);
static driver_intr_t mcpcia_intr;
static void mcpcia_enable_intr(struct mcpcia_softc *, int);
static void mcpcia_disable_intr(struct mcpcia_softc *, int);


static device_method_t mcpcia_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			mcpcia_probe),
	DEVMETHOD(device_attach,		mcpcia_attach),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr,		mcpcia_setup_intr),
	DEVMETHOD(bus_teardown_intr,		mcpcia_teardown_intr),
	DEVMETHOD(bus_alloc_resource,		pci_alloc_resource),
	DEVMETHOD(bus_release_resource,		pci_release_resource),
	DEVMETHOD(bus_activate_resource,	pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	pci_deactivate_resource),

	{ 0, 0 }
};
static driver_t mcpcia_driver = {
	"mcpcia", mcpcia_methods, sizeof (struct mcpcia_softc)
};

/*
 * SGMAP window for ISA: 8M at 8M
 */
#define	MCPCIA_ISA_SG_MAPPED_BASE	(8*1024*1024)
#define	MCPCIA_ISA_SG_MAPPED_SIZE	(8*1024*1024)

/*
 * Direct-mapped window: 2G at 2G
 */
#define	MCPCIA_DIRECT_MAPPED_BASE	(2UL*1024UL*1024UL*1024UL)
#define	MCPCIA_DIRECT_MAPPED_SIZE	(2UL*1024UL*1024UL*1024UL)

/*
 * SGMAP window for PCI: 1G at 1G
 */
#define	MCPCIA_PCI_SG_MAPPED_BASE	(1UL*1024UL*1024UL*1024UL)
#define	MCPCIA_PCI_SG_MAPPED_SIZE	(1UL*1024UL*1024UL*1024UL)

#define	MCPCIA_SGTLB_INVALIDATE(sc)					\
do {									\
	alpha_mb();							\
	REGVAL(MCPCIA_SG_TBIA(sc)) = 0xdeadbeef;			\
	alpha_mb();							\
} while (0)

static void mcpcia_dma_init(struct mcpcia_softc *);
static void mcpcia_sgmap_map(void *, vm_offset_t, vm_offset_t);

#define MCPCIA_SOFTC(dev)	(struct mcpcia_softc *) device_get_softc(dev)

static struct mcpcia_softc *mcpcia_root;

static alpha_chipset_inb_t	mcpcia_inb;
static alpha_chipset_inw_t	mcpcia_inw;
static alpha_chipset_inl_t	mcpcia_inl;
static alpha_chipset_outb_t	mcpcia_outb;
static alpha_chipset_outw_t	mcpcia_outw;
static alpha_chipset_outl_t	mcpcia_outl;
static alpha_chipset_readb_t	mcpcia_readb;
static alpha_chipset_readw_t	mcpcia_readw;
static alpha_chipset_readl_t	mcpcia_readl;
static alpha_chipset_writeb_t	mcpcia_writeb;
static alpha_chipset_writew_t	mcpcia_writew;
static alpha_chipset_writel_t	mcpcia_writel;
static alpha_chipset_maxdevs_t	mcpcia_maxdevs;
static alpha_chipset_cfgreadb_t	mcpcia_cfgreadb;
static alpha_chipset_cfgreadw_t	mcpcia_cfgreadw;
static alpha_chipset_cfgreadl_t	mcpcia_cfgreadl;
static alpha_chipset_cfgwriteb_t mcpcia_cfgwriteb;
static alpha_chipset_cfgwritew_t mcpcia_cfgwritew;
static alpha_chipset_cfgwritel_t mcpcia_cfgwritel;

static alpha_chipset_t mcpcia_chipset = {
	mcpcia_inb,
	mcpcia_inw,
	mcpcia_inl,
	mcpcia_outb,
	mcpcia_outw,
	mcpcia_outl,
	mcpcia_readb,
	mcpcia_readw,
	mcpcia_readl,
	mcpcia_writeb,
	mcpcia_writew,
	mcpcia_writel,
	mcpcia_maxdevs,
	mcpcia_cfgreadb,
	mcpcia_cfgreadw,
	mcpcia_cfgreadl,
	mcpcia_cfgwriteb,
	mcpcia_cfgwritew,
	mcpcia_cfgwritel,
};

#define	MCPCIA_NMBR(port)	((port >> 30) & 0x3)
#define	MCPCIA_INST(port)	mcpcias[MCPCIA_NMBR(port)]
#define	MCPCIA_ADDR(port)	(port & 0x3fffffff)

static u_int8_t
mcpcia_inb(u_int32_t port)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(port));
	if (port < (1 << 16)) {
		if (mcpcia_eisa == NULL) {
			return (0xff);
		}
		return SPARSE_READ_BYTE(mcpcia_eisa->io_base, port);
	}
	return SPARSE_READ_BYTE(sc->io_base, MCPCIA_ADDR(port));
}

static u_int16_t
mcpcia_inw(u_int32_t port)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(port));
	if (port < (1 << 16)) {
		if (mcpcia_eisa == NULL) {
			return (0xffff);
		}
		return SPARSE_READ_WORD(mcpcia_eisa->io_base, port);
	}
	return SPARSE_READ_WORD(sc->io_base, MCPCIA_ADDR(port));
}

static u_int32_t
mcpcia_inl(u_int32_t port)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(port));
	return SPARSE_READ_LONG(sc->io_base, MCPCIA_ADDR(port));
}

static void
mcpcia_outb(u_int32_t port, u_int8_t data)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(port));
	if (port < (1 << 16)) {
		if (mcpcia_eisa)
			SPARSE_WRITE_BYTE(mcpcia_eisa->io_base, port, data);
	} else {
		SPARSE_WRITE_BYTE(sc->io_base, MCPCIA_ADDR(port), data);
	}
	alpha_mb();
}

static void
mcpcia_outw(u_int32_t port, u_int16_t data)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(port));
	if (port < (1 << 16)) {
		if (mcpcia_eisa)
			SPARSE_WRITE_WORD(mcpcia_eisa->io_base, port, data);
	} else {
		SPARSE_WRITE_WORD(sc->io_base, MCPCIA_ADDR(port), data);
	}
	alpha_mb();
}

static void
mcpcia_outl(u_int32_t port, u_int32_t data)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(port));
	SPARSE_WRITE_LONG(sc->io_base, MCPCIA_ADDR(port), data);
	alpha_mb();
}

static u_int8_t
mcpcia_readb(u_int32_t pa)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(pa));
	if (pa < (8 << 20)) {
		if (mcpcia_eisa == NULL) {
			return (0xff);
		}
		return SPARSE_READ_BYTE(mcpcia_eisa->smem_base, pa);
	}
	return SPARSE_READ_BYTE(sc->smem_base, MCPCIA_ADDR(pa));
}

static u_int16_t
mcpcia_readw(u_int32_t pa)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(pa));
	if (pa < (8 << 20)) {
		if (mcpcia_eisa == NULL) {
			return (0xffff);
		}
		return SPARSE_READ_WORD(mcpcia_eisa->smem_base, pa);
	}
	return SPARSE_READ_WORD(sc->smem_base, MCPCIA_ADDR(pa));
}

static u_int32_t
mcpcia_readl(u_int32_t pa)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(pa));
	return SPARSE_READ_LONG(sc->smem_base, MCPCIA_ADDR(pa));
}

static void
mcpcia_writeb(u_int32_t pa, u_int8_t data)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(pa));
	if (pa < (8 << 20)) {
		if (mcpcia_eisa)
			SPARSE_WRITE_BYTE(mcpcia_eisa->smem_base, pa, data);
	} else {
		SPARSE_WRITE_BYTE(sc->smem_base, MCPCIA_ADDR(pa), data);
	}
	alpha_mb();
}

static void
mcpcia_writew(u_int32_t pa, u_int16_t data)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(pa));
	if (pa < (8 << 20)) {
		if (mcpcia_eisa)
			SPARSE_WRITE_WORD(mcpcia_eisa->smem_base, pa, data);
	} else {
		SPARSE_WRITE_WORD(sc->smem_base, MCPCIA_ADDR(pa), data);
	}
	alpha_mb();
}

static void
mcpcia_writel(u_int32_t pa, u_int32_t data)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(MCPCIA_INST(pa));
	SPARSE_WRITE_LONG(sc->smem_base, MCPCIA_ADDR(pa), data);
	alpha_mb();
}

static int
mcpcia_maxdevs(u_int b)
{
	return (MCPCIA_MAXDEV);
}

static u_int32_t mcpcia_cfgread(u_int, u_int, u_int, u_int, u_int, int);
static void mcpcia_cfgwrite(u_int, u_int, u_int, u_int, u_int, int, u_int32_t);

#if	0
#define	RCFGP	printf
#else
#define	RCFGP	if (0) printf
#endif

static u_int32_t
mcpcia_cfgread(u_int bh, u_int bus, u_int slot, u_int func, u_int off, int sz)
{
	device_t dev;
	struct mcpcia_softc *sc;
	u_int32_t *dp, data, rvp;
	u_int64_t paddr;

	RCFGP("CFGREAD %u.%u.%u.%u.%u.%d", bh, bus, slot, func, off, sz);
	rvp = data = ~0;
	if (bh == (u_int8_t)-1)
		bh = bus >> 4;
	dev = mcpcias[bh];
	if (dev == (device_t) 0) {
		RCFGP(" (no dev)\n");
		return (data);
	}
	sc = MCPCIA_SOFTC(dev);
	bus &= 0xf;

	/*
	 * There's nothing in slot 0 on a primary bus.
	 */
	if (bus == 0 && (slot < 1 || slot >= MCPCIA_MAXDEV)) {
		RCFGP(" (no slot)\n");
		return (data);
	}

	paddr = bus << 21;
	paddr |= slot << 16;
	paddr |= func << 13;
	paddr |= ((sz - 1) << 3);
	paddr |= ((unsigned long) ((off >> 2) << 7));
	paddr |= MCPCIA_PCI_CONF;
	paddr |= sc->sysbase;
	dp = (u_int32_t *)KV(paddr);
	RCFGP(" hose %d MID%d paddr 0x%lx", bh, mcbus_get_mid(dev), paddr);
	if (badaddr(dp, sizeof (*dp)) == 0) {
		data = *dp;
	}
	if (data != ~0) {
		if (sz == 1) {
			rvp = SPARSE_BYTE_EXTRACT(off, data);
		} else if (sz == 2) {
			rvp = SPARSE_WORD_EXTRACT(off, data);
		} else {
			rvp = data;
		}
	} else {
		rvp = data;
	}
	RCFGP(" data %x->0x%x\n", data, rvp);
	return (rvp);
}

#if	0
#define	WCFGP	printf
#else
#define	WCFGP	if (0) printf
#endif

static void
mcpcia_cfgwrite(u_int bh, u_int bus, u_int slot, u_int func, u_int off,
	int sz, u_int32_t data)
{
	device_t dev;
	struct mcpcia_softc *sc;
	u_int32_t *dp;
	u_int64_t paddr;

	WCFGP("CFGWRITE %u.%u.%u.%u.%u.%d", bh, bus, slot, func, off, sz);
	if (bh == (u_int8_t)-1)
		bh = bus >> 4;
	dev = mcpcias[bh];
	if (dev == (device_t) 0) {
		WCFGP(" (no dev)\n");
		return;
	}
	sc = MCPCIA_SOFTC(dev);
	bus &= 0xf;

	/*
	 * There's nothing in slot 0 on a primary bus.
	 */
	if (bus == 0 && (slot < 1 || slot >= MCPCIA_MAXDEV)) {
		WCFGP(" (no slot)\n");
		return;
	}

	paddr = bus << 21;
	paddr |= slot << 16;
	paddr |= func << 13;
	paddr |= ((sz - 1) << 3);
	paddr |= ((unsigned long) ((off >> 2) << 7));
	paddr |= MCPCIA_PCI_CONF;
	paddr |= sc->sysbase;
	dp = (u_int32_t *)KV(paddr);
	WCFGP(" hose %d MID%d paddr 0x%lx\n", bh, mcbus_get_mid(dev), paddr);
	if (badaddr(dp, sizeof (*dp)) == 0) {
		u_int32_t new_data;
		if (sz == 1) {
			new_data = SPARSE_BYTE_INSERT(off, data);
		} else if (sz == 2) {
			new_data = SPARSE_WORD_INSERT(off, data);
		} else  {
			new_data = data;
		}
		*dp = new_data;
	}
}

static u_int8_t
mcpcia_cfgreadb(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	return (u_int8_t) mcpcia_cfgread(h, b, s, f, r, 1);
}

static u_int16_t
mcpcia_cfgreadw(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	return (u_int16_t) mcpcia_cfgread(h, b, s, f, r, 2);
}

static u_int32_t
mcpcia_cfgreadl(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	return mcpcia_cfgread(h, b, s, f, r, 4);
}

static void
mcpcia_cfgwriteb(u_int h, u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	mcpcia_cfgwrite(h, b, s, f, r, 1, (u_int32_t) data);
}

static void
mcpcia_cfgwritew(u_int h, u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	mcpcia_cfgwrite(h, b, s, f, r, 2, (u_int32_t) data);
}

static void
mcpcia_cfgwritel(u_int h, u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	mcpcia_cfgwrite(h, b, s, f, r, 4, (u_int32_t) data);
}

static int
mcpcia_probe(device_t dev)
{
	device_t child;
	int unit;
	struct mcpcia_softc *xc, *sc = MCPCIA_SOFTC(dev);

	unit = device_get_unit(dev);
	if (mcpcias[unit]) {
		printf("%s: already attached\n", device_get_nameunit(dev));
		return EEXIST;
	}
	sc->mcpcia_inst = unit;
	if ((xc = mcpcia_root) == NULL) {
		mcpcia_root = sc;
	} else {
		while (xc->next)
			xc = xc->next;
		xc->next = sc;
	}
	sc->dev = mcpcias[unit] = dev;
	/* PROBE ? */
	device_set_desc(dev, "MCPCIA PCI Adapter");
	if (unit == 0) {
		pci_init_resources();
	}
	child = device_add_child(dev, "pcib", unit);
	device_set_ivars(child, &sc->mcpcia_inst);
	return (0);
}

static int
mcpcia_attach(device_t dev)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);
	device_t p = device_get_parent(dev);
	vm_offset_t regs;
	u_int32_t ctl;
	int mid, gid, rval;
	void *intr;

	chipset = mcpcia_chipset;
	mid = mcbus_get_mid(dev);
	gid = mcbus_get_gid(dev);

	sc->sysbase = MCBUS_IOSPACE |
	    (((u_int64_t) gid) << MCBUS_GID_SHIFT) | \
	    (((u_int64_t) mid) << MCBUS_MID_SHIFT);
	regs		= (vm_offset_t) KV(sc->sysbase);
	sc->dmem_base	= regs + MCPCIA_PCI_DENSE;
	sc->smem_base	= regs + MCPCIA_PCI_SPARSE;
	sc->io_base	= regs + MCPCIA_PCI_IOSPACE;

	/*
 	 * Disable interrupts and clear errors prior to probing
	 */
	REGVAL(MCPCIA_INT_MASK0(sc)) = 0;
	REGVAL(MCPCIA_INT_MASK1(sc)) = 0;
	REGVAL(MCPCIA_CAP_ERR(sc)) = 0xFFFFFFFF;
	alpha_mb();


	/*
	 * Say who we are
	 */
	ctl = REGVAL(MCPCIA_PCI_REV(sc));
	printf("%s: Horse Revision %d, %s Handed Saddle Revision %d,"
	    " CAP Revision %d\n", device_get_nameunit(dev), HORSE_REV(ctl),
	    (SADDLE_TYPE(ctl) & 1)? "Right": "Left", SADDLE_REV(ctl),
	    CAP_REV(ctl));

	/*
	 * See if we're the fella with the EISA bus...
	 */

	if (EISA_PRESENT(REGVAL(MCPCIA_PCI_REV(sc)))) {
		mcpcia_eisa = sc;
	}

	/*
	 * Set up DMA stuff here.
	 */

	mcpcia_dma_init(sc);

	/*
	 * Register our interrupt service requirements with out parent.
	 */
	rval =
	    BUS_SETUP_INTR(p, dev, NULL, INTR_TYPE_MISC, mcpcia_intr, 0, &intr);
	if (rval == 0) {
		if (sc == mcpcia_eisa) {
			printf("Attaching Real Console\n");
			dec_kn300_cons_init();
			/*
			 * Enable EISA interrupts.
			 */
			mcpcia_enable_intr(sc, 16);
		}
		bus_generic_attach(dev);
	}
	return (rval);
}

static void
mcpcia_enable_intr(struct mcpcia_softc *sc, int irq)
{
	alpha_mb();
	REGVAL(MCPCIA_INT_MASK0(sc)) |= (1 << irq);
	alpha_mb();
}

static void
mcpcia_disable_intr(struct mcpcia_softc *sc, int irq)
{
	alpha_mb();
	REGVAL(MCPCIA_INT_MASK0(sc)) &= ~(1 << irq);
	alpha_mb();
}

static int
mcpcia_setup_intr(device_t dev, device_t child, struct resource *ir, int flags,
       driver_intr_t *intr, void *arg, void **cp)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);
	int slot, mid, gid, birq, irq, error, intpin, h;
	
	intpin = pci_get_intpin(child);
	if (intpin == 0) {
		/* No IRQ used */
		return (0);
	}
	if (intpin < 1 || intpin > 4) {
		/* Bad IRQ */
		return (ENXIO);
	}

	slot = pci_get_slot(child);
	mid = mcbus_get_mid(dev);
	gid = mcbus_get_gid(dev);

	if (slot == 0) {
		device_t bdev; 
		/* bridged - get slot from granparent */
		/* note that this is broken for all but the most trival case */
		bdev = device_get_parent(device_get_parent(child));
		slot = pci_get_slot(bdev);
	}

	if (mid == 5 && slot == 1) {
		irq = 16;	/* MID 5, slot 1, is the internal NCR 53c810 */
	} else if (slot >= 2 && slot <= 5) {
		irq = (slot - 2) * 4;
	} else {
		device_printf(child, "wierd slot number (%d); can't make irq\n",
		    slot);
		return (ENXIO);
	}
	error = rman_activate_resource(ir);
	if (error)
		return error;

	/*
	 * We now construct a vector as the hardware would, unless
	 * this is the internal NCR 53c810 interrupt.
	 */
	if (irq == 16) {
		h = MCPCIA_VEC_NCR;
	} else {
		h = MCPCIA_VEC_PCI + ((mid - 4) * MCPCIA_VECWIDTH_PER_MCPCIA) +
		    (slot * MCPCIA_VECWIDTH_PER_SLOT) +
		    ((intpin - 1) * MCPCIA_VECWIDTH_PER_INTPIN);
	}
	birq = irq + INTRCNT_KN300_IRQ;
	error = alpha_setup_intr(h, intr, arg, cp, &intrcnt[birq]);
	if (error)
		return error;
	mcpcia_enable_intr(sc, irq);
	device_printf(child, "interrupting at IRQ 0x%x int%c (vec 0x%x)\n",
	    irq, intpin - 1 + 'A' , h);
	return (0);
}

static int
mcpcia_teardown_intr(device_t dev, device_t child, struct resource *i, void *c)
{
	struct mcpcia_softc *sc = MCPCIA_SOFTC(dev);
	int slot, mid, irq;

	slot = pci_get_slot(child);
	mid = mcbus_get_mid(dev);

	if (mid == 5 && slot == 1) {
		irq = 16;
	} else if (slot >= 2 && slot <= 5) {
		irq = (slot - 2) * 4;
	} else {
		return (ENXIO);
	}
	mcpcia_disable_intr(sc, irq);
	alpha_teardown_intr(c);
	return (rman_deactivate_resource(i));
}

static void
mcpcia_sgmap_map(void *arg, vm_offset_t ba, vm_offset_t pa)
{
	u_int64_t *sgtable = arg;
	int index = alpha_btop(ba - MCPCIA_ISA_SG_MAPPED_BASE);

	if (pa) {
		if (pa > (1L<<32))
			panic("mcpcia_sgmap_map: can't map address 0x%lx", pa);
		sgtable[index] = ((pa >> 13) << 1) | 1;
	} else {
		sgtable[index] = 0;
	}
	alpha_mb();
	MCPCIA_SGTLB_INVALIDATE(mcpcia_eisa);
}

static void
mcpcia_dma_init(struct mcpcia_softc *sc)
{

	/*
	 * Disable all windows first.
	 */

	REGVAL(MCPCIA_W0_BASE(sc)) = 0;
	REGVAL(MCPCIA_W1_BASE(sc)) = 0;
	REGVAL(MCPCIA_W2_BASE(sc)) = 0;
	REGVAL(MCPCIA_W3_BASE(sc)) = 0;
	REGVAL(MCPCIA_T0_BASE(sc)) = 0;
	REGVAL(MCPCIA_T1_BASE(sc)) = 0;
	REGVAL(MCPCIA_T2_BASE(sc)) = 0;
	REGVAL(MCPCIA_T3_BASE(sc)) = 0;
	alpha_mb();

	/*
	 * Set up window 0 as an 8MB SGMAP-mapped window starting at 8MB.
	 * Do this only for the EISA carrying MCPCIA. Partly because
	 * there's only one chipset sgmap thingie.
	 */

	if (sc == mcpcia_eisa) {
		void *sgtable;
		REGVAL(MCPCIA_W0_MASK(sc)) = MCPCIA_WMASK_8M;

		sgtable = contigmalloc(8192, M_DEVBUF,
		    M_NOWAIT, 0, 1L<<34, 32<<10, 1L<<34);

		if (sgtable == NULL) {
			panic("mcpcia_dma_init: cannot allocate sgmap");
			/* NOTREACHED */
		}
		REGVAL(MCPCIA_T0_BASE(sc)) =
		    pmap_kextract((vm_offset_t)sgtable) >> MCPCIA_TBASEX_SHIFT;

		alpha_mb();
		REGVAL(MCPCIA_W0_BASE(sc)) = MCPCIA_WBASE_EN |
		    MCPCIA_WBASE_SG | MCPCIA_ISA_SG_MAPPED_BASE;
		alpha_mb();
		MCPCIA_SGTLB_INVALIDATE(sc);
		chipset.sgmap = sgmap_map_create(MCPCIA_ISA_SG_MAPPED_BASE,
		    MCPCIA_ISA_SG_MAPPED_BASE + MCPCIA_ISA_SG_MAPPED_SIZE - 1,
		    mcpcia_sgmap_map, sgtable);
	}

	/*
	 * Set up window 1 as a 2 GB Direct-mapped window starting at 2GB.
	 */

	REGVAL(MCPCIA_W1_MASK(sc)) = MCPCIA_WMASK_2G;
	REGVAL(MCPCIA_T1_BASE(sc)) = 0;
	alpha_mb();
	REGVAL(MCPCIA_W1_BASE(sc)) =
		MCPCIA_DIRECT_MAPPED_BASE | MCPCIA_WBASE_EN;
	alpha_mb();

	/*
	 * When we get around to redoing the 'chipset' stuff to have more
	 * than one sgmap handler...
	 */

#if	0
	/*
	 * Set up window 2 as a 1G SGMAP-mapped window starting at 1G.
	 */

	REGVAL(MCPCIA_W2_MASK(sc)) = MCPCIA_WMASK_1G;
	REGVAL(MCPCIA_T2_BASE(sc)) =
		ccp->cc_pci_sgmap.aps_ptpa >> MCPCIA_TBASEX_SHIFT;
	alpha_mb();
	REGVAL(MCPCIA_W2_BASE(sc)) =
		MCPCIA_WBASE_EN | MCPCIA_WBASE_SG | MCPCIA_PCI_SG_MAPPED_BASE;
	alpha_mb();
#endif

	/* XXX XXX BEGIN XXX XXX */
	{							/* XXX */
		alpha_XXX_dmamap_or = MCPCIA_DIRECT_MAPPED_BASE;/* XXX */
	}							/* XXX */
	/* XXX XXX END XXX XXX */
}

/*
 */

static void
mcpcia_intr(void *arg)
{
	unsigned long vec = (unsigned long) arg;

	/*
	 * Check for I2C interrupts.  These are technically within
	 * the PCI vector range, but no PCI device should ever map
	 * to them.
	 */
	if (vec == MCPCIA_I2C_CVEC) {
		printf("i2c: controller interrupt\n");
		return;
	}
	if (vec == MCPCIA_I2C_BVEC) {
		printf("i2c: bus interrupt\n");
		return;
	}

	alpha_dispatch_intr(NULL, vec);
}
DRIVER_MODULE(mcpcia, mcbus, mcpcia_driver, mcpcia_devclass, 0, 0);
