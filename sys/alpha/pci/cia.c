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
 *	$Id$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>
#include <machine/bwx.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	cia_devclass;
static device_t		cia0;		/* XXX only one for now */

static void cia_intr(void* frame, u_long vector);

struct cia_softc {
	vm_offset_t	dmem_base;	/* dense memory */
	vm_offset_t	smem_base;	/* sparse memory */
	vm_offset_t	io_base;	/* dense i/o */
	vm_offset_t	cfg0_base;	/* dense pci0 config */
	vm_offset_t	cfg1_base;	/* dense pci1 config */
};

#define CIA_SOFTC(dev)	(struct cia_softc*) device_get_softc(dev)

static alpha_chipset_inb_t	cia_bwx_inb;
static alpha_chipset_inw_t	cia_bwx_inw;
static alpha_chipset_inl_t	cia_bwx_inl;
static alpha_chipset_outb_t	cia_bwx_outb;
static alpha_chipset_outw_t	cia_bwx_outw;
static alpha_chipset_outl_t	cia_bwx_outl;
static alpha_chipset_maxdevs_t	cia_bwx_maxdevs;
static alpha_chipset_cfgreadb_t	cia_bwx_cfgreadb;
static alpha_chipset_cfgreadw_t	cia_bwx_cfgreadw;
static alpha_chipset_cfgreadl_t	cia_bwx_cfgreadl;
static alpha_chipset_cfgwriteb_t cia_bwx_cfgwriteb;
static alpha_chipset_cfgwritew_t cia_bwx_cfgwritew;
static alpha_chipset_cfgwritel_t cia_bwx_cfgwritel;

static alpha_chipset_t cia_bwx_chipset = {
	cia_bwx_inb,
	cia_bwx_inw,
	cia_bwx_inl,
	cia_bwx_outb,
	cia_bwx_outw,
	cia_bwx_outl,
	cia_bwx_maxdevs,
	cia_bwx_cfgreadb,
	cia_bwx_cfgreadw,
	cia_bwx_cfgreadl,
	cia_bwx_cfgwriteb,
	cia_bwx_cfgwritew,
	cia_bwx_cfgwritel,
};

static u_int8_t
cia_bwx_inb(u_int32_t port)
{
	return ldbu(KV(CIA_EV56_BWIO + port));
}

static u_int16_t
cia_bwx_inw(u_int32_t port)
{
	return ldwu(KV(CIA_EV56_BWIO + port));
}

static u_int32_t
cia_bwx_inl(u_int32_t port)
{
	return ldl(KV(CIA_EV56_BWIO + port));
}

static void
cia_bwx_outb(u_int32_t port, u_int8_t data)
{
	stb(KV(CIA_EV56_BWIO + port), data);
}

static void
cia_bwx_outw(u_int32_t port, u_int16_t data)
{
	stw(KV(CIA_EV56_BWIO + port), data);
}

static void
cia_bwx_outl(u_int32_t port, u_int32_t data)
{
	stl(KV(CIA_EV56_BWIO + port), data);
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
	if (badaddr((caddr_t)va, 1)) return ~0;
	return ldbu(va);
}

static u_int16_t
cia_bwx_cfgreadw(u_int b, u_int s, u_int f, u_int r)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 2)) return ~0;
	return ldwu(va);
}

static u_int32_t
cia_bwx_cfgreadl(u_int b, u_int s, u_int f, u_int r)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 4)) return ~0;
	return ldl(va);
}

static void
cia_bwx_cfgwriteb(u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 1)) return;
	return stb(va, data);
}

static void
cia_bwx_cfgwritew(u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 2)) return;
	return stw(va, data);
}

static void
cia_bwx_cfgwritel(u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 4)) return;
	return stl(va, data);
}

static int cia_probe(device_t dev);
static int cia_attach(device_t dev);

static device_method_t cia_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cia_probe),
	DEVMETHOD(device_attach,	cia_attach),

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
		panic("cia_init: only supporting bwx for now");

	chipset = cia_bwx_chipset;
}

static int
cia_probe(device_t dev)
{
	if (cia0)
		return ENXIO;
	cia0 = dev;
	device_set_desc(dev, "21174 PCI adapter"); /* XXX */
	return 0;
}

static int
cia_attach(device_t dev)
{
	struct cia_softc* sc = CIA_SOFTC(dev);

	cia_init();

	sc->dmem_base = CIA_EV56_BWMEM;
	sc->smem_base = CIA_PCI_SMEM1;
	sc->io_base = CIA_EV56_BWIO;
	sc->cfg0_base = CIA_EV56_BWCONF0;
	sc->cfg1_base = CIA_EV56_BWCONF1;

	set_iointr(cia_intr);
	return 0;
}

static void
cia_intr(void* frame, u_long vector)
{
}

DRIVER_MODULE(cia, root, cia_driver, cia_devclass, 0, 0);

