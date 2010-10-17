/* D10V-specific support for 32-bit ELF
   Copyright 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.
   Contributed by Martin Hunt (hunt@cygnus.com).

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
#include "elf/d10v.h"

static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void d10v_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static asection * elf32_d10v_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static bfd_boolean elf32_d10v_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_boolean elf32_d10v_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_vma extract_rel_addend
  PARAMS ((bfd *, bfd_byte *, reloc_howto_type *));
static void insert_rel_addend
  PARAMS ((bfd *, bfd_byte *, reloc_howto_type *, bfd_vma));
static bfd_boolean elf32_d10v_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *,
	   bfd_byte *, Elf_Internal_Rela *, Elf_Internal_Sym *,
	   asection **));

/* Use REL instead of RELA to save space.  */
#define USE_REL	1

static reloc_howto_type elf_d10v_howto_table[] =
  {
    /* This reloc does nothing.  */
    HOWTO (R_D10V_NONE,		/* type */
	   0,			/* rightshift */
	   2,			/* size (0 = byte, 1 = short, 2 = long) */
	   32,			/* bitsize */
	   FALSE,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont, /* complain_on_overflow */
	   bfd_elf_generic_reloc, /* special_function */
	   "R_D10V_NONE",	/* name */
	   FALSE,		/* partial_inplace */
	   0,			/* src_mask */
	   0,			/* dst_mask */
	   FALSE),		/* pcrel_offset */

    /* An PC Relative 10-bit relocation, shifted by 2  */
    /* right container */
    HOWTO (R_D10V_10_PCREL_R,	/* type */
	   2,	                /* rightshift */
	   2,	                /* size (0 = byte, 1 = short, 2 = long) */
	   7,	                /* bitsize */
	   TRUE,	        /* pc_relative */
	   0,	                /* bitpos */
	   complain_overflow_bitfield, /* complain_on_overflow */
	   bfd_elf_generic_reloc, /* special_function */
	   "R_D10V_10_PCREL_R",	/* name */
	   FALSE,	        /* partial_inplace */
	   0xff,		/* src_mask */
	   0xff,   		/* dst_mask */
	   TRUE),		/* pcrel_offset */

    /* An PC Relative 10-bit relocation, shifted by 2  */
    /* left container */
    HOWTO (R_D10V_10_PCREL_L,	/* type */
	   2,	                /* rightshift */
	   2,	                /* size (0 = byte, 1 = short, 2 = long) */
	   7,	                /* bitsize */
	   TRUE,	        /* pc_relative */
	   15,	                /* bitpos */
	   complain_overflow_bitfield, /* complain_on_overflow */
	   bfd_elf_generic_reloc, /* special_function */
	   "R_D10V_10_PCREL_L",	/* name */
	   FALSE,	        /* partial_inplace */
	   0x07f8000,		/* src_mask */
	   0x07f8000,   	/* dst_mask */
	   TRUE),		/* pcrel_offset */

    /* A 16 bit absolute relocation */
    HOWTO (R_D10V_16,		/* type */
	   0,			/* rightshift */
	   1,			/* size (0 = byte, 1 = short, 2 = long) */
	   16,			/* bitsize */
	   FALSE,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont, /* complain_on_overflow */
	   bfd_elf_generic_reloc, /* special_function */
	   "R_D10V_16",		/* name */
	   FALSE,		/* partial_inplace */
	   0xffff,		/* src_mask */
	   0xffff,		/* dst_mask */
	   FALSE),		/* pcrel_offset */

    /* An 18 bit absolute relocation, right shifted 2 */
    HOWTO (R_D10V_18,		/* type */
	   2,			/* rightshift */
	   1,			/* size (0 = byte, 1 = short, 2 = long) */
	   16,			/* bitsize */
	   FALSE,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont, /* complain_on_overflow */
	   bfd_elf_generic_reloc, /* special_function */
	   "R_D10V_18",		/* name */
	   FALSE,		/* partial_inplace */
	   0xffff,		/* src_mask */
	   0xffff,		/* dst_mask */
	   FALSE),		/* pcrel_offset */

    /* A relative 18 bit relocation, right shifted by 2  */
    HOWTO (R_D10V_18_PCREL,	/* type */
	   2,			/* rightshift */
	   2,			/* size (0 = byte, 1 = short, 2 = long) */
	   15,			/* bitsize */
	   TRUE,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_bitfield, /* complain_on_overflow */
	   bfd_elf_generic_reloc, /* special_function */
	   "R_D10V_18_PCREL",	/* name */
	   FALSE,		/* partial_inplace */
	   0xffff,		/* src_mask */
	   0xffff,		/* dst_mask */
	   TRUE),			/* pcrel_offset */

    /* A 32 bit absolute relocation */
    HOWTO (R_D10V_32,		/* type */
	   0,			/* rightshift */
	   2,			/* size (0 = byte, 1 = short, 2 = long) */
	   32,			/* bitsize */
	   FALSE,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont, /* complain_on_overflow */
	   bfd_elf_generic_reloc, /* special_function */
	   "R_D10V_32",		/* name */
	   FALSE,		/* partial_inplace */
	   0xffffffff,		/* src_mask */
	   0xffffffff,		/* dst_mask */
	   FALSE),		/* pcrel_offset */

    /* GNU extension to record C++ vtable hierarchy */
    HOWTO (R_D10V_GNU_VTINHERIT, /* type */
	   0,                     /* rightshift */
	   2,                     /* size (0 = byte, 1 = short, 2 = long) */
	   0,                     /* bitsize */
	   FALSE,                 /* pc_relative */
	   0,                     /* bitpos */
	   complain_overflow_dont, /* complain_on_overflow */
	   NULL,                  /* special_function */
	   "R_D10V_GNU_VTINHERIT", /* name */
	   FALSE,                 /* partial_inplace */
	   0,                     /* src_mask */
	   0,                     /* dst_mask */
	   FALSE),                /* pcrel_offset */

    /* GNU extension to record C++ vtable member usage */
    HOWTO (R_D10V_GNU_VTENTRY,     /* type */
	   0,                     /* rightshift */
	   2,                     /* size (0 = byte, 1 = short, 2 = long) */
	   0,                     /* bitsize */
	   FALSE,                 /* pc_relative */
	   0,                     /* bitpos */
	   complain_overflow_dont, /* complain_on_overflow */
	   _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
	   "R_D10V_GNU_VTENTRY",   /* name */
	   FALSE,                 /* partial_inplace */
	   0,                     /* src_mask */
	   0,                     /* dst_mask */
	   FALSE),                /* pcrel_offset */
  };

/* Map BFD reloc types to D10V ELF reloc types.  */

struct d10v_reloc_map
  {
    bfd_reloc_code_real_type bfd_reloc_val;
    unsigned char elf_reloc_val;
  };

static const struct d10v_reloc_map d10v_reloc_map[] =
  {
    { BFD_RELOC_NONE, R_D10V_NONE, },
    { BFD_RELOC_D10V_10_PCREL_R, R_D10V_10_PCREL_R },
    { BFD_RELOC_D10V_10_PCREL_L, R_D10V_10_PCREL_L },
    { BFD_RELOC_16, R_D10V_16 },
    { BFD_RELOC_D10V_18, R_D10V_18 },
    { BFD_RELOC_D10V_18_PCREL, R_D10V_18_PCREL },
    { BFD_RELOC_32, R_D10V_32 },
    { BFD_RELOC_VTABLE_INHERIT, R_D10V_GNU_VTINHERIT },
    { BFD_RELOC_VTABLE_ENTRY, R_D10V_GNU_VTENTRY },
  };

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0;
       i < sizeof (d10v_reloc_map) / sizeof (struct d10v_reloc_map);
       i++)
    {
      if (d10v_reloc_map[i].bfd_reloc_val == code)
	return &elf_d10v_howto_table[d10v_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Set the howto pointer for an D10V ELF reloc.  */

static void
d10v_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_D10V_max);
  cache_ptr->howto = &elf_d10v_howto_table[r_type];
}

static asection *
elf32_d10v_gc_mark_hook (sec, info, rel, h, sym)
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
      case R_D10V_GNU_VTINHERIT:
      case R_D10V_GNU_VTENTRY:
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

static bfd_boolean
elf32_d10v_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  /* we don't use got and plt entries for d10v */
  return TRUE;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
elf32_d10v_check_relocs (abfd, info, sec, relocs)
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

      switch (ELF32_R_TYPE (rel->r_info))
        {
        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_D10V_GNU_VTINHERIT:
          if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_D10V_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;
        }
    }

  return TRUE;
}

static bfd_vma
extract_rel_addend (abfd, where, howto)
     bfd *abfd;
     bfd_byte *where;
     reloc_howto_type *howto;
{
  bfd_vma insn, val;

  switch (howto->size)
    {
    case 0:
      insn = bfd_get_8 (abfd, where);
      break;
    case 1:
      insn = bfd_get_16 (abfd, where);
      break;
    case 2:
      insn = bfd_get_32 (abfd, where);
      break;
    default:
      abort ();
    }

  val = (insn & howto->dst_mask) >> howto->bitpos << howto->rightshift;
  /* We should really be testing for signed addends here, but we don't
     have that info directly in the howto.  */
  if (howto->pc_relative)
    {
      bfd_vma sign;
      sign = howto->dst_mask & (~howto->dst_mask >> 1 | ~(-(bfd_vma) 1 >> 1));
      sign = sign >> howto->bitpos << howto->rightshift;
      val = (val ^ sign) - sign;
    }
  return val;
}

static void
insert_rel_addend (abfd, where, howto, addend)
     bfd *abfd;
     bfd_byte *where;
     reloc_howto_type *howto;
     bfd_vma addend;
{
  bfd_vma insn;

  addend = (addend >> howto->rightshift << howto->bitpos) & howto->dst_mask;
  insn = ~howto->dst_mask;
  switch (howto->size)
    {
    case 0:
      insn &= bfd_get_8 (abfd, where);
      insn |= addend;
      bfd_put_8 (abfd, insn, where);
      break;
    case 1:
      insn &= bfd_get_16 (abfd, where);
      insn |= addend;
      bfd_put_16 (abfd, insn, where);
      break;
    case 2:
      insn &= bfd_get_32 (abfd, where);
      insn |= addend;
      bfd_put_32 (abfd, insn, where);
      break;
    default:
      abort ();
    }
}

/* Relocate a D10V ELF section.  */
static bfd_boolean
elf32_d10v_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Rela *rel, *relend;
  const char *name;

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

      if (r_type == R_D10V_GNU_VTENTRY
          || r_type == R_D10V_GNU_VTINHERIT )
        continue;

      howto = elf_d10v_howto_table + r_type;

      if (info->relocatable)
	{
	  bfd_vma val;
	  bfd_byte *where;

	  /* This is a relocatable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx >= symtab_hdr->sh_info)
	    continue;

	  sym = local_syms + r_symndx;
	  if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
	    continue;

	  sec = local_sections[r_symndx];
	  val = sec->output_offset;
	  if (val == 0)
	    continue;

	  where = contents + rel->r_offset;
	  val += extract_rel_addend (input_bfd, where, howto);
	  insert_rel_addend (input_bfd, where, howto, val);
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
	  if ((sec->flags & SEC_MERGE)
	      && ELF_ST_TYPE (sym->st_info) == STT_SECTION)
	    {
	      asection *msec;
	      bfd_vma addend;
	      bfd_byte *where = contents + rel->r_offset;

	      addend = extract_rel_addend (input_bfd, where, howto);
	      msec = sec;
	      addend = _bfd_elf_rel_local_sym (output_bfd, sym, &msec, addend);
	      addend -= relocation;
	      addend += msec->output_section->vma + msec->output_offset;
	      insert_rel_addend (input_bfd, where, howto, addend);
	    }
	}
      else
	{
	  bfd_boolean unresolved_reloc, warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);
	}

      if (h != NULL)
	name = h->root.root.string;
      else
	{
	  name = (bfd_elf_string_from_elf_section
		  (input_bfd, symtab_hdr->sh_link, sym->st_name));
	  if (name == NULL || *name == '\0')
	    name = bfd_section_name (input_bfd, sec);
	}

      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
                                    contents, rel->r_offset,
                                    relocation, (bfd_vma) 0);

      if (r != bfd_reloc_ok)
	{
	  const char * msg = (const char *) 0;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      if (!((*info->callbacks->reloc_overflow)
		    (info, name, howto->name, (bfd_vma) 0,
		     input_bfd, input_section, rel->r_offset)))
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
	      /* fall through */

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
#define ELF_ARCH		bfd_arch_d10v
#define ELF_MACHINE_CODE	EM_D10V
#define ELF_MACHINE_ALT1	EM_CYGNUS_D10V
#define ELF_MAXPAGESIZE		0x1000

#define TARGET_BIG_SYM          bfd_elf32_d10v_vec
#define TARGET_BIG_NAME		"elf32-d10v"

#define elf_info_to_howto	             0
#define elf_info_to_howto_rel	             d10v_info_to_howto_rel
#define elf_backend_object_p	             0
#define elf_backend_final_write_processing   0
#define elf_backend_gc_mark_hook             elf32_d10v_gc_mark_hook
#define elf_backend_gc_sweep_hook            elf32_d10v_gc_sweep_hook
#define elf_backend_check_relocs             elf32_d10v_check_relocs
#define elf_backend_relocate_section         elf32_d10v_relocate_section
#define elf_backend_can_gc_sections          1

#include "elf32-target.h"
