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

/*
 * file/module function dispatcher, support, etc.
 */

#include <stand.h>
#include <string.h>
#include <sys/param.h>
#include <sys/linker.h>

#include "bootstrap.h"

static int			file_load(char *filename, vm_offset_t dest, struct preloaded_file **result);
static int			file_loadraw(char *type, char *name);
static int			file_load_dependancies(struct preloaded_file *base_mod);
static char *			file_search(char *name);
struct kernel_module *		file_findmodule(struct preloaded_file *fp, char *modname);
static char			*mod_searchmodule(char *name);
static void			file_insert_tail(struct preloaded_file *mp);
struct file_metadata*		metadata_next(struct file_metadata *base_mp, int type);

/* load address should be tweaked by first module loaded (kernel) */
static vm_offset_t	loadaddr = 0;

static const char	*default_searchpath ="/boot/kernel;/boot/modules;/modules";

struct preloaded_file *preloaded_files = NULL;

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
	return(file_loadraw(typestr, argv[1]));
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
    struct preloaded_file	*fp;
    
    while (preloaded_files != NULL) {
	fp = preloaded_files;
	preloaded_files = preloaded_files->f_next;
	file_discard(fp);
    }
    loadaddr = 0;
    unsetenv("kernelname");
    return(CMD_OK);
}

COMMAND_SET(lsmod, "lsmod", "list loaded modules", command_lsmod);

static int
command_lsmod(int argc, char *argv[])
{
    struct preloaded_file	*fp;
    struct kernel_module	*mp;
    struct file_metadata	*md;
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
    for (fp = preloaded_files; fp; fp = fp->f_next) {
	sprintf(lbuf, " %p: %s (%s, 0x%lx)\n", 
		(void *) fp->f_addr, fp->f_name, fp->f_type, (long) fp->f_size);
	pager_output(lbuf);
	if (fp->f_args != NULL) {
	    pager_output("    args: ");
	    pager_output(fp->f_args);
	    pager_output("\n");
	}
	if (fp->f_modules) {
	    pager_output("  modules: ");
	    for (mp = fp->f_modules; mp; mp = mp->m_next) {
		sprintf(lbuf, "%s ", mp->m_name);
		pager_output(lbuf);
	    }
	    pager_output("\n");
	}
	if (verbose) {
	    /* XXX could add some formatting smarts here to display some better */
	    for (md = fp->f_metadata; md != NULL; md = md->md_next) {
		sprintf(lbuf, "      0x%04x, 0x%lx\n", md->md_type, (long) md->md_size);
		pager_output(lbuf);
	    }
	}
    }
    pager_close();
    return(CMD_OK);
}

/*
 * File level interface, functions file_*
 */
int
file_load(char *filename, vm_offset_t dest, struct preloaded_file **result)
{
    struct preloaded_file *fp;
    int error;
    int i;

    error = EFTYPE;
    for (i = 0, fp = NULL; file_formats[i] && fp == NULL; i++) {
	error = (file_formats[i]->l_load)(filename, loadaddr, &fp);
	if (error == 0) {
	    fp->f_loader = i;		/* remember the loader */
	    *result = fp;
	    break;
	}
	if (error == EFTYPE)
	    continue;		/* Unknown to this handler? */
	if (error) {
	    sprintf(command_errbuf, "can't load file '%s': %s",
		filename, strerror(error));
	    break;
	}
    }
    return (error);
}

static int
file_load_dependancies(struct preloaded_file *base_file) {
    struct file_metadata *md;
    struct preloaded_file *fp;
    char *dmodname;
    int error;

    md = file_findmetadata(base_file, MODINFOMD_DEPLIST);
    if (md == NULL)
	return (0);
    error = 0;
    do {
	dmodname = (char *)md->md_data;
	if (file_findmodule(NULL, dmodname) == NULL) {
	    printf("loading required module '%s'\n", dmodname);
	    error = mod_load(dmodname, 0, NULL);
	    if (error)
		break;
	}
	md = metadata_next(md, MODINFOMD_DEPLIST);
    } while (md);
    if (!error)
	return (0);
    /* Load failed; discard everything */
    while (base_file != NULL) {
        fp = base_file;
        base_file = base_file->f_next;
        file_discard(fp);
    }
    return (error);
}
/*
 * We've been asked to load (name) as (type), so just suck it in,
 * no arguments or anything.
 */
int
file_loadraw(char *type, char *name)
{
    struct preloaded_file	*fp;
    char			*cp;
    int				fd, got;
    vm_offset_t			laddr;

    /* We can't load first */
    if ((file_findfile(NULL, NULL)) == NULL) {
	command_errmsg = "can't load file before kernel";
	return(CMD_ERROR);
    }

    /* locate the file on the load path */
    cp = file_search(name);
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
    fp = file_alloc();
    fp->f_name = name;
    fp->f_type = strdup(type);
    fp->f_args = NULL;
    fp->f_metadata = NULL;
    fp->f_loader = -1;
    fp->f_addr = loadaddr;
    fp->f_size = laddr - loadaddr;

    /* recognise space consumption */
    loadaddr = laddr;

    /* Add to the list of loaded files */
    file_insert_tail(fp);
    close(fd);
    return(CMD_OK);
}

/*
 * Load the module (name), pass it (argc),(argv), add container file
 * to the list of loaded files.
 * If module is already loaded just assign new argc/argv.
 */
int
mod_load(char *modname, int argc, char *argv[])
{
    struct preloaded_file	*fp, *last_file;
    struct kernel_module	*mp;
    int				err;
    char			*filename;

    /* see if module is already loaded */
    mp = file_findmodule(NULL, modname);
    if (mp) {
#ifdef moduleargs
	if (mp->m_args)
	    free(mp->m_args);
	mp->m_args = unargv(argc, argv);
#endif
	sprintf(command_errbuf, "warning: module '%s' already loaded", mp->m_name);
	return (0);
    }
    /* locate file with the module on the search path */
    filename = mod_searchmodule(modname);
    if (filename == NULL) {
	sprintf(command_errbuf, "can't find '%s'", modname);
	return (ENOENT);
    }
    for (last_file = preloaded_files; 
	 last_file != NULL && last_file->f_next != NULL;
	 last_file = last_file->f_next)
	;

    fp = NULL;
    do {
	err = file_load(filename, loadaddr, &fp);
	if (err)
	    break;
#ifdef moduleargs
	mp = file_findmodule(fp, modname);
	if (mp == NULL) {
	    sprintf(command_errbuf, "module '%s' not found in the file '%s': %s",
		modname, filename, strerror(err));
	    err = ENOENT;
	    break;
	}
	mp->m_args = unargv(argc, argv);
#else
	fp->f_args = unargv(argc, argv);
#endif
	loadaddr = fp->f_addr + fp->f_size;
	file_insert_tail(fp);		/* Add to the list of loaded files */
	if (file_load_dependancies(fp) != 0) {
	    err = ENOENT;
	    last_file->f_next = NULL;
	    loadaddr = last_file->f_addr + last_file->f_size;
	    fp = NULL;
	    break;
	}
    } while(0);
    if (err == EFTYPE)
	sprintf(command_errbuf, "don't know how to load module '%s'", filename);
    if (err && fp)
	file_discard(fp);
    free(filename);
    return (err);
}

/*
 * Find a file matching (name) and (type).
 * NULL may be passed as a wildcard to either.
 */
struct preloaded_file *
file_findfile(char *name, char *type)
{
    struct preloaded_file *fp;

    for (fp = preloaded_files; fp != NULL; fp = fp->f_next) {
	if (((name == NULL) || !strcmp(name, fp->f_name)) &&
	    ((type == NULL) || !strcmp(type, fp->f_type)))
	    break;
    }
    return (fp);
}

/*
 * Find a module matching (name) inside of given file.
 * NULL may be passed as a wildcard.
 */
struct kernel_module *
file_findmodule(struct preloaded_file *fp, char *modname)
{
    struct kernel_module *mp;

    if (fp == NULL) {
	for (fp = preloaded_files; fp; fp = fp->f_next) {
	    for (mp = fp->f_modules; mp; mp = mp->m_next) {
    		if (strcmp(modname, mp->m_name) == 0)
		    return (mp);
	    }
	}
	return (NULL);
    }
    for (mp = fp->f_modules; mp; mp = mp->m_next) {
        if (strcmp(modname, mp->m_name) == 0)
	    return (mp);
    }
    return (NULL);
}
/*
 * Make a copy of (size) bytes of data from (p), and associate them as
 * metadata of (type) to the module (mp).
 */
void
file_addmetadata(struct preloaded_file *fp, int type, size_t size, void *p)
{
    struct file_metadata	*md;

    md = malloc(sizeof(struct file_metadata) - sizeof(md->md_data) + size);
    md->md_size = size;
    md->md_type = type;
    bcopy(p, md->md_data, size);
    md->md_next = fp->f_metadata;
    fp->f_metadata = md;
}

/*
 * Find a metadata object of (type) associated with the file (fp)
 */
struct file_metadata *
file_findmetadata(struct preloaded_file *fp, int type)
{
    struct file_metadata *md;

    for (md = fp->f_metadata; md != NULL; md = md->md_next)
	if (md->md_type == type)
	    break;
    return(md);
}

struct file_metadata *
metadata_next(struct file_metadata *md, int type)
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
file_search(char *name)
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
    result = file_search(tn);
    free(tn);
    /* Look for just (name) (useful for finding kernels) */
    if (result == NULL)
	result = file_search(name);

    return(result);
}

int
file_addmodule(struct preloaded_file *fp, char *modname,
	struct kernel_module **newmp)
{
    struct kernel_module *mp;

    mp = file_findmodule(fp, modname);
    if (mp)
	return (EEXIST);
    mp = malloc(sizeof(struct kernel_module));
    if (mp == NULL)
	return (ENOMEM);
    bzero(mp, sizeof(struct kernel_module));
    mp->m_name = strdup(modname);
    mp->m_fp = fp;
    mp->m_next = fp->f_modules;
    fp->f_modules = mp;
    if (newmp)
	*newmp = mp;
    return (0);
}

/*
 * Throw a file away
 */
void
file_discard(struct preloaded_file *fp)
{
    struct file_metadata	*md, *md1;
    struct kernel_module	*mp, *mp1;
    if (fp == NULL)
	return;
    md = fp->f_metadata;
    while (md) {
	md1 = md;
	md = md->md_next;
	free(md1);
    }
    mp = fp->f_modules;
    while (mp) {
	if (mp->m_name)
	    free(mp->m_name);
	mp1 = mp;
	mp = mp->m_next;
	free(mp1);
    }	
    if (fp->f_name != NULL)
	free(fp->f_name);
    if (fp->f_type != NULL)
        free(fp->f_type);
    if (fp->f_args != NULL)
        free(fp->f_args);
    free(fp);
}

/*
 * Allocate a new file; must be used instead of malloc()
 * to ensure safe initialisation.
 */
struct preloaded_file *
file_alloc(void)
{
    struct preloaded_file	*fp;
    
    if ((fp = malloc(sizeof(struct preloaded_file))) != NULL) {
	bzero(fp, sizeof(struct preloaded_file));
    }
    return (fp);
}

/*
 * Add a module to the chain
 */
static void
file_insert_tail(struct preloaded_file *fp)
{
    struct preloaded_file	*cm;
    
    /* Append to list of loaded file */
    fp->f_next = NULL;
    if (preloaded_files == NULL) {
	preloaded_files = fp;
    } else {
	for (cm = preloaded_files; cm->f_next != NULL; cm = cm->f_next)
	    ;
	cm->f_next = fp;
    }
}

