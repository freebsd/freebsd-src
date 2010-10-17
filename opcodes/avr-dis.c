/* Disassemble AVR instructions.
   Copyright 1999, 2000 Free Software Foundation, Inc.

   Contributed by Denis Chertykov <denisc@overta.ru>

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

#include <assert.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "opintl.h"
#include "libiberty.h"

struct avr_opcodes_s
{
  char *name;
  char *constraints;
  char *opcode;
  int insn_size;		/* in words */
  int isa;
  unsigned int bin_opcode;
};

#define AVR_INSN(NAME, CONSTR, OPCODE, SIZE, ISA, BIN) \
{#NAME, CONSTR, OPCODE, SIZE, ISA, BIN},

const struct avr_opcodes_s avr_opcodes[] =
{
  #include "opcode/avr.h"
  {NULL, NULL, NULL, 0, 0, 0}
};

static int avr_operand PARAMS ((unsigned int, unsigned int,
				unsigned int, int, char *, char *, int));

static int
avr_operand (insn, insn2, pc, constraint, buf, comment, regs)
     unsigned int insn;
     unsigned int insn2;
     unsigned int pc;
     int constraint;
     char *buf;
     char *comment;
     int regs;
{
  int ok = 1;

  switch (constraint)
    {
      /* Any register operand.  */
    case 'r':
      if (regs)
	insn = (insn & 0xf) | ((insn & 0x0200) >> 5); /* source register */
      else
	insn = (insn & 0x01f0) >> 4; /* destination register */
      
      sprintf (buf, "r%d", insn);
      break;

    case 'd':
      if (regs)
	sprintf (buf, "r%d", 16 + (insn & 0xf));
      else
	sprintf (buf, "r%d", 16 + ((insn & 0xf0) >> 4));
      break;
      
    case 'w':
      sprintf (buf, "r%d", 24 + ((insn & 0x30) >> 3));
      break;
      
    case 'a':
      if (regs)
	sprintf (buf, "r%d", 16 + (insn & 7));
      else
	sprintf (buf, "r%d", 16 + ((insn >> 4) & 7));
      break;

    case 'v':
      if (regs)
	sprintf (buf, "r%d", (insn & 0xf) * 2);
      else
	sprintf (buf, "r%d", ((insn & 0xf0) >> 3));
      break;

    case 'e':
      {
	char *xyz;

	switch (insn & 0x100f)
	  {
	    case 0x0000: xyz = "Z";  break;
	    case 0x1001: xyz = "Z+"; break;
	    case 0x1002: xyz = "-Z"; break;
	    case 0x0008: xyz = "Y";  break;
	    case 0x1009: xyz = "Y+"; break;
	    case 0x100a: xyz = "-Y"; break;
	    case 0x100c: xyz = "X";  break;
	    case 0x100d: xyz = "X+"; break;
	    case 0x100e: xyz = "-X"; break;
	    default: xyz = "??"; ok = 0;
	  }
	sprintf (buf, xyz);

	if (AVR_UNDEF_P (insn))
	  sprintf (comment, _("undefined"));
      }
      break;

    case 'z':
      *buf++ = 'Z';
      if (insn & 0x1)
	*buf++ = '+';
      *buf = '\0';
      if (AVR_UNDEF_P (insn))
	sprintf (comment, _("undefined"));
      break;

    case 'b':
      {
	unsigned int x;
	
	x = (insn & 7);
	x |= (insn >> 7) & (3 << 3);
	x |= (insn >> 8) & (1 << 5);
	
	if (insn & 0x8)
	  *buf++ = 'Y';
	else
	  *buf++ = 'Z';
	sprintf (buf, "+%d", x);
	sprintf (comment, "0x%02x", x);
      }
      break;
      
    case 'h':
      sprintf (buf, "0x%x",
	       ((((insn & 1) | ((insn & 0x1f0) >> 3)) << 16) | insn2) * 2);
      break;
      
    case 'L':
      {
	int rel_addr = (((insn & 0xfff) ^ 0x800) - 0x800) * 2;
	sprintf (buf, ".%+-8d", rel_addr);
	sprintf (comment, "0x%x", pc + 2 + rel_addr);
      }
      break;

    case 'l':
      {
	int rel_addr = ((((insn >> 3) & 0x7f) ^ 0x40) - 0x40) * 2;
	sprintf (buf, ".%+-8d", rel_addr);
	sprintf (comment, "0x%x", pc + 2 + rel_addr);
      }
      break;

    case 'i':
      sprintf (buf, "0x%04X", insn2);
      break;
      
    case 'M':
      sprintf (buf, "0x%02X", ((insn & 0xf00) >> 4) | (insn & 0xf));
      sprintf (comment, "%d", ((insn & 0xf00) >> 4) | (insn & 0xf));
      break;

    case 'n':
      sprintf (buf, "??");
      fprintf (stderr, _("Internal disassembler error"));
      ok = 0;
      break;
      
    case 'K':
      {
	unsigned int x;

	x = (insn & 0xf) | ((insn >> 2) & 0x30);
	sprintf (buf, "0x%02x", x);
	sprintf (comment, "%d", x);
      }
      break;
      
    case 's':
      sprintf (buf, "%d", insn & 7);
      break;
      
    case 'S':
      sprintf (buf, "%d", (insn >> 4) & 7);
      break;
      
    case 'P':
      {
	unsigned int x;
	x = (insn & 0xf);
	x |= (insn >> 5) & 0x30;
	sprintf (buf, "0x%02x", x);
	sprintf (comment, "%d", x);
      }
      break;

    case 'p':
      {
	unsigned int x;
	
	x = (insn >> 3) & 0x1f;
	sprintf (buf, "0x%02x", x);
	sprintf (comment, "%d", x);
      }
      break;
      
    case '?':
      *buf = '\0';
      break;
      
    default:
      sprintf (buf, "??");
      fprintf (stderr, _("unknown constraint `%c'"), constraint);
      ok = 0;
    }

    return ok;
}

static unsigned short avrdis_opcode PARAMS ((bfd_vma, disassemble_info *));

static unsigned short
avrdis_opcode (addr, info)
     bfd_vma addr;
     disassemble_info *info;
{
  bfd_byte buffer[2];
  int status;
  status = info->read_memory_func(addr, buffer, 2, info);
  if (status != 0)
    {
      info->memory_error_func(status, addr, info);
      return -1;
    }
  return bfd_getl16 (buffer);
}


int
print_insn_avr(addr, info)
     bfd_vma addr;
     disassemble_info *info;
{
  unsigned int insn, insn2;
  const struct avr_opcodes_s *opcode;
  static unsigned int *maskptr;
  void *stream = info->stream;
  fprintf_ftype prin = info->fprintf_func;
  static unsigned int *avr_bin_masks;
  static int initialized;
  int cmd_len = 2;
  int ok = 0;
  char op1[20], op2[20], comment1[40], comment2[40];

  if (!initialized)
    {
      unsigned int nopcodes;

      nopcodes = sizeof (avr_opcodes) / sizeof (struct avr_opcodes_s);
      
      avr_bin_masks = (unsigned int *)
	xmalloc (nopcodes * sizeof (unsigned int));

      for (opcode = avr_opcodes, maskptr = avr_bin_masks;
	   opcode->name;
	   opcode++, maskptr++)
	{
	  char * s;
	  unsigned int bin = 0;
	  unsigned int mask = 0;
	
	  for (s = opcode->opcode; *s; ++s)
	    {
	      bin <<= 1;
	      mask <<= 1;
	      bin |= (*s == '1');
	      mask |= (*s == '1' || *s == '0');
	    }
	  assert (s - opcode->opcode == 16);
	  assert (opcode->bin_opcode == bin);
	  *maskptr = mask;
	}

      initialized = 1;
    }

  insn = avrdis_opcode (addr, info);
  
  for (opcode = avr_opcodes, maskptr = avr_bin_masks;
       opcode->name;
       opcode++, maskptr++)
    {
      if ((insn & *maskptr) == opcode->bin_opcode)
	break;
    }
  
  /* Special case: disassemble `ldd r,b+0' as `ld r,b', and
     `std b+0,r' as `st b,r' (next entry in the table).  */

  if (AVR_DISP0_P (insn))
    opcode++;

  op1[0] = 0;
  op2[0] = 0;
  comment1[0] = 0;
  comment2[0] = 0;

  if (opcode->name)
    {
      char *op = opcode->constraints;

      insn2 = 0;
      ok = 1;

      if (opcode->insn_size > 1)
	{
	  insn2 = avrdis_opcode (addr + 2, info);
	  cmd_len = 4;
	}

      if (*op && *op != '?')
	{
	  int regs = REGISTER_P (*op);

	  ok = avr_operand (insn, insn2, addr, *op, op1, comment1, 0);

	  if (ok && *(++op) == ',')
	    ok = avr_operand (insn, insn2, addr, *(++op), op2,
			      *comment1 ? comment2 : comment1, regs);
	}
    }

  if (!ok)
    {
      /* Unknown opcode, or invalid combination of operands.  */
      sprintf (op1, "0x%04x", insn);
      op2[0] = 0;
      sprintf (comment1, "????");
      comment2[0] = 0;
    }

  (*prin) (stream, "%s", ok ? opcode->name : ".word");

  if (*op1)
    (*prin) (stream, "\t%s", op1);

  if (*op2)
    (*prin) (stream, ", %s", op2);

  if (*comment1)
    (*prin) (stream, "\t; %s", comment1);

  if (*comment2)
    (*prin) (stream, " %s", comment2);

  return cmd_len;
}
