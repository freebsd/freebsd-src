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
#include "bfdlink.h"

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

/* Information held for an ELF symbol.  The first field is the
   corresponding asymbol.  Every symbol is an ELF file is actually a
   pointer to this structure, although it is often handled as a
   pointer to an asymbol.  */

typedef struct
{
  /* The BFD symbol.  */
  asymbol symbol;
  /* ELF symbol information.  */
  Elf_Internal_Sym internal_elf_sym;
  /* Backend specific information.  */
  union
    {
      unsigned int hppa_arg_reloc;
      PTR mips_extr;
      PTR any;
    }
  tc_data;
} elf_symbol_type;

/* ELF linker hash table entries.  */

struct elf_link_hash_entry
{
  struct bfd_link_hash_entry root;

  /* Symbol index in output file.  This is initialized to -1.  It is
     set to -2 if the symbol is used by a reloc.  */
  long indx;

  /* Symbol size.  */
  bfd_size_type size;

  /* Symbol alignment (common symbols only).  */
  bfd_size_type align;

  /* Symbol index as a dynamic symbol.  Initialized to -1, and remains
     -1 if this is not a dynamic symbol.  */
  long dynindx;

  /* String table index in .dynstr if this is a dynamic symbol.  */
  unsigned long dynstr_index;

  /* If this is a weak defined symbol from a dynamic object, this
     field points to a defined symbol with the same value, if there is
     one.  Otherwise it is NULL.  */
  struct elf_link_hash_entry *weakdef;

  /* Symbol type (STT_NOTYPE, STT_OBJECT, etc.).  */
  char type;

  /* Some flags; legal values follow.  */
  unsigned char elf_link_hash_flags;
  /* Symbol is referenced by a non-shared object.  */
#define ELF_LINK_HASH_REF_REGULAR 01
  /* Symbol is defined by a non-shared object.  */
#define ELF_LINK_HASH_DEF_REGULAR 02
  /* Symbol is referenced by a shared object.  */
#define ELF_LINK_HASH_REF_DYNAMIC 04
  /* Symbol is defined by a shared object.  */
#define ELF_LINK_HASH_DEF_DYNAMIC 010
  /* Symbol is referenced by two or more shared objects.  */
#define ELF_LINK_HASH_REF_DYNAMIC_MULTIPLE 020
  /* Symbol is defined by two or more shared objects.  */
#define ELF_LINK_HASH_DEF_DYNAMIC_MULTIPLE 040
  /* Dynamic symbol has been adjustd.  */
#define ELF_LINK_HASH_DYNAMIC_ADJUSTED 0100
  /* Symbol is defined as weak.  */
#define ELF_LINK_HASH_DEFINED_WEAK 0200
};

/* ELF linker hash table.  */

struct elf_link_hash_table
{
  struct bfd_link_hash_table root;
  /* The first dynamic object found during a link.  We create several
     special input sections when linking against dynamic objects, and
     we simply attach them to the first one found.  */
  bfd *dynobj;
  /* The number of symbols found in the link which must be put into
     the .dynsym section.  */
  size_t dynsymcount;
  /* The string table of dynamic symbols, which becomes the .dynstr
     section.  */
  struct strtab *dynstr;
  /* The number of buckets in the hash table in the .hash section.
     This is based on the number of dynamic symbols.  */
  size_t bucketcount;
};

/* Look up an entry in an ELF linker hash table.  */

#define elf_link_hash_lookup(table, string, create, copy, follow)	\
  ((struct elf_link_hash_entry *)					\
   bfd_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

/* Traverse an ELF linker hash table.  */

#define elf_link_hash_traverse(table, func, info)			\
  (bfd_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct bfd_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the ELF linker hash table from a link_info structure.  */

#define elf_hash_table(p) ((struct elf_link_hash_table *) ((p)->hash))

/* Constant information held for an ELF backend.  */

struct elf_backend_data
{
  /* Whether the backend uses REL or RELA relocations.  FIXME: some
     ELF backends use both.  When we need to support one, this whole
     approach will need to be changed.  */
  int use_rela_p;

  /* Whether this backend is 64 bits or not.  FIXME: Who cares?  */
  int elf_64_p;

  /* The architecture for this backend.  */
  enum bfd_architecture arch;

  /* The ELF machine code (EM_xxxx) for this backend.  */
  int elf_machine_code;

  /* The maximum page size for this backend.  */
  bfd_vma maxpagesize;

  /* This is true if the linker should act like collect and gather
     global constructors and destructors by name.  This is true for
     MIPS ELF because the Irix 5 tools can not handle the .init
     section.  */
  boolean collect;

  /* A function to translate an ELF RELA relocation to a BFD arelent
     structure.  */
  void (*elf_info_to_howto) PARAMS ((bfd *, arelent *,
				     Elf_Internal_Rela *));

  /* A function to translate an ELF REL relocation to a BFD arelent
     structure.  */
  void (*elf_info_to_howto_rel) PARAMS ((bfd *, arelent *,
					 Elf_Internal_Rel *));

  /* A function to determine whether a symbol is global when
     partitioning the symbol table into local and global symbols.
     This should be NULL for most targets, in which case the correct
     thing will be done.  MIPS ELF, at least on the Irix 5, has
     special requirements.  */
  boolean (*elf_backend_sym_is_global) PARAMS ((bfd *, asymbol *));

  /* The remaining functions are hooks which are called only if they
     are not NULL.  */

  /* A function to permit a backend specific check on whether a
     particular BFD format is relevant for an object file, and to
     permit the backend to set any global information it wishes.  When
     this is called elf_elfheader is set, but anything else should be
     used with caution.  If this returns false, the check_format
     routine will return a bfd_error_wrong_format error.  */
  boolean (*elf_backend_object_p) PARAMS ((bfd *));

  /* A function to do additional symbol processing when reading the
     ELF symbol table.  This is where any processor-specific special
     section indices are handled.  */
  void (*elf_backend_symbol_processing) PARAMS ((bfd *, asymbol *));

  /* A function to do additional symbol processing after reading the
     entire ELF symbol table.  */
  boolean (*elf_backend_symbol_table_processing) PARAMS ((bfd *,
							  elf_symbol_type *,
							  int));

  /* A function to do additional processing on the ELF section header
     just before writing it out.  This is used to set the flags and
     type fields for some sections, or to actually write out data for
     unusual sections.  */
  boolean (*elf_backend_section_processing) PARAMS ((bfd *,
						     Elf32_Internal_Shdr *));

  /* A function to handle unusual section types when creating BFD
     sections from ELF sections.  */
  boolean (*elf_backend_section_from_shdr) PARAMS ((bfd *,
						    Elf32_Internal_Shdr *,
						    char *));

  /* A function to set up the ELF section header for a BFD section in
     preparation for writing it out.  This is where the flags and type
     fields are set for unusual sections.  */
  boolean (*elf_backend_fake_sections) PARAMS ((bfd *, Elf32_Internal_Shdr *,
						asection *));

  /* A function to get the ELF section index for a BFD section.  If
     this returns true, the section was found.  If it is a normal ELF
     section, *RETVAL should be left unchanged.  If it is not a normal
     ELF section *RETVAL should be set to the SHN_xxxx index.  */
  boolean (*elf_backend_section_from_bfd_section)
    PARAMS ((bfd *, Elf32_Internal_Shdr *, asection *, int *retval));

  /* If this field is not NULL, it is called by the add_symbols phase
     of a link just before adding a symbol to the global linker hash
     table.  It may modify any of the fields as it wishes.  If *NAME
     is set to NULL, the symbol will be skipped rather than being
     added to the hash table.  This function is responsible for
     handling all processor dependent symbol bindings and section
     indices, and must set at least *FLAGS and *SEC for each processor
     dependent case; failure to do so will cause a link error.  */
  boolean (*elf_add_symbol_hook)
    PARAMS ((bfd *abfd, struct bfd_link_info *info,
	     const Elf_Internal_Sym *, const char **name,
	     flagword *flags, asection **sec, bfd_vma *value));

  /* If this field is not NULL, it is called by the elf_link_output_sym
     phase of a link for each symbol which will appear in the object file.  */
  boolean (*elf_backend_link_output_symbol_hook)
    PARAMS ((bfd *, struct bfd_link_info *info, const char *,
	     Elf_Internal_Sym *, asection *));

  /* The CREATE_DYNAMIC_SECTIONS function is called by the ELF backend
     linker the first time it encounters a dynamic object in the link.
     This function must create any sections required for dynamic
     linking.  The ABFD argument is a dynamic object.  The .interp,
     .dynamic, .dynsym, .dynstr, and .hash functions have already been
     created, and this function may modify the section flags if
     desired.  This function will normally create the .got and .plt
     sections, but different backends have different requirements.  */
  boolean (*elf_backend_create_dynamic_sections)
    PARAMS ((bfd *abfd, struct bfd_link_info *info));

  /* The ADJUST_DYNAMIC_SYMBOL function is called by the ELF backend
     linker for every symbol which is defined by a dynamic object and
     referenced by a regular object.  This is called after all the
     input files have been seen, but before the SIZE_DYNAMIC_SECTIONS
     function has been called.  The hash table entry should be
     bfd_link_hash_defined, and it should be defined in a section from
     a dynamic object.  Dynamic object sections are not included in
     the final link, and this function is responsible for changing the
     value to something which the rest of the link can deal with.
     This will normally involve adding an entry to the .plt or .got or
     some such section, and setting the symbol to point to that.  */
  boolean (*elf_backend_adjust_dynamic_symbol)
    PARAMS ((struct bfd_link_info *info, struct elf_link_hash_entry *h));

  /* The SIZE_DYNAMIC_SECTIONS function is called by the ELF backend
     linker after all the linker input files have been seen but before
     the sections sizes have been set.  This is called after
     ADJUST_DYNAMIC_SYMBOL has been called on all appropriate symbols.
     It is only called when linking against a dynamic object.  It must
     set the sizes of the dynamic sections, and may fill in their
     contents as well.  The generic ELF linker can handle the .dynsym,
     .dynstr and .hash sections.  This function must handle the
     .interp section and any sections created by the
     CREATE_DYNAMIC_SECTIONS entry point.  */
  boolean (*elf_backend_size_dynamic_sections)
    PARAMS ((bfd *output_bfd, struct bfd_link_info *info));

  /* The RELOCATE_SECTION function is called by the ELF backend linker
     to handle the relocations for a section.

     The relocs are always passed as Rela structures; if the section
     actually uses Rel structures, the r_addend field will always be
     zero.

     This function is responsible for adjust the section contents as
     necessary, and (if using Rela relocs and generating a
     relocateable output file) adjusting the reloc addend as
     necessary.

     This function does not have to worry about setting the reloc
     address or the reloc symbol index.

     LOCAL_SYMS is a pointer to the swapped in local symbols.

     LOCAL_SECTIONS is an array giving the section in the input file
     corresponding to the st_shndx field of each local symbol.

     The global hash table entry for the global symbols can be found
     via elf_sym_hashes (input_bfd).

     When generating relocateable output, this function must handle
     STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
     going to be the section symbol corresponding to the output
     section, which means that the addend must be adjusted
     accordingly.  */
  boolean (*elf_backend_relocate_section)
    PARAMS ((bfd *output_bfd, struct bfd_link_info *info,
	     bfd *input_bfd, asection *input_section, bfd_byte *contents,
	     Elf_Internal_Rela *relocs, Elf_Internal_Sym *local_syms,
	     asection **local_sections, char *output_names));

  /* The FINISH_DYNAMIC_SYMBOL function is called by the ELF backend
     linker just before it writes a symbol out to the .dynsym section.
     The processor backend may make any required adjustment to the
     symbol.  It may also take the opportunity to set contents of the
     dynamic sections.  Note that FINISH_DYNAMIC_SYMBOL is called on
     all .dynsym symbols, while ADJUST_DYNAMIC_SYMBOL is only called
     on those symbols which are defined by a dynamic object.  */
  boolean (*elf_backend_finish_dynamic_symbol)
    PARAMS ((bfd *output_bfd, struct bfd_link_info *info,
	     struct elf_link_hash_entry *h, Elf_Internal_Sym *sym));

  /* The FINISH_DYNAMIC_SECTIONS function is called by the ELF backend
     linker just before it writes all the dynamic sections out to the
     output file.  The FINISH_DYNAMIC_SYMBOL will have been called on
     all dynamic symbols.  */
  boolean (*elf_backend_finish_dynamic_sections)
    PARAMS ((bfd *output_bfd, struct bfd_link_info *info));

  /* A function to do any beginning processing needed for the ELF file
     before building the ELF headers and computing file positions.  */
  void (*elf_backend_begin_write_processing)
    PARAMS ((bfd *, struct bfd_link_info *));

  /* A function to do any final processing needed for the ELF file
     before writing it out.  */
  void (*elf_backend_final_write_processing)
    PARAMS ((bfd *, struct bfd_link_info *));

  /* The swapping table to use when dealing with ECOFF information.
     Used for the MIPS ELF .mdebug section.  */
  const struct ecoff_debug_swap *elf_backend_ecoff_debug_swap;
};

struct elf_sym_extra
{
  int elf_sym_num;		/* sym# after locals/globals are reordered */
};

typedef struct elf_sym_extra Elf_Sym_Extra;

/* Information stored for each BFD section in an ELF file.  This
   structure is allocated by elf_new_section_hook.  */

struct bfd_elf_section_data {
  /* The ELF header for this section.  */
  Elf_Internal_Shdr this_hdr;
  /* The ELF header for the reloc section associated with this
     section, if any.  */
  Elf_Internal_Shdr rel_hdr;
  /* The ELF section number of this section.  Only used for an output
     file.  */
  int this_idx;
  /* The ELF section number of the reloc section associated with this
     section, if any.  Only used for an output file.  */
  int rel_idx;
  /* Used by the backend linker to store the symbol hash table entries
     associated with relocs against global symbols.  */
  struct elf_link_hash_entry **rel_hashes;
  /* A pointer to the unswapped external relocs; this may be NULL.  */
  PTR relocs;
};

#define elf_section_data(sec)  ((struct bfd_elf_section_data*)sec->used_by_bfd)

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
  Elf_Sym_Extra *sym_extra;
  asymbol **section_syms;	/* STT_SECTION symbols for each section */
  int num_section_syms;		/* number of section_syms allocated */
  Elf_Internal_Shdr symtab_hdr;
  Elf_Internal_Shdr shstrtab_hdr;
  Elf_Internal_Shdr strtab_hdr;
  Elf_Internal_Shdr dynsymtab_hdr;
  Elf_Internal_Shdr dynstrtab_hdr;
  int symtab_section, shstrtab_section, strtab_section, dynsymtab_section;
  file_ptr next_file_pos;
  void *prstatus;		/* The raw /proc prstatus structure */
  void *prpsinfo;		/* The raw /proc prpsinfo structure */
  bfd_vma gp;			/* The gp value (MIPS only, for now) */
  int gp_size;			/* The gp size (MIPS only, for now) */

  /* A mapping from external symbols to entries in the linker hash
     table, used when linking.  This is indexed by the symbol index
     minus the sh_info field of the symbol table header.  */
  struct elf_link_hash_entry **sym_hashes;

  /* The linker ELF emulation code needs to let the backend ELF linker
     know what filename should be used for a dynamic object if the
     dynamic object is found using a search.  This field is used to
     hold that information.  */
  const char *dt_needed_name;

  /* Irix 5 often screws up the symbol table, sorting local symbols
     after global symbols.  This flag is set if the symbol table in
     this BFD appears to be screwed up.  If it is, we ignore the
     sh_info field in the symbol table header, and always read all the
     symbols.  */
  boolean bad_symtab;
};

#define elf_tdata(bfd)		((bfd) -> tdata.elf_obj_data)
#define elf_elfheader(bfd)	(elf_tdata(bfd) -> elf_header)
#define elf_elfsections(bfd)	(elf_tdata(bfd) -> elf_sect_ptr)
#define elf_shstrtab(bfd)	(elf_tdata(bfd) -> strtab_ptr)
#define elf_onesymtab(bfd)	(elf_tdata(bfd) -> symtab_section)
#define elf_dynsymtab(bfd)	(elf_tdata(bfd) -> dynsymtab_section)
#define elf_num_locals(bfd)	(elf_tdata(bfd) -> num_locals)
#define elf_num_globals(bfd)	(elf_tdata(bfd) -> num_globals)
#define elf_sym_extra(bfd)	(elf_tdata(bfd) -> sym_extra)
#define elf_section_syms(bfd)	(elf_tdata(bfd) -> section_syms)
#define elf_num_section_syms(bfd) (elf_tdata(bfd) -> num_section_syms)
#define core_prpsinfo(bfd)	(elf_tdata(bfd) -> prpsinfo)
#define core_prstatus(bfd)	(elf_tdata(bfd) -> prstatus)
#define elf_gp(bfd)		(elf_tdata(bfd) -> gp)
#define elf_gp_size(bfd)	(elf_tdata(bfd) -> gp_size)
#define elf_sym_hashes(bfd)	(elf_tdata(bfd) -> sym_hashes)
#define elf_dt_needed_name(bfd)	(elf_tdata(bfd) -> dt_needed_name)
#define elf_bad_symtab(bfd)	(elf_tdata(bfd) -> bad_symtab)

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
							    bfd *,
							    char **));
extern boolean bfd_elf_mkobject PARAMS ((bfd *));
extern Elf_Internal_Shdr *bfd_elf_find_section PARAMS ((bfd *, char *));
extern boolean _bfd_elf_make_section_from_shdr
  PARAMS ((bfd *abfd, Elf_Internal_Shdr *hdr, const char *name));
extern struct bfd_hash_entry *_bfd_elf_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
extern struct bfd_link_hash_table *_bfd_elf_link_hash_table_create
  PARAMS ((bfd *));
extern boolean _bfd_elf_link_hash_table_init
  PARAMS ((struct elf_link_hash_table *, bfd *,
	   struct bfd_hash_entry *(*) (struct bfd_hash_entry *,
				       struct bfd_hash_table *,
				       const char *)));

extern boolean bfd_elf32_write_object_contents PARAMS ((bfd *));
extern boolean bfd_elf64_write_object_contents PARAMS ((bfd *));

extern const bfd_target *bfd_elf32_object_p PARAMS ((bfd *));
extern const bfd_target *bfd_elf32_core_file_p PARAMS ((bfd *));
extern char *bfd_elf32_core_file_failing_command PARAMS ((bfd *));
extern int bfd_elf32_core_file_failing_signal PARAMS ((bfd *));
extern boolean bfd_elf32_core_file_matches_executable_p PARAMS ((bfd *,
								 bfd *));
extern boolean bfd_elf32_set_section_contents PARAMS ((bfd *, sec_ptr, PTR,
						       file_ptr,
						       bfd_size_type));

extern long bfd_elf32_get_symtab_upper_bound PARAMS ((bfd *));
extern long bfd_elf32_get_symtab PARAMS ((bfd *, asymbol **));
extern long bfd_elf32_get_dynamic_symtab_upper_bound PARAMS ((bfd *));
extern long bfd_elf32_canonicalize_dynamic_symtab PARAMS ((bfd *, asymbol **));
extern long bfd_elf32_get_reloc_upper_bound PARAMS ((bfd *, sec_ptr));
extern long bfd_elf32_canonicalize_reloc PARAMS ((bfd *, sec_ptr,
						  arelent **, asymbol **));
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
extern boolean bfd_elf32_bfd_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_elf32_bfd_final_link
  PARAMS ((bfd *, struct bfd_link_info *));

extern void bfd_elf32_swap_symbol_in
  PARAMS ((bfd *, Elf32_External_Sym *, Elf_Internal_Sym *));
extern void bfd_elf32_swap_symbol_out
  PARAMS ((bfd *, Elf_Internal_Sym *, Elf32_External_Sym *));
extern void bfd_elf32_swap_reloc_in
  PARAMS ((bfd *, Elf32_External_Rel *, Elf_Internal_Rel *));
extern void bfd_elf32_swap_reloc_out
  PARAMS ((bfd *, Elf_Internal_Rel *, Elf32_External_Rel *));
extern void bfd_elf32_swap_reloca_in
  PARAMS ((bfd *, Elf32_External_Rela *, Elf_Internal_Rela *));
extern void bfd_elf32_swap_reloca_out
  PARAMS ((bfd *, Elf_Internal_Rela *, Elf32_External_Rela *));
extern void bfd_elf32_swap_dyn_in
  PARAMS ((bfd *, const Elf32_External_Dyn *, Elf_Internal_Dyn *));
extern void bfd_elf32_swap_dyn_out
  PARAMS ((bfd *, const Elf_Internal_Dyn *, Elf32_External_Dyn *));
extern boolean bfd_elf32_add_dynamic_entry
  PARAMS ((struct bfd_link_info *, bfd_vma, bfd_vma));

/* If the target doesn't have reloc handling written yet:  */
extern void bfd_elf32_no_info_to_howto PARAMS ((bfd *, arelent *,
						Elf32_Internal_Rela *));

extern const bfd_target *bfd_elf64_object_p PARAMS ((bfd *));
extern const bfd_target *bfd_elf64_core_file_p PARAMS ((bfd *));
extern char *bfd_elf64_core_file_failing_command PARAMS ((bfd *));
extern int bfd_elf64_core_file_failing_signal PARAMS ((bfd *));
extern boolean bfd_elf64_core_file_matches_executable_p PARAMS ((bfd *,
								 bfd *));
extern boolean bfd_elf64_set_section_contents PARAMS ((bfd *, sec_ptr, PTR,
						       file_ptr,
						       bfd_size_type));

extern long bfd_elf64_get_symtab_upper_bound PARAMS ((bfd *));
extern long bfd_elf64_get_symtab PARAMS ((bfd *, asymbol **));
extern long bfd_elf64_get_dynamic_symtab_upper_bound PARAMS ((bfd *));
extern long bfd_elf64_canonicalize_dynamic_symtab PARAMS ((bfd *, asymbol **));
extern long bfd_elf64_get_reloc_upper_bound PARAMS ((bfd *, sec_ptr));
extern long bfd_elf64_canonicalize_reloc PARAMS ((bfd *, sec_ptr,
						  arelent **, asymbol **));
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
extern boolean bfd_elf64_bfd_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_elf64_bfd_final_link
  PARAMS ((bfd *, struct bfd_link_info *));

extern void bfd_elf64_swap_symbol_in
  PARAMS ((bfd *, Elf64_External_Sym *, Elf_Internal_Sym *));
extern void bfd_elf64_swap_symbol_out
  PARAMS ((bfd *, Elf_Internal_Sym *, Elf64_External_Sym *));
extern void bfd_elf64_swap_reloc_in
  PARAMS ((bfd *, Elf64_External_Rel *, Elf_Internal_Rel *));
extern void bfd_elf64_swap_reloc_out
  PARAMS ((bfd *, Elf_Internal_Rel *, Elf64_External_Rel *));
extern void bfd_elf64_swap_reloca_in
  PARAMS ((bfd *, Elf64_External_Rela *, Elf_Internal_Rela *));
extern void bfd_elf64_swap_reloca_out
  PARAMS ((bfd *, Elf_Internal_Rela *, Elf64_External_Rela *));
extern void bfd_elf64_swap_dyn_in
  PARAMS ((bfd *, const Elf64_External_Dyn *, Elf_Internal_Dyn *));
extern void bfd_elf64_swap_dyn_out
  PARAMS ((bfd *, const Elf_Internal_Dyn *, Elf64_External_Dyn *));
extern boolean bfd_elf64_add_dynamic_entry
  PARAMS ((struct bfd_link_info *, bfd_vma, bfd_vma));

/* If the target doesn't have reloc handling written yet:  */
extern void bfd_elf64_no_info_to_howto PARAMS ((bfd *, arelent *,
						Elf64_Internal_Rela *));

#endif /* _LIBELF_H_ */
