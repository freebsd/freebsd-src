/*-
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stand.h>
#include <string.h>

#include <sys/param.h>
#include <sys/linker.h>
#include <machine/elf.h>

#ifdef EFI
#include <efi.h>
#include <efilib.h>
#include "loader_efi.h"
#else
#include "host_syscall.h"
#endif
#include <machine/metadata.h>

#include "bootstrap.h"
#include "efi.h"
#include "kboot.h"

#include "platform/acfreebsd.h"
#include "acconfig.h"
#define ACPI_SYSTEM_XFACE
#include "actypes.h"
#include "actbl.h"

#include "cache.h"

#ifndef EFI
#define LOADER_PAGE_SIZE PAGE_SIZE
#endif

#ifdef EFI
static EFI_GUID acpi_guid = ACPI_TABLE_GUID;
static EFI_GUID acpi20_guid = ACPI_20_TABLE_GUID;
#endif

static int elf64_exec(struct preloaded_file *amp);
static int elf64_obj_exec(struct preloaded_file *amp);

bool do_mem_map = false;

/* Usually provided by loader_efi.h -- maybe just delete? */
#ifndef EFI
int bi_load(char *args, vm_offset_t *modulep, vm_offset_t *kernendp,
    bool exit_bs);
#endif

static struct file_format arm64_elf = {
	elf64_loadfile,
	elf64_exec
};

struct file_format *file_formats[] = {
	&arm64_elf,
	NULL
};

#ifndef EFI
extern uintptr_t tramp;
extern uint32_t tramp_size;
extern uint32_t tramp_data_offset;

struct trampoline_data {
	uint64_t	entry;			//  0 (PA where kernel loaded)
	uint64_t	modulep;		//  8 module metadata
	uint64_t	memmap_src;		// 16 Linux-provided memory map PA
	uint64_t	memmap_dst;		// 24 Module data copy PA
	uint64_t	memmap_len;		// 32 Length to copy
};
#endif

static int
elf64_exec(struct preloaded_file *fp)
{
	vm_offset_t modulep, kernendp;
#ifdef EFI
	vm_offset_t		clean_addr;
	size_t			clean_size;
	void (*entry)(vm_offset_t);
#else
	vm_offset_t		trampolinebase;
	vm_offset_t		staging;
	void			*trampcode;
	uint64_t		*trampoline;
	struct trampoline_data	*trampoline_data;
	int			nseg;
	void			*kseg;
#endif
	struct file_metadata	*md;
	Elf_Ehdr		*ehdr;
	int			error;
#ifdef EFI
	ACPI_TABLE_RSDP *rsdp;
	char buf[24];
	int revision;

	/*
	 * Report the RSDP to the kernel. The old code used the 'hints' method
	 * to communite this to the kernel. However, while convenient, the
	 * 'hints' method is fragile and does not work when static hints are
	 * compiled into the kernel. Instead, move to setting different tunables
	 * that start with acpi. The old 'hints' can be removed before we branch
	 * for FreeBSD 15.
	 */
	rsdp = efi_get_table(&acpi20_guid);
	if (rsdp == NULL) {
		rsdp = efi_get_table(&acpi_guid);
	}
	if (rsdp != NULL) {
		sprintf(buf, "0x%016llx", (unsigned long long)rsdp);
		setenv("hint.acpi.0.rsdp", buf, 1);
		setenv("acpi.rsdp", buf, 1);
		revision = rsdp->Revision;
		if (revision == 0)
			revision = 1;
		sprintf(buf, "%d", revision);
		setenv("hint.acpi.0.revision", buf, 1);
		setenv("acpi.revision", buf, 1);
		strncpy(buf, rsdp->OemId, sizeof(rsdp->OemId));
		buf[sizeof(rsdp->OemId)] = '\0';
		setenv("hint.acpi.0.oem", buf, 1);
		setenv("acpi.oem", buf, 1);
		sprintf(buf, "0x%016x", rsdp->RsdtPhysicalAddress);
		setenv("hint.acpi.0.rsdt", buf, 1);
		setenv("acpi.rsdt", buf, 1);
		if (revision >= 2) {
			/* XXX extended checksum? */
			sprintf(buf, "0x%016llx",
			    (unsigned long long)rsdp->XsdtPhysicalAddress);
			setenv("hint.acpi.0.xsdt", buf, 1);
			setenv("acpi.xsdt", buf, 1);
			sprintf(buf, "%d", rsdp->Length);
			setenv("hint.acpi.0.xsdt_length", buf, 1);
			setenv("acpi.xsdt_length", buf, 1);
		}
	}
#else
	vm_offset_t rsdp;
	rsdp = acpi_rsdp();
	if (rsdp != 0) {
		char buf[24];

		printf("Found ACPI 2.0 at %#016lx\n", rsdp);
		sprintf(buf, "0x%016llx", (unsigned long long)rsdp);
		setenv("hint.acpi.0.rsdp", buf, 1); /* For 13.1R bootability */
		setenv("acpi.rsdp", buf, 1);
		/* Nobody uses the rest of that stuff */
	}


	// XXX Question: why not just use malloc?
	trampcode = host_getmem(LOADER_PAGE_SIZE);
	if (trampcode == NULL) {
		printf("Unable to allocate trampoline\n");
		return (ENOMEM);
	}
	bzero((void *)trampcode, LOADER_PAGE_SIZE);
	bcopy((void *)&tramp, (void *)trampcode, tramp_size);
	trampoline = (void *)trampcode;

	/*
	 * Figure out where to put it.
	 *
	 * Linux does not allow us to kexec_load into any part of memory. Find
	 * the first available chunk of physical memory where loading is
	 * possible (staging).
	 *
	 * The kernel is loaded at the 'base' address in continguous physical
	 * memory. We use the 2MB in front of the kernel as a place to put our
	 * trampoline, but that's really overkill since we only need ~100 bytes.
	 * The arm64 kernel's entry requirements are only 'load the kernel at a
	 * 2MB alignment' and it figures out the rest, creates the right page
	 * tables, etc.
	 */
	staging = kboot_get_phys_load_segment();
	printf("Load address at %#jx\n", (uintmax_t)staging);
	printf("Relocation offset is %#jx\n", (uintmax_t)elf64_relocation_offset);
#endif

	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
        	return(EFTYPE);

	ehdr = (Elf_Ehdr *)&(md->md_data);
#ifdef EFI
	entry = efi_translate(ehdr->e_entry);

	efi_time_fini();
#endif
	error = bi_load(fp->f_args, &modulep, &kernendp, true);
	if (error != 0) {
#ifdef EFI
		efi_time_init();
#endif
		return (error);
	}

	dev_cleanup();

#ifdef EFI
	/* Clean D-cache under kernel area and invalidate whole I-cache */
	clean_addr = (vm_offset_t)efi_translate(fp->f_addr);
	clean_size = (vm_offset_t)efi_translate(kernendp) - clean_addr;

	cpu_flush_dcache((void *)clean_addr, clean_size);
	cpu_inval_icache();

	(*entry)(modulep);

#else
	/* Linux will flush the caches, just pass this data into our trampoline and go */
	trampoline_data = (void *)trampoline + tramp_data_offset;
	memset(trampoline_data, 0, sizeof(*trampoline_data));
	trampoline_data->entry = ehdr->e_entry - fp->f_addr + staging;
	trampoline_data->modulep = modulep;
	printf("Modulep = %jx\n", (uintmax_t)modulep);
	if (efi_map_phys_src != 0) {
		md = file_findmetadata(fp, MODINFOMD_EFI_MAP);
		if (md == NULL || md->md_addr == 0) {
			printf("Need to copy EFI MAP, but EFI MAP not found. %p\n", md);
		} else {
			printf("Metadata EFI map loaded at VA %lx\n", md->md_addr);
			efi_map_phys_dst = md->md_addr + staging +
			    roundup2(sizeof(struct efi_map_header), 16) - fp->f_addr;
			trampoline_data->memmap_src = efi_map_phys_src;
			trampoline_data->memmap_dst = efi_map_phys_dst;
			trampoline_data->memmap_len = efi_map_size - roundup2(sizeof(struct efi_map_header), 16);
			printf("Copying UEFI Memory Map data from %#lx to %#lx %ld bytes\n",
			    efi_map_phys_src,
			    trampoline_data->memmap_dst,
			    trampoline_data->memmap_len);
		}
	}
	/*
	 * Copy the trampoline to the ksegs. Since we're just bouncing off of
	 * this into the kernel, no need to preserve the pages. On arm64, the
	 * kernel sets up the initial page table, so we don't have to preserve
	 * the memory used for the trampoline past when it calls the kernel.
	 */
	printf("kernendp = %#llx\n", (long long)kernendp);
	trampolinebase = staging + (kernendp - fp->f_addr);
	printf("trampolinebase = %#llx\n", (long long)trampolinebase);
	archsw.arch_copyin((void *)trampcode, kernendp, tramp_size);
	printf("Trampoline bouncing to %#llx\n", (long long)trampoline_data->entry);

	kboot_kseg_get(&nseg, &kseg);
	error = host_kexec_load(trampolinebase, nseg, kseg, HOST_KEXEC_ARCH_AARCH64);
	if (error != 0)
		panic("kexec_load returned error: %d", error);
	host_reboot(HOST_REBOOT_MAGIC1, HOST_REBOOT_MAGIC2, HOST_REBOOT_CMD_KEXEC, 0);
#endif

	panic("exec returned");
}

static int
elf64_obj_exec(struct preloaded_file *fp)
{

	printf("%s called for preloaded file %p (=%s):\n", __func__, fp,
	    fp->f_name);
	return (ENOSYS);
}
