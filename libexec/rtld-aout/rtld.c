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
 *	$FreeBSD$
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
#include <dlfcn.h>
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

#include <link.h>

#include "md.h"
#include "shlib.h"
#include "support.h"
#include "dynamic.h"

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
 * Structure for building a list of shared objects.
 */
struct so_list {
	struct so_map	*sol_map;	/* Link map for shared object */
	struct so_list	*sol_next;	/* Next entry in the list */
};

/*
 * Loader private data, hung off <so_map>->som_spd
 */
struct somap_private {
	int		spd_version;
	struct so_map	*spd_parent;
	struct so_list	*spd_children;
	struct so_map	*spd_prev;
	dev_t		spd_dev;
	ino_t		spd_ino;
	int		spd_refcount;
	int		spd_flags;
#define RTLD_MAIN	0x01
#define RTLD_RTLD	0x02
#define RTLD_DL		0x04
#define RTLD_INIT	0x08
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

/* Start of search paths */
#define LM_PATHS(smp)	((char *) \
	((smp)->som_addr + LM_OFFSET(smp) + LD_PATHS((smp)->som_dynamic)))

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

#ifndef RELOC_EXTERN_P
#define RELOC_EXTERN_P(s) ((s)->r_extern)
#endif

#ifndef RELOC_SYMBOL
#define RELOC_SYMBOL(s) ((s)->r_symbolnum)
#endif

#ifndef RELOC_PCREL_P
#define RELOC_PCREL_P(s) ((s)->r_pcrel)
#endif

static char		__main_progname[] = "main";
static char		*main_progname = __main_progname;
static char		us[] = "/usr/libexec/ld.so";

char			**environ;
char			*__progname;
int			errno;

static uid_t		uid, euid;
static gid_t		gid, egid;
static int		careful;
static int		anon_fd = -1;

static char		*ld_bind_now;
static char		*ld_ignore_missing_objects;
static char		*ld_library_path;
static char		*ld_preload;
static char		*ld_tracing;
static char		*ld_suppress_warnings;
static char		*ld_warn_non_pure_code;

struct so_map		*link_map_head;
struct so_map		*link_map_tail;
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
static struct so_map	*map_object __P((	char *,
						struct sod *,
						struct so_map *));
static int		map_preload __P((void));
static int		map_sods __P((struct so_map *));
static int		reloc_and_init __P((struct so_map *, int));
static void		unmap_object __P((struct so_map	*, int));
static struct so_map	*alloc_link_map __P((	char *, struct sod *,
						struct so_map *, caddr_t,
						struct _dynamic *));
static void		free_link_map __P((struct so_map *));
static inline int	check_text_reloc __P((	struct relocation_info *,
						struct so_map *,
						caddr_t));
static int		reloc_map __P((struct so_map *, int));
static void		reloc_copy __P((struct so_map *));
static void		init_object __P((struct so_map *));
static void		init_sods __P((struct so_list *));
static int		call_map __P((struct so_map *, char *));
static char		*findhint __P((char *, int, int *));
static char		*rtfindlib __P((char *, int, int));
static char		*rtfindfile __P((char *));
void			binder_entry __P((void));
long			binder __P((jmpslot_t *));
static struct nzlist	*lookup __P((char *, struct so_map **, int));
static inline struct rt_symbol	*lookup_rts __P((char *));
static struct rt_symbol	*enter_rts __P((char *, long, int, caddr_t,
						long, struct so_map *));
static void		die __P((void));
static void		generror __P((char *, ...));
static int		maphints __P((void));
static void		unmaphints __P((void));
static void		ld_trace __P((struct so_map *));
static void		rt_readenv __P((void));
static int		hinthash __P((char *, int));
int			rtld __P((int, struct crt_ldso *, struct _dynamic *));

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
	struct relocation_info	*reloc;
	struct relocation_info	*reloc_limit;	/* End+1 of relocation */
	struct so_debug		*ddp;
	struct so_map		*main_map;
	struct so_map		*smp;
	char			*add_paths;

	/* Check version */
	if (version != CRT_VERSION_BSD_2 &&
	    version != CRT_VERSION_BSD_3 &&
	    version != CRT_VERSION_BSD_4 &&
	    version != CRT_VERSION_SUN)
		return -1;

	/* Fixup __DYNAMIC structure */
	(long)dp->d_un.d_sdt += crtp->crt_ba;

	/* Relocate ourselves */
	reloc = (struct relocation_info *) (LD_REL(dp) + crtp->crt_ba);
	reloc_limit =
		(struct relocation_info *) ((char *) reloc + LD_RELSZ(dp));
	while(reloc < reloc_limit) {
		/*
		 * Objects linked with "-Bsymbolic" (in particular, ld.so
		 * itself) can end up having unused relocation entries at
		 * the end.  These can be detected by the fact that they
		 * have an address of 0.
		 */
		if(reloc->r_address == 0)	/* We're done */
		    break;
		md_relocate_simple(reloc, crtp->crt_ba,
			reloc->r_address + crtp->crt_ba);
		++reloc;
	}

	if (version >= CRT_VERSION_BSD_4)
		__progname = crtp->crt_ldso;
	if (version >= CRT_VERSION_BSD_3)
		main_progname = crtp->crt_prog;

	/* Some buggy versions of crt0.o have crt_ldso filled in as NULL. */
	if (__progname == NULL)
		__progname = us;

	/* Fill in some fields in _DYNAMIC or crt structure */
	if (version >= CRT_VERSION_BSD_4)
		crtp->crt_ldentry = &ld_entry;		/* crt */
	else
		crtp->crt_dp->d_entry = &ld_entry;	/* _DYNAMIC */

	/* Setup out (private) environ variable */
	environ = crtp->crt_ep;

	/* Get user and group identifiers */
	uid = getuid(); euid = geteuid();
	gid = getgid(); egid = getegid();

	careful = (uid != euid) || (gid != egid);

	rt_readenv();

	anon_open();

	/* Make a link map entry for the main program */
	main_map = alloc_link_map(main_progname,
			     (struct sod *) NULL, (struct so_map *) NULL,
			     (caddr_t) 0, crtp->crt_dp);
	LM_PRIVATE(main_map)->spd_refcount++;
	LM_PRIVATE(main_map)->spd_flags |= RTLD_MAIN;

	/* Make a link map entry for ourselves */
	smp = alloc_link_map(us,
			     (struct sod *) NULL, (struct so_map *) NULL,
                             (caddr_t) crtp->crt_ba, dp);
	LM_PRIVATE(smp)->spd_refcount++;
	LM_PRIVATE(smp)->spd_flags |= RTLD_RTLD;

	/*
	 * Setup the executable's run path
	 */
	if (version >= CRT_VERSION_BSD_4) {
		add_paths = LM_PATHS(main_map);
		if (add_paths)
			add_search_path(add_paths);
	}

	/*
	 * Setup the directory search list for findshlib.  We use only
	 * the standard search path.  Any extra directories from
	 * LD_LIBRARY_PATH are searched explicitly, in rtfindlib.
	 */
	std_search_path();

	/* Map in LD_PRELOADs before the main program's shared objects so we
	   can intercept those calls */
	if (ld_preload != NULL) {
	        if(map_preload() == -1)			/* Failed */
			die();
	}

	/* Map all the shared objects that the main program depends upon */
	if(map_sods(main_map) == -1)
		die();

	if(ld_tracing) {	/* We're done */
		ld_trace(link_map_head);
		exit(0);
	}

	crtp->crt_dp->d_un.d_sdt->sdt_loaded = link_map_head->som_next;

	/* Relocate and initialize all mapped objects */
	if(reloc_and_init(main_map, ld_bind_now != NULL) == -1)	/* Failed */
		die();

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

void
ld_trace(smp)
	struct so_map *smp;
{
	char	*fmt1, *fmt2, *fmt, *main_local;
	int	c;

	if ((main_local = getenv("LD_TRACE_LOADED_OBJECTS_PROGNAME")) == NULL)
		main_local = "";

	if ((fmt1 = getenv("LD_TRACE_LOADED_OBJECTS_FMT1")) == NULL)
		fmt1 = "\t-l%o.%m => %p (%x)\n";

	if ((fmt2 = getenv("LD_TRACE_LOADED_OBJECTS_FMT2")) == NULL)
		fmt2 = "\t%o (%x)\n";

	for (; smp; smp = smp->som_next) {
		struct sod	*sodp;
		char		*name, *path;

		if ((sodp = smp->som_sod) == NULL)
			continue;

		name = (char *)sodp->sod_name;
		if (LM_PARENT(smp))
			name += (long)LM_LDBASE(LM_PARENT(smp));

		if ((path = smp->som_path) == NULL)
			path = "not found";

		fmt = sodp->sod_library ? fmt1 : fmt2;
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
					printf("%s", main_progname);
					break;
				case 'o':
					printf("%s", name);
					break;
				case 'm':
					printf("%d", sodp->sod_major);
					break;
				case 'n':
					printf("%d", sodp->sod_minor);
					break;
				case 'p':
					printf("%s", path);
					break;
				case 'x':
					printf("%p", smp->som_addr);
					break;
				}
				break;
			}
			++fmt;
		}
	}
}

/*
 * Allocate a new link map and return a pointer to it.
 *
 * PATH is the pathname of the shared object.
 *
 * SODP is a pointer to the shared object dependency structure responsible
 * for causing the new object to be loaded.  PARENT is the shared object
 * into which SODP points.  Both can be NULL if the new object is not
 * being loaded as a result of a shared object dependency.
 *
 * ADDR is the address at which the object has been mapped.  DP is a pointer
 * to its _dynamic structure.
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

#ifdef DEBUG /* { */
	xprintf("alloc_link_map: \"%s\" at %p\n", path, addr);
#endif /* } */

        /*
         * Allocate so_map and private area with a single malloc.  Round
         * up the size of so_map so the private area is aligned.
         */
        smp_size = ((((sizeof(struct so_map)) + sizeof (void *) - 1) /
                     sizeof (void *)) * sizeof (void *));

	smp = (struct so_map *)xmalloc(smp_size +
                                        sizeof (struct somap_private));
	smpp = (struct somap_private *) (((caddr_t) smp) + smp_size);

	/* Link the new entry into the list of link maps */
	smp->som_next = NULL;
	smpp->spd_prev = link_map_tail;
	if(link_map_tail == NULL)	/* First link map entered into list */
		link_map_head = link_map_tail = smp;
	else {				/* Append to end of list */
		link_map_tail->som_next = smp;
		link_map_tail = smp;
	}

	smp->som_addr = addr;
	smp->som_path = path ? strdup(path) : NULL;
	smp->som_sod = sodp;
	smp->som_dynamic = dp;
	smp->som_spd = (caddr_t)smpp;

	smpp->spd_refcount = 0;
	smpp->spd_flags = 0;
	smpp->spd_parent = parent;
	smpp->spd_children = NULL;
	smpp->a_text = 0;
	smpp->a_data = 0;
	smpp->a_bss = 0;
#ifdef SUN_COMPAT
	smpp->spd_offset =
		(addr==0 && dp && dp->d_version==LD_VERSION_SUN) ? PAGSIZ : 0;
#endif
	return smp;
}

/*
 * Remove the specified link map entry from the list of link maps, and free
 * the associated storage.
 */
	static void
free_link_map(smp)
	struct so_map	*smp;
{
	struct somap_private	*smpp = LM_PRIVATE(smp);

#ifdef DEBUG /* { */
	xprintf("free_link_map: \"%s\"\n", smp->som_path);
#endif /* } */

	if(smpp->spd_prev == NULL)	/* Removing first entry in list */
		link_map_head = smp->som_next;
	else				/* Update link of previous entry */
		smpp->spd_prev->som_next = smp->som_next;

	if(smp->som_next == NULL)	/* Removing last entry in list */
		link_map_tail = smpp->spd_prev;
	else				/* Update back link of next entry */
		LM_PRIVATE(smp->som_next)->spd_prev = smpp->spd_prev;

	free(smp->som_path);
	free(smp);
}

/*
 * Map the shared object specified by PATH into memory, if it is not
 * already mapped.  Increment the object's reference count, and return a
 * pointer to its link map.
 *
 * As a special case, if PATH is NULL, it is taken to refer to the main
 * program.
 *
 * SODP is a pointer to the shared object dependency structure that caused
 * this object to be requested.  PARENT is a pointer to the link map of
 * the shared object containing that structure.  For a shared object not
 * being mapped as a result of a shared object dependency, these pointers
 * should be NULL.  An example of this is a shared object that is explicitly
 * loaded via dlopen().
 *
 * The return value is a pointer to the link map for the requested object.
 * If the operation failed, the return value is NULL.  In that case, an
 * error message can be retrieved by calling dlerror().
 */
	static struct so_map *
map_object(path, sodp, parent)
	char		*path;
	struct sod	*sodp;
	struct so_map	*parent;
{
	struct so_map	*smp;
	struct stat	statbuf;

	if(path == NULL)	/* Special case for the main program itself */
		smp = link_map_head;
	else {
		/*
		 * Check whether the shared object is already mapped.
		 * We check first for an exact match by pathname.  That
		 * will detect the usual case.  If no match is found by
		 * pathname, then stat the file, and check for a match by
		 * device and inode.  That will detect the less common case
		 * involving multiple links to the same library.
		 */
		for(smp = link_map_head;  smp != NULL;  smp = smp->som_next) {
			if(!(LM_PRIVATE(smp)->spd_flags & (RTLD_MAIN|RTLD_RTLD))
			&& smp->som_path != NULL
			&& strcmp(smp->som_path, path) == 0)
				break;
		}
		if(smp == NULL) {  /* Check for a match by device and inode */
			if (stat(path, &statbuf) == -1) {
				generror ("cannot stat \"%s\" : %s",
					path, strerror(errno));
				return NULL;
			}
			for (smp = link_map_head;  smp != NULL;
			     smp = smp->som_next) {
				struct somap_private *smpp = LM_PRIVATE(smp);

				if (!(smpp->spd_flags & (RTLD_MAIN | RTLD_RTLD))
				&& smpp->spd_ino == statbuf.st_ino
				&& smpp->spd_dev == statbuf.st_dev)
					break;
			}
		}
	}

	if (smp == NULL) {	/* We must map the object */
		struct _dynamic	*dp;
		int		fd;
		caddr_t		addr;
		struct exec	hdr;
		struct somap_private *smpp;

		if ((fd = open(path, O_RDONLY, 0)) == -1) {
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

		/*
		 * Map the entire address space of the object.  It is
		 * tempting to map just the text segment at first, in
		 * order to avoid having to use mprotect to change the
		 * protections of the data segment.  But that would not
		 * be correct.  Mmap might find a group of free pages
		 * large enough to hold the text segment, but not large
		 * enough for the entire object.  When we then mapped
		 * in the data and BSS segments, they would either be
		 * non-contiguous with the text segment (if we didn't
		 * specify MAP_FIXED), or they would map over some
		 * previously mapped region (if we did use MAP_FIXED).
		 * The only way we can be sure of getting a contigous
		 * region that is large enough is to map the entire
		 * region at once.
		 */
		if ((addr = mmap(0, hdr.a_text + hdr.a_data + hdr.a_bss,
			 PROT_READ|PROT_EXEC,
			 MAP_COPY, fd, 0)) == (caddr_t)-1) {
			generror ("mmap failed for \"%s\" : %s",
				  path, strerror (errno));
			(void)close(fd);
			return NULL;
		}

		(void)close(fd);

		/* Change the data segment to writable */
		if (mprotect(addr + hdr.a_text, hdr.a_data,
		    PROT_READ|PROT_WRITE|PROT_EXEC) != 0) {
			generror ("mprotect failed for \"%s\" : %s",
				  path, strerror (errno));
			(void)munmap(addr, hdr.a_text + hdr.a_data + hdr.a_bss);
			return NULL;
		}

		/* Map in pages of zeros for the BSS segment */
		if (mmap(addr + hdr.a_text + hdr.a_data, hdr.a_bss,
			 PROT_READ|PROT_WRITE|PROT_EXEC,
			 MAP_ANON|MAP_COPY|MAP_FIXED,
			 anon_fd, 0) == (caddr_t)-1) {
			generror ("mmap failed for \"%s\" : %s",
				  path, strerror (errno));
			(void)munmap(addr, hdr.a_text + hdr.a_data + hdr.a_bss);
			return NULL;
		}

		/* Assume _DYNAMIC is the first data item */
		dp = (struct _dynamic *)(addr+hdr.a_text);

		/* Fixup __DYNAMIC structure */
		(long)dp->d_un.d_sdt += (long)addr;

		smp = alloc_link_map(path, sodp, parent, addr, dp);

		/* save segment sizes for unmap. */
		smpp = LM_PRIVATE(smp);
		smpp->a_text = hdr.a_text;
		smpp->a_data = hdr.a_data;
		smpp->a_bss = hdr.a_bss;

		/*
		 * Save the device and inode, so we can detect multiple links
		 * to the same library.  Note, if we reach this point, then
		 * statbuf is guaranteed to have been filled in.
		 */
		smpp->spd_dev = statbuf.st_dev;
		smpp->spd_ino = statbuf.st_ino;
	}

	LM_PRIVATE(smp)->spd_refcount++;
	if(LM_PRIVATE(smp)->spd_refcount == 1) {  /* First use of object */
		/*
		 * Recursively map all of the shared objects that this
		 * one depends upon.
		 */
		if(map_sods(smp) == -1) {		/* Failed */
			unmap_object(smp, 0);		/* Clean up */
			return NULL;
		}
	}

	return smp;
}

/*
 * Map all the shared libraries named in the LD_PRELOAD environment
 * variable.
 *
 * Returns 0 on success, -1 on failure.  On failure, an error message can
 * be gotten via dlerror().
 */
        static int
map_preload __P((void)) {
	char    *ld_name = ld_preload;
	char	*name;

	while ((name = strsep(&ld_name, ":")) != NULL) {
		char		*path = NULL;
		struct so_map	*smp = NULL;

		if (*name != '\0') {
			path = (strchr(name, '/') != NULL) ?  strdup(name) :
				rtfindfile(name);
		}
		if (path == NULL) {
			generror("Can't find LD_PRELOAD shared"
				" library \"%s\"", name);
		} else {
			smp = map_object(path, (struct sod *) NULL,
				(struct so_map *) NULL);
			free(path);
		}
		if (ld_name != NULL)
			*(ld_name - 1) = ':';
		if (smp == NULL) {
			/*
			 * We don't bother to unmap already-loaded libraries
			 * on failure, because in that case the program is
			 * about to die anyway.
			 */
			return -1;
		}
	}
	return 0;
}

/*
 * Map all of the shared objects that a given object depends upon.  PARENT is
 * a pointer to the link map for the shared object whose dependencies are
 * to be mapped.
 *
 * Returns 0 on success.  Returns -1 on failure.  In that case, an error
 * message can be retrieved by calling dlerror().
 */
	static int
map_sods(parent)
	struct so_map	*parent;
{
	struct somap_private	*parpp = LM_PRIVATE(parent);
	struct so_list		**soltail = &parpp->spd_children;
	long			next = LD_NEED(parent->som_dynamic);

	while(next != 0) {
		struct sod	*sodp =
			(struct sod *) (LM_LDBASE(parent) + next);
		char		*name =
			(char *) (LM_LDBASE(parent) + sodp->sod_name);
		char		*path = NULL;
		struct so_map	*smp = NULL;

		if(sodp->sod_library) {
			path = rtfindlib(name, sodp->sod_major,
					 sodp->sod_minor);
			if(path == NULL && !ld_tracing) {
				generror ("Can't find shared library"
					  " \"lib%s.so.%d.%d\"", name,
					  sodp->sod_major, sodp->sod_minor);
			}
		} else {
			if(careful && name[0] != '/') {
				generror("Shared library path must start"
					 " with \"/\" for \"%s\"", name);
			} else
				path = strdup(name);
		}

		if(path != NULL) {
			smp = map_object(path, sodp, parent);
			free(path);
		}

		if(smp != NULL) {
			struct so_list	*solp = (struct so_list *)
				xmalloc(sizeof(struct so_list));
			solp->sol_map = smp;
			solp->sol_next = NULL;
			*soltail = solp;
			soltail = &solp->sol_next;
		} else if(ld_tracing) {
			/*
			 * Allocate a dummy map entry so that we will get the
			 * "not found" message.
			 */
			(void)alloc_link_map(NULL, sodp, parent, 0, 0);
		} else if (ld_ignore_missing_objects) {
			char *msg;
			/*
			 * Call __dlerror() even it we're not going to use
			 * the message, in order to clear the saved message.
			 */
			msg = __dlerror();  /* Should never be NULL */
			if (!ld_suppress_warnings)
				warnx("warning: %s", msg);
		} else  /* Give up */
			break;

		next = sodp->sod_next;
	}

	if(next != 0) {
		/*
		 * Oh drat, we have to clean up a mess.
		 *
		 * We failed to load a shared object that we depend upon.
		 * So now we have to unload any dependencies that we had
		 * already successfully loaded prior to the error.
		 *
		 * Cleaning up doesn't matter so much for the initial
		 * loading of the program, since any failure is going to
		 * terminate the program anyway.  But it is very important
		 * to clean up properly when something is being loaded
		 * via dlopen().
		 */
		struct so_list		*solp;

		while((solp = parpp->spd_children) != NULL) {
			unmap_object(solp->sol_map, 0);
			parpp->spd_children = solp->sol_next;
			free(solp);
		}

		return -1;
	}

	return 0;
}

/*
 * Relocate and initialize the tree of shared objects rooted at the given
 * link map entry.  Returns 0 on success, or -1 on failure.  On failure,
 * an error message can be retrieved via dlerror().
 */
	static int
reloc_and_init(root, bind_now)
	struct so_map	*root;
	int		bind_now;
{
	struct so_map	*smp;

	/*
	 * Relocate all newly-loaded objects.  We avoid recursion for this
	 * step by taking advantage of a few facts.  This function is called
	 * only when there are in fact some newly-loaded objects to process.
	 * Furthermore, all newly-loaded objects will have their link map
	 * entries at the end of the link map list.  And, the root of the
	 * tree of objects just loaded will have been the first to be loaded
	 * and therefore the first new object in the link map list.  Finally,
	 * we take advantage of the fact that we can relocate the newly-loaded
	 * objects in any order.
	 *
	 * All these facts conspire to let us simply loop over the tail
	 * portion of the link map list, relocating each object so
	 * encountered.
	 */
	for(smp = root;  smp != NULL;  smp = smp->som_next) {
		if(!(LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)) {
			if(reloc_map(smp, bind_now) < 0)
				return -1;
		}
	}

	/*
	 * Copy any relocated initialized data.  Again, we can just loop
	 * over the appropriate portion of the link map list.
	 */
	for(smp = root;  smp != NULL;  smp = smp->som_next) {
		if(!(LM_PRIVATE(smp)->spd_flags & RTLD_RTLD))
			reloc_copy(smp);
	}

	/*
	 * Call any object initialization routines.
	 *
	 * Here, the order is very important, and we cannot simply loop
	 * over the newly-loaded objects as we did before.  Rather, we
	 * have to initialize the tree of new objects depth-first, and
	 * process the sibling objects at each level in reverse order
	 * relative to the dependency list.
	 *
	 * Here is the reason we initialize depth-first.  If an object
	 * depends on one or more other objects, then the objects it
	 * depends on should be initialized first, before the parent
	 * object itself.  For it is possible that the parent's
	 * initialization routine will need the services provided by the
	 * objects it depends on -- and those objects had better already
	 * be initialized.
	 *
	 * We initialize the objects at each level of the tree in reverse
	 * order for a similar reason.  When an object is linked with
	 * several libraries, it is common for routines in the earlier
	 * libraries to call routines in the later libraries.  So, again,
	 * the later libraries need to be initialized first.
	 *
	 * The upshot of these rules is that we have to use recursion to
	 * get the libraries initialized in the best order.  But the
	 * recursion is never likely to be very deep.
	 */
	init_object(root);

	return 0;
}

/*
 * Remove a reference to the shared object specified by SMP.  If no
 * references remain, unmap the object and, recursively, its descendents.
 * This function also takes care of calling the finalization routines for
 * objects that are removed.
 *
 * If KEEP is true, then the actual calls to munmap() are skipped,
 * and the object is kept in memory.  That is used only for finalization,
 * from dlexit(), when the program is exiting.  There are two reasons
 * for it.  First, the program is exiting and there is no point in
 * spending the time to explicitly unmap its shared objects.  Second,
 * even after dlexit() has been called, there are still a couple of
 * calls that are made to functions in libc.  (This is really a bug
 * in crt0.)  So libc and the main program, at least, must remain
 * mapped in that situation.
 *
 * Under no reasonable circumstances should this function fail.  If
 * anything goes wrong, we consider it an internal error, and report
 * it with err().
 */
	static void
unmap_object(smp, keep)
	struct so_map	*smp;
	int		keep;
{
	struct somap_private	*smpp = LM_PRIVATE(smp);

	smpp->spd_refcount--;
	if(smpp->spd_refcount == 0) {		/* Finished with this object */
		struct so_list	*solp;

		if(smpp->spd_flags & RTLD_INIT) {	/* Was initialized */
			/*
			 * Call the object's finalization routine.  For
			 * backward compatibility, we first try to call
			 * ".fini".  If that does not exist, we call
			 * "__fini".
			 */
			if(call_map(smp, ".fini") == -1)
				call_map(smp, "__fini");
		}

		/* Recursively unreference the object's descendents */
		while((solp = smpp->spd_children) != NULL) {
			unmap_object(solp->sol_map, keep);
			smpp->spd_children = solp->sol_next;
			free(solp);
		}

		if(!keep) {	/* Unmap the object from memory */
			if(munmap(smp->som_addr,
			smpp->a_text + smpp->a_data + smpp->a_bss) < 0)
				err(1, "internal error 1: munmap failed");

			/* Unlink and free the object's link map entry */
			free_link_map(smp);
		}
	}
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

	if (!ld_suppress_warnings && ld_warn_non_pure_code)
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
reloc_map(smp, bind_now)
	struct so_map		*smp;
	int			bind_now;
{
	/*
	 * Caching structure for reducing the number of calls to
	 * lookup() during relocation.
	 *
	 * While relocating a given shared object, the dynamic linker
	 * maintains a caching vector that is directly indexed by
	 * the symbol number in the relocation entry.  The first time
	 * a given symbol is looked up, the caching vector is
	 * filled in with a pointer to the symbol table entry, and
	 * a pointer to the so_map of the shared object in which the
	 * symbol was defined.  On subsequent uses of the same symbol,
	 * that information is retrieved directly from the caching
	 * vector, without calling lookup() again.
	 *
	 * A symbol that is referenced in a relocation entry is
	 * typically referenced in many relocation entries, so this
	 * caching reduces the number of calls to lookup()
	 * dramatically.  The overall improvement in the speed of
	 * dynamic linking is also dramatic -- as much as a factor
	 * of three for programs that use many shared libaries.
	 */
	struct cacheent {
		struct nzlist *np;	/* Pointer to symbol entry */
		struct so_map *src_map;	/* Shared object that defined symbol */
	};

	struct _dynamic		*dp = smp->som_dynamic;
	struct relocation_info	*r = LM_REL(smp);
	struct relocation_info	*rend = r + LD_RELSZ(dp)/sizeof(*r);
	long			symbolbase = (long)LM_SYMBOL(smp, 0);
	char			*stringbase = LM_STRINGS(smp);
	int symsize		= LD_VERSION_NZLIST_P(dp->d_version) ?
					sizeof(struct nzlist) :
					sizeof(struct nlist);
	long			numsyms = LD_STABSZ(dp) / symsize;
	size_t			cachebytes = numsyms * sizeof(struct cacheent);
	struct cacheent		*symcache =
					(struct cacheent *) alloca(cachebytes);

	if(symcache == NULL) {
		generror("Cannot allocate symbol caching vector for %s",
			smp->som_path);
		return -1;
	}
	bzero(symcache, cachebytes);

	if (LD_PLTSZ(dp))
		md_fix_jmpslot(LM_PLT(smp),
				(long)LM_PLT(smp), (long)binder_entry);

	for (; r < rend; r++) {
		char	*sym;
		caddr_t	addr;

		/*
		 * Objects linked with "-Bsymbolic" can end up having unused
		 * relocation entries at the end.  These can be detected by
		 * the fact that they have an address of 0.
		 */
		if(r->r_address == 0)	/* Finished relocating this object */
			break;

		addr = smp->som_addr + r->r_address;
		if (check_text_reloc(r, smp, addr) < 0)
			return -1;

		if (RELOC_EXTERN_P(r)) {
			struct so_map	*src_map = NULL;
			struct nzlist	*p, *np;
			long	relocation;

			if (RELOC_JMPTAB_P(r) && !bind_now)
				continue;

			p = (struct nzlist *)
				(symbolbase + symsize * RELOC_SYMBOL(r));

			if (p->nz_type == (N_SETV + N_EXT))
				src_map = smp;

			sym = stringbase + p->nz_strx;

			/*
			 * Look up the symbol, checking the caching
			 * vector first.
			 */
			np = symcache[RELOC_SYMBOL(r)].np;
			if(np != NULL)	/* Symbol already cached */
				src_map = symcache[RELOC_SYMBOL(r)].src_map;
			else {	/* Symbol not cached yet */
				np = lookup(sym, &src_map, RELOC_JMPTAB_P(r));
				/*
				 * Record the needed information about
				 * the symbol in the caching vector,
				 * so that we won't have to call
				 * lookup the next time we encounter
				 * the symbol.
				 */
				symcache[RELOC_SYMBOL(r)].np = np;
				symcache[RELOC_SYMBOL(r)].src_map = src_map;
			}

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
			relocation = np->nz_value;
			if (src_map)
				relocation += (long)src_map->som_addr;

			if (RELOC_JMPTAB_P(r)) {
				md_bind_jmpslot(relocation, addr);
				continue;
			}

			relocation += md_get_addend(r, addr);

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
init_object(smp)
	struct so_map		*smp;
{
	struct somap_private	*smpp = LM_PRIVATE(smp);

	if(!(smpp->spd_flags & RTLD_INIT)) {	/* Not initialized yet */
		smpp->spd_flags |= RTLD_INIT;

		/* Make sure all the children are initialized */
		if(smpp->spd_children != NULL)
			init_sods(smpp->spd_children);

		if(call_map(smp, ".init") == -1)
			call_map(smp, "__init");
	}
}

	static void
init_sods(solp)
	struct so_list	*solp;
{
	/* Recursively initialize the rest of the list */
	if(solp->sol_next != NULL)
		init_sods(solp->sol_next);

	/* Initialize the first element of the list */
	init_object(solp->sol_map);
}


/*
 * Call a function in a given shared object.  SMP is the shared object, and
 * SYM is the name of the function.
 *
 * Returns 0 on success, or -1 if the symbol was not found.  Failure is not
 * necessarily an error condition, so no error message is generated.
 */
	static int
call_map(smp, sym)
	struct so_map		*smp;
	char			*sym;
{
	struct so_map		*src_map = smp;
	struct nzlist		*np;

	np = lookup(sym, &src_map, 1);
	if (np) {
		(*(void (*)())(src_map->som_addr + np->nz_value))();
		return 0;
	}

	return -1;
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

static struct hints_header	*hheader;	/* NULL means not mapped */
static struct hints_bucket	*hbuckets;
static char			*hstrtab;

/*
 * Map the hints file into memory, if it is not already mapped.  Returns
 * 0 on success, or -1 on failure.
 */
	static int
maphints __P((void))
{
	static int		hints_bad;	/* TRUE if hints are unusable */
	static int		paths_added;
	int			hfd;
	struct hints_header	hdr;
	caddr_t			addr;

	if (hheader != NULL)	/* Already mapped */
		return 0;

	if (hints_bad)		/* Known to be corrupt or unavailable */
		return -1;

	if ((hfd = open(_PATH_LD_HINTS, O_RDONLY, 0)) == -1) {
		hints_bad = 1;
		return -1;
	}

	/* Read the header and check it */

	if (read(hfd, &hdr, sizeof hdr) != sizeof hdr ||
	    HH_BADMAG(hdr) ||
	    (hdr.hh_version != LD_HINTS_VERSION_1 &&
	     hdr.hh_version != LD_HINTS_VERSION_2)) {
		close(hfd);
		hints_bad = 1;
		return -1;
	}

	/* Map the hints into memory */

	addr = mmap(0, hdr.hh_ehints, PROT_READ, MAP_SHARED, hfd, 0);
	if (addr == (caddr_t)-1) {
		close(hfd);
		hints_bad = 1;
		return -1;
	}

	close(hfd);

	hheader = (struct hints_header *)addr;
	hbuckets = (struct hints_bucket *)(addr + hheader->hh_hashtab);
	hstrtab = (char *)(addr + hheader->hh_strtab);
	/* pluck out the system ldconfig path */
	if (hheader->hh_version >= LD_HINTS_VERSION_2 && !paths_added) {
		add_search_path(hstrtab + hheader->hh_dirlist);
		paths_added = 1;
	}

	return 0;
}

/*
 * Unmap the hints file, if it is currently mapped.
 */
	static void
unmaphints()
{
	if (hheader != NULL) {
		munmap((caddr_t)hheader, hheader->hh_ehints);
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

/*
 * Search for a library in the hints generated by ldconfig.  On success,
 * returns the full pathname of the matching library.  This string is
 * always dynamically allocated on the heap.
 *
 * Returns the minor number of the matching library via the pointer
 * argument MINORP.
 *
 * Returns NULL if the library cannot be found.
 */
	static char *
findhint(name, major, minorp)
	char	*name;
	int	major;
	int	*minorp;
{
	struct hints_bucket	*bp =
		hbuckets + (hinthash(name, major) % hheader->hh_nbucket);

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

		/*
		 * We accept the current hints entry if its name matches
		 * and its major number matches.  We don't have to search
		 * for the best minor number, because that was already
		 * done by "ldconfig" when it built the hints file.
		 */
		if (strcmp(name, hstrtab + bp->hi_namex) == 0 &&
		    bp->hi_major == major) {
			struct stat s;

			if (stat(hstrtab + bp->hi_pathx, &s) == -1)
				return NULL;  /* Doesn't actually exist */
			*minorp = bp->hi_ndewey >= 2 ? bp->hi_minor : -1;
			return strdup(hstrtab + bp->hi_pathx);
		}

		if (bp->hi_next == -1)
			break;

		/* Move on to next in bucket */
		bp = &hbuckets[bp->hi_next];
	}

	/* No hints available for name */
	return NULL;
}

/*
 * Search for the given shared library.  On success, returns a string
 * containing the full pathname for the library.  This string is always
 * dynamically allocated on the heap.
 *
 * Returns NULL if the library cannot be found.
 */
	static char *
rtfindlib(name, major, minor)
	char	*name;
	int	major, minor;
{
	char	*ld_path = ld_library_path;
	char	*path = NULL;
	int	realminor = -1;

	if (ld_path != NULL) {	/* First, search the directories in ld_path */
		/*
		 * There is no point in trying to use the hints file for this.
		 */
		char	*dir;

		while (path == NULL && (dir = strsep(&ld_path, ":")) != NULL) {
			path = search_lib_dir(dir, name, &major, &realminor, 0);
			if (ld_path != NULL)
				*(ld_path - 1) = ':';
		}
	}

	if (path == NULL && maphints() == 0)	/* Search the hints file */
		path = findhint(name, major, &realminor);

	if (path == NULL)	/* Search the standard directories */
		path = findshlib(name, &major, &realminor, 0);

	if (path != NULL && realminor < minor && !ld_suppress_warnings) {
		warnx("warning: %s: minor version %d"
		      " older than expected %d, using it anyway",
		      path, realminor, minor);
	}

	return path;
}

/*
 * Search for the given shared library file.  This is similar to rtfindlib,
 * except that the argument is the actual name of the desired library file.
 * Thus there is no need to worry about version numbers.  The return value
 * is a string containing the full pathname for the library.  This string
 * is always dynamically allocated on the heap.
 *
 * Returns NULL if the library cannot be found.
 */
	static char *
rtfindfile(name)
	char	*name;
{
	char	*ld_path = ld_library_path;
	char	*path = NULL;

	if (ld_path != NULL) {	/* First, search the directories in ld_path */
		char	*dir;

		while (path == NULL && (dir = strsep(&ld_path, ":")) != NULL) {
			struct stat	sb;

			path = concat(dir, "/", name);
			if (lstat(path, &sb) == -1) {	/* Does not exist */
				free(path);
				path = NULL;
			}
			if (ld_path != NULL)
				*(ld_path - 1) = ':';
		}
	}

	/*
	 * We don't search the hints file.  It is organized around major
	 * and minor version numbers, so it is not suitable for finding
	 * a specific file name.
	 */

	if (path == NULL)	/* Search the standard directories */
		path = find_lib_file(name);

	return path;
}

/*
 * Buffer for error messages and a pointer that is set to point to the buffer
 * when a error occurs.  It acts as a last error flag, being set to NULL
 * after an error is returned.
 */
#define DLERROR_BUF_SIZE 512
static char  dlerror_buf [DLERROR_BUF_SIZE];
static char *dlerror_msg = NULL;


	static void *
__dlopen(path, mode)
	char	*path;
	int	mode;
{
	struct so_map	*old_tail = link_map_tail;
	struct so_map	*smp;
	int		bind_now = mode == RTLD_NOW;

	/*
	 * path == NULL is handled by map_object()
	 */

	anon_open();

	/* Map the object, and the objects on which it depends */
	smp = map_object(path, (struct sod *) NULL, (struct so_map *) NULL);
	if(smp == NULL)		/* Failed */
		return NULL;
	LM_PRIVATE(smp)->spd_flags |= RTLD_DL;

	/* Relocate and initialize all newly-mapped objects */
	if(link_map_tail != old_tail) {  /* We have mapped some new objects */
		if(reloc_and_init(smp, bind_now) == -1)	/* Failed */
			return NULL;
	}

	unmaphints();
	anon_close();

	return smp;
}

	static int
__dlclose(fd)
	void	*fd;
{
	struct so_map	*smp = (struct so_map *)fd;
	struct so_map	*scanp;

#ifdef DEBUG
        xprintf("dlclose(%s): refcount = %d\n", smp->som_path,
                LM_PRIVATE(smp)->spd_refcount);
#endif
	/* Check the argument for validity */
	for(scanp = link_map_head;  scanp != NULL;  scanp = scanp->som_next)
		if(scanp == smp)	/* We found the map in the list */
			break;
	if(scanp == NULL || !(LM_PRIVATE(smp)->spd_flags & RTLD_DL)) {
		generror("Invalid argument to dlclose");
		return -1;
	}

        unmap_object(smp, 0);

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
	if (smp != NULL && LM_PRIVATE(smp)->spd_flags & RTLD_DL)
		src_map = smp;

	np = lookup(sym, &src_map, 1);
	if (np == NULL)
		return NULL;

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
#ifdef DEBUG
xprintf("__dlexit called\n");
#endif

	unmap_object(link_map_head, 1);
}

/*
 * Print the current error message and exit with failure status.
 */
static void
die __P((void))
{
	char *msg;

	fprintf(stderr, "ld.so failed");
	if ((msg = __dlerror()) != NULL)
		fprintf(stderr, ": %s", msg);
	putc('\n', stderr);
	_exit(1);
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

	vsnprintf(buf, sizeof(buf), fmt, ap);
	(void)write(1, buf, strlen(buf));
	va_end(ap);
}

/*
 * rt_readenv() etc.
 *
 * Do a sweep over the environment once only, pick up what
 * looks interesting.
 *
 * This is pretty obscure, but is relatively simple.  Simply
 * look at each environment variable, if it starts with "LD_" then
 * look closer at it.  If it's in our table, set the variable
 * listed.  effectively, this is like:
 *    ld_preload = careful ? NULL : getenv("LD_PRELOAD");
 * except that the environment is scanned once only to pick up all
 * known variables, rather than scanned multiple times for each
 * variable.
 *
 * If an environment variable of interest is set to the empty string, we
 * treat it as if it were unset.
 */

#define L(n, u, v) { n, sizeof(n) - 1, u, v },
struct env_scan_tab {
	char	*name;
	int	len;
	int	unsafe;
	char	**value;
} scan_tab[] = {
	L("LD_LIBRARY_PATH=",		1, &ld_library_path)
	L("LD_PRELOAD=",		1, &ld_preload)
	L("LD_IGNORE_MISSING_OBJECTS=",	1, &ld_ignore_missing_objects)
	L("LD_TRACE_LOADED_OBJECTS=",	0, &ld_tracing)
	L("LD_BIND_NOW=",		0, &ld_bind_now)
	L("LD_SUPPRESS_WARNINGS=",	0, &ld_suppress_warnings)
	L("LD_WARN_NON_PURE_CODE=",	0, &ld_warn_non_pure_code)
	{ NULL, 0, NULL }
};
#undef L

static void
rt_readenv()
{
	char **p = environ;
	char *v;
	struct env_scan_tab *t;

	/* for each string in the environment... */
	while ((v = *p++)) {

		/* check for LD_xxx */
		if (v[0] != 'L' || v[1] != 'D' || v[2] != '_')
			continue;

		for (t = scan_tab; t->name; t++) {
			if (careful && t->unsafe)
				continue;	/* skip for set[ug]id */
			if (strncmp(t->name, v, t->len) == 0) {
				if (*(v + t->len) != '\0')	/* Not empty */
					*t->value = v + t->len;
				break;
			}
		}
	}
}
