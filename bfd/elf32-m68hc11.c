/* Motorola 68HC11-specific support for 32-bit ELF
   Copyright 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
   Contributed by Stephane Carrez (stcarrez@nerim.fr)
   (Heavily copied from the D10V port by Martin Hunt (hunt@cygnus.com))

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
#include "elf32-m68hc1x.h"
#include "elf/m68hc11.h"
#include "opcode/m68hc11.h"

/* Relocation functions.  */
static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  (bfd *, bfd_reloc_code_real_type);
static void m68hc11_info_to_howto_rel
  (bfd *, arelent *, Elf_Internal_Rela *);

/* Trampoline generation.  */
static bfd_boolean m68hc11_elf_size_one_stub
  (struct bfd_hash_entry *gen_entry, void *in_arg);
static bfd_boolean m68hc11_elf_build_one_stub
  (struct bfd_hash_entry *gen_entry, void *in_arg);
static struct bfd_link_hash_table* m68hc11_elf_bfd_link_hash_table_create
  (bfd* abfd);

/* Linker relaxation.  */
static bfd_boolean m68hc11_elf_relax_section
  (bfd *, asection *, struct bfd_link_info *, bfd_boolean *);
static void m68hc11_elf_relax_delete_bytes
  (bfd *, asection *, bfd_vma, int);
static void m68hc11_relax_group
  (bfd *, asection *, bfd_byte *, unsigned, unsigned long, unsigned long);
static int compare_reloc (const void *, const void *);

/* Use REL instead of RELA to save space */
#define USE_REL	1

/* The Motorola 68HC11 microcontroller only addresses 64Kb but we also
   support a memory bank switching mechanism similar to 68HC12.
   We must handle 8 and 16-bit relocations.  The 32-bit relocation
   are used for debugging sections (DWARF2) to represent a virtual
   address.
   The 3-bit and 16-bit PC rel relocation is only used by 68HC12.  */
static reloc_howto_type elf_m68hc11_howto_table[] = {
  /* This reloc does nothing.  */
  HOWTO (R_M68HC11_NONE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_NONE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 8 bit absolute relocation */
  HOWTO (R_M68HC11_8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_8",		/* name */
	 FALSE,			/* partial_inplace */
	 0x00ff,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 8 bit absolute relocation (upper address) */
  HOWTO (R_M68HC11_HI8,		/* type */
	 8,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_HI8",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00ff,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 8 bit absolute relocation (upper address) */
  HOWTO (R_M68HC11_LO8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_LO8",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00ff,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 8 bit PC-rel relocation */
  HOWTO (R_M68HC11_PCREL_8,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_PCREL_8",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00ff,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 TRUE),                 /* pcrel_offset */

  /* A 16 bit absolute relocation */
  HOWTO (R_M68HC11_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont /*bitfield */ ,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_16",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  This one is never used for the
     code relocation.  It's used by gas for -gstabs generation.  */
  HOWTO (R_M68HC11_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_32",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 3 bit absolute relocation */
  HOWTO (R_M68HC11_3B,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 3,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_4B",	/* name */
	 FALSE,			/* partial_inplace */
	 0x003,			/* src_mask */
	 0x003,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit PC-rel relocation */
  HOWTO (R_M68HC11_PCREL_16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_PCREL_16",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),                 /* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_M68HC11_GNU_VTINHERIT,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_M68HC11_GNU_VTINHERIT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_M68HC11_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 _bfd_elf_rel_vtable_reloc_fn,	/* special_function */
	 "R_M68HC11_GNU_VTENTRY",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 24 bit relocation */
  HOWTO (R_M68HC11_24,	        /* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_24",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16-bit low relocation */
  HOWTO (R_M68HC11_LO16,        /* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_LO16",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A page relocation */
  HOWTO (R_M68HC11_PAGE,        /* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M68HC11_PAGE",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00ff,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  EMPTY_HOWTO (14),
  EMPTY_HOWTO (15),
  EMPTY_HOWTO (16),
  EMPTY_HOWTO (17),
  EMPTY_HOWTO (18),
  EMPTY_HOWTO (19),

  /* Mark beginning of a jump instruction (any form).  */
  HOWTO (R_M68HC11_RL_JUMP,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 m68hc11_elf_ignore_reloc,	/* special_function */
	 "R_M68HC11_RL_JUMP",	/* name */
	 TRUE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),                 /* pcrel_offset */

  /* Mark beginning of Gcc relaxation group instruction.  */
  HOWTO (R_M68HC11_RL_GROUP,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 m68hc11_elf_ignore_reloc,	/* special_function */
	 "R_M68HC11_RL_GROUP",	/* name */
	 TRUE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),                 /* pcrel_offset */
};

/* Map BFD reloc types to M68HC11 ELF reloc types.  */

struct m68hc11_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct m68hc11_reloc_map m68hc11_reloc_map[] = {
  {BFD_RELOC_NONE, R_M68HC11_NONE,},
  {BFD_RELOC_8, R_M68HC11_8},
  {BFD_RELOC_M68HC11_HI8, R_M68HC11_HI8},
  {BFD_RELOC_M68HC11_LO8, R_M68HC11_LO8},
  {BFD_RELOC_8_PCREL, R_M68HC11_PCREL_8},
  {BFD_RELOC_16_PCREL, R_M68HC11_PCREL_16},
  {BFD_RELOC_16, R_M68HC11_16},
  {BFD_RELOC_32, R_M68HC11_32},
  {BFD_RELOC_M68HC11_3B, R_M68HC11_3B},

  {BFD_RELOC_VTABLE_INHERIT, R_M68HC11_GNU_VTINHERIT},
  {BFD_RELOC_VTABLE_ENTRY, R_M68HC11_GNU_VTENTRY},

  {BFD_RELOC_M68HC11_LO16, R_M68HC11_LO16},
  {BFD_RELOC_M68HC11_PAGE, R_M68HC11_PAGE},
  {BFD_RELOC_M68HC11_24, R_M68HC11_24},

  {BFD_RELOC_M68HC11_RL_JUMP, R_M68HC11_RL_JUMP},
  {BFD_RELOC_M68HC11_RL_GROUP, R_M68HC11_RL_GROUP},
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
                                 bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0;
       i < sizeof (m68hc11_reloc_map) / sizeof (struct m68hc11_reloc_map);
       i++)
    {
      if (m68hc11_reloc_map[i].bfd_reloc_val == code)
	return &elf_m68hc11_howto_table[m68hc11_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Set the howto pointer for an M68HC11 ELF reloc.  */

static void
m68hc11_info_to_howto_rel (bfd *abfd ATTRIBUTE_UNUSED,
                           arelent *cache_ptr, Elf_Internal_Rela *dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_M68HC11_max);
  cache_ptr->howto = &elf_m68hc11_howto_table[r_type];
}


/* Far trampoline generation.  */

/* Build a 68HC11 trampoline stub.  */
static bfd_boolean
m68hc11_elf_build_one_stub (struct bfd_hash_entry *gen_entry, void *in_arg)
{
  struct elf32_m68hc11_stub_hash_entry *stub_entry;
  struct bfd_link_info *info;
  struct m68hc11_elf_link_hash_table *htab;
  asection *stub_sec;
  bfd *stub_bfd;
  bfd_byte *loc;
  bfd_vma sym_value, phys_page, phys_addr;

  /* Massage our args to the form they really have.  */
  stub_entry = (struct elf32_m68hc11_stub_hash_entry *) gen_entry;
  info = (struct bfd_link_info *) in_arg;

  htab = m68hc11_elf_hash_table (info);

  stub_sec = stub_entry->stub_sec;

  /* Make a note of the offset within the stubs for this entry.  */
  stub_entry->stub_offset = stub_sec->_raw_size;
  stub_sec->_raw_size += 10;
  loc = stub_sec->contents + stub_entry->stub_offset;

  stub_bfd = stub_sec->owner;

  /* Create the trampoline call stub:

     pshb
     ldab #%page(symbol)
     ldy #%addr(symbol)
     jmp __trampoline

  */
  sym_value = (stub_entry->target_value
               + stub_entry->target_section->output_offset
               + stub_entry->target_section->output_section->vma);
  phys_addr = m68hc11_phys_addr (&htab->pinfo, sym_value);
  phys_page = m68hc11_phys_page (&htab->pinfo, sym_value);

  /* pshb; ldab #%page(sym) */
  bfd_put_8 (stub_bfd, 0x37, loc);
  bfd_put_8 (stub_bfd, 0xC6, loc + 1);
  bfd_put_8 (stub_bfd, phys_page, loc + 2);
  loc += 3;

  /* ldy #%addr(sym)  */
  bfd_put_8 (stub_bfd, 0x18, loc);
  bfd_put_8 (stub_bfd, 0xCE, loc + 1);
  bfd_put_16 (stub_bfd, phys_addr, loc + 2);
  loc += 4;

  /* jmp __trampoline  */
  bfd_put_8 (stub_bfd, 0x7E, loc);
  bfd_put_16 (stub_bfd, htab->pinfo.trampoline_addr, loc + 1);

  return TRUE;
}

/* As above, but don't actually build the stub.  Just bump offset so
   we know stub section sizes.  */

static bfd_boolean
m68hc11_elf_size_one_stub (struct bfd_hash_entry *gen_entry,
                           void *in_arg ATTRIBUTE_UNUSED)
{
  struct elf32_m68hc11_stub_hash_entry *stub_entry;

  /* Massage our args to the form they really have.  */
  stub_entry = (struct elf32_m68hc11_stub_hash_entry *) gen_entry;

  stub_entry->stub_sec->_raw_size += 10;
  return TRUE;
}

/* Create a 68HC11 ELF linker hash table.  */

static struct bfd_link_hash_table *
m68hc11_elf_bfd_link_hash_table_create (bfd *abfd)
{
  struct m68hc11_elf_link_hash_table *ret;

  ret = m68hc11_elf_hash_table_create (abfd);
  if (ret == (struct m68hc11_elf_link_hash_table *) NULL)
    return NULL;

  ret->size_one_stub = m68hc11_elf_size_one_stub;
  ret->build_one_stub = m68hc11_elf_build_one_stub;

  return &ret->root.root;
}


/* 68HC11 Linker Relaxation.  */

struct m68hc11_direct_relax
{
  const char *name;
  unsigned char code;
  unsigned char direct_code;
} m68hc11_direct_relax_table[] = {
  { "adca", 0xB9, 0x99 },
  { "adcb", 0xF9, 0xD9 },
  { "adda", 0xBB, 0x9B },
  { "addb", 0xFB, 0xDB },
  { "addd", 0xF3, 0xD3 },
  { "anda", 0xB4, 0x94 },
  { "andb", 0xF4, 0xD4 },
  { "cmpa", 0xB1, 0x91 },
  { "cmpb", 0xF1, 0xD1 },
  { "cpd",  0xB3, 0x93 },
  { "cpxy", 0xBC, 0x9C },
/* { "cpy",  0xBC, 0x9C }, */
  { "eora", 0xB8, 0x98 },
  { "eorb", 0xF8, 0xD8 },
  { "jsr",  0xBD, 0x9D },
  { "ldaa", 0xB6, 0x96 },
  { "ldab", 0xF6, 0xD6 },
  { "ldd",  0xFC, 0xDC },
  { "lds",  0xBE, 0x9E },
  { "ldxy", 0xFE, 0xDE },
  /*  { "ldy",  0xFE, 0xDE },*/
  { "oraa", 0xBA, 0x9A },
  { "orab", 0xFA, 0xDA },
  { "sbca", 0xB2, 0x92 },
  { "sbcb", 0xF2, 0xD2 },
  { "staa", 0xB7, 0x97 },
  { "stab", 0xF7, 0xD7 },
  { "std",  0xFD, 0xDD },
  { "sts",  0xBF, 0x9F },
  { "stxy", 0xFF, 0xDF },
  /*  { "sty",  0xFF, 0xDF },*/
  { "suba", 0xB0, 0x90 },
  { "subb", 0xF0, 0xD0 },
  { "subd", 0xB3, 0x93 },
  { 0, 0, 0 }
};

static struct m68hc11_direct_relax *
find_relaxable_insn (unsigned char code)
{
  int i;

  for (i = 0; m68hc11_direct_relax_table[i].name; i++)
    if (m68hc11_direct_relax_table[i].code == code)
      return &m68hc11_direct_relax_table[i];

  return 0;
}

static int
compare_reloc (const void *e1, const void *e2)
{
  const Elf_Internal_Rela *i1 = (const Elf_Internal_Rela *) e1;
  const Elf_Internal_Rela *i2 = (const Elf_Internal_Rela *) e2;

  if (i1->r_offset == i2->r_offset)
    return 0;
  else
    return i1->r_offset < i2->r_offset ? -1 : 1;
}

#define M6811_OP_LDX_IMMEDIATE (0xCE)

static void
m68hc11_relax_group (bfd *abfd, asection *sec, bfd_byte *contents,
                     unsigned value, unsigned long offset,
                     unsigned long end_group)
{
  unsigned char code;
  unsigned long start_offset;
  unsigned long ldx_offset = offset;
  unsigned long ldx_size;
  int can_delete_ldx;
  int relax_ldy = 0;

  /* First instruction of the relax group must be a
     LDX #value or LDY #value.  If this is not the case,
     ignore the relax group.  */
  code = bfd_get_8 (abfd, contents + offset);
  if (code == 0x18)
    {
      relax_ldy++;
      offset++;
      code = bfd_get_8 (abfd, contents + offset);
    }
  ldx_size = offset - ldx_offset + 3;
  offset += 3;
  if (code != M6811_OP_LDX_IMMEDIATE || offset >= end_group)
    return;


  /* We can remove the LDX/LDY only when all bset/brclr instructions
     of the relax group have been converted to use direct addressing
     mode.  */
  can_delete_ldx = 1;
  while (offset < end_group)
    {
      unsigned isize;
      unsigned new_value;
      int bset_use_y;

      bset_use_y = 0;
      start_offset = offset;
      code = bfd_get_8 (abfd, contents + offset);
      if (code == 0x18)
        {
          bset_use_y++;
          offset++;
          code = bfd_get_8 (abfd, contents + offset);
        }

      /* Check the instruction and translate to use direct addressing mode.  */
      switch (code)
        {
          /* bset */
        case 0x1C:
          code = 0x14;
          isize = 3;
          break;

          /* brclr */
        case 0x1F:
          code = 0x13;
          isize = 4;
          break;

          /* brset */
        case 0x1E:
          code = 0x12;
          isize = 4;
          break;

          /* bclr */
        case 0x1D:
          code = 0x15;
          isize = 3;
          break;

          /* This instruction is not recognized and we are not
             at end of the relax group.  Ignore and don't remove
             the first LDX (we don't know what it is used for...).  */
        default:
          return;
        }
      new_value = (unsigned) bfd_get_8 (abfd, contents + offset + 1);
      new_value += value;
      if ((new_value & 0xff00) == 0 && bset_use_y == relax_ldy)
        {
          bfd_put_8 (abfd, code, contents + offset);
          bfd_put_8 (abfd, new_value, contents + offset + 1);
          if (start_offset != offset)
            {
              m68hc11_elf_relax_delete_bytes (abfd, sec, start_offset,
                                              offset - start_offset);
              end_group--;
            }
        }
      else
        {
          can_delete_ldx = 0;
        }
      offset = start_offset + isize;
    }
  if (can_delete_ldx)
    {
      /* Remove the move instruction (3 or 4 bytes win).  */
      m68hc11_elf_relax_delete_bytes (abfd, sec, ldx_offset, ldx_size);
    }
}

/* This function handles relaxing for the 68HC11.


	and somewhat more difficult to support.  */

static bfd_boolean
m68hc11_elf_relax_section (bfd *abfd, asection *sec,
                           struct bfd_link_info *link_info, bfd_boolean *again)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Shdr *shndx_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *free_relocs = NULL;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  bfd_byte *free_contents = NULL;
  Elf32_External_Sym *free_extsyms = NULL;
  Elf_Internal_Rela *prev_insn_branch = NULL;
  Elf_Internal_Rela *prev_insn_group = NULL;
  unsigned insn_group_value = 0;
  Elf_Internal_Sym *isymbuf = NULL;

  /* Assume nothing changes.  */
  *again = FALSE;

  /* We don't have to do anything for a relocatable link, if
     this section does not have relocs, or if this is not a
     code section.  */
  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0)
    return TRUE;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (sec->_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  shndx_hdr = &elf_tdata (abfd)->symtab_shndx_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;
  if (! link_info->keep_memory)
    free_relocs = internal_relocs;

  /* Checking for branch relaxation relies on the relocations to
     be sorted on 'r_offset'.  This is not guaranteed so we must sort.  */
  qsort (internal_relocs, sec->reloc_count, sizeof (Elf_Internal_Rela),
         compare_reloc);

  /* Walk through them looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;
      bfd_vma value;
      Elf_Internal_Sym *isym;
      asection *sym_sec;
      int is_far = 0;

      /* If this isn't something that can be relaxed, then ignore
	 this reloc.  */
      if (ELF32_R_TYPE (irel->r_info) != (int) R_M68HC11_16
          && ELF32_R_TYPE (irel->r_info) != (int) R_M68HC11_RL_JUMP
          && ELF32_R_TYPE (irel->r_info) != (int) R_M68HC11_RL_GROUP)
        {
          prev_insn_branch = 0;
          prev_insn_group = 0;
          continue;
        }

      /* Get the section contents if we haven't done so already.  */
      if (contents == NULL)
	{
	  /* Get cached copy if it exists.  */
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  else
	    {
	      /* Go get them off disk.  */
	      contents = (bfd_byte *) bfd_malloc (sec->_raw_size);
	      if (contents == NULL)
		goto error_return;
	      free_contents = contents;

	      if (! bfd_get_section_contents (abfd, sec, contents,
					      (file_ptr) 0, sec->_raw_size))
		goto error_return;
	    }
	}

      /* Try to eliminate an unconditional 8 bit pc-relative branch
	 which immediately follows a conditional 8 bit pc-relative
	 branch around the unconditional branch.

	    original:		new:
	    bCC lab1		bCC' lab2
	    bra lab2
	   lab1:	       lab1:

	 This happens when the bCC can't reach lab2 at assembly time,
	 but due to other relaxations it can reach at link time.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_M68HC11_RL_JUMP)
	{
	  Elf_Internal_Rela *nrel;
	  unsigned char code;
          unsigned char roffset;

          prev_insn_branch = 0;
          prev_insn_group = 0;

	  /* Do nothing if this reloc is the last byte in the section.  */
	  if (irel->r_offset + 2 >= sec->_cooked_size)
	    continue;

	  /* See if the next instruction is an unconditional pc-relative
	     branch, more often than not this test will fail, so we
	     test it first to speed things up.  */
	  code = bfd_get_8 (abfd, contents + irel->r_offset + 2);
	  if (code != 0x7e)
	    continue;

	  /* Also make sure the next relocation applies to the next
	     instruction and that it's a pc-relative 8 bit branch.  */
	  nrel = irel + 1;
	  if (nrel == irelend
	      || irel->r_offset + 3 != nrel->r_offset
	      || ELF32_R_TYPE (nrel->r_info) != (int) R_M68HC11_16)
	    continue;

	  /* Make sure our destination immediately follows the
	     unconditional branch.  */
          roffset = bfd_get_8 (abfd, contents + irel->r_offset + 1);
          if (roffset != 3)
            continue;

          prev_insn_branch = irel;
          prev_insn_group = 0;
          continue;
        }

      /* Read this BFD's symbols if we haven't done so already.  */
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
	  isym = isymbuf + ELF32_R_SYM (irel->r_info);
          is_far = isym->st_other & STO_M68HC12_FAR;
          sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
	  symval = (isym->st_value
		    + sym_sec->output_section->vma
		    + sym_sec->output_offset);
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
	    {
	      /* This appears to be a reference to an undefined
                 symbol.  Just ignore it--it will be caught by the
                 regular reloc processing.  */
              prev_insn_branch = 0;
              prev_insn_group = 0;
	      continue;
	    }

          is_far = h->other & STO_M68HC12_FAR;
          isym = 0;
          sym_sec = h->root.u.def.section;
	  symval = (h->root.u.def.value
		    + sym_sec->output_section->vma
		    + sym_sec->output_offset);
	}

      if (ELF32_R_TYPE (irel->r_info) == (int) R_M68HC11_RL_GROUP)
	{
          prev_insn_branch = 0;
          prev_insn_group = 0;

	  /* Do nothing if this reloc is the last byte in the section.  */
	  if (irel->r_offset == sec->_cooked_size)
	    continue;

          prev_insn_group = irel;
          insn_group_value = isym->st_value;
          continue;
        }

      /* When we relax some bytes, the size of our section changes.
         This affects the layout of next input sections that go in our
         output section.  When the symbol is part of another section that
         will go in the same output section as the current one, it's
         final address may now be incorrect (too far).  We must let the
         linker re-compute all section offsets before processing this
         reloc.  Code example:

                                Initial             Final
         .sect .text            section size = 6    section size = 4
         jmp foo
         jmp bar
         .sect .text.foo_bar    output_offset = 6   output_offset = 4
         foo: rts
         bar: rts

         If we process the reloc now, the jmp bar is replaced by a
         relative branch to the initial bar address (output_offset 6).  */
      if (*again && sym_sec != sec
          && sym_sec->output_section == sec->output_section)
        {
          prev_insn_group = 0;
          prev_insn_branch = 0;
          continue;
        }

      value = symval;
      /* Try to turn a far branch to a near branch.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_M68HC11_16
          && prev_insn_branch)
        {
          bfd_vma offset;
          unsigned char code;

          offset = value - (prev_insn_branch->r_offset
                            + sec->output_section->vma
                            + sec->output_offset + 2);

          /* If the offset is still out of -128..+127 range,
             leave that far branch unchanged.  */
          if ((offset & 0xff80) != 0 && (offset & 0xff80) != 0xff80)
            {
              prev_insn_branch = 0;
              continue;
            }

          /* Shrink the branch.  */
          code = bfd_get_8 (abfd, contents + prev_insn_branch->r_offset);
          if (code == 0x7e)
            {
              code = 0x20;
              bfd_put_8 (abfd, code, contents + prev_insn_branch->r_offset);
              bfd_put_8 (abfd, 0xff,
                         contents + prev_insn_branch->r_offset + 1);
              irel->r_offset = prev_insn_branch->r_offset + 1;
              irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
                                           R_M68HC11_PCREL_8);
              m68hc11_elf_relax_delete_bytes (abfd, sec,
                                              irel->r_offset + 1, 1);
            }
          else
            {
              code ^= 0x1;
              bfd_put_8 (abfd, code, contents + prev_insn_branch->r_offset);
              bfd_put_8 (abfd, 0xff,
                         contents + prev_insn_branch->r_offset + 1);
              irel->r_offset = prev_insn_branch->r_offset + 1;
              irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
                                           R_M68HC11_PCREL_8);
              m68hc11_elf_relax_delete_bytes (abfd, sec,
                                              irel->r_offset + 1, 3);
            }
          prev_insn_branch = 0;
          *again = TRUE;
        }

      /* Try to turn a 16 bit address into a 8 bit page0 address.  */
      else if (ELF32_R_TYPE (irel->r_info) == (int) R_M68HC11_16
               && (value & 0xff00) == 0)
	{
          unsigned char code;
          unsigned short offset;
          struct m68hc11_direct_relax *rinfo;

          prev_insn_branch = 0;
          offset = bfd_get_16 (abfd, contents + irel->r_offset);
          offset += value;
          if ((offset & 0xff00) != 0)
            {
              prev_insn_group = 0;
              continue;
            }

          if (prev_insn_group)
            {
              unsigned long old_sec_size = sec->_cooked_size;

              /* Note that we've changed the relocation contents, etc.  */
              elf_section_data (sec)->relocs = internal_relocs;
              free_relocs = NULL;

              elf_section_data (sec)->this_hdr.contents = contents;
              free_contents = NULL;

              symtab_hdr->contents = (bfd_byte *) isymbuf;
              free_extsyms = NULL;

              m68hc11_relax_group (abfd, sec, contents, offset,
                                   prev_insn_group->r_offset,
                                   insn_group_value);
              irel = prev_insn_group;
              prev_insn_group = 0;
              irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
                                           R_M68HC11_NONE);
              if (sec->_cooked_size != old_sec_size)
                *again = TRUE;
              continue;
            }

          /* Get the opcode.  */
          code = bfd_get_8 (abfd, contents + irel->r_offset - 1);
          rinfo = find_relaxable_insn (code);
          if (rinfo == 0)
            {
              prev_insn_group = 0;
              continue;
            }

          /* Note that we've changed the relocation contents, etc.  */
          elf_section_data (sec)->relocs = internal_relocs;
          free_relocs = NULL;

          elf_section_data (sec)->this_hdr.contents = contents;
          free_contents = NULL;

          symtab_hdr->contents = (bfd_byte *) isymbuf;
          free_extsyms = NULL;

          /* Fix the opcode.  */
          /* printf ("A relaxable case : 0x%02x (%s)\n",
             code, rinfo->name); */
          bfd_put_8 (abfd, rinfo->direct_code,
                     contents + irel->r_offset - 1);

          /* Delete one byte of data (upper byte of address).  */
          m68hc11_elf_relax_delete_bytes (abfd, sec, irel->r_offset, 1);

          /* Fix the relocation's type.  */
          irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
                                       R_M68HC11_8);

          /* That will change things, so, we should relax again.  */
          *again = TRUE;
        }
      else if (ELF32_R_TYPE (irel->r_info) == R_M68HC11_16 && !is_far)
        {
          unsigned char code;
          bfd_vma offset;

          prev_insn_branch = 0;
          code = bfd_get_8 (abfd, contents + irel->r_offset - 1);
          if (code == 0x7e || code == 0xbd)
            {
              offset = value - (irel->r_offset
                                + sec->output_section->vma
                                + sec->output_offset + 1);
              offset += bfd_get_16 (abfd, contents + irel->r_offset);

              /* If the offset is still out of -128..+127 range,
                 leave that far branch unchanged.  */
              if ((offset & 0xff80) == 0 || (offset & 0xff80) == 0xff80)
                {

                  /* Note that we've changed the relocation contents, etc.  */
                  elf_section_data (sec)->relocs = internal_relocs;
                  free_relocs = NULL;

                  elf_section_data (sec)->this_hdr.contents = contents;
                  free_contents = NULL;

                  symtab_hdr->contents = (bfd_byte *) isymbuf;
                  free_extsyms = NULL;

                  /* Shrink the branch.  */
                  code = (code == 0x7e) ? 0x20 : 0x8d;
                  bfd_put_8 (abfd, code,
                             contents + irel->r_offset - 1);
                  bfd_put_8 (abfd, 0xff,
                             contents + irel->r_offset);
                  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
                                               R_M68HC11_PCREL_8);
                  m68hc11_elf_relax_delete_bytes (abfd, sec,
                                                  irel->r_offset + 1, 1);
                  /* That will change things, so, we should relax again.  */
                  *again = TRUE;
                }
            }
        }
      prev_insn_branch = 0;
      prev_insn_group = 0;
    }

  if (free_relocs != NULL)
    {
      free (free_relocs);
      free_relocs = NULL;
    }

  if (free_contents != NULL)
    {
      if (! link_info->keep_memory)
	free (free_contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
	}
      free_contents = NULL;
    }

  if (free_extsyms != NULL)
    {
      if (! link_info->keep_memory)
	free (free_extsyms);
      else
	{
	  /* Cache the symbols for elf_link_input_bfd.  */
	  symtab_hdr->contents = (unsigned char *) isymbuf;
	}
      free_extsyms = NULL;
    }

  return TRUE;

 error_return:
  if (free_relocs != NULL)
    free (free_relocs);
  if (free_contents != NULL)
    free (free_contents);
  if (free_extsyms != NULL)
    free (free_extsyms);
  return FALSE;
}

/* Delete some bytes from a section while relaxing.  */

static void
m68hc11_elf_relax_delete_bytes (bfd *abfd, asection *sec,
                                bfd_vma addr, int count)
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  bfd_vma toaddr;
  Elf_Internal_Sym *isymbuf, *isym, *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  toaddr = sec->_cooked_size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count,
	   (size_t) (toaddr - addr - count));

  sec->_cooked_size -= count;

  /* Adjust all the relocs.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      unsigned char code;
      unsigned char offset;
      unsigned short raddr;
      unsigned long old_offset;
      int branch_pos;

      old_offset = irel->r_offset;

      /* See if this reloc was for the bytes we have deleted, in which
	 case we no longer care about it.  Don't delete relocs which
	 represent addresses, though.  */
      if (ELF32_R_TYPE (irel->r_info) != R_M68HC11_RL_JUMP
          && irel->r_offset >= addr && irel->r_offset < addr + count)
        irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
                                     R_M68HC11_NONE);

      if (ELF32_R_TYPE (irel->r_info) == R_M68HC11_NONE)
        continue;

      /* Get the new reloc address.  */
      if ((irel->r_offset > addr
	   && irel->r_offset < toaddr))
	irel->r_offset -= count;

      /* If this is a PC relative reloc, see if the range it covers
         includes the bytes we have deleted.  */
      switch (ELF32_R_TYPE (irel->r_info))
	{
	default:
	  break;

	case R_M68HC11_RL_JUMP:
          code = bfd_get_8 (abfd, contents + irel->r_offset);
          switch (code)
            {
              /* jsr and jmp instruction are also marked with RL_JUMP
                 relocs but no adjustment must be made.  */
            case 0x7e:
            case 0x9d:
            case 0xbd:
              continue;

            case 0x12:
            case 0x13:
              branch_pos = 3;
              raddr = 4;

              /* Special case when we translate a brclr N,y into brclr *<addr>
                 In this case, the 0x18 page2 prefix is removed.
                 The reloc offset is not modified but the instruction
                 size is reduced by 1.  */
              if (old_offset == addr)
                raddr++;
              break;

            case 0x1e:
            case 0x1f:
              branch_pos = 3;
              raddr = 4;
              break;

            case 0x18:
              branch_pos = 4;
              raddr = 5;
              break;

            default:
              branch_pos = 1;
              raddr = 2;
              break;
            }
          offset = bfd_get_8 (abfd, contents + irel->r_offset + branch_pos);
          raddr += old_offset;
          raddr += ((unsigned short) offset | ((offset & 0x80) ? 0xff00 : 0));
          if (irel->r_offset < addr && raddr > addr)
            {
              offset -= count;
              bfd_put_8 (abfd, offset, contents + irel->r_offset + branch_pos);
            }
          else if (irel->r_offset >= addr && raddr <= addr)
            {
              offset += count;
              bfd_put_8 (abfd, offset, contents + irel->r_offset + branch_pos);
            }
          else
            {
              /*printf ("Not adjusted 0x%04x [0x%4x 0x%4x]\n", raddr,
                irel->r_offset, addr);*/
            }

          break;
	}
    }

  /* Adjust the local symbols defined in this section.  */
  isymend = isymbuf + symtab_hdr->sh_info;
  for (isym = isymbuf; isym < isymend; isym++)
    {
      if (isym->st_shndx == sec_shndx
	  && isym->st_value > addr
	  && isym->st_value <= toaddr)
	isym->st_value -= count;
    }

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
	  && sym_hash->root.u.def.value <= toaddr)
	{
	  sym_hash->root.u.def.value -= count;
	}
    }
}

/* Specific sections:
   - The .page0 is a data section that is mapped in [0x0000..0x00FF].
     Page0 accesses are faster on the M68HC11. Soft registers used by GCC-m6811
     are located in .page0.
   - The .vectors is the section that represents the interrupt
     vectors.  */
static struct bfd_elf_special_section const elf32_m68hc11_special_sections[]=
{
  { ".eeprom",   7, 0, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE },
  { ".softregs", 9, 0, SHT_NOBITS,   SHF_ALLOC + SHF_WRITE },
  { ".page0",    6, 0, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE },
  { ".vectors",  8, 0, SHT_PROGBITS, SHF_ALLOC },
  { NULL,        0, 0, 0,            0 }
};

#define ELF_ARCH		bfd_arch_m68hc11
#define ELF_MACHINE_CODE	EM_68HC11
#define ELF_MAXPAGESIZE		0x1000

#define TARGET_BIG_SYM          bfd_elf32_m68hc11_vec
#define TARGET_BIG_NAME		"elf32-m68hc11"

#define elf_info_to_howto	0
#define elf_info_to_howto_rel	m68hc11_info_to_howto_rel
#define bfd_elf32_bfd_relax_section  m68hc11_elf_relax_section
#define elf_backend_gc_mark_hook     elf32_m68hc11_gc_mark_hook
#define elf_backend_gc_sweep_hook    elf32_m68hc11_gc_sweep_hook
#define elf_backend_check_relocs     elf32_m68hc11_check_relocs
#define elf_backend_relocate_section elf32_m68hc11_relocate_section
#define elf_backend_add_symbol_hook  elf32_m68hc11_add_symbol_hook
#define elf_backend_object_p	0
#define elf_backend_final_write_processing	0
#define elf_backend_can_gc_sections		1
#define elf_backend_special_sections elf32_m68hc11_special_sections

#define bfd_elf32_bfd_link_hash_table_create \
                                m68hc11_elf_bfd_link_hash_table_create
#define bfd_elf32_bfd_link_hash_table_free \
				m68hc11_elf_bfd_link_hash_table_free
#define bfd_elf32_bfd_merge_private_bfd_data \
					_bfd_m68hc11_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags	_bfd_m68hc11_elf_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data \
					_bfd_m68hc11_elf_print_private_bfd_data

#include "elf32-target.h"
