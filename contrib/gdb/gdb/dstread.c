/* Read apollo DST symbol tables and convert to internal format, for GDB.
   Contributed by Troy Rollo, University of NSW (troy@cbme.unsw.edu.au).
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000
   Free Software Foundation, Inc.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "breakpoint.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "buildsym.h"
#include "obstack.h"

#include "gdb_string.h"

#include "dst.h"

CORE_ADDR cur_src_start_addr, cur_src_end_addr;
dst_sec blocks_info, lines_info, symbols_info;

/* Vector of line number information.  */

static struct linetable *line_vector;

/* Index of next entry to go in line_vector_index.  */

static int line_vector_index;

/* Last line number recorded in the line vector.  */

static int prev_line_number;

/* Number of elements allocated for line_vector currently.  */

static int line_vector_length;

static int init_dst_sections (int);

static void read_dst_symtab (struct objfile *);

static void find_dst_sections (bfd *, sec_ptr, PTR);

static void dst_symfile_init (struct objfile *);

static void dst_new_init (struct objfile *);

static void dst_symfile_read (struct objfile *, int);

static void dst_symfile_finish (struct objfile *);

static void dst_end_symtab (struct objfile *);

static void complete_symtab (char *, CORE_ADDR, unsigned int);

static void dst_start_symtab (void);

static void dst_record_line (int, CORE_ADDR);

/* Manage the vector of line numbers.  */
/* FIXME: Use record_line instead.  */

static void
dst_record_line (int line, CORE_ADDR pc)
{
  struct linetable_entry *e;
  /* Make sure line vector is big enough.  */

  if (line_vector_index + 2 >= line_vector_length)
    {
      line_vector_length *= 2;
      line_vector = (struct linetable *)
	xrealloc ((char *) line_vector, sizeof (struct linetable)
		  + (line_vector_length
		     * sizeof (struct linetable_entry)));
    }

  e = line_vector->item + line_vector_index++;
  e->line = line;
  e->pc = pc;
}

/* Start a new symtab for a new source file.
   It indicates the start of data for one original source file.  */
/* FIXME: use start_symtab, like coffread.c now does.  */

static void
dst_start_symtab (void)
{
  /* Initialize the source file line number information for this file.  */

  if (line_vector)		/* Unlikely, but maybe possible? */
    xfree (line_vector);
  line_vector_index = 0;
  line_vector_length = 1000;
  prev_line_number = -2;	/* Force first line number to be explicit */
  line_vector = (struct linetable *)
    xmalloc (sizeof (struct linetable)
	     + line_vector_length * sizeof (struct linetable_entry));
}

/* Save the vital information from when starting to read a file,
   for use when closing off the current file.
   NAME is the file name the symbols came from, START_ADDR is the first
   text address for the file, and SIZE is the number of bytes of text.  */

static void
complete_symtab (char *name, CORE_ADDR start_addr, unsigned int size)
{
  last_source_file = savestring (name, strlen (name));
  cur_src_start_addr = start_addr;
  cur_src_end_addr = start_addr + size;

  if (current_objfile->ei.entry_point >= cur_src_start_addr &&
      current_objfile->ei.entry_point < cur_src_end_addr)
    {
      current_objfile->ei.entry_file_lowpc = cur_src_start_addr;
      current_objfile->ei.entry_file_highpc = cur_src_end_addr;
    }
}

/* Finish the symbol definitions for one main source file,
   close off all the lexical contexts for that file
   (creating struct block's for them), then make the
   struct symtab for that file and put it in the list of all such. */
/* FIXME: Use end_symtab, like coffread.c now does.  */

static void
dst_end_symtab (struct objfile *objfile)
{
  register struct symtab *symtab;
  register struct blockvector *blockvector;
  register struct linetable *lv;

  /* Create the blockvector that points to all the file's blocks.  */

  blockvector = make_blockvector (objfile);

  /* Now create the symtab object for this source file.  */
  symtab = allocate_symtab (last_source_file, objfile);

  /* Fill in its components.  */
  symtab->blockvector = blockvector;
  symtab->free_code = free_linetable;
  symtab->free_ptr = 0;
  symtab->filename = last_source_file;
  symtab->dirname = NULL;
  symtab->debugformat = obsavestring ("Apollo DST", 10,
				      &objfile->symbol_obstack);
  lv = line_vector;
  lv->nitems = line_vector_index;
  symtab->linetable = (struct linetable *)
    xrealloc ((char *) lv, (sizeof (struct linetable)
			    + lv->nitems * sizeof (struct linetable_entry)));

  free_named_symtabs (symtab->filename);

  /* Reinitialize for beginning of new file. */
  line_vector = 0;
  line_vector_length = -1;
  last_source_file = NULL;
}

/* dst_symfile_init ()
   is the dst-specific initialization routine for reading symbols.

   We will only be called if this is a DST or DST-like file.
   BFD handles figuring out the format of the file, and code in symtab.c
   uses BFD's determination to vector to us.

   The ultimate result is a new symtab (or, FIXME, eventually a psymtab).  */

static void
dst_symfile_init (struct objfile *objfile)
{
  asection *section;
  bfd *abfd = objfile->obfd;

  init_entry_point_info (objfile);

}

/* This function is called for every section; it finds the outer limits
   of the line table (minimum and maximum file offset) so that the
   mainline code can read the whole thing for efficiency.  */

/* ARGSUSED */
static void
find_dst_sections (bfd *abfd, sec_ptr asect, PTR vpinfo)
{
  int size, count;
  long base;
  file_ptr offset, maxoff;
  dst_sec *section;

/* WARNING WILL ROBINSON!  ACCESSING BFD-PRIVATE DATA HERE!  FIXME!  */
  size = asect->_raw_size;
  offset = asect->filepos;
  base = asect->vma;
/* End of warning */

  section = NULL;
  if (!strcmp (asect->name, ".blocks"))
    section = &blocks_info;
  else if (!strcmp (asect->name, ".lines"))
    section = &lines_info;
  else if (!strcmp (asect->name, ".symbols"))
    section = &symbols_info;
  if (!section)
    return;
  section->size = size;
  section->position = offset;
  section->base = base;
}


/* The BFD for this file -- only good while we're actively reading
   symbols into a psymtab or a symtab.  */

static bfd *symfile_bfd;

/* Read a symbol file, after initialization by dst_symfile_init.  */
/* FIXME!  Addr and Mainline are not used yet -- this will not work for
   shared libraries or add_file!  */

/* ARGSUSED */
static void
dst_symfile_read (struct objfile *objfile, int mainline)
{
  bfd *abfd = objfile->obfd;
  char *name = bfd_get_filename (abfd);
  int desc;
  register int val;
  int num_symbols;
  int symtab_offset;
  int stringtab_offset;

  symfile_bfd = abfd;		/* Kludge for swap routines */

/* WARNING WILL ROBINSON!  ACCESSING BFD-PRIVATE DATA HERE!  FIXME!  */
  desc = fileno ((FILE *) (abfd->iostream));	/* File descriptor */

  /* Read the line number table, all at once.  */
  bfd_map_over_sections (abfd, find_dst_sections, (PTR) NULL);

  val = init_dst_sections (desc);
  if (val < 0)
    error ("\"%s\": error reading debugging symbol tables\n", name);

  init_minimal_symbol_collection ();
  make_cleanup_discard_minimal_symbols ();

  /* Now that the executable file is positioned at symbol table,
     process it and define symbols accordingly.  */

  read_dst_symtab (objfile);

  /* Sort symbols alphabetically within each block.  */

  {
    struct symtab *s;
    for (s = objfile->symtabs; s != NULL; s = s->next)
      {
	sort_symtab_syms (s);
      }
  }

  /* Install any minimal symbols that have been collected as the current
     minimal symbols for this objfile. */

  install_minimal_symbols (objfile);
}

static void
dst_new_init (struct objfile *ignore)
{
  /* Nothin' to do */
}

/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

static void
dst_symfile_finish (struct objfile *objfile)
{
  /* Nothing to do */
}


/* Get the next line number from the DST. Returns 0 when we hit an
 * end directive or cannot continue for any other reason.
 *
 * Note that ordinary pc deltas are multiplied by two. Apparently
 * this is what was really intended.
 */
static int
get_dst_line (signed char **buffer, long *pc)
{
  static last_pc = 0;
  static long last_line = 0;
  static int last_file = 0;
  dst_ln_entry_ptr_t entry;
  int size;
  dst_src_loc_t *src_loc;

  if (*pc != -1)
    {
      last_pc = *pc;
      *pc = -1;
    }
  entry = (dst_ln_entry_ptr_t) * buffer;

  while (dst_ln_ln_delta (*entry) == dst_ln_escape_flag)
    {
      switch (entry->esc.esc_code)
	{
	case dst_ln_pad:
	  size = 1;		/* pad byte */
	  break;
	case dst_ln_file:
	  /* file escape.  Next 4 bytes are a dst_src_loc_t */
	  size = 5;
	  src_loc = (dst_src_loc_t *) (*buffer + 1);
	  last_line = src_loc->line_number;
	  last_file = src_loc->file_index;
	  break;
	case dst_ln_dln1_dpc1:
	  /* 1 byte line delta, 1 byte pc delta */
	  last_line += (*buffer)[1];
	  last_pc += 2 * (unsigned char) (*buffer)[2];
	  dst_record_line (last_line, last_pc);
	  size = 3;
	  break;
	case dst_ln_dln2_dpc2:
	  /* 2 bytes line delta, 2 bytes pc delta */
	  last_line += *(short *) (*buffer + 1);
	  last_pc += 2 * (*(short *) (*buffer + 3));
	  size = 5;
	  dst_record_line (last_line, last_pc);
	  break;
	case dst_ln_ln4_pc4:
	  /* 4 bytes ABSOLUTE line number, 4 bytes ABSOLUTE pc */
	  last_line = *(unsigned long *) (*buffer + 1);
	  last_pc = *(unsigned long *) (*buffer + 5);
	  size = 9;
	  dst_record_line (last_line, last_pc);
	  break;
	case dst_ln_dln1_dpc0:
	  /* 1 byte line delta, pc delta = 0 */
	  size = 2;
	  last_line += (*buffer)[1];
	  break;
	case dst_ln_ln_off_1:
	  /* statement escape, stmt # = 1 (2nd stmt on line) */
	  size = 1;
	  break;
	case dst_ln_ln_off:
	  /* statement escape, stmt # = next byte */
	  size = 2;
	  break;
	case dst_ln_entry:
	  /* entry escape, next byte is entry number */
	  size = 2;
	  break;
	case dst_ln_exit:
	  /* exit escape */
	  size = 1;
	  break;
	case dst_ln_stmt_end:
	  /* gap escape, 4 bytes pc delta */
	  size = 5;
	  /* last_pc += 2 * (*(long *) (*buffer + 1)); */
	  /* Apparently this isn't supposed to actually modify
	   * the pc value. Totally weird.
	   */
	  break;
	case dst_ln_escape_11:
	case dst_ln_escape_12:
	case dst_ln_escape_13:
	  size = 1;
	  break;
	case dst_ln_nxt_byte:
	  /* This shouldn't happen. If it does, we're SOL */
	  return 0;
	  break;
	case dst_ln_end:
	  /* end escape, final entry follows */
	  return 0;
	}
      *buffer += (size < 0) ? -size : size;
      entry = (dst_ln_entry_ptr_t) * buffer;
    }
  last_line += dst_ln_ln_delta (*entry);
  last_pc += entry->delta.pc_delta * 2;
  (*buffer)++;
  dst_record_line (last_line, last_pc);
  return 1;
}

static void
enter_all_lines (char *buffer, long address)
{
  if (buffer)
    while (get_dst_line (&buffer, &address));
}

static int
get_dst_entry (char *buffer, dst_rec_ptr_t *ret_entry)
{
  int size;
  dst_rec_ptr_t entry;
  static int last_type;
  int ar_size;
  static unsigned lu3;

  entry = (dst_rec_ptr_t) buffer;
  switch (entry->rec_type)
    {
    case dst_typ_pad:
      size = 0;
      break;
    case dst_typ_comp_unit:
      size = sizeof (DST_comp_unit (entry));
      break;
    case dst_typ_section_tab:
      size = sizeof (DST_section_tab (entry))
	+ ((int) DST_section_tab (entry).number_of_sections
	   - dst_dummy_array_size) * sizeof (long);
      break;
    case dst_typ_file_tab:
      size = sizeof (DST_file_tab (entry))
	+ ((int) DST_file_tab (entry).number_of_files
	   - dst_dummy_array_size) * sizeof (dst_file_desc_t);
      break;
    case dst_typ_block:
      size = sizeof (DST_block (entry))
	+ ((int) DST_block (entry).n_of_code_ranges
	   - dst_dummy_array_size) * sizeof (dst_code_range_t);
      break;
    case dst_typ_5:
      size = -1;
      break;
    case dst_typ_var:
      size = sizeof (DST_var (entry)) -
	sizeof (dst_var_loc_long_t) * dst_dummy_array_size +
	DST_var (entry).no_of_locs *
	(DST_var (entry).short_locs ?
	 sizeof (dst_var_loc_short_t) :
	 sizeof (dst_var_loc_long_t));
      break;
    case dst_typ_pointer:
      size = sizeof (DST_pointer (entry));
      break;
    case dst_typ_array:
      size = sizeof (DST_array (entry));
      break;
    case dst_typ_subrange:
      size = sizeof (DST_subrange (entry));
      break;
    case dst_typ_set:
      size = sizeof (DST_set (entry));
      break;
    case dst_typ_implicit_enum:
      size = sizeof (DST_implicit_enum (entry))
	+ ((int) DST_implicit_enum (entry).nelems
	   - dst_dummy_array_size) * sizeof (dst_rel_offset_t);
      break;
    case dst_typ_explicit_enum:
      size = sizeof (DST_explicit_enum (entry))
	+ ((int) DST_explicit_enum (entry).nelems
	   - dst_dummy_array_size) * sizeof (dst_enum_elem_t);
      break;
    case dst_typ_short_rec:
      size = sizeof (DST_short_rec (entry))
	+ DST_short_rec (entry).nfields * sizeof (dst_short_field_t)
	- dst_dummy_array_size * sizeof (dst_field_t);
      break;
    case dst_typ_short_union:
      size = sizeof (DST_short_union (entry))
	+ DST_short_union (entry).nfields * sizeof (dst_short_field_t)
	- dst_dummy_array_size * sizeof (dst_field_t);
      break;
    case dst_typ_file:
      size = sizeof (DST_file (entry));
      break;
    case dst_typ_offset:
      size = sizeof (DST_offset (entry));
      break;
    case dst_typ_alias:
      size = sizeof (DST_alias (entry));
      break;
    case dst_typ_signature:
      size = sizeof (DST_signature (entry)) +
	((int) DST_signature (entry).nargs -
	 dst_dummy_array_size) * sizeof (dst_arg_t);
      break;
    case dst_typ_21:
      size = -1;
      break;
    case dst_typ_old_label:
      size = sizeof (DST_old_label (entry));
      break;
    case dst_typ_scope:
      size = sizeof (DST_scope (entry));
      break;
    case dst_typ_end_scope:
      size = 0;
      break;
    case dst_typ_25:
    case dst_typ_26:
      size = -1;
      break;
    case dst_typ_string_tab:
    case dst_typ_global_name_tab:
      size = sizeof (DST_string_tab (entry))
	+ DST_string_tab (entry).length
	- dst_dummy_array_size;
      break;
    case dst_typ_forward:
      size = sizeof (DST_forward (entry));
      get_dst_entry ((char *) entry + DST_forward (entry).rec_off, &entry);
      break;
    case dst_typ_aux_size:
      size = sizeof (DST_aux_size (entry));
      break;
    case dst_typ_aux_align:
      size = sizeof (DST_aux_align (entry));
      break;
    case dst_typ_aux_field_size:
      size = sizeof (DST_aux_field_size (entry));
      break;
    case dst_typ_aux_field_off:
      size = sizeof (DST_aux_field_off (entry));
      break;
    case dst_typ_aux_field_align:
      size = sizeof (DST_aux_field_align (entry));
      break;
    case dst_typ_aux_qual:
      size = sizeof (DST_aux_qual (entry));
      break;
    case dst_typ_aux_var_bound:
      size = sizeof (DST_aux_var_bound (entry));
      break;
    case dst_typ_extension:
      size = DST_extension (entry).rec_size;
      break;
    case dst_typ_string:
      size = sizeof (DST_string (entry));
      break;
    case dst_typ_old_entry:
      size = 48;		/* Obsolete entry type */
      break;
    case dst_typ_const:
      size = sizeof (DST_const (entry))
	+ DST_const (entry).value.length
	- sizeof (DST_const (entry).value.val);
      break;
    case dst_typ_reference:
      size = sizeof (DST_reference (entry));
      break;
    case dst_typ_old_record:
    case dst_typ_old_union:
    case dst_typ_record:
    case dst_typ_union:
      size = sizeof (DST_record (entry))
	+ ((int) DST_record (entry).nfields
	   - dst_dummy_array_size) * sizeof (dst_field_t);
      break;
    case dst_typ_aux_type_deriv:
      size = sizeof (DST_aux_type_deriv (entry));
      break;
    case dst_typ_locpool:
      size = sizeof (DST_locpool (entry))
	+ ((int) DST_locpool (entry).length -
	   dst_dummy_array_size);
      break;
    case dst_typ_variable:
      size = sizeof (DST_variable (entry));
      break;
    case dst_typ_label:
      size = sizeof (DST_label (entry));
      break;
    case dst_typ_entry:
      size = sizeof (DST_entry (entry));
      break;
    case dst_typ_aux_lifetime:
      size = sizeof (DST_aux_lifetime (entry));
      break;
    case dst_typ_aux_ptr_base:
      size = sizeof (DST_aux_ptr_base (entry));
      break;
    case dst_typ_aux_src_range:
      size = sizeof (DST_aux_src_range (entry));
      break;
    case dst_typ_aux_reg_val:
      size = sizeof (DST_aux_reg_val (entry));
      break;
    case dst_typ_aux_unit_names:
      size = sizeof (DST_aux_unit_names (entry))
	+ ((int) DST_aux_unit_names (entry).number_of_names
	   - dst_dummy_array_size) * sizeof (dst_rel_offset_t);
      break;
    case dst_typ_aux_sect_info:
      size = sizeof (DST_aux_sect_info (entry))
	+ ((int) DST_aux_sect_info (entry).number_of_refs
	   - dst_dummy_array_size) * sizeof (dst_sect_ref_t);
      break;
    default:
      size = -1;
      break;
    }
  if (size == -1)
    {
      fprintf_unfiltered (gdb_stderr, "Warning: unexpected DST entry type (%d) found\nLast valid entry was of type: %d\n",
			  (int) entry->rec_type,
			  last_type);
      fprintf_unfiltered (gdb_stderr, "Last unknown_3 value: %d\n", lu3);
      size = 0;
    }
  else
    last_type = entry->rec_type;
  if (size & 1)			/* Align on a word boundary */
    size++;
  size += 2;
  *ret_entry = entry;
  return size;
}

static int
next_dst_entry (char **buffer, dst_rec_ptr_t *entry, dst_sec *table)
{
  if (*buffer - table->buffer >= table->size)
    {
      *entry = NULL;
      return 0;
    }
  *buffer += get_dst_entry (*buffer, entry);
  return 1;
}

#define NEXT_BLK(a, b) next_dst_entry(a, b, &blocks_info)
#define NEXT_SYM(a, b) next_dst_entry(a, b, &symbols_info)
#define	DST_OFFSET(a, b) ((char *) (a) + (b))

static dst_rec_ptr_t section_table = NULL;

char *
get_sec_ref (dst_sect_ref_t *ref)
{
  dst_sec *section = NULL;
  long offset;

  if (!section_table || !ref->sect_index)
    return NULL;
  offset = DST_section_tab (section_table).section_base[ref->sect_index - 1]
    + ref->sect_offset;
  if (offset >= blocks_info.base &&
      offset < blocks_info.base + blocks_info.size)
    section = &blocks_info;
  else if (offset >= symbols_info.base &&
	   offset < symbols_info.base + symbols_info.size)
    section = &symbols_info;
  else if (offset >= lines_info.base &&
	   offset < lines_info.base + lines_info.size)
    section = &lines_info;
  if (!section)
    return NULL;
  return section->buffer + (offset - section->base);
}

CORE_ADDR
dst_get_addr (int section, long offset)
{
  if (!section_table || !section)
    return 0;
  return DST_section_tab (section_table).section_base[section - 1] + offset;
}

CORE_ADDR
dst_sym_addr (dst_sect_ref_t *ref)
{
  if (!section_table || !ref->sect_index)
    return 0;
  return DST_section_tab (section_table).section_base[ref->sect_index - 1]
    + ref->sect_offset;
}

static struct type *
create_new_type (struct objfile *objfile)
{
  struct type *type;

  type = (struct type *)
    obstack_alloc (&objfile->symbol_obstack, sizeof (struct type));
  memset (type, 0, sizeof (struct type));
  return type;
}

static struct symbol *
create_new_symbol (struct objfile *objfile, char *name)
{
  struct symbol *sym = (struct symbol *)
  obstack_alloc (&objfile->symbol_obstack, sizeof (struct symbol));
  memset (sym, 0, sizeof (struct symbol));
  SYMBOL_NAME (sym) = obsavestring (name, strlen (name),
				    &objfile->symbol_obstack);
  SYMBOL_VALUE (sym) = 0;
  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;

  SYMBOL_CLASS (sym) = LOC_BLOCK;
  return sym;
};

static struct type *decode_dst_type (struct objfile *, dst_rec_ptr_t);

static struct type *
decode_type_desc (struct objfile *objfile, dst_type_t *type_desc,
		  dst_rec_ptr_t base)
{
  struct type *type;
  dst_rec_ptr_t entry;
  if (type_desc->std_type.user_defined_type)
    {
      entry = (dst_rec_ptr_t) DST_OFFSET (base,
					  dst_user_type_offset (*type_desc));
      type = decode_dst_type (objfile, entry);
    }
  else
    {
      switch (type_desc->std_type.dtc)
	{
	case dst_int8_type:
	  type = builtin_type_signed_char;
	  break;
	case dst_int16_type:
	  type = builtin_type_short;
	  break;
	case dst_int32_type:
	  type = builtin_type_long;
	  break;
	case dst_uint8_type:
	  type = builtin_type_unsigned_char;
	  break;
	case dst_uint16_type:
	  type = builtin_type_unsigned_short;
	  break;
	case dst_uint32_type:
	  type = builtin_type_unsigned_long;
	  break;
	case dst_real32_type:
	  type = builtin_type_float;
	  break;
	case dst_real64_type:
	  type = builtin_type_double;
	  break;
	case dst_complex_type:
	  type = builtin_type_complex;
	  break;
	case dst_dcomplex_type:
	  type = builtin_type_double_complex;
	  break;
	case dst_bool8_type:
	  type = builtin_type_char;
	  break;
	case dst_bool16_type:
	  type = builtin_type_short;
	  break;
	case dst_bool32_type:
	  type = builtin_type_long;
	  break;
	case dst_char_type:
	  type = builtin_type_char;
	  break;
	  /* The next few are more complex. I will take care
	   * of them properly at a later point.
	   */
	case dst_string_type:
	  type = builtin_type_void;
	  break;
	case dst_ptr_type:
	  type = builtin_type_void;
	  break;
	case dst_set_type:
	  type = builtin_type_void;
	  break;
	case dst_proc_type:
	  type = builtin_type_void;
	  break;
	case dst_func_type:
	  type = builtin_type_void;
	  break;
	  /* Back tto some ordinary ones */
	case dst_void_type:
	  type = builtin_type_void;
	  break;
	case dst_uchar_type:
	  type = builtin_type_unsigned_char;
	  break;
	default:
	  type = builtin_type_void;
	  break;
	}
    }
  return type;
}

struct structure_list
{
  struct structure_list *next;
  struct type *type;
};

static struct structure_list *struct_list = NULL;

static struct type *
find_dst_structure (char *name)
{
  struct structure_list *element;

  for (element = struct_list; element; element = element->next)
    if (!strcmp (name, TYPE_NAME (element->type)))
      return element->type;
  return NULL;
}


static struct type *
decode_dst_structure (struct objfile *objfile, dst_rec_ptr_t entry, int code,
		      int version)
{
  struct type *type, *child_type;
  char *struct_name;
  char *name, *field_name;
  int i;
  int fieldoffset, fieldsize;
  dst_type_t type_desc;
  struct structure_list *element;

  struct_name = DST_OFFSET (entry, DST_record (entry).noffset);
  name = concat ((code == TYPE_CODE_UNION) ? "union " : "struct ",
		 struct_name, NULL);
  type = find_dst_structure (name);
  if (type)
    {
      xfree (name);
      return type;
    }
  type = create_new_type (objfile);
  TYPE_NAME (type) = obstack_copy0 (&objfile->symbol_obstack,
				    name, strlen (name));
  xfree (name);
  TYPE_CODE (type) = code;
  TYPE_LENGTH (type) = DST_record (entry).size;
  TYPE_NFIELDS (type) = DST_record (entry).nfields;
  TYPE_FIELDS (type) = (struct field *)
    obstack_alloc (&objfile->symbol_obstack, sizeof (struct field) *
		   DST_record (entry).nfields);
  fieldoffset = fieldsize = 0;
  INIT_CPLUS_SPECIFIC (type);
  element = (struct structure_list *)
    xmalloc (sizeof (struct structure_list));
  element->type = type;
  element->next = struct_list;
  struct_list = element;
  for (i = 0; i < DST_record (entry).nfields; i++)
    {
      switch (version)
	{
	case 2:
	  field_name = DST_OFFSET (entry,
				   DST_record (entry).f.ofields[i].noffset);
	  fieldoffset = DST_record (entry).f.ofields[i].foffset * 8 +
	    DST_record (entry).f.ofields[i].bit_offset;
	  fieldsize = DST_record (entry).f.ofields[i].size;
	  type_desc = DST_record (entry).f.ofields[i].type_desc;
	  break;
	case 1:
	  field_name = DST_OFFSET (entry,
				   DST_record (entry).f.fields[i].noffset);
	  type_desc = DST_record (entry).f.fields[i].type_desc;
	  switch (DST_record (entry).f.fields[i].f.field_loc.format_tag)
	    {
	    case dst_field_byte:
	      fieldoffset = DST_record (entry).f.
		fields[i].f.field_byte.offset * 8;
	      fieldsize = -1;
	      break;
	    case dst_field_bit:
	      fieldoffset = DST_record (entry).f.
		fields[i].f.field_bit.byte_offset * 8 +
		DST_record (entry).f.
		fields[i].f.field_bit.bit_offset;
	      fieldsize = DST_record (entry).f.
		fields[i].f.field_bit.nbits;
	      break;
	    case dst_field_loc:
	      fieldoffset += fieldsize;
	      fieldsize = -1;
	      break;
	    }
	  break;
	case 0:
	  field_name = DST_OFFSET (entry,
				   DST_record (entry).f.sfields[i].noffset);
	  fieldoffset = DST_record (entry).f.sfields[i].foffset;
	  type_desc = DST_record (entry).f.sfields[i].type_desc;
	  if (i < DST_record (entry).nfields - 1)
	    fieldsize = DST_record (entry).f.sfields[i + 1].foffset;
	  else
	    fieldsize = DST_record (entry).size;
	  fieldsize -= fieldoffset;
	  fieldoffset *= 8;
	  fieldsize *= 8;
	}
      TYPE_FIELDS (type)[i].name =
	obstack_copy0 (&objfile->symbol_obstack,
		       field_name, strlen (field_name));
      TYPE_FIELDS (type)[i].type = decode_type_desc (objfile,
						     &type_desc,
						     entry);
      if (fieldsize == -1)
	fieldsize = TYPE_LENGTH (TYPE_FIELDS (type)[i].type) *
	  8;
      TYPE_FIELDS (type)[i].bitsize = fieldsize;
      TYPE_FIELDS (type)[i].bitpos = fieldoffset;
    }
  return type;
}

static struct type *
decode_dst_type (struct objfile *objfile, dst_rec_ptr_t entry)
{
  struct type *child_type, *type, *range_type, *index_type;

  switch (entry->rec_type)
    {
    case dst_typ_var:
      return decode_type_desc (objfile,
			       &DST_var (entry).type_desc,
			       entry);
      break;
    case dst_typ_variable:
      return decode_type_desc (objfile,
			       &DST_variable (entry).type_desc,
			       entry);
      break;
    case dst_typ_short_rec:
      return decode_dst_structure (objfile, entry, TYPE_CODE_STRUCT, 0);
    case dst_typ_short_union:
      return decode_dst_structure (objfile, entry, TYPE_CODE_UNION, 0);
    case dst_typ_union:
      return decode_dst_structure (objfile, entry, TYPE_CODE_UNION, 1);
    case dst_typ_record:
      return decode_dst_structure (objfile, entry, TYPE_CODE_STRUCT, 1);
    case dst_typ_old_union:
      return decode_dst_structure (objfile, entry, TYPE_CODE_UNION, 2);
    case dst_typ_old_record:
      return decode_dst_structure (objfile, entry, TYPE_CODE_STRUCT, 2);
    case dst_typ_pointer:
      return make_pointer_type (
				 decode_type_desc (objfile,
					     &DST_pointer (entry).type_desc,
						   entry),
				 NULL);
    case dst_typ_array:
      child_type = decode_type_desc (objfile,
				     &DST_pointer (entry).type_desc,
				     entry);
      index_type = lookup_fundamental_type (objfile,
					    FT_INTEGER);
      range_type = create_range_type ((struct type *) NULL,
				      index_type, DST_array (entry).lo_bound,
				      DST_array (entry).hi_bound);
      return create_array_type ((struct type *) NULL, child_type,
				range_type);
    case dst_typ_alias:
      return decode_type_desc (objfile,
			       &DST_alias (entry).type_desc,
			       entry);
    default:
      return builtin_type_int;
    }
}

struct symbol_list
{
  struct symbol_list *next;
  struct symbol *symbol;
};

static struct symbol_list *dst_global_symbols = NULL;
static int total_globals = 0;

static void
decode_dst_locstring (char *locstr, struct symbol *sym)
{
  dst_loc_entry_t *entry, *next_entry;
  CORE_ADDR temp;
  int count = 0;

  while (1)
    {
      if (count++ == 100)
	{
	  fprintf_unfiltered (gdb_stderr, "Error reading locstring\n");
	  break;
	}
      entry = (dst_loc_entry_t *) locstr;
      next_entry = (dst_loc_entry_t *) (locstr + 1);
      switch (entry->header.code)
	{
	case dst_lsc_end:	/* End of string */
	  return;
	case dst_lsc_indirect:	/* Indirect through previous. Arg == 6 */
	  /* Or register ax x == arg */
	  if (entry->header.arg < 6)
	    {
	      SYMBOL_CLASS (sym) = LOC_REGISTER;
	      SYMBOL_VALUE (sym) = entry->header.arg + 8;
	    }
	  /* We predict indirects */
	  locstr++;
	  break;
	case dst_lsc_dreg:
	  SYMBOL_CLASS (sym) = LOC_REGISTER;
	  SYMBOL_VALUE (sym) = entry->header.arg;
	  locstr++;
	  break;
	case dst_lsc_section:	/* Section (arg+1) */
	  SYMBOL_VALUE (sym) = dst_get_addr (entry->header.arg + 1, 0);
	  locstr++;
	  break;
	case dst_lsc_sec_byte:	/* Section (next_byte+1) */
	  SYMBOL_VALUE (sym) = dst_get_addr (locstr[1] + 1, 0);
	  locstr += 2;
	  break;
	case dst_lsc_add:	/* Add (arg+1)*2 */
	case dst_lsc_sub:	/* Subtract (arg+1)*2 */
	  temp = (entry->header.arg + 1) * 2;
	  locstr++;
	  if (*locstr == dst_multiply_256)
	    {
	      temp <<= 8;
	      locstr++;
	    }
	  switch (entry->header.code)
	    {
	    case dst_lsc_add:
	      if (SYMBOL_CLASS (sym) == LOC_LOCAL)
		SYMBOL_CLASS (sym) = LOC_ARG;
	      SYMBOL_VALUE (sym) += temp;
	      break;
	    case dst_lsc_sub:
	      SYMBOL_VALUE (sym) -= temp;
	      break;
	    }
	  break;
	case dst_lsc_add_byte:
	case dst_lsc_sub_byte:
	  switch (entry->header.arg & 0x03)
	    {
	    case 1:
	      temp = (unsigned char) locstr[1];
	      locstr += 2;
	      break;
	    case 2:
	      temp = *(unsigned short *) (locstr + 1);
	      locstr += 3;
	      break;
	    case 3:
	      temp = *(unsigned long *) (locstr + 1);
	      locstr += 5;
	      break;
	    }
	  if (*locstr == dst_multiply_256)
	    {
	      temp <<= 8;
	      locstr++;
	    }
	  switch (entry->header.code)
	    {
	    case dst_lsc_add_byte:
	      if (SYMBOL_CLASS (sym) == LOC_LOCAL)
		SYMBOL_CLASS (sym) = LOC_ARG;
	      SYMBOL_VALUE (sym) += temp;
	      break;
	    case dst_lsc_sub_byte:
	      SYMBOL_VALUE (sym) -= temp;
	      break;
	    }
	  break;
	case dst_lsc_sbreg:	/* Stack base register (frame pointer). Arg==0 */
	  if (next_entry->header.code != dst_lsc_indirect)
	    {
	      SYMBOL_VALUE (sym) = 0;
	      SYMBOL_CLASS (sym) = LOC_STATIC;
	      return;
	    }
	  SYMBOL_VALUE (sym) = 0;
	  SYMBOL_CLASS (sym) = LOC_LOCAL;
	  locstr++;
	  break;
	default:
	  SYMBOL_VALUE (sym) = 0;
	  SYMBOL_CLASS (sym) = LOC_STATIC;
	  return;
	}
    }
}

static struct symbol_list *
process_dst_symbols (struct objfile *objfile, dst_rec_ptr_t entry, char *name,
		     int *nsyms_ret)
{
  struct symbol_list *list = NULL, *element;
  struct symbol *sym;
  char *symname;
  int nsyms = 0;
  char *location;
  long line;
  dst_type_t symtype;
  struct type *type;
  dst_var_attr_t attr;
  dst_var_loc_t loc_type;
  unsigned loc_index;
  long loc_value;

  if (!entry)
    {
      *nsyms_ret = 0;
      return NULL;
    }
  location = (char *) entry;
  while (NEXT_SYM (&location, &entry) &&
	 entry->rec_type != dst_typ_end_scope)
    {
      if (entry->rec_type == dst_typ_var)
	{
	  if (DST_var (entry).short_locs)
	    {
	      loc_type = DST_var (entry).locs.shorts[0].loc_type;
	      loc_index = DST_var (entry).locs.shorts[0].loc_index;
	      loc_value = DST_var (entry).locs.shorts[0].location;
	    }
	  else
	    {
	      loc_type = DST_var (entry).locs.longs[0].loc_type;
	      loc_index = DST_var (entry).locs.longs[0].loc_index;
	      loc_value = DST_var (entry).locs.longs[0].location;
	    }
	  if (loc_type == dst_var_loc_external)
	    continue;
	  symname = DST_OFFSET (entry, DST_var (entry).noffset);
	  line = DST_var (entry).src_loc.line_number;
	  symtype = DST_var (entry).type_desc;
	  attr = DST_var (entry).attributes;
	}
      else if (entry->rec_type == dst_typ_variable)
	{
	  symname = DST_OFFSET (entry,
				DST_variable (entry).noffset);
	  line = DST_variable (entry).src_loc.line_number;
	  symtype = DST_variable (entry).type_desc;
	  attr = DST_variable (entry).attributes;
	}
      else
	{
	  continue;
	}
      if (symname && name && !strcmp (symname, name))
	/* It's the function return value */
	continue;
      sym = create_new_symbol (objfile, symname);

      if ((attr & (1 << dst_var_attr_global)) ||
	  (attr & (1 << dst_var_attr_static)))
	SYMBOL_CLASS (sym) = LOC_STATIC;
      else
	SYMBOL_CLASS (sym) = LOC_LOCAL;
      SYMBOL_LINE (sym) = line;
      SYMBOL_TYPE (sym) = decode_type_desc (objfile, &symtype,
					    entry);
      SYMBOL_VALUE (sym) = 0;
      switch (entry->rec_type)
	{
	case dst_typ_var:
	  switch (loc_type)
	    {
	    case dst_var_loc_abs:
	      SYMBOL_VALUE_ADDRESS (sym) = loc_value;
	      break;
	    case dst_var_loc_sect_off:
	    case dst_var_loc_ind_sect_off:	/* What is this? */
	      SYMBOL_VALUE_ADDRESS (sym) = dst_get_addr (
							  loc_index,
							  loc_value);
	      break;
	    case dst_var_loc_ind_reg_rel:	/* What is this? */
	    case dst_var_loc_reg_rel:
	      /* If it isn't fp relative, specify the
	       * register it's relative to.
	       */
	      if (loc_index)
		{
		  sym->aux_value.basereg = loc_index;
		}
	      SYMBOL_VALUE (sym) = loc_value;
	      if (loc_value > 0 &&
		  SYMBOL_CLASS (sym) == LOC_BASEREG)
		SYMBOL_CLASS (sym) = LOC_BASEREG_ARG;
	      break;
	    case dst_var_loc_reg:
	      SYMBOL_VALUE (sym) = loc_index;
	      SYMBOL_CLASS (sym) = LOC_REGISTER;
	      break;
	    }
	  break;
	case dst_typ_variable:
	  /* External variable..... don't try to interpret
	   * its nonexistant locstring.
	   */
	  if (DST_variable (entry).loffset == -1)
	    continue;
	  decode_dst_locstring (DST_OFFSET (entry,
					    DST_variable (entry).loffset),
				sym);
	}
      element = (struct symbol_list *)
	xmalloc (sizeof (struct symbol_list));

      if (attr & (1 << dst_var_attr_global))
	{
	  element->next = dst_global_symbols;
	  dst_global_symbols = element;
	  total_globals++;
	}
      else
	{
	  element->next = list;
	  list = element;
	  nsyms++;
	}
      element->symbol = sym;
    }
  *nsyms_ret = nsyms;
  return list;
}


static struct symbol *
process_dst_function (struct objfile *objfile, dst_rec_ptr_t entry, char *name,
		      CORE_ADDR address)
{
  struct symbol *sym;
  struct type *type, *ftype;
  dst_rec_ptr_t sym_entry, typ_entry;
  char *location;
  struct symbol_list *element;

  type = builtin_type_int;
  sym = create_new_symbol (objfile, name);
  SYMBOL_CLASS (sym) = LOC_BLOCK;

  if (entry)
    {
      location = (char *) entry;
      do
	{
	  NEXT_SYM (&location, &sym_entry);
	}
      while (sym_entry && sym_entry->rec_type != dst_typ_signature);

      if (sym_entry)
	{
	  SYMBOL_LINE (sym) =
	    DST_signature (sym_entry).src_loc.line_number;
	  if (DST_signature (sym_entry).result)
	    {
	      typ_entry = (dst_rec_ptr_t)
		DST_OFFSET (sym_entry,
			    DST_signature (sym_entry).result);
	      type = decode_dst_type (objfile, typ_entry);
	    }
	}
    }

  if (!type->function_type)
    {
      ftype = create_new_type (objfile);
      type->function_type = ftype;
      ftype->target_type = type;
      ftype->code = TYPE_CODE_FUNC;
    }
  SYMBOL_TYPE (sym) = type->function_type;

  /* Now add ourselves to the global symbols list */
  element = (struct symbol_list *)
    xmalloc (sizeof (struct symbol_list));

  element->next = dst_global_symbols;
  dst_global_symbols = element;
  total_globals++;
  element->symbol = sym;

  return sym;
}

static struct block *
process_dst_block (struct objfile *objfile, dst_rec_ptr_t entry)
{
  struct block *block;
  struct symbol *function = NULL;
  CORE_ADDR address;
  long size;
  char *name;
  dst_rec_ptr_t child_entry, symbol_entry;
  struct block *child_block;
  int total_symbols = 0;
  char fake_name[20];
  static long fake_seq = 0;
  struct symbol_list *symlist, *nextsym;
  int symnum;

  if (DST_block (entry).noffset)
    name = DST_OFFSET (entry, DST_block (entry).noffset);
  else
    name = NULL;
  if (DST_block (entry).n_of_code_ranges)
    {
      address = dst_sym_addr (
			       &DST_block (entry).code_ranges[0].code_start);
      size = DST_block (entry).code_ranges[0].code_size;
    }
  else
    {
      address = -1;
      size = 0;
    }
  symbol_entry = (dst_rec_ptr_t) get_sec_ref (&DST_block (entry).symbols_start);
  switch (DST_block (entry).block_type)
    {
      /* These are all really functions. Even the "program" type.
       * This is because the Apollo OS was written in Pascal, and
       * in Pascal, the main procedure is described as the Program.
       * Cute, huh?
       */
    case dst_block_procedure:
    case dst_block_function:
    case dst_block_subroutine:
    case dst_block_program:
      prim_record_minimal_symbol (name, address, mst_text, objfile);
      function = process_dst_function (
					objfile,
					symbol_entry,
					name,
					address);
      enter_all_lines (get_sec_ref (&DST_block (entry).code_ranges[0].lines_start), address);
      break;
    case dst_block_block_data:
      break;

    default:
      /* GDB has to call it something, and the module name
       * won't cut it
       */
      sprintf (fake_name, "block_%08lx", fake_seq++);
      function = process_dst_function (
					objfile, NULL, fake_name, address);
      break;
    }
  symlist = process_dst_symbols (objfile, symbol_entry,
				 name, &total_symbols);
  block = (struct block *)
    obstack_alloc (&objfile->symbol_obstack,
		   sizeof (struct block) +
		     (total_symbols - 1) * sizeof (struct symbol *));

  symnum = 0;
  while (symlist)
    {
      nextsym = symlist->next;

      block->sym[symnum] = symlist->symbol;

      xfree (symlist);
      symlist = nextsym;
      symnum++;
    }
  BLOCK_NSYMS (block) = total_symbols;
  BLOCK_START (block) = address;
  BLOCK_END (block) = address + size;
  BLOCK_SUPERBLOCK (block) = 0;
  if (function)
    {
      SYMBOL_BLOCK_VALUE (function) = block;
      BLOCK_FUNCTION (block) = function;
    }
  else
    BLOCK_FUNCTION (block) = 0;

  if (DST_block (entry).child_block_off)
    {
      child_entry = (dst_rec_ptr_t) DST_OFFSET (entry,
					 DST_block (entry).child_block_off);
      while (child_entry)
	{
	  child_block = process_dst_block (objfile, child_entry);
	  if (child_block)
	    {
	      if (BLOCK_START (child_block) <
		  BLOCK_START (block) ||
		  BLOCK_START (block) == -1)
		BLOCK_START (block) =
		  BLOCK_START (child_block);
	      if (BLOCK_END (child_block) >
		  BLOCK_END (block) ||
		  BLOCK_END (block) == -1)
		BLOCK_END (block) =
		  BLOCK_END (child_block);
	      BLOCK_SUPERBLOCK (child_block) = block;
	    }
	  if (DST_block (child_entry).sibling_block_off)
	    child_entry = (dst_rec_ptr_t) DST_OFFSET (
						       child_entry,
				 DST_block (child_entry).sibling_block_off);
	  else
	    child_entry = NULL;
	}
    }
  record_pending_block (objfile, block, NULL);
  return block;
}


static void
read_dst_symtab (struct objfile *objfile)
{
  char *buffer;
  dst_rec_ptr_t entry, file_table, root_block;
  char *source_file;
  struct block *block, *global_block;
  int symnum;
  struct symbol_list *nextsym;
  int module_num = 0;
  struct structure_list *element;

  current_objfile = objfile;
  buffer = blocks_info.buffer;
  while (NEXT_BLK (&buffer, &entry))
    {
      if (entry->rec_type == dst_typ_comp_unit)
	{
	  file_table = (dst_rec_ptr_t) DST_OFFSET (entry,
					  DST_comp_unit (entry).file_table);
	  section_table = (dst_rec_ptr_t) DST_OFFSET (entry,
				       DST_comp_unit (entry).section_table);
	  root_block = (dst_rec_ptr_t) DST_OFFSET (entry,
				   DST_comp_unit (entry).root_block_offset);
	  source_file = DST_OFFSET (file_table,
				DST_file_tab (file_table).files[0].noffset);
	  /* Point buffer to the start of the next comp_unit */
	  buffer = DST_OFFSET (entry,
			       DST_comp_unit (entry).data_size);
	  dst_start_symtab ();

	  block = process_dst_block (objfile, root_block);

	  global_block = (struct block *)
	    obstack_alloc (&objfile->symbol_obstack,
			   sizeof (struct block) +
			     (total_globals - 1) *
			   sizeof (struct symbol *));
	  BLOCK_NSYMS (global_block) = total_globals;
	  for (symnum = 0; symnum < total_globals; symnum++)
	    {
	      nextsym = dst_global_symbols->next;

	      global_block->sym[symnum] =
		dst_global_symbols->symbol;

	      xfree (dst_global_symbols);
	      dst_global_symbols = nextsym;
	    }
	  dst_global_symbols = NULL;
	  total_globals = 0;
	  BLOCK_FUNCTION (global_block) = 0;
	  BLOCK_START (global_block) = BLOCK_START (block);
	  BLOCK_END (global_block) = BLOCK_END (block);
	  BLOCK_SUPERBLOCK (global_block) = 0;
	  BLOCK_SUPERBLOCK (block) = global_block;
	  record_pending_block (objfile, global_block, NULL);

	  complete_symtab (source_file,
			   BLOCK_START (block),
			   BLOCK_END (block) - BLOCK_START (block));
	  module_num++;
	  dst_end_symtab (objfile);
	}
    }
  if (module_num)
    prim_record_minimal_symbol ("<end_of_program>",
				BLOCK_END (block), mst_text, objfile);
  /* One more faked symbol to make sure nothing can ever run off the
   * end of the symbol table. This one represents the end of the
   * text space. It used to be (CORE_ADDR) -1 (effectively the highest
   * int possible), but some parts of gdb treated it as a signed
   * number and failed comparisons. We could equally use 7fffffff,
   * but no functions are ever mapped to an address higher than
   * 40000000
   */
  prim_record_minimal_symbol ("<end_of_text>",
			      (CORE_ADDR) 0x40000000,
			      mst_text, objfile);
  while (struct_list)
    {
      element = struct_list;
      struct_list = element->next;
      xfree (element);
    }
}


/* Support for line number handling */
static char *linetab = NULL;
static long linetab_offset;
static unsigned long linetab_size;

/* Read in all the line numbers for fast lookups later.  Leave them in
   external (unswapped) format in memory; we'll swap them as we enter
   them into GDB's data structures.  */
static int
init_one_section (int chan, dst_sec *secinfo)
{
  if (secinfo->size == 0
      || lseek (chan, secinfo->position, 0) == -1
      || (secinfo->buffer = xmalloc (secinfo->size)) == NULL
      || myread (chan, secinfo->buffer, secinfo->size) == -1)
    return 0;
  else
    return 1;
}

static int
init_dst_sections (int chan)
{

  if (!init_one_section (chan, &blocks_info) ||
      !init_one_section (chan, &lines_info) ||
      !init_one_section (chan, &symbols_info))
    return -1;
  else
    return 0;
}

/* Fake up support for relocating symbol addresses.  FIXME.  */

struct section_offsets dst_symfile_faker =
{0};

void
dst_symfile_offsets (struct objfile *objfile, struct section_addr_info *addrs)
{
  objfile->num_sections = 1;
  objfile->section_offsets = &dst_symfile_faker;
}

/* Register our ability to parse symbols for DST BFD files */

static struct sym_fns dst_sym_fns =
{
  /* FIXME: Can this be integrated with coffread.c?  If not, should it be
     a separate flavour like ecoff?  */
  (enum bfd_flavour) -2,

  dst_new_init,			/* sym_new_init: init anything gbl to entire symtab */
  dst_symfile_init,		/* sym_init: read initial info, setup for sym_read() */
  dst_symfile_read,		/* sym_read: read a symbol file into symtab */
  dst_symfile_finish,		/* sym_finish: finished with file, cleanup */
  dst_symfile_offsets,		/* sym_offsets:  xlate external to internal form */
  NULL				/* next: pointer to next struct sym_fns */
};

void
_initialize_dstread (void)
{
  add_symtab_fns (&dst_sym_fns);
}
