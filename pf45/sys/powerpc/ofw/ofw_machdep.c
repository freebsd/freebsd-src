/*-
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: ofw_machdep.c,v 1.5 2000/05/23 13:25:43 tsubai Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/smp.h>
#include <sys/stat.h>

#include <net/ethernet.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofw_bus.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/platform.h>
#include <machine/ofw_machdep.h>

#define	OFMEM_REGIONS	32
static struct mem_region OFmem[OFMEM_REGIONS + 1], OFavail[OFMEM_REGIONS + 3];
static struct mem_region OFfree[OFMEM_REGIONS + 3];
static int nOFmem;

extern register_t ofmsr[5];
static int	(*ofwcall)(void *);
static void	*fdt;
int		ofw_real_mode;

int		ofw_32bit_mode_entry(void *);
static void	ofw_quiesce(void);
static int	openfirmware(void *args);

/*
 * Saved SPRG0-3 from OpenFirmware. Will be restored prior to the callback.
 */
register_t	ofw_sprg0_save;

static __inline void
ofw_sprg_prepare(void)
{
	/*
	 * Assume that interrupt are disabled at this point, or
	 * SPRG1-3 could be trashed
	 */
	__asm __volatile("mfsprg0 %0\n\t"
			 "mtsprg0 %1\n\t"
	    		 "mtsprg1 %2\n\t"
	    		 "mtsprg2 %3\n\t"
			 "mtsprg3 %4\n\t"
			 : "=&r"(ofw_sprg0_save)
			 : "r"(ofmsr[1]),
			 "r"(ofmsr[2]),
			 "r"(ofmsr[3]),
			 "r"(ofmsr[4]));
}

static __inline void
ofw_sprg_restore(void)
{
	/*
	 * Note that SPRG1-3 contents are irrelevant. They are scratch
	 * registers used in the early portion of trap handling when
	 * interrupts are disabled.
	 *
	 * PCPU data cannot be used until this routine is called !
	 */
	__asm __volatile("mtsprg0 %0" :: "r"(ofw_sprg0_save));
}

/*
 * Memory region utilities: determine if two regions overlap,
 * and merge two overlapping regions into one
 */
static int
memr_overlap(struct mem_region *r1, struct mem_region *r2)
{
	if ((r1->mr_start + r1->mr_size) < r2->mr_start ||
	    (r2->mr_start + r2->mr_size) < r1->mr_start)
		return (FALSE);
	
	return (TRUE);	
}

static void
memr_merge(struct mem_region *from, struct mem_region *to)
{
	vm_offset_t end;
	end = ulmax(to->mr_start + to->mr_size, from->mr_start + from->mr_size);
	to->mr_start = ulmin(from->mr_start, to->mr_start);
	to->mr_size = end - to->mr_start;
}

static int
parse_ofw_memory(phandle_t node, const char *prop, struct mem_region *output)
{
	cell_t address_cells, size_cells;
	cell_t OFmem[4*(OFMEM_REGIONS + 1)];
	int sz, i, j;
	int apple_hack_mode;
	phandle_t phandle;

	sz = 0;
	apple_hack_mode = 0;

	/*
	 * Get #address-cells from root node, defaulting to 1 if it cannot
	 * be found.
	 */
	phandle = OF_finddevice("/");
	if (OF_getprop(phandle, "#address-cells", &address_cells, 
	    sizeof(address_cells)) < sizeof(address_cells))
		address_cells = 1;
	if (OF_getprop(phandle, "#size-cells", &size_cells, 
	    sizeof(size_cells)) < sizeof(size_cells))
		size_cells = 1;

	/*
	 * On Apple hardware, address_cells is always 1 for "available",
	 * even when it is explicitly set to 2. Then all memory above 4 GB
	 * should be added by hand to the available list. Detect Apple hardware
	 * by seeing if ofw_real_mode is set -- only Apple seems to use
	 * virtual-mode OF.
	 */
	if (strcmp(prop, "available") == 0 && !ofw_real_mode)
		apple_hack_mode = 1;
	
	if (apple_hack_mode)
		address_cells = 1;

	/*
	 * Get memory.
	 */
	if ((node == -1) || (sz = OF_getprop(node, prop,
	    OFmem, sizeof(OFmem[0]) * 4 * OFMEM_REGIONS)) <= 0)
		panic("Physical memory map not found");

	i = 0;
	j = 0;
	while (i < sz/sizeof(cell_t)) {
	      #ifndef __powerpc64__
		/* On 32-bit PPC, ignore regions starting above 4 GB */
		if (address_cells > 1 && OFmem[i] > 0) {
			i += address_cells + size_cells;
			continue;
		}
	      #endif

		output[j].mr_start = OFmem[i++];
		if (address_cells == 2) {
			#ifdef __powerpc64__
			output[j].mr_start <<= 32;
			#endif
			output[j].mr_start += OFmem[i++];
		}
			
		output[j].mr_size = OFmem[i++];
		if (size_cells == 2) {
			#ifdef __powerpc64__
			output[j].mr_size <<= 32;
			#endif
			output[j].mr_size += OFmem[i++];
		}

	      #ifndef __powerpc64__
		/*
		 * Check for memory regions extending above 32-bit
		 * memory space, and restrict them to stay there.
		 */
		if (((uint64_t)output[j].mr_start +
		    (uint64_t)output[j].mr_size) >
		    BUS_SPACE_MAXADDR_32BIT) {
			output[j].mr_size = BUS_SPACE_MAXADDR_32BIT -
			    output[j].mr_start;
		}
	      #endif

		j++;
	}
	sz = j*sizeof(output[0]);

	#ifdef __powerpc64__
	if (apple_hack_mode) {
		/* Add in regions above 4 GB to the available list */
		struct mem_region himem[OFMEM_REGIONS];
		int hisz;

		hisz = parse_ofw_memory(node, "reg", himem);
		for (i = 0; i < hisz/sizeof(himem[0]); i++) {
			if (himem[i].mr_start > BUS_SPACE_MAXADDR_32BIT) {
				output[j].mr_start = himem[i].mr_start;
				output[j].mr_size = himem[i].mr_size;
				j++;
			}
		}
		sz = j*sizeof(output[0]);
	}
	#endif

	return (sz);
}

/*
 * This is called during powerpc_init, before the system is really initialized.
 * It shall provide the total and the available regions of RAM.
 * Both lists must have a zero-size entry as terminator.
 * The available regions need not take the kernel into account, but needs
 * to provide space for two additional entry beyond the terminating one.
 */
void
ofw_mem_regions(struct mem_region **memp, int *memsz,
		struct mem_region **availp, int *availsz)
{
	phandle_t phandle;
	int asz, msz, fsz;
	int i, j;
	int still_merging;

	asz = msz = 0;

	/*
	 * Get memory.
	 */
	phandle = OF_finddevice("/memory");
	if (phandle == -1)
		phandle = OF_finddevice("/memory@0");

	msz = parse_ofw_memory(phandle, "reg", OFmem);
	nOFmem = msz / sizeof(struct mem_region);
	asz = parse_ofw_memory(phandle, "available", OFavail);

	*memp = OFmem;
	*memsz = nOFmem;
	
	/*
	 * OFavail may have overlapping regions - collapse these
	 * and copy out remaining regions to OFfree
	 */
	asz /= sizeof(struct mem_region);
	do {
		still_merging = FALSE;
		for (i = 0; i < asz; i++) {
			if (OFavail[i].mr_size == 0)
				continue;
			for (j = i+1; j < asz; j++) {
				if (OFavail[j].mr_size == 0)
					continue;
				if (memr_overlap(&OFavail[j], &OFavail[i])) {
					memr_merge(&OFavail[j], &OFavail[i]);
					/* mark inactive */
					OFavail[j].mr_size = 0;
					still_merging = TRUE;
				}
			}
		}
	} while (still_merging == TRUE);

	/* evict inactive ranges */
	for (i = 0, fsz = 0; i < asz; i++) {
		if (OFavail[i].mr_size != 0) {
			OFfree[fsz] = OFavail[i];
			fsz++;
		}
	}

	*availp = OFfree;
	*availsz = fsz;
}

void
OF_initial_setup(void *fdt_ptr, void *junk, int (*openfirm)(void *))
{
	if (ofmsr[0] & PSL_DR)
		ofw_real_mode = 0;
	else
		ofw_real_mode = 1;

	ofwcall = NULL;

	#ifdef __powerpc64__
		/*
		 * For PPC64, we need to use some hand-written
		 * asm trampolines to get to OF.
		 */
		if (openfirm != NULL)
			ofwcall = ofw_32bit_mode_entry;
	#else
		ofwcall = openfirm;
	#endif

	fdt = fdt_ptr;

	#ifdef FDT_DTB_STATIC
	/* Check for a statically included blob */
	if (fdt == NULL)
		fdt = &fdt_static_dtb;
	#endif
}

boolean_t
OF_bootstrap()
{
	boolean_t status = FALSE;

	if (ofwcall != NULL) {
		if (ofw_real_mode) {
			status = OF_install(OFW_STD_REAL, 0);
		} else {
			#ifdef __powerpc64__
			status = OF_install(OFW_STD_32BIT, 0);
			#else
			status = OF_install(OFW_STD_DIRECT, 0);
			#endif
		}

		if (status != TRUE)
			return status;

		OF_init(openfirmware);

		/*
		 * On some machines, we need to quiesce OF to turn off
		 * background processes.
		 */
		ofw_quiesce();
	} else if (fdt != NULL) {
		status = OF_install(OFW_FDT, 0);

		if (status != TRUE)
			return status;

		OF_init(fdt);
	} 

	return (status);
}

static void
ofw_quiesce(void)
{
	phandle_t rootnode;
	char model[32];
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
	} args;

	/*
	 * Only quiesce Open Firmware on PowerMac11,2 and 12,1. It is
	 * necessary there to shut down a background thread doing fan
	 * management, and is harmful on other machines.
	 *
	 * Note: we don't need to worry about which OF module we are
	 * using since this is called only from very early boot, within
	 * OF's boot context.
	 */

	rootnode = OF_finddevice("/");
	if (OF_getprop(rootnode, "model", model, sizeof(model)) > 0) {
		if (strcmp(model, "PowerMac11,2") == 0 ||
		    strcmp(model, "PowerMac12,1") == 0) {
			args.name = (cell_t)(uintptr_t)"quiesce";
			args.nargs = 0;
			args.nreturns = 0;
			openfirmware(&args);
		}
	}
}

static int
openfirmware_core(void *args)
{
	int		result;
	register_t	oldmsr;

	/*
	 * Turn off exceptions - we really don't want to end up
	 * anywhere unexpected with PCPU set to something strange
	 * or the stack pointer wrong.
	 */
	oldmsr = intr_disable();

	ofw_sprg_prepare();

#if defined(AIM) && !defined(__powerpc64__)
	/*
	 * Clear battable[] translations
	 */
	if (!(cpu_features & PPC_FEATURE_64))
		__asm __volatile("mtdbatu 2, %0\n"
				 "mtdbatu 3, %0" : : "r" (0));
	isync();
#endif

       	result = ofwcall(args);
	ofw_sprg_restore();

	intr_restore(oldmsr);

	return (result);
}

#ifdef SMP
struct ofw_rv_args {
	void *args;
	int retval;
	volatile int in_progress;
};

static void
ofw_rendezvous_dispatch(void *xargs)
{
	struct ofw_rv_args *rv_args = xargs;

	/* NOTE: Interrupts are disabled here */

	if (PCPU_GET(cpuid) == 0) {
		/*
		 * Execute all OF calls on CPU 0
		 */
		rv_args->retval = openfirmware_core(rv_args->args);
		rv_args->in_progress = 0;
	} else {
		/*
		 * Spin with interrupts off on other CPUs while OF has
		 * control of the machine.
		 */
		while (rv_args->in_progress)
			cpu_spinwait();
	}
}
#endif

static int
openfirmware(void *args)
{
	int result;
	#ifdef SMP
	struct ofw_rv_args rv_args;
	#endif

	if (pmap_bootstrapped && ofw_real_mode)
		args = (void *)pmap_kextract((vm_offset_t)args);

	#ifdef SMP
	rv_args.args = args;
	rv_args.in_progress = 1;
	smp_rendezvous(smp_no_rendevous_barrier, ofw_rendezvous_dispatch,
	    smp_no_rendevous_barrier, &rv_args);
	result = rv_args.retval;
	#else
	result = openfirmware_core(args);
	#endif

	return (result);
}

void
OF_reboot()
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t arg;
	} args;

	args.name = (cell_t)(uintptr_t)"interpret";
	args.nargs = 1;
	args.nreturns = 0;
	args.arg = (cell_t)(uintptr_t)"reset-all";
	openfirmware_core(&args); /* Don't do rendezvous! */

	for (;;);	/* just in case */
}

void
OF_getetheraddr(device_t dev, u_char *addr)
{
	phandle_t	node;

	node = ofw_bus_get_node(dev);
	OF_getprop(node, "local-mac-address", addr, ETHER_ADDR_LEN);
}

/*
 * Return a bus handle and bus tag that corresponds to the register
 * numbered regno for the device referenced by the package handle
 * dev. This function is intended to be used by console drivers in
 * early boot only. It works by mapping the address of the device's
 * register in the address space of its parent and recursively walk
 * the device tree upward this way.
 */
static void
OF_get_addr_props(phandle_t node, uint32_t *addrp, uint32_t *sizep, int *pcip)
{
	char name[16];
	uint32_t addr, size;
	int pci, res;

	res = OF_getprop(node, "#address-cells", &addr, sizeof(addr));
	if (res == -1)
		addr = 2;
	res = OF_getprop(node, "#size-cells", &size, sizeof(size));
	if (res == -1)
		size = 1;
	pci = 0;
	if (addr == 3 && size == 2) {
		res = OF_getprop(node, "name", name, sizeof(name));
		if (res != -1) {
			name[sizeof(name) - 1] = '\0';
			pci = (strcmp(name, "pci") == 0) ? 1 : 0;
		}
	}
	if (addrp != NULL)
		*addrp = addr;
	if (sizep != NULL)
		*sizep = size;
	if (pcip != NULL)
		*pcip = pci;
}

int
OF_decode_addr(phandle_t dev, int regno, bus_space_tag_t *tag,
    bus_space_handle_t *handle)
{
	uint32_t cell[32];
	bus_addr_t addr, raddr, baddr;
	bus_size_t size, rsize;
	uint32_t c, nbridge, naddr, nsize;
	phandle_t bridge, parent;
	u_int spc, rspc;
	int pci, pcib, res;

	/* Sanity checking. */
	if (dev == 0)
		return (EINVAL);
	bridge = OF_parent(dev);
	if (bridge == 0)
		return (EINVAL);
	if (regno < 0)
		return (EINVAL);
	if (tag == NULL || handle == NULL)
		return (EINVAL);

	/* Get the requested register. */
	OF_get_addr_props(bridge, &naddr, &nsize, &pci);
	res = OF_getprop(dev, (pci) ? "assigned-addresses" : "reg",
	    cell, sizeof(cell));
	if (res == -1)
		return (ENXIO);
	if (res % sizeof(cell[0]))
		return (ENXIO);
	res /= sizeof(cell[0]);
	regno *= naddr + nsize;
	if (regno + naddr + nsize > res)
		return (EINVAL);
	spc = (pci) ? cell[regno] & OFW_PCI_PHYS_HI_SPACEMASK : ~0;
	addr = 0;
	for (c = 0; c < naddr; c++)
		addr = ((uint64_t)addr << 32) | cell[regno++];
	size = 0;
	for (c = 0; c < nsize; c++)
		size = ((uint64_t)size << 32) | cell[regno++];

	/*
	 * Map the address range in the bridge's decoding window as given
	 * by the "ranges" property. If a node doesn't have such property
	 * then no mapping is done.
	 */
	parent = OF_parent(bridge);
	while (parent != 0) {
		OF_get_addr_props(parent, &nbridge, NULL, &pcib);
		res = OF_getprop(bridge, "ranges", cell, sizeof(cell));
		if (res == -1)
			goto next;
		if (res % sizeof(cell[0]))
			return (ENXIO);
		res /= sizeof(cell[0]);
		regno = 0;
		while (regno < res) {
			rspc = (pci)
			    ? cell[regno] & OFW_PCI_PHYS_HI_SPACEMASK
			    : ~0;
			if (rspc != spc) {
				regno += naddr + nbridge + nsize;
				continue;
			}
			raddr = 0;
			for (c = 0; c < naddr; c++)
				raddr = ((uint64_t)raddr << 32) | cell[regno++];
			rspc = (pcib)
			    ? cell[regno] & OFW_PCI_PHYS_HI_SPACEMASK
			    : ~0;
			baddr = 0;
			for (c = 0; c < nbridge; c++)
				baddr = ((uint64_t)baddr << 32) | cell[regno++];
			rsize = 0;
			for (c = 0; c < nsize; c++)
				rsize = ((uint64_t)rsize << 32) | cell[regno++];
			if (addr < raddr || addr >= raddr + rsize)
				continue;
			addr = addr - raddr + baddr;
			if (rspc != ~0)
				spc = rspc;
		}

	next:
		bridge = parent;
		parent = OF_parent(bridge);
		OF_get_addr_props(bridge, &naddr, &nsize, &pci);
	}

	*tag = &bs_le_tag;
	return (bus_space_map(*tag, addr, size, 0, handle));
}

