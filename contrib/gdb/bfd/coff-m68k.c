/* BFD back-end for Motorola 68000 COFF binaries.
   Copyright 1990, 91, 92, 93, 94, 95, 1996 Free Software Foundation, Inc.
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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "obstack.h"
#include "coff/m68k.h"
#include "coff/internal.h"
#include "libcoff.h"

#ifndef LYNX_SPECIAL_FN
#define LYNX_SPECIAL_FN 0
#endif

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)

#ifndef COFF_PAGE_SIZE
/* The page size is a guess based on ELF.  */
#define COFF_PAGE_SIZE 0x2000
#endif

/* Clean up namespace.  */
#define m68kcoff_howto_table	_bfd_m68kcoff_howto_table
#define m68k_rtype2howto	_bfd_m68kcoff_rtype2howto
#define m68k_howto2rtype	_bfd_m68kcoff_howto2rtype
#define m68k_reloc_type_lookup	_bfd_m68kcoff_reloc_type_lookup

#ifdef ONLY_DECLARE_RELOCS
extern reloc_howto_type m68kcoff_howto_table[];
#else
reloc_howto_type m68kcoff_howto_table[] = 
{
  HOWTO(R_RELBYTE,	       0,  0,  	8,  false, 0, complain_overflow_bitfield, LYNX_SPECIAL_FN, "8",	true, 0x000000ff,0x000000ff, false),
  HOWTO(R_RELWORD,	       0,  1, 	16, false, 0, complain_overflow_bitfield, LYNX_SPECIAL_FN, "16",	true, 0x0000ffff,0x0000ffff, false),
  HOWTO(R_RELLONG,	       0,  2, 	32, false, 0, complain_overflow_bitfield, LYNX_SPECIAL_FN, "32",	true, 0xffffffff,0xffffffff, false),
  HOWTO(R_PCRBYTE,	       0,  0, 	8,  true,  0, complain_overflow_signed, LYNX_SPECIAL_FN, "DISP8",    true, 0x000000ff,0x000000ff, false),
  HOWTO(R_PCRWORD,	       0,  1, 	16, true,  0, complain_overflow_signed, LYNX_SPECIAL_FN, "DISP16",   true, 0x0000ffff,0x0000ffff, false),
  HOWTO(R_PCRLONG,	       0,  2, 	32, true,  0, complain_overflow_signed, LYNX_SPECIAL_FN, "DISP32",   true, 0xffffffff,0xffffffff, false),
  HOWTO(R_RELLONG_NEG,	       0,  -2, 	32, false, 0, complain_overflow_bitfield, LYNX_SPECIAL_FN, "-32",	true, 0xffffffff,0xffffffff, false),
};
#endif /* not ONLY_DECLARE_RELOCS */

#ifndef BADMAG
#define BADMAG(x) M68KBADMAG(x)
#endif
#define M68 1		/* Customize coffcode.h */

/* Turn a howto into a reloc number */

#ifdef ONLY_DECLARE_RELOCS
extern void m68k_rtype2howto PARAMS ((arelent *internal, int relocentry));
extern int m68k_howto2rtype PARAMS ((reloc_howto_type *));
extern reloc_howto_type *m68k_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
#else
void
m68k_rtype2howto(internal, relocentry)
     arelent *internal;
     int relocentry;
{
  switch (relocentry) 
  {
   case R_RELBYTE:	internal->howto = m68kcoff_howto_table + 0; break;
   case R_RELWORD:	internal->howto = m68kcoff_howto_table + 1; break;
   case R_RELLONG:	internal->howto = m68kcoff_howto_table + 2; break;
   case R_PCRBYTE:	internal->howto = m68kcoff_howto_table + 3; break;
   case R_PCRWORD:	internal->howto = m68kcoff_howto_table + 4; break;
   case R_PCRLONG:	internal->howto = m68kcoff_howto_table + 5; break;
   case R_RELLONG_NEG:	internal->howto = m68kcoff_howto_table + 6; break;
  }
}

int 
m68k_howto2rtype (internal)
     reloc_howto_type *internal;
{
  if (internal->pc_relative) 
  {
    switch (internal->bitsize) 
    {
     case 32: return R_PCRLONG;
     case 16: return R_PCRWORD;
     case 8: return R_PCRBYTE;
    }
  }
  else 
  {
    switch (internal->bitsize) 
     {
      case 32: return R_RELLONG;
      case 16: return R_RELWORD;
      case 8: return R_RELBYTE;
     }
  }
  return R_RELLONG;    
}

reloc_howto_type *
m68k_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    default:			return NULL;
    case BFD_RELOC_8:		return m68kcoff_howto_table + 0;
    case BFD_RELOC_16:		return m68kcoff_howto_table + 1;
    case BFD_RELOC_CTOR:
    case BFD_RELOC_32:		return m68kcoff_howto_table + 2;
    case BFD_RELOC_8_PCREL:	return m68kcoff_howto_table + 3;
    case BFD_RELOC_16_PCREL:	return m68kcoff_howto_table + 4;
    case BFD_RELOC_32_PCREL:	return m68kcoff_howto_table + 5;
      /* FIXME: There doesn't seem to be a code for R_RELLONG_NEG.  */
    }
  /*NOTREACHED*/
}

#endif /* not ONLY_DECLARE_RELOCS */

#define RTYPE2HOWTO(internal, relocentry) \
  m68k_rtype2howto(internal, (relocentry)->r_type)

#define SELECT_RELOC(external, internal) \
  external.r_type = m68k_howto2rtype(internal);

#define coff_bfd_reloc_type_lookup m68k_reloc_type_lookup

#define coff_relocate_section _bfd_coff_generic_relocate_section

#include "coffcode.h"

const bfd_target 
#ifdef TARGET_SYM
  TARGET_SYM =
#else
  m68kcoff_vec =
#endif
{
#ifdef TARGET_NAME
  TARGET_NAME,
#else
  "coff-m68k",			/* name */
#endif
  bfd_target_coff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
#ifdef NAMES_HAVE_UNDERSCORE
  '_',
#else
  0,				/* leading underscore */
#endif
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

  COFF_SWAP_TABLE
 };
