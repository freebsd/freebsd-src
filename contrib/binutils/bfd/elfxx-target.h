/* Target definitions for NN-bit ELF
   Copyright 1993, 1994, 1995, 1996, 1997 Free Software Foundation, Inc.

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

/* This structure contains everything that BFD knows about a target.
   It includes things like its byte order, name, what routines to call
   to do various operations, etc.  Every BFD points to a target structure
   with its "xvec" member.

   There are two such structures here:  one for big-endian machines and
   one for little-endian machines.   */

#define	bfd_elfNN_close_and_cleanup _bfd_elf_close_and_cleanup
#define bfd_elfNN_bfd_free_cached_info _bfd_generic_bfd_free_cached_info
#ifndef bfd_elfNN_get_section_contents
#define bfd_elfNN_get_section_contents _bfd_generic_get_section_contents
#endif

#define bfd_elfNN_canonicalize_dynamic_symtab _bfd_elf_canonicalize_dynamic_symtab
#define bfd_elfNN_canonicalize_reloc	_bfd_elf_canonicalize_reloc
#ifndef bfd_elfNN_find_nearest_line
#define bfd_elfNN_find_nearest_line	_bfd_elf_find_nearest_line
#endif
#define bfd_elfNN_read_minisymbols	_bfd_elf_read_minisymbols
#define bfd_elfNN_minisymbol_to_symbol	_bfd_elf_minisymbol_to_symbol
#define bfd_elfNN_get_dynamic_symtab_upper_bound _bfd_elf_get_dynamic_symtab_upper_bound
#define bfd_elfNN_get_lineno		_bfd_elf_get_lineno
#ifndef bfd_elfNN_get_reloc_upper_bound
#define bfd_elfNN_get_reloc_upper_bound _bfd_elf_get_reloc_upper_bound
#endif
#define bfd_elfNN_get_symbol_info	_bfd_elf_get_symbol_info
#define bfd_elfNN_get_symtab		_bfd_elf_get_symtab
#define bfd_elfNN_get_symtab_upper_bound _bfd_elf_get_symtab_upper_bound
#if 0 /* done in elf-bfd.h */
#define bfd_elfNN_link_record_dynamic_symbol _bfd_elf_link_record_dynamic_symbol
#endif
#define bfd_elfNN_make_empty_symbol	_bfd_elf_make_empty_symbol
#define bfd_elfNN_new_section_hook	_bfd_elf_new_section_hook
#define bfd_elfNN_set_arch_mach		_bfd_elf_set_arch_mach
#ifndef bfd_elfNN_set_section_contents
#define bfd_elfNN_set_section_contents	_bfd_elf_set_section_contents
#endif
#define bfd_elfNN_sizeof_headers	_bfd_elf_sizeof_headers
#define bfd_elfNN_write_object_contents _bfd_elf_write_object_contents
#define bfd_elfNN_write_corefile_contents _bfd_elf_write_corefile_contents

#define bfd_elfNN_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

#ifndef elf_backend_got_symbol_offset
#define elf_backend_got_symbol_offset (bfd_vma) 0
#endif
#ifndef elf_backend_want_got_plt
#define elf_backend_want_got_plt 0
#endif
#ifndef elf_backend_plt_readonly
#define elf_backend_plt_readonly 0
#endif
#ifndef elf_backend_want_plt_sym
#define elf_backend_want_plt_sym 0
#endif
#ifndef elf_backend_plt_not_loaded
#define elf_backend_plt_not_loaded 0
#endif
#ifndef elf_backend_plt_alignment
#define elf_backend_plt_alignment 2
#endif
#ifndef elf_backend_want_dynbss
#define elf_backend_want_dynbss 1
#endif

#define bfd_elfNN_bfd_debug_info_start	bfd_void
#define bfd_elfNN_bfd_debug_info_end	bfd_void
#define bfd_elfNN_bfd_debug_info_accumulate	(PROTO(void,(*),(bfd*, struct sec *))) bfd_void

#ifndef bfd_elfNN_bfd_get_relocated_section_contents
#define bfd_elfNN_bfd_get_relocated_section_contents \
 bfd_generic_get_relocated_section_contents
#endif

#ifndef bfd_elfNN_bfd_relax_section
#define bfd_elfNN_bfd_relax_section bfd_generic_relax_section
#endif

#ifndef elf_backend_can_gc_sections
#define elf_backend_can_gc_sections 0
#endif
#ifndef elf_backend_gc_mark_hook
#define elf_backend_gc_mark_hook	NULL
#endif
#ifndef elf_backend_gc_sweep_hook
#define elf_backend_gc_sweep_hook	NULL
#endif
#ifndef bfd_elfNN_bfd_gc_sections
#define bfd_elfNN_bfd_gc_sections _bfd_elfNN_gc_sections
#endif

#define bfd_elfNN_bfd_make_debug_symbol \
  ((asymbol *(*) PARAMS ((bfd *, void *, unsigned long))) bfd_nullvoidptr)

#ifndef bfd_elfNN_bfd_copy_private_symbol_data
#define bfd_elfNN_bfd_copy_private_symbol_data \
  _bfd_elf_copy_private_symbol_data
#endif

#ifndef bfd_elfNN_bfd_copy_private_section_data
#define bfd_elfNN_bfd_copy_private_section_data \
  _bfd_elf_copy_private_section_data
#endif
#ifndef bfd_elfNN_bfd_copy_private_bfd_data
#define bfd_elfNN_bfd_copy_private_bfd_data \
  ((boolean (*) PARAMS ((bfd *, bfd *))) bfd_true)
#endif
#ifndef bfd_elfNN_bfd_print_private_bfd_data
#define bfd_elfNN_bfd_print_private_bfd_data \
  _bfd_elf_print_private_bfd_data
#endif
#ifndef bfd_elfNN_bfd_merge_private_bfd_data
#define bfd_elfNN_bfd_merge_private_bfd_data \
  ((boolean (*) PARAMS ((bfd *, bfd *))) bfd_true)
#endif
#ifndef bfd_elfNN_bfd_set_private_flags
#define bfd_elfNN_bfd_set_private_flags \
  ((boolean (*) PARAMS ((bfd *, flagword))) bfd_true)
#endif
#ifndef bfd_elfNN_bfd_is_local_label_name
#define bfd_elfNN_bfd_is_local_label_name _bfd_elf_is_local_label_name
#endif

#ifndef bfd_elfNN_get_dynamic_reloc_upper_bound
#define bfd_elfNN_get_dynamic_reloc_upper_bound \
  _bfd_elf_get_dynamic_reloc_upper_bound
#endif
#ifndef bfd_elfNN_canonicalize_dynamic_reloc
#define bfd_elfNN_canonicalize_dynamic_reloc \
  _bfd_elf_canonicalize_dynamic_reloc
#endif

#ifdef elf_backend_relocate_section
#ifndef bfd_elfNN_bfd_link_hash_table_create
#define bfd_elfNN_bfd_link_hash_table_create _bfd_elf_link_hash_table_create
#endif
#else /* ! defined (elf_backend_relocate_section) */
/* If no backend relocate_section routine, use the generic linker.  */
#ifndef bfd_elfNN_bfd_link_hash_table_create
#define bfd_elfNN_bfd_link_hash_table_create \
  _bfd_generic_link_hash_table_create
#endif
#ifndef bfd_elfNN_bfd_link_add_symbols
#define bfd_elfNN_bfd_link_add_symbols	_bfd_generic_link_add_symbols
#endif
#ifndef bfd_elfNN_bfd_final_link
#define bfd_elfNN_bfd_final_link	_bfd_generic_final_link
#endif
#endif /* ! defined (elf_backend_relocate_section) */
#ifndef bfd_elfNN_bfd_link_split_section
#define bfd_elfNN_bfd_link_split_section _bfd_generic_link_split_section
#endif

#ifndef bfd_elfNN_archive_p
#define bfd_elfNN_archive_p bfd_generic_archive_p
#endif

#ifndef bfd_elfNN_write_archive_contents
#define bfd_elfNN_write_archive_contents _bfd_write_archive_contents
#endif

#ifndef bfd_elfNN_mkobject
#define bfd_elfNN_mkobject bfd_elf_mkobject
#endif

#ifndef bfd_elfNN_mkcorefile
#define bfd_elfNN_mkcorefile bfd_elf_mkcorefile
#endif

#ifndef bfd_elfNN_mkarchive
#define bfd_elfNN_mkarchive _bfd_generic_mkarchive
#endif

#ifndef elf_symbol_leading_char
#define elf_symbol_leading_char 0
#endif

#ifndef elf_info_to_howto
#define elf_info_to_howto 0
#endif

#ifndef elf_info_to_howto_rel
#define elf_info_to_howto_rel 0
#endif

#ifndef ELF_MAXPAGESIZE
  #error ELF_MAXPAGESIZE is not defined
#define ELF_MAXPAGESIZE 1
#endif

#ifndef elf_backend_collect
#define elf_backend_collect false
#endif
#ifndef elf_backend_type_change_ok
#define elf_backend_type_change_ok false
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
#ifndef elf_backend_get_symbol_type
#define elf_backend_get_symbol_type 0
#endif
#ifndef elf_backend_section_processing
#define elf_backend_section_processing	0
#endif
#ifndef elf_backend_section_from_shdr
#define elf_backend_section_from_shdr	0
#endif
#ifndef elf_backend_section_from_phdr
#define elf_backend_section_from_phdr	0
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
#ifndef elf_backend_check_relocs
#define elf_backend_check_relocs	0
#endif
#ifndef elf_backend_adjust_dynamic_symbol
#define elf_backend_adjust_dynamic_symbol 0
#endif
#ifndef elf_backend_always_size_sections
#define elf_backend_always_size_sections 0
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
#ifndef elf_backend_additional_program_headers
#define elf_backend_additional_program_headers	0
#endif
#ifndef elf_backend_modify_segment_map
#define elf_backend_modify_segment_map	0
#endif
#ifndef elf_backend_ecoff_debug_swap
#define elf_backend_ecoff_debug_swap	0
#endif
#ifndef elf_backend_got_header_size
#define elf_backend_got_header_size	0
#endif
#ifndef elf_backend_plt_header_size
#define elf_backend_plt_header_size	0
#endif
#ifndef elf_backend_post_process_headers
#define elf_backend_post_process_headers	NULL
#endif
#ifndef elf_backend_print_symbol_all
#define elf_backend_print_symbol_all		NULL
#endif
#ifndef elf_backend_output_arch_syms
#define elf_backend_output_arch_syms		NULL
#endif
#ifndef elf_backend_copy_indirect_symbol
#define elf_backend_copy_indirect_symbol  _bfd_elf_link_hash_copy_indirect
#endif
#ifndef elf_backend_hide_symbol
#define elf_backend_hide_symbol		_bfd_elf_link_hash_hide_symbol
#endif


/* Previously, backends could only use SHT_REL or SHT_RELA relocation
   sections, but not both.  They defined USE_REL to indicate SHT_REL
   sections, and left it undefined to indicated SHT_RELA sections.
   For backwards compatibility, we still support this usage.  */
#ifndef USE_REL
#define USE_REL 0
#else
#undef USE_REL
#define USE_REL 1
#endif 

/* Use these in new code.  */
#ifndef elf_backend_may_use_rel_p 
#define elf_backend_may_use_rel_p USE_REL
#endif 
#ifndef elf_backend_may_use_rela_p
#define elf_backend_may_use_rela_p !USE_REL
#endif
#ifndef elf_backend_default_use_rela_p 
#define elf_backend_default_use_rela_p !USE_REL
#endif

#ifndef ELF_MACHINE_ALT1
#define ELF_MACHINE_ALT1 0
#endif

#ifndef ELF_MACHINE_ALT2
#define ELF_MACHINE_ALT2 0
#endif

#ifndef elf_backend_size_info
#define elf_backend_size_info _bfd_elfNN_size_info
#endif

#ifndef elf_backend_sign_extend_vma
#define elf_backend_sign_extend_vma 0
#endif

extern const struct elf_size_info _bfd_elfNN_size_info;

static CONST struct elf_backend_data elfNN_bed =
{
  ELF_ARCH,			/* arch */
  ELF_MACHINE_CODE,		/* elf_machine_code */
  ELF_MAXPAGESIZE,		/* maxpagesize */
  elf_info_to_howto,
  elf_info_to_howto_rel,
  elf_backend_sym_is_global,
  elf_backend_object_p,
  elf_backend_symbol_processing,
  elf_backend_symbol_table_processing,
  elf_backend_get_symbol_type,
  elf_backend_section_processing,
  elf_backend_section_from_shdr,
  elf_backend_section_from_phdr,
  elf_backend_fake_sections,
  elf_backend_section_from_bfd_section,
  elf_backend_add_symbol_hook,
  elf_backend_link_output_symbol_hook,
  elf_backend_create_dynamic_sections,
  elf_backend_check_relocs,
  elf_backend_adjust_dynamic_symbol,
  elf_backend_always_size_sections,
  elf_backend_size_dynamic_sections,
  elf_backend_relocate_section,
  elf_backend_finish_dynamic_symbol,
  elf_backend_finish_dynamic_sections,
  elf_backend_begin_write_processing,
  elf_backend_final_write_processing,
  elf_backend_additional_program_headers,
  elf_backend_modify_segment_map,
  elf_backend_gc_mark_hook,
  elf_backend_gc_sweep_hook,
  elf_backend_post_process_headers,
  elf_backend_print_symbol_all,
  elf_backend_output_arch_syms,
  elf_backend_copy_indirect_symbol,
  elf_backend_hide_symbol,
  elf_backend_ecoff_debug_swap,
  ELF_MACHINE_ALT1,
  ELF_MACHINE_ALT2,
  &elf_backend_size_info,
  elf_backend_got_symbol_offset,
  elf_backend_got_header_size,
  elf_backend_plt_header_size,
  elf_backend_collect,
  elf_backend_type_change_ok,
  elf_backend_may_use_rel_p,
  elf_backend_may_use_rela_p,
  elf_backend_default_use_rela_p,
  elf_backend_sign_extend_vma,
  elf_backend_want_got_plt,
  elf_backend_plt_readonly,
  elf_backend_want_plt_sym,
  elf_backend_plt_not_loaded,
  elf_backend_plt_alignment,
  elf_backend_can_gc_sections,
  elf_backend_want_dynbss
};

/* Forward declaration for use when initialising alternative_target field.  */
#ifdef TARGET_LITTLE_SYM
extern const bfd_target TARGET_LITTLE_SYM;
#endif

#ifdef TARGET_BIG_SYM
const bfd_target TARGET_BIG_SYM =
{
  /* name: identify kind of target */
  TARGET_BIG_NAME,

  /* flavour: general indication about file */
  bfd_target_elf_flavour,

  /* byteorder: data is big endian */
  BFD_ENDIAN_BIG,

  /* header_byteorder: header is also big endian */
  BFD_ENDIAN_BIG,

  /* object_flags: mask of all file flags */
  (HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS |
   DYNAMIC | WP_TEXT | D_PAGED),

  /* section_flags: mask of all section flags */
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY |
   SEC_CODE | SEC_DATA | SEC_DEBUGGING | SEC_EXCLUDE | SEC_SORT_ENTRIES),

   /* leading_symbol_char: is the first char of a user symbol
      predictable, and if so what is it */
  elf_symbol_leading_char,

  /* ar_pad_char: pad character for filenames within an archive header
     FIXME:  this really has nothing to do with ELF, this is a characteristic
     of the archiver and/or os and should be independently tunable */
  '/',

  /* ar_max_namelen: maximum number of characters in an archive header
     FIXME:  this really has nothing to do with ELF, this is a characteristic
     of the archiver and should be independently tunable.  This value is
     a WAG (wild a** guess) */
  14,

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
    bfd_elfNN_object_p,		/* assembler/linker output (object file) */
    bfd_elfNN_archive_p,	/* an archive */
    bfd_elfNN_core_file_p	/* a core file */
  },

  /* bfd_set_format: set the format of a file being written */
  { bfd_false,
    bfd_elfNN_mkobject,
    bfd_elfNN_mkarchive,
    bfd_elfNN_mkcorefile
  },

  /* bfd_write_contents: write cached information into a file being written */
  { bfd_false,
    bfd_elfNN_write_object_contents,
    bfd_elfNN_write_archive_contents,
    bfd_elfNN_write_corefile_contents,
  },

      BFD_JUMP_TABLE_GENERIC (bfd_elfNN),
      BFD_JUMP_TABLE_COPY (bfd_elfNN),
      BFD_JUMP_TABLE_CORE (bfd_elfNN),
#ifdef bfd_elfNN_archive_functions
      BFD_JUMP_TABLE_ARCHIVE (bfd_elfNN_archive),
#else
      BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
#endif
      BFD_JUMP_TABLE_SYMBOLS (bfd_elfNN),
      BFD_JUMP_TABLE_RELOCS (bfd_elfNN),
      BFD_JUMP_TABLE_WRITE (bfd_elfNN),
      BFD_JUMP_TABLE_LINK (bfd_elfNN),
      BFD_JUMP_TABLE_DYNAMIC (bfd_elfNN),

  /* Alternative endian target.  */
#ifdef TARGET_LITTLE_SYM
  & TARGET_LITTLE_SYM,
#else
  NULL,
#endif

  /* backend_data: */
  (PTR) &elfNN_bed
};
#endif

#ifdef TARGET_LITTLE_SYM
const bfd_target TARGET_LITTLE_SYM =
{
  /* name: identify kind of target */
  TARGET_LITTLE_NAME,

  /* flavour: general indication about file */
  bfd_target_elf_flavour,

  /* byteorder: data is little endian */
  BFD_ENDIAN_LITTLE,

  /* header_byteorder: header is also little endian */
  BFD_ENDIAN_LITTLE,

  /* object_flags: mask of all file flags */
  (HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS |
   DYNAMIC | WP_TEXT | D_PAGED),

  /* section_flags: mask of all section flags */
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY |
   SEC_CODE | SEC_DATA | SEC_DEBUGGING | SEC_EXCLUDE | SEC_SORT_ENTRIES),

   /* leading_symbol_char: is the first char of a user symbol
      predictable, and if so what is it */
  elf_symbol_leading_char,

  /* ar_pad_char: pad character for filenames within an archive header
     FIXME:  this really has nothing to do with ELF, this is a characteristic
     of the archiver and/or os and should be independently tunable */
  '/',

  /* ar_max_namelen: maximum number of characters in an archive header
     FIXME:  this really has nothing to do with ELF, this is a characteristic
     of the archiver and should be independently tunable.  This value is
     a WAG (wild a** guess) */
  14,

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
    bfd_elfNN_object_p,		/* assembler/linker output (object file) */
    bfd_elfNN_archive_p,	/* an archive */
    bfd_elfNN_core_file_p	/* a core file */
  },

  /* bfd_set_format: set the format of a file being written */
  { bfd_false,
    bfd_elfNN_mkobject,
    bfd_elfNN_mkarchive,
    bfd_elfNN_mkcorefile
  },

  /* bfd_write_contents: write cached information into a file being written */
  { bfd_false,
    bfd_elfNN_write_object_contents,
    bfd_elfNN_write_archive_contents,
    bfd_elfNN_write_corefile_contents,
  },

      BFD_JUMP_TABLE_GENERIC (bfd_elfNN),
      BFD_JUMP_TABLE_COPY (bfd_elfNN),
      BFD_JUMP_TABLE_CORE (bfd_elfNN),
#ifdef bfd_elfNN_archive_functions
      BFD_JUMP_TABLE_ARCHIVE (bfd_elfNN_archive),
#else
      BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
#endif
      BFD_JUMP_TABLE_SYMBOLS (bfd_elfNN),
      BFD_JUMP_TABLE_RELOCS (bfd_elfNN),
      BFD_JUMP_TABLE_WRITE (bfd_elfNN),
      BFD_JUMP_TABLE_LINK (bfd_elfNN),
      BFD_JUMP_TABLE_DYNAMIC (bfd_elfNN),

  /* Alternative endian target.  */
#ifdef TARGET_BIG_SYM
  & TARGET_BIG_SYM,
#else
  NULL,
#endif
  
  /* backend_data: */
  (PTR) &elfNN_bed
};
#endif
