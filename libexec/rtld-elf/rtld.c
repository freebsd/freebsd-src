/*-
 * Copyright 1996-1998 John D. Polstra.
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
 *      $Id: rtld.c,v 1.2 1998/04/30 07:48:00 dfr Exp $
 */

/*
 * Dynamic linker for ELF.
 *
 * John Polstra <jdp@polstra.com>.
 */

#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif

#include <sys/param.h>
#include <sys/mman.h>

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "rtld.h"

/*
 * Debugging support.
 */

#define assert(cond)	((cond) ? (void) 0 :\
    (msg("oops: " __XSTRING(__LINE__) "\n"), abort()))
#define msg(s)		(write(1, s, strlen(s)))
#define trace()		msg("trace: " __XSTRING(__LINE__) "\n");

#define END_SYM		"end"

/* Types. */
typedef void (*func_ptr_type)();

/*
 * Function declarations.
 */
static void call_fini_functions(Obj_Entry *);
static void call_init_functions(Obj_Entry *);
static void die(void);
static void digest_dynamic(Obj_Entry *);
static Obj_Entry *digest_phdr(const Elf32_Phdr *, int, caddr_t);
static Obj_Entry *dlcheck(void *);
static int do_copy_relocations(Obj_Entry *);
static unsigned long elf_hash(const char *);
static char *find_library(const char *, const Obj_Entry *);
static const Elf32_Sym *find_symdef(unsigned long, const Obj_Entry *,
  const Obj_Entry **, bool);
static void init_rtld(caddr_t);
static bool is_exported(const Elf32_Sym *);
static int load_needed_objects(Obj_Entry *);
static Obj_Entry *load_object(char *);
static Obj_Entry *obj_from_addr(const void *);
static int relocate_objects(Obj_Entry *, bool);
static void rtld_exit(void);
static char *search_library_path(const char *, const char *);
static const Elf32_Sym *symlook_obj(const char *, unsigned long,
  const Obj_Entry *, bool);
static void unref_object_dag(Obj_Entry *);
void r_debug_state(void);
static void linkmap_add(Obj_Entry *);
static void linkmap_delete(Obj_Entry *);
static void trace_loaded_objects(Obj_Entry *obj);

void xprintf(const char *, ...);

#ifdef DEBUG
static const char *basename(const char *);
#endif

/* Assembly language entry point for lazy binding. */
extern void _rtld_bind_start(void);

/*
 * Assembly language macro for getting the GOT pointer.
 */
#ifdef __i386__
#define get_got_address()				\
    ({ Elf32_Addr *thegot;				\
       __asm__("movl %%ebx,%0" : "=rm"(thegot));	\
       thegot; })
#else
#error "This file only supports the i386 architecture"
#endif

/*
 * Data declarations.
 */
static char *error_message;	/* Message for dlerror(), or NULL */
struct r_debug r_debug;	/* for GDB; */
static bool trust;		/* False for setuid and setgid programs */
static char *ld_bind_now;	/* Environment variable for immediate binding */
static char *ld_debug;		/* Environment variable for debugging */
static char *ld_library_path;	/* Environment variable for search path */
static char *ld_tracing;	/* Called from ldd to print libs */
static Obj_Entry *obj_list;	/* Head of linked list of shared objects */
static Obj_Entry **obj_tail;	/* Link field of last object in list */
static Obj_Entry *obj_main;	/* The main program shared object */
static Obj_Entry obj_rtld;	/* The dynamic linker shared object */

#define GDB_STATE(s)	r_debug.r_state = s; r_debug_state();

/*
 * These are the functions the dynamic linker exports to application
 * programs.  They are the only symbols the dynamic linker is willing
 * to export from itself.
 */
static func_ptr_type exports[] = {
    (func_ptr_type) &_rtld_error,
    (func_ptr_type) &dlclose,
    (func_ptr_type) &dlerror,
    (func_ptr_type) &dlopen,
    (func_ptr_type) &dlsym,
    NULL
};

/*
 * Global declarations normally provided by crt1.  The dynamic linker is
 * not build with crt1, so we have to provide them ourselves.
 */
char *__progname;
char **environ;

/*
 * Main entry point for dynamic linking.  The first argument is the
 * stack pointer.  The stack is expected to be laid out as described
 * in the SVR4 ABI specification, Intel 386 Processor Supplement.
 * Specifically, the stack pointer points to a word containing
 * ARGC.  Following that in the stack is a null-terminated sequence
 * of pointers to argument strings.  Then comes a null-terminated
 * sequence of pointers to environment strings.  Finally, there is a
 * sequence of "auxiliary vector" entries.
 *
 * The second argument points to a place to store the dynamic linker's
 * exit procedure pointer.
 *
 * The return value is the main program's entry point.
 */
func_ptr_type
_rtld(Elf32_Word *sp, func_ptr_type *exit_proc)
{
    Elf32_Auxinfo *aux_info[AT_COUNT];
    int i;
    int argc;
    char **argv;
    char **env;
    Elf32_Auxinfo *aux;
    Elf32_Auxinfo *auxp;

    /*
     * On entry, the dynamic linker itself has not been relocated yet.
     * Be very careful not to reference any global data until after
     * init_rtld has returned.  It is OK to reference file-scope statics
     * and string constants, and to call static and global functions.
     */

    /* Find the auxiliary vector on the stack. */
    argc = *sp++;
    argv = (char **) sp;
    sp += argc + 1;	/* Skip over arguments and NULL terminator */
    env = (char **) sp;
    while (*sp++ != 0)	/* Skip over environment, and NULL terminator */
	;
    aux = (Elf32_Auxinfo *) sp;

    /* Digest the auxiliary vector. */
    for (i = 0;  i < AT_COUNT;  i++)
	aux_info[i] = NULL;
    for (auxp = aux;  auxp->a_type != AT_NULL;  auxp++) {
	if (auxp->a_type < AT_COUNT)
	    aux_info[auxp->a_type] = auxp;
    }

    /* Initialize and relocate ourselves. */
    assert(aux_info[AT_BASE] != NULL);
    init_rtld((caddr_t) aux_info[AT_BASE]->a_un.a_ptr);

    __progname = obj_rtld.path;
    environ = env;

    trust = geteuid() == getuid() && getegid() == getgid();

    ld_bind_now = getenv("LD_BIND_NOW");
    if (trust) {
	ld_debug = getenv("LD_DEBUG");
	ld_library_path = getenv("LD_LIBRARY_PATH");
    }
    ld_tracing = getenv("LD_TRACE_LOADED_OBJECTS");

    if (ld_debug != NULL && *ld_debug != '\0')
	debug = 1;
    dbg("%s is initialized, base address = %p", __progname,
	(caddr_t) aux_info[AT_BASE]->a_un.a_ptr);

    /*
     * Load the main program, or process its program header if it is
     * already loaded.
     */
    if (aux_info[AT_EXECFD] != NULL) {	/* Load the main program. */
	int fd = aux_info[AT_EXECFD]->a_un.a_val;
	dbg("loading main program");
	obj_main = map_object(fd);
	close(fd);
	if (obj_main == NULL)
	    die();
    } else {				/* Main program already loaded. */
	const Elf32_Phdr *phdr;
	int phnum;
	caddr_t entry;

	dbg("processing main program's program header");
	assert(aux_info[AT_PHDR] != NULL);
	phdr = (const Elf32_Phdr *) aux_info[AT_PHDR]->a_un.a_ptr;
	assert(aux_info[AT_PHNUM] != NULL);
	phnum = aux_info[AT_PHNUM]->a_un.a_val;
	assert(aux_info[AT_PHENT] != NULL);
	assert(aux_info[AT_PHENT]->a_un.a_val == sizeof(Elf32_Phdr));
	assert(aux_info[AT_ENTRY] != NULL);
	entry = (caddr_t) aux_info[AT_ENTRY]->a_un.a_ptr;
	obj_main = digest_phdr(phdr, phnum, entry);
    }

    obj_main->path = xstrdup(argv[0]);
    obj_main->mainprog = true;
    digest_dynamic(obj_main);

    linkmap_add(obj_main);
    linkmap_add(&obj_rtld);

    /* Link the main program into the list of objects. */
    *obj_tail = obj_main;
    obj_tail = &obj_main->next;
    obj_main->refcount++;

    dbg("loading needed objects");
    if (load_needed_objects(obj_main) == -1)
	die();

    if (ld_tracing) {		/* We're done */
	trace_loaded_objects(obj_main);
	exit(0);
    }

    dbg("relocating objects");
    if (relocate_objects(obj_main,
	ld_bind_now != NULL && *ld_bind_now != '\0') == -1)
	die();

    dbg("doing copy relocations");
    if (do_copy_relocations(obj_main) == -1)
	die();

    dbg("calling _init functions");
    call_init_functions(obj_main->next);

    dbg("transferring control to program entry point = %p", obj_main->entry);

    r_debug_state();		/* say hello to gdb! */

    /* Return the exit procedure and the program entry point. */
    *exit_proc = (func_ptr_type) rtld_exit;
    return (func_ptr_type) obj_main->entry;
}

caddr_t
_rtld_bind(const Obj_Entry *obj, Elf32_Word reloff)
{
    const Elf32_Rel *rel;
    const Elf32_Sym *def;
    const Obj_Entry *defobj;
    Elf32_Addr *where;
    caddr_t target;

    rel = (const Elf32_Rel *) ((caddr_t) obj->pltrel + reloff);
    assert(ELF32_R_TYPE(rel->r_info) == R_386_JMP_SLOT);

    where = (Elf32_Addr *) (obj->relocbase + rel->r_offset);
    def = find_symdef(ELF32_R_SYM(rel->r_info), obj, &defobj, true);
    if (def == NULL)
	die();

    target = (caddr_t) (defobj->relocbase + def->st_value);

    dbg("\"%s\" in \"%s\" ==> %p in \"%s\"",
      defobj->strtab + def->st_name, basename(obj->path),
      target, basename(defobj->path));

    *where = (Elf32_Addr) target;
    return target;
}

/*
 * Error reporting function.  Use it like printf.  If formats the message
 * into a buffer, and sets things up so that the next call to dlerror()
 * will return the message.
 */
void
_rtld_error(const char *fmt, ...)
{
    static char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    error_message = buf;
    va_end(ap);
}

#ifdef DEBUG
static const char *
basename(const char *name)
{
    const char *p = strrchr(name, '/');
    return p != NULL ? p + 1 : name;
}
#endif

static void
call_fini_functions(Obj_Entry *first)
{
    Obj_Entry *obj;

    for (obj = first;  obj != NULL;  obj = obj->next)
	if (obj->fini != NULL)
	    (*obj->fini)();
}

static void
call_init_functions(Obj_Entry *first)
{
    if (first != NULL) {
	call_init_functions(first->next);
	if (first->init != NULL)
	    (*first->init)();
    }
}

static void
die(void)
{
    const char *msg = dlerror();

    if (msg == NULL)
	msg = "Fatal error";
    errx(1, "%s", msg);
}

/*
 * Process a shared object's DYNAMIC section, and save the important
 * information in its Obj_Entry structure.
 */
static void
digest_dynamic(Obj_Entry *obj)
{
    const Elf32_Dyn *dynp;
    Needed_Entry **needed_tail = &obj->needed;
    const Elf32_Dyn *dyn_rpath = NULL;

    for (dynp = obj->dynamic;  dynp->d_tag != DT_NULL;  dynp++) {
	switch (dynp->d_tag) {

	case DT_REL:
	    obj->rel = (const Elf32_Rel *) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_RELSZ:
	    obj->relsize = dynp->d_un.d_val;
	    break;

	case DT_RELENT:
	    assert(dynp->d_un.d_val == sizeof(Elf32_Rel));
	    break;

	case DT_JMPREL:
	    obj->pltrel = (const Elf32_Rel *)
	      (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_PLTRELSZ:
	    obj->pltrelsize = dynp->d_un.d_val;
	    break;

	case DT_RELA:
	case DT_RELASZ:
	case DT_RELAENT:
	    assert(0);	/* Should never appear for i386 */
	    break;

	case DT_PLTREL:
	    assert(dynp->d_un.d_val == DT_REL);		/* For the i386 */
	    break;

	case DT_SYMTAB:
	    obj->symtab = (const Elf32_Sym *)
	      (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_SYMENT:
	    assert(dynp->d_un.d_val == sizeof(Elf32_Sym));
	    break;

	case DT_STRTAB:
	    obj->strtab = (const char *) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_STRSZ:
	    obj->strsize = dynp->d_un.d_val;
	    break;

	case DT_HASH:
	    {
		const Elf32_Word *hashtab = (const Elf32_Word *)
		  (obj->relocbase + dynp->d_un.d_ptr);
		obj->nbuckets = hashtab[0];
		obj->nchains = hashtab[1];
		obj->buckets = hashtab + 2;
		obj->chains = obj->buckets + obj->nbuckets;
	    }
	    break;

	case DT_NEEDED:
	    assert(!obj->rtld);
	    {
		Needed_Entry *nep = NEW(Needed_Entry);
		nep->name = dynp->d_un.d_val;
		nep->obj = NULL;
		nep->next = NULL;

		*needed_tail = nep;
		needed_tail = &nep->next;
	    }
	    break;

	case DT_PLTGOT:
	    obj->got = (Elf32_Addr *) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_TEXTREL:
	    obj->textrel = true;
	    break;

	case DT_SYMBOLIC:
	    obj->symbolic = true;
	    break;

	case DT_RPATH:
	    /*
	     * We have to wait until later to process this, because we
	     * might not have gotten the address of the string table yet.
	     */
	    dyn_rpath = dynp;
	    break;

	case DT_SONAME:
	    /* Not used by the dynamic linker. */
	    break;

	case DT_INIT:
	    obj->init = (void (*)(void)) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_FINI:
	    obj->fini = (void (*)(void)) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_DEBUG:
	    /* XXX - not implemented yet */
	    dbg("Filling in DT_DEBUG entry");
	    ((Elf32_Dyn*)dynp)->d_un.d_ptr = (Elf32_Addr) &r_debug;
	    break;
	}
    }

    if (dyn_rpath != NULL)
	obj->rpath = obj->strtab + dyn_rpath->d_un.d_val;
}

/*
 * Process a shared object's program header.  This is used only for the
 * main program, when the kernel has already loaded the main program
 * into memory before calling the dynamic linker.  It creates and
 * returns an Obj_Entry structure.
 */
static Obj_Entry *
digest_phdr(const Elf32_Phdr *phdr, int phnum, caddr_t entry)
{
    Obj_Entry *obj = CNEW(Obj_Entry);
    const Elf32_Phdr *phlimit = phdr + phnum;
    const Elf32_Phdr *ph;
    int nsegs = 0;

    for (ph = phdr;  ph < phlimit;  ph++) {
	switch (ph->p_type) {

	case PT_PHDR:
	    assert((const Elf32_Phdr *) ph->p_vaddr == phdr);
	    obj->phdr = (const Elf32_Phdr *) ph->p_vaddr;
	    obj->phsize = ph->p_memsz;
	    break;

	case PT_LOAD:
	    assert(nsegs < 2);
	    if (nsegs == 0) {	/* First load segment */
		obj->vaddrbase = trunc_page(ph->p_vaddr);
		obj->mapbase = (caddr_t) obj->vaddrbase;
		obj->relocbase = obj->mapbase - obj->vaddrbase;
		obj->textsize = round_page(ph->p_vaddr + ph->p_memsz) -
		  obj->vaddrbase;
	    } else {		/* Last load segment */
		obj->mapsize = round_page(ph->p_vaddr + ph->p_memsz) -
		  obj->vaddrbase;
	    }
	    nsegs++;
	    break;

	case PT_DYNAMIC:
	    obj->dynamic = (const Elf32_Dyn *) ph->p_vaddr;
	    break;
	}
    }
    assert(nsegs == 2);

    obj->entry = entry;
    return obj;
}

static Obj_Entry *
dlcheck(void *handle)
{
    Obj_Entry *obj;

    for (obj = obj_list;  obj != NULL;  obj = obj->next)
	if (obj == (Obj_Entry *) handle)
	    break;

    if (obj == NULL || obj->dl_refcount == 0) {
	_rtld_error("Invalid shared object handle %p", handle);
	return NULL;
    }
    return obj;
}

/*
 * Process the special R_386_COPY relocations in the main program.  These
 * copy data from a shared object into a region in the main program's BSS
 * segment.
 *
 * Returns 0 on success, -1 on failure.
 */
static int
do_copy_relocations(Obj_Entry *dstobj)
{
    const Elf32_Rel *rellim;
    const Elf32_Rel *rel;

    assert(dstobj->mainprog);	/* COPY relocations are invalid elsewhere */

    rellim = (const Elf32_Rel *) ((caddr_t) dstobj->rel + dstobj->relsize);
    for (rel = dstobj->rel;  rel < rellim;  rel++) {
	if (ELF32_R_TYPE(rel->r_info) == R_386_COPY) {
	    void *dstaddr;
	    const Elf32_Sym *dstsym;
	    const char *name;
	    unsigned long hash;
	    size_t size;
	    const void *srcaddr;
	    const Elf32_Sym *srcsym;
	    Obj_Entry *srcobj;

	    dstaddr = (void *) (dstobj->relocbase + rel->r_offset);
	    dstsym = dstobj->symtab + ELF32_R_SYM(rel->r_info);
	    name = dstobj->strtab + dstsym->st_name;
	    hash = elf_hash(name);
	    size = dstsym->st_size;

	    for (srcobj = dstobj->next;  srcobj != NULL;  srcobj = srcobj->next)
		if ((srcsym = symlook_obj(name, hash, srcobj, false)) != NULL)
		    break;

	    if (srcobj == NULL) {
		_rtld_error("Undefined symbol \"%s\" referenced from COPY"
		  " relocation in %s", name, dstobj->path);
		return -1;
	    }

	    srcaddr = (const void *) (srcobj->relocbase + srcsym->st_value);
	    memcpy(dstaddr, srcaddr, size);
	}
    }

    return 0;
}

/*
 * Hash function for symbol table lookup.  Don't even think about changing
 * this.  It is specified by the System V ABI.
 */
static unsigned long
elf_hash(const char *name)
{
    const unsigned char *p = (const unsigned char *) name;
    unsigned long h = 0;
    unsigned long g;

    while (*p != '\0') {
	h = (h << 4) + *p++;
	if ((g = h & 0xf0000000) != 0)
	    h ^= g >> 24;
	h &= ~g;
    }
    return h;
}

/*
 * Find the library with the given name, and return its full pathname.
 * The returned string is dynamically allocated.  Generates an error
 * message and returns NULL if the library cannot be found.
 *
 * If the second argument is non-NULL, then it refers to an already-
 * loaded shared object, whose library search path will be searched.
 */
static char *
find_library(const char *name, const Obj_Entry *refobj)
{
    char *pathname;

    if (strchr(name, '/') != NULL) {	/* Hard coded pathname */
	if (name[0] != '/' && !trust) {
	    _rtld_error("Absolute pathname required for shared object \"%s\"",
	      name);
	    return NULL;
	}
	return xstrdup(name);
    }

    dbg(" Searching for \"%s\"", name);

    if ((refobj != NULL &&
      (pathname = search_library_path(name, refobj->rpath)) != NULL) ||
      (pathname = search_library_path(name, ld_library_path)) != NULL ||
      (pathname = search_library_path(name, STANDARD_LIBRARY_PATH)) != NULL)
	return pathname;

    _rtld_error("Shared object \"%s\" not found", name);
    return NULL;
}

/*
 * Given a symbol number in a referencing object, find the corresponding
 * definition of the symbol.  Returns a pointer to the symbol, or NULL if
 * no definition was found.  Returns a pointer to the Obj_Entry of the
 * defining object via the reference parameter DEFOBJ_OUT.
 */
static const Elf32_Sym *
find_symdef(unsigned long symnum, const Obj_Entry *refobj,
    const Obj_Entry **defobj_out, bool in_plt)
{
    const Elf32_Sym *ref;
    const Elf32_Sym *strongdef;
    const Elf32_Sym *weakdef;
    const Obj_Entry *obj;
    const Obj_Entry *strongobj;
    const Obj_Entry *weakobj;
    const char *name;
    unsigned long hash;

    ref = refobj->symtab + symnum;
    name = refobj->strtab + ref->st_name;
    hash = elf_hash(name);

    if (refobj->symbolic) {	/* Look first in the referencing object */
	const Elf32_Sym *def = symlook_obj(name, hash, refobj, in_plt);
	if (def != NULL) {
	    *defobj_out = refobj;
	    return def;
	}
    }

    /*
     * Look in all loaded objects.  Skip the referencing object, if
     * we have already searched it.  We keep track of the first weak
     * definition and the first strong definition we encounter.  If
     * we find a strong definition we stop searching, because there
     * won't be anything better than that.
     */
    strongdef = weakdef = NULL;
    strongobj = weakobj = NULL;
    for (obj = obj_list;  obj != NULL;  obj = obj->next) {
	if (obj != refobj || !refobj->symbolic) {
	    const Elf32_Sym *def = symlook_obj(name, hash, obj, in_plt);
	    if (def != NULL) {
		if (ELF32_ST_BIND(def->st_info) == STB_WEAK) {
		    if (weakdef == NULL) {
			weakdef = def;
			weakobj = obj;
		    }
		} else {
		    strongdef = def;
		    strongobj = obj;
		    break;	/* We are done. */
		}
	    }
	}
    }

    /*
     * If we still don't have a strong definition, search the dynamic
     * linker itself, and possibly resolve the symbol from there.
     * This is how the application links to dynamic linker services
     * such as dlopen.  Only the values listed in the "exports" array
     * can be resolved from the dynamic linker.
     */
    if (strongdef == NULL) {
	const Elf32_Sym *def = symlook_obj(name, hash, &obj_rtld, in_plt);
	if (def != NULL && is_exported(def)) {
	    if (ELF32_ST_BIND(def->st_info) == STB_WEAK) {
		if (weakdef == NULL) {
		    weakdef = def;
		    weakobj = &obj_rtld;
		}
	    } else {
		strongdef = def;
		strongobj = &obj_rtld;
	    }
	}
    }

    if (strongdef != NULL) {
	*defobj_out = strongobj;
	return strongdef;
    }
    if (weakdef != NULL) {
	*defobj_out = weakobj;
	return weakdef;
    }

    _rtld_error("%s: Undefined symbol \"%s\"", refobj->path, name);
    return NULL;
}

/*
 * Initialize the dynamic linker.  The argument is the address at which
 * the dynamic linker has been mapped into memory.  The primary task of
 * this function is to relocate the dynamic linker.
 */
static void
init_rtld(caddr_t mapbase)
{
    /* Conjure up an Obj_Entry structure for the dynamic linker. */

    obj_rtld.path = "/usr/libexec/ld-elf.so.1";
    obj_rtld.rtld = true;
    obj_rtld.mapbase = mapbase;
    obj_rtld.relocbase = mapbase;
    obj_rtld.got = get_got_address();
    obj_rtld.dynamic = (const Elf32_Dyn *) (obj_rtld.mapbase + obj_rtld.got[0]);

    digest_dynamic(&obj_rtld);
    assert(obj_rtld.needed == NULL);
    assert(!obj_rtld.textrel);

    /*
     * Temporarily put the dynamic linker entry into the object list, so
     * that symbols can be found.
     */
    obj_list = &obj_rtld;
    obj_tail = &obj_rtld.next;

    relocate_objects(&obj_rtld, true);

    /* Make the object list empty again. */
    obj_list = NULL;
    obj_tail = &obj_list;

    r_debug.r_brk = r_debug_state;
    r_debug.r_state = RT_CONSISTENT;
}

static bool
is_exported(const Elf32_Sym *def)
{
    func_ptr_type value;
    const func_ptr_type *p;

    value = (func_ptr_type)(obj_rtld.relocbase + def->st_value);
    for (p = exports;  *p != NULL;  p++)
	if (*p == value)
	    return true;
    return false;
}

/*
 * Given a shared object, traverse its list of needed objects, and load
 * each of them.  Returns 0 on success.  Generates an error message and
 * returns -1 on failure.
 */
static int
load_needed_objects(Obj_Entry *first)
{
    Obj_Entry *obj;

    for (obj = first;  obj != NULL;  obj = obj->next) {
	Needed_Entry *needed;

	for (needed = obj->needed;  needed != NULL;  needed = needed->next) {
	    const char *name = obj->strtab + needed->name;
	    char *path = find_library(name, obj);

	    needed->obj = NULL;
	    if (path == NULL && !ld_tracing)
		return -1;

	    if (path) {
		needed->obj = load_object(path);
		if (needed->obj == NULL && !ld_tracing)
		    return -1;		/* XXX - cleanup */
	    }
	}
    }

    return 0;
}

/*
 * Load a shared object into memory, if it is not already loaded.  The
 * argument must be a string allocated on the heap.  This function assumes
 * responsibility for freeing it when necessary.
 *
 * Returns a pointer to the Obj_Entry for the object.  Returns NULL
 * on failure.
 */
static Obj_Entry *
load_object(char *path)
{
    Obj_Entry *obj;

    for (obj = obj_list->next;  obj != NULL;  obj = obj->next)
	if (strcmp(obj->path, path) == 0)
	    break;

    if (obj == NULL) {	/* First use of this object, so we must map it in */
	int fd;

	if ((fd = open(path, O_RDONLY)) == -1) {
	    _rtld_error("Cannot open \"%s\"", path);
	    return NULL;
	}
	obj = map_object(fd);
	close(fd);
	if (obj == NULL) {
	    free(path);
	    return NULL;
	}

	obj->path = path;
	digest_dynamic(obj);

	*obj_tail = obj;
	obj_tail = &obj->next;
	linkmap_add(obj);	/* for GDB */

	dbg("  %p .. %p: %s", obj->mapbase,
	  obj->mapbase + obj->mapsize - 1, obj->path);
	if (obj->textrel)
	    dbg("  WARNING: %s has impure text", obj->path);
    } else
	free(path);

    obj->refcount++;
    return obj;
}

static Obj_Entry *
obj_from_addr(const void *addr)
{
    unsigned long endhash;
    Obj_Entry *obj;

    endhash = elf_hash(END_SYM);
    for (obj = obj_list;  obj != NULL;  obj = obj->next) {
	const Elf32_Sym *endsym;

	if (addr < (void *) obj->mapbase)
	    continue;
	if ((endsym = symlook_obj(END_SYM, endhash, obj, true)) == NULL)
	    continue;	/* No "end" symbol?! */
	if (addr < (void *) (obj->relocbase + endsym->st_value))
	    return obj;
    }
    return NULL;
}

/*
 * Relocate newly-loaded shared objects.  The argument is a pointer to
 * the Obj_Entry for the first such object.  All objects from the first
 * to the end of the list of objects are relocated.  Returns 0 on success,
 * or -1 on failure.
 */
static int
relocate_objects(Obj_Entry *first, bool bind_now)
{
    Obj_Entry *obj;

    for (obj = first;  obj != NULL;  obj = obj->next) {
	const Elf32_Rel *rellim;
	const Elf32_Rel *rel;

	if (obj->nbuckets == 0 || obj->nchains == 0 || obj->buckets == NULL ||
	    obj->symtab == NULL || obj->strtab == NULL) {
	    _rtld_error("%s: Shared object has no run-time symbol table",
	      obj->path);
	    return -1;
	}

	if (obj->textrel) {
	    /* There are relocations to the write-protected text segment. */
	    if (mprotect(obj->mapbase, obj->textsize,
	      PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
		_rtld_error("%s: Cannot write-enable text segment: %s",
		  obj->path, strerror(errno));
		return -1;
	    }
	}

	/* Process the non-PLT relocations. */
	rellim = (const Elf32_Rel *) ((caddr_t) obj->rel + obj->relsize);
	for (rel = obj->rel;  rel < rellim;  rel++) {
	    Elf32_Addr *where = (Elf32_Addr *) (obj->relocbase + rel->r_offset);

	    switch (ELF32_R_TYPE(rel->r_info)) {

	    case R_386_NONE:
		break;

	    case R_386_32:
		{
		    const Elf32_Sym *def;
		    const Obj_Entry *defobj;

		    def = find_symdef(ELF32_R_SYM(rel->r_info), obj, &defobj,
		      false);
		    if (def == NULL)
			return -1;

		    *where += (Elf32_Addr) (defobj->relocbase + def->st_value);
		}
		break;

	    case R_386_PC32:
		/*
		 * I don't think the dynamic linker should ever see this
		 * type of relocation.  But the binutils-2.6 tools sometimes
		 * generate it.
		 */
		{
		    const Elf32_Sym *def;
		    const Obj_Entry *defobj;

		    def = find_symdef(ELF32_R_SYM(rel->r_info), obj, &defobj,
		      false);
		    if (def == NULL)
			return -1;

		    *where +=
		      (Elf32_Addr) (defobj->relocbase + def->st_value) -
		      (Elf32_Addr) where;
		}
		break;

	    case R_386_COPY:
		/*
		 * These are deferred until all other relocations have
		 * been done.  All we do here is make sure that the COPY
		 * relocation is not in a shared library.  They are allowed
		 * only in executable files.
		 */
		if (!obj->mainprog) {
		    _rtld_error("%s: Unexpected R_386_COPY relocation"
		      " in shared library", obj->path);
		    return -1;
		}
		break;

	    case R_386_GLOB_DAT:
		{
		    const Elf32_Sym *def;
		    const Obj_Entry *defobj;

		    def = find_symdef(ELF32_R_SYM(rel->r_info), obj, &defobj,
		      false);
		    if (def == NULL)
			return -1;

		    *where = (Elf32_Addr) (defobj->relocbase + def->st_value);
		}
		break;

	    case R_386_RELATIVE:
		*where += (Elf32_Addr) obj->relocbase;
		break;

	    default:
		_rtld_error("%s: Unsupported relocation type %d"
		  " in non-PLT relocations\n", obj->path,
		  ELF32_R_TYPE(rel->r_info));
		return -1;
	    }
	}

	if (obj->textrel) {	/* Re-protected the text segment. */
	    if (mprotect(obj->mapbase, obj->textsize,
	      PROT_READ|PROT_EXEC) == -1) {
		_rtld_error("%s: Cannot write-protect text segment: %s",
		  obj->path, strerror(errno));
		return -1;
	    }
	}

	/* Process the PLT relocations. */
	rellim = (const Elf32_Rel *) ((caddr_t) obj->pltrel + obj->pltrelsize);
	if (bind_now) {
	    /* Fully resolve procedure addresses now */
	    for (rel = obj->pltrel;  rel < rellim;  rel++) {
		Elf32_Addr *where = (Elf32_Addr *)
		  (obj->relocbase + rel->r_offset);
		const Elf32_Sym *def;
		const Obj_Entry *defobj;

		assert(ELF32_R_TYPE(rel->r_info) == R_386_JMP_SLOT);

		def = find_symdef(ELF32_R_SYM(rel->r_info), obj, &defobj, true);
		if (def == NULL)
		    return -1;

		*where = (Elf32_Addr) (defobj->relocbase + def->st_value);
	    }
	} else {	/* Just relocate the GOT slots pointing into the PLT */
	    for (rel = obj->pltrel;  rel < rellim;  rel++) {
		Elf32_Addr *where = (Elf32_Addr *)
		  (obj->relocbase + rel->r_offset);
		*where += (Elf32_Addr) obj->relocbase;
	    }
	}

	/*
	 * Set up the magic number and version in the Obj_Entry.  These
	 * were checked in the crt1.o from the original ElfKit, so we
	 * set them for backward compatibility.
	 */
	obj->magic = RTLD_MAGIC;
	obj->version = RTLD_VERSION;

	/* Set the special GOT entries. */
	if (obj->got) {
	    obj->got[1] = (Elf32_Addr) obj;
	    obj->got[2] = (Elf32_Addr) &_rtld_bind_start;
	}
    }

    return 0;
}

/*
 * Cleanup procedure.  It will be called (by the atexit mechanism) just
 * before the process exits.
 */
static void
rtld_exit(void)
{
    dbg("rtld_exit()");
    call_fini_functions(obj_list->next);
}

static char *
search_library_path(const char *name, const char *path)
{
    size_t namelen = strlen(name);
    const char *p = path;

    if (p == NULL)
	return NULL;

    p += strspn(p, ":;");
    while (*p != '\0') {
	size_t len = strcspn(p, ":;");

	if (*p == '/' || trust) {
	    char *pathname;
	    const char *dir = p;
	    size_t dirlen = len;

	    pathname = xmalloc(dirlen + 1 + namelen + 1);
	    strncpy(pathname, dir, dirlen);
	    pathname[dirlen] = '/';
	    strcpy(pathname + dirlen + 1, name);

	    dbg("  Trying \"%s\"", pathname);
	    if (access(pathname, F_OK) == 0)		/* We found it */
		return pathname;

	    free(pathname);
	}
	p += len;
	p += strspn(p, ":;");
    }

    return NULL;
}

int
dlclose(void *handle)
{
    Obj_Entry *root = dlcheck(handle);

    if (root == NULL)
	return -1;

    GDB_STATE(RT_DELETE);

    root->dl_refcount--;
    unref_object_dag(root);
    if (root->refcount == 0) {	/* We are finished with some objects. */
	Obj_Entry *obj;
	Obj_Entry **linkp;

	/* Finalize objects that are about to be unmapped. */
	for (obj = obj_list->next;  obj != NULL;  obj = obj->next)
	    if (obj->refcount == 0 && obj->fini != NULL)
		(*obj->fini)();

	/* Unmap all objects that are no longer referenced. */
	linkp = &obj_list->next;
	while ((obj = *linkp) != NULL) {
	    if (obj->refcount == 0) {
		munmap(obj->mapbase, obj->mapsize);
		free(obj->path);
		while (obj->needed != NULL) {
		    Needed_Entry *needed = obj->needed;
		    obj->needed = needed->next;
		    free(needed);
		}
		linkmap_delete(obj);
		*linkp = obj->next;
		free(obj);
	    } else
		linkp = &obj->next;
	}
    }

    GDB_STATE(RT_CONSISTENT);

    return 0;
}

const char *
dlerror(void)
{
    char *msg = error_message;
    error_message = NULL;
    return msg;
}

void *
dlopen(const char *name, int mode)
{
    Obj_Entry **old_obj_tail = obj_tail;
    Obj_Entry *obj = NULL;

    GDB_STATE(RT_ADD);

    if (name == NULL)
	obj = obj_main;
    else {
	char *path = find_library(name, NULL);
	if (path != NULL)
	    obj = load_object(path);
    }

    if (obj) {
	obj->dl_refcount++;
	if (*old_obj_tail != NULL) {		/* We loaded something new. */
	    assert(*old_obj_tail == obj);

	    /* XXX - Clean up properly after an error. */
	    if (load_needed_objects(obj) == -1) {
		obj->dl_refcount--;
		obj = NULL;
	    } else if (relocate_objects(obj, mode == RTLD_NOW) == -1) {
		obj->dl_refcount--;
		obj = NULL;
	    } else
		call_init_functions(obj);
	}
    }

    GDB_STATE(RT_CONSISTENT);

    return obj;
}

void *
dlsym(void *handle, const char *name)
{
    const Obj_Entry *obj;
    unsigned long hash;
    const Elf32_Sym *def;

    hash = elf_hash(name);

    if (handle == RTLD_NEXT) {
	void *retaddr;

	retaddr = __builtin_return_address(0);	/* __GNUC__ only */
	if ((obj = obj_from_addr(retaddr)) == NULL) {
	    _rtld_error("Cannot determine caller's shared object");
	    return NULL;
	}
	def = NULL;
	while ((obj = obj->next) != NULL)
	    if ((def = symlook_obj(name, hash, obj, true)) != NULL)
		break;
    } else {
	if ((obj = dlcheck(handle)) == NULL)
	    return NULL;
	/*
	 * XXX - This isn't correct.  The search should include the whole
	 * DAG rooted at the given object.
	 */
	def = symlook_obj(name, hash, obj, true);
    }

    if (def != NULL)
	return obj->relocbase + def->st_value;

    _rtld_error("Undefined symbol \"%s\"", name);
    return NULL;
}


/*
 * Search the symbol table of a single shared object for a symbol of
 * the given name.  Returns a pointer to the symbol, or NULL if no
 * definition was found.
 *
 * The symbol's hash value is passed in for efficiency reasons; that
 * eliminates many recomputations of the hash value.
 */
static const Elf32_Sym *
symlook_obj(const char *name, unsigned long hash, const Obj_Entry *obj,
  bool in_plt)
{
    unsigned long symnum = obj->buckets[hash % obj->nbuckets];

    while (symnum != STN_UNDEF) {
	const Elf32_Sym *symp;
	const char *strp;

	assert(symnum < obj->nchains);
	symp = obj->symtab + symnum;
	assert(symp->st_name != 0);
	strp = obj->strtab + symp->st_name;

	if (strcmp(name, strp) == 0)
	    return symp->st_shndx != SHN_UNDEF ||
	      (!in_plt && symp->st_value != 0 &&
	      ELF32_ST_TYPE(symp->st_info) == STT_FUNC) ? symp : NULL;

	symnum = obj->chains[symnum];
    }

    return NULL;
}

static void
unref_object_dag(Obj_Entry *root)
{
    assert(root->refcount != 0);
    root->refcount--;
    if (root->refcount == 0) {
	const Needed_Entry *needed;

	for (needed = root->needed;  needed != NULL;  needed = needed->next)
	    unref_object_dag(needed->obj);
    }
}

/*
 * Non-mallocing printf, for use by malloc itself.
 * XXX - This doesn't belong in this module.
 */
void
xprintf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    (void)write(1, buf, strlen(buf));
    va_end(ap);
}

void
r_debug_state(void)
{
}

static void
linkmap_add(Obj_Entry *obj)
{
    struct link_map *l = &obj->linkmap;
    struct link_map *prev;

    obj->linkmap.l_name = obj->path;
    obj->linkmap.l_addr = obj->mapbase;
    obj->linkmap.l_ld = obj->dynamic;
#ifdef __mips__
    /* GDB needs load offset on MIPS to use the symbols */
    obj->linkmap.l_offs = obj->relocbase;
#endif

    if (r_debug.r_map == NULL) {
	r_debug.r_map = l;
	return;
    }
    
    for (prev = r_debug.r_map; prev->l_next != NULL; prev = prev->l_next)
	;
    l->l_prev = prev;
    prev->l_next = l;
    l->l_next = NULL;
}

void linkmap_delete(Obj_Entry *obj)
{
    struct link_map *l = &obj->linkmap;

    if (l->l_prev == NULL) {
	if ((r_debug.r_map = l->l_next) != NULL)
	    l->l_next->l_prev = NULL;
	return;
    }

    if ((l->l_prev->l_next = l->l_next) != NULL)
	l->l_next->l_prev = l->l_prev;
}

void trace_loaded_objects(Obj_Entry *obj)
{
    char	*fmt1, *fmt2, *fmt, *main_local;
    int		c;

    if ((main_local = getenv("LD_TRACE_LOADED_OBJECTS_PROGNAME")) == NULL)
	main_local = "";

    if ((fmt1 = getenv("LD_TRACE_LOADED_OBJECTS_FMT1")) == NULL)
	fmt1 = "\t%o => %p (%x)\n";

    if ((fmt2 = getenv("LD_TRACE_LOADED_OBJECTS_FMT2")) == NULL)
	fmt2 = "\t%o (%x)\n";

    for (; obj; obj = obj->next) {
	Needed_Entry		*needed;
	char			*name, *path;
	bool			is_lib;

	for (needed = obj->needed; needed; needed = needed->next) {
	    name = (char *)obj->strtab + needed->name;
	    if (!strncmp(name, "lib", 3)) {
		is_lib = true;	/* XXX bogus */
	    } else {
		is_lib = false;
	    }

	    if (needed->obj == NULL)
		path = "not found";
	    else
		path = needed->obj->path;

	    fmt = is_lib ? fmt1 : fmt2;
	    while ((c = *fmt++) != '\0') {
		switch (c) {
		default:
		    putchar(c);
		    continue;
		case '\\':
		    switch (c = *fmt) {
		    case '\0':
			continue;
		    case 'n':
			putchar('\n');
			break;
		    case 't':
			putchar('\t');
			break;
		    }
		    break;
		case '%':
		    switch (c = *fmt) {
		    case '\0':
			continue;
		    case '%':
		    default:
			putchar(c);
			break;
		    case 'A':
			printf("%s", main_local);
			break;
		    case 'a':
			printf("%s", obj_main->path);
			break;
		    case 'o':
			printf("%s", name);
			break;
#if 0
		    case 'm':
			printf("%d", sodp->sod_major);
			break;
		    case 'n':
			printf("%d", sodp->sod_minor);
			break;
#endif
		    case 'p':
			printf("%s", path);
			break;
		    case 'x':
			printf("%p", needed->obj ? needed->obj->mapbase : 0);
			break;
		    }
		    break;
		}
		++fmt;
	    }
	}
    }
}
