/* Define a target vector and some small routines for a variant of a.out.
   Copyright (C) 1990, 1991, 1992, 1993 Free Software Foundation, Inc.

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

#include "aout/aout64.h"
#include "aout/stab_gnu.h"
#include "aout/ar.h"
/*#include "libaout.h"*/

extern CONST struct reloc_howto_struct * NAME(aout,reloc_type_lookup) ();

/* Set parameters about this a.out file that are machine-dependent.
   This routine is called from some_aout_object_p just before it returns.  */
#ifndef MY_callback
static bfd_target *
DEFUN(MY(callback),(abfd),
      bfd *abfd)
{
  struct internal_exec *execp = exec_hdr (abfd);

  /* Calculate the file positions of the parts of a newly read aout header */
  obj_textsec (abfd)->_raw_size = N_TXTSIZE(*execp);

  /* The virtual memory addresses of the sections */
  obj_textsec (abfd)->vma = N_TXTADDR(*execp);
  obj_datasec (abfd)->vma = N_DATADDR(*execp);
  obj_bsssec  (abfd)->vma = N_BSSADDR(*execp);

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

  /* Don't set sizes now -- can't be sure until we know arch & mach.
     Sizes get set in set_sizes callback, later.  */
#if 0
  adata(abfd).page_size = PAGE_SIZE;
#ifdef SEGMENT_SIZE
  adata(abfd).segment_size = SEGMENT_SIZE;
#else
  adata(abfd).segment_size = PAGE_SIZE;
#endif
  adata(abfd).exec_bytes_size = EXEC_BYTES_SIZE;
#endif

  return abfd->xvec;
}
#endif

#ifndef MY_object_p
/* Finish up the reading of an a.out file header */

static bfd_target *
DEFUN(MY(object_p),(abfd),
     bfd *abfd)
{
  struct external_exec exec_bytes;	/* Raw exec header from file */
  struct internal_exec exec;		/* Cleaned-up exec header */
  bfd_target *target;

  if (bfd_read ((PTR) &exec_bytes, 1, EXEC_BYTES_SIZE, abfd)
      != EXEC_BYTES_SIZE) {
    bfd_error = wrong_format;
    return 0;
  }

#ifdef NO_SWAP_MAGIC
  memcpy (&exec.a_info, exec_bytes.e_info, sizeof(exec.a_info));
#else
  exec.a_info = bfd_h_get_32 (abfd, exec_bytes.e_info);
#endif /* NO_SWAP_MAGIC */

  if (N_BADMAG (exec)) return 0;
#ifdef MACHTYPE_OK
  if (!(MACHTYPE_OK (N_MACHTYPE (exec)))) return 0;
#endif

  NAME(aout,swap_exec_header_in)(abfd, &exec_bytes, &exec);
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
DEFUN(MY(mkobject),(abfd),
      bfd *abfd)
{
  if (NAME(aout,mkobject)(abfd) == false)
    return false;
#if 0 /* Sizes get set in set_sizes callback, later, after we know
	 the architecture and machine.  */
  adata(abfd).page_size = PAGE_SIZE;
#ifdef SEGMENT_SIZE
  adata(abfd).segment_size = SEGMENT_SIZE;
#else
  adata(abfd).segment_size = PAGE_SIZE;
#endif
  adata(abfd).exec_bytes_size = EXEC_BYTES_SIZE;
#endif
  return true;
}
#define MY_mkobject MY(mkobject)
#endif

/* Write an object file.
   Section contents have already been written.  We write the
   file header, symbols, and relocation.  */

#ifndef MY_write_object_contents
static boolean
DEFUN(MY(write_object_contents),(abfd),
      bfd *abfd)
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
DEFUN(MY(set_sizes),(abfd), bfd *abfd)
{
  adata(abfd).page_size = PAGE_SIZE;
#ifdef SEGMENT_SIZE
  adata(abfd).segment_size = SEGMENT_SIZE;
#else
  adata(abfd).segment_size = PAGE_SIZE;
#endif
  adata(abfd).exec_bytes_size = EXEC_BYTES_SIZE;
  return true;
}
#define MY_set_sizes MY(set_sizes)
#endif

#ifndef MY_backend_data
static CONST struct aout_backend_data MY(backend_data) = {
  0,				/* zmagic contiguous */
  0,				/* text incl header */
  0,				/* text vma? */
  MY_set_sizes,
  0,				/* exec header is counted */
};
#define MY_backend_data &MY(backend_data)
#endif

/* We assume BFD generic archive files.  */
#ifndef	MY_openr_next_archived_file
#define	MY_openr_next_archived_file	bfd_generic_openr_next_archived_file
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
#ifndef	MY_write_armap
#define	MY_write_armap		bsd_write_armap
#endif
#ifndef	MY_truncate_arname
#define	MY_truncate_arname		bfd_bsd_truncate_arname
#endif

/* No core file defined here -- configure in trad-core.c separately.  */
#ifndef	MY_core_file_failing_command
#define	MY_core_file_failing_command _bfd_dummy_core_file_failing_command
#endif
#ifndef	MY_core_file_failing_signal
#define	MY_core_file_failing_signal	_bfd_dummy_core_file_failing_signal
#endif
#ifndef	MY_core_file_matches_executable_p
#define	MY_core_file_matches_executable_p	\
				_bfd_dummy_core_file_matches_executable_p
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
#ifndef MY_slurp_armap
#define MY_slurp_armap NAME(aout,slurp_armap)
#endif
#ifndef MY_slurp_extended_name_table
#define MY_slurp_extended_name_table NAME(aout,slurp_extended_name_table)
#endif
#ifndef MY_truncate_arname
#define MY_truncate_arname NAME(aout,truncate_arname)
#endif
#ifndef MY_write_armap
#define MY_write_armap NAME(aout,write_armap)
#endif
#ifndef MY_close_and_cleanup
#define MY_close_and_cleanup NAME(aout,close_and_cleanup)
#endif
#ifndef MY_set_section_contents
#define MY_set_section_contents NAME(aout,set_section_contents)
#endif
#ifndef MY_get_section_contents
#define MY_get_section_contents NAME(aout,get_section_contents)
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
#ifndef MY_openr_next_archived_file
#define MY_openr_next_archived_file NAME(aout,openr_next_archived_file)
#endif
#ifndef MY_find_nearest_line
#define MY_find_nearest_line NAME(aout,find_nearest_line)
#endif
#ifndef MY_generic_stat_arch_elt
#define MY_generic_stat_arch_elt NAME(aout,generic_stat_arch_elt)
#endif
#ifndef MY_sizeof_headers
#define MY_sizeof_headers NAME(aout,sizeof_headers)
#endif
#ifndef MY_bfd_debug_info_start
#define MY_bfd_debug_info_start NAME(aout,bfd_debug_info_start)
#endif
#ifndef MY_bfd_debug_info_end
#define MY_bfd_debug_info_end NAME(aout,bfd_debug_info_end)
#endif
#ifndef MY_bfd_debug_info_accumulat
#define MY_bfd_debug_info_accumulat NAME(aout,bfd_debug_info_accumulat)
#endif
#ifndef MY_reloc_howto_type_lookup
#define MY_reloc_howto_type_lookup NAME(aout,reloc_type_lookup)
#endif
#ifndef MY_make_debug_symbol
#define MY_make_debug_symbol 0
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
bfd_target MY(vec) =
{
  TARGETNAME,		/* name */
  bfd_target_aout_flavour,
#ifdef TARGET_IS_BIG_ENDIAN_P
  true,				/* target byte order (big) */
  true,				/* target headers byte order (big) */
#else
  false,			/* target byte order (little) */
  false,			/* target headers byte order (little) */
#endif
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | DYNAMIC | WP_TEXT | D_PAGED),
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  MY_symbol_leading_char,
  AR_PAD_CHAR,			/* ar_pad_char */
  15,				/* ar_max_namelen */
  3,				/* minimum alignment */
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

  MY_core_file_failing_command,
  MY_core_file_failing_signal,
  MY_core_file_matches_executable_p,
  MY_slurp_armap,
  MY_slurp_extended_name_table,
  MY_truncate_arname,
  MY_write_armap,
  MY_close_and_cleanup,
  MY_set_section_contents,
  MY_get_section_contents,
  MY_new_section_hook,
  MY_get_symtab_upper_bound,
  MY_get_symtab,
  MY_get_reloc_upper_bound,
  MY_canonicalize_reloc,
  MY_make_empty_symbol,
  MY_print_symbol,
  MY_get_symbol_info,
  MY_get_lineno,
  MY_set_arch_mach,
  MY_openr_next_archived_file,
  MY_find_nearest_line,
  MY_generic_stat_arch_elt,
  MY_sizeof_headers,
  MY_bfd_debug_info_start,
  MY_bfd_debug_info_end,
  MY_bfd_debug_info_accumulate,
  bfd_generic_get_relocated_section_contents,
  bfd_generic_relax_section,
  bfd_generic_seclet_link,
  MY_reloc_howto_type_lookup,
  MY_make_debug_symbol,
  (PTR) MY_backend_data,
};
#endif /* MY_BFD_TARGET */
