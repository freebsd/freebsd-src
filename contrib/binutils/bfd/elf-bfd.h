/* BFD back-end data structures for ELF files.
   Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
   2002 Free Software Foundation, Inc.
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

#ifndef _LIBELF_H_
#define _LIBELF_H_ 1

#include "elf/common.h"
#include "elf/internal.h"
#include "elf/external.h"
#include "bfdlink.h"

/* The number of entries in a section is its size divided by the size
   of a single entry.  This is normally only applicable to reloc and
   symbol table sections.  */
#define NUM_SHDR_ENTRIES(shdr) ((shdr)->sh_size / (shdr)->sh_entsize)

/* If size isn't specified as 64 or 32, NAME macro should fail.  */
/* Do not "beautify" the CONCAT* macro args.  Traditional C will not
   remove whitespace added here, and thus will fail to concatenate
   the tokens.  */
#ifndef NAME
#if ARCH_SIZE==64
#define NAME(x,y) CONCAT4 (x,64,_,y)
#endif
#if ARCH_SIZE==32
#define NAME(x,y) CONCAT4 (x,32,_,y)
#endif
#endif

#ifndef NAME
#define NAME(x,y) CONCAT4 (x,NOSIZE,_,y)
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

  /* Version information.  This is from an Elf_Internal_Versym
     structure in a SHT_GNU_versym section.  It is zero if there is no
     version information.  */
  unsigned short version;

} elf_symbol_type;

struct elf_strtab_hash;

/* ELF linker hash table entries.  */

struct elf_link_hash_entry
{
  struct bfd_link_hash_entry root;

  /* Symbol index in output file.  This is initialized to -1.  It is
     set to -2 if the symbol is used by a reloc.  */
  long indx;

  /* Symbol index as a dynamic symbol.  Initialized to -1, and remains
     -1 if this is not a dynamic symbol.  */
  /* ??? Note that this is consistently used as a synonym for tests
     against whether we can perform various simplifying transformations
     to the code.  (E.g. changing a pc-relative jump to a PLT entry
     into a pc-relative jump to the target function.)  That test, which
     is often relatively complex, and someplaces wrong or incomplete,
     should really be replaced by a predicate in elflink.c.

     End result: this field -1 does not indicate that the symbol is
     not in the dynamic symbol table, but rather that the symbol is
     not visible outside this DSO.  */
  long dynindx;

  /* String table index in .dynstr if this is a dynamic symbol.  */
  unsigned long dynstr_index;

  /* Hash value of the name computed using the ELF hash function.  */
  unsigned long elf_hash_value;

  /* If this is a weak defined symbol from a dynamic object, this
     field points to a defined symbol with the same value, if there is
     one.  Otherwise it is NULL.  */
  struct elf_link_hash_entry *weakdef;

  /* If this symbol is used in the linker created sections, the processor
     specific backend uses this field to map the field into the offset
     from the beginning of the section.  */
  struct elf_linker_section_pointers *linker_section_pointer;

  /* Version information.  */
  union
  {
    /* This field is used for a symbol which is not defined in a
       regular object.  It points to the version information read in
       from the dynamic object.  */
    Elf_Internal_Verdef *verdef;
    /* This field is used for a symbol which is defined in a regular
       object.  It is set up in size_dynamic_sections.  It points to
       the version information we should write out for this symbol.  */
    struct bfd_elf_version_tree *vertree;
  } verinfo;

  /* Virtual table entry use information.  This array is nominally of size
     size/sizeof(target_void_pointer), though we have to be able to assume
     and track a size while the symbol is still undefined.  It is indexed
     via offset/sizeof(target_void_pointer).  */
  size_t vtable_entries_size;
  boolean *vtable_entries_used;

  /* Virtual table derivation info.  */
  struct elf_link_hash_entry *vtable_parent;

  /* If this symbol requires an entry in the global offset table, the
     processor specific backend uses this field to track usage and
     final offset.  We use a union and two names primarily to document
     the intent of any particular piece of code.  The field should be
     used as a count until size_dynamic_sections, at which point the
     contents of the .got is fixed.  Afterward, if this field is -1,
     then the symbol does not require a global offset table entry.  */
  union
    {
      bfd_signed_vma refcount;
      bfd_vma offset;
    } got;

  /* Same, but tracks a procedure linkage table entry.  */
  union
    {
      bfd_signed_vma refcount;
      bfd_vma offset;
    } plt;

  /* Symbol size.  */
  bfd_size_type size;

  /* Symbol type (STT_NOTYPE, STT_OBJECT, etc.).  */
  char type;

  /* Symbol st_other value, symbol visibility.  */
  unsigned char other;

  /* Some flags; legal values follow.  */
  unsigned short elf_link_hash_flags;
  /* Symbol is referenced by a non-shared object.  */
#define ELF_LINK_HASH_REF_REGULAR 01
  /* Symbol is defined by a non-shared object.  */
#define ELF_LINK_HASH_DEF_REGULAR 02
  /* Symbol is referenced by a shared object.  */
#define ELF_LINK_HASH_REF_DYNAMIC 04
  /* Symbol is defined by a shared object.  */
#define ELF_LINK_HASH_DEF_DYNAMIC 010
  /* Symbol has a non-weak reference from a non-shared object.  */
#define ELF_LINK_HASH_REF_REGULAR_NONWEAK 020
  /* Dynamic symbol has been adjustd.  */
#define ELF_LINK_HASH_DYNAMIC_ADJUSTED 040
  /* Symbol needs a copy reloc.  */
#define ELF_LINK_HASH_NEEDS_COPY 0100
  /* Symbol needs a procedure linkage table entry.  */
#define ELF_LINK_HASH_NEEDS_PLT 0200
  /* Symbol appears in a non-ELF input file.  */
#define ELF_LINK_NON_ELF 0400
  /* Symbol should be marked as hidden in the version information.  */
#define ELF_LINK_HIDDEN 01000
  /* Symbol was forced to local scope due to a version script file.  */
#define ELF_LINK_FORCED_LOCAL 02000
  /* Symbol was marked during garbage collection.  */
#define ELF_LINK_HASH_MARK 04000
  /* Symbol is referenced by a non-GOT/non-PLT relocation.  This is
     not currently set by all the backends.  */
#define ELF_LINK_NON_GOT_REF 010000
};

/* Records local symbols to be emitted in the dynamic symbol table.  */

struct elf_link_local_dynamic_entry
{
  struct elf_link_local_dynamic_entry *next;

  /* The input bfd this symbol came from.  */
  bfd *input_bfd;

  /* The index of the local symbol being copied.  */
  long input_indx;

  /* The index in the outgoing dynamic symbol table.  */
  long dynindx;

  /* A copy of the input symbol.  */
  Elf_Internal_Sym isym;
};

enum elf_link_info_type
{
  ELF_INFO_TYPE_NONE,
  ELF_INFO_TYPE_STABS,
  ELF_INFO_TYPE_MERGE,
  ELF_INFO_TYPE_EH_FRAME,
  ELF_INFO_TYPE_EH_FRAME_HDR,
  ELF_INFO_TYPE_LAST
};

/* ELF linker hash table.  */

struct elf_link_hash_table
{
  struct bfd_link_hash_table root;

  /* Whether we have created the special dynamic sections required
     when linking against or generating a shared object.  */
  boolean dynamic_sections_created;

  /* The BFD used to hold special sections created by the linker.
     This will be the first BFD found which requires these sections to
     be created.  */
  bfd *dynobj;

  /* The value to use when initialising got.refcount/offset and
     plt.refcount/offset in an elf_link_hash_entry.  Set to zero when
     the values are refcounts.  Set to -1 in size_dynamic_sections
     when the values may be offsets.  */
  bfd_signed_vma init_refcount;

  /* The number of symbols found in the link which must be put into
     the .dynsym section.  */
  bfd_size_type dynsymcount;

  /* The string table of dynamic symbols, which becomes the .dynstr
     section.  */
  struct elf_strtab_hash *dynstr;

  /* The number of buckets in the hash table in the .hash section.
     This is based on the number of dynamic symbols.  */
  bfd_size_type bucketcount;

  /* A linked list of DT_NEEDED names found in dynamic objects
     included in the link.  */
  struct bfd_link_needed_list *needed;

  /* The _GLOBAL_OFFSET_TABLE_ symbol.  */
  struct elf_link_hash_entry *hgot;

  /* A pointer to information used to link stabs in sections.  */
  PTR stab_info;

  /* A pointer to information used to merge SEC_MERGE sections.  */
  PTR merge_info;

  /* A linked list of local symbols to be added to .dynsym.  */
  struct elf_link_local_dynamic_entry *dynlocal;

  /* A linked list of DT_RPATH/DT_RUNPATH names found in dynamic
     objects included in the link.  */
  struct bfd_link_needed_list *runpath;
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

/* Returns true if the hash table is a struct elf_link_hash_table.  */
#define is_elf_hash_table(p)					      	\
  ((p)->hash->type == bfd_link_elf_hash_table)

/* Used by bfd_section_from_r_symndx to cache a small number of local
   symbol to section mappings.  */
#define LOCAL_SYM_CACHE_SIZE 32
struct sym_sec_cache
{
  bfd *abfd;
  unsigned long indx[LOCAL_SYM_CACHE_SIZE];
  asection *sec[LOCAL_SYM_CACHE_SIZE];
};

/* Constant information held for an ELF backend.  */

struct elf_size_info {
  unsigned char sizeof_ehdr, sizeof_phdr, sizeof_shdr;
  unsigned char sizeof_rel, sizeof_rela, sizeof_sym, sizeof_dyn, sizeof_note;

  /* The size of entries in the .hash section.  */
  unsigned char sizeof_hash_entry;

  /* The number of internal relocations to allocate per external
     relocation entry.  */
  unsigned char int_rels_per_ext_rel;

  unsigned char arch_size, file_align;
  unsigned char elfclass, ev_current;
  int (*write_out_phdrs)
    PARAMS ((bfd *, const Elf_Internal_Phdr *, unsigned int));
  boolean (*write_shdrs_and_ehdr)
    PARAMS ((bfd *));
  void (*write_relocs)
    PARAMS ((bfd *, asection *, PTR));
  void (*swap_symbol_out)
    PARAMS ((bfd *, const Elf_Internal_Sym *, PTR, PTR));
  boolean (*slurp_reloc_table)
    PARAMS ((bfd *, asection *, asymbol **, boolean));
  long (*slurp_symbol_table)
    PARAMS ((bfd *, asymbol **, boolean));
  void (*swap_dyn_in)
    PARAMS ((bfd *, const PTR, Elf_Internal_Dyn *));
  void (*swap_dyn_out)
    PARAMS ((bfd *, const Elf_Internal_Dyn *, PTR));

  /* This function, if defined, is called to swap in a REL
     relocation.  If an external relocation corresponds to more than
     one internal relocation, then all relocations are swapped in at
     once.  */
  void (*swap_reloc_in)
    PARAMS ((bfd *, const bfd_byte *, Elf_Internal_Rel *));

  /* This function, if defined, is called to swap out a REL
     relocation.  */
  void (*swap_reloc_out)
    PARAMS ((bfd *, const Elf_Internal_Rel *, bfd_byte *));

  /* This function, if defined, is called to swap in a RELA
     relocation.  If an external relocation corresponds to more than
     one internal relocation, then all relocations are swapped in at
     once.  */
  void (*swap_reloca_in)
    PARAMS ((bfd *, const bfd_byte *, Elf_Internal_Rela *));

  /* This function, if defined, is called to swap out a RELA
     relocation.  */
  void (*swap_reloca_out)
    PARAMS ((bfd *, const Elf_Internal_Rela *, bfd_byte *));

};

#define elf_symbol_from(ABFD,S) \
	(((S)->the_bfd->xvec->flavour == bfd_target_elf_flavour \
	  && (S)->the_bfd->tdata.elf_obj_data != 0) \
	 ? (elf_symbol_type *) (S) \
	 : 0)

enum elf_reloc_type_class {
  reloc_class_normal,
  reloc_class_relative,
  reloc_class_plt,
  reloc_class_copy
};

struct elf_reloc_cookie
{
  Elf_Internal_Rela *rels, *rel, *relend;
  PTR locsyms;
  PTR locsym_shndx;
  bfd *abfd;
  size_t locsymcount;
  size_t extsymoff;
  struct elf_link_hash_entry **sym_hashes;
  boolean bad_symtab;
};

struct elf_backend_data
{
  /* The architecture for this backend.  */
  enum bfd_architecture arch;

  /* The ELF machine code (EM_xxxx) for this backend.  */
  int elf_machine_code;

  /* The maximum page size for this backend.  */
  bfd_vma maxpagesize;

  /* A function to translate an ELF RELA relocation to a BFD arelent
     structure.  */
  void (*elf_info_to_howto)
    PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));

  /* A function to translate an ELF REL relocation to a BFD arelent
     structure.  */
  void (*elf_info_to_howto_rel)
    PARAMS ((bfd *, arelent *, Elf_Internal_Rel *));

  /* A function to determine whether a symbol is global when
     partitioning the symbol table into local and global symbols.
     This should be NULL for most targets, in which case the correct
     thing will be done.  MIPS ELF, at least on the Irix 5, has
     special requirements.  */
  boolean (*elf_backend_sym_is_global)
    PARAMS ((bfd *, asymbol *));

  /* The remaining functions are hooks which are called only if they
     are not NULL.  */

  /* A function to permit a backend specific check on whether a
     particular BFD format is relevant for an object file, and to
     permit the backend to set any global information it wishes.  When
     this is called elf_elfheader is set, but anything else should be
     used with caution.  If this returns false, the check_format
     routine will return a bfd_error_wrong_format error.  */
  boolean (*elf_backend_object_p)
    PARAMS ((bfd *));

  /* A function to do additional symbol processing when reading the
     ELF symbol table.  This is where any processor-specific special
     section indices are handled.  */
  void (*elf_backend_symbol_processing)
    PARAMS ((bfd *, asymbol *));

  /* A function to do additional symbol processing after reading the
     entire ELF symbol table.  */
  boolean (*elf_backend_symbol_table_processing)
    PARAMS ((bfd *, elf_symbol_type *, unsigned int));

   /* A function to set the type of the info field.  Processor-specific
     types should be handled here.  */
  int (*elf_backend_get_symbol_type)
    PARAMS (( Elf_Internal_Sym *, int));

  /* A function to do additional processing on the ELF section header
     just before writing it out.  This is used to set the flags and
     type fields for some sections, or to actually write out data for
     unusual sections.  */
  boolean (*elf_backend_section_processing)
    PARAMS ((bfd *, Elf32_Internal_Shdr *));

  /* A function to handle unusual section types when creating BFD
     sections from ELF sections.  */
  boolean (*elf_backend_section_from_shdr)
    PARAMS ((bfd *, Elf32_Internal_Shdr *, char *));

  /* A function to convert machine dependent section header flags to
     BFD internal section header flags.  */
  boolean (*elf_backend_section_flags)
    PARAMS ((flagword *, Elf32_Internal_Shdr *));

  /* A function to handle unusual program segment types when creating BFD
     sections from ELF program segments.  */
  boolean (*elf_backend_section_from_phdr)
    PARAMS ((bfd *, Elf32_Internal_Phdr *, int));

  /* A function to set up the ELF section header for a BFD section in
     preparation for writing it out.  This is where the flags and type
     fields are set for unusual sections.  */
  boolean (*elf_backend_fake_sections)
    PARAMS ((bfd *, Elf32_Internal_Shdr *, asection *));

  /* A function to get the ELF section index for a BFD section.  If
     this returns true, the section was found.  If it is a normal ELF
     section, *RETVAL should be left unchanged.  If it is not a normal
     ELF section *RETVAL should be set to the SHN_xxxx index.  */
  boolean (*elf_backend_section_from_bfd_section)
    PARAMS ((bfd *, asection *, int *retval));

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

  /* The CHECK_RELOCS function is called by the add_symbols phase of
     the ELF backend linker.  It is called once for each section with
     relocs of an object file, just after the symbols for the object
     file have been added to the global linker hash table.  The
     function must look through the relocs and do any special handling
     required.  This generally means allocating space in the global
     offset table, and perhaps allocating space for a reloc.  The
     relocs are always passed as Rela structures; if the section
     actually uses Rel structures, the r_addend field will always be
     zero.  */
  boolean (*check_relocs)
    PARAMS ((bfd *abfd, struct bfd_link_info *info, asection *o,
	     const Elf_Internal_Rela *relocs));

  /* The ADJUST_DYNAMIC_SYMBOL function is called by the ELF backend
     linker for every symbol which is defined by a dynamic object and
     referenced by a regular object.  This is called after all the
     input files have been seen, but before the SIZE_DYNAMIC_SECTIONS
     function has been called.  The hash table entry should be
     bfd_link_hash_defined ore bfd_link_hash_defweak, and it should be
     defined in a section from a dynamic object.  Dynamic object
     sections are not included in the final link, and this function is
     responsible for changing the value to something which the rest of
     the link can deal with.  This will normally involve adding an
     entry to the .plt or .got or some such section, and setting the
     symbol to point to that.  */
  boolean (*elf_backend_adjust_dynamic_symbol)
    PARAMS ((struct bfd_link_info *info, struct elf_link_hash_entry *h));

  /* The ALWAYS_SIZE_SECTIONS function is called by the backend linker
     after all the linker input files have been seen but before the
     section sizes have been set.  This is called after
     ADJUST_DYNAMIC_SYMBOL, but before SIZE_DYNAMIC_SECTIONS.  */
  boolean (*elf_backend_always_size_sections)
    PARAMS ((bfd *output_bfd, struct bfd_link_info *info));

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
	     asection **local_sections));

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
     before writing it out.  The LINKER argument is true if this BFD
     was created by the ELF backend linker.  */
  void (*elf_backend_final_write_processing)
    PARAMS ((bfd *, boolean linker));

  /* This function is called by get_program_header_size.  It should
     return the number of additional program segments which this BFD
     will need.  It should return -1 on error.  */
  int (*elf_backend_additional_program_headers)
    PARAMS ((bfd *));

  /* This function is called to modify an existing segment map in a
     backend specific fashion.  */
  boolean (*elf_backend_modify_segment_map)
    PARAMS ((bfd *));

  /* This function is called during section gc to discover the section a
     particular relocation refers to.  It need not be defined for hosts
     that have no queer relocation types.  */
  asection * (*gc_mark_hook)
    PARAMS ((bfd *abfd, struct bfd_link_info *, Elf_Internal_Rela *,
	     struct elf_link_hash_entry *h, Elf_Internal_Sym *));

  /* This function, if defined, is called during the sweep phase of gc
     in order that a backend might update any data structures it might
     be maintaining.  */
  boolean (*gc_sweep_hook)
    PARAMS ((bfd *abfd, struct bfd_link_info *info, asection *o,
	     const Elf_Internal_Rela *relocs));

  /* This function, if defined, is called after the ELF headers have
     been created.  This allows for things like the OS and ABI versions
     to be changed.  */
  void (*elf_backend_post_process_headers)
    PARAMS ((bfd *, struct bfd_link_info *));

  /* This function, if defined, prints a symbol to file and returns the
     name of the symbol to be printed.  It should return NULL to fall
     back to default symbol printing.  */
  const char *(*elf_backend_print_symbol_all)
    PARAMS ((bfd *, PTR, asymbol *));

  /* This function, if defined, is called after all local symbols and
     global symbols converted to locals are emited into the symtab
     section.  It allows the backend to emit special global symbols
     not handled in the hash table.  */
  boolean (*elf_backend_output_arch_syms)
    PARAMS ((bfd *, struct bfd_link_info *, PTR,
	    boolean (*) (PTR, const char *, Elf_Internal_Sym *, asection *)));

  /* Copy any information related to dynamic linking from a pre-existing 
     symbol to a newly created symbol.  Also called to copy flags and
     other back-end info to a weakdef, in which case the symbol is not
     newly created and plt/got refcounts and dynamic indices should not
     be copied.  */
  void (*elf_backend_copy_indirect_symbol)
    PARAMS ((struct elf_link_hash_entry *, struct elf_link_hash_entry *));

  /* Modify any information related to dynamic linking such that the
     symbol is not exported.  */
  void (*elf_backend_hide_symbol)
    PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *, boolean));

  /* Emit relocations.  Overrides default routine for emitting relocs,
     except during a relocatable link, or if all relocs are being emitted.  */
  void (*elf_backend_emit_relocs)
    PARAMS ((bfd *, asection *, Elf_Internal_Shdr *, Elf_Internal_Rela *));

  /* Count relocations.  Not called for relocatable links
     or if all relocs are being preserved in the output.  */
  unsigned int (*elf_backend_count_relocs)
    PARAMS ((asection *, Elf_Internal_Rela *));

  /* This function, if defined, is called when an NT_PRSTATUS note is found
     in a core file. */
  boolean (*elf_backend_grok_prstatus)
    PARAMS ((bfd *, Elf_Internal_Note *));

  /* This function, if defined, is called when an NT_PSINFO or NT_PRPSINFO
     note is found in a core file. */
  boolean (*elf_backend_grok_psinfo)
    PARAMS ((bfd *, Elf_Internal_Note *));

  /* Functions to print VMAs.  Special code to handle 64 bit ELF files.  */
  void (* elf_backend_sprintf_vma)
    PARAMS ((bfd *, char *, bfd_vma));
  void (* elf_backend_fprintf_vma)
    PARAMS ((bfd *, PTR, bfd_vma));

  /* This function returns class of a reloc type.  */
  enum elf_reloc_type_class (*elf_backend_reloc_type_class)
    PARAMS ((const Elf_Internal_Rela *));

  /* This function, if defined, removes information about discarded functions
     from other sections which mention them.  */
  boolean (*elf_backend_discard_info)
    PARAMS ((bfd *, struct elf_reloc_cookie *, struct bfd_link_info *));

  /* This function, if defined, signals that the function above has removed
     the discarded relocations for this section.  */
  boolean (*elf_backend_ignore_discarded_relocs)
    PARAMS ((asection *));

  /* This function, if defined, may write out the given section.
     Returns true if it did so and false if the caller should.  */
  boolean (*elf_backend_write_section)
    PARAMS ((bfd *, asection *, bfd_byte *));

  /* The swapping table to use when dealing with ECOFF information.
     Used for the MIPS ELF .mdebug section.  */
  const struct ecoff_debug_swap *elf_backend_ecoff_debug_swap;

  /* Alternate EM_xxxx machine codes for this backend.  */
  int elf_machine_alt1;
  int elf_machine_alt2;

  const struct elf_size_info *s;

  /* offset of the _GLOBAL_OFFSET_TABLE_ symbol from the start of the
     .got section */
  bfd_vma got_symbol_offset;

  /* The size in bytes of the headers for the GOT and PLT.  This includes
     the so-called reserved entries on some systems.  */
  bfd_vma got_header_size;
  bfd_vma plt_header_size;

  /* This is true if the linker should act like collect and gather
     global constructors and destructors by name.  This is true for
     MIPS ELF because the Irix 5 tools can not handle the .init
     section.  */
  unsigned collect : 1;

  /* This is true if the linker should ignore changes to the type of a
     symbol.  This is true for MIPS ELF because some Irix 5 objects
     record undefined functions as STT_OBJECT although the definitions
     are STT_FUNC.  */
  unsigned type_change_ok : 1;

  /* Whether the backend may use REL relocations.  (Some backends use
     both REL and RELA relocations, and this flag is set for those
     backends.)  */
  unsigned may_use_rel_p : 1;

  /* Whether the backend may use RELA relocations.  (Some backends use
     both REL and RELA relocations, and this flag is set for those
     backends.)  */
  unsigned may_use_rela_p : 1;

  /* Whether the default relocation type is RELA.  If a backend with
     this flag set wants REL relocations for a particular section,
     it must note that explicitly.  Similarly, if this flag is clear,
     and the backend wants RELA relocations for a particular
     section.  */
  unsigned default_use_rela_p : 1;

  /* True if addresses "naturally" sign extend.  This is used when
     swapping in from Elf32 when BFD64.  */
  unsigned sign_extend_vma : 1;

  unsigned want_got_plt : 1;
  unsigned plt_readonly : 1;
  unsigned want_plt_sym : 1;
  unsigned plt_not_loaded : 1;
  unsigned plt_alignment : 4;
  unsigned can_gc_sections : 1;
  unsigned can_refcount : 1;
  unsigned want_got_sym : 1;
  unsigned want_dynbss : 1;
};

/* Information stored for each BFD section in an ELF file.  This
   structure is allocated by elf_new_section_hook.  */

struct bfd_elf_section_data
{
  /* The ELF header for this section.  */
  Elf_Internal_Shdr this_hdr;

  /* The ELF header for the reloc section associated with this
     section, if any.  */
  Elf_Internal_Shdr rel_hdr;

  /* If there is a second reloc section associated with this section,
     as can happen on Irix 6, this field points to the header.  */
  Elf_Internal_Shdr *rel_hdr2;

  /* The number of relocations currently assigned to REL_HDR.  */
  unsigned int rel_count;

  /* The number of relocations currently assigned to REL_HDR2.  */
  unsigned int rel_count2;

  /* A pointer to a linked list tracking dynamic relocs copied for
     local symbols.  */
  PTR local_dynrel;

  /* A pointer to the bfd section used for dynamic relocs.  */
  asection *sreloc;

  /* The ELF section number of this section.  Only used for an output
     file.  */
  int this_idx;

  /* The ELF section number of the reloc section indicated by
     REL_HDR if any.  Only used for an output file.  */
  int rel_idx;

  /* The ELF section number of the reloc section indicated by
     REL_HDR2 if any.  Only used for an output file.  */
  int rel_idx2;

  /* Used by the backend linker to store the symbol hash table entries
     associated with relocs against global symbols.  */
  struct elf_link_hash_entry **rel_hashes;

  /* A pointer to the swapped relocs.  If the section uses REL relocs,
     rather than RELA, all the r_addend fields will be zero.  This
     pointer may be NULL.  It is used by the backend linker.  */
  Elf_Internal_Rela *relocs;

  /* Used by the backend linker when generating a shared library to
     record the dynamic symbol index for a section symbol
     corresponding to this section.  A value of 0 means that there is
     no dynamic symbol for this section.  */
  long dynindx;

  /* A pointer used for various section optimizations.  */
  PTR sec_info;

  /* Type of that information.  */
  enum elf_link_info_type sec_info_type;

  /* Group name, if this section is part of a group.  */
  const char *group_name;

  /* A linked list of sections in the group.  Circular when used by
     the linker.  */
  asection *next_in_group;

  /* A pointer available for the processor specific ELF backend.  */
  PTR tdata;

  /* Nonzero if this section uses RELA relocations, rather than REL.  */
  unsigned int use_rela_p:1;
};

#define elf_section_data(sec)  ((struct bfd_elf_section_data*)sec->used_by_bfd)
#define elf_group_name(sec)    (elf_section_data(sec)->group_name)
#define elf_next_in_group(sec) (elf_section_data(sec)->next_in_group)

/* Return true if section has been discarded.  */
#define elf_discarded_section(sec)					\
  (!bfd_is_abs_section(sec)						\
   && bfd_is_abs_section((sec)->output_section)				\
   && elf_section_data (sec)->sec_info_type != ELF_INFO_TYPE_MERGE)

#define get_elf_backend_data(abfd) \
  ((struct elf_backend_data *) (abfd)->xvec->backend_data)

/* Enumeration to specify the special section.  */
typedef enum elf_linker_section_enum
{
  LINKER_SECTION_UNKNOWN,		/* not used */
  LINKER_SECTION_GOT,			/* .got section for global offset pointers */
  LINKER_SECTION_PLT,			/* .plt section for generated procedure stubs */
  LINKER_SECTION_SDATA,			/* .sdata/.sbss section for PowerPC */
  LINKER_SECTION_SDATA2,		/* .sdata2/.sbss2 section for PowerPC */
  LINKER_SECTION_MAX			/* # of linker sections */
} elf_linker_section_enum_t;

/* Sections created by the linker.  */

typedef struct elf_linker_section
{
  char *name;				/* name of the section */
  char *rel_name;			/* name of the associated .rel{,a}. section */
  char *bss_name;			/* name of a related .bss section */
  char *sym_name;			/* name of symbol to reference this section */
  asection *section;			/* pointer to the section */
  asection *bss_section;		/* pointer to the bss section associated with this */
  asection *rel_section;		/* pointer to the relocations needed for this section */
  struct elf_link_hash_entry *sym_hash;	/* pointer to the created symbol hash value */
  bfd_vma initial_size;			/* initial size before any linker generated allocations */
  bfd_vma sym_offset;			/* offset of symbol from beginning of section */
  bfd_vma hole_size;			/* size of reserved address hole in allocation */
  bfd_vma hole_offset;			/* current offset for the hole */
  bfd_vma max_hole_offset;		/* maximum offset for the hole */
  elf_linker_section_enum_t which;	/* which section this is */
  boolean hole_written_p;		/* whether the hole has been initialized */
  unsigned int alignment;		/* alignment for the section */
  flagword flags;			/* flags to use to create the section */
} elf_linker_section_t;

/* Linked list of allocated pointer entries.  This hangs off of the symbol lists, and
   provides allows us to return different pointers, based on different addend's.  */

typedef struct elf_linker_section_pointers
{
  struct elf_linker_section_pointers *next;	/* next allocated pointer for this symbol */
  bfd_vma offset;				/* offset of pointer from beginning of section */
  bfd_vma addend;				/* addend used */
  elf_linker_section_enum_t which;		/* which linker section this is */
  boolean written_address_p;			/* whether address was written yet */
} elf_linker_section_pointers_t;

/* Some private data is stashed away for future use using the tdata pointer
   in the bfd structure.  */

struct elf_obj_tdata
{
  Elf_Internal_Ehdr elf_header[1];	/* Actual data, but ref like ptr */
  Elf_Internal_Shdr **elf_sect_ptr;
  Elf_Internal_Phdr *phdr;
  struct elf_segment_map *segment_map;
  struct elf_strtab_hash *strtab_ptr;
  int num_locals;
  int num_globals;
  unsigned int num_elf_sections;	/* elf_sect_ptr size */
  int num_section_syms;
  asymbol **section_syms;		/* STT_SECTION symbols for each section */
  Elf_Internal_Shdr symtab_hdr;
  Elf_Internal_Shdr shstrtab_hdr;
  Elf_Internal_Shdr strtab_hdr;
  Elf_Internal_Shdr dynsymtab_hdr;
  Elf_Internal_Shdr dynstrtab_hdr;
  Elf_Internal_Shdr dynversym_hdr;
  Elf_Internal_Shdr dynverref_hdr;
  Elf_Internal_Shdr dynverdef_hdr;
  Elf_Internal_Shdr symtab_shndx_hdr;
  unsigned int symtab_section, shstrtab_section;
  unsigned int strtab_section, dynsymtab_section;
  unsigned int symtab_shndx_section;
  unsigned int dynversym_section, dynverdef_section, dynverref_section;
  file_ptr next_file_pos;
#if 0
  /* we don't need these inside bfd anymore, and I think
     these weren't used outside bfd.  */
  void *prstatus;			/* The raw /proc prstatus structure */
  void *prpsinfo;			/* The raw /proc prpsinfo structure */
#endif
  bfd_vma gp;				/* The gp value */
  unsigned int gp_size;			/* The gp size */

  Elf_Internal_Shdr **group_sect_ptr;
  int num_group;

  /* Information grabbed from an elf core file.  */
  int core_signal;
  int core_pid;
  int core_lwpid;
  char* core_program;
  char* core_command;

  /* This is set to true if the object was created by the backend
     linker.  */
  boolean linker;

  /* A mapping from external symbols to entries in the linker hash
     table, used when linking.  This is indexed by the symbol index
     minus the sh_info field of the symbol table header.  */
  struct elf_link_hash_entry **sym_hashes;

  /* A mapping from local symbols to offsets into the global offset
     table, used when linking.  This is indexed by the symbol index.
     Like for the globals, we use a union and two names primarily to
     document the intent of any particular piece of code.  The field
     should be used as a count until size_dynamic_sections, at which
     point the contents of the .got is fixed.  Afterward, if an entry
     is -1, then the symbol does not require a global offset table entry.  */
  union
    {
      bfd_signed_vma *refcounts;
      bfd_vma *offsets;
    } local_got;

  /* A mapping from local symbols to offsets into the various linker
     sections added.  This is index by the symbol index.  */
  elf_linker_section_pointers_t **linker_section_pointers;

  /* The linker ELF emulation code needs to let the backend ELF linker
     know what filename should be used for a dynamic object if the
     dynamic object is found using a search.  The emulation code then
     sometimes needs to know what name was actually used.  Until the
     file has been added to the linker symbol table, this field holds
     the name the linker wants.  After it has been added, it holds the
     name actually used, which will be the DT_SONAME entry if there is
     one.  */
  const char *dt_name;

  /* When a reference in a regular object is resolved by a shared
     object is loaded into via the DT_NEEDED entries by the linker
     ELF emulation code, we need to add the shared object to the
     DT_NEEDED list of the resulting binary to indicate the dependency
     as if the -l option is passed to the linker. This field holds the
     name of the loaded shared object.  */
  const char *dt_soname;

  /* Irix 5 often screws up the symbol table, sorting local symbols
     after global symbols.  This flag is set if the symbol table in
     this BFD appears to be screwed up.  If it is, we ignore the
     sh_info field in the symbol table header, and always read all the
     symbols.  */
  boolean bad_symtab;

  /* Records the result of `get_program_header_size'.  */
  bfd_size_type program_header_size;

  /* Used by find_nearest_line entry point.  */
  PTR line_info;

  /* Used by MIPS ELF find_nearest_line entry point.  The structure
     could be included directly in this one, but there's no point to
     wasting the memory just for the infrequently called
     find_nearest_line.  */
  struct mips_elf_find_line *find_line_info;

  /* A place to stash dwarf1 info for this bfd.  */
  struct dwarf1_debug *dwarf1_find_line_info;

  /* A place to stash dwarf2 info for this bfd.  */
  PTR dwarf2_find_line_info;

  /* An array of stub sections indexed by symbol number, used by the
     MIPS ELF linker.  FIXME: We should figure out some way to only
     include this field for a MIPS ELF target.  */
  asection **local_stubs;

  /* Used to determine if the e_flags field has been initialized */
  boolean flags_init;

  /* Used to determine if PT_GNU_EH_FRAME segment header should be
     created.  */
  boolean eh_frame_hdr;

  /* Number of symbol version definitions we are about to emit.  */
  unsigned int cverdefs;

  /* Number of symbol version references we are about to emit.  */
  unsigned int cverrefs;

  /* Symbol version definitions in external objects.  */
  Elf_Internal_Verdef *verdef;

  /* Symbol version references to external objects.  */
  Elf_Internal_Verneed *verref;

  /* Linker sections that we are interested in.  */
  struct elf_linker_section *linker_section[ (int)LINKER_SECTION_MAX ];

  /* The Irix 5 support uses two virtual sections, which represent
     text/data symbols defined in dynamic objects.  */
  asymbol *elf_data_symbol;
  asymbol *elf_text_symbol;
  asection *elf_data_section;
  asection *elf_text_section;
};

#define elf_tdata(bfd)		((bfd) -> tdata.elf_obj_data)
#define elf_elfheader(bfd)	(elf_tdata(bfd) -> elf_header)
#define elf_elfsections(bfd)	(elf_tdata(bfd) -> elf_sect_ptr)
#define elf_numsections(bfd)	(elf_tdata(bfd) -> num_elf_sections)
#define elf_shstrtab(bfd)	(elf_tdata(bfd) -> strtab_ptr)
#define elf_onesymtab(bfd)	(elf_tdata(bfd) -> symtab_section)
#define elf_symtab_shndx(bfd)	(elf_tdata(bfd) -> symtab_shndx_section)
#define elf_dynsymtab(bfd)	(elf_tdata(bfd) -> dynsymtab_section)
#define elf_dynversym(bfd)	(elf_tdata(bfd) -> dynversym_section)
#define elf_dynverdef(bfd)	(elf_tdata(bfd) -> dynverdef_section)
#define elf_dynverref(bfd)	(elf_tdata(bfd) -> dynverref_section)
#define elf_num_locals(bfd)	(elf_tdata(bfd) -> num_locals)
#define elf_num_globals(bfd)	(elf_tdata(bfd) -> num_globals)
#define elf_section_syms(bfd)	(elf_tdata(bfd) -> section_syms)
#define elf_num_section_syms(bfd) (elf_tdata(bfd) -> num_section_syms)
#define core_prpsinfo(bfd)	(elf_tdata(bfd) -> prpsinfo)
#define core_prstatus(bfd)	(elf_tdata(bfd) -> prstatus)
#define elf_gp(bfd)		(elf_tdata(bfd) -> gp)
#define elf_gp_size(bfd)	(elf_tdata(bfd) -> gp_size)
#define elf_sym_hashes(bfd)	(elf_tdata(bfd) -> sym_hashes)
#define elf_local_got_refcounts(bfd) (elf_tdata(bfd) -> local_got.refcounts)
#define elf_local_got_offsets(bfd) (elf_tdata(bfd) -> local_got.offsets)
#define elf_local_ptr_offsets(bfd) (elf_tdata(bfd) -> linker_section_pointers)
#define elf_dt_name(bfd)	(elf_tdata(bfd) -> dt_name)
#define elf_dt_soname(bfd)	(elf_tdata(bfd) -> dt_soname)
#define elf_bad_symtab(bfd)	(elf_tdata(bfd) -> bad_symtab)
#define elf_flags_init(bfd)	(elf_tdata(bfd) -> flags_init)
#define elf_linker_section(bfd,n) (elf_tdata(bfd) -> linker_section[(int)n])

extern void _bfd_elf_swap_verdef_in
  PARAMS ((bfd *, const Elf_External_Verdef *, Elf_Internal_Verdef *));
extern void _bfd_elf_swap_verdef_out
  PARAMS ((bfd *, const Elf_Internal_Verdef *, Elf_External_Verdef *));
extern void _bfd_elf_swap_verdaux_in
  PARAMS ((bfd *, const Elf_External_Verdaux *, Elf_Internal_Verdaux *));
extern void _bfd_elf_swap_verdaux_out
  PARAMS ((bfd *, const Elf_Internal_Verdaux *, Elf_External_Verdaux *));
extern void _bfd_elf_swap_verneed_in
  PARAMS ((bfd *, const Elf_External_Verneed *, Elf_Internal_Verneed *));
extern void _bfd_elf_swap_verneed_out
  PARAMS ((bfd *, const Elf_Internal_Verneed *, Elf_External_Verneed *));
extern void _bfd_elf_swap_vernaux_in
  PARAMS ((bfd *, const Elf_External_Vernaux *, Elf_Internal_Vernaux *));
extern void _bfd_elf_swap_vernaux_out
  PARAMS ((bfd *, const Elf_Internal_Vernaux *, Elf_External_Vernaux *));
extern void _bfd_elf_swap_versym_in
  PARAMS ((bfd *, const Elf_External_Versym *, Elf_Internal_Versym *));
extern void _bfd_elf_swap_versym_out
  PARAMS ((bfd *, const Elf_Internal_Versym *, Elf_External_Versym *));

extern int _bfd_elf_section_from_bfd_section
  PARAMS ((bfd *, asection *));
extern char *bfd_elf_string_from_elf_section
  PARAMS ((bfd *, unsigned, unsigned));
extern char *bfd_elf_get_str_section
  PARAMS ((bfd *, unsigned));

extern boolean _bfd_elf_copy_private_bfd_data
  PARAMS ((bfd *, bfd *));
extern boolean _bfd_elf_print_private_bfd_data
  PARAMS ((bfd *, PTR));
extern void bfd_elf_print_symbol
  PARAMS ((bfd *, PTR, asymbol *, bfd_print_symbol_type));

#define elf_string_from_elf_strtab(abfd, strindex) \
  bfd_elf_string_from_elf_section(abfd, elf_elfheader(abfd)->e_shstrndx, \
				  strindex)

#define bfd_elf32_print_symbol	bfd_elf_print_symbol
#define bfd_elf64_print_symbol	bfd_elf_print_symbol

extern void _bfd_elf_sprintf_vma
  PARAMS ((bfd *, char *, bfd_vma));
extern void _bfd_elf_fprintf_vma
  PARAMS ((bfd *, PTR, bfd_vma));

extern enum elf_reloc_type_class _bfd_elf_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));
extern bfd_vma _bfd_elf_rela_local_sym
  PARAMS ((bfd *, Elf_Internal_Sym *, asection *, Elf_Internal_Rela *));
extern bfd_vma _bfd_elf_rel_local_sym
  PARAMS ((bfd *, Elf_Internal_Sym *, asection **, bfd_vma));
extern bfd_vma _bfd_elf_section_offset
  PARAMS ((bfd *, struct bfd_link_info *, asection *, bfd_vma));

extern unsigned long bfd_elf_hash
  PARAMS ((const char *));

extern bfd_reloc_status_type bfd_elf_generic_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
extern boolean bfd_elf_mkobject
  PARAMS ((bfd *));
extern boolean bfd_elf_mkcorefile
  PARAMS ((bfd *));
extern Elf_Internal_Shdr *bfd_elf_find_section
  PARAMS ((bfd *, char *));
extern boolean _bfd_elf_make_section_from_shdr
  PARAMS ((bfd *, Elf_Internal_Shdr *, const char *));
extern boolean _bfd_elf_make_section_from_phdr
  PARAMS ((bfd *, Elf_Internal_Phdr *, int, const char *));
extern struct bfd_hash_entry *_bfd_elf_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
extern struct bfd_link_hash_table *_bfd_elf_link_hash_table_create
  PARAMS ((bfd *));
extern void _bfd_elf_link_hash_copy_indirect
  PARAMS ((struct elf_link_hash_entry *, struct elf_link_hash_entry *));
extern void _bfd_elf_link_hash_hide_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *, boolean));
extern boolean _bfd_elf_link_hash_table_init
  PARAMS ((struct elf_link_hash_table *, bfd *,
	   struct bfd_hash_entry *(*) (struct bfd_hash_entry *,
				       struct bfd_hash_table *,
				       const char *)));
extern boolean _bfd_elf_slurp_version_tables
  PARAMS ((bfd *));

extern boolean _bfd_elf_merge_sections
  PARAMS ((bfd *, struct bfd_link_info *));

extern boolean _bfd_elf_copy_private_symbol_data
  PARAMS ((bfd *, asymbol *, bfd *, asymbol *));
extern boolean _bfd_elf_copy_private_section_data
  PARAMS ((bfd *, asection *, bfd *, asection *));
extern boolean _bfd_elf_write_object_contents
  PARAMS ((bfd *));
extern boolean _bfd_elf_write_corefile_contents
  PARAMS ((bfd *));
extern boolean _bfd_elf_set_section_contents
  PARAMS ((bfd *, sec_ptr, PTR, file_ptr, bfd_size_type));
extern long _bfd_elf_get_symtab_upper_bound
  PARAMS ((bfd *));
extern long _bfd_elf_get_symtab
  PARAMS ((bfd *, asymbol **));
extern long _bfd_elf_get_dynamic_symtab_upper_bound
  PARAMS ((bfd *));
extern long _bfd_elf_canonicalize_dynamic_symtab
  PARAMS ((bfd *, asymbol **));
extern long _bfd_elf_get_reloc_upper_bound
  PARAMS ((bfd *, sec_ptr));
extern long _bfd_elf_canonicalize_reloc
  PARAMS ((bfd *, sec_ptr, arelent **, asymbol **));
extern long _bfd_elf_get_dynamic_reloc_upper_bound
  PARAMS ((bfd *));
extern long _bfd_elf_canonicalize_dynamic_reloc
  PARAMS ((bfd *, arelent **, asymbol **));
extern asymbol *_bfd_elf_make_empty_symbol
  PARAMS ((bfd *));
extern void _bfd_elf_get_symbol_info
  PARAMS ((bfd *, asymbol *, symbol_info *));
extern boolean _bfd_elf_is_local_label_name
  PARAMS ((bfd *, const char *));
extern alent *_bfd_elf_get_lineno
  PARAMS ((bfd *, asymbol *));
extern boolean _bfd_elf_set_arch_mach
  PARAMS ((bfd *, enum bfd_architecture, unsigned long));
extern boolean _bfd_elf_find_nearest_line
  PARAMS ((bfd *, asection *, asymbol **, bfd_vma, const char **,
	   const char **, unsigned int *));
#define _bfd_elf_read_minisymbols _bfd_generic_read_minisymbols
#define _bfd_elf_minisymbol_to_symbol _bfd_generic_minisymbol_to_symbol
extern int _bfd_elf_sizeof_headers
  PARAMS ((bfd *, boolean));
extern boolean _bfd_elf_new_section_hook
  PARAMS ((bfd *, asection *));
extern boolean _bfd_elf_init_reloc_shdr
  PARAMS ((bfd *, Elf_Internal_Shdr *, asection *, boolean));

/* If the target doesn't have reloc handling written yet:  */
extern void _bfd_elf_no_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));

extern boolean bfd_section_from_shdr
  PARAMS ((bfd *, unsigned int shindex));
extern boolean bfd_section_from_phdr
  PARAMS ((bfd *, Elf_Internal_Phdr *, int));

extern int _bfd_elf_symbol_from_bfd_symbol
  PARAMS ((bfd *, asymbol **));

extern asection *bfd_section_from_r_symndx
  PARAMS ((bfd *, struct sym_sec_cache *, asection *, unsigned long));
extern asection *bfd_section_from_elf_index
  PARAMS ((bfd *, unsigned int));
extern boolean _bfd_elf_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern struct bfd_strtab_hash *_bfd_elf_stringtab_init
  PARAMS ((void));

extern struct elf_strtab_hash * _bfd_elf_strtab_init
  PARAMS ((void));
extern void _bfd_elf_strtab_free
  PARAMS ((struct elf_strtab_hash *));
extern bfd_size_type _bfd_elf_strtab_add
  PARAMS ((struct elf_strtab_hash *, const char *, boolean));
extern void _bfd_elf_strtab_addref
  PARAMS ((struct elf_strtab_hash *, bfd_size_type));
extern void _bfd_elf_strtab_delref
  PARAMS ((struct elf_strtab_hash *, bfd_size_type));
extern void _bfd_elf_strtab_clear_all_refs
  PARAMS ((struct elf_strtab_hash *));
extern bfd_size_type _bfd_elf_strtab_size
  PARAMS ((struct elf_strtab_hash *));
extern bfd_size_type _bfd_elf_strtab_offset
  PARAMS ((struct elf_strtab_hash *, bfd_size_type));
extern boolean _bfd_elf_strtab_emit
  PARAMS ((bfd *, struct elf_strtab_hash *));
extern void _bfd_elf_strtab_finalize
  PARAMS ((struct elf_strtab_hash *));

extern boolean _bfd_elf_discard_section_eh_frame
  PARAMS ((bfd *, struct bfd_link_info *, asection *, asection *,
	   boolean (*) (bfd_vma, PTR), struct elf_reloc_cookie *));
extern boolean _bfd_elf_discard_section_eh_frame_hdr
  PARAMS ((bfd *, struct bfd_link_info *, asection *));
extern bfd_vma _bfd_elf_eh_frame_section_offset
  PARAMS ((bfd *, asection *, bfd_vma));
extern boolean _bfd_elf_write_section_eh_frame
  PARAMS ((bfd *, asection *, asection *, bfd_byte *));
extern boolean _bfd_elf_write_section_eh_frame_hdr
  PARAMS ((bfd *, asection *));
extern boolean _bfd_elf_maybe_strip_eh_frame_hdr
  PARAMS ((struct bfd_link_info *));

extern boolean _bfd_elf_link_record_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
extern long _bfd_elf_link_lookup_local_dynindx
  PARAMS ((struct bfd_link_info *, bfd *, long));
extern boolean _bfd_elf_compute_section_file_positions
  PARAMS ((bfd *, struct bfd_link_info *));
extern void _bfd_elf_assign_file_positions_for_relocs
  PARAMS ((bfd *));
extern file_ptr _bfd_elf_assign_file_position_for_section
  PARAMS ((Elf_Internal_Shdr *, file_ptr, boolean));

extern boolean _bfd_elf_validate_reloc
  PARAMS ((bfd *, arelent *));

extern boolean _bfd_elf_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_elf_create_got_section
  PARAMS ((bfd *, struct bfd_link_info *));
extern unsigned long _bfd_elf_link_renumber_dynsyms
  PARAMS ((bfd *, struct bfd_link_info *));

extern boolean _bfd_elfcore_make_pseudosection
  PARAMS ((bfd *, char *, size_t, ufile_ptr));
extern char *_bfd_elfcore_strndup
  PARAMS ((bfd *, char *, size_t));

extern elf_linker_section_t *_bfd_elf_create_linker_section
  PARAMS ((bfd *, struct bfd_link_info *, enum elf_linker_section_enum,
	   elf_linker_section_t *));

extern elf_linker_section_pointers_t *_bfd_elf_find_pointer_linker_section
  PARAMS ((elf_linker_section_pointers_t *, bfd_vma,
	   elf_linker_section_enum_t));

extern boolean bfd_elf32_create_pointer_linker_section
  PARAMS ((bfd *, struct bfd_link_info *, elf_linker_section_t *,
	   struct elf_link_hash_entry *, const Elf32_Internal_Rela *));

extern bfd_vma bfd_elf32_finish_pointer_linker_section
  PARAMS ((bfd *, bfd *, struct bfd_link_info *, elf_linker_section_t *,
	   struct elf_link_hash_entry *, bfd_vma,
	   const Elf32_Internal_Rela *, int));

extern boolean bfd_elf64_create_pointer_linker_section
  PARAMS ((bfd *, struct bfd_link_info *, elf_linker_section_t *,
	   struct elf_link_hash_entry *, const Elf64_Internal_Rela *));

extern bfd_vma bfd_elf64_finish_pointer_linker_section
  PARAMS ((bfd *, bfd *, struct bfd_link_info *, elf_linker_section_t *,
	   struct elf_link_hash_entry *, bfd_vma,
	   const Elf64_Internal_Rela *, int));

extern boolean _bfd_elf_make_linker_section_rela
  PARAMS ((bfd *, elf_linker_section_t *, int));

extern const bfd_target *bfd_elf32_object_p
  PARAMS ((bfd *));
extern const bfd_target *bfd_elf32_core_file_p
  PARAMS ((bfd *));
extern char *bfd_elf32_core_file_failing_command
  PARAMS ((bfd *));
extern int bfd_elf32_core_file_failing_signal
  PARAMS ((bfd *));
extern boolean bfd_elf32_core_file_matches_executable_p
  PARAMS ((bfd *, bfd *));

extern boolean bfd_elf32_bfd_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_elf32_bfd_final_link
  PARAMS ((bfd *, struct bfd_link_info *));

extern void bfd_elf32_swap_symbol_in
  PARAMS ((bfd *, const Elf32_External_Sym *, const Elf_External_Sym_Shndx *,
	   Elf_Internal_Sym *));
extern void bfd_elf32_swap_symbol_out
  PARAMS ((bfd *, const Elf_Internal_Sym *, PTR, PTR));
extern void bfd_elf32_swap_reloc_in
  PARAMS ((bfd *, const Elf32_External_Rel *, Elf_Internal_Rel *));
extern void bfd_elf32_swap_reloc_out
  PARAMS ((bfd *, const Elf_Internal_Rel *, Elf32_External_Rel *));
extern void bfd_elf32_swap_reloca_in
  PARAMS ((bfd *, const Elf32_External_Rela *, Elf_Internal_Rela *));
extern void bfd_elf32_swap_reloca_out
  PARAMS ((bfd *, const Elf_Internal_Rela *, Elf32_External_Rela *));
extern void bfd_elf32_swap_phdr_in
  PARAMS ((bfd *, const Elf32_External_Phdr *, Elf_Internal_Phdr *));
extern void bfd_elf32_swap_phdr_out
  PARAMS ((bfd *, const Elf_Internal_Phdr *, Elf32_External_Phdr *));
extern void bfd_elf32_swap_dyn_in
  PARAMS ((bfd *, const PTR, Elf_Internal_Dyn *));
extern void bfd_elf32_swap_dyn_out
  PARAMS ((bfd *, const Elf_Internal_Dyn *, PTR));
extern long bfd_elf32_slurp_symbol_table
  PARAMS ((bfd *, asymbol **, boolean));
extern boolean bfd_elf32_write_shdrs_and_ehdr
  PARAMS ((bfd *));
extern int bfd_elf32_write_out_phdrs
  PARAMS ((bfd *, const Elf_Internal_Phdr *, unsigned int));
extern void bfd_elf32_write_relocs
  PARAMS ((bfd *, asection *, PTR));
extern boolean bfd_elf32_slurp_reloc_table
  PARAMS ((bfd *, asection *, asymbol **, boolean));
extern boolean bfd_elf32_add_dynamic_entry
  PARAMS ((struct bfd_link_info *, bfd_vma, bfd_vma));
extern boolean bfd_elf32_link_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern Elf_Internal_Rela *_bfd_elf32_link_read_relocs
  PARAMS ((bfd *, asection *, PTR, Elf_Internal_Rela *, boolean));

extern const bfd_target *bfd_elf64_object_p
  PARAMS ((bfd *));
extern const bfd_target *bfd_elf64_core_file_p
  PARAMS ((bfd *));
extern char *bfd_elf64_core_file_failing_command
  PARAMS ((bfd *));
extern int bfd_elf64_core_file_failing_signal
  PARAMS ((bfd *));
extern boolean bfd_elf64_core_file_matches_executable_p
  PARAMS ((bfd *, bfd *));
extern boolean bfd_elf64_bfd_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_elf64_bfd_final_link
  PARAMS ((bfd *, struct bfd_link_info *));

extern void bfd_elf64_swap_symbol_in
  PARAMS ((bfd *, const Elf64_External_Sym *, const Elf_External_Sym_Shndx *,
	   Elf_Internal_Sym *));
extern void bfd_elf64_swap_symbol_out
  PARAMS ((bfd *, const Elf_Internal_Sym *, PTR, PTR));
extern void bfd_elf64_swap_reloc_in
  PARAMS ((bfd *, const Elf64_External_Rel *, Elf_Internal_Rel *));
extern void bfd_elf64_swap_reloc_out
  PARAMS ((bfd *, const Elf_Internal_Rel *, Elf64_External_Rel *));
extern void bfd_elf64_swap_reloca_in
  PARAMS ((bfd *, const Elf64_External_Rela *, Elf_Internal_Rela *));
extern void bfd_elf64_swap_reloca_out
  PARAMS ((bfd *, const Elf_Internal_Rela *, Elf64_External_Rela *));
extern void bfd_elf64_swap_phdr_in
  PARAMS ((bfd *, const Elf64_External_Phdr *, Elf_Internal_Phdr *));
extern void bfd_elf64_swap_phdr_out
  PARAMS ((bfd *, const Elf_Internal_Phdr *, Elf64_External_Phdr *));
extern void bfd_elf64_swap_dyn_in
  PARAMS ((bfd *, const PTR, Elf_Internal_Dyn *));
extern void bfd_elf64_swap_dyn_out
  PARAMS ((bfd *, const Elf_Internal_Dyn *, PTR));
extern long bfd_elf64_slurp_symbol_table
  PARAMS ((bfd *, asymbol **, boolean));
extern boolean bfd_elf64_write_shdrs_and_ehdr
  PARAMS ((bfd *));
extern int bfd_elf64_write_out_phdrs
  PARAMS ((bfd *, const Elf_Internal_Phdr *, unsigned int));
extern void bfd_elf64_write_relocs
  PARAMS ((bfd *, asection *, PTR));
extern boolean bfd_elf64_slurp_reloc_table
  PARAMS ((bfd *, asection *, asymbol **, boolean));
extern boolean bfd_elf64_add_dynamic_entry
  PARAMS ((struct bfd_link_info *, bfd_vma, bfd_vma));
extern boolean bfd_elf64_link_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern Elf_Internal_Rela *_bfd_elf64_link_read_relocs
  PARAMS ((bfd *, asection *, PTR, Elf_Internal_Rela *, boolean));

#define bfd_elf32_link_record_dynamic_symbol \
  _bfd_elf_link_record_dynamic_symbol
#define bfd_elf64_link_record_dynamic_symbol \
  _bfd_elf_link_record_dynamic_symbol

extern boolean _bfd_elf32_link_record_local_dynamic_symbol
  PARAMS ((struct bfd_link_info *, bfd *, long));
extern boolean _bfd_elf64_link_record_local_dynamic_symbol
  PARAMS ((struct bfd_link_info *, bfd *, long));

extern boolean _bfd_elf_close_and_cleanup
  PARAMS ((bfd *));
extern bfd_reloc_status_type _bfd_elf_rel_vtable_reloc_fn
  PARAMS ((bfd *, arelent *, struct symbol_cache_entry *, PTR,
           asection *, bfd *, char **));

extern boolean _bfd_elf32_gc_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_elf32_gc_common_finalize_got_offsets
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_elf32_gc_common_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_elf32_gc_record_vtinherit
  PARAMS ((bfd *, asection *, struct elf_link_hash_entry *, bfd_vma));
extern boolean _bfd_elf32_gc_record_vtentry
  PARAMS ((bfd *, asection *, struct elf_link_hash_entry *, bfd_vma));

extern boolean _bfd_elf64_gc_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_elf64_gc_common_finalize_got_offsets
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_elf64_gc_common_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_elf64_gc_record_vtinherit
  PARAMS ((bfd *, asection *, struct elf_link_hash_entry *, bfd_vma));
extern boolean _bfd_elf64_gc_record_vtentry
  PARAMS ((bfd *, asection *, struct elf_link_hash_entry *, bfd_vma));

extern boolean _bfd_elf32_reloc_symbol_deleted_p
  PARAMS ((bfd_vma, PTR));
extern boolean _bfd_elf64_reloc_symbol_deleted_p
  PARAMS ((bfd_vma, PTR));

/* Exported interface for writing elf corefile notes. */
extern char *elfcore_write_note 
  PARAMS ((bfd *, char *, int *, char *, int, void *, int));
extern char *elfcore_write_prpsinfo 
  PARAMS ((bfd *, char *, int *, char *, char *));
extern char *elfcore_write_prstatus 
  PARAMS ((bfd *, char *, int *, long, int, void *));
extern char * elfcore_write_pstatus 
  PARAMS ((bfd *, char *, int *, long, int, void *));
extern char *elfcore_write_prfpreg 
  PARAMS ((bfd *, char *, int *, void *, int));
extern char *elfcore_write_prxfpreg 
  PARAMS ((bfd *, char *, int *, void *, int));
extern char *elfcore_write_lwpstatus 
  PARAMS ((bfd*, char*, int*, long, int, void*));

/* MIPS ELF specific routines.  */

extern boolean _bfd_mips_elf_object_p
  PARAMS ((bfd *));
extern boolean _bfd_mips_elf_section_from_shdr
  PARAMS ((bfd *, Elf_Internal_Shdr *, char *));
extern boolean _bfd_mips_elf_fake_sections
  PARAMS ((bfd *, Elf_Internal_Shdr *, asection *));
extern boolean _bfd_mips_elf_section_from_bfd_section
  PARAMS ((bfd *, asection *, int *));
extern boolean _bfd_mips_elf_section_processing
  PARAMS ((bfd *, Elf_Internal_Shdr *));
extern void _bfd_mips_elf_symbol_processing
  PARAMS ((bfd *, asymbol *));
extern boolean _bfd_mips_elf_read_ecoff_info
  PARAMS ((bfd *, asection *, struct ecoff_debug_info *));
extern void _bfd_mips_elf_final_write_processing
  PARAMS ((bfd *, boolean));
extern bfd_reloc_status_type _bfd_mips_elf_hi16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
extern bfd_reloc_status_type _bfd_mips_elf_lo16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
extern bfd_reloc_status_type _bfd_mips_elf_gprel16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
extern bfd_reloc_status_type _bfd_mips_elf_got16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
extern bfd_reloc_status_type _bfd_mips_elf_gprel32_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
extern boolean _bfd_mips_elf_set_private_flags
  PARAMS ((bfd *, flagword));
extern boolean _bfd_mips_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
extern boolean _bfd_mips_elf_find_nearest_line
  PARAMS ((bfd *, asection *, asymbol **, bfd_vma, const char **,
	   const char **, unsigned int *));
extern boolean _bfd_mips_elf_set_section_contents
  PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type));
extern boolean _bfd_mips_elf_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_mips_elf_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));
extern boolean _bfd_mips_elf_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
extern boolean _bfd_mips_elf_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
extern boolean _bfd_mips_elf_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern asection * _bfd_mips_elf_gc_mark_hook
  PARAMS ((bfd *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
extern boolean _bfd_mips_elf_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
extern boolean _bfd_mips_elf_always_size_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_mips_elf_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_mips_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
extern struct bfd_link_hash_table *_bfd_mips_elf_link_hash_table_create
  PARAMS ((bfd *));
extern boolean _bfd_mips_elf_print_private_bfd_data
  PARAMS ((bfd *, PTR));
extern boolean _bfd_mips_elf_link_output_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const char *, Elf_Internal_Sym *,
	   asection *));
extern boolean _bfd_mips_elf_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
extern int _bfd_mips_elf_additional_program_headers
  PARAMS ((bfd *));
extern boolean _bfd_mips_elf_modify_segment_map
  PARAMS ((bfd *));
extern boolean _bfd_mips_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));

/* SH ELF specific routine.  */

extern boolean _sh_elf_set_mach_from_flags
  PARAMS ((bfd *));

#endif /* _LIBELF_H_ */
