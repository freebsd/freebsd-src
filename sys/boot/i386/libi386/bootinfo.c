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
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <machine/bootinfo.h>
#include "bootstrap.h"
#include "libi386.h"
#include "btxv86.h"

static struct bootinfo	bi;

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

vm_offset_t	bi_copymodules(vm_offset_t addr);

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
		case 'm':
		    howto |= RB_MUTE;
		    break;
		case 'g':
		    howto |= RB_GDB;
		    break;
		case 'h':
		    howto |= RB_SERIAL;
		    break;
		case 'p':
		    howto |= RB_PAUSE;
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
	i386_copyin(ep->ev_name, addr, strlen(ep->ev_name));
	addr += strlen(ep->ev_name);
	i386_copyin("=", addr, 1);
	addr++;
	if (ep->ev_value != NULL) {
	    i386_copyin(ep->ev_value, addr, strlen(ep->ev_value));
	    addr += strlen(ep->ev_value);
	}
	i386_copyin("", addr, 1);
	addr++;
    }
    i386_copyin("", addr, 1);
    addr++;
    return(addr);
}

/*
 * Copy module-related data into the load area, where it can be
 * used as a directory for loaded modules.
 *
 * Module data is presented in a self-describing format.  Each datum
 * is preceeded by a 32-bit identifier and a 32-bit size field.
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
    i386_copyin(&x, a, sizeof(x));		\
    a += sizeof(x);				\
}

#define MOD_STR(t, a, s) {			\
    COPY32(t, a);				\
    COPY32(strlen(s) + 1, a);			\
    i386_copyin(s, a, strlen(s) + 1);		\
    a += roundup(strlen(s) + 1, sizeof(u_long));\
}

#define MOD_NAME(a, s)	MOD_STR(MODINFO_NAME, a, s)
#define MOD_TYPE(a, s)	MOD_STR(MODINFO_TYPE, a, s)
#define MOD_ARGS(a, s)	MOD_STR(MODINFO_ARGS, a, s)

#define MOD_VAR(t, a, s) {			\
    COPY32(t, a);				\
    COPY32(sizeof(s), a);			\
    i386_copyin(&s, a, sizeof(s));		\
    a += roundup(sizeof(s), sizeof(u_long));	\
}

#define MOD_ADDR(a, s)	MOD_VAR(MODINFO_ADDR, a, s)
#define MOD_SIZE(a, s)	MOD_VAR(MODINFO_SIZE, a, s)

#define MOD_METADATA(a, mm) {			\
    COPY32(MODINFO_METADATA | mm->md_type, a);	\
    COPY32(mm->md_size, a);			\
    i386_copyin(mm->md_data, a, mm->md_size);	\
    a += roundup(mm->md_size, sizeof(u_long));\
}

#define MOD_END(a) {				\
    COPY32(MODINFO_END, a);			\
    COPY32(0, a);				\
}

vm_offset_t
bi_copymodules(vm_offset_t addr)
{
    struct loaded_module	*mp;
    struct module_metadata	*md;

    /* start with the first module on the list, should be the kernel */
    for (mp = mod_findmodule(NULL, NULL); mp != NULL; mp = mp->m_next) {

	MOD_NAME(addr, mp->m_name);	/* this field must come first */
	MOD_TYPE(addr, mp->m_type);
	if (mp->m_args)
	    MOD_ARGS(addr, mp->m_args);
	MOD_ADDR(addr, mp->m_addr);
	MOD_SIZE(addr, mp->m_size);
	for (md = mp->m_metadata; md != NULL; md = md->md_next)
	    if (!(md->md_type & MODINFOMD_NOCOPY))
		MOD_METADATA(addr, md);
    }
    MOD_END(addr);
    return(addr);
}

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
bi_load(char *args, int *howtop, int *bootdevp, vm_offset_t *bip)
{
    struct loaded_module	*xp;
    struct i386_devdesc		*rootdev;
    vm_offset_t			addr;
    char			*rootdevname;
    int				bootdevnr, i;
    u_int			pad;
    char			*kernelname;
    const char			*kernelpath;

    *howtop = bi_getboothowto(args);

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
    getrootmount(i386_fmtdev((void *)rootdev));

    /* Do legacy rootdev guessing */

    /* XXX - use a default bootdev of 0.  Is this ok??? */
    bootdevnr = 0;

    switch(rootdev->d_type) {
    case DEVT_CD:
	    /* Pass in BIOS device number. */
	    bi.bi_bios_dev = bc_unit2bios(rootdev->d_kind.bioscd.unit);
	    bootdevnr = bc_getdev(rootdev);
	    break;

    case DEVT_DISK:
	/* pass in the BIOS device number of the current disk */
	bi.bi_bios_dev = bd_unit2bios(rootdev->d_kind.biosdisk.unit);
	bootdevnr = bd_getdev(rootdev);
	break;

    case DEVT_NET:
	    break;
	    
    default:
	printf("WARNING - don't know how to boot from device type %d\n", rootdev->d_type);
    }
    if (bootdevnr == -1) {
	printf("root device %s invalid\n", i386_fmtdev(rootdev));
	return (EINVAL);
    }
    free(rootdev);
    *bootdevp = bootdevnr;

    /* legacy bootinfo structure */
    bi.bi_version = BOOTINFO_VERSION;
    bi.bi_kernelname = 0;		/* XXX char * -> kernel name */
    bi.bi_nfs_diskless = 0;		/* struct nfs_diskless * */
    bi.bi_n_bios_used = 0;		/* XXX would have to hook biosdisk driver for these */
    for (i = 0; i < N_BIOS_GEOM; i++)
        bi.bi_bios_geom[i] = bd_getbigeom(i);
    bi.bi_size = sizeof(bi);
    bi.bi_memsizes_valid = 1;
    bi.bi_basemem = bios_basemem / 1024;
    bi.bi_extmem = bios_extmem / 1024;

    /* find the last module in the chain */
    addr = 0;
    for (xp = mod_findmodule(NULL, NULL); xp != NULL; xp = xp->m_next) {
	if (addr < (xp->m_addr + xp->m_size))
	    addr = xp->m_addr + xp->m_size;
    }
    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }

    /* copy our environment */
    bi.bi_envp = addr;
    addr = bi_copyenv(addr);

    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }
    /* copy module list and metadata */
    bi.bi_modulep = addr;
    addr = bi_copymodules(addr);

    /* all done copying stuff in, save end of loaded object space */
    bi.bi_kernend = addr;

    *howtop |= RB_BOOTINFO;		/* it's there now */

    /*
     * Get the kernel name, strip off any device prefix.
     */
    kernelname = getenv("kernelname");
    i386_getdev(NULL, kernelname, &kernelpath);
    bi.bi_kernelname = VTOP(kernelpath);
    *bip = VTOP(&bi);

    return(0);
}
