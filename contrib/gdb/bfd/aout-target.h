/* Define a target vector and some small routines for a variant of a.out.
   Copyright (C) 1990, 91, 92, 93, 94, 95, 1996 Free Software Foundation, Inc.

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

#include "aout/aout64.h"
#include "aout/stab_gnu.h"
#include "aout/ar.h"
/*#include "libaout.h"*/

extern reloc_howto_type * NAME(aout,reloc_type_lookup) ();

/* Set parameters about this a.out file that are machine-dependent.
   This routine is called from some_aout_object_p just before it returns.  */
#ifndef MY_callback
static const bfd_target *
MY(callback) (abfd)
     bfd *abfd;
{
  struct internal_exec *execp = exec_hdr (abfd);
  unsigned int arch_align_power;
  unsigned long arch_align;

  /* Calculate the file positions of the parts of a newly read aout header */
  obj_textsec (abfd)->_raw_size = N_TXTSIZE(*execp);

  /* The virtual memory addresses of the sections */
  obj_textsec (abfd)->vma = N_TXTADDR(*execp);
  obj_datasec (abfd)->vma = N_DATADDR(*execp);
  obj_bsssec  (abfd)->vma = N_BSSADDR(*execp);

  obj_textsec (abfd)->lma = obj_textsec (abfd)->vma;
  obj_datasec (abfd)->lma = obj_datasec (abfd)->vma;
  obj_bsssec (abfd)->lma = obj_bsssec (abfd)->vma;

  /* The file offsets of the sections */
  obj_textsec (abfd)->filepos = N_TXTOFF (*execp);
  obj_datasec (abfd)->filepos = N_DATOFF (*execp);

  /* The file offsets of the relocation info */
  obj_textsec (abfd)->rel_filepos = N_TRELOFF(*execp);
  obj_datasec (abfd)->rel_filepos = N_DRELOFF(*execp);

  /* The file offsets of the string table and symbol table.  */
  obj_sym_filepos (abfd) = N_SYMOFF (*execp);
  obj_str_filepos (abfd) = N_STROFF (*execp);
  
  /* Determine the architecture and machine type of the object file.  */
#ifdef SET_ARCH_MACH
  SET_ARCH_MACH(abfd, *execp);
#else
  bfd_default_set_arch_mach(abfd, DEFAULT_ARCH, 0);
#endif

  /* Now that we know the architecture, set the alignments of the
     sections.  This is normally done by NAME(aout,new_section_hook),
     but when the initial sections were created the architecture had
     not yet been set.  However, for backward compatibility, we don't
     set the alignment power any higher than as required by the size
     of the section.  */
  arch_align_power = bfd_get_arch_info (abfd)->section_align_power;
  arch_align = 1 << arch_align_power;
  if ((BFD_ALIGN (obj_textsec (abfd)->_raw_size, arch_align)
       == obj_textsec (abfd)->_raw_size)
      && (BFD_ALIGN (obj_datasec (abfd)->_raw_size, arch_align)
	  == obj_datasec (abfd)->_raw_size)
      && (BFD_ALIGN (obj_bsssec (abfd)->_raw_size, arch_align)
	  == obj_bsssec (abfd)->_raw_size))
    {
      obj_textsec (abfd)->alignment_power = arch_align_power;
      obj_datasec (abfd)->alignment_power = arch_align_power;
      obj_bsssec (abfd)->alignment_power = arch_align_power;
    }

  /* Don't set sizes now -- can't be sure until we know arch & mach.
     Sizes get set in set_sizes callback, later.  */
#if 0
  adata(abfd).page_size = TARGET_PAGE_SIZE;
#ifdef SEGMENT_SIZE
  adata(abfd).segment_size = SEGMENT_SIZE;
#else
  adata(abfd).segment_size = TARGET_PAGE_SIZE;
#endif
  adata(abfd).exec_bytes_size = EXEC_BYTES_SIZE;
#endif

  return abfd->xvec;
}
#endif

#ifndef MY_object_p
/* Finish up the reading of an a.out file header */

static const bfd_target *
MY(object_p) (abfd)
     bfd *abfd;
{
  struct external_exec exec_bytes;	/* Raw exec header from file */
  struct internal_exec exec;		/* Cleaned-up exec header */
  const bfd_target *target;

  if (bfd_read ((PTR) &exec_bytes, 1, EXEC_BYTES_SIZE, abfd)
      != EXEC_BYTES_SIZE) {
    if (bfd_get_error () != bfd_error_system_call)
      bfd_set_error (bfd_error_wrong_format);
    return 0;
  }

#ifdef SWAP_MAGIC
  exec.a_info = SWAP_MAGIC (exec_bytes.e_info);
#else
  exec.a_info = bfd_h_get_32 (abfd, exec_bytes.e_info);
#endif /* SWAP_MAGIC */

  if (N_BADMAG (exec)) return 0;
#ifdef MACHTYPE_OK
  if (!(MACHTYPE_OK (N_MACHTYPE (exec)))) return 0;
#endif

  NAME(aout,swap_exec_header_in)(abfd, &exec_bytes, &exec);

#ifdef SWAP_MAGIC
  /* swap_exec_header_in read in a_info with the wrong byte order */
  exec.a_info = SWAP_MAGIC (exec_bytes.e_info);
#endif /* SWAP_MAGIC */

  target = NAME(aout,some_aout_object_p) (abfd, &exec, MY(callback));

#ifdef ENTRY_CAN_BE_ZERO
  /* The NEWSOS3 entry-point is/was 0, which (amongst other lossage)
   * means that it isn't obvious if EXEC_P should be set.
   * All of the following must be true for an executable:
   * There must be no relocations, the bfd can be neither an
   * archive nor an archive element, and the file must be executable. */

  if (exec.a_trsize + exec.a_drsize == 0
      && bfd_get_format(abfd) == bfd_object && abfd->my_archive == NULL)
    {
      struct stat buf;
#ifndef S_IXUSR
#define S_IXUSR 0100	/* Execute by owner.  */
#endif
      if (stat(abfd->filename, &buf) == 0 && (buf.st_mode & S_IXUSR))
	abfd->flags |= EXEC_P;
    }
#endif /* ENTRY_CAN_BE_ZERO */

  return target;
}
#define MY_object_p MY(object_p)
#endif


#ifndef MY_mkobject
static boolean
MY(mkobject) (abfd)
     bfd *abfd;
{
  if (NAME(aout,mkobject)(abfd) == false)
    return false;
#if 0 /* Sizes get set in set_sizes callback, later, after we know
	 the architecture and machine.  */
  adata(abfd).page_size = TARGET_PAGE_SIZE;
#ifdef SEGMENT_SIZE
  adata(abfd).segment_size = SEGMENT_SIZE;
#else
  adata(abfd).segment_size = TARGET_PAGE_SIZE;
#endif
  adata(abfd).exec_bytes_size = EXEC_BYTES_SIZE;
#endif
  return true;
}
#define MY_mkobject MY(mkobject)
#endif

#ifndef MY_bfd_copy_private_section_data

/* Copy private section data.  This actually does nothing with the
   sections.  It copies the subformat field.  We copy it here, because
   we need to know whether this is a QMAGIC file before we set the
   section contents, and copy_private_bfd_data is not called until
   after the section contents have been set.  */

/*ARGSUSED*/
static boolean
MY_bfd_copy_private_section_data (ibfd, isec, obfd, osec)
     bfd *ibfd;
     asection *isec;
     bfd *obfd;
     asection *osec;
{
  if (bfd_get_flavour (obfd) == bfd_target_aout_flavour)
    obj_aout_subformat (obfd) = obj_aout_subformat (ibfd);
  return true;
}

#endif

/* Write an object file.
   Section contents have already been written.  We write the
   file header, symbols, and relocation.  */

#ifndef MY_write_object_contents
static boolean
MY(write_object_contents) (abfd)
     bfd *abfd;
{
  struct external_exec exec_bytes;
  struct internal_exec *execp = exec_hdr (abfd);

#if CHOOSE_RELOC_SIZE
  CHOOSE_RELOC_SIZE(abfd);
#else
  obj_reloc_entry_size (abfd) = RELOC_STD_SIZE;
#endif

  WRITE_HEADERS(abfd, execp);

  return true;
}
#define MY_write_object_contents MY(write_object_contents)
#endif

#ifndef MY_set_sizes
static boolean
MY(set_sizes) (abfd)
     bfd *abfd;
{
  adata(abfd).page_size = TARGET_PAGE_SIZE;

#ifdef SEGMENT_SIZE
  adata(abfd).segment_size = SEGMENT_SIZE;
#else
  adata(abfd).segment_size = TARGET_PAGE_SIZE;
#endif

#ifdef ZMAGIC_DISK_BLOCK_SIZE
  adata(abfd).zmagic_disk_block_size = ZMAGIC_DISK_BLOCK_SIZE;
#else
  adata(abfd).zmagic_disk_block_size = TARGET_PAGE_SIZE;
#endif

  adata(abfd).exec_bytes_size = EXEC_BYTES_SIZE;
  return true;
}
#define MY_set_sizes MY(set_sizes)
#endif

#ifndef MY_exec_hdr_flags
#define MY_exec_hdr_flags 0
#endif

#ifndef MY_backend_data

#ifndef MY_zmagic_contiguous
#define MY_zmagic_contiguous 0
#endif
#ifndef MY_text_includes_header
#define MY_text_includes_header 0
#endif
#ifndef MY_exec_header_not_counted
#define MY_exec_header_not_counted 0
#endif
#ifndef MY_add_dynamic_symbols
#define MY_add_dynamic_symbols 0
#endif
#ifndef MY_add_one_symbol
#define MY_add_one_symbol 0
#endif
#ifndef MY_link_dynamic_object
#define MY_link_dynamic_object 0
#endif
#ifndef MY_write_dynamic_symbol
#define MY_write_dynamic_symbol 0
#endif
#ifndef MY_check_dynamic_reloc
#define MY_check_dynamic_reloc 0
#endif
#ifndef MY_finish_dynamic_link
#define MY_finish_dynamic_link 0
#endif

static CONST struct aout_backend_data MY(backend_data) = {
  MY_zmagic_contiguous,
  MY_text_includes_header,
  MY_exec_hdr_flags,
  0,				/* text vma? */
  MY_set_sizes,
  MY_exec_header_not_counted,
  MY_add_dynamic_symbols,
  MY_add_one_symbol,
  MY_link_dynamic_object,
  MY_write_dynamic_symbol,
  MY_check_dynamic_reloc,
  MY_finish_dynamic_link
};
#define MY_backend_data &MY(backend_data)
#endif

#ifndef MY_final_link_callback

/* Callback for the final_link routine to set the section offsets.  */

static void MY_final_link_callback
  PARAMS ((bfd *, file_ptr *, file_ptr *, file_ptr *));

static void
MY_final_link_callback (abfd, ptreloff, pdreloff, psymoff)
     bfd *abfd;
     file_ptr *ptreloff;
     file_ptr *pdreloff;
     file_ptr *psymoff;
{
  struct internal_exec *execp = exec_hdr (abfd);

  *ptreloff = N_TRELOFF (*execp);
  *pdreloff = N_DRELOFF (*execp);
  *psymoff = N_SYMOFF (*execp);
}

#endif

#ifndef MY_bfd_final_link

/* Final link routine.  We need to use a call back to get the correct
   offsets in the output file.  */

static boolean
MY_bfd_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  return NAME(aout,final_link) (abfd, info, MY_final_link_callback);
}

#endif

/* We assume BFD generic archive files.  */
#ifndef	MY_openr_next_archived_file
#define	MY_openr_next_archived_file	bfd_generic_openr_next_archived_file
#endif
#ifndef MY_get_elt_at_index
#define MY_get_elt_at_index		_bfd_generic_get_elt_at_index
#endif
#ifndef	MY_generic_stat_arch_elt
#define	MY_generic_stat_arch_elt	bfd_generic_stat_arch_elt
#endif
#ifndef	MY_slurp_armap
#define	MY_slurp_armap			bfd_slurp_bsd_armap
#endif
#ifndef	MY_slurp_extended_name_table
#define	MY_slurp_extended_name_table	_bfd_slurp_extended_name_table
#endif
#ifndef MY_construct_extended_name_table
#define MY_construct_extended_name_table \
  _bfd_archive_bsd_construct_extended_name_table
#endif
#ifndef	MY_write_armap
#define	MY_write_armap		bsd_write_armap
#endif
#ifndef MY_read_ar_hdr
#define MY_read_ar_hdr		_bfd_generic_read_ar_hdr
#endif
#ifndef	MY_truncate_arname
#define	MY_truncate_arname		bfd_bsd_truncate_arname
#endif
#ifndef MY_update_armap_timestamp
#define MY_update_armap_timestamp _bfd_archive_bsd_update_armap_timestamp
#endif

/* No core file defined here -- configure in trad-core.c separately.  */
#ifndef	MY_core_file_failing_command
#define	MY_core_file_failing_command _bfd_nocore_core_file_failing_command
#endif
#ifndef	MY_core_file_failing_signal
#define	MY_core_file_failing_signal	_bfd_nocore_core_file_failing_signal
#endif
#ifndef	MY_core_file_matches_executable_p
#define	MY_core_file_matches_executable_p	\
				_bfd_nocore_core_file_matches_executable_p
#endif
#ifndef	MY_core_file_p
#define	MY_core_file_p		_bfd_dummy_target
#endif

#ifndef MY_bfd_debug_info_start
#define MY_bfd_debug_info_start		bfd_void
#endif
#ifndef MY_bfd_debug_info_end
#define MY_bfd_debug_info_end		bfd_void
#endif
#ifndef MY_bfd_debug_info_accumulate
#define MY_bfd_debug_info_accumulate	\
			(void (*) PARAMS ((bfd*, struct sec *))) bfd_void
#endif

#ifndef MY_core_file_failing_command
#define MY_core_file_failing_command NAME(aout,core_file_failing_command)
#endif
#ifndef MY_core_file_failing_signal
#define MY_core_file_failing_signal NAME(aout,core_file_failing_signal)
#endif
#ifndef MY_core_file_matches_executable_p
#define MY_core_file_matches_executable_p NAME(aout,core_file_matches_executable_p)
#endif
#ifndef MY_set_section_contents
#define MY_set_section_contents NAME(aout,set_section_contents)
#endif
#ifndef MY_get_section_contents
#define MY_get_section_contents NAME(aout,get_section_contents)
#endif
#ifndef MY_get_section_contents_in_window
#define MY_get_section_contents_in_window _bfd_generic_get_section_contents_in_window
#endif
#ifndef MY_new_section_hook
#define MY_new_section_hook NAME(aout,new_section_hook)
#endif
#ifndef MY_get_symtab_upper_bound
#define MY_get_symtab_upper_bound NAME(aout,get_symtab_upper_bound)
#endif
#ifndef MY_get_symtab
#define MY_get_symtab NAME(aout,get_symtab)
#endif
#ifndef MY_get_reloc_upper_bound
#define MY_get_reloc_upper_bound NAME(aout,get_reloc_upper_bound)
#endif
#ifndef MY_canonicalize_reloc
#define MY_canonicalize_reloc NAME(aout,canonicalize_reloc)
#endif
#ifndef MY_make_empty_symbol
#define MY_make_empty_symbol NAME(aout,make_empty_symbol)
#endif
#ifndef MY_print_symbol
#define MY_print_symbol NAME(aout,print_symbol)
#endif
#ifndef MY_get_symbol_info
#define MY_get_symbol_info NAME(aout,get_symbol_info)
#endif
#ifndef MY_get_lineno
#define MY_get_lineno NAME(aout,get_lineno)
#endif
#ifndef MY_set_arch_mach
#define MY_set_arch_mach NAME(aout,set_arch_mach)
#endif
#ifndef MY_find_nearest_line
#define MY_find_nearest_line NAME(aout,find_nearest_line)
#endif
#ifndef MY_sizeof_headers
#define MY_sizeof_headers NAME(aout,sizeof_headers)
#endif
#ifndef MY_bfd_get_relocated_section_contents
#define MY_bfd_get_relocated_section_contents \
			bfd_generic_get_relocated_section_contents
#endif
#ifndef MY_bfd_relax_section
#define MY_bfd_relax_section bfd_generic_relax_section
#endif
#ifndef MY_bfd_reloc_type_lookup
#define MY_bfd_reloc_type_lookup NAME(aout,reloc_type_lookup)
#endif
#ifndef MY_bfd_make_debug_symbol
#define MY_bfd_make_debug_symbol 0
#endif
#ifndef MY_read_minisymbols
#define MY_read_minisymbols NAME(aout,read_minisymbols)
#endif
#ifndef MY_minisymbol_to_symbol
#define MY_minisymbol_to_symbol NAME(aout,minisymbol_to_symbol)
#endif
#ifndef MY_bfd_link_hash_table_create
#define MY_bfd_link_hash_table_create NAME(aout,link_hash_table_create)
#endif
#ifndef MY_bfd_link_add_symbols
#define MY_bfd_link_add_symbols NAME(aout,link_add_symbols)
#endif
#ifndef MY_bfd_link_split_section
#define MY_bfd_link_split_section  _bfd_generic_link_split_section
#endif


#ifndef MY_bfd_copy_private_bfd_data
#define MY_bfd_copy_private_bfd_data _bfd_generic_bfd_copy_private_bfd_data
#endif

#ifndef MY_bfd_merge_private_bfd_data
#define MY_bfd_merge_private_bfd_data _bfd_generic_bfd_merge_private_bfd_data
#endif

#ifndef MY_bfd_copy_private_symbol_data
#define MY_bfd_copy_private_symbol_data _bfd_generic_bfd_copy_private_symbol_data
#endif

#ifndef MY_bfd_print_private_bfd_data
#define MY_bfd_print_private_bfd_data _bfd_generic_bfd_print_private_bfd_data
#endif

#ifndef MY_bfd_set_private_flags
#define MY_bfd_set_private_flags _bfd_generic_bfd_set_private_flags
#endif

#ifndef MY_bfd_is_local_label
#define MY_bfd_is_local_label bfd_generic_is_local_label
#endif

#ifndef MY_bfd_free_cached_info
#define MY_bfd_free_cached_info NAME(aout,bfd_free_cached_info)
#endif

#ifndef MY_close_and_cleanup
#define MY_close_and_cleanup MY_bfd_free_cached_info
#endif

#ifndef MY_get_dynamic_symtab_upper_bound
#define MY_get_dynamic_symtab_upper_bound \
  _bfd_nodynamic_get_dynamic_symtab_upper_bound
#endif
#ifndef MY_canonicalize_dynamic_symtab
#define MY_canonicalize_dynamic_symtab \
  _bfd_nodynamic_canonicalize_dynamic_symtab
#endif
#ifndef MY_get_dynamic_reloc_upper_bound
#define MY_get_dynamic_reloc_upper_bound \
  _bfd_nodynamic_get_dynamic_reloc_upper_bound
#endif
#ifndef MY_canonicalize_dynamic_reloc
#define MY_canonicalize_dynamic_reloc \
  _bfd_nodynamic_canonicalize_dynamic_reloc
#endif

/* Aout symbols normally have leading underscores */
#ifndef MY_symbol_leading_char 
#define MY_symbol_leading_char '_'
#endif

/* Aout archives normally use spaces for padding */
#ifndef AR_PAD_CHAR
#define AR_PAD_CHAR ' '
#endif

#ifndef MY_BFD_TARGET
const bfd_target MY(vec) =
{
  TARGETNAME,		/* name */
  bfd_target_aout_flavour,
#ifdef TARGET_IS_BIG_ENDIAN_P
  BFD_ENDIAN_BIG,		/* target byte order (big) */
  BFD_ENDIAN_BIG,		/* target headers byte order (big) */
#else
  BFD_ENDIAN_LITTLE,		/* target byte order (little) */
  BFD_ENDIAN_LITTLE,		/* target headers byte order (little) */
#endif
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | DYNAMIC | WP_TEXT | D_PAGED),
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  MY_symbol_leading_char,
  AR_PAD_CHAR,			/* ar_pad_char */
  15,				/* ar_max_namelen */
#ifdef TARGET_IS_BIG_ENDIAN_P
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */
#else
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */
#endif
    {_bfd_dummy_target, MY_object_p, /* bfd_check_format */
       bfd_generic_archive_p, MY_core_file_p},
    {bfd_false, MY_mkobject,	/* bfd_set_format */
       _bfd_generic_mkarchive, bfd_false},
    {bfd_false, MY_write_object_contents, /* bfd_write_contents */
       _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (MY),
     BFD_JUMP_TABLE_COPY (MY),
     BFD_JUMP_TABLE_CORE (MY),
     BFD_JUMP_TABLE_ARCHIVE (MY),
     BFD_JUMP_TABLE_SYMBOLS (MY),
     BFD_JUMP_TABLE_RELOCS (MY),
     BFD_JUMP_TABLE_WRITE (MY),
     BFD_JUMP_TABLE_LINK (MY),
     BFD_JUMP_TABLE_DYNAMIC (MY),

  (PTR) MY_backend_data,
};
#endif /* MY_BFD_TARGET */
