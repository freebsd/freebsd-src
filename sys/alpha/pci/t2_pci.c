/*-
 * Copyright (c) 2000 Doug Rabson
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
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPELCAL, EXEMPLARY, OR CONSEQUENTIAL
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

#include <alpha/pci/t2reg.h>
#include <alpha/pci/t2var.h>

#include "alphapci_if.h"
#include "pcib_if.h"

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	pcib_devclass;

static int
t2_pcib_probe(device_t dev)
{
	device_t child;

	device_set_desc(dev, "T2 PCI host bus adapter");

	child = device_add_child(dev, "pci", 0);
	device_set_ivars(child, 0);

	return 0;
}

static int
t2_pcib_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	if (which == PCIB_IVAR_BUS) {
		*result = 0;
		return 0;
	}
	return ENOENT;
}

static void *
t2_pcib_cvt_dense(device_t dev, vm_offset_t addr)
{
	addr &= 0xffffffffUL;
	return (void *) KV(addr | T2_PCI_DENSE);
}

static int
t2_pcib_maxslots(device_t dev)
{
	return 31;
}

#define T2_CFGOFF(b, s, f, r)					\
	((b) ? (((b) << 16) | ((s) << 11) | ((f) << 8) | (r))	\
	 : ((1 << ((s) + 11)) | ((f) << 8) | (r)))

#define T2_TYPE1_SETUP(b,s,old_hae3) if((b)) {			\
        do {							\
		(s) = splhigh();				\
		(old_hae3) = REGVAL(T2_HAE0_3);			\
		alpha_mb();					\
		REGVAL(T2_HAE0_3) = (old_hae3) | (1<<30);	\
		alpha_mb();					\
        } while(0);						\
}

#define T2_TYPE1_TEARDOWN(b,s,old_hae3) if((b)) {	\
        do {						\
		alpha_mb();				\
		REGVAL(T2_HAE0_3) = (old_hae3);		\
		alpha_mb();				\
		splx((s));				\
        } while(0);					\
}

#define SWIZ_CFGREAD(b, s, f, r, width, type) do {			 \
	type val = ~0;							 \
	int ipl = 0;							 \
	u_int32_t old_hae3 = 0;						 \
	vm_offset_t off = T2_CFGOFF(b, s, f, r);			 \
	vm_offset_t kv = SPARSE_##width##_ADDRESS(KV(T2_PCI_CONF), off); \
	alpha_mb();							 \
	T2_TYPE1_SETUP(b,ipl,old_hae3);					 \
	if (!badaddr((caddr_t)kv, sizeof(type))) {			 \
		val = SPARSE_##width##_EXTRACT(off, SPARSE_READ(kv));	 \
	}								 \
        T2_TYPE1_TEARDOWN(b,ipl,old_hae3);				 \
	return val;							 \
} while (0)

#define SWIZ_CFGWRITE(b, s, f, r, data, width, type) do {		 \
	int ipl = 0;							 \
	u_int32_t old_hae3 = 0;						 \
	vm_offset_t off = T2_CFGOFF(b, s, f, r);			 \
	vm_offset_t kv = SPARSE_##width##_ADDRESS(KV(T2_PCI_CONF), off); \
	alpha_mb();							 \
	T2_TYPE1_SETUP(b,ipl,old_hae3);					 \
	if (!badaddr((caddr_t)kv, sizeof(type))) {			 \
                SPARSE_WRITE(kv, SPARSE_##width##_INSERT(off, data));	 \
		alpha_wmb();						 \
	}								 \
        T2_TYPE1_TEARDOWN(b,ipl,old_hae3);				 \
	return;								 \
} while (0)

static u_int32_t
t2_pcib_read_config(device_t dev, u_int b, u_int s, u_int f,
		    u_int reg, int width)
{
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
t2_pcib_write_config(device_t dev, u_int b, u_int s, u_int f,
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

static device_method_t t2_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		t2_pcib_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	t2_pcib_read_ivar),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* alphapci interface */
	DEVMETHOD(alphapci_cvt_dense,	t2_pcib_cvt_dense),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	t2_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	t2_pcib_read_config),
	DEVMETHOD(pcib_write_config,	t2_pcib_write_config),

	{ 0, 0 }
};

static driver_t t2_pcib_driver = {
	"pcib",
	t2_pcib_methods,
	1,
};

DRIVER_MODULE(pcib, t2, t2_pcib_driver, pcib_devclass, 0, 0);
