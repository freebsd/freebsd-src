/*
 * RRS section definitions.
 * Nomenclature and, more importantly, the layout of the various
 * data structures defined in this header file are borrowed from
 * Sun Microsystems' original <link.h>, so we can provide compatibility
 * with the SunOS 4.x shared library scheme.
 *
 *	$Id: link.h,v 1.2 1993/10/22 21:04:19 pk Exp $
 *		(derived from: @(#)link.h 1.6 88/08/19 SMI
 *		Copyright (c) 1987 by Sun Microsystems, Inc.)
 */

#ifndef _LINK_H_
#define _LINK_H_

/*
 * A `link_object' structure descibes a shared object that is needed
 * to complete the link edit process of the object containing it.
 * A list of such objects (chained through `lo_next') is pointed at
 * by `ld_need' in the link_dynamic_2 structure.
 */

struct link_object {
	long	lo_name;		/* name (relative to load address) */
	u_int	lo_library : 1,		/* searched for by library rules */
		lo_unused : 31;
	short	lo_major;		/* major version number */
	short	lo_minor;		/* minor version number */
	long	lo_next;		/* next one (often relative) */
};

/*
 * `link_maps' are used by the run-time link editor (ld.so) to keep
 * track of all shared objects loaded into a process' address space.
 * These structures are only used at run-time and do not occur within
 * the text or data segment of an executable or shared library.
 */
struct link_map {
	caddr_t	lm_addr;		/* address at which object mapped */
	char 	*lm_name;		/* full name of loaded object */
	struct	link_map *lm_next;	/* next object in map */
	struct	link_object *lm_lop;	/* link object that got us here */
	caddr_t lm_lob;			/* base address for said link object */
	u_int	lm_rwt : 1;		/* text is read/write */
	struct	link_dynamic *lm_ld;	/* dynamic structure */
	caddr_t	lm_lpd;			/* loader private data */
};

/*
 * Symbol description with size. This is simply an `nlist' with
 * one field (nz_size) added.
 * Used to convey size information on items in the data segment
 * of shared objects. An array of these live in the shared object's
 * text segment and is address by the `ld_symbols' field.
 */
struct nzlist {
	struct nlist	nlist;
	u_long		nz_size;
#define nz_un		nlist.n_un
#define nz_strx		nlist.n_un.n_strx
#define nz_name		nlist.n_un.n_name
#define nz_type		nlist.n_type
#define nz_value	nlist.n_value
#define nz_desc		nlist.n_desc
#define nz_other	nlist.n_other
};

/*
 * The `link_dynamic_2' structure contains offsets to various data
 * structures needed to do run-time relocation.
 */
struct link_dynamic_2 {
	struct	link_map *ld_loaded;	/* list of loaded objects */
	long	ld_need;		/* list of needed objects */
	long	ld_rules;		/* search rules for library objects */
	long	ld_got;			/* global offset table */
	long	ld_plt;			/* procedure linkage table */
	long	ld_rel;			/* relocation table */
	long	ld_hash;		/* symbol hash table */
	long	ld_symbols;		/* symbol table itself */
	long	(*ld_stab_hash)();	/* "pointer" to symbol hash function */
	long	ld_buckets;		/* number of hash buckets */
	long	ld_strings;		/* symbol strings */
	long	ld_str_sz;		/* size of symbol strings */
	long	ld_text_sz;		/* size of text area */
	long	ld_plt_sz;		/* size of procedure linkage table */
};

/*
 * RRS symbol hash table, addressed by `ld_hash' in link_dynamic_2
 * Used to quickly lookup symbols of the shared object by hashing
 * on the symbol's name. `rh_symbolnum' is the index of the symbol
 * in the shared object's symbol list (`ld_symbols'), `rh_next' is
 * the next symbol in the hash bucket (in case of collisions).
 */
struct rrs_hash {
	int	rh_symbolnum;		/* symbol number */
	int	rh_next;		/* next hash entry */
};

/*
 * `rt_symbols' is used to keep track of run-time allocated commons
 * and data items copied from shared objects.
 */
struct rt_symbol {
	struct nzlist		*rt_sp;		/* the symbol */
	struct rt_symbol	*rt_next;	/* next in linear list */
	struct rt_symbol	*rt_link;	/* next in bucket */
	caddr_t			rt_srcaddr;	/* address of "master" copy */
};

/*
 * Debugger interface structure.
 */
struct 	ld_debug {
	int	ldd_version;		/* version # of interface */
	int	ldd_in_debugger;	/* a debugger is running us */
	int	ldd_sym_loaded;		/* we loaded some symbols */
	char    *ldd_bp_addr;		/* place for ld-generated bpt */
	int	ldd_bp_inst;		/* instruction which was there */
	struct rt_symbol *ldd_cp;	/* commons we built */
};

/*
 * Entry points into ld.so - user interface to the run-time linker.
 * (see also libdl.a)
 */
struct ld_entry {
	int	(*dlopen)();
	int	(*dlclose)();
	int	(*dlsym)();
};

/*
 * This is the structure pointed at by the __DYNAMIC symbol if an
 * executable requires the attention of the run-time link editor.
 * __DYNAMIC is given the value zero if no run-time linking needs to
 * be done (it is always present in shared objects).
 * The union `ld_un' provides for different versions of the dynamic
 * linking mechanism (switched on by `ld_version'). The last version
 * used by Sun is 3. We leave some room here and go to version number
 * 8 for NetBSD, the main difference lying in the support for the
 * `nz_list' type of symbols.
 */

struct	link_dynamic {
	int	ld_version;		/* version # of this structure */
	struct 	ld_debug *ldd;
	union {
		struct link_dynamic_2 *ld_2;
	} ld_un;
	struct  ld_entry *ld_entry;
};

#define LD_VERSION_SUN		(3)
#define LD_VERSION_BSD		(8)
#define LD_VERSION_NZLIST_P(v)	((v) >= 8)

#define LD_GOT(x)	((x)->ld_un.ld_2->ld_got)
#define LD_PLT(x)	((x)->ld_un.ld_2->ld_plt)
#define LD_REL(x)	((x)->ld_un.ld_2->ld_rel)
#define LD_SYMBOL(x)	((x)->ld_un.ld_2->ld_symbols)
#define LD_HASH(x)	((x)->ld_un.ld_2->ld_hash)
#define LD_STRINGS(x)	((x)->ld_un.ld_2->ld_strings)
#define LD_NEED(x)	((x)->ld_un.ld_2->ld_need)
#define LD_BUCKETS(x)	((x)->ld_un.ld_2->ld_buckets)

#define LD_GOTSZ(x)	((x)->ld_un.ld_2->ld_plt - (x)->ld_un.ld_2->ld_got)
#define LD_RELSZ(x)	((x)->ld_un.ld_2->ld_hash - (x)->ld_un.ld_2->ld_rel)
#define LD_HASHSZ(x)	((x)->ld_un.ld_2->ld_symbols - (x)->ld_un.ld_2->ld_hash)
#define LD_STABSZ(x)	((x)->ld_un.ld_2->ld_strings - (x)->ld_un.ld_2->ld_symbols)
#define LD_PLTSZ(x)	((x)->ld_un.ld_2->ld_plt_sz)
#define LD_STRSZ(x)	((x)->ld_un.ld_2->ld_str_sz)
#define LD_TEXTSZ(x)	((x)->ld_un.ld_2->ld_text_sz)

/*
 * Interface to ld.so (see link(5))
 */
struct crt_ldso {
	int		crt_ba;		/* Base address of ld.so */
	int		crt_dzfd;	/* "/dev/zero" file decriptor (SunOS) */
	int		crt_ldfd;	/* ld.so file descriptor */
	struct link_dynamic	*crt_dp;/* Main's __DYNAMIC */
	char		**crt_ep;	/* environment strings */
	caddr_t		crt_bp;		/* Breakpoint if run from debugger */
};

/*
 * Version passed from crt0 to ld.so (1st argument to _rtld()).
 */
#define CRT_VERSION_SUN		1
#define CRT_VERSION_BSD		2


/*
 * Maximum number of recognized shared object version numbers.
 */
#define MAXDEWEY	8

/*
 * Header of the hints file.
 */
struct hints_header {
	long		hh_magic;
#define HH_MAGIC	011421044151
	long		hh_version;	/* Interface version number */
#define LD_HINTS_VERSION_1	1
	long		hh_hashtab;	/* Location of hash table */
	long		hh_nbucket;	/* Number of buckets in hashtab */
	long		hh_strtab;	/* Location of strings */
	long		hh_strtab_sz;	/* Size of strings */
	long		hh_ehints;	/* End of hints (max offset in file) */
};

#define HH_BADMAG(hdr)	((hdr).hh_magic != HH_MAGIC)

/*
 * Hash table element in hints file.
 */
struct hints_bucket {
	/* namex and pathx are indices into the string table */
	int		hi_namex;		/* Library name */
	int		hi_pathx;		/* Full path */
	int		hi_dewey[MAXDEWEY];	/* The versions */
	int		hi_ndewey;		/* Number of version numbers */
#define hi_major hi_dewey[0]
#define hi_minor hi_dewey[1]
	int		hi_next;		/* Next in this bucket */
};

#define _PATH_LD_HINTS		"/var/run/ld.so.hints"

#endif /* _LINK_H_ */

