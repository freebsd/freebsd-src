/* Alpha specific support for 64-bit ELF
   Copyright 1996, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@tamu.edu>.

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

/* $FreeBSD$ */

/* We need a published ABI spec for this.  Until one comes out, don't
   assume this'll remain unchanged forever.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"

#include "elf/alpha.h"

#define ALPHAECOFF

#define NO_COFF_RELOCS
#define NO_COFF_SYMBOLS
#define NO_COFF_LINENOS

/* Get the ECOFF swapping routines.  Needed for the debug information.  */
#include "coff/internal.h"
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/ecoff.h"
#include "coff/alpha.h"
#include "aout/ar.h"
#include "libcoff.h"
#include "libecoff.h"
#define ECOFF_64
#include "ecoffswap.h"

static int alpha_elf_dynamic_symbol_p
  PARAMS((struct elf_link_hash_entry *, struct bfd_link_info *));
static struct bfd_hash_entry * elf64_alpha_link_hash_newfunc
  PARAMS((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct bfd_link_hash_table * elf64_alpha_bfd_link_hash_table_create
  PARAMS((bfd *));

static bfd_reloc_status_type elf64_alpha_reloc_nil
  PARAMS((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type elf64_alpha_reloc_bad
  PARAMS((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type elf64_alpha_do_reloc_gpdisp
  PARAMS((bfd *, bfd_vma, bfd_byte *, bfd_byte *));
static bfd_reloc_status_type elf64_alpha_reloc_gpdisp
  PARAMS((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

static reloc_howto_type * elf64_alpha_bfd_reloc_type_lookup
  PARAMS((bfd *, bfd_reloc_code_real_type));
static void elf64_alpha_info_to_howto
  PARAMS((bfd *, arelent *, Elf64_Internal_Rela *));

static boolean elf64_alpha_mkobject
  PARAMS((bfd *));
static boolean elf64_alpha_object_p
  PARAMS((bfd *));
static boolean elf64_alpha_section_from_shdr
  PARAMS((bfd *, Elf64_Internal_Shdr *, char *));
static boolean elf64_alpha_fake_sections
  PARAMS((bfd *, Elf64_Internal_Shdr *, asection *));
static boolean elf64_alpha_create_got_section
  PARAMS((bfd *, struct bfd_link_info *));
static boolean elf64_alpha_create_dynamic_sections
  PARAMS((bfd *, struct bfd_link_info *));

static boolean elf64_alpha_read_ecoff_info
  PARAMS((bfd *, asection *, struct ecoff_debug_info *));
static boolean elf64_alpha_is_local_label_name
  PARAMS((bfd *, const char *));
static boolean elf64_alpha_find_nearest_line
  PARAMS((bfd *, asection *, asymbol **, bfd_vma, const char **,
	  const char **, unsigned int *));

#if defined(__STDC__) || defined(ALMOST_STDC)
struct alpha_elf_link_hash_entry;
#endif

static boolean elf64_alpha_output_extsym
  PARAMS((struct alpha_elf_link_hash_entry *, PTR));

static boolean elf64_alpha_can_merge_gots
  PARAMS((bfd *, bfd *));
static void elf64_alpha_merge_gots
  PARAMS((bfd *, bfd *));
static boolean elf64_alpha_calc_got_offsets_for_symbol
  PARAMS ((struct alpha_elf_link_hash_entry *, PTR));
static void elf64_alpha_calc_got_offsets PARAMS ((struct bfd_link_info *));
static boolean elf64_alpha_size_got_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf64_alpha_always_size_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf64_alpha_calc_dynrel_sizes
  PARAMS ((struct alpha_elf_link_hash_entry *, struct bfd_link_info *));
static boolean elf64_alpha_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));
static boolean elf64_alpha_check_relocs
  PARAMS((bfd *, struct bfd_link_info *, asection *sec,
	  const Elf_Internal_Rela *));
static boolean elf64_alpha_adjust_dynamic_symbol
  PARAMS((struct bfd_link_info *, struct elf_link_hash_entry *));
static boolean elf64_alpha_size_dynamic_sections
  PARAMS((bfd *, struct bfd_link_info *));
static boolean elf64_alpha_relocate_section
  PARAMS((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	  Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static boolean elf64_alpha_finish_dynamic_symbol
  PARAMS((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	  Elf_Internal_Sym *));
static boolean elf64_alpha_finish_dynamic_sections
  PARAMS((bfd *, struct bfd_link_info *));
static boolean elf64_alpha_final_link
  PARAMS((bfd *, struct bfd_link_info *));
static boolean elf64_alpha_merge_ind_symbols
  PARAMS((struct alpha_elf_link_hash_entry *, PTR));
static Elf_Internal_Rela * elf64_alpha_find_reloc_at_ofs
  PARAMS ((Elf_Internal_Rela *, Elf_Internal_Rela *, bfd_vma, int));

struct alpha_elf_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* External symbol information.  */
  EXTR esym;

  /* Cumulative flags for all the .got entries.  */
  int flags;

  /* Contexts (LITUSE) in which a literal was referenced.  */
#define ALPHA_ELF_LINK_HASH_LU_ADDR 0x01
#define ALPHA_ELF_LINK_HASH_LU_MEM  0x02
#define ALPHA_ELF_LINK_HASH_LU_BYTE 0x04
#define ALPHA_ELF_LINK_HASH_LU_FUNC 0x08

  /* Used to implement multiple .got subsections.  */
  struct alpha_elf_got_entry
  {
    struct alpha_elf_got_entry *next;

    /* which .got subsection?  */
    bfd *gotobj;

    /* the addend in effect for this entry.  */
    bfd_vma addend;

    /* the .got offset for this entry.  */
    int got_offset;

    int flags;

    /* An additional flag.  */
#define ALPHA_ELF_GOT_ENTRY_RELOCS_DONE 0x10

    int use_count;
  } *got_entries;

  /* used to count non-got, non-plt relocations for delayed sizing
     of relocation sections.  */
  struct alpha_elf_reloc_entry
  {
    struct alpha_elf_reloc_entry *next;

    /* which .reloc section? */
    asection *srel;

    /* what kind of relocation? */
    unsigned long rtype;

    /* how many did we find?  */
    unsigned long count;
  } *reloc_entries;
};

/* Alpha ELF linker hash table.  */

struct alpha_elf_link_hash_table
{
  struct elf_link_hash_table root;

  /* The head of a list of .got subsections linked through
     alpha_elf_tdata(abfd)->got_link_next.  */
  bfd *got_list;
};

/* Look up an entry in a Alpha ELF linker hash table.  */

#define alpha_elf_link_hash_lookup(table, string, create, copy, follow)	\
  ((struct alpha_elf_link_hash_entry *)					\
   elf_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

/* Traverse a Alpha ELF linker hash table.  */

#define alpha_elf_link_hash_traverse(table, func, info)			\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the Alpha ELF linker hash table from a link_info structure.  */

#define alpha_elf_hash_table(p) \
  ((struct alpha_elf_link_hash_table *) ((p)->hash))

/* Get the object's symbols as our own entry type.  */

#define alpha_elf_sym_hashes(abfd) \
  ((struct alpha_elf_link_hash_entry **)elf_sym_hashes(abfd))

/* Should we do dynamic things to this symbol?  */

static int
alpha_elf_dynamic_symbol_p (h, info)
     struct elf_link_hash_entry *h;
     struct bfd_link_info *info;
{
  if (h == NULL)
    return false;

  while (h->root.type == bfd_link_hash_indirect
	 || h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->dynindx == -1)
    return false;

  if (h->root.type == bfd_link_hash_undefweak
      || h->root.type == bfd_link_hash_defweak)
    return true;

  switch (ELF_ST_VISIBILITY (h->other))
    {
    case STV_DEFAULT:
      break;
    case STV_HIDDEN:
    case STV_INTERNAL:
      return false;
    case STV_PROTECTED:
      if (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)
        return false;
      break;
    }

  if ((info->shared && !info->symbolic)
      || ((h->elf_link_hash_flags
	   & (ELF_LINK_HASH_DEF_DYNAMIC | ELF_LINK_HASH_REF_REGULAR))
	  == (ELF_LINK_HASH_DEF_DYNAMIC | ELF_LINK_HASH_REF_REGULAR)))
    return true;

  return false;
}

/* Create an entry in a Alpha ELF linker hash table.  */

static struct bfd_hash_entry *
elf64_alpha_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct alpha_elf_link_hash_entry *ret =
    (struct alpha_elf_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct alpha_elf_link_hash_entry *) NULL)
    ret = ((struct alpha_elf_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct alpha_elf_link_hash_entry)));
  if (ret == (struct alpha_elf_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct alpha_elf_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct alpha_elf_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      memset (&ret->esym, 0, sizeof (EXTR));
      /* We use -2 as a marker to indicate that the information has
	 not been set.  -1 means there is no associated ifd.  */
      ret->esym.ifd = -2;
      ret->flags = 0;
      ret->got_entries = NULL;
      ret->reloc_entries = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create a Alpha ELF linker hash table.  */

static struct bfd_link_hash_table *
elf64_alpha_bfd_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct alpha_elf_link_hash_table *ret;

  ret = ((struct alpha_elf_link_hash_table *)
	 bfd_zalloc (abfd, sizeof (struct alpha_elf_link_hash_table)));
  if (ret == (struct alpha_elf_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       elf64_alpha_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  return &ret->root.root;
}

/* We have some private fields hanging off of the elf_tdata structure.  */

struct alpha_elf_obj_tdata
{
  struct elf_obj_tdata root;

  /* For every input file, these are the got entries for that object's
     local symbols.  */
  struct alpha_elf_got_entry ** local_got_entries;

  /* For every input file, this is the object that owns the got that
     this input file uses.  */
  bfd *gotobj;

  /* For every got, this is a linked list through the objects using this got */
  bfd *in_got_link_next;

  /* For every got, this is a link to the next got subsegment.  */
  bfd *got_link_next;

  /* For every got, this is the section.  */
  asection *got;

  /* For every got, this is it's total number of *entries*.  */
  int total_got_entries;

  /* For every got, this is the sum of the number of *entries* required
     to hold all of the member object's local got.  */
  int n_local_got_entries;
};

#define alpha_elf_tdata(abfd) \
  ((struct alpha_elf_obj_tdata *) (abfd)->tdata.any)

static boolean
elf64_alpha_mkobject (abfd)
     bfd *abfd;
{
  abfd->tdata.any = bfd_zalloc (abfd, sizeof (struct alpha_elf_obj_tdata));
  if (abfd->tdata.any == NULL)
    return false;
  return true;
}

static boolean
elf64_alpha_object_p (abfd)
     bfd *abfd;
{
  /* Allocate our special target data.  */
  struct alpha_elf_obj_tdata *new_tdata;
  new_tdata = bfd_zalloc (abfd, sizeof (struct alpha_elf_obj_tdata));
  if (new_tdata == NULL)
    return false;
  new_tdata->root = *abfd->tdata.elf_obj_data;
  abfd->tdata.any = new_tdata;

  /* Set the right machine number for an Alpha ELF file.  */
  return bfd_default_set_arch_mach (abfd, bfd_arch_alpha, 0);
}

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

static reloc_howto_type elf64_alpha_howto_table[] =
{
  HOWTO (R_ALPHA_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_nil,	/* special_function */
	 "NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 32 bit reference to a symbol.  */
  HOWTO (R_ALPHA_REFLONG,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFLONG",		/* name */
	 false,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit reference to a symbol.  */
  HOWTO (R_ALPHA_REFQUAD,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFQUAD",		/* name */
	 false,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 32 bit GP relative offset.  This is just like REFLONG except
     that when the value is used the value of the gp register will be
     added in.  */
  HOWTO (R_ALPHA_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "GPREL32",		/* name */
	 false,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Used for an instruction that refers to memory off the GP register.  */
  HOWTO (R_ALPHA_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "ELF_LITERAL",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* This reloc only appears immediately following an ELF_LITERAL reloc.
     It identifies a use of the literal.  The symbol index is special:
     1 means the literal address is in the base register of a memory
     format instruction; 2 means the literal address is in the byte
     offset register of a byte-manipulation instruction; 3 means the
     literal address is in the target register of a jsr instruction.
     This does not actually do any relocation.  */
  HOWTO (R_ALPHA_LITUSE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_nil,	/* special_function */
	 "LITUSE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Load the gp register.  This is always used for a ldah instruction
     which loads the upper 16 bits of the gp register.  The symbol
     index of the GPDISP instruction is an offset in bytes to the lda
     instruction that loads the lower 16 bits.  The value to use for
     the relocation is the difference between the GP value and the
     current location; the load will always be done against a register
     holding the current address.

     NOTE: Unlike ECOFF, partial in-place relocation is not done.  If
     any offset is present in the instructions, it is an offset from
     the register to the ldah instruction.  This lets us avoid any
     stupid hackery like inventing a gp value to do partial relocation
     against.  Also unlike ECOFF, we do the whole relocation off of
     the GPDISP rather than a GPDISP_HI16/GPDISP_LO16 pair.  An odd,
     space consuming bit, that, since all the information was present
     in the GPDISP_HI16 reloc.  */
  HOWTO (R_ALPHA_GPDISP,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_gpdisp, /* special_function */
	 "GPDISP",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 21 bit branch.  */
  HOWTO (R_ALPHA_BRADDR,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 21,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "BRADDR",		/* name */
	 false,			/* partial_inplace */
	 0x1fffff,		/* src_mask */
	 0x1fffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A hint for a jump to a register.  */
  HOWTO (R_ALPHA_HINT,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 14,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "HINT",		/* name */
	 false,			/* partial_inplace */
	 0x3fff,		/* src_mask */
	 0x3fff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16 bit PC relative offset.  */
  HOWTO (R_ALPHA_SREL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL16",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 32 bit PC relative offset.  */
  HOWTO (R_ALPHA_SREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL32",		/* name */
	 false,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 64 bit PC relative offset.  */
  HOWTO (R_ALPHA_SREL64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL64",		/* name */
	 false,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* Push a value on the reloc evaluation stack.  */
  /* Not implemented -- it's dumb.  */
  HOWTO (R_ALPHA_OP_PUSH,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "OP_PUSH",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Store the value from the stack at the given address.  Store it in
     a bitfield of size r_size starting at bit position r_offset.  */
  /* Not implemented -- it's dumb.  */
  HOWTO (R_ALPHA_OP_STORE,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "OP_STORE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Subtract the reloc address from the value on the top of the
     relocation stack.  */
  /* Not implemented -- it's dumb.  */
  HOWTO (R_ALPHA_OP_PSUB,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "OP_PSUB",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Shift the value on the top of the relocation stack right by the
     given value.  */
  /* Not implemented -- it's dumb.  */
  HOWTO (R_ALPHA_OP_PRSHIFT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "OP_PRSHIFT",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Change the value of GP used by +r_addend until the next GPVALUE or the
     end of the input bfd.  */
  /* Not implemented -- it's dumb.  */
  HOWTO (R_ALPHA_GPVALUE,
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "GPVALUE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high 16 bits of the displacement from GP to the target.  */
  HOWTO (R_ALPHA_GPRELHIGH,
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "GPRELHIGH",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The low 16 bits of the displacement from GP to the target.  */
  HOWTO (R_ALPHA_GPRELLOW,
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "GPRELLOW",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 16-bit displacement from the GP to the target.  */
  /* XXX: Not implemented.  */
  HOWTO (R_ALPHA_IMMED_GP_16,
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "IMMED_GP_16",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high bits of a 32-bit displacement from the GP to the target; the
     low bits are supplied in the subsequent R_ALPHA_IMMED_LO32 relocs.  */
  /* XXX: Not implemented.  */
  HOWTO (R_ALPHA_IMMED_GP_HI32,
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "IMMED_GP_HI32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high bits of a 32-bit displacement to the starting address of the
     current section (the relocation target is ignored); the low bits are
     supplied in the subsequent R_ALPHA_IMMED_LO32 relocs.  */
  /* XXX: Not implemented.  */
  HOWTO (R_ALPHA_IMMED_SCN_HI32,
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "IMMED_SCN_HI32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high bits of a 32-bit displacement from the previous br, bsr, jsr
     or jmp insn (as tagged by a BRADDR or HINT reloc) to the target; the
     low bits are supplied by subsequent R_ALPHA_IMMED_LO32 relocs.  */
  /* XXX: Not implemented.  */
  HOWTO (R_ALPHA_IMMED_BR_HI32,
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "IMMED_BR_HI32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* The low 16 bits of a displacement calculated in a previous HI32 reloc.  */
  /* XXX: Not implemented.  */
  HOWTO (R_ALPHA_IMMED_LO32,
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 elf64_alpha_reloc_bad, /* special_function */
	 "IMMED_LO32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Misc ELF relocations.  */

  /* A dynamic relocation to copy the target into our .dynbss section.  */
  /* Not generated, as all Alpha objects use PIC, so it is not needed.  It
     is present because every other ELF has one, but should not be used
     because .dynbss is an ugly thing.  */
  HOWTO (R_ALPHA_COPY,
	 0,
	 0,
	 0,
	 false,
	 0,
	 complain_overflow_dont,
	 bfd_elf_generic_reloc,
	 "COPY",
	 false,
	 0,
	 0,
	 true),

  /* A dynamic relocation for a .got entry.  */
  HOWTO (R_ALPHA_GLOB_DAT,
	 0,
	 0,
	 0,
	 false,
	 0,
	 complain_overflow_dont,
	 bfd_elf_generic_reloc,
	 "GLOB_DAT",
	 false,
	 0,
	 0,
	 true),

  /* A dynamic relocation for a .plt entry.  */
  HOWTO (R_ALPHA_JMP_SLOT,
	 0,
	 0,
	 0,
	 false,
	 0,
	 complain_overflow_dont,
	 bfd_elf_generic_reloc,
	 "JMP_SLOT",
	 false,
	 0,
	 0,
	 true),

  /* A dynamic relocation to add the base of the DSO to a 64-bit field.  */
  HOWTO (R_ALPHA_RELATIVE,
	 0,
	 0,
	 0,
	 false,
	 0,
	 complain_overflow_dont,
	 bfd_elf_generic_reloc,
	 "RELATIVE",
	 false,
	 0,
	 0,
	 true)
};

/* A relocation function which doesn't do anything.  */

static bfd_reloc_status_type
elf64_alpha_reloc_nil (abfd, reloc, sym, data, sec, output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc;
     asymbol *sym ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *sec;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd)
    reloc->address += sec->output_offset;
  return bfd_reloc_ok;
}

/* A relocation function used for an unsupported reloc.  */

static bfd_reloc_status_type
elf64_alpha_reloc_bad (abfd, reloc, sym, data, sec, output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc;
     asymbol *sym ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *sec;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd)
    reloc->address += sec->output_offset;
  return bfd_reloc_notsupported;
}

/* Do the work of the GPDISP relocation.  */

static bfd_reloc_status_type
elf64_alpha_do_reloc_gpdisp (abfd, gpdisp, p_ldah, p_lda)
     bfd *abfd;
     bfd_vma gpdisp;
     bfd_byte *p_ldah;
     bfd_byte *p_lda;
{
  bfd_reloc_status_type ret = bfd_reloc_ok;
  bfd_vma addend;
  unsigned long i_ldah, i_lda;

  i_ldah = bfd_get_32 (abfd, p_ldah);
  i_lda = bfd_get_32 (abfd, p_lda);

  /* Complain if the instructions are not correct.  */
  if (((i_ldah >> 26) & 0x3f) != 0x09
      || ((i_lda >> 26) & 0x3f) != 0x08)
    ret = bfd_reloc_dangerous;

  /* Extract the user-supplied offset, mirroring the sign extensions
     that the instructions perform.  */
  addend = ((i_ldah & 0xffff) << 16) | (i_lda & 0xffff);
  addend = (addend ^ 0x80008000) - 0x80008000;

  gpdisp += addend;

  if ((bfd_signed_vma) gpdisp < -(bfd_signed_vma) 0x80000000
      || (bfd_signed_vma) gpdisp >= (bfd_signed_vma) 0x7fff8000)
    ret = bfd_reloc_overflow;

  /* compensate for the sign extension again.  */
  i_ldah = ((i_ldah & 0xffff0000)
	    | (((gpdisp >> 16) + ((gpdisp >> 15) & 1)) & 0xffff));
  i_lda = (i_lda & 0xffff0000) | (gpdisp & 0xffff);

  bfd_put_32 (abfd, i_ldah, p_ldah);
  bfd_put_32 (abfd, i_lda, p_lda);

  return ret;
}

/* The special function for the GPDISP reloc.  */

static bfd_reloc_status_type
elf64_alpha_reloc_gpdisp (abfd, reloc_entry, sym, data, input_section,
			  output_bfd, err_msg)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *sym ATTRIBUTE_UNUSED;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **err_msg;
{
  bfd_reloc_status_type ret;
  bfd_vma gp, relocation;
  bfd_byte *p_ldah, *p_lda;

  /* Don't do anything if we're not doing a final link.  */
  if (output_bfd)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (reloc_entry->address > input_section->_cooked_size ||
      reloc_entry->address + reloc_entry->addend > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* The gp used in the portion of the output object to which this
     input object belongs is cached on the input bfd.  */
  gp = _bfd_get_gp_value (abfd);

  relocation = (input_section->output_section->vma
		+ input_section->output_offset
		+ reloc_entry->address);

  p_ldah = (bfd_byte *) data + reloc_entry->address;
  p_lda = p_ldah + reloc_entry->addend;

  ret = elf64_alpha_do_reloc_gpdisp (abfd, gp - relocation, p_ldah, p_lda);

  /* Complain if the instructions are not correct.  */
  if (ret == bfd_reloc_dangerous)
    *err_msg = _("GPDISP relocation did not find ldah and lda instructions");

  return ret;
}

/* A mapping from BFD reloc types to Alpha ELF reloc types.  */

struct elf_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  int elf_reloc_val;
};

static const struct elf_reloc_map elf64_alpha_reloc_map[] =
{
  {BFD_RELOC_NONE,		R_ALPHA_NONE},
  {BFD_RELOC_32,		R_ALPHA_REFLONG},
  {BFD_RELOC_64,		R_ALPHA_REFQUAD},
  {BFD_RELOC_CTOR,		R_ALPHA_REFQUAD},
  {BFD_RELOC_GPREL32,		R_ALPHA_GPREL32},
  {BFD_RELOC_ALPHA_ELF_LITERAL,	R_ALPHA_LITERAL},
  {BFD_RELOC_ALPHA_LITUSE,	R_ALPHA_LITUSE},
  {BFD_RELOC_ALPHA_GPDISP,	R_ALPHA_GPDISP},
  {BFD_RELOC_23_PCREL_S2,	R_ALPHA_BRADDR},
  {BFD_RELOC_ALPHA_HINT,	R_ALPHA_HINT},
  {BFD_RELOC_16_PCREL,		R_ALPHA_SREL16},
  {BFD_RELOC_32_PCREL,		R_ALPHA_SREL32},
  {BFD_RELOC_64_PCREL,		R_ALPHA_SREL64},

/* The BFD_RELOC_ALPHA_USER_* relocations are used by the assembler to process
   the explicit !<reloc>!sequence relocations, and are mapped into the normal
   relocations at the end of processing.  */
  {BFD_RELOC_ALPHA_USER_LITERAL,	R_ALPHA_LITERAL},
  {BFD_RELOC_ALPHA_USER_LITUSE_BASE,	R_ALPHA_LITUSE},
  {BFD_RELOC_ALPHA_USER_LITUSE_BYTOFF,	R_ALPHA_LITUSE},
  {BFD_RELOC_ALPHA_USER_LITUSE_JSR,	R_ALPHA_LITUSE},
  {BFD_RELOC_ALPHA_USER_GPDISP,		R_ALPHA_GPDISP},
  {BFD_RELOC_ALPHA_USER_GPRELHIGH,	R_ALPHA_GPRELHIGH},
  {BFD_RELOC_ALPHA_USER_GPRELLOW,	R_ALPHA_GPRELLOW},
};

/* Given a BFD reloc type, return a HOWTO structure.  */

static reloc_howto_type *
elf64_alpha_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  const struct elf_reloc_map *i, *e;
  i = e = elf64_alpha_reloc_map;
  e += sizeof (elf64_alpha_reloc_map) / sizeof (struct elf_reloc_map);
  for (; i != e; ++i)
    {
      if (i->bfd_reloc_val == code)
	return &elf64_alpha_howto_table[i->elf_reloc_val];
    }
  return 0;
}

/* Given an Alpha ELF reloc type, fill in an arelent structure.  */

static void
elf64_alpha_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf64_Internal_Rela *dst;
{
  unsigned r_type;

  r_type = ELF64_R_TYPE(dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_ALPHA_max);
  cache_ptr->howto = &elf64_alpha_howto_table[r_type];
}

/* These functions do relaxation for Alpha ELF.

   Currently I'm only handling what I can do with existing compiler
   and assembler support, which means no instructions are removed,
   though some may be nopped.  At this time GCC does not emit enough
   information to do all of the relaxing that is possible.  It will
   take some not small amount of work for that to happen.

   There are a couple of interesting papers that I once read on this
   subject, that I cannot find references to at the moment, that
   related to Alpha in particular.  They are by David Wall, then of
   DEC WRL.  */

#define OP_LDA		0x08
#define OP_LDAH		0x09
#define INSN_JSR	0x68004000
#define INSN_JSR_MASK	0xfc00c000
#define OP_LDQ		0x29
#define OP_BR		0x30
#define OP_BSR		0x34
#define INSN_UNOP	0x2fe00000

struct alpha_relax_info
{
  bfd *abfd;
  asection *sec;
  bfd_byte *contents;
  Elf_Internal_Rela *relocs, *relend;
  struct bfd_link_info *link_info;
  boolean changed_contents;
  boolean changed_relocs;
  bfd_vma gp;
  bfd *gotobj;
  asection *tsec;
  struct alpha_elf_link_hash_entry *h;
  struct alpha_elf_got_entry *gotent;
  unsigned char other;
};

static Elf_Internal_Rela * elf64_alpha_relax_with_lituse
  PARAMS((struct alpha_relax_info *info, bfd_vma symval,
          Elf_Internal_Rela *irel, Elf_Internal_Rela *irelend));

static boolean elf64_alpha_relax_without_lituse
  PARAMS((struct alpha_relax_info *info, bfd_vma symval,
          Elf_Internal_Rela *irel));

static bfd_vma elf64_alpha_relax_opt_call
  PARAMS((struct alpha_relax_info *info, bfd_vma symval));

static boolean elf64_alpha_relax_section
  PARAMS((bfd *abfd, asection *sec, struct bfd_link_info *link_info,
	  boolean *again));

static Elf_Internal_Rela *
elf64_alpha_find_reloc_at_ofs (rel, relend, offset, type)
     Elf_Internal_Rela *rel, *relend;
     bfd_vma offset;
     int type;
{
  while (rel < relend)
    {
      if (rel->r_offset == offset && ELF64_R_TYPE (rel->r_info) == type)
	return rel;
      ++rel;
    }
  return NULL;
}

static Elf_Internal_Rela *
elf64_alpha_relax_with_lituse (info, symval, irel, irelend)
     struct alpha_relax_info *info;
     bfd_vma symval;
     Elf_Internal_Rela *irel, *irelend;
{
  Elf_Internal_Rela *urel;
  int flags, count, i;
  bfd_signed_vma disp;
  boolean fits16;
  boolean fits32;
  boolean lit_reused = false;
  boolean all_optimized = true;
  unsigned int lit_insn;

  lit_insn = bfd_get_32 (info->abfd, info->contents + irel->r_offset);
  if (lit_insn >> 26 != OP_LDQ)
    {
      ((*_bfd_error_handler)
       ("%s: %s+0x%lx: warning: LITERAL relocation against unexpected insn",
	bfd_get_filename (info->abfd), info->sec->name,
	(unsigned long)irel->r_offset));
      return irel;
    }

  /* Summarize how this particular LITERAL is used.  */
  for (urel = irel+1, flags = count = 0; urel < irelend; ++urel, ++count)
    {
      if (ELF64_R_TYPE (urel->r_info) != R_ALPHA_LITUSE)
	break;
      if (urel->r_addend >= 0 && urel->r_addend <= 3)
	flags |= 1 << urel->r_addend;
    }

  /* A little preparation for the loop...  */
  disp = symval - info->gp;

  for (urel = irel+1, i = 0; i < count; ++i, ++urel)
    {
      unsigned int insn;
      int insn_disp;
      bfd_signed_vma xdisp;

      insn = bfd_get_32 (info->abfd, info->contents + urel->r_offset);

      switch (urel->r_addend)
	{
	default: /* 0 = ADDRESS FORMAT */
	  /* This type is really just a placeholder to note that all
	     uses cannot be optimized, but to still allow some.  */
	  all_optimized = false;
	  break;

	case 1: /* MEM FORMAT */
	  /* We can always optimize 16-bit displacements.  */

	  /* Extract the displacement from the instruction, sign-extending
	     it if necessary, then test whether it is within 16 or 32 bits
	     displacement from GP.  */
	  insn_disp = insn & 0x0000ffff;
	  if (insn_disp & 0x00008000)
	    insn_disp |= 0xffff0000;  /* Negative: sign-extend.  */

	  xdisp = disp + insn_disp;
	  fits16 = (xdisp >= - (bfd_signed_vma) 0x00008000 && xdisp < 0x00008000);
	  fits32 = (xdisp >= - (bfd_signed_vma) 0x80000000 && xdisp < 0x7fff8000);

	  if (fits16)
	    {
	      /* Take the op code and dest from this insn, take the base
		 register from the literal insn.  Leave the offset alone.  */
	      insn = (insn & 0xffe0ffff) | (lit_insn & 0x001f0000);
	      urel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info),
					   R_ALPHA_GPRELLOW);
	      urel->r_addend = irel->r_addend;
	      info->changed_relocs = true;

	      bfd_put_32 (info->abfd, insn, info->contents + urel->r_offset);
	      info->changed_contents = true;
	    }

	  /* If all mem+byte, we can optimize 32-bit mem displacements.  */
	  else if (fits32 && !(flags & ~6))
	    {
	      /* FIXME: sanity check that lit insn Ra is mem insn Rb.  */

	      irel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info),
					   R_ALPHA_GPRELHIGH);
	      lit_insn = (OP_LDAH << 26) | (lit_insn & 0x03ff0000);
	      bfd_put_32 (info->abfd, lit_insn,
			  info->contents + irel->r_offset);
	      lit_reused = true;
	      info->changed_contents = true;

	      urel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info),
					   R_ALPHA_GPRELLOW);
	      urel->r_addend = irel->r_addend;
	      info->changed_relocs = true;
	    }
	  else
	    all_optimized = false;
	  break;

	case 2: /* BYTE OFFSET FORMAT */
	  /* We can always optimize byte instructions.  */

	  /* FIXME: sanity check the insn for byte op.  Check that the
	     literal dest reg is indeed Rb in the byte insn.  */

	  insn = (insn & ~0x001ff000) | ((symval & 7) << 13) | 0x1000;

	  urel->r_info = ELF64_R_INFO (0, R_ALPHA_NONE);
	  urel->r_addend = 0;
	  info->changed_relocs = true;

	  bfd_put_32 (info->abfd, insn, info->contents + urel->r_offset);
	  info->changed_contents = true;
	  break;

	case 3: /* CALL FORMAT */
	  {
	    /* If not zero, place to jump without needing pv.  */
	    bfd_vma optdest = elf64_alpha_relax_opt_call (info, symval);
	    bfd_vma org = (info->sec->output_section->vma
			   + info->sec->output_offset
			   + urel->r_offset + 4);
	    bfd_signed_vma odisp;

	    odisp = (optdest ? optdest : symval) - org;
	    if (odisp >= -0x400000 && odisp < 0x400000)
	      {
		Elf_Internal_Rela *xrel;

		/* Preserve branch prediction call stack when possible.  */
		if ((insn & INSN_JSR_MASK) == INSN_JSR)
		  insn = (OP_BSR << 26) | (insn & 0x03e00000);
		else
		  insn = (OP_BR << 26) | (insn & 0x03e00000);

		urel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info),
					     R_ALPHA_BRADDR);
		urel->r_addend = irel->r_addend;

		if (optdest)
		  urel->r_addend += optdest - symval;
		else
		  all_optimized = false;

		bfd_put_32 (info->abfd, insn, info->contents + urel->r_offset);

		/* Kill any HINT reloc that might exist for this insn.  */
		xrel = (elf64_alpha_find_reloc_at_ofs
			(info->relocs, info->relend, urel->r_offset,
			 R_ALPHA_HINT));
		if (xrel)
		  xrel->r_info = ELF64_R_INFO (0, R_ALPHA_NONE);

		info->changed_contents = true;
		info->changed_relocs = true;
	      }
	    else
	      all_optimized = false;

	    /* ??? If target gp == current gp we can eliminate the gp reload.
	       This does depend on every place a gp could be reloaded will
	       be, which currently happens for all code produced by gcc, but
	       not necessarily by hand-coded assembly, or if sibling calls
	       are enabled in gcc.

	       Perhaps conditionalize this on a flag being set in the target
	       object file's header, and have gcc set it?  */
	  }
	  break;
	}
    }

  /* If all cases were optimized, we can reduce the use count on this
     got entry by one, possibly eliminating it.  */
  if (all_optimized)
    {
      info->gotent->use_count -= 1;
      alpha_elf_tdata (info->gotent->gotobj)->total_got_entries -= 1;
      if (!info->h)
	alpha_elf_tdata (info->gotent->gotobj)->n_local_got_entries -= 1;

      /* If the literal instruction is no longer needed (it may have been
	 reused.  We can eliminate it.
	 ??? For now, I don't want to deal with compacting the section,
	 so just nop it out.  */
      if (!lit_reused)
	{
	  irel->r_info = ELF64_R_INFO (0, R_ALPHA_NONE);
	  info->changed_relocs = true;

	  bfd_put_32 (info->abfd, INSN_UNOP, info->contents + irel->r_offset);
	  info->changed_contents = true;
	}
    }

  return irel + count;
}

static bfd_vma
elf64_alpha_relax_opt_call (info, symval)
     struct alpha_relax_info *info;
     bfd_vma symval;
{
  /* If the function has the same gp, and we can identify that the
     function does not use its function pointer, we can eliminate the
     address load.  */

  /* If the symbol is marked NOPV, we are being told the function never
     needs its procedure value.  */
  if ((info->other & STO_ALPHA_STD_GPLOAD) == STO_ALPHA_NOPV)
    return symval;

  /* If the symbol is marked STD_GP, we are being told the function does
     a normal ldgp in the first two words.  */
  else if ((info->other & STO_ALPHA_STD_GPLOAD) == STO_ALPHA_STD_GPLOAD)
    ;

  /* Otherwise, we may be able to identify a GP load in the first two
     words, which we can then skip.  */
  else
    {
      Elf_Internal_Rela *tsec_relocs, *tsec_relend, *tsec_free, *gpdisp;
      bfd_vma ofs;

      /* Load the relocations from the section that the target symbol is in.  */
      if (info->sec == info->tsec)
	{
	  tsec_relocs = info->relocs;
	  tsec_relend = info->relend;
	  tsec_free = NULL;
	}
      else
	{
	  tsec_relocs = (_bfd_elf64_link_read_relocs
		         (info->abfd, info->tsec, (PTR) NULL,
			 (Elf_Internal_Rela *) NULL,
			 info->link_info->keep_memory));
	  if (tsec_relocs == NULL)
	    return 0;
	  tsec_relend = tsec_relocs + info->tsec->reloc_count;
	  tsec_free = (info->link_info->keep_memory ? NULL : tsec_relocs);
	}

      /* Recover the symbol's offset within the section.  */
      ofs = (symval - info->tsec->output_section->vma
	     - info->tsec->output_offset);

      /* Look for a GPDISP reloc.  */
      gpdisp = (elf64_alpha_find_reloc_at_ofs
		(tsec_relocs, tsec_relend, ofs, R_ALPHA_GPDISP));

      if (!gpdisp || gpdisp->r_addend != 4)
	{
	  if (tsec_free)
	    free (tsec_free);
	  return 0;
	}
      if (tsec_free)
        free (tsec_free);
    }

  /* We've now determined that we can skip an initial gp load.  Verify
     that the call and the target use the same gp.   */
  if (info->link_info->hash->creator != info->tsec->owner->xvec
      || info->gotobj != alpha_elf_tdata (info->tsec->owner)->gotobj)
    return 0;

  return symval + 8;
}

static boolean
elf64_alpha_relax_without_lituse (info, symval, irel)
     struct alpha_relax_info *info;
     bfd_vma symval;
     Elf_Internal_Rela *irel;
{
  unsigned int insn;
  bfd_signed_vma disp;

  /* Get the instruction.  */
  insn = bfd_get_32 (info->abfd, info->contents + irel->r_offset);

  if (insn >> 26 != OP_LDQ)
    {
      ((*_bfd_error_handler)
       ("%s: %s+0x%lx: warning: LITERAL relocation against unexpected insn",
	bfd_get_filename (info->abfd), info->sec->name,
	(unsigned long) irel->r_offset));
      return true;
    }

  /* So we aren't told much.  Do what we can with the address load and
     fake the rest.  All of the optimizations here require that the
     offset from the GP fit in 16 bits.  */

  disp = symval - info->gp;
  if (disp < -0x8000 || disp >= 0x8000)
    return true;

  /* On the LITERAL instruction itself, consider exchanging
     `ldq R,X(gp)' for `lda R,Y(gp)'.  */

  insn = (OP_LDA << 26) | (insn & 0x03ff0000);
  bfd_put_32 (info->abfd, insn, info->contents + irel->r_offset);
  info->changed_contents = true;

  irel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info), R_ALPHA_GPRELLOW);
  info->changed_relocs = true;

  /* Reduce the use count on this got entry by one, possibly
     eliminating it.  */
  info->gotent->use_count -= 1;
  alpha_elf_tdata (info->gotent->gotobj)->total_got_entries -= 1;
  if (!info->h)
    alpha_elf_tdata (info->gotent->gotobj)->n_local_got_entries -= 1;

  /* ??? Search forward through this basic block looking for insns
     that use the target register.  Stop after an insn modifying the
     register is seen, or after a branch or call.

     Any such memory load insn may be substituted by a load directly
     off the GP.  This allows the memory load insn to be issued before
     the calculated GP register would otherwise be ready.

     Any such jsr insn can be replaced by a bsr if it is in range.

     This would mean that we'd have to _add_ relocations, the pain of
     which gives one pause.  */

  return true;
}

static boolean
elf64_alpha_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     boolean *again;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *free_relocs = NULL;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *free_contents = NULL;
  Elf64_External_Sym *extsyms = NULL;
  Elf64_External_Sym *free_extsyms = NULL;
  struct alpha_elf_got_entry **local_got_entries;
  struct alpha_relax_info info;

  /* We are not currently changing any sizes, so only one pass.  */
  *again = false;

  if (link_info->relocateable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0)
    return true;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (sec->_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  local_got_entries = alpha_elf_tdata(abfd)->local_got_entries;

  /* Load the relocations for this section.  */
  internal_relocs = (_bfd_elf64_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;
  if (! link_info->keep_memory)
    free_relocs = internal_relocs;

  memset(&info, 0, sizeof (info));
  info.abfd = abfd;
  info.sec = sec;
  info.link_info = link_info;
  info.relocs = internal_relocs;
  info.relend = irelend = internal_relocs + sec->reloc_count;

  /* Find the GP for this object.  */
  info.gotobj = alpha_elf_tdata (abfd)->gotobj;
  if (info.gotobj)
    {
      asection *sgot = alpha_elf_tdata (info.gotobj)->got;
      info.gp = _bfd_get_gp_value (info.gotobj);
      if (info.gp == 0)
	{
	  info.gp = (sgot->output_section->vma
		     + sgot->output_offset
		     + 0x8000);
	  _bfd_set_gp_value (info.gotobj, info.gp);
	}
    }

  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;
      Elf_Internal_Sym isym;
      struct alpha_elf_got_entry *gotent;

      if (ELF64_R_TYPE (irel->r_info) != (int) R_ALPHA_LITERAL)
	continue;

      /* Get the section contents.  */
      if (info.contents == NULL)
	{
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    info.contents = elf_section_data (sec)->this_hdr.contents;
	  else
	    {
	      info.contents = (bfd_byte *) bfd_malloc (sec->_raw_size);
	      if (info.contents == NULL)
		goto error_return;
	      free_contents = info.contents;

	      if (! bfd_get_section_contents (abfd, sec, info.contents,
					      (file_ptr) 0, sec->_raw_size))
		goto error_return;
	    }
	}

      /* Read this BFD's symbols if we haven't done so already.  */
      if (extsyms == NULL)
	{
	  if (symtab_hdr->contents != NULL)
	    extsyms = (Elf64_External_Sym *) symtab_hdr->contents;
	  else
	    {
	      extsyms = ((Elf64_External_Sym *)
			 bfd_malloc (symtab_hdr->sh_size));
	      if (extsyms == NULL)
		goto error_return;
	      free_extsyms = extsyms;
	      if (bfd_seek (abfd, symtab_hdr->sh_offset, SEEK_SET) != 0
		  || (bfd_read (extsyms, 1, symtab_hdr->sh_size, abfd)
		      != symtab_hdr->sh_size))
		goto error_return;
	    }
	}

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF64_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  bfd_elf64_swap_symbol_in (abfd,
				    extsyms + ELF64_R_SYM (irel->r_info),
				    &isym);
	  if (isym.st_shndx == SHN_UNDEF)
	    info.tsec = bfd_und_section_ptr;
	  else if (isym.st_shndx > 0 && isym.st_shndx < SHN_LORESERVE)
	    info.tsec = bfd_section_from_elf_index (abfd, isym.st_shndx);
	  else if (isym.st_shndx == SHN_ABS)
	    info.tsec = bfd_abs_section_ptr;
	  else if (isym.st_shndx == SHN_COMMON)
	    info.tsec = bfd_com_section_ptr;
	  else
	    continue;	/* who knows.  */

	  info.h = NULL;
	  info.other = isym.st_other;
	  gotent = local_got_entries[ELF64_R_SYM(irel->r_info)];
	  symval = isym.st_value;
	}
      else
	{
	  unsigned long indx;
	  struct alpha_elf_link_hash_entry *h;

	  indx = ELF64_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = alpha_elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);

	  while (h->root.root.type == bfd_link_hash_indirect
		 || h->root.root.type == bfd_link_hash_warning)
	    h = (struct alpha_elf_link_hash_entry *)h->root.root.u.i.link;

	  /* We can't do anthing with undefined or dynamic symbols.  */
	  if (h->root.root.type == bfd_link_hash_undefined
	      || h->root.root.type == bfd_link_hash_undefweak
	      || alpha_elf_dynamic_symbol_p (&h->root, link_info))
	    continue;

	  info.h = h;
	  info.gotent = gotent;
	  info.tsec = h->root.root.u.def.section;
	  info.other = h->root.other;
	  gotent = h->got_entries;
	  symval = h->root.root.u.def.value;
	}

      /* Search for the got entry to be used by this relocation.  */
      while (gotent->gotobj != info.gotobj || gotent->addend != irel->r_addend)
	gotent = gotent->next;
      info.gotent = gotent;

      symval += info.tsec->output_section->vma + info.tsec->output_offset;
      symval += irel->r_addend;

      BFD_ASSERT(info.gotent != NULL);

      /* If there exist LITUSE relocations immediately following, this
	 opens up all sorts of interesting optimizations, because we
	 now know every location that this address load is used.  */

      if (irel+1 < irelend && ELF64_R_TYPE (irel[1].r_info) == R_ALPHA_LITUSE)
	{
	  irel = elf64_alpha_relax_with_lituse (&info, symval, irel, irelend);
	  if (irel == NULL)
	    goto error_return;
	}
      else
	{
	  if (!elf64_alpha_relax_without_lituse (&info, symval, irel))
	    goto error_return;
	}
    }

  if (!elf64_alpha_size_got_sections (abfd, link_info))
    return false;

  if (info.changed_relocs)
    {
      elf_section_data (sec)->relocs = internal_relocs;
    }
  else if (free_relocs != NULL)
    {
      free (free_relocs);
    }

  if (info.changed_contents)
    {
      elf_section_data (sec)->this_hdr.contents = info.contents;
    }
  else if (free_contents != NULL)
    {
      if (! link_info->keep_memory)
	free (free_contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = info.contents;
	}
    }

  if (free_extsyms != NULL)
    {
      if (! link_info->keep_memory)
	free (free_extsyms);
      else
	{
	  /* Cache the symbols for elf_link_input_bfd.  */
	  symtab_hdr->contents = extsyms;
	}
    }

  *again = info.changed_contents || info.changed_relocs;

  return true;

 error_return:
  if (free_relocs != NULL)
    free (free_relocs);
  if (free_contents != NULL)
    free (free_contents);
  if (free_extsyms != NULL)
    free (free_extsyms);
  return false;
}

/* PLT/GOT Stuff */
#define PLT_HEADER_SIZE 32
#define PLT_HEADER_WORD1	0xc3600000	/* br   $27,.+4     */
#define PLT_HEADER_WORD2	0xa77b000c	/* ldq  $27,12($27) */
#define PLT_HEADER_WORD3	0x47ff041f	/* nop              */
#define PLT_HEADER_WORD4	0x6b7b0000	/* jmp  $27,($27)   */

#define PLT_ENTRY_SIZE 12
#define PLT_ENTRY_WORD1		0xc3800000	/* br   $28, plt0   */
#define PLT_ENTRY_WORD2		0
#define PLT_ENTRY_WORD3		0

#define MAX_GOT_ENTRIES		(64*1024 / 8)

#ifndef ELF_DYNAMIC_INTERPRETER
#define ELF_DYNAMIC_INTERPRETER "/usr/lib/ld.so"
#endif

/* Handle an Alpha specific section when reading an object file.  This
   is called when elfcode.h finds a section with an unknown type.
   FIXME: We need to handle the SHF_ALPHA_GPREL flag, but I'm not sure
   how to.  */

static boolean
elf64_alpha_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf64_Internal_Shdr *hdr;
     char *name;
{
  asection *newsect;

  /* There ought to be a place to keep ELF backend specific flags, but
     at the moment there isn't one.  We just keep track of the
     sections by their name, instead.  Fortunately, the ABI gives
     suggested names for all the MIPS specific sections, so we will
     probably get away with this.  */
  switch (hdr->sh_type)
    {
    case SHT_ALPHA_DEBUG:
      if (strcmp (name, ".mdebug") != 0)
	return false;
      break;
#ifdef ERIC_neverdef
    case SHT_ALPHA_REGINFO:
      if (strcmp (name, ".reginfo") != 0
	  || hdr->sh_size != sizeof (Elf64_External_RegInfo))
	return false;
      break;
#endif
    default:
      return false;
    }

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
    return false;
  newsect = hdr->bfd_section;

  if (hdr->sh_type == SHT_ALPHA_DEBUG)
    {
      if (! bfd_set_section_flags (abfd, newsect,
				   (bfd_get_section_flags (abfd, newsect)
				    | SEC_DEBUGGING)))
	return false;
    }

#ifdef ERIC_neverdef
  /* For a .reginfo section, set the gp value in the tdata information
     from the contents of this section.  We need the gp value while
     processing relocs, so we just get it now.  */
  if (hdr->sh_type == SHT_ALPHA_REGINFO)
    {
      Elf64_External_RegInfo ext;
      Elf64_RegInfo s;

      if (! bfd_get_section_contents (abfd, newsect, (PTR) &ext,
				      (file_ptr) 0, sizeof ext))
	return false;
      bfd_alpha_elf64_swap_reginfo_in (abfd, &ext, &s);
      elf_gp (abfd) = s.ri_gp_value;
    }
#endif

  return true;
}

/* Set the correct type for an Alpha ELF section.  We do this by the
   section name, which is a hack, but ought to work.  */

static boolean
elf64_alpha_fake_sections (abfd, hdr, sec)
     bfd *abfd;
     Elf64_Internal_Shdr *hdr;
     asection *sec;
{
  register const char *name;

  name = bfd_get_section_name (abfd, sec);

  if (strcmp (name, ".mdebug") == 0)
    {
      hdr->sh_type = SHT_ALPHA_DEBUG;
      /* In a shared object on Irix 5.3, the .mdebug section has an
         entsize of 0.  FIXME: Does this matter?  */
      if ((abfd->flags & DYNAMIC) != 0 )
	hdr->sh_entsize = 0;
      else
	hdr->sh_entsize = 1;
    }
#ifdef ERIC_neverdef
  else if (strcmp (name, ".reginfo") == 0)
    {
      hdr->sh_type = SHT_ALPHA_REGINFO;
      /* In a shared object on Irix 5.3, the .reginfo section has an
         entsize of 0x18.  FIXME: Does this matter?  */
      if ((abfd->flags & DYNAMIC) != 0)
	hdr->sh_entsize = sizeof (Elf64_External_RegInfo);
      else
	hdr->sh_entsize = 1;

      /* Force the section size to the correct value, even if the
	 linker thinks it is larger.  The link routine below will only
	 write out this much data for .reginfo.  */
      hdr->sh_size = sec->_raw_size = sizeof (Elf64_External_RegInfo);
    }
  else if (strcmp (name, ".hash") == 0
	   || strcmp (name, ".dynamic") == 0
	   || strcmp (name, ".dynstr") == 0)
    {
      hdr->sh_entsize = 0;
      hdr->sh_info = SIZEOF_ALPHA_DYNSYM_SECNAMES;
    }
#endif
  else if (strcmp (name, ".sdata") == 0
	   || strcmp (name, ".sbss") == 0
	   || strcmp (name, ".lit4") == 0
	   || strcmp (name, ".lit8") == 0)
    hdr->sh_flags |= SHF_ALPHA_GPREL;

  return true;
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We use it to put .comm items in .sbss, and not .bss.  */

static boolean
elf64_alpha_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     const Elf_Internal_Sym *sym;
     const char **namep ATTRIBUTE_UNUSED;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp;
{
  if (sym->st_shndx == SHN_COMMON
      && !info->relocateable
      && sym->st_size <= bfd_get_gp_size (abfd))
    {
      /* Common symbols less than or equal to -G nn bytes are
	 automatically put into .sbss.  */

      asection *scomm = bfd_get_section_by_name (abfd, ".scommon");

      if (scomm == NULL)
	{
	  scomm = bfd_make_section (abfd, ".scommon");
	  if (scomm == NULL
	      || !bfd_set_section_flags (abfd, scomm, (SEC_ALLOC
						       | SEC_IS_COMMON
						       | SEC_LINKER_CREATED)))
	    return false;
	}

      *secp = scomm;
      *valp = sym->st_size;
    }

  return true;
}

/* Create the .got section.  */

static boolean
elf64_alpha_create_got_section(abfd, info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
{
  asection *s;

  if (bfd_get_section_by_name (abfd, ".got"))
    return true;

  s = bfd_make_section (abfd, ".got");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					   | SEC_HAS_CONTENTS
					   | SEC_IN_MEMORY
					   | SEC_LINKER_CREATED))
      || !bfd_set_section_alignment (abfd, s, 3))
    return false;

  alpha_elf_tdata (abfd)->got = s;

  return true;
}

/* Create all the dynamic sections.  */

static boolean
elf64_alpha_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection *s;
  struct elf_link_hash_entry *h;

  /* We need to create .plt, .rela.plt, .got, and .rela.got sections.  */

  s = bfd_make_section (abfd, ".plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					    | SEC_HAS_CONTENTS
					    | SEC_IN_MEMORY
					    | SEC_LINKER_CREATED
					    | SEC_CODE))
      || ! bfd_set_section_alignment (abfd, s, 3))
    return false;

  /* Define the symbol _PROCEDURE_LINKAGE_TABLE_ at the start of the
     .plt section.  */
  h = NULL;
  if (! (_bfd_generic_link_add_one_symbol
	 (info, abfd, "_PROCEDURE_LINKAGE_TABLE_", BSF_GLOBAL, s,
	  (bfd_vma) 0, (const char *) NULL, false,
	  get_elf_backend_data (abfd)->collect,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! _bfd_elf_link_record_dynamic_symbol (info, h))
    return false;

  s = bfd_make_section (abfd, ".rela.plt");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					   | SEC_HAS_CONTENTS
					   | SEC_IN_MEMORY
					   | SEC_LINKER_CREATED
					   | SEC_READONLY))
      || ! bfd_set_section_alignment (abfd, s, 3))
    return false;

  /* We may or may not have created a .got section for this object, but
     we definitely havn't done the rest of the work.  */

  if (!elf64_alpha_create_got_section (abfd, info))
    return false;

  s = bfd_make_section(abfd, ".rela.got");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					   | SEC_HAS_CONTENTS
					   | SEC_IN_MEMORY
					   | SEC_LINKER_CREATED
					   | SEC_READONLY))
      || !bfd_set_section_alignment (abfd, s, 3))
    return false;

  /* Define the symbol _GLOBAL_OFFSET_TABLE_ at the start of the
     dynobj's .got section.  We don't do this in the linker script
     because we don't want to define the symbol if we are not creating
     a global offset table.  */
  h = NULL;
  if (!(_bfd_generic_link_add_one_symbol
	(info, abfd, "_GLOBAL_OFFSET_TABLE_", BSF_GLOBAL,
	 alpha_elf_tdata(abfd)->got, (bfd_vma) 0, (const char *) NULL,
	 false, get_elf_backend_data (abfd)->collect,
	 (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! _bfd_elf_link_record_dynamic_symbol (info, h))
    return false;

  elf_hash_table (info)->hgot = h;

  return true;
}

/* Read ECOFF debugging information from a .mdebug section into a
   ecoff_debug_info structure.  */

static boolean
elf64_alpha_read_ecoff_info (abfd, section, debug)
     bfd *abfd;
     asection *section;
     struct ecoff_debug_info *debug;
{
  HDRR *symhdr;
  const struct ecoff_debug_swap *swap;
  char *ext_hdr = NULL;

  swap = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;
  memset (debug, 0, sizeof (*debug));

  ext_hdr = (char *) bfd_malloc ((size_t) swap->external_hdr_size);
  if (ext_hdr == NULL && swap->external_hdr_size != 0)
    goto error_return;

  if (bfd_get_section_contents (abfd, section, ext_hdr, (file_ptr) 0,
				swap->external_hdr_size)
      == false)
    goto error_return;

  symhdr = &debug->symbolic_header;
  (*swap->swap_hdr_in) (abfd, ext_hdr, symhdr);

  /* The symbolic header contains absolute file offsets and sizes to
     read.  */
#define READ(ptr, offset, count, size, type)				\
  if (symhdr->count == 0)						\
    debug->ptr = NULL;							\
  else									\
    {									\
      debug->ptr = (type) bfd_malloc ((size_t) (size * symhdr->count));	\
      if (debug->ptr == NULL)						\
	goto error_return;						\
      if (bfd_seek (abfd, (file_ptr) symhdr->offset, SEEK_SET) != 0	\
	  || (bfd_read (debug->ptr, size, symhdr->count,		\
			abfd) != size * symhdr->count))			\
	goto error_return;						\
    }

  READ (line, cbLineOffset, cbLine, sizeof (unsigned char), unsigned char *);
  READ (external_dnr, cbDnOffset, idnMax, swap->external_dnr_size, PTR);
  READ (external_pdr, cbPdOffset, ipdMax, swap->external_pdr_size, PTR);
  READ (external_sym, cbSymOffset, isymMax, swap->external_sym_size, PTR);
  READ (external_opt, cbOptOffset, ioptMax, swap->external_opt_size, PTR);
  READ (external_aux, cbAuxOffset, iauxMax, sizeof (union aux_ext),
	union aux_ext *);
  READ (ss, cbSsOffset, issMax, sizeof (char), char *);
  READ (ssext, cbSsExtOffset, issExtMax, sizeof (char), char *);
  READ (external_fdr, cbFdOffset, ifdMax, swap->external_fdr_size, PTR);
  READ (external_rfd, cbRfdOffset, crfd, swap->external_rfd_size, PTR);
  READ (external_ext, cbExtOffset, iextMax, swap->external_ext_size, PTR);
#undef READ

  debug->fdr = NULL;
  debug->adjust = NULL;

  return true;

 error_return:
  if (ext_hdr != NULL)
    free (ext_hdr);
  if (debug->line != NULL)
    free (debug->line);
  if (debug->external_dnr != NULL)
    free (debug->external_dnr);
  if (debug->external_pdr != NULL)
    free (debug->external_pdr);
  if (debug->external_sym != NULL)
    free (debug->external_sym);
  if (debug->external_opt != NULL)
    free (debug->external_opt);
  if (debug->external_aux != NULL)
    free (debug->external_aux);
  if (debug->ss != NULL)
    free (debug->ss);
  if (debug->ssext != NULL)
    free (debug->ssext);
  if (debug->external_fdr != NULL)
    free (debug->external_fdr);
  if (debug->external_rfd != NULL)
    free (debug->external_rfd);
  if (debug->external_ext != NULL)
    free (debug->external_ext);
  return false;
}

/* Alpha ELF local labels start with '$'.  */

static boolean
elf64_alpha_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
{
  return name[0] == '$';
}

/* Alpha ELF follows MIPS ELF in using a special find_nearest_line
   routine in order to handle the ECOFF debugging information.  We
   still call this mips_elf_find_line because of the slot
   find_line_info in elf_obj_tdata is declared that way.  */

struct mips_elf_find_line
{
  struct ecoff_debug_info d;
  struct ecoff_find_line i;
};

static boolean
elf64_alpha_find_nearest_line (abfd, section, symbols, offset, filename_ptr,
			       functionname_ptr, line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     const char **filename_ptr;
     const char **functionname_ptr;
     unsigned int *line_ptr;
{
  asection *msec;

  if (_bfd_dwarf2_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr, 0,
				     &elf_tdata (abfd)->dwarf2_find_line_info))
    return true;

  msec = bfd_get_section_by_name (abfd, ".mdebug");
  if (msec != NULL)
    {
      flagword origflags;
      struct mips_elf_find_line *fi;
      const struct ecoff_debug_swap * const swap =
	get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

      /* If we are called during a link, alpha_elf_final_link may have
	 cleared the SEC_HAS_CONTENTS field.  We force it back on here
	 if appropriate (which it normally will be).  */
      origflags = msec->flags;
      if (elf_section_data (msec)->this_hdr.sh_type != SHT_NOBITS)
	msec->flags |= SEC_HAS_CONTENTS;

      fi = elf_tdata (abfd)->find_line_info;
      if (fi == NULL)
	{
	  bfd_size_type external_fdr_size;
	  char *fraw_src;
	  char *fraw_end;
	  struct fdr *fdr_ptr;

	  fi = ((struct mips_elf_find_line *)
		bfd_zalloc (abfd, sizeof (struct mips_elf_find_line)));
	  if (fi == NULL)
	    {
	      msec->flags = origflags;
	      return false;
	    }

	  if (!elf64_alpha_read_ecoff_info (abfd, msec, &fi->d))
	    {
	      msec->flags = origflags;
	      return false;
	    }

	  /* Swap in the FDR information.  */
	  fi->d.fdr = ((struct fdr *)
		       bfd_alloc (abfd,
				  (fi->d.symbolic_header.ifdMax *
				   sizeof (struct fdr))));
	  if (fi->d.fdr == NULL)
	    {
	      msec->flags = origflags;
	      return false;
	    }
	  external_fdr_size = swap->external_fdr_size;
	  fdr_ptr = fi->d.fdr;
	  fraw_src = (char *) fi->d.external_fdr;
	  fraw_end = (fraw_src
		      + fi->d.symbolic_header.ifdMax * external_fdr_size);
	  for (; fraw_src < fraw_end; fraw_src += external_fdr_size, fdr_ptr++)
	    (*swap->swap_fdr_in) (abfd, (PTR) fraw_src, fdr_ptr);

	  elf_tdata (abfd)->find_line_info = fi;

	  /* Note that we don't bother to ever free this information.
             find_nearest_line is either called all the time, as in
             objdump -l, so the information should be saved, or it is
             rarely called, as in ld error messages, so the memory
             wasted is unimportant.  Still, it would probably be a
             good idea for free_cached_info to throw it away.  */
	}

      if (_bfd_ecoff_locate_line (abfd, section, offset, &fi->d, swap,
				  &fi->i, filename_ptr, functionname_ptr,
				  line_ptr))
	{
	  msec->flags = origflags;
	  return true;
	}

      msec->flags = origflags;
    }

  /* Fall back on the generic ELF find_nearest_line routine.  */

  return _bfd_elf_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr);
}

/* Structure used to pass information to alpha_elf_output_extsym.  */

struct extsym_info
{
  bfd *abfd;
  struct bfd_link_info *info;
  struct ecoff_debug_info *debug;
  const struct ecoff_debug_swap *swap;
  boolean failed;
};

static boolean
elf64_alpha_output_extsym (h, data)
     struct alpha_elf_link_hash_entry *h;
     PTR data;
{
  struct extsym_info *einfo = (struct extsym_info *) data;
  boolean strip;
  asection *sec, *output_section;

  if (h->root.indx == -2)
    strip = false;
  else if (((h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
           || (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0)
          && (h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
          && (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    strip = true;
  else if (einfo->info->strip == strip_all
          || (einfo->info->strip == strip_some
              && bfd_hash_lookup (einfo->info->keep_hash,
                                  h->root.root.root.string,
                                  false, false) == NULL))
    strip = true;
  else
    strip = false;

  if (strip)
    return true;

  if (h->esym.ifd == -2)
    {
      h->esym.jmptbl = 0;
      h->esym.cobol_main = 0;
      h->esym.weakext = 0;
      h->esym.reserved = 0;
      h->esym.ifd = ifdNil;
      h->esym.asym.value = 0;
      h->esym.asym.st = stGlobal;

      if (h->root.root.type != bfd_link_hash_defined
         && h->root.root.type != bfd_link_hash_defweak)
       h->esym.asym.sc = scAbs;
      else
       {
         const char *name;

         sec = h->root.root.u.def.section;
         output_section = sec->output_section;

         /* When making a shared library and symbol h is the one from
            the another shared library, OUTPUT_SECTION may be null.  */
         if (output_section == NULL)
           h->esym.asym.sc = scUndefined;
         else
           {
             name = bfd_section_name (output_section->owner, output_section);

             if (strcmp (name, ".text") == 0)
               h->esym.asym.sc = scText;
             else if (strcmp (name, ".data") == 0)
               h->esym.asym.sc = scData;
             else if (strcmp (name, ".sdata") == 0)
               h->esym.asym.sc = scSData;
             else if (strcmp (name, ".rodata") == 0
                      || strcmp (name, ".rdata") == 0)
               h->esym.asym.sc = scRData;
             else if (strcmp (name, ".bss") == 0)
               h->esym.asym.sc = scBss;
             else if (strcmp (name, ".sbss") == 0)
               h->esym.asym.sc = scSBss;
             else if (strcmp (name, ".init") == 0)
               h->esym.asym.sc = scInit;
             else if (strcmp (name, ".fini") == 0)
               h->esym.asym.sc = scFini;
             else
               h->esym.asym.sc = scAbs;
           }
       }

      h->esym.asym.reserved = 0;
      h->esym.asym.index = indexNil;
    }

  if (h->root.root.type == bfd_link_hash_common)
    h->esym.asym.value = h->root.root.u.c.size;
  else if (h->root.root.type == bfd_link_hash_defined
	   || h->root.root.type == bfd_link_hash_defweak)
    {
      if (h->esym.asym.sc == scCommon)
       h->esym.asym.sc = scBss;
      else if (h->esym.asym.sc == scSCommon)
       h->esym.asym.sc = scSBss;

      sec = h->root.root.u.def.section;
      output_section = sec->output_section;
      if (output_section != NULL)
       h->esym.asym.value = (h->root.root.u.def.value
                             + sec->output_offset
                             + output_section->vma);
      else
       h->esym.asym.value = 0;
    }
  else if ((h->root.elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      /* Set type and value for a symbol with a function stub.  */
      h->esym.asym.st = stProc;
      sec = bfd_get_section_by_name (einfo->abfd, ".plt");
      if (sec == NULL)
	h->esym.asym.value = 0;
      else
	{
	  output_section = sec->output_section;
	  if (output_section != NULL)
	    h->esym.asym.value = (h->root.plt.offset
				  + sec->output_offset
				  + output_section->vma);
	  else
	    h->esym.asym.value = 0;
	}
#if 0 /* FIXME?  */
      h->esym.ifd = 0;
#endif
    }

  if (! bfd_ecoff_debug_one_external (einfo->abfd, einfo->debug, einfo->swap,
                                     h->root.root.root.string,
                                     &h->esym))
    {
      einfo->failed = true;
      return false;
    }

  return true;
}

/* FIXME:  Create a runtime procedure table from the .mdebug section.

static boolean
mips_elf_create_procedure_table (handle, abfd, info, s, debug)
     PTR handle;
     bfd *abfd;
     struct bfd_link_info *info;
     asection *s;
     struct ecoff_debug_info *debug;
*/

/* Handle dynamic relocations when doing an Alpha ELF link.  */

static boolean
elf64_alpha_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  bfd *dynobj;
  asection *sreloc;
  const char *rel_sec_name;
  Elf_Internal_Shdr *symtab_hdr;
  struct alpha_elf_link_hash_entry **sym_hashes;
  struct alpha_elf_got_entry **local_got_entries;
  const Elf_Internal_Rela *rel, *relend;
  int got_created;

  if (info->relocateable)
    return true;

  dynobj = elf_hash_table(info)->dynobj;
  if (dynobj == NULL)
    elf_hash_table(info)->dynobj = dynobj = abfd;

  sreloc = NULL;
  rel_sec_name = NULL;
  symtab_hdr = &elf_tdata(abfd)->symtab_hdr;
  sym_hashes = alpha_elf_sym_hashes(abfd);
  local_got_entries = alpha_elf_tdata(abfd)->local_got_entries;
  got_created = 0;

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; ++rel)
    {
      unsigned long r_symndx, r_type;
      struct alpha_elf_link_hash_entry *h;

      r_symndx = ELF64_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];

	  while (h->root.root.type == bfd_link_hash_indirect
		 || h->root.root.type == bfd_link_hash_warning)
	    h = (struct alpha_elf_link_hash_entry *)h->root.root.u.i.link;

	  h->root.elf_link_hash_flags |= ELF_LINK_HASH_REF_REGULAR;
	}
      r_type = ELF64_R_TYPE (rel->r_info);

      switch (r_type)
	{
	case R_ALPHA_LITERAL:
	  {
	    struct alpha_elf_got_entry *gotent;
	    int flags = 0;

	    if (h)
	      {
		/* Search for and possibly create a got entry.  */
		for (gotent = h->got_entries; gotent ; gotent = gotent->next)
		  if (gotent->gotobj == abfd &&
		      gotent->addend == rel->r_addend)
		    break;

		if (!gotent)
		  {
		    gotent = ((struct alpha_elf_got_entry *)
			      bfd_alloc (abfd,
					 sizeof (struct alpha_elf_got_entry)));
		    if (!gotent)
		      return false;

		    gotent->gotobj = abfd;
		    gotent->addend = rel->r_addend;
		    gotent->got_offset = -1;
		    gotent->flags = 0;
		    gotent->use_count = 1;

		    gotent->next = h->got_entries;
		    h->got_entries = gotent;

		    alpha_elf_tdata (abfd)->total_got_entries++;
		  }
		else
		  gotent->use_count += 1;
	      }
	    else
	      {
		/* This is a local .got entry -- record for merge.  */
		if (!local_got_entries)
		  {
		    size_t size;
		    size = (symtab_hdr->sh_info
			    * sizeof (struct alpha_elf_got_entry *));

		    local_got_entries = ((struct alpha_elf_got_entry **)
					 bfd_alloc (abfd, size));
		    if (!local_got_entries)
		      return false;

		    memset (local_got_entries, 0, size);
		    alpha_elf_tdata (abfd)->local_got_entries =
		      local_got_entries;
		  }

		for (gotent = local_got_entries[ELF64_R_SYM(rel->r_info)];
		     gotent != NULL && gotent->addend != rel->r_addend;
		     gotent = gotent->next)
		  continue;
		if (!gotent)
		  {
		    gotent = ((struct alpha_elf_got_entry *)
			      bfd_alloc (abfd,
					 sizeof (struct alpha_elf_got_entry)));
		    if (!gotent)
		      return false;

		    gotent->gotobj = abfd;
		    gotent->addend = rel->r_addend;
		    gotent->got_offset = -1;
		    gotent->flags = 0;
		    gotent->use_count = 1;

		    gotent->next = local_got_entries[ELF64_R_SYM(rel->r_info)];
		    local_got_entries[ELF64_R_SYM(rel->r_info)] = gotent;

		    alpha_elf_tdata(abfd)->total_got_entries++;
		    alpha_elf_tdata(abfd)->n_local_got_entries++;
		  }
		else
		  gotent->use_count += 1;
	      }

	    /* Remember how this literal is used from its LITUSEs.
	       This will be important when it comes to decide if we can
	       create a .plt entry for a function symbol.  */
	    if (rel+1 < relend
		&& ELF64_R_TYPE (rel[1].r_info) == R_ALPHA_LITUSE)
	      {
		do
		  {
		    ++rel;
		    if (rel->r_addend >= 1 && rel->r_addend <= 3)
		      flags |= 1 << rel->r_addend;
		  }
		while (rel+1 < relend &&
		       ELF64_R_TYPE (rel[1].r_info) == R_ALPHA_LITUSE);
	      }
	    else
	      {
		/* No LITUSEs -- presumably the address is not being
		   loaded for nothing.  */
		flags = ALPHA_ELF_LINK_HASH_LU_ADDR;
	      }

	    gotent->flags |= flags;
	    if (h)
	      {
		/* Make a guess as to whether a .plt entry will be needed.  */
		if ((h->flags |= flags) == ALPHA_ELF_LINK_HASH_LU_FUNC)
		  h->root.elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
		else
		  h->root.elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	      }
	  }
	  /* FALLTHRU */

	case R_ALPHA_GPDISP:
	case R_ALPHA_GPREL32:
	case R_ALPHA_GPRELHIGH:
	case R_ALPHA_GPRELLOW:
	  /* We don't actually use the .got here, but the sections must
	     be created before the linker maps input sections to output
	     sections.  */
	  if (!got_created)
	    {
	      if (!elf64_alpha_create_got_section (abfd, info))
		return false;

	      /* Make sure the object's gotobj is set to itself so
		 that we default to every object with its own .got.
		 We'll merge .gots later once we've collected each
		 object's info.  */
	      alpha_elf_tdata(abfd)->gotobj = abfd;

	      got_created = 1;
	    }
	  break;

	case R_ALPHA_SREL16:
	case R_ALPHA_SREL32:
	case R_ALPHA_SREL64:
	  if (h == NULL)
	    break;
	  /* FALLTHRU */

	case R_ALPHA_REFLONG:
	case R_ALPHA_REFQUAD:
	  if (rel_sec_name == NULL)
	    {
	      rel_sec_name = (bfd_elf_string_from_elf_section
			      (abfd, elf_elfheader(abfd)->e_shstrndx,
			       elf_section_data(sec)->rel_hdr.sh_name));
	      if (rel_sec_name == NULL)
		return false;

	      BFD_ASSERT (strncmp (rel_sec_name, ".rela", 5) == 0
			  && strcmp (bfd_get_section_name (abfd, sec),
				     rel_sec_name+5) == 0);
	    }

	  /* We need to create the section here now whether we eventually
	     use it or not so that it gets mapped to an output section by
	     the linker.  If not used, we'll kill it in
	     size_dynamic_sections.  */
	  if (sreloc == NULL)
	    {
	      sreloc = bfd_get_section_by_name (dynobj, rel_sec_name);
	      if (sreloc == NULL)
		{
		  sreloc = bfd_make_section (dynobj, rel_sec_name);
		  if (sreloc == NULL
		      || !bfd_set_section_flags (dynobj, sreloc,
						 ((sec->flags & (SEC_ALLOC
								 | SEC_LOAD))
						  | SEC_HAS_CONTENTS
						  | SEC_IN_MEMORY
						  | SEC_LINKER_CREATED
						  | SEC_READONLY))
		      || !bfd_set_section_alignment (dynobj, sreloc, 3))
		    return false;
		}
	    }

	  if (h)
	    {
	      /* Since we havn't seen all of the input symbols yet, we
		 don't know whether we'll actually need a dynamic relocation
		 entry for this reloc.  So make a record of it.  Once we
		 find out if this thing needs dynamic relocation we'll
		 expand the relocation sections by the appropriate amount.  */

	      struct alpha_elf_reloc_entry *rent;

	      for (rent = h->reloc_entries; rent; rent = rent->next)
		if (rent->rtype == r_type && rent->srel == sreloc)
		  break;

	      if (!rent)
		{
		  rent = ((struct alpha_elf_reloc_entry *)
			  bfd_alloc (abfd,
				     sizeof (struct alpha_elf_reloc_entry)));
		  if (!rent)
		    return false;

		  rent->srel = sreloc;
		  rent->rtype = r_type;
		  rent->count = 1;

		  rent->next = h->reloc_entries;
		  h->reloc_entries = rent;
		}
	      else
		rent->count++;
	    }
	  else if (info->shared && (sec->flags & SEC_ALLOC))
	    {
	      /* If this is a shared library, and the section is to be
		 loaded into memory, we need a RELATIVE reloc.  */
	      sreloc->_raw_size += sizeof (Elf64_External_Rela);
	    }
	  break;
	}
    }

  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static boolean
elf64_alpha_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  asection *s;
  struct alpha_elf_link_hash_entry *ah;

  dynobj = elf_hash_table(info)->dynobj;
  ah = (struct alpha_elf_link_hash_entry *)h;

  /* Now that we've seen all of the input symbols, finalize our decision
     about whether this symbol should get a .plt entry.  */

  if (h->root.type != bfd_link_hash_undefweak
      && alpha_elf_dynamic_symbol_p (h, info)
      && ((h->type == STT_FUNC
	   && !(ah->flags & ALPHA_ELF_LINK_HASH_LU_ADDR))
	  || (h->type == STT_NOTYPE
	      && ah->flags == ALPHA_ELF_LINK_HASH_LU_FUNC))
      /* Don't prevent otherwise valid programs from linking by attempting
	 to create a new .got entry somewhere.  A Correct Solution would be
	 to add a new .got section to a new object file and let it be merged
	 somewhere later.  But for now don't bother.  */
      && ah->got_entries)
    {
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;

      s = bfd_get_section_by_name(dynobj, ".plt");
      if (!s && !elf64_alpha_create_dynamic_sections (dynobj, info))
	return false;

      /* The first bit of the .plt is reserved.  */
      if (s->_raw_size == 0)
	s->_raw_size = PLT_HEADER_SIZE;

      h->plt.offset = s->_raw_size;
      s->_raw_size += PLT_ENTRY_SIZE;

      /* If this symbol is not defined in a regular file, and we are not
	 generating a shared library, then set the symbol to the location
	 in the .plt.  This is required to make function pointers compare
	 equal between the normal executable and the shared library.  */
      if (! info->shared
	  && h->root.type != bfd_link_hash_defweak)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = h->plt.offset;
	}

      /* We also need a JMP_SLOT entry in the .rela.plt section.  */
      s = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (s != NULL);
      s->_raw_size += sizeof (Elf64_External_Rela);

      return true;
    }
  else
    h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;

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
     is not a function.  The Alpha, since it uses .got entries for all
     symbols even in regular objects, does not need the hackery of a
     .dynbss section and COPY dynamic relocations.  */

  return true;
}

/* Symbol versioning can create new symbols, and make our old symbols
   indirect to the new ones.  Consolidate the got and reloc information
   in these situations.  */

static boolean
elf64_alpha_merge_ind_symbols (hi, dummy)
     struct alpha_elf_link_hash_entry *hi;
     PTR dummy ATTRIBUTE_UNUSED;
{
  struct alpha_elf_link_hash_entry *hs;

  if (hi->root.root.type != bfd_link_hash_indirect)
    return true;
  hs = hi;
  do {
    hs = (struct alpha_elf_link_hash_entry *)hs->root.root.u.i.link;
  } while (hs->root.root.type == bfd_link_hash_indirect);

  /* Merge the flags.  Whee.  */

  hs->flags |= hi->flags;

  /* Merge the .got entries.  Cannibalize the old symbol's list in
     doing so, since we don't need it anymore.  */

  if (hs->got_entries == NULL)
    hs->got_entries = hi->got_entries;
  else
    {
      struct alpha_elf_got_entry *gi, *gs, *gin, *gsh;

      gsh = hs->got_entries;
      for (gi = hi->got_entries; gi ; gi = gin)
	{
	  gin = gi->next;
	  for (gs = gsh; gs ; gs = gs->next)
	    if (gi->gotobj == gs->gotobj && gi->addend == gs->addend)
	      goto got_found;
	  gi->next = hs->got_entries;
	  hs->got_entries = gi;
	got_found:;
	}
    }
  hi->got_entries = NULL;

  /* And similar for the reloc entries.  */

  if (hs->reloc_entries == NULL)
    hs->reloc_entries = hi->reloc_entries;
  else
    {
      struct alpha_elf_reloc_entry *ri, *rs, *rin, *rsh;

      rsh = hs->reloc_entries;
      for (ri = hi->reloc_entries; ri ; ri = rin)
	{
	  rin = ri->next;
	  for (rs = rsh; rs ; rs = rs->next)
	    if (ri->rtype == rs->rtype)
	      {
		rs->count += ri->count;
		goto found_reloc;
	      }
	  ri->next = hs->reloc_entries;
	  hs->reloc_entries = ri;
	found_reloc:;
	}
    }
  hi->reloc_entries = NULL;

  return true;
}

/* Is it possible to merge two object file's .got tables?  */

static boolean
elf64_alpha_can_merge_gots (a, b)
     bfd *a, *b;
{
  int total = alpha_elf_tdata (a)->total_got_entries;
  bfd *bsub;

  /* Trivial quick fallout test.  */
  if (total + alpha_elf_tdata (b)->total_got_entries <= MAX_GOT_ENTRIES)
    return true;

  /* By their nature, local .got entries cannot be merged.  */
  if ((total += alpha_elf_tdata (b)->n_local_got_entries) > MAX_GOT_ENTRIES)
    return false;

  /* Failing the common trivial comparison, we must effectively
     perform the merge.  Not actually performing the merge means that
     we don't have to store undo information in case we fail.  */
  for (bsub = b; bsub ; bsub = alpha_elf_tdata (bsub)->in_got_link_next)
    {
      struct alpha_elf_link_hash_entry **hashes = alpha_elf_sym_hashes (bsub);
      Elf_Internal_Shdr *symtab_hdr = &elf_tdata (bsub)->symtab_hdr;
      int i, n;

      n = NUM_SHDR_ENTRIES (symtab_hdr) - symtab_hdr->sh_info;
      for (i = 0; i < n; ++i)
	{
	  struct alpha_elf_got_entry *ae, *be;
	  struct alpha_elf_link_hash_entry *h;

	  h = hashes[i];
	  while (h->root.root.type == bfd_link_hash_indirect
	         || h->root.root.type == bfd_link_hash_warning)
	    h = (struct alpha_elf_link_hash_entry *)h->root.root.u.i.link;

	  for (be = h->got_entries; be ; be = be->next)
	    {
	      if (be->use_count == 0)
	        continue;
	      if (be->gotobj != b)
	        continue;

	      for (ae = h->got_entries; ae ; ae = ae->next)
	        if (ae->gotobj == a && ae->addend == be->addend)
		  goto global_found;

	      if (++total > MAX_GOT_ENTRIES)
	        return false;
	    global_found:;
	    }
	}
    }

  return true;
}

/* Actually merge two .got tables.  */

static void
elf64_alpha_merge_gots (a, b)
     bfd *a, *b;
{
  int total = alpha_elf_tdata (a)->total_got_entries;
  bfd *bsub;

  /* Remember local expansion.  */
  {
    int e = alpha_elf_tdata (b)->n_local_got_entries;
    total += e;
    alpha_elf_tdata (a)->n_local_got_entries += e;
  }

  for (bsub = b; bsub ; bsub = alpha_elf_tdata (bsub)->in_got_link_next)
    {
      struct alpha_elf_got_entry **local_got_entries;
      struct alpha_elf_link_hash_entry **hashes;
      Elf_Internal_Shdr *symtab_hdr;
      int i, n;

      /* Let the local .got entries know they are part of a new subsegment.  */
      local_got_entries = alpha_elf_tdata (bsub)->local_got_entries;
      if (local_got_entries)
        {
	  n = elf_tdata (bsub)->symtab_hdr.sh_info;
	  for (i = 0; i < n; ++i)
	    {
	      struct alpha_elf_got_entry *ent;
	      for (ent = local_got_entries[i]; ent; ent = ent->next)
	        ent->gotobj = a;
	    }
        }

      /* Merge the global .got entries.  */
      hashes = alpha_elf_sym_hashes (bsub);
      symtab_hdr = &elf_tdata (bsub)->symtab_hdr;

      n = NUM_SHDR_ENTRIES (symtab_hdr) - symtab_hdr->sh_info;
      for (i = 0; i < n; ++i)
        {
	  struct alpha_elf_got_entry *ae, *be, **pbe, **start;
	  struct alpha_elf_link_hash_entry *h;

	  h = hashes[i];
	  while (h->root.root.type == bfd_link_hash_indirect
	         || h->root.root.type == bfd_link_hash_warning)
	    h = (struct alpha_elf_link_hash_entry *)h->root.root.u.i.link;

	  start = &h->got_entries;
	  for (pbe = start, be = *start; be ; pbe = &be->next, be = be->next)
	    {
	      if (be->use_count == 0)
	        {
		  *pbe = be->next;
		  continue;
	        }
	      if (be->gotobj != b)
	        continue;

	      for (ae = *start; ae ; ae = ae->next)
	        if (ae->gotobj == a && ae->addend == be->addend)
		  {
		    ae->flags |= be->flags;
		    ae->use_count += be->use_count;
		    *pbe = be->next;
		    goto global_found;
		  }
	      be->gotobj = a;
	      total += 1;

	    global_found:;
	    }
        }

      alpha_elf_tdata (bsub)->gotobj = a;
    }
  alpha_elf_tdata (a)->total_got_entries = total;

  /* Merge the two in_got chains.  */
  {
    bfd *next;

    bsub = a;
    while ((next = alpha_elf_tdata (bsub)->in_got_link_next) != NULL)
      bsub = next;

    alpha_elf_tdata (bsub)->in_got_link_next = b;
  }
}

/* Calculate the offsets for the got entries.  */

static boolean
elf64_alpha_calc_got_offsets_for_symbol (h, arg)
     struct alpha_elf_link_hash_entry *h;
     PTR arg;
{
  struct alpha_elf_got_entry *gotent;

  for (gotent = h->got_entries; gotent; gotent = gotent->next)
    if (gotent->use_count > 0)
      {
	bfd_size_type *plge
	  = &alpha_elf_tdata (gotent->gotobj)->got->_raw_size;

	gotent->got_offset = *plge;
	*plge += 8;
      }

  return true;
}

static void
elf64_alpha_calc_got_offsets (info)
     struct bfd_link_info *info;
{
  bfd *i, *got_list = alpha_elf_hash_table(info)->got_list;

  /* First, zero out the .got sizes, as we may be recalculating the
     .got after optimizing it.  */
  for (i = got_list; i ; i = alpha_elf_tdata(i)->got_link_next)
    alpha_elf_tdata(i)->got->_raw_size = 0;

  /* Next, fill in the offsets for all the global entries.  */
  alpha_elf_link_hash_traverse (alpha_elf_hash_table (info),
				elf64_alpha_calc_got_offsets_for_symbol,
				NULL);

  /* Finally, fill in the offsets for the local entries.  */
  for (i = got_list; i ; i = alpha_elf_tdata(i)->got_link_next)
    {
      bfd_size_type got_offset = alpha_elf_tdata(i)->got->_raw_size;
      bfd *j;

      for (j = i; j ; j = alpha_elf_tdata(j)->in_got_link_next)
	{
	  struct alpha_elf_got_entry **local_got_entries, *gotent;
	  int k, n;

	  local_got_entries = alpha_elf_tdata(j)->local_got_entries;
	  if (!local_got_entries)
	    continue;

	  for (k = 0, n = elf_tdata(j)->symtab_hdr.sh_info; k < n; ++k)
	    for (gotent = local_got_entries[k]; gotent; gotent = gotent->next)
	      if (gotent->use_count > 0)
	        {
		  gotent->got_offset = got_offset;
		  got_offset += 8;
	        }
	}

      alpha_elf_tdata(i)->got->_raw_size = got_offset;
      alpha_elf_tdata(i)->got->_cooked_size = got_offset;
    }
}

/* Constructs the gots.  */

static boolean
elf64_alpha_size_got_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *i, *got_list, *cur_got_obj;
  int something_changed = 0;

  got_list = alpha_elf_hash_table (info)->got_list;

  /* On the first time through, pretend we have an existing got list
     consisting of all of the input files.  */
  if (got_list == NULL)
    {
      for (i = info->input_bfds; i ; i = i->link_next)
	{
	  bfd *this_got = alpha_elf_tdata (i)->gotobj;
	  if (this_got == NULL)
	    continue;

	  /* We are assuming no merging has yet ocurred.  */
	  BFD_ASSERT (this_got == i);

          if (alpha_elf_tdata (this_got)->total_got_entries > MAX_GOT_ENTRIES)
	    {
	      /* Yikes! A single object file has too many entries.  */
	      (*_bfd_error_handler)
	        (_("%s: .got subsegment exceeds 64K (size %d)"),
	         bfd_get_filename (i),
	         alpha_elf_tdata (this_got)->total_got_entries * 8);
	      return false;
	    }

	  if (got_list == NULL)
	    got_list = this_got;
	  else
	    alpha_elf_tdata(cur_got_obj)->got_link_next = this_got;
	  cur_got_obj = this_got;
	}

      /* Strange degenerate case of no got references.  */
      if (got_list == NULL)
	return true;

      alpha_elf_hash_table (info)->got_list = got_list;

      /* Force got offsets to be recalculated.  */
      something_changed = 1;
    }

  cur_got_obj = got_list;
  i = alpha_elf_tdata(cur_got_obj)->got_link_next;
  while (i != NULL)
    {
      if (elf64_alpha_can_merge_gots (cur_got_obj, i))
	{
	  elf64_alpha_merge_gots (cur_got_obj, i);
	  i = alpha_elf_tdata(i)->got_link_next;
	  alpha_elf_tdata(cur_got_obj)->got_link_next = i;
	  something_changed = 1;
	}
      else
	{
	  cur_got_obj = i;
	  i = alpha_elf_tdata(i)->got_link_next;
	}
    }

  /* Once the gots have been merged, fill in the got offsets for
     everything therein.  */
  if (1 || something_changed)
    elf64_alpha_calc_got_offsets (info);

  return true;
}

static boolean
elf64_alpha_always_size_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *i;

  if (info->relocateable)
    return true;

  /* First, take care of the indirect symbols created by versioning.  */
  alpha_elf_link_hash_traverse (alpha_elf_hash_table (info),
				elf64_alpha_merge_ind_symbols,
				NULL);

  if (!elf64_alpha_size_got_sections (output_bfd, info))
    return false;

  /* Allocate space for all of the .got subsections.  */
  i = alpha_elf_hash_table (info)->got_list;
  for ( ; i ; i = alpha_elf_tdata(i)->got_link_next)
    {
      asection *s = alpha_elf_tdata(i)->got;
      if (s->_raw_size > 0)
	{
	  s->contents = (bfd_byte *) bfd_zalloc (i, s->_raw_size);
	  if (s->contents == NULL)
	    return false;
	}
    }

  return true;
}

/* Work out the sizes of the dynamic relocation entries.  */

static boolean
elf64_alpha_calc_dynrel_sizes (h, info)
     struct alpha_elf_link_hash_entry *h;
     struct bfd_link_info *info;
{
  /* If the symbol was defined as a common symbol in a regular object
     file, and there was no definition in any dynamic object, then the
     linker will have allocated space for the symbol in a common
     section but the ELF_LINK_HASH_DEF_REGULAR flag will not have been
     set.  This is done for dynamic symbols in
     elf_adjust_dynamic_symbol but this is not done for non-dynamic
     symbols, somehow.  */
  if (((h->root.elf_link_hash_flags
       & (ELF_LINK_HASH_DEF_REGULAR
	  | ELF_LINK_HASH_REF_REGULAR
	  | ELF_LINK_HASH_DEF_DYNAMIC))
       == ELF_LINK_HASH_REF_REGULAR)
      && (h->root.root.type == bfd_link_hash_defined
	  || h->root.root.type == bfd_link_hash_defweak)
      && !(h->root.root.u.def.section->owner->flags & DYNAMIC))
    {
      h->root.elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
    }

  /* If the symbol is dynamic, we'll need all the relocations in their
     natural form.  If this is a shared object, and it has been forced
     local, we'll need the same number of RELATIVE relocations.  */

  if (alpha_elf_dynamic_symbol_p (&h->root, info) || info->shared)
    {
      struct alpha_elf_reloc_entry *relent;
      bfd *dynobj;
      struct alpha_elf_got_entry *gotent;
      bfd_size_type count;
      asection *srel;

      for (relent = h->reloc_entries; relent; relent = relent->next)
	if (relent->rtype == R_ALPHA_REFLONG
	    || relent->rtype == R_ALPHA_REFQUAD)
	  {
	    relent->srel->_raw_size +=
	      sizeof (Elf64_External_Rela) * relent->count;
	  }

      dynobj = elf_hash_table(info)->dynobj;
      count = 0;

      for (gotent = h->got_entries; gotent ; gotent = gotent->next)
	count++;

      /* If we are using a .plt entry, subtract one, as the first
	 reference uses a .rela.plt entry instead.  */
      if (h->root.plt.offset != MINUS_ONE)
	count--;

      if (count > 0)
	{
	  srel = bfd_get_section_by_name (dynobj, ".rela.got");
	  BFD_ASSERT (srel != NULL);
	  srel->_raw_size += sizeof (Elf64_External_Rela) * count;
	}
    }

  return true;
}

/* Set the sizes of the dynamic sections.  */

static boolean
elf64_alpha_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean reltext;
  boolean relplt;

  dynobj = elf_hash_table(info)->dynobj;
  BFD_ASSERT(dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (!info->shared)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}

      /* Now that we've seen all of the input files, we can decide which
	 symbols need dynamic relocation entries and which don't.  We've
	 collected information in check_relocs that we can now apply to
	 size the dynamic relocation sections.  */
      alpha_elf_link_hash_traverse (alpha_elf_hash_table (info),
				    elf64_alpha_calc_dynrel_sizes,
				    info);

      /* When building shared libraries, each local .got entry needs a
	 RELATIVE reloc.  */
      if (info->shared)
	{
	  bfd *i;
	  asection *srel;
	  bfd_size_type count;

	  srel = bfd_get_section_by_name (dynobj, ".rela.got");
	  BFD_ASSERT (srel != NULL);

	  for (i = alpha_elf_hash_table(info)->got_list, count = 0;
	       i != NULL;
	       i = alpha_elf_tdata(i)->got_link_next)
	    count += alpha_elf_tdata(i)->n_local_got_entries;

	  srel->_raw_size += count * sizeof (Elf64_External_Rela);
	}
    }
  /* else we're not dynamic and by definition we don't need such things.  */

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  reltext = false;
  relplt = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      boolean strip;

      if (!(s->flags & SEC_LINKER_CREATED))
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      /* If we don't need this section, strip it from the output file.
	 This is to handle .rela.bss and .rela.plt.  We must create it
	 in create_dynamic_sections, because it must be created before
	 the linker maps input sections to output sections.  The
	 linker does that before adjust_dynamic_symbol is called, and
	 it is that function which decides whether anything needs to
	 go into these sections.  */

      strip = false;

      if (strncmp (name, ".rela", 5) == 0)
	{
	  strip = (s->_raw_size == 0);

	  if (!strip)
	    {
	      const char *outname;
	      asection *target;

	      /* If this relocation section applies to a read only
		 section, then we probably need a DT_TEXTREL entry.  */
	      outname = bfd_get_section_name (output_bfd,
					      s->output_section);
	      target = bfd_get_section_by_name (output_bfd, outname + 5);
	      if (target != NULL
		  && (target->flags & SEC_READONLY) != 0
		  && (target->flags & SEC_ALLOC) != 0)
		reltext = true;

	      if (strcmp(name, ".rela.plt") == 0)
		relplt = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strcmp (name, ".plt") != 0)
	{
	  /* It's not one of our dynamic sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	_bfd_strip_section_from_output (info, s);
      else
	{
	  /* Allocate memory for the section contents.  */
	  s->contents = (bfd_byte *) bfd_zalloc(dynobj, s->_raw_size);
	  if (s->contents == NULL && s->_raw_size != 0)
	    return false;
	}
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf64_alpha_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (!info->shared)
	{
	  if (!bfd_elf64_add_dynamic_entry (info, DT_DEBUG, 0))
	    return false;
	}

      if (! bfd_elf64_add_dynamic_entry (info, DT_PLTGOT, 0))
	return false;

      if (relplt)
	{
	  if (! bfd_elf64_add_dynamic_entry (info, DT_PLTRELSZ, 0)
	      || ! bfd_elf64_add_dynamic_entry (info, DT_PLTREL, DT_RELA)
	      || ! bfd_elf64_add_dynamic_entry (info, DT_JMPREL, 0))
	    return false;
	}

      if (! bfd_elf64_add_dynamic_entry (info, DT_RELA, 0)
	  || ! bfd_elf64_add_dynamic_entry (info, DT_RELASZ, 0)
	  || ! bfd_elf64_add_dynamic_entry (info, DT_RELAENT,
					    sizeof (Elf64_External_Rela)))
	return false;

      if (reltext)
	{
	  if (! bfd_elf64_add_dynamic_entry (info, DT_TEXTREL, 0))
	    return false;
	  info->flags |= DF_TEXTREL;
	}
    }

  return true;
}

/* Relocate an Alpha ELF section.  */

static boolean
elf64_alpha_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;
  asection *sec, *sgot, *srel, *srelgot;
  bfd *dynobj, *gotobj;
  bfd_vma gp;

  srelgot = srel = NULL;
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  dynobj = elf_hash_table (info)->dynobj;
  if (dynobj)
    {
      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
    }

  /* Find the gp value for this input bfd.  */
  sgot = NULL;
  gp = 0;
  gotobj = alpha_elf_tdata (input_bfd)->gotobj;
  if (gotobj)
    {
      sgot = alpha_elf_tdata (gotobj)->got;
      gp = _bfd_get_gp_value (gotobj);
      if (gp == 0)
	{
	  gp = (sgot->output_section->vma
		+ sgot->output_offset
		+ 0x8000);
	  _bfd_set_gp_value (gotobj, gp);
	}
    }

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct alpha_elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      bfd_vma relocation;
      bfd_vma addend;
      bfd_reloc_status_type r;

      r_type = ELF64_R_TYPE(rel->r_info);
      if (r_type < 0 || r_type >= (int) R_ALPHA_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = elf64_alpha_howto_table + r_type;

      r_symndx = ELF64_R_SYM(rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */

	  /* The symbol associated with GPDISP and LITUSE is
	     immaterial.  Only the addend is significant.  */
	  if (r_type == R_ALPHA_GPDISP || r_type == R_ALPHA_LITUSE)
	    continue;

	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE(sym->st_info) == STT_SECTION)
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
	  h = alpha_elf_sym_hashes (input_bfd)[r_symndx - symtab_hdr->sh_info];

	  while (h->root.root.type == bfd_link_hash_indirect
		 || h->root.root.type == bfd_link_hash_warning)
	    h = (struct alpha_elf_link_hash_entry *)h->root.root.u.i.link;

	  if (h->root.root.type == bfd_link_hash_defined
	      || h->root.root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.root.u.def.section;

#if rth_notdef
	      if ((r_type == R_ALPHA_LITERAL
		   && elf_hash_table(info)->dynamic_sections_created
		   && (!info->shared
		       || !info->symbolic
		       || !(h->root.elf_link_hash_flags
			    & ELF_LINK_HASH_DEF_REGULAR)))
		  || (info->shared
		      && (!info->symbolic
			  || !(h->root.elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR))
		      && (input_section->flags & SEC_ALLOC)
		      && (r_type == R_ALPHA_REFLONG
			  || r_type == R_ALPHA_REFQUAD
			  || r_type == R_ALPHA_LITERAL)))
		{
		  /* In these cases, we don't need the relocation value.
		     We check specially because in some obscure cases
		     sec->output_section will be NULL.  */
		  relocation = 0;
		}
#else
	      /* FIXME: Are not these obscure cases simply bugs?  Let's
		 get something working and come back to this.  */
	      if (sec->output_section == NULL)
		relocation = 0;
#endif /* rth_notdef */
	      else
		{
		  relocation = (h->root.root.u.def.value
				+ sec->output_section->vma
				+ sec->output_offset);
		}
	    }
	  else if (h->root.root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else if (info->shared && !info->symbolic
		   && !info->no_undefined
		   && ELF_ST_VISIBILITY (h->root.other) == STV_DEFAULT)
	    relocation = 0;
	  else
	    {
	      if (!((*info->callbacks->undefined_symbol)
		    (info, h->root.root.root.string, input_bfd,
		     input_section, rel->r_offset,
		     (!info->shared || info->no_undefined
		      || ELF_ST_VISIBILITY (h->root.other)))))
		return false;
	      relocation = 0;
	    }
	}
      addend = rel->r_addend;

      switch (r_type)
	{
	case R_ALPHA_GPDISP:
	  {
	    bfd_byte *p_ldah, *p_lda;

	    BFD_ASSERT(gp != 0);

	    relocation = (input_section->output_section->vma
			  + input_section->output_offset
			  + rel->r_offset);

	    p_ldah = contents + rel->r_offset - input_section->vma;
	    p_lda = p_ldah + rel->r_addend;

	    r = elf64_alpha_do_reloc_gpdisp (input_bfd, gp - relocation,
					     p_ldah, p_lda);
	  }
	  break;

	case R_ALPHA_OP_PUSH:
	case R_ALPHA_OP_STORE:
	case R_ALPHA_OP_PSUB:
	case R_ALPHA_OP_PRSHIFT:
	  /* We hate these silly beasts.  */
	  abort ();

	case R_ALPHA_LITERAL:
	  {
	    struct alpha_elf_got_entry *gotent;
	    boolean dynamic_symbol;

	    BFD_ASSERT(sgot != NULL);
	    BFD_ASSERT(gp != 0);

	    if (h != NULL)
	      {
		gotent = h->got_entries;
		dynamic_symbol = alpha_elf_dynamic_symbol_p (&h->root, info);
	      }
	    else
	      {
		gotent = (alpha_elf_tdata(input_bfd)->
			  local_got_entries[r_symndx]);
		dynamic_symbol = false;
	      }

	    BFD_ASSERT(gotent != NULL);

	    while (gotent->gotobj != gotobj || gotent->addend != addend)
	      gotent = gotent->next;

	    BFD_ASSERT(gotent->use_count >= 1);

	    /* Initialize the .got entry's value.  */
	    if (!(gotent->flags & ALPHA_ELF_GOT_ENTRY_RELOCS_DONE))
	      {
		bfd_put_64 (output_bfd, relocation+addend,
			    sgot->contents + gotent->got_offset);

		/* If the symbol has been forced local, output a
		   RELATIVE reloc, otherwise it will be handled in
		   finish_dynamic_symbol.  */
		if (info->shared && !dynamic_symbol)
		  {
		    Elf_Internal_Rela outrel;

		    BFD_ASSERT(srelgot != NULL);

		    outrel.r_offset = (sgot->output_section->vma
				       + sgot->output_offset
				       + gotent->got_offset);
		    outrel.r_info = ELF64_R_INFO(0, R_ALPHA_RELATIVE);
		    outrel.r_addend = 0;

		    bfd_elf64_swap_reloca_out (output_bfd, &outrel,
					       ((Elf64_External_Rela *)
					        srelgot->contents)
					       + srelgot->reloc_count++);
		    BFD_ASSERT (sizeof (Elf64_External_Rela)
				* srelgot->reloc_count
				<= srelgot->_cooked_size);
		  }

		gotent->flags |= ALPHA_ELF_GOT_ENTRY_RELOCS_DONE;
	      }

	    /* Figure the gprel relocation.  */
	    addend = 0;
	    relocation = (sgot->output_section->vma
			  + sgot->output_offset
			  + gotent->got_offset);
	    relocation -= gp;
	  }
	  /* overflow handled by _bfd_final_link_relocate */
	  goto default_reloc;

	case R_ALPHA_GPREL32:
	case R_ALPHA_GPRELLOW:
	  BFD_ASSERT(gp != 0);
	  relocation -= gp;
	  goto default_reloc;

	case R_ALPHA_GPRELHIGH:
	  BFD_ASSERT(gp != 0);
	  relocation -= gp;
	  relocation += addend;
	  addend = 0;
	  relocation = (((bfd_signed_vma) relocation >> 16)
			+ ((relocation >> 15) & 1));
	  goto default_reloc;

	case R_ALPHA_BRADDR:
	case R_ALPHA_HINT:
	  /* The regular PC-relative stuff measures from the start of
	     the instruction rather than the end.  */
	  addend -= 4;
	  goto default_reloc;

	case R_ALPHA_REFLONG:
	case R_ALPHA_REFQUAD:
	  {
	    Elf_Internal_Rela outrel;
	    boolean skip;

	    /* Careful here to remember RELATIVE relocations for global
	       variables for symbolic shared objects.  */

	    if (h && alpha_elf_dynamic_symbol_p (&h->root, info))
	      {
		BFD_ASSERT(h->root.dynindx != -1);
		outrel.r_info = ELF64_R_INFO(h->root.dynindx, r_type);
		outrel.r_addend = addend;
		addend = 0, relocation = 0;
	      }
	    else if (info->shared && (input_section->flags & SEC_ALLOC))
	      {
		outrel.r_info = ELF64_R_INFO(0, R_ALPHA_RELATIVE);
		outrel.r_addend = 0;
	      }
	    else
	      goto default_reloc;

	    if (!srel)
	      {
		const char *name;

		name = (bfd_elf_string_from_elf_section
			(input_bfd, elf_elfheader(input_bfd)->e_shstrndx,
			 elf_section_data(input_section)->rel_hdr.sh_name));
		BFD_ASSERT(name != NULL);

		srel = bfd_get_section_by_name (dynobj, name);
		BFD_ASSERT(srel != NULL);
	      }

	    skip = false;

	    if (elf_section_data (input_section)->stab_info == NULL)
	      outrel.r_offset = rel->r_offset;
	    else
	      {
		bfd_vma off;

		off = (_bfd_stab_section_offset
		       (output_bfd, &elf_hash_table (info)->stab_info,
			input_section,
			&elf_section_data (input_section)->stab_info,
			rel->r_offset));
		if (off == (bfd_vma) -1)
		  skip = true;
		outrel.r_offset = off;
	      }

	    if (! skip)
	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);
	    else
	      memset (&outrel, 0, sizeof outrel);

	    bfd_elf64_swap_reloca_out (output_bfd, &outrel,
				       ((Elf64_External_Rela *)
					srel->contents)
				       + srel->reloc_count++);
	    BFD_ASSERT (sizeof (Elf64_External_Rela) * srel->reloc_count
			<= srel->_cooked_size);
	  }
	  goto default_reloc;

	default:
	default_reloc:
	  r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents, rel->r_offset, relocation,
					addend);
	  break;
	}

      switch (r)
	{
	case bfd_reloc_ok:
	  break;

	case bfd_reloc_overflow:
	  {
	    const char *name;

	    if (h != NULL)
	      name = h->root.root.root.string;
	    else
	      {
		name = (bfd_elf_string_from_elf_section
			(input_bfd, symtab_hdr->sh_link, sym->st_name));
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

	default:
	case bfd_reloc_outofrange:
	  abort ();
	}
    }

  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
elf64_alpha_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj = elf_hash_table(info)->dynobj;

  if (h->plt.offset != MINUS_ONE)
    {
      /* Fill in the .plt entry for this symbol.  */
      asection *splt, *sgot, *srel;
      Elf_Internal_Rela outrel;
      bfd_vma got_addr, plt_addr;
      bfd_vma plt_index;
      struct alpha_elf_got_entry *gotent;

      BFD_ASSERT (h->dynindx != -1);

      /* The first .got entry will be updated by the .plt with the
	 address of the target function.  */
      gotent = ((struct alpha_elf_link_hash_entry *) h)->got_entries;
      BFD_ASSERT (gotent && gotent->addend == 0);

      splt = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (splt != NULL);
      srel = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (srel != NULL);
      sgot = alpha_elf_tdata (gotent->gotobj)->got;
      BFD_ASSERT (sgot != NULL);

      got_addr = (sgot->output_section->vma
		  + sgot->output_offset
		  + gotent->got_offset);
      plt_addr = (splt->output_section->vma
		  + splt->output_offset
		  + h->plt.offset);

      plt_index = (h->plt.offset - PLT_HEADER_SIZE) / PLT_ENTRY_SIZE;

      /* Fill in the entry in the procedure linkage table.  */
      {
	unsigned insn1, insn2, insn3;

	insn1 = PLT_ENTRY_WORD1 | ((-(h->plt.offset + 4) >> 2) & 0x1fffff);
	insn2 = PLT_ENTRY_WORD2;
	insn3 = PLT_ENTRY_WORD3;

	bfd_put_32 (output_bfd, insn1, splt->contents + h->plt.offset);
	bfd_put_32 (output_bfd, insn2, splt->contents + h->plt.offset + 4);
	bfd_put_32 (output_bfd, insn3, splt->contents + h->plt.offset + 8);
      }

      /* Fill in the entry in the .rela.plt section.  */
      outrel.r_offset = got_addr;
      outrel.r_info = ELF64_R_INFO(h->dynindx, R_ALPHA_JMP_SLOT);
      outrel.r_addend = 0;

      bfd_elf64_swap_reloca_out (output_bfd, &outrel,
				 ((Elf64_External_Rela *)srel->contents
				  + plt_index));

      if (!(h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	{
	  /* Mark the symbol as undefined, rather than as defined in the
	     .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	}

      /* Fill in the entries in the .got.  */
      bfd_put_64 (output_bfd, plt_addr, sgot->contents + gotent->got_offset);

      /* Subsequent .got entries will continue to bounce through the .plt.  */
      if (gotent->next)
	{
	  srel = bfd_get_section_by_name (dynobj, ".rela.got");
	  BFD_ASSERT (! info->shared || srel != NULL);

	  gotent = gotent->next;
	  do
	    {
	      sgot = alpha_elf_tdata(gotent->gotobj)->got;
	      BFD_ASSERT(sgot != NULL);
	      BFD_ASSERT(gotent->addend == 0);

	      bfd_put_64 (output_bfd, plt_addr,
		          sgot->contents + gotent->got_offset);

	      if (info->shared)
		{
		  outrel.r_offset = (sgot->output_section->vma
				     + sgot->output_offset
				     + gotent->got_offset);
		  outrel.r_info = ELF64_R_INFO(0, R_ALPHA_RELATIVE);
		  outrel.r_addend = 0;

		  bfd_elf64_swap_reloca_out (output_bfd, &outrel,
					     ((Elf64_External_Rela *)
					      srel->contents)
					     + srel->reloc_count++);
		  BFD_ASSERT (sizeof (Elf64_External_Rela) * srel->reloc_count
			      <= srel->_cooked_size);
		}

	      gotent = gotent->next;
	    }
          while (gotent != NULL);
	}
    }
  else if (alpha_elf_dynamic_symbol_p (h, info))
    {
      /* Fill in the dynamic relocations for this symbol's .got entries.  */
      asection *srel;
      Elf_Internal_Rela outrel;
      struct alpha_elf_got_entry *gotent;

      srel = bfd_get_section_by_name (dynobj, ".rela.got");
      BFD_ASSERT (srel != NULL);

      outrel.r_info = ELF64_R_INFO (h->dynindx, R_ALPHA_GLOB_DAT);
      for (gotent = ((struct alpha_elf_link_hash_entry *) h)->got_entries;
	   gotent != NULL;
	   gotent = gotent->next)
	{
	  asection *sgot = alpha_elf_tdata (gotent->gotobj)->got;
	  outrel.r_offset = (sgot->output_section->vma
			     + sgot->output_offset
			     + gotent->got_offset);
	  outrel.r_addend = gotent->addend;

	  bfd_elf64_swap_reloca_out (output_bfd, &outrel,
				     ((Elf64_External_Rela *)srel->contents
				      + srel->reloc_count++));
	  BFD_ASSERT (sizeof (Elf64_External_Rela) * srel->reloc_count
		      <= srel->_cooked_size);
	}
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
elf64_alpha_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;

  dynobj = elf_hash_table (info)->dynobj;
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      Elf64_External_Dyn *dyncon, *dynconend;

      splt = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (splt != NULL && sdyn != NULL);

      dyncon = (Elf64_External_Dyn *) sdyn->contents;
      dynconend = (Elf64_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  asection *s;

	  bfd_elf64_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:
	      name = ".plt";
	      goto get_vma;
	    case DT_PLTRELSZ:
	      name = ".rela.plt";
	      goto get_size;
	    case DT_JMPREL:
	      name = ".rela.plt";
	      goto get_vma;

	    case DT_RELASZ:
	      /* My interpretation of the TIS v1.1 ELF document indicates
		 that RELASZ should not include JMPREL.  This is not what
		 the rest of the BFD does.  It is, however, what the
		 glibc ld.so wants.  Do this fixup here until we found
		 out who is right.  */
	      s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	      if (s)
		{
		  dyn.d_un.d_val -=
		    (s->_cooked_size ? s->_cooked_size : s->_raw_size);
		}
	      break;

	    get_vma:
	      s = bfd_get_section_by_name (output_bfd, name);
	      dyn.d_un.d_ptr = (s ? s->vma : 0);
	      break;

	    get_size:
	      s = bfd_get_section_by_name (output_bfd, name);
	      dyn.d_un.d_val =
		(s->_cooked_size ? s->_cooked_size : s->_raw_size);
	      break;
	    }

	  bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	}

      /* Initialize the PLT0 entry */
      if (splt->_raw_size > 0)
	{
	  bfd_put_32 (output_bfd, PLT_HEADER_WORD1, splt->contents);
	  bfd_put_32 (output_bfd, PLT_HEADER_WORD2, splt->contents + 4);
	  bfd_put_32 (output_bfd, PLT_HEADER_WORD3, splt->contents + 8);
	  bfd_put_32 (output_bfd, PLT_HEADER_WORD4, splt->contents + 12);

	  /* The next two words will be filled in by ld.so */
	  bfd_put_64 (output_bfd, 0, splt->contents + 16);
	  bfd_put_64 (output_bfd, 0, splt->contents + 24);

	  elf_section_data (splt->output_section)->this_hdr.sh_entsize =
	    PLT_HEADER_SIZE;
	}
    }

  return true;
}

/* We need to use a special link routine to handle the .reginfo and
   the .mdebug sections.  We need to merge all instances of these
   sections together, not write them all out sequentially.  */

static boolean
elf64_alpha_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection *o;
  struct bfd_link_order *p;
  asection *reginfo_sec, *mdebug_sec, *gptab_data_sec, *gptab_bss_sec;
  struct ecoff_debug_info debug;
  const struct ecoff_debug_swap *swap
    = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;
  HDRR *symhdr = &debug.symbolic_header;
  PTR mdebug_handle = NULL;

#if 0
	      if (++ngots == 2)
		{
		  (*info->callbacks->warning)
		    (info, _("using multiple gp values"), (char *) NULL,
		     output_bfd, (asection *) NULL, (bfd_vma) 0);
		}
#endif

  /* Go through the sections and collect the .reginfo and .mdebug
     information.  */
  reginfo_sec = NULL;
  mdebug_sec = NULL;
  gptab_data_sec = NULL;
  gptab_bss_sec = NULL;
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
#ifdef ERIC_neverdef
      if (strcmp (o->name, ".reginfo") == 0)
	{
	  memset (&reginfo, 0, sizeof reginfo);

	  /* We have found the .reginfo section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      Elf64_External_RegInfo ext;
	      Elf64_RegInfo sub;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* The linker emulation code has probably clobbered the
                 size to be zero bytes.  */
	      if (input_section->_raw_size == 0)
		input_section->_raw_size = sizeof (Elf64_External_RegInfo);

	      if (! bfd_get_section_contents (input_bfd, input_section,
					      (PTR) &ext,
					      (file_ptr) 0,
					      sizeof ext))
		return false;

	      bfd_alpha_elf64_swap_reginfo_in (input_bfd, &ext, &sub);

	      reginfo.ri_gprmask |= sub.ri_gprmask;
	      reginfo.ri_cprmask[0] |= sub.ri_cprmask[0];
	      reginfo.ri_cprmask[1] |= sub.ri_cprmask[1];
	      reginfo.ri_cprmask[2] |= sub.ri_cprmask[2];
	      reginfo.ri_cprmask[3] |= sub.ri_cprmask[3];

	      /* ri_gp_value is set by the function
		 alpha_elf_section_processing when the section is
		 finally written out.  */

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* Force the section size to the value we want.  */
	  o->_raw_size = sizeof (Elf64_External_RegInfo);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  reginfo_sec = o;
	}
#endif

      if (strcmp (o->name, ".mdebug") == 0)
	{
	  struct extsym_info einfo;

	  /* We have found the .mdebug section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  symhdr->magic = swap->sym_magic;
	  /* FIXME: What should the version stamp be?  */
	  symhdr->vstamp = 0;
	  symhdr->ilineMax = 0;
	  symhdr->cbLine = 0;
	  symhdr->idnMax = 0;
	  symhdr->ipdMax = 0;
	  symhdr->isymMax = 0;
	  symhdr->ioptMax = 0;
	  symhdr->iauxMax = 0;
	  symhdr->issMax = 0;
	  symhdr->issExtMax = 0;
	  symhdr->ifdMax = 0;
	  symhdr->crfd = 0;
	  symhdr->iextMax = 0;

	  /* We accumulate the debugging information itself in the
	     debug_info structure.  */
	  debug.line = NULL;
	  debug.external_dnr = NULL;
	  debug.external_pdr = NULL;
	  debug.external_sym = NULL;
	  debug.external_opt = NULL;
	  debug.external_aux = NULL;
	  debug.ss = NULL;
	  debug.ssext = debug.ssext_end = NULL;
	  debug.external_fdr = NULL;
	  debug.external_rfd = NULL;
	  debug.external_ext = debug.external_ext_end = NULL;

	  mdebug_handle = bfd_ecoff_debug_init (abfd, &debug, swap, info);
	  if (mdebug_handle == (PTR) NULL)
	    return false;

	  if (1)
	    {
	      asection *s;
	      EXTR esym;
	      bfd_vma last;
	      unsigned int i;
	      static const char * const name[] =
		{
		  ".text", ".init", ".fini", ".data",
		  ".rodata", ".sdata", ".sbss", ".bss"
		};
	      static const int sc[] = { scText, scInit, scFini, scData,
					  scRData, scSData, scSBss, scBss };

	      esym.jmptbl = 0;
	      esym.cobol_main = 0;
	      esym.weakext = 0;
	      esym.reserved = 0;
	      esym.ifd = ifdNil;
	      esym.asym.iss = issNil;
	      esym.asym.st = stLocal;
	      esym.asym.reserved = 0;
	      esym.asym.index = indexNil;
	      for (i = 0; i < 8; i++)
		{
		  esym.asym.sc = sc[i];
		  s = bfd_get_section_by_name (abfd, name[i]);
		  if (s != NULL)
		    {
		      esym.asym.value = s->vma;
		      last = s->vma + s->_raw_size;
		    }
		  else
		    esym.asym.value = last;

		  if (! bfd_ecoff_debug_one_external (abfd, &debug, swap,
						      name[i], &esym))
		    return false;
		}
	    }

	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      const struct ecoff_debug_swap *input_swap;
	      struct ecoff_debug_info input_debug;
	      char *eraw_src;
	      char *eraw_end;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      if (bfd_get_flavour (input_bfd) != bfd_target_elf_flavour
		  || (get_elf_backend_data (input_bfd)
		      ->elf_backend_ecoff_debug_swap) == NULL)
		{
		  /* I don't know what a non ALPHA ELF bfd would be
		     doing with a .mdebug section, but I don't really
		     want to deal with it.  */
		  continue;
		}

	      input_swap = (get_elf_backend_data (input_bfd)
			    ->elf_backend_ecoff_debug_swap);

	      BFD_ASSERT (p->size == input_section->_raw_size);

	      /* The ECOFF linking code expects that we have already
		 read in the debugging information and set up an
		 ecoff_debug_info structure, so we do that now.  */
	      if (!elf64_alpha_read_ecoff_info (input_bfd, input_section,
						&input_debug))
		return false;

	      if (! (bfd_ecoff_debug_accumulate
		     (mdebug_handle, abfd, &debug, swap, input_bfd,
		      &input_debug, input_swap, info)))
		return false;

	      /* Loop through the external symbols.  For each one with
		 interesting information, try to find the symbol in
		 the linker global hash table and save the information
		 for the output external symbols.  */
	      eraw_src = input_debug.external_ext;
	      eraw_end = (eraw_src
			  + (input_debug.symbolic_header.iextMax
			     * input_swap->external_ext_size));
	      for (;
		   eraw_src < eraw_end;
		   eraw_src += input_swap->external_ext_size)
		{
		  EXTR ext;
		  const char *name;
		  struct alpha_elf_link_hash_entry *h;

		  (*input_swap->swap_ext_in) (input_bfd, (PTR) eraw_src, &ext);
		  if (ext.asym.sc == scNil
		      || ext.asym.sc == scUndefined
		      || ext.asym.sc == scSUndefined)
		    continue;

		  name = input_debug.ssext + ext.asym.iss;
		  h = alpha_elf_link_hash_lookup (alpha_elf_hash_table (info),
						  name, false, false, true);
		  if (h == NULL || h->esym.ifd != -2)
		    continue;

		  if (ext.ifd != -1)
		    {
		      BFD_ASSERT (ext.ifd
				  < input_debug.symbolic_header.ifdMax);
		      ext.ifd = input_debug.ifdmap[ext.ifd];
		    }

		  h->esym = ext;
		}

	      /* Free up the information we just read.  */
	      free (input_debug.line);
	      free (input_debug.external_dnr);
	      free (input_debug.external_pdr);
	      free (input_debug.external_sym);
	      free (input_debug.external_opt);
	      free (input_debug.external_aux);
	      free (input_debug.ss);
	      free (input_debug.ssext);
	      free (input_debug.external_fdr);
	      free (input_debug.external_rfd);
	      free (input_debug.external_ext);

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

#ifdef ERIC_neverdef
	  if (info->shared)
	    {
	      /* Create .rtproc section.  */
	      rtproc_sec = bfd_get_section_by_name (abfd, ".rtproc");
	      if (rtproc_sec == NULL)
		{
		  flagword flags = (SEC_HAS_CONTENTS
				    | SEC_IN_MEMORY
				    | SEC_LINKER_CREATED
				    | SEC_READONLY);

		  rtproc_sec = bfd_make_section (abfd, ".rtproc");
		  if (rtproc_sec == NULL
		      || ! bfd_set_section_flags (abfd, rtproc_sec, flags)
		      || ! bfd_set_section_alignment (abfd, rtproc_sec, 12))
		    return false;
		}

	      if (! alpha_elf_create_procedure_table (mdebug_handle, abfd,
						     info, rtproc_sec, &debug))
		return false;
	    }
#endif

	  /* Build the external symbol information.  */
	  einfo.abfd = abfd;
	  einfo.info = info;
	  einfo.debug = &debug;
	  einfo.swap = swap;
	  einfo.failed = false;
	  elf_link_hash_traverse (elf_hash_table (info),
				  elf64_alpha_output_extsym,
				  (PTR) &einfo);
	  if (einfo.failed)
	    return false;

	  /* Set the size of the .mdebug section.  */
	  o->_raw_size = bfd_ecoff_debug_size (abfd, &debug, swap);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  mdebug_sec = o;
	}

#ifdef ERIC_neverdef
      if (strncmp (o->name, ".gptab.", sizeof ".gptab." - 1) == 0)
	{
	  const char *subname;
	  unsigned int c;
	  Elf64_gptab *tab;
	  Elf64_External_gptab *ext_tab;
	  unsigned int i;

	  /* The .gptab.sdata and .gptab.sbss sections hold
	     information describing how the small data area would
	     change depending upon the -G switch.  These sections
	     not used in executables files.  */
	  if (! info->relocateable)
	    {
	      asection **secpp;

	      for (p = o->link_order_head;
		   p != (struct bfd_link_order *) NULL;
		   p = p->next)
		{
		  asection *input_section;

		  if (p->type != bfd_indirect_link_order)
		    {
		      if (p->type == bfd_fill_link_order)
			continue;
		      abort ();
		    }

		  input_section = p->u.indirect.section;

		  /* Hack: reset the SEC_HAS_CONTENTS flag so that
		     elf_link_input_bfd ignores this section.  */
		  input_section->flags &=~ SEC_HAS_CONTENTS;
		}

	      /* Skip this section later on (I don't think this
		 currently matters, but someday it might).  */
	      o->link_order_head = (struct bfd_link_order *) NULL;

	      /* Really remove the section.  */
	      for (secpp = &abfd->sections;
		   *secpp != o;
		   secpp = &(*secpp)->next)
		;
	      *secpp = (*secpp)->next;
	      --abfd->section_count;

	      continue;
	    }

	  /* There is one gptab for initialized data, and one for
	     uninitialized data.  */
	  if (strcmp (o->name, ".gptab.sdata") == 0)
	    gptab_data_sec = o;
	  else if (strcmp (o->name, ".gptab.sbss") == 0)
	    gptab_bss_sec = o;
	  else
	    {
	      (*_bfd_error_handler)
		(_("%s: illegal section name `%s'"),
		 bfd_get_filename (abfd), o->name);
	      bfd_set_error (bfd_error_nonrepresentable_section);
	      return false;
	    }

	  /* The linker script always combines .gptab.data and
	     .gptab.sdata into .gptab.sdata, and likewise for
	     .gptab.bss and .gptab.sbss.  It is possible that there is
	     no .sdata or .sbss section in the output file, in which
	     case we must change the name of the output section.  */
	  subname = o->name + sizeof ".gptab" - 1;
	  if (bfd_get_section_by_name (abfd, subname) == NULL)
	    {
	      if (o == gptab_data_sec)
		o->name = ".gptab.data";
	      else
		o->name = ".gptab.bss";
	      subname = o->name + sizeof ".gptab" - 1;
	      BFD_ASSERT (bfd_get_section_by_name (abfd, subname) != NULL);
	    }

	  /* Set up the first entry.  */
	  c = 1;
	  tab = (Elf64_gptab *) bfd_malloc (c * sizeof (Elf64_gptab));
	  if (tab == NULL)
	    return false;
	  tab[0].gt_header.gt_current_g_value = elf_gp_size (abfd);
	  tab[0].gt_header.gt_unused = 0;

	  /* Combine the input sections.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      bfd_size_type size;
	      unsigned long last;
	      bfd_size_type gpentry;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* Combine the gptab entries for this input section one
		 by one.  We know that the input gptab entries are
		 sorted by ascending -G value.  */
	      size = bfd_section_size (input_bfd, input_section);
	      last = 0;
	      for (gpentry = sizeof (Elf64_External_gptab);
		   gpentry < size;
		   gpentry += sizeof (Elf64_External_gptab))
		{
		  Elf64_External_gptab ext_gptab;
		  Elf64_gptab int_gptab;
		  unsigned long val;
		  unsigned long add;
		  boolean exact;
		  unsigned int look;

		  if (! (bfd_get_section_contents
			 (input_bfd, input_section, (PTR) &ext_gptab,
			  gpentry, sizeof (Elf64_External_gptab))))
		    {
		      free (tab);
		      return false;
		    }

		  bfd_alpha_elf64_swap_gptab_in (input_bfd, &ext_gptab,
						&int_gptab);
		  val = int_gptab.gt_entry.gt_g_value;
		  add = int_gptab.gt_entry.gt_bytes - last;

		  exact = false;
		  for (look = 1; look < c; look++)
		    {
		      if (tab[look].gt_entry.gt_g_value >= val)
			tab[look].gt_entry.gt_bytes += add;

		      if (tab[look].gt_entry.gt_g_value == val)
			exact = true;
		    }

		  if (! exact)
		    {
		      Elf64_gptab *new_tab;
		      unsigned int max;

		      /* We need a new table entry.  */
		      new_tab = ((Elf64_gptab *)
				 bfd_realloc ((PTR) tab,
					      (c + 1) * sizeof (Elf64_gptab)));
		      if (new_tab == NULL)
			{
			  free (tab);
			  return false;
			}
		      tab = new_tab;
		      tab[c].gt_entry.gt_g_value = val;
		      tab[c].gt_entry.gt_bytes = add;

		      /* Merge in the size for the next smallest -G
			 value, since that will be implied by this new
			 value.  */
		      max = 0;
		      for (look = 1; look < c; look++)
			{
			  if (tab[look].gt_entry.gt_g_value < val
			      && (max == 0
				  || (tab[look].gt_entry.gt_g_value
				      > tab[max].gt_entry.gt_g_value)))
			    max = look;
			}
		      if (max != 0)
			tab[c].gt_entry.gt_bytes +=
			  tab[max].gt_entry.gt_bytes;

		      ++c;
		    }

		  last = int_gptab.gt_entry.gt_bytes;
		}

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* The table must be sorted by -G value.  */
	  if (c > 2)
	    qsort (tab + 1, c - 1, sizeof (tab[0]), gptab_compare);

	  /* Swap out the table.  */
	  ext_tab = ((Elf64_External_gptab *)
		     bfd_alloc (abfd, c * sizeof (Elf64_External_gptab)));
	  if (ext_tab == NULL)
	    {
	      free (tab);
	      return false;
	    }

	  for (i = 0; i < c; i++)
	    bfd_alpha_elf64_swap_gptab_out (abfd, tab + i, ext_tab + i);
	  free (tab);

	  o->_raw_size = c * sizeof (Elf64_External_gptab);
	  o->contents = (bfd_byte *) ext_tab;

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;
	}
#endif

    }

  /* Invoke the regular ELF backend linker to do all the work.  */
  if (! bfd_elf64_bfd_final_link (abfd, info))
    return false;

  /* Now write out the computed sections.  */

  /* The .got subsections...  */
  {
    bfd *i, *dynobj = elf_hash_table(info)->dynobj;
    for (i = alpha_elf_hash_table(info)->got_list;
	 i != NULL;
	 i = alpha_elf_tdata(i)->got_link_next)
      {
	asection *sgot;

	/* elf_bfd_final_link already did everything in dynobj.  */
	if (i == dynobj)
	  continue;

	sgot = alpha_elf_tdata(i)->got;
	if (! bfd_set_section_contents (abfd, sgot->output_section,
					sgot->contents, sgot->output_offset,
					sgot->_raw_size))
	  return false;
      }
  }

#ifdef ERIC_neverdef
  if (reginfo_sec != (asection *) NULL)
    {
      Elf64_External_RegInfo ext;

      bfd_alpha_elf64_swap_reginfo_out (abfd, &reginfo, &ext);
      if (! bfd_set_section_contents (abfd, reginfo_sec, (PTR) &ext,
				      (file_ptr) 0, sizeof ext))
	return false;
    }
#endif

  if (mdebug_sec != (asection *) NULL)
    {
      BFD_ASSERT (abfd->output_has_begun);
      if (! bfd_ecoff_write_accumulated_debug (mdebug_handle, abfd, &debug,
					       swap, info,
					       mdebug_sec->filepos))
	return false;

      bfd_ecoff_debug_free (mdebug_handle, abfd, &debug, swap, info);
    }

  if (gptab_data_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_data_sec,
				      gptab_data_sec->contents,
				      (file_ptr) 0,
				      gptab_data_sec->_raw_size))
	return false;
    }

  if (gptab_bss_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_bss_sec,
				      gptab_bss_sec->contents,
				      (file_ptr) 0,
				      gptab_bss_sec->_raw_size))
	return false;
    }

  return true;
}

/* ECOFF swapping routines.  These are used when dealing with the
   .mdebug section, which is in the ECOFF debugging format.  Copied
   from elf32-mips.c.  */
static const struct ecoff_debug_swap
elf64_alpha_ecoff_debug_swap =
{
  /* Symbol table magic number.  */
  magicSym2,
  /* Alignment of debugging information.  E.g., 4.  */
  8,
  /* Sizes of external symbolic information.  */
  sizeof (struct hdr_ext),
  sizeof (struct dnr_ext),
  sizeof (struct pdr_ext),
  sizeof (struct sym_ext),
  sizeof (struct opt_ext),
  sizeof (struct fdr_ext),
  sizeof (struct rfd_ext),
  sizeof (struct ext_ext),
  /* Functions to swap in external symbolic data.  */
  ecoff_swap_hdr_in,
  ecoff_swap_dnr_in,
  ecoff_swap_pdr_in,
  ecoff_swap_sym_in,
  ecoff_swap_opt_in,
  ecoff_swap_fdr_in,
  ecoff_swap_rfd_in,
  ecoff_swap_ext_in,
  _bfd_ecoff_swap_tir_in,
  _bfd_ecoff_swap_rndx_in,
  /* Functions to swap out external symbolic data.  */
  ecoff_swap_hdr_out,
  ecoff_swap_dnr_out,
  ecoff_swap_pdr_out,
  ecoff_swap_sym_out,
  ecoff_swap_opt_out,
  ecoff_swap_fdr_out,
  ecoff_swap_rfd_out,
  ecoff_swap_ext_out,
  _bfd_ecoff_swap_tir_out,
  _bfd_ecoff_swap_rndx_out,
  /* Function to read in symbolic data.  */
  elf64_alpha_read_ecoff_info
};

/* Use a non-standard hash bucket size of 8.  */

const struct elf_size_info alpha_elf_size_info =
{
  sizeof (Elf64_External_Ehdr),
  sizeof (Elf64_External_Phdr),
  sizeof (Elf64_External_Shdr),
  sizeof (Elf64_External_Rel),
  sizeof (Elf64_External_Rela),
  sizeof (Elf64_External_Sym),
  sizeof (Elf64_External_Dyn),
  sizeof (Elf_External_Note),
  8,
  1,
  64, 8,
  ELFCLASS64, EV_CURRENT,
  bfd_elf64_write_out_phdrs,
  bfd_elf64_write_shdrs_and_ehdr,
  bfd_elf64_write_relocs,
  bfd_elf64_swap_symbol_out,
  bfd_elf64_slurp_reloc_table,
  bfd_elf64_slurp_symbol_table,
  bfd_elf64_swap_dyn_in,
  bfd_elf64_swap_dyn_out,
  NULL,
  NULL,
  NULL,
  NULL
};

#define TARGET_LITTLE_SYM	bfd_elf64_alpha_vec
#define TARGET_LITTLE_NAME	"elf64-alpha"
#define ELF_ARCH		bfd_arch_alpha
#define ELF_MACHINE_CODE	EM_ALPHA
#define ELF_MAXPAGESIZE	0x10000

#define bfd_elf64_bfd_link_hash_table_create \
  elf64_alpha_bfd_link_hash_table_create

#define bfd_elf64_bfd_reloc_type_lookup \
  elf64_alpha_bfd_reloc_type_lookup
#define elf_info_to_howto \
  elf64_alpha_info_to_howto

#define bfd_elf64_mkobject \
  elf64_alpha_mkobject
#define elf_backend_object_p \
  elf64_alpha_object_p

#define elf_backend_section_from_shdr \
  elf64_alpha_section_from_shdr
#define elf_backend_fake_sections \
  elf64_alpha_fake_sections

#define bfd_elf64_bfd_is_local_label_name \
  elf64_alpha_is_local_label_name
#define bfd_elf64_find_nearest_line \
  elf64_alpha_find_nearest_line
#define bfd_elf64_bfd_relax_section \
  elf64_alpha_relax_section

#define elf_backend_add_symbol_hook \
  elf64_alpha_add_symbol_hook
#define elf_backend_check_relocs \
  elf64_alpha_check_relocs
#define elf_backend_create_dynamic_sections \
  elf64_alpha_create_dynamic_sections
#define elf_backend_adjust_dynamic_symbol \
  elf64_alpha_adjust_dynamic_symbol
#define elf_backend_always_size_sections \
  elf64_alpha_always_size_sections
#define elf_backend_size_dynamic_sections \
  elf64_alpha_size_dynamic_sections
#define elf_backend_relocate_section \
  elf64_alpha_relocate_section
#define elf_backend_finish_dynamic_symbol \
  elf64_alpha_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
  elf64_alpha_finish_dynamic_sections
#define bfd_elf64_bfd_final_link \
  elf64_alpha_final_link

#define elf_backend_ecoff_debug_swap \
  &elf64_alpha_ecoff_debug_swap

#define elf_backend_size_info \
  alpha_elf_size_info

/* A few constants that determine how the .plt section is set up.  */
#define elf_backend_want_got_plt 0
#define elf_backend_plt_readonly 0
#define elf_backend_want_plt_sym 1
#define elf_backend_got_header_size 0
#define elf_backend_plt_header_size PLT_HEADER_SIZE

#include "elf64-target.h"
