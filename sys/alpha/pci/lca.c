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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <alpha/pci/lcareg.h>
#include <alpha/pci/lcavar.h>
#include <alpha/isa/isavar.h>

#include <machine/cpuconf.h>
#include <machine/intr.h>
#include <machine/md_var.h>
#include <machine/sgmap.h>
#include <machine/swiz.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	lca_devclass;
static device_t		lca0;		/* XXX only one for now */

struct lca_softc {
	int		junk;
};

#define LCA_SOFTC(dev)	(struct lca_softc*) device_get_softc(dev)

static alpha_chipset_read_hae_t	lca_read_hae;
static alpha_chipset_write_hae_t lca_write_hae;

static alpha_chipset_t lca_chipset = {
	lca_read_hae,
	lca_write_hae,
};

/*
 * The LCA HAE is write-only.  According to NetBSD, this is where it starts.
 */
static u_int32_t	lca_hae_mem = 0x80000000;

/*
 * The first 16Mb ignores the HAE.  The next 112Mb uses the HAE to set
 * the high bits of the PCI address.
 */
#define REG1 (1UL << 24)

static u_int32_t
lca_set_hae_mem(void *arg, u_int32_t pa)
{
	int s; 
	u_int32_t msb;
	if(pa >= REG1){
		msb = pa & 0xf8000000;
		pa -= msb;
		s = splhigh();
                if (msb != lca_hae_mem) {
			lca_hae_mem = msb;
			REGVAL(LCA_IOC_HAE) = lca_hae_mem;
			alpha_mb();
			alpha_mb();
		}
		splx(s);
	}
	return pa;
}

static u_int64_t
lca_read_hae(void)
{
	return lca_hae_mem & 0xf8000000;
}

static void
lca_write_hae(u_int64_t hae)
{
	u_int32_t pa = hae;
	lca_set_hae_mem(0, pa);
}

static int lca_probe(device_t dev);
static int lca_attach(device_t dev);
static device_method_t lca_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lca_probe),
	DEVMETHOD(device_attach,	lca_attach),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr,	isa_setup_intr),
	DEVMETHOD(bus_teardown_intr,	isa_teardown_intr),

	{ 0, 0 }
};

static driver_t lca_driver = {
	"lca",
	lca_methods,
	sizeof(struct lca_softc),
};

#define LCA_SGMAP_BASE		(8*1024*1024)
#define LCA_SGMAP_SIZE		(8*1024*1024)

static void
lca_sgmap_invalidate(void)
{
	alpha_mb();
	REGVAL(LCA_IOC_TBIA) = 0;
	alpha_mb();
}

static void
lca_sgmap_map(void *arg, bus_addr_t ba, vm_offset_t pa)
{
	u_int64_t *sgtable = arg;
	int index = alpha_btop(ba - LCA_SGMAP_BASE);

	if (pa) {
		if (pa > (1L<<32))
			panic("lca_sgmap_map: can't map address 0x%lx", pa);
		sgtable[index] = ((pa >> 13) << 1) | 1;
	} else {
		sgtable[index] = 0;
	}
	alpha_mb();
	lca_sgmap_invalidate();
}

static void
lca_init_sgmap(void)
{
	void *sgtable;

	/*
	 * First setup Window 0 to map 8Mb to 16Mb with an
	 * sgmap. Allocate the map aligned to a 32 boundary.
	 */
	REGVAL64(LCA_IOC_W_BASE0) = LCA_SGMAP_BASE |
		IOC_W_BASE_SG | IOC_W_BASE_WEN;
	alpha_mb();

	REGVAL64(LCA_IOC_W_MASK0) = IOC_W_MASK_8M;
	alpha_mb();

	sgtable = contigmalloc(8192, M_DEVBUF, M_NOWAIT,
			       0, (1L<<34),
			       32*1024, (1L<<34));
	if (!sgtable)
		panic("lca_init_sgmap: can't allocate page table");
	chipset.sgmap = sgmap_map_create(LCA_SGMAP_BASE,
					 LCA_SGMAP_BASE + LCA_SGMAP_SIZE,
					 lca_sgmap_map, sgtable);

	
	REGVAL64(LCA_IOC_W_T_BASE0) = pmap_kextract((vm_offset_t) sgtable);
	alpha_mb();
	REGVAL64(LCA_IOC_TB_ENA) = IOC_TB_ENA_TEN;
	alpha_mb();
	lca_sgmap_invalidate();
}

void
lca_init()
{
	static int initted = 0;
	static struct swiz_space io_space, mem_space;

	if (initted) return;
	initted = 1;

	swiz_init_space(&io_space, KV(LCA_PCI_SIO));
	swiz_init_space_hae(&mem_space, KV(LCA_PCI_SPARSE),
			    lca_set_hae_mem, 0);

	busspace_isa_io = (struct alpha_busspace *) &io_space;
	busspace_isa_mem = (struct alpha_busspace *) &mem_space;

	/* Type 0 PCI conf access. */
	REGVAL64(LCA_IOC_CONF) = 0;

	if (platform.pci_intr_init)
		platform.pci_intr_init();

	chipset = lca_chipset;
}

static void
lca_machine_check(unsigned long mces, struct trapframe *framep,
    unsigned long vector, unsigned long param);

static int
lca_probe(device_t dev)
{
	if (lca0)
		return ENXIO;
	lca0 = dev;
	device_set_desc(dev, "21066 Core Logic chipset"); /* XXX */

	isa_init_intr();
	lca_init_sgmap();

	platform.mcheck_handler = lca_machine_check;

	device_add_child(dev, "pcib", 0);

	return 0;
}

static int
lca_attach(device_t dev)
{
	lca_init();

	set_iointr(alpha_dispatch_intr);

	snprintf(chipset_type, sizeof(chipset_type), "lca");
	chipset_bwx = 0;
	chipset_ports = LCA_PCI_SIO;
	chipset_memory = LCA_PCI_SPARSE;
	chipset_dense = LCA_PCI_DENSE;
	chipset_hae_mask = IOC_HAE_ADDREXT;

	bus_generic_attach(dev);
	return 0;
}

static void
lca_machine_check(unsigned long mces, struct trapframe *framep,
    unsigned long vector, unsigned long param)
{
	long stat0;

	machine_check(mces, framep, vector, param);
	/* clear error flags in IOC_STATUS0 register */
	stat0 = REGVAL64(LCA_IOC_STAT0);
	REGVAL64(LCA_IOC_STAT0) = stat0;
}

DRIVER_MODULE(lca, root, lca_driver, lca_devclass, 0, 0);

