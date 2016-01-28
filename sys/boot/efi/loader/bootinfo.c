/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2004, 2006 Marcel Moolenaar
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <sys/boot.h>
#include <machine/cpufunc.h>
#include <machine/elf.h>
#include <machine/metadata.h>
#include <machine/psl.h>
#include <machine/specialreg.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"
#include "framebuffer.h"
#include "loader_efi.h"

int bi_load(char *args, vm_offset_t *modulep, vm_offset_t *kernendp);

static const char howto_switches[] = "aCdrgDmphsv";
static int howto_masks[] = {
	RB_ASKNAME, RB_CDROM, RB_KDB, RB_DFLTROOT, RB_GDB, RB_MULTIPLE,
	RB_MUTE, RB_PAUSE, RB_SERIAL, RB_SINGLE, RB_VERBOSE
};

static int
bi_getboothowto(char *kargs)
{
	const char *sw;
	char *opts;
	char *console;
	int howto, i;

	howto = 0;

	/* Get the boot options from the environment first. */
	for (i = 0; howto_names[i].ev != NULL; i++) {
		if (getenv(howto_names[i].ev) != NULL)
			howto |= howto_names[i].mask;
	}

	console = getenv("console");
	if (console != NULL) {
		if (strcmp(console, "comconsole") == 0)
			howto |= RB_SERIAL;
		if (strcmp(console, "nullconsole") == 0)
			howto |= RB_MUTE;
	}

	/* Parse kargs */
	if (kargs == NULL)
		return (howto);

	opts = strchr(kargs, '-');
	while (opts != NULL) {
		while (*(++opts) != '\0') {
			sw = strchr(howto_switches, *opts);
			if (sw == NULL)
				break;
			howto |= howto_masks[sw - howto_switches];
		}
		opts = strchr(opts, '-');
	}

	return (howto);
}

/*
 * Copy the environment into the load area starting at (addr).
 * Each variable is formatted as <name>=<value>, with a single nul
 * separating each variable, and a double nul terminating the environment.
 */
static vm_offset_t
bi_copyenv(vm_offset_t start)
{
	struct env_var *ep;
	vm_offset_t addr, last;
	size_t len;

	addr = last = start;

	/* Traverse the environment. */
	for (ep = environ; ep != NULL; ep = ep->ev_next) {
		len = strlen(ep->ev_name);
		if ((size_t)archsw.arch_copyin(ep->ev_name, addr, len) != len)
			break;
		addr += len;
		if (archsw.arch_copyin("=", addr, 1) != 1)
			break;
		addr++;
		if (ep->ev_value != NULL) {
			len = strlen(ep->ev_value);
			if ((size_t)archsw.arch_copyin(ep->ev_value, addr, len) != len)
				break;
			addr += len;
		}
		if (archsw.arch_copyin("", addr, 1) != 1)
			break;
		last = ++addr;
	}

	if (archsw.arch_copyin("", last++, 1) != 1)
		last = start;
	return(last);
}

/*
 * Copy module-related data into the load area, where it can be
 * used as a directory for loaded modules.
 *
 * Module data is presented in a self-describing format.  Each datum
 * is preceded by a 32-bit identifier and a 32-bit size field.
 *
 * Currently, the following data are saved:
 *
 * MOD_NAME	(variable)		module name (string)
 * MOD_TYPE	(variable)		module type (string)
 * MOD_ARGS	(variable)		module parameters (string)
 * MOD_ADDR	sizeof(vm_offset_t)	module load address
 * MOD_SIZE	sizeof(size_t)		module size
 * MOD_METADATA	(variable)		type-specific metadata
 */
#define	COPY32(v, a, c) {					\
	uint32_t x = (v);					\
	if (c)							\
		archsw.arch_copyin(&x, a, sizeof(x));		\
	a += sizeof(x);						\
}

#define	MOD_STR(t, a, s, c) {					\
	COPY32(t, a, c);					\
	COPY32(strlen(s) + 1, a, c);				\
	if (c)							\
		archsw.arch_copyin(s, a, strlen(s) + 1);	\
	a += roundup(strlen(s) + 1, sizeof(u_long));		\
}

#define	MOD_NAME(a, s, c)	MOD_STR(MODINFO_NAME, a, s, c)
#define	MOD_TYPE(a, s, c)	MOD_STR(MODINFO_TYPE, a, s, c)
#define	MOD_ARGS(a, s, c)	MOD_STR(MODINFO_ARGS, a, s, c)

#define	MOD_VAR(t, a, s, c) {					\
	COPY32(t, a, c);					\
	COPY32(sizeof(s), a, c);				\
	if (c)							\
		archsw.arch_copyin(&s, a, sizeof(s));		\
	a += roundup(sizeof(s), sizeof(u_long));		\
}

#define	MOD_ADDR(a, s, c)	MOD_VAR(MODINFO_ADDR, a, s, c)
#define	MOD_SIZE(a, s, c)	MOD_VAR(MODINFO_SIZE, a, s, c)

#define	MOD_METADATA(a, mm, c) {				\
	COPY32(MODINFO_METADATA | mm->md_type, a, c);		\
	COPY32(mm->md_size, a, c);				\
	if (c)							\
		archsw.arch_copyin(mm->md_data, a, mm->md_size);	\
	a += roundup(mm->md_size, sizeof(u_long));		\
}

#define	MOD_END(a, c) {						\
	COPY32(MODINFO_END, a, c);				\
	COPY32(0, a, c);					\
}

static vm_offset_t
bi_copymodules(vm_offset_t addr)
{
	struct preloaded_file *fp;
	struct file_metadata *md;
	int c;
	uint64_t v;

	c = addr != 0;
	/* Start with the first module on the list, should be the kernel. */
	for (fp = file_findfile(NULL, NULL); fp != NULL; fp = fp->f_next) {
		MOD_NAME(addr, fp->f_name, c); /* This must come first. */
		MOD_TYPE(addr, fp->f_type, c);
		if (fp->f_args)
			MOD_ARGS(addr, fp->f_args, c);
		v = fp->f_addr;
		MOD_ADDR(addr, v, c);
		v = fp->f_size;
		MOD_SIZE(addr, v, c);
		for (md = fp->f_metadata; md != NULL; md = md->md_next)
			if (!(md->md_type & MODINFOMD_NOCOPY))
				MOD_METADATA(addr, md, c);
	}
	MOD_END(addr, c);
	return(addr);
}

static int
bi_load_efi_data(struct preloaded_file *kfp)
{
	EFI_MEMORY_DESCRIPTOR *mm;
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS status;
	size_t efisz;
	UINTN efi_mapkey;
	UINTN mmsz, pages, retry, sz;
	UINT32 mmver;
	struct efi_map_header *efihdr;
	struct efi_fb efifb;

	if (efi_find_framebuffer(&efifb) == 0) {
		printf("EFI framebuffer information:\n");
		printf("addr, size     0x%lx, 0x%lx\n", efifb.fb_addr,
		    efifb.fb_size);
		printf("dimensions     %d x %d\n", efifb.fb_width,
		    efifb.fb_height);
		printf("stride         %d\n", efifb.fb_stride);
		printf("masks          0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		    efifb.fb_mask_red, efifb.fb_mask_green, efifb.fb_mask_blue,
		    efifb.fb_mask_reserved);

		file_addmetadata(kfp, MODINFOMD_EFI_FB, sizeof(efifb), &efifb);
	}

	efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;

	/*
	 * It is possible that the first call to ExitBootServices may change
	 * the map key. Fetch a new map key and retry ExitBootServices in that
	 * case.
	 */
	for (retry = 2; retry > 0; retry--) {
		/*
		 * Allocate enough pages to hold the bootinfo block and the
		 * memory map EFI will return to us. The memory map has an
		 * unknown size, so we have to determine that first. Note that
		 * the AllocatePages call can itself modify the memory map, so
		 * we have to take that into account as well. The changes to
		 * the memory map are caused by splitting a range of free
		 * memory into two (AFAICT), so that one is marked as being
		 * loader data.
		 */
		sz = 0;
		BS->GetMemoryMap(&sz, NULL, &efi_mapkey, &mmsz, &mmver);
		sz += mmsz;
		sz = (sz + 0xf) & ~0xf;
		pages = EFI_SIZE_TO_PAGES(sz + efisz);
		status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
		     pages, &addr);
		if (EFI_ERROR(status)) {
			printf("%s: AllocatePages error %lu\n", __func__,
			    EFI_ERROR_CODE(status));
			return (ENOMEM);
		}

		/*
		 * Read the memory map and stash it after bootinfo. Align the
		 * memory map on a 16-byte boundary (the bootinfo block is page
		 * aligned).
		 */
		efihdr = (struct efi_map_header *)addr;
		mm = (void *)((uint8_t *)efihdr + efisz);
		sz = (EFI_PAGE_SIZE * pages) - efisz;

		status = BS->GetMemoryMap(&sz, mm, &efi_mapkey, &mmsz, &mmver);
		if (EFI_ERROR(status)) {
			printf("%s: GetMemoryMap error %lu\n", __func__,
			    EFI_ERROR_CODE(status));
			return (EINVAL);
		}
		status = BS->ExitBootServices(IH, efi_mapkey);
		if (EFI_ERROR(status) == 0) {
			efihdr->memory_size = sz;
			efihdr->descriptor_size = mmsz;
			efihdr->descriptor_version = mmver;
			file_addmetadata(kfp, MODINFOMD_EFI_MAP, efisz + sz,
			    efihdr);
			return (0);
		}
		BS->FreePages(addr, pages);
	}
	printf("ExitBootServices error %lu\n", EFI_ERROR_CODE(status));
	return (EINVAL);
}

/*
 * Load the information expected by an amd64 kernel.
 *
 * - The 'boothowto' argument is constructed.
 * - The 'bootdev' argument is constructed.
 * - The 'bootinfo' struct is constructed, and copied into the kernel space.
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
int
bi_load(char *args, vm_offset_t *modulep, vm_offset_t *kernendp)
{
	struct preloaded_file *xp, *kfp;
	struct devdesc *rootdev;
	struct file_metadata *md;
	vm_offset_t addr;
	uint64_t kernend;
	uint64_t envp;
	vm_offset_t size;
	char *rootdevname;
	int howto;

	howto = bi_getboothowto(args);

	/*
	 * Allow the environment variable 'rootdev' to override the supplied
	 * device. This should perhaps go to MI code and/or have $rootdev
	 * tested/set by MI code before launching the kernel.
	 */
	rootdevname = getenv("rootdev");
	archsw.arch_getdev((void**)(&rootdev), rootdevname, NULL);
	if (rootdev == NULL) {
		printf("Can't determine root device.\n");
		return(EINVAL);
	}

	/* Try reading the /etc/fstab file to select the root device */
	getrootmount(efi_fmtdev((void *)rootdev));

	addr = 0;
	for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
		if (addr < (xp->f_addr + xp->f_size))
			addr = xp->f_addr + xp->f_size;
	}

	/* Pad to a page boundary. */
	addr = roundup(addr, PAGE_SIZE);

	/* Copy our environment. */
	envp = addr;
	addr = bi_copyenv(addr);

	/* Pad to a page boundary. */
	addr = roundup(addr, PAGE_SIZE);

	kfp = file_findfile(NULL, "elf kernel");
	if (kfp == NULL)
		kfp = file_findfile(NULL, "elf64 kernel");
	if (kfp == NULL)
		panic("can't find kernel file");
	kernend = 0;	/* fill it in later */
	file_addmetadata(kfp, MODINFOMD_HOWTO, sizeof howto, &howto);
	file_addmetadata(kfp, MODINFOMD_ENVP, sizeof envp, &envp);
	file_addmetadata(kfp, MODINFOMD_KERNEND, sizeof kernend, &kernend);

	bi_load_efi_data(kfp);

	/* Figure out the size and location of the metadata. */
	*modulep = addr;
	size = bi_copymodules(0);
	kernend = roundup(addr + size, PAGE_SIZE);
	*kernendp = kernend;

	/* patch MODINFOMD_KERNEND */
	md = file_findmetadata(kfp, MODINFOMD_KERNEND);
	bcopy(&kernend, md->md_data, sizeof kernend);

	/* Copy module list and metadata. */
	(void)bi_copymodules(addr);

	return (0);
}
