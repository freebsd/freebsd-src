/*-
 * Copyright (c) 2019 Juniper Networks, Inc
 * Copyright (c) 2004 Olivier Houchard
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
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

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/linker.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/vmparam.h>

#ifdef FDT
#include <contrib/libfdt/libfdt.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_subr.h>
#include <dev/ofw/openfirm.h>
#include <machine/pte.h>
#include <machine/vm.h>
#include <sys/boot.h>
#endif

#ifdef FDT
#define PRELOAD_PUSH_VALUE(type, value) do {			\
	*(type *)(preload_ptr + preload_size) = (value);	\
	preload_size += sizeof(type);				\
} while (0)

#define PRELOAD_PUSH_STRING(str) do {				\
	uint32_t ssize;						\
	ssize = strlen(str) + 1;				\
	PRELOAD_PUSH_VALUE(uint32_t, ssize);			\
	strcpy((char*)(preload_ptr + preload_size), str);	\
	preload_size += ssize;					\
	preload_size = roundup(preload_size, sizeof(u_long));	\
} while (0)

static int build_l2_block_pagetable(vm_offset_t, uint64_t,
    struct arm64_bootparams *);

#define	INITRD_START	"linux,initrd-start"
#define	INITRD_END	"linux,initrd-end"
#define	KENV_SIZE	2048

static char	static_kenv[KENV_SIZE];
static caddr_t	metadata_endptr;
#endif

#define	PMAP_BOOTSTRAP_PAGES	2

extern vm_offset_t end;

/*
 * Fake up a boot descriptor table
 */
static vm_offset_t
fake_preload_metadata(void *dtb_ptr, size_t dtb_size,
    struct arm64_bootparams *abp)
{
	vm_offset_t	lastaddr;
	static char	fake_preload[256];
	caddr_t		preload_ptr;
	size_t		preload_size;

	preload_ptr = (caddr_t)&fake_preload[0];
	preload_size = 0;

	PRELOAD_PUSH_VALUE(uint32_t, MODINFO_NAME);
	PRELOAD_PUSH_STRING("kernel");

	PRELOAD_PUSH_VALUE(uint32_t, MODINFO_TYPE);
	PRELOAD_PUSH_STRING("elf kernel");

	PRELOAD_PUSH_VALUE(uint32_t, MODINFO_ADDR);
	PRELOAD_PUSH_VALUE(uint32_t, sizeof(vm_offset_t));
	PRELOAD_PUSH_VALUE(uint64_t, VM_MIN_KERNEL_ADDRESS);

	PRELOAD_PUSH_VALUE(uint32_t, MODINFO_SIZE);
	PRELOAD_PUSH_VALUE(uint32_t, sizeof(size_t));
	PRELOAD_PUSH_VALUE(uint64_t, (size_t)(&end - VM_MIN_KERNEL_ADDRESS));

	lastaddr = (vm_offset_t)&end;
	lastaddr = roundup(lastaddr, sizeof(vm_offset_t));

#ifdef FDT
	if (dtb_ptr != NULL &&
	    (build_l2_block_pagetable(lastaddr, dtb_size, abp) == 0)) {
		/* Copy DTB to KVA space and insert it into module chain. */
		PRELOAD_PUSH_VALUE(uint32_t, MODINFO_METADATA | MODINFOMD_DTBP);
		PRELOAD_PUSH_VALUE(uint32_t, sizeof(uint64_t));
		PRELOAD_PUSH_VALUE(uint64_t, (uint64_t)lastaddr);
		memmove((void *)lastaddr, dtb_ptr, dtb_size);
		lastaddr += dtb_size;
	}

	lastaddr = roundup(lastaddr, sizeof(vm_offset_t));
	/* End marker */
	metadata_endptr = preload_ptr;
#endif

	PRELOAD_PUSH_VALUE(uint32_t, 0);
	PRELOAD_PUSH_VALUE(uint32_t, 0);

	preload_metadata = fake_preload;

	return (lastaddr);
}

/*
 * Support booting from U-Boot's booti command. If modulep (register x0)
 * is a valid address then it is a pointer to FDT.
 */
vm_offset_t
linux_parse_boot_param(struct arm64_bootparams *abp)
{
	uint32_t		dtb_size = 0;
	struct fdt_header	*dtb_ptr = NULL;

#if defined(FDT) && !defined(FDT_DTB_STATIC)
	/* Test if modulep (x0) point to valid DTB. */
	dtb_ptr = (struct fdt_header *)abp->modulep;
	if (dtb_ptr && fdt_check_header(dtb_ptr) == 0)
		dtb_size = fdt_totalsize(dtb_ptr);
#endif
	return (fake_preload_metadata(dtb_ptr, dtb_size, abp));
}

#ifdef FDT
/*
 * Builds count 2 MiB page table entries.
 * During startup, locore.S maps kernel memory in L2 page table.
 * Create space to copy size bytes following the kernel memory.
 * See build_l2_block_pagetable in locore.S
 */
static int
build_l2_block_pagetable(vm_offset_t lastaddr, uint64_t size,
    struct arm64_bootparams *abp)
{
	vm_offset_t	l2_block_entry, *l2pt_entry;
	int32_t		count_2mib;
	volatile uint64_t	output_bits;

	/* Number of 2MiB pages */
	count_2mib = ((lastaddr - KERNBASE) + size) >> L2_SHIFT;

	/* All the memory must not cross a 1GiB boundary */
	if (count_2mib >= Ln_ENTRIES) {
		printf("%s: Adding %#lx bytes makes kernel cross 1GiB boundary\n",
		     __FUNCTION__, size);
		return EINVAL;
	}

	/* size fits within the last 2MiB page table entry */
	if (((lastaddr - KERNBASE) >> L2_SHIFT) == count_2mib)
		return 0;

	/* Build the L2 block entry */
	l2_block_entry = ATTR_IDX(VM_MEMATTR_WRITE_BACK) | L2_BLOCK | ATTR_AF;
#ifdef SMP
	l2_block_entry |= ATTR_SH(ATTR_SH_IS);
#endif
	/* Number of 2MiB pages mapped to kernel */
	count_2mib = (lastaddr - KERNBASE) >> L2_SHIFT;

	/* Go to last L2 page table entry. Each pagetable entry is 8 bytes */
	l2pt_entry = (vm_offset_t*)((abp->kern_l1pt - PAGE_SIZE) +
	    (count_2mib << 3));
	output_bits = (*l2pt_entry++ >> L2_SHIFT) + 1;

	/* Build count 2MiB page table entries */
	for (count_2mib = size >> L2_SHIFT; count_2mib >= 0;
	    l2pt_entry++, output_bits++, count_2mib--)
		*l2pt_entry = (output_bits << L2_SHIFT) | l2_block_entry;

	return 0;
}

/*
 * Align start addr to 1GiB boundary and build L1 page table entry for it.
 * See build_l1_block_pagetable in locore.S
 */
static void
build_l1_block_pagetable(vm_offset_t start, struct arm64_bootparams *abp)
{
	vm_offset_t l1_table_idx, l1_block_entry, phy_addr, *l1_table_entry;

	/* Find the table index */
	l1_table_idx = (start >> L1_SHIFT) & Ln_ADDR_MASK;

	/* Build the L1 block entry */
	l1_block_entry = ATTR_nG | ATTR_IDX(VM_MEMATTR_UNCACHEABLE) |
	    L1_BLOCK | ATTR_AF;
#ifdef SMP
	l1_block_entry |= ATTR_SH(ATTR_SH_IS);
#endif

	/* Set the physical address */
	phy_addr = l1_block_entry | (l1_table_idx << L1_SHIFT);

	/* Index of L1 pagetable. Each pagetable entry is 8 bytes */
	l1_table_entry  = (vm_offset_t*)((abp->kern_l0pt + PAGE_SIZE) +
	    (l1_table_idx << 3));
	*l1_table_entry = phy_addr;
}

/*
 * Copy the initrd image passed using U-Boot's booti command into
 * KVA space.
 */
static void
linux_load_initrd(vm_offset_t *lastaddr, struct arm64_bootparams *abp)
{
	phandle_t	chosen;
	uint64_t 	initrd_start = 0, initrd_end = 0;
	uint64_t 	initrd_size;
	caddr_t		preload_ptr;
	size_t		preload_size = 0;

	if ((chosen = OF_finddevice("/chosen")) == -1)
		return;

	if (!(OF_hasprop(chosen, INITRD_START) &&
	    OF_hasprop(chosen, INITRD_END)))
		return;

	if ((OF_getprop(chosen, INITRD_START, &initrd_start, sizeof(uint64_t))) > 0)
		initrd_start = fdt64_to_cpu(initrd_start);

	if ((OF_getprop(chosen, INITRD_END, &initrd_end, sizeof(uint64_t))) > 0)
		initrd_end = fdt64_to_cpu(initrd_end);

	if ((initrd_size = (initrd_end - initrd_start)) <= 0)
		return;

	if (build_l2_block_pagetable(*lastaddr, initrd_size, abp) != 0)
		return;

	build_l1_block_pagetable(initrd_start, abp);

	/* Copy the initrd image to virtual address space */
	memmove((void*)(*lastaddr), (void*)initrd_start, initrd_size);

	preload_ptr = metadata_endptr;

	PRELOAD_PUSH_VALUE(uint32_t, MODINFO_NAME);
	PRELOAD_PUSH_STRING("initrd");

	PRELOAD_PUSH_VALUE(uint32_t, MODINFO_TYPE);
	PRELOAD_PUSH_STRING("md_image");

	PRELOAD_PUSH_VALUE(uint32_t, MODINFO_SIZE);
	PRELOAD_PUSH_VALUE(uint32_t, sizeof(uint64_t));
	PRELOAD_PUSH_VALUE(uint64_t, initrd_size);

	PRELOAD_PUSH_VALUE(uint32_t, MODINFO_ADDR);
	PRELOAD_PUSH_VALUE(uint32_t, sizeof(vm_offset_t));
	PRELOAD_PUSH_VALUE(uint64_t, *lastaddr);

	*lastaddr += initrd_size;
	*lastaddr = roundup(*lastaddr, sizeof(vm_offset_t));

	/* End marker */
	metadata_endptr = preload_ptr;
	PRELOAD_PUSH_VALUE(uint32_t, 0);
	PRELOAD_PUSH_VALUE(uint32_t, 0);
}

/*
 * Parse initrd image arguments, bootargs passed in FDT from U-Boot.
 */
void
parse_bootargs(vm_offset_t *lastaddr, struct arm64_bootparams *abp)
{

	/*
	 * Fake metadata is used to support boot from U-Boot. Process bootargs,
	 * initrd args from FDT blob set in fake medadata.
	 */
	if (metadata_endptr == NULL)
		return;

	/* Booted from U-Boot */
	linux_load_initrd(lastaddr, abp);

	/*
	 * L2 PTEs map addresses in order of kernel, dtb, initrd image.
	 * Add L2 pages at the end for pmap to bootstrap L2, L3 PTEs, etc.
	 */
	build_l2_block_pagetable(*lastaddr,
	    (PMAP_BOOTSTRAP_PAGES * L2_SIZE) - 1, abp);

	init_static_kenv(static_kenv, sizeof(static_kenv));
	ofw_parse_bootargs();
}
#endif
