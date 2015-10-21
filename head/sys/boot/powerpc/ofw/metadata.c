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
#include <sys/reboot.h>
#include <sys/linker.h>
#include <sys/boot.h>
#include <fdt_platform.h>

#include <machine/metadata.h>

#include "bootstrap.h"

int
md_getboothowto(char *kargs)
{
    char	*cp;
    int		howto;
    int		active;
    int		i;
    
    /* Parse kargs */
    howto = 0;
    if (kargs != NULL) {
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

static int align;

#define COPY32(v, a, c) {			\
    u_int32_t	x = (v);			\
    if (c)					\
        archsw.arch_copyin(&x, a, sizeof(x));	\
    a += sizeof(x);				\
}

#define MOD_STR(t, a, s, c) {			\
    COPY32(t, a, c);				\
    COPY32(strlen(s) + 1, a, c)			\
    if (c)					\
        archsw.arch_copyin(s, a, strlen(s) + 1);\
    a += roundup(strlen(s) + 1, align);		\
}

#define MOD_NAME(a, s, c)	MOD_STR(MODINFO_NAME, a, s, c)
#define MOD_TYPE(a, s, c)	MOD_STR(MODINFO_TYPE, a, s, c)
#define MOD_ARGS(a, s, c)	MOD_STR(MODINFO_ARGS, a, s, c)

#define MOD_VAR(t, a, s, c) {			\
    COPY32(t, a, c);				\
    COPY32(sizeof(s), a, c);			\
    if (c)					\
        archsw.arch_copyin(&s, a, sizeof(s));	\
    a += roundup(sizeof(s), align);		\
}

#define MOD_ADDR(a, s, c)	MOD_VAR(MODINFO_ADDR, a, s, c)
#define MOD_SIZE(a, s, c)	MOD_VAR(MODINFO_SIZE, a, s, c)

#define MOD_METADATA(a, mm, c) {		\
    COPY32(MODINFO_METADATA | mm->md_type, a, c);\
    COPY32(mm->md_size, a, c);			\
    if (c)					\
        archsw.arch_copyin(mm->md_data, a, mm->md_size);\
    a += roundup(mm->md_size, align);		\
}

#define MOD_END(a, c) {				\
    COPY32(MODINFO_END, a, c);			\
    COPY32(0, a, c);				\
}

vm_offset_t
md_copymodules(vm_offset_t addr, int kern64)
{
    struct preloaded_file	*fp;
    struct file_metadata	*md;
    uint64_t			scratch64;
    int				c;

    c = addr != 0;
    /* start with the first module on the list, should be the kernel */
    for (fp = file_findfile(NULL, NULL); fp != NULL; fp = fp->f_next) {

	MOD_NAME(addr, fp->f_name, c);	/* this field must come first */
	MOD_TYPE(addr, fp->f_type, c);
	if (fp->f_args)
	    MOD_ARGS(addr, fp->f_args, c);
	if (kern64) {
		scratch64 = fp->f_addr;
		MOD_ADDR(addr, scratch64, c);
		scratch64 = fp->f_size;
		MOD_SIZE(addr, scratch64, c);
	} else {
		MOD_ADDR(addr, fp->f_addr, c);
		MOD_SIZE(addr, fp->f_size, c);
	}
	for (md = fp->f_metadata; md != NULL; md = md->md_next) {
	    if (!(md->md_type & MODINFOMD_NOCOPY)) {
		MOD_METADATA(addr, md, c);
	    }
	}
    }
    MOD_END(addr, c);
    return(addr);
}

/*
 * Load the information expected by a powerpc kernel.
 *
 * - The 'boothowto' argument is constructed
 * - The 'bootdev' argument is constructed
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
int
md_load_dual(char *args, vm_offset_t *modulep, vm_offset_t *dtb, int kern64)
{
    struct preloaded_file	*kfp;
    struct preloaded_file	*xp;
    struct file_metadata	*md;
    vm_offset_t			kernend;
    vm_offset_t			addr;
    vm_offset_t			envp;
    vm_offset_t			fdtp;
    vm_offset_t			size;
    uint64_t			scratch64;
    char			*rootdevname;
    int				howto;

    align = kern64 ? 8 : 4;
    howto = md_getboothowto(args);

    /* 
     * Allow the environment variable 'rootdev' to override the supplied device 
     * This should perhaps go to MI code and/or have $rootdev tested/set by
     * MI code before launching the kernel.
     */
    rootdevname = getenv("rootdev");
    if (rootdevname == NULL)
	    rootdevname = getenv("currdev");
    /* Try reading the /etc/fstab file to select the root device */
    getrootmount(rootdevname);

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

    /* Copy out FDT */
    *dtb = fdtp = 0;
    if (getenv("usefdt") != NULL) {
        size = fdt_copy(addr);
        *dtb = fdtp = addr;
        addr = roundup(addr + size, PAGE_SIZE);
    }

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
        if (fdtp != 0) {
	    scratch64 = fdtp;
	    file_addmetadata(kfp, MODINFOMD_DTBP, sizeof scratch64, &scratch64);
        }
	scratch64 = kernend;
	file_addmetadata(kfp, MODINFOMD_KERNEND, sizeof scratch64, &scratch64);
    } else {
	file_addmetadata(kfp, MODINFOMD_ENVP, sizeof envp, &envp);
        if (fdtp != 0)
	    file_addmetadata(kfp, MODINFOMD_DTBP, sizeof fdtp, &fdtp);
	file_addmetadata(kfp, MODINFOMD_KERNEND, sizeof kernend, &kernend);
    }

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
	
    (void)md_copymodules(addr, kern64);

    return(0);
}

int
md_load(char *args, vm_offset_t *modulep, vm_offset_t *dtb)
{
    return (md_load_dual(args, modulep, dtb, 0));
}

int
md_load64(char *args, vm_offset_t *modulep, vm_offset_t *dtb)
{
    return (md_load_dual(args, modulep, dtb, 1));
}

