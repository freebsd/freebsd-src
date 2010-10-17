/* Mach-O support for BFD.
   Copyright 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

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

#ifndef TARGET_NAME
#error TARGET_NAME must be defined
#endif /* TARGET_NAME */

#ifndef TARGET_STRING
#error TARGET_STRING must be defined
#endif /* TARGET_STRING */

#ifndef TARGET_BIG_ENDIAN
#error TARGET_BIG_ENDIAN must be defined
#endif /* TARGET_BIG_ENDIAN */

#ifndef TARGET_ARCHIVE
#error TARGET_ARCHIVE must be defined
#endif /* TARGET_ARCHIVE */

#if ((TARGET_ARCHIVE) && (! TARGET_BIG_ENDIAN))
#error Mach-O fat files must always be big-endian.
#endif /* ((TARGET_ARCHIVE) && (! TARGET_BIG_ENDIAN)) */

const bfd_target TARGET_NAME =
{
  TARGET_STRING,		/* Name.  */
  bfd_target_mach_o_flavour,
#if TARGET_BIG_ENDIAN
  BFD_ENDIAN_BIG,		/* Target byte order.  */
  BFD_ENDIAN_BIG,		/* Target headers byte order.  */
#else
  BFD_ENDIAN_LITTLE,		/* Target byte order.  */
  BFD_ENDIAN_LITTLE,		/* Target headers byte order.  */
#endif
  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | DYNAMIC | WP_TEXT | D_PAGED),
  (SEC_CODE | SEC_DATA | SEC_ROM | SEC_HAS_CONTENTS
   | SEC_ALLOC | SEC_LOAD | SEC_RELOC),	/* Section flags.  */
  '_',				/* symbol_leading_char.  */
  ' ',				/* ar_pad_char.  */
  16,				/* ar_max_namelen.  */

#if TARGET_BIG_ENDIAN
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Data.  */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Hdrs.  */
#else
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* hdrs */
#endif /* TARGET_BIG_ENDIAN */

  {				/* bfd_check_format.  */
#if TARGET_ARCHIVE
    _bfd_dummy_target,
    _bfd_dummy_target,
    bfd_mach_o_archive_p,
    _bfd_dummy_target,
#else
    _bfd_dummy_target,
    bfd_mach_o_object_p,
    _bfd_dummy_target,
    bfd_mach_o_core_p
#endif
  },
  {				/* bfd_set_format.  */
    bfd_false,
    bfd_mach_o_mkobject,
    bfd_false,
    bfd_mach_o_mkobject,
  },
  {				/* bfd_write_contents.  */
    bfd_false,
    bfd_mach_o_write_contents,
    bfd_false,
    bfd_mach_o_write_contents,
  },

  BFD_JUMP_TABLE_GENERIC (bfd_mach_o),
  BFD_JUMP_TABLE_COPY (bfd_mach_o),
  BFD_JUMP_TABLE_CORE (bfd_mach_o),
  BFD_JUMP_TABLE_ARCHIVE (bfd_mach_o),
  BFD_JUMP_TABLE_SYMBOLS (bfd_mach_o),
  BFD_JUMP_TABLE_RELOCS (bfd_mach_o),
  BFD_JUMP_TABLE_WRITE (bfd_mach_o),
  BFD_JUMP_TABLE_LINK (bfd_mach_o),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  NULL
};

