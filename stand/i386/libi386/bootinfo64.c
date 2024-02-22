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

#include <stand.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <machine/bootinfo.h>
#include <machine/cpufunc.h>
#include <machine/metadata.h>
#include <machine/psl.h>
#include <machine/specialreg.h>
#include "bootstrap.h"
#include "modinfo.h"
#include "libi386.h"
#include "btxv86.h"

#ifdef LOADER_GELI_SUPPORT
#include "geliboot.h"
#endif

/*
 * Check to see if this CPU supports long mode.
 */
static int
bi_checkcpu(void)
{
    char *cpu_vendor;
    int vendor[3];
    int eflags;
    unsigned int regs[4];

    /* Check for presence of "cpuid". */
    eflags = read_eflags();
    write_eflags(eflags ^ PSL_ID);
    if (!((eflags ^ read_eflags()) & PSL_ID))
	return (0);

    /* Fetch the vendor string. */
    do_cpuid(0, regs);
    vendor[0] = regs[1];
    vendor[1] = regs[3];
    vendor[2] = regs[2];
    cpu_vendor = (char *)vendor;

    /* Check for vendors that support AMD features. */
    if (strncmp(cpu_vendor, INTEL_VENDOR_ID, 12) != 0 &&
	strncmp(cpu_vendor, AMD_VENDOR_ID, 12) != 0 &&
	strncmp(cpu_vendor, HYGON_VENDOR_ID, 12) != 0 &&
	strncmp(cpu_vendor, CENTAUR_VENDOR_ID, 12) != 0)
	return (0);

    /* Has to support AMD features. */
    do_cpuid(0x80000000, regs);
    if (!(regs[0] >= 0x80000001))
	return (0);

    /* Check for long mode. */
    do_cpuid(0x80000001, regs);
    return (regs[3] & AMDID_LM);
}

/*
 * Load the information expected by an amd64 kernel.
 *
 * - The 'boothowto' argument is constructed
 * - The 'bootdev' argument is constructed
 * - The 'bootinfo' struct is constructed, and copied into the kernel space.
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
int
bi_load64(char *args, vm_offset_t *modulep,
    vm_offset_t *kernendp, int add_smap)
{
    struct preloaded_file	*xp, *kfp;
    struct i386_devdesc		*rootdev;
    struct file_metadata	*md;
    uint64_t			kernend;
    uint64_t			envp;
    uint64_t			module;
    uint64_t			addr;
    vm_offset_t			size;
    char			*rootdevname;
    int				howto;

    if (!bi_checkcpu()) {
	printf("CPU doesn't support long mode\n");
	return (EINVAL);
    }

    howto = bi_getboothowto(args);

    /*
     * Allow the environment variable 'rootdev' to override the supplied device
     * This should perhaps go to MI code and/or have $rootdev tested/set by
     * MI code before launching the kernel.
     */
    rootdevname = getenv("rootdev");
    i386_getdev((void **)(&rootdev), rootdevname, NULL);
    if (rootdev == NULL) {		/* bad $rootdev/$currdev */
	printf("can't determine root device\n");
	return(EINVAL);
    }

    /* Try reading the /etc/fstab file to select the root device */
    getrootmount(devformat(&rootdev->dd));

    addr = 0;
    /* find the last module in the chain */
    for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
        if (addr < (xp->f_addr + xp->f_size))
            addr = xp->f_addr + xp->f_size;
    }
    /* pad to a page boundary */
    addr = roundup(addr, PAGE_SIZE);

    addr = build_font_module(addr);

    /* place the metadata before anything */
    module = *modulep = addr;

    kfp = file_findfile(NULL, "elf kernel");
    if (kfp == NULL)
      kfp = file_findfile(NULL, "elf64 kernel");
    if (kfp == NULL)
	panic("can't find kernel file");
    kernend = 0;	/* fill it in later */
    file_addmetadata(kfp, MODINFOMD_HOWTO, sizeof howto, &howto);
    file_addmetadata(kfp, MODINFOMD_ENVP, sizeof envp, &envp);
    file_addmetadata(kfp, MODINFOMD_KERNEND, sizeof kernend, &kernend);
    file_addmetadata(kfp, MODINFOMD_MODULEP, sizeof module, &module);
    if (add_smap != 0)
        bios_addsmapdata(kfp);
#ifdef LOADER_GELI_SUPPORT
    geli_export_key_metadata(kfp);
#endif
    bi_load_vbe_data(kfp);

    size = md_copymodules(0, true);

    /* copy our environment */
    envp = roundup(addr + size, PAGE_SIZE);
    addr = md_copyenv(envp);

    /* set kernend */
    kernend = roundup(addr, PAGE_SIZE);
    *kernendp = kernend;

    /* patch MODINFOMD_KERNEND */
    md = file_findmetadata(kfp, MODINFOMD_KERNEND);
    bcopy(&kernend, md->md_data, sizeof kernend);

    /* patch MODINFOMD_ENVP */
    md = file_findmetadata(kfp, MODINFOMD_ENVP);
    bcopy(&envp, md->md_data, sizeof envp);

    /* copy module list and metadata */
    (void)md_copymodules(*modulep, true);

    return(0);
}
