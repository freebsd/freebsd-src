/* CR16C ELF support for BFD.
   Copyright 2004 Free Software Foundation, Inc.

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
   Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _ELF_CR16C_H
#define _ELF_CR16C_H

#include "bfd.h"
#include "elf/reloc-macros.h"

/* Creating indices for reloc_map_index array.  */
START_RELOC_NUMBERS (elf_cr16c_reloc_type)
     RELOC_NUMBER (RINDEX_16C_NUM08,		0)
     RELOC_NUMBER (RINDEX_16C_NUM08_C,		1)
     RELOC_NUMBER (RINDEX_16C_NUM16,		2)
     RELOC_NUMBER (RINDEX_16C_NUM16_C,		3)
     RELOC_NUMBER (RINDEX_16C_NUM32,		4)
     RELOC_NUMBER (RINDEX_16C_NUM32_C,		5)
     RELOC_NUMBER (RINDEX_16C_DISP04,		6)
     RELOC_NUMBER (RINDEX_16C_DISP04_C,		7)
     RELOC_NUMBER (RINDEX_16C_DISP08,		8)
     RELOC_NUMBER (RINDEX_16C_DISP08_C,		9)
     RELOC_NUMBER (RINDEX_16C_DISP16,		10)
     RELOC_NUMBER (RINDEX_16C_DISP16_C,		11)
     RELOC_NUMBER (RINDEX_16C_DISP24,		12)
     RELOC_NUMBER (RINDEX_16C_DISP24_C,		13)
     RELOC_NUMBER (RINDEX_16C_DISP24a,		14)
     RELOC_NUMBER (RINDEX_16C_DISP24a_C,	15)
     RELOC_NUMBER (RINDEX_16C_REG04,		16)
     RELOC_NUMBER (RINDEX_16C_REG04_C,		17)
     RELOC_NUMBER (RINDEX_16C_REG04a,		18)
     RELOC_NUMBER (RINDEX_16C_REG04a_C,		19)
     RELOC_NUMBER (RINDEX_16C_REG14,		20)
     RELOC_NUMBER (RINDEX_16C_REG14_C,		21)
     RELOC_NUMBER (RINDEX_16C_REG16,		22)
     RELOC_NUMBER (RINDEX_16C_REG16_C,		23)
     RELOC_NUMBER (RINDEX_16C_REG20,		24)
     RELOC_NUMBER (RINDEX_16C_REG20_C,		25)
     RELOC_NUMBER (RINDEX_16C_ABS20,		26)
     RELOC_NUMBER (RINDEX_16C_ABS20_C,		27)
     RELOC_NUMBER (RINDEX_16C_ABS24,		28)
     RELOC_NUMBER (RINDEX_16C_ABS24_C,		29)
     RELOC_NUMBER (RINDEX_16C_IMM04,		30)
     RELOC_NUMBER (RINDEX_16C_IMM04_C,		31)
     RELOC_NUMBER (RINDEX_16C_IMM16,		32)
     RELOC_NUMBER (RINDEX_16C_IMM16_C,		33)
     RELOC_NUMBER (RINDEX_16C_IMM20,		34)
     RELOC_NUMBER (RINDEX_16C_IMM20_C,		35)
     RELOC_NUMBER (RINDEX_16C_IMM24,		36)
     RELOC_NUMBER (RINDEX_16C_IMM24_C,		37)
     RELOC_NUMBER (RINDEX_16C_IMM32,		38)
     RELOC_NUMBER (RINDEX_16C_IMM32_C,		39)
END_RELOC_NUMBERS (RINDEX_16C_MAX)

/* CR16C Relocation Types ('cr_reloc_type' entry in the reloc_map structure).
   The relocation constant name is determined as follows :

   R_16C_<format><size>[_C]

   Where :

     <format> is one of the following:
	NUM  - R_NUMBER mnemonic,
	DISP - R_16C_DISPL mnemonic,
	REG  - R_16C_REGREL mnemonic,
	ABS  - R_16C_ABS mnemonic,
	IMM  - R_16C_IMMED mnemonic,
     <size> stands for R_S_16C_<size> 
     _C means 'code label' and is only added when R_ADDRTYPE subfield 
     is of type R_CODE_ADDR.  */
   
/* The table below shows what the hex digits in the definition of the
   relocation type constants correspond to.
   ------------------------------------------------------------------
	R_SIZESP	R_FORMAT	R_RELTO	      R_ADDRTYPE
   ------------------------------------------------------------------  */
/*	R_S_16C_08	R_NUMBER 	R_ABS 	      R_ADDRESS */
#define R_16C_NUM08	0X0001

/*	R_S_16C_08	R_NUMBER 	R_ABS 	      R_CODE_ADDR */
#define R_16C_NUM08_C	0X0006

/*	R_S_16C_16	R_NUMBER 	R_ABS 	      R_ADDRESS */
#define R_16C_NUM16	0X1001

/*	R_S_16C_16	R_NUMBER 	R_ABS 	      R_CODE_ADDR */
#define R_16C_NUM16_C 	0X1006

/*      R_S_16C_32      R_NUMBER	R_ABS	      R_ADDRESS */
#define R_16C_NUM32     0X2001

/*      R_S_16C_32      R_NUMBER	R_ABS	      R_CODE_ADDR */
#define R_16C_NUM32_C   0X2006

/*	R_S_16C_04	R_16C_DISPL 	R_PCREL	      R_ADDRESS */
#define R_16C_DISP04	0X5411

/*	R_S_16C_04	R_16C_DISPL 	R_PCREL	      R_CODE_ADDR */
#define R_16C_DISP04_C	0X5416

/*	R_S_16C_08	R_16C_DISPL 	R_PCREL	      R_ADDRESS */
#define R_16C_DISP08	0X0411

/*	R_S_16C_08	R_16C_DISPL 	R_PCREL	      R_CODE_ADDR */
#define R_16C_DISP08_C	0X0416

/*	R_S_16C_16	R_16C_DISPL 	R_PCREL	      R_ADDRESS */
#define R_16C_DISP16	0X1411

/*	R_S_16C_16	R_16C_DISPL 	R_PCREL	      R_CODE_ADDR */
#define R_16C_DISP16_C	0X1416

/*	R_S_16C_24	R_16C_DISPL 	R_PCREL	      R_ADDRESS */
#define R_16C_DISP24	0X7411

/*	R_S_16C_24	R_16C_DISPL 	R_PCREL	      R_CODE_ADDR */
#define R_16C_DISP24_C	0X7416

/*	R_S_16C_24a	R_16C_DISPL 	R_PCREL	      R_ADDRESS */
#define R_16C_DISP24a	0X6411

/*	R_S_16C_24a	R_16C_DISPL 	R_PCREL	      R_CODE_ADDR */
#define R_16C_DISP24a_C	0X6416

/*	R_S_16C_04	R_16C_REGREL 	R_ABS 	      R_ADDRESS */
#define R_16C_REG04	0X5201

/*	R_S_16C_04	R_16C_REGREL 	R_ABS 	      R_CODE_ADDR */
#define R_16C_REG04_C	0X5206

/*	R_S_16C_04_a	R_16C_REGREL 	R_ABS 	      R_ADDRESS */
#define R_16C_REG04a	0X4201

/*	R_S_16C_04_a	R_16C_REGREL 	R_ABS 	      R_CODE_ADDR */
#define R_16C_REG04a_C	0X4206

/*	R_S_16C_14	R_16C_REGREL 	R_ABS 	      R_ADDRESS */
#define R_16C_REG14	0X3201

/*	R_S_16C_14	R_16C_REGREL 	R_ABS 	      R_CODE_ADDR */
#define R_16C_REG14_C	0X3206

/*	R_S_16C_16	R_16C_REGREL 	R_ABS 	      R_ADDRESS */
#define R_16C_REG16	0X1201

/*	R_S_16C_16	R_16C_REGREL 	R_ABS 	      R_CODE_ADDR */
#define R_16C_REG16_C	0X1206

/*	R_S_16C_20	R_16C_REGREL 	R_ABS 	      R_ADDRESS */
#define R_16C_REG20	0X8201

/*	R_S_16C_20	R_16C_REGREL 	R_ABS 	      R_CODE_ADDR */
#define R_16C_REG20_C	0X8206

/*      R_S_16C_20      R_16C_ABS	R_ABS	      R_ADDRESS */
#define R_16C_ABS20     0X8101

/*      R_S_16C_20      R_16C_ABS	R_ABS	      R_CODE_ADDR */
#define R_16C_ABS20_C   0X8106

/*      R_S_16C_24      R_16C_ABS	R_ABS	      R_ADDRESS */
#define R_16C_ABS24     0X7101

/*      R_S_16C_24      R_16C_ABS	R_ABS	      R_CODE_ADDR */
#define R_16C_ABS24_C   0X7106

/*      R_S_16C_04      R_16C_IMMED	R_ABS	      R_ADDRESS */
#define R_16C_IMM04     0X5301

/*      R_S_16C_04      R_16C_IMMED	R_ABS	      R_CODE_ADDR */
#define R_16C_IMM04_C   0X5306

/*      R_S_16C_16      R_16C_IMMED	R_ABS	      R_ADDRESS */
#define R_16C_IMM16     0X1301

/*      R_S_16C_16      R_16C_IMMED	R_ABS	      R_CODE_ADDR */
#define R_16C_IMM16_C   0X1306

/*      R_S_16C_20      R_16C_IMMED	R_ABS	      R_ADDRESS */
#define R_16C_IMM20     0X8301

/*      R_S_16C_20      R_16C_IMMED	R_ABS	      R_CODE_ADDR */
#define R_16C_IMM20_C   0X8306

/*      R_S_16C_24      R_16C_IMMED	R_ABS	      R_ADDRESS */
#define R_16C_IMM24     0X7301

/*      R_S_16C_24      R_16C_IMMED	R_ABS	      R_CODE_ADDR */
#define R_16C_IMM24_C   0X7306

/*      R_S_16C_32      R_16C_IMMED	R_ABS	      R_ADDRESS */
#define R_16C_IMM32     0X2301

/*      R_S_16C_32      R_16C_IMMED	R_ABS	      R_CODE_ADDR */
#define R_16C_IMM32_C   0X2306


/* Relocation item type.  */
#define   R_ADDRTYPE	 0x000f
#define   R_ADDRESS      0x0001    /* Take address of symbol.  */
#define   R_CODE_ADDR    0x0006    /* Take address of symbol divided by 2.  */

/* Relocation action.  */
#define   R_RELTO        0x00f0
#define   R_ABS          0x0000    /* Keep symbol's address as such.  */
#define   R_PCREL        0x0010    /* Subtract the pc address of hole.  */

/* Relocation item data format.  */
#define   R_FORMAT       0x0f00
#define   R_NUMBER       0x0000    /* Retain as two's complement value.  */
#define   R_16C_DISPL    0x0400    /* CR16C displacement type.  */
#define   R_16C_ABS      0x0100    /* CR16C absolute type.  */
#define   R_16C_REGREL   0x0200    /* CR16C register-relative type.  */
#define   R_16C_IMMED    0x0300    /* CR16C immediate type.  */

/* Relocation item size. */
#define   R_SIZESP       0xf000
#define   R_S_16C_04     0x5000
#define   R_S_16C_04_a   0x4000
#define   R_S_16C_08	 0x0000
#define   R_S_16C_14     0x3000
#define   R_S_16C_16	 0x1000
#define   R_S_16C_20     0x8000
#define   R_S_16C_24_a   0x6000
#define   R_S_16C_24	 0x7000
#define   R_S_16C_32     0x2000


/* Processor specific section indices.  These sections do not actually
   exist.  Symbols with a st_shndx field corresponding to one of these
   values have a special meaning.  */

/* Far common symbol.  */
#define SHN_CR16C_FCOMMON	0xff00
#define SHN_CR16C_NCOMMON	0xff01

typedef struct reloc_map
{
  unsigned short            cr_reloc_type;  /* CR relocation type.  */
  bfd_reloc_code_real_type  bfd_reloc_enum; /* BFD relocation enum.  */
} RELOC_MAP;

#endif /* _ELF_CR16C_H */
