/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 *	from: FreeBSD: src/sys/boot/sparc64/loader/metadata.c,v 1.6
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/boot.h>
#include <sys/reboot.h>
#if defined(LOADER_FDT_SUPPORT)
#include <fdt_platform.h>
#endif

#ifdef __arm__
#include <machine/elf.h>
#endif
#include <machine/metadata.h>

#include "bootstrap.h"
#include "modinfo.h"

#ifdef LOADER_GELI_SUPPORT
#include "geliboot.h"
#endif

static int
md_getboothowto(char *kargs)
{
    int		howto;

    /* Parse kargs */
    howto = boot_parse_cmdline(kargs);
    howto |= boot_env_to_howto();
    if (!strcmp(getenv("console"), "comconsole"))
	howto |= RB_SERIAL;
    if (!strcmp(getenv("console"), "nullconsole"))
	howto |= RB_MUTE;
    return(howto);
}

/*
 * Copy the environment into the load area starting at (addr).
 * Each variable is formatted as <name>=<value>, with a single nul
 * separating each variable, and a double nul terminating the environment.
 */
static vm_offset_t
md_copyenv(vm_offset_t addr)
{
    struct env_var	*ep;

    /* traverse the environment */
    for (ep = environ; ep != NULL; ep = ep->ev_next) {
	archsw.arch_copyin(ep->ev_name, addr, strlen(ep->ev_name));
	addr += strlen(ep->ev_name);
	archsw.arch_copyin("=", addr, 1);
	addr++;
	if (ep->ev_value != NULL) {
	    archsw.arch_copyin(ep->ev_value, addr, strlen(ep->ev_value));
	    addr += strlen(ep->ev_value);
	}
	archsw.arch_copyin("", addr, 1);
	addr++;
    }
    archsw.arch_copyin("", addr, 1);
    addr++;
    return(addr);
}

/*
 * Load the information expected by a kernel.
 *
 * - The 'boothowto' argument is constructed
 * - The 'bootdev' argument is constructed
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
static int
md_load_dual(char *args, vm_offset_t *modulep, vm_offset_t *dtb, int kern64)
{
    struct preloaded_file	*kfp;
    struct preloaded_file	*xp;
    struct file_metadata	*md;
    vm_offset_t			kernend;
    vm_offset_t			addr;
    vm_offset_t			envp;
#if defined(LOADER_FDT_SUPPORT)
    vm_offset_t			fdtp;
#endif
    vm_offset_t			size;
    uint64_t			scratch64;
    char			*rootdevname;
    int				howto;
#ifdef __arm__
    vm_offset_t			vaddr;
    int				i;

	/*
	 * These metadata addreses must be converted for kernel after
	 * relocation.
	 */
    uint32_t			mdt[] = {
	    MODINFOMD_SSYM, MODINFOMD_ESYM, MODINFOMD_KERNEND,
	    MODINFOMD_ENVP,
#if defined(LOADER_FDT_SUPPORT)
	    MODINFOMD_DTBP
#endif
    };
#endif

    align = kern64 ? 8 : 4;
    howto = md_getboothowto(args);

    /*
     * Allow the environment variable 'rootdev' to override the supplied
     * device. This should perhaps go to MI code and/or have $rootdev
     * tested/set by MI code before launching the kernel.
     */
    rootdevname = getenv("rootdev");
    if (rootdevname == NULL)
	rootdevname = getenv("currdev");
    /* Try reading the /etc/fstab file to select the root device */
    getrootmount(rootdevname);

    /* Find the last module in the chain */
    addr = 0;
    for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
	if (addr < (xp->f_addr + xp->f_size))
	    addr = xp->f_addr + xp->f_size;
    }
    /* Pad to a page boundary */
    addr = roundup(addr, PAGE_SIZE);

    /* Copy our environment */
    envp = addr;
    addr = md_copyenv(addr);

    /* Pad to a page boundary */
    addr = roundup(addr, PAGE_SIZE);

#if defined(LOADER_FDT_SUPPORT)
    /* Copy out FDT */
    fdtp = 0;
#if defined(__powerpc__)
    if (getenv("usefdt") != NULL)
#endif
    {
	size = fdt_copy(addr);
	fdtp = addr;
	addr = roundup(addr + size, PAGE_SIZE);
    }
#endif

    kernend = 0;
    kfp = file_findfile(NULL, kern64 ? "elf64 kernel" : "elf32 kernel");
    if (kfp == NULL)
	kfp = file_findfile(NULL, "elf kernel");
    if (kfp == NULL)
	panic("can't find kernel file");
    file_addmetadata(kfp, MODINFOMD_HOWTO, sizeof howto, &howto);
    if (kern64) {
	scratch64 = envp;
	file_addmetadata(kfp, MODINFOMD_ENVP, sizeof scratch64, &scratch64);
#if defined(LOADER_FDT_SUPPORT)
	if (fdtp != 0) {
	    scratch64 = fdtp;
	    file_addmetadata(kfp, MODINFOMD_DTBP, sizeof scratch64, &scratch64);
	}
#endif
	scratch64 = kernend;
	file_addmetadata(kfp, MODINFOMD_KERNEND,
		sizeof scratch64, &scratch64);
    } else {
	file_addmetadata(kfp, MODINFOMD_ENVP, sizeof envp, &envp);
#if defined(LOADER_FDT_SUPPORT)
	if (fdtp != 0)
	    file_addmetadata(kfp, MODINFOMD_DTBP, sizeof fdtp, &fdtp);
#endif
	file_addmetadata(kfp, MODINFOMD_KERNEND, sizeof kernend, &kernend);
    }
#ifdef LOADER_GELI_SUPPORT
    geli_export_key_metadata(kfp);
#endif

    *modulep = addr;
    size = md_copymodules(0, kern64);
    kernend = roundup(addr + size, PAGE_SIZE);

    md = file_findmetadata(kfp, MODINFOMD_KERNEND);
    if (kern64) {
	scratch64 = kernend;
	bcopy(&scratch64, md->md_data, sizeof scratch64);
    } else {
	bcopy(&kernend, md->md_data, sizeof kernend);
    }

#ifdef __arm__
    /* Convert addresses to the final VA */
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

    (void)md_copymodules(addr, kern64);
#if defined(LOADER_FDT_SUPPORT)
    if (dtb != NULL)
	*dtb = fdtp;
#endif

    return(0);
}

int
md_load(char *args, vm_offset_t *modulep, vm_offset_t *dtb)
{
    return (md_load_dual(args, modulep, dtb, 0));
}

#if defined(__powerpc__)
int
md_load64(char *args, vm_offset_t *modulep, vm_offset_t *dtb)
{
    return (md_load_dual(args, modulep, dtb, 1));
}
#endif
