/* m68hc11-dis.c -- Motorola 68HC11 & 68HC12 disassembly
   Copyright 1999, 2000, 2001, 2002, 2003, 2006
   Free Software Foundation, Inc.
   Written by Stephane Carrez (stcarrez@nerim.fr)

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

#include <stdio.h>

#include "ansidecl.h"
#include "opcode/m68hc11.h"
#include "dis-asm.h"

#define PC_REGNUM 3

static const char *const reg_name[] = {
  "X", "Y", "SP", "PC"
};

static const char *const reg_src_table[] = {
  "A", "B", "CCR", "TMP3", "D", "X", "Y", "SP"
};

static const char *const reg_dst_table[] = {
  "A", "B", "CCR", "TMP2", "D", "X", "Y", "SP"
};

#define OP_PAGE_MASK (M6811_OP_PAGE2|M6811_OP_PAGE3|M6811_OP_PAGE4)

/* Prototypes for local functions.  */
static int read_memory (bfd_vma, bfd_byte *, int, struct disassemble_info *);
static int print_indexed_operand (bfd_vma, struct disassemble_info *,
                                  int*, int, int, bfd_vma);
static int print_insn (bfd_vma, struct disassemble_info *, int);

static int
read_memory (bfd_vma memaddr, bfd_byte* buffer, int size,
             struct disassemble_info* info)
{
  int status;

  /* Get first byte.  Only one at a time because we don't know the
     size of the insn.  */
  status = (*info->read_memory_func) (memaddr, buffer, size, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }
  return 0;
}


/* Read the 68HC12 indexed operand byte and print the corresponding mode.
   Returns the number of bytes read or -1 if failure.  */
static int
print_indexed_operand (bfd_vma memaddr, struct disassemble_info* info,
                       int* indirect, int mov_insn, int pc_offset,
                       bfd_vma endaddr)
{
  bfd_byte buffer[4];
  int reg;
  int status;
  short sval;
  int pos = 1;

  if (indirect)
    *indirect = 0;

  status = read_memory (memaddr, &buffer[0], 1, info);
  if (status != 0)
    {
      return status;
    }

  /* n,r with 5-bits signed constant.  */
  if ((buffer[0] & 0x20) == 0)
    {
      reg = (buffer[0] >> 6) & 3;
      sval = (buffer[0] & 0x1f);
      if (sval & 0x10)
	sval |= 0xfff0;
      /* 68HC12 requires an adjustment for movb/movw pc relative modes.  */
      if (reg == PC_REGNUM && info->mach == bfd_mach_m6812 && mov_insn)
        sval += pc_offset;
      (*info->fprintf_func) (info->stream, "%d,%s",
			     (int) sval, reg_name[reg]);

      if (reg == PC_REGNUM)
        {
          (* info->fprintf_func) (info->stream, " {");
          (* info->print_address_func) (endaddr + sval, info);
          (* info->fprintf_func) (info->stream, "}");
        }
    }

  /* Auto pre/post increment/decrement.  */
  else if ((buffer[0] & 0xc0) != 0xc0)
    {
      const char *mode;

      reg = (buffer[0] >> 6) & 3;
      sval = (buffer[0] & 0x0f);
      if (sval & 0x8)
	{
	  sval |= 0xfff0;
	  sval = -sval;
	  mode = "-";
	}
      else
	{
	  sval = sval + 1;
	  mode = "+";
	}
      (*info->fprintf_func) (info->stream, "%d,%s%s%s",
			     (int) sval,
			     (buffer[0] & 0x10 ? "" : mode),
			     reg_name[reg], (buffer[0] & 0x10 ? mode : ""));
    }

  /* [n,r] 16-bits offset indexed indirect.  */
  else if ((buffer[0] & 0x07) == 3)
    {
      if (mov_insn)
	{
	  (*info->fprintf_func) (info->stream, "<invalid op: 0x%x>",
				 buffer[0] & 0x0ff);
	  return 0;
	}
      reg = (buffer[0] >> 3) & 0x03;
      status = read_memory (memaddr + pos, &buffer[0], 2, info);
      if (status != 0)
	{
	  return status;
	}

      pos += 2;
      sval = ((buffer[0] << 8) | (buffer[1] & 0x0FF));
      (*info->fprintf_func) (info->stream, "[%u,%s]",
			     sval & 0x0ffff, reg_name[reg]);
      if (indirect)
        *indirect = 1;
    }

  /* n,r with 9 and 16 bit signed constant.  */
  else if ((buffer[0] & 0x4) == 0)
    {
      if (mov_insn)
	{
	  (*info->fprintf_func) (info->stream, "<invalid op: 0x%x>",
				 buffer[0] & 0x0ff);
	  return 0;
	}
      reg = (buffer[0] >> 3) & 0x03;
      status = read_memory (memaddr + pos,
			    &buffer[1], (buffer[0] & 0x2 ? 2 : 1), info);
      if (status != 0)
	{
	  return status;
	}
      if (buffer[0] & 2)
	{
	  sval = ((buffer[1] << 8) | (buffer[2] & 0x0FF));
	  sval &= 0x0FFFF;
	  pos += 2;
          endaddr += 2;
	}
      else
	{
	  sval = buffer[1] & 0x00ff;
	  if (buffer[0] & 0x01)
	    sval |= 0xff00;
	  pos++;
          endaddr++;
	}
      (*info->fprintf_func) (info->stream, "%d,%s",
			     (int) sval, reg_name[reg]);
      if (reg == PC_REGNUM)
        {
          (* info->fprintf_func) (info->stream, " {");
          (* info->print_address_func) (endaddr + sval, info);
          (* info->fprintf_func) (info->stream, "}");
        }
    }
  else
    {
      reg = (buffer[0] >> 3) & 0x03;
      switch (buffer[0] & 3)
	{
	case 0:
	  (*info->fprintf_func) (info->stream, "A,%s", reg_name[reg]);
	  break;
	case 1:
	  (*info->fprintf_func) (info->stream, "B,%s", reg_name[reg]);
	  break;
	case 2:
	  (*info->fprintf_func) (info->stream, "D,%s", reg_name[reg]);
	  break;
	case 3:
	default:
	  (*info->fprintf_func) (info->stream, "[D,%s]", reg_name[reg]);
          if (indirect)
            *indirect = 1;
	  break;
	}
    }

  return pos;
}

/* Disassemble one instruction at address 'memaddr'.  Returns the number
   of bytes used by that instruction.  */
static int
print_insn (bfd_vma memaddr, struct disassemble_info* info, int arch)
{
  int status;
  bfd_byte buffer[4];
  unsigned char code;
  long format, pos, i;
  short sval;
  const struct m68hc11_opcode *opcode;

  /* Get first byte.  Only one at a time because we don't know the
     size of the insn.  */
  status = read_memory (memaddr, buffer, 1, info);
  if (status != 0)
    {
      return status;
    }

  format = 0;
  code = buffer[0];
  pos = 0;

  /* Look for page2,3,4 opcodes.  */
  if (code == M6811_OPCODE_PAGE2)
    {
      pos++;
      format = M6811_OP_PAGE2;
    }
  else if (code == M6811_OPCODE_PAGE3 && arch == cpu6811)
    {
      pos++;
      format = M6811_OP_PAGE3;
    }
  else if (code == M6811_OPCODE_PAGE4 && arch == cpu6811)
    {
      pos++;
      format = M6811_OP_PAGE4;
    }

  /* We are in page2,3,4; get the real opcode.  */
  if (pos == 1)
    {
      status = read_memory (memaddr + pos, &buffer[1], 1, info);
      if (status != 0)
	{
	  return status;
	}
      code = buffer[1];
    }


  /* Look first for a 68HC12 alias.  All of them are 2-bytes long and
     in page 1.  There is no operand to print.  We read the second byte
     only when we have a possible match.  */
  if ((arch & cpu6812) && format == 0)
    {
      int must_read = 1;

      /* Walk the alias table to find a code1+code2 match.  */
      for (i = 0; i < m68hc12_num_alias; i++)
	{
	  if (m68hc12_alias[i].code1 == code)
	    {
	      if (must_read)
		{
		  status = read_memory (memaddr + pos + 1,
					&buffer[1], 1, info);
		  if (status != 0)
		    break;

		  must_read = 1;
		}
	      if (m68hc12_alias[i].code2 == (unsigned char) buffer[1])
		{
		  (*info->fprintf_func) (info->stream, "%s",
					 m68hc12_alias[i].name);
		  return 2;
		}
	    }
	}
    }

  pos++;

  /* Scan the opcode table until we find the opcode
     with the corresponding page.  */
  opcode = m68hc11_opcodes;
  for (i = 0; i < m68hc11_num_opcodes; i++, opcode++)
    {
      int offset;
      int pc_src_offset;
      int pc_dst_offset = 0;

      if ((opcode->arch & arch) == 0)
	continue;
      if (opcode->opcode != code)
	continue;
      if ((opcode->format & OP_PAGE_MASK) != format)
	continue;

      if (opcode->format & M6812_OP_REG)
	{
	  int j;
	  int is_jump;

	  if (opcode->format & M6811_OP_JUMP_REL)
	    is_jump = 1;
	  else
	    is_jump = 0;

	  status = read_memory (memaddr + pos, &buffer[0], 1, info);
	  if (status != 0)
	    {
	      return status;
	    }
	  for (j = 0; i + j < m68hc11_num_opcodes; j++)
	    {
	      if ((opcode[j].arch & arch) == 0)
		continue;
	      if (opcode[j].opcode != code)
		continue;
	      if (is_jump)
		{
		  if (!(opcode[j].format & M6811_OP_JUMP_REL))
		    continue;

		  if ((opcode[j].format & M6812_OP_IBCC_MARKER)
		      && (buffer[0] & 0xc0) != 0x80)
		    continue;
		  if ((opcode[j].format & M6812_OP_TBCC_MARKER)
		      && (buffer[0] & 0xc0) != 0x40)
		    continue;
		  if ((opcode[j].format & M6812_OP_DBCC_MARKER)
		      && (buffer[0] & 0xc0) != 0)
		    continue;
		  if ((opcode[j].format & M6812_OP_EQ_MARKER)
		      && (buffer[0] & 0x20) == 0)
		    break;
		  if (!(opcode[j].format & M6812_OP_EQ_MARKER)
		      && (buffer[0] & 0x20) != 0)
		    break;
		  continue;
		}
	      if (opcode[j].format & M6812_OP_EXG_MARKER && buffer[0] & 0x80)
		break;
	      if ((opcode[j].format & M6812_OP_SEX_MARKER)
		  && (((buffer[0] & 0x07) >= 3 && (buffer[0] & 7) <= 7))
		  && ((buffer[0] & 0x0f0) <= 0x20))
		break;
	      if (opcode[j].format & M6812_OP_TFR_MARKER
		  && !(buffer[0] & 0x80))
		break;
	    }
	  if (i + j < m68hc11_num_opcodes)
	    opcode = &opcode[j];
	}

      /* We have found the opcode.  Extract the operand and print it.  */
      (*info->fprintf_func) (info->stream, "%s", opcode->name);

      format = opcode->format;
      if (format & (M6811_OP_MASK | M6811_OP_BITMASK
		    | M6811_OP_JUMP_REL | M6812_OP_JUMP_REL16))
	{
	  (*info->fprintf_func) (info->stream, "\t");
	}

      /* The movb and movw must be handled in a special way...
         The source constant 'ii' is not always at the same place.
         This is the same for the destination for the post-indexed byte.
         The 'offset' is used to do the appropriate correction.

                                   offset          offset
                              for constant     for destination
         movb   18 OB ii hh ll       0          0
                18 08 xb ii          1          -1
                18 0C hh ll hh ll    0          0
                18 09 xb hh ll       1          -1
                18 0D xb hh ll       0          0
                18 0A xb xb          0          0

         movw   18 03 jj kk hh ll    0          0
                18 00 xb jj kk       1          -1
                18 04 hh ll hh ll    0          0
                18 01 xb hh ll       1          -1
                18 05 xb hh ll       0          0
                18 02 xb xb          0          0

         After the source operand is read, the position 'pos' is incremented
         this explains the negative offset for destination.

         movb/movw above are the only instructions with this matching
         format.  */
      offset = ((format & M6812_OP_IDX_P2)
                && (format & (M6811_OP_IMM8 | M6811_OP_IMM16 |
                              M6811_OP_IND16)));

      /* Operand with one more byte: - immediate, offset,
         direct-low address.  */
      if (format &
	  (M6811_OP_IMM8 | M6811_OP_IX | M6811_OP_IY | M6811_OP_DIRECT))
	{
	  status = read_memory (memaddr + pos + offset, &buffer[0], 1, info);
	  if (status != 0)
	    {
	      return status;
	    }

	  pos++;

          /* This movb/movw is special (see above).  */
          offset = -offset;

          pc_dst_offset = 2;
	  if (format & M6811_OP_IMM8)
	    {
	      (*info->fprintf_func) (info->stream, "#%d", (int) buffer[0]);
	      format &= ~M6811_OP_IMM8;
              /* Set PC destination offset.  */
              pc_dst_offset = 1;
	    }
	  else if (format & M6811_OP_IX)
	    {
	      /* Offsets are in range 0..255, print them unsigned.  */
	      (*info->fprintf_func) (info->stream, "%u,x", buffer[0] & 0x0FF);
	      format &= ~M6811_OP_IX;
	    }
	  else if (format & M6811_OP_IY)
	    {
	      (*info->fprintf_func) (info->stream, "%u,y", buffer[0] & 0x0FF);
	      format &= ~M6811_OP_IY;
	    }
	  else if (format & M6811_OP_DIRECT)
	    {
	      (*info->fprintf_func) (info->stream, "*");
	      (*info->print_address_func) (buffer[0] & 0x0FF, info);
	      format &= ~M6811_OP_DIRECT;
	    }
	}

#define M6812_DST_MOVE  (M6812_OP_IND16_P2 | M6812_OP_IDX_P2)
#define M6812_INDEXED_FLAGS (M6812_OP_IDX|M6812_OP_IDX_1|M6812_OP_IDX_2)
      /* Analyze the 68HC12 indexed byte.  */
      if (format & M6812_INDEXED_FLAGS)
	{
          int indirect;
          bfd_vma endaddr;

          endaddr = memaddr + pos + 1;
          if (format & M6811_OP_IND16)
            endaddr += 2;
          pc_src_offset = -1;
          pc_dst_offset = 1;
	  status = print_indexed_operand (memaddr + pos, info, &indirect,
                                          (format & M6812_DST_MOVE),
                                          pc_src_offset, endaddr);
	  if (status < 0)
	    {
	      return status;
	    }
	  pos += status;

          /* The indirect addressing mode of the call instruction does
             not need the page code.  */
          if ((format & M6812_OP_PAGE) && indirect)
            format &= ~M6812_OP_PAGE;
	}

      /* 68HC12 dbcc/ibcc/tbcc operands.  */
      if ((format & M6812_OP_REG) && (format & M6811_OP_JUMP_REL))
	{
	  status = read_memory (memaddr + pos, &buffer[0], 2, info);
	  if (status != 0)
	    {
	      return status;
	    }
	  (*info->fprintf_func) (info->stream, "%s,",
				 reg_src_table[buffer[0] & 0x07]);
	  sval = buffer[1] & 0x0ff;
	  if (buffer[0] & 0x10)
	    sval |= 0xff00;

	  pos += 2;
	  (*info->print_address_func) (memaddr + pos + sval, info);
	  format &= ~(M6812_OP_REG | M6811_OP_JUMP_REL);
	}
      else if (format & (M6812_OP_REG | M6812_OP_REG_2))
	{
	  status = read_memory (memaddr + pos, &buffer[0], 1, info);
	  if (status != 0)
	    {
	      return status;
	    }

	  pos++;
	  (*info->fprintf_func) (info->stream, "%s,%s",
				 reg_src_table[(buffer[0] >> 4) & 7],
				 reg_dst_table[(buffer[0] & 7)]);
	}

      if (format & (M6811_OP_IMM16 | M6811_OP_IND16))
	{
	  int val;
          bfd_vma addr;
          unsigned page = 0;

	  status = read_memory (memaddr + pos + offset, &buffer[0], 2, info);
	  if (status != 0)
	    {
	      return status;
	    }
	  if (format & M6812_OP_IDX_P2)
	    offset = -2;
	  else
	    offset = 0;
	  pos += 2;

	  val = ((buffer[0] << 8) | (buffer[1] & 0x0FF));
	  val &= 0x0FFFF;
          addr = val;
          pc_dst_offset = 2;
          if (format & M6812_OP_PAGE)
            {
              status = read_memory (memaddr + pos + offset, buffer, 1, info);
              if (status != 0)
                return status;

              page = (unsigned) buffer[0];
              if (addr >= M68HC12_BANK_BASE && addr < 0x0c000)
                addr = ((val - M68HC12_BANK_BASE)
                        | (page << M68HC12_BANK_SHIFT))
                   + M68HC12_BANK_VIRT;
            }
          else if ((arch & cpu6812)
                   && addr >= M68HC12_BANK_BASE && addr < 0x0c000)
             {
                int cur_page;
                bfd_vma vaddr;
                
                if (memaddr >= M68HC12_BANK_VIRT)
                   cur_page = ((memaddr - M68HC12_BANK_VIRT)
                               >> M68HC12_BANK_SHIFT);
                else
                   cur_page = 0;

                vaddr = ((addr - M68HC12_BANK_BASE)
                         + (cur_page << M68HC12_BANK_SHIFT))
                   + M68HC12_BANK_VIRT;
                if (!info->symbol_at_address_func (addr, info)
                    && info->symbol_at_address_func (vaddr, info))
                   addr = vaddr;
             }
	  if (format & M6811_OP_IMM16)
	    {
	      format &= ~M6811_OP_IMM16;
	      (*info->fprintf_func) (info->stream, "#");
	    }
	  else
	    format &= ~M6811_OP_IND16;

	  (*info->print_address_func) (addr, info);
          if (format & M6812_OP_PAGE)
            {
              (* info->fprintf_func) (info->stream, " {");
              (* info->print_address_func) (val, info);
              (* info->fprintf_func) (info->stream, ", %d}", page);
              format &= ~M6812_OP_PAGE;
              pos += 1;
            }
	}

      if (format & M6812_OP_IDX_P2)
	{
	  (*info->fprintf_func) (info->stream, ", ");
	  status = print_indexed_operand (memaddr + pos + offset, info,
                                          0, 1, pc_dst_offset,
                                          memaddr + pos + offset + 1);
	  if (status < 0)
	    return status;
	  pos += status;
	}

      if (format & M6812_OP_IND16_P2)
	{
	  int val;

	  (*info->fprintf_func) (info->stream, ", ");

	  status = read_memory (memaddr + pos + offset, &buffer[0], 2, info);
	  if (status != 0)
	    {
	      return status;
	    }
	  pos += 2;

	  val = ((buffer[0] << 8) | (buffer[1] & 0x0FF));
	  val &= 0x0FFFF;
	  (*info->print_address_func) (val, info);
	}

      /* M6811_OP_BITMASK and M6811_OP_JUMP_REL must be treated separately
         and in that order.  The brset/brclr insn have a bitmask and then
         a relative branch offset.  */
      if (format & M6811_OP_BITMASK)
	{
	  status = read_memory (memaddr + pos, &buffer[0], 1, info);
	  if (status != 0)
	    {
	      return status;
	    }
	  pos++;
	  (*info->fprintf_func) (info->stream, " #$%02x%s",
				 buffer[0] & 0x0FF,
				 (format & M6811_OP_JUMP_REL ? " " : ""));
	  format &= ~M6811_OP_BITMASK;
	}
      if (format & M6811_OP_JUMP_REL)
	{
	  int val;

	  status = read_memory (memaddr + pos, &buffer[0], 1, info);
	  if (status != 0)
	    {
	      return status;
	    }

	  pos++;
	  val = (buffer[0] & 0x80) ? buffer[0] | 0xFFFFFF00 : buffer[0];
	  (*info->print_address_func) (memaddr + pos + val, info);
	  format &= ~M6811_OP_JUMP_REL;
	}
      else if (format & M6812_OP_JUMP_REL16)
	{
	  int val;

	  status = read_memory (memaddr + pos, &buffer[0], 2, info);
	  if (status != 0)
	    {
	      return status;
	    }

	  pos += 2;
	  val = ((buffer[0] << 8) | (buffer[1] & 0x0FF));
	  if (val & 0x8000)
	    val |= 0xffff0000;

	  (*info->print_address_func) (memaddr + pos + val, info);
	  format &= ~M6812_OP_JUMP_REL16;
	}

      if (format & M6812_OP_PAGE)
	{
	  int val;

	  status = read_memory (memaddr + pos + offset, &buffer[0], 1, info);
	  if (status != 0)
	    {
	      return status;
	    }
	  pos += 1;

	  val = buffer[0] & 0x0ff;
	  (*info->fprintf_func) (info->stream, ", %d", val);
	}
      
#ifdef DEBUG
      /* Consistency check.  'format' must be 0, so that we have handled
         all formats; and the computed size of the insn must match the
         opcode table content.  */
      if (format & ~(M6811_OP_PAGE4 | M6811_OP_PAGE3 | M6811_OP_PAGE2))
	{
	  (*info->fprintf_func) (info->stream, "; Error, format: %lx", format);
	}
      if (pos != opcode->size)
	{
	  (*info->fprintf_func) (info->stream, "; Error, size: %ld expect %d",
				 pos, opcode->size);
	}
#endif
      return pos;
    }

  /* Opcode not recognized.  */
  if (format == M6811_OP_PAGE2 && arch & cpu6812
      && ((code >= 0x30 && code <= 0x39) || (code >= 0x40)))
    (*info->fprintf_func) (info->stream, "trap\t#%d", code & 0x0ff);

  else if (format == M6811_OP_PAGE2)
    (*info->fprintf_func) (info->stream, ".byte\t0x%02x, 0x%02x",
			   M6811_OPCODE_PAGE2, code);
  else if (format == M6811_OP_PAGE3)
    (*info->fprintf_func) (info->stream, ".byte\t0x%02x, 0x%02x",
			   M6811_OPCODE_PAGE3, code);
  else if (format == M6811_OP_PAGE4)
    (*info->fprintf_func) (info->stream, ".byte\t0x%02x, 0x%02x",
			   M6811_OPCODE_PAGE4, code);
  else
    (*info->fprintf_func) (info->stream, ".byte\t0x%02x", code);

  return pos;
}

/* Disassemble one instruction at address 'memaddr'.  Returns the number
   of bytes used by that instruction.  */
int
print_insn_m68hc11 (bfd_vma memaddr, struct disassemble_info* info)
{
  return print_insn (memaddr, info, cpu6811);
}

int
print_insn_m68hc12 (bfd_vma memaddr, struct disassemble_info* info)
{
  return print_insn (memaddr, info, cpu6812);
}
