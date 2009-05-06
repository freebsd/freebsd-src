/* Object file "section" support for the BFD library.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003
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
.{* This structure is used for a comdat section, as in PE.  A comdat
.   section is associated with a particular symbol.  When the linker
.   sees a comdat section, it keeps only one of the sections with a
.   given name and associated with a given symbol.  *}
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
.};
.
.typedef struct bfd_section
.{
.  {* The name of the section; the name isn't a copy, the pointer is
.     the same as that passed to bfd_make_section.  *}
.  const char *name;
.
.  {* A unique sequence number.  *}
.  int id;
.
.  {* Which section in the bfd; 0..n-1 as sections are created in a bfd.  *}
.  int index;
.
.  {* The next section in the list belonging to the BFD, or NULL.  *}
.  struct bfd_section *next;
.
.  {* The field flags contains attributes of the section. Some
.     flags are read in from the object file, and some are
.     synthesized from other information.  *}
.  flagword flags;
.
.#define SEC_NO_FLAGS   0x000
.
.  {* Tells the OS to allocate space for this section when loading.
.     This is clear for a section containing debug information only.  *}
.#define SEC_ALLOC      0x001
.
.  {* Tells the OS to load the section from the file when loading.
.     This is clear for a .bss section.  *}
.#define SEC_LOAD       0x002
.
.  {* The section contains data still to be relocated, so there is
.     some relocation information too.  *}
.#define SEC_RELOC      0x004
.
.  {* ELF reserves 4 processor specific bits and 8 operating system
.     specific bits in sh_flags; at present we can get away with just
.     one in communicating between the assembler and BFD, but this
.     isn't a good long-term solution.  *}
.#define SEC_ARCH_BIT_0 0x008
.
.  {* A signal to the OS that the section contains read only data.  *}
.#define SEC_READONLY   0x010
.
.  {* The section contains code only.  *}
.#define SEC_CODE       0x020
.
.  {* The section contains data only.  *}
.#define SEC_DATA       0x040
.
.  {* The section will reside in ROM.  *}
.#define SEC_ROM        0x080
.
.  {* The section contains constructor information. This section
.     type is used by the linker to create lists of constructors and
.     destructors used by <<g++>>. When a back end sees a symbol
.     which should be used in a constructor list, it creates a new
.     section for the type of name (e.g., <<__CTOR_LIST__>>), attaches
.     the symbol to it, and builds a relocation. To build the lists
.     of constructors, all the linker has to do is catenate all the
.     sections called <<__CTOR_LIST__>> and relocate the data
.     contained within - exactly the operations it would peform on
.     standard data.  *}
.#define SEC_CONSTRUCTOR 0x100
.
.  {* The section has contents - a data section could be
.     <<SEC_ALLOC>> | <<SEC_HAS_CONTENTS>>; a debug section could be
.     <<SEC_HAS_CONTENTS>>  *}
.#define SEC_HAS_CONTENTS 0x200
.
.  {* An instruction to the linker to not output the section
.     even if it has information which would normally be written.  *}
.#define SEC_NEVER_LOAD 0x400
.
.  {* The section is a COFF shared library section.  This flag is
.     only for the linker.  If this type of section appears in
.     the input file, the linker must copy it to the output file
.     without changing the vma or size.  FIXME: Although this
.     was originally intended to be general, it really is COFF
.     specific (and the flag was renamed to indicate this).  It
.     might be cleaner to have some more general mechanism to
.     allow the back end to control what the linker does with
.     sections.  *}
.#define SEC_COFF_SHARED_LIBRARY 0x800
.
.  {* The section contains thread local data.  *}
.#define SEC_THREAD_LOCAL 0x1000
.
.  {* The section has GOT references.  This flag is only for the
.     linker, and is currently only used by the elf32-hppa back end.
.     It will be set if global offset table references were detected
.     in this section, which indicate to the linker that the section
.     contains PIC code, and must be handled specially when doing a
.     static link.  *}
.#define SEC_HAS_GOT_REF 0x4000
.
.  {* The section contains common symbols (symbols may be defined
.     multiple times, the value of a symbol is the amount of
.     space it requires, and the largest symbol value is the one
.     used).  Most targets have exactly one of these (which we
.     translate to bfd_com_section_ptr), but ECOFF has two.  *}
.#define SEC_IS_COMMON 0x8000
.
.  {* The section contains only debugging information.  For
.     example, this is set for ELF .debug and .stab sections.
.     strip tests this flag to see if a section can be
.     discarded.  *}
.#define SEC_DEBUGGING 0x10000
.
.  {* The contents of this section are held in memory pointed to
.     by the contents field.  This is checked by bfd_get_section_contents,
.     and the data is retrieved from memory if appropriate.  *}
.#define SEC_IN_MEMORY 0x20000
.
.  {* The contents of this section are to be excluded by the
.     linker for executable and shared objects unless those
.     objects are to be further relocated.  *}
.#define SEC_EXCLUDE 0x40000
.
.  {* The contents of this section are to be sorted based on the sum of
.     the symbol and addend values specified by the associated relocation
.     entries.  Entries without associated relocation entries will be
.     appended to the end of the section in an unspecified order.  *}
.#define SEC_SORT_ENTRIES 0x80000
.
.  {* When linking, duplicate sections of the same name should be
.     discarded, rather than being combined into a single section as
.     is usually done.  This is similar to how common symbols are
.     handled.  See SEC_LINK_DUPLICATES below.  *}
.#define SEC_LINK_ONCE 0x100000
.
.  {* If SEC_LINK_ONCE is set, this bitfield describes how the linker
.     should handle duplicate sections.  *}
.#define SEC_LINK_DUPLICATES 0x600000
.
.  {* This value for SEC_LINK_DUPLICATES means that duplicate
.     sections with the same name should simply be discarded.  *}
.#define SEC_LINK_DUPLICATES_DISCARD 0x0
.
.  {* This value for SEC_LINK_DUPLICATES means that the linker
.     should warn if there are any duplicate sections, although
.     it should still only link one copy.  *}
.#define SEC_LINK_DUPLICATES_ONE_ONLY 0x200000
.
.  {* This value for SEC_LINK_DUPLICATES means that the linker
.     should warn if any duplicate sections are a different size.  *}
.#define SEC_LINK_DUPLICATES_SAME_SIZE 0x400000
.
.  {* This value for SEC_LINK_DUPLICATES means that the linker
.     should warn if any duplicate sections contain different
.     contents.  *}
.#define SEC_LINK_DUPLICATES_SAME_CONTENTS 0x600000
.
.  {* This section was created by the linker as part of dynamic
.     relocation or other arcane processing.  It is skipped when
.     going through the first-pass output, trusting that someone
.     else up the line will take care of it later.  *}
.#define SEC_LINKER_CREATED 0x800000
.
.  {* This section should not be subject to garbage collection.  *}
.#define SEC_KEEP 0x1000000
.
.  {* This section contains "short" data, and should be placed
.     "near" the GP.  *}
.#define SEC_SMALL_DATA 0x2000000
.
.  {* This section contains data which may be shared with other
.     executables or shared objects.  *}
.#define SEC_SHARED 0x4000000
.
.  {* When a section with this flag is being linked, then if the size of
.     the input section is less than a page, it should not cross a page
.     boundary.  If the size of the input section is one page or more, it
.     should be aligned on a page boundary.  *}
.#define SEC_BLOCK 0x8000000
.
.  {* Conditionally link this section; do not link if there are no
.     references found to any symbol in the section.  *}
.#define SEC_CLINK 0x10000000
.
.  {* Attempt to merge identical entities in the section.
.     Entity size is given in the entsize field.  *}
.#define SEC_MERGE 0x20000000
.
.  {* If given with SEC_MERGE, entities to merge are zero terminated
.     strings where entsize specifies character size instead of fixed
.     size entries.  *}
.#define SEC_STRINGS 0x40000000
.
.  {* This section contains data about section groups.  *}
.#define SEC_GROUP 0x80000000
.
.  {*  End of section flags.  *}
.
.  {* Some internal packed boolean fields.  *}
.
.  {* See the vma field.  *}
.  unsigned int user_set_vma : 1;
.
.  {* Whether relocations have been processed.  *}
.  unsigned int reloc_done : 1;
.
.  {* A mark flag used by some of the linker backends.  *}
.  unsigned int linker_mark : 1;
.
.  {* Another mark flag used by some of the linker backends.  Set for
.     output sections that have an input section.  *}
.  unsigned int linker_has_input : 1;
.
.  {* A mark flag used by some linker backends for garbage collection.  *}
.  unsigned int gc_mark : 1;
.
.  {* The following flags are used by the ELF linker. *}
.
.  {* Mark sections which have been allocated to segments.  *}
.  unsigned int segment_mark : 1;
.
.  {* Type of sec_info information.  *}
.  unsigned int sec_info_type:3;
.#define ELF_INFO_TYPE_NONE      0
.#define ELF_INFO_TYPE_STABS     1
.#define ELF_INFO_TYPE_MERGE     2
.#define ELF_INFO_TYPE_EH_FRAME  3
.#define ELF_INFO_TYPE_JUST_SYMS 4
.
.  {* Nonzero if this section uses RELA relocations, rather than REL.  *}
.  unsigned int use_rela_p:1;
.
.  {* Bits used by various backends.  *}
.  unsigned int has_tls_reloc:1;
.
.  {* Nonzero if this section needs the relax finalize pass.  *}
.  unsigned int need_finalize_relax:1;
.
.  {* Nonzero if this section has a gp reloc.  *}
.  unsigned int has_gp_reloc:1;
.
.  {* Unused bits.  *}
.  unsigned int flag13:1;
.  unsigned int flag14:1;
.  unsigned int flag15:1;
.  unsigned int flag16:4;
.  unsigned int flag20:4;
.  unsigned int flag24:8;
.
.  {* End of internal packed boolean fields.  *}
.
.  {*  The virtual memory address of the section - where it will be
.      at run time.  The symbols are relocated against this.  The
.      user_set_vma flag is maintained by bfd; if it's not set, the
.      backend can assign addresses (for example, in <<a.out>>, where
.      the default address for <<.data>> is dependent on the specific
.      target and various flags).  *}
.  bfd_vma vma;
.
.  {*  The load address of the section - where it would be in a
.      rom image; really only used for writing section header
.      information.  *}
.  bfd_vma lma;
.
.  {* The size of the section in octets, as it will be output.
.     Contains a value even if the section has no contents (e.g., the
.     size of <<.bss>>).  This will be filled in after relocation.  *}
.  bfd_size_type _cooked_size;
.
.  {* The original size on disk of the section, in octets.  Normally this
.     value is the same as the size, but if some relaxing has
.     been done, then this value will be bigger.  *}
.  bfd_size_type _raw_size;
.
.  {* If this section is going to be output, then this value is the
.     offset in *bytes* into the output section of the first byte in the
.     input section (byte ==> smallest addressable unit on the
.     target).  In most cases, if this was going to start at the
.     100th octet (8-bit quantity) in the output section, this value
.     would be 100.  However, if the target byte size is 16 bits
.     (bfd_octets_per_byte is "2"), this value would be 50.  *}
.  bfd_vma output_offset;
.
.  {* The output section through which to map on output.  *}
.  struct bfd_section *output_section;
.
.  {* The alignment requirement of the section, as an exponent of 2 -
.     e.g., 3 aligns to 2^3 (or 8).  *}
.  unsigned int alignment_power;
.
.  {* If an input section, a pointer to a vector of relocation
.     records for the data in this section.  *}
.  struct reloc_cache_entry *relocation;
.
.  {* If an output section, a pointer to a vector of pointers to
.     relocation records for the data in this section.  *}
.  struct reloc_cache_entry **orelocation;
.
.  {* The number of relocation records in one of the above.  *}
.  unsigned reloc_count;
.
.  {* Information below is back end specific - and not always used
.     or updated.  *}
.
.  {* File position of section data.  *}
.  file_ptr filepos;
.
.  {* File position of relocation info.  *}
.  file_ptr rel_filepos;
.
.  {* File position of line data.  *}
.  file_ptr line_filepos;
.
.  {* Pointer to data for applications.  *}
.  void *userdata;
.
.  {* If the SEC_IN_MEMORY flag is set, this points to the actual
.     contents.  *}
.  unsigned char *contents;
.
.  {* Attached line number information.  *}
.  alent *lineno;
.
.  {* Number of line number records.  *}
.  unsigned int lineno_count;
.
.  {* Entity size for merging purposes.  *}
.  unsigned int entsize;
.
.  {* Optional information about a COMDAT entry; NULL if not COMDAT.  *}
.  struct bfd_comdat_info *comdat;
.
.  {* Points to the kept section if this section is a link-once section,
.     and is discarded.  *}
.  struct bfd_section *kept_section;
.
.  {* When a section is being output, this value changes as more
.     linenumbers are written out.  *}
.  file_ptr moving_line_filepos;
.
.  {* What the section number is in the target world.  *}
.  int target_index;
.
.  void *used_by_bfd;
.
.  {* If this is a constructor section then here is a list of the
.     relocations created to relocate items within it.  *}
.  struct relent_chain *constructor_chain;
.
.  {* The BFD which owns the section.  *}
.  bfd *owner;
.
.  {* A symbol which points at this section only.  *}
.  struct bfd_symbol *symbol;
.  struct bfd_symbol **symbol_ptr_ptr;
.
.  struct bfd_link_order *link_order_head;
.  struct bfd_link_order *link_order_tail;
.} asection;
.
.{* These sections are global, and are managed by BFD.  The application
.   and target back end are not permitted to change the values in
.   these sections.  New code should use the section_ptr macros rather
.   than referring directly to the const sections.  The const sections
.   may eventually vanish.  *}
.#define BFD_ABS_SECTION_NAME "*ABS*"
.#define BFD_UND_SECTION_NAME "*UND*"
.#define BFD_COM_SECTION_NAME "*COM*"
.#define BFD_IND_SECTION_NAME "*IND*"
.
.{* The absolute section.  *}
.extern asection bfd_abs_section;
.#define bfd_abs_section_ptr ((asection *) &bfd_abs_section)
.#define bfd_is_abs_section(sec) ((sec) == bfd_abs_section_ptr)
.{* Pointer to the undefined section.  *}
.extern asection bfd_und_section;
.#define bfd_und_section_ptr ((asection *) &bfd_und_section)
.#define bfd_is_und_section(sec) ((sec) == bfd_und_section_ptr)
.{* Pointer to the common section.  *}
.extern asection bfd_com_section;
.#define bfd_com_section_ptr ((asection *) &bfd_com_section)
.{* Pointer to the indirect section.  *}
.extern asection bfd_ind_section;
.#define bfd_ind_section_ptr ((asection *) &bfd_ind_section)
.#define bfd_is_ind_section(sec) ((sec) == bfd_ind_section_ptr)
.
.#define bfd_is_const_section(SEC)		\
. (   ((SEC) == bfd_abs_section_ptr)		\
.  || ((SEC) == bfd_und_section_ptr)		\
.  || ((SEC) == bfd_com_section_ptr)		\
.  || ((SEC) == bfd_ind_section_ptr))
.
.extern const struct bfd_symbol * const bfd_abs_symbol;
.extern const struct bfd_symbol * const bfd_com_symbol;
.extern const struct bfd_symbol * const bfd_und_symbol;
.extern const struct bfd_symbol * const bfd_ind_symbol;
.#define bfd_get_section_size_before_reloc(section) \
.     ((section)->_raw_size)
.#define bfd_get_section_size_after_reloc(section) \
.     ((section)->reloc_done ? (section)->_cooked_size \
.                            : (abort (), (bfd_size_type) 1))
.
.{* Macros to handle insertion and deletion of a bfd's sections.  These
.   only handle the list pointers, ie. do not adjust section_count,
.   target_index etc.  *}
.#define bfd_section_list_remove(ABFD, PS) \
.  do							\
.    {							\
.      asection **_ps = PS;				\
.      asection *_s = *_ps;				\
.      *_ps = _s->next;					\
.      if (_s->next == NULL)				\
.        (ABFD)->section_tail = _ps;			\
.    }							\
.  while (0)
.#define bfd_section_list_insert(ABFD, PS, S) \
.  do							\
.    {							\
.      asection **_ps = PS;				\
.      asection *_s = S;				\
.      _s->next = *_ps;					\
.      *_ps = _s;					\
.      if (_s->next == NULL)				\
.        (ABFD)->section_tail = &_s->next;		\
.    }							\
.  while (0)
.
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

#define STD_SECTION(SEC, FLAGS, SYM, NAME, IDX)				\
  const asymbol * const SYM = (asymbol *) &global_syms[IDX]; 		\
  asection SEC = 							\
    /* name, id,  index, next, flags, user_set_vma, reloc_done,      */	\
    { NAME,  IDX, 0,     NULL, FLAGS, 0,            0,			\
									\
    /* linker_mark, linker_has_input, gc_mark, segment_mark,         */	\
       0,           0,                1,       0,			\
									\
    /* sec_info_type, use_rela_p, has_tls_reloc,                     */ \
       0,	      0,	  0,					\
									\
    /* need_finalize_relax, has_gp_reloc,                            */ \
       0,		    0,						\
									\
    /* flag13, flag14, flag15, flag16, flag20, flag24,               */ \
       0,      0,      0,      0,      0,      0,			\
									\
    /* vma, lma, _cooked_size, _raw_size,                            */	\
       0,   0,   0,            0,					\
									\
    /* output_offset, output_section,      alignment_power,          */	\
       0,             (struct bfd_section *) &SEC, 0,			\
									\
    /* relocation, orelocation, reloc_count, filepos, rel_filepos,   */	\
       NULL,       NULL,        0,           0,       0,		\
									\
    /* line_filepos, userdata, contents, lineno, lineno_count,       */	\
       0,            NULL,     NULL,     NULL,   0,			\
									\
    /* entsize, comdat, kept_section, moving_line_filepos,           */	\
       0,       NULL,   NULL,	      0,				\
									\
    /* target_index, used_by_bfd, constructor_chain, owner,          */	\
       0,            NULL,        NULL,              NULL,		\
									\
    /* symbol,                                                       */	\
       (struct bfd_symbol *) &global_syms[IDX],				\
									\
    /* symbol_ptr_ptr,                                               */	\
       (struct bfd_symbol **) &SYM,					\
									\
    /* link_order_head, link_order_tail                              */	\
       NULL,            NULL						\
    }

STD_SECTION (bfd_com_section, SEC_IS_COMMON, bfd_com_symbol,
	     BFD_COM_SECTION_NAME, 0);
STD_SECTION (bfd_und_section, 0, bfd_und_symbol, BFD_UND_SECTION_NAME, 1);
STD_SECTION (bfd_abs_section, 0, bfd_abs_symbol, BFD_ABS_SECTION_NAME, 2);
STD_SECTION (bfd_ind_section, 0, bfd_ind_symbol, BFD_IND_SECTION_NAME, 3);
#undef STD_SECTION

struct section_hash_entry
{
  struct bfd_hash_entry root;
  asection section;
};

/* Initialize an entry in the section hash table.  */

struct bfd_hash_entry *
bfd_section_hash_newfunc (struct bfd_hash_entry *entry,
			  struct bfd_hash_table *table,
			  const char *string)
{
  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (entry == NULL)
    {
      entry = (struct bfd_hash_entry *)
	bfd_hash_allocate (table, sizeof (struct section_hash_entry));
      if (entry == NULL)
	return entry;
    }

  /* Call the allocation method of the superclass.  */
  entry = bfd_hash_newfunc (entry, table, string);
  if (entry != NULL)
    memset (&((struct section_hash_entry *) entry)->section, 0,
	    sizeof (asection));

  return entry;
}

#define section_hash_lookup(table, string, create, copy) \
  ((struct section_hash_entry *) \
   bfd_hash_lookup ((table), (string), (create), (copy)))

/* Initializes a new section.  NEWSECT->NAME is already set.  */

static asection *
bfd_section_init (bfd *abfd, asection *newsect)
{
  static int section_id = 0x10;  /* id 0 to 3 used by STD_SECTION.  */

  newsect->id = section_id;
  newsect->index = abfd->section_count;
  newsect->owner = abfd;

  /* Create a symbol whose only job is to point to this section.  This
     is useful for things like relocs which are relative to the base
     of a section.  */
  newsect->symbol = bfd_make_empty_symbol (abfd);
  if (newsect->symbol == NULL)
    return NULL;

  newsect->symbol->name = newsect->name;
  newsect->symbol->value = 0;
  newsect->symbol->section = newsect;
  newsect->symbol->flags = BSF_SECTION_SYM;

  newsect->symbol_ptr_ptr = &newsect->symbol;

  if (! BFD_SEND (abfd, _new_section_hook, (abfd, newsect)))
    return NULL;

  section_id++;
  abfd->section_count++;
  *abfd->section_tail = newsect;
  abfd->section_tail = &newsect->next;
  return newsect;
}

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
	bfd_section_list_clear

SYNOPSIS
	void bfd_section_list_clear (bfd *);

DESCRIPTION
	Clears the section list, and also resets the section count and
	hash table entries.
*/

void
bfd_section_list_clear (bfd *abfd)
{
  abfd->sections = NULL;
  abfd->section_tail = &abfd->sections;
  abfd->section_count = 0;
  memset (abfd->section_htab.table, 0,
	  abfd->section_htab.size * sizeof (struct bfd_hash_entry *));
}

/*
FUNCTION
	bfd_get_section_by_name

SYNOPSIS
	asection *bfd_get_section_by_name (bfd *abfd, const char *name);

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
bfd_get_section_by_name (bfd *abfd, const char *name)
{
  struct section_hash_entry *sh;

  sh = section_hash_lookup (&abfd->section_htab, name, FALSE, FALSE);
  if (sh != NULL)
    return &sh->section;

  return NULL;
}

/*
FUNCTION
	bfd_get_unique_section_name

SYNOPSIS
	char *bfd_get_unique_section_name
	  (bfd *abfd, const char *templat, int *count);

DESCRIPTION
	Invent a section name that is unique in @var{abfd} by tacking
	a dot and a digit suffix onto the original @var{templat}.  If
	@var{count} is non-NULL, then it specifies the first number
	tried as a suffix to generate a unique name.  The value
	pointed to by @var{count} will be incremented in this case.
*/

char *
bfd_get_unique_section_name (bfd *abfd, const char *templat, int *count)
{
  int num;
  unsigned int len;
  char *sname;

  len = strlen (templat);
  sname = bfd_malloc (len + 8);
  if (sname == NULL)
    return NULL;
  memcpy (sname, templat, len);
  num = 1;
  if (count != NULL)
    num = *count;

  do
    {
      /* If we have a million sections, something is badly wrong.  */
      if (num > 999999)
	abort ();
      sprintf (sname + len, ".%d", num++);
    }
  while (section_hash_lookup (&abfd->section_htab, sname, FALSE, FALSE));

  if (count != NULL)
    *count = num;
  return sname;
}

/*
FUNCTION
	bfd_make_section_old_way

SYNOPSIS
	asection *bfd_make_section_old_way (bfd *abfd, const char *name);

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
bfd_make_section_old_way (bfd *abfd, const char *name)
{
  struct section_hash_entry *sh;
  asection *newsect;

  if (abfd->output_has_begun)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return NULL;
    }

  if (strcmp (name, BFD_ABS_SECTION_NAME) == 0)
    return bfd_abs_section_ptr;

  if (strcmp (name, BFD_COM_SECTION_NAME) == 0)
    return bfd_com_section_ptr;

  if (strcmp (name, BFD_UND_SECTION_NAME) == 0)
    return bfd_und_section_ptr;

  if (strcmp (name, BFD_IND_SECTION_NAME) == 0)
    return bfd_ind_section_ptr;

  sh = section_hash_lookup (&abfd->section_htab, name, TRUE, FALSE);
  if (sh == NULL)
    return NULL;

  newsect = &sh->section;
  if (newsect->name != NULL)
    {
      /* Section already exists.  */
      return newsect;
    }

  newsect->name = name;
  return bfd_section_init (abfd, newsect);
}

/*
FUNCTION
	bfd_make_section_anyway

SYNOPSIS
	asection *bfd_make_section_anyway (bfd *abfd, const char *name);

DESCRIPTION
   Create a new empty section called @var{name} and attach it to the end of
   the chain of sections for @var{abfd}.  Create a new section even if there
   is already a section with that name.

   Return <<NULL>> and set <<bfd_error>> on error; possible errors are:
   o <<bfd_error_invalid_operation>> - If output has already started for @var{abfd}.
   o <<bfd_error_no_memory>> - If memory allocation fails.
*/

sec_ptr
bfd_make_section_anyway (bfd *abfd, const char *name)
{
  struct section_hash_entry *sh;
  asection *newsect;

  if (abfd->output_has_begun)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return NULL;
    }

  sh = section_hash_lookup (&abfd->section_htab, name, TRUE, FALSE);
  if (sh == NULL)
    return NULL;

  newsect = &sh->section;
  if (newsect->name != NULL)
    {
      /* We are making a section of the same name.  It can't go in
	 section_htab without generating a unique section name and
	 that would be pointless;  We don't need to traverse the
	 hash table.  */
      newsect = bfd_zalloc (abfd, sizeof (asection));
      if (newsect == NULL)
	return NULL;
    }

  newsect->name = name;
  return bfd_section_init (abfd, newsect);
}

/*
FUNCTION
	bfd_make_section

SYNOPSIS
	asection *bfd_make_section (bfd *, const char *name);

DESCRIPTION
   Like <<bfd_make_section_anyway>>, but return <<NULL>> (without calling
   bfd_set_error ()) without changing the section chain if there is already a
   section named @var{name}.  If there is an error, return <<NULL>> and set
   <<bfd_error>>.
*/

asection *
bfd_make_section (bfd *abfd, const char *name)
{
  struct section_hash_entry *sh;
  asection *newsect;

  if (abfd->output_has_begun)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return NULL;
    }

  if (strcmp (name, BFD_ABS_SECTION_NAME) == 0
      || strcmp (name, BFD_COM_SECTION_NAME) == 0
      || strcmp (name, BFD_UND_SECTION_NAME) == 0
      || strcmp (name, BFD_IND_SECTION_NAME) == 0)
    return NULL;

  sh = section_hash_lookup (&abfd->section_htab, name, TRUE, FALSE);
  if (sh == NULL)
    return NULL;

  newsect = &sh->section;
  if (newsect->name != NULL)
    {
      /* Section already exists.  */
      return NULL;
    }

  newsect->name = name;
  return bfd_section_init (abfd, newsect);
}

/*
FUNCTION
	bfd_set_section_flags

SYNOPSIS
	bfd_boolean bfd_set_section_flags
	  (bfd *abfd, asection *sec, flagword flags);

DESCRIPTION
	Set the attributes of the section @var{sec} in the BFD
	@var{abfd} to the value @var{flags}. Return <<TRUE>> on success,
	<<FALSE>> on error. Possible error returns are:

	o <<bfd_error_invalid_operation>> -
	The section cannot have one or more of the attributes
	requested. For example, a .bss section in <<a.out>> may not
	have the <<SEC_HAS_CONTENTS>> field set.

*/

bfd_boolean
bfd_set_section_flags (bfd *abfd ATTRIBUTE_UNUSED,
		       sec_ptr section,
		       flagword flags)
{
#if 0
  /* If you try to copy a text section from an input file (where it
     has the SEC_CODE flag set) to an output file, this loses big if
     the bfd_applicable_section_flags (abfd) doesn't have the SEC_CODE
     set - which it doesn't, at least not for a.out.  FIXME */

  if ((flags & bfd_applicable_section_flags (abfd)) != flags)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return FALSE;
    }
#endif

  section->flags = flags;
  return TRUE;
}

/*
FUNCTION
	bfd_map_over_sections

SYNOPSIS
	void bfd_map_over_sections
	  (bfd *abfd,
	   void (*func) (bfd *abfd, asection *sect, void *obj),
	   void *obj);

DESCRIPTION
	Call the provided function @var{func} for each section
	attached to the BFD @var{abfd}, passing @var{obj} as an
	argument. The function will be called as if by

|	func (abfd, the_section, obj);

	This is the preferred method for iterating over sections; an
	alternative would be to use a loop:

|	   section *p;
|	   for (p = abfd->sections; p != NULL; p = p->next)
|	      func (abfd, p, ...)

*/

void
bfd_map_over_sections (bfd *abfd,
		       void (*operation) (bfd *, asection *, void *),
		       void *user_storage)
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
	bfd_boolean bfd_set_section_size
	  (bfd *abfd, asection *sec, bfd_size_type val);

DESCRIPTION
	Set @var{sec} to the size @var{val}. If the operation is
	ok, then <<TRUE>> is returned, else <<FALSE>>.

	Possible error returns:
	o <<bfd_error_invalid_operation>> -
	Writing has started to the BFD, so setting the size is invalid.

*/

bfd_boolean
bfd_set_section_size (bfd *abfd, sec_ptr ptr, bfd_size_type val)
{
  /* Once you've started writing to any section you cannot create or change
     the size of any others.  */

  if (abfd->output_has_begun)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return FALSE;
    }

  ptr->_cooked_size = val;
  ptr->_raw_size = val;

  return TRUE;
}

/*
FUNCTION
	bfd_set_section_contents

SYNOPSIS
	bfd_boolean bfd_set_section_contents
	  (bfd *abfd, asection *section, const void *data,
	   file_ptr offset, bfd_size_type count);

DESCRIPTION
	Sets the contents of the section @var{section} in BFD
	@var{abfd} to the data starting in memory at @var{data}. The
	data is written to the output section starting at offset
	@var{offset} for @var{count} octets.

	Normally <<TRUE>> is returned, else <<FALSE>>. Possible error
	returns are:
	o <<bfd_error_no_contents>> -
	The output section does not have the <<SEC_HAS_CONTENTS>>
	attribute, so nothing can be written to it.
	o and some more too

	This routine is front end to the back end function
	<<_bfd_set_section_contents>>.

*/

#define bfd_get_section_size_now(abfd, sec) \
  (sec->reloc_done \
   ? bfd_get_section_size_after_reloc (sec) \
   : bfd_get_section_size_before_reloc (sec))

bfd_boolean
bfd_set_section_contents (bfd *abfd,
			  sec_ptr section,
			  const void *location,
			  file_ptr offset,
			  bfd_size_type count)
{
  bfd_size_type sz;

  if (!(bfd_get_section_flags (abfd, section) & SEC_HAS_CONTENTS))
    {
      bfd_set_error (bfd_error_no_contents);
      return FALSE;
    }

  sz = bfd_get_section_size_now (abfd, section);
  if ((bfd_size_type) offset > sz
      || count > sz
      || offset + count > sz
      || count != (size_t) count)
    {
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  switch (abfd->direction)
    {
    case read_direction:
    case no_direction:
      bfd_set_error (bfd_error_invalid_operation);
      return FALSE;

    case write_direction:
      break;

    case both_direction:
      /* File is opened for update. `output_has_begun' some time ago when
	   the file was created.  Do not recompute sections sizes or alignments
	   in _bfd_set_section_content.  */
      abfd->output_has_begun = TRUE;
      break;
    }

  /* Record a copy of the data in memory if desired.  */
  if (section->contents
      && location != section->contents + offset)
    memcpy (section->contents + offset, location, (size_t) count);

  if (BFD_SEND (abfd, _bfd_set_section_contents,
		(abfd, section, location, offset, count)))
    {
      abfd->output_has_begun = TRUE;
      return TRUE;
    }

  return FALSE;
}

/*
FUNCTION
	bfd_get_section_contents

SYNOPSIS
	bfd_boolean bfd_get_section_contents
	  (bfd *abfd, asection *section, void *location, file_ptr offset,
	   bfd_size_type count);

DESCRIPTION
	Read data from @var{section} in BFD @var{abfd}
	into memory starting at @var{location}. The data is read at an
	offset of @var{offset} from the start of the input section,
	and is read for @var{count} bytes.

	If the contents of a constructor with the <<SEC_CONSTRUCTOR>>
	flag set are requested or if the section does not have the
	<<SEC_HAS_CONTENTS>> flag set, then the @var{location} is filled
	with zeroes. If no errors occur, <<TRUE>> is returned, else
	<<FALSE>>.

*/
bfd_boolean
bfd_get_section_contents (bfd *abfd,
			  sec_ptr section,
			  void *location,
			  file_ptr offset,
			  bfd_size_type count)
{
  bfd_size_type sz;

  if (section->flags & SEC_CONSTRUCTOR)
    {
      memset (location, 0, (size_t) count);
      return TRUE;
    }

  /* Even if reloc_done is TRUE, this function reads unrelocated
     contents, so we want the raw size.  */
  sz = section->_raw_size;
  if ((bfd_size_type) offset > sz
      || count > sz
      || offset + count > sz
      || count != (size_t) count)
    {
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  if (count == 0)
    /* Don't bother.  */
    return TRUE;

  if ((section->flags & SEC_HAS_CONTENTS) == 0)
    {
      memset (location, 0, (size_t) count);
      return TRUE;
    }

  if ((section->flags & SEC_IN_MEMORY) != 0)
    {
      memcpy (location, section->contents + offset, (size_t) count);
      return TRUE;
    }

  return BFD_SEND (abfd, _bfd_get_section_contents,
		   (abfd, section, location, offset, count));
}

/*
FUNCTION
	bfd_copy_private_section_data

SYNOPSIS
	bfd_boolean bfd_copy_private_section_data
	  (bfd *ibfd, asection *isec, bfd *obfd, asection *osec);

DESCRIPTION
	Copy private section information from @var{isec} in the BFD
	@var{ibfd} to the section @var{osec} in the BFD @var{obfd}.
	Return <<TRUE>> on success, <<FALSE>> on error.  Possible error
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
	becomes empty, remove it from the output bfd.

	This function won't actually do anything except twiddle flags
	if called too late in the linking process, when it's not safe
	to remove sections.
*/
void
_bfd_strip_section_from_output (struct bfd_link_info *info, asection *s)
{
  asection *os;
  asection *is;
  bfd *abfd;

  s->flags |= SEC_EXCLUDE;

  /* If the section wasn't assigned to an output section, or the
     section has been discarded by the linker script, there's nothing
     more to do.  */
  os = s->output_section;
  if (os == NULL || os->owner == NULL)
    return;

  /* If the output section has other (non-excluded) input sections, we
     can't remove it.  */
  for (abfd = info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (is = abfd->sections; is != NULL; is = is->next)
      if (is->output_section == os && (is->flags & SEC_EXCLUDE) == 0)
	return;

  /* If the output section is empty, flag it for removal too.
     See ldlang.c:strip_excluded_output_sections for the action.  */
  os->flags |= SEC_EXCLUDE;
}

/*
FUNCTION
	bfd_generic_discard_group

SYNOPSIS
	bfd_boolean bfd_generic_discard_group (bfd *abfd, asection *group);

DESCRIPTION
	Remove all members of @var{group} from the output.
*/

bfd_boolean
bfd_generic_discard_group (bfd *abfd ATTRIBUTE_UNUSED,
			   asection *group ATTRIBUTE_UNUSED)
{
  return TRUE;
}
