/* BFD back-end for i386 a.out binaries.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.

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


/* The only 386 aout system we have here is GO32 from DJ. 
   These numbers make BFD work with that. If your aout 386 system
   doesn't work with these, we'll have to split them into different
   files.  Send me (sac@cygnus.com) the runes to make it work on your
   system, and I'll stick it in for the next release.  */

#define N_HEADER_IN_TEXT(x) 0
#define BYTES_IN_WORD 4

#define N_TXTOFF(x) 0x20
#define N_TXTADDR(x) (N_MAGIC(x)==ZMAGIC ? 0x1020 : 0)

#define N_TXTSIZE(x) ((x).a_text)
#if 0
#define N_DATADDR(x) (N_MAGIC(x)==OMAGIC? (N_TXTADDR(x)+(x).a_text) : (SEGMENT_SIZE + ((0x1020+(x).a_text-1) & ~(SEGMENT_SIZE-1))))
#define NOSUBEXECB

#endif
#define TARGET_PAGE_SIZE 4096
#define SEGMENT_SIZE 0x400000
#define DEFAULT_ARCH bfd_arch_i386

#define MY(OP) CAT(i386aout_,OP)
#define TARGETNAME "a.out-i386"
#define NO_WRITE_HEADER_KLUDGE 1

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libaout.h"
static boolean MY(set_sizes)();
#define MY_backend_data &MY(backend_data)
static CONST struct aout_backend_data MY(backend_data) = {
  0,				/* zmagic contiguous */
  1,				/* text incl header */
  0,				/* exec_hdr_flags */
  0,				/* text vma? */
  MY(set_sizes),
  1,				/* exec header not counted */
  0,				/* add_dynamic_symbols */
  0,				/* add_one_symbol */
  0,				/* link_dynamic_object */
  0,				/* write_dynamic_symbol */
  0,				/* check_dynamic_reloc */
  0				/* finish_dynamic_link */
};

#include "aout-target.h"
