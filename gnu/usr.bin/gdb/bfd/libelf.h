/* BFD back-end data structures for ELF files.
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef _LIBELF_H_
#define _LIBELF_H_ 1

#include "elf/common.h"
#include "elf/internal.h"
#include "elf/external.h"

/* If size isn't specified as 64 or 32, NAME macro should fail.  */
#ifndef NAME
#if ARCH_SIZE==64
#define NAME(x,y) CAT4(x,64,_,y)
#endif
#if ARCH_SIZE==32
#define NAME(x,y) CAT4(x,32,_,y)
#endif
#endif

#ifndef NAME
#define NAME(x,y) CAT4(x,NOSIZE,_,y)
#endif

#define ElfNAME(X)	NAME(Elf,X)
#define elfNAME(X)	NAME(elf,X)

typedef struct
{
  asymbol symbol;
  Elf_Internal_Sym internal_elf_sym;
  union
    {
      unsigned int hppa_arg_reloc;
      PTR any;
    }
  tc_data;
} elf_symbol_type;

struct elf_backend_data
{
  int use_rela_p;
  int elf_64_p;
  enum bfd_architecture arch;
  void (*elf_info_to_howto) PARAMS ((bfd *, arelent *,
				     Elf_Internal_Rela *));
  void (*elf_info_to_howto_rel) PARAMS ((bfd *, arelent *,
					 Elf_Internal_Rel *));
  bfd_vma maxpagesize;
  void (*write_relocs) PARAMS ((bfd *, asection *, PTR));

  void (*elf_backend_symbol_processing) PARAMS ((bfd *, asymbol *));
  boolean (*elf_backend_symbol_table_processing) PARAMS ((bfd *, elf_symbol_type *, int));
  boolean (*elf_backend_section_processing) PARAMS ((bfd *, Elf32_Internal_Shdr *));
  boolean (*elf_backend_section_from_shdr) PARAMS ((bfd *, Elf32_Internal_Shdr *, char *));
  boolean (*elf_backend_fake_sections) PARAMS ((bfd *, Elf32_Internal_Shdr *, asection *));
  boolean (*elf_backend_section_from_bfd_section) PARAMS ((bfd *, Elf32_Internal_Shdr *, asection *, int *));
};

struct elf_sym_extra
{
  int elf_sym_num;		/* sym# after locals/globals are reordered */
};

typedef struct elf_sym_extra Elf_Sym_Extra;

struct bfd_elf_arch_map {
  enum bfd_architecture bfd_arch;
  int elf_arch;
};

extern const struct bfd_elf_arch_map bfd_elf_arch_map[];
extern const int bfd_elf_arch_map_size;

struct bfd_elf_section_data {
  Elf_Internal_Shdr this_hdr;
  Elf_Internal_Shdr rel_hdr;
  int this_idx, rel_idx;
};
#define elf_section_data(sec)  ((struct bfd_elf_section_data*)sec->used_by_bfd)
#define shdr_name(abfd,shdr)	(elf_shstrtab (abfd)->tab + (shdr)->sh_name)

#define get_elf_backend_data(abfd) \
  ((struct elf_backend_data *) (abfd)->xvec->backend_data)

struct strtab
{
  char *tab;
  int nentries;
  int length;
};

/* Some private data is stashed away for future use using the tdata pointer
   in the bfd structure.  */

struct elf_obj_tdata
{
  Elf_Internal_Ehdr elf_header[1];	/* Actual data, but ref like ptr */
  Elf_Internal_Shdr **elf_sect_ptr;
  Elf_Internal_Phdr *phdr;
  struct strtab *strtab_ptr;
  int num_locals;
  int num_globals;
  Elf_Internal_Sym *internal_syms;
  elf_symbol_type *symbols;	/* elf_symbol_type */
  Elf_Sym_Extra *sym_extra;
  asymbol **section_syms;	/* STT_SECTION symbols for each section */
  int num_section_syms;		/* number of section_syms allocated */
  Elf_Internal_Shdr symtab_hdr;
  Elf_Internal_Shdr shstrtab_hdr;
  Elf_Internal_Shdr strtab_hdr;
  int symtab_section, shstrtab_section, strtab_section;
  file_ptr next_file_pos;
  void *prstatus;		/* The raw /proc prstatus structure */
  void *prpsinfo;		/* The raw /proc prpsinfo structure */
  bfd_vma gp;			/* The gp value (MIPS only, for now) */
  int gp_size;			/* The gp size (MIPS only, for now) */
};

#define elf_tdata(bfd)		((bfd) -> tdata.elf_obj_data)
#define elf_elfheader(bfd)	(elf_tdata(bfd) -> elf_header)
#define elf_elfsections(bfd)	(elf_tdata(bfd) -> elf_sect_ptr)
#define elf_shstrtab(bfd)	(elf_tdata(bfd) -> strtab_ptr)
#define elf_onesymtab(bfd)	(elf_tdata(bfd) -> symtab_section)
#define elf_num_locals(bfd)	(elf_tdata(bfd) -> num_locals)
#define elf_num_globals(bfd)	(elf_tdata(bfd) -> num_globals)
#define elf_sym_extra(bfd)	(elf_tdata(bfd) -> sym_extra)
#define elf_section_syms(bfd)	(elf_tdata(bfd) -> section_syms)
#define elf_num_section_syms(bfd) (elf_tdata(bfd) -> num_section_syms)
#define core_prpsinfo(bfd)	(elf_tdata(bfd) -> prpsinfo)
#define core_prstatus(bfd)	(elf_tdata(bfd) -> prstatus)
#define obj_symbols(bfd)	(elf_tdata(bfd) -> symbols)
#define obj_internal_syms(bfd)	(elf_tdata(bfd) -> internal_syms)
#define elf_gp(bfd)		(elf_tdata(bfd) -> gp)
#define elf_gp_size(bfd)	(elf_tdata(bfd) -> gp_size)

extern char * elf_string_from_elf_section PARAMS ((bfd *, unsigned, unsigned));
extern char * elf_get_str_section PARAMS ((bfd *, unsigned));

#define bfd_elf32_mkobject	bfd_elf_mkobject
#define bfd_elf64_mkobject	bfd_elf_mkobject
#define elf_mkobject		bfd_elf_mkobject

extern unsigned long bfd_elf_hash PARAMS ((CONST unsigned char *));

extern bfd_reloc_status_type bfd_elf_generic_reloc PARAMS ((bfd *,
							    arelent *,
							    asymbol *,
							    PTR,
							    asection *,
							    bfd *));
extern boolean bfd_elf_mkobject PARAMS ((bfd *));
extern Elf_Internal_Shdr *bfd_elf_find_section PARAMS ((bfd *, char *));

extern boolean bfd_elf32_write_object_contents PARAMS ((bfd *));
extern boolean bfd_elf64_write_object_contents PARAMS ((bfd *));

extern bfd_target *bfd_elf32_object_p PARAMS ((bfd *));
extern bfd_target *bfd_elf32_core_file_p PARAMS ((bfd *));
extern char *bfd_elf32_core_file_failing_command PARAMS ((bfd *));
extern int bfd_elf32_core_file_failing_signal PARAMS ((bfd *));
extern boolean bfd_elf32_core_file_matches_executable_p PARAMS ((bfd *,
								 bfd *));
extern boolean bfd_elf32_set_section_contents PARAMS ((bfd *, sec_ptr, PTR,
						       file_ptr,
						       bfd_size_type));

extern unsigned int bfd_elf32_get_symtab_upper_bound PARAMS ((bfd *));
extern unsigned int bfd_elf32_get_symtab PARAMS ((bfd *, asymbol **));
extern unsigned int bfd_elf32_get_reloc_upper_bound PARAMS ((bfd *, sec_ptr));
extern unsigned int bfd_elf32_canonicalize_reloc PARAMS ((bfd *, sec_ptr,
							  arelent **,
							  asymbol **));
extern asymbol *bfd_elf32_make_empty_symbol PARAMS ((bfd *));
extern void bfd_elf32_print_symbol PARAMS ((bfd *, PTR, asymbol *,
					    bfd_print_symbol_type));
extern void bfd_elf32_get_symbol_info PARAMS ((bfd *, asymbol *,
					       symbol_info *));
extern alent *bfd_elf32_get_lineno PARAMS ((bfd *, asymbol *));
extern boolean bfd_elf32_set_arch_mach PARAMS ((bfd *, enum bfd_architecture,
						unsigned long));
extern boolean bfd_elf32_find_nearest_line PARAMS ((bfd *, asection *,
						    asymbol **,
						    bfd_vma, CONST char **,
						    CONST char **,
						    unsigned int *));
extern int bfd_elf32_sizeof_headers PARAMS ((bfd *, boolean));
extern void bfd_elf32__write_relocs PARAMS ((bfd *, asection *, PTR));
extern boolean bfd_elf32_new_section_hook PARAMS ((bfd *, asection *));

/* If the target doesn't have reloc handling written yet:  */
extern void bfd_elf32_no_info_to_howto PARAMS ((bfd *, arelent *,
						Elf32_Internal_Rela *));

extern bfd_target *bfd_elf64_object_p PARAMS ((bfd *));
extern bfd_target *bfd_elf64_core_file_p PARAMS ((bfd *));
extern char *bfd_elf64_core_file_failing_command PARAMS ((bfd *));
extern int bfd_elf64_core_file_failing_signal PARAMS ((bfd *));
extern boolean bfd_elf64_core_file_matches_executable_p PARAMS ((bfd *,
								 bfd *));
extern boolean bfd_elf64_set_section_contents PARAMS ((bfd *, sec_ptr, PTR,
						       file_ptr,
						       bfd_size_type));

extern unsigned int bfd_elf64_get_symtab_upper_bound PARAMS ((bfd *));
extern unsigned int bfd_elf64_get_symtab PARAMS ((bfd *, asymbol **));
extern unsigned int bfd_elf64_get_reloc_upper_bound PARAMS ((bfd *, sec_ptr));
extern unsigned int bfd_elf64_canonicalize_reloc PARAMS ((bfd *, sec_ptr,
							  arelent **,
							  asymbol **));
extern asymbol *bfd_elf64_make_empty_symbol PARAMS ((bfd *));
extern void bfd_elf64_print_symbol PARAMS ((bfd *, PTR, asymbol *,
					    bfd_print_symbol_type));
extern void bfd_elf64_get_symbol_info PARAMS ((bfd *, asymbol *,
					       symbol_info *));
extern alent *bfd_elf64_get_lineno PARAMS ((bfd *, asymbol *));
extern boolean bfd_elf64_set_arch_mach PARAMS ((bfd *, enum bfd_architecture,
						unsigned long));
extern boolean bfd_elf64_find_nearest_line PARAMS ((bfd *, asection *,
						    asymbol **,
						    bfd_vma, CONST char **,
						    CONST char **,
						    unsigned int *));
extern int bfd_elf64_sizeof_headers PARAMS ((bfd *, boolean));
extern void bfd_elf64__write_relocs PARAMS ((bfd *, asection *, PTR));
extern boolean bfd_elf64_new_section_hook PARAMS ((bfd *, asection *));

/* If the target doesn't have reloc handling written yet:  */
extern void bfd_elf64_no_info_to_howto PARAMS ((bfd *, arelent *,
						Elf64_Internal_Rela *));

#endif /* _LIBELF_H_ */
