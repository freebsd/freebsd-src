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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <sys/rman.h>
#include <pci/pcivar.h>
#include <alpha/pci/tsunamireg.h>
#include <alpha/pci/tsunamivar.h>
#include <alpha/pci/pcibus.h>
#include <machine/resource.h>
#include <machine/bwx.h>

#include "alphapci_if.h"
#include "pcib_if.h"

struct tsunami_hose_softc {
	struct bwx_space io;	/* accessor for ports */
	struct bwx_space mem;	/* accessor for memory */
	struct rman	io_rman; /* resource manager for ports */
	struct rman	mem_rman; /* resource manager for memory */
};

static devclass_t	pcib_devclass;


static int
tsunami_pcib_probe(device_t dev)
{
	struct tsunami_hose_softc *sc = device_get_softc(dev);
	device_t child;

	device_set_desc(dev, "21271 PCI host bus adapter");

	pci_init_resources();	/* XXX probably don't need */
	child = device_add_child(dev, "pci", -1);

	bwx_init_space(&sc->io, KV(TSUNAMI_IO(device_get_unit(dev))));
	bwx_init_space(&sc->mem, KV(TSUNAMI_MEM(device_get_unit(dev))));

	sc->io_rman.rm_start = 0;
	sc->io_rman.rm_end = ~0u;
	sc->io_rman.rm_type = RMAN_ARRAY;
	sc->io_rman.rm_descr = "I/O ports";
	if (rman_init(&sc->io_rman)
	    || rman_manage_region(&sc->io_rman, 0x0, (1L << 32)))
		panic("tsunami_pcib_probe: io_rman");

	sc->mem_rman.rm_start = 0;
	sc->mem_rman.rm_end = ~0u;
	sc->mem_rman.rm_type = RMAN_ARRAY;
	sc->mem_rman.rm_descr = "I/O memory";
	if (rman_init(&sc->mem_rman)
	    || rman_manage_region(&sc->mem_rman, 0x0, (1L << 32)))
		panic("tsunami_pcib_probe: mem_rman");

	/*
	 * Replace the temporary bootstrap spaces with real onys. This
	 * isn't stictly necessary but it keeps things tidy.
	 */
	if (device_get_unit(dev) == 0) {
		busspace_isa_io = (struct alpha_busspace *) &sc->io;
		busspace_isa_mem = (struct alpha_busspace *) &sc->mem;
	}

	return 0;
}

static int
tsunami_pcib_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	if (which == PCIB_IVAR_BUS) {
		*result = 0;
	}
	return ENOENT;
}

static void *
tsunami_pcib_cvt_dense(device_t dev, vm_offset_t addr)
{
	int h = device_get_unit(dev);
	addr &= 0xffffffffUL;
	return (void *) KV(addr | TSUNAMI_MEM(h));
}

static void *
tsunami_pcib_cvt_bwx(device_t dev, vm_offset_t addr)
{
	int h = device_get_unit(dev);
	addr &= 0xffffffffUL;
	return (void *) KV(addr | TSUNAMI_MEM(h));
}

static kobj_t
tsunami_pcib_get_bustag(device_t dev, int type)
{
	struct tsunami_hose_softc *sc = device_get_softc(dev);

	switch (type) {
	case SYS_RES_IOPORT:
		return (kobj_t) &sc->io;

	case SYS_RES_MEMORY:
		return (kobj_t) &sc->mem;
	}

	return 0;
}

static struct rman *
tsunami_pcib_get_rman(device_t dev, int type)
{
	struct tsunami_hose_softc *sc = device_get_softc(dev);

	switch (type) {
	case SYS_RES_IOPORT:
		return &sc->io_rman;

	case SYS_RES_MEMORY:
		return &sc->mem_rman;
	}

	return 0;
}

static int
tsunami_pcib_maxslots(device_t dev)
{
	return 31;
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

#define CFGREAD(h, b, s, f, r, op, width, type) do {	\
        vm_offset_t va;					\
	type data;					\
	va = TSUNAMI_CFGADDR(b, s, f, r, h);		\
	tsunami_clear_abort();				\
	if (badaddr((caddr_t)va, width)) {		\
		tsunami_check_abort();			\
		return ~0;				\
	}						\
	data = ##op##(va);				\
	if (tsunami_check_abort())			\
		return ~0;				\
	return data;					\
} while (0)

#define CFGWRITE(h, b, s, f, r, data, op, width) do {	\
        vm_offset_t va;					\
	va = TSUNAMI_CFGADDR(b, s, f, r, h);		\
	tsunami_clear_abort();				\
	if (badaddr((caddr_t)va, width))		\
		return;					\
	##op##(va, data);				\
	tsunami_check_abort();				\
} while (0)

static u_int32_t
tsunami_pcib_read_config(device_t dev, u_int b, u_int s, u_int f,
			 u_int reg, int width)
{
	int h = device_get_unit(dev);
	switch (width) {
	case 1:
		CFGREAD(h, b, s, f, reg, ldbu, 1, u_int8_t);
		break;
	case 2:
		CFGREAD(h, b, s, f, reg, ldwu, 2, u_int16_t);
		break;
	case 4:
		CFGREAD(h, b, s, f, reg, ldl, 4, u_int32_t);
	}
	return ~0;
}

static void
tsunami_pcib_write_config(device_t dev, u_int b, u_int s, u_int f,
			  u_int reg, u_int32_t val, int width)
{
	int h = device_get_unit(dev);
	switch (width) {
	case 1:
		CFGWRITE(h, b, s, f, reg, val, stb, 1);
		break;
	case 2:
		CFGWRITE(h, b, s, f, reg, val, stw, 2);
		break;
	case 4:
		CFGWRITE(h, b, s, f, reg, val, stl, 4);
	}
}

static device_method_t tsunami_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tsunami_pcib_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	tsunami_pcib_read_ivar),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	pci_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	alpha_platform_pci_setup_intr),
	DEVMETHOD(bus_teardown_intr,	alpha_platform_pci_teardown_intr),

	/* alphapci interface */
	DEVMETHOD(alphapci_cvt_dense,	tsunami_pcib_cvt_dense),
	DEVMETHOD(alphapci_cvt_bwx,	tsunami_pcib_cvt_bwx),
	DEVMETHOD(alphapci_get_bustag,	tsunami_pcib_get_bustag),
	DEVMETHOD(alphapci_get_rman,	tsunami_pcib_get_rman),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	tsunami_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	tsunami_pcib_read_config),
	DEVMETHOD(pcib_write_config,	tsunami_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	alpha_pci_route_interrupt),

	{ 0, 0 }
};


static driver_t tsunami_pcib_driver = {
	"pcib",
	tsunami_pcib_methods,
	sizeof(struct tsunami_hose_softc),
};


DRIVER_MODULE(pcib, tsunami, tsunami_pcib_driver, pcib_devclass, 0, 0);


