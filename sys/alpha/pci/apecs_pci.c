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
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPEAPECSL, EXEMPLARY, OR CONSEQUENTIAL
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
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <machine/cpuconf.h>
#include <machine/swiz.h>
#include <machine/md_var.h>
#include <machine/rpb.h>
#include <machine/resource.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>
#include <alpha/pci/pcibus.h>
#include <alpha/isa/isavar.h>

#include "alphapci_if.h"
#include "pcib_if.h"

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	pcib_devclass;

static int
apecs_pcib_probe(device_t dev)
{
	device_set_desc(dev, "2107x PCI host bus adapter");

	pci_init_resources();
	device_add_child(dev, "pci", 0);

	return 0;
}

static int
apecs_pcib_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	if (which == PCIB_IVAR_BUS) {
		*result = 0;
		return 0;
	}
	return ENOENT;
}

static struct resource *
apecs_pcib_alloc_resource(device_t bus, device_t child, int type, int *rid,
			  u_long start, u_long end, u_long count, u_int flags)
{
	if ((hwrpb->rpb_type == ST_DEC_2100_A50) &&
	    (type == SYS_RES_IRQ))
		return isa_alloc_intr(bus, child, start);
	else
		return alpha_pci_alloc_resource(bus, child, type, rid,
					  start, end, count, flags);
}

static int
apecs_pcib_release_resource(device_t bus, device_t child, int type, int rid,
			    struct resource *r)
{
	if ((hwrpb->rpb_type == ST_DEC_2100_A50) &&
	    (type == SYS_RES_IRQ))
		return isa_release_intr(bus, child, r);
	else
		return pci_release_resource(bus, child, type, rid, r);
}

static void *
apecs_pcib_cvt_dense(device_t dev, vm_offset_t addr)
{
	addr &= 0xffffffffUL;
	return (void *) KV(addr | APECS_PCI_DENSE);
}

static int
apecs_pcib_maxslots(device_t dev)
{
	return 31;
}

#define APECS_SWIZ_CFGOFF(b, s, f, r) \
	(((b) << 16) | ((s) << 11) | ((f) << 8) | (r))

#define APECS_TYPE1_SETUP(b,s,old_haxr2) if((b)) {	\
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

#define SWIZ_CFGREAD(b, s, f, r, width, type) do {			\
	type val = ~0;							\
	int ipl = 0;							\
	u_int32_t old_haxr2 = 0;					\
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);		\
	vm_offset_t kv =						\
		SPARSE_##width##_ADDRESS(KV(APECS_PCI_CONF), off);	\
	alpha_mb();							\
	APECS_TYPE1_SETUP(b,ipl,old_haxr2);				\
	if (!badaddr((caddr_t)kv, sizeof(type))) {			\
		val = SPARSE_##width##_EXTRACT(off, SPARSE_READ(kv));	\
	}								\
        APECS_TYPE1_TEARDOWN(b,ipl,old_haxr2);				\
	return val;							\
} while (0)

#define SWIZ_CFGWRITE(b, s, f, r, data, width, type) do {		\
	int ipl = 0;							\
	u_int32_t old_haxr2 = 0;					\
	vm_offset_t off = APECS_SWIZ_CFGOFF(b, s, f, r);		\
	vm_offset_t kv =						\
		SPARSE_##width##_ADDRESS(KV(APECS_PCI_CONF), off);	\
	alpha_mb();							\
	APECS_TYPE1_SETUP(b,ipl,old_haxr2);				\
	if (!badaddr((caddr_t)kv, sizeof(type))) {			\
                SPARSE_WRITE(kv, SPARSE_##width##_INSERT(off, data));	\
		alpha_wmb();						\
	}								\
        APECS_TYPE1_TEARDOWN(b,ipl,old_haxr2);				\
	return;								\
} while (0)

u_int32_t
apecs_pcib_read_config(device_t dev, u_int b, u_int s, u_int f,
		       u_int reg, int width)
{
	pcicfgregs cfg;

	if ((reg == PCIR_INTLINE) && (width == 1) && 
	     (platform.pci_intr_map != NULL)) {
		cfg.bus = b;
		cfg.slot = s;
		cfg.func = f;
		cfg.intline = 255;
		cfg.intpin =
		    apecs_pcib_read_config(dev, b, s, f, PCIR_INTPIN, 1);
		platform.pci_intr_map((void *)&cfg);
		if (cfg.intline != 255)
			return cfg.intline;
	}

	switch (width) {
	case 1:
		SWIZ_CFGREAD(b, s, f, reg, BYTE, u_int8_t);
		break;
	case 2:
		SWIZ_CFGREAD(b, s, f, reg, WORD, u_int16_t);
		break;
	case 4:
		SWIZ_CFGREAD(b, s, f, reg, LONG, u_int32_t);
	}
	return ~0;
}

static void
apecs_pcib_write_config(device_t dev, u_int b, u_int s, u_int f,
			u_int reg, u_int32_t val, int width)
{
	switch (width) {
	case 1:
		SWIZ_CFGWRITE(b, s, f, reg, val, BYTE, u_int8_t);
		break;
	case 2:
		SWIZ_CFGWRITE(b, s, f, reg, val, WORD, u_int16_t);
		break;
	case 4:
		SWIZ_CFGWRITE(b, s, f, reg, val, LONG, u_int32_t);
	}
}

static device_method_t apecs_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		apecs_pcib_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	apecs_pcib_read_ivar),
	DEVMETHOD(bus_alloc_resource,	apecs_pcib_alloc_resource),
	DEVMETHOD(bus_release_resource,	apecs_pcib_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* alphapci interface */
	DEVMETHOD(alphapci_cvt_dense,	apecs_pcib_cvt_dense),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	apecs_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	apecs_pcib_read_config),
	DEVMETHOD(pcib_write_config,	apecs_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	alpha_pci_route_interrupt),

	{ 0, 0 }
};

static driver_t apecs_pcib_driver = {
	"pcib",
	apecs_pcib_methods,
	1,
};

DRIVER_MODULE(pcib, apecs, apecs_pcib_driver, pcib_devclass, 0, 0);
