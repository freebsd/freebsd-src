/* Motorola 68k series support for 32-bit ELF
   Copyright 1993, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
   2004 Free Software Foundation, Inc.

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
#include "elf/m68k.h"

static reloc_howto_type *reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void rtype_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static struct bfd_hash_entry *elf_m68k_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct bfd_link_hash_table *elf_m68k_link_hash_table_create
  PARAMS ((bfd *));
static bfd_boolean elf_m68k_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static asection *elf_m68k_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static bfd_boolean elf_m68k_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_boolean elf_m68k_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static bfd_boolean elf_m68k_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean elf_m68k_discard_copies
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean elf_m68k_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static bfd_boolean elf_m68k_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static bfd_boolean elf_m68k_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

static bfd_boolean elf32_m68k_set_private_flags
  PARAMS ((bfd *, flagword));
static bfd_boolean elf32_m68k_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_boolean elf32_m68k_print_private_bfd_data
  PARAMS ((bfd *, PTR));
static enum elf_reloc_type_class elf32_m68k_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));

static reloc_howto_type howto_table[] = {
  HOWTO(R_68K_NONE,       0, 0, 0, FALSE,0, complain_overflow_dont,     bfd_elf_generic_reloc, "R_68K_NONE",      FALSE, 0, 0x00000000,FALSE),
  HOWTO(R_68K_32,         0, 2,32, FALSE,0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_68K_32",        FALSE, 0, 0xffffffff,FALSE),
  HOWTO(R_68K_16,         0, 1,16, FALSE,0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_68K_16",        FALSE, 0, 0x0000ffff,FALSE),
  HOWTO(R_68K_8,          0, 0, 8, FALSE,0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_68K_8",         FALSE, 0, 0x000000ff,FALSE),
  HOWTO(R_68K_PC32,       0, 2,32, TRUE, 0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_68K_PC32",      FALSE, 0, 0xffffffff,TRUE),
  HOWTO(R_68K_PC16,       0, 1,16, TRUE, 0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_PC16",      FALSE, 0, 0x0000ffff,TRUE),
  HOWTO(R_68K_PC8,        0, 0, 8, TRUE, 0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_PC8",       FALSE, 0, 0x000000ff,TRUE),
  HOWTO(R_68K_GOT32,      0, 2,32, TRUE, 0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_68K_GOT32",     FALSE, 0, 0xffffffff,TRUE),
  HOWTO(R_68K_GOT16,      0, 1,16, TRUE, 0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_GOT16",     FALSE, 0, 0x0000ffff,TRUE),
  HOWTO(R_68K_GOT8,       0, 0, 8, TRUE, 0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_GOT8",      FALSE, 0, 0x000000ff,TRUE),
  HOWTO(R_68K_GOT32O,     0, 2,32, FALSE,0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_68K_GOT32O",    FALSE, 0, 0xffffffff,FALSE),
  HOWTO(R_68K_GOT16O,     0, 1,16, FALSE,0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_GOT16O",    FALSE, 0, 0x0000ffff,FALSE),
  HOWTO(R_68K_GOT8O,      0, 0, 8, FALSE,0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_GOT8O",     FALSE, 0, 0x000000ff,FALSE),
  HOWTO(R_68K_PLT32,      0, 2,32, TRUE, 0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_68K_PLT32",     FALSE, 0, 0xffffffff,TRUE),
  HOWTO(R_68K_PLT16,      0, 1,16, TRUE, 0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_PLT16",     FALSE, 0, 0x0000ffff,TRUE),
  HOWTO(R_68K_PLT8,       0, 0, 8, TRUE, 0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_PLT8",      FALSE, 0, 0x000000ff,TRUE),
  HOWTO(R_68K_PLT32O,     0, 2,32, FALSE,0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_68K_PLT32O",    FALSE, 0, 0xffffffff,FALSE),
  HOWTO(R_68K_PLT16O,     0, 1,16, FALSE,0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_PLT16O",    FALSE, 0, 0x0000ffff,FALSE),
  HOWTO(R_68K_PLT8O,      0, 0, 8, FALSE,0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_68K_PLT8O",     FALSE, 0, 0x000000ff,FALSE),
  HOWTO(R_68K_COPY,       0, 0, 0, FALSE,0, complain_overflow_dont,     bfd_elf_generic_reloc, "R_68K_COPY",      FALSE, 0, 0xffffffff,FALSE),
  HOWTO(R_68K_GLOB_DAT,   0, 2,32, FALSE,0, complain_overflow_dont,     bfd_elf_generic_reloc, "R_68K_GLOB_DAT",  FALSE, 0, 0xffffffff,FALSE),
  HOWTO(R_68K_JMP_SLOT,   0, 2,32, FALSE,0, complain_overflow_dont,     bfd_elf_generic_reloc, "R_68K_JMP_SLOT",  FALSE, 0, 0xffffffff,FALSE),
  HOWTO(R_68K_RELATIVE,   0, 2,32, FALSE,0, complain_overflow_dont,     bfd_elf_generic_reloc, "R_68K_RELATIVE",  FALSE, 0, 0xffffffff,FALSE),
  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_68K_GNU_VTINHERIT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_68K_GNU_VTINHERIT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),
  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_68K_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_elf_rel_vtable_reloc_fn, /* special_function */
	 "R_68K_GNU_VTENTRY",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),
};

static void
rtype_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  BFD_ASSERT (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_68K_max);
  cache_ptr->howto = &howto_table[ELF32_R_TYPE(dst->r_info)];
}

#define elf_info_to_howto rtype_to_howto

static const struct
{
  bfd_reloc_code_real_type bfd_val;
  int elf_val;
} reloc_map[] = {
  { BFD_RELOC_NONE, R_68K_NONE },
  { BFD_RELOC_32, R_68K_32 },
  { BFD_RELOC_16, R_68K_16 },
  { BFD_RELOC_8, R_68K_8 },
  { BFD_RELOC_32_PCREL, R_68K_PC32 },
  { BFD_RELOC_16_PCREL, R_68K_PC16 },
  { BFD_RELOC_8_PCREL, R_68K_PC8 },
  { BFD_RELOC_32_GOT_PCREL, R_68K_GOT32 },
  { BFD_RELOC_16_GOT_PCREL, R_68K_GOT16 },
  { BFD_RELOC_8_GOT_PCREL, R_68K_GOT8 },
  { BFD_RELOC_32_GOTOFF, R_68K_GOT32O },
  { BFD_RELOC_16_GOTOFF, R_68K_GOT16O },
  { BFD_RELOC_8_GOTOFF, R_68K_GOT8O },
  { BFD_RELOC_32_PLT_PCREL, R_68K_PLT32 },
  { BFD_RELOC_16_PLT_PCREL, R_68K_PLT16 },
  { BFD_RELOC_8_PLT_PCREL, R_68K_PLT8 },
  { BFD_RELOC_32_PLTOFF, R_68K_PLT32O },
  { BFD_RELOC_16_PLTOFF, R_68K_PLT16O },
  { BFD_RELOC_8_PLTOFF, R_68K_PLT8O },
  { BFD_RELOC_NONE, R_68K_COPY },
  { BFD_RELOC_68K_GLOB_DAT, R_68K_GLOB_DAT },
  { BFD_RELOC_68K_JMP_SLOT, R_68K_JMP_SLOT },
  { BFD_RELOC_68K_RELATIVE, R_68K_RELATIVE },
  { BFD_RELOC_CTOR, R_68K_32 },
  { BFD_RELOC_VTABLE_INHERIT, R_68K_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_68K_GNU_VTENTRY },
};

static reloc_howto_type *
reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
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
#define ELF_ARCH bfd_arch_m68k

/* Functions for the m68k ELF linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/libc.so.1"

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 20

/* The first entry in a procedure linkage table looks like this.  See
   the SVR4 ABI m68k supplement to see how this works.  */

static const bfd_byte elf_m68k_plt0_entry[PLT_ENTRY_SIZE] =
{
  0x2f, 0x3b, 0x01, 0x70, /* move.l (%pc,addr),-(%sp) */
  0, 0, 0, 0,		  /* replaced with offset to .got + 4.  */
  0x4e, 0xfb, 0x01, 0x71, /* jmp ([%pc,addr]) */
  0, 0, 0, 0,		  /* replaced with offset to .got + 8.  */
  0, 0, 0, 0		  /* pad out to 20 bytes.  */
};

/* Subsequent entries in a procedure linkage table look like this.  */

static const bfd_byte elf_m68k_plt_entry[PLT_ENTRY_SIZE] =
{
  0x4e, 0xfb, 0x01, 0x71, /* jmp ([%pc,symbol@GOTPC]) */
  0, 0, 0, 0,		  /* replaced with offset to symbol's .got entry.  */
  0x2f, 0x3c,		  /* move.l #offset,-(%sp) */
  0, 0, 0, 0,		  /* replaced with offset into relocation table.  */
  0x60, 0xff,		  /* bra.l .plt */
  0, 0, 0, 0		  /* replaced with offset to start of .plt.  */
};

#define CPU32_FLAG(abfd)  (elf_elfheader (abfd)->e_flags & EF_CPU32)

#define PLT_CPU32_ENTRY_SIZE 24
/* Procedure linkage table entries for the cpu32 */
static const bfd_byte elf_cpu32_plt0_entry[PLT_CPU32_ENTRY_SIZE] =
{
  0x2f, 0x3b, 0x01, 0x70, /* move.l (%pc,addr),-(%sp) */
  0, 0, 0, 0,             /* replaced with offset to .got + 4.  */
  0x22, 0x7b, 0x01, 0x70, /* moveal %pc@(0xc), %a1 */
  0, 0, 0, 0,             /* replace with offset to .got +8.  */
  0x4e, 0xd1,             /* jmp %a1@ */
  0, 0, 0, 0,             /* pad out to 24 bytes.  */
  0, 0
};

static const bfd_byte elf_cpu32_plt_entry[PLT_CPU32_ENTRY_SIZE] =
{
  0x22, 0x7b, 0x01, 0x70,  /* moveal %pc@(0xc), %a1 */
  0, 0, 0, 0,              /* replaced with offset to symbol's .got entry.  */
  0x4e, 0xd1,              /* jmp %a1@ */
  0x2f, 0x3c,              /* move.l #offset,-(%sp) */
  0, 0, 0, 0,              /* replaced with offset into relocation table.  */
  0x60, 0xff,              /* bra.l .plt */
  0, 0, 0, 0,              /* replaced with offset to start of .plt.  */
  0, 0
};

/* The m68k linker needs to keep track of the number of relocs that it
   decides to copy in check_relocs for each symbol.  This is so that it
   can discard PC relative relocs if it doesn't need them when linking
   with -Bsymbolic.  We store the information in a field extending the
   regular ELF linker hash table.  */

/* This structure keeps track of the number of PC relative relocs we have
   copied for a given symbol.  */

struct elf_m68k_pcrel_relocs_copied
{
  /* Next section.  */
  struct elf_m68k_pcrel_relocs_copied *next;
  /* A section in dynobj.  */
  asection *section;
  /* Number of relocs copied in this section.  */
  bfd_size_type count;
};

/* m68k ELF linker hash entry.  */

struct elf_m68k_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* Number of PC relative relocs copied for this symbol.  */
  struct elf_m68k_pcrel_relocs_copied *pcrel_relocs_copied;
};

#define elf_m68k_hash_entry(ent) ((struct elf_m68k_link_hash_entry *) (ent))

/* m68k ELF linker hash table.  */

struct elf_m68k_link_hash_table
{
  struct elf_link_hash_table root;

  /* Small local sym to section mapping cache.  */
  struct sym_sec_cache sym_sec;
};

/* Get the m68k ELF linker hash table from a link_info structure.  */

#define elf_m68k_hash_table(p) \
  ((struct elf_m68k_link_hash_table *) (p)->hash)

/* Create an entry in an m68k ELF linker hash table.  */

static struct bfd_hash_entry *
elf_m68k_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct bfd_hash_entry *ret = entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = bfd_hash_allocate (table,
			     sizeof (struct elf_m68k_link_hash_entry));
  if (ret == NULL)
    return ret;

  /* Call the allocation method of the superclass.  */
  ret = _bfd_elf_link_hash_newfunc (ret, table, string);
  if (ret != NULL)
    elf_m68k_hash_entry (ret)->pcrel_relocs_copied = NULL;

  return ret;
}

/* Create an m68k ELF linker hash table.  */

static struct bfd_link_hash_table *
elf_m68k_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf_m68k_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct elf_m68k_link_hash_table);

  ret = (struct elf_m68k_link_hash_table *) bfd_malloc (amt);
  if (ret == (struct elf_m68k_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       elf_m68k_link_hash_newfunc))
    {
      free (ret);
      return NULL;
    }

  ret->sym_sec.abfd = NULL;

  return &ret->root.root;
}

/* Keep m68k-specific flags in the ELF header.  */
static bfd_boolean
elf32_m68k_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */
static bfd_boolean
elf32_m68k_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
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

/* Display the flags field.  */
static bfd_boolean
elf32_m68k_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE *file = (FILE *) ptr;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  /* Print normal ELF private data.  */
  _bfd_elf_print_private_bfd_data (abfd, ptr);

  /* Ignore init flag - it may not be set, despite the flags field containing valid data.  */

  /* xgettext:c-format */
  fprintf (file, _("private flags = %lx:"), elf_elfheader (abfd)->e_flags);

  if (elf_elfheader (abfd)->e_flags & EF_CPU32)
    fprintf (file, _(" [cpu32]"));

  if (elf_elfheader (abfd)->e_flags & EF_M68000)
    fprintf (file, _(" [m68000]"));

  fputc ('\n', file);

  return TRUE;
}
/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static bfd_boolean
elf_m68k_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
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
  local_got_refcounts = elf_local_got_refcounts (abfd);

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
	case R_68K_GOT8:
	case R_68K_GOT16:
	case R_68K_GOT32:
	  if (h != NULL
	      && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* Fall through.  */
	case R_68K_GOT8O:
	case R_68K_GOT16O:
	case R_68K_GOT32O:
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
		  srelgot = bfd_make_section (dynobj, ".rela.got");
		  if (srelgot == NULL
		      || !bfd_set_section_flags (dynobj, srelgot,
						 (SEC_ALLOC
						  | SEC_LOAD
						  | SEC_HAS_CONTENTS
						  | SEC_IN_MEMORY
						  | SEC_LINKER_CREATED
						  | SEC_READONLY))
		      || !bfd_set_section_alignment (dynobj, srelgot, 2))
		    return FALSE;
		}
	    }

	  if (h != NULL)
	    {
	      if (h->got.refcount == 0)
		{
		  /* Make sure this symbol is output as a dynamic symbol.  */
		  if (h->dynindx == -1
		      && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
		    {
		      if (!bfd_elf_link_record_dynamic_symbol (info, h))
			return FALSE;
		    }

		  /* Allocate space in the .got section.  */
		  sgot->_raw_size += 4;
		  /* Allocate relocation space.  */
		  srelgot->_raw_size += sizeof (Elf32_External_Rela);
		}
	      h->got.refcount++;
	    }
	  else
	    {
	      /* This is a global offset table entry for a local symbol.  */
	      if (local_got_refcounts == NULL)
		{
		  bfd_size_type size;

		  size = symtab_hdr->sh_info;
		  size *= sizeof (bfd_signed_vma);
		  local_got_refcounts = ((bfd_signed_vma *)
					 bfd_zalloc (abfd, size));
		  if (local_got_refcounts == NULL)
		    return FALSE;
		  elf_local_got_refcounts (abfd) = local_got_refcounts;
		}
	      if (local_got_refcounts[r_symndx] == 0)
		{
		  sgot->_raw_size += 4;
		  if (info->shared)
		    {
		      /* If we are generating a shared object, we need to
			 output a R_68K_RELATIVE reloc so that the dynamic
			 linker can adjust this GOT entry.  */
		      srelgot->_raw_size += sizeof (Elf32_External_Rela);
		    }
		}
	      local_got_refcounts[r_symndx]++;
	    }
	  break;

	case R_68K_PLT8:
	case R_68K_PLT16:
	case R_68K_PLT32:
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

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.refcount++;
	  break;

	case R_68K_PLT8O:
	case R_68K_PLT16O:
	case R_68K_PLT32O:
	  /* This symbol requires a procedure linkage table entry.  */

	  if (h == NULL)
	    {
	      /* It does not make sense to have this relocation for a
		 local symbol.  FIXME: does it?  How to handle it if
		 it does make sense?  */
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }

	  /* Make sure this symbol is output as a dynamic symbol.  */
	  if (h->dynindx == -1
	      && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	    {
	      if (!bfd_elf_link_record_dynamic_symbol (info, h))
		return FALSE;
	    }

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.refcount++;
	  break;

	case R_68K_PC8:
	case R_68K_PC16:
	case R_68K_PC32:
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
		    || h->root.type == bfd_link_hash_defweak
		    || (h->elf_link_hash_flags
			& ELF_LINK_HASH_DEF_REGULAR) == 0)))
	    {
	      if (h != NULL)
		{
		  /* Make sure a plt entry is created for this symbol if
		     it turns out to be a function defined by a dynamic
		     object.  */
		  h->plt.refcount++;
		}
	      break;
	    }
	  /* Fall through.  */
	case R_68K_8:
	case R_68K_16:
	case R_68K_32:
	  if (h != NULL)
	    {
	      /* Make sure a plt entry is created for this symbol if it
		 turns out to be a function defined by a dynamic object.  */
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
		      sreloc = bfd_make_section (dynobj, name);
		      if (sreloc == NULL
			  || !bfd_set_section_flags (dynobj, sreloc,
						     (SEC_ALLOC
						      | SEC_LOAD
						      | SEC_HAS_CONTENTS
						      | SEC_IN_MEMORY
						      | SEC_LINKER_CREATED
						      | SEC_READONLY))
			  || !bfd_set_section_alignment (dynobj, sreloc, 2))
			return FALSE;
		    }
		  elf_section_data (sec)->sreloc = sreloc;
		}

	      if (sec->flags & SEC_READONLY
		  /* Don't set DF_TEXTREL yet for PC relative
		     relocations, they might be discarded later.  */
		  && !(ELF32_R_TYPE (rel->r_info) == R_68K_PC8
		       || ELF32_R_TYPE (rel->r_info) == R_68K_PC16
		       || ELF32_R_TYPE (rel->r_info) == R_68K_PC32))
		    info->flags |= DF_TEXTREL;

	      sreloc->_raw_size += sizeof (Elf32_External_Rela);

	      /* We count the number of PC relative relocations we have
		 entered for this symbol, so that we can discard them
		 again if, in the -Bsymbolic case, the symbol is later
		 defined by a regular object, or, in the normal shared
		 case, the symbol is forced to be local.  Note that this
		 function is only called if we are using an m68kelf linker
		 hash table, which means that h is really a pointer to an
		 elf_m68k_link_hash_entry.  */
	      if (ELF32_R_TYPE (rel->r_info) == R_68K_PC8
		  || ELF32_R_TYPE (rel->r_info) == R_68K_PC16
		  || ELF32_R_TYPE (rel->r_info) == R_68K_PC32)
		{
		  struct elf_m68k_pcrel_relocs_copied *p;
		  struct elf_m68k_pcrel_relocs_copied **head;

		  if (h != NULL)
		    {
		      struct elf_m68k_link_hash_entry *eh
			= elf_m68k_hash_entry (h);
		      head = &eh->pcrel_relocs_copied;
		    }
		  else
		    {
		      asection *s;
		      s = (bfd_section_from_r_symndx
			   (abfd, &elf_m68k_hash_table (info)->sym_sec,
			    sec, r_symndx));
		      if (s == NULL)
			return FALSE;

		      head = ((struct elf_m68k_pcrel_relocs_copied **)
			      &elf_section_data (s)->local_dynrel);
		    }

		  for (p = *head; p != NULL; p = p->next)
		    if (p->section == sreloc)
		      break;

		  if (p == NULL)
		    {
		      p = ((struct elf_m68k_pcrel_relocs_copied *)
			   bfd_alloc (dynobj, (bfd_size_type) sizeof *p));
		      if (p == NULL)
			return FALSE;
		      p->next = *head;
		      *head = p;
		      p->section = sreloc;
		      p->count = 0;
		    }

		  ++p->count;
		}
	    }

	  break;

	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_68K_GNU_VTINHERIT:
	  if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return FALSE;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_68K_GNU_VTENTRY:
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
elf_m68k_gc_mark_hook (sec, info, rel, h, sym)
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
	case R_68K_GNU_VTINHERIT:
	case R_68K_GNU_VTENTRY:
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
elf_m68k_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;
  bfd *dynobj;
  asection *sgot;
  asection *srelgot;

  dynobj = elf_hash_table (info)->dynobj;
  if (dynobj == NULL)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  sgot = bfd_get_section_by_name (dynobj, ".got");
  srelgot = bfd_get_section_by_name (dynobj, ".rela.got");

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_68K_GOT8:
	case R_68K_GOT16:
	case R_68K_GOT32:
	case R_68K_GOT8O:
	case R_68K_GOT16O:
	case R_68K_GOT32O:
	  r_symndx = ELF32_R_SYM (rel->r_info);
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      if (h->got.refcount > 0)
		{
		  --h->got.refcount;
		  if (h->got.refcount == 0)
		    {
		      /* We don't need the .got entry any more.  */
		      sgot->_raw_size -= 4;
		      srelgot->_raw_size -= sizeof (Elf32_External_Rela);
		    }
		}
	    }
	  else if (local_got_refcounts != NULL)
	    {
	      if (local_got_refcounts[r_symndx] > 0)
		{
		  --local_got_refcounts[r_symndx];
		  if (local_got_refcounts[r_symndx] == 0)
		    {
		      /* We don't need the .got entry any more.  */
		      sgot->_raw_size -= 4;
		      if (info->shared)
			srelgot->_raw_size -= sizeof (Elf32_External_Rela);
		    }
		}
	    }
	  break;

	case R_68K_PLT8:
	case R_68K_PLT16:
	case R_68K_PLT32:
	case R_68K_PLT8O:
	case R_68K_PLT16O:
	case R_68K_PLT32O:
	case R_68K_PC8:
	case R_68K_PC16:
	case R_68K_PC32:
	case R_68K_8:
	case R_68K_16:
	case R_68K_32:
	  r_symndx = ELF32_R_SYM (rel->r_info);
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      if (h->plt.refcount > 0)
		--h->plt.refcount;
	    }
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
elf_m68k_adjust_dynamic_symbol (info, h)
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
     will fill in the contents of the procedure linkage table later,
     when we know the address of the .got section.  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (! info->shared
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) == 0
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
	  BFD_ASSERT ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0);
	  h->plt.offset = (bfd_vma) -1;
	  return TRUE;
	}

      /* GC may have rendered this entry unused.  */
      if (h->plt.refcount <= 0)
	{
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.offset = (bfd_vma) -1;
	  return TRUE;
	}

      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1
	  && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	{
	  if (! bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);

      /* If this is the first .plt entry, make room for the special
	 first entry.  */
      if (s->_raw_size == 0)
	{
	  if (CPU32_FLAG (dynobj))
	    s->_raw_size += PLT_CPU32_ENTRY_SIZE;
	  else
	    s->_raw_size += PLT_ENTRY_SIZE;
	}

      /* If this symbol is not defined in a regular file, and we are
	 not generating a shared library, then set the symbol to this
	 location in the .plt.  This is required to make function
	 pointers compare as equal between the normal executable and
	 the shared library.  */
      if (!info->shared
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = s->_raw_size;
	}

      h->plt.offset = s->_raw_size;

      /* Make room for this entry.  */
      if (CPU32_FLAG (dynobj))
        s->_raw_size += PLT_CPU32_ENTRY_SIZE;
      else
        s->_raw_size += PLT_ENTRY_SIZE;

      /* We also need to make an entry in the .got.plt section, which
	 will be placed in the .got section by the linker script.  */
      s = bfd_get_section_by_name (dynobj, ".got.plt");
      BFD_ASSERT (s != NULL);
      s->_raw_size += 4;

      /* We also need to make an entry in the .rela.plt section.  */
      s = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (s != NULL);
      s->_raw_size += sizeof (Elf32_External_Rela);

      return TRUE;
    }

  /* Reinitialize the plt offset now that it is not used as a reference
     count any more.  */
  h->plt.offset = (bfd_vma) -1;

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
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

  /* We must generate a R_68K_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
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
      if (!bfd_set_section_alignment (dynobj, s, power_of_two))
	return FALSE;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return TRUE;
}

/* Set the sizes of the dynamic sections.  */

static bfd_boolean
elf_m68k_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  bfd_boolean plt;
  bfd_boolean relocs;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (info->executable)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
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

  /* If this is a -Bsymbolic shared link, then we need to discard all
     PC relative relocs against symbols defined in a regular object.
     For the normal shared case we discard the PC relative relocs
     against symbols that have become local due to visibility changes.
     We allocated space for them in the check_relocs routine, but we
     will not fill them in in the relocate_section routine.  */
  if (info->shared)
    elf_link_hash_traverse (elf_hash_table (info),
			    elf_m68k_discard_copies,
			    (PTR) info);

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  plt = FALSE;
  relocs = FALSE;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      bfd_boolean strip;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      strip = FALSE;

      if (strcmp (name, ".plt") == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* Strip this section if we don't need it; see the
                 comment below.  */
	      strip = TRUE;
	    }
	  else
	    {
	      /* Remember whether there is a PLT.  */
	      plt = TRUE;
	    }
	}
      else if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->_raw_size == 0)
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
	      strip = TRUE;
	    }
	  else
	    {
	      relocs = TRUE;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strncmp (name, ".got", 4) != 0)
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
	return FALSE;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf_m68k_finish_dynamic_sections, but we
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

      if ((info->flags & DF_TEXTREL) != 0)
	{
	  if (!add_dynamic_entry (DT_TEXTREL, 0))
	    return FALSE;
	}
    }
#undef add_dynamic_entry

  return TRUE;
}

/* This function is called via elf_link_hash_traverse if we are
   creating a shared object.  In the -Bsymbolic case it discards the
   space allocated to copy PC relative relocs against symbols which
   are defined in regular objects.  For the normal shared case, it
   discards space for pc-relative relocs that have become local due to
   symbol visibility changes.  We allocated space for them in the
   check_relocs routine, but we won't fill them in in the
   relocate_section routine.

   We also check whether any of the remaining relocations apply
   against a readonly section, and set the DF_TEXTREL flag in this
   case.  */

static bfd_boolean
elf_m68k_discard_copies (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct bfd_link_info *info = (struct bfd_link_info *) inf;
  struct elf_m68k_pcrel_relocs_copied *s;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
      || (!info->symbolic
	  && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0))
    {
      if ((info->flags & DF_TEXTREL) == 0)
	{
	  /* Look for relocations against read-only sections.  */
	  for (s = elf_m68k_hash_entry (h)->pcrel_relocs_copied;
	       s != NULL;
	       s = s->next)
	    if ((s->section->flags & SEC_READONLY) != 0)
	      {
		info->flags |= DF_TEXTREL;
		break;
	      }
	}

      return TRUE;
    }

  for (s = elf_m68k_hash_entry (h)->pcrel_relocs_copied;
       s != NULL;
       s = s->next)
    s->section->_raw_size -= s->count * sizeof (Elf32_External_Rela);

  return TRUE;
}

/* Relocate an M68K ELF section.  */

static bfd_boolean
elf_m68k_relocate_section (output_bfd, info, input_bfd, input_section,
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
  asection *sgot;
  asection *splt;
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
      bfd_boolean unresolved_reloc;
      bfd_reloc_status_type r;

      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_68K_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
      howto = howto_table + r_type;

      r_symndx = ELF32_R_SYM (rel->r_info);

      h = NULL;
      sym = NULL;
      sec = NULL;
      unresolved_reloc = FALSE;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	}
      else
	{
	  bfd_boolean warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);
	}

      switch (r_type)
	{
	case R_68K_GOT8:
	case R_68K_GOT16:
	case R_68K_GOT32:
	  /* Relocation is to the address of the entry for this symbol
	     in the global offset table.  */
	  if (h != NULL
	      && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* Fall through.  */
	case R_68K_GOT8O:
	case R_68K_GOT16O:
	case R_68K_GOT32O:
	  /* Relocation is the offset of the entry for this symbol in
	     the global offset table.  */

	  {
	    bfd_vma off;

	    if (sgot == NULL)
	      {
		sgot = bfd_get_section_by_name (dynobj, ".got");
		BFD_ASSERT (sgot != NULL);
	      }

	    if (h != NULL)
	      {
		bfd_boolean dyn;

		off = h->got.offset;
		BFD_ASSERT (off != (bfd_vma) -1);

		dyn = elf_hash_table (info)->dynamic_sections_created;
		if (!WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info->shared, h)
		    || (info->shared
			&& (info->symbolic
			    || h->dynindx == -1
			    || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
			&& (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)) != 0)
		  {
		    /* This is actually a static link, or it is a
		       -Bsymbolic link and the symbol is defined
		       locally, or the symbol was forced to be local
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
			bfd_put_32 (output_bfd, relocation,
				    sgot->contents + off);
			h->got.offset |= 1;
		      }
		  }
		else
		  unresolved_reloc = FALSE;
	      }
	    else
	      {
		BFD_ASSERT (local_got_offsets != NULL
			    && local_got_offsets[r_symndx] != (bfd_vma) -1);

		off = local_got_offsets[r_symndx];

		/* The offset must always be a multiple of 4.  We use
		   the least significant bit to record whether we have
		   already generated the necessary reloc.  */
		if ((off & 1) != 0)
		  off &= ~1;
		else
		  {
		    bfd_put_32 (output_bfd, relocation, sgot->contents + off);

		    if (info->shared)
		      {
			asection *s;
			Elf_Internal_Rela outrel;
			bfd_byte *loc;

			s = bfd_get_section_by_name (dynobj, ".rela.got");
			BFD_ASSERT (s != NULL);

			outrel.r_offset = (sgot->output_section->vma
					   + sgot->output_offset
					   + off);
			outrel.r_info = ELF32_R_INFO (0, R_68K_RELATIVE);
			outrel.r_addend = relocation;
			loc = s->contents;
			loc += s->reloc_count++ * sizeof (Elf32_External_Rela);
			bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
		      }

		    local_got_offsets[r_symndx] |= 1;
		  }
	      }

	    relocation = sgot->output_offset + off;
	    if (r_type == R_68K_GOT8O
		|| r_type == R_68K_GOT16O
		|| r_type == R_68K_GOT32O)
	      {
		/* This relocation does not use the addend.  */
		rel->r_addend = 0;
	      }
	    else
	      relocation += sgot->output_section->vma;
	  }
	  break;

	case R_68K_PLT8:
	case R_68K_PLT16:
	case R_68K_PLT32:
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

	  relocation = (splt->output_section->vma
			+ splt->output_offset
			+ h->plt.offset);
	  unresolved_reloc = FALSE;
	  break;

	case R_68K_PLT8O:
	case R_68K_PLT16O:
	case R_68K_PLT32O:
	  /* Relocation is the offset of the entry for this symbol in
	     the procedure linkage table.  */
	  BFD_ASSERT (h != NULL && h->plt.offset != (bfd_vma) -1);

	  if (splt == NULL)
	    {
	      splt = bfd_get_section_by_name (dynobj, ".plt");
	      BFD_ASSERT (splt != NULL);
	    }

	  relocation = h->plt.offset;
	  unresolved_reloc = FALSE;

	  /* This relocation does not use the addend.  */
	  rel->r_addend = 0;

	  break;

	case R_68K_PC8:
	case R_68K_PC16:
	case R_68K_PC32:
	  if (h == NULL
	      || (info->shared
		  && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0))
	    break;
	  /* Fall through.  */
	case R_68K_8:
	case R_68K_16:
	case R_68K_32:
	  if (info->shared
	      && r_symndx != 0
	      && (input_section->flags & SEC_ALLOC) != 0
	      && (h == NULL
		  || ELF_ST_VISIBILITY (h->other) == STV_DEFAULT
		  || h->root.type != bfd_link_hash_undefweak)
	      && ((r_type != R_68K_PC8
		   && r_type != R_68K_PC16
		   && r_type != R_68K_PC32)
		  || (h != NULL
		      && h->dynindx != -1
		      && (!info->symbolic
			  || (h->elf_link_hash_flags
			      & ELF_LINK_HASH_DEF_REGULAR) == 0))))
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;
	      bfd_boolean skip, relocate;

	      /* When generating a shared object, these relocations
		 are copied into the output file to be resolved at run
		 time.  */

	      skip = FALSE;
	      relocate = FALSE;

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info, input_section,
					 rel->r_offset);
	      if (outrel.r_offset == (bfd_vma) -1)
		skip = TRUE;
	      else if (outrel.r_offset == (bfd_vma) -2)
		skip = TRUE, relocate = TRUE;
	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      else if (h != NULL
		       && h->dynindx != -1
		       && (r_type == R_68K_PC8
			   || r_type == R_68K_PC16
			   || r_type == R_68K_PC32
			   || !info->shared
			   || !info->symbolic
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))
		{
		  outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = rel->r_addend;
		}
	      else
		{
		  /* This symbol is local, or marked to become local.  */
		  if (r_type == R_68K_32)
		    {
		      relocate = TRUE;
		      outrel.r_info = ELF32_R_INFO (0, R_68K_RELATIVE);
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

	      sreloc = elf_section_data (input_section)->sreloc;
	      if (sreloc == NULL)
		abort ();

	      loc = sreloc->contents;
	      loc += sreloc->reloc_count++ * sizeof (Elf32_External_Rela);
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);

	      /* This reloc will be computed at runtime, so there's no
                 need to do anything now, except for R_68K_32
                 relocations that have been turned into
                 R_68K_RELATIVE.  */
	      if (!relocate)
		continue;
	    }

	  break;

	case R_68K_GNU_VTINHERIT:
	case R_68K_GNU_VTENTRY:
	  /* These are no-ops in the end.  */
	  continue;

	default:
	  break;
	}

      /* Dynamic relocs are not propagated for SEC_DEBUGGING sections
	 because such sections are not SEC_ALLOC and thus ld.so will
	 not process them.  */
      if (unresolved_reloc
	  && !((input_section->flags & SEC_DEBUGGING) != 0
	       && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0))
	{
	  (*_bfd_error_handler)
	    (_("%s(%s+0x%lx): unresolvable relocation against symbol `%s'"),
	     bfd_archive_filename (input_bfd),
	     bfd_get_section_name (input_bfd, input_section),
	     (long) rel->r_offset,
	     h->root.root.string);
	  return FALSE;
	}

      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, rel->r_addend);

      if (r != bfd_reloc_ok)
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
		return FALSE;
	      if (*name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  if (r == bfd_reloc_overflow)
	    {
	      if (!(info->callbacks->reloc_overflow
		    (info, name, howto->name, (bfd_vma) 0,
		     input_bfd, input_section, rel->r_offset)))
		return FALSE;
	    }
	  else
	    {
	      (*_bfd_error_handler)
		(_("%s(%s+0x%lx): reloc against `%s': error %d"),
		 bfd_archive_filename (input_bfd),
		 bfd_get_section_name (input_bfd, input_section),
		 (long) rel->r_offset, name, (int) r);
	      return FALSE;
	    }
	}
    }

  return TRUE;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static bfd_boolean
elf_m68k_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;
  int plt_off1, plt_off2, plt_off3;

  dynobj = elf_hash_table (info)->dynobj;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *sgot;
      asection *srela;
      bfd_vma plt_index;
      bfd_vma got_offset;
      Elf_Internal_Rela rela;
      bfd_byte *loc;

      /* This symbol has an entry in the procedure linkage table.  Set
	 it up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = bfd_get_section_by_name (dynobj, ".plt");
      sgot = bfd_get_section_by_name (dynobj, ".got.plt");
      srela = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (splt != NULL && sgot != NULL && srela != NULL);

      /* Get the index in the procedure linkage table which
	 corresponds to this symbol.  This is the index of this symbol
	 in all the symbols for which we are making plt entries.  The
	 first entry in the procedure linkage table is reserved.  */
      if ( CPU32_FLAG (output_bfd))
        plt_index = h->plt.offset / PLT_CPU32_ENTRY_SIZE - 1;
      else
        plt_index = h->plt.offset / PLT_ENTRY_SIZE - 1;

      /* Get the offset into the .got table of the entry that
	 corresponds to this function.  Each .got entry is 4 bytes.
	 The first three are reserved.  */
      got_offset = (plt_index + 3) * 4;

      if ( CPU32_FLAG (output_bfd))
        {
          /* Fill in the entry in the procedure linkage table.  */
          memcpy (splt->contents + h->plt.offset, elf_cpu32_plt_entry,
	          PLT_CPU32_ENTRY_SIZE);
          plt_off1 = 4;
          plt_off2 = 12;
          plt_off3 = 18;
        }
      else
        {
          /* Fill in the entry in the procedure linkage table.  */
          memcpy (splt->contents + h->plt.offset, elf_m68k_plt_entry,
	          PLT_ENTRY_SIZE);
          plt_off1 = 4;
          plt_off2 = 10;
          plt_off3 = 16;
        }

      /* The offset is relative to the first extension word.  */
      bfd_put_32 (output_bfd,
		  (sgot->output_section->vma
		   + sgot->output_offset
		   + got_offset
		   - (splt->output_section->vma
		      + h->plt.offset + 2)),
		  splt->contents + h->plt.offset + plt_off1);

      bfd_put_32 (output_bfd, plt_index * sizeof (Elf32_External_Rela),
		  splt->contents + h->plt.offset + plt_off2);
      bfd_put_32 (output_bfd, - (h->plt.offset + plt_off3),
		  splt->contents + h->plt.offset + plt_off3);

      /* Fill in the entry in the global offset table.  */
      bfd_put_32 (output_bfd,
		  (splt->output_section->vma
		   + splt->output_offset
		   + h->plt.offset
		   + 8),
		  sgot->contents + got_offset);

      /* Fill in the entry in the .rela.plt section.  */
      rela.r_offset = (sgot->output_section->vma
		       + sgot->output_offset
		       + got_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_68K_JMP_SLOT);
      rela.r_addend = 0;
      loc = srela->contents + plt_index * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
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
		       + (h->got.offset &~ (bfd_vma) 1));

      /* If this is a -Bsymbolic link, and the symbol is defined
	 locally, we just want to emit a RELATIVE reloc.  Likewise if
	 the symbol was forced to be local because of a version file.
	 The entry in the global offset table will already have been
	 initialized in the relocate_section function.  */
      if (info->shared
	  && (info->symbolic
	      || h->dynindx == -1
	      || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	{
	  rela.r_info = ELF32_R_INFO (0, R_68K_RELATIVE);
	  rela.r_addend = bfd_get_signed_32 (output_bfd,
					     (sgot->contents
					      + (h->got.offset &~ (bfd_vma) 1)));
	}
      else
	{
	  bfd_put_32 (output_bfd, (bfd_vma) 0,
		      sgot->contents + (h->got.offset &~ (bfd_vma) 1));
	  rela.r_info = ELF32_R_INFO (h->dynindx, R_68K_GLOB_DAT);
	  rela.r_addend = 0;
	}

      loc = srela->contents;
      loc += srela->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
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
      rela.r_info = ELF32_R_INFO (h->dynindx, R_68K_COPY);
      rela.r_addend = 0;
      loc = s->contents + s->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}

/* Finish up the dynamic sections.  */

static bfd_boolean
elf_m68k_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
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
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
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
	      if (s->_cooked_size != 0)
		dyn.d_un.d_val = s->_cooked_size;
	      else
		dyn.d_un.d_val = s->_raw_size;
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
		{
		  if (s->_cooked_size != 0)
		    dyn.d_un.d_val -= s->_cooked_size;
		  else
		    dyn.d_un.d_val -= s->_raw_size;
		}
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;
	    }
	}

      /* Fill in the first entry in the procedure linkage table.  */
      if (splt->_raw_size > 0)
	{
          if (!CPU32_FLAG (output_bfd))
            {
	      memcpy (splt->contents, elf_m68k_plt0_entry, PLT_ENTRY_SIZE);
	      bfd_put_32 (output_bfd,
		          (sgot->output_section->vma
		           + sgot->output_offset + 4
		           - (splt->output_section->vma + 2)),
		          splt->contents + 4);
	      bfd_put_32 (output_bfd,
		          (sgot->output_section->vma
		           + sgot->output_offset + 8
		           - (splt->output_section->vma + 10)),
		          splt->contents + 12);
              elf_section_data (splt->output_section)->this_hdr.sh_entsize
               = PLT_ENTRY_SIZE;
            }
          else /* cpu32 */
            {
              memcpy (splt->contents, elf_cpu32_plt0_entry, PLT_CPU32_ENTRY_SIZE);
	      bfd_put_32 (output_bfd,
		          (sgot->output_section->vma
		           + sgot->output_offset + 4
		           - (splt->output_section->vma + 2)),
		          splt->contents + 4);
	      bfd_put_32 (output_bfd,
		          (sgot->output_section->vma
		           + sgot->output_offset + 8
		           - (splt->output_section->vma + 10)),
		          splt->contents + 12);
              elf_section_data (splt->output_section)->this_hdr.sh_entsize
               = PLT_CPU32_ENTRY_SIZE;
            }
	}
    }

  /* Fill in the first three entries in the global offset table.  */
  if (sgot->_raw_size > 0)
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

/* Given a .data section and a .emreloc in-memory section, store
   relocation information into the .emreloc section which can be
   used at runtime to relocate the section.  This is called by the
   linker when the --embedded-relocs switch is used.  This is called
   after the add_symbols entry point has been called for all the
   objects, and before the final_link entry point is called.  */

bfd_boolean
bfd_m68k_elf32_create_embedded_relocs (abfd, info, datasec, relsec, errmsg)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *datasec;
     asection *relsec;
     char **errmsg;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Sym *isymbuf = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *p;
  bfd_size_type amt;

  BFD_ASSERT (! info->relocatable);

  *errmsg = NULL;

  if (datasec->reloc_count == 0)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf_link_read_relocs
		     (abfd, datasec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  amt = (bfd_size_type) datasec->reloc_count * 12;
  relsec->contents = (bfd_byte *) bfd_alloc (abfd, amt);
  if (relsec->contents == NULL)
    goto error_return;

  p = relsec->contents;

  irelend = internal_relocs + datasec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++, p += 12)
    {
      asection *targetsec;

      /* We are going to write a four byte longword into the runtime
       reloc section.  The longword will be the address in the data
       section which must be relocated.  It is followed by the name
       of the target section NUL-padded or truncated to 8
       characters.  */

      /* We can only relocate absolute longword relocs at run time.  */
      if (ELF32_R_TYPE (irel->r_info) != (int) R_68K_32)
	{
	  *errmsg = _("unsupported reloc type");
	  bfd_set_error (bfd_error_bad_value);
	  goto error_return;
	}

      /* Get the target section referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  Elf_Internal_Sym *isym;

	  /* Read this BFD's local symbols if we haven't done so already.  */
	  if (isymbuf == NULL)
	    {
	      isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	      if (isymbuf == NULL)
		isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
						symtab_hdr->sh_info, 0,
						NULL, NULL, NULL);
	      if (isymbuf == NULL)
		goto error_return;
	    }

	  isym = isymbuf + ELF32_R_SYM (irel->r_info);
	  targetsec = bfd_section_from_elf_index (abfd, isym->st_shndx);
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;

	  /* An external symbol.  */
	  indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    targetsec = h->root.u.def.section;
	  else
	    targetsec = NULL;
	}

      bfd_put_32 (abfd, irel->r_offset + datasec->output_offset, p);
      memset (p + 4, 0, 8);
      if (targetsec != NULL)
	strncpy (p + 4, targetsec->output_section->name, 8);
    }

  if (isymbuf != NULL && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (internal_relocs != NULL
      && elf_section_data (datasec)->relocs != internal_relocs)
    free (internal_relocs);
  return TRUE;

error_return:
  if (isymbuf != NULL && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (internal_relocs != NULL
      && elf_section_data (datasec)->relocs != internal_relocs)
    free (internal_relocs);
  return FALSE;
}

static enum elf_reloc_type_class
elf32_m68k_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  switch ((int) ELF32_R_TYPE (rela->r_info))
    {
    case R_68K_RELATIVE:
      return reloc_class_relative;
    case R_68K_JMP_SLOT:
      return reloc_class_plt;
    case R_68K_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

#define TARGET_BIG_SYM			bfd_elf32_m68k_vec
#define TARGET_BIG_NAME			"elf32-m68k"
#define ELF_MACHINE_CODE		EM_68K
#define ELF_MAXPAGESIZE			0x2000
#define elf_backend_create_dynamic_sections \
					_bfd_elf_create_dynamic_sections
#define bfd_elf32_bfd_link_hash_table_create \
					elf_m68k_link_hash_table_create
#define bfd_elf32_bfd_final_link	bfd_elf_gc_common_final_link

#define elf_backend_check_relocs	elf_m68k_check_relocs
#define elf_backend_adjust_dynamic_symbol \
					elf_m68k_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
					elf_m68k_size_dynamic_sections
#define elf_backend_relocate_section	elf_m68k_relocate_section
#define elf_backend_finish_dynamic_symbol \
					elf_m68k_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					elf_m68k_finish_dynamic_sections
#define elf_backend_gc_mark_hook	elf_m68k_gc_mark_hook
#define elf_backend_gc_sweep_hook	elf_m68k_gc_sweep_hook
#define bfd_elf32_bfd_merge_private_bfd_data \
                                        elf32_m68k_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags \
                                        elf32_m68k_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data \
                                        elf32_m68k_print_private_bfd_data
#define elf_backend_reloc_type_class	elf32_m68k_reloc_type_class

#define elf_backend_can_gc_sections 1
#define elf_backend_can_refcount 1
#define elf_backend_want_got_plt 1
#define elf_backend_plt_readonly 1
#define elf_backend_want_plt_sym 0
#define elf_backend_got_header_size	12
#define elf_backend_rela_normal		1

#include "elf32-target.h"
