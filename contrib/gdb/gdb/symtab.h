/* Symbol table definitions for GDB.
   Copyright 1986, 89, 91, 92, 93, 94, 95, 96, 1998
             Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if !defined (SYMTAB_H)
#define SYMTAB_H 1

/* Some definitions and declarations to go with use of obstacks.  */

#include "obstack.h"
#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
#include "bcache.h"

/* Don't do this; it means that if some .o's are compiled with GNU C
   and some are not (easy to do accidentally the way we configure
   things; also it is a pain to have to "make clean" every time you
   want to switch compilers), then GDB dies a horrible death.  */
/* GNU C supports enums that are bitfields.  Some compilers don't. */
#if 0 && defined(__GNUC__) && !defined(BYTE_BITFIELD)
#define	BYTE_BITFIELD	:8;
#else
#define	BYTE_BITFIELD	/*nothing*/
#endif

/* Define a structure for the information that is common to all symbol types,
   including minimal symbols, partial symbols, and full symbols.  In a
   multilanguage environment, some language specific information may need to
   be recorded along with each symbol.

   These fields are ordered to encourage good packing, since we frequently
   have tens or hundreds of thousands of these.  */

struct general_symbol_info
{
  /* Name of the symbol.  This is a required field.  Storage for the name is
     allocated on the psymbol_obstack or symbol_obstack for the associated
     objfile. */

  char *name;

  /* Value of the symbol.  Which member of this union to use, and what
     it means, depends on what kind of symbol this is and its
     SYMBOL_CLASS.  See comments there for more details.  All of these
     are in host byte order (though what they point to might be in
     target byte order, e.g. LOC_CONST_BYTES).  */

  union
    {
      /* The fact that this is a long not a LONGEST mainly limits the
	 range of a LOC_CONST.  Since LOC_CONST_BYTES exists, I'm not
	 sure that is a big deal.  */
      long ivalue;

      struct block *block;

      char *bytes;

      CORE_ADDR address;

      /* for opaque typedef struct chain */

      struct symbol *chain;
    }
  value;

  /* Since one and only one language can apply, wrap the language specific
     information inside a union. */

  union
    {
      struct cplus_specific      /* For C++ */
				/*  and Java */
	{
	  char *demangled_name;
	} cplus_specific;
      struct chill_specific      /* For Chill */
	{
	  char *demangled_name;
	} chill_specific;
    } language_specific;

  /* Record the source code language that applies to this symbol.
     This is used to select one of the fields from the language specific
     union above. */

  enum language language BYTE_BITFIELD;

  /* Which section is this symbol in?  This is an index into
     section_offsets for this objfile.  Negative means that the symbol
     does not get relocated relative to a section.
     Disclaimer: currently this is just used for xcoff, so don't
     expect all symbol-reading code to set it correctly (the ELF code
     also tries to set it correctly).  */

  short section;

  /* The bfd section associated with this symbol. */

  asection *bfd_section;
};

extern CORE_ADDR symbol_overlayed_address PARAMS((CORE_ADDR, asection *));

#define SYMBOL_NAME(symbol)		(symbol)->ginfo.name
#define SYMBOL_VALUE(symbol)		(symbol)->ginfo.value.ivalue
#define SYMBOL_VALUE_ADDRESS(symbol)	(symbol)->ginfo.value.address
#define SYMBOL_VALUE_BYTES(symbol)	(symbol)->ginfo.value.bytes
#define SYMBOL_BLOCK_VALUE(symbol)	(symbol)->ginfo.value.block
#define SYMBOL_VALUE_CHAIN(symbol)	(symbol)->ginfo.value.chain
#define SYMBOL_LANGUAGE(symbol)		(symbol)->ginfo.language
#define SYMBOL_SECTION(symbol)		(symbol)->ginfo.section
#define SYMBOL_BFD_SECTION(symbol)	(symbol)->ginfo.bfd_section

#define SYMBOL_CPLUS_DEMANGLED_NAME(symbol)	\
  (symbol)->ginfo.language_specific.cplus_specific.demangled_name

/* Macro that initializes the language dependent portion of a symbol
   depending upon the language for the symbol. */

#define SYMBOL_INIT_LANGUAGE_SPECIFIC(symbol,language)			\
  do {									\
    SYMBOL_LANGUAGE (symbol) = language;				\
    if (SYMBOL_LANGUAGE (symbol) == language_cplus			\
	|| SYMBOL_LANGUAGE (symbol) == language_java			\
	)								\
      {									\
	SYMBOL_CPLUS_DEMANGLED_NAME (symbol) = NULL;			\
      }									\
    else if (SYMBOL_LANGUAGE (symbol) == language_chill)		\
      {									\
	SYMBOL_CHILL_DEMANGLED_NAME (symbol) = NULL;			\
      }									\
    else								\
      {									\
	memset (&(symbol)->ginfo.language_specific, 0,			\
		sizeof ((symbol)->ginfo.language_specific));		\
      }									\
  } while (0)

/* Macro that attempts to initialize the demangled name for a symbol,
   based on the language of that symbol.  If the language is set to
   language_auto, it will attempt to find any demangling algorithm
   that works and then set the language appropriately.  If no demangling
   of any kind is found, the language is set back to language_unknown,
   so we can avoid doing this work again the next time we encounter
   the symbol.  Any required space to store the name is obtained from the
   specified obstack. */

#define SYMBOL_INIT_DEMANGLED_NAME(symbol,obstack)			\
  do {									\
    char *demangled = NULL;						\
    if (SYMBOL_LANGUAGE (symbol) == language_cplus			\
	|| SYMBOL_LANGUAGE (symbol) == language_auto)			\
      {									\
	demangled =							\
	  cplus_demangle (SYMBOL_NAME (symbol), DMGL_PARAMS | DMGL_ANSI);\
	if (demangled != NULL)						\
	  {								\
	    SYMBOL_LANGUAGE (symbol) = language_cplus;			\
	    SYMBOL_CPLUS_DEMANGLED_NAME (symbol) = 			\
	      obsavestring (demangled, strlen (demangled), (obstack));	\
	    free (demangled);						\
	  }								\
	else								\
	  {								\
	    SYMBOL_CPLUS_DEMANGLED_NAME (symbol) = NULL;		\
	  }								\
      }									\
    if (SYMBOL_LANGUAGE (symbol) == language_java)			\
      {									\
	demangled =							\
	  cplus_demangle (SYMBOL_NAME (symbol),				\
			  DMGL_PARAMS | DMGL_ANSI | DMGL_JAVA);		\
	if (demangled != NULL)						\
	  {								\
	    SYMBOL_LANGUAGE (symbol) = language_java;			\
	    SYMBOL_CPLUS_DEMANGLED_NAME (symbol) = 			\
	      obsavestring (demangled, strlen (demangled), (obstack));	\
	    free (demangled);						\
	  }								\
	else								\
	  {								\
	    SYMBOL_CPLUS_DEMANGLED_NAME (symbol) = NULL;		\
	  }								\
      }									\
    if (demangled == NULL						\
	&& (SYMBOL_LANGUAGE (symbol) == language_chill			\
	    || SYMBOL_LANGUAGE (symbol) == language_auto))		\
      {									\
	demangled =							\
	  chill_demangle (SYMBOL_NAME (symbol));			\
	if (demangled != NULL)						\
	  {								\
	    SYMBOL_LANGUAGE (symbol) = language_chill;			\
	    SYMBOL_CHILL_DEMANGLED_NAME (symbol) = 			\
	      obsavestring (demangled, strlen (demangled), (obstack));	\
	    free (demangled);						\
	  }								\
	else								\
	  {								\
	    SYMBOL_CHILL_DEMANGLED_NAME (symbol) = NULL;		\
	  }								\
      }									\
    if (SYMBOL_LANGUAGE (symbol) == language_auto)			\
      {									\
	SYMBOL_LANGUAGE (symbol) = language_unknown;			\
      }									\
  } while (0)

/* Macro that returns the demangled name for a symbol based on the language
   for that symbol.  If no demangled name exists, returns NULL. */

#define SYMBOL_DEMANGLED_NAME(symbol)					\
  (SYMBOL_LANGUAGE (symbol) == language_cplus				\
   || SYMBOL_LANGUAGE (symbol) == language_java				\
   ? SYMBOL_CPLUS_DEMANGLED_NAME (symbol)				\
   : (SYMBOL_LANGUAGE (symbol) == language_chill			\
      ? SYMBOL_CHILL_DEMANGLED_NAME (symbol)				\
      : NULL))

#define SYMBOL_CHILL_DEMANGLED_NAME(symbol)				\
  (symbol)->ginfo.language_specific.chill_specific.demangled_name

/* Macro that returns the "natural source name" of a symbol.  In C++ this is
   the "demangled" form of the name if demangle is on and the "mangled" form
   of the name if demangle is off.  In other languages this is just the
   symbol name.  The result should never be NULL. */

#define SYMBOL_SOURCE_NAME(symbol)					\
  (demangle && SYMBOL_DEMANGLED_NAME (symbol) != NULL			\
   ? SYMBOL_DEMANGLED_NAME (symbol)					\
   : SYMBOL_NAME (symbol))

/* Macro that returns the "natural assembly name" of a symbol.  In C++ this is
   the "mangled" form of the name if demangle is off, or if demangle is on and
   asm_demangle is off.  Otherwise if asm_demangle is on it is the "demangled"
   form.  In other languages this is just the symbol name.  The result should
   never be NULL. */

#define SYMBOL_LINKAGE_NAME(symbol)					\
  (demangle && asm_demangle && SYMBOL_DEMANGLED_NAME (symbol) != NULL	\
   ? SYMBOL_DEMANGLED_NAME (symbol)					\
   : SYMBOL_NAME (symbol))

/* Macro that tests a symbol for a match against a specified name string.
   First test the unencoded name, then looks for and test a C++ encoded
   name if it exists.  Note that whitespace is ignored while attempting to
   match a C++ encoded name, so that "foo::bar(int,long)" is the same as
   "foo :: bar (int, long)".
   Evaluates to zero if the match fails, or nonzero if it succeeds. */

#define SYMBOL_MATCHES_NAME(symbol, name)				\
  (STREQ (SYMBOL_NAME (symbol), (name))					\
   || (SYMBOL_DEMANGLED_NAME (symbol) != NULL				\
       && strcmp_iw (SYMBOL_DEMANGLED_NAME (symbol), (name)) == 0))
   
/* Macro that tests a symbol for an re-match against the last compiled regular
   expression.  First test the unencoded name, then look for and test a C++
   encoded name if it exists.
   Evaluates to zero if the match fails, or nonzero if it succeeds. */

#define SYMBOL_MATCHES_REGEXP(symbol)					\
  (re_exec (SYMBOL_NAME (symbol)) != 0					\
   || (SYMBOL_DEMANGLED_NAME (symbol) != NULL				\
       && re_exec (SYMBOL_DEMANGLED_NAME (symbol)) != 0))
   
/* Define a simple structure used to hold some very basic information about
   all defined global symbols (text, data, bss, abs, etc).  The only required
   information is the general_symbol_info.

   In many cases, even if a file was compiled with no special options for
   debugging at all, as long as was not stripped it will contain sufficient
   information to build a useful minimal symbol table using this structure.
   Even when a file contains enough debugging information to build a full
   symbol table, these minimal symbols are still useful for quickly mapping
   between names and addresses, and vice versa.  They are also sometimes
   used to figure out what full symbol table entries need to be read in. */

struct minimal_symbol
{

  /* The general symbol info required for all types of symbols.

     The SYMBOL_VALUE_ADDRESS contains the address that this symbol
     corresponds to.  */

  struct general_symbol_info ginfo;

  /* The info field is available for caching machine-specific information
     so it doesn't have to rederive the info constantly (over a serial line).
     It is initialized to zero and stays that way until target-dependent code
     sets it.  Storage for any data pointed to by this field should be allo-
     cated on the symbol_obstack for the associated objfile.  
     The type would be "void *" except for reasons of compatibility with older
     compilers.  This field is optional.

     Currently, the AMD 29000 tdep.c uses it to remember things it has decoded
     from the instructions in the function header, and the MIPS-16 code uses
     it to identify 16-bit procedures.  */

  char *info;

#ifdef SOFUN_ADDRESS_MAYBE_MISSING
  /* Which source file is this symbol in?  Only relevant for mst_file_*.  */
  char *filename;
#endif

  /* Classification types for this symbol.  These should be taken as "advisory
     only", since if gdb can't easily figure out a classification it simply
     selects mst_unknown.  It may also have to guess when it can't figure out
     which is a better match between two types (mst_data versus mst_bss) for
     example.  Since the minimal symbol info is sometimes derived from the
     BFD library's view of a file, we need to live with what information bfd
     supplies. */

  enum minimal_symbol_type
    {
      mst_unknown = 0,		/* Unknown type, the default */
      mst_text,			/* Generally executable instructions */
      mst_data,			/* Generally initialized data */
      mst_bss,			/* Generally uninitialized data */
      mst_abs,			/* Generally absolute (nonrelocatable) */
      /* GDB uses mst_solib_trampoline for the start address of a shared
	 library trampoline entry.  Breakpoints for shared library functions
	 are put there if the shared library is not yet loaded.
	 After the shared library is loaded, lookup_minimal_symbol will
	 prefer the minimal symbol from the shared library (usually
	 a mst_text symbol) over the mst_solib_trampoline symbol, and the
	 breakpoints will be moved to their true address in the shared
	 library via breakpoint_re_set.  */
      mst_solib_trampoline,	/* Shared library trampoline code */
      /* For the mst_file* types, the names are only guaranteed to be unique
	 within a given .o file.  */
      mst_file_text,		/* Static version of mst_text */
      mst_file_data,		/* Static version of mst_data */
      mst_file_bss		/* Static version of mst_bss */
    } type BYTE_BITFIELD;
};

#define MSYMBOL_INFO(msymbol)		(msymbol)->info
#define MSYMBOL_TYPE(msymbol)		(msymbol)->type


/* All of the name-scope contours of the program
   are represented by `struct block' objects.
   All of these objects are pointed to by the blockvector.

   Each block represents one name scope.
   Each lexical context has its own block.

   The blockvector begins with some special blocks.
   The GLOBAL_BLOCK contains all the symbols defined in this compilation
   whose scope is the entire program linked together.
   The STATIC_BLOCK contains all the symbols whose scope is the
   entire compilation excluding other separate compilations.
   Blocks starting with the FIRST_LOCAL_BLOCK are not special.

   Each block records a range of core addresses for the code that
   is in the scope of the block.  The STATIC_BLOCK and GLOBAL_BLOCK
   give, for the range of code, the entire range of code produced
   by the compilation that the symbol segment belongs to.

   The blocks appear in the blockvector
   in order of increasing starting-address,
   and, within that, in order of decreasing ending-address.

   This implies that within the body of one function
   the blocks appear in the order of a depth-first tree walk.  */

struct blockvector
{
  /* Number of blocks in the list.  */
  int nblocks;
  /* The blocks themselves.  */
  struct block *block[1];
};

#define BLOCKVECTOR_NBLOCKS(blocklist) (blocklist)->nblocks
#define BLOCKVECTOR_BLOCK(blocklist,n) (blocklist)->block[n]

/* Special block numbers */

#define GLOBAL_BLOCK		0
#define	STATIC_BLOCK		1
#define	FIRST_LOCAL_BLOCK	2

struct block
{

  /* Addresses in the executable code that are in this block.  */

  CORE_ADDR startaddr;
  CORE_ADDR endaddr;

  /* The symbol that names this block, if the block is the body of a
     function; otherwise, zero.  */

  struct symbol *function;

  /* The `struct block' for the containing block, or 0 if none.

     The superblock of a top-level local block (i.e. a function in the
     case of C) is the STATIC_BLOCK.  The superblock of the
     STATIC_BLOCK is the GLOBAL_BLOCK.  */

  struct block *superblock;

  /* Version of GCC used to compile the function corresponding
     to this block, or 0 if not compiled with GCC.  When possible,
     GCC should be compatible with the native compiler, or if that
     is not feasible, the differences should be fixed during symbol
     reading.  As of 16 Apr 93, this flag is never used to distinguish
     between gcc2 and the native compiler.

     If there is no function corresponding to this block, this meaning
     of this flag is undefined.  */

  unsigned char gcc_compile_flag;

  /* Number of local symbols.  */

  int nsyms;

  /* The symbols.  If some of them are arguments, then they must be
     in the order in which we would like to print them.  */

  struct symbol *sym[1];
};

#define BLOCK_START(bl)		(bl)->startaddr
#define BLOCK_END(bl)		(bl)->endaddr
#define BLOCK_NSYMS(bl)		(bl)->nsyms
#define BLOCK_SYM(bl, n)	(bl)->sym[n]
#define BLOCK_FUNCTION(bl)	(bl)->function
#define BLOCK_SUPERBLOCK(bl)	(bl)->superblock
#define BLOCK_GCC_COMPILED(bl)	(bl)->gcc_compile_flag

/* Nonzero if symbols of block BL should be sorted alphabetically.
   Don't sort a block which corresponds to a function.  If we did the
   sorting would have to preserve the order of the symbols for the
   arguments.  */

#define BLOCK_SHOULD_SORT(bl) ((bl)->nsyms >= 40 && BLOCK_FUNCTION (bl) == NULL)


/* Represent one symbol name; a variable, constant, function or typedef.  */

/* Different name spaces for symbols.  Looking up a symbol specifies a
   namespace and ignores symbol definitions in other name spaces. */
 
typedef enum 
{
  /* UNDEF_NAMESPACE is used when a namespace has not been discovered or
     none of the following apply.  This usually indicates an error either
     in the symbol information or in gdb's handling of symbols. */

  UNDEF_NAMESPACE,

  /* VAR_NAMESPACE is the usual namespace.  In C, this contains variables,
     function names, typedef names and enum type values. */

  VAR_NAMESPACE,

  /* STRUCT_NAMESPACE is used in C to hold struct, union and enum type names.
     Thus, if `struct foo' is used in a C program, it produces a symbol named
     `foo' in the STRUCT_NAMESPACE. */

  STRUCT_NAMESPACE,

  /* LABEL_NAMESPACE may be used for names of labels (for gotos);
     currently it is not used and labels are not recorded at all.  */

  LABEL_NAMESPACE,

  /* Searching namespaces. These overlap with VAR_NAMESPACE, providing
     some granularity with the search_symbols function. */

  /* Everything in VAR_NAMESPACE minus FUNCTIONS_-, TYPES_-, and
     METHODS_NAMESPACE */
  VARIABLES_NAMESPACE,

  /* All functions -- for some reason not methods, though. */
  FUNCTIONS_NAMESPACE,

  /* All defined types */
  TYPES_NAMESPACE,

  /* All class methods -- why is this separated out? */
  METHODS_NAMESPACE

} namespace_enum;

/* An address-class says where to find the value of a symbol.  */

enum address_class
{
  /* Not used; catches errors */

  LOC_UNDEF,

  /* Value is constant int SYMBOL_VALUE, host byteorder */

  LOC_CONST,

  /* Value is at fixed address SYMBOL_VALUE_ADDRESS */

  LOC_STATIC,

  /* Value is in register.  SYMBOL_VALUE is the register number.  */

  LOC_REGISTER,

  /* It's an argument; the value is at SYMBOL_VALUE offset in arglist.  */

  LOC_ARG,

  /* Value address is at SYMBOL_VALUE offset in arglist.  */

  LOC_REF_ARG,

  /* Value is in register number SYMBOL_VALUE.  Just like LOC_REGISTER
     except this is an argument.  Probably the cleaner way to handle
     this would be to separate address_class (which would include
     separate ARG and LOCAL to deal with FRAME_ARGS_ADDRESS versus
     FRAME_LOCALS_ADDRESS), and an is_argument flag.

     For some symbol formats (stabs, for some compilers at least),
     the compiler generates two symbols, an argument and a register.
     In some cases we combine them to a single LOC_REGPARM in symbol
     reading, but currently not for all cases (e.g. it's passed on the
     stack and then loaded into a register).  */

  LOC_REGPARM,

  /* Value is in specified register.  Just like LOC_REGPARM except the
     register holds the address of the argument instead of the argument
     itself. This is currently used for the passing of structs and unions
     on sparc and hppa.  It is also used for call by reference where the
     address is in a register, at least by mipsread.c.  */

  LOC_REGPARM_ADDR,

  /* Value is a local variable at SYMBOL_VALUE offset in stack frame.  */

  LOC_LOCAL,

  /* Value not used; definition in SYMBOL_TYPE.  Symbols in the namespace
     STRUCT_NAMESPACE all have this class.  */

  LOC_TYPEDEF,

  /* Value is address SYMBOL_VALUE_ADDRESS in the code */

  LOC_LABEL,

  /* In a symbol table, value is SYMBOL_BLOCK_VALUE of a `struct block'.
     In a partial symbol table, SYMBOL_VALUE_ADDRESS is the start address
     of the block.  Function names have this class. */

  LOC_BLOCK,

  /* Value is a constant byte-sequence pointed to by SYMBOL_VALUE_BYTES, in
     target byte order.  */

  LOC_CONST_BYTES,

  /* Value is arg at SYMBOL_VALUE offset in stack frame. Differs from
     LOC_LOCAL in that symbol is an argument; differs from LOC_ARG in
     that we find it in the frame (FRAME_LOCALS_ADDRESS), not in the
     arglist (FRAME_ARGS_ADDRESS).  Added for i960, which passes args
     in regs then copies to frame.  */

  LOC_LOCAL_ARG,

  /* Value is at SYMBOL_VALUE offset from the current value of
     register number SYMBOL_BASEREG.  This exists mainly for the same
     things that LOC_LOCAL and LOC_ARG do; but we need to do this
     instead because on 88k DWARF gives us the offset from the
     frame/stack pointer, rather than the offset from the "canonical
     frame address" used by COFF, stabs, etc., and we don't know how
     to convert between these until we start examining prologues.

     Note that LOC_BASEREG is much less general than a DWARF expression.
     We don't need the generality (at least not yet), and storing a general
     DWARF expression would presumably take up more space than the existing
     scheme.  */

  LOC_BASEREG,

  /* Same as LOC_BASEREG but it is an argument.  */

  LOC_BASEREG_ARG,

  /* Value is at fixed address, but the address of the variable has
     to be determined from the minimal symbol table whenever the
     variable is referenced.
     This happens if debugging information for a global symbol is
     emitted and the corresponding minimal symbol is defined
     in another object file or runtime common storage.
     The linker might even remove the minimal symbol if the global
     symbol is never referenced, in which case the symbol remains
     unresolved.  */

  LOC_UNRESOLVED,

  /* Value is at a thread-specific location calculated by a
     target-specific method. */
     
  LOC_THREAD_LOCAL_STATIC,
     
  /* The variable does not actually exist in the program.
     The value is ignored.  */

  LOC_OPTIMIZED_OUT,

  /* The variable is static, but actually lives at * (address).
   * I.e. do an extra indirection to get to it.
   * This is used on HP-UX to get at globals that are allocated
   * in shared libraries, where references from images other
   * than the one where the global was allocated are done
   * with a level of indirection.
   */

  LOC_INDIRECT

};

/* Linked list of symbol's live ranges. */

struct range_list		
{
  CORE_ADDR start;
  CORE_ADDR end;
  struct range_list *next;	
};

/* Linked list of aliases for a particular main/primary symbol.  */
struct alias_list
  {
    struct symbol *sym;
    struct alias_list *next;
  };

struct symbol
{

  /* The general symbol info required for all types of symbols. */

  struct general_symbol_info ginfo;

  /* Data type of value */

  struct type *type;

  /* Name space code.  */

#ifdef __MFC4__
  /* FIXME: don't conflict with C++'s namespace */
  /* would be safer to do a global change for all namespace identifiers. */
  #define namespace _namespace
#endif
  namespace_enum namespace BYTE_BITFIELD;

  /* Address class */

  enum address_class aclass BYTE_BITFIELD;

  /* Line number of definition.  FIXME:  Should we really make the assumption
     that nobody will try to debug files longer than 64K lines?  What about
     machine generated programs? */

  unsigned short line;
  
  /* Some symbols require an additional value to be recorded on a per-
     symbol basis.  Stash those values here. */

  union
    {
      /* Used by LOC_BASEREG and LOC_BASEREG_ARG.  */
      short basereg;
    }
  aux_value;


  /* Link to a list of aliases for this symbol.
     Only a "primary/main symbol may have aliases.  */
  struct alias_list *aliases;

  /* List of ranges where this symbol is active.  This is only
     used by alias symbols at the current time.  */
  struct range_list *ranges;
};


#define SYMBOL_NAMESPACE(symbol)	(symbol)->namespace
#define SYMBOL_CLASS(symbol)		(symbol)->aclass
#define SYMBOL_TYPE(symbol)		(symbol)->type
#define SYMBOL_LINE(symbol)		(symbol)->line
#define SYMBOL_BASEREG(symbol)		(symbol)->aux_value.basereg
#define SYMBOL_ALIASES(symbol)		(symbol)->aliases
#define SYMBOL_RANGES(symbol)		(symbol)->ranges

/* A partial_symbol records the name, namespace, and address class of
   symbols whose types we have not parsed yet.  For functions, it also
   contains their memory address, so we can find them from a PC value.
   Each partial_symbol sits in a partial_symtab, all of which are chained
   on a  partial symtab list and which points to the corresponding 
   normal symtab once the partial_symtab has been referenced.  */

struct partial_symbol
{

  /* The general symbol info required for all types of symbols. */

  struct general_symbol_info ginfo;

  /* Name space code.  */

  namespace_enum namespace BYTE_BITFIELD;

  /* Address class (for info_symbols) */

  enum address_class aclass BYTE_BITFIELD;

};

#define PSYMBOL_NAMESPACE(psymbol)	(psymbol)->namespace
#define PSYMBOL_CLASS(psymbol)		(psymbol)->aclass


/* Source-file information.  This describes the relation between source files,
   ine numbers and addresses in the program text.  */

struct sourcevector
{
  int length;			/* Number of source files described */
  struct source *source[1];	/* Descriptions of the files */
};

/* Each item represents a line-->pc (or the reverse) mapping.  This is
   somewhat more wasteful of space than one might wish, but since only
   the files which are actually debugged are read in to core, we don't
   waste much space.  */

struct linetable_entry
{
  int line;
  CORE_ADDR pc;
};

/* The order of entries in the linetable is significant.  They should
   be sorted by increasing values of the pc field.  If there is more than
   one entry for a given pc, then I'm not sure what should happen (and
   I not sure whether we currently handle it the best way).

   Example: a C for statement generally looks like this

   	10	0x100	- for the init/test part of a for stmt.
   	20	0x200
   	30	0x300
   	10	0x400	- for the increment part of a for stmt.

   */

struct linetable
{
  int nitems;

  /* Actually NITEMS elements.  If you don't like this use of the
     `struct hack', you can shove it up your ANSI (seriously, if the
     committee tells us how to do it, we can probably go along).  */
  struct linetable_entry item[1];
};

/* All the information on one source file.  */

struct source
{
  char *name;			/* Name of file */
  struct linetable contents;
};

/* How to relocate the symbols from each section in a symbol file.
   Each struct contains an array of offsets.
   The ordering and meaning of the offsets is file-type-dependent;
   typically it is indexed by section numbers or symbol types or
   something like that.

   To give us flexibility in changing the internal representation
   of these offsets, the ANOFFSET macro must be used to insert and
   extract offset values in the struct.  */

struct section_offsets
  {
    CORE_ADDR offsets[1];		/* As many as needed. */
  };

#define	ANOFFSET(secoff, whichone)	(secoff->offsets[whichone])

/* The maximum possible size of a section_offsets table.  */
 
#define SIZEOF_SECTION_OFFSETS \
  (sizeof (struct section_offsets) \
   + sizeof (((struct section_offsets *) 0)->offsets) * (SECT_OFF_MAX-1))


/* Each source file or header is represented by a struct symtab. 
   These objects are chained through the `next' field.  */

struct symtab
  {

    /* Chain of all existing symtabs.  */

    struct symtab *next;

    /* List of all symbol scope blocks for this symtab.  May be shared
       between different symtabs (and normally is for all the symtabs
       in a given compilation unit).  */

    struct blockvector *blockvector;

    /* Table mapping core addresses to line numbers for this file.
       Can be NULL if none.  Never shared between different symtabs.  */

    struct linetable *linetable;

    /* Section in objfile->section_offsets for the blockvector and
       the linetable.  Probably always SECT_OFF_TEXT.  */

    int block_line_section;

    /* If several symtabs share a blockvector, exactly one of them
       should be designed the primary, so that the blockvector
       is relocated exactly once by objfile_relocate.  */

    int primary;

    /* Name of this source file.  */

    char *filename;

    /* Directory in which it was compiled, or NULL if we don't know.  */

    char *dirname;

    /* This component says how to free the data we point to:
       free_contents => do a tree walk and free each object.
       free_nothing => do nothing; some other symtab will free
         the data this one uses.
      free_linetable => free just the linetable.  FIXME: Is this redundant
      with the primary field?  */

    enum free_code
      {
	free_nothing, free_contents, free_linetable
	}
    free_code;

    /* Pointer to one block of storage to be freed, if nonzero.  */
    /* This is IN ADDITION to the action indicated by free_code.  */
    
    char *free_ptr;

    /* Total number of lines found in source file.  */

    int nlines;

    /* line_charpos[N] is the position of the (N-1)th line of the
       source file.  "position" means something we can lseek() to; it
       is not guaranteed to be useful any other way.  */

    int *line_charpos;

    /* Language of this source file.  */

    enum language language;

    /* String that identifies the format of the debugging information, such
       as "stabs", "dwarf 1", "dwarf 2", "coff", etc.  This is mostly useful
       for automated testing of gdb but may also be information that is
       useful to the user. */

    char *debugformat;

    /* String of version information.  May be zero.  */

    char *version;

    /* Full name of file as found by searching the source path.
       NULL if not yet known.  */

    char *fullname;

    /* Object file from which this symbol information was read.  */

    struct objfile *objfile;

  };

#define BLOCKVECTOR(symtab)	(symtab)->blockvector
#define LINETABLE(symtab)	(symtab)->linetable


/* Each source file that has not been fully read in is represented by
   a partial_symtab.  This contains the information on where in the
   executable the debugging symbols for a specific file are, and a
   list of names of global symbols which are located in this file.
   They are all chained on partial symtab lists.

   Even after the source file has been read into a symtab, the
   partial_symtab remains around.  They are allocated on an obstack,
   psymbol_obstack.  FIXME, this is bad for dynamic linking or VxWorks-
   style execution of a bunch of .o's.  */

struct partial_symtab
{

  /* Chain of all existing partial symtabs.  */

  struct partial_symtab *next;

  /* Name of the source file which this partial_symtab defines */

  char *filename;

  /* Information about the object file from which symbols should be read.  */

  struct objfile *objfile;

  /* Set of relocation offsets to apply to each section.  */ 

  struct section_offsets *section_offsets;

  /* Range of text addresses covered by this file; texthigh is the
     beginning of the next section. */

  CORE_ADDR textlow;
  CORE_ADDR texthigh;

  /* Array of pointers to all of the partial_symtab's which this one
     depends on.  Since this array can only be set to previous or
     the current (?) psymtab, this dependency tree is guaranteed not
     to have any loops.  "depends on" means that symbols must be read
     for the dependencies before being read for this psymtab; this is
     for type references in stabs, where if foo.c includes foo.h, declarations
     in foo.h may use type numbers defined in foo.c.  For other debugging
     formats there may be no need to use dependencies.  */

  struct partial_symtab **dependencies;

  int number_of_dependencies;

  /* Global symbol list.  This list will be sorted after readin to
     improve access.  Binary search will be the usual method of
     finding a symbol within it. globals_offset is an integer offset
     within global_psymbols[].  */

  int globals_offset;
  int n_global_syms;

  /* Static symbol list.  This list will *not* be sorted after readin;
     to find a symbol in it, exhaustive search must be used.  This is
     reasonable because searches through this list will eventually
     lead to either the read in of a files symbols for real (assumed
     to take a *lot* of time; check) or an error (and we don't care
     how long errors take).  This is an offset and size within
     static_psymbols[].  */

  int statics_offset;
  int n_static_syms;

  /* Pointer to symtab eventually allocated for this source file, 0 if
     !readin or if we haven't looked for the symtab after it was readin.  */

  struct symtab *symtab;

  /* Pointer to function which will read in the symtab corresponding to
     this psymtab.  */

  void (*read_symtab) PARAMS ((struct partial_symtab *));

  /* Information that lets read_symtab() locate the part of the symbol table
     that this psymtab corresponds to.  This information is private to the
     format-dependent symbol reading routines.  For further detail examine
     the various symbol reading modules.  Should really be (void *) but is
     (char *) as with other such gdb variables.  (FIXME) */

  char *read_symtab_private;

  /* Non-zero if the symtab corresponding to this psymtab has been readin */

  unsigned char readin;
};

/* A fast way to get from a psymtab to its symtab (after the first time).  */
#define	PSYMTAB_TO_SYMTAB(pst)  \
    ((pst) -> symtab != NULL ? (pst) -> symtab : psymtab_to_symtab (pst))


/* The virtual function table is now an array of structures which have the
   form { int16 offset, delta; void *pfn; }. 

   In normal virtual function tables, OFFSET is unused.
   DELTA is the amount which is added to the apparent object's base
   address in order to point to the actual object to which the
   virtual function should be applied.
   PFN is a pointer to the virtual function.

   Note that this macro is g++ specific (FIXME). */
  
#define VTBL_FNADDR_OFFSET 2

/* Macro that yields non-zero value iff NAME is the prefix for C++ operator
   names.  If you leave out the parenthesis here you will lose!
   Currently 'o' 'p' CPLUS_MARKER is used for both the symbol in the
   symbol-file and the names in gdb's symbol table.
   Note that this macro is g++ specific (FIXME). */

#define OPNAME_PREFIX_P(NAME) \
  ((NAME)[0] == 'o' && (NAME)[1] == 'p' && is_cplus_marker ((NAME)[2]))

/* Macro that yields non-zero value iff NAME is the prefix for C++ vtbl
   names.  Note that this macro is g++ specific (FIXME).
   '_vt$' is the old cfront-style vtables; '_VT$' is the new
   style, using thunks (where '$' is really CPLUS_MARKER). */

#define VTBL_PREFIX_P(NAME) \
  ((NAME)[0] == '_' \
   && (((NAME)[1] == 'V' && (NAME)[2] == 'T') \
       || ((NAME)[1] == 'v' && (NAME)[2] == 't')) \
   && is_cplus_marker ((NAME)[3]))

/* Macro that yields non-zero value iff NAME is the prefix for C++ destructor
   names.  Note that this macro is g++ specific (FIXME).  */

#define DESTRUCTOR_PREFIX_P(NAME) \
  ((NAME)[0] == '_' && is_cplus_marker ((NAME)[1]) && (NAME)[2] == '_')


/* External variables and functions for the objects described above. */

/* This symtab variable specifies the current file for printing source lines */

extern struct symtab *current_source_symtab;

/* This is the next line to print for listing source lines.  */

extern int current_source_line;

/* See the comment in symfile.c about how current_objfile is used. */

extern struct objfile *current_objfile;

/* True if we are nested inside psymtab_to_symtab. */

extern int currently_reading_symtab;

/* From utils.c.  */
extern int demangle;
extern int asm_demangle;

/* symtab.c lookup functions */

/* lookup a symbol table by source file name */

extern struct symtab *
lookup_symtab PARAMS ((char *));

/* lookup a symbol by name (optional block, optional symtab) */

extern struct symbol *
lookup_symbol PARAMS ((const char *, const struct block *,
		       const namespace_enum, int *, struct symtab **));

/* lookup a symbol by name, within a specified block */
  
extern struct symbol *
lookup_block_symbol PARAMS ((const struct block *, const char *,
 			     const namespace_enum));

/* lookup a [struct, union, enum] by name, within a specified block */

extern struct type *
lookup_struct PARAMS ((char *, struct block *));

extern struct type *
lookup_union PARAMS ((char *, struct block *));

extern struct type *
lookup_enum PARAMS ((char *, struct block *));

/* lookup the function corresponding to the block */

extern struct symbol *
block_function PARAMS ((struct block *));

/* from blockframe.c: */

/* lookup the function symbol corresponding to the address */

extern struct symbol *
find_pc_function PARAMS ((CORE_ADDR));

/* lookup the function corresponding to the address and section */

extern struct symbol *
find_pc_sect_function PARAMS ((CORE_ADDR, asection *));
  
/* lookup function from address, return name, start addr and end addr */

extern int 
find_pc_partial_function PARAMS ((CORE_ADDR, char **,
					     CORE_ADDR *, CORE_ADDR *));

extern void
clear_pc_function_cache PARAMS ((void));

extern int 
find_pc_sect_partial_function PARAMS ((CORE_ADDR, asection *, 
                                       char **, CORE_ADDR *, CORE_ADDR *));

/* from symtab.c: */

/* lookup partial symbol table by filename */

extern struct partial_symtab *
lookup_partial_symtab PARAMS ((char *));

/* lookup partial symbol table by address */

extern struct partial_symtab *
find_pc_psymtab PARAMS ((CORE_ADDR));

/* lookup partial symbol table by address and section */

extern struct partial_symtab *
find_pc_sect_psymtab PARAMS ((CORE_ADDR, asection *));

/* lookup full symbol table by address */

extern struct symtab *
find_pc_symtab PARAMS ((CORE_ADDR));

/* lookup full symbol table by address and section */

extern struct symtab *
find_pc_sect_symtab PARAMS ((CORE_ADDR, asection *));

/* lookup partial symbol by address */

extern struct partial_symbol *
find_pc_psymbol PARAMS ((struct partial_symtab *, CORE_ADDR));

/* lookup partial symbol by address and section */

extern struct partial_symbol *
find_pc_sect_psymbol PARAMS ((struct partial_symtab *, CORE_ADDR, asection *));

extern int
find_pc_line_pc_range PARAMS ((CORE_ADDR, CORE_ADDR *, CORE_ADDR *));

extern int
contained_in PARAMS ((struct block *, struct block *));

extern void
reread_symbols PARAMS ((void));

extern struct type *
lookup_transparent_type PARAMS ((const char *));


/* Macro for name of symbol to indicate a file compiled with gcc. */
#ifndef GCC_COMPILED_FLAG_SYMBOL
#define GCC_COMPILED_FLAG_SYMBOL "gcc_compiled."
#endif

/* Macro for name of symbol to indicate a file compiled with gcc2. */
#ifndef GCC2_COMPILED_FLAG_SYMBOL
#define GCC2_COMPILED_FLAG_SYMBOL "gcc2_compiled."
#endif

/* Functions for dealing with the minimal symbol table, really a misc
   address<->symbol mapping for things we don't have debug symbols for.  */

extern void prim_record_minimal_symbol PARAMS ((const char *, CORE_ADDR,
						enum minimal_symbol_type,
						struct objfile *));

extern struct minimal_symbol *prim_record_minimal_symbol_and_info
  PARAMS ((const char *, CORE_ADDR,
	   enum minimal_symbol_type,
	   char *info, int section,
	   asection *bfd_section,
	   struct objfile *));

#ifdef SOFUN_ADDRESS_MAYBE_MISSING
extern CORE_ADDR find_stab_function_addr PARAMS ((char *,
						  struct partial_symtab *,
						  struct objfile *));
#endif

extern struct minimal_symbol *
lookup_minimal_symbol PARAMS ((const char *, const char *, struct objfile *));

extern struct minimal_symbol *
lookup_minimal_symbol_text PARAMS ((const char *, const char *, struct objfile *));

struct minimal_symbol *
lookup_minimal_symbol_solib_trampoline PARAMS ((const char *,
						const char *,
						struct objfile *));

extern struct minimal_symbol *
lookup_minimal_symbol_by_pc PARAMS ((CORE_ADDR));

extern struct minimal_symbol *
lookup_minimal_symbol_by_pc_section PARAMS ((CORE_ADDR, asection *));

extern struct minimal_symbol *
lookup_solib_trampoline_symbol_by_pc PARAMS ((CORE_ADDR));

extern CORE_ADDR
find_solib_trampoline_target PARAMS ((CORE_ADDR));

extern void
init_minimal_symbol_collection PARAMS ((void));

extern void
discard_minimal_symbols PARAMS ((int));

extern void
install_minimal_symbols PARAMS ((struct objfile *));

/* Sort all the minimal symbols in OBJFILE.  */

extern void msymbols_sort PARAMS ((struct objfile *objfile));

struct symtab_and_line
{
  struct symtab *symtab;
  asection      *section;
  /* Line number.  Line numbers start at 1 and proceed through symtab->nlines.
     0 is never a valid line number; it is used to indicate that line number
     information is not available.  */
  int line;

  CORE_ADDR pc;
  CORE_ADDR end;
};

#define INIT_SAL(sal) { \
  (sal)->symtab  = 0;   \
  (sal)->section = 0;   \
  (sal)->line    = 0;   \
  (sal)->pc      = 0;   \
  (sal)->end     = 0;   \
}

struct symtabs_and_lines
{
  struct symtab_and_line *sals;
  int nelts;
};



/* Some types and macros needed for exception catchpoints.
   Can't put these in target.h because symtab_and_line isn't
   known there. This file will be included by breakpoint.c,
   hppa-tdep.c, etc. */

/* Enums for exception-handling support */
enum exception_event_kind {
  EX_EVENT_THROW,
  EX_EVENT_CATCH
};

/* Type for returning info about an exception */
struct exception_event_record {
  enum exception_event_kind   kind;
  struct symtab_and_line      throw_sal;
  struct symtab_and_line      catch_sal;
  /* This may need to be extended in the future, if
     some platforms allow reporting more information,
     such as point of rethrow, type of exception object,
     type expected by catch clause, etc. */ 
};

#define CURRENT_EXCEPTION_KIND       (current_exception_event->kind)
#define CURRENT_EXCEPTION_CATCH_SAL  (current_exception_event->catch_sal)
#define CURRENT_EXCEPTION_CATCH_LINE (current_exception_event->catch_sal.line)
#define CURRENT_EXCEPTION_CATCH_FILE (current_exception_event->catch_sal.symtab->filename)
#define CURRENT_EXCEPTION_CATCH_PC   (current_exception_event->catch_sal.pc)
#define CURRENT_EXCEPTION_THROW_SAL  (current_exception_event->throw_sal)
#define CURRENT_EXCEPTION_THROW_LINE (current_exception_event->throw_sal.line)
#define CURRENT_EXCEPTION_THROW_FILE (current_exception_event->throw_sal.symtab->filename)
#define CURRENT_EXCEPTION_THROW_PC   (current_exception_event->throw_sal.pc)


/* Given a pc value, return line number it is in.  Second arg nonzero means
   if pc is on the boundary use the previous statement's line number.  */

extern struct symtab_and_line
find_pc_line PARAMS ((CORE_ADDR, int));

/* Same function, but specify a section as well as an address */

extern struct symtab_and_line
find_pc_sect_line PARAMS ((CORE_ADDR, asection *, int));

/* Given an address, return the nearest symbol at or below it in memory.
   Optionally return the symtab it's from through 2nd arg, and the
   address in inferior memory of the symbol through 3rd arg.  */

extern struct symbol *
find_addr_symbol PARAMS ((CORE_ADDR, struct symtab **, CORE_ADDR *));

/* Given a symtab and line number, return the pc there.  */

extern int
find_line_pc PARAMS ((struct symtab *, int, CORE_ADDR *));

extern int 
find_line_pc_range PARAMS ((struct symtab_and_line,
			    CORE_ADDR *, CORE_ADDR *));

extern void
resolve_sal_pc PARAMS ((struct symtab_and_line *));

/* Given a string, return the line specified by it.  For commands like "list"
   and "breakpoint".  */

extern struct symtabs_and_lines
decode_line_spec PARAMS ((char *, int));

extern struct symtabs_and_lines
decode_line_spec_1 PARAMS ((char *, int));

extern struct symtabs_and_lines
decode_line_1 PARAMS ((char **, int, struct symtab *, int, char ***));

#if MAINTENANCE_CMDS

/* Symmisc.c */

void
maintenance_print_symbols PARAMS ((char *, int));

void
maintenance_print_psymbols PARAMS ((char *, int));

void
maintenance_print_msymbols PARAMS ((char *, int));

void
maintenance_print_objfiles PARAMS ((char *, int));

void
maintenance_check_symtabs PARAMS ((char *, int));

/* maint.c */

void
maintenance_print_statistics PARAMS ((char *, int));

#endif

extern void
free_symtab PARAMS ((struct symtab *));

/* Symbol-reading stuff in symfile.c and solib.c.  */

extern struct symtab *
psymtab_to_symtab PARAMS ((struct partial_symtab *));

extern void
clear_solib PARAMS ((void));

extern struct objfile *
symbol_file_add PARAMS ((char *, int, CORE_ADDR, int, int, int, int, int));

/* source.c */

extern int
identify_source_line PARAMS ((struct symtab *, int, int, CORE_ADDR));

extern void
print_source_lines PARAMS ((struct symtab *, int, int, int));

extern void
forget_cached_source_info PARAMS ((void));

extern void
select_source_symtab PARAMS ((struct symtab *));

extern char **make_symbol_completion_list PARAMS ((char *, char *));

extern struct symbol **
make_symbol_overload_list PARAMS ((struct symbol *));

/* symtab.c */

extern struct partial_symtab *
find_main_psymtab PARAMS ((void));

/* blockframe.c */

extern struct blockvector *
blockvector_for_pc PARAMS ((CORE_ADDR, int *));

extern struct blockvector *
blockvector_for_pc_sect PARAMS ((CORE_ADDR, asection *, int *, 
				 struct symtab *));

/* symfile.c */

extern void
clear_symtab_users PARAMS ((void));

extern enum language
deduce_language_from_filename PARAMS ((char *));

/* symtab.c */

extern int
in_prologue PARAMS ((CORE_ADDR pc, CORE_ADDR func_start));

extern struct symbol *
fixup_symbol_section PARAMS ((struct symbol  *, struct objfile *));

/* Symbol searching */

/* When using search_symbols, a list of the following structs is returned.
   Callers must free the search list using free_symbol_search! */
struct symbol_search
{
  /* The block in which the match was found. Could be, for example,
     STATIC_BLOCK or GLOBAL_BLOCK. */
  int block;

  /* Information describing what was found.

     If symtab abd symbol are NOT NULL, then information was found
     for this match. */
  struct symtab *symtab;
  struct symbol *symbol;

  /* If msymbol is non-null, then a match was made on something for
     which only minimal_symbols exist. */
  struct minimal_symbol *msymbol;

  /* A link to the next match, or NULL for the end. */
  struct symbol_search *next;
};

extern void search_symbols PARAMS ((char *, namespace_enum, int, char **, struct symbol_search **));
extern void free_search_symbols PARAMS ((struct symbol_search *));

#endif /* !defined(SYMTAB_H) */
