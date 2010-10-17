/* BFD back-end for a.out.adobe binaries.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000, 2001,
   2002, 2003
   Free Software Foundation, Inc.
   Written by Cygnus Support.  Based on bout.c.

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#include "aout/adobe.h"

#include "aout/stab_gnu.h"
#include "libaout.h"		/* BFD a.out internal data structures.  */

/* Forward decl.  */
extern const bfd_target a_out_adobe_vec;

static const bfd_target *aout_adobe_callback
  PARAMS ((bfd *));
extern bfd_boolean aout_32_slurp_symbol_table
  PARAMS ((bfd *abfd));
extern bfd_boolean aout_32_write_syms
  PARAMS ((bfd *));
static void aout_adobe_write_section
  PARAMS ((bfd *abfd, sec_ptr sect));
static const bfd_target * aout_adobe_object_p
  PARAMS ((bfd *));
static bfd_boolean aout_adobe_mkobject
  PARAMS ((bfd *));
static bfd_boolean aout_adobe_write_object_contents
  PARAMS ((bfd *));
static bfd_boolean aout_adobe_set_section_contents
  PARAMS ((bfd *, asection *, const PTR, file_ptr, bfd_size_type));
static bfd_boolean aout_adobe_set_arch_mach
  PARAMS ((bfd *, enum bfd_architecture, unsigned long));
static int     aout_adobe_sizeof_headers
  PARAMS ((bfd *, bfd_boolean));

/* Swaps the information in an executable header taken from a raw byte
   stream memory image, into the internal exec_header structure.  */

void aout_adobe_swap_exec_header_in
  PARAMS ((bfd *, struct external_exec *, struct internal_exec *));

void
aout_adobe_swap_exec_header_in (abfd, raw_bytes, execp)
     bfd *abfd;
     struct external_exec *raw_bytes;
     struct internal_exec *execp;
{
  struct external_exec *bytes = (struct external_exec *) raw_bytes;

  /* Now fill in fields in the execp, from the bytes in the raw data.  */
  execp->a_info   = H_GET_32 (abfd, bytes->e_info);
  execp->a_text   = GET_WORD (abfd, bytes->e_text);
  execp->a_data   = GET_WORD (abfd, bytes->e_data);
  execp->a_bss    = GET_WORD (abfd, bytes->e_bss);
  execp->a_syms   = GET_WORD (abfd, bytes->e_syms);
  execp->a_entry  = GET_WORD (abfd, bytes->e_entry);
  execp->a_trsize = GET_WORD (abfd, bytes->e_trsize);
  execp->a_drsize = GET_WORD (abfd, bytes->e_drsize);
}

/* Swaps the information in an internal exec header structure into the
   supplied buffer ready for writing to disk.  */

void aout_adobe_swap_exec_header_out
  PARAMS ((bfd *, struct internal_exec *, struct external_exec *));

void
aout_adobe_swap_exec_header_out (abfd, execp, raw_bytes)
     bfd *abfd;
     struct internal_exec *execp;
     struct external_exec *raw_bytes;
{
  struct external_exec *bytes = (struct external_exec *) raw_bytes;

  /* Now fill in fields in the raw data, from the fields in the exec
     struct.  */
  H_PUT_32 (abfd, execp->a_info  , bytes->e_info);
  PUT_WORD (abfd, execp->a_text  , bytes->e_text);
  PUT_WORD (abfd, execp->a_data  , bytes->e_data);
  PUT_WORD (abfd, execp->a_bss   , bytes->e_bss);
  PUT_WORD (abfd, execp->a_syms  , bytes->e_syms);
  PUT_WORD (abfd, execp->a_entry , bytes->e_entry);
  PUT_WORD (abfd, execp->a_trsize, bytes->e_trsize);
  PUT_WORD (abfd, execp->a_drsize, bytes->e_drsize);
}

static const bfd_target *
aout_adobe_object_p (abfd)
     bfd *abfd;
{
  struct internal_exec anexec;
  struct external_exec exec_bytes;
  char *targ;
  bfd_size_type amt = EXEC_BYTES_SIZE;

  if (bfd_bread ((PTR) &exec_bytes, amt, abfd) != amt)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  anexec.a_info = H_GET_32 (abfd, exec_bytes.e_info);

  /* Normally we just compare for the magic number.
     However, a bunch of Adobe tools aren't fixed up yet; they generate
     files using ZMAGIC(!).
     If the environment variable GNUTARGET is set to "a.out.adobe", we will
     take just about any a.out file as an Adobe a.out file.  FIXME!  */

  if (N_BADMAG (anexec))
    {
      targ = getenv ("GNUTARGET");
      if (targ && !strcmp (targ, a_out_adobe_vec.name))
	/* Just continue anyway, if specifically set to this format.  */
	;
      else
	{
	  bfd_set_error (bfd_error_wrong_format);
	  return 0;
	}
    }

  aout_adobe_swap_exec_header_in (abfd, &exec_bytes, &anexec);
  return aout_32_some_aout_object_p (abfd, &anexec, aout_adobe_callback);
}

/* Finish up the opening of a b.out file for reading.  Fill in all the
   fields that are not handled by common code.  */

static const bfd_target *
aout_adobe_callback (abfd)
     bfd *abfd;
{
  struct internal_exec *execp = exec_hdr (abfd);
  asection *sect;
  struct external_segdesc ext[1];
  char *section_name;
  char try_again[30];	/* Name and number.  */
  char *newname;
  int trynum;
  flagword flags;

  /* Architecture and machine type -- unknown in this format.  */
  bfd_set_arch_mach (abfd, bfd_arch_unknown, 0L);

  /* The positions of the string table and symbol table.  */
  obj_str_filepos (abfd) = N_STROFF (*execp);
  obj_sym_filepos (abfd) = N_SYMOFF (*execp);

  /* Suck up the section information from the file, one section at a time.  */
  for (;;)
    {
      bfd_size_type amt = sizeof (*ext);
      if (bfd_bread ((PTR) ext, amt, abfd) != amt)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);

	  return 0;
	}
      switch (ext->e_type[0])
	{
	case N_TEXT:
	  section_name = ".text";
	  flags = SEC_CODE | SEC_LOAD | SEC_ALLOC | SEC_HAS_CONTENTS;
	  break;

	case N_DATA:
	  section_name = ".data";
	  flags = SEC_DATA | SEC_LOAD | SEC_ALLOC | SEC_HAS_CONTENTS;
	  break;

	case N_BSS:
	  section_name = ".bss";
	  flags = SEC_DATA | SEC_HAS_CONTENTS;
	  break;

	case 0:
	  goto no_more_sections;

	default:
	  (*_bfd_error_handler)
	    (_("%s: Unknown section type in a.out.adobe file: %x\n"),
	     bfd_archive_filename (abfd), ext->e_type[0]);
	  goto no_more_sections;
	}

      /* First one is called ".text" or whatever; subsequent ones are
	 ".text1", ".text2", ...  */
      bfd_set_error (bfd_error_no_error);
      sect = bfd_make_section (abfd, section_name);
      trynum = 0;

      while (!sect)
	{
	  if (bfd_get_error () != bfd_error_no_error)
	    /* Some other error -- slide into the sunset.  */
	    return 0;
	  sprintf (try_again, "%s%d", section_name, ++trynum);
	  sect = bfd_make_section (abfd, try_again);
	}

      /* Fix the name, if it is a sprintf'd name.  */
      if (sect->name == try_again)
	{
	  amt = strlen (sect->name);
	  newname = (char *) bfd_zalloc (abfd, amt);
	  if (newname == NULL)
	    return 0;
	  strcpy (newname, sect->name);
	  sect->name = newname;
	}

      /* Now set the section's attributes.  */
      bfd_set_section_flags (abfd, sect, flags);
      /* Assumed big-endian.  */
      sect->_raw_size = ((ext->e_size[0] << 8)
			 | ext->e_size[1] << 8
			 | ext->e_size[2]);
      sect->_cooked_size = sect->_raw_size;
      sect->vma = H_GET_32 (abfd, ext->e_virtbase);
      sect->filepos = H_GET_32 (abfd, ext->e_filebase);
      /* FIXME XXX alignment?  */

      /* Set relocation information for first section of each type.  */
      if (trynum == 0)
	switch (ext->e_type[0])
	  {
	  case N_TEXT:
	    sect->rel_filepos = N_TRELOFF (*execp);
	    sect->reloc_count = execp->a_trsize;
	    break;

	  case N_DATA:
	    sect->rel_filepos = N_DRELOFF (*execp);
	    sect->reloc_count = execp->a_drsize;
	    break;

	  default:
	    break;
	  }
    }
 no_more_sections:

  adata (abfd).reloc_entry_size = sizeof (struct reloc_std_external);
  adata (abfd).symbol_entry_size = sizeof (struct external_nlist);
  adata (abfd).page_size = 1; /* Not applicable.  */
  adata (abfd).segment_size = 1; /* Not applicable.  */
  adata (abfd).exec_bytes_size = EXEC_BYTES_SIZE;

  return abfd->xvec;
}

struct bout_data_struct
  {
    struct aoutdata a;
    struct internal_exec e;
  };

static bfd_boolean
aout_adobe_mkobject (abfd)
     bfd *abfd;
{
  struct bout_data_struct *rawptr;
  bfd_size_type amt = sizeof (struct bout_data_struct);

  rawptr = (struct bout_data_struct *) bfd_zalloc (abfd, amt);
  if (rawptr == NULL)
    return FALSE;

  abfd->tdata.bout_data = rawptr;
  exec_hdr (abfd) = &rawptr->e;

  adata (abfd).reloc_entry_size = sizeof (struct reloc_std_external);
  adata (abfd).symbol_entry_size = sizeof (struct external_nlist);
  adata (abfd).page_size = 1; /* Not applicable.  */
  adata (abfd).segment_size = 1; /* Not applicable.  */
  adata (abfd).exec_bytes_size = EXEC_BYTES_SIZE;

  return TRUE;
}

static bfd_boolean
aout_adobe_write_object_contents (abfd)
     bfd *abfd;
{
  struct external_exec swapped_hdr;
  static struct external_segdesc sentinel[1];	/* Initialized to zero.  */
  asection *sect;
  bfd_size_type amt;

  exec_hdr (abfd)->a_info = ZMAGIC;

  /* Calculate text size as total of text sections, etc.  */

  exec_hdr (abfd)->a_text = 0;
  exec_hdr (abfd)->a_data = 0;
  exec_hdr (abfd)->a_bss  = 0;
  exec_hdr (abfd)->a_trsize = 0;
  exec_hdr (abfd)->a_drsize = 0;

  for (sect = abfd->sections; sect; sect = sect->next)
    {
      if (sect->flags & SEC_CODE)
	{
	  exec_hdr (abfd)->a_text += sect->_raw_size;
	  exec_hdr (abfd)->a_trsize += sect->reloc_count *
	    sizeof (struct reloc_std_external);
	}
      else if (sect->flags & SEC_DATA)
	{
	  exec_hdr (abfd)->a_data += sect->_raw_size;
	  exec_hdr (abfd)->a_drsize += sect->reloc_count *
	    sizeof (struct reloc_std_external);
	}
      else if (sect->flags & SEC_ALLOC && !(sect->flags & SEC_LOAD))
	{
	  exec_hdr (abfd)->a_bss += sect->_raw_size;
	}
    }

  exec_hdr (abfd)->a_syms = bfd_get_symcount (abfd)
    * sizeof (struct external_nlist);
  exec_hdr (abfd)->a_entry = bfd_get_start_address (abfd);

  aout_adobe_swap_exec_header_out (abfd, exec_hdr (abfd), &swapped_hdr);

  amt = EXEC_BYTES_SIZE;
  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0
      || bfd_bwrite ((PTR) &swapped_hdr, amt, abfd) != amt)
    return FALSE;

  /* Now write out the section information.  Text first, data next, rest
     afterward.  */

  for (sect = abfd->sections; sect; sect = sect->next)
    if (sect->flags & SEC_CODE)
      aout_adobe_write_section (abfd, sect);

  for (sect = abfd->sections; sect; sect = sect->next)
    if (sect->flags & SEC_DATA)
      aout_adobe_write_section (abfd, sect);

  for (sect = abfd->sections; sect; sect = sect->next)
    if (!(sect->flags & (SEC_CODE | SEC_DATA)))
      aout_adobe_write_section (abfd, sect);

  /* Write final `sentinel` section header (with type of 0).  */
  amt = sizeof (*sentinel);
  if (bfd_bwrite ((PTR) sentinel, amt, abfd) != amt)
    return FALSE;

  /* Now write out reloc info, followed by syms and strings.  */
  if (bfd_get_symcount (abfd) != 0)
    {
      if (bfd_seek (abfd, (file_ptr) (N_SYMOFF (*exec_hdr (abfd))), SEEK_SET)
	  != 0)
	return FALSE;

      if (! aout_32_write_syms (abfd))
	return FALSE;

      if (bfd_seek (abfd, (file_ptr) (N_TRELOFF (*exec_hdr (abfd))), SEEK_SET)
	  != 0)
	return FALSE;

      for (sect = abfd->sections; sect; sect = sect->next)
	if (sect->flags & SEC_CODE)
	  if (!aout_32_squirt_out_relocs (abfd, sect))
	    return FALSE;

      if (bfd_seek (abfd, (file_ptr) (N_DRELOFF (*exec_hdr (abfd))), SEEK_SET)
	  != 0)
	return FALSE;

      for (sect = abfd->sections; sect; sect = sect->next)
	if (sect->flags & SEC_DATA)
	  if (!aout_32_squirt_out_relocs (abfd, sect))
	    return FALSE;
    }

  return TRUE;
}

static void
aout_adobe_write_section (abfd, sect)
     bfd *abfd ATTRIBUTE_UNUSED;
     sec_ptr sect ATTRIBUTE_UNUSED;
{
  /* FIXME XXX */
}

static bfd_boolean
aout_adobe_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     const PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  file_ptr section_start;
  sec_ptr sect;

  /* Set by bfd.c handler.  */
  if (! abfd->output_has_begun)
    {
      /* Assign file offsets to sections.  Text sections are first, and
	 are contiguous.  Then data sections.  Everything else at the end.  */
      section_start = N_TXTOFF (ignore<-->me);

      for (sect = abfd->sections; sect; sect = sect->next)
	{
	  if (sect->flags & SEC_CODE)
	    {
	      sect->filepos = section_start;
	      /* FIXME:  Round to alignment.  */
	      section_start += sect->_raw_size;
	    }
	}

      for (sect = abfd->sections; sect; sect = sect->next)
	{
	  if (sect->flags & SEC_DATA)
	    {
	      sect->filepos = section_start;
	      /* FIXME:  Round to alignment.  */
	      section_start += sect->_raw_size;
	    }
	}

      for (sect = abfd->sections; sect; sect = sect->next)
	{
	  if (sect->flags & SEC_HAS_CONTENTS &&
	      !(sect->flags & (SEC_CODE | SEC_DATA)))
	    {
	      sect->filepos = section_start;
	      /* FIXME:  Round to alignment.  */
	      section_start += sect->_raw_size;
	    }
	}
    }

  /* Regardless, once we know what we're doing, we might as well get
     going.  */
  if (bfd_seek (abfd, section->filepos + offset, SEEK_SET) != 0)
    return FALSE;

  if (count == 0)
    return TRUE;

  return bfd_bwrite ((PTR) location, count, abfd) == count;
}

static bfd_boolean
aout_adobe_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  if (! bfd_default_set_arch_mach (abfd, arch, machine))
    return FALSE;

  if (arch == bfd_arch_unknown
      || arch == bfd_arch_m68k)
    return TRUE;

  return FALSE;
}

static int
aout_adobe_sizeof_headers (ignore_abfd, ignore)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     bfd_boolean ignore ATTRIBUTE_UNUSED;
{
  return sizeof (struct internal_exec);
}

/* Build the transfer vector for Adobe A.Out files.  */

#define aout_32_close_and_cleanup aout_32_bfd_free_cached_info

#define aout_32_bfd_make_debug_symbol \
  ((asymbol *(*) PARAMS ((bfd *, void *, unsigned long))) bfd_nullvoidptr)

#define aout_32_bfd_reloc_type_lookup \
  ((reloc_howto_type *(*) \
    PARAMS ((bfd *, bfd_reloc_code_real_type))) bfd_nullvoidptr)

#define	aout_32_set_arch_mach		aout_adobe_set_arch_mach
#define	aout_32_set_section_contents	aout_adobe_set_section_contents

#define	aout_32_sizeof_headers		aout_adobe_sizeof_headers
#define aout_32_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define aout_32_get_section_contents_in_window _bfd_generic_get_section_contents_in_window
#define aout_32_bfd_relax_section       bfd_generic_relax_section
#define aout_32_bfd_gc_sections         bfd_generic_gc_sections
#define aout_32_bfd_merge_sections	bfd_generic_merge_sections
#define aout_32_bfd_discard_group	bfd_generic_discard_group
#define aout_32_bfd_link_hash_table_create \
  _bfd_generic_link_hash_table_create
#define aout_32_bfd_link_hash_table_free \
  _bfd_generic_link_hash_table_free
#define aout_32_bfd_link_add_symbols	_bfd_generic_link_add_symbols
#define aout_32_bfd_link_just_syms	_bfd_generic_link_just_syms
#define aout_32_bfd_final_link		_bfd_generic_final_link
#define aout_32_bfd_link_split_section	_bfd_generic_link_split_section

const bfd_target a_out_adobe_vec =
  {
    "a.out.adobe",		/* name */
    bfd_target_aout_flavour,
    BFD_ENDIAN_BIG,		/* data byte order is unknown (big assumed) */
    BFD_ENDIAN_BIG,		/* hdr byte order is big */
    (HAS_RELOC | EXEC_P |	/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT ),
    /* section flags */
    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_CODE | SEC_DATA | SEC_RELOC),
    '_',				/*  symbol leading char */
    ' ',				/* ar_pad_char */
    16,					/* ar_max_namelen */

    bfd_getb64, bfd_getb_signed_64, bfd_putb64,
    bfd_getb32, bfd_getb_signed_32, bfd_putb32,
    bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
    bfd_getb64, bfd_getb_signed_64, bfd_putb64,
    bfd_getb32, bfd_getb_signed_32, bfd_putb32,
    bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */
    {_bfd_dummy_target, aout_adobe_object_p,	/* bfd_check_format */
     bfd_generic_archive_p, _bfd_dummy_target},
    {bfd_false, aout_adobe_mkobject,		/* bfd_set_format */
     _bfd_generic_mkarchive, bfd_false},
    {bfd_false, aout_adobe_write_object_contents,/* bfd_write_contents */
     _bfd_write_archive_contents, bfd_false},

    BFD_JUMP_TABLE_GENERIC (aout_32),
    BFD_JUMP_TABLE_COPY (_bfd_generic),
    BFD_JUMP_TABLE_CORE (_bfd_nocore),
    BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_bsd),
    BFD_JUMP_TABLE_SYMBOLS (aout_32),
    BFD_JUMP_TABLE_RELOCS (aout_32),
    BFD_JUMP_TABLE_WRITE (aout_32),
    BFD_JUMP_TABLE_LINK (aout_32),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    NULL,

    (PTR) 0
  };
