/* Instruction printing code for the AMD 29000
   Copyright 1990, 1993, 1994, 1995, 1998, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by Jim Kingdon.

This file is part of GDB.

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
#include "opcode/a29k.h"

static void print_general PARAMS ((int, struct disassemble_info *));
static void print_special PARAMS ((unsigned int, struct disassemble_info *));
static int is_delayed_branch PARAMS ((int));
static void find_bytes_little
  PARAMS ((char *, unsigned char *, unsigned char *, unsigned char *,
	   unsigned char *));
static void find_bytes_big
  PARAMS ((char *, unsigned char *, unsigned char *, unsigned char *,
	   unsigned char *));
static int print_insn PARAMS ((bfd_vma, struct disassemble_info *));


/* Print a symbolic representation of a general-purpose
   register number NUM on STREAM.
   NUM is a number as found in the instruction, not as found in
   debugging symbols; it must be in the range 0-255.  */
static void
print_general (num, info)
     int num;
     struct disassemble_info *info;
{
  if (num < 128)
    (*info->fprintf_func) (info->stream, "gr%d", num);
  else
    (*info->fprintf_func) (info->stream, "lr%d", num - 128);
}

/* Like print_general but a special-purpose register.

   The mnemonics used by the AMD assembler are not quite the same
   as the ones in the User's Manual.  We use the ones that the
   assembler uses.  */
static void
print_special (num, info)
     unsigned int num;
     struct disassemble_info *info;
{
  /* Register names of registers 0-SPEC0_NUM-1.  */
  static char *spec0_names[] = {
    "vab", "ops", "cps", "cfg", "cha", "chd", "chc", "rbp", "tmc", "tmr",
    "pc0", "pc1", "pc2", "mmu", "lru", "rsn", "rma0", "rmc0", "rma1", "rmc1",
    "spc0", "spc1", "spc2", "iba0", "ibc0", "iba1", "ibc1", "dba", "dbc",
    "cir", "cdr"
    };
#define SPEC0_NUM ((sizeof spec0_names) / (sizeof spec0_names[0]))

  /* Register names of registers 128-128+SPEC128_NUM-1.  */
  static char *spec128_names[] = {
    "ipc", "ipa", "ipb", "q", "alu", "bp", "fc", "cr"
    };
#define SPEC128_NUM ((sizeof spec128_names) / (sizeof spec128_names[0]))

  /* Register names of registers 160-160+SPEC160_NUM-1.  */
  static char *spec160_names[] = {
    "fpe", "inte", "fps", "sr163", "exop"
    };
#define SPEC160_NUM ((sizeof spec160_names) / (sizeof spec160_names[0]))

  if (num < SPEC0_NUM)
    (*info->fprintf_func) (info->stream, spec0_names[num]);
  else if (num >= 128 && num < 128 + SPEC128_NUM)
    (*info->fprintf_func) (info->stream, spec128_names[num-128]);
  else if (num >= 160 && num < 160 + SPEC160_NUM)
    (*info->fprintf_func) (info->stream, spec160_names[num-160]);
  else
    (*info->fprintf_func) (info->stream, "sr%d", num);
}

/* Is an instruction with OPCODE a delayed branch?  */
static int
is_delayed_branch (opcode)
     int opcode;
{
  return (opcode == 0xa8 || opcode == 0xa9 || opcode == 0xa0 || opcode == 0xa1
	  || opcode == 0xa4 || opcode == 0xa5
	  || opcode == 0xb4 || opcode == 0xb5
	  || opcode == 0xc4 || opcode == 0xc0
	  || opcode == 0xac || opcode == 0xad
	  || opcode == 0xcc);
}

/* Now find the four bytes of INSN and put them in *INSN{0,8,16,24}.  */
static void
find_bytes_big (insn, insn0, insn8, insn16, insn24)
     char *insn;
     unsigned char *insn0;
     unsigned char *insn8;
     unsigned char *insn16;
     unsigned char *insn24;
{
  *insn24 = insn[0];
  *insn16 = insn[1];
  *insn8  = insn[2];
  *insn0  = insn[3];
}

static void
find_bytes_little (insn, insn0, insn8, insn16, insn24)
     char *insn;
     unsigned char *insn0;
     unsigned char *insn8;
     unsigned char *insn16;
     unsigned char *insn24;
{
  *insn24 = insn[3];
  *insn16 = insn[2];
  *insn8 = insn[1];
  *insn0 = insn[0];
}

typedef void (*find_byte_func_type)
     PARAMS ((char *, unsigned char *, unsigned char *,
	      unsigned char *, unsigned char *));

/* Print one instruction from MEMADDR on INFO->STREAM.
   Return the size of the instruction (always 4 on a29k).  */

static int
print_insn (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  /* The raw instruction.  */
  char insn[4];

  /* The four bytes of the instruction.  */
  unsigned char insn24, insn16, insn8, insn0;

  find_byte_func_type find_byte_func = (find_byte_func_type)info->private_data;

  struct a29k_opcode const * opcode;

  {
    int status =
      (*info->read_memory_func) (memaddr, (bfd_byte *) &insn[0], 4, info);
    if (status != 0)
      {
	(*info->memory_error_func) (status, memaddr, info);
	return -1;
      }
  }

  (*find_byte_func) (insn, &insn0, &insn8, &insn16, &insn24);

  printf ("%02x%02x%02x%02x ", insn24, insn16, insn8, insn0);

  /* Handle the nop (aseq 0x40,gr1,gr1) specially */
  if ((insn24==0x70) && (insn16==0x40) && (insn8==0x01) && (insn0==0x01)) {
    (*info->fprintf_func) (info->stream,"nop");
    return 4;
  }

  /* The opcode is always in insn24.  */
  for (opcode = &a29k_opcodes[0];
       opcode < &a29k_opcodes[num_opcodes];
       ++opcode)
    {
      if (((unsigned long) insn24 << 24) == opcode->opcode)
	{
	  char *s;

	  (*info->fprintf_func) (info->stream, "%s ", opcode->name);
	  for (s = opcode->args; *s != '\0'; ++s)
	    {
	      switch (*s)
		{
		case 'a':
		  print_general (insn8, info);
		  break;

		case 'b':
		  print_general (insn0, info);
		  break;

		case 'c':
		  print_general (insn16, info);
		  break;

		case 'i':
		  (*info->fprintf_func) (info->stream, "%d", insn0);
		  break;

		case 'x':
		  (*info->fprintf_func) (info->stream, "0x%x", (insn16 << 8) + insn0);
		  break;

		case 'h':
		  /* This used to be %x for binutils.  */
		  (*info->fprintf_func) (info->stream, "0x%x",
				    (insn16 << 24) + (insn0 << 16));
		  break;

		case 'X':
		  (*info->fprintf_func) (info->stream, "%d",
				    ((insn16 << 8) + insn0) | 0xffff0000);
		  break;

		case 'P':
		  /* This output looks just like absolute addressing, but
		     maybe that's OK (it's what the GDB m68k and EBMON
		     a29k disassemblers do).  */
		  /* All the shifting is to sign-extend it.  p*/
		  (*info->print_address_func)
		    (memaddr +
		     (((int)((insn16 << 10) + (insn0 << 2)) << 14) >> 14),
		     info);
		  break;

		case 'A':
		  (*info->print_address_func)
		    ((insn16 << 10) + (insn0 << 2), info);
		  break;

		case 'e':
		  (*info->fprintf_func) (info->stream, "%d", insn16 >> 7);
		  break;

		case 'n':
		  (*info->fprintf_func) (info->stream, "0x%x", insn16 & 0x7f);
		  break;

		case 'v':
		  (*info->fprintf_func) (info->stream, "0x%x", insn16);
		  break;

		case 's':
		  print_special (insn8, info);
		  break;

		case 'u':
		  (*info->fprintf_func) (info->stream, "%d", insn0 >> 7);
		  break;

		case 'r':
		  (*info->fprintf_func) (info->stream, "%d", (insn0 >> 4) & 7);
		  break;

		case 'I':
		  if ((insn16 & 3) != 0)
		    (*info->fprintf_func) (info->stream, "%d", insn16 & 3);
		  break;

		case 'd':
		  (*info->fprintf_func) (info->stream, "%d", (insn0 >> 2) & 3);
		  break;

		case 'f':
		  (*info->fprintf_func) (info->stream, "%d", insn0 & 3);
		  break;

		case 'F':
		  (*info->fprintf_func) (info->stream, "%d", (insn16 >> 2) & 15);
		  break;

		case 'C':
		  (*info->fprintf_func) (info->stream, "%d", insn16 & 3);
		  break;

		default:
		  (*info->fprintf_func) (info->stream, "%c", *s);
		}
	    }

	  /* Now we look for a const,consth pair of instructions,
	     in which case we try to print the symbolic address.  */
	  if (insn24 == 2)  /* consth */
	    {
	      int errcode;
	      char prev_insn[4];
	      unsigned char prev_insn0, prev_insn8, prev_insn16, prev_insn24;

	      errcode = (*info->read_memory_func) (memaddr - 4,
						   (bfd_byte *) &prev_insn[0],
						   4,
						   info);
	      if (errcode == 0)
		{
		  /* If it is a delayed branch, we need to look at the
		     instruction before the delayed brach to handle
		     things like

		     const _foo
		     call _printf
		     consth _foo
		     */
		  (*find_byte_func) (prev_insn, &prev_insn0, &prev_insn8,
				     &prev_insn16, &prev_insn24);
		  if (is_delayed_branch (prev_insn24))
		    {
		      errcode = (*info->read_memory_func)
			(memaddr - 8, (bfd_byte *) &prev_insn[0], 4, info);
		      (*find_byte_func) (prev_insn, &prev_insn0, &prev_insn8,
					 &prev_insn16, &prev_insn24);
		    }
		}

	      /* If there was a problem reading memory, then assume
		 the previous instruction was not const.  */
	      if (errcode == 0)
		{
		  /* Is it const to the same register?  */
		  if (prev_insn24 == 3
		      && prev_insn8 == insn8)
		    {
		      (*info->fprintf_func) (info->stream, "\t; ");
		      (*info->print_address_func)
			(((insn16 << 24) + (insn0 << 16)
			  + (prev_insn16 << 8) + (prev_insn0)),
			 info);
		    }
		}
	    }

	  return 4;
	}
    }
  /* This used to be %8x for binutils.  */
  (*info->fprintf_func)
    (info->stream, ".word 0x%08x",
     (insn24 << 24) + (insn16 << 16) + (insn8 << 8) + insn0);
  return 4;
}

/* Disassemble an big-endian a29k instruction.  */
int
print_insn_big_a29k (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  info->private_data = (PTR) find_bytes_big;
  return print_insn (memaddr, info);
}

/* Disassemble a little-endian a29k instruction.  */
int
print_insn_little_a29k (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  info->private_data = (PTR) find_bytes_little;
  return print_insn (memaddr, info);
}
