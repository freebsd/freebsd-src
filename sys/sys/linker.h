/*-
 * Copyright (c) 1997 Doug Rabson
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
 *	$Id: linker.h,v 1.1 1997/05/07 16:05:45 dfr Exp $
 */

#ifndef _SYS_LINKER_H_
#define _SYS_LINKER_H_

#ifdef KERNEL

#define M_LINKER	M_TEMP	/* XXX */

/*
 * Object representing a file which has been loaded by the linker.
 */
typedef struct linker_file* linker_file_t;
typedef TAILQ_HEAD(, linker_file) linker_file_list_t;

struct linker_file_ops {
    /*
     * Lookup a symbol in the file's symbol table.  If the symbol is
     * not found then return ENOENT, otherwise zero.  If the symbol
     * found is a common symbol, return with *address set to zero and
     * *size set to the size of the common space required.  Otherwise
     * set *address the value of the symbol.
     */
    int			(*lookup_symbol)(linker_file_t, const char* name,
					 caddr_t* address, size_t* size);

    /*
     * Unload a file, releasing dependancies and freeing storage.
     */
    void		(*unload)(linker_file_t);
};

struct common_symbol {
    STAILQ_ENTRY(common_symbol) link;
    char*		name;
    caddr_t		address;
};

struct linker_file {
    int			refs;		/* reference count */
    int			userrefs;	/* modload(2) count */
    TAILQ_ENTRY(linker_file) link;	/* list of all loaded files */
    char*		filename;	/* file which was loaded */
    int			id;		/* unique id */
    caddr_t		address;	/* load address */
    size_t		size;		/* size of file */
    int			ndeps;		/* number of dependancies */
    linker_file_t*	deps;		/* list of dependancies */
    STAILQ_HEAD(, common_symbol) common; /* list of common symbols */
    TAILQ_HEAD(, module) modules;	/* modules in this file */
    void*		priv;		/* implementation data */

    struct linker_file_ops* ops;
};

/*
 * Object implementing a class of file (a.out, elf, etc.)
 */
typedef struct linker_class *linker_class_t;
typedef TAILQ_HEAD(, linker_class) linker_class_list_t;

struct linker_class_ops {
    /* 
     * Load a file, returning the new linker_file_t in *result.  If
     * the class does not recognise the file type, zero should be
     * returned, without modifying *result.  If the file is
     * recognised, the file should be loaded, *result set to the new
     * file and zero returned.  If some other error is detected an
     * appropriate errno should be returned.
     */
    int		(*load_file)(const char* filename, linker_file_t* result);
};

struct linker_class {
    TAILQ_ENTRY(linker_class) link;	/* list of all file classes */
    const char*		desc;		/* description (e.g. "a.out") */
    void*		priv;		/* implementation data */

    struct linker_class_ops *ops;
};

/*
 * The file which is currently loading.  Used to register modules with
 * the files which contain them.
 */
extern linker_file_t	linker_current_file;

/*
 * Add a new file class to the linker.
 */
int linker_add_class(const char* desc, void* priv,
		     struct linker_class_ops* ops);

/*
 * Load a file, trying each file class until one succeeds.
 */
int linker_load_file(const char* filename, linker_file_t* result);

/*
 * Find a currently loaded file given its filename.
 */
linker_file_t linker_find_file_by_name(const char* filename);

/*
 * Find a currently loaded file given its file id.
 */
linker_file_t linker_find_file_by_id(int fileid);

/*
 * Called from a class handler when a file is laoded.
 */
linker_file_t linker_make_file(const char* filename, void* priv,
			       struct linker_file_ops* ops);

/*
 * Unload a file, freeing up memory.
 */
int linker_file_unload(linker_file_t file);

/*
 * Add a dependancy to a file.
 */
int linker_file_add_dependancy(linker_file_t file, linker_file_t dep);

/*
 * Lookup a symbol in a file.  If deps is TRUE, look in dependancies
 * if not found in file.
 */
caddr_t linker_file_lookup_symbol(linker_file_t file, const char* name, 
				  int deps);

#ifdef KLD_DEBUG

extern int kld_debug;
#define KLD_DEBUG_FILE	1	/* file load/unload */
#define KLD_DEBUG_SYM	2	/* symbol lookup */

#define KLD_DPF(cat, args)					\
	do {							\
		if (KLD_debug & KLD_DEBUG_##cat) printf args;	\
	} while (0)

#else

#define KLD_DPF(cat, args)

#endif

#endif /* KERNEL */

struct kld_file_stat {
    int		version;	/* set to sizeof(linker_file_stat) */
    char        name[MAXPATHLEN];
    int		refs;
    int		id;
    caddr_t	address;	/* load address */
    size_t	size;		/* size in bytes */
};

#ifndef KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
int	kldload(const char* file);
int	kldunload(int fileid);
int	kldfind(const char* file);
int	kldnext(int fileid);
int	kldstat(int fileid, struct kld_file_stat* stat);
int	kldfirstmod(int fileid);
__END_DECLS

#endif

#endif	/* !_SYS_KLD_H_ */
