/* CGEN generic disassembler support code.

   Copyright (C) 1996, 1997, 1998 Free Software Foundation, Inc.

   This file is part of the GNU Binutils and GDB, the GNU debugger.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysdep.h"
#include <stdio.h>
#include "ansidecl.h"
#include "libiberty.h"
#include "bfd.h"
#include "symcat.h"
#include "opcode/cgen.h"

/* This is not published as part of the public interface so we don't
   declare this in cgen.h.  */
extern CGEN_OPCODE_DATA * cgen_current_opcode_data;

/* Disassembler instruction hash table.  */
static CGEN_INSN_LIST ** dis_hash_table;

void
cgen_dis_init ()
{
  if (dis_hash_table)
    {
      free (dis_hash_table);
      dis_hash_table = NULL;
    }
}

/* Build the disassembler instruction hash table.  */

static void
build_dis_hash_table ()
{
  int bigend = cgen_current_endian == CGEN_ENDIAN_BIG;
  unsigned int hash;
  char buf [4];
  unsigned long value;
  int count = cgen_insn_count ();
  CGEN_OPCODE_DATA * data = cgen_current_opcode_data;
  CGEN_INSN_TABLE * insn_table = data->insn_table;
  unsigned int entry_size = insn_table->entry_size;
  unsigned int hash_size = insn_table->dis_hash_table_size;
  const CGEN_INSN * insn;
  CGEN_INSN_LIST * insn_lists;
  CGEN_INSN_LIST * new_insns;

  /* The space allocated for the hash table consists of two parts:
     the hash table and the hash lists.  */

  dis_hash_table = (CGEN_INSN_LIST **)
    xmalloc (hash_size * sizeof (CGEN_INSN_LIST *)
	     + count * sizeof (CGEN_INSN_LIST));
  memset (dis_hash_table, 0,
	  hash_size * sizeof (CGEN_INSN_LIST *)
	  + count * sizeof (CGEN_INSN_LIST));
  insn_lists = (CGEN_INSN_LIST *) (dis_hash_table + hash_size);

  /* Add compiled in insns.
     The table is scanned backwards as later additions are inserted in
     front of earlier ones and we want earlier ones to be prefered.
     We stop at the first one as it is a reserved entry.
     This is a bit tricky as the attribute member of CGEN_INSN is variable
     among architectures.  This code could be moved to cgen-asm.in, but
     I prefer to keep it here for now.  */

  for (insn = (CGEN_INSN *)
       ((char *) insn_table->init_entries
	+ entry_size * (insn_table->num_init_entries - 1));
       insn > insn_table->init_entries;
       insn = (CGEN_INSN *) ((char *) insn - entry_size), ++ insn_lists)
    {
      /* We don't know whether the target uses the buffer or the base insn
	 to hash on, so set both up.  */
      value = CGEN_INSN_VALUE (insn);
      switch (CGEN_INSN_MASK_BITSIZE (insn))
	{
	case 8:
	  buf[0] = value;
	  break;
	case 16:
	  if (bigend)
	    bfd_putb16 ((bfd_vma) value, buf);
	  else
	    bfd_putl16 ((bfd_vma) value, buf);
	  break;
	case 32:
	  if (bigend)
	    bfd_putb32 ((bfd_vma) value, buf);
	  else
	    bfd_putl32 ((bfd_vma) value, buf);
	  break;
	default:
	  abort ();
	}

      hash = insn_table->dis_hash (buf, value);

      insn_lists->next = dis_hash_table [hash];
      insn_lists->insn = insn;

      dis_hash_table [hash] = insn_lists;
    }

  /* Add runtime added insns.
     ??? Currently later added insns will be prefered over earlier ones.
     Not sure this is a bug or not.  */
  for (new_insns = insn_table->new_entries;
       new_insns != NULL;
       new_insns = new_insns->next, ++ insn_lists)
    {
      /* We don't know whether the target uses the buffer or the base insn
	 to hash on, so set both up.  */
      value = CGEN_INSN_VALUE (new_insns->insn);
      switch (CGEN_INSN_MASK_BITSIZE (new_insns->insn))
	{
	case 8:
	  buf[0] = value;
	  break;
	case 16:
	  if (bigend)
	    bfd_putb16 ((bfd_vma) value, buf);
	  else
	    bfd_putl16 ((bfd_vma) value, buf);
	  break;
	case 32:
	  if (bigend)
	    bfd_putb32 ((bfd_vma) value, buf);
	  else
	    bfd_putl32 ((bfd_vma) value, buf);
	  break;
	default:
	  abort ();
	}

      hash = insn_table->dis_hash (buf, value);

      insn_lists->next = dis_hash_table [hash];
      insn_lists->insn = new_insns->insn;

      dis_hash_table [hash] = insn_lists;
    }
}

/* Return the first entry in the hash list for INSN.  */

CGEN_INSN_LIST *
cgen_dis_lookup_insn (buf, value)
     const char * buf;
     unsigned long value;
{
  unsigned int hash;

  if (dis_hash_table == NULL)
    build_dis_hash_table ();

  hash = cgen_current_opcode_data->insn_table->dis_hash (buf, value);

  return dis_hash_table [hash];
}
