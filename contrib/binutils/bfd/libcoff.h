/* BFD COFF object file private structure.
   Copyright (C) 1990, 91, 92, 93, 94, 95, 96, 97, 1998
   Free Software Foundation, Inc.
   Written by Cygnus Support.

** NOTE: libcoff.h is a GENERATED file.  Don't change it; instead,
** change libcoff-in.h or coffcode.h.

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

#include "bfdlink.h"

/* Object file tdata; access macros */

#define coff_data(bfd)		((bfd)->tdata.coff_obj_data)
#define exec_hdr(bfd)		(coff_data(bfd)->hdr)
#define obj_pe(bfd)             (coff_data(bfd)->pe)
#define obj_symbols(bfd)	(coff_data(bfd)->symbols)
#define	obj_sym_filepos(bfd)	(coff_data(bfd)->sym_filepos)

#define obj_relocbase(bfd)	(coff_data(bfd)->relocbase)
#define obj_raw_syments(bfd)	(coff_data(bfd)->raw_syments)
#define obj_raw_syment_count(bfd)	(coff_data(bfd)->raw_syment_count)
#define obj_convert(bfd)	(coff_data(bfd)->conversion_table)
#define obj_conv_table_size(bfd) (coff_data(bfd)->conv_table_size)

#define obj_coff_external_syms(bfd) (coff_data (bfd)->external_syms)
#define obj_coff_keep_syms(bfd)	(coff_data (bfd)->keep_syms)
#define obj_coff_strings(bfd)	(coff_data (bfd)->strings)
#define obj_coff_keep_strings(bfd) (coff_data (bfd)->keep_strings)
#define obj_coff_sym_hashes(bfd) (coff_data (bfd)->sym_hashes)

#define obj_coff_local_toc_table(bfd) (coff_data(bfd)->local_toc_sym_map)

/* `Tdata' information kept for COFF files.  */

typedef struct coff_tdata
{
  struct   coff_symbol_struct *symbols;	/* symtab for input bfd */
  unsigned int *conversion_table;
  int conv_table_size;
  file_ptr sym_filepos;

  struct coff_ptr_struct *raw_syments;
  unsigned int raw_syment_count;

  /* These are only valid once writing has begun */
  long int relocbase;

  /* These members communicate important constants about the symbol table
     to GDB's symbol-reading code.  These `constants' unfortunately vary
     from coff implementation to implementation...  */
  unsigned local_n_btmask;
  unsigned local_n_btshft;
  unsigned local_n_tmask;
  unsigned local_n_tshift;
  unsigned local_symesz;
  unsigned local_auxesz;
  unsigned local_linesz;

  /* The unswapped external symbols.  May be NULL.  Read by
     _bfd_coff_get_external_symbols.  */
  PTR external_syms;
  /* If this is true, the external_syms may not be freed.  */
  boolean keep_syms;

  /* The string table.  May be NULL.  Read by
     _bfd_coff_read_string_table.  */
  char *strings;
  /* If this is true, the strings may not be freed.  */
  boolean keep_strings;

  /* is this a PE format coff file */
  int pe;
  /* Used by the COFF backend linker.  */
  struct coff_link_hash_entry **sym_hashes;

  /* used by the pe linker for PowerPC */
  int *local_toc_sym_map;

  struct bfd_link_info *link_info;

  /* Used by coff_find_nearest_line.  */
  PTR line_info;

  /* Copy of some of the f_flags bits in the COFF filehdr structure,
     used by ARM code.  */
  flagword flags;

} coff_data_type;

/* Tdata for pe image files. */
typedef struct pe_tdata
{
  coff_data_type coff;
  struct internal_extra_pe_aouthdr pe_opthdr;
  int dll;
  int has_reloc_section;
  boolean (*in_reloc_p) PARAMS((bfd *, reloc_howto_type *));
  flagword real_flags;
} pe_data_type;

#define pe_data(bfd)		((bfd)->tdata.pe_obj_data)

/* Tdata for XCOFF files.  */

struct xcoff_tdata
{
  /* Basic COFF information.  */
  coff_data_type coff;

  /* True if a large a.out header should be generated.  */
  boolean full_aouthdr;

  /* TOC value.  */
  bfd_vma toc;

  /* Index of section holding TOC.  */
  int sntoc;

  /* Index of section holding entry point.  */
  int snentry;

  /* .text alignment from optional header.  */
  int text_align_power;

  /* .data alignment from optional header.  */
  int data_align_power;

  /* modtype from optional header.  */
  short modtype;

  /* cputype from optional header.  */
  short cputype;

  /* maxdata from optional header.  */
  bfd_size_type maxdata;

  /* maxstack from optional header.  */
  bfd_size_type maxstack;

  /* Used by the XCOFF backend linker.  */
  asection **csects;
  unsigned long *debug_indices;
  unsigned int import_file_id;
};

#define xcoff_data(abfd) ((abfd)->tdata.xcoff_obj_data)

/* We take the address of the first element of a asymbol to ensure that the
 * macro is only ever applied to an asymbol.  */
#define coffsymbol(asymbol) ((coff_symbol_type *)(&((asymbol)->the_bfd)))

/* The used_by_bfd field of a section may be set to a pointer to this
   structure.  */

struct coff_section_tdata
{
  /* The relocs, swapped into COFF internal form.  This may be NULL.  */
  struct internal_reloc *relocs;
  /* If this is true, the relocs entry may not be freed.  */
  boolean keep_relocs;
  /* The section contents.  This may be NULL.  */
  bfd_byte *contents;
  /* If this is true, the contents entry may not be freed.  */
  boolean keep_contents;
  /* Information cached by coff_find_nearest_line.  */
  bfd_vma offset;
  unsigned int i;
  const char *function;
  int line_base;
  /* A pointer used for .stab linking optimizations.  */
  PTR stab_info;
  /* Available for individual backends.  */
  PTR tdata;
};

/* An accessor macro for the coff_section_tdata structure.  */
#define coff_section_data(abfd, sec) \
  ((struct coff_section_tdata *) (sec)->used_by_bfd)

/* Tdata for sections in XCOFF files.  This is used by the linker.  */

struct xcoff_section_tdata
{
  /* Used for XCOFF csects created by the linker; points to the real
     XCOFF section which contains this csect.  */
  asection *enclosing;
  /* The lineno_count field for the enclosing section, because we are
     going to clobber it there.  */
  unsigned int lineno_count;
  /* The first and one past the last symbol indices for symbols used
     by this csect.  */
  unsigned long first_symndx;
  unsigned long last_symndx;
};

/* An accessor macro the xcoff_section_tdata structure.  */
#define xcoff_section_data(abfd, sec) \
  ((struct xcoff_section_tdata *) coff_section_data ((abfd), (sec))->tdata)

/* Tdata for sections in PEI image files.  */

struct pei_section_tdata
{
  /* The virtual size of the section.  */
  bfd_size_type virt_size;
};

/* An accessor macro for the pei_section_tdata structure.  */
#define pei_section_data(abfd, sec) \
  ((struct pei_section_tdata *) coff_section_data ((abfd), (sec))->tdata)

/* COFF linker hash table entries.  */

struct coff_link_hash_entry
{
  struct bfd_link_hash_entry root;

  /* Symbol index in output file.  Set to -1 initially.  Set to -2 if
     there is a reloc against this symbol.  */
  long indx;

  /* Symbol type.  */
  unsigned short type;

  /* Symbol class.  */
  unsigned char class;

  /* Number of auxiliary entries.  */
  char numaux;

  /* BFD to take auxiliary entries from.  */
  bfd *auxbfd;

  /* Pointer to array of auxiliary entries, if any.  */
  union internal_auxent *aux;
};

/* COFF linker hash table.  */

struct coff_link_hash_table
{
  struct bfd_link_hash_table root;
  /* A pointer to information used to link stabs in sections.  */
  PTR stab_info;
};

/* Look up an entry in a COFF linker hash table.  */

#define coff_link_hash_lookup(table, string, create, copy, follow)	\
  ((struct coff_link_hash_entry *)					\
   bfd_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

/* Traverse a COFF linker hash table.  */

#define coff_link_hash_traverse(table, func, info)			\
  (bfd_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct bfd_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the COFF linker hash table from a link_info structure.  */

#define coff_hash_table(p) ((struct coff_link_hash_table *) ((p)->hash))

/* Functions in coffgen.c.  */
extern const bfd_target *coff_object_p PARAMS ((bfd *));
extern struct sec *coff_section_from_bfd_index PARAMS ((bfd *, int));
extern long coff_get_symtab_upper_bound PARAMS ((bfd *));
extern long coff_get_symtab PARAMS ((bfd *, asymbol **));
extern int coff_count_linenumbers PARAMS ((bfd *));
extern struct coff_symbol_struct *coff_symbol_from PARAMS ((bfd *, asymbol *));
extern boolean coff_renumber_symbols PARAMS ((bfd *, int *));
extern void coff_mangle_symbols PARAMS ((bfd *));
extern boolean coff_write_symbols PARAMS ((bfd *));
extern boolean coff_write_linenumbers PARAMS ((bfd *));
extern alent *coff_get_lineno PARAMS ((bfd *, asymbol *));
extern asymbol *coff_section_symbol PARAMS ((bfd *, char *));
extern boolean _bfd_coff_get_external_symbols PARAMS ((bfd *));
extern const char *_bfd_coff_read_string_table PARAMS ((bfd *));
extern boolean _bfd_coff_free_symbols PARAMS ((bfd *));
extern struct coff_ptr_struct *coff_get_normalized_symtab PARAMS ((bfd *));
extern long coff_get_reloc_upper_bound PARAMS ((bfd *, sec_ptr));
extern asymbol *coff_make_empty_symbol PARAMS ((bfd *));
extern void coff_print_symbol PARAMS ((bfd *, PTR filep, asymbol *,
				       bfd_print_symbol_type how));
extern void coff_get_symbol_info PARAMS ((bfd *, asymbol *,
					  symbol_info *ret));
extern boolean _bfd_coff_is_local_label_name PARAMS ((bfd *, const char *));
extern asymbol *coff_bfd_make_debug_symbol PARAMS ((bfd *, PTR,
						    unsigned long));
extern boolean coff_find_nearest_line PARAMS ((bfd *,
					       asection *,
					       asymbol **,
					       bfd_vma offset,
					       CONST char **filename_ptr,
					       CONST char **functionname_ptr,
					       unsigned int *line_ptr));
extern int coff_sizeof_headers PARAMS ((bfd *, boolean reloc));
extern boolean bfd_coff_reloc16_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, boolean *));
extern bfd_byte *bfd_coff_reloc16_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *, boolean relocateable, asymbol **));
extern bfd_vma bfd_coff_reloc16_get_value PARAMS ((arelent *,
						   struct bfd_link_info *,
						   asection *));
extern void bfd_perform_slip PARAMS ((bfd *abfd, unsigned int slip,
				      asection *input_section,
				      bfd_vma val));

/* Functions and types in cofflink.c.  */

#define STRING_SIZE_SIZE (4)

/* We use a hash table to merge identical enum, struct, and union
   definitions in the linker.  */

/* Information we keep for a single element (an enum value, a
   structure or union field) in the debug merge hash table.  */

struct coff_debug_merge_element
{
  /* Next element.  */
  struct coff_debug_merge_element *next;

  /* Name.  */
  const char *name;

  /* Type.  */
  unsigned int type;

  /* Symbol index for complex type.  */
  long tagndx;
};

/* A linked list of debug merge entries for a given name.  */

struct coff_debug_merge_type
{
  /* Next type with the same name.  */
  struct coff_debug_merge_type *next;

  /* Class of type.  */
  int class;

  /* Symbol index where this type is defined.  */
  long indx;

  /* List of elements.  */
  struct coff_debug_merge_element *elements;
};

/* Information we store in the debug merge hash table.  */

struct coff_debug_merge_hash_entry
{
  struct bfd_hash_entry root;

  /* A list of types with this name.  */
  struct coff_debug_merge_type *types;
};

/* The debug merge hash table.  */

struct coff_debug_merge_hash_table
{
  struct bfd_hash_table root;
};

/* Initialize a COFF debug merge hash table.  */

#define coff_debug_merge_hash_table_init(table) \
  (bfd_hash_table_init (&(table)->root, _bfd_coff_debug_merge_hash_newfunc))

/* Free a COFF debug merge hash table.  */

#define coff_debug_merge_hash_table_free(table) \
  (bfd_hash_table_free (&(table)->root))

/* Look up an entry in a COFF debug merge hash table.  */

#define coff_debug_merge_hash_lookup(table, string, create, copy) \
  ((struct coff_debug_merge_hash_entry *) \
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

/* Information we keep for each section in the output file when doing
   a relocateable link.  */

struct coff_link_section_info
{
  /* The relocs to be output.  */
  struct internal_reloc *relocs;
  /* For each reloc against a global symbol whose index was not known
     when the reloc was handled, the global hash table entry.  */
  struct coff_link_hash_entry **rel_hashes;
};

/* Information that we pass around while doing the final link step.  */

struct coff_final_link_info
{
  /* General link information.  */
  struct bfd_link_info *info;
  /* Output BFD.  */
  bfd *output_bfd;
  /* Used to indicate failure in traversal routine.  */
  boolean failed;
  /* If doing "task linking" set only during the time when we want the
     global symbol writer to convert the storage class of defined global
     symbols from global to static. */
  boolean global_to_static;
  /* Hash table for long symbol names.  */
  struct bfd_strtab_hash *strtab;
  /* When doing a relocateable link, an array of information kept for
     each output section, indexed by the target_index field.  */
  struct coff_link_section_info *section_info;
  /* Symbol index of last C_FILE symbol (-1 if none).  */
  long last_file_index;
  /* Contents of last C_FILE symbol.  */
  struct internal_syment last_file;
  /* Symbol index of first aux entry of last .bf symbol with an empty
     endndx field (-1 if none).  */
  long last_bf_index;
  /* Contents of last_bf_index aux entry.  */
  union internal_auxent last_bf;
  /* Hash table used to merge debug information.  */
  struct coff_debug_merge_hash_table debug_merge;
  /* Buffer large enough to hold swapped symbols of any input file.  */
  struct internal_syment *internal_syms;
  /* Buffer large enough to hold sections of symbols of any input file.  */
  asection **sec_ptrs;
  /* Buffer large enough to hold output indices of symbols of any
     input file.  */
  long *sym_indices;
  /* Buffer large enough to hold output symbols for any input file.  */
  bfd_byte *outsyms;
  /* Buffer large enough to hold external line numbers for any input
     section.  */
  bfd_byte *linenos;
  /* Buffer large enough to hold any input section.  */
  bfd_byte *contents;
  /* Buffer large enough to hold external relocs of any input section.  */
  bfd_byte *external_relocs;
  /* Buffer large enough to hold swapped relocs of any input section.  */
  struct internal_reloc *internal_relocs;
};

extern struct bfd_hash_entry *_bfd_coff_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
extern boolean _bfd_coff_link_hash_table_init
  PARAMS ((struct coff_link_hash_table *, bfd *,
	   struct bfd_hash_entry *(*) (struct bfd_hash_entry *,
				       struct bfd_hash_table *,
				       const char *)));
extern struct bfd_link_hash_table *_bfd_coff_link_hash_table_create
  PARAMS ((bfd *));
extern const char *_bfd_coff_internal_syment_name
  PARAMS ((bfd *, const struct internal_syment *, char *));
extern boolean _bfd_coff_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_coff_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
extern struct internal_reloc *_bfd_coff_read_internal_relocs
  PARAMS ((bfd *, asection *, boolean, bfd_byte *, boolean,
	   struct internal_reloc *));
extern boolean _bfd_coff_generic_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));

extern struct bfd_hash_entry *_bfd_coff_debug_merge_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
extern boolean _bfd_coff_write_global_sym
  PARAMS ((struct coff_link_hash_entry *, PTR));
extern boolean _bfd_coff_write_task_globals
  PARAMS ((struct coff_link_hash_entry *, PTR));
extern boolean _bfd_coff_link_input_bfd
  PARAMS ((struct coff_final_link_info *, bfd *));
extern boolean _bfd_coff_reloc_link_order
  PARAMS ((bfd *, struct coff_final_link_info *, asection *,
	   struct bfd_link_order *));


#define coff_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

/* Functions in xcofflink.c.  */

extern long _bfd_xcoff_get_dynamic_symtab_upper_bound PARAMS ((bfd *));
extern long _bfd_xcoff_canonicalize_dynamic_symtab
  PARAMS ((bfd *, asymbol **));
extern long _bfd_xcoff_get_dynamic_reloc_upper_bound PARAMS ((bfd *));
extern long _bfd_xcoff_canonicalize_dynamic_reloc
  PARAMS ((bfd *, arelent **, asymbol **));
extern struct bfd_link_hash_table *_bfd_xcoff_bfd_link_hash_table_create
  PARAMS ((bfd *));
extern boolean _bfd_xcoff_bfd_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_xcoff_bfd_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_ppc_xcoff_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));

/* Functions in coff-ppc.c.  FIXME: These are called be pe.em in the
   linker, and so should start with bfd and be declared in bfd.h.  */

extern boolean ppc_allocate_toc_section PARAMS ((struct bfd_link_info *));
extern boolean ppc_process_before_allocation
  PARAMS ((bfd *, struct bfd_link_info *));

/* And more taken from the source .. */

typedef struct coff_ptr_struct
{

        /* Remembers the offset from the first symbol in the file for
          this symbol. Generated by coff_renumber_symbols. */
unsigned int offset;

        /* Should the value of this symbol be renumbered.  Used for
          XCOFF C_BSTAT symbols.  Set by coff_slurp_symbol_table.  */
unsigned int fix_value : 1;

        /* Should the tag field of this symbol be renumbered.
          Created by coff_pointerize_aux. */
unsigned int fix_tag : 1;

        /* Should the endidx field of this symbol be renumbered.
          Created by coff_pointerize_aux. */
unsigned int fix_end : 1;

        /* Should the x_csect.x_scnlen field be renumbered.
          Created by coff_pointerize_aux. */
unsigned int fix_scnlen : 1;

        /* Fix up an XCOFF C_BINCL/C_EINCL symbol.  The value is the
          index into the line number entries.  Set by
          coff_slurp_symbol_table.  */
unsigned int fix_line : 1;

        /* The container for the symbol structure as read and translated
           from the file. */

union {
   union internal_auxent auxent;
   struct internal_syment syment;
 } u;
} combined_entry_type;


 /* Each canonical asymbol really looks like this: */

typedef struct coff_symbol_struct
{
    /* The actual symbol which the rest of BFD works with */
asymbol symbol;

    /* A pointer to the hidden information for this symbol */
combined_entry_type *native;

    /* A pointer to the linenumber information for this symbol */
struct lineno_cache_entry *lineno;

    /* Have the line numbers been relocated yet ? */
boolean done_lineno;
} coff_symbol_type;
typedef struct
{
  void (*_bfd_coff_swap_aux_in) PARAMS ((
       bfd            *abfd,
       PTR             ext,
       int             type,
       int             class,
       int             indaux,
       int             numaux,
       PTR             in));

  void (*_bfd_coff_swap_sym_in) PARAMS ((
       bfd            *abfd ,
       PTR             ext,
       PTR             in));

  void (*_bfd_coff_swap_lineno_in) PARAMS ((
       bfd            *abfd,
       PTR            ext,
       PTR             in));

 unsigned int (*_bfd_coff_swap_aux_out) PARAMS ((
       bfd     *abfd,
       PTR     in,
       int     type,
       int     class,
       int     indaux,
       int     numaux,
       PTR     ext));

 unsigned int (*_bfd_coff_swap_sym_out) PARAMS ((
      bfd      *abfd,
      PTR      in,
      PTR      ext));

 unsigned int (*_bfd_coff_swap_lineno_out) PARAMS ((
       bfd     *abfd,
       PTR     in,
       PTR     ext));

 unsigned int (*_bfd_coff_swap_reloc_out) PARAMS ((
       bfd     *abfd,
       PTR     src,
       PTR     dst));

 unsigned int (*_bfd_coff_swap_filehdr_out) PARAMS ((
       bfd     *abfd,
       PTR     in,
       PTR     out));

 unsigned int (*_bfd_coff_swap_aouthdr_out) PARAMS ((
       bfd     *abfd,
       PTR     in,
       PTR     out));

 unsigned int (*_bfd_coff_swap_scnhdr_out) PARAMS ((
       bfd     *abfd,
       PTR     in,
       PTR     out));

 unsigned int _bfd_filhsz;
 unsigned int _bfd_aoutsz;
 unsigned int _bfd_scnhsz;
 unsigned int _bfd_symesz;
 unsigned int _bfd_auxesz;
 unsigned int _bfd_relsz;
 unsigned int _bfd_linesz;
 boolean _bfd_coff_long_filenames;
 boolean _bfd_coff_long_section_names;
 unsigned int _bfd_coff_default_section_alignment_power;
 void (*_bfd_coff_swap_filehdr_in) PARAMS ((
       bfd     *abfd,
       PTR     ext,
       PTR     in));
 void (*_bfd_coff_swap_aouthdr_in) PARAMS ((
       bfd     *abfd,
       PTR     ext,
       PTR     in));
 void (*_bfd_coff_swap_scnhdr_in) PARAMS ((
       bfd     *abfd,
       PTR     ext,
       PTR     in));
 void (*_bfd_coff_swap_reloc_in) PARAMS ((
       bfd     *abfd,
       PTR     ext,
       PTR     in));
 boolean (*_bfd_coff_bad_format_hook) PARAMS ((
       bfd     *abfd,
       PTR     internal_filehdr));
 boolean (*_bfd_coff_set_arch_mach_hook) PARAMS ((
       bfd     *abfd,
       PTR     internal_filehdr));
 PTR (*_bfd_coff_mkobject_hook) PARAMS ((
       bfd     *abfd,
       PTR     internal_filehdr,
       PTR     internal_aouthdr));
 flagword (*_bfd_styp_to_sec_flags_hook) PARAMS ((
       bfd     *abfd,
       PTR     internal_scnhdr,
       const char *name));
 void (*_bfd_set_alignment_hook) PARAMS ((
       bfd     *abfd,
       asection *sec,
       PTR     internal_scnhdr));
 boolean (*_bfd_coff_slurp_symbol_table) PARAMS ((
       bfd     *abfd));
 boolean (*_bfd_coff_symname_in_debug) PARAMS ((
       bfd     *abfd,
       struct internal_syment *sym));
 boolean (*_bfd_coff_pointerize_aux_hook) PARAMS ((
       bfd *abfd,
       combined_entry_type *table_base,
       combined_entry_type *symbol,
       unsigned int indaux,
       combined_entry_type *aux));
 boolean (*_bfd_coff_print_aux) PARAMS ((
       bfd *abfd,
       FILE *file,
       combined_entry_type *table_base,
       combined_entry_type *symbol,
       combined_entry_type *aux,
       unsigned int indaux));
 void (*_bfd_coff_reloc16_extra_cases) PARAMS ((
       bfd     *abfd,
       struct bfd_link_info *link_info,
       struct bfd_link_order *link_order,
       arelent *reloc,
       bfd_byte *data,
       unsigned int *src_ptr,
       unsigned int *dst_ptr));
 int (*_bfd_coff_reloc16_estimate) PARAMS ((
       bfd *abfd,
       asection *input_section,
       arelent *r,
       unsigned int shrink,
       struct bfd_link_info *link_info));
 boolean (*_bfd_coff_sym_is_global) PARAMS ((
       bfd *abfd,
       struct internal_syment *));
 boolean (*_bfd_coff_compute_section_file_positions) PARAMS ((
       bfd *abfd));
 boolean (*_bfd_coff_start_final_link) PARAMS ((
       bfd *output_bfd,
       struct bfd_link_info *info));
 boolean (*_bfd_coff_relocate_section) PARAMS ((
       bfd *output_bfd,
       struct bfd_link_info *info,
       bfd *input_bfd,
       asection *input_section,
       bfd_byte *contents,
       struct internal_reloc *relocs,
       struct internal_syment *syms,
       asection **sections));
 reloc_howto_type *(*_bfd_coff_rtype_to_howto) PARAMS ((
       bfd *abfd,
       asection *sec,
       struct internal_reloc *rel,
       struct coff_link_hash_entry *h,
       struct internal_syment *sym,
       bfd_vma *addendp));
 boolean (*_bfd_coff_adjust_symndx) PARAMS ((
       bfd *obfd,
       struct bfd_link_info *info,
       bfd *ibfd,
       asection *sec,
       struct internal_reloc *reloc,
       boolean *adjustedp));
 boolean (*_bfd_coff_link_add_one_symbol) PARAMS ((
       struct bfd_link_info *info,
       bfd *abfd,
       const char *name,
       flagword flags, 
       asection *section,
       bfd_vma value,
       const char *string,
       boolean copy,
       boolean collect, 
       struct bfd_link_hash_entry **hashp));

 boolean (*_bfd_coff_link_output_has_begun) PARAMS ((
       bfd * abfd ));
 boolean (*_bfd_coff_final_link_postscript) PARAMS ((
       bfd * abfd,
       struct coff_final_link_info * pfinfo));

} bfd_coff_backend_data;

#define coff_backend_info(abfd) ((bfd_coff_backend_data *) (abfd)->xvec->backend_data)

#define bfd_coff_swap_aux_in(a,e,t,c,ind,num,i) \
        ((coff_backend_info (a)->_bfd_coff_swap_aux_in) (a,e,t,c,ind,num,i))

#define bfd_coff_swap_sym_in(a,e,i) \
        ((coff_backend_info (a)->_bfd_coff_swap_sym_in) (a,e,i))

#define bfd_coff_swap_lineno_in(a,e,i) \
        ((coff_backend_info ( a)->_bfd_coff_swap_lineno_in) (a,e,i))

#define bfd_coff_swap_reloc_out(abfd, i, o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_reloc_out) (abfd, i, o))

#define bfd_coff_swap_lineno_out(abfd, i, o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_lineno_out) (abfd, i, o))

#define bfd_coff_swap_aux_out(a,i,t,c,ind,num,o) \
        ((coff_backend_info (a)->_bfd_coff_swap_aux_out) (a,i,t,c,ind,num,o))

#define bfd_coff_swap_sym_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_sym_out) (abfd, i, o))

#define bfd_coff_swap_scnhdr_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_scnhdr_out) (abfd, i, o))

#define bfd_coff_swap_filehdr_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_filehdr_out) (abfd, i, o))

#define bfd_coff_swap_aouthdr_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_aouthdr_out) (abfd, i, o))

#define bfd_coff_filhsz(abfd) (coff_backend_info (abfd)->_bfd_filhsz)
#define bfd_coff_aoutsz(abfd) (coff_backend_info (abfd)->_bfd_aoutsz)
#define bfd_coff_scnhsz(abfd) (coff_backend_info (abfd)->_bfd_scnhsz)
#define bfd_coff_symesz(abfd) (coff_backend_info (abfd)->_bfd_symesz)
#define bfd_coff_auxesz(abfd) (coff_backend_info (abfd)->_bfd_auxesz)
#define bfd_coff_relsz(abfd)  (coff_backend_info (abfd)->_bfd_relsz)
#define bfd_coff_linesz(abfd) (coff_backend_info (abfd)->_bfd_linesz)
#define bfd_coff_long_filenames(abfd) (coff_backend_info (abfd)->_bfd_coff_long_filenames)
#define bfd_coff_long_section_names(abfd) \
        (coff_backend_info (abfd)->_bfd_coff_long_section_names)
#define bfd_coff_default_section_alignment_power(abfd) \
        (coff_backend_info (abfd)->_bfd_coff_default_section_alignment_power)
#define bfd_coff_swap_filehdr_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_filehdr_in) (abfd, i, o))

#define bfd_coff_swap_aouthdr_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_aouthdr_in) (abfd, i, o))

#define bfd_coff_swap_scnhdr_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_scnhdr_in) (abfd, i, o))

#define bfd_coff_swap_reloc_in(abfd, i, o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_reloc_in) (abfd, i, o))

#define bfd_coff_bad_format_hook(abfd, filehdr) \
        ((coff_backend_info (abfd)->_bfd_coff_bad_format_hook) (abfd, filehdr))

#define bfd_coff_set_arch_mach_hook(abfd, filehdr)\
        ((coff_backend_info (abfd)->_bfd_coff_set_arch_mach_hook) (abfd, filehdr))
#define bfd_coff_mkobject_hook(abfd, filehdr, aouthdr)\
        ((coff_backend_info (abfd)->_bfd_coff_mkobject_hook) (abfd, filehdr, aouthdr))

#define bfd_coff_styp_to_sec_flags_hook(abfd, scnhdr, name)\
        ((coff_backend_info (abfd)->_bfd_styp_to_sec_flags_hook) (abfd, scnhdr, name))

#define bfd_coff_set_alignment_hook(abfd, sec, scnhdr)\
        ((coff_backend_info (abfd)->_bfd_set_alignment_hook) (abfd, sec, scnhdr))

#define bfd_coff_slurp_symbol_table(abfd)\
        ((coff_backend_info (abfd)->_bfd_coff_slurp_symbol_table) (abfd))

#define bfd_coff_symname_in_debug(abfd, sym)\
        ((coff_backend_info (abfd)->_bfd_coff_symname_in_debug) (abfd, sym))

#define bfd_coff_print_aux(abfd, file, base, symbol, aux, indaux)\
        ((coff_backend_info (abfd)->_bfd_coff_print_aux)\
         (abfd, file, base, symbol, aux, indaux))

#define bfd_coff_reloc16_extra_cases(abfd, link_info, link_order, reloc, data, src_ptr, dst_ptr)\
        ((coff_backend_info (abfd)->_bfd_coff_reloc16_extra_cases)\
         (abfd, link_info, link_order, reloc, data, src_ptr, dst_ptr))

#define bfd_coff_reloc16_estimate(abfd, section, reloc, shrink, link_info)\
        ((coff_backend_info (abfd)->_bfd_coff_reloc16_estimate)\
         (abfd, section, reloc, shrink, link_info))

#define bfd_coff_sym_is_global(abfd, sym)\
        ((coff_backend_info (abfd)->_bfd_coff_sym_is_global)\
         (abfd, sym))

#define bfd_coff_compute_section_file_positions(abfd)\
        ((coff_backend_info (abfd)->_bfd_coff_compute_section_file_positions)\
         (abfd))

#define bfd_coff_start_final_link(obfd, info)\
        ((coff_backend_info (obfd)->_bfd_coff_start_final_link)\
         (obfd, info))
#define bfd_coff_relocate_section(obfd,info,ibfd,o,con,rel,isyms,secs)\
        ((coff_backend_info (ibfd)->_bfd_coff_relocate_section)\
         (obfd, info, ibfd, o, con, rel, isyms, secs))
#define bfd_coff_rtype_to_howto(abfd, sec, rel, h, sym, addendp)\
        ((coff_backend_info (abfd)->_bfd_coff_rtype_to_howto)\
         (abfd, sec, rel, h, sym, addendp))
#define bfd_coff_adjust_symndx(obfd, info, ibfd, sec, rel, adjustedp)\
        ((coff_backend_info (abfd)->_bfd_coff_adjust_symndx)\
         (obfd, info, ibfd, sec, rel, adjustedp))
#define bfd_coff_link_add_one_symbol(info,abfd,name,flags,section,value,string,cp,coll,hashp)\
        ((coff_backend_info (abfd)->_bfd_coff_link_add_one_symbol)\
         (info, abfd, name, flags, section, value, string, cp, coll, hashp))

#define bfd_coff_link_output_has_begun(a) \
        ((coff_backend_info (a)->_bfd_coff_link_output_has_begun) (a))
#define bfd_coff_final_link_postscript(a,p) \
        ((coff_backend_info (a)->_bfd_coff_final_link_postscript) (a,p))

