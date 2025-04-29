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

#include <stand.h>
#include <string.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/reboot.h>
#include <sys/boot.h>
#include <machine/cpufunc.h>
#include <machine/elf.h>
#include <machine/metadata.h>
#include <machine/psl.h>

#ifdef EFI
#include <efi.h>
#include <efilib.h>
#else
#include "kboot.h"
#endif

#include "bootstrap.h"
#include "modinfo.h"

#if defined(__amd64__)
#include <machine/specialreg.h>
#endif

#ifdef EFI
#include "loader_efi.h"
#include "gfx_fb.h"
#endif

#if defined(LOADER_FDT_SUPPORT)
#include <fdt_platform.h>
#endif

#ifdef LOADER_GELI_SUPPORT
#include "geliboot.h"
#endif

static int
bi_getboothowto(char *kargs)
{
#ifdef EFI
	const char *sw, *tmp;
	char *opts;
	int speed, port;
	char buf[50];
#endif
	char *console;
	int howto;

	howto = boot_parse_cmdline(kargs);
	howto |= boot_env_to_howto();

	console = getenv("console");
	if (console != NULL) {
		if (strcmp(console, "comconsole") == 0)
			howto |= RB_SERIAL;
		if (strcmp(console, "nullconsole") == 0)
			howto |= RB_MUTE;
#ifdef EFI
#if defined(__i386__) || defined(__amd64__)
		if (strcmp(console, "efi") == 0 &&
		    getenv("efi_8250_uid") != NULL &&
		    getenv("hw.uart.console") == NULL) {
			/*
			 * If we found a 8250 com port and com speed, we need to
			 * tell the kernel where the serial port is, and how
			 * fast. Ideally, we'd get the port from ACPI, but that
			 * isn't running in the loader. Do the next best thing
			 * by allowing it to be set by a loader.conf variable,
			 * either a EFI specific one, or the compatible
			 * comconsole_port if not. PCI support is needed, but
			 * for that we'd ideally refactor the
			 * libi386/comconsole.c code to have identical behavior.
			 * We only try to set the port for cases where we saw
			 * the Serial(x) node when parsing, otherwise
			 * specialized hardware that has Uart nodes will have a
			 * bogus address set.
			 * But if someone specifically setup hw.uart.console,
			 * don't override that.
			 */
			speed = -1;
			port = -1;
			tmp = getenv("efi_com_speed");
			if (tmp != NULL)
				speed = strtol(tmp, NULL, 0);
			tmp = getenv("efi_com_port");
			if (tmp != NULL)
				port = strtol(tmp, NULL, 0);
			if (port <= 0) {
				tmp = getenv("comconsole_port");
				if (tmp != NULL)
					port = strtol(tmp, NULL, 0);
				else {
					if (port == 0)
						port = 0x3f8;
				}
			}
			if (speed != -1 && port != -1) {
				snprintf(buf, sizeof(buf), "io:%d,br:%d", port,
				    speed);
				env_setenv("hw.uart.console", EV_VOLATILE, buf,
				    NULL, NULL);
			}
		}
#endif
#endif
	}

	return (howto);
}

#ifdef EFI
static EFI_STATUS
efi_do_vmap(EFI_MEMORY_DESCRIPTOR *mm, UINTN sz, UINTN mmsz, UINT32 mmver)
{
	EFI_MEMORY_DESCRIPTOR *desc, *viter, *vmap;
	EFI_STATUS ret;
	int curr, ndesc, nset;

	nset = 0;
	desc = mm;
	ndesc = sz / mmsz;
	vmap = malloc(sz);
	if (vmap == NULL)
		/* This isn't really an EFI error case, but pretend it is */
		return (EFI_OUT_OF_RESOURCES);
	viter = vmap;
	for (curr = 0; curr < ndesc;
	    curr++, desc = NextMemoryDescriptor(desc, mmsz)) {
		if ((desc->Attribute & EFI_MEMORY_RUNTIME) != 0) {
			++nset;
			desc->VirtualStart = desc->PhysicalStart;
			*viter = *desc;
			viter = NextMemoryDescriptor(viter, mmsz);
		}
	}
	ret = RS->SetVirtualAddressMap(nset * mmsz, mmsz, mmver, vmap);
	free(vmap);
	return (ret);
}

static int
bi_load_efi_data(struct preloaded_file *kfp, bool exit_bs)
{
	EFI_MEMORY_DESCRIPTOR *mm;
	EFI_PHYSICAL_ADDRESS addr = 0;
	EFI_STATUS status;
	const char *efi_novmap;
	size_t efisz;
	UINTN efi_mapkey;
	UINTN dsz, pages, retry, sz;
	UINT32 mmver;
	struct efi_map_header *efihdr;
	bool do_vmap;

#ifdef MODINFOMD_EFI_FB
	struct efi_fb efifb;

	efifb.fb_addr = gfx_state.tg_fb.fb_addr;
	efifb.fb_size = gfx_state.tg_fb.fb_size;
	efifb.fb_height = gfx_state.tg_fb.fb_height;
	efifb.fb_width = gfx_state.tg_fb.fb_width;
	efifb.fb_stride = gfx_state.tg_fb.fb_stride;
	efifb.fb_mask_red = gfx_state.tg_fb.fb_mask_red;
	efifb.fb_mask_green = gfx_state.tg_fb.fb_mask_green;
	efifb.fb_mask_blue = gfx_state.tg_fb.fb_mask_blue;
	efifb.fb_mask_reserved = gfx_state.tg_fb.fb_mask_reserved;

	if (efifb.fb_addr != 0) {
		printf("EFI framebuffer information:\n");
		printf("addr, size     0x%jx, 0x%jx\n",
		    efifb.fb_addr, efifb.fb_size);
		printf("dimensions     %d x %d\n",
		    efifb.fb_width, efifb.fb_height);
		printf("stride         %d\n", efifb.fb_stride);
		printf("masks          0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		    efifb.fb_mask_red, efifb.fb_mask_green, efifb.fb_mask_blue,
		    efifb.fb_mask_reserved);

		file_addmetadata(kfp, MODINFOMD_EFI_FB, sizeof(efifb), &efifb);
	}
#endif

	do_vmap = true;
	efi_novmap = getenv("efi_disable_vmap");
	if (efi_novmap != NULL)
		do_vmap = strcasecmp(efi_novmap, "YES") != 0;

	efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;

	/*
	 * Assign size of EFI_MEMORY_DESCRIPTOR to keep compatible with
	 * u-boot which doesn't fill this value when buffer for memory
	 * descriptors is too small (eg. 0 to obtain memory map size)
	 */
	dsz = sizeof(EFI_MEMORY_DESCRIPTOR);

	/*
	 * Allocate enough pages to hold the bootinfo block and the
	 * memory map EFI will return to us. The memory map has an
	 * unknown size, so we have to determine that first. Note that
	 * the AllocatePages call can itself modify the memory map, so
	 * we have to take that into account as well. The changes to
	 * the memory map are caused by splitting a range of free
	 * memory into two, so that one is marked as being loader
	 * data.
	 */

	sz = 0;
	mm = NULL;

	/*
	 * Matthew Garrett has observed at least one system changing the
	 * memory map when calling ExitBootServices, causing it to return an
	 * error, probably because callbacks are allocating memory.
	 * So we need to retry calling it at least once.
	 */
	for (retry = 2; retry > 0; retry--) {
		for (;;) {
			status = BS->GetMemoryMap(&sz, mm, &efi_mapkey, &dsz, &mmver);
			if (!EFI_ERROR(status))
				break;

			if (status != EFI_BUFFER_TOO_SMALL) {
				printf("%s: GetMemoryMap error %lu\n", __func__,
	                           EFI_ERROR_CODE(status));
				return (EINVAL);
			}

			if (addr != 0)
				BS->FreePages(addr, pages);

			/* Add 10 descriptors to the size to allow for
			 * fragmentation caused by calling AllocatePages */
			sz += (10 * dsz);
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
			efihdr = (struct efi_map_header *)(uintptr_t)addr;
			mm = (void *)((uint8_t *)efihdr + efisz);
			sz = (EFI_PAGE_SIZE * pages) - efisz;
		}

		if (!exit_bs)
			break;
		status = efi_exit_boot_services(efi_mapkey);
		if (!EFI_ERROR(status))
			break;
	}

	if (retry == 0) {
		BS->FreePages(addr, pages);
		printf("ExitBootServices error %lu\n", EFI_ERROR_CODE(status));
		return (EINVAL);
	}

	/*
	 * This may be disabled by setting efi_disable_vmap in
	 * loader.conf(5). By default we will setup the virtual
	 * map entries.
	 */

	if (do_vmap)
		efi_do_vmap(mm, sz, dsz, mmver);
	efihdr->memory_size = sz;
	efihdr->descriptor_size = dsz;
	efihdr->descriptor_version = mmver;
	file_addmetadata(kfp, MODINFOMD_EFI_MAP, efisz + sz,
	    efihdr);

	return (0);
}
#endif

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
bi_load(char *args, vm_offset_t *modulep, vm_offset_t *kernendp, bool exit_bs)
{
	struct preloaded_file *xp, *kfp;
	struct devdesc *rootdev;
	struct file_metadata *md;
	vm_offset_t addr;
	uint64_t kernend;
#ifdef MODINFOMD_MODULEP
	uint64_t module;
#endif
	uint64_t envp;
	vm_offset_t size;
	char *rootdevname;
	int howto;
#ifdef __i386__
	/*
	 * The 32-bit UEFI loader is used to
	 * boot the 64-bit kernel on machines
	 * that support it.
	 */
	bool is64 = true;
#else
	bool is64 = sizeof(long) == 8;
#endif
#if defined(LOADER_FDT_SUPPORT)
	vm_offset_t dtbp;
	int dtb_size;
#endif
#if defined(__arm__)
	vm_offset_t vaddr;
	size_t i;
	/*
	 * These metadata addreses must be converted for kernel after
	 * relocation.
	 */
	uint32_t		mdt[] = {
	    MODINFOMD_SSYM, MODINFOMD_ESYM, MODINFOMD_KERNEND,
	    MODINFOMD_ENVP, MODINFOMD_FONT,
#if defined(LOADER_FDT_SUPPORT)
	    MODINFOMD_DTBP
#endif
	};
#endif
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
	getrootmount(devformat(rootdev));

	addr = 0;
	for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
		if (addr < xp->f_addr + xp->f_size)
			addr = xp->f_addr + xp->f_size;
	}

	/* Pad to a page boundary. */
	addr = roundup(addr, PAGE_SIZE);

#ifdef EFI
	addr = build_font_module(addr);

	/* Pad to a page boundary. */
	addr = roundup(addr, PAGE_SIZE);

	addr = build_splash_module(addr);

	/* Pad to a page boundary. */
	addr = roundup(addr, PAGE_SIZE);
#endif

	/* Copy our environment. */
	envp = addr;
	addr = md_copyenv(addr);

	/* Pad to a page boundary. */
	addr = roundup(addr, PAGE_SIZE);

#if defined(LOADER_FDT_SUPPORT)
	/* Handle device tree blob */
	dtbp = addr;
	dtb_size = fdt_copy(addr);

	/* Pad to a page boundary */
	if (dtb_size)
		addr += roundup(dtb_size, PAGE_SIZE);
#endif

	kfp = file_findfile(NULL, md_kerntype);
	if (kfp == NULL)
		panic("can't find kernel file");
	kernend = 0;	/* fill it in later */

	/* Figure out the size and location of the metadata. */
	*modulep = addr;

	file_addmetadata(kfp, MODINFOMD_HOWTO, sizeof(howto), &howto);
	file_addmetadata(kfp, MODINFOMD_ENVP, sizeof(envp), &envp);
#if defined(LOADER_FDT_SUPPORT)
	if (dtb_size)
		file_addmetadata(kfp, MODINFOMD_DTBP, sizeof(dtbp), &dtbp);
	else
		printf("WARNING! Trying to fire up the kernel, but no "
		    "device tree blob found!\n");
#endif
	file_addmetadata(kfp, MODINFOMD_KERNEND, sizeof(kernend), &kernend);
#ifdef MODINFOMD_MODULEP
	module = *modulep;
	file_addmetadata(kfp, MODINFOMD_MODULEP, sizeof(module), &module);
#endif
#ifdef EFI
#ifndef __i386__
	file_addmetadata(kfp, MODINFOMD_FW_HANDLE, sizeof(ST), &ST);
#endif
#if defined(__amd64__) || defined(__i386__)
	file_addmetadata(kfp, MODINFOMD_EFI_ARCH, sizeof(MACHINE_ARCH),
	    MACHINE_ARCH);
#endif
#endif
#ifdef LOADER_GELI_SUPPORT
	geli_export_key_metadata(kfp);
#endif
#ifdef EFI
	bi_load_efi_data(kfp, exit_bs);
#else
	bi_loadsmap(kfp);
#endif

	size = md_copymodules(0, is64);	/* Find the size of the modules */
	kernend = roundup(addr + size, PAGE_SIZE);
	*kernendp = kernend;

	/* patch MODINFOMD_KERNEND */
	md = file_findmetadata(kfp, MODINFOMD_KERNEND);
	bcopy(&kernend, md->md_data, sizeof kernend);

#if defined(__arm__)
	*modulep -= __elfN(relocation_offset);

	/* Do relocation fixup on metadata of each module. */
	for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
		for (i = 0; i < nitems(mdt); i++) {
			md = file_findmetadata(xp, mdt[i]);
			if (md) {
				bcopy(md->md_data, &vaddr, sizeof vaddr);
				vaddr -= __elfN(relocation_offset);
				bcopy(&vaddr, md->md_data, sizeof vaddr);
			}
		}
	}
#endif

	/* Copy module list and metadata. */
	(void)md_copymodules(addr, is64);

	return (0);
}
