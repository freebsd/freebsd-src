/* BFD back-end data structures for NLM (NetWare Loadable Modules) files.
   Copyright (C) 1993 Free Software Foundation, Inc.
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

#ifndef _LIBNLM_H_
#define _LIBNLM_H_ 1

#ifdef ARCH_SIZE
#  define NLM_ARCH_SIZE ARCH_SIZE
#endif
#include "nlm/common.h"
#include "nlm/internal.h"
#include "nlm/external.h"

/* A reloc for an imported NLM symbol.  Normal relocs are associated
   with sections, and include a symbol.  These relocs are associated
   with (undefined) symbols, and include a section.  */

struct nlm_relent
{
  /* Section of reloc.  */
  asection *section;
  /* Reloc info (sym_ptr_ptr field set only when canonicalized).  */
  arelent reloc;
};

/* Information we keep for an NLM symbol.  */

typedef struct
{
  /* BFD symbol.  */
  asymbol symbol;
  /* Number of reloc entries for imported symbol.  */
  bfd_size_type rcnt;
  /* Array of reloc information for imported symbol.  */
  struct nlm_relent *relocs;
} nlmNAME(symbol_type);

extern boolean nlm_mkobject PARAMS ((bfd *));
extern boolean nlm_set_arch_mach PARAMS ((bfd *, enum bfd_architecture,
					  unsigned long));

extern void nlmNAME(get_symbol_info)
     PARAMS ((bfd *, asymbol *, symbol_info *));
extern long nlmNAME(get_symtab_upper_bound)
     PARAMS ((bfd *));
extern long nlmNAME(get_symtab)
     PARAMS ((bfd *, asymbol **));
extern asymbol *nlmNAME(make_empty_symbol)
     PARAMS ((bfd *));
extern void nlmNAME(print_symbol)
     PARAMS ((bfd *, PTR, asymbol *, bfd_print_symbol_type));
extern long nlmNAME(get_reloc_upper_bound)
     PARAMS ((bfd *, asection *));
extern long nlmNAME(canonicalize_reloc)
     PARAMS ((bfd *, asection *, arelent **, asymbol **));
extern const bfd_target *nlmNAME(object_p)
     PARAMS ((bfd *));
extern boolean nlmNAME(set_arch_mach)
     PARAMS ((bfd *, enum bfd_architecture, unsigned long));
extern boolean nlmNAME(set_section_contents)
     PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type));
extern boolean nlmNAME(write_object_contents)
     PARAMS ((bfd *));

/* Some private data is stashed away for future use using the tdata pointer
   in the bfd structure.  */

struct nlm_obj_tdata
{
  /* Actual data, but ref like ptr */
  Nlm_Internal_Fixed_Header	nlm_fixed_hdr[1];
  Nlm_Internal_Variable_Header	nlm_variable_hdr[1];
  Nlm_Internal_Version_Header	nlm_version_hdr[1];
  Nlm_Internal_Copyright_Header	nlm_copyright_hdr[1];
  Nlm_Internal_Extended_Header	nlm_extended_hdr[1];
  Nlm_Internal_Custom_Header	nlm_custom_hdr[1];
  Nlm_Internal_Cygnus_Ext_Header nlm_cygnus_ext_hdr[1];
  /* BFD NLM symbols.  */
  nlmNAME(symbol_type)		*nlm_symbols;
  /* Lowest text and data VMA values.  */
  bfd_vma			nlm_text_low;
  bfd_vma			nlm_data_low;
  /* Caches for data read from object file.  */
  arelent *			nlm_reloc_fixups;
  asection **			nlm_reloc_fixup_secs;
  /* Backend specific information.  This should probably be a pointer,
     but that would require yet another entry point to initialize the
     structure.  */
  union
    {
      struct	/* Alpha backend information.  */
	{
	  bfd_vma gp;			/* GP value.  */
	  bfd_vma lita_address;		/* .lita section address.  */
	  bfd_size_type lita_size;	/* .lita section size.  */
	}
      alpha_backend_data;
    }
  backend_data;
};

#define nlm_tdata(bfd)			((bfd) -> tdata.nlm_obj_data)
#define nlm_fixed_header(bfd)		(nlm_tdata(bfd) -> nlm_fixed_hdr)
#define nlm_variable_header(bfd)	(nlm_tdata(bfd) -> nlm_variable_hdr)
#define nlm_version_header(bfd)		(nlm_tdata(bfd) -> nlm_version_hdr)
#define nlm_copyright_header(bfd)	(nlm_tdata(bfd) -> nlm_copyright_hdr)
#define nlm_extended_header(bfd)	(nlm_tdata(bfd) -> nlm_extended_hdr)
#define nlm_custom_header(bfd)		(nlm_tdata(bfd) -> nlm_custom_hdr)
#define nlm_cygnus_ext_header(bfd)	(nlm_tdata(bfd) -> nlm_cygnus_ext_hdr)
#define nlm_get_symbols(bfd)		(nlm_tdata(bfd) -> nlm_symbols)
#define nlm_set_symbols(bfd, p)		(nlm_tdata(bfd) -> nlm_symbols = (p))
#define nlm_set_text_low(bfd, i)	(nlm_tdata(bfd) -> nlm_text_low = (i))
#define nlm_get_text_low(bfd)		(nlm_tdata(bfd) -> nlm_text_low)
#define nlm_set_data_low(bfd, i)	(nlm_tdata(bfd) -> nlm_data_low = (i))
#define nlm_get_data_low(bfd)		(nlm_tdata(bfd) -> nlm_data_low)
#define nlm_relocation_fixups(bfd)	(nlm_tdata(bfd) -> nlm_reloc_fixups)
#define nlm_relocation_fixup_secs(bfd)	(nlm_tdata(bfd)->nlm_reloc_fixup_secs)

#define nlm_alpha_backend_data(bfd) \
  (&nlm_tdata (bfd)->backend_data.alpha_backend_data)

/* This is used when writing out the external relocs. */

struct reloc_and_sec
{
  arelent *rel;
  asection *sec;
};

/* We store some function pointer in the backend structure.  This lets
   different NLM targets share most of the same code, while providing
   slightly different code where necessary.  */

struct nlm_backend_data
{
  /* Signature for this backend.  */
  char signature[NLM_SIGNATURE_SIZE];
  /* Size of the fixed header.  */
  bfd_size_type fixed_header_size;
  /* Size of optional prefix for this backend.  Some backend may
     require this to be a function, but so far a constant is OK.  This
     is for a prefix which precedes the standard NLM fixed header.  */
  bfd_size_type optional_prefix_size;
  /* Architecture.  */
  enum bfd_architecture arch;
  /* Machine.  */
  long mach;
  /* Some NLM formats do not use the uninitialized data section, so
     all uninitialized data must be put into the regular data section
     instead.  */
  boolean no_uninitialized_data;
  /* Some NLM formats have a prefix on the file.  If this function is
     not NULL, it will be called by nlm_object_p.  It should return
     true if this file could match this format, and it should leave
     the BFD such that a bfd_read will pick up the fixed header.  */
  boolean (*nlm_backend_object_p) PARAMS ((bfd *));
  /* Write out the prefix.  This function may be NULL.  This must
     write out the same number of bytes as is in the field
     optional_prefix_size.  */
  boolean (*nlm_write_prefix) PARAMS ((bfd *));
  /* Read a relocation fixup from abfd.  The reloc information is
     machine specific.  The second argument is the symbol if this is
     an import, or NULL if this is a reloc fixup.  This function
     should set the third argument to the section which the reloc
     belongs in, and the fourth argument to the reloc itself; it does
     not need to fill in the sym_ptr_ptr field for a reloc against an
     import symbol.  */
  boolean (*nlm_read_reloc) PARAMS ((bfd *, nlmNAME(symbol_type) *,
				     asection **, arelent *));
  /* To make objcopy to an i386 NLM work, the i386 backend needs a
     chance to work over the relocs.  This is a bit icky.  */
  boolean (*nlm_mangle_relocs) PARAMS ((bfd *, asection *, PTR data,
					bfd_vma offset,
					bfd_size_type count));
  /* Read an import record from abfd.  It would be nice if this
     were in a machine-dependent format, but it doesn't seem to be. */
  boolean (*nlm_read_import) PARAMS ((bfd *, nlmNAME(symbol_type) *));
  /* Write an import record to abfd. */
  boolean (*nlm_write_import) PARAMS ((bfd *, asection *, arelent *));
  /* Set the section for a public symbol.  This may be NULL, in which
     case a default method will be used.  */
  boolean (*nlm_set_public_section) PARAMS ((bfd *, nlmNAME(symbol_type) *));
  /* Get the offset to write out for a public symbol.  This may be
     NULL, in which case a default method will be used.  */
  bfd_vma (*nlm_get_public_offset) PARAMS ((bfd *, asymbol *));
  /* Swap the fixed header in and out */
  void (*nlm_swap_fhdr_in) PARAMS ((bfd *,
				    PTR,
				    Nlm_Internal_Fixed_Header *));
  void (*nlm_swap_fhdr_out) PARAMS ((bfd *,
				     struct nlm_internal_fixed_header *,
				     PTR));
  /* Write out an external reference.  */
  boolean (*nlm_write_external) PARAMS ((bfd *, bfd_size_type,
					 asymbol *,
					 struct reloc_and_sec *));
  boolean (*nlm_write_export) PARAMS ((bfd *, asymbol *, bfd_vma));
};

#define nlm_backend(bfd) \
  ((struct nlm_backend_data *)((bfd) -> xvec -> backend_data))
#define nlm_signature(bfd) \
  (nlm_backend(bfd) -> signature)
#define nlm_fixed_header_size(bfd) \
  (nlm_backend(bfd) -> fixed_header_size)
#define nlm_optional_prefix_size(bfd) \
  (nlm_backend(bfd) -> optional_prefix_size)
#define nlm_architecture(bfd) \
  (nlm_backend(bfd) -> arch)
#define nlm_machine(bfd) \
  (nlm_backend(bfd) -> mach)
#define nlm_no_uninitialized_data(bfd) \
  (nlm_backend(bfd) -> no_uninitialized_data)
#define nlm_backend_object_p_func(bfd) \
  (nlm_backend(bfd) -> nlm_backend_object_p)
#define nlm_write_prefix_func(bfd) \
  (nlm_backend(bfd) -> nlm_write_prefix)
#define nlm_read_reloc_func(bfd) \
  (nlm_backend(bfd) -> nlm_read_reloc)
#define nlm_mangle_relocs_func(bfd) \
  (nlm_backend(bfd) -> nlm_mangle_relocs)
#define nlm_read_import_func(bfd) \
  (nlm_backend(bfd) -> nlm_read_import)
#define nlm_write_import_func(bfd) \
  (nlm_backend(bfd) -> nlm_write_import)
#define nlm_set_public_section_func(bfd) \
  (nlm_backend(bfd) -> nlm_set_public_section)
#define nlm_get_public_offset_func(bfd) \
  (nlm_backend(bfd) -> nlm_get_public_offset)
#define nlm_swap_fixed_header_in_func(bfd) \
  (nlm_backend(bfd) -> nlm_swap_fhdr_in)
#define nlm_swap_fixed_header_out_func(bfd) \
  (nlm_backend(bfd) -> nlm_swap_fhdr_out)
#define nlm_write_external_func(bfd) \
  (nlm_backend(bfd) -> nlm_write_external)
#define nlm_write_export_func(bfd) \
  (nlm_backend(bfd) -> nlm_write_export)

/* The NLM code, data, and uninitialized sections have no names defined
   in the NLM, but bfd wants to give them names, so use the traditional
   UNIX names.  */

#define NLM_CODE_NAME			".text"
#define NLM_INITIALIZED_DATA_NAME	".data"
#define NLM_UNINITIALIZED_DATA_NAME	".bss"

#endif /* _LIBNLM_H_ */
