/* Table of opcodes for the AMD 29000
   Copyright (C) 1990, 1991 Free Software Foundation, Inc.

This file is part of GDB and GAS.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

struct a29k_opcode {
  /* Name of the instruction.  */
  char *name;

  /* Opcode word */
  unsigned long opcode;

  /* A string of characters which describe the operands.
     Valid characters are:
     ,        Itself.  The character appears in the assembly code.
     a        RA.  The register number is in bits 8-15 of the instruction.
     b        RB.  The register number is in bits 0-7 of the instruction.
     c        RC.  The register number is in bits 16-23 of the instruction.
     i        An immediate operand is in bits 0-7 of the instruction.
     x        Bits 0-7 and 16-23 of the instruction are bits 0-7 and 8-15
              (respectively) of the immediate operand.
     h        Same as x but the instruction contains bits 16-31 of the
              immediate operand.
     X        Same as x but bits 16-31 of the signed immediate operand
              are set to 1 (thus the operand is always negative).
     P,A      Bits 0-7 and 16-23 of the instruction are bits 2-9 and 10-17
              (respectively) of the immediate operand.
	      P=PC-relative, sign-extended to 32 bits.
	      A=Absolute, zero-extended to 32 bits.
     e        CE bit (bit 23) for a load/store instruction.
     n        Control field (bits 16-22) for a load/store instruction.
     v        Immediate operand in bits 16-23 of the instruction.
              (used for trap numbers).
     s        SA.  Special-purpose register number in bits 8-15
              of the instruction.
     u        UI--bit 7 of the instruction.
     r        RND--bits 4-6 of the instruction.
     d        FD--bits 2-3 of the instruction.
     f        FS--bits 0-1 of the instruction.

     Extensions for 29050:

     d	      FMT--bits 2-3 of the instruction (not really new).
     f	      ACN--bits 0-1 of the instruction (not really new).
     F	      FUNC--Special function in bits 18-21 of the instruction.
     C	      ACN--bits 16-17 specifying the accumlator register.  */
  char *args;
};

#ifndef CONST
#define CONST
#endif /* CONST */

static CONST struct a29k_opcode a29k_opcodes[] =
{

{ "add", 0x14000000, "c,a,b" },
{ "add", 0x15000000, "c,a,i" },
{ "addc", 0x1c000000, "c,a,b" },
{ "addc", 0x1d000000, "c,a,i" },
{ "addcs", 0x18000000, "c,a,b" },
{ "addcs", 0x19000000, "c,a,i" },
{ "addcu", 0x1a000000, "c,a,b" },
{ "addcu", 0x1b000000, "c,a,i" },
{ "adds", 0x10000000, "c,a,b" },
{ "adds", 0x11000000, "c,a,i" },
{ "addu", 0x12000000, "c,a,b" },
{ "addu", 0x13000000, "c,a,i" },
{ "and", 0x90000000, "c,a,b" },
{ "and", 0x91000000, "c,a,i" },
{ "andn", 0x9c000000, "c,a,b" },
{ "andn", 0x9d000000, "c,a,i" },
{ "aseq", 0x70000000, "v,a,b" },
{ "aseq", 0x71000000, "v,a,i" },
{ "asge", 0x5c000000, "v,a,b" },
{ "asge", 0x5d000000, "v,a,i" },
{ "asgeu", 0x5e000000, "v,a,b" },
{ "asgeu", 0x5f000000, "v,a,i" },
{ "asgt", 0x58000000, "v,a,b" },
{ "asgt", 0x59000000, "v,a,i" },
{ "asgtu", 0x5a000000, "v,a,b" },
{ "asgtu", 0x5b000000, "v,a,i" },
{ "asle", 0x54000000, "v,a,b" },
{ "asle", 0x55000000, "v,a,i" },
{ "asleu", 0x56000000, "v,a,b" },
{ "asleu", 0x57000000, "v,a,i" },
{ "aslt", 0x50000000, "v,a,b" },
{ "aslt", 0x51000000, "v,a,i" },
{ "asltu", 0x52000000, "v,a,b" },
{ "asltu", 0x53000000, "v,a,i" },
{ "asneq", 0x72000000, "v,a,b" },
{ "asneq", 0x73000000, "v,a,i" },
{ "call", 0xa8000000, "a,P" },
{ "call", 0xa9000000, "a,A" },
{ "calli", 0xc8000000, "a,b" },
{ "class", 0xe6000000, "c,a,f" },
{ "clz", 0x08000000, "c,b" },
{ "clz", 0x09000000, "c,i" },
{ "const", 0x03000000, "a,x" },
{ "consth", 0x02000000, "a,h" },
{ "consthz", 0x05000000, "a,h" },
{ "constn", 0x01000000, "a,X" },
{ "convert", 0xe4000000, "c,a,u,r,d,f" },
{ "cpbyte", 0x2e000000, "c,a,b" },
{ "cpbyte", 0x2f000000, "c,a,i" },
{ "cpeq", 0x60000000, "c,a,b" },
{ "cpeq", 0x61000000, "c,a,i" },
{ "cpge", 0x4c000000, "c,a,b" },
{ "cpge", 0x4d000000, "c,a,i" },
{ "cpgeu", 0x4e000000, "c,a,b" },
{ "cpgeu", 0x4f000000, "c,a,i" },
{ "cpgt", 0x48000000, "c,a,b" },
{ "cpgt", 0x49000000, "c,a,i" },
{ "cpgtu", 0x4a000000, "c,a,b" },
{ "cpgtu", 0x4b000000, "c,a,i" },
{ "cple", 0x44000000, "c,a,b" },
{ "cple", 0x45000000, "c,a,i" },
{ "cpleu", 0x46000000, "c,a,b" },
{ "cpleu", 0x47000000, "c,a,i" },
{ "cplt", 0x40000000, "c,a,b" },
{ "cplt", 0x41000000, "c,a,i" },
{ "cpltu", 0x42000000, "c,a,b" },
{ "cpltu", 0x43000000, "c,a,i" },
{ "cpneq", 0x62000000, "c,a,b" },
{ "cpneq", 0x63000000, "c,a,i" },
{ "dadd", 0xf1000000, "c,a,b" },
{ "ddiv", 0xf7000000, "c,a,b" },
{ "deq", 0xeb000000, "c,a,b" },
{ "dge", 0xef000000, "c,a,b" },
{ "dgt", 0xed000000, "c,a,b" },
{ "div", 0x6a000000, "c,a,b" },
{ "div", 0x6b000000, "c,a,i" },
{ "div0", 0x68000000, "c,b" },
{ "div0", 0x69000000, "c,i" },
{ "divide", 0xe1000000, "c,a,b" },
{ "dividu", 0xe3000000, "c,a,b" },
{ "divl", 0x6c000000, "c,a,b" },
{ "divl", 0x6d000000, "c,a,i" },
{ "divrem", 0x6e000000, "c,a,b" },
{ "divrem", 0x6f000000, "c,a,i" },
{ "dmac", 0xd9000000, "F,C,a,b" },
{ "dmsm", 0xdb000000, "c,a,b" },
{ "dmul", 0xf5000000, "c,a,b" },
{ "dsub", 0xf3000000, "c,a,b" },
{ "emulate", 0xd7000000, "v,a,b" },
{ "exbyte", 0x0a000000, "c,a,b" },
{ "exbyte", 0x0b000000, "c,a,i" },
{ "exhw", 0x7c000000, "c,a,b" },
{ "exhw", 0x7d000000, "c,a,i" },
{ "exhws", 0x7e000000, "c,a" },
{ "extract", 0x7a000000, "c,a,b" },
{ "extract", 0x7b000000, "c,a,i" },
{ "fadd", 0xf0000000, "c,a,b" },
{ "fdiv", 0xf6000000, "c,a,b" },
{ "fdmul", 0xf9000000, "c,a,b" },
{ "feq", 0xea000000, "c,a,b" },
{ "fge", 0xee000000, "c,a,b" },
{ "fgt", 0xec000000, "c,a,b" },
{ "fmac", 0xd8000000, "F,C,a,b" },
{ "fmsm", 0xda000000, "c,a,b" },
{ "fmul", 0xf4000000, "c,a,b" },
{ "fsub", 0xf2000000, "c,a,b" },
{ "halt", 0x89000000, "" },
{ "inbyte", 0x0c000000, "c,a,b" },
{ "inbyte", 0x0d000000, "c,a,i" },
{ "inhw", 0x78000000, "c,a,b" },
{ "inhw", 0x79000000, "c,a,i" },
{ "inv", 0x9f000000, "" },
{ "iret", 0x88000000, "" },
{ "iretinv", 0x8c000000, "" },
{ "jmp", 0xa0000000, "P" },
{ "jmp", 0xa1000000, "A" },
{ "jmpf", 0xa4000000, "a,P" },
{ "jmpf", 0xa5000000, "a,A" },
{ "jmpfdec", 0xb4000000, "a,P" },
{ "jmpfdec", 0xb5000000, "a,A" },
{ "jmpfi", 0xc4000000, "a,b" },
{ "jmpi", 0xc0000000, "b" },
{ "jmpt", 0xac000000, "a,P" },
{ "jmpt", 0xad000000, "a,A" },
{ "jmpti", 0xcc000000, "a,b" },
{ "load", 0x16000000, "e,n,a,b" },
{ "load", 0x17000000, "e,n,a,i" },
{ "loadl", 0x06000000, "e,n,a,b" },
{ "loadl", 0x07000000, "e,n,a,i" },
{ "loadm", 0x36000000, "e,n,a,b" },
{ "loadm", 0x37000000, "e,n,a,i" },
{ "loadset", 0x26000000, "e,n,a,b" },
{ "loadset", 0x27000000, "e,n,a,i" },
{ "mfacc", 0xe9000100, "c,d,f" },
{ "mfsr", 0xc6000000, "c,s" },
{ "mftlb", 0xb6000000, "c,a" },
{ "mtacc", 0xe8010000, "a,d,f" },
{ "mtsr", 0xce000000, "s,b" },
{ "mtsrim", 0x04000000, "s,x" },
{ "mttlb", 0xbe000000, "a,b" },
{ "mul", 0x64000000, "c,a,b" },
{ "mul", 0x65000000, "c,a,i" },
{ "mull", 0x66000000, "c,a,b" },
{ "mull", 0x67000000, "c,a,i" },
{ "multiplu", 0xe2000000, "c,a,b" },
{ "multiply", 0xe0000000, "c,a,b" },
{ "multm", 0xde000000, "c,a,b" },
{ "multmu", 0xdf000000, "c,a,b" },
{ "mulu", 0x74000000, "c,a,b" },
{ "mulu", 0x75000000, "c,a,i" },
{ "nand", 0x9a000000, "c,a,b" },
{ "nand", 0x9b000000, "c,a,i" },
{ "nop", 0x70400101, "" },
{ "nor", 0x98000000, "c,a,b" },
{ "nor", 0x99000000, "c,a,i" },
{ "or", 0x92000000, "c,a,b" },
{ "or", 0x93000000, "c,a,i" },
{ "orn", 0xaa000000, "c,a,b" },
{ "orn", 0xab000000, "c,a,i" },

/* The description of "setip" in Chapter 8 ("instruction set") of the user's
   manual claims that these are absolute register numbers.  But section
   7.2.1 explains that they are not.  The latter is correct, so print
   these normally ("lr0", "lr5", etc.).  */
{ "setip", 0x9e000000, "c,a,b" },

{ "sll", 0x80000000, "c,a,b" },
{ "sll", 0x81000000, "c,a,i" },
{ "sqrt", 0xe5000000, "c,a,f" },
{ "sra", 0x86000000, "c,a,b" },
{ "sra", 0x87000000, "c,a,i" },
{ "srl", 0x82000000, "c,a,b" },
{ "srl", 0x83000000, "c,a,i" },
{ "store", 0x1e000000, "e,n,a,b" },
{ "store", 0x1f000000, "e,n,a,i" },
{ "storel", 0x0e000000, "e,n,a,b" },
{ "storel", 0x0f000000, "e,n,a,i" },
{ "storem", 0x3e000000, "e,n,a,b" },
{ "storem", 0x3f000000, "e,n,a,i" },
{ "sub", 0x24000000, "c,a,b" },
{ "sub", 0x25000000, "c,a,i" },
{ "subc", 0x2c000000, "c,a,b" },
{ "subc", 0x2d000000, "c,a,i" },
{ "subcs", 0x28000000, "c,a,b" },
{ "subcs", 0x29000000, "c,a,i" },
{ "subcu", 0x2a000000, "c,a,b" },
{ "subcu", 0x2b000000, "c,a,i" },
{ "subr", 0x34000000, "c,a,b" },
{ "subr", 0x35000000, "c,a,i" },
{ "subrc", 0x3c000000, "c,a,b" },
{ "subrc", 0x3d000000, "c,a,i" },
{ "subrcs", 0x38000000, "c,a,b" },
{ "subrcs", 0x39000000, "c,a,i" },
{ "subrcu", 0x3a000000, "c,a,b" },
{ "subrcu", 0x3b000000, "c,a,i" },
{ "subrs", 0x30000000, "c,a,b" },
{ "subrs", 0x31000000, "c,a,i" },
{ "subru", 0x32000000, "c,a,b" },
{ "subru", 0x33000000, "c,a,i" },
{ "subs", 0x20000000, "c,a,b" },
{ "subs", 0x21000000, "c,a,i" },
{ "subu", 0x22000000, "c,a,b" },
{ "subu", 0x23000000, "c,a,i" },
{ "xnor", 0x96000000, "c,a,b" },
{ "xnor", 0x97000000, "c,a,i" },
{ "xor", 0x94000000, "c,a,b" },
{ "xor", 0x95000000, "c,a,i" },

{ "", 0x0, "" }		/* Dummy entry, not included in NUM_OPCODES.  This
			   lets code examine entry i+1 without checking
			   if we've run off the end of the table.  */
};

CONST unsigned int num_opcodes = (((sizeof a29k_opcodes) / (sizeof a29k_opcodes[0])) - 1);

/*
 * $Log: a29k.h,v $
 * Revision 1.1  1993/11/03 00:55:48  paul
 * Brought over NetBSD's gas ready for pk's shared libs.
 *
 * Revision 1.1  1993/10/02  21:00:40  pk
 * GNU gas 1.92.3 based assembler supporting PIC code (for i386 and sparc).
 *
 * Revision 1.2  1992/02/29  17:10:43  rich
 * various smallish fixes from mail archives
 *
 * Revision 1.1.1.1  1992/02/24  02:34:30  rich
 * devo fork
 *
 * Revision 1.1  1991/12/01  02:22:19  sac
 * Initial revision
 *
 * Revision 1.5  1991/11/07  16:59:19  sac
 * Fixed encoding of mtacc instruction.
 *
 * Revision 1.4  1991/08/06  07:20:27  rich
 * Fixing CONST declarations.
 *
 * Revision 1.3  1991/08/05  22:31:05  rich
 * *** empty log message ***
 *
 * Revision 1.2  1991/07/15  23:34:04  steve
 * *** empty log message ***
 *
 * Revision 1.1  1991/05/19  00:19:33  rich
 * Initial revision
 *
 * Revision 1.1.1.1  1991/04/04  18:15:23  rich
 * new gas main line
 *
 * Revision 1.1  1991/04/04  18:15:23  rich
 * Initial revision
 *
 * Revision 1.2  1991/03/30  17:13:19  rich
 * num_opcodes now unsigned.  Also, added rcsid and log.
 *
 *
 */

/* end of a29k-opcode.h */
