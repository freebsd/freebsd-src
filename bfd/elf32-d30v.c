/* D30V-specific support for 32-bit ELF
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.
   Contributed by Martin Hunt (hunt@cygnus.com).

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/d30v.h"

#define MAX32 ((bfd_signed_vma) 0x7fffffff)
#define MIN32 (- MAX32 - 1)

static bfd_reloc_status_type
bfd_elf_d30v_reloc (bfd *abfd,
		    arelent *reloc_entry,
		    asymbol *symbol,
		    void * data,
		    asection *input_section,
		    bfd *output_bfd,
		    char **error_message)
{
  bfd_signed_vma relocation;
  bfd_vma in1, in2, num;
  bfd_vma tmp_addr = 0;
  bfd_reloc_status_type r;
  asection *reloc_target_output_section;
  bfd_size_type addr = reloc_entry->address;
  bfd_reloc_status_type flag = bfd_reloc_ok;
  bfd_vma output_base = 0;
  reloc_howto_type *howto = reloc_entry->howto;
  int make_absolute = 0;

  if (output_bfd != NULL)
    {
      /* Partial linking -- do nothing.  */
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  r = bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
                             input_section, output_bfd, error_message);
  if (r != bfd_reloc_continue)
    return r;

  /* A hacked-up version of bfd_perform_reloc() follows.  */
 if (bfd_is_und_section (symbol->section)
      && (symbol->flags & BSF_WEAK) == 0
      && output_bfd == NULL)
    flag = bfd_reloc_undefined;

  /* Is the address of the relocation really within the section?  */
  if (reloc_entry->address > bfd_get_section_limit (abfd, input_section))
    return bfd_reloc_outofrange;

  /* Work out which section the relocation is targeted at and the
     initial relocation command value.  */

  /* Get symbol value.  (Common symbols are special.)  */
  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  reloc_target_output_section = symbol->section->output_section;

  /* Convert input-section-relative symbol value to absolute.  */
  output_base = reloc_target_output_section->vma;
  relocation += output_base + symbol->section->output_offset;

  /* Add in supplied addend.  */
  relocation += reloc_entry->addend;

  /* Here the variable relocation holds the final address of the
     symbol we are relocating against, plus any addend.  */
  if (howto->pc_relative)
    {
      tmp_addr = input_section->output_section->vma
	+ input_section->output_offset
	+ reloc_entry->address;
      relocation -= tmp_addr;
    }

  in1 = bfd_get_32 (abfd, (bfd_byte *) data + addr);
  in2 = bfd_get_32 (abfd, (bfd_byte *) data + addr + 4);

  /* Extract the addend.  */
  num = ((in2 & 0x3FFFF)
	 | ((in2 & 0xFF00000) >> 2)
	 | ((in1 & 0x3F) << 26));
  in1 &= 0xFFFFFFC0;
  in2 = 0x80000000;

  relocation += num;

  if (howto->pc_relative && howto->bitsize == 32)
    {
      /* The D30V has a PC that doesn't wrap and PC-relative jumps are
	 signed, so a PC-relative jump can't be more than +/- 2^31 bytes.
	 If one exceeds this, change it to an absolute jump.  */
      if (relocation > MAX32 || relocation < MIN32)
	{
	  relocation = (relocation + tmp_addr) & 0xffffffff;
	  make_absolute = 1;
	}
    }

  in1 |= (relocation >> 26) & 0x3F;		/* Top 6 bits.  */
  in2 |= ((relocation & 0x03FC0000) << 2);  	/* Next 8 bits.  */
  in2 |= relocation & 0x0003FFFF;		/* Bottom 18 bits.  */

  /* Change a PC-relative instruction to its
     absolute equivalent with this simple hack.  */
  if (make_absolute)
    in1 |= 0x00100000;

  bfd_put_32 (abfd, in1, (bfd_byte *) data + addr);
  bfd_put_32 (abfd, in2, (bfd_byte *) data + addr + 4);

  return flag;
}

static bfd_reloc_status_type
bfd_elf_d30v_reloc_21 (bfd *abfd,
		       arelent *reloc_entry,
		       asymbol *symbol,
		       void * data,
		       asection *input_section,
		       bfd *output_bfd,
		       char **error_message)
{
  bfd_vma relocation;
  bfd_vma in1, num;
  bfd_reloc_status_type r;
  asection *reloc_target_output_section;
  bfd_size_type addr = reloc_entry->address;
  bfd_reloc_status_type flag = bfd_reloc_ok;
  bfd_vma output_base = 0;
  reloc_howto_type *howto = reloc_entry->howto;
  int mask, max;

  if (output_bfd != NULL)
    {
      /* Partial linking -- do nothing.  */
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  r = bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
                             input_section, output_bfd, error_message);
  if (r != bfd_reloc_continue)
    return r;

  /* A hacked-up version of bfd_perform_reloc() follows.  */
  if (bfd_is_und_section (symbol->section)
      && (symbol->flags & BSF_WEAK) == 0
      && output_bfd == NULL)
    flag = bfd_reloc_undefined;

  /* Is the address of the relocation really within the section?  */
  if (reloc_entry->address > bfd_get_section_limit (abfd, input_section))
    return bfd_reloc_outofrange;

  /* Work out which section the relocation is targeted at and the
     initial relocation command value.  */

  /* Get symbol value.  (Common symbols are special.)  */
  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  reloc_target_output_section = symbol->section->output_section;

  /* Convert input-section-relative symbol value to absolute.  */
  output_base = reloc_target_output_section->vma;
  relocation += output_base + symbol->section->output_offset;

  /* Add in supplied addend.  */
  relocation += reloc_entry->addend;

  /* Here the variable relocation holds the final address of the
     symbol we are relocating against, plus any addend.  */

  if (howto->pc_relative)
    {
      relocation -= (input_section->output_section->vma
		     + input_section->output_offset);
      if (howto->pcrel_offset)
	relocation -= reloc_entry->address;
    }

  in1 = bfd_get_32 (abfd, (bfd_byte *) data + addr);

  mask =  (1 << howto->bitsize) - 1;
  if (howto->bitsize == 6)
    mask <<= 12;
  max = (1 << (howto->bitsize + 2)) - 1;

  /* Extract the addend.  */
  num = in1 & mask;  /* 18 bits.  */
  if (howto->bitsize == 6)
    num >>= 12;
  num <<= 3; /* shift left 3.  */
  in1 &= ~mask;  /* Mask out addend.  */

  relocation += num;
  if (howto->type == R_D30V_21_PCREL_R
      || howto->type == R_D30V_15_PCREL_R
      || howto->type == R_D30V_9_PCREL_R)
    relocation += 4;

  if ((int) relocation < 0)
    {
      if (~ (int) relocation > max)
	flag = bfd_reloc_overflow;
    }
  else
    {
      if ((int) relocation > max)
	flag = bfd_reloc_overflow;
    }

  relocation >>= 3;
  if (howto->bitsize == 6)
    in1 |= ((relocation & (mask >> 12)) << 12);
  else
    in1 |= relocation & mask;

  bfd_put_32 (abfd, in1, (bfd_byte *) data + addr);

  return flag;
}

static reloc_howto_type elf_d30v_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_D30V_NONE,		/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc,	/* Special_function.  */
	 "R_D30V_NONE",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Src_mask.  */
	 0,			/* Dst_mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A 6 bit absolute relocation.  */
  HOWTO (R_D30V_6,		/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 6,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc,	/* Special_function.  */
	 "R_D30V_6",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0x3f,			/* Src_mask.  */
	 0x3f,			/* Dst_mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A relative 9 bit relocation, right shifted by 3.  */
  HOWTO (R_D30V_9_PCREL,	/* Type.  */
	 3,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 6,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 bfd_elf_d30v_reloc_21,	/* Special_function.  */
	 "R_D30V_9_PCREL",	/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0x3f,			/* Src_mask.  */
	 0x3f,			/* Dst_mask.  */
	 TRUE),			/* PCrel_offset.  */

  /* A relative 9 bit relocation, right shifted by 3.  */
  HOWTO (R_D30V_9_PCREL_R,	/* Type.  */
	 3,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 6,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 bfd_elf_d30v_reloc_21,	/* Special_function.  */
	 "R_D30V_9_PCREL_R",	/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0x3f,			/* Src_mask.  */
	 0x3f,			/* Dst_mask.  */
	 TRUE),			/* PCrel_offset.  */

  /* An absolute 15 bit relocation, right shifted by 3.  */
  HOWTO (R_D30V_15,		/* Type.  */
	 3,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 12,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc,	/* Special_function.  */
	 "R_D30V_15",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0xfff,			/* Src_mask.  */
	 0xfff,			/* Dst_mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A relative 15 bit relocation, right shifted by 3.  */
  HOWTO (R_D30V_15_PCREL,	/* Type.  */
	 3,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 12,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 bfd_elf_d30v_reloc_21,	/* Special_function.  */
	 "R_D30V_15_PCREL",	/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0xfff,			/* Src_mask.  */
	 0xfff,			/* Dst_mask.  */
	 TRUE),			/* PCrel_offset.  */

  /* A relative 15 bit relocation, right shifted by 3.  */
  HOWTO (R_D30V_15_PCREL_R,	/* Type.  */
	 3,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 12,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 bfd_elf_d30v_reloc_21,	/* Special_function.  */
	 "R_D30V_15_PCREL_R",	/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0xfff,			/* Src_mask.  */
	 0xfff,			/* Dst_mask.  */
	 TRUE),			/* PCrel_offset.  */

  /* An absolute 21 bit relocation, right shifted by 3.  */
  HOWTO (R_D30V_21,		/* Type.  */
	 3,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 18,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc,	/* Special_function.  */
	 "R_D30V_21",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0x3ffff,		/* Src_mask.  */
	 0x3ffff,		/* Dst_mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A relative 21 bit relocation, right shifted by 3.  */
  HOWTO (R_D30V_21_PCREL,	/* Type.  */
	 3,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 18,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 bfd_elf_d30v_reloc_21,	/* Special_function.  */
	 "R_D30V_21_PCREL",	/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0x3ffff,		/* Src_mask.  */
	 0x3ffff,		/* Dst_mask.  */
	 TRUE),			/* PCrel_offset.  */

  /* A relative 21 bit relocation, right shifted by 3, in the Right container.  */
  HOWTO (R_D30V_21_PCREL_R,	/* Type.  */
	 3,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 18,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 bfd_elf_d30v_reloc_21,	/* Special_function.  */
	 "R_D30V_21_PCREL_R",	/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0x3ffff,		/* Src_mask.  */
	 0x3ffff,		/* Dst_mask.  */
	 TRUE),			/* PCrel_offset.  */

  /* A D30V 32 bit absolute relocation.  */
  HOWTO (R_D30V_32,		/* Type.  */
	 0,			/* Rightshift.  */
	 4,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 bfd_elf_d30v_reloc,	/* Special_function.  */
	 "R_D30V_32",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0xffffffff,		/* Src_mask.  */
	 0xffffffff,		/* Dst_mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A relative 32 bit relocation.  */
  HOWTO (R_D30V_32_PCREL,	/* Type.  */
	 0,			/* Rightshift.  */
	 4,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 bfd_elf_d30v_reloc,	/* Special_function.  */
	 "R_D30V_32_PCREL",	/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0xffffffff,		/* Src_mask.  */
	 0xffffffff,		/* Dst_mask.  */
	 TRUE),			/* PCrel_offset.  */

  /* A regular 32 bit absolute relocation.  */
  HOWTO (R_D30V_32_NORMAL,	/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc,	/* Special_function.  */
	 "R_D30V_32_NORMAL",	/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0xffffffff,		/* Src_mask.  */
	 0xffffffff,		/* Dst_mask.  */
	 FALSE),		/* PCrel_offset.  */

};

/* Map BFD reloc types to D30V ELF reloc types.  */

struct d30v_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct d30v_reloc_map d30v_reloc_map[] =
{
  { BFD_RELOC_NONE, R_D30V_NONE, },
  { BFD_RELOC_D30V_6, R_D30V_6 },
  { BFD_RELOC_D30V_9_PCREL, R_D30V_9_PCREL },
  { BFD_RELOC_D30V_9_PCREL_R, R_D30V_9_PCREL_R },
  { BFD_RELOC_D30V_15, R_D30V_15 },
  { BFD_RELOC_D30V_15_PCREL, R_D30V_15_PCREL },
  { BFD_RELOC_D30V_15_PCREL_R, R_D30V_15_PCREL_R },
  { BFD_RELOC_D30V_21, R_D30V_21 },
  { BFD_RELOC_D30V_21_PCREL, R_D30V_21_PCREL },
  { BFD_RELOC_D30V_21_PCREL_R, R_D30V_21_PCREL_R },
  { BFD_RELOC_D30V_32, R_D30V_32 },
  { BFD_RELOC_D30V_32_PCREL, R_D30V_32_PCREL },
  { BFD_RELOC_32, R_D30V_32_NORMAL },
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
				 bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0;
       i < sizeof (d30v_reloc_map) / sizeof (struct d30v_reloc_map);
       i++)
    {
      if (d30v_reloc_map[i].bfd_reloc_val == code)
	return &elf_d30v_howto_table[d30v_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Set the howto pointer for an D30V ELF reloc (type REL).  */

static void
d30v_info_to_howto_rel (bfd *abfd ATTRIBUTE_UNUSED,
			arelent *cache_ptr,
			Elf_Internal_Rela *dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_D30V_max);
  cache_ptr->howto = &elf_d30v_howto_table[r_type];
}

/* Set the howto pointer for an D30V ELF reloc (type RELA).  */

static void
d30v_info_to_howto_rela (bfd *abfd ATTRIBUTE_UNUSED,
			 arelent *cache_ptr,
			 Elf_Internal_Rela *dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_D30V_max);
  cache_ptr->howto = &elf_d30v_howto_table[r_type];
}

#define ELF_ARCH		bfd_arch_d30v
#define ELF_MACHINE_CODE	EM_D30V
#define ELF_MACHINE_ALT1	EM_CYGNUS_D30V
#define ELF_MAXPAGESIZE		0x1000

#define TARGET_BIG_SYM          bfd_elf32_d30v_vec
#define TARGET_BIG_NAME		"elf32-d30v"

#define elf_info_to_howto	d30v_info_to_howto_rela
#define elf_info_to_howto_rel	d30v_info_to_howto_rel
#define elf_backend_object_p	0
#define elf_backend_final_write_processing	0

#include "elf32-target.h"
