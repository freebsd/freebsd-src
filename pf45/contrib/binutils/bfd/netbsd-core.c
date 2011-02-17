/* BFD back end for NetBSD style core files
   Copyright 1988, 1989, 1991, 1992, 1993, 1996, 1998, 1999, 2000, 2001,
   2002, 2003, 2004
   Free Software Foundation, Inc.
   Written by Paul Kranenburg, EUR

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
#include "libaout.h"           /* BFD a.out internal data structures */

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/core.h>

/*
 * FIXME: On NetBSD/sparc CORE_FPU_OFFSET should be (sizeof (struct trapframe))
 */

/* Offset of StackGhost cookie within `struct md_coredump' on
   OpenBSD/sparc.  */
#define CORE_WCOOKIE_OFFSET	344

struct netbsd_core_struct {
	struct core core;
} *rawptr;

/* forward declarations */

static const bfd_target *netbsd_core_file_p
  PARAMS ((bfd *abfd));
static char *netbsd_core_file_failing_command
  PARAMS ((bfd *abfd));
static int netbsd_core_file_failing_signal
  PARAMS ((bfd *abfd));
static bfd_boolean netbsd_core_file_matches_executable_p
  PARAMS ((bfd *core_bfd, bfd *exec_bfd));
static void swap_abort
  PARAMS ((void));

/* Handle NetBSD-style core dump file.  */

static const bfd_target *
netbsd_core_file_p (abfd)
     bfd *abfd;

{
  int i, val;
  file_ptr offset;
  asection *asect, *asect2;
  struct core core;
  struct coreseg coreseg;
  bfd_size_type amt = sizeof core;

  val = bfd_bread ((void *) &core, amt, abfd);
  if (val != sizeof core)
    {
      /* Too small to be a core file */
      bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  if (CORE_GETMAGIC (core) != COREMAGIC)
    {
      bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  amt = sizeof (struct netbsd_core_struct);
  rawptr = (struct netbsd_core_struct *) bfd_zalloc (abfd, amt);
  if (rawptr == NULL)
    return 0;

  rawptr->core = core;
  abfd->tdata.netbsd_core_data = rawptr;

  offset = core.c_hdrsize;
  for (i = 0; i < core.c_nseg; i++)
    {
      const char *sname;
      flagword flags;

      if (bfd_seek (abfd, offset, SEEK_SET) != 0)
	goto punt;

      val = bfd_bread ((void *) &coreseg, (bfd_size_type) sizeof coreseg, abfd);
      if (val != sizeof coreseg)
	{
	  bfd_set_error (bfd_error_file_truncated);
	  goto punt;
	}
      if (CORE_GETMAGIC (coreseg) != CORESEGMAGIC)
	{
	  bfd_set_error (bfd_error_wrong_format);
	  goto punt;
	}

      offset += core.c_seghdrsize;

      switch (CORE_GETFLAG (coreseg))
	{
	case CORE_CPU:
	  sname = ".reg";
	  flags = SEC_ALLOC + SEC_HAS_CONTENTS;
	  break;
	case CORE_DATA:
	  sname = ".data";
	  flags = SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS;
	  break;
	case CORE_STACK:
	  sname = ".stack";
	  flags = SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS;
	  break;
	default:
	  sname = ".unknown";
	  flags = SEC_ALLOC + SEC_HAS_CONTENTS;
	  break;
	}
      asect = bfd_make_section_anyway (abfd, sname);
      if (asect == NULL)
	goto punt;

      asect->flags = flags;
      asect->_raw_size = coreseg.c_size;
      asect->vma = coreseg.c_addr;
      asect->filepos = offset;
      asect->alignment_power = 2;

      if (CORE_GETMID (core) == M_SPARC_NETBSD
	  && CORE_GETFLAG (coreseg) == CORE_CPU
	  && coreseg.c_size > CORE_WCOOKIE_OFFSET)
	{
	  /* Truncate the .reg section.  */
	  asect->_raw_size = CORE_WCOOKIE_OFFSET;

	  /* And create the .wcookie section.  */
	  asect = bfd_make_section_anyway (abfd, ".wcookie");
	  if (asect == NULL)
	    goto punt;

	  asect->flags = SEC_ALLOC + SEC_HAS_CONTENTS;
	  asect->_raw_size = 4;
	  asect->vma = 0;
	  asect->filepos = offset + CORE_WCOOKIE_OFFSET;
	  asect->alignment_power = 2;
	}

      offset += coreseg.c_size;

#ifdef CORE_FPU_OFFSET
      switch (CORE_GETFLAG (coreseg))
	{
	case CORE_CPU:
	  /* Hackish...  */
	  asect->_raw_size = CORE_FPU_OFFSET;
	  asect2 = bfd_make_section_anyway (abfd, ".reg2");
	  if (asect2 == NULL)
	    goto punt;
	  asect2->_raw_size = coreseg.c_size - CORE_FPU_OFFSET;
	  asect2->vma = 0;
	  asect2->filepos = asect->filepos + CORE_FPU_OFFSET;
	  asect2->alignment_power = 2;
	  asect2->flags = SEC_ALLOC + SEC_HAS_CONTENTS;
	  break;
	}
#endif
    }

  /* OK, we believe you.  You're a core file (sure, sure).  */
  return abfd->xvec;

 punt:
  bfd_release (abfd, abfd->tdata.any);
  abfd->tdata.any = NULL;
  bfd_section_list_clear (abfd);
  return 0;
}

static char*
netbsd_core_file_failing_command (abfd)
	bfd *abfd;
{
 /*return core_command (abfd);*/
  return abfd->tdata.netbsd_core_data->core.c_name;
}

static int
netbsd_core_file_failing_signal (abfd)
	bfd *abfd;
{
  /*return core_signal (abfd);*/
  return abfd->tdata.netbsd_core_data->core.c_signo;
}

static bfd_boolean
netbsd_core_file_matches_executable_p  (core_bfd, exec_bfd)
     bfd *core_bfd ATTRIBUTE_UNUSED;
     bfd *exec_bfd ATTRIBUTE_UNUSED;
{
  return TRUE;		/* FIXME, We have no way of telling at this point */
}

/* If somebody calls any byte-swapping routines, shoot them.  */
static void
swap_abort ()
{
  abort (); /* This way doesn't require any declaration for ANSI to fuck up */
}

#define	NO_GET ((bfd_vma (*) (const void *)) swap_abort)
#define	NO_PUT ((void (*) (bfd_vma, void *)) swap_abort)
#define	NO_GETS ((bfd_signed_vma (*) (const void *)) swap_abort)
#define	NO_GET64 ((bfd_uint64_t (*) (const void *)) swap_abort)
#define	NO_PUT64 ((void (*) (bfd_uint64_t, void *)) swap_abort)
#define	NO_GETS64 ((bfd_int64_t (*) (const void *)) swap_abort)

const bfd_target netbsd_core_vec =
  {
    "netbsd-core",
    bfd_target_unknown_flavour,
    BFD_ENDIAN_UNKNOWN,		/* target byte order */
    BFD_ENDIAN_UNKNOWN,		/* target headers byte order */
    (HAS_RELOC | EXEC_P |	/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    0,			                                   /* symbol prefix */
    ' ',						   /* ar_pad_char */
    16,							   /* ar_max_namelen */
    NO_GET64, NO_GETS64, NO_PUT64,	/* 64 bit data.  */
    NO_GET, NO_GETS, NO_PUT,		/* 32 bit data.  */
    NO_GET, NO_GETS, NO_PUT,		/* 16 bit data.  */
    NO_GET64, NO_GETS64, NO_PUT64,	/* 64 bit hdrs.  */
    NO_GET, NO_GETS, NO_PUT,		/* 32 bit hdrs.  */
    NO_GET, NO_GETS, NO_PUT,		/* 16 bit hdrs.  */

    {				/* bfd_check_format */
      _bfd_dummy_target,		/* unknown format */
      _bfd_dummy_target,		/* object file */
      _bfd_dummy_target,		/* archive */
      netbsd_core_file_p		/* a core file */
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
    BFD_JUMP_TABLE_CORE (netbsd),
    BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
    BFD_JUMP_TABLE_SYMBOLS (_bfd_nosymbols),
    BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
    BFD_JUMP_TABLE_WRITE (_bfd_generic),
    BFD_JUMP_TABLE_LINK (_bfd_nolink),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    NULL,

    (PTR) 0			/* backend_data */
  };
