/* BFD back-end for i386 a.out binaries under dynix.
   Copyright 1994, 1995, 2001, 2003 Free Software Foundation, Inc.

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

/* This BFD is currently only tested with gdb, writing object files
   may not work.  */

#define TEXT_START_ADDR 4096
#define TARGET_PAGE_SIZE	4096
#define SEGMENT_SIZE	TARGET_PAGE_SIZE

#include "aout/dynix3.h"

#define DEFAULT_ARCH	bfd_arch_i386
#define MACHTYPE_OK(mtype) ((mtype) == M_386 || (mtype) == M_UNKNOWN)

/* Do not "beautify" the CONCAT* macro args.  Traditional C will not
   remove whitespace added here, and thus will fail to concatenate
   the tokens.  */
#define MY(OP) CONCAT2 (i386dynix_,OP)
#define TARGETNAME "a.out-i386-dynix"
#define NAME(x,y) CONCAT3 (i386dynix,_32_,y)
#define ARCH_SIZE 32
#define NAME_swap_exec_header_in NAME(i386dynix_32_,swap_exec_header_in)
#define MY_get_section_contents aout_32_get_section_contents

/* aoutx.h requires definitions for NMAGIC, BMAGIC and QMAGIC.  */
#define NMAGIC 0
#define BMAGIC OMAGIC
#define QMAGIC XMAGIC

#include "aoutx.h"

/* (Ab)use some fields in the internal exec header to be able to read
   executables that contain shared data.  */

#define a_shdata a_tload
#define a_shdrsize a_dload

void
i386dynix_32_swap_exec_header_in (abfd, raw_bytes, execp)
     bfd *abfd;
     struct external_exec *raw_bytes;
     struct internal_exec *execp;
{
  struct external_exec *bytes = (struct external_exec *)raw_bytes;

  /* The internal_exec structure has some fields that are unused in this
     configuration (IE for i960), so ensure that all such uninitialized
     fields are zero'd out.  There are places where two of these structs
     are memcmp'd, and thus the contents do matter. */
  memset ((PTR) execp, 0, sizeof (struct internal_exec));
  /* Now fill in fields in the execp, from the bytes in the raw data.  */
  execp->a_info   = H_GET_32 (abfd, bytes->e_info);
  execp->a_text   = GET_WORD (abfd, bytes->e_text);
  execp->a_data   = GET_WORD (abfd, bytes->e_data);
  execp->a_bss    = GET_WORD (abfd, bytes->e_bss);
  execp->a_syms   = GET_WORD (abfd, bytes->e_syms);
  execp->a_entry  = GET_WORD (abfd, bytes->e_entry);
  execp->a_trsize = GET_WORD (abfd, bytes->e_trsize);
  execp->a_drsize = GET_WORD (abfd, bytes->e_drsize);
  execp->a_shdata = GET_WORD (abfd, bytes->e_shdata);
  execp->a_shdrsize = GET_WORD (abfd, bytes->e_shdrsize);
}

#include "aout-target.h"
