/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 */

#ifndef lint
static char sccsid[] = "@(#)ld.c	6.10 (Berkeley) 5/22/91";
#endif /* not lint */

/* Linker `ld' for GNU
   Copyright (C) 1988 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Written by Richard Stallman with some help from Eric Albert.
   Set, indirect, and warning symbol features added by Randy Smith.  */
   
/* Define how to initialize system-dependent header fields.  */

#include <ar.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>

/* symseg.h defines the obsolete GNU debugging format; we should nuke it.  */
#define CORE_ADDR unsigned long	/* For symseg.h */
#include "symseg.h"

#define N_SET_MAGIC(exec, val)  ((exec).a_magic = val)

/* If compiled with GNU C, use the built-in alloca */
#ifdef __GNUC__
#define alloca __builtin_alloca
#endif

#define min(a,b) ((a) < (b) ? (a) : (b))

/* Macro to control the number of undefined references printed */
#define MAX_UREFS_PRINTED	10

/* Size of a page; obtained from the operating system.  */

int page_size;

/* Name this program was invoked by.  */

char *progname;

/* System dependencies */

/* Define this to specify the default executable format.  */

#ifndef DEFAULT_MAGIC
#define DEFAULT_MAGIC ZMAGIC
#endif

#ifdef hp300
#define	INITIALIZE_HEADER	outheader.a_mid = MID_HP300
#endif

/* create screwball format for 386BSD to save space on floppies -wfj */
int screwballmode;

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

/* The following #if has been modifed for cross compilation */
/* It originally read:  #if defined(sun) && defined(sparc)  */
/* Marc Ullman, Stanford University    Nov. 1 1989  */
#if defined(sun) && (TARGET == SUN4)
/* Sparc (Sun 4) macros */
#undef relocation_info
#define relocation_info	                reloc_info_sparc
#define RELOC_ADDRESS(r)		((r)->r_address)
#define RELOC_EXTERN_P(r)               ((r)->r_extern)
#define RELOC_TYPE(r)                   ((r)->r_index)
#define RELOC_SYMBOL(r)                 ((r)->r_index)
#define RELOC_MEMORY_SUB_P(r)		0
#define RELOC_MEMORY_ADD_P(r)           0
#define RELOC_ADD_EXTRA(r)              ((r)->r_addend)
#define RELOC_PCREL_P(r)             \
        ((r)->r_type >= RELOC_DISP8 && (r)->r_type <= RELOC_WDISP22)
#define RELOC_VALUE_RIGHTSHIFT(r)       (reloc_target_rightshift[(r)->r_type])
#define RELOC_TARGET_SIZE(r)            (reloc_target_size[(r)->r_type])
#define RELOC_TARGET_BITPOS(r)          0
#define RELOC_TARGET_BITSIZE(r)         (reloc_target_bitsize[(r)->r_type])

/* Note that these are very dependent on the order of the enums in
   enum reloc_type (in a.out.h); if they change the following must be
   changed */
/* Also note that the last few may be incorrect; I have no information */
static int reloc_target_rightshift[] = {
  0, 0, 0, 0, 0, 0, 2, 2, 10, 0, 0, 0, 0, 0, 0,
};
static int reloc_target_size[] = {
  0, 1, 2, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
};
static int reloc_target_bitsize[] = {
  8, 16, 32, 8, 16, 32, 30, 22, 22, 22, 13, 10, 32, 32, 16,
};

#define	MAX_ALIGNMENT	(sizeof (double))
#endif

/* Default macros */
#ifndef RELOC_ADDRESS
#define RELOC_ADDRESS(r)		((r)->r_address)
#define RELOC_EXTERN_P(r)		((r)->r_extern)
#define RELOC_TYPE(r)		((r)->r_symbolnum)
#define RELOC_SYMBOL(r)		((r)->r_symbolnum)
#define RELOC_MEMORY_SUB_P(r)	0
#define RELOC_MEMORY_ADD_P(r)	1
#undef RELOC_ADD_EXTRA
#define RELOC_PCREL_P(r)		((r)->r_pcrel)
#define RELOC_VALUE_RIGHTSHIFT(r)	0
#define RELOC_TARGET_SIZE(r)		((r)->r_length)
#define RELOC_TARGET_BITPOS(r)	0
#define RELOC_TARGET_BITSIZE(r)	32
#endif

#ifndef MAX_ALIGNMENT
#define	MAX_ALIGNMENT	(sizeof (int))
#endif

#ifdef nounderscore
#define LPREFIX '.'
#else
#define LPREFIX 'L'
#endif

#ifndef TEXT_START
#define TEXT_START(x) N_TXTADDR(x)
#endif

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
#endif				/* This is input to LD, in a .o file.  */

#ifndef N_SETB
#define	N_SETB	0x1A		/* Bss set element symbol */
#endif				/* This is input to LD, in a .o file.  */

/* Macros dealing with the set element symbols defined in a.out.h */
#define	SET_ELEMENT_P(x)	((x)>=N_SETA&&(x)<=(N_SETB|N_EXT))
#define TYPE_OF_SET_ELEMENT(x)	((x)-N_SETA+N_ABS)

#ifndef N_SETV
#define N_SETV	0x1C		/* Pointer to set vector in data area.  */
#endif				/* This is output from LD.  */

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

#ifndef __GNU_STAB__

/* Line number for the data section.  This is to be used to describe
   the source location of a variable declaration.  */
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

/* Global symbol data is recorded in these structures,
   one for each global symbol.
   They are found via hashing in 'symtab', which points to a vector of buckets.
   Each bucket is a chain of these structures through the link field.  */

typedef
  struct glosym
    {
      /* Pointer to next symbol in this symbol's hash bucket.  */
      struct glosym *link;
      /* Name of this symbol.  */
      char *name;
      /* Value of this symbol as a global symbol.  */
      long value;
      /* Chain of external 'nlist's in files for this symbol, both defs
	 and refs.  */
      struct nlist *refs;
      /* Any warning message that might be associated with this symbol
         from an N_WARNING symbol encountered. */
      char *warning;
      /* Nonzero means definitions of this symbol as common have been seen,
	 and the value here is the largest size specified by any of them.  */
      int max_common_size;
      /* For relocatable_output, records the index of this global sym in the
	 symbol table to be written, with the first global sym given index 0.*/
      int def_count;
      /* Nonzero means a definition of this global symbol is known to exist.
	 Library members should not be loaded on its account.  */
      char defined;
      /* Nonzero means a reference to this global symbol has been seen
	 in a file that is surely being loaded.
	 A value higher than 1 is the n_type code for the symbol's
	 definition.  */
      char referenced;
      /* A count of the number of undefined references printed for a
	 specific symbol.  If a symbol is unresolved at the end of
	 digest_symbols (and the loading run is supposed to produce
	 relocatable output) do_file_warnings keeps track of how many
	 unresolved reference error messages have been printed for
	 each symbol here.  When the number hits MAX_UREFS_PRINTED,
	 messages stop. */
      unsigned char undef_refs;
      /* 1 means that this symbol has multiple definitions.  2 means
         that it has multiple definitions, and some of them are set
	 elements, one of which has been printed out already.  */
      unsigned char multiply_defined;
      /* Nonzero means print a message at all refs or defs of this symbol */
      char trace;
    }
  symbol;

/* Demangler for C++. */
extern char *cplus_demangle ();

/* Demangler function to use. */
char *(*demangler)() = NULL;

/* Number of buckets in symbol hash table */
#define	TABSIZE	1009

/* The symbol hash table: a vector of TABSIZE pointers to struct glosym. */
symbol *symtab[TABSIZE];

/* Number of symbols in symbol hash table. */
int num_hash_tab_syms = 0;

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

/* Count the number of set element type symbols and the number of
   separate vectors which these symbols will fit into.  See the
   GNU a.out.h for more info.
   This count is computed by 'enter_file_symbols' */
int set_symbol_count;
int set_vector_count;

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

/* Count the number of definitions done indirectly (ie. done relative
   to the value of some other symbol. */
int global_indirect_count;

/* Count the number of warning symbols encountered. */
int warning_count;

/* Total number of symbols to be written in the output file.
   Computed by digest_symbols from the variables above.  */
int nsyms;


/* Nonzero means ptr to symbol entry for symbol to use as start addr.
   -e sets this.  */
symbol *entry_symbol;

symbol *edata_symbol;   /* the symbol _edata */
symbol *etext_symbol;   /* the symbol _etext */
symbol *end_symbol;	/* the symbol _end */

/* Each input file, and each library member ("subfile") being loaded,
   has a `file_entry' structure for it.

   For files specified by command args, these are contained in the vector
   which `file_table' points to.

   For library members, they are dynamically allocated,
   and chained through the `chain' field.
   The chain is found in the `subfiles' field of the `file_entry'.
   The `file_entry' objects for the members have `superfile' fields pointing
   to the one for the library.  */

struct file_entry {
  /* Name of this file.  */
  char *filename;
  /* Name to use for the symbol giving address of text start */
  /* Usually the same as filename, but for a file spec'd with -l
     this is the -l switch itself rather than the filename.  */
  char *local_sym_name;

  /* Describe the layout of the contents of the file */

  /* The file's a.out header.  */
  struct exec header;
  /* Offset in file of GDB symbol segment, or 0 if there is none.  */
  int symseg_offset;

  /* Describe data from the file loaded into core */

  /* Symbol table of the file.  */
  struct nlist *symbols;
  /* Size in bytes of string table.  */
  int string_size;
  /* Pointer to the string table.
     The string table is not kept in core all the time,
     but when it is in core, its address is here.  */
  char *strings;

  /* Next two used only if `relocatable_output' or if needed for */
  /* output of undefined reference line numbers. */

  /* Text reloc info saved by `write_text' for `coptxtrel'.  */
  struct relocation_info *textrel;
  /* Data reloc info saved by `write_data' for `copdatrel'.  */
  struct relocation_info *datarel;

  /* Relation of this file's segments to the output file */

  /* Start of this file's text seg in the output file core image.  */
  int text_start_address;
  /* Start of this file's data seg in the output file core image.  */
  int data_start_address;
  /* Start of this file's bss seg in the output file core image.  */
  int bss_start_address;
  /* Offset in bytes in the output file symbol table
     of the first local symbol for this file.  Set by `write_file_symbols'.  */
  int local_syms_offset;

  /* For library members only */

  /* For a library, points to chain of entries for the library members.  */
  struct file_entry *subfiles;
  /* For a library member, offset of the member within the archive.
     Zero for files that are not library members.  */
  int starting_offset;
  /* Size of contents of this file, if library member.  */
  int total_size;
  /* For library member, points to the library's own entry.  */
  struct file_entry *superfile;
  /* For library member, points to next entry for next member.  */
  struct file_entry *chain;

  /* 1 if file is a library. */
  char library_flag;

  /* 1 if file's header has been read into this structure.  */
  char header_read_flag;

  /* 1 means search a set of directories for this file.  */
  char search_dirs_flag;

  /* 1 means this is base file of incremental load.
     Do not load this file's text or data.
     Also default text_start to after this file's bss. */
  char just_syms_flag;
};

/* Vector of entries for input files specified by arguments.
   These are all the input files except for members of specified libraries.  */
struct file_entry *file_table;

/* Length of that vector.  */
int number_of_files;

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

#ifdef COFF_ENCAPSULATE
struct coffheader coffheader;
int need_coff_header;
#endif

/* The following are computed by `digest_symbols'.  */

int text_size;		/* total size of text of all input files.  */
int data_size;		/* total size of data of all input files.  */
int bss_size;		/* total size of bss of all input files.  */
int text_reloc_size;	/* total size of text relocation of all input files.  */
int data_reloc_size;	/* total size of data relocation of all input */
			/* files.  */

/* Specifications of start and length of the area reserved at the end
   of the text segment for the set vectors.  Computed in 'digest_symbols' */
int set_sect_start;
int set_sect_size;

/* Pointer for in core storage for the above vectors, before they are
   written. */
unsigned long *set_vectors;

/* Amount of cleared space to leave between the text and data segments.  */

int text_pad;

/* Amount of bss segment to include as part of the data segment.  */

int data_pad;

/* Format of __.SYMDEF:
   First, a longword containing the size of the 'symdef' data that follows.
   Second, zero or more 'symdef' structures.
   Third, a longword containing the length of symbol name strings.
   Fourth, zero or more symbol name strings (each followed by a null).  */

struct symdef {
  int symbol_name_string_index;
  int library_member_offset;
};

/* Record most of the command options.  */

/* Address we assume the text section will be loaded at.
   We relocate symbols and text and data for this, but we do not
   write any padding in the output file for it.  */
int text_start;

/* Offset of default entry-pc within the text section.  */
int entry_offset;

/* Address we decide the data section will be loaded at.  */
int data_start;

/* `text-start' address is normally this much plus a page boundary.
   This is not a user option; it is fixed for each system.  */
int text_start_alignment;

/* Nonzero if -T was specified in the command line.
   This prevents text_start from being set later to default values.  */
int T_flag_specified;

/* Nonzero if -Tdata was specified in the command line.
   This prevents data_start from being set later to default values.  */
int Tdata_flag_specified;

/* Size to pad data section up to.
   We simply increase the size of the data section, padding with zeros,
   and reduce the size of the bss section to match.  */
int specified_data_size;

/* Magic number to use for the output file, set by switch.  */
int magic;

/* Nonzero means print names of input files as processed.  */
int trace_files;

/* Which symbols should be stripped (omitted from the output):
   none, all, or debugger symbols.  */
enum { STRIP_NONE, STRIP_ALL, STRIP_DEBUGGER } strip_symbols;

/* Which local symbols should be omitted:
   none, all, or those starting with L.
   This is irrelevant if STRIP_NONE.  */
enum { DISCARD_NONE, DISCARD_ALL, DISCARD_L } discard_locals;

/* 1 => write load map.  */
int write_map;

/* 1 => write relocation into output file so can re-input it later.  */
int relocatable_output;

/* 1 => assign space to common symbols even if `relocatable_output'.  */
int force_common_definition;

/* Standard directories to search for files specified by -l.  */
char *standard_search_dirs[] =
#ifdef STANDARD_SEARCH_DIRS
  {STANDARD_SEARCH_DIRS};
#else
#ifdef NON_NATIVE
  {"/usr/local/lib/gnu"};
#else
  {"/lib", "/usr/lib", "/usr/local/lib"};
#endif
#endif

/* Actual vector of directories to search;
   this contains those specified with -L plus the standard ones.  */
char **search_dirs;

/* Length of the vector `search_dirs'.  */
int n_search_dirs;

/* Non zero means to create the output executable. */
/* Cleared by nonfatal errors.  */
int make_executable;

/* Force the executable to be output, even if there are non-fatal
   errors */
int force_executable;

/* Keep a list of any symbols referenced from the command line (so
   that error messages for these guys can be generated). This list is
   zero terminated. */
struct glosym **cmdline_references;
int cl_refs_allocated;

void bcopy (), bzero ();
int malloc (), realloc ();
#ifndef alloca
int alloca ();
#endif
int free ();

int xmalloc ();
int xrealloc ();
void fatal ();
void fatal_with_file ();
void perror_name ();
void perror_file ();
void error ();

void digest_symbols ();
void print_symbols ();
void load_symbols ();
void decode_command ();
void list_undefined_symbols ();
void list_unresolved_references ();
void write_output ();
void write_header ();
void write_text ();
void read_file_relocation ();
void write_data ();
void write_rel ();
void write_syms ();
void write_symsegs ();
void mywrite ();
void symtab_init ();
void padfile ();
char *concat ();
char *get_file_name ();
symbol *getsym (), *getsym_soft ();

int
main (argc, argv)
     char **argv;
     int argc;
{
/*   Added this to stop ld core-dumping on very large .o files.    */
#ifdef RLIMIT_STACK
  /* Get rid of any avoidable limit on stack size.  */
  {
    struct rlimit rlim;

    /* Set the stack limit huge so that alloca does not fail. */
    getrlimit (RLIMIT_STACK, &rlim);
    rlim.rlim_cur = rlim.rlim_max;
    setrlimit (RLIMIT_STACK, &rlim);
  }
#endif /* RLIMIT_STACK */

  page_size = getpagesize ();
  progname = argv[0];

  /* Clear the cumulative info on the output file.  */

  text_size = 0;
  data_size = 0;
  bss_size = 0;
  text_reloc_size = 0;
  data_reloc_size = 0;

  data_pad = 0;
  text_pad = 0;

  /* Initialize the data about options.  */

  specified_data_size = 0;
  strip_symbols = STRIP_NONE;
  trace_files = 0;
  discard_locals = DISCARD_NONE;
  entry_symbol = 0;
  write_map = 0;
  relocatable_output = 0;
  force_common_definition = 0;
  T_flag_specified = 0;
  Tdata_flag_specified = 0;
  magic = DEFAULT_MAGIC;
  make_executable = 1;
  force_executable = 0;
  set_element_prefixes = 0;

  /* Initialize the cumulative counts of symbols.  */

  local_sym_count = 0;
  non_L_local_sym_count = 0;
  debugger_sym_count = 0;
  undefined_global_sym_count = 0;
  set_symbol_count = 0;
  set_vector_count = 0;
  global_indirect_count = 0;
  warning_count = 0;
  multiple_def_count = 0;
  common_defined_global_count = 0;

  /* Keep a list of symbols referenced from the command line */
  cl_refs_allocated = 10;
  cmdline_references
    = (struct glosym **) xmalloc (cl_refs_allocated
				  * sizeof(struct glosym *));
  *cmdline_references = 0;

  /* Completely decode ARGV.  */

  decode_command (argc, argv);

  /* Create the symbols `etext', `edata' and `end'.  */

  if (!relocatable_output)
    symtab_init ();

  /* Determine whether to count the header as part of
     the text size, and initialize the text size accordingly.
     This depends on the kind of system and on the output format selected.  */

  N_SET_MAGIC (outheader, magic);
#ifdef INITIALIZE_HEADER
  INITIALIZE_HEADER;
#endif

  text_size = sizeof (struct exec);
#ifdef COFF_ENCAPSULATE
  if (relocatable_output == 0 && file_table[0].just_syms_flag == 0)
    {
      need_coff_header = 1;
      /* set this flag now, since it will change the values of N_TXTOFF, etc */
      N_SET_FLAGS (outheader, N_FLAGS_COFF_ENCAPSULATE);
      text_size += sizeof (struct coffheader);
    }
#endif

  text_size -= N_TXTOFF (outheader);

  if (text_size < 0)
    text_size = 0;
  entry_offset = text_size;

  if (!T_flag_specified && !relocatable_output && !screwballmode)
    text_start = TEXT_START (outheader);

  /* The text-start address is normally this far past a page boundary.  */
  text_start_alignment = text_start % page_size;

  /* Load symbols of all input files.
     Also search all libraries and decide which library members to load.  */

  load_symbols ();

  /* Compute where each file's sections go, and relocate symbols.  */

  digest_symbols ();

  /* Print error messages for any missing symbols, for any warning
     symbols, and possibly multiple definitions */

  do_warnings (stderr);

  /* Print a map, if requested.  */

  if (write_map) print_symbols (stdout);

  /* Write the output file.  */

  if (make_executable || force_executable)
    write_output ();

  exit (!make_executable);
}

void decode_option ();

/* Analyze a command line argument.
   Return 0 if the argument is a filename.
   Return 1 if the argument is a option complete in itself.
   Return 2 if the argument is a option which uses an argument.

   Thus, the value is the number of consecutive arguments
   that are part of options.  */

int
classify_arg (arg)
     register char *arg;
{
  if (*arg != '-') return 0;
  switch (arg[1])
    {
    case 'A':
    case 'D':
    case 'e':
    case 'L':
    case 'l':
    case 'o':
    case 'u':
    case 'V':
    case 'y':
      if (arg[2])
	return 1;
      return 2;

    case 'B':
      if (! strcmp (&arg[2], "static"))
	return 1;

    case 'T':
      if (arg[2] == 0)
	return 2;
      if (! strcmp (&arg[2], "text"))
	return 2;
      if (! strcmp (&arg[2], "data"))
	return 2;
      return 1;
    }

  return 1;
}

/* Process the command arguments,
   setting up file_table with an entry for each input file,
   and setting variables according to the options.  */

void
decode_command (argc, argv)
     char **argv;
     int argc;
{
  register int i;
  register struct file_entry *p;
  char *cp;

  number_of_files = 0;
  output_filename = "a.out";

  n_search_dirs = 0;
  search_dirs = (char **) xmalloc (sizeof (char *));

  /* First compute number_of_files so we know how long to make file_table.  */
  /* Also process most options completely.  */

  for (i = 1; i < argc; i++)
    {
      register int code = classify_arg (argv[i]);
      if (code)
	{
	  if (i + code > argc)
	    fatal ("no argument following %s\n", argv[i]);

	  decode_option (argv[i], argv[i+1]);

	  if (argv[i][1] == 'l' || argv[i][1] == 'A')
	    number_of_files++;

	  i += code - 1;
	}
      else
	number_of_files++;
    }

  if (!number_of_files)
    fatal ("no input files", 0);

  p = file_table
    = (struct file_entry *) xmalloc (number_of_files * sizeof (struct file_entry));
  bzero (p, number_of_files * sizeof (struct file_entry));

  /* Now scan again and fill in file_table.  */
  /* All options except -A and -l are ignored here.  */

  for (i = 1; i < argc; i++)
    {
      register int code = classify_arg (argv[i]);

      if (code)
	{
	  char *string;
	  if (code == 2)
	    string = argv[i+1];
	  else
	    string = &argv[i][2];

	  if (argv[i][1] == 'A')
	    {
	      if (p != file_table)
		fatal ("-A specified before an input file other than the first");

	      p->filename = string;
	      p->local_sym_name = string;
	      p->just_syms_flag = 1;
	      p++;
	    }
	  if (argv[i][1] == 'l')
	    {
	      if (cp = rindex(string, '/'))
		{
		  *cp++ = '\0';
		  cp = concat (string, "/lib", cp);
		  p->filename = concat (cp, ".a", "");
	        }
	      else
	        p->filename = concat ("lib", string, ".a");

	      p->local_sym_name = concat ("-l", string, "");
	      p->search_dirs_flag = 1;
	      p++;
	    }
	  i += code - 1;
	}
      else
	{
	  p->filename = argv[i];
	  p->local_sym_name = argv[i];
	  p++;
	}
    }

  /* Now check some option settings for consistency.  */

#ifdef NMAGIC
  if ((magic == ZMAGIC || magic == NMAGIC)
#else
  if ((magic == ZMAGIC)
#endif
      && (text_start - text_start_alignment) & (page_size - 1))
    fatal ("-T argument not multiple of page size, with sharable output", 0);

  /* Append the standard search directories to the user-specified ones.  */
  {
    int n = sizeof standard_search_dirs / sizeof standard_search_dirs[0];
    n_search_dirs += n;
    search_dirs
      = (char **) xrealloc (search_dirs, n_search_dirs * sizeof (char *));
    bcopy (standard_search_dirs, &search_dirs[n_search_dirs - n],
	   n * sizeof (char *));
  }
}


void
add_cmdline_ref (sp)
     struct glosym *sp;
{
  struct glosym **ptr;

  for (ptr = cmdline_references;
       ptr < cmdline_references + cl_refs_allocated && *ptr;
       ptr++)
    ;

  if (ptr >= cmdline_references + cl_refs_allocated - 1)
    {
      int diff = ptr - cmdline_references;

      cl_refs_allocated *= 2;
      cmdline_references = (struct glosym **)
	xrealloc (cmdline_references,
		 cl_refs_allocated * sizeof (struct glosym *));
      ptr = cmdline_references + diff;
    }

  *ptr++ = sp;
  *ptr = (struct glosym *) 0;
}

int
set_element_prefixed_p (name)
     char *name;
{
  struct string_list_element *p;
  int i;

  for (p = set_element_prefixes; p; p = p->next)
    {
      for (i = 0; p->str[i] != '\0' && (p->str[i] == name[i]); i++)
	;

      if (p->str[i] == '\0')
	return 1;
    }
  return 0;
}

int parse ();

/* Record an option and arrange to act on it later.
   ARG should be the following command argument,
   which may or may not be used by this option.

   The `l' and `A' options are ignored here since they actually
   specify input files.  */

void
decode_option (swt, arg)
     register char *swt, *arg;
{
  /* We get Bstatic from gcc on suns.  */
  if (! strcmp (swt + 1, "Bstatic"))
    return;
  if (! strcmp (swt + 1, "Ttext"))
    {
      text_start = parse (arg, "%x", "invalid argument to -Ttext");
      T_flag_specified = 1;
      return;
    }
  if (! strcmp (swt + 1, "Tdata"))
    {
      data_start = parse (arg, "%x", "invalid argument to -Tdata");
      Tdata_flag_specified = 1;
      return;
    }
  if (! strcmp (swt + 1, "noinhibit-exec"))
    {
      force_executable = 1;
      return;
    }
  if (! strcmp (swt + 1, "screwballmode"))
    {
      screwballmode = 1;
      magic = OMAGIC;
      text_start = sizeof(struct exec);
      return;
    }

  if (swt[2] != 0)
    arg = &swt[2];

  switch (swt[1])
    {
    case 'A':
      return;

    case 'D':
      specified_data_size = parse (arg, "%x", "invalid argument to -D");
      return;

    case 'd':
      force_common_definition = 1;
      return;

    case 'e':
      entry_symbol = getsym (arg);
      if (!entry_symbol->defined && !entry_symbol->referenced)
	undefined_global_sym_count++;
      entry_symbol->referenced = 1;
      add_cmdline_ref (entry_symbol);
      return;

    case 'l':
      /* If linking with libg++, use the C++ demangler. */
      if (arg != NULL && strcmp (arg, "g++") == 0)
	demangler = cplus_demangle;
      return;
      magic = OMAGIC;

    case 'L':
      n_search_dirs++;
      search_dirs
	= (char **) xrealloc (search_dirs, n_search_dirs * sizeof (char *));
      search_dirs[n_search_dirs - 1] = arg;
      return;

    case 'M':
      write_map = 1;
      return;

    case 'N':
      magic = OMAGIC;
#ifdef notnow
text_start = sizeof(struct exec); /* XXX */
screwballmode=1;
#endif
      return;

#ifdef NMAGIC
    case 'n':
      magic = NMAGIC;
      return;
#endif

    case 'o':
      output_filename = arg;
      return;

    case 'r':
      relocatable_output = 1;
      magic = OMAGIC;
      text_start = 0;
      return;

    case 'S':
      strip_symbols = STRIP_DEBUGGER;
      return;

    case 's':
      strip_symbols = STRIP_ALL;
      return;

    case 'T':
      text_start = parse (arg, "%x", "invalid argument to -T");
      T_flag_specified = 1;
      return;

    case 't':
      trace_files = 1;
      return;

    case 'u':
      {
	register symbol *sp = getsym (arg);
	if (!sp->defined && !sp->referenced)
	  undefined_global_sym_count++;
	sp->referenced = 1;
	add_cmdline_ref (sp);
      }
      return;

    case 'V':
      {
	struct string_list_element *new
	  = (struct string_list_element *)
	    xmalloc (sizeof (struct string_list_element));

	new->str = arg;
	new->next = set_element_prefixes;
	set_element_prefixes = new;
	return;
      }

    case 'X':
      discard_locals = DISCARD_L;
      return;

    case 'x':
      discard_locals = DISCARD_ALL;
      return;

    case 'y':
      {
	register symbol *sp = getsym (&swt[2]);
	sp->trace = 1;
      }
      return;

    case 'z':
      magic = ZMAGIC;
      return;

    default:
      fatal ("invalid command option `%s'", swt);
    }
}

/** Convenient functions for operating on one or all files being */
 /** loaded.  */
void print_file_name ();

/* Call FUNCTION on each input file entry.
   Do not call for entries for libraries;
   instead, call once for each library member that is being loaded.

   FUNCTION receives two arguments: the entry, and ARG.  */

void
each_file (function, arg)
     register void (*function)();
     register int arg;
{
  register int i;

  for (i = 0; i < number_of_files; i++)
    {
      register struct file_entry *entry = &file_table[i];
      if (entry->library_flag)
        {
	  register struct file_entry *subentry = entry->subfiles;
	  for (; subentry; subentry = subentry->chain)
	    (*function) (subentry, arg);
	}
      else
	(*function) (entry, arg);
    }
}

/* Call FUNCTION on each input file entry until it returns a non-zero
   value.  Return this value.
   Do not call for entries for libraries;
   instead, call once for each library member that is being loaded.

   FUNCTION receives two arguments: the entry, and ARG.  It must be a
   function returning unsigned long (though this can probably be fudged). */

unsigned long
check_each_file (function, arg)
     register unsigned long (*function)();
     register int arg;
{
  register int i;
  register unsigned long return_val;

  for (i = 0; i < number_of_files; i++)
    {
      register struct file_entry *entry = &file_table[i];
      if (entry->library_flag)
        {
	  register struct file_entry *subentry = entry->subfiles;
	  for (; subentry; subentry = subentry->chain)
	    if (return_val = (*function) (subentry, arg))
	      return return_val;
	}
      else
	if (return_val = (*function) (entry, arg))
	  return return_val;
    }
  return 0;
}

/* Like `each_file' but ignore files that were just for symbol definitions.  */

void
each_full_file (function, arg)
     register void (*function)();
     register int arg;
{
  register int i;

  for (i = 0; i < number_of_files; i++)
    {
      register struct file_entry *entry = &file_table[i];
      if (entry->just_syms_flag)
	continue;
      if (entry->library_flag)
        {
	  register struct file_entry *subentry = entry->subfiles;
	  for (; subentry; subentry = subentry->chain)
	    (*function) (subentry, arg);
	}
      else
	(*function) (entry, arg);
    }
}

/* Close the input file that is now open.  */

void
file_close ()
{
  close (input_desc);
  input_desc = 0;
  input_file = 0;
}

/* Open the input file specified by 'entry', and return a descriptor.
   The open file is remembered; if the same file is opened twice in a row,
   a new open is not actually done.  */

int
file_open (entry)
     register struct file_entry *entry;
{
  register int desc;

  if (entry->superfile)
    return file_open (entry->superfile);

  if (entry == input_file)
    return input_desc;

  if (input_file) file_close ();

  if (entry->search_dirs_flag)
    {
      int i;

      for (i = 0; i < n_search_dirs; i++)
	{
	  register char *string
	    = concat (search_dirs[i], "/", entry->filename);
	  desc = open (string, O_RDONLY, 0);
	  if (desc > 0)
	    {
	      entry->filename = string;
	      entry->search_dirs_flag = 0;
	      break;
	    }
	  free (string);
	}
    }
  else
    desc = open (entry->filename, O_RDONLY, 0);

  if (desc > 0)
    {
      input_file = entry;
      input_desc = desc;
      return desc;
    }

  perror_file (entry);
  /* NOTREACHED */
}

/* Print the filename of ENTRY on OUTFILE (a stdio stream),
   and then a newline.  */

void
prline_file_name (entry, outfile)
     struct file_entry *entry;
     FILE *outfile;
{
  print_file_name (entry, outfile);
  fprintf (outfile, "\n");
}

/* Print the filename of ENTRY on OUTFILE (a stdio stream).  */

void
print_file_name (entry, outfile)
     struct file_entry *entry;
     FILE *outfile;
{
  if (entry->superfile)
    {
      print_file_name (entry->superfile, outfile);
      fprintf (outfile, "(%s)", entry->filename);
    }
  else
    fprintf (outfile, "%s", entry->filename);
}

/* Return the filename of entry as a string (malloc'd for the purpose) */

char *
get_file_name (entry)
     struct file_entry *entry;
{
  char *result, *supfile;
  if (entry->superfile)
    {
      supfile = get_file_name (entry->superfile);
      result = (char *) xmalloc (strlen (supfile)
				 + strlen (entry->filename) + 3);
      sprintf (result, "%s(%s)", supfile, entry->filename);
      free (supfile);
    }
  else
    {
      result = (char *) xmalloc (strlen (entry->filename) + 1);
      strcpy (result, entry->filename);
    }
  return result;
}

/* Medium-level input routines for rel files.  */

/* Read a file's header into the proper place in the file_entry.
   DESC is the descriptor on which the file is open.
   ENTRY is the file's entry.  */

void
read_header (desc, entry)
     int desc;
     register struct file_entry *entry;
{
  register int len;
  struct exec *loc = (struct exec *) &entry->header;

  lseek (desc, entry->starting_offset, 0);
#ifdef COFF_ENCAPSULATE
  if (entry->just_syms_flag)
    lseek (desc, sizeof(coffheader), 1);
#endif
  len = read (desc, loc, sizeof (struct exec));
  if (len != sizeof (struct exec))
    fatal_with_file ("failure reading header of ", entry);
  if (N_BADMAG (*loc))
    fatal_with_file ("bad magic number in ", entry);

  entry->header_read_flag = 1;
}

/* Read the symbols of file ENTRY into core.
   Assume it is already open, on descriptor DESC.
   Also read the length of the string table, which follows the symbol table,
   but don't read the contents of the string table.  */

void
read_entry_symbols (desc, entry)
     struct file_entry *entry;
     int desc;
{
  int str_size;

  if (!entry->header_read_flag)
    read_header (desc, entry);

  entry->symbols = (struct nlist *) xmalloc (entry->header.a_syms);

  lseek (desc, N_SYMOFF (entry->header) + entry->starting_offset, 0);
  if (entry->header.a_syms != read (desc, entry->symbols, entry->header.a_syms))
    fatal_with_file ("premature end of file in symbols of ", entry);

  lseek (desc, N_STROFF (entry->header) + entry->starting_offset, 0);
  if (sizeof str_size != read (desc, &str_size, sizeof str_size))
    fatal_with_file ("bad string table size in ", entry);

  entry->string_size = str_size;
}

/* Read the string table of file ENTRY into core.
   Assume it is already open, on descriptor DESC.
   Also record whether a GDB symbol segment follows the string table.  */

void
read_entry_strings (desc, entry)
     struct file_entry *entry;
     int desc;
{
  int buffer;

  if (!entry->header_read_flag)
    read_header (desc, entry);

  lseek (desc, N_STROFF (entry->header) + entry->starting_offset, 0);
  if (entry->string_size != read (desc, entry->strings, entry->string_size))
    fatal_with_file ("premature end of file in strings of ", entry);

#if 0
  /* While we are here, see if the file has a symbol segment at the end.
     For a separate file, just try reading some more.
     For a library member, compare current pos against total size.  */
  if (entry->superfile)
    {
      if (entry->total_size == N_STROFF (entry->header) + entry->string_size)
	return;
    }
  else
    {
      buffer = read (desc, &buffer, sizeof buffer);
      if (buffer == 0)
	return;
      if (buffer != sizeof buffer)
	fatal_with_file ("premature end of file in GDB symbol segment of ", entry);
    }
#endif
  /* Don't try to do anything with symsegs.  */
  return;
#if 0
  /* eliminate warning of `statement not reached'.  */
  entry->symseg_offset = N_STROFF (entry->header) + entry->string_size;
#endif
}

/* Read in the symbols of all input files.  */

void read_file_symbols (), read_entry_symbols (), read_entry_strings ();
void enter_file_symbols (), enter_global_ref (), search_library ();

void
load_symbols ()
{
  register int i;

  if (trace_files) fprintf (stderr, "Loading symbols:\n\n");

  for (i = 0; i < number_of_files; i++)
    {
      register struct file_entry *entry = &file_table[i];
      read_file_symbols (entry);
    }

  if (trace_files) fprintf (stderr, "\n");
}

/* If ENTRY is a rel file, read its symbol and string sections into core.
   If it is a library, search it and load the appropriate members
   (which means calling this function recursively on those members).  */

void
read_file_symbols (entry)
     register struct file_entry *entry;
{
  register int desc;
  register int len;
  struct exec hdr;

  desc = file_open (entry);

#ifdef COFF_ENCAPSULATE
  if (entry->just_syms_flag)
    lseek (desc, sizeof(coffheader),0);
#endif

  len = read (desc, &hdr, sizeof hdr);
  if (len != sizeof hdr)
    fatal_with_file ("failure reading header of ", entry);

  if (!N_BADMAG (hdr))
    {
      read_entry_symbols (desc, entry);
      entry->strings = (char *) alloca (entry->string_size);
      read_entry_strings (desc, entry);
      enter_file_symbols (entry);
      entry->strings = 0;
    }
  else
    {
      char armag[SARMAG];

      lseek (desc, 0, 0);
      if (SARMAG != read (desc, armag, SARMAG) || strncmp (armag, ARMAG, SARMAG))
	fatal_with_file ("malformed input file (not rel or archive) ", entry);
      entry->library_flag = 1;
      search_library (desc, entry);
    }

  file_close ();
}

/* Enter the external symbol defs and refs of ENTRY in the hash table.  */

void
enter_file_symbols (entry)
     struct file_entry *entry;
{
  register struct nlist
    *p,
    *end = entry->symbols + entry->header.a_syms / sizeof (struct nlist);

  if (trace_files) prline_file_name (entry, stderr);

  for (p = entry->symbols; p < end; p++)
    {
      if (p->n_type == (N_SETV | N_EXT)) continue;
      if (set_element_prefixes
	  && set_element_prefixed_p (p->n_un.n_strx + entry->strings))
	p->n_type += (N_SETA - N_ABS);

      if (SET_ELEMENT_P (p->n_type))
	{
	  set_symbol_count++;
	  if (!relocatable_output)
	    enter_global_ref (p, p->n_un.n_strx + entry->strings, entry);
	}
      else if (p->n_type == N_WARNING)
	{
	  char *name = p->n_un.n_strx + entry->strings;

	  /* Grab the next entry.  */
	  p++;
	  if (p->n_type != (N_UNDF | N_EXT))
	    {
	      fprintf (stderr, "%s: Warning symbol found in %s without external reference following.\n",
		       progname, entry->filename);
	      make_executable = 0;
	      p--;		/* Process normally.  */
	    }
	  else
	    {
	      symbol *sp;
	      char *sname = p->n_un.n_strx + entry->strings;
	      /* Deal with the warning symbol.  */
	      enter_global_ref (p, p->n_un.n_strx + entry->strings, entry);
	      sp = getsym (sname);
	      sp->warning = (char *) xmalloc (strlen(name) + 1);
	      strcpy (sp->warning, name);
	      warning_count++;
	    }
	}
      else if (p->n_type & N_EXT)
	enter_global_ref (p, p->n_un.n_strx + entry->strings, entry);
      else if (p->n_un.n_strx && !(p->n_type & (N_STAB | N_EXT)))
	{
	  if ((p->n_un.n_strx + entry->strings)[0] != LPREFIX)
	    non_L_local_sym_count++;
	  local_sym_count++;
	}
      else debugger_sym_count++;
    }

   /* Count one for the local symbol that we generate,
      whose name is the file's name (usually) and whose address
      is the start of the file's text.  */

  local_sym_count++;
  non_L_local_sym_count++;
}

/* Enter one global symbol in the hash table.
   NLIST_P points to the `struct nlist' read from the file
   that describes the global symbol.  NAME is the symbol's name.
   ENTRY is the file entry for the file the symbol comes from.

   The `struct nlist' is modified by placing it on a chain of
   all such structs that refer to the same global symbol.
   This chain starts in the `refs' field of the symbol table entry
   and is chained through the `n_name'.  */

void
enter_global_ref (nlist_p, name, entry)
     register struct nlist *nlist_p;
     char *name;
     struct file_entry *entry;
{
  register symbol *sp = getsym (name);
  register int type = nlist_p->n_type;
  int oldref = sp->referenced;
  int olddef = sp->defined;

  nlist_p->n_un.n_name = (char *) sp->refs;
  sp->refs = nlist_p;

  sp->referenced = 1;
  if (type != (N_UNDF | N_EXT) || nlist_p->n_value)
    {
      if (!sp->defined || sp->defined == (N_UNDF | N_EXT))
	sp->defined = type;

      if (oldref && !olddef)
	/* It used to be undefined and we're defining it.  */
	undefined_global_sym_count--;

      if (!olddef && type == (N_UNDF | N_EXT) && nlist_p->n_value)
	{
	  /* First definition and it's common.  */
	  common_defined_global_count++;
	  sp->max_common_size = nlist_p->n_value;
	}
      else if (olddef && sp->max_common_size && type != (N_UNDF | N_EXT))
	{
	  /* It used to be common and we're defining it as
	     something else.  */
	  common_defined_global_count--;
	  sp->max_common_size = 0;
	}
      else if (olddef && sp->max_common_size && type == (N_UNDF | N_EXT)
	  && sp->max_common_size < nlist_p->n_value)
	/* It used to be common and this is a new common entry to
	   which we need to pay attention.  */
	sp->max_common_size = nlist_p->n_value;

      /* Are we defining it as a set element?  */
      if (SET_ELEMENT_P (type)
	  && (!olddef || (olddef && sp->max_common_size)))
	set_vector_count++;
      /* As an indirection?  */
      else if (type == (N_INDR | N_EXT))
	{
	  /* Indirect symbols value should be modified to point
	     a symbol being equivalenced to. */
	  nlist_p->n_value
	    = (unsigned int) getsym ((nlist_p + 1)->n_un.n_strx
				     + entry->strings);
	  if ((symbol *) nlist_p->n_value == sp)
	    {
	      /* Somebody redefined a symbol to be itself.  */
	      fprintf (stderr, "%s: Symbol %s indirected to itself.\n",
		       entry->filename, name);
	      /* Rewrite this symbol as being a global text symbol
		 with value 0.  */
	      nlist_p->n_type = sp->defined = N_TEXT | N_EXT;
	      nlist_p->n_value = 0;
	      /* Don't make the output executable.  */
	      make_executable = 0;
	    }
	  else
	    global_indirect_count++;
	}
    }
  else
    if (!oldref)
#ifndef DOLLAR_KLUDGE
      undefined_global_sym_count++;
#else
      {
	if (entry->superfile && type == (N_UNDF | N_EXT) && name[1] == '$')
	  {
	    /* This is an (ISI?) $-conditional; skip it */
	    sp->referenced = 0;
	    if (sp->trace)
	      {
		fprintf (stderr, "symbol %s is a $-conditional ignored in ", sp->name);
		print_file_name (entry, stderr);
		fprintf (stderr, "\n");
	      }
	    return;
	  }
	else
	  undefined_global_sym_count++;
      }
#endif

  if (sp == end_symbol && entry->just_syms_flag && !T_flag_specified
	&& !screwballmode)
    text_start = nlist_p->n_value;

  if (sp->trace)
    {
      register char *reftype;
      switch (type & N_TYPE)
	{
	case N_UNDF:
	  if (nlist_p->n_value)
	    reftype = "defined as common";
	  else reftype = "referenced";
	  break;

	case N_ABS:
	  reftype = "defined as absolute";
	  break;

	case N_TEXT:
	  reftype = "defined in text section";
	  break;

	case N_DATA:
	  reftype = "defined in data section";
	  break;

	case N_BSS:
	  reftype = "defined in BSS section";
	  break;

	case N_SETT:
	  reftype = "is a text set element";
	  break;

	case N_SETD:
	  reftype = "is a data set element";
	  break;

	case N_SETB:
	  reftype = "is a BSS set element";
	  break;

	case N_SETA:
	  reftype = "is an absolute set element";
	  break;

	case N_SETV:
	  reftype = "defined in data section as vector";
	  break;

	case N_INDR:
	  reftype = (char *) alloca (23
				     + strlen ((nlist_p + 1)->n_un.n_strx
					       + entry->strings));
	  sprintf (reftype, "defined equivalent to %s",
		   (nlist_p + 1)->n_un.n_strx + entry->strings);
	  break;

#ifdef sequent
	case N_SHUNDF:
	  reftype = "shared undf";
	  break;

/* These conflict with cases above.
	case N_SHDATA:
	  reftype = "shared data";
	  break;

	case N_SHBSS:
	  reftype = "shared BSS";
	  break;
*/
	default:
	  reftype = "I don't know this type";
	  break;
#endif
	}

      fprintf (stderr, "symbol %s %s in ", sp->name, reftype);
      print_file_name (entry, stderr);
      fprintf (stderr, "\n");
    }
}

/* This return 0 if the given file entry's symbol table does *not*
   contain the nlist point entry, and it returns the files entry
   pointer (cast to unsigned long) if it does. */

unsigned long
contains_symbol (entry, n_ptr)
     struct file_entry *entry;
     register struct nlist *n_ptr;
{
  if (n_ptr >= entry->symbols &&
      n_ptr < (entry->symbols
	       + (entry->header.a_syms / sizeof (struct nlist))))
    return (unsigned long) entry;
  return 0;
}


/* Searching libraries */

struct file_entry *decode_library_subfile ();
void linear_library (), symdef_library ();

/* Search the library ENTRY, already open on descriptor DESC.
   This means deciding which library members to load,
   making a chain of `struct file_entry' for those members,
   and entering their global symbols in the hash table.  */

void
search_library (desc, entry)
     int desc;
     struct file_entry *entry;
{
  int member_length;
  register char *name;
  register struct file_entry *subentry;

  if (!undefined_global_sym_count) return;

  /* Examine its first member, which starts SARMAG bytes in.  */
  subentry = decode_library_subfile (desc, entry, SARMAG, &member_length);
  if (!subentry) return;

  name = subentry->filename;
  free (subentry);

  /* Search via __.SYMDEF if that exists, else linearly.  */

  if (!strcmp (name, "__.SYMDEF"))
    symdef_library (desc, entry, member_length);
  else
    linear_library (desc, entry);
}

/* Construct and return a file_entry for a library member.
   The library's file_entry is library_entry, and the library is open on DESC.
   SUBFILE_OFFSET is the byte index in the library of this member's header.
   We store the length of the member into *LENGTH_LOC.  */

struct file_entry *
decode_library_subfile (desc, library_entry, subfile_offset, length_loc)
     int desc;
     struct file_entry *library_entry;
     int subfile_offset;
     int *length_loc;
{
  int bytes_read;
  register int namelen;
  int member_length;
  register char *name;
  struct ar_hdr hdr1;
  register struct file_entry *subentry;

  lseek (desc, subfile_offset, 0);

  bytes_read = read (desc, &hdr1, sizeof hdr1);
  if (!bytes_read)
    return 0;		/* end of archive */

  if (sizeof hdr1 != bytes_read)
    fatal_with_file ("malformed library archive ", library_entry);

  if (sscanf (hdr1.ar_size, "%d", &member_length) != 1)
    fatal_with_file ("malformatted header of archive member in ", library_entry);

  subentry = (struct file_entry *) xmalloc (sizeof (struct file_entry));
  bzero (subentry, sizeof (struct file_entry));

  for (namelen = 0;
       namelen < sizeof hdr1.ar_name
       && hdr1.ar_name[namelen] != 0 && hdr1.ar_name[namelen] != ' '
       && hdr1.ar_name[namelen] != '/';
       namelen++);

  name = (char *) xmalloc (namelen+1);
  strncpy (name, hdr1.ar_name, namelen);
  name[namelen] = 0;

  subentry->filename = name;
  subentry->local_sym_name = name;
  subentry->symbols = 0;
  subentry->strings = 0;
  subentry->subfiles = 0;
  subentry->starting_offset = subfile_offset + sizeof hdr1;
  subentry->superfile = library_entry;
  subentry->library_flag = 0;
  subentry->header_read_flag = 0;
  subentry->just_syms_flag = 0;
  subentry->chain = 0;
  subentry->total_size = member_length;

  (*length_loc) = member_length;

  return subentry;
}

int subfile_wanted_p ();

/* Search a library that has a __.SYMDEF member.
   DESC is a descriptor on which the library is open.
     The file pointer is assumed to point at the __.SYMDEF data.
   ENTRY is the library's file_entry.
   MEMBER_LENGTH is the length of the __.SYMDEF data.  */

void
symdef_library (desc, entry, member_length)
     int desc;
     struct file_entry *entry;
     int member_length;
{
  int *symdef_data = (int *) xmalloc (member_length);
  register struct symdef *symdef_base;
  char *sym_name_base;
  int number_of_symdefs;
  int length_of_strings;
  int not_finished;
  int bytes_read;
  register int i;
  struct file_entry *prev = 0;
  int prev_offset = 0;

  bytes_read = read (desc, symdef_data, member_length);
  if (bytes_read != member_length)
    fatal_with_file ("malformatted __.SYMDEF in ", entry);

  number_of_symdefs = *symdef_data / sizeof (struct symdef);
  if (number_of_symdefs < 0 ||
       number_of_symdefs * sizeof (struct symdef) + 2 * sizeof (int) > member_length)
    fatal_with_file ("malformatted __.SYMDEF in ", entry);

  symdef_base = (struct symdef *) (symdef_data + 1);
  length_of_strings = *(int *) (symdef_base + number_of_symdefs);

  if (length_of_strings < 0
      || number_of_symdefs * sizeof (struct symdef) + length_of_strings
	  + 2 * sizeof (int) > member_length)
    fatal_with_file ("malformatted __.SYMDEF in ", entry);

  sym_name_base = sizeof (int) + (char *) (symdef_base + number_of_symdefs);

  /* Check all the string indexes for validity.  */

  for (i = 0; i < number_of_symdefs; i++)
    {
      register int index = symdef_base[i].symbol_name_string_index;
      if (index < 0 || index >= length_of_strings
	  || (index && *(sym_name_base + index - 1)))
	fatal_with_file ("malformatted __.SYMDEF in ", entry);
    }

  /* Search the symdef data for members to load.
     Do this until one whole pass finds nothing to load.  */

  not_finished = 1;
  while (not_finished)
    {
      not_finished = 0;

      /* Scan all the symbols mentioned in the symdef for ones that we need.
	 Load the library members that contain such symbols.  */

      for (i = 0;
	   (i < number_of_symdefs
	    && (undefined_global_sym_count || common_defined_global_count));
	   i++)
	if (symdef_base[i].symbol_name_string_index >= 0)
	  {
	    register symbol *sp;

	    sp = getsym_soft (sym_name_base
			      + symdef_base[i].symbol_name_string_index);

	    /* If we find a symbol that appears to be needed, think carefully
	       about the archive member that the symbol is in.  */

	    if (sp && ((sp->referenced && !sp->defined)
		       || (sp->defined && sp->max_common_size)))
	      {
		int junk;
		register int j;
		register int offset = symdef_base[i].library_member_offset;
		struct file_entry *subentry;

		/* Don't think carefully about any archive member
		   more than once in a given pass.  */

		if (prev_offset == offset)
		  continue;
		prev_offset = offset;

		/* Read the symbol table of the archive member.  */

		subentry = decode_library_subfile (desc, entry, offset, &junk);
		if (subentry == 0)
		  fatal ("invalid offset for %s in symbol table of %s",
			 sym_name_base
			 + symdef_base[i].symbol_name_string_index,
			 entry->filename);
		read_entry_symbols (desc, subentry);
		subentry->strings = (char *) malloc (subentry->string_size);
		read_entry_strings (desc, subentry);

		/* Now scan the symbol table and decide whether to load.  */

		if (!subfile_wanted_p (subentry))
		  {
		    free (subentry->symbols);
		    free (subentry);
		  }
		else
		  {
		    /* This member is needed; load it.
		       Since we are loading something on this pass,
		       we must make another pass through the symdef data.  */

		    not_finished = 1;

		    enter_file_symbols (subentry);

		    if (prev)
		      prev->chain = subentry;
		    else entry->subfiles = subentry;
		    prev = subentry;

		    /* Clear out this member's symbols from the symdef data
		       so that following passes won't waste time on them.  */

		    for (j = 0; j < number_of_symdefs; j++)
		      {
			if (symdef_base[j].library_member_offset == offset)
			  symdef_base[j].symbol_name_string_index = -1;
		      }
		  }

		/* We'll read the strings again if we need them again.  */
		free (subentry->strings);
		subentry->strings = 0;
	      }
	  }
    }

  free (symdef_data);
}

/* Search a library that has no __.SYMDEF.
   ENTRY is the library's file_entry.
   DESC is the descriptor it is open on.  */

void
linear_library (desc, entry)
     int desc;
     struct file_entry *entry;
{
  register struct file_entry *prev = 0;
  register int this_subfile_offset = SARMAG;

  while (undefined_global_sym_count || common_defined_global_count)
    {
      int member_length;
      register struct file_entry *subentry;

      subentry = decode_library_subfile (desc, entry, this_subfile_offset,
					 &member_length);

      if (!subentry) return;

      read_entry_symbols (desc, subentry);
      subentry->strings = (char *) alloca (subentry->string_size);
      read_entry_strings (desc, subentry);

      if (!subfile_wanted_p (subentry))
	{
	  free (subentry->symbols);
	  free (subentry);
	}
      else
	{
	  enter_file_symbols (subentry);

	  if (prev)
	    prev->chain = subentry;
	  else entry->subfiles = subentry;
	  prev = subentry;
	  subentry->strings = 0; /* Since space will dissapear on return */
	}

      this_subfile_offset += member_length + sizeof (struct ar_hdr);
      if (this_subfile_offset & 1) this_subfile_offset++;
    }
}

/* ENTRY is an entry for a library member.
   Its symbols have been read into core, but not entered.
   Return nonzero if we ought to load this member.  */

int
subfile_wanted_p (entry)
     struct file_entry *entry;
{
  register struct nlist *p;
  register struct nlist *end
    = entry->symbols + entry->header.a_syms / sizeof (struct nlist);
#ifdef DOLLAR_KLUDGE
  register int dollar_cond = 0;
#endif

  for (p = entry->symbols; p < end; p++)
    {
      register int type = p->n_type;
      register char *name = p->n_un.n_strx + entry->strings;

      /* If the symbol has an interesting definition, we could
	 potentially want it.  */
      if (type & N_EXT
	  && (type != (N_UNDF | N_EXT) || p->n_value

#ifdef DOLLAR_KLUDGE
	       || name[1] == '$'
#endif
	      )
	  && !SET_ELEMENT_P (type)
	  && !set_element_prefixed_p (name))
	{
	  register symbol *sp = getsym_soft (name);

#ifdef DOLLAR_KLUDGE
	  if (name[1] == '$')
	    {
	      sp = getsym_soft (&name[2]);
	      dollar_cond = 1;
	      if (!sp) continue;
	      if (sp->referenced)
		{
		  if (write_map)
		    {
		      print_file_name (entry, stdout);
		      fprintf (stdout, " needed due to $-conditional %s\n", name);
		    }
		  return 1;
		}
	      continue;
	    }
#endif

	  /* If this symbol has not been hashed, we can't be looking for it. */

	  if (!sp) continue;

	  if ((sp->referenced && !sp->defined)
	      || (sp->defined && sp->max_common_size && (type & N_TEXT) == 0))
	    {
	      /* This is a symbol we are looking for.  It is either
	         not yet defined or defined as a common.  */
#ifdef DOLLAR_KLUDGE
	      if (dollar_cond) continue;
#endif
	      if (type == (N_UNDF | N_EXT))
		{
		  /* Symbol being defined as common.
		     Remember this, but don't load subfile just for this.  */

		  /* If it didn't used to be common, up the count of
		     common symbols.  */
		  if (!sp->max_common_size)
		    common_defined_global_count++;

		  if (sp->max_common_size < p->n_value)
		    sp->max_common_size = p->n_value;
		  if (!sp->defined)
		    undefined_global_sym_count--;
		  sp->defined = 1;
		  continue;
		}

	      if (write_map)
		{
		  print_file_name (entry, stdout);
		  fprintf (stdout, " needed due to %s\n", sp->name);
		}
	      return 1;
	    }
	}
    }

  return 0;
}

void consider_file_section_lengths (), relocate_file_addresses ();

/* Having entered all the global symbols and found the sizes of sections
   of all files to be linked, make all appropriate deductions from this data.

   We propagate global symbol values from definitions to references.
   We compute the layout of the output file and where each input file's
   contents fit into it.  */

void
digest_symbols ()
{
  register int i;
  int setv_fill_count;

  if (trace_files)
    fprintf (stderr, "Digesting symbol information:\n\n");

  /* Compute total size of sections */

  each_file (consider_file_section_lengths, 0);

  /* If necessary, pad text section to full page in the file.
     Include the padding in the text segment size.  */

  if (magic == ZMAGIC)
    {
      int text_end = text_size + N_TXTOFF (outheader);
      text_pad = ((text_end + page_size - 1) & (- page_size)) - text_end;
      text_size += text_pad;
    }

#ifdef _N_BASEADDR
  /* SunOS 4.1 N_TXTADDR depends on the value of outheader.a_entry.  */
  outheader.a_entry = N_PAGSIZ (outheader);
#endif

  outheader.a_text = text_size;
#ifdef sequent
  outheader.a_text += N_ADDRADJ (outheader);
#endif

  /* Make the data segment address start in memory on a suitable boundary.  */

  if (! Tdata_flag_specified)
    data_start = N_DATADDR (outheader) + text_start - TEXT_START (outheader);

  /* Set up the set element vector */

  if (!relocatable_output)
    {
      /* The set sector size is the number of set elements + a word
         for each symbol for the length word at the beginning of the
	 vector, plus a word for each symbol for a zero at the end of
	 the vector (for incremental linking).  */
      set_sect_size
	= (2 * set_symbol_count + set_vector_count) * sizeof (unsigned long);
      set_sect_start = data_start + data_size;
      data_size += set_sect_size;
      set_vectors = (unsigned long *) xmalloc (set_sect_size);
      setv_fill_count = 0;
    }

  /* Compute start addresses of each file's sections and symbols.  */

  each_full_file (relocate_file_addresses, 0);

  /* Now, for each symbol, verify that it is defined globally at most once.
     Put the global value into the symbol entry.
     Common symbols are allocated here, in the BSS section.
     Each defined symbol is given a '->defined' field
      which is the correct N_ code for its definition,
      except in the case of common symbols with -r.
     Then make all the references point at the symbol entry
     instead of being chained together. */

  defined_global_sym_count = 0;

  for (i = 0; i < TABSIZE; i++)
    {
      register symbol *sp;
      for (sp = symtab[i]; sp; sp = sp->link)
	{
	  /* For each symbol */
	  register struct nlist *p, *next;
	  int defs = 0, com = sp->max_common_size;
	  struct nlist *first_definition;
	  for (p = sp->refs; p; p = next)
	    {
	      register int type = p->n_type;

	      if (SET_ELEMENT_P (type))
		{
		  if (relocatable_output)
		    fatal ("internal: global ref to set element with -r");
		  if (!defs++)
		    {
		      sp->value = set_sect_start
			+ setv_fill_count++ * sizeof (unsigned long);
		      sp->defined = N_SETV | N_EXT;
		      first_definition = p;
		    }
		  else if ((sp->defined & ~N_EXT) != N_SETV)
		    {
		      sp->multiply_defined = 1;
		      multiple_def_count++;
		    }
		  set_vectors[setv_fill_count++] = p->n_value;
		}
	      else if ((type & N_EXT) && type != (N_UNDF | N_EXT)
		       && (type & N_TYPE) != N_FN)
		{
		  /* non-common definition */
		  if (defs++ && sp->value != p->n_value)
		    {
		      sp->multiply_defined = 1;
		      multiple_def_count++;
		    }
		  sp->value = p->n_value;
		  sp->defined = type;
		  first_definition = p;
		}
	      next = (struct nlist *) p->n_un.n_name;
	      p->n_un.n_name = (char *) sp;
	    }
	  /* Allocate as common if defined as common and not defined for real */
	  if (com && !defs)
	    {
	      if (!relocatable_output || force_common_definition)
		{
		  int align = sizeof (int);

		  /* Round up to nearest sizeof (int).  I don't know
		     whether this is necessary or not (given that
		     alignment is taken care of later), but it's
		     traditional, so I'll leave it in.  Note that if
		     this size alignment is ever removed, ALIGN above
		     will have to be initialized to 1 instead of
		     sizeof (int).  */

		  com = (com + sizeof (int) - 1) & (- sizeof (int));

		  while (!(com & align))
		    align <<= 1;

		  align = align > MAX_ALIGNMENT ? MAX_ALIGNMENT : align;

		  bss_size = ((((bss_size + data_size + data_start)
			      + (align - 1)) & (- align))
			      - data_size - data_start);

		  sp->value = data_start + data_size + bss_size;
		  sp->defined = N_BSS | N_EXT;
		  bss_size += com;
		  if (write_map)
		    printf ("Allocating common %s: %x at %x\n",
			    sp->name, com, sp->value);
		}
	      else
		{
		  sp->defined = 0;
		  undefined_global_sym_count++;
		}
	    }
	  /* Set length word at front of vector and zero byte at end.
	     Reverse the vector itself to put it in file order.  */
	  if ((sp->defined & ~N_EXT) == N_SETV)
	    {
	      unsigned long length_word_index
		= (sp->value - set_sect_start) / sizeof (unsigned long);
	      unsigned long i, tmp;

	      set_vectors[length_word_index]
		= setv_fill_count - 1 - length_word_index;

	      /* Reverse the vector.  */
	      for (i = 1;
		   i < (setv_fill_count - length_word_index - 1) / 2 + 1;
		   i++)
		{
		  tmp = set_vectors[length_word_index + i];
		  set_vectors[length_word_index + i]
		    = set_vectors[setv_fill_count - i];
		  set_vectors[setv_fill_count - i] = tmp;
		}

	      set_vectors[setv_fill_count++] = 0;
	    }
	  if (sp->defined)
	    defined_global_sym_count++;
	}
    }

  if (end_symbol)		/* These are null if -r.  */
    {
      etext_symbol->value = text_size + text_start;
      edata_symbol->value = data_start + data_size;
      end_symbol->value = data_start + data_size + bss_size;
    }

  /* Figure the data_pad now, so that it overlaps with the bss addresses.  */

  if (specified_data_size && specified_data_size > data_size)
    data_pad = specified_data_size - data_size;

  if (magic == ZMAGIC)
    data_pad = ((data_pad + data_size + page_size - 1) & (- page_size))
               - data_size;

  bss_size -= data_pad;
  if (bss_size < 0) bss_size = 0;

  data_size += data_pad;
}

/* Accumulate the section sizes of input file ENTRY
   into the section sizes of the output file.  */

void
consider_file_section_lengths (entry)
     register struct file_entry *entry;
{
  if (entry->just_syms_flag)
    return;

  entry->text_start_address = text_size;
  /* If there were any vectors, we need to chop them off */
  text_size += entry->header.a_text;
  entry->data_start_address = data_size;
  data_size += entry->header.a_data;
  entry->bss_start_address = bss_size;
  bss_size += entry->header.a_bss;

  text_reloc_size += entry->header.a_trsize;
  data_reloc_size += entry->header.a_drsize;
}

/* Determine where the sections of ENTRY go into the output file,
   whose total section sizes are already known.
   Also relocate the addresses of the file's local and debugger symbols.  */

void
relocate_file_addresses (entry)
     register struct file_entry *entry;
{
  entry->text_start_address += text_start;
  /* Note that `data_start' and `data_size' have not yet been
     adjusted for `data_pad'.  If they had been, we would get the wrong
     results here.  */
  entry->data_start_address += data_start;
  entry->bss_start_address += data_start + data_size;

  {
    register struct nlist *p;
    register struct nlist *end
      = entry->symbols + entry->header.a_syms / sizeof (struct nlist);

    for (p = entry->symbols; p < end; p++)
      {
	/* If this belongs to a section, update it by the section's start address */
	register int type = p->n_type & N_TYPE;

	switch (type)
	  {
	  case N_TEXT:
	  case N_SETT:
	    p->n_value += entry->text_start_address;
	    break;
	  case N_DATA:
	  case N_SETV:
	  case N_SETD:
	    /* A symbol whose value is in the data section
	       is present in the input file as if the data section
	       started at an address equal to the length of the file's text.  */
	    p->n_value += entry->data_start_address - entry->header.a_text;
	    break;
	  case N_BSS:
	  case N_SETB:
	    /* likewise for symbols with value in BSS.  */
	    p->n_value += entry->bss_start_address
	      - entry->header.a_text - entry->header.a_data;
	    break;
	  }
      }
  }
}

void describe_file_sections (), list_file_locals ();

/* Print a complete or partial map of the output file.  */

void
print_symbols (outfile)
     FILE *outfile;
{
  register int i;

  fprintf (outfile, "\nFiles:\n\n");

  each_file (describe_file_sections, outfile);

  fprintf (outfile, "\nGlobal symbols:\n\n");

  for (i = 0; i < TABSIZE; i++)
    {
      register symbol *sp;
      for (sp = symtab[i]; sp; sp = sp->link)
	{
	  if (sp->defined == 1)
	    fprintf (outfile, "  %s: common, length 0x%x\n", sp->name, sp->max_common_size);
	  if (sp->defined)
	    fprintf (outfile, "  %s: 0x%x\n", sp->name, sp->value);
	  else if (sp->referenced)
	    fprintf (outfile, "  %s: undefined\n", sp->name);
	}
    }

  each_file (list_file_locals, outfile);
}

void
describe_file_sections (entry, outfile)
     struct file_entry *entry;
     FILE *outfile;
{
  fprintf (outfile, "  ");
  print_file_name (entry, outfile);
  if (entry->just_syms_flag)
    fprintf (outfile, " symbols only\n", 0);
  else
    fprintf (outfile, " text %x(%x), data %x(%x), bss %x(%x) hex\n",
	     entry->text_start_address, entry->header.a_text,
	     entry->data_start_address, entry->header.a_data,
	     entry->bss_start_address, entry->header.a_bss);
}

void
list_file_locals (entry, outfile)
     struct file_entry *entry;
     FILE *outfile;
{
  register struct nlist
    *p,
    *end = entry->symbols + entry->header.a_syms / sizeof (struct nlist);

  entry->strings = (char *) alloca (entry->string_size);
  read_entry_strings (file_open (entry), entry);

  fprintf (outfile, "\nLocal symbols of ");
  print_file_name (entry, outfile);
  fprintf (outfile, ":\n\n");

  for (p = entry->symbols; p < end; p++)
    /* If this is a definition,
       update it if necessary by this file's start address.  */
    if (!(p->n_type & (N_STAB | N_EXT)))
      fprintf (outfile, "  %s: 0x%x\n",
	       entry->strings + p->n_un.n_strx, p->n_value);

  entry->strings = 0;		/* All done with them.  */
}


/* Static vars for do_warnings and subroutines of it */
int list_unresolved_refs;	/* List unresolved refs */
int list_warning_symbols;	/* List warning syms */
int list_multiple_defs;		/* List multiple definitions */

/*
 * Structure for communication between do_file_warnings and it's
 * helper routines.  Will in practice be an array of three of these:
 * 0) Current line, 1) Next line, 2) Source file info.
 */
struct line_debug_entry
{
  int line;
  char *filename;
  struct nlist *sym;
};

void qsort ();
/*
 * Helper routines for do_file_warnings.
 */

/* Return an integer less than, equal to, or greater than 0 as per the
   relation between the two relocation entries.  Used by qsort.  */

int
relocation_entries_relation (rel1, rel2)
     struct relocation_info *rel1, *rel2;
{
  return RELOC_ADDRESS(rel1) - RELOC_ADDRESS(rel2);
}

/* Moves to the next debugging symbol in the file.  USE_DATA_SYMBOLS
   determines the type of the debugging symbol to look for (DSLINE or
   SLINE).  STATE_POINTER keeps track of the old and new locatiosn in
   the file.  It assumes that state_pointer[1] is valid; ie
   that it.sym points into some entry in the symbol table.  If
   state_pointer[1].sym == 0, this routine should not be called.  */

int
next_debug_entry (use_data_symbols, state_pointer)
     register int use_data_symbols;
     /* Next must be passed by reference! */
     struct line_debug_entry state_pointer[3];
{
  register struct line_debug_entry
    *current = state_pointer,
    *next = state_pointer + 1,
    /* Used to store source file */
    *source = state_pointer + 2;
  struct file_entry *entry = (struct file_entry *) source->sym;

  current->sym = next->sym;
  current->line = next->line;
  current->filename = next->filename;

  while (++(next->sym) < (entry->symbols
			  + entry->header.a_syms/sizeof (struct nlist)))
    {
      /* n_type is a char, and N_SOL, N_EINCL and N_BINCL are > 0x80, so
       * may look negative...therefore, must mask to low bits
       */
      switch (next->sym->n_type & 0xff)
	{
	case N_SLINE:
	  if (use_data_symbols) continue;
	  next->line = next->sym->n_desc;
	  return 1;
	case N_DSLINE:
	  if (!use_data_symbols) continue;
	  next->line = next->sym->n_desc;
	  return 1;
#ifdef HAVE_SUN_STABS
	case N_EINCL:
	  next->filename = source->filename;
	  continue;
#endif
	case N_SO:
	  source->filename = next->sym->n_un.n_strx + entry->strings;
	  source->line++;
#ifdef HAVE_SUN_STABS
	case N_BINCL:
#endif
	case N_SOL:
	  next->filename
	    = next->sym->n_un.n_strx + entry->strings;
	default:
	  continue;
	}
    }
  next->sym = (struct nlist *) 0;
  return 0;
}

/* Create a structure to save the state of a scan through the debug
   symbols.  USE_DATA_SYMBOLS is set if we should be scanning for
   DSLINE's instead of SLINE's.  entry is the file entry which points
   at the symbols to use.  */

struct line_debug_entry *
init_debug_scan (use_data_symbols, entry)
     int use_data_symbols;
     struct file_entry *entry;
{
  struct line_debug_entry
    *state_pointer
      = (struct line_debug_entry *)
	xmalloc (3 * sizeof (struct line_debug_entry));
  register struct line_debug_entry
    *current = state_pointer,
    *next = state_pointer + 1,
    *source = state_pointer + 2; /* Used to store source file */

  struct nlist *tmp;

  for (tmp = entry->symbols;
       tmp < (entry->symbols
	      + entry->header.a_syms/sizeof (struct nlist));
       tmp++)
    if (tmp->n_type == (int) N_SO)
      break;

  if (tmp >= (entry->symbols
	      + entry->header.a_syms/sizeof (struct nlist)))
    {
      /* I believe this translates to "We lose" */
      current->filename = next->filename = entry->filename;
      current->line = next->line = -1;
      current->sym = next->sym = (struct nlist *) 0;
      return state_pointer;
    }

  next->line = source->line = 0;
  next->filename = source->filename
    = (tmp->n_un.n_strx + entry->strings);
  source->sym = (struct nlist *) entry;
  next->sym = tmp;

  next_debug_entry (use_data_symbols, state_pointer); /* To setup next */

  if (!next->sym)		/* No line numbers for this section; */
				/* setup output results as appropriate */
    {
      if (source->line)
	{
	  current->filename = source->filename = entry->filename;
	  current->line = -1;	/* Don't print lineno */
	}
      else
	{
	  current->filename = source->filename;
	  current->line = 0;
	}
      return state_pointer;
    }


  next_debug_entry (use_data_symbols, state_pointer); /* To setup current */

  return state_pointer;
}

/* Takes an ADDRESS (in either text or data space) and a STATE_POINTER
   which describes the current location in the implied scan through
   the debug symbols within the file which ADDRESS is within, and
   returns the source line number which corresponds to ADDRESS.  */

int
address_to_line (address, state_pointer)
     unsigned long address;
     /* Next must be passed by reference! */
     struct line_debug_entry state_pointer[3];
{
  struct line_debug_entry
    *current = state_pointer,
    *next = state_pointer + 1;
  struct line_debug_entry *tmp_pointer;

  int use_data_symbols;

  if (next->sym)
    use_data_symbols = (next->sym->n_type & N_TYPE) == N_DATA;
  else
    return current->line;

  /* Go back to the beginning if we've already passed it.  */
  if (current->sym->n_value > address)
    {
      tmp_pointer = init_debug_scan (use_data_symbols,
				     (struct file_entry *)
				     ((state_pointer + 2)->sym));
      state_pointer[0] = tmp_pointer[0];
      state_pointer[1] = tmp_pointer[1];
      state_pointer[2] = tmp_pointer[2];
      free (tmp_pointer);
    }

  /* If we're still in a bad way, return -1, meaning invalid line.  */
  if (current->sym->n_value > address)
    return -1;

  while (next->sym
	 && next->sym->n_value <= address
	 && next_debug_entry (use_data_symbols, state_pointer))
    ;
  return current->line;
}


/* Macros for manipulating bitvectors.  */
#define	BIT_SET_P(bv, index)	((bv)[(index) >> 3] & 1 << ((index) & 0x7))
#define	SET_BIT(bv, index)	((bv)[(index) >> 3] |= 1 << ((index) & 0x7))

/* This routine will scan through the relocation data of file ENTRY,
   printing out references to undefined symbols and references to
   symbols defined in files with N_WARNING symbols.  If DATA_SEGMENT
   is non-zero, it will scan the data relocation segment (and use
   N_DSLINE symbols to track line number); otherwise it will scan the
   text relocation segment.  Warnings will be printed on the output
   stream OUTFILE.  Eventually, every nlist symbol mapped through will
   be marked in the NLIST_BITVECTOR, so we don't repeat ourselves when
   we scan the nlists themselves.  */

do_relocation_warnings (entry, data_segment, outfile, nlist_bitvector)
     struct file_entry *entry;
     int data_segment;
     FILE *outfile;
     unsigned char *nlist_bitvector;
{
  struct relocation_info
    *reloc_start = data_segment ? entry->datarel : entry->textrel,
    *reloc;
  int reloc_size
    = ((data_segment ? entry->header.a_drsize : entry->header.a_trsize)
       / sizeof (struct relocation_info));
  int start_of_segment
    = (data_segment ? entry->data_start_address : entry->text_start_address);
  struct nlist *start_of_syms = entry->symbols;
  struct line_debug_entry *state_pointer
    = init_debug_scan (data_segment != 0, entry);
  register struct line_debug_entry
    *current = state_pointer;
  /* Assigned to generally static values; should not be written into.  */
  char *errfmt;
  /* Assigned to alloca'd values cand copied into; should be freed
     when done.  */
  char *errmsg;
  int invalidate_line_number;

  /* We need to sort the relocation info here.  Sheesh, so much effort
     for one lousy error optimization. */

  qsort (reloc_start, reloc_size, sizeof (struct relocation_info),
	 relocation_entries_relation);

  for (reloc = reloc_start;
       reloc < (reloc_start + reloc_size);
       reloc++)
    {
      register struct nlist *s;
      register symbol *g;

      /* If the relocation isn't resolved through a symbol, continue */
      if (!RELOC_EXTERN_P(reloc))
	continue;

      s = &(entry->symbols[RELOC_SYMBOL(reloc)]);

      /* Local symbols shouldn't ever be used by relocation info, so
	 the next should be safe.
	 This is, of course, wrong.  References to local BSS symbols can be
	 the targets of relocation info, and they can (must) be
	 resolved through symbols.  However, these must be defined properly,
	 (the assembler would have caught it otherwise), so we can
	 ignore these cases.  */
      if (!(s->n_type & N_EXT))
	continue;

      g = (symbol *) s->n_un.n_name;
      errmsg = 0;

      if (!g->defined && list_unresolved_refs) /* Reference */
	{
	  /* Mark as being noted by relocation warning pass.  */
	  SET_BIT (nlist_bitvector, s - start_of_syms);

	  if (g->undef_refs >= MAX_UREFS_PRINTED)    /* Listed too many */
	    continue;

	  /* Undefined symbol which we should mention */

	  if (++(g->undef_refs) == MAX_UREFS_PRINTED)
	    {
	      errfmt = "More undefined symbol %s refs follow";
	      invalidate_line_number = 1;
	    }
	  else
	    {
	      errfmt = "Undefined symbol %s referenced from %s segment";
	      invalidate_line_number = 0;
	    }
	}
      else					     /* Defined */
	{
	  /* Potential symbol warning here */
	  if (!g->warning) continue;

	  /* Mark as being noted by relocation warning pass.  */
	  SET_BIT (nlist_bitvector, s - start_of_syms);

	  errfmt = 0;
	  errmsg = g->warning;
	  invalidate_line_number = 0;
	}


      /* If errfmt == 0, errmsg has already been defined.  */
      if (errfmt != 0)
	{
	  char *nm;

	  if (demangler == NULL || (nm = (*demangler)(g->name)) == NULL)
	    nm = g->name;
	  errmsg = (char *) xmalloc (strlen (errfmt) + strlen (nm) + 1);
	  sprintf (errmsg, errfmt, nm, data_segment ? "data" : "text");
	  if (nm != g->name)
	    free (nm);
	}

      address_to_line (RELOC_ADDRESS (reloc) + start_of_segment,
		       state_pointer);

      if (current->line >=0)
	fprintf (outfile, "%s:%d: %s\n", current->filename,
		 invalidate_line_number ? 0 : current->line, errmsg);
      else
	fprintf (outfile, "%s: %s\n", current->filename, errmsg);

      if (errfmt != 0)
	free (errmsg);
    }

  free (state_pointer);
}

/* Print on OUTFILE a list of all warnings generated by references
   and/or definitions in the file ENTRY.  List source file and line
   number if possible, just the .o file if not. */

void
do_file_warnings (entry, outfile)
     struct file_entry *entry;
     FILE *outfile;
{
  int number_of_syms = entry->header.a_syms / sizeof (struct nlist);
  unsigned char *nlist_bitvector
    = (unsigned char *) alloca ((number_of_syms >> 3) + 1);
  struct line_debug_entry *text_scan, *data_scan;
  int i;
  char *errfmt, *file_name;
  int line_number;
  int dont_allow_symbol_name;

  bzero (nlist_bitvector, (number_of_syms >> 3) + 1);

  /* Read in the files strings if they aren't available */
  if (!entry->strings)
    {
      int desc;

      entry->strings = (char *) alloca (entry->string_size);
      desc = file_open (entry);
      read_entry_strings (desc, entry);
    }

  read_file_relocation (entry);

  /* Do text warnings based on a scan through the relocation info.  */
  do_relocation_warnings (entry, 0, outfile, nlist_bitvector);

  /* Do data warnings based on a scan through the relocation info.  */
  do_relocation_warnings (entry, 1, outfile, nlist_bitvector);

  /* Scan through all of the nlist entries in this file and pick up
     anything that the scan through the relocation stuff didn't.  */

  text_scan = init_debug_scan (0, entry);
  data_scan = init_debug_scan (1, entry);

  for (i = 0; i < number_of_syms; i++)
    {
      struct nlist *s;
      struct glosym *g;

      s = entry->symbols + i;

      if (!(s->n_type & N_EXT))
	continue;

      g = (symbol *) s->n_un.n_name;
      dont_allow_symbol_name = 0;

      if (list_multiple_defs && g->multiply_defined)
	{
	  errfmt = "Definition of symbol %s (multiply defined)";
	  switch (s->n_type)
	    {
	    case N_TEXT | N_EXT:
	      line_number = address_to_line (s->n_value, text_scan);
	      file_name = text_scan[0].filename;
	      break;
	    case N_DATA | N_EXT:
	      line_number = address_to_line (s->n_value, data_scan);
	      file_name = data_scan[0].filename;
	      break;
	    case N_SETA | N_EXT:
	    case N_SETT | N_EXT:
	    case N_SETD | N_EXT:
	    case N_SETB | N_EXT:
	      if (g->multiply_defined == 2)
		continue;
	      errfmt = "First set element definition of symbol %s (multiply defined)";
	      break;
	    default:
	      continue;		/* Don't print out multiple defs
				   at references.  */
	    }
	}
      else if (BIT_SET_P (nlist_bitvector, i))
	continue;
      else if (list_unresolved_refs && !g->defined)
	{
	  if (g->undef_refs >= MAX_UREFS_PRINTED)
	    continue;

	  if (++(g->undef_refs) == MAX_UREFS_PRINTED)
	    errfmt = "More undefined \"%s\" refs follow";
	  else
	    errfmt = "Undefined symbol \"%s\" referenced";
	  line_number = -1;
	}
      else if (g->warning)
	{
	  /* There are two cases in which we don't want to
	     do this.  The first is if this is a definition instead of
	     a reference.  The second is if it's the reference used by
	     the warning stabs itself.  */
	  if (s->n_type != (N_EXT | N_UNDF)
	      || (i && (s-1)->n_type == N_WARNING))
	    continue;

	  errfmt = g->warning;
	  line_number = -1;
	  dont_allow_symbol_name = 1;
	}
      else
	continue;

      if (line_number == -1)
	fprintf (outfile, "%s: ", entry->filename);
      else
	fprintf (outfile, "%s:%d: ", file_name, line_number);

      if (dont_allow_symbol_name)
	fprintf (outfile, "%s", errfmt);
      else
	{
	  char *nm;
	  if (demangler != NULL && (nm = (*demangler)(g->name)) != NULL)
	    {
	      fprintf (outfile, errfmt, nm);
	      free (nm);
	    }
	  else
	    fprintf (outfile, errfmt, g->name);
	}

      fputc ('\n', outfile);
    }
  free (text_scan);
  free (data_scan);
  entry->strings = 0;		/* Since it will dissapear anyway.  */
}

do_warnings (outfile)
     FILE *outfile;
{
  list_unresolved_refs = !relocatable_output && undefined_global_sym_count;
  list_warning_symbols = warning_count;
  list_multiple_defs = multiple_def_count != 0;

  if (!(list_unresolved_refs ||
	list_warning_symbols ||
	list_multiple_defs      ))
    /* No need to run this routine */
    return;

  each_file (do_file_warnings, outfile);

  if (list_unresolved_refs || list_multiple_defs)
    make_executable = 0;
}

/* Write the output file */

void
write_output ()
{
  struct stat statbuf;
  int filemode;

  (void) unlink (output_filename);
  outdesc = open (output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (outdesc < 0) perror_name (output_filename);

  if (fstat (outdesc, &statbuf) < 0)
    perror_name (output_filename);

  filemode = statbuf.st_mode;

  chmod (output_filename, filemode & ~0111);

  /* Output the a.out header.  */
  write_header ();

  /* Output the text and data segments, relocating as we go.  */
  write_text ();
  write_data ();

  /* Output the merged relocation info, if requested with `-r'.  */
  if (relocatable_output)
    write_rel ();

  /* Output the symbol table (both globals and locals).  */
  write_syms ();

  /* Copy any GDB symbol segments from input files.  */
  write_symsegs ();

  close (outdesc);

  if (chmod (output_filename, filemode | 0111) == -1)
    perror_name (output_filename);
}

void modify_location (), perform_relocation (), copy_text (), copy_data ();

void
write_header ()
{
  N_SET_MAGIC (outheader, magic);
  outheader.a_text = text_size;
#ifdef sequent
  outheader.a_text += N_ADDRADJ (outheader);
  if (entry_symbol == 0)
    entry_symbol = getsym("start");
#endif
  outheader.a_data = data_size;
  outheader.a_bss = bss_size;
  outheader.a_entry = (entry_symbol ? entry_symbol->value
		       : text_start + entry_offset);
if (screwballmode)  {
  N_SET_MAGIC (outheader, ZMAGIC);
  outheader.a_text = 0;
  outheader.a_data = text_size + data_size;
  outheader.a_entry = (entry_symbol ? entry_symbol->value
		       : sizeof(struct exec));
}
#ifdef COFF_ENCAPSULATE
  if (need_coff_header)
    {
      /* We are encapsulating BSD format within COFF format.  */
      struct coffscn *tp, *dp, *bp;

      tp = &coffheader.scns[0];
      dp = &coffheader.scns[1];
      bp = &coffheader.scns[2];

      strcpy (tp->s_name, ".text");
      tp->s_paddr = text_start;
      tp->s_vaddr = text_start;
      tp->s_size = text_size;
      tp->s_scnptr = sizeof (struct coffheader) + sizeof (struct exec);
      tp->s_relptr = 0;
      tp->s_lnnoptr = 0;
      tp->s_nreloc = 0;
      tp->s_nlnno = 0;
      tp->s_flags = 0x20;
      strcpy (dp->s_name, ".data");
      dp->s_paddr = data_start;
      dp->s_vaddr = data_start;
      dp->s_size = data_size;
      dp->s_scnptr = tp->s_scnptr + tp->s_size;
      dp->s_relptr = 0;
      dp->s_lnnoptr = 0;
      dp->s_nreloc = 0;
      dp->s_nlnno = 0;
      dp->s_flags = 0x40;
      strcpy (bp->s_name, ".bss");
      bp->s_paddr = dp->s_vaddr + dp->s_size;
      bp->s_vaddr = bp->s_paddr;
      bp->s_size = bss_size;
      bp->s_scnptr = 0;
      bp->s_relptr = 0;
      bp->s_lnnoptr = 0;
      bp->s_nreloc = 0;
      bp->s_nlnno = 0;
      bp->s_flags = 0x80;

      coffheader.f_magic = COFF_MAGIC;
      coffheader.f_nscns = 3;
      /* store an unlikely time so programs can
       * tell that there is a bsd header
       */
      coffheader.f_timdat = 1;
      coffheader.f_symptr = 0;
      coffheader.f_nsyms = 0;
      coffheader.f_opthdr = 28;
      coffheader.f_flags = 0x103;
      /* aouthdr */
      coffheader.magic = ZMAGIC;
      coffheader.vstamp = 0;
      coffheader.tsize = tp->s_size;
      coffheader.dsize = dp->s_size;
      coffheader.bsize = bp->s_size;
      coffheader.entry = outheader.a_entry;
      coffheader.text_start = tp->s_vaddr;
      coffheader.data_start = dp->s_vaddr;
    }
#endif

#ifdef INITIALIZE_HEADER
  INITIALIZE_HEADER;
#endif

  if (strip_symbols == STRIP_ALL)
    nsyms = 0;
  else
    {
      nsyms = (defined_global_sym_count
	       + undefined_global_sym_count);
      if (discard_locals == DISCARD_L)
	nsyms += non_L_local_sym_count;
      else if (discard_locals == DISCARD_NONE)
	nsyms += local_sym_count;
      /* One extra for following reference on indirects */
      if (relocatable_output)
	nsyms += set_symbol_count + global_indirect_count;
    }

  if (strip_symbols == STRIP_NONE)
    nsyms += debugger_sym_count;

  outheader.a_syms = nsyms * sizeof (struct nlist);

  if (relocatable_output)
    {
      outheader.a_trsize = text_reloc_size;
      outheader.a_drsize = data_reloc_size;
    }
  else
    {
      outheader.a_trsize = 0;
      outheader.a_drsize = 0;
    }

#ifdef COFF_ENCAPSULATE
  if (need_coff_header)
    mywrite (&coffheader, sizeof coffheader, 1, outdesc);
#endif
  mywrite (&outheader, sizeof (struct exec), 1, outdesc);
if (screwballmode)
  N_SET_MAGIC (outheader, OMAGIC);

  /* Output whatever padding is required in the executable file
     between the header and the start of the text.  */

#ifndef COFF_ENCAPSULATE
  padfile (N_TXTOFF (outheader) - sizeof outheader, outdesc);
#endif
}

/* Relocate the text segment of each input file
   and write to the output file.  */

void
write_text ()
{
  if (trace_files)
    fprintf (stderr, "Copying and relocating text:\n\n");

  each_full_file (copy_text, 0);
  file_close ();

  if (trace_files)
    fprintf (stderr, "\n");

  padfile (text_pad, outdesc);
}

int
text_offset (entry)
     struct file_entry *entry;
{
  return entry->starting_offset + N_TXTOFF (entry->header);
}

/* Read in all of the relocation information */

void
read_relocation ()
{
  each_full_file (read_file_relocation, 0);
}

/* Read in the relocation sections of ENTRY if necessary */

void
read_file_relocation (entry)
     struct file_entry *entry;
{
  register struct relocation_info *reloc;
  int desc;
  int read_return;

  desc = -1;
  if (!entry->textrel)
    {
      reloc = (struct relocation_info *) xmalloc (entry->header.a_trsize);
      desc = file_open (entry);
      lseek (desc,
	     text_offset (entry) + entry->header.a_text + entry->header.a_data,
	     L_SET);
      if (entry->header.a_trsize != (read_return = read (desc, reloc, entry->header.a_trsize)))
	{
	  fprintf (stderr, "Return from read: %d\n", read_return);
	  fatal_with_file ("premature eof in text relocation of ", entry);
	}
      entry->textrel = reloc;
    }

  if (!entry->datarel)
    {
      reloc = (struct relocation_info *) xmalloc (entry->header.a_drsize);
      if (desc == -1) desc = file_open (entry);
      lseek (desc,
	     text_offset (entry) + entry->header.a_text
	     + entry->header.a_data + entry->header.a_trsize,
	     L_SET);
      if (entry->header.a_drsize != read (desc, reloc, entry->header.a_drsize))
	fatal_with_file ("premature eof in data relocation of ", entry);
      entry->datarel = reloc;
    }
}

/* Read the text segment contents of ENTRY, relocate them,
   and write the result to the output file.
   If `-r', save the text relocation for later reuse.  */

void
copy_text (entry)
     struct file_entry *entry;
{
  register char *bytes;
  register int desc;
  register struct relocation_info *reloc;

  if (trace_files)
    prline_file_name (entry, stderr);

  desc = file_open (entry);

  /* Allocate space for the file's text section */

  bytes = (char *) alloca (entry->header.a_text);

  /* Deal with relocation information however is appropriate */

  if (entry->textrel)  reloc = entry->textrel;
  else if (relocatable_output)
    {
      read_file_relocation (entry);
      reloc = entry->textrel;
    }
  else
    {
      reloc = (struct relocation_info *) alloca (entry->header.a_trsize);
      lseek (desc, text_offset (entry) + entry->header.a_text + entry->header.a_data, 0);
      if (entry->header.a_trsize != read (desc, reloc, entry->header.a_trsize))
	fatal_with_file ("premature eof in text relocation of ", entry);
    }

  /* Read the text section into core.  */

  lseek (desc, text_offset (entry), 0);
  if (entry->header.a_text != read (desc, bytes, entry->header.a_text))
    fatal_with_file ("premature eof in text section of ", entry);


  /* Relocate the text according to the text relocation.  */

  perform_relocation (bytes, entry->text_start_address, entry->header.a_text,
		      reloc, entry->header.a_trsize, entry);

  /* Write the relocated text to the output file.  */

  mywrite (bytes, 1, entry->header.a_text, outdesc);
}

/* Relocate the data segment of each input file
   and write to the output file.  */

void
write_data ()
{
  if (trace_files)
    fprintf (stderr, "Copying and relocating data:\n\n");

  each_full_file (copy_data, 0);
  file_close ();

  /* Write out the set element vectors.  See digest symbols for
     description of length of the set vector section.  */

  if (set_vector_count)
    mywrite (set_vectors, 2 * set_symbol_count + set_vector_count,
	     sizeof (unsigned long), outdesc);

  if (trace_files)
    fprintf (stderr, "\n");

  padfile (data_pad, outdesc);
}

/* Read the data segment contents of ENTRY, relocate them,
   and write the result to the output file.
   If `-r', save the data relocation for later reuse.
   See comments in `copy_text'.  */

void
copy_data (entry)
     struct file_entry *entry;
{
  register struct relocation_info *reloc;
  register char *bytes;
  register int desc;

  if (trace_files)
    prline_file_name (entry, stderr);

  desc = file_open (entry);

  bytes = (char *) alloca (entry->header.a_data);

  if (entry->datarel) reloc = entry->datarel;
  else if (relocatable_output)	/* Will need this again */
    {
      read_file_relocation (entry);
      reloc = entry->datarel;
    }
  else
    {
      reloc = (struct relocation_info *) alloca (entry->header.a_drsize);
      lseek (desc, text_offset (entry) + entry->header.a_text
	     + entry->header.a_data + entry->header.a_trsize,
	     0);
      if (entry->header.a_drsize != read (desc, reloc, entry->header.a_drsize))
	fatal_with_file ("premature eof in data relocation of ", entry);
    }

  lseek (desc, text_offset (entry) + entry->header.a_text, 0);
  if (entry->header.a_data != read (desc, bytes, entry->header.a_data))
    fatal_with_file ("premature eof in data section of ", entry);

  perform_relocation (bytes, entry->data_start_address - entry->header.a_text,
		      entry->header.a_data, reloc, entry->header.a_drsize, entry);

  mywrite (bytes, 1, entry->header.a_data, outdesc);
}

/* Relocate ENTRY's text or data section contents.
   DATA is the address of the contents, in core.
   DATA_SIZE is the length of the contents.
   PC_RELOCATION is the difference between the address of the contents
     in the output file and its address in the input file.
   RELOC_INFO is the address of the relocation info, in core.
   RELOC_SIZE is its length in bytes.  */
/* This version is about to be severly hacked by Randy.  Hope it
   works afterwards. */
void
perform_relocation (data, pc_relocation, data_size, reloc_info, reloc_size, entry)
     char *data;
     struct relocation_info *reloc_info;
     struct file_entry *entry;
     int pc_relocation;
     int data_size;
     int reloc_size;
{
  register struct relocation_info *p = reloc_info;
  struct relocation_info *end
    = reloc_info + reloc_size / sizeof (struct relocation_info);
  int text_relocation = entry->text_start_address;
  int data_relocation = entry->data_start_address - entry->header.a_text;
  int bss_relocation
    = entry->bss_start_address - entry->header.a_text - entry->header.a_data;

  for (; p < end; p++)
    {
      register int relocation = 0;
      register int addr = RELOC_ADDRESS(p);
      register unsigned int mask = 0;

      if (addr >= data_size)
	fatal_with_file ("relocation address out of range in ", entry);

      if (RELOC_EXTERN_P(p))
	{
	  int symindex = RELOC_SYMBOL (p) * sizeof (struct nlist);
	  symbol *sp = ((symbol *)
			(((struct nlist *)
			  (((char *)entry->symbols) + symindex))
			 ->n_un.n_name));

#ifdef N_INDR
	  /* Resolve indirection */
	  if ((sp->defined & ~N_EXT) == N_INDR)
	    sp = (symbol *) sp->value;
#endif

	  if (symindex >= entry->header.a_syms)
	    fatal_with_file ("relocation symbolnum out of range in ", entry);

	  /* If the symbol is undefined, leave it at zero.  */
	  if (! sp->defined)
	    relocation = 0;
	  else
	    relocation = sp->value;
	}
      else switch (RELOC_TYPE(p))
	{
	case N_TEXT:
	case N_TEXT | N_EXT:
	  relocation = text_relocation;
	  break;

	case N_DATA:
	case N_DATA | N_EXT:
	  /* A word that points to beginning of the the data section
	     initially contains not 0 but rather the "address" of that section
	     in the input file, which is the length of the file's text.  */
	  relocation = data_relocation;
	  break;

	case N_BSS:
	case N_BSS | N_EXT:
	  /* Similarly, an input word pointing to the beginning of the bss
	     initially contains the length of text plus data of the file.  */
	  relocation = bss_relocation;
	  break;

	case N_ABS:
	case N_ABS | N_EXT:
	  /* Don't know why this code would occur, but apparently it does.  */
	  break;

	default:
	  fatal_with_file ("nonexternal relocation code invalid in ", entry);
	}

#ifdef RELOC_ADD_EXTRA
      relocation += RELOC_ADD_EXTRA(p);
      if (relocatable_output)
	{
	  /* Non-PC relative relocations which are absolute
	     or which have become non-external now have fixed
	     relocations.  Set the ADD_EXTRA of this relocation
	     to be the relocation we have now determined.  */
	  if (! RELOC_PCREL_P (p))
	    {
	      if ((int)p->r_type <= RELOC_32
		  || RELOC_EXTERN_P (p) == 0)
		RELOC_ADD_EXTRA (p) = relocation;
	    }
	  /* External PC-relative relocations continue to move around;
	     update their relocations by the amount they have moved
	     so far.  */
	  else if (RELOC_EXTERN_P (p))
	    RELOC_ADD_EXTRA (p) -= pc_relocation;
	  continue;
	}
#endif

      if (RELOC_PCREL_P(p))
	relocation -= pc_relocation;

      relocation >>= RELOC_VALUE_RIGHTSHIFT(p);

      /* Unshifted mask for relocation */
      mask = 1 << RELOC_TARGET_BITSIZE(p) - 1;
      mask |= mask - 1;
      relocation &= mask;

      /* Shift everything up to where it's going to be used */
      relocation <<= RELOC_TARGET_BITPOS(p);
      mask <<= RELOC_TARGET_BITPOS(p);

      switch (RELOC_TARGET_SIZE(p))
	{
	case 0:
	  if (RELOC_MEMORY_SUB_P(p))
	    relocation -= mask & *(char *) (data + addr);
	  else if (RELOC_MEMORY_ADD_P(p))
	    relocation += mask & *(char *) (data + addr);
	  *(char *) (data + addr) &= ~mask;
	  *(char *) (data + addr) |= relocation;
	  break;

	case 1:
#ifdef tahoe
	  if (((int) data + addr & 1) == 0)
	    {
#endif
	      if (RELOC_MEMORY_SUB_P(p))
		relocation -= mask & *(short *) (data + addr);
	      else if (RELOC_MEMORY_ADD_P(p))
		relocation += mask & *(short *) (data + addr);
	      *(short *) (data + addr) &= ~mask;
	      *(short *) (data + addr) |= relocation;
#ifdef tahoe
	    }
	  /*
	   * The CCI Power 6 (aka Tahoe) architecture has byte-aligned
	   * instruction operands but requires data accesses to be aligned.
	   * Brain-damage...
	   */
	  else
	    {
	      unsigned char *da = (unsigned char *) (data + addr);
	      unsigned short s = da[0] << 8 | da[1];

	      if (RELOC_MEMORY_SUB_P(p))
		relocation -= mask & s;
	      else if (RELOC_MEMORY_ADD_P(p))
		relocation += mask & s;
	      s &= ~mask;
	      s |= relocation;
	      da[0] = s >> 8;
	      da[1] = s;
	    }
#endif
	  break;

	case 2:
#ifndef _CROSS_TARGET_ARCH
#ifdef tahoe
	  if (((int) data + addr & 3) == 0)
	    {
#endif
	      if (RELOC_MEMORY_SUB_P(p))
		relocation -= mask & *(long *) (data + addr);
	      else if (RELOC_MEMORY_ADD_P(p))
		relocation += mask & *(long *) (data + addr);
	      *(long *) (data + addr) &= ~mask;
	      *(long *) (data + addr) |= relocation;
#ifdef tahoe
	    }
	  else
	    {
	      unsigned char *da = (unsigned char *) (data + addr);
	      unsigned long l = da[0] << 24 | da[1] << 16 | da[2] << 8 | da[3];

	      if (RELOC_MEMORY_SUB_P(p))
		relocation -= mask & l;
	      else if (RELOC_MEMORY_ADD_P(p))
		relocation += mask & l;
	      l &= ~mask;
	      l |= relocation;
	      da[0] = l >> 24;
	      da[1] = l >> 16;
	      da[2] = l >> 8;
	      da[3] = l;
	    }
#endif
#else
	/* Handle long word alignment requirements of SPARC architecture */
	/* WARNING:  This fix makes an assumption on byte ordering */
	/* Marc Ullman, Stanford University    Nov. 1 1989  */
	  if (RELOC_MEMORY_SUB_P(p)) {
	    relocation -= mask & 
	      ((*(unsigned short *) (data + addr) << 16) |
		*(unsigned short *) (data + addr + 2));
	  } else if (RELOC_MEMORY_ADD_P(p)) {
	    relocation += mask &
	      ((*(unsigned short *) (data + addr) << 16) |
		*(unsigned short *) (data + addr + 2));
	  }
	  *(unsigned short *) (data + addr)     &= (~mask >> 16);
	  *(unsigned short *) (data + addr + 2) &= (~mask & 0xffff);
	  *(unsigned short *) (data + addr)     |= (relocation >> 16);
	  *(unsigned short *) (data + addr + 2) |= (relocation & 0xffff);
#endif
	  break;

	default:
	  fatal_with_file ("Unimplemented relocation field length in ", entry);
	}
    }
}

/* For relocatable_output only: write out the relocation,
   relocating the addresses-to-be-relocated.  */

void coptxtrel (), copdatrel ();

void
write_rel ()
{
  register int i;
  register int count = 0;

  if (trace_files)
    fprintf (stderr, "Writing text relocation:\n\n");

  /* Assign each global symbol a sequence number, giving the order
     in which `write_syms' will write it.
     This is so we can store the proper symbolnum fields
     in relocation entries we write.  */

  for (i = 0; i < TABSIZE; i++)
    {
      symbol *sp;
      for (sp = symtab[i]; sp; sp = sp->link)
	if (sp->referenced || sp->defined)
	  {
	    sp->def_count = count++;
	    /* Leave room for the reference required by N_INDR, if
	       necessary.  */
	    if ((sp->defined & ~N_EXT) == N_INDR)
	      count++;
	  }
    }
  /* Correct, because if (relocatable_output), we will also be writing
     whatever indirect blocks we have.  */
  if (count != defined_global_sym_count
      + undefined_global_sym_count + global_indirect_count)
    fatal ("internal error");

  /* Write out the relocations of all files, remembered from copy_text.  */

  each_full_file (coptxtrel, 0);

  if (trace_files)
    fprintf (stderr, "\nWriting data relocation:\n\n");

  each_full_file (copdatrel, 0);

  if (trace_files)
    fprintf (stderr, "\n");
}

void
coptxtrel (entry)
     struct file_entry *entry;
{
  register struct relocation_info *p, *end;
  register int reloc = entry->text_start_address;

  p = entry->textrel;
  end = (struct relocation_info *) (entry->header.a_trsize + (char *) p);
  while (p < end)
    {
      RELOC_ADDRESS(p) += reloc;
      if (RELOC_EXTERN_P(p))
	{
	  register int symindex = RELOC_SYMBOL(p) * sizeof (struct nlist);
	  symbol *symptr = ((symbol *)
			    (((struct nlist *)
			      (((char *)entry->symbols) + symindex))
			     ->n_un.n_name));

	  if (symindex >= entry->header.a_syms)
	    fatal_with_file ("relocation symbolnum out of range in ", entry);

#ifdef N_INDR
	  /* Resolve indirection.  */
	  if ((symptr->defined & ~N_EXT) == N_INDR)
	    symptr = (symbol *) symptr->value;
#endif

	  /* If the symbol is now defined, change the external relocation
	     to an internal one.  */

	  if (symptr->defined)
	    {
	      RELOC_EXTERN_P(p) = 0;
	      RELOC_SYMBOL(p) = (symptr->defined & N_TYPE);
#ifdef RELOC_ADD_EXTRA
	      /* If we aren't going to be adding in the value in
	         memory on the next pass of the loader, then we need
		 to add it in from the relocation entry.  Otherwise
	         the work we did in this pass is lost.  */
	      if (!RELOC_MEMORY_ADD_P(p))
		RELOC_ADD_EXTRA (p) += symptr->value;
#endif
	    }
	  else
	    /* Debugger symbols come first, so have to start this
	       after them.  */
	      RELOC_SYMBOL(p) = (symptr->def_count + nsyms
				 - defined_global_sym_count
				 - undefined_global_sym_count
				 - global_indirect_count);
	}
      p++;
    }
  mywrite (entry->textrel, 1, entry->header.a_trsize, outdesc);
}

void
copdatrel (entry)
     struct file_entry *entry;
{
  register struct relocation_info *p, *end;
  /* Relocate the address of the relocation.
     Old address is relative to start of the input file's data section.
     New address is relative to start of the output file's data section.  */
  register int reloc = entry->data_start_address - text_size;

  p = entry->datarel;
  end = (struct relocation_info *) (entry->header.a_drsize + (char *) p);
  while (p < end)
    {
      RELOC_ADDRESS(p) += reloc;
      if (RELOC_EXTERN_P(p))
	{
	  register int symindex = RELOC_SYMBOL(p) * sizeof (struct nlist);
	  symbol *symptr = ((symbol *)
			    (((struct nlist *)
			      (((char *)entry->symbols) + symindex))
			     ->n_un.n_name));
	  int symtype;

	  if (symindex >= entry->header.a_syms)
	    fatal_with_file ("relocation symbolnum out of range in ", entry);

#ifdef N_INDR
	  /* Resolve indirection.  */
	  if ((symptr->defined & ~N_EXT) == N_INDR)
	    symptr = (symbol *) symptr->value;
#endif

	   symtype = symptr->defined & N_TYPE;

	  if (force_common_definition
	      || symtype == N_DATA || symtype == N_TEXT || symtype == N_ABS)
	    {
	      RELOC_EXTERN_P(p) = 0;
	      RELOC_SYMBOL(p) = symtype;
	    }
	  else
	    /* Debugger symbols come first, so have to start this
	       after them.  */
	    RELOC_SYMBOL(p)
	      = (((symbol *)
		  (((struct nlist *)
		    (((char *)entry->symbols) + symindex))
		   ->n_un.n_name))
		 ->def_count
		 + nsyms - defined_global_sym_count
		 - undefined_global_sym_count
		 - global_indirect_count);
	}
      p++;
    }
  mywrite (entry->datarel, 1, entry->header.a_drsize, outdesc);
}

void write_file_syms ();
void write_string_table ();

/* Offsets and current lengths of symbol and string tables in output file. */

int symbol_table_offset;
int symbol_table_len;

/* Address in output file where string table starts.  */
int string_table_offset;

/* Offset within string table
   where the strings in `strtab_vector' should be written.  */
int string_table_len;

/* Total size of string table strings allocated so far,
   including strings in `strtab_vector'.  */
int strtab_size;

/* Vector whose elements are strings to be added to the string table.  */
char **strtab_vector;

/* Vector whose elements are the lengths of those strings.  */
int *strtab_lens;

/* Index in `strtab_vector' at which the next string will be stored.  */
int strtab_index;

/* Add the string NAME to the output file string table.
   Record it in `strtab_vector' to be output later.
   Return the index within the string table that this string will have.  */

int
assign_string_table_index (name)
     char *name;
{
  register int index = strtab_size;
  register int len = strlen (name) + 1;

  strtab_size += len;
  strtab_vector[strtab_index] = name;
  strtab_lens[strtab_index++] = len;

  return index;
}

FILE *outstream = (FILE *) 0;

/* Write the contents of `strtab_vector' into the string table.
   This is done once for each file's local&debugger symbols
   and once for the global symbols.  */

void
write_string_table ()
{
  register int i;

  lseek (outdesc, string_table_offset + string_table_len, 0);

  if (!outstream)
    outstream = fdopen (outdesc, "w");

  for (i = 0; i < strtab_index; i++)
    {
      fwrite (strtab_vector[i], 1, strtab_lens[i], outstream);
      string_table_len += strtab_lens[i];
    }

  fflush (outstream);

  /* Report I/O error such as disk full.  */
  if (ferror (outstream))
    perror_name (output_filename);
}

/* Write the symbol table and string table of the output file.  */

void
write_syms ()
{
  /* Number of symbols written so far.  */
  int syms_written = 0;
  register int i;
  register symbol *sp;

  /* Buffer big enough for all the global symbols.  One
     extra struct for each indirect symbol to hold the extra reference
     following. */
  struct nlist *buf
    = (struct nlist *) alloca ((defined_global_sym_count
				+ undefined_global_sym_count
				+ global_indirect_count)
			       * sizeof (struct nlist));
  /* Pointer for storing into BUF.  */
  register struct nlist *bufp = buf;

  /* Size of string table includes the bytes that store the size.  */
  strtab_size = sizeof strtab_size;

  symbol_table_offset = N_SYMOFF (outheader);
  symbol_table_len = 0;
  string_table_offset = N_STROFF (outheader);
  string_table_len = strtab_size;

  if (strip_symbols == STRIP_ALL)
    return;

  /* Write the local symbols defined by the various files.  */

  each_file (write_file_syms, &syms_written);
  file_close ();

  /* Now write out the global symbols.  */

  /* Allocate two vectors that record the data to generate the string
     table from the global symbols written so far.  This must include
     extra space for the references following indirect outputs. */

  strtab_vector = (char **) alloca ((num_hash_tab_syms
				     + global_indirect_count) * sizeof (char *));
  strtab_lens = (int *) alloca ((num_hash_tab_syms
				 + global_indirect_count) * sizeof (int));
  strtab_index = 0;

  /* Scan the symbol hash table, bucket by bucket.  */

  for (i = 0; i < TABSIZE; i++)
    for (sp = symtab[i]; sp; sp = sp->link)
      {
	struct nlist nl;

	nl.n_other = 0;
	nl.n_desc = 0;

	/* Compute a `struct nlist' for the symbol.  */

	if (sp->defined || sp->referenced)
	  {
	    /* common condition needs to be before undefined condition */
	    /* because unallocated commons are set undefined in */
	    /* digest_symbols */
	    if (sp->defined > 1) /* defined with known type */
	      {
		/* If the target of an indirect symbol has been
		   defined and we are outputting an executable,
		   resolve the indirection; it's no longer needed */
		if (!relocatable_output
		    && ((sp->defined & N_TYPE) == N_INDR)
		    && (((symbol *) sp->value)->defined > 1))
		  {
		    symbol *newsp = (symbol *) sp->value;
		    nl.n_type = newsp->defined;
		    nl.n_value = newsp->value;
		  }
		else
		  {
		    nl.n_type = sp->defined;
		    if (sp->defined != (N_INDR | N_EXT))
		      nl.n_value = sp->value;
		    else
		      nl.n_value = 0;
		  }
	      }
	    else if (sp->max_common_size) /* defined as common but not allocated. */
	      {
		/* happens only with -r and not -d */
		/* write out a common definition */
		nl.n_type = N_UNDF | N_EXT;
		nl.n_value = sp->max_common_size;
	      }
	    else if (!sp->defined)	      /* undefined -- legit only if -r */
	      {
		nl.n_type = N_UNDF | N_EXT;
		nl.n_value = 0;
	      }
	    else
	      fatal ("internal error: %s defined in mysterious way", sp->name);

	    /* Allocate string table space for the symbol name.  */

	    nl.n_un.n_strx = assign_string_table_index (sp->name);

	    /* Output to the buffer and count it.  */

	    *bufp++ = nl;
	    syms_written++;
	    if (nl.n_type == (N_INDR | N_EXT))
	      {
		struct nlist xtra_ref;
		xtra_ref.n_type = N_EXT | N_UNDF;
		xtra_ref.n_un.n_strx
		  = assign_string_table_index (((symbol *) sp->value)->name);
		xtra_ref.n_other = 0;
		xtra_ref.n_desc = 0;
		xtra_ref.n_value = 0;
		*bufp++ = xtra_ref;
		syms_written++;
	      }
	  }
      }

  /* Output the buffer full of `struct nlist's.  */

  lseek (outdesc, symbol_table_offset + symbol_table_len, 0);
  mywrite (buf, sizeof (struct nlist), bufp - buf, outdesc);
  symbol_table_len += sizeof (struct nlist) * (bufp - buf);

  if (syms_written != nsyms)
    fatal ("internal error: wrong number of symbols written into output file", 0);

  if (symbol_table_offset + symbol_table_len != string_table_offset)
    fatal ("internal error: inconsistent symbol table length", 0);

  /* Now the total string table size is known, so write it.
     We are already positioned at the right place in the file.  */

  mywrite (&strtab_size, sizeof (int), 1, outdesc);  /* we're at right place */

  /* Write the strings for the global symbols.  */

  write_string_table ();
}

/* Write the local and debugger symbols of file ENTRY.
   Increment *SYMS_WRITTEN_ADDR for each symbol that is written.  */

/* Note that we do not combine identical names of local symbols.
   dbx or gdb would be confused if we did that.  */

void
write_file_syms (entry, syms_written_addr)
     struct file_entry *entry;
     int *syms_written_addr;
{
  register struct nlist *p = entry->symbols;
  register struct nlist *end = p + entry->header.a_syms / sizeof (struct nlist);

  /* Buffer to accumulate all the syms before writing them.
     It has one extra slot for the local symbol we generate here.  */
  struct nlist *buf
    = (struct nlist *) alloca (entry->header.a_syms + sizeof (struct nlist));
  register struct nlist *bufp = buf;

  /* Upper bound on number of syms to be written here.  */
  int max_syms = (entry->header.a_syms / sizeof (struct nlist)) + 1;

  /* Make tables that record, for each symbol, its name and its name's length.
     The elements are filled in by `assign_string_table_index'.  */

  strtab_vector = (char **) alloca (max_syms * sizeof (char *));
  strtab_lens = (int *) alloca (max_syms * sizeof (int));
  strtab_index = 0;

  /* Generate a local symbol for the start of this file's text.  */

  if (discard_locals != DISCARD_ALL)
    {
      struct nlist nl;

      nl.n_type = N_FN | N_EXT;
      nl.n_un.n_strx = assign_string_table_index (entry->local_sym_name);
      nl.n_value = entry->text_start_address;
      nl.n_desc = 0;
      nl.n_other = 0;
      *bufp++ = nl;
      (*syms_written_addr)++;
      entry->local_syms_offset = *syms_written_addr * sizeof (struct nlist);
    }

  /* Read the file's string table.  */

  entry->strings = (char *) alloca (entry->string_size);
  read_entry_strings (file_open (entry), entry);

  for (; p < end; p++)
    {
      register int type = p->n_type;
      register int write = 0;

      /* WRITE gets 1 for a non-global symbol that should be written.  */


      if (SET_ELEMENT_P (type))	/* This occurs even if global.  These */
				/* types of symbols are never written */
				/* globally, though they are stored */
				/* globally.  */
        write = relocatable_output;
      else if (!(type & (N_STAB | N_EXT)))
        /* ordinary local symbol */
	write = ((discard_locals != DISCARD_ALL)
		 && !(discard_locals == DISCARD_L &&
		      (p->n_un.n_strx + entry->strings)[0] == LPREFIX)
		 && type != N_WARNING);
      else if (!(type & N_EXT))
	/* debugger symbol */
        write = (strip_symbols == STRIP_NONE);

      if (write)
	{
	  /* If this symbol has a name,
	     allocate space for it in the output string table.  */

	  if (p->n_un.n_strx)
	    p->n_un.n_strx = assign_string_table_index (p->n_un.n_strx
							+ entry->strings);

	  /* Output this symbol to the buffer and count it.  */

	  *bufp++ = *p;
	  (*syms_written_addr)++;
	}
    }

  /* All the symbols are now in BUF; write them.  */

  lseek (outdesc, symbol_table_offset + symbol_table_len, 0);
  mywrite (buf, sizeof (struct nlist), bufp - buf, outdesc);
  symbol_table_len += sizeof (struct nlist) * (bufp - buf);

  /* Write the string-table data for the symbols just written,
     using the data in vectors `strtab_vector' and `strtab_lens'.  */

  write_string_table ();
  entry->strings = 0;		/* Since it will dissapear anyway.  */
}

/* Copy any GDB symbol segments from the input files to the output file.
   The contents of the symbol segment is copied without change
   except that we store some information into the beginning of it.  */

void write_file_symseg ();

void
write_symsegs ()
{
  each_file (write_file_symseg, 0);
}

void
write_file_symseg (entry)
     struct file_entry *entry;
{
  char buffer[4096];
  struct symbol_root root;
  int indesc;
  int len;

  if (entry->symseg_offset == 0)
    return;

  /* This entry has a symbol segment.  Read the root of the segment.  */

  indesc = file_open (entry);
  lseek (indesc, entry->symseg_offset + entry->starting_offset, 0);
  if (sizeof root != read (indesc, &root, sizeof root))
    fatal_with_file ("premature end of file in symbol segment of ", entry);

  /* Store some relocation info into the root.  */

  root.ldsymoff = entry->local_syms_offset;
  root.textrel = entry->text_start_address;
  root.datarel = entry->data_start_address - entry->header.a_text;
  root.bssrel = entry->bss_start_address
    - entry->header.a_text - entry->header.a_data;
  root.databeg = entry->data_start_address - root.datarel;
  root.bssbeg = entry->bss_start_address - root.bssrel;

  /* Write the modified root into the output file.  */

  mywrite (&root, sizeof root, 1, outdesc);

  /* Copy the rest of the symbol segment unchanged.  */

  if (entry->superfile)
    {
      /* Library member: number of bytes to copy is determined
	 from the member's total size.  */

      int total = entry->total_size - entry->symseg_offset - sizeof root;

      while (total > 0)
	{
	  len = read (indesc, buffer, min (sizeof buffer, total));

	  if (len != min (sizeof buffer, total))
	    fatal_with_file ("premature end of file in symbol segment of ", entry);
	  total -= len;
	  mywrite (buffer, len, 1, outdesc);
	}
    }
  else
    {
      /* A separate file: copy until end of file.  */

      while (len = read (indesc, buffer, sizeof buffer))
	{
	  mywrite (buffer, len, 1, outdesc);
	  if (len < sizeof buffer)
	    break;
	}
    }

  file_close ();
}

/* Create the symbol table entries for `etext', `edata' and `end'.  */

void
symtab_init ()
{
#ifndef nounderscore
  edata_symbol = getsym ("_edata");
  etext_symbol = getsym ("_etext");
  end_symbol = getsym ("_end");
#else
  edata_symbol = getsym ("edata");
  etext_symbol = getsym ("etext");
  end_symbol = getsym ("end");
#endif

#ifdef sun
  {
    symbol *dynamic_symbol = getsym ("__DYNAMIC");
    dynamic_symbol->defined = N_ABS | N_EXT;
    dynamic_symbol->referenced = 1;
    dynamic_symbol->value = 0;
  }
#endif

#ifdef sequent
  {
    symbol *_387_flt_symbol = getsym ("_387_flt");
    _387_flt_symbol->defined = N_ABS | N_EXT;
    _387_flt_symbol->referenced = 1;
    _387_flt_symbol->value = 0;
  }
#endif

  edata_symbol->defined = N_DATA | N_EXT;
  etext_symbol->defined = N_TEXT | N_EXT;
  end_symbol->defined = N_BSS | N_EXT;

  edata_symbol->referenced = 1;
  etext_symbol->referenced = 1;
  end_symbol->referenced = 1;
}

/* Compute the hash code for symbol name KEY.  */

int
hash_string (key)
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

/* Get the symbol table entry for the global symbol named KEY.
   Create one if there is none.  */

symbol *
getsym (key)
     char *key;
{
  register int hashval;
  register symbol *bp;

  /* Determine the proper bucket.  */

  hashval = hash_string (key) % TABSIZE;

  /* Search the bucket.  */

  for (bp = symtab[hashval]; bp; bp = bp->link)
    if (! strcmp (key, bp->name))
      return bp;

  /* Nothing was found; create a new symbol table entry.  */

  bp = (symbol *) xmalloc (sizeof (symbol));
  bp->refs = 0;
  bp->name = (char *) xmalloc (strlen (key) + 1);
  strcpy (bp->name, key);
  bp->defined = 0;
  bp->referenced = 0;
  bp->trace = 0;
  bp->value = 0;
  bp->max_common_size = 0;
  bp->warning = 0;
  bp->undef_refs = 0;
  bp->multiply_defined = 0;

  /* Add the entry to the bucket.  */

  bp->link = symtab[hashval];
  symtab[hashval] = bp;

  ++num_hash_tab_syms;

  return bp;
}

/* Like `getsym' but return 0 if the symbol is not already known.  */

symbol *
getsym_soft (key)
     char *key;
{
  register int hashval;
  register symbol *bp;

  /* Determine which bucket.  */

  hashval = hash_string (key) % TABSIZE;

  /* Search the bucket.  */

  for (bp = symtab[hashval]; bp; bp = bp->link)
    if (! strcmp (key, bp->name))
      return bp;

  return 0;
}

/* Report a fatal error.
   STRING is a printf format string and ARG is one arg for it.  */

void
fatal (string, arg)
     char *string, *arg;
{
  fprintf (stderr, "ld: ");
  fprintf (stderr, string, arg);
  fprintf (stderr, "\n");
  exit (1);
}

/* Report a fatal error.  The error message is STRING
   followed by the filename of ENTRY.  */

void
fatal_with_file (string, entry)
     char *string;
     struct file_entry *entry;
{
  fprintf (stderr, "ld: ");
  fprintf (stderr, string);
  print_file_name (entry, stderr);
  fprintf (stderr, "\n");
  exit (1);
}

/* Report a fatal error using the message for the last failed system call,
   followed by the string NAME.  */

void
perror_name (name)
     char *name;
{
  extern int errno, sys_nerr;
  extern char *sys_errlist[];
  char *s;

  if (errno < sys_nerr)
    s = concat ("", sys_errlist[errno], " for %s");
  else
    s = "cannot open %s";
  fatal (s, name);
}

/* Report a fatal error using the message for the last failed system call,
   followed by the name of file ENTRY.  */

void
perror_file (entry)
     struct file_entry *entry;
{
  extern int errno, sys_nerr;
  extern char *sys_errlist[];
  char *s;

  if (errno < sys_nerr)
    s = concat ("", sys_errlist[errno], " for ");
  else
    s = "cannot open ";
  fatal_with_file (s, entry);
}

/* Report a nonfatal error.
   STRING is a format for printf, and ARG1 ... ARG3 are args for it.  */

void
error (string, arg1, arg2, arg3)
     char *string, *arg1, *arg2, *arg3;
{
  fprintf (stderr, "%s: ", progname);
  fprintf (stderr, string, arg1, arg2, arg3);
  fprintf (stderr, "\n");
}


/* Output COUNT*ELTSIZE bytes of data at BUF
   to the descriptor DESC.  */

void
mywrite (buf, count, eltsize, desc)
     char *buf;
     int count;
     int eltsize;
     int desc;
{
  register int val;
  register int bytes = count * eltsize;

  while (bytes > 0)
    {
      val = write (desc, buf, bytes);
      if (val <= 0)
	perror_name (output_filename);
      buf += val;
      bytes -= val;
    }
}

/* Output PADDING zero-bytes to descriptor OUTDESC.
   PADDING may be negative; in that case, do nothing.  */

void
padfile (padding, outdesc)
     int padding;
     int outdesc;
{
  register char *buf;
  if (padding <= 0)
    return;

  buf = (char *) alloca (padding);
  bzero (buf, padding);
  mywrite (buf, padding, 1, outdesc);
}

/* Return a newly-allocated string
   whose contents concatenate the strings S1, S2, S3.  */

char *
concat (s1, s2, s3)
     char *s1, *s2, *s3;
{
  register int len1 = strlen (s1), len2 = strlen (s2), len3 = strlen (s3);
  register char *result = (char *) xmalloc (len1 + len2 + len3 + 1);

  strcpy (result, s1);
  strcpy (result + len1, s2);
  strcpy (result + len1 + len2, s3);
  result[len1 + len2 + len3] = 0;

  return result;
}

/* Parse the string ARG using scanf format FORMAT, and return the result.
   If it does not parse, report fatal error
   generating the error message using format string ERROR and ARG as arg.  */

int
parse (arg, format, error)
     char *arg, *format;
{
  int x;
  if (1 != sscanf (arg, format, &x))
    fatal (error, arg);
  return x;
}

/* Like malloc but get fatal error if memory is exhausted.  */

int
xmalloc (size)
     int size;
{
  register int result = malloc (size);
  if (!result)
    fatal ("virtual memory exhausted", 0);
  return result;
}

/* Like realloc but get fatal error if memory is exhausted.  */

int
xrealloc (ptr, size)
     char *ptr;
     int size;
{
  register int result = realloc (ptr, size);
  if (!result)
    fatal ("virtual memory exhausted", 0);
  return result;
}

#ifdef USG

void
bzero (p, n)
     char *p;
{
  memset (p, 0, n);
}

void
bcopy (from, to, n)
     char *from, *to;
{
  memcpy (to, from, n);
}

getpagesize ()
{
  return (4096);
}

#endif

#if defined(sun) && (TARGET == SUN4)

/* Don't use local pagesize to build for Sparc.  */

getpagesize ()
{
  return (8192);
}
#endif
