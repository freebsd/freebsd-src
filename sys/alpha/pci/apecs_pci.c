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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <pci/pcivar.h>
#include <machine/swiz.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>

#include "alphapci_if.h"
#include "pcib_if.h"

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	pcib_devclass;

static int
apecs_pcib_probe(device_t dev)
{
	device_set_desc(dev, "2107x PCI host bus adapter");

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
apecs_pcib_read_config(device_t dev, int b, int s, int f,
		       int reg, int width)
{
	switch (width) {
	case 1:
		SWIZ_CFGREAD(b, s, f, reg, BYTE, u_int8_t);
	case 2:
		SWIZ_CFGREAD(b, s, f, reg, WORD, u_int16_t);
	case 4:
		SWIZ_CFGREAD(b, s, f, reg, LONG, u_int32_t);
	}
	return ~0;
}

static void
apecs_pcib_write_config(device_t dev, int b, int s, int f,
			int reg, u_int32_t val, int width)
{
	switch (width) {
	case 1:
		SWIZ_CFGWRITE(b, s, f, reg, val, BYTE, u_int8_t);
	case 2:
		SWIZ_CFGWRITE(b, s, f, reg, val, WORD, u_int16_t);
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
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* alphapci interface */
	DEVMETHOD(alphapci_cvt_dense,	apecs_pcib_cvt_dense),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	apecs_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	apecs_pcib_read_config),
	DEVMETHOD(pcib_write_config,	apecs_pcib_write_config),

	{ 0, 0 }
};

static driver_t apecs_pcib_driver = {
	"pcib",
	apecs_pcib_methods,
	1,
};

DRIVER_MODULE(pcib, apecs, apecs_pcib_driver, pcib_devclass, 0, 0);
