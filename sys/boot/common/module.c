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
 *	$Id: module.c,v 1.1.1.1 1998/08/21 03:17:41 msmith Exp $
 */

/*
 * module function dispatcher, support, etc.
 *
 * XXX need a 'searchmodule' function that takes a name and
 *     traverses a search path.
 */

#include <stand.h>
#include <string.h>

#include "bootstrap.h"

/* XXX load address should be tweaked by first module loaded (kernel) */
static vm_offset_t	loadaddr = 0;

struct loaded_module *loaded_modules = NULL;

COMMAND_SET(load, "load", "load a kernel or module", command_load);

static int
command_load(int argc, char *argv[])
{
    return(mod_load(argv[1], argc - 2, argv + 2));
}

COMMAND_SET(unload, "unload", "unload all modules", command_unload);

static int
command_unload(int argc, char *argv[])
{
    struct loaded_module	*mp;
    struct module_metadata	*md;
    
    while (loaded_modules != NULL) {
	mp = loaded_modules;
	loaded_modules = loaded_modules->m_next;
	while (mp->m_metadata != NULL) {
	    md = mp->m_metadata;
	    mp->m_metadata = mp->m_metadata->md_next;
	    free(md);
	}
	free(mp->m_name);
	free(mp->m_type);
	if (mp->m_args != NULL)
	    free(mp->m_args);
	free(mp);
    }
    loadaddr = 0;
    return(CMD_OK);
}

COMMAND_SET(lsmod, "lsmod", "list loaded modules", command_lsmod);

static int
command_lsmod(int argc, char *argv[])
{
    struct loaded_module	*am;
    char			lbuf[80];
    
    pager_open();
    for (am = loaded_modules; (am != NULL); am = am->m_next) {
	sprintf(lbuf, " %x: %s (%s, 0x%x)\n", 
		am->m_addr, am->m_name, am->m_type, am->m_size);
	pager_output(lbuf);
	if (am->m_args != NULL) {
	    pager_output("    args: ");
	    pager_output(am->m_args);
	    pager_output("\n");
	}
    }
    pager_close();
    return(CMD_OK);
}

int
mod_load(char *name, int argc, char *argv[])
{
    struct loaded_module	*am, *cm;
    int				i, err;
    
    for (i = 0, am = NULL; (module_formats[i] != NULL) && (am == NULL); i++) {
	/* XXX call searchmodule() to search for module (name) */
	if ((err = (module_formats[i]->l_load)(name, loadaddr, &am)) != 0) {

	    /* Unknown to this handler? */
	    if (err == EFTYPE)
		continue;
		
	    /* Fatal error */
	    sprintf(command_errbuf, "can't load module '%s': %s", name, strerror(err));
	    return(CMD_ERROR);
	} else {
	    /* remember the loader */
	    am->m_loader = i;
	}
    }
    if (am == NULL) {
	sprintf(command_errbuf, "can't work out what to do with '%s'", name);
	return(CMD_ERROR);
    }
    /* where can we put the next one? */
    loadaddr = am->m_addr + am->m_size;
	
    /* Load was OK, set args */
    am->m_args = unargv(argc, argv);

    /* Append to list of loaded modules */
    am->m_next = NULL;
    if (loaded_modules == NULL) {
	loaded_modules = am;
    } else {
	for (cm = loaded_modules; cm->m_next != NULL; cm = cm->m_next)
	    ;
	cm->m_next = am;
    }
    return(CMD_OK);
}

struct loaded_module *
mod_findmodule(char *name, char *type)
{
    struct loaded_module	*mp;
    
    for (mp = loaded_modules; mp != NULL; mp = mp->m_next) {
	if (((name == NULL) || !strcmp(name, mp->m_name)) &&
	    ((type == NULL) || !strcmp(type, mp->m_type)))
	    break;
    }
    return(mp);
}

void
mod_addmetadata(struct loaded_module *mp, int type, size_t size, void *p)
{
    struct module_metadata	*md;

    md = malloc(sizeof(struct module_metadata) + size);
    md->md_size = size;
    md->md_type = type;
    bcopy(p, md->md_data, size);
    md->md_next = mp->m_metadata;
    mp->m_metadata = md;
}

struct module_metadata *
mod_findmetadata(struct loaded_module *mp, int type)
{
    struct module_metadata	*md;

    for (md = mp->m_metadata; md != NULL; md = md->md_next)
	if (md->md_type == type)
	    break;
    return(md);
}
