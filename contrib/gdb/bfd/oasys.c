/* BFD back-end for oasys objects.
   Copyright 1990, 91, 92, 93, 94, 95, 1996 Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support, <sac@cygnus.com>.

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

#define UNDERSCORE_HACK 1
#include "bfd.h"
#include "sysdep.h"
#include <ctype.h>
#include "libbfd.h"
#include "oasys.h"
#include "liboasys.h"

/* XXX - FIXME.  offsetof belongs in the system-specific files in
   ../include/sys. */
/* Define offsetof for those systems which lack it */

#ifndef offsetof
#define offsetof(type, identifier) (size_t) &(((type *) 0)->identifier)
#endif

static boolean oasys_read_record PARAMS ((bfd *,
					  oasys_record_union_type *));
static boolean oasys_write_sections PARAMS ((bfd *));
static boolean oasys_write_record PARAMS ((bfd *,
					   oasys_record_enum_type,
					   oasys_record_union_type *,
					   size_t));
static boolean oasys_write_syms PARAMS ((bfd *));
static boolean oasys_write_header PARAMS ((bfd *));
static boolean oasys_write_end PARAMS ((bfd *));
static boolean oasys_write_data PARAMS ((bfd *));

/* Read in all the section data and relocation stuff too */
PROTO (static boolean, oasys_slurp_section_data, (bfd * CONST abfd));

static boolean
oasys_read_record (abfd, record)
     bfd *abfd;
     oasys_record_union_type *record;
{
  if (bfd_read ((PTR) record, 1, sizeof (record->header), abfd)
      != sizeof (record->header))
    return false;

  if ((size_t) record->header.length <= (size_t) sizeof (record->header))
    return true;
  if (bfd_read ((PTR) (((char *) record) + sizeof (record->header)),
		1, record->header.length - sizeof (record->header),
		abfd)
      != record->header.length - sizeof (record->header))
    return false;
  return true;
}
static size_t
oasys_string_length (record)
     oasys_record_union_type *record;
{
  return record->header.length
    - ((char *) record->symbol.name - (char *) record);
}

/*****************************************************************************/

/*

Slurp the symbol table by reading in all the records at the start file
till we get to the first section record.

We'll sort the symbolss into  two lists, defined and undefined. The
undefined symbols will be placed into the table according to their
refno.

We do this by placing all undefined symbols at the front of the table
moving in, and the defined symbols at the end of the table moving back.

*/

static boolean
oasys_slurp_symbol_table (abfd)
     bfd *CONST abfd;
{
  oasys_record_union_type record;
  oasys_data_type *data = OASYS_DATA (abfd);
  boolean loop = true;
  asymbol *dest_defined;
  asymbol *dest;
  char *string_ptr;


  if (data->symbols != (asymbol *) NULL)
    {
      return true;
    }
  /* Buy enough memory for all the symbols and all the names */
  data->symbols =
    (asymbol *) bfd_alloc (abfd, sizeof (asymbol) * abfd->symcount);
#ifdef UNDERSCORE_HACK
  /* buy 1 more char for each symbol to keep the underscore in*/
  data->strings = bfd_alloc (abfd, data->symbol_string_length +
			     abfd->symcount);
#else
  data->strings = bfd_alloc (abfd, data->symbol_string_length);
#endif
  if (!data->symbols || !data->strings)
    return false;

  dest_defined = data->symbols + abfd->symcount - 1;

  string_ptr = data->strings;
  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    return false;
  while (loop)
    {

      if (! oasys_read_record (abfd, &record))
	return false;
      switch (record.header.type)
	{
	case oasys_record_is_header_enum:
	  break;
	case oasys_record_is_local_enum:
	case oasys_record_is_symbol_enum:
	  {
	    int flag = record.header.type == (int) oasys_record_is_local_enum ?
	    (BSF_LOCAL) : (BSF_GLOBAL | BSF_EXPORT);


	    size_t length = oasys_string_length (&record);
	    switch (record.symbol.relb & RELOCATION_TYPE_BITS)
	      {
	      case RELOCATION_TYPE_ABS:
		dest = dest_defined--;
		dest->section = bfd_abs_section_ptr;
		dest->flags = 0;

		break;
	      case RELOCATION_TYPE_REL:
		dest = dest_defined--;
		dest->section =
		  OASYS_DATA (abfd)->sections[record.symbol.relb &
					      RELOCATION_SECT_BITS];
		if (record.header.type == (int) oasys_record_is_local_enum)
		  {
		    dest->flags = BSF_LOCAL;
		    if (dest->section == (asection *) (~0))
		      {
			/* It seems that sometimes internal symbols are tied up, but
		       still get output, even though there is no
		       section */
			dest->section = 0;
		      }
		  }
		else
		  {

		    dest->flags = flag;
		  }
		break;
	      case RELOCATION_TYPE_UND:
		dest = data->symbols + bfd_h_get_16 (abfd, record.symbol.refno);
		dest->section = bfd_und_section_ptr;
		break;
	      case RELOCATION_TYPE_COM:
		dest = dest_defined--;
		dest->name = string_ptr;
		dest->the_bfd = abfd;

		dest->section = bfd_com_section_ptr;

		break;
	      default:
		dest = dest_defined--;
		BFD_ASSERT (0);
		break;
	      }
	    dest->name = string_ptr;
	    dest->the_bfd = abfd;
	    dest->udata.p = (PTR) NULL;
	    dest->value = bfd_h_get_32 (abfd, record.symbol.value);

#ifdef UNDERSCORE_HACK
	    if (record.symbol.name[0] != '_')
	      {
		string_ptr[0] = '_';
		string_ptr++;
	      }
#endif
	    memcpy (string_ptr, record.symbol.name, length);


	    string_ptr[length] = 0;
	    string_ptr += length + 1;
	  }
	  break;
	default:
	  loop = false;
	}
    }
  return true;
}

static long
oasys_get_symtab_upper_bound (abfd)
     bfd *CONST abfd;
{
  if (! oasys_slurp_symbol_table (abfd))
    return -1;

  return (abfd->symcount + 1) * (sizeof (oasys_symbol_type *));
}

/*
*/

extern const bfd_target oasys_vec;

long
oasys_get_symtab (abfd, location)
     bfd *abfd;
     asymbol **location;
{
  asymbol *symbase;
  unsigned int counter;
  if (oasys_slurp_symbol_table (abfd) == false)
    {
      return -1;
    }
  symbase = OASYS_DATA (abfd)->symbols;
  for (counter = 0; counter < abfd->symcount; counter++)
    {
      *(location++) = symbase++;
    }
  *location = 0;
  return abfd->symcount;
}

/***********************************************************************
*  archive stuff
*/

static const bfd_target *
oasys_archive_p (abfd)
     bfd *abfd;
{
  oasys_archive_header_type header;
  oasys_extarchive_header_type header_ext;
  unsigned int i;
  file_ptr filepos;

  if (bfd_seek (abfd, (file_ptr) 0, false) != 0
      || (bfd_read ((PTR) & header_ext, 1, sizeof (header_ext), abfd)
	  != sizeof (header_ext)))
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  header.version = bfd_h_get_32 (abfd, header_ext.version);
  header.mod_count = bfd_h_get_32 (abfd, header_ext.mod_count);
  header.mod_tbl_offset = bfd_h_get_32 (abfd, header_ext.mod_tbl_offset);
  header.sym_tbl_size = bfd_h_get_32 (abfd, header_ext.sym_tbl_size);
  header.sym_count = bfd_h_get_32 (abfd, header_ext.sym_count);
  header.sym_tbl_offset = bfd_h_get_32 (abfd, header_ext.sym_tbl_offset);
  header.xref_count = bfd_h_get_32 (abfd, header_ext.xref_count);
  header.xref_lst_offset = bfd_h_get_32 (abfd, header_ext.xref_lst_offset);

  /*
    There isn't a magic number in an Oasys archive, so the best we
    can do to verify reasnableness is to make sure that the values in
    the header are too weird
    */

  if (header.version > 10000 ||
      header.mod_count > 10000 ||
      header.sym_count > 100000 ||
      header.xref_count > 100000)
    return (const bfd_target *) NULL;

  /*
    That all worked, let's buy the space for the header and read in
    the headers.
    */
  {
    oasys_ar_data_type *ar =
    (oasys_ar_data_type *) bfd_alloc (abfd, sizeof (oasys_ar_data_type));

    oasys_module_info_type *module =
    (oasys_module_info_type *)
    bfd_alloc (abfd, sizeof (oasys_module_info_type) * header.mod_count);
    oasys_module_table_type record;

    if (!ar || !module)
      return NULL;

    abfd->tdata.oasys_ar_data = ar;
    ar->module = module;
    ar->module_count = header.mod_count;

    filepos = header.mod_tbl_offset;
    for (i = 0; i < header.mod_count; i++)
      {
	if (bfd_seek (abfd, filepos, SEEK_SET) != 0)
	  return NULL;

	/* There are two ways of specifying the archive header */

	if (0)
	  {
	    oasys_extmodule_table_type_a_type record_ext;
	    if (bfd_read ((PTR) & record_ext, 1, sizeof (record_ext), abfd)
		!= sizeof (record_ext))
	      return NULL;

	    record.mod_size = bfd_h_get_32 (abfd, record_ext.mod_size);
	    record.file_offset = bfd_h_get_32 (abfd, record_ext.file_offset);

	    record.dep_count = bfd_h_get_32 (abfd, record_ext.dep_count);
	    record.depee_count = bfd_h_get_32 (abfd, record_ext.depee_count);
	    record.sect_count = bfd_h_get_32 (abfd, record_ext.sect_count);

	    module[i].name = bfd_alloc (abfd, 33);
	    if (!module[i].name)
	      return NULL;

	    memcpy (module[i].name, record_ext.mod_name, 33);
	    filepos +=
	      sizeof (record_ext) +
	      record.dep_count * 4 +
	      record.depee_count * 4 +
	      record.sect_count * 8 + 187;
	  }
	else
	  {
	    oasys_extmodule_table_type_b_type record_ext;
	    if (bfd_read ((PTR) & record_ext, 1, sizeof (record_ext), abfd)
		!= sizeof (record_ext))
	      return NULL;

	    record.mod_size = bfd_h_get_32 (abfd, record_ext.mod_size);
	    record.file_offset = bfd_h_get_32 (abfd, record_ext.file_offset);

	    record.dep_count = bfd_h_get_32 (abfd, record_ext.dep_count);
	    record.depee_count = bfd_h_get_32 (abfd, record_ext.depee_count);
	    record.sect_count = bfd_h_get_32 (abfd, record_ext.sect_count);
	    record.module_name_size = bfd_h_get_32 (abfd, record_ext.mod_name_length);

	    module[i].name = bfd_alloc (abfd, record.module_name_size + 1);
	    if (!module[i].name)
	      return NULL;
	    if (bfd_read ((PTR) module[i].name, 1, record.module_name_size,
			  abfd)
		!= record.module_name_size)
	      return NULL;
	    module[i].name[record.module_name_size] = 0;
	    filepos +=
	      sizeof (record_ext) +
	      record.dep_count * 4 +
	      record.module_name_size + 1;

	  }


	module[i].size = record.mod_size;
	module[i].pos = record.file_offset;
	module[i].abfd = 0;
      }

  }
  return abfd->xvec;
}

static boolean
oasys_mkobject (abfd)
     bfd *abfd;
{

  abfd->tdata.oasys_obj_data = (oasys_data_type *) bfd_alloc (abfd, sizeof (oasys_data_type));
  return abfd->tdata.oasys_obj_data ? true : false;
}

#define MAX_SECS 16
static const bfd_target *
oasys_object_p (abfd)
     bfd *abfd;
{
  oasys_data_type *oasys;
  oasys_data_type *save = OASYS_DATA (abfd);
  boolean loop = true;
  boolean had_usefull = false;

  abfd->tdata.oasys_obj_data = 0;
  oasys_mkobject (abfd);
  oasys = OASYS_DATA (abfd);
  memset ((PTR) oasys->sections, 0xff, sizeof (oasys->sections));

  /* Point to the start of the file */
  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    goto fail;
  oasys->symbol_string_length = 0;
  /* Inspect the records, but only keep the section info -
     remember the size of the symbols
     */
  oasys->first_data_record = 0;
  while (loop)
    {
      oasys_record_union_type record;
      if (! oasys_read_record (abfd, &record))
	goto fail;
      if ((size_t) record.header.length < (size_t) sizeof (record.header))
	goto fail;


      switch ((oasys_record_enum_type) (record.header.type))
	{
	case oasys_record_is_header_enum:
	  had_usefull = true;
	  break;
	case oasys_record_is_symbol_enum:
	case oasys_record_is_local_enum:
	  /* Count symbols and remember their size for a future malloc   */
	  abfd->symcount++;
	  oasys->symbol_string_length += 1 + oasys_string_length (&record);
	  had_usefull = true;
	  break;
	case oasys_record_is_section_enum:
	  {
	    asection *s;
	    char *buffer;
	    unsigned int section_number;
	    if (record.section.header.length != sizeof (record.section))
	      {
		goto fail;
	      }
	    buffer = bfd_alloc (abfd, 3);
	    if (!buffer)
	      goto fail;
	    section_number = record.section.relb & RELOCATION_SECT_BITS;
	    sprintf (buffer, "%u", section_number);
	    s = bfd_make_section (abfd, buffer);
	    oasys->sections[section_number] = s;
	    switch (record.section.relb & RELOCATION_TYPE_BITS)
	      {
	      case RELOCATION_TYPE_ABS:
	      case RELOCATION_TYPE_REL:
		break;
	      case RELOCATION_TYPE_UND:
	      case RELOCATION_TYPE_COM:
		BFD_FAIL ();
	      }

	    s->_raw_size = bfd_h_get_32 (abfd, record.section.value);
	    s->vma = bfd_h_get_32 (abfd, record.section.vma);
	    s->flags = 0;
	    had_usefull = true;
	  }
	  break;
	case oasys_record_is_data_enum:
	  oasys->first_data_record = bfd_tell (abfd) - record.header.length;
	case oasys_record_is_debug_enum:
	case oasys_record_is_module_enum:
	case oasys_record_is_named_section_enum:
	case oasys_record_is_end_enum:
	  if (had_usefull == false)
	    goto fail;
	  loop = false;
	  break;
	default:
	  goto fail;
	}
    }
  oasys->symbols = (asymbol *) NULL;
  /*
    Oasys support several architectures, but I can't see a simple way
    to discover which one is in a particular file - we'll guess
    */
  bfd_default_set_arch_mach (abfd, bfd_arch_m68k, 0);
  if (abfd->symcount != 0)
    {
      abfd->flags |= HAS_SYMS;
    }

  /*
    We don't know if a section has data until we've read it..
    */

  oasys_slurp_section_data (abfd);


  return abfd->xvec;

fail:
  (void) bfd_release (abfd, oasys);
  abfd->tdata.oasys_obj_data = save;
  return (const bfd_target *) NULL;
}


static void
oasys_get_symbol_info (ignore_abfd, symbol, ret)
     bfd *ignore_abfd;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
  if (!symbol->section)
    ret->type = (symbol->flags & BSF_LOCAL) ? 'a' : 'A';
}

static void
oasys_print_symbol (ignore_abfd, afile, symbol, how)
     bfd *ignore_abfd;
     PTR afile;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *) afile;

  switch (how)
    {
    case bfd_print_symbol_name:
    case bfd_print_symbol_more:
      fprintf (file, "%s", symbol->name);
      break;
    case bfd_print_symbol_all:
      {
	CONST char *section_name = symbol->section == (asection *) NULL ?
	(CONST char *) "*abs" : symbol->section->name;

	bfd_print_symbol_vandf ((PTR) file, symbol);

	fprintf (file, " %-5s %s",
		 section_name,
		 symbol->name);
      }
      break;
    }
}
/*
 The howto table is build using the top two bits of a reloc byte to
 index into it. The bits are PCREL,WORD/LONG
*/
static reloc_howto_type howto_table[] =
{

  HOWTO (0, 0, 1, 16, false, 0, complain_overflow_bitfield, 0, "abs16", true, 0x0000ffff, 0x0000ffff, false),
  HOWTO (0, 0, 2, 32, false, 0, complain_overflow_bitfield, 0, "abs32", true, 0xffffffff, 0xffffffff, false),
  HOWTO (0, 0, 1, 16, true, 0, complain_overflow_signed, 0, "pcrel16", true, 0x0000ffff, 0x0000ffff, false),
  HOWTO (0, 0, 2, 32, true, 0, complain_overflow_signed, 0, "pcrel32", true, 0xffffffff, 0xffffffff, false)
};

/* Read in all the section data and relocation stuff too */
static boolean
oasys_slurp_section_data (abfd)
     bfd *CONST abfd;
{
  oasys_record_union_type record;
  oasys_data_type *data = OASYS_DATA (abfd);
  boolean loop = true;

  oasys_per_section_type *per;

  asection *s;

  /* See if the data has been slurped already .. */
  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      per = oasys_per_section (s);
      if (per->initialized == true)
	return true;
    }

  if (data->first_data_record == 0)
    return true;

  if (bfd_seek (abfd, data->first_data_record, SEEK_SET) != 0)
    return false;
  while (loop)
    {
      if (! oasys_read_record (abfd, &record))
	return false;
      switch (record.header.type)
	{
	case oasys_record_is_header_enum:
	  break;
	case oasys_record_is_data_enum:
	  {

	    bfd_byte *src = record.data.data;
	    bfd_byte *end_src = ((bfd_byte *) & record) + record.header.length;
	    bfd_byte *dst_ptr;
	    bfd_byte *dst_base_ptr;
	    unsigned int relbit;
	    unsigned int count;
	    asection *section =
	    data->sections[record.data.relb & RELOCATION_SECT_BITS];
	    bfd_vma dst_offset;

	    per = oasys_per_section (section);

	    if (per->initialized == false)
	      {
		per->data = (bfd_byte *) bfd_zalloc (abfd, section->_raw_size);
		if (!per->data)
		  return false;
		per->reloc_tail_ptr = (oasys_reloc_type **) & (section->relocation);
		per->had_vma = false;
		per->initialized = true;
		section->reloc_count = 0;
		section->flags = SEC_ALLOC;
	      }

	    dst_offset = bfd_h_get_32 (abfd, record.data.addr);
	    if (per->had_vma == false)
	      {
		/* Take the first vma we see as the base */
		section->vma = dst_offset;
		per->had_vma = true;
	      }

	    dst_offset -= section->vma;

	    dst_base_ptr = oasys_per_section (section)->data;
	    dst_ptr = oasys_per_section (section)->data +
	      dst_offset;

	    if (src < end_src)
	      {
		section->flags |= SEC_LOAD | SEC_HAS_CONTENTS;
	      }
	    while (src < end_src)
	      {
		unsigned char mod_byte = *src++;
		size_t gap = end_src - src;

		count = 8;
		if (mod_byte == 0 && gap >= 8)
		  {
		    dst_ptr[0] = src[0];
		    dst_ptr[1] = src[1];
		    dst_ptr[2] = src[2];
		    dst_ptr[3] = src[3];
		    dst_ptr[4] = src[4];
		    dst_ptr[5] = src[5];
		    dst_ptr[6] = src[6];
		    dst_ptr[7] = src[7];
		    dst_ptr += 8;
		    src += 8;
		  }
		else
		  {
		    for (relbit = 1; count-- != 0 && src < end_src; relbit <<= 1)
		      {
			if (relbit & mod_byte)
			  {
			    unsigned char reloc = *src;
			    /* This item needs to be relocated */
			    switch (reloc & RELOCATION_TYPE_BITS)
			      {
			      case RELOCATION_TYPE_ABS:

				break;

			      case RELOCATION_TYPE_REL:
				{
				  /* Relocate the item relative to the section */
				  oasys_reloc_type *r =
				  (oasys_reloc_type *)
				  bfd_alloc (abfd,
					     sizeof (oasys_reloc_type));
				  if (!r)
				    return false;
				  *(per->reloc_tail_ptr) = r;
				  per->reloc_tail_ptr = &r->next;
				  r->next = (oasys_reloc_type *) NULL;
				  /* Reference to undefined symbol */
				  src++;
				  /* There is no symbol */
				  r->symbol = 0;
				  /* Work out the howto */
				  abort ();
#if 0
				  r->relent.section =
				    data->sections[reloc &
						   RELOCATION_SECT_BITS];

				  r->relent.addend = -
				    r->relent.section->vma;
#endif
				  r->relent.address = dst_ptr - dst_base_ptr;
				  r->relent.howto = &howto_table[reloc >> 6];
				  r->relent.sym_ptr_ptr = (asymbol **) NULL;
				  section->reloc_count++;

				  /* Fake up the data to look like it's got the -ve pc in it, this makes
				       it much easier to convert into other formats. This is done by
				       hitting the addend.
				       */
				  if (r->relent.howto->pc_relative == true)
				    {
				      r->relent.addend -= dst_ptr - dst_base_ptr;
				    }


				}
				break;


			      case RELOCATION_TYPE_UND:
				{
				  oasys_reloc_type *r =
				  (oasys_reloc_type *)
				  bfd_alloc (abfd,
					     sizeof (oasys_reloc_type));
				  if (!r)
				    return false;
				  *(per->reloc_tail_ptr) = r;
				  per->reloc_tail_ptr = &r->next;
				  r->next = (oasys_reloc_type *) NULL;
				  /* Reference to undefined symbol */
				  src++;
				  /* Get symbol number */
				  r->symbol = (src[0] << 8) | src[1];
				  /* Work out the howto */
				  abort ();

#if 0
				  r->relent.section = (asection
						       *) NULL;
#endif
				  r->relent.addend = 0;
				  r->relent.address = dst_ptr - dst_base_ptr;
				  r->relent.howto = &howto_table[reloc >> 6];
				  r->relent.sym_ptr_ptr = (asymbol **) NULL;
				  section->reloc_count++;

				  src += 2;
				  /* Fake up the data to look like it's got the -ve pc in it, this makes
				       it much easier to convert into other formats. This is done by
				       hitting the addend.
				       */
				  if (r->relent.howto->pc_relative == true)
				    {
				      r->relent.addend -= dst_ptr - dst_base_ptr;
				    }



				}
				break;
			      case RELOCATION_TYPE_COM:
				BFD_FAIL ();
			      }
			  }
			*dst_ptr++ = *src++;
		      }
		  }
	      }
	  }
	  break;
	case oasys_record_is_local_enum:
	case oasys_record_is_symbol_enum:
	case oasys_record_is_section_enum:
	  break;
	default:
	  loop = false;
	}
    }

  return true;

}

static boolean
oasys_new_section_hook (abfd, newsect)
     bfd *abfd;
     asection *newsect;
{
  newsect->used_by_bfd = (PTR)
    bfd_alloc (abfd, sizeof (oasys_per_section_type));
  if (!newsect->used_by_bfd)
    return false;
  oasys_per_section (newsect)->data = (bfd_byte *) NULL;
  oasys_per_section (newsect)->section = newsect;
  oasys_per_section (newsect)->offset = 0;
  oasys_per_section (newsect)->initialized = false;
  newsect->alignment_power = 1;
  /* Turn the section string into an index */

  sscanf (newsect->name, "%u", &newsect->target_index);

  return true;
}


static long
oasys_get_reloc_upper_bound (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  if (! oasys_slurp_section_data (abfd))
    return -1;
  return (asect->reloc_count + 1) * sizeof (arelent *);
}

static boolean
oasys_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  oasys_per_section_type *p = (oasys_per_section_type *) section->used_by_bfd;
  oasys_slurp_section_data (abfd);
  if (p->initialized == false)
    {
      (void) memset (location, 0, (int) count);
    }
  else
    {
      (void) memcpy (location, (PTR) (p->data + offset), (int) count);
    }
  return true;
}


long
oasys_canonicalize_reloc (ignore_abfd, section, relptr, symbols)
     bfd *ignore_abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
  unsigned int reloc_count = 0;
  oasys_reloc_type *src = (oasys_reloc_type *) (section->relocation);
  while (src != (oasys_reloc_type *) NULL)
    {
      abort ();

#if 0
      if (src->relent.section == (asection *) NULL)
	{
	  src->relent.sym_ptr_ptr = symbols + src->symbol;
	}
#endif

      *relptr++ = &src->relent;
      src = src->next;
      reloc_count++;
    }
  *relptr = (arelent *) NULL;
  return section->reloc_count = reloc_count;
}




/* Writing */


/* Calculate the checksum and write one record */
static boolean
oasys_write_record (abfd, type, record, size)
     bfd *abfd;
     oasys_record_enum_type type;
     oasys_record_union_type *record;
     size_t size;
{
  int checksum;
  size_t i;
  unsigned char *ptr;

  record->header.length = size;
  record->header.type = (int) type;
  record->header.check_sum = 0;
  record->header.fill = 0;
  ptr = (unsigned char *) &record->pad[0];
  checksum = 0;
  for (i = 0; i < size; i++)
    {
      checksum += *ptr++;
    }
  record->header.check_sum = 0xff & (-checksum);
  if (bfd_write ((PTR) record, 1, size, abfd) != size)
    return false;
  return true;
}


/* Write out all the symbols */
static boolean
oasys_write_syms (abfd)
     bfd *abfd;
{
  unsigned int count;
  asymbol **generic = bfd_get_outsymbols (abfd);
  unsigned int index = 0;
  for (count = 0; count < bfd_get_symcount (abfd); count++)
    {

      oasys_symbol_record_type symbol;
      asymbol *CONST g = generic[count];

      CONST char *src = g->name;
      char *dst = symbol.name;
      unsigned int l = 0;

      if (bfd_is_com_section (g->section))
	{
	  symbol.relb = RELOCATION_TYPE_COM;
	  bfd_h_put_16 (abfd, index, symbol.refno);
	  index++;
	}
      else if (bfd_is_abs_section (g->section))
	{
	  symbol.relb = RELOCATION_TYPE_ABS;
	  bfd_h_put_16 (abfd, 0, symbol.refno);

	}
      else if (bfd_is_und_section (g->section))
	{
	  symbol.relb = RELOCATION_TYPE_UND;
	  bfd_h_put_16 (abfd, index, symbol.refno);
	  /* Overload the value field with the output index number */
	  index++;
	}
      else if (g->flags & BSF_DEBUGGING)
	{
	  /* throw it away */
	  continue;
	}
      else
	{
	  if (g->section == (asection *) NULL)
	    {
	      /* Sometime, the oasys tools give out a symbol with illegal
	   bits in it, we'll output it in the same broken way */

	      symbol.relb = RELOCATION_TYPE_REL | 0;
	    }
	  else
	    {
	      symbol.relb = RELOCATION_TYPE_REL | g->section->output_section->target_index;
	    }
	  bfd_h_put_16 (abfd, 0, symbol.refno);
	}
#ifdef UNDERSCORE_HACK
      if (src[l] == '_')
	dst[l++] = '.';
#endif
      while (src[l])
	{
	  dst[l] = src[l];
	  l++;
	}

      bfd_h_put_32 (abfd, g->value, symbol.value);


      if (g->flags & BSF_LOCAL)
	{
	  if (! oasys_write_record (abfd,
				    oasys_record_is_local_enum,
				    (oasys_record_union_type *) & symbol,
				    offsetof (oasys_symbol_record_type,
					      name[0]) + l))
	    return false;
	}
      else
	{
	  if (! oasys_write_record (abfd,
				    oasys_record_is_symbol_enum,
				    (oasys_record_union_type *) & symbol,
				    offsetof (oasys_symbol_record_type,
					      name[0]) + l))
	    return false;
	}
      g->value = index - 1;
    }

  return true;
}


 /* Write a section header for each section */
static boolean
oasys_write_sections (abfd)
     bfd *abfd;
{
  asection *s;
  static oasys_section_record_type out;

  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      if (!isdigit (s->name[0]))
	{
	  (*_bfd_error_handler)
	    ("%s: can not represent section `%s' in oasys",
	     bfd_get_filename (abfd), s->name);
	  bfd_set_error (bfd_error_nonrepresentable_section);
	  return false;
	}
      out.relb = RELOCATION_TYPE_REL | s->target_index;
      bfd_h_put_32 (abfd, s->_cooked_size, out.value);
      bfd_h_put_32 (abfd, s->vma, out.vma);

      if (! oasys_write_record (abfd,
				oasys_record_is_section_enum,
				(oasys_record_union_type *) & out,
				sizeof (out)))
	return false;
    }
  return true;
}

static boolean
oasys_write_header (abfd)
     bfd *abfd;
{
  /* Create and write the header */
  oasys_header_record_type r;
  size_t length = strlen (abfd->filename);
  if (length > (size_t) sizeof (r.module_name))
    {
      length = sizeof (r.module_name);
    }

  (void) memcpy (r.module_name,
		 abfd->filename,
		 length);
  (void) memset (r.module_name + length,
		 ' ',
		 sizeof (r.module_name) - length);

  r.version_number = OASYS_VERSION_NUMBER;
  r.rev_number = OASYS_REV_NUMBER;
  if (! oasys_write_record (abfd,
			    oasys_record_is_header_enum,
			    (oasys_record_union_type *) & r,
			    offsetof (oasys_header_record_type,
				      description[0])))
    return false;

  return true;
}

static boolean
oasys_write_end (abfd)
     bfd *abfd;
{
  oasys_end_record_type end;
  unsigned char null = 0;
  end.relb = RELOCATION_TYPE_ABS;
  bfd_h_put_32 (abfd, abfd->start_address, end.entry);
  bfd_h_put_16 (abfd, 0, end.fill);
  end.zero = 0;
  if (! oasys_write_record (abfd,
			    oasys_record_is_end_enum,
			    (oasys_record_union_type *) & end,
			    sizeof (end)))
    return false;
  if (bfd_write ((PTR) & null, 1, 1, abfd) != 1)
    return false;
  return true;
}

static int
comp (ap, bp)
     CONST PTR ap;
     CONST PTR bp;
{
  arelent *a = *((arelent **) ap);
  arelent *b = *((arelent **) bp);
  return a->address - b->address;
}

/*
 Writing data..

*/
static boolean
oasys_write_data (abfd)
     bfd *abfd;
{
  asection *s;
  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      if (s->flags & SEC_LOAD)
	{
	  bfd_byte *raw_data = oasys_per_section (s)->data;
	  oasys_data_record_type processed_data;
	  bfd_size_type current_byte_index = 0;
	  unsigned int relocs_to_go = s->reloc_count;
	  arelent **p = s->orelocation;
	  if (s->reloc_count != 0)
	    {
/* Sort the reloc records so it's easy to insert the relocs into the
	   data */

	      qsort (s->orelocation,
		     s->reloc_count,
		     sizeof (arelent **),
		     comp);
	    }
	  current_byte_index = 0;
	  processed_data.relb = s->target_index | RELOCATION_TYPE_REL;

	  while (current_byte_index < s->_cooked_size)
	    {
	      /* Scan forwards by eight bytes or however much is left and see if
	       there are any relocations going on */
	      bfd_byte *mod = &processed_data.data[0];
	      bfd_byte *dst = &processed_data.data[1];

	      unsigned int i = 0;
	      *mod = 0;


	      bfd_h_put_32 (abfd, s->vma + current_byte_index,
			    processed_data.addr);

	      /* Don't start a relocation unless you're sure you can finish it
 	       within the same data record.  The worst case relocation is a
 	       4-byte relocatable value which is split across two modification
 	       bytes (1 relocation byte + 2 symbol reference bytes + 2 data +
 	       1 modification byte + 2 data = 8 bytes total).  That's where
 	       the magic number 8 comes from.
 	    */
	      while (current_byte_index < s->_raw_size && dst <=
		     &processed_data.data[sizeof (processed_data.data) - 8])
		{


		  if (relocs_to_go != 0)
		    {
		      arelent *r = *p;
		      reloc_howto_type *const how = r->howto;
		      /* There is a relocation, is it for this byte ? */
		      if (r->address == current_byte_index)
			{
			  unsigned char rel_byte;

			  p++;
			  relocs_to_go--;

			  *mod |= (1 << i);
			  if (how->pc_relative)
			    {
			      rel_byte = RELOCATION_PCREL_BIT;

			      /* Also patch the raw data so that it doesn't have
			 the -ve stuff any more */
			      if (how->size != 2)
				{
				  bfd_put_16 (abfd,
					      bfd_get_16 (abfd, raw_data) +
					      current_byte_index, raw_data);
				}

			      else
				{
				  bfd_put_32 (abfd,
					      bfd_get_32 (abfd, raw_data) +
					      current_byte_index, raw_data);
				}
			    }
			  else
			    {
			      rel_byte = 0;
			    }
			  if (how->size == 2)
			    {
			      rel_byte |= RELOCATION_32BIT_BIT;
			    }

			  /* Is this a section relative relocation, or a symbol
		       relative relocation ? */
			  abort ();

#if 0
			  if (r->section != (asection *) NULL)
			    {
			      /* The relent has a section attached, so it must be section
			     relative */
			      rel_byte |= RELOCATION_TYPE_REL;
			      rel_byte |= r->section->output_section->target_index;
			      *dst++ = rel_byte;
			    }
			  else
#endif
			    {
			      asymbol *p = *(r->sym_ptr_ptr);

			      /* If this symbol has a section attached, then it
			     has already been resolved.  Change from a symbol
			     ref to a section ref */
			      if (p->section != (asection *) NULL)
				{
				  rel_byte |= RELOCATION_TYPE_REL;
				  rel_byte |=
				    p->section->output_section->target_index;
				  *dst++ = rel_byte;
				}
			      else
				{
				  rel_byte |= RELOCATION_TYPE_UND;
				  *dst++ = rel_byte;
				  /* Next two bytes are a symbol index - we can get
			       this from the symbol value which has been zapped
			       into the symbol index in the table when the
			       symbol table was written
			       */
				  *dst++ = p->value >> 8;
				  *dst++ = p->value;
				}
			    }
#define ADVANCE { if (++i >= 8) { i = 0; mod = dst++; *mod = 0; } current_byte_index++; }
			  /* relocations never occur from an unloadable section,
		       so we can assume that raw_data is not NULL
		     */
			  *dst++ = *raw_data++;
			  ADVANCE
			    * dst++ = *raw_data++;
			  ADVANCE
			    if (how->size == 2)
			    {
			      *dst++ = *raw_data++;
			      ADVANCE
				* dst++ = *raw_data++;
			      ADVANCE
			    }
			  continue;
			}
		    }
		  /* If this is coming from an unloadable section then copy
		   zeros */
		  if (raw_data == NULL)
		    {
		      *dst++ = 0;
		    }
		  else
		    {
		      *dst++ = *raw_data++;
		    }
		  ADVANCE
		}

	      /* Don't write a useless null modification byte */
	      if (dst == mod + 1)
		{
		  --dst;
		}

	      if (! oasys_write_record (abfd,
					oasys_record_is_data_enum,
					((oasys_record_union_type *)
					 & processed_data),
					dst - (bfd_byte *) & processed_data))
		return false;
	    }
	}
    }

  return true;
}

static boolean
oasys_write_object_contents (abfd)
     bfd *abfd;
{
  if (! oasys_write_header (abfd))
    return false;
  if (! oasys_write_syms (abfd))
    return false;
  if (! oasys_write_sections (abfd))
    return false;
  if (! oasys_write_data (abfd))
    return false;
  if (! oasys_write_end (abfd))
    return false;
  return true;
}




/** exec and core file sections */

/* set section contents is complicated with OASYS since the format is
* not a byte image, but a record stream.
*/
static boolean
oasys_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (count != 0)
    {
      if (oasys_per_section (section)->data == (bfd_byte *) NULL)
	{
	  oasys_per_section (section)->data =
	    (bfd_byte *) (bfd_alloc (abfd, section->_cooked_size));
	  if (!oasys_per_section (section)->data)
	    return false;
	}
      (void) memcpy ((PTR) (oasys_per_section (section)->data + offset),
		     location,
		     (size_t) count);
    }
  return true;
}



/* Native-level interface to symbols. */

/* We read the symbols into a buffer, which is discarded when this
function exits.  We read the strings into a buffer large enough to
hold them all plus all the cached symbol entries. */

static asymbol *
oasys_make_empty_symbol (abfd)
     bfd *abfd;
{

  oasys_symbol_type *new =
  (oasys_symbol_type *) bfd_zalloc (abfd, sizeof (oasys_symbol_type));
  if (!new)
    return NULL;
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}




/* User should have checked the file flags; perhaps we should return
BFD_NO_MORE_SYMBOLS if there are none? */

static bfd *
oasys_openr_next_archived_file (arch, prev)
     bfd *arch;
     bfd *prev;
{
  oasys_ar_data_type *ar = OASYS_AR_DATA (arch);
  oasys_module_info_type *p;
  /* take the next one from the arch state, or reset */
  if (prev == (bfd *) NULL)
    {
      /* Reset the index - the first two entries are bogus*/
      ar->module_index = 0;
    }

  p = ar->module + ar->module_index;
  ar->module_index++;

  if (ar->module_index <= ar->module_count)
    {
      if (p->abfd == (bfd *) NULL)
	{
	  p->abfd = _bfd_create_empty_archive_element_shell (arch);
	  p->abfd->origin = p->pos;
	  p->abfd->filename = p->name;

	  /* Fixup a pointer to this element for the member */
	  p->abfd->arelt_data = (PTR) p;
	}
      return p->abfd;
    }
  else
    {
      bfd_set_error (bfd_error_no_more_archived_files);
      return (bfd *) NULL;
    }
}

static boolean
oasys_find_nearest_line (abfd,
			 section,
			 symbols,
			 offset,
			 filename_ptr,
			 functionname_ptr,
			 line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     char **filename_ptr;
     char **functionname_ptr;
     unsigned int *line_ptr;
{
  return false;

}

static int
oasys_generic_stat_arch_elt (abfd, buf)
     bfd *abfd;
     struct stat *buf;
{
  oasys_module_info_type *mod = (oasys_module_info_type *) abfd->arelt_data;
  if (mod == (oasys_module_info_type *) NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }
  else
    {
      buf->st_size = mod->size;
      buf->st_mode = 0666;
      return 0;
    }
}

static int
oasys_sizeof_headers (abfd, exec)
     bfd *abfd;
     boolean exec;
{
  return 0;
}

#define	oasys_close_and_cleanup _bfd_generic_close_and_cleanup
#define oasys_bfd_free_cached_info _bfd_generic_bfd_free_cached_info

#define oasys_slurp_armap bfd_true
#define oasys_slurp_extended_name_table bfd_true
#define oasys_construct_extended_name_table \
  ((boolean (*) PARAMS ((bfd *, char **, bfd_size_type *, const char **))) \
   bfd_true)
#define oasys_truncate_arname bfd_dont_truncate_arname
#define oasys_write_armap \
  ((boolean (*) \
    PARAMS ((bfd *, unsigned int, struct orl *, unsigned int, int))) \
   bfd_true)
#define oasys_read_ar_hdr bfd_nullvoidptr
#define oasys_get_elt_at_index _bfd_generic_get_elt_at_index
#define oasys_update_armap_timestamp bfd_true

#define oasys_bfd_is_local_label bfd_generic_is_local_label
#define oasys_get_lineno _bfd_nosymbols_get_lineno
#define oasys_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define oasys_read_minisymbols _bfd_generic_read_minisymbols
#define oasys_minisymbol_to_symbol _bfd_generic_minisymbol_to_symbol

#define oasys_bfd_reloc_type_lookup _bfd_norelocs_bfd_reloc_type_lookup

#define oasys_set_arch_mach bfd_default_set_arch_mach

#define oasys_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

#define oasys_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define oasys_bfd_relax_section bfd_generic_relax_section
#define oasys_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define oasys_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define oasys_bfd_final_link _bfd_generic_final_link
#define oasys_bfd_link_split_section _bfd_generic_link_split_section

/*SUPPRESS 460 */
const bfd_target oasys_vec =
{
  "oasys",			/* name */
  bfd_target_oasys_flavour,
  BFD_ENDIAN_BIG,		/* target byte order */
  BFD_ENDIAN_BIG,		/* target headers byte order */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
  (SEC_CODE | SEC_DATA | SEC_ROM | SEC_HAS_CONTENTS
   | SEC_ALLOC | SEC_LOAD | SEC_RELOC),	/* section flags */
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {_bfd_dummy_target,
   oasys_object_p,		/* bfd_check_format */
   oasys_archive_p,
   _bfd_dummy_target,
  },
  {				/* bfd_set_format */
    bfd_false,
    oasys_mkobject,
    _bfd_generic_mkarchive,
    bfd_false
  },
  {				/* bfd_write_contents */
    bfd_false,
    oasys_write_object_contents,
    _bfd_write_archive_contents,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (oasys),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (oasys),
  BFD_JUMP_TABLE_SYMBOLS (oasys),
  BFD_JUMP_TABLE_RELOCS (oasys),
  BFD_JUMP_TABLE_WRITE (oasys),
  BFD_JUMP_TABLE_LINK (oasys),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  (PTR) 0
};
