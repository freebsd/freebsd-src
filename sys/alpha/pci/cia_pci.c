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
 */
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <sys/rman.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/cpuconf.h>
#include <machine/bwx.h>
#include <machine/swiz.h>

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>
#include <alpha/pci/pcibus.h>
#include <alpha/isa/isavar.h>

#include "alphapci_if.h"
#include "pcib_if.h"

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	pcib_devclass;

static int
cia_pcib_probe(device_t dev)
{
	device_set_desc(dev, "2117x PCI host bus adapter");

	pci_init_resources();
	device_add_child(dev, "pci", 0);

	return 0;
}

static int
cia_pcib_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	switch (which) {
	case  PCIB_IVAR_BUS:
		*result = 0;
		return 0;
	}
	return ENOENT;
}

static void *
cia_pcib_cvt_dense(device_t dev, vm_offset_t addr)
{
	addr &= 0xffffffffUL;
	return (void *) KV(addr | CIA_PCI_DENSE);
}

static void *
cia_pcib_cvt_bwx(device_t dev, vm_offset_t addr)
{
	if (chipset_bwx) {
		addr &= 0xffffffffUL;
		return (void *) KV(addr | CIA_EV56_BWMEM);
	} else {
		return 0;
	}
}

static void
cia_clear_abort(void)
{
	/*
	 * Some (apparently-common) revisions of EB164 and AlphaStation
	 * firmware do the Wrong thing with PCI master and target aborts,
	 * which are caused by accesing the configuration space of devices
	 * that don't exist (for example).
	 *
	 * To work around this, we clear the CIA error register's PCI
	 * master and target abort bits before touching PCI configuration
	 * space and check it afterwards.  If it indicates a master or target
	 * abort, the device wasn't there so we return 0xffffffff.
	 */
	REGVAL(CIA_CSR_CIA_ERR) = CIA_ERR_RCVD_MAS_ABT|CIA_ERR_RCVD_TAR_ABT;
	alpha_mb();
	alpha_pal_draina();	
}

static int
cia_check_abort(void)
{
	u_int32_t errbits;
	int ba = 0;

	alpha_pal_draina();	
	alpha_mb();
	errbits = REGVAL(CIA_CSR_CIA_ERR);
	if (errbits & (CIA_ERR_RCVD_MAS_ABT|CIA_ERR_RCVD_TAR_ABT))
		ba = 1;

	if (errbits) {
		REGVAL(CIA_CSR_CIA_ERR) = errbits;
		alpha_mb();
		alpha_pal_draina();
	}

	return ba;
}

#define CIA_BWX_CFGADDR(b, s, f, r)				\
	KV(((b) ? CIA_EV56_BWCONF1 : CIA_EV56_BWCONF0)		\
	   | ((b) << 16) | ((s) << 11) | ((f) << 8) | (r))

#define BWX_CFGREAD(b, s, f, r, width, type, op) do {	\
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);	\
	type data;					\
	cia_clear_abort();				\
	if (badaddr((caddr_t)va, width)) {		\
		cia_check_abort();			\
		return ~0;				\
	}						\
	data = op(va);					\
	if (cia_check_abort())				\
		return ~0;				\
	return data;					\
} while (0)

#define BWX_CFGWRITE(b, s, f, r, data, width, type, op) do {	\
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);		\
	cia_clear_abort();					\
	if (badaddr((caddr_t)va, width)) return;		\
	op(va, data);						\
	cia_check_abort();					\
	return;							\
} while (0)

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

/*
 * From NetBSD:
 * Some (apparently-common) revisions of EB164 and AlphaStation
 * firmware do the Wrong thing with PCI master and target aborts,
 * which are caused by accesing the configuration space of devices
 * that don't exist (for example).
 *
 * To work around this, we clear the CIA error register's PCI
 * master and target abort bits before touching PCI configuration
 * space and check it afterwards.  If it indicates a master or target
 * abort, the device wasn't there so we return ~0
 */


#define SWIZ_CFGREAD(b, s, f, r, width, type) do {			     \
	type val = ~0;							     \
	int ipl = 0;							     \
	u_int32_t old_cfg = 0, errbits;					     \
	vm_offset_t off = CIA_SWIZ_CFGOFF(b, s, f, r);			     \
	vm_offset_t kv = SPARSE_##width##_ADDRESS(KV(CIA_PCI_CONF), off);    \
	REGVAL(CIA_CSR_CIA_ERR) = CIA_ERR_RCVD_MAS_ABT|CIA_ERR_RCVD_TAR_ABT; \
	alpha_mb();							     \
	CIA_TYPE1_SETUP(b,ipl,old_cfg);					     \
	if (!badaddr((caddr_t)kv, sizeof(type))) {			     \
		val = SPARSE_##width##_EXTRACT(off, SPARSE_READ(kv));	     \
	}								     \
        CIA_TYPE1_TEARDOWN(b,ipl,old_cfg);				     \
	errbits = REGVAL(CIA_CSR_CIA_ERR);				     \
	if (errbits & (CIA_ERR_RCVD_MAS_ABT|CIA_ERR_RCVD_TAR_ABT))	     \
		val = ~0;						     \
	if (errbits) {							     \
		REGVAL(CIA_CSR_CIA_ERR) = errbits;			     \
		alpha_mb();						     \
		alpha_pal_draina();					     \
	}								     \
        return val;							     \
} while (0)

#define SWIZ_CFGWRITE(b, s, f, r, data, width, type) do {		  \
	int ipl = 0;							  \
	u_int32_t old_cfg = 0;						  \
	vm_offset_t off = CIA_SWIZ_CFGOFF(b, s, f, r);			  \
	vm_offset_t kv = SPARSE_##width##_ADDRESS(KV(CIA_PCI_CONF), off); \
	alpha_mb();							  \
	CIA_TYPE1_SETUP(b,ipl,old_cfg);					  \
	if (!badaddr((caddr_t)kv, sizeof(type))) {			  \
                SPARSE_WRITE(kv, SPARSE_##width##_INSERT(off, data));	  \
		alpha_wmb();						  \
	}								  \
        CIA_TYPE1_TEARDOWN(b,ipl,old_cfg);				  \
	return;								  \
} while (0)

static u_int32_t
cia_pcib_swiz_read_config(u_int b, u_int s, u_int f, u_int reg, int width)
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
cia_pcib_swiz_write_config(u_int b, u_int s, u_int f, u_int reg,
			   u_int32_t val, int width)
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

static u_int32_t
cia_pcib_bwx_read_config(u_int b, u_int s, u_int f, u_int reg, int width)
{
	switch (width) {
	case 1:
		BWX_CFGREAD(b, s, f, reg, 1, u_int8_t, ldbu);
		break;
	case 2:
		BWX_CFGREAD(b, s, f, reg, 2, u_int16_t, ldwu);
		break;
	case 4:
		BWX_CFGREAD(b, s, f, reg, 4, u_int32_t, ldl);
	}
	return ~0;
}

static void
cia_pcib_bwx_write_config(u_int b, u_int s, u_int f, u_int reg,
			  u_int32_t val, u_int width)
{
	switch (width) {
	case 1:
		BWX_CFGWRITE(b, s, f, reg, val, 1, u_int8_t, stb);
		break;
	case 2:
		BWX_CFGWRITE(b, s, f, reg, val, 2, u_int16_t, stw);
		break;
	case 4:
		BWX_CFGWRITE(b, s, f, reg, val, 4, u_int32_t, stl);
	}
}

static int
cia_pcib_maxslots(device_t dev)
{
	return 31;
}

static u_int32_t
cia_pcib_read_config(device_t dev, int b, int s, int f,
		     int reg, int width)
{
	pcicfgregs cfg;

	if ((reg == PCIR_INTLINE) && (width == 1) && 
	     (platform.pci_intr_map != NULL)) {
		cfg.bus = b;
		cfg.slot = s;
		cfg.func = f;
		cfg.intline = 255;
		cfg.intpin =
		    cia_pcib_read_config(dev, b, s, f, PCIR_INTPIN, 1);
		platform.pci_intr_map((void *)&cfg);
		if (cfg.intline != 255)
			return cfg.intline;
	}

	if (chipset_bwx)
		return cia_pcib_bwx_read_config(b, s, f, reg, width);
	else
		return cia_pcib_swiz_read_config(b, s, f, reg, width);
}

static void
cia_pcib_write_config(device_t dev, int b, int s, int f,
		      int reg, u_int32_t val, int width)
{
	if (chipset_bwx)
		cia_pcib_bwx_write_config(b, s, f, reg, val, width);
	else
		cia_pcib_swiz_write_config(b, s, f, reg, val, width);
}

static device_method_t cia_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cia_pcib_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	cia_pcib_read_ivar),
	DEVMETHOD(bus_alloc_resource,	alpha_pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	pci_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	alpha_platform_pci_setup_intr),
	DEVMETHOD(bus_teardown_intr,	alpha_platform_pci_teardown_intr),

	/* alphapci interface */
	DEVMETHOD(alphapci_cvt_dense,	cia_pcib_cvt_dense),
	DEVMETHOD(alphapci_cvt_bwx,	cia_pcib_cvt_bwx),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	cia_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	cia_pcib_read_config),
	DEVMETHOD(pcib_write_config,	cia_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	alpha_pci_route_interrupt),

	{ 0, 0 }
};

static driver_t cia_pcib_driver = {
	"pcib",
	cia_pcib_methods,
	1,
};

DRIVER_MODULE(pcib, cia, cia_pcib_driver, pcib_devclass, 0, 0);
