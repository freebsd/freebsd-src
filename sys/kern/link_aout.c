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
 * $FreeBSD$
 */

#ifndef __alpha__

#define FREEBSD_AOUT	1

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/linker.h>

#include <vm/vm_zone.h>

#ifndef __ELF__
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/vmparam.h>
#endif

#include <a.out.h>
#include <link.h>

static int		link_aout_load_module(const char*, linker_file_t*);

static int		link_aout_load_file(const char*, linker_file_t*);

static int		link_aout_lookup_symbol(linker_file_t, const char*,
						c_linker_sym_t*);
static int		link_aout_symbol_values(linker_file_t file, c_linker_sym_t sym,
						linker_symval_t* symval);
static int		link_aout_search_symbol(linker_file_t lf, caddr_t value,
						c_linker_sym_t* sym, long* diffp);
static void		link_aout_unload_file(linker_file_t);
static void		link_aout_unload_module(linker_file_t);

static struct linker_class_ops link_aout_class_ops = {
    link_aout_load_module,
};

static struct linker_file_ops link_aout_file_ops = {
    link_aout_lookup_symbol,
    link_aout_symbol_values,
    link_aout_search_symbol,
    link_aout_unload_file,
};
static struct linker_file_ops link_aout_module_ops = {
    link_aout_lookup_symbol,
    link_aout_symbol_values,
    link_aout_search_symbol,
    link_aout_unload_module,
};

typedef struct aout_file {
    char*		address;	/* Load address */
    struct _dynamic*	dynamic;	/* Symbol table etc. */
} *aout_file_t;

static int		load_dependancies(linker_file_t lf);
static int		relocate_file(linker_file_t lf);

/*
 * The kernel symbol table starts here.
 */
extern struct _dynamic _DYNAMIC;

static void
link_aout_init(void* arg)
{
#ifndef __ELF__
    struct _dynamic* dp = &_DYNAMIC;
#endif

    linker_add_class("a.out", NULL, &link_aout_class_ops);

#ifndef __ELF__
    if (dp) {
	aout_file_t af;

	af = malloc(sizeof(struct aout_file), M_LINKER, M_NOWAIT);
	if (af == NULL)
	    panic("link_aout_init: Can't create linker structures for kernel");
	bzero(af, sizeof(*af));

	af->address = 0;
	af->dynamic = dp;
	linker_kernel_file =
	    linker_make_file(kernelname, af, &link_aout_file_ops);
	if (linker_kernel_file == NULL)
	    panic("link_aout_init: Can't create linker structures for kernel");
	linker_kernel_file->address = (caddr_t) KERNBASE;
	linker_kernel_file->size = -(long)linker_kernel_file->address;
	linker_current_file = linker_kernel_file;
	linker_kernel_file->flags |= LINKER_FILE_LINKED;
    }
#endif
}

SYSINIT(link_aout, SI_SUB_KLD, SI_ORDER_THIRD, link_aout_init, 0);

static int
link_aout_load_module(const char* filename, linker_file_t* result)
{
    caddr_t		modptr, baseptr;
    char		*type;
    struct exec		*ehdr;
    aout_file_t		af;
    linker_file_t	lf;
    int			error;
    
    /* Look to see if we have the module preloaded. */
    if ((modptr = preload_search_by_name(filename)) == NULL)
	return(link_aout_load_file(filename, result));

    /* It's preloaded, check we can handle it and collect information. */
    if (((type = (char *)preload_search_info(modptr, MODINFO_TYPE)) == NULL) ||
	strcmp(type, "a.out module") ||
	((baseptr = preload_search_info(modptr, MODINFO_ADDR)) == NULL) ||
	((ehdr = (struct exec *)preload_search_info(modptr, MODINFO_METADATA | MODINFOMD_AOUTEXEC)) == NULL))
	return(0);			/* we can't handle this */

    /* Looks like we can handle this one */
    af = malloc(sizeof(struct aout_file), M_LINKER, M_WAITOK);
    bzero(af, sizeof(*af));
    af->address = baseptr;

    /* Assume _DYNAMIC is the first data item. */
    af->dynamic = (struct _dynamic*)(af->address + ehdr->a_text);
    if (af->dynamic->d_version != LD_VERSION_BSD) {
	free(af, M_LINKER);
	return(0);			/* we can't handle this */
    }
    af->dynamic->d_un.d_sdt = (struct section_dispatch_table *)
	((char *)af->dynamic->d_un.d_sdt + (vm_offset_t)af->address);

    /* Register with kld */
    lf = linker_make_file(filename, af, &link_aout_module_ops);
    if (lf == NULL) {
	free(af, M_LINKER);
	return(ENOMEM);
    }
    lf->address = af->address;
    lf->size = ehdr->a_text + ehdr->a_data + ehdr->a_bss;

    /* Try to load dependancies */
    if (((error = load_dependancies(lf)) != 0) ||
	((error = relocate_file(lf)) != 0)) {
	linker_file_unload(lf);
	return(error);
    }
    lf->flags |= LINKER_FILE_LINKED;
    *result = lf;
    return(0);
}

static int
link_aout_load_file(const char* filename, linker_file_t* result)
{
    struct nameidata nd;
    struct proc* p = curproc;	/* XXX */
    int error = 0;
    int resid;
    struct exec header;
    aout_file_t af;
    linker_file_t lf;
    char *pathname;

    pathname = linker_search_path(filename);
    if (pathname == NULL)
	return ENOENT;
    NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, pathname, p);
    error = vn_open(&nd, FREAD, 0);
    free(pathname, M_LINKER);
    if (error)
	return error;
    NDFREE(&nd, NDF_ONLY_PNBUF);

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
    af = malloc(sizeof(struct aout_file), M_LINKER, M_WAITOK);
    bzero(af, sizeof(*af));
    af->address = malloc(header.a_text + header.a_data + header.a_bss,
			 M_LINKER, M_WAITOK);
    
    /*
     * Read the text and data sections and zero the bss.
     */
    error = vn_rdwr(UIO_READ, nd.ni_vp, (void*) af->address,
		    header.a_text + header.a_data, 0,
		    UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid, p);
    if (error)
	goto out;
    bzero(af->address + header.a_text + header.a_data, header.a_bss);

    /*
     * Assume _DYNAMIC is the first data item.
     */
    af->dynamic = (struct _dynamic*) (af->address + header.a_text);
    if (af->dynamic->d_version != LD_VERSION_BSD) {
	free(af->address, M_LINKER);
	free(af, M_LINKER);
	goto out;
    }
    af->dynamic->d_un.d_sdt = (struct section_dispatch_table *)
	((char *)af->dynamic->d_un.d_sdt + (vm_offset_t)af->address);

    lf = linker_make_file(filename, af, &link_aout_file_ops);
    if (lf == NULL) {
	free(af->address, M_LINKER);
	free(af, M_LINKER);
	error = ENOMEM;
	goto out;
    }
    lf->address = af->address;
    lf->size = header.a_text + header.a_data + header.a_bss;

    if ((error = load_dependancies(lf)) != 0
	|| (error = relocate_file(lf)) != 0) {
	linker_file_unload(lf);
	goto out;
    }

    lf->flags |= LINKER_FILE_LINKED;
    *result = lf;

out:
    VOP_UNLOCK(nd.ni_vp, 0, p);
    vn_close(nd.ni_vp, FREAD, p->p_ucred, p);

    return error;
}

static void
link_aout_unload_file(linker_file_t file)
{
    aout_file_t af = file->priv;

    if (af) {
	if (af->address)
	    free(af->address, M_LINKER);
	free(af, M_LINKER);
    }
}

static void
link_aout_unload_module(linker_file_t file)
{
    aout_file_t af = file->priv;

    if (af)
	free(af, M_LINKER);
    if (file->filename)
	preload_delete_name(file->filename);
}

#define AOUT_RELOC(af, type, off) (type*) ((af)->address + (off))

static int
load_dependancies(linker_file_t lf)
{
    aout_file_t af = lf->priv;
    linker_file_t lfdep;
    long off;
    struct sod* sodp;
    char* name;
    char* filename = 0;
    int error = 0;

    /*
     * All files are dependant on /kernel.
     */
    if (linker_kernel_file) {
	linker_kernel_file->refs++;
	linker_file_add_dependancy(lf, linker_kernel_file);
    }

    off = LD_NEED(af->dynamic);

    /*
     * Load the dependancies.
     */
    while (off != 0) {
	sodp = AOUT_RELOC(af, struct sod, off);
	name = AOUT_RELOC(af, char, sodp->sod_name);

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
}

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
	printf("link_aout: unsupported relocation size %d\n", r->r_length);
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
	printf("link_aout: unsupported relocation size %d\n", r->r_length);
}

static int
relocate_file(linker_file_t lf)
{
    aout_file_t af = lf->priv;
    struct relocation_info* rel;
    struct relocation_info* erel;
    struct relocation_info* r;
    struct nzlist* symbolbase;
    char* stringbase;
    struct nzlist* np;
    char* sym;
    long relocation;

    rel = AOUT_RELOC(af, struct relocation_info, LD_REL(af->dynamic));
    erel = AOUT_RELOC(af, struct relocation_info,
		      LD_REL(af->dynamic) + LD_RELSZ(af->dynamic));
    symbolbase = AOUT_RELOC(af, struct nzlist, LD_SYMBOL(af->dynamic));
    stringbase = AOUT_RELOC(af, char, LD_STRINGS(af->dynamic));

    for (r = rel; r < erel; r++) {
	char* addr;

	if (r->r_address == 0)
	    break;

	addr = AOUT_RELOC(af, char, r->r_address);
	if (r->r_extern) {
	    np = &symbolbase[r->r_symbolnum];
	    sym = &stringbase[np->nz_strx];

	    if (sym[0] != '_') {
		printf("link_aout: bad symbol name %s\n", sym);
		relocation = 0;
	    } else
		relocation = (intptr_t)
		    linker_file_lookup_symbol(lf, sym + 1,
					      np->nz_type != (N_SETV+N_EXT));
	    if (!relocation) {
		printf("link_aout: symbol %s not found\n", sym);
		return ENOENT;
	    }
	    
	    relocation += read_relocation(r, addr);

	    if (r->r_jmptable) {
		printf("link_aout: can't cope with jump table relocations\n");
		continue;
	    }

	    if (r->r_pcrel)
		relocation -= (intptr_t) af->address;

	    if (r->r_copy) {
		printf("link_aout: can't cope with copy relocations\n");
		continue;
	    }
	    
	    write_relocation(r, addr, relocation);
	} else {
	    write_relocation(r, addr,
			     (intptr_t)(read_relocation(r, addr) + af->address));
	}
	
    }

    return 0;
}

static long
symbol_hash_value(aout_file_t af, const char* name)
{
    long hashval;
    const char* p;

    hashval = '_';		/* fake a starting '_' for C symbols */
    for (p = name; *p; p++)
	hashval = (hashval << 1) + *p;

    return (hashval & 0x7fffffff) % LD_BUCKETS(af->dynamic);
}

int
link_aout_lookup_symbol(linker_file_t file, const char* name,
			c_linker_sym_t* sym)
{
    aout_file_t af = file->priv;
    long hashval;
    struct rrs_hash* hashbase;
    struct nzlist* symbolbase;
    char* stringbase;
    struct rrs_hash* hp;
    struct nzlist* np;
    char* cp;

    if (LD_BUCKETS(af->dynamic) == 0)
	return 0;

    hashbase = AOUT_RELOC(af, struct rrs_hash, LD_HASH(af->dynamic));
    symbolbase = AOUT_RELOC(af, struct nzlist, LD_SYMBOL(af->dynamic));
    stringbase = AOUT_RELOC(af, char, LD_STRINGS(af->dynamic));

restart:
    hashval = symbol_hash_value(af, name);
    hp = &hashbase[hashval];
    if (hp->rh_symbolnum == -1)
	return ENOENT;

    while (hp) {
	np = (struct nzlist *) &symbolbase[hp->rh_symbolnum];
	cp = stringbase + np->nz_strx;
	/*
	 * Note: we fake the leading '_' for C symbols.
	 */
	if (cp[0] == '_' && !strcmp(cp + 1, name))
	    break;

	if (hp->rh_next == 0)
	    hp = NULL;
	else
	    hp = &hashbase[hp->rh_next];
    }

    if (hp == NULL)
	/*
	 * Not found.
	 */
	return ENOENT;

    /*
     * Check for an aliased symbol, whatever that is.
     */
    if (np->nz_type == N_INDR+N_EXT) {
	name = stringbase + (++np)->nz_strx + 1; /* +1 for '_' */
	goto restart;
    }

    /*
     * Check this is an actual definition of the symbol.
     */
    if (np->nz_value == 0)
	return ENOENT;

    if (np->nz_type == N_UNDF+N_EXT && np->nz_value != 0) {
	if (np->nz_other == AUX_FUNC)
	    /* weak function */
	    return ENOENT;
    }

    *sym = (linker_sym_t) np;

    return 0;
}


static int
link_aout_symbol_values(linker_file_t file, c_linker_sym_t sym,
			linker_symval_t* symval)
{
    aout_file_t af = file->priv;
    const struct nzlist* np = (const struct nzlist*) sym;
    char* stringbase;
    long numsym = LD_STABSZ(af->dynamic) / sizeof(struct nzlist);
    struct nzlist *symbase;

    /* Is it one of ours?  It could be another module... */
    symbase = AOUT_RELOC(af, struct nzlist, LD_SYMBOL(af->dynamic));
    if (np < symbase)
	return ENOENT;
    if ((np - symbase) > numsym)
	return ENOENT;

    stringbase = AOUT_RELOC(af, char, LD_STRINGS(af->dynamic));

    symval->name = stringbase + np->nz_strx + 1; /* +1 for '_' */
    if (np->nz_type == N_UNDF+N_EXT && np->nz_value != 0) {
	symval->value = 0;
	symval->size = np->nz_value;
    } else {
	symval->value = AOUT_RELOC(af, char, np->nz_value);
	symval->size = np->nz_size;
    }
    return 0;
}

static int
link_aout_search_symbol(linker_file_t lf, caddr_t value,
			c_linker_sym_t* sym, long* diffp)
{
	aout_file_t af = lf->priv;
	u_long off = (uintptr_t) (void *) value;
	u_long diff = off;
	u_long sp_nz_value;
	struct nzlist* sp;
	struct nzlist* ep;
	struct nzlist* best = 0;

	for (sp = AOUT_RELOC(af, struct nzlist, LD_SYMBOL(af->dynamic)),
		 ep = (struct nzlist *) ((caddr_t) sp + LD_STABSZ(af->dynamic));
	     sp < ep; sp++) {
		if (sp->nz_name == 0)
			continue;
		sp_nz_value = sp->nz_value + (uintptr_t) (void *) af->address;
		if (off >= sp_nz_value) {
			if (off - sp_nz_value < diff) {
				diff = off - sp_nz_value;
				best = sp;
				if (diff == 0)
					break;
			} else if (off - sp_nz_value == diff) {
				best = sp;
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

#endif /* !__alpha__ */
