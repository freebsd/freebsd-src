/* Object file "section" support for the BFD library.
   Copyright (C) 1990, 91, 92, 93, 94, 95, 96, 97, 98, 1999
   Free Software Foundation, Inc.
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
SECTION
	Sections

	The raw data contained within a BFD is maintained through the
	section abstraction.  A single BFD may have any number of
	sections.  It keeps hold of them by pointing to the first;
	each one points to the next in the list.

	Sections are supported in BFD in <<section.c>>.

@menu
@* Section Input::
@* Section Output::
@* typedef asection::
@* section prototypes::
@end menu

INODE
Section Input, Section Output, Sections, Sections
SUBSECTION
	Section input

	When a BFD is opened for reading, the section structures are
	created and attached to the BFD.

	Each section has a name which describes the section in the
	outside world---for example, <<a.out>> would contain at least
	three sections, called <<.text>>, <<.data>> and <<.bss>>.

	Names need not be unique; for example a COFF file may have several
	sections named <<.data>>.

	Sometimes a BFD will contain more than the ``natural'' number of
	sections. A back end may attach other sections containing
	constructor data, or an application may add a section (using
	<<bfd_make_section>>) to the sections attached to an already open
	BFD. For example, the linker creates an extra section
	<<COMMON>> for each input file's BFD to hold information about
	common storage.

	The raw data is not necessarily read in when
	the section descriptor is created. Some targets may leave the
	data in place until a <<bfd_get_section_contents>> call is
	made. Other back ends may read in all the data at once.  For
	example, an S-record file has to be read once to determine the
	size of the data. An IEEE-695 file doesn't contain raw data in
	sections, but data and relocation expressions intermixed, so
	the data area has to be parsed to get out the data and
	relocations.

INODE
Section Output, typedef asection, Section Input, Sections

SUBSECTION
	Section output

	To write a new object style BFD, the various sections to be
	written have to be created. They are attached to the BFD in
	the same way as input sections; data is written to the
	sections using <<bfd_set_section_contents>>.

	Any program that creates or combines sections (e.g., the assembler
	and linker) must use the <<asection>> fields <<output_section>> and
	<<output_offset>> to indicate the file sections to which each
	section must be written.  (If the section is being created from
	scratch, <<output_section>> should probably point to the section
	itself and <<output_offset>> should probably be zero.)

	The data to be written comes from input sections attached
	(via <<output_section>> pointers) to
	the output sections.  The output section structure can be
	considered a filter for the input section: the output section
	determines the vma of the output data and the name, but the
	input section determines the offset into the output section of
	the data to be written.

	E.g., to create a section "O", starting at 0x100, 0x123 long,
	containing two subsections, "A" at offset 0x0 (i.e., at vma
	0x100) and "B" at offset 0x20 (i.e., at vma 0x120) the <<asection>>
	structures would look like:

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
	Link orders

	The data within a section is stored in a @dfn{link_order}.
	These are much like the fixups in <<gas>>.  The link_order
	abstraction allows a section to grow and shrink within itself.

	A link_order knows how big it is, and which is the next
	link_order and where the raw data for it is; it also points to
	a list of relocations which apply to it.

	The link_order is used by the linker to perform relaxing on
	final code.  The compiler creates code which is as big as
	necessary to make it work without relaxing, and the user can
	select whether to relax.  Sometimes relaxing takes a lot of
	time.  The linker runs around the relocations to see if any
	are attached to data which can be shrunk, if so it does it on
	a link_order by link_order basis.

*/


#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"

/*
DOCDD
INODE
typedef asection, section prototypes, Section Output, Sections
SUBSECTION
	typedef asection

	Here is the section structure:

CODE_FRAGMENT
.
. {* This structure is used for a comdat section, as in PE.  A comdat
.    section is associated with a particular symbol.  When the linker
.    sees a comdat section, it keeps only one of the sections with a
.    given name and associated with a given symbol. *}
.
.struct bfd_comdat_info
.{
.  {* The name of the symbol associated with a comdat section.  *}
.  const char *name;
.
.  {* The local symbol table index of the symbol associated with a
.     comdat section.  This is only meaningful to the object file format
.     specific code; it is not an index into the list returned by
.     bfd_canonicalize_symtab.  *}
.  long symbol;
.
.  {* If this section is being discarded, the linker uses this field
.     to point to the input section which is being kept.  *}
.  struct sec *sec;
.};
.
.typedef struct sec
.{
.        {* The name of the section; the name isn't a copy, the pointer is
.        the same as that passed to bfd_make_section. *}
.
.    CONST char *name;
.
.        {* Which section is it; 0..nth.      *}
.
.   int index;
.
.        {* The next section in the list belonging to the BFD, or NULL. *}
.
.    struct sec *next;
.
.        {* The field flags contains attributes of the section. Some
.           flags are read in from the object file, and some are
.           synthesized from other information.  *}
.
.    flagword flags;
.
.#define SEC_NO_FLAGS   0x000
.
.        {* Tells the OS to allocate space for this section when loading.
.           This is clear for a section containing debug information
.           only. *}
.#define SEC_ALLOC      0x001
.
.        {* Tells the OS to load the section from the file when loading.
.           This is clear for a .bss section. *}
.#define SEC_LOAD       0x002
.
.        {* The section contains data still to be relocated, so there is
.           some relocation information too. *}
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
.           section for the type of name (e.g., <<__CTOR_LIST__>>), attaches
.           the symbol to it, and builds a relocation. To build the lists
.           of constructors, all the linker has to do is catenate all the
.           sections called <<__CTOR_LIST__>> and relocate the data
.           contained within - exactly the operations it would peform on
.           standard data. *}
.#define SEC_CONSTRUCTOR 0x100
.
.        {* The section is a constructor, and should be placed at the
.          end of the text, data, or bss section(?). *}
.#define SEC_CONSTRUCTOR_TEXT 0x1100
.#define SEC_CONSTRUCTOR_DATA 0x2100
.#define SEC_CONSTRUCTOR_BSS  0x3100
.
.        {* The section has contents - a data section could be
.           <<SEC_ALLOC>> | <<SEC_HAS_CONTENTS>>; a debug section could be
.           <<SEC_HAS_CONTENTS>> *}
.#define SEC_HAS_CONTENTS 0x200
.
.        {* An instruction to the linker to not output the section
.           even if it has information which would normally be written. *}
.#define SEC_NEVER_LOAD 0x400
.
.        {* The section is a COFF shared library section.  This flag is
.           only for the linker.  If this type of section appears in
.           the input file, the linker must copy it to the output file
.           without changing the vma or size.  FIXME: Although this
.           was originally intended to be general, it really is COFF
.           specific (and the flag was renamed to indicate this).  It
.           might be cleaner to have some more general mechanism to
.           allow the back end to control what the linker does with
.           sections. *}
.#define SEC_COFF_SHARED_LIBRARY 0x800
.
.        {* The section contains common symbols (symbols may be defined
.           multiple times, the value of a symbol is the amount of
.           space it requires, and the largest symbol value is the one
.           used).  Most targets have exactly one of these (which we
.	    translate to bfd_com_section_ptr), but ECOFF has two. *}
.#define SEC_IS_COMMON 0x8000
.
.        {* The section contains only debugging information.  For
.           example, this is set for ELF .debug and .stab sections.
.           strip tests this flag to see if a section can be
.           discarded. *}
.#define SEC_DEBUGGING 0x10000
.
.        {* The contents of this section are held in memory pointed to
.           by the contents field.  This is checked by
.           bfd_get_section_contents, and the data is retrieved from
.           memory if appropriate.  *}
.#define SEC_IN_MEMORY 0x20000
.
.        {* The contents of this section are to be excluded by the
.	    linker for executable and shared objects unless those
.	    objects are to be further relocated.  *}
.#define SEC_EXCLUDE 0x40000
.
.	{* The contents of this section are to be sorted by the
.	   based on the address specified in the associated symbol
.	   table.  *}
.#define SEC_SORT_ENTRIES 0x80000
.
.	{* When linking, duplicate sections of the same name should be
.	   discarded, rather than being combined into a single section as
.	   is usually done.  This is similar to how common symbols are
.	   handled.  See SEC_LINK_DUPLICATES below.  *}
.#define SEC_LINK_ONCE 0x100000
.
.	{* If SEC_LINK_ONCE is set, this bitfield describes how the linker
.	   should handle duplicate sections.  *}
.#define SEC_LINK_DUPLICATES 0x600000
.
.	{* This value for SEC_LINK_DUPLICATES means that duplicate
.	   sections with the same name should simply be discarded. *}
.#define SEC_LINK_DUPLICATES_DISCARD 0x0
.
.	{* This value for SEC_LINK_DUPLICATES means that the linker
.	   should warn if there are any duplicate sections, although
.	   it should still only link one copy.  *}
.#define SEC_LINK_DUPLICATES_ONE_ONLY 0x200000
.
.	{* This value for SEC_LINK_DUPLICATES means that the linker
.	   should warn if any duplicate sections are a different size.  *}
.#define SEC_LINK_DUPLICATES_SAME_SIZE 0x400000
.
.	{* This value for SEC_LINK_DUPLICATES means that the linker
.	   should warn if any duplicate sections contain different
.	   contents.  *}
.#define SEC_LINK_DUPLICATES_SAME_CONTENTS 0x600000
.
.	{* This section was created by the linker as part of dynamic
.	   relocation or other arcane processing.  It is skipped when
.	   going through the first-pass output, trusting that someone
.	   else up the line will take care of it later.  *}
.#define SEC_LINKER_CREATED 0x800000
.
.	{* This section should not be subject to garbage collection.  *}
.#define SEC_KEEP 0x1000000
.
.	{* This section contains "short" data, and should be placed
.	   "near" the GP.  *}
.#define SEC_SMALL_DATA 0x2000000
.
. 	{* This section contains data which may be shared with other
.	   executables or shared objects.  *}
.#define SEC_SHARED 0x4000000
.
.	{*  End of section flags.  *}
.
.	{* Some internal packed boolean fields.  *}
.
.	{* See the vma field.  *}
.	unsigned int user_set_vma : 1;
.
.	{* Whether relocations have been processed.  *}
.	unsigned int reloc_done : 1;
.
.	{* A mark flag used by some of the linker backends.  *}
.	unsigned int linker_mark : 1;
.
.	{* A mark flag used by some linker backends for garbage collection.  *}
.	unsigned int gc_mark : 1;
.
.	{* End of internal packed boolean fields.  *}
.
.       {*  The virtual memory address of the section - where it will be
.           at run time.  The symbols are relocated against this.  The
.	    user_set_vma flag is maintained by bfd; if it's not set, the
.	    backend can assign addresses (for example, in <<a.out>>, where
.	    the default address for <<.data>> is dependent on the specific
.	    target and various flags).  *}
.
.   bfd_vma vma;
.
.       {*  The load address of the section - where it would be in a
.           rom image; really only used for writing section header
.	    information. *}
.
.   bfd_vma lma;
.
.        {* The size of the section in octets, as it will be output.
.           Contains a value even if the section has no contents (e.g., the
.           size of <<.bss>>).  This will be filled in after relocation.  *}
.
.   bfd_size_type _cooked_size;
.
.        {* The original size on disk of the section, in octets.  Normally this
.	    value is the same as the size, but if some relaxing has
.	    been done, then this value will be bigger.  *}
.
.   bfd_size_type _raw_size;
.
.        {* If this section is going to be output, then this value is the
.           offset in *bytes* into the output section of the first byte in the
.           input section (byte ==> smallest addressable unit on the
.           target).  In most cases, if this was going to start at the
.           100th octet (8-bit quantity) in the output section, this value
.           would be 100.  However, if the target byte size is 16 bits
.           (bfd_octets_per_byte is "2"), this value would be 50. *}
.
.   bfd_vma output_offset;
.
.        {* The output section through which to map on output. *}
.
.   struct sec *output_section;
.
.        {* The alignment requirement of the section, as an exponent of 2 -
.           e.g., 3 aligns to 2^3 (or 8). *}
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
.        {* If the SEC_IN_MEMORY flag is set, this points to the actual
.           contents.  *}
.   unsigned char *contents;
.
.        {* Attached line number information *}
.
.   alent *lineno;
.
.        {* Number of line number records   *}
.
.   unsigned int lineno_count;
.
.	 {* Optional information about a COMDAT entry; NULL if not COMDAT *}
.
.   struct bfd_comdat_info *comdat;
.
.        {* When a section is being output, this value changes as more
.           linenumbers are written out *}
.
.   file_ptr moving_line_filepos;
.
.        {* What the section number is in the target world  *}
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
.	 {* A symbol which points at this section only *}
.   struct symbol_cache_entry *symbol;
.   struct symbol_cache_entry **symbol_ptr_ptr;
.
.   struct bfd_link_order *link_order_head;
.   struct bfd_link_order *link_order_tail;
.} asection ;
.
.    {* These sections are global, and are managed by BFD.  The application
.       and target back end are not permitted to change the values in
.	these sections.  New code should use the section_ptr macros rather
.       than referring directly to the const sections.  The const sections
.       may eventually vanish.  *}
.#define BFD_ABS_SECTION_NAME "*ABS*"
.#define BFD_UND_SECTION_NAME "*UND*"
.#define BFD_COM_SECTION_NAME "*COM*"
.#define BFD_IND_SECTION_NAME "*IND*"
.
.    {* the absolute section *}
.extern const asection bfd_abs_section;
.#define bfd_abs_section_ptr ((asection *) &bfd_abs_section)
.#define bfd_is_abs_section(sec) ((sec) == bfd_abs_section_ptr)
.    {* Pointer to the undefined section *}
.extern const asection bfd_und_section;
.#define bfd_und_section_ptr ((asection *) &bfd_und_section)
.#define bfd_is_und_section(sec) ((sec) == bfd_und_section_ptr)
.    {* Pointer to the common section *}
.extern const asection bfd_com_section;
.#define bfd_com_section_ptr ((asection *) &bfd_com_section)
.    {* Pointer to the indirect section *}
.extern const asection bfd_ind_section;
.#define bfd_ind_section_ptr ((asection *) &bfd_ind_section)
.#define bfd_is_ind_section(sec) ((sec) == bfd_ind_section_ptr)
.
.extern const struct symbol_cache_entry * const bfd_abs_symbol;
.extern const struct symbol_cache_entry * const bfd_com_symbol;
.extern const struct symbol_cache_entry * const bfd_und_symbol;
.extern const struct symbol_cache_entry * const bfd_ind_symbol;
.#define bfd_get_section_size_before_reloc(section) \
.     ((section)->reloc_done ? (abort (), (bfd_size_type) 1) \
.                            : (section)->_raw_size)
.#define bfd_get_section_size_after_reloc(section) \
.     ((section)->reloc_done ? (section)->_cooked_size \
.                            : (abort (), (bfd_size_type) 1))
*/

/* We use a macro to initialize the static asymbol structures because
   traditional C does not permit us to initialize a union member while
   gcc warns if we don't initialize it.  */
 /* the_bfd, name, value, attr, section [, udata] */
#ifdef __STDC__
#define GLOBAL_SYM_INIT(NAME, SECTION) \
  { 0, NAME, 0, BSF_SECTION_SYM, (asection *) SECTION, { 0 }}
#else
#define GLOBAL_SYM_INIT(NAME, SECTION) \
  { 0, NAME, 0, BSF_SECTION_SYM, (asection *) SECTION }
#endif

/* These symbols are global, not specific to any BFD.  Therefore, anything
   that tries to change them is broken, and should be repaired.  */

static const asymbol global_syms[] =
{
  GLOBAL_SYM_INIT (BFD_COM_SECTION_NAME, &bfd_com_section),
  GLOBAL_SYM_INIT (BFD_UND_SECTION_NAME, &bfd_und_section),
  GLOBAL_SYM_INIT (BFD_ABS_SECTION_NAME, &bfd_abs_section),
  GLOBAL_SYM_INIT (BFD_IND_SECTION_NAME, &bfd_ind_section)
};

#define STD_SECTION(SEC, FLAGS, SYM, NAME, IDX)	\
  const asymbol * const SYM = (asymbol *) &global_syms[IDX]; \
  const asection SEC = \
    /* name, index, next, flags, set_vma, reloc_done, linker_mark, gc_mark */ \
    { NAME,  0,     0,    FLAGS, 0,       0,          0,           0,	      \
									      \
    /* vma, lma, _cooked_size, _raw_size, output_offset, output_section, */   \
       0,   0,   0,            0,         0,             (struct sec *) &SEC, \
									      \
    /* alig..., reloc..., orel..., reloc_count, filepos, rel_..., line_... */ \
       0,       0,        0,       0,           0,       0, 	   0,	      \
									      \
    /* userdata, contents, lineno, lineno_count */ 			      \
       0,        0,        0,      0,                      		      \
									      \
    /* comdat_info, moving_line_filepos, target_index, used_by_bfd,  */       \
       NULL,        0,                   0,            0, 		      \
									      \
    /* cons..., owner, symbol */ 					      \
       0,       0,     (struct symbol_cache_entry *) &global_syms[IDX],       \
									      \
    /* symbol_ptr_ptr,                      link_order_head, ..._tail */      \
       (struct symbol_cache_entry **) &SYM, 0,               0                \
    }

STD_SECTION (bfd_com_section, SEC_IS_COMMON, bfd_com_symbol,
	     BFD_COM_SECTION_NAME, 0);
STD_SECTION (bfd_und_section, 0, bfd_und_symbol, BFD_UND_SECTION_NAME, 1);
STD_SECTION (bfd_abs_section, 0, bfd_abs_symbol, BFD_ABS_SECTION_NAME, 2);
STD_SECTION (bfd_ind_section, 0, bfd_ind_symbol, BFD_IND_SECTION_NAME, 3);
#undef STD_SECTION

/*
DOCDD
INODE
section prototypes,  , typedef asection, Sections
SUBSECTION
	Section prototypes

These are the functions exported by the section handling part of BFD.
*/

/*
FUNCTION
	bfd_get_section_by_name

SYNOPSIS
	asection *bfd_get_section_by_name(bfd *abfd, CONST char *name);

DESCRIPTION
	Run through @var{abfd} and return the one of the
	<<asection>>s whose name matches @var{name}, otherwise <<NULL>>.
	@xref{Sections}, for more information.

	This should only be used in special cases; the normal way to process
	all sections of a given name is to use <<bfd_map_over_sections>> and
	<<strcmp>> on the name (or better yet, base it on the section flags
	or something else) for each section.
*/

asection *
bfd_get_section_by_name (abfd, name)
     bfd *abfd;
     CONST char *name;
{
  asection *sect;

  for (sect = abfd->sections; sect != NULL; sect = sect->next)
    if (!strcmp (sect->name, name))
      return sect;
  return NULL;
}


/*
FUNCTION
	bfd_make_section_old_way

SYNOPSIS
	asection *bfd_make_section_old_way(bfd *abfd, CONST char *name);

DESCRIPTION
	Create a new empty section called @var{name}
	and attach it to the end of the chain of sections for the
	BFD @var{abfd}. An attempt to create a section with a name which
	is already in use returns its pointer without changing the
	section chain.

	It has the funny name since this is the way it used to be
	before it was rewritten....

	Possible errors are:
	o <<bfd_error_invalid_operation>> -
	If output has already started for this BFD.
	o <<bfd_error_no_memory>> -
	If memory allocation fails.

*/


asection *
bfd_make_section_old_way (abfd, name)
     bfd *abfd;
     CONST char *name;
{
  asection *sec = bfd_get_section_by_name (abfd, name);
  if (sec == (asection *) NULL)
    {
      sec = bfd_make_section (abfd, name);
    }
  return sec;
}

/*
FUNCTION
	bfd_make_section_anyway

SYNOPSIS
	asection *bfd_make_section_anyway(bfd *abfd, CONST char *name);

DESCRIPTION
   Create a new empty section called @var{name} and attach it to the end of
   the chain of sections for @var{abfd}.  Create a new section even if there
   is already a section with that name.

   Return <<NULL>> and set <<bfd_error>> on error; possible errors are:
   o <<bfd_error_invalid_operation>> - If output has already started for @var{abfd}.
   o <<bfd_error_no_memory>> - If memory allocation fails.
*/

sec_ptr
bfd_make_section_anyway (abfd, name)
     bfd *abfd;
     CONST char *name;
{
  asection *newsect;
  asection **prev = &abfd->sections;
  asection *sect = abfd->sections;

  if (abfd->output_has_begun)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return NULL;
    }

  while (sect)
    {
      prev = &sect->next;
      sect = sect->next;
    }

  newsect = (asection *) bfd_zalloc (abfd, sizeof (asection));
  if (newsect == NULL)
    return NULL;

  newsect->name = name;
  newsect->index = abfd->section_count++;
  newsect->flags = SEC_NO_FLAGS;

  newsect->userdata = NULL;
  newsect->contents = NULL;
  newsect->next = (asection *) NULL;
  newsect->relocation = (arelent *) NULL;
  newsect->reloc_count = 0;
  newsect->line_filepos = 0;
  newsect->owner = abfd;
  newsect->comdat = NULL;

  /* Create a symbol whos only job is to point to this section. This is
     useful for things like relocs which are relative to the base of a
     section.  */
  newsect->symbol = bfd_make_empty_symbol (abfd);
  if (newsect->symbol == NULL)
    return NULL;
  newsect->symbol->name = name;
  newsect->symbol->value = 0;
  newsect->symbol->section = newsect;
  newsect->symbol->flags = BSF_SECTION_SYM;

  newsect->symbol_ptr_ptr = &newsect->symbol;

  if (BFD_SEND (abfd, _new_section_hook, (abfd, newsect)) != true)
    {
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
   Like <<bfd_make_section_anyway>>, but return <<NULL>> (without calling
   bfd_set_error ()) without changing the section chain if there is already a
   section named @var{name}.  If there is an error, return <<NULL>> and set
   <<bfd_error>>.
*/

asection *
bfd_make_section (abfd, name)
     bfd *abfd;
     CONST char *name;
{
  asection *sect = abfd->sections;

  if (strcmp (name, BFD_ABS_SECTION_NAME) == 0)
    {
      return bfd_abs_section_ptr;
    }
  if (strcmp (name, BFD_COM_SECTION_NAME) == 0)
    {
      return bfd_com_section_ptr;
    }
  if (strcmp (name, BFD_UND_SECTION_NAME) == 0)
    {
      return bfd_und_section_ptr;
    }

  if (strcmp (name, BFD_IND_SECTION_NAME) == 0)
    {
      return bfd_ind_section_ptr;
    }

  while (sect)
    {
      if (!strcmp (sect->name, name))
	return NULL;
      sect = sect->next;
    }

  /* The name is not already used; go ahead and make a new section.  */
  return bfd_make_section_anyway (abfd, name);
}


/*
FUNCTION
	bfd_set_section_flags

SYNOPSIS
	boolean bfd_set_section_flags(bfd *abfd, asection *sec, flagword flags);

DESCRIPTION
	Set the attributes of the section @var{sec} in the BFD
	@var{abfd} to the value @var{flags}. Return <<true>> on success,
	<<false>> on error. Possible error returns are:

	o <<bfd_error_invalid_operation>> -
	The section cannot have one or more of the attributes
	requested. For example, a .bss section in <<a.out>> may not
	have the <<SEC_HAS_CONTENTS>> field set.

*/

/*ARGSUSED*/
boolean
bfd_set_section_flags (abfd, section, flags)
     bfd *abfd ATTRIBUTE_UNUSED;
     sec_ptr section;
     flagword flags;
{
#if 0
  /* If you try to copy a text section from an input file (where it
     has the SEC_CODE flag set) to an output file, this loses big if
     the bfd_applicable_section_flags (abfd) doesn't have the SEC_CODE
     set - which it doesn't, at least not for a.out.  FIXME */

  if ((flags & bfd_applicable_section_flags (abfd)) != flags)
    {
      bfd_set_error (bfd_error_invalid_operation);
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
	Call the provided function @var{func} for each section
	attached to the BFD @var{abfd}, passing @var{obj} as an
	argument. The function will be called as if by

|	func(abfd, the_section, obj);

	This is the prefered method for iterating over sections; an
	alternative would be to use a loop:

|	   section *p;
|	   for (p = abfd->sections; p != NULL; p = p->next)
|	      func(abfd, p, ...)


*/

/*VARARGS2*/
void
bfd_map_over_sections (abfd, operation, user_storage)
     bfd *abfd;
     void (*operation) PARAMS ((bfd * abfd, asection * sect, PTR obj));
     PTR user_storage;
{
  asection *sect;
  unsigned int i = 0;

  for (sect = abfd->sections; sect != NULL; i++, sect = sect->next)
    (*operation) (abfd, sect, user_storage);

  if (i != abfd->section_count)	/* Debugging */
    abort ();
}


/*
FUNCTION
	bfd_set_section_size

SYNOPSIS
	boolean bfd_set_section_size(bfd *abfd, asection *sec, bfd_size_type val);

DESCRIPTION
	Set @var{sec} to the size @var{val}. If the operation is
	ok, then <<true>> is returned, else <<false>>.

	Possible error returns:
	o <<bfd_error_invalid_operation>> -
	Writing has started to the BFD, so setting the size is invalid.

*/

boolean
bfd_set_section_size (abfd, ptr, val)
     bfd *abfd;
     sec_ptr ptr;
     bfd_size_type val;
{
  /* Once you've started writing to any section you cannot create or change
     the size of any others. */

  if (abfd->output_has_begun)
    {
      bfd_set_error (bfd_error_invalid_operation);
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
	@var{offset} for @var{count} octets.



	Normally <<true>> is returned, else <<false>>. Possible error
	returns are:
	o <<bfd_error_no_contents>> -
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
bfd_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  bfd_size_type sz;

  if (!(bfd_get_section_flags (abfd, section) & SEC_HAS_CONTENTS))
    {
      bfd_set_error (bfd_error_no_contents);
      return (false);
    }

  if (offset < 0)
    {
    bad_val:
      bfd_set_error (bfd_error_bad_value);
      return false;
    }
  sz = bfd_get_section_size_now (abfd, section);
  if ((bfd_size_type) offset > sz
      || count > sz
      || offset + count > sz)
    goto bad_val;

  switch (abfd->direction)
    {
    case read_direction:
    case no_direction:
      bfd_set_error (bfd_error_invalid_operation);
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
	Read data from @var{section} in BFD @var{abfd}
	into memory starting at @var{location}. The data is read at an
	offset of @var{offset} from the start of the input section,
	and is read for @var{count} bytes.

	If the contents of a constructor with the <<SEC_CONSTRUCTOR>>
	flag set are requested or if the section does not have the
	<<SEC_HAS_CONTENTS>> flag set, then the @var{location} is filled
	with zeroes. If no errors occur, <<true>> is returned, else
	<<false>>.



*/
boolean
bfd_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  bfd_size_type sz;

  if (section->flags & SEC_CONSTRUCTOR)
    {
      memset (location, 0, (unsigned) count);
      return true;
    }

  if (offset < 0)
    {
    bad_val:
      bfd_set_error (bfd_error_bad_value);
      return false;
    }
  /* Even if reloc_done is true, this function reads unrelocated
     contents, so we want the raw size.  */
  sz = section->_raw_size;
  if ((bfd_size_type) offset > sz || count > sz || offset + count > sz)
    goto bad_val;

  if (count == 0)
    /* Don't bother.  */
    return true;

  if ((section->flags & SEC_HAS_CONTENTS) == 0)
    {
      memset (location, 0, (unsigned) count);
      return true;
    }

  if ((section->flags & SEC_IN_MEMORY) != 0)
    {
      memcpy (location, section->contents + offset, (size_t) count);
      return true;
    }

  return BFD_SEND (abfd, _bfd_get_section_contents,
		   (abfd, section, location, offset, count));
}

/*
FUNCTION
	bfd_copy_private_section_data

SYNOPSIS
	boolean bfd_copy_private_section_data(bfd *ibfd, asection *isec, bfd *obfd, asection *osec);

DESCRIPTION
	Copy private section information from @var{isec} in the BFD
	@var{ibfd} to the section @var{osec} in the BFD @var{obfd}.
	Return <<true>> on success, <<false>> on error.  Possible error
	returns are:

	o <<bfd_error_no_memory>> -
	Not enough memory exists to create private data for @var{osec}.

.#define bfd_copy_private_section_data(ibfd, isection, obfd, osection) \
.     BFD_SEND (obfd, _bfd_copy_private_section_data, \
.		(ibfd, isection, obfd, osection))
*/

/*
FUNCTION
	_bfd_strip_section_from_output

SYNOPSIS
	void _bfd_strip_section_from_output
	(struct bfd_link_info *info, asection *section);

DESCRIPTION
	Remove @var{section} from the output.  If the output section
	becomes empty, remove it from the output bfd.  @var{info} may
	be NULL; if it is not, it is used to decide whether the output
	section is empty.
*/
void
_bfd_strip_section_from_output (info, s)
     struct bfd_link_info *info;
     asection *s;
{
  asection **spp, *os;
  struct bfd_link_order *p, *pp;
  boolean keep_os;

  /* Excise the input section from the link order.

     FIXME: For all calls that I can see to this function, the link
     orders have not yet been set up.  So why are we checking them? --
     Ian */
  os = s->output_section;
  for (p = os->link_order_head, pp = NULL; p != NULL; pp = p, p = p->next)
    if (p->type == bfd_indirect_link_order
	&& p->u.indirect.section == s)
      {
	if (pp)
	  pp->next = p->next;
	else
	  os->link_order_head = p->next;
	if (!p->next)
	  os->link_order_tail = pp;
	break;
      }

  keep_os = os->link_order_head != NULL;

  if (! keep_os && info != NULL)
    {
      bfd *abfd;
      for (abfd = info->input_bfds; abfd != NULL; abfd = abfd->link_next)
	{
	  asection *is;
	  for (is = abfd->sections; is != NULL; is = is->next)
	    {
	      if (is != s && is->output_section == os)
		break;
	    }
	  if (is != NULL)
	    break;
	}
      if (abfd != NULL)
	keep_os = true;
    }

  /* If the output section is empty, remove it too.  Careful about sections
     that have been discarded in the link script -- they are mapped to 
     bfd_abs_section, which has no owner.  */
  if (!keep_os && os->owner != NULL)
    {
      for (spp = &os->owner->sections; *spp; spp = &(*spp)->next)
	if (*spp == os)
	  {
	    *spp = os->next;
	    os->owner->section_count--;
	    break;
	  }
    }
}
