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
 *    derived from this software without specific prior written permission
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
 *	$Id: rtld.c,v 1.27 1995/09/27 23:17:33 nate Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>
#include <sys/mman.h>
#ifndef MAP_COPY
#define MAP_COPY	MAP_PRIVATE
#endif
#include <err.h>
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ld.h"

#ifndef MAP_ANON
#define MAP_ANON	0
#define anon_open() do {					\
	if ((anon_fd = open("/dev/zero", O_RDWR, 0)) == -1)	\
		err("open: %s", "/dev/zero");			\
} while (0)
#define anon_close() do {	\
	(void)close(anon_fd);	\
	anon_fd = -1;		\
} while (0)
#else
#define anon_open()
#define anon_close()
#endif

/*
 * Loader private data, hung off <so_map>->som_spd
 */
struct somap_private {
	int		spd_version;
	struct so_map	*spd_parent;
	int		spd_refcount;
	int		spd_flags;
#define RTLD_MAIN	1
#define RTLD_RTLD	2
#define RTLD_DL		4
#define RTLD_INIT	8
	unsigned long	a_text;    /* text size, if known     */
	unsigned long	a_data;    /* initialized data size   */
	unsigned long	a_bss;     /* uninitialized data size */

#ifdef SUN_COMPAT
	long		spd_offset;	/* Correction for Sun main programs */
#endif
};

#define LM_PRIVATE(smp)	((struct somap_private *)(smp)->som_spd)

#ifdef SUN_COMPAT
#define LM_OFFSET(smp)	(LM_PRIVATE(smp)->spd_offset)
#else
#define LM_OFFSET(smp)	(0)
#endif

/* Base address for section_dispatch_table entries */
#define LM_LDBASE(smp)	(smp->som_addr + LM_OFFSET(smp))

/* Start of text segment */
#define LM_TXTADDR(smp)	(smp->som_addr == (caddr_t)0 ? PAGSIZ : 0)

/* Start of run-time relocation_info */
#define LM_REL(smp)	((struct relocation_info *) \
	(smp->som_addr + LM_OFFSET(smp) + LD_REL((smp)->som_dynamic)))

/* Start of symbols */
#define LM_SYMBOL(smp, i)	((struct nzlist *) \
	(smp->som_addr + LM_OFFSET(smp) + LD_SYMBOL((smp)->som_dynamic) + \
		i * (LD_VERSION_NZLIST_P(smp->som_dynamic->d_version) ? \
			sizeof(struct nzlist) : sizeof(struct nlist))))

/* Start of hash table */
#define LM_HASH(smp)	((struct rrs_hash *) \
	((smp)->som_addr + LM_OFFSET(smp) + LD_HASH((smp)->som_dynamic)))

/* Start of strings */
#define LM_STRINGS(smp)	((char *) \
	((smp)->som_addr + LM_OFFSET(smp) + LD_STRINGS((smp)->som_dynamic)))

/* End of text */
#define LM_ETEXT(smp)	((char *) \
	((smp)->som_addr + LM_TXTADDR(smp) + LD_TEXTSZ((smp)->som_dynamic)))

/* Needed shared objects */
#define LM_NEED(smp)	((struct sod *) \
	((smp)->som_addr + LM_TXTADDR(smp) + LD_NEED((smp)->som_dynamic)))

/* PLT is in data segment, so don't use LM_OFFSET here */
#define LM_PLT(smp)	((jmpslot_t *) \
	((smp)->som_addr + LD_PLT((smp)->som_dynamic)))

/* Parent of link map */
#define LM_PARENT(smp)	(LM_PRIVATE(smp)->spd_parent)

char			**environ;
char			*__progname;
int			errno;

static uid_t		uid, euid;
static gid_t		gid, egid;
static int		careful;
static char		__main_progname[] = "main";
static char		*main_progname = __main_progname;
static char		us[] = "/usr/libexec/ld.so";
static int		anon_fd = -1;

struct so_map		*link_map_head, *main_map;
struct so_map		**link_map_tail = &link_map_head;
struct rt_symbol	*rt_symbol_head;

static void		*__dlopen __P((char *, int));
static int		__dlclose __P((void *));
static void		*__dlsym __P((void *, char *));
static char		*__dlerror __P((void));
static void		__dlexit __P((void));

static struct ld_entry	ld_entry = {
	__dlopen, __dlclose, __dlsym, __dlerror, __dlexit
};

       void		xprintf __P((char *, ...));
static void		load_objects __P((	struct crt_ldso *,
						struct _dynamic *));
static struct so_map	*map_object __P((struct sod *, struct so_map *));
static int		unmap_object __P((struct so_map	*));
static struct so_map	*load_object __P((struct sod *, struct so_map *,
					  int, int));
static int		unload_object __P((struct so_map	*));
static struct so_map	*alloc_link_map __P((	char *, struct sod *,
						struct so_map *, caddr_t,
						struct _dynamic *));
static inline int	check_text_reloc __P((	struct relocation_info *,
						struct so_map *,
						caddr_t));
static int		reloc_map __P((struct so_map *));
static void		reloc_copy __P((struct so_map *));
static void		init_map __P((struct so_map *, char *, int));
static void		call_map __P((struct so_map *, char *));
static char		*rtfindlib __P((char *, int, int, int *));
void			binder_entry __P((void));
long			binder __P((jmpslot_t *));
static struct nzlist	*lookup __P((char *, struct so_map **, int));
static inline struct rt_symbol	*lookup_rts __P((char *));
static struct rt_symbol	*enter_rts __P((char *, long, int, caddr_t,
						long, struct so_map *));
static void		generror __P((char *, ...));
static void		maphints __P((void));
static void		unmaphints __P((void));

static int		dl_cascade __P((struct so_map *));

static inline int
strcmp (register const char *s1, register const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == 0)
			return (0);
	return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

#include "md-static-funcs.c"

/*
 * Called from assembler stub that has set up crtp (passed from crt0)
 * and dp (our __DYNAMIC).
 */
int
rtld(version, crtp, dp)
int			version;
struct crt_ldso		*crtp;
struct _dynamic		*dp;
{
	int			n;
	int			nreloc;		/* # of ld.so relocations */
	struct relocation_info	*reloc;
	struct so_debug		*ddp;
	struct so_map		*smp;

	/* Check version */
	if (version != CRT_VERSION_BSD_2 &&
	    version != CRT_VERSION_BSD_3 &&
	    version != CRT_VERSION_SUN)
		return -1;

	/* Fixup __DYNAMIC structure */
	(long)dp->d_un.d_sdt += crtp->crt_ba;

	/* Divide by hand to avoid possible use of library division routine */
	for (nreloc = 0, n = LD_RELSZ(dp);
	     n > 0;
	     n -= sizeof(struct relocation_info) ) nreloc++;


	/* Relocate ourselves */
	for (reloc = (struct relocation_info *)(LD_REL(dp) + crtp->crt_ba);
	     nreloc;
	     nreloc--, reloc++) {

	     register long	addr = reloc->r_address + crtp->crt_ba;

	     md_relocate_simple(reloc, crtp->crt_ba, addr);
	}

	__progname = "ld.so";
	if (version >= CRT_VERSION_BSD_3)
		main_progname = crtp->crt_prog;

	/* Setup out (private) environ variable */
	environ = crtp->crt_ep;

	/* Get user and group identifiers */
	uid = getuid(); euid = geteuid();
	gid = getgid(); egid = getegid();

	careful = (uid != euid) || (gid != egid);

	if (careful) {
		unsetenv("LD_LIBRARY_PATH");
		unsetenv("LD_NOSTD_PATH");
		unsetenv("LD_PRELOAD");
	}

	/* Setup directory search */
	add_search_path(getenv("LD_LIBRARY_PATH"));
	if (getenv("LD_NOSTD_PATH") == NULL)
		std_search_path();

	anon_open();
	/* Load required objects into the process address space */
	load_objects(crtp, dp);

	/* Fill in some fields in main's __DYNAMIC structure */
	crtp->crt_dp->d_entry = &ld_entry;
	crtp->crt_dp->d_un.d_sdt->sdt_loaded = link_map_head->som_next;

	/* Relocate all loaded objects according to their RRS segments */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		if (reloc_map(smp) < 0)
			return -1;
	}

	/* Copy any relocated initialized data. */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		reloc_copy(smp);
	}

	/* Call any object initialization routines. */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		init_map(smp, ".init", 0);
	}

	ddp = crtp->crt_dp->d_debug;
	ddp->dd_cc = rt_symbol_head;
	if (ddp->dd_in_debugger) {
		caddr_t	addr = (caddr_t)((long)crtp->crt_bp & (~(PAGSIZ - 1)));

		/* Set breakpoint for the benefit of debuggers */
		if (mprotect(addr, PAGSIZ,
				PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
			err(1, "Cannot set breakpoint (%s)", main_progname);
		}
		md_set_breakpoint((long)crtp->crt_bp, (long *)&ddp->dd_bpt_shadow);
		if (mprotect(addr, PAGSIZ, PROT_READ|PROT_EXEC) == -1) {
			err(1, "Cannot re-protect breakpoint (%s)",
				main_progname);
		}

		ddp->dd_bpt_addr = crtp->crt_bp;
		if (link_map_head)
			ddp->dd_sym_loaded = 1;
	}

	/* Close the hints file */
	unmaphints();

	/* Close our file descriptor */
	(void)close(crtp->crt_ldfd);
	anon_close();
	return LDSO_VERSION_HAS_DLEXIT;
}


static void
load_objects(crtp, dp)
struct crt_ldso	*crtp;
struct _dynamic	*dp;
{
	struct so_map	*smp;
	int		tracing = (int)getenv("LD_TRACE_LOADED_OBJECTS");

	/* Handle LD_PRELOAD's here */

	/* Make an entry for the main program */
	smp = alloc_link_map(main_progname, (struct sod *)0, (struct so_map *)0,
					(caddr_t)0, crtp->crt_dp);
	LM_PRIVATE(smp)->spd_refcount++;
	LM_PRIVATE(smp)->spd_flags |= RTLD_MAIN;

	/* Make an entry for ourselves */
	smp = alloc_link_map(us, (struct sod *)0, (struct so_map *)0,
					(caddr_t)crtp->crt_ba, dp);
	LM_PRIVATE(smp)->spd_refcount++;
	LM_PRIVATE(smp)->spd_flags |= RTLD_RTLD;

	for (smp = link_map_head; smp; smp = smp->som_next) {
		struct sod	*sodp;
		long		next = 0;

		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;

		if (smp->som_dynamic)
			next = LD_NEED(smp->som_dynamic);

		while (next) {
			struct so_map	*newmap;

			sodp = (struct sod *)(LM_LDBASE(smp) + next);
			if ((newmap = map_object(sodp, smp)) == NULL) {
				if (!tracing) {
					errx(1, "%s: %s", main_progname,
					     __dlerror());
				}
				newmap = alloc_link_map(NULL, sodp, smp, 0, 0);
			}
			LM_PRIVATE(newmap)->spd_refcount++;
			next = sodp->sod_next;
		}
	}

	if (! tracing)
		return;

	for (smp = link_map_head; smp; smp = smp->som_next) {
		struct sod	*sodp;
		char		*name, *path;

		if ((sodp = smp->som_sod) == NULL)
			continue;
		name = sodp->sod_name + LM_LDBASE(LM_PARENT(smp));

		if ((path = smp->som_path) == NULL)
			path = "not found";

		if (sodp->sod_library)
			printf("\t-l%s.%d => %s (%p)\n", name,
					sodp->sod_major, path, smp->som_addr);
		else
			printf("\t%s => %s (%p)\n", name, path, smp->som_addr);
	}

	exit(0);
}

/*
 * Allocate a new link map for shared object NAME loaded at ADDR as a
 * result of the presence of link object LOP in the link map PARENT.
 */
	static struct so_map *
alloc_link_map(path, sodp, parent, addr, dp)
	char		*path;
	struct sod	*sodp;
	struct so_map	*parent;
	caddr_t		addr;
	struct _dynamic	*dp;
{
	struct so_map		*smp;
	struct somap_private	*smpp;
        size_t                   smp_size;

        /*
         * Allocate so_map and private area with a single malloc.  Round
         * up the size of so_map so the private area is aligned.
         */
        smp_size = ((((sizeof(struct so_map)) + sizeof (void *) - 1) /
                     sizeof (void *)) * sizeof (void *));

	smp = (struct so_map *)xmalloc(smp_size +
                                        sizeof (struct somap_private));
	smpp = (struct somap_private *) (((caddr_t) smp) + smp_size);
	smp->som_next = NULL;
	*link_map_tail = smp;
	link_map_tail = &smp->som_next;

	smp->som_addr = addr;
        if (path == NULL)
            smp->som_path = NULL;
        else
            smp->som_path = strdup(path);
	smp->som_sod = sodp;
	smp->som_dynamic = dp;
	smp->som_spd = (caddr_t)smpp;

/*XXX*/	if (addr == 0) main_map = smp;

	smpp->spd_refcount = 0;
	smpp->spd_flags = 0;
	smpp->spd_parent = parent;
	smpp->a_text = 0;
	smpp->a_data = 0;
	smpp->a_bss = 0;
#ifdef SUN_COMPAT
	smpp->spd_offset =
		(addr==0 && dp && dp->d_version==LD_VERSION_SUN) ? PAGSIZ : 0;
#endif
	return smp;
}

	static struct so_map *
find_object(sodp, smp)
	struct sod	*sodp;
	struct so_map	*smp;
{
	char		*path, *name = (char *)(sodp->sod_name + LM_LDBASE(smp));
	int		usehints = 0;
	struct so_map	*p;

	if (sodp->sod_library) {
		usehints = 1;
again:
		path = rtfindlib(name, sodp->sod_major,
				 sodp->sod_minor, &usehints);
		if (path == NULL) {
			generror ("Can't find shared library \"%s\"",
				  name);
			return NULL;
		}
	} else {
		if (careful && *name != '/') {
			generror ("Shared library path must start with \"/\" for \"%s\"",
				  name);
			return NULL;
		}
		path = name;
	}

	/* Check if already loaded */
	for (p = link_map_head; p; p = p->som_next)
		if (p->som_path && strcmp(p->som_path, path) == 0)
			break;

	return p;
}

/*
 * Map object identified by link object sodp which was found in link
 * map smp.  Returns a pointer to the link map for the requested object.
 *
 * On failure, it sets an error message that can be retrieved by __dlerror,
 * and returns NULL.
 */
	static struct so_map *
map_object(sodp, smp)
	struct sod	*sodp;
	struct so_map	*smp;
{
	struct _dynamic	*dp;
	char		*path, *name = (char *)(sodp->sod_name + LM_LDBASE(smp));
	int		fd;
	caddr_t		addr;
	struct exec	hdr;
	int		usehints = 0;
	struct so_map	*p;
	struct somap_private *smpp;

	if (sodp->sod_library) {
		usehints = 1;
again:
		path = rtfindlib(name, sodp->sod_major,
				 sodp->sod_minor, &usehints);
		if (path == NULL) {
			generror ("Can't find shared library"
				  " \"lib%s.so.%d.%d\"",
				  name, sodp->sod_major, sodp->sod_minor);
			return NULL;
		}
	} else {
		if (careful && *name != '/') {
			generror ("Shared library path must start with \"/\" for \"%s\"",
				  name);
			return NULL;
		}
		path = name;
	}

	/* Check if already loaded */
	for (p = link_map_head; p; p = p->som_next)
		if (p->som_path && strcmp(p->som_path, path) == 0)
			break;

	if (p != NULL)
		return p;

	if ((fd = open(path, O_RDONLY, 0)) == -1) {
		if (usehints) {
			usehints = 0;
			goto again;
		}
 		generror ("open failed for \"%s\" : %s",
			  path, strerror (errno));
		return NULL;
	}

	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
 		generror ("header read failed for \"%s\"", path);
		(void)close(fd);
		return NULL;
	}

	if (N_BADMAG(hdr)) {
 		generror ("bad magic number in \"%s\"", path);
		(void)close(fd);
		return NULL;
	}

	if ((addr = mmap(0, hdr.a_text + hdr.a_data + hdr.a_bss,
	         PROT_READ|PROT_EXEC,
	         MAP_COPY, fd, 0)) == (caddr_t)-1) {
 		generror ("mmap failed for \"%s\" : %s",
			  path, strerror (errno));
		(void)close(fd);
		return NULL;
	}

	if (mprotect(addr + hdr.a_text, hdr.a_data,
	    PROT_READ|PROT_WRITE|PROT_EXEC) != 0) {
 		generror ("mprotect failed for \"%s\" : %s",
			  path, strerror (errno));
		(void)close(fd);
		return NULL;
	}

	if (mmap(addr + hdr.a_text + hdr.a_data, hdr.a_bss,
		 PROT_READ|PROT_WRITE|PROT_EXEC,
		 MAP_ANON|MAP_COPY|MAP_FIXED,
		 anon_fd, 0) == (caddr_t)-1) {
		generror ("mmap failed for \"%s\" : %s",
			  path, strerror (errno));
		(void)close(fd);
		return NULL;
	}

	(void)close(fd);

	/* Assume _DYNAMIC is the first data item */
	dp = (struct _dynamic *)(addr+hdr.a_text);

	/* Fixup __DYNAMIC structure */
	(long)dp->d_un.d_sdt += (long)addr;

	p = alloc_link_map(path, sodp, smp, addr, dp);

        /* save segment sizes for unmap. */
        smpp = LM_PRIVATE(p);
        smpp->a_text = hdr.a_text;
        smpp->a_data = hdr.a_data;
        smpp->a_bss = hdr.a_bss;
	return p;
}

/*
 * Unmap an object that is nolonger in use.
 */
	static int
unmap_object(smp)
	struct so_map	*smp;
{
	struct so_map	        *prev, *p;
	struct somap_private	*smpp;

        /* Find the object in the list and unlink it */

        for (prev = NULL, p = link_map_head;
             p != smp;
             prev = p, p = p->som_next) continue;

        if (prev == NULL) {
            link_map_head = smp->som_next;
            if (smp->som_next == NULL)
                link_map_tail = &link_map_head;
        } else {
            prev->som_next = smp->som_next;
            if (smp->som_next == NULL)
                link_map_tail = &prev->som_next;
        }

        /* Unmap the sections we have mapped */

        smpp = LM_PRIVATE(smp);

	if (munmap(smp->som_addr, smpp->a_text + smpp->a_data) < 0) {
                generror ("munmap failed: %s", strerror (errno));
                return -1;
        }
        if (smpp->a_bss > 0) {
		if (munmap(smp->som_addr + smpp->a_text + smpp->a_data,
                           smpp->a_bss) < 0) {
			generror ("munmap failed: %s", strerror (errno));
			return -1;
                    }
        }
	if (smp->som_path) free(smp->som_path);
	free(smp);
        return 0;
}

static inline int
check_text_reloc(r, smp, addr)
struct relocation_info	*r;
struct so_map		*smp;
caddr_t			addr;
{
	char	*sym;

	if (addr >= LM_ETEXT(smp))
		return 0;

	if (RELOC_EXTERN_P(r))
		sym = LM_STRINGS(smp) +
				LM_SYMBOL(smp, RELOC_SYMBOL(r))->nz_strx;
	else
		sym = "";

	if (getenv("LD_SUPPRESS_WARNINGS") == NULL &&
	    getenv("LD_WARN_NON_PURE_CODE") != NULL)
		warnx("warning: non pure code in %s at %x (%s)",
				smp->som_path, r->r_address, sym);

	if (smp->som_write == 0 &&
		mprotect(smp->som_addr + LM_TXTADDR(smp),
				LD_TEXTSZ(smp->som_dynamic),
				PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
		generror ("mprotect failed for \"%s\" : %s",
			  smp->som_path, strerror (errno));
		return -1;
	}

	smp->som_write = 1;
	return 0;
}

static int
reloc_map(smp)
	struct so_map		*smp;
{
	struct _dynamic		*dp = smp->som_dynamic;
	struct relocation_info	*r = LM_REL(smp);
	struct relocation_info	*rend = r + LD_RELSZ(dp)/sizeof(*r);
	long			symbolbase = (long)LM_SYMBOL(smp, 0);
	char			*stringbase = LM_STRINGS(smp);
	int symsize		= LD_VERSION_NZLIST_P(dp->d_version) ?
					sizeof(struct nzlist) :
					sizeof(struct nlist);

	if (LD_PLTSZ(dp))
		md_fix_jmpslot(LM_PLT(smp),
				(long)LM_PLT(smp), (long)binder_entry);

	for (; r < rend; r++) {
		char	*sym;
		caddr_t	addr = smp->som_addr + r->r_address;

		if (check_text_reloc(r, smp, addr) < 0)
			return -1;

		if (RELOC_EXTERN_P(r)) {
			struct so_map	*src_map = NULL;
			struct nzlist	*p, *np;
			long	relocation = md_get_addend(r, addr);

			if (RELOC_LAZY_P(r))
				continue;

			p = (struct nzlist *)
				(symbolbase + symsize * RELOC_SYMBOL(r));

			if (p->nz_type == (N_SETV + N_EXT))
				src_map = smp;

			sym = stringbase + p->nz_strx;

			np = lookup(sym, &src_map, 0/*XXX-jumpslots!*/);
			if (np == NULL) {
				generror ("Undefined symbol \"%s\" in %s:%s",
					sym, main_progname, smp->som_path);
				return -1;
                        }
			/*
			 * Found symbol definition.
			 * If it's in a link map, adjust value
			 * according to the load address of that map.
			 * Otherwise it's a run-time allocated common
			 * whose value is already up-to-date.
			 */
			relocation += np->nz_value;
			if (src_map)
				relocation += (long)src_map->som_addr;

			if (RELOC_PCREL_P(r))
				relocation -= (long)smp->som_addr;

			if (RELOC_COPY_P(r) && src_map) {
				(void)enter_rts(sym,
					(long)addr,
					N_DATA + N_EXT,
					src_map->som_addr + np->nz_value,
					np->nz_size, src_map);
				continue;
			}
			md_relocate(r, relocation, addr, 0);

		} else {
			md_relocate(r,
#ifdef SUN_COMPAT
				md_get_rt_segment_addend(r, addr)
#else
				md_get_addend(r, addr)
#endif
					+ (long)smp->som_addr, addr, 0);
		}

	}

	if (smp->som_write) {
		if (mprotect(smp->som_addr + LM_TXTADDR(smp),
				LD_TEXTSZ(smp->som_dynamic),
				PROT_READ|PROT_EXEC) == -1) {
			generror ("mprotect failed for \"%s\" : %s",
			  	  smp->som_path, strerror (errno));
			return -1;
		}
		smp->som_write = 0;
	}
	return 0;
}

static void
reloc_copy(smp)
	struct so_map		*smp;
{
	struct rt_symbol	*rtsp;

	for (rtsp = rt_symbol_head; rtsp; rtsp = rtsp->rt_next)
		if ((rtsp->rt_smp == NULL || rtsp->rt_smp == smp) &&
				rtsp->rt_sp->nz_type == N_DATA + N_EXT) {
			bcopy(rtsp->rt_srcaddr, (caddr_t)rtsp->rt_sp->nz_value,
							rtsp->rt_sp->nz_size);
		}
}

static void
init_map(smp, sym, dependants)
	struct so_map		*smp;
	char			*sym;
	int			dependants;
{
	struct so_map		*src_map = smp;
	struct nzlist		*np;

	if (LM_PRIVATE(smp)->spd_flags & RTLD_INIT)
	    	return;

	LM_PRIVATE(smp)->spd_flags |= RTLD_INIT;

	if (dependants) {
	    struct sod	*sodp;
	    struct so_map	*smp2;
	    long		next;

	    next = LD_NEED(smp->som_dynamic);

	    while (next) {
		sodp = (struct sod *)(LM_LDBASE(smp) + next);
		smp2 = find_object(sodp, smp);
		if (smp2)
		    init_map(smp2, sym, dependants);
		next = sodp->sod_next;
	    }
	}

	np = lookup(sym, &src_map, 1);
	if (np)
		(*(void (*)())(src_map->som_addr + np->nz_value))();
}

static void
call_map(smp, sym)
	struct so_map		*smp;
	char			*sym;
{
	struct so_map		*src_map = smp;
	struct nzlist		*np;

	np = lookup(sym, &src_map, 1);
	if (np)
		(*(void (*)())(src_map->som_addr + np->nz_value))();
}

/*
 * Load an object with all its dependant objects, recording the type of the
 * object and optionally calling its init function.
 */
static struct so_map *
load_object(sodp, parent, type, init)
    struct sod	*sodp;
    struct so_map	*parent;
    int		type;
    int		init;
{
    struct so_map* smp;

    /*
     * Find or map the object.
     */
    smp = map_object(sodp, parent);
    if (smp == NULL) return NULL;

    /*
     * The first time the object is mapped, load it's dependant objects and
     * relocate it.
     */
    if (LM_PRIVATE(smp)->spd_refcount++ == 0) {
	struct sod	*sodp;
	struct so_map	*smp2;
	long		next;

	next = LD_NEED(smp->som_dynamic);

	/*
	 * Load dependant objects but defer initialisation until later.
	 * When all the dependants (and sub dependants, etc.) have been
	 * loaded and relocated, it is safe to call the init functions,
	 * using a recursive call to init_map.  This ensures that if init
	 * code in the dependants calls code in the parent, it will work
	 * as expected.
	 */
	while (next) {
	    sodp = (struct sod *)(LM_LDBASE(smp) + next);
	    /*
	     * Dependant objects (of both dlopen and main) don't get a
	     * specific type.
	     */
	    if ((smp2 = load_object(sodp, smp, 0, 0)) == NULL) {
#ifdef DEBUG
xprintf("ld.so: map_object failed on cascaded %s %s (%d.%d): %s\n",
	smp->sod_library ? "library" : "file", smp->sod_name,
	smp->sod_major, smp->sod_minor, strerror(errno));
#endif
		unload_object(smp);
		return NULL;
	    }
	    next = sodp->sod_next;
	}

	LM_PRIVATE(smp)->spd_flags |= type;
	if (reloc_map(smp) < 0) {
	    unload_object(smp);
	    return NULL;
	}
	reloc_copy(smp);
	if (init) {
	    init_map(smp, ".init", 1);
	}
    }

    return smp;
}

/*
 * Unload an object, recursively unloading dependant objects.
 */
static int
unload_object(smp)
    struct so_map *smp;
{
    struct so_map 	*smp2;
    struct sod		*sodp;
    long		next;

    if (--LM_PRIVATE(smp)->spd_refcount != 0)
	return -1;

    /*
     * Call destructors for the object (before unloading its dependants
     * since destructors may use them.  Only call destructors if constructors
     * have been called.
     */
    if (LM_PRIVATE(smp)->spd_flags & RTLD_INIT)
	call_map(smp, ".fini");

    /*
     * Unmap any dependant objects first.
     */
    next = LD_NEED(smp->som_dynamic);
    while (next) {
	sodp = (struct sod *)(LM_LDBASE(smp) + next);
	smp2 = find_object(sodp, smp);
	if (smp2)
	    unload_object(smp2);
	next = sodp->sod_next;
    }

    /*
     * Remove from address space.
     */
    if (unmap_object(smp) < 0)
	return -1;

    return 0;
}

/*
 * Run-time common symbol table.
 */

#define RTC_TABSIZE		57
static struct rt_symbol 	*rt_symtab[RTC_TABSIZE];

/*
 * Compute hash value for run-time symbol table
 */
	static inline int
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

	static inline struct rt_symbol *
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
enter_rts(name, value, type, srcaddr, size, smp)
	char		*name;
	long		value;
	int		type;
	caddr_t		srcaddr;
	long		size;
	struct so_map	*smp;
{
	register int			hashval;
	register struct rt_symbol	*rtsp, **rpp;

	/* Determine which bucket */
	hashval = hash_string(name) % RTC_TABSIZE;

	/* Find end of bucket */
	for (rpp = &rt_symtab[hashval]; *rpp; rpp = &(*rpp)->rt_link)
		continue;

	/* Allocate new common symbol */
	rtsp = (struct rt_symbol *)malloc(sizeof(struct rt_symbol));
	rtsp->rt_sp = (struct nzlist *)malloc(sizeof(struct nzlist));
	rtsp->rt_sp->nz_name = strdup(name);
	rtsp->rt_sp->nz_value = value;
	rtsp->rt_sp->nz_type = type;
	rtsp->rt_sp->nz_size = size;
	rtsp->rt_srcaddr = srcaddr;
	rtsp->rt_smp = smp;
	rtsp->rt_link = NULL;

	/* Link onto linear list as well */
	rtsp->rt_next = rt_symbol_head;
	rt_symbol_head = rtsp;

	*rpp = rtsp;

	return rtsp;
}


/*
 * Lookup NAME in the link maps. The link map producing a definition
 * is returned in SRC_MAP. If SRC_MAP is not NULL on entry the search is
 * confined to that map. If STRONG is set, the symbol returned must
 * have a proper type (used by binder()).
 */
	static struct nzlist *
lookup(name, src_map, strong)
	char		*name;
	struct so_map	**src_map;	/* IN/OUT */
	int		strong;
{
	long			common_size = 0;
	struct so_map		*smp;
	struct rt_symbol	*rtsp;

	if ((rtsp = lookup_rts(name)) != NULL)
		return rtsp->rt_sp;

	/*
	 * Search all maps for a definition of NAME
	 */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		int		buckets;
		long		hashval;
		struct rrs_hash	*hp;
		char		*cp;
		struct	nzlist	*np;

		/* Some local caching */
		long		symbolbase;
		struct rrs_hash	*hashbase;
		char		*stringbase;
		int		symsize;

		if (*src_map && smp != *src_map)
			continue;

		if ((buckets = LD_BUCKETS(smp->som_dynamic)) == 0)
			continue;

		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;

restart:
		/*
		 * Compute bucket in which the symbol might be found.
		 */
		for (hashval = 0, cp = name; *cp; cp++)
			hashval = (hashval << 1) + *cp;

		hashval = (hashval & 0x7fffffff) % buckets;

		hashbase = LM_HASH(smp);
		hp = hashbase + hashval;
		if (hp->rh_symbolnum == -1)
			/* Nothing in this bucket */
			continue;

		symbolbase = (long)LM_SYMBOL(smp, 0);
		stringbase = LM_STRINGS(smp);
		symsize	= LD_VERSION_NZLIST_P(smp->som_dynamic->d_version)?
				sizeof(struct nzlist) :
				sizeof(struct nlist);
		while (hp) {
			np = (struct nzlist *)
				(symbolbase + hp->rh_symbolnum * symsize);
			cp = stringbase + np->nz_strx;
			if (strcmp(cp, name) == 0)
				break;
			if (hp->rh_next == 0)
				hp = NULL;
			else
				hp = hashbase + hp->rh_next;
		}
		if (hp == NULL)
			/* Nothing in this bucket */
			continue;

		/*
		 * We have a symbol with the name we're looking for.
		 */
		if (np->nz_type == N_INDR+N_EXT) {
			/*
			 * Next symbol gives the aliased name. Restart
			 * search with new name and confine to this map.
			 */
			name = stringbase + (++np)->nz_strx;
			*src_map = smp;
			goto restart;
		}

		if (np->nz_value == 0)
			/* It's not a definition */
			continue;

		if (np->nz_type == N_UNDF+N_EXT && np->nz_value != 0) {
			if (np->nz_other == AUX_FUNC) {
				/* It's a weak function definition */
				if (strong)
					continue;
			} else {
				/* It's a common, note value and continue search */
				if (common_size < np->nz_value)
					common_size = np->nz_value;
				continue;
			}
		}

		*src_map = smp;
		return np;
	}

	if (common_size == 0)
		/* Not found */
		return NULL;

	/*
	 * It's a common, enter into run-time common symbol table.
	 */
	rtsp = enter_rts(name, (long)calloc(1, common_size),
					N_UNDF + N_EXT, 0, common_size, NULL);

#if DEBUG
	xprintf("Allocating common: %s size %d at %#x\n", name, common_size,
                rtsp->rt_sp->nz_value);
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
	struct so_map	*smp, *src_map = NULL;
	long		addr;
	char		*sym;
	struct nzlist	*np;
	int		index;

	/*
	 * Find the PLT map that contains JSP.
	 */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		if (LM_PLT(smp) < jsp &&
			jsp < LM_PLT(smp) + LD_PLTSZ(smp->som_dynamic)/sizeof(*jsp))
			break;
	}

	if (smp == NULL)
		errx(1, "Call to binder from unknown location: %#x\n", jsp);

	index = jsp->reloc_index & JMPSLOT_RELOC_MASK;

	/* Get the local symbol this jmpslot refers to */
	sym = LM_STRINGS(smp) +
		LM_SYMBOL(smp,RELOC_SYMBOL(&LM_REL(smp)[index]))->nz_strx;

	np = lookup(sym, &src_map, 1);
	if (np == NULL)
		errx(1, "Undefined symbol \"%s\" called from %s:%s at %#x",
				sym, main_progname, smp->som_path, jsp);

	/* Fixup jmpslot so future calls transfer directly to target */
	addr = np->nz_value;
	if (src_map)
		addr += (long)src_map->som_addr;

	md_fix_jmpslot(jsp, (long)jsp, addr);

#if DEBUG
        xprintf(" BINDER: %s located at = %#x in %s\n", sym, addr,
                src_map->som_path);
#endif
	return addr;
}


static int			hfd;
static long			hsize;
static struct hints_header	*hheader;
static struct hints_bucket	*hbuckets;
static char			*hstrtab;

#define HINTS_VALID (hheader != NULL && hheader != (struct hints_header *)-1)

	static void
maphints __P((void))
{
	caddr_t		addr;

	if ((hfd = open(_PATH_LD_HINTS, O_RDONLY, 0)) == -1) {
		hheader = (struct hints_header *)-1;
		return;
	}

	hsize = PAGSIZ;
	addr = mmap(0, hsize, PROT_READ, MAP_COPY, hfd, 0);

	if (addr == (caddr_t)-1) {
		close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	hheader = (struct hints_header *)addr;
	if (HH_BADMAG(*hheader)) {
		munmap(addr, hsize);
		close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_version != LD_HINTS_VERSION_1) {
		munmap(addr, hsize);
		close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_ehints > hsize) {
		if (mmap(addr+hsize, hheader->hh_ehints - hsize,
				PROT_READ, MAP_COPY|MAP_FIXED,
				hfd, hsize) != (caddr_t)(addr+hsize)) {

			munmap((caddr_t)hheader, hsize);
			close(hfd);
			hheader = (struct hints_header *)-1;
			return;
		}
	}

	hbuckets = (struct hints_bucket *)(addr + hheader->hh_hashtab);
	hstrtab = (char *)(addr + hheader->hh_strtab);
}

	static void
unmaphints()
{

	if (HINTS_VALID) {
		munmap((caddr_t)hheader, hsize);
		close(hfd);
		hheader = NULL;
	}
}

	int
hinthash(cp, vmajor)
	char	*cp;
	int	vmajor;
{
	int	k = 0;

	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	k = (((k << 1) + (k >> 14)) ^ (vmajor*257)) & 0x3fff;

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

	bp = hbuckets + (hinthash(name, major) % hheader->hh_nbucket);

	while (1) {
		/* Sanity check */
		if (bp->hi_namex >= hheader->hh_strtab_sz) {
			warnx("Bad name index: %#x\n", bp->hi_namex);
			break;
		}
		if (bp->hi_pathx >= hheader->hh_strtab_sz) {
			warnx("Bad path index: %#x\n", bp->hi_pathx);
			break;
		}

		if (strcmp(name, hstrtab + bp->hi_namex) == 0) {
			/* It's `name', check version numbers */
			if (bp->hi_major == major &&
				(bp->hi_ndewey < 2 || bp->hi_minor >= minor)) {
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
	char	*cp, *ld_path = getenv("LD_LIBRARY_PATH");
	int	realminor;

	if (hheader == NULL)
		maphints();

	if (!HINTS_VALID || !(*usehints))
		goto lose;

	if (ld_path != NULL) {
		/* Prefer paths from LD_LIBRARY_PATH */
		while ((cp = strsep(&ld_path, ":")) != NULL) {

			cp = findhint(name, major, minor, cp);
			if (ld_path)
				*(ld_path-1) = ':';
			if (cp)
				return cp;
		}
		/* Not found in hints, try directory search */
		realminor = -1;
		cp = (char *)findshlib(name, &major, &realminor, 0);
		if (cp && realminor >= minor)
			return cp;
	}

	/* No LD_LIBRARY_PATH or lib not found in there; check default */
	cp = findhint(name, major, minor, NULL);
	if (cp)
		return cp;

lose:
	/* No hints available for name */
	*usehints = 0;
	realminor = -1;
	cp = (char *)findshlib(name, &major, &realminor, 0);
	if (cp) {
		if (realminor < minor && getenv("LD_SUPPRESS_WARNINGS") == NULL)
			warnx("warning: lib%s.so.%d.%d: "
			      "minor version < %d expected, using it anyway",
			      name, major, realminor, minor);
		return cp;
	}
	generror ("Can't find shared library \"%s\"",
		  name);
	return NULL;
}

static struct somap_private dlmap_private = {
		0,
		(struct so_map *)0,
		0,
#ifdef SUN_COMPAT
		0,
#endif
};

static struct so_map dlmap = {
	(caddr_t)0,
	"internal",
	(struct so_map *)0,
	(struct sod *)0,
	(caddr_t)0,
	(u_int)0,
	(struct _dynamic *)0,
	(caddr_t)&dlmap_private
};

/*
 * Buffer for error messages and a pointer that is set to point to the buffer
 * when a error occurs.  It acts as a last error flag, being set to NULL
 * after an error is returned.
 */
#define DLERROR_BUF_SIZE 512
static char  dlerror_buf [DLERROR_BUF_SIZE];
static char *dlerror_msg = NULL;


	static void *
__dlopen(name, mode)
	char	*name;
	int	mode;
{
	struct sod	*sodp;
	struct so_map	*smp;

	/*
	 * A NULL argument returns the current set of mapped objects.
	 */
	if (name == NULL) {
		LM_PRIVATE(link_map_head)->spd_refcount++;
		return link_map_head;
        }
	if ((sodp = (struct sod *)malloc(sizeof(struct sod))) == NULL) {
		generror ("malloc failed: %s", strerror (errno));
		return NULL;
	}

	sodp->sod_name = (long)strdup(name);
	sodp->sod_library = 0;
	sodp->sod_major = sodp->sod_minor = 0;

	if ((smp = load_object(sodp, &dlmap, RTLD_DL, 1)) == NULL) {
#ifdef DEBUG
		xprintf("%s: %s\n", name, dlerror_buf);
#endif
		return NULL;
	}

	/*
	 * If this was newly loaded, call the _init() function in the 
	 * object as per manpage.
	 */
	if (LM_PRIVATE(smp)->spd_refcount == 1)
	    call_map(smp, "__init");

	return smp;
}

	static int
__dlclose(fd)
	void	*fd;
{
	struct so_map	*smp = (struct so_map *)fd;

#ifdef DEBUG
        xprintf("dlclose(%s): refcount = %d\n", smp->som_path,
                LM_PRIVATE(smp)->spd_refcount);
#endif

	if (smp == NULL) {
	    generror("NULL argument to dlclose");
	    return -1;
	}

	if (LM_PRIVATE(smp)->spd_refcount > 1) {
	    LM_PRIVATE(smp)->spd_refcount--;
	    return 0;
	}

	/*
	 * Call the function _fini() in the object as per manpage.
	 */
	call_map(smp, "__fini");

	free((void*) smp->som_sod->sod_name);
	free(smp->som_sod);
        if (unload_object(smp) < 0)
		return -1;

	return 0;
}

	static void *
__dlsym(fd, sym)
	void	*fd;
	char	*sym;
{
	struct so_map	*smp = (struct so_map *)fd, *src_map = NULL;
	struct nzlist	*np;
	long		addr;

	/*
	 * Restrict search to passed map if dlopen()ed.
	 */
	if (smp && LM_PRIVATE(smp)->spd_flags & RTLD_DL)
		src_map = smp;

	np = lookup(sym, &src_map, 1);
	if (np == NULL) {
		generror ("Symbol \"%s\" not found", sym);
		return NULL;
        }
	/* Fixup jmpslot so future calls transfer directly to target */
	addr = np->nz_value;
	if (src_map)
		addr += (long)src_map->som_addr;

	return (void *)addr;
}

	static char *
__dlerror __P((void))
{
        char *err;

        err = dlerror_msg;
        dlerror_msg = NULL;  /* Next call will return NULL */

        return err;
}

	static void
__dlexit __P((void))
{
	struct so_map *smp;

	for (smp = link_map_head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & (RTLD_RTLD|RTLD_MAIN))
			continue;
		call_map(smp, ".fini");
	}
}

/*
 * Generate an error message that can be later be retrieved via dlerror.
 */
static void
#if __STDC__
generror(char *fmt, ...)
#else
generror(fmt, va_alist)
char	*fmt;
#endif
{
	va_list	ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
        vsnprintf (dlerror_buf, DLERROR_BUF_SIZE, fmt, ap);
        dlerror_msg = dlerror_buf;

	va_end(ap);
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

