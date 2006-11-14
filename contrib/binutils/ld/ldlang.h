/* ldlang.h - linker command language support
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002, 2003, 2004
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

#define DEFAULT_MEMORY_REGION   "*default*"

typedef enum
{
  lang_input_file_is_l_enum,
  lang_input_file_is_symbols_only_enum,
  lang_input_file_is_marker_enum,
  lang_input_file_is_fake_enum,
  lang_input_file_is_search_file_enum,
  lang_input_file_is_file_enum
} lang_input_file_enum_type;

struct _fill_type
{
  size_t size;
  unsigned char data[1];
};

typedef struct statement_list
{
  union lang_statement_union *head;
  union lang_statement_union **tail;
} lang_statement_list_type;

typedef struct memory_region_struct
{
  char *name;
  struct memory_region_struct *next;
  bfd_vma origin;
  bfd_size_type length;
  bfd_vma current;
  bfd_size_type old_length;
  flagword flags;
  flagword not_flags;
  bfd_boolean had_full_message;
} lang_memory_region_type;

typedef struct lang_statement_header_struct
{
  union lang_statement_union *next;
  enum statement_enum
  {
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

typedef struct
{
  lang_statement_header_type header;
  union etree_union *exp;
} lang_assignment_statement_type;

typedef struct lang_target_statement_struct
{
  lang_statement_header_type header;
  const char *target;
} lang_target_statement_type;

typedef struct lang_output_statement_struct
{
  lang_statement_header_type header;
  const char *name;
} lang_output_statement_type;

/* Section types specified in a linker script.  */

enum section_type
{
  normal_section,
  dsect_section,
  copy_section,
  noload_section,
  info_section,
  overlay_section
};

/* This structure holds a list of program headers describing
   segments in which this section should be placed.  */

typedef struct lang_output_section_phdr_list
{
  struct lang_output_section_phdr_list *next;
  const char *name;
  bfd_boolean used;
} lang_output_section_phdr_list;

typedef struct lang_output_section_statement_struct
{
  lang_statement_header_type header;
  union etree_union *addr_tree;
  lang_statement_list_type children;
  const char *memspec;
  union lang_statement_union *next;
  const char *name;

  int processed;

  asection *bfd_section;
  flagword flags;		/* Or together of all input sections.  */
  enum section_type sectype;
  lang_memory_region_type *region;
  lang_memory_region_type *lma_region;
  size_t block_value;
  fill_type *fill;

  int subsection_alignment;	/* Alignment of components.  */
  int section_alignment;	/* Alignment of start of section.  */

  union etree_union *load_base;

  /* If non-null, an expression to evaluate after setting the section's
     size.  The expression is evaluated inside REGION (above) with '.'
     set to the end of the section.  Used in the last overlay section
     to move '.' past all the overlaid sections.  */
  union etree_union *update_dot_tree;

  lang_output_section_phdr_list *phdrs;
} lang_output_section_statement_type;

typedef struct
{
  lang_statement_header_type header;
} lang_common_statement_type;

typedef struct
{
  lang_statement_header_type header;
} lang_object_symbols_statement_type;

typedef struct
{
  lang_statement_header_type header;
  fill_type *fill;
  int size;
  asection *output_section;
} lang_fill_statement_type;

typedef struct
{
  lang_statement_header_type header;
  unsigned int type;
  union etree_union *exp;
  bfd_vma value;
  asection *output_section;
  bfd_vma output_vma;
} lang_data_statement_type;

/* Generate a reloc in the output file.  */

typedef struct
{
  lang_statement_header_type header;

  /* Reloc to generate.  */
  bfd_reloc_code_real_type reloc;

  /* Reloc howto structure.  */
  reloc_howto_type *howto;

  /* Section to generate reloc against.
     Exactly one of section and name must be NULL.  */
  asection *section;

  /* Name of symbol to generate reloc against.
     Exactly one of section and name must be NULL.  */
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

typedef struct lang_input_statement_struct
{
  lang_statement_header_type header;
  /* Name of this file.  */
  const char *filename;
  /* Name to use for the symbol giving address of text start.
     Usually the same as filename, but for a file spec'd with
     -l this is the -l switch itself rather than the filename.  */
  const char *local_sym_name;

  bfd *the_bfd;

  bfd_boolean closed;
  file_ptr passive_position;

  /* Symbol table of the file.  */
  asymbol **asymbols;
  unsigned int symbol_count;

  /* Point to the next file - whatever it is, wanders up and down
     archives */
  union lang_statement_union *next;

  /* Point to the next file, but skips archive contents.  */
  union lang_statement_union *next_real_file;

  bfd_boolean is_archive;

  /* 1 means search a set of directories for this file.  */
  bfd_boolean search_dirs_flag;

  /* 1 means this was found in a search directory marked as sysrooted,
     if search_dirs_flag is false, otherwise, that it should be
     searched in ld_sysroot before any other location, as long as it
     starts with a slash.  */
  bfd_boolean sysrooted;

  /* 1 means this is base file of incremental load.
     Do not load this file's text or data.
     Also default text_start to after this file's bss.  */
  bfd_boolean just_syms_flag;

  /* Whether to search for this entry as a dynamic archive.  */
  bfd_boolean dynamic;

  /* Whether this entry should cause a DT_NEEDED tag only when
     satisfying references from regular files, or always.  */
  bfd_boolean as_needed;

  /* Whether to include the entire contents of an archive.  */
  bfd_boolean whole_archive;

  bfd_boolean loaded;

#if 0
  unsigned int globals_in_this_file;
#endif
  const char *target;
  bfd_boolean real;
} lang_input_statement_type;

typedef struct
{
  lang_statement_header_type header;
  asection *section;
  lang_input_statement_type *ifile;

} lang_input_section_type;

typedef struct
{
  lang_statement_header_type header;
  asection *section;
  union lang_statement_union *file;
} lang_afile_asection_pair_statement_type;

typedef struct lang_wild_statement_struct
{
  lang_statement_header_type header;
  const char *filename;
  bfd_boolean filenames_sorted;
  struct wildcard_list *section_list;
  bfd_boolean keep_sections;
  lang_statement_list_type children;
} lang_wild_statement_type;

typedef struct lang_address_statement_struct
{
  lang_statement_header_type header;
  const char *section_name;
  union etree_union *address;
} lang_address_statement_type;

typedef struct
{
  lang_statement_header_type header;
  bfd_vma output_offset;
  size_t size;
  asection *output_section;
  fill_type *fill;
} lang_padding_statement_type;

/* A group statement collects a set of libraries together.  The
   libraries are searched multiple times, until no new undefined
   symbols are found.  The effect is to search a group of libraries as
   though they were a single library.  */

typedef struct
{
  lang_statement_header_type header;
  lang_statement_list_type children;
} lang_group_statement_type;

typedef union lang_statement_union
{
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

struct lang_phdr
{
  struct lang_phdr *next;
  const char *name;
  unsigned long type;
  bfd_boolean filehdr;
  bfd_boolean phdrs;
  etree_type *at;
  etree_type *flags;
};

/* This structure is used to hold a list of sections which may not
   cross reference each other.  */

typedef struct lang_nocrossref
{
  struct lang_nocrossref *next;
  const char *name;
} lang_nocrossref_type;

/* The list of nocrossref lists.  */

struct lang_nocrossrefs
{
  struct lang_nocrossrefs *next;
  lang_nocrossref_type *list;
};

extern struct lang_nocrossrefs *nocrossref_list;

/* This structure is used to hold a list of input section names which
   will not match an output section in the linker script.  */

struct unique_sections
{
  struct unique_sections *next;
  const char *name;
};

/* This structure records symbols for which we need to keep track of
   definedness for use in the DEFINED () test.  */

struct lang_definedness_hash_entry
{
  struct bfd_hash_entry root;
  int iteration;
};

extern struct unique_sections *unique_section_list;

extern lang_output_section_statement_type *abs_output_section;
extern lang_statement_list_type lang_output_section_statement;
extern bfd_boolean lang_has_input_file;
extern etree_type *base;
extern lang_statement_list_type *stat_ptr;
extern bfd_boolean delete_output_file_on_failure;

extern struct bfd_sym_chain entry_symbol;
extern const char *entry_section;
extern bfd_boolean entry_from_cmdline;
extern lang_statement_list_type file_chain;

extern int lang_statement_iteration;

extern void lang_init
  (void);
extern lang_memory_region_type *lang_memory_region_lookup
  (const char *const, bfd_boolean);
extern lang_memory_region_type *lang_memory_region_default
  (asection *);
extern void lang_map
  (void);
extern void lang_set_flags
  (lang_memory_region_type *, const char *, int);
extern void lang_add_output
  (const char *, int from_script);
extern lang_output_section_statement_type *lang_enter_output_section_statement
  (const char *output_section_statement_name,
   etree_type *address_exp,
   enum section_type sectype,
   etree_type *align,
   etree_type *subalign,
   etree_type *);
extern void lang_final
  (void);
extern void lang_process
  (void);
extern void lang_section_start
  (const char *, union etree_union *);
extern void lang_add_entry
  (const char *, bfd_boolean);
extern void lang_add_target
  (const char *);
extern void lang_add_wild
  (struct wildcard_spec *, struct wildcard_list *, bfd_boolean);
extern void lang_add_map
  (const char *);
extern void lang_add_fill
  (fill_type *);
extern lang_assignment_statement_type *lang_add_assignment
  (union etree_union *);
extern void lang_add_attribute
  (enum statement_enum);
extern void lang_startup
  (const char *);
extern void lang_float
  (bfd_boolean);
extern void lang_leave_output_section_statement
  (fill_type *, const char *, lang_output_section_phdr_list *,
   const char *);
extern void lang_abs_symbol_at_end_of
  (const char *, const char *);
extern void lang_abs_symbol_at_beginning_of
  (const char *, const char *);
extern void lang_statement_append
  (lang_statement_list_type *, lang_statement_union_type *,
   lang_statement_union_type **);
extern void lang_for_each_input_file
  (void (*dothis) (lang_input_statement_type *));
extern void lang_for_each_file
  (void (*dothis) (lang_input_statement_type *));
extern void lang_reset_memory_regions
  (void);
extern void lang_do_assignments
  (lang_statement_union_type *, lang_output_section_statement_type *,
   fill_type *, bfd_vma);

#define LANG_FOR_EACH_INPUT_STATEMENT(statement)			\
  lang_input_statement_type *statement;					\
  for (statement = (lang_input_statement_type *) file_chain.head;	\
       statement != (lang_input_statement_type *) NULL;			\
       statement = (lang_input_statement_type *) statement->next)	\

extern void lang_process
  (void);
extern void ldlang_add_file
  (lang_input_statement_type *);
extern lang_output_section_statement_type *lang_output_section_find
  (const char * const);
extern lang_input_statement_type *lang_add_input_file
  (const char *, lang_input_file_enum_type, const char *);
extern void lang_add_keepsyms_file
  (const char *);
extern lang_output_section_statement_type *
  lang_output_section_statement_lookup
  (const char *const);
extern void ldlang_add_undef
  (const char *const);
extern void lang_add_output_format
  (const char *, const char *, const char *, int);
extern void lang_list_init
  (lang_statement_list_type *);
extern void lang_add_data
  (int type, union etree_union *);
extern void lang_add_reloc
  (bfd_reloc_code_real_type, reloc_howto_type *, asection *, const char *,
   union etree_union *);
extern void lang_for_each_statement
  (void (*) (lang_statement_union_type *));
extern void *stat_alloc
  (size_t);
extern void dprint_statement
  (lang_statement_union_type *, int);
extern bfd_vma lang_size_sections
  (lang_statement_union_type *, lang_output_section_statement_type *,
   lang_statement_union_type **, fill_type *, bfd_vma, bfd_boolean *,
   bfd_boolean);
extern void lang_enter_group
  (void);
extern void lang_leave_group
  (void);
extern void lang_add_section
  (lang_statement_list_type *, asection *,
   lang_output_section_statement_type *, lang_input_statement_type *);
extern void lang_new_phdr
  (const char *, etree_type *, bfd_boolean, bfd_boolean, etree_type *,
   etree_type *);
extern void lang_add_nocrossref
  (lang_nocrossref_type *);
extern void lang_enter_overlay
  (etree_type *, etree_type *);
extern void lang_enter_overlay_section
  (const char *);
extern void lang_leave_overlay_section
  (fill_type *, lang_output_section_phdr_list *);
extern void lang_leave_overlay
  (etree_type *, int, fill_type *, const char *,
   lang_output_section_phdr_list *, const char *);

extern struct bfd_elf_version_tree *lang_elf_version_info;

extern struct bfd_elf_version_expr *lang_new_vers_pattern
  (struct bfd_elf_version_expr *, const char *, const char *);
extern struct bfd_elf_version_tree *lang_new_vers_node
  (struct bfd_elf_version_expr *, struct bfd_elf_version_expr *);
extern struct bfd_elf_version_deps *lang_add_vers_depend
  (struct bfd_elf_version_deps *, const char *);
extern void lang_register_vers_node
  (const char *, struct bfd_elf_version_tree *, struct bfd_elf_version_deps *);
bfd_boolean unique_section_p
  (const char *);
extern void lang_add_unique
  (const char *);
extern const char *lang_get_output_target
  (void);
extern void lang_track_definedness (const char *);
extern int lang_symbol_definition_iteration (const char *);
extern void lang_update_definedness
  (const char *, struct bfd_link_hash_entry *);

#endif
