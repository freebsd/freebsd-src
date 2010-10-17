/* pj-dis.c -- Disassemble picoJava instructions.
   Copyright 1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Steve Chamberlain, of Transmeta (sac@pobox.com).

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
#include "sysdep.h"
#include "opcode/pj.h"
#include "dis-asm.h"

extern const pj_opc_info_t pj_opc_info[512];

static int get_int PARAMS ((bfd_vma, int *, struct disassemble_info *));


static int
get_int (memaddr, iptr, info)
     bfd_vma memaddr;
     int *iptr;
     struct disassemble_info *info;
{
  unsigned char ival[4];

  int status = info->read_memory_func (memaddr, ival, 4, info);

  *iptr = (ival[0] << 24)
    | (ival[1] << 16)
    | (ival[2] << 8)
    | (ival[3] << 0);

  return status;
}

int
print_insn_pj (addr, info)
     bfd_vma addr;
     struct disassemble_info *info;
{
  fprintf_ftype fprintf_fn = info->fprintf_func;
  void *stream = info->stream;
  unsigned char opcode;
  int status;

  if ((status = info->read_memory_func (addr, &opcode, 1, info)))
    goto fail;

  if (opcode == 0xff)
    {
      unsigned char byte_2;
      if ((status = info->read_memory_func (addr + 1, &byte_2, 1, info)))
	goto fail;
      fprintf_fn (stream, "%s\t", pj_opc_info[opcode + byte_2].u.name);
      return 2;
    }
  else
    {
      char *sep = "\t";
      int insn_start = addr;
      const pj_opc_info_t *op = &pj_opc_info[opcode];
      int a;
      addr++;
      fprintf_fn (stream, "%s", op->u.name);

      /* The tableswitch instruction is followed by the default
	 address, low value, high value and the destinations.  */

      if (strcmp (op->u.name, "tableswitch") == 0)
	{
	  int lowval;
	  int highval;
	  int val;

	  addr = (addr + 3) & ~3;
	  if ((status = get_int (addr, &val, info)))
	    goto fail;

	  fprintf_fn (stream, " default: ");
	  (*info->print_address_func) (val + insn_start, info);
	  addr += 4;

	  if ((status = get_int (addr, &lowval, info)))
	    goto fail;
	  addr += 4;

	  if ((status = get_int (addr, &highval, info)))
	    goto fail;
	  addr += 4;

	  while (lowval <= highval)
	    {
	      if ((status = get_int (addr, &val, info)))
		goto fail;
	      fprintf_fn (stream, " %d:[", lowval);
	      (*info->print_address_func) (val + insn_start, info);
	      fprintf_fn (stream, " ]");
	      addr += 4;
	      lowval++;
	    }
	  return addr - insn_start;
	}

      /* The lookupswitch instruction is followed by the default
	 address, element count and pairs of values and
	 addresses.  */

      if (strcmp (op->u.name, "lookupswitch") == 0)
	{
	  int count;
	  int val;

	  addr = (addr + 3) & ~3;
	  if ((status = get_int (addr, &val, info)))
	    goto fail;
	  addr += 4;

	  fprintf_fn (stream, " default: ");
	  (*info->print_address_func) (val + insn_start, info);

	  if ((status = get_int (addr, &count, info)))
	    goto fail;
	  addr += 4;

	  while (count--)
	    {
	      if ((status = get_int (addr, &val, info)))
		goto fail;
	      addr += 4;
	      fprintf_fn (stream, " %d:[", val);

	      if ((status = get_int (addr, &val, info)))
		goto fail;
	      addr += 4;

	      (*info->print_address_func) (val + insn_start, info);
	      fprintf_fn (stream, " ]");
	    }
	  return addr - insn_start;
	}
      for (a = 0; op->arg[a]; a++)
	{
	  unsigned char data[4];
	  int val = 0;
	  int i;
	  int size = ASIZE (op->arg[a]);

	  if ((status = info->read_memory_func (addr, data, size, info)))
	    goto fail;

	  val = (UNS (op->arg[0]) || ((data[0] & 0x80) == 0)) ? 0 : -1;

	  for (i = 0; i < size; i++)
	    val = (val << 8) | (data[i] & 0xff);

	  if (PCREL (op->arg[a]))
	    (*info->print_address_func) (val + insn_start, info);
	  else
	    fprintf_fn (stream, "%s%d", sep, val);

	  sep = ",";
	  addr += size;
	}
      return op->len;
    }

 fail:
  info->memory_error_func (status, addr, info);
  return -1;
}
