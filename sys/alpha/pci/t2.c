/*
 * Copyright (c) 2000 Andrew Gallatin & Doug Rabson
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

/*
 * T2 CBUS to PCI bridge
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

#include <alpha/pci/t2reg.h>
#include <alpha/pci/t2var.h>
#include <alpha/pci/pcibus.h>
#include <alpha/isa/isavar.h>
#include <machine/intr.h>
#include <machine/resource.h>
#include <machine/intrcnt.h>
#include <machine/cpuconf.h>
#include <machine/swiz.h>
#include <machine/sgmap.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa + t2_csr_base)

vm_offset_t	t2_csr_base = 0UL;

static devclass_t	t2_devclass;
static device_t		t2_0;		/* XXX only one for now */

struct t2_softc {
	int		junk;
};

#define T2_SOFTC(dev)	(struct t2_softc*) device_get_softc(dev)

static alpha_chipset_read_hae_t	t2_read_hae;
static alpha_chipset_write_hae_t t2_write_hae;

static alpha_chipset_t t2_chipset = {
	t2_read_hae,
	t2_write_hae,
};

static u_int32_t	t2_hae_mem;

#define REG1 (1UL << 24)

static u_int32_t
t2_set_hae_mem(void *arg, u_int32_t pa)
{
	int s; 
	u_int32_t msb;

	if(pa >= REG1){
		msb = pa & 0xf8000000;
		pa -= msb;
		msb >>= 27;	/* t2 puts high bits in the bottom of the register */
		s = splhigh();
		if (msb != t2_hae_mem) {
			t2_hae_mem = msb;
			REGVAL(T2_HAE0_1) = t2_hae_mem;
			alpha_mb();
			t2_hae_mem = REGVAL(T2_HAE0_1);
		}
		splx(s);
	}
	return pa;
}

static u_int64_t
t2_read_hae(void)
{
	return t2_hae_mem << 27;
}

static void
t2_write_hae(u_int64_t hae)
{
	u_int32_t pa = hae;
	t2_set_hae_mem(0, pa);
}

static int t2_probe(device_t dev);
static int t2_attach(device_t dev);
static int t2_setup_intr(device_t dev, device_t child,
			    struct resource *irq, int flags,
			    void *intr, void *arg, void **cookiep);
static int t2_teardown_intr(device_t dev, device_t child,
			    struct resource *irq, void *cookie);
static void
t2_dispatch_intr(void *frame, unsigned long vector);
static void
t2_machine_check(unsigned long mces, struct trapframe *framep, 
		 unsigned long vector, unsigned long param);


static device_method_t t2_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			t2_probe),
	DEVMETHOD(device_attach,		t2_attach),

	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,		pci_alloc_resource),
	DEVMETHOD(bus_release_resource,		pci_release_resource),
	DEVMETHOD(bus_activate_resource, 	pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	pci_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		t2_setup_intr),
	DEVMETHOD(bus_teardown_intr,		t2_teardown_intr),

	{ 0, 0 }
};

static driver_t t2_driver = {
	"t2",
	t2_methods,
	sizeof(struct t2_softc),
};


#define T2_SGMAP_BASE		(8*1024*1024)
#define T2_SGMAP_SIZE		(8*1024*1024)

static void
t2_sgmap_invalidate(void)
{
	u_int64_t val;

	alpha_mb();
	val = REGVAL64(T2_IOCSR);
	val |= T2_IOCSRL_ITLB;
	REGVAL64(T2_IOCSR) = val;
	alpha_mb();
	alpha_mb();
	val = REGVAL64(T2_IOCSR);
	val &= ~T2_IOCSRL_ITLB;
	REGVAL64(T2_IOCSR) = val;
	alpha_mb();
	alpha_mb();
}

static void
t2_sgmap_map(void *arg, bus_addr_t ba, vm_offset_t pa)
{
	u_int64_t *sgtable = arg;
	int index = alpha_btop(ba - T2_SGMAP_BASE);

	if (pa) {
		if (pa > (1L<<32))
			panic("t2_sgmap_map: can't map address 0x%lx", pa);
		sgtable[index] = ((pa >> 13) << 1) | 1;
	} else {
		sgtable[index] = 0;
	}
	alpha_mb();
	t2_sgmap_invalidate();
}


static void
t2_init_sgmap(void)
{
	void *sgtable;

	/*
	 * First setup Window 2 to map 8Mb to 16Mb with an
	 * sgmap. Allocate the map aligned to a 32 boundary.
	 *
	 *  bits 31..20 of WBASE represent the pci start address
	 *  (in units of 1Mb), and bits 11..0 represent the pci
	 *  end address
	 */
	REGVAL(T2_WBASE2) = T2_WSIZE_8M|T2_WINDOW_ENABLE|T2_WINDOW_SG
	                     | ((T2_SGMAP_BASE >> 20) << 20)
	                     | ((T2_SGMAP_BASE + T2_SGMAP_SIZE) >> 20);
	REGVAL(T2_WMASK2) = T2_WMASK_8M;
	alpha_mb();

	sgtable = contigmalloc(8192, M_DEVBUF, M_NOWAIT,
			       0, (1L<<34),
			       32*1024, (1L<<34));
	if (!sgtable)
		panic("t2_init_sgmap: can't allocate page table");

	REGVAL(T2_TBASE2) =  
	    (pmap_kextract((vm_offset_t) sgtable) >> T2_TBASE_SHIFT);

	chipset.sgmap = sgmap_map_create(T2_SGMAP_BASE,
					 T2_SGMAP_BASE + T2_SGMAP_SIZE,
					 t2_sgmap_map, sgtable);
}

/*
 * Perform basic chipset init/fixup.  Called by various early
 * consumers to ensure that the system will work before the 
 * bus methods are invoked.
 *
 */

void
t2_init()
{
	static int initted = 0;
	static struct swiz_space io_space, mem_space;

	if (initted) return;
	initted = 1;

	swiz_init_space(&io_space, KV(T2_PCI_SIO));
	swiz_init_space_hae(&mem_space, KV(T2_PCI_SPARSE),
			    t2_set_hae_mem, 0);

	busspace_isa_io = (kobj_t) &io_space;
	busspace_isa_mem = (kobj_t) &mem_space;

	chipset = t2_chipset;
}

static int
t2_probe(device_t dev)
{
	device_t child;

	if (t2_0)
		return ENXIO;
	t2_0 = dev;
	device_set_desc(dev, "T2 Core Logic chipset"); 

	pci_init_resources();

	/* 
	 * initialize the DMA windows
	 */

	REGVAL(T2_WBASE1) = T2_WSIZE_1G|T2_WINDOW_ENABLE|T2_WINDOW_DIRECT|0x7ff;
	REGVAL(T2_WMASK1) = T2_WMASK_1G;
	REGVAL(T2_TBASE1) = 0;

	REGVAL(T2_WBASE2) = 0x0;


	/* 
	 *  enable the PCI "Hole" for ISA devices which use memory in
	 *  the 512k - 1MB range
	 */
	REGVAL(T2_HBASE) = 1 << 13;
	t2_init_sgmap();


	/* initialize the HAEs */
	REGVAL(T2_HAE0_1) = 0x0; 
	alpha_mb();
	REGVAL(T2_HAE0_2) = 0x0; 
	alpha_mb();
	REGVAL(T2_HAE0_3) = 0x0; 
	alpha_mb();
	
	child = device_add_child(dev, "pcib", 0);
	device_set_ivars(child, 0);

	return 0;
}

static int
t2_attach(device_t dev)
{
	t2_init();

	platform.mcheck_handler = t2_machine_check;
	set_iointr(t2_dispatch_intr);
	platform.isa_setup_intr = t2_setup_intr;
	platform.isa_teardown_intr = t2_teardown_intr;

	snprintf(chipset_type, sizeof(chipset_type), "t2");

	bus_generic_attach(dev);

	return 0;
}

/*
 * magical mystery table partly obtained from Linux
 * at least some of their values for PCI masks
 * were incorrect, and I've filled in my own extrapolations
 * XXX this needs more testers 
 */

unsigned long t2_shadow_mask = -1L;
static const char irq_to_mask[40] = {
	-1,  6, -1,  8, 15, 12,  7,  9,		/* ISA 0-7  */
	-1, 16, 17, 18,  3, -1, 21, 22, 	/* ISA 8-15 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* ?? EISA XXX */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* ?? EISA XXX */
	 0,  1,  2,  3,  4,  5,  6,  7		/* PCI 0-7 XXX */
};

static void
t2_disable_intr(int vector)
{
	int mask = (vector - 0x900) >> 4;

	t2_shadow_mask |= (1UL << mask);

	if (mask <= 7)
		outb(SLAVE0_ICU, t2_shadow_mask);
	else if (mask <= 15)
		outb(SLAVE1_ICU, t2_shadow_mask >> 8);
	else 
		outb(SLAVE2_ICU, t2_shadow_mask >> 16);
}

static void
t2_enable_intr(int vector)
{
	int mask = (vector - 0x900) >> 4;

	t2_shadow_mask &= ~(1UL << mask);

	if (mask <= 7)
		outb(SLAVE0_ICU, t2_shadow_mask);
	else if (mask <= 15)
		outb(SLAVE1_ICU, t2_shadow_mask >> 8);
	else 
		outb(SLAVE2_ICU, t2_shadow_mask >> 16);
}

static int
t2_setup_intr(device_t dev, device_t child,
	       struct resource *irq, int flags,
	       void *intr, void *arg, void **cookiep)
{
	int error, mask, vector;

	mask = irq_to_mask[irq->r_start];
	vector = 0x800 + (mask << 4);
	
	error = rman_activate_resource(irq);
	if (error)
		return error;

	error = alpha_setup_intr(device_get_nameunit(child ? child : dev),
			vector, intr, arg, ithread_priority(flags), cookiep,
			&intrcnt[irq->r_start],
			t2_disable_intr, t2_enable_intr);
	if (error)
		return error;

	/* Enable interrupt */

	t2_shadow_mask &= ~(1UL << mask);

	if (mask <= 7)
		outb(SLAVE0_ICU, t2_shadow_mask);
	else if (mask <= 15)
		outb(SLAVE1_ICU, t2_shadow_mask >> 8);
	else 
		outb(SLAVE2_ICU, t2_shadow_mask >> 16);

	device_printf(child, "interrupting at T2 irq %d\n",
		      (int) irq->r_start);

	return 0;
}

static int
t2_teardown_intr(device_t dev, device_t child,
	       struct resource *irq, void *cookie)
{
	int mask;

	mask = irq_to_mask[irq->r_start];

	/* Disable interrupt */

	t2_shadow_mask |= (1UL << mask);

	if (mask <= 7)
		outb(SLAVE0_ICU, t2_shadow_mask);
	else if (mask <= 15)
		outb(SLAVE1_ICU, t2_shadow_mask >> 8);
	else 
		outb(SLAVE2_ICU, t2_shadow_mask >> 16);

	alpha_teardown_intr(cookie);
	return rman_deactivate_resource(irq);
}

static void
t2_ack_intr(unsigned long vector)
{
	int mask = (vector - 0x800) >> 4;
	
	switch (mask) {
		case 0 ... 7:
			outb(SLAVE0_ICU-1, (0xe0 | (mask)));
			outb(MASTER_ICU-1, (0xe0 | 1));
			break;
		case 8 ... 15:
			outb(SLAVE1_ICU-1, (0xe0 | (mask - 8)));
			outb(MASTER_ICU-1, (0xe0 | 3));
			break;
		case 16 ... 24:
			outb(SLAVE2_ICU-1, (0xe0 | (mask - 16)));
			outb(MASTER_ICU-1, (0xe0 | 4));
			break;
	}
}


static void
t2_dispatch_intr(void *frame, unsigned long vector)
{
	alpha_dispatch_intr(frame, vector);
	t2_ack_intr(vector);
}

static void
t2_machine_check(unsigned long mces, struct trapframe *framep, 
    unsigned long vector, unsigned long param)
{
	int expected;

	expected = mc_expected;
	machine_check(mces, framep, vector, param);
	/* for some reason the alpha_pal_wrmces() doesn't clear all
	   pending machine checks & we may take another */
	mc_expected = expected;
}

DRIVER_MODULE(t2, root, t2_driver, t2_devclass, 0, 0);
