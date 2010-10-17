/* BFD back-end for TMS320C30 coff binaries.
   Copyright 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Steven Haworth (steve@pm.cse.rmit.edu.au)

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/tic30.h"
#include "coff/internal.h"
#include "libcoff.h"

static int  coff_tic30_select_reloc PARAMS ((reloc_howto_type *));
static void rtype2howto PARAMS ((arelent *, struct internal_reloc *));
static void reloc_processing PARAMS ((arelent *, struct internal_reloc *, asymbol **, bfd *, asection *));

reloc_howto_type * tic30_coff_reloc_type_lookup PARAMS ((bfd *, bfd_reloc_code_real_type));

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (1)

reloc_howto_type tic30_coff_howto_table[] =
  {
    HOWTO (R_TIC30_ABS16, 2, 1, 16, FALSE, 0, 0, NULL,
	   "16", FALSE, 0x0000FFFF, 0x0000FFFF, FALSE),
    HOWTO (R_TIC30_ABS24, 2, 2, 24, FALSE, 8, complain_overflow_bitfield, NULL,
	   "24", FALSE, 0xFFFFFF00, 0xFFFFFF00, FALSE),
    HOWTO (R_TIC30_LDP, 18, 0, 24, FALSE, 0, complain_overflow_bitfield, NULL,
	   "LDP", FALSE, 0x00FF0000, 0x000000FF, FALSE),
    HOWTO (R_TIC30_ABS32, 2, 2, 32, FALSE, 0, complain_overflow_bitfield, NULL,
	   "32", FALSE, 0xFFFFFFFF, 0xFFFFFFFF, FALSE),
    HOWTO (R_TIC30_PC16, 2, 1, 16, TRUE, 0, complain_overflow_signed, NULL,
	   "PCREL", FALSE, 0x0000FFFF, 0x0000FFFF, FALSE),
    EMPTY_HOWTO (-1)
  };

#ifndef coff_bfd_reloc_type_lookup
#define coff_bfd_reloc_type_lookup tic30_coff_reloc_type_lookup

/* For the case statement use the code values used in tc_gen_reloc to
   map to the howto table entries that match those in both the aout
   and coff implementations.  */

reloc_howto_type *
tic30_coff_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_8:
    case BFD_RELOC_TIC30_LDP:
      return &tic30_coff_howto_table[2];
    case BFD_RELOC_16:
      return &tic30_coff_howto_table[0];
    case BFD_RELOC_24:
      return &tic30_coff_howto_table[1];
    case BFD_RELOC_16_PCREL:
      return &tic30_coff_howto_table[4];
    case BFD_RELOC_32:
      return &tic30_coff_howto_table[3];
    default:
      return (reloc_howto_type *) NULL;
    }
}

#endif

/* Turn a howto into a reloc number.  */

static int
coff_tic30_select_reloc (howto)
     reloc_howto_type *howto;
{
  return howto->type;
}

#define SELECT_RELOC(x,howto) x.r_type = coff_tic30_select_reloc(howto)

#define BADMAG(x) TIC30BADMAG(x)
#define TIC30 1			/* Customize coffcode.h */
#define __A_MAGIC_SET__

/* Code to swap in the reloc */
#define SWAP_IN_RELOC_OFFSET  H_GET_32
#define SWAP_OUT_RELOC_OFFSET H_PUT_32
#define SWAP_OUT_RELOC_EXTRA(abfd, src, dst) dst->r_stuff[0] = 'S'; \
dst->r_stuff[1] = 'C';

/* Code to turn a r_type into a howto ptr, uses the above howto table.  */

static void
rtype2howto (internal, dst)
     arelent *internal;
     struct internal_reloc *dst;
{
  switch (dst->r_type)
    {
    case R_TIC30_ABS16:
      internal->howto = &tic30_coff_howto_table[0];
      break;
    case R_TIC30_ABS24:
      internal->howto = &tic30_coff_howto_table[1];
      break;
    case R_TIC30_ABS32:
      internal->howto = &tic30_coff_howto_table[3];
      break;
    case R_TIC30_LDP:
      internal->howto = &tic30_coff_howto_table[2];
      break;
    case R_TIC30_PC16:
      internal->howto = &tic30_coff_howto_table[4];
      break;
    default:
      abort ();
      break;
    }
}

#define RTYPE2HOWTO(internal, relocentry) rtype2howto (internal, relocentry)

/* Perform any necessary magic to the addend in a reloc entry */

#define CALC_ADDEND(abfd, symbol, ext_reloc, cache_ptr) \
 cache_ptr->addend =  ext_reloc.r_offset;

#define RELOC_PROCESSING(relent,reloc,symbols,abfd,section) \
 reloc_processing(relent, reloc, symbols, abfd, section)

static void
reloc_processing (relent, reloc, symbols, abfd, section)
     arelent *relent;
     struct internal_reloc *reloc;
     asymbol **symbols;
     bfd *abfd;
     asection *section;
{
  relent->address = reloc->r_vaddr;
  rtype2howto (relent, reloc);

  if (reloc->r_symndx > 0)
    relent->sym_ptr_ptr = symbols + obj_convert (abfd)[reloc->r_symndx];
  else
    relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;

  relent->addend = reloc->r_offset;
  relent->address -= section->vma;
}

#include "coffcode.h"

const bfd_target tic30_coff_vec =
{
  "coff-tic30",			/* name */
  bfd_target_coff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  '_',				/* leading symbol underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* hdrs */

  {_bfd_dummy_target, coff_object_p,	/* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive,	/* bfd_set_format */
   bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (coff),
  BFD_JUMP_TABLE_COPY (coff),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
  BFD_JUMP_TABLE_SYMBOLS (coff),
  BFD_JUMP_TABLE_RELOCS (coff),
  BFD_JUMP_TABLE_WRITE (coff),
  BFD_JUMP_TABLE_LINK (coff),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  COFF_SWAP_TABLE
};
