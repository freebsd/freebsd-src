/*  MSP430-specific support for 32-bit ELF
    Copyright (C) 2002, 2003, 2004 Free Software Foundation, Inc.
    Contributed by Dmitry Diky <diwil@mail.ru>

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
#include "libiberty.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/msp430.h"

/* Use RELA instead of REL.  */
#undef USE_REL

static reloc_howto_type elf_msp430_howto_table[] =
{
  HOWTO (R_MSP430_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MSP430_NONE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_MSP430_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MSP430_32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 13 bit PC relative relocation.  */
  HOWTO (R_MSP430_10_PCREL,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 10,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MSP430_13_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xfff,			/* src_mask */
	 0xfff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_MSP430_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MSP430_16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation for command address.  */
  HOWTO (R_MSP430_16_PCREL,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MSP430_16_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 16 bit absolute relocation, byte operations.  */
  HOWTO (R_MSP430_16_BYTE,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MSP430_16_BYTE",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation for command address.  */
  HOWTO (R_MSP430_16_PCREL_BYTE,/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MSP430_16_PCREL_BYTE",/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 13 bit PC relative relocation for complicated polymorphs.  */
  HOWTO (R_MSP430_2X_PCREL,	/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 10,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MSP430_2X_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xfff,			/* src_mask */
	 0xfff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 16 bit relaxable relocation for command address.  */
  HOWTO (R_MSP430_RL_PCREL,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MSP430_RL_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE)			/* pcrel_offset */
};

/* Map BFD reloc types to MSP430 ELF reloc types.  */

struct msp430_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned int elf_reloc_val;
};

static const struct msp430_reloc_map msp430_reloc_map[] =
  {
    {BFD_RELOC_NONE,                 R_MSP430_NONE},
    {BFD_RELOC_32,                   R_MSP430_32},
    {BFD_RELOC_MSP430_10_PCREL,      R_MSP430_10_PCREL},
    {BFD_RELOC_16,                   R_MSP430_16_BYTE},
    {BFD_RELOC_MSP430_16_PCREL,      R_MSP430_16_PCREL},
    {BFD_RELOC_MSP430_16,            R_MSP430_16},
    {BFD_RELOC_MSP430_16_PCREL_BYTE, R_MSP430_16_PCREL_BYTE},
    {BFD_RELOC_MSP430_16_BYTE,       R_MSP430_16_BYTE},
    {BFD_RELOC_MSP430_2X_PCREL,      R_MSP430_2X_PCREL},
    {BFD_RELOC_MSP430_RL_PCREL,      R_MSP430_RL_PCREL}
  };

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (bfd * abfd ATTRIBUTE_UNUSED,
				 bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0; i < ARRAY_SIZE (msp430_reloc_map); i++)
    if (msp430_reloc_map[i].bfd_reloc_val == code)
      return &elf_msp430_howto_table[msp430_reloc_map[i].elf_reloc_val];

  return NULL;
}

/* Set the howto pointer for an MSP430 ELF reloc.  */

static void
msp430_info_to_howto_rela (bfd * abfd ATTRIBUTE_UNUSED,
			   arelent * cache_ptr,
			   Elf_Internal_Rela * dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_MSP430_max);
  cache_ptr->howto = &elf_msp430_howto_table[r_type];
}

static asection *
elf32_msp430_gc_mark_hook (asection * sec,
			   struct bfd_link_info * info ATTRIBUTE_UNUSED,
			   Elf_Internal_Rela * rel,
			   struct elf_link_hash_entry * h,
			   Elf_Internal_Sym * sym)
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
elf32_msp430_gc_sweep_hook (bfd * abfd ATTRIBUTE_UNUSED,
			    struct bfd_link_info * info ATTRIBUTE_UNUSED,
			    asection * sec ATTRIBUTE_UNUSED,
			    const Elf_Internal_Rela * relocs ATTRIBUTE_UNUSED)
{
  /* We don't use got and plt entries for msp430.  */
  return TRUE;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
elf32_msp430_check_relocs (bfd * abfd, struct bfd_link_info * info,
			   asection * sec, const Elf_Internal_Rela * relocs)
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
    }

  return TRUE;
}

/* Perform a single relocation.  By default we use the standard BFD
   routines, but a few relocs, we have to do them ourselves.  */

static bfd_reloc_status_type
msp430_final_link_relocate (reloc_howto_type * howto, bfd * input_bfd,
			    asection * input_section, bfd_byte * contents,
			    Elf_Internal_Rela * rel, bfd_vma relocation)
{
  bfd_reloc_status_type r = bfd_reloc_ok;
  bfd_vma x;
  bfd_signed_vma srel;

  switch (howto->type)
    {
    case R_MSP430_10_PCREL:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;
      srel -= rel->r_offset;
      srel -= 2;		/* Branch instructions add 2 to the PC...  */
      srel -= (input_section->output_section->vma +
	       input_section->output_offset);

      if (srel & 1)
	return bfd_reloc_outofrange;

      /* MSP430 addresses commands as words.  */
      srel >>= 1;

      /* Check for an overflow.  */
      if (srel < -512 || srel > 511)
	return bfd_reloc_overflow;

      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xfc00) | (srel & 0x3ff);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_MSP430_2X_PCREL:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;
      srel -= rel->r_offset;
      srel -= 2;		/* Branch instructions add 2 to the PC...  */
      srel -= (input_section->output_section->vma +
	       input_section->output_offset);

      if (srel & 1)
	return bfd_reloc_outofrange;

      /* MSP430 addresses commands as words.  */
      srel >>= 1;

      /* Check for an overflow.  */
      if (srel < -512 || srel > 511)
	return bfd_reloc_overflow;

      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xfc00) | (srel & 0x3ff);
      bfd_put_16 (input_bfd, x, contents);
      /* Handle second jump instruction.  */
      x = bfd_get_16 (input_bfd, contents - 2);
      srel += 1;
      x = (x & 0xfc00) | (srel & 0x3ff);
      bfd_put_16 (input_bfd, x, contents - 2);
      break;

    case R_MSP430_16_PCREL:
    case R_MSP430_RL_PCREL:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;
      srel -= rel->r_offset;
      /* Only branch instructions add 2 to the PC...  */
      srel -= (input_section->output_section->vma +
	       input_section->output_offset);

      if (srel & 1)
	return bfd_reloc_outofrange;

      bfd_put_16 (input_bfd, srel & 0xffff, contents);
      break;

    case R_MSP430_16_PCREL_BYTE:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;
      srel -= rel->r_offset;
      /* Only branch instructions add 2 to the PC...  */
      srel -= (input_section->output_section->vma +
	       input_section->output_offset);

      bfd_put_16 (input_bfd, srel & 0xffff, contents);
      break;

    case R_MSP430_16_BYTE:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;
      bfd_put_16 (input_bfd, srel & 0xffff, contents);
      break;

    case R_MSP430_16:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;

      if (srel & 1)
	return bfd_reloc_notsupported;

      bfd_put_16 (input_bfd, srel & 0xffff, contents);
      break;

    default:
      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, rel->r_addend);
    }

  return r;
}

/* Relocate an MSP430 ELF section.  */

static bfd_boolean
elf32_msp430_relocate_section (bfd * output_bfd ATTRIBUTE_UNUSED,
			       struct bfd_link_info * info,
			       bfd * input_bfd,
			       asection * input_section,
			       bfd_byte * contents,
			       Elf_Internal_Rela * relocs,
			       Elf_Internal_Sym * local_syms,
			       asection ** local_sections)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

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

      /* This is a final link.  */

      r_type = ELF32_R_TYPE (rel->r_info);
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto = elf_msp430_howto_table + ELF32_R_TYPE (rel->r_info);
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

      r = msp430_final_link_relocate (howto, input_bfd, input_section,
				      contents, rel, relocation);

      if (r != bfd_reloc_ok)
	{
	  const char *msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		  (info, (h ? &h->root : NULL), name, howto->name,
		   (bfd_vma) 0, input_bfd, input_section,
		   rel->r_offset);
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

/* The final processing done just before writing out a MSP430 ELF object
   file.  This gets the MSP430 architecture right based on the machine
   number.  */

static void
bfd_elf_msp430_final_write_processing (bfd * abfd,
				       bfd_boolean linker ATTRIBUTE_UNUSED)
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
    case bfd_mach_msp110:
      val = E_MSP430_MACH_MSP430x11x1;
      break;

    case bfd_mach_msp11:
      val = E_MSP430_MACH_MSP430x11;
      break;

    case bfd_mach_msp12:
      val = E_MSP430_MACH_MSP430x12;
      break;

    case bfd_mach_msp13:
      val = E_MSP430_MACH_MSP430x13;
      break;

    case bfd_mach_msp14:
      val = E_MSP430_MACH_MSP430x14;
      break;

    case bfd_mach_msp15:
      val = E_MSP430_MACH_MSP430x15;
      break;

    case bfd_mach_msp16:
      val = E_MSP430_MACH_MSP430x16;
      break;

    case bfd_mach_msp31:
      val = E_MSP430_MACH_MSP430x31;
      break;

    case bfd_mach_msp32:
      val = E_MSP430_MACH_MSP430x32;
      break;

    case bfd_mach_msp33:
      val = E_MSP430_MACH_MSP430x33;
      break;

    case bfd_mach_msp41:
      val = E_MSP430_MACH_MSP430x41;
      break;

    case bfd_mach_msp42:
      val = E_MSP430_MACH_MSP430x42;
      break;

    case bfd_mach_msp43:
      val = E_MSP430_MACH_MSP430x43;
      break;

    case bfd_mach_msp44:
      val = E_MSP430_MACH_MSP430x44;
      break;
    }

  elf_elfheader (abfd)->e_machine = EM_MSP430;
  elf_elfheader (abfd)->e_flags &= ~EF_MSP430_MACH;
  elf_elfheader (abfd)->e_flags |= val;
}

/* Set the right machine number.  */

static bfd_boolean
elf32_msp430_object_p (bfd * abfd)
{
  int e_set = bfd_mach_msp14;

  if (elf_elfheader (abfd)->e_machine == EM_MSP430
      || elf_elfheader (abfd)->e_machine == EM_MSP430_OLD)
    {
      int e_mach = elf_elfheader (abfd)->e_flags & EF_MSP430_MACH;

      switch (e_mach)
	{
	default:
	case E_MSP430_MACH_MSP430x11:
	  e_set = bfd_mach_msp11;
	  break;

	case E_MSP430_MACH_MSP430x11x1:
	  e_set = bfd_mach_msp110;
	  break;

	case E_MSP430_MACH_MSP430x12:
	  e_set = bfd_mach_msp12;
	  break;

	case E_MSP430_MACH_MSP430x13:
	  e_set = bfd_mach_msp13;
	  break;

	case E_MSP430_MACH_MSP430x14:
	  e_set = bfd_mach_msp14;
	  break;

	case E_MSP430_MACH_MSP430x15:
	  e_set = bfd_mach_msp15;
	  break;

	case E_MSP430_MACH_MSP430x16:
	  e_set = bfd_mach_msp16;
	  break;

	case E_MSP430_MACH_MSP430x31:
	  e_set = bfd_mach_msp31;
	  break;

	case E_MSP430_MACH_MSP430x32:
	  e_set = bfd_mach_msp32;
	  break;

	case E_MSP430_MACH_MSP430x33:
	  e_set = bfd_mach_msp33;
	  break;

	case E_MSP430_MACH_MSP430x41:
	  e_set = bfd_mach_msp41;
	  break;

	case E_MSP430_MACH_MSP430x42:
	  e_set = bfd_mach_msp42;
	  break;

	case E_MSP430_MACH_MSP430x43:
	  e_set = bfd_mach_msp43;
	  break;

	case E_MSP430_MACH_MSP430x44:
	  e_set = bfd_mach_msp44;
	  break;
	}
    }

  return bfd_default_set_arch_mach (abfd, bfd_arch_msp430, e_set);
}

static void
elf32_msp430_post_process_headers (bfd * abfd,
				   struct bfd_link_info * link_info ATTRIBUTE_UNUSED)
{
  Elf_Internal_Ehdr * i_ehdrp;	/* ELF file header, internal form.  */

  i_ehdrp = elf_elfheader (abfd);

#ifndef ELFOSABI_STANDALONE
#define ELFOSABI_STANDALONE	255
#endif

  i_ehdrp->e_ident[EI_OSABI] = ELFOSABI_STANDALONE;
}

/* These functions handle relaxing for the msp430.
   Relaxation required only in two cases:
    - Bad hand coding like jumps from one section to another or
      from file to file.
    - Sibling calls. This will affect onlu 'jump label' polymorph. Without
      relaxing this enlarges code by 2 bytes. Sibcalls implemented but
      do not work in gcc's port by the reason I do not know.
   Anyway, if a relaxation required, user should pass -relax option to the
   linker.

   There are quite a few relaxing opportunities available on the msp430:

   ================================================================

   1. 3 words -> 1 word

   eq      ==      jeq label    		jne +4; br lab
   ne      !=      jne label    		jeq +4; br lab
   lt      <       jl  label    		jge +4; br lab
   ltu     <       jlo label    		lhs +4; br lab
   ge      >=      jge label    		jl  +4; br lab
   geu     >=      jhs label    		jlo +4; br lab

   2. 4 words -> 1 word

   ltn     <       jn                      jn  +2; jmp +4; br lab

   3. 4 words -> 2 words

   gt      >       jeq +2; jge label       jeq +6; jl  +4; br label
   gtu     >       jeq +2; jhs label       jeq +6; jlo +4; br label

   4. 4 words -> 2 words and 2 labels

   leu     <=      jeq label; jlo label    jeq +2; jhs +4; br label
   le      <=      jeq label; jl  label    jeq +2; jge +4; br label
   =================================================================

   codemap for first cases is (labels masked ):
	      eq:	0x2002,0x4010,0x0000 -> 0x2400
	      ne:	0x2402,0x4010,0x0000 -> 0x2000
	      lt:	0x3402,0x4010,0x0000 -> 0x3800
	      ltu:	0x2c02,0x4010,0x0000 -> 0x2800
	      ge:	0x3802,0x4010,0x0000 -> 0x3400
	      geu:	0x2802,0x4010,0x0000 -> 0x2c00

  second case:
	      ltn:	0x3001,0x3c02,0x4010,0x0000 -> 0x3000

  third case:
	      gt:	0x2403,0x3802,0x4010,0x0000 -> 0x2401,0x3400
	      gtu:	0x2403,0x2802,0x4010,0x0000 -> 0x2401,0x2c00

  fourth case:
	      leu:	0x2401,0x2c02,0x4010,0x0000 -> 0x2400,0x2800
	      le:	0x2401,0x3402,0x4010,0x0000 -> 0x2400,0x3800

  Unspecified case :)
	      jump:	0x4010,0x0000 -> 0x3c00.  */

#define NUMB_RELAX_CODES	12
static struct rcodes_s
{
  int f0, f1;			/* From code.  */
  int t0, t1;			/* To code.  */
  int labels;			/* Position of labels: 1 - one label at first
				   word, 2 - one at second word, 3 - two
				   labels at both.  */
  int cdx;			/* Words to match.  */
  int bs;			/* Shrink bytes.  */
  int off;			/* Offset from old label for new code.  */
  int ncl;			/* New code length.  */
} rcode[] =
{/*                               lab,cdx,bs,off,ncl */
  { 0x0000, 0x0000, 0x3c00, 0x0000, 1, 0, 2, 2,	 2},	/* jump */
  { 0x0000, 0x2002, 0x2400, 0x0000, 1, 1, 4, 4,	 2},	/* eq */
  { 0x0000, 0x2402, 0x2000, 0x0000, 1, 1, 4, 4,	 2},	/* ne */
  { 0x0000, 0x3402, 0x3800, 0x0000, 1, 1, 4, 4,	 2},	/* lt */
  { 0x0000, 0x2c02, 0x2800, 0x0000, 1, 1, 4, 4,	 2},	/* ltu */
  { 0x0000, 0x3802, 0x3400, 0x0000, 1, 1, 4, 4,	 2},	/* ge */
  { 0x0000, 0x2802, 0x2c00, 0x0000, 1, 1, 4, 4,	 2},	/* geu */
  { 0x3001, 0x3c02, 0x3000, 0x0000, 1, 2, 6, 6,	 2},	/* ltn */
  { 0x2403, 0x3802, 0x2401, 0x3400, 2, 2, 4, 6,	 4},	/* gt */
  { 0x2403, 0x2802, 0x2401, 0x2c00, 2, 2, 4, 6,	 4},	/* gtu */
  { 0x2401, 0x2c02, 0x2400, 0x2800, 3, 2, 4, 6,	 4},	/* leu , 2 labels */
  { 0x2401, 0x2c02, 0x2400, 0x2800, 3, 2, 4, 6,	 4},	/* le  , 2 labels */
  { 0, 	    0, 	    0, 	    0, 	    0, 0, 0, 0,  0}
};

/* Return TRUE if a symbol exists at the given address.  */

static bfd_boolean
msp430_elf_symbol_address_p (bfd * abfd,
			     asection * sec,
			     Elf_Internal_Sym * isym,
			     bfd_vma addr)
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  Elf_Internal_Sym *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  /* Examine all the local symbols.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  for (isymend = isym + symtab_hdr->sh_info; isym < isymend; isym++)
    if (isym->st_shndx == sec_shndx && isym->st_value == addr)
      return TRUE;

  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
	      - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;

      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec
	  && sym_hash->root.u.def.value == addr)
	return TRUE;
    }

  return FALSE;
}

/* Adjust all local symbols defined as '.section + 0xXXXX' (.section has sec_shndx)
    referenced from current and other sections */
static bfd_boolean
msp430_elf_relax_adjust_locals(bfd * abfd, asection * sec, bfd_vma addr,
    int count, unsigned int sec_shndx, bfd_vma toaddr)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *irel;
  Elf_Internal_Rela *irelend;
  Elf_Internal_Sym *isym;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;
  symtab_hdr = & elf_tdata (abfd)->symtab_hdr;
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      int sidx = ELF32_R_SYM(irel->r_info);
      Elf_Internal_Sym *lsym = isym + sidx;
      
      /* Adjust symbols referenced by .sec+0xXX */
      if (irel->r_addend > addr && irel->r_addend < toaddr 
	  && lsym->st_shndx == sec_shndx)
	irel->r_addend -= count;
    }
  
  return TRUE;
}

/* Delete some bytes from a section while relaxing.  */

static bfd_boolean
msp430_elf_relax_delete_bytes (bfd * abfd, asection * sec, bfd_vma addr,
			       int count)
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel;
  Elf_Internal_Rela *irelend;
  Elf_Internal_Rela *irelalign;
  bfd_vma toaddr;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;
  asection *p;

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
  symtab_hdr = & elf_tdata (abfd)->symtab_hdr;
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      /* Get the new reloc address.  */
      if ((irel->r_offset > addr && irel->r_offset < toaddr))
	irel->r_offset -= count;
    }

  for (p = abfd->sections; p != NULL; p = p->next)
    msp430_elf_relax_adjust_locals(abfd,p,addr,count,sec_shndx,toaddr);
  
  /* Adjust the local symbols defined in this section.  */
  symtab_hdr = & elf_tdata (abfd)->symtab_hdr;
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  for (isymend = isym + symtab_hdr->sh_info; isym < isymend; isym++)
    if (isym->st_shndx == sec_shndx
	&& isym->st_value > addr && isym->st_value < toaddr)
      isym->st_value -= count;

  /* Now adjust the global symbols defined in this section.  */
  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
	      - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;

      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec
	  && sym_hash->root.u.def.value > addr
	  && sym_hash->root.u.def.value < toaddr)
	sym_hash->root.u.def.value -= count;
    }

  return TRUE;
}


static bfd_boolean
msp430_elf_relax_section (bfd * abfd, asection * sec,
			  struct bfd_link_info * link_info,
			  bfd_boolean * again)
{
  Elf_Internal_Shdr * symtab_hdr;
  Elf_Internal_Rela * internal_relocs;
  Elf_Internal_Rela * irel;
  Elf_Internal_Rela * irelend;
  bfd_byte *          contents = NULL;
  Elf_Internal_Sym *  isymbuf = NULL;

  /* Assume nothing changes.  */
  *again = FALSE;

  /* We don't have to do anything for a relocatable link, if
     this section does not have relocs, or if this is not a
     code section.  */
  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0 || (sec->flags & SEC_CODE) == 0)
    return TRUE;

  symtab_hdr = & elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs =
    _bfd_elf_link_read_relocs (abfd, sec, NULL, NULL, link_info->keep_memory);
  if (internal_relocs == NULL)
    goto error_return;

  /* Walk through them looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;

      /* If this isn't something that can be relaxed, then ignore
         this reloc.  */
      if (ELF32_R_TYPE (irel->r_info) != (int) R_MSP430_RL_PCREL)
	continue;

      /* Get the section contents if we haven't done so already.  */
      if (contents == NULL)
	{
	  /* Get cached copy if it exists.  */
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  else if (! bfd_malloc_and_get_section (abfd, sec, &contents))
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
		    + sym_sec->output_section->vma + sym_sec->output_offset);
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

      /* Try to turn a 16bit pc-relative branch into a 10bit pc-relative
         branch.  */
      /* Paranoia? paranoia...  */      
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MSP430_RL_PCREL)
	{
	  bfd_vma value = symval;

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= irel->r_offset;
	  value += irel->r_addend;

	  /* See if the value will fit in 10 bits, note the high value is
	     1016 as the target will be two bytes closer if we are
	     able to relax. */
	  if ((long) value < 1016 && (long) value > -1016)
	    {
	      int code0 = 0, code1 = 0, code2 = 0;
	      int i;
	      struct rcodes_s *rx;

	      /* Get the opcode.  */
	      if (irel->r_offset >= 6)
		code0 = bfd_get_16 (abfd, contents + irel->r_offset - 6);

	      if (irel->r_offset >= 4)
		code1 = bfd_get_16 (abfd, contents + irel->r_offset - 4);

	      code2 = bfd_get_16 (abfd, contents + irel->r_offset - 2);

	      if (code2 != 0x4010)
		continue;

	      /* Check r4 and r3.  */
	      for (i = NUMB_RELAX_CODES - 1; i >= 0; i--)
		{
		  rx = &rcode[i];
		  if (rx->cdx == 2 && rx->f0 == code0 && rx->f1 == code1)
		    break;
		  else if (rx->cdx == 1 && rx->f1 == code1)
		    break;
		  else if (rx->cdx == 0)	/* This is an unconditional jump.  */
		    break;
		}

	      /* Check labels:
		   .Label0:       ; we do not care about this label
		      jeq    +6
		   .Label1:       ; make sure there is no label here
		      jl     +4
		   .Label2:       ; make sure there is no label here
		      br .Label_dst

	         So, if there is .Label1 or .Label2 we cannot relax this code.
	         This actually should not happen, cause for relaxable
		 instructions we use RL_PCREL reloc instead of 16_PCREL.
		 Will change this in the future. */

	      if (rx->cdx > 0
		  && msp430_elf_symbol_address_p (abfd, sec, isymbuf,
						  irel->r_offset - 2))
		continue;
	      if (rx->cdx > 1
		  && msp430_elf_symbol_address_p (abfd, sec, isymbuf,
						  irel->r_offset - 4))
		continue;

	      /* Note that we've changed the relocs, section contents, etc.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      elf_section_data (sec)->this_hdr.contents = contents;
	      symtab_hdr->contents = (unsigned char *) isymbuf;

	      /* Fix the relocation's type.  */
	      if (rx->labels == 3)	/* Handle special cases.  */
		irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_MSP430_2X_PCREL);
	      else
		irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_MSP430_10_PCREL);

	      /* Fix the opcode right way.  */
	      bfd_put_16 (abfd, rx->t0, contents + irel->r_offset - rx->off);
	      if (rx->t1)
		bfd_put_16 (abfd, rx->t1,
			    contents + irel->r_offset - rx->off + 2);

	      /* Delete bytes. */
	      if (!msp430_elf_relax_delete_bytes (abfd, sec,
						  irel->r_offset - rx->off +
						  rx->ncl, rx->bs))
		goto error_return;

	      /* Handle unconditional jumps.  */
	      if (rx->cdx == 0)
		irel->r_offset -= 2;

	      /* That will change things, so, we should relax again.
	         Note that this is not required, and it may be slow.  */
	      *again = TRUE;
	    }
	}
    }

  if (isymbuf != NULL && symtab_hdr->contents != (unsigned char *) isymbuf)
    {
      if (!link_info->keep_memory)
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
      if (!link_info->keep_memory)
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
  if (isymbuf != NULL && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return FALSE;
}


#define ELF_ARCH		bfd_arch_msp430
#define ELF_MACHINE_CODE	EM_MSP430
#define ELF_MACHINE_ALT1	EM_MSP430_OLD
#define ELF_MAXPAGESIZE		1

#define TARGET_LITTLE_SYM       bfd_elf32_msp430_vec
#define TARGET_LITTLE_NAME	"elf32-msp430"

#define elf_info_to_howto	             msp430_info_to_howto_rela
#define elf_info_to_howto_rel	             NULL
#define elf_backend_relocate_section         elf32_msp430_relocate_section
#define elf_backend_gc_mark_hook             elf32_msp430_gc_mark_hook
#define elf_backend_gc_sweep_hook            elf32_msp430_gc_sweep_hook
#define elf_backend_check_relocs             elf32_msp430_check_relocs
#define elf_backend_can_gc_sections          1
#define elf_backend_final_write_processing   bfd_elf_msp430_final_write_processing
#define elf_backend_object_p		     elf32_msp430_object_p
#define elf_backend_post_process_headers     elf32_msp430_post_process_headers
#define bfd_elf32_bfd_relax_section	     msp430_elf_relax_section

#include "elf32-target.h"
