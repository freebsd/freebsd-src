/* BFD back-end for MS-DOS executables.
   Copyright 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
   Written by Bryan Ford of the University of Utah.

   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).

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
#include "libaout.h"

#if 0
struct exe_header
{
	unsigned short magic;
	unsigned short bytes_in_last_page;
	unsigned short npages;	/* number of 512-byte "pages" including this header */
	unsigned short nrelocs;
	unsigned short header_paras;	/* number of 16-byte paragraphs in header */
	unsigned short reserved;
	unsigned short load_switch;
	unsigned short ss_ofs;
	unsigned short sp;
	unsigned short checksum;
	unsigned short ip;
	unsigned short cs_ofs;
	unsigned short reloc_ofs;
	unsigned short reserved2;
	unsigned short something1;
	unsigned short something2;
	unsigned short something3;
};
#endif

#define EXE_MAGIC	0x5a4d
#define EXE_LOAD_HIGH	0x0000
#define EXE_LOAD_LOW	0xffff
#define EXE_PAGE_SIZE	512


static int
msdos_sizeof_headers (abfd, exec)
     bfd *abfd;
     boolean exec;
{
  return 0;
}

static boolean
msdos_write_object_contents (abfd)
     bfd *abfd;
{
  static char hdr[EXE_PAGE_SIZE];
  file_ptr outfile_size = sizeof(hdr);
  bfd_vma high_vma = 0;
  asection *sec;

  /* Find the total size of the program on disk and in memory.  */
  for (sec = abfd->sections; sec != (asection *) NULL; sec = sec->next)
    {
      if (bfd_get_section_size_before_reloc (sec) == 0)
        continue;
      if (bfd_get_section_flags (abfd, sec) & SEC_ALLOC)
        {
	  bfd_vma sec_vma = bfd_get_section_vma (abfd, sec)
	  		    + bfd_get_section_size_before_reloc (sec);
	  if (sec_vma > high_vma)
	    high_vma = sec_vma;
	}
      if (bfd_get_section_flags (abfd, sec) & SEC_LOAD)
        {
	  file_ptr sec_end = sizeof(hdr)
	  		     + bfd_get_section_vma (abfd, sec)
			     + bfd_get_section_size_before_reloc (sec);
	  if (sec_end > outfile_size)
	    outfile_size = sec_end;
	}
    }

  /* Make sure the program isn't too big.  */
  if (high_vma > (bfd_vma)0xffff)
    {
      bfd_set_error(bfd_error_file_too_big);
      return false;
    }

  /* constants */
  bfd_h_put_16(abfd, EXE_MAGIC, &hdr[0]);
  bfd_h_put_16(abfd, EXE_PAGE_SIZE / 16, &hdr[8]);
  bfd_h_put_16(abfd, EXE_LOAD_LOW, &hdr[12]);
  bfd_h_put_16(abfd, 0x3e, &hdr[24]);
  bfd_h_put_16(abfd, 0x0001, &hdr[28]); /* XXX??? */
  bfd_h_put_16(abfd, 0x30fb, &hdr[30]); /* XXX??? */
  bfd_h_put_16(abfd, 0x726a, &hdr[32]); /* XXX??? */

  /* bytes in last page (0 = full page) */
  bfd_h_put_16(abfd, outfile_size & (EXE_PAGE_SIZE - 1), &hdr[2]);

  /* number of pages */
  bfd_h_put_16(abfd, (outfile_size + EXE_PAGE_SIZE - 1) / EXE_PAGE_SIZE,
  	       &hdr[4]);

  /* Set the initial stack pointer to the end of the bss.
     The program's crt0 code must relocate it to a real stack.  */
  bfd_h_put_16(abfd, high_vma, &hdr[16]);

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0
      || bfd_write (hdr, 1, sizeof(hdr), abfd) != sizeof(hdr))
    return false;

  return true;
}

static boolean
msdos_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{

  if (count == 0)
    return true;

  section->filepos = EXE_PAGE_SIZE + bfd_get_section_vma (abfd, section);

  if (bfd_get_section_flags (abfd, section) & SEC_LOAD)
    {
      if (bfd_seek (abfd, (file_ptr) (section->filepos + offset), SEEK_SET) != 0
          || bfd_write (location, 1, count, abfd) != count)
        return false;
    }

  return true;
}



#define msdos_mkobject aout_32_mkobject
#define msdos_make_empty_symbol aout_32_make_empty_symbol
#define msdos_bfd_reloc_type_lookup aout_32_reloc_type_lookup

#define	msdos_close_and_cleanup _bfd_generic_close_and_cleanup
#define msdos_bfd_free_cached_info _bfd_generic_bfd_free_cached_info
#define msdos_new_section_hook _bfd_generic_new_section_hook
#define msdos_get_section_contents _bfd_generic_get_section_contents
#define msdos_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window
#define msdos_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define msdos_bfd_relax_section bfd_generic_relax_section
#define msdos_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define msdos_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define msdos_bfd_final_link _bfd_generic_final_link
#define msdos_bfd_link_split_section _bfd_generic_link_split_section
#define msdos_set_arch_mach _bfd_generic_set_arch_mach

#define msdos_get_symtab_upper_bound _bfd_nosymbols_get_symtab_upper_bound
#define msdos_get_symtab _bfd_nosymbols_get_symtab
#define msdos_print_symbol _bfd_nosymbols_print_symbol
#define msdos_get_symbol_info _bfd_nosymbols_get_symbol_info
#define msdos_find_nearest_line _bfd_nosymbols_find_nearest_line
#define msdos_get_lineno _bfd_nosymbols_get_lineno
#define msdos_bfd_is_local_label _bfd_nosymbols_bfd_is_local_label
#define msdos_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define msdos_read_minisymbols _bfd_nosymbols_read_minisymbols
#define msdos_minisymbol_to_symbol _bfd_nosymbols_minisymbol_to_symbol

#define msdos_canonicalize_reloc _bfd_norelocs_canonicalize_reloc
#define msdos_get_reloc_upper_bound _bfd_norelocs_get_reloc_upper_bound
#define msdos_32_bfd_link_split_section  _bfd_generic_link_split_section

const bfd_target i386msdos_vec =
{
  "msdos",			/* name */
  bfd_target_msdos_flavour,
  BFD_ENDIAN_LITTLE,		/* target byte order */
  BFD_ENDIAN_LITTLE,		/* target headers byte order */
  (EXEC_P),			/* object flags */
  (SEC_CODE | SEC_DATA | SEC_HAS_CONTENTS
   | SEC_ALLOC | SEC_LOAD),	/* section flags */
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* hdrs */

  {
    _bfd_dummy_target,
    _bfd_dummy_target,		/* bfd_check_format */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {
    bfd_false,
    msdos_mkobject,
    _bfd_generic_mkarchive,
    bfd_false,
  },
  {				/* bfd_write_contents */
    bfd_false,
    msdos_write_object_contents,
    _bfd_write_archive_contents,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (msdos),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (msdos),
  BFD_JUMP_TABLE_RELOCS (msdos),
  BFD_JUMP_TABLE_WRITE (msdos),
  BFD_JUMP_TABLE_LINK (msdos),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  (PTR) 0
};


