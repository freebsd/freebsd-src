/*-
 * Copyright (c) 2014 Roger Pau Monné <royger@FreeBSD.org>
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
 * This multiboot implementation only implements a subset of the full
 * multiboot specification in order to be able to boot Xen and a
 * FreeBSD Dom0. Trying to use it to boot other multiboot compliant
 * kernels will most surely fail.
 *
 * The full multiboot specification can be found here:
 * http://www.gnu.org/software/grub/manual/multiboot/multiboot.html
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

#include "bootstrap.h"
#include "multiboot.h"
#include "libi386.h"
#include "modinfo.h"
#include <btxv86.h>

#define MULTIBOOT_SUPPORTED_FLAGS \
				(MULTIBOOT_PAGE_ALIGN|MULTIBOOT_MEMORY_INFO)
#define NUM_MODULES		2

extern int elf32_loadfile_raw(char *filename, uint64_t dest,
    struct preloaded_file **result, int multiboot);
extern int elf64_load_modmetadata(struct preloaded_file *fp, uint64_t dest);
extern int elf64_obj_loadfile(char *filename, uint64_t dest,
    struct preloaded_file **result);

static int multiboot_loadfile(char *, uint64_t, struct preloaded_file **);
static int multiboot_exec(struct preloaded_file *);

static int multiboot_obj_loadfile(char *, uint64_t, struct preloaded_file **);
static int multiboot_obj_exec(struct preloaded_file *fp);

struct file_format multiboot = { multiboot_loadfile, multiboot_exec };
struct file_format multiboot_obj =
    { multiboot_obj_loadfile, multiboot_obj_exec };

extern void multiboot_tramp();

static const char mbl_name[] = "FreeBSD Loader";

static int
multiboot_loadfile(char *filename, uint64_t dest,
    struct preloaded_file **result)
{
	uint32_t		*magic;
	int			 i, error;
	caddr_t			 header_search;
	ssize_t			 search_size;
	int			 fd;
	struct multiboot_header	*header;
	char			*cmdline;

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
		close(fd);
		return (ENOMEM);
	}
	search_size = read(fd, header_search, MULTIBOOT_SEARCH);
	magic = (uint32_t *)header_search;

	header = NULL;
	for (i = 0; i < (search_size / sizeof(uint32_t)); i++) {
		if (magic[i] == MULTIBOOT_HEADER_MAGIC) {
			header = (struct multiboot_header *)&magic[i];
			break;
		}
	}

	if (header == NULL) {
		error = EFTYPE;
		goto out;
	}

	/* Valid multiboot header has been found, validate checksum */
	if (header->magic + header->flags + header->checksum != 0) {
		printf(
	"Multiboot checksum failed, magic: 0x%x flags: 0x%x checksum: 0x%x\n",
	header->magic, header->flags, header->checksum);
		error = EFTYPE;
		goto out;
	}

	if ((header->flags & ~MULTIBOOT_SUPPORTED_FLAGS) != 0) {
		printf("Unsupported multiboot flags found: 0x%x\n",
		    header->flags);
		error = EFTYPE;
		goto out;
	}

	error = elf32_loadfile_raw(filename, dest, result, 1);
	if (error != 0) {
		printf(
	"elf32_loadfile_raw failed: %d unable to load multiboot kernel\n",
	error);
		goto out;
	}

	/*
	 * f_addr is already aligned to PAGE_SIZE, make sure
	 * f_size it's also aligned so when the modules are loaded
	 * they are aligned to PAGE_SIZE.
	 */
	(*result)->f_size = roundup((*result)->f_size, PAGE_SIZE);

out:
	free(header_search);
	close(fd);
	return (error);
}

static int
multiboot_exec(struct preloaded_file *fp)
{
	vm_offset_t			 modulep, kernend, entry;
	struct file_metadata		*md;
	Elf_Ehdr			*ehdr;
	struct multiboot_info		*mb_info = NULL;
	struct multiboot_mod_list	*mb_mod = NULL;
	char				*cmdline = NULL;
	size_t				 len;
	int				 error, mod_num;
	struct xen_header		 header;

	_Static_assert(sizeof(header) <= PAGE_SIZE, "header too large for page");

	/*
	 * Don't pass the memory size found by the bootloader, the memory
	 * available to Dom0 will be lower than that.
	 */
	unsetenv("smbios.memory.enabled");

	/* Allocate the multiboot struct and fill the basic details. */
	mb_info = malloc(sizeof(struct multiboot_info));
	if (mb_info == NULL) {
		error = ENOMEM;
		goto error;
	}
	bzero(mb_info, sizeof(struct multiboot_info));
	mb_info->flags = MULTIBOOT_INFO_MEMORY|MULTIBOOT_INFO_BOOT_LOADER_NAME;
	mb_info->mem_lower = bios_basemem / 1024;
	mb_info->mem_upper = bios_extmem / 1024;
	mb_info->boot_loader_name = VTOP(mbl_name);

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
		mb_info->cmdline = VTOP(cmdline);
		mb_info->flags |= MULTIBOOT_INFO_CMDLINE;
	}

	/* Find the entry point of the Xen kernel and save it for later */
	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL) {
		printf("Unable to find %s entry point\n", fp->f_name);
		error = EINVAL;
		goto error;
	}
	ehdr = (Elf_Ehdr *)&(md->md_data);
	entry = ehdr->e_entry & 0xffffff;

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

	fp = file_findfile(NULL, md_kerntype);
	if (fp == NULL) {
		printf("No FreeBSD kernel provided, aborting\n");
		error = EINVAL;
		goto error;
	}

	mb_mod = malloc(sizeof(struct multiboot_mod_list) * NUM_MODULES);
	if (mb_mod == NULL) {
		error = ENOMEM;
		goto error;
	}

	bzero(mb_mod, sizeof(struct multiboot_mod_list) * NUM_MODULES);

	error = bi_load64(fp->f_args, &modulep, &kernend, 0);
	if (error != 0) {
		printf("bi_load64 failed: %d\n", error);
		goto error;
	}

	mb_mod[0].mod_start = fp->f_addr;
	mb_mod[0].mod_end = fp->f_addr + fp->f_size - PAGE_SIZE;

	mb_mod[1].mod_start = mb_mod[0].mod_end;
	mb_mod[1].mod_end = kernend;
	/* Indicate the kernel metadata is prefixed with a xen_header. */
	cmdline = strdup("header");
	if (cmdline == NULL) {
		printf("Out of memory, aborting\n");
		error = ENOMEM;
		goto error;
	}
	mb_mod[1].cmdline = VTOP(cmdline);

	mb_info->mods_count = NUM_MODULES;
	mb_info->mods_addr = VTOP(mb_mod);
	mb_info->flags |= MULTIBOOT_INFO_MODS;

	header.flags = XENHEADER_HAS_MODULEP_OFFSET;
	header.modulep_offset = modulep - mb_mod[1].mod_start;
	archsw.arch_copyin(&header, mb_mod[1].mod_start, sizeof(header));

	dev_cleanup();
	__exec((void *)VTOP(multiboot_tramp), (void *)entry,
	    (void *)VTOP(mb_info));

	panic("exec returned");

error:
	if (mb_mod)
		free(mb_mod);
	if (mb_info)
		free(mb_info);
	if (cmdline)
		free(cmdline);
	return (error);
}

static int
multiboot_obj_loadfile(char *filename, uint64_t dest,
    struct preloaded_file **result)
{
	struct preloaded_file	*mfp, *kfp, *rfp;
	struct kernel_module	*kmp;
	int			 error, mod_num;

	/* See if there's a multiboot kernel loaded */
	mfp = file_findfile(NULL, md_kerntype_mb);
	if (mfp == NULL)
		return (EFTYPE);

	/*
	 * We have a multiboot kernel loaded, see if there's a FreeBSD
	 * kernel loaded also.
	 */
	kfp = file_findfile(NULL, md_kerntype);
	if (kfp == NULL) {
		/*
		 * No kernel loaded, this must be it. The kernel has to
		 * be loaded as a raw file, it will be processed by
		 * Xen and correctly loaded as an ELF file.
		 */
		rfp = file_loadraw(filename, md_kerntype, 0);
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
multiboot_obj_exec(struct preloaded_file *fp)
{

	return (EFTYPE);
}
