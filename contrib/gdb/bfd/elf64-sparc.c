/* SPARC-specific support for 64-bit ELF
   Copyright (C) 1993, 1995, 1996 Free Software Foundation, Inc.

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

/* We need a published ABI spec for this.  Until one comes out, don't
   assume this'll remain unchanged forever.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"

#define SPARC64_OLD_RELOCS
#include "elf/sparc.h"

static reloc_howto_type *sparc64_elf_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void elf_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));

static boolean sparc64_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static boolean sparc64_elf_object_p PARAMS ((bfd *));

/* The howto table and associated functions.
   ??? Some of the relocation values have changed.  Until we're ready
   to upgrade, we have our own copy.  At some point a non upward compatible
   change will be made at which point this table can be deleted and we'll
   use the one in elf32-sparc.c.  */

static bfd_reloc_status_type sparc_elf_wdisp16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

static reloc_howto_type sparc64_elf_howto_table[] = 
{
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_8,         0,0, 8,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_8",       false,0,0x000000ff,true),
  HOWTO(R_SPARC_16,        0,1,16,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_16",      false,0,0x0000ffff,true),
  HOWTO(R_SPARC_32,        0,2,32,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_32",      false,0,0xffffffff,true),
  HOWTO(R_SPARC_DISP8,     0,0, 8,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP8",   false,0,0x000000ff,true),
  HOWTO(R_SPARC_DISP16,    0,1,16,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP16",  false,0,0x0000ffff,true),
  HOWTO(R_SPARC_DISP32,    0,2,32,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP32",  false,0,0x00ffffff,true),
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
  HOWTO(R_SPARC_UA32,      0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_UA32",    false,0,0x00000000,true),
#if 0 /* not used yet */
  HOWTO(R_SPARC_PLT32,     0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PLT32",    false,0,0x00000000,true),
  HOWTO(R_SPARC_HIPLT22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HIPLT22",  false,0,0x00000000,true),
  HOWTO(R_SPARC_LOPLT10,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_LOPLT10",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT32,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT32",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT22",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT10,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT10",  false,0,0x00000000,true),
#endif
  HOWTO(R_SPARC_10,        0,2,10,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_10",      false,0,0x000003ff,true),
  HOWTO(R_SPARC_11,        0,2,11,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_11",      false,0,0x000007ff,true),
  HOWTO(R_SPARC_64,        0,4,00,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_64",      false,0,~ (bfd_vma) 0, true),
  HOWTO(R_SPARC_OLO10,     0,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_OLO10",   false,0,0x000003ff,true),
  HOWTO(R_SPARC_HH22,     42,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HH22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_HM10,     32,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HM10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_LM22,     10,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_LM22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC_HH22,  42,2,22,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HH22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC_HM10,  32,2,10,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HM10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_PC_LM22,  10,2,22,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_LM22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_WDISP16,   2,2,16,true, 0,complain_overflow_signed,  sparc_elf_wdisp16_reloc,"R_SPARC_WDISP16", false,0,0x00000000,true),
  HOWTO(R_SPARC_WDISP19,   2,2,22,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP19", false,0,0x0007ffff,true),
  HOWTO(R_SPARC_GLOB_JMP,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GLOB_DAT",false,0,0x00000000,true),
  HOWTO(R_SPARC_7,         0,2, 7,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_7",       false,0,0x0000007f,true),
#if 0 /* not used yet */
  HOWTO(R_SPARC_5,         0,2, 5,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_5",       false,0,0x0000001f,true),
  HOWTO(R_SPARC_6,         0,2, 6,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_6",       false,0,0x0000003f,true),
#endif
};

struct elf_reloc_map {
  unsigned char bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static CONST struct elf_reloc_map sparc_reloc_map[] =
{
  { BFD_RELOC_NONE, R_SPARC_NONE, },
  { BFD_RELOC_16, R_SPARC_16, },
  { BFD_RELOC_8, R_SPARC_8 },
  { BFD_RELOC_8_PCREL, R_SPARC_DISP8 },
  /* ??? This might cause us to need separate functions in elf{32,64}-sparc.c
     (we could still have just one table), but is this reloc ever used?  */
  { BFD_RELOC_CTOR, R_SPARC_32 }, /* @@ Assumes 32 bits.  */
  { BFD_RELOC_32, R_SPARC_32 },
  { BFD_RELOC_32_PCREL, R_SPARC_DISP32 },
  { BFD_RELOC_HI22, R_SPARC_HI22 },
  { BFD_RELOC_LO10, R_SPARC_LO10, },
  { BFD_RELOC_32_PCREL_S2, R_SPARC_WDISP30 },
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
  /* ??? Doesn't dwarf use this?  */
/*{ BFD_RELOC_SPARC_UA32, R_SPARC_UA32 }, not used?? */
  {BFD_RELOC_SPARC_10, R_SPARC_10},
  {BFD_RELOC_SPARC_11, R_SPARC_11},
  {BFD_RELOC_SPARC_64, R_SPARC_64},
  {BFD_RELOC_SPARC_OLO10, R_SPARC_OLO10},
  {BFD_RELOC_SPARC_HH22, R_SPARC_HH22},
  {BFD_RELOC_SPARC_HM10, R_SPARC_HM10},
  {BFD_RELOC_SPARC_LM22, R_SPARC_LM22},
  {BFD_RELOC_SPARC_PC_HH22, R_SPARC_PC_HH22},
  {BFD_RELOC_SPARC_PC_HM10, R_SPARC_PC_HM10},
  {BFD_RELOC_SPARC_PC_LM22, R_SPARC_PC_LM22},
  {BFD_RELOC_SPARC_WDISP16, R_SPARC_WDISP16},
  {BFD_RELOC_SPARC_WDISP19, R_SPARC_WDISP19},
  {BFD_RELOC_SPARC_GLOB_JMP, R_SPARC_GLOB_JMP},
  {BFD_RELOC_SPARC_7, R_SPARC_7},
#if 0 /* unused */
  {BFD_RELOC_SPARC_5, R_SPARC_5},
  {BFD_RELOC_SPARC_6, R_SPARC_6},
#endif
};

static reloc_howto_type *
sparc64_elf_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;
  for (i = 0; i < sizeof (sparc_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (sparc_reloc_map[i].bfd_reloc_val == code)
	return &sparc64_elf_howto_table[(int) sparc_reloc_map[i].elf_reloc_val];
    }
  return 0;
}

static void
elf_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf64_Internal_Rela *dst;
{
  BFD_ASSERT (ELF64_R_TYPE (dst->r_info) < (unsigned int) R_SPARC_max);
  cache_ptr->howto = &sparc64_elf_howto_table[ELF64_R_TYPE (dst->r_info)];
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
     char **error_message;
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

  x = bfd_get_32 (abfd, (char *) data + reloc_entry->address);
  x |= ((((relocation >> 2) & 0xc000) << 6)
	| ((relocation >> 2) & 0x3fff));
  bfd_put_32 (abfd, x, (char *) data + reloc_entry->address);

  if ((bfd_signed_vma) relocation < - 0x40000
      || (bfd_signed_vma) relocation > 0x3ffff)
    return bfd_reloc_overflow;
  else
    return bfd_reloc_ok;
}

/* Relocate a SPARC64 ELF section.  */

static boolean
sparc64_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sec;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      r_type = ELF64_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_SPARC_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = sparc64_elf_howto_table + r_type;

      r_symndx = ELF64_R_SYM (rel->r_info);

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
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      relocation = (h->root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset)))
		return false;
	      relocation = 0;
	    }
	}

      if (r_type != R_SPARC_WDISP16)
	r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				      contents, rel->r_offset,
				      relocation, rel->r_addend);
      else
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
		    name = (bfd_elf_string_from_elf_section
			    (input_bfd,
			     symtab_hdr->sh_link,
			     sym->st_name));
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

/* Set the right machine number for a SPARC64 ELF file.  */

static boolean
sparc64_elf_object_p (abfd)
     bfd *abfd;
{
  return bfd_default_set_arch_mach (abfd, bfd_arch_sparc, bfd_mach_sparc_v9);
}

#define TARGET_BIG_SYM	bfd_elf64_sparc_vec
#define TARGET_BIG_NAME	"elf64-sparc"
#define ELF_ARCH	bfd_arch_sparc
#define ELF_MACHINE_CODE EM_SPARC64
#define ELF_MAXPAGESIZE 0x100000

#define bfd_elf64_bfd_reloc_type_lookup	sparc64_elf_reloc_type_lookup
#define elf_backend_relocate_section	sparc64_elf_relocate_section
#define elf_backend_object_p		sparc64_elf_object_p

#include "elf64-target.h"
