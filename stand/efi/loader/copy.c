/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Benno Rice under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <stand.h>
#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>

#include "loader_efi.h"

#define	M(x)	((x) * 1024 * 1024)
#define	G(x)	(1UL * (x) * 1024 * 1024 * 1024)

#if defined(__i386__) || defined(__amd64__)
#include <machine/cpufunc.h>
#include <machine/specialreg.h>
#include <machine/vmparam.h>

/*
 * The code is excerpted from sys/x86/x86/identcpu.c: identify_cpu(),
 * identify_hypervisor(), and dev/hyperv/vmbus/hyperv.c: hyperv_identify().
 */
#define CPUID_LEAF_HV_MAXLEAF		0x40000000
#define CPUID_LEAF_HV_INTERFACE		0x40000001
#define CPUID_LEAF_HV_FEATURES		0x40000003
#define CPUID_LEAF_HV_LIMITS		0x40000005
#define CPUID_HV_IFACE_HYPERV		0x31237648	/* HV#1 */
#define CPUID_HV_MSR_TIME_REFCNT	0x0002	/* MSR_HV_TIME_REF_COUNT */
#define CPUID_HV_MSR_HYPERCALL		0x0020

static int
running_on_hyperv(void)
{
	char hv_vendor[16];
	uint32_t regs[4];

	do_cpuid(1, regs);
	if ((regs[2] & CPUID2_HV) == 0)
		return (0);

	do_cpuid(CPUID_LEAF_HV_MAXLEAF, regs);
	if (regs[0] < CPUID_LEAF_HV_LIMITS)
		return (0);

	((uint32_t *)&hv_vendor)[0] = regs[1];
	((uint32_t *)&hv_vendor)[1] = regs[2];
	((uint32_t *)&hv_vendor)[2] = regs[3];
	hv_vendor[12] = '\0';
	if (strcmp(hv_vendor, "Microsoft Hv") != 0)
		return (0);

	do_cpuid(CPUID_LEAF_HV_INTERFACE, regs);
	if (regs[0] != CPUID_HV_IFACE_HYPERV)
		return (0);

	do_cpuid(CPUID_LEAF_HV_FEATURES, regs);
	if ((regs[0] & CPUID_HV_MSR_HYPERCALL) == 0)
		return (0);
	if ((regs[0] & CPUID_HV_MSR_TIME_REFCNT) == 0)
		return (0);

	return (1);
}

static void
efi_verify_staging_size(unsigned long *nr_pages)
{
	UINTN sz;
	EFI_MEMORY_DESCRIPTOR *map = NULL, *p;
	EFI_PHYSICAL_ADDRESS start, end;
	UINTN key, dsz;
	UINT32 dver;
	EFI_STATUS status;
	int i, ndesc;
	unsigned long available_pages = 0;

	sz = 0;

	for (;;) {
		status = BS->GetMemoryMap(&sz, map, &key, &dsz, &dver);
		if (!EFI_ERROR(status))
			break;

		if (status != EFI_BUFFER_TOO_SMALL) {
			printf("Can't read memory map: %lu\n",
			    EFI_ERROR_CODE(status));
			goto out;
		}

		free(map);

		/* Allocate 10 descriptors more than the size reported,
		 * to allow for any fragmentation caused by calling
		 * malloc */
		map = malloc(sz + (10 * dsz));
		if (map == NULL) {
			printf("Unable to allocate memory\n");
			goto out;
		}
	}

	ndesc = sz / dsz;
	for (i = 0, p = map; i < ndesc;
	     i++, p = NextMemoryDescriptor(p, dsz)) {
		start = p->PhysicalStart;
		end = start + p->NumberOfPages * EFI_PAGE_SIZE;

		if (KERNLOAD < start || KERNLOAD >= end)
			continue;

		available_pages = p->NumberOfPages -
			((KERNLOAD - start) >> EFI_PAGE_SHIFT);
		break;
	}

	if (available_pages == 0) {
		printf("Can't find valid memory map for staging area!\n");
		goto out;
	}

	i++;
	p = NextMemoryDescriptor(p, dsz);

	for ( ; i < ndesc;
	     i++, p = NextMemoryDescriptor(p, dsz)) {
		if (p->Type != EfiConventionalMemory &&
		    p->Type != EfiLoaderData)
			break;

		if (p->PhysicalStart != end)
			break;

		end = p->PhysicalStart + p->NumberOfPages * EFI_PAGE_SIZE;

		available_pages += p->NumberOfPages;
	}

	if (*nr_pages > available_pages) {
		printf("Staging area's size is reduced: %ld -> %ld!\n",
		    *nr_pages, available_pages);
		*nr_pages = available_pages;
	}
out:
	free(map);
}
#endif /* __i386__ || __amd64__ */

#if defined(__arm__)
#define	DEFAULT_EFI_STAGING_SIZE	32
#else
#define	DEFAULT_EFI_STAGING_SIZE	64
#endif
#ifndef EFI_STAGING_SIZE
#define	EFI_STAGING_SIZE	DEFAULT_EFI_STAGING_SIZE
#endif

#if defined(__aarch64__) || defined(__amd64__) || defined(__arm__) || \
    defined(__riscv)
#define	EFI_STAGING_2M_ALIGN	1
#else
#define	EFI_STAGING_2M_ALIGN	0
#endif

#if defined(__amd64__)
#define	EFI_STAGING_SLOP	M(8)
#else
#define	EFI_STAGING_SLOP	0
#endif

static u_long staging_slop = EFI_STAGING_SLOP;

EFI_PHYSICAL_ADDRESS	staging, staging_end, staging_base;
int			stage_offset_set = 0;
ssize_t			stage_offset;

static void
efi_copy_free(void)
{
	BS->FreePages(staging_base, (staging_end - staging_base) /
	    EFI_PAGE_SIZE);
	stage_offset_set = 0;
	stage_offset = 0;
}

#ifdef __amd64__
int copy_staging = COPY_STAGING_AUTO;

static int
command_copy_staging(int argc, char *argv[])
{
	static const char *const mode[3] = {
		[COPY_STAGING_ENABLE] = "enable",
		[COPY_STAGING_DISABLE] = "disable",
		[COPY_STAGING_AUTO] = "auto",
	};
	int prev, res;

	res = CMD_OK;
	if (argc > 2) {
		res = CMD_ERROR;
	} else if (argc == 2) {
		prev = copy_staging;
		if (strcmp(argv[1], "enable") == 0)
			copy_staging = COPY_STAGING_ENABLE;
		else if (strcmp(argv[1], "disable") == 0)
			copy_staging = COPY_STAGING_DISABLE;
		else if (strcmp(argv[1], "auto") == 0)
			copy_staging = COPY_STAGING_AUTO;
		else {
			printf("usage: copy_staging enable|disable|auto\n");
			res = CMD_ERROR;
		}
		if (res == CMD_OK && prev != copy_staging) {
			printf("changed copy_staging, unloading kernel\n");
			unload();
			efi_copy_free();
			efi_copy_init();
		}
	} else {
		printf("copy staging: %s\n", mode[copy_staging]);
	}
	return (res);
}
COMMAND_SET(copy_staging, "copy_staging", "copy staging", command_copy_staging);
#endif

static int
command_staging_slop(int argc, char *argv[])
{
	char *endp;
	u_long new, prev;
	int res;

	res = CMD_OK;
	if (argc > 2) {
		res = CMD_ERROR;
	} else if (argc == 2) {
		new = strtoul(argv[1], &endp, 0);
		if (*endp != '\0') {
			printf("invalid slop value\n");
			res = CMD_ERROR;
		}
		if (res == CMD_OK && staging_slop != new) {
			printf("changed slop, unloading kernel\n");
			unload();
			efi_copy_free();
			efi_copy_init();
		}
	} else {
		printf("staging slop %#lx\n", staging_slop);
	}
	return (res);
}
COMMAND_SET(staging_slop, "staging_slop", "set staging slop",
    command_staging_slop);

#if defined(__i386__) || defined(__amd64__)
/*
 * The staging area must reside in the the first 1GB or 4GB physical
 * memory: see elf64_exec() in
 * boot/efi/loader/arch/amd64/elf64_freebsd.c.
 */
static EFI_PHYSICAL_ADDRESS
get_staging_max(void)
{
	EFI_PHYSICAL_ADDRESS res;

#if defined(__i386__)
	res = G(1);
#elif defined(__amd64__)
	res = copy_staging == COPY_STAGING_ENABLE ? G(1) : G(4);
#endif
	return (res);
}
#define	EFI_ALLOC_METHOD	AllocateMaxAddress
#else
#define	EFI_ALLOC_METHOD	AllocateAnyPages
#endif

int
efi_copy_init(void)
{
	EFI_STATUS	status;
	unsigned long nr_pages;
	vm_offset_t ess;

	ess = EFI_STAGING_SIZE;
	if (ess < DEFAULT_EFI_STAGING_SIZE)
		ess = DEFAULT_EFI_STAGING_SIZE;
	nr_pages = EFI_SIZE_TO_PAGES(M(1) * ess);

#if defined(__i386__) || defined(__amd64__)
	/*
	 * We'll decrease nr_pages, if it's too big. Currently we only
	 * apply this to FreeBSD VM running on Hyper-V. Why? Please see
	 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=211746#c28
	 */
	if (running_on_hyperv())
		efi_verify_staging_size(&nr_pages);

	staging = get_staging_max();
#endif
	status = BS->AllocatePages(EFI_ALLOC_METHOD, EfiLoaderData,
	    nr_pages, &staging);
	if (EFI_ERROR(status)) {
		printf("failed to allocate staging area: %lu\n",
		    EFI_ERROR_CODE(status));
		return (status);
	}
	staging_base = staging;
	staging_end = staging + nr_pages * EFI_PAGE_SIZE;

#if EFI_STAGING_2M_ALIGN
	/*
	 * Round the kernel load address to a 2MiB value. This is needed
	 * because the kernel builds a page table based on where it has
	 * been loaded in physical address space. As the kernel will use
	 * either a 1MiB or 2MiB page for this we need to make sure it
	 * is correctly aligned for both cases.
	 */
	staging = roundup2(staging, M(2));
#endif

	return (0);
}

static bool
efi_check_space(vm_offset_t end)
{
	EFI_PHYSICAL_ADDRESS addr, new_base, new_staging;
	EFI_STATUS status;
	unsigned long nr_pages;

	end = roundup2(end, EFI_PAGE_SIZE);

	/* There is already enough space */
	if (end + staging_slop <= staging_end)
		return (true);

	if (!boot_services_active) {
		if (end <= staging_end)
			return (true);
		panic("efi_check_space: cannot expand staging area "
		    "after boot services were exited\n");
	}

	/*
	 * Add slop at the end:
	 * 1. amd64 kernel expects to do some very early allocations
	 *    by carving out memory after kernend.  Slop guarantees
	 *    that it does not ovewrite anything useful.
	 * 2. It seems that initial calculation of the staging size
	 *    could be somewhat smaller than actually copying in after
	 *    boot services are exited.  Slop avoids calling
	 *    BS->AllocatePages() when it cannot work.
	 */
	end += staging_slop;

	nr_pages = EFI_SIZE_TO_PAGES(end - staging_end);
#if defined(__i386__) || defined(__amd64__)
	/*
	 * i386 needs all memory to be allocated under the 1G boundary.
	 * amd64 needs all memory to be allocated under the 1G or 4G boundary.
	 */
	if (end > get_staging_max())
		goto before_staging;
#endif

	/* Try to allocate more space after the previous allocation */
	addr = staging_end;
	status = BS->AllocatePages(AllocateAddress, EfiLoaderData, nr_pages,
	    &addr);
	if (!EFI_ERROR(status)) {
		staging_end = staging_end + nr_pages * EFI_PAGE_SIZE;
		return (true);
	}

before_staging:
	/* Try allocating space before the previous allocation */
	if (staging < nr_pages * EFI_PAGE_SIZE)
		goto expand;
	addr = staging - nr_pages * EFI_PAGE_SIZE;
#if EFI_STAGING_2M_ALIGN
	/* See efi_copy_init for why this is needed */
	addr = rounddown2(addr, M(2));
#endif
	nr_pages = EFI_SIZE_TO_PAGES(staging_base - addr);
	status = BS->AllocatePages(AllocateAddress, EfiLoaderData, nr_pages,
	    &addr);
	if (!EFI_ERROR(status)) {
		/*
		 * Move the old allocation and update the state so
		 * translation still works.
		 */
		staging_base = addr;
		memmove((void *)(uintptr_t)staging_base,
		    (void *)(uintptr_t)staging, staging_end - staging);
		stage_offset -= staging - staging_base;
		staging = staging_base;
		return (true);
	}

expand:
	nr_pages = EFI_SIZE_TO_PAGES(end - (vm_offset_t)staging);
#if EFI_STAGING_2M_ALIGN
	nr_pages += M(2) / EFI_PAGE_SIZE;
#endif
#if defined(__i386__) || defined(__amd64__)
	new_base = get_staging_max();
#endif
	status = BS->AllocatePages(EFI_ALLOC_METHOD, EfiLoaderData,
	    nr_pages, &new_base);
	if (!EFI_ERROR(status)) {
#if EFI_STAGING_2M_ALIGN
		new_staging = roundup2(new_base, M(2));
#else
		new_staging = new_base;
#endif
		/*
		 * Move the old allocation and update the state so
		 * translation still works.
		 */
		memcpy((void *)(uintptr_t)new_staging,
		    (void *)(uintptr_t)staging, staging_end - staging);
		BS->FreePages(staging_base, (staging_end - staging_base) /
		    EFI_PAGE_SIZE);
		stage_offset -= staging - new_staging;
		staging = new_staging;
		staging_end = new_base + nr_pages * EFI_PAGE_SIZE;
		staging_base = new_base;
		return (true);
	}

	printf("efi_check_space: Unable to expand staging area\n");
	return (false);
}

void *
efi_translate(vm_offset_t ptr)
{

	return ((void *)(ptr + stage_offset));
}

ssize_t
efi_copyin(const void *src, vm_offset_t dest, const size_t len)
{

	if (!stage_offset_set) {
		stage_offset = (vm_offset_t)staging - dest;
		stage_offset_set = 1;
	}

	/* XXX: Callers do not check for failure. */
	if (!efi_check_space(dest + stage_offset + len)) {
		errno = ENOMEM;
		return (-1);
	}
	bcopy(src, (void *)(dest + stage_offset), len);
	return (len);
}

ssize_t
efi_copyout(const vm_offset_t src, void *dest, const size_t len)
{

	/* XXX: Callers do not check for failure. */
	if (src + stage_offset + len > staging_end) {
		errno = ENOMEM;
		return (-1);
	}
	bcopy((void *)(src + stage_offset), dest, len);
	return (len);
}

ssize_t
efi_readin(readin_handle_t fd, vm_offset_t dest, const size_t len)
{

	if (!stage_offset_set) {
		stage_offset = (vm_offset_t)staging - dest;
		stage_offset_set = 1;
	}

	if (!efi_check_space(dest + stage_offset + len)) {
		errno = ENOMEM;
		return (-1);
	}
	return (VECTX_READ(fd, (void *)(dest + stage_offset), len));
}

void
efi_copy_finish(void)
{
	uint64_t	*src, *dst, *last;

	src = (uint64_t *)(uintptr_t)staging;
	dst = (uint64_t *)(uintptr_t)(staging - stage_offset);
	last = (uint64_t *)(uintptr_t)staging_end;

	while (src < last)
		*dst++ = *src++;
}

void
efi_copy_finish_nop(void)
{
}
