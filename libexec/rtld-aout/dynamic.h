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
 *	$Id: dynamic.h,v 1.4 1997/05/13 10:23:46 dfr Exp $
 */

#ifndef __DYNAMIC_H__
#define __DYNAMIC_H__

#define SUN_COMPAT

#include "md.h"
#include "link.h"

#ifndef RELOC_JMPTAB_P

#define RELOC_JMPTAB_P(r)		((r)->r_jmptable)
#define RELOC_BASEREL_P(r)		((r)->r_baserel)
#define RELOC_RELATIVE_P(r)		((r)->r_relative)
#define RELOC_COPY_P(r)			((r)->r_copy)
#define RELOC_LAZY_P(r)			((r)->r_jmptable)

#define CHECK_GOT_RELOC(r)		((r)->r_pcrel)
#define RELOC_PIC_TYPE(r)		((r)->r_baserel? \
						PIC_TYPE_LARGE:PIC_TYPE_NONE)
#endif

#ifndef RELOC_INIT_SEGMENT_RELOC
#define RELOC_INIT_SEGMENT_RELOC(r)
#endif

#ifndef MAX_GOTOFF
#define MAX_GOTOFF(x)	(LONG_MAX)
#endif

#ifndef MIN_GOTOFF
#define MIN_GOTOFF(x)	(LONG_MIN)
#endif

/*
 * Internal representation of relocation types
 */
#define RELTYPE_EXTERN		1
#define RELTYPE_JMPSLOT		2
#define RELTYPE_BASEREL		4
#define RELTYPE_RELATIVE	8
#define RELTYPE_COPY		16

#define N_ISWEAK(p)		(N_BIND(p) & BIND_WEAK)

typedef struct localsymbol {
	struct nzlist		nzlist;		/* n[z]list from file */
	struct glosym		*symbol;	/* Corresponding global symbol,
						   if any */
	struct localsymbol	*next;		/* List of definitions */
	struct file_entry	*entry;		/* Backpointer to file */
	long			gotslot_offset;	/* Position in GOT, if any */
	int			symbolnum;	/* Position in output nlist */
	int			flags;
#define LS_L_SYMBOL		1	/* Local symbol starts with an `L' */
#define LS_WRITE		2	/* Symbol goes in output symtable */
#define LS_RENAME		4	/* xlat name to `<file>.<name>' */
#define LS_HASGOTSLOT		8	/* This symbol has a GOT entry */
#define LS_WARNING		16	/* Second part of a N_WARNING duo */
} localsymbol_t;

/*
 * Global symbol data is recorded in these structures, one for each global
 * symbol. They are found via hashing in 'symtab', which points to a vector
 * of buckets. Each bucket is a chain of these structures through the link
 * field.
 *
 * Rewritten version to support extra info for dynamic linking.
 */

struct glosym {
	struct glosym	*link;	/* Next symbol hash bucket. */
	char		*name;	/* Name of this symbol.  */
	long		value;	/* Value of this symbol */
	localsymbol_t	*refs;	/* Chain of local symbols from object
				   files pertaining to this global
				   symbol */
	localsymbol_t	*sorefs;/* Same for local symbols from shared
				   object files. */

	char	*warning;	/* message, from N_WARNING nlists */
	int	common_size;	/* Common size */
	int	symbolnum;	/* Symbol index in output symbol table */
	int	rrs_symbolnum;	/* Symbol index in RRS symbol table */

	localsymbol_t	*def_lsp;	/* The local symbol that gave this
					   global symbol its definition */

	char	defined;	/* Definition of this symbol */
	char	so_defined;	/* Definition of this symbol in a shared
				   object. These go into the RRS symbol table */
	u_char	undef_refs;	/* Count of number of "undefined"
				   messages printed for this symbol */
	u_char	mult_defs;	/* Same for "multiply defined" symbols */
	struct glosym	*alias;	/* For symbols of type N_INDR, this
				   points at the real symbol. */
	int	setv_count;	/* Number of elements in N_SETV symbols */
	int	size;		/* Size of this symbol (either from N_SIZE
				   symbols or a from shared object's RRS */
	int	aux;		/* Auxiliary type information conveyed in
				   the `n_other' field of nlists */

	/* The offset into one of the RRS tables, -1 if not used */
	long	jmpslot_offset;
	long	gotslot_offset;

	long			flags;

#define GS_DEFINED		0x1	/* Symbol has definition (notyetused)*/
#define GS_REFERENCED		0x2	/* Symbol is referred to by something
					   interesting */
#define GS_TRACE		0x4	/* Symbol will be traced */
#define GS_HASJMPSLOT		0x8	/*				 */
#define GS_HASGOTSLOT		0x10	/* Some state bits concerning    */
#define GS_CPYRELOCRESERVED	0x20	/* entries in GOT and PLT tables */
#define GS_CPYRELOCCLAIMED	0x40	/*				 */
#define GS_WEAK			0x80	/* Symbol is weakly defined */

};
#ifndef __symbol_defined__
#define __symbol_defined__
typedef struct glosym symbol;
#endif

/* The symbol hash table: a vector of SYMTABSIZE pointers to struct glosym. */
extern symbol *symtab[];
#define FOR_EACH_SYMBOL(i,sp) {					\
	int i;							\
	for (i = 0; i < SYMTABSIZE; i++) {				\
		register symbol *sp;				\
		for (sp = symtab[i]; sp; sp = sp->link)

#define END_EACH_SYMBOL	}}

extern symbol	*got_symbol;		/* the symbol __GLOBAL_OFFSET_TABLE_ */
extern symbol	*dynamic_symbol;	/* the symbol __DYNAMIC */

/*
 * Each input file, and each library member ("subfile") being loaded, has a
 * `file_entry' structure for it.
 *
 * For files specified by command args, these are contained in the vector which
 * `file_table' points to.
 *
 * For library members, they are dynamically allocated, and chained through the
 * `chain' field. The chain is found in the `subfiles' field of the
 * `file_entry'. The `file_entry' objects for the members have `superfile'
 * fields pointing to the one for the library.
 *
 * Rewritten version to support extra info for dynamic linking.
 */

struct file_entry {
	char	*filename;	/* Name of this file.  */
	/*
	 * Name to use for the symbol giving address of text start Usually
	 * the same as filename, but for a file spec'd with -l this is the -l
	 * switch itself rather than the filename.
	 */
	char		*local_sym_name;
	struct exec	header;	/* The file's a.out header.  */
	localsymbol_t	*symbols;	/* Symbol table of the file. */
	int		nsymbols;	/* Number of symbols in above array. */
	int		string_size;	/* Size in bytes of string table. */
	char		*strings;	/* Pointer to the string table when
					   in core, NULL otherwise */
	int		strings_offset;	/* Offset of string table,
					   (normally N_STROFF() + 4) */
	/*
	 * Next two used only if `relocatable_output' or if needed for
	 * output of undefined reference line numbers.
	 */
	struct relocation_info	*textrel;	/* Text relocations */
	int			ntextrel;	/* # of text relocations */
	struct relocation_info	*datarel;	/* Data relocations */
	int			ndatarel;	/* # of data relocations */

	/*
	 * Relation of this file's segments to the output file.
	 */
	int 	text_start_address;	/* Start of this file's text segment
					   in the output file core image. */
	int	data_start_address;	/* Start of this file's data segment
					   in the output file core image. */
	int	bss_start_address;	/* Start of this file's bss segment
					   in the output file core image. */
	struct file_entry *subfiles;	/* For a library, points to chain of
					   entries for the library members. */
	struct file_entry *superfile;	/* For library member, points to the
					   library's own entry.  */
	struct file_entry *chain;	/* For library member, points to next
					   entry for next member.  */
	int	starting_offset;	/* For a library member, offset of the
					   member within the archive. Zero for
					   files that are not library members.*/
	int	total_size;		/* Size of contents of this file,
					   if library member. */
#ifdef SUN_COMPAT
	struct file_entry *silly_archive;/* For shared libraries which have
					    a .sa companion */
#endif
	int	lib_major, lib_minor;	/* Version numbers of a shared object */

	int	flags;
#define E_IS_LIBRARY		1	/* File is a an archive */
#define E_HEADER_VALID		2	/* File's header has been read */
#define E_SEARCH_DIRS		4	/* Search directories for file */
#define E_SEARCH_DYNAMIC	8	/* Search for shared libs allowed */
#define E_JUST_SYMS		0x10	/* File is used for incremental load */
#define E_DYNAMIC		0x20	/* File is a shared object */
#define E_SCRAPPED		0x40	/* Ignore this file */
#define E_SYMBOLS_USED		0x80	/* Symbols from this entry were used */
#define E_SECONDCLASS		0x100	/* Shared object is a subsidiary */
};

/*
 * Runtime Relocation Section (RRS).
 * This describes the data structures that go into the output text and data
 * segments to support the run-time linker. The RRS can be empty (plain old
 * static linking), or can just exist of GOT and PLT entries (in case of
 * statically linked PIC code).
 */
extern int		rrs_section_type;	/* What's in the RRS section */
#define RRS_NONE	0
#define RRS_PARTIAL	1
#define RRS_FULL	2
extern int		rrs_text_size;		/* Size of RRS text additions */
extern int		rrs_text_start;		/* Location of above */
extern int		rrs_data_size;		/* Size of RRS data additions */
extern int		rrs_data_start;		/* Location of above */
extern char		*rrs_search_paths;	/* `-L' RT paths */

/* Version number to put in __DYNAMIC (set by -V) */
extern int	soversion;
#ifndef DEFAULT_SOVERSION
#define DEFAULT_SOVERSION	LD_VERSION_BSD
#endif

extern int		pc_relocation;		/* Current PC reloc value */

extern int		number_of_shobjs;	/* # of shared objects linked in */

/* Current link mode */
extern int		link_mode;
#define DYNAMIC		1		/* Consider shared libraries */
#define SYMBOLIC	2		/* Force symbolic resolution */
#define FORCEARCHIVE	4		/* Force inclusion of all members
					   of archives */
#define SHAREABLE	8		/* Build a shared object */
#define SILLYARCHIVE	16		/* Process .sa companions, if any */
#define FORCEDYNAMIC	32		/* Force dynamic output even if no
					   shared libraries included */
#define WARNRRSTEXT	64		/* Warn about rrs in text */

extern FILE		*outstream;	/* Output file. */
extern struct exec	outheader;	/* Output file header. */
extern int		magic;		/* Output file magic. */
extern int		oldmagic;
extern int		relocatable_output;
extern int		pic_type;
#define PIC_TYPE_NONE	0
#define PIC_TYPE_SMALL	1
#define PIC_TYPE_LARGE	2

void	read_header __P((int, struct file_entry *));
void	read_entry_symbols __P((int, struct file_entry *));
void	read_entry_strings __P((int, struct file_entry *));
void	read_entry_relocation __P((int, struct file_entry *));
void	enter_file_symbols __P((struct file_entry *));
void	read_file_symbols __P((struct file_entry *));
int	set_element_prefixed_p __P((char *));
int	text_offset __P((struct file_entry *));
int	file_open __P((struct file_entry *));
void	each_file __P((void (*)(), void *));
void	each_full_file __P((void (*)(), void *));
unsigned long	check_each_file __P((unsigned long (*)(), void *));
void	mywrite __P((void *, int, int, FILE *));
void	padfile __P((int, FILE *));

/* In warnings.c: */
void	perror_name __P((char *));
void	perror_file __P((struct file_entry *));
void	print_symbols __P((FILE *));
char	*get_file_name __P((struct file_entry *));
void	print_file_name __P((struct file_entry *, FILE *));
void	prline_file_name __P((struct file_entry *, FILE *));
int	do_warnings __P((FILE *));

/* In etc.c: */
#include "support.h"

/* In symbol.c: */
void	symtab_init __P((int));
symbol	*getsym __P((char *)), *getsym_soft __P((char *));

/* In lib.c: */
void	search_library __P((int, struct file_entry *));
void	read_shared_object __P((int, struct file_entry *));
int	findlib __P((struct file_entry *));

/* In shlib.c: */
#include "shlib.h"

/* In rrs.c: */
void	init_rrs __P((void));
int	rrs_add_shobj __P((struct file_entry *));
void	alloc_rrs_reloc __P((struct file_entry *, symbol *));
void	alloc_rrs_segment_reloc __P((struct file_entry *, struct relocation_info  *));
void	alloc_rrs_jmpslot __P((struct file_entry *, symbol *));
void	alloc_rrs_gotslot __P((struct file_entry *, struct relocation_info  *, localsymbol_t *));
void	alloc_rrs_cpy_reloc __P((struct file_entry *, symbol *));

int	claim_rrs_reloc __P((struct file_entry *, struct relocation_info *, symbol *, long *));
long	claim_rrs_jmpslot __P((struct file_entry *, struct relocation_info *, symbol *, long));
long	claim_rrs_gotslot __P((struct file_entry *, struct relocation_info *, struct localsymbol *, long));
long	claim_rrs_internal_gotslot __P((struct file_entry *, struct relocation_info *, struct localsymbol *, long));
void	claim_rrs_cpy_reloc __P((struct file_entry *, struct relocation_info *, symbol *));
void	claim_rrs_segment_reloc __P((struct file_entry *, struct relocation_info *));
void	consider_rrs_section_lengths __P((void));
void	relocate_rrs_addresses __P((void));
void	write_rrs __P((void));

/* In <md>.c */
void	md_init_header __P((struct exec *, int, int));
long	md_get_addend __P((struct relocation_info *, unsigned char *));
void	md_relocate __P((struct relocation_info *, long, unsigned char *, int));
void	md_make_jmpslot __P((jmpslot_t *, long, long));
void	md_fix_jmpslot __P((jmpslot_t *, long, u_long));
int	md_make_reloc __P((struct relocation_info *, struct relocation_info *, int));
void	md_make_jmpreloc __P((struct relocation_info *, struct relocation_info *, int));
void	md_make_gotreloc __P((struct relocation_info *, struct relocation_info *, int));
void	md_make_copyreloc __P((struct relocation_info *, struct relocation_info *));
void	md_set_breakpoint __P((long, long *));

#ifdef NEED_SWAP
/* In xbits.c: */
void	swap_longs __P((long *, int));
void	swap_symbols __P((struct nlist *, int));
void	swap_zsymbols __P((struct nzlist *, int));
void	swap_ranlib_hdr __P((struct ranlib *, int));
void	swap__dynamic __P((struct link_dynamic *));
void	swap_section_dispatch_table __P((struct section_dispatch_table *));
void	swap_so_debug __P((struct so_debug *));
void	swapin_sod __P((struct sod *, int));
void	swapout_sod __P((struct sod *, int));
void	swapout_fshash __P((struct fshash *, int));
#endif

#endif /* __DYNAMIC_H__ */
