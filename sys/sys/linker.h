/*-
 * Copyright (c) 1997-2000 Doug Rabson
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
 * $FreeBSD: src/sys/sys/linker.h,v 1.36 2003/05/01 03:31:17 peter Exp $
 */

#ifndef _SYS_LINKER_H_
#define _SYS_LINKER_H_

#ifdef _KERNEL

#include <machine/elf.h>
#include <sys/kobj.h>

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_LINKER);
#endif

struct mod_depend;

/*
 * Object representing a file which has been loaded by the linker.
 */
typedef struct linker_file* linker_file_t;
typedef TAILQ_HEAD(, linker_file) linker_file_list_t;

typedef caddr_t linker_sym_t;		/* opaque symbol */
typedef c_caddr_t c_linker_sym_t;	/* const opaque symbol */
typedef int (*linker_function_name_callback_t)(const char *, void *);

/*
 * expanded out linker_sym_t
 */
typedef struct linker_symval {
    const char*		name;
    caddr_t		value;
    size_t		size;
} linker_symval_t;

struct common_symbol {
    STAILQ_ENTRY(common_symbol) link;
    char*		name;
    caddr_t		address;
};

struct linker_file {
    KOBJ_FIELDS;
    int			refs;		/* reference count */
    int			userrefs;	/* kldload(2) count */
    int			flags;
#define LINKER_FILE_LINKED	0x1	/* file has been fully linked */
    TAILQ_ENTRY(linker_file) link;	/* list of all loaded files */
    char*		filename;	/* file which was loaded */
    int			id;		/* unique id */
    caddr_t		address;	/* load address */
    size_t		size;		/* size of file */
    int			ndeps;		/* number of dependencies */
    linker_file_t*	deps;		/* list of dependencies */
    STAILQ_HEAD(, common_symbol) common; /* list of common symbols */
    TAILQ_HEAD(, module) modules;	/* modules in this file */
    TAILQ_ENTRY(linker_file) loaded;	/* preload dependency support */
};

/*
 * Object implementing a class of file (a.out, elf, etc.)
 */
typedef struct linker_class *linker_class_t;
typedef TAILQ_HEAD(, linker_class) linker_class_list_t;

struct linker_class {
    KOBJ_CLASS_FIELDS;
    TAILQ_ENTRY(linker_class) link;	/* list of all file classes */
};

/*
 * The "file" for the kernel.
 */
extern linker_file_t	linker_kernel_file;

/*
 * Add a new file class to the linker.
 */
int linker_add_class(linker_class_t _cls);

/*
 * Load a kernel module.
 */
int linker_load_module(const char *_kldname, const char *_modname,
    struct linker_file *_parent, struct mod_depend *_verinfo,
    struct linker_file **_lfpp);

/*
 * Obtain a reference to a module, loading it if required.
 */
int linker_reference_module(const char* _modname, struct mod_depend *_verinfo,
			    linker_file_t* _result);

/*
 * Find a currently loaded file given its filename.
 */
linker_file_t linker_find_file_by_name(const char* _filename);

/*
 * Find a currently loaded file given its file id.
 */
linker_file_t linker_find_file_by_id(int _fileid);

/*
 * Called from a class handler when a file is laoded.
 */
linker_file_t linker_make_file(const char* _filename, linker_class_t _cls);

/*
 * Unload a file, freeing up memory.
 */
int linker_file_unload(linker_file_t _file);

/*
 * Add a dependency to a file.
 */
int linker_file_add_dependency(linker_file_t _file, linker_file_t _dep);

/*
 * Lookup a symbol in a file.  If deps is TRUE, look in dependencies
 * if not found in file.
 */
caddr_t linker_file_lookup_symbol(linker_file_t _file, const char* _name, 
				  int _deps);

/*
 * Lookup a linker set in a file.  Return pointers to the first entry,
 * last + 1, and count of entries.  Use: for (p = start; p < stop; p++) {}
 * void *start is really: "struct yoursetmember ***start;"
 */
int linker_file_lookup_set(linker_file_t _file, const char *_name,
			   void *_start, void *_stop, int *_count);

/*
 * This routine is responsible for finding dependencies of userland
 * initiated kldload(2)'s of files.
 */
int linker_load_dependencies(linker_file_t _lf);

/*
 * DDB Helpers, tuned specifically for ddb/db_kld.c
 */
int linker_ddb_lookup(const char *_symstr, c_linker_sym_t *_sym);
int linker_ddb_search_symbol(caddr_t _value, c_linker_sym_t *_sym,
			     long *_diffp);
int linker_ddb_symbol_values(c_linker_sym_t _sym, linker_symval_t *_symval);


#endif	/* _KERNEL */

/*
 * Module information subtypes
 */
#define MODINFO_END		0x0000		/* End of list */
#define MODINFO_NAME		0x0001		/* Name of module (string) */
#define MODINFO_TYPE		0x0002		/* Type of module (string) */
#define MODINFO_ADDR		0x0003		/* Loaded address */
#define MODINFO_SIZE		0x0004		/* Size of module */
#define MODINFO_EMPTY		0x0005		/* Has been deleted */
#define MODINFO_ARGS		0x0006		/* Parameters string */
#define MODINFO_METADATA	0x8000		/* Module-specfic */

#define MODINFOMD_AOUTEXEC	0x0001		/* a.out exec header */
#define MODINFOMD_ELFHDR	0x0002		/* ELF header */
#define MODINFOMD_SSYM		0x0003		/* start of symbols */
#define MODINFOMD_ESYM		0x0004		/* end of symbols */
#define MODINFOMD_DYNAMIC	0x0005		/* _DYNAMIC pointer */
/* These values are MD on these two platforms */
#if !defined(__sparc64__) && !defined(__powerpc__)
#define MODINFOMD_ENVP		0x0006		/* envp[] */
#define MODINFOMD_HOWTO		0x0007		/* boothowto */
#define MODINFOMD_KERNEND	0x0008		/* kernend */
#endif
#define MODINFOMD_NOCOPY	0x8000		/* don't copy this metadata to the kernel */

#define MODINFOMD_DEPLIST	(0x4001 | MODINFOMD_NOCOPY)	/* depends on */

#ifdef _KERNEL
#define MD_FETCH(mdp, info, type) ({ \
	type *__p; \
	__p = (type *)preload_search_info((mdp), MODINFO_METADATA | (info)); \
	__p ? *__p : 0; \
})
#endif

#define	LINKER_HINTS_VERSION	1		/* linker.hints file version */

#ifdef _KERNEL

/*
 * Module lookup
 */
extern caddr_t		preload_metadata;
extern caddr_t		preload_search_by_name(const char *_name);
extern caddr_t		preload_search_by_type(const char *_type);
extern caddr_t		preload_search_next_name(caddr_t _base);
extern caddr_t		preload_search_info(caddr_t _mod, int _inf);
extern void		preload_delete_name(const char *_name);
extern void		preload_bootstrap_relocate(vm_offset_t _offset);

#ifdef KLD_DEBUG

extern int kld_debug;
#define KLD_DEBUG_FILE	1	/* file load/unload */
#define KLD_DEBUG_SYM	2	/* symbol lookup */

#define KLD_DPF(cat, args)					\
	do {							\
		if (kld_debug & KLD_DEBUG_##cat) printf args;	\
	} while (0)

#else

#define KLD_DPF(cat, args)

#endif

/* Support functions */
int	elf_reloc(linker_file_t _lf, const void *_rel, int _type);
int	elf_reloc_local(linker_file_t _lf, const void *_rel, int _type);
Elf_Addr elf_lookup(linker_file_t, Elf_Word, int);
const Elf_Sym *elf_get_sym(linker_file_t _lf, Elf_Word _symidx);
const char *elf_get_symname(linker_file_t _lf, Elf_Word _symidx);

int elf_cpu_load_file(linker_file_t);
int elf_cpu_unload_file(linker_file_t);

/* values for type */
#define ELF_RELOC_REL	1
#define ELF_RELOC_RELA	2

#endif /* _KERNEL */

struct kld_file_stat {
    int		version;	/* set to sizeof(linker_file_stat) */
    char        name[MAXPATHLEN];
    int		refs;
    int		id;
    caddr_t	address;	/* load address */
    size_t	size;		/* size in bytes */
};

struct kld_sym_lookup {
    int		version;	/* set to sizeof(struct kld_sym_lookup) */
    char	*symname;	/* Symbol name we are looking up */
    u_long	symvalue;
    size_t	symsize;
};
#define KLDSYM_LOOKUP	1

#ifndef _KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
int	kldload(const char* _file);
int	kldunload(int _fileid);
int	kldfind(const char* _file);
int	kldnext(int _fileid);
int	kldstat(int _fileid, struct kld_file_stat* _stat);
int	kldfirstmod(int _fileid);
int	kldsym(int _fileid, int _cmd, void *_data);
__END_DECLS

#endif

#endif /* !_SYS_LINKER_H_ */
