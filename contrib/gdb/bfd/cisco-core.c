/* BFD back-end for CISCO crash dumps.

Copyright 1994 Free Software Foundation, Inc.

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
/* core_file_failing_signal returns a host signal (this probably should
   be fixed).  */
#include <signal.h>

#define CRASH_INFO (0xffc)
#define CRASH_MAGIC 0xdead1234

typedef enum {
    CRASH_REASON_NOTCRASHED = 0,
    CRASH_REASON_EXCEPTION = 1,
    CRASH_REASON_CORRUPT = 2,
  } crashreason;

struct crashinfo_external
{
  char magic[4];			/* Magic number */
  char version[4];		/* Version number */
  char reason[4];		/* Crash reason */
  char cpu_vector[4];		/* CPU vector for exceptions */
  char registers[4];		/* Pointer to saved registers */
  char rambase[4];		/* Base of RAM (not in V1 crash info) */
};

struct cisco_core_struct
{
  int sig;
};

static const bfd_target *
cisco_core_file_p (abfd)
     bfd *abfd;
{
  char buf[4];
  unsigned int crashinfo_offset;
  struct crashinfo_external crashinfo;
  int nread;
  unsigned int rambase;
  sec_ptr asect;
  struct stat statbuf;

  if (bfd_seek (abfd, CRASH_INFO, SEEK_SET) != 0)
    return NULL;

  nread = bfd_read (buf, 1, 4, abfd);
  if (nread != 4)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }
  crashinfo_offset = bfd_get_32 (abfd, buf);

  if (bfd_seek (abfd, crashinfo_offset, SEEK_SET) != 0)
    return NULL;

  nread = bfd_read (&crashinfo, 1, sizeof (crashinfo), abfd);
  if (nread != sizeof (crashinfo))
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (bfd_stat (abfd, &statbuf) < 0)
    {
      bfd_set_error (bfd_error_system_call);
      return NULL;
    }

  if (bfd_get_32 (abfd, crashinfo.magic) != CRASH_MAGIC)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  switch (bfd_get_32 (abfd, crashinfo.version))
    {
    case 0:
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    case 1:
      rambase = 0;
      break;
    default:
    case 2:
      rambase = bfd_get_32 (abfd, crashinfo.rambase);
      break;
    }

  /* OK, we believe you.  You're a core file.  */

  abfd->tdata.cisco_core_data =
    ((struct cisco_core_struct *)
     bfd_zmalloc (sizeof (struct cisco_core_struct)));
  if (abfd->tdata.cisco_core_data == NULL)
    return NULL;

  switch ((crashreason) bfd_get_32 (abfd, crashinfo.reason))
    {
    case CRASH_REASON_NOTCRASHED:
      /* Crash file probably came from write core.  */
      abfd->tdata.cisco_core_data->sig = 0;
      break;
    case CRASH_REASON_CORRUPT:
      /* The crash context area was corrupt -- proceed with caution.
	 We have no way of passing this information back to the caller.  */
      abfd->tdata.cisco_core_data->sig = 0;
      break;
    case CRASH_REASON_EXCEPTION:
      /* Crash occured due to CPU exception.  */

      /* This is 68k-specific; for MIPS we'll need to interpret
	 cpu_vector differently based on the target configuration
	 (since CISCO core files don't seem to have the processor
	 encoded in them).  */

      switch (bfd_get_32 (abfd, crashinfo.cpu_vector))
	{
	   /* bus error           */
	case 2 : abfd->tdata.cisco_core_data->sig = SIGBUS; break;
	   /* address error       */
	case 3 : abfd->tdata.cisco_core_data->sig = SIGBUS; break;
	   /* illegal instruction */
	case 4 : abfd->tdata.cisco_core_data->sig = SIGILL;  break;
	   /* zero divide         */
	case 5 : abfd->tdata.cisco_core_data->sig = SIGFPE;  break;
	   /* chk instruction     */
	case 6 : abfd->tdata.cisco_core_data->sig = SIGFPE; break;
	   /* trapv instruction   */
	case 7 : abfd->tdata.cisco_core_data->sig = SIGFPE; break;
	   /* privilege violation */
	case 8 : abfd->tdata.cisco_core_data->sig = SIGSEGV; break;
	   /* trace trap          */
	case 9 : abfd->tdata.cisco_core_data->sig = SIGTRAP;  break;
	   /* line 1010 emulator  */
	case 10: abfd->tdata.cisco_core_data->sig = SIGILL;  break;
	   /* line 1111 emulator  */
	case 11: abfd->tdata.cisco_core_data->sig = SIGILL;  break;

	  /* Coprocessor protocol violation.  Using a standard MMU or FPU
	     this cannot be triggered by software.  Call it a SIGBUS.  */
	case 13: abfd->tdata.cisco_core_data->sig = SIGBUS;  break;

	  /* interrupt           */
	case 31: abfd->tdata.cisco_core_data->sig = SIGINT;  break;
	  /* breakpoint          */
	case 33: abfd->tdata.cisco_core_data->sig = SIGTRAP;  break;

	  /* floating point err  */
	case 48: abfd->tdata.cisco_core_data->sig = SIGFPE;  break;
	  /* floating point err  */
	case 49: abfd->tdata.cisco_core_data->sig = SIGFPE;  break;
	  /* zero divide         */
	case 50: abfd->tdata.cisco_core_data->sig = SIGFPE;  break;
	  /* underflow           */
	case 51: abfd->tdata.cisco_core_data->sig = SIGFPE;  break;
	  /* operand error       */
	case 52: abfd->tdata.cisco_core_data->sig = SIGFPE;  break;
	   /* overflow            */
	case 53: abfd->tdata.cisco_core_data->sig = SIGFPE;  break;
	  /* NAN                 */
	case 54: abfd->tdata.cisco_core_data->sig = SIGFPE;  break;
	default:
	  /* "software generated"*/
	  abfd->tdata.cisco_core_data->sig = SIGEMT;
	}
      break;
    default:
      /* Unknown crash reason.  */
      abfd->tdata.cisco_core_data->sig = 0;
      break;
    }

  abfd->sections = NULL;
  abfd->section_count = 0;

  asect = (asection *) bfd_zmalloc (sizeof (asection));
  if (asect == NULL)
    goto error_return;
  asect->name = ".reg";
  asect->flags = SEC_HAS_CONTENTS;
  /* This can be bigger than the real size.  Set it to the size of the whole
     core file.  */
  asect->_raw_size = statbuf.st_size;
  asect->vma = 0;
  asect->filepos = bfd_get_32 (abfd, crashinfo.registers) - rambase;
  asect->next = abfd->sections;
  abfd->sections = asect;
  ++abfd->section_count;

  /* There is only one section containing data from the target system's RAM.
     We call it .data.  */
  asect = (asection *) bfd_zmalloc (sizeof (asection));
  if (asect == NULL)
    goto error_return;
  asect->name = ".data";
  asect->flags = SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS;
  /* The size of memory is the size of the core file itself.  */
  asect->_raw_size = statbuf.st_size;
  asect->vma = rambase;
  asect->filepos = 0;
  asect->next = abfd->sections;
  abfd->sections = asect;
  ++abfd->section_count;

  return abfd->xvec;

 error_return:
  {
    sec_ptr nextsect;
    for (asect = abfd->sections; asect != NULL;)
      {
	nextsect = asect->next;
	free (asect);
	asect = nextsect;
      }
    free (abfd->tdata.cisco_core_data);
    return NULL;
  }
}

char *
cisco_core_file_failing_command (abfd)
     bfd *abfd;
{
  return NULL;
}

int
cisco_core_file_failing_signal (abfd)
     bfd *abfd;
{
  return abfd->tdata.cisco_core_data->sig;
}

boolean
cisco_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd;
     bfd *exec_bfd;
{
  return true;
}

const bfd_target cisco_core_vec =
  {
    "trad-core",
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
    bfd_getb64, bfd_getb_signed_64, bfd_putb64,
    bfd_getb32, bfd_getb_signed_32, bfd_putb32,
    bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
    bfd_getb64, bfd_getb_signed_64, bfd_putb64,
    bfd_getb32, bfd_getb_signed_32, bfd_putb32,
    bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

    {				/* bfd_check_format */
     _bfd_dummy_target,		/* unknown format */
     _bfd_dummy_target,		/* object file */
     _bfd_dummy_target,		/* archive */
     cisco_core_file_p	/* a core file */
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
       BFD_JUMP_TABLE_CORE (cisco),
       BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
       BFD_JUMP_TABLE_SYMBOLS (_bfd_nosymbols),
       BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
       BFD_JUMP_TABLE_WRITE (_bfd_generic),
       BFD_JUMP_TABLE_LINK (_bfd_nolink),
       BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    (PTR) 0			/* backend_data */
};
