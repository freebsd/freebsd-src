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
 * $FreeBSD: src/sys/boot/arc/lib/bootinfo.c,v 1.2 1999/08/28 00:39:37 peter Exp $
 */

#include <stand.h>
#include <string.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <machine/elf.h>
#include <machine/prom.h>
#include <machine/rpb.h>
#include <machine/bootinfo.h>
#include "bootstrap.h"

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
	alpha_copyin(ep->ev_name, addr, strlen(ep->ev_name));
	addr += strlen(ep->ev_name);
	alpha_copyin("=", addr, 1);
	addr++;
	if (ep->ev_value != NULL) {
	    alpha_copyin(ep->ev_value, addr, strlen(ep->ev_value));
	    addr += strlen(ep->ev_value);
	}
	alpha_copyin("", addr, 1);
	addr++;
    }
    alpha_copyin("", addr, 1);
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
 * MOD_ADDR	sizeof(vm_offset_t)	module load address
 * MOD_SIZE	sizeof(size_t)		module size
 * MOD_METADATA	(variable)		type-specific metadata
 */
#define COPY32(v, a) {				\
    u_int32_t	x = (v);			\
    alpha_copyin(&x, a, sizeof(x));		\
    a += sizeof(x);				\
}

#define MOD_STR(t, a, s) {			\
    COPY32(t, a);				\
    COPY32(strlen(s) + 1, a);			\
    alpha_copyin(s, a, strlen(s) + 1);		\
    a += roundup(strlen(s) + 1, sizeof(u_int64_t));\
}

#define MOD_NAME(a, s)	MOD_STR(MODINFO_NAME, a, s)
#define MOD_TYPE(a, s)	MOD_STR(MODINFO_TYPE, a, s)

#define MOD_VAR(t, a, s) {			\
    COPY32(t, a);				\
    COPY32(sizeof(s), a);			\
    alpha_copyin(&s, a, sizeof(s));		\
    a += roundup(sizeof(s), sizeof(u_int64_t));	\
}

#define MOD_ADDR(a, s)	MOD_VAR(MODINFO_ADDR, a, s)
#define MOD_SIZE(a, s)	MOD_VAR(MODINFO_SIZE, a, s)

#define MOD_METADATA(a, mm) {			\
    COPY32(MODINFO_METADATA | mm->md_type, a);	\
    COPY32(mm->md_size, a);			\
    alpha_copyin(mm->md_data, a, mm->md_size);	\
    a += roundup(mm->md_size, sizeof(u_int64_t));\
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
 * Load the information expected by an alpha kernel.
 *
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
int
bi_load(struct bootinfo_v1 *bi, vm_offset_t *ffp_save,
	struct loaded_module *mp)
{
    struct loaded_module	*xp;
    vm_offset_t			addr, bootinfo_addr;
    u_int			pad;
    vm_offset_t			ssym, esym;
    struct module_metadata	*md;

    ssym = esym = 0;
    if ((md = mod_findmetadata(mp, MODINFOMD_SSYM)) != NULL)
	ssym = *((vm_offset_t *)&(md->md_data));
    if ((md = mod_findmetadata(mp, MODINFOMD_ESYM)) != NULL)
	esym = *((vm_offset_t *)&(md->md_data));
    if (ssym == 0 || esym == 0)
	ssym = esym = 0;		/* sanity */

    bi->ssym = ssym;
    bi->esym = esym;

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
    bi->envp = (char *)addr;
    addr = bi_copyenv(addr);

    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }
    /* copy module list and metadata */
    bi->modptr = addr;
    addr = bi_copymodules(addr);

    /* all done copying stuff in, save end of loaded object space */
    bi->kernend = addr;

    *ffp_save = ALPHA_K0SEG_TO_PHYS((addr + PAGE_MASK) & ~PAGE_MASK)
	>> PAGE_SHIFT;
    *ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

    return(0);
}
