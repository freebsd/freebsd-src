/* BFD back-end for Irix core files.
   Copyright 1993, 1994 Free Software Foundation, Inc.
   Written by Stu Grossman, Cygnus Support.
   Converted to back-end form by Ian Lance Taylor, Cygnus Support

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

/* This file can only be compiled on systems which use Irix style core
   files (namely, Irix 4 and Irix 5, so far).  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#ifdef IRIX_CORE

#include <core.out.h>

struct sgi_core_struct 
{
  int sig;
  char cmd[CORE_NAMESIZE];
};

#define core_hdr(bfd) ((bfd)->tdata.sgi_core_data)
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
  asect->alignment_power = 4;

  return asect;
}

static const bfd_target *
irix_core_core_file_p (abfd)
     bfd *abfd;
{
  int val;
  int i;
  char *secname;
  struct coreout coreout;
  struct idesc *idg, *idf, *ids;

  val = bfd_read ((PTR)&coreout, 1, sizeof coreout, abfd);
  if (val != sizeof coreout)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  if (coreout.c_magic != CORE_MAGIC
      || coreout.c_version != CORE_VERSION1)
    return 0;

  core_hdr (abfd) = (struct sgi_core_struct *) bfd_zalloc (abfd, sizeof (struct sgi_core_struct));
  if (!core_hdr (abfd))
    return NULL;

  strncpy (core_command (abfd), coreout.c_name, CORE_NAMESIZE);
  core_signal (abfd) = coreout.c_sigcause;

  if (bfd_seek (abfd, coreout.c_vmapoffset, SEEK_SET) != 0)
    return NULL;

  for (i = 0; i < coreout.c_nvmap; i++)
    {
      struct vmap vmap;

      val = bfd_read ((PTR)&vmap, 1, sizeof vmap, abfd);
      if (val != sizeof vmap)
	break;

      switch (vmap.v_type)
	{
	case VDATA:
	  secname = ".data";
	  break;
	case VSTACK:
	  secname = ".stack";
	  break;
#ifdef VMAPFILE
	case VMAPFILE:
	  secname = ".mapfile";
	  break;
#endif
	default:
	  continue;
	}

      /* A file offset of zero means that the section is not contained
	 in the corefile.  */
      if (vmap.v_offset == 0)
	continue;

      if (!make_bfd_asection (abfd, secname,
			      SEC_ALLOC+SEC_LOAD+SEC_HAS_CONTENTS,
			      vmap.v_len,
			      vmap.v_vaddr,
			      vmap.v_offset,
			      2))
	return NULL;
    }

  /* Make sure that the regs are contiguous within the core file. */

  idg = &coreout.c_idesc[I_GPREGS];
  idf = &coreout.c_idesc[I_FPREGS];
  ids = &coreout.c_idesc[I_SPECREGS];

  if (idg->i_offset + idg->i_len != idf->i_offset
      || idf->i_offset + idf->i_len != ids->i_offset)
    return 0;			/* Can't deal with non-contig regs */

  if (bfd_seek (abfd, idg->i_offset, SEEK_SET) != 0)
    return NULL;

  make_bfd_asection (abfd, ".reg",
		     SEC_HAS_CONTENTS,
		     idg->i_len + idf->i_len + ids->i_len,
		     0,
		     idg->i_offset);

  /* OK, we believe you.  You're a core file (sure, sure).  */

  return abfd->xvec;
}

static char *
irix_core_core_file_failing_command (abfd)
     bfd *abfd;
{
  return core_command (abfd);
}

static int
irix_core_core_file_failing_signal (abfd)
     bfd *abfd;
{
  return core_signal (abfd);
}

static boolean
irix_core_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd, *exec_bfd;
{
  return true;			/* XXX - FIXME */
}

static asymbol *
irix_core_make_empty_symbol (abfd)
     bfd *abfd;
{
  asymbol *new = (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
  if (new)
    new->the_bfd = abfd;
  return new;
}

#define irix_core_get_symtab_upper_bound _bfd_nosymbols_get_symtab_upper_bound
#define irix_core_get_symtab _bfd_nosymbols_get_symtab
#define irix_core_print_symbol _bfd_nosymbols_print_symbol
#define irix_core_get_symbol_info _bfd_nosymbols_get_symbol_info
#define irix_core_bfd_is_local_label _bfd_nosymbols_bfd_is_local_label
#define irix_core_get_lineno _bfd_nosymbols_get_lineno
#define irix_core_find_nearest_line _bfd_nosymbols_find_nearest_line
#define irix_core_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define irix_core_read_minisymbols _bfd_nosymbols_read_minisymbols
#define irix_core_minisymbol_to_symbol _bfd_nosymbols_minisymbol_to_symbol

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

const bfd_target irix_core_vec =
  {
    "irix-core",
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
     irix_core_core_file_p	/* a core file */
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
       BFD_JUMP_TABLE_CORE (irix_core),
       BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
       BFD_JUMP_TABLE_SYMBOLS (irix_core),
       BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
       BFD_JUMP_TABLE_WRITE (_bfd_generic),
       BFD_JUMP_TABLE_LINK (_bfd_nolink),
       BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    (PTR) 0			/* backend_data */
};

#endif /* IRIX_CORE */
