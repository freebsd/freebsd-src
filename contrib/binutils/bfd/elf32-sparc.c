/* SPARC-specific support for 32-bit ELF
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

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
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/sparc.h"
#include "opcode/sparc.h"

static reloc_howto_type *elf32_sparc_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void elf32_sparc_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static boolean elf32_sparc_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static boolean elf32_sparc_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static boolean elf32_sparc_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf32_sparc_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, boolean *));
static boolean elf32_sparc_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static boolean elf32_sparc_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static boolean elf32_sparc_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf32_sparc_merge_private_bfd_data PARAMS ((bfd *, bfd *));
static boolean elf32_sparc_object_p
  PARAMS ((bfd *));
static void elf32_sparc_final_write_processing
  PARAMS ((bfd *, boolean));
static enum elf_reloc_type_class elf32_sparc_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));
static asection * elf32_sparc_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static boolean elf32_sparc_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

/* The relocation "howto" table.  */

static bfd_reloc_status_type sparc_elf_notsupported_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type sparc_elf_wdisp16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

reloc_howto_type _bfd_sparc_elf_howto_table[] =
{
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_8,         0,0, 8,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_8",       false,0,0x000000ff,true),
  HOWTO(R_SPARC_16,        0,1,16,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_16",      false,0,0x0000ffff,true),
  HOWTO(R_SPARC_32,        0,2,32,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_32",      false,0,0xffffffff,true),
  HOWTO(R_SPARC_DISP8,     0,0, 8,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP8",   false,0,0x000000ff,true),
  HOWTO(R_SPARC_DISP16,    0,1,16,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP16",  false,0,0x0000ffff,true),
  HOWTO(R_SPARC_DISP32,    0,2,32,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP32",  false,0,0xffffffff,true),
  HOWTO(R_SPARC_WDISP30,   2,2,30,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP30", false,0,0x3fffffff,true),
  HOWTO(R_SPARC_WDISP22,   2,2,22,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP22", false,0,0x003fffff,true),
  HOWTO(R_SPARC_HI22,     10,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HI22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_22,        0,2,22,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_22",      false,0,0x003fffff,true),
  HOWTO(R_SPARC_13,        0,2,13,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_13",      false,0,0x00001fff,true),
  HOWTO(R_SPARC_LO10,      0,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_LO10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT10,     0,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GOT10",   false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT13,     0,2,13,false,0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_GOT13",   false,0,0x00001fff,true),
  HOWTO(R_SPARC_GOT22,    10,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GOT22",   false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC10,      0,2,10,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_PC10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_PC22,     10,2,22,true, 0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_PC22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_WPLT30,    2,2,30,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WPLT30",  false,0,0x3fffffff,true),
  HOWTO(R_SPARC_COPY,      0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_COPY",    false,0,0x00000000,true),
  HOWTO(R_SPARC_GLOB_DAT,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GLOB_DAT",false,0,0x00000000,true),
  HOWTO(R_SPARC_JMP_SLOT,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_JMP_SLOT",false,0,0x00000000,true),
  HOWTO(R_SPARC_RELATIVE,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_RELATIVE",false,0,0x00000000,true),
  HOWTO(R_SPARC_UA32,      0,2,32,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_UA32",    false,0,0xffffffff,true),
  HOWTO(R_SPARC_PLT32,     0,0,00,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_PLT32",   false,0,0xffffffff,true),
  HOWTO(R_SPARC_HIPLT22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HIPLT22",  false,0,0x00000000,true),
  HOWTO(R_SPARC_LOPLT10,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_LOPLT10",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT32,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT32",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT22",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT10,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT10",  false,0,0x00000000,true),
  HOWTO(R_SPARC_10,        0,2,10,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_10",      false,0,0x000003ff,true),
  HOWTO(R_SPARC_11,        0,2,11,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_11",      false,0,0x000007ff,true),
  /* These are for sparc64 in a 64 bit environment.
     Values need to be here because the table is indexed by reloc number.  */
  HOWTO(R_SPARC_64,        0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_64",      false,0,0x00000000,true),
  HOWTO(R_SPARC_OLO10,     0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_OLO10",   false,0,0x00000000,true),
  HOWTO(R_SPARC_HH22,      0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HH22",    false,0,0x00000000,true),
  HOWTO(R_SPARC_HM10,      0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HM10",    false,0,0x00000000,true),
  HOWTO(R_SPARC_LM22,      0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_LM22",    false,0,0x00000000,true),
  HOWTO(R_SPARC_PC_HH22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PC_HH22", false,0,0x00000000,true),
  HOWTO(R_SPARC_PC_HM10,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PC_HM10", false,0,0x00000000,true),
  HOWTO(R_SPARC_PC_LM22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PC_LM22", false,0,0x00000000,true),
  /* End sparc64 in 64 bit environment values.
     The following are for sparc64 in a 32 bit environment.  */
  HOWTO(R_SPARC_WDISP16,   2,2,16,true, 0,complain_overflow_signed,  sparc_elf_wdisp16_reloc,"R_SPARC_WDISP16", false,0,0x00000000,true),
  HOWTO(R_SPARC_WDISP19,   2,2,19,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP19", false,0,0x0007ffff,true),
  HOWTO(R_SPARC_UNUSED_42, 0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_UNUSED_42",false,0,0x00000000,true),
  HOWTO(R_SPARC_7,         0,2, 7,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_7",       false,0,0x0000007f,true),
  HOWTO(R_SPARC_5,         0,2, 5,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_5",       false,0,0x0000001f,true),
  HOWTO(R_SPARC_6,         0,2, 6,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_6",       false,0,0x0000003f,true),
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_UA64,      0,0, 0,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_UA64",    false,0,0x00000000,true),
  HOWTO(R_SPARC_UA16,      0,1,16,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_UA16",    false,0,0x0000ffff,true),
  HOWTO(R_SPARC_REV32,     0,2,32,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_REV32",   false,0,0xffffffff,true),
};
static reloc_howto_type elf32_sparc_vtinherit_howto =
  HOWTO (R_SPARC_GNU_VTINHERIT, 0,2,0,false,0,complain_overflow_dont, NULL, "R_SPARC_GNU_VTINHERIT", false,0, 0, false);
static reloc_howto_type elf32_sparc_vtentry_howto =
  HOWTO (R_SPARC_GNU_VTENTRY, 0,2,0,false,0,complain_overflow_dont, _bfd_elf_rel_vtable_reloc_fn,"R_SPARC_GNU_VTENTRY", false,0,0, false);

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct elf_reloc_map sparc_reloc_map[] =
{
  { BFD_RELOC_NONE, R_SPARC_NONE, },
  { BFD_RELOC_16, R_SPARC_16, },
  { BFD_RELOC_16_PCREL, R_SPARC_DISP16 },
  { BFD_RELOC_8, R_SPARC_8 },
  { BFD_RELOC_8_PCREL, R_SPARC_DISP8 },
  { BFD_RELOC_CTOR, R_SPARC_32 },
  { BFD_RELOC_32, R_SPARC_32 },
  { BFD_RELOC_32_PCREL, R_SPARC_DISP32 },
  { BFD_RELOC_HI22, R_SPARC_HI22 },
  { BFD_RELOC_LO10, R_SPARC_LO10, },
  { BFD_RELOC_32_PCREL_S2, R_SPARC_WDISP30 },
  { BFD_RELOC_SPARC_PLT32, R_SPARC_PLT32 },
  { BFD_RELOC_SPARC22, R_SPARC_22 },
  { BFD_RELOC_SPARC13, R_SPARC_13 },
  { BFD_RELOC_SPARC_GOT10, R_SPARC_GOT10 },
  { BFD_RELOC_SPARC_GOT13, R_SPARC_GOT13 },
  { BFD_RELOC_SPARC_GOT22, R_SPARC_GOT22 },
  { BFD_RELOC_SPARC_PC10, R_SPARC_PC10 },
  { BFD_RELOC_SPARC_PC22, R_SPARC_PC22 },
  { BFD_RELOC_SPARC_WPLT30, R_SPARC_WPLT30 },
  { BFD_RELOC_SPARC_COPY, R_SPARC_COPY },
  { BFD_RELOC_SPARC_GLOB_DAT, R_SPARC_GLOB_DAT },
  { BFD_RELOC_SPARC_JMP_SLOT, R_SPARC_JMP_SLOT },
  { BFD_RELOC_SPARC_RELATIVE, R_SPARC_RELATIVE },
  { BFD_RELOC_SPARC_WDISP22, R_SPARC_WDISP22 },
  { BFD_RELOC_SPARC_UA16, R_SPARC_UA16 },
  { BFD_RELOC_SPARC_UA32, R_SPARC_UA32 },
  { BFD_RELOC_SPARC_UA64, R_SPARC_UA64 },
  { BFD_RELOC_SPARC_10, R_SPARC_10 },
  { BFD_RELOC_SPARC_11, R_SPARC_11 },
  { BFD_RELOC_SPARC_64, R_SPARC_64 },
  { BFD_RELOC_SPARC_OLO10, R_SPARC_OLO10 },
  { BFD_RELOC_SPARC_HH22, R_SPARC_HH22 },
  { BFD_RELOC_SPARC_HM10, R_SPARC_HM10 },
  { BFD_RELOC_SPARC_LM22, R_SPARC_LM22 },
  { BFD_RELOC_SPARC_PC_HH22, R_SPARC_PC_HH22 },
  { BFD_RELOC_SPARC_PC_HM10, R_SPARC_PC_HM10 },
  { BFD_RELOC_SPARC_PC_LM22, R_SPARC_PC_LM22 },
  { BFD_RELOC_SPARC_WDISP16, R_SPARC_WDISP16 },
  { BFD_RELOC_SPARC_WDISP19, R_SPARC_WDISP19 },
  { BFD_RELOC_SPARC_7, R_SPARC_7 },
  { BFD_RELOC_SPARC_5, R_SPARC_5 },
  { BFD_RELOC_SPARC_6, R_SPARC_6 },
  { BFD_RELOC_SPARC_REV32, R_SPARC_REV32 },
  { BFD_RELOC_VTABLE_INHERIT, R_SPARC_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_SPARC_GNU_VTENTRY },
};

static reloc_howto_type *
elf32_sparc_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  switch (code)
    {
    case BFD_RELOC_VTABLE_INHERIT:
      return &elf32_sparc_vtinherit_howto;

    case BFD_RELOC_VTABLE_ENTRY:
      return &elf32_sparc_vtentry_howto;

    default:
      for (i = 0; i < sizeof (sparc_reloc_map) / sizeof (struct elf_reloc_map); i++)
        {
          if (sparc_reloc_map[i].bfd_reloc_val == code)
	    return &_bfd_sparc_elf_howto_table[(int) sparc_reloc_map[i].elf_reloc_val];
        }
    }
    bfd_set_error (bfd_error_bad_value);
    return NULL;
}

/* We need to use ELF32_R_TYPE so we have our own copy of this function,
   and elf64-sparc.c has its own copy.  */

static void
elf32_sparc_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  switch (ELF32_R_TYPE(dst->r_info))
    {
    case R_SPARC_GNU_VTINHERIT:
      cache_ptr->howto = &elf32_sparc_vtinherit_howto;
      break;

    case R_SPARC_GNU_VTENTRY:
      cache_ptr->howto = &elf32_sparc_vtentry_howto;
      break;

    default:
      BFD_ASSERT (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_SPARC_max_std);
      cache_ptr->howto = &_bfd_sparc_elf_howto_table[ELF32_R_TYPE(dst->r_info)];
    }
}

/* For unsupported relocs.  */

static bfd_reloc_status_type
sparc_elf_notsupported_reloc (abfd,
			     reloc_entry,
			     symbol,
			     data,
			     input_section,
			     output_bfd,
			     error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
{
  return bfd_reloc_notsupported;
}

/* Handle the WDISP16 reloc.  */

static bfd_reloc_status_type
sparc_elf_wdisp16_reloc (abfd,
			 reloc_entry,
			 symbol,
			 data,
			 input_section,
			 output_bfd,
			 error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;
  bfd_vma x;

  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != NULL)
    return bfd_reloc_continue;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  relocation = (symbol->value
		+ symbol->section->output_section->vma
		+ symbol->section->output_offset);
  relocation += reloc_entry->addend;
  relocation -=	(input_section->output_section->vma
		 + input_section->output_offset);
  relocation -= reloc_entry->address;

  x = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);
  x |= ((((relocation >> 2) & 0xc000) << 6)
	| ((relocation >> 2) & 0x3fff));
  bfd_put_32 (abfd, x, (bfd_byte *) data + reloc_entry->address);

  if ((bfd_signed_vma) relocation < - 0x40000
      || (bfd_signed_vma) relocation > 0x3ffff)
    return bfd_reloc_overflow;
  else
    return bfd_reloc_ok;
}

/* Functions for the SPARC ELF linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/ld.so.1"

/* The nop opcode we use.  */

#define SPARC_NOP 0x01000000

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 12

/* The first four entries in a procedure linkage table are reserved,
   and the initial contents are unimportant (we zero them out).
   Subsequent entries look like this.  See the SVR4 ABI SPARC
   supplement to see how this works.  */

/* sethi %hi(.-.plt0),%g1.  We fill in the address later.  */
#define PLT_ENTRY_WORD0 0x03000000
/* b,a .plt0.  We fill in the offset later.  */
#define PLT_ENTRY_WORD1 0x30800000
/* nop.  */
#define PLT_ENTRY_WORD2 SPARC_NOP

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static boolean
elf32_sparc_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sgot;
  asection *srelgot;
  asection *sreloc;

  if (info->relocateable)
    return true;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_offsets = elf_local_got_offsets (abfd);

  sgot = NULL;
  srelgot = NULL;
  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_SPARC_GOT10:
	case R_SPARC_GOT13:
	case R_SPARC_GOT22:
	  /* This symbol requires a global offset table entry.  */

	  if (dynobj == NULL)
	    {
	      /* Create the .got section.  */
	      elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! _bfd_elf_create_got_section (dynobj, info))
		return false;
	    }

	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  if (srelgot == NULL
	      && (h != NULL || info->shared))
	    {
	      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
	      if (srelgot == NULL)
		{
		  srelgot = bfd_make_section (dynobj, ".rela.got");
		  if (srelgot == NULL
		      || ! bfd_set_section_flags (dynobj, srelgot,
						  (SEC_ALLOC
						   | SEC_LOAD
						   | SEC_HAS_CONTENTS
						   | SEC_IN_MEMORY
						   | SEC_LINKER_CREATED
						   | SEC_READONLY))
		      || ! bfd_set_section_alignment (dynobj, srelgot, 2))
		    return false;
		}
	    }

	  if (h != NULL)
	    {
	      if (h->got.offset != (bfd_vma) -1)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      h->got.offset = sgot->_raw_size;

	      /* Make sure this symbol is output as a dynamic symbol.  */
	      if (h->dynindx == -1)
		{
		  if (! bfd_elf32_link_record_dynamic_symbol (info, h))
		    return false;
		}

	      srelgot->_raw_size += sizeof (Elf32_External_Rela);
	    }
	  else
	    {
	      /* This is a global offset table entry for a local
                 symbol.  */
	      if (local_got_offsets == NULL)
		{
		  bfd_size_type size;
		  register unsigned int i;

		  size = symtab_hdr->sh_info;
		  size *= sizeof (bfd_vma);
		  local_got_offsets = (bfd_vma *) bfd_alloc (abfd, size);
		  if (local_got_offsets == NULL)
		    return false;
		  elf_local_got_offsets (abfd) = local_got_offsets;
		  for (i = 0; i < symtab_hdr->sh_info; i++)
		    local_got_offsets[i] = (bfd_vma) -1;
		}
	      if (local_got_offsets[r_symndx] != (bfd_vma) -1)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      local_got_offsets[r_symndx] = sgot->_raw_size;

	      if (info->shared)
		{
		  /* If we are generating a shared object, we need to
                     output a R_SPARC_RELATIVE reloc so that the
                     dynamic linker can adjust this GOT entry.  */
		  srelgot->_raw_size += sizeof (Elf32_External_Rela);
		}
	    }

	  sgot->_raw_size += 4;

	  /* If the .got section is more than 0x1000 bytes, we add
	     0x1000 to the value of _GLOBAL_OFFSET_TABLE_, so that 13
	     bit relocations have a greater chance of working.  */
	  if (sgot->_raw_size >= 0x1000
	      && elf_hash_table (info)->hgot->root.u.def.value == 0)
	    elf_hash_table (info)->hgot->root.u.def.value = 0x1000;

	  break;

	case R_SPARC_PLT32:
	case R_SPARC_WPLT30:
	  /* This symbol requires a procedure linkage table entry.  We
             actually build the entry in adjust_dynamic_symbol,
             because this might be a case of linking PIC code without
             linking in any dynamic objects, in which case we don't
             need to generate a procedure linkage table after all.  */

	  if (h == NULL)
	    {
	      /* The Solaris native assembler will generate a WPLT30
                 reloc for a local symbol if you assemble a call from
                 one section to another when using -K pic.  We treat
                 it as WDISP30.  */
	      if (ELF32_R_TYPE (rel->r_info) != R_SPARC_WPLT30)
		goto r_sparc_plt32;
	      break;
	    }

	  /* Make sure this symbol is output as a dynamic symbol.  */
	  if (h->dynindx == -1)
	    {
	      if (! bfd_elf32_link_record_dynamic_symbol (info, h))
		return false;
	    }

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;

	  if (ELF32_R_TYPE (rel->r_info) != R_SPARC_WPLT30)
	    goto r_sparc_plt32;
	  break;

	case R_SPARC_PC10:
	case R_SPARC_PC22:
	  if (h != NULL)
	    h->elf_link_hash_flags |= ELF_LINK_NON_GOT_REF;

	  if (h != NULL
	      && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* Fall through.  */
	case R_SPARC_DISP8:
	case R_SPARC_DISP16:
	case R_SPARC_DISP32:
	case R_SPARC_WDISP30:
	case R_SPARC_WDISP22:
	case R_SPARC_WDISP19:
	case R_SPARC_WDISP16:
	  if (h != NULL)
	    h->elf_link_hash_flags |= ELF_LINK_NON_GOT_REF;

	  /* If we are linking with -Bsymbolic, we do not need to copy
             a PC relative reloc against a global symbol which is
             defined in an object we are including in the link (i.e.,
             DEF_REGULAR is set).  FIXME: At this point we have not
             seen all the input files, so it is possible that
             DEF_REGULAR is not set now but will be set later (it is
             never cleared).  This needs to be handled as in
             elf32-i386.c.  */
	  if (h == NULL
	      || (info->symbolic
		  && (h->elf_link_hash_flags
		      & ELF_LINK_HASH_DEF_REGULAR) != 0))
	    break;
	  /* Fall through.  */
	case R_SPARC_8:
	case R_SPARC_16:
	case R_SPARC_32:
	case R_SPARC_HI22:
	case R_SPARC_22:
	case R_SPARC_13:
	case R_SPARC_LO10:
	case R_SPARC_UA16:
	case R_SPARC_UA32:
	  if (h != NULL)
	    h->elf_link_hash_flags |= ELF_LINK_NON_GOT_REF;

	r_sparc_plt32:
	  if (info->shared && (sec->flags & SEC_ALLOC))
	    {
	      /* When creating a shared object, we must copy these
                 relocs into the output file.  We create a reloc
                 section in dynobj and make room for the reloc.  */
	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (abfd,
			   elf_elfheader (abfd)->e_shstrndx,
			   elf_section_data (sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return false;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (abfd, sec),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      flagword flags;

		      sreloc = bfd_make_section (dynobj, name);
		      flags = (SEC_HAS_CONTENTS | SEC_READONLY
			       | SEC_IN_MEMORY | SEC_LINKER_CREATED);
		      if ((sec->flags & SEC_ALLOC) != 0)
			flags |= SEC_ALLOC | SEC_LOAD;
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (dynobj, sreloc, flags)
			  || ! bfd_set_section_alignment (dynobj, sreloc, 2))
			return false;
		    }
		  if (sec->flags & SEC_READONLY)
		    info->flags |= DF_TEXTREL;
		}

	      sreloc->_raw_size += sizeof (Elf32_External_Rela);
	    }

	  break;

        case R_SPARC_GNU_VTINHERIT:
          if (!_bfd_elf32_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return false;
          break;

        case R_SPARC_GNU_VTENTRY:
          if (!_bfd_elf32_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return false;
          break;

	default:
	  break;
	}
    }

  return true;
}

static asection *
elf32_sparc_gc_mark_hook (sec, info, rel, h, sym)
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
      case R_SPARC_GNU_VTINHERIT:
      case R_SPARC_GNU_VTENTRY:
        break;

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

/* Update the got entry reference counts for the section being removed.  */
static boolean
elf32_sparc_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{

  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;
  unsigned long r_symndx;
  struct elf_link_hash_entry *h;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    switch (ELF32_R_TYPE (rel->r_info))
      {
      case R_SPARC_GOT10:
      case R_SPARC_GOT13:
      case R_SPARC_GOT22:
	r_symndx = ELF32_R_SYM (rel->r_info);
	if (r_symndx >= symtab_hdr->sh_info)
	  {
	    h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	    if (h->got.refcount > 0)
	      h->got.refcount--;
	  }
	else
	  {
	    if (local_got_refcounts[r_symndx] > 0)
	      local_got_refcounts[r_symndx]--;
	  }
        break;

      case R_SPARC_PLT32:
      case R_SPARC_HIPLT22:
      case R_SPARC_LOPLT10:
      case R_SPARC_PCPLT32:
      case R_SPARC_PCPLT10:
	r_symndx = ELF32_R_SYM (rel->r_info);
	if (r_symndx >= symtab_hdr->sh_info)
	  {
	    h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	    if (h->plt.refcount > 0)
	      h->plt.refcount--;
	  }
	break;

      default:
	break;
      }

  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static boolean
elf32_sparc_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  asection *s;
  unsigned int power_of_two;

  dynobj = elf_hash_table (info)->dynobj;

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (dynobj != NULL
	      && ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT)
		  || h->weakdef != NULL
		  || ((h->elf_link_hash_flags
		       & ELF_LINK_HASH_DEF_DYNAMIC) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_REF_REGULAR) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)));

  /* If this is a function, put it in the procedure linkage table.  We
     will fill in the contents of the procedure linkage table later
     (although we could actually do it here).  The STT_NOTYPE
     condition is a hack specifically for the Oracle libraries
     delivered for Solaris; for some inexplicable reason, they define
     some of their functions as STT_NOTYPE when they really should be
     STT_FUNC.  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0
      || (h->type == STT_NOTYPE
	  && (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	  && (h->root.u.def.section->flags & SEC_CODE) != 0))
    {
      if (! elf_hash_table (info)->dynamic_sections_created
	  || ((!info->shared || info->symbolic || h->dynindx == -1)
	      && (h->elf_link_hash_flags
		  & ELF_LINK_HASH_DEF_REGULAR) != 0))
	{
	  /* This case can occur if we saw a WPLT30 reloc in an input
	     file, but none of the input files were dynamic objects.
	     Or, when linking the main application or a -Bsymbolic
	     shared library against PIC code.  Or when a global symbol
	     has been made private, e.g. via versioning.

	     In these cases we know what value the symbol will resolve
	     to, so we don't actually need to build a procedure linkage
	     table, and we can just do a WDISP30 reloc instead.  */

	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	  return true;
	}

      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);

      /* The first four entries in .plt are reserved.  */
      if (s->_raw_size == 0)
	s->_raw_size = 4 * PLT_ENTRY_SIZE;

      /* The procedure linkage table has a maximum size.  */
      if (s->_raw_size >= 0x400000)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

     /* If this symbol is not defined in a regular file, and we are
       not generating a shared library, then set the symbol to this
       location in the .plt.  This is required to make function
       pointers compare as equal between the normal executable and
       the shared library.  */
     if (! info->shared
	&& (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
      {
	h->root.u.def.section = s;
	h->root.u.def.value = s->_raw_size;
      }

      h->plt.offset = s->_raw_size;

      /* Make room for this entry.  */
      s->_raw_size += PLT_ENTRY_SIZE;

      /* We also need to make an entry in the .rela.plt section.  */

      s = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (s != NULL);
      s->_raw_size += sizeof (Elf32_External_Rela);

      return true;
    }

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      return true;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.  */
  if (info->shared)
    return true;

  /* If there are no references to this symbol that do not use the
     GOT, we don't need to generate a copy reloc.  */
  if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0)
    return true;

  /* We must allocate the symbol in our .dynbss section, which will
     become part of the .bss section of the executable.  There will be
     an entry for this symbol in the .dynsym section.  The dynamic
     object will contain position independent code, so all references
     from the dynamic object to this symbol will go through the global
     offset table.  The dynamic linker will use the .dynsym entry to
     determine the address it must put in the global offset table, so
     both the dynamic object and the regular object will refer to the
     same memory location for the variable.  */

  s = bfd_get_section_by_name (dynobj, ".dynbss");
  BFD_ASSERT (s != NULL);

  /* We must generate a R_SPARC_COPY reloc to tell the dynamic linker
     to copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rel.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      srel = bfd_get_section_by_name (dynobj, ".rela.bss");
      BFD_ASSERT (srel != NULL);
      srel->_raw_size += sizeof (Elf32_External_Rela);
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_COPY;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 3)
    power_of_two = 3;

  /* Apply the required alignment.  */
  s->_raw_size = BFD_ALIGN (s->_raw_size,
			    (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (dynobj, s))
    {
      if (! bfd_set_section_alignment (dynobj, s, power_of_two))
	return false;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return true;
}

/* Set the sizes of the dynamic sections.  */

static boolean
elf32_sparc_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean relplt;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}

      /* Make space for the trailing nop in .plt.  */
      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);
      if (s->_raw_size > 0)
	s->_raw_size += 4;
    }
  else
    {
      /* We may have created entries in the .rela.got section.
         However, if we are not creating the dynamic sections, we will
         not actually use these entries.  Reset the size of .rela.got,
         which will cause it to get stripped from the output file
         below.  */
      s = bfd_get_section_by_name (dynobj, ".rela.got");
      if (s != NULL)
	s->_raw_size = 0;
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  relplt = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      boolean strip;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      strip = false;

      if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* If we don't need this section, strip it from the
		 output file.  This is to handle .rela.bss and
		 .rel.plt.  We must create it in
		 create_dynamic_sections, because it must be created
		 before the linker maps input sections to output
		 sections.  The linker does that before
		 adjust_dynamic_symbol is called, and it is that
		 function which decides whether anything needs to go
		 into these sections.  */
	      strip = true;
	    }
	  else
	    {
	      if (strcmp (name, ".rela.plt") == 0)
		relplt = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strcmp (name, ".plt") != 0
	       && strcmp (name, ".got") != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	{
	  _bfd_strip_section_from_output (info, s);
	  continue;
	}

      /* Allocate memory for the section contents.  */
      /* FIXME: This should be a call to bfd_alloc not bfd_zalloc.
	 Unused entries should be reclaimed before the section's contents
	 are written out, but at the moment this does not happen.  Thus in
	 order to prevent writing out garbage, we initialise the section's
	 contents to zero.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return false;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf32_sparc_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  bfd_elf32_add_dynamic_entry (info, (bfd_vma) (TAG), (bfd_vma) (VAL))

      if (!info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return false;
	}

      if (relplt)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return false;
	}

      if (!add_dynamic_entry (DT_RELA, 0)
	  || !add_dynamic_entry (DT_RELASZ, 0)
	  || !add_dynamic_entry (DT_RELAENT, sizeof (Elf32_External_Rela)))
	return false;

      if (info->flags & DF_TEXTREL)
	{
	  if (!add_dynamic_entry (DT_TEXTREL, 0))
	    return false;
	}
    }
#undef add_dynamic_entry

  return true;
}

#define SET_SEC_DO_RELAX(section) do { elf_section_data(section)->tdata = (void *)1; } while (0)
#define SEC_DO_RELAX(section) (elf_section_data(section)->tdata == (void *)1)

static boolean
elf32_sparc_relax_section (abfd, section, link_info, again)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
     boolean *again;
{
  *again = false;
  SET_SEC_DO_RELAX (section);
  return true;
}

/* This is the condition under which finish_dynamic_symbol will be called
   from elflink.h.  If elflink.h doesn't call our finish_dynamic_symbol
   routine, we'll need to do something about initializing any .plt and .got
   entries in relocate_section.  */
#define WILL_CALL_FINISH_DYNAMIC_SYMBOL(DYN, INFO, H)			\
  ((DYN)								\
   && ((INFO)->shared							\
       || ((H)->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)	\
   && ((H)->dynindx != -1						\
       || ((H)->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0))

/* Relocate a SPARC ELF section.  */

static boolean
elf32_sparc_relocate_section (output_bfd, info, input_bfd, input_section,
			      contents, relocs, local_syms, local_sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  bfd_vma got_base;
  asection *sgot;
  asection *splt;
  asection *sreloc;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  local_got_offsets = elf_local_got_offsets (input_bfd);

  if (elf_hash_table (info)->hgot == NULL)
    got_base = 0;
  else
    got_base = elf_hash_table (info)->hgot->root.u.def.value;

  sgot = NULL;
  splt = NULL;
  sreloc = NULL;

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sec;
      bfd_vma relocation, off;
      bfd_reloc_status_type r;
      boolean is_plt = false;
      boolean unresolved_reloc;

      r_type = ELF32_R_TYPE (rel->r_info);

      if (r_type == R_SPARC_GNU_VTINHERIT
          || r_type == R_SPARC_GNU_VTENTRY)
        continue;

      if (r_type < 0 || r_type >= (int) R_SPARC_max_std)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = _bfd_sparc_elf_howto_table + r_type;

      r_symndx = ELF32_R_SYM (rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
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
      unresolved_reloc = false;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, sec, rel);
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  relocation = 0;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      if (sec->output_section == NULL)
		 /* Set a flag that will be cleared later if we find a
		   relocation value for this symbol.  output_section
		   is typically NULL for symbols satisfied by a shared
		   library.  */
		unresolved_reloc = true;
	      else
		relocation = (h->root.u.def.value
			      + sec->output_section->vma
			      + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    ;
	  else if (info->shared
		   && (!info->symbolic || info->allow_shlib_undefined)
		   && !info->no_undefined
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    ;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset,
		      (!info->shared || info->no_undefined
		       || ELF_ST_VISIBILITY (h->other)))))
		return false;
	    }
	}

      switch (r_type)
	{
	case R_SPARC_GOT10:
	case R_SPARC_GOT13:
	case R_SPARC_GOT22:
	  /* Relocation is to the entry for this symbol in the global
             offset table.  */
	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  if (h != NULL)
	    {
	      boolean dyn;

	      off = h->got.offset;
	      BFD_ASSERT (off != (bfd_vma) -1);
	      dyn = elf_hash_table (info)->dynamic_sections_created;

	      if (! WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info, h)
		  || (info->shared
		      && (info->symbolic
			  || h->dynindx == -1
			  || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL))
		      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
		{
		  /* This is actually a static link, or it is a
                     -Bsymbolic link and the symbol is defined
                     locally, or the symbol was forced to be local
                     because of a version file.  We must initialize
                     this entry in the global offset table.  Since the
                     offset must always be a multiple of 4, we use the
                     least significant bit to record whether we have
                     initialized it already.

		     When doing a dynamic link, we create a .rela.got
		     relocation entry to initialize the value.  This
		     is done in the finish_dynamic_symbol routine.  */
		  if ((off & 1) != 0)
		    off &= ~1;
		  else
		    {
		      bfd_put_32 (output_bfd, relocation,
				  sgot->contents + off);
		      h->got.offset |= 1;
		    }
		}
	      else
		unresolved_reloc = false;
	    }
	  else
	    {
	      BFD_ASSERT (local_got_offsets != NULL
			  && local_got_offsets[r_symndx] != (bfd_vma) -1);

	      off = local_got_offsets[r_symndx];

	      /* The offset must always be a multiple of 4.  We use
		 the least significant bit to record whether we have
		 already processed this entry.  */
	      if ((off & 1) != 0)
		off &= ~1;
	      else
		{
		  bfd_put_32 (output_bfd, relocation, sgot->contents + off);

		  if (info->shared)
		    {
		      asection *srelgot;
		      Elf_Internal_Rela outrel;

		      /* We need to generate a R_SPARC_RELATIVE reloc
			 for the dynamic linker.  */
		      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
		      BFD_ASSERT (srelgot != NULL);

		      outrel.r_offset = (sgot->output_section->vma
					 + sgot->output_offset
					 + off);
		      outrel.r_info = ELF32_R_INFO (0, R_SPARC_RELATIVE);
		      outrel.r_addend = 0;
		      bfd_elf32_swap_reloca_out (output_bfd, &outrel,
						 (((Elf32_External_Rela *)
						   srelgot->contents)
						  + srelgot->reloc_count));
		      ++srelgot->reloc_count;
		    }

		  local_got_offsets[r_symndx] |= 1;
		}
	    }
	  relocation = sgot->output_offset + off - got_base;
	  break;

	case R_SPARC_PLT32:
	  if (h == NULL || h->plt.offset == (bfd_vma) -1)
	    {
	      r_type = R_SPARC_32;
	      goto r_sparc_plt32;
	    }
	  /* Fall through.  */
	case R_SPARC_WPLT30:
	  /* Relocation is to the entry for this symbol in the
             procedure linkage table.  */

	  /* The Solaris native assembler will generate a WPLT30 reloc
	     for a local symbol if you assemble a call from one
	     section to another when using -K pic.  We treat it as
	     WDISP30.  */
	  if (h == NULL)
	    break;

	  if (h->plt.offset == (bfd_vma) -1)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
                 happens when statically linking PIC code, or when
                 using -Bsymbolic.  */
	      break;
	    }

	  if (splt == NULL)
	    {
	      splt = bfd_get_section_by_name (dynobj, ".plt");
	      BFD_ASSERT (splt != NULL);
	    }

	  relocation = (splt->output_section->vma
			+ splt->output_offset
			+ h->plt.offset);
	  unresolved_reloc = false;
	  if (r_type == R_SPARC_PLT32)
	    {
	      r_type = R_SPARC_32;
	      is_plt = true;
	      goto r_sparc_plt32;
	    }
	  break;

	case R_SPARC_PC10:
	case R_SPARC_PC22:
	  if (h != NULL
	      && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* Fall through.  */
	case R_SPARC_DISP8:
	case R_SPARC_DISP16:
	case R_SPARC_DISP32:
	case R_SPARC_WDISP30:
	case R_SPARC_WDISP22:
	case R_SPARC_WDISP19:
	case R_SPARC_WDISP16:
	  if (h == NULL
	      || (info->symbolic
		  && (h->elf_link_hash_flags
		      & ELF_LINK_HASH_DEF_REGULAR) != 0))
	    break;
	  /* Fall through.  */
	case R_SPARC_8:
	case R_SPARC_16:
	case R_SPARC_32:
	case R_SPARC_HI22:
	case R_SPARC_22:
	case R_SPARC_13:
	case R_SPARC_LO10:
	case R_SPARC_UA16:
	case R_SPARC_UA32:
	r_sparc_plt32:
	  if (info->shared
	      && r_symndx != 0
	      && (input_section->flags & SEC_ALLOC))
	    {
	      Elf_Internal_Rela outrel;
	      boolean skip, relocate = false;

	      /* When generating a shared object, these relocations
                 are copied into the output file to be resolved at run
                 time.  */

	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (input_bfd,
			   elf_elfheader (input_bfd)->e_shstrndx,
			   elf_section_data (input_section)->rel_hdr.sh_name));
		  if (name == NULL)
		    return false;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (input_bfd,
							       input_section),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  BFD_ASSERT (sreloc != NULL);
		}

	      skip = false;

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info, input_section,
					 rel->r_offset);
	      if (outrel.r_offset == (bfd_vma) -1)
		skip = true;
	      else if (outrel.r_offset == (bfd_vma) -2)
		skip = true, relocate = true;
	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      /* Optimize unaligned reloc usage now that we know where
		 it finally resides.  */
	      switch (r_type)
		{
		case R_SPARC_16:
		  if (outrel.r_offset & 1)
		    r_type = R_SPARC_UA16;
		  break;
		case R_SPARC_UA16:
		  if (!(outrel.r_offset & 1))
		    r_type = R_SPARC_16;
		  break;
		case R_SPARC_32:
		  if (outrel.r_offset & 3)
		    r_type = R_SPARC_UA32;
		  break;
		case R_SPARC_UA32:
		  if (!(outrel.r_offset & 3))
		    r_type = R_SPARC_32;
		  break;
	  	case R_SPARC_DISP8:
		case R_SPARC_DISP16:
	  	case R_SPARC_DISP32:
		  /* If the symbol is not dynamic, we should not keep
		     a dynamic relocation.  But an .rela.* slot has been
		     allocated for it, output R_SPARC_NONE.
		     FIXME: Add code tracking needed dynamic relocs as
		     e.g. i386 has.  */
		  if (h->dynindx == -1)
		    skip = true, relocate = true;
		  break;
		}

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      /* h->dynindx may be -1 if the symbol was marked to
                 become local.  */
	      else if (h != NULL && ! is_plt
		       && ((! info->symbolic && h->dynindx != -1)
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))
		{
		  BFD_ASSERT (h->dynindx != -1);
		  outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = rel->r_addend;
		}
	      else
		{
		  if (r_type == R_SPARC_32)
		    {
		      outrel.r_info = ELF32_R_INFO (0, R_SPARC_RELATIVE);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		  else
		    {
		      long indx;

		      if (is_plt)
			sec = splt;
		      else if (h == NULL)
			sec = local_sections[r_symndx];
		      else
			{
			  BFD_ASSERT (h->root.type == bfd_link_hash_defined
				      || (h->root.type
					  == bfd_link_hash_defweak));
			  sec = h->root.u.def.section;
			}
		      if (sec != NULL && bfd_is_abs_section (sec))
			indx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return false;
			}
		      else
			{
			  asection *osec;

			  osec = sec->output_section;
			  indx = elf_section_data (osec)->dynindx;

			  /* FIXME: we really should be able to link non-pic
			     shared libraries.  */
			  if (indx == 0)
			    {
			      BFD_FAIL ();
			      (*_bfd_error_handler)
				(_("%s: probably compiled without -fPIC?"),
				 bfd_archive_filename (input_bfd));
			      bfd_set_error (bfd_error_bad_value);
			      return false;
			    }
			}

		      outrel.r_info = ELF32_R_INFO (indx, r_type);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		}

	      bfd_elf32_swap_reloca_out (output_bfd, &outrel,
					 (((Elf32_External_Rela *)
					   sreloc->contents)
					  + sreloc->reloc_count));
	      ++sreloc->reloc_count;

	      /* This reloc will be computed at runtime, so there's no
                 need to do anything now.  */
	      if (! relocate)
		continue;
	    }
	  break;

	default:
	  break;
	}

      /* Dynamic relocs are not propagated for SEC_DEBUGGING sections
	 because such sections are not SEC_ALLOC and thus ld.so will
	 not process them.  */
      if (unresolved_reloc
	  && !((input_section->flags & SEC_DEBUGGING) != 0
	       && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0))
	(*_bfd_error_handler)
	  (_("%s(%s+0x%lx): unresolvable relocation against symbol `%s'"),
	   bfd_archive_filename (input_bfd),
	   bfd_get_section_name (input_bfd, input_section),
	   (long) rel->r_offset,
	   h->root.root.string);

      r = bfd_reloc_continue;
      if (r_type == R_SPARC_WDISP16)
	{
	  bfd_vma x;

	  relocation += rel->r_addend;
	  relocation -= (input_section->output_section->vma
			 + input_section->output_offset);
	  relocation -= rel->r_offset;

	  x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	  x |= ((((relocation >> 2) & 0xc000) << 6)
		| ((relocation >> 2) & 0x3fff));
	  bfd_put_32 (input_bfd, x, contents + rel->r_offset);

	  if ((bfd_signed_vma) relocation < - 0x40000
	      || (bfd_signed_vma) relocation > 0x3ffff)
	    r = bfd_reloc_overflow;
	  else
	    r = bfd_reloc_ok;
	}
      else if (r_type == R_SPARC_REV32)
	{
	  bfd_vma x;

	  relocation = relocation + rel->r_addend;

	  x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	  x = x + relocation;
	  bfd_putl32 (/*input_bfd,*/ x, contents + rel->r_offset);
	  r = bfd_reloc_ok;
	}
      else if ((r_type == R_SPARC_WDISP30 || r_type == R_SPARC_WPLT30)
	       && SEC_DO_RELAX (input_section)
	       && rel->r_offset + 4 < input_section->_raw_size)
	{
#define G0		0
#define O7		15
#define XCC		(2 << 20)
#define COND(x)		(((x)&0xf)<<25)
#define CONDA		COND(0x8)
#define INSN_BPA	(F2(0,1) | CONDA | BPRED | XCC)
#define INSN_BA		(F2(0,2) | CONDA)
#define INSN_OR		F3(2, 0x2, 0)
#define INSN_NOP	F2(0,4)

	  bfd_vma x, y;

	  /* If the instruction is a call with either:
	     restore
	     arithmetic instruction with rd == %o7
	     where rs1 != %o7 and rs2 if it is register != %o7
	     then we can optimize if the call destination is near
	     by changing the call into a branch always.  */
	  x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	  y = bfd_get_32 (input_bfd, contents + rel->r_offset + 4);
	  if ((x & OP(~0)) == OP(1) && (y & OP(~0)) == OP(2))
	    {
	      if (((y & OP3(~0)) == OP3(0x3d) /* restore */
		   || ((y & OP3(0x28)) == 0 /* arithmetic */
		       && (y & RD(~0)) == RD(O7)))
		  && (y & RS1(~0)) != RS1(O7)
		  && ((y & F3I(~0))
		      || (y & RS2(~0)) != RS2(O7)))
		{
		  bfd_vma reloc;

		  reloc = relocation + rel->r_addend - rel->r_offset;
		  reloc -= (input_section->output_section->vma
			   + input_section->output_offset);

		  /* Ensure the reloc fits into simm22.  */
		  if ((reloc & 3) == 0
		      && ((reloc & ~(bfd_vma)0x7fffff) == 0
			  || ((reloc | 0x7fffff) == ~(bfd_vma)0)))
		    {
		      reloc >>= 2;

		      /* Check whether it fits into simm19 on v9.  */
		      if (((reloc & 0x3c0000) == 0
			   || (reloc & 0x3c0000) == 0x3c0000)
			  && (elf_elfheader (output_bfd)->e_flags & EF_SPARC_32PLUS))
			x = INSN_BPA | (reloc & 0x7ffff); /* ba,pt %xcc */
		      else
			x = INSN_BA | (reloc & 0x3fffff); /* ba */
		      bfd_put_32 (input_bfd, x, contents + rel->r_offset);
		      r = bfd_reloc_ok;
		      if (rel->r_offset >= 4
			  && (y & (0xffffffff ^ RS1(~0)))
			      == (INSN_OR | RD(O7) | RS2(G0)))
			{
			  bfd_vma z;
			  unsigned int reg;

			  z = bfd_get_32 (input_bfd,
					  contents + rel->r_offset - 4);
			  if ((z & (0xffffffff ^ RD(~0)))
			      != (INSN_OR | RS1(O7) | RS2(G0)))
			    break;

			  /* The sequence was
			     or %o7, %g0, %rN
			     call foo
			     or %rN, %g0, %o7

			     If call foo was replaced with ba, replace
			     or %rN, %g0, %o7 with nop.  */

			  reg = (y & RS1(~0)) >> 14;
			  if (reg != ((z & RD(~0)) >> 25)
			      || reg == G0 || reg == O7)
			    break;

			  bfd_put_32 (input_bfd, (bfd_vma) INSN_NOP,
				      contents + rel->r_offset + 4);
			}

		    }
		}
	    }
	}

      if (r == bfd_reloc_continue)
	r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				      contents, rel->r_offset,
				      relocation, rel->r_addend);

      if (r != bfd_reloc_ok)
	{
	  switch (r)
	    {
	    default:
	    case bfd_reloc_outofrange:
	      abort ();
	    case bfd_reloc_overflow:
	      {
		const char *name;

		if (h != NULL)
		  name = h->root.root.string;
		else
		  {
		    name = bfd_elf_string_from_elf_section (input_bfd,
							    symtab_hdr->sh_link,
							    sym->st_name);
		    if (name == NULL)
		      return false;
		    if (*name == '\0')
		      name = bfd_section_name (input_bfd, sec);
		  }
		if (! ((*info->callbacks->reloc_overflow)
		       (info, name, howto->name, (bfd_vma) 0,
			input_bfd, input_section, rel->r_offset)))
		  return false;
	      }
	      break;
	    }
	}
    }

  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
elf32_sparc_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;

  dynobj = elf_hash_table (info)->dynobj;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *srela;
      Elf_Internal_Rela rela;

      /* This symbol has an entry in the procedure linkage table.  Set
         it up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = bfd_get_section_by_name (dynobj, ".plt");
      srela = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (splt != NULL && srela != NULL);

      /* Fill in the entry in the procedure linkage table.  */
      bfd_put_32 (output_bfd,
		  PLT_ENTRY_WORD0 + h->plt.offset,
		  splt->contents + h->plt.offset);
      bfd_put_32 (output_bfd,
		  (PLT_ENTRY_WORD1
		   + (((- (h->plt.offset + 4)) >> 2) & 0x3fffff)),
		  splt->contents + h->plt.offset + 4);
      bfd_put_32 (output_bfd, (bfd_vma) PLT_ENTRY_WORD2,
		  splt->contents + h->plt.offset + 8);

      /* Fill in the entry in the .rela.plt section.  */
      rela.r_offset = (splt->output_section->vma
		       + splt->output_offset
		       + h->plt.offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_SPARC_JMP_SLOT);
      rela.r_addend = 0;
      bfd_elf32_swap_reloca_out (output_bfd, &rela,
				 ((Elf32_External_Rela *) srela->contents
				  + h->plt.offset / PLT_ENTRY_SIZE - 4));

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	  /* If the symbol is weak, we do need to clear the value.
	     Otherwise, the PLT entry would provide a definition for
	     the symbol even if the symbol wasn't defined anywhere,
	     and so the symbol would never be NULL.  */
	  if ((h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR_NONWEAK)
	      == 0)
	    sym->st_value = 0;
	}
    }

  if (h->got.offset != (bfd_vma) -1)
    {
      asection *sgot;
      asection *srela;
      Elf_Internal_Rela rela;

      /* This symbol has an entry in the global offset table.  Set it
         up.  */

      sgot = bfd_get_section_by_name (dynobj, ".got");
      srela = bfd_get_section_by_name (dynobj, ".rela.got");
      BFD_ASSERT (sgot != NULL && srela != NULL);

      rela.r_offset = (sgot->output_section->vma
		       + sgot->output_offset
		       + (h->got.offset &~ (bfd_vma) 1));

      /* If this is a -Bsymbolic link, and the symbol is defined
	 locally, we just want to emit a RELATIVE reloc.  Likewise if
	 the symbol was forced to be local because of a version file.
	 The entry in the global offset table will already have been
	 initialized in the relocate_section function.  */
      if (info->shared
	  && (info->symbolic || h->dynindx == -1)
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	rela.r_info = ELF32_R_INFO (0, R_SPARC_RELATIVE);
      else
	{
	  bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + h->got.offset);
	  rela.r_info = ELF32_R_INFO (h->dynindx, R_SPARC_GLOB_DAT);
	}

      rela.r_addend = 0;
      bfd_elf32_swap_reloca_out (output_bfd, &rela,
				 ((Elf32_External_Rela *) srela->contents
				  + srela->reloc_count));
      ++srela->reloc_count;
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      asection *s;
      Elf_Internal_Rela rela;

      /* This symbols needs a copy reloc.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1);

      s = bfd_get_section_by_name (h->root.u.def.section->owner,
				   ".rela.bss");
      BFD_ASSERT (s != NULL);

      rela.r_offset = (h->root.u.def.value
		       + h->root.u.def.section->output_section->vma
		       + h->root.u.def.section->output_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_SPARC_COPY);
      rela.r_addend = 0;
      bfd_elf32_swap_reloca_out (output_bfd, &rela,
				 ((Elf32_External_Rela *) s->contents
				  + s->reloc_count));
      ++s->reloc_count;
    }

  /* Mark some specially defined symbols as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0
      || strcmp (h->root.root.string, "_PROCEDURE_LINKAGE_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return true;
}

/* Finish up the dynamic sections.  */

static boolean
elf32_sparc_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;
  asection *sgot;

  dynobj = elf_hash_table (info)->dynobj;

  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      Elf32_External_Dyn *dyncon, *dynconend;

      splt = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (splt != NULL && sdyn != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  boolean size;

	  bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:   name = ".plt"; size = false; break;
	    case DT_PLTRELSZ: name = ".rela.plt"; size = true; break;
	    case DT_JMPREL:   name = ".rela.plt"; size = false; break;
	    default:	  name = NULL; size = false; break;
	    }

	  if (name != NULL)
	    {
	      asection *s;

	      s = bfd_get_section_by_name (output_bfd, name);
	      if (s == NULL)
		dyn.d_un.d_val = 0;
	      else
		{
		  if (! size)
		    dyn.d_un.d_ptr = s->vma;
		  else
		    {
		      if (s->_cooked_size != 0)
			dyn.d_un.d_val = s->_cooked_size;
		      else
			dyn.d_un.d_val = s->_raw_size;
		    }
		}
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	    }
	}

      /* Clear the first four entries in the procedure linkage table,
	 and put a nop in the last four bytes.  */
      if (splt->_raw_size > 0)
	{
	  memset (splt->contents, 0, 4 * PLT_ENTRY_SIZE);
	  bfd_put_32 (output_bfd, (bfd_vma) SPARC_NOP,
		      splt->contents + splt->_raw_size - 4);
	}

      elf_section_data (splt->output_section)->this_hdr.sh_entsize =
	PLT_ENTRY_SIZE;
    }

  /* Set the first entry in the global offset table to the address of
     the dynamic section.  */
  sgot = bfd_get_section_by_name (dynobj, ".got");
  BFD_ASSERT (sgot != NULL);
  if (sgot->_raw_size > 0)
    {
      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);
    }

  elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;

  return true;
}

/* Functions for dealing with the e_flags field.

   We don't define set_private_flags or copy_private_bfd_data because
   the only currently defined values are based on the bfd mach number,
   so we use the latter instead and defer setting e_flags until the
   file is written out.  */

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static boolean
elf32_sparc_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  boolean error;
  /* FIXME: This should not be static.  */
  static unsigned long previous_ibfd_e_flags = (unsigned long) -1;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  error = false;

  if (bfd_get_mach (ibfd) >= bfd_mach_sparc_v9)
    {
      error = true;
      (*_bfd_error_handler)
	(_("%s: compiled for a 64 bit system and target is 32 bit"),
	 bfd_archive_filename (ibfd));
    }
  else if ((ibfd->flags & DYNAMIC) == 0)
    {
      if (bfd_get_mach (obfd) < bfd_get_mach (ibfd))
	bfd_set_arch_mach (obfd, bfd_arch_sparc, bfd_get_mach (ibfd));
    }

  if (((elf_elfheader (ibfd)->e_flags & EF_SPARC_LEDATA)
       != previous_ibfd_e_flags)
      && previous_ibfd_e_flags != (unsigned long) -1)
    {
      (*_bfd_error_handler)
	(_("%s: linking little endian files with big endian files"),
	 bfd_archive_filename (ibfd));
      error = true;
    }
  previous_ibfd_e_flags = elf_elfheader (ibfd)->e_flags & EF_SPARC_LEDATA;

  if (error)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  return true;
}

/* Set the right machine number.  */

static boolean
elf32_sparc_object_p (abfd)
     bfd *abfd;
{
  if (elf_elfheader (abfd)->e_machine == EM_SPARC32PLUS)
    {
      if (elf_elfheader (abfd)->e_flags & EF_SPARC_SUN_US3)
	return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
					  bfd_mach_sparc_v8plusb);
      else if (elf_elfheader (abfd)->e_flags & EF_SPARC_SUN_US1)
	return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
					  bfd_mach_sparc_v8plusa);
      else if (elf_elfheader (abfd)->e_flags & EF_SPARC_32PLUS)
	return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
					  bfd_mach_sparc_v8plus);
      else
	return false;
    }
  else if (elf_elfheader (abfd)->e_flags & EF_SPARC_LEDATA)
    return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
				      bfd_mach_sparc_sparclite_le);
  else
    return bfd_default_set_arch_mach (abfd, bfd_arch_sparc, bfd_mach_sparc);
}

/* The final processing done just before writing out the object file.
   We need to set the e_machine field appropriately.  */

static void
elf32_sparc_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker ATTRIBUTE_UNUSED;
{
  switch (bfd_get_mach (abfd))
    {
    case bfd_mach_sparc :
      break; /* nothing to do */
    case bfd_mach_sparc_v8plus :
      elf_elfheader (abfd)->e_machine = EM_SPARC32PLUS;
      elf_elfheader (abfd)->e_flags &=~ EF_SPARC_32PLUS_MASK;
      elf_elfheader (abfd)->e_flags |= EF_SPARC_32PLUS;
      break;
    case bfd_mach_sparc_v8plusa :
      elf_elfheader (abfd)->e_machine = EM_SPARC32PLUS;
      elf_elfheader (abfd)->e_flags &=~ EF_SPARC_32PLUS_MASK;
      elf_elfheader (abfd)->e_flags |= EF_SPARC_32PLUS | EF_SPARC_SUN_US1;
      break;
    case bfd_mach_sparc_v8plusb :
      elf_elfheader (abfd)->e_machine = EM_SPARC32PLUS;
      elf_elfheader (abfd)->e_flags &=~ EF_SPARC_32PLUS_MASK;
      elf_elfheader (abfd)->e_flags |= EF_SPARC_32PLUS | EF_SPARC_SUN_US1
				       | EF_SPARC_SUN_US3;
      break;
    case bfd_mach_sparc_sparclite_le :
      elf_elfheader (abfd)->e_machine = EM_SPARC;
      elf_elfheader (abfd)->e_flags |= EF_SPARC_LEDATA;
      break;
    default :
      abort ();
      break;
    }
}

static enum elf_reloc_type_class
elf32_sparc_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  switch ((int) ELF32_R_TYPE (rela->r_info))
    {
    case R_SPARC_RELATIVE:
      return reloc_class_relative;
    case R_SPARC_JMP_SLOT:
      return reloc_class_plt;
    case R_SPARC_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

#define TARGET_BIG_SYM	bfd_elf32_sparc_vec
#define TARGET_BIG_NAME	"elf32-sparc"
#define ELF_ARCH	bfd_arch_sparc
#define ELF_MACHINE_CODE EM_SPARC
#define ELF_MACHINE_ALT1 EM_SPARC32PLUS
#define ELF_MAXPAGESIZE 0x10000

#define bfd_elf32_bfd_reloc_type_lookup	elf32_sparc_reloc_type_lookup
#define bfd_elf32_bfd_relax_section	elf32_sparc_relax_section
#define elf_info_to_howto		elf32_sparc_info_to_howto
#define elf_backend_create_dynamic_sections \
					_bfd_elf_create_dynamic_sections
#define elf_backend_check_relocs	elf32_sparc_check_relocs
#define elf_backend_adjust_dynamic_symbol \
					elf32_sparc_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
					elf32_sparc_size_dynamic_sections
#define elf_backend_relocate_section	elf32_sparc_relocate_section
#define elf_backend_finish_dynamic_symbol \
					elf32_sparc_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					elf32_sparc_finish_dynamic_sections
#define bfd_elf32_bfd_merge_private_bfd_data \
					elf32_sparc_merge_private_bfd_data
#define elf_backend_object_p		elf32_sparc_object_p
#define elf_backend_final_write_processing \
					elf32_sparc_final_write_processing
#define elf_backend_gc_mark_hook        elf32_sparc_gc_mark_hook
#define elf_backend_gc_sweep_hook       elf32_sparc_gc_sweep_hook
#define elf_backend_reloc_type_class	elf32_sparc_reloc_type_class

#define elf_backend_can_gc_sections 1
#define elf_backend_want_got_plt 0
#define elf_backend_plt_readonly 0
#define elf_backend_want_plt_sym 1
#define elf_backend_got_header_size 4
#define elf_backend_plt_header_size (4*PLT_ENTRY_SIZE)

#include "elf32-target.h"
