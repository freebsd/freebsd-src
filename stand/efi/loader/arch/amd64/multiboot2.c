/*-
 * Copyright (c) 2021 Roger Pau Monné <royger@FreeBSD.org>
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

/*
 * This multiboot2 implementation only implements a subset of the full
 * multiboot2 specification in order to be able to boot Xen and a
 * FreeBSD Dom0. Trying to use it to boot other multiboot2 compliant
 * kernels will most surely fail.
 *
 * The full multiboot specification can be found here:
 * https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/stdint.h>
#define _MACHINE_ELF_WANT_32BIT
#include <machine/elf.h>
#include <machine/metadata.h>
#include <string.h>
#include <stand.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"
#include "multiboot2.h"
#include "loader_efi.h"

extern int elf32_loadfile_raw(char *filename, uint64_t dest,
    struct preloaded_file **result, int multiboot);
extern int elf64_load_modmetadata(struct preloaded_file *fp, uint64_t dest);
extern int elf64_obj_loadfile(char *filename, uint64_t dest,
    struct preloaded_file **result);
extern int bi_load(char *args, vm_offset_t *modulep, vm_offset_t *kernendp,
    bool exit_bs);

extern void multiboot2_exec(void *entry, uint64_t multiboot_info,
    uint64_t stack);

/*
 * Multiboot2 header information to pass between the loading and the exec
 * functions.
 */
struct mb2hdr {
	uint32_t efi64_entry;
};

static int
loadfile(char *filename, uint64_t dest, struct preloaded_file **result)
{
	unsigned int		 i;
	int			 error, fd;
	void			*header_search = NULL;
	void			*multiboot = NULL;
	ssize_t			 search_size;
	struct multiboot_header	*header;
	char			*cmdline;
	struct mb2hdr		 hdr;
	bool			 keep_bs = false;

	/*
	 * Read MULTIBOOT_SEARCH size in order to search for the
	 * multiboot magic header.
	 */
	if (filename == NULL)
		return (EFTYPE);
	if ((fd = open(filename, O_RDONLY)) == -1)
		return (errno);
	header_search = malloc(MULTIBOOT_SEARCH);
	if (header_search == NULL) {
		error = ENOMEM;
		goto out;
	}
	search_size = read(fd, header_search, MULTIBOOT_SEARCH);

	for (i = 0; i < search_size; i += MULTIBOOT_HEADER_ALIGN) {
		header = header_search + i;
		if (header->magic == MULTIBOOT2_HEADER_MAGIC)
			break;
	}

	if (i >= search_size) {
		error = EFTYPE;
		goto out;
	}

	/* Valid multiboot header has been found, validate checksum */
	if (header->magic + header->architecture + header->header_length +
	    header->checksum != 0) {
		printf("Multiboot checksum failed, magic: %#x "
		    "architecture: %#x header_length %#x checksum: %#x\n",
		    header->magic, header->architecture, header->header_length,
		    header->checksum);
		error = EFTYPE;
		goto out;
	}

	if (header->architecture != MULTIBOOT2_ARCHITECTURE_I386) {
		printf("Unsupported architecture: %#x\n",
		    header->architecture);
		error = EFTYPE;
		goto out;
	}

	multiboot = malloc(header->header_length - sizeof(*header));
	error = lseek(fd, i + sizeof(*header), SEEK_SET);
	if (error != i + sizeof(*header)) {
		printf("Unable to set file pointer to header location: %d\n",
		    error);
		goto out;
	}
	search_size = read(fd, multiboot,
	    header->header_length - sizeof(*header));

	bzero(&hdr, sizeof(hdr));
	for (i = 0; i < search_size; ) {
		struct multiboot_header_tag *tag;
		struct multiboot_header_tag_entry_address *entry;
		struct multiboot_header_tag_information_request *req;
		unsigned int j;

		tag = multiboot + i;

		switch(tag->type) {
		case MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST:
			req = (void *)tag;
			for (j = 0;
			    j < (tag->size - sizeof(*tag)) / sizeof(uint32_t);
			    j++) {
				switch (req->requests[j]) {
				case MULTIBOOT_TAG_TYPE_MMAP:
				case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
					/* Only applicable to BIOS. */
					break;

				case MULTIBOOT_TAG_TYPE_EFI_BS:
				case MULTIBOOT_TAG_TYPE_EFI64:
				case MULTIBOOT_TAG_TYPE_EFI64_IH:
					/* Tags unconditionally added. */
					break;

				default:
					if (req->flags &
					    MULTIBOOT_HEADER_TAG_OPTIONAL)
						break;

					printf(
				"Unknown non-optional information request %u\n",
					    req->requests[j]);
					error = EINVAL;
					goto out;
				}
			}
			break;

		case MULTIBOOT_HEADER_TAG_EFI_BS:
			/* Never shut down BS. */
			keep_bs = true;
			break;

		case MULTIBOOT_HEADER_TAG_MODULE_ALIGN:
			/* We will align modules by default already. */
		case MULTIBOOT_HEADER_TAG_END:
			break;

		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64:
			entry = (void *)tag;
			hdr.efi64_entry = entry->entry_addr;
			break;

		default:
			if (tag->flags & MULTIBOOT_HEADER_TAG_OPTIONAL)
				break;
			printf("Unknown header tag %#x not optional\n",
			    tag->type);
			error = EINVAL;
			goto out;
		}

		i += roundup2(tag->size, MULTIBOOT_TAG_ALIGN);
		if (tag->type == MULTIBOOT_HEADER_TAG_END)
			break;
	}

	if (hdr.efi64_entry == 0) {
		printf("No EFI64 entry address provided\n");
		error = EINVAL;
		goto out;
	}
	if (!keep_bs) {
		printf("Unable to boot MB2 with BS exited\n");
		error = EINVAL;
		goto out;
	}

	error = elf32_loadfile_raw(filename, dest, result, 1);
	if (error != 0) {
		printf(
	"elf32_loadfile_raw failed: %d unable to load multiboot kernel\n",
		    error);
		goto out;
	}

	file_addmetadata(*result, MODINFOMD_NOCOPY | MODINFOMD_MB2HDR,
	    sizeof(hdr), &hdr);

	/*
	 * f_addr is already aligned to PAGE_SIZE, make sure
	 * f_size it's also aligned so when the modules are loaded
	 * they are aligned to PAGE_SIZE.
	 */
	(*result)->f_size = roundup((*result)->f_size, PAGE_SIZE);

out:
	if (header_search != NULL)
		free(header_search);
	if (multiboot != NULL)
		free(multiboot);
	close(fd);
	return (error);
}

static unsigned int add_string(void *buf, unsigned int type, const char *str)
{
	struct multiboot_tag *tag;

	tag = buf;
	tag->type = type;
	tag->size = sizeof(*tag) + strlen(str) + 1;
	strcpy(buf + sizeof(*tag), str);
	return (roundup2(tag->size, MULTIBOOT_TAG_ALIGN));
}

static unsigned int add_efi(void *buf)
{
	struct multiboot_tag *bs;
	struct multiboot_tag_efi64 *efi64;
	struct multiboot_tag_efi64_ih *ih;
	unsigned int len;

	len = 0;
	bs = buf;
	bs->type = MULTIBOOT_TAG_TYPE_EFI_BS;
	bs->size = sizeof(*bs);
	len += roundup2(bs->size, MULTIBOOT_TAG_ALIGN);

	efi64 = buf + len;
	efi64->type = MULTIBOOT_TAG_TYPE_EFI64;
	efi64->size = sizeof(*efi64);
	efi64->pointer = (uintptr_t)ST;
	len += roundup2(efi64->size, MULTIBOOT_TAG_ALIGN);

	ih = buf + len;
	ih->type = MULTIBOOT_TAG_TYPE_EFI64_IH;
	ih->size = sizeof(*ih);
	ih->pointer = (uintptr_t)IH;

	return (len + roundup2(ih->size, MULTIBOOT_TAG_ALIGN));
}

static unsigned int add_module(void *buf, vm_offset_t start, vm_offset_t end,
    const char *cmdline)
{
	struct multiboot_tag_module *mod;

	mod = buf;
	mod->type = MULTIBOOT_TAG_TYPE_MODULE;
	mod->size = sizeof(*mod);
	mod->mod_start = start;
	mod->mod_end = end;
	if (cmdline != NULL)
	{
		strcpy(buf + sizeof(*mod), cmdline);
		mod->size += strlen(cmdline) + 1;
	}

	return (roundup2(mod->size, MULTIBOOT_TAG_ALIGN));
}

static unsigned int add_end(void *buf)
{
	struct multiboot_tag *tag;

	tag = buf;
	tag->type = MULTIBOOT_TAG_TYPE_END;
	tag->size = sizeof(*tag);

	return (roundup2(tag->size, MULTIBOOT_TAG_ALIGN));
}

static int
exec(struct preloaded_file *fp)
{
	EFI_PHYSICAL_ADDRESS		 addr = 0;
	EFI_PHYSICAL_ADDRESS		 stack = 0;
	EFI_STATUS			 status;
	void				*multiboot_space;
	vm_offset_t			 modulep, kernend, kern_base,
					 payload_base;
	char				*cmdline = NULL;
	size_t				 len;
	int				 error;
	uint32_t			*total_size;
	struct file_metadata		*md;
	struct xen_header		 header;
	struct mb2hdr			*hdr;


	_Static_assert(sizeof(header) <= PAGE_SIZE, "header too big");

	if ((md = file_findmetadata(fp,
	    MODINFOMD_NOCOPY | MODINFOMD_MB2HDR)) == NULL) {
		printf("Missing Multiboot2 EFI64 entry point\n");
		return(EFTYPE);
	}
	hdr = (void *)&md->md_data;

	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(PAGE_SIZE), &addr);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate pages for multiboot2 header: %lu\n",
		    EFI_ERROR_CODE(status));
		error = ENOMEM;
		goto error;
	}
	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(128 * 1024), &stack);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate pages for Xen stack: %lu\n",
		    EFI_ERROR_CODE(status));
		error = ENOMEM;
		goto error;
	}

	/*
	 * Scratch space to build the multiboot2 header. Reserve the start of
	 * the space to place the header with the size, which we don't know
	 * yet.
	 */
	multiboot_space = (void *)(uintptr_t)(addr + sizeof(uint32_t) * 2);

	/*
	 * Don't pass the memory size found by the bootloader, the memory
	 * available to Dom0 will be lower than that.
	 */
	unsetenv("smbios.memory.enabled");

	/* Set the Xen command line. */
	if (fp->f_args == NULL) {
		/* Add the Xen command line if it is set. */
		cmdline = getenv("xen_cmdline");
		if (cmdline != NULL) {
			fp->f_args = strdup(cmdline);
			if (fp->f_args == NULL) {
				error = ENOMEM;
				goto error;
			}
		}
	}
	if (fp->f_args != NULL) {
		len = strlen(fp->f_name) + 1 + strlen(fp->f_args) + 1;
		cmdline = malloc(len);
		if (cmdline == NULL) {
			error = ENOMEM;
			goto error;
		}
		snprintf(cmdline, len, "%s %s", fp->f_name, fp->f_args);
		multiboot_space += add_string(multiboot_space,
		    MULTIBOOT_TAG_TYPE_CMDLINE, cmdline);
		free(cmdline);
	}

	multiboot_space += add_string(multiboot_space,
	    MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME, "FreeBSD Loader");
	multiboot_space += add_efi(multiboot_space);

	/*
	 * Prepare the multiboot module list, Xen assumes the first
	 * module is the Dom0 kernel, and the second one is the initramfs.
	 * This is not optimal for FreeBSD, that doesn't have a initramfs
	 * but instead loads modules dynamically and creates the metadata
	 * info on-the-fly.
	 *
	 * As expected, the first multiboot module is going to be the
	 * FreeBSD kernel loaded as a raw file. The second module is going
	 * to contain the metadata info and the loaded modules.
	 *
	 * There's a small header prefixed in the second module that contains
	 * some information required to calculate the relocated address of
	 * modulep based on the original offset of modulep from the start of
	 * the module address. Note other fields might be added to this header
	 * if required.
	 *
	 * Native layout:
	 *           fp->f_addr + fp->f_size
	 * +---------+----------------+------------+
	 * |         |                |            |
	 * | Kernel  |    Modules     |  Metadata  |
	 * |         |                |            |
	 * +---------+----------------+------------+
	 * fp->f_addr                 modulep      kernend
	 *
	 * Xen dom0 layout:
	 * fp->f_addr             fp->f_addr + fp->f_size
	 * +---------+------------+----------------+------------+
	 * |         |            |                |            |
	 * | Kernel  | xen_header |    Modules     |  Metadata  |
	 * |         |            |                |            |
	 * +---------+------------+----------------+------------+
	 * 	                                   modulep      kernend
	 * \________/\__________________________________________/
	 *  module 0                 module 1
	 */

	fp = file_findfile(NULL, "elf kernel");
	if (fp == NULL) {
		printf("No FreeBSD kernel provided, aborting\n");
		error = EINVAL;
		goto error;
	}

	error = bi_load(fp->f_args, &modulep, &kernend, false);
	if (error != 0)
		goto error;

	/*
	 * Note that the Xen kernel requires to be started with BootServices
	 * enabled, and hence we cannot use efi_copy_finish to relocate the
	 * loaded data from the staging area to the expected loaded addresses.
	 * This is fine because the Xen kernel is relocatable, so it can boot
	 * fine straight from the staging area. We use efi_translate to get the
	 * staging addresses where the kernels and metadata are currently
	 * loaded.
	 */
	kern_base = (uintptr_t)efi_translate(fp->f_addr);
	payload_base = kern_base + fp->f_size - PAGE_SIZE;
	multiboot_space += add_module(multiboot_space, kern_base, payload_base,
	    NULL);
	multiboot_space += add_module(multiboot_space, payload_base,
	    (uintptr_t)efi_translate(kernend), "header");

	header.flags = XENHEADER_HAS_MODULEP_OFFSET;
	header.modulep_offset = modulep - (fp->f_addr + fp->f_size - PAGE_SIZE);
	archsw.arch_copyin(&header, fp->f_addr + fp->f_size - PAGE_SIZE,
	    sizeof(header));

	multiboot_space += add_end(multiboot_space);
	total_size = (uint32_t *)(uintptr_t)(addr);
	*total_size = (uintptr_t)multiboot_space - addr;

	if (*total_size > PAGE_SIZE)
		panic("Multiboot header exceeds fixed size");

	efi_time_fini();
	dev_cleanup();
	multiboot2_exec(efi_translate(hdr->efi64_entry), addr,
	    stack + 128 * 1024);

	panic("exec returned");

error:
	if (addr)
		BS->FreePages(addr, EFI_SIZE_TO_PAGES(PAGE_SIZE));
	if (stack)
		BS->FreePages(stack, EFI_SIZE_TO_PAGES(128 * 1024));
	return (error);
}

static int
obj_loadfile(char *filename, uint64_t dest, struct preloaded_file **result)
{
	struct preloaded_file	*mfp, *kfp, *rfp;
	struct kernel_module	*kmp;
	int			 error;

	/* See if there's a multiboot kernel loaded */
	mfp = file_findfile(NULL, "elf multiboot kernel");
	if (mfp == NULL)
		return (EFTYPE);

	/*
	 * We have a multiboot kernel loaded, see if there's a FreeBSD
	 * kernel loaded also.
	 */
	kfp = file_findfile(NULL, "elf kernel");
	if (kfp == NULL) {
		/*
		 * No kernel loaded, this must be it. The kernel has to
		 * be loaded as a raw file, it will be processed by
		 * Xen and correctly loaded as an ELF file.
		 */
		rfp = file_loadraw(filename, "elf kernel", 0);
		if (rfp == NULL) {
			printf(
			"Unable to load %s as a multiboot payload kernel\n",
			filename);
			return (EINVAL);
		}

		/* Load kernel metadata... */
		setenv("kernelname", filename, 1);
		error = elf64_load_modmetadata(rfp, rfp->f_addr + rfp->f_size);
		if (error) {
			printf("Unable to load kernel %s metadata error: %d\n",
			    rfp->f_name, error);
			return (EINVAL);
		}


		/*
		 * Reserve one page at the end of the kernel to place some
		 * metadata in order to cope for Xen relocating the modules and
		 * the metadata information.
		 */
		rfp->f_size = roundup(rfp->f_size, PAGE_SIZE);
		rfp->f_size += PAGE_SIZE;
		*result = rfp;
	} else {
		/* The rest should be loaded as regular modules */
		error = elf64_obj_loadfile(filename, dest, result);
		if (error != 0) {
			printf("Unable to load %s as an object file, error: %d",
			    filename, error);
			return (error);
		}
	}

	return (0);
}

static int
obj_exec(struct preloaded_file *fp)
{

	return (EFTYPE);
}

struct file_format multiboot2 = { loadfile, exec };
struct file_format multiboot2_obj = { obj_loadfile, obj_exec };
