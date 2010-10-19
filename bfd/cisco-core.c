/* BFD back-end for CISCO crash dumps.
   Copyright 1994, 1997, 1999, 2000, 2001, 2002, 2004
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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
/* core_file_failing_signal returns a host signal (this probably should
   be fixed).  */
#include <signal.h>

/* for MSVC builds */
#ifndef SIGTRAP
# define SIGTRAP 5
#endif
#ifndef SIGEMT
# define SIGEMT 6
#endif
#ifndef SIGBUS
# define SIGBUS 10
#endif

int crash_info_locs[] = {
  0x0250,	/* mips, ppc, x86, i960 */
  0x0400,	/* m68k, mips, x86, i960 */
  0x0FFC,	/* m68k, mips, ppc, x86, i960 */
  0x3000,	/* ppc */
  0x4FFC,	/* m68k */
  -1
};

#define CRASH_MAGIC	0xdead1234
#define MASK_ADDR(x)	((x) & 0x0fffffff)	/* Mask crash info address */

typedef enum {
    CRASH_REASON_NOTCRASHED = 0,
    CRASH_REASON_EXCEPTION = 1,
    CRASH_REASON_CORRUPT = 2,
} crashreason;

typedef struct {
  char magic[4];		/* Magic number */
  char version[4];		/* Version number */
  char reason[4];		/* Crash reason */
  char cpu_vector[4];		/* CPU vector for exceptions */
  char registers[4];		/* Pointer to saved registers */
  char rambase[4];		/* Base of RAM (not in V1 crash info) */
  char textbase[4];		/* Base of .text section (not in V3 crash info) */
  char database[4];		/* Base of .data section (not in V3 crash info) */
  char bssbase[4];		/* Base of .bss section (not in V3 crash info) */
} crashinfo_external;

struct cisco_core_struct
{
  int sig;
};

static const bfd_target *cisco_core_file_validate PARAMS ((bfd *, int));
static const bfd_target *cisco_core_file_p PARAMS ((bfd *));
char *cisco_core_file_failing_command PARAMS ((bfd *));
int cisco_core_file_failing_signal PARAMS ((bfd *));
#define cisco_core_file_matches_executable_p generic_core_file_matches_executable_p

/* Examine the file for a crash info struct at the offset given by
   CRASH_INFO_LOC.  */

static const bfd_target *
cisco_core_file_validate (abfd, crash_info_loc)
     bfd *abfd;
     int crash_info_loc;
{
  char buf[4];
  unsigned int crashinfo_offset;
  crashinfo_external crashinfo;
  int nread;
  unsigned int magic;
  unsigned int version;
  unsigned int rambase;
  sec_ptr asect;
  struct stat statbuf;
  bfd_size_type amt;

  if (bfd_seek (abfd, (file_ptr) crash_info_loc, SEEK_SET) != 0)
    return NULL;

  nread = bfd_bread (buf, (bfd_size_type) 4, abfd);
  if (nread != 4)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }
  crashinfo_offset = MASK_ADDR (bfd_get_32 (abfd, buf));

  if (bfd_seek (abfd, (file_ptr) crashinfo_offset, SEEK_SET) != 0)
    {
      /* Most likely we failed because of a bogus (huge) offset */
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  nread = bfd_bread (&crashinfo, (bfd_size_type) sizeof (crashinfo), abfd);
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

  magic = bfd_get_32 (abfd, crashinfo.magic);
  if (magic != CRASH_MAGIC)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  version = bfd_get_32 (abfd, crashinfo.version);
  if (version == 0)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }
  else if (version == 1)
    {
      /* V1 core dumps don't specify the dump base, assume 0 */
      rambase = 0;
    }
  else
    {
      rambase = bfd_get_32 (abfd, crashinfo.rambase);
    }

  /* OK, we believe you.  You're a core file.  */

  amt = sizeof (struct cisco_core_struct);
  abfd->tdata.cisco_core_data = (struct cisco_core_struct *) bfd_zmalloc (amt);
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
#ifndef SIGEMT
#define SIGEMT SIGTRAP
#endif
	  /* "software generated"*/
	  abfd->tdata.cisco_core_data->sig = SIGEMT;
	}
      break;
    default:
      /* Unknown crash reason.  */
      abfd->tdata.cisco_core_data->sig = 0;
      break;
    }

  /* Create a ".data" section that maps the entire file, which is
     essentially a dump of the target system's RAM.  */

  asect = bfd_make_section_anyway (abfd, ".data");
  if (asect == NULL)
    goto error_return;
  asect->flags = SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS;
  /* The size of memory is the size of the core file itself.  */
  asect->size = statbuf.st_size;
  asect->vma = rambase;
  asect->filepos = 0;

  /* Create a ".crash" section to allow access to the saved
     crash information.  */

  asect = bfd_make_section_anyway (abfd, ".crash");
  if (asect == NULL)
    goto error_return;
  asect->flags = SEC_HAS_CONTENTS;
  asect->vma = 0;
  asect->filepos = crashinfo_offset;
  asect->size = sizeof (crashinfo);

  /* Create a ".reg" section to allow access to the saved
     registers.  */

  asect = bfd_make_section_anyway (abfd, ".reg");
  if (asect == NULL)
    goto error_return;
  asect->flags = SEC_HAS_CONTENTS;
  asect->vma = 0;
  asect->filepos = bfd_get_32 (abfd, crashinfo.registers) - rambase;
  /* Since we don't know the exact size of the saved register info,
     choose a register section size that is either the remaining part
     of the file, or 1024, whichever is smaller.  */
  nread = statbuf.st_size - asect->filepos;
  asect->size = (nread < 1024) ? nread : 1024;

  return abfd->xvec;

  /* Get here if we have already started filling out the BFD
     and there is an error of some kind.  */

 error_return:
  bfd_release (abfd, abfd->tdata.any);
  abfd->tdata.any = NULL;
  bfd_section_list_clear (abfd);
  return NULL;
}

static const bfd_target *
cisco_core_file_p (abfd)
     bfd *abfd;
{
  int *crash_info_locp;
  const bfd_target *target = NULL;

  for (crash_info_locp = crash_info_locs;
       *crash_info_locp != -1  &&  target == NULL;
       crash_info_locp++)
    {
      target = cisco_core_file_validate (abfd, *crash_info_locp);
    }
  return (target);
}

char *
cisco_core_file_failing_command (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
  return NULL;
}

int
cisco_core_file_failing_signal (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
  return abfd->tdata.cisco_core_data->sig;
}

extern const bfd_target cisco_core_little_vec;

const bfd_target cisco_core_big_vec =
  {
    "cisco-ios-core-big",
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

    & cisco_core_little_vec,

    (PTR) 0			/* backend_data */
};

const bfd_target cisco_core_little_vec =
  {
    "cisco-ios-core-little",
    bfd_target_unknown_flavour,
    BFD_ENDIAN_LITTLE,		/* target byte order */
    BFD_ENDIAN_LITTLE,		/* target headers byte order */
    (HAS_RELOC | EXEC_P |	/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    0,			                                   /* symbol prefix */
    ' ',						   /* ar_pad_char */
    16,							   /* ar_max_namelen */
    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    bfd_getl32, bfd_getl_signed_32, bfd_putl32,
    bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    bfd_getl32, bfd_getl_signed_32, bfd_putl32,
    bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

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

    &cisco_core_big_vec,

    (PTR) 0			/* backend_data */
};
