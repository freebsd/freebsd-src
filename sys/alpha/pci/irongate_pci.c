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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/bwx.h>
#include <sys/rman.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <alpha/pci/irongatereg.h>
#include <alpha/pci/irongatevar.h>
#include <alpha/pci/pcibus.h>

#include "alphapci_if.h"
#include "pcib_if.h"

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	pcib_devclass;

static int
irongate_pcib_probe(device_t dev)
{

	device_set_desc(dev, "AMD 751 PCI host bus adapter");

	pci_init_resources();
	device_add_child(dev, "pci", 0);

	/*  
	 * XXX  -- The SRM console doesn't properly initialize
	 * the AcerLabs M1533C southbridge.  We must turn off 32-bit 
	 * DMA support.
	 */
	if ((0x153310b9 == PCIB_READ_CONFIG(dev, 0, 7, 0,
					    PCIR_DEVVENDOR, 4))) {
		u_int8_t value = PCIB_READ_CONFIG(dev, 0, 7, 0, 0x42, 1);
		value &= ~0x40;
		PCIB_WRITE_CONFIG(dev, 0, 7, 0, 0x42, 0, 1);
	}

	return 0;
}

static int
irongate_pcib_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	switch (which) {
	case  PCIB_IVAR_BUS:
		*result = 0;
		return 0;
	}
	return ENOENT;
}

static void *
irongate_pcib_cvt_dense(device_t dev, vm_offset_t addr)
{
	addr &= 0xffffffffUL;
	return (void *) KV(addr | IRONGATE_MEM);
}

static void *
irongate_pcib_cvt_bwx(device_t dev, vm_offset_t addr)
{
	addr &= 0xffffffffUL;
	return (void *) KV(addr | IRONGATE_MEM);
}

static int
irongate_pcib_maxslots(device_t dev)
{
	return 31;
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

#define CFGREAD(b, s, f, r, width, type, op) do {	\
        vm_offset_t va;					\
	type data;					\
	va = IRONGATE_CFGADDR(b, s, f, r);		\
	irongate_clear_abort();				\
	if (badaddr((caddr_t)va, width)) {		\
		irongate_check_abort();			\
		return ~0;				\
	}						\
	data = op(va);					\
	if (irongate_check_abort())			\
		return ~0;				\
	return data;					\
} while (0)

#define CFGWRITE(b, s, f, r, data, width, op) do {	\
        vm_offset_t va;					\
	va = IRONGATE_CFGADDR(b, s, f, r);		\
	irongate_clear_abort();				\
	if (badaddr((caddr_t)va, width))		\
		return;					\
	op(va, data);					\
	irongate_check_abort();				\
} while (0)

static u_int32_t
irongate_pcib_read_config(device_t dev, u_int b, u_int s, u_int f,
			  u_int reg, int width)
{
	switch (width) {
	case 1:
		CFGREAD(b, s, f, reg, 1, u_int8_t, ldbu);
		break;
	case 2:
		CFGREAD(b, s, f, reg, 2, u_int16_t, ldwu);
		break;
	case 4:
		CFGREAD(b, s, f, reg, 4, u_int32_t, ldl);
	}
	return ~0;
}

static void
irongate_pcib_write_config(device_t dev, u_int b, u_int s, u_int f,
			   u_int reg, u_int32_t val, int width)
{
	switch (width) {
	case 1:
		CFGWRITE(b, s, f, reg, val, 1, stb);
		break;
	case 2:
		CFGWRITE(b, s, f, reg, val, 2, stw);
		break;
	case 4:
		CFGWRITE(b, s, f, reg, val, 4, stl);
	}
}

static device_method_t irongate_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		irongate_pcib_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	irongate_pcib_read_ivar),
	DEVMETHOD(bus_alloc_resource,	alpha_pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	pci_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr, 	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* alphapci interface */
	DEVMETHOD(alphapci_cvt_dense,	irongate_pcib_cvt_dense),
	DEVMETHOD(alphapci_cvt_bwx,	irongate_pcib_cvt_bwx),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	irongate_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	irongate_pcib_read_config),
	DEVMETHOD(pcib_write_config,	irongate_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	alpha_pci_route_interrupt),

	{ 0, 0 }
};

static driver_t irongate_pcib_driver = {
	"pcib",
	irongate_pcib_methods,
	1,
};

DRIVER_MODULE(pcib, irongate, irongate_pcib_driver, pcib_devclass, 0, 0);


