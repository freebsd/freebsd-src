/* BFD back end for NetBSD style core files
   Copyright 1988, 89, 91, 92, 93, 96, 1998 Free Software Foundation, Inc.
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

struct netbsd_core_struct {
	struct core core;
} *rawptr;

/* forward declarations */

static const bfd_target *	netbsd_core_file_p PARAMS ((bfd *abfd));
static char *		netbsd_core_file_failing_command PARAMS ((bfd *abfd));
static int 		netbsd_core_file_failing_signal PARAMS ((bfd *abfd));
static boolean		netbsd_core_file_matches_executable_p
			 PARAMS ((bfd *core_bfd, bfd *exec_bfd));
static void		swap_abort PARAMS ((void));

/* Handle NetBSD-style core dump file.  */

/* ARGSUSED */
static const bfd_target *
netbsd_core_file_p (abfd)
     bfd *abfd;

{
	int		i, val, offset;
	asection	*asect, *asect2;
	struct core	core;
	struct coreseg	coreseg;

	val = bfd_read ((void *)&core, 1, sizeof core, abfd);
	if (val != sizeof core) {
		/* Too small to be a core file */
		bfd_set_error(bfd_error_wrong_format);
		return 0;
	}

	if (CORE_GETMAGIC(core) != COREMAGIC) {
		bfd_set_error(bfd_error_wrong_format);
		return 0;
	}

	rawptr = (struct netbsd_core_struct *)
		bfd_zalloc (abfd, sizeof (struct netbsd_core_struct));
	if (rawptr == NULL) {
		bfd_set_error(bfd_error_no_memory);
		return 0;
	}

	rawptr->core = core;
	abfd->tdata.netbsd_core_data = rawptr;

	offset = core.c_hdrsize;
	for (i = 0; i < core.c_nseg; i++) {

		if (bfd_seek (abfd, offset, SEEK_SET) != 0)
			goto punt;

		val = bfd_read ((void *)&coreseg, 1, sizeof coreseg, abfd);
		if (val != sizeof coreseg) {
			bfd_set_error(bfd_error_file_truncated);
			goto punt;
		}
		if (CORE_GETMAGIC(coreseg) != CORESEGMAGIC) {
			bfd_set_error(bfd_error_wrong_format);
			goto punt;
		}

		offset += core.c_seghdrsize;

		asect = (asection *) bfd_zalloc (abfd, sizeof (asection));
		if (asect == NULL) {
			bfd_set_error(bfd_error_no_memory);
			goto punt;
		}

		asect->_raw_size = coreseg.c_size;
		asect->vma = coreseg.c_addr;
		asect->filepos = offset;
		asect->alignment_power = 2;
		asect->next = abfd->sections;
		abfd->sections = asect;
		abfd->section_count++;
		offset += coreseg.c_size;

		switch (CORE_GETFLAG(coreseg)) {
		case CORE_CPU:
			asect->name = ".reg";
			asect->flags = SEC_ALLOC + SEC_HAS_CONTENTS;
#ifdef CORE_FPU_OFFSET
			/* Hackish...  */
			asect->_raw_size = CORE_FPU_OFFSET;
			asect2 = (asection *)bfd_zalloc (abfd,
							 sizeof (asection));
			if (asect2 == NULL) {
				bfd_set_error(bfd_error_no_memory);
				goto punt;
			}
			asect2->_raw_size = coreseg.c_size - CORE_FPU_OFFSET;
			asect2->vma = 0;
			asect2->filepos = asect->filepos + CORE_FPU_OFFSET;
			asect2->alignment_power = 2;
			asect2->next = abfd->sections;
			asect2->name = ".reg2";
			asect2->flags = SEC_ALLOC + SEC_HAS_CONTENTS;
			abfd->sections = asect2;
			abfd->section_count++;
#endif

			break;
		case CORE_DATA:
			asect->name = ".data";
			asect->flags = SEC_ALLOC+SEC_LOAD+SEC_HAS_CONTENTS;
			break;
		case CORE_STACK:
			asect->name = ".stack";
			asect->flags = SEC_ALLOC+SEC_LOAD+SEC_HAS_CONTENTS;
			break;
		}
	}

	/* OK, we believe you.  You're a core file (sure, sure).  */
	return abfd->xvec;

punt:	{
		asection	*anext;
		for (asect = abfd->sections; asect; asect = anext) {
			anext = asect->next;
			free((void *)asect);
		}
	}
	free ((void *)rawptr);
	abfd->tdata.netbsd_core_data = NULL;
	abfd->sections = NULL;
	abfd->section_count = 0;
	return 0;
}

static char*
netbsd_core_file_failing_command (abfd)
	bfd *abfd;
{
 /*return core_command (abfd);*/
  return abfd->tdata.netbsd_core_data->core.c_name;
}

/* ARGSUSED */
static int
netbsd_core_file_failing_signal (abfd)
	bfd *abfd;
{
  /*return core_signal (abfd);*/
  return abfd->tdata.netbsd_core_data->core.c_signo;
}

/* ARGSUSED */
static boolean
netbsd_core_file_matches_executable_p  (core_bfd, exec_bfd)
     bfd *core_bfd, *exec_bfd;
{
  return true;		/* FIXME, We have no way of telling at this point */
}

/* If somebody calls any byte-swapping routines, shoot them.  */
static void
swap_abort ()
{
  abort (); /* This way doesn't require any declaration for ANSI to fuck up */
}
#define	NO_GET	((bfd_vma (*) PARAMS ((   const bfd_byte *))) swap_abort )
#define	NO_PUT	((void    (*) PARAMS ((bfd_vma, bfd_byte *))) swap_abort )
#define	NO_SIGNED_GET \
  ((bfd_signed_vma (*) PARAMS ((const bfd_byte *))) swap_abort )

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
