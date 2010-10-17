/* Configurable Xtensa ISA support.
   Copyright 2003 Free Software Foundation, Inc.

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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "xtensa-isa.h"
#include "xtensa-isa-internal.h"

xtensa_isa xtensa_default_isa = NULL;

static int
opname_lookup_compare (const void *v1, const void *v2)
{
  opname_lookup_entry *e1 = (opname_lookup_entry *)v1;
  opname_lookup_entry *e2 = (opname_lookup_entry *)v2;

  return strcmp (e1->key, e2->key);
}


xtensa_isa
xtensa_isa_init (void)
{
  xtensa_isa isa;
  int mod;

  isa = xtensa_load_isa (0);
  if (isa == 0)
    {
      fprintf (stderr, "Failed to initialize Xtensa base ISA module\n");
      return NULL;
    }

  for (mod = 1; xtensa_isa_modules[mod].get_num_opcodes_fn; mod++)
    {
      if (!xtensa_extend_isa (isa, mod))
	{
	  fprintf (stderr, "Failed to initialize Xtensa TIE ISA module\n");
	  return NULL;
	}
    }

  return isa;
}

/* ISA information.  */

static int
xtensa_check_isa_config (xtensa_isa_internal *isa,
			 struct config_struct *config_table)
{
  int i, j;

  if (!config_table)
    {
      fprintf (stderr, "Error: Empty configuration table in ISA DLL\n");
      return 0;
    }

  /* For the first module, save a pointer to the table and record the
     specified endianness and availability of the density option.  */

  if (isa->num_modules == 0)
    {
      int found_memory_order = 0;

      isa->config = config_table;
      isa->has_density = 1;  /* Default to have density option.  */

      for (i = 0; config_table[i].param_name; i++)
	{
	  if (!strcmp (config_table[i].param_name, "IsaMemoryOrder"))
	    {
	      isa->is_big_endian =
		(strcmp (config_table[i].param_value, "BigEndian") == 0);
	      found_memory_order = 1;
	    }
	  if (!strcmp (config_table[i].param_name, "IsaUseDensityInstruction"))
	    {
	      isa->has_density = atoi (config_table[i].param_value);
	    }
	}
      if (!found_memory_order)
	{
	  fprintf (stderr, "Error: \"IsaMemoryOrder\" missing from "
		   "configuration table in ISA DLL\n");
	  return 0;
	}

      return 1;
    }

  /* For subsequent modules, check that the parameters match.  Note: This
     code is sufficient to handle the current model where there are never
     more than 2 modules; we might at some point want to handle cases where
     module N > 0 specifies some parameters not included in the base table,
     and we would then add those to isa->config so that subsequent modules
     would check against them. */

  for (i = 0; config_table[i].param_name; i++)
    {
      for (j = 0; isa->config[j].param_name; j++)
	{
	  if (!strcmp (config_table[i].param_name, isa->config[j].param_name))
	    {
	      int mismatch;
	      if (!strcmp (config_table[i].param_name, "IsaCoprocessorCount"))
		{
		  /* Only require the coprocessor count to be <= the base.  */
		  int tiecnt = atoi (config_table[i].param_value);
		  int basecnt = atoi (isa->config[j].param_value);
		  mismatch = (tiecnt > basecnt);
		}
	      else
		mismatch = strcmp (config_table[i].param_value,
				   isa->config[j].param_value);
	      if (mismatch)
		{
#define MISMATCH_MESSAGE \
"Error: Configuration mismatch in the \"%s\" parameter:\n\
the configuration used when the TIE file was compiled had a value of\n\
\"%s\", while the current configuration has a value of\n\
\"%s\". Please rerun the TIE compiler with a matching\n\
configuration.\n"
		  fprintf (stderr, MISMATCH_MESSAGE,
			   config_table[i].param_name,
			   config_table[i].param_value,
			   isa->config[j].param_value);
		  return 0;
		}
	      break;
	    }
	}
    }

  return 1;
}


static int
xtensa_add_isa (xtensa_isa_internal *isa, libisa_module_specifier libisa)
{
  int (*get_num_opcodes_fn) (void);
  struct config_struct *(*get_config_table_fn) (void);
  xtensa_opcode_internal **(*get_opcodes_fn) (void);
  int (*decode_insn_fn) (const xtensa_insnbuf);
  xtensa_opcode_internal **opcodes;
  int opc, insn_size, prev_num_opcodes, new_num_opcodes, this_module;

  get_num_opcodes_fn = xtensa_isa_modules[libisa].get_num_opcodes_fn;
  get_opcodes_fn = xtensa_isa_modules[libisa].get_opcodes_fn;
  decode_insn_fn = xtensa_isa_modules[libisa].decode_insn_fn;
  get_config_table_fn = xtensa_isa_modules[libisa].get_config_table_fn;

  if (!get_num_opcodes_fn || !get_opcodes_fn || !decode_insn_fn
      || (!get_config_table_fn && isa->num_modules == 0))
    return 0;

  if (get_config_table_fn
      && !xtensa_check_isa_config (isa, get_config_table_fn ()))
    return 0;

  prev_num_opcodes = isa->num_opcodes;
  new_num_opcodes = (*get_num_opcodes_fn) ();

  isa->num_opcodes += new_num_opcodes;
  isa->opcode_table = (xtensa_opcode_internal **)
    realloc (isa->opcode_table, isa->num_opcodes *
	     sizeof (xtensa_opcode_internal *));
  isa->opname_lookup_table = (opname_lookup_entry *)
    realloc (isa->opname_lookup_table, isa->num_opcodes *
	     sizeof (opname_lookup_entry));

  opcodes = (*get_opcodes_fn) ();

  insn_size = isa->insn_size;
  for (opc = 0; opc < new_num_opcodes; opc++)
    {
      xtensa_opcode_internal *intopc = opcodes[opc];
      int newopc = prev_num_opcodes + opc;
      isa->opcode_table[newopc] = intopc;
      isa->opname_lookup_table[newopc].key = intopc->name;
      isa->opname_lookup_table[newopc].opcode = newopc;
      if (intopc->length > insn_size)
	insn_size = intopc->length;
    }

  isa->insn_size = insn_size;
  isa->insnbuf_size = ((isa->insn_size + sizeof (xtensa_insnbuf_word) - 1) /
		       sizeof (xtensa_insnbuf_word));

  qsort (isa->opname_lookup_table, isa->num_opcodes,
	 sizeof (opname_lookup_entry), opname_lookup_compare);

  /* Check for duplicate opcode names.  */
  for (opc = 1; opc < isa->num_opcodes; opc++)
    {
      if (!opname_lookup_compare (&isa->opname_lookup_table[opc-1],
				  &isa->opname_lookup_table[opc]))
	{
	  fprintf (stderr, "Error: Duplicate TIE opcode \"%s\"\n",
		   isa->opname_lookup_table[opc].key);
	  return 0;
	}
    }

  this_module = isa->num_modules;
  isa->num_modules += 1;

  isa->module_opcode_base = (int *) realloc (isa->module_opcode_base,
					     isa->num_modules * sizeof (int));
  isa->module_decode_fn = (xtensa_insn_decode_fn *)
    realloc (isa->module_decode_fn, isa->num_modules *
	     sizeof (xtensa_insn_decode_fn));

  isa->module_opcode_base[this_module] = prev_num_opcodes;
  isa->module_decode_fn[this_module] = decode_insn_fn;

  xtensa_default_isa = isa;

  return 1;	/* Library was successfully added.  */
}


xtensa_isa
xtensa_load_isa (libisa_module_specifier libisa)
{
  xtensa_isa_internal *isa;

  isa = (xtensa_isa_internal *) malloc (sizeof (xtensa_isa_internal));
  memset (isa, 0, sizeof (xtensa_isa_internal));
  if (!xtensa_add_isa (isa, libisa))
    {
      xtensa_isa_free (isa);
      return NULL;
    }
  return (xtensa_isa) isa;
}


int
xtensa_extend_isa (xtensa_isa isa, libisa_module_specifier libisa)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  return xtensa_add_isa (intisa, libisa);
}


void
xtensa_isa_free (xtensa_isa isa)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  if (intisa->opcode_table)
    free (intisa->opcode_table);
  if (intisa->opname_lookup_table)
    free (intisa->opname_lookup_table);
  if (intisa->module_opcode_base)
    free (intisa->module_opcode_base);
  if (intisa->module_decode_fn)
    free (intisa->module_decode_fn);
  free (intisa);
}


int
xtensa_insn_maxlength (xtensa_isa isa)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  return intisa->insn_size;
}


int
xtensa_insnbuf_size (xtensa_isa isa)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *)isa;
  return intisa->insnbuf_size;
}


int
xtensa_num_opcodes (xtensa_isa isa)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  return intisa->num_opcodes;
}


xtensa_opcode
xtensa_opcode_lookup (xtensa_isa isa, const char *opname)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  opname_lookup_entry entry, *result;

  entry.key = opname;
  result = bsearch (&entry, intisa->opname_lookup_table, intisa->num_opcodes,
		    sizeof (opname_lookup_entry), opname_lookup_compare);
  if (!result) return XTENSA_UNDEFINED;
  return result->opcode;
}


xtensa_opcode
xtensa_decode_insn (xtensa_isa isa, const xtensa_insnbuf insn)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  int n, opc;
  for (n = 0; n < intisa->num_modules; n++) {
    opc = (intisa->module_decode_fn[n]) (insn);
    if (opc != XTENSA_UNDEFINED)
      return intisa->module_opcode_base[n] + opc;
  }
  return XTENSA_UNDEFINED;
}


/* Opcode information.  */

void
xtensa_encode_insn (xtensa_isa isa, xtensa_opcode opc, xtensa_insnbuf insn)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  xtensa_insnbuf template = intisa->opcode_table[opc]->template();
  int len = intisa->opcode_table[opc]->length;
  int n;

  /* Convert length to 32-bit words.  */
  len = (len + 3) / 4;

  /* Copy the template.  */
  for (n = 0; n < len; n++)
    insn[n] = template[n];

  /* Fill any unused buffer space with zeros.  */
  for ( ; n < intisa->insnbuf_size; n++)
    insn[n] = 0;
}


const char *
xtensa_opcode_name (xtensa_isa isa, xtensa_opcode opc)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  return intisa->opcode_table[opc]->name;
}


int
xtensa_insn_length (xtensa_isa isa, xtensa_opcode opc)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  return intisa->opcode_table[opc]->length;
}


int
xtensa_insn_length_from_first_byte (xtensa_isa isa, char first_byte)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  int is_density = (first_byte & (intisa->is_big_endian ? 0x80 : 0x08)) != 0;
  return (intisa->has_density && is_density ? 2 : 3);
}


int
xtensa_num_operands (xtensa_isa isa, xtensa_opcode opc)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  return intisa->opcode_table[opc]->iclass->num_operands;
}


xtensa_operand
xtensa_get_operand (xtensa_isa isa, xtensa_opcode opc, int opnd)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  xtensa_iclass_internal *iclass = intisa->opcode_table[opc]->iclass;
  if (opnd >= iclass->num_operands)
    return NULL;
  return (xtensa_operand) iclass->operands[opnd];
}


/* Operand information.  */

char *
xtensa_operand_kind (xtensa_operand opnd)
{
  xtensa_operand_internal *intop = (xtensa_operand_internal *) opnd;
  return intop->operand_kind;
}


char
xtensa_operand_inout (xtensa_operand opnd)
{
  xtensa_operand_internal *intop = (xtensa_operand_internal *) opnd;
  return intop->inout;
}


uint32
xtensa_operand_get_field (xtensa_operand opnd, const xtensa_insnbuf insn)
{
  xtensa_operand_internal *intop = (xtensa_operand_internal *) opnd;
  return (*intop->get_field) (insn);
}


void
xtensa_operand_set_field (xtensa_operand opnd, xtensa_insnbuf insn, uint32 val)
{
  xtensa_operand_internal *intop = (xtensa_operand_internal *) opnd;
  return (*intop->set_field) (insn, val);
}


xtensa_encode_result
xtensa_operand_encode (xtensa_operand opnd, uint32 *valp)
{
  xtensa_operand_internal *intop = (xtensa_operand_internal *) opnd;
  return (*intop->encode) (valp);
}


uint32
xtensa_operand_decode (xtensa_operand opnd, uint32 val)
{
  xtensa_operand_internal *intop = (xtensa_operand_internal *) opnd;
  return (*intop->decode) (val);
}


int
xtensa_operand_isPCRelative (xtensa_operand opnd)
{
  xtensa_operand_internal *intop = (xtensa_operand_internal *) opnd;
  return intop->isPCRelative;
}


uint32
xtensa_operand_do_reloc (xtensa_operand opnd, uint32 addr, uint32 pc)
{
  xtensa_operand_internal *intop = (xtensa_operand_internal *) opnd;
  if (!intop->isPCRelative)
    return addr;
  return (*intop->do_reloc) (addr, pc);
}


uint32
xtensa_operand_undo_reloc (xtensa_operand opnd, uint32 offset, uint32 pc)
{
  xtensa_operand_internal *intop = (xtensa_operand_internal *) opnd;
  if (!intop->isPCRelative)
    return offset;
  return (*intop->undo_reloc) (offset, pc);
}


/* Instruction buffers.  */

xtensa_insnbuf
xtensa_insnbuf_alloc (xtensa_isa isa)
{
  return (xtensa_insnbuf) malloc (xtensa_insnbuf_size (isa) *
				  sizeof (xtensa_insnbuf_word));
}


void
xtensa_insnbuf_free (xtensa_insnbuf buf)
{
  free( buf );
}


/* Given <byte_index>, the index of a byte in a xtensa_insnbuf, our
   internal representation of a xtensa instruction word, return the index of
   its word and the bit index of its low order byte in the xtensa_insnbuf.  */

static inline int
byte_to_word_index (int byte_index)
{
  return byte_index / sizeof (xtensa_insnbuf_word);
}


static inline int
byte_to_bit_index (int byte_index)
{
  return (byte_index & 0x3) * 8;
}


/* Copy an instruction in the 32 bit words pointed at by <insn> to characters
   pointed at by <cp>.  This is more complicated than you might think because
   we want 16 bit instructions in bytes 2,3 for big endian. This function
   allows us to specify which byte in <insn> to start with and which way to
   increment, allowing trivial implementation for both big and little endian.
   And it seems to make pretty good code for both.  */

void
xtensa_insnbuf_to_chars (xtensa_isa isa, const xtensa_insnbuf insn, char *cp)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  int insn_size = xtensa_insn_maxlength (intisa);
  int fence_post, start, increment, i, byte_count;
  xtensa_opcode opc;

  if (intisa->is_big_endian)
    {
      start = insn_size - 1;
      increment = -1;
    }
  else
    {
      start = 0;
      increment = 1;
    }

  /* Find the opcode; do nothing if the buffer does not contain a valid
     instruction since we need to know how many bytes to copy.  */
  opc = xtensa_decode_insn (isa, insn);
  if (opc == XTENSA_UNDEFINED)
    return;

  byte_count = xtensa_insn_length (isa, opc);
  fence_post = start + (byte_count * increment);

  for (i = start; i != fence_post; i += increment, ++cp)
    {
      int word_inx = byte_to_word_index (i);
      int bit_inx = byte_to_bit_index (i);

      *cp = (insn[word_inx] >> bit_inx) & 0xff;
    }
}

/* Inward conversion from byte stream to xtensa_insnbuf.  See
   xtensa_insnbuf_to_chars for a discussion of why this is
   complicated by endianness.  */
    
void
xtensa_insnbuf_from_chars (xtensa_isa isa, xtensa_insnbuf insn, const char* cp)
{
  xtensa_isa_internal *intisa = (xtensa_isa_internal *) isa;
  int insn_size = xtensa_insn_maxlength (intisa);
  int fence_post, start, increment, i;

  if (intisa->is_big_endian)
    {
      start = insn_size - 1;
      increment = -1;
    }
  else
    {
      start = 0;
      increment = 1;
    }

  fence_post = start + (insn_size * increment);
  memset (insn, 0, xtensa_insnbuf_size (isa) * sizeof (xtensa_insnbuf_word));

  for ( i = start; i != fence_post; i += increment, ++cp )
    {
      int word_inx = byte_to_word_index (i);
      int bit_inx = byte_to_bit_index (i);

      insn[word_inx] |= (*cp & 0xff) << bit_inx;
    }
}

