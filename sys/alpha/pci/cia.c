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
 * $FreeBSD$
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

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>
#include <alpha/pci/pcibus.h>
#include <alpha/isa/isavar.h>
#include <machine/bwx.h>
#include <machine/swiz.h>
#include <machine/intr.h>
#include <machine/intrcnt.h>
#include <machine/cpuconf.h>
#include <machine/rpb.h>
#include <machine/resource.h>
#include <machine/sgmap.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	cia_devclass;
static device_t		cia0;		/* XXX only one for now */
static u_int32_t	cia_hae_mem;
static int		cia_rev, cia_ispyxis, cia_config;

struct cia_softc {
	int		junk;		/* no softc */
};

#define CIA_SOFTC(dev)	(struct cia_softc*) device_get_softc(dev)

static alpha_chipset_inb_t	cia_bwx_inb, cia_swiz_inb;
static alpha_chipset_inw_t	cia_bwx_inw, cia_swiz_inw;
static alpha_chipset_inl_t	cia_bwx_inl, cia_swiz_inl;
static alpha_chipset_outb_t	cia_bwx_outb, cia_swiz_outb;
static alpha_chipset_outw_t	cia_bwx_outw, cia_swiz_outw;
static alpha_chipset_outl_t	cia_bwx_outl, cia_swiz_outl;
static alpha_chipset_readb_t	cia_bwx_readb, cia_swiz_readb;
static alpha_chipset_readw_t	cia_bwx_readw, cia_swiz_readw;
static alpha_chipset_readl_t	cia_bwx_readl, cia_swiz_readl;
static alpha_chipset_writeb_t	cia_bwx_writeb, cia_swiz_writeb;
static alpha_chipset_writew_t	cia_bwx_writew, cia_swiz_writew;
static alpha_chipset_writel_t	cia_bwx_writel, cia_swiz_writel;
static alpha_chipset_maxdevs_t	cia_bwx_maxdevs, cia_swiz_maxdevs;
static alpha_chipset_cfgreadb_t	cia_bwx_cfgreadb, cia_swiz_cfgreadb;
static alpha_chipset_cfgreadw_t	cia_bwx_cfgreadw, cia_swiz_cfgreadw;
static alpha_chipset_cfgreadl_t	cia_bwx_cfgreadl, cia_swiz_cfgreadl;
static alpha_chipset_cfgwriteb_t cia_bwx_cfgwriteb, cia_swiz_cfgwriteb;
static alpha_chipset_cfgwritew_t cia_bwx_cfgwritew, cia_swiz_cfgwritew;
static alpha_chipset_cfgwritel_t cia_bwx_cfgwritel, cia_swiz_cfgwritel;
static alpha_chipset_addrcvt_t   cia_cvt_dense,  cia_cvt_bwx;
static alpha_chipset_read_hae_t	cia_read_hae;
static alpha_chipset_write_hae_t cia_write_hae;

static alpha_chipset_t cia_bwx_chipset = {
	cia_bwx_inb,
	cia_bwx_inw,
	cia_bwx_inl,
	cia_bwx_outb,
	cia_bwx_outw,
	cia_bwx_outl,
	cia_bwx_readb,
	cia_bwx_readw,
	cia_bwx_readl,
	cia_bwx_writeb,
	cia_bwx_writew,
	cia_bwx_writel,
	cia_bwx_maxdevs,
	cia_bwx_cfgreadb,
	cia_bwx_cfgreadw,
	cia_bwx_cfgreadl,
	cia_bwx_cfgwriteb,
	cia_bwx_cfgwritew,
	cia_bwx_cfgwritel,
	cia_cvt_dense,
	cia_cvt_bwx,
	cia_read_hae,
	cia_write_hae,
};
static alpha_chipset_t cia_swiz_chipset = {
	cia_swiz_inb,
	cia_swiz_inw,
	cia_swiz_inl,
	cia_swiz_outb,
	cia_swiz_outw,
	cia_swiz_outl,
	cia_swiz_readb,
	cia_swiz_readw,
	cia_swiz_readl,
	cia_swiz_writeb,
	cia_swiz_writew,
	cia_swiz_writel,
	cia_swiz_maxdevs,
	cia_swiz_cfgreadb,
	cia_swiz_cfgreadw,
	cia_swiz_cfgreadl,
	cia_swiz_cfgwriteb,
	cia_swiz_cfgwritew,
	cia_swiz_cfgwritel,
	cia_cvt_dense,
	NULL,
	cia_read_hae,
	cia_write_hae,
};

static u_int8_t
cia_bwx_inb(u_int32_t port)
{
	alpha_mb();
	return ldbu(KV(CIA_EV56_BWIO+BWX_EV56_INT1 + port));
}

static u_int16_t
cia_bwx_inw(u_int32_t port)
{
	alpha_mb();
	return ldwu(KV(CIA_EV56_BWIO+BWX_EV56_INT2 + port));
}

static u_int32_t
cia_bwx_inl(u_int32_t port)
{
	alpha_mb();
	return ldl(KV(CIA_EV56_BWIO+BWX_EV56_INT4 + port));
}

static void
cia_bwx_outb(u_int32_t port, u_int8_t data)
{
	stb(KV(CIA_EV56_BWIO+BWX_EV56_INT1 + port), data);
	alpha_wmb();
}

static void
cia_bwx_outw(u_int32_t port, u_int16_t data)
{
	stw(KV(CIA_EV56_BWIO+BWX_EV56_INT2 + port), data);
	alpha_wmb();
}

static void
cia_bwx_outl(u_int32_t port, u_int32_t data)
{
	stl(KV(CIA_EV56_BWIO+BWX_EV56_INT4 + port), data);
	alpha_wmb();
}

static u_int8_t
cia_bwx_readb(u_int32_t pa)
{
	alpha_mb();
	return ldbu(KV(CIA_EV56_BWMEM+BWX_EV56_INT1 + pa));
}

static u_int16_t
cia_bwx_readw(u_int32_t pa)
{
	alpha_mb();
	return ldwu(KV(CIA_EV56_BWMEM+BWX_EV56_INT2 + pa));
}

static u_int32_t
cia_bwx_readl(u_int32_t pa)
{
	alpha_mb();
	return ldl(KV(CIA_EV56_BWMEM+BWX_EV56_INT4 + pa));
}

static void
cia_bwx_writeb(u_int32_t pa, u_int8_t data)
{
	stb(KV(CIA_EV56_BWMEM+BWX_EV56_INT1 + pa), data);
	alpha_wmb();
}

static void
cia_bwx_writew(u_int32_t pa, u_int16_t data)
{
	stw(KV(CIA_EV56_BWMEM+BWX_EV56_INT2 + pa), data);
	alpha_wmb();
}

static void
cia_bwx_writel(u_int32_t pa, u_int32_t data)
{
	stl(KV(CIA_EV56_BWMEM+BWX_EV56_INT4 + pa), data);
	alpha_wmb();
}

static int
cia_bwx_maxdevs(u_int b)
{
	return 12;		/* XXX */
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

static u_int8_t
cia_bwx_cfgreadb(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	u_int8_t data;
	cia_clear_abort();
	if (badaddr((caddr_t)va, 1)) {
		cia_check_abort();
		return ~0;
	}
	data = ldbu(va+BWX_EV56_INT1);
	if (cia_check_abort())
		return ~0;
	return data;
}

static u_int16_t
cia_bwx_cfgreadw(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	u_int16_t data;
	cia_clear_abort();
	if (badaddr((caddr_t)va, 2)) {
		cia_check_abort();
		return ~0;
	}
	data = ldwu(va+BWX_EV56_INT2);
	if (cia_check_abort())
		return ~0;
	return data;
}

static u_int32_t
cia_bwx_cfgreadl(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	u_int32_t data;
	cia_clear_abort();
	if (badaddr((caddr_t)va, 4)) {
		cia_check_abort();
		return ~0;
	}
	data = ldl(va+BWX_EV56_INT4);
	if (cia_check_abort())
		return ~0;
	return data;
}

static void
cia_bwx_cfgwriteb(u_int h, u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	cia_clear_abort();
	if (badaddr((caddr_t)va, 1)) return;
	stb(va+BWX_EV56_INT1, data);
	cia_check_abort();
}

static void
cia_bwx_cfgwritew(u_int h, u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 2)) return;
	stw(va+BWX_EV56_INT2, data);
	cia_check_abort();
}

static void
cia_bwx_cfgwritel(u_int h, u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	vm_offset_t va = CIA_BWX_CFGADDR(b, s, f, r);
	if (badaddr((caddr_t)va, 4)) return;
	stl(va+BWX_EV56_INT4, data);
	cia_check_abort();
}

static u_int8_t
cia_swiz_inb(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_BYTE(KV(CIA_PCI_SIO1), port);
}

static u_int16_t
cia_swiz_inw(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_WORD(KV(CIA_PCI_SIO1), port);
}

static u_int32_t
cia_swiz_inl(u_int32_t port)
{
	alpha_mb();
	return SPARSE_READ_LONG(KV(CIA_PCI_SIO1), port);
}

static void
cia_swiz_outb(u_int32_t port, u_int8_t data)
{
	SPARSE_WRITE_BYTE(KV(CIA_PCI_SIO1), port, data);
	alpha_wmb();
}

static void
cia_swiz_outw(u_int32_t port, u_int16_t data)
{
	SPARSE_WRITE_WORD(KV(CIA_PCI_SIO1), port, data);
	alpha_wmb();
}

static void
cia_swiz_outl(u_int32_t port, u_int32_t data)
{
	SPARSE_WRITE_LONG(KV(CIA_PCI_SIO1), port, data);
	alpha_wmb();
}

static __inline void
cia_swiz_set_hae_mem(u_int32_t *pa)
{
	/* Only bother with region 1 */
#define REG1 (7 << 29)
	if ((cia_hae_mem & REG1) != (*pa & REG1)) {
		/*
		 * Seems fairly paranoid but this is what Linux does...
		 */
		u_int32_t msb = *pa & REG1;
		int s = splhigh();
		cia_hae_mem = (cia_hae_mem & ~REG1) | msb;
		REGVAL(CIA_CSR_HAE_MEM) = cia_hae_mem;
		alpha_mb();
		cia_hae_mem = REGVAL(CIA_CSR_HAE_MEM);
		splx(s);
		*pa -= msb;
	}
}

static u_int8_t
cia_swiz_readb(u_int32_t pa)
{
	alpha_mb();
	cia_swiz_set_hae_mem(&pa);
	return SPARSE_READ_BYTE(KV(CIA_PCI_SMEM1), pa);
}

static u_int16_t
cia_swiz_readw(u_int32_t pa)
{
	alpha_mb();
	cia_swiz_set_hae_mem(&pa);
	return SPARSE_READ_WORD(KV(CIA_PCI_SMEM1), pa);
}

static u_int32_t
cia_swiz_readl(u_int32_t pa)
{
	alpha_mb();
	cia_swiz_set_hae_mem(&pa);
	return SPARSE_READ_LONG(KV(CIA_PCI_SMEM1), pa);
}

static void
cia_swiz_writeb(u_int32_t pa, u_int8_t data)
{
	cia_swiz_set_hae_mem(&pa);
	SPARSE_WRITE_BYTE(KV(CIA_PCI_SMEM1), pa, data);
	alpha_wmb();
}

static void
cia_swiz_writew(u_int32_t pa, u_int16_t data)
{
	cia_swiz_set_hae_mem(&pa);
	SPARSE_WRITE_WORD(KV(CIA_PCI_SMEM1), pa, data);
	alpha_wmb();
}

static void
cia_swiz_writel(u_int32_t pa, u_int32_t data)
{
	cia_swiz_set_hae_mem(&pa);
	SPARSE_WRITE_LONG(KV(CIA_PCI_SMEM1), pa, data);
	alpha_wmb();
}

static int
cia_swiz_maxdevs(u_int b)
{
	return 12;		/* XXX */
}

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


#define SWIZ_CFGREAD(b, s, f, r, width, type)				\
	type val = ~0;							\
	int ipl = 0;							\
	u_int32_t old_cfg = 0, errbits;					\
	vm_offset_t off = CIA_SWIZ_CFGOFF(b, s, f, r);			\
	vm_offset_t kv = SPARSE_##width##_ADDRESS(KV(CIA_PCI_CONF), off); \
	REGVAL(CIA_CSR_CIA_ERR) = CIA_ERR_RCVD_MAS_ABT|CIA_ERR_RCVD_TAR_ABT;\
	alpha_mb();							\
	CIA_TYPE1_SETUP(b,ipl,old_cfg);					\
	if (!badaddr((caddr_t)kv, sizeof(type))) {			\
		val = SPARSE_##width##_EXTRACT(off, SPARSE_READ(kv));	\
	}								\
        CIA_TYPE1_TEARDOWN(b,ipl,old_cfg);				\
	errbits = REGVAL(CIA_CSR_CIA_ERR);				\
	if (errbits & (CIA_ERR_RCVD_MAS_ABT|CIA_ERR_RCVD_TAR_ABT))	\
		val = ~0;						\
	if (errbits) {							\
		REGVAL(CIA_CSR_CIA_ERR) = errbits;			\
		alpha_mb();						\
		alpha_pal_draina();					\
	}								\
        return val;

#define SWIZ_CFGWRITE(b, s, f, r, data, width, type)			\
	int ipl = 0;							\
	u_int32_t old_cfg = 0;						\
	vm_offset_t off = CIA_SWIZ_CFGOFF(b, s, f, r);			\
	vm_offset_t kv = SPARSE_##width##_ADDRESS(KV(CIA_PCI_CONF), off); \
	alpha_mb();							\
	CIA_TYPE1_SETUP(b,ipl,old_cfg);					\
	if (!badaddr((caddr_t)kv, sizeof(type))) {			\
                SPARSE_WRITE(kv, SPARSE_##width##_INSERT(off, data));	\
		alpha_wmb();						\
	}								\
        CIA_TYPE1_TEARDOWN(b,ipl,old_cfg);				\
	return;							

static u_int8_t
cia_swiz_cfgreadb(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	SWIZ_CFGREAD(b, s, f, r, BYTE, u_int8_t);
}

static u_int16_t
cia_swiz_cfgreadw(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	SWIZ_CFGREAD(b, s, f, r, WORD, u_int16_t);
}

static u_int32_t
cia_swiz_cfgreadl(u_int h, u_int b, u_int s, u_int f, u_int r)
{
	SWIZ_CFGREAD(b, s, f, r, LONG, u_int32_t);
}

static void
cia_swiz_cfgwriteb(u_int h, u_int b, u_int s, u_int f, u_int r, u_int8_t data)
{
	SWIZ_CFGWRITE(b, s, f, r, data, BYTE, u_int8_t);
}

static void
cia_swiz_cfgwritew(u_int h, u_int b, u_int s, u_int f, u_int r, u_int16_t data)
{
	SWIZ_CFGWRITE(b, s, f, r, data, WORD, u_int16_t);
}

static void
cia_swiz_cfgwritel(u_int h, u_int b, u_int s, u_int f, u_int r, u_int32_t data)
{
	SWIZ_CFGWRITE(b, s, f, r, data, LONG, u_int32_t);
}

vm_offset_t
cia_cvt_dense(vm_offset_t addr)
{
	addr &= 0xffffffffUL;
	return (addr | CIA_PCI_DENSE);
	
}

vm_offset_t
cia_cvt_bwx(vm_offset_t addr)
{
	addr &= 0xffffffffUL;
	return (addr |= CIA_EV56_BWMEM);
}

static u_int64_t
cia_read_hae(void)
{
	return cia_hae_mem & REG1;
}

static void
cia_write_hae(u_int64_t hae)
{
	u_int32_t pa = hae;
	cia_swiz_set_hae_mem(&pa);
}

static int cia_probe(device_t dev);
static int cia_attach(device_t dev);
static int cia_setup_intr(device_t dev, device_t child,
			  struct resource *irq, int flags,
			  driver_intr_t *intr, void *arg, void **cookiep);
static int cia_teardown_intr(device_t dev, device_t child,
			     struct resource *irq, void *cookie);

static device_method_t cia_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cia_probe),
	DEVMETHOD(device_attach,	cia_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	pci_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	cia_setup_intr),
	DEVMETHOD(bus_teardown_intr,	cia_teardown_intr),

	{ 0, 0 }
};

static driver_t cia_driver = {
	"cia",
	cia_methods,
	sizeof(struct cia_softc),
};

#define CIA_SGMAP_BASE		(8*1024*1024)
#define CIA_SGMAP_SIZE		(8*1024*1024)
#define	CIA_PYXIS_BUG_BASE	(128*1024*1024)
#define	CIA_PYXIS_BUG_SIZE	(2*1024*1024)

static void
cia_sgmap_invalidate(void)
{
	REGVAL(CIA_PCI_TBIA) = CIA_PCI_TBIA_ALL;
	alpha_mb();
}

static void
cia_sgmap_invalidate_pyxis(void)
{
	volatile u_int64_t dummy;
	u_int32_t ctrl;
	int i, s;

	s = splhigh();

	/*
	 * Put the Pyxis into PCI loopback mode.
	 */
	alpha_mb();
	ctrl = REGVAL(CIA_CSR_CTRL);
	REGVAL(CIA_CSR_CTRL) = ctrl | CTRL_PCI_LOOP_EN;
	alpha_mb();

	/*
	 * Now, read from PCI dense memory space at offset 128M (our
	 * target window base), skipping 64k on each read.  This forces
	 * S/G TLB misses.
	 *
	 * XXX Looks like the TLB entries are `not quite LRU'.  We need
	 * XXX to read more times than there are actual tags!
	 */
	for (i = 0; i < CIA_TLB_NTAGS + 4; i++) {
		dummy = *((volatile u_int64_t *)
		    ALPHA_PHYS_TO_K0SEG(CIA_PCI_DENSE + CIA_PYXIS_BUG_BASE +
		    (i * 65536)));
	}

	/*
	 * Restore normal PCI operation.
	 */
	alpha_mb();
	REGVAL(CIA_CSR_CTRL) = ctrl;
	alpha_mb();

	splx(s);
}

static void
cia_sgmap_map(void *arg, vm_offset_t ba, vm_offset_t pa)
{
	u_int64_t *sgtable = arg;
	int index = alpha_btop(ba - CIA_SGMAP_BASE);

	if (pa) {
		if (pa > (1L<<32))
			panic("cia_sgmap_map: can't map address 0x%lx", pa);
		sgtable[index] = ((pa >> 13) << 1) | 1;
	} else {
		sgtable[index] = 0;
	}
	alpha_mb();

	if (cia_ispyxis)
		cia_sgmap_invalidate_pyxis();
	else
		cia_sgmap_invalidate();
}

static void
cia_init_sgmap(void)
{
	void *sgtable;

	/*
	 * First setup Window 0 to map 8Mb to 16Mb with an
	 * sgmap. Allocate the map aligned to a 32k boundary.
	 */
	REGVAL(CIA_PCI_W0BASE) = (CIA_SGMAP_BASE
				  | CIA_PCI_WnBASE_SG_EN
				  | CIA_PCI_WnBASE_W_EN);
	alpha_mb();

	REGVAL(CIA_PCI_W0MASK) = CIA_PCI_WnMASK_8M;
	alpha_mb();

	sgtable = contigmalloc(8192, M_DEVBUF, M_NOWAIT,
			       0, (1L<<34),
			       32*1024, (1L<<34));
	if (!sgtable)
		panic("cia_init_sgmap: can't allocate page table");
	REGVAL(CIA_PCI_T0BASE) =
		(pmap_kextract((vm_offset_t) sgtable) >> CIA_PCI_TnBASE_SHIFT);

	chipset.sgmap = sgmap_map_create(CIA_SGMAP_BASE,
					 CIA_SGMAP_BASE + CIA_SGMAP_SIZE - 1,
					 cia_sgmap_map, sgtable);

	if (cia_ispyxis) {
		/*
		 * Pyxis has broken TLB invalidate. We use the NetBSD
		 * workaround of using another region to spill entries 
		 * out of the TLB. The 'bug' region is 2Mb mapped at
		 * 128Mb.
		 */
		int i;
		vm_offset_t pa;
		u_int64_t *bugtable;

		REGVAL(CIA_PCI_W2BASE) = CIA_PYXIS_BUG_BASE |
		    CIA_PCI_WnBASE_SG_EN | CIA_PCI_WnBASE_W_EN;
		alpha_mb();

		REGVAL(CIA_PCI_W2MASK) = CIA_PCI_WnMASK_2M;
		alpha_mb();

		bugtable = contigmalloc(8192, M_DEVBUF, M_NOWAIT,
				       0, (1L<<34),
				       2*1024, (1L<<34));
		if (!bugtable)
			panic("cia_init_sgmap: can't allocate page table");
		REGVAL(CIA_PCI_T2BASE) =
			(pmap_kextract((vm_offset_t) bugtable)
			 >> CIA_PCI_TnBASE_SHIFT);

		pa = sgmap_overflow_page();
		for (i = 0; i < alpha_btop(CIA_PYXIS_BUG_SIZE); i++)
			bugtable[i] = ((pa >> 13) << 1) | 1;
	}
}

void
cia_init()
{
	static int initted = 0;

	if (initted) return;
	initted = 1;

	cia_rev = REGVAL(CIA_CSR_REV) & REV_MASK;

	/*
	 * Determine if we have a Pyxis.  Only two systypes can
	 * have this: the EB164 systype (AlphaPC164LX and AlphaPC164SX)
	 * and the DEC_ST550 systype (Miata).
	 */
	if ((hwrpb->rpb_type == ST_EB164 &&
	     (hwrpb->rpb_variation & SV_ST_MASK) >= SV_ST_ALPHAPC164LX_400) ||
	    hwrpb->rpb_type == ST_DEC_550)
		cia_ispyxis = TRUE;
	else
		cia_ispyxis = FALSE;
	
	/*
	 * ALCOR/ALCOR2 Revisions >= 2 and Pyxis have the CNFG register.
	 */
	if (cia_rev >= 2 || cia_ispyxis)
		cia_config = REGVAL(CIA_CSR_CNFG);
	else
		cia_config = 0;

	if (alpha_implver() != ALPHA_IMPLVER_EV5
	    || alpha_amask(ALPHA_AMASK_BWX)
	    || !(cia_config & CNFG_BWEN)) {
		chipset = cia_swiz_chipset;
		chipset_bwx = 0;
	} else {
		chipset = cia_bwx_chipset;
		chipset_bwx = 1;
	}
	cia_hae_mem = REGVAL(CIA_CSR_HAE_MEM);

#if 0
	chipset = cia_swiz_chipset; /* XXX */
	cia_ispyxis = 0;
	chipset_bwx = 0;
#endif

	if (platform.pci_intr_init)
		platform.pci_intr_init();
}

static int
cia_probe(device_t dev)
{
	if (cia0)
		return ENXIO;
	cia0 = dev;
	device_set_desc(dev, "2117x Core Logic chipset"); /* XXX */

	pci_init_resources();
	isa_init_intr();
	cia_init_sgmap();

	device_add_child(dev, "pcib", 0);

	return 0;
}

static int
cia_attach(device_t dev)
{
	char* name;
	int pass;

	cia_init();

	if (cia_ispyxis) {
		name = "Pyxis";
		pass = cia_rev;
	} else {
		name = chipset_bwx ? "Alcor 2" : "Alcor";
		pass = cia_rev+1;
	}
	printf("cia0: %s, pass %d\n", name, pass);
	if (cia_config)
		printf("cia0: extended capabilities: %b\n",
		       cia_config, CIA_CSR_CNFG_BITS);

#ifdef DEC_ST550
	if (hwrpb->rpb_type == ST_DEC_550 &&
	    (hwrpb->rpb_variation & SV_ST_MASK) < SV_ST_MIATA_1_5) {
		/*
		 * Miata 1 systems have a bug: DMA cannot cross
		 * an 8k boundary!  Make sure PCI read prefetching
		 * is disabled on these chips.  Note that secondary
		 * PCI busses don't have this problem, because of
		 * the way PPBs handle PCI read requests.
		 *
		 * In the 21174 Technical Reference Manual, this is
		 * actually documented as "Pyxis Pass 1", but apparently
		 * there are chips that report themselves as "Pass 1"
		 * which do not have the bug!  Miatas with the Cypress
		 * PCI-ISA bridge (i.e. Miata 1.5 and Miata 2) do not
		 * have the bug, so we use this check.
		 *
		 * XXX We also need to deal with this boundary constraint
		 * XXX in the PCI bus 0 (and ISA) DMA tags, but some
		 * XXX drivers are going to need to be changed first.
		 */
		u_int32_t ctrl;

		/* XXX no bets... */
		printf("cia0: WARNING: Pyxis pass 1 DMA bug; no bets...\n");

		alpha_mb();
		ctrl = REGVAL(CIA_CSR_CTRL);
		ctrl &= ~(CTRL_RD_TYPE|CTRL_RL_TYPE|CTRL_RM_TYPE);
		REGVAL(CIA_CSR_CTRL) = ctrl;
		alpha_mb();
	}
#endif

	if (!platform.iointr)	/* XXX */
		set_iointr(alpha_dispatch_intr);

	if (chipset_bwx) {
		if (cia_ispyxis)
			snprintf(chipset_type, sizeof(chipset_type), "pyxis");
		else
			snprintf(chipset_type, sizeof(chipset_type), "alcor2");
		chipset_ports = CIA_EV56_BWIO;
		chipset_memory = CIA_EV56_BWMEM;
		chipset_dense = CIA_PCI_DENSE;
	} else {
		snprintf(chipset_type, sizeof(chipset_type), "cia");
		chipset_ports = CIA_PCI_SIO1;
		chipset_memory = CIA_PCI_SMEM1;
		chipset_dense = CIA_PCI_DENSE;
		chipset_hae_mask = 7L << 29;
	}

	bus_generic_attach(dev);
	return 0;
}

static int
cia_setup_intr(device_t dev, device_t child,
	       struct resource *irq, int flags,
	       driver_intr_t *intr, void *arg, void **cookiep)
{
	int error;
	
	error = rman_activate_resource(irq);
	if (error)
		return error;

	error = alpha_setup_intr(0x900 + (irq->r_start << 4),
			intr, arg, cookiep,
			&intrcnt[INTRCNT_EB164_IRQ + irq->r_start]);
	if (error)
		return error;

	/* Enable PCI interrupt */
	platform.pci_intr_enable(irq->r_start);

	device_printf(child, "interrupting at CIA irq %d\n",
		      (int) irq->r_start);

	return 0;
}

static int
cia_teardown_intr(device_t dev, device_t child,
		  struct resource *irq, void *cookie)
{
	alpha_teardown_intr(cookie);
	return rman_deactivate_resource(irq);
}

DRIVER_MODULE(cia, root, cia_driver, cia_devclass, 0, 0);
