/* CGEN generic assembler support code.

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
#include <ctype.h>
#include "ansidecl.h"
#include "libiberty.h"
#include "bfd.h"
#include "symcat.h"
#include "opcode/cgen.h"

/* Operand parsing callback.  */
const char * (*cgen_parse_operand_fn)
     PARAMS ((enum cgen_parse_operand_type, const char **, int, int,
	      enum cgen_parse_operand_result *, bfd_vma *));

/* This is not published as part of the public interface so we don't
   declare this in cgen.h.  */
extern CGEN_OPCODE_DATA *cgen_current_opcode_data;

/* Assembler instruction hash table.  */
static CGEN_INSN_LIST **asm_hash_table;

/* Called once at startup and whenever machine/endian change.  */

void
cgen_asm_init ()
{
  if (asm_hash_table)
    {
      free (asm_hash_table);
      asm_hash_table = NULL;
    }
}

/* Called whenever starting to parse an insn.  */

void
cgen_init_parse_operand ()
{
  /* This tells the callback to re-initialize.  */
  (void) (*cgen_parse_operand_fn) (CGEN_PARSE_OPERAND_INIT, NULL, 0, 0,
				   NULL, NULL);
}

/* Build the assembler instruction hash table.  */

static void
build_asm_hash_table ()
{
  unsigned int hash;
  int count = cgen_insn_count ();
  CGEN_OPCODE_DATA *data = cgen_current_opcode_data;
  CGEN_INSN_TABLE *insn_table = data->insn_table;
  unsigned int entry_size = insn_table->entry_size;
  unsigned int hash_size = insn_table->asm_hash_table_size;
  const CGEN_INSN *insn;
  CGEN_INSN_LIST *insn_lists,*new_insns;

  /* The space allocated for the hash table consists of two parts:
     the hash table and the hash lists.  */

  asm_hash_table = (CGEN_INSN_LIST **)
    xmalloc (hash_size * sizeof (CGEN_INSN_LIST *)
	     + count * sizeof (CGEN_INSN_LIST));
  memset (asm_hash_table, 0,
	  hash_size * sizeof (CGEN_INSN_LIST *)
	  + count * sizeof (CGEN_INSN_LIST));
  insn_lists = (CGEN_INSN_LIST *) (asm_hash_table + hash_size);

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
       insn = (CGEN_INSN *) ((char *) insn - entry_size), ++insn_lists)
    {
      hash = (*insn_table->asm_hash) CGEN_INSN_MNEMONIC (insn);
      insn_lists->next = asm_hash_table[hash];
      insn_lists->insn = insn;
      asm_hash_table[hash] = insn_lists;
    }

  /* Add runtime added insns.
     ??? Currently later added insns will be prefered over earlier ones.
     Not sure this is a bug or not.  */
  for (new_insns = insn_table->new_entries;
       new_insns != NULL;
       new_insns = new_insns->next, ++insn_lists)
    {
      hash = (*insn_table->asm_hash) CGEN_INSN_MNEMONIC (new_insns->insn);
      insn_lists->next = asm_hash_table[hash];
      insn_lists->insn = new_insns->insn;
      asm_hash_table[hash] = insn_lists;
    }
}

/* Return the first entry in the hash list for INSN.  */

CGEN_INSN_LIST *
cgen_asm_lookup_insn (insn)
     const char *insn;
{
  unsigned int hash;

  if (asm_hash_table == NULL)
    build_asm_hash_table ();

  hash = (*cgen_current_opcode_data->insn_table->asm_hash) (insn);
  return asm_hash_table[hash];
}

/* Keyword parser.
   The result is NULL upon success or an error message.
   If successful, *STRP is updated to point passed the keyword.

   ??? At present we have a static notion of how to pick out a keyword.
   Later we can allow a target to customize this if necessary [say by
   recording something in the keyword table].  */

const char *
cgen_parse_keyword (strp, keyword_table, valuep)
     const char **strp;
     CGEN_KEYWORD *keyword_table;
     long *valuep;
{
  const CGEN_KEYWORD_ENTRY *ke;
  char buf[256];
  const char *p,*start;

  p = start = *strp;

  /* Allow any first character.
     Note that this allows recognizing ",a" for the annul flag in sparc
     even though "," is subsequently not a valid keyword char.  */
  if (*p)
    ++p;

  /* Now allow letters, digits, and _.  */
  while (((p - start) < (int) sizeof (buf))
	 && (isalnum ((unsigned char) *p) || *p == '_'))
    ++p;

  if (p - start >= (int) sizeof (buf))
    return "unrecognized keyword/register name";

  memcpy (buf, start, p - start);
  buf[p - start] = 0;

  ke = cgen_keyword_lookup_name (keyword_table, buf);

  if (ke != NULL)
    {
      *valuep = ke->value;
      /* Don't advance pointer if we recognized the null keyword.  */
      if (ke->name[0] != 0)
	*strp = p;
      return NULL;
    }

  return "unrecognized keyword/register name";
}

/* Signed integer parser.  */

const char *
cgen_parse_signed_integer (strp, opindex, valuep)
     const char **strp;
     int opindex;
     long *valuep;
{
  bfd_vma value;
  enum cgen_parse_operand_result result;
  const char *errmsg;

  errmsg = (*cgen_parse_operand_fn) (CGEN_PARSE_OPERAND_INTEGER, strp,
				     opindex, BFD_RELOC_NONE,
				     &result, &value);
  /* FIXME: Examine `result'.  */
  if (!errmsg)
    *valuep = value;
  return errmsg;
}

/* Unsigned integer parser.  */

const char *
cgen_parse_unsigned_integer (strp, opindex, valuep)
     const char **strp;
     int opindex;
     unsigned long *valuep;
{
  bfd_vma value;
  enum cgen_parse_operand_result result;
  const char *errmsg;

  errmsg = (*cgen_parse_operand_fn) (CGEN_PARSE_OPERAND_INTEGER, strp,
				     opindex, BFD_RELOC_NONE,
				     &result, &value);
  /* FIXME: Examine `result'.  */
  if (!errmsg)
    *valuep = value;
  return errmsg;
}

/* Address parser.  */

const char *
cgen_parse_address (strp, opindex, opinfo, resultp, valuep)
     const char **strp;
     int opindex;
     int opinfo;
     enum cgen_parse_operand_result *resultp;
     long *valuep;
{
  bfd_vma value;
  enum cgen_parse_operand_result result_type;
  const char *errmsg;

  errmsg = (*cgen_parse_operand_fn) (CGEN_PARSE_OPERAND_ADDRESS, strp,
				     opindex, opinfo,
				     &result_type, &value);
  /* FIXME: Examine `result'.  */
  if (!errmsg)
    {
      if (resultp != NULL)
	*resultp = result_type;
      *valuep = value;
    }
  return errmsg;
}

/* Signed integer validation routine.  */

const char *
cgen_validate_signed_integer (value, min, max)
     long value, min, max;
{
  if (value < min || value > max)
    {
      const char *err =
	"operand out of range (%ld not between %ld and %ld)";
      static char buf[100];

      sprintf (buf, err, value, min, max);
      return buf;
    }

  return NULL;
}

/* Unsigned integer validation routine.
   Supplying `min' here may seem unnecessary, but we also want to handle
   cases where min != 0 (and max > LONG_MAX).  */

const char *
cgen_validate_unsigned_integer (value, min, max)
     unsigned long value, min, max;
{
  if (value < min || value > max)
    {
      const char *err =
	"operand out of range (%lu not between %lu and %lu)";
      static char buf[100];

      sprintf (buf, err, value, min, max);
      return buf;
    }

  return NULL;
}
