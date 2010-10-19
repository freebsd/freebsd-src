/* BFD back-end for National Semiconductor's CRX ELF
   Copyright 2004 Free Software Foundation, Inc.
   Written by Tomer Levi, NSC, Israel.

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
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/crx.h"

static reloc_howto_type *elf_crx_reloc_type_lookup
  (bfd *, bfd_reloc_code_real_type);
static void elf_crx_info_to_howto
  (bfd *, arelent *, Elf_Internal_Rela *);
static bfd_boolean elf32_crx_relax_delete_bytes
  (struct bfd_link_info *, bfd *, asection *, bfd_vma, int);
static bfd_reloc_status_type crx_elf_final_link_relocate
  (reloc_howto_type *, bfd *, bfd *, asection *,
   bfd_byte *, bfd_vma, bfd_vma, bfd_vma,
   struct bfd_link_info *, asection *, int);
static bfd_boolean elf32_crx_relocate_section
  (bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **);
static asection * elf32_crx_gc_mark_hook
  (asection *, struct bfd_link_info *, Elf_Internal_Rela *,
   struct elf_link_hash_entry *, Elf_Internal_Sym *);
static bfd_boolean elf32_crx_gc_sweep_hook
  (bfd *, struct bfd_link_info *, asection *,
   const Elf_Internal_Rela *);
static bfd_boolean elf32_crx_relax_section
  (bfd *, asection *, struct bfd_link_info *, bfd_boolean *);
static bfd_byte * elf32_crx_get_relocated_section_contents
  (bfd *, struct bfd_link_info *, struct bfd_link_order *,
   bfd_byte *, bfd_boolean, asymbol **);

/* crx_reloc_map array maps BFD relocation enum into a CRGAS relocation type.  */

struct crx_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_enum; /* BFD relocation enum.  */
  unsigned short crx_reloc_type;	   /* CRX relocation type.  */
};

static const struct crx_reloc_map crx_reloc_map[R_CRX_MAX] =
{
  {BFD_RELOC_NONE,	    R_CRX_NONE},
  {BFD_RELOC_CRX_REL4,	    R_CRX_REL4},
  {BFD_RELOC_CRX_REL8,	    R_CRX_REL8},
  {BFD_RELOC_CRX_REL8_CMP,  R_CRX_REL8_CMP},
  {BFD_RELOC_CRX_REL16,	    R_CRX_REL16},
  {BFD_RELOC_CRX_REL24,	    R_CRX_REL24},
  {BFD_RELOC_CRX_REL32,	    R_CRX_REL32},
  {BFD_RELOC_CRX_REGREL12,  R_CRX_REGREL12},
  {BFD_RELOC_CRX_REGREL22,  R_CRX_REGREL22},
  {BFD_RELOC_CRX_REGREL28,  R_CRX_REGREL28},
  {BFD_RELOC_CRX_REGREL32,  R_CRX_REGREL32},
  {BFD_RELOC_CRX_ABS16,	    R_CRX_ABS16},
  {BFD_RELOC_CRX_ABS32,	    R_CRX_ABS32},
  {BFD_RELOC_CRX_NUM8,	    R_CRX_NUM8},
  {BFD_RELOC_CRX_NUM16,	    R_CRX_NUM16},
  {BFD_RELOC_CRX_NUM32,	    R_CRX_NUM32},
  {BFD_RELOC_CRX_IMM16,	    R_CRX_IMM16},
  {BFD_RELOC_CRX_IMM32,	    R_CRX_IMM32},
  {BFD_RELOC_CRX_SWITCH8,   R_CRX_SWITCH8},
  {BFD_RELOC_CRX_SWITCH16,  R_CRX_SWITCH16},
  {BFD_RELOC_CRX_SWITCH32,  R_CRX_SWITCH32}
};

static reloc_howto_type crx_elf_howto_table[] =
{
  HOWTO (R_CRX_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REL4,		/* type */
	 1,			/* rightshift */
	 0,			/* size */
	 4,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REL4",		/* name */
	 FALSE,			/* partial_inplace */
	 0xf,			/* src_mask */
	 0xf,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REL8,		/* type */
	 1,			/* rightshift */
	 0,			/* size */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REL8",		/* name */
	 FALSE,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REL8_CMP,	/* type */
	 1,			/* rightshift */
	 0,			/* size */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REL8_CMP",	/* name */
	 FALSE,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REL16,		/* type */
	 1,			/* rightshift */
	 1,			/* size */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REL16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REL24,		/* type */
	 1,			/* rightshift */
	 2,			/* size */
	 24,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REL24",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REL32,		/* type */
	 1,			/* rightshift */
	 2,			/* size */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REL32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REGREL12,	/* type */
	 0,			/* rightshift */
	 1,			/* size */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REGREL12",	/* name */
	 FALSE,			/* partial_inplace */
	 0xfff,			/* src_mask */
	 0xfff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REGREL22,	/* type */
	 0,			/* rightshift */
	 2,			/* size */
	 22,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REGREL22",	/* name */
	 FALSE,			/* partial_inplace */
	 0x3fffff,		/* src_mask */
	 0x3fffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REGREL28,	/* type */
	 0,			/* rightshift */
	 2,			/* size */
	 28,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REGREL28",	/* name */
	 FALSE,			/* partial_inplace */
	 0xfffffff,		/* src_mask */
	 0xfffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_REGREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_REGREL32",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_ABS16,		/* type */
	 0,			/* rightshift */
	 1,			/* size */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_ABS16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_ABS32,		/* type */
	 0,			/* rightshift */
	 2,			/* size */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_ABS32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_NUM8,		/* type */
	 0,			/* rightshift */
	 0,			/* size */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_NUM8",		/* name */
	 FALSE,			/* partial_inplace */
	 0xff,	  		/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_NUM16,		/* type */
	 0,			/* rightshift */
	 1,			/* size */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_NUM16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,  		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_NUM32,		/* type */
	 0,			/* rightshift */
	 2,			/* size */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_NUM32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,  		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_IMM16,		/* type */
	 0,			/* rightshift */
	 1,			/* size */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_IMM16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,  		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRX_IMM32,		/* type */
	 0,			/* rightshift */
	 2,			/* size */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_IMM32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,  		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
 
  /* An 8 bit switch table entry.  This is generated for an expression
     such as ``.byte L1 - L2''.  The offset holds the difference
     between the reloc address and L2.  */
  HOWTO (R_CRX_SWITCH8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_SWITCH8",	/* name */
	 FALSE,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 16 bit switch table entry.  This is generated for an expression
     such as ``.word L1 - L2''.  The offset holds the difference
     between the reloc address and L2.  */
  HOWTO (R_CRX_SWITCH16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_SWITCH16",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 32 bit switch table entry.  This is generated for an expression
     such as ``.long L1 - L2''.  The offset holds the difference
     between the reloc address and L2.  */
  HOWTO (R_CRX_SWITCH32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRX_SWITCH32",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE)			/* pcrel_offset */
};

/* Retrieve a howto ptr using a BFD reloc_code.  */

static reloc_howto_type *
elf_crx_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			   bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0; i < R_CRX_MAX; i++)
    if (code == crx_reloc_map[i].bfd_reloc_enum)
      return &crx_elf_howto_table[crx_reloc_map[i].crx_reloc_type];

  printf ("This relocation Type is not supported -0x%x\n", code);
  return 0;
}

/* Retrieve a howto ptr using an internal relocation entry.  */

static void
elf_crx_info_to_howto (bfd *abfd ATTRIBUTE_UNUSED, arelent *cache_ptr,
		       Elf_Internal_Rela *dst)
{
  unsigned int r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_CRX_MAX);
  cache_ptr->howto = &crx_elf_howto_table[r_type];
}

/* Perform a relocation as part of a final link.  */

static bfd_reloc_status_type
crx_elf_final_link_relocate (reloc_howto_type *howto, bfd *input_bfd,
			     bfd *output_bfd ATTRIBUTE_UNUSED,
			     asection *input_section, bfd_byte *contents,
			     bfd_vma offset, bfd_vma Rvalue, bfd_vma addend,
			     struct bfd_link_info *info ATTRIBUTE_UNUSED,
			     asection *sec ATTRIBUTE_UNUSED,
			     int is_local ATTRIBUTE_UNUSED)
{
  unsigned short r_type = howto->type;
  bfd_byte *hit_data = contents + offset;
  bfd_vma reloc_bits, check;

  switch (r_type)
    {
     case R_CRX_IMM16:
     case R_CRX_IMM32:
     case R_CRX_ABS16:
     case R_CRX_ABS32:
     case R_CRX_REL8_CMP:
     case R_CRX_REL16:
     case R_CRX_REL24:
     case R_CRX_REL32:
     case R_CRX_REGREL12:
     case R_CRX_REGREL22:
     case R_CRX_REGREL28:
     case R_CRX_REGREL32:
       /* 'hit_data' is relative to the start of the instruction, not the
	  relocation offset. Advance it to account for the exact offset.  */
       hit_data += 2;
       break;

     case R_CRX_REL4:
       /* This relocation type is used only in 'Branch if Equal to 0'
	  instructions and requires special handling.  */
       Rvalue -= 1;
       break;

     case R_CRX_NONE:
       return bfd_reloc_ok;
       break;

     case R_CRX_SWITCH8:
     case R_CRX_SWITCH16:
     case R_CRX_SWITCH32:
       /* We only care about the addend, where the difference between 
	  expressions is kept.  */
       Rvalue = 0;
       
     default:
       break;
    }

  if (howto->pc_relative)
    {
      /* Subtract the address of the section containing the location.  */
      Rvalue -= (input_section->output_section->vma
		 + input_section->output_offset);
      /* Subtract the position of the location within the section.  */
      Rvalue -= offset;
    }

  /* Add in supplied addend.  */
  Rvalue += addend;

  /* Complain if the bitfield overflows, whether it is considered
     as signed or unsigned.  */
  check = Rvalue >> howto->rightshift;

  /* Assumes two's complement.  This expression avoids
     overflow if howto->bitsize is the number of bits in
     bfd_vma.  */
  reloc_bits = (((1 << (howto->bitsize - 1)) - 1) << 1) | 1;

  if (((bfd_vma) check & ~reloc_bits) != 0
      && (((bfd_vma) check & ~reloc_bits)
	  != (-(bfd_vma) 1 & ~reloc_bits)))
    {
      /* The above right shift is incorrect for a signed
	 value.  See if turning on the upper bits fixes the
	 overflow.  */
      if (howto->rightshift && (bfd_signed_vma) Rvalue < 0)
	{
	  check |= ((bfd_vma) - 1
		    & ~((bfd_vma) - 1
			>> howto->rightshift));
	  if (((bfd_vma) check & ~reloc_bits)
	      != (-(bfd_vma) 1 & ~reloc_bits))
	    return bfd_reloc_overflow;
	}
      else
	return bfd_reloc_overflow;
    }

  /* Drop unwanted bits from the value we are relocating to.  */
  Rvalue >>= (bfd_vma) howto->rightshift;

  /* Apply dst_mask to select only relocatable part of the insn.  */
  Rvalue &= howto->dst_mask;

  switch (howto->size)
    {
     case 0:
       if (r_type == R_CRX_REL4)
	 {
	   Rvalue <<= 4;
	   Rvalue |= (bfd_get_8 (input_bfd, hit_data) & 0x0f);
	 }

       bfd_put_8 (input_bfd, (unsigned char) Rvalue, hit_data);
       break;

     case 1:
       if (r_type == R_CRX_REGREL12)
	 Rvalue |= (bfd_get_16 (input_bfd, hit_data) & 0xf000);

       bfd_put_16 (input_bfd, Rvalue, hit_data);
       break;

     case 2:
       if (r_type == R_CRX_REL24
	   || r_type == R_CRX_REGREL22
	   || r_type == R_CRX_REGREL28)
	 Rvalue |= (((bfd_get_16 (input_bfd, hit_data) << 16) |
		      bfd_get_16 (input_bfd, hit_data + 2)) & ~howto->dst_mask);

       if (r_type == R_CRX_NUM32 || r_type == R_CRX_SWITCH32)
	 /* Relocation on DATA is purely little-endian, that is, for a
	    multi-byte datum, the lowest address in memory contains the
	    little end of the datum, that is, the least significant byte.
	    Therefore we use BFD's byte Putting functions.  */
	 bfd_put_32 (input_bfd, Rvalue, hit_data);
       else
	 /* Relocation on INSTRUCTIONS is different : Instructions are
	    word-addressable, that is, each word itself is arranged according
	    to little-endian convention, whereas the words are arranged with
	    respect to one another in BIG ENDIAN fashion.
	    When there is an immediate value that spans a word boundary, it is
	    split in a big-endian way with respect to the words.  */
	 {
	   bfd_put_16 (input_bfd, (Rvalue >> 16) & 0xffff, hit_data);
	   bfd_put_16 (input_bfd, Rvalue & 0xffff, hit_data + 2);
	 }
     break;

     default:
       return bfd_reloc_notsupported;
    }

  return bfd_reloc_ok;
}

/* Delete some bytes from a section while relaxing.  */

static bfd_boolean
elf32_crx_relax_delete_bytes (struct bfd_link_info *link_info, bfd *abfd, 
			      asection *sec, bfd_vma addr, int count)
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Rela *irelalign;
  bfd_vma toaddr;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  struct elf_link_hash_entry **start_hashes;
  unsigned int symcount;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count,
	   (size_t) (toaddr - addr - count));
  sec->size -= count;

  /* Adjust all the relocs.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      /* Get the new reloc address.  */
      if ((irel->r_offset > addr
	   && irel->r_offset < toaddr))
	irel->r_offset -= count;
    }

  /* Adjust the local symbols defined in this section.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  for (isymend = isym + symtab_hdr->sh_info; isym < isymend; isym++)
    {
      if (isym->st_shndx == sec_shndx
	  && isym->st_value > addr
	  && isym->st_value < toaddr)
	{
	  /* Adjust the addend of SWITCH relocations in this section, 
	     which reference this local symbol.  */
	  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
	    {
	      unsigned long r_symndx;
	      Elf_Internal_Sym *rsym;
	      bfd_vma addsym, subsym;

	      /* Skip if not a SWITCH relocation.  */
	      if (ELF32_R_TYPE (irel->r_info) != (int) R_CRX_SWITCH8
		  && ELF32_R_TYPE (irel->r_info) != (int) R_CRX_SWITCH16
		  && ELF32_R_TYPE (irel->r_info) != (int) R_CRX_SWITCH32)
		  continue;

	      r_symndx = ELF32_R_SYM (irel->r_info);
	      rsym = (Elf_Internal_Sym *) symtab_hdr->contents + r_symndx;

	      /* Skip if not the local adjusted symbol.  */
	      if (rsym != isym)
		continue;

	      addsym = isym->st_value;
	      subsym = addsym - irel->r_addend;

	      /* Fix the addend only when -->> (addsym > addr >= subsym).  */
	      if (subsym <= addr)
		irel->r_addend -= count;
	      else
		continue;
	    }

	  isym->st_value -= count;
	}
    }

  /* Now adjust the global symbols defined in this section.  */
  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
	      - symtab_hdr->sh_info);
  sym_hashes = start_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;

  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;

      /* The '--wrap SYMBOL' option is causing a pain when the object file, 
	 containing the definition of __wrap_SYMBOL, includes a direct 
	 call to SYMBOL as well. Since both __wrap_SYMBOL and SYMBOL reference 
	 the same symbol (which is __wrap_SYMBOL), but still exist as two 
	 different symbols in 'sym_hashes', we don't want to adjust 
	 the global symbol __wrap_SYMBOL twice.  
	 This check is only relevant when symbols are being wrapped.  */
      if (link_info->wrap_hash != NULL)
	{
	  struct elf_link_hash_entry **cur_sym_hashes;
	  
	  /* Loop only over the symbols whom been already checked.  */
	  for (cur_sym_hashes = start_hashes; cur_sym_hashes < sym_hashes; 
	       cur_sym_hashes++)
	    {
	      /* If the current symbol is identical to 'sym_hash', that means 
		 the symbol was already adjusted (or at least checked).  */
	      if (*cur_sym_hashes == sym_hash)
		break;
	    }
	  /* Don't adjust the symbol again.  */
	  if (cur_sym_hashes < sym_hashes)
	    continue;
	}

      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec
	  && sym_hash->root.u.def.value > addr
	  && sym_hash->root.u.def.value < toaddr)
	sym_hash->root.u.def.value -= count;
    }

  return TRUE;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   which uses elf32_crx_relocate_section.  */

static bfd_byte *
elf32_crx_get_relocated_section_contents (bfd *output_bfd,
					  struct bfd_link_info *link_info,
					  struct bfd_link_order *link_order,
					  bfd_byte *data,
					  bfd_boolean relocatable,
					  asymbol **symbols)
{
  Elf_Internal_Shdr *symtab_hdr;
  asection *input_section = link_order->u.indirect.section;
  bfd *input_bfd = input_section->owner;
  asection **sections = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Sym *isymbuf = NULL;

  /* We only need to handle the case of relaxing, or of having a
     particular set of section contents, specially.  */
  if (relocatable
      || elf_section_data (input_section)->this_hdr.contents == NULL)
    return bfd_generic_get_relocated_section_contents (output_bfd, link_info,
						       link_order, data,
						       relocatable,
						       symbols);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  memcpy (data, elf_section_data (input_section)->this_hdr.contents,
	  (size_t) input_section->size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      Elf_Internal_Sym *isym;
      Elf_Internal_Sym *isymend;
      asection **secpp;
      bfd_size_type amt;

      internal_relocs = (_bfd_elf_link_read_relocs
			 (input_bfd, input_section, (PTR) NULL,
			  (Elf_Internal_Rela *) NULL, FALSE));
      if (internal_relocs == NULL)
	goto error_return;

      if (symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      amt = symtab_hdr->sh_info;
      amt *= sizeof (asection *);
      sections = bfd_malloc (amt);
      if (sections == NULL && amt != 0)
	goto error_return;

      isymend = isymbuf + symtab_hdr->sh_info;
      for (isym = isymbuf, secpp = sections; isym < isymend; ++isym, ++secpp)
	{
	  asection *isec;

	  if (isym->st_shndx == SHN_UNDEF)
	    isec = bfd_und_section_ptr;
	  else if (isym->st_shndx == SHN_ABS)
	    isec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    isec = bfd_com_section_ptr;
	  else
	    isec = bfd_section_from_elf_index (input_bfd, isym->st_shndx);

	  *secpp = isec;
	}

      if (! elf32_crx_relocate_section (output_bfd, link_info, input_bfd,
				     input_section, data, internal_relocs,
				     isymbuf, sections))
	goto error_return;

      if (sections != NULL)
	free (sections);
      if (isymbuf != NULL
	  && symtab_hdr->contents != (unsigned char *) isymbuf)
	free (isymbuf);
      if (elf_section_data (input_section)->relocs != internal_relocs)
	free (internal_relocs);
    }

  return data;

 error_return:
  if (sections != NULL)
    free (sections);
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (internal_relocs != NULL
      && elf_section_data (input_section)->relocs != internal_relocs)
    free (internal_relocs);
  return NULL;
}

/* Relocate a CRX ELF section.  */

static bfd_boolean
elf32_crx_relocate_section (bfd *output_bfd, struct bfd_link_info *info,
			    bfd *input_bfd, asection *input_section,
			    bfd_byte *contents, Elf_Internal_Rela *relocs,
			    Elf_Internal_Sym *local_syms,
			    asection **local_sections)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel, *relend;

  if (info->relocatable)
    return TRUE;

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
      howto = crx_elf_howto_table + (r_type);

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

      r = crx_elf_final_link_relocate (howto, input_bfd, output_bfd,
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
	       /* Fall through.  */

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

/* This function handles relaxing for the CRX.

   There's quite a few relaxing opportunites available on the CRX:

	* bal/bcond:32 -> bal/bcond:16				   2 bytes
	* bcond:16 -> bcond:8					   2 bytes
	* cmpbcond:24 -> cmpbcond:8				   2 bytes
	* arithmetic imm32 -> arithmetic imm16			   2 bytes

   Symbol- and reloc-reading infrastructure copied from elf-m10200.c.  */

static bfd_boolean
elf32_crx_relax_section (bfd *abfd, asection *sec,
			 struct bfd_link_info *link_info, bfd_boolean *again)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  Elf_Internal_Sym *isymbuf = NULL;

  /* Assume nothing changes.  */
  *again = FALSE;

  /* We don't have to do anything for a relocatable link, if
     this section does not have relocs, or if this is not a
     code section.  */
  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  /* Walk through them looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;

      /* If this isn't something that can be relaxed, then ignore
	 this reloc.  */
      if (ELF32_R_TYPE (irel->r_info) != (int) R_CRX_REL32
	  && ELF32_R_TYPE (irel->r_info) != (int) R_CRX_REL16
	  && ELF32_R_TYPE (irel->r_info) != (int) R_CRX_REL24
	  && ELF32_R_TYPE (irel->r_info) != (int) R_CRX_IMM32)
	continue;

      /* Get the section contents if we haven't done so already.  */
      if (contents == NULL)
	{
	  /* Get cached copy if it exists.  */
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  /* Go get them off disk.  */
	  else if (!bfd_malloc_and_get_section (abfd, sec, &contents))
	    goto error_return;
	}

      /* Read this BFD's local symbols if we haven't done so already.  */
      if (isymbuf == NULL && symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  Elf_Internal_Sym *isym;
	  asection *sym_sec;

	  isym = isymbuf + ELF32_R_SYM (irel->r_info);
	  if (isym->st_shndx == SHN_UNDEF)
	    sym_sec = bfd_und_section_ptr;
	  else if (isym->st_shndx == SHN_ABS)
	    sym_sec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    sym_sec = bfd_com_section_ptr;
	  else
	    sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
	  symval = (isym->st_value
		    + sym_sec->output_section->vma
		    + sym_sec->output_offset);
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;

	  /* An external symbol.  */
	  indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);

	  if (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	    /* This appears to be a reference to an undefined
	       symbol.  Just ignore it--it will be caught by the
	       regular reloc processing.  */
	    continue;

	  symval = (h->root.u.def.value
		    + h->root.u.def.section->output_section->vma
		    + h->root.u.def.section->output_offset);
	}

      /* For simplicity of coding, we are going to modify the section
	 contents, the section relocs, and the BFD symbol table.  We
	 must tell the rest of the code not to free up this
	 information.  It would be possible to instead create a table
	 of changes which have to be made, as is done in coff-mips.c;
	 that would be more work, but would require less memory when
	 the linker is run.  */

      /* Try to turn a 32bit pc-relative branch/call into
	 a 16bit pc-relative branch/call.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_CRX_REL32)
	{
	  bfd_vma value = symval;

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= irel->r_offset;
	  value += irel->r_addend;

	  /* See if the value will fit in 16 bits, note the high value is
	     0xfffe + 2 as the target will be two bytes closer if we are
	     able to relax.  */
	  if ((long) value < 0x10000 && (long) value > -0x10002)
	    {
	      unsigned short code;

	      /* Get the opcode.  */
	      code = (unsigned short) bfd_get_16 (abfd, contents + irel->r_offset);

	      /* Verify it's a 'bal'/'bcond' and fix the opcode.  */
	      if ((code & 0xfff0) == 0x3170)
		bfd_put_8 (abfd, 0x30, contents + irel->r_offset + 1);
	      else if ((code & 0xf0ff) == 0x707f)
		bfd_put_8 (abfd, 0x7e, contents + irel->r_offset);
	      else
		continue;

	      /* Note that we've changed the relocs, section contents, etc.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      elf_section_data (sec)->this_hdr.contents = contents;
	      symtab_hdr->contents = (unsigned char *) isymbuf;

	      /* Fix the relocation's type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_CRX_REL16);

	      /* Delete two bytes of data.  */
	      if (!elf32_crx_relax_delete_bytes (link_info, abfd, sec,
						   irel->r_offset + 2, 2))
		goto error_return;

	      /* That will change things, so, we should relax again.
		 Note that this is not required, and it may be slow.  */
	      *again = TRUE;
	    }
	}

      /* Try to turn a 16bit pc-relative branch into an
	 8bit pc-relative branch.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_CRX_REL16)
	{
	  bfd_vma value = symval;

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= irel->r_offset;
	  value += irel->r_addend;

	  /* See if the value will fit in 8 bits, note the high value is
	     0xfc + 2 as the target will be two bytes closer if we are
	     able to relax.  */
	  if ((long) value < 0xfe && (long) value > -0x100)
	    {
	      unsigned short code;

	      /* Get the opcode.  */
	      code = (unsigned short) bfd_get_16 (abfd, contents + irel->r_offset);

	      /* Verify it's a 'bcond' opcode.  */
	      if ((code & 0xf0ff) != 0x707e)
		continue;

	      /* Note that we've changed the relocs, section contents, etc.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      elf_section_data (sec)->this_hdr.contents = contents;
	      symtab_hdr->contents = (unsigned char *) isymbuf;

	      /* Fix the relocation's type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_CRX_REL8);

	      /* Delete two bytes of data.  */
	      if (!elf32_crx_relax_delete_bytes (link_info, abfd, sec,
						   irel->r_offset + 2, 2))
		goto error_return;

	      /* That will change things, so, we should relax again.
		 Note that this is not required, and it may be slow.  */
	      *again = TRUE;
	    }
	}

      /* Try to turn a 24bit pc-relative cmp&branch into
	 an 8bit pc-relative cmp&branch.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_CRX_REL24)
	{
	  bfd_vma value = symval;

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= irel->r_offset;
	  value += irel->r_addend;

	  /* See if the value will fit in 8 bits, note the high value is
	     0x7e + 2 as the target will be two bytes closer if we are
	     able to relax.  */
	  if ((long) value < 0x100 && (long) value > -0x100)
	    {
	      unsigned short code;

	      /* Get the opcode.  */
	      code = (unsigned short) bfd_get_16 (abfd, contents + irel->r_offset);

	      /* Verify it's a 'cmp&branch' opcode.  */
	      if ((code & 0xfff0) != 0x3180 && (code & 0xfff0) != 0x3190
	       && (code & 0xfff0) != 0x31a0 && (code & 0xfff0) != 0x31c0
	       && (code & 0xfff0) != 0x31d0 && (code & 0xfff0) != 0x31e0
	       /* Or a Co-processor branch ('bcop').  */
	       && (code & 0xfff0) != 0x3010 && (code & 0xfff0) != 0x3110)
		continue;

	      /* Note that we've changed the relocs, section contents, etc.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      elf_section_data (sec)->this_hdr.contents = contents;
	      symtab_hdr->contents = (unsigned char *) isymbuf;

	      /* Fix the opcode.  */
	      bfd_put_8 (abfd, 0x30, contents + irel->r_offset + 1);

	      /* Fix the relocation's type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_CRX_REL8_CMP);

	      /* Delete two bytes of data.  */
	      if (!elf32_crx_relax_delete_bytes (link_info, abfd, sec,
						   irel->r_offset + 4, 2))
		goto error_return;

	      /* That will change things, so, we should relax again.
		 Note that this is not required, and it may be slow.  */
	      *again = TRUE;
	    }
	}

      /* Try to turn a 32bit immediate address into
	 a 16bit immediate address.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_CRX_IMM32)
	{
	  bfd_vma value = symval;

	  /* See if the value will fit in 16 bits.  */
	  if ((long) value < 0x7fff && (long) value > -0x8000)
	    {
	      unsigned short code;

	      /* Get the opcode.  */
	      code = (unsigned short) bfd_get_16 (abfd, contents + irel->r_offset);

	      /* Verify it's a 'arithmetic double'.  */
	      if ((code & 0xf0f0) != 0x20f0)
		continue;

	      /* Note that we've changed the relocs, section contents, etc.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      elf_section_data (sec)->this_hdr.contents = contents;
	      symtab_hdr->contents = (unsigned char *) isymbuf;

	      /* Fix the opcode.  */
	      bfd_put_8 (abfd, (code & 0xff) - 0x10, contents + irel->r_offset);

	      /* Fix the relocation's type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_CRX_IMM16);

	      /* Delete two bytes of data.  */
	      if (!elf32_crx_relax_delete_bytes (link_info, abfd, sec,
						   irel->r_offset + 2, 2))
		goto error_return;

	      /* That will change things, so, we should relax again.
		 Note that this is not required, and it may be slow.  */
	      *again = TRUE;
	    }
	}
    }

  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    {
      if (! link_info->keep_memory)
	free (isymbuf);
      else
	{
	  /* Cache the symbols for elf_link_input_bfd.  */
	  symtab_hdr->contents = (unsigned char *) isymbuf;
	}
    }

  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    {
      if (! link_info->keep_memory)
	free (contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
	}
    }

  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return TRUE;

 error_return:
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return FALSE;
}

static asection *
elf32_crx_gc_mark_hook (asection *sec,
			struct bfd_link_info *info ATTRIBUTE_UNUSED,
			Elf_Internal_Rela *rel ATTRIBUTE_UNUSED,
			struct elf_link_hash_entry *h,
			Elf_Internal_Sym *sym)
{
  if (h == NULL)
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  switch (h->root.type)
    {
    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      return h->root.u.def.section;

    case bfd_link_hash_common:
      return h->root.u.c.p->section;

    default:
      return NULL;
    }
}

/* Update the got entry reference counts for the section being removed.  */

static bfd_boolean
elf32_crx_gc_sweep_hook (bfd *abfd ATTRIBUTE_UNUSED,
			 struct bfd_link_info *info ATTRIBUTE_UNUSED,
			 asection *sec ATTRIBUTE_UNUSED,
			 const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED)
{
  /* We don't support garbage collection of GOT and PLT relocs yet.  */
  return TRUE;
}

/* Definitions for setting CRX target vector.  */
#define TARGET_LITTLE_SYM		bfd_elf32_crx_vec
#define TARGET_LITTLE_NAME		"elf32-crx"
#define ELF_ARCH			bfd_arch_crx
#define ELF_MACHINE_CODE		EM_CRX
#define ELF_MAXPAGESIZE			0x1
#define elf_symbol_leading_char		'_'

#define bfd_elf32_bfd_reloc_type_lookup	elf_crx_reloc_type_lookup
#define elf_info_to_howto		elf_crx_info_to_howto
#define elf_info_to_howto_rel		0
#define elf_backend_relocate_section	elf32_crx_relocate_section
#define bfd_elf32_bfd_relax_section	elf32_crx_relax_section
#define bfd_elf32_bfd_get_relocated_section_contents \
				elf32_crx_get_relocated_section_contents
#define elf_backend_gc_mark_hook        elf32_crx_gc_mark_hook
#define elf_backend_gc_sweep_hook       elf32_crx_gc_sweep_hook
#define elf_backend_can_gc_sections     1
#define elf_backend_rela_normal		1

#include "elf32-target.h"
