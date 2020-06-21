/*-
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
#ifdef FDT
#include <sys/boot.h>
#endif

#include <machine/cpu.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/vmparam.h>

#ifdef FDT
#include <contrib/libfdt/libfdt.h>
#include <dev/fdt/fdt_common.h>
#endif

extern int *end;
static char *loader_envp;
static char static_kenv[4096];


#ifdef FDT
#define	CMDLINE_GUARD "FreeBSD:"
#define	LBABI_MAX_COMMAND_LINE 512
static char linux_command_line[LBABI_MAX_COMMAND_LINE + 1];
#endif

/*
 * Fake up a boot descriptor table
 */
 #define PRELOAD_PUSH_VALUE(type, value) do {		\
	*(type *)(preload_ptr + size) = (value);	\
	size += sizeof(type);				\
} while (0)

 #define PRELOAD_PUSH_STRING(str) do {			\
 	uint32_t ssize;					\
 	ssize = strlen(str) + 1;			\
 	PRELOAD_PUSH_VALUE(uint32_t, ssize);		\
	strcpy((char*)(preload_ptr + size), str);	\
	size += ssize;					\
	size = roundup(size, sizeof(u_long));		\
} while (0)


/* Build minimal set of metatda. */
static vm_offset_t
fake_preload_metadata(void *dtb_ptr, size_t dtb_size)
{
	vm_offset_t lastaddr;
	static char fake_preload[256];
	caddr_t preload_ptr;
	size_t size;

	lastaddr = (vm_offset_t)&end;
	preload_ptr = (caddr_t)&fake_preload[0];
	size = 0;

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

	if (dtb_ptr != NULL) {
		/* Copy DTB to KVA space and insert it into module chain. */
		lastaddr = roundup(lastaddr, sizeof(int));
		PRELOAD_PUSH_VALUE(uint32_t, MODINFO_METADATA | MODINFOMD_DTBP);
		PRELOAD_PUSH_VALUE(uint32_t, sizeof(uint64_t));
		PRELOAD_PUSH_VALUE(uint64_t, (uint64_t)lastaddr);
		memmove((void *)lastaddr, dtb_ptr, dtb_size);
		lastaddr += dtb_size;
		lastaddr = roundup(lastaddr, sizeof(int));
	}
	/* End marker */
	PRELOAD_PUSH_VALUE(uint32_t, 0);
	PRELOAD_PUSH_VALUE(uint32_t, 0);

	preload_metadata = (caddr_t)(uintptr_t)fake_preload;

	init_static_kenv(NULL, 0);

	return (lastaddr);
}

#ifdef FDT

/* Convert the U-Boot command line into FreeBSD kenv and boot options. */
static void
cmdline_set_env(char *cmdline, const char *guard)
{
	size_t guard_len;

	/* Skip leading spaces. */
	while (isspace(*cmdline))
		cmdline++;

	/* Test and remove guard. */
	if (guard != NULL && guard[0] != '\0') {
		guard_len  =  strlen(guard);
		if (strncasecmp(cmdline, guard, guard_len) != 0)
			return;
		cmdline += guard_len;
	}

	boothowto |= boot_parse_cmdline(cmdline);
}

void
parse_fdt_bootargs(void)
{

	if (loader_envp == NULL && fdt_get_chosen_bootargs(linux_command_line,
	    LBABI_MAX_COMMAND_LINE) == 0) {
		init_static_kenv(static_kenv, sizeof(static_kenv));
		cmdline_set_env(linux_command_line, CMDLINE_GUARD);
	}
}

#endif

#if defined(LINUX_BOOT_ABI) && defined(FDT)
static vm_offset_t
linux_parse_boot_param(struct arm64_bootparams *abp)
{
	struct fdt_header *dtb_ptr;
	size_t dtb_size;

	if (abp->modulep == 0)
		return (0);
	/* Test if modulep point to valid DTB. */
	dtb_ptr = (struct fdt_header *)abp->modulep;
	if (fdt_check_header(dtb_ptr) != 0)
		return (0);
	dtb_size = fdt_totalsize(dtb_ptr);
	return (fake_preload_metadata(dtb_ptr, dtb_size));
}

#endif

static vm_offset_t
freebsd_parse_boot_param(struct arm64_bootparams *abp)
{
	vm_offset_t lastaddr = 0;
	void *kmdp;
#ifdef DDB
	vm_offset_t ksym_start;
	vm_offset_t ksym_end;
#endif

	if (abp->modulep == 0)
		return (0);

	preload_metadata = (caddr_t)(uintptr_t)(abp->modulep);
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		return (0);

	boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
	loader_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
	init_static_kenv(loader_envp, 0);
	lastaddr = MD_FETCH(kmdp, MODINFOMD_KERNEND, vm_offset_t);
#ifdef DDB
	ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
	ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
	db_fetch_ksymtab(ksym_start, ksym_end, 0);
#endif
	return (lastaddr);
}

vm_offset_t
parse_boot_param(struct arm64_bootparams *abp)
{
	vm_offset_t lastaddr;

#if defined(LINUX_BOOT_ABI) && defined(FDT)
	lastaddr = linux_parse_boot_param(abp);
	if (lastaddr != 0)
		return (lastaddr);
#endif
	lastaddr = freebsd_parse_boot_param(abp);
	if (lastaddr != 0)
		return (lastaddr);

	/* Fall back to hardcoded metadata. */
	lastaddr = fake_preload_metadata(NULL, 0);

	return (lastaddr);
}
