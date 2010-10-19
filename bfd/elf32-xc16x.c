/* Infineon XC16X-specific support for 16-bit ELF.
   Copyright 2006  Free Software Foundation, Inc.
   Contributed by KPIT Cummins Infosystems 

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
   Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/xc16x.h"
#include "elf/dwarf2.h"
#include "libiberty.h"

static reloc_howto_type xc16x_elf_howto_table [] =
{
  /* This reloc does nothing.  */
  HOWTO (R_XC16X_NONE,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XC16X_NONE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 8 bit absolute relocation.  */
  HOWTO (R_XC16X_ABS_8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XC16X_ABS_8",	/* name */
	 TRUE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_XC16X_ABS_16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XC16X_ABS_16",	/* name */
	 TRUE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_XC16X_ABS_32,	/* type */
  	 0,			/* rightshift */
  	 2,			/* size (0 = byte, 1 = short, 2 = long) */
  	 32,			/* bitsize */
  	 FALSE,			/* pc_relative */
  	 0,			/* bitpos */
  	 complain_overflow_bitfield, /* complain_on_overflow */
  	 bfd_elf_generic_reloc,	/* special_function */
  	 "R_XC16X_ABS_32",	/* name */
  	 TRUE,			/* partial_inplace */
  	 0x00000000,		/* src_mask */
  	 0xffffffff,		/* dst_mask */
  	 FALSE),		/* pcrel_offset */


  /* A PC relative 8 bit relocation.  */
  HOWTO (R_XC16X_8_PCREL,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 8,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_XC16X_8_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 TRUE),		/* pcrel_offset */

  /* Relocation regarding page number.  */
    HOWTO (R_XC16X_PAG,	/* type */
  	 0,			/* rightshift */
  	 1,			/* size (0 = byte, 1 = short, 2 = long) */
  	 16,			/* bitsize */
  	 FALSE,			/* pc_relative */
  	 0,			/* bitpos */
  	 complain_overflow_signed, /* complain_on_overflow */
  	 bfd_elf_generic_reloc, /* special_function */
  	 "R_XC16X_PAG",	/* name */
  	 TRUE,			/* partial_inplace */
  	 0x00000000,		/* src_mask */
  	 0x0000ffff,		/* dst_mask */
  	 FALSE),		/* pcrel_offset */


  /* Relocation regarding page number.  */
      HOWTO (R_XC16X_POF,	/* type */
    	 0,			/* rightshift */
    	 1,			/* size (0 = byte, 1 = short, 2 = long) */
    	 16,			/* bitsize */
    	 FALSE,			/* pc_relative */
    	 0,			/* bitpos  */
    	 complain_overflow_signed, /* complain_on_overflow  */
    	 bfd_elf_generic_reloc, /* special_function  */
    	 "R_XC16X_POF",	/* name  */
    	 TRUE,			/* partial_inplace  */
    	 0x00000000,		/* src_mask  */
    	 0x0000ffff,		/* dst_mask  */
    	 FALSE),		/* pcrel_offset  */


  /* Relocation regarding segment number.   */
      HOWTO (R_XC16X_SEG,	/* type  */
    	 0,			/* rightshift  */
    	 1,			/* size (0 = byte, 1 = short, 2 = long)  */
    	 16,			/* bitsize  */
    	 FALSE,			/* pc_relative  */
    	 0,			/* bitpos  */
    	 complain_overflow_signed, /* complain_on_overflow  */
    	 bfd_elf_generic_reloc, /* special_function  */
    	 "R_XC16X_SEG",	/* name  */
    	 TRUE,			/* partial_inplace  */
    	 0x00000000,		/* src_mask  */
    	 0x0000ffff,		/* dst_mask  */
    	 FALSE),		/* pcrel_offset  */

  /* Relocation regarding segment offset.  */
      HOWTO (R_XC16X_SOF,	/* type  */
    	 0,			/* rightshift  */
    	 1,			/* size (0 = byte, 1 = short, 2 = long)  */
    	 16,			/* bitsize  */
    	 FALSE,			/* pc_relative  */
    	 0,			/* bitpos  */
    	 complain_overflow_signed, /* complain_on_overflow  */
    	 bfd_elf_generic_reloc, /* special_function  */
    	 "R_XC16X_SOF",	/* name */
    	 TRUE,			/* partial_inplace  */
    	 0x00000000,		/* src_mask  */
    	 0x0000ffff,		/* dst_mask  */
    	 FALSE)			/* pcrel_offset  */
};


/* Map BFD reloc types to XC16X ELF reloc types.  */

struct xc16x_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned int xc16x_reloc_val;
};

static const struct xc16x_reloc_map xc16x_reloc_map [] =
{
  { BFD_RELOC_NONE,           R_XC16X_NONE },
  { BFD_RELOC_8,              R_XC16X_ABS_8 },
  { BFD_RELOC_16,             R_XC16X_ABS_16 },
  { BFD_RELOC_32,             R_XC16X_ABS_32 },
  { BFD_RELOC_8_PCREL,        R_XC16X_8_PCREL },
  { BFD_RELOC_XC16X_PAG,      R_XC16X_PAG},
  { BFD_RELOC_XC16X_POF,      R_XC16X_POF},
  { BFD_RELOC_XC16X_SEG,      R_XC16X_SEG},
  { BFD_RELOC_XC16X_SOF,      R_XC16X_SOF},
};


/* This function is used to search for correct relocation type from
   howto structure.  */

static reloc_howto_type *
xc16x_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			 bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = ARRAY_SIZE (xc16x_reloc_map); --i;)
    if (xc16x_reloc_map [i].bfd_reloc_val == code)
      return & xc16x_elf_howto_table [xc16x_reloc_map[i].xc16x_reloc_val];

  return NULL;
}

/* For a particular operand this function is
   called to finalise the type of relocation.  */

static void
elf32_xc16x_info_to_howto (bfd *abfd ATTRIBUTE_UNUSED, arelent *bfd_reloc,
			   Elf_Internal_Rela *elf_reloc)
{
  unsigned int r;
  unsigned int i;

  r = ELF32_R_TYPE (elf_reloc->r_info);
  for (i = 0; i < ARRAY_SIZE (xc16x_elf_howto_table); i++)
    if (xc16x_elf_howto_table[i].type == r)
      {
	bfd_reloc->howto = &xc16x_elf_howto_table[i];
	return;
      }
  abort ();
}

static bfd_reloc_status_type
elf32_xc16x_final_link_relocate (unsigned long r_type,
				 bfd *input_bfd,
				 bfd *output_bfd ATTRIBUTE_UNUSED,
				 asection *input_section ATTRIBUTE_UNUSED,
				 bfd_byte *contents,
				 bfd_vma offset,
				 bfd_vma value,
				 bfd_vma addend,
				 struct bfd_link_info *info ATTRIBUTE_UNUSED,
				 asection *sym_sec ATTRIBUTE_UNUSED,
				 int is_local ATTRIBUTE_UNUSED)
{
  bfd_byte *hit_data = contents + offset;
  bfd_vma val1;

  switch (r_type)
    {
    case R_XC16X_NONE:
      return bfd_reloc_ok;

    case R_XC16X_ABS_16:
      value += addend;
      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_XC16X_8_PCREL:
      bfd_put_8 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

      /* Following case is to find page number from actual
	 address for this divide value by 16k i.e. page size.  */

    case R_XC16X_PAG:
      value += addend;
      value /= 0x4000;
      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

      /* Following case is to find page offset from actual address
	 for this take modulo of value by 16k i.e. page size.  */

    case R_XC16X_POF:
      value += addend;
      value %= 0x4000;
      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

      /* Following case is to find segment number from actual
	 address for this divide value by 64k i.e. segment size.  */

    case R_XC16X_SEG:
      value += addend;
      value /= 0x10000;
      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

      /* Following case is to find segment offset from actual address
	 for this take modulo of value by 64k i.e. segment size.  */

    case R_XC16X_SOF:
      value += addend;
      value %= 0x10000;
      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_XC16X_ABS_32:
      if (!strstr (input_section->name,".debug"))
	{
	  value += addend;
	  val1 = value;
	  value %= 0x4000;
	  val1 /= 0x4000;
	  val1 = val1 << 16;
	  value += val1;
	  bfd_put_32 (input_bfd, value, hit_data);
	}
      else
	{
	  value += addend;
	  bfd_put_32 (input_bfd, value, hit_data);
	}
      return bfd_reloc_ok;

    default:
      return bfd_reloc_notsupported;
    }
}

static bfd_boolean
elf32_xc16x_relocate_section (bfd *output_bfd,
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

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      unsigned int r_type;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      /* This is a final link.  */
      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);
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

      r = elf32_xc16x_final_link_relocate (r_type, input_bfd, output_bfd,
					input_section,
					contents, rel->r_offset,
					relocation, rel->r_addend,
					info, sec, h == NULL);
    }

  return TRUE;
}


static void
elf32_xc16x_final_write_processing (bfd *abfd,
				    bfd_boolean linker ATTRIBUTE_UNUSED)
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
    case bfd_mach_xc16x:
      val = 0x1000;
      break;

    case bfd_mach_xc16xl:
      val = 0x1001;
      break;

    case bfd_mach_xc16xs:
      val = 0x1002;
      break;
    }

  elf_elfheader (abfd)->e_flags |= val;
}

static unsigned long
elf32_xc16x_mach (flagword flags)
{  
  switch (flags)
    {
    case 0x1000:
    default: 
      return bfd_mach_xc16x;

    case 0x1001:
      return bfd_mach_xc16xl;

    case 0x1002:
      return bfd_mach_xc16xs;
    }
}


static bfd_boolean
elf32_xc16x_object_p (bfd *abfd)
{
  bfd_default_set_arch_mach (abfd, bfd_arch_xc16x,
			     elf32_xc16x_mach (elf_elfheader (abfd)->e_flags));
  return TRUE;
}


#define ELF_ARCH		bfd_arch_xc16x
#define ELF_MACHINE_CODE	EM_XC16X
#define ELF_MAXPAGESIZE		0x100

#define TARGET_LITTLE_SYM       bfd_elf32_xc16x_vec
#define TARGET_LITTLE_NAME	"elf32-xc16x"
#define elf_backend_final_write_processing	elf32_xc16x_final_write_processing
#define elf_backend_object_p   		elf32_xc16x_object_p
#define elf_backend_can_gc_sections	1
#define bfd_elf32_bfd_reloc_type_lookup	xc16x_reloc_type_lookup
#define elf_info_to_howto		elf32_xc16x_info_to_howto
#define elf_info_to_howto_rel		elf32_xc16x_info_to_howto
#define elf_backend_relocate_section  	elf32_xc16x_relocate_section
#define elf_backend_rela_normal		1

#include "elf32-target.h"
