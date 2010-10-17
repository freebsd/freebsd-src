/* Special version of <a.out.h> for use under hp-ux.
   Copyright 1988, 1991 Free Software Foundation, Inc.

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

/* THIS FILE IS OBSOLETE.  It needs to be revised as a variant "external"
   a.out format for use with BFD.  */

/* The `exec' structure and overall layout must be close to HP's when
   we are running on an HP system, otherwise we will not be able to
   execute the resulting file. */

/* Allow this file to be included twice. */
#ifndef __GNU_EXEC_MACROS__

struct exec
{
  unsigned short a_machtype;	/* machine type */
  unsigned short a_magic;	/* magic number */
  unsigned long a_spare1;
  unsigned long a_spare2;
  unsigned long a_text;		/* length of text, in bytes */
  unsigned long a_data;		/* length of data, in bytes */
  unsigned long a_bss;		/* length of uninitialized data area for file, in bytes */
  unsigned long a_trsize;	/* length of relocation info for text, in bytes */
  unsigned long a_drsize;	/* length of relocation info for data, in bytes */
  unsigned long a_spare3;	/* HP = pascal interface size */
  unsigned long a_spare4;	/* HP = symbol table size */
  unsigned long a_spare5;	/* HP = debug name table size */
  unsigned long a_entry;	/* start address */
  unsigned long a_spare6;	/* HP = source line table size */
  unsigned long a_spare7;	/* HP = value table size */
  unsigned long a_syms;		/* length of symbol table data in file, in bytes */
  unsigned long a_spare8;
};

/* Tell a.out.gnu.h not to define `struct exec'.  */
#define __STRUCT_EXEC_OVERRIDE__

#include "../a.out.gnu.h"

#undef N_MAGIC
#undef N_MACHTYPE
#undef N_FLAGS
#undef N_SET_INFO
#undef N_SET_MAGIC
#undef N_SET_MACHTYPE
#undef N_SET_FLAGS

#define N_MAGIC(exec) ((exec) . a_magic)
#define N_MACHTYPE(exec) ((exec) . a_machtype)
#define N_SET_MAGIC(exec, magic) (((exec) . a_magic) = (magic))
#define N_SET_MACHTYPE(exec, machtype) (((exec) . a_machtype) = (machtype))

#undef N_BADMAG
#define N_BADMAG(x) ((_N_BADMAG (x)) || (_N_BADMACH (x)))

#define _N_BADMACH(x)							\
(((N_MACHTYPE (x)) != HP9000S200_ID) &&					\
 ((N_MACHTYPE (x)) != HP98x6_ID))

#define HP98x6_ID 0x20A
#define HP9000S200_ID 0x20C

#undef _N_HDROFF
#define _N_HDROFF(x) (SEGMENT_SIZE - (sizeof (struct exec)))

#define SEGMENT_SIZE 0x1000

#endif /* __GNU_EXEC_MACROS__ */
