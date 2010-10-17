/* XSTORMY16-specific support for 32-bit ELF.
   Copyright 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "elf/xstormy16.h"
#include "libiberty.h"

/* Forward declarations.  */
static reloc_howto_type * xstormy16_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void xstormy16_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static bfd_reloc_status_type xstormy16_elf_24_reloc
  PARAMS ((bfd *abfd, arelent *reloc_entry, asymbol *symbol,
	   PTR data, asection *input_section, bfd *output_bfd,
	   char **error_message));
static bfd_boolean xstormy16_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_boolean xstormy16_relax_plt_check
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean xstormy16_relax_plt_realloc
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean xstormy16_elf_relax_section
  PARAMS ((bfd *abfd, asection *sec, struct bfd_link_info *link_info,
	   bfd_boolean *again));
static bfd_boolean xstormy16_elf_always_size_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean xstormy16_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static bfd_boolean xstormy16_elf_finish_dynamic_sections
  PARAMS((bfd *, struct bfd_link_info *));
static bfd_boolean xstormy16_elf_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static asection * xstormy16_elf_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));

static reloc_howto_type xstormy16_elf_howto_table [] =
{
  /* This reloc does nothing.  */
  HOWTO (R_XSTORMY16_NONE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_NONE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_XSTORMY16_32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_XSTORMY16_16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 8 bit absolute relocation.  */
  HOWTO (R_XSTORMY16_8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_8",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit pc-relative relocation.  */
  HOWTO (R_XSTORMY16_PC32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_PC32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 16 bit pc-relative relocation.  */
  HOWTO (R_XSTORMY16_PC16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_PC16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* An 8 bit pc-relative relocation.  */
  HOWTO (R_XSTORMY16_PC8,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_PC8",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 12-bit pc-relative relocation suitable for the branch instructions.  */
  HOWTO (R_XSTORMY16_REL_12,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 11,			/* bitsize */
	 TRUE,			/* pc_relative */
	 1,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_REL_12",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0ffe,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 24-bit absolute relocation suitable for the jump instructions.  */
  HOWTO (R_XSTORMY16_24,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 xstormy16_elf_24_reloc,	/* special_function */
	 "R_XSTORMY16_24",	/* name */
	 TRUE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff00ff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 16 bit absolute relocation to a function pointer.  */
  HOWTO (R_XSTORMY16_FPTR16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_FPTR16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Low order 16 bit value of a high memory address.  */
  HOWTO (R_XSTORMY16_LO16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_LO16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* High order 16 bit value of a high memory address.  */
  HOWTO (R_XSTORMY16_HI16,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_HI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
 
  /* A 12 bit absolute relocation.  */
  HOWTO (R_XSTORMY16_12,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_XSTORMY16_12",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0x0fff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
};

static reloc_howto_type xstormy16_elf_howto_table2 [] =
{
  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_XSTORMY16_GNU_VTINHERIT, /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_XSTORMY16_GNU_VTINHERIT", /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_XSTORMY16_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_XSTORMY16_GNU_VTENTRY",   /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

};

/* Map BFD reloc types to XSTORMY16 ELF reloc types.  */

typedef struct xstormy16_reloc_map
{
  bfd_reloc_code_real_type  bfd_reloc_val;
  unsigned int              xstormy16_reloc_val;
  reloc_howto_type *        table;
} reloc_map;

static const reloc_map xstormy16_reloc_map [] =
{
  { BFD_RELOC_NONE,                 R_XSTORMY16_NONE,          xstormy16_elf_howto_table },
  { BFD_RELOC_32,                   R_XSTORMY16_32,            xstormy16_elf_howto_table },
  { BFD_RELOC_16,                   R_XSTORMY16_16,            xstormy16_elf_howto_table },
  { BFD_RELOC_8,                    R_XSTORMY16_8,             xstormy16_elf_howto_table },
  { BFD_RELOC_32_PCREL,             R_XSTORMY16_PC32,          xstormy16_elf_howto_table },
  { BFD_RELOC_16_PCREL,             R_XSTORMY16_PC16,          xstormy16_elf_howto_table },
  { BFD_RELOC_8_PCREL,              R_XSTORMY16_PC8,           xstormy16_elf_howto_table },
  { BFD_RELOC_XSTORMY16_REL_12,     R_XSTORMY16_REL_12,        xstormy16_elf_howto_table },
  { BFD_RELOC_XSTORMY16_24,	    R_XSTORMY16_24,            xstormy16_elf_howto_table },
  { BFD_RELOC_XSTORMY16_FPTR16,	    R_XSTORMY16_FPTR16,        xstormy16_elf_howto_table },
  { BFD_RELOC_LO16,                 R_XSTORMY16_LO16,          xstormy16_elf_howto_table },
  { BFD_RELOC_HI16,                 R_XSTORMY16_HI16,          xstormy16_elf_howto_table },
  { BFD_RELOC_XSTORMY16_12,         R_XSTORMY16_12,            xstormy16_elf_howto_table },
  { BFD_RELOC_VTABLE_INHERIT,       R_XSTORMY16_GNU_VTINHERIT, xstormy16_elf_howto_table2 },
  { BFD_RELOC_VTABLE_ENTRY,         R_XSTORMY16_GNU_VTENTRY,   xstormy16_elf_howto_table2 },
};

static reloc_howto_type *
xstormy16_reloc_type_lookup (abfd, code)
     bfd * abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = ARRAY_SIZE (xstormy16_reloc_map); --i;)
    {
      const reloc_map * entry;

      entry = xstormy16_reloc_map + i;

      if (entry->bfd_reloc_val == code)
	return entry->table + (entry->xstormy16_reloc_val
			       - entry->table[0].type);
    }

  return NULL;
}

/* Set the howto pointer for an XSTORMY16 ELF reloc.  */

static void
xstormy16_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd * abfd ATTRIBUTE_UNUSED;
     arelent * cache_ptr;
     Elf_Internal_Rela * dst;
{
  unsigned int r_type = ELF32_R_TYPE (dst->r_info);

  if (r_type <= (unsigned int) R_XSTORMY16_12)
    cache_ptr->howto = &xstormy16_elf_howto_table [r_type];
  else if (r_type - R_XSTORMY16_GNU_VTINHERIT
	   <= (unsigned int) R_XSTORMY16_GNU_VTENTRY)
    cache_ptr->howto
      = &xstormy16_elf_howto_table2 [r_type - R_XSTORMY16_GNU_VTINHERIT];
  else
    abort ();
}

/* Handle the R_XSTORMY16_24 reloc, which has an odd bit arrangement.  */

static bfd_reloc_status_type
xstormy16_elf_24_reloc (abfd, reloc_entry, symbol, data, input_section,
			 output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation, x;

  if (output_bfd != NULL)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  x = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);
  x &= 0x0000ff00;
  x |= relocation & 0xff;
  x |= (relocation << 8) & 0xffff0000;
  bfd_put_32 (abfd, x, (bfd_byte *) data + reloc_entry->address);

  if (relocation & ~ (bfd_vma) 0xffffff)
    return bfd_reloc_overflow;

  return bfd_reloc_ok;
}

/* We support 16-bit pointers to code above 64k by generating a thunk
   below 64k containing a JMPF instruction to the final address.  We
   cannot, unfortunately, minimize the number of thunks unless the
   -relax switch is given, as otherwise we have no idea where the
   sections will fall in the address space.  */

static bfd_boolean
xstormy16_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  const Elf_Internal_Rela *rel, *relend;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Shdr *symtab_hdr;
  bfd_vma *local_plt_offsets;
  asection *splt;
  bfd *dynobj;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata(abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_plt_offsets = elf_local_got_offsets (abfd);
  splt = NULL;
  dynobj = elf_hash_table(info)->dynobj;

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; ++rel)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      bfd_vma *offset;

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
	  /* This relocation describes a 16-bit pointer to a function.
	     We may need to allocate a thunk in low memory; reserve memory
	     for it now.  */
	case R_XSTORMY16_FPTR16:
	  if (rel->r_addend != 0)
	    {
	      (*info->callbacks->warning)
		(info, _("non-zero addend in @fptr reloc"), 0,
		 abfd, 0, 0);
	    }

	  if (dynobj == NULL)
	    elf_hash_table (info)->dynobj = dynobj = abfd;
	  if (splt == NULL)
	    {
	      splt = bfd_get_section_by_name (dynobj, ".plt");
	      if (splt == NULL)
		{
		  splt = bfd_make_section (dynobj, ".plt");
		  if (splt == NULL
		      || ! bfd_set_section_flags (dynobj, splt,
						  (SEC_ALLOC
						   | SEC_LOAD
						   | SEC_HAS_CONTENTS
						   | SEC_IN_MEMORY
						   | SEC_LINKER_CREATED
						   | SEC_READONLY
						   | SEC_CODE))
		      || ! bfd_set_section_alignment (dynobj, splt, 1))
		    return FALSE;
		}
	    }

	  if (h != NULL)
	    offset = &h->plt.offset;
	  else
	    {
	      if (local_plt_offsets == NULL)
		{
		  size_t size;
		  unsigned int i;

		  size = symtab_hdr->sh_info * sizeof (bfd_vma);
		  local_plt_offsets = (bfd_vma *) bfd_alloc (abfd, size);
		  if (local_plt_offsets == NULL)
		    return FALSE;
		  elf_local_got_offsets (abfd) = local_plt_offsets;

		  for (i = 0; i < symtab_hdr->sh_info; i++)
		    local_plt_offsets[i] = (bfd_vma) -1;
		}
	      offset = &local_plt_offsets[r_symndx];
	    }

	  if (*offset == (bfd_vma) -1)
	    {
	      *offset = splt->_raw_size;
	      splt->_raw_size += 4;
	    }
	  break;

	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
        case R_XSTORMY16_GNU_VTINHERIT:
          if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
        case R_XSTORMY16_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return FALSE;
          break;
	}
    }

  return TRUE;
}

/* A subroutine of xstormy16_elf_relax_section.  If the global symbol H
   is within the low 64k, remove any entry for it in the plt.  */

struct relax_plt_data
{
  asection *splt;
  bfd_boolean *again;
};

static bfd_boolean
xstormy16_relax_plt_check (h, xdata)
     struct elf_link_hash_entry *h;
     PTR xdata;
{
  struct relax_plt_data *data = (struct relax_plt_data *) xdata;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->plt.offset != (bfd_vma) -1)
    {
      bfd_vma address;

      if (h->root.type == bfd_link_hash_undefined
	  || h->root.type == bfd_link_hash_undefweak)
	address = 0;
      else
	address = (h->root.u.def.section->output_section->vma
		   + h->root.u.def.section->output_offset
		   + h->root.u.def.value);

      if (address <= 0xffff)
	{
	  h->plt.offset = -1;
	  data->splt->_cooked_size -= 4;
	  *data->again = TRUE;
	}
    }

  return TRUE;
}

/* A subroutine of xstormy16_elf_relax_section.  If the global symbol H
   previously had a plt entry, give it a new entry offset.  */

static bfd_boolean
xstormy16_relax_plt_realloc (h, xdata)
     struct elf_link_hash_entry *h;
     PTR xdata;
{
  bfd_vma *entry = (bfd_vma *) xdata;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->plt.offset != (bfd_vma) -1)
    {
      h->plt.offset = *entry;
      *entry += 4;
    }

  return TRUE;
}

static bfd_boolean
xstormy16_elf_relax_section (dynobj, splt, info, again)
     bfd *dynobj;
     asection *splt;
     struct bfd_link_info *info;
     bfd_boolean *again;
{
  struct relax_plt_data relax_plt_data;
  bfd *ibfd;

  /* Assume nothing changes.  */
  *again = FALSE;

  if (info->relocatable)
    return TRUE;

  /* We only relax the .plt section at the moment.  */
  if (dynobj != elf_hash_table (info)->dynobj
      || strcmp (splt->name, ".plt") != 0)
    return TRUE;

  /* Quick check for an empty plt.  */
  if (splt->_raw_size == 0)
    return TRUE;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (splt->_cooked_size == 0)
    splt->_cooked_size = splt->_raw_size;

  /* Map across all global symbols; see which ones happen to
     fall in the low 64k.  */
  relax_plt_data.splt = splt;
  relax_plt_data.again = again;
  elf_link_hash_traverse (elf_hash_table (info), xstormy16_relax_plt_check,
			  &relax_plt_data);

  /* Likewise for local symbols, though that's somewhat less convenient
     as we have to walk the list of input bfds and swap in symbol data.  */
  for (ibfd = info->input_bfds; ibfd ; ibfd = ibfd->link_next)
    {
      bfd_vma *local_plt_offsets = elf_local_got_offsets (ibfd);
      Elf_Internal_Shdr *symtab_hdr;
      Elf_Internal_Sym *isymbuf = NULL;
      unsigned int idx;

      if (! local_plt_offsets)
	continue;

      symtab_hdr = &elf_tdata (ibfd)->symtab_hdr;
      if (symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (ibfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    return FALSE;
	}

      for (idx = 0; idx < symtab_hdr->sh_info; ++idx)
	{
	  Elf_Internal_Sym *isym;
	  asection *tsec;
	  bfd_vma address;

	  if (local_plt_offsets[idx] == (bfd_vma) -1)
	    continue;

	  isym = &isymbuf[idx];
	  if (isym->st_shndx == SHN_UNDEF)
	    continue;
	  else if (isym->st_shndx == SHN_ABS)
	    tsec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    tsec = bfd_com_section_ptr;
	  else
	    tsec = bfd_section_from_elf_index (ibfd, isym->st_shndx);

	  address = (tsec->output_section->vma
		     + tsec->output_offset
		     + isym->st_value);
	  if (address <= 0xffff)
	    {
	      local_plt_offsets[idx] = -1;
	      splt->_cooked_size -= 4;
	      *again = TRUE;
	    }
	}

      if (isymbuf != NULL
	  && symtab_hdr->contents != (unsigned char *) isymbuf)
	{
	  if (! info->keep_memory)
	    free (isymbuf);
	  else
	    {
	      /* Cache the symbols for elf_link_input_bfd.  */
	      symtab_hdr->contents = (unsigned char *) isymbuf;
	    }
	}
    }

  /* If we changed anything, walk the symbols again to reallocate
     .plt entry addresses.  */
  if (*again && splt->_cooked_size > 0)
    {
      bfd_vma entry = 0;

      elf_link_hash_traverse (elf_hash_table (info),
			      xstormy16_relax_plt_realloc, &entry);

      for (ibfd = info->input_bfds; ibfd ; ibfd = ibfd->link_next)
	{
	  bfd_vma *local_plt_offsets = elf_local_got_offsets (ibfd);
	  unsigned int nlocals = elf_tdata (ibfd)->symtab_hdr.sh_info;
	  unsigned int idx;

	  if (! local_plt_offsets)
	    continue;

	  for (idx = 0; idx < nlocals; ++idx)
	    if (local_plt_offsets[idx] != (bfd_vma) -1)
	      {
	        local_plt_offsets[idx] = entry;
		entry += 4;
	      }
	}
    }

  splt->_raw_size = splt->_cooked_size;
  return TRUE;
}

static bfd_boolean
xstormy16_elf_always_size_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *splt;

  if (info->relocatable)
    return TRUE;

  dynobj = elf_hash_table (info)->dynobj;
  if (dynobj == NULL)
    return TRUE;

  splt = bfd_get_section_by_name (dynobj, ".plt");
  BFD_ASSERT (splt != NULL);

  splt->contents = (bfd_byte *) bfd_zalloc (dynobj, splt->_raw_size);
  if (splt->contents == NULL)
    return FALSE;

  return TRUE;
}

/* Relocate an XSTORMY16 ELF section.

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
xstormy16_elf_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, local_syms, local_sections)
     bfd *                   output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *  info;
     bfd *                   input_bfd;
     asection *              input_section;
     bfd_byte *              contents;
     Elf_Internal_Rela *     relocs;
     Elf_Internal_Sym *      local_syms;
     asection **             local_sections;
{
  Elf_Internal_Shdr *           symtab_hdr;
  struct elf_link_hash_entry ** sym_hashes;
  Elf_Internal_Rela *           rel;
  Elf_Internal_Rela *           relend;
  bfd *dynobj;
  asection *splt;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  dynobj = elf_hash_table (info)->dynobj;
  splt = NULL;
  if (dynobj != NULL)
    splt = bfd_get_section_by_name (dynobj, ".plt");

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

      r_type = ELF32_R_TYPE (rel->r_info);

      if (   r_type == R_XSTORMY16_GNU_VTINHERIT
	  || r_type == R_XSTORMY16_GNU_VTENTRY)
	continue;

      r_symndx = ELF32_R_SYM (rel->r_info);
      howto  = xstormy16_elf_howto_table + ELF32_R_TYPE (rel->r_info);
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

      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_XSTORMY16_24:
	  {
	    bfd_vma reloc = relocation + rel->r_addend;
	    unsigned int x;

	    x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	    x &= 0x0000ff00;
	    x |= reloc & 0xff;
	    x |= (reloc << 8) & 0xffff0000;
	    bfd_put_32 (input_bfd, x, contents + rel->r_offset);

	    if (reloc & ~0xffffff)
	      r = bfd_reloc_overflow;
	    else
	      r = bfd_reloc_ok;
	    break;
	  }

	case R_XSTORMY16_FPTR16:
	  {
	    bfd_vma *plt_offset;

	    if (h != NULL)
	      plt_offset = &h->plt.offset;
	    else
	      plt_offset = elf_local_got_offsets (input_bfd) + r_symndx;

	    if (relocation <= 0xffff)
	      {
	        /* If the symbol is in range for a 16-bit address, we should
		   have deallocated the plt entry in relax_section.  */
	        BFD_ASSERT (*plt_offset == (bfd_vma) -1);
	      }
	    else
	      {
		/* If the symbol is out of range for a 16-bit address,
		   we must have allocated a plt entry.  */
		BFD_ASSERT (*plt_offset != (bfd_vma) -1);

		/* If this is the first time we've processed this symbol,
		   fill in the plt entry with the correct symbol address.  */
		if ((*plt_offset & 1) == 0)
		  {
		    unsigned int x;

		    x = 0x00000200;  /* jmpf */
		    x |= relocation & 0xff;
		    x |= (relocation << 8) & 0xffff0000;
		    bfd_put_32 (input_bfd, x, splt->contents + *plt_offset);
		    *plt_offset |= 1;
		  }

		relocation = (splt->output_section->vma
			      + splt->output_offset
			      + (*plt_offset & -2));
	      }
	    r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					  contents, rel->r_offset,
					  relocation, 0);
	    break;
	  }

	default:
	  r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents, rel->r_offset,
					relocation, rel->r_addend);
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
		(info, name, input_bfd, input_section, rel->r_offset,
		 TRUE);
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

/* This must exist if dynobj is ever set.  */

static bfd_boolean
xstormy16_elf_finish_dynamic_sections (abfd, info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *splt;

  /* As an extra sanity check, verify that all plt entries have
     been filled in.  */

  if ((dynobj = elf_hash_table (info)->dynobj) != NULL
      && (splt = bfd_get_section_by_name (dynobj, ".plt")) != NULL)
    {
      bfd_byte *contents = splt->contents;
      unsigned int i, size = splt->_raw_size;
      for (i = 0; i < size; i += 4)
	{
	  unsigned int x = bfd_get_32 (dynobj, contents + i);
	  BFD_ASSERT (x != 0);
	}
    }

  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
xstormy16_elf_gc_mark_hook (sec, info, rel, h, sym)
     asection *                   sec;
     struct bfd_link_info *       info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *          rel;
     struct elf_link_hash_entry * h;
     Elf_Internal_Sym *           sym;
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_XSTORMY16_GNU_VTINHERIT:
	case R_XSTORMY16_GNU_VTENTRY:
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

static bfd_boolean
xstormy16_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *                     abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *    info ATTRIBUTE_UNUSED;
     asection *                sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela * relocs ATTRIBUTE_UNUSED;
{
  return TRUE;
}

#define ELF_ARCH		bfd_arch_xstormy16
#define ELF_MACHINE_CODE	EM_XSTORMY16
#define ELF_MAXPAGESIZE		0x100

#define TARGET_LITTLE_SYM       bfd_elf32_xstormy16_vec
#define TARGET_LITTLE_NAME	"elf32-xstormy16"

#define elf_info_to_howto_rel			NULL
#define elf_info_to_howto			xstormy16_info_to_howto_rela
#define elf_backend_relocate_section		xstormy16_elf_relocate_section
#define elf_backend_gc_mark_hook		xstormy16_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook		xstormy16_elf_gc_sweep_hook
#define elf_backend_check_relocs                xstormy16_elf_check_relocs
#define elf_backend_always_size_sections \
  xstormy16_elf_always_size_sections
#define elf_backend_finish_dynamic_sections \
  xstormy16_elf_finish_dynamic_sections

#define elf_backend_can_gc_sections		1
#define elf_backend_rela_normal			1

#define bfd_elf32_bfd_reloc_type_lookup		xstormy16_reloc_type_lookup
#define bfd_elf32_bfd_relax_section		xstormy16_elf_relax_section

#include "elf32-target.h"
