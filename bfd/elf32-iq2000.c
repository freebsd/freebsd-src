/* IQ2000-specific support for 32-bit ELF.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.

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
#include "elf/iq2000.h"

/* Forward declarations.  */

/* Private relocation functions.  */
static bfd_reloc_status_type iq2000_elf_relocate_hi16	       PARAMS ((bfd *, Elf_Internal_Rela *, bfd_byte *, bfd_vma));
static reloc_howto_type *    iq2000_reloc_type_lookup	       PARAMS ((bfd *, bfd_reloc_code_real_type));
static void		     iq2000_info_to_howto_rela	       PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static bfd_boolean	     iq2000_elf_relocate_section       PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *, Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static bfd_reloc_status_type iq2000_final_link_relocate	       PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *, Elf_Internal_Rela *, bfd_vma));
static bfd_boolean	     iq2000_elf_gc_sweep_hook	       PARAMS ((bfd *, struct bfd_link_info *, asection *, const Elf_Internal_Rela *));
static asection *	     iq2000_elf_gc_mark_hook	       PARAMS ((asection *sec, struct bfd_link_info *, Elf_Internal_Rela *, struct elf_link_hash_entry *, Elf_Internal_Sym *));
static reloc_howto_type *    iq2000_reloc_type_lookup	       PARAMS ((bfd *, bfd_reloc_code_real_type));
static int		     elf32_iq2000_machine	       PARAMS ((bfd *));
static bfd_boolean	     iq2000_elf_object_p	       PARAMS ((bfd *));
static bfd_boolean	     iq2000_elf_set_private_flags      PARAMS ((bfd *, flagword));
static bfd_boolean	     iq2000_elf_copy_private_bfd_data  PARAMS ((bfd *, bfd *));
static bfd_boolean	     iq2000_elf_merge_private_bfd_data PARAMS ((bfd *, bfd *));
static bfd_boolean	     iq2000_elf_print_private_bfd_data PARAMS ((bfd *, PTR));
static bfd_boolean	     iq2000_elf_check_relocs	       PARAMS ((bfd *, struct bfd_link_info *, asection *, const Elf_Internal_Rela *));
static bfd_reloc_status_type iq2000_elf_howto_hi16_reloc       PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));


static reloc_howto_type iq2000_elf_howto_table [] =
{
  /* This reloc does nothing.  */

  HOWTO (R_IQ2000_NONE,		     /* type */
	 0,			     /* rightshift */
	 2,			     /* size (0 = byte, 1 = short, 2 = long) */
	 32,			     /* bitsize */
	 FALSE,			     /* pc_relative */
	 0,			     /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	     /* special_function */
	 "R_IQ2000_NONE",	     /* name */
	 FALSE,			     /* partial_inplace */
	 0,			     /* src_mask */
	 0,			     /* dst_mask */
	 FALSE),		     /* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_IQ2000_16,		     /* type */
	 0,			     /* rightshift */
	 1,			     /* size (0 = byte, 1 = short, 2 = long) */
	 16,			     /* bitsize */
	 FALSE,			     /* pc_relative */
	 0,			     /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	     /* special_function */
	 "R_IQ2000_16",		     /* name */
	 FALSE,			     /* partial_inplace */
	 0x0000,		     /* src_mask */
	 0xffff,		     /* dst_mask */
	 FALSE),		     /* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_IQ2000_32,		     /* type */
	 0,			     /* rightshift */
	 2,			     /* size (0 = byte, 1 = short, 2 = long) */
	 31,			     /* bitsize */
	 FALSE,			     /* pc_relative */
	 0,			     /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	     /* special_function */
	 "R_IQ2000_32",		     /* name */
	 FALSE,			     /* partial_inplace */
	 0x00000000,		     /* src_mask */
	 0x7fffffff,		     /* dst_mask */
	 FALSE),		     /* pcrel_offset */

  /* 26 bit branch address.  */
  HOWTO (R_IQ2000_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
				/* This needs complex overflow
				   detection, because the upper four
				   bits must match the PC.  */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_IQ2000_26",		/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x03ffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16 bit PC relative reference.  */
  HOWTO (R_IQ2000_PC16,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_IQ2000_PC16",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* high 16 bits of symbol value.  */
  HOWTO (R_IQ2000_HI16,		/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 15,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 iq2000_elf_howto_hi16_reloc,	/* special_function */
	 "R_IQ2000_HI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0x7fff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_IQ2000_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_IQ2000_LO16",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16-bit jump offset.  */
  HOWTO (R_IQ2000_OFFSET_16,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_IQ2000_OFFSET_16",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 21-bit jump offset.  */
  HOWTO (R_IQ2000_OFFSET_21,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 21,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_IQ2000_OFFSET_21",	/* name */
	 FALSE,			/* partial_inplace */
	 0x000000,		/* src_mask */
	 0x1fffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* unsigned high 16 bits of value.  */
  HOWTO (R_IQ2000_OFFSET_21,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_IQ2000_UHI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0x7fff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute debug relocation.  */
  HOWTO (R_IQ2000_32_DEBUG,	     /* type */
	 0,			     /* rightshift */
	 2,			     /* size (0 = byte, 1 = short, 2 = long) */
	 32,			     /* bitsize */
	 FALSE,			     /* pc_relative */
	 0,			     /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	     /* special_function */
	 "R_IQ2000_32",		     /* name */
	 FALSE,			     /* partial_inplace */
	 0x00000000,		     /* src_mask */
	 0xffffffff,		     /* dst_mask */
	 FALSE),		     /* pcrel_offset */

};

/* GNU extension to record C++ vtable hierarchy.  */
static reloc_howto_type iq2000_elf_vtinherit_howto =
  HOWTO (R_IQ2000_GNU_VTINHERIT,    /* type */
	 0,			   /* rightshift */
	 2,			   /* size (0 = byte, 1 = short, 2 = long) */
	 0,			   /* bitsize */
	 FALSE,			   /* pc_relative */
	 0,			   /* bitpos */
	 complain_overflow_dont,   /* complain_on_overflow */
	 NULL,			   /* special_function */
	 "R_IQ2000_GNU_VTINHERIT",  /* name */
	 FALSE,			   /* partial_inplace */
	 0,			   /* src_mask */
	 0,			   /* dst_mask */
	 FALSE);		   /* pcrel_offset */

/* GNU extension to record C++ vtable member usage.  */
static reloc_howto_type iq2000_elf_vtentry_howto =
  HOWTO (R_IQ2000_GNU_VTENTRY,	   /* type */
	 0,			   /* rightshift */
	 2,			   /* size (0 = byte, 1 = short, 2 = long) */
	 0,			   /* bitsize */
	 FALSE,			   /* pc_relative */
	 0,			   /* bitpos */
	 complain_overflow_dont,   /* complain_on_overflow */
	 NULL,			   /* special_function */
	 "R_IQ2000_GNU_VTENTRY",    /* name */
	 FALSE,			   /* partial_inplace */
	 0,			   /* src_mask */
	 0,			   /* dst_mask */
	 FALSE);		   /* pcrel_offset */


/* Map BFD reloc types to IQ2000 ELF reloc types.  */

struct iq2000_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned int iq2000_reloc_val;
};

static const struct iq2000_reloc_map iq2000_reloc_map [] =
{
  { BFD_RELOC_NONE,	       R_IQ2000_NONE },
  { BFD_RELOC_16,	       R_IQ2000_16 },
  { BFD_RELOC_32,	       R_IQ2000_32 },
  { BFD_RELOC_MIPS_JMP,	       R_IQ2000_26 },
  { BFD_RELOC_16_PCREL_S2,     R_IQ2000_PC16 },
  { BFD_RELOC_HI16,	       R_IQ2000_HI16 },
  { BFD_RELOC_LO16,	       R_IQ2000_LO16 },
  { BFD_RELOC_IQ2000_OFFSET_16,R_IQ2000_OFFSET_16 },
  { BFD_RELOC_IQ2000_OFFSET_21,R_IQ2000_OFFSET_21 },
  { BFD_RELOC_IQ2000_UHI16,    R_IQ2000_UHI16 },
  { BFD_RELOC_VTABLE_INHERIT,  R_IQ2000_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY,    R_IQ2000_GNU_VTENTRY },
};

static bfd_reloc_status_type
iq2000_elf_howto_hi16_reloc (abfd,
		     reloc_entry,
		     symbol,
		     data,
		     input_section,
		     output_bfd,
		     error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;

  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  /* if %lo will have sign-extension, compensate by add 0x10000 to hi portion */
  if (relocation & 0x8000)
    reloc_entry->addend += 0x10000;

  /* Now do the reloc in the usual way.	 */
  ret = bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
				input_section, output_bfd, error_message);

  /* put it back the way it was */
  if (relocation & 0x8000)
    reloc_entry->addend -= 0x10000;

  return ret;
}

static bfd_reloc_status_type
iq2000_elf_relocate_hi16 (input_bfd, relhi, contents, value)
     bfd *input_bfd;
     Elf_Internal_Rela *relhi;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;

  insn = bfd_get_32 (input_bfd, contents + relhi->r_offset);
  
  value += relhi->r_addend;
  value &= 0x7fffffff; /* mask off top-bit which is Harvard mask bit */

  /* if top-bit of %lo value is on, this means that %lo will
     sign-propagate and so we compensate by adding 1 to %hi value */
  if (value & 0x8000)
    value += 0x10000;

  value >>= 16; 
  insn = ((insn & ~0xFFFF) | value);

  bfd_put_32 (input_bfd, insn, contents + relhi->r_offset);
  return bfd_reloc_ok;
}

static reloc_howto_type *
iq2000_reloc_type_lookup (abfd, code)
     bfd * abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  /* Note that the iq2000_elf_howto_table is indxed by the R_
     constants.	 Thus, the order that the howto records appear in the
     table *must* match the order of the relocation types defined in
     include/elf/iq2000.h.  */

  switch (code)
    {
    case BFD_RELOC_NONE:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_NONE];
    case BFD_RELOC_16:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_16];
    case BFD_RELOC_32:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_32];
    case BFD_RELOC_MIPS_JMP:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_26];
    case BFD_RELOC_IQ2000_OFFSET_16:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_OFFSET_16];
    case BFD_RELOC_IQ2000_OFFSET_21:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_OFFSET_21];
    case BFD_RELOC_16_PCREL_S2:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_PC16];
    case BFD_RELOC_HI16:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_HI16];
    case BFD_RELOC_IQ2000_UHI16:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_UHI16];
    case BFD_RELOC_LO16:
      return &iq2000_elf_howto_table[ (int) R_IQ2000_LO16];
    case BFD_RELOC_VTABLE_INHERIT:
      return &iq2000_elf_vtinherit_howto;
    case BFD_RELOC_VTABLE_ENTRY:
      return &iq2000_elf_vtentry_howto;
    default:
      /* Pacify gcc -Wall.  */
      return NULL;
    }
  return NULL;
}


/* Perform a single relocation.	 By default we use the standard BFD
   routines.  */

static bfd_reloc_status_type
iq2000_final_link_relocate (howto, input_bfd, input_section, contents, rel, relocation)
     reloc_howto_type *	 howto;
     bfd *		 input_bfd;
     asection *		 input_section;
     bfd_byte *		 contents;
     Elf_Internal_Rela * rel;
     bfd_vma		 relocation;
{
  return _bfd_final_link_relocate (howto, input_bfd, input_section,
				   contents, rel->r_offset,
				   relocation, rel->r_addend);
}

/* Set the howto pointer for a IQ2000 ELF reloc.  */

static void
iq2000_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd * abfd ATTRIBUTE_UNUSED;
     arelent * cache_ptr;
     Elf_Internal_Rela * dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  switch (r_type)
    {
    case R_IQ2000_GNU_VTINHERIT:
      cache_ptr->howto = & iq2000_elf_vtinherit_howto;
      break;

    case R_IQ2000_GNU_VTENTRY:
      cache_ptr->howto = & iq2000_elf_vtentry_howto;
      break;

    default:
      cache_ptr->howto = & iq2000_elf_howto_table [r_type];
      break;
    }
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.	 */
 
static bfd_boolean
iq2000_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  bfd_boolean changed = FALSE;
  
  if (info->relocatable)
    return TRUE;
  
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof(Elf32_External_Sym);
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
      
      switch (ELF32_R_TYPE (rel->r_info))
	{
	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_IQ2000_GNU_VTINHERIT:
	  if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return FALSE;
	  break;
	  
	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_IQ2000_GNU_VTENTRY:
	  if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return FALSE;
	  break;

	case R_IQ2000_32:
	  /* For debug section, change to special harvard-aware relocations */
	  if (memcmp (sec->name, ".debug", 6) == 0
	      || memcmp (sec->name, ".stab", 5) == 0
	      || memcmp (sec->name, ".eh_frame", 9) == 0)
	    {
	      ((Elf_Internal_Rela *) rel)->r_info
		= ELF32_R_INFO (ELF32_R_SYM (rel->r_info), R_IQ2000_32_DEBUG);
	      changed = TRUE;
	    }
	  break;
	}
    }

  if (changed)
    /* Note that we've changed relocs, otherwise if !info->keep_memory
       we'll free the relocs and lose our changes.  */
    (const Elf_Internal_Rela *) (elf_section_data (sec)->relocs) = relocs;

  return TRUE;
}


/* Relocate a IQ2000 ELF section.
   There is some attempt to make this function usable for many architectures,
   both USE_REL and USE_RELA ['twould be nice if such a critter existed],
   if only to serve as a learning tool.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjusting the section contents as
   necessary, and (if using Rela relocs and generating a relocatable
   output file) adjusting the reloc addend as necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocatable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.	 */

static bfd_boolean
iq2000_elf_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, local_syms, local_sections)
     bfd *		     output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *  info;
     bfd *		     input_bfd;
     asection *		     input_section;
     bfd_byte *		     contents;
     Elf_Internal_Rela *     relocs;
     Elf_Internal_Sym *	     local_syms;
     asection **	     local_sections;
{
  Elf_Internal_Shdr *		symtab_hdr;
  struct elf_link_hash_entry ** sym_hashes;
  Elf_Internal_Rela *		rel;
  Elf_Internal_Rela *		relend;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *	   howto;
      unsigned long		   r_symndx;
      Elf_Internal_Sym *	   sym;
      asection *		   sec;
      struct elf_link_hash_entry * h;
      bfd_vma			   relocation;
      bfd_reloc_status_type	   r;
      const char *		   name = NULL;
      int			   r_type;
      
      r_type = ELF32_R_TYPE (rel->r_info);
      
      if (   r_type == R_IQ2000_GNU_VTINHERIT
	  || r_type == R_IQ2000_GNU_VTENTRY)
	continue;
      
      r_symndx = ELF32_R_SYM (rel->r_info);

      /* This is a final link.	*/
      howto  = iq2000_elf_howto_table + ELF32_R_TYPE (rel->r_info);
      h	     = NULL;
      sym    = NULL;
      sec    = NULL;
      
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
	  
	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, sec) : name;
#ifdef DEBUG
	  fprintf (stderr, "local: sec: %s, sym: %s (%d), value: %x + %x + %x addend %x\n",
		   sec->name, name, sym->st_name,
		   sec->output_section->vma, sec->output_offset,
		   sym->st_value, rel->r_addend);
#endif
	}
      else
	{
	  bfd_boolean unresolved_reloc;
	  bfd_boolean warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);

	  name = h->root.root.string;
	}

      switch (r_type)
	{
	case R_IQ2000_HI16:
	  r = iq2000_elf_relocate_hi16 (input_bfd, rel, contents, relocation);
	  break;

	case R_IQ2000_PC16:
	  rel->r_addend -= 4;
	  /* Fall through.  */

	default:
	  r = iq2000_final_link_relocate (howto, input_bfd, input_section,
					 contents, rel, relocation);
	  break;
	}

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


/* Update the got entry reference counts for the section being
   removed.  */

static bfd_boolean
iq2000_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *		       abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *    info ATTRIBUTE_UNUSED;
     asection *		       sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela * relocs ATTRIBUTE_UNUSED;
{
  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.	*/

static asection *
iq2000_elf_gc_mark_hook (sec, info, rel, h, sym)
     asection *			  sec;
     struct bfd_link_info *	  info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *	  rel;
     struct elf_link_hash_entry * h;
     Elf_Internal_Sym *		  sym;
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_IQ2000_GNU_VTINHERIT:
	case R_IQ2000_GNU_VTENTRY:
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


/* Return the MACH for an e_flags value.  */

static int
elf32_iq2000_machine (abfd)
     bfd *abfd;
{
  switch (elf_elfheader (abfd)->e_flags & EF_IQ2000_CPU_MASK)
    {
    case EF_IQ2000_CPU_IQ2000:	return bfd_mach_iq2000;
    case EF_IQ2000_CPU_IQ10:  return bfd_mach_iq10;
    }

  return bfd_mach_iq2000;
}


/* Function to set the ELF flag bits.  */

static bfd_boolean
iq2000_elf_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Copy backend specific data from one object module to another.  */

static bfd_boolean
iq2000_elf_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  BFD_ASSERT (!elf_flags_init (obfd)
	      || elf_elfheader (obfd)->e_flags == elf_elfheader (ibfd)->e_flags);

  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_flags_init (obfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static bfd_boolean
iq2000_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags, old_partial;
  flagword new_flags, new_partial;
  bfd_boolean error = FALSE;
  char new_opt[80];
  char old_opt[80];

  new_opt[0] = old_opt[0] = '\0';
  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;

#ifdef DEBUG
  (*_bfd_error_handler) ("old_flags = 0x%.8lx, new_flags = 0x%.8lx, init = %s, filename = %s",
			 old_flags, new_flags, elf_flags_init (obfd) ? "yes" : "no",
			 bfd_get_filename (ibfd));
#endif

  if (!elf_flags_init (obfd))
    {
      /* First call, no flags set.  */
      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = new_flags;
    }

  else if (new_flags == old_flags)
    /* Compatible flags are ok.	 */
    ;

  else		/* Possibly incompatible flags.	 */
    {
      /* Warn if different cpu is used (allow a specific cpu to override
	 the generic cpu).  */
      new_partial = (new_flags & EF_IQ2000_CPU_MASK);
      old_partial = (old_flags & EF_IQ2000_CPU_MASK);
      if (new_partial == old_partial)
	;

      else
	{
	  switch (new_partial)
	    {
	    default:		  strcat (new_opt, " -m2000");	break;
	    case EF_IQ2000_CPU_IQ2000:	strcat (new_opt, " -m2000");  break;
	    case EF_IQ2000_CPU_IQ10:  strcat (new_opt, " -m10");  break;
	    }

	  switch (old_partial)
	    {
	    default:		  strcat (old_opt, " -m2000");	break;
	    case EF_IQ2000_CPU_IQ2000:	strcat (old_opt, " -m2000");  break;
	    case EF_IQ2000_CPU_IQ10:  strcat (old_opt, " -m10");  break;
	    }
	}
      
      /* Print out any mismatches from above.  */
      if (new_opt[0])
	{
	  error = TRUE;
	  (*_bfd_error_handler)
	    (_("%s: compiled with %s and linked with modules compiled with %s"),
	     bfd_get_filename (ibfd), new_opt, old_opt);
	}

      new_flags &= ~ EF_IQ2000_ALL_FLAGS;
      old_flags &= ~ EF_IQ2000_ALL_FLAGS;

      /* Warn about any other mismatches.  */
      if (new_flags != old_flags)
	{
	  error = TRUE;
	  (*_bfd_error_handler)
	    (_("%s: uses different e_flags (0x%lx) fields than previous modules (0x%lx)"),
	     bfd_get_filename (ibfd), (long)new_flags, (long)old_flags);
	}
    }

  if (error)
    bfd_set_error (bfd_error_bad_value);

  return !error;
}


static bfd_boolean
iq2000_elf_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE *file = (FILE *) ptr;
  flagword flags;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  /* Print normal ELF private data.  */
  _bfd_elf_print_private_bfd_data (abfd, ptr);

  flags = elf_elfheader (abfd)->e_flags;
  fprintf (file, _("private flags = 0x%lx:"), (long)flags);

  switch (flags & EF_IQ2000_CPU_MASK)
    {
    default:							break;
    case EF_IQ2000_CPU_IQ2000:	fprintf (file, " -m2000");	break;
    case EF_IQ2000_CPU_IQ10:  fprintf (file, " -m10");	break;
    }

  fputc ('\n', file);
  return TRUE;
}

static
bfd_boolean
iq2000_elf_object_p (abfd)
     bfd *abfd;
{
  /* Irix 5 and 6 is broken.  Object file symbol tables are not always
     sorted correctly such that local symbols precede global symbols,
     and the sh_info field in the symbol table is not always right.  */
  elf_bad_symtab (abfd) = TRUE;

  bfd_default_set_arch_mach (abfd, bfd_arch_iq2000,
			     elf32_iq2000_machine (abfd));
  return TRUE;
}


#define ELF_ARCH		bfd_arch_iq2000
#define ELF_MACHINE_CODE	EM_IQ2000
#define ELF_MAXPAGESIZE		0x1000

#define TARGET_BIG_SYM		bfd_elf32_iq2000_vec
#define TARGET_BIG_NAME		"elf32-iq2000"

#define elf_info_to_howto_rel			NULL
#define elf_info_to_howto			iq2000_info_to_howto_rela
#define elf_backend_relocate_section		iq2000_elf_relocate_section
#define elf_backend_gc_mark_hook		iq2000_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook		iq2000_elf_gc_sweep_hook
#define elf_backend_check_relocs		iq2000_elf_check_relocs
#define elf_backend_object_p			iq2000_elf_object_p
#define elf_backend_rela_normal			1

#define elf_backend_can_gc_sections		1

#define bfd_elf32_bfd_reloc_type_lookup		iq2000_reloc_type_lookup
#define bfd_elf32_bfd_set_private_flags		iq2000_elf_set_private_flags
#define bfd_elf32_bfd_copy_private_bfd_data	iq2000_elf_copy_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data	iq2000_elf_merge_private_bfd_data
#define bfd_elf32_bfd_print_private_bfd_data	iq2000_elf_print_private_bfd_data

#include "elf32-target.h"
