/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *	$Id: rtld.c,v 1.8 1993/11/08 13:20:40 pk Exp $
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#ifndef BSD
#define MAP_COPY	MAP_PRIVATE
#define MAP_FILE	0
#define MAP_ANON	0
#endif
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ld.h"

#ifndef BSD		/* Need do better than this */
#define NEED_DEV_ZERO	1
#endif

/*
 * Loader private data, hung off link_map->lm_lpd
 */
struct lm_private {
	int		lpd_version;
	struct link_map	*lpd_parent;
#ifdef SUN_COMPAT
	long		lpd_offset;	/* Correction for Sun main programs */
#endif
};

#ifdef SUN_COMPAT
#define LM_OFFSET(lmp)	(((struct lm_private *)((lmp)->lm_lpd))->lpd_offset)
#else
#define LM_OFFSET(lmp)	(0)
#endif

/* Base address for link_dynamic_2 entries */
#define LM_LDBASE(lmp)	(lmp->lm_addr + LM_OFFSET(lmp))

/* Start of text segment */
#define LM_TXTADDR(lmp)	(lmp->lm_addr == (caddr_t)0 ? PAGSIZ : 0)

/* Start of run-time relocation_info */
#define LM_REL(lmp)	((struct relocation_info *) \
			(lmp->lm_addr + LM_OFFSET(lmp) + LD_REL((lmp)->lm_ld)))

/* Start of symbols */
#define LM_SYMBOL(lmp, i)	((struct nzlist *) \
		(lmp->lm_addr + LM_OFFSET(lmp) + LD_SYMBOL((lmp)->lm_ld) + \
			i * (LD_VERSION_NZLIST_P(lmp->lm_ld->ld_version) ? \
				sizeof(struct nzlist) : sizeof(struct nlist))))

/* Start of hash table */
#define LM_HASH(lmp)	((struct rrs_hash *) \
		(lmp->lm_addr + LM_OFFSET(lmp) + LD_HASH((lmp)->lm_ld)))

/* Start of strings */
#define LM_STRINGS(lmp)	((char *) \
		(lmp->lm_addr + LM_OFFSET(lmp) + LD_STRINGS((lmp)->lm_ld)))

/* End of text */
#define LM_ETEXT(lmp)	((char *) \
		(lmp->lm_addr + LM_TXTADDR(lmp) + LD_TEXTSZ((lmp)->lm_ld)))

/* PLT is in data segment, so don't use LM_OFFSET here */
#define LM_PLT(lmp)	((jmpslot_t *) \
		(lmp->lm_addr + LD_PLT((lmp)->lm_ld)))

/* Parent of link map */
#define LM_PARENT(lmp)	(((struct lm_private *)((lmp)->lm_lpd))->lpd_parent)

char			**environ;
int			errno;
uid_t			uid, euid;
gid_t			gid, egid;
int			careful;

struct link_map		*link_map_head, *main_map;
struct link_map		**link_map_tail = &link_map_head;
struct rt_symbol	*rt_symbol_head;

static int		dlopen(), dlclose(), dlsym();

static struct ld_entry	ld_entry = {
	dlopen, dlclose, dlsym
};

static void		xprintf __P((char *, ...));
static void		init_brk __P((void));
static void		load_maps __P((struct crt_ldso *));
static void		map_object __P((struct link_object *, struct link_map *));
static void		alloc_link_map __P((	char *, struct link_object *,
						struct link_map *, caddr_t,
						struct link_dynamic *));
static void		check_text_reloc __P((	struct relocation_info *,
						struct link_map *,
						caddr_t));
static void		reloc_maps __P((void));
static void		reloc_copy __P((void));
static char		*rtfindlib __P((char *, int, int, int *));
void			binder_entry __P((void));
long			binder __P((jmpslot_t *));
static struct nzlist	*lookup __P((char *, struct link_map **));
static struct rt_symbol	*lookup_rts __P((char *));
static struct rt_symbol	*enter_rts __P((char *, long, int, caddr_t, long));

#include "md-static-funcs.c"

/*
 * Called from assembler stub that has set up crtp (passed from crt0)
 * and dp (our __DYNAMIC).
 */
void
rtld(version, crtp, dp)
int			version;
struct crt_ldso		*crtp;
struct link_dynamic	*dp;
{
	int			n;
	int			nreloc;		/* # of ld.so relocations */
	struct relocation_info	*reloc;
	char			**envp;

	/* Check version */
	if (version != CRT_VERSION_BSD && version != CRT_VERSION_SUN)
		return;

	/* Fixup __DYNAMIC structure */
	(long)dp->ld_un.ld_2 += crtp->crt_ba;

	/* Be careful not to use .div routine from library */
	for (	nreloc = 0, n = LD_RELSZ(dp);
		n > 0;
		n -= sizeof(struct relocation_info) ) nreloc++;

	
	/* Relocate ourselves */
	for (	reloc = (struct relocation_info *)
				(dp->ld_un.ld_2->ld_rel + crtp->crt_ba);
		nreloc;
		nreloc--, reloc++) {

		register long	addr = reloc->r_address + crtp->crt_ba;

		md_relocate_simple(reloc, crtp->crt_ba, addr);
	}

	progname = "ld.so";

	/* Setup out (private) environ variable */
	environ = crtp->crt_ep;

	/* Get user and group identifiers */
	uid = getuid(); euid = geteuid();
	gid = getgid(); egid = getegid();

	careful = (uid != euid) || (gid != egid);

	if (careful) {
		unsetenv("LD_LIBRARY_PATH");
		unsetenv("LD_PRELOAD");
		unsetenv("LD_RUN_PATH"); /* In case we ever implement this */
	}

	/* Setup directory search */
	std_search_dirs(getenv("LD_LIBRARY_PATH"));

	/* Load required objects into the process address space */
	load_maps(crtp);

	/* Relocate all loaded objects according to their RRS segments */
	reloc_maps();
	reloc_copy();

	/* Fill in some field in main's __DYNAMIC structure */
	crtp->crt_dp->ld_entry = &ld_entry;
	crtp->crt_dp->ldd->ldd_cp = rt_symbol_head;
}


static void
load_maps(crtp)
struct crt_ldso	*crtp;
{
	struct link_map		*lmp;
	int			tracing = (int)getenv("LD_TRACE_LOADED_OBJECTS");

	/* Handle LD_PRELOAD's here */

	/* Make an entry for the main program */
	alloc_link_map("main", (struct link_object *)0, (struct link_map *)0,
					(caddr_t)0, crtp->crt_dp);

	for (lmp = link_map_head; lmp; lmp = lmp->lm_next) {
		struct link_object	*lop;
		long			next = 0;

		if (lmp->lm_ld)
			next = LD_NEED(lmp->lm_ld);

		while (next) {
			lop = (struct link_object *) (LM_LDBASE(lmp) + next);
			map_object(lop, lmp);
			next = lop->lo_next;
		}
	}

	if (! tracing)
		return;

	for (lmp = link_map_head; lmp; lmp = lmp->lm_next) {
		struct link_object	*lop;
		char			*name, *path;

		if ((lop = lmp->lm_lop) == NULL)
			continue;

		name = lop->lo_name + LM_LDBASE(LM_PARENT(lmp));

		if ((path = lmp->lm_name) == NULL)
			path = "not found";

		if (lop->lo_library)
			printf("\t-l%s.%d => %s\n", name, lop->lo_major, path);
		else
			printf("\t%s => %s\n", name, path);
	}

	_exit(0);
}

/*
 * Allocate a new link map for an shared object NAME loaded at ADDR as a
 * result of the presence of link object LOP in the link map PARENT.
 */
static void
alloc_link_map(name, lop, parent, addr, dp)
char			*name;
struct link_map		*parent;
struct link_object	*lop;
caddr_t			addr;
struct link_dynamic	*dp;
{
	struct link_map		*lmp;
	struct lm_private	*lmpp;

	lmpp = (struct lm_private *)xmalloc(sizeof(struct lm_private));
	lmp = (struct link_map *)xmalloc(sizeof(struct link_map));
	lmp->lm_next = NULL;
	*link_map_tail = lmp;
	link_map_tail = &lmp->lm_next;

	lmp->lm_addr = addr;
	lmp->lm_name = name;
	lmp->lm_lop = lop;
	lmp->lm_ld = dp;
	lmp->lm_lpd = (caddr_t)lmpp;

/*XXX*/	if (addr == 0) main_map = lmp;

	lmpp->lpd_parent = parent;

#ifdef SUN_COMPAT
	lmpp->lpd_offset =
		(addr == 0 && dp->ld_version == LD_VERSION_SUN) ? PAGSIZ : 0;
#endif
}

/*
 * Map object identified by link object LOP which was found
 * in link map LMP.
 */
static void
map_object(lop, lmp)
struct link_object	*lop;
struct link_map		*lmp;
{
	struct link_dynamic	*dp;
	char		*path, *name = (char *)(lop->lo_name + LM_LDBASE(lmp));
	int		fd;
	caddr_t		addr;
	struct exec	hdr;
	int		usehints = 0;

	if (lop->lo_library) {
		usehints = 1;
again:
		path = rtfindlib(name, lop->lo_major, lop->lo_minor, &usehints);
		if (path == NULL)
			fatal("Cannot find lib%s.so.%d.%d\n",
					name, lop->lo_major, lop->lo_minor);
	} else {
		path = name;
	}

	fd = open(path, O_RDONLY, 0);
	if (fd == -1) {
		if (usehints) {
			usehints = 0;
			goto again;
		}
		fatal("%s not found", path);
	}

	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		fatal("%s: Cannot read exec header", path);
	}

	if (N_BADMAG(hdr))
		fatal("%s: Incorrect format", path);

	if ((addr = mmap(0, hdr.a_text + hdr.a_data,
				PROT_READ|PROT_EXEC,
				MAP_FILE|MAP_COPY, fd, 0)) == (caddr_t)-1)
		fatal("Cannot map %s text\n", path);

	if (mmap(addr + hdr.a_text, hdr.a_data,
				PROT_READ|PROT_WRITE|PROT_EXEC,
				MAP_FILE|MAP_FIXED|MAP_COPY,
				fd, hdr.a_text) == (caddr_t)-1)
		fatal("Cannot map %s data", path);

	close(fd);

	fd = -1;
#ifdef NEED_DEV_ZERO
	if ((fd = open("/dev/zero", O_RDWR, 0)) == -1)
		perror("/dev/zero");
#endif
	if (hdr.a_bss && mmap(addr + hdr.a_text + hdr.a_data, hdr.a_bss,
				PROT_READ|PROT_WRITE|PROT_EXEC,
				MAP_ANON|MAP_FIXED|MAP_COPY,
				fd, hdr.a_text + hdr.a_data) == (caddr_t)-1)
		fatal("Cannot map %s bss", path);

#ifdef NEED_DEV_ZERO
	close(fd);
#endif

	/* Assume _DYNAMIC is the first data item */
	dp = (struct link_dynamic *)(addr+hdr.a_text);

	/* Fixup __DYNAMIC structure */
	(long)dp->ld_un.ld_2 += (long)addr;

	alloc_link_map(path, lop, lmp, addr, dp);

}

static void
reloc_maps()
{
	struct link_map		*lmp;

	for (lmp = link_map_head; lmp; lmp = lmp->lm_next) {

		struct link_dynamic	*dp = lmp->lm_ld;
		struct relocation_info	*r = LM_REL(lmp);
		struct relocation_info	*rend = r + LD_RELSZ(dp)/sizeof(*r);

		if (LD_PLTSZ(dp))
			md_fix_jmpslot(LM_PLT(lmp),
					(long)LM_PLT(lmp), (long)binder_entry);

		for (; r < rend; r++) {
			char	*sym;
			caddr_t	addr = lmp->lm_addr + r->r_address;

			check_text_reloc(r, lmp, addr);

			if (RELOC_EXTERN_P(r)) {
				struct link_map	*src_map;
				struct nzlist	*np;
				long	relocation = md_get_addend(r, addr);

				if (RELOC_LAZY_P(r))
					continue;

				sym = LM_STRINGS(lmp) +
					LM_SYMBOL(lmp,RELOC_SYMBOL(r))->nz_strx;

				np = lookup(sym, &src_map);
				if (np == NULL)
					fatal("Undefined symbol in %s: %s\n",
							lmp->lm_name, sym);

				/*
				 * Found symbol definition.
				 * If it's in a link map, adjust value
				 * according to the load address of that map.
				 * Otherwise it's a run-time allocated common
				 * whose value is already up-to-date.
				 */
				relocation += np->nz_value;
				if (src_map)
					relocation += (long)src_map->lm_addr;

				if (RELOC_PCREL_P(r))
					relocation -= (long)lmp->lm_addr;

				if (RELOC_COPY_P(r) && src_map) {
#if DEBUG
xprintf("RELOCATE(%s) copy: from %s at %#x(%#x+%#x) to %s at %#x, reloc = %#x, size %d\n",
lmp->lm_name, src_map->lm_name, src_map->lm_addr + np->nz_value,
src_map->lm_addr, np->nz_value, sym, addr, relocation, np->nz_size);
#endif
					(void)enter_rts(sym,
						(long)addr,
						N_DATA + N_EXT,
						src_map->lm_addr + np->nz_value,
						np->nz_size);
					continue;
				}
#if DEBUG
xprintf("RELOCATE(%s) external: %s at %#x, reloc = %#x\n", lmp->lm_name, sym, addr, relocation);
#endif
				md_relocate(r, relocation, addr, 0);

			} else {
#if DEBUG
xprintf("RELOCATE(%s) internal at %#x, reloc = %#x\n", lmp->lm_name, addr, md_get_rt_segment_addend(r,addr));
#endif
				md_relocate(r,
#ifdef SUN_COMPAT
					md_get_rt_segment_addend(r, addr)
#else
					md_get_addend(r, addr)
#endif
						+ (long)lmp->lm_addr, addr, 0);
			}

		}

	}
}

static void
reloc_copy()
{
	struct rt_symbol	*rtsp;

	for (rtsp = rt_symbol_head; rtsp; rtsp = rtsp->rt_next)
		if (rtsp->rt_sp->nz_type == N_DATA + N_EXT) {
#ifdef DEBUG
xprintf("reloc_copy: from %#x to %#x, size %d\n",
rtsp->rt_srcaddr, rtsp->rt_sp->nz_value, rtsp->rt_sp->nz_size);
#endif
			bcopy(rtsp->rt_srcaddr, (caddr_t)rtsp->rt_sp->nz_value,
							rtsp->rt_sp->nz_size);
		}
}

static void
check_text_reloc(r, lmp, addr)
struct relocation_info	*r;
struct link_map		*lmp;
caddr_t			addr;
{
	char	*sym;

	if (addr >= LM_ETEXT(lmp))
		return;

	if (RELOC_EXTERN_P(r))
		sym = LM_STRINGS(lmp) +
				LM_SYMBOL(lmp, RELOC_SYMBOL(r))->nz_strx;
	else
		sym = "";

#ifdef DEBUG
	fprintf(stderr, "ld.so: warning: non pure code in %s at %x (%s)\n",
				lmp->lm_name, r->r_address, sym);
#endif

	if (mprotect(	lmp->lm_addr + LM_TXTADDR(lmp),
			LD_TEXTSZ(lmp->lm_ld),
			PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {

		perror("mprotect"),
		fatal("Cannot enable writes to %s\n", lmp->lm_name);
	}

	lmp->lm_rwt = 1;
}

static struct nzlist *
lookup(name, src_map)
char	*name;
struct link_map	**src_map;
{
	long			common_size = 0;
	struct link_map		*lmp;
	struct rt_symbol	*rtsp;

	*src_map = NULL;

	if ((rtsp = lookup_rts(name)) != NULL)
		return rtsp->rt_sp;

	/*
	 * Search all maps for a definition of NAME
	 */
	for (lmp = link_map_head; lmp; lmp = lmp->lm_next) {
		int		buckets = LD_BUCKETS(lmp->lm_ld);
		long		hashval = 0;
		struct rrs_hash	*hp;
		char		*cp;
		struct	nzlist	*np;

		/*
		 * Compute bucket in which the symbol might be found.
		 */
		for (cp = name; *cp; cp++)
			hashval = (hashval << 1) + *cp;

		hashval = (hashval & 0x7fffffff) % buckets;

		hp = LM_HASH(lmp) + hashval;
		if (hp->rh_symbolnum == -1)
			/* Nothing in this bucket */
			continue;

		while (hp) {
			np = LM_SYMBOL(lmp, hp->rh_symbolnum);
			cp = LM_STRINGS(lmp) + np->nz_strx;
			if (strcmp(cp, name) == 0)
				break;
			if (hp->rh_next == 0)
				hp = NULL;
			else
				hp = LM_HASH(lmp) + hp->rh_next;
		}
		if (hp == NULL)
			/* Nothing in this bucket */
			continue;

		/*
		 * We have a symbol with the name we're looking for.
		 */

		if (np->nz_value == 0)
			/* It's not a definition */
			continue;

		if (np->nz_type == N_UNDF+N_EXT && np->nz_value != 0) {
			/* It's a common, note value and continue search */
			if (common_size < np->nz_value)
				common_size = np->nz_value;
			continue;
		}

		*src_map = lmp;
		return np;
	}

	if (common_size == 0)
		/* Not found */
		return NULL;

	/*
	 * It's a common, enter into run-time common symbol table.
	 */
	rtsp = enter_rts(name, (long)calloc(1, common_size),
					N_UNDF + N_EXT, 0, common_size);

#if DEBUG
xprintf("Allocating common: %s size %d at %#x\n", name, common_size, rtsp->rt_sp->nz_value);
#endif

	return rtsp->rt_sp;
}


/*
 * This routine is called from the jumptable to resolve
 * procedure calls to shared objects.
 */
long
binder(jsp)
jmpslot_t	*jsp;
{
	struct link_map	*lmp, *src_map;
	long		addr;
	char		*sym;
	struct nzlist	*np;
	int		index;

	/*
	 * Find the PLT map that contains JSP.
	 */
	for (lmp = link_map_head; lmp; lmp = lmp->lm_next) {
		if (LM_PLT(lmp) < jsp &&
			jsp < LM_PLT(lmp) + LD_PLTSZ(lmp->lm_ld)/sizeof(*jsp))
			break;
	}

	if (lmp == NULL)
		fatal("Call to binder from unknown location: %#x\n", jsp);

	index = jsp->reloc_index & JMPSLOT_RELOC_MASK;

	/* Get the local symbol this jmpslot refers to */
	sym = LM_STRINGS(lmp) +
		LM_SYMBOL(lmp,RELOC_SYMBOL(&LM_REL(lmp)[index]))->nz_strx;

	np = lookup(sym, &src_map);
	if (np == NULL)
		fatal("Undefined symbol \"%s\" called from %s at %#x", sym,
							lmp->lm_name, jsp);

	/* Fixup jmpslot so future calls transfer directly to target */
	addr = np->nz_value;
	if (src_map)
		addr += (long)src_map->lm_addr;

	md_fix_jmpslot(jsp, (long)jsp, addr);

#if DEBUG
xprintf(" BINDER: %s located at = %#x in %s\n", sym, addr, src_map->lm_name);
#endif
	return addr;
}


/*
 * Run-time common symbol table.
 */

#define RTC_TABSIZE		57
static struct rt_symbol 	*rt_symtab[RTC_TABSIZE];

/*
 * Compute hash value for run-time symbol table
 */
static int
hash_string(key)
	char *key;
{
	register char *cp;
	register int k;

	cp = key;
	k = 0;
	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	return k;
}

/*
 * Lookup KEY in the run-time common symbol table.
 */

static struct rt_symbol *
lookup_rts(key)
	char *key;
{
	register int			hashval;
	register struct rt_symbol	*rtsp;

	/* Determine which bucket.  */

	hashval = hash_string(key) % RTC_TABSIZE;

	/* Search the bucket.  */

	for (rtsp = rt_symtab[hashval]; rtsp; rtsp = rtsp->rt_link)
		if (strcmp(key, rtsp->rt_sp->nz_name) == 0)
			return rtsp;

	return NULL;
}

static struct rt_symbol *
enter_rts(name, value, type, srcaddr, size)
	char	*name;
	long	value;
	int	type;
	caddr_t	srcaddr;
	long	size;
{
	register int			hashval;
	register struct rt_symbol	*rtsp, **rpp;

	/* Determine which bucket */
	hashval = hash_string(name) % RTC_TABSIZE;

	/* Find end of bucket */
	for (rpp = &rt_symtab[hashval]; *rpp; rpp = &(*rpp)->rt_link)
		;

	/* Allocate new common symbol */
	rtsp = (struct rt_symbol *)malloc(sizeof(struct rt_symbol));
	rtsp->rt_sp = (struct nzlist *)malloc(sizeof(struct nzlist));
	rtsp->rt_sp->nz_name = strdup(name);
	rtsp->rt_sp->nz_value = value;
	rtsp->rt_sp->nz_type = type;
	rtsp->rt_sp->nz_size = size;
	rtsp->rt_srcaddr = srcaddr;
	rtsp->rt_link = NULL;

	/* Link onto linear list as well */
	rtsp->rt_next = rt_symbol_head;
	rt_symbol_head = rtsp;

	*rpp = rtsp;

	return rtsp;
}

static struct hints_header	*hheader;
static struct hints_bucket	*hbuckets;
static char			*hstrtab;

#define HINTS_VALID (hheader != NULL && hheader != (struct hints_header *)-1)

static void
maphints()
{
	caddr_t		addr;
	long		msize;
	int		fd;

	if ((fd = open(_PATH_LD_HINTS, O_RDONLY, 0)) == -1) {
		hheader = (struct hints_header *)-1;
		return;
	}

	msize = PAGSIZ;
	addr = mmap(0, msize, PROT_READ, MAP_FILE|MAP_COPY, fd, 0);

	if (addr == (caddr_t)-1) {
		hheader = (struct hints_header *)-1;
		return;
	}

	hheader = (struct hints_header *)addr;
	if (HH_BADMAG(*hheader)) {
		munmap(addr, msize);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_version != LD_HINTS_VERSION_1) {
		munmap(addr, msize);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_ehints > msize) {
		if (mmap(addr+msize, hheader->hh_ehints - msize,
				PROT_READ, MAP_FILE|MAP_COPY|MAP_FIXED,
				fd, msize) != (caddr_t)(addr+msize)) {

			munmap((caddr_t)hheader, msize);
			hheader = (struct hints_header *)-1;
			return;
		}
	}
	close(fd);

	hbuckets = (struct hints_bucket *)(addr + hheader->hh_hashtab);
	hstrtab = (char *)(addr + hheader->hh_strtab);
}

int
hinthash(cp, vmajor, vminor)
char	*cp;
int	vmajor, vminor;
{
	int	k = 0;

	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	k = (((k << 1) + (k >> 14)) ^ (vmajor*257)) & 0x3fff;
	k = (((k << 1) + (k >> 14)) ^ (vminor*167)) & 0x3fff;

	return k;
}

#undef major
#undef minor

static char *
findhint(name, major, minor, preferred_path)
char	*name;
int	major, minor;
char	*preferred_path;
{
	struct hints_bucket	*bp;

	bp = hbuckets + (hinthash(name, major, minor) % hheader->hh_nbucket);

	while (1) {
		/* Sanity check */
		if (bp->hi_namex >= hheader->hh_strtab_sz) {
			fprintf(stderr, "Bad name index: %#x\n", bp->hi_namex);
			break;
		}
		if (bp->hi_pathx >= hheader->hh_strtab_sz) {
			fprintf(stderr, "Bad path index: %#x\n", bp->hi_pathx);
			break;
		}

		if (strcmp(name, hstrtab + bp->hi_namex) == 0) {
			/* It's `name', check version numbers */
			if (bp->hi_major == major &&
				(bp->hi_ndewey < 2 || bp->hi_minor == minor)) {
					if (preferred_path == NULL ||
					    strcmp(preferred_path,
						hstrtab + bp->hi_pathx) == 0) {
						return hstrtab + bp->hi_pathx;
					}
			}
		}

		if (bp->hi_next == -1)
			break;

		/* Move on to next in bucket */
		bp = &hbuckets[bp->hi_next];
	}

	/* No hints available for name */
	return NULL;
}

static char *
rtfindlib(name, major, minor, usehints)
char	*name;
int	major, minor;
int	*usehints;
{
	char	*hint;
	char	*cp, *ld_path = getenv("LD_LIBRARY_PATH");

	if (hheader == NULL)
		maphints();

	if (!HINTS_VALID || !(*usehints)) {
		*usehints = 0;
		return (char *)findshlib(name, &major, &minor);
	}

	if (ld_path != NULL) {
		/* Prefer paths from LD_LIBRARY_PATH */
		while ((cp = strsep(&ld_path, ":")) != NULL) {

			hint = findhint(name, major, minor, cp);
			if (ld_path)
				*(ld_path-1) = ':';
			if (hint)
				return hint;
		}
	} else {
		/* No LD_LIBRARY_PATH, check default */
		hint = findhint(name, major, minor, NULL);
		if (hint)
			return hint;
	}

	/* No hints available for name */
	*usehints = 0;
	return (char *)findshlib(name, &major, &minor);
}

static int
dlopen(name, mode)
char	*name;
int	mode;
{
	xprintf("dlopen(%s, %x)\n", name, mode);
	return -1;
}

static int
dlclose(fd)
int	fd;
{
	xprintf("dlclose(%d)\n", fd);
	return -1;
}

static int
dlsym(fd, sym)
int	fd;
char	*sym;
{
	xprintf("dlsym(%d, %s)\n", fd, sym);
	return 0;
}

/*
 * Private heap functions.
 */

static caddr_t	curbrk;

static void
init_brk()
{
	struct rlimit   rlim;
	char		*cp, **cpp = environ;

	if (getrlimit(RLIMIT_STACK, &rlim) < 0) {
		xprintf("ld.so: brk: getrlimit failure\n");
		_exit(1);
	}

	/*
	 * Walk to the top of stack
	 */
	if (*cpp) {
		while (*cpp) cpp++;
		cp = *--cpp;
		while (*cp) cp++;
	} else
		cp = (char *)&cp;

	curbrk = (caddr_t)
		(((long)(cp - 1 - rlim.rlim_cur) + PAGSIZ) & ~(PAGSIZ - 1));
}

void
#if __STDC__
xprintf(char *fmt, ...)
#else
xprintf(fmt, va_alist)
char	*fmt;
#endif
{
	char buf[256];
	va_list	ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	vsprintf(buf, fmt, ap);
	(void)write(1, buf, strlen(buf));
	va_end(ap);
}

caddr_t
sbrk(incr)
int incr;
{
	int	fd = -1;
	caddr_t	oldbrk;

	if (curbrk == 0)
		init_brk();

#if DEBUG
xprintf("sbrk: incr = %#x, curbrk = %#x\n", incr, curbrk);
#endif
	if (incr == 0)
		return curbrk;

	incr = (incr + PAGSIZ - 1) & ~(PAGSIZ - 1);

#ifdef NEED_DEV_ZERO
	fd = open("/dev/zero", O_RDWR, 0);
	if (fd == -1)
		perror("/dev/zero");
#endif

	if (mmap(curbrk, incr,
			PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_FIXED|MAP_COPY, fd, 0) == (caddr_t)-1) {
		perror("Cannot map anonymous memory");
	}

#ifdef NEED_DEV_ZERO
	close(fd);
#endif

	oldbrk = curbrk;
	curbrk += incr;

	return oldbrk;
}
