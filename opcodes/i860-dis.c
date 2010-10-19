/* Disassembler for the i860.
   Copyright 2000, 2003 Free Software Foundation, Inc.

   Contributed by Jason Eckhardt <jle@cygnus.com>.

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

#include "dis-asm.h"
#include "opcode/i860.h"

/* Later we should probably choose the prefix based on which OS flavor.  */
#define I860_REG_PREFIX "%"

/* Integer register names (encoded as 0..31 in the instruction).  */
static const char *const grnames[] = 
 {"r0",  "r1",  "sp",  "fp",  "r4",  "r5",  "r6",  "r7",
  "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"};

/* FP register names (encoded as 0..31 in the instruction).  */
static const char *const frnames[] = 
 {"f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
  "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"};

/* Control/status register names (encoded as 0..11 in the instruction).
   Registers bear, ccr, p0, p1, p2 and p3 are XP only.  */
static const char *const crnames[] = 
 {"fir", "psr", "dirbase", "db", "fsr", "epsr", "bear", "ccr",
  "p0", "p1", "p2", "p3", "--", "--", "--", "--" };



/* True if opcode is xor, xorh, and, andh, or, orh, andnot, andnoth.  */
#define BITWISE_OP(op)  ((op) == 0x30 || (op) == 0x31		\
			 || (op) == 0x34 || (op) == 0x35	\
			 || (op) == 0x38 || (op) == 0x39	\
			 || (op) == 0x3c || (op) == 0x3d	\
			 || (op) == 0x33 || (op) == 0x37	\
			 || (op) == 0x3b || (op) == 0x3f)


/* Sign extend N-bit number.  */
static int
sign_ext (unsigned int x, int n)
{
  int t;
  t = x >> (n - 1);
  t = ((-t) << n) | x;
  return t;
}


/* Print a PC-relative branch offset.  VAL is the sign extended value
   from the branch instruction.  */
static void
print_br_address (disassemble_info *info, bfd_vma memaddr, long val)
{

  long adj = (long)memaddr + 4 + (val << 2);

  (*info->fprintf_func) (info->stream, "0x%08lx", adj);
	    
  /* Attempt to obtain a symbol for the target address.  */
	
  if (info->print_address_func && adj != 0)
    {
      (*info->fprintf_func) (info->stream, "\t// ");
      (*info->print_address_func) (adj, info);
    }
}


/* Print one instruction.  */
int
print_insn_i860 (bfd_vma memaddr, disassemble_info *info)
{
  bfd_byte buff[4];
  unsigned int insn, i;
  int status;
  const struct i860_opcode *opcode = 0;

  status = (*info->read_memory_func) (memaddr, buff, sizeof (buff), info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }

  /* Note that i860 instructions are always accessed as little endian
     data, regardless of the endian mode of the i860.  */
  insn = bfd_getl32 (buff);

  status = 0;
  i = 0;
  while (i860_opcodes[i].name != NULL)
    {
      opcode = &i860_opcodes[i];
      if ((insn & opcode->match) == opcode->match
	  && (insn & opcode->lose) == 0)
	{
	  status = 1;
	  break;
	}
      ++i;
    }

  if (status == 0)
    {
      /* Instruction not in opcode table.  */
      (*info->fprintf_func) (info->stream, ".long %#08x", insn);
    }
  else
    {
      const char *s;
      int val;

      /* If this a flop (or a shrd) and its dual bit is set,
         prefix with 'd.'.  */ 	
      if (((insn & 0xfc000000) == 0x48000000
           || (insn & 0xfc000000) == 0xb0000000)
          && (insn & 0x200))
	(*info->fprintf_func) (info->stream, "d.%s\t", opcode->name);
      else
	(*info->fprintf_func) (info->stream, "%s\t", opcode->name);

      for (s = opcode->args; *s; s++)
	{
	  switch (*s)
	    {
	    /* Integer register (src1).  */
	    case '1':
	      (*info->fprintf_func) (info->stream, "%s%s", I860_REG_PREFIX,
				     grnames[(insn >> 11) & 0x1f]);
	      break;

	    /* Integer register (src2).  */
	    case '2':
	      (*info->fprintf_func) (info->stream, "%s%s", I860_REG_PREFIX,
				     grnames[(insn >> 21) & 0x1f]);
	      break;

	    /* Integer destination register.  */
	    case 'd':
	      (*info->fprintf_func) (info->stream, "%s%s", I860_REG_PREFIX,
				     grnames[(insn >> 16) & 0x1f]);
	      break;

	    /* Floating-point register (src1).  */
	    case 'e':
	      (*info->fprintf_func) (info->stream, "%s%s", I860_REG_PREFIX,
				     frnames[(insn >> 11) & 0x1f]);
	      break;

	    /* Floating-point register (src2).  */
	    case 'f':
	      (*info->fprintf_func) (info->stream, "%s%s", I860_REG_PREFIX,
				     frnames[(insn >> 21) & 0x1f]);
	      break;

	    /* Floating-point destination register.  */
	    case 'g':
	      (*info->fprintf_func) (info->stream, "%s%s", I860_REG_PREFIX,
				     frnames[(insn >> 16) & 0x1f]);
	      break;

	    /* Control register.  */
	    case 'c':
	      (*info->fprintf_func) (info->stream, "%s%s", I860_REG_PREFIX,
				     crnames[(insn >> 21) & 0xf]);
	      break;

	    /* 16-bit immediate (sign extend, except for bitwise ops).  */
	    case 'i':
	      if (BITWISE_OP ((insn & 0xfc000000) >> 26))
		(*info->fprintf_func) (info->stream, "0x%04x",
				       (unsigned int) (insn & 0xffff));
	      else
		(*info->fprintf_func) (info->stream, "%d",
				       sign_ext ((insn & 0xffff), 16));
	      break;

	    /* 16-bit immediate, aligned (2^0, ld.b).  */
	    case 'I':
	      (*info->fprintf_func) (info->stream, "%d",
				     sign_ext ((insn & 0xffff), 16));
	      break;

	    /* 16-bit immediate, aligned (2^1, ld.s).  */
	    case 'J':
	      (*info->fprintf_func) (info->stream, "%d",
				     sign_ext ((insn & 0xfffe), 16));
	      break;

	    /* 16-bit immediate, aligned (2^2, ld.l, {p}fld.l, fst.l).  */
	    case 'K':
	      (*info->fprintf_func) (info->stream, "%d",
				     sign_ext ((insn & 0xfffc), 16));
	      break;

	    /* 16-bit immediate, aligned (2^3, {p}fld.d, fst.d).  */
	    case 'L':
	      (*info->fprintf_func) (info->stream, "%d",
				     sign_ext ((insn & 0xfff8), 16));
	      break;

	    /* 16-bit immediate, aligned (2^4, {p}fld.q, fst.q).  */
	    case 'M':
	      (*info->fprintf_func) (info->stream, "%d",
				     sign_ext ((insn & 0xfff0), 16));
	      break;

	    /* 5-bit immediate (zero extend).  */
	    case '5':
	      (*info->fprintf_func) (info->stream, "%d",
				     ((insn >> 11) & 0x1f));
	      break;

	    /* Split 16 bit immediate (20..16:10..0).  */
	    case 's':
	      val = ((insn >> 5) & 0xf800) | (insn & 0x07ff);
	      (*info->fprintf_func) (info->stream, "%d",
				     sign_ext (val, 16));
	      break;

	    /* Split 16 bit immediate, aligned. (2^0, st.b).  */
	    case 'S':
	      val = ((insn >> 5) & 0xf800) | (insn & 0x07ff);
	      (*info->fprintf_func) (info->stream, "%d",
				     sign_ext (val, 16));
	      break;

	    /* Split 16 bit immediate, aligned. (2^1, st.s).  */
	    case 'T':
	      val = ((insn >> 5) & 0xf800) | (insn & 0x07fe);
	      (*info->fprintf_func) (info->stream, "%d",
				     sign_ext (val, 16));
	      break;

	    /* Split 16 bit immediate, aligned. (2^2, st.l).  */
	    case 'U':
	      val = ((insn >> 5) & 0xf800) | (insn & 0x07fc);
	      (*info->fprintf_func) (info->stream, "%d",
				     sign_ext (val, 16));
	      break;

	    /* 26-bit PC relative immediate (lbroff).  */
	    case 'l':
	      val = sign_ext ((insn & 0x03ffffff), 26);
	      print_br_address (info, memaddr, val);
	      break;

	    /* 16-bit PC relative immediate (sbroff).  */
	    case 'r':
	      val = sign_ext ((((insn >> 5) & 0xf800) | (insn & 0x07ff)), 16);
	      print_br_address (info, memaddr, val);
	      break;

	    default:
	      (*info->fprintf_func) (info->stream, "%c", *s);
	      break;
	    }
	}
    }

  return sizeof (insn);
}

