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
 *	$Id$
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
    caddr_t		address;	/* Load address */
    Elf_Dyn*		dynamic;	/* Symbol table etc. */
    Elf_Off		nbuckets;	/* DT_HASH info */
    Elf_Off		nchains;
    const Elf_Off*	buckets;
    const Elf_Off*	chains;
    caddr_t		hash;
    caddr_t		strtab;		/* DT_STRTAB */
    Elf_Sym*		symtab;		/* DT_SYMTAB */
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
    Elf_Dyn *dp;

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
	    ef->strtab = (caddr_t) dp->d_un.d_ptr;
	    break;
	case DT_SYMTAB:
	    ef->symtab = (Elf_Sym*) dp->d_un.d_ptr;
	    break;
	case DT_SYMENT:
	    if (dp->d_un.d_val != sizeof(Elf_Sym))
		return ENOEXEC;
	}
    }
    return 0;
}

static int
link_elf_load_file(const char* filename, linker_file_t* result)
{
#if 0
    struct nameidata nd;
    struct proc* p = curproc;	/* XXX */
    int error = 0;
    int resid;
    struct exec header;
    elf_file_t ef;
    linker_file_t lf;

    NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, filename, p);
    error = vn_open(&nd, FREAD, 0);
    if (error)
	return error;

    /*
     * Read the a.out header from the file.
     */
    error = vn_rdwr(UIO_READ, nd.ni_vp, (void*) &header, sizeof header, 0,
		    UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid, p);
    if (error)
	goto out;

    if (N_BADMAG(header) || !(N_GETFLAG(header) & EX_DYNAMIC))
	goto out;

    /*
     * We have an a.out file, so make some space to read it in.
     */
    ef = malloc(sizeof(struct elf_file), M_LINKER, M_WAITOK);
    ef->address = malloc(header.a_text + header.a_data + header.a_bss,
			 M_LINKER, M_WAITOK);
    
    /*
     * Read the text and data sections and zero the bss.
     */
    error = vn_rdwr(UIO_READ, nd.ni_vp, (void*) ef->address,
		    header.a_text + header.a_data, 0,
		    UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid, p);
    if (error)
	goto out;
    bzero(ef->address + header.a_text + header.a_data, header.a_bss);

    /*
     * Assume _DYNAMIC is the first data item.
     */
    ef->dynamic = (struct _dynamic*) (ef->address + header.a_text);
    if (ef->dynamic->d_version != LD_VERSION_BSD) {
	free(ef->address, M_LINKER);
	free(ef, M_LINKER);
	goto out;
    }
    (long) ef->dynamic->d_un.d_sdt += ef->address;

    lf = linker_make_file(filename, ef, &link_elf_file_ops);
    if (lf == NULL) {
	free(ef->address, M_LINKER);
	free(ef, M_LINKER);
	error = ENOMEM;
	goto out;
    }
    lf->address = ef->address;
    lf->size = header.a_text + header.a_data + header.a_bss;

    if ((error = load_dependancies(lf)) != 0
	|| (error = relocate_file(lf)) != 0) {
	linker_file_unload(lf);
	goto out;
    }

    *result = lf;

out:
    VOP_UNLOCK(nd.ni_vp, 0, p);
    vn_close(nd.ni_vp, FREAD, p->p_ucred, p);

    return error;
#else
    return ENOEXEC;
#endif
}

static void
link_elf_unload(linker_file_t file)
{
    elf_file_t ef = file->priv;

    if (ef) {
	if (ef->address)
	    free(ef->address, M_LINKER);
	free(ef, M_LINKER);
    }
}

#define ELF_RELOC(ef, type, off) (type*) ((ef)->address + (off))

static int
load_dependancies(linker_file_t lf)
{
#if 0
    elf_file_t ef = lf->priv;
    linker_file_t lfdep;
    long off;
    struct sod* sodp;
    char* name;
    char* filename = 0;
    int error = 0;

    /*
     * All files are dependant on /kernel.
     */
    linker_kernel_file->refs++;
    linker_file_add_dependancy(lf, linker_kernel_file);

    off = LD_NEED(ef->dynamic);

    /*
     * Load the dependancies.
     */
    while (off != 0) {
	sodp = ELF_RELOC(ef, struct sod, off);
	name = ELF_RELOC(ef, char, sodp->sod_name);

	/*
	 * Prepend pathname if dep is not an absolute filename.
	 */
	if (name[0] != '/') {
	    char* p;
	    filename = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
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
	off = sodp->sod_next;
    }

out:
    if (filename)
	free(filename, M_TEMP);
    return error;
#else
    return ENOEXEC;
#endif
}

#if 0
/*
 * XXX i386 dependant.
 */
static long
read_relocation(struct relocation_info* r, char* addr)
{
    int length = r->r_length;
    if (length == 0)
	return *(u_char*) addr;
    else if (length == 1)
	return *(u_short*) addr;
    else if (length == 2)
	return *(u_int*) addr;
    else
	printf("link_elf: unsupported relocation size %d\n", r->r_length);
    return 0;
}

static void
write_relocation(struct relocation_info* r, char* addr, long value)
{
    int length = r->r_length;
    if (length == 0)
	*(u_char*) addr = value;
    else if (length == 1)
	*(u_short*) addr = value;
    else if (length == 2)
	*(u_int*) addr = value;
    else
	printf("link_elf: unsupported relocation size %d\n", r->r_length);
}

static int
relocate_file(linker_file_t lf)
{
    elf_file_t ef = lf->priv;
    struct relocation_info* rel;
    struct relocation_info* erel;
    struct relocation_info* r;
    struct nzlist* symbolbase;
    char* stringbase;
    struct nzlist* np;
    char* sym;
    long relocation;

    rel = ELF_RELOC(ef, struct relocation_info, LD_REL(ef->dynamic));
    erel = ELF_RELOC(ef, struct relocation_info,
		      LD_REL(ef->dynamic) + LD_RELSZ(ef->dynamic));
    symbolbase = ELF_RELOC(ef, struct nzlist, LD_SYMBOL(ef->dynamic));
    stringbase = ELF_RELOC(ef, char, LD_STRINGS(ef->dynamic));

    for (r = rel; r < erel; r++) {
	char* addr;

	if (r->r_address == 0)
	    break;

	addr = ELF_RELOC(ef, char, r->r_address);
	if (r->r_extern) {
	    np = &symbolbase[r->r_symbolnum];
	    sym = &stringbase[np->nz_strx];

	    if (sym[0] != '_') {
		printf("link_elf: bad symbol name %s\n", sym);
		relocation = 0;
	    } else
		relocation = (long)
		    linker_file_lookup_symbol(lf, sym + 1,
					      np->nz_type != (N_SETV+N_EXT));
	    if (!relocation) {
		printf("link_elf: symbol %s not found\n", sym);
		return ENOENT;
	    }
	    
	    relocation += read_relocation(r, addr);

	    if (r->r_jmptable) {
		printf("link_elf: can't cope with jump table relocations\n");
		continue;
	    }

	    if (r->r_pcrel)
		relocation -= (long) ef->address;

	    if (r->r_copy) {
		printf("link_elf: can't cope with copy relocations\n");
		continue;
	    }
	    
	    write_relocation(r, addr, relocation);
	} else {
	    write_relocation(r, addr,
			     (long)(read_relocation(r, addr) + ef->address));
	}
	
    }

    return 0;
}

static long
symbol_hash_value(elf_file_t ef, const char* name)
{
    long hashval;
    const char* p;

    hashval = '_';		/* fake a starting '_' for C symbols */
    for (p = name; *p; p++)
	hashval = (hashval << 1) + *p;

    return (hashval & 0x7fffffff) % LD_BUCKETS(ef->dynamic);
}

#endif

int
link_elf_lookup_symbol(linker_file_t lf, const char* name, linker_sym_t* sym)
{
	elf_file_t ef = lf->priv;
	int symcount = ef->nchains;
	Elf_Sym* es;
	int i;

	/* XXX use hash table */
	for (i = 0, es = ef->symtab; i < ef->nchains; i++, es++) {
		if (es->st_name == 0)
			continue;
		if (!strcmp(ef->strtab + es->st_name, name)) {
			*sym = (linker_sym_t) es;
			return 0;
		}
	}

	return ENOENT;
}

static void
link_elf_symbol_values(linker_file_t lf, linker_sym_t sym, linker_symval_t* symval)
{
	elf_file_t ef = lf->priv;
	Elf_Sym* es = (Elf_Sym*) sym;

	symval->name = ef->strtab + es->st_name;
	symval->value = (caddr_t) es->st_value;
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
	Elf_Sym* es;
	Elf_Sym* best = 0;
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

