/*
 *	$Id: ld.h,v 1.10 1994/02/13 20:41:34 jkh Exp $
 */
/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 */

#define SUN_COMPAT

#ifndef N_SIZE
#define N_SIZE		0xc
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef __P
#ifndef __STDC__
#define __P(a)	()
#else
#define __P(a)	a
#endif
#endif

/* If compiled with GNU C, use the built-in alloca */
#if defined(__GNUC__) || defined(sparc)
#define alloca __builtin_alloca
#endif

#ifdef __FreeBSD__
#define FreeBSD
#endif

#include "md.h"
#include "link.h"

/* Macro to control the number of undefined references printed */
#define MAX_UREFS_PRINTED	10

/* Align to power-of-two boundary */
#define PALIGN(x,p)	(((x) +  (u_long)(p) - 1) & (-(u_long)(p)))

/* Align to machine dependent boundary */
#define MALIGN(x)	PALIGN(x,MAX_ALIGNMENT)

/* Name this program was invoked by.  */
char	*progname;

/* System dependencies */

/* Define this to specify the default executable format.  */

#ifndef DEFAULT_MAGIC
#ifdef FreeBSD
#define DEFAULT_MAGIC QMAGIC
extern int	netzmagic;
#else
#define DEFAULT_MAGIC ZMAGIC
#endif
#endif


/*
 * Ok.  Following are the relocation information macros.  If your
 * system should not be able to use the default set (below), you must
 * define the following:

 *   relocation_info: This must be typedef'd (or #define'd) to the type
 * of structure that is stored in the relocation info section of your
 * a.out files.  Often this is defined in the a.out.h for your system.
 *
 *   RELOC_ADDRESS (rval): Offset into the current section of the
 * <whatever> to be relocated.  *Must be an lvalue*.
 *
 *   RELOC_EXTERN_P (rval):  Is this relocation entry based on an
 * external symbol (1), or was it fully resolved upon entering the
 * loader (0) in which case some combination of the value in memory
 * (if RELOC_MEMORY_ADD_P) and the extra (if RELOC_ADD_EXTRA) contains
 * what the value of the relocation actually was.  *Must be an lvalue*.
 *
 *   RELOC_TYPE (rval): If this entry was fully resolved upon
 * entering the loader, what type should it be relocated as?
 *
 *   RELOC_SYMBOL (rval): If this entry was not fully resolved upon
 * entering the loader, what is the index of it's symbol in the symbol
 * table?  *Must be a lvalue*.
 *
 *   RELOC_MEMORY_ADD_P (rval): This should return true if the final
 * relocation value output here should be added to memory, or if the
 * section of memory described should simply be set to the relocation
 * value.
 *
 *   RELOC_ADD_EXTRA (rval): (Optional) This macro, if defined, gives
 * an extra value to be added to the relocation value based on the
 * individual relocation entry.  *Must be an lvalue if defined*.
 *
 *   RELOC_PCREL_P (rval): True if the relocation value described is
 * pc relative.
 *
 *   RELOC_VALUE_RIGHTSHIFT (rval): Number of bits right to shift the
 * final relocation value before putting it where it belongs.
 *
 *   RELOC_TARGET_SIZE (rval): log to the base 2 of the number of
 * bytes of size this relocation entry describes; 1 byte == 0; 2 bytes
 * == 1; 4 bytes == 2, and etc.  This is somewhat redundant (we could
 * do everything in terms of the bit operators below), but having this
 * macro could end up producing better code on machines without fancy
 * bit twiddling.  Also, it's easier to understand/code big/little
 * endian distinctions with this macro.
 *
 *   RELOC_TARGET_BITPOS (rval): The starting bit position within the
 * object described in RELOC_TARGET_SIZE in which the relocation value
 * will go.
 *
 *   RELOC_TARGET_BITSIZE (rval): How many bits are to be replaced
 * with the bits of the relocation value.  It may be assumed by the
 * code that the relocation value will fit into this many bits.  This
 * may be larger than RELOC_TARGET_SIZE if such be useful.
 *
 *
 *		Things I haven't implemented
 *		----------------------------
 *
 *    Values for RELOC_TARGET_SIZE other than 0, 1, or 2.
 *
 *    Pc relative relocation for External references.
 *
 *
 */


/* Default macros */
#ifndef RELOC_ADDRESS

#define RELOC_ADDRESS(r)		((r)->r_address)
#define RELOC_EXTERN_P(r)		((r)->r_extern)
#define RELOC_TYPE(r)			((r)->r_symbolnum)
#define RELOC_SYMBOL(r)			((r)->r_symbolnum)
#define RELOC_MEMORY_SUB_P(r)		0
#define RELOC_MEMORY_ADD_P(r)		1
#undef RELOC_ADD_EXTRA
#define RELOC_PCREL_P(r)		((r)->r_pcrel)
#define RELOC_VALUE_RIGHTSHIFT(r)	0
#if defined(RTLD) && defined(SUN_COMPAT)
#define RELOC_TARGET_SIZE(r)		(2)	/* !!!!! Sun BUG compatible */
#else
#define RELOC_TARGET_SIZE(r)		((r)->r_length)
#endif
#define RELOC_TARGET_BITPOS(r)		0
#define RELOC_TARGET_BITSIZE(r)		32

#define RELOC_JMPTAB_P(r)		((r)->r_jmptable)
#define RELOC_BASEREL_P(r)		((r)->r_baserel)
#define RELOC_RELATIVE_P(r)		((r)->r_relative)
#define RELOC_COPY_P(r)			((r)->r_copy)
#define RELOC_LAZY_P(r)			((r)->r_jmptable)

#define CHECK_GOT_RELOC(r)		((r)->r_pcrel)

#endif

/*
 * Internal representation of relocation types
 */
#define RELTYPE_EXTERN		1
#define RELTYPE_JMPSLOT		2
#define RELTYPE_BASEREL		4
#define RELTYPE_RELATIVE	8
#define RELTYPE_COPY		16

#ifdef nounderscore
#define LPREFIX '.'
#else
#define LPREFIX 'L'
#endif

#ifndef TEXT_START
#define TEXT_START(x)		N_TXTADDR(x)
#endif

#ifndef DATA_START
#define DATA_START(x)		N_DATADDR(x)
#endif

/* If a this type of symbol is encountered, its name is a warning
   message to print each time the symbol referenced by the next symbol
   table entry is referenced.

   This feature may be used to allow backwards compatibility with
   certain functions (eg. gets) but to discourage programmers from
   their use.

   So if, for example, you wanted to have ld print a warning whenever
   the function "gets" was used in their C program, you would add the
   following to the assembler file in which gets is defined:

	.stabs "Obsolete function \"gets\" referenced",30,0,0,0
	.stabs "_gets",1,0,0,0

   These .stabs do not necessarily have to be in the same file as the
   gets function, they simply must exist somewhere in the compilation.  */

#ifndef N_WARNING
#define N_WARNING 0x1E		/* Warning message to print if symbol
				   included */
#endif				/* This is input to ld */

/* Special global symbol types understood by GNU LD.  */

/* The following type indicates the definition of a symbol as being
   an indirect reference to another symbol.  The other symbol
   appears as an undefined reference, immediately following this symbol.

   Indirection is asymmetrical.  The other symbol's value will be used
   to satisfy requests for the indirect symbol, but not vice versa.
   If the other symbol does not have a definition, libraries will
   be searched to find a definition.

   So, for example, the following two lines placed in an assembler
   input file would result in an object file which would direct gnu ld
   to resolve all references to symbol "foo" as references to symbol
   "bar".

	.stabs "_foo",11,0,0,0
	.stabs "_bar",1,0,0,0

   Note that (11 == (N_INDR | N_EXT)) and (1 == (N_UNDF | N_EXT)).  */

#ifndef N_INDR
#define N_INDR 0xa
#endif

/* The following symbols refer to set elements.  These are expected
   only in input to the loader; they should not appear in loader
   output (unless relocatable output is requested).  To be recognized
   by the loader, the input symbols must have their N_EXT bit set.
   All the N_SET[ATDB] symbols with the same name form one set.  The
   loader collects all of these elements at load time and outputs a
   vector for each name.
   Space (an array of 32 bit words) is allocated for the set in the
   data section, and the n_value field of each set element value is
   stored into one word of the array.
   The first word of the array is the length of the set (number of
   elements).  The last word of the vector is set to zero for possible
   use by incremental loaders.  The array is ordered by the linkage
   order; the first symbols which the linker encounters will be first
   in the array.

   In C syntax this looks like:

	struct set_vector {
	  unsigned int length;
	  unsigned int vector[length];
	  unsigned int always_zero;
	};

   Before being placed into the array, each element is relocated
   according to its type.  This allows the loader to create an array
   of pointers to objects automatically.  N_SETA type symbols will not
   be relocated.

   The address of the set is made into an N_SETV symbol
   whose name is the same as the name of the set.
   This symbol acts like a N_DATA global symbol
   in that it can satisfy undefined external references.

   For the purposes of determining whether or not to load in a library
   file, set element definitions are not considered "real
   definitions"; they will not cause the loading of a library
   member.

   If relocatable output is requested, none of this processing is
   done.  The symbols are simply relocated and passed through to the
   output file.

   So, for example, the following three lines of assembler code
   (whether in one file or scattered between several different ones)
   will produce a three element vector (total length is five words;
   see above), referenced by the symbol "_xyzzy", which will have the
   addresses of the routines _init1, _init2, and _init3.

   *NOTE*: If symbolic addresses are used in the n_value field of the
   defining .stabs, those symbols must be defined in the same file as
   that containing the .stabs.

	.stabs "_xyzzy",23,0,0,_init1
	.stabs "_xyzzy",23,0,0,_init2
	.stabs "_xyzzy",23,0,0,_init3

   Note that (23 == (N_SETT | N_EXT)).  */

#ifndef N_SETA
#define	N_SETA	0x14		/* Absolute set element symbol */
#endif				/* This is input to LD, in a .o file.  */

#ifndef N_SETT
#define	N_SETT	0x16		/* Text set element symbol */
#endif				/* This is input to LD, in a .o file.  */

#ifndef N_SETD
#define	N_SETD	0x18		/* Data set element symbol */
#endif				/* This is input to LD, in a .o file. */

#ifndef N_SETB
#define	N_SETB	0x1A		/* Bss set element symbol */
#endif				/* This is input to LD, in a .o file. */

/* Macros dealing with the set element symbols defined in a.out.h */
#define	SET_ELEMENT_P(x)	((x) >= N_SETA && (x) <= (N_SETB|N_EXT))
#define TYPE_OF_SET_ELEMENT(x)	((x) - N_SETA + N_ABS)

#ifndef N_SETV
#define N_SETV	0x1C		/* Pointer to set vector in data area. */
#endif				/* This is output from LD. */


#ifndef __GNU_STAB__

/* Line number for the data section.  This is to be used to describe
   the source location of a variable declaration. */
#ifndef N_DSLINE
#define N_DSLINE (N_SLINE+N_DATA-N_TEXT)
#endif

/* Line number for the bss section.  This is to be used to describe
   the source location of a variable declaration.  */
#ifndef N_BSLINE
#define N_BSLINE (N_SLINE+N_BSS-N_TEXT)
#endif

#endif /* not __GNU_STAB__ */


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
#define LS_GOTSLOTCLAIMED	8	/* This symbol has a GOT entry */
} localsymbol_t;

/* Symbol table */

/*
 * Global symbol data is recorded in these structures, one for each global
 * symbol. They are found via hashing in 'symtab', which points to a vector
 * of buckets. Each bucket is a chain of these structures through the link
 * field.
 */

typedef struct glosym {
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

	struct nlist	*def_nlist;	/* The local symbol that gave this
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

#define GS_DEFINED		1	/* Symbol has definition (notyetused)*/
#define GS_REFERENCED		2	/* Symbol is referred to by something
					   interesting */
#define GS_TRACE		4	/* Symbol will be traced */
#define GS_JMPSLOTCLAIMED	8	/*				 */
#define GS_GOTSLOTCLAIMED	0x10	/* Some state bits concerning    */
#define GS_CPYRELOCRESERVED	0x20	/* entries in GOT and PLT tables */
#define GS_CPYRELOCCLAIMED	0x40	/*				 */

} symbol;

/* Number of buckets in symbol hash table */
#define	SYMTABSIZE	1009

/* The symbol hash table: a vector of SYMTABSIZE pointers to struct glosym. */
extern symbol *symtab[];
#define FOR_EACH_SYMBOL(i,sp) {					\
	int i;							\
	for (i = 0; i < SYMTABSIZE; i++) {				\
		register symbol *sp;				\
		for (sp = symtab[i]; sp; sp = sp->link)

#define END_EACH_SYMBOL	}}

/* # of global symbols referenced and not defined.  */
extern int	undefined_global_sym_count;

/* # of undefined symbols referenced by shared objects */
extern int	undefined_shobj_sym_count;

/* # of multiply defined symbols. */
extern int	multiple_def_count;

/* # of common symbols. */
extern int	common_defined_global_count;

/* # of warning symbols encountered. */
extern int	warning_count;

/*
 * Define a linked list of strings which define symbols which should be
 * treated as set elements even though they aren't.  Any symbol with a prefix
 * matching one of these should be treated as a set element.
 * 
 * This is to make up for deficiencies in many assemblers which aren't willing
 * to pass any stabs through to the loader which they don't understand.
 */
struct string_list_element {
	char *str;
	struct string_list_element *next;
};

extern symbol	*entry_symbol;		/* the entry symbol, if any */
extern symbol	*edata_symbol;		/* the symbol _edata */
extern symbol	*etext_symbol;		/* the symbol _etext */
extern symbol	*end_symbol;		/* the symbol _end */
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
};

/*
 * Section start addresses.
 */
extern int	text_size;		/* total size of text. */
extern int	text_start;		/* start of text */
extern int	text_pad;		/* clear space between text and data */
extern int	data_size;		/* total size of data. */
extern int	data_start;		/* start of data */
extern int	data_pad;		/* part of bss segment within data */

extern int	bss_size;		/* total size of bss. */
extern int	bss_start;		/* start of bss */

extern int	text_reloc_size;	/* total size of text relocation. */
extern int	data_reloc_size;	/* total size of data relocation. */

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

extern int		outdesc;	/* Output file descriptor. */
extern struct exec	outheader;	/* Output file header. */
extern int		magic;		/* Output file magic. */
extern int		oldmagic;
extern int		relocatable_output;

/* Size of a page. */
extern int	page_size;

extern char	**search_dirs;	/* Directories to search for libraries. */
extern int	n_search_dirs;	/* Length of above. */

extern int	write_map;	/* write a load map (`-M') */

extern void	(*fatal_cleanup_hook)__P((void));

void	read_header __P((int, struct file_entry *));
void	read_entry_symbols __P((int, struct file_entry *));
void	read_entry_strings __P((int, struct file_entry *));
void	read_entry_relocation __P((int, struct file_entry *));
void	enter_file_symbols __P((struct file_entry *));
void	read_file_symbols __P((struct file_entry *));
void	mywrite __P((void *, int, int, int));

/* In warnings.c: */
void	perror_name __P((char *));
void	perror_file __P((struct file_entry *));
void	fatal_with_file __P((char *, struct file_entry *, ...));
void	print_symbols __P((FILE *));
char	*get_file_name __P((struct file_entry *));
void	print_file_name __P((struct file_entry *, FILE *));
void	prline_file_name __P((struct file_entry *, FILE *));
int	do_warnings __P((FILE *));

/* In etc.c: */
void	*xmalloc __P((int));
void	*xrealloc __P((void *, int));
void	fatal __P((char *, ...));
void	error __P((char *, ...));
void	padfile __P((int,int));
char	*concat __P((const char *, const char *, const char *));
int	parse __P((char *, char *, char *));

/* In symbol.c: */
void	symtab_init __P((int));
symbol	*getsym __P((char *)), *getsym_soft __P((char *));

/* In lib.c: */
void	search_library __P((int, struct file_entry *));
void	read_shared_object __P((int, struct file_entry *));
int	findlib __P((struct file_entry *));

/* In shlib.c: */
char	*findshlib __P((char *, int *, int *, int));
void	add_search_dir __P((char *));
void	std_search_dirs __P((char *));

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

#ifdef NEED_SWAP
void	md_swapin_exec_hdr __P((struct exec *));
void	md_swapout_exec_hdr __P((struct exec *));
void	md_swapin_reloc __P((struct relocation_info *, int));
void	md_swapout_reloc __P((struct relocation_info *, int));
void	md_swapout_jmpslot __P((jmpslot_t *, int));

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
