/* Generic symbol-table support for the BFD library.
   Copyright (C) 1990, 1991, 1992, 1993 Free Software Foundation, Inc.
   Written by Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
SECTION
	Symbols

	BFD tries to maintain as much symbol information as it can when
	it moves information from file to file. BFD passes information
	to applications though the <<asymbol>> structure. When the
	application requests the symbol table, BFD reads the table in
	the native form and translates parts of it into the internal
	format. To maintain more than the information passed to
	applications, some targets keep some information ``behind the
	scenes'' in a structure only the particular back end knows
	about. For example, the coff back end keeps the original
	symbol table structure as well as the canonical structure when
	a BFD is read in. On output, the coff back end can reconstruct
	the output symbol table so that no information is lost, even
	information unique to coff which BFD doesn't know or
	understand. If a coff symbol table were read, but were written
	through an a.out back end, all the coff specific information
	would be lost. The symbol table of a BFD
	is not necessarily read in until a canonicalize request is
	made. Then the BFD back end fills in a table provided by the
	application with pointers to the canonical information.  To
	output symbols, the application provides BFD with a table of
	pointers to pointers to <<asymbol>>s. This allows applications
	like the linker to output a symbol as it was read, since the ``behind
	the scenes'' information will be still available.
@menu
@* Reading Symbols::
@* Writing Symbols::
@* typedef asymbol::
@* symbol handling functions::
@end menu

INODE
Reading Symbols, Writing Symbols, Symbols, Symbols
SUBSECTION
	Reading symbols

	There are two stages to reading a symbol table from a BFD:
	allocating storage, and the actual reading process. This is an
	excerpt from an application which reads the symbol table:

|	  long storage_needed;
|	  asymbol **symbol_table;
|	  long number_of_symbols;
|	  long i;
|
|	  storage_needed = bfd_get_symtab_upper_bound (abfd);
|
|         if (storage_needed < 0)
|           FAIL
|
|	  if (storage_needed == 0) {
|	     return ;
|	  }
|	  symbol_table = (asymbol **) xmalloc (storage_needed);
|	    ...
|	  number_of_symbols =
|	     bfd_canonicalize_symtab (abfd, symbol_table);
|
|         if (number_of_symbols < 0)
|           FAIL
|
|	  for (i = 0; i < number_of_symbols; i++) {
|	     process_symbol (symbol_table[i]);
|	  }

	All storage for the symbols themselves is in an obstack
	connected to the BFD; it is freed when the BFD is closed.


INODE
Writing Symbols, typedef asymbol, Reading Symbols, Symbols
SUBSECTION
	Writing symbols

	Writing of a symbol table is automatic when a BFD open for
	writing is closed. The application attaches a vector of
	pointers to pointers to symbols to the BFD being written, and
	fills in the symbol count. The close and cleanup code reads
	through the table provided and performs all the necessary
	operations. The BFD output code must always be provided with an
	``owned'' symbol: one which has come from another BFD, or one
	which has been created using <<bfd_make_empty_symbol>>.  Here is an
	example showing the creation of a symbol table with only one element:

|	#include "bfd.h"
|	main()
|	{
|	  bfd *abfd;
|	  asymbol *ptrs[2];
|	  asymbol *new;
|
|	  abfd = bfd_openw("foo","a.out-sunos-big");
|	  bfd_set_format(abfd, bfd_object);
|	  new = bfd_make_empty_symbol(abfd);
|	  new->name = "dummy_symbol";
|	  new->section = bfd_make_section_old_way(abfd, ".text");
|	  new->flags = BSF_GLOBAL;
|	  new->value = 0x12345;
|
|	  ptrs[0] = new;
|	  ptrs[1] = (asymbol *)0;
|
|	  bfd_set_symtab(abfd, ptrs, 1);
|	  bfd_close(abfd);
|	}
|
|	./makesym
|	nm foo
|	00012345 A dummy_symbol

	Many formats cannot represent arbitary symbol information; for
 	instance, the <<a.out>> object format does not allow an
	arbitary number of sections. A symbol pointing to a section
	which is not one  of <<.text>>, <<.data>> or <<.bss>> cannot
	be described.

*/



/*
DOCDD
INODE
typedef asymbol, symbol handling functions, Writing Symbols, Symbols

*/
/*
SUBSECTION
	typedef asymbol

	An <<asymbol>> has the form:

*/

/*
CODE_FRAGMENT

.
.typedef struct symbol_cache_entry
.{
.	{* A pointer to the BFD which owns the symbol. This information
.	   is necessary so that a back end can work out what additional
.   	   information (invisible to the application writer) is carried
.	   with the symbol.
.
.	   This field is *almost* redundant, since you can use section->owner
.	   instead, except that some symbols point to the global sections
.	   bfd_{abs,com,und}_section.  This could be fixed by making
.	   these globals be per-bfd (or per-target-flavor).  FIXME. *}
.
.  struct _bfd *the_bfd; {* Use bfd_asymbol_bfd(sym) to access this field. *}
.
.	{* The text of the symbol. The name is left alone, and not copied; the
.	   application may not alter it. *}
.  CONST char *name;
.
.	{* The value of the symbol.  This really should be a union of a
.          numeric value with a pointer, since some flags indicate that
.          a pointer to another symbol is stored here.  *}
.  symvalue value;
.
.	{* Attributes of a symbol: *}
.
.#define BSF_NO_FLAGS    0x00
.
.	{* The symbol has local scope; <<static>> in <<C>>. The value
. 	   is the offset into the section of the data. *}
.#define BSF_LOCAL	0x01
.
.	{* The symbol has global scope; initialized data in <<C>>. The
.	   value is the offset into the section of the data. *}
.#define BSF_GLOBAL	0x02
.
.	{* The symbol has global scope and is exported. The value is
.	   the offset into the section of the data. *}
.#define BSF_EXPORT	BSF_GLOBAL {* no real difference *}
.
.	{* A normal C symbol would be one of:
.	   <<BSF_LOCAL>>, <<BSF_FORT_COMM>>,  <<BSF_UNDEFINED>> or
.	   <<BSF_GLOBAL>> *}
.
.	{* The symbol is a debugging record. The value has an arbitary
.	   meaning. *}
.#define BSF_DEBUGGING	0x08
.
.	{* The symbol denotes a function entry point.  Used in ELF,
.	   perhaps others someday.  *}
.#define BSF_FUNCTION    0x10
.
.	{* Used by the linker. *}
.#define BSF_KEEP        0x20
.#define BSF_KEEP_G      0x40
.
.	{* A weak global symbol, overridable without warnings by
.	   a regular global symbol of the same name.  *}
.#define BSF_WEAK        0x80
.
.       {* This symbol was created to point to a section, e.g. ELF's
.	   STT_SECTION symbols.  *}
.#define BSF_SECTION_SYM 0x100
.
.	{* The symbol used to be a common symbol, but now it is
.	   allocated. *}
.#define BSF_OLD_COMMON  0x200
.
.	{* The default value for common data. *}
.#define BFD_FORT_COMM_DEFAULT_VALUE 0
.
.	{* In some files the type of a symbol sometimes alters its
.	   location in an output file - ie in coff a <<ISFCN>> symbol
.	   which is also <<C_EXT>> symbol appears where it was
.	   declared and not at the end of a section.  This bit is set
.  	   by the target BFD part to convey this information. *}
.
.#define BSF_NOT_AT_END    0x400
.
.	{* Signal that the symbol is the label of constructor section. *}
.#define BSF_CONSTRUCTOR   0x800
.
.	{* Signal that the symbol is a warning symbol. If the symbol
.	   is a warning symbol, then the value field (I know this is
.	   tacky) will point to the asymbol which when referenced will
.	   cause the warning. *}
.#define BSF_WARNING       0x1000
.
.	{* Signal that the symbol is indirect. The value of the symbol
.	   is a pointer to an undefined asymbol which contains the
.	   name to use instead. *}
.#define BSF_INDIRECT      0x2000
.
.	{* BSF_FILE marks symbols that contain a file name.  This is used
.	   for ELF STT_FILE symbols.  *}
.#define BSF_FILE          0x4000
.
.	{* Symbol is from dynamic linking information.  *}
.#define BSF_DYNAMIC	   0x8000
.
.  flagword flags;
.
.	{* A pointer to the section to which this symbol is
.	   relative.  This will always be non NULL, there are special
.          sections for undefined and absolute symbols *}
.  struct sec *section;
.
.	{* Back end special data. This is being phased out in favour
.	   of making this a union. *}
.  PTR udata;
.
.} asymbol;
*/

#include "bfd.h"
#include "sysdep.h"

#include "libbfd.h"
#include "aout/stab_gnu.h"

/*
DOCDD
INODE
symbol handling functions,  , typedef asymbol, Symbols
SUBSECTION
	Symbol handling functions
*/

/*
FUNCTION
	bfd_get_symtab_upper_bound

DESCRIPTION
	Return the number of bytes required to store a vector of pointers
	to <<asymbols>> for all the symbols in the BFD @var{abfd},
	including a terminal NULL pointer. If there are no symbols in
	the BFD, then return 0.  If an error occurs, return -1.

.#define bfd_get_symtab_upper_bound(abfd) \
.     BFD_SEND (abfd, _bfd_get_symtab_upper_bound, (abfd))

*/

/*
FUNCTION
	bfd_is_local_label

SYNOPSIS
        boolean bfd_is_local_label(bfd *abfd, asymbol *sym);

DESCRIPTION
	Return true if the given symbol @var{sym} in the BFD @var{abfd} is
	a compiler generated local label, else return false.
.#define bfd_is_local_label(abfd, sym) \
.     BFD_SEND (abfd, _bfd_is_local_label,(abfd, sym))
*/

/*
FUNCTION
	bfd_canonicalize_symtab

DESCRIPTION
	Read the symbols from the BFD @var{abfd}, and fills in
	the vector @var{location} with pointers to the symbols and
	a trailing NULL.
	Return the actual number of symbol pointers, not
	including the NULL.


.#define bfd_canonicalize_symtab(abfd, location) \
.     BFD_SEND (abfd, _bfd_canonicalize_symtab,\
.                  (abfd, location))

*/


/*
FUNCTION
	bfd_set_symtab

SYNOPSIS
	boolean bfd_set_symtab (bfd *abfd, asymbol **location, unsigned int count);

DESCRIPTION
	Arrange that when the output BFD @var{abfd} is closed,
	the table @var{location} of @var{count} pointers to symbols
	will be written.
*/

boolean
bfd_set_symtab (abfd, location, symcount)
     bfd *abfd;
     asymbol **location;
     unsigned int symcount;
{
  if ((abfd->format != bfd_object) || (bfd_read_p (abfd)))
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  bfd_get_outsymbols (abfd) = location;
  bfd_get_symcount (abfd) = symcount;
  return true;
}

/*
FUNCTION
	bfd_print_symbol_vandf

SYNOPSIS
	void bfd_print_symbol_vandf(PTR file, asymbol *symbol);

DESCRIPTION
	Print the value and flags of the @var{symbol} supplied to the
	stream @var{file}.
*/
void
bfd_print_symbol_vandf (arg, symbol)
     PTR arg;
     asymbol *symbol;
{
  FILE *file = (FILE *) arg;
  flagword type = symbol->flags;
  if (symbol->section != (asection *) NULL)
    {
      fprintf_vma (file, symbol->value + symbol->section->vma);
    }
  else
    {
      fprintf_vma (file, symbol->value);
    }

  /* This presumes that a symbol can not be both BSF_DEBUGGING and
     BSF_DYNAMIC.  */
  fprintf (file, " %c%c%c%c%c%c%c",
	   (type & BSF_LOCAL) ? 'l' : ' ',
	   (type & BSF_GLOBAL) ? 'g' : ' ',
	   (type & BSF_WEAK) ? 'w' : ' ',
	   (type & BSF_CONSTRUCTOR) ? 'C' : ' ',
	   (type & BSF_WARNING) ? 'W' : ' ',
	   (type & BSF_INDIRECT) ? 'I' : ' ',
	   (type & BSF_DEBUGGING) ? 'd'
	   : (type & BSF_DYNAMIC) ? 'D' : ' ');
}


/*
FUNCTION
	bfd_make_empty_symbol

DESCRIPTION
	Create a new <<asymbol>> structure for the BFD @var{abfd}
	and return a pointer to it.

	This routine is necessary because each back end has private
	information surrounding the <<asymbol>>. Building your own
	<<asymbol>> and pointing to it will not create the private
	information, and will cause problems later on.

.#define bfd_make_empty_symbol(abfd) \
.     BFD_SEND (abfd, _bfd_make_empty_symbol, (abfd))
*/

/*
FUNCTION
	bfd_make_debug_symbol

DESCRIPTION
	Create a new <<asymbol>> structure for the BFD @var{abfd},
	to be used as a debugging symbol.  Further details of its use have
	yet to be worked out.

.#define bfd_make_debug_symbol(abfd,ptr,size) \
.        BFD_SEND (abfd, _bfd_make_debug_symbol, (abfd, ptr, size))
*/

struct section_to_type
{
  CONST char *section;
  char type;
};

/* Map section names to POSIX/BSD single-character symbol types.
   This table is probably incomplete.  It is sorted for convenience of
   adding entries.  Since it is so short, a linear search is used.  */
static CONST struct section_to_type stt[] =
{
  {"*DEBUG*", 'N'},
  {".bss", 'b'},
  {".data", 'd'},
  {".sbss", 's'},		/* Small BSS (uninitialized data) */
  {".scommon", 'c'},		/* Small common */
  {".sdata", 'g'},		/* Small initialized data */
  {".text", 't'},
  {0, 0}
};

/* Return the single-character symbol type corresponding to
   section S, or '?' for an unknown COFF section.  */

static char
coff_section_type (s)
     char *s;
{
  CONST struct section_to_type *t;

  for (t = &stt[0]; t->section; t++)
    if (!strcmp (s, t->section))
      return t->type;
  return '?';
}

#ifndef islower
#define islower(c) ((c) >= 'a' && (c) <= 'z')
#endif
#ifndef toupper
#define toupper(c) (islower(c) ? ((c) & ~0x20) : (c))
#endif

/*
FUNCTION
	bfd_decode_symclass

DESCRIPTION
	Return a character corresponding to the symbol
	class of @var{symbol}, or '?' for an unknown class.

SYNOPSIS
	int bfd_decode_symclass(asymbol *symbol);
*/
int
bfd_decode_symclass (symbol)
     asymbol *symbol;
{
  char c;

  if (bfd_is_com_section (symbol->section))
    return 'C';
  if (bfd_is_und_section (symbol->section))
    return 'U';
  if (bfd_is_ind_section (symbol->section))
    return 'I';
  if (!(symbol->flags & (BSF_GLOBAL | BSF_LOCAL)))
    return '?';

  if (bfd_is_abs_section (symbol->section))
    c = 'a';
  else if (symbol->section)
    c = coff_section_type (symbol->section->name);
  else
    return '?';
  if (symbol->flags & BSF_GLOBAL)
    c = toupper (c);
  return c;

  /* We don't have to handle these cases just yet, but we will soon:
     N_SETV: 'v';
     N_SETA: 'l';
     N_SETT: 'x';
     N_SETD: 'z';
     N_SETB: 's';
     N_INDR: 'i';
     */
}

/*
FUNCTION
	bfd_symbol_info

DESCRIPTION
	Fill in the basic info about symbol that nm needs.
	Additional info may be added by the back-ends after
	calling this function.

SYNOPSIS
	void bfd_symbol_info(asymbol *symbol, symbol_info *ret);
*/

void
bfd_symbol_info (symbol, ret)
     asymbol *symbol;
     symbol_info *ret;
{
  ret->type = bfd_decode_symclass (symbol);
  if (ret->type != 'U')
    ret->value = symbol->value + symbol->section->vma;
  else
    ret->value = 0;
  ret->name = symbol->name;
}

void
bfd_symbol_is_absolute ()
{
  abort ();
}
