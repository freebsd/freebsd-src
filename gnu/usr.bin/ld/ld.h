/*
 *	$FreeBSD$
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

#include "md.h"
#include "link.h"

/* Macro to control the number of undefined references printed */
#define MAX_UREFS_PRINTED	10

/* Align to power-of-two boundary */
#define PALIGN(x,p)	(((x) +  (u_long)(p) - 1) & (-(u_long)(p)))

/* Align to machine dependent boundary */
#define MALIGN(x)	PALIGN(x,MAX_ALIGNMENT)

/* Define this to specify the default executable format.  */
#ifndef DEFAULT_MAGIC
#ifdef __FreeBSD__
#define DEFAULT_MAGIC QMAGIC
extern int	netzmagic;
#else
#define DEFAULT_MAGIC ZMAGIC
#endif
#endif

#ifdef DEMANGLE_CPLUSPLUS
extern char *demangle __P((char*));
#else
#define demangle(name) name
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

#endif

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


/* Number of buckets in symbol hash table */
#define	SYMTABSIZE	1009

/* # of global symbols referenced and not defined.  */
extern int	undefined_global_sym_count;

/* # of weak symbols referenced and not defined.  */
extern int	undefined_weak_sym_count;

/* # of undefined symbols referenced by shared objects */
extern int	undefined_shobj_sym_count;

/* # of multiply defined symbols. */
extern int	multiple_def_count;

/* # of common symbols. */
extern int	common_defined_global_count;

/* # of warning symbols encountered. */
extern int	warn_sym_count;
extern int	list_warning_symbols;

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

struct glosym;
#ifndef __symbol_defined__
#define __symbol_defined__
typedef struct glosym symbol;
#endif

extern symbol	*entry_symbol;		/* the entry symbol, if any */
extern symbol	*edata_symbol;		/* the symbol _edata */
extern symbol	*etext_symbol;		/* the symbol _etext */
extern symbol	*end_symbol;		/* the symbol _end */

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

extern int		magic;		/* Output file magic. */
extern int		relocatable_output;

/* Size of a page. */
extern int	page_size;

extern char	**search_dirs;	/* Directories to search for libraries. */
extern int	n_search_dirs;	/* Length of above. */

extern int	write_map;	/* write a load map (`-M') */

#include "dynamic.h"
