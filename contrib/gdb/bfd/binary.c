/* BFD back-end for binary objects.
   Copyright 1994 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support, <ian@cygnus.com>

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

/* This is a BFD backend which may be used to write binary objects.
   It may only be used for output, not input.  The intention is that
   this may be used as an output format for objcopy in order to
   generate raw binary data.

   This is very simple.  The only complication is that the real data
   will start at some address X, and in some cases we will not want to
   include X zeroes just to get to that point.  Since the start
   address is not meaningful for this object file format, we use it
   instead to indicate the number of zeroes to skip at the start of
   the file.  objcopy cooperates by specially setting the start
   address to zero by default.  */

#include <ctype.h>

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

/* Any bfd we create by reading a binary file has three symbols:
   a start symbol, an end symbol, and an absolute length symbol.  */
#define BIN_SYMS 3

static boolean binary_mkobject PARAMS ((bfd *));
static const bfd_target *binary_object_p PARAMS ((bfd *));
static boolean binary_get_section_contents
  PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type));
static long binary_get_symtab_upper_bound PARAMS ((bfd *));
static char *mangle_name PARAMS ((bfd *, char *));
static long binary_get_symtab PARAMS ((bfd *, asymbol **));
static asymbol *binary_make_empty_symbol PARAMS ((bfd *));
static void binary_get_symbol_info PARAMS ((bfd *, asymbol *, symbol_info *));
static boolean binary_set_section_contents
  PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type));
static int binary_sizeof_headers PARAMS ((bfd *, boolean));

/* Create a binary object.  Invoked via bfd_set_format.  */

static boolean
binary_mkobject (abfd)
     bfd *abfd;
{
  return true;
}

/* Any file may be considered to be a binary file, provided the target
   was not defaulted.  That is, it must be explicitly specified as
   being binary.  */

static const bfd_target *
binary_object_p (abfd)
     bfd *abfd;
{
  struct stat statbuf;
  asection *sec;

  if (abfd->target_defaulted)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  abfd->symcount = BIN_SYMS;

  /* Find the file size.  */
  if (bfd_stat (abfd, &statbuf) < 0)
    {
      bfd_set_error (bfd_error_system_call);
      return NULL;
    }

  /* One data section.  */
  sec = bfd_make_section (abfd, ".data");
  if (sec == NULL)
    return NULL;
  sec->flags = SEC_ALLOC | SEC_LOAD | SEC_DATA | SEC_HAS_CONTENTS;
  sec->vma = 0;
  sec->_raw_size = statbuf.st_size;
  sec->filepos = 0;

  abfd->tdata.any = (PTR) sec;

  return abfd->xvec;
}

#define binary_close_and_cleanup _bfd_generic_close_and_cleanup
#define binary_bfd_free_cached_info _bfd_generic_bfd_free_cached_info
#define binary_new_section_hook _bfd_generic_new_section_hook

/* Get contents of the only section.  */

static boolean
binary_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (bfd_seek (abfd, offset, SEEK_SET) != 0
      || bfd_read (location, 1, count, abfd) != count)
    return false;
  return true;
}

/* Return the amount of memory needed to read the symbol table.  */

static long
binary_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  return (BIN_SYMS + 1) * sizeof (asymbol *);
}

/* Create a symbol name based on the bfd's filename.  */

static char *
mangle_name (abfd, suffix)
     bfd *abfd;
     char *suffix;
{
  int size;
  char *buf;
  char *p;

  size = (strlen (bfd_get_filename (abfd))
	  + strlen (suffix)
	  + sizeof "_binary__");

  buf = (char *) bfd_alloc (abfd, size);
  if (buf == NULL)
    return "";

  sprintf (buf, "_binary_%s_%s", bfd_get_filename (abfd), suffix);

  /* Change any non-alphanumeric characters to underscores.  */
  for (p = buf; *p; p++)
    if (! isalnum (*p))
      *p = '_';

  return buf;
}

/* Return the symbol table.  */

static long
binary_get_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  asection *sec = (asection *) abfd->tdata.any;
  asymbol *syms;
  unsigned int i;

  syms = (asymbol *) bfd_alloc (abfd, BIN_SYMS * sizeof (asymbol));
  if (syms == NULL)
    return false;

  /* Start symbol.  */
  syms[0].the_bfd = abfd;
  syms[0].name = mangle_name (abfd, "start");
  syms[0].value = 0;
  syms[0].flags = BSF_GLOBAL;
  syms[0].section = sec;
  syms[0].udata.p = NULL;

  /* End symbol.  */
  syms[1].the_bfd = abfd;
  syms[1].name = mangle_name (abfd, "end");
  syms[1].value = sec->_raw_size;
  syms[1].flags = BSF_GLOBAL;
  syms[1].section = sec;
  syms[1].udata.p = NULL;

  /* Size symbol.  */
  syms[2].the_bfd = abfd;
  syms[2].name = mangle_name (abfd, "size");
  syms[2].value = sec->_raw_size;
  syms[2].flags = BSF_GLOBAL;
  syms[2].section = bfd_abs_section_ptr;
  syms[2].udata.p = NULL;

  for (i = 0; i < BIN_SYMS; i++)
    *alocation++ = syms++;
  *alocation = NULL;

  return BIN_SYMS;
}

/* Make an empty symbol.  */

static asymbol *
binary_make_empty_symbol (abfd)
     bfd *abfd;
{
  return (asymbol *) bfd_alloc (abfd, sizeof (asymbol));
}

#define binary_print_symbol _bfd_nosymbols_print_symbol

/* Get information about a symbol.  */

static void
binary_get_symbol_info (ignore_abfd, symbol, ret)
     bfd *ignore_abfd;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
}

#define binary_bfd_is_local_label bfd_generic_is_local_label
#define binary_get_lineno _bfd_nosymbols_get_lineno
#define binary_find_nearest_line _bfd_nosymbols_find_nearest_line
#define binary_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define binary_read_minisymbols _bfd_generic_read_minisymbols
#define binary_minisymbol_to_symbol _bfd_generic_minisymbol_to_symbol

#define binary_get_reloc_upper_bound \
  ((long (*) PARAMS ((bfd *, asection *))) bfd_0l)
#define binary_canonicalize_reloc \
  ((long (*) PARAMS ((bfd *, asection *, arelent **, asymbol **))) bfd_0l)
#define binary_bfd_reloc_type_lookup _bfd_norelocs_bfd_reloc_type_lookup

/* Set the architecture of a binary file.  */
#define binary_set_arch_mach _bfd_generic_set_arch_mach

/* Write section contents of a binary file.  */

static boolean
binary_set_section_contents (abfd, sec, data, offset, size)
     bfd *abfd;
     asection *sec;
     PTR data;
     file_ptr offset;
     bfd_size_type size;
{
  if (! abfd->output_has_begun)
    {
      bfd_vma low;
      asection *s;

      /* The lowest section VMA sets the virtual address of the start
         of the file.  We use the set the file position of all the
         sections.  */
      low = abfd->sections->vma;
      for (s = abfd->sections->next; s != NULL; s = s->next)
	if (s->vma < low)
	  low = s->vma;

      for (s = abfd->sections; s != NULL; s = s->next)
	s->filepos = s->vma - low;

      abfd->output_has_begun = true;
    }

  return _bfd_generic_set_section_contents (abfd, sec, data, offset, size);
}

/* No space is required for header information.  */

static int
binary_sizeof_headers (abfd, exec)
     bfd *abfd;
     boolean exec;
{
  return 0;
}

#define binary_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define binary_bfd_relax_section bfd_generic_relax_section
#define binary_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define binary_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define binary_bfd_final_link _bfd_generic_final_link
#define binary_bfd_link_split_section _bfd_generic_link_split_section
#define binary_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

const bfd_target binary_vec =
{
  "binary",			/* name */
  bfd_target_unknown_flavour,	/* flavour */
  BFD_ENDIAN_UNKNOWN,		/* byteorder */
  BFD_ENDIAN_UNKNOWN,		/* header_byteorder */
  EXEC_P,			/* object_flags */
  (SEC_ALLOC | SEC_LOAD | SEC_READONLY | SEC_CODE | SEC_DATA
   | SEC_ROM | SEC_HAS_CONTENTS), /* section_flags */
  0,				/* symbol_leading_char */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */
  {				/* bfd_check_format */
    _bfd_dummy_target,
    binary_object_p,		/* bfd_check_format */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {				/* bfd_set_format */
    bfd_false,
    binary_mkobject,
    bfd_false,
    bfd_false,
  },
  {				/* bfd_write_contents */
    bfd_false,
    bfd_true,
    bfd_false,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (binary),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (binary),
  BFD_JUMP_TABLE_RELOCS (binary),
  BFD_JUMP_TABLE_WRITE (binary),
  BFD_JUMP_TABLE_LINK (binary),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL
};
