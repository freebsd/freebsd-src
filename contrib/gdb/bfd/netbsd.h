/* BFD back-end definitions used by all NetBSD targets.
   Copyright (C) 1990, 1991, 1992 Free Software Foundation, Inc.

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
*/

/* NetBSD fits its header into the start of its text segment */
#define	N_HEADER_IN_TEXT(x)	1
#define TEXT_START_ADDR		TARGET_PAGE_SIZE

#define N_MACHTYPE(exec) \
	((enum machine_type)(((exec).a_info >> 16) & 0x03ff))
#define N_FLAGS(exec) \
	(((exec).a_info >> 26) & 0x3f)

#define N_SET_INFO(exec, magic, type, flags) \
	((exec).a_info = ((magic) & 0xffff) \
	 | (((int)(type) & 0x3ff) << 16) \
	 | (((flags) & 0x3f) << 24))
#define N_SET_MACHTYPE(exec, machtype) \
	((exec).a_info = \
         ((exec).a_info & 0xfb00ffff) | ((((int)(machtype))&0x3ff) << 16))
#define N_SET_FLAGS(exec, flags) \
	((exec).a_info = \
	 ((exec).a_info & 0x03ffffff) | ((flags & 0x03f) << 26))

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libaout.h"

/* On NetBSD, the magic number is always in ntohl's "network" (big-endian)
   format.  */
#define SWAP_MAGIC(ext) bfd_getb32 (ext)


#define MY_write_object_contents MY(write_object_contents)
static boolean MY(write_object_contents) PARAMS ((bfd *abfd));
#define MY_text_includes_header 1

#include "aout-target.h"

/* Write an object file.
   Section contents have already been written.  We write the
   file header, symbols, and relocation.  */

static boolean
MY(write_object_contents) (abfd)
     bfd *abfd;
{
  struct external_exec exec_bytes;
  struct internal_exec *execp = exec_hdr (abfd);

#if CHOOSE_RELOC_SIZE
  CHOOSE_RELOC_SIZE(abfd);
#else
  obj_reloc_entry_size (abfd) = RELOC_STD_SIZE;
#endif

  /* Magic number, maestro, please!  */
  switch (bfd_get_arch(abfd)) {
  case bfd_arch_m68k:
    if (strcmp (abfd->xvec->name, "a.out-m68k4k-netbsd") == 0)
      N_SET_MACHTYPE(*execp, M_68K4K_NETBSD);
    else
      N_SET_MACHTYPE(*execp, M_68K_NETBSD);
    break;
  case bfd_arch_sparc:
    N_SET_MACHTYPE(*execp, M_SPARC_NETBSD);
    break;
  case bfd_arch_i386:
    N_SET_MACHTYPE(*execp, M_386_NETBSD);
    break;
  case bfd_arch_ns32k:
    N_SET_MACHTYPE(*execp, M_532_NETBSD);
    break;
  default:
    N_SET_MACHTYPE(*execp, M_UNKNOWN);
    break;
  }

  /* The NetBSD magic number is always big-endian */
#ifndef TARGET_IS_BIG_ENDIAN_P
  /* XXX aren't there any macro to change byteorder of a word independent of
     the host's or target's endianesses?  */
  execp->a_info
    = (execp->a_info & 0xff) << 24 | (execp->a_info & 0xff00) << 8
      | (execp->a_info & 0xff0000) >> 8 | (execp->a_info & 0xff000000) >> 24;
#endif

  WRITE_HEADERS(abfd, execp);

  return true;
}
