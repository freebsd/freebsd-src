/* BFD back-end for HP/UX core files.
   Copyright 1993, 1994 Free Software Foundation, Inc.
   Written by Stu Grossman, Cygnus Support.
   Converted to back-end form by Ian Lance Taylor, Cygnus SUpport

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

/* This file can only be compiled on systems which use HP/UX style
   core files.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#if defined (HOST_HPPAHPUX) || defined (HOST_HP300HPUX)

/* FIXME: sys/core.h doesn't exist for HPUX version 7.  HPUX version
   5, 6, and 7 core files seem to be standard trad-core.c type core
   files; can we just use trad-core.c in addition to this file?  */

#include <sys/core.h>
#include <sys/utsname.h>

#endif /* HOST_HPPAHPUX */

#ifdef HOST_HPPABSD

/* Not a very swift place to put it, but that's where the BSD port
   puts them.  */
#include "/hpux/usr/include/sys/core.h"

#endif /* HOST_HPPABSD */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <machine/reg.h>
#include <sys/user.h>		/* After a.out.h  */
#include <sys/file.h>
#include <errno.h>

/* These are stored in the bfd's tdata */

struct hpux_core_struct 
{
  int sig;
  char cmd[MAXCOMLEN + 1];
};

#define core_hdr(bfd) ((bfd)->tdata.hpux_core_data)
#define core_signal(bfd) (core_hdr(bfd)->sig)
#define core_command(bfd) (core_hdr(bfd)->cmd)

static asection *
make_bfd_asection (abfd, name, flags, _raw_size, vma, alignment_power)
     bfd *abfd;
     CONST char *name;
     flagword flags;
     bfd_size_type _raw_size;
     bfd_vma vma;
     unsigned int alignment_power;
{
  asection *asect;

  asect = bfd_make_section_anyway (abfd, name);
  if (!asect)
    return NULL;

  asect->flags = flags;
  asect->_raw_size = _raw_size;
  asect->vma = vma;
  asect->filepos = bfd_tell (abfd);
  asect->alignment_power = alignment_power;

  return asect;
}

static asymbol *
hpux_core_make_empty_symbol (abfd)
     bfd *abfd;
{
  asymbol *new = (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
  if (new)
    new->the_bfd = abfd;
  return new;
}

static const bfd_target *
hpux_core_core_file_p (abfd)
     bfd *abfd;
{
  core_hdr (abfd) = (struct hpux_core_struct *)
    bfd_zalloc (abfd, sizeof (struct hpux_core_struct));
  if (!core_hdr (abfd))
    return NULL;

  while (1)
    {
      int val;
      struct corehead core_header;

      val = bfd_read ((void *) &core_header, 1, sizeof core_header, abfd);
      if (val <= 0)
	break;
      switch (core_header.type)
	{
	case CORE_KERNEL:
	case CORE_FORMAT:
	  bfd_seek (abfd, core_header.len, SEEK_CUR);	/* Just skip this */
	  break;
	case CORE_EXEC:
	  {
	    struct proc_exec proc_exec;
	    if (bfd_read ((void *) &proc_exec, 1, core_header.len, abfd)
		!= core_header.len)
	      break;
	    strncpy (core_command (abfd), proc_exec.cmd, MAXCOMLEN + 1);
	  }
	  break;
	case CORE_PROC:
	  {
	    struct proc_info proc_info;
	    if (!make_bfd_asection (abfd, ".reg",
				    SEC_HAS_CONTENTS,
				    core_header.len,
				(int) &proc_info - (int) & proc_info.hw_regs,
				    2))
	      return NULL;

	    if (bfd_read (&proc_info, 1, core_header.len, abfd)
		!= core_header.len)
	      break;
	    core_signal (abfd) = proc_info.sig;
	  }
	  break;

	case CORE_DATA:
	case CORE_STACK:
	case CORE_TEXT:
	case CORE_MMF:
	case CORE_SHM:
	  if (!make_bfd_asection (abfd, ".data",
				  SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS,
				  core_header.len, core_header.addr, 2))
	    return NULL;

	  bfd_seek (abfd, core_header.len, SEEK_CUR);
	  break;

	default:
	  /* Falling into here is an error and should prevent this
	     target from matching.  That way systems which use hpux
	     cores along with other formats can still work.  */
	  return 0;
	}
    }

  /* OK, we believe you.  You're a core file (sure, sure).  */

  return abfd->xvec;
}

static char *
hpux_core_core_file_failing_command (abfd)
     bfd *abfd;
{
  return core_command (abfd);
}

/* ARGSUSED */
static int
hpux_core_core_file_failing_signal (abfd)
     bfd *abfd;
{
  return core_signal (abfd);
}

/* ARGSUSED */
static boolean
hpux_core_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd, *exec_bfd;
{
  return true;			/* FIXME, We have no way of telling at this point */
}

#define hpux_core_get_symtab_upper_bound _bfd_nosymbols_get_symtab_upper_bound
#define hpux_core_get_symtab _bfd_nosymbols_get_symtab
#define hpux_core_print_symbol _bfd_nosymbols_print_symbol
#define hpux_core_get_symbol_info _bfd_nosymbols_get_symbol_info
#define hpux_core_bfd_is_local_label _bfd_nosymbols_bfd_is_local_label
#define hpux_core_get_lineno _bfd_nosymbols_get_lineno
#define hpux_core_find_nearest_line _bfd_nosymbols_find_nearest_line
#define hpux_core_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define hpux_core_read_minisymbols _bfd_nosymbols_read_minisymbols
#define hpux_core_minisymbol_to_symbol _bfd_nosymbols_minisymbol_to_symbol

/* If somebody calls any byte-swapping routines, shoot them.  */
void
swap_abort()
{
  abort(); /* This way doesn't require any declaration for ANSI to fuck up */
}
#define	NO_GET	((bfd_vma (*) PARAMS ((   const bfd_byte *))) swap_abort )
#define	NO_PUT	((void    (*) PARAMS ((bfd_vma, bfd_byte *))) swap_abort )
#define	NO_SIGNED_GET \
  ((bfd_signed_vma (*) PARAMS ((const bfd_byte *))) swap_abort )

const bfd_target hpux_core_vec =
  {
    "hpux-core",
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
     hpux_core_core_file_p	/* a core file */
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
       BFD_JUMP_TABLE_CORE (hpux_core),
       BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
       BFD_JUMP_TABLE_SYMBOLS (hpux_core),
       BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
       BFD_JUMP_TABLE_WRITE (_bfd_generic),
       BFD_JUMP_TABLE_LINK (_bfd_nolink),
       BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    (PTR) 0			/* backend_data */
};
