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
 *	$Id: link_aout.c,v 1.2 1997/08/02 14:31:35 bde Exp $
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
#include <a.out.h>
#include <link.h>

static int		link_aout_load_file(const char*, linker_file_t*);

static int		link_aout_lookup_symbol(linker_file_t, const char*,
						caddr_t*, size_t*);
static void		link_aout_unload(linker_file_t);

static struct linker_class_ops link_aout_class_ops = {
    link_aout_load_file,
};

static struct linker_file_ops link_aout_file_ops = {
    link_aout_lookup_symbol,
    link_aout_unload,
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
    struct _dynamic* dp = &_DYNAMIC;

    linker_add_class("a.out", NULL, &link_aout_class_ops);

    if (dp) {
	aout_file_t af;

	af = malloc(sizeof(struct aout_file), M_LINKER, M_NOWAIT);
	if (af == NULL)
	    panic("link_aout_init: Can't create linker structures for kernel");

	af->address = 0;
	af->dynamic = dp;
	linker_kernel_file =
	    linker_make_file(kernelname, af, &link_aout_file_ops);
	if (linker_kernel_file == NULL)
	    panic("link_aout_init: Can't create linker structures for kernel");
	/*
	 * XXX there must be a better way of getting these constants.
	 */
	linker_kernel_file->address = (caddr_t) 0xf0100000;
	linker_kernel_file->size = -0xf0100000;
	linker_current_file = linker_kernel_file;
    }
}

SYSINIT(link_aout, SI_SUB_KMEM, SI_ORDER_THIRD, link_aout_init, 0);

static int
link_aout_load_file(const char* filename, linker_file_t* result)
{
    struct nameidata nd;
    struct vnode* file;
    struct proc* p = curproc;	/* XXX */
    int error = 0;
    int resid;
    struct iovec aiov;
    struct uio auio;
    struct exec header;
    aout_file_t af;
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
    af = malloc(sizeof(struct aout_file), M_LINKER, M_WAITOK);
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
    (long) af->dynamic->d_un.d_sdt += af->address;

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

    *result = lf;

out:
    VOP_UNLOCK(nd.ni_vp, 0, p);
    vn_close(nd.ni_vp, FREAD, p->p_ucred, p);

    return error;
}

static void
link_aout_unload(linker_file_t file)
{
    aout_file_t af = file->priv;

    if (af) {
	if (af->address)
	    free(af->address, M_LINKER);
	free(af, M_LINKER);
    }
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
    linker_kernel_file->refs++;
    linker_file_add_dependancy(lf, linker_kernel_file);

    off = LD_NEED(af->dynamic);

    /*
     * Load the dependancies.
     */
    while (off != 0) {
	sodp = AOUT_RELOC(af, struct sod, off);
	name = AOUT_RELOC(af, char, sodp->sod_name);

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
		relocation = (long)
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
		relocation -= (long) af->address;

	    if (r->r_copy) {
		printf("link_aout: can't cope with copy relocations\n");
		continue;
	    }
	    
	    write_relocation(r, addr, relocation);
	} else {
	    write_relocation(r, addr,
			     (long)(read_relocation(r, addr) + af->address));
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
			caddr_t* address, size_t* size)
{
    aout_file_t af = file->priv;
    int buckets;
    long hashval;
    struct rrs_hash* hashbase;
    struct nzlist* symbolbase;
    char* stringbase;
    struct rrs_hash* hp;
    struct nzlist* np;
    char* cp;

    if (LD_BUCKETS(af->dynamic) == 0)
	return NULL;

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
	*address = 0;
	*size = np->nz_value;
    } else {
	*address = AOUT_RELOC(af, char, np->nz_value);
	*size = np->nz_size;
    }

    return 0;
}
