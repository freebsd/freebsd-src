/* Target definitions for 32-bit ELF
   Copyright 1993, 1994 Free Software Foundation, Inc.

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

/* This structure contains everything that BFD knows about a target.
   It includes things like its byte order, name, what routines to call
   to do various operations, etc.  Every BFD points to a target structure
   with its "xvec" member.

   There are two such structures here:  one for big-endian machines and
   one for little-endian machines.   */

#define	bfd_elf32_close_and_cleanup _bfd_generic_close_and_cleanup
#define bfd_elf32_bfd_free_cached_info _bfd_generic_bfd_free_cached_info
#ifndef bfd_elf32_get_section_contents
#define bfd_elf32_get_section_contents _bfd_generic_get_section_contents
#endif

#define bfd_elf32_bfd_get_relocated_section_contents \
 bfd_generic_get_relocated_section_contents
#define bfd_elf32_bfd_relax_section bfd_generic_relax_section
#define bfd_elf32_bfd_make_debug_symbol \
  ((asymbol *(*) PARAMS ((bfd *, void *, unsigned long))) bfd_nullvoidptr)

#ifndef bfd_elf32_bfd_copy_private_section_data
#define bfd_elf32_bfd_copy_private_section_data \
  ((boolean (*) PARAMS ((bfd *, asection *, bfd *, asection *))) bfd_true)
#endif
#ifndef bfd_elf32_bfd_copy_private_bfd_data
#define bfd_elf32_bfd_copy_private_bfd_data \
  ((boolean (*) PARAMS ((bfd *, bfd *))) bfd_true)
#endif
#ifndef bfd_elf32_bfd_is_local_label
#define bfd_elf32_bfd_is_local_label bfd_generic_is_local_label
#endif

#ifndef bfd_elf32_get_dynamic_reloc_upper_bound
#define bfd_elf32_get_dynamic_reloc_upper_bound \
  _bfd_nodynamic_get_dynamic_reloc_upper_bound
#endif
#ifndef bfd_elf32_canonicalize_dynamic_reloc
#define bfd_elf32_canonicalize_dynamic_reloc \
  _bfd_nodynamic_canonicalize_dynamic_reloc
#endif

#ifdef elf_backend_relocate_section
#ifndef bfd_elf32_bfd_link_hash_table_create
#define bfd_elf32_bfd_link_hash_table_create _bfd_elf_link_hash_table_create
#endif
#else /* ! defined (elf_backend_relocate_section) */
/* If no backend relocate_section routine, use the generic linker.  */
#ifndef bfd_elf32_bfd_link_hash_table_create
#define bfd_elf32_bfd_link_hash_table_create \
  _bfd_generic_link_hash_table_create
#endif
#ifndef bfd_elf32_bfd_link_add_symbols
#define bfd_elf32_bfd_link_add_symbols	_bfd_generic_link_add_symbols
#endif
#ifndef bfd_elf32_bfd_final_link
#define bfd_elf32_bfd_final_link	_bfd_generic_final_link
#endif
#endif /* ! defined (elf_backend_relocate_section) */

#ifndef elf_info_to_howto_rel
#define elf_info_to_howto_rel 0
#endif

#ifndef ELF_MAXPAGESIZE
#define ELF_MAXPAGESIZE 1
#endif

#ifndef elf_backend_collect
#define elf_backend_collect false
#endif

#ifndef elf_backend_sym_is_global
#define elf_backend_sym_is_global	0
#endif
#ifndef elf_backend_object_p
#define elf_backend_object_p		0
#endif
#ifndef elf_backend_symbol_processing
#define elf_backend_symbol_processing	0
#endif
#ifndef elf_backend_symbol_table_processing
#define elf_backend_symbol_table_processing	0
#endif
#ifndef elf_backend_section_processing
#define elf_backend_section_processing	0
#endif
#ifndef elf_backend_section_from_shdr
#define elf_backend_section_from_shdr	0
#endif
#ifndef elf_backend_fake_sections
#define elf_backend_fake_sections	0
#endif
#ifndef elf_backend_section_from_bfd_section
#define elf_backend_section_from_bfd_section	0
#endif
#ifndef elf_backend_add_symbol_hook
#define elf_backend_add_symbol_hook	0
#endif
#ifndef elf_backend_link_output_symbol_hook
#define elf_backend_link_output_symbol_hook 0
#endif
#ifndef elf_backend_create_dynamic_sections
#define elf_backend_create_dynamic_sections 0
#endif
#ifndef elf_backend_adjust_dynamic_symbol
#define elf_backend_adjust_dynamic_symbol 0
#endif
#ifndef elf_backend_size_dynamic_sections
#define elf_backend_size_dynamic_sections 0
#endif
#ifndef elf_backend_relocate_section
#define elf_backend_relocate_section	0
#endif
#ifndef elf_backend_finish_dynamic_symbol
#define elf_backend_finish_dynamic_symbol	0
#endif
#ifndef elf_backend_finish_dynamic_sections
#define elf_backend_finish_dynamic_sections	0
#endif
#ifndef elf_backend_begin_write_processing
#define elf_backend_begin_write_processing	0
#endif
#ifndef elf_backend_final_write_processing
#define elf_backend_final_write_processing	0
#endif
#ifndef elf_backend_ecoff_debug_swap
#define elf_backend_ecoff_debug_swap	0
#endif

static CONST struct elf_backend_data elf32_bed =
{
#ifdef USE_REL
  0,				/* use_rela_p */
#else
  1,				/* use_rela_p */
#endif
  0,				/* elf_64_p */
  ELF_ARCH,			/* arch */
  ELF_MACHINE_CODE,		/* elf_machine_code */
  ELF_MAXPAGESIZE,		/* maxpagesize */
  elf_backend_collect,
  elf_info_to_howto,
  elf_info_to_howto_rel,
  elf_backend_sym_is_global,
  elf_backend_object_p,
  elf_backend_symbol_processing,
  elf_backend_symbol_table_processing,
  elf_backend_section_processing,
  elf_backend_section_from_shdr,
  elf_backend_fake_sections,
  elf_backend_section_from_bfd_section,
  elf_backend_add_symbol_hook,
  elf_backend_link_output_symbol_hook,
  elf_backend_create_dynamic_sections,
  elf_backend_adjust_dynamic_symbol,
  elf_backend_size_dynamic_sections,
  elf_backend_relocate_section,
  elf_backend_finish_dynamic_symbol,
  elf_backend_finish_dynamic_sections,
  elf_backend_begin_write_processing,
  elf_backend_final_write_processing,
  elf_backend_ecoff_debug_swap
};

#ifdef TARGET_BIG_SYM
const bfd_target TARGET_BIG_SYM =
{
  /* name: identify kind of target */
  TARGET_BIG_NAME,

  /* flavour: general indication about file */
  bfd_target_elf_flavour,

  /* byteorder_big_p: data is big endian */
  true,

  /* header_byteorder_big_p: header is also big endian */
  true,

  /* object_flags: mask of all file flags */
  (HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS |
   DYNAMIC | WP_TEXT | D_PAGED),
  
  /* section_flags: mask of all section flags */
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY |
   SEC_CODE | SEC_DATA | SEC_DEBUGGING),

   /* leading_symbol_char: is the first char of a user symbol
      predictable, and if so what is it */
   0,

  /* ar_pad_char: pad character for filenames within an archive header
     FIXME:  this really has nothing to do with ELF, this is a characteristic
     of the archiver and/or os and should be independently tunable */
  '/',

  /* ar_max_namelen: maximum number of characters in an archive header
     FIXME:  this really has nothing to do with ELF, this is a characteristic
     of the archiver and should be independently tunable.  This value is
     a WAG (wild a** guess) */
  14,

  /* align_power_min: minimum alignment restriction for any section
     FIXME:  this value may be target machine dependent */
  3,

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
    bfd_elf32_object_p,		/* assembler/linker output (object file) */
    bfd_generic_archive_p,	/* an archive */
    bfd_elf32_core_file_p	/* a core file */
  },

  /* bfd_set_format: set the format of a file being written */
  { bfd_false,
    bfd_elf_mkobject,
    _bfd_generic_mkarchive,
    bfd_false
  },

  /* bfd_write_contents: write cached information into a file being written */
  { bfd_false,
    bfd_elf32_write_object_contents,
    _bfd_write_archive_contents,
    bfd_false
  },

      BFD_JUMP_TABLE_GENERIC (bfd_elf32),
      BFD_JUMP_TABLE_COPY (bfd_elf32),
      BFD_JUMP_TABLE_CORE (bfd_elf32),
      BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
      BFD_JUMP_TABLE_SYMBOLS (bfd_elf32),
      BFD_JUMP_TABLE_RELOCS (bfd_elf32),
      BFD_JUMP_TABLE_WRITE (bfd_elf32),
      BFD_JUMP_TABLE_LINK (bfd_elf32),
      BFD_JUMP_TABLE_DYNAMIC (bfd_elf32),

  /* backend_data: */
  (PTR) &elf32_bed,
};
#endif

#ifdef TARGET_LITTLE_SYM
const bfd_target TARGET_LITTLE_SYM =
{
  /* name: identify kind of target */
  TARGET_LITTLE_NAME,

  /* flavour: general indication about file */
  bfd_target_elf_flavour,

  /* byteorder_big_p: data is big endian */
  false,		/* Nope -- this one's little endian */

  /* header_byteorder_big_p: header is also big endian */
  false,		/* Nope -- this one's little endian */

  /* object_flags: mask of all file flags */
  (HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS |
   DYNAMIC | WP_TEXT | D_PAGED),
  
  /* section_flags: mask of all section flags */
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY |
   SEC_CODE | SEC_DATA | SEC_DEBUGGING),

   /* leading_symbol_char: is the first char of a user symbol
      predictable, and if so what is it */
   0,

  /* ar_pad_char: pad character for filenames within an archive header
     FIXME:  this really has nothing to do with ELF, this is a characteristic
     of the archiver and/or os and should be independently tunable */
  '/',

  /* ar_max_namelen: maximum number of characters in an archive header
     FIXME:  this really has nothing to do with ELF, this is a characteristic
     of the archiver and should be independently tunable.  This value is
     a WAG (wild a** guess) */
  14,

  /* align_power_min: minimum alignment restriction for any section
     FIXME:  this value may be target machine dependent */
  3,

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
    bfd_elf32_object_p,		/* assembler/linker output (object file) */
    bfd_generic_archive_p,	/* an archive */
    bfd_elf32_core_file_p	/* a core file */
  },

  /* bfd_set_format: set the format of a file being written */
  { bfd_false,
    bfd_elf_mkobject,
    _bfd_generic_mkarchive,
    bfd_false
  },

  /* bfd_write_contents: write cached information into a file being written */
  { bfd_false,
    bfd_elf32_write_object_contents,
    _bfd_write_archive_contents,
    bfd_false
  },

      BFD_JUMP_TABLE_GENERIC (bfd_elf32),
      BFD_JUMP_TABLE_COPY (bfd_elf32),
      BFD_JUMP_TABLE_CORE (bfd_elf32),
      BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
      BFD_JUMP_TABLE_SYMBOLS (bfd_elf32),
      BFD_JUMP_TABLE_RELOCS (bfd_elf32),
      BFD_JUMP_TABLE_WRITE (bfd_elf32),
      BFD_JUMP_TABLE_LINK (bfd_elf32),
      BFD_JUMP_TABLE_DYNAMIC (bfd_elf32),

  /* backend_data: */
  (PTR) &elf32_bed,
};
#endif
