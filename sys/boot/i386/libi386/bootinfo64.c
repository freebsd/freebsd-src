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
 *	$Id: bootinfo.c,v 1.1.1.1 1998/08/21 03:17:41 msmith Exp $
 */

#include <sys/reboot.h>
#include <stand.h>
#include "bootstrap.h"


/*
 * Return a 'boothowto' value corresponding to the kernel arguments in
 * (kargs) and any relevant environment variables.
 */
static struct 
{
    char	*ev;
    int		mask;
} howto_names[] = {
    {"boot_askname",	RB_ASKNAME},
    {"boot_userconfig",	RB_CONFIG},
    {"boot_ddb",	RB_KDB},
    {"boot_gdb",	RB_GDB},
    {"boot_single",	RB_SINGLE},
    {"boot_verbose",	RB_VERBOSE},
    {NULL,	0}
};

int
bi_getboothowto(char *kargs)
{
    char	*cp;
    int		howto;
    int		active;
    int		i;
    
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
		case 'd':
		    howto |= RB_KDB;
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
	}
	cp++;
    }
    for (i = 0; howto_names[i].ev != NULL; i++)
	if (getenv(howto_names[i].ev) != NULL)
	    howto |= howto_names[i].mask;
    if (!strcmp(getenv("console"), "comconsole"))
	howto |= RB_SERIAL;
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
	vpbcopy(ep->ev_name, addr, strlen(ep->ev_name));
	addr += strlen(ep->ev_name);
	vpbcopy("=", addr, 1);
	addr++;
	if (ep->ev_value != NULL) {
	    vpbcopy(ep->ev_value, addr, strlen(ep->ev_value));
	    addr += strlen(ep->ev_value);
	}
	vpbcopy("", addr, 1);
	addr++;
    }
    vpbcopy("", addr, 1);
    addr++;
}

/*
 * Copy module-related data into the load area, where it can be
 * used as a directory for loaded modules.
 *
 * Module data is presented in a self-describing format.  Each datum
 * is preceeded by a 16-bit identifier and a 16-bit size field.
 *
 * Currently, the following data are saved:
 *
 * MOD_NAME	(variable)		module name (string)
 * MOD_TYPE	(variable)		module type (string)
 * MOD_ADDR	sizeof(vm_offset_t)	module load address
 * MOD_SIZE	sizeof(size_t)		module size
 * MOD_METADATA	(variable)		type-specific metadata
 */
#define MOD_STR(t, a, s) {				\
    u_int32_t ident = (t << 16) + strlen(s) + 1;	\
    vpbcopy(&ident, a, sizeof(ident));			\
    a += sizeof(ident);					\
    vpbcopy(s, a, strlen(s) + 1);			\
    a += strlen(s) + 1;					\
}

#define MOD_NAME(a, s)	MOD_STR(MODINFO_NAME, a, s)
#define MOD_TYPE(a, s)	MOD_STR(MODINFO_TYPE, a, s)

#define MOD_VAR(t, a, s) {			\
    u_int32_t ident = (t << 16) + sizeof(s);	\
    vpbcopy(&ident, a, sizeof(ident));		\
    a += sizeof(ident);				\
    vpbcopy(&s, a, sizeof(s));			\
    a += sizeof(s);				\
}

#define MOD_ADDR(a, s)	MOD_VAR(MODINFO_ADDR, a, s)
#define MOD_SIZE(a, s)	MOD_VAR(MODINFO_SIZE, a, s)

#define MOD_METADATA(a, mm) {							\
    u_int32_t ident = ((MODINFO_METADATA | mm->md_type) << 16) + mm->md_size;	\
    vpbcopy(&ident, a, sizeof(ident));						\
    a += sizeof(ident);								\
    vpbcopy(mm->md_data, a, mm->md_size);					\
    a += mm->md_size;								\
}

vm_offset_t
bi_copymodules(vm_offset_t addr)
{
    struct loaded_module	*mp;
    struct module_metadata	*md;

    /* start with the first module on the list, should be the kernel */
    for (mp = mod_findmodule(NULL, NULL); mp != NULL; mp = mp->m_next) {
	
	MOD_NAME(addr, mp->m_name);
	MOD_TYPE(addr, mp->m_type);
	MOD_ADDR(addr, mp->m_addr);
	MOD_SIZE(addr, mp->m_size);
	for (md = mp->m_metadata; md != NULL; md = md->md_next)
	    if (!(md->md_type & MODINFOMD_NOCOPY))
		MOD_METADATA(addr, md);
    }
    return(addr);
}
