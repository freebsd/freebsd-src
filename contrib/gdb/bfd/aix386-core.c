/* BFD back-end for AIX on PS/2 core files.
   This was based on trad-core.c, which was written by John Gilmore of
        Cygnus Support.
   Copyright 1988, 1989, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
   Written by Minh Tran-Le <TRANLE@INTELLICORP.COM>.
   Converted to back end form by Ian Lance Taylor <ian@cygnus.com>.

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
#include "obstack.h"
#include "coff/i386.h"
#include "coff/internal.h"
#include "libcoff.h"

#include <stdio.h>
#include <stddef.h>
#include <signal.h>

#include <errno.h>

#if defined (_AIX) && defined (_I386)
#define NOCHECKS		/* this is for coredump.h */
#define _h_USER			/* avoid including user.h from coredump.h */
#include <uinfo.h>
#include <sys/i386/coredump.h>
#endif /* _AIX && _I386 */

/* maybe this could work on some other i386 but I have not tried it
 * mtranle@paris - Tue Sep 24 12:49:35 1991
 */

#ifndef COR_MAGIC
# define COR_MAGIC "core"
#endif

/* need this cast because ptr is really void * */
#define core_hdr(bfd) \
    (((bfd->tdata.trad_core_data))->hdr)
#define core_section(bfd,n) \
    (((bfd)->tdata.trad_core_data)->sections[n])
#define core_regsec(bfd) \
    (((bfd)->tdata.trad_core_data)->reg_section)
#define core_reg2sec(bfd) \
    (((bfd)->tdata.trad_core_data)->reg2_section)

/* These are stored in the bfd's tdata */
struct trad_core_struct {
  struct corehdr *hdr;		/* core file header */
  asection *reg_section;
  asection *reg2_section;
  asection *sections[MAX_CORE_SEGS];
};

static const bfd_target *
aix386_core_file_p (abfd)
     bfd *abfd;
{
  int i,n;
  unsigned char longbuf[4];	/* Raw bytes of various header fields */
  int core_size = sizeof (struct corehdr);
  struct corehdr *core;
  struct mergem {
    struct trad_core_struct coredata;
    struct corehdr internal_core;
  } *mergem;

  if (bfd_read ((PTR)longbuf, 1, sizeof (longbuf), abfd) != sizeof (longbuf))
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  if (strncmp(longbuf,COR_MAGIC,4)) return 0;

  if (bfd_seek (abfd, 0L, false) < 0) return 0;

  mergem = (struct mergem *)bfd_zalloc (abfd, sizeof (struct mergem));
  if (mergem == NULL)
    return 0;

  core = &mergem->internal_core;

  if ((bfd_read ((PTR) core, 1, core_size, abfd)) != core_size)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      bfd_release (abfd, (char *)mergem);
      return 0;
    }

  set_tdata (abfd, &mergem->coredata);
  core_hdr (abfd) = core;

  /* create the sections.  This is raunchy, but bfd_close wants to reclaim
     them */
  core_regsec (abfd) = (asection *) bfd_zalloc (abfd, sizeof (asection));
  if (core_regsec (abfd) == NULL)
    {
    loser:
      bfd_release (abfd, (char *)mergem);
      return 0;
    }
  core_reg2sec (abfd) = (asection *) bfd_zalloc (abfd, sizeof (asection));
  if (core_reg2sec (abfd) == NULL)
    {
    loser1:
     bfd_release (abfd, core_regsec (abfd));
      goto loser;
    }

  for (i=0, n=0 ; (i < MAX_CORE_SEGS) && (core->cd_segs[i].cs_type) ; i++)
    {
      if (core->cd_segs[i].cs_offset == 0)
	continue;
      core_section (abfd,n) =
	(asection *) bfd_zalloc (abfd, sizeof (asection));
      if (core_section (abfd,n) == NULL)
	{
	  int j;
	  if (n > 0)
	    {
	      for (j=0; j < n; j++)
		bfd_release (abfd, core_section(abfd, j));
	    }
	  bfd_release (abfd, (char *)mergem);
	  goto loser1;
	}

      switch (core->cd_segs[i].cs_type)
	{
	case COR_TYPE_DATA:
	  core_section (abfd, n)->name = ".data";
	  core_section (abfd, n)->flags = (SEC_ALLOC + SEC_LOAD +
					   SEC_HAS_CONTENTS);
	  break;
	case COR_TYPE_STACK:
	  core_section (abfd, n)->name = ".stack";
	  core_section (abfd, n)->flags = (SEC_ALLOC + SEC_LOAD +
					   SEC_HAS_CONTENTS);
	  break;
	case COR_TYPE_LIBDATA:
	  core_section (abfd, n)->name = ".libdata";
	  core_section (abfd, n)->flags = (SEC_ALLOC + SEC_HAS_CONTENTS);
	  break;
	case COR_TYPE_WRITE:
	  core_section (abfd, n)->name = ".writeable";
	  core_section (abfd, n)->flags = (SEC_ALLOC + SEC_HAS_CONTENTS);
	  break;
	case COR_TYPE_MSC:
	  core_section (abfd, n)->name = ".misc";
	  core_section (abfd, n)->flags = (SEC_ALLOC + SEC_HAS_CONTENTS);
	  break;
	default:
	  core_section (abfd, n)->name = ".unknown";
	  core_section (abfd, n)->flags = (SEC_ALLOC + SEC_HAS_CONTENTS);
	  break;
	}
      core_section (abfd, n)->_raw_size = core->cd_segs[i].cs_len;
      core_section (abfd, n)->vma       = core->cd_segs[i].cs_address;
      core_section (abfd, n)->filepos   = core->cd_segs[i].cs_offset;
      core_section (abfd, n)->alignment_power = 2;
      core_section (abfd, n)->next      = NULL;
      if (n > 0)
	core_section (abfd, (n-1))->next = core_section (abfd, n);

      abfd->section_count = ++n;
    }

  core_regsec (abfd)->name = ".reg";
  core_reg2sec (abfd)->name = ".reg2";

  core_regsec (abfd)->flags = SEC_HAS_CONTENTS;
  core_reg2sec (abfd)->flags = SEC_HAS_CONTENTS;

  core_regsec (abfd)->_raw_size = sizeof(core->cd_regs);
  core_reg2sec (abfd)->_raw_size = sizeof(core->cd_fpregs);

  core_regsec (abfd)->vma = -1;
  core_reg2sec (abfd)->vma = -1;

  /* We'll access the regs afresh in the core file, like any section: */
  core_regsec (abfd)->filepos = (file_ptr)offsetof(struct corehdr,cd_regs[0]);
  core_reg2sec (abfd)->filepos = (file_ptr)offsetof(struct corehdr,
						    cd_fpregs);

  /* add the 2 reg fake sections to abfd */
  abfd->section_count += 2;
  abfd->sections = core_regsec (abfd);
  core_regsec (abfd)->next = core_reg2sec (abfd);
  core_reg2sec (abfd)->next = core_section (abfd, 0);

  return abfd->xvec;
}

static char *
aix386_core_file_failing_command (abfd)
     bfd *abfd;
{
  return core_hdr (abfd)->cd_comm;
}

static int
aix386_core_file_failing_signal (abfd)
     bfd *abfd;
{
  return core_hdr (abfd)->cd_cursig;
}

static boolean
aix386_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd;
     bfd *exec_bfd;
{
  return true;			/* FIXME, We have no way of telling at this
				   point */
}

/* If somebody calls any byte-swapping routines, shoot them.  */
void
swap_abort()
{
  abort(); /* This way doesn't require any declaration for ANSI to fuck up */
}
#define	NO_GET	((PROTO(bfd_vma, (*), (       const bfd_byte *))) swap_abort )
#define NO_GETS ((PROTO(bfd_signed_vma, (*), (const bfd_byte *))) swap_abort )
#define	NO_PUT	((PROTO(void,        (*), (bfd_vma, bfd_byte *))) swap_abort )

const bfd_target aix386_core_vec =
  {
    "aix386-core",
    bfd_target_unknown_flavour,
    BFD_ENDIAN_BIG,		/* target byte order */
    BFD_ENDIANG_BIG,		/* target headers byte order */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    0,						/* leading underscore */
    ' ',					/* ar_pad_char */
    16,						/* ar_max_namelen */
    NO_GET, NO_GETS, NO_PUT,
    NO_GET, NO_GETS, NO_PUT,
    NO_GET, NO_GETS, NO_PUT, /* data */
    NO_GET, NO_GETS, NO_PUT,
    NO_GET, NO_GETS, NO_PUT,
    NO_GET, NO_GETS, NO_PUT, /* hdrs */

    {_bfd_dummy_target, _bfd_dummy_target,
     _bfd_dummy_target, aix386_core_file_p},
    {bfd_false, bfd_false,	/* bfd_create_object */
     bfd_false, bfd_false},
    {bfd_false, bfd_false,	/* bfd_write_contents */
     bfd_false, bfd_false},

     BFD_JUMP_TABLE_GENERIC (_bfd_generic),
     BFD_JUMP_TABLE_COPY (_bfd_generic),
     BFD_JUMP_TABLE_CORE (aix386),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
     BFD_JUMP_TABLE_SYMBOLS (_bfd_nosymbols),
     BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
     BFD_JUMP_TABLE_WRITE (_bfd_generic),
     BFD_JUMP_TABLE_LINK (_bfd_nolink),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    (PTR) 0
};
