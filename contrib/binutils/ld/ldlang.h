/* ldlang.h - linker command language support
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002
   Free Software Foundation, Inc.

   This file is part of GLD, the Gnu Linker.

   GLD is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GLD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GLD; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef LDLANG_H
#define LDLANG_H

typedef enum {
  lang_input_file_is_l_enum,
  lang_input_file_is_symbols_only_enum,
  lang_input_file_is_marker_enum,
  lang_input_file_is_fake_enum,
  lang_input_file_is_search_file_enum,
  lang_input_file_is_file_enum
} lang_input_file_enum_type;

typedef unsigned int fill_type;

typedef struct statement_list {
  union lang_statement_union *head;
  union lang_statement_union **tail;
} lang_statement_list_type;

typedef struct memory_region_struct {
  char *name;
  struct memory_region_struct *next;
  bfd_vma origin;
  bfd_size_type length;
  bfd_vma current;
  bfd_size_type old_length;
  flagword flags;
  flagword not_flags;
  boolean had_full_message;
} lang_memory_region_type;

typedef struct lang_statement_header_struct {
  union lang_statement_union *next;
  enum statement_enum {
    lang_output_section_statement_enum,
    lang_assignment_statement_enum,
    lang_input_statement_enum,
    lang_address_statement_enum,
    lang_wild_statement_enum,
    lang_input_section_enum,
    lang_object_symbols_statement_enum,
    lang_fill_statement_enum,
    lang_data_statement_enum,
    lang_reloc_statement_enum,
    lang_target_statement_enum,
    lang_output_statement_enum,
    lang_padding_statement_enum,
    lang_group_statement_enum,

    lang_afile_asection_pair_statement_enum,
    lang_constructors_statement_enum
  } type;
} lang_statement_header_type;

typedef struct {
  lang_statement_header_type header;
  union etree_union *exp;
} lang_assignment_statement_type;

typedef struct lang_target_statement_struct {
  lang_statement_header_type header;
  const char *target;
} lang_target_statement_type;

typedef struct lang_output_statement_struct {
  lang_statement_header_type header;
  const char *name;
} lang_output_statement_type;

/* Section types specified in a linker script.  */

enum section_type {
  normal_section,
  dsect_section,
  copy_section,
  noload_section,
  info_section,
  overlay_section
};

/* This structure holds a list of program headers describing segments
   in which this section should be placed.  */

struct lang_output_section_phdr_list {
  struct lang_output_section_phdr_list *next;
  const char *name;
  boolean used;
};

typedef struct lang_output_section_statement_struct {
  lang_statement_header_type header;
  union etree_union *addr_tree;
  lang_statement_list_type children;
  const char *memspec;
  union lang_statement_union *next;
  const char *name;

  boolean processed;

  asection *bfd_section;
  flagword flags;		/* Or together of all input sections */
  enum section_type sectype;
  struct memory_region_struct *region;
  struct memory_region_struct *lma_region;
  size_t block_value;
  fill_type fill;

  int subsection_alignment;	/* alignment of components */
  int section_alignment;	/* alignment of start of section */

  union etree_union *load_base;

  struct lang_output_section_phdr_list *phdrs;
} lang_output_section_statement_type;

typedef struct {
  lang_statement_header_type header;
} lang_common_statement_type;

typedef struct {
  lang_statement_header_type header;
} lang_object_symbols_statement_type;

typedef struct {
  lang_statement_header_type header;
  fill_type fill;
  int size;
  asection *output_section;
} lang_fill_statement_type;

typedef struct {
  lang_statement_header_type header;
  unsigned int type;
  union etree_union *exp;
  bfd_vma value;
  asection *output_section;
  bfd_vma output_vma;
} lang_data_statement_type;

/* Generate a reloc in the output file.  */

typedef struct {
  lang_statement_header_type header;

  /* Reloc to generate.  */
  bfd_reloc_code_real_type reloc;

  /* Reloc howto structure.  */
  reloc_howto_type *howto;

  /* Section to generate reloc against.  Exactly one of section and
     name must be NULL.  */
  asection *section;

  /* Name of symbol to generate reloc against.  Exactly one of section
     and name must be NULL.  */
  const char *name;

  /* Expression for addend.  */
  union etree_union *addend_exp;

  /* Resolved addend.  */
  bfd_vma addend_value;

  /* Output section where reloc should be performed.  */
  asection *output_section;

  /* VMA within output section.  */
  bfd_vma output_vma;
} lang_reloc_statement_type;

typedef struct lang_input_statement_struct {
  lang_statement_header_type header;
  /* Name of this file.  */
  const char *filename;
  /* Name to use for the symbol giving address of text start */
  /* Usually the same as filename, but for a file spec'd with -l
     this is the -l switch itself rather than the filename.  */
  const char *local_sym_name;

  bfd *the_bfd;

  boolean closed;
  file_ptr passive_position;

  /* Symbol table of the file.  */
  asymbol **asymbols;
  unsigned int symbol_count;

  /* Point to the next file - whatever it is, wanders up and down
     archives */

  union lang_statement_union *next;
  /* Point to the next file, but skips archive contents */
  union lang_statement_union *next_real_file;

  boolean is_archive;

  /* 1 means search a set of directories for this file.  */
  boolean search_dirs_flag;

  /* 1 means this is base file of incremental load.
     Do not load this file's text or data.
     Also default text_start to after this file's bss.  */

  boolean just_syms_flag;

  /* Whether to search for this entry as a dynamic archive.  */
  boolean dynamic;

  /* Whether to include the entire contents of an archive.  */
  boolean whole_archive;

  boolean loaded;

#if 0
  unsigned int globals_in_this_file;
#endif
  const char *target;
  boolean real;
} lang_input_statement_type;

typedef struct {
  lang_statement_header_type header;
  asection *section;
  lang_input_statement_type *ifile;

} lang_input_section_type;

typedef struct {
  lang_statement_header_type header;
  asection *section;
  union lang_statement_union *file;
} lang_afile_asection_pair_statement_type;

typedef struct lang_wild_statement_struct {
  lang_statement_header_type header;
  const char *filename;
  boolean filenames_sorted;
  struct wildcard_list *section_list;
  boolean keep_sections;
  lang_statement_list_type children;
} lang_wild_statement_type;

typedef struct lang_address_statement_struct {
  lang_statement_header_type header;
  const char *section_name;
  union etree_union *address;
} lang_address_statement_type;

typedef struct {
  lang_statement_header_type header;
  bfd_vma output_offset;
  size_t size;
  asection *output_section;
  fill_type fill;
} lang_padding_statement_type;

/* A group statement collects a set of libraries together.  The
   libraries are searched multiple times, until no new undefined
   symbols are found.  The effect is to search a group of libraries as
   though they were a single library.  */

typedef struct {
  lang_statement_header_type header;
  lang_statement_list_type children;
} lang_group_statement_type;

typedef union lang_statement_union {
  lang_statement_header_type header;
  lang_wild_statement_type wild_statement;
  lang_data_statement_type data_statement;
  lang_reloc_statement_type reloc_statement;
  lang_address_statement_type address_statement;
  lang_output_section_statement_type output_section_statement;
  lang_afile_asection_pair_statement_type afile_asection_pair_statement;
  lang_assignment_statement_type assignment_statement;
  lang_input_statement_type input_statement;
  lang_target_statement_type target_statement;
  lang_output_statement_type output_statement;
  lang_input_section_type input_section;
  lang_common_statement_type common_statement;
  lang_object_symbols_statement_type object_symbols_statement;
  lang_fill_statement_type fill_statement;
  lang_padding_statement_type padding_statement;
  lang_group_statement_type group_statement;
} lang_statement_union_type;

/* This structure holds information about a program header, from the
   PHDRS command in the linker script.  */

struct lang_phdr {
  struct lang_phdr *next;
  const char *name;
  unsigned long type;
  boolean filehdr;
  boolean phdrs;
  etree_type *at;
  etree_type *flags;
};

/* This structure is used to hold a list of sections which may not
   cross reference each other.  */

struct lang_nocrossref {
  struct lang_nocrossref *next;
  const char *name;
};

/* The list of nocrossref lists.  */

struct lang_nocrossrefs {
  struct lang_nocrossrefs *next;
  struct lang_nocrossref *list;
};

extern struct lang_nocrossrefs *nocrossref_list;

/* This structure is used to hold a list of input section names which
   will not match an output section in the linker script.  */

struct unique_sections {
  struct unique_sections *next;
  const char *name;
};

extern struct unique_sections *unique_section_list;

extern lang_output_section_statement_type *abs_output_section;
extern lang_statement_list_type lang_output_section_statement;
extern boolean lang_has_input_file;
extern etree_type *base;
extern lang_statement_list_type *stat_ptr;
extern boolean delete_output_file_on_failure;

extern const char *entry_symbol;
extern const char *entry_section;
extern boolean entry_from_cmdline;
extern lang_statement_list_type file_chain;

extern void lang_init PARAMS ((void));
extern struct memory_region_struct *lang_memory_region_lookup
  PARAMS ((const char *const));
extern struct memory_region_struct *lang_memory_region_default
  PARAMS ((asection *));
extern void lang_map PARAMS ((void));
extern void lang_set_flags PARAMS ((lang_memory_region_type *, const char *,
				    int));
extern void lang_add_output PARAMS ((const char *, int from_script));
extern lang_output_section_statement_type *lang_enter_output_section_statement
  PARAMS ((const char *output_section_statement_name,
	   etree_type * address_exp,
	   enum section_type sectype,
	   bfd_vma block_value,
	   etree_type *align,
	   etree_type *subalign,
	   etree_type *));
extern void lang_final PARAMS ((void));
extern void lang_process PARAMS ((void));
extern void lang_section_start PARAMS ((const char *, union etree_union *));
extern void lang_add_entry PARAMS ((const char *, boolean));
extern void lang_add_target PARAMS ((const char *));
extern void lang_add_wild
  PARAMS ((struct wildcard_spec *, struct wildcard_list *, boolean));
extern void lang_add_map PARAMS ((const char *));
extern void lang_add_fill PARAMS ((int));
extern lang_assignment_statement_type * lang_add_assignment PARAMS ((union etree_union *));
extern void lang_add_attribute PARAMS ((enum statement_enum));
extern void lang_startup PARAMS ((const char *));
extern void lang_float PARAMS ((enum bfd_boolean));
extern void lang_leave_output_section_statement
  PARAMS ((bfd_vma, const char *, struct lang_output_section_phdr_list *,
           const char *));
extern void lang_abs_symbol_at_end_of PARAMS ((const char *, const char *));
extern void lang_abs_symbol_at_beginning_of PARAMS ((const char *,
						     const char *));
extern void lang_statement_append PARAMS ((struct statement_list *,
					   union lang_statement_union *,
					   union lang_statement_union **));
extern void lang_for_each_input_file
  PARAMS ((void (*dothis) (lang_input_statement_type *)));
extern void lang_for_each_file
  PARAMS ((void (*dothis) (lang_input_statement_type *)));
extern void lang_reset_memory_regions PARAMS ((void));
extern bfd_vma lang_do_assignments
  PARAMS ((lang_statement_union_type * s,
	   lang_output_section_statement_type *output_section_statement,
	   fill_type fill,
	   bfd_vma dot));

#define LANG_FOR_EACH_INPUT_STATEMENT(statement)		\
  lang_input_statement_type *statement;				\
  for (statement = (lang_input_statement_type *)file_chain.head;\
       statement != (lang_input_statement_type *)NULL;		\
       statement = (lang_input_statement_type *)statement->next)\

extern void lang_process PARAMS ((void));
extern void ldlang_add_file PARAMS ((lang_input_statement_type *));
extern lang_output_section_statement_type *lang_output_section_find
  PARAMS ((const char * const));
extern lang_input_statement_type *lang_add_input_file
  PARAMS ((const char *name, lang_input_file_enum_type file_type,
	   const char *target));
extern void lang_add_keepsyms_file PARAMS ((const char *filename));
extern lang_output_section_statement_type *
  lang_output_section_statement_lookup PARAMS ((const char * const name));
extern void ldlang_add_undef PARAMS ((const char *const name));
extern void lang_add_output_format PARAMS ((const char *, const char *,
					    const char *, int from_script));
extern void lang_list_init PARAMS ((lang_statement_list_type*));
extern void lang_add_data PARAMS ((int type, union etree_union *));
extern void lang_add_reloc
  PARAMS ((bfd_reloc_code_real_type reloc, reloc_howto_type *howto,
	   asection *section, const char *name, union etree_union *addend));
extern void lang_for_each_statement
  PARAMS ((void (*func) (lang_statement_union_type *)));
extern PTR stat_alloc PARAMS ((size_t size));
extern void dprint_statement PARAMS ((lang_statement_union_type *, int));
extern bfd_vma lang_size_sections
  PARAMS ((lang_statement_union_type *s,
	   lang_output_section_statement_type *output_section_statement,
	   lang_statement_union_type **prev, fill_type fill,
	   bfd_vma dot, boolean *relax));
extern void lang_enter_group PARAMS ((void));
extern void lang_leave_group PARAMS ((void));
extern void lang_add_section
  PARAMS ((lang_statement_list_type *ptr, asection *section,
	   lang_output_section_statement_type *output,
	   lang_input_statement_type *file));
extern void lang_new_phdr
  PARAMS ((const char *, etree_type *, boolean, boolean, etree_type *,
	   etree_type *));
extern void lang_add_nocrossref PARAMS ((struct lang_nocrossref *));
extern void lang_enter_overlay PARAMS ((etree_type *, etree_type *, int));
extern void lang_enter_overlay_section PARAMS ((const char *));
extern void lang_leave_overlay_section
  PARAMS ((bfd_vma, struct lang_output_section_phdr_list *));
extern void lang_leave_overlay
  PARAMS ((bfd_vma, const char *, struct lang_output_section_phdr_list *,
           const char *));

extern struct bfd_elf_version_tree *lang_elf_version_info;

extern struct bfd_elf_version_expr *lang_new_vers_pattern
  PARAMS ((struct bfd_elf_version_expr *, const char *, const char *));
extern struct bfd_elf_version_tree *lang_new_vers_node
  PARAMS ((struct bfd_elf_version_expr *, struct bfd_elf_version_expr *));
extern struct bfd_elf_version_deps *lang_add_vers_depend
  PARAMS ((struct bfd_elf_version_deps *, const char *));
extern void lang_register_vers_node
  PARAMS ((const char *, struct bfd_elf_version_tree *,
	   struct bfd_elf_version_deps *));
boolean unique_section_p PARAMS ((const char *));
extern void lang_add_unique PARAMS ((const char *));

#endif
