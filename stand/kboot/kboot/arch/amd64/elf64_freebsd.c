/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2014 The FreeBSD Foundation
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

#define __ELF_WORD_SIZE 64
#include <sys/param.h>
#include <sys/exec.h>
#include <sys/linker.h>
#include <string.h>
#include <machine/elf.h>
#include <stand.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#ifdef EFI
#include <efi.h>
#include <efilib.h>
#else
#include "host_syscall.h"
#endif

#include "bootstrap.h"
#include "kboot.h"
#include "efi.h"

#include "platform/acfreebsd.h"
#include "acconfig.h"
#define ACPI_SYSTEM_XFACE
#include "actypes.h"
#include "actbl.h"

#ifdef EFI
#include "loader_efi.h"

static EFI_GUID acpi_guid = ACPI_TABLE_GUID;
static EFI_GUID acpi20_guid = ACPI_20_TABLE_GUID;
#else
/* Usually provided by loader_efi.h */
extern int bi_load(char *args, vm_offset_t *modulep, vm_offset_t *kernendp,
    bool exit_bs);
#endif

#ifdef EFI
#define LOADER_PAGE_SIZE EFI_PAGE_SIZE
#else
#define LOADER_PAGE_SIZE PAGE_SIZE
#endif

static int	elf64_exec(struct preloaded_file *amp);
static int	elf64_obj_exec(struct preloaded_file *amp);

static struct file_format amd64_elf = {
	.l_load = elf64_loadfile,
	.l_exec = elf64_exec,
};
static struct file_format amd64_elf_obj = {
	.l_load = elf64_obj_loadfile,
	.l_exec = elf64_obj_exec,
};

#ifdef EFI
extern struct file_format multiboot2;
extern struct file_format multiboot2_obj;
#endif

struct file_format *file_formats[] = {
#ifdef EFI
	&multiboot2,
	&multiboot2_obj,
#endif
	&amd64_elf,
	&amd64_elf_obj,
	NULL
};

#ifndef	EFI
/*
 * We create the stack that we want. We store any memory map table that we have
 * top copy (the metadata has already been filled in). We pop these args off and
 * copy if neeed be. Then, we have the address of the page tables we make on top
 * (so we pop that off and set %cr3). We have the entry point to the kernel
 * (which retq pops off) This leaves the stack that the btext wants: offset 4 is
 * modulep and offset8 is kernend, with the filler bytes to keep this
 * aligned. This also makes the trampoline very simple: pop some args, maybe copy
 * pop the page table and then return into btext as defined in the kernel.
 */
struct trampoline_data {
	uint64_t	memmap_src;		// Linux-provided memory map PA
	uint64_t	memmap_dst;		// Module data copy PA
	uint64_t	memmap_len;		// Length to copy
	uint64_t	pt4;			// Page table address to pop
	uint64_t	entry;			// return address to jump to kernel
	uint32_t	fill1;			// 0
	uint32_t	modulep;		// 4 module metadata
	uint32_t	kernend;		// 8 kernel end
	uint32_t	fill2;			// 12
};
_Static_assert(sizeof(struct trampoline_data) == 56, "Bad size for trampoline data");
#endif

static pml4_entry_t *PT4;
static pdp_entry_t *PT3_l, *PT3_u;
static pd_entry_t *PT2_l0, *PT2_l1, *PT2_l2, *PT2_l3, *PT2_u0, *PT2_u1;

#ifdef EFI
static pdp_entry_t *PT3;
static pd_entry_t *PT2;

static void (*trampoline)(uint64_t stack, void *copy_finish, uint64_t kernend,
    uint64_t modulep, pml4_entry_t *pagetable, uint64_t entry);
#endif

extern uintptr_t tramp;
extern uint32_t tramp_size;
#ifndef EFI
extern uint32_t tramp_data_offset;
#endif

/*
 * There is an ELF kernel and one or more ELF modules loaded.
 * We wish to start executing the kernel image, so make such
 * preparations as are required, and do so.
 */
static int
elf64_exec(struct preloaded_file *fp)
{
	struct file_metadata	*md;
	Elf_Ehdr 		*ehdr;
	vm_offset_t		modulep, kernend;
	int			err, i;
	char			buf[24];
#ifdef EFI
	ACPI_TABLE_RSDP		*rsdp = NULL;
	int			revision;
	int			copy_auto;
	vm_offset_t		trampstack, trampcode;
#else
	vm_offset_t		rsdp = 0;
	void			*trampcode;
	int			nseg;
	void			*kseg;
	vm_offset_t		trampolinebase;
	uint64_t		*trampoline;
	struct trampoline_data	*trampoline_data;
	vm_offset_t		staging;
	int			error;
#endif

#ifdef EFI
	copy_auto = copy_staging == COPY_STAGING_AUTO;
	if (copy_auto)
		copy_staging = fp->f_kernphys_relocatable ?
		    COPY_STAGING_DISABLE : COPY_STAGING_ENABLE;
#else
	/*
	 * Figure out where to put it.
	 *
	 * Linux does not allow us to do kexec_load into any part of memory. Ask
	 * kboot_get_phys_load_segment to resolve the first available chunk of
	 * physical memory where loading is possible (staging).
	 *
	 * The kernel is loaded at the 'base' address in continguous physical
	 * pages (using 2MB super pages). The first such page is unused by the
	 * kernel and serves as a good place to put not only the trampoline, but
	 * the page table pages that the trampoline needs to setup the proper
	 * kernel starting environment.
	 */
	staging = trampolinebase = kboot_get_phys_load_segment();
	trampolinebase += 1ULL << 20;	/* Copy trampoline to base + 1MB, kernel will wind up at 2MB */
	printf("Load address at %#jx\n", (uintmax_t)trampolinebase);
	printf("Relocation offset is %#jx\n", (uintmax_t)elf64_relocation_offset);
#endif

	/*
	 * Report the RSDP to the kernel. While this can be found with
	 * a BIOS boot, the RSDP may be elsewhere when booted from UEFI.
	 */
#ifdef EFI
	rsdp = efi_get_table(&acpi20_guid);
	if (rsdp == NULL) {
		rsdp = efi_get_table(&acpi_guid);
	}
#else
	rsdp = acpi_rsdp();
#endif
	if (rsdp != 0) {
		sprintf(buf, "0x%016llx", (unsigned long long)rsdp);
		setenv("acpi.rsdp", buf, 1);
	}
	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
		return (EFTYPE);
	ehdr = (Elf_Ehdr *)&(md->md_data);

#ifdef EFI
	trampcode = copy_staging == COPY_STAGING_ENABLE ?
	    (vm_offset_t)G(1) : (vm_offset_t)G(4);
	err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 1,
	    (EFI_PHYSICAL_ADDRESS *)&trampcode);
	if (EFI_ERROR(err)) {
		printf("Unable to allocate trampoline\n");
		if (copy_auto)
			copy_staging = COPY_STAGING_AUTO;
		return (ENOMEM);
	}
	trampstack = trampcode + LOADER_PAGE_SIZE - 8;
#else
	// XXX Question: why not just use malloc?
	trampcode = host_getmem(LOADER_PAGE_SIZE);
	if (trampcode == NULL) {
		printf("Unable to allocate trampoline\n");
		return (ENOMEM);
	}
#endif
	bzero((void *)trampcode, LOADER_PAGE_SIZE);
	bcopy((void *)&tramp, (void *)trampcode, tramp_size);
	trampoline = (void *)trampcode;

#ifdef EFI
	if (copy_staging == COPY_STAGING_ENABLE) {
		PT4 = (pml4_entry_t *)G(1);
		err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 3,
		    (EFI_PHYSICAL_ADDRESS *)&PT4);
		if (EFI_ERROR(err)) {
			printf("Unable to allocate trampoline page table\n");
			BS->FreePages(trampcode, 1);
			if (copy_auto)
				copy_staging = COPY_STAGING_AUTO;
			return (ENOMEM);
		}
		bzero(PT4, 3 * LOADER_PAGE_SIZE);
		PT3 = &PT4[512];
		PT2 = &PT3[512];

		/*
		 * This is kinda brutal, but every single 1GB VM
		 * memory segment points to the same first 1GB of
		 * physical memory.  But it is more than adequate.
		 */
		for (i = 0; i < NPTEPG; i++) {
			/*
			 * Each slot of the L4 pages points to the
			 * same L3 page.
			 */
			PT4[i] = (pml4_entry_t)PT3;
			PT4[i] |= PG_V | PG_RW;

			/*
			 * Each slot of the L3 pages points to the
			 * same L2 page.
			 */
			PT3[i] = (pdp_entry_t)PT2;
			PT3[i] |= PG_V | PG_RW;

			/*
			 * The L2 page slots are mapped with 2MB pages for 1GB.
			 */
			PT2[i] = (pd_entry_t)i * M(2);
			PT2[i] |= PG_V | PG_RW | PG_PS;
		}
	} else {
		PT4 = (pml4_entry_t *)G(4);
		err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 9,
		    (EFI_PHYSICAL_ADDRESS *)&PT4);
		if (EFI_ERROR(err)) {
			printf("Unable to allocate trampoline page table\n");
			BS->FreePages(trampcode, 9);
			if (copy_auto)
				copy_staging = COPY_STAGING_AUTO;
			return (ENOMEM);
		}
		bzero(PT4, 9 * LOADER_PAGE_SIZE);

		PT3_l = &PT4[NPML4EPG * 1];
		PT3_u = &PT4[NPML4EPG * 2];
		PT2_l0 = &PT4[NPML4EPG * 3];
		PT2_l1 = &PT4[NPML4EPG * 4];
		PT2_l2 = &PT4[NPML4EPG * 5];
		PT2_l3 = &PT4[NPML4EPG * 6];
		PT2_u0 = &PT4[NPML4EPG * 7];
		PT2_u1 = &PT4[NPML4EPG * 8];

		/* 1:1 mapping of lower 4G */
		PT4[0] = (pml4_entry_t)PT3_l | PG_V | PG_RW;
		PT3_l[0] = (pdp_entry_t)PT2_l0 | PG_V | PG_RW;
		PT3_l[1] = (pdp_entry_t)PT2_l1 | PG_V | PG_RW;
		PT3_l[2] = (pdp_entry_t)PT2_l2 | PG_V | PG_RW;
		PT3_l[3] = (pdp_entry_t)PT2_l3 | PG_V | PG_RW;
		for (i = 0; i < 4 * NPDEPG; i++) {
			PT2_l0[i] = ((pd_entry_t)i << PDRSHIFT) | PG_V |
			    PG_RW | PG_PS;
		}

		/* mapping of kernel 2G below top */
		PT4[NPML4EPG - 1] = (pml4_entry_t)PT3_u | PG_V | PG_RW;
		PT3_u[NPDPEPG - 2] = (pdp_entry_t)PT2_u0 | PG_V | PG_RW;
		PT3_u[NPDPEPG - 1] = (pdp_entry_t)PT2_u1 | PG_V | PG_RW;
		/* compat mapping of phys @0 */
		PT2_u0[0] = PG_PS | PG_V | PG_RW;
		/* this maps past staging area */
		for (i = 1; i < 2 * NPDEPG; i++) {
			PT2_u0[i] = ((pd_entry_t)staging +
			    ((pd_entry_t)i - 1) * NBPDR) |
			    PG_V | PG_RW | PG_PS;
		}
	}
#else
	{
		vm_offset_t pabase, pa_pt3_l, pa_pt3_u, pa_pt2_l0, pa_pt2_l1, pa_pt2_l2, pa_pt2_l3, pa_pt2_u0, pa_pt2_u1;

		/* We'll find a place for these later */
		PT4 = (pml4_entry_t *)host_getmem(9 * LOADER_PAGE_SIZE);
		bzero(PT4, 9 * LOADER_PAGE_SIZE);

		PT3_l = &PT4[NPML4EPG * 1];
		PT3_u = &PT4[NPML4EPG * 2];
		PT2_l0 = &PT4[NPML4EPG * 3];
		PT2_l1 = &PT4[NPML4EPG * 4];
		PT2_l2 = &PT4[NPML4EPG * 5];
		PT2_l3 = &PT4[NPML4EPG * 6];
		PT2_u0 = &PT4[NPML4EPG * 7];
		PT2_u1 = &PT4[NPML4EPG * 8];

		pabase = trampolinebase + LOADER_PAGE_SIZE;
		pa_pt3_l = pabase + LOADER_PAGE_SIZE * 1;
		pa_pt3_u = pabase + LOADER_PAGE_SIZE * 2;
		pa_pt2_l0 = pabase + LOADER_PAGE_SIZE * 3;
		pa_pt2_l1 = pabase + LOADER_PAGE_SIZE * 4;
		pa_pt2_l2 = pabase + LOADER_PAGE_SIZE * 5;
		pa_pt2_l3 = pabase + LOADER_PAGE_SIZE * 6;
		pa_pt2_u0 = pabase + LOADER_PAGE_SIZE * 7;
		pa_pt2_u1 = pabase + LOADER_PAGE_SIZE * 8;

		/* 1:1 mapping of lower 4G */
		PT4[0] = (pml4_entry_t)pa_pt3_l | PG_V | PG_RW;
		PT3_l[0] = (pdp_entry_t)pa_pt2_l0 | PG_V | PG_RW;
		PT3_l[1] = (pdp_entry_t)pa_pt2_l1 | PG_V | PG_RW;
		PT3_l[2] = (pdp_entry_t)pa_pt2_l2 | PG_V | PG_RW;
		PT3_l[3] = (pdp_entry_t)pa_pt2_l3 | PG_V | PG_RW;
		for (i = 0; i < 4 * NPDEPG; i++) {	/* we overflow PT2_l0 into _l1, etc */
			PT2_l0[i] = ((pd_entry_t)i << PDRSHIFT) | PG_V |
			    PG_RW | PG_PS;
		}

		/* mapping of kernel 2G below top */
		PT4[NPML4EPG - 1] = (pml4_entry_t)pa_pt3_u | PG_V | PG_RW;
		PT3_u[NPDPEPG - 2] = (pdp_entry_t)pa_pt2_u0 | PG_V | PG_RW;
		PT3_u[NPDPEPG - 1] = (pdp_entry_t)pa_pt2_u1 | PG_V | PG_RW;
		/* compat mapping of phys @0 */
		PT2_u0[0] = PG_PS | PG_V | PG_RW;
		/* this maps past staging area */
		/*
		 * Kernel uses the KERNSTART (== KERNBASE + 2MB) entry to figure
		 * out where we loaded the kernel. This is PT2_u0[1] (since
		 * these map 2MB pages. So the PA that this maps has to be
		 * kboot's staging + 2MB.  For UEFI we do 'i - 1' since we load
		 * the kernel right at staging (and assume the first address we
		 * load is 2MB in efi_copyin). However for kboot, staging + 1 *
		 * NBPDR == staging + 2MB which is where the kernel starts. Our
		 * trampoline need not be mapped into the kernel space since we
		 * execute PA==VA for that, and the trampoline can just go away
		 * once the kernel is called.
		 *
		 * Staging should likely be as low as possible, though, because
		 * all the 'early' allocations are at kernend (which the kernel
		 * calls physfree).
		 */
		for (i = 1; i < 2 * NPDEPG; i++) {	/* we overflow PT2_u0 into _u1 */
			PT2_u0[i] = ((pd_entry_t)staging +
			    ((pd_entry_t)i) * NBPDR) |
			    PG_V | PG_RW | PG_PS;
			if (i < 10) printf("Mapping %d to %#lx staging %#lx\n", i, PT2_u0[i], staging);
		}
	}
#endif

#ifdef EFI
	printf("staging %#lx (%scopying) tramp %p PT4 %p\n",
	    staging, copy_staging == COPY_STAGING_ENABLE ? "" : "not ",
	    trampoline, PT4);
#else
	printf("staging %#lx tramp %p PT4 %p\n", staging, (void *)trampolinebase,
	    (void *)trampolinebase + LOADER_PAGE_SIZE);
#endif
	printf("Start @ 0x%lx ...\n", ehdr->e_entry);

#ifdef EFI
	efi_time_fini();
#endif
	err = bi_load(fp->f_args, &modulep, &kernend, true);
	if (err != 0) {
#ifdef EFI
		efi_time_init();
		if (copy_auto)
			copy_staging = COPY_STAGING_AUTO;
#endif
		return (err);
	}

	dev_cleanup();

#ifdef EFI
	trampoline(trampstack, copy_staging == COPY_STAGING_ENABLE ?
	    efi_copy_finish : efi_copy_finish_nop, kernend, modulep,
	    PT4, ehdr->e_entry);
#else
	trampoline_data = (void *)trampoline + tramp_data_offset;
	trampoline_data->entry = ehdr->e_entry;	/* VA since we start MMU with KERNBASE, etc  */
	if (efi_map_phys_src != 0) {
		md = file_findmetadata(fp, MODINFOMD_EFI_MAP);
		if (md == NULL || md->md_addr == 0) {
			printf("Need to copy EFI MAP, but EFI MAP not found. %p\n", md);
		} else {
			printf("Metadata EFI map loaded at VA %lx\n", md->md_addr);
			efi_map_phys_dst = md->md_addr + staging +	/* md_addr is taging relative */
			    roundup2(sizeof(struct efi_map_header), 16); /* Skip header */
			trampoline_data->memmap_src = efi_map_phys_src;
			trampoline_data->memmap_dst = efi_map_phys_dst;
			trampoline_data->memmap_len = efi_map_size - roundup2(sizeof(struct efi_map_header), 16);
			printf("Copying UEFI Memory Map data from %#lx to %#lx %ld bytes\n",
			    trampoline_data->memmap_src,
			    trampoline_data->memmap_dst,
			    trampoline_data->memmap_len);
		}
	}
	/*
	 * So we compute the VA of the module data by modulep + KERNBASE....
	 * need to make sure that that address is mapped right. We calculate
	 * the start of available memory to allocate via kernend (which is
	 * calculated with a phyaddr of "kernend + PA(PT_u0[1])"), so we better
	 * make sure we're not overwriting the last 2MB of the kernel :).
	 */
	trampoline_data->pt4 = trampolinebase + LOADER_PAGE_SIZE;
	trampoline_data->modulep = modulep;	/* Offset from KERNBASE */
	trampoline_data->kernend = kernend;	/* Offset from the load address */
	trampoline_data->fill1 = trampoline_data->fill2 = 0;
	printf("Modulep = %lx kernend %lx\n", modulep, kernend);
	/* NOTE: when copyting in, it's relative to the start of our 'area' not an abs addr */
	/* Copy the trampoline to the ksegs */
	archsw.arch_copyin((void *)trampcode, trampolinebase - staging, tramp_size);
	/* Copy the page table to the ksegs */
	archsw.arch_copyin(PT4, trampoline_data->pt4 - staging, 9 * LOADER_PAGE_SIZE);

	kboot_kseg_get(&nseg, &kseg);
	error = host_kexec_load(trampolinebase, nseg, kseg, HOST_KEXEC_ARCH_X86_64);
	if (error != 0)
		panic("kexec_load returned error: %d", error);
	host_reboot(HOST_REBOOT_MAGIC1, HOST_REBOOT_MAGIC2, HOST_REBOOT_CMD_KEXEC, 0);
#endif

	panic("exec returned");
}

static int
elf64_obj_exec(struct preloaded_file *fp)
{

	return (EFTYPE);
}
