/* picoJava specific support for 32-bit ELF
   Copyright 1999, 2000, 2001, 2002, 2005 Free Software Foundation, Inc.
   Contributed by Steve Chamberlan of Transmeta (sac@pobox.com).

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
#include "elf/pj.h"

/* This function is used for normal relocs.  This is like the COFF
   function, and is almost certainly incorrect for other ELF targets.  */

static bfd_reloc_status_type
pj_elf_reloc (bfd *abfd,
	      arelent *reloc_entry,
	      asymbol *symbol_in,
	      void * data,
	      asection *input_section,
	      bfd *output_bfd,
	      char **error_message ATTRIBUTE_UNUSED)
{
  unsigned long insn;
  bfd_vma sym_value;
  enum elf_pj_reloc_type r_type;
  bfd_vma addr = reloc_entry->address;
  bfd_byte *hit_data = addr + (bfd_byte *) data;

  r_type = (enum elf_pj_reloc_type) reloc_entry->howto->type;

  if (output_bfd != NULL)
    {
      /* Partial linking--do nothing.  */
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (symbol_in != NULL
      && bfd_is_und_section (symbol_in->section))
    return bfd_reloc_undefined;

  if (bfd_is_com_section (symbol_in->section))
    sym_value = 0;
  else
    sym_value = (symbol_in->value +
		 symbol_in->section->output_section->vma +
		 symbol_in->section->output_offset);

  switch (r_type)
    {
    case R_PJ_DATA_DIR32:
      insn = bfd_get_32 (abfd, hit_data);
      insn += sym_value + reloc_entry->addend;
      bfd_put_32 (abfd, (bfd_vma) insn, hit_data);
      break;

      /* Relocations in code are always bigendian, no matter what the
	 data endianness is.  */

    case R_PJ_CODE_DIR32:
      insn = bfd_getb32 (hit_data);
      insn += sym_value + reloc_entry->addend;
      bfd_putb32 ((bfd_vma) insn, hit_data);
      break;

    case R_PJ_CODE_REL16:
      insn = bfd_getb16 (hit_data);
      insn += sym_value + reloc_entry->addend
        -  (input_section->output_section->vma
            + input_section->output_offset);
      bfd_putb16 ((bfd_vma) insn, hit_data);
      break;
    case R_PJ_CODE_LO16:
      insn = bfd_getb16 (hit_data);
      insn += sym_value + reloc_entry->addend;
      bfd_putb16 ((bfd_vma) insn, hit_data);
      break;

    case R_PJ_CODE_HI16:
      insn = bfd_getb16 (hit_data);
      insn += (sym_value + reloc_entry->addend) >> 16;
      bfd_putb16 ((bfd_vma) insn, hit_data);
      break;

    default:
      abort ();
      break;
    }

  return bfd_reloc_ok;
}

static reloc_howto_type pj_elf_howto_table[] =
{
  /* No relocation.  */
  HOWTO (R_PJ_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 pj_elf_reloc,		/* special_function */
	 "R_PJ_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 32 bit absolute relocation.  Setting partial_inplace to TRUE and
     src_mask to a non-zero value is similar to the COFF toolchain.  */
  HOWTO (R_PJ_DATA_DIR32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 pj_elf_reloc,		/* special_function */
	 "R_PJ_DIR32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 32 bit PC relative relocation.  */
  HOWTO (R_PJ_CODE_REL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 pj_elf_reloc,		/* special_function */
	 "R_PJ_REL32",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

/* 16 bit PC relative relocation.  */
  HOWTO (R_PJ_CODE_REL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overf6w */
	 pj_elf_reloc,		/* special_function */
	 "R_PJ_REL16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */
  EMPTY_HOWTO (4),
  EMPTY_HOWTO (5),
  HOWTO (R_PJ_CODE_DIR32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 pj_elf_reloc,		/* special_function */
	 "R_PJ_CODE_DIR32",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  EMPTY_HOWTO (7),
  EMPTY_HOWTO (8),
  EMPTY_HOWTO (9),
  EMPTY_HOWTO (10),
  EMPTY_HOWTO (11),
  EMPTY_HOWTO (12),

  HOWTO (R_PJ_CODE_LO16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 pj_elf_reloc,		/* special_function */
	 "R_PJ_LO16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

    HOWTO (R_PJ_CODE_HI16,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 pj_elf_reloc,		/* special_function */
	 "R_PJ_HI16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_PJ_GNU_VTINHERIT,    /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_PJ_GNU_VTINHERIT",  /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_PJ_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_PJ_GNU_VTENTRY",   /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */
};

/* This structure is used to map BFD reloc codes to PJ ELF relocs.  */

struct elf_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

/* An array mapping BFD reloc codes to PJ ELF relocs.  */

static const struct elf_reloc_map pj_reloc_map[] =
{
    { BFD_RELOC_NONE, 		R_PJ_NONE          },
    { BFD_RELOC_32, 		R_PJ_DATA_DIR32    },
    { BFD_RELOC_PJ_CODE_DIR16, 	R_PJ_CODE_DIR16    },
    { BFD_RELOC_PJ_CODE_DIR32, 	R_PJ_CODE_DIR32    },
    { BFD_RELOC_PJ_CODE_LO16, 	R_PJ_CODE_LO16     },
    { BFD_RELOC_PJ_CODE_HI16, 	R_PJ_CODE_HI16     },
    { BFD_RELOC_PJ_CODE_REL32,  R_PJ_CODE_REL32    },
    { BFD_RELOC_PJ_CODE_REL16,  R_PJ_CODE_REL16    },
    { BFD_RELOC_VTABLE_INHERIT, R_PJ_GNU_VTINHERIT },
    { BFD_RELOC_VTABLE_ENTRY,   R_PJ_GNU_VTENTRY   },
};

/* Given a BFD reloc code, return the howto structure for the
   corresponding PJ ELf reloc.  */

static reloc_howto_type *
pj_elf_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			  bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0; i < sizeof (pj_reloc_map) / sizeof (struct elf_reloc_map); i++)
    if (pj_reloc_map[i].bfd_reloc_val == code)
      return & pj_elf_howto_table[(int) pj_reloc_map[i].elf_reloc_val];

  return NULL;
}

/* Given an ELF reloc, fill in the howto field of a relent.  */

static void
pj_elf_info_to_howto (bfd *abfd ATTRIBUTE_UNUSED,
		      arelent *cache_ptr,
		      Elf_Internal_Rela *dst)
{
  unsigned int r;

  r = ELF32_R_TYPE (dst->r_info);

  BFD_ASSERT (r < (unsigned int) R_PJ_max);

  cache_ptr->howto = &pj_elf_howto_table[r];
}

/* Take this moment to fill in the special picoJava bits in the
   e_flags field.  */

static void
pj_elf_final_write_processing (bfd *abfd,
			       bfd_boolean linker ATTRIBUTE_UNUSED)
{
  elf_elfheader (abfd)->e_flags |= EF_PICOJAVA_ARCH;
  elf_elfheader (abfd)->e_flags |= EF_PICOJAVA_GNUCALLS;
}

#define TARGET_BIG_SYM		bfd_elf32_pj_vec
#define TARGET_BIG_NAME		"elf32-pj"
#define TARGET_LITTLE_SYM	bfd_elf32_pjl_vec
#define TARGET_LITTLE_NAME	"elf32-pjl"
#define ELF_ARCH		bfd_arch_pj
#define ELF_MACHINE_CODE	EM_PJ
#define ELF_MACHINE_ALT1	EM_PJ_OLD
#define ELF_MAXPAGESIZE		0x1000
#define bfd_elf32_bfd_get_relocated_section_contents bfd_generic_get_relocated_section_contents
#define bfd_elf32_bfd_reloc_type_lookup	             pj_elf_reloc_type_lookup
#define elf_backend_final_write_processing           pj_elf_final_write_processing
#define elf_info_to_howto		             pj_elf_info_to_howto
#include "elf32-target.h"
