/* Target definitions for 32/64-bit NLM (NetWare Loadable Module)
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.

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

#define nlm_core_file_p _bfd_dummy_target

#define nlm_get_symtab_upper_bound nlmNAME(get_symtab_upper_bound)
#define nlm_get_symtab nlmNAME(get_symtab)
#define nlm_make_empty_symbol nlmNAME(make_empty_symbol)
#define nlm_print_symbol nlmNAME(print_symbol)
#define nlm_get_symbol_info nlmNAME(get_symbol_info)
#define nlm_bfd_is_local_label bfd_generic_is_local_label
#define nlm_get_lineno _bfd_nosymbols_get_lineno
#define nlm_find_nearest_line _bfd_nosymbols_find_nearest_line
#define nlm_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define nlm_read_minisymbols _bfd_generic_read_minisymbols
#define nlm_minisymbol_to_symbol _bfd_generic_minisymbol_to_symbol

#define nlm_get_reloc_upper_bound nlmNAME(get_reloc_upper_bound)
#define nlm_canonicalize_reloc nlmNAME(canonicalize_reloc)
#define nlm_bfd_reloc_type_lookup bfd_default_reloc_type_lookup

#define nlm_set_section_contents nlmNAME(set_section_contents)

#define nlm_sizeof_headers _bfd_nolink_sizeof_headers
#define nlm_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define nlm_bfd_relax_section bfd_generic_relax_section
#define nlm_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define nlm_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define nlm_bfd_final_link _bfd_generic_final_link
#define nlm_bfd_link_split_section _bfd_generic_link_split_section

/* This structure contains everything that BFD knows about a target.
   It includes things like its byte order, name, what routines to call
   to do various operations, etc.  Every BFD points to a target structure
   with its "xvec" member.

   There are two such structures here:  one for big-endian machines and
   one for little-endian machines.   */


#ifdef TARGET_BIG_SYM
const bfd_target TARGET_BIG_SYM =
{
  /* name: identify kind of target */
  TARGET_BIG_NAME,

  /* flavour: general indication about file */
  bfd_target_nlm_flavour,

  /* byteorder: data is big endian */
  BFD_ENDIAN_BIG,

  /* header_byteorder: header is also big endian */
  BFD_ENDIAN_BIG,

  /* object_flags: mask of all file flags */
  (HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS
   | WP_TEXT),
  
  /* section_flags: mask of all section flags */
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY |
   SEC_CODE | SEC_DATA), 

   /* leading_symbol_char: is the first char of a user symbol
      predictable, and if so what is it */
   0,

  /* ar_pad_char: pad character for filenames within an archive header
     FIXME:  this really has nothing to do with NLM, this is a characteristic
     of the archiver and/or os and should be independently tunable */
  '/',

  /* ar_max_namelen: maximum number of characters in an archive header
     FIXME:  this really has nothing to do with NLM, this is a characteristic
     of the archiver and should be independently tunable.  This value is
     a WAG (wild a** guess) */
  15,

  /* Routines to byte-swap various sized integers from the data sections */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
    bfd_getb32, bfd_getb_signed_32, bfd_putb32,
    bfd_getb16, bfd_getb_signed_16, bfd_putb16,

  /* Routines to byte-swap various sized integers from the file headers */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
    bfd_getb32, bfd_getb_signed_32, bfd_putb32,
    bfd_getb16, bfd_getb_signed_16, bfd_putb16,

  /* bfd_check_format: check the format of a file being read */
  { _bfd_dummy_target,		/* unknown format */
    nlmNAME(object_p),		/* assembler/linker output (object file) */
    bfd_generic_archive_p,	/* an archive */
    nlm_core_file_p		/* a core file */
  },

  /* bfd_set_format: set the format of a file being written */
  { bfd_false,
    nlm_mkobject,
    _bfd_generic_mkarchive,
    bfd_false
  },

  /* bfd_write_contents: write cached information into a file being written */
  { bfd_false,
    nlmNAME(write_object_contents),
    _bfd_write_archive_contents,
    bfd_false
  },

  /* Initialize a jump table with the standard macro.  All names start with
     "nlm" */
  BFD_JUMP_TABLE_GENERIC (_bfd_generic),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (nlm),
  BFD_JUMP_TABLE_RELOCS (nlm),
  BFD_JUMP_TABLE_WRITE (nlm),
  BFD_JUMP_TABLE_LINK (nlm),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  /* backend_data: */
  (PTR) TARGET_BACKEND_DATA
};
#endif

#ifdef TARGET_LITTLE_SYM
const bfd_target TARGET_LITTLE_SYM =
{
  /* name: identify kind of target */
  TARGET_LITTLE_NAME,

  /* flavour: general indication about file */
  bfd_target_nlm_flavour,

  /* byteorder: data is little endian */
  BFD_ENDIAN_LITTLE,

  /* header_byteorder: header is also little endian */
  BFD_ENDIAN_LITTLE,

  /* object_flags: mask of all file flags */
  (HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS
   | WP_TEXT),
  
  /* section_flags: mask of all section flags */
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY |
   SEC_DATA), 

   /* leading_symbol_char: is the first char of a user symbol
      predictable, and if so what is it */
   0,

  /* ar_pad_char: pad character for filenames within an archive header
     FIXME:  this really has nothing to do with NLM, this is a characteristic
     of the archiver and/or os and should be independently tunable */
  '/',

  /* ar_max_namelen: maximum number of characters in an archive header
     FIXME:  this really has nothing to do with NLM, this is a characteristic
     of the archiver and should be independently tunable.  This value is
     a WAG (wild a** guess) */
  15,

  /* Routines to byte-swap various sized integers from the data sections */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    bfd_getl32, bfd_getl_signed_32, bfd_putl32,
    bfd_getl16, bfd_getl_signed_16, bfd_putl16,

  /* Routines to byte-swap various sized integers from the file headers */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    bfd_getl32, bfd_getl_signed_32, bfd_putl32,
    bfd_getl16, bfd_getl_signed_16, bfd_putl16,

  /* bfd_check_format: check the format of a file being read */
  { _bfd_dummy_target,		/* unknown format */
    nlmNAME(object_p),		/* assembler/linker output (object file) */
    bfd_generic_archive_p,	/* an archive */
    nlm_core_file_p		/* a core file */
  },

  /* bfd_set_format: set the format of a file being written */
  { bfd_false,
    nlm_mkobject,
    _bfd_generic_mkarchive,
    bfd_false
  },

  /* bfd_write_contents: write cached information into a file being written */
  { bfd_false,
    nlmNAME(write_object_contents),
    _bfd_write_archive_contents,
    bfd_false
  },

  /* Initialize a jump table with the standard macro.  All names start with
     "nlm" */
  BFD_JUMP_TABLE_GENERIC (_bfd_generic),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (nlm),
  BFD_JUMP_TABLE_RELOCS (nlm),
  BFD_JUMP_TABLE_WRITE (nlm),
  BFD_JUMP_TABLE_LINK (nlm),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  /* backend_data: */
  (PTR) TARGET_BACKEND_DATA
};
#endif
