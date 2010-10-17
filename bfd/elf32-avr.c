/* AVR-specific support for 32-bit ELF
   Copyright 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.
   Contributed by Denis Chertykov <denisc@overta.ru>

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/avr.h"

static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void avr_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static asection *elf32_avr_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static bfd_boolean elf32_avr_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_boolean elf32_avr_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_reloc_status_type avr_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, bfd_vma));
static bfd_boolean elf32_avr_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static void bfd_elf_avr_final_write_processing PARAMS ((bfd *, bfd_boolean));
static bfd_boolean elf32_avr_object_p PARAMS ((bfd *));

static reloc_howto_type elf_avr_howto_table[] =
{
  HOWTO (R_AVR_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_AVR_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 7 bit PC relative relocation.  */
  HOWTO (R_AVR_7_PCREL,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 7,			/* bitsize */
	 TRUE,			/* pc_relative */
	 3,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_AVR_7_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 13 bit PC relative relocation.  */
  HOWTO (R_AVR_13_PCREL,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 13,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_AVR_13_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xfff,			/* src_mask */
	 0xfff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_AVR_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation for command address.  */
  HOWTO (R_AVR_16_PM,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_16_PM",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 16 bit address.
     For LDI command.  */
  HOWTO (R_AVR_LO8_LDI,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_LO8_LDI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A high 8 bit absolute relocation of 16 bit address.
     For LDI command.  */
  HOWTO (R_AVR_HI8_LDI,		/* type */
	 8,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HI8_LDI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A high 6 bit absolute relocation of 22 bit address.
     For LDI command.  */
  HOWTO (R_AVR_HH8_LDI,		/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HH8_LDI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A negative low 8 bit absolute relocation of 16 bit address.
     For LDI command.  */
  HOWTO (R_AVR_LO8_LDI_NEG,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_LO8_LDI_NEG",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A hegative high 8 bit absolute relocation of 16 bit address.
     For LDI command.  */
  HOWTO (R_AVR_HI8_LDI_NEG,	/* type */
	 8,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HI8_LDI_NEG",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A hegative high 6 bit absolute relocation of 22 bit address.
     For LDI command.  */
  HOWTO (R_AVR_HH8_LDI_NEG,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HH8_LDI_NEG",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  */
  HOWTO (R_AVR_LO8_LDI_PM,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_LO8_LDI_PM",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A high 8 bit absolute relocation of 16 bit program memory address.
     For LDI command.  */
  HOWTO (R_AVR_HI8_LDI_PM,	/* type */
	 9,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HI8_LDI_PM",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A high 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  */
  HOWTO (R_AVR_HH8_LDI_PM,	/* type */
	 17,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HH8_LDI_PM",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of a negative 24 bit
     program memory address.  For LDI command.  */
  HOWTO (R_AVR_LO8_LDI_PM_NEG,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_LO8_LDI_PM_NEG", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A high 8 bit absolute relocation of a negative 16 bit
     program memory address.  For LDI command.  */
  HOWTO (R_AVR_HI8_LDI_PM_NEG,	/* type */
	 9,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HI8_LDI_PM_NEG", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A high 8 bit absolute relocation of a negative 24 bit
     program memory address.  For LDI command.  */
  HOWTO (R_AVR_HH8_LDI_PM_NEG,	/* type */
	 17,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HH8_LDI_PM_NEG", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* Relocation for CALL command in ATmega.  */
  HOWTO (R_AVR_CALL,		/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 23,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_CALL",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE)			/* pcrel_offset */
};

/* Map BFD reloc types to AVR ELF reloc types.  */

struct avr_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned int elf_reloc_val;
};

 static const struct avr_reloc_map avr_reloc_map[] =
{
  { BFD_RELOC_NONE,                 R_AVR_NONE },
  { BFD_RELOC_32,                   R_AVR_32 },
  { BFD_RELOC_AVR_7_PCREL,          R_AVR_7_PCREL },
  { BFD_RELOC_AVR_13_PCREL,         R_AVR_13_PCREL },
  { BFD_RELOC_16,                   R_AVR_16 },
  { BFD_RELOC_AVR_16_PM,            R_AVR_16_PM },
  { BFD_RELOC_AVR_LO8_LDI,          R_AVR_LO8_LDI},
  { BFD_RELOC_AVR_HI8_LDI,          R_AVR_HI8_LDI },
  { BFD_RELOC_AVR_HH8_LDI,          R_AVR_HH8_LDI },
  { BFD_RELOC_AVR_LO8_LDI_NEG,      R_AVR_LO8_LDI_NEG },
  { BFD_RELOC_AVR_HI8_LDI_NEG,      R_AVR_HI8_LDI_NEG },
  { BFD_RELOC_AVR_HH8_LDI_NEG,      R_AVR_HH8_LDI_NEG },
  { BFD_RELOC_AVR_LO8_LDI_PM,       R_AVR_LO8_LDI_PM },
  { BFD_RELOC_AVR_HI8_LDI_PM,       R_AVR_HI8_LDI_PM },
  { BFD_RELOC_AVR_HH8_LDI_PM,       R_AVR_HH8_LDI_PM },
  { BFD_RELOC_AVR_LO8_LDI_PM_NEG,   R_AVR_LO8_LDI_PM_NEG },
  { BFD_RELOC_AVR_HI8_LDI_PM_NEG,   R_AVR_HI8_LDI_PM_NEG },
  { BFD_RELOC_AVR_HH8_LDI_PM_NEG,   R_AVR_HH8_LDI_PM_NEG },
  { BFD_RELOC_AVR_CALL,             R_AVR_CALL }
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0;
       i < sizeof (avr_reloc_map) / sizeof (struct avr_reloc_map);
       i++)
    {
      if (avr_reloc_map[i].bfd_reloc_val == code)
	return &elf_avr_howto_table[avr_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Set the howto pointer for an AVR ELF reloc.  */

static void
avr_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_AVR_max);
  cache_ptr->howto = &elf_avr_howto_table[r_type];
}

static asection *
elf32_avr_gc_mark_hook (sec, info, rel, h, sym)
     asection *sec;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
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
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

static bfd_boolean
elf32_avr_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  /* We don't use got and plt entries for avr.  */
  return TRUE;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
elf32_avr_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof (Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
        h = sym_hashes[r_symndx - symtab_hdr->sh_info];
    }

  return TRUE;
}

/* Perform a single relocation.  By default we use the standard BFD
   routines, but a few relocs, we have to do them ourselves.  */

static bfd_reloc_status_type
avr_final_link_relocate (howto, input_bfd, input_section,
			 contents, rel, relocation)
     reloc_howto_type *  howto;
     bfd *               input_bfd;
     asection *          input_section;
     bfd_byte *          contents;
     Elf_Internal_Rela * rel;
     bfd_vma             relocation;
{
  bfd_reloc_status_type r = bfd_reloc_ok;
  bfd_vma               x;
  bfd_signed_vma	srel;

  switch (howto->type)
    {
    case R_AVR_7_PCREL:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;
      srel -= rel->r_offset;
      srel -= 2;	/* Branch instructions add 2 to the PC...  */
      srel -= (input_section->output_section->vma +
	       input_section->output_offset);

      if (srel & 1)
	return bfd_reloc_outofrange;
      if (srel > ((1 << 7) - 1) || (srel < - (1 << 7)))
	return bfd_reloc_overflow;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xfc07) | (((srel >> 1) << 3) & 0x3f8);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_13_PCREL:
      contents   += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;
      srel -= rel->r_offset;
      srel -= 2;	/* Branch instructions add 2 to the PC...  */
      srel -= (input_section->output_section->vma +
	       input_section->output_offset);

      if (srel & 1)
	return bfd_reloc_outofrange;

      /* AVR addresses commands as words.  */
      srel >>= 1;

      /* Check for overflow.  */
      if (srel < -2048 || srel > 2047)
	{
	  /* Apply WRAPAROUND if possible.  */
	  switch (bfd_get_mach (input_bfd))
	    {
	    case bfd_mach_avr2:
	    case bfd_mach_avr4:
	      break;

	    default:
	      return bfd_reloc_overflow;
	    }
	}

      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf000) | (srel & 0xfff);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_LO8_LDI:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HI8_LDI:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = (srel >> 8) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HH8_LDI:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = (srel >> 16) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_LO8_LDI_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HI8_LDI_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      srel = (srel >> 8) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HH8_LDI_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      srel = (srel >> 16) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_LO8_LDI_PM:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HI8_LDI_PM:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      srel = (srel >> 8) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HH8_LDI_PM:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      srel = (srel >> 16) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_LO8_LDI_PM_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HI8_LDI_PM_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      srel = (srel >> 8) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HH8_LDI_PM_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      srel = (srel >> 16) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_CALL:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      x = bfd_get_16 (input_bfd, contents);
      x |= ((srel & 0x10000) | ((srel << 3) & 0x1f00000)) >> 16;
      bfd_put_16 (input_bfd, x, contents);
      bfd_put_16 (input_bfd, (bfd_vma) srel & 0xffff, contents+2);
      break;

    default:
      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, rel->r_addend);
    }

  return r;
}

/* Relocate an AVR ELF section.  */
static bfd_boolean
elf32_avr_relocate_section (output_bfd, info, input_bfd, input_section,
			    contents, relocs, local_syms, local_sections)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  Elf_Internal_Shdr *           symtab_hdr;
  struct elf_link_hash_entry ** sym_hashes;
  Elf_Internal_Rela *           rel;
  Elf_Internal_Rela *           relend;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *           howto;
      unsigned long                r_symndx;
      Elf_Internal_Sym *           sym;
      asection *                   sec;
      struct elf_link_hash_entry * h;
      bfd_vma                      relocation;
      bfd_reloc_status_type        r;
      const char *                 name = NULL;
      int                          r_type;

      /* This is a final link.  */
      r_type = ELF32_R_TYPE (rel->r_info);
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto  = elf_avr_howto_table + ELF32_R_TYPE (rel->r_info);
      h      = NULL;
      sym    = NULL;
      sec    = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);

	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, sec) : name;
	}
      else
	{
	  bfd_boolean unresolved_reloc, warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);
	}

      r = avr_final_link_relocate (howto, input_bfd, input_section,
				   contents, rel, relocation);

      if (r != bfd_reloc_ok)
	{
	  const char * msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, name, howto->name, (bfd_vma) 0,
		 input_bfd, input_section, rel->r_offset);
	      break;

	    case bfd_reloc_undefined:
	      r = info->callbacks->undefined_symbol
		(info, name, input_bfd, input_section, rel->r_offset, TRUE);
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      break;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      break;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous relocation");
	      break;

	    default:
	      msg = _("internal error: unknown error");
	      break;
	    }

	  if (msg)
	    r = info->callbacks->warning
	      (info, msg, name, input_bfd, input_section, rel->r_offset);

	  if (! r)
	    return FALSE;
	}
    }

  return TRUE;
}

/* The final processing done just before writing out a AVR ELF object
   file.  This gets the AVR architecture right based on the machine
   number.  */

static void
bfd_elf_avr_final_write_processing (abfd, linker)
     bfd *abfd;
     bfd_boolean linker ATTRIBUTE_UNUSED;
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
    case bfd_mach_avr2:
      val = E_AVR_MACH_AVR2;
      break;

    case bfd_mach_avr1:
      val = E_AVR_MACH_AVR1;
      break;

    case bfd_mach_avr3:
      val = E_AVR_MACH_AVR3;
      break;

    case bfd_mach_avr4:
      val = E_AVR_MACH_AVR4;
      break;

    case bfd_mach_avr5:
      val = E_AVR_MACH_AVR5;
      break;
    }

  elf_elfheader (abfd)->e_machine = EM_AVR;
  elf_elfheader (abfd)->e_flags &= ~ EF_AVR_MACH;
  elf_elfheader (abfd)->e_flags |= val;
}

/* Set the right machine number.  */

static bfd_boolean
elf32_avr_object_p (abfd)
     bfd *abfd;
{
  unsigned int e_set = bfd_mach_avr2;
  if (elf_elfheader (abfd)->e_machine == EM_AVR
      || elf_elfheader (abfd)->e_machine == EM_AVR_OLD)
    {
      int e_mach = elf_elfheader (abfd)->e_flags & EF_AVR_MACH;
      switch (e_mach)
	{
	default:
	case E_AVR_MACH_AVR2:
	  e_set = bfd_mach_avr2;
	  break;

	case E_AVR_MACH_AVR1:
	  e_set = bfd_mach_avr1;
	  break;

	case E_AVR_MACH_AVR3:
	  e_set = bfd_mach_avr3;
	  break;

	case E_AVR_MACH_AVR4:
	  e_set = bfd_mach_avr4;
	  break;

	case E_AVR_MACH_AVR5:
	  e_set = bfd_mach_avr5;
	  break;
	}
    }
  return bfd_default_set_arch_mach (abfd, bfd_arch_avr,
				    e_set);
}

#define ELF_ARCH		bfd_arch_avr
#define ELF_MACHINE_CODE	EM_AVR
#define ELF_MACHINE_ALT1	EM_AVR_OLD
#define ELF_MAXPAGESIZE		1

#define TARGET_LITTLE_SYM       bfd_elf32_avr_vec
#define TARGET_LITTLE_NAME	"elf32-avr"

#define elf_info_to_howto	             avr_info_to_howto_rela
#define elf_info_to_howto_rel	             NULL
#define elf_backend_relocate_section         elf32_avr_relocate_section
#define elf_backend_gc_mark_hook             elf32_avr_gc_mark_hook
#define elf_backend_gc_sweep_hook            elf32_avr_gc_sweep_hook
#define elf_backend_check_relocs             elf32_avr_check_relocs
#define elf_backend_can_gc_sections          1
#define elf_backend_rela_normal		     1
#define elf_backend_final_write_processing \
					bfd_elf_avr_final_write_processing
#define elf_backend_object_p		elf32_avr_object_p

#include "elf32-target.h"
