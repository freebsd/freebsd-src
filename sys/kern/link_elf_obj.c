/*-
 * Copyright (c) 1998 Doug Rabson
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
 *	$Id: link_elf.c,v 1.1 1998/08/24 08:25:26 dfr Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/linker.h>
#include <machine/elf.h>

#include <vm/vm.h>
#include <vm/vm_prot.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

extern int	elf_reloc(linker_file_t lf, const Elf_Rela *rela,
			  const char *sym);

static int	link_elf_load_file(const char*, linker_file_t*);
static int	link_elf_lookup_symbol(linker_file_t, const char*,
				       linker_sym_t*);
static void	link_elf_symbol_values(linker_file_t, linker_sym_t, linker_symval_t*);
static int	link_elf_search_symbol(linker_file_t, caddr_t value,
				       linker_sym_t* sym, long* diffp);

static void	link_elf_unload(linker_file_t);

/*
 * The file representing the currently running kernel.  This contains
 * the global symbol table.
 */

linker_file_t linker_kernel_file;

static struct linker_class_ops link_elf_class_ops = {
    link_elf_load_file,
};

static struct linker_file_ops link_elf_file_ops = {
    link_elf_lookup_symbol,
    link_elf_symbol_values,
    link_elf_search_symbol,
    link_elf_unload,
};

typedef struct elf_file {
    caddr_t		address;	/* Relocation address */
#ifdef SPARSE_MAPPING
    vm_object_t		object;		/* VM object to hold file pages */
#endif
    const Elf_Dyn*	dynamic;	/* Symbol table etc. */
    Elf_Off		nbuckets;	/* DT_HASH info */
    Elf_Off		nchains;
    const Elf_Off*	buckets;
    const Elf_Off*	chains;
    caddr_t		hash;
    caddr_t		strtab;		/* DT_STRTAB */
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
} *elf_file_t;

static int		parse_dynamic(linker_file_t lf);
static int		load_dependancies(linker_file_t lf);
static int		relocate_file(linker_file_t lf);

/*
 * The kernel symbol table starts here.
 */
extern struct _dynamic _DYNAMIC;

static void
link_elf_init(void* arg)
{
    Elf_Dyn* dp = (Elf_Dyn*) &_DYNAMIC;

#if ELF_TARG_CLASS == ELFCLASS32
    linker_add_class("elf32", NULL, &link_elf_class_ops);
#else
    linker_add_class("elf64", NULL, &link_elf_class_ops);
#endif

    if (dp) {
	elf_file_t ef;

	ef = malloc(sizeof(struct elf_file), M_LINKER, M_NOWAIT);
	if (ef == NULL)
	    panic("link_elf_init: Can't create linker structures for kernel");

	ef->address = 0;
#ifdef SPARSE_MAPPING
	ef->object = 0;
#endif
	ef->dynamic = dp;
	linker_kernel_file =
	    linker_make_file(kernelname, ef, &link_elf_file_ops);
	if (linker_kernel_file == NULL)
	    panic("link_elf_init: Can't create linker structures for kernel");
	parse_dynamic(linker_kernel_file);
	/*
	 * XXX there must be a better way of getting these constants.
	 */
#ifdef __alpha__
	linker_kernel_file->address = (caddr_t) 0xfffffc0000300000;
#else
	linker_kernel_file->address = (caddr_t) 0xf0100000;
#endif
	linker_kernel_file->size = -(long)linker_kernel_file->address;
	linker_current_file = linker_kernel_file;
    }
}

SYSINIT(link_elf, SI_SUB_KMEM, SI_ORDER_THIRD, link_elf_init, 0);

static int
parse_dynamic(linker_file_t lf)
{
    elf_file_t ef = lf->priv;
    const Elf_Dyn *dp;
    int plttype = DT_REL;

    for (dp = ef->dynamic; dp->d_tag != DT_NULL; dp++) {
	switch (dp->d_tag) {
	case DT_HASH:
	{
	    /* From src/libexec/rtld-elf/rtld.c */
	    const Elf_Off *hashtab = (const Elf_Off *)
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
	}
    }

    if (plttype == DT_RELA) {
	ef->pltrela = (const Elf_Rela *) ef->pltrel;
	ef->pltrel = NULL;
	ef->pltrelasize = ef->pltrelsize;
	ef->pltrelsize = 0;
    }

    return 0;
}

static void
link_elf_error(const char *s)
{
    printf("kldload: %s\n", s);
}

static int
link_elf_load_file(const char* filename, linker_file_t* result)
{
    struct nameidata nd;
    struct proc* p = curproc;	/* XXX */
    union {
	Elf_Ehdr hdr;
	char buf[PAGE_SIZE];
    } u;
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
    caddr_t base_addr;
    Elf_Off data_offset;
    Elf_Addr data_vaddr;
    Elf_Addr data_vlimit;
    caddr_t data_addr;
    Elf_Addr clear_vaddr;
    caddr_t clear_addr;
    size_t nclear;
    Elf_Addr bss_vaddr;
    Elf_Addr bss_vlimit;
    caddr_t bss_addr;
    int error = 0;
    int resid;
    elf_file_t ef;
    linker_file_t lf;

    NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, filename, p);
    error = vn_open(&nd, FREAD, 0);
    if (error)
	return error;

    /*
     * Read the elf header from the file.
     */
    error = vn_rdwr(UIO_READ, nd.ni_vp, (void*) &u, sizeof u, 0,
		    UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid, p);
    nbytes = sizeof u - resid;
    if (error)
	goto out;

    if (!IS_ELF(u.hdr)) {
	error = ENOEXEC;
	goto out;
    }

    if (u.hdr.e_ident[EI_CLASS] != ELF_TARG_CLASS
      || u.hdr.e_ident[EI_DATA] != ELF_TARG_DATA) {
	link_elf_error("Unsupported file layout");
	error = ENOEXEC;
	goto out;
    }
    if (u.hdr.e_ident[EI_VERSION] != EV_CURRENT
      || u.hdr.e_version != EV_CURRENT) {
	link_elf_error("Unsupported file version");
	error = ENOEXEC;
	goto out;
    }
    if (u.hdr.e_type != ET_EXEC && u.hdr.e_type != ET_DYN) {
	link_elf_error("Unsupported file type");
	error = ENOEXEC;
	goto out;
    }
    if (u.hdr.e_machine != ELF_TARG_MACH) {
	link_elf_error("Unsupported machine");
	error = ENOEXEC;
	goto out;
    }
    if (!ELF_MACHINE_OK(u.hdr.e_machine)) {
	link_elf_error("Incompatibile elf machine type");
	error = ENOEXEC;
	goto out;
    }

    /*
     * We rely on the program header being in the first page.  This is
     * not strictly required by the ABI specification, but it seems to
     * always true in practice.  And, it simplifies things considerably.
     */
    if (!((u.hdr.e_phentsize == sizeof(Elf_Phdr))
	  || (u.hdr.e_phoff + u.hdr.e_phnum*sizeof(Elf_Phdr) <= PAGE_SIZE)
	  || (u.hdr.e_phoff + u.hdr.e_phnum*sizeof(Elf_Phdr) <= nbytes)))
	link_elf_error("Unreadable program headers");

    /*
     * Scan the program header entries, and save key information.
     *
     * We rely on there being exactly two load segments, text and data,
     * in that order.
     */
    phdr = (Elf_Phdr *) (u.buf + u.hdr.e_phoff);
    phlimit = phdr + u.hdr.e_phnum;
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
	    segs[nsegs] = phdr;
	    ++nsegs;
	    break;

	case PT_PHDR:
	    phphdr = phdr;
	    break;

	case PT_DYNAMIC:
	    phdyn = phdr;
	    break;
	}

	++phdr;
    }
    if (phdyn == NULL) {
	link_elf_error("Object is not dynamically-linked");
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

    ef = malloc(sizeof(struct elf_file), M_LINKER, M_WAITOK);
#ifdef SPARSE_MAPPING
    ef->object = vm_object_allocate(OBJT_DEFAULT, mapsize >> PAGE_SHIFT);
    if (ef->object == NULL) {
	free(ef, M_LINKER);
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
	free(ef, M_LINKER);
	goto out;
    }
#else
    ef->address = malloc(mapsize, M_LINKER, M_WAITOK);
#endif
    mapbase = ef->address;

    /*
     * Read the text and data sections and zero the bss.
     */
    for (i = 0; i < 2; i++) {
	caddr_t segbase = mapbase + segs[i]->p_vaddr - base_vaddr;
	error = vn_rdwr(UIO_READ, nd.ni_vp,
			segbase, segs[i]->p_filesz, segs[i]->p_offset,
			UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid, p);
	if (error) {
#ifdef SPARSE_MAPPING
	    vm_map_remove(kernel_map, (vm_offset_t) ef->address,
			  (vm_offset_t) ef->address
			  + (ef->object->size << PAGE_SHIFT));
	    vm_object_deallocate(ef->object);
#else
	    free(ef->address, M_LINKER);
#endif
	    free(ef, M_LINKER);
	    goto out;
	}
	bzero(segbase + segs[i]->p_filesz,
	      segs[i]->p_memsz - segs[i]->p_filesz);

#ifdef SPARSE_MAPPING
	/*
	 * Wire down the pages
	 */
	vm_map_pageable(kernel_map,
			(vm_offset_t) segbase,
			(vm_offset_t) segbase + segs[i]->p_memsz,
			FALSE);
#endif
    }

    ef->dynamic = (const Elf_Dyn *) (mapbase + phdyn->p_vaddr - base_vaddr);

    lf = linker_make_file(filename, ef, &link_elf_file_ops);
    if (lf == NULL) {
#ifdef SPARSE_MAPPING
	vm_map_remove(kernel_map, (vm_offset_t) ef->address,
		      (vm_offset_t) ef->address
		      + (ef->object->size << PAGE_SHIFT));
	vm_object_deallocate(ef->object);
#else
	free(ef->address, M_LINKER);
#endif
	free(ef, M_LINKER);
	error = ENOMEM;
	goto out;
    }
    lf->address = ef->address;
    lf->size = mapsize;

    if ((error = parse_dynamic(lf)) != 0
	|| (error = load_dependancies(lf)) != 0
	|| (error = relocate_file(lf)) != 0) {
	linker_file_unload(lf);
	goto out;
    }

    *result = lf;

out:
    VOP_UNLOCK(nd.ni_vp, 0, p);
    vn_close(nd.ni_vp, FREAD, p->p_ucred, p);

    return error;
}

static void
link_elf_unload(linker_file_t file)
{
    elf_file_t ef = file->priv;

    if (ef) {
#ifdef SPARSE_MAPPING
	if (ef->object) {
	    vm_map_remove(kernel_map, (vm_offset_t) ef->address,
			  (vm_offset_t) ef->address
			  + (ef->object->size << PAGE_SHIFT));
	    vm_object_deallocate(ef->object);
	}
#else
	free(ef->address, M_LINKER);
#endif
	free(ef, M_LINKER);
    }
}

static int
load_dependancies(linker_file_t lf)
{
    elf_file_t ef = lf->priv;
    linker_file_t lfdep;
    char* name;
    char* filename = 0;
    const Elf_Dyn *dp;
    int error = 0;

    /*
     * All files are dependant on /kernel.
     */
    linker_kernel_file->refs++;
    linker_file_add_dependancy(lf, linker_kernel_file);

    
    for (dp = ef->dynamic; dp->d_tag != DT_NULL; dp++) {
	if (dp->d_tag == DT_NEEDED) {
	    name = ef->strtab + dp->d_un.d_val;

	    /*
	     * Prepend pathname if dep is not an absolute filename.
	     */
	    if (name[0] != '/') {
		char* p;
		if (!filename) {
		    filename = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
		    if (!filename) {
			error = ENOMEM;
			goto out;
		    }
		}
		p = lf->filename + strlen(lf->filename) - 1;
		while (p >= lf->filename && *p != '/')
		    p--;
		if (p >= lf->filename) {
		    strncpy(filename, lf->filename, p - lf->filename);
		    filename[p - lf->filename] = '\0';
		    strcat(filename, "/");
		    strcat(filename, name);
		    name = filename;
		}
	    }
	    error = linker_load_file(name, &lfdep);
	    if (error)
		goto out;
	    error = linker_file_add_dependancy(lf, lfdep);
	    if (error)
		goto out;
	}
    }

out:
    if (filename)
	free(filename, M_TEMP);
    return error;
}

static const char *
symbol_name(elf_file_t ef, const Elf_Rela *rela)
{
    const Elf_Sym *ref;

    if (ELF_R_SYM(rela->r_info)) {
	ref = ef->symtab + ELF_R_SYM(rela->r_info);
	return ef->strtab + ref->st_name;
    } else
	return NULL;
}

static int
relocate_file(linker_file_t lf)
{
    elf_file_t ef = lf->priv;
    const Elf_Rel *rellim;
    const Elf_Rel *rel;
    const Elf_Rela *relalim;
    const Elf_Rela *rela;

    /* Perform relocations without addend if there are any: */
    rellim = (const Elf_Rel *) ((caddr_t) ef->rel + ef->relsize);
    for (rel = ef->rel;  ef->rel != NULL && rel < rellim;  rel++) {
	Elf_Rela locrela;

	locrela.r_info = rel->r_info;
	locrela.r_offset = rel->r_offset;
	locrela.r_addend = 0;
	if (elf_reloc(lf, &locrela, symbol_name(ef, &locrela)))
	    return ENOENT;
    }

    /* Perform relocations with addend if there are any: */
    relalim = (const Elf_Rela *) ((caddr_t) ef->rela + ef->relasize);
    for (rela = ef->rela;  ef->rela != NULL && rela < relalim;  rela++) {
	if (elf_reloc(lf, rela, symbol_name(ef, rela)))
	    return ENOENT;
    }

    /* Perform PLT relocations without addend if there are any: */
    rellim = (const Elf_Rel *) ((caddr_t) ef->pltrel + ef->pltrelsize);
    for (rel = ef->pltrel;  ef->pltrel != NULL && rel < rellim;  rel++) {
	Elf_Rela locrela;

	locrela.r_info = rel->r_info;
	locrela.r_offset = rel->r_offset;
	locrela.r_addend = 0;
	if (elf_reloc(lf, &locrela, symbol_name(ef, &locrela)))
	    return ENOENT;
    }

    /* Perform relocations with addend if there are any: */
    relalim = (const Elf_Rela *) ((caddr_t) ef->pltrela + ef->pltrelasize);
    for (rela = ef->pltrela;  ef->pltrela != NULL && rela < relalim;  rela++) {
	if (elf_reloc(lf, rela, symbol_name(ef, rela)))
	    return ENOENT;
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

int
link_elf_lookup_symbol(linker_file_t lf, const char* name, linker_sym_t* sym)
{
    elf_file_t ef = lf->priv;
    unsigned long symnum;
    const Elf_Sym* es;
    unsigned long hash;
    int i;

    hash = elf_hash(name);
    symnum = ef->buckets[hash % ef->nbuckets];

    while (symnum != STN_UNDEF) {
	const Elf_Sym *symp;
	const char *strp;

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
		*sym = (linker_sym_t) symp;
		return 0;
	    } else
		return ENOENT;
	}

	symnum = ef->chains[symnum];
    }

    return ENOENT;
}

static void
link_elf_symbol_values(linker_file_t lf, linker_sym_t sym, linker_symval_t* symval)
{
	elf_file_t ef = lf->priv;
	Elf_Sym* es = (Elf_Sym*) sym;

	symval->name = ef->strtab + es->st_name;
	symval->value = (caddr_t) ef->address + es->st_value;
	symval->size = es->st_size;
}

static int
link_elf_search_symbol(linker_file_t lf, caddr_t value,
		       linker_sym_t* sym, long* diffp)
{
	elf_file_t ef = lf->priv;
	u_long off = (u_long) value;
	u_long diff = off;
	int symcount = ef->nchains;
	const Elf_Sym* es;
	const Elf_Sym* best = 0;
	int i;

	for (i = 0, es = ef->symtab; i < ef->nchains; i++, es++) {
		if (es->st_name == 0)
			continue;
		if (off >= es->st_value) {
			if (off - es->st_value < diff) {
				diff = off - es->st_value;
				best = es;
				if (diff == 0)
					break;
			} else if (off - es->st_value == diff) {
				best = es;
			}
		}
	}
	if (best == 0)
		*diffp = off;
	else
		*diffp = diff;
	*sym = (linker_sym_t) best;

	return 0;
}

