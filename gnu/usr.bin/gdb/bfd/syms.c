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

	BFD trys to maintain as much symbol information as it can when
	it moves information from file to file. BFD passes information
	to applications though the <<asymbol>> structure. When the
	application requests the symbol table, BFD reads the table in
	the native form and translates parts of it into the internal
	format. To maintain more than the infomation passed to
	applications some targets keep some information `behind the
	scenes', in a structure only the particular back end knows
	about. For example, the coff back end keeps the original
	symbol table structure as well as the canonical structure when
	a BFD is read in. On output, the coff back end can reconstruct
	the output symbol table so that no information is lost, even
	information unique to coff which BFD doesn't know or
	understand. If a coff symbol table was read, but was written
	through an a.out back end, all the coff specific information
	would be lost. The symbol table of a BFD
	is not necessarily read in until a canonicalize request is
	made. Then the BFD back end fills in a table provided by the
	application with pointers to the canonical information.  To
	output symbols, the application provides BFD with a table of
	pointers to pointers to <<asymbol>>s. This allows applications
	like the linker to output a symbol as read, since the `behind
	the scenes' information will be still available. 
@menu
@* Reading Symbols::
@* Writing Symbols::
@* typedef asymbol::
@* symbol handling functions::
@end menu

INODE
Reading Symbols, Writing Symbols, Symbols, Symbols
SUBSECTION
	Reading Symbols

	There are two stages to reading a symbol table from a BFD;
	allocating storage, and the actual reading process. This is an
	excerpt from an appliction which reads the symbol table:

|	  unsigned int storage_needed;
|	  asymbol **symbol_table;
|	  unsigned int number_of_symbols;
|	  unsigned int i;
|	
|	  storage_needed = get_symtab_upper_bound (abfd);
|	
|	  if (storage_needed == 0) {
|	     return ;
|	  }
|	  symbol_table = (asymbol **) bfd_xmalloc (storage_needed);
|	    ...
|	  number_of_symbols = 
|	     bfd_canonicalize_symtab (abfd, symbol_table); 
|	
|	  for (i = 0; i < number_of_symbols; i++) {
|	     process_symbol (symbol_table[i]);
|	  }

	All storage for the symbols themselves is in an obstack
	connected to the BFD, and is freed when the BFD is closed.


INODE
Writing Symbols, typedef asymbol, Reading Symbols, Symbols
SUBSECTION
	Writing Symbols

	Writing of a symbol table is automatic when a BFD open for
	writing is closed. The application attaches a vector of
	pointers to pointers to symbols to the BFD being written, and
	fills in the symbol count. The close and cleanup code reads
	through the table provided and performs all the necessary
	operations. The outputing code must always be provided with an
	'owned' symbol; one which has come from another BFD, or one
	which has been created using <<bfd_make_empty_symbol>>.   An
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
 	instance the <<a.out>> object format does not allow an
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
.	{* The text of the symbol. The name is left alone, and not copied - the
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
.	{* The symbol has global scope, and is exported. The value is
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
	Symbol Handling Functions
*/

/*
FUNCTION
	get_symtab_upper_bound

DESCRIPTION
	Returns the number of bytes required in a vector of pointers
	to <<asymbols>> for all the symbols in the supplied BFD,
	including a terminal NULL pointer. If there are no symbols in
	the BFD, then 0 is returned.

.#define get_symtab_upper_bound(abfd) \
.     BFD_SEND (abfd, _get_symtab_upper_bound, (abfd))

*/

/*
FUNCTION
	bfd_canonicalize_symtab

DESCRIPTION
	Supplied a BFD and a pointer to an uninitialized vector of
	pointers. This reads in the symbols from the BFD, and fills in
	the table with pointers to the symbols, and a trailing NULL.
	The routine returns the actual number of symbol pointers not
	including the NULL.


.#define bfd_canonicalize_symtab(abfd, location) \
.     BFD_SEND (abfd, _bfd_canonicalize_symtab,\
.                  (abfd, location))

*/


/*
FUNCTION
	bfd_set_symtab

DESCRIPTION
	Provided a table of pointers to symbols and a count, writes to
	the output BFD the symbols when closed.

SYNOPSIS
	boolean bfd_set_symtab (bfd *, asymbol **, unsigned int );
*/

boolean
bfd_set_symtab (abfd, location, symcount)
     bfd *abfd;
     asymbol **location;
     unsigned int symcount;
{
  if ((abfd->format != bfd_object) || (bfd_read_p (abfd))) {
    bfd_error = invalid_operation;
    return false;
  }

  bfd_get_outsymbols (abfd) = location;
  bfd_get_symcount (abfd) = symcount;
  return true;
}

/*
FUNCTION
	bfd_print_symbol_vandf

DESCRIPTION
	Prints the value and flags of the symbol supplied to the stream file.

SYNOPSIS
	void bfd_print_symbol_vandf(PTR file, asymbol *symbol);
*/
void
DEFUN(bfd_print_symbol_vandf,(file, symbol),
PTR file AND
asymbol *symbol)
{
  flagword type = symbol->flags;
  if (symbol->section != (asection *)NULL)
      {
	fprintf_vma(file, symbol->value+symbol->section->vma);
      }
  else 
      {
	fprintf_vma(file, symbol->value);
      }
  fprintf(file," %c%c%c%c%c%c%c",
	  (type & BSF_LOCAL)  ? 'l':' ',
	  (type & BSF_GLOBAL) ? 'g' : ' ',
	  (type & BSF_WEAK) ? 'w' : ' ',
	  (type & BSF_CONSTRUCTOR) ? 'C' : ' ',
	  (type & BSF_WARNING) ? 'W' : ' ',
	  (type & BSF_INDIRECT) ? 'I' : ' ',
	  (type & BSF_DEBUGGING) ? 'd' :' ');

}


/*
FUNCTION
	bfd_make_empty_symbol

DESCRIPTION
	This function creates a new <<asymbol>> structure for the BFD,
	and returns a pointer to it.

	This routine is necessary, since each back end has private
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
	This function creates a new <<asymbol>> structure for the BFD,
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

/* Map COFF section names to POSIX/BSD single-character symbol types.
   This table is probably incomplete.  It is sorted for convenience of
   adding entries.  Since it is so short, a linear search is used.  */
static CONST struct section_to_type stt[] = {
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
   COFF section S, or '?' for an unknown COFF section.  */

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
	class of symbol, or '?' for an unknown class.

SYNOPSIS
	int bfd_decode_symclass(asymbol *symbol);
*/
int
DEFUN(bfd_decode_symclass,(symbol),
asymbol *symbol)
{
  char c;

  if (bfd_is_com_section (symbol->section))
    return 'C';
  if (symbol->section == &bfd_und_section)
    return 'U';
  if (symbol->section == &bfd_ind_section)
    return 'I';
  if (!(symbol->flags & (BSF_GLOBAL|BSF_LOCAL)))
    return '?';

  if (symbol->section == &bfd_abs_section)
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
DEFUN(bfd_symbol_info,(symbol, ret),
      asymbol *symbol AND
      symbol_info *ret)
{
  ret->type = bfd_decode_symclass (symbol);
  if (ret->type != 'U')
    ret->value = symbol->value+symbol->section->vma;
  else
    ret->value = 0;
  ret->name = symbol->name;
}

void
bfd_symbol_is_absolute()
{
  abort();
}

