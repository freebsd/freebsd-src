/*-
 * Copyright (c) 1998-2000 Doug Rabson
 * Copyright (c) 2004 Peter Wemm
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

#include "opt_ddb.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
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
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <sys/link_elf.h>

#include "linker_if.h"

typedef struct {
	void		*addr;
	Elf_Off		size;
	int		flags;
	int		sec;	/* Original section */
	char		*name;
} Elf_progent;

typedef struct {
	Elf_Rel		*rel;
	int		nrel;
	int		sec;
} Elf_relent;

typedef struct {
	Elf_Rela	*rela;
	int		nrela;
	int		sec;
} Elf_relaent;


typedef struct elf_file {
	struct linker_file lf;		/* Common fields */

	int		preloaded;
	caddr_t		address;	/* Relocation address */
	vm_object_t	object;		/* VM object to hold file pages */
	Elf_Shdr	*e_shdr;

	Elf_progent	*progtab;
	int		nprogtab;

	Elf_relaent	*relatab;
	int		nrela;

	Elf_relent	*reltab;
	int		nrel;

	Elf_Sym		*ddbsymtab;	/* The symbol table we are using */
	long		ddbsymcnt;	/* Number of symbols */
	caddr_t		ddbstrtab;	/* String table */
	long		ddbstrcnt;	/* number of bytes in string table */

	caddr_t		shstrtab;	/* Section name string table */
	long		shstrcnt;	/* number of bytes in string table */

} *elf_file_t;

static int	link_elf_link_preload(linker_class_t cls,
		    const char *, linker_file_t *);
static int	link_elf_link_preload_finish(linker_file_t);
static int	link_elf_load_file(linker_class_t, const char *, linker_file_t *);
static int	link_elf_lookup_symbol(linker_file_t, const char *,
		    c_linker_sym_t *);
static int	link_elf_symbol_values(linker_file_t, c_linker_sym_t,
		    linker_symval_t *);
static int	link_elf_search_symbol(linker_file_t, caddr_t value,
		    c_linker_sym_t *sym, long *diffp);

static void	link_elf_unload_file(linker_file_t);
static int	link_elf_lookup_set(linker_file_t, const char *,
		    void ***, void ***, int *);
static int	link_elf_each_function_name(linker_file_t,
		    int (*)(const char *, void *), void *);
static void	link_elf_reloc_local(linker_file_t);

static Elf_Addr elf_obj_lookup(linker_file_t lf, Elf_Word symidx, int deps);

static kobj_method_t link_elf_methods[] = {
	KOBJMETHOD(linker_lookup_symbol,	link_elf_lookup_symbol),
	KOBJMETHOD(linker_symbol_values,	link_elf_symbol_values),
	KOBJMETHOD(linker_search_symbol,	link_elf_search_symbol),
	KOBJMETHOD(linker_unload,		link_elf_unload_file),
	KOBJMETHOD(linker_load_file,		link_elf_load_file),
	KOBJMETHOD(linker_link_preload,		link_elf_link_preload),
	KOBJMETHOD(linker_link_preload_finish,	link_elf_link_preload_finish),
	KOBJMETHOD(linker_lookup_set,		link_elf_lookup_set),
	KOBJMETHOD(linker_each_function_name,	link_elf_each_function_name),
	{ 0, 0 }
};

static struct linker_class link_elf_class = {
#if ELF_TARG_CLASS == ELFCLASS32
	"elf32_obj",
#else
	"elf64_obj",
#endif
	link_elf_methods, sizeof(struct elf_file)
};

static int	relocate_file(elf_file_t ef);

static void
link_elf_error(const char *s)
{
	printf("kldload: %s\n", s);
}

static void
link_elf_init(void *arg)
{

	linker_add_class(&link_elf_class);
}

SYSINIT(link_elf_obj, SI_SUB_KLD, SI_ORDER_SECOND, link_elf_init, 0);

static int
link_elf_link_preload(linker_class_t cls, const char *filename,
    linker_file_t *result)
{
	Elf_Ehdr *hdr;
	Elf_Shdr *shdr;
	Elf_Sym *es;
	void *modptr, *baseptr, *sizeptr;
	char *type;
	elf_file_t ef;
	linker_file_t lf;
	Elf_Addr off;
	int error, i, j, pb, ra, rl, shstrindex, symstrindex, symtabindex;

	/* Look to see if we have the file preloaded */
	modptr = preload_search_by_name(filename);
	if (modptr == NULL)
		return ENOENT;

	type = (char *)preload_search_info(modptr, MODINFO_TYPE);
	baseptr = preload_search_info(modptr, MODINFO_ADDR);
	sizeptr = preload_search_info(modptr, MODINFO_SIZE);
	hdr = (Elf_Ehdr *)preload_search_info(modptr, MODINFO_METADATA |
	    MODINFOMD_ELFHDR);
	shdr = (Elf_Shdr *)preload_search_info(modptr, MODINFO_METADATA |
	    MODINFOMD_SHDR);
	if (type == NULL || (strcmp(type, "elf" __XSTRING(__ELF_WORD_SIZE)
	    " obj module") != 0 &&
	    strcmp(type, "elf obj module") != 0)) {
		return (EFTYPE);
	}
	if (baseptr == NULL || sizeptr == NULL || hdr == NULL ||
	    shdr == NULL)
		return (EINVAL);

	lf = linker_make_file(filename, &link_elf_class);
	if (lf == NULL)
		return (ENOMEM);

	ef = (elf_file_t)lf;
	ef->preloaded = 1;
	ef->address = *(caddr_t *)baseptr;
	lf->address = *(caddr_t *)baseptr;
	lf->size = *(size_t *)sizeptr;

	if (hdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||
	    hdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
	    hdr->e_ident[EI_VERSION] != EV_CURRENT ||
	    hdr->e_version != EV_CURRENT ||
	    hdr->e_type != ET_REL ||
	    hdr->e_machine != ELF_TARG_MACH) {
		error = EFTYPE;
		goto out;
	}
	ef->e_shdr = shdr;

	/* Scan the section header for information and table sizing. */
	symtabindex = -1;
	symstrindex = -1;
	for (i = 0; i < hdr->e_shnum; i++) {
		switch (shdr[i].sh_type) {
		case SHT_PROGBITS:
		case SHT_NOBITS:
			ef->nprogtab++;
			break;
		case SHT_SYMTAB:
			symtabindex = i;
			symstrindex = shdr[i].sh_link;
			break;
		case SHT_REL:
			ef->nrel++;
			break;
		case SHT_RELA:
			ef->nrela++;
			break;
		}
	}

	shstrindex = hdr->e_shstrndx;
	if (ef->nprogtab == 0 || symstrindex < 0 ||
	    symstrindex >= hdr->e_shnum ||
	    shdr[symstrindex].sh_type != SHT_STRTAB || shstrindex == 0 ||
	    shstrindex >= hdr->e_shnum ||
	    shdr[shstrindex].sh_type != SHT_STRTAB) {
		printf("%s: bad/missing section headers\n", filename);
		error = ENOEXEC;
		goto out;
	}

	/* Allocate space for tracking the load chunks */
	if (ef->nprogtab != 0)
		ef->progtab = malloc(ef->nprogtab * sizeof(*ef->progtab),
		    M_LINKER, M_WAITOK | M_ZERO);
	if (ef->nrel != 0)
		ef->reltab = malloc(ef->nrel * sizeof(*ef->reltab), M_LINKER,
		    M_WAITOK | M_ZERO);
	if (ef->nrela != 0)
		ef->relatab = malloc(ef->nrela * sizeof(*ef->relatab), M_LINKER,
		    M_WAITOK | M_ZERO);
	if ((ef->nprogtab != 0 && ef->progtab == NULL) ||
	    (ef->nrel != 0 && ef->reltab == NULL) ||
	    (ef->nrela != 0 && ef->relatab == NULL)) {
		error = ENOMEM;
		goto out;
	}

	/* XXX, relocate the sh_addr fields saved by the loader. */
	off = 0;
	for (i = 0; i < hdr->e_shnum; i++) {
		if (shdr[i].sh_addr != 0 && (off == 0 || shdr[i].sh_addr < off))
			off = shdr[i].sh_addr;
	}
	for (i = 0; i < hdr->e_shnum; i++) {
		if (shdr[i].sh_addr != 0)
			shdr[i].sh_addr = shdr[i].sh_addr - off +
			    (Elf_Addr)ef->address;
	}

	ef->ddbsymcnt = shdr[symtabindex].sh_size / sizeof(Elf_Sym);
	ef->ddbsymtab = (Elf_Sym *)shdr[symtabindex].sh_addr;
	ef->ddbstrcnt = shdr[symstrindex].sh_size;
	ef->ddbstrtab = (char *)shdr[symstrindex].sh_addr;
	ef->shstrcnt = shdr[shstrindex].sh_size;
	ef->shstrtab = (char *)shdr[shstrindex].sh_addr;

	/* Now fill out progtab and the relocation tables. */
	pb = 0;
	rl = 0;
	ra = 0;
	for (i = 0; i < hdr->e_shnum; i++) {
		switch (shdr[i].sh_type) {
		case SHT_PROGBITS:
		case SHT_NOBITS:
			ef->progtab[pb].addr = (void *)shdr[i].sh_addr;
			if (shdr[i].sh_type == SHT_PROGBITS)
				ef->progtab[pb].name = "<<PROGBITS>>";
			else
				ef->progtab[pb].name = "<<NOBITS>>";
			ef->progtab[pb].size = shdr[i].sh_size;
			ef->progtab[pb].sec = i;
			if (ef->shstrtab && shdr[i].sh_name != 0)
				ef->progtab[pb].name =
				    ef->shstrtab + shdr[i].sh_name;

			/* Update all symbol values with the offset. */
			for (j = 0; j < ef->ddbsymcnt; j++) {
				es = &ef->ddbsymtab[j];
				if (es->st_shndx != i)
					continue;
				es->st_value += (Elf_Addr)ef->progtab[pb].addr;
			}
			pb++;
			break;
		case SHT_REL:
			ef->reltab[rl].rel = (Elf_Rel *)shdr[i].sh_addr;
			ef->reltab[rl].nrel = shdr[i].sh_size / sizeof(Elf_Rel);
			ef->reltab[rl].sec = shdr[i].sh_info;
			rl++;
			break;
		case SHT_RELA:
			ef->relatab[ra].rela = (Elf_Rela *)shdr[i].sh_addr;
			ef->relatab[ra].nrela =
			    shdr[i].sh_size / sizeof(Elf_Rela);
			ef->relatab[ra].sec = shdr[i].sh_info;
			ra++;
			break;
		}
	}
	if (pb != ef->nprogtab)
		panic("lost progbits");
	if (rl != ef->nrel)
		panic("lost rel");
	if (ra != ef->nrela)
		panic("lost rela");

	/* Local intra-module relocations */
	link_elf_reloc_local(lf);

	*result = lf;
	return (0);

out:
	/* preload not done this way */
	linker_file_unload(lf, LINKER_UNLOAD_FORCE);
	return (error);
}

static int
link_elf_link_preload_finish(linker_file_t lf)
{
	elf_file_t ef;
	int error;

	ef = (elf_file_t)lf;
	error = relocate_file(ef);
	if (error)
		return error;

	/* Notify MD code that a module is being loaded. */
	error = elf_cpu_load_file(lf);
	if (error)
		return (error);

	return (0);
}

static int
link_elf_load_file(linker_class_t cls, const char *filename,
    linker_file_t *result)
{
	struct nameidata nd;
	struct thread *td = curthread;	/* XXX */
	Elf_Ehdr *hdr;
	Elf_Shdr *shdr;
	Elf_Sym *es;
	int nbytes, i, j;
	vm_offset_t mapbase;
	size_t mapsize;
	int error = 0;
	int resid, flags;
	elf_file_t ef;
	linker_file_t lf;
	int symtabindex;
	int symstrindex;
	int shstrindex;
	int nsym;
	int pb, rl, ra;
	int alignmask;

	GIANT_REQUIRED;

	shdr = NULL;
	lf = NULL;
	mapsize = 0;
	hdr = NULL;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, filename, td);
	flags = FREAD;
	error = vn_open(&nd, &flags, 0, -1);
	if (error)
		return error;
	NDFREE(&nd, NDF_ONLY_PNBUF);
#ifdef MAC
	error = mac_check_kld_load(td->td_ucred, nd.ni_vp);
	if (error) {
		goto out;
	}
#endif

	/* Read the elf header from the file. */
	hdr = malloc(sizeof(*hdr), M_LINKER, M_WAITOK);
	if (hdr == NULL) {
		error = ENOMEM;
		goto out;
	}
	error = vn_rdwr(UIO_READ, nd.ni_vp, (void *)hdr, sizeof(*hdr), 0,
	    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
	    &resid, td);
	if (error)
		goto out;
	if (resid != 0){
		error = ENOEXEC;
		goto out;
	}

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
	if (hdr->e_type != ET_REL) {
		link_elf_error("Unsupported file type");
		error = ENOEXEC;
		goto out;
	}
	if (hdr->e_machine != ELF_TARG_MACH) {
		link_elf_error("Unsupported machine");
		error = ENOEXEC;
		goto out;
	}

	lf = linker_make_file(filename, &link_elf_class);
	if (!lf) {
		error = ENOMEM;
		goto out;
	}
	ef = (elf_file_t) lf;
	ef->nprogtab = 0;
	ef->e_shdr = 0;
	ef->nrel = 0;
	ef->nrela = 0;

	/* Allocate and read in the section header */
	nbytes = hdr->e_shnum * hdr->e_shentsize;
	if (nbytes == 0 || hdr->e_shoff == 0 ||
	    hdr->e_shentsize != sizeof(Elf_Shdr)) {
		error = ENOEXEC;
		goto out;
	}
	shdr = malloc(nbytes, M_LINKER, M_WAITOK);
	if (shdr == NULL) {
		error = ENOMEM;
		goto out;
	}
	ef->e_shdr = shdr;
	error = vn_rdwr(UIO_READ, nd.ni_vp, (caddr_t)shdr, nbytes, hdr->e_shoff,
	    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED, &resid, td);
	if (error)
		goto out;
	if (resid) {
		error = ENOEXEC;
		goto out;
	}

	/* Scan the section header for information and table sizing. */
	nsym = 0;
	symtabindex = -1;
	symstrindex = -1;
	for (i = 0; i < hdr->e_shnum; i++) {
		switch (shdr[i].sh_type) {
		case SHT_PROGBITS:
		case SHT_NOBITS:
			ef->nprogtab++;
			break;
		case SHT_SYMTAB:
			nsym++;
			symtabindex = i;
			symstrindex = shdr[i].sh_link;
			break;
		case SHT_REL:
			ef->nrel++;
			break;
		case SHT_RELA:
			ef->nrela++;
			break;
		case SHT_STRTAB:
			break;
		}
	}
	if (ef->nprogtab == 0) {
		link_elf_error("file has no contents");
		error = ENOEXEC;
		goto out;
	}
	if (nsym != 1) {
		/* Only allow one symbol table for now */
		link_elf_error("file has no valid symbol table");
		error = ENOEXEC;
		goto out;
	}
	if (symstrindex < 0 || symstrindex > hdr->e_shnum ||
	    shdr[symstrindex].sh_type != SHT_STRTAB) {
		link_elf_error("file has invalid symbol strings");
		error = ENOEXEC;
		goto out;
	}

	/* Allocate space for tracking the load chunks */
	if (ef->nprogtab != 0)
		ef->progtab = malloc(ef->nprogtab * sizeof(*ef->progtab),
		    M_LINKER, M_WAITOK | M_ZERO);
	if (ef->nrel != 0)
		ef->reltab = malloc(ef->nrel * sizeof(*ef->reltab), M_LINKER,
		    M_WAITOK | M_ZERO);
	if (ef->nrela != 0)
		ef->relatab = malloc(ef->nrela * sizeof(*ef->relatab), M_LINKER,
		    M_WAITOK | M_ZERO);
	if ((ef->nprogtab != 0 && ef->progtab == NULL) ||
	    (ef->nrel != 0 && ef->reltab == NULL) ||
	    (ef->nrela != 0 && ef->relatab == NULL)) {
		error = ENOMEM;
		goto out;
	}

	if (symtabindex == -1)
		panic("lost symbol table index");
	/* Allocate space for and load the symbol table */
	ef->ddbsymcnt = shdr[symtabindex].sh_size / sizeof(Elf_Sym);
	ef->ddbsymtab = malloc(shdr[symtabindex].sh_size, M_LINKER, M_WAITOK);
	if (ef->ddbsymtab == NULL) {
		error = ENOMEM;
		goto out;
	}
	error = vn_rdwr(UIO_READ, nd.ni_vp, (void *)ef->ddbsymtab,
	    shdr[symtabindex].sh_size, shdr[symtabindex].sh_offset,
	    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
	    &resid, td);
	if (error)
		goto out;
	if (resid != 0){
		error = EINVAL;
		goto out;
	}

	if (symstrindex == -1)
		panic("lost symbol string index");
	/* Allocate space for and load the symbol strings */
	ef->ddbstrcnt = shdr[symstrindex].sh_size;
	ef->ddbstrtab = malloc(shdr[symstrindex].sh_size, M_LINKER, M_WAITOK);
	if (ef->ddbstrtab == NULL) {
		error = ENOMEM;
		goto out;
	}
	error = vn_rdwr(UIO_READ, nd.ni_vp, ef->ddbstrtab,
	    shdr[symstrindex].sh_size, shdr[symstrindex].sh_offset,
	    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
	    &resid, td);
	if (error)
		goto out;
	if (resid != 0){
		error = EINVAL;
		goto out;
	}

	/* Do we have a string table for the section names?  */
	shstrindex = -1;
	if (hdr->e_shstrndx != 0 &&
	    shdr[hdr->e_shstrndx].sh_type == SHT_STRTAB) {
		shstrindex = hdr->e_shstrndx;
		ef->shstrcnt = shdr[shstrindex].sh_size;
		ef->shstrtab = malloc(shdr[shstrindex].sh_size, M_LINKER,
		    M_WAITOK);
		if (ef->shstrtab == NULL) {
			error = ENOMEM;
			goto out;
		}
		error = vn_rdwr(UIO_READ, nd.ni_vp, ef->shstrtab,
		    shdr[shstrindex].sh_size, shdr[shstrindex].sh_offset,
		    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
		    &resid, td);
		if (error)
			goto out;
		if (resid != 0){
			error = EINVAL;
			goto out;
		}
	}

	/* Size up code/data(progbits) and bss(nobits). */
	alignmask = 0;
	for (i = 0; i < hdr->e_shnum; i++) {
		switch (shdr[i].sh_type) {
		case SHT_PROGBITS:
		case SHT_NOBITS:
			alignmask = shdr[i].sh_addralign - 1;
			mapsize += alignmask;
			mapsize &= ~alignmask;
			mapsize += shdr[i].sh_size;
			break;
		}
	}

	/*
	 * We know how much space we need for the text/data/bss/etc.
	 * This stuff needs to be in a single chunk so that profiling etc
	 * can get the bounds and gdb can associate offsets with modules
	 */
	ef->object = vm_object_allocate(OBJT_DEFAULT,
	    round_page(mapsize) >> PAGE_SHIFT);
	if (ef->object == NULL) {
		error = ENOMEM;
		goto out;
	}
	ef->address = (caddr_t) vm_map_min(kernel_map);
	error = vm_map_find(kernel_map, ef->object, 0, &mapbase,
	    round_page(mapsize), TRUE, VM_PROT_ALL, VM_PROT_ALL, FALSE);
	if (error) {
		vm_object_deallocate(ef->object);
		ef->object = 0;
		goto out;
	}

	/* Wire the pages */
	error = vm_map_wire(kernel_map, mapbase,
	    mapbase + round_page(mapsize),
	    VM_MAP_WIRE_SYSTEM|VM_MAP_WIRE_NOHOLES);
	if (error != KERN_SUCCESS) {
		error = ENOMEM;
		goto out;
	}

	/* Inform the kld system about the situation */
	lf->address = ef->address = (caddr_t)mapbase;
	lf->size = mapsize;

	/*
	 * Now load code/data(progbits), zero bss(nobits), allocate space for
	 * and load relocs
	 */
	pb = 0;
	rl = 0;
	ra = 0;
	alignmask = 0;
	for (i = 0; i < hdr->e_shnum; i++) {
		switch (shdr[i].sh_type) {
		case SHT_PROGBITS:
		case SHT_NOBITS:
			alignmask = shdr[i].sh_addralign - 1;
			mapbase += alignmask;
			mapbase &= ~alignmask;
			ef->progtab[pb].addr = (void *)(uintptr_t)mapbase;
			if (shdr[i].sh_type == SHT_PROGBITS) {
				ef->progtab[pb].name = "<<PROGBITS>>";
				error = vn_rdwr(UIO_READ, nd.ni_vp,
				    ef->progtab[pb].addr,
				    shdr[i].sh_size, shdr[i].sh_offset,
				    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred,
				    NOCRED, &resid, td);
				if (error)
					goto out;
				if (resid != 0){
					error = EINVAL;
					goto out;
				}
			} else {
				ef->progtab[pb].name = "<<NOBITS>>";
				bzero(ef->progtab[pb].addr, shdr[i].sh_size);
			}
			ef->progtab[pb].size = shdr[i].sh_size;
			ef->progtab[pb].sec = i;
			if (ef->shstrtab && shdr[i].sh_name != 0)
				ef->progtab[pb].name =
				    ef->shstrtab + shdr[i].sh_name;

			/* Update all symbol values with the offset. */
			for (j = 0; j < ef->ddbsymcnt; j++) {
				es = &ef->ddbsymtab[j];
				if (es->st_shndx != i)
					continue;
				es->st_value += (Elf_Addr)ef->progtab[pb].addr;
			}
			mapbase += shdr[i].sh_size;
			pb++;
			break;
		case SHT_REL:
			ef->reltab[rl].rel = malloc(shdr[i].sh_size, M_LINKER,
			    M_WAITOK);
			ef->reltab[rl].nrel = shdr[i].sh_size / sizeof(Elf_Rel);
			ef->reltab[rl].sec = shdr[i].sh_info;
			error = vn_rdwr(UIO_READ, nd.ni_vp,
			    (void *)ef->reltab[rl].rel,
			    shdr[i].sh_size, shdr[i].sh_offset,
			    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
			    &resid, td);
			if (error)
				goto out;
			if (resid != 0){
				error = EINVAL;
				goto out;
			}
			rl++;
			break;
		case SHT_RELA:
			ef->relatab[ra].rela = malloc(shdr[i].sh_size, M_LINKER,
			    M_WAITOK);
			ef->relatab[ra].nrela =
			    shdr[i].sh_size / sizeof(Elf_Rela);
			ef->relatab[ra].sec = shdr[i].sh_info;
			error = vn_rdwr(UIO_READ, nd.ni_vp,
			    (void *)ef->relatab[ra].rela,
			    shdr[i].sh_size, shdr[i].sh_offset,
			    UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED,
			    &resid, td);
			if (error)
				goto out;
			if (resid != 0){
				error = EINVAL;
				goto out;
			}
			ra++;
			break;
		}
	}
	if (pb != ef->nprogtab)
		panic("lost progbits");
	if (rl != ef->nrel)
		panic("lost rel");
	if (ra != ef->nrela)
		panic("lost rela");
	if (mapbase != (vm_offset_t)ef->address + mapsize)
		panic("mapbase 0x%lx != address %p + mapsize 0x%lx (0x%lx)\n",
		    mapbase, ef->address, mapsize,
		    (vm_offset_t)ef->address + mapsize);

	/* Local intra-module relocations */
	link_elf_reloc_local(lf);

	/* Pull in dependencies */
	error = linker_load_dependencies(lf);
	if (error)
		goto out;

	/* External relocations */
	error = relocate_file(ef);
	if (error)
		goto out;

	/* Notify MD code that a module is being loaded. */
	error = elf_cpu_load_file(lf);
	if (error)
		goto out;

	*result = lf;

out:
	if (error && lf)
		linker_file_unload(lf, LINKER_UNLOAD_FORCE);
	if (hdr)
		free(hdr, M_LINKER);
	VOP_UNLOCK(nd.ni_vp, 0, td);
	vn_close(nd.ni_vp, FREAD, td->td_ucred, td);

	return error;
}

static void
link_elf_unload_file(linker_file_t file)
{
	elf_file_t ef = (elf_file_t) file;
	int i;

	/* Notify MD code that a module is being unloaded. */
	elf_cpu_unload_file(file);

	if (ef->preloaded) {
		if (ef->reltab)
			free(ef->reltab, M_LINKER);
		if (ef->relatab)
			free(ef->relatab, M_LINKER);
		if (ef->progtab)
			free(ef->progtab, M_LINKER);
		if (file->filename != NULL)
			preload_delete_name(file->filename);
		/* XXX reclaim module memory? */
		return;
	}

	for (i = 0; i < ef->nrel; i++)
		if (ef->reltab[i].rel)
			free(ef->reltab[i].rel, M_LINKER);
	for (i = 0; i < ef->nrela; i++)
		if (ef->relatab[i].rela)
			free(ef->relatab[i].rela, M_LINKER);
	if (ef->reltab)
		free(ef->reltab, M_LINKER);
	if (ef->relatab)
		free(ef->relatab, M_LINKER);
	if (ef->progtab)
		free(ef->progtab, M_LINKER);

	if (ef->object) {
		vm_map_remove(kernel_map, (vm_offset_t) ef->address,
		    (vm_offset_t) ef->address +
		    (ef->object->size << PAGE_SHIFT));
	}
	if (ef->e_shdr)
		free(ef->e_shdr, M_LINKER);
	if (ef->ddbsymtab)
		free(ef->ddbsymtab, M_LINKER);
	if (ef->ddbstrtab)
		free(ef->ddbstrtab, M_LINKER);
	if (ef->shstrtab)
		free(ef->shstrtab, M_LINKER);
}

static const char *
symbol_name(elf_file_t ef, Elf_Word r_info)
{
	const Elf_Sym *ref;

	if (ELF_R_SYM(r_info)) {
		ref = ef->ddbsymtab + ELF_R_SYM(r_info);
		return ef->ddbstrtab + ref->st_name;
	} else
		return NULL;
}

static Elf_Addr
findbase(elf_file_t ef, int sec)
{
	int i;
	Elf_Addr base = 0;

	for (i = 0; i < ef->nprogtab; i++) {
		if (sec == ef->progtab[i].sec) {
			base = (Elf_Addr)ef->progtab[i].addr;
			break;
		}
	}
	return base;
}

static int
relocate_file(elf_file_t ef)
{
	const Elf_Rel *rellim;
	const Elf_Rel *rel;
	const Elf_Rela *relalim;
	const Elf_Rela *rela;
	const char *symname;
	const Elf_Sym *sym;
	int i;
	Elf_Word symidx;
	Elf_Addr base;


	/* Perform relocations without addend if there are any: */
	for (i = 0; i < ef->nrel; i++) {
		rel = ef->reltab[i].rel;
		if (rel == NULL)
			panic("lost a reltab!");
		rellim = rel + ef->reltab[i].nrel;
		base = findbase(ef, ef->reltab[i].sec);
		if (base == 0)
			panic("lost base for reltab");
		for ( ; rel < rellim; rel++) {
			symidx = ELF_R_SYM(rel->r_info);
			if (symidx >= ef->ddbsymcnt)
				continue;
			sym = ef->ddbsymtab + symidx;
			/* Local relocs are already done */
			if (ELF_ST_BIND(sym->st_info) == STB_LOCAL)
				continue;
			if (elf_reloc(&ef->lf, base, rel, ELF_RELOC_REL,
			    elf_obj_lookup)) {
				symname = symbol_name(ef, rel->r_info);
				printf("link_elf_obj: symbol %s undefined\n",
				    symname);
				return ENOENT;
			}
		}
	}

	/* Perform relocations with addend if there are any: */
	for (i = 0; i < ef->nrela; i++) {
		rela = ef->relatab[i].rela;
		if (rela == NULL)
			panic("lost a relatab!");
		relalim = rela + ef->relatab[i].nrela;
		base = findbase(ef, ef->relatab[i].sec);
		if (base == 0)
			panic("lost base for relatab");
		for ( ; rela < relalim; rela++) {
			symidx = ELF_R_SYM(rela->r_info);
			if (symidx >= ef->ddbsymcnt)
				continue;
			sym = ef->ddbsymtab + symidx;
			/* Local relocs are already done */
			if (ELF_ST_BIND(sym->st_info) == STB_LOCAL)
				continue;
			if (elf_reloc(&ef->lf, base, rela, ELF_RELOC_RELA,
			    elf_obj_lookup)) {
				symname = symbol_name(ef, rela->r_info);
				printf("link_elf_obj: symbol %s undefined\n",
				    symname);
				return ENOENT;
			}
		}
	}

	return 0;
}

static int
link_elf_lookup_symbol(linker_file_t lf, const char *name, c_linker_sym_t *sym)
{
	elf_file_t ef = (elf_file_t) lf;
	const Elf_Sym *symp;
	const char *strp;
	int i;

	for (i = 0, symp = ef->ddbsymtab; i < ef->ddbsymcnt; i++, symp++) {
		strp = ef->ddbstrtab + symp->st_name;
		if (symp->st_shndx != SHN_UNDEF && strcmp(name, strp) == 0) {
			*sym = (c_linker_sym_t) symp;
			return 0;
		}
	}
	return ENOENT;
}

static int
link_elf_symbol_values(linker_file_t lf, c_linker_sym_t sym,
    linker_symval_t *symval)
{
	elf_file_t ef = (elf_file_t) lf;
	const Elf_Sym *es = (const Elf_Sym*) sym;

	if (es >= ef->ddbsymtab && es < (ef->ddbsymtab + ef->ddbsymcnt)) {
		symval->name = ef->ddbstrtab + es->st_name;
		symval->value = (caddr_t)es->st_value;
		symval->size = es->st_size;
		return 0;
	}
	return ENOENT;
}

static int
link_elf_search_symbol(linker_file_t lf, caddr_t value,
    c_linker_sym_t *sym, long *diffp)
{
	elf_file_t ef = (elf_file_t) lf;
	u_long off = (uintptr_t) (void *) value;
	u_long diff = off;
	u_long st_value;
	const Elf_Sym *es;
	const Elf_Sym *best = 0;
	int i;

	for (i = 0, es = ef->ddbsymtab; i < ef->ddbsymcnt; i++, es++) {
		if (es->st_name == 0)
			continue;
		st_value = es->st_value;
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
	elf_file_t ef = (elf_file_t)lf;
	void **start, **stop;
	int i, count;

	/* Relative to section number */
	for (i = 0; i < ef->nprogtab; i++) {
		if ((strncmp(ef->progtab[i].name, "set_", 4) == 0) &&
		    strcmp(ef->progtab[i].name + 4, name) == 0) {
			start  = (void **)ef->progtab[i].addr;
			stop = (void **)((char *)ef->progtab[i].addr +
			    ef->progtab[i].size);
			count = stop - start;
			if (startp)
				*startp = start;
			if (stopp)
				*stopp = stop;
			if (countp)
				*countp = count;
			return (0);
		}
	}
	return (ESRCH);
}

static int
link_elf_each_function_name(linker_file_t file,
    int (*callback)(const char *, void *), void *opaque)
{
	elf_file_t ef = (elf_file_t)file;
	const Elf_Sym *symp;
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

/*
 * Symbol lookup function that can be used when the symbol index is known (ie
 * in relocations). It uses the symbol index instead of doing a fully fledged
 * hash table based lookup when such is valid. For example for local symbols.
 * This is not only more efficient, it's also more correct. It's not always
 * the case that the symbol can be found through the hash table.
 */
static Elf_Addr
elf_obj_lookup(linker_file_t lf, Elf_Word symidx, int deps)
{
	elf_file_t ef = (elf_file_t)lf;
	const Elf_Sym *sym;
	const char *symbol;
	Elf_Addr ret;

	/* Don't even try to lookup the symbol if the index is bogus. */
	if (symidx >= ef->ddbsymcnt)
		return (0);

	sym = ef->ddbsymtab + symidx;

	/* Quick answer if there is a definition included. */
	if (sym->st_shndx != SHN_UNDEF)
		return (sym->st_value);

	/* If we get here, then it is undefined and needs a lookup. */
	switch (ELF_ST_BIND(sym->st_info)) {
	case STB_LOCAL:
		/* Local, but undefined? huh? */
		return (0);

	case STB_GLOBAL:
		/* Relative to Data or Function name */
		symbol = ef->ddbstrtab + sym->st_name;

		/* Force a lookup failure if the symbol name is bogus. */
		if (*symbol == 0)
			return (0);
		ret = ((Elf_Addr)linker_file_lookup_symbol(lf, symbol, deps));
		return ret;

	case STB_WEAK:
		printf("link_elf_obj: Weak symbols not supported\n");
		return (0);

	default:
		return (0);
	}
}

static void
link_elf_reloc_local(linker_file_t lf)
{
	elf_file_t ef = (elf_file_t)lf;
	const Elf_Rel *rellim;
	const Elf_Rel *rel;
	const Elf_Rela *relalim;
	const Elf_Rela *rela;
	const Elf_Sym *sym;
	Elf_Addr base;
	int i;
	Elf_Word symidx;

	/* Perform relocations without addend if there are any: */
	for (i = 0; i < ef->nrel; i++) {
		rel = ef->reltab[i].rel;
		if (rel == NULL)
			panic("lost a reltab!");
		rellim = rel + ef->reltab[i].nrel;
		base = findbase(ef, ef->reltab[i].sec);
		if (base == 0)
			panic("lost base for reltab");
		for ( ; rel < rellim; rel++) {
			symidx = ELF_R_SYM(rel->r_info);
			if (symidx >= ef->ddbsymcnt)
				continue;
			sym = ef->ddbsymtab + symidx;
			/* Only do local relocs */
			if (ELF_ST_BIND(sym->st_info) != STB_LOCAL)
				continue;
			elf_reloc_local(lf, base, rel, ELF_RELOC_REL,
			    elf_obj_lookup);
		}
	}

	/* Perform relocations with addend if there are any: */
	for (i = 0; i < ef->nrela; i++) {
		rela = ef->relatab[i].rela;
		if (rela == NULL)
			panic("lost a relatab!");
		relalim = rela + ef->relatab[i].nrela;
		base = findbase(ef, ef->relatab[i].sec);
		if (base == 0)
			panic("lost base for relatab");
		for ( ; rela < relalim; rela++) {
			symidx = ELF_R_SYM(rela->r_info);
			if (symidx >= ef->ddbsymcnt)
				continue;
			sym = ef->ddbsymtab + symidx;
			/* Only do local relocs */
			if (ELF_ST_BIND(sym->st_info) != STB_LOCAL)
				continue;
			elf_reloc_local(lf, base, rela, ELF_RELOC_RELA,
			    elf_obj_lookup);
		}
	}
}
