/*-
 * Copyright 1996, 1997, 1998, 1999, 2000 John D. Polstra.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef RTLD_H /* { */
#define RTLD_H 1

#include <machine/elf.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <elf-hints.h>
#include <link.h>
#include <stddef.h>

#include "rtld_machdep.h"

#ifndef STANDARD_LIBRARY_PATH
#define STANDARD_LIBRARY_PATH	"/usr/lib"
#endif

#define NEW(type)	((type *) xmalloc(sizeof(type)))
#define CNEW(type)	((type *) xcalloc(sizeof(type)))

/* We might as well do booleans like C++. */
typedef unsigned char bool;
#define false	0
#define true	1

struct stat;
struct Struct_Obj_Entry;

/* Lists of shared objects */
typedef struct Struct_Objlist_Entry {
    STAILQ_ENTRY(Struct_Objlist_Entry) link;
    struct Struct_Obj_Entry *obj;
} Objlist_Entry;

typedef STAILQ_HEAD(Struct_Objlist, Struct_Objlist_Entry) Objlist;

/* Types of init and fini functions */
typedef void (*InitFunc)(void);

/* Lists of shared object dependencies */
typedef struct Struct_Needed_Entry {
    struct Struct_Needed_Entry *next;
    struct Struct_Obj_Entry *obj;
    unsigned long name;		/* Offset of name in string table */
} Needed_Entry;

/* Lock object */
typedef struct Struct_LockInfo {
    void *context;		/* Client context for creating locks */
    void *thelock;		/* The one big lock */
    /* Debugging aids. */
    volatile int rcount;	/* Number of readers holding lock */
    volatile int wcount;	/* Number of writers holding lock */
    /* Methods */
    void *(*lock_create)(void *context);
    void (*rlock_acquire)(void *lock);
    void (*wlock_acquire)(void *lock);
    void (*rlock_release)(void *lock);
    void (*wlock_release)(void *lock);
    void (*lock_destroy)(void *lock);
    void (*context_destroy)(void *context);
} LockInfo;

/*
 * Shared object descriptor.
 *
 * Items marked with "(%)" are dynamically allocated, and must be freed
 * when the structure is destroyed.
 *
 * CAUTION: It appears that the JDK port peeks into these structures.
 * It looks at "next" and "mapbase" at least.  Don't add new members
 * near the front, until this can be straightened out.
 */
typedef struct Struct_Obj_Entry {
    /*
     * These two items have to be set right for compatibility with the
     * original ElfKit crt1.o.
     */
    Elf_Word magic;		/* Magic number (sanity check) */
    Elf_Word version;		/* Version number of struct format */

    struct Struct_Obj_Entry *next;
    char *path;			/* Pathname of underlying file (%) */
    int refcount;
    int dl_refcount;		/* Number of times loaded by dlopen */

    /* These items are computed by map_object() or by digest_phdr(). */
    caddr_t mapbase;		/* Base address of mapped region */
    size_t mapsize;		/* Size of mapped region in bytes */
    size_t textsize;		/* Size of text segment in bytes */
    Elf_Addr vaddrbase;		/* Base address in shared object file */
    caddr_t relocbase;		/* Relocation constant = mapbase - vaddrbase */
    const Elf_Dyn *dynamic;	/* Dynamic section */
    caddr_t entry;		/* Entry point */
    const Elf_Phdr *phdr;	/* Program header if it is mapped, else NULL */
    size_t phsize;		/* Size of program header in bytes */
    const char *interp;		/* Pathname of the interpreter, if any */

    /* Items from the dynamic section. */
    Elf_Addr *pltgot;		/* PLT or GOT, depending on architecture */
    const Elf_Rel *rel;		/* Relocation entries */
    unsigned long relsize;	/* Size in bytes of relocation info */
    const Elf_Rela *rela;	/* Relocation entries with addend */
    unsigned long relasize;	/* Size in bytes of addend relocation info */
    const Elf_Rel *pltrel;	/* PLT relocation entries */
    unsigned long pltrelsize;	/* Size in bytes of PLT relocation info */
    const Elf_Rela *pltrela;	/* PLT relocation entries with addend */
    unsigned long pltrelasize;	/* Size in bytes of PLT addend reloc info */
    const Elf_Sym *symtab;	/* Symbol table */
    const char *strtab;		/* String table */
    unsigned long strsize;	/* Size in bytes of string table */

    const Elf_Addr *buckets;	/* Hash table buckets array */
    unsigned long nbuckets;	/* Number of buckets */
    const Elf_Addr *chains;	/* Hash table chain array */
    unsigned long nchains;	/* Number of chains */

    const char *rpath;		/* Search path specified in object */
    Needed_Entry *needed;	/* Shared objects needed by this one (%) */

    InitFunc init;		/* Initialization function to call */
    InitFunc fini;		/* Termination function to call */

    bool mainprog;		/* True if this is the main program */
    bool rtld;			/* True if this is the dynamic linker */
    bool textrel;		/* True if there are relocations to text seg */
    bool symbolic;		/* True if generated with "-Bsymbolic" */
    bool traced;		/* Already printed in ldd trace output */
    bool jmpslots_done;		/* Already have relocated the jump slots */
    bool init_done;		/* Already have added object to init list */

    struct link_map linkmap;	/* for GDB and dlinfo() */
    Objlist dldags;		/* Object belongs to these dlopened DAGs (%) */
    Objlist dagmembers;		/* DAG has these members (%) */
    dev_t dev;			/* Object's filesystem's device */
    ino_t ino;			/* Object's inode number */
} Obj_Entry;

#define RTLD_MAGIC	0xd550b87a
#define RTLD_VERSION	1

/*
 * Symbol cache entry used during relocation to avoid multiple lookups
 * of the same symbol.
 */
typedef struct Struct_SymCache {
    const Elf_Sym *sym;		/* Symbol table entry */
    const Obj_Entry *obj;	/* Shared object which defines it */
} SymCache;

extern void _rtld_error(const char *, ...) __printflike(1, 2);
extern Obj_Entry *map_object(int, const char *, const struct stat *);
extern void *xcalloc(size_t);
extern void *xmalloc(size_t);
extern char *xstrdup(const char *);
extern Elf_Addr _GLOBAL_OFFSET_TABLE_[];

/*
 * Function declarations.
 */
int do_copy_relocations(Obj_Entry *);
unsigned long elf_hash(const char *);
const Elf_Sym *find_symdef(unsigned long, const Obj_Entry *,
  const Obj_Entry **, bool, SymCache *);
void init_pltgot(Obj_Entry *);
void lockdflt_init(LockInfo *);
void obj_free(Obj_Entry *);
Obj_Entry *obj_new(void);
int reloc_non_plt(Obj_Entry *, Obj_Entry *);
int reloc_plt(Obj_Entry *);
int reloc_jmpslots(Obj_Entry *);
void _rtld_bind_start(void);
const Elf_Sym *symlook_obj(const char *, unsigned long,
  const Obj_Entry *, bool);

#endif /* } */
