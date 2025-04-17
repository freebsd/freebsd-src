/*-
 * Copyright (c) 2022 Netflix, Inc
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

#include <sys/param.h>
#include <machine/pc/bios.h>
#include <machine/metadata.h>

#include "stand.h"
#include "host_syscall.h"
#include "efi.h"
#include "kboot.h"
#include "bootstrap.h"

/*
 * Abbreviated x86 Linux struct boot_param for the so-called zero-page.
 * We have to use this to get systab and memmap since neither of those
 * are exposed in a sane way. We only define what we need and pad for
 * everything else to minimize cross-coupling.
 *
 * Transcribed in FreeBSD-ese from Linux's asm/bootparam.h for x86 as of
 * 6.15, but these details haven't changed in a long time.
 */

struct linux_efi_info {
	uint32_t efi_loader_signature;	/* 0x00 */
	uint32_t efi_systab;		/* 0x04 */
	uint32_t efi_memdesc_size;	/* 0x08 */
	uint32_t efi_memdesc_version;	/* 0x0c */
	uint32_t efi_memmap;		/* 0x10 */
	uint32_t efi_memmap_size;	/* 0x14 */
	uint32_t efi_systab_hi;		/* 0x18 */
	uint32_t efi_memmap_hi;		/* 0x1c */
} __packed;

struct linux_boot_params {
	uint8_t _pad1[0x1c0];				/* 0x000 */
	struct linux_efi_info efi_info;			/* 0x1c0 */
	uint8_t _pad2[0x1000 - 0x1c0 - sizeof(struct linux_efi_info)]; /* 0x1e0 */
} __packed;	/* Total size 4k, the page size on x86 */

bool
enumerate_memory_arch(void)
{
	struct linux_boot_params bp;

	/*
	 * Sadly, there's no properly exported data for the EFI memory map nor
	 * the system table. systab is passed in from the original boot loader.
	 * memmap is obtained from boot time services (which are long gone) and
	 * then modified and passed to SetVirtualAddressMap. Even though the
	 * latter is in runtime services, it can only be called once and Linux
	 * has already called it. So unless we can dig all this out from the
	 * Linux kernel, there's no other wy to get it. A proper way would be to
	 * publish these in /sys/firmware/efi, but that's not done yet. We can
	 * only get the runtime subset and can't get systbl at all from today's
	 * (6.15) Linux kernel. Linux's pandora boot loader will copy this same
	 * information when it calls the new kernel, but since we don't use the
	 * bzImage kexec vector, we have to harvest it here.
	 */
	if (data_from_kernel("boot_params", &bp, sizeof(bp))) {
		uint64_t systbl, memmap;

		systbl = (uint64_t)bp.efi_info.efi_systab_hi << 32 |
		    bp.efi_info.efi_systab;
		memmap = (uint64_t)bp.efi_info.efi_memmap_hi << 32 |
		    bp.efi_info.efi_memmap;

		efi_set_systbl(systbl);
		efi_read_from_pa(memmap, bp.efi_info.efi_memmap_size,
		    bp.efi_info.efi_memdesc_size, bp.efi_info.efi_memdesc_version);
		printf("UEFI SYSTAB PA: %#lx\n", systbl);
		printf("UEFI MMAP: Ver %d Ent Size %d Tot Size %d PA %#lx\n",
		    bp.efi_info.efi_memdesc_version, bp.efi_info.efi_memdesc_size,
		    bp.efi_info.efi_memmap_size, memmap);
	}
	/*
	 * So, we can't use the EFI map for this, so we have to fall back to
	 * the proc iomem stuff to at least get started...
	 */
	if (!populate_avail_from_iomem()) {
		printf("Populate from avail also failed.\n");
		return (false);
	} else {
		printf("Populate worked...\n");
	}
	print_avail();
	return (true);
}

/* XXX refactor with aarch64 */
uint64_t
kboot_get_phys_load_segment(void)
{
#define HOLE_SIZE	(64ul << 20)
#define KERN_ALIGN	(2ul << 20)
	static uint64_t	s = 0;

	if (s != 0)
		return (s);

	print_avail();
	s = first_avail(KERN_ALIGN, HOLE_SIZE, SYSTEM_RAM);
	printf("KBOOT GET PHYS Using %#llx\n", (long long)s);
	if (s != 0)
		return (s);
	s = 0x40000000 | 0x4200000;	/* should never get here */
	/* XXX PANIC? XXX */
	printf("Falling back to the crazy address %#lx which works in qemu\n", s);
	return (s);
}

void
bi_loadsmap(struct preloaded_file *kfp)
{
	efi_bi_loadsmap(kfp);
}
