/* ARM ELF support for BFD.
   Copyright 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _ELF_ARM_H
#define _ELF_ARM_H

#include "elf/reloc-macros.h"

/* Processor specific flags for the ELF header e_flags field.  */
#define EF_ARM_RELEXEC     0x01
#define EF_ARM_HASENTRY    0x02
#define EF_ARM_INTERWORK   0x04
#define EF_ARM_APCS_26     0x08
#define EF_ARM_APCS_FLOAT  0x10
#define EF_ARM_PIC         0x20
#define EF_ARM_ALIGN8	   0x40		/* 8-bit structure alignment is in use.  */
#define EF_ARM_NEW_ABI     0x80
#define EF_ARM_OLD_ABI     0x100
#define EF_ARM_SOFT_FLOAT  0x200
#define EF_ARM_VFP_FLOAT   0x400
#define EF_ARM_MAVERICK_FLOAT 0x800

/* Other constants defined in the ARM ELF spec. version B-01.  */
#define EF_ARM_SYMSARESORTED 0x04	/* NB conflicts with EF_INTERWORK */
#define EF_ARM_DYNSYMSUSESEGIDX 0x08	/* NB conflicts with EF_APCS26 */
#define EF_ARM_MAPSYMSFIRST 0x10	/* NB conflicts with EF_APCS_FLOAT */
#define EF_ARM_EABIMASK      0xFF000000

#define EF_ARM_EABI_VERSION(flags) ((flags) & EF_ARM_EABIMASK)
#define EF_ARM_EABI_UNKNOWN  0x00000000
#define EF_ARM_EABI_VER1     0x01000000
#define EF_ARM_EABI_VER2     0x02000000

/* Local aliases for some flags to match names used by COFF port.  */
#define F_INTERWORK	   EF_ARM_INTERWORK
#define F_APCS26	   EF_ARM_APCS_26
#define F_APCS_FLOAT	   EF_ARM_APCS_FLOAT
#define F_PIC              EF_ARM_PIC
#define F_SOFT_FLOAT	   EF_ARM_SOFT_FLOAT
#define F_VFP_FLOAT	   EF_ARM_VFP_FLOAT

/* Additional symbol types for Thumb.  */
#define STT_ARM_TFUNC      STT_LOPROC   /* A Thumb function.  */
#define STT_ARM_16BIT      STT_HIPROC   /* A Thumb label.  */

/* ARM-specific values for sh_flags.  */
#define SHF_ENTRYSECT      0x10000000   /* Section contains an entry point.  */
#define SHF_COMDEF         0x80000000   /* Section may be multiply defined in the input to a link step.  */

/* ARM-specific program header flags.  */
#define PF_ARM_SB          0x10000000   /* Segment contains the location addressed by the static base.  */
#define PF_ARM_PI          0x20000000   /* Segment is position-independent.  */
#define PF_ARM_ABS         0x40000000   /* Segment must be loaded at its base address.  */

/* Relocation types.  */

START_RELOC_NUMBERS (elf_arm_reloc_type)
  RELOC_NUMBER (R_ARM_NONE,             0)
  RELOC_NUMBER (R_ARM_PC24,             1)
  RELOC_NUMBER (R_ARM_ABS32,            2)
  RELOC_NUMBER (R_ARM_REL32,            3)
#ifdef OLD_ARM_ABI
  RELOC_NUMBER (R_ARM_ABS8,             4)
  RELOC_NUMBER (R_ARM_ABS16,            5)
  RELOC_NUMBER (R_ARM_ABS12,            6)
  RELOC_NUMBER (R_ARM_THM_ABS5,         7)
  RELOC_NUMBER (R_ARM_THM_PC22,         8)
  RELOC_NUMBER (R_ARM_SBREL32,          9)
  RELOC_NUMBER (R_ARM_AMP_VCALL9,      10)
  RELOC_NUMBER (R_ARM_THM_PC11,        11)   /* Cygnus extension to abi: Thumb unconditional branch.  */
  RELOC_NUMBER (R_ARM_THM_PC9,         12)   /* Cygnus extension to abi: Thumb conditional branch.  */
  RELOC_NUMBER (R_ARM_GNU_VTINHERIT,   13)  
  RELOC_NUMBER (R_ARM_GNU_VTENTRY,     14)  
#else /* not OLD_ARM_ABI */
  RELOC_NUMBER (R_ARM_PC13,             4)
  RELOC_NUMBER (R_ARM_ABS16,            5)
  RELOC_NUMBER (R_ARM_ABS12,            6)
  RELOC_NUMBER (R_ARM_THM_ABS5,         7)
  RELOC_NUMBER (R_ARM_ABS8,             8)
  RELOC_NUMBER (R_ARM_SBREL32,          9)
  RELOC_NUMBER (R_ARM_THM_PC22,        10)
  RELOC_NUMBER (R_ARM_THM_PC8,         11)
  RELOC_NUMBER (R_ARM_AMP_VCALL9,      12)
  RELOC_NUMBER (R_ARM_SWI24,           13)
  RELOC_NUMBER (R_ARM_THM_SWI8,        14)
  RELOC_NUMBER (R_ARM_XPC25,           15)
  RELOC_NUMBER (R_ARM_THM_XPC22,       16)
#endif /* not OLD_ARM_ABI */
  RELOC_NUMBER (R_ARM_COPY,            20)   /* Copy symbol at runtime.  */
  RELOC_NUMBER (R_ARM_GLOB_DAT,        21)   /* Create GOT entry.  */
  RELOC_NUMBER (R_ARM_JUMP_SLOT,       22)   /* Create PLT entry.  */
  RELOC_NUMBER (R_ARM_RELATIVE,        23)   /* Adjust by program base.  */
  RELOC_NUMBER (R_ARM_GOTOFF,          24)   /* 32 bit offset to GOT.  */
  RELOC_NUMBER (R_ARM_GOTPC,           25)   /* 32 bit PC relative offset to GOT.  */
  RELOC_NUMBER (R_ARM_GOT32,           26)   /* 32 bit GOT entry.  */
  RELOC_NUMBER (R_ARM_PLT32,           27)   /* 32 bit PLT address.  */
#ifdef OLD_ARM_ABI
  FAKE_RELOC   (FIRST_INVALID_RELOC,   28)
  FAKE_RELOC   (LAST_INVALID_RELOC,   249)
#else /* not OLD_ARM_ABI */
  FAKE_RELOC   (FIRST_INVALID_RELOC1,  28)
  FAKE_RELOC   (LAST_INVALID_RELOC1,   31)
  RELOC_NUMBER (R_ARM_ALU_PCREL7_0,    32)
  RELOC_NUMBER (R_ARM_ALU_PCREL15_8,   33)
  RELOC_NUMBER (R_ARM_ALU_PCREL23_15,  34)
  RELOC_NUMBER (R_ARM_LDR_SBREL11_0,   35)
  RELOC_NUMBER (R_ARM_ALU_SBREL19_12,  36)
  RELOC_NUMBER (R_ARM_ALU_SBREL27_20,  37)
  FAKE_RELOC   (FIRST_INVALID_RELOC2,  38)
  FAKE_RELOC   (LAST_INVALID_RELOC2,   99)
  RELOC_NUMBER (R_ARM_GNU_VTENTRY,    100)
  RELOC_NUMBER (R_ARM_GNU_VTINHERIT,  101)
  RELOC_NUMBER (R_ARM_THM_PC11,       102)   /* Cygnus extension to abi: Thumb unconditional branch.  */
  RELOC_NUMBER (R_ARM_THM_PC9,        103)   /* Cygnus extension to abi: Thumb conditional branch.  */
  FAKE_RELOC   (FIRST_INVALID_RELOC3, 104)
  FAKE_RELOC   (LAST_INVALID_RELOC3,  248)
  RELOC_NUMBER (R_ARM_RXPC25,         249)
#endif /* not OLD_ARM_ABI */
  RELOC_NUMBER (R_ARM_RSBREL32,       250)
  RELOC_NUMBER (R_ARM_THM_RPC22,      251)
  RELOC_NUMBER (R_ARM_RREL32,         252)
  RELOC_NUMBER (R_ARM_RABS32,         253)
  RELOC_NUMBER (R_ARM_RPC24,          254)
  RELOC_NUMBER (R_ARM_RBASE,          255)
END_RELOC_NUMBERS (R_ARM_max)

/* The name of the note section used to identify arm variants.  */
#define ARM_NOTE_SECTION ".note.gnu.arm.ident"
     
#endif /* _ELF_ARM_H */
