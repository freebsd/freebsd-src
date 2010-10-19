/* VAX series support for 32-bit ELF
   Copyright 1993, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006 Free Software Foundation, Inc.
   Contributed by Matt Thomas <matt@3am-software.com>.

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
#include "elf/vax.h"

static reloc_howto_type *reloc_type_lookup (bfd *, bfd_reloc_code_real_type);
static void rtype_to_howto (bfd *, arelent *, Elf_Internal_Rela *);
static struct bfd_hash_entry *elf_vax_link_hash_newfunc (struct bfd_hash_entry *,
							 struct bfd_hash_table *,
							 const char *);
static struct bfd_link_hash_table *elf_vax_link_hash_table_create (bfd *);
static bfd_boolean elf_vax_check_relocs (bfd *, struct bfd_link_info *,
					 asection *, const Elf_Internal_Rela *);
static asection *elf_vax_gc_mark_hook (asection *, struct bfd_link_info *,
				       Elf_Internal_Rela *,
				       struct elf_link_hash_entry *,
				       Elf_Internal_Sym *);
static bfd_boolean elf_vax_gc_sweep_hook (bfd *, struct bfd_link_info *,
					  asection *,
					  const Elf_Internal_Rela *);
static bfd_boolean elf_vax_adjust_dynamic_symbol (struct bfd_link_info *,
						  struct elf_link_hash_entry *);
static bfd_boolean elf_vax_size_dynamic_sections (bfd *, struct bfd_link_info *);
static bfd_boolean elf_vax_relocate_section (bfd *, struct bfd_link_info *,
					     bfd *, asection *, bfd_byte *,
					     Elf_Internal_Rela *,
					     Elf_Internal_Sym *, asection **);
static bfd_boolean elf_vax_finish_dynamic_symbol (bfd *, struct bfd_link_info *,
						  struct elf_link_hash_entry *,
						  Elf_Internal_Sym *);
static bfd_boolean elf_vax_finish_dynamic_sections (bfd *,
						    struct bfd_link_info *);

static bfd_boolean elf32_vax_set_private_flags (bfd *, flagword);
static bfd_boolean elf32_vax_merge_private_bfd_data (bfd *, bfd *);
static bfd_boolean elf32_vax_print_private_bfd_data (bfd *, PTR);

static reloc_howto_type howto_table[] = {
  HOWTO (R_VAX_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x00000000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_VAX_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_32",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_VAX_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_VAX_8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_8",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x000000ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_VAX_PC32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_PC32",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_VAX_PC16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_PC16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_VAX_PC8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_PC8",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x000000ff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_VAX_GOT32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_GOT32",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),

  HOWTO (R_VAX_PLT32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_PLT32",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),

  HOWTO (R_VAX_COPY,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_COPY",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_VAX_GLOB_DAT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_GLOB_DAT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_VAX_JMP_SLOT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_JMP_SLOT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_VAX_RELATIVE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_VAX_RELATIVE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_VAX_GNU_VTINHERIT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_VAX_GNU_VTINHERIT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_VAX_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_elf_rel_vtable_reloc_fn, /* special_function */
	 "R_VAX_GNU_VTENTRY",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */
};

static void
rtype_to_howto (bfd *abfd ATTRIBUTE_UNUSED, arelent *cache_ptr,
		Elf_Internal_Rela *dst)
{
  BFD_ASSERT (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_VAX_max);
  cache_ptr->howto = &howto_table[ELF32_R_TYPE(dst->r_info)];
}

#define elf_info_to_howto rtype_to_howto

static const struct
{
  bfd_reloc_code_real_type bfd_val;
  int elf_val;
} reloc_map[] = {
  { BFD_RELOC_NONE, R_VAX_NONE },
  { BFD_RELOC_32, R_VAX_32 },
  { BFD_RELOC_16, R_VAX_16 },
  { BFD_RELOC_8, R_VAX_8 },
  { BFD_RELOC_32_PCREL, R_VAX_PC32 },
  { BFD_RELOC_16_PCREL, R_VAX_PC16 },
  { BFD_RELOC_8_PCREL, R_VAX_PC8 },
  { BFD_RELOC_32_GOT_PCREL, R_VAX_GOT32 },
  { BFD_RELOC_32_PLT_PCREL, R_VAX_PLT32 },
  { BFD_RELOC_NONE, R_VAX_COPY },
  { BFD_RELOC_VAX_GLOB_DAT, R_VAX_GLOB_DAT },
  { BFD_RELOC_VAX_JMP_SLOT, R_VAX_JMP_SLOT },
  { BFD_RELOC_VAX_RELATIVE, R_VAX_RELATIVE },
  { BFD_RELOC_CTOR, R_VAX_32 },
  { BFD_RELOC_VTABLE_INHERIT, R_VAX_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_VAX_GNU_VTENTRY },
};

static reloc_howto_type *
reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED, bfd_reloc_code_real_type code)
{
  unsigned int i;
  for (i = 0; i < sizeof (reloc_map) / sizeof (reloc_map[0]); i++)
    {
      if (reloc_map[i].bfd_val == code)
	return &howto_table[reloc_map[i].elf_val];
    }
  return 0;
}

#define bfd_elf32_bfd_reloc_type_lookup reloc_type_lookup
#define ELF_ARCH bfd_arch_vax
/* end code generated by elf.el */

/* Functions for the VAX ELF linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/usr/libexec/ld.elf_so"

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 12

/* The first entry in a procedure linkage table looks like this.  See
   the SVR4 ABI VAX supplement to see how this works.  */

static const bfd_byte elf_vax_plt0_entry[PLT_ENTRY_SIZE] =
{
  0xdd, 0xef,		/* pushl l^ */
  0, 0, 0, 0,		/* offset to .plt.got + 4 */
  0x17, 0xff,		/* jmp @L^(pc) */
  0, 0, 0, 0,		/* offset to .plt.got + 8 */
};

/* Subsequent entries in a procedure linkage table look like this.  */

static const bfd_byte elf_vax_plt_entry[PLT_ENTRY_SIZE] =
{
  0x40, 0x00,		/* .word ^M<r6> */
  0x16,	0xef,		/* jsb L^(pc) */
  0, 0, 0, 0,		/* replaced with offset to start of .plt  */
  0, 0, 0, 0,		/* index into .rela.plt */
};

/* The VAX linker needs to keep track of the number of relocs that it
   decides to copy in check_relocs for each symbol.  This is so that it
   can discard PC relative relocs if it doesn't need them when linking
   with -Bsymbolic.  We store the information in a field extending the
   regular ELF linker hash table.  */

/* This structure keeps track of the number of PC relative relocs we have
   copied for a given symbol.  */

struct elf_vax_pcrel_relocs_copied
{
  /* Next section.  */
  struct elf_vax_pcrel_relocs_copied *next;
  /* A section in dynobj.  */
  asection *section;
  /* Number of relocs copied in this section.  */
  bfd_size_type count;
};

/* VAX ELF linker hash entry.  */

struct elf_vax_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* Number of PC relative relocs copied for this symbol.  */
  struct elf_vax_pcrel_relocs_copied *pcrel_relocs_copied;

  bfd_vma got_addend;
};

/* VAX ELF linker hash table.  */

struct elf_vax_link_hash_table
{
  struct elf_link_hash_table root;
};

/* Declare this now that the above structures are defined.  */

static bfd_boolean elf_vax_discard_copies (struct elf_vax_link_hash_entry *,
					   PTR);

/* Declare this now that the above structures are defined.  */

static bfd_boolean elf_vax_instantiate_got_entries (struct elf_link_hash_entry *,
						    PTR);

/* Traverse an VAX ELF linker hash table.  */

#define elf_vax_link_hash_traverse(table, func, info)			\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (bfd_boolean (*) (struct elf_link_hash_entry *, PTR)) (func),	\
    (info)))

/* Get the VAX ELF linker hash table from a link_info structure.  */

#define elf_vax_hash_table(p) ((struct elf_vax_link_hash_table *) (p)->hash)

/* Create an entry in an VAX ELF linker hash table.  */

static struct bfd_hash_entry *
elf_vax_link_hash_newfunc (struct bfd_hash_entry *entry,
			   struct bfd_hash_table *table,
			   const char *string)
{
  struct elf_vax_link_hash_entry *ret =
    (struct elf_vax_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = ((struct elf_vax_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct elf_vax_link_hash_entry)));
  if (ret == NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf_vax_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != NULL)
    {
      ret->pcrel_relocs_copied = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create an VAX ELF linker hash table.  */

static struct bfd_link_hash_table *
elf_vax_link_hash_table_create (bfd *abfd)
{
  struct elf_vax_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct elf_vax_link_hash_table);

  ret = bfd_malloc (amt);
  if (ret == NULL)
    return NULL;

  if (!_bfd_elf_link_hash_table_init (&ret->root, abfd,
				      elf_vax_link_hash_newfunc,
				      sizeof (struct elf_vax_link_hash_entry)))
    {
      free (ret);
      return NULL;
    }

  return &ret->root.root;
}

/* Keep vax-specific flags in the ELF header */
static bfd_boolean
elf32_vax_set_private_flags (bfd *abfd, flagword flags)
{
  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */
static bfd_boolean
elf32_vax_merge_private_bfd_data (bfd *ibfd, bfd *obfd)
{
  flagword out_flags;
  flagword in_flags;

  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  in_flags  = elf_elfheader (ibfd)->e_flags;
  out_flags = elf_elfheader (obfd)->e_flags;

  if (!elf_flags_init (obfd))
    {
      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = in_flags;
    }

  return TRUE;
}

/* Display the flags field */
static bfd_boolean
elf32_vax_print_private_bfd_data (bfd *abfd, PTR ptr)
{
  FILE *file = (FILE *) ptr;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  /* Print normal ELF private data.  */
  _bfd_elf_print_private_bfd_data (abfd, ptr);

  /* Ignore init flag - it may not be set, despite the flags field containing valid data.  */

  /* xgettext:c-format */
  fprintf (file, _("private flags = %lx:"), elf_elfheader (abfd)->e_flags);

  if (elf_elfheader (abfd)->e_flags & EF_VAX_NONPIC)
    fprintf (file, _(" [nonpic]"));

  if (elf_elfheader (abfd)->e_flags & EF_VAX_DFLOAT)
    fprintf (file, _(" [d-float]"));

  if (elf_elfheader (abfd)->e_flags & EF_VAX_GFLOAT)
    fprintf (file, _(" [g-float]"));

  fputc ('\n', file);

  return TRUE;
}
/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static bfd_boolean
elf_vax_check_relocs (bfd *abfd, struct bfd_link_info *info, asection *sec,
		      const Elf_Internal_Rela *relocs)
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sgot;
  asection *srelgot;
  asection *sreloc;

  if (info->relocatable)
    return TRUE;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);

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
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}

      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_VAX_GOT32:
	  if (h != NULL
	      && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;

	  /* This symbol requires a global offset table entry.  */

	  if (dynobj == NULL)
	    {
	      /* Create the .got section.  */
	      elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (!_bfd_elf_create_got_section (dynobj, info))
		return FALSE;
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
		  srelgot = bfd_make_section_with_flags (dynobj,
							 ".rela.got",
							 (SEC_ALLOC
							  | SEC_LOAD
							  | SEC_HAS_CONTENTS
							  | SEC_IN_MEMORY
							  | SEC_LINKER_CREATED
							  | SEC_READONLY));
		  if (srelgot == NULL
		      || !bfd_set_section_alignment (dynobj, srelgot, 2))
		    return FALSE;
		}
	    }

	  if (h != NULL)
	    {
	      struct elf_vax_link_hash_entry *eh;

	      eh = (struct elf_vax_link_hash_entry *) h;
	      if (h->got.refcount == -1)
		{
		  h->got.refcount = 1;
		  eh->got_addend = rel->r_addend;
		}
	      else
		{
		  h->got.refcount++;
		  if (eh->got_addend != (bfd_vma) rel->r_addend)
		    (*_bfd_error_handler)
		      (_("%s: warning: GOT addend of %ld to `%s' does not match previous GOT addend of %ld"),
			      bfd_get_filename (abfd), rel->r_addend,
			      h->root.root.string,
			      eh->got_addend);

		}
	    }
	  break;

	case R_VAX_PLT32:
	  /* This symbol requires a procedure linkage table entry.  We
	     actually build the entry in adjust_dynamic_symbol,
             because this might be a case of linking PIC code which is
             never referenced by a dynamic object, in which case we
             don't need to generate a procedure linkage table entry
             after all.  */

	  /* If this is a local symbol, we resolve it directly without
	     creating a procedure linkage table entry.  */
	  if (h == NULL)
	    continue;

	  h->needs_plt = 1;
	  if (h->plt.refcount == -1)
	    h->plt.refcount = 1;
	  else
	    h->plt.refcount++;
	  break;

	case R_VAX_PC8:
	case R_VAX_PC16:
	case R_VAX_PC32:
	  /* If we are creating a shared library and this is not a local
	     symbol, we need to copy the reloc into the shared library.
	     However when linking with -Bsymbolic and this is a global
	     symbol which is defined in an object we are including in the
	     link (i.e., DEF_REGULAR is set), then we can resolve the
	     reloc directly.  At this point we have not seen all the input
	     files, so it is possible that DEF_REGULAR is not set now but
	     will be set later (it is never cleared).  We account for that
	     possibility below by storing information in the
	     pcrel_relocs_copied field of the hash table entry.  */
	  if (!(info->shared
		&& (sec->flags & SEC_ALLOC) != 0
		&& h != NULL
		&& (!info->symbolic
		    || !h->def_regular)))
	    {
	      if (h != NULL)
		{
		  /* Make sure a plt entry is created for this symbol if
		     it turns out to be a function defined by a dynamic
		     object.  */
		  if (h->plt.refcount == -1)
		    h->plt.refcount = 1;
		  else
		    h->plt.refcount++;
		}
	      break;
	    }
	  /* Fall through.  */
	case R_VAX_8:
	case R_VAX_16:
	case R_VAX_32:
	  if (h != NULL)
	    {
	      /* Make sure a plt entry is created for this symbol if it
		 turns out to be a function defined by a dynamic object.  */
	      if (h->plt.refcount == -1)
		h->plt.refcount = 1;
	      else
		h->plt.refcount++;
	    }

	  /* If we are creating a shared library, we need to copy the
	     reloc into the shared library.  */
	  if (info->shared
	      && (sec->flags & SEC_ALLOC) != 0)
	    {
	      /* When creating a shared object, we must copy these
		 reloc types into the output file.  We create a reloc
		 section in dynobj and make room for this reloc.  */
	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (abfd,
			   elf_elfheader (abfd)->e_shstrndx,
			   elf_section_data (sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return FALSE;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (abfd, sec),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      sreloc = bfd_make_section_with_flags (dynobj,
							    name,
							    (SEC_ALLOC
							     | SEC_LOAD
							     | SEC_HAS_CONTENTS
							     | SEC_IN_MEMORY
							     | SEC_LINKER_CREATED
							     | SEC_READONLY));
		      if (sreloc == NULL
			  || !bfd_set_section_alignment (dynobj, sreloc, 2))
			return FALSE;
		    }
		  if (sec->flags & SEC_READONLY)
		    info->flags |= DF_TEXTREL;
		}

	      sreloc->size += sizeof (Elf32_External_Rela);

	      /* If we are linking with -Bsymbolic, we count the number of
		 PC relative relocations we have entered for this symbol,
		 so that we can discard them again if the symbol is later
		 defined by a regular object.  Note that this function is
		 only called if we are using a vaxelf linker hash table,
		 which means that h is really a pointer to an
		 elf_vax_link_hash_entry.  */
	      if ((ELF32_R_TYPE (rel->r_info) == R_VAX_PC8
		   || ELF32_R_TYPE (rel->r_info) == R_VAX_PC16
		   || ELF32_R_TYPE (rel->r_info) == R_VAX_PC32)
		  && info->symbolic)
		{
		  struct elf_vax_link_hash_entry *eh;
		  struct elf_vax_pcrel_relocs_copied *p;

		  eh = (struct elf_vax_link_hash_entry *) h;

		  for (p = eh->pcrel_relocs_copied; p != NULL; p = p->next)
		    if (p->section == sreloc)
		      break;

		  if (p == NULL)
		    {
		      p = ((struct elf_vax_pcrel_relocs_copied *)
			   bfd_alloc (dynobj, (bfd_size_type) sizeof *p));
		      if (p == NULL)
			return FALSE;
		      p->next = eh->pcrel_relocs_copied;
		      eh->pcrel_relocs_copied = p;
		      p->section = sreloc;
		      p->count = 0;
		    }

		  ++p->count;
		}
	    }

	  break;

	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_VAX_GNU_VTINHERIT:
	  if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return FALSE;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_VAX_GNU_VTENTRY:
	  if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return FALSE;
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
elf_vax_gc_mark_hook (asection *sec,
		      struct bfd_link_info *info ATTRIBUTE_UNUSED,
		      Elf_Internal_Rela *rel,
		      struct elf_link_hash_entry *h,
		      Elf_Internal_Sym *sym)
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_VAX_GNU_VTINHERIT:
	case R_VAX_GNU_VTENTRY:
	  break;

	default:
	  switch (h->root.type)
	    {
	    default:
	      break;

	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;
	    }
	}
    }
  else
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

static bfd_boolean
elf_vax_gc_sweep_hook (bfd *abfd, struct bfd_link_info *info, asection *sec,
		       const Elf_Internal_Rela *relocs)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel, *relend;
  bfd *dynobj;

  dynobj = elf_hash_table (info)->dynobj;
  if (dynobj == NULL)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h = NULL;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx >= symtab_hdr->sh_info)
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}

      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_VAX_GOT32:
	  if (h != NULL && h->got.refcount > 0)
	    --h->got.refcount;
	  break;

	case R_VAX_PLT32:
	case R_VAX_PC8:
	case R_VAX_PC16:
	case R_VAX_PC32:
	case R_VAX_8:
	case R_VAX_16:
	case R_VAX_32:
	  if (h != NULL && h->plt.refcount > 0)
	    --h->plt.refcount;
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static bfd_boolean
elf_vax_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  asection *s;
  unsigned int power_of_two;

  dynobj = elf_hash_table (info)->dynobj;

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (dynobj != NULL
	      && (h->needs_plt
		  || h->u.weakdef != NULL
		  || (h->def_dynamic
		      && h->ref_regular
		      && !h->def_regular)));

  /* If this is a function, put it in the procedure linkage table.  We
     will fill in the contents of the procedure linkage table later,
     when we know the address of the .got section.  */
  if (h->type == STT_FUNC
      || h->needs_plt)
    {
      if (! info->shared
	  && !h->def_dynamic
	  && !h->ref_dynamic
	  /* We must always create the plt entry if it was referenced
	     by a PLTxxO relocation.  In this case we already recorded
	     it as a dynamic symbol.  */
	  && h->dynindx == -1)
	{
	  /* This case can occur if we saw a PLTxx reloc in an input
	     file, but the symbol was never referred to by a dynamic
	     object.  In such a case, we don't actually need to build
	     a procedure linkage table, and we can just do a PCxx
	     reloc instead.  */
	  BFD_ASSERT (h->needs_plt);
	  h->plt.offset = (bfd_vma) -1;
	  return TRUE;
	}

      /* GC may have rendered this entry unused.  */
      if (h->plt.refcount <= 0)
	{
	  h->needs_plt = 0;
	  h->plt.offset = (bfd_vma) -1;
	  return TRUE;
	}

      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1)
	{
	  if (! bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);

      /* If this is the first .plt entry, make room for the special
	 first entry.  */
      if (s->size == 0)
	{
	  s->size += PLT_ENTRY_SIZE;
	}

      /* If this symbol is not defined in a regular file, and we are
	 not generating a shared library, then set the symbol to this
	 location in the .plt.  This is required to make function
	 pointers compare as equal between the normal executable and
	 the shared library.  */
      if (!info->shared
	  && !h->def_regular)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = s->size;
	}

      h->plt.offset = s->size;

      /* Make room for this entry.  */
      s->size += PLT_ENTRY_SIZE;

      /* We also need to make an entry in the .got.plt section, which
	 will be placed in the .got section by the linker script.  */

      s = bfd_get_section_by_name (dynobj, ".got.plt");
      BFD_ASSERT (s != NULL);
      s->size += 4;

      /* We also need to make an entry in the .rela.plt section.  */

      s = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (s != NULL);
      s->size += sizeof (Elf32_External_Rela);

      return TRUE;
    }

  /* Reinitialize the plt offset now that it is not used as a reference
     count any more.  */
  h->plt.offset = (bfd_vma) -1;

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->u.weakdef != NULL)
    {
      BFD_ASSERT (h->u.weakdef->root.type == bfd_link_hash_defined
		  || h->u.weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->u.weakdef->root.u.def.section;
      h->root.u.def.value = h->u.weakdef->root.u.def.value;
      return TRUE;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.  */
  if (info->shared)
    return TRUE;

  if (h->size == 0)
    {
      (*_bfd_error_handler) (_("dynamic variable `%s' is zero size"),
			     h->root.root.string);
      return TRUE;
    }

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

  /* We must generate a R_VAX_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      srel = bfd_get_section_by_name (dynobj, ".rela.bss");
      BFD_ASSERT (srel != NULL);
      srel->size += sizeof (Elf32_External_Rela);
      h->needs_copy = 1;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 3)
    power_of_two = 3;

  /* Apply the required alignment.  */
  s->size = BFD_ALIGN (s->size, (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (dynobj, s))
    {
      if (!bfd_set_section_alignment (dynobj, s, power_of_two))
	return FALSE;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->size;

  /* Increment the section size to make room for the symbol.  */
  s->size += h->size;

  return TRUE;
}

/* Set the sizes of the dynamic sections.  */

static bfd_boolean
elf_vax_size_dynamic_sections (bfd *output_bfd, struct bfd_link_info *info)
{
  bfd *dynobj;
  asection *s;
  bfd_boolean plt;
  bfd_boolean relocs;
  bfd_boolean reltext;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (info->executable)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
    }
  else
    {
      /* We may have created entries in the .rela.got and .got sections.
	 However, if we are not creating the dynamic sections, we will
	 not actually use these entries.  Reset the size of .rela.got
	 and .got, which will cause it to get stripped from the output
	 file below.  */
      s = bfd_get_section_by_name (dynobj, ".rela.got");
      if (s != NULL)
	s->size = 0;
      s = bfd_get_section_by_name (dynobj, ".got.plt");
      if (s != NULL)
	s->size = 0;
      s = bfd_get_section_by_name (dynobj, ".got");
      if (s != NULL)
	s->size = 0;
    }

  /* If this is a -Bsymbolic shared link, then we need to discard all PC
     relative relocs against symbols defined in a regular object.  We
     allocated space for them in the check_relocs routine, but we will not
     fill them in in the relocate_section routine.  */
  if (info->shared && info->symbolic)
    elf_vax_link_hash_traverse (elf_vax_hash_table (info),
				elf_vax_discard_copies,
				NULL);

  /* If this is a -Bsymbolic shared link or a static link, we need to
     discard all the got entries we've recorded.  Otherwise, we need to
     instantiate (allocate space for them).  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_vax_instantiate_got_entries,
			  (PTR) info);

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  plt = FALSE;
  relocs = FALSE;
  reltext = FALSE;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      if (strcmp (name, ".plt") == 0)
	{
	  /* Remember whether there is a PLT.  */
	  plt = s->size != 0;
	}
      else if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->size != 0)
	    {
	      asection *target;

	      /* Remember whether there are any reloc sections other
                 than .rela.plt.  */
	      if (strcmp (name, ".rela.plt") != 0)
		{
		  const char *outname;

		  relocs = TRUE;

		  /* If this relocation section applies to a read only
		     section, then we probably need a DT_TEXTREL
		     entry.  .rela.plt is actually associated with
		     .got.plt, which is never readonly.  */
		  outname = bfd_get_section_name (output_bfd,
						  s->output_section);
		  target = bfd_get_section_by_name (output_bfd, outname + 5);
		  if (target != NULL
		      && (target->flags & SEC_READONLY) != 0
		      && (target->flags & SEC_ALLOC) != 0)
		    reltext = TRUE;
		}

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strncmp (name, ".got", 4) != 0
	       && strcmp (name, ".dynbss") != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (s->size == 0)
	{
	  /* If we don't need this section, strip it from the
	     output file.  This is mostly to handle .rela.bss and
	     .rela.plt.  We must create both sections in
	     create_dynamic_sections, because they must be created
	     before the linker maps input sections to output
	     sections.  The linker does that before
	     adjust_dynamic_symbol is called, and it is that
	     function which decides whether anything needs to go
	     into these sections.  */
	  s->flags |= SEC_EXCLUDE;
	  continue;
	}

      if ((s->flags & SEC_HAS_CONTENTS) == 0)
	continue;

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_alloc (dynobj, s->size);
      if (s->contents == NULL)
	return FALSE;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf_vax_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  _bfd_elf_add_dynamic_entry (info, TAG, VAL)

      if (!info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return FALSE;
	}

      if (plt)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return FALSE;
	}

      if (relocs)
	{
	  if (!add_dynamic_entry (DT_RELA, 0)
	      || !add_dynamic_entry (DT_RELASZ, 0)
	      || !add_dynamic_entry (DT_RELAENT, sizeof (Elf32_External_Rela)))
	    return FALSE;
	}

      if (reltext || (info->flags & DF_TEXTREL) != 0)
	{
	  if (!add_dynamic_entry (DT_TEXTREL, 0))
	    return FALSE;
	}
    }
#undef add_dynamic_entry

  return TRUE;
}

/* This function is called via elf_vax_link_hash_traverse if we are
   creating a shared object with -Bsymbolic.  It discards the space
   allocated to copy PC relative relocs against symbols which are defined
   in regular objects.  We allocated space for them in the check_relocs
   routine, but we won't fill them in in the relocate_section routine.  */

static bfd_boolean
elf_vax_discard_copies (struct elf_vax_link_hash_entry *h,
			PTR ignore ATTRIBUTE_UNUSED)
{
  struct elf_vax_pcrel_relocs_copied *s;

  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct elf_vax_link_hash_entry *) h->root.root.u.i.link;

  /* We only discard relocs for symbols defined in a regular object.  */
  if (!h->root.def_regular)
    return TRUE;

  for (s = h->pcrel_relocs_copied; s != NULL; s = s->next)
    s->section->size -= s->count * sizeof (Elf32_External_Rela);

  return TRUE;
}

/* This function is called via elf_link_hash_traverse.  It looks for entries
   that have GOT or PLT (.GOT) references.  If creating a static object or a
   shared object with -Bsymbolic, it resets the reference count back to 0
   and sets the offset to -1 so normal PC32 relocation will be done.  If
   creating a shared object or executable, space in the .got and .rela.got
   will be reserved for the symbol.  */

static bfd_boolean
elf_vax_instantiate_got_entries (struct elf_link_hash_entry *h, PTR infoptr)
{
  struct bfd_link_info *info = (struct bfd_link_info *) infoptr;
  bfd *dynobj;
  asection *sgot;
  asection *srelgot;

  /* We don't care about non-GOT (and non-PLT) entries.  */
  if (h->got.refcount <= 0 && h->plt.refcount <= 0)
    return TRUE;

  dynobj = elf_hash_table (info)->dynobj;
  if (dynobj == NULL)
    return TRUE;

  sgot = bfd_get_section_by_name (dynobj, ".got");
  srelgot = bfd_get_section_by_name (dynobj, ".rela.got");

  if (!elf_hash_table (info)->dynamic_sections_created
      || (info->shared && info->symbolic))
    {
      h->got.refcount = 0;
      h->got.offset = (bfd_vma) -1;
      h->plt.refcount = 0;
      h->plt.offset = (bfd_vma) -1;
    }
  else if (h->got.refcount > 0)
    {
      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1)
	{
	  if (!bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      /* Allocate space in the .got and .rela.got sections.  */
      sgot->size += 4;
      srelgot->size += sizeof (Elf32_External_Rela);
    }

  return TRUE;
}

/* Relocate an VAX ELF section.  */

static bfd_boolean
elf_vax_relocate_section (bfd *output_bfd,
			  struct bfd_link_info *info,
			  bfd *input_bfd,
			  asection *input_section,
			  bfd_byte *contents,
			  Elf_Internal_Rela *relocs,
			  Elf_Internal_Sym *local_syms,
			  asection **local_sections)
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  bfd_vma plt_index;
  bfd_vma got_offset;
  asection *sgot;
  asection *splt;
  asection *sgotplt;
  asection *sreloc;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  if (info->relocatable)
    return TRUE;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  local_got_offsets = elf_local_got_offsets (input_bfd);

  sgot = NULL;
  splt = NULL;
  sgotplt = NULL;
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
      bfd_vma relocation;
      bfd_reloc_status_type r;

      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_VAX_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
      howto = howto_table + r_type;

      /* This is a final link.  */
      r_symndx = ELF32_R_SYM (rel->r_info);
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
	  bfd_boolean unresolved_reloc;
	  bfd_boolean warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);

	  if ((h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	      && ((r_type == R_VAX_PLT32
		   && h->plt.offset != (bfd_vma) -1
		   && elf_hash_table (info)->dynamic_sections_created)
		  || (r_type == R_VAX_GOT32
		      && strcmp (h->root.root.string,
				 "_GLOBAL_OFFSET_TABLE_") != 0
		      && elf_hash_table (info)->dynamic_sections_created
		      && (! info->shared
			  || (! info->symbolic && h->dynindx != -1)
			  || !h->def_regular))
		  || (info->shared
		      && ((! info->symbolic && h->dynindx != -1)
			  || !h->def_regular)
		      && ((input_section->flags & SEC_ALLOC) != 0
			  /* DWARF will emit R_VAX_32 relocations in its
			     sections against symbols defined externally
			     in shared libraries.  We can't do anything
			     with them here.  */

			  || ((input_section->flags & SEC_DEBUGGING) != 0
			      && h->def_dynamic))
		      && (r_type == R_VAX_8
			  || r_type == R_VAX_16
			  || r_type == R_VAX_32
			  || r_type == R_VAX_PC8
			  || r_type == R_VAX_PC16
			  || r_type == R_VAX_PC32))))
	    /* In these cases, we don't need the relocation
	       value.  We check specially because in some
	       obscure cases sec->output_section will be NULL.  */
	    relocation = 0;
	}

      switch (r_type)
	{
	case R_VAX_GOT32:
	  /* Relocation is to the address of the entry for this symbol
	     in the global offset table.  */
	  if (h == NULL || h->got.offset == (bfd_vma) -1)
	    break;

	  /* Relocation is the offset of the entry for this symbol in
	     the global offset table.  */

	  {
	    bfd_vma off;

	    if (sgot == NULL)
	      {
		sgot = bfd_get_section_by_name (dynobj, ".got");
		BFD_ASSERT (sgot != NULL);
	      }

	    BFD_ASSERT (h != NULL);
	    off = h->got.offset;
	    BFD_ASSERT (off != (bfd_vma) -1);
	    BFD_ASSERT (off < sgot->size);

	    if (info->shared
		&& h->dynindx == -1
		&& h->def_regular)
	      {
		/* The symbol was forced to be local
		   because of a version file..  We must initialize
		   this entry in the global offset table.  Since
		   the offset must always be a multiple of 4, we
		   use the least significant bit to record whether
		   we have initialized it already.

		   When doing a dynamic link, we create a .rela.got
		   relocation entry to initialize the value.  This
		   is done in the finish_dynamic_symbol routine.  */
		if ((off & 1) != 0)
		  off &= ~1;
		else
		  {
		    bfd_put_32 (output_bfd, relocation + rel->r_addend,
				sgot->contents + off);
		    h->got.offset |= 1;
		  }
	      } else {
		bfd_put_32 (output_bfd, rel->r_addend, sgot->contents + off);
	      }

	    relocation = sgot->output_offset + off;
	    /* The GOT relocation uses the addend.  */
	    rel->r_addend = 0;

	    /* Change the reference to be indirect.  */
	    contents[rel->r_offset - 1] |= 0x10;
	    relocation += sgot->output_section->vma;
	  }
	  break;

	case R_VAX_PLT32:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  /* Resolve a PLTxx reloc against a local symbol directly,
	     without using the procedure linkage table.  */
	  if (h == NULL)
	    break;

	  if (h->plt.offset == (bfd_vma) -1
	      || !elf_hash_table (info)->dynamic_sections_created)
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

	  if (sgotplt == NULL)
	    {
	      sgotplt = bfd_get_section_by_name (dynobj, ".got.plt");
	      BFD_ASSERT (splt != NULL);
	    }

	  plt_index = h->plt.offset / PLT_ENTRY_SIZE - 1;

	  /* Get the offset into the .got table of the entry that
	     corresponds to this function.  Each .got entry is 4 bytes.
	     The first two are reserved.  */
	  got_offset = (plt_index + 3) * 4;

	  /* We want the relocate to point into the .got.plt instead
	     of the plt itself.  */
	  relocation = (sgotplt->output_section->vma
			+ sgotplt->output_offset
			+ got_offset);
	  contents[rel->r_offset-1] |= 0x10; /* make indirect */
	  if (rel->r_addend == 2)
	    {
	      h->plt.offset |= 1;
	    }
	  else if (rel->r_addend != 0)
	    (*_bfd_error_handler)
	      (_("%s: warning: PLT addend of %d to `%s' from %s section ignored"),
		      bfd_get_filename (input_bfd), rel->r_addend,
		      h->root.root.string,
		      bfd_get_section_name (input_bfd, input_section));
	  rel->r_addend = 0;

	  break;

	case R_VAX_PC8:
	case R_VAX_PC16:
	case R_VAX_PC32:
	  if (h == NULL)
	    break;
	  /* Fall through.  */
	case R_VAX_8:
	case R_VAX_16:
	case R_VAX_32:
	  if (info->shared
	      && r_symndx != 0
	      && (input_section->flags & SEC_ALLOC) != 0
	      && ((r_type != R_VAX_PC8
		   && r_type != R_VAX_PC16
		   && r_type != R_VAX_PC32)
		  || (!info->symbolic
		      || !h->def_regular)))
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;
	      bfd_boolean skip, relocate;

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
		    return FALSE;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (input_bfd,
							       input_section),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  BFD_ASSERT (sreloc != NULL);
		}

	      skip = FALSE;
	      relocate = FALSE;

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info, input_section,
					 rel->r_offset);
	      if (outrel.r_offset == (bfd_vma) -1)
		skip = TRUE;
	      if (outrel.r_offset == (bfd_vma) -2)
		skip = TRUE, relocate = TRUE;
	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      if (skip)
		  memset (&outrel, 0, sizeof outrel);
	      /* h->dynindx may be -1 if the symbol was marked to
                 become local.  */
	      else if (h != NULL
		       && ((! info->symbolic && h->dynindx != -1)
			   || !h->def_regular))
		{
		  BFD_ASSERT (h->dynindx != -1);
		  outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = relocation + rel->r_addend;
		}
	      else
		{
		  if (r_type == R_VAX_32)
		    {
		      relocate = TRUE;
		      outrel.r_info = ELF32_R_INFO (0, R_VAX_RELATIVE);
		      BFD_ASSERT (bfd_get_signed_32 (input_bfd,
						     &contents[rel->r_offset]) == 0);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		  else
		    {
		      long indx;

		      if (bfd_is_abs_section (sec))
			indx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return FALSE;
			}
		      else
			{
			  asection *osec;

			  osec = sec->output_section;
			  indx = elf_section_data (osec)->dynindx;
			  BFD_ASSERT (indx > 0);
			}

		      outrel.r_info = ELF32_R_INFO (indx, r_type);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		}

	      if (!strcmp (bfd_get_section_name (input_bfd, input_section),
			   ".text") != 0 ||
		  (info->shared
		   && ELF32_R_TYPE(outrel.r_info) != R_VAX_32
		   && ELF32_R_TYPE(outrel.r_info) != R_VAX_RELATIVE
		   && ELF32_R_TYPE(outrel.r_info) != R_VAX_COPY
		   && ELF32_R_TYPE(outrel.r_info) != R_VAX_JMP_SLOT
		   && ELF32_R_TYPE(outrel.r_info) != R_VAX_GLOB_DAT))
		{
		  if (h != NULL)
		    (*_bfd_error_handler)
		      (_("%s: warning: %s relocation against symbol `%s' from %s section"),
		      bfd_get_filename (input_bfd), howto->name,
		      h->root.root.string,
		      bfd_get_section_name (input_bfd, input_section));
		  else
		    (*_bfd_error_handler)
		      (_("%s: warning: %s relocation to 0x%x from %s section"),
		      bfd_get_filename (input_bfd), howto->name,
		      outrel.r_addend,
		      bfd_get_section_name (input_bfd, input_section));
		}
	      loc = sreloc->contents;
	      loc += sreloc->reloc_count++ * sizeof (Elf32_External_Rela);
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);

	      /* This reloc will be computed at runtime, so there's no
                 need to do anything now, except for R_VAX_32
                 relocations that have been turned into
                 R_VAX_RELATIVE.  */
	      if (!relocate)
		continue;
	    }

	  break;

	case R_VAX_GNU_VTINHERIT:
	case R_VAX_GNU_VTENTRY:
	  /* These are no-ops in the end.  */
	  continue;

	default:
	  break;
	}

      /* VAX PCREL relocations are from the end of relocation, not the start.
         So subtract the difference from the relocation amount since we can't
         add it to the offset.  */
      if (howto->pc_relative && howto->pcrel_offset)
	relocation -= bfd_get_reloc_size(howto);

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
		  name = NULL;
		else
		  {
		    name = bfd_elf_string_from_elf_section (input_bfd,
							    symtab_hdr->sh_link,
							    sym->st_name);
		    if (name == NULL)
		      return FALSE;
		    if (*name == '\0')
		      name = bfd_section_name (input_bfd, sec);
		  }
		if (!(info->callbacks->reloc_overflow
		      (info, (h ? &h->root : NULL), name, howto->name,
		       (bfd_vma) 0, input_bfd, input_section,
		       rel->r_offset)))
		  return FALSE;
	      }
	      break;
	    }
	}
    }

  return TRUE;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static bfd_boolean
elf_vax_finish_dynamic_symbol (bfd *output_bfd, struct bfd_link_info *info,
			       struct elf_link_hash_entry *h,
			       Elf_Internal_Sym *sym)
{
  bfd *dynobj;

  dynobj = elf_hash_table (info)->dynobj;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *sgot;
      asection *srela;
      bfd_vma plt_index;
      bfd_vma got_offset;
      bfd_vma addend;
      Elf_Internal_Rela rela;
      bfd_byte *loc;

      /* This symbol has an entry in the procedure linkage table.  Set
	 it up.  */
      BFD_ASSERT (h->dynindx != -1);

      splt = bfd_get_section_by_name (dynobj, ".plt");
      sgot = bfd_get_section_by_name (dynobj, ".got.plt");
      srela = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (splt != NULL && sgot != NULL && srela != NULL);

      addend = 2 * (h->plt.offset & 1);
      h->plt.offset &= ~1;

      /* Get the index in the procedure linkage table which
	 corresponds to this symbol.  This is the index of this symbol
	 in all the symbols for which we are making plt entries.  The
	 first entry in the procedure linkage table is reserved.  */
      plt_index = h->plt.offset / PLT_ENTRY_SIZE - 1;

      /* Get the offset into the .got table of the entry that
	 corresponds to this function.  Each .got entry is 4 bytes.
	 The first two are reserved.  */
      got_offset = (plt_index + 3) * 4;

      /* Fill in the entry in the procedure linkage table.  */
      memcpy (splt->contents + h->plt.offset, elf_vax_plt_entry,
	          PLT_ENTRY_SIZE);

      /* The offset is relative to the first extension word.  */
      bfd_put_32 (output_bfd,
		  -(h->plt.offset + 8),
		  splt->contents + h->plt.offset + 4);

      bfd_put_32 (output_bfd, plt_index * sizeof (Elf32_External_Rela),
		  splt->contents + h->plt.offset + 8);

      /* Fill in the entry in the global offset table.  */
      bfd_put_32 (output_bfd,
		  (splt->output_section->vma
		   + splt->output_offset
		   + h->plt.offset) + addend,
		  sgot->contents + got_offset);

      /* Fill in the entry in the .rela.plt section.  */
      rela.r_offset = (sgot->output_section->vma
		       + sgot->output_offset
		       + got_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_VAX_JMP_SLOT);
      rela.r_addend = addend;
      loc = srela->contents + plt_index * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);

      if (!h->def_regular)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	}
    }

  if (h->got.offset != (bfd_vma) -1)
    {
      asection *sgot;
      asection *srela;
      Elf_Internal_Rela rela;
      bfd_byte *loc;

      /* This symbol has an entry in the global offset table.  Set it
	 up.  */
      sgot = bfd_get_section_by_name (dynobj, ".got");
      srela = bfd_get_section_by_name (dynobj, ".rela.got");
      BFD_ASSERT (sgot != NULL && srela != NULL);

      rela.r_offset = (sgot->output_section->vma
		       + sgot->output_offset
		       + (h->got.offset &~ 1));

      /* If the symbol was forced to be local because of a version file
	 locally we just want to emit a RELATIVE reloc.  The entry in
	 the global offset table will already have been initialized in
	 the relocate_section function.  */
      if (info->shared
	  && h->dynindx == -1
	  && h->def_regular)
	{
	  rela.r_info = ELF32_R_INFO (0, R_VAX_RELATIVE);
	}
      else
	{
	  rela.r_info = ELF32_R_INFO (h->dynindx, R_VAX_GLOB_DAT);
	}
      rela.r_addend = bfd_get_signed_32 (output_bfd,
					 (sgot->contents
					  + (h->got.offset & ~1)));

      loc = srela->contents;
      loc += srela->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
    }

  if (h->needs_copy)
    {
      asection *s;
      Elf_Internal_Rela rela;
      bfd_byte *loc;

      /* This symbol needs a copy reloc.  Set it up.  */
      BFD_ASSERT (h->dynindx != -1
		  && (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak));

      s = bfd_get_section_by_name (h->root.u.def.section->owner,
				   ".rela.bss");
      BFD_ASSERT (s != NULL);

      rela.r_offset = (h->root.u.def.value
		       + h->root.u.def.section->output_section->vma
		       + h->root.u.def.section->output_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_VAX_COPY);
      rela.r_addend = 0;
      loc = s->contents + s->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || h == elf_hash_table (info)->hgot)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}

/* Finish up the dynamic sections.  */

static bfd_boolean
elf_vax_finish_dynamic_sections (bfd *output_bfd, struct bfd_link_info *info)
{
  bfd *dynobj;
  asection *sgot;
  asection *sdyn;

  dynobj = elf_hash_table (info)->dynobj;

  sgot = bfd_get_section_by_name (dynobj, ".got.plt");
  BFD_ASSERT (sgot != NULL);
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      Elf32_External_Dyn *dyncon, *dynconend;

      splt = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (splt != NULL && sdyn != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  asection *s;

	  bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	    case DT_PLTGOT:
	      name = ".got";
	      goto get_vma;
	    case DT_JMPREL:
	      name = ".rela.plt";
	    get_vma:
	      s = bfd_get_section_by_name (output_bfd, name);
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_PLTRELSZ:
	      s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_val = s->size;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_RELASZ:
	      /* The procedure linkage table relocs (DT_JMPREL) should
		 not be included in the overall relocs (DT_RELA).
		 Therefore, we override the DT_RELASZ entry here to
		 make it not include the JMPREL relocs.  Since the
		 linker script arranges for .rela.plt to follow all
		 other relocation sections, we don't have to worry
		 about changing the DT_RELA entry.  */
	      s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	      if (s != NULL)
		dyn.d_un.d_val -= s->size;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;
	    }
	}

      /* Fill in the first entry in the procedure linkage table.  */
      if (splt->size > 0)
	{
	  memcpy (splt->contents, elf_vax_plt0_entry, PLT_ENTRY_SIZE);
	  bfd_put_32 (output_bfd,
		          (sgot->output_section->vma
		           + sgot->output_offset + 4
		           - (splt->output_section->vma + 6)),
		          splt->contents + 2);
	  bfd_put_32 (output_bfd,
		          (sgot->output_section->vma
		           + sgot->output_offset + 8
		           - (splt->output_section->vma + 12)),
		          splt->contents + 8);
          elf_section_data (splt->output_section)->this_hdr.sh_entsize
           = PLT_ENTRY_SIZE;
	}
    }

  /* Fill in the first three entries in the global offset table.  */
  if (sgot->size > 0)
    {
      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 4);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 8);
    }

  elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;

  return TRUE;
}

#define TARGET_LITTLE_SYM		bfd_elf32_vax_vec
#define TARGET_LITTLE_NAME		"elf32-vax"
#define ELF_MACHINE_CODE		EM_VAX
#define ELF_MAXPAGESIZE			0x1000

#define elf_backend_create_dynamic_sections \
					_bfd_elf_create_dynamic_sections
#define bfd_elf32_bfd_link_hash_table_create \
					elf_vax_link_hash_table_create
#define bfd_elf32_bfd_final_link	bfd_elf_gc_common_final_link

#define elf_backend_check_relocs	elf_vax_check_relocs
#define elf_backend_adjust_dynamic_symbol \
					elf_vax_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
					elf_vax_size_dynamic_sections
#define elf_backend_relocate_section	elf_vax_relocate_section
#define elf_backend_finish_dynamic_symbol \
					elf_vax_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					elf_vax_finish_dynamic_sections
#define elf_backend_gc_mark_hook	elf_vax_gc_mark_hook
#define elf_backend_gc_sweep_hook	elf_vax_gc_sweep_hook
#define bfd_elf32_bfd_merge_private_bfd_data \
                                        elf32_vax_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags \
                                        elf32_vax_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data \
                                        elf32_vax_print_private_bfd_data

#define elf_backend_can_gc_sections	1
#define elf_backend_want_got_plt	1
#define elf_backend_plt_readonly	1
#define elf_backend_want_plt_sym	0
#define elf_backend_got_header_size	16
#define elf_backend_rela_normal		1

#include "elf32-target.h"
