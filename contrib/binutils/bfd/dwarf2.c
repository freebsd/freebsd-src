/* DWARF 2 support.
   Copyright 1994, 1995, 1996, 1997, 1998 Free Software Foundation, Inc.

   Adapted from gdb/dwarf2read.c by Gavin Koch of Cygnus Solutions
   (gavin@cygnus.com).

   From the dwarf2read.c header:
   Adapted by Gary Funck (gary@intrepid.com), Intrepid Technology,
   Inc.  with support from Florida State University (under contract
   with the Ada Joint Program Office), and Silicon Graphics, Inc.
   Initial contribution by Brent Benson, Harris Computer Systems, Inc.,
   based on Fred Fish's (Cygnus Support) implementation of DWARF 1
   support in dwarfread.c

This file is part of BFD.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libiberty.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/dwarf2.h"

/* The data in the .debug_line statement prologue looks like this.  */
struct line_head
  {
    unsigned int total_length;
    unsigned short version;
    unsigned int prologue_length;
    unsigned char minimum_instruction_length;
    unsigned char default_is_stmt;
    int line_base;
    unsigned char line_range;
    unsigned char opcode_base;
    unsigned char *standard_opcode_lengths;
  };

/* Attributes have a name and a value */
struct attribute
  {
    enum dwarf_attribute name;
    enum dwarf_form form;
    union
      {
	char *str;
	struct dwarf_block *blk;
	unsigned int unsnd;
	int snd;
	bfd_vma addr;
      }
    u;
  };

/* Get at parts of an attribute structure */

#define DW_STRING(attr)    ((attr)->u.str)
#define DW_UNSND(attr)     ((attr)->u.unsnd)
#define DW_BLOCK(attr)     ((attr)->u.blk)
#define DW_SND(attr)       ((attr)->u.snd)
#define DW_ADDR(attr)	   ((attr)->u.addr)

/* Blocks are a bunch of untyped bytes. */
struct dwarf_block
  {
    unsigned int size;
    char *data;
  };


struct dwarf2_debug {

  /* A list of all previously read comp_units. */
  struct comp_unit* all_comp_units;

  /* The next unread compilation unit within the .debug_info section.
     Zero indicates that the .debug_info section has not been loaded
     into a buffer yet.*/
  char* info_ptr;

  /* Pointer to the end of the .debug_info section memory buffer. */
  char* info_ptr_end;

  /* Pointer to the .debug_abbrev section loaded into memory. */
  char* dwarf_abbrev_buffer;

  /* Length of the loaded .debug_abbrev section. */
  unsigned long dwarf_abbrev_size;
};


/* VERBATUM 
   The following function up to the END VERBATUM mark are 
   copied directly from dwarf2read.c. */

/* read dwarf information from a buffer */

static unsigned int
read_1_byte (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_8 (abfd, (bfd_byte *) buf);
}

static int
read_1_signed_byte (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_signed_8 (abfd, (bfd_byte *) buf);
}

static unsigned int
read_2_bytes (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_16 (abfd, (bfd_byte *) buf);
}

#if 0

/* This is not used.  */

static int
read_2_signed_bytes (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_signed_16 (abfd, (bfd_byte *) buf);
}

#endif

static unsigned int
read_4_bytes (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_32 (abfd, (bfd_byte *) buf);
}

#if 0

/* This is not used.  */

static int
read_4_signed_bytes (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_signed_32 (abfd, (bfd_byte *) buf);
}

#endif

static unsigned int
read_8_bytes (abfd, buf)
     bfd *abfd;
     char *buf;
{
  return bfd_get_64 (abfd, (bfd_byte *) buf);
}

static char *
read_n_bytes (abfd, buf, size)
     bfd * abfd;
     char *buf;
     unsigned int size;
{
  /* If the size of a host char is 8 bits, we can return a pointer
     to the buffer, otherwise we have to copy the data to a buffer
     allocated on the temporary obstack.  */
  return buf;
}

static char *
read_string (abfd, buf, bytes_read_ptr)
     bfd *abfd;
     char *buf;
     unsigned int *bytes_read_ptr;
{
  /* If the size of a host char is 8 bits, we can return a pointer
     to the string, otherwise we have to copy the string to a buffer
     allocated on the temporary obstack.  */
  if (*buf == '\0')
    {
      *bytes_read_ptr = 1;
      return NULL;
    }
  *bytes_read_ptr = strlen (buf) + 1;
  return buf;
}

static unsigned int
read_unsigned_leb128 (abfd, buf, bytes_read_ptr)
     bfd *abfd;
     char *buf;
     unsigned int *bytes_read_ptr;
{
  unsigned int result, num_read;
  int i, shift;
  unsigned char byte;

  result = 0;
  shift = 0;
  num_read = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      num_read++;
      result |= ((byte & 127) << shift);
      if ((byte & 128) == 0)
	{
	  break;
	}
      shift += 7;
    }
  *bytes_read_ptr = num_read;
  return result;
}

static int
read_signed_leb128 (abfd, buf, bytes_read_ptr)
     bfd *abfd;
     char *buf;
     unsigned int *bytes_read_ptr;
{
  int result;
  int i, shift, size, num_read;
  unsigned char byte;

  result = 0;
  shift = 0;
  size = 32;
  num_read = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      num_read++;
      result |= ((byte & 127) << shift);
      shift += 7;
      if ((byte & 128) == 0)
	{
	  break;
	}
    }
  if ((shift < size) && (byte & 0x40))
    {
      result |= -(1 << shift);
    }
  *bytes_read_ptr = num_read;
  return result;
}

/* END VERBATUM */


static bfd_vma
read_address (abfd, buf)
     bfd *abfd;
     char *buf;
{
  bfd_vma retval = 0;

  if (sizeof(retval) == 4)
    {
      retval = bfd_get_32 (abfd, (bfd_byte *) buf);
    } else {
      retval = bfd_get_64 (abfd, (bfd_byte *) buf);
    }
  return retval;
}





/* This data structure holds the information of an abbrev. */
struct abbrev_info
  {
    unsigned int number;	/* number identifying abbrev */
    enum dwarf_tag tag;		/* dwarf tag */
    int has_children;		/* boolean */
    unsigned int num_attrs;	/* number of attributes */
    struct attr_abbrev *attrs;	/* an array of attribute descriptions */
    struct abbrev_info *next;	/* next in chain */
  };

struct attr_abbrev
  {
    enum dwarf_attribute name;
    enum dwarf_form form;
  };

#ifndef ABBREV_HASH_SIZE
#define ABBREV_HASH_SIZE 121
#endif
#ifndef ATTR_ALLOC_CHUNK
#define ATTR_ALLOC_CHUNK 4
#endif

/* Lookup an abbrev_info structure in the abbrev hash table.  */

static struct abbrev_info *
lookup_abbrev (number,abbrevs)
     unsigned int number;
     struct abbrev_info **abbrevs;
{
  unsigned int hash_number;
  struct abbrev_info *abbrev;

  hash_number = number % ABBREV_HASH_SIZE;
  abbrev = abbrevs[hash_number];

  while (abbrev)
    {
      if (abbrev->number == number)
	return abbrev;
      else
	abbrev = abbrev->next;
    }
  return NULL;
}

/* In DWARF version 2, the description of the debugging information is
   stored in a separate .debug_abbrev section.  Before we read any
   dies from a section we read in all abbreviations and install them
   in a hash table.  */

static struct abbrev_info**
read_abbrevs (abfd, offset)
     bfd * abfd;
     unsigned int offset;
{
  struct abbrev_info **abbrevs;
  char *abbrev_ptr;
  struct abbrev_info *cur_abbrev;
  unsigned int abbrev_number, bytes_read, abbrev_name;
  unsigned int abbrev_form, hash_number;
  struct dwarf2_debug *stash;

  stash = elf_tdata(abfd)->dwarf2_find_line_info;

  if (! stash->dwarf_abbrev_buffer)
    {
      asection *msec;

      msec = bfd_get_section_by_name (abfd, ".debug_abbrev");
      if (! msec)
	{
	  (*_bfd_error_handler) ("Dwarf Error: Can't find .debug_abbrev section.");
	  bfd_set_error (bfd_error_bad_value);
	  return 0;
	}
      
      stash->dwarf_abbrev_size = bfd_get_section_size_before_reloc (msec);
      stash->dwarf_abbrev_buffer = (unsigned char*) bfd_alloc (abfd, stash->dwarf_abbrev_size);
      if (! stash->dwarf_abbrev_buffer)
	  return 0;
      
      if (! bfd_get_section_contents (abfd, msec, 
				      stash->dwarf_abbrev_buffer, 0,
				      stash->dwarf_abbrev_size))
	return 0;
    }

  if (offset > stash->dwarf_abbrev_size)
    {
      (*_bfd_error_handler) ("Dwarf Error: Abbrev offset (%u) bigger than abbrev size (%u).", 
			     offset, stash->dwarf_abbrev_size );
      bfd_set_error (bfd_error_bad_value);
      return 0;
    }

  abbrevs = (struct abbrev_info**) bfd_zalloc (abfd, sizeof(struct abbrev_info*) * ABBREV_HASH_SIZE);

  abbrev_ptr = stash->dwarf_abbrev_buffer + offset;
  abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
  abbrev_ptr += bytes_read;

  /* loop until we reach an abbrev number of 0 */
  while (abbrev_number)
    {
      cur_abbrev = (struct abbrev_info*)bfd_zalloc (abfd, sizeof (struct abbrev_info));

      /* read in abbrev header */
      cur_abbrev->number = abbrev_number;
      cur_abbrev->tag = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      cur_abbrev->has_children = read_1_byte (abfd, abbrev_ptr);
      abbrev_ptr += 1;

      /* now read in declarations */
      abbrev_name = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      abbrev_form = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      while (abbrev_name)
	{
	  if ((cur_abbrev->num_attrs % ATTR_ALLOC_CHUNK) == 0)
	    {
	      cur_abbrev->attrs = (struct attr_abbrev *)
		bfd_realloc (cur_abbrev->attrs,
			     (cur_abbrev->num_attrs + ATTR_ALLOC_CHUNK)
			     * sizeof (struct attr_abbrev));
	      if (! cur_abbrev->attrs)
		return 0;
	    }
	  cur_abbrev->attrs[cur_abbrev->num_attrs].name = abbrev_name;
	  cur_abbrev->attrs[cur_abbrev->num_attrs++].form = abbrev_form;
	  abbrev_name = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	  abbrev_form = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	}

      hash_number = abbrev_number % ABBREV_HASH_SIZE;
      cur_abbrev->next = abbrevs[hash_number];
      abbrevs[hash_number] = cur_abbrev;

      /* Get next abbreviation.
         Under Irix6 the abbreviations for a compilation unit are not
	 always properly terminated with an abbrev number of 0.
	 Exit loop if we encounter an abbreviation which we have
	 already read (which means we are about to read the abbreviations
	 for the next compile unit) or if the end of the abbreviation
	 table is reached.  */
      if ((unsigned int) (abbrev_ptr - stash->dwarf_abbrev_buffer)
	    >= stash->dwarf_abbrev_size)
	break;
      abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      if (lookup_abbrev (abbrev_number,abbrevs) != NULL)
	break;
    }

  return abbrevs;
}

/* Read an attribute described by an abbreviated attribute.  */

static char *
read_attribute (attr, abbrev, abfd, info_ptr)
     struct attribute *attr;
     struct attr_abbrev *abbrev;
     bfd *abfd;
     char *info_ptr;
{
  unsigned int bytes_read;
  struct dwarf_block *blk;

  attr->name = abbrev->name;
  attr->form = abbrev->form;
  switch (abbrev->form)
    {
    case DW_FORM_addr:
    case DW_FORM_ref_addr:
      DW_ADDR (attr) = read_address (abfd, info_ptr);
      info_ptr += sizeof(bfd_vma);
      break;
    case DW_FORM_block2:
      blk = (struct dwarf_block *) bfd_alloc (abfd, sizeof (struct dwarf_block));
      blk->size = read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_block4:
      blk = (struct dwarf_block *) bfd_alloc (abfd, sizeof (struct dwarf_block));
      blk->size = read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_data2:
      DW_UNSND (attr) = read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      break;
    case DW_FORM_data4:
      DW_UNSND (attr) = read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      break;
    case DW_FORM_data8:
      DW_UNSND (attr) = read_8_bytes (abfd, info_ptr);
      info_ptr += 8;
      break;
    case DW_FORM_string:
      DW_STRING (attr) = read_string (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_block:
      blk = (struct dwarf_block *) bfd_alloc (abfd, sizeof (struct dwarf_block));
      blk->size = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_block1:
      blk = (struct dwarf_block *) bfd_alloc (abfd, sizeof (struct dwarf_block));
      blk->size = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_data1:
      DW_UNSND (attr) = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      break;
    case DW_FORM_flag:
      DW_UNSND (attr) = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      break;
    case DW_FORM_sdata:
      DW_SND (attr) = read_signed_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_udata:
      DW_UNSND (attr) = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_ref1:
      DW_UNSND (attr) = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      break;
    case DW_FORM_ref2:
      DW_UNSND (attr) = read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      break;
    case DW_FORM_ref4:
      DW_UNSND (attr) = read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      break;
    case DW_FORM_ref_udata:
      DW_UNSND (attr) = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_strp:
    case DW_FORM_indirect:
    default:
      (*_bfd_error_handler) ("Dwarf Error: Invalid or unhandled FORM value: %d.",
			     abbrev->form);
      bfd_set_error (bfd_error_bad_value);
    }
  return info_ptr;
}


/* Source line information table routines. */

#define FILE_ALLOC_CHUNK 5
#define DIR_ALLOC_CHUNK 5

struct line_info {
  struct line_info* prev_line;

  bfd_vma address;
  char* filename;
  unsigned int line;
  unsigned int column;
};

struct fileinfo {
  char *name;
  unsigned int dir;
  unsigned int time;
  unsigned int size;
};

struct line_info_table {
  bfd* abfd;

  unsigned int num_files;
  unsigned int num_dirs;

  char* comp_dir;
  char** dirs;
  struct fileinfo* files;
  struct line_info* last_line;
};

static void 
add_line_info (table, address, filename, line, column)
     struct line_info_table* table;
     bfd_vma address;
     char* filename;
     unsigned int line;
     unsigned int column;
{
  struct line_info* info = (struct line_info*)
    bfd_alloc (table->abfd, sizeof (struct line_info));

  info->prev_line = table->last_line;
  table->last_line = info;

  info->address = address;
  info->filename = filename;
  info->line = line;
  info->column = column;
}

static char* 
concat_filename (table, file)
     struct line_info_table* table;
     unsigned int file;
{
  char* filename = table->files[file - 1].name;
  if (*filename == '/')
    return filename;

  else
    {
      char* dirname = (table->files[file - 1].dir
		       ? table->dirs[table->files[file - 1].dir - 1]
		       : table->comp_dir);
      return (char*) concat (dirname, "/", filename, NULL);
    }
}

/* Decode the line number information for the compilation unit whose
   line number info is at OFFSET in the .debug_line section.
   The compilation directory of the file is passed in COMP_DIR.  */

static struct line_info_table*
decode_line_info (abfd, offset, comp_dir)
     bfd *abfd;
     unsigned int offset;
     char *comp_dir;
{
  static char* dwarf_line_buffer = 0;

  struct line_info_table* table;

  char *line_ptr;
  char *line_end;
  struct line_head lh;
  unsigned int i, bytes_read;
  char *cur_file, *cur_dir;
  unsigned char op_code, extended_op, adj_opcode;

  if (! dwarf_line_buffer)
    {
      asection *msec;
      unsigned long size;

      msec = bfd_get_section_by_name (abfd, ".debug_line");
      if (! msec)
	{
	  (*_bfd_error_handler) ("Dwarf Error: Can't find .debug_line section.");
	  bfd_set_error (bfd_error_bad_value);
	  return 0;
	}
      
      size = bfd_get_section_size_before_reloc (msec);
      dwarf_line_buffer = (unsigned char*) bfd_alloc (abfd, size);
      if (! dwarf_line_buffer)
	return 0;

      if (! bfd_get_section_contents (abfd, msec, 
				      dwarf_line_buffer, 0,
				      size))
	return 0;
    }

  table = (struct line_info_table*) bfd_alloc (abfd, 
					       sizeof (struct line_info_table));
  table->abfd = abfd;
  table->comp_dir = comp_dir;

  table->num_files = 0;
  table->files = NULL;

  table->num_dirs = 0;
  table->dirs = NULL;

  line_ptr = dwarf_line_buffer + offset;

  /* read in the prologue */
  lh.total_length = read_4_bytes (abfd, line_ptr);
  line_ptr += 4;
  line_end = line_ptr + lh.total_length;
  lh.version = read_2_bytes (abfd, line_ptr);
  line_ptr += 2;
  lh.prologue_length = read_4_bytes (abfd, line_ptr);
  line_ptr += 4;
  lh.minimum_instruction_length = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh.default_is_stmt = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh.line_base = read_1_signed_byte (abfd, line_ptr);
  line_ptr += 1;
  lh.line_range = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh.opcode_base = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh.standard_opcode_lengths = (unsigned char *)
    bfd_alloc (abfd, lh.opcode_base * sizeof (unsigned char));

  lh.standard_opcode_lengths[0] = 1;
  for (i = 1; i < lh.opcode_base; ++i)
    {
      lh.standard_opcode_lengths[i] = read_1_byte (abfd, line_ptr);
      line_ptr += 1;
    }

  /* Read directory table  */
  while ((cur_dir = read_string (abfd, line_ptr, &bytes_read)) != NULL)
    {
      line_ptr += bytes_read;
      if ((table->num_dirs % DIR_ALLOC_CHUNK) == 0)
	{
	  table->dirs = (char **)
	    bfd_realloc (table->dirs,
			 (table->num_dirs + DIR_ALLOC_CHUNK) * sizeof (char *));
	  if (! table->dirs)
	    return 0;
	}
      table->dirs[table->num_dirs++] = cur_dir;
    }
  line_ptr += bytes_read;

  /* Read file name table */
  while ((cur_file = read_string (abfd, line_ptr, &bytes_read)) != NULL)
    {
      line_ptr += bytes_read;
      if ((table->num_files % FILE_ALLOC_CHUNK) == 0)
	{
	  table->files = (struct fileinfo *)
	    bfd_realloc (table->files,
			 (table->num_files + FILE_ALLOC_CHUNK)
			 * sizeof (struct fileinfo));
	  if (! table->files)
	    return 0;
	}
      table->files[table->num_files].name = cur_file;
      table->files[table->num_files].dir =
	read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
      line_ptr += bytes_read;
      table->files[table->num_files].time =
	read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
      line_ptr += bytes_read;
      table->files[table->num_files].size =
	read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
      line_ptr += bytes_read;
      table->num_files++;
    }
  line_ptr += bytes_read;

  /* Read the statement sequences until there's nothing left.  */
  while (line_ptr < line_end)
    {
      /* state machine registers  */
      bfd_vma address = 0;
      char* filename = concat_filename (table, 1);
      unsigned int line = 1;
      unsigned int column = 0;
      int is_stmt = lh.default_is_stmt;
      int basic_block = 0;
      int end_sequence = 0;

      /* Decode the table. */
      while (! end_sequence)
	{
	  op_code = read_1_byte (abfd, line_ptr);
	  line_ptr += 1;
	  switch (op_code)
	    {
	    case DW_LNS_extended_op:
	      line_ptr += 1;	/* ignore length */
	      extended_op = read_1_byte (abfd, line_ptr);
	      line_ptr += 1;
	      switch (extended_op)
		{
		case DW_LNE_end_sequence:
		  end_sequence = 1;
		  add_line_info (table, address, filename, line, column);
		  break;
		case DW_LNE_set_address:
		  address = read_address (abfd, line_ptr);
		  address &= 0xffffffff;
		  line_ptr += sizeof (bfd_vma);
		  break;
		case DW_LNE_define_file:
		  cur_file = read_string (abfd, line_ptr, &bytes_read);
		  line_ptr += bytes_read;
		  if ((table->num_files % FILE_ALLOC_CHUNK) == 0)
		    {
		      table->files = (struct fileinfo *)
			bfd_realloc (table->files,
				     (table->num_files + FILE_ALLOC_CHUNK)
				     * sizeof (struct fileinfo));
		      if (! table->files)
			return 0;
		    }
		  table->files[table->num_files].name = cur_file;
		  table->files[table->num_files].dir =
		    read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
		  line_ptr += bytes_read;
		  table->files[table->num_files].time =
		    read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
		  line_ptr += bytes_read;
		  table->files[table->num_files].size =
		    read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
		  line_ptr += bytes_read;
		  table->num_files++;
		  break;
		default:
		  (*_bfd_error_handler) ("Dwarf Error: mangled line number section.");
		  bfd_set_error (bfd_error_bad_value);
		  return 0;
		}
	      break;
	    case DW_LNS_copy:
	      add_line_info (table, address, filename, line, column);
	      basic_block = 0;
	      break;
	    case DW_LNS_advance_pc:
	      address += lh.minimum_instruction_length
		* read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      break;
	    case DW_LNS_advance_line:
	      line += read_signed_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      break;
	    case DW_LNS_set_file:
	      {
		unsigned int file;

		/* The file and directory tables are 0 based, the references
		   are 1 based.  */
		file = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
		line_ptr += bytes_read;
		filename = concat_filename (table, file);
		break;
	      }
	    case DW_LNS_set_column:
	      column = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      break;
	    case DW_LNS_negate_stmt:
	      is_stmt = (!is_stmt);
	      break;
	    case DW_LNS_set_basic_block:
	      basic_block = 1;
	      break;
	    case DW_LNS_const_add_pc:
	      address += (255 - lh.opcode_base) / lh.line_range;
	      break;
	    case DW_LNS_fixed_advance_pc:
	      address += read_2_bytes (abfd, line_ptr);
	      line_ptr += 2;
	      break;
	    default:		/* special operand */
	      adj_opcode = op_code - lh.opcode_base;
	      address += (adj_opcode / lh.line_range)
		* lh.minimum_instruction_length;
	      line += lh.line_base + (adj_opcode % lh.line_range);
	      /* append row to matrix using current values */
	      add_line_info (table, address, filename, line, column);
	      basic_block = 1;
	    }
	}
    }

  return table;
}


/* If ADDR is within TABLE set the output parameters and return true,
   otherwise return false.  The output parameters, FILENAME_PTR and
   LINENUMBER_PTR, are pointers to the objects to be filled in. */

static boolean
lookup_address_in_line_info_table (table, 
				   addr,
				   filename_ptr, 
				   linenumber_ptr)
     struct line_info_table* table;
     bfd_vma addr;
     const char **filename_ptr;
     unsigned int *linenumber_ptr;
{
  struct line_info* each_line;
  struct line_info* next_line;
  
  for (next_line = 0, each_line = table->last_line;
       each_line;
       next_line = each_line, each_line = each_line->prev_line)
    {
      if (addr >= each_line->address
	  && (next_line == 0
	      || addr < next_line->address)) 
	{
	  *filename_ptr = each_line->filename;
	  *linenumber_ptr = each_line->line;
	  return true;
	}
    }
  
  return false;
}
  



/* Function table functions. */

struct funcinfo {
  struct funcinfo *prev_func;

  char* name;
  bfd_vma low;
  bfd_vma high;
};


/* If ADDR is within TABLE, set FUNCTIONNAME_PTR, and return true. */

static boolean
lookup_address_in_function_table (table, 
				  addr,
				  functionname_ptr)
     struct funcinfo* table;
     bfd_vma addr;
     const char **functionname_ptr;
{
  struct funcinfo* each_func;

  for (each_func = table;
       each_func;
       each_func = each_func->prev_func)
    {
      if (addr >= (each_func->low & 0xffffffff)
	  && addr < (each_func->high & 0xffffffff))
	{
	  *functionname_ptr = each_func->name;
	  return true;
	}
    }
  
  return false;
}




/* DWARF2 Compilation unit functions. */


/* A minimal decoding of DWARF2 compilation units.  We only decode
   what's needed to get to the line number information. */

struct comp_unit {

  /* Chain the previously read compilation units. */
  struct comp_unit* next_unit;

  /* Keep the bdf convenient (for memory allocation). */
  bfd* abfd;

  /* The lowest and higest addresses contained in this compilation
     unit as specified in the compilation unit header. */
  bfd_vma low;
  bfd_vma high;

  /* The DW_AT_name attribute (for error messages). */
  char* name;

  /* The abbrev hash table. */
  struct abbrev_info** abbrevs;

  /* Note that an error was found by comp_unit_find_nearest_line. */
  int error;

  /* The DW_AT_comp_dir attribute */
  char* comp_dir;

  /* True if there is a line number table associated with this comp. unit. */
  int stmtlist;
  
  /* The offset into .debug_line of the line number table. */
  unsigned long line_offset;

  /* Pointer to the first child die for the comp unit. */
  char *first_child_die_ptr;

  /* The end of the comp unit. */
  char *end_ptr;

  /* The decoded line number, NULL if not yet decoded. */
  struct line_info_table* line_table;

  /* A list of the functions found in this comp. unit. */
  struct funcinfo* function_table; 

};



/* Scan over each die in a comp. unit looking for functions to add
   to the function table. */

static boolean
scan_unit_for_functions (unit)
     struct comp_unit *unit;
{
  bfd *abfd = unit->abfd;
  char *info_ptr = unit->first_child_die_ptr;
  int nesting_level = 1;

  while (nesting_level)
    {
      unsigned int abbrev_number, bytes_read, i;
      struct abbrev_info *abbrev;
      struct attribute attr;
      struct funcinfo *func;
      char* name = 0;

      abbrev_number = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;

      if (! abbrev_number)
	{
	  nesting_level--;
	  continue;
	}
      
      abbrev = lookup_abbrev (abbrev_number,unit->abbrevs);
      if (! abbrev)
	{
	  (*_bfd_error_handler) ("Dwarf Error: Could not find abbrev number %d.", 
			     abbrev_number);
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      
      if (abbrev->tag == DW_TAG_subprogram)
	{
	  func = (struct funcinfo*) bfd_zalloc (abfd, sizeof (struct funcinfo));
	  func->prev_func = unit->function_table;
	  unit->function_table = func;
	}
      else
	func = NULL;
  
      for (i = 0; i < abbrev->num_attrs; ++i)
	{
	  info_ptr = read_attribute (&attr, &abbrev->attrs[i], abfd, info_ptr);
	  
	  if (func)
	    {
	      switch (attr.name)
		{
		case DW_AT_name:
		  
		  name = DW_STRING (&attr);

		  /* Prefer DW_AT_MIPS_linkage_name over DW_AT_name.  */
		  if (func->name == NULL)
		    func->name = DW_STRING (&attr);
		  break;
		  
		case DW_AT_MIPS_linkage_name:
		  func->name = DW_STRING (&attr);
		  break;

		case DW_AT_low_pc:
		  func->low = DW_ADDR (&attr);
		  break;

		case DW_AT_high_pc:
		  func->high = DW_ADDR (&attr);
		  break;

		default:
		  break;
		}
	    }
	  else
	    {
	      switch (attr.name)
		{
		case DW_AT_name:
		  name = DW_STRING (&attr);
		  break;
		  
		default:
		  break;
		}
	    }
	}

      if (abbrev->has_children)
	nesting_level++;
    }

  return true;
}






/* Parse a DWARF2 compilation unit starting at INFO_PTR.  This includes
   the compilation unit header that proceeds the DIE's, but does not
   include the length field that preceeds each compilation unit header.
   END_PTR points one past the end of this comp unit.

   This routine does not read the whole compilation unit; only enough
   to get to the line number information for the compilation unit.
   */

static struct comp_unit*
parse_comp_unit (abfd, info_ptr, end_ptr)
     bfd* abfd;
     char* info_ptr;
     char* end_ptr;
{
  struct comp_unit* unit;

  unsigned short version;
  unsigned int abbrev_offset;
  unsigned char addr_size;
  struct abbrev_info** abbrevs;

  unsigned int abbrev_number, bytes_read, i;
  struct abbrev_info *abbrev;
  struct attribute attr;

  version = read_2_bytes (abfd, info_ptr);
  info_ptr += 2;
  abbrev_offset = read_4_bytes (abfd, info_ptr);
  info_ptr += 4;
  addr_size = read_1_byte (abfd, info_ptr);
  info_ptr += 1;

  if (version != 2)
    {
      (*_bfd_error_handler) ("Dwarf Error: found dwarf version '%hu' in compilation unit '%s', this reader only handles version 2 information.", 
			 version,
			 unit->name);
      bfd_set_error (bfd_error_bad_value);
      return 0;
    }

  if (addr_size != sizeof (bfd_vma))
    {
      (*_bfd_error_handler) ("Dwarf Error: found address size '%u' in compilation unit '%s', this readers only handles address size '%u'.",
			 addr_size,
			 unit->name,
			 sizeof (bfd_vma));
      bfd_set_error (bfd_error_bad_value);
      return 0;
    }

  /* Read the abbrevs for this compilation unit into a table */
  abbrevs = read_abbrevs (abfd, abbrev_offset);
  if (! abbrevs)
      return 0;

  abbrev_number = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
  info_ptr += bytes_read;
  if (! abbrev_number)
    {
      (*_bfd_error_handler) ("Dwarf Error: Bad abbrev number: %d.",
			 abbrev_number);
      bfd_set_error (bfd_error_bad_value);
      return 0;
    }

  abbrev = lookup_abbrev (abbrev_number, abbrevs);
  if (! abbrev)
    {
      (*_bfd_error_handler) ("Dwarf Error: Could not find abbrev number %d.",
			 abbrev_number);
      bfd_set_error (bfd_error_bad_value);
      return 0;
    }
  
  unit = (struct comp_unit*) bfd_zalloc (abfd, sizeof (struct comp_unit));
  unit->abfd = abfd;
  unit->abbrevs = abbrevs;
  unit->end_ptr = end_ptr;

  for (i = 0; i < abbrev->num_attrs; ++i)
    {
      info_ptr = read_attribute (&attr, &abbrev->attrs[i], abfd, info_ptr);

      /* Store the data if it is of an attribute we want to keep in a
	 partial symbol table.  */
      switch (attr.name)
	{
	case DW_AT_stmt_list:
	  unit->stmtlist = 1;
	  unit->line_offset = DW_UNSND (&attr);
	  break;

	case DW_AT_name:
	  unit->name = DW_STRING (&attr);
	  break;

	case DW_AT_low_pc:
	  unit->low = DW_ADDR (&attr);
	  break;

	case DW_AT_high_pc:
	  unit->high = DW_ADDR (&attr);
	  break;

	case DW_AT_comp_dir:
	  {
	    char* comp_dir = DW_STRING (&attr);
	    if (comp_dir)
	      {
		/* Irix 6.2 native cc prepends <machine>.: to the compilation
		   directory, get rid of it.  */
		char *cp = (char*) strchr (comp_dir, ':');

		if (cp && cp != comp_dir && cp[-1] == '.' && cp[1] == '/')
		  comp_dir = cp + 1;
	      }
	    unit->comp_dir = comp_dir;
	    break;
	  }

	default:
	  break;
	}
    }

  unit->first_child_die_ptr = info_ptr;
  return unit;
}





/* Return true if UNIT contains the address given by ADDR. */

static boolean
comp_unit_contains_address (unit, addr)
     struct comp_unit* unit;
     bfd_vma addr;
{
  return ! unit->error
    && ( addr >= (unit->low & 0xffffffff)
	&& addr <= (unit->high & 0xffffffff));
}


/* If UNIT contains ADDR, set the output parameters to the values for
   the line containing ADDR.  The output parameters, FILENAME_PTR,
   FUNCTIONNAME_PTR, and LINENUMBER_PTR, are pointers to the objects
   to be filled in.  

   Return true of UNIT contains ADDR, and no errors were encountered;
   false otherwise.  */

static boolean
comp_unit_find_nearest_line (unit, addr,
			     filename_ptr, functionname_ptr, linenumber_ptr)
     struct comp_unit* unit;
     bfd_vma addr;
     const char **filename_ptr;
     const char **functionname_ptr;
     unsigned int *linenumber_ptr;
{
  boolean line_p;
  boolean func_p;
  
  if (unit->error)
    return false;

  if (! unit->line_table)
    {
      if (! unit->stmtlist)
	{
	  unit->error = 1;
	  return false;
	}
  
      unit->line_table = decode_line_info (unit->abfd,
					   unit->line_offset, 
					   unit->comp_dir);

      if (! unit->line_table)
	{
	  unit->error = 1;
	  return false;
	}
      
      if (! scan_unit_for_functions (unit))
	{
	  unit->error = 1;
	  return false;
	}
    }

  line_p = lookup_address_in_line_info_table (unit->line_table,
					      addr,
					      filename_ptr, 
					      linenumber_ptr);
  func_p = lookup_address_in_function_table (unit->function_table, 
					     addr,
					     functionname_ptr);
  return line_p || func_p;
}

/* The DWARF2 version of find_nearest line.
   Return true if the line is found without error. */

boolean
_bfd_dwarf2_find_nearest_line (abfd, section, symbols, offset,
			  filename_ptr, functionname_ptr, linenumber_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     const char **filename_ptr;
     const char **functionname_ptr;
     unsigned int *linenumber_ptr;
{
  /* Read each compilation unit from the section .debug_info, and check
     to see if it contains the address we are searching for.  If yes,
     lookup the address, and return the line number info.  If no, go
     on to the next compilation unit.  

     We keep a list of all the previously read compilation units, and
     a pointer to the next un-read compilation unit.  Check the 
     previously read units before reading more.
     */

  struct dwarf2_debug *stash = elf_tdata (abfd)->dwarf2_find_line_info;

  /* What address are we looking for? */
  bfd_vma addr = offset + section->vma;

  struct comp_unit* each;
  
  *filename_ptr = NULL;
  *functionname_ptr = NULL;
  *linenumber_ptr = 0;

  if (! stash)
    {
      asection *msec;
      unsigned long size;
      
      stash = elf_tdata (abfd)->dwarf2_find_line_info =
	(struct dwarf2_debug*) bfd_zalloc (abfd, sizeof (struct dwarf2_debug));
      
      if (! stash)
	return false;
      
      msec = bfd_get_section_by_name (abfd, ".debug_info");
      if (! msec)
	{
	  /* No dwarf2 info.  Note that at this point the stash
	     has been allocated, but contains zeros, this lets
	     future calls to this function fail quicker. */
	  return false;
	}

      size = bfd_get_section_size_before_reloc (msec);
      stash->info_ptr = (unsigned char*) bfd_alloc (abfd, size);
      
      if (! stash->info_ptr)
	return false;

      if (! bfd_get_section_contents (abfd, msec, stash->info_ptr, 0, size))
	{
	  stash->info_ptr = 0;
	  return false;
	}

      stash->info_ptr_end = stash->info_ptr + size;
    }

  
  /* A null info_ptr indicates that there is no dwarf2 info 
     (or that an error occured while setting up the stash). */

  if (! stash->info_ptr)
    return false;



  /* Check the previously read comp. units first. */

  for (each = stash->all_comp_units; each; each = each->next_unit)
    {
      if (comp_unit_contains_address (each, addr))
	return comp_unit_find_nearest_line (each, addr,
					    filename_ptr, 
					    functionname_ptr, 
					    linenumber_ptr);
    }


  /* Read each remaining comp. units checking each as they are read. */
  while (stash->info_ptr < stash->info_ptr_end)
    {
      struct comp_unit* each;
      unsigned int length;

      length = read_4_bytes (abfd, stash->info_ptr);
      stash->info_ptr += 4;

      if (length > 0)
        {
	  each = parse_comp_unit (abfd, stash->info_ptr, 
				  stash->info_ptr + length);
	  stash->info_ptr += length;

	  if (each)
	    {
	      each->next_unit = stash->all_comp_units;
	      stash->all_comp_units = each;
	      
	      if (comp_unit_contains_address (each, addr))
		return comp_unit_find_nearest_line (each, addr,
						    filename_ptr, 
						    functionname_ptr, 
						    linenumber_ptr);
	    }
	}
    }

  return false;
}

/* end of file */
