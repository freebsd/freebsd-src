/* Support for the generic parts of most COFF variants, for BFD.
   Copyright 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
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
Most of this hacked by  Steve Chamberlain,
			sac@cygnus.com
*/
/*

SECTION
	coff backends

	BFD supports a number of different flavours of coff format.
	The major differences between formats are the sizes and
	alignments of fields in structures on disk, and the occasional
	extra field.

	Coff in all its varieties is implemented with a few common
	files and a number of implementation specific files. For
	example, The 88k bcs coff format is implemented in the file
	@file{coff-m88k.c}. This file @code{#include}s
	@file{coff/m88k.h} which defines the external structure of the
	coff format for the 88k, and @file{coff/internal.h} which
	defines the internal structure. @file{coff-m88k.c} also
	defines the relocations used by the 88k format
	@xref{Relocations}.

	The Intel i960 processor version of coff is implemented in
	@file{coff-i960.c}. This file has the same structure as
	@file{coff-m88k.c}, except that it includes @file{coff/i960.h}
	rather than @file{coff-m88k.h}.

SUBSECTION
	Porting to a new version of coff

	The recommended method is to select from the existing
	implementations the version of coff which is most like the one
	you want to use.  For example, we'll say that i386 coff is
	the one you select, and that your coff flavour is called foo.
	Copy @file{i386coff.c} to @file{foocoff.c}, copy
	@file{../include/coff/i386.h} to @file{../include/coff/foo.h},
	and add the lines to @file{targets.c} and @file{Makefile.in}
	so that your new back end is used. Alter the shapes of the
	structures in @file{../include/coff/foo.h} so that they match
	what you need. You will probably also have to add
	@code{#ifdef}s to the code in @file{coff/internal.h} and
	@file{coffcode.h} if your version of coff is too wild.

	You can verify that your new BFD backend works quite simply by
	building @file{objdump} from the @file{binutils} directory,
	and making sure that its version of what's going on and your
	host system's idea (assuming it has the pretty standard coff
	dump utility, usually called @code{att-dump} or just
	@code{dump}) are the same.  Then clean up your code, and send
	what you've done to Cygnus. Then your stuff will be in the
	next release, and you won't have to keep integrating it.

SUBSECTION
	How the coff backend works

SUBSUBSECTION
	File layout

	The Coff backend is split into generic routines that are
	applicable to any Coff target and routines that are specific
	to a particular target.  The target-specific routines are
	further split into ones which are basically the same for all
	Coff targets except that they use the external symbol format
	or use different values for certain constants.

	The generic routines are in @file{coffgen.c}.  These routines
	work for any Coff target.  They use some hooks into the target
	specific code; the hooks are in a @code{bfd_coff_backend_data}
	structure, one of which exists for each target.

	The essentially similar target-specific routines are in
	@file{coffcode.h}.  This header file includes executable C code.
	The various Coff targets first include the appropriate Coff
	header file, make any special defines that are needed, and
	then include @file{coffcode.h}.

	Some of the Coff targets then also have additional routines in
	the target source file itself.

	For example, @file{coff-i960.c} includes
	@file{coff/internal.h} and @file{coff/i960.h}.  It then
	defines a few constants, such as @code{I960}, and includes
	@file{coffcode.h}.  Since the i960 has complex relocation
	types, @file{coff-i960.c} also includes some code to
	manipulate the i960 relocs.  This code is not in
	@file{coffcode.h} because it would not be used by any other
	target.

SUBSUBSECTION
	Bit twiddling

	Each flavour of coff supported in BFD has its own header file
	describing the external layout of the structures. There is also
	an internal description of the coff layout, in
	@file{coff/internal.h}. A major function of the
	coff backend is swapping the bytes and twiddling the bits to
	translate the external form of the structures into the normal
	internal form. This is all performed in the
	@code{bfd_swap}_@i{thing}_@i{direction} routines. Some
	elements are different sizes between different versions of
	coff; it is the duty of the coff version specific include file
	to override the definitions of various packing routines in
	@file{coffcode.h}. E.g., the size of line number entry in coff is
	sometimes 16 bits, and sometimes 32 bits. @code{#define}ing
	@code{PUT_LNSZ_LNNO} and @code{GET_LNSZ_LNNO} will select the
	correct one. No doubt, some day someone will find a version of
	coff which has a varying field size not catered to at the
	moment. To port BFD, that person will have to add more @code{#defines}.
	Three of the bit twiddling routines are exported to
	@code{gdb}; @code{coff_swap_aux_in}, @code{coff_swap_sym_in}
	and @code{coff_swap_linno_in}. @code{GDB} reads the symbol
	table on its own, but uses BFD to fix things up.  More of the
	bit twiddlers are exported for @code{gas};
	@code{coff_swap_aux_out}, @code{coff_swap_sym_out},
	@code{coff_swap_lineno_out}, @code{coff_swap_reloc_out},
	@code{coff_swap_filehdr_out}, @code{coff_swap_aouthdr_out},
	@code{coff_swap_scnhdr_out}. @code{Gas} currently keeps track
	of all the symbol table and reloc drudgery itself, thereby
	saving the internal BFD overhead, but uses BFD to swap things
	on the way out, making cross ports much safer.  Doing so also
	allows BFD (and thus the linker) to use the same header files
	as @code{gas}, which makes one avenue to disaster disappear.

SUBSUBSECTION
	Symbol reading

	The simple canonical form for symbols used by BFD is not rich
	enough to keep all the information available in a coff symbol
	table. The back end gets around this problem by keeping the original
	symbol table around, "behind the scenes".

	When a symbol table is requested (through a call to
	@code{bfd_canonicalize_symtab}), a request gets through to
	@code{coff_get_normalized_symtab}. This reads the symbol table from
	the coff file and swaps all the structures inside into the
	internal form. It also fixes up all the pointers in the table
	(represented in the file by offsets from the first symbol in
	the table) into physical pointers to elements in the new
	internal table. This involves some work since the meanings of
	fields change depending upon context: a field that is a
	pointer to another structure in the symbol table at one moment
	may be the size in bytes of a structure at the next.  Another
	pass is made over the table. All symbols which mark file names
	(<<C_FILE>> symbols) are modified so that the internal
	string points to the value in the auxent (the real filename)
	rather than the normal text associated with the symbol
	(@code{".file"}).

	At this time the symbol names are moved around. Coff stores
	all symbols less than nine characters long physically
	within the symbol table; longer strings are kept at the end of
	the file in the string 	table. This pass moves all strings
	into memory and replaces them with pointers to the strings.


	The symbol table is massaged once again, this time to create
	the canonical table used by the BFD application. Each symbol
	is inspected in turn, and a decision made (using the
	@code{sclass} field) about the various flags to set in the
	@code{asymbol}.  @xref{Symbols}. The generated canonical table
	shares strings with the hidden internal symbol table.

	Any linenumbers are read from the coff file too, and attached
	to the symbols which own the functions the linenumbers belong to.

SUBSUBSECTION
	Symbol writing

	Writing a symbol to a coff file which didn't come from a coff
	file will lose any debugging information. The @code{asymbol}
	structure remembers the BFD from which the symbol was taken, and on
	output the back end makes sure that the same destination target as
	source target is present.

	When the symbols have come from a coff file then all the
	debugging information is preserved.

	Symbol tables are provided for writing to the back end in a
	vector of pointers to pointers. This allows applications like
	the linker to accumulate and output large symbol tables
	without having to do too much byte copying.

	This function runs through the provided symbol table and
	patches each symbol marked as a file place holder
	(@code{C_FILE}) to point to the next file place holder in the
	list. It also marks each @code{offset} field in the list with
	the offset from the first symbol of the current symbol.

	Another function of this procedure is to turn the canonical
	value form of BFD into the form used by coff. Internally, BFD
	expects symbol values to be offsets from a section base; so a
	symbol physically at 0x120, but in a section starting at
	0x100, would have the value 0x20. Coff expects symbols to
	contain their final value, so symbols have their values
	changed at this point to reflect their sum with their owning
	section.  This transformation uses the
	<<output_section>> field of the @code{asymbol}'s
	@code{asection} @xref{Sections}.

	o <<coff_mangle_symbols>>

	This routine runs though the provided symbol table and uses
	the offsets generated by the previous pass and the pointers
	generated when the symbol table was read in to create the
	structured hierachy required by coff. It changes each pointer
	to a symbol into the index into the symbol table of the asymbol.

	o <<coff_write_symbols>>

	This routine runs through the symbol table and patches up the
	symbols from their internal form into the coff way, calls the
	bit twiddlers, and writes out the table to the file.

*/

/*
INTERNAL_DEFINITION
	coff_symbol_type

DESCRIPTION
	The hidden information for an <<asymbol>> is described in a
	<<combined_entry_type>>:

CODE_FRAGMENT
.
.typedef struct coff_ptr_struct
.{
.
.       {* Remembers the offset from the first symbol in the file for
.          this symbol. Generated by coff_renumber_symbols. *}
.unsigned int offset;
.
.       {* Should the value of this symbol be renumbered.  Used for
.          XCOFF C_BSTAT symbols.  Set by coff_slurp_symbol_table.  *}
.unsigned int fix_value : 1;
.
.       {* Should the tag field of this symbol be renumbered.
.          Created by coff_pointerize_aux. *}
.unsigned int fix_tag : 1;
.
.       {* Should the endidx field of this symbol be renumbered.
.          Created by coff_pointerize_aux. *}
.unsigned int fix_end : 1;
.
.       {* Should the x_csect.x_scnlen field be renumbered.
.          Created by coff_slurp_symbol_table. *}
.unsigned int fix_scnlen : 1;
.
.       {* The container for the symbol structure as read and translated
.           from the file. *}
.
.union {
.   union internal_auxent auxent;
.   struct internal_syment syment;
. } u;
.} combined_entry_type;
.
.
.{* Each canonical asymbol really looks like this: *}
.
.typedef struct coff_symbol_struct
.{
.   {* The actual symbol which the rest of BFD works with *}
.asymbol symbol;
.
.   {* A pointer to the hidden information for this symbol *}
.combined_entry_type *native;
.
.   {* A pointer to the linenumber information for this symbol *}
.struct lineno_cache_entry *lineno;
.
.   {* Have the line numbers been relocated yet ? *}
.boolean done_lineno;
.} coff_symbol_type;


*/

#include "coffswap.h"

/* void warning(); */

/*
 * Return a word with STYP_* (scnhdr.s_flags) flags set to represent the
 * incoming SEC_* flags.  The inverse of this function is styp_to_sec_flags().
 * NOTE: If you add to/change this routine, you should mirror the changes
 * 	in styp_to_sec_flags().
 */
static long
sec_to_styp_flags (sec_name, sec_flags)
     CONST char *sec_name;
     flagword sec_flags;
{
  long styp_flags = 0;

  if (!strcmp (sec_name, _TEXT))
    {
      styp_flags = STYP_TEXT;
    }
  else if (!strcmp (sec_name, _DATA))
    {
      styp_flags = STYP_DATA;
#ifdef TWO_DATA_SECS
    }
  else if (!strcmp (sec_name, ".data2"))
    {
      styp_flags = STYP_DATA;
#endif /* TWO_DATA_SECS */
    }
  else if (!strcmp (sec_name, _BSS))
    {
      styp_flags = STYP_BSS;
#ifdef _COMMENT
    }
  else if (!strcmp (sec_name, _COMMENT))
    {
      styp_flags = STYP_INFO;
#endif /* _COMMENT */
#ifdef _LIB
    }
  else if (!strcmp (sec_name, _LIB))
    {
      styp_flags = STYP_LIB;
#endif /* _LIB */
#ifdef _LIT
    }
  else if (!strcmp (sec_name, _LIT))
    {
      styp_flags = STYP_LIT;
#endif /* _LIT */
    }
  else if (!strcmp (sec_name, ".debug"))
    {
#ifdef STYP_DEBUG
      styp_flags = STYP_DEBUG;
#else
      styp_flags = STYP_INFO;
#endif
    }
  else if (!strcmp (sec_name, ".stab")
	   || !strncmp (sec_name, ".stabstr", 8))
    {
      styp_flags = STYP_INFO;
    }
  /* Try and figure out what it should be */
  else if (sec_flags & SEC_CODE)
    {
      styp_flags = STYP_TEXT;
    }
  else if (sec_flags & SEC_DATA)
    {
      styp_flags = STYP_DATA;
    }
  else if (sec_flags & SEC_READONLY)
    {
#ifdef STYP_LIT			/* 29k readonly text/data section */
      styp_flags = STYP_LIT;
#else
      styp_flags = STYP_TEXT;
#endif /* STYP_LIT */
    }
  else if (sec_flags & SEC_LOAD)
    {
      styp_flags = STYP_TEXT;
    }
  else if (sec_flags & SEC_ALLOC)
    {
      styp_flags = STYP_BSS;
    }

#ifdef STYP_NOLOAD
  if ((sec_flags & (SEC_NEVER_LOAD | SEC_COFF_SHARED_LIBRARY)) != 0)
    styp_flags |= STYP_NOLOAD;
#endif

  return (styp_flags);
}
/*
 * Return a word with SEC_* flags set to represent the incoming
 * STYP_* flags (from scnhdr.s_flags).   The inverse of this
 * function is sec_to_styp_flags().
 * NOTE: If you add to/change this routine, you should mirror the changes
 *      in sec_to_styp_flags().
 */
static flagword
styp_to_sec_flags (abfd, hdr)
     bfd * abfd;
     PTR hdr;
{
  struct internal_scnhdr *internal_s = (struct internal_scnhdr *) hdr;
  long styp_flags = internal_s->s_flags;
  flagword sec_flags = 0;

#ifdef STYP_NOLOAD
  if (styp_flags & STYP_NOLOAD)
    {
      sec_flags |= SEC_NEVER_LOAD;
    }
#endif /* STYP_NOLOAD */

  /* For 386 COFF, at least, an unloadable text or data section is
     actually a shared library section.  */
  if (styp_flags & STYP_TEXT)
    {
      if (sec_flags & SEC_NEVER_LOAD)
	sec_flags |= SEC_CODE | SEC_COFF_SHARED_LIBRARY;
      else
	sec_flags |= SEC_CODE | SEC_LOAD | SEC_ALLOC;
    }
  else if (styp_flags & STYP_DATA)
    {
      if (sec_flags & SEC_NEVER_LOAD)
	sec_flags |= SEC_DATA | SEC_COFF_SHARED_LIBRARY;
      else
	sec_flags |= SEC_DATA | SEC_LOAD | SEC_ALLOC;
    }
  else if (styp_flags & STYP_BSS)
    {
#ifdef BSS_NOLOAD_IS_SHARED_LIBRARY
      if (sec_flags & SEC_NEVER_LOAD)
	sec_flags |= SEC_ALLOC | SEC_COFF_SHARED_LIBRARY;
      else
#endif
	sec_flags |= SEC_ALLOC;
    }
  else if (styp_flags & STYP_INFO)
    {
      /* We mark these as SEC_DEBUGGING, but only if COFF_PAGE_SIZE is
	 defined.  coff_compute_section_file_positions uses
	 COFF_PAGE_SIZE to ensure that the low order bits of the
	 section VMA and the file offset match.  If we don't know
	 COFF_PAGE_SIZE, we can't ensure the correct correspondence,
	 and demand page loading of the file will fail.  */
#ifdef COFF_PAGE_SIZE
      sec_flags |= SEC_DEBUGGING;
#endif
    }
  else
    {
      sec_flags |= SEC_ALLOC | SEC_LOAD;
    }

#ifdef STYP_LIT			/* A29k readonly text/data section type */
  if ((styp_flags & STYP_LIT) == STYP_LIT)
    {
      sec_flags = (SEC_LOAD | SEC_ALLOC | SEC_READONLY);
    }
#endif /* STYP_LIT */
#ifdef STYP_OTHER_LOAD		/* Other loaded sections */
  if (styp_flags & STYP_OTHER_LOAD)
    {
      sec_flags = (SEC_LOAD | SEC_ALLOC);
    }
#endif /* STYP_SDATA */

  return (sec_flags);
}

#define	get_index(symbol)	((long) (symbol)->udata)

/*
INTERNAL_DEFINITION
	bfd_coff_backend_data

CODE_FRAGMENT

Special entry points for gdb to swap in coff symbol table parts:
.typedef struct
.{
.  void (*_bfd_coff_swap_aux_in) PARAMS ((
.       bfd            *abfd,
.       PTR             ext,
.       int             type,
.       int             class,
.       int             indaux,
.       int             numaux,
.       PTR             in));
.
.  void (*_bfd_coff_swap_sym_in) PARAMS ((
.       bfd            *abfd ,
.       PTR             ext,
.       PTR             in));
.
.  void (*_bfd_coff_swap_lineno_in) PARAMS ((
.       bfd            *abfd,
.       PTR            ext,
.       PTR             in));
.

Special entry points for gas to swap out coff parts:

. unsigned int (*_bfd_coff_swap_aux_out) PARAMS ((
.       bfd   	*abfd,
.       PTR	in,
.       int    	type,
.       int    	class,
.       int     indaux,
.       int     numaux,
.       PTR    	ext));
.
. unsigned int (*_bfd_coff_swap_sym_out) PARAMS ((
.      bfd      *abfd,
.      PTR	in,
.      PTR	ext));
.
. unsigned int (*_bfd_coff_swap_lineno_out) PARAMS ((
.      	bfd   	*abfd,
.      	PTR	in,
.	PTR	ext));
.
. unsigned int (*_bfd_coff_swap_reloc_out) PARAMS ((
.      	bfd     *abfd,
.     	PTR	src,
.	PTR	dst));
.
. unsigned int (*_bfd_coff_swap_filehdr_out) PARAMS ((
.      	bfd  	*abfd,
.	PTR 	in,
.	PTR 	out));
.
. unsigned int (*_bfd_coff_swap_aouthdr_out) PARAMS ((
.      	bfd 	*abfd,
.	PTR 	in,
.	PTR	out));
.
. unsigned int (*_bfd_coff_swap_scnhdr_out) PARAMS ((
.      	bfd  	*abfd,
.      	PTR	in,
.	PTR	out));
.

Special entry points for generic COFF routines to call target
dependent COFF routines:

. unsigned int _bfd_filhsz;
. unsigned int _bfd_aoutsz;
. unsigned int _bfd_scnhsz;
. unsigned int _bfd_symesz;
. unsigned int _bfd_auxesz;
. unsigned int _bfd_linesz;
. boolean _bfd_coff_long_filenames;
. void (*_bfd_coff_swap_filehdr_in) PARAMS ((
.       bfd     *abfd,
.       PTR     ext,
.       PTR     in));
. void (*_bfd_coff_swap_aouthdr_in) PARAMS ((
.       bfd     *abfd,
.       PTR     ext,
.       PTR     in));
. void (*_bfd_coff_swap_scnhdr_in) PARAMS ((
.       bfd     *abfd,
.       PTR     ext,
.       PTR     in));
. boolean (*_bfd_coff_bad_format_hook) PARAMS ((
.       bfd     *abfd,
.       PTR     internal_filehdr));
. boolean (*_bfd_coff_set_arch_mach_hook) PARAMS ((
.       bfd     *abfd,
.       PTR     internal_filehdr));
. PTR (*_bfd_coff_mkobject_hook) PARAMS ((
.       bfd     *abfd,
.       PTR     internal_filehdr,
.       PTR     internal_aouthdr));
. flagword (*_bfd_styp_to_sec_flags_hook) PARAMS ((
.       bfd     *abfd,
.       PTR     internal_scnhdr));
. asection *(*_bfd_make_section_hook) PARAMS ((
.       bfd     *abfd,
.       char    *name));
. void (*_bfd_set_alignment_hook) PARAMS ((
.       bfd     *abfd,
.       asection *sec,
.       PTR     internal_scnhdr));
. boolean (*_bfd_coff_slurp_symbol_table) PARAMS ((
.       bfd     *abfd));
. boolean (*_bfd_coff_symname_in_debug) PARAMS ((
.       bfd     *abfd,
.       struct internal_syment *sym));
. void (*_bfd_coff_reloc16_extra_cases) PARAMS ((
.       bfd     *abfd,
.       struct bfd_link_info *link_info,
.       struct bfd_link_order *link_order,
.       arelent *reloc,
.       bfd_byte *data,
.       unsigned int *src_ptr,
.       unsigned int *dst_ptr));
. int (*_bfd_coff_reloc16_estimate) PARAMS ((
.       bfd *abfd,
.       asection *input_section,
.       arelent *r,
.       unsigned int shrink,
.       struct bfd_link_info *link_info));
.
.} bfd_coff_backend_data;
.
.#define coff_backend_info(abfd) ((bfd_coff_backend_data *) (abfd)->xvec->backend_data)
.
.#define bfd_coff_swap_aux_in(a,e,t,c,ind,num,i) \
.        ((coff_backend_info (a)->_bfd_coff_swap_aux_in) (a,e,t,c,ind,num,i))
.
.#define bfd_coff_swap_sym_in(a,e,i) \
.        ((coff_backend_info (a)->_bfd_coff_swap_sym_in) (a,e,i))
.
.#define bfd_coff_swap_lineno_in(a,e,i) \
.        ((coff_backend_info ( a)->_bfd_coff_swap_lineno_in) (a,e,i))
.
.#define bfd_coff_swap_reloc_out(abfd, i, o) \
.        ((coff_backend_info (abfd)->_bfd_coff_swap_reloc_out) (abfd, i, o))
.
.#define bfd_coff_swap_lineno_out(abfd, i, o) \
.        ((coff_backend_info (abfd)->_bfd_coff_swap_lineno_out) (abfd, i, o))
.
.#define bfd_coff_swap_aux_out(a,i,t,c,ind,num,o) \
.        ((coff_backend_info (a)->_bfd_coff_swap_aux_out) (a,i,t,c,ind,num,o))
.
.#define bfd_coff_swap_sym_out(abfd, i,o) \
.        ((coff_backend_info (abfd)->_bfd_coff_swap_sym_out) (abfd, i, o))
.
.#define bfd_coff_swap_scnhdr_out(abfd, i,o) \
.        ((coff_backend_info (abfd)->_bfd_coff_swap_scnhdr_out) (abfd, i, o))
.
.#define bfd_coff_swap_filehdr_out(abfd, i,o) \
.        ((coff_backend_info (abfd)->_bfd_coff_swap_filehdr_out) (abfd, i, o))
.
.#define bfd_coff_swap_aouthdr_out(abfd, i,o) \
.        ((coff_backend_info (abfd)->_bfd_coff_swap_aouthdr_out) (abfd, i, o))
.
.#define bfd_coff_filhsz(abfd) (coff_backend_info (abfd)->_bfd_filhsz)
.#define bfd_coff_aoutsz(abfd) (coff_backend_info (abfd)->_bfd_aoutsz)
.#define bfd_coff_scnhsz(abfd) (coff_backend_info (abfd)->_bfd_scnhsz)
.#define bfd_coff_symesz(abfd) (coff_backend_info (abfd)->_bfd_symesz)
.#define bfd_coff_auxesz(abfd) (coff_backend_info (abfd)->_bfd_auxesz)
.#define bfd_coff_linesz(abfd) (coff_backend_info (abfd)->_bfd_linesz)
.#define bfd_coff_long_filenames(abfd) (coff_backend_info (abfd)->_bfd_coff_long_filenames)
.#define bfd_coff_swap_filehdr_in(abfd, i,o) \
.        ((coff_backend_info (abfd)->_bfd_coff_swap_filehdr_in) (abfd, i, o))
.
.#define bfd_coff_swap_aouthdr_in(abfd, i,o) \
.        ((coff_backend_info (abfd)->_bfd_coff_swap_aouthdr_in) (abfd, i, o))
.
.#define bfd_coff_swap_scnhdr_in(abfd, i,o) \
.        ((coff_backend_info (abfd)->_bfd_coff_swap_scnhdr_in) (abfd, i, o))
.
.#define bfd_coff_bad_format_hook(abfd, filehdr) \
.        ((coff_backend_info (abfd)->_bfd_coff_bad_format_hook) (abfd, filehdr))
.
.#define bfd_coff_set_arch_mach_hook(abfd, filehdr)\
.        ((coff_backend_info (abfd)->_bfd_coff_set_arch_mach_hook) (abfd, filehdr))
.#define bfd_coff_mkobject_hook(abfd, filehdr, aouthdr)\
.        ((coff_backend_info (abfd)->_bfd_coff_mkobject_hook) (abfd, filehdr, aouthdr))
.
.#define bfd_coff_styp_to_sec_flags_hook(abfd, scnhdr)\
.        ((coff_backend_info (abfd)->_bfd_styp_to_sec_flags_hook) (abfd, scnhdr))
.
.#define bfd_coff_make_section_hook(abfd, name)\
.        ((coff_backend_info (abfd)->_bfd_make_section_hook) (abfd, name))
.
.#define bfd_coff_set_alignment_hook(abfd, sec, scnhdr)\
.        ((coff_backend_info (abfd)->_bfd_set_alignment_hook) (abfd, sec, scnhdr))
.
.#define bfd_coff_slurp_symbol_table(abfd)\
.        ((coff_backend_info (abfd)->_bfd_coff_slurp_symbol_table) (abfd))
.
.#define bfd_coff_symname_in_debug(abfd, sym)\
.        ((coff_backend_info (abfd)->_bfd_coff_symname_in_debug) (abfd, sym))
.
.#define bfd_coff_reloc16_extra_cases(abfd, link_info, link_order, reloc, data, src_ptr, dst_ptr)\
.        ((coff_backend_info (abfd)->_bfd_coff_reloc16_extra_cases)\
.         (abfd, link_info, link_order, reloc, data, src_ptr, dst_ptr))
.
.#define bfd_coff_reloc16_estimate(abfd, section, reloc, shrink, link_info)\
.        ((coff_backend_info (abfd)->_bfd_coff_reloc16_estimate)\
.         (abfd, section, reloc, shrink, link_info))
.
*/

/* See whether the magic number matches.  */

static boolean
coff_bad_format_hook (abfd, filehdr)
     bfd * abfd;
     PTR filehdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

  if (BADMAG (*internal_f))
    return false;

  /* if the optional header is NULL or not the correct size then
     quit; the only difference I can see between m88k dgux headers (MC88DMAGIC)
     and Intel 960 readwrite headers (I960WRMAGIC) is that the
     optional header is of a different size.

     But the mips keeps extra stuff in it's opthdr, so dont check
     when doing that
     */

#if defined(M88) || defined(I960)
  if (internal_f->f_opthdr != 0 && AOUTSZ != internal_f->f_opthdr)
    return false;
#endif

  return true;
}

static asection *
coff_make_section_hook (abfd, name)
     bfd * abfd;
     char *name;
{
#ifdef TWO_DATA_SECS
  /* FIXME: This predates the call to bfd_make_section_anyway
     in make_a_section_from_file, and can probably go away.  */
  /* On SCO a file created by the Microsoft assembler can have two
     .data sections.  We use .data2 for the second one.  */
  if (strcmp (name, _DATA) == 0)
    return bfd_make_section (abfd, ".data2");
#endif
  return (asection *) NULL;
}

/*
   initialize a section structure with information peculiar to this
   particular implementation of coff
*/

static boolean
coff_new_section_hook (abfd, section)
     bfd * abfd;
     asection * section;
{
  section->alignment_power = abfd->xvec->align_power_min;
  /* Allocate aux records for section symbols, to store size and
     related info.

     @@ Shouldn't use constant multiplier here!  */
  coffsymbol (section->symbol)->native =
    (combined_entry_type *) bfd_zalloc (abfd,
					sizeof (combined_entry_type) * 10);

#ifdef COFF_SPARC
  /* This is to allow double-word operations on addresses in data or bss. */
  if (strcmp (section->name, ".data") == 0
      || strcmp (section->name, ".bss") == 0)
    section->alignment_power = 3;
#endif /* COFF_SPARC */

  return true;
}


#ifdef I960

/* Set the alignment of a BFD section.  */

static void
coff_set_alignment_hook (abfd, section, scnhdr)
     bfd * abfd;
     asection * section;
     PTR scnhdr;
{
  struct internal_scnhdr *hdr = (struct internal_scnhdr *) scnhdr;
  unsigned int i;

  for (i = 0; i < 32; i++)
    if ((1 << i) >= hdr->s_align)
      break;
  section->alignment_power = i;
}

#else /* ! I960 */

#define coff_set_alignment_hook \
  ((void (*) PARAMS ((bfd *, asection *, PTR))) bfd_void)

#endif /* ! I960 */

static boolean
coff_mkobject (abfd)
     bfd * abfd;
{
  coff_data_type *coff;

  abfd->tdata.coff_obj_data = (struct coff_tdata *) bfd_zalloc (abfd, sizeof (coff_data_type));
  if (abfd->tdata.coff_obj_data == 0)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  coff = coff_data (abfd);
  coff->symbols = (coff_symbol_type *) NULL;
  coff->conversion_table = (unsigned int *) NULL;
  coff->raw_syments = (struct coff_ptr_struct *) NULL;
  coff->raw_linenos = (struct lineno *) NULL;
  coff->relocbase = 0;
/*  make_abs_section(abfd);*/
  return true;
}

/* Create the COFF backend specific information.  */

static PTR
coff_mkobject_hook (abfd, filehdr, aouthdr)
     bfd * abfd;
     PTR filehdr;
     PTR aouthdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;
  coff_data_type *coff;

  if (coff_mkobject (abfd) == false)
    return NULL;

  coff = coff_data (abfd);

  coff->sym_filepos = internal_f->f_symptr;
  coff->flags = internal_f->f_flags;

  /* These members communicate important constants about the symbol
     table to GDB's symbol-reading code.  These `constants'
     unfortunately vary among coff implementations...  */
  coff->local_n_btmask = N_BTMASK;
  coff->local_n_btshft = N_BTSHFT;
  coff->local_n_tmask = N_TMASK;
  coff->local_n_tshift = N_TSHIFT;
  coff->local_symesz = SYMESZ;
  coff->local_auxesz = AUXESZ;
  coff->local_linesz = LINESZ;

  return (PTR) coff;
}

/* Determine the machine architecture and type.  FIXME: This is target
   dependent because the magic numbers are defined in the target
   dependent header files.  But there is no particular need for this.
   If the magic numbers were moved to a separate file, this function
   would be target independent and would also be much more successful
   at linking together COFF files for different architectures.  */

static boolean
coff_set_arch_mach_hook (abfd, filehdr)
     bfd *abfd;
     PTR filehdr;
{
  long machine;
  enum bfd_architecture arch;
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

  machine = 0;
  switch (internal_f->f_magic)
    {
#ifdef I386MAGIC
    case I386MAGIC:
    case I386PTXMAGIC:
    case I386AIXMAGIC:		/* Danbury PS/2 AIX C Compiler */
    case LYNXCOFFMAGIC:	/* shadows the m68k Lynx number below, sigh */
      arch = bfd_arch_i386;
      machine = 0;
      break;
#endif

#ifdef A29K_MAGIC_BIG
    case A29K_MAGIC_BIG:
    case A29K_MAGIC_LITTLE:
      arch = bfd_arch_a29k;
      machine = 0;
      break;
#endif

#ifdef MC68MAGIC
    case MC68MAGIC:
    case M68MAGIC:
#ifdef MC68KBCSMAGIC
    case MC68KBCSMAGIC:
#endif
#ifdef APOLLOM68KMAGIC
    case APOLLOM68KMAGIC:
#endif
#ifdef LYNXCOFFMAGIC
    case LYNXCOFFMAGIC:
#endif
      arch = bfd_arch_m68k;
      machine = 68020;
      break;
#endif
#ifdef MC88MAGIC
    case MC88MAGIC:
    case MC88DMAGIC:
    case MC88OMAGIC:
      arch = bfd_arch_m88k;
      machine = 88100;
      break;
#endif
#ifdef Z8KMAGIC
    case Z8KMAGIC:
      arch = bfd_arch_z8k;
      switch (internal_f->f_flags & F_MACHMASK)
	{
	case F_Z8001:
	  machine = bfd_mach_z8001;
	  break;
	case F_Z8002:
	  machine = bfd_mach_z8002;
	  break;
	default:
	  return false;
	}
      break;
#endif
#ifdef I960
#ifdef I960ROMAGIC
    case I960ROMAGIC:
    case I960RWMAGIC:
      arch = bfd_arch_i960;
      switch (F_I960TYPE & internal_f->f_flags)
	{
	default:
	case F_I960CORE:
	  machine = bfd_mach_i960_core;
	  break;
	case F_I960KB:
	  machine = bfd_mach_i960_kb_sb;
	  break;
	case F_I960MC:
	  machine = bfd_mach_i960_mc;
	  break;
	case F_I960XA:
	  machine = bfd_mach_i960_xa;
	  break;
	case F_I960CA:
	  machine = bfd_mach_i960_ca;
	  break;
	case F_I960KA:
	  machine = bfd_mach_i960_ka_sa;
	  break;
	}
      break;
#endif
#endif

#ifdef U802ROMAGIC
    case U802ROMAGIC:
    case U802WRMAGIC:
    case U802TOCMAGIC:
      arch = bfd_arch_rs6000;
      machine = 6000;
      break;
#endif

#ifdef WE32KMAGIC
    case WE32KMAGIC:
      arch = bfd_arch_we32k;
      machine = 0;
      break;
#endif

#ifdef H8300MAGIC
    case H8300MAGIC:
      arch = bfd_arch_h8300;
      machine = bfd_mach_h8300;
      /* !! FIXME this probably isn't the right place for this */
      abfd->flags |= BFD_IS_RELAXABLE;
      break;
#endif

#ifdef H8300HMAGIC
    case H8300HMAGIC:
      arch = bfd_arch_h8300;
      machine = bfd_mach_h8300h;
      /* !! FIXME this probably isn't the right place for this */
      abfd->flags |= BFD_IS_RELAXABLE;
      break;
#endif

#ifdef SH_ARCH_MAGIC
    case SH_ARCH_MAGIC:
      arch = bfd_arch_sh;
      machine = 0;
      break;
#endif

#ifdef H8500MAGIC
    case H8500MAGIC:
      arch = bfd_arch_h8500;
      machine = 0;
      break;
#endif

#ifdef SPARCMAGIC
    case SPARCMAGIC:
#ifdef LYNXCOFFMAGIC
    case LYNXCOFFMAGIC:
#endif
      arch = bfd_arch_sparc;
      machine = 0;
      break;
#endif

    default:			/* Unreadable input file type */
      arch = bfd_arch_obscure;
      break;
    }

  bfd_default_set_arch_mach (abfd, arch, machine);
  return true;
}

#ifdef SYMNAME_IN_DEBUG

static boolean
symname_in_debug_hook (abfd, sym)
     bfd * abfd;
     struct internal_syment *sym;
{
  return SYMNAME_IN_DEBUG (sym) ? true : false;
}

#else

#define symname_in_debug_hook \
  (boolean (*) PARAMS ((bfd *, struct internal_syment *))) bfd_false

#endif

/*
SUBSUBSECTION
	Writing relocations

	To write relocations, the back end steps though the
	canonical relocation table and create an
	@code{internal_reloc}. The symbol index to use is removed from
	the @code{offset} field in the symbol table supplied.  The
	address comes directly from the sum of the section base
	address and the relocation offset; the type is dug directly
	from the howto field.  Then the @code{internal_reloc} is
	swapped into the shape of an @code{external_reloc} and written
	out to disk.

*/

static boolean
coff_write_relocs (abfd)
     bfd * abfd;
{
  asection *s;
  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      unsigned int i;
      struct external_reloc dst;

      arelent **p = s->orelocation;
      if (bfd_seek (abfd, s->rel_filepos, SEEK_SET) != 0)
	return false;
      for (i = 0; i < s->reloc_count; i++)
	{
	  struct internal_reloc n;
	  arelent *q = p[i];
	  memset ((PTR) & n, 0, sizeof (n));

	  n.r_vaddr = q->address + s->vma;

#ifdef R_IHCONST
	  /* The 29k const/consth reloc pair is a real kludge.  The consth
	 part doesn't have a symbol; it has an offset.  So rebuilt
	 that here.  */
	  if (q->howto->type == R_IHCONST)
	    n.r_symndx = q->addend;
	  else
#endif
	  if (q->sym_ptr_ptr)
	    {
	      if (q->sym_ptr_ptr == bfd_abs_section_ptr->symbol_ptr_ptr)
		/* This is a relocation relative to the absolute symbol.  */
		n.r_symndx = -1;
	      else
		{
		  n.r_symndx = get_index ((*(q->sym_ptr_ptr)));
		  /* Take notice if the symbol reloc points to a symbol
		   we don't have in our symbol table.  What should we
		   do for this??  */
		  if (n.r_symndx > obj_conv_table_size (abfd))
		    abort ();
		}
	    }

#ifdef SWAP_OUT_RELOC_OFFSET
	  n.r_offset = q->addend;
#endif

#ifdef SELECT_RELOC
	  /* Work out reloc type from what is required */
	  SELECT_RELOC (n, q->howto);
#else
	  n.r_type = q->howto->type;
#endif
	  coff_swap_reloc_out (abfd, &n, &dst);
	  if (bfd_write ((PTR) & dst, 1, RELSZ, abfd) != RELSZ)
	    return false;
	}
    }

  return true;
}

/* Set flags and magic number of a coff file from architecture and machine
   type.  Result is true if we can represent the arch&type, false if not.  */

static boolean
coff_set_flags (abfd, magicp, flagsp)
     bfd * abfd;
     unsigned *magicp;
     unsigned short *flagsp;
{
  switch (bfd_get_arch (abfd))
    {
#ifdef Z8KMAGIC
    case bfd_arch_z8k:
      *magicp = Z8KMAGIC;
      switch (bfd_get_mach (abfd))
	{
	case bfd_mach_z8001:
	  *flagsp = F_Z8001;
	  break;
	case bfd_mach_z8002:
	  *flagsp = F_Z8002;
	  break;
	default:
	  return false;
	}
      return true;
#endif
#ifdef I960ROMAGIC

    case bfd_arch_i960:

      {
	unsigned flags;
	*magicp = I960ROMAGIC;
	/*
	  ((bfd_get_file_flags(abfd) & WP_TEXT) ? I960ROMAGIC :
	  I960RWMAGIC);   FIXME???
	  */
	switch (bfd_get_mach (abfd))
	  {
	  case bfd_mach_i960_core:
	    flags = F_I960CORE;
	    break;
	  case bfd_mach_i960_kb_sb:
	    flags = F_I960KB;
	    break;
	  case bfd_mach_i960_mc:
	    flags = F_I960MC;
	    break;
	  case bfd_mach_i960_xa:
	    flags = F_I960XA;
	    break;
	  case bfd_mach_i960_ca:
	    flags = F_I960CA;
	    break;
	  case bfd_mach_i960_ka_sa:
	    flags = F_I960KA;
	    break;
	  default:
	    return false;
	  }
	*flagsp = flags;
	return true;
      }
      break;
#endif
#ifdef I386MAGIC
    case bfd_arch_i386:
      *magicp = I386MAGIC;
#ifdef LYNXOS
      /* Just overwrite the usual value if we're doing Lynx. */
      *magicp = LYNXCOFFMAGIC;
#endif
      return true;
      break;
#endif
#ifdef MC68MAGIC
    case bfd_arch_m68k:
#ifdef APOLLOM68KMAGIC
      *magicp = APOLLO_COFF_VERSION_NUMBER;
#else
      *magicp = MC68MAGIC;
#endif
#ifdef LYNXOS
      /* Just overwrite the usual value if we're doing Lynx. */
      *magicp = LYNXCOFFMAGIC;
#endif
      return true;
      break;
#endif

#ifdef MC88MAGIC
    case bfd_arch_m88k:
      *magicp = MC88OMAGIC;
      return true;
      break;
#endif
#ifdef H8300MAGIC
    case bfd_arch_h8300:
      switch (bfd_get_mach (abfd))
	{
	case bfd_mach_h8300:
	  *magicp = H8300MAGIC;
	  return true;
	case bfd_mach_h8300h:
	  *magicp = H8300HMAGIC;
	  return true;
	}
      break;
#endif

#ifdef SH_ARCH_MAGIC
    case bfd_arch_sh:
      *magicp = SH_ARCH_MAGIC;
      return true;
      break;
#endif

#ifdef SPARCMAGIC
    case bfd_arch_sparc:
      *magicp = SPARCMAGIC;
#ifdef LYNXOS
      /* Just overwrite the usual value if we're doing Lynx. */
      *magicp = LYNXCOFFMAGIC;
#endif
      return true;
      break;
#endif

#ifdef H8500MAGIC
    case bfd_arch_h8500:
      *magicp = H8500MAGIC;
      return true;
      break;
#endif
#ifdef A29K_MAGIC_BIG
    case bfd_arch_a29k:
      if (abfd->xvec->byteorder_big_p)
	*magicp = A29K_MAGIC_BIG;
      else
	*magicp = A29K_MAGIC_LITTLE;
      return true;
      break;
#endif

#ifdef WE32KMAGIC
    case bfd_arch_we32k:
      *magicp = WE32KMAGIC;
      return true;
      break;
#endif

#ifdef U802TOCMAGIC
    case bfd_arch_rs6000:
    case bfd_arch_powerpc:
      *magicp = U802TOCMAGIC;
      return true;
      break;
#endif

    default:			/* Unknown architecture */
      /* return false;  -- fall through to "return false" below, to avoid
       "statement never reached" errors on the one below. */
      break;
    }

  return false;
}


static boolean
coff_set_arch_mach (abfd, arch, machine)
     bfd * abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  unsigned dummy1;
  unsigned short dummy2;

  if (! bfd_default_set_arch_mach (abfd, arch, machine))
    return false;

  if (arch != bfd_arch_unknown &&
      coff_set_flags (abfd, &dummy1, &dummy2) != true)
    return false;		/* We can't represent this type */

  return true;			/* We're easy ... */
}


/* Calculate the file position for each section. */

static void
coff_compute_section_file_positions (abfd)
     bfd * abfd;
{
  asection *current;
  asection *previous = (asection *) NULL;
  file_ptr sofar = FILHSZ;
#ifndef I960
  file_ptr old_sofar;
#endif
  if (bfd_get_start_address (abfd))
    {
      /*  A start address may have been added to the original file. In this
	case it will need an optional header to record it.  */
      abfd->flags |= EXEC_P;
    }

  if (abfd->flags & EXEC_P)
    sofar += AOUTSZ;

  sofar += abfd->section_count * SCNHSZ;
  for (current = abfd->sections;
       current != (asection *) NULL;
       current = current->next)
    {

      /* Only deal with sections which have contents */
      if (!(current->flags & SEC_HAS_CONTENTS))
	continue;

      /* Align the sections in the file to the same boundary on
	 which they are aligned in virtual memory.  I960 doesn't
	 do this (FIXME) so we can stay in sync with Intel.  960
	 doesn't yet page from files... */
#ifndef I960
      {
	/* make sure this section is aligned on the right boundary - by
	 padding the previous section up if necessary */

	old_sofar = sofar;
	sofar = BFD_ALIGN (sofar, 1 << current->alignment_power);
	if (previous != (asection *) NULL)
	  {
	    previous->_raw_size += sofar - old_sofar;
	  }
      }

#endif

#ifdef COFF_PAGE_SIZE
      /* In demand paged files the low order bits of the file offset
	 must match the low order bits of the virtual address.  */
      if ((abfd->flags & D_PAGED) != 0)
	sofar += (current->vma - sofar) % COFF_PAGE_SIZE;
#endif

      current->filepos = sofar;

      sofar += current->_raw_size;
#ifndef I960
      /* make sure that this section is of the right size too */
      old_sofar = sofar;
      sofar = BFD_ALIGN (sofar, 1 << current->alignment_power);
      current->_raw_size += sofar - old_sofar;
#endif

#ifdef _LIB
      /* Force .lib sections to start at zero.  The vma is then
	 incremented in coff_set_section_contents.  This is right for
	 SVR3.2.  */
      if (strcmp (current->name, _LIB) == 0)
	bfd_set_section_vma (abfd, current, 0);
#endif

      previous = current;
    }
  obj_relocbase (abfd) = sofar;
}

#ifndef RS6000COFF_C

/* If .file, .text, .data, .bss symbols are missing, add them.  */
/* @@ Should we only be adding missing symbols, or overriding the aux
   values for existing section symbols?  */
static boolean
coff_add_missing_symbols (abfd)
     bfd *abfd;
{
  unsigned int nsyms = bfd_get_symcount (abfd);
  asymbol **sympp = abfd->outsymbols;
  asymbol **sympp2;
  unsigned int i;
  int need_text = 1, need_data = 1, need_bss = 1, need_file = 1;

  for (i = 0; i < nsyms; i++)
    {
      coff_symbol_type *csym = coff_symbol_from (abfd, sympp[i]);
      CONST char *name;
      if (csym)
	{
	  /* only do this if there is a coff representation of the input
	   symbol */
	  if (csym->native && csym->native->u.syment.n_sclass == C_FILE)
	    {
	      need_file = 0;
	      continue;
	    }
	  name = csym->symbol.name;
	  if (!name)
	    continue;
	  if (!strcmp (name, _TEXT))
	    need_text = 0;
#ifdef APOLLO_M68
	  else if (!strcmp (name, ".wtext"))
	    need_text = 0;
#endif
	  else if (!strcmp (name, _DATA))
	    need_data = 0;
	  else if (!strcmp (name, _BSS))
	    need_bss = 0;
	}
    }
  /* Now i == bfd_get_symcount (abfd).  */
  /* @@ For now, don't deal with .file symbol.  */
  need_file = 0;

  if (!need_text && !need_data && !need_bss && !need_file)
    return true;
  nsyms += need_text + need_data + need_bss + need_file;
  sympp2 = (asymbol **) bfd_alloc_by_size_t (abfd, nsyms * sizeof (asymbol *));
  if (!sympp2)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  memcpy (sympp2, sympp, i * sizeof (asymbol *));
  if (need_file)
    {
      /* @@ Generate fake .file symbol, in sympp2[i], and increment i.  */
      abort ();
    }
  if (need_text)
    sympp2[i++] = coff_section_symbol (abfd, _TEXT);
  if (need_data)
    sympp2[i++] = coff_section_symbol (abfd, _DATA);
  if (need_bss)
    sympp2[i++] = coff_section_symbol (abfd, _BSS);
  BFD_ASSERT (i == nsyms);
  bfd_set_symtab (abfd, sympp2, nsyms);
  return true;
}

#endif /* ! defined (RS6000COFF_C) */

/* SUPPRESS 558 */
/* SUPPRESS 529 */
static boolean
coff_write_object_contents (abfd)
     bfd * abfd;
{
  asection *current;
  unsigned int count;

  boolean hasrelocs = false;
  boolean haslinno = false;
  file_ptr reloc_base;
  file_ptr lineno_base;
  file_ptr sym_base;
  file_ptr scn_base;
  file_ptr data_base;
  unsigned long reloc_size = 0;
  unsigned long lnno_size = 0;
  asection *text_sec = NULL;
  asection *data_sec = NULL;
  asection *bss_sec = NULL;

  struct internal_filehdr internal_f;
  struct internal_aouthdr internal_a;


  bfd_set_error (bfd_error_system_call);
  /* Number the output sections, starting from one on the first section
     with a name which doesn't start with a *.
     @@ The code doesn't make this check.  Is it supposed to be done,
     or isn't it??  */
  count = 1;
  for (current = abfd->sections; current != (asection *) NULL;
       current = current->next)
    {
      current->target_index = count;
      count++;
    }

  if (abfd->output_has_begun == false)
    {
      coff_compute_section_file_positions (abfd);
    }

  if (abfd->sections != (asection *) NULL)
    {
      scn_base = abfd->sections->filepos;
    }
  else
    {
      scn_base = 0;
    }
  if (bfd_seek (abfd, scn_base, SEEK_SET) != 0)
    return false;
  reloc_base = obj_relocbase (abfd);

  /* Make a pass through the symbol table to count line number entries and
     put them into the correct asections */

  lnno_size = coff_count_linenumbers (abfd) * LINESZ;
  data_base = scn_base;

  /* Work out the size of the reloc and linno areas */

  for (current = abfd->sections; current != NULL; current =
       current->next)
    {
      /* We give section headers to +ve indexes */
      if (current->target_index > 0)
	{

	  reloc_size += current->reloc_count * RELSZ;
	  data_base += SCNHSZ;
	}

    }

  lineno_base = reloc_base + reloc_size;
  sym_base = lineno_base + lnno_size;

  /* Indicate in each section->line_filepos its actual file address */
  for (current = abfd->sections; current != NULL; current =
       current->next)
    {
      if (current->target_index > 0)
	{

	  if (current->lineno_count)
	    {
	      current->line_filepos = lineno_base;
	      current->moving_line_filepos = lineno_base;
	      lineno_base += current->lineno_count * LINESZ;
	    }
	  else
	    {
	      current->line_filepos = 0;
	    }
	  if (current->reloc_count)
	    {
	      current->rel_filepos = reloc_base;
	      reloc_base += current->reloc_count * RELSZ;
	    }
	  else
	    {
	      current->rel_filepos = 0;
	    }
	}
    }



  /* Write section headers to the file.  */
  internal_f.f_nscns = 0;
  if (bfd_seek (abfd,
		(file_ptr) ((abfd->flags & EXEC_P) ?
			    (FILHSZ + AOUTSZ) : FILHSZ),
		SEEK_SET)
      != 0)
    return false;

  {
#if 0
    unsigned int pad = abfd->flags & D_PAGED ? data_base : 0;
#endif
    unsigned int pad = 0;

    for (current = abfd->sections;
	 current != NULL;
	 current = current->next)
      {
	struct internal_scnhdr section;
	if (current->target_index > 0)
	  {
	    internal_f.f_nscns++;
	    strncpy (&(section.s_name[0]), current->name, 8);
#ifdef _LIB
	    /* Always set s_vaddr of .lib to 0.  This is right for SVR3.2
	   Ian Taylor <ian@cygnus.com>.  */
	    if (strcmp (current->name, _LIB) == 0)
	      section.s_vaddr = 0;
	    else
#endif
	      section.s_vaddr = current->lma + pad;
	    section.s_paddr = current->lma + pad;
	    section.s_size = current->_raw_size - pad;
	    /*
	  If this section has no size or is unloadable then the scnptr
	  will be 0 too
	  */
	    if (current->_raw_size - pad == 0 ||
		(current->flags & (SEC_LOAD | SEC_HAS_CONTENTS)) == 0)
	      {
		section.s_scnptr = 0;
	      }
	    else
	      {
		section.s_scnptr = current->filepos;
	      }
	    section.s_relptr = current->rel_filepos;
	    section.s_lnnoptr = current->line_filepos;
	    section.s_nreloc = current->reloc_count;
	    section.s_nlnno = current->lineno_count;
	    if (current->reloc_count != 0)
	      hasrelocs = true;
	    if (current->lineno_count != 0)
	      haslinno = true;

	    section.s_flags = sec_to_styp_flags (current->name, current->flags);

	    if (!strcmp (current->name, _TEXT))
	      {
		text_sec = current;
	      }
	    else if (!strcmp (current->name, _DATA))
	      {
		data_sec = current;
#ifdef TWO_DATA_SECS
	      }
	    else if (!strcmp (current->name, ".data2"))
	      {
		data_sec = current;
#endif /* TWO_DATA_SECS */
	      }
	    else if (!strcmp (current->name, _BSS))
	      {
		bss_sec = current;
	      }

#ifdef I960
	    section.s_align = (current->alignment_power
			       ? 1 << current->alignment_power
			       : 0);

#endif
	    {
	      SCNHDR buff;

	      coff_swap_scnhdr_out (abfd, &section, &buff);
	      if (bfd_write ((PTR) (&buff), 1, SCNHSZ, abfd) != SCNHSZ)
		return false;

	    }

	    pad = 0;
	  }
      }
  }


  /* OK, now set up the filehdr... */

  /* Don't include the internal abs section in the section count */

  /*
    We will NOT put a fucking timestamp in the header here. Every time you
    put it back, I will come in and take it out again.  I'm sorry.  This
    field does not belong here.  We fill it with a 0 so it compares the
    same but is not a reasonable time. -- gnu@cygnus.com
    */
  internal_f.f_timdat = 0;

  if (bfd_get_symcount (abfd) != 0)
    internal_f.f_symptr = sym_base;
  else
    internal_f.f_symptr = 0;

  internal_f.f_flags = 0;

  if (abfd->flags & EXEC_P)
    internal_f.f_opthdr = AOUTSZ;
  else
    internal_f.f_opthdr = 0;

  if (!hasrelocs)
    internal_f.f_flags |= F_RELFLG;
  if (!haslinno)
    internal_f.f_flags |= F_LNNO;
  if (0 == bfd_get_symcount (abfd))
    internal_f.f_flags |= F_LSYMS;
  if (abfd->flags & EXEC_P)
    internal_f.f_flags |= F_EXEC;

  if (!abfd->xvec->byteorder_big_p)
    internal_f.f_flags |= F_AR32WR;
  else
    internal_f.f_flags |= F_AR32W;

  /*
    FIXME, should do something about the other byte orders and
    architectures.
    */

  memset (&internal_a, 0, sizeof internal_a);

  /* Set up architecture-dependent stuff */

  {
    unsigned int magic = 0;
    unsigned short flags = 0;
    coff_set_flags (abfd, &magic, &flags);
    internal_f.f_magic = magic;
    internal_f.f_flags |= flags;
    /* ...and the "opt"hdr... */

#ifdef A29K
#ifdef ULTRA3			/* NYU's machine */
    /* FIXME: This is a bogus check.  I really want to see if there
   * is a .shbss or a .shdata section, if so then set the magic
   * number to indicate a shared data executable.
   */
    if (internal_f.f_nscns >= 7)
      internal_a.magic = SHMAGIC;	/* Shared magic */
    else
#endif /* ULTRA3 */
      internal_a.magic = NMAGIC;/* Assume separate i/d */
#define __A_MAGIC_SET__
#endif /* A29K */
#ifdef I960
    internal_a.magic = (magic == I960ROMAGIC ? NMAGIC : OMAGIC);
#define __A_MAGIC_SET__
#endif /* I960 */
#if M88
#define __A_MAGIC_SET__
    internal_a.magic = PAGEMAGICBCS;
#endif /* M88 */

#if APOLLO_M68
#define __A_MAGIC_SET__
    internal_a.magic = APOLLO_COFF_VERSION_NUMBER;
#endif

#if M68 || WE32K
#define __A_MAGIC_SET__
    /* Never was anything here for the 68k */
#endif /* M68 || WE32K */

#if I386
#define __A_MAGIC_SET__
    internal_a.magic = ZMAGIC;
#endif /* I386 */

#if RS6000COFF_C
#define __A_MAGIC_SET__
    internal_a.magic = (abfd->flags & D_PAGED) ? RS6K_AOUTHDR_ZMAGIC :
      (abfd->flags & WP_TEXT) ? RS6K_AOUTHDR_NMAGIC :
      RS6K_AOUTHDR_OMAGIC;
#endif

#ifndef __A_MAGIC_SET__
#include "Your aouthdr magic number is not being set!"
#else
#undef __A_MAGIC_SET__
#endif
  }
  /* Now should write relocs, strings, syms */
  obj_sym_filepos (abfd) = sym_base;

  if (bfd_get_symcount (abfd) != 0)
    {
#ifndef RS6000COFF_C
      if (!coff_add_missing_symbols (abfd))
	return false;
#endif
      if (!coff_renumber_symbols (abfd))
	return false;
      coff_mangle_symbols (abfd);
      if (! coff_write_symbols (abfd))
	return false;
      if (! coff_write_linenumbers (abfd))
	return false;
      if (! coff_write_relocs (abfd))
	return false;
    }
  if (text_sec)
    {
      internal_a.tsize = bfd_get_section_size_before_reloc (text_sec);
      internal_a.text_start = internal_a.tsize ? text_sec->vma : 0;
    }
  if (data_sec)
    {
      internal_a.dsize = bfd_get_section_size_before_reloc (data_sec);
      internal_a.data_start = internal_a.dsize ? data_sec->vma : 0;
    }
  if (bss_sec)
    {
      internal_a.bsize = bfd_get_section_size_before_reloc (bss_sec);
    }

  internal_a.entry = bfd_get_start_address (abfd);
  internal_f.f_nsyms = bfd_get_symcount (abfd);

  /* now write them */
  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    return false;
  {
    FILHDR buff;
    coff_swap_filehdr_out (abfd, (PTR) & internal_f, (PTR) & buff);
    if (bfd_write ((PTR) & buff, 1, FILHSZ, abfd) != FILHSZ)
      return false;
  }
  if (abfd->flags & EXEC_P)
    {
      AOUTHDR buff;
      coff_swap_aouthdr_out (abfd, (PTR) & internal_a, (PTR) & buff);
      if (bfd_write ((PTR) & buff, 1, AOUTSZ, abfd) != AOUTSZ)
	return false;
    }
  return true;
}

static boolean
coff_set_section_contents (abfd, section, location, offset, count)
     bfd * abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (abfd->output_has_begun == false)	/* set by bfd.c handler */
    coff_compute_section_file_positions (abfd);

#ifdef _LIB
  /* If this is a .lib section, bump the vma address so that it
       winds up being the number of .lib sections output.  This is
       right for SVR3.2.  Shared libraries should probably get more
       generic support.  Ian Taylor <ian@cygnus.com>.  */
  if (strcmp (section->name, _LIB) == 0)
    ++section->lma;
#endif

  /* Don't write out bss sections - one way to do this is to
       see if the filepos has not been set. */
  if (section->filepos == 0)
    return true;

  if (bfd_seek (abfd, (file_ptr) (section->filepos + offset), SEEK_SET) != 0)
    return false;

  if (count != 0)
    {
      return (bfd_write (location, 1, count, abfd) == count) ? true : false;
    }
  return true;
}
#if 0
static boolean
coff_close_and_cleanup (abfd)
     bfd *abfd;
{
  if (!bfd_read_p (abfd))
    switch (abfd->format)
      {
      case bfd_archive:
	if (!_bfd_write_archive_contents (abfd))
	  return false;
	break;
      case bfd_object:
	if (!coff_write_object_contents (abfd))
	  return false;
	break;
      default:
	bfd_set_error (bfd_error_invalid_operation);
	return false;
      }

  /* We depend on bfd_close to free all the memory on the obstack.  */
  /* FIXME if bfd_release is not using obstacks! */
  return true;
}

#endif

static PTR
buy_and_read (abfd, where, seek_direction, size)
     bfd *abfd;
     file_ptr where;
     int seek_direction;
     size_t size;
{
  PTR area = (PTR) bfd_alloc (abfd, size);
  if (!area)
    {
      bfd_set_error (bfd_error_no_memory);
      return (NULL);
    }
  if (bfd_seek (abfd, where, seek_direction) != 0
      || bfd_read (area, 1, size, abfd) != size)
    return (NULL);
  return (area);
}				/* buy_and_read() */

/*
SUBSUBSECTION
	Reading linenumbers

	Creating the linenumber table is done by reading in the entire
	coff linenumber table, and creating another table for internal use.

	A coff linenumber table is structured so that each function
	is marked as having a line number of 0. Each line within the
	function is an offset from the first line in the function. The
	base of the line number information for the table is stored in
	the symbol associated with the function.

	The information is copied from the external to the internal
	table, and each symbol which marks a function is marked by
	pointing its...

	How does this work ?

*/

static boolean
coff_slurp_line_table (abfd, asect)
     bfd *abfd;
     asection *asect;
{
  LINENO *native_lineno;
  alent *lineno_cache;

  BFD_ASSERT (asect->lineno == (alent *) NULL);

  native_lineno = (LINENO *) buy_and_read (abfd,
					   asect->line_filepos,
					   SEEK_SET,
					   (size_t) (LINESZ *
						     asect->lineno_count));
  lineno_cache =
    (alent *) bfd_alloc (abfd, (size_t) ((asect->lineno_count + 1) * sizeof (alent)));
  if (lineno_cache == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  else
    {
      unsigned int counter = 0;
      alent *cache_ptr = lineno_cache;
      LINENO *src = native_lineno;

      while (counter < asect->lineno_count)
	{
	  struct internal_lineno dst;
	  coff_swap_lineno_in (abfd, src, &dst);
	  cache_ptr->line_number = dst.l_lnno;

	  if (cache_ptr->line_number == 0)
	    {
	      coff_symbol_type *sym =
	      (coff_symbol_type *) (dst.l_addr.l_symndx
		      + obj_raw_syments (abfd))->u.syment._n._n_n._n_zeroes;
	      cache_ptr->u.sym = (asymbol *) sym;
	      sym->lineno = cache_ptr;
	    }
	  else
	    {
	      cache_ptr->u.offset = dst.l_addr.l_paddr
		- bfd_section_vma (abfd, asect);
	    }			/* If no linenumber expect a symbol index */

	  cache_ptr++;
	  src++;
	  counter++;
	}
      cache_ptr->line_number = 0;

    }
  asect->lineno = lineno_cache;
  /* FIXME, free native_lineno here, or use alloca or something. */
  return true;
}

static boolean
coff_slurp_symbol_table (abfd)
     bfd * abfd;
{
  combined_entry_type *native_symbols;
  coff_symbol_type *cached_area;
  unsigned int *table_ptr;

  unsigned int number_of_symbols = 0;
  if (obj_symbols (abfd))
    return true;
  if (bfd_seek (abfd, obj_sym_filepos (abfd), SEEK_SET) != 0)
    return false;

  /* Read in the symbol table */
  if ((native_symbols = coff_get_normalized_symtab (abfd)) == NULL)
    {
      return (false);
    }				/* on error */

  /* Allocate enough room for all the symbols in cached form */
  cached_area =
    (coff_symbol_type *)
    bfd_alloc (abfd, (size_t) (bfd_get_symcount (abfd) * sizeof (coff_symbol_type)));

  if (cached_area == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }				/* on error */
  table_ptr =
    (unsigned int *)
    bfd_alloc (abfd, (size_t) (bfd_get_symcount (abfd) * sizeof (unsigned int)));

  if (table_ptr == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  else
    {
      coff_symbol_type *dst = cached_area;
      unsigned int last_native_index = bfd_get_symcount (abfd);
      unsigned int this_index = 0;
      while (this_index < last_native_index)
	{
	  combined_entry_type *src = native_symbols + this_index;
	  table_ptr[this_index] = number_of_symbols;
	  dst->symbol.the_bfd = abfd;

	  dst->symbol.name = (char *) (src->u.syment._n._n_n._n_offset);
	  /* We use the native name field to point to the cached field.  */
	  src->u.syment._n._n_n._n_zeroes = (long) dst;
	  dst->symbol.section = coff_section_from_bfd_index (abfd,
						     src->u.syment.n_scnum);
	  dst->symbol.flags = 0;
	  dst->done_lineno = false;

	  switch (src->u.syment.n_sclass)
	    {
#ifdef I960
	    case C_LEAFEXT:
#if 0
	      dst->symbol.value = src->u.syment.n_value - dst->symbol.section->vma;
	      dst->symbol.flags = BSF_EXPORT | BSF_GLOBAL;
	      dst->symbol.flags |= BSF_NOT_AT_END;
#endif
	      /* Fall through to next case */

#endif

	    case C_EXT:
#ifdef RS6000COFF_C
	    case C_HIDEXT:
#endif
	      if ((src->u.syment.n_scnum) == 0)
		{
		  if ((src->u.syment.n_value) == 0)
		    {
		      dst->symbol.section = bfd_und_section_ptr;
		      dst->symbol.value = 0;
		    }
		  else
		    {
		      dst->symbol.section = bfd_com_section_ptr;
		      dst->symbol.value = (src->u.syment.n_value);
		    }
		}
	      else
		{
		  /*
	    Base the value as an index from the base of the
	    section
	    */

		  dst->symbol.flags = BSF_EXPORT | BSF_GLOBAL;
		  dst->symbol.value = src->u.syment.n_value - dst->symbol.section->vma;

		  if (ISFCN ((src->u.syment.n_type)))
		    {
		      /*
	      A function ext does not go at the end of a file
	      */
		      dst->symbol.flags |= BSF_NOT_AT_END;
		    }
		}

#ifdef RS6000COFF_C
	      /* If this symbol has a csect aux of type LD, the scnlen field
	   is actually the index of the containing csect symbol.  We
	   need to pointerize it.  */
	      if (src->u.syment.n_numaux > 0)
		{
		  combined_entry_type *aux;

		  aux = src + src->u.syment.n_numaux - 1;
		  if (SMTYP_SMTYP (aux->u.auxent.x_csect.x_smtyp) == XTY_LD)
		    {
		      aux->u.auxent.x_csect.x_scnlen.p =
			native_symbols + aux->u.auxent.x_csect.x_scnlen.l;
		      aux->fix_scnlen = 1;
		    }
		}
#endif

	      break;

	    case C_STAT:	/* static			 */
#ifdef I960
	    case C_LEAFSTAT:	/* static leaf procedure        */
#endif
	    case C_LABEL:	/* label			 */
	      if (src->u.syment.n_scnum == -2)
		dst->symbol.flags = BSF_DEBUGGING;
	      else
		dst->symbol.flags = BSF_LOCAL;
	      /*
	  Base the value as an index from the base of the section, if
	  there is one
	  */
	      if (dst->symbol.section)
		dst->symbol.value = (src->u.syment.n_value) -
		  dst->symbol.section->vma;
	      else
		dst->symbol.value = (src->u.syment.n_value);
	      break;

	    case C_MOS:	/* member of structure	 */
	    case C_EOS:	/* end of structure		 */
#ifdef NOTDEF			/* C_AUTOARG has the same value */
#ifdef C_GLBLREG
	    case C_GLBLREG:	/* A29k-specific storage class */
#endif
#endif
	    case C_REGPARM:	/* register parameter		 */
	    case C_REG:	/* register variable		 */
#ifdef C_AUTOARG
	    case C_AUTOARG:	/* 960-specific storage class */
#endif
	    case C_TPDEF:	/* type definition		 */
	    case C_ARG:
	    case C_AUTO:	/* automatic variable */
	    case C_FIELD:	/* bit field */
	    case C_ENTAG:	/* enumeration tag		 */
	    case C_MOE:	/* member of enumeration	 */
	    case C_MOU:	/* member of union		 */
	    case C_UNTAG:	/* union tag			 */
	      dst->symbol.flags = BSF_DEBUGGING;
	      dst->symbol.value = (src->u.syment.n_value);
	      break;

	    case C_FILE:	/* file name			 */
	    case C_STRTAG:	/* structure tag		 */
#ifdef RS6000COFF_C
	    case C_BINCL:	/* beginning of include file     */
	    case C_EINCL:	/* ending of include file        */
	    case C_GSYM:
	    case C_LSYM:
	    case C_PSYM:
	    case C_RSYM:
	    case C_RPSYM:
	    case C_STSYM:
	    case C_DECL:
	    case C_ENTRY:
	    case C_FUN:
	    case C_ESTAT:
#endif
	      dst->symbol.flags = BSF_DEBUGGING;
	      dst->symbol.value = (src->u.syment.n_value);
	      break;

#ifdef RS6000COFF_C
	    case C_BSTAT:
	      dst->symbol.flags = BSF_DEBUGGING;
	      dst->symbol.value = src->u.syment.n_value;

	      /* The value is actually a symbol index.  Save a pointer to
	   the symbol instead of the index.  FIXME: This should use a
	   union.  */
	      src->u.syment.n_value =
		(long) (native_symbols + src->u.syment.n_value);
	      src->fix_value = 1;
	      break;
#endif

	    case C_BLOCK:	/* ".bb" or ".eb"		 */
	    case C_FCN:	/* ".bf" or ".ef"		 */
	    case C_EFCN:	/* physical end of function	 */
	      dst->symbol.flags = BSF_LOCAL;
	      /*
	  Base the value as an index from the base of the section
	  */
	      dst->symbol.value = (src->u.syment.n_value) - dst->symbol.section->vma;
	      break;

	    case C_NULL:
	    case C_EXTDEF:	/* external definition		 */
	    case C_ULABEL:	/* undefined label		 */
	    case C_USTATIC:	/* undefined static		 */
	    case C_LINE:	/* line # reformatted as symbol table entry */
	    case C_ALIAS:	/* duplicate tag		 */
	    case C_HIDDEN:	/* ext symbol in dmert public lib */
	    default:

	      fprintf (stderr, "Unrecognized storage class %d (assuming debugging)\n  for %s symbol `%s'\n",
		       src->u.syment.n_sclass, dst->symbol.section->name,
		       dst->symbol.name);
/*	abort();*/
	      dst->symbol.flags = BSF_DEBUGGING;
	      dst->symbol.value = (src->u.syment.n_value);
	      break;
	    }

/*      BFD_ASSERT(dst->symbol.flags != 0);*/

	  dst->native = src;

	  dst->symbol.udata = 0;
	  dst->lineno = (alent *) NULL;
	  this_index += (src->u.syment.n_numaux) + 1;
	  dst++;
	  number_of_symbols++;
	}			/* walk the native symtab */
    }				/* bfdize the native symtab */

  obj_symbols (abfd) = cached_area;
  obj_raw_syments (abfd) = native_symbols;

  obj_conv_table_size (abfd) = bfd_get_symcount (abfd);
  bfd_get_symcount (abfd) = number_of_symbols;
  obj_convert (abfd) = table_ptr;
  /* Slurp the line tables for each section too */
  {
    asection *p;
    p = abfd->sections;
    while (p)
      {
	coff_slurp_line_table (abfd, p);
	p = p->next;
      }
  }
  return true;
}				/* coff_slurp_symbol_table() */

/*
SUBSUBSECTION
	Reading relocations

	Coff relocations are easily transformed into the internal BFD form
	(@code{arelent}).

	Reading a coff relocation table is done in the following stages:

	o Read the entire coff relocation table into memory.

	o Process each relocation in turn; first swap it from the
	external to the internal form.

	o Turn the symbol referenced in the relocation's symbol index
	into a pointer into the canonical symbol table.
	This table is the same as the one returned by a call to
	@code{bfd_canonicalize_symtab}. The back end will call that
	routine and save the result if a canonicalization hasn't been done.

	o The reloc index is turned into a pointer to a howto
	structure, in a back end specific way. For instance, the 386
	and 960 use the @code{r_type} to directly produce an index
	into a howto table vector; the 88k subtracts a number from the
	@code{r_type} field and creates an addend field.


*/

#ifndef CALC_ADDEND
#define CALC_ADDEND(abfd, ptr, reloc, cache_ptr)                \
  {                                                             \
    coff_symbol_type *coffsym = (coff_symbol_type *) NULL;      \
    if (ptr && bfd_asymbol_bfd (ptr) != abfd)                   \
      coffsym = (obj_symbols (abfd)                             \
                 + (cache_ptr->sym_ptr_ptr - symbols));         \
    else if (ptr)                                               \
      coffsym = coff_symbol_from (abfd, ptr);                   \
    if (coffsym != (coff_symbol_type *) NULL                    \
        && coffsym->native->u.syment.n_scnum == 0)              \
      cache_ptr->addend = 0;                                    \
    else if (ptr && bfd_asymbol_bfd (ptr) == abfd               \
             && ptr->section != (asection *) NULL)              \
      cache_ptr->addend = - (ptr->section->vma + ptr->value);   \
    else                                                        \
      cache_ptr->addend = 0;                                    \
  }
#endif

static boolean
coff_slurp_reloc_table (abfd, asect, symbols)
     bfd * abfd;
     sec_ptr asect;
     asymbol ** symbols;
{
  RELOC *native_relocs;
  arelent *reloc_cache;
  arelent *cache_ptr;

  unsigned int idx;

  if (asect->relocation)
    return true;
  if (asect->reloc_count == 0)
    return true;
  if (asect->flags & SEC_CONSTRUCTOR)
    return true;
  if (!coff_slurp_symbol_table (abfd))
    return false;
  native_relocs =
    (RELOC *) buy_and_read (abfd,
			    asect->rel_filepos,
			    SEEK_SET,
			    (size_t) (RELSZ *
				      asect->reloc_count));
  reloc_cache = (arelent *)
    bfd_alloc (abfd, (size_t) (asect->reloc_count * sizeof (arelent)));

  if (reloc_cache == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }


  for (idx = 0; idx < asect->reloc_count; idx++)
    {
#ifdef RELOC_PROCESSING
      struct internal_reloc dst;
      struct external_reloc *src;

      cache_ptr = reloc_cache + idx;
      src = native_relocs + idx;
      bfd_swap_reloc_in (abfd, src, &dst);

      RELOC_PROCESSING (cache_ptr, &dst, symbols, abfd, asect);
#else
      struct internal_reloc dst;
      asymbol *ptr;
      struct external_reloc *src;

      cache_ptr = reloc_cache + idx;
      src = native_relocs + idx;

      bfd_swap_reloc_in (abfd, src, &dst);


      cache_ptr->address = dst.r_vaddr;

      if (dst.r_symndx != -1)
	{
	  /* @@ Should never be greater than count of symbols!  */
	  if (dst.r_symndx >= obj_conv_table_size (abfd))
	    abort ();
	  cache_ptr->sym_ptr_ptr = symbols + obj_convert (abfd)[dst.r_symndx];
	  ptr = *(cache_ptr->sym_ptr_ptr);
	}
      else
	{
	  cache_ptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	  ptr = 0;
	}

      /* The symbols definitions that we have read in have been
	 relocated as if their sections started at 0. But the offsets
	 refering to the symbols in the raw data have not been
	 modified, so we have to have a negative addend to compensate.

	 Note that symbols which used to be common must be left alone */

      /* Calculate any reloc addend by looking at the symbol */
      CALC_ADDEND (abfd, ptr, dst, cache_ptr);

      cache_ptr->address -= asect->vma;
/* !!     cache_ptr->section = (asection *) NULL;*/

      /* Fill in the cache_ptr->howto field from dst.r_type */
      RTYPE2HOWTO (cache_ptr, &dst);
#endif

    }

  asect->relocation = reloc_cache;
  return true;
}


/* This is stupid.  This function should be a boolean predicate.  */
static long
coff_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd * abfd;
     sec_ptr section;
     arelent ** relptr;
     asymbol ** symbols;
{
  arelent *tblptr = section->relocation;
  unsigned int count = 0;


  if (section->flags & SEC_CONSTRUCTOR)
    {
      /* this section has relocs made up by us, they are not in the
       file, so take them out of their chain and place them into
       the data area provided */
      arelent_chain *chain = section->constructor_chain;
      for (count = 0; count < section->reloc_count; count++)
	{
	  *relptr++ = &chain->relent;
	  chain = chain->next;
	}

    }
  else
    {
      if (! coff_slurp_reloc_table (abfd, section, symbols))
	return -1;

      tblptr = section->relocation;

      for (; count++ < section->reloc_count;)
	*relptr++ = tblptr++;


    }
  *relptr = 0;
  return section->reloc_count;
}

#ifdef GNU960
file_ptr
coff_sym_filepos (abfd)
     bfd *abfd;
{
  return obj_sym_filepos (abfd);
}
#endif

#ifndef coff_reloc16_estimate
#define coff_reloc16_estimate dummy_reloc16_estimate

static int
dummy_reloc16_estimate (abfd, input_section, reloc, shrink, link_info)
     bfd *abfd;
     asection *input_section;
     arelent *reloc;
     unsigned int shrink;
     struct bfd_link_info *link_info;
{
  abort ();
}

#endif

#ifndef coff_reloc16_extra_cases
#define coff_reloc16_extra_cases dummy_reloc16_extra_cases
/* This works even if abort is not declared in any header file.  */
static void
dummy_reloc16_extra_cases (abfd, link_info, link_order, reloc, data, src_ptr,
			   dst_ptr)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     arelent *reloc;
     bfd_byte *data;
     unsigned int *src_ptr;
     unsigned int *dst_ptr;
{
  fprintf (stderr, "%s\n", reloc->howto->name);
  abort ();
}
#endif

static CONST bfd_coff_backend_data bfd_coff_std_swap_table =
{
  coff_swap_aux_in, coff_swap_sym_in, coff_swap_lineno_in,
  coff_swap_aux_out, coff_swap_sym_out,
  coff_swap_lineno_out, coff_swap_reloc_out,
  coff_swap_filehdr_out, coff_swap_aouthdr_out,
  coff_swap_scnhdr_out,
  FILHSZ, AOUTSZ, SCNHSZ, SYMESZ, AUXESZ, LINESZ,
#ifdef COFF_LONG_FILENAMES
  true,
#else
  false,
#endif
  coff_swap_filehdr_in, coff_swap_aouthdr_in, coff_swap_scnhdr_in,
  coff_bad_format_hook, coff_set_arch_mach_hook, coff_mkobject_hook,
  styp_to_sec_flags, coff_make_section_hook, coff_set_alignment_hook,
  coff_slurp_symbol_table, symname_in_debug_hook,
  coff_reloc16_extra_cases, coff_reloc16_estimate
};

#define	coff_close_and_cleanup _bfd_generic_close_and_cleanup
#define coff_bfd_free_cached_info _bfd_generic_bfd_free_cached_info
#define	coff_get_section_contents _bfd_generic_get_section_contents

#define coff_bfd_copy_private_section_data \
  _bfd_generic_bfd_copy_private_section_data
#define coff_bfd_copy_private_bfd_data _bfd_generic_bfd_copy_private_bfd_data

#ifndef coff_bfd_is_local_label
#define coff_bfd_is_local_label bfd_generic_is_local_label
#endif

/* The reloc lookup routine must be supplied by each individual COFF
   backend.  */
#ifndef coff_bfd_reloc_type_lookup
#define coff_bfd_reloc_type_lookup _bfd_norelocs_bfd_reloc_type_lookup
#endif

#define coff_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define coff_bfd_relax_section bfd_generic_relax_section
#define coff_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define coff_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define coff_bfd_final_link _bfd_generic_final_link
