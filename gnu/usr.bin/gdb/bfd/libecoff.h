/* BFD ECOFF object file private structure.
   Copyright (C) 1993 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

/* This is the backend information kept for ECOFF files.  This
   structure is constant for a particular backend.  The first element
   is the COFF backend data structure, so that ECOFF targets can use
   the generic COFF code.  */

#define ecoff_backend(abfd) \
  ((struct ecoff_backend_data *) (abfd)->xvec->backend_data)

struct ecoff_backend_data
{
  /* COFF backend information.  This must be the first field.  */
  bfd_coff_backend_data coff;
  /* Supported architecture.  */
  enum bfd_architecture arch;
  /* Symbol table magic number.  */
  int sym_magic;
  /* Initial portion of armap string.  */
  const char *armap_start;
  /* Alignment of debugging information.  E.g., 4.  */
  bfd_size_type debug_align;
  /* The page boundary used to align sections in a demand-paged
     executable file.  E.g., 0x1000.  */
  bfd_vma round;
  /* Bitsize of constructor entries.  */
  unsigned int constructor_bitsize;
  /* Reloc to use for constructor entries.  */
  CONST struct reloc_howto_struct *constructor_reloc;
  /* Sizes of external symbolic information.  */
  bfd_size_type external_hdr_size;
  bfd_size_type external_dnr_size;
  bfd_size_type external_pdr_size;
  bfd_size_type external_sym_size;
  bfd_size_type external_opt_size;
  bfd_size_type external_fdr_size;
  bfd_size_type external_rfd_size;
  bfd_size_type external_ext_size;
  /* Functions to swap in external symbolic data.  */
  void (*swap_hdr_in) PARAMS ((bfd *, PTR, HDRR *));
  void (*swap_dnr_in) PARAMS ((bfd *, PTR, DNR *));
  void (*swap_pdr_in) PARAMS ((bfd *, PTR, PDR *));
  void (*swap_sym_in) PARAMS ((bfd *, PTR, SYMR *));
  void (*swap_opt_in) PARAMS ((bfd *, PTR, OPTR *));
  void (*swap_fdr_in) PARAMS ((bfd *, PTR, FDR *));
  void (*swap_rfd_in) PARAMS ((bfd *, PTR, RFDT *));
  void (*swap_ext_in) PARAMS ((bfd *, PTR, EXTR *));
  /* Functions to swap out external symbolic data.  */
  void (*swap_hdr_out) PARAMS ((bfd *, const HDRR *, PTR));
  void (*swap_dnr_out) PARAMS ((bfd *, const DNR *, PTR));
  void (*swap_pdr_out) PARAMS ((bfd *, const PDR *, PTR));
  void (*swap_sym_out) PARAMS ((bfd *, const SYMR *, PTR));
  void (*swap_opt_out) PARAMS ((bfd *, const OPTR *, PTR));
  void (*swap_fdr_out) PARAMS ((bfd *, const FDR *, PTR));
  void (*swap_rfd_out) PARAMS ((bfd *, const RFDT *, PTR));
  void (*swap_ext_out) PARAMS ((bfd *, const EXTR *, PTR));
  /* It so happens that the auxiliary type information has the same
     type and format for all known ECOFF targets.  I don't see any
     reason that that should change, so at least for now the auxiliary
     swapping information is not in this table.  */
  /* External reloc size.  */
  bfd_size_type external_reloc_size;
  /* Reloc swapping functions.  */
  void (*swap_reloc_in) PARAMS ((bfd *, PTR, struct internal_reloc *));
  void (*swap_reloc_out) PARAMS ((bfd *, const struct internal_reloc *, PTR));
  /* Backend reloc tweaking.  */
  void (*finish_reloc) PARAMS ((bfd *, struct internal_reloc *, arelent *));
};

/* This is the target specific information kept for ECOFF files.  */

#define ecoff_data(abfd) ((abfd)->tdata.ecoff_obj_data)

typedef struct ecoff_tdata
{
  /* The reloc file position, set by
     ecoff_compute_section_file_positions.  */
  file_ptr reloc_filepos;

  /* The symbol table file position, set by ecoff_mkobject_hook.  */
  file_ptr sym_filepos;

  /* The start and end of the text segment.  Only valid for an
     existing file, not for one we are creating.  */
  unsigned long text_start;
  unsigned long text_end;

  /* The cached gp value.  This is used when relocating.  */
  bfd_vma gp;

  /* The maximum size of objects to optimize using gp.  This is
     typically set by the -G option to the compiler, assembler or
     linker.  */
  int gp_size;

  /* The register masks.  When linking, all the masks found in the
     input files are combined into the masks of the output file.
     These are not all used for all targets, but that's OK, because
     the relevant ones are the only ones swapped in and out.  */
  unsigned long gprmask;
  unsigned long fprmask;
  unsigned long cprmask[4];

  /* The size of the unswapped ECOFF symbolic information.  */
  bfd_size_type raw_size;

  /* The unswapped ECOFF symbolic information.  */
  PTR raw_syments;

  /* The swapped ECOFF symbolic header.  */
  HDRR symbolic_header;

  /* Pointers to the unswapped symbolic information.  */
  unsigned char *line;
  PTR external_dnr;	/* struct dnr_ext */
  PTR external_pdr;	/* struct pdr_ext */
  PTR external_sym;	/* struct sym_ext */
  PTR external_opt;	/* struct opt_ext */
  union aux_ext *external_aux;
  char *ss;
  char *ssext;
  PTR external_fdr;	/* struct fdr_ext */
  PTR external_rfd;	/* struct rfd_ext */
  PTR external_ext;	/* struct ext_ext */

  /* The swapped FDR information.  */
  FDR *fdr;

  /* The FDR index.  This is set for an input BFD to a link so that
     the external symbols can set their FDR index correctly.  */
  unsigned int ifdbase;

  /* The canonical BFD symbols.  */
  struct ecoff_symbol_struct *canonical_symbols;

} ecoff_data_type;

/* Each canonical asymbol really looks like this.  */

typedef struct ecoff_symbol_struct
{
  /* The actual symbol which the rest of BFD works with */
  asymbol symbol;

  /* The fdr for this symbol.  */
  FDR *fdr;

  /* true if this is a local symbol rather than an external one.  */
  boolean local;

  /* A pointer to the unswapped hidden information for this symbol.
     This is either a struct sym_ext or a struct ext_ext, depending on
     the value of the local field above.  */
  PTR native;
} ecoff_symbol_type;

/* We take the address of the first element of a asymbol to ensure that the
   macro is only ever applied to an asymbol.  */
#define ecoffsymbol(asymbol) ((ecoff_symbol_type *) (&((asymbol)->the_bfd)))

/* This is a hack borrowed from coffcode.h; we need to save the index
   of an external symbol when we write it out so that can set the
   symbol index correctly when we write out the relocs.  */
#define ecoff_get_sym_index(symbol) ((unsigned long) (symbol)->udata)
#define ecoff_set_sym_index(symbol, idx) ((symbol)->udata = (PTR) (idx))

/* Make an ECOFF object.  */
extern boolean ecoff_mkobject PARAMS ((bfd *));

/* Read in the ECOFF symbolic debugging information.  */
extern boolean ecoff_slurp_symbolic_info PARAMS ((bfd *));

/* Generic ECOFF BFD backend vectors.  */
extern asymbol *ecoff_make_empty_symbol PARAMS ((bfd *abfd));
extern unsigned int ecoff_get_symtab_upper_bound PARAMS ((bfd *abfd));
extern unsigned int ecoff_get_symtab PARAMS ((bfd *abfd,
					      asymbol **alocation));
extern void ecoff_get_symbol_info PARAMS ((bfd *abfd,
					   asymbol *symbol,
					   symbol_info *ret));
extern void ecoff_print_symbol PARAMS ((bfd *abfd, PTR filep,
					asymbol *symbol,
					bfd_print_symbol_type how));
extern unsigned int ecoff_canonicalize_reloc PARAMS ((bfd *abfd,
						      asection *section,
						      arelent **relptr,
						      asymbol **symbols));
extern boolean ecoff_find_nearest_line PARAMS ((bfd *abfd,
						asection *section,
						asymbol **symbols,
						bfd_vma offset,
						CONST char **filename_ptr,
						CONST char **fnname_ptr,
						unsigned int *retline_ptr));
extern boolean ecoff_bfd_seclet_link PARAMS ((bfd *abfd, PTR data,
					      boolean relocateable));
extern boolean ecoff_set_arch_mach PARAMS ((bfd *abfd,
					    enum bfd_architecture arch,
					    unsigned long machine));
extern int ecoff_sizeof_headers PARAMS ((bfd *abfd, boolean reloc));
extern boolean ecoff_set_section_contents PARAMS ((bfd *abfd,
						   asection *section,
						   PTR location,
						   file_ptr offset,
						   bfd_size_type count));
extern boolean ecoff_get_section_contents PARAMS ((bfd *abfd,
						   asection *section,
						   PTR location,
						   file_ptr offset,
						   bfd_size_type count));
extern boolean ecoff_write_object_contents PARAMS ((bfd *abfd));
extern boolean ecoff_slurp_armap PARAMS ((bfd *abfd));
extern boolean ecoff_write_armap PARAMS ((bfd *abfd, unsigned int elength,
					  struct orl *map,
					  unsigned int orl_count,
					  int stridx));
#define ecoff_slurp_extended_name_table	_bfd_slurp_extended_name_table
extern bfd_target *ecoff_archive_p PARAMS ((bfd *abfd));
#define ecoff_get_lineno \
  ((alent *(*) PARAMS ((bfd *, asymbol *))) bfd_nullvoidptr)
#define ecoff_truncate_arname		bfd_dont_truncate_arname
#define ecoff_openr_next_archived_file	bfd_generic_openr_next_archived_file
#define ecoff_generic_stat_arch_elt	bfd_generic_stat_arch_elt
#define ecoff_get_reloc_upper_bound	coff_get_reloc_upper_bound
#define	ecoff_close_and_cleanup		bfd_generic_close_and_cleanup
#define ecoff_bfd_debug_info_start	bfd_void
#define ecoff_bfd_debug_info_end	bfd_void
#define ecoff_bfd_debug_info_accumulate	\
  ((void (*) PARAMS ((bfd *, struct sec *))) bfd_void)
#define ecoff_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define ecoff_bfd_relax_section		bfd_generic_relax_section
#define ecoff_bfd_make_debug_symbol \
  ((asymbol *(*) PARAMS ((bfd *, void *, unsigned long))) bfd_nullvoidptr)

/* Hook functions for the generic COFF section reading code.  */
extern PTR ecoff_mkobject_hook PARAMS ((bfd *, PTR filehdr, PTR aouthdr));
extern asection *ecoff_make_section_hook PARAMS ((bfd *abfd, char *name));
extern boolean ecoff_new_section_hook PARAMS ((bfd *abfd,
					       asection *section));
#define ecoff_set_alignment_hook \
  ((void (*) PARAMS ((bfd *, asection *, PTR))) bfd_void)
extern boolean ecoff_set_arch_mach_hook PARAMS ((bfd *abfd, PTR filehdr));
extern long ecoff_sec_to_styp_flags PARAMS ((CONST char *name,
					     flagword flags));
extern flagword ecoff_styp_to_sec_flags PARAMS ((bfd *abfd, PTR hdr));
extern boolean ecoff_slurp_symbol_table PARAMS ((bfd *abfd));
