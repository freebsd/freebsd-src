/*	$Id: ld.h,v 1.2 1993/11/09 04:18:59 paul Exp $	*/
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

#include "md.h"
#include "link.h"

/* Macro to control the number of undefined references printed */
#define MAX_UREFS_PRINTED	10

/* Align to power-of-two boundary */
#define PALIGN(x,p)	(((x) +  (u_long)(p) - 1) & (-(u_long)(p)))

/* Align to machine dependent boundary */
#define MALIGN(x)	PALIGN(x,MAX_ALIGNMENT)

/* Size of a page; obtained from the operating system.  */

int page_size;

/* Name this program was invoked by.  */

char *progname;

/* System dependencies */

/* Define this to specify the default executable format.  */

#ifndef DEFAULT_MAGIC
#define DEFAULT_MAGIC ZMAGIC
#endif

#ifdef QMAGIC
int oldmagic;
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

/* Symbol table */

/*
 * Global symbol data is recorded in these structures, one for each global
 * symbol. They are found via hashing in 'symtab', which points to a vector
 * of buckets. Each bucket is a chain of these structures through the link
 * field.
 */

typedef struct glosym {
	/* Pointer to next symbol in this symbol's hash bucket.  */
	struct glosym  *link;
	/* Name of this symbol.  */
	char           *name;
	/* Value of this symbol as a global symbol.  */
	long            value;
	/*
	 * Chain of external 'nlist's in files for this symbol, both defs and
	 * refs.
	 */
	struct localsymbol	*refs;
	/*
	 * Any warning message that might be associated with this symbol from
	 * an N_WARNING symbol encountered.
	 */
	char           *warning;
	/*
	 * Nonzero means definitions of this symbol as common have been seen,
	 * and the value here is the largest size specified by any of them.
	 */
	int             max_common_size;
	/*
	 * For relocatable_output, records the index of this global sym in
	 * the symbol table to be written, with the first global sym given
	 * index 0.
	 */
	int             symbolnum;
	/*
	 * For dynamically linked output, records the index in the RRS
	 * symbol table.
	 */
	int             rrs_symbolnum;
	/*
	 * Nonzero means a definition of this global symbol is known to
	 * exist. Library members should not be loaded on its account.
	 */
	char            defined;
	/*
	 * Nonzero means a reference to this global symbol has been seen in a
	 * file that is surely being loaded. A value higher than 1 is the
	 * n_type code for the symbol's definition.
	 */
	char            referenced;
	/*
	 * A count of the number of undefined references printed for a
	 * specific symbol.  If a symbol is unresolved at the end of
	 * digest_symbols (and the loading run is supposed to produce
	 * relocatable output) do_file_warnings keeps track of how many
	 * unresolved reference error messages have been printed for each
	 * symbol here.  When the number hits MAX_UREFS_PRINTED, messages
	 * stop.
	 */
	unsigned char   undef_refs;
	/*
	 * 1 means that this symbol has multiple definitions.  2 means that
	 * it has multiple definitions, and some of them are set elements,
	 * one of which has been printed out already.
	 */
	unsigned char   multiply_defined;
	/* Nonzero means print a message at all refs or defs of this symbol */
	char            trace;

	/*
	 * For symbols of type N_INDR, this points at the real symbol.
	 */
	struct glosym	*alias;

	/*
	 * Count number of elements in set vector if symbol is of type N_SETV
	 */
	int		setv_count;

	/* Dynamic lib support */

	/*
	 * Nonzero means a definition of this global symbol has been found
	 * in a shared object. These symbols do not go into the symbol
	 * section of the resulting a.out file. They *do* go into the
	 * dynamic link information segment.
	 */
	char            so_defined;

	/* Size of symbol as determined by N_SIZE 'nlist's in object files */
	int             size;

	/* Auxialiary info to put in the `nz_other' field of the
	 * RRS symbol table. Used by the run-time linker to resolve
	 * references to function addresses from within shared objects.
	 */
	int		aux;
#define RRS_FUNC	2

	/*
	 * Chain of external 'nlist's in shared objects for this symbol, both
	 * defs and refs.
	 */
	struct localsymbol	*sorefs;

	/* The offset into one of the RRS tables, -1 if not used */
	long			jmpslot_offset;
	char			jmpslot_claimed;

	long			gotslot_offset;
	char			gotslot_claimed;

	char			cpyreloc_reserved;
	char			cpyreloc_claimed;

	/* The local symbol that gave this global symbol its definition */
	struct nlist		*def_nlist;
} symbol;

/* Number of buckets in symbol hash table */
#define	TABSIZE	1009

/* The symbol hash table: a vector of TABSIZE pointers to struct glosym. */
symbol *symtab[TABSIZE];
#define FOR_EACH_SYMBOL(i,sp) {					\
	int i;							\
	for (i = 0; i < TABSIZE; i++) {				\
		register symbol *sp;				\
		for (sp = symtab[i]; sp; sp = sp->link)

#define END_EACH_SYMBOL	}}

/* Number of symbols in symbol hash table. */
int num_hash_tab_syms;

/* Count the number of nlist entries that are for local symbols.
   This count and the three following counts
   are incremented as as symbols are entered in the symbol table.  */
int local_sym_count;

/* Count number of nlist entries that are for local symbols
   whose names don't start with L. */
int non_L_local_sym_count;

/* Count the number of nlist entries for debugger info.  */
int debugger_sym_count;

/* Count the number of global symbols referenced and not defined.  */
int undefined_global_sym_count;

/* Count the number of symbols referenced from shared objects and not defined */
int undefined_shobj_sym_count;

/* Count the number of global symbols multiply defined.  */
int multiple_def_count;

/* Count the number of defined global symbols.
   Each symbol is counted only once
   regardless of how many different nlist entries refer to it,
   since the output file will need only one nlist entry for it.
   This count is computed by `digest_symbols';
   it is undefined while symbols are being loaded. */
int defined_global_sym_count;

/* Count the number of symbols defined through common declarations.
   This count is kept in symdef_library, linear_library, and
   enter_global_ref.  It is incremented when the defined flag is set
   in a symbol because of a common definition, and decremented when
   the symbol is defined "for real" (ie. by something besides a common
   definition).  */
int common_defined_global_count;

/* Count the number of linker defined symbols.
   XXX - Currently, only __DYNAMIC and _G_O_T_ go here if required,
   perhaps _etext, _edata and _end should go here too */
int	special_sym_count;

/* Count number of aliased symbols */
int	global_alias_count;

/* Count number of set element type symbols and the number of separate
   vectors which these symbols will fit into */
int	set_symbol_count;
int	set_vector_count;

/* Define a linked list of strings which define symbols which should
   be treated as set elements even though they aren't.  Any symbol
   with a prefix matching one of these should be treated as a set
   element.

   This is to make up for deficiencies in many assemblers which aren't
   willing to pass any stabs through to the loader which they don't
   understand.  */
struct string_list_element {
  char *str;
  struct string_list_element *next;
};

struct string_list_element *set_element_prefixes;

/* Count the number of warning symbols encountered. */
int warning_count;

/* 1 => write load map.  */
int write_map;

/* 1 => write relocation into output file so can re-input it later.  */
int	relocatable_output;

/* Nonzero means ptr to symbol entry for symbol to use as start addr.
   -e sets this.  */
symbol	*entry_symbol;

symbol	*edata_symbol;		/* the symbol _edata */
symbol	*etext_symbol;		/* the symbol _etext */
symbol	*end_symbol;		/* the symbol _end */
symbol	*got_symbol;		/* the symbol __GLOBAL_OFFSET_TABLE_ */
symbol	*dynamic_symbol;	/* the symbol __DYNAMIC */


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
	/* Name of this file.  */
	char           *filename;

	/*
	 * Name to use for the symbol giving address of text start Usually
	 * the same as filename, but for a file spec'd with -l this is the -l
	 * switch itself rather than the filename.
	 */
	char           *local_sym_name;

	/* Describe the layout of the contents of the file */

	/* The file's a.out header.  */
	struct exec     header;
	/* Offset in file of GDB symbol segment, or 0 if there is none.  */
	int             symseg_offset;

	/* Describe data from the file loaded into core */

	/*
	 * Symbol table of the file.
	 * We need access to the global symbol early, ie. before
	 * symbols are asssigned there final values. gotslot_offset is
	 * here because GOT entries may be generated for local symbols.
	 */
	struct localsymbol {
		struct nzlist		nzlist;
		struct glosym		*symbol;
		struct localsymbol	*next;
		long			gotslot_offset;
		char			gotslot_claimed;
	} *symbols;

	/* Number of symbols in above array. */
	int		nsymbols;

	/* Size in bytes of string table.  */
	int             string_size;

	/*
	 * Pointer to the string table. The string table is not kept in core
	 * all the time, but when it is in core, its address is here.
	 */
	char           *strings;

	/* Offset of string table (normally N_STROFF() + 4) */
	int		strings_offset;

	/* Next two used only if `relocatable_output' or if needed for */
	/* output of undefined reference line numbers. */

	/* Text reloc info saved by `write_text' for `coptxtrel'.  */
	struct relocation_info *textrel;
	int		ntextrel;

	/* Data reloc info saved by `write_data' for `copdatrel'.  */
	struct relocation_info *datarel;
	int		ndatarel;

	/* Relation of this file's segments to the output file */

	/* Start of this file's text seg in the output file core image.  */
	int             text_start_address;

	/* Start of this file's data seg in the output file core image.  */
	int             data_start_address;

	/* Start of this file's bss seg in the output file core image.  */
	int             bss_start_address;
	/*
	 * Offset in bytes in the output file symbol table of the first local
	 * symbol for this file. Set by `write_file_symbols'.
	 */
	int             local_syms_offset;

	/* For library members only */

	/* For a library, points to chain of entries for the library members. */
	struct file_entry *subfiles;

	/*
	 * For a library member, offset of the member within the archive.
	 * Zero for files that are not library members.
	 */
	int             starting_offset;

	/* Size of contents of this file, if library member.  */
	int             total_size;

	/* For library member, points to the library's own entry.  */
	struct file_entry *superfile;

	/* For library member, points to next entry for next member.  */
	struct file_entry *chain;

	/* 1 if file is a library. */
	char            library_flag;

	/* 1 if file's header has been read into this structure.  */
	char            header_read_flag;

	/* 1 means search a set of directories for this file.  */
	char            search_dirs_flag;

	/*
	 * 1 means this is base file of incremental load. Do not load this
	 * file's text or data. Also default text_start to after this file's
	 * bss.
	 */
	char            just_syms_flag;

	/* 1 means search for dynamic libraries (dependent on -B switch) */
	char            search_dynamic_flag;

	/* version numbers of selected shared library */
	int             lib_major, lib_minor;

	/* This entry is a shared object */
	char            is_dynamic;
};

typedef struct localsymbol localsymbol_t;

/* Vector of entries for input files specified by arguments.
   These are all the input files except for members of specified libraries.  */
struct file_entry *file_table;

/* Length of that vector.  */
int number_of_files;

/* Current link mode */
#define DYNAMIC		1		/* Consider shared libraries */
#define SYMBOLIC	2		/* Force symbolic resolution */
#define FORCEARCHIVE	4		/* Force inclusion of all members
					   of archives */
#define SHAREABLE	8		/* Build a shared object */
int link_mode;

/*
 * Runtime Relocation Section (RRS).
 * This describes the data structures that go into the output text and data
 * segments to support the run-time linker. The RRS can be empty (plain old
 * static linking), or can just exist of GOT and PLT entries (in case of
 * statically linked PIC code).
 */

int	rrs_section_type;
#define RRS_NONE	0
#define RRS_PARTIAL	1
#define RRS_FULL	2

int	rrs_text_size;
int	rrs_data_size;
int	rrs_text_start;
int	rrs_data_start;

/* Version number to put in __DYNAMIC (set by -V) */
int	soversion;

/* When loading the text and data, we can avoid doing a close
   and another open between members of the same library.

   These two variables remember the file that is currently open.
   Both are zero if no file is open.

   See `each_file' and `file_close'.  */

struct file_entry *input_file;
int input_desc;

/* The name of the file to write; "a.out" by default.  */

char *output_filename;

/* Descriptor for writing that file with `mywrite'.  */

int outdesc;

/* Header for that file (filled in by `write_header').  */

struct exec outheader;

/* The following are computed by `digest_symbols'.  */

int text_size;		/* total size of text of all input files. */
int data_size;		/* total size of data of all input files. */
int bss_size;		/* total size of bss of all input files. */
int text_reloc_size;	/* total size of text relocation of all input files. */
int data_reloc_size;	/* total size of data relocation of all input files. */

/* Relocation offsets set by perform_relocation(). Defined globaly here
   because some of the RRS routines need access to them */
int	text_relocation;
int	data_relocation;
int	bss_relocation;
int	pc_relocation;

/* Specifications of start and length of the area reserved at the end
   of the data segment for the set vectors.  Computed in 'digest_symbols' */
int set_sect_start;
int set_sect_size;

/* Amount of cleared space to leave between the text and data segments.  */
int text_pad;

/* Amount of bss segment to include as part of the data segment.  */
int data_pad;


/* Record most of the command options.  */

/* Address we assume the text section will be loaded at.
   We relocate symbols and text and data for this, but we do not
   write any padding in the output file for it.  */
int text_start;

/* Offset of default entry-pc within the text section.  */
int entry_offset;

/* Address we decide the data section will be loaded at.  */
int data_start;
int bss_start;

/* Keep a list of any symbols referenced from the command line (so
   that error messages for these guys can be generated). This list is
   zero terminated. */
struct glosym **cmdline_references;
int cl_refs_allocated;

/*
 * Actual vector of directories to search; this contains those specified with
 * -L plus the standard ones.
 */
char	**search_dirs;

/* Length of the vector `search_dirs'.  */
int	n_search_dirs;

void	digest_symbols __P((void));
void	load_symbols __P((void));
void	decode_command __P((int, char **));
void	read_header __P((int, struct file_entry *));
void	read_entry_symbols __P((int, struct file_entry *));
void	read_entry_strings __P((int, struct file_entry *));
void	read_entry_relocation __P((int, struct file_entry *));
void	write_output __P((void));
void	write_header __P((void));
void	write_text __P((void));
void	write_data __P((void));
void	write_rel __P((void));
void	write_syms __P((void));
void	write_symsegs __P((void));
void	mywrite ();

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
char	*concat __P((char *, char *, char *));
int	parse __P((char *, char *, char *));

/* In symbol.c: */
void	symtab_init __P((int));
symbol	*getsym __P((char *)), *getsym_soft __P((char *));

/* In lib.c: */
void	search_library __P((int, struct file_entry *));
void	read_shared_object __P((int, struct file_entry *));
int	findlib __P((struct file_entry *));

/* In shlib.c: */
char	*findshlib __P((char *, int *, int *));
void	add_search_dir __P((char *));
void	std_search_dirs __P((char *));

/* In rrs.c: */
void	init_rrs __P((void));
void	rrs_add_shobj __P((struct file_entry *));
void	alloc_rrs_reloc __P((symbol *));
void	alloc_rrs_segment_reloc __P((struct relocation_info  *));
void	alloc_rrs_jmpslot __P((symbol *));
void	alloc_rrs_gotslot __P((struct relocation_info  *, localsymbol_t *));
void	alloc_rrs_copy_reloc __P((symbol *));

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
void	swap_link_dynamic __P((struct link_dynamic *));
void	swap_link_dynamic_2 __P((struct link_dynamic_2 *));
void	swap_ld_debug __P((struct ld_debug *));
void	swapin_link_object __P((struct link_object *, int));
void	swapout_link_object __P((struct link_object *, int));
void	swapout_fshash __P((struct fshash *, int));
#endif
