/* BFD back-end for OSF/1 core files.
   Copyright 1993, 1994 Free Software Foundation, Inc.

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

/* This file can only be compiled on systems which use OSF/1 style
   core files.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#include <stdio.h>
#include <string.h>
#include <sys/user.h>
#include <sys/core.h>

/* forward declarations */

static asection *
make_bfd_asection PARAMS ((bfd *, CONST char *, flagword, bfd_size_type,
			   bfd_vma, file_ptr));
static asymbol *
osf_core_make_empty_symbol PARAMS ((bfd *));
static const bfd_target *
osf_core_core_file_p PARAMS ((bfd *));
static char *
osf_core_core_file_failing_command PARAMS ((bfd *));
static int
osf_core_core_file_failing_signal PARAMS ((bfd *));
static boolean
osf_core_core_file_matches_executable_p PARAMS ((bfd *, bfd *));
static void
swap_abort PARAMS ((void));

/* These are stored in the bfd's tdata */

struct osf_core_struct 
{
  int sig;
  char cmd[MAXCOMLEN + 1];
};

#define core_hdr(bfd) ((bfd)->tdata.osf_core_data)
#define core_signal(bfd) (core_hdr(bfd)->sig)
#define core_command(bfd) (core_hdr(bfd)->cmd)

static asection *
make_bfd_asection (abfd, name, flags, _raw_size, vma, filepos)
     bfd *abfd;
     CONST char *name;
     flagword flags;
     bfd_size_type _raw_size;
     bfd_vma vma;
     file_ptr filepos;
{
  asection *asect;

  asect = bfd_make_section_anyway (abfd, name);
  if (!asect)
    return NULL;

  asect->flags = flags;
  asect->_raw_size = _raw_size;
  asect->vma = vma;
  asect->filepos = filepos;
  asect->alignment_power = 8;

  return asect;
}

static asymbol *
osf_core_make_empty_symbol (abfd)
     bfd *abfd;
{
  asymbol *new = (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
  if (new)
    new->the_bfd = abfd;
  return new;
}

static const bfd_target *
osf_core_core_file_p (abfd)
     bfd *abfd;
{
  int val;
  int i;
  char *secname;
  struct core_filehdr core_header;

  val = bfd_read ((PTR)&core_header, 1, sizeof core_header, abfd);
  if (val != sizeof core_header)
    return NULL;

  if (strncmp (core_header.magic, "Core", 4) != 0)
    return NULL;

  core_hdr (abfd) = (struct osf_core_struct *)
    bfd_zalloc (abfd, sizeof (struct osf_core_struct));
  if (!core_hdr (abfd))
    return NULL;

  strncpy (core_command (abfd), core_header.name, MAXCOMLEN + 1);
  core_signal (abfd) = core_header.signo;

  for (i = 0; i < core_header.nscns; i++)
    {
      struct core_scnhdr core_scnhdr;
      flagword flags;

      val = bfd_read ((PTR)&core_scnhdr, 1, sizeof core_scnhdr, abfd);
      if (val != sizeof core_scnhdr)
	break;

      /* Skip empty sections.  */
      if (core_scnhdr.size == 0 || core_scnhdr.scnptr == 0)
	continue;

      switch (core_scnhdr.scntype)
	{
	case SCNRGN:
	  secname = ".data";
	  flags = SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS;
	  break;
	case SCNSTACK:
	  secname = ".stack";
	  flags = SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS;
	  break;
	case SCNREGS:
	  secname = ".reg";
	  flags = SEC_HAS_CONTENTS;
	  break;
	default:
	  (*_bfd_error_handler) ("Unhandled OSF/1 core file section type %d\n",
				 core_scnhdr.scntype);
	  continue;
	}

      if (!make_bfd_asection (abfd, secname, flags,
			      (bfd_size_type) core_scnhdr.size,
			      (bfd_vma) core_scnhdr.vaddr,
			      (file_ptr) core_scnhdr.scnptr))
	return NULL;
    }

  /* OK, we believe you.  You're a core file (sure, sure).  */

  return abfd->xvec;
}

static char *
osf_core_core_file_failing_command (abfd)
     bfd *abfd;
{
  return core_command (abfd);
}

/* ARGSUSED */
static int
osf_core_core_file_failing_signal (abfd)
     bfd *abfd;
{
  return core_signal (abfd);
}

/* ARGSUSED */
static boolean
osf_core_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd, *exec_bfd;
{
  return true;		/* FIXME, We have no way of telling at this point */
}

#define osf_core_get_symtab_upper_bound _bfd_nosymbols_get_symtab_upper_bound
#define osf_core_get_symtab _bfd_nosymbols_get_symtab
#define osf_core_print_symbol _bfd_nosymbols_print_symbol
#define osf_core_get_symbol_info _bfd_nosymbols_get_symbol_info
#define osf_core_bfd_is_local_label _bfd_nosymbols_bfd_is_local_label
#define osf_core_get_lineno _bfd_nosymbols_get_lineno
#define osf_core_find_nearest_line _bfd_nosymbols_find_nearest_line
#define osf_core_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define osf_core_read_minisymbols _bfd_nosymbols_read_minisymbols
#define osf_core_minisymbol_to_symbol _bfd_nosymbols_minisymbol_to_symbol

/* If somebody calls any byte-swapping routines, shoot them.  */
static void
swap_abort()
{
  abort(); /* This way doesn't require any declaration for ANSI to fuck up */
}
#define	NO_GET	((bfd_vma (*) PARAMS ((   const bfd_byte *))) swap_abort )
#define	NO_PUT	((void    (*) PARAMS ((bfd_vma, bfd_byte *))) swap_abort )
#define	NO_SIGNED_GET \
  ((bfd_signed_vma (*) PARAMS ((const bfd_byte *))) swap_abort )

const bfd_target osf_core_vec =
  {
    "osf-core",
    bfd_target_unknown_flavour,
    BFD_ENDIAN_BIG,		/* target byte order */
    BFD_ENDIAN_BIG,		/* target headers byte order */
    (HAS_RELOC | EXEC_P |	/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    0,			                                   /* symbol prefix */
    ' ',						   /* ar_pad_char */
    16,							   /* ar_max_namelen */
    NO_GET, NO_SIGNED_GET, NO_PUT,	/* 64 bit data */
    NO_GET, NO_SIGNED_GET, NO_PUT,	/* 32 bit data */
    NO_GET, NO_SIGNED_GET, NO_PUT,	/* 16 bit data */
    NO_GET, NO_SIGNED_GET, NO_PUT,	/* 64 bit hdrs */
    NO_GET, NO_SIGNED_GET, NO_PUT,	/* 32 bit hdrs */
    NO_GET, NO_SIGNED_GET, NO_PUT,	/* 16 bit hdrs */

    {				/* bfd_check_format */
     _bfd_dummy_target,		/* unknown format */
     _bfd_dummy_target,		/* object file */
     _bfd_dummy_target,		/* archive */
     osf_core_core_file_p	/* a core file */
    },
    {				/* bfd_set_format */
     bfd_false, bfd_false,
     bfd_false, bfd_false
    },
    {				/* bfd_write_contents */
     bfd_false, bfd_false,
     bfd_false, bfd_false
    },
    
       BFD_JUMP_TABLE_GENERIC (_bfd_generic),
       BFD_JUMP_TABLE_COPY (_bfd_generic),
       BFD_JUMP_TABLE_CORE (osf_core),
       BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
       BFD_JUMP_TABLE_SYMBOLS (osf_core),
       BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
       BFD_JUMP_TABLE_WRITE (_bfd_generic),
       BFD_JUMP_TABLE_LINK (_bfd_nolink),
       BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    (PTR) 0			/* backend_data */
};
