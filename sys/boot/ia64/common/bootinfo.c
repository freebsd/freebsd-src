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
 * $FreeBSD$
 */

#include <stand.h>
#include <string.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <machine/elf.h>
#include <machine/bootinfo.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"

/*
 * Return a 'boothowto' value corresponding to the kernel arguments in
 * (kargs) and any relevant environment variables.
 */
static struct 
{
    const char	*ev;
    int		mask;
} howto_names[] = {
    {"boot_askname",	RB_ASKNAME},
    {"boot_cdrom",	RB_CDROM},
    {"boot_userconfig",	RB_CONFIG},
    {"boot_ddb",	RB_KDB},
    {"boot_gdb",	RB_GDB},
    {"boot_single",	RB_SINGLE},
    {"boot_verbose",	RB_VERBOSE},
    {NULL,	0}
};

extern char *efi_fmtdev(void *vdev);

int
bi_getboothowto(char *kargs)
{
    char	*cp;
    int		howto;
    int		active;
    int		i;
    
    /* Parse kargs */
    howto = 0;
    if (kargs  != NULL) {
	cp = kargs;
	active = 0;
	while (*cp != 0) {
	    if (!active && (*cp == '-')) {
		active = 1;
	    } else if (active)
		switch (*cp) {
		case 'a':
		    howto |= RB_ASKNAME;
		    break;
		case 'c':
		    howto |= RB_CONFIG;
		    break;
		case 'C':
		    howto |= RB_CDROM;
		    break;
		case 'd':
		    howto |= RB_KDB;
		    break;
		case 'D':
		    howto |= RB_MULTIPLE;
		    break;
		case 'm':
		    howto |= RB_MUTE;
		    break;
		case 'g':
		    howto |= RB_GDB;
		    break;
		case 'h':
		    howto |= RB_SERIAL;
		    break;
		case 'r':
		    howto |= RB_DFLTROOT;
		    break;
		case 's':
		    howto |= RB_SINGLE;
		    break;
		case 'v':
		    howto |= RB_VERBOSE;
		    break;
		default:
		    active = 0;
		    break;
		}
	    cp++;
	}
    }
    /* get equivalents from the environment */
    for (i = 0; howto_names[i].ev != NULL; i++)
	if (getenv(howto_names[i].ev) != NULL)
	    howto |= howto_names[i].mask;
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
vm_offset_t
bi_copyenv(vm_offset_t addr)
{
    struct env_var	*ep;
    
    /* traverse the environment */
    for (ep = environ; ep != NULL; ep = ep->ev_next) {
	efi_copyin(ep->ev_name, addr, strlen(ep->ev_name));
	addr += strlen(ep->ev_name);
	efi_copyin("=", addr, 1);
	addr++;
	if (ep->ev_value != NULL) {
	    efi_copyin(ep->ev_value, addr, strlen(ep->ev_value));
	    addr += strlen(ep->ev_value);
	}
	efi_copyin("", addr, 1);
	addr++;
    }
    efi_copyin("", addr, 1);
    addr++;
    return(addr);
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
#define COPY32(v, a) {				\
    u_int32_t	x = (v);			\
    efi_copyin(&x, a, sizeof(x));		\
    a += sizeof(x);				\
}

#define MOD_STR(t, a, s) {			\
    COPY32(t, a);				\
    COPY32(strlen(s) + 1, a);			\
    efi_copyin(s, a, strlen(s) + 1);		\
    a += roundup(strlen(s) + 1, sizeof(u_int64_t));\
}

#define MOD_NAME(a, s)	MOD_STR(MODINFO_NAME, a, s)
#define MOD_TYPE(a, s)	MOD_STR(MODINFO_TYPE, a, s)
#define MOD_ARGS(a, s)	MOD_STR(MODINFO_ARGS, a, s)

#define MOD_VAR(t, a, s) {			\
    COPY32(t, a);				\
    COPY32(sizeof(s), a);			\
    efi_copyin(&s, a, sizeof(s));		\
    a += roundup(sizeof(s), sizeof(u_int64_t));	\
}

#define MOD_ADDR(a, s)	MOD_VAR(MODINFO_ADDR, a, s)
#define MOD_SIZE(a, s)	MOD_VAR(MODINFO_SIZE, a, s)

#define MOD_METADATA(a, mm) {			\
    COPY32(MODINFO_METADATA | mm->md_type, a);	\
    COPY32(mm->md_size, a);			\
    efi_copyin(mm->md_data, a, mm->md_size);	\
    a += roundup(mm->md_size, sizeof(u_int64_t));\
}

#define MOD_END(a) {				\
    COPY32(MODINFO_END, a);			\
    COPY32(0, a);				\
}

vm_offset_t
bi_copymodules(vm_offset_t addr)
{
    struct preloaded_file	*fp;
    struct file_metadata	*md;

    /* start with the first module on the list, should be the kernel */
    for (fp = file_findfile(NULL, NULL); fp != NULL; fp = fp->f_next) {

	MOD_NAME(addr, fp->f_name);	/* this field must come first */
	MOD_TYPE(addr, fp->f_type);
	if (fp->f_args)
	    MOD_ARGS(addr, fp->f_args);
	MOD_ADDR(addr, fp->f_addr);
	MOD_SIZE(addr, fp->f_size);
	for (md = fp->f_metadata; md != NULL; md = md->md_next)
	    if (!(md->md_type & MODINFOMD_NOCOPY))
		MOD_METADATA(addr, md);
    }
    MOD_END(addr);
    return(addr);
}

/*
 * Load the information expected by an alpha kernel.
 *
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
int
bi_load(struct bootinfo *bi, struct preloaded_file *fp, UINTN *mapkey)
{
    char			*rootdevname;
    struct efi_devdesc		*rootdev;
    struct preloaded_file	*xp;
    vm_offset_t			addr, bootinfo_addr;
    u_int			pad;
    vm_offset_t			ssym, esym;
    struct file_metadata	*md;
    EFI_STATUS			status;
    UINTN			key;

    /*
     * Version 1 bootinfo.
     */
    bi->bi_magic = BOOTINFO_MAGIC;
    bi->bi_version = 1;

    /*
     * Calculate boothowto.
     */
    bi->bi_boothowto = bi_getboothowto(fp->f_args);

    /*
     * Stash EFI System Table.
     */
    bi->bi_systab = (u_int64_t) ST;

    /* 
     * Allow the environment variable 'rootdev' to override the supplied device 
     * This should perhaps go to MI code and/or have $rootdev tested/set by
     * MI code before launching the kernel.
     */
    rootdevname = getenv("rootdev");
    efi_getdev((void **)(&rootdev), rootdevname, NULL);
    if (rootdev == NULL) {		/* bad $rootdev/$currdev */
	printf("can't determine root device\n");
	return(EINVAL);
    }

    /* Try reading the /etc/fstab file to select the root device */
    getrootmount(efi_fmtdev((void *)rootdev));
    free(rootdev);

    ssym = esym = 0;
    if ((md = file_findmetadata(fp, MODINFOMD_SSYM)) != NULL)
	ssym = *((vm_offset_t *)&(md->md_data));
    if ((md = file_findmetadata(fp, MODINFOMD_ESYM)) != NULL)
	esym = *((vm_offset_t *)&(md->md_data));
    if (ssym == 0 || esym == 0)
	ssym = esym = 0;		/* sanity */

    bi->bi_symtab = ssym;
    bi->bi_esymtab = esym;

    fpswa_init(&bi->bi_fpswa);		/* find FPSWA interface */

    /* find the last module in the chain */
    addr = 0;
    for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
	if (addr < (xp->f_addr + xp->f_size))
	    addr = xp->f_addr + xp->f_size;
    }
    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }

    /* copy our environment */
    bi->bi_envp = addr;
    addr = bi_copyenv(addr);

    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }
    /* copy module list and metadata */
    bi->bi_modulep = addr;
    addr = bi_copymodules(addr);

    /* all done copying stuff in, save end of loaded object space */
    bi->bi_kernend = addr;

    /* read memory map and stash it after bootinfo */
    bi->bi_memmap = (u_int64_t)(bi + 1);
    bi->bi_memmap_size = 8192 - sizeof(struct bootinfo);
    status = BS->GetMemoryMap(&bi->bi_memmap_size,
			      (EFI_MEMORY_DESCRIPTOR *)bi->bi_memmap,
			      &key,
			      &bi->bi_memdesc_size,
			      &bi->bi_memdesc_version);
    if (EFI_ERROR(status)) {
	printf("bi_load: Can't read memory map\n");
	return EINVAL;
    }
    *mapkey = key;

    return(0);
}
