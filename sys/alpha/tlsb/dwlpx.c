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
 * $FreeBSD: src/sys/alpha/tlsb/dwlpx.c,v 1.11 1999/08/28 00:39:15 peter Exp $
 */

#include "opt_simos.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/swiz.h>
#include <alpha/tlsb/dwlpxreg.h>

#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/tlsbvar.h>

#include <alpha/tlsb/kftxxvar.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)
#define DWLPX_BASE(n, h)	((((u_long)(n) - 4) << 36)	\
				 | ((u_long)(h) << 34)		\
				 | (1L << 39))

static devclass_t	dwlpx_devclass;
static device_t		dwlpx0;		/* XXX only one for now */

struct dwlpx_softc {
	vm_offset_t	dmem_base;	/* dense memory */
	vm_offset_t	smem_base;	/* sparse memory */
	vm_offset_t	io_base;	/* sparse i/o */
	vm_offset_t	cfg_base;	/* sparse pci config */
};

#define DWLPX_SOFTC(dev)	(struct dwlpx_softc*) device_get_softc(dev)

static alpha_chipset_inb_t	dwlpx_inb;
static alpha_chipset_inw_t	dwlpx_inw;
static alpha_chipset_inl_t	dwlpx_inl;
static alpha_chipset_outb_t	dwlpx_outb;
static alpha_chipset_outw_t	dwlpx_outw;
static alpha_chipset_outl_t	dwlpx_outl;
static alpha_chipset_readb_t	dwlpx_readb;
static alpha_chipset_readw_t	dwlpx_readw;
static alpha_chipset_readl_t	dwlpx_readl;
static alpha_chipset_writeb_t	dwlpx_writeb;
static alpha_chipset_writew_t	dwlpx_writew;
static alpha_chipset_writel_t	dwlpx_writel;
static alpha_chipset_maxdevs_t	dwlpx_maxdevs;
static alpha_chipset_cfgreadb_t	dwlpx_cfgreadb;
static alpha_chipset_cfgreadw_t	dwlpx_cfgreadw;
static alpha_chipset_cfgreadl_t	dwlpx_cfgreadl;
static alpha_chipset_cfgwriteb_t dwlpx_cfgwriteb;
static alpha_chipset_cfgwritew_t dwlpx_cfgwritew;
static alpha_chipset_cfgwritel_t dwlpx_cfgwritel;

static alpha_chipset_t dwlpx_chipset = {
	dwlpx_inb,
	dwlpx_inw,
	dwlpx_inl,
	dwlpx_outb,
	dwlpx_outw,
	dwlpx_outl,
	dwlpx_readb,
	dwlpx_readw,
	dwlpx_readl,
	dwlpx_writeb,
	dwlpx_writew,
	dwlpx_writel,
	dwlpx_maxdevs,
	dwlpx_cfgreadb,
	dwlpx_cfgreadw,
	dwlpx_cfgreadl,
	dwlpx_cfgwriteb,
	dwlpx_cfgwritew,
	dwlpx_cfgwritel,
};

/*
 * For supporting multiple busses, we will encode the dwlpx unit number into
 * the port address as Linux does.
 */

static u_int8_t
dwlpx_inb(u_int32_t port)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	return SPARSE_READ_BYTE(sc->io_base, port);
}

static u_int16_t
dwlpx_inw(u_int32_t port)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	return SPARSE_READ_WORD(sc->io_base, port);
}

static u_int32_t
dwlpx_inl(u_int32_t port)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	return SPARSE_READ_LONG(sc->io_base, port);
}

static void
dwlpx_outb(u_int32_t port, u_int8_t data)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);

	SPARSE_WRITE_BYTE(sc->io_base, port, data);
}

static void
dwlpx_outw(u_int32_t port, u_int16_t data)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	SPARSE_WRITE_WORD(sc->io_base, port, data);
}

static void
dwlpx_outl(u_int32_t port, u_int32_t data)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	SPARSE_WRITE_LONG(sc->io_base, port, data);
}

static u_int8_t
dwlpx_readb(u_int32_t pa)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	return SPARSE_READ_BYTE(sc->smem_base, pa);
}

static u_int16_t
dwlpx_readw(u_int32_t pa)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	return SPARSE_READ_WORD(sc->smem_base, pa);
}

static u_int32_t
dwlpx_readl(u_int32_t pa)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	return SPARSE_READ_LONG(sc->smem_base, pa);
}

static void
dwlpx_writeb(u_int32_t pa, u_int8_t data)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);

	SPARSE_WRITE_BYTE(sc->smem_base, pa, data);
}

static void
dwlpx_writew(u_int32_t pa, u_int16_t data)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	SPARSE_WRITE_WORD(sc->smem_base, pa, data);
}

static void
dwlpx_writel(u_int32_t pa, u_int32_t data)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);
	SPARSE_WRITE_LONG(sc->smem_base, pa, data);
}

static int
dwlpx_maxdevs(u_int b)
{
	return 12;		/* XXX */
}

/* XXX only support bus 0 */

#define DWLPX_CFGOFF(b, s, f, r) \
	(((b) << 16) | ((s) << 11) | ((f) << 8) | (r))

#define CFGREAD(b, s, f, r, width)					\
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);			\
	vm_offset_t off = DWLPX_CFGOFF(b, s, f, r);			\
	vm_offset_t kv = SPARSE_##width##_ADDRESS(sc->cfg_base, off);	\
	if (badaddr((caddr_t)kv, 4)) return ~0;				\
	return SPARSE_##width##_EXTRACT(off, SPARSE_READ(kv))

#define CFGWRITE(b, s, f, r, data, width)				\
	struct dwlpx_softc* sc = DWLPX_SOFTC(dwlpx0);			\
	vm_offset_t off = DWLPX_CFGOFF(b, s, f, r);			\
	vm_offset_t kv = SPARSE_##width##_ADDRESS(sc->cfg_base, off);	\
	if (badaddr((caddr_t)kv, 4)) return;				\
	SPARSE_WRITE(kv, SPARSE_##width##_INSERT(off, data))

static u_int8_t
dwlpx_cfgreadb(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(b, s, f, r, BYTE);
}

static u_int16_t
dwlpx_cfgreadw(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(b, s, f, r, WORD);
}

static u_int32_t
dwlpx_cfgreadl(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	CFGREAD(b, s, f, r, LONG);
}

static void
dwlpx_cfgwriteb(u_int h, u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	CFGWRITE(b, s, f, r, data, BYTE);
}

static void
dwlpx_cfgwritew(u_int h, u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	CFGWRITE(b, s, f, r, data, WORD);
}

static void
dwlpx_cfgwritel(u_int h, u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	CFGWRITE(b, s, f, r, data, LONG);
}

static int dwlpx_probe(device_t dev);
static int dwlpx_attach(device_t dev);
static driver_intr_t	dwlpx_intr;

static device_method_t dwlpx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dwlpx_probe),
	DEVMETHOD(device_attach,	dwlpx_attach),

	{ 0, 0 }
};

static driver_t dwlpx_driver = {
	"dwlpx",
	dwlpx_methods,
	sizeof(struct dwlpx_softc),
};

static int
dwlpx_probe(device_t dev)
{
	if (dwlpx0)
		return ENXIO;
	dwlpx0 = dev;
	device_set_desc(dev, "DWLPA or DWLPB PCI adapter");
	return 0;
}

static int
dwlpx_attach(device_t dev)
{
	struct dwlpx_softc* sc = DWLPX_SOFTC(dev);
	device_t parent = device_get_parent(dev);
	vm_offset_t regs;
	void *intr;

	dwlpx0 = dev;

	chipset = dwlpx_chipset;
	/* chipset.intrdev = dev; */

	regs = KV(DWLPX_BASE(kft_get_node(dev), kft_get_hosenum(dev)));
	sc->dmem_base	= regs + (0L << 32);
	sc->smem_base	= regs + (1L << 32);
	sc->io_base	= regs + (2L << 32);
	sc->cfg_base	= regs + (3L << 32);

	*(u_int32_t*) (regs + PCIA_CTL(0)) = 1;	/* Type1 config cycles */

	return BUS_SETUP_INTR(parent, dev, NULL, INTR_TYPE_MISC, dwlpx_intr, 0, &intr);

	return 0;
}

static void
dwlpx_intr(void* arg)
{
#ifdef SIMOS
	extern void simos_intr(int);
	simos_intr(0);
#endif
}

DRIVER_MODULE(dwlpx, kft, dwlpx_driver, dwlpx_devclass, 0, 0);

