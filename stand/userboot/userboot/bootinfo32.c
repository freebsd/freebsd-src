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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <i386/include/bootinfo.h>

#include "bootstrap.h"
#include "modinfo.h"
#include "libuserboot.h"

#ifdef LOADER_GELI_SUPPORT
#include "geliboot.h"
#endif

static struct bootinfo  bi;

/*
 * Load the information expected by an i386 kernel.
 *
 * - The 'boothowto' argument is constructed
 * - The 'bootdev' argument is constructed
 * - The 'bootinfo' struct is constructed, and copied into the kernel space.
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
int
bi_load32(char *args, int *howtop, int *bootdevp, vm_offset_t *bip, vm_offset_t *modulep, vm_offset_t *kernendp)
{
    struct preloaded_file	*xp, *kfp;
    struct devdesc		*rootdev;
    struct file_metadata	*md;
    vm_offset_t			addr;
    vm_offset_t			kernend;
    vm_offset_t			envp;
    vm_offset_t			size;
    vm_offset_t			ssym, esym;
    char			*rootdevname;
    int				bootdevnr, howto;
    char			*kernelname;
    const char			*kernelpath;
    uint64_t			lowmem, highmem;

    howto = bi_getboothowto(args);

    /* 
     * Allow the environment variable 'rootdev' to override the supplied device 
     * This should perhaps go to MI code and/or have $rootdev tested/set by
     * MI code before launching the kernel.
     */
    rootdevname = getenv("rootdev");
    userboot_getdev((void **)(&rootdev), rootdevname, NULL);
    if (rootdev == NULL) {		/* bad $rootdev/$currdev */
	printf("can't determine root device\n");
	return(EINVAL);
    }

    /* Try reading the /etc/fstab file to select the root device */
    getrootmount(devformat(rootdev));

    bootdevnr = 0;
#if 0
    if (bootdevnr == -1) {
	printf("root device %s invalid\n", devformat(rootdev));
	return (EINVAL);
    }
#endif
    free(rootdev);

    /* find the last module in the chain */
    addr = 0;
    for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
	if (addr < (xp->f_addr + xp->f_size))
	    addr = xp->f_addr + xp->f_size;
    }
    /* pad to a page boundary */
    addr = roundup(addr, PAGE_SIZE);

    /* copy our environment */
    envp = addr;
    addr = md_copyenv(addr);

    /* pad to a page boundary */
    addr = roundup(addr, PAGE_SIZE);

    kfp = file_findfile(NULL, "elf kernel");
    if (kfp == NULL)
      kfp = file_findfile(NULL, "elf32 kernel");
    if (kfp == NULL)
	panic("can't find kernel file");
    kernend = 0;	/* fill it in later */
    file_addmetadata(kfp, MODINFOMD_HOWTO, sizeof howto, &howto);
    file_addmetadata(kfp, MODINFOMD_ENVP, sizeof envp, &envp);
    file_addmetadata(kfp, MODINFOMD_KERNEND, sizeof kernend, &kernend);
    bios_addsmapdata(kfp);
#ifdef LOADER_GELI_SUPPORT
    geli_export_key_metadata(kfp);
#endif

    /* Figure out the size and location of the metadata */
    *modulep = addr;
    size = md_copymodules(0, false);
    kernend = roundup(addr + size, PAGE_SIZE);
    *kernendp = kernend;

    /* patch MODINFOMD_KERNEND */
    md = file_findmetadata(kfp, MODINFOMD_KERNEND);
    bcopy(&kernend, md->md_data, sizeof kernend);

    /* copy module list and metadata */
    (void)md_copymodules(addr, false);

    ssym = esym = 0;
    md = file_findmetadata(kfp, MODINFOMD_SSYM);
    if (md != NULL)
	ssym = *((vm_offset_t *)&(md->md_data));
    md = file_findmetadata(kfp, MODINFOMD_ESYM);
    if (md != NULL)
	esym = *((vm_offset_t *)&(md->md_data));
    if (ssym == 0 || esym == 0)
	ssym = esym = 0;		/* sanity */

    /* legacy bootinfo structure */
    kernelname = getenv("kernelname");
    userboot_getdev(NULL, kernelname, &kernelpath);
    bi.bi_version = BOOTINFO_VERSION;
#if 0
    for (i = 0; i < N_BIOS_GEOM; i++)
        bi.bi_bios_geom[i] = bd_getbigeom(i);
#endif
    bi.bi_size = sizeof(bi);
    CALLBACK(getmem, &lowmem, &highmem);
    bi.bi_memsizes_valid = 1;
    bi.bi_basemem = 640;
    bi.bi_extmem = (lowmem - 0x100000) / 1024;
    bi.bi_envp = envp;
    bi.bi_modulep = *modulep;
    bi.bi_kernend = kernend;
    bi.bi_symtab = ssym;       /* XXX this is only the primary kernel symtab */
    bi.bi_esymtab = esym;

    /*
     * Copy the legacy bootinfo and kernel name to the guest at 0x2000
     */
    bi.bi_kernelname = 0x2000 + sizeof(bi);
    CALLBACK(copyin, &bi, 0x2000, sizeof(bi));
    CALLBACK(copyin, kernelname, 0x2000 + sizeof(bi), strlen(kernelname) + 1);

    /* legacy boot arguments */
    *howtop = howto | RB_BOOTINFO;
    *bootdevp = bootdevnr;
    *bip = 0x2000;

    return(0);
}
