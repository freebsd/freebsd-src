/* CGEN generic opcode support.

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
#include <ctype.h>
#include <stdio.h>
#include "ansidecl.h"
#include "libiberty.h"
#include "bfd.h"
#include "symcat.h"
#include "opcode/cgen.h"

/* State variables.
   These record the state of the currently selected cpu, machine, endian, etc.
   They are set by cgen_set_cpu.  */

/* Current opcode data.  */
CGEN_OPCODE_DATA *cgen_current_opcode_data;

/* Current machine (a la BFD machine number).  */
int cgen_current_mach;

/* Current endian.  */
enum cgen_endian cgen_current_endian = CGEN_ENDIAN_UNKNOWN;

/* FIXME: To support multiple architectures, we need to return a handle
   to the state set up by this function, and pass the handle back to the
   other functions.  Later.  */

void
cgen_set_cpu (data, mach, endian)
     CGEN_OPCODE_DATA *data;
     int mach;
     enum cgen_endian endian;
{
  static int init_once_p;

  cgen_current_opcode_data = data;
  cgen_current_mach = mach;
  cgen_current_endian = endian;

  /* Initialize those things that only need be done once.  */
  if (! init_once_p)
    {
      /* Nothing to do currently.  */
      init_once_p = 1;
    }

#if 0 /* This isn't done here because it would put assembler support in the
	 disassembler, etc.  The caller is required to call these after calling
	 us.  */
  /* Reset the hash tables.  */
  cgen_asm_init ();
  cgen_dis_init ();
#endif  
}

static unsigned int hash_keyword_name
  PARAMS ((const CGEN_KEYWORD *, const char *, int));
static unsigned int hash_keyword_value
  PARAMS ((const CGEN_KEYWORD *, unsigned int));
static void build_keyword_hash_tables
  PARAMS ((CGEN_KEYWORD *));

/* Return number of hash table entries to use for N elements.  */
#define KEYWORD_HASH_SIZE(n) ((n) <= 31 ? 17 : 31)

/* Look up *NAMEP in the keyword table KT.
   The result is the keyword entry or NULL if not found.  */

const CGEN_KEYWORD_ENTRY *
cgen_keyword_lookup_name (kt, name)
     CGEN_KEYWORD *kt;
     const char *name;
{
  const CGEN_KEYWORD_ENTRY *ke;
  const char *p,*n;

  if (kt->name_hash_table == NULL)
    build_keyword_hash_tables (kt);

  ke = kt->name_hash_table[hash_keyword_name (kt, name, 0)];

  /* We do case insensitive comparisons.
     If that ever becomes a problem, add an attribute that denotes
     "do case sensitive comparisons".  */

  while (ke != NULL)
    {
      n = name;
      p = ke->name;

      while (*p
	     && (*p == *n
		 || (isalpha ((unsigned char) *p)
		     && (tolower ((unsigned char) *p)
			 == tolower ((unsigned char) *n)))))
	++n, ++p;

      if (!*p && !*n)
	return ke;

      ke = ke->next_name;
    }

  if (kt->null_entry)
    return kt->null_entry;
  return NULL;
}

/* Look up VALUE in the keyword table KT.
   The result is the keyword entry or NULL if not found.  */

const CGEN_KEYWORD_ENTRY *
cgen_keyword_lookup_value (kt, value)
     CGEN_KEYWORD *kt;
     int value;
{
  const CGEN_KEYWORD_ENTRY *ke;

  if (kt->name_hash_table == NULL)
    build_keyword_hash_tables (kt);

  ke = kt->value_hash_table[hash_keyword_value (kt, value)];

  while (ke != NULL)
    {
      if (value == ke->value)
	return ke;
      ke = ke->next_value;
    }

  return NULL;
}

/* Add an entry to a keyword table.  */

void
cgen_keyword_add (kt, ke)
     CGEN_KEYWORD *kt;
     CGEN_KEYWORD_ENTRY *ke;
{
  unsigned int hash;

  if (kt->name_hash_table == NULL)
    build_keyword_hash_tables (kt);

  hash = hash_keyword_name (kt, ke->name, 0);
  ke->next_name = kt->name_hash_table[hash];
  kt->name_hash_table[hash] = ke;

  hash = hash_keyword_value (kt, ke->value);
  ke->next_value = kt->value_hash_table[hash];
  kt->value_hash_table[hash] = ke;

  if (ke->name[0] == 0)
    kt->null_entry = ke;
}

/* FIXME: Need function to return count of keywords.  */

/* Initialize a keyword table search.
   SPEC is a specification of what to search for.
   A value of NULL means to find every keyword.
   Currently NULL is the only acceptable value [further specification
   deferred].
   The result is an opaque data item used to record the search status.
   It is passed to each call to cgen_keyword_search_next.  */

CGEN_KEYWORD_SEARCH
cgen_keyword_search_init (kt, spec)
     CGEN_KEYWORD *kt;
     const char *spec;
{
  CGEN_KEYWORD_SEARCH search;

  /* FIXME: Need to specify format of PARAMS.  */
  if (spec != NULL)
    abort ();

  if (kt->name_hash_table == NULL)
    build_keyword_hash_tables (kt);

  search.table = kt;
  search.spec = spec;
  search.current_hash = 0;
  search.current_entry = NULL;
  return search;
}

/* Return the next keyword specified by SEARCH.
   The result is the next entry or NULL if there are no more.  */

const CGEN_KEYWORD_ENTRY *
cgen_keyword_search_next (search)
     CGEN_KEYWORD_SEARCH *search;
{
  /* Has search finished?  */
  if (search->current_hash == search->table->hash_table_size)
    return NULL;

  /* Search in progress?  */
  if (search->current_entry != NULL
      /* Anything left on this hash chain?  */
      && search->current_entry->next_name != NULL)
    {
      search->current_entry = search->current_entry->next_name;
      return search->current_entry;
    }

  /* Move to next hash chain [unless we haven't started yet].  */
  if (search->current_entry != NULL)
    ++search->current_hash;

  while (search->current_hash < search->table->hash_table_size)
    {
      search->current_entry = search->table->name_hash_table[search->current_hash];
      if (search->current_entry != NULL)
	return search->current_entry;
      ++search->current_hash;
    }

  return NULL;
}

/* Return first entry in hash chain for NAME.
   If CASE_SENSITIVE_P is non-zero, return a case sensitive hash.  */

static unsigned int
hash_keyword_name (kt, name, case_sensitive_p)
     const CGEN_KEYWORD *kt;
     const char *name;
     int case_sensitive_p;
{
  unsigned int hash;

  if (case_sensitive_p)
    for (hash = 0; *name; ++name)
      hash = (hash * 97) + (unsigned char) *name;
  else
    for (hash = 0; *name; ++name)
      hash = (hash * 97) + (unsigned char) tolower (*name);
  return hash % kt->hash_table_size;
}

/* Return first entry in hash chain for VALUE.  */

static unsigned int
hash_keyword_value (kt, value)
     const CGEN_KEYWORD *kt;
     unsigned int value;
{
  return value % kt->hash_table_size;
}

/* Build a keyword table's hash tables.
   We probably needn't build the value hash table for the assembler when
   we're using the disassembler, but we keep things simple.  */

static void
build_keyword_hash_tables (kt)
     CGEN_KEYWORD *kt;
{
  int i;
  /* Use the number of compiled in entries as an estimate for the
     typical sized table [not too many added at runtime].  */
  unsigned int size = KEYWORD_HASH_SIZE (kt->num_init_entries);

  kt->hash_table_size = size;
  kt->name_hash_table = (CGEN_KEYWORD_ENTRY **)
    xmalloc (size * sizeof (CGEN_KEYWORD_ENTRY *));
  memset (kt->name_hash_table, 0, size * sizeof (CGEN_KEYWORD_ENTRY *));
  kt->value_hash_table = (CGEN_KEYWORD_ENTRY **)
    xmalloc (size * sizeof (CGEN_KEYWORD_ENTRY *));
  memset (kt->value_hash_table, 0, size * sizeof (CGEN_KEYWORD_ENTRY *));

  /* The table is scanned backwards as we want keywords appearing earlier to
     be prefered over later ones.  */
  for (i = kt->num_init_entries - 1; i >= 0; --i)
    cgen_keyword_add (kt, &kt->init_entries[i]);
}

/* Hardware support.  */

const CGEN_HW_ENTRY *
cgen_hw_lookup (name)
     const char *name;
{
  const CGEN_HW_ENTRY *hw = cgen_current_opcode_data->hw_list;

  while (hw != NULL)
    {
      if (strcmp (name, hw->name) == 0)
	return hw;
      hw = hw->next;
    }

  return NULL;
}

/* Instruction support.  */

/* Return number of instructions.  This includes any added at runtime.  */

int
cgen_insn_count ()
{
  int count = cgen_current_opcode_data->insn_table->num_init_entries;
  CGEN_INSN_LIST *insn = cgen_current_opcode_data->insn_table->new_entries;

  for ( ; insn != NULL; insn = insn->next)
    ++count;

  return count;
}
