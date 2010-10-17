/* This file is obj-hp300.h
   Copyright 1993, 2000 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define __STRUCT_EXEC_OVERRIDE__

struct exec_bytes
{
  unsigned char a_info[4];   /* a_machtype/a_magic */
  unsigned char a_spare1[4];
  unsigned char a_spare2[4];
  unsigned char a_text[4];   /* length of text, in bytes */
  unsigned char a_data[4];   /* length of data, in bytes */
  unsigned char a_bss[4];    /* length of uninitialized data area for file, in bytes */
  unsigned char a_trsize[4]; /* length of relocation info for text, in bytes */
  unsigned char a_drsize[4]; /* length of relocation info for data, in bytes */
  unsigned char a_spare3[4]; /* HP = pascal interface size */
  unsigned char a_spare4[4]; /* HP = symbol table size */
  unsigned char a_spare5[4]; /* HP = debug name table size */
  unsigned char a_entry[4];  /* start address */
  unsigned char a_spare6[4]; /* HP = source line table size */
  unsigned char a_spare7[4]; /* HP = value table size */
  unsigned char a_syms[4];   /* length of symbol table data in file, in bytes */
  unsigned char a_spare8[4];
};

/* How big the "struct exec" is on disk */
#define EXEC_BYTES_SIZE (16 * 4)

struct exec
{
  unsigned long a_info;
  unsigned long a_spare1;
  unsigned long a_spare2;
  unsigned long a_text;
  unsigned long a_data;
  unsigned long a_bss;
  unsigned long a_trsize;
  unsigned long a_drsize;
  unsigned long a_spare3;
  unsigned long a_spare4;
  unsigned long a_spare5;
  unsigned long a_entry;
  unsigned long a_spare6;
  unsigned long a_spare7;
  unsigned long a_syms;
  unsigned long a_spare8;
};

#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE	(OMAGIC)
#define AOUT_VERSION	0x02
#define AOUT_MACHTYPE	0x0c
#define OMAGIC		0x106

#define obj_header_append	hp300_header_append
#include "config/obj-aout.h"
