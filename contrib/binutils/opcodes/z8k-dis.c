/* Disassemble z8000 code.
   Copyright 1992, 1993, 1998, 2000, 2001
   Free Software Foundation, Inc.

This file is part of GNU Binutils.

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

#include "sysdep.h"
#include "dis-asm.h"

#define DEFINE_TABLE
#include "z8k-opc.h"

#include <setjmp.h>

typedef struct {
  /* These are all indexed by nibble number (i.e only every other entry
     of bytes is used, and every 4th entry of words).  */
  unsigned char nibbles[24];
  unsigned char bytes[24];
  unsigned short words[24];

  /* Nibble number of first word not yet fetched.  */
  int max_fetched;
  bfd_vma insn_start;
  jmp_buf bailout;

  long tabl_index;
  char instr_asmsrc[80];
  unsigned long arg_reg[0x0f];
  unsigned long immediate;
  unsigned long displacement;
  unsigned long address;
  unsigned long cond_code;
  unsigned long ctrl_code;
  unsigned long flags;
  unsigned long interrupts;
} instr_data_s;

static int fetch_data PARAMS ((struct disassemble_info *, int));


/* Make sure that bytes from INFO->PRIVATE_DATA->BUFFER (inclusive)
   to ADDR (exclusive) are valid.  Returns 1 for success, longjmps
   on error.  */
#define FETCH_DATA(info, nibble) \
  ((nibble) < ((instr_data_s *) (info->private_data))->max_fetched \
   ? 1 : fetch_data ((info), (nibble)))

static int
fetch_data (info, nibble)
     struct disassemble_info *info;
     int nibble;
{
  unsigned char mybuf[20];
  int status;
  instr_data_s *priv = (instr_data_s *) info->private_data;

  if ((nibble % 4) != 0)
    abort ();

  status = (*info->read_memory_func) (priv->insn_start,
				      (bfd_byte *) mybuf,
				      nibble / 2,
				      info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, priv->insn_start, info);
      longjmp (priv->bailout, 1);
    }

  {
    int i;
    unsigned char *p = mybuf;

    for (i = 0; i < nibble;)
      {
	priv->words[i] = (p[0] << 8) | p[1];

	priv->bytes[i] = *p;
	priv->nibbles[i++] = *p >> 4;
	priv->nibbles[i++] = *p & 0xf;

	++p;
	priv->bytes[i] = *p;
	priv->nibbles[i++] = *p >> 4;
	priv->nibbles[i++] = *p & 0xf;

	++p;
      }
  }
  priv->max_fetched = nibble;
  return 1;
}

static char *codes[16] = {
  "f",
  "lt",
  "le",
  "ule",
  "ov/pe",
  "mi",
  "eq",
  "c/ult",
  "t",
  "ge",
  "gt",
  "ugt",
  "nov/po",
  "pl",
  "ne",
  "nc/uge"
};

static char *ctrl_names[8] = {
  "<invld>",
  "flags",
  "fcw",
  "refresh",
  "psapseg",
  "psapoff",
  "nspseg",
  "nspoff"
};

static int seg_length;
static int print_insn_z8k PARAMS ((bfd_vma, disassemble_info *, int));
int z8k_lookup_instr PARAMS ((unsigned char *, disassemble_info *));
static void output_instr
  PARAMS ((instr_data_s *, unsigned long, disassemble_info *));
static void unpack_instr PARAMS ((instr_data_s *, int, disassemble_info *));
static void unparse_instr PARAMS ((instr_data_s *, int));

static int
print_insn_z8k (addr, info, is_segmented)
     bfd_vma addr;
     disassemble_info *info;
     int is_segmented;
{
  instr_data_s instr_data;

  info->private_data = (PTR) &instr_data;
  instr_data.max_fetched = 0;
  instr_data.insn_start = addr;
  if (setjmp (instr_data.bailout) != 0)
    /* Error return.  */
    return -1;

  instr_data.tabl_index = z8k_lookup_instr (instr_data.nibbles, info);
  if (instr_data.tabl_index > 0)
    {
      unpack_instr (&instr_data, is_segmented, info);
      unparse_instr (&instr_data, is_segmented);
      output_instr (&instr_data, addr, info);
      return z8k_table[instr_data.tabl_index].length + seg_length;
    }
  else
    {
      FETCH_DATA (info, 4);
      (*info->fprintf_func) (info->stream, ".word %02x%02x",
			     instr_data.bytes[0], instr_data.bytes[2]);
      return 2;
    }
}

int
print_insn_z8001 (addr, info)
     bfd_vma addr;
     disassemble_info *info;
{
  return print_insn_z8k (addr, info, 1);
}

int
print_insn_z8002 (addr, info)
     bfd_vma addr;
     disassemble_info *info;
{
  return print_insn_z8k (addr, info, 0);
}

int
z8k_lookup_instr (nibbles, info)
     unsigned char *nibbles;
     disassemble_info *info;
{

  int nibl_index, tabl_index;
  int nibl_matched;
  unsigned short instr_nibl;
  unsigned short tabl_datum, datum_class, datum_value;

  nibl_matched = 0;
  tabl_index = 0;
  while (!nibl_matched && z8k_table[tabl_index].name)
    {
      nibl_matched = 1;
      for (nibl_index = 0;
	   nibl_index < z8k_table[tabl_index].length * 2 && nibl_matched;
	   nibl_index++)
	{
	  if ((nibl_index % 4) == 0)
	    /* Fetch one word at a time.  */
	    FETCH_DATA (info, nibl_index + 4);
	  instr_nibl = nibbles[nibl_index];

	  tabl_datum = z8k_table[tabl_index].byte_info[nibl_index];
	  datum_class = tabl_datum & CLASS_MASK;
	  datum_value = ~CLASS_MASK & tabl_datum;

	  switch (datum_class)
	    {
	    case CLASS_BIT:
	      if (datum_value != instr_nibl)
		nibl_matched = 0;
	      break;
	    case CLASS_00II:
	      if (!((~instr_nibl) & 0x4))
		nibl_matched = 0;
	      break;
	    case CLASS_01II:
	      if (!(instr_nibl & 0x4))
		nibl_matched = 0;
	      break;
	    case CLASS_0CCC:
	      if (!((~instr_nibl) & 0x8))
		nibl_matched = 0;
	      break;
	    case CLASS_1CCC:
	      if (!(instr_nibl & 0x8))
		nibl_matched = 0;
	      break;
	    case CLASS_0DISP7:
	      if (!((~instr_nibl) & 0x8))
		nibl_matched = 0;
	      nibl_index += 1;
	      break;
	    case CLASS_1DISP7:
	      if (!(instr_nibl & 0x8))
		nibl_matched = 0;
	      nibl_index += 1;
	      break;
	    case CLASS_REGN0:
	      if (instr_nibl == 0)
		nibl_matched = 0;
	      break;
	    case CLASS_BIT_1OR2:
	      if ((instr_nibl | 0x2) != (datum_value | 0x2))
		nibl_matched = 0;
	      break;
	    default:
	      break;
	    }
	}
      if (nibl_matched)
	{
	  return tabl_index;
	}

      tabl_index++;
    }
  return -1;
}

static void
output_instr (instr_data, addr, info)
     instr_data_s *instr_data;
     unsigned long addr ATTRIBUTE_UNUSED;
     disassemble_info *info;
{
  int loop, loop_limit;
  char tmp_str[20];
  char out_str[100];

  strcpy (out_str, "\t");

  loop_limit = (z8k_table[instr_data->tabl_index].length + seg_length) * 2;
  FETCH_DATA (info, loop_limit);
  for (loop = 0; loop < loop_limit; loop++)
    {
      sprintf (tmp_str, "%x", instr_data->nibbles[loop]);
      strcat (out_str, tmp_str);
    }

  while (loop++ < 8)
    {
      strcat (out_str, " ");
    }

  strcat (out_str, instr_data->instr_asmsrc);

  (*info->fprintf_func) (info->stream, "%s", out_str);
}

static void
unpack_instr (instr_data, is_segmented, info)
     instr_data_s *instr_data;
     int is_segmented;
     disassemble_info *info;
{
  int nibl_count, loop;
  unsigned short instr_nibl, instr_byte, instr_word;
  long instr_long;
  unsigned int tabl_datum, datum_class;
  unsigned short datum_value;

  nibl_count = 0;
  loop = 0;
  seg_length = 0;
  while (z8k_table[instr_data->tabl_index].byte_info[loop] != 0)
    {
      FETCH_DATA (info, nibl_count + 4 - (nibl_count % 4));
      instr_nibl = instr_data->nibbles[nibl_count];
      instr_byte = instr_data->bytes[nibl_count & ~1];
      instr_word = instr_data->words[nibl_count & ~3];

      tabl_datum = z8k_table[instr_data->tabl_index].byte_info[loop];
      datum_class = tabl_datum & CLASS_MASK;
      datum_value = tabl_datum & ~CLASS_MASK;

      switch (datum_class)
	{
	case CLASS_DISP:
	  switch (datum_value)
	    {
	    case ARG_DISP16:
	      instr_data->displacement = instr_data->insn_start + 4
		+ (signed short) (instr_word & 0xffff);
	      nibl_count += 3;
	      break;
	    case ARG_DISP12:
	      if (instr_word & 0x800)
		{
		  /* neg. 12 bit displacement */
		  instr_data->displacement = instr_data->insn_start + 2
		    - (signed short) ((instr_word & 0xfff) | 0xf000) * 2;
		}
	      else
		{
		  instr_data->displacement = instr_data->insn_start + 2
		    - (instr_word & 0x0fff) * 2;
		}
	      nibl_count += 2;
	      break;
	    default:
	      break;
	    }
	  break;
	case CLASS_IMM:
	  switch (datum_value)
	    {
	    case ARG_IMM4:
	      instr_data->immediate = instr_nibl;
	      break;
	    case ARG_NIM8:
	      instr_data->immediate = (-instr_byte);
	      nibl_count += 1;
	      break;
	    case ARG_IMM8:
	      instr_data->immediate = instr_byte;
	      nibl_count += 1;
	      break;
	    case ARG_IMM16:
	      instr_data->immediate = instr_word;
	      nibl_count += 3;
	      break;
	    case ARG_IMM32:
	      FETCH_DATA (info, nibl_count + 8);
	      instr_long = (instr_data->words[nibl_count] << 16)
		| (instr_data->words[nibl_count + 4]);
	      instr_data->immediate = instr_long;
	      nibl_count += 7;
	      break;
	    case ARG_IMMN:
	      instr_data->immediate = instr_nibl - 1;
	      break;
	    case ARG_IMM4M1:
	      instr_data->immediate = instr_nibl + 1;
	      break;
	    case ARG_IMM_1:
	      instr_data->immediate = 1;
	      break;
	    case ARG_IMM_2:
	      instr_data->immediate = 2;
	      break;
	    case ARG_IMM2:
	      instr_data->immediate = instr_nibl & 0x3;
	      break;
	    default:
	      break;
	    }
	  break;
	case CLASS_CC:
	  instr_data->cond_code = instr_nibl;
	  break;
	case CLASS_ADDRESS:
	  if (is_segmented)
	    {
	      if (instr_nibl & 0x8)
		{
		  FETCH_DATA (info, nibl_count + 8);
		  instr_long = (instr_data->words[nibl_count] << 16)
		    | (instr_data->words[nibl_count + 4]);
		  instr_data->address = ((instr_word & 0x7f00) << 8)
		    + (instr_long & 0xffff);
		  nibl_count += 7;
		  seg_length = 2;
		}
	      else
		{
		  instr_data->address = ((instr_word & 0x7f00) << 8)
		    + (instr_word & 0x00ff);
		  nibl_count += 3;
		}
	    }
	  else
	    {
	      instr_data->address = instr_word;
	      nibl_count += 3;
	    }
	  break;
	case CLASS_0CCC:
	case CLASS_1CCC:
	  instr_data->ctrl_code = instr_nibl & 0x7;
	  break;
	case CLASS_0DISP7:
	  instr_data->displacement =
	    instr_data->insn_start + 2 - (instr_byte & 0x7f) * 2;
	  nibl_count += 1;
	  break;
	case CLASS_1DISP7:
	  instr_data->displacement =
	    instr_data->insn_start + 2 - (instr_byte & 0x7f) * 2;
	  nibl_count += 1;
	  break;
	case CLASS_01II:
	  instr_data->interrupts = instr_nibl & 0x3;
	  break;
	case CLASS_00II:
	  instr_data->interrupts = instr_nibl & 0x3;
	  break;
	case CLASS_BIT:
	  instr_data->ctrl_code = instr_nibl & 0x7;
	  break;
	case CLASS_FLAGS:
	  instr_data->flags = instr_nibl;
	  break;
	case CLASS_REG:
	  instr_data->arg_reg[datum_value] = instr_nibl;
	  break;
	case CLASS_REGN0:
	  instr_data->arg_reg[datum_value] = instr_nibl;
	  break;
	case CLASS_DISP8:
	  instr_data->displacement =
	    instr_data->insn_start + 2 + (signed char) instr_byte * 2;
	  nibl_count += 1;
	  break;
	default:
	  abort ();
	  break;
	}

      loop += 1;
      nibl_count += 1;
    }
}

static void
unparse_instr (instr_data, is_segmented)
     instr_data_s *instr_data;
     int is_segmented;
{
  unsigned short datum_value;
  unsigned int tabl_datum, datum_class;
  int loop, loop_limit;
  char out_str[80], tmp_str[25];

  sprintf (out_str, "\t%s\t", z8k_table[instr_data->tabl_index].name);

  loop_limit = z8k_table[instr_data->tabl_index].noperands;
  for (loop = 0; loop < loop_limit; loop++)
    {
      if (loop)
	strcat (out_str, ",");

      tabl_datum = z8k_table[instr_data->tabl_index].arg_info[loop];
      datum_class = tabl_datum & CLASS_MASK;
      datum_value = tabl_datum & ~CLASS_MASK;

      switch (datum_class)
	{
	case CLASS_X:
	  sprintf (tmp_str, "0x%0lx(R%ld)", instr_data->address,
		   instr_data->arg_reg[datum_value]);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_BA:
	  sprintf (tmp_str, "r%ld(#%lx)", instr_data->arg_reg[datum_value],
		   instr_data->immediate);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_BX:
	  sprintf (tmp_str, "r%ld(R%ld)", instr_data->arg_reg[datum_value],
		   instr_data->arg_reg[ARG_RX]);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_DISP:
	  sprintf (tmp_str, "0x%0lx", instr_data->displacement);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_IMM:
	  sprintf (tmp_str, "#0x%0lx", instr_data->immediate);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_CC:
	  sprintf (tmp_str, "%s", codes[instr_data->cond_code]);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_CTRL:
	  sprintf (tmp_str, "%s", ctrl_names[instr_data->ctrl_code]);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_DA:
	case CLASS_ADDRESS:
	  sprintf (tmp_str, "0x%0lx", instr_data->address);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_IR:
	  if (is_segmented)
	    sprintf (tmp_str, "@rr%ld", instr_data->arg_reg[datum_value]);
	  else
	    sprintf (tmp_str, "@r%ld", instr_data->arg_reg[datum_value]);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_FLAGS:
	  sprintf (tmp_str, "0x%0lx", instr_data->flags);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_REG_BYTE:
	  if (instr_data->arg_reg[datum_value] >= 0x8)
	    sprintf (tmp_str, "rl%ld",
		     instr_data->arg_reg[datum_value] - 0x8);
	  else
	    sprintf (tmp_str, "rh%ld", instr_data->arg_reg[datum_value]);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_REG_WORD:
	  sprintf (tmp_str, "r%ld", instr_data->arg_reg[datum_value]);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_REG_QUAD:
	  sprintf (tmp_str, "rq%ld", instr_data->arg_reg[datum_value]);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_REG_LONG:
	  sprintf (tmp_str, "rr%ld", instr_data->arg_reg[datum_value]);
	  strcat (out_str, tmp_str);
	  break;
	case CLASS_PR:
	  if (is_segmented)
	    sprintf (tmp_str, "rr%ld", instr_data->arg_reg[datum_value]);
	  else
	    sprintf (tmp_str, "r%ld", instr_data->arg_reg[datum_value]);
	  strcat (out_str, tmp_str);
	  break;
	default:
	  abort ();
	  break;
	}
    }

  strcpy (instr_data->instr_asmsrc, out_str);
}
