/* DLX specific support for 32-bit ELF
   Copyright 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

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
#include "elf/dlx.h"

#define USE_REL 1

#define bfd_elf32_bfd_reloc_type_lookup elf32_dlx_reloc_type_lookup
#define elf_info_to_howto               elf32_dlx_info_to_howto
#define elf_info_to_howto_rel           elf32_dlx_info_to_howto_rel
#define elf_backend_check_relocs        elf32_dlx_check_relocs

/* The gas default behavior is not to preform the %hi modifier so that the
   GNU assembler can have the lower 16 bits offset placed in the insn, BUT
   we do like the gas to indicate it is %hi reloc type so when we in the link
   loader phase we can have the corrected hi16 vale replace the buggous lo16
   value that was placed there by gas.  */

static int skip_dlx_elf_hi16_reloc = 0;

extern int set_dlx_skip_hi16_flag (int);

int
set_dlx_skip_hi16_flag (int flag)
{
  skip_dlx_elf_hi16_reloc = flag;
  return flag;
}

static bfd_reloc_status_type
_bfd_dlx_elf_hi16_reloc (bfd *abfd,
			 arelent *reloc_entry,
			 asymbol *symbol,
			 void * data,
			 asection *input_section,
			 bfd *output_bfd,
			 char **error_message)
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;

  /* If the skip flag is set then we simply do the generic relocating, this
     is more of a hack for dlx gas/gld, so we do not need to do the %hi/%lo
     fixup like mips gld did.   */
  if (skip_dlx_elf_hi16_reloc)
    return bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
                          input_section, output_bfd, error_message);

  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  ret = bfd_reloc_ok;

  if (bfd_is_und_section (symbol->section)
      && output_bfd == (bfd *) NULL)
    ret = bfd_reloc_undefined;

  relocation = (bfd_is_com_section (symbol->section)) ? 0 : symbol->value;
  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;
  relocation += bfd_get_16 (abfd, (bfd_byte *)data + reloc_entry->address);

  if (reloc_entry->address > bfd_get_section_limit (abfd, input_section))
    return bfd_reloc_outofrange;

  bfd_put_16 (abfd, (short)((relocation >> 16) & 0xFFFF),
              (bfd_byte *)data + reloc_entry->address);

  return ret;
}

/* ELF relocs are against symbols.  If we are producing relocatable
   output, and the reloc is against an external symbol, and nothing
   has given us any additional addend, the resulting reloc will also
   be against the same symbol.  In such a case, we don't want to
   change anything about the way the reloc is handled, since it will
   all be done at final link time.  Rather than put special case code
   into bfd_perform_relocation, all the reloc types use this howto
   function.  It just short circuits the reloc if producing
   relocatable output against an external symbol.  */

static bfd_reloc_status_type
elf32_dlx_relocate16 (bfd *abfd,
		      arelent *reloc_entry,
		      asymbol *symbol,
		      void * data,
		      asection *input_section,
		      bfd *output_bfd,
		      char **error_message ATTRIBUTE_UNUSED)
{
  unsigned long insn, vallo, allignment;
  int           val;

  /* HACK: I think this first condition is necessary when producing
     relocatable output.  After the end of HACK, the code is identical
     to bfd_elf_generic_reloc().  I would _guess_ the first change
     belongs there rather than here.  martindo 1998-10-23.  */

  if (skip_dlx_elf_hi16_reloc)
    return bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
                                 input_section, output_bfd, error_message);

  /* Check undefined section and undefined symbols.  */
  if (bfd_is_und_section (symbol->section)
      && output_bfd == (bfd *) NULL)
    return bfd_reloc_undefined;

  /* Can not support a long jump to sections other then .text.  */
  if (strcmp (input_section->name, symbol->section->output_section->name) != 0)
    {
      fprintf (stderr,
	       "BFD Link Error: branch (PC rel16) to section (%s) not supported\n",
	       symbol->section->output_section->name);
      return bfd_reloc_undefined;
    }

  insn  = bfd_get_32 (abfd, (bfd_byte *)data + reloc_entry->address);
  allignment = 1 << (input_section->output_section->alignment_power - 1);
  vallo = insn & 0x0000FFFF;

  if (vallo & 0x8000)
    vallo = ~(vallo | 0xFFFF0000) + 1;

  /* vallo points to the vma of next instruction.  */
  vallo += (((unsigned long)(input_section->output_section->vma +
                           input_section->output_offset) +
            allignment) & ~allignment);

  /* val is the displacement (PC relative to next instruction).  */
  val =  (symbol->section->output_offset +
	  symbol->section->output_section->vma +
	  symbol->value) - vallo;

  if (abs ((int) val) > 0x00007FFF)
    return bfd_reloc_outofrange;

  insn  = (insn & 0xFFFF0000) | (val & 0x0000FFFF);

  bfd_put_32 (abfd, insn,
              (bfd_byte *) data + reloc_entry->address);

  return bfd_reloc_ok;
}

static bfd_reloc_status_type
elf32_dlx_relocate26 (bfd *abfd,
		      arelent *reloc_entry,
		      asymbol *symbol,
		      void * data,
		      asection *input_section,
		      bfd *output_bfd,
		      char **error_message ATTRIBUTE_UNUSED)
{
  unsigned long insn, vallo, allignment;
  int           val;

  /* HACK: I think this first condition is necessary when producing
     relocatable output.  After the end of HACK, the code is identical
     to bfd_elf_generic_reloc().  I would _guess_ the first change
     belongs there rather than here.  martindo 1998-10-23.  */

  if (skip_dlx_elf_hi16_reloc)
    return bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
                                 input_section, output_bfd, error_message);

  /* Check undefined section and undefined symbols.  */
  if (bfd_is_und_section (symbol->section)
      && output_bfd == (bfd *) NULL)
    return bfd_reloc_undefined;

  /* Can not support a long jump to sections other then .text   */
  if (strcmp (input_section->name, symbol->section->output_section->name) != 0)
    {
      fprintf (stderr,
	       "BFD Link Error: jump (PC rel26) to section (%s) not supported\n",
	       symbol->section->output_section->name);
      return bfd_reloc_undefined;
    }

  insn  = bfd_get_32 (abfd, (bfd_byte *)data + reloc_entry->address);
  allignment = 1 << (input_section->output_section->alignment_power - 1);
  vallo = insn & 0x03FFFFFF;

  if (vallo & 0x03000000)
    vallo = ~(vallo | 0xFC000000) + 1;

  /* vallo is the vma for the next instruction.  */
  vallo += (((unsigned long) (input_section->output_section->vma +
			      input_section->output_offset) +
	     allignment) & ~allignment);

  /* val is the displacement (PC relative to next instruction).  */
  val = (symbol->section->output_offset +
	 symbol->section->output_section->vma + symbol->value)
    - vallo;

  if (abs ((int) val) > 0x01FFFFFF)
    return bfd_reloc_outofrange;

  insn  = (insn & 0xFC000000) | (val & 0x03FFFFFF);
  bfd_put_32 (abfd, insn,
              (bfd_byte *) data + reloc_entry->address);

  return bfd_reloc_ok;
}

static reloc_howto_type dlx_elf_howto_table[]=
{
  /* No relocation.  */
  HOWTO (R_DLX_NONE,            /* Type. */
	 0,                     /* Rightshift.  */
	 0,                     /* size (0 = byte, 1 = short, 2 = long).  */
	 0,                     /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_DLX_NONE",          /* Name.  */
	 FALSE,                 /* Partial_inplace.  */
	 0,                     /* Src_mask.  */
	 0,                     /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */

  /* 8 bit relocation.  */
  HOWTO (R_DLX_RELOC_8,         /* Type. */
	 0,                     /* Rightshift.  */
	 0,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 8,                     /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_DLX_RELOC_8",       /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xff,                  /* Src_mask.  */
	 0xff,                  /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */

  /* 16 bit relocation.  */
  HOWTO (R_DLX_RELOC_16,        /* Type. */
	 0,                     /* Rightshift.  */
	 1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 16,                    /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_DLX_RELOC_16",      /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xffff,                /* Src_mask.  */
	 0xffff,                /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */

  /* 32 bit relocation.  */
  HOWTO (R_DLX_RELOC_32,        /* Type. */
	 0,                     /* Rightshift.  */
	 2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 32,                    /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_DLX_RELOC_32",      /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xffffffff,            /* Src_mask.  */
	 0xffffffff,            /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */

  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_DLX_GNU_VTINHERIT,   /* Type. */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 NULL,			/* Special_function.  */
	 "R_DLX_GNU_VTINHERIT", /* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Src_mask.  */
	 0,			/* Dst_mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_DLX_GNU_VTENTRY,     /* Type. */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 _bfd_elf_rel_vtable_reloc_fn,/* Special_function.  */
	 "R_DLX_GNU_VTENTRY",	/* Name.  */
	 FALSE,		  	/* Partial_inplace.  */
	 0,			/* Src_mask.  */
	 0,			/* Dst_mask.  */
	 FALSE)		  	/* PCrel_offset.  */
};

/* 16 bit offset for pc-relative branches.  */
static reloc_howto_type elf_dlx_gnu_rel16_s2 =
  HOWTO (R_DLX_RELOC_16_PCREL,  /* Type. */
	 0,                     /* Rightshift.  */
	 1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 16,                    /* Bitsize.  */
	 TRUE,                  /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 elf32_dlx_relocate16,  /* Special_function.  */
	 "R_DLX_RELOC_16_PCREL",/* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xffff,                /* Src_mask.  */
	 0xffff,                /* Dst_mask.  */
	 TRUE);                 /* PCrel_offset.  */

/* 26 bit offset for pc-relative branches.  */
static reloc_howto_type elf_dlx_gnu_rel26_s2 =
  HOWTO (R_DLX_RELOC_26_PCREL,  /* Type. */
	 0,                     /* Rightshift.  */
	 2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 26,                    /* Bitsize.  */
	 TRUE,                  /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 elf32_dlx_relocate26,  /* Special_function.  */
	 "R_DLX_RELOC_26_PCREL",/* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xffff,                /* Src_mask.  */
	 0xffff,                /* Dst_mask.  */
	 TRUE);                 /* PCrel_offset.  */

/* High 16 bits of symbol value.  */
static reloc_howto_type elf_dlx_reloc_16_hi =
  HOWTO (R_DLX_RELOC_16_HI,     /* Type. */
	 16,                    /* Rightshift.  */
	 2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 32,                    /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 _bfd_dlx_elf_hi16_reloc,/* Special_function.  */
	 "R_DLX_RELOC_16_HI",   /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xFFFF,                /* Src_mask.  */
	 0xffff,                /* Dst_mask.  */
	 FALSE);                /* PCrel_offset.  */

  /* Low 16 bits of symbol value.  */
static reloc_howto_type elf_dlx_reloc_16_lo =
  HOWTO (R_DLX_RELOC_16_LO,     /* Type. */
	 0,                     /* Rightshift.  */
	 1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 16,                    /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_DLX_RELOC_16_LO",   /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xffff,                /* Src_mask.  */
	 0xffff,                /* Dst_mask.  */
	 FALSE);                /* PCrel_offset.  */

/* A mapping from BFD reloc types to DLX ELF reloc types.
   Stolen from elf32-mips.c.

   More about this table - for dlx elf relocation we do not really
   need this table, if we have a rtype defined in this table will
   caused tc_gen_relocate confused and die on us, but if we remove
   this table it will caused more problem, so for now simple solution
   is to remove those entries which may cause problem.  */
struct elf_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  enum elf_dlx_reloc_type elf_reloc_val;
};

static const struct elf_reloc_map dlx_reloc_map[] =
{
  { BFD_RELOC_NONE,           R_DLX_NONE },
  { BFD_RELOC_16,             R_DLX_RELOC_16 },
  { BFD_RELOC_32,             R_DLX_RELOC_32 },
  { BFD_RELOC_DLX_HI16_S,     R_DLX_RELOC_16_HI },
  { BFD_RELOC_DLX_LO16,       R_DLX_RELOC_16_LO },
  { BFD_RELOC_VTABLE_INHERIT,	R_DLX_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY,	R_DLX_GNU_VTENTRY }
};

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
elf32_dlx_check_relocs (bfd *abfd,
			struct bfd_link_info *info,
			asection *sec,
			const Elf_Internal_Rela *relocs)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
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
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}

      switch (ELF32_R_TYPE (rel->r_info))
        {
        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_DLX_GNU_VTINHERIT:
          if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_DLX_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return FALSE;
          break;
        }
    }

  return TRUE;
}

/* Given a BFD reloc type, return a howto structure.  */

static reloc_howto_type *
elf32_dlx_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			     bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0; i < sizeof (dlx_reloc_map) / sizeof (struct elf_reloc_map); i++)
    if (dlx_reloc_map[i].bfd_reloc_val == code)
      return &dlx_elf_howto_table[(int) dlx_reloc_map[i].elf_reloc_val];

  switch (code)
    {
    default:
      bfd_set_error (bfd_error_bad_value);
      return NULL;
    case BFD_RELOC_16_PCREL_S2:
      return &elf_dlx_gnu_rel16_s2;
    case BFD_RELOC_DLX_JMP26:
      return &elf_dlx_gnu_rel26_s2;
    case BFD_RELOC_HI16_S:
      return &elf_dlx_reloc_16_hi;
    case BFD_RELOC_LO16:
      return &elf_dlx_reloc_16_lo;
    }
}

static reloc_howto_type *
dlx_rtype_to_howto (unsigned int r_type)
{
  switch (r_type)
    {
    case R_DLX_RELOC_16_PCREL:
      return & elf_dlx_gnu_rel16_s2;
      break;
    case R_DLX_RELOC_26_PCREL:
      return & elf_dlx_gnu_rel26_s2;
      break;
    case R_DLX_RELOC_16_HI:
      return & elf_dlx_reloc_16_hi;
      break;
    case R_DLX_RELOC_16_LO:
      return & elf_dlx_reloc_16_lo;
      break;

    default:
      BFD_ASSERT (r_type < (unsigned int) R_DLX_max);
      return & dlx_elf_howto_table[r_type];
      break;
    }
}

static void
elf32_dlx_info_to_howto (bfd * abfd ATTRIBUTE_UNUSED,
			 arelent * cache_ptr ATTRIBUTE_UNUSED,
			 Elf_Internal_Rela * dst ATTRIBUTE_UNUSED)
{
  abort ();
}

static void
elf32_dlx_info_to_howto_rel (bfd *abfd ATTRIBUTE_UNUSED,
			     arelent *cache_ptr,
			     Elf_Internal_Rela *dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  cache_ptr->howto = dlx_rtype_to_howto (r_type);
  return;
}

#define TARGET_BIG_SYM          bfd_elf32_dlx_big_vec
#define TARGET_BIG_NAME         "elf32-dlx"
#define ELF_ARCH                bfd_arch_dlx
#define ELF_MACHINE_CODE        EM_DLX
#define ELF_MAXPAGESIZE         1 /* FIXME: This number is wrong,  It should be the page size in bytes.  */

#include "elf32-target.h"
