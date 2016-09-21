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

#include "opt_platform.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/smp.h>
#include <sys/stat.h>
#include <sys/endian.h>

#include <net/ethernet.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_subr.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/platform.h>
#include <machine/ofw_machdep.h>
#include <machine/trap.h>

static void	*fdt;
int		ofw_real_mode;

#ifdef AIM
extern register_t ofmsr[5];
extern void	*openfirmware_entry;
char		save_trap_init[0x2f00];          /* EXC_LAST */
char		save_trap_of[0x2f00];            /* EXC_LAST */

int		ofwcall(void *);
static int	openfirmware(void *args);

__inline void
ofw_save_trap_vec(char *save_trap_vec)
{
	if (!ofw_real_mode)
                return;

	bcopy((void *)EXC_RST, save_trap_vec, EXC_LAST - EXC_RST);
}

static __inline void
ofw_restore_trap_vec(char *restore_trap_vec)
{
	if (!ofw_real_mode)
                return;

	bcopy(restore_trap_vec, (void *)EXC_RST, EXC_LAST - EXC_RST);
	__syncicache(EXC_RSVD, EXC_LAST - EXC_RSVD);
}

/*
 * Saved SPRG0-3 from OpenFirmware. Will be restored prior to the callback.
 */
#ifndef __powerpc64__
register_t	ofw_sprg0_save;

static __inline void
ofw_sprg_prepare(void)
{
	if (ofw_real_mode)
		return;
	
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
	if (ofw_real_mode)
		return;
	
	/*
	 * Note that SPRG1-3 contents are irrelevant. They are scratch
	 * registers used in the early portion of trap handling when
	 * interrupts are disabled.
	 *
	 * PCPU data cannot be used until this routine is called !
	 */
	__asm __volatile("mtsprg0 %0" :: "r"(ofw_sprg0_save));
}
#endif

#endif

static int
parse_ofw_memory(phandle_t node, const char *prop, struct mem_region *output)
{
	cell_t address_cells, size_cells;
	cell_t OFmem[4 * PHYS_AVAIL_SZ];
	int sz, i, j;
	phandle_t phandle;

	sz = 0;

	/*
	 * Get #address-cells from root node, defaulting to 1 if it cannot
	 * be found.
	 */
	phandle = OF_finddevice("/");
	if (OF_getencprop(phandle, "#address-cells", &address_cells, 
	    sizeof(address_cells)) < (ssize_t)sizeof(address_cells))
		address_cells = 1;
	if (OF_getencprop(phandle, "#size-cells", &size_cells, 
	    sizeof(size_cells)) < (ssize_t)sizeof(size_cells))
		size_cells = 1;

	/*
	 * Get memory.
	 */
	if (node == -1 || (sz = OF_getencprop(node, prop,
	    OFmem, sizeof(OFmem))) <= 0)
		panic("Physical memory map not found");

	i = 0;
	j = 0;
	while (i < sz/sizeof(cell_t)) {
	      #if !defined(__powerpc64__) && !defined(BOOKE)
		/* On 32-bit PPC (OEA), ignore regions starting above 4 GB */
		if (address_cells > 1 && OFmem[i] > 0) {
			i += address_cells + size_cells;
			continue;
		}
	      #endif

		output[j].mr_start = OFmem[i++];
		if (address_cells == 2) {
			output[j].mr_start <<= 32;
			output[j].mr_start += OFmem[i++];
		}
			
		output[j].mr_size = OFmem[i++];
		if (size_cells == 2) {
			output[j].mr_size <<= 32;
			output[j].mr_size += OFmem[i++];
		}

	      #if !defined(__powerpc64__) && !defined(BOOKE)
		/* Book-E can support 36-bit addresses. */
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

	return (sz);
}

static int
excise_fdt_reserved(struct mem_region *avail, int asz)
{
	struct {
		uint64_t address;
		uint64_t size;
	} fdtmap[16];
	ssize_t fdtmapsize;
	phandle_t chosen;
	int i, j, k;

	chosen = OF_finddevice("/chosen");
	fdtmapsize = OF_getprop(chosen, "fdtmemreserv", fdtmap, sizeof(fdtmap));

	for (j = 0; j < fdtmapsize/sizeof(fdtmap[0]); j++) {
		fdtmap[j].address = be64toh(fdtmap[j].address);
		fdtmap[j].size = be64toh(fdtmap[j].size);
	}

	for (i = 0; i < asz; i++) {
		for (j = 0; j < fdtmapsize/sizeof(fdtmap[0]); j++) {
			/*
			 * Case 1: Exclusion region encloses complete
			 * available entry. Drop it and move on.
			 */
			if (fdtmap[j].address <= avail[i].mr_start &&
			    fdtmap[j].address + fdtmap[j].size >=
			    avail[i].mr_start + avail[i].mr_size) {
				for (k = i+1; k < asz; k++)
					avail[k-1] = avail[k];
				asz--;
				i--; /* Repeat some entries */
				continue;
			}

			/*
			 * Case 2: Exclusion region starts in available entry.
			 * Trim it to where the entry begins and append
			 * a new available entry with the region after
			 * the excluded region, if any.
			 */
			if (fdtmap[j].address >= avail[i].mr_start &&
			    fdtmap[j].address < avail[i].mr_start +
			    avail[i].mr_size) {
				if (fdtmap[j].address + fdtmap[j].size < 
				    avail[i].mr_start + avail[i].mr_size) {
					avail[asz].mr_start =
					    fdtmap[j].address + fdtmap[j].size;
					avail[asz].mr_size = avail[i].mr_start +
					     avail[i].mr_size -
					     avail[asz].mr_start;
					asz++;
				}

				avail[i].mr_size = fdtmap[j].address -
				    avail[i].mr_start;
			}

			/*
			 * Case 3: Exclusion region ends in available entry.
			 * Move start point to where the exclusion zone ends.
			 * The case of a contained exclusion zone has already
			 * been caught in case 2.
			 */
			if (fdtmap[j].address + fdtmap[j].size >=
			    avail[i].mr_start && fdtmap[j].address +
			    fdtmap[j].size < avail[i].mr_start +
			    avail[i].mr_size) {
				avail[i].mr_size += avail[i].mr_start;
				avail[i].mr_start =
				    fdtmap[j].address + fdtmap[j].size;
				avail[i].mr_size -= avail[i].mr_start;
			}
		}
	}

	return (asz);
}

/*
 * This is called during powerpc_init, before the system is really initialized.
 * It shall provide the total and the available regions of RAM.
 * The available regions need not take the kernel into account.
 */
void
ofw_mem_regions(struct mem_region *memp, int *memsz,
		struct mem_region *availp, int *availsz)
{
	phandle_t phandle;
	int asz, msz;
	int res;
	char name[31];

	asz = msz = 0;

	/*
	 * Get memory from all the /memory nodes.
	 */
	for (phandle = OF_child(OF_peer(0)); phandle != 0;
	    phandle = OF_peer(phandle)) {
		if (OF_getprop(phandle, "name", name, sizeof(name)) <= 0)
			continue;
		if (strncmp(name, "memory", sizeof(name)) != 0 &&
		    strncmp(name, "memory@", strlen("memory@")) != 0)
			continue;

		res = parse_ofw_memory(phandle, "reg", &memp[msz]);
		msz += res/sizeof(struct mem_region);
		if (OF_getproplen(phandle, "available") >= 0)
			res = parse_ofw_memory(phandle, "available",
			    &availp[asz]);
		else
			res = parse_ofw_memory(phandle, "reg", &availp[asz]);
		asz += res/sizeof(struct mem_region);
	}

	phandle = OF_finddevice("/chosen");
	if (OF_hasprop(phandle, "fdtmemreserv"))
		asz = excise_fdt_reserved(availp, asz);

	*memsz = msz;
	*availsz = asz;
}

void
OF_initial_setup(void *fdt_ptr, void *junk, int (*openfirm)(void *))
{
#ifdef AIM
	ofmsr[0] = mfmsr();
	#ifdef __powerpc64__
	ofmsr[0] &= ~PSL_SF;
	#else
	__asm __volatile("mfsprg0 %0" : "=&r"(ofmsr[1]));
	__asm __volatile("mfsprg1 %0" : "=&r"(ofmsr[2]));
	__asm __volatile("mfsprg2 %0" : "=&r"(ofmsr[3]));
	__asm __volatile("mfsprg3 %0" : "=&r"(ofmsr[4]));
	#endif
	openfirmware_entry = openfirm;

	if (ofmsr[0] & PSL_DR)
		ofw_real_mode = 0;
	else
		ofw_real_mode = 1;

	ofw_save_trap_vec(save_trap_init);
#else
	ofw_real_mode = 1;
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

#ifdef AIM
	if (openfirmware_entry != NULL) {
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
	} else
#endif
	if (fdt != NULL) {
		status = OF_install(OFW_FDT, 0);

		if (status != TRUE)
			return status;

		OF_init(fdt);
		OF_interpret("perform-fixup", 0);
	} 

	return (status);
}

#ifdef AIM
void
ofw_quiesce(void)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
	} args;

	KASSERT(!pmap_bootstrapped, ("Cannot call ofw_quiesce after VM is up"));

	args.name = (cell_t)(uintptr_t)"quiesce";
	args.nargs = 0;
	args.nreturns = 0;
	openfirmware(&args);
}

static int
openfirmware_core(void *args)
{
	int		result;
	register_t	oldmsr;

	if (openfirmware_entry == NULL)
		return (-1);

	/*
	 * Turn off exceptions - we really don't want to end up
	 * anywhere unexpected with PCPU set to something strange
	 * or the stack pointer wrong.
	 */
	oldmsr = intr_disable();

#ifndef __powerpc64__
	ofw_sprg_prepare();
#endif

	/* Save trap vectors */
	ofw_save_trap_vec(save_trap_of);

	/* Restore initially saved trap vectors */
	ofw_restore_trap_vec(save_trap_init);

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

	/* Restore trap vecotrs */
	ofw_restore_trap_vec(save_trap_of);

#ifndef __powerpc64__
	ofw_sprg_restore();
#endif

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

	if (openfirmware_entry == NULL)
		return (-1);

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

#endif /* AIM */

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
int
OF_decode_addr(phandle_t dev, int regno, bus_space_tag_t *tag,
    bus_space_handle_t *handle, bus_size_t *sz)
{
	bus_addr_t addr;
	bus_size_t size;
	pcell_t pci_hi;
	int flags, res;

	res = ofw_reg_to_paddr(dev, regno, &addr, &size, &pci_hi);
	if (res < 0)
		return (res);

	if (pci_hi == OFW_PADDR_NOT_PCI) {
		*tag = &bs_be_tag;
		flags = 0;
	} else {
		*tag = &bs_le_tag;
		flags = (pci_hi & OFW_PCI_PHYS_HI_PREFETCHABLE) ? 
		    BUS_SPACE_MAP_PREFETCHABLE: 0;
	}

	if (sz != NULL)
		*sz = size;

	return (bus_space_map(*tag, addr, size, flags, handle));
}

