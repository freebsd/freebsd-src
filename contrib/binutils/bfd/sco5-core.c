/* BFD back end for SCO5 core files (U-area and raw sections)
   Copyright 1998 Free Software Foundation, Inc.
   Written by Jouke Numan <jnuman@hiscom.nl>

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

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>

#include <sys/user.h>           /* After a.out.h  */
#include <sys/paccess.h>
#include <sys/region.h>

struct sco5_core_struct
{
  struct user u;
};

/* forward declarations */

static asection *
make_bfd_asection PARAMS ((bfd *, const char *, flagword, bfd_size_type,
                           bfd_vma, file_ptr));
static asymbol *sco5_core_make_empty_symbol PARAMS ((bfd *));
static struct user *read_uarea PARAMS ((bfd *, int));
const bfd_target *sco5_core_file_p PARAMS ((bfd *abfd));
char *sco5_core_file_failing_command PARAMS ((bfd *abfd));
int sco5_core_file_failing_signal PARAMS ((bfd *abfd));
boolean sco5_core_file_matches_executable_p PARAMS ((bfd *core_bfd,
						     bfd *exec_bfd));
static void swap_abort PARAMS ((void));

static asection *
make_bfd_asection (abfd, name, flags, _raw_size, vma, filepos)
     bfd *abfd;
     const char *name;
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
  asect->alignment_power = 2;

  return asect;
}

static asymbol *
sco5_core_make_empty_symbol (abfd)
     bfd *abfd;
{
  asymbol *new = (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
  if (new)
    new->the_bfd = abfd;
  return new;
}

static struct user *
read_uarea(abfd, filepos)
     bfd *abfd;
     int filepos;

{
  struct sco5_core_struct *rawptr;

  rawptr = ((struct sco5_core_struct *)
	    bfd_zmalloc (sizeof (struct sco5_core_struct)));
  if (rawptr == NULL)
    return NULL;

  abfd->tdata.sco5_core_data = rawptr;

  if ((bfd_seek (abfd, filepos, SEEK_SET) != 0)
      || (bfd_read ((void *)&rawptr->u, 1, sizeof rawptr->u, abfd)
	  != sizeof rawptr->u))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* Sanity check perhaps??? */
  if (rawptr->u.u_dsize > 0x1000000)    /* Remember, it's in pages...  */
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }
  if (rawptr->u.u_ssize > 0x1000000)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }
  return &rawptr->u;
}

/* ARGSUSED */
const bfd_target *
sco5_core_file_p (abfd)
     bfd *abfd;
{
  int coffset_siz, val, nsecs, cheadoffs;
  int coresize;
  struct user *u;
  struct coreoffsets coffsets;
  struct coresecthead chead;
  char *secname;
  flagword flags;

  /* Read coreoffsets region at end of core (see core(FP)) */

  {
    FILE *stream = bfd_cache_lookup (abfd);
    struct stat statbuf;
    if (stream == NULL)
      return NULL;
    if (fstat (fileno (stream), &statbuf) < 0)
      {
        bfd_set_error (bfd_error_system_call);
        return NULL;
      }
    coresize = statbuf.st_size;
  }
  /* Last long in core is sizeof struct coreoffsets, read it */
  if ((bfd_seek (abfd, coresize-sizeof coffset_siz, SEEK_SET) != 0)
      || (bfd_read ((void *)&coffset_siz, 1, sizeof coffset_siz, abfd)
	  != sizeof coffset_siz) )
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* Use it to seek start of coreoffsets region, read it and determine
     validity */
  if ((bfd_seek (abfd, coresize-coffset_siz, SEEK_SET) != 0)
      || (bfd_read ((void *)&coffsets, 1, sizeof coffsets, abfd)
	  != sizeof coffsets)
      || ((coffsets.u_info != 1) && (coffsets.u_info != C_VERSION)))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (coffsets.u_info == 1)
    {
      /* Old version, no section heads, read info from user struct */

      u = read_uarea(abfd, coffsets.u_user);
      if (! u)
          return NULL;

      if (!make_bfd_asection (abfd, ".reg", SEC_HAS_CONTENTS,
                              (bfd_size_type) coffsets.u_usize,
                              0 - (bfd_vma) u->u_ar0,
                              (file_ptr) coffsets.u_user))
        return NULL;

      if (!make_bfd_asection (abfd, ".data",
			      SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS,
                              ((bfd_size_type) u->u_exdata.ux_dsize
			       + u->u_exdata.ux_bsize),
                              (bfd_vma) u->u_exdata.ux_datorg,
                              (file_ptr) coffsets.u_data))
        return NULL;

      if (!make_bfd_asection (abfd, ".stack",
			      SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS,
                              (bfd_size_type) u->u_ssize * NBPC,
                              (bfd_vma) u->u_sub,
                              (file_ptr) coffsets.u_stack))
        return NULL;

      return abfd->xvec;		/* Done for version 1 */
    }

  /* Immediately before coreoffsets region is a long with offset in core
     to first coresecthead (CORES_OFFSETS), the long before this is the
     number of section heads in the list. Read both longs and read the
     coresecthead and check its validity */

  if ((bfd_seek (abfd,
		 coresize - coffset_siz - 2 * sizeof coffset_siz,
		 SEEK_SET) != 0)
      || (bfd_read ((void *)&nsecs, 1, sizeof nsecs, abfd) != sizeof nsecs)
      || (bfd_read ((void *)&cheadoffs, 1, sizeof cheadoffs, abfd)
	  != sizeof cheadoffs)
      || (bfd_seek (abfd, cheadoffs, SEEK_SET) != 0)
      || (bfd_read ((void *)&chead, 1, sizeof chead, abfd) != sizeof chead)
      || (chead.cs_stype != CORES_OFFSETS)
      || (chead.cs_x.csx_magic != COREMAGIC_NUMBER))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* OK, we believe you.  You're a core file (sure, sure).  */

  /* Now loop over all regions and map them */
  nsecs--;				/* We've seen CORES_OFFSETS already */
  for (; nsecs; nsecs--)
    {
      if ((bfd_seek (abfd, chead.cs_hseek, SEEK_SET) != 0)
	  || bfd_read ((void *)&chead, 1, sizeof chead, abfd) != sizeof chead)
        {
          bfd_set_error (bfd_error_wrong_format);
          return NULL;
        }

      switch (chead.cs_stype)
	{
	case CORES_MAGIC:			/* Core header, check magic */
	  if (chead.cs_x.csx_magic != COREMAGIC_NUMBER)
	    {
	      bfd_set_error (bfd_error_wrong_format);
	      return NULL;
	    }
	  secname = NULL;
	  nsecs++;				/* MAGIC not in section cnt!*/
	  break;
	case CORES_UAREA:			/* U-area, read in tdata */
	  u = read_uarea(abfd, chead.cs_sseek);
	  if (! u)
	    return NULL;

          /* This is tricky.  As the "register section", we give them
	     the entire upage and stack.  u.u_ar0 points to where
	     "register 0" is stored.  There are two tricks with this,
	     though.  One is that the rest of the registers might be
	     at positive or negative (or both) displacements from
	     *u_ar0.  The other is that u_ar0 is sometimes an absolute
	     address in kernel memory, and on other systems it is an
	     offset from the beginning of the `struct user'.

	     As a practical matter, we don't know where the registers
	     actually are, so we have to pass the whole area to GDB.
	     We encode the value of u_ar0 by setting the .regs section
	     up so that its virtual memory address 0 is at the place
	     pointed to by u_ar0 (by setting the vma of the start of
	     the section to -u_ar0).  GDB uses this info to locate the
	     regs, using minor trickery to get around the
	     offset-or-absolute-addr problem.  */

	  chead.cs_vaddr = 0 - (bfd_vma) u->u_ar0;

          secname = ".reg";
          flags = SEC_HAS_CONTENTS;

          break;
	case CORES_PREGION:			/* A program region, map it */
	  switch (chead.cs_x.csx_preg.csxp_rtyp)
	    {
	    case PT_DATA:
	      secname = ".data";  	/* Data region.          */
	      break;
	    case PT_STACK:
	      secname = ".stack"; 	/* Stack region.         */
	      break;
	    case PT_SHMEM:
	      secname = ".shmem"; 	/* Shared memory         */
	      break;
	    case PT_LIBDAT:
	      secname = ".libdat";	/* Shared library data   */
	      break;
	    case PT_V86:
	      secname = ".virt86";	/* Virtual 8086 mode     */
	      break;
	    case PT_SHFIL:
	      secname = ".mmfile";	/* Memory mapped file    */
	      break;
	    case PT_XDATA0:
	      secname = ".Xdat0"; 	/* XENIX data region, virtual 0 */
	      break;
	    default:
	      secname = "";
	    }
	  flags = SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS;
	  break;
	case CORES_PROC:			/* struct proc */
	case CORES_ITIMER:			/* interval timers */
	case CORES_SCOUTSNAME:			/* struct scoutsname */
	  secname = NULL;	/* Ignore these */
	  break;
        default:
          (*_bfd_error_handler) ("Unhandled SCO core file section type %d\n",
                                 chead.cs_stype);
          continue;
        }

      if (secname
	  && !make_bfd_asection (abfd, secname, flags,
				 (bfd_size_type) chead.cs_vsize,
				 (bfd_vma) chead.cs_vaddr,
				 (file_ptr) chead.cs_sseek))
        return NULL;

    }

  return abfd->xvec;

}

char *
sco5_core_file_failing_command (abfd)
     bfd *abfd;
{
  char *com = abfd->tdata.sco5_core_data->u.u_comm;
  if (*com)
    return com;
  else
    return NULL;
}

/* ARGSUSED */
int
sco5_core_file_failing_signal (ignore_abfd)
     bfd *ignore_abfd;
{
  return ((ignore_abfd->tdata.sco5_core_data->u.u_sysabort != 0)
	  ? ignore_abfd->tdata.sco5_core_data->u.u_sysabort
	  : -1);
}

/* ARGSUSED */
boolean
sco5_core_file_matches_executable_p  (core_bfd, exec_bfd)
     bfd *core_bfd, *exec_bfd;
{
  return true;          /* FIXME, We have no way of telling at this point */
}

#define sco5_core_get_symtab_upper_bound _bfd_nosymbols_get_symtab_upper_bound
#define sco5_core_get_symtab _bfd_nosymbols_get_symtab
#define sco5_core_print_symbol _bfd_nosymbols_print_symbol
#define sco5_core_get_symbol_info _bfd_nosymbols_get_symbol_info
#define sco5_core_bfd_is_local_label_name _bfd_nosymbols_bfd_is_local_label_name
#define sco5_core_get_lineno _bfd_nosymbols_get_lineno
#define sco5_core_find_nearest_line _bfd_nosymbols_find_nearest_line
#define sco5_core_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define sco5_core_read_minisymbols _bfd_nosymbols_read_minisymbols
#define sco5_core_minisymbol_to_symbol _bfd_nosymbols_minisymbol_to_symbol

/* If somebody calls any byte-swapping routines, shoot them.  */
static void
swap_abort ()
{
  abort (); /* This way doesn't require any declaration for ANSI to fuck up */
}
#define NO_GET  ((bfd_vma (*) PARAMS ((   const bfd_byte *))) swap_abort )
#define NO_PUT  ((void    (*) PARAMS ((bfd_vma, bfd_byte *))) swap_abort )
#define NO_SIGNED_GET \
  ((bfd_signed_vma (*) PARAMS ((const bfd_byte *))) swap_abort )

const bfd_target sco5_core_vec =
  {
    "sco5-core",
    bfd_target_unknown_flavour,
    BFD_ENDIAN_LITTLE,         /* target byte order */
    BFD_ENDIAN_LITTLE,         /* target headers byte order */
    (HAS_RELOC | EXEC_P |       /* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    0,                                                     /* symbol prefix */
    ' ',                                                   /* ar_pad_char */
    16,                                                    /* ar_max_namelen */
    NO_GET, NO_SIGNED_GET, NO_PUT,      /* 64 bit data */
    NO_GET, NO_SIGNED_GET, NO_PUT,      /* 32 bit data */
    NO_GET, NO_SIGNED_GET, NO_PUT,      /* 16 bit data */
    NO_GET, NO_SIGNED_GET, NO_PUT,      /* 64 bit hdrs */
    NO_GET, NO_SIGNED_GET, NO_PUT,      /* 32 bit hdrs */
    NO_GET, NO_SIGNED_GET, NO_PUT,      /* 16 bit hdrs */

    {                           /* bfd_check_format */
     _bfd_dummy_target,         /* unknown format */
     _bfd_dummy_target,         /* object file */
     _bfd_dummy_target,         /* archive */
     sco5_core_file_p      	/* a core file */
    },
    {                           /* bfd_set_format */
     bfd_false, bfd_false,
     bfd_false, bfd_false
    },
    {                           /* bfd_write_contents */
     bfd_false, bfd_false,
     bfd_false, bfd_false
    },

       BFD_JUMP_TABLE_GENERIC (_bfd_generic),
       BFD_JUMP_TABLE_COPY (_bfd_generic),
       BFD_JUMP_TABLE_CORE (sco5),
       BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
       BFD_JUMP_TABLE_SYMBOLS (sco5_core),
       BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
       BFD_JUMP_TABLE_WRITE (_bfd_generic),
       BFD_JUMP_TABLE_LINK (_bfd_nolink),
       BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    NULL,

    (PTR) 0                     /* backend_data */
};
