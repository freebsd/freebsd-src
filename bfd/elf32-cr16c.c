/* BFD back-end for National Semiconductor's CR16C ELF
   Copyright 2004, 2005 Free Software Foundation, Inc.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "elf/cr16c.h"
#include "elf-bfd.h"


#define USE_REL	1	/* CR16C uses REL relocations instead of RELA.  */

/* The following definition is based on EMPTY_HOWTO macro, 
   but also initiates the "name" field in HOWTO struct.  */
#define ONLY_NAME_HOWTO(C) \
  HOWTO ((C), 0, 0, 0, FALSE, 0, complain_overflow_dont, NULL, \
	  STRINGX(C), FALSE, 0, 0, FALSE)

/* reloc_map_index array maps CRASM relocation type into a BFD
   relocation enum. The array's indices are synchronized with 
   RINDEX_16C_* indices, created in include/elf/cr16c.h.
   The array is used in:
   1. elf32-cr16c.c : elf_cr16c_reloc_type_lookup().
   2. asreloc.c : find_reloc_type(). */

RELOC_MAP reloc_map_index[RINDEX_16C_MAX] =
{
  {R_16C_NUM08,     BFD_RELOC_16C_NUM08},
  {R_16C_NUM08_C,   BFD_RELOC_16C_NUM08_C},
  {R_16C_NUM16,     BFD_RELOC_16C_NUM16},
  {R_16C_NUM16_C,   BFD_RELOC_16C_NUM16_C},
  {R_16C_NUM32,     BFD_RELOC_16C_NUM32},
  {R_16C_NUM32_C,   BFD_RELOC_16C_NUM32_C},
  {R_16C_DISP04,    BFD_RELOC_16C_DISP04},
  {R_16C_DISP04_C,  BFD_RELOC_16C_DISP04_C},
  {R_16C_DISP08,    BFD_RELOC_16C_DISP08},
  {R_16C_DISP08_C,  BFD_RELOC_16C_DISP08_C},
  {R_16C_DISP16,    BFD_RELOC_16C_DISP16},
  {R_16C_DISP16_C,  BFD_RELOC_16C_DISP16_C},
  {R_16C_DISP24,    BFD_RELOC_16C_DISP24},
  {R_16C_DISP24_C,  BFD_RELOC_16C_DISP24_C},
  {R_16C_DISP24a,   BFD_RELOC_16C_DISP24a},
  {R_16C_DISP24a_C, BFD_RELOC_16C_DISP24a_C},
  {R_16C_REG04,     BFD_RELOC_16C_REG04},
  {R_16C_REG04_C,   BFD_RELOC_16C_REG04_C},
  {R_16C_REG04a,    BFD_RELOC_16C_REG04a},
  {R_16C_REG04a_C,  BFD_RELOC_16C_REG04a_C},
  {R_16C_REG14,     BFD_RELOC_16C_REG14},
  {R_16C_REG14_C,   BFD_RELOC_16C_REG14_C},
  {R_16C_REG16,     BFD_RELOC_16C_REG16},
  {R_16C_REG16_C,   BFD_RELOC_16C_REG16_C},
  {R_16C_REG20,     BFD_RELOC_16C_REG20},
  {R_16C_REG20_C,   BFD_RELOC_16C_REG20_C},
  {R_16C_ABS20,     BFD_RELOC_16C_ABS20},
  {R_16C_ABS20_C,   BFD_RELOC_16C_ABS20_C},
  {R_16C_ABS24,     BFD_RELOC_16C_ABS24},
  {R_16C_ABS24_C,   BFD_RELOC_16C_ABS24_C},
  {R_16C_IMM04,     BFD_RELOC_16C_IMM04},
  {R_16C_IMM04_C,   BFD_RELOC_16C_IMM04_C},
  {R_16C_IMM16,     BFD_RELOC_16C_IMM16},
  {R_16C_IMM16_C,   BFD_RELOC_16C_IMM16_C},
  {R_16C_IMM20,     BFD_RELOC_16C_IMM20},
  {R_16C_IMM20_C,   BFD_RELOC_16C_IMM20_C},
  {R_16C_IMM24,     BFD_RELOC_16C_IMM24},
  {R_16C_IMM24_C,   BFD_RELOC_16C_IMM24_C},
  {R_16C_IMM32,     BFD_RELOC_16C_IMM32},
  {R_16C_IMM32_C,   BFD_RELOC_16C_IMM32_C}
};

static reloc_howto_type elf_howto_table[] =
{
  /* 00 */ ONLY_NAME_HOWTO (RINDEX_16C_NUM08),
  /* 01 */ ONLY_NAME_HOWTO (RINDEX_16C_NUM08_C),
  /* 02 */ ONLY_NAME_HOWTO (RINDEX_16C_NUM16),
  /* 03 */ ONLY_NAME_HOWTO (RINDEX_16C_NUM16_C),
  /* 04 */ ONLY_NAME_HOWTO (RINDEX_16C_NUM32),
  /* 05 */ ONLY_NAME_HOWTO (RINDEX_16C_NUM32_C),
  /* 06 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP04),
  /* 07 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP04_C),
  /* 08 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP08),
  /* 09 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP08_C),
  /* 10 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP16),
  /* 11 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP16_C),
  /* 12 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP24),
  /* 13 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP24_C),
  /* 14 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP24a),
  /* 15 */ ONLY_NAME_HOWTO (RINDEX_16C_DISP24a_C),
  /* 16 */ ONLY_NAME_HOWTO (RINDEX_16C_REG04),
  /* 17 */ ONLY_NAME_HOWTO (RINDEX_16C_REG04_C),
  /* 18 */ ONLY_NAME_HOWTO (RINDEX_16C_REG04a),
  /* 19 */ ONLY_NAME_HOWTO (RINDEX_16C_REG04a_C),
  /* 20 */ ONLY_NAME_HOWTO (RINDEX_16C_REG14),
  /* 21 */ ONLY_NAME_HOWTO (RINDEX_16C_REG14_C),
  /* 22 */ ONLY_NAME_HOWTO (RINDEX_16C_REG16),
  /* 23 */ ONLY_NAME_HOWTO (RINDEX_16C_REG16_C),
  /* 24 */ ONLY_NAME_HOWTO (RINDEX_16C_REG20),
  /* 25 */ ONLY_NAME_HOWTO (RINDEX_16C_REG20_C),
  /* 26 */ ONLY_NAME_HOWTO (RINDEX_16C_ABS20),
  /* 27 */ ONLY_NAME_HOWTO (RINDEX_16C_ABS20_C),
  /* 28 */ ONLY_NAME_HOWTO (RINDEX_16C_ABS24),
  /* 29 */ ONLY_NAME_HOWTO (RINDEX_16C_ABS24_C),
  /* 30 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM04),
  /* 31 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM04_C),
  /* 32 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM16),
  /* 33 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM16_C),
  /* 34 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM20),
  /* 35 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM20_C),
  /* 36 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM24),
  /* 37 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM24_C),
  /* 38 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM32),
  /* 39 */ ONLY_NAME_HOWTO (RINDEX_16C_IMM32_C)
};


/* Code to turn a code_type into a howto ptr, uses the above howto table.  */

static reloc_howto_type *
elf_cr16c_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			     bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0; i < RINDEX_16C_MAX; i++)
    {
      if (code == reloc_map_index[i].bfd_reloc_enum)
	{
	  /* printf ("CR16C Relocation Type is - %x\n", code); */
	  return & elf_howto_table[i];
	}
    }

  /* printf ("This relocation Type is not supported - %x\n", code); */
  return 0;
}

static void
elf_cr16c_info_to_howto (bfd *abfd ATTRIBUTE_UNUSED,
			 arelent *cache_ptr ATTRIBUTE_UNUSED,
			 Elf_Internal_Rela *dst ATTRIBUTE_UNUSED)
{
  abort ();
}

static void
elf_cr16c_info_to_howto_rel (bfd *abfd ATTRIBUTE_UNUSED,
			     arelent *cache_ptr,
			     Elf_Internal_Rela *dst)
{
  unsigned int r_type = ELF32_R_TYPE (dst->r_info);

  BFD_ASSERT (r_type < (unsigned int) RINDEX_16C_MAX);
  cache_ptr->howto = &elf_howto_table[r_type];
}

/* Perform a relocation as part of a final link.  */

static bfd_reloc_status_type
cr16c_elf_final_link_relocate (reloc_howto_type *howto,
			       bfd *abfd,
			       bfd *output_bfd ATTRIBUTE_UNUSED,
			       asection *input_section,
			       bfd_byte *data,
			       bfd_vma octets,
			       bfd_vma Rvalue,
			       bfd_vma addend ATTRIBUTE_UNUSED,
			       struct bfd_link_info *info ATTRIBUTE_UNUSED,
			       asection *sym_sec ATTRIBUTE_UNUSED,
			       int is_local ATTRIBUTE_UNUSED)
{
  long value;
  short sword;			/* Extracted from the hole and put back.  */
  unsigned long format, addr_type, code_factor;
  unsigned short size;
  unsigned short r_type;
  asymbol *symbol = NULL;

  unsigned long disp20_opcod;
  char neg = 0;
  char neg2pos = 0;

  long left_val = 0;
  long plus_factor = 0;		/* To be added to the hole.  */

#define MIN_BYTE	((int) 0xFFFFFF80)
#define MIN_WORD	((int) 0xFFFF8000)
#define	MAX_UWORD	((unsigned) 0x0000FFFF)
#define	MAX_UBYTE	((unsigned) 0x000000FF)

  r_type = reloc_map_index[howto->type].cr_reloc_type;
  format = r_type & R_FORMAT;
  size = r_type & R_SIZESP;
  addr_type = r_type & R_ADDRTYPE;
  code_factor = ((addr_type == R_CODE_ADDR) ? 1 : 0);

  if (sym_sec)
    symbol = sym_sec->symbol;

  switch (format)
    {
    case R_NUMBER:
      switch (size)
	{
	case R_S_16C_08: 	/* One byte.  */
	  value = bfd_get_8 (abfd, (char *) data + octets);
	  break;
	case R_S_16C_16: 	/* Two bytes. */
	  sword = bfd_get_16 (abfd, (bfd_byte *) data + octets);
	  value = sword;
	  break;
	case R_S_16C_32:	/* Four bytes.  */
	  value = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	  break;
	default:
	  return bfd_reloc_notsupported;
	}
      break;

    case R_16C_DISPL:
      switch (size)
	{
	case R_S_16C_04:    /* word1(4-7).  */
	  value = bfd_get_8 (abfd, (char *) data + octets);
	  left_val = value & 0xF;
	  value = (value & 0xF0) >> 4;
	  value++;
	  value <<= 1;
	  break;
	case R_S_16C_08:    /* word1(0-3,8-11).  */
	  sword = bfd_get_16 (abfd, (char *) data + octets);
	  value = sword & 0x000F;
	  value |= ((sword & 0x0F00) >> 4);
	  left_val = sword & 0xF0F0;
	  value <<= 1;
	  if (value & 0x100)
	    value |= 0xFFFFFF00;
	  break;
	case R_S_16C_16:    /* word2.  */
	  sword = bfd_get_16 (abfd, (bfd_byte *) data + octets);
	  value = sword;
	  value = ((value & 0xFFFE) >> 1) | ((value & 0x1) << 15);
	  value <<= 1;
	  if (value & 0x10000)
	    value |= 0xFFFF0000;
	  break;
	case R_S_16C_24_a:	/* word1(0-7),word2.  */
	  value = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	  left_val = value & 0x0000FF00;
	  value = ((value & 0xFFFE0000) >> 17) |
	    ((value & 0x00010000) << 7) | ((value & 0x000000FF) << 15);
	  value <<= 1;
	  if (value & 0x1000000)
	    value |= 0xFE000000;
	  break;
	case R_S_16C_24:    /* word2(0-3,8-11),word3.  */
	  value = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	  left_val = value & 0x0000F0F0;
	  value = ((value >> 16) & 0x0000FFFF) |
	    ((value & 0x00000F00) << 8) | ((value & 0x0000000F) << 20);

	  value = ((value & 0x00FFFFFE) >> 1) | ((value & 0x00000001) << 23);

	  value <<= 1;
	  if (value & 0x1000000)
	    value |= 0xFE000000;
	  break;
	default:
	  return bfd_reloc_notsupported;
	}
      break;

    case R_16C_REGREL:
      switch (size)
	{
	case R_S_16C_04:    /* word1(12-15) not scaled.  */
	  value = bfd_get_8 (abfd, (char *) data + octets);
	  left_val = value & 0xF0;
	  value = value & 0xF;
	  break;
	case R_S_16C_04_a:	/* word1(12-15) scaled by 2.  */
	  value = bfd_get_8 (abfd, (char *) data + octets);
	  left_val = value & 0xF0;
	  value = value & 0xF;
	  value <<= 1;
	  break;
	case R_S_16C_14:    /* word1(4-5),word2(0-3,8-15).  */
	  value = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	  left_val = value & 0x00F0FFCF;
	  value = ((value & 0xc0000000) >> 24) |
	    ((value & 0x3F000000) >> 16) |
	    ((value & 0x000F0000) >> 16) | (value & 0x00000030);
	  break;
	case R_S_16C_16:    /* word2.  */
	  sword = bfd_get_16 (abfd, (bfd_byte *) data + octets);
	  value = sword;
	  break;
	case R_S_16C_20:    /* word2(8-11),word3.  */
	  value = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	  left_val = value & 0xF0;
	  value = (value & 0xF) << 16;
	  sword = bfd_get_16 (abfd, (bfd_byte *) data + octets + 1);
	  value = value | (unsigned short) sword;
	  disp20_opcod = bfd_get_32 (abfd, (bfd_byte *) data + octets - 3);
	  disp20_opcod |= 0x0FFF0000;
	  if ((disp20_opcod == 0x4FFF0018) ||	/* loadb -disp20(reg) */
	      (disp20_opcod == 0x5FFF0018) ||	/* loadb -disp20(rp)  */
	      (disp20_opcod == 0x8FFF0018) ||	/* loadd -disp20(reg) */
	      (disp20_opcod == 0x9FFF0018) ||	/* loadd -disp20(rp)  */
	      (disp20_opcod == 0xCFFF0018) ||	/* loadw -disp20(reg) */
	      (disp20_opcod == 0xDFFF0018) ||	/* loadw -disp20(rp)  */
	      (disp20_opcod == 0x4FFF0019) ||	/* storb -disp20(reg) */
	      (disp20_opcod == 0x5FFF0019) ||	/* storb -disp20(rp)  */
	      (disp20_opcod == 0x8FFF0019) ||	/* stord -disp20(reg) */
	      (disp20_opcod == 0x9FFF0019) ||	/* stord -disp20(rp)  */
	      (disp20_opcod == 0xCFFF0019) ||	/* storw -disp20(reg) */
	      (disp20_opcod == 0xDFFF0019))
	    {	/* storw -disp20(rp).  */
	      neg = 1;
	      value |= 0xFFF00000;
	    }

	  break;
	default:
	  return bfd_reloc_notsupported;
	}
      break;

    case R_16C_ABS:
      switch (size)
	{
	case R_S_16C_20:    /* word1(0-3),word2.  */
	  value = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	  left_val = value & 0x0000FFF0;
	  value = ((value & 0xFFFF0000) >> 16) |
	    ((value & 0x0000000F) << 16);
	  break;
	case R_S_16C_24:   /* word2(0-3,8-11),word3.  */
	  value = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	  left_val = value & 0x0000F0F0;
	  value = ((value & 0xFFFF0000) >> 16) |
	    ((value & 0x00000F00) << 8) | ((value & 0x0000000F) << 20);
	  break;
	default:
	  return bfd_reloc_notsupported;
	}
      break;

    case R_16C_IMMED:
      switch (size)
	{
	case R_S_16C_04:    /* word1/2(4-7).  */
	  value = bfd_get_8 (abfd, (char *) data + octets);
	  left_val = value & 0xF;
	  value = (value & 0xF0) >> 4;
	  break;
	case R_S_16C_16:    /* word2.  */
	  sword = bfd_get_16 (abfd, (bfd_byte *) data + octets);
	  value = sword;
	  break;
	case R_S_16C_20:    /* word1(0-3),word2.  */
	  value = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	  left_val = value & 0x0000FFF0;
	  value = ((value & 0xFFFF0000) >> 16) |
	    ((value & 0x0000000F) << 16);
	  break;
	case R_S_16C_32:    /* word2, word3.  */
	  value = bfd_get_32 (abfd, (bfd_byte *) data + octets);
	  value = ((value & 0x0000FFFF) << 16) |
	    ((value & 0xFFFF0000) >> 16);
	  break;
	default:
	  return bfd_reloc_notsupported;
	}
      break;
    default:
      return bfd_reloc_notsupported;
    }

  switch ((r_type & R_RELTO) >> 4)
    {

    case 0:	/* R_ABS.  */
      plus_factor = Rvalue;
      break;
    case 1:	/* R_PCREL.  */
      plus_factor = Rvalue -
	(input_section->output_section->vma + input_section->output_offset);
      break;
    default:
      return bfd_reloc_notsupported;
    }

  if (neg)
    {
      if (plus_factor >= -value)
	neg2pos = 1;
      /* We need to change load/stor with negative
	 displ opcode to positive disp opcode (CR16C).  */
    }

  value = value + (plus_factor >> code_factor);

  switch (format)
    {
    case R_NUMBER:
      switch (size)
	{
	case R_S_16C_08: 	/* One byte.  */
	  if (value > (int) MAX_UBYTE || value < MIN_BYTE)
	    return bfd_reloc_overflow;
	  value &= 0xFF;
	  bfd_put_8 (abfd, (bfd_vma) value, (unsigned char *) data + octets);
	  break;

	case R_S_16C_16:	/* Two bytes.  */
	  if (value > (int) MAX_UWORD || value < MIN_WORD)
	    return bfd_reloc_overflow;
	  value &= 0xFFFF;
	  sword = value;
	  bfd_put_16 (abfd, (bfd_vma) sword,
		      (unsigned char *) data + octets);
	  break;

	case R_S_16C_32:	/* Four bytes.  */
	  value &= 0xFFFFFFFF;
	  bfd_put_32 (abfd, (bfd_vma) value, (bfd_byte *) data + octets);
	  break;

	default:
	  return bfd_reloc_notsupported;
	}
      break;

    case R_16C_DISPL:
      switch (size)
	{
	case R_S_16C_04:	/* word1(4-7).  */
	  if ((value - 32) > 32 || value < 2)
	    return bfd_reloc_overflow;
	  value >>= 1;
	  value--;
	  value &= 0xF;
	  value <<= 4;
	  value |= left_val;
	  bfd_put_8 (abfd, (bfd_vma) value, (unsigned char *) data + octets);
	  break;

	case R_S_16C_08:    /* word1(0-3,8-11).  */
	  if (value > 255 || value < -256 || value == 0x80)
	    return bfd_reloc_overflow;
	  value &= 0x1FF;
	  value >>= 1;
	  sword = value & 0x000F;
	  sword |= (value & 0x00F0) << 4;
	  sword |= left_val;
	  bfd_put_16 (abfd, (bfd_vma) sword,
		      (unsigned char *) data + octets);
	  break;

	case R_S_16C_16:    /* word2.  */
	  if (value > 65535 || value < -65536)
	    return bfd_reloc_overflow;
	  value >>= 1;
	  value &= 0xFFFF;
	  value = ((value & 0x8000) >> 15) | ((value & 0x7FFF) << 1);
	  sword = value;
	  bfd_put_16 (abfd, (bfd_vma) sword,
		      (unsigned char *) data + octets);
	  break;

	case R_S_16C_24_a:	/* word1(0-7),word2.  */
	  if (value > 16777215 || value < -16777216)
	    return bfd_reloc_overflow;
	  value &= 0x1FFFFFF;
	  value >>= 1;
	  value = ((value & 0x00007FFF) << 17) |
	    ((value & 0x00800000) >> 7) | ((value & 0x007F8000) >> 15);
	  value |= left_val;
	  bfd_put_32 (abfd, (bfd_vma) value, (bfd_byte *) data + octets);
	  break;

	case R_S_16C_24:    /* word2(0-3,8-11),word3.  */
	  if (value > 16777215 || value < -16777216)
	    return bfd_reloc_overflow;
	  value &= 0x1FFFFFF;
	  value >>= 1;

	  value = ((value & 0x007FFFFF) << 1) | ((value & 0x00800000) >> 23);

	  value = ((value & 0x0000FFFF) << 16) |
	    ((value & 0x000F0000) >> 8) | ((value & 0x00F00000) >> 20);
	  value |= left_val;
	  bfd_put_32 (abfd, (bfd_vma) value, (bfd_byte *) data + octets);
	  break;

	default:
	  return bfd_reloc_notsupported;
	}
      break;

    case R_16C_REGREL:
      switch (size)
	{
	case R_S_16C_04:	/* word1(12-15) not scaled.  */
	  if (value > 13 || value < 0)
	    return bfd_reloc_overflow;
	  value &= 0xF;
	  value |= left_val;
	  bfd_put_8 (abfd, (bfd_vma) value, (unsigned char *) data + octets);
	  break;

	case R_S_16C_04_a:	/* word1(12-15) not scaled.  */
	  if (value > 26 || value < 0)
	    return bfd_reloc_overflow;
	  value &= 0x1F;
	  value >>= 1;
	  value |= left_val;
	  bfd_put_8 (abfd, (bfd_vma) value, (unsigned char *) data + octets);
	  break;

	case R_S_16C_14:	/* word1(4-5),word2(0-3,8-15).  */
	  if (value < 0 || value > 16383)
	    return bfd_reloc_overflow;
	  value &= 0x3FFF;
	  value = ((value & 0x000000c0) << 24) |
	    ((value & 0x00003F00) << 16) |
	    ((value & 0x0000000F) << 16) | (value & 0x00000030);
	  value |= left_val;
	  bfd_put_32 (abfd, (bfd_vma) value, (bfd_byte *) data + octets);
	  break;

	case R_S_16C_16:	/* word2.  */
	  if (value > 65535 || value < 0)
	    return bfd_reloc_overflow;
	  value &= 0xFFFF;
	  sword = value;
	  bfd_put_16 (abfd, (bfd_vma) sword,
		      (unsigned char *) data + octets);
	  break;

	case R_S_16C_20:	/* word2(8-11),word3.  */
	  /* if (value > 1048575 || value < 0) RELOC_ERROR(1); */
	  value &= 0xFFFFF;
	  sword = value & 0x0000FFFF;
	  value = (value & 0x000F0000) >> 16;
	  value |= left_val;
	  bfd_put_8 (abfd, (bfd_vma) value, (unsigned char *) data + octets);
	  bfd_put_16 (abfd, (bfd_vma) sword,
		      (unsigned char *) data + octets + 1);
	  if (neg2pos)
	    {
	      /* Change load/stor negative displ opcode
	         to load/stor positive displ opcode.  */
	      value = bfd_get_8 (abfd, (char *) data + octets - 3);
	      value &= 0xF7;
	      value |= 0x2;
	      bfd_put_8 (abfd, (bfd_vma) value,
			 (unsigned char *) data + octets - 3);
	    }
	  break;

	default:
	  return bfd_reloc_notsupported;
	}
      break;

    case R_16C_ABS:
      switch (size)
	{
	case R_S_16C_20:	/* word1(0-3),word2.  */
	  if (value > 1048575 || value < 0)
	    return bfd_reloc_overflow;
	  value &= 0xFFFFF;
	  value = ((value & 0x0000FFFF) << 16) |
	    ((value & 0x000F0000) >> 16);
	  value |= left_val;
	  bfd_put_32 (abfd, (bfd_vma) value, (bfd_byte *) data + octets);
	  break;

	case R_S_16C_24:	/* word2(0-3,8-11),word3.  */
	  /* if (value > 16777215 || value < 0) RELOC_ERROR(1); */
	  value &= 0xFFFFFF;
	  value = ((value & 0x0000FFFF) << 16) |
	    ((value & 0x000F0000) >> 8) | ((value & 0x00F00000) >> 20);
	  value |= left_val;
	  bfd_put_32 (abfd, (bfd_vma) value, (bfd_byte *) data + octets);
	  break;

	default:
	  return bfd_reloc_notsupported;
	}
      break;

    case R_16C_IMMED:
      switch (size)
	{
	case R_S_16C_04:	/* word1/2(4-7).  */
	  if (value > 15 || value < -1)
	    return bfd_reloc_overflow;
	  value &= 0xF;
	  value <<= 4;
	  value |= left_val;
	  bfd_put_8 (abfd, (bfd_vma) value, (unsigned char *) data + octets);
	  break;

	case R_S_16C_16:	/* word2.  */
	  if (value > 32767 || value < -32768)
	    return bfd_reloc_overflow;
	  value &= 0xFFFF;
	  sword = value;
	  bfd_put_16 (abfd, (bfd_vma) sword,
		      (unsigned char *) data + octets);
	  break;

	case R_S_16C_20:	/* word1(0-3),word2.  */
	  if (value > 1048575 || value < 0)
	    return bfd_reloc_overflow;
	  value &= 0xFFFFF;
	  value = ((value & 0x0000FFFF) << 16) |
	    ((value & 0x000F0000) >> 16);
	  value |= left_val;
	  bfd_put_32 (abfd, (bfd_vma) value, (bfd_byte *) data + octets);
	  break;

	case R_S_16C_32:	/* word2, word3.  */
	  value &= 0xFFFFFFFF;
	  value = ((value & 0x0000FFFF) << 16) |
	    ((value & 0xFFFF0000) >> 16);
	  bfd_put_32 (abfd, (bfd_vma) value, (bfd_byte *) data + octets);
	  break;

	default:
	  return bfd_reloc_notsupported;
	}
      break;
    default:
      return bfd_reloc_notsupported;
    }

  return bfd_reloc_ok;
}

/* Relocate a CR16C ELF section.  */

static bfd_boolean
elf32_cr16c_relocate_section (bfd *output_bfd,
			      struct bfd_link_info *info,
			      bfd *input_bfd,
			      asection *input_section,
			      bfd_byte *contents,
			      Elf_Internal_Rela *relocs,
			      Elf_Internal_Sym *local_syms,
			      asection **local_sections)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel, *relend;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);
      howto = elf_howto_table + r_type;

      if (info->relocatable)
	{
	  /* This is a relocatable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

	  continue;
	}

      /* This is a final link.  */
      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	}
      else
	{
	  bfd_boolean unresolved_reloc, warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);
	}

      r = cr16c_elf_final_link_relocate (howto, input_bfd, output_bfd,
					 input_section,
					 contents, rel->r_offset,
					 relocation, rel->r_addend,
					 info, sec, h == NULL);

      if (r != bfd_reloc_ok)
	{
	  const char *name;
	  const char *msg = (const char *) 0;

	  if (h != NULL)
	    name = h->root.root.string;
	  else
	    {
	      name = (bfd_elf_string_from_elf_section
		      (input_bfd, symtab_hdr->sh_link, sym->st_name));
	      if (name == NULL || *name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      if (!((*info->callbacks->reloc_overflow)
		    (info, (h ? &h->root : NULL), name, howto->name,
		     (bfd_vma) 0, input_bfd, input_section,
		     rel->r_offset)))
		return FALSE;
	      break;

	    case bfd_reloc_undefined:
	      if (!((*info->callbacks->undefined_symbol)
		    (info, name, input_bfd, input_section,
		     rel->r_offset, TRUE)))
		return FALSE;
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      goto common_error;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      goto common_error;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous error");
	      goto common_error;

	    default:
	      msg = _("internal error: unknown error");
	      /* fall through */

	    common_error:
	      if (!((*info->callbacks->warning)
		    (info, msg, name, input_bfd, input_section,
		     rel->r_offset)))
		return FALSE;
	      break;
	    }
	}
    }

  return TRUE;
}

static asection *
elf32_cr16c_gc_mark_hook (asection *sec,
			  struct bfd_link_info *info ATTRIBUTE_UNUSED,
			  Elf_Internal_Rela *rel,
			  struct elf_link_hash_entry *h,
			  Elf_Internal_Sym *sym)
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{

	default:
	  switch (h->root.type)
	    {
	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;

	    default:
	      break;
	    }
	}
    }
  else
    {
      return bfd_section_from_elf_index (sec->owner, sym->st_shndx);
    }

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

static bfd_boolean
elf32_cr16c_gc_sweep_hook (bfd *abfd ATTRIBUTE_UNUSED,
			   struct bfd_link_info *info ATTRIBUTE_UNUSED,
			   asection *sec ATTRIBUTE_UNUSED,
			   const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED)
{
  /* We don't support garbage collection of GOT and PLT relocs yet.  */
  return TRUE;
}

/* CR16C ELF uses three common sections:
   One is for default common symbols (placed in usual common section).
   Second is for near common symbols (placed in "ncommon" section).
   Third is for far common symbols (placed in "fcommon" section).
   The following implementation is based on elf32-mips architecture */

static asection  cr16c_elf_fcom_section;
static asymbol   cr16c_elf_fcom_symbol;
static asymbol * cr16c_elf_fcom_symbol_ptr;
static asection  cr16c_elf_ncom_section;
static asymbol   cr16c_elf_ncom_symbol;
static asymbol * cr16c_elf_ncom_symbol_ptr;

/* Given a BFD section, try to locate the
   corresponding ELF section index.  */

static bfd_boolean
elf32_cr16c_section_from_bfd_section (bfd *abfd ATTRIBUTE_UNUSED,
				      asection *sec,
				      int *retval)
{
  if (strcmp (bfd_get_section_name (abfd, sec), ".fcommon") == 0)
    *retval = SHN_CR16C_FCOMMON;
  else if (strcmp (bfd_get_section_name (abfd, sec), ".ncommon") == 0)
    *retval = SHN_CR16C_NCOMMON;
  else
    return FALSE;

  return TRUE;
}

/* Handle the special CR16C section numbers that a symbol may use.  */

static void
elf32_cr16c_symbol_processing (bfd *abfd ATTRIBUTE_UNUSED,
			       asymbol *asym)
{
  elf_symbol_type *elfsym = (elf_symbol_type *) asym;
  unsigned int indx;

  indx = elfsym->internal_elf_sym.st_shndx;

  switch (indx)
    {
    case SHN_CR16C_FCOMMON:
      if (cr16c_elf_fcom_section.name == NULL)
	{
	  /* Initialize the far common section.  */
	  cr16c_elf_fcom_section.name = ".fcommon";
	  cr16c_elf_fcom_section.flags = SEC_IS_COMMON | SEC_ALLOC;
	  cr16c_elf_fcom_section.output_section = &cr16c_elf_fcom_section;
	  cr16c_elf_fcom_section.symbol = &cr16c_elf_fcom_symbol;
	  cr16c_elf_fcom_section.symbol_ptr_ptr = &cr16c_elf_fcom_symbol_ptr;
	  cr16c_elf_fcom_symbol.name = ".fcommon";
	  cr16c_elf_fcom_symbol.flags = BSF_SECTION_SYM;
	  cr16c_elf_fcom_symbol.section = &cr16c_elf_fcom_section;
	  cr16c_elf_fcom_symbol_ptr = &cr16c_elf_fcom_symbol;
	}
      asym->section = &cr16c_elf_fcom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;
    case SHN_CR16C_NCOMMON:
      if (cr16c_elf_ncom_section.name == NULL)
	{
	  /* Initialize the far common section.  */
	  cr16c_elf_ncom_section.name = ".ncommon";
	  cr16c_elf_ncom_section.flags = SEC_IS_COMMON | SEC_ALLOC;
	  cr16c_elf_ncom_section.output_section = &cr16c_elf_ncom_section;
	  cr16c_elf_ncom_section.symbol = &cr16c_elf_ncom_symbol;
	  cr16c_elf_ncom_section.symbol_ptr_ptr = &cr16c_elf_ncom_symbol_ptr;
	  cr16c_elf_ncom_symbol.name = ".ncommon";
	  cr16c_elf_ncom_symbol.flags = BSF_SECTION_SYM;
	  cr16c_elf_ncom_symbol.section = &cr16c_elf_ncom_section;
	  cr16c_elf_ncom_symbol_ptr = &cr16c_elf_ncom_symbol;
	}
      asym->section = &cr16c_elf_ncom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;
    }
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We must handle the special cr16c section numbers here.  */

static bfd_boolean
elf32_cr16c_add_symbol_hook (bfd *abfd,
			     struct bfd_link_info *info ATTRIBUTE_UNUSED,
			     Elf_Internal_Sym *sym,
			     const char **namep ATTRIBUTE_UNUSED,
			     flagword *flagsp ATTRIBUTE_UNUSED,
			     asection **secp,
			     bfd_vma *valp)
{
  unsigned int indx = sym->st_shndx;

  switch (indx)
    {
    case SHN_CR16C_FCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".fcommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;
    case SHN_CR16C_NCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".ncommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;
    }

  return TRUE;
}

static bfd_boolean
elf32_cr16c_link_output_symbol_hook (struct bfd_link_info *info ATTRIBUTE_UNUSED,
				     const char *name ATTRIBUTE_UNUSED,
				     Elf_Internal_Sym *sym,
				     asection *input_sec,
				     struct elf_link_hash_entry *h ATTRIBUTE_UNUSED)
{
  /* If we see a common symbol, which implies a relocatable link, then
     if a symbol was in a special common section in an input file, mark
     it as a special common in the output file.  */

  if (sym->st_shndx == SHN_COMMON)
    {
      if (strcmp (input_sec->name, ".fcommon") == 0)
	sym->st_shndx = SHN_CR16C_FCOMMON;
      else if (strcmp (input_sec->name, ".ncommon") == 0)
	sym->st_shndx = SHN_CR16C_NCOMMON;
    }

  return TRUE;
}

/* Definitions for setting CR16C target vector.  */
#define TARGET_LITTLE_SYM		bfd_elf32_cr16c_vec
#define TARGET_LITTLE_NAME		"elf32-cr16c"
#define ELF_ARCH			bfd_arch_cr16c
#define ELF_MACHINE_CODE		EM_CR
#define ELF_MAXPAGESIZE			0x1
#define elf_symbol_leading_char		'_'

#define bfd_elf32_bfd_reloc_type_lookup		elf_cr16c_reloc_type_lookup
#define elf_info_to_howto			elf_cr16c_info_to_howto
#define elf_info_to_howto_rel			elf_cr16c_info_to_howto_rel
#define elf_backend_relocate_section		elf32_cr16c_relocate_section
#define elf_backend_gc_mark_hook        	elf32_cr16c_gc_mark_hook
#define elf_backend_gc_sweep_hook       	elf32_cr16c_gc_sweep_hook
#define elf_backend_symbol_processing		elf32_cr16c_symbol_processing
#define elf_backend_section_from_bfd_section 	elf32_cr16c_section_from_bfd_section
#define elf_backend_add_symbol_hook		elf32_cr16c_add_symbol_hook
#define elf_backend_link_output_symbol_hook 	elf32_cr16c_link_output_symbol_hook

#define elf_backend_can_gc_sections     1

#include "elf32-target.h"
