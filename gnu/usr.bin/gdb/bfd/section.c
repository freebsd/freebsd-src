/* Object file "section" support for the BFD library.
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
	Sections

	Sections are supported in BFD in <<section.c>>.

	The raw data contained within a BFD is maintained through the
	section abstraction.  A single BFD may have any number of
	sections, and keeps hold of them by pointing to the first,
	each one points to the next in the list.

@menu
@* Section Input::
@* Section Output::
@* typedef asection::
@* section prototypes::
@end menu

INODE
Section Input, Section Output, Sections, Sections
SUBSECTION
	Section Input

	When a BFD is opened for reading, the section structures are
	created and attached to the BFD.

	Each section has a name which describes the section in the
	outside world - for example, <<a.out>> would contain at least
	three sections, called <<.text>>, <<.data>> and <<.bss>>. 

	Names need not be unique; for example a COFF file may have several
	sections named .data.

	Sometimes a BFD will contain more than the 'natural' number of
	sections. A back end may attach other sections containing
	constructor data, or an application may add a section (using
	bfd_make_section) to the sections attached to an already open
	BFD. For example, the linker creates a supernumary section
	<<COMMON>> for each input file's BFD to hold information about
	common storage.

	The raw data is not necessarily read in at the same time as
	the section descriptor is created. Some targets may leave the
	data in place until a <<bfd_get_section_contents>> call is
	made. Other back ends may read in all the data at once - For
	example; an S-record file has to be read once to determine the
	size of the data. An IEEE-695 file doesn't contain raw data in
	sections, but data and relocation expressions intermixed, so
	the data area has to be parsed to get out the data and
	relocations.

INODE
Section Output, typedef asection, Section Input, Sections

SUBSECTION
	Section Output

	To write a new object style BFD, the various sections to be
	written have to be created. They are attached to the BFD in
	the same way as input sections, data is written to the
	sections using <<bfd_set_section_contents>>.  

	Any program that creates or combines sections (e.g., the assembler
	and linker) must use the fields <<output_section>> and
	<<output_offset>> to indicate the file sections to which each
	section must be written.  (If the section is being created from
	scratch, <<output_section>> should probably point to the section
	itself, and <<output_offset>> should probably be zero.)

	The data to be written comes from input sections attached to
	the output sections.  The output section structure can be
	considered a filter for the input section, the output section
	determines the vma of the output data and the name, but the
	input section determines the offset into the output section of
	the data to be written.

	E.g., to create a section "O", starting at 0x100, 0x123 long,
	containing two subsections, "A" at offset 0x0 (ie at vma
	0x100) and "B" at offset 0x20 (ie at vma 0x120) the structures
	would look like:

|   section name          "A"
|     output_offset   0x00
|     size            0x20
|     output_section ----------->  section name    "O"
|                             |    vma             0x100
|   section name          "B" |    size            0x123
|     output_offset   0x20    |
|     size            0x103   |
|     output_section  --------|


SUBSECTION
	Seglets

	The data within a section is stored in a <<seglet>>.  These
	are much like the fixups in <<gas>>.  The seglet abstraction
	allows the a section to grow and shrink within itself.

	A seglet knows how big it is, and which is the next seglet and
	where the raw data for it is, and also points to a list of
	relocations which apply to it.

	The seglet is used by the linker to perform relaxing on final
	code.  The application creates code which is as big as
	necessary to make it work without relaxing, and the user can
	select whether to relax.  Sometimes relaxing takes a lot of
	time.  The linker runs around the relocations to see if any
	are attached to data which can be shrunk, if so it does it on
	a seglet by seglet basis.

*/


#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"


/*
DOCDD
INODE
typedef asection, section prototypes, Section Output, Sections
SUBSECTION
	typedef asection

	The shape of a section struct:

CODE_FRAGMENT
.
.typedef struct sec 
.{
.        {* The name of the section, the name isn't a copy, the pointer is
.        the same as that passed to bfd_make_section. *}
.
.    CONST char *name;
.
.        {* Which section is it 0.nth      *}
.
.   int index;                      
.
.        {* The next section in the list belonging to the BFD, or NULL. *}
.
.    struct sec *next;
.
.        {* The field flags contains attributes of the section. Some of
.           flags are read in from the object file, and some are
.           synthesized from other information.  *}         
.
.    flagword flags;
.
.#define SEC_NO_FLAGS   0x000
.
.        {* Tells the OS to allocate space for this section when loaded.
.           This would clear for a section containing debug information
.           only. *}
.#define SEC_ALLOC      0x001
.          
.        {* Tells the OS to load the section from the file when loading.
.           This would be clear for a .bss section *}
.#define SEC_LOAD       0x002
.
.        {* The section contains data still to be relocated, so there will
.           be some relocation information too. *}
.#define SEC_RELOC      0x004
.
.#if 0   {* Obsolete ? *}
.#define SEC_BALIGN     0x008
.#endif
.
.        {* A signal to the OS that the section contains read only
.          data. *}
.#define SEC_READONLY   0x010
.
.        {* The section contains code only. *}
.#define SEC_CODE       0x020
.
.        {* The section contains data only. *}
.#define SEC_DATA       0x040
.
.        {* The section will reside in ROM. *}
.#define SEC_ROM        0x080
.
.        {* The section contains constructor information. This section
.           type is used by the linker to create lists of constructors and
.           destructors used by <<g++>>. When a back end sees a symbol
.           which should be used in a constructor list, it creates a new
.           section for the type of name (eg <<__CTOR_LIST__>>), attaches
.           the symbol to it and builds a relocation. To build the lists
.           of constructors, all the linker has to do is catenate all the
.           sections called <<__CTOR_LIST__>> and relocte the data
.           contained within - exactly the operations it would peform on
.           standard data. *}
.#define SEC_CONSTRUCTOR 0x100
.
.        {* The section is a constuctor, and should be placed at the
.          end of the text, data, or bss section(?). *}
.#define SEC_CONSTRUCTOR_TEXT 0x1100
.#define SEC_CONSTRUCTOR_DATA 0x2100
.#define SEC_CONSTRUCTOR_BSS  0x3100
.
.        {* The section has contents - a data section could be
.           <<SEC_ALLOC>> | <<SEC_HAS_CONTENTS>>, a debug section could be
.           <<SEC_HAS_CONTENTS>> *}
.#define SEC_HAS_CONTENTS 0x200
.
.        {* An instruction to the linker not to output sections
.          containing this flag even if they have information which
.          would normally be written. *}
.#define SEC_NEVER_LOAD 0x400
.
.        {* The section is a shared library section.  The linker must leave
.           these completely alone, as the vma and size are used when
.           the executable is loaded. *}
.#define SEC_SHARED_LIBRARY 0x800
.
.        {* The section is a common section (symbols may be defined
.           multiple times, the value of a symbol is the amount of
.           space it requires, and the largest symbol value is the one
.           used).  Most targets have exactly one of these (which we
.	    translate to bfd_com_section), but ECOFF has two. *}
.#define SEC_IS_COMMON 0x8000
.
.        {* The section contains only debugging information.  For
.           example, this is set for ELF .debug and .stab sections.
.           strip tests this flag to see if a section can be
.           discarded. *}
.#define SEC_DEBUGGING 0x10000
.
.	{*  End of section flags.  *}
.
.       {*  The virtual memory address of the section - where it will be
.           at run time.  The symbols are relocated against this.  The
.	    user_set_vma flag is maintained by bfd; if it's not set, the
.	    backend can assign addresses (for example, in <<a.out>>, where
.	    the default address for <<.data>> is dependent on the specific
.	    target and various flags).  *}
.
.   bfd_vma vma;
.   boolean user_set_vma;
.
.       {*  The load address of the section - where it would be in a
.           rom image, really only used for writing section header
.	    information. *}
.
.   bfd_vma lma;
.
.        {* The size of the section in bytes, as it will be output.
.           contains a value even if the section has no contents (eg, the
.           size of <<.bss>>). This will be filled in after relocation *}
.
.   bfd_size_type _cooked_size;    
.
.        {* The size on disk of the section in bytes originally.  Normally this
.	    value is the same as the size, but if some relaxing has
.	    been done, then this value will be bigger.  *}
.
.   bfd_size_type _raw_size;    
.
.        {* If this section is going to be output, then this value is the
.           offset into the output section of the first byte in the input
.           section. Eg, if this was going to start at the 100th byte in
.           the output section, this value would be 100. *}
.
.   bfd_vma output_offset;
.
.        {* The output section through which to map on output. *}
.
.   struct sec *output_section;
.
.        {* The alignment requirement of the section, as an exponent - eg
.           3 aligns to 2^3 (or 8) *}
.
.   unsigned int alignment_power;
.
.        {* If an input section, a pointer to a vector of relocation
.           records for the data in this section. *}
.
.   struct reloc_cache_entry *relocation;
.
.        {* If an output section, a pointer to a vector of pointers to
.           relocation records for the data in this section. *}
.
.   struct reloc_cache_entry **orelocation;
.
.        {* The number of relocation records in one of the above  *}
.
.   unsigned reloc_count;
.
.        {* Information below is back end specific - and not always used
.           or updated.  *}
.
.        {* File position of section data    *}
.
.   file_ptr filepos;      
.        
.        {* File position of relocation info *}
.
.   file_ptr rel_filepos;
.
.        {* File position of line data       *}
.
.   file_ptr line_filepos;
.
.        {* Pointer to data for applications *}
.
.   PTR userdata;
.
.   struct lang_output_section *otheruserdata;
.
.        {* Attached line number information *}
.
.   alent *lineno;
.        
.        {* Number of line number records   *}
.
.   unsigned int lineno_count;
.
.        {* When a section is being output, this value changes as more
.           linenumbers are written out *}
.
.   file_ptr moving_line_filepos;
.
.        {* what the section number is in the target world  *}
.
.   int target_index;
.
.   PTR used_by_bfd;
.
.        {* If this is a constructor section then here is a list of the
.           relocations created to relocate items within it. *}
.
.   struct relent_chain *constructor_chain;
.
.        {* The BFD which owns the section. *}
.
.   bfd *owner;
.
.   boolean reloc_done;
.	 {* A symbol which points at this section only *}
.   struct symbol_cache_entry *symbol;  
.   struct symbol_cache_entry **symbol_ptr_ptr;
.
.   struct bfd_seclet *seclets_head;
.   struct bfd_seclet *seclets_tail;
.} asection ;
.
.
.    {* These sections are global, and are managed by BFD.  The application
.       and target back end are not permitted to change the values in
.	these sections.  *}
.#define BFD_ABS_SECTION_NAME "*ABS*"
.#define BFD_UND_SECTION_NAME "*UND*"
.#define BFD_COM_SECTION_NAME "*COM*"
.#define BFD_IND_SECTION_NAME "*IND*"
.
.    {* the absolute section *}
.extern asection bfd_abs_section;
.    {* Pointer to the undefined section *}
.extern asection bfd_und_section;
.    {* Pointer to the common section *}
.extern asection bfd_com_section;
.    {* Pointer to the indirect section *}
.extern asection bfd_ind_section;
.
.extern struct symbol_cache_entry *bfd_abs_symbol;
.extern struct symbol_cache_entry *bfd_com_symbol;
.extern struct symbol_cache_entry *bfd_und_symbol;
.extern struct symbol_cache_entry *bfd_ind_symbol;
.#define bfd_get_section_size_before_reloc(section) \
.     (section->reloc_done ? (abort(),1): (section)->_raw_size)
.#define bfd_get_section_size_after_reloc(section) \
.     ((section->reloc_done) ? (section)->_cooked_size: (abort(),1))
*/

/* These symbols are global, not specific to any BFD.  Therefore, anything
   that tries to change them is broken, and should be repaired.  */
static CONST asymbol global_syms[] = {
  /* the_bfd, name, value, attr, section [, udata] */
  { 0, BFD_COM_SECTION_NAME, 0, BSF_SECTION_SYM, &bfd_com_section },
  { 0, BFD_UND_SECTION_NAME, 0, BSF_SECTION_SYM, &bfd_und_section },
  { 0, BFD_ABS_SECTION_NAME, 0, BSF_SECTION_SYM, &bfd_abs_section },
  { 0, BFD_IND_SECTION_NAME, 0, BSF_SECTION_SYM, &bfd_ind_section },
};

#define STD_SECTION(SEC, FLAGS, SYM, NAME, IDX)	\
  asymbol *SYM = (asymbol *) &global_syms[IDX]; \
  asection SEC = { NAME, 0, 0, FLAGS, 0, 0, (boolean) 0, 0, 0, 0, &SEC,\
		    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, (boolean) 0, \
		     (asymbol *) &global_syms[IDX], &SYM, }

STD_SECTION (bfd_com_section, SEC_IS_COMMON, bfd_com_symbol, BFD_COM_SECTION_NAME, 0);
STD_SECTION (bfd_und_section, 0, bfd_und_symbol, BFD_UND_SECTION_NAME, 1);
STD_SECTION (bfd_abs_section, 0, bfd_abs_symbol, BFD_ABS_SECTION_NAME, 2);
STD_SECTION (bfd_ind_section, 0, bfd_ind_symbol, BFD_IND_SECTION_NAME, 3);
#undef STD_SECTION

/*
DOCDD
INODE
section prototypes,  , typedef asection, Sections
SUBSECTION
	section prototypes

These are the functions exported by the section handling part of
<<libbfd>.
*/

/*
FUNCTION 
	bfd_get_section_by_name

SYNOPSIS
	asection *bfd_get_section_by_name(bfd *abfd, CONST char *name);

DESCRIPTION
	Runs through the provided @var{abfd} and returns the one of the
	<<asection>>s who's name matches that provided, otherwise NULL.
	@xref{Sections}, for more information.

	This should only be used in special cases; the normal way to process
	all sections of a given name is to use bfd_map_over_sections and
	strcmp on the name (or better yet, base it on the section flags
	or something else) for each section.
*/

asection *
DEFUN(bfd_get_section_by_name,(abfd, name),
      bfd *abfd AND
      CONST char *name)
{
  asection *sect;

  for (sect = abfd->sections; sect != NULL; sect = sect->next)
    if (!strcmp (sect->name, name)) return sect;
  return NULL;
}


/*
FUNCTION
	bfd_make_section_old_way

SYNOPSIS
	asection *bfd_make_section_old_way(bfd *, CONST char *name);

DESCRIPTION
	This function creates a new empty section called @var{name}
	and attaches it to the end of the chain of sections for the
	BFD supplied. An attempt to create a section with a name which
	is already in use, returns its pointer without changing the
	section chain.

	It has the funny name since this is the way it used to be
	before is was rewritten...

	Possible errors are:
	o invalid_operation -
	If output has already started for this BFD.
	o no_memory -
	If obstack alloc fails.

*/


asection *
DEFUN(bfd_make_section_old_way,(abfd, name),
      bfd *abfd AND
      CONST char * name)
{
  asection *sec = bfd_get_section_by_name(abfd, name);
  if (sec == (asection *)NULL) 
    {
      sec = bfd_make_section(abfd, name);
    }
  return sec;
}

/*
FUNCTION
	bfd_make_section_anyway

SYNOPSIS
	asection *bfd_make_section_anyway(bfd *, CONST char *name);

DESCRIPTION
   Create a new empty section called @var{name} and attach it to the end of
   the chain of sections for @var{abfd}.  Create a new section even if there
   is already a section with that name.  

   Returns NULL and sets bfd_error on error; possible errors are:
   o invalid_operation - If output has already started for @var{abfd}.
   o no_memory - If obstack alloc fails.
*/

sec_ptr
bfd_make_section_anyway (abfd, name)
     bfd *abfd;
     CONST char *name;
{
  asection *newsect;
  asection **prev = &abfd->sections;
  asection * sect = abfd->sections;

  if (abfd->output_has_begun)
    {
      bfd_error = invalid_operation;
      return NULL;
    }

  while (sect) {
    prev = &sect->next;
    sect = sect->next;
  }

  newsect = (asection *) bfd_zalloc(abfd, sizeof (asection));
  if (newsect == NULL) {
    bfd_error = no_memory;
    return NULL;
  }

  newsect->name = name;
  newsect->index = abfd->section_count++;
  newsect->flags = SEC_NO_FLAGS;

  newsect->userdata = 0;
  newsect->next = (asection *)NULL;
  newsect->relocation = (arelent *)NULL;
  newsect->reloc_count = 0;
  newsect->line_filepos =0;
  newsect->owner = abfd;

  /* Create a symbol whos only job is to point to this section. This is
     useful for things like relocs which are relative to the base of a
     section.  */
  newsect->symbol = bfd_make_empty_symbol(abfd);
  newsect->symbol->name = name;
  newsect->symbol->value = 0;
  newsect->symbol->section = newsect;
  newsect->symbol->flags = BSF_SECTION_SYM;

  newsect->symbol_ptr_ptr = &newsect->symbol;

  if (BFD_SEND (abfd, _new_section_hook, (abfd, newsect)) != true) {
    free (newsect);
    return NULL;
  }

  *prev = newsect;
  return newsect;
}

/*
FUNCTION
	bfd_make_section

SYNOPSIS
	asection *bfd_make_section(bfd *, CONST char *name);

DESCRIPTION
   Like bfd_make_section_anyway, but return NULL (without setting
   bfd_error) without changing the section chain if there is already a
   section named @var{name}.  If there is an error, return NULL and set
   bfd_error.
*/

sec_ptr
DEFUN(bfd_make_section,(abfd, name),
      bfd *abfd AND
      CONST char * name)
{
  asection * sect = abfd->sections;

  if (strcmp(name, BFD_ABS_SECTION_NAME) == 0) 
  {
    return &bfd_abs_section;
  }
  if (strcmp(name, BFD_COM_SECTION_NAME) == 0) 
  {
    return &bfd_com_section;
  }
  if (strcmp(name, BFD_UND_SECTION_NAME) == 0) 
  {
    return &bfd_und_section;
  }

  if (strcmp(name, BFD_IND_SECTION_NAME) == 0) 
  {
    return &bfd_ind_section;
  }

  while (sect) {
    if (!strcmp(sect->name, name)) return NULL;
    sect = sect->next;
  }

  /* The name is not already used; go ahead and make a new section.  */
  return bfd_make_section_anyway (abfd, name);
}


/*
FUNCTION
	bfd_set_section_flags

SYNOPSIS
	boolean bfd_set_section_flags(bfd *, asection *, flagword);

DESCRIPTION
	Attempts to set the attributes of the section named in the BFD
	supplied to the value. Returns true on success, false on
	error. Possible error returns are:

	o invalid operation -
	The section cannot have one or more of the attributes
	requested. For example, a .bss section in <<a.out>> may not
	have the <<SEC_HAS_CONTENTS>> field set.

*/

boolean
DEFUN(bfd_set_section_flags,(abfd, section, flags),
     bfd *abfd AND
     sec_ptr section AND
     flagword flags)
{
#if 0
  /* If you try to copy a text section from an input file (where it
     has the SEC_CODE flag set) to an output file, this loses big if
     the bfd_applicable_section_flags (abfd) doesn't have the SEC_CODE
     set - which it doesn't, at least not for a.out.  FIXME */

  if ((flags & bfd_applicable_section_flags (abfd)) != flags) {
    bfd_error = invalid_operation;
    return false;
  }
#endif

  section->flags = flags;
  return true;
}


/*
FUNCTION
	bfd_map_over_sections

SYNOPSIS
	void bfd_map_over_sections(bfd *abfd,
				   void (*func)(bfd *abfd,
						asection *sect,
						PTR obj),
				   PTR obj);

DESCRIPTION
	Calls the provided function @var{func} for each section
	attached to the BFD @var{abfd}, passing @var{obj} as an
	argument. The function will be called as if by 

|	func(abfd, the_section, obj);

	This is the prefered method for iterating over sections, an
	alternative would be to use a loop:

|	   section *p;
|	   for (p = abfd->sections; p != NULL; p = p->next)
|	      func(abfd, p, ...)


*/

/*VARARGS2*/
void
DEFUN(bfd_map_over_sections,(abfd, operation, user_storage),
      bfd *abfd AND
      void (*operation) PARAMS ((bfd *abfd, asection *sect, PTR obj)) AND
      PTR user_storage)
{
  asection *sect;
  int i = 0;
  
  for (sect = abfd->sections; sect != NULL; i++, sect = sect->next)
    (*operation) (abfd, sect, user_storage);

  if (i != abfd->section_count)         /* Debugging */
    abort();
}


/*
FUNCTION
	bfd_set_section_size

SYNOPSIS
	boolean bfd_set_section_size(bfd *, asection *, bfd_size_type val);

DESCRIPTION
	Sets @var{section} to the size @var{val}. If the operation is
	ok, then <<true>> is returned, else <<false>>. 

	Possible error returns:
	o invalid_operation -
	Writing has started to the BFD, so setting the size is invalid

*/

boolean
DEFUN(bfd_set_section_size,(abfd, ptr, val),
      bfd *abfd AND
      sec_ptr ptr AND
      bfd_size_type val)
{
  /* Once you've started writing to any section you cannot create or change
     the size of any others. */

  if (abfd->output_has_begun) {
    bfd_error = invalid_operation;
    return false;
  }

  ptr->_cooked_size = val;
  ptr->_raw_size = val;
  
  return true;
}

/*
FUNCTION
	bfd_set_section_contents

SYNOPSIS
	boolean bfd_set_section_contents
         (bfd *abfd,        
         asection *section,
         PTR data,
         file_ptr offset,
         bfd_size_type count);


DESCRIPTION
	Sets the contents of the section @var{section} in BFD
	@var{abfd} to the data starting in memory at @var{data}. The
	data is written to the output section starting at offset
	@var{offset} for @var{count} bytes. 



	Normally <<true>> is returned, else <<false>>. Possible error
	returns are:
	o no_contents -
	The output section does not have the <<SEC_HAS_CONTENTS>>
	attribute, so nothing can be written to it.
	o and some more too

	This routine is front end to the back end function
	<<_bfd_set_section_contents>>.


*/

#define bfd_get_section_size_now(abfd,sec) \
(sec->reloc_done \
 ? bfd_get_section_size_after_reloc (sec) \
 : bfd_get_section_size_before_reloc (sec))

boolean
DEFUN(bfd_set_section_contents,(abfd, section, location, offset, count),
      bfd *abfd AND
      sec_ptr section AND
      PTR location AND
      file_ptr offset AND
      bfd_size_type count)
{
  bfd_size_type sz;

  if (!bfd_get_section_flags(abfd, section) & SEC_HAS_CONTENTS) 
      {
        bfd_error = no_contents;
        return(false);
      }

  if (offset < 0)
    {
    bad_val:
      bfd_error = bad_value;
      return false;
    }
  sz = bfd_get_section_size_now (abfd, section);
  if (offset > sz
      || count > sz
      || offset + count > sz)
    goto bad_val;

  switch (abfd->direction)
    {
      case read_direction:
      case no_direction:
	bfd_error = invalid_operation;
	return false;

      case write_direction:
	break;

      case both_direction:
	/* File is opened for update. `output_has_begun' some time ago when
	   the file was created.  Do not recompute sections sizes or alignments
	   in _bfd_set_section_content.  */
	abfd->output_has_begun = true;
	break;
    }

  if (BFD_SEND (abfd, _bfd_set_section_contents,
                (abfd, section, location, offset, count))) 
      {
        abfd->output_has_begun = true;
        return true;
      }

  return false;
}

/*
FUNCTION
	bfd_get_section_contents

SYNOPSIS
	boolean bfd_get_section_contents 
        (bfd *abfd, asection *section, PTR location,
         file_ptr offset, bfd_size_type count);

DESCRIPTION
	This function reads data from @var{section} in BFD @var{abfd}
	into memory starting at @var{location}. The data is read at an
	offset of @var{offset} from the start of the input section,
	and is read for @var{count} bytes.

	If the contents of a constuctor with the <<SEC_CONSTUCTOR>>
	flag set are requested, then the @var{location} is filled with
	zeroes. If no errors occur, <<true>> is returned, else
	<<false>>.



*/
boolean
DEFUN(bfd_get_section_contents,(abfd, section, location, offset, count),
      bfd *abfd AND
      sec_ptr section AND
      PTR location AND
      file_ptr offset AND
      bfd_size_type count)
{
  bfd_size_type sz;

  if (section->flags & SEC_CONSTRUCTOR) 
    {
      memset(location, 0, (unsigned)count);
      return true;
    }

  if (offset < 0)
    {
    bad_val:
      bfd_error = bad_value;
      return false;
    }
  sz = bfd_get_section_size_now (abfd, section);
  if (offset > sz
      || count > sz
      || offset + count > sz)
    goto bad_val;

  if (count == 0)
    /* Don't bother.  */
    return true;

  return BFD_SEND (abfd, _bfd_get_section_contents,
		   (abfd, section, location, offset, count));
}
