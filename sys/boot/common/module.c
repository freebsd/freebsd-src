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
 * $FreeBSD: src/sys/boot/common/module.c,v 1.13 2000/02/25 05:10:44 bp Exp $
 */

/*
 * module function dispatcher, support, etc.
 */

#include <stand.h>
#include <string.h>
#include <sys/param.h>
#include <sys/linker.h>

#include "bootstrap.h"

static int			mod_loadmodule(char *name, int argc, char *argv[], struct loaded_module **mpp);
static int			file_load_dependancies(struct loaded_module *base_file);
static char			*mod_searchfile(char *name);
static char			*mod_searchmodule(char *name);
static void			mod_append(struct loaded_module *mp);
static struct module_metadata 	*metadata_next(struct module_metadata *md, int type);

/* load address should be tweaked by first module loaded (kernel) */
static vm_offset_t	loadaddr = 0;

static char		*default_searchpath ="/;/boot;/modules";

struct loaded_module *loaded_modules = NULL;

/*
 * load an object, either a disk file or code module.
 *
 * To load a file, the syntax is:
 *
 * load -t <type> <path>
 *
 * code modules are loaded as:
 *
 * load <path> <options>
 */

COMMAND_SET(load, "load", "load a kernel or module", command_load);

static int
command_load(int argc, char *argv[])
{
    char	*typestr;
    int		dofile, ch, error;
    
    dofile = 0;
    optind = 1;
    optreset = 1;
    typestr = NULL;
    if (argc == 1) {
	command_errmsg = "no filename specified";
	return(CMD_ERROR);
    }
    while ((ch = getopt(argc, argv, "t:")) != -1) {
	switch(ch) {
	case 't':
	    typestr = optarg;
	    dofile = 1;
	    break;
	case '?':
	default:
	    /* getopt has already reported an error */
	    return(CMD_OK);
	}
    }
    argv += (optind - 1);
    argc -= (optind - 1);

    /*
     * Request to load a raw file?
     */
    if (dofile) {
	if ((typestr == NULL) || (*typestr == 0)) {
	    command_errmsg = "invalid load type";
	    return(CMD_ERROR);
	}
	return(mod_loadobj(typestr, argv[1]));
    }
    
    /*
     * Looks like a request for a module.
     */
    error = mod_load(argv[1], argc - 2, argv + 2);
    if (error == EEXIST)
	sprintf(command_errbuf, "warning: module '%s' already loaded", argv[1]);
    return (error == 0 ? CMD_OK : CMD_ERROR);
}

COMMAND_SET(unload, "unload", "unload all modules", command_unload);

static int
command_unload(int argc, char *argv[])
{
    struct loaded_module	*mp;
    
    while (loaded_modules != NULL) {
	mp = loaded_modules;
	loaded_modules = loaded_modules->m_next;
	mod_discard(mp);
    }
    loadaddr = 0;
    return(CMD_OK);
}

COMMAND_SET(lsmod, "lsmod", "list loaded modules", command_lsmod);

static int
command_lsmod(int argc, char *argv[])
{
    struct loaded_module	*am;
    struct module_metadata	*md;
    char			lbuf[80];
    int				ch, verbose;

    verbose = 0;
    optind = 1;
    optreset = 1;
    while ((ch = getopt(argc, argv, "v")) != -1) {
	switch(ch) {
	case 'v':
	    verbose = 1;
	    break;
	case '?':
	default:
	    /* getopt has already reported an error */
	    return(CMD_OK);
	}
    }

    pager_open();
    for (am = loaded_modules; (am != NULL); am = am->m_next) {
	sprintf(lbuf, " %p: %s (%s, 0x%lx)\n", 
		(void *) am->m_addr, am->m_name, am->m_type, (long) am->m_size);
	pager_output(lbuf);
	if (am->m_args != NULL) {
	    pager_output("    args: ");
	    pager_output(am->m_args);
	    pager_output("\n");
	}
	if (verbose)
	    /* XXX could add some formatting smarts here to display some better */
	    for (md = am->m_metadata; md != NULL; md = md->md_next) {
		sprintf(lbuf, "      0x%04x, 0x%lx\n", md->md_type, (long) md->md_size);
		pager_output(lbuf);
	    }
    }
    pager_close();
    return(CMD_OK);
}

/*
 * We've been asked to load (name) and give it (argc),(argv).
 * Start by trying to load it, and then attempt to load all of its 
 * dependancies.  If we fail at any point, throw them all away and
 * fail the entire load.
 *
 * XXX if a depended-on module requires arguments, it must be loaded
 *     explicitly first.
 */
int
mod_load(char *name, int argc, char *argv[])
{
    struct loaded_module	*last_mod, *base_mod, *mp;
    int				error;

    /* remember previous last module on chain */
    for (last_mod = loaded_modules; 
	 (last_mod != NULL) && (last_mod->m_next != NULL);
	 last_mod = last_mod->m_next)
	;
    
    /* 
     * Load the first module; note that it's the only one that gets
     * arguments explicitly.
     */
    error = mod_loadmodule(name, argc, argv, &base_mod);
    if (error)
	return (error);

    error = file_load_dependancies(base_mod);
    if (!error)
	return (0);

    /* Load failed; discard everything */
    last_mod->m_next = NULL;
    loadaddr = last_mod->m_addr + last_mod->m_size;
    while (base_mod != NULL) {
        mp = base_mod;
        base_mod = base_mod->m_next;
        mod_discard(mp);
    }
    return (error);
}

/*
 * We've been asked to load (name) as (type), so just suck it in,
 * no arguments or anything.
 */
int
mod_loadobj(char *type, char *name)
{
    struct loaded_module	*mp;
    char			*cp;
    int				fd, got;
    vm_offset_t			laddr;

    /* We can't load first */
    if ((mod_findmodule(NULL, NULL)) == NULL) {
	command_errmsg = "can't load file before kernel";
	return(CMD_ERROR);
    }

    /* locate the file on the load path */
    cp = mod_searchfile(name);
    if (cp == NULL) {
	sprintf(command_errbuf, "can't find '%s'", name);
	return(CMD_ERROR);
    }
    name = cp;
    
    if ((fd = open(name, O_RDONLY)) < 0) {
	sprintf(command_errbuf, "can't open '%s': %s", name, strerror(errno));
	free(name);
	return(CMD_ERROR);
    }

    laddr = loadaddr;
    for (;;) {
	/* read in 4k chunks; size is not really important */
	got = archsw.arch_readin(fd, laddr, 4096);
	if (got == 0)				/* end of file */
	    break;
	if (got < 0) {				/* error */
	    sprintf(command_errbuf, "error reading '%s': %s", name, strerror(errno));
	    free(name);
	    close(fd);
	    return(CMD_ERROR);
	}
	laddr += got;
    }
    
    /* Looks OK so far; create & populate control structure */
    mp = malloc(sizeof(struct loaded_module));
    mp->m_name = name;
    mp->m_type = strdup(type);
    mp->m_args = NULL;
    mp->m_metadata = NULL;
    mp->m_loader = -1;
    mp->m_addr = loadaddr;
    mp->m_size = laddr - loadaddr;

    /* recognise space consumption */
    loadaddr = laddr;

    /* Add to the list of loaded modules */
    mod_append(mp);
    close(fd);
    return(CMD_OK);
}

/*
 * Load the module (name), pass it (argc),(argv).
 * Don't do any dependancy checking.
 */
static int
mod_loadmodule(char *name, int argc, char *argv[], struct loaded_module **mpp)
{
    struct loaded_module	*mp;
    int				i, err;
    char			*cp;

    /* locate the module on the search path */
    cp = mod_searchmodule(name);
    if (cp == NULL) {
	sprintf(command_errbuf, "can't find '%s'", name);
	return (ENOENT);
    }
    name = cp;

    cp = strrchr(name, '/');
    if (cp)
        cp++;
    else
        cp = name;
    /* see if module is already loaded */
    mp = mod_findmodule(cp, NULL);
    if (mp) {
	*mpp = mp;
	return (EEXIST);
    }

    err = 0;
    for (i = 0, mp = NULL; (module_formats[i] != NULL) && (mp == NULL); i++) {
	if ((err = (module_formats[i]->l_load)(name, loadaddr, &mp)) != 0) {

	    /* Unknown to this handler? */
	    if (err == EFTYPE)
		continue;
		
	    /* Fatal error */
	    sprintf(command_errbuf, "can't load module '%s': %s", name, strerror(err));
	    free(name);
	    return (err);
	} else {

	    /* Load was OK, set args */
	    mp->m_args = unargv(argc, argv);

	    /* where can we put the next one? */
	    loadaddr = mp->m_addr + mp->m_size;
	
	    /* remember the loader */
	    mp->m_loader = i;

	    /* Add to the list of loaded modules */
	    mod_append(mp);
	    *mpp = mp;

	    break;
	}
    }
    if (err == EFTYPE)
	sprintf(command_errbuf, "don't know how to load module '%s'", name);
    free(name);
    return (err);
}

static int
file_load_dependancies(struct loaded_module *base_file)
{
    struct module_metadata	*md;
    char 			*dmodname;
    int				error;

    md = mod_findmetadata(base_file, MODINFOMD_DEPLIST);
    if (md == NULL)
	return (0);
    error = 0;
    do {
	dmodname = (char *)md->md_data;
	if (mod_findmodule(NULL, dmodname) == NULL) {
	    printf("loading required module '%s'\n", dmodname);
	    error = mod_load(dmodname, 0, NULL);
	    if (error && error != EEXIST)
		break;
	}
	md = metadata_next(md, MODINFOMD_DEPLIST);
    } while (md);
    return (error);
}

/*
 * Find a module matching (name) and (type).
 * NULL may be passed as a wildcard to either.
 */
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

/*
 * Make a copy of (size) bytes of data from (p), and associate them as
 * metadata of (type) to the module (mp).
 */
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

/*
 * Find a metadata object of (type) associated with the module
 * (mp)
 */
struct module_metadata *
mod_findmetadata(struct loaded_module *mp, int type)
{
    struct module_metadata	*md;

    for (md = mp->m_metadata; md != NULL; md = md->md_next)
	if (md->md_type == type)
	    break;
    return(md);
}

struct module_metadata *
metadata_next(struct module_metadata *md, int type)
{
    if (md == NULL)
	return (NULL);
    while((md = md->md_next) != NULL)
	if (md->md_type == type)
	    break;
    return (md);
}

/*
 * Attempt to find the file (name) on the module searchpath.
 * If (name) is qualified in any way, we simply check it and
 * return it or NULL.  If it is not qualified, then we attempt
 * to construct a path using entries in the environment variable
 * module_path.
 *
 * The path we return a pointer to need never be freed, as we manage
 * it internally.
 */
static char *
mod_searchfile(char *name)
{
    char		*result;
    char		*path, *sp;
    const char		*cp;
    struct stat		sb;

    /* Don't look for nothing */
    if (name == NULL)
	return(name);

    if (*name == 0)
	return(strdup(name));

    /*
     * See if there's a device on the front, or a directory name.
     */
    archsw.arch_getdev(NULL, name, &cp);
    if ((cp != name) || (strchr(name, '/') != NULL)) {
	/* Qualified, so just see if it exists */
	if (stat(name, &sb) == 0)
	    return(strdup(name));
	return(NULL);
    }
    
    /*
     * Get the module path
     */
    if ((cp = getenv("module_path")) == NULL)
	cp = default_searchpath;
    sp = path = strdup(cp);
    
    /*
     * Traverse the path, splitting off ';'-delimited components.
     */
    result = NULL;
    while((cp = strsep(&path, ";")) != NULL) {
	result = malloc(strlen(cp) + strlen(name) + 5);
	strcpy(result, cp);
	if (cp[strlen(cp) - 1] != '/')
	    strcat(result, "/");
	strcat(result, name);
/*	printf("search '%s'\n", result); */
	if ((stat(result, &sb) == 0) && 
	    S_ISREG(sb.st_mode))
	    break;
	free(result);
	result = NULL;
    }
    free(sp);
    return(result);
}

/*
 * Attempt to locate the file containing the module (name)
 */
static char *
mod_searchmodule(char *name)
{
    char	*tn, *result;
    
    /* Look for (name).ko */
    tn = malloc(strlen(name) + 3 + 1);
    strcpy(tn, name);
    strcat(tn, ".ko");
    result = mod_searchfile(tn);
    free(tn);
    /* Look for just (name) (useful for finding kernels) */
    if (result == NULL)
	result = mod_searchfile(name);

    return(result);
}


/*
 * Throw a module away
 */
void
mod_discard(struct loaded_module *mp)
{
    struct module_metadata	*md;

    if (mp != NULL) {
	while (mp->m_metadata != NULL) {
	    md = mp->m_metadata;
	    mp->m_metadata = mp->m_metadata->md_next;
	    free(md);
	}	
	if (mp->m_name != NULL)
	    free(mp->m_name);
	if (mp->m_type != NULL)
	    free(mp->m_type);
	if (mp->m_args != NULL)
	    free(mp->m_args);
	free(mp);
    }
}

/*
 * Allocate a new module; must be used instead of malloc()
 * to ensure safe initialisation.
 */
struct loaded_module *
mod_allocmodule(void)
{
    struct loaded_module	*mp;
    
    if ((mp = malloc(sizeof(struct loaded_module))) != NULL) {
	bzero(mp, sizeof(struct loaded_module));
    }
    return(mp);
}


/*
 * Add a module to the chain
 */
static void
mod_append(struct loaded_module *mp)
{
    struct loaded_module	*cm;
    
    /* Append to list of loaded modules */
    mp->m_next = NULL;
    if (loaded_modules == NULL) {
	loaded_modules = mp;
    } else {
	for (cm = loaded_modules; cm->m_next != NULL; cm = cm->m_next)
	    ;
	cm->m_next = mp;
    }
}
