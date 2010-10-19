/* OpenRISC-specific support for 32-bit ELF.
   Copyright 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
   Contributed by Johan Rydberg, jrydberg@opencores.org

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/openrisc.h"
#include "libiberty.h"

static reloc_howto_type openrisc_elf_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_OPENRISC_NONE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_OPENRISC_NONE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A PC relative 26 bit relocation, right shifted by 2.  */
  HOWTO (R_OPENRISC_INSN_REL_26, /* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_OPENRISC_INSN_REL_26", /* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x03ffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A absolute 26 bit relocation, right shifted by 2.  */
  HOWTO (R_OPENRISC_INSN_ABS_26, /* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_OPENRISC_INSN_ABS_26", /* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x03ffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_OPENRISC_LO_16_IN_INSN, /* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_OPENRISC_LO_16_IN_INSN", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_OPENRISC_HI_16_IN_INSN, /* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_OPENRISC_HI_16_IN_INSN",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 8 bit absolute relocation.  */
  HOWTO (R_OPENRISC_8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_OPENRISC_8",	/* name */
	 TRUE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_OPENRISC_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_OPENRISC_16",	/* name */
	 TRUE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_OPENRISC_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_OPENRISC_32",	/* name */
	 TRUE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_OPENRISC_GNU_VTINHERIT, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_OPENRISC_GNU_VTINHERIT", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_OPENRISC_GNU_VTENTRY, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_elf_rel_vtable_reloc_fn, /* special_function */
	 "R_OPENRISC_GNU_VTENTRY", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */
};

/* Map BFD reloc types to OpenRISC ELF reloc types.  */

struct openrisc_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned int openrisc_reloc_val;
};

static const struct openrisc_reloc_map openrisc_reloc_map[] =
{
  { BFD_RELOC_NONE, 		R_OPENRISC_NONE },
  { BFD_RELOC_32, 		R_OPENRISC_32 },
  { BFD_RELOC_16, 		R_OPENRISC_16 },
  { BFD_RELOC_8, 		R_OPENRISC_8 },
  { BFD_RELOC_OPENRISC_REL_26,R_OPENRISC_INSN_REL_26 },
  { BFD_RELOC_OPENRISC_ABS_26,R_OPENRISC_INSN_ABS_26 },
    { BFD_RELOC_HI16, 		R_OPENRISC_HI_16_IN_INSN },
  { BFD_RELOC_LO16, 		R_OPENRISC_LO_16_IN_INSN },
  { BFD_RELOC_VTABLE_INHERIT,	R_OPENRISC_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, 	R_OPENRISC_GNU_VTENTRY }
};

static reloc_howto_type *
openrisc_reloc_type_lookup (bfd * abfd ATTRIBUTE_UNUSED,
			    bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = ARRAY_SIZE (openrisc_reloc_map); --i;)
    if (openrisc_reloc_map[i].bfd_reloc_val == code)
      return & openrisc_elf_howto_table[openrisc_reloc_map[i].
				       openrisc_reloc_val];

  return NULL;
}

/* Set the howto pointer for an OpenRISC ELF reloc.  */

static void
openrisc_info_to_howto_rela (bfd * abfd ATTRIBUTE_UNUSED,
			     arelent * cache_ptr,
			     Elf_Internal_Rela * dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_OPENRISC_max);
  cache_ptr->howto = & openrisc_elf_howto_table[r_type];
}

/* Perform a single relocation.  By default we use the standard BFD
   routines, but a few relocs, we have to do them ourselves.  */

static bfd_reloc_status_type
openrisc_final_link_relocate (reloc_howto_type *howto,
			      bfd *input_bfd,
			      asection *input_section,
			      bfd_byte *contents,
			      Elf_Internal_Rela *rel,
			      bfd_vma relocation)
{
  bfd_reloc_status_type r = bfd_reloc_ok;

  switch (howto->type)
    {
    case R_OPENRISC_LO_16_IN_INSN:
      relocation &= 0xffff;
      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, rel->r_addend);
      break;

    default:
      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, rel->r_addend);
    }

  return r;
}

/* Relocate an OpenRISC ELF section.

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
   accordingly.  */

static bfd_boolean
openrisc_elf_relocate_section (bfd *output_bfd,
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
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel++)
    {
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      const char *name = NULL;
      int r_type;

      r_type = ELF32_R_TYPE (rel->r_info);
      r_symndx = ELF32_R_SYM (rel->r_info);

      if (r_type == R_OPENRISC_GNU_VTINHERIT
	  || r_type == R_OPENRISC_GNU_VTENTRY)
	continue;

      if ((unsigned int) r_type >
	  (sizeof openrisc_elf_howto_table / sizeof (reloc_howto_type)))
	abort ();

      /* This is a final link.  */
      howto = openrisc_elf_howto_table + ELF32_R_TYPE (rel->r_info);
      h = NULL;
      sym = NULL;
      sec = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
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

      r = openrisc_final_link_relocate (howto, input_bfd, input_section,
					contents, rel, relocation);

      if (r != bfd_reloc_ok)
	{
	  const char *msg = NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, (h ? &h->root : NULL), name, howto->name,
		 (bfd_vma) 0, input_bfd, input_section, rel->r_offset);
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

	  if (!r)
	    return FALSE;
	}
    }

  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
openrisc_elf_gc_mark_hook (asection *sec,
			   struct bfd_link_info *info ATTRIBUTE_UNUSED,
			   Elf_Internal_Rela *rel,
			   struct elf_link_hash_entry *h,
			   Elf_Internal_Sym *sym)
{
  if (h == NULL)
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  switch (ELF32_R_TYPE (rel->r_info))
    {
    case R_OPENRISC_GNU_VTINHERIT:
    case R_OPENRISC_GNU_VTENTRY:
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

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

static bfd_boolean
openrisc_elf_gc_sweep_hook (bfd *abfd ATTRIBUTE_UNUSED,
			    struct bfd_link_info *info ATTRIBUTE_UNUSED,
			    asection *sec ATTRIBUTE_UNUSED,
			    const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED)
{
  return TRUE;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
openrisc_elf_check_relocs (bfd *abfd,
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
  sym_hashes_end =
    sym_hashes + symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
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
	case R_OPENRISC_GNU_VTINHERIT:
	  if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return FALSE;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_OPENRISC_GNU_VTENTRY:
	  if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return FALSE;
	  break;
	}
    }

  return TRUE;
}

/* Set the right machine number.  */

static bfd_boolean
openrisc_elf_object_p (bfd *abfd)
{
  bfd_default_set_arch_mach (abfd, bfd_arch_openrisc, 0);
  return TRUE;
}

/* Store the machine number in the flags field.  */

static void
openrisc_elf_final_write_processing (bfd *abfd,
				     bfd_boolean linker ATTRIBUTE_UNUSED)
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
      val = 0;
      break;
    }

  elf_elfheader (abfd)->e_flags &= ~0xf;
  elf_elfheader (abfd)->e_flags |= val;
}


#define ELF_ARCH			bfd_arch_openrisc
#define ELF_MACHINE_CODE		EM_OPENRISC
#define ELF_MACHINE_ALT1		EM_OPENRISC_OLD
#define ELF_MAXPAGESIZE			0x1000

#define TARGET_BIG_SYM			bfd_elf32_openrisc_vec
#define TARGET_BIG_NAME			"elf32-openrisc"

#define elf_info_to_howto_rel		NULL
#define elf_info_to_howto		openrisc_info_to_howto_rela
#define elf_backend_relocate_section	openrisc_elf_relocate_section
#define elf_backend_gc_mark_hook	openrisc_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook	openrisc_elf_gc_sweep_hook
#define elf_backend_check_relocs	openrisc_elf_check_relocs

#define elf_backend_can_gc_sections	1
#define elf_backend_rela_normal		1

#define bfd_elf32_bfd_reloc_type_lookup openrisc_reloc_type_lookup

#define elf_backend_object_p                openrisc_elf_object_p
#define elf_backend_final_write_processing  openrisc_elf_final_write_processing

#include "elf32-target.h"
