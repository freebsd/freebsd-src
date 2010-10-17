/* Copyright 2002 Free Software Foundation, Inc.

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

#undef  F_LSYMS
#define	F_LSYMS		F_LSYMS_TICOFF

static bfd_boolean
ticoff0_bad_format_hook (abfd, filehdr)
     bfd *abfd;
     PTR filehdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

  if (COFF0_BADMAG (*internal_f))
    return FALSE;

  return TRUE;
}

static bfd_boolean
ticoff1_bad_format_hook (abfd, filehdr)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR filehdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

  if (COFF1_BADMAG (*internal_f))
    return FALSE;

  return TRUE;
}

/* Replace the stock _bfd_coff_is_local_label_name
   to recognize TI COFF local labels.  */
static bfd_boolean 
ticoff_bfd_is_local_label_name (abfd, name)
  bfd *abfd ATTRIBUTE_UNUSED;
  const char *name;
{
  if (TICOFF_LOCAL_LABEL_P(name))
    return TRUE;
  return FALSE;
}

#define coff_bfd_is_local_label_name ticoff_bfd_is_local_label_name

/* Customize coffcode.h; the default coff_ functions are set up to use COFF2; 
   coff_bad_format_hook uses BADMAG, so set that for COFF2.  The COFF1
   and COFF0 vectors use custom _bad_format_hook procs instead of setting
   BADMAG.  */ 
#define BADMAG(x) COFF2_BADMAG(x)
#include "coffcode.h"

/* COFF0 differs in file/section header size and relocation entry size.  */
static const bfd_coff_backend_data ticoff0_swap_table = 
{
  coff_SWAP_aux_in, coff_SWAP_sym_in, coff_SWAP_lineno_in,
  coff_SWAP_aux_out, coff_SWAP_sym_out,
  coff_SWAP_lineno_out, coff_SWAP_reloc_out,
  coff_SWAP_filehdr_out, coff_SWAP_aouthdr_out,
  coff_SWAP_scnhdr_out,
  FILHSZ_V0, AOUTSZ, SCNHSZ_V01, SYMESZ, AUXESZ, RELSZ_V0, LINESZ, FILNMLEN,
#ifdef COFF_LONG_FILENAMES
  TRUE,
#else
  FALSE,
#endif
#ifdef COFF_LONG_SECTION_NAMES
  TRUE,
#else
  FALSE,
#endif
  COFF_DEFAULT_SECTION_ALIGNMENT_POWER,
  coff_SWAP_filehdr_in, coff_SWAP_aouthdr_in, coff_SWAP_scnhdr_in,
  coff_SWAP_reloc_in, ticoff0_bad_format_hook, coff_set_arch_mach_hook,
  coff_mkobject_hook, styp_to_sec_flags, coff_set_alignment_hook,
  coff_slurp_symbol_table, symname_in_debug_hook, coff_pointerize_aux_hook,
  coff_print_aux, coff_reloc16_extra_cases, coff_reloc16_estimate,
  coff_classify_symbol, coff_compute_section_file_positions,
  coff_start_final_link, coff_relocate_section, coff_rtype_to_howto,
  coff_adjust_symndx, coff_link_add_one_symbol,
  coff_link_output_has_begun, coff_final_link_postscript
};

/* COFF1 differs in section header size.  */
static const bfd_coff_backend_data ticoff1_swap_table = 
{
  coff_SWAP_aux_in, coff_SWAP_sym_in, coff_SWAP_lineno_in,
  coff_SWAP_aux_out, coff_SWAP_sym_out,
  coff_SWAP_lineno_out, coff_SWAP_reloc_out,
  coff_SWAP_filehdr_out, coff_SWAP_aouthdr_out,
  coff_SWAP_scnhdr_out,
  FILHSZ, AOUTSZ, SCNHSZ_V01, SYMESZ, AUXESZ, RELSZ, LINESZ, FILNMLEN,
#ifdef COFF_LONG_FILENAMES
  TRUE,
#else
  FALSE,
#endif
#ifdef COFF_LONG_SECTION_NAMES
  TRUE,
#else
  FALSE,
#endif
  COFF_DEFAULT_SECTION_ALIGNMENT_POWER,
  coff_SWAP_filehdr_in, coff_SWAP_aouthdr_in, coff_SWAP_scnhdr_in,
  coff_SWAP_reloc_in, ticoff1_bad_format_hook, coff_set_arch_mach_hook,
  coff_mkobject_hook, styp_to_sec_flags, coff_set_alignment_hook,
  coff_slurp_symbol_table, symname_in_debug_hook, coff_pointerize_aux_hook,
  coff_print_aux, coff_reloc16_extra_cases, coff_reloc16_estimate,
  coff_classify_symbol, coff_compute_section_file_positions,
  coff_start_final_link, coff_relocate_section, coff_rtype_to_howto,
  coff_adjust_symndx, coff_link_add_one_symbol,
  coff_link_output_has_begun, coff_final_link_postscript
};

