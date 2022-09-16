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
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <machine/specialreg.h>

#include "bootstrap.h"
#include "modinfo.h"
#include "libuserboot.h"

/*
 * Check to see if this CPU supports long mode.
 */
static int
bi_checkcpu(void)
{
#if 0
    char *cpu_vendor;
    int vendor[3];
    int eflags, regs[4];

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
	strncmp(cpu_vendor, CENTAUR_VENDOR_ID, 12) != 0)
	return (0);

    /* Has to support AMD features. */
    do_cpuid(0x80000000, regs);
    if (!(regs[0] >= 0x80000001))
	return (0);

    /* Check for long mode. */
    do_cpuid(0x80000001, regs);
    return (regs[3] & AMDID_LM);
#else
	return (1);
#endif
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
bi_load64(char *args, vm_offset_t *modulep, vm_offset_t *kernendp)
{
    struct preloaded_file	*xp, *kfp;
    struct devdesc		*rootdev;
    struct file_metadata	*md;
    vm_offset_t			addr;
    uint64_t			kernend;
    uint64_t			envp;
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
    userboot_getdev((void **)(&rootdev), rootdevname, NULL);
    if (rootdev == NULL) {		/* bad $rootdev/$currdev */
	printf("can't determine root device\n");
	return(EINVAL);
    }

    /* Try reading the /etc/fstab file to select the root device */
    getrootmount(devformat(rootdev));

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
    addr = bi_copyenv(addr);

    /* pad to a page boundary */
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
    bios_addsmapdata(kfp);

    /* Figure out the size and location of the metadata */
    *modulep = addr;
    size = md_copymodules(0, true);
    kernend = roundup(addr + size, PAGE_SIZE);
    *kernendp = kernend;

    /* patch MODINFOMD_KERNEND */
    md = file_findmetadata(kfp, MODINFOMD_KERNEND);
    bcopy(&kernend, md->md_data, sizeof kernend);

    /* copy module list and metadata */
    (void)md_copymodules(addr, true);

    return(0);
}
