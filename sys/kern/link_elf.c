/*-
 * Copyright (c) 1998-2000 Doug Rabson
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

#include "opt_gdb.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#ifdef GPROF
#include <sys/gmon.h>
#endif
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/linker.h>

#include <machine/elf.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#ifdef SPARSE_MAPPING
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#endif
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <sys/link_elf.h>

#include "linker_if.h"

typedef struct elf_file {
    struct linker_file	lf;		/* Common fields */
    int			preloaded;	/* Was file pre-loaded */
    caddr_t		address;	/* Relocation address */
#ifdef SPARSE_MAPPING
    vm_object_t		object;		/* VM object to hold file pages */
#endif
    Elf_Dyn*		dynamic;	/* Symbol table etc. */
    Elf_Hashelt		nbuckets;	/* DT_HASH info */
    Elf_Hashelt		nchains;
    const Elf_Hashelt*	buckets;
    const Elf_Hashelt*	chains;
    caddr_t		hash;
    caddr_t		strtab;		/* DT_STRTAB */
    int			strsz;		/* DT_STRSZ */
    const Elf_Sym*	symtab;		/* DT_SYMTAB */
    Elf_Addr*		got;		/* DT_PLTGOT */
    const Elf_Rel*	pltrel;		/* DT_JMPREL */
    int			pltrelsize;	/* DT_PLTRELSZ */
    const Elf_Rela*	pltrela;	/* DT_JMPREL */
    int			pltrelasize;	/* DT_PLTRELSZ */
    const Elf_Rel*	rel;		/* DT_REL */
    int			relsize;	/* DT_RELSZ */
    const Elf_Rela*	rela;		/* DT_RELA */
    int			relasize;	/* DT_RELASZ */
    caddr_t		modptr;
    const Elf_Sym*	ddbsymtab;	/* The symbol table we are using */
    long		ddbsymcnt;	/* Number of symbols */
    caddr_t		ddbstrtab;	/* String table */
    long		ddbstrcnt;	/* number of bytes in string table */
    caddr_t		symbase;	/* malloc'ed symbold base */
    caddr_t		strbase;	/* malloc'ed string base */
#ifdef GDB
    struct link_map	gdb;		/* hooks for gdb */
#endif
} *elf_file_t;

static int	link_elf_link_common_finish(linker_file_t);
static int	link_elf_link_preload(linker_class_t cls,
				      const char*, linker_file_t*);
static int	link_elf_link_preload_finish(linker_file_t);
static int	link_elf_load_file(linker_class_t, const char*, linker_file_t*);
static int	link_elf_lookup_symbol(linker_file_t, const char*,
				       c_linker_sym_t*);
static int	link_elf_symbol_values(linker_file_t, c_linker_sym_t, linker_symval_t*);
static int	link_elf_search_symbol(linker_file_t, caddr_t value,
				       c_linker_sym_t* sym, long* diffp);

static void	link_elf_unload_file(linker_file_t);
static void	link_elf_unload_preload(linker_file_t);
static int	link_elf_lookup_set(linker_file_t, const char *,
				    void ***, void ***, int *);
static int	link_elf_each_function_name(linker_file_t,
				int (*)(const char *, void *),
				void *);
static void	link_elf_reloc_local(linker_file_t);
static Elf_Addr	elf_lookup(linker_file_t lf, Elf_Word symidx, int deps);

static kobj_method_t link_elf_methods[] = {
    KOBJMETHOD(linker_lookup_symbol,	link_elf_lookup_symbol),
    KOBJMETHOD(linker_symbol_values,	link_elf_symbol_values),
    KOBJMETHOD(linker_search_symbol,	link_elf_search_symbol),
    KOBJMETHOD(linker_unload,		link_elf_unload_file),
    KOBJMETHOD(linker_load_file,	link_elf_load_file),
    KOBJMETHOD(linker_link_preload,	link_elf_link_preload),
    KOBJMETHOD(linker_link_preload_finish, link_elf_link_preload_finish),
    KOBJMETHOD(linker_lookup_set,	link_elf_lookup_set),
    KOBJMETHOD(linker_each_function_name, link_elf_each_function_name),
    { 0, 0 }
};

static struct linker_class link_elf_class = {
#if ELF_TARG_CLASS == ELFCLASS32
    "elf32",
#else
    "elf64",
#endif
    link_elf_methods, sizeof(struct elf_file)
};

static int		parse_dynamic(elf_file_t ef);
static int		relocate_file(elf_file_t ef);
static int		link_elf_preload_parse_symbols(elf_file_t ef);

#ifdef GDB
static void		r_debug_state(struct r_debug *dummy_one,
				      struct link_map *dummy_two);

/*
 * A list of loaded modules for GDB to use for loading symbols.
 */
struct r_debug r_debug;

#define GDB_STATE(s)	r_debug.r_state = s; r_debug_state(NULL, NULL);

/*
 * Function for the debugger to set a breakpoint on to gain control.
 */
static void
r_debug_state(struct r_debug *dummy_one __unused,
	      struct link_map *dummy_two __unused)
{
}

static void
link_elf_add_gdb(struct link_map *l)
{
    struct link_map *prev;

    l->l_next = NULL;

    if (r_debug.r_map == NULL) {
	/* Add first. */
	l->l_prev = NULL;
	r_debug.r_map = l;
    } else {
	/* Append to list. */
	for (prev = r_debug.r_map; prev->l_next != NULL; prev = prev->l_next)
	    ;
	l->l_prev = prev;
	prev->l_next = l;
    }
}

static void
link_elf_delete_gdb(struct link_map *l)
{
    if (l->l_prev == NULL) {
	/* Remove first. */
	if ((r_debug.r_map = l->l_next) != NULL)
	    l->l_next->l_prev = NULL;
    } else {
	/* Remove any but first. */
	if ((l->l_prev->l_next = l->l_next) != NULL)
	    l->l_next->l_prev = l->l_prev;
    }
}
#endif /* GDB */

#ifdef __ia64__
Elf_Addr link_elf_get_gp(linker_file_t);
#endif

/*
 * The kernel symbol table starts here.
 */
extern struct _dynamic _DYNAMIC;

static void
link_elf_error(const char *s)
{
    printf("kldload: %s\n", s);
}

/*
 * Actions performed after linking/loading both the preloaded kernel and any
 * modules; whether preloaded or dynamicly loaded.
 */
static int
link_elf_link_common_finish(linker_file_t lf)
{
#ifdef GDB
    elf_file_t ef = (elf_file_t)lf;
    char *newfilename;
#endif
    int error;

    /* Notify MD code that a module is being loaded. */
    error = elf_cpu_load_file(lf);
    if (error)
	return (error);

#ifdef GDB
    GDB_STATE(RT_ADD);
    ef->gdb.l_addr = lf->address;
    newfilename = malloc(strlen(lf->filename) + 1, M_LINKER, M_WAITOK);
    strcpy(newfilename, lf->filename);
    ef->gdb.l_name = newfilename;
    ef->gdb.l_ld = ef->dynamic;
    link_elf_add_gdb(&ef->gdb);
    GDB_STATE(RT_CONSISTENT);
#endif

    return (0);
}

static void
link_elf_init(void* arg)
{
    Elf_Dyn	*dp;
    caddr_t	modptr, baseptr, sizeptr;
    elf_file_t	ef;
    char	*modname;

    linker_add_class(&link_elf_class);

    dp = (Elf_Dyn*) &_DYNAMIC;
    modname = NULL;
    modptr = preload_search_by_type("elf" __XSTRING(__ELF_WORD_SIZE) " kernel");
    if (modptr == NULL)
	modptr = preload_search_by_type("elf kernel");
    if (modptr)
	modname = (char *)preload_search_info(modptr, MODINFO_NAME);
    if (modname == NULL)
	modname = "kernel";
    linker_kernel_file = linker_make_file(modname, &link_elf_class);
    if (linker_kernel_file == NULL)
	panic("link_elf_init: Can't create linker structures for kernel");

    ef = (elf_file_t) linker_kernel_file;
    ef->preloaded = 1;
    ef->address = 0;
#ifdef SPARSE_MAPPING
    ef->object = 0;
#endif
    ef->dynamic = dp;

    if (dp)
	parse_dynamic(ef);
    linker_kernel_file->address = (caddr_t) KERNBASE;
    linker_kernel_file->size = -(intptr_t)linker_kernel_file->address;

    if (modptr) {
	ef->modptr = modptr;
	baseptr = preload_search_info(modptr, MODINFO_ADDR);
	if (baseptr)
	    linker_kernel_file->address = *(caddr_t *)baseptr;
	sizeptr = preload_search_info(modptr, MODINFO_SIZE);
	if (sizeptr)
	    linker_kernel_file->size = *(size_t *)sizeptr;
    }
    (void)link_elf_preload_parse_symbols(ef);

#ifdef GDB
    r_debug.r_map = NULL;
    r_debug.r_brk = r_debug_state;
    r_debug.r_state = RT_CONSISTENT;
#endif

    (void)link_elf_link_common_finish(linker_kernel_file);
}

SYSINIT(link_elf, SI_SUB_KLD, SI_ORDER_SECOND, link_elf_init, 0);

static int
link_elf_preload_parse_symbols(elf_file_t ef)
{
    caddr_t	pointer;
    caddr_t	ssym, esym, base;
    caddr_t	strtab;
    int		strcnt;
    Elf_Sym*	symtab;
    int		symcnt;

    if (ef->modptr == NULL)
	return 0;
    pointer = preload_search_info(ef->modptr, MODINFO_METADATA|MODINFOMD_SSYM);
    if (pointer == NULL)
	return 0;
    ssym = *(caddr_t *)pointer;
    pointer = preload_search_info(ef->modptr, MODINFO_METADATA|MODINFOMD_ESYM);
    if (pointer == NULL)
	return 0;
    esym = *(caddr_t *)pointer;

    base = ssym;

    symcnt = *(long *)base;
    base += sizeof(long);
    symtab = (Elf_Sym *)base;
    base += roundup(symcnt, sizeof(long));

    if (base > esym || base < ssym) {
	printf("Symbols are corrupt!\n");
	return EINVAL;
    }

    strcnt = *(long *)base;
    base += sizeof(long);
    strtab = base;
    base += roundup(strcnt, sizeof(long));

    if (base > esym || base < ssym) {
	printf("Symbols are corrupt!\n");
	return EINVAL;
    }

    ef->ddbsymtab = symtab;
    ef->ddbsymcnt = symcnt / sizeof(Elf_Sym);
    ef->ddbstrtab = strtab;
    ef->ddbstrcnt = strcnt;

    return 0;
}

static int
parse_dynamic(elf_file_t ef)
{
    Elf_Dyn *dp;
    int plttype = DT_REL;

    for (dp = ef->dynamic; dp->d_tag != DT_NULL; dp++) {
	switch (dp->d_tag) {
	case DT_HASH:
	{
	    /* From src/libexec/rtld-elf/rtld.c */
	    const Elf_Hashelt *hashtab = (const Elf_Hashelt *)
		(ef->address + dp->d_un.d_ptr);
	    ef->nbuckets = hashtab[0];
	    ef->nchains = hashtab[1];
	    ef->buckets = hashtab + 2;
	    ef->chains = ef->buckets + ef->nbuckets;
	    break;
	}
	case DT_STRTAB:
	    ef->strtab = (caddr_t) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_STRSZ:
	    ef->strsz = dp->d_un.d_val;
	    break;
	case DT_SYMTAB:
	    ef->symtab = (Elf_Sym*) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_SYMENT:
	    if (dp->d_un.d_val != sizeof(Elf_Sym))
		return ENOEXEC;
	    break;
	case DT_PLTGOT:
	    ef->got = (Elf_Addr *) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_REL:
	    ef->rel = (const Elf_Rel *) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_RELSZ:
	    ef->relsize = dp->d_un.d_val;
	    break;
	case DT_RELENT:
	    if (dp->d_un.d_val != sizeof(Elf_Rel))
		return ENOEXEC;
	    break;
	case DT_JMPREL:
	    ef->pltrel = (const Elf_Rel *) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_PLTRELSZ:
	    ef->pltrelsize = dp->d_un.d_val;
	    break;
	case DT_RELA:
	    ef->rela = (const Elf_Rela *) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_RELASZ:
	    ef->relasize = dp->d_un.d_val;
	    break;
	case DT_RELAENT:
	    if (dp->d_un.d_val != sizeof(Elf_Rela))
		return ENOEXEC;
	    break;
	case DT_PLTREL:
	    plttype = dp->d_un.d_val;
	    if (plttype != DT_REL && plttype != DT_RELA)
		return ENOEXEC;
	    break;
#ifdef GDB
	case DT_DEBUG:
	    dp->d_un.d_ptr = (Elf_Addr) &r_debug;
	    break;
#endif
	}
    }

    if (plttype == DT_RELA) {
	ef->pltrela = (const Elf_Rela *) ef->pltrel;
	ef->pltrel = NULL;
	ef->pltrelasize = ef->pltrelsize;
	ef->pltrelsize = 0;
    }

    ef->ddbsymtab = ef->symtab;
    ef->ddbsymcnt = ef->nchains;
    ef->ddbstrtab = ef->strtab;
    ef->ddbstrcnt = ef->strsz;

    return 0;
}

static int
link_elf_link_preload(linker_class_t cls,
		      const char* filename, linker_file_t *result)
{
    caddr_t		modptr, baseptr, sizeptr, dynptr;
    char		*type;
    elf_file_t		ef;
    linker_file_t	lf;
    int			error;
    vm_offset_t		dp;

    /* Look to see if we have the file preloaded */
    modptr = preload_search_by_name(filename);
    if (modptr == NULL)
	return ENOENT;

    type = (char *)preload_search_info(modptr, MODINFO_TYPE);
    baseptr = preload_search_info(modptr, MODINFO_ADDR);
    sizeptr = preload_search_info(modptr, MODINFO_SIZE);
    dynptr = preload_search_info(modptr, MODINFO_METADATA|MODINFOMD_DYNAMIC);
    if (type == NULL ||
	(strcmp(type, "elf" __XSTRING(__ELF_WORD_SIZE) " module") != 0 &&
	 strcmp(type, "elf module") != 0))
	return (EFTYPE);
    if (baseptr == NULL || sizeptr == NULL || dynptr == NULL)
	return (EINVAL);

    lf = linker_make_file(filename, &link_elf_class);
    if (lf == NULL) {
	return ENOMEM;
    }

    ef = (elf_file_t) lf;
    ef->preloaded = 1;
    ef->modptr = modptr;
    ef->address = *(caddr_t *)baseptr;
#ifdef SPARSE_MAPPING
    ef->object = 0;
#endif
    dp = (vm_offset_t)ef->address + *(vm_offset_t *)dynptr;
    ef->dynamic = (Elf_Dyn *)dp;
    lf->address = ef->address;
    lf->size = *(size_t *)sizeptr;

    error = parse_dynamic(ef);
    if (error) {
	linker_file_unload(lf, LINKER_UNLOAD_FORCE);
	return error;
    }
    link_elf_reloc_local(lf);
    *result = lf;
    return (0);
}

static int
link_elf_link_preload_finish(linker_file_t lf)
{
    elf_file_t		ef;
    int error;

    ef = (elf_file_t) lf;
#if 0	/* this will be more trouble than it's worth for now */
    for (dp = ef->dynamic; dp->d_tag != DT_NULL; dp++) {
	if (dp->d_tag != DT_NEEDED)
	    continue;
	modname = ef->strtab + dp->d_un.d_val;
	error = linker_load_module(modname, lf);
	if (error)
	    goto out;
    }
#endif
    error = relocate_file(ef);
    if (error)
	return error;
    (void)link_elf_preload_parse_symbols(ef);

    return (link_elf_link_common_finish(lf));
}

static int
link_elf_load_file(linker_class_t cls, const char* filename,
	linker_file_t* result)
{
    struct nameidata nd;
    struct thread* td = curthread;	/* XXX */
    Elf_Ehdr *hdr;
    caddr_t firstpage;
    int nbytes, i;
    Elf_Phdr *phdr;
    Elf_Phdr *phlimit;
    Elf_Phdr *segs[2];
    int nsegs;
    Elf_Phdr *phdyn;
    Elf_Phdr *phphdr;
    caddr_t mapbase;
    size_t mapsize;
    Elf_Off base_offset;
    Elf_Addr base_vaddr;
    Elf_Addr base_vlimit;
    int error = 0;
    int resid, flags;
    elf_file_t ef;
    linker_file_t lf;
    Elf_Shdr *shdr;
    int symtabindex;
    int symstrindex;
    int symcnt;
    int strcnt;

    GIANT_REQUIRED;

    shdr = NULL;
    lf = NULL;

    NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, filename, td);
    flags = FREAD;
    error = vn_open(&nd, &flags, 0, -1);
    if (error)
	return error;
    NDFREE(&nd, NDF_ONLY_PNBUF);
#ifdef MAC
    error = mac_check_kld_load(curthread->td_ucred, nd.ni_vp);
    if (error) {
	firstpage = NULL;
	goto out;
    }
#endif

    /*
     * Read the elf header from the file.
     */
    firstpage = malloc(PAGE_SIZE, M_LINKER, M_WAITOK);
    if (firstpage == NULL) {
	error = ENOMEM;
	goto out;
    }
    hdr = (Elf_Ehdr *)firstpage;
    error = vn_rdwr(UIO_READ, nd.ni_vp, firstpage, PAGE_SIZE, 0,
		    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
		    &resid, td);
    nbytes = PAGE_SIZE - resid;
    if (error)
	goto out;

    if (!IS_ELF(*hdr)) {
	error = ENOEXEC;
	goto out;
    }

    if (hdr->e_ident[EI_CLASS] != ELF_TARG_CLASS
      || hdr->e_ident[EI_DATA] != ELF_TARG_DATA) {
	link_elf_error("Unsupported file layout");
	error = ENOEXEC;
	goto out;
    }
    if (hdr->e_ident[EI_VERSION] != EV_CURRENT
      || hdr->e_version != EV_CURRENT) {
	link_elf_error("Unsupported file version");
	error = ENOEXEC;
	goto out;
    }
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) {
	link_elf_error("Unsupported file type");
	error = ENOEXEC;
	goto out;
    }
    if (hdr->e_machine != ELF_TARG_MACH) {
	link_elf_error("Unsupported machine");
	error = ENOEXEC;
	goto out;
    }

    /*
     * We rely on the program header being in the first page.  This is
     * not strictly required by the ABI specification, but it seems to
     * always true in practice.  And, it simplifies things considerably.
     */
    if (!((hdr->e_phentsize == sizeof(Elf_Phdr)) &&
	  (hdr->e_phoff + hdr->e_phnum*sizeof(Elf_Phdr) <= PAGE_SIZE) &&
	  (hdr->e_phoff + hdr->e_phnum*sizeof(Elf_Phdr) <= nbytes)))
	link_elf_error("Unreadable program headers");

    /*
     * Scan the program header entries, and save key information.
     *
     * We rely on there being exactly two load segments, text and data,
     * in that order.
     */
    phdr = (Elf_Phdr *) (firstpage + hdr->e_phoff);
    phlimit = phdr + hdr->e_phnum;
    nsegs = 0;
    phdyn = NULL;
    phphdr = NULL;
    while (phdr < phlimit) {
	switch (phdr->p_type) {

	case PT_LOAD:
	    if (nsegs == 2) {
		link_elf_error("Too many sections");
		error = ENOEXEC;
		goto out;
	    }
	    /*
	     * XXX: We just trust they come in right order ??
	     */
	    segs[nsegs] = phdr;
	    ++nsegs;
	    break;

	case PT_PHDR:
	    phphdr = phdr;
	    break;

	case PT_DYNAMIC:
	    phdyn = phdr;
	    break;

	case PT_INTERP:
	    link_elf_error("Unsupported file type");
	    error = ENOEXEC;
	    goto out;
	}

	++phdr;
    }
    if (phdyn == NULL) {
	link_elf_error("Object is not dynamically-linked");
	error = ENOEXEC;
	goto out;
    }
    if (nsegs != 2) {
	link_elf_error("Too few sections");
	error = ENOEXEC;
	goto out;
    }

    /*
     * Allocate the entire address space of the object, to stake out our
     * contiguous region, and to establish the base address for relocation.
     */
    base_offset = trunc_page(segs[0]->p_offset);
    base_vaddr = trunc_page(segs[0]->p_vaddr);
    base_vlimit = round_page(segs[1]->p_vaddr + segs[1]->p_memsz);
    mapsize = base_vlimit - base_vaddr;

    lf = linker_make_file(filename, &link_elf_class);
    if (!lf) {
	error = ENOMEM;
	goto out;
    }

    ef = (elf_file_t) lf;
#ifdef SPARSE_MAPPING
    ef->object = vm_object_allocate(OBJT_DEFAULT, mapsize >> PAGE_SHIFT);
    if (ef->object == NULL) {
	error = ENOMEM;
	goto out;
    }
    vm_object_reference(ef->object);
    ef->address = (caddr_t) vm_map_min(kernel_map);
    error = vm_map_find(kernel_map, ef->object, 0,
			(vm_offset_t *) &ef->address,
			mapsize, 1,
			VM_PROT_ALL, VM_PROT_ALL, 0);
    if (error) {
	vm_object_deallocate(ef->object);
	ef->object = 0;
	goto out;
    }
#else
    ef->address = malloc(mapsize, M_LINKER, M_WAITOK);
    if (!ef->address) {
	error = ENOMEM;
	goto out;
    }
#endif
    mapbase = ef->address;

    /*
     * Read the text and data sections and zero the bss.
     */
    for (i = 0; i < 2; i++) {
	caddr_t segbase = mapbase + segs[i]->p_vaddr - base_vaddr;
	error = vn_rdwr(UIO_READ, nd.ni_vp,
			segbase, segs[i]->p_filesz, segs[i]->p_offset,
			UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
			&resid, td);
	if (error) {
	    goto out;
	}
	bzero(segbase + segs[i]->p_filesz,
	      segs[i]->p_memsz - segs[i]->p_filesz);

#ifdef SPARSE_MAPPING
	/*
	 * Wire down the pages
	 */
	vm_map_wire(kernel_map,
		    (vm_offset_t) segbase,
		    (vm_offset_t) segbase + segs[i]->p_memsz,
		    VM_MAP_WIRE_SYSTEM|VM_MAP_WIRE_NOHOLES);
#endif
    }

#ifdef GPROF
    /* Update profiling information with the new text segment. */
    kmupetext((uintfptr_t)(mapbase + segs[0]->p_vaddr - base_vaddr +
	segs[0]->p_memsz));
#endif

    ef->dynamic = (Elf_Dyn *) (mapbase + phdyn->p_vaddr - base_vaddr);

    lf->address = ef->address;
    lf->size = mapsize;

    error = parse_dynamic(ef);
    if (error)
	goto out;
    link_elf_reloc_local(lf);

    error = linker_load_dependencies(lf);
    if (error)
	goto out;
#if 0	/* this will be more trouble than it's worth for now */
    for (dp = ef->dynamic; dp->d_tag != DT_NULL; dp++) {
	if (dp->d_tag != DT_NEEDED)
	    continue;
	modname = ef->strtab + dp->d_un.d_val;
	error = linker_load_module(modname, lf);
	if (error)
	    goto out;
    }
#endif
    error = relocate_file(ef);
    if (error)
	goto out;

    /* Try and load the symbol table if it's present.  (you can strip it!) */
    nbytes = hdr->e_shnum * hdr->e_shentsize;
    if (nbytes == 0 || hdr->e_shoff == 0)
	goto nosyms;
    shdr = malloc(nbytes, M_LINKER, M_WAITOK | M_ZERO);
    if (shdr == NULL) {
	error = ENOMEM;
	goto out;
    }
    error = vn_rdwr(UIO_READ, nd.ni_vp,
		    (caddr_t)shdr, nbytes, hdr->e_shoff,
		    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
		    &resid, td);
    if (error)
	goto out;
    symtabindex = -1;
    symstrindex = -1;
    for (i = 0; i < hdr->e_shnum; i++) {
	if (shdr[i].sh_type == SHT_SYMTAB) {
	    symtabindex = i;
	    symstrindex = shdr[i].sh_link;
	}
    }
    if (symtabindex < 0 || symstrindex < 0)
	goto nosyms;

    symcnt = shdr[symtabindex].sh_size;
    ef->symbase = malloc(symcnt, M_LINKER, M_WAITOK);
    strcnt = shdr[symstrindex].sh_size;
    ef->strbase = malloc(strcnt, M_LINKER, M_WAITOK);

    if (ef->symbase == NULL || ef->strbase == NULL) {
	error = ENOMEM;
	goto out;
    }
    error = vn_rdwr(UIO_READ, nd.ni_vp,
		    ef->symbase, symcnt, shdr[symtabindex].sh_offset,
		    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
		    &resid, td);
    if (error)
	goto out;
    error = vn_rdwr(UIO_READ, nd.ni_vp,
		    ef->strbase, strcnt, shdr[symstrindex].sh_offset,
		    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
		    &resid, td);
    if (error)
	goto out;

    ef->ddbsymcnt = symcnt / sizeof(Elf_Sym);
    ef->ddbsymtab = (const Elf_Sym *)ef->symbase;
    ef->ddbstrcnt = strcnt;
    ef->ddbstrtab = ef->strbase;

    error = link_elf_link_common_finish(lf);
    if (error)
	goto out;

nosyms:

    *result = lf;

out:
    if (error && lf)
	linker_file_unload(lf, LINKER_UNLOAD_FORCE);
    if (shdr)
	free(shdr, M_LINKER);
    if (firstpage)
	free(firstpage, M_LINKER);
    VOP_UNLOCK(nd.ni_vp, 0, td);
    vn_close(nd.ni_vp, FREAD, td->td_ucred, td);

    return error;
}

static void
link_elf_unload_file(linker_file_t file)
{
    elf_file_t ef = (elf_file_t) file;

#ifdef GDB
    if (ef->gdb.l_ld) {
	GDB_STATE(RT_DELETE);
	free((void *)(uintptr_t)ef->gdb.l_name, M_LINKER);
	link_elf_delete_gdb(&ef->gdb);
	GDB_STATE(RT_CONSISTENT);
    }
#endif

    /* Notify MD code that a module is being unloaded. */
    elf_cpu_unload_file(file);

    if (ef->preloaded) {
	link_elf_unload_preload(file);
	return;
    }

#ifdef SPARSE_MAPPING
    if (ef->object) {
	vm_map_remove(kernel_map, (vm_offset_t) ef->address,
		      (vm_offset_t) ef->address
		      + (ef->object->size << PAGE_SHIFT));
	vm_object_deallocate(ef->object);
    }
#else
    if (ef->address)
	free(ef->address, M_LINKER);
#endif
    if (ef->symbase)
	free(ef->symbase, M_LINKER);
    if (ef->strbase)
	free(ef->strbase, M_LINKER);
}

static void
link_elf_unload_preload(linker_file_t file)
{
    if (file->filename)
	preload_delete_name(file->filename);
}

static const char *
symbol_name(elf_file_t ef, Elf_Word r_info)
{
    const Elf_Sym *ref;

    if (ELF_R_SYM(r_info)) {
	ref = ef->symtab + ELF_R_SYM(r_info);
	return ef->strtab + ref->st_name;
    } else
	return NULL;
}

static int
relocate_file(elf_file_t ef)
{
    const Elf_Rel *rellim;
    const Elf_Rel *rel;
    const Elf_Rela *relalim;
    const Elf_Rela *rela;
    const char *symname;

    /* Perform relocations without addend if there are any: */
    rel = ef->rel;
    if (rel) {
	rellim = (const Elf_Rel *)((const char *)ef->rel + ef->relsize);
	while (rel < rellim) {
	    if (elf_reloc(&ef->lf, (Elf_Addr)ef->address, rel, ELF_RELOC_REL,
			  elf_lookup)) {
		symname = symbol_name(ef, rel->r_info);
		printf("link_elf: symbol %s undefined\n", symname);
		return ENOENT;
	    }
	    rel++;
	}
    }

    /* Perform relocations with addend if there are any: */
    rela = ef->rela;
    if (rela) {
	relalim = (const Elf_Rela *)((const char *)ef->rela + ef->relasize);
	while (rela < relalim) {
	    if (elf_reloc(&ef->lf, (Elf_Addr)ef->address, rela, ELF_RELOC_RELA,
			  elf_lookup)) {
		symname = symbol_name(ef, rela->r_info);
		printf("link_elf: symbol %s undefined\n", symname);
		return ENOENT;
	    }
	    rela++;
	}
    }

    /* Perform PLT relocations without addend if there are any: */
    rel = ef->pltrel;
    if (rel) {
	rellim = (const Elf_Rel *)((const char *)ef->pltrel + ef->pltrelsize);
	while (rel < rellim) {
	    if (elf_reloc(&ef->lf, (Elf_Addr)ef->address, rel, ELF_RELOC_REL,
			  elf_lookup)) {
		symname = symbol_name(ef, rel->r_info);
		printf("link_elf: symbol %s undefined\n", symname);
		return ENOENT;
	    }
	    rel++;
	}
    }

    /* Perform relocations with addend if there are any: */
    rela = ef->pltrela;
    if (rela) {
	relalim = (const Elf_Rela *)((const char *)ef->pltrela + ef->pltrelasize);
	while (rela < relalim) {
	    if (elf_reloc(&ef->lf, (Elf_Addr)ef->address, rela, ELF_RELOC_RELA,
			  elf_lookup)) {
		symname = symbol_name(ef, rela->r_info);
		printf("link_elf: symbol %s undefined\n", symname);
		return ENOENT;
	    }
	    rela++;
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

static int
link_elf_lookup_symbol(linker_file_t lf, const char* name, c_linker_sym_t* sym)
{
    elf_file_t ef = (elf_file_t) lf;
    unsigned long symnum;
    const Elf_Sym* symp;
    const char *strp;
    unsigned long hash;
    int i;

    /* First, search hashed global symbols */
    hash = elf_hash(name);
    symnum = ef->buckets[hash % ef->nbuckets];

    while (symnum != STN_UNDEF) {
	if (symnum >= ef->nchains) {
	    printf("link_elf_lookup_symbol: corrupt symbol table\n");
	    return ENOENT;
	}

	symp = ef->symtab + symnum;
	if (symp->st_name == 0) {
	    printf("link_elf_lookup_symbol: corrupt symbol table\n");
	    return ENOENT;
	}

	strp = ef->strtab + symp->st_name;

	if (strcmp(name, strp) == 0) {
	    if (symp->st_shndx != SHN_UNDEF ||
		(symp->st_value != 0 &&
		 ELF_ST_TYPE(symp->st_info) == STT_FUNC)) {
		*sym = (c_linker_sym_t) symp;
		return 0;
	    } else
		return ENOENT;
	}

	symnum = ef->chains[symnum];
    }

    /* If we have not found it, look at the full table (if loaded) */
    if (ef->symtab == ef->ddbsymtab)
	return ENOENT;

    /* Exhaustive search */
    for (i = 0, symp = ef->ddbsymtab; i < ef->ddbsymcnt; i++, symp++) {
	strp = ef->ddbstrtab + symp->st_name;
	if (strcmp(name, strp) == 0) {
	    if (symp->st_shndx != SHN_UNDEF ||
		(symp->st_value != 0 &&
		 ELF_ST_TYPE(symp->st_info) == STT_FUNC)) {
		*sym = (c_linker_sym_t) symp;
		return 0;
	    } else
		return ENOENT;
	}
    }

    return ENOENT;
}

static int
link_elf_symbol_values(linker_file_t lf, c_linker_sym_t sym, linker_symval_t* symval)
{
	elf_file_t ef = (elf_file_t) lf;
	const Elf_Sym* es = (const Elf_Sym*) sym;

	if (es >= ef->symtab && es < (ef->symtab + ef->nchains)) {
	    symval->name = ef->strtab + es->st_name;
	    symval->value = (caddr_t) ef->address + es->st_value;
	    symval->size = es->st_size;
	    return 0;
	}
	if (ef->symtab == ef->ddbsymtab)
	    return ENOENT;
	if (es >= ef->ddbsymtab && es < (ef->ddbsymtab + ef->ddbsymcnt)) {
	    symval->name = ef->ddbstrtab + es->st_name;
	    symval->value = (caddr_t) ef->address + es->st_value;
	    symval->size = es->st_size;
	    return 0;
	}
	return ENOENT;
}

static int
link_elf_search_symbol(linker_file_t lf, caddr_t value,
		       c_linker_sym_t* sym, long* diffp)
{
	elf_file_t ef = (elf_file_t) lf;
	u_long off = (uintptr_t) (void *) value;
	u_long diff = off;
	u_long st_value;
	const Elf_Sym* es;
	const Elf_Sym* best = 0;
	int i;

	for (i = 0, es = ef->ddbsymtab; i < ef->ddbsymcnt; i++, es++) {
		if (es->st_name == 0)
			continue;
		st_value = es->st_value + (uintptr_t) (void *) ef->address;
		if (off >= st_value) {
			if (off - st_value < diff) {
				diff = off - st_value;
				best = es;
				if (diff == 0)
					break;
			} else if (off - st_value == diff) {
				best = es;
			}
		}
	}
	if (best == 0)
		*diffp = off;
	else
		*diffp = diff;
	*sym = (c_linker_sym_t) best;

	return 0;
}

/*
 * Look up a linker set on an ELF system.
 */
static int
link_elf_lookup_set(linker_file_t lf, const char *name,
		    void ***startp, void ***stopp, int *countp)
{
	c_linker_sym_t sym;
	linker_symval_t symval;
	char *setsym;
	void **start, **stop;
	int len, error = 0, count;

	len = strlen(name) + sizeof("__start_set_"); /* sizeof includes \0 */
	setsym = malloc(len, M_LINKER, M_WAITOK);
	if (setsym == NULL)
		return ENOMEM;

	/* get address of first entry */
	snprintf(setsym, len, "%s%s", "__start_set_", name);
	error = link_elf_lookup_symbol(lf, setsym, &sym);
	if (error)
		goto out;
	link_elf_symbol_values(lf, sym, &symval);
	if (symval.value == 0) {
		error = ESRCH;
		goto out;
	}
	start = (void **)symval.value;

	/* get address of last entry */
	snprintf(setsym, len, "%s%s", "__stop_set_", name);
	error = link_elf_lookup_symbol(lf, setsym, &sym);
	if (error)
		goto out;
	link_elf_symbol_values(lf, sym, &symval);
	if (symval.value == 0) {
		error = ESRCH;
		goto out;
	}
	stop = (void **)symval.value;

	/* and the number of entries */
	count = stop - start;

	/* and copy out */
	if (startp)
		*startp = start;
	if (stopp)
		*stopp = stop;
	if (countp)
		*countp = count;

out:
	free(setsym, M_LINKER);
	return error;
}

static int
link_elf_each_function_name(linker_file_t file,
  int (*callback)(const char *, void *), void *opaque) {
    elf_file_t ef = (elf_file_t)file;
    const Elf_Sym* symp;
    int i, error;
	
    /* Exhaustive search */
    for (i = 0, symp = ef->ddbsymtab; i < ef->ddbsymcnt; i++, symp++) {
	if (symp->st_value != 0 &&
	    ELF_ST_TYPE(symp->st_info) == STT_FUNC) {
		error = callback(ef->ddbstrtab + symp->st_name, opaque);
		if (error)
		    return (error);
	}
    }
    return (0);
}

#ifdef __ia64__
/*
 * Each KLD has its own GP. The GP value for each load module is given by
 * DT_PLTGOT on ia64. We need GP to construct function descriptors, but
 * don't have direct access to the ELF file structure. The link_elf_get_gp()
 * function returns the GP given a pointer to a generic linker file struct.
 */
Elf_Addr
link_elf_get_gp(linker_file_t lf)
{
	elf_file_t ef = (elf_file_t)lf;
	return (Elf_Addr)ef->got;
}
#endif

const Elf_Sym *
elf_get_sym(linker_file_t lf, Elf_Word symidx)
{
	elf_file_t ef = (elf_file_t)lf;

	if (symidx >= ef->nchains)
		return (NULL);
	return (ef->symtab + symidx);
}

const char *
elf_get_symname(linker_file_t lf, Elf_Word symidx)
{
	elf_file_t ef = (elf_file_t)lf;
	const Elf_Sym *sym;

	if (symidx >= ef->nchains)
		return (NULL);
	sym = ef->symtab + symidx;
	return (ef->strtab + sym->st_name);
}

/*
 * Symbol lookup function that can be used when the symbol index is known (ie
 * in relocations). It uses the symbol index instead of doing a fully fledged
 * hash table based lookup when such is valid. For example for local symbols.
 * This is not only more efficient, it's also more correct. It's not always
 * the case that the symbol can be found through the hash table.
 */
static Elf_Addr
elf_lookup(linker_file_t lf, Elf_Word symidx, int deps)
{
	elf_file_t ef = (elf_file_t)lf;
	const Elf_Sym *sym;
	const char *symbol;

	/* Don't even try to lookup the symbol if the index is bogus. */
	if (symidx >= ef->nchains)
		return (0);

	sym = ef->symtab + symidx;

	/*
	 * Don't do a full lookup when the symbol is local. It may even
	 * fail because it may not be found through the hash table.
	 */
	if (ELF_ST_BIND(sym->st_info) == STB_LOCAL) {
		/* Force lookup failure when we have an insanity. */
		if (sym->st_shndx == SHN_UNDEF || sym->st_value == 0)
			return (0);
		return ((Elf_Addr)ef->address + sym->st_value);
	}

	/*
	 * XXX we can avoid doing a hash table based lookup for global
	 * symbols as well. This however is not always valid, so we'll
	 * just do it the hard way for now. Performance tweaks can
	 * always be added.
	 */

	symbol = ef->strtab + sym->st_name;

	/* Force a lookup failure if the symbol name is bogus. */
	if (*symbol == 0)
		return (0);

	return ((Elf_Addr)linker_file_lookup_symbol(lf, symbol, deps));
}

static void
link_elf_reloc_local(linker_file_t lf)
{
    const Elf_Rel *rellim;
    const Elf_Rel *rel;
    const Elf_Rela *relalim;
    const Elf_Rela *rela;
    elf_file_t ef = (elf_file_t)lf;

    /* Perform relocations without addend if there are any: */
    if ((rel = ef->rel) != NULL) {
	rellim = (const Elf_Rel *)((const char *)ef->rel + ef->relsize);
	while (rel < rellim) {
	    elf_reloc_local(lf, (Elf_Addr)ef->address, rel, ELF_RELOC_REL,
			    elf_lookup);
	    rel++;
	}
    }

    /* Perform relocations with addend if there are any: */
    if ((rela = ef->rela) != NULL) {
	relalim = (const Elf_Rela *)((const char *)ef->rela + ef->relasize);
	while (rela < relalim) {
	    elf_reloc_local(lf, (Elf_Addr)ef->address, rela, ELF_RELOC_RELA,
			    elf_lookup);
	    rela++;
	}
    }
}
