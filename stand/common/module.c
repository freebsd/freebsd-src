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

/*
 * file/module function dispatcher, support, etc.
 */

#include <stand.h>
#include <string.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/stdint.h>
#include <sys/font.h>
#include <gfx_fb.h>

#if defined(LOADER_FDT_SUPPORT)
#include <fdt_platform.h>
#endif

#include "bootstrap.h"

#define	MDIR_REMOVED	0x0001
#define	MDIR_NOHINTS	0x0002

struct moduledir {
	char	*d_path;	/* path of modules directory */
	u_char	*d_hints;	/* content of linker.hints file */
	int	d_hintsz;	/* size of hints data */
	int	d_flags;
	STAILQ_ENTRY(moduledir) d_link;
};

static int			file_load(char *filename, vm_offset_t dest, struct preloaded_file **result);
static int			file_load_dependencies(struct preloaded_file *base_mod);
static char *			file_search(const char *name, char **extlist);
static struct kernel_module *	file_findmodule(struct preloaded_file *fp, char *modname, struct mod_depend *verinfo);
static int			file_havepath(const char *name);
static char			*mod_searchmodule(char *name, struct mod_depend *verinfo);
static char *			mod_searchmodule_pnpinfo(const char *bus, const char *pnpinfo);
static void			file_insert_tail(struct preloaded_file *mp);
static void			file_remove(struct preloaded_file *fp);
struct file_metadata*		metadata_next(struct file_metadata *base_mp, int type);
static void			moduledir_readhints(struct moduledir *mdp);
static void			moduledir_rebuild(void);

/* load address should be tweaked by first module loaded (kernel) */
static vm_offset_t	loadaddr = 0;

#if defined(LOADER_FDT_SUPPORT)
static const char	*default_searchpath =
    "/boot/kernel;/boot/modules;/boot/dtb";
#else
static const char	*default_searchpath = "/boot/kernel;/boot/modules";
#endif

static STAILQ_HEAD(, moduledir) moduledir_list =
    STAILQ_HEAD_INITIALIZER(moduledir_list);

struct preloaded_file *preloaded_files = NULL;

static char *kld_ext_list[] = {
    ".ko",
    "",
    ".debug",
    NULL
};

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
	struct preloaded_file *fp;
	char	*typestr;
#ifdef LOADER_VERIEXEC
	char	*prefix, *skip;
#endif
	int		dflag, dofile, dokld, ch, error;

	dflag = dokld = dofile = 0;
	optind = 1;
	optreset = 1;
	typestr = NULL;
	if (argc == 1) {
		command_errmsg = "no filename specified";
		return (CMD_CRIT);
	}
#ifdef LOADER_VERIEXEC
	prefix = NULL;
	skip = NULL;
#endif
	while ((ch = getopt(argc, argv, "dkp:s:t:")) != -1) {
		switch(ch) {
		case 'd':
			dflag++;
			break;
		case 'k':
			dokld = 1;
			break;
#ifdef LOADER_VERIEXEC
		case 'p':
			prefix = optarg;
			break;
		case 's':
			skip = optarg;
			break;
#endif
		case 't':
			typestr = optarg;
			dofile = 1;
			break;
		case '?':
		default:
			/* getopt has already reported an error */
			return (CMD_OK);
		}
	}
	argv += (optind - 1);
	argc -= (optind - 1);

	/*
	 * Request to load a raw file?
	 */
	if (dofile) {
		if ((argc != 2) || (typestr == NULL) || (*typestr == 0)) {
			command_errmsg = "invalid load type";
			return (CMD_CRIT);
		}

#ifdef LOADER_VERIEXEC
		if (strncmp(typestr, "manifest", 8) == 0) {
			if (dflag > 0)
				ve_debug_set(dflag);
			return (load_manifest(argv[1], prefix, skip, NULL));
		}
#ifdef LOADER_VERIEXEC_PASS_MANIFEST
		if (strncmp(typestr, "pass_manifest", 13) == 0) {
			if (dflag > 0)
				ve_debug_set(dflag);
		    return (pass_manifest(argv[1], prefix));
		}
#endif
#endif

		fp = file_findfile(argv[1], typestr);
		if (fp) {
			snprintf(command_errbuf, sizeof(command_errbuf),
			  "warning: file '%s' already loaded", argv[1]);
			return (CMD_WARN);
		}

		if (file_loadraw(argv[1], typestr, 1) != NULL)
			return (CMD_OK);

		/* Failing to load mfs_root is never going to end well! */
		if (strcmp("mfs_root", typestr) == 0)
			return (CMD_FATAL);

		return (CMD_ERROR);
	}
	/*
	 * Do we have explicit KLD load ?
	 */
	if (dokld || file_havepath(argv[1])) {
		error = mod_loadkld(argv[1], argc - 2, argv + 2);
		if (error == EEXIST) {
			snprintf(command_errbuf, sizeof(command_errbuf),
			  "warning: KLD '%s' already loaded", argv[1]);
			return (CMD_WARN);
		}
	
		return (error == 0 ? CMD_OK : CMD_CRIT);
	}
	/*
	 * Looks like a request for a module.
	 */
	error = mod_load(argv[1], NULL, argc - 2, argv + 2);
	if (error == EEXIST) {
		snprintf(command_errbuf, sizeof(command_errbuf),
		  "warning: module '%s' already loaded", argv[1]);
		return (CMD_WARN);
	}

	return (error == 0 ? CMD_OK : CMD_CRIT);
}

#ifdef LOADER_GELI_SUPPORT
COMMAND_SET(load_geli, "load_geli", "load a geli key", command_load_geli);

static int
command_load_geli(int argc, char *argv[])
{
	char	typestr[80];
	char	*cp;
	int		ch, num;

	if (argc < 3) {
		command_errmsg = "usage is [-n key#] <prov> <file>";
		return(CMD_ERROR);
	}

	num = 0;
	optind = 1;
	optreset = 1;
	while ((ch = getopt(argc, argv, "n:")) != -1) {
		switch(ch) {
		case 'n':
			num = strtol(optarg, &cp, 0);
			if (cp == optarg) {
				snprintf(command_errbuf, sizeof(command_errbuf),
				  "bad key index '%s'", optarg);
				return(CMD_ERROR);
			}
			break;
		case '?':
		default:
			/* getopt has already reported an error */
			return(CMD_OK);
		}
	}
	argv += (optind - 1);
	argc -= (optind - 1);
	sprintf(typestr, "%s:geli_keyfile%d", argv[1], num);
	return (file_loadraw(argv[2], typestr, 1) ? CMD_OK : CMD_ERROR);
}
#endif

void
unload(void)
{
	struct preloaded_file *fp;

	while (preloaded_files != NULL) {
		fp = preloaded_files;
		preloaded_files = preloaded_files->f_next;
		file_discard(fp);
	}
	loadaddr = 0;
	unsetenv("kernelname");
}

COMMAND_SET(unload, "unload", "unload all modules", command_unload);

static int
command_unload(int argc, char *argv[])
{
	unload();
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
	int				ch, verbose, ret = 0;

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
		snprintf(lbuf, sizeof(lbuf), " %p: ", (void *) fp->f_addr);
		pager_output(lbuf);
		pager_output(fp->f_name);
		snprintf(lbuf, sizeof(lbuf), " (%s, 0x%lx)\n", fp->f_type,
		  (long)fp->f_size);
		if (pager_output(lbuf))
			break;
		if (fp->f_args != NULL) {
			pager_output("    args: ");
			pager_output(fp->f_args);
			if (pager_output("\n"))
				break;
		}
		if (fp->f_modules) {
			pager_output("  modules: ");
			for (mp = fp->f_modules; mp; mp = mp->m_next) {
				snprintf(lbuf, sizeof(lbuf), "%s.%d ", mp->m_name,
				  mp->m_version);
				pager_output(lbuf);
			}
			if (pager_output("\n"))
				break;
		}
		if (verbose) {
			/* XXX could add some formatting smarts here to display some better */
			for (md = fp->f_metadata; md != NULL; md = md->md_next) {
				snprintf(lbuf, sizeof(lbuf), "      0x%04x, 0x%lx\n",
				  md->md_type, (long) md->md_size);
				if (pager_output(lbuf))
					break;
			}
		}
		if (ret)
			break;
	}
	pager_close();
	return(CMD_OK);
}

COMMAND_SET(pnpmatch, "pnpmatch", "list matched modules based on pnpinfo", command_pnpmatch);

static int pnp_dump_flag = 0;
static int pnp_unbound_flag = 0;
static int pnp_verbose_flag = 0;

static int
command_pnpmatch(int argc, char *argv[])
{
	char *module;
	int ch;

	pnp_verbose_flag = 0;
	pnp_dump_flag = 0;
	optind = 1;
	optreset = 1;
	while ((ch = getopt(argc, argv, "vd")) != -1) {
		switch(ch) {
		case 'v':
			pnp_verbose_flag = 1;
			break;
		case 'd':
			pnp_dump_flag = 1;
			break;
		case '?':
		default:
			/* getopt has already reported an error */
			return(CMD_OK);
		}
	}
	argv += optind;
	argc -= optind;

	if (argc != 2) {
		command_errmsg = "Usage: pnpmatch <busname> compat=<compatdata>";
		return (CMD_CRIT);
	}

	module = mod_searchmodule_pnpinfo(argv[0], argv[1]);
	if (module)
		printf("Matched module: %s\n", module);
	else
		printf("No module matches %s on bus %s\n", argv[1], argv[0]);

	return (CMD_OK);
}

COMMAND_SET(pnpload, "pnpload", "load matched modules based on pnpinfo", command_pnpload);

static int
command_pnpload(int argc, char *argv[])
{
	char *module;
	int ch, error;

	pnp_verbose_flag = 0;
	pnp_dump_flag = 0;
	optind = 1;
	optreset = 1;
	while ((ch = getopt(argc, argv, "vd")) != -1) {
		switch(ch) {
		case 'v':
			pnp_verbose_flag = 1;
			break;
		case 'd':
			pnp_dump_flag = 1;
			break;
		case '?':
		default:
			/* getopt has already reported an error */
			return(CMD_OK);
		}
	}
	argv += optind;
	argc -= optind;

	if (argc != 2) {
		command_errmsg = "Usage: pnpload <busname> compat=<compatdata>";
		return (CMD_ERROR);
	}

	module = mod_searchmodule_pnpinfo(argv[0], argv[1]);

	error = mod_load(module, NULL, 0, NULL);
	if (error == EEXIST) {
		snprintf(command_errbuf, sizeof(command_errbuf),
		  "warning: module '%s' already loaded", argv[1]);
		return (CMD_WARN);
	}

	return (error == 0 ? CMD_OK : CMD_CRIT);
}

#if defined(LOADER_FDT_SUPPORT)
static void
pnpautoload_fdt_bus(const char *busname) {
	const char *pnpstring;
	const char *compatstr;
	char *pnpinfo = NULL;
	char *module = NULL;
	int tag = 0, len, pnplen;
	int error;

	while (1) {
		pnpstring = fdt_devmatch_next(&tag, &len);
		if (pnpstring == NULL)
			return;

		compatstr = pnpstring;
		for (pnplen = 0; pnplen != len; compatstr = pnpstring + pnplen) {
			pnplen += strlen(compatstr) + 1;
			asprintf(&pnpinfo, "compat=%s", compatstr);

			module = mod_searchmodule_pnpinfo(busname, pnpinfo);
			if (module) {
				error = mod_loadkld(module, 0, NULL);
				if (error)
					printf("Cannot load module %s\n", module);
				break;
			}
		}
		free(pnpinfo);
		free(module);
	}
}
#endif

struct pnp_bus {
	const char *name;
	void (*load)(const char *busname);
};

struct pnp_bus pnp_buses[] = {
#if defined(LOADER_FDT_SUPPORT)
	{"simplebus", pnpautoload_fdt_bus},
	{"ofwbus", pnpautoload_fdt_bus},
	{"iicbus", pnpautoload_fdt_bus},
	{"spibus", pnpautoload_fdt_bus},
#endif
};

COMMAND_SET(pnpautoload, "pnpautoload", "auto load modules based on pnpinfo", command_pnpautoload);

static int
command_pnpautoload(int argc, char *argv[])
{
	int i;
	int verbose;
	int ch, match;

	pnp_verbose_flag = 0;
	pnp_dump_flag = 0;
	verbose = 0;
	optind = 1;
	optreset = 1;
	match = 0;
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
	argv += (optind - 1);
	argc -= (optind - 1);

	if (argc > 2)
		return (CMD_ERROR);

	for (i = 0; i < nitems(pnp_buses); i++) {
		if (argc == 2 && strcmp(argv[1], pnp_buses[i].name) != 0) {
			if (verbose)
				printf("Skipping bus %s\n", pnp_buses[i].name);
			continue;
		}
		if (verbose)
			printf("Autoloading modules for %s\n", pnp_buses[i].name);
		pnp_buses[i].load(pnp_buses[i].name);
		match = 1;
	}
	if (match == 0)
		printf("Unsupported bus %s\n", argv[1]);

	return (CMD_OK);
}

/*
 * File level interface, functions file_*
 */
int
file_load(char *filename, vm_offset_t dest, struct preloaded_file **result)
{
	static int last_file_format = 0;
	struct preloaded_file *fp;
	int error;
	int i;

	if (archsw.arch_loadaddr != NULL)
		dest = archsw.arch_loadaddr(LOAD_RAW, filename, dest);

	error = EFTYPE;
	for (i = last_file_format, fp = NULL;
	     file_formats[i] && fp == NULL; i++) {
		error = (file_formats[i]->l_load)(filename, dest, &fp);
		if (error == 0) {
			fp->f_loader = last_file_format = i; /* remember the loader */
			*result = fp;
			break;
		} else if (last_file_format == i && i != 0) {
			/* Restart from the beginning */
			i = -1;
			last_file_format = 0;
			fp = NULL;
			continue;
		}
		if (error == EFTYPE)
			continue;		/* Unknown to this handler? */
		if (error) {
			snprintf(command_errbuf, sizeof(command_errbuf),
			  "can't load file '%s': %s", filename, strerror(error));
			break;
		}
	}
	return (error);
}

static int
file_load_dependencies(struct preloaded_file *base_file)
{
	struct file_metadata *md;
	struct preloaded_file *fp;
	struct mod_depend *verinfo;
	struct kernel_module *mp;
	char *dmodname;
	int error;

	md = file_findmetadata(base_file, MODINFOMD_DEPLIST);
	if (md == NULL)
		return (0);
	error = 0;
	do {
		verinfo = (struct mod_depend*)md->md_data;
		dmodname = (char *)(verinfo + 1);
		if (file_findmodule(NULL, dmodname, verinfo) == NULL) {
			printf("loading required module '%s'\n", dmodname);
			error = mod_load(dmodname, verinfo, 0, NULL);
			if (error)
				break;
			/*
			 * If module loaded via kld name which isn't listed
			 * in the linker.hints file, we should check if it have
			 * required version.
			 */
			mp = file_findmodule(NULL, dmodname, verinfo);
			if (mp == NULL) {
				snprintf(command_errbuf, sizeof(command_errbuf),
				  "module '%s' exists but with wrong version", dmodname);
				error = ENOENT;
				break;
			}
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

vm_offset_t
build_font_module(vm_offset_t addr)
{
	vt_font_bitmap_data_t *bd;
	struct vt_font *fd;
	struct preloaded_file *fp;
	size_t size;
	uint32_t checksum;
	int i;
	struct font_info fi;
	struct fontlist *fl;
	uint64_t fontp;

	if (STAILQ_EMPTY(&fonts))
		return (addr);

	/* We can't load first */
	if ((file_findfile(NULL, NULL)) == NULL) {
		printf("Can not load font module: %s\n",
		    "the kernel is not loaded");
		return (addr);
	}

	/* helper pointers */
	bd = NULL;
	STAILQ_FOREACH(fl, &fonts, font_next) {
		if (gfx_state.tg_font.vf_width == fl->font_data->vfbd_width &&
		    gfx_state.tg_font.vf_height == fl->font_data->vfbd_height) {
			/*
			 * Kernel does have better built in font.
			 */
			if (fl->font_flags == FONT_BUILTIN)
				return (addr);

			bd = fl->font_data;
			break;
		}
	}
	if (bd == NULL)
		return (addr);
	fd = bd->vfbd_font;

	fi.fi_width = fd->vf_width;
	checksum = fi.fi_width;
	fi.fi_height = fd->vf_height;
	checksum += fi.fi_height;
	fi.fi_bitmap_size = bd->vfbd_uncompressed_size;
	checksum += fi.fi_bitmap_size;

	size = roundup2(sizeof (struct font_info), 8);
	for (i = 0; i < VFNT_MAPS; i++) {
		fi.fi_map_count[i] = fd->vf_map_count[i];
		checksum += fi.fi_map_count[i];
		size += fd->vf_map_count[i] * sizeof (struct vfnt_map);
		size += roundup2(size, 8);
	}
	size += bd->vfbd_uncompressed_size;

	fi.fi_checksum = -checksum;

	fp = file_findfile(NULL, "elf kernel");
	if (fp == NULL)
		fp = file_findfile(NULL, "elf64 kernel");
	if (fp == NULL)
		panic("can't find kernel file");

	fontp = addr;
	addr += archsw.arch_copyin(&fi, addr, sizeof (struct font_info));
	addr = roundup2(addr, 8);

	/* Copy maps. */
	for (i = 0; i < VFNT_MAPS; i++) {
		if (fd->vf_map_count[i] != 0) {
			addr += archsw.arch_copyin(fd->vf_map[i], addr,
			    fd->vf_map_count[i] * sizeof (struct vfnt_map));
			addr = roundup2(addr, 8);
		}
	}

	/* Copy the bitmap. */
	addr += archsw.arch_copyin(fd->vf_bytes, addr, fi.fi_bitmap_size);

	/* Looks OK so far; populate control structure */
	file_addmetadata(fp, MODINFOMD_FONT, sizeof(fontp), &fontp);
	return (addr);
}

#ifdef LOADER_VERIEXEC_VECTX
#define VECTX_HANDLE(fd) vctx
#else
#define VECTX_HANDLE(fd) fd
#endif


/*
 * We've been asked to load (fname) as (type), so just suck it in,
 * no arguments or anything.
 */
struct preloaded_file *
file_loadraw(const char *fname, char *type, int insert)
{
	struct preloaded_file	*fp;
	char			*name;
	int				fd, got;
	vm_offset_t			laddr;
#ifdef LOADER_VERIEXEC_VECTX
	struct vectx		*vctx;
	int			verror;
#endif

	/* We can't load first */
	if ((file_findfile(NULL, NULL)) == NULL) {
		command_errmsg = "can't load file before kernel";
		return(NULL);
	}

	/* locate the file on the load path */
	name = file_search(fname, NULL);
	if (name == NULL) {
		snprintf(command_errbuf, sizeof(command_errbuf),
		  "can't find '%s'", fname);
		return(NULL);
	}

	if ((fd = open(name, O_RDONLY)) < 0) {
		snprintf(command_errbuf, sizeof(command_errbuf),
		  "can't open '%s': %s", name, strerror(errno));
		free(name);
		return(NULL);
	}

#ifdef LOADER_VERIEXEC_VECTX
	vctx = vectx_open(fd, name, 0L, NULL, &verror, __func__);
	if (verror) {
		sprintf(command_errbuf, "can't verify '%s': %s",
		    name, ve_error_get());
		free(name);
		free(vctx);
		close(fd);
		return(NULL);
	}
#else
#ifdef LOADER_VERIEXEC
	if (verify_file(fd, name, 0, VE_MUST, __func__) < 0) {
		sprintf(command_errbuf, "can't verify '%s': %s",
		    name, ve_error_get());
		free(name);
		close(fd);
		return(NULL);
	}
#endif
#endif

	if (archsw.arch_loadaddr != NULL)
		loadaddr = archsw.arch_loadaddr(LOAD_RAW, name, loadaddr);

	printf("%s ", name);

	laddr = loadaddr;
	for (;;) {
		/* read in 4k chunks; size is not really important */
		got = archsw.arch_readin(VECTX_HANDLE(fd), laddr, 4096);
		if (got == 0)				/* end of file */
			break;
		if (got < 0) {				/* error */
			snprintf(command_errbuf, sizeof(command_errbuf),
			  "error reading '%s': %s", name, strerror(errno));
			free(name);
			close(fd);
#ifdef LOADER_VERIEXEC_VECTX
			free(vctx);
#endif
			return(NULL);
		}
		laddr += got;
	}

	printf("size=%#jx\n", (uintmax_t)(laddr - loadaddr));
#ifdef LOADER_VERIEXEC_VECTX
	verror = vectx_close(vctx, VE_MUST, __func__);
	if (verror) {
		free(name);
		close(fd);
		free(vctx);
		return(NULL);
	}
#endif

	/* Looks OK so far; create & populate control structure */
	fp = file_alloc();
	if (fp == NULL) {
		snprintf(command_errbuf, sizeof (command_errbuf),
		    "no memory to load %s", name);
		free(name);
		close(fd);
		return (NULL);
	}
	fp->f_name = name;
	fp->f_type = strdup(type);
	fp->f_args = NULL;
	fp->f_metadata = NULL;
	fp->f_loader = -1;
	fp->f_addr = loadaddr;
	fp->f_size = laddr - loadaddr;

	if (fp->f_type == NULL) {
		snprintf(command_errbuf, sizeof (command_errbuf),
		    "no memory to load %s", name);
		free(name);
		close(fd);
		return (NULL);
	}
	/* recognise space consumption */
	loadaddr = laddr;

	/* Add to the list of loaded files */
	if (insert != 0)
		file_insert_tail(fp);
	close(fd);
	return(fp);
}

/*
 * Load the module (name), pass it (argc),(argv), add container file
 * to the list of loaded files.
 * If module is already loaded just assign new argc/argv.
 */
int
mod_load(char *modname, struct mod_depend *verinfo, int argc, char *argv[])
{
	struct kernel_module	*mp;
	int				err;
	char			*filename;

	if (file_havepath(modname)) {
		printf("Warning: mod_load() called instead of mod_loadkld() for module '%s'\n", modname);
		return (mod_loadkld(modname, argc, argv));
	}
	/* see if module is already loaded */
	mp = file_findmodule(NULL, modname, verinfo);
	if (mp) {
#ifdef moduleargs
		free(mp->m_args);
		mp->m_args = unargv(argc, argv);
#endif
		snprintf(command_errbuf, sizeof(command_errbuf),
		  "warning: module '%s' already loaded", mp->m_name);
		return (0);
	}
	/* locate file with the module on the search path */
	filename = mod_searchmodule(modname, verinfo);
	if (filename == NULL) {
		snprintf(command_errbuf, sizeof(command_errbuf),
		  "can't find '%s'", modname);
		return (ENOENT);
	}
	err = mod_loadkld(filename, argc, argv);
	free(filename);
	return (err);
}

/*
 * Load specified KLD. If path is omitted, then try to locate it via
 * search path.
 */
int
mod_loadkld(const char *kldname, int argc, char *argv[])
{
	struct preloaded_file	*fp;
	int			err;
	char			*filename;
	vm_offset_t		loadaddr_saved;

	/*
	 * Get fully qualified KLD name
	 */
	filename = file_search(kldname, kld_ext_list);
	if (filename == NULL) {
		snprintf(command_errbuf, sizeof(command_errbuf),
		  "can't find '%s'", kldname);
		return (ENOENT);
	}
	/*
	 * Check if KLD already loaded
	 */
	fp = file_findfile(filename, NULL);
	if (fp) {
		snprintf(command_errbuf, sizeof(command_errbuf),
		  "warning: KLD '%s' already loaded", filename);
		free(filename);
		return (0);
	}

	do {
		err = file_load(filename, loadaddr, &fp);
		if (err)
			break;
		fp->f_args = unargv(argc, argv);
		loadaddr_saved = loadaddr;
		loadaddr = fp->f_addr + fp->f_size;
		file_insert_tail(fp);	/* Add to the list of loaded files */
		if (file_load_dependencies(fp) != 0) {
			err = ENOENT;
			file_remove(fp);
			loadaddr = loadaddr_saved;
			fp = NULL;
			break;
		}
	} while(0);
	if (err == EFTYPE) {
		snprintf(command_errbuf, sizeof(command_errbuf),
		  "don't know how to load module '%s'", filename);
	}
	if (err)
		file_discard(fp);
	free(filename);
	return (err);
}

/*
 * Find a file matching (name) and (type).
 * NULL may be passed as a wildcard to either.
 */
struct preloaded_file *
file_findfile(const char *name, const char *type)
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
file_findmodule(struct preloaded_file *fp, char *modname,
	struct mod_depend *verinfo)
{
	struct kernel_module *mp, *best;
	int bestver, mver;

	if (fp == NULL) {
		for (fp = preloaded_files; fp; fp = fp->f_next) {
			mp = file_findmodule(fp, modname, verinfo);
			if (mp)
				return (mp);
		}
		return (NULL);
	}
	best = NULL;
	bestver = 0;
	for (mp = fp->f_modules; mp; mp = mp->m_next) {
		if (strcmp(modname, mp->m_name) == 0) {
			if (verinfo == NULL)
				return (mp);
			mver = mp->m_version;
			if (mver == verinfo->md_ver_preferred)
				return (mp);
			if (mver >= verinfo->md_ver_minimum &&
			  mver <= verinfo->md_ver_maximum &&
			  mver > bestver) {
				best = mp;
				bestver = mver;
			}
		}
	}
	return (best);
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
	if (md != NULL) {
		md->md_size = size;
		md->md_type = type;
		bcopy(p, md->md_data, size);
		md->md_next = fp->f_metadata;
	}
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

/*
 * Remove all metadata from the file.
 */
void
file_removemetadata(struct preloaded_file *fp)
{
	struct file_metadata *md, *next;

	for (md = fp->f_metadata; md != NULL; md = next)
	{
		next = md->md_next;
		free(md);
	}
	fp->f_metadata = NULL;
}

/*
 * Add a buffer to the list of preloaded "files".
 */
int
file_addbuf(const char *name, const char *type, size_t len, void *buf)
{
	struct preloaded_file	*fp;
	vm_offset_t dest;

	/* We can't load first */
	if ((file_findfile(NULL, NULL)) == NULL) {
		command_errmsg = "can't load file before kernel";
		return (-1);
	}

	/* Figure out where to load the data. */
	dest = loadaddr;
	if (archsw.arch_loadaddr != NULL)
		dest = archsw.arch_loadaddr(LOAD_RAW, (void *)name, dest);

	/* Create & populate control structure */
	fp = file_alloc();
	if (fp == NULL) {
		snprintf(command_errbuf, sizeof (command_errbuf),
		    "no memory to load %s", name);
		return (-1);
	}
	fp->f_name = strdup(name);
	fp->f_type = strdup(type);
	fp->f_args = NULL;
	fp->f_metadata = NULL;
	fp->f_loader = -1;
	fp->f_addr = dest;
	fp->f_size = len;
	if ((fp->f_name == NULL) || (fp->f_type == NULL)) {
		snprintf(command_errbuf, sizeof (command_errbuf),
		    "no memory to load %s", name);
		free(fp->f_name);
		free(fp->f_type);
		return (-1);
	}

	/* Copy the data in. */
	archsw.arch_copyin(buf, fp->f_addr, len);
	loadaddr = fp->f_addr + len;

	/* Add to the list of loaded files */
	file_insert_tail(fp);
	return(0);
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

static char *emptyextlist[] = { "", NULL };

/*
 * Check if the given file is in place and return full path to it.
 */
static char *
file_lookup(const char *path, const char *name, int namelen, char **extlist)
{
	struct stat	st;
	char	*result, *cp, **cpp;
	int		pathlen, extlen, len;

	pathlen = strlen(path);
	extlen = 0;
	if (extlist == NULL)
		extlist = emptyextlist;
	for (cpp = extlist; *cpp; cpp++) {
		len = strlen(*cpp);
		if (len > extlen)
			extlen = len;
	}
	result = malloc(pathlen + namelen + extlen + 2);
	if (result == NULL)
		return (NULL);
	bcopy(path, result, pathlen);
	if (pathlen > 0 && result[pathlen - 1] != '/')
		result[pathlen++] = '/';
	cp = result + pathlen;
	bcopy(name, cp, namelen);
	cp += namelen;
	for (cpp = extlist; *cpp; cpp++) {
		strcpy(cp, *cpp);
		if (stat(result, &st) == 0 && S_ISREG(st.st_mode))
			return result;
	}
	free(result);
	return NULL;
}

/*
 * Check if file name have any qualifiers
 */
static int
file_havepath(const char *name)
{
	const char		*cp;

	archsw.arch_getdev(NULL, name, &cp);
	return (cp != name || strchr(name, '/') != NULL);
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
file_search(const char *name, char **extlist)
{
	struct moduledir	*mdp;
	struct stat		sb;
	char		*result;
	int			namelen;

	/* Don't look for nothing */
	if (name == NULL)
		return(NULL);

	if (*name == 0)
		return(strdup(name));

	if (file_havepath(name)) {
		/* Qualified, so just see if it exists */
		if (stat(name, &sb) == 0)
			return(strdup(name));
		return(NULL);
	}
	moduledir_rebuild();
	result = NULL;
	namelen = strlen(name);
	STAILQ_FOREACH(mdp, &moduledir_list, d_link) {
		result = file_lookup(mdp->d_path, name, namelen, extlist);
		if (result)
			break;
	}
	return(result);
}

#define	INT_ALIGN(base, ptr)	ptr = \
	(base) + roundup2((ptr) - (base), sizeof(int))

static char *
mod_search_hints(struct moduledir *mdp, const char *modname,
	struct mod_depend *verinfo)
{
	u_char	*cp, *recptr, *bufend, *best;
	char	*result;
	int		*intp, bestver, blen, clen, found, ival, modnamelen, reclen;

	moduledir_readhints(mdp);
	modnamelen = strlen(modname);
	found = 0;
	result = NULL;
	bestver = 0;
	if (mdp->d_hints == NULL)
		goto bad;
	recptr = mdp->d_hints;
	bufend = recptr + mdp->d_hintsz;
	clen = blen = 0;
	best = cp = NULL;
	while (recptr < bufend && !found) {
		intp = (int*)recptr;
		reclen = *intp++;
		ival = *intp++;
		cp = (u_char*)intp;
		switch (ival) {
		case MDT_VERSION:
			clen = *cp++;
			if (clen != modnamelen || bcmp(cp, modname, clen) != 0)
				break;
			cp += clen;
			INT_ALIGN(mdp->d_hints, cp);
			ival = *(int*)cp;
			cp += sizeof(int);
			clen = *cp++;
			if (verinfo == NULL || ival == verinfo->md_ver_preferred) {
				found = 1;
				break;
			}
			if (ival >= verinfo->md_ver_minimum &&
			  ival <= verinfo->md_ver_maximum &&
			  ival > bestver) {
				bestver = ival;
				best = cp;
				blen = clen;
			}
			break;
		default:
			break;
		}
		recptr += reclen + sizeof(int);
	}
	/*
	 * Finally check if KLD is in the place
	 */
	if (found)
		result = file_lookup(mdp->d_path, (const char *)cp, clen, NULL);
	else if (best)
		result = file_lookup(mdp->d_path, (const char *)best, blen, NULL);
bad:
	/*
	 * If nothing found or hints is absent - fallback to the old way
	 * by using "kldname[.ko]" as module name.
	 */
	if (!found && !bestver && result == NULL)
		result = file_lookup(mdp->d_path, modname, modnamelen, kld_ext_list);
	return result;
}

static int
getint(void **ptr)
{
	int *p = *ptr;
	int rv;

	p = (int *)roundup2((intptr_t)p, sizeof(int));
	rv = *p++;
	*ptr = p;
	return rv;
}

static void
getstr(void **ptr, char *val)
{
	int *p = *ptr;
	char *c = (char *)p;
	int len = *(uint8_t *)c;

	memcpy(val, c + 1, len);
	val[len] = 0;
	c += len + 1;
	*ptr = (void *)c;
}

static int
pnpval_as_int(const char *val, const char *pnpinfo)
{
	int rv;
	char key[256];
	char *cp;

	if (pnpinfo == NULL)
		return -1;

	cp = strchr(val, ';');
	key[0] = ' ';
	if (cp == NULL)
		strlcpy(key + 1, val, sizeof(key) - 1);
	else {
		memcpy(key + 1, val, cp - val);
		key[cp - val + 1] = '\0';
	}
	strlcat(key, "=", sizeof(key));
	if (strncmp(key + 1, pnpinfo, strlen(key + 1)) == 0)
		rv = strtol(pnpinfo + strlen(key + 1), NULL, 0);
	else {
		cp = strstr(pnpinfo, key);
		if (cp == NULL)
			rv = -1;
		else
			rv = strtol(cp + strlen(key), NULL, 0);
	}
	return rv;
}

static void
quoted_strcpy(char *dst, const char *src)
{
	char q = ' ';

	if (*src == '\'' || *src == '"')
		q = *src++;
	while (*src && *src != q)
		*dst++ = *src++; // XXX backtick quoting
	*dst++ = '\0';
	// XXX overflow
}

static char *
pnpval_as_str(const char *val, const char *pnpinfo)
{
	static char retval[256];
	char key[256];
	char *cp;

	if (pnpinfo == NULL) {
		*retval = '\0';
		return retval;
	}

	cp = strchr(val, ';');
	key[0] = ' ';
	if (cp == NULL)
		strlcpy(key + 1, val, sizeof(key) - 1);
	else {
		memcpy(key + 1, val, cp - val);
		key[cp - val + 1] = '\0';
	}
	strlcat(key, "=", sizeof(key));
	if (strncmp(key + 1, pnpinfo, strlen(key + 1)) == 0)
		quoted_strcpy(retval, pnpinfo + strlen(key + 1));
	else {
		cp = strstr(pnpinfo, key);
		if (cp == NULL)
			strcpy(retval, "MISSING");
		else
			quoted_strcpy(retval, cp + strlen(key));
	}
	return retval;
}

static char *
devmatch_search_hints(struct moduledir *mdp, const char *bus, const char *dev, const char *pnpinfo)
{
	char val1[256], val2[256];
	int ival, len, ents, i, notme, mask, bit, v, found;
	void *ptr, *walker, *hints_end;
	char *lastmod = NULL, *cp, *s;

	moduledir_readhints(mdp);
	found = 0;
	if (mdp->d_hints == NULL)
		goto bad;
	walker = mdp->d_hints;
	hints_end = walker + mdp->d_hintsz;
	while (walker < hints_end && !found) {
		len = getint(&walker);
		ival = getint(&walker);
		ptr = walker;
		switch (ival) {
		case MDT_VERSION:
			getstr(&ptr, val1);
			ival = getint(&ptr);
			getstr(&ptr, val2);
			if (pnp_dump_flag || pnp_verbose_flag)
				printf("Version: if %s.%d kmod %s\n", val1, ival, val2);
			break;
		case MDT_MODULE:
			getstr(&ptr, val1);
			getstr(&ptr, val2);
			if (lastmod)
				free(lastmod);
			lastmod = strdup(val2);
			if (pnp_dump_flag || pnp_verbose_flag)
				printf("module %s in %s\n", val1, val1);
			break;
		case MDT_PNP_INFO:
			if (!pnp_dump_flag && !pnp_unbound_flag && lastmod && strcmp(lastmod, "kernel") == 0)
				break;
			getstr(&ptr, val1);
			getstr(&ptr, val2);
			ents = getint(&ptr);
			if (pnp_dump_flag || pnp_verbose_flag)
				printf("PNP info for bus %s format %s %d entries (%s)\n",
				    val1, val2, ents, lastmod);
			if (strcmp(val1, "usb") == 0) {
				if (pnp_verbose_flag)
					printf("Treating usb as uhub -- bug in source table still?\n");
				strcpy(val1, "uhub");
			}
			if (bus && strcmp(val1, bus) != 0) {
				if (pnp_verbose_flag)
					printf("Skipped because table for bus %s, looking for %s\n",
					    val1, bus);
				break;
			}
			for (i = 0; i < ents; i++) {
				if (pnp_verbose_flag)
					printf("---------- Entry %d ----------\n", i);
				if (pnp_dump_flag)
					printf("   ");
				cp = val2;
				notme = 0;
				mask = -1;
				bit = -1;
				do {
					switch (*cp) {
						/* All integer fields */
					case 'I':
					case 'J':
					case 'G':
					case 'L':
					case 'M':
						ival = getint(&ptr);
						if (pnp_dump_flag) {
							printf("%#x:", ival);
							break;
						}
						if (bit >= 0 && ((1 << bit) & mask) == 0)
							break;
						v = pnpval_as_int(cp + 2, pnpinfo);
						if (pnp_verbose_flag)
							printf("Matching %s (%c) table=%#x tomatch=%#x\n",
							    cp + 2, *cp, v, ival);
						switch (*cp) {
						case 'J':
							if (ival == -1)
								break;
							/*FALLTHROUGH*/
						case 'I':
							if (v != ival)
								notme++;
							break;
						case 'G':
							if (v < ival)
								notme++;
							break;
						case 'L':
							if (v > ival)
								notme++;
							break;
						case 'M':
							mask = ival;
							break;
						}
						break;
						/* String fields */
					case 'D':
					case 'Z':
						getstr(&ptr, val1);
						if (pnp_dump_flag) {
							printf("'%s':", val1);
							break;
						}
						if (*cp == 'D')
							break;
						s = pnpval_as_str(cp + 2, pnpinfo);
						if (strcmp(s, val1) != 0)
							notme++;
						break;
						/* Key override fields, required to be last in the string */
					case 'T':
						/*
						 * This is imperfect and only does one key and will be redone
						 * to be more general for multiple keys. Currently, nothing
						 * does that.
						 */
						if (pnp_dump_flag)				/* No per-row data stored */
							break;
						if (cp[strlen(cp) - 1] == ';')		/* Skip required ; at end */
							cp[strlen(cp) - 1] = '\0';	/* in case it's not there */
						if ((s = strstr(pnpinfo, cp + 2)) == NULL)
							notme++;
						else if (s > pnpinfo && s[-1] != ' ')
							notme++;
						break;
					default:
						printf("Unknown field type %c\n:", *cp);
						break;
					}
					bit++;
					cp = strchr(cp, ';');
					if (cp)
						cp++;
				} while (cp && *cp);
				if (pnp_dump_flag)
					printf("\n");
				else if (!notme) {
					if (!pnp_unbound_flag) {
						if (pnp_verbose_flag)
							printf("Matches --- %s ---\n", lastmod);
					}
					found++;
				}
			}
			break;
		default:
			break;
		}
		walker = (void *)(len - sizeof(int) + (intptr_t)walker);
	}
	if (pnp_unbound_flag && found == 0 && *pnpinfo) {
		if (pnp_verbose_flag)
			printf("------------------------- ");
		printf("%s on %s pnpinfo %s", *dev ? dev : "unattached", bus, pnpinfo);
		if (pnp_verbose_flag)
			printf(" -------------------------");
		printf("\n");
	}
	if (found != 0)
		return (lastmod);
	free(lastmod);

bad:
	return (NULL);
}

/*
 * Attempt to locate the file containing the module (name)
 */
static char *
mod_searchmodule(char *name, struct mod_depend *verinfo)
{
	struct	moduledir *mdp;
	char	*result;

	moduledir_rebuild();
	/*
	 * Now we ready to lookup module in the given directories
	 */
	result = NULL;
	STAILQ_FOREACH(mdp, &moduledir_list, d_link) {
		result = mod_search_hints(mdp, name, verinfo);
		if (result)
			break;
	}

	return(result);
}

static char *
mod_searchmodule_pnpinfo(const char *bus, const char *pnpinfo)
{
	struct	moduledir *mdp;
	char	*result;

	moduledir_rebuild();
	/*
	 * Now we ready to lookup module in the given directories
	 */
	result = NULL;
	STAILQ_FOREACH(mdp, &moduledir_list, d_link) {
		result = devmatch_search_hints(mdp, bus, NULL, pnpinfo);
		if (result)
			break;
	}

	return(result);
}

int
file_addmodule(struct preloaded_file *fp, char *modname, int version,
	struct kernel_module **newmp)
{
	struct kernel_module *mp;
	struct mod_depend mdepend;

	bzero(&mdepend, sizeof(mdepend));
	mdepend.md_ver_preferred = version;
	mp = file_findmodule(fp, modname, &mdepend);
	if (mp)
		return (EEXIST);
	mp = calloc(1, sizeof(struct kernel_module));
	if (mp == NULL)
		return (ENOMEM);
	mp->m_name = strdup(modname);
	if (mp->m_name == NULL) {
		free(mp);
		return (ENOMEM);
	}
	mp->m_version = version;
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
		free(mp->m_name);
		mp1 = mp;
		mp = mp->m_next;
		free(mp1);
	}
	free(fp->f_name);
	free(fp->f_type);
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

	return (calloc(1, sizeof(struct preloaded_file)));
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

/*
 * Remove module from the chain
 */
static void
file_remove(struct preloaded_file *fp)
{
	struct preloaded_file   *cm;

	if (preloaded_files == NULL)
		return;

	if (preloaded_files == fp) {
		preloaded_files = fp->f_next;
		return;
        }
        for (cm = preloaded_files; cm->f_next != NULL; cm = cm->f_next) {
		if (cm->f_next == fp) {
			cm->f_next = fp->f_next;
			return;
		}
	}
}

static char *
moduledir_fullpath(struct moduledir *mdp, const char *fname)
{
	char *cp;

	cp = malloc(strlen(mdp->d_path) + strlen(fname) + 2);
	if (cp == NULL)
		return NULL;
	strcpy(cp, mdp->d_path);
	strcat(cp, "/");
	strcat(cp, fname);
	return (cp);
}

/*
 * Read linker.hints file into memory performing some sanity checks.
 */
static void
moduledir_readhints(struct moduledir *mdp)
{
	struct stat	st;
	char	*path;
	int		fd, size, version;

	if (mdp->d_hints != NULL || (mdp->d_flags & MDIR_NOHINTS))
		return;
	path = moduledir_fullpath(mdp, "linker.hints");
	if (stat(path, &st) != 0 ||
	  st.st_size < (ssize_t)(sizeof(version) + sizeof(int)) ||
	  st.st_size > LINKER_HINTS_MAX || (fd = open(path, O_RDONLY)) < 0) {
		free(path);
		mdp->d_flags |= MDIR_NOHINTS;
		return;
	}
	free(path);
	size = read(fd, &version, sizeof(version));
	if (size != sizeof(version) || version != LINKER_HINTS_VERSION)
		goto bad;
	size = st.st_size - size;
	mdp->d_hints = malloc(size);
	if (mdp->d_hints == NULL)
		goto bad;
	if (read(fd, mdp->d_hints, size) != size)
		goto bad;
	mdp->d_hintsz = size;
	close(fd);
	return;
bad:
	close(fd);
	free(mdp->d_hints);
	mdp->d_hints = NULL;
	mdp->d_flags |= MDIR_NOHINTS;
	return;
}

/*
 * Extract directories from the ';' separated list, remove duplicates.
 */
static void
moduledir_rebuild(void)
{
	struct	moduledir *mdp, *mtmp;
	const char	*path, *cp, *ep;
	size_t	cplen;

	path = getenv("module_path");
	if (path == NULL)
		path = default_searchpath;
	/*
	 * Rebuild list of module directories if it changed
	 */
	STAILQ_FOREACH(mdp, &moduledir_list, d_link)
		mdp->d_flags |= MDIR_REMOVED;

	for (ep = path; *ep != 0;  ep++) {
		cp = ep;
		for (; *ep != 0 && *ep != ';'; ep++)
			;
		/*
		 * Ignore trailing slashes
		 */
		for (cplen = ep - cp; cplen > 1 && cp[cplen - 1] == '/'; cplen--)
			;
		STAILQ_FOREACH(mdp, &moduledir_list, d_link) {
			if (strlen(mdp->d_path) != cplen ||	bcmp(cp, mdp->d_path, cplen) != 0)
				continue;
			mdp->d_flags &= ~MDIR_REMOVED;
			break;
		}
		if (mdp == NULL) {
			mdp = malloc(sizeof(*mdp) + cplen + 1);
			if (mdp == NULL)
				return;
			mdp->d_path = (char*)(mdp + 1);
			bcopy(cp, mdp->d_path, cplen);
			mdp->d_path[cplen] = 0;
			mdp->d_hints = NULL;
			mdp->d_flags = 0;
			STAILQ_INSERT_TAIL(&moduledir_list, mdp, d_link);
		}
		if (*ep == 0)
			break;
	}
	/*
	 * Delete unused directories if any
	 */
	mdp = STAILQ_FIRST(&moduledir_list);
	while (mdp) {
		if ((mdp->d_flags & MDIR_REMOVED) == 0) {
			mdp = STAILQ_NEXT(mdp, d_link);
		} else {
			free(mdp->d_hints);
			mtmp = mdp;
			mdp = STAILQ_NEXT(mdp, d_link);
			STAILQ_REMOVE(&moduledir_list, mtmp, moduledir, d_link);
			free(mtmp);
		}
	}
	return;
}
