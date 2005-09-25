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

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>
#include <alpha/isa/isavar.h>

#include <machine/bwx.h>
#include <machine/cpuconf.h>
#include <machine/intr.h>
#include <machine/intrcnt.h>
#include <machine/md_var.h>
#include <machine/resource.h>
#include <machine/rpb.h>
#include <machine/sgmap.h>
#include <machine/swiz.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include "alphapci_if.h"

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	cia_devclass;
static device_t		cia0;		/* XXX only one for now */
static u_int32_t	cia_hae_mem;
static int		cia_rev, cia_ispyxis, cia_config;

struct cia_softc {
	int		junk;		/* no softc */
};

#define CIA_SOFTC(dev)	(struct cia_softc*) device_get_softc(dev)

static alpha_chipset_read_hae_t	cia_read_hae;
static alpha_chipset_write_hae_t cia_write_hae;

static alpha_chipset_t cia_bwx_chipset = {
	cia_read_hae,
	cia_write_hae,
};
static alpha_chipset_t cia_swiz_chipset = {
	cia_read_hae,
	cia_write_hae,
};

static u_int32_t
cia_swiz_set_hae_mem(void *arg, u_int32_t pa)
{
	/* Only bother with region 1 */
#define REG1 (7 << 29)
	if ((cia_hae_mem & REG1) != (pa & REG1)) {
		/*
		 * Seems fairly paranoid but this is what Linux does...
		 */
		u_int32_t msb = pa & REG1;
		register_t s;
		
		s = intr_disable();
		cia_hae_mem = (cia_hae_mem & ~REG1) | msb;
		REGVAL(CIA_CSR_HAE_MEM) = cia_hae_mem;
		alpha_mb();
		cia_hae_mem = REGVAL(CIA_CSR_HAE_MEM);
		intr_restore(s);
	}
	return pa & ~REG1;
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
	cia_swiz_set_hae_mem(0, pa);
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
	int i;
	register_t s;

	s = intr_disable();
	
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

	intr_restore(s);
}

static void
cia_sgmap_map(void *arg, bus_addr_t ba, vm_offset_t pa)
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
	chipset.pci_sgmap = NULL;
	chipset.dmsize = 1UL * 1024UL * 1024UL * 1024UL;
	chipset.dmoffset = 1UL * 1024UL * 1024UL * 1024UL;

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
	static union space {
		struct bwx_space bwx;
		struct swiz_space swiz;
	} io_space, mem_space;

	if (initted) return;
	initted = 1;

	if (chipset_bwx == 0) {
		swiz_init_space(&io_space.swiz, KV(CIA_PCI_SIO1));
		swiz_init_space_hae(&mem_space.swiz, KV(CIA_PCI_SMEM1),
				    cia_swiz_set_hae_mem, 0);

		chipset = cia_swiz_chipset;
	} else {
		bwx_init_space(&io_space.bwx, KV(CIA_EV56_BWIO));
		bwx_init_space(&mem_space.bwx, KV(CIA_EV56_BWMEM));

		chipset = cia_bwx_chipset;
	}
	cia_hae_mem = REGVAL(CIA_CSR_HAE_MEM);

	busspace_isa_io = (struct alpha_busspace *) &io_space;
	busspace_isa_mem = (struct alpha_busspace *) &mem_space;

	if (platform.pci_intr_init)
		platform.pci_intr_init();
}

static int
cia_probe(device_t dev)
{
	uintptr_t use_bwx = 1;
	device_t child;

	if (cia0)
		return ENXIO;
	cia0 = dev;
	device_set_desc(dev, "2117x Core Logic chipset"); /* XXX */

	isa_init_intr();

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

	cia_init_sgmap();

	/*
	 * ALCOR/ALCOR2 Revisions >= 2 and Pyxis have the CNFG register.
	 */
	if (cia_rev >= 2 || cia_ispyxis)
		cia_config = REGVAL(CIA_CSR_CNFG);
	else
		cia_config = 0;

	if ((alpha_implver() < ALPHA_IMPLVER_EV5) ||
	    (alpha_amask(ALPHA_AMASK_BWX) != 0) ||
	    (cia_config & CNFG_BWEN) == 0) {
		use_bwx = 0;
	} else {
		use_bwx = 1;
	}

	if (cia_ispyxis) {
		if (use_bwx == 0) {
			printf("PYXIS but not BWX?\n");
		}
	}

	child = device_add_child(dev, "pcib", 0);
	chipset_bwx = use_bwx = (use_bwx == (uintptr_t) 1);
	device_set_ivars(child, (void *)use_bwx);
	return 0;
}

static int
cia_attach(device_t dev)
{
	char* name;
	int pass;

	cia_init();

	name = cia_ispyxis ? "Pyxis" : "ALCOR/ALCOR2";
	if (cia_ispyxis) {
		name = "Pyxis";
		pass = cia_rev;
	} else {
		name = "ALCOR/ALCOR2";
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
		snprintf(chipset_type, sizeof(chipset_type), "cia/bwx");
		chipset_bwx = 1;
		chipset_ports = CIA_EV56_BWIO;
		chipset_memory = CIA_EV56_BWMEM;
		chipset_dense = CIA_PCI_DENSE;
	} else {
		snprintf(chipset_type, sizeof(chipset_type), "cia/swiz");
		chipset_bwx = 0;
		chipset_ports = CIA_PCI_SIO1;
		chipset_memory = CIA_PCI_SMEM1;
		chipset_dense = CIA_PCI_DENSE;
		chipset_hae_mask = 7L << 29;
	}

	bus_generic_attach(dev);
	return 0;
}

static void
cia_disable_intr(uintptr_t vector)
{
	int irq;

	irq = (vector - 0x900) >> 4;
	mtx_lock_spin(&icu_lock);
	platform.pci_intr_disable(irq);
	mtx_unlock_spin(&icu_lock);
}

static void
cia_enable_intr(uintptr_t vector)
{
	int irq;

	irq = (vector - 0x900) >> 4;
	mtx_lock_spin(&icu_lock);
	platform.pci_intr_enable(irq);
	mtx_unlock_spin(&icu_lock);
}

static int
cia_setup_intr(device_t dev, device_t child,
	       struct resource *irq, int flags,
	       driver_intr_t *intr, void *arg, void **cookiep)
{
	int error, start;
	
	error = rman_activate_resource(irq);
	if (error)
		return error;
	start = rman_get_start(irq);

	error = alpha_setup_intr(
			device_get_nameunit(child ? child : dev),
			0x900 + (start << 4), intr, arg, flags, cookiep,
			&intrcnt[INTRCNT_EB164_IRQ + start],
			cia_disable_intr, cia_enable_intr);
	if (error)
		return error;

	/* Enable PCI interrupt */
	mtx_lock_spin(&icu_lock);
	platform.pci_intr_enable(start);
	mtx_unlock_spin(&icu_lock);

	device_printf(child, "interrupting at CIA irq %d\n", start);

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
