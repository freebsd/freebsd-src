/* Print mips instructions for GDB, the GNU debugger, or for objdump.
   Copyright 1989, 91-97, 1998 Free Software Foundation, Inc.
   Contributed by Nobuyuki Hikichi(hikichi@sra.co.jp).

This file is part of GDB, GAS, and the GNU binutils.

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

#include <ansidecl.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/mips.h"

/* FIXME: These are needed to figure out if this is a mips16 symbol or
   not.  It would be better to think of a cleaner way to do this.  */
#include "elf-bfd.h"
#include "elf/mips.h"

static int print_insn_mips16 PARAMS ((bfd_vma, struct disassemble_info *));
static void print_mips16_insn_arg
  PARAMS ((int, const struct mips_opcode *, int, boolean, int, bfd_vma,
	   struct disassemble_info *));

/* Mips instructions are never longer than this many bytes.  */
#define MAXLEN 4

static void print_insn_arg PARAMS ((const char *, unsigned long, bfd_vma,
				    struct disassemble_info *));
static int _print_insn_mips PARAMS ((bfd_vma, unsigned long int,
				     struct disassemble_info *));


/* FIXME: This should be shared with gdb somehow.  */
#define REGISTER_NAMES 	\
    {	"zero",	"at",	"v0",	"v1",	"a0",	"a1",	"a2",	"a3", \
	"t0",	"t1",	"t2",	"t3",	"t4",	"t5",	"t6",	"t7", \
	"s0",	"s1",	"s2",	"s3",	"s4",	"s5",	"s6",	"s7", \
	"t8",	"t9",	"k0",	"k1",	"gp",	"sp",	"s8",	"ra", \
	"sr",	"lo",	"hi",	"bad",	"cause","pc",    \
	"f0",   "f1",   "f2",   "f3",   "f4",   "f5",   "f6",   "f7", \
	"f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "f14",  "f15", \
	"f16",  "f17",  "f18",  "f19",  "f20",  "f21",  "f22",  "f23",\
	"f24",  "f25",  "f26",  "f27",  "f28",  "f29",  "f30",  "f31",\
	"fsr",  "fir",  "fp",   "inx",  "rand", "tlblo","ctxt", "tlbhi",\
	"epc",  "prid"\
    }

static CONST char * CONST reg_names[] = REGISTER_NAMES;

/* The mips16 register names.  */
static const char * const mips16_reg_names[] =
{
  "s0", "s1", "v0", "v1", "a0", "a1", "a2", "a3"
};

/* subroutine */
static void
print_insn_arg (d, l, pc, info)
     const char *d;
     register unsigned long int l;
     bfd_vma pc;
     struct disassemble_info *info;
{
  int delta;

  switch (*d)
    {
    case ',':
    case '(':
    case ')':
      (*info->fprintf_func) (info->stream, "%c", *d);
      break;

    case 's':
    case 'b':
    case 'r':
    case 'v':
      (*info->fprintf_func) (info->stream, "$%s",
			     reg_names[(l >> OP_SH_RS) & OP_MASK_RS]);
      break;

    case 't':
    case 'w':
      (*info->fprintf_func) (info->stream, "$%s",
			     reg_names[(l >> OP_SH_RT) & OP_MASK_RT]);
      break;

    case 'i':
    case 'u':
      (*info->fprintf_func) (info->stream, "0x%x",
			(l >> OP_SH_IMMEDIATE) & OP_MASK_IMMEDIATE);
      break;

    case 'j': /* same as i, but sign-extended */
    case 'o':
      delta = (l >> OP_SH_DELTA) & OP_MASK_DELTA;
      if (delta & 0x8000)
	delta |= ~0xffff;
      (*info->fprintf_func) (info->stream, "%d",
			     delta);
      break;

    case 'h':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (unsigned int) ((l >> OP_SH_PREFX)
					     & OP_MASK_PREFX));
      break;

    case 'k':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (unsigned int) ((l >> OP_SH_CACHE)
					     & OP_MASK_CACHE));
      break;

    case 'a':
      (*info->print_address_func)
	(((pc & 0xF0000000) | (((l >> OP_SH_TARGET) & OP_MASK_TARGET) << 2)),
	 info);
      break;

    case 'p':
      /* sign extend the displacement */
      delta = (l >> OP_SH_DELTA) & OP_MASK_DELTA;
      if (delta & 0x8000)
	delta |= ~0xffff;
      (*info->print_address_func)
	((delta << 2) + pc + 4,
	 info);
      break;

    case 'd':
      (*info->fprintf_func) (info->stream, "$%s",
			     reg_names[(l >> OP_SH_RD) & OP_MASK_RD]);
      break;

    case 'z':
      (*info->fprintf_func) (info->stream, "$%s", reg_names[0]);
      break;

    case '<':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_SHAMT) & OP_MASK_SHAMT);
      break;

    case 'c':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_CODE) & OP_MASK_CODE);
      break;

    case 'C':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_COPZ) & OP_MASK_COPZ);
      break;

    case 'B':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_SYSCALL) & OP_MASK_SYSCALL);
      break;

    case 'S':
    case 'V':
      (*info->fprintf_func) (info->stream, "$f%d",
			     (l >> OP_SH_FS) & OP_MASK_FS);
      break;


    case 'T':
    case 'W':
      (*info->fprintf_func) (info->stream, "$f%d",
			     (l >> OP_SH_FT) & OP_MASK_FT);
      break;

    case 'D':
      (*info->fprintf_func) (info->stream, "$f%d",
			     (l >> OP_SH_FD) & OP_MASK_FD);
      break;

    case 'R':
      (*info->fprintf_func) (info->stream, "$f%d",
			     (l >> OP_SH_FR) & OP_MASK_FR);
      break;

    case 'E':
      (*info->fprintf_func) (info->stream, "$%d",
			     (l >> OP_SH_RT) & OP_MASK_RT);
      break;

    case 'G':
      (*info->fprintf_func) (info->stream, "$%d",
			     (l >> OP_SH_RD) & OP_MASK_RD);
      break;

    case 'N':
      (*info->fprintf_func) (info->stream, "$fcc%d",
			     (l >> OP_SH_BCC) & OP_MASK_BCC);
      break;

    case 'M':
      (*info->fprintf_func) (info->stream, "$fcc%d",
			     (l >> OP_SH_CCC) & OP_MASK_CCC);
      break;

    case 'P':
      (*info->fprintf_func) (info->stream, "%d",
			     (l >> OP_SH_PERFREG) & OP_MASK_PERFREG);
      break;


    default:
      (*info->fprintf_func) (info->stream,
			     "# internal error, undefined modifier(%c)", *d);
      break;
    }
}

/* Print the mips instruction at address MEMADDR in debugged memory,
   on using INFO.  Returns length of the instruction, in bytes, which is
   always 4.  BIGENDIAN must be 1 if this is big-endian code, 0 if
   this is little-endian code.  */

static int
_print_insn_mips (memaddr, word, info)
     bfd_vma memaddr;
     unsigned long int word;
     struct disassemble_info *info;
{
  register const struct mips_opcode *op;
  int target_processor, mips_isa;
  static boolean init = 0;
  static const struct mips_opcode *mips_hash[OP_MASK_OP + 1];

  /* Build a hash table to shorten the search time.  */
  if (! init)
    {
      unsigned int i;

      for (i = 0; i <= OP_MASK_OP; i++)
	{
	  for (op = mips_opcodes; op < &mips_opcodes[NUMOPCODES]; op++)
	    {
	      if (op->pinfo == INSN_MACRO)
		continue;
	      if (i == ((op->match >> OP_SH_OP) & OP_MASK_OP))
		{
		  mips_hash[i] = op;
		  break;
		}
	    }
        }

      init = 1;
    }

  switch (info->mach)
    {
      case bfd_mach_mips3000:
	target_processor = 3000;
	mips_isa = 1;
	break;
      case bfd_mach_mips3900:
	target_processor = 3900;
	mips_isa = 1;
	break;
      case bfd_mach_mips4000:
	target_processor = 4000;
	mips_isa = 3;
	break;
      case bfd_mach_mips4010:
	target_processor = 4010;
	mips_isa = 2;
	break;
      case bfd_mach_mips4100:
	target_processor = 4100;
	mips_isa = 3;
	break;
      case bfd_mach_mips4300:
	target_processor = 4300;
	mips_isa = 3;
	break;
      case bfd_mach_mips4400:
	target_processor = 4400;
	mips_isa = 3;
	break;
      case bfd_mach_mips4600:
	target_processor = 4600;
	mips_isa = 3;
	break;
      case bfd_mach_mips4650:
	target_processor = 4650;
	mips_isa = 3;
	break;
      case bfd_mach_mips5000:
	target_processor = 5000;
	mips_isa = 4;
	break;
      case bfd_mach_mips6000:
	target_processor = 6000;
	mips_isa = 2;
	break;
      case bfd_mach_mips8000:
	target_processor = 8000;
	mips_isa = 4;
	break;
      case bfd_mach_mips10000:
	target_processor = 10000;
	mips_isa = 4;
	break;
      case bfd_mach_mips16:
	target_processor = 16;
	mips_isa = 3;
	break;
      default:
	target_processor = 3000;
	mips_isa = 3;
	break;

    }

  info->bytes_per_chunk = 4;
  info->display_endian = info->endian;

  op = mips_hash[(word >> OP_SH_OP) & OP_MASK_OP];
  if (op != NULL)
    {
      for (; op < &mips_opcodes[NUMOPCODES]; op++)
	{
	  if (op->pinfo != INSN_MACRO && (word & op->mask) == op->match)
	    {
	      register const char *d;
	      int insn_isa;

	      if ((op->membership & INSN_ISA) == INSN_ISA1)
		insn_isa = 1;
	      else if ((op->membership & INSN_ISA) == INSN_ISA2)
		insn_isa = 2;
	      else if ((op->membership & INSN_ISA) == INSN_ISA3)
		insn_isa = 3;
	      else if ((op->membership & INSN_ISA) == INSN_ISA4)
		insn_isa = 4;
	      else
		insn_isa = 15;

	      if (insn_isa > mips_isa
		  && (target_processor == 4650
		      && op->membership & INSN_4650) == 0
		  && (target_processor == 4010
		      && op->membership & INSN_4010) == 0
		  && (target_processor == 4100
		      && op->membership & INSN_4100) == 0
		  && (target_processor == 3900
		      && op->membership & INSN_3900) == 0)
		continue;

	      (*info->fprintf_func) (info->stream, "%s", op->name);

	      d = op->args;
	      if (d != NULL && *d != '\0')
		{
		    (*info->fprintf_func) (info->stream, "\t");
		  for (; *d != '\0'; d++)
		      print_insn_arg (d, word, memaddr, info);
		}

	      return 4;
	    }
	}
    }

  /* Handle undefined instructions.  */
  (*info->fprintf_func) (info->stream, "0x%x", word);
  return 4;
}

int
print_insn_big_mips (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  bfd_byte buffer[4];
  int status;

  if (info->mach == 16
      || (info->flavour == bfd_target_elf_flavour
	  && info->symbols != NULL
	  && ((*(elf_symbol_type **) info->symbols)->internal_elf_sym.st_other
	      == STO_MIPS16)))
    return print_insn_mips16 (memaddr, info);

  status = (*info->read_memory_func) (memaddr, buffer, 4, info);
  if (status == 0)
    return _print_insn_mips (memaddr, (unsigned long) bfd_getb32 (buffer),
			     info);
  else
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }
}

int
print_insn_little_mips (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  bfd_byte buffer[4];
  int status;


  if (info->mach == 16
      || (info->flavour == bfd_target_elf_flavour
	  && info->symbols != NULL
	  && ((*(elf_symbol_type **) info->symbols)->internal_elf_sym.st_other
	      == STO_MIPS16)))
    return print_insn_mips16 (memaddr, info);

  status = (*info->read_memory_func) (memaddr, buffer, 4, info);
  if (status == 0)
    return _print_insn_mips (memaddr, (unsigned long) bfd_getl32 (buffer),
			     info);
  else
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }
}

/* Disassemble mips16 instructions.  */

static int
print_insn_mips16 (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  int status;
  bfd_byte buffer[2];
  int length;
  int insn;
  boolean use_extend;
  int extend = 0;
  const struct mips_opcode *op, *opend;

  info->bytes_per_chunk = 2;
  info->display_endian = info->endian;

  info->insn_info_valid = 1;
  info->branch_delay_insns = 0;
  info->data_size = 0;
  info->insn_type = dis_nonbranch;
  info->target = 0;
  info->target2 = 0;

  status = (*info->read_memory_func) (memaddr, buffer, 2, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }

  length = 2;

  if (info->endian == BFD_ENDIAN_BIG)
    insn = bfd_getb16 (buffer);
  else
    insn = bfd_getl16 (buffer);

  /* Handle the extend opcode specially.  */
  use_extend = false;
  if ((insn & 0xf800) == 0xf000)
    {
      use_extend = true;
      extend = insn & 0x7ff;

      memaddr += 2;

      status = (*info->read_memory_func) (memaddr, buffer, 2, info);
      if (status != 0)
	{
	  (*info->fprintf_func) (info->stream, "extend 0x%x",
				 (unsigned int) extend);
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}

      if (info->endian == BFD_ENDIAN_BIG)
	insn = bfd_getb16 (buffer);
      else
	insn = bfd_getl16 (buffer);

      /* Check for an extend opcode followed by an extend opcode.  */
      if ((insn & 0xf800) == 0xf000)
	{
	  (*info->fprintf_func) (info->stream, "extend 0x%x",
				 (unsigned int) extend);
	  info->insn_type = dis_noninsn;
	  return length;
	}

      length += 2;
    }

  /* FIXME: Should probably use a hash table on the major opcode here.  */

  opend = mips16_opcodes + bfd_mips16_num_opcodes;
  for (op = mips16_opcodes; op < opend; op++)
    {
      if (op->pinfo != INSN_MACRO && (insn & op->mask) == op->match)
	{
	  const char *s;

	  if (strchr (op->args, 'a') != NULL)
	    {
	      if (use_extend)
		{
		  (*info->fprintf_func) (info->stream, "extend 0x%x",
					 (unsigned int) extend);
		  info->insn_type = dis_noninsn;
		  return length - 2;
		}

	      use_extend = false;

	      memaddr += 2;

	      status = (*info->read_memory_func) (memaddr, buffer, 2,
						  info);
	      if (status == 0)
		{
		  use_extend = true;
		  if (info->endian == BFD_ENDIAN_BIG)
		    extend = bfd_getb16 (buffer);
		  else
		    extend = bfd_getl16 (buffer);
		  length += 2;
		}
	    }

	  (*info->fprintf_func) (info->stream, "%s", op->name);
	  if (op->args[0] != '\0')
	    (*info->fprintf_func) (info->stream, "\t");

	  for (s = op->args; *s != '\0'; s++)
	    {
	      if (*s == ','
		  && s[1] == 'w'
		  && (((insn >> MIPS16OP_SH_RX) & MIPS16OP_MASK_RX)
		      == ((insn >> MIPS16OP_SH_RY) & MIPS16OP_MASK_RY)))
		{
		  /* Skip the register and the comma.  */
		  ++s;
		  continue;
		}
	      if (*s == ','
		  && s[1] == 'v'
		  && (((insn >> MIPS16OP_SH_RZ) & MIPS16OP_MASK_RZ)
		      == ((insn >> MIPS16OP_SH_RX) & MIPS16OP_MASK_RX)))
		{
		  /* Skip the register and the comma.  */
		  ++s;
		  continue;
		}
	      print_mips16_insn_arg (*s, op, insn, use_extend, extend, memaddr,
				     info);
	    }

	  if ((op->pinfo & INSN_UNCOND_BRANCH_DELAY) != 0)
	    {
	      info->branch_delay_insns = 1;
	      if (info->insn_type != dis_jsr)
		info->insn_type = dis_branch;
	    }

	  return length;
	}
    }

  if (use_extend)
    (*info->fprintf_func) (info->stream, "0x%x", extend | 0xf000);
  (*info->fprintf_func) (info->stream, "0x%x", insn);
  info->insn_type = dis_noninsn;

  return length;
}

/* Disassemble an operand for a mips16 instruction.  */

static void
print_mips16_insn_arg (type, op, l, use_extend, extend, memaddr, info)
     int type;
     const struct mips_opcode *op;
     int l;
     boolean use_extend;
     int extend;
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  switch (type)
    {
    case ',':
    case '(':
    case ')':
      (*info->fprintf_func) (info->stream, "%c", type);
      break;

    case 'y':
    case 'w':
      (*info->fprintf_func) (info->stream, "$%s",
			     mips16_reg_names[((l >> MIPS16OP_SH_RY)
					       & MIPS16OP_MASK_RY)]);
      break;

    case 'x':
    case 'v':
      (*info->fprintf_func) (info->stream, "$%s",
			     mips16_reg_names[((l >> MIPS16OP_SH_RX)
					       & MIPS16OP_MASK_RX)]);
      break;

    case 'z':
      (*info->fprintf_func) (info->stream, "$%s",
			     mips16_reg_names[((l >> MIPS16OP_SH_RZ)
					       & MIPS16OP_MASK_RZ)]);
      break;

    case 'Z':
      (*info->fprintf_func) (info->stream, "$%s",
			     mips16_reg_names[((l >> MIPS16OP_SH_MOVE32Z)
					       & MIPS16OP_MASK_MOVE32Z)]);
      break;

    case '0':
      (*info->fprintf_func) (info->stream, "$%s", reg_names[0]);
      break;

    case 'S':
      (*info->fprintf_func) (info->stream, "$%s", reg_names[29]);
      break;

    case 'P':
      (*info->fprintf_func) (info->stream, "$pc");
      break;

    case 'R':
      (*info->fprintf_func) (info->stream, "$%s", reg_names[31]);
      break;

    case 'X':
      (*info->fprintf_func) (info->stream, "$%s",
			     reg_names[((l >> MIPS16OP_SH_REGR32)
					& MIPS16OP_MASK_REGR32)]);
      break;

    case 'Y':
      (*info->fprintf_func) (info->stream, "$%s",
			     reg_names[MIPS16OP_EXTRACT_REG32R (l)]);
      break;

    case '<':
    case '>':
    case '[':
    case ']':
    case '4':
    case '5':
    case 'H':
    case 'W':
    case 'D':
    case 'j':
    case '6':
    case '8':
    case 'V':
    case 'C':
    case 'U':
    case 'k':
    case 'K':
    case 'p':
    case 'q':
    case 'A':
    case 'B':
    case 'E':
      {
	int immed, nbits, shift, signedp, extbits, pcrel, extu, branch;

	shift = 0;
	signedp = 0;
	extbits = 16;
	pcrel = 0;
	extu = 0;
	branch = 0;
	switch (type)
	  {
	  case '<':
	    nbits = 3;
	    immed = (l >> MIPS16OP_SH_RZ) & MIPS16OP_MASK_RZ;
	    extbits = 5;
	    extu = 1;
	    break;
	  case '>':
	    nbits = 3;
	    immed = (l >> MIPS16OP_SH_RX) & MIPS16OP_MASK_RX;
	    extbits = 5;
	    extu = 1;
	    break;
	  case '[':
	    nbits = 3;
	    immed = (l >> MIPS16OP_SH_RZ) & MIPS16OP_MASK_RZ;
	    extbits = 6;
	    extu = 1;
	    break;
	  case ']':
	    nbits = 3;
	    immed = (l >> MIPS16OP_SH_RX) & MIPS16OP_MASK_RX;
	    extbits = 6;
	    extu = 1;
	    break;
	  case '4':
	    nbits = 4;
	    immed = (l >> MIPS16OP_SH_IMM4) & MIPS16OP_MASK_IMM4;
	    signedp = 1;
	    extbits = 15;
	    break;
	  case '5':
	    nbits = 5;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    info->insn_type = dis_dref;
	    info->data_size = 1;
	    break;
	  case 'H':
	    nbits = 5;
	    shift = 1;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    info->insn_type = dis_dref;
	    info->data_size = 2;
	    break;
	  case 'W':
	    nbits = 5;
	    shift = 2;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    if ((op->pinfo & MIPS16_INSN_READ_PC) == 0
		&& (op->pinfo & MIPS16_INSN_READ_SP) == 0)
	      {
		info->insn_type = dis_dref;
		info->data_size = 4;
	      }
	    break;
	  case 'D':
	    nbits = 5;
	    shift = 3;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    info->insn_type = dis_dref;
	    info->data_size = 8;
	    break;
	  case 'j':
	    nbits = 5;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    signedp = 1;
	    break;
	  case '6':
	    nbits = 6;
	    immed = (l >> MIPS16OP_SH_IMM6) & MIPS16OP_MASK_IMM6;
	    break;
	  case '8':
	    nbits = 8;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    break;
	  case 'V':
	    nbits = 8;
	    shift = 2;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    /* FIXME: This might be lw, or it might be addiu to $sp or
               $pc.  We assume it's load.  */
	    info->insn_type = dis_dref;
	    info->data_size = 4;
	    break;
	  case 'C':
	    nbits = 8;
	    shift = 3;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    info->insn_type = dis_dref;
	    info->data_size = 8;
	    break;
	  case 'U':
	    nbits = 8;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    extu = 1;
	    break;
	  case 'k':
	    nbits = 8;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    signedp = 1;
	    break;
	  case 'K':
	    nbits = 8;
	    shift = 3;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    signedp = 1;
	    break;
	  case 'p':
	    nbits = 8;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    signedp = 1;
	    pcrel = 1;
	    branch = 1;
	    info->insn_type = dis_condbranch;
	    break;
	  case 'q':
	    nbits = 11;
	    immed = (l >> MIPS16OP_SH_IMM11) & MIPS16OP_MASK_IMM11;
	    signedp = 1;
	    pcrel = 1;
	    branch = 1;
	    info->insn_type = dis_branch;
	    break;
	  case 'A':
	    nbits = 8;
	    shift = 2;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    pcrel = 1;
	    /* FIXME: This can be lw or la.  We assume it is lw.  */
	    info->insn_type = dis_dref;
	    info->data_size = 4;
	    break;
	  case 'B':
	    nbits = 5;
	    shift = 3;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    pcrel = 1;
	    info->insn_type = dis_dref;
	    info->data_size = 8;
	    break;
	  case 'E':
	    nbits = 5;
	    shift = 2;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    pcrel = 1;
	    break;
	  default:
	    abort ();
	  }

	if (! use_extend)
	  {
	    if (signedp && immed >= (1 << (nbits - 1)))
	      immed -= 1 << nbits;
	    immed <<= shift;
	    if ((type == '<' || type == '>' || type == '[' || type == '[')
		&& immed == 0)
	      immed = 8;
	  }
	else
	  {
	    if (extbits == 16)
	      immed |= ((extend & 0x1f) << 11) | (extend & 0x7e0);
	    else if (extbits == 15)
	      immed |= ((extend & 0xf) << 11) | (extend & 0x7f0);
	    else
	      immed = ((extend >> 6) & 0x1f) | (extend & 0x20);
	    immed &= (1 << extbits) - 1;
	    if (! extu && immed >= (1 << (extbits - 1)))
	      immed -= 1 << extbits;
	  }

	if (! pcrel)
	  (*info->fprintf_func) (info->stream, "%d", immed);
	else
	  {
	    bfd_vma baseaddr;
	    bfd_vma val;

	    if (branch)
	      {
		immed *= 2;
		baseaddr = memaddr + 2;
	      }
	    else if (use_extend)
	      baseaddr = memaddr - 2;
	    else
	      {
		int status;
		bfd_byte buffer[2];

		baseaddr = memaddr;

		/* If this instruction is in the delay slot of a jr
                   instruction, the base address is the address of the
                   jr instruction.  If it is in the delay slot of jalr
                   instruction, the base address is the address of the
                   jalr instruction.  This test is unreliable: we have
                   no way of knowing whether the previous word is
                   instruction or data.  */
		status = (*info->read_memory_func) (memaddr - 4, buffer, 2,
						    info);
		if (status == 0
		    && (((info->endian == BFD_ENDIAN_BIG
			  ? bfd_getb16 (buffer)
			  : bfd_getl16 (buffer))
			 & 0xf800) == 0x1800))
		  baseaddr = memaddr - 4;
		else
		  {
		    status = (*info->read_memory_func) (memaddr - 2, buffer,
							2, info);
		    if (status == 0
			&& (((info->endian == BFD_ENDIAN_BIG
			      ? bfd_getb16 (buffer)
			      : bfd_getl16 (buffer))
			     & 0xf81f) == 0xe800))
		      baseaddr = memaddr - 2;
		  }
	      }
	    val = (baseaddr & ~ ((1 << shift) - 1)) + immed;
	    (*info->print_address_func) (val, info);
	    info->target = val;
	  }
      }
      break;

    case 'a':
      if (! use_extend)
	extend = 0;
      l = ((l & 0x1f) << 23) | ((l & 0x3e0) << 13) | (extend << 2);
      (*info->print_address_func) ((memaddr & 0xf0000000) | l, info);
      info->insn_type = dis_jsr;
      info->target = (memaddr & 0xf0000000) | l;
      info->branch_delay_insns = 1;
      break;

    case 'l':
    case 'L':
      {
	int need_comma, amask, smask;

	need_comma = 0;

	l = (l >> MIPS16OP_SH_IMM6) & MIPS16OP_MASK_IMM6;

	amask = (l >> 3) & 7;

	if (amask > 0 && amask < 5)
	  {
	    (*info->fprintf_func) (info->stream, "$%s", reg_names[4]);
	    if (amask > 1)
	      (*info->fprintf_func) (info->stream, "-$%s",
				     reg_names[amask + 3]);
	    need_comma = 1;
	  }

	smask = (l >> 1) & 3;
	if (smask == 3)
	  {
	    (*info->fprintf_func) (info->stream, "%s??",
				   need_comma ? "," : "");
	    need_comma = 1;
	  }
	else if (smask > 0)
	  {
	    (*info->fprintf_func) (info->stream, "%s$%s",
				   need_comma ? "," : "",
				   reg_names[16]);
	    if (smask > 1)
	      (*info->fprintf_func) (info->stream, "-$%s",
				     reg_names[smask + 15]);
	    need_comma = 1;
	  }

	if (l & 1)
	  {
	    (*info->fprintf_func) (info->stream, "%s$%s",
				   need_comma ? "," : "",
				   reg_names[31]);
	    need_comma = 1;
	  }

	if (amask == 5 || amask == 6)
	  {
	    (*info->fprintf_func) (info->stream, "%s$f0",
				   need_comma ? "," : "");
	    if (amask == 6)
	      (*info->fprintf_func) (info->stream, "-$f1");
	  }
      }
      break;

    default:
      abort ();
    }
}
