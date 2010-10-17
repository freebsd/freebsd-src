/* BFD back-end for HPPA BSD core files.
   Copyright 1993, 1994, 1995, 1998, 1999, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by the Center for Software Science at the University of Utah
   and by Cygnus Support.

   The core file structure for the Utah 4.3BSD and OSF1 ports on the
   PA is a mix between traditional cores and hpux cores -- just
   different enough that supporting this format would tend to add
   gross hacks to trad-core.c or hpux-core.c.  So instead we keep any
   gross hacks isolated to this file.  */

/* This file can only be compiled on systems which use HPPA-BSD style
   core files.

   I would not expect this to be of use to any other host/target, but
   you never know.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#if defined (HOST_HPPABSD)

#include "machine/vmparam.h"

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <machine/reg.h>
#include <sys/user.h>		/* After a.out.h  */
#include <sys/file.h>

static asection *make_bfd_asection
  PARAMS ((bfd *, const char *, flagword, bfd_size_type, file_ptr,
	   unsigned int));
static const bfd_target *hppabsd_core_core_file_p
  PARAMS ((bfd *));
static char *hppabsd_core_core_file_failing_command
  PARAMS ((bfd *));
static int hppabsd_core_core_file_failing_signal
  PARAMS ((bfd *));
static bfd_boolean hppabsd_core_core_file_matches_executable_p
  PARAMS ((bfd *, bfd *));
static void swap_abort
  PARAMS ((void));

/* These are stored in the bfd's tdata.  */

struct hppabsd_core_struct
  {
    int sig;
    char cmd[MAXCOMLEN + 1];
    asection *data_section;
    asection *stack_section;
    asection *reg_section;
  };

#define core_hdr(bfd) ((bfd)->tdata.hppabsd_core_data)
#define core_signal(bfd) (core_hdr(bfd)->sig)
#define core_command(bfd) (core_hdr(bfd)->cmd)
#define core_datasec(bfd) (core_hdr(bfd)->data_section)
#define core_stacksec(bfd) (core_hdr(bfd)->stack_section)
#define core_regsec(bfd) (core_hdr(bfd)->reg_section)

static asection *
make_bfd_asection (abfd, name, flags, _raw_size, offset, alignment_power)
     bfd *abfd;
     const char *name;
     flagword flags;
     bfd_size_type _raw_size;
     file_ptr offset;
     unsigned int alignment_power;
{
  asection *asect;

  asect = bfd_make_section (abfd, name);
  if (!asect)
    return NULL;

  asect->flags = flags;
  asect->_raw_size = _raw_size;
  asect->filepos = offset;
  asect->alignment_power = alignment_power;

  return asect;
}

static const bfd_target *
hppabsd_core_core_file_p (abfd)
     bfd *abfd;
{
  int val;
  struct user u;
  struct hppabsd_core_struct *coredata;
  int clicksz;

  /* Try to read in the u-area.  We will need information from this
     to know how to grok the rest of the core structures.  */
  val = bfd_bread ((void *) &u, (bfd_size_type) sizeof u, abfd);
  if (val != sizeof u)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* Get the page size out of the u structure.  This will be different
     for PA 1.0 machines and PA 1.1 machines.   Yuk!  */
  clicksz = u.u_pcb.pcb_pgsz;

  /* clicksz must be a power of two >= 2k.  */
  if (clicksz < 0x800
      || clicksz != (clicksz & -clicksz))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* Sanity checks.  Make sure the size of the core file matches the
     the size computed from information within the core itself.  */
  {
    FILE *stream = bfd_cache_lookup (abfd);
    struct stat statbuf;
    if (stream == NULL || fstat (fileno (stream), &statbuf) < 0)
      {
	bfd_set_error (bfd_error_system_call);
	return NULL;
      }
    if (NBPG * (UPAGES + u.u_dsize + u.u_ssize) > statbuf.st_size)
      {
	bfd_set_error (bfd_error_file_truncated);
	return NULL;
      }
    if (clicksz * (UPAGES + u.u_dsize + u.u_ssize) < statbuf.st_size)
      {
	/* The file is too big.  Maybe it's not a core file
	   or we otherwise have bad values for u_dsize and u_ssize).  */
	bfd_set_error (bfd_error_wrong_format);
	return NULL;
      }
  }

  /* OK, we believe you.  You're a core file (sure, sure).  */

  coredata = (struct hppabsd_core_struct *)
    bfd_zalloc (abfd, (bfd_size_type) sizeof (struct hppabsd_core_struct));
  if (!coredata)
    return NULL;

  /* Make the core data and available via the tdata part of the BFD.  */
  abfd->tdata.hppabsd_core_data = coredata;

  /* Create the sections.  */
  core_stacksec (abfd) = make_bfd_asection (abfd, ".stack",
					   SEC_ALLOC + SEC_HAS_CONTENTS,
					   clicksz * u.u_ssize,
					   NBPG * (USIZE + KSTAKSIZE)
					     + clicksz * u.u_dsize, 2);
  if (core_stacksec (abfd) == NULL)
    goto fail;
  core_stacksec (abfd)->vma = USRSTACK;

  core_datasec (abfd) = make_bfd_asection (abfd, ".data",
					  SEC_ALLOC + SEC_LOAD
					    + SEC_HAS_CONTENTS,
					  clicksz * u.u_dsize,
					  NBPG * (USIZE + KSTAKSIZE), 2);
  if (core_datasec (abfd) == NULL)
    goto fail;
  core_datasec (abfd)->vma = UDATASEG;

  core_regsec (abfd) = make_bfd_asection (abfd, ".reg",
					 SEC_HAS_CONTENTS,
					 KSTAKSIZE * NBPG,
					 NBPG * USIZE, 2);
  if (core_regsec (abfd) == NULL)
    goto fail;
  core_regsec (abfd)->vma = 0;

  strncpy (core_command (abfd), u.u_comm, MAXCOMLEN + 1);
  core_signal (abfd) = u.u_code;
  return abfd->xvec;

 fail:
  bfd_release (abfd, abfd->tdata.any);
  abfd->tdata.any = NULL;
  bfd_section_list_clear (abfd);
  return NULL;
}

static char *
hppabsd_core_core_file_failing_command (abfd)
     bfd *abfd;
{
  return core_command (abfd);
}

static int
hppabsd_core_core_file_failing_signal (abfd)
     bfd *abfd;
{
  return core_signal (abfd);
}

static bfd_boolean
hppabsd_core_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd, *exec_bfd;
{
  /* There's no way to know this...  */
  return TRUE;
}

/* If somebody calls any byte-swapping routines, shoot them.  */
static void
swap_abort ()
{
  /* This way doesn't require any declaration for ANSI to fuck up.  */
  abort ();
}

#define	NO_GET ((bfd_vma (*) (const void *)) swap_abort)
#define	NO_PUT ((void (*) (bfd_vma, void *)) swap_abort)
#define	NO_GETS ((bfd_signed_vma (*) (const void *)) swap_abort)
#define	NO_GET64 ((bfd_uint64_t (*) (const void *)) swap_abort)
#define	NO_PUT64 ((void (*) (bfd_uint64_t, void *)) swap_abort)
#define	NO_GETS64 ((bfd_int64_t (*) (const void *)) swap_abort)

const bfd_target hppabsd_core_vec =
  {
    "hppabsd-core",
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
    NO_GET64, NO_GETS64, NO_PUT64,	/* 64 bit data */
    NO_GET, NO_GETS, NO_PUT,		/* 32 bit data */
    NO_GET, NO_GETS, NO_PUT,		/* 16 bit data */
    NO_GET64, NO_GETS64, NO_PUT64,	/* 64 bit hdrs */
    NO_GET, NO_GETS, NO_PUT,		/* 32 bit hdrs */
    NO_GET, NO_GETS, NO_PUT,		/* 16 bit hdrs */

    {				/* bfd_check_format */
      _bfd_dummy_target,		/* unknown format */
      _bfd_dummy_target,		/* object file */
      _bfd_dummy_target,		/* archive */
      hppabsd_core_core_file_p		/* a core file */
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
    BFD_JUMP_TABLE_CORE (hppabsd_core),
    BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
    BFD_JUMP_TABLE_SYMBOLS (_bfd_nosymbols),
    BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
    BFD_JUMP_TABLE_WRITE (_bfd_generic),
    BFD_JUMP_TABLE_LINK (_bfd_nolink),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    NULL,

    (PTR) 0			/* backend_data */
  };
#endif
