/* BFD semi-generic back-end for a.out binaries.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 98, 99, 2000
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
	a.out backends

DESCRIPTION

	BFD supports a number of different flavours of a.out format,
	though the major differences are only the sizes of the
	structures on disk, and the shape of the relocation
	information.

	The support is split into a basic support file @file{aoutx.h}
	and other files which derive functions from the base. One
	derivation file is @file{aoutf1.h} (for a.out flavour 1), and
	adds to the basic a.out functions support for sun3, sun4, 386
	and 29k a.out files, to create a target jump vector for a
	specific target.

	This information is further split out into more specific files
	for each machine, including @file{sunos.c} for sun3 and sun4,
	@file{newsos3.c} for the Sony NEWS, and @file{demo64.c} for a
	demonstration of a 64 bit a.out format.

	The base file @file{aoutx.h} defines general mechanisms for
	reading and writing records to and from disk and various
	other methods which BFD requires. It is included by
	@file{aout32.c} and @file{aout64.c} to form the names
	<<aout_32_swap_exec_header_in>>, <<aout_64_swap_exec_header_in>>, etc.

	As an example, this is what goes on to make the back end for a
	sun4, from @file{aout32.c}:

|	#define ARCH_SIZE 32
|	#include "aoutx.h"

	Which exports names:

|	...
|	aout_32_canonicalize_reloc
|	aout_32_find_nearest_line
|	aout_32_get_lineno
|	aout_32_get_reloc_upper_bound
|	...

	from @file{sunos.c}:

|	#define TARGET_NAME "a.out-sunos-big"
|	#define VECNAME    sunos_big_vec
|	#include "aoutf1.h"

	requires all the names from @file{aout32.c}, and produces the jump vector

|	sunos_big_vec

	The file @file{host-aout.c} is a special case.  It is for a large set
	of hosts that use ``more or less standard'' a.out files, and
	for which cross-debugging is not interesting.  It uses the
	standard 32-bit a.out support routines, but determines the
	file offsets and addresses of the text, data, and BSS
	sections, the machine architecture and machine type, and the
	entry point address, in a host-dependent manner.  Once these
	values have been determined, generic code is used to handle
	the  object file.

	When porting it to run on a new system, you must supply:

|        HOST_PAGE_SIZE
|        HOST_SEGMENT_SIZE
|        HOST_MACHINE_ARCH       (optional)
|        HOST_MACHINE_MACHINE    (optional)
|        HOST_TEXT_START_ADDR
|        HOST_STACK_END_ADDR

	in the file @file{../include/sys/h-@var{XXX}.h} (for your host).  These
	values, plus the structures and macros defined in @file{a.out.h} on
	your host system, will produce a BFD target that will access
	ordinary a.out files on your host. To configure a new machine
	to use @file{host-aout.c}, specify:

|	TDEFAULTS = -DDEFAULT_VECTOR=host_aout_big_vec
|	TDEPFILES= host-aout.o trad-core.o

	in the @file{config/@var{XXX}.mt} file, and modify @file{configure.in}
	to use the
	@file{@var{XXX}.mt} file (by setting "<<bfd_target=XXX>>") when your
	configuration is selected.

*/

/* Some assumptions:
   * Any BFD with D_PAGED set is ZMAGIC, and vice versa.
     Doesn't matter what the setting of WP_TEXT is on output, but it'll
     get set on input.
   * Any BFD with D_PAGED clear and WP_TEXT set is NMAGIC.
   * Any BFD with both flags clear is OMAGIC.
   (Just want to make these explicit, so the conditions tested in this
   file make sense if you're more familiar with a.out than with BFD.)  */

#define KEEPIT udata.i

#include <ctype.h>
#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"

#include "libaout.h"
#include "libbfd.h"
#include "aout/aout64.h"
#include "aout/stab_gnu.h"
#include "aout/ar.h"

static boolean aout_get_external_symbols PARAMS ((bfd *));
static boolean translate_from_native_sym_flags
  PARAMS ((bfd *, aout_symbol_type *));
static boolean translate_to_native_sym_flags
  PARAMS ((bfd *, asymbol *, struct external_nlist *));
static void adjust_o_magic PARAMS ((bfd *, struct internal_exec *));
static void adjust_z_magic PARAMS ((bfd *, struct internal_exec *));
static void adjust_n_magic PARAMS ((bfd *, struct internal_exec *));

/*
SUBSECTION
	Relocations

DESCRIPTION
	The file @file{aoutx.h} provides for both the @emph{standard}
	and @emph{extended} forms of a.out relocation records.

	The standard records contain only an
	address, a symbol index, and a type field. The extended records
	(used on 29ks and sparcs) also have a full integer for an
	addend.

*/
#ifndef CTOR_TABLE_RELOC_HOWTO
#define CTOR_TABLE_RELOC_IDX 2
#define CTOR_TABLE_RELOC_HOWTO(BFD) ((obj_reloc_entry_size(BFD) == RELOC_EXT_SIZE \
	     ? howto_table_ext : howto_table_std) \
	    + CTOR_TABLE_RELOC_IDX)
#endif

#ifndef MY_swap_std_reloc_in
#define MY_swap_std_reloc_in NAME(aout,swap_std_reloc_in)
#endif

#ifndef MY_swap_ext_reloc_in
#define MY_swap_ext_reloc_in NAME(aout,swap_ext_reloc_in)
#endif

#ifndef MY_swap_std_reloc_out
#define MY_swap_std_reloc_out NAME(aout,swap_std_reloc_out)
#endif

#ifndef MY_swap_ext_reloc_out
#define MY_swap_ext_reloc_out NAME(aout,swap_ext_reloc_out)
#endif

#ifndef MY_final_link_relocate
#define MY_final_link_relocate _bfd_final_link_relocate
#endif

#ifndef MY_relocate_contents
#define MY_relocate_contents _bfd_relocate_contents
#endif

#define howto_table_ext NAME(aout,ext_howto_table)
#define howto_table_std NAME(aout,std_howto_table)

reloc_howto_type howto_table_ext[] =
{
  /* type           rs   size bsz  pcrel bitpos ovrf                  sf name          part_inpl readmask setmask pcdone */
  HOWTO(RELOC_8,      0,  0,  	8,  false, 0, complain_overflow_bitfield,0,"8",        false, 0,0x000000ff, false),
  HOWTO(RELOC_16,     0,  1, 	16, false, 0, complain_overflow_bitfield,0,"16",       false, 0,0x0000ffff, false),
  HOWTO(RELOC_32,     0,  2, 	32, false, 0, complain_overflow_bitfield,0,"32",       false, 0,0xffffffff, false),
  HOWTO(RELOC_DISP8,  0,  0, 	8,  true,  0, complain_overflow_signed,0,"DISP8", 	false, 0,0x000000ff, false),
  HOWTO(RELOC_DISP16, 0,  1, 	16, true,  0, complain_overflow_signed,0,"DISP16", 	false, 0,0x0000ffff, false),
  HOWTO(RELOC_DISP32, 0,  2, 	32, true,  0, complain_overflow_signed,0,"DISP32", 	false, 0,0xffffffff, false),
  HOWTO(RELOC_WDISP30,2,  2, 	30, true,  0, complain_overflow_signed,0,"WDISP30", 	false, 0,0x3fffffff, false),
  HOWTO(RELOC_WDISP22,2,  2, 	22, true,  0, complain_overflow_signed,0,"WDISP22", 	false, 0,0x003fffff, false),
  HOWTO(RELOC_HI22,   10, 2, 	22, false, 0, complain_overflow_bitfield,0,"HI22",	false, 0,0x003fffff, false),
  HOWTO(RELOC_22,     0,  2, 	22, false, 0, complain_overflow_bitfield,0,"22",       false, 0,0x003fffff, false),
  HOWTO(RELOC_13,     0,  2, 	13, false, 0, complain_overflow_bitfield,0,"13",       false, 0,0x00001fff, false),
  HOWTO(RELOC_LO10,   0,  2, 	10, false, 0, complain_overflow_dont,0,"LO10",     false, 0,0x000003ff, false),
  HOWTO(RELOC_SFA_BASE,0, 2, 	32, false, 0, complain_overflow_bitfield,0,"SFA_BASE", false, 0,0xffffffff, false),
  HOWTO(RELOC_SFA_OFF13,0,2, 	32, false, 0, complain_overflow_bitfield,0,"SFA_OFF13",false, 0,0xffffffff, false),
  HOWTO(RELOC_BASE10, 0,  2, 	10, false, 0, complain_overflow_dont,0,"BASE10",   false, 0,0x000003ff, false),
  HOWTO(RELOC_BASE13, 0,  2,	13, false, 0, complain_overflow_signed,0,"BASE13",   false, 0,0x00001fff, false),
  HOWTO(RELOC_BASE22, 10, 2,	22, false, 0, complain_overflow_bitfield,0,"BASE22",   false, 0,0x003fffff, false),
  HOWTO(RELOC_PC10,   0,  2,	10, true,  0, complain_overflow_dont,0,"PC10",	false, 0,0x000003ff, true),
  HOWTO(RELOC_PC22,   10,  2,	22, true,  0, complain_overflow_signed,0,"PC22", false, 0,0x003fffff, true),
  HOWTO(RELOC_JMP_TBL,2,  2, 	30, true,  0, complain_overflow_signed,0,"JMP_TBL", 	false, 0,0x3fffffff, false),
  HOWTO(RELOC_SEGOFF16,0, 2,	0,  false, 0, complain_overflow_bitfield,0,"SEGOFF16",	false, 0,0x00000000, false),
  HOWTO(RELOC_GLOB_DAT,0, 2,	0,  false, 0, complain_overflow_bitfield,0,"GLOB_DAT",	false, 0,0x00000000, false),
  HOWTO(RELOC_JMP_SLOT,0, 2,	0,  false, 0, complain_overflow_bitfield,0,"JMP_SLOT",	false, 0,0x00000000, false),
  HOWTO(RELOC_RELATIVE,0, 2,	0,  false, 0, complain_overflow_bitfield,0,"RELATIVE",	false, 0,0x00000000, false),
  HOWTO(0,  0, 0,    0,  false, 0, complain_overflow_dont, 0, "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(0,  0, 0,    0,  false, 0, complain_overflow_dont, 0, "R_SPARC_NONE",    false,0,0x00000000,true),
#define RELOC_SPARC_REV32 RELOC_WDISP19
  HOWTO(RELOC_SPARC_REV32,    0,  2, 	32, false, 0, complain_overflow_dont,0,"R_SPARC_REV32",       false, 0,0xffffffff, false),
};

/* Convert standard reloc records to "arelent" format (incl byte swap).  */

reloc_howto_type howto_table_std[] = {
  /* type              rs size bsz  pcrel bitpos ovrf                     sf name     part_inpl readmask  setmask    pcdone */
HOWTO( 0,	       0,  0,  	8,  false, 0, complain_overflow_bitfield,0,"8",		true, 0x000000ff,0x000000ff, false),
HOWTO( 1,	       0,  1, 	16, false, 0, complain_overflow_bitfield,0,"16",	true, 0x0000ffff,0x0000ffff, false),
HOWTO( 2,	       0,  2, 	32, false, 0, complain_overflow_bitfield,0,"32",	true, 0xffffffff,0xffffffff, false),
HOWTO( 3,	       0,  4, 	64, false, 0, complain_overflow_bitfield,0,"64",	true, 0xdeaddead,0xdeaddead, false),
HOWTO( 4,	       0,  0, 	8,  true,  0, complain_overflow_signed,  0,"DISP8",	true, 0x000000ff,0x000000ff, false),
HOWTO( 5,	       0,  1, 	16, true,  0, complain_overflow_signed,  0,"DISP16",	true, 0x0000ffff,0x0000ffff, false),
HOWTO( 6,	       0,  2, 	32, true,  0, complain_overflow_signed,  0,"DISP32",	true, 0xffffffff,0xffffffff, false),
HOWTO( 7,	       0,  4, 	64, true,  0, complain_overflow_signed,  0,"DISP64",	true, 0xfeedface,0xfeedface, false),
HOWTO( 8,	       0,  2,    0, false, 0, complain_overflow_bitfield,0,"GOT_REL",	false,         0,0x00000000, false),
HOWTO( 9,	       0,  1,   16, false, 0, complain_overflow_bitfield,0,"BASE16",	false,0xffffffff,0xffffffff, false),
HOWTO(10,	       0,  2,   32, false, 0, complain_overflow_bitfield,0,"BASE32",	false,0xffffffff,0xffffffff, false),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
  HOWTO(16,	       0,  2,	 0, false, 0, complain_overflow_bitfield,0,"JMP_TABLE", false,         0,0x00000000, false),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
  HOWTO(32,	       0,  2,	 0, false, 0, complain_overflow_bitfield,0,"RELATIVE",  false,         0,0x00000000, false),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
EMPTY_HOWTO (-1),
  HOWTO(40,	       0,  2,	 0, false, 0, complain_overflow_bitfield,0,"BASEREL",   false,         0,0x00000000, false),
};

#define TABLE_SIZE(TABLE)	(sizeof (TABLE)/sizeof (TABLE[0]))

reloc_howto_type *
NAME(aout,reloc_type_lookup) (abfd,code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
#define EXT(i,j)	case i: return &howto_table_ext[j]
#define STD(i,j)	case i: return &howto_table_std[j]
  int ext = obj_reloc_entry_size (abfd) == RELOC_EXT_SIZE;
  if (code == BFD_RELOC_CTOR)
    switch (bfd_get_arch_info (abfd)->bits_per_address)
      {
      case 32:
	code = BFD_RELOC_32;
	break;
      case 64:
	code = BFD_RELOC_64;
	break;
      }
  if (ext)
    switch (code)
      {
	EXT (BFD_RELOC_8, 0);
	EXT (BFD_RELOC_16, 1);
	EXT (BFD_RELOC_32, 2);
	EXT (BFD_RELOC_HI22, 8);
	EXT (BFD_RELOC_LO10, 11);
	EXT (BFD_RELOC_32_PCREL_S2, 6);
	EXT (BFD_RELOC_SPARC_WDISP22, 7);
	EXT (BFD_RELOC_SPARC13, 10);
	EXT (BFD_RELOC_SPARC_GOT10, 14);
	EXT (BFD_RELOC_SPARC_BASE13, 15);
	EXT (BFD_RELOC_SPARC_GOT13, 15);
	EXT (BFD_RELOC_SPARC_GOT22, 16);
	EXT (BFD_RELOC_SPARC_PC10, 17);
	EXT (BFD_RELOC_SPARC_PC22, 18);
	EXT (BFD_RELOC_SPARC_WPLT30, 19);
	EXT (BFD_RELOC_SPARC_REV32, 26);
      default: return (reloc_howto_type *) NULL;
      }
  else
    /* std relocs */
    switch (code)
      {
	STD (BFD_RELOC_16, 1);
	STD (BFD_RELOC_32, 2);
	STD (BFD_RELOC_8_PCREL, 4);
	STD (BFD_RELOC_16_PCREL, 5);
	STD (BFD_RELOC_32_PCREL, 6);
	STD (BFD_RELOC_16_BASEREL, 9);
	STD (BFD_RELOC_32_BASEREL, 10);
      default: return (reloc_howto_type *) NULL;
      }
}

/*
SUBSECTION
	Internal entry points

DESCRIPTION
	@file{aoutx.h} exports several routines for accessing the
	contents of an a.out file, which are gathered and exported in
	turn by various format specific files (eg sunos.c).

*/

/*
FUNCTION
	 aout_@var{size}_swap_exec_header_in

SYNOPSIS
	void aout_@var{size}_swap_exec_header_in,
           (bfd *abfd,
            struct external_exec *raw_bytes,
            struct internal_exec *execp);

DESCRIPTION
	Swap the information in an executable header @var{raw_bytes} taken
	from a raw byte stream memory image into the internal exec header
	structure @var{execp}.
*/

#ifndef NAME_swap_exec_header_in
void
NAME(aout,swap_exec_header_in) (abfd, raw_bytes, execp)
     bfd *abfd;
     struct external_exec *raw_bytes;
     struct internal_exec *execp;
{
  struct external_exec *bytes = (struct external_exec *)raw_bytes;

  /* The internal_exec structure has some fields that are unused in this
     configuration (IE for i960), so ensure that all such uninitialized
     fields are zero'd out.  There are places where two of these structs
     are memcmp'd, and thus the contents do matter.  */
  memset ((PTR) execp, 0, sizeof (struct internal_exec));
  /* Now fill in fields in the execp, from the bytes in the raw data.  */
  execp->a_info   = bfd_h_get_32 (abfd, bytes->e_info);
  execp->a_text   = GET_WORD (abfd, bytes->e_text);
  execp->a_data   = GET_WORD (abfd, bytes->e_data);
  execp->a_bss    = GET_WORD (abfd, bytes->e_bss);
  execp->a_syms   = GET_WORD (abfd, bytes->e_syms);
  execp->a_entry  = GET_WORD (abfd, bytes->e_entry);
  execp->a_trsize = GET_WORD (abfd, bytes->e_trsize);
  execp->a_drsize = GET_WORD (abfd, bytes->e_drsize);
}
#define NAME_swap_exec_header_in NAME(aout,swap_exec_header_in)
#endif

/*
FUNCTION
	aout_@var{size}_swap_exec_header_out

SYNOPSIS
	void aout_@var{size}_swap_exec_header_out
	  (bfd *abfd,
	   struct internal_exec *execp,
	   struct external_exec *raw_bytes);

DESCRIPTION
	Swap the information in an internal exec header structure
	@var{execp} into the buffer @var{raw_bytes} ready for writing to disk.
*/
void
NAME(aout,swap_exec_header_out) (abfd, execp, raw_bytes)
     bfd *abfd;
     struct internal_exec *execp;
     struct external_exec *raw_bytes;
{
  struct external_exec *bytes = (struct external_exec *)raw_bytes;

  /* Now fill in fields in the raw data, from the fields in the exec struct.  */
  bfd_h_put_32 (abfd, execp->a_info  , bytes->e_info);
  PUT_WORD (abfd, execp->a_text  , bytes->e_text);
  PUT_WORD (abfd, execp->a_data  , bytes->e_data);
  PUT_WORD (abfd, execp->a_bss   , bytes->e_bss);
  PUT_WORD (abfd, execp->a_syms  , bytes->e_syms);
  PUT_WORD (abfd, execp->a_entry , bytes->e_entry);
  PUT_WORD (abfd, execp->a_trsize, bytes->e_trsize);
  PUT_WORD (abfd, execp->a_drsize, bytes->e_drsize);
}

/* Make all the section for an a.out file.  */

boolean
NAME(aout,make_sections) (abfd)
     bfd *abfd;
{
  if (obj_textsec (abfd) == (asection *) NULL
      && bfd_make_section (abfd, ".text") == (asection *) NULL)
    return false;
  if (obj_datasec (abfd) == (asection *) NULL
      && bfd_make_section (abfd, ".data") == (asection *) NULL)
    return false;
  if (obj_bsssec (abfd) == (asection *) NULL
      && bfd_make_section (abfd, ".bss") == (asection *) NULL)
    return false;
  return true;
}

/*
FUNCTION
	aout_@var{size}_some_aout_object_p

SYNOPSIS
	const bfd_target *aout_@var{size}_some_aout_object_p
	 (bfd *abfd,
	  const bfd_target *(*callback_to_real_object_p) ());

DESCRIPTION
	Some a.out variant thinks that the file open in @var{abfd}
	checking is an a.out file.  Do some more checking, and set up
	for access if it really is.  Call back to the calling
	environment's "finish up" function just before returning, to
	handle any last-minute setup.
*/

const bfd_target *
NAME(aout,some_aout_object_p) (abfd, execp, callback_to_real_object_p)
     bfd *abfd;
     struct internal_exec *execp;
     const bfd_target *(*callback_to_real_object_p) PARAMS ((bfd *));
{
  struct aout_data_struct *rawptr, *oldrawptr;
  const bfd_target *result;

  rawptr = (struct aout_data_struct  *) bfd_zalloc (abfd, sizeof (struct aout_data_struct ));
  if (rawptr == NULL)
    return 0;

  oldrawptr = abfd->tdata.aout_data;
  abfd->tdata.aout_data = rawptr;

  /* Copy the contents of the old tdata struct.
     In particular, we want the subformat, since for hpux it was set in
     hp300hpux.c:swap_exec_header_in and will be used in
     hp300hpux.c:callback.  */
  if (oldrawptr != NULL)
    *abfd->tdata.aout_data = *oldrawptr;

  abfd->tdata.aout_data->a.hdr = &rawptr->e;
  *(abfd->tdata.aout_data->a.hdr) = *execp;	/* Copy in the internal_exec struct */
  execp = abfd->tdata.aout_data->a.hdr;

  /* Set the file flags */
  abfd->flags = BFD_NO_FLAGS;
  if (execp->a_drsize || execp->a_trsize)
    abfd->flags |= HAS_RELOC;
  /* Setting of EXEC_P has been deferred to the bottom of this function */
  if (execp->a_syms)
    abfd->flags |= HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS;
  if (N_DYNAMIC(*execp))
    abfd->flags |= DYNAMIC;

  if (N_MAGIC (*execp) == ZMAGIC)
    {
      abfd->flags |= D_PAGED | WP_TEXT;
      adata (abfd).magic = z_magic;
    }
  else if (N_MAGIC (*execp) == QMAGIC)
    {
      abfd->flags |= D_PAGED | WP_TEXT;
      adata (abfd).magic = z_magic;
      adata (abfd).subformat = q_magic_format;
    }
  else if (N_MAGIC (*execp) == NMAGIC)
    {
      abfd->flags |= WP_TEXT;
      adata (abfd).magic = n_magic;
    }
  else if (N_MAGIC (*execp) == OMAGIC
	   || N_MAGIC (*execp) == BMAGIC)
    adata (abfd).magic = o_magic;
  else
    {
      /* Should have been checked with N_BADMAG before this routine
	 was called.  */
      abort ();
    }

  bfd_get_start_address (abfd) = execp->a_entry;

  obj_aout_symbols (abfd) = (aout_symbol_type *)NULL;
  bfd_get_symcount (abfd) = execp->a_syms / sizeof (struct external_nlist);

  /* The default relocation entry size is that of traditional V7 Unix.  */
  obj_reloc_entry_size (abfd) = RELOC_STD_SIZE;

  /* The default symbol entry size is that of traditional Unix.  */
  obj_symbol_entry_size (abfd) = EXTERNAL_NLIST_SIZE;

#ifdef USE_MMAP
  bfd_init_window (&obj_aout_sym_window (abfd));
  bfd_init_window (&obj_aout_string_window (abfd));
#endif
  obj_aout_external_syms (abfd) = NULL;
  obj_aout_external_strings (abfd) = NULL;
  obj_aout_sym_hashes (abfd) = NULL;

  if (! NAME(aout,make_sections) (abfd))
    return NULL;

  obj_datasec (abfd)->_raw_size = execp->a_data;
  obj_bsssec (abfd)->_raw_size = execp->a_bss;

  obj_textsec (abfd)->flags =
    (execp->a_trsize != 0
     ? (SEC_ALLOC | SEC_LOAD | SEC_CODE | SEC_HAS_CONTENTS | SEC_RELOC)
     : (SEC_ALLOC | SEC_LOAD | SEC_CODE | SEC_HAS_CONTENTS));
  obj_datasec (abfd)->flags =
    (execp->a_drsize != 0
     ? (SEC_ALLOC | SEC_LOAD | SEC_DATA | SEC_HAS_CONTENTS | SEC_RELOC)
     : (SEC_ALLOC | SEC_LOAD | SEC_DATA | SEC_HAS_CONTENTS));
  obj_bsssec (abfd)->flags = SEC_ALLOC;

#ifdef THIS_IS_ONLY_DOCUMENTATION
  /* The common code can't fill in these things because they depend
     on either the start address of the text segment, the rounding
     up of virtual addresses between segments, or the starting file
     position of the text segment -- all of which varies among different
     versions of a.out.  */

  /* Call back to the format-dependent code to fill in the rest of the
     fields and do any further cleanup.  Things that should be filled
     in by the callback:  */

  struct exec *execp = exec_hdr (abfd);

  obj_textsec (abfd)->size = N_TXTSIZE(*execp);
  obj_textsec (abfd)->raw_size = N_TXTSIZE(*execp);
  /* data and bss are already filled in since they're so standard */

  /* The virtual memory addresses of the sections */
  obj_textsec (abfd)->vma = N_TXTADDR(*execp);
  obj_datasec (abfd)->vma = N_DATADDR(*execp);
  obj_bsssec  (abfd)->vma = N_BSSADDR(*execp);

  /* The file offsets of the sections */
  obj_textsec (abfd)->filepos = N_TXTOFF(*execp);
  obj_datasec (abfd)->filepos = N_DATOFF(*execp);

  /* The file offsets of the relocation info */
  obj_textsec (abfd)->rel_filepos = N_TRELOFF(*execp);
  obj_datasec (abfd)->rel_filepos = N_DRELOFF(*execp);

  /* The file offsets of the string table and symbol table.  */
  obj_str_filepos (abfd) = N_STROFF (*execp);
  obj_sym_filepos (abfd) = N_SYMOFF (*execp);

  /* Determine the architecture and machine type of the object file.  */
  switch (N_MACHTYPE (*exec_hdr (abfd))) {
  default:
    abfd->obj_arch = bfd_arch_obscure;
    break;
  }

  adata(abfd)->page_size = TARGET_PAGE_SIZE;
  adata(abfd)->segment_size = SEGMENT_SIZE;
  adata(abfd)->exec_bytes_size = EXEC_BYTES_SIZE;

  return abfd->xvec;

  /* The architecture is encoded in various ways in various a.out variants,
     or is not encoded at all in some of them.  The relocation size depends
     on the architecture and the a.out variant.  Finally, the return value
     is the bfd_target vector in use.  If an error occurs, return zero and
     set bfd_error to the appropriate error code.

     Formats such as b.out, which have additional fields in the a.out
     header, should cope with them in this callback as well.  */
#endif				/* DOCUMENTATION */

  result = (*callback_to_real_object_p) (abfd);

  /* Now that the segment addresses have been worked out, take a better
     guess at whether the file is executable.  If the entry point
     is within the text segment, assume it is.  (This makes files
     executable even if their entry point address is 0, as long as
     their text starts at zero.).

     This test had to be changed to deal with systems where the text segment
     runs at a different location than the default.  The problem is that the
     entry address can appear to be outside the text segment, thus causing an
     erroneous conclusion that the file isn't executable.

     To fix this, we now accept any non-zero entry point as an indication of
     executability.  This will work most of the time, since only the linker
     sets the entry point, and that is likely to be non-zero for most systems.  */

  if (execp->a_entry != 0
      || (execp->a_entry >= obj_textsec(abfd)->vma
	  && execp->a_entry < obj_textsec(abfd)->vma + obj_textsec(abfd)->_raw_size))
    abfd->flags |= EXEC_P;
#ifdef STAT_FOR_EXEC
  else
    {
      struct stat stat_buf;

      /* The original heuristic doesn't work in some important cases.
        The a.out file has no information about the text start
        address.  For files (like kernels) linked to non-standard
        addresses (ld -Ttext nnn) the entry point may not be between
        the default text start (obj_textsec(abfd)->vma) and
        (obj_textsec(abfd)->vma) + text size.  This is not just a mach
        issue.  Many kernels are loaded at non standard addresses.  */
      if (abfd->iostream != NULL
	  && (abfd->flags & BFD_IN_MEMORY) == 0
	  && (fstat(fileno((FILE *) (abfd->iostream)), &stat_buf) == 0)
	  && ((stat_buf.st_mode & 0111) != 0))
	abfd->flags |= EXEC_P;
    }
#endif /* STAT_FOR_EXEC */

  if (result)
    {
#if 0 /* These should be set correctly anyways.  */
      abfd->sections = obj_textsec (abfd);
      obj_textsec (abfd)->next = obj_datasec (abfd);
      obj_datasec (abfd)->next = obj_bsssec (abfd);
#endif
    }
  else
    {
      free (rawptr);
      abfd->tdata.aout_data = oldrawptr;
    }
  return result;
}

/*
FUNCTION
	aout_@var{size}_mkobject

SYNOPSIS
	boolean aout_@var{size}_mkobject, (bfd *abfd);

DESCRIPTION
	Initialize BFD @var{abfd} for use with a.out files.
*/

boolean
NAME(aout,mkobject) (abfd)
     bfd *abfd;
{
  struct aout_data_struct  *rawptr;

  bfd_set_error (bfd_error_system_call);

  /* Use an intermediate variable for clarity */
  rawptr = (struct aout_data_struct *)bfd_zalloc (abfd, sizeof (struct aout_data_struct ));

  if (rawptr == NULL)
    return false;

  abfd->tdata.aout_data = rawptr;
  exec_hdr (abfd) = &(rawptr->e);

  obj_textsec (abfd) = (asection *)NULL;
  obj_datasec (abfd) = (asection *)NULL;
  obj_bsssec (abfd) = (asection *)NULL;

  return true;
}

/*
FUNCTION
	aout_@var{size}_machine_type

SYNOPSIS
	enum machine_type  aout_@var{size}_machine_type
	 (enum bfd_architecture arch,
	  unsigned long machine));

DESCRIPTION
	Keep track of machine architecture and machine type for
	a.out's. Return the <<machine_type>> for a particular
	architecture and machine, or <<M_UNKNOWN>> if that exact architecture
	and machine can't be represented in a.out format.

	If the architecture is understood, machine type 0 (default)
	is always understood.
*/

enum machine_type
NAME(aout,machine_type) (arch, machine, unknown)
     enum bfd_architecture arch;
     unsigned long machine;
     boolean *unknown;
{
  enum machine_type arch_flags;

  arch_flags = M_UNKNOWN;
  *unknown = true;

  switch (arch) {
  case bfd_arch_sparc:
    if (machine == 0
	|| machine == bfd_mach_sparc
	|| machine == bfd_mach_sparc_sparclite
	|| machine == bfd_mach_sparc_sparclite_le
	|| machine == bfd_mach_sparc_v9)
      arch_flags = M_SPARC;
    else if (machine == bfd_mach_sparc_sparclet)
      arch_flags = M_SPARCLET;
    break;

  case bfd_arch_m68k:
    switch (machine) {
    case 0:		  arch_flags = M_68010; break;
    case bfd_mach_m68000: arch_flags = M_UNKNOWN; *unknown = false; break;
    case bfd_mach_m68010: arch_flags = M_68010; break;
    case bfd_mach_m68020: arch_flags = M_68020; break;
    default:		  arch_flags = M_UNKNOWN; break;
    }
    break;

  case bfd_arch_i386:
    if (machine == 0)	arch_flags = M_386;
    break;

  case bfd_arch_a29k:
    if (machine == 0)	arch_flags = M_29K;
    break;

  case bfd_arch_arm:
    if (machine == 0)	arch_flags = M_ARM;
    break;

  case bfd_arch_mips:
    switch (machine) {
    case 0:
    case bfd_mach_mips3000:
    case bfd_mach_mips3900:
      arch_flags = M_MIPS1;
      break;
    case bfd_mach_mips6000:
      arch_flags = M_MIPS2;
      break;
    case bfd_mach_mips4000:
    case bfd_mach_mips4010:
    case bfd_mach_mips4100:
    case bfd_mach_mips4300:
    case bfd_mach_mips4400:
    case bfd_mach_mips4600:
    case bfd_mach_mips4650:
    case bfd_mach_mips8000:
    case bfd_mach_mips10000:
    case bfd_mach_mips16:
    case bfd_mach_mips32:
    case bfd_mach_mips32_4k:
    case bfd_mach_mips5:
    case bfd_mach_mips64:
    case bfd_mach_mips_sb1:
      /* FIXME: These should be MIPS3, MIPS4, MIPS16, MIPS32, etc.  */
      arch_flags = M_MIPS2;
      break;
    default:
      arch_flags = M_UNKNOWN;
      break;
    }
    break;

  case bfd_arch_ns32k:
    switch (machine) {
    case 0:    		arch_flags = M_NS32532; break;
    case 32032:		arch_flags = M_NS32032; break;
    case 32532:		arch_flags = M_NS32532; break;
    default:		arch_flags = M_UNKNOWN; break;
    }
    break;

  case bfd_arch_vax:
    *unknown = false;
    break;

  case bfd_arch_cris:
    if (machine == 0 || machine == 255)	arch_flags = M_CRIS;
    break;

  default:
    arch_flags = M_UNKNOWN;
  }

  if (arch_flags != M_UNKNOWN)
    *unknown = false;

  return arch_flags;
}

/*
FUNCTION
	aout_@var{size}_set_arch_mach

SYNOPSIS
	boolean aout_@var{size}_set_arch_mach,
	 (bfd *,
	  enum bfd_architecture arch,
	  unsigned long machine));

DESCRIPTION
	Set the architecture and the machine of the BFD @var{abfd} to the
	values @var{arch} and @var{machine}.  Verify that @var{abfd}'s format
	can support the architecture required.
*/

boolean
NAME(aout,set_arch_mach) (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  if (! bfd_default_set_arch_mach (abfd, arch, machine))
    return false;

  if (arch != bfd_arch_unknown)
    {
      boolean unknown;

      NAME(aout,machine_type) (arch, machine, &unknown);
      if (unknown)
	return false;
    }

  /* Determine the size of a relocation entry */
  switch (arch) {
  case bfd_arch_sparc:
  case bfd_arch_a29k:
  case bfd_arch_mips:
    obj_reloc_entry_size (abfd) = RELOC_EXT_SIZE;
    break;
  default:
    obj_reloc_entry_size (abfd) = RELOC_STD_SIZE;
    break;
  }

  return (*aout_backend_info(abfd)->set_sizes) (abfd);
}

static void
adjust_o_magic (abfd, execp)
     bfd *abfd;
     struct internal_exec *execp;
{
  file_ptr pos = adata (abfd).exec_bytes_size;
  bfd_vma vma = 0;
  int pad = 0;

  /* Text.  */
  obj_textsec(abfd)->filepos = pos;
  if (!obj_textsec(abfd)->user_set_vma)
    obj_textsec(abfd)->vma = vma;
  else
    vma = obj_textsec(abfd)->vma;

  pos += obj_textsec(abfd)->_raw_size;
  vma += obj_textsec(abfd)->_raw_size;

  /* Data.  */
  if (!obj_datasec(abfd)->user_set_vma)
    {
#if 0	    /* ?? Does alignment in the file image really matter? */
      pad = align_power (vma, obj_datasec(abfd)->alignment_power) - vma;
#endif
      obj_textsec(abfd)->_raw_size += pad;
      pos += pad;
      vma += pad;
      obj_datasec(abfd)->vma = vma;
    }
  else
    vma = obj_datasec(abfd)->vma;
  obj_datasec(abfd)->filepos = pos;
  pos += obj_datasec(abfd)->_raw_size;
  vma += obj_datasec(abfd)->_raw_size;

  /* BSS.  */
  if (!obj_bsssec(abfd)->user_set_vma)
    {
#if 0
      pad = align_power (vma, obj_bsssec(abfd)->alignment_power) - vma;
#endif
      obj_datasec(abfd)->_raw_size += pad;
      pos += pad;
      vma += pad;
      obj_bsssec(abfd)->vma = vma;
    }
  else
    {
      /* The VMA of the .bss section is set by the the VMA of the
         .data section plus the size of the .data section.  We may
         need to add padding bytes to make this true.  */
      pad = obj_bsssec (abfd)->vma - vma;
      if (pad > 0)
	{
	  obj_datasec (abfd)->_raw_size += pad;
	  pos += pad;
	}
    }
  obj_bsssec(abfd)->filepos = pos;

  /* Fix up the exec header.  */
  execp->a_text = obj_textsec(abfd)->_raw_size;
  execp->a_data = obj_datasec(abfd)->_raw_size;
  execp->a_bss = obj_bsssec(abfd)->_raw_size;
  N_SET_MAGIC (*execp, OMAGIC);
}

static void
adjust_z_magic (abfd, execp)
     bfd *abfd;
     struct internal_exec *execp;
{
  bfd_size_type data_pad, text_pad;
  file_ptr text_end;
  CONST struct aout_backend_data *abdp;
  int ztih;			/* Nonzero if text includes exec header.  */

  abdp = aout_backend_info (abfd);

  /* Text.  */
  ztih = (abdp != NULL
	  && (abdp->text_includes_header
	      || obj_aout_subformat (abfd) == q_magic_format));
  obj_textsec(abfd)->filepos = (ztih
				? adata(abfd).exec_bytes_size
				: adata(abfd).zmagic_disk_block_size);
  if (! obj_textsec(abfd)->user_set_vma)
    {
      /* ?? Do we really need to check for relocs here?  */
      obj_textsec(abfd)->vma = ((abfd->flags & HAS_RELOC)
				? 0
				: (ztih
				   ? (abdp->default_text_vma
				      + adata(abfd).exec_bytes_size)
				   : abdp->default_text_vma));
      text_pad = 0;
    }
  else
    {
      /* The .text section is being loaded at an unusual address.  We
         may need to pad it such that the .data section starts at a page
         boundary.  */
      if (ztih)
	text_pad = ((obj_textsec (abfd)->filepos - obj_textsec (abfd)->vma)
		    & (adata (abfd).page_size - 1));
      else
	text_pad = ((- obj_textsec (abfd)->vma)
		    & (adata (abfd).page_size - 1));
    }

  /* Find start of data.  */
  if (ztih)
    {
      text_end = obj_textsec (abfd)->filepos + obj_textsec (abfd)->_raw_size;
      text_pad += BFD_ALIGN (text_end, adata (abfd).page_size) - text_end;
    }
  else
    {
      /* Note that if page_size == zmagic_disk_block_size, then
	 filepos == page_size, and this case is the same as the ztih
	 case.  */
      text_end = obj_textsec (abfd)->_raw_size;
      text_pad += BFD_ALIGN (text_end, adata (abfd).page_size) - text_end;
      text_end += obj_textsec (abfd)->filepos;
    }
  obj_textsec(abfd)->_raw_size += text_pad;
  text_end += text_pad;

  /* Data.  */
  if (!obj_datasec(abfd)->user_set_vma)
    {
      bfd_vma vma;
      vma = obj_textsec(abfd)->vma + obj_textsec(abfd)->_raw_size;
      obj_datasec(abfd)->vma = BFD_ALIGN (vma, adata(abfd).segment_size);
    }
  if (abdp && abdp->zmagic_mapped_contiguous)
    {
      text_pad = (obj_datasec(abfd)->vma
		  - obj_textsec(abfd)->vma
		  - obj_textsec(abfd)->_raw_size);
      obj_textsec(abfd)->_raw_size += text_pad;
    }
  obj_datasec(abfd)->filepos = (obj_textsec(abfd)->filepos
				+ obj_textsec(abfd)->_raw_size);

  /* Fix up exec header while we're at it.  */
  execp->a_text = obj_textsec(abfd)->_raw_size;
  if (ztih && (!abdp || (abdp && !abdp->exec_header_not_counted)))
    execp->a_text += adata(abfd).exec_bytes_size;
  if (obj_aout_subformat (abfd) == q_magic_format)
    N_SET_MAGIC (*execp, QMAGIC);
  else
    N_SET_MAGIC (*execp, ZMAGIC);

  /* Spec says data section should be rounded up to page boundary.  */
  obj_datasec(abfd)->_raw_size
    = align_power (obj_datasec(abfd)->_raw_size,
		   obj_bsssec(abfd)->alignment_power);
  execp->a_data = BFD_ALIGN (obj_datasec(abfd)->_raw_size,
			     adata(abfd).page_size);
  data_pad = execp->a_data - obj_datasec(abfd)->_raw_size;

  /* BSS.  */
  if (!obj_bsssec(abfd)->user_set_vma)
    obj_bsssec(abfd)->vma = (obj_datasec(abfd)->vma
			     + obj_datasec(abfd)->_raw_size);
  /* If the BSS immediately follows the data section and extra space
     in the page is left after the data section, fudge data
     in the header so that the bss section looks smaller by that
     amount.  We'll start the bss section there, and lie to the OS.
     (Note that a linker script, as well as the above assignment,
     could have explicitly set the BSS vma to immediately follow
     the data section.)  */
  if (align_power (obj_bsssec(abfd)->vma, obj_bsssec(abfd)->alignment_power)
      == obj_datasec(abfd)->vma + obj_datasec(abfd)->_raw_size)
    execp->a_bss = (data_pad > obj_bsssec(abfd)->_raw_size) ? 0 :
      obj_bsssec(abfd)->_raw_size - data_pad;
  else
    execp->a_bss = obj_bsssec(abfd)->_raw_size;
}

static void
adjust_n_magic (abfd, execp)
     bfd *abfd;
     struct internal_exec *execp;
{
  file_ptr pos = adata(abfd).exec_bytes_size;
  bfd_vma vma = 0;
  int pad;

  /* Text.  */
  obj_textsec(abfd)->filepos = pos;
  if (!obj_textsec(abfd)->user_set_vma)
    obj_textsec(abfd)->vma = vma;
  else
    vma = obj_textsec(abfd)->vma;
  pos += obj_textsec(abfd)->_raw_size;
  vma += obj_textsec(abfd)->_raw_size;

  /* Data.  */
  obj_datasec(abfd)->filepos = pos;
  if (!obj_datasec(abfd)->user_set_vma)
    obj_datasec(abfd)->vma = BFD_ALIGN (vma, adata(abfd).segment_size);
  vma = obj_datasec(abfd)->vma;

  /* Since BSS follows data immediately, see if it needs alignment.  */
  vma += obj_datasec(abfd)->_raw_size;
  pad = align_power (vma, obj_bsssec(abfd)->alignment_power) - vma;
  obj_datasec(abfd)->_raw_size += pad;
  pos += obj_datasec(abfd)->_raw_size;

  /* BSS.  */
  if (!obj_bsssec(abfd)->user_set_vma)
    obj_bsssec(abfd)->vma = vma;
  else
    vma = obj_bsssec(abfd)->vma;

  /* Fix up exec header.  */
  execp->a_text = obj_textsec(abfd)->_raw_size;
  execp->a_data = obj_datasec(abfd)->_raw_size;
  execp->a_bss = obj_bsssec(abfd)->_raw_size;
  N_SET_MAGIC (*execp, NMAGIC);
}

boolean
NAME(aout,adjust_sizes_and_vmas) (abfd, text_size, text_end)
     bfd *abfd;
     bfd_size_type *text_size;
     file_ptr *text_end ATTRIBUTE_UNUSED;
{
  struct internal_exec *execp = exec_hdr (abfd);

  if (! NAME(aout,make_sections) (abfd))
    return false;

  if (adata(abfd).magic != undecided_magic)
    return true;

  obj_textsec(abfd)->_raw_size =
    align_power(obj_textsec(abfd)->_raw_size,
		obj_textsec(abfd)->alignment_power);

  *text_size = obj_textsec (abfd)->_raw_size;
  /* Rule (heuristic) for when to pad to a new page.  Note that there
     are (at least) two ways demand-paged (ZMAGIC) files have been
     handled.  Most Berkeley-based systems start the text segment at
     (TARGET_PAGE_SIZE).  However, newer versions of SUNOS start the text
     segment right after the exec header; the latter is counted in the
     text segment size, and is paged in by the kernel with the rest of
     the text.  */

  /* This perhaps isn't the right way to do this, but made it simpler for me
     to understand enough to implement it.  Better would probably be to go
     right from BFD flags to alignment/positioning characteristics.  But the
     old code was sloppy enough about handling the flags, and had enough
     other magic, that it was a little hard for me to understand.  I think
     I understand it better now, but I haven't time to do the cleanup this
     minute.  */

  if (abfd->flags & D_PAGED)
    /* Whether or not WP_TEXT is set -- let D_PAGED override.  */
    adata(abfd).magic = z_magic;
  else if (abfd->flags & WP_TEXT)
    adata(abfd).magic = n_magic;
  else
    adata(abfd).magic = o_magic;

#ifdef BFD_AOUT_DEBUG /* requires gcc2 */
#if __GNUC__ >= 2
  fprintf (stderr, "%s text=<%x,%x,%x> data=<%x,%x,%x> bss=<%x,%x,%x>\n",
	   ({ char *str;
	      switch (adata(abfd).magic) {
	      case n_magic: str = "NMAGIC"; break;
	      case o_magic: str = "OMAGIC"; break;
	      case z_magic: str = "ZMAGIC"; break;
	      default: abort ();
	      }
	      str;
	    }),
	   obj_textsec(abfd)->vma, obj_textsec(abfd)->_raw_size,
	   	obj_textsec(abfd)->alignment_power,
	   obj_datasec(abfd)->vma, obj_datasec(abfd)->_raw_size,
	   	obj_datasec(abfd)->alignment_power,
	   obj_bsssec(abfd)->vma, obj_bsssec(abfd)->_raw_size,
	   	obj_bsssec(abfd)->alignment_power);
#endif
#endif

  switch (adata(abfd).magic)
    {
    case o_magic:
      adjust_o_magic (abfd, execp);
      break;
    case z_magic:
      adjust_z_magic (abfd, execp);
      break;
    case n_magic:
      adjust_n_magic (abfd, execp);
      break;
    default:
      abort ();
    }

#ifdef BFD_AOUT_DEBUG
  fprintf (stderr, "       text=<%x,%x,%x> data=<%x,%x,%x> bss=<%x,%x>\n",
	   obj_textsec(abfd)->vma, obj_textsec(abfd)->_raw_size,
	   	obj_textsec(abfd)->filepos,
	   obj_datasec(abfd)->vma, obj_datasec(abfd)->_raw_size,
	   	obj_datasec(abfd)->filepos,
	   obj_bsssec(abfd)->vma, obj_bsssec(abfd)->_raw_size);
#endif

  return true;
}

/*
FUNCTION
	aout_@var{size}_new_section_hook

SYNOPSIS
        boolean aout_@var{size}_new_section_hook,
	   (bfd *abfd,
	    asection *newsect));

DESCRIPTION
	Called by the BFD in response to a @code{bfd_make_section}
	request.
*/
boolean
NAME(aout,new_section_hook) (abfd, newsect)
     bfd *abfd;
     asection *newsect;
{
  /* align to double at least */
  newsect->alignment_power = bfd_get_arch_info(abfd)->section_align_power;

  if (bfd_get_format (abfd) == bfd_object)
  {
    if (obj_textsec(abfd) == NULL && !strcmp(newsect->name, ".text")) {
	obj_textsec(abfd)= newsect;
	newsect->target_index = N_TEXT;
	return true;
      }

    if (obj_datasec(abfd) == NULL && !strcmp(newsect->name, ".data")) {
	obj_datasec(abfd) = newsect;
	newsect->target_index = N_DATA;
	return true;
      }

    if (obj_bsssec(abfd) == NULL && !strcmp(newsect->name, ".bss")) {
	obj_bsssec(abfd) = newsect;
	newsect->target_index = N_BSS;
	return true;
      }

  }

  /* We allow more than three sections internally */
  return true;
}

boolean
NAME(aout,set_section_contents) (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  file_ptr text_end;
  bfd_size_type text_size;

  if (! abfd->output_has_begun)
    {
      if (! NAME(aout,adjust_sizes_and_vmas) (abfd, &text_size, &text_end))
	return false;
    }

  if (section == obj_bsssec (abfd))
    {
      bfd_set_error (bfd_error_no_contents);
      return false;
    }

  if (section != obj_textsec (abfd)
      && section != obj_datasec (abfd))
    {
      (*_bfd_error_handler)
	(_("%s: can not represent section `%s' in a.out object file format"),
	 bfd_get_filename (abfd), bfd_get_section_name (abfd, section));
      bfd_set_error (bfd_error_nonrepresentable_section);
      return false;
    }

  if (count != 0)
    {
      if (bfd_seek (abfd, section->filepos + offset, SEEK_SET) != 0
	  || bfd_write (location, 1, count, abfd) != count)
	return false;
    }

  return true;
}

/* Read the external symbols from an a.out file.  */

static boolean
aout_get_external_symbols (abfd)
     bfd *abfd;
{
  if (obj_aout_external_syms (abfd) == (struct external_nlist *) NULL)
    {
      bfd_size_type count;
      struct external_nlist *syms;

      count = exec_hdr (abfd)->a_syms / EXTERNAL_NLIST_SIZE;

#ifdef USE_MMAP
      if (bfd_get_file_window (abfd,
			       obj_sym_filepos (abfd), exec_hdr (abfd)->a_syms,
			       &obj_aout_sym_window (abfd), true) == false)
	return false;
      syms = (struct external_nlist *) obj_aout_sym_window (abfd).data;
#else
      /* We allocate using malloc to make the values easy to free
	 later on.  If we put them on the objalloc it might not be
	 possible to free them.  */
      syms = ((struct external_nlist *)
	      bfd_malloc ((size_t) count * EXTERNAL_NLIST_SIZE));
      if (syms == (struct external_nlist *) NULL && count != 0)
	return false;

      if (bfd_seek (abfd, obj_sym_filepos (abfd), SEEK_SET) != 0
	  || (bfd_read (syms, 1, exec_hdr (abfd)->a_syms, abfd)
	      != exec_hdr (abfd)->a_syms))
	{
	  free (syms);
	  return false;
	}
#endif

      obj_aout_external_syms (abfd) = syms;
      obj_aout_external_sym_count (abfd) = count;
    }

  if (obj_aout_external_strings (abfd) == NULL
      && exec_hdr (abfd)->a_syms != 0)
    {
      unsigned char string_chars[BYTES_IN_WORD];
      bfd_size_type stringsize;
      char *strings;

      /* Get the size of the strings.  */
      if (bfd_seek (abfd, obj_str_filepos (abfd), SEEK_SET) != 0
	  || (bfd_read ((PTR) string_chars, BYTES_IN_WORD, 1, abfd)
	      != BYTES_IN_WORD))
	return false;
      stringsize = GET_WORD (abfd, string_chars);

#ifdef USE_MMAP
      if (bfd_get_file_window (abfd, obj_str_filepos (abfd), stringsize,
			       &obj_aout_string_window (abfd), true) == false)
	return false;
      strings = (char *) obj_aout_string_window (abfd).data;
#else
      strings = (char *) bfd_malloc ((size_t) stringsize + 1);
      if (strings == NULL)
	return false;

      /* Skip space for the string count in the buffer for convenience
	 when using indexes.  */
      if (bfd_read (strings + BYTES_IN_WORD, 1, stringsize - BYTES_IN_WORD,
		    abfd)
	  != stringsize - BYTES_IN_WORD)
	{
	  free (strings);
	  return false;
	}
#endif

      /* Ensure that a zero index yields an empty string.  */
      strings[0] = '\0';

      strings[stringsize - 1] = 0;

      obj_aout_external_strings (abfd) = strings;
      obj_aout_external_string_size (abfd) = stringsize;
    }

  return true;
}

/* Translate an a.out symbol into a BFD symbol.  The desc, other, type
   and symbol->value fields of CACHE_PTR will be set from the a.out
   nlist structure.  This function is responsible for setting
   symbol->flags and symbol->section, and adjusting symbol->value.  */

static boolean
translate_from_native_sym_flags (abfd, cache_ptr)
     bfd *abfd;
     aout_symbol_type *cache_ptr;
{
  flagword visible;

  if ((cache_ptr->type & N_STAB) != 0
      || cache_ptr->type == N_FN)
    {
      asection *sec;

      /* This is a debugging symbol.  */

      cache_ptr->symbol.flags = BSF_DEBUGGING;

      /* Work out the symbol section.  */
      switch (cache_ptr->type & N_TYPE)
	{
	case N_TEXT:
	case N_FN:
	  sec = obj_textsec (abfd);
	  break;
	case N_DATA:
	  sec = obj_datasec (abfd);
	  break;
	case N_BSS:
	  sec = obj_bsssec (abfd);
	  break;
	default:
	case N_ABS:
	  sec = bfd_abs_section_ptr;
	  break;
	}

      cache_ptr->symbol.section = sec;
      cache_ptr->symbol.value -= sec->vma;

      return true;
    }

  /* Get the default visibility.  This does not apply to all types, so
     we just hold it in a local variable to use if wanted.  */
  if ((cache_ptr->type & N_EXT) == 0)
    visible = BSF_LOCAL;
  else
    visible = BSF_GLOBAL;

  switch (cache_ptr->type)
    {
    default:
    case N_ABS: case N_ABS | N_EXT:
      cache_ptr->symbol.section = bfd_abs_section_ptr;
      cache_ptr->symbol.flags = visible;
      break;

    case N_UNDF | N_EXT:
      if (cache_ptr->symbol.value != 0)
	{
	  /* This is a common symbol.  */
	  cache_ptr->symbol.flags = BSF_GLOBAL;
	  cache_ptr->symbol.section = bfd_com_section_ptr;
	}
      else
	{
	  cache_ptr->symbol.flags = 0;
	  cache_ptr->symbol.section = bfd_und_section_ptr;
	}
      break;

    case N_TEXT: case N_TEXT | N_EXT:
      cache_ptr->symbol.section = obj_textsec (abfd);
      cache_ptr->symbol.value -= cache_ptr->symbol.section->vma;
      cache_ptr->symbol.flags = visible;
      break;

      /* N_SETV symbols used to represent set vectors placed in the
	 data section.  They are no longer generated.  Theoretically,
	 it was possible to extract the entries and combine them with
	 new ones, although I don't know if that was ever actually
	 done.  Unless that feature is restored, treat them as data
	 symbols.  */
    case N_SETV: case N_SETV | N_EXT:
    case N_DATA: case N_DATA | N_EXT:
      cache_ptr->symbol.section = obj_datasec (abfd);
      cache_ptr->symbol.value -= cache_ptr->symbol.section->vma;
      cache_ptr->symbol.flags = visible;
      break;

    case N_BSS: case N_BSS | N_EXT:
      cache_ptr->symbol.section = obj_bsssec (abfd);
      cache_ptr->symbol.value -= cache_ptr->symbol.section->vma;
      cache_ptr->symbol.flags = visible;
      break;

    case N_SETA: case N_SETA | N_EXT:
    case N_SETT: case N_SETT | N_EXT:
    case N_SETD: case N_SETD | N_EXT:
    case N_SETB: case N_SETB | N_EXT:
      {
	/* This code is no longer needed.  It used to be used to make
           the linker handle set symbols, but they are now handled in
           the add_symbols routine instead.  */
#if 0
	asection *section;
	arelent_chain *reloc;
	asection *into_section;

	/* This is a set symbol.  The name of the symbol is the name
	   of the set (e.g., __CTOR_LIST__).  The value of the symbol
	   is the value to add to the set.  We create a section with
	   the same name as the symbol, and add a reloc to insert the
	   appropriate value into the section.

	   This action is actually obsolete; it used to make the
	   linker do the right thing, but the linker no longer uses
	   this function.  */

	section = bfd_get_section_by_name (abfd, cache_ptr->symbol.name);
	if (section == NULL)
	  {
	    char *copy;

	    copy = bfd_alloc (abfd, strlen (cache_ptr->symbol.name) + 1);
	    if (copy == NULL)
	      return false;

	    strcpy (copy, cache_ptr->symbol.name);
	    section = bfd_make_section (abfd, copy);
	    if (section == NULL)
	      return false;
	  }

	reloc = (arelent_chain *) bfd_alloc (abfd, sizeof (arelent_chain));
	if (reloc == NULL)
	  return false;

	/* Build a relocation entry for the constructor.  */
	switch (cache_ptr->type & N_TYPE)
	  {
	  case N_SETA:
	    into_section = bfd_abs_section_ptr;
	    cache_ptr->type = N_ABS;
	    break;
	  case N_SETT:
	    into_section = obj_textsec (abfd);
	    cache_ptr->type = N_TEXT;
	    break;
	  case N_SETD:
	    into_section = obj_datasec (abfd);
	    cache_ptr->type = N_DATA;
	    break;
	  case N_SETB:
	    into_section = obj_bsssec (abfd);
	    cache_ptr->type = N_BSS;
	    break;
	  }

	/* Build a relocation pointing into the constructor section
	   pointing at the symbol in the set vector specified.  */
	reloc->relent.addend = cache_ptr->symbol.value;
	cache_ptr->symbol.section = into_section;
	reloc->relent.sym_ptr_ptr = into_section->symbol_ptr_ptr;

	/* We modify the symbol to belong to a section depending upon
	   the name of the symbol, and add to the size of the section
	   to contain a pointer to the symbol. Build a reloc entry to
	   relocate to this symbol attached to this section.  */
	section->flags = SEC_CONSTRUCTOR | SEC_RELOC;

	section->reloc_count++;
	section->alignment_power = 2;

	reloc->next = section->constructor_chain;
	section->constructor_chain = reloc;
	reloc->relent.address = section->_raw_size;
	section->_raw_size += BYTES_IN_WORD;

	reloc->relent.howto = CTOR_TABLE_RELOC_HOWTO(abfd);

#endif /* 0 */

	switch (cache_ptr->type & N_TYPE)
	  {
	  case N_SETA:
	    cache_ptr->symbol.section = bfd_abs_section_ptr;
	    break;
	  case N_SETT:
	    cache_ptr->symbol.section = obj_textsec (abfd);
	    break;
	  case N_SETD:
	    cache_ptr->symbol.section = obj_datasec (abfd);
	    break;
	  case N_SETB:
	    cache_ptr->symbol.section = obj_bsssec (abfd);
	    break;
	  }

	cache_ptr->symbol.flags |= BSF_CONSTRUCTOR;
      }
      break;

    case N_WARNING:
      /* This symbol is the text of a warning message.  The next
	 symbol is the symbol to associate the warning with.  If a
	 reference is made to that symbol, a warning is issued.  */
      cache_ptr->symbol.flags = BSF_DEBUGGING | BSF_WARNING;
      cache_ptr->symbol.section = bfd_abs_section_ptr;
      break;

    case N_INDR: case N_INDR | N_EXT:
      /* An indirect symbol.  This consists of two symbols in a row.
	 The first symbol is the name of the indirection.  The second
	 symbol is the name of the target.  A reference to the first
	 symbol becomes a reference to the second.  */
      cache_ptr->symbol.flags = BSF_DEBUGGING | BSF_INDIRECT | visible;
      cache_ptr->symbol.section = bfd_ind_section_ptr;
      break;

    case N_WEAKU:
      cache_ptr->symbol.section = bfd_und_section_ptr;
      cache_ptr->symbol.flags = BSF_WEAK;
      break;

    case N_WEAKA:
      cache_ptr->symbol.section = bfd_abs_section_ptr;
      cache_ptr->symbol.flags = BSF_WEAK;
      break;

    case N_WEAKT:
      cache_ptr->symbol.section = obj_textsec (abfd);
      cache_ptr->symbol.value -= cache_ptr->symbol.section->vma;
      cache_ptr->symbol.flags = BSF_WEAK;
      break;

    case N_WEAKD:
      cache_ptr->symbol.section = obj_datasec (abfd);
      cache_ptr->symbol.value -= cache_ptr->symbol.section->vma;
      cache_ptr->symbol.flags = BSF_WEAK;
      break;

    case N_WEAKB:
      cache_ptr->symbol.section = obj_bsssec (abfd);
      cache_ptr->symbol.value -= cache_ptr->symbol.section->vma;
      cache_ptr->symbol.flags = BSF_WEAK;
      break;
    }

  return true;
}

/* Set the fields of SYM_POINTER according to CACHE_PTR.  */

static boolean
translate_to_native_sym_flags (abfd, cache_ptr, sym_pointer)
     bfd *abfd;
     asymbol *cache_ptr;
     struct external_nlist *sym_pointer;
{
  bfd_vma value = cache_ptr->value;
  asection *sec;
  bfd_vma off;

  /* Mask out any existing type bits in case copying from one section
     to another.  */
  sym_pointer->e_type[0] &= ~N_TYPE;

  sec = bfd_get_section (cache_ptr);
  off = 0;

  if (sec == NULL)
    {
      /* This case occurs, e.g., for the *DEBUG* section of a COFF
	 file.  */
      (*_bfd_error_handler)
	(_("%s: can not represent section for symbol `%s' in a.out object file format"),
	 bfd_get_filename (abfd),
	 cache_ptr->name != NULL ? cache_ptr->name : _("*unknown*"));
      bfd_set_error (bfd_error_nonrepresentable_section);
      return false;
    }

  if (sec->output_section != NULL)
    {
      off = sec->output_offset;
      sec = sec->output_section;
    }

  if (bfd_is_abs_section (sec))
    sym_pointer->e_type[0] |= N_ABS;
  else if (sec == obj_textsec (abfd))
    sym_pointer->e_type[0] |= N_TEXT;
  else if (sec == obj_datasec (abfd))
    sym_pointer->e_type[0] |= N_DATA;
  else if (sec == obj_bsssec (abfd))
    sym_pointer->e_type[0] |= N_BSS;
  else if (bfd_is_und_section (sec))
    sym_pointer->e_type[0] = N_UNDF | N_EXT;
  else if (bfd_is_ind_section (sec))
    sym_pointer->e_type[0] = N_INDR;
  else if (bfd_is_com_section (sec))
    sym_pointer->e_type[0] = N_UNDF | N_EXT;
  else
    {
      (*_bfd_error_handler)
	(_("%s: can not represent section `%s' in a.out object file format"),
	 bfd_get_filename (abfd), bfd_get_section_name (abfd, sec));
      bfd_set_error (bfd_error_nonrepresentable_section);
      return false;
    }

  /* Turn the symbol from section relative to absolute again */
  value += sec->vma + off;

  if ((cache_ptr->flags & BSF_WARNING) != 0)
    sym_pointer->e_type[0] = N_WARNING;

  if ((cache_ptr->flags & BSF_DEBUGGING) != 0)
    sym_pointer->e_type[0] = ((aout_symbol_type *) cache_ptr)->type;
  else if ((cache_ptr->flags & BSF_GLOBAL) != 0)
    sym_pointer->e_type[0] |= N_EXT;
  else if ((cache_ptr->flags & BSF_LOCAL) != 0)
    sym_pointer->e_type[0] &= ~N_EXT;

  if ((cache_ptr->flags & BSF_CONSTRUCTOR) != 0)
    {
      int type = ((aout_symbol_type *) cache_ptr)->type;
      switch (type)
	{
	case N_ABS:	type = N_SETA; break;
	case N_TEXT:	type = N_SETT; break;
	case N_DATA:	type = N_SETD; break;
	case N_BSS:	type = N_SETB; break;
	}
      sym_pointer->e_type[0] = type;
    }

  if ((cache_ptr->flags & BSF_WEAK) != 0)
    {
      int type;

      switch (sym_pointer->e_type[0] & N_TYPE)
	{
	default:
	case N_ABS:	type = N_WEAKA; break;
	case N_TEXT:	type = N_WEAKT; break;
	case N_DATA:	type = N_WEAKD; break;
	case N_BSS:	type = N_WEAKB; break;
	case N_UNDF:	type = N_WEAKU; break;
	}
      sym_pointer->e_type[0] = type;
    }

  PUT_WORD(abfd, value, sym_pointer->e_value);

  return true;
}

/* Native-level interface to symbols.  */

asymbol *
NAME(aout,make_empty_symbol) (abfd)
     bfd *abfd;
{
  aout_symbol_type  *new =
    (aout_symbol_type *)bfd_zalloc (abfd, sizeof (aout_symbol_type));
  if (!new)
    return NULL;
  new->symbol.the_bfd = abfd;

  return &new->symbol;
}

/* Translate a set of internal symbols into external symbols.  */

boolean
NAME(aout,translate_symbol_table) (abfd, in, ext, count, str, strsize, dynamic)
     bfd *abfd;
     aout_symbol_type *in;
     struct external_nlist *ext;
     bfd_size_type count;
     char *str;
     bfd_size_type strsize;
     boolean dynamic;
{
  struct external_nlist *ext_end;

  ext_end = ext + count;
  for (; ext < ext_end; ext++, in++)
    {
      bfd_vma x;

      x = GET_WORD (abfd, ext->e_strx);
      in->symbol.the_bfd = abfd;

      /* For the normal symbols, the zero index points at the number
	 of bytes in the string table but is to be interpreted as the
	 null string.  For the dynamic symbols, the number of bytes in
	 the string table is stored in the __DYNAMIC structure and the
	 zero index points at an actual string.  */
      if (x == 0 && ! dynamic)
	in->symbol.name = "";
      else if (x < strsize)
	in->symbol.name = str + x;
      else
	return false;

      in->symbol.value = GET_SWORD (abfd,  ext->e_value);
      in->desc = bfd_h_get_16 (abfd, ext->e_desc);
      in->other = bfd_h_get_8 (abfd, ext->e_other);
      in->type = bfd_h_get_8 (abfd,  ext->e_type);
      in->symbol.udata.p = NULL;

      if (! translate_from_native_sym_flags (abfd, in))
	return false;

      if (dynamic)
	in->symbol.flags |= BSF_DYNAMIC;
    }

  return true;
}

/* We read the symbols into a buffer, which is discarded when this
   function exits.  We read the strings into a buffer large enough to
   hold them all plus all the cached symbol entries.  */

boolean
NAME(aout,slurp_symbol_table) (abfd)
     bfd *abfd;
{
  struct external_nlist *old_external_syms;
  aout_symbol_type *cached;
  size_t cached_size;

  /* If there's no work to be done, don't do any */
  if (obj_aout_symbols (abfd) != (aout_symbol_type *) NULL)
    return true;

  old_external_syms = obj_aout_external_syms (abfd);

  if (! aout_get_external_symbols (abfd))
    return false;

  cached_size = (obj_aout_external_sym_count (abfd)
		 * sizeof (aout_symbol_type));
  cached = (aout_symbol_type *) bfd_malloc (cached_size);
  if (cached == NULL && cached_size != 0)
    return false;
  if (cached_size != 0)
    memset (cached, 0, cached_size);

  /* Convert from external symbol information to internal.  */
  if (! (NAME(aout,translate_symbol_table)
	 (abfd, cached,
	  obj_aout_external_syms (abfd),
	  obj_aout_external_sym_count (abfd),
	  obj_aout_external_strings (abfd),
	  obj_aout_external_string_size (abfd),
	  false)))
    {
      free (cached);
      return false;
    }

  bfd_get_symcount (abfd) = obj_aout_external_sym_count (abfd);

  obj_aout_symbols (abfd) = cached;

  /* It is very likely that anybody who calls this function will not
     want the external symbol information, so if it was allocated
     because of our call to aout_get_external_symbols, we free it up
     right away to save space.  */
  if (old_external_syms == (struct external_nlist *) NULL
      && obj_aout_external_syms (abfd) != (struct external_nlist *) NULL)
    {
#ifdef USE_MMAP
      bfd_free_window (&obj_aout_sym_window (abfd));
#else
      free (obj_aout_external_syms (abfd));
#endif
      obj_aout_external_syms (abfd) = NULL;
    }

  return true;
}

/* We use a hash table when writing out symbols so that we only write
   out a particular string once.  This helps particularly when the
   linker writes out stabs debugging entries, because each different
   contributing object file tends to have many duplicate stabs
   strings.

   This hash table code breaks dbx on SunOS 4.1.3, so we don't do it
   if BFD_TRADITIONAL_FORMAT is set.  */

static bfd_size_type add_to_stringtab
  PARAMS ((bfd *, struct bfd_strtab_hash *, const char *, boolean));
static boolean emit_stringtab PARAMS ((bfd *, struct bfd_strtab_hash *));

/* Get the index of a string in a strtab, adding it if it is not
   already present.  */

static INLINE bfd_size_type
add_to_stringtab (abfd, tab, str, copy)
     bfd *abfd;
     struct bfd_strtab_hash *tab;
     const char *str;
     boolean copy;
{
  boolean hash;
  bfd_size_type index;

  /* An index of 0 always means the empty string.  */
  if (str == 0 || *str == '\0')
    return 0;

  /* Don't hash if BFD_TRADITIONAL_FORMAT is set, because SunOS dbx
     doesn't understand a hashed string table.  */
  hash = true;
  if ((abfd->flags & BFD_TRADITIONAL_FORMAT) != 0)
    hash = false;

  index = _bfd_stringtab_add (tab, str, hash, copy);

  if (index != (bfd_size_type) -1)
    {
      /* Add BYTES_IN_WORD to the return value to account for the
	 space taken up by the string table size.  */
      index += BYTES_IN_WORD;
    }

  return index;
}

/* Write out a strtab.  ABFD is already at the right location in the
   file.  */

static boolean
emit_stringtab (abfd, tab)
     register bfd *abfd;
     struct bfd_strtab_hash *tab;
{
  bfd_byte buffer[BYTES_IN_WORD];

  /* The string table starts with the size.  */
  PUT_WORD (abfd, _bfd_stringtab_size (tab) + BYTES_IN_WORD, buffer);
  if (bfd_write ((PTR) buffer, 1, BYTES_IN_WORD, abfd) != BYTES_IN_WORD)
    return false;

  return _bfd_stringtab_emit (abfd, tab);
}

boolean
NAME(aout,write_syms) (abfd)
     bfd *abfd;
{
  unsigned int count ;
  asymbol **generic = bfd_get_outsymbols (abfd);
  struct bfd_strtab_hash *strtab;

  strtab = _bfd_stringtab_init ();
  if (strtab == NULL)
    return false;

  for (count = 0; count < bfd_get_symcount (abfd); count++)
    {
      asymbol *g = generic[count];
      bfd_size_type indx;
      struct external_nlist nsp;

      indx = add_to_stringtab (abfd, strtab, g->name, false);
      if (indx == (bfd_size_type) -1)
	goto error_return;
      PUT_WORD (abfd, indx, (bfd_byte *) nsp.e_strx);

      if (bfd_asymbol_flavour(g) == abfd->xvec->flavour)
	{
	  bfd_h_put_16(abfd, aout_symbol(g)->desc,  nsp.e_desc);
	  bfd_h_put_8(abfd, aout_symbol(g)->other,  nsp.e_other);
	  bfd_h_put_8(abfd, aout_symbol(g)->type,  nsp.e_type);
	}
      else
	{
	  bfd_h_put_16(abfd,0, nsp.e_desc);
	  bfd_h_put_8(abfd, 0, nsp.e_other);
	  bfd_h_put_8(abfd, 0, nsp.e_type);
	}

      if (! translate_to_native_sym_flags (abfd, g, &nsp))
	goto error_return;

      if (bfd_write((PTR)&nsp,1,EXTERNAL_NLIST_SIZE, abfd)
	  != EXTERNAL_NLIST_SIZE)
	goto error_return;

      /* NB: `KEEPIT' currently overlays `udata.p', so set this only
	 here, at the end.  */
      g->KEEPIT = count;
    }

  if (! emit_stringtab (abfd, strtab))
    goto error_return;

  _bfd_stringtab_free (strtab);

  return true;

error_return:
  _bfd_stringtab_free (strtab);
  return false;
}

long
NAME(aout,get_symtab) (abfd, location)
     bfd *abfd;
     asymbol **location;
{
    unsigned int counter = 0;
    aout_symbol_type *symbase;

    if (!NAME(aout,slurp_symbol_table) (abfd))
      return -1;

    for (symbase = obj_aout_symbols(abfd); counter++ < bfd_get_symcount (abfd);)
      *(location++) = (asymbol *) ( symbase++);
    *location++ =0;
    return bfd_get_symcount (abfd);
}

/* Standard reloc stuff */
/* Output standard relocation information to a file in target byte order.  */

extern void  NAME(aout,swap_std_reloc_out)
  PARAMS ((bfd *, arelent *, struct reloc_std_external *));

void
NAME(aout,swap_std_reloc_out) (abfd, g, natptr)
     bfd *abfd;
     arelent *g;
     struct reloc_std_external *natptr;
{
  int r_index;
  asymbol *sym = *(g->sym_ptr_ptr);
  int r_extern;
  unsigned int r_length;
  int r_pcrel;
  int r_baserel, r_jmptable, r_relative;
  asection *output_section = sym->section->output_section;

  PUT_WORD(abfd, g->address, natptr->r_address);

  r_length = g->howto->size ;	/* Size as a power of two */
  r_pcrel  = (int) g->howto->pc_relative; /* Relative to PC? */
  /* XXX This relies on relocs coming from a.out files.  */
  r_baserel = (g->howto->type & 8) != 0;
  r_jmptable = (g->howto->type & 16) != 0;
  r_relative = (g->howto->type & 32) != 0;

#if 0
  /* For a standard reloc, the addend is in the object file.  */
  r_addend = g->addend + (*(g->sym_ptr_ptr))->section->output_section->vma;
#endif

  /* name was clobbered by aout_write_syms to be symbol index */

  /* If this relocation is relative to a symbol then set the
     r_index to the symbols index, and the r_extern bit.

     Absolute symbols can come in in two ways, either as an offset
     from the abs section, or as a symbol which has an abs value.
     check for that here
     */

  if (bfd_is_com_section (output_section)
      || bfd_is_abs_section (output_section)
      || bfd_is_und_section (output_section))
    {
      if (bfd_abs_section_ptr->symbol == sym)
      {
	/* Whoops, looked like an abs symbol, but is really an offset
	   from the abs section */
	r_index = N_ABS;
	r_extern = 0;
       }
      else
      {
	/* Fill in symbol */
	r_extern = 1;
	r_index = (*(g->sym_ptr_ptr))->KEEPIT;

      }
    }
  else
    {
      /* Just an ordinary section */
      r_extern = 0;
      r_index  = output_section->target_index;
    }

  /* now the fun stuff */
  if (bfd_header_big_endian (abfd)) {
      natptr->r_index[0] = r_index >> 16;
      natptr->r_index[1] = r_index >> 8;
      natptr->r_index[2] = r_index;
      natptr->r_type[0] =
       (r_extern?    RELOC_STD_BITS_EXTERN_BIG: 0)
	| (r_pcrel?     RELOC_STD_BITS_PCREL_BIG: 0)
	 | (r_baserel?   RELOC_STD_BITS_BASEREL_BIG: 0)
	  | (r_jmptable?  RELOC_STD_BITS_JMPTABLE_BIG: 0)
	   | (r_relative?  RELOC_STD_BITS_RELATIVE_BIG: 0)
	    | (r_length <<  RELOC_STD_BITS_LENGTH_SH_BIG);
    } else {
	natptr->r_index[2] = r_index >> 16;
	natptr->r_index[1] = r_index >> 8;
	natptr->r_index[0] = r_index;
	natptr->r_type[0] =
	 (r_extern?    RELOC_STD_BITS_EXTERN_LITTLE: 0)
	  | (r_pcrel?     RELOC_STD_BITS_PCREL_LITTLE: 0)
	   | (r_baserel?   RELOC_STD_BITS_BASEREL_LITTLE: 0)
	    | (r_jmptable?  RELOC_STD_BITS_JMPTABLE_LITTLE: 0)
	     | (r_relative?  RELOC_STD_BITS_RELATIVE_LITTLE: 0)
	      | (r_length <<  RELOC_STD_BITS_LENGTH_SH_LITTLE);
      }
}

/* Extended stuff */
/* Output extended relocation information to a file in target byte order.  */

extern void NAME(aout,swap_ext_reloc_out)
  PARAMS ((bfd *, arelent *, struct reloc_ext_external *));

void
NAME(aout,swap_ext_reloc_out) (abfd, g, natptr)
     bfd *abfd;
     arelent *g;
     register struct reloc_ext_external *natptr;
{
  int r_index;
  int r_extern;
  unsigned int r_type;
  unsigned int r_addend;
  asymbol *sym = *(g->sym_ptr_ptr);
  asection *output_section = sym->section->output_section;

  PUT_WORD (abfd, g->address, natptr->r_address);

  r_type = (unsigned int) g->howto->type;

  r_addend = g->addend;
  if ((sym->flags & BSF_SECTION_SYM) != 0)
    r_addend += (*(g->sym_ptr_ptr))->section->output_section->vma;

  /* If this relocation is relative to a symbol then set the
     r_index to the symbols index, and the r_extern bit.

     Absolute symbols can come in in two ways, either as an offset
     from the abs section, or as a symbol which has an abs value.
     check for that here.  */

  if (bfd_is_abs_section (bfd_get_section (sym)))
    {
      r_extern = 0;
      r_index = N_ABS;
    }
  else if ((sym->flags & BSF_SECTION_SYM) == 0)
    {
      if (bfd_is_und_section (bfd_get_section (sym))
	  || (sym->flags & BSF_GLOBAL) != 0)
	r_extern = 1;
      else
	r_extern = 0;
      r_index = (*(g->sym_ptr_ptr))->KEEPIT;
    }
  else
    {
      /* Just an ordinary section */
      r_extern = 0;
      r_index = output_section->target_index;
    }

  /* now the fun stuff */
  if (bfd_header_big_endian (abfd)) {
    natptr->r_index[0] = r_index >> 16;
    natptr->r_index[1] = r_index >> 8;
    natptr->r_index[2] = r_index;
    natptr->r_type[0] =
      ((r_extern? RELOC_EXT_BITS_EXTERN_BIG: 0)
       | (r_type << RELOC_EXT_BITS_TYPE_SH_BIG));
  } else {
    natptr->r_index[2] = r_index >> 16;
    natptr->r_index[1] = r_index >> 8;
    natptr->r_index[0] = r_index;
    natptr->r_type[0] =
     (r_extern? RELOC_EXT_BITS_EXTERN_LITTLE: 0)
      | (r_type << RELOC_EXT_BITS_TYPE_SH_LITTLE);
  }

  PUT_WORD (abfd, r_addend, natptr->r_addend);
}

/* BFD deals internally with all things based from the section they're
   in. so, something in 10 bytes into a text section  with a base of
   50 would have a symbol (.text+10) and know .text vma was 50.

   Aout keeps all it's symbols based from zero, so the symbol would
   contain 60. This macro subs the base of each section from the value
   to give the true offset from the section */

#define MOVE_ADDRESS(ad)       						\
  if (r_extern) {							\
   /* undefined symbol */						\
     cache_ptr->sym_ptr_ptr = symbols + r_index;			\
     cache_ptr->addend = ad;						\
     } else {								\
    /* defined, section relative. replace symbol with pointer to    	\
       symbol which points to section  */				\
    switch (r_index) {							\
    case N_TEXT:							\
    case N_TEXT | N_EXT:						\
      cache_ptr->sym_ptr_ptr  = obj_textsec(abfd)->symbol_ptr_ptr;	\
      cache_ptr->addend = ad  - su->textsec->vma;			\
      break;								\
    case N_DATA:							\
    case N_DATA | N_EXT:						\
      cache_ptr->sym_ptr_ptr  = obj_datasec(abfd)->symbol_ptr_ptr;	\
      cache_ptr->addend = ad - su->datasec->vma;			\
      break;								\
    case N_BSS:								\
    case N_BSS | N_EXT:							\
      cache_ptr->sym_ptr_ptr  = obj_bsssec(abfd)->symbol_ptr_ptr;	\
      cache_ptr->addend = ad - su->bsssec->vma;				\
      break;								\
    default:								\
    case N_ABS:								\
    case N_ABS | N_EXT:							\
     cache_ptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;	\
      cache_ptr->addend = ad;						\
      break;								\
    }									\
  }     								\

void
NAME(aout,swap_ext_reloc_in) (abfd, bytes, cache_ptr, symbols, symcount)
     bfd *abfd;
     struct reloc_ext_external *bytes;
     arelent *cache_ptr;
     asymbol **symbols;
     bfd_size_type symcount;
{
  unsigned int r_index;
  int r_extern;
  unsigned int r_type;
  struct aoutdata *su = &(abfd->tdata.aout_data->a);

  cache_ptr->address = (GET_SWORD (abfd, bytes->r_address));

  /* now the fun stuff */
  if (bfd_header_big_endian (abfd)) {
    r_index =  (bytes->r_index[0] << 16)
	     | (bytes->r_index[1] << 8)
	     |  bytes->r_index[2];
    r_extern = (0 != (bytes->r_type[0] & RELOC_EXT_BITS_EXTERN_BIG));
    r_type   =       (bytes->r_type[0] & RELOC_EXT_BITS_TYPE_BIG)
				      >> RELOC_EXT_BITS_TYPE_SH_BIG;
  } else {
    r_index =  (bytes->r_index[2] << 16)
	     | (bytes->r_index[1] << 8)
	     |  bytes->r_index[0];
    r_extern = (0 != (bytes->r_type[0] & RELOC_EXT_BITS_EXTERN_LITTLE));
    r_type   =       (bytes->r_type[0] & RELOC_EXT_BITS_TYPE_LITTLE)
				      >> RELOC_EXT_BITS_TYPE_SH_LITTLE;
  }

  cache_ptr->howto =  howto_table_ext + r_type;

  /* Base relative relocs are always against the symbol table,
     regardless of the setting of r_extern.  r_extern just reflects
     whether the symbol the reloc is against is local or global.  */
  if (r_type == RELOC_BASE10
      || r_type == RELOC_BASE13
      || r_type == RELOC_BASE22)
    r_extern = 1;

  if (r_extern && r_index > symcount)
    {
      /* We could arrange to return an error, but it might be useful
         to see the file even if it is bad.  */
      r_extern = 0;
      r_index = N_ABS;
    }

  MOVE_ADDRESS(GET_SWORD(abfd, bytes->r_addend));
}

void
NAME(aout,swap_std_reloc_in) (abfd, bytes, cache_ptr, symbols, symcount)
     bfd *abfd;
     struct reloc_std_external *bytes;
     arelent *cache_ptr;
     asymbol **symbols;
     bfd_size_type symcount;
{
  unsigned int r_index;
  int r_extern;
  unsigned int r_length;
  int r_pcrel;
  int r_baserel, r_jmptable, r_relative;
  struct aoutdata  *su = &(abfd->tdata.aout_data->a);
  unsigned int howto_idx;

  cache_ptr->address = bfd_h_get_32 (abfd, bytes->r_address);

  /* now the fun stuff */
  if (bfd_header_big_endian (abfd)) {
    r_index =  (bytes->r_index[0] << 16)
      | (bytes->r_index[1] << 8)
	|  bytes->r_index[2];
    r_extern  = (0 != (bytes->r_type[0] & RELOC_STD_BITS_EXTERN_BIG));
    r_pcrel   = (0 != (bytes->r_type[0] & RELOC_STD_BITS_PCREL_BIG));
    r_baserel = (0 != (bytes->r_type[0] & RELOC_STD_BITS_BASEREL_BIG));
    r_jmptable= (0 != (bytes->r_type[0] & RELOC_STD_BITS_JMPTABLE_BIG));
    r_relative= (0 != (bytes->r_type[0] & RELOC_STD_BITS_RELATIVE_BIG));
    r_length  =       (bytes->r_type[0] & RELOC_STD_BITS_LENGTH_BIG)
      			>> RELOC_STD_BITS_LENGTH_SH_BIG;
  } else {
    r_index =  (bytes->r_index[2] << 16)
      | (bytes->r_index[1] << 8)
	|  bytes->r_index[0];
    r_extern  = (0 != (bytes->r_type[0] & RELOC_STD_BITS_EXTERN_LITTLE));
    r_pcrel   = (0 != (bytes->r_type[0] & RELOC_STD_BITS_PCREL_LITTLE));
    r_baserel = (0 != (bytes->r_type[0] & RELOC_STD_BITS_BASEREL_LITTLE));
    r_jmptable= (0 != (bytes->r_type[0] & RELOC_STD_BITS_JMPTABLE_LITTLE));
    r_relative= (0 != (bytes->r_type[0] & RELOC_STD_BITS_RELATIVE_LITTLE));
    r_length  =       (bytes->r_type[0] & RELOC_STD_BITS_LENGTH_LITTLE)
      			>> RELOC_STD_BITS_LENGTH_SH_LITTLE;
  }

  howto_idx = r_length + 4 * r_pcrel + 8 * r_baserel
	      + 16 * r_jmptable + 32 * r_relative;
  BFD_ASSERT (howto_idx < TABLE_SIZE (howto_table_std));
  cache_ptr->howto =  howto_table_std + howto_idx;
  BFD_ASSERT (cache_ptr->howto->type != (unsigned int) -1);

  /* Base relative relocs are always against the symbol table,
     regardless of the setting of r_extern.  r_extern just reflects
     whether the symbol the reloc is against is local or global.  */
  if (r_baserel)
    r_extern = 1;

  if (r_extern && r_index > symcount)
    {
      /* We could arrange to return an error, but it might be useful
         to see the file even if it is bad.  */
      r_extern = 0;
      r_index = N_ABS;
    }

  MOVE_ADDRESS(0);
}

/* Read and swap the relocs for a section.  */

boolean
NAME(aout,slurp_reloc_table) (abfd, asect, symbols)
     bfd *abfd;
     sec_ptr asect;
     asymbol **symbols;
{
  unsigned int count;
  bfd_size_type reloc_size;
  PTR relocs;
  arelent *reloc_cache;
  size_t each_size;
  unsigned int counter = 0;
  arelent *cache_ptr;

  if (asect->relocation)
    return true;

  if (asect->flags & SEC_CONSTRUCTOR)
    return true;

  if (asect == obj_datasec (abfd))
    reloc_size = exec_hdr(abfd)->a_drsize;
  else if (asect == obj_textsec (abfd))
    reloc_size = exec_hdr(abfd)->a_trsize;
  else if (asect == obj_bsssec (abfd))
    reloc_size = 0;
  else
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  if (bfd_seek (abfd, asect->rel_filepos, SEEK_SET) != 0)
    return false;

  each_size = obj_reloc_entry_size (abfd);

  count = reloc_size / each_size;

  reloc_cache = (arelent *) bfd_malloc ((size_t) (count * sizeof (arelent)));
  if (reloc_cache == NULL && count != 0)
    return false;
  memset (reloc_cache, 0, count * sizeof (arelent));

  relocs = bfd_malloc ((size_t) reloc_size);
  if (relocs == NULL && reloc_size != 0)
    {
      free (reloc_cache);
      return false;
    }

  if (bfd_read (relocs, 1, reloc_size, abfd) != reloc_size)
    {
      free (relocs);
      free (reloc_cache);
      return false;
    }

  cache_ptr = reloc_cache;
  if (each_size == RELOC_EXT_SIZE)
    {
      register struct reloc_ext_external *rptr =
	(struct reloc_ext_external *) relocs;

      for (; counter < count; counter++, rptr++, cache_ptr++)
	MY_swap_ext_reloc_in (abfd, rptr, cache_ptr, symbols,
			      bfd_get_symcount (abfd));
    }
  else
    {
      register struct reloc_std_external *rptr =
	(struct reloc_std_external *) relocs;

      for (; counter < count; counter++, rptr++, cache_ptr++)
	MY_swap_std_reloc_in (abfd, rptr, cache_ptr, symbols,
			      bfd_get_symcount (abfd));
    }

  free (relocs);

  asect->relocation = reloc_cache;
  asect->reloc_count = cache_ptr - reloc_cache;

  return true;
}

/* Write out a relocation section into an object file.  */

boolean
NAME(aout,squirt_out_relocs) (abfd, section)
     bfd *abfd;
     asection *section;
{
  arelent **generic;
  unsigned char *native, *natptr;
  size_t each_size;

  unsigned int count = section->reloc_count;
  size_t natsize;

  if (count == 0 || section->orelocation == NULL)
    return true;

  each_size = obj_reloc_entry_size (abfd);
  natsize = each_size * count;
  native = (unsigned char *) bfd_zalloc (abfd, natsize);
  if (!native)
    return false;

  generic = section->orelocation;

  if (each_size == RELOC_EXT_SIZE)
    {
      for (natptr = native;
	   count != 0;
	   --count, natptr += each_size, ++generic)
	MY_swap_ext_reloc_out (abfd, *generic,
			       (struct reloc_ext_external *) natptr);
    }
  else
    {
      for (natptr = native;
	   count != 0;
	   --count, natptr += each_size, ++generic)
	MY_swap_std_reloc_out(abfd, *generic, (struct reloc_std_external *)natptr);
    }

  if ( bfd_write ((PTR) native, 1, natsize, abfd) != natsize) {
    bfd_release(abfd, native);
    return false;
  }
  bfd_release (abfd, native);

  return true;
}

/* This is stupid.  This function should be a boolean predicate */
long
NAME(aout,canonicalize_reloc) (abfd, section, relptr, symbols)
     bfd *abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
  arelent *tblptr = section->relocation;
  unsigned int count;

  if (section == obj_bsssec (abfd))
    {
      *relptr = NULL;
      return 0;
    }

  if (!(tblptr || NAME(aout,slurp_reloc_table) (abfd, section, symbols)))
    return -1;

  if (section->flags & SEC_CONSTRUCTOR) {
    arelent_chain *chain = section->constructor_chain;
    for (count = 0; count < section->reloc_count; count ++) {
      *relptr ++ = &chain->relent;
      chain = chain->next;
    }
  }
  else {
    tblptr = section->relocation;

    for (count = 0; count++ < section->reloc_count;)
      {
	*relptr++ = tblptr++;
      }
  }
  *relptr = 0;

  return section->reloc_count;
}

long
NAME(aout,get_reloc_upper_bound) (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  if (bfd_get_format (abfd) != bfd_object) {
    bfd_set_error (bfd_error_invalid_operation);
    return -1;
  }
  if (asect->flags & SEC_CONSTRUCTOR) {
    return (sizeof (arelent *) * (asect->reloc_count+1));
  }

  if (asect == obj_datasec (abfd))
    return (sizeof (arelent *)
	    * ((exec_hdr(abfd)->a_drsize / obj_reloc_entry_size (abfd))
	       + 1));

  if (asect == obj_textsec (abfd))
    return (sizeof (arelent *)
	    * ((exec_hdr(abfd)->a_trsize / obj_reloc_entry_size (abfd))
	       + 1));

  if (asect == obj_bsssec (abfd))
    return sizeof (arelent *);

  if (asect == obj_bsssec (abfd))
    return 0;

  bfd_set_error (bfd_error_invalid_operation);
  return -1;
}

long
NAME(aout,get_symtab_upper_bound) (abfd)
     bfd *abfd;
{
  if (!NAME(aout,slurp_symbol_table) (abfd))
    return -1;

  return (bfd_get_symcount (abfd)+1) * (sizeof (aout_symbol_type *));
}

 alent *
NAME(aout,get_lineno) (ignore_abfd, ignore_symbol)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *ignore_symbol ATTRIBUTE_UNUSED;
{
  return (alent *)NULL;
}

void
NAME(aout,get_symbol_info) (ignore_abfd, symbol, ret)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);

  if (ret->type == '?')
    {
      int type_code = aout_symbol(symbol)->type & 0xff;
      const char *stab_name = bfd_get_stab_name (type_code);
      static char buf[10];

      if (stab_name == NULL)
	{
	  sprintf (buf, "(%d)", type_code);
	  stab_name = buf;
	}
      ret->type = '-';
      ret->stab_type = type_code;
      ret->stab_other = (unsigned) (aout_symbol(symbol)->other & 0xff);
      ret->stab_desc = (unsigned) (aout_symbol(symbol)->desc & 0xffff);
      ret->stab_name = stab_name;
    }
}

void
NAME(aout,print_symbol) (ignore_abfd, afile, symbol, how)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     PTR afile;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *)afile;

  switch (how) {
  case bfd_print_symbol_name:
    if (symbol->name)
      fprintf (file,"%s", symbol->name);
    break;
  case bfd_print_symbol_more:
    fprintf (file,"%4x %2x %2x",(unsigned) (aout_symbol(symbol)->desc & 0xffff),
	    (unsigned) (aout_symbol(symbol)->other & 0xff),
	    (unsigned) (aout_symbol(symbol)->type));
    break;
  case bfd_print_symbol_all:
    {
   CONST char *section_name = symbol->section->name;

      bfd_print_symbol_vandf((PTR)file,symbol);

      fprintf (file," %-5s %04x %02x %02x",
	      section_name,
	      (unsigned) (aout_symbol(symbol)->desc & 0xffff),
	      (unsigned) (aout_symbol(symbol)->other & 0xff),
	      (unsigned) (aout_symbol(symbol)->type  & 0xff));
      if (symbol->name)
        fprintf (file," %s", symbol->name);
    }
    break;
  }
}

/* If we don't have to allocate more than 1MB to hold the generic
   symbols, we use the generic minisymbol methord: it's faster, since
   it only translates the symbols once, not multiple times.  */
#define MINISYM_THRESHOLD (1000000 / sizeof (asymbol))

/* Read minisymbols.  For minisymbols, we use the unmodified a.out
   symbols.  The minisymbol_to_symbol function translates these into
   BFD asymbol structures.  */

long
NAME(aout,read_minisymbols) (abfd, dynamic, minisymsp, sizep)
     bfd *abfd;
     boolean dynamic;
     PTR *minisymsp;
     unsigned int *sizep;
{
  if (dynamic)
    {
      /* We could handle the dynamic symbols here as well, but it's
         easier to hand them off.  */
      return _bfd_generic_read_minisymbols (abfd, dynamic, minisymsp, sizep);
    }

  if (! aout_get_external_symbols (abfd))
    return -1;

  if (obj_aout_external_sym_count (abfd) < MINISYM_THRESHOLD)
    return _bfd_generic_read_minisymbols (abfd, dynamic, minisymsp, sizep);

  *minisymsp = (PTR) obj_aout_external_syms (abfd);

  /* By passing the external symbols back from this routine, we are
     giving up control over the memory block.  Clear
     obj_aout_external_syms, so that we do not try to free it
     ourselves.  */
  obj_aout_external_syms (abfd) = NULL;

  *sizep = EXTERNAL_NLIST_SIZE;
  return obj_aout_external_sym_count (abfd);
}

/* Convert a minisymbol to a BFD asymbol.  A minisymbol is just an
   unmodified a.out symbol.  The SYM argument is a structure returned
   by bfd_make_empty_symbol, which we fill in here.  */

asymbol *
NAME(aout,minisymbol_to_symbol) (abfd, dynamic, minisym, sym)
     bfd *abfd;
     boolean dynamic;
     const PTR minisym;
     asymbol *sym;
{
  if (dynamic
      || obj_aout_external_sym_count (abfd) < MINISYM_THRESHOLD)
    return _bfd_generic_minisymbol_to_symbol (abfd, dynamic, minisym, sym);

  memset (sym, 0, sizeof (aout_symbol_type));

  /* We call translate_symbol_table to translate a single symbol.  */
  if (! (NAME(aout,translate_symbol_table)
	 (abfd,
	  (aout_symbol_type *) sym,
	  (struct external_nlist *) minisym,
	  (bfd_size_type) 1,
	  obj_aout_external_strings (abfd),
	  obj_aout_external_string_size (abfd),
	  false)))
    return NULL;

  return sym;
}

/*
 provided a BFD, a section and an offset into the section, calculate
 and return the name of the source file and the line nearest to the
 wanted location.
*/

boolean
NAME(aout,find_nearest_line)
     (abfd, section, symbols, offset, filename_ptr, functionname_ptr, line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     CONST char **filename_ptr;
     CONST char **functionname_ptr;
     unsigned int *line_ptr;
{
  /* Run down the file looking for the filename, function and linenumber */
  asymbol **p;
  CONST char *directory_name = NULL;
  CONST char *main_file_name = NULL;
  CONST char *current_file_name = NULL;
  CONST char *line_file_name = NULL; /* Value of current_file_name at line number.  */
  CONST char *line_directory_name = NULL; /* Value of directory_name at line number.  */
  bfd_vma low_line_vma = 0;
  bfd_vma low_func_vma = 0;
  asymbol *func = 0;
  size_t filelen, funclen;
  char *buf;

  *filename_ptr = abfd->filename;
  *functionname_ptr = 0;
  *line_ptr = 0;
  if (symbols != (asymbol **)NULL) {
    for (p = symbols; *p; p++) {
      aout_symbol_type  *q = (aout_symbol_type *) (*p);
    next:
      switch (q->type){
      case N_TEXT:
	/* If this looks like a file name symbol, and it comes after
           the line number we have found so far, but before the
           offset, then we have probably not found the right line
           number.  */
	if (q->symbol.value <= offset
	    && ((q->symbol.value > low_line_vma
		 && (line_file_name != NULL
		     || *line_ptr != 0))
		|| (q->symbol.value > low_func_vma
		    && func != NULL)))
	  {
	    const char *symname;

	    symname = q->symbol.name;
	    if (strcmp (symname + strlen (symname) - 2, ".o") == 0)
	      {
		if (q->symbol.value > low_line_vma)
		  {
		    *line_ptr = 0;
		    line_file_name = NULL;
		  }
		if (q->symbol.value > low_func_vma)
		  func = NULL;
	      }
	  }
	break;

      case N_SO:
	/* If this symbol is less than the offset, but greater than
           the line number we have found so far, then we have not
           found the right line number.  */
	if (q->symbol.value <= offset)
	  {
	    if (q->symbol.value > low_line_vma)
	      {
		*line_ptr = 0;
		line_file_name = NULL;
	      }
	    if (q->symbol.value > low_func_vma)
	      func = NULL;
	  }

	main_file_name = current_file_name = q->symbol.name;
	/* Look ahead to next symbol to check if that too is an N_SO.  */
	p++;
	if (*p == NULL)
	  break;
	q = (aout_symbol_type *) (*p);
	if (q->type != (int)N_SO)
	  goto next;

	/* Found a second N_SO  First is directory; second is filename.  */
	directory_name = current_file_name;
	main_file_name = current_file_name = q->symbol.name;
	if (obj_textsec(abfd) != section)
	  goto done;
	break;
      case N_SOL:
	current_file_name = q->symbol.name;
	break;

      case N_SLINE:

      case N_DSLINE:
      case N_BSLINE:
	/* We'll keep this if it resolves nearer than the one we have
           already.  */
	if (q->symbol.value >= low_line_vma
	    && q->symbol.value <= offset)
	  {
	    *line_ptr = q->desc;
	    low_line_vma = q->symbol.value;
	    line_file_name = current_file_name;
	    line_directory_name = directory_name;
	  }
	break;
      case N_FUN:
	{
	  /* We'll keep this if it is nearer than the one we have already */
	  if (q->symbol.value >= low_func_vma &&
	      q->symbol.value <= offset) {
	    low_func_vma = q->symbol.value;
	    func = (asymbol *)q;
	  }
	  else if (q->symbol.value > offset)
	    goto done;
	}
	break;
      }
    }
  }

 done:
  if (*line_ptr != 0)
    {
      main_file_name = line_file_name;
      directory_name = line_directory_name;
    }

  if (main_file_name == NULL
      || IS_ABSOLUTE_PATH (main_file_name)
      || directory_name == NULL)
    filelen = 0;
  else
    filelen = strlen (directory_name) + strlen (main_file_name);
  if (func == NULL)
    funclen = 0;
  else
    funclen = strlen (bfd_asymbol_name (func));

  if (adata (abfd).line_buf != NULL)
    free (adata (abfd).line_buf);
  if (filelen + funclen == 0)
    adata (abfd).line_buf = buf = NULL;
  else
    {
      buf = (char *) bfd_malloc (filelen + funclen + 3);
      adata (abfd).line_buf = buf;
      if (buf == NULL)
	return false;
    }

  if (main_file_name != NULL)
    {
      if (IS_ABSOLUTE_PATH (main_file_name) || directory_name == NULL)
	*filename_ptr = main_file_name;
      else
	{
	  sprintf (buf, "%s%s", directory_name, main_file_name);
	  *filename_ptr = buf;
	  buf += filelen + 1;
	}
    }

  if (func)
    {
      const char *function = func->name;
      char *p;

      /* The caller expects a symbol name.  We actually have a
	 function name, without the leading underscore.  Put the
	 underscore back in, so that the caller gets a symbol name.  */
      if (bfd_get_symbol_leading_char (abfd) == '\0')
	strcpy (buf, function);
      else
	{
	  buf[0] = bfd_get_symbol_leading_char (abfd);
	  strcpy (buf + 1, function);
	}
      /* Have to remove : stuff */
      p = strchr (buf, ':');
      if (p != NULL)
	*p = '\0';
      *functionname_ptr = buf;
    }

  return true;
}

int
NAME(aout,sizeof_headers) (abfd, execable)
     bfd *abfd;
     boolean execable ATTRIBUTE_UNUSED;
{
  return adata(abfd).exec_bytes_size;
}

/* Free all information we have cached for this BFD.  We can always
   read it again later if we need it.  */

boolean
NAME(aout,bfd_free_cached_info) (abfd)
     bfd *abfd;
{
  asection *o;

  if (bfd_get_format (abfd) != bfd_object)
    return true;

#define BFCI_FREE(x) if (x != NULL) { free (x); x = NULL; }
  BFCI_FREE (obj_aout_symbols (abfd));
#ifdef USE_MMAP
  obj_aout_external_syms (abfd) = 0;
  bfd_free_window (&obj_aout_sym_window (abfd));
  bfd_free_window (&obj_aout_string_window (abfd));
  obj_aout_external_strings (abfd) = 0;
#else
  BFCI_FREE (obj_aout_external_syms (abfd));
  BFCI_FREE (obj_aout_external_strings (abfd));
#endif
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    BFCI_FREE (o->relocation);
#undef BFCI_FREE

  return true;
}

/* a.out link code.  */

static boolean aout_link_add_object_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean aout_link_check_archive_element
  PARAMS ((bfd *, struct bfd_link_info *, boolean *));
static boolean aout_link_free_symbols PARAMS ((bfd *));
static boolean aout_link_check_ar_symbols
  PARAMS ((bfd *, struct bfd_link_info *, boolean *pneeded));
static boolean aout_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));

/* Routine to create an entry in an a.out link hash table.  */

struct bfd_hash_entry *
NAME(aout,link_hash_newfunc) (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct aout_link_hash_entry *ret = (struct aout_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct aout_link_hash_entry *) NULL)
    ret = ((struct aout_link_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct aout_link_hash_entry)));
  if (ret == (struct aout_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct aout_link_hash_entry *)
	 _bfd_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				 table, string));
  if (ret)
    {
      /* Set local fields.  */
      ret->written = false;
      ret->indx = -1;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Initialize an a.out link hash table.  */

boolean
NAME(aout,link_hash_table_init) (table, abfd, newfunc)
     struct aout_link_hash_table *table;
     bfd *abfd;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
{
  return _bfd_link_hash_table_init (&table->root, abfd, newfunc);
}

/* Create an a.out link hash table.  */

struct bfd_link_hash_table *
NAME(aout,link_hash_table_create) (abfd)
     bfd *abfd;
{
  struct aout_link_hash_table *ret;

  ret = ((struct aout_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct aout_link_hash_table)));
  if (ret == NULL)
    return (struct bfd_link_hash_table *) NULL;
  if (! NAME(aout,link_hash_table_init) (ret, abfd,
					 NAME(aout,link_hash_newfunc)))
    {
      free (ret);
      return (struct bfd_link_hash_table *) NULL;
    }
  return &ret->root;
}

/* Given an a.out BFD, add symbols to the global hash table as
   appropriate.  */

boolean
NAME(aout,link_add_symbols) (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  switch (bfd_get_format (abfd))
    {
    case bfd_object:
      return aout_link_add_object_symbols (abfd, info);
    case bfd_archive:
      return _bfd_generic_link_add_archive_symbols
	(abfd, info, aout_link_check_archive_element);
    default:
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }
}

/* Add symbols from an a.out object file.  */

static boolean
aout_link_add_object_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  if (! aout_get_external_symbols (abfd))
    return false;
  if (! aout_link_add_symbols (abfd, info))
    return false;
  if (! info->keep_memory)
    {
      if (! aout_link_free_symbols (abfd))
	return false;
    }
  return true;
}

/* Check a single archive element to see if we need to include it in
   the link.  *PNEEDED is set according to whether this element is
   needed in the link or not.  This is called from
   _bfd_generic_link_add_archive_symbols.  */

static boolean
aout_link_check_archive_element (abfd, info, pneeded)
     bfd *abfd;
     struct bfd_link_info *info;
     boolean *pneeded;
{
  if (! aout_get_external_symbols (abfd))
    return false;

  if (! aout_link_check_ar_symbols (abfd, info, pneeded))
    return false;

  if (*pneeded)
    {
      if (! aout_link_add_symbols (abfd, info))
	return false;
    }

  if (! info->keep_memory || ! *pneeded)
    {
      if (! aout_link_free_symbols (abfd))
	return false;
    }

  return true;
}

/* Free up the internal symbols read from an a.out file.  */

static boolean
aout_link_free_symbols (abfd)
     bfd *abfd;
{
  if (obj_aout_external_syms (abfd) != (struct external_nlist *) NULL)
    {
#ifdef USE_MMAP
      bfd_free_window (&obj_aout_sym_window (abfd));
#else
      free ((PTR) obj_aout_external_syms (abfd));
#endif
      obj_aout_external_syms (abfd) = (struct external_nlist *) NULL;
    }
  if (obj_aout_external_strings (abfd) != (char *) NULL)
    {
#ifdef USE_MMAP
      bfd_free_window (&obj_aout_string_window (abfd));
#else
      free ((PTR) obj_aout_external_strings (abfd));
#endif
      obj_aout_external_strings (abfd) = (char *) NULL;
    }
  return true;
}

/* Look through the internal symbols to see if this object file should
   be included in the link.  We should include this object file if it
   defines any symbols which are currently undefined.  If this object
   file defines a common symbol, then we may adjust the size of the
   known symbol but we do not include the object file in the link
   (unless there is some other reason to include it).  */

static boolean
aout_link_check_ar_symbols (abfd, info, pneeded)
     bfd *abfd;
     struct bfd_link_info *info;
     boolean *pneeded;
{
  register struct external_nlist *p;
  struct external_nlist *pend;
  char *strings;

  *pneeded = false;

  /* Look through all the symbols.  */
  p = obj_aout_external_syms (abfd);
  pend = p + obj_aout_external_sym_count (abfd);
  strings = obj_aout_external_strings (abfd);
  for (; p < pend; p++)
    {
      int type = bfd_h_get_8 (abfd, p->e_type);
      const char *name;
      struct bfd_link_hash_entry *h;

      /* Ignore symbols that are not externally visible.  This is an
	 optimization only, as we check the type more thoroughly
	 below.  */
      if (((type & N_EXT) == 0
	   || (type & N_STAB) != 0
	   || type == N_FN)
	  && type != N_WEAKA
	  && type != N_WEAKT
	  && type != N_WEAKD
	  && type != N_WEAKB)
	{
	  if (type == N_WARNING
	      || type == N_INDR)
	    ++p;
	  continue;
	}

      name = strings + GET_WORD (abfd, p->e_strx);
      h = bfd_link_hash_lookup (info->hash, name, false, false, true);

      /* We are only interested in symbols that are currently
	 undefined or common.  */
      if (h == (struct bfd_link_hash_entry *) NULL
	  || (h->type != bfd_link_hash_undefined
	      && h->type != bfd_link_hash_common))
	{
	  if (type == (N_INDR | N_EXT))
	    ++p;
	  continue;
	}

      if (type == (N_TEXT | N_EXT)
	  || type == (N_DATA | N_EXT)
	  || type == (N_BSS | N_EXT)
	  || type == (N_ABS | N_EXT)
	  || type == (N_INDR | N_EXT))
	{
	  /* This object file defines this symbol.  We must link it
	     in.  This is true regardless of whether the current
	     definition of the symbol is undefined or common.  If the
	     current definition is common, we have a case in which we
	     have already seen an object file including
	         int a;
	     and this object file from the archive includes
	         int a = 5;
	     In such a case we must include this object file.

	     FIXME: The SunOS 4.1.3 linker will pull in the archive
	     element if the symbol is defined in the .data section,
	     but not if it is defined in the .text section.  That
	     seems a bit crazy to me, and I haven't implemented it.
	     However, it might be correct.  */
	  if (! (*info->callbacks->add_archive_element) (info, abfd, name))
	    return false;
	  *pneeded = true;
	  return true;
	}

      if (type == (N_UNDF | N_EXT))
	{
	  bfd_vma value;

	  value = GET_WORD (abfd, p->e_value);
	  if (value != 0)
	    {
	      /* This symbol is common in the object from the archive
		 file.  */
	      if (h->type == bfd_link_hash_undefined)
		{
		  bfd *symbfd;
		  unsigned int power;

		  symbfd = h->u.undef.abfd;
		  if (symbfd == (bfd *) NULL)
		    {
		      /* This symbol was created as undefined from
			 outside BFD.  We assume that we should link
			 in the object file.  This is done for the -u
			 option in the linker.  */
		      if (! (*info->callbacks->add_archive_element) (info,
								     abfd,
								     name))
			return false;
		      *pneeded = true;
		      return true;
		    }
		  /* Turn the current link symbol into a common
		     symbol.  It is already on the undefs list.  */
		  h->type = bfd_link_hash_common;
		  h->u.c.p = ((struct bfd_link_hash_common_entry *)
			      bfd_hash_allocate (&info->hash->table,
				  sizeof (struct bfd_link_hash_common_entry)));
		  if (h->u.c.p == NULL)
		    return false;

		  h->u.c.size = value;

		  /* FIXME: This isn't quite right.  The maximum
		     alignment of a common symbol should be set by the
		     architecture of the output file, not of the input
		     file.  */
		  power = bfd_log2 (value);
		  if (power > bfd_get_arch_info (abfd)->section_align_power)
		    power = bfd_get_arch_info (abfd)->section_align_power;
		  h->u.c.p->alignment_power = power;

		  h->u.c.p->section = bfd_make_section_old_way (symbfd,
								"COMMON");
		}
	      else
		{
		  /* Adjust the size of the common symbol if
		     necessary.  */
		  if (value > h->u.c.size)
		    h->u.c.size = value;
		}
	    }
	}

      if (type == N_WEAKA
	  || type == N_WEAKT
	  || type == N_WEAKD
	  || type == N_WEAKB)
	{
	  /* This symbol is weak but defined.  We must pull it in if
	     the current link symbol is undefined, but we don't want
	     it if the current link symbol is common.  */
	  if (h->type == bfd_link_hash_undefined)
	    {
	      if (! (*info->callbacks->add_archive_element) (info, abfd, name))
		return false;
	      *pneeded = true;
	      return true;
	    }
	}
    }

  /* We do not need this object file.  */
  return true;
}

/* Add all symbols from an object file to the hash table.  */

static boolean
aout_link_add_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  boolean (*add_one_symbol) PARAMS ((struct bfd_link_info *, bfd *,
				     const char *, flagword, asection *,
				     bfd_vma, const char *, boolean,
				     boolean,
				     struct bfd_link_hash_entry **));
  struct external_nlist *syms;
  bfd_size_type sym_count;
  char *strings;
  boolean copy;
  struct aout_link_hash_entry **sym_hash;
  register struct external_nlist *p;
  struct external_nlist *pend;

  syms = obj_aout_external_syms (abfd);
  sym_count = obj_aout_external_sym_count (abfd);
  strings = obj_aout_external_strings (abfd);
  if (info->keep_memory)
    copy = false;
  else
    copy = true;

  if (aout_backend_info (abfd)->add_dynamic_symbols != NULL)
    {
      if (! ((*aout_backend_info (abfd)->add_dynamic_symbols)
	     (abfd, info, &syms, &sym_count, &strings)))
	return false;
    }

  /* We keep a list of the linker hash table entries that correspond
     to particular symbols.  We could just look them up in the hash
     table, but keeping the list is more efficient.  Perhaps this
     should be conditional on info->keep_memory.  */
  sym_hash = ((struct aout_link_hash_entry **)
	      bfd_alloc (abfd,
			 ((size_t) sym_count
			  * sizeof (struct aout_link_hash_entry *))));
  if (sym_hash == NULL && sym_count != 0)
    return false;
  obj_aout_sym_hashes (abfd) = sym_hash;

  add_one_symbol = aout_backend_info (abfd)->add_one_symbol;
  if (add_one_symbol == NULL)
    add_one_symbol = _bfd_generic_link_add_one_symbol;

  p = syms;
  pend = p + sym_count;
  for (; p < pend; p++, sym_hash++)
    {
      int type;
      const char *name;
      bfd_vma value;
      asection *section;
      flagword flags;
      const char *string;

      *sym_hash = NULL;

      type = bfd_h_get_8 (abfd, p->e_type);

      /* Ignore debugging symbols.  */
      if ((type & N_STAB) != 0)
	continue;

      name = strings + GET_WORD (abfd, p->e_strx);
      value = GET_WORD (abfd, p->e_value);
      flags = BSF_GLOBAL;
      string = NULL;
      switch (type)
	{
	default:
	  abort ();

	case N_UNDF:
	case N_ABS:
	case N_TEXT:
	case N_DATA:
	case N_BSS:
	case N_FN_SEQ:
	case N_COMM:
	case N_SETV:
	case N_FN:
	  /* Ignore symbols that are not externally visible.  */
	  continue;
	case N_INDR:
	  /* Ignore local indirect symbol.  */
	  ++p;
	  ++sym_hash;
	  continue;

	case N_UNDF | N_EXT:
	  if (value == 0)
	    {
	      section = bfd_und_section_ptr;
	      flags = 0;
	    }
	  else
	    section = bfd_com_section_ptr;
	  break;
	case N_ABS | N_EXT:
	  section = bfd_abs_section_ptr;
	  break;
	case N_TEXT | N_EXT:
	  section = obj_textsec (abfd);
	  value -= bfd_get_section_vma (abfd, section);
	  break;
	case N_DATA | N_EXT:
	case N_SETV | N_EXT:
	  /* Treat N_SETV symbols as N_DATA symbol; see comment in
	     translate_from_native_sym_flags.  */
	  section = obj_datasec (abfd);
	  value -= bfd_get_section_vma (abfd, section);
	  break;
	case N_BSS | N_EXT:
	  section = obj_bsssec (abfd);
	  value -= bfd_get_section_vma (abfd, section);
	  break;
	case N_INDR | N_EXT:
	  /* An indirect symbol.  The next symbol is the symbol
	     which this one really is.  */
	  BFD_ASSERT (p + 1 < pend);
	  ++p;
	  string = strings + GET_WORD (abfd, p->e_strx);
	  section = bfd_ind_section_ptr;
	  flags |= BSF_INDIRECT;
	  break;
	case N_COMM | N_EXT:
	  section = bfd_com_section_ptr;
	  break;
	case N_SETA: case N_SETA | N_EXT:
	  section = bfd_abs_section_ptr;
	  flags |= BSF_CONSTRUCTOR;
	  break;
	case N_SETT: case N_SETT | N_EXT:
	  section = obj_textsec (abfd);
	  flags |= BSF_CONSTRUCTOR;
	  value -= bfd_get_section_vma (abfd, section);
	  break;
	case N_SETD: case N_SETD | N_EXT:
	  section = obj_datasec (abfd);
	  flags |= BSF_CONSTRUCTOR;
	  value -= bfd_get_section_vma (abfd, section);
	  break;
	case N_SETB: case N_SETB | N_EXT:
	  section = obj_bsssec (abfd);
	  flags |= BSF_CONSTRUCTOR;
	  value -= bfd_get_section_vma (abfd, section);
	  break;
	case N_WARNING:
	  /* A warning symbol.  The next symbol is the one to warn
	     about.  */
	  BFD_ASSERT (p + 1 < pend);
	  ++p;
	  string = name;
	  name = strings + GET_WORD (abfd, p->e_strx);
	  section = bfd_und_section_ptr;
	  flags |= BSF_WARNING;
	  break;
	case N_WEAKU:
	  section = bfd_und_section_ptr;
	  flags = BSF_WEAK;
	  break;
	case N_WEAKA:
	  section = bfd_abs_section_ptr;
	  flags = BSF_WEAK;
	  break;
	case N_WEAKT:
	  section = obj_textsec (abfd);
	  value -= bfd_get_section_vma (abfd, section);
	  flags = BSF_WEAK;
	  break;
	case N_WEAKD:
	  section = obj_datasec (abfd);
	  value -= bfd_get_section_vma (abfd, section);
	  flags = BSF_WEAK;
	  break;
	case N_WEAKB:
	  section = obj_bsssec (abfd);
	  value -= bfd_get_section_vma (abfd, section);
	  flags = BSF_WEAK;
	  break;
	}

      if (! ((*add_one_symbol)
	     (info, abfd, name, flags, section, value, string, copy, false,
	      (struct bfd_link_hash_entry **) sym_hash)))
	return false;

      /* Restrict the maximum alignment of a common symbol based on
	 the architecture, since a.out has no way to represent
	 alignment requirements of a section in a .o file.  FIXME:
	 This isn't quite right: it should use the architecture of the
	 output file, not the input files.  */
      if ((*sym_hash)->root.type == bfd_link_hash_common
	  && ((*sym_hash)->root.u.c.p->alignment_power >
	      bfd_get_arch_info (abfd)->section_align_power))
	(*sym_hash)->root.u.c.p->alignment_power =
	  bfd_get_arch_info (abfd)->section_align_power;

      /* If this is a set symbol, and we are not building sets, then
	 it is possible for the hash entry to not have been set.  In
	 such a case, treat the symbol as not globally defined.  */
      if ((*sym_hash)->root.type == bfd_link_hash_new)
	{
	  BFD_ASSERT ((flags & BSF_CONSTRUCTOR) != 0);
	  *sym_hash = NULL;
	}

      if (type == (N_INDR | N_EXT) || type == N_WARNING)
	++sym_hash;
    }

  return true;
}

/* A hash table used for header files with N_BINCL entries.  */

struct aout_link_includes_table
{
  struct bfd_hash_table root;
};

/* A linked list of totals that we have found for a particular header
   file.  */

struct aout_link_includes_totals
{
  struct aout_link_includes_totals *next;
  bfd_vma total;
};

/* An entry in the header file hash table.  */

struct aout_link_includes_entry
{
  struct bfd_hash_entry root;
  /* List of totals we have found for this file.  */
  struct aout_link_includes_totals *totals;
};

/* Look up an entry in an the header file hash table.  */

#define aout_link_includes_lookup(table, string, create, copy) \
  ((struct aout_link_includes_entry *) \
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

/* During the final link step we need to pass around a bunch of
   information, so we do it in an instance of this structure.  */

struct aout_final_link_info
{
  /* General link information.  */
  struct bfd_link_info *info;
  /* Output bfd.  */
  bfd *output_bfd;
  /* Reloc file positions.  */
  file_ptr treloff, dreloff;
  /* File position of symbols.  */
  file_ptr symoff;
  /* String table.  */
  struct bfd_strtab_hash *strtab;
  /* Header file hash table.  */
  struct aout_link_includes_table includes;
  /* A buffer large enough to hold the contents of any section.  */
  bfd_byte *contents;
  /* A buffer large enough to hold the relocs of any section.  */
  PTR relocs;
  /* A buffer large enough to hold the symbol map of any input BFD.  */
  int *symbol_map;
  /* A buffer large enough to hold output symbols of any input BFD.  */
  struct external_nlist *output_syms;
};

static struct bfd_hash_entry *aout_link_includes_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static boolean aout_link_input_bfd
  PARAMS ((struct aout_final_link_info *, bfd *input_bfd));
static boolean aout_link_write_symbols
  PARAMS ((struct aout_final_link_info *, bfd *input_bfd));
static boolean aout_link_write_other_symbol
  PARAMS ((struct aout_link_hash_entry *, PTR));
static boolean aout_link_input_section
  PARAMS ((struct aout_final_link_info *, bfd *input_bfd,
	   asection *input_section, file_ptr *reloff_ptr,
	   bfd_size_type rel_size));
static boolean aout_link_input_section_std
  PARAMS ((struct aout_final_link_info *, bfd *input_bfd,
	   asection *input_section, struct reloc_std_external *,
	   bfd_size_type rel_size, bfd_byte *contents));
static boolean aout_link_input_section_ext
  PARAMS ((struct aout_final_link_info *, bfd *input_bfd,
	   asection *input_section, struct reloc_ext_external *,
	   bfd_size_type rel_size, bfd_byte *contents));
static INLINE asection *aout_reloc_index_to_section
  PARAMS ((bfd *, int));
static boolean aout_link_reloc_link_order
  PARAMS ((struct aout_final_link_info *, asection *,
	   struct bfd_link_order *));

/* The function to create a new entry in the header file hash table.  */

static struct bfd_hash_entry *
aout_link_includes_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct aout_link_includes_entry *ret =
    (struct aout_link_includes_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct aout_link_includes_entry *) NULL)
    ret = ((struct aout_link_includes_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct aout_link_includes_entry)));
  if (ret == (struct aout_link_includes_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct aout_link_includes_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));
  if (ret)
    {
      /* Set local fields.  */
      ret->totals = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Do the final link step.  This is called on the output BFD.  The
   INFO structure should point to a list of BFDs linked through the
   link_next field which can be used to find each BFD which takes part
   in the output.  Also, each section in ABFD should point to a list
   of bfd_link_order structures which list all the input sections for
   the output section.  */

boolean
NAME(aout,final_link) (abfd, info, callback)
     bfd *abfd;
     struct bfd_link_info *info;
     void (*callback) PARAMS ((bfd *, file_ptr *, file_ptr *, file_ptr *));
{
  struct aout_final_link_info aout_info;
  boolean includes_hash_initialized = false;
  register bfd *sub;
  bfd_size_type trsize, drsize;
  size_t max_contents_size;
  size_t max_relocs_size;
  size_t max_sym_count;
  bfd_size_type text_size;
  file_ptr text_end;
  register struct bfd_link_order *p;
  asection *o;
  boolean have_link_order_relocs;

  if (info->shared)
    abfd->flags |= DYNAMIC;

  aout_info.info = info;
  aout_info.output_bfd = abfd;
  aout_info.contents = NULL;
  aout_info.relocs = NULL;
  aout_info.symbol_map = NULL;
  aout_info.output_syms = NULL;

  if (! bfd_hash_table_init_n (&aout_info.includes.root,
			       aout_link_includes_newfunc,
			       251))
    goto error_return;
  includes_hash_initialized = true;

  /* Figure out the largest section size.  Also, if generating
     relocateable output, count the relocs.  */
  trsize = 0;
  drsize = 0;
  max_contents_size = 0;
  max_relocs_size = 0;
  max_sym_count = 0;
  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    {
      size_t sz;

      if (info->relocateable)
	{
	  if (bfd_get_flavour (sub) == bfd_target_aout_flavour)
	    {
	      trsize += exec_hdr (sub)->a_trsize;
	      drsize += exec_hdr (sub)->a_drsize;
	    }
	  else
	    {
	      /* FIXME: We need to identify the .text and .data sections
		 and call get_reloc_upper_bound and canonicalize_reloc to
		 work out the number of relocs needed, and then multiply
		 by the reloc size.  */
	      (*_bfd_error_handler)
		(_("%s: relocateable link from %s to %s not supported"),
		 bfd_get_filename (abfd),
		 sub->xvec->name, abfd->xvec->name);
	      bfd_set_error (bfd_error_invalid_operation);
	      goto error_return;
	    }
	}

      if (bfd_get_flavour (sub) == bfd_target_aout_flavour)
	{
	  sz = bfd_section_size (sub, obj_textsec (sub));
	  if (sz > max_contents_size)
	    max_contents_size = sz;
	  sz = bfd_section_size (sub, obj_datasec (sub));
	  if (sz > max_contents_size)
	    max_contents_size = sz;

	  sz = exec_hdr (sub)->a_trsize;
	  if (sz > max_relocs_size)
	    max_relocs_size = sz;
	  sz = exec_hdr (sub)->a_drsize;
	  if (sz > max_relocs_size)
	    max_relocs_size = sz;

	  sz = obj_aout_external_sym_count (sub);
	  if (sz > max_sym_count)
	    max_sym_count = sz;
	}
    }

  if (info->relocateable)
    {
      if (obj_textsec (abfd) != (asection *) NULL)
	trsize += (_bfd_count_link_order_relocs (obj_textsec (abfd)
						 ->link_order_head)
		   * obj_reloc_entry_size (abfd));
      if (obj_datasec (abfd) != (asection *) NULL)
	drsize += (_bfd_count_link_order_relocs (obj_datasec (abfd)
						 ->link_order_head)
		   * obj_reloc_entry_size (abfd));
    }

  exec_hdr (abfd)->a_trsize = trsize;
  exec_hdr (abfd)->a_drsize = drsize;

  exec_hdr (abfd)->a_entry = bfd_get_start_address (abfd);

  /* Adjust the section sizes and vmas according to the magic number.
     This sets a_text, a_data and a_bss in the exec_hdr and sets the
     filepos for each section.  */
  if (! NAME(aout,adjust_sizes_and_vmas) (abfd, &text_size, &text_end))
    goto error_return;

  /* The relocation and symbol file positions differ among a.out
     targets.  We are passed a callback routine from the backend
     specific code to handle this.
     FIXME: At this point we do not know how much space the symbol
     table will require.  This will not work for any (nonstandard)
     a.out target that needs to know the symbol table size before it
     can compute the relocation file positions.  This may or may not
     be the case for the hp300hpux target, for example.  */
  (*callback) (abfd, &aout_info.treloff, &aout_info.dreloff,
	       &aout_info.symoff);
  obj_textsec (abfd)->rel_filepos = aout_info.treloff;
  obj_datasec (abfd)->rel_filepos = aout_info.dreloff;
  obj_sym_filepos (abfd) = aout_info.symoff;

  /* We keep a count of the symbols as we output them.  */
  obj_aout_external_sym_count (abfd) = 0;

  /* We accumulate the string table as we write out the symbols.  */
  aout_info.strtab = _bfd_stringtab_init ();
  if (aout_info.strtab == NULL)
    goto error_return;

  /* Allocate buffers to hold section contents and relocs.  */
  aout_info.contents = (bfd_byte *) bfd_malloc (max_contents_size);
  aout_info.relocs = (PTR) bfd_malloc (max_relocs_size);
  aout_info.symbol_map = (int *) bfd_malloc (max_sym_count * sizeof (int *));
  aout_info.output_syms = ((struct external_nlist *)
			   bfd_malloc ((max_sym_count + 1)
				       * sizeof (struct external_nlist)));
  if ((aout_info.contents == NULL && max_contents_size != 0)
      || (aout_info.relocs == NULL && max_relocs_size != 0)
      || (aout_info.symbol_map == NULL && max_sym_count != 0)
      || aout_info.output_syms == NULL)
    goto error_return;

  /* If we have a symbol named __DYNAMIC, force it out now.  This is
     required by SunOS.  Doing this here rather than in sunos.c is a
     hack, but it's easier than exporting everything which would be
     needed.  */
  {
    struct aout_link_hash_entry *h;

    h = aout_link_hash_lookup (aout_hash_table (info), "__DYNAMIC",
			       false, false, false);
    if (h != NULL)
      aout_link_write_other_symbol (h, &aout_info);
  }

  /* The most time efficient way to do the link would be to read all
     the input object files into memory and then sort out the
     information into the output file.  Unfortunately, that will
     probably use too much memory.  Another method would be to step
     through everything that composes the text section and write it
     out, and then everything that composes the data section and write
     it out, and then write out the relocs, and then write out the
     symbols.  Unfortunately, that requires reading stuff from each
     input file several times, and we will not be able to keep all the
     input files open simultaneously, and reopening them will be slow.

     What we do is basically process one input file at a time.  We do
     everything we need to do with an input file once--copy over the
     section contents, handle the relocation information, and write
     out the symbols--and then we throw away the information we read
     from it.  This approach requires a lot of lseeks of the output
     file, which is unfortunate but still faster than reopening a lot
     of files.

     We use the output_has_begun field of the input BFDs to see
     whether we have already handled it.  */
  for (sub = info->input_bfds; sub != (bfd *) NULL; sub = sub->link_next)
    sub->output_has_begun = false;

  /* Mark all sections which are to be included in the link.  This
     will normally be every section.  We need to do this so that we
     can identify any sections which the linker has decided to not
     include.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_indirect_link_order)
	    p->u.indirect.section->linker_mark = true;
	}
    }

  have_link_order_relocs = false;
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      for (p = o->link_order_head;
	   p != (struct bfd_link_order *) NULL;
	   p = p->next)
	{
	  if (p->type == bfd_indirect_link_order
	      && (bfd_get_flavour (p->u.indirect.section->owner)
		  == bfd_target_aout_flavour))
	    {
	      bfd *input_bfd;

	      input_bfd = p->u.indirect.section->owner;
	      if (! input_bfd->output_has_begun)
		{
		  if (! aout_link_input_bfd (&aout_info, input_bfd))
		    goto error_return;
		  input_bfd->output_has_begun = true;
		}
	    }
	  else if (p->type == bfd_section_reloc_link_order
		   || p->type == bfd_symbol_reloc_link_order)
	    {
	      /* These are handled below.  */
	      have_link_order_relocs = true;
	    }
	  else
	    {
	      if (! _bfd_default_link_order (abfd, info, o, p))
		goto error_return;
	    }
	}
    }

  /* Write out any symbols that we have not already written out.  */
  aout_link_hash_traverse (aout_hash_table (info),
			   aout_link_write_other_symbol,
			   (PTR) &aout_info);

  /* Now handle any relocs we were asked to create by the linker.
     These did not come from any input file.  We must do these after
     we have written out all the symbols, so that we know the symbol
     indices to use.  */
  if (have_link_order_relocs)
    {
      for (o = abfd->sections; o != (asection *) NULL; o = o->next)
	{
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      if (p->type == bfd_section_reloc_link_order
		  || p->type == bfd_symbol_reloc_link_order)
		{
		  if (! aout_link_reloc_link_order (&aout_info, o, p))
		    goto error_return;
		}
	    }
	}
    }

  if (aout_info.contents != NULL)
    {
      free (aout_info.contents);
      aout_info.contents = NULL;
    }
  if (aout_info.relocs != NULL)
    {
      free (aout_info.relocs);
      aout_info.relocs = NULL;
    }
  if (aout_info.symbol_map != NULL)
    {
      free (aout_info.symbol_map);
      aout_info.symbol_map = NULL;
    }
  if (aout_info.output_syms != NULL)
    {
      free (aout_info.output_syms);
      aout_info.output_syms = NULL;
    }
  if (includes_hash_initialized)
    {
      bfd_hash_table_free (&aout_info.includes.root);
      includes_hash_initialized = false;
    }

  /* Finish up any dynamic linking we may be doing.  */
  if (aout_backend_info (abfd)->finish_dynamic_link != NULL)
    {
      if (! (*aout_backend_info (abfd)->finish_dynamic_link) (abfd, info))
	goto error_return;
    }

  /* Update the header information.  */
  abfd->symcount = obj_aout_external_sym_count (abfd);
  exec_hdr (abfd)->a_syms = abfd->symcount * EXTERNAL_NLIST_SIZE;
  obj_str_filepos (abfd) = obj_sym_filepos (abfd) + exec_hdr (abfd)->a_syms;
  obj_textsec (abfd)->reloc_count =
    exec_hdr (abfd)->a_trsize / obj_reloc_entry_size (abfd);
  obj_datasec (abfd)->reloc_count =
    exec_hdr (abfd)->a_drsize / obj_reloc_entry_size (abfd);

  /* Write out the string table, unless there are no symbols.  */
  if (abfd->symcount > 0)
    {
      if (bfd_seek (abfd, obj_str_filepos (abfd), SEEK_SET) != 0
	  || ! emit_stringtab (abfd, aout_info.strtab))
	goto error_return;
    }
  else if (obj_textsec (abfd)->reloc_count == 0
	   && obj_datasec (abfd)->reloc_count == 0)
    {
      bfd_byte b;

      b = 0;
      if (bfd_seek (abfd,
		    (obj_datasec (abfd)->filepos
		     + exec_hdr (abfd)->a_data
		     - 1),
		    SEEK_SET) != 0
	  || bfd_write (&b, 1, 1, abfd) != 1)
	goto error_return;
    }

  return true;

 error_return:
  if (aout_info.contents != NULL)
    free (aout_info.contents);
  if (aout_info.relocs != NULL)
    free (aout_info.relocs);
  if (aout_info.symbol_map != NULL)
    free (aout_info.symbol_map);
  if (aout_info.output_syms != NULL)
    free (aout_info.output_syms);
  if (includes_hash_initialized)
    bfd_hash_table_free (&aout_info.includes.root);
  return false;
}

/* Link an a.out input BFD into the output file.  */

static boolean
aout_link_input_bfd (finfo, input_bfd)
     struct aout_final_link_info *finfo;
     bfd *input_bfd;
{
  bfd_size_type sym_count;

  BFD_ASSERT (bfd_get_format (input_bfd) == bfd_object);

  /* If this is a dynamic object, it may need special handling.  */
  if ((input_bfd->flags & DYNAMIC) != 0
      && aout_backend_info (input_bfd)->link_dynamic_object != NULL)
    {
      return ((*aout_backend_info (input_bfd)->link_dynamic_object)
	      (finfo->info, input_bfd));
    }

  /* Get the symbols.  We probably have them already, unless
     finfo->info->keep_memory is false.  */
  if (! aout_get_external_symbols (input_bfd))
    return false;

  sym_count = obj_aout_external_sym_count (input_bfd);

  /* Write out the symbols and get a map of the new indices.  The map
     is placed into finfo->symbol_map.  */
  if (! aout_link_write_symbols (finfo, input_bfd))
    return false;

  /* Relocate and write out the sections.  These functions use the
     symbol map created by aout_link_write_symbols.  The linker_mark
     field will be set if these sections are to be included in the
     link, which will normally be the case.  */
  if (obj_textsec (input_bfd)->linker_mark)
    {
      if (! aout_link_input_section (finfo, input_bfd,
				     obj_textsec (input_bfd),
				     &finfo->treloff,
				     exec_hdr (input_bfd)->a_trsize))
	return false;
    }
  if (obj_datasec (input_bfd)->linker_mark)
    {
      if (! aout_link_input_section (finfo, input_bfd,
				     obj_datasec (input_bfd),
				     &finfo->dreloff,
				     exec_hdr (input_bfd)->a_drsize))
	return false;
    }

  /* If we are not keeping memory, we don't need the symbols any
     longer.  We still need them if we are keeping memory, because the
     strings in the hash table point into them.  */
  if (! finfo->info->keep_memory)
    {
      if (! aout_link_free_symbols (input_bfd))
	return false;
    }

  return true;
}

/* Adjust and write out the symbols for an a.out file.  Set the new
   symbol indices into a symbol_map.  */

static boolean
aout_link_write_symbols (finfo, input_bfd)
     struct aout_final_link_info *finfo;
     bfd *input_bfd;
{
  bfd *output_bfd;
  bfd_size_type sym_count;
  char *strings;
  enum bfd_link_strip strip;
  enum bfd_link_discard discard;
  struct external_nlist *outsym;
  bfd_size_type strtab_index;
  register struct external_nlist *sym;
  struct external_nlist *sym_end;
  struct aout_link_hash_entry **sym_hash;
  int *symbol_map;
  boolean pass;
  boolean skip_next;

  output_bfd = finfo->output_bfd;
  sym_count = obj_aout_external_sym_count (input_bfd);
  strings = obj_aout_external_strings (input_bfd);
  strip = finfo->info->strip;
  discard = finfo->info->discard;
  outsym = finfo->output_syms;

  /* First write out a symbol for this object file, unless we are
     discarding such symbols.  */
  if (strip != strip_all
      && (strip != strip_some
	  || bfd_hash_lookup (finfo->info->keep_hash, input_bfd->filename,
			      false, false) != NULL)
      && discard != discard_all)
    {
      bfd_h_put_8 (output_bfd, N_TEXT, outsym->e_type);
      bfd_h_put_8 (output_bfd, 0, outsym->e_other);
      bfd_h_put_16 (output_bfd, (bfd_vma) 0, outsym->e_desc);
      strtab_index = add_to_stringtab (output_bfd, finfo->strtab,
				       input_bfd->filename, false);
      if (strtab_index == (bfd_size_type) -1)
	return false;
      PUT_WORD (output_bfd, strtab_index, outsym->e_strx);
      PUT_WORD (output_bfd,
		(bfd_get_section_vma (output_bfd,
				      obj_textsec (input_bfd)->output_section)
		 + obj_textsec (input_bfd)->output_offset),
		outsym->e_value);
      ++obj_aout_external_sym_count (output_bfd);
      ++outsym;
    }

  pass = false;
  skip_next = false;
  sym = obj_aout_external_syms (input_bfd);
  sym_end = sym + sym_count;
  sym_hash = obj_aout_sym_hashes (input_bfd);
  symbol_map = finfo->symbol_map;
  memset (symbol_map, 0, sym_count * sizeof *symbol_map);
  for (; sym < sym_end; sym++, sym_hash++, symbol_map++)
    {
      const char *name;
      int type;
      struct aout_link_hash_entry *h;
      boolean skip;
      asection *symsec;
      bfd_vma val = 0;
      boolean copy;

      /* We set *symbol_map to 0 above for all symbols.  If it has
         already been set to -1 for this symbol, it means that we are
         discarding it because it appears in a duplicate header file.
         See the N_BINCL code below.  */
      if (*symbol_map == -1)
	continue;

      /* Initialize *symbol_map to -1, which means that the symbol was
         not copied into the output file.  We will change it later if
         we do copy the symbol over.  */
      *symbol_map = -1;

      type = bfd_h_get_8 (input_bfd, sym->e_type);
      name = strings + GET_WORD (input_bfd, sym->e_strx);

      h = NULL;

      if (pass)
	{
	  /* Pass this symbol through.  It is the target of an
	     indirect or warning symbol.  */
	  val = GET_WORD (input_bfd, sym->e_value);
	  pass = false;
	}
      else if (skip_next)
	{
	  /* Skip this symbol, which is the target of an indirect
	     symbol that we have changed to no longer be an indirect
	     symbol.  */
	  skip_next = false;
	  continue;
	}
      else
	{
	  struct aout_link_hash_entry *hresolve;

	  /* We have saved the hash table entry for this symbol, if
	     there is one.  Note that we could just look it up again
	     in the hash table, provided we first check that it is an
	     external symbol.  */
	  h = *sym_hash;

	  /* Use the name from the hash table, in case the symbol was
             wrapped.  */
	  if (h != NULL)
	    name = h->root.root.string;

	  /* If this is an indirect or warning symbol, then change
	     hresolve to the base symbol.  We also change *sym_hash so
	     that the relocation routines relocate against the real
	     symbol.  */
	  hresolve = h;
	  if (h != (struct aout_link_hash_entry *) NULL
	      && (h->root.type == bfd_link_hash_indirect
		  || h->root.type == bfd_link_hash_warning))
	    {
	      hresolve = (struct aout_link_hash_entry *) h->root.u.i.link;
	      while (hresolve->root.type == bfd_link_hash_indirect
		     || hresolve->root.type == bfd_link_hash_warning)
		hresolve = ((struct aout_link_hash_entry *)
			    hresolve->root.u.i.link);
	      *sym_hash = hresolve;
	    }

	  /* If the symbol has already been written out, skip it.  */
	  if (h != (struct aout_link_hash_entry *) NULL
	      && h->root.type != bfd_link_hash_warning
	      && h->written)
	    {
	      if ((type & N_TYPE) == N_INDR
		  || type == N_WARNING)
		skip_next = true;
	      *symbol_map = h->indx;
	      continue;
	    }

	  /* See if we are stripping this symbol.  */
	  skip = false;
	  switch (strip)
	    {
	    case strip_none:
	      break;
	    case strip_debugger:
	      if ((type & N_STAB) != 0)
		skip = true;
	      break;
	    case strip_some:
	      if (bfd_hash_lookup (finfo->info->keep_hash, name, false, false)
		  == NULL)
		skip = true;
	      break;
	    case strip_all:
	      skip = true;
	      break;
	    }
	  if (skip)
	    {
	      if (h != (struct aout_link_hash_entry *) NULL)
		h->written = true;
	      continue;
	    }

	  /* Get the value of the symbol.  */
	  if ((type & N_TYPE) == N_TEXT
	      || type == N_WEAKT)
	    symsec = obj_textsec (input_bfd);
	  else if ((type & N_TYPE) == N_DATA
		   || type == N_WEAKD)
	    symsec = obj_datasec (input_bfd);
	  else if ((type & N_TYPE) == N_BSS
		   || type == N_WEAKB)
	    symsec = obj_bsssec (input_bfd);
	  else if ((type & N_TYPE) == N_ABS
		   || type == N_WEAKA)
	    symsec = bfd_abs_section_ptr;
	  else if (((type & N_TYPE) == N_INDR
		    && (hresolve == (struct aout_link_hash_entry *) NULL
			|| (hresolve->root.type != bfd_link_hash_defined
			    && hresolve->root.type != bfd_link_hash_defweak
			    && hresolve->root.type != bfd_link_hash_common)))
		   || type == N_WARNING)
	    {
	      /* Pass the next symbol through unchanged.  The
		 condition above for indirect symbols is so that if
		 the indirect symbol was defined, we output it with
		 the correct definition so the debugger will
		 understand it.  */
	      pass = true;
	      val = GET_WORD (input_bfd, sym->e_value);
	      symsec = NULL;
	    }
	  else if ((type & N_STAB) != 0)
	    {
	      val = GET_WORD (input_bfd, sym->e_value);
	      symsec = NULL;
	    }
	  else
	    {
	      /* If we get here with an indirect symbol, it means that
		 we are outputting it with a real definition.  In such
		 a case we do not want to output the next symbol,
		 which is the target of the indirection.  */
	      if ((type & N_TYPE) == N_INDR)
		skip_next = true;

	      symsec = NULL;

	      /* We need to get the value from the hash table.  We use
		 hresolve so that if we have defined an indirect
		 symbol we output the final definition.  */
	      if (h == (struct aout_link_hash_entry *) NULL)
		{
		  switch (type & N_TYPE)
		    {
		    case N_SETT:
		      symsec = obj_textsec (input_bfd);
		      break;
		    case N_SETD:
		      symsec = obj_datasec (input_bfd);
		      break;
		    case N_SETB:
		      symsec = obj_bsssec (input_bfd);
		      break;
		    case N_SETA:
		      symsec = bfd_abs_section_ptr;
		      break;
		    default:
		      val = 0;
		      break;
		    }
		}
	      else if (hresolve->root.type == bfd_link_hash_defined
		       || hresolve->root.type == bfd_link_hash_defweak)
		{
		  asection *input_section;
		  asection *output_section;

		  /* This case usually means a common symbol which was
		     turned into a defined symbol.  */
		  input_section = hresolve->root.u.def.section;
		  output_section = input_section->output_section;
		  BFD_ASSERT (bfd_is_abs_section (output_section)
			      || output_section->owner == output_bfd);
		  val = (hresolve->root.u.def.value
			 + bfd_get_section_vma (output_bfd, output_section)
			 + input_section->output_offset);

		  /* Get the correct type based on the section.  If
		     this is a constructed set, force it to be
		     globally visible.  */
		  if (type == N_SETT
		      || type == N_SETD
		      || type == N_SETB
		      || type == N_SETA)
		    type |= N_EXT;

		  type &=~ N_TYPE;

		  if (output_section == obj_textsec (output_bfd))
		    type |= (hresolve->root.type == bfd_link_hash_defined
			     ? N_TEXT
			     : N_WEAKT);
		  else if (output_section == obj_datasec (output_bfd))
		    type |= (hresolve->root.type == bfd_link_hash_defined
			     ? N_DATA
			     : N_WEAKD);
		  else if (output_section == obj_bsssec (output_bfd))
		    type |= (hresolve->root.type == bfd_link_hash_defined
			     ? N_BSS
			     : N_WEAKB);
		  else
		    type |= (hresolve->root.type == bfd_link_hash_defined
			     ? N_ABS
			     : N_WEAKA);
		}
	      else if (hresolve->root.type == bfd_link_hash_common)
		val = hresolve->root.u.c.size;
	      else if (hresolve->root.type == bfd_link_hash_undefweak)
		{
		  val = 0;
		  type = N_WEAKU;
		}
	      else
		val = 0;
	    }
	  if (symsec != (asection *) NULL)
	    val = (symsec->output_section->vma
		   + symsec->output_offset
		   + (GET_WORD (input_bfd, sym->e_value)
		      - symsec->vma));

	  /* If this is a global symbol set the written flag, and if
	     it is a local symbol see if we should discard it.  */
	  if (h != (struct aout_link_hash_entry *) NULL)
	    {
	      h->written = true;
	      h->indx = obj_aout_external_sym_count (output_bfd);
	    }
	  else if ((type & N_TYPE) != N_SETT
		   && (type & N_TYPE) != N_SETD
		   && (type & N_TYPE) != N_SETB
		   && (type & N_TYPE) != N_SETA)
	    {
	      switch (discard)
		{
		case discard_none:
		  break;
		case discard_l:
		  if ((type & N_STAB) == 0
		      && bfd_is_local_label_name (input_bfd, name))
		    skip = true;
		  break;
		case discard_all:
		  skip = true;
		  break;
		}
	      if (skip)
		{
		  pass = false;
		  continue;
		}
	    }

	  /* An N_BINCL symbol indicates the start of the stabs
	     entries for a header file.  We need to scan ahead to the
	     next N_EINCL symbol, ignoring nesting, adding up all the
	     characters in the symbol names, not including the file
	     numbers in types (the first number after an open
	     parenthesis).  */
	  if (type == N_BINCL)
	    {
	      struct external_nlist *incl_sym;
	      int nest;
	      struct aout_link_includes_entry *incl_entry;
	      struct aout_link_includes_totals *t;

	      val = 0;
	      nest = 0;
	      for (incl_sym = sym + 1; incl_sym < sym_end; incl_sym++)
		{
		  int incl_type;

		  incl_type = bfd_h_get_8 (input_bfd, incl_sym->e_type);
		  if (incl_type == N_EINCL)
		    {
		      if (nest == 0)
			break;
		      --nest;
		    }
		  else if (incl_type == N_BINCL)
		    ++nest;
		  else if (nest == 0)
		    {
		      const char *s;

		      s = strings + GET_WORD (input_bfd, incl_sym->e_strx);
		      for (; *s != '\0'; s++)
			{
			  val += *s;
			  if (*s == '(')
			    {
			      /* Skip the file number.  */
			      ++s;
			      while (isdigit ((unsigned char) *s))
				++s;
			      --s;
			    }
			}
		    }
		}

	      /* If we have already included a header file with the
                 same value, then replace this one with an N_EXCL
                 symbol.  */
	      copy = ! finfo->info->keep_memory;
	      incl_entry = aout_link_includes_lookup (&finfo->includes,
						      name, true, copy);
	      if (incl_entry == NULL)
		return false;
	      for (t = incl_entry->totals; t != NULL; t = t->next)
		if (t->total == val)
		  break;
	      if (t == NULL)
		{
		  /* This is the first time we have seen this header
                     file with this set of stabs strings.  */
		  t = ((struct aout_link_includes_totals *)
		       bfd_hash_allocate (&finfo->includes.root,
					  sizeof *t));
		  if (t == NULL)
		    return false;
		  t->total = val;
		  t->next = incl_entry->totals;
		  incl_entry->totals = t;
		}
	      else
		{
		  int *incl_map;

		  /* This is a duplicate header file.  We must change
                     it to be an N_EXCL entry, and mark all the
                     included symbols to prevent outputting them.  */
		  type = N_EXCL;

		  nest = 0;
		  for (incl_sym = sym + 1, incl_map = symbol_map + 1;
		       incl_sym < sym_end;
		       incl_sym++, incl_map++)
		    {
		      int incl_type;

		      incl_type = bfd_h_get_8 (input_bfd, incl_sym->e_type);
		      if (incl_type == N_EINCL)
			{
			  if (nest == 0)
			    {
			      *incl_map = -1;
			      break;
			    }
			  --nest;
			}
		      else if (incl_type == N_BINCL)
			++nest;
		      else if (nest == 0)
			*incl_map = -1;
		    }
		}
	    }
	}

      /* Copy this symbol into the list of symbols we are going to
	 write out.  */
      bfd_h_put_8 (output_bfd, type, outsym->e_type);
      bfd_h_put_8 (output_bfd, bfd_h_get_8 (input_bfd, sym->e_other),
		   outsym->e_other);
      bfd_h_put_16 (output_bfd, bfd_h_get_16 (input_bfd, sym->e_desc),
		    outsym->e_desc);
      copy = false;
      if (! finfo->info->keep_memory)
	{
	  /* name points into a string table which we are going to
	     free.  If there is a hash table entry, use that string.
	     Otherwise, copy name into memory.  */
	  if (h != (struct aout_link_hash_entry *) NULL)
	    name = h->root.root.string;
	  else
	    copy = true;
	}
      strtab_index = add_to_stringtab (output_bfd, finfo->strtab,
				       name, copy);
      if (strtab_index == (bfd_size_type) -1)
	return false;
      PUT_WORD (output_bfd, strtab_index, outsym->e_strx);
      PUT_WORD (output_bfd, val, outsym->e_value);
      *symbol_map = obj_aout_external_sym_count (output_bfd);
      ++obj_aout_external_sym_count (output_bfd);
      ++outsym;
    }

  /* Write out the output symbols we have just constructed.  */
  if (outsym > finfo->output_syms)
    {
      bfd_size_type outsym_count;

      if (bfd_seek (output_bfd, finfo->symoff, SEEK_SET) != 0)
	return false;
      outsym_count = outsym - finfo->output_syms;
      if (bfd_write ((PTR) finfo->output_syms,
		     (bfd_size_type) EXTERNAL_NLIST_SIZE,
		     (bfd_size_type) outsym_count, output_bfd)
	  != outsym_count * EXTERNAL_NLIST_SIZE)
	return false;
      finfo->symoff += outsym_count * EXTERNAL_NLIST_SIZE;
    }

  return true;
}

/* Write out a symbol that was not associated with an a.out input
   object.  */

static boolean
aout_link_write_other_symbol (h, data)
     struct aout_link_hash_entry *h;
     PTR data;
{
  struct aout_final_link_info *finfo = (struct aout_final_link_info *) data;
  bfd *output_bfd;
  int type;
  bfd_vma val;
  struct external_nlist outsym;
  bfd_size_type indx;

  output_bfd = finfo->output_bfd;

  if (aout_backend_info (output_bfd)->write_dynamic_symbol != NULL)
    {
      if (! ((*aout_backend_info (output_bfd)->write_dynamic_symbol)
	     (output_bfd, finfo->info, h)))
	{
	  /* FIXME: No way to handle errors.  */
	  abort ();
	}
    }

  if (h->written)
    return true;

  h->written = true;

  /* An indx of -2 means the symbol must be written.  */
  if (h->indx != -2
      && (finfo->info->strip == strip_all
	  || (finfo->info->strip == strip_some
	      && bfd_hash_lookup (finfo->info->keep_hash, h->root.root.string,
				  false, false) == NULL)))
    return true;

  switch (h->root.type)
    {
    default:
      abort ();
      /* Avoid variable not initialized warnings.  */
      return true;
    case bfd_link_hash_new:
      /* This can happen for set symbols when sets are not being
         built.  */
      return true;
    case bfd_link_hash_undefined:
      type = N_UNDF | N_EXT;
      val = 0;
      break;
    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      {
	asection *sec;

	sec = h->root.u.def.section->output_section;
	BFD_ASSERT (bfd_is_abs_section (sec)
		    || sec->owner == output_bfd);
	if (sec == obj_textsec (output_bfd))
	  type = h->root.type == bfd_link_hash_defined ? N_TEXT : N_WEAKT;
	else if (sec == obj_datasec (output_bfd))
	  type = h->root.type == bfd_link_hash_defined ? N_DATA : N_WEAKD;
	else if (sec == obj_bsssec (output_bfd))
	  type = h->root.type == bfd_link_hash_defined ? N_BSS : N_WEAKB;
	else
	  type = h->root.type == bfd_link_hash_defined ? N_ABS : N_WEAKA;
	type |= N_EXT;
	val = (h->root.u.def.value
	       + sec->vma
	       + h->root.u.def.section->output_offset);
      }
      break;
    case bfd_link_hash_common:
      type = N_UNDF | N_EXT;
      val = h->root.u.c.size;
      break;
    case bfd_link_hash_undefweak:
      type = N_WEAKU;
      val = 0;
    case bfd_link_hash_indirect:
    case bfd_link_hash_warning:
      /* FIXME: Ignore these for now.  The circumstances under which
	 they should be written out are not clear to me.  */
      return true;
    }

  bfd_h_put_8 (output_bfd, type, outsym.e_type);
  bfd_h_put_8 (output_bfd, 0, outsym.e_other);
  bfd_h_put_16 (output_bfd, 0, outsym.e_desc);
  indx = add_to_stringtab (output_bfd, finfo->strtab, h->root.root.string,
			   false);
  if (indx == (bfd_size_type) -1)
    {
      /* FIXME: No way to handle errors.  */
      abort ();
    }
  PUT_WORD (output_bfd, indx, outsym.e_strx);
  PUT_WORD (output_bfd, val, outsym.e_value);

  if (bfd_seek (output_bfd, finfo->symoff, SEEK_SET) != 0
      || bfd_write ((PTR) &outsym, (bfd_size_type) EXTERNAL_NLIST_SIZE,
		    (bfd_size_type) 1, output_bfd) != EXTERNAL_NLIST_SIZE)
    {
      /* FIXME: No way to handle errors.  */
      abort ();
    }

  finfo->symoff += EXTERNAL_NLIST_SIZE;
  h->indx = obj_aout_external_sym_count (output_bfd);
  ++obj_aout_external_sym_count (output_bfd);

  return true;
}

/* Link an a.out section into the output file.  */

static boolean
aout_link_input_section (finfo, input_bfd, input_section, reloff_ptr,
			 rel_size)
     struct aout_final_link_info *finfo;
     bfd *input_bfd;
     asection *input_section;
     file_ptr *reloff_ptr;
     bfd_size_type rel_size;
{
  bfd_size_type input_size;
  PTR relocs;

  /* Get the section contents.  */
  input_size = bfd_section_size (input_bfd, input_section);
  if (! bfd_get_section_contents (input_bfd, input_section,
				  (PTR) finfo->contents,
				  (file_ptr) 0, input_size))
    return false;

  /* Read in the relocs if we haven't already done it.  */
  if (aout_section_data (input_section) != NULL
      && aout_section_data (input_section)->relocs != NULL)
    relocs = aout_section_data (input_section)->relocs;
  else
    {
      relocs = finfo->relocs;
      if (rel_size > 0)
	{
	  if (bfd_seek (input_bfd, input_section->rel_filepos, SEEK_SET) != 0
	      || bfd_read (relocs, 1, rel_size, input_bfd) != rel_size)
	    return false;
	}
    }

  /* Relocate the section contents.  */
  if (obj_reloc_entry_size (input_bfd) == RELOC_STD_SIZE)
    {
      if (! aout_link_input_section_std (finfo, input_bfd, input_section,
					 (struct reloc_std_external *) relocs,
					 rel_size, finfo->contents))
	return false;
    }
  else
    {
      if (! aout_link_input_section_ext (finfo, input_bfd, input_section,
					 (struct reloc_ext_external *) relocs,
					 rel_size, finfo->contents))
	return false;
    }

  /* Write out the section contents.  */
  if (! bfd_set_section_contents (finfo->output_bfd,
				  input_section->output_section,
				  (PTR) finfo->contents,
				  input_section->output_offset,
				  input_size))
    return false;

  /* If we are producing relocateable output, the relocs were
     modified, and we now write them out.  */
  if (finfo->info->relocateable && rel_size > 0)
    {
      if (bfd_seek (finfo->output_bfd, *reloff_ptr, SEEK_SET) != 0)
	return false;
      if (bfd_write (relocs, (bfd_size_type) 1, rel_size, finfo->output_bfd)
	  != rel_size)
	return false;
      *reloff_ptr += rel_size;

      /* Assert that the relocs have not run into the symbols, and
	 that if these are the text relocs they have not run into the
	 data relocs.  */
      BFD_ASSERT (*reloff_ptr <= obj_sym_filepos (finfo->output_bfd)
		  && (reloff_ptr != &finfo->treloff
		      || (*reloff_ptr
			  <= obj_datasec (finfo->output_bfd)->rel_filepos)));
    }

  return true;
}

/* Get the section corresponding to a reloc index.  */

static INLINE asection *
aout_reloc_index_to_section (abfd, indx)
     bfd *abfd;
     int indx;
{
  switch (indx & N_TYPE)
    {
    case N_TEXT:
      return obj_textsec (abfd);
    case N_DATA:
      return obj_datasec (abfd);
    case N_BSS:
      return obj_bsssec (abfd);
    case N_ABS:
    case N_UNDF:
      return bfd_abs_section_ptr;
    default:
      abort ();
    }
  /*NOTREACHED*/
  return NULL;
}

/* Relocate an a.out section using standard a.out relocs.  */

static boolean
aout_link_input_section_std (finfo, input_bfd, input_section, relocs,
			     rel_size, contents)
     struct aout_final_link_info *finfo;
     bfd *input_bfd;
     asection *input_section;
     struct reloc_std_external *relocs;
     bfd_size_type rel_size;
     bfd_byte *contents;
{
  boolean (*check_dynamic_reloc) PARAMS ((struct bfd_link_info *,
					  bfd *, asection *,
					  struct aout_link_hash_entry *,
					  PTR, bfd_byte *, boolean *,
					  bfd_vma *));
  bfd *output_bfd;
  boolean relocateable;
  struct external_nlist *syms;
  char *strings;
  struct aout_link_hash_entry **sym_hashes;
  int *symbol_map;
  bfd_size_type reloc_count;
  register struct reloc_std_external *rel;
  struct reloc_std_external *rel_end;

  output_bfd = finfo->output_bfd;
  check_dynamic_reloc = aout_backend_info (output_bfd)->check_dynamic_reloc;

  BFD_ASSERT (obj_reloc_entry_size (input_bfd) == RELOC_STD_SIZE);
  BFD_ASSERT (input_bfd->xvec->header_byteorder
	      == output_bfd->xvec->header_byteorder);

  relocateable = finfo->info->relocateable;
  syms = obj_aout_external_syms (input_bfd);
  strings = obj_aout_external_strings (input_bfd);
  sym_hashes = obj_aout_sym_hashes (input_bfd);
  symbol_map = finfo->symbol_map;

  reloc_count = rel_size / RELOC_STD_SIZE;
  rel = relocs;
  rel_end = rel + reloc_count;
  for (; rel < rel_end; rel++)
    {
      bfd_vma r_addr;
      int r_index;
      int r_extern;
      int r_pcrel;
      int r_baserel = 0;
      reloc_howto_type *howto;
      struct aout_link_hash_entry *h = NULL;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      r_addr = GET_SWORD (input_bfd, rel->r_address);

#ifdef MY_reloc_howto
      howto = MY_reloc_howto(input_bfd, rel, r_index, r_extern, r_pcrel);
#else
      {
	int r_jmptable;
	int r_relative;
	int r_length;
	unsigned int howto_idx;

	if (bfd_header_big_endian (input_bfd))
	  {
	    r_index   =  ((rel->r_index[0] << 16)
			  | (rel->r_index[1] << 8)
			  | rel->r_index[2]);
	    r_extern  = (0 != (rel->r_type[0] & RELOC_STD_BITS_EXTERN_BIG));
	    r_pcrel   = (0 != (rel->r_type[0] & RELOC_STD_BITS_PCREL_BIG));
	    r_baserel = (0 != (rel->r_type[0] & RELOC_STD_BITS_BASEREL_BIG));
	    r_jmptable= (0 != (rel->r_type[0] & RELOC_STD_BITS_JMPTABLE_BIG));
	    r_relative= (0 != (rel->r_type[0] & RELOC_STD_BITS_RELATIVE_BIG));
	    r_length  = ((rel->r_type[0] & RELOC_STD_BITS_LENGTH_BIG)
			 >> RELOC_STD_BITS_LENGTH_SH_BIG);
	  }
	else
	  {
	    r_index   = ((rel->r_index[2] << 16)
			 | (rel->r_index[1] << 8)
			 | rel->r_index[0]);
	    r_extern  = (0 != (rel->r_type[0] & RELOC_STD_BITS_EXTERN_LITTLE));
	    r_pcrel   = (0 != (rel->r_type[0] & RELOC_STD_BITS_PCREL_LITTLE));
	    r_baserel = (0 != (rel->r_type[0]
			       & RELOC_STD_BITS_BASEREL_LITTLE));
	    r_jmptable= (0 != (rel->r_type[0]
			       & RELOC_STD_BITS_JMPTABLE_LITTLE));
	    r_relative= (0 != (rel->r_type[0]
			       & RELOC_STD_BITS_RELATIVE_LITTLE));
	    r_length  = ((rel->r_type[0] & RELOC_STD_BITS_LENGTH_LITTLE)
			 >> RELOC_STD_BITS_LENGTH_SH_LITTLE);
	  }

	howto_idx = (r_length + 4 * r_pcrel + 8 * r_baserel
		     + 16 * r_jmptable + 32 * r_relative);
	BFD_ASSERT (howto_idx < TABLE_SIZE (howto_table_std));
	howto = howto_table_std + howto_idx;
      }
#endif

      if (relocateable)
	{
	  /* We are generating a relocateable output file, and must
	     modify the reloc accordingly.  */
	  if (r_extern)
	    {
	      /* If we know the symbol this relocation is against,
		 convert it into a relocation against a section.  This
		 is what the native linker does.  */
	      h = sym_hashes[r_index];
	      if (h != (struct aout_link_hash_entry *) NULL
		  && (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak))
		{
		  asection *output_section;

		  /* Change the r_extern value.  */
		  if (bfd_header_big_endian (output_bfd))
		    rel->r_type[0] &=~ RELOC_STD_BITS_EXTERN_BIG;
		  else
		    rel->r_type[0] &=~ RELOC_STD_BITS_EXTERN_LITTLE;

		  /* Compute a new r_index.  */
		  output_section = h->root.u.def.section->output_section;
		  if (output_section == obj_textsec (output_bfd))
		    r_index = N_TEXT;
		  else if (output_section == obj_datasec (output_bfd))
		    r_index = N_DATA;
		  else if (output_section == obj_bsssec (output_bfd))
		    r_index = N_BSS;
		  else
		    r_index = N_ABS;

		  /* Add the symbol value and the section VMA to the
		     addend stored in the contents.  */
		  relocation = (h->root.u.def.value
				+ output_section->vma
				+ h->root.u.def.section->output_offset);
		}
	      else
		{
		  /* We must change r_index according to the symbol
		     map.  */
		  r_index = symbol_map[r_index];

		  if (r_index == -1)
		    {
		      if (h != NULL)
			{
			  /* We decided to strip this symbol, but it
                             turns out that we can't.  Note that we
                             lose the other and desc information here.
                             I don't think that will ever matter for a
                             global symbol.  */
			  if (h->indx < 0)
			    {
			      h->indx = -2;
			      h->written = false;
			      if (! aout_link_write_other_symbol (h,
								  (PTR) finfo))
				return false;
			    }
			  r_index = h->indx;
			}
		      else
			{
			  const char *name;

			  name = strings + GET_WORD (input_bfd,
						     syms[r_index].e_strx);
			  if (! ((*finfo->info->callbacks->unattached_reloc)
				 (finfo->info, name, input_bfd, input_section,
				  r_addr)))
			    return false;
			  r_index = 0;
			}
		    }

		  relocation = 0;
		}

	      /* Write out the new r_index value.  */
	      if (bfd_header_big_endian (output_bfd))
		{
		  rel->r_index[0] = r_index >> 16;
		  rel->r_index[1] = r_index >> 8;
		  rel->r_index[2] = r_index;
		}
	      else
		{
		  rel->r_index[2] = r_index >> 16;
		  rel->r_index[1] = r_index >> 8;
		  rel->r_index[0] = r_index;
		}
	    }
	  else
	    {
	      asection *section;

	      /* This is a relocation against a section.  We must
		 adjust by the amount that the section moved.  */
	      section = aout_reloc_index_to_section (input_bfd, r_index);
	      relocation = (section->output_section->vma
			    + section->output_offset
			    - section->vma);
	    }

	  /* Change the address of the relocation.  */
	  PUT_WORD (output_bfd,
		    r_addr + input_section->output_offset,
		    rel->r_address);

	  /* Adjust a PC relative relocation by removing the reference
	     to the original address in the section and including the
	     reference to the new address.  */
	  if (r_pcrel)
	    relocation -= (input_section->output_section->vma
			   + input_section->output_offset
			   - input_section->vma);

#ifdef MY_relocatable_reloc
	  MY_relocatable_reloc (howto, output_bfd, rel, relocation, r_addr);
#endif

	  if (relocation == 0)
	    r = bfd_reloc_ok;
	  else
	    r = MY_relocate_contents (howto,
					input_bfd, relocation,
					contents + r_addr);
	}
      else
	{
	  boolean hundef;

	  /* We are generating an executable, and must do a full
	     relocation.  */
	  hundef = false;

	  if (r_extern)
	    {
	      h = sym_hashes[r_index];

	      if (h != (struct aout_link_hash_entry *) NULL
		  && (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak))
		{
		  relocation = (h->root.u.def.value
				+ h->root.u.def.section->output_section->vma
				+ h->root.u.def.section->output_offset);
		}
	      else if (h != (struct aout_link_hash_entry *) NULL
		       && h->root.type == bfd_link_hash_undefweak)
		relocation = 0;
	      else
		{
		  hundef = true;
		  relocation = 0;
		}
	    }
	  else
	    {
	      asection *section;

	      section = aout_reloc_index_to_section (input_bfd, r_index);
	      relocation = (section->output_section->vma
			    + section->output_offset
			    - section->vma);
	      if (r_pcrel)
		relocation += input_section->vma;
	    }

	  if (check_dynamic_reloc != NULL)
	    {
	      boolean skip;

	      if (! ((*check_dynamic_reloc)
		     (finfo->info, input_bfd, input_section, h,
		      (PTR) rel, contents, &skip, &relocation)))
		return false;
	      if (skip)
		continue;
	    }

	  /* Now warn if a global symbol is undefined.  We could not
             do this earlier, because check_dynamic_reloc might want
             to skip this reloc.  */
	  if (hundef && ! finfo->info->shared && ! r_baserel)
	    {
	      const char *name;

	      if (h != NULL)
		name = h->root.root.string;
	      else
		name = strings + GET_WORD (input_bfd, syms[r_index].e_strx);
	      if (! ((*finfo->info->callbacks->undefined_symbol)
		     (finfo->info, name, input_bfd, input_section,
		     r_addr, true)))
		return false;
	    }

	  r = MY_final_link_relocate (howto,
				      input_bfd, input_section,
				      contents, r_addr, relocation,
				      (bfd_vma) 0);
	}

      if (r != bfd_reloc_ok)
	{
	  switch (r)
	    {
	    default:
	    case bfd_reloc_outofrange:
	      abort ();
	    case bfd_reloc_overflow:
	      {
		const char *name;

		if (h != NULL)
		  name = h->root.root.string;
		else if (r_extern)
		  name = strings + GET_WORD (input_bfd,
					     syms[r_index].e_strx);
		else
		  {
		    asection *s;

		    s = aout_reloc_index_to_section (input_bfd, r_index);
		    name = bfd_section_name (input_bfd, s);
		  }
		if (! ((*finfo->info->callbacks->reloc_overflow)
		       (finfo->info, name, howto->name,
			(bfd_vma) 0, input_bfd, input_section, r_addr)))
		  return false;
	      }
	      break;
	    }
	}
    }

  return true;
}

/* Relocate an a.out section using extended a.out relocs.  */

static boolean
aout_link_input_section_ext (finfo, input_bfd, input_section, relocs,
			     rel_size, contents)
     struct aout_final_link_info *finfo;
     bfd *input_bfd;
     asection *input_section;
     struct reloc_ext_external *relocs;
     bfd_size_type rel_size;
     bfd_byte *contents;
{
  boolean (*check_dynamic_reloc) PARAMS ((struct bfd_link_info *,
					  bfd *, asection *,
					  struct aout_link_hash_entry *,
					  PTR, bfd_byte *, boolean *,
					  bfd_vma *));
  bfd *output_bfd;
  boolean relocateable;
  struct external_nlist *syms;
  char *strings;
  struct aout_link_hash_entry **sym_hashes;
  int *symbol_map;
  bfd_size_type reloc_count;
  register struct reloc_ext_external *rel;
  struct reloc_ext_external *rel_end;

  output_bfd = finfo->output_bfd;
  check_dynamic_reloc = aout_backend_info (output_bfd)->check_dynamic_reloc;

  BFD_ASSERT (obj_reloc_entry_size (input_bfd) == RELOC_EXT_SIZE);
  BFD_ASSERT (input_bfd->xvec->header_byteorder
	      == output_bfd->xvec->header_byteorder);

  relocateable = finfo->info->relocateable;
  syms = obj_aout_external_syms (input_bfd);
  strings = obj_aout_external_strings (input_bfd);
  sym_hashes = obj_aout_sym_hashes (input_bfd);
  symbol_map = finfo->symbol_map;

  reloc_count = rel_size / RELOC_EXT_SIZE;
  rel = relocs;
  rel_end = rel + reloc_count;
  for (; rel < rel_end; rel++)
    {
      bfd_vma r_addr;
      int r_index;
      int r_extern;
      unsigned int r_type;
      bfd_vma r_addend;
      struct aout_link_hash_entry *h = NULL;
      asection *r_section = NULL;
      bfd_vma relocation;

      r_addr = GET_SWORD (input_bfd, rel->r_address);

      if (bfd_header_big_endian (input_bfd))
	{
	  r_index  = ((rel->r_index[0] << 16)
		      | (rel->r_index[1] << 8)
		      | rel->r_index[2]);
	  r_extern = (0 != (rel->r_type[0] & RELOC_EXT_BITS_EXTERN_BIG));
	  r_type   = ((rel->r_type[0] & RELOC_EXT_BITS_TYPE_BIG)
		      >> RELOC_EXT_BITS_TYPE_SH_BIG);
	}
      else
	{
	  r_index  = ((rel->r_index[2] << 16)
		      | (rel->r_index[1] << 8)
		      | rel->r_index[0]);
	  r_extern = (0 != (rel->r_type[0] & RELOC_EXT_BITS_EXTERN_LITTLE));
	  r_type   = ((rel->r_type[0] & RELOC_EXT_BITS_TYPE_LITTLE)
		      >> RELOC_EXT_BITS_TYPE_SH_LITTLE);
	}

      r_addend = GET_SWORD (input_bfd, rel->r_addend);

      BFD_ASSERT (r_type < TABLE_SIZE (howto_table_ext));

      if (relocateable)
	{
	  /* We are generating a relocateable output file, and must
	     modify the reloc accordingly.  */
	  if (r_extern
	      || r_type == RELOC_BASE10
	      || r_type == RELOC_BASE13
	      || r_type == RELOC_BASE22)
	    {
	      /* If we know the symbol this relocation is against,
		 convert it into a relocation against a section.  This
		 is what the native linker does.  */
	      if (r_type == RELOC_BASE10
		  || r_type == RELOC_BASE13
		  || r_type == RELOC_BASE22)
		h = NULL;
	      else
		h = sym_hashes[r_index];
	      if (h != (struct aout_link_hash_entry *) NULL
		  && (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak))
		{
		  asection *output_section;

		  /* Change the r_extern value.  */
		  if (bfd_header_big_endian (output_bfd))
		    rel->r_type[0] &=~ RELOC_EXT_BITS_EXTERN_BIG;
		  else
		    rel->r_type[0] &=~ RELOC_EXT_BITS_EXTERN_LITTLE;

		  /* Compute a new r_index.  */
		  output_section = h->root.u.def.section->output_section;
		  if (output_section == obj_textsec (output_bfd))
		    r_index = N_TEXT;
		  else if (output_section == obj_datasec (output_bfd))
		    r_index = N_DATA;
		  else if (output_section == obj_bsssec (output_bfd))
		    r_index = N_BSS;
		  else
		    r_index = N_ABS;

		  /* Add the symbol value and the section VMA to the
		     addend.  */
		  relocation = (h->root.u.def.value
				+ output_section->vma
				+ h->root.u.def.section->output_offset);

		  /* Now RELOCATION is the VMA of the final
		     destination.  If this is a PC relative reloc,
		     then ADDEND is the negative of the source VMA.
		     We want to set ADDEND to the difference between
		     the destination VMA and the source VMA, which
		     means we must adjust RELOCATION by the change in
		     the source VMA.  This is done below.  */
		}
	      else
		{
		  /* We must change r_index according to the symbol
		     map.  */
		  r_index = symbol_map[r_index];

		  if (r_index == -1)
		    {
		      if (h != NULL)
			{
			  /* We decided to strip this symbol, but it
                             turns out that we can't.  Note that we
                             lose the other and desc information here.
                             I don't think that will ever matter for a
                             global symbol.  */
			  if (h->indx < 0)
			    {
			      h->indx = -2;
			      h->written = false;
			      if (! aout_link_write_other_symbol (h,
								  (PTR) finfo))
				return false;
			    }
			  r_index = h->indx;
			}
		      else
			{
			  const char *name;

			  name = strings + GET_WORD (input_bfd,
						     syms[r_index].e_strx);
			  if (! ((*finfo->info->callbacks->unattached_reloc)
				 (finfo->info, name, input_bfd, input_section,
				  r_addr)))
			    return false;
			  r_index = 0;
			}
		    }

		  relocation = 0;

		  /* If this is a PC relative reloc, then the addend
		     is the negative of the source VMA.  We must
		     adjust it by the change in the source VMA.  This
		     is done below.  */
		}

	      /* Write out the new r_index value.  */
	      if (bfd_header_big_endian (output_bfd))
		{
		  rel->r_index[0] = r_index >> 16;
		  rel->r_index[1] = r_index >> 8;
		  rel->r_index[2] = r_index;
		}
	      else
		{
		  rel->r_index[2] = r_index >> 16;
		  rel->r_index[1] = r_index >> 8;
		  rel->r_index[0] = r_index;
		}
	    }
	  else
	    {
	      /* This is a relocation against a section.  We must
		 adjust by the amount that the section moved.  */
	      r_section = aout_reloc_index_to_section (input_bfd, r_index);
	      relocation = (r_section->output_section->vma
			    + r_section->output_offset
			    - r_section->vma);

	      /* If this is a PC relative reloc, then the addend is
		 the difference in VMA between the destination and the
		 source.  We have just adjusted for the change in VMA
		 of the destination, so we must also adjust by the
		 change in VMA of the source.  This is done below.  */
	    }

	  /* As described above, we must always adjust a PC relative
	     reloc by the change in VMA of the source.  However, if
	     pcrel_offset is set, then the addend does not include the
	     location within the section, in which case we don't need
	     to adjust anything.  */
	  if (howto_table_ext[r_type].pc_relative
	      && ! howto_table_ext[r_type].pcrel_offset)
	    relocation -= (input_section->output_section->vma
			   + input_section->output_offset
			   - input_section->vma);

	  /* Change the addend if necessary.  */
	  if (relocation != 0)
	    PUT_WORD (output_bfd, r_addend + relocation, rel->r_addend);

	  /* Change the address of the relocation.  */
	  PUT_WORD (output_bfd,
		    r_addr + input_section->output_offset,
		    rel->r_address);
	}
      else
	{
	  boolean hundef;
	  bfd_reloc_status_type r;

	  /* We are generating an executable, and must do a full
	     relocation.  */
	  hundef = false;

	  if (r_extern)
	    {
	      h = sym_hashes[r_index];

	      if (h != (struct aout_link_hash_entry *) NULL
		  && (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak))
		{
		  relocation = (h->root.u.def.value
				+ h->root.u.def.section->output_section->vma
				+ h->root.u.def.section->output_offset);
		}
	      else if (h != (struct aout_link_hash_entry *) NULL
		       && h->root.type == bfd_link_hash_undefweak)
		relocation = 0;
	      else
		{
		  hundef = true;
		  relocation = 0;
		}
	    }
	  else if (r_type == RELOC_BASE10
		   || r_type == RELOC_BASE13
		   || r_type == RELOC_BASE22)
	    {
	      struct external_nlist *sym;
	      int type;

	      /* For base relative relocs, r_index is always an index
                 into the symbol table, even if r_extern is 0.  */
	      sym = syms + r_index;
	      type = bfd_h_get_8 (input_bfd, sym->e_type);
	      if ((type & N_TYPE) == N_TEXT
		  || type == N_WEAKT)
		r_section = obj_textsec (input_bfd);
	      else if ((type & N_TYPE) == N_DATA
		       || type == N_WEAKD)
		r_section = obj_datasec (input_bfd);
	      else if ((type & N_TYPE) == N_BSS
		       || type == N_WEAKB)
		r_section = obj_bsssec (input_bfd);
	      else if ((type & N_TYPE) == N_ABS
		       || type == N_WEAKA)
		r_section = bfd_abs_section_ptr;
	      else
		abort ();
	      relocation = (r_section->output_section->vma
			    + r_section->output_offset
			    + (GET_WORD (input_bfd, sym->e_value)
			       - r_section->vma));
	    }
	  else
	    {
	      r_section = aout_reloc_index_to_section (input_bfd, r_index);

	      /* If this is a PC relative reloc, then R_ADDEND is the
		 difference between the two vmas, or
		   old_dest_sec + old_dest_off - (old_src_sec + old_src_off)
		 where
		   old_dest_sec == section->vma
		 and
		   old_src_sec == input_section->vma
		 and
		   old_src_off == r_addr

		 _bfd_final_link_relocate expects RELOCATION +
		 R_ADDEND to be the VMA of the destination minus
		 r_addr (the minus r_addr is because this relocation
		 is not pcrel_offset, which is a bit confusing and
		 should, perhaps, be changed), or
		   new_dest_sec
		 where
		   new_dest_sec == output_section->vma + output_offset
		 We arrange for this to happen by setting RELOCATION to
		   new_dest_sec + old_src_sec - old_dest_sec

		 If this is not a PC relative reloc, then R_ADDEND is
		 simply the VMA of the destination, so we set
		 RELOCATION to the change in the destination VMA, or
		   new_dest_sec - old_dest_sec
		 */
	      relocation = (r_section->output_section->vma
			    + r_section->output_offset
			    - r_section->vma);
	      if (howto_table_ext[r_type].pc_relative)
		relocation += input_section->vma;
	    }

	  if (check_dynamic_reloc != NULL)
	    {
	      boolean skip;

	      if (! ((*check_dynamic_reloc)
		     (finfo->info, input_bfd, input_section, h,
		      (PTR) rel, contents, &skip, &relocation)))
		return false;
	      if (skip)
		continue;
	    }

	  /* Now warn if a global symbol is undefined.  We could not
             do this earlier, because check_dynamic_reloc might want
             to skip this reloc.  */
	  if (hundef
	      && ! finfo->info->shared
	      && r_type != RELOC_BASE10
	      && r_type != RELOC_BASE13
	      && r_type != RELOC_BASE22)
	    {
	      const char *name;

	      if (h != NULL)
		name = h->root.root.string;
	      else
		name = strings + GET_WORD (input_bfd, syms[r_index].e_strx);
	      if (! ((*finfo->info->callbacks->undefined_symbol)
		     (finfo->info, name, input_bfd, input_section,
		     r_addr, true)))
		return false;
	    }

	  if (r_type != RELOC_SPARC_REV32)
	    r = MY_final_link_relocate (howto_table_ext + r_type,
					input_bfd, input_section,
					contents, r_addr, relocation,
					r_addend);
	  else
	    {
	      bfd_vma x;

	      x = bfd_get_32 (input_bfd, contents + r_addr);
	      x = x + relocation + r_addend;
	      bfd_putl32 (/*input_bfd,*/ x, contents + r_addr);
	      r = bfd_reloc_ok;
	    }

	  if (r != bfd_reloc_ok)
	    {
	      switch (r)
		{
		default:
		case bfd_reloc_outofrange:
		  abort ();
		case bfd_reloc_overflow:
		  {
		    const char *name;

		    if (h != NULL)
		      name = h->root.root.string;
		    else if (r_extern
			     || r_type == RELOC_BASE10
			     || r_type == RELOC_BASE13
			     || r_type == RELOC_BASE22)
		      name = strings + GET_WORD (input_bfd,
						 syms[r_index].e_strx);
		    else
		      {
			asection *s;

			s = aout_reloc_index_to_section (input_bfd, r_index);
			name = bfd_section_name (input_bfd, s);
		      }
		    if (! ((*finfo->info->callbacks->reloc_overflow)
			   (finfo->info, name, howto_table_ext[r_type].name,
			    r_addend, input_bfd, input_section, r_addr)))
		      return false;
		  }
		  break;
		}
	    }
	}
    }

  return true;
}

/* Handle a link order which is supposed to generate a reloc.  */

static boolean
aout_link_reloc_link_order (finfo, o, p)
     struct aout_final_link_info *finfo;
     asection *o;
     struct bfd_link_order *p;
{
  struct bfd_link_order_reloc *pr;
  int r_index;
  int r_extern;
  reloc_howto_type *howto;
  file_ptr *reloff_ptr = NULL;
  struct reloc_std_external srel;
  struct reloc_ext_external erel;
  PTR rel_ptr;

  pr = p->u.reloc.p;

  if (p->type == bfd_section_reloc_link_order)
    {
      r_extern = 0;
      if (bfd_is_abs_section (pr->u.section))
	r_index = N_ABS | N_EXT;
      else
	{
	  BFD_ASSERT (pr->u.section->owner == finfo->output_bfd);
	  r_index = pr->u.section->target_index;
	}
    }
  else
    {
      struct aout_link_hash_entry *h;

      BFD_ASSERT (p->type == bfd_symbol_reloc_link_order);
      r_extern = 1;
      h = ((struct aout_link_hash_entry *)
	   bfd_wrapped_link_hash_lookup (finfo->output_bfd, finfo->info,
					 pr->u.name, false, false, true));
      if (h != (struct aout_link_hash_entry *) NULL
	  && h->indx >= 0)
	r_index = h->indx;
      else if (h != NULL)
	{
	  /* We decided to strip this symbol, but it turns out that we
	     can't.  Note that we lose the other and desc information
	     here.  I don't think that will ever matter for a global
	     symbol.  */
	  h->indx = -2;
	  h->written = false;
	  if (! aout_link_write_other_symbol (h, (PTR) finfo))
	    return false;
	  r_index = h->indx;
	}
      else
	{
	  if (! ((*finfo->info->callbacks->unattached_reloc)
		 (finfo->info, pr->u.name, (bfd *) NULL,
		  (asection *) NULL, (bfd_vma) 0)))
	    return false;
	  r_index = 0;
	}
    }

  howto = bfd_reloc_type_lookup (finfo->output_bfd, pr->reloc);
  if (howto == 0)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  if (o == obj_textsec (finfo->output_bfd))
    reloff_ptr = &finfo->treloff;
  else if (o == obj_datasec (finfo->output_bfd))
    reloff_ptr = &finfo->dreloff;
  else
    abort ();

  if (obj_reloc_entry_size (finfo->output_bfd) == RELOC_STD_SIZE)
    {
#ifdef MY_put_reloc
      MY_put_reloc(finfo->output_bfd, r_extern, r_index, p->offset, howto,
		   &srel);
#else
      {
	int r_pcrel;
	int r_baserel;
	int r_jmptable;
	int r_relative;
	int r_length;

	r_pcrel = howto->pc_relative;
	r_baserel = (howto->type & 8) != 0;
	r_jmptable = (howto->type & 16) != 0;
	r_relative = (howto->type & 32) != 0;
	r_length = howto->size;

	PUT_WORD (finfo->output_bfd, p->offset, srel.r_address);
	if (bfd_header_big_endian (finfo->output_bfd))
	  {
	    srel.r_index[0] = r_index >> 16;
	    srel.r_index[1] = r_index >> 8;
	    srel.r_index[2] = r_index;
	    srel.r_type[0] =
	      ((r_extern ?     RELOC_STD_BITS_EXTERN_BIG : 0)
	       | (r_pcrel ?    RELOC_STD_BITS_PCREL_BIG : 0)
	       | (r_baserel ?  RELOC_STD_BITS_BASEREL_BIG : 0)
	       | (r_jmptable ? RELOC_STD_BITS_JMPTABLE_BIG : 0)
	       | (r_relative ? RELOC_STD_BITS_RELATIVE_BIG : 0)
	       | (r_length <<  RELOC_STD_BITS_LENGTH_SH_BIG));
	  }
	else
	  {
	    srel.r_index[2] = r_index >> 16;
	    srel.r_index[1] = r_index >> 8;
	    srel.r_index[0] = r_index;
	    srel.r_type[0] =
	      ((r_extern ?     RELOC_STD_BITS_EXTERN_LITTLE : 0)
	       | (r_pcrel ?    RELOC_STD_BITS_PCREL_LITTLE : 0)
	       | (r_baserel ?  RELOC_STD_BITS_BASEREL_LITTLE : 0)
	       | (r_jmptable ? RELOC_STD_BITS_JMPTABLE_LITTLE : 0)
	       | (r_relative ? RELOC_STD_BITS_RELATIVE_LITTLE : 0)
	       | (r_length <<  RELOC_STD_BITS_LENGTH_SH_LITTLE));
	  }
      }
#endif
      rel_ptr = (PTR) &srel;

      /* We have to write the addend into the object file, since
	 standard a.out relocs are in place.  It would be more
	 reliable if we had the current contents of the file here,
	 rather than assuming zeroes, but we can't read the file since
	 it was opened using bfd_openw.  */
      if (pr->addend != 0)
	{
	  bfd_size_type size;
	  bfd_reloc_status_type r;
	  bfd_byte *buf;
	  boolean ok;

	  size = bfd_get_reloc_size (howto);
	  buf = (bfd_byte *) bfd_zmalloc (size);
	  if (buf == (bfd_byte *) NULL)
	    return false;
	  r = MY_relocate_contents (howto, finfo->output_bfd,
				      pr->addend, buf);
	  switch (r)
	    {
	    case bfd_reloc_ok:
	      break;
	    default:
	    case bfd_reloc_outofrange:
	      abort ();
	    case bfd_reloc_overflow:
	      if (! ((*finfo->info->callbacks->reloc_overflow)
		     (finfo->info,
		      (p->type == bfd_section_reloc_link_order
		       ? bfd_section_name (finfo->output_bfd,
					   pr->u.section)
		       : pr->u.name),
		      howto->name, pr->addend, (bfd *) NULL,
		      (asection *) NULL, (bfd_vma) 0)))
		{
		  free (buf);
		  return false;
		}
	      break;
	    }
	  ok = bfd_set_section_contents (finfo->output_bfd, o,
					 (PTR) buf,
					 (file_ptr) p->offset,
					 size);
	  free (buf);
	  if (! ok)
	    return false;
	}
    }
  else
    {
#ifdef MY_put_ext_reloc
      MY_put_ext_reloc (finfo->output_bfd, r_extern, r_index, p->offset,
			howto, &erel, pr->addend);
#else
      PUT_WORD (finfo->output_bfd, p->offset, erel.r_address);

      if (bfd_header_big_endian (finfo->output_bfd))
	{
	  erel.r_index[0] = r_index >> 16;
	  erel.r_index[1] = r_index >> 8;
	  erel.r_index[2] = r_index;
	  erel.r_type[0] =
	    ((r_extern ? RELOC_EXT_BITS_EXTERN_BIG : 0)
	     | (howto->type << RELOC_EXT_BITS_TYPE_SH_BIG));
	}
      else
	{
	  erel.r_index[2] = r_index >> 16;
	  erel.r_index[1] = r_index >> 8;
	  erel.r_index[0] = r_index;
	  erel.r_type[0] =
	    (r_extern ? RELOC_EXT_BITS_EXTERN_LITTLE : 0)
	      | (howto->type << RELOC_EXT_BITS_TYPE_SH_LITTLE);
	}

      PUT_WORD (finfo->output_bfd, pr->addend, erel.r_addend);
#endif /* MY_put_ext_reloc */

      rel_ptr = (PTR) &erel;
    }

  if (bfd_seek (finfo->output_bfd, *reloff_ptr, SEEK_SET) != 0
      || (bfd_write (rel_ptr, (bfd_size_type) 1,
		     obj_reloc_entry_size (finfo->output_bfd),
		     finfo->output_bfd)
	  != obj_reloc_entry_size (finfo->output_bfd)))
    return false;

  *reloff_ptr += obj_reloc_entry_size (finfo->output_bfd);

  /* Assert that the relocs have not run into the symbols, and that n
     the text relocs have not run into the data relocs.  */
  BFD_ASSERT (*reloff_ptr <= obj_sym_filepos (finfo->output_bfd)
	      && (reloff_ptr != &finfo->treloff
		  || (*reloff_ptr
		      <= obj_datasec (finfo->output_bfd)->rel_filepos)));

  return true;
}
