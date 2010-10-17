/* Disassemble SH64 instructions.
   Copyright 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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

#include "dis-asm.h"
#include "sysdep.h"
#include "sh64-opc.h"
#include "libiberty.h"

/* We need to refer to the ELF header structure.  */
#include "elf-bfd.h"
#include "elf/sh.h"
#include "elf32-sh64.h"

#define ELF_MODE32_CODE_LABEL_P(SYM) \
 (((elf_symbol_type *) (SYM))->internal_elf_sym.st_other & STO_SH5_ISA32)

#define SAVED_MOVI_R(INFO) \
 (((struct sh64_disassemble_info *) ((INFO)->private_data))->address_reg)

#define SAVED_MOVI_IMM(INFO) \
 (((struct sh64_disassemble_info *) ((INFO)->private_data))->built_address)

struct sh64_disassemble_info
 {
   /* When we see a MOVI, we save the register and the value, and merge a
      subsequent SHORI and display the address, if there is one.  */
   unsigned int address_reg;
   bfd_signed_vma built_address;

   /* This is the range decriptor for the current address.  It is kept
      around for the next call.  */
   sh64_elf_crange crange;
 };

/* Each item in the table is a mask to indicate which bits to be set
   to determine an instruction's operator.
   The index is as same as the instruction in the opcode table.
   Note that some archs have this as a field in the opcode table.  */
static unsigned long *shmedia_opcode_mask_table;

static void initialize_shmedia_opcode_mask_table PARAMS ((void));
static int print_insn_shmedia PARAMS ((bfd_vma, disassemble_info *));
static const char *creg_name PARAMS ((int));
static bfd_boolean init_sh64_disasm_info PARAMS ((struct disassemble_info *));
static enum sh64_elf_cr_type sh64_get_contents_type_disasm
  PARAMS ((bfd_vma, struct disassemble_info *));

/* Initialize the SH64 opcode mask table for each instruction in SHmedia
   mode.  */

static void
initialize_shmedia_opcode_mask_table ()
{
  int n_opc;
  int n;

  /* Calculate number of opcodes.  */
  for (n_opc = 0; shmedia_table[n_opc].name != NULL; n_opc++)
    ;

  shmedia_opcode_mask_table
    = xmalloc (sizeof (shmedia_opcode_mask_table[0]) * n_opc);

  for (n = 0; n < n_opc; n++)
    {
      int i;

      unsigned long mask = 0;

      for (i = 0; shmedia_table[n].arg[i] != A_NONE; i++)
	{
	  int offset = shmedia_table[n].nibbles[i];
	  int length;

	  switch (shmedia_table[n].arg[i])
	    {
	    case A_GREG_M:
	    case A_GREG_N:
	    case A_GREG_D:
	    case A_CREG_K:
	    case A_CREG_J:
	    case A_FREG_G:
	    case A_FREG_H:
	    case A_FREG_F:
	    case A_DREG_G:
	    case A_DREG_H:
	    case A_DREG_F:
	    case A_FMREG_G:
	    case A_FMREG_H:
	    case A_FMREG_F:
	    case A_FPREG_G:
	    case A_FPREG_H:
	    case A_FPREG_F:
	    case A_FVREG_G:
	    case A_FVREG_H:
	    case A_FVREG_F:
	    case A_REUSE_PREV:
	      length = 6;
	      break;

	    case A_TREG_A:
	    case A_TREG_B:
	      length = 3;
	      break;

	    case A_IMMM:
	      abort ();
	      break;

	    case A_IMMU5:
	      length = 5;
	      break;

	    case A_IMMS6:
	    case A_IMMU6:
	    case A_IMMS6BY32:
	      length = 6;
	      break;

	    case A_IMMS10:
	    case A_IMMS10BY1:
	    case A_IMMS10BY2:
	    case A_IMMS10BY4:
	    case A_IMMS10BY8:
	      length = 10;
	      break;

	    case A_IMMU16:
	    case A_IMMS16:
	    case A_PCIMMS16BY4:
	    case A_PCIMMS16BY4_PT:
	      length = 16;
	      break;

	    default:
	      abort ();
	      length = 0;
	      break;
	    }

	  if (length != 0)
	    mask |= (0xffffffff >> (32 - length)) << offset;
	}
      shmedia_opcode_mask_table[n] = 0xffffffff & ~mask;
    }
}

/* Get a predefined control-register-name, or return NULL.  */

const char *
creg_name (cregno)
     int cregno;
{
  const shmedia_creg_info *cregp;

  /* If control register usage is common enough, change this to search a
     hash-table.  */
  for (cregp = shmedia_creg_table; cregp->name != NULL; cregp++)
    {
      if (cregp->cregno == cregno)
	return cregp->name;
    }

  return NULL;
}

/* Main function to disassemble SHmedia instructions.  */

static int
print_insn_shmedia (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  fprintf_ftype fprintf_fn = info->fprintf_func;
  void *stream = info->stream;

  unsigned char insn[4];
  unsigned long instruction;
  int status;
  int n;
  const shmedia_opcode_info *op;
  int i;
  unsigned int r = 0;
  long imm = 0;
  bfd_vma disp_pc_addr;

  status = info->read_memory_func (memaddr, insn, 4, info);

  /* If we can't read four bytes, something is wrong.  Display any data we
     can get as .byte:s.  */
  if (status != 0)
    {
      int i;

      for (i = 0; i < 3; i++)
	{
	  status = info->read_memory_func (memaddr + i, insn, 1, info);
	  if (status != 0)
	    break;
	  (*fprintf_fn) (stream, "%s0x%02x",
			 i == 0 ? ".byte " : ", ",
			 insn[0]);
	}

      return i ? i : -1;
    }

  /* Rearrange the bytes to make up an instruction.  */
  if (info->endian == BFD_ENDIAN_LITTLE)
    instruction = bfd_getl32 (insn);
  else
    instruction = bfd_getb32 (insn);

  /* FIXME: Searching could be implemented using a hash on relevant
     fields.  */
  for (n = 0, op = shmedia_table;
       op->name != NULL
       && ((instruction & shmedia_opcode_mask_table[n]) != op->opcode_base);
       n++, op++)
    ;

  /* FIXME: We should also check register number constraints.  */
  if (op->name == NULL)
    {
      fprintf_fn (stream, ".long 0x%08x", instruction);
      return 4;
    }

  fprintf_fn (stream, "%s\t", op->name);

  for (i = 0; i < 3 && op->arg[i] != A_NONE; i++)
    {
      unsigned long temp = instruction >> op->nibbles[i];
      int by_number = 0;

      if (i > 0 && op->arg[i] != A_REUSE_PREV)
	fprintf_fn (stream, ",");

      switch (op->arg[i])
	{
	case A_REUSE_PREV:
	  continue;

	case A_GREG_M:
	case A_GREG_N:
	case A_GREG_D:
	  r = temp & 0x3f;
	  fprintf_fn (stream, "r%d", r);
	  break;

	case A_FVREG_F:
	case A_FVREG_G:
	case A_FVREG_H:
	  r = temp & 0x3f;
	  fprintf_fn (stream, "fv%d", r);
	  break;

	case A_FPREG_F:
	case A_FPREG_G:
	case A_FPREG_H:
	  r = temp & 0x3f;
	  fprintf_fn (stream, "fp%d", r);
	  break;

	case A_FMREG_F:
	case A_FMREG_G:
	case A_FMREG_H:
	  r = temp & 0x3f;
	  fprintf_fn (stream, "mtrx%d", r);
	  break;

	case A_CREG_K:
	case A_CREG_J:
	  {
	    const char *name;
	    r = temp & 0x3f;

	    name = creg_name (r);

	    if (name != NULL)
	      fprintf_fn (stream, "%s", name);
	    else
	      fprintf_fn (stream, "cr%d", r);
	  }
	  break;

	case A_FREG_G:
	case A_FREG_H:
	case A_FREG_F:
	  r = temp & 0x3f;
	  fprintf_fn (stream, "fr%d", r);
	  break;

	case A_DREG_G:
	case A_DREG_H:
	case A_DREG_F:
	  r = temp & 0x3f;
	  fprintf_fn (stream, "dr%d", r);
	  break;

	case A_TREG_A:
	case A_TREG_B:
	  r = temp & 0x7;
	  fprintf_fn (stream, "tr%d", r);
	  break;

	  /* A signed 6-bit number.  */
	case A_IMMS6:
	  imm = temp & 0x3f;
	  if (imm & (unsigned long) 0x20)
	    imm |= ~(unsigned long) 0x3f;
	  fprintf_fn (stream, "%d", imm);
	  break;

	  /* A signed 6-bit number, multiplied by 32 when used.  */
	case A_IMMS6BY32:
	  imm = temp & 0x3f;
	  if (imm & (unsigned long) 0x20)
	    imm |= ~(unsigned long) 0x3f;
	  fprintf_fn (stream, "%d", imm * 32);
	  break;

	  /* A signed 10-bit number, multiplied by 8 when used.  */
	case A_IMMS10BY8:
	  by_number++;
	  /* Fall through.  */

	  /* A signed 10-bit number, multiplied by 4 when used.  */
	case A_IMMS10BY4:
	  by_number++;
	  /* Fall through.  */

	  /* A signed 10-bit number, multiplied by 2 when used.  */
	case A_IMMS10BY2:
	  by_number++;
	  /* Fall through.  */

	  /* A signed 10-bit number.  */
	case A_IMMS10:
	case A_IMMS10BY1:
	  imm = temp & 0x3ff;
	  if (imm & (unsigned long) 0x200)
	    imm |= ~(unsigned long) 0x3ff;
	  imm <<= by_number;
	  fprintf_fn (stream, "%d", imm);
	  break;

	  /* A signed 16-bit number.  */
	case A_IMMS16:
	  imm = temp & 0xffff;
	  if (imm & (unsigned long) 0x8000)
	    imm |= ~((unsigned long) 0xffff);
	  fprintf_fn (stream, "%d", imm);
	  break;

	  /* A PC-relative signed 16-bit number, multiplied by 4 when
	     used.  */
	case A_PCIMMS16BY4:
	  imm = temp & 0xffff;	/* 16 bits */
	  if (imm & (unsigned long) 0x8000)
	    imm |= ~(unsigned long) 0xffff;
	  imm <<= 2;
	  disp_pc_addr = (bfd_vma) imm + memaddr;
	  (*info->print_address_func) (disp_pc_addr, info);
	  break;

	  /* An unsigned 5-bit number.  */
	case A_IMMU5:
	  imm = temp & 0x1f;
	  fprintf_fn (stream, "%d", imm);
	  break;

	  /* An unsigned 6-bit number.  */
	case A_IMMU6:
	  imm = temp & 0x3f;
	  fprintf_fn (stream, "%d", imm);
	  break;

	  /* An unsigned 16-bit number.  */
	case A_IMMU16:
	  imm = temp & 0xffff;
	  fprintf_fn (stream, "%d", imm);
	  break;

	default:
	  abort ();
	  break;
	}
    }

  /* FIXME: Looks like 32-bit values only are handled.
     FIXME: PC-relative numbers aren't handled correctly.  */
  if (op->opcode_base == (unsigned long) SHMEDIA_SHORI_OPC
      && SAVED_MOVI_R (info) == r)
    {
      asection *section = info->section;

      /* Most callers do not set the section field correctly yet.  Revert
	 to getting the section from symbols, if any. */
      if (section == NULL
	  && info->symbols != NULL
	  && bfd_asymbol_flavour (info->symbols[0]) == bfd_target_elf_flavour
	  && ! bfd_is_und_section (bfd_get_section (info->symbols[0]))
	  && ! bfd_is_abs_section (bfd_get_section (info->symbols[0])))
	section = bfd_get_section (info->symbols[0]);

      /* Only guess addresses when the contents of this section is fully
	 relocated.  Otherwise, the value will be zero or perhaps even
	 bogus.  */
      if (section == NULL
	  || section->owner == NULL
	  || elf_elfheader (section->owner)->e_type == ET_EXEC)
	{
	  bfd_signed_vma shori_addr;

	  shori_addr = SAVED_MOVI_IMM (info) << 16;
	  shori_addr |= imm;

	  fprintf_fn (stream, "\t! 0x");
	  (*info->print_address_func) (shori_addr, info);
	}
    }

  if (op->opcode_base == SHMEDIA_MOVI_OPC)
    {
      SAVED_MOVI_IMM (info) = imm;
      SAVED_MOVI_R (info) = r;
    }
  else
    {
      SAVED_MOVI_IMM (info) = 0;
      SAVED_MOVI_R (info) = 255;
    }

  return 4;
}

/* Check the type of contents about to be disassembled.  This is like
   sh64_get_contents_type (which may be called from here), except that it
   takes the same arguments as print_insn_* and does what can be done if
   no section is available.  */

static enum sh64_elf_cr_type
sh64_get_contents_type_disasm (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  struct sh64_disassemble_info *sh64_infop = info->private_data;

  /* Perhaps we have a region from a previous probe and it still counts
     for this address?  */
  if (sh64_infop->crange.cr_type != CRT_NONE
      && memaddr >= sh64_infop->crange.cr_addr
      && memaddr < sh64_infop->crange.cr_addr + sh64_infop->crange.cr_size)
    return sh64_infop->crange.cr_type;

  /* If we have a section, try and use it.  */
  if (info->section
      && bfd_get_flavour (info->section->owner) == bfd_target_elf_flavour)
    {
      enum sh64_elf_cr_type cr_type
	= sh64_get_contents_type (info->section, memaddr,
				  &sh64_infop->crange);

      if (cr_type != CRT_NONE)
	return cr_type;
    }

  /* If we have symbols, we can try and get at a section from *that*.  */
  if (info->symbols != NULL
      && bfd_asymbol_flavour (info->symbols[0]) == bfd_target_elf_flavour
      && ! bfd_is_und_section (bfd_get_section (info->symbols[0]))
      && ! bfd_is_abs_section (bfd_get_section (info->symbols[0])))
    {
      enum sh64_elf_cr_type cr_type
	= sh64_get_contents_type (bfd_get_section (info->symbols[0]),
				  memaddr, &sh64_infop->crange);

      if (cr_type != CRT_NONE)
	return cr_type;
    }

  /* We can make a reasonable guess based on the st_other field of a
     symbol; for a BranchTarget this is marked as STO_SH5_ISA32 and then
     it's most probably code there.  */
  if (info->symbols
      && bfd_asymbol_flavour (info->symbols[0]) == bfd_target_elf_flavour
      && elf_symbol_from (bfd_asymbol_bfd (info->symbols[0]),
			  info->symbols[0])->internal_elf_sym.st_other
      == STO_SH5_ISA32)
    return CRT_SH5_ISA32;

  /* If all else fails, guess this is code and guess on the low bit set.  */
  return (memaddr & 1) == 1 ? CRT_SH5_ISA32 : CRT_SH5_ISA16;
}

/* Initialize static and dynamic disassembly state.  */

static bfd_boolean
init_sh64_disasm_info (info)
     struct disassemble_info *info;
{
  struct sh64_disassemble_info *sh64_infop
    = calloc (sizeof (*sh64_infop), 1);

  if (sh64_infop == NULL)
    return FALSE;

  info->private_data = sh64_infop;

  SAVED_MOVI_IMM (info) = 0;
  SAVED_MOVI_R (info) = 255;

  if (shmedia_opcode_mask_table == NULL)
    initialize_shmedia_opcode_mask_table ();

  return TRUE;
}

/* Main entry to disassemble SHmedia instructions, given an endian set in
   INFO.  Note that the simulator uses this as the main entry and does not
   use any of the functions further below.  */

int
print_insn_sh64x_media (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  if (info->private_data == NULL && ! init_sh64_disasm_info (info))
    return -1;

  /* Make reasonable output.  */
  info->bytes_per_line = 4;
  info->bytes_per_chunk = 4;

  return print_insn_shmedia (memaddr, info);
}

/* Main entry to disassemble SHmedia insns.
   If we see an SHcompact instruction, return -2.  */

int
print_insn_sh64 (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  enum bfd_endian endian = info->endian;
  enum sh64_elf_cr_type cr_type;

  if (info->private_data == NULL && ! init_sh64_disasm_info (info))
    return -1;

  cr_type = sh64_get_contents_type_disasm (memaddr, info);
  if (cr_type != CRT_SH5_ISA16)
    {
      int length = 4 - (memaddr % 4);
      info->display_endian = endian;

      /* If we got an uneven address to indicate SHmedia, adjust it.  */
      if (cr_type == CRT_SH5_ISA32 && length == 3)
	memaddr--, length = 4;

      /* Only disassemble on four-byte boundaries.  Addresses that are not
	 a multiple of four can happen after a data region.  */
      if (cr_type == CRT_SH5_ISA32 && length == 4)
	return print_insn_sh64x_media (memaddr, info);

      /* We get CRT_DATA *only* for data regions in a mixed-contents
	 section.  For sections with data only, we get indication of one
	 of the ISA:s.  You may think that we shouldn't disassemble
	 section with only data if we can figure that out.  However, the
	 disassembly function is by default not called for data-only
	 sections, so if the user explicitly specified disassembly of a
	 data section, that's what we should do.  */
      if (cr_type == CRT_DATA || length != 4)
	{
	  int status;
	  unsigned char data[4];
	  struct sh64_disassemble_info *sh64_infop = info->private_data;

	  if (length == 4
	      && sh64_infop->crange.cr_type != CRT_NONE
	      && memaddr >= sh64_infop->crange.cr_addr
	      && memaddr < (sh64_infop->crange.cr_addr
			    + sh64_infop->crange.cr_size))
	    length
	      = (sh64_infop->crange.cr_addr
		 + sh64_infop->crange.cr_size - memaddr);

	  status
	    = (*info->read_memory_func) (memaddr, data,
					 length >= 4 ? 4 : length, info);

	  if (status == 0 && length >= 4)
	    {
	      (*info->fprintf_func) (info->stream, ".long 0x%08lx",
				     endian == BFD_ENDIAN_BIG
				     ? (long) (bfd_getb32 (data))
				     : (long) (bfd_getl32 (data)));
	      return 4;
	    }
	  else
	    {
	      int i;

	      for (i = 0; i < length; i++)
		{
		  status = info->read_memory_func (memaddr + i, data, 1, info);
		  if (status != 0)
		    break;
		  (*info->fprintf_func) (info->stream, "%s0x%02x",
					 i == 0 ? ".byte " : ", ",
					 data[0]);
		}

	      return i ? i : -1;
	    }
	}
    }

  /* SH1 .. SH4 instruction, let caller handle it.  */
  return -2;
}
