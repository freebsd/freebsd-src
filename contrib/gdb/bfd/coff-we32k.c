/* BFD back-end for we32k COFF files.
   Copyright (C) 1992, 1993, 1994 Free Software Foundation, Inc.
   Contributed by Brendan Kehoe (brendan@cs.widener.edu).

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "obstack.h"
#include "coff/we32k.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)

static reloc_howto_type howto_table[] = 
{
    {0},
    {1},
    {2},
    {3},
    {4},
    {5},
  HOWTO(R_DIR32,	       0,  2, 	32, false, 0,complain_overflow_bitfield, 0, "dir32",	true, 0xffffffff,0xffffffff, false),
    {7},
    {010},
    {011},
    {012},
    {013},
    {014},
    {015},
    {016},
  HOWTO(R_RELBYTE,	       0,  0,  	8,  false, 0, complain_overflow_bitfield, 0, "8",	true, 0x000000ff,0x000000ff, false),
  HOWTO(R_RELWORD,	       0,  1, 	16, false, 0, complain_overflow_bitfield, 0, "16",	true, 0x0000ffff,0x0000ffff, false),
  HOWTO(R_RELLONG,	       0,  2, 	32, false, 0, complain_overflow_bitfield, 0, "32",	true, 0xffffffff,0xffffffff, false),
  HOWTO(R_PCRBYTE,	       0,  0, 	8,  true,  0, complain_overflow_signed, 0, "DISP8",    true, 0x000000ff,0x000000ff, false),
  HOWTO(R_PCRWORD,	       0,  1, 	16, true,  0, complain_overflow_signed, 0, "DISP16",   true, 0x0000ffff,0x0000ffff, false),
  HOWTO(R_PCRLONG,	       0,  2, 	32, true,  0, complain_overflow_signed, 0, "DISP32",   true, 0xffffffff,0xffffffff, false),
};

/* Turn a howto into a reloc  nunmber */

#define SELECT_RELOC(x,howto) { x.r_type = howto->type; }
#define BADMAG(x) WE32KBADMAG(x)
#define WE32K	1

#define RTYPE2HOWTO(cache_ptr, dst) \
	    (cache_ptr)->howto = howto_table + (dst)->r_type;

#include "coffcode.h"

#define coff_write_armap bsd_write_armap

const bfd_target we32kcoff_vec =
{
  "coff-we32k",			/* name */
  bfd_target_coff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  0,				/* leading underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */

  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

 {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
 {bfd_false, coff_mkobject, _bfd_generic_mkarchive, /* bfd_set_format */
   bfd_false},
 {bfd_false, coff_write_object_contents, /* bfd_write_contents */
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

  COFF_SWAP_TABLE,
};
