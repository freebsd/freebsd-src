/* Motorola 68HC12-specific support for 32-bit ELF
   Copyright 1999, 2000, 2002, 2003 Free Software Foundation, Inc.
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
static bfd_boolean m68hc12_elf_size_one_stub
  (struct bfd_hash_entry *gen_entry, void *in_arg);
static bfd_boolean m68hc12_elf_build_one_stub
  (struct bfd_hash_entry *gen_entry, void *in_arg);
static struct bfd_link_hash_table* m68hc12_elf_bfd_link_hash_table_create
  (bfd*);

static bfd_boolean m68hc12_elf_set_mach_from_flags PARAMS ((bfd *));

/* Use REL instead of RELA to save space */
#define USE_REL	1

/* The 68HC12 microcontroler has a memory bank switching system
   with a 16Kb window in the 64Kb address space.  The extended memory
   is mapped in the 16Kb window (at 0x8000).  The page register controls
   which 16Kb bank is mapped.  The call/rtc instructions take care of
   bank switching in function calls/returns.

   For GNU Binutils to work, we consider there is a physical memory
   at 0..0x0ffff and a kind of virtual memory above that.  Symbols
   in virtual memory have their addresses treated in a special way
   when disassembling and when linking.

   For the linker to work properly, we must always relocate the virtual
   memory as if it is mapped at 0x8000.  When a 16-bit relocation is
   made in the virtual memory, we check that it does not cross the
   memory bank where it is used.  This would involve a page change
   which would be wrong.  The 24-bit relocation is for that and it
   treats the address as a physical address + page number.


					Banked
					Address Space
                                        |               |       Page n
					+---------------+ 0x1010000
                                        |               |
                                        | jsr _foo      |
                                        | ..            |       Page 3
                                        | _foo:         |
					+---------------+ 0x100C000
					|	        |
                                        | call _bar     |
					| ..	        |	Page 2
					| _bar:	        |
					+---------------+ 0x1008000
				/------>|	        |
				|	| call _foo     |	Page 1
				|	|       	|
				|	+---------------+ 0x1004000
      Physical			|	|	        |
      Address Space		|	|	        |	Page 0
				|	|	        |
    +-----------+ 0x00FFFF	|	+---------------+ 0x1000000
    |		|		|
    | call _foo	|		|
    |		|		|
    +-----------+ 0x00BFFF -+---/
    |		|           |
    |		|	    |
    |		| 16K	    |
    |		|	    |
    +-----------+ 0x008000 -+
    |		|
    |		|
    =		=
    |		|
    |		|
    +-----------+ 0000


   The 'call _foo' must be relocated with page 3 and 16-bit address
   mapped at 0x8000.

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
	 "R_M68HC12_NONE",	/* name */
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
	 "R_M68HC12_8",		/* name */
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
	 "R_M68HC12_HI8",	/* name */
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
	 "R_M68HC12_LO8",	/* name */
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
	 "R_M68HC12_PCREL_8",	/* name */
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
	 "R_M68HC12_16",	/* name */
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
	 "R_M68HC12_32",	/* name */
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
	 "R_M68HC12_4B",	/* name */
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
	 "R_M68HC12_PCREL_16",	/* name */
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
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 m68hc11_elf_special_reloc,	/* special_function */
	 "R_M68HC12_24",	/* name */
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
	 complain_overflow_dont,	/* complain_on_overflow */
	 m68hc11_elf_special_reloc,/* special_function */
	 "R_M68HC12_LO16",	/* name */
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
	 complain_overflow_dont,	/* complain_on_overflow */
	 m68hc11_elf_special_reloc,/* special_function */
	 "R_M68HC12_PAGE",	/* name */
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
	 "R_M68HC12_RL_JUMP",	/* name */
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
	 "R_M68HC12_RL_GROUP",	/* name */
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

/* Build a 68HC12 trampoline stub.  */
static bfd_boolean
m68hc12_elf_build_one_stub (struct bfd_hash_entry *gen_entry, void *in_arg)
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
  stub_sec->_raw_size += 7;
  loc = stub_sec->contents + stub_entry->stub_offset;

  stub_bfd = stub_sec->owner;

  /* Create the trampoline call stub:

     ldy #%addr(symbol)
     call %page(symbol), __trampoline

  */
  sym_value = (stub_entry->target_value
               + stub_entry->target_section->output_offset
               + stub_entry->target_section->output_section->vma);
  phys_addr = m68hc11_phys_addr (&htab->pinfo, sym_value);
  phys_page = m68hc11_phys_page (&htab->pinfo, sym_value);

  /* ldy #%page(sym) */
  bfd_put_8 (stub_bfd, 0xCD, loc);
  bfd_put_16 (stub_bfd, phys_addr, loc + 1);
  loc += 3;

  /* call %page(sym), __trampoline  */
  bfd_put_8 (stub_bfd, 0x4a, loc);
  bfd_put_16 (stub_bfd, htab->pinfo.trampoline_addr, loc + 1);
  bfd_put_8 (stub_bfd, phys_page, loc + 3);

  return TRUE;
}

/* As above, but don't actually build the stub.  Just bump offset so
   we know stub section sizes.  */

static bfd_boolean
m68hc12_elf_size_one_stub (struct bfd_hash_entry *gen_entry,
                           void *in_arg ATTRIBUTE_UNUSED)
{
  struct elf32_m68hc11_stub_hash_entry *stub_entry;

  /* Massage our args to the form they really have.  */
  stub_entry = (struct elf32_m68hc11_stub_hash_entry *) gen_entry;

  stub_entry->stub_sec->_raw_size += 7;
  return TRUE;
}

/* Create a 68HC12 ELF linker hash table.  */

static struct bfd_link_hash_table *
m68hc12_elf_bfd_link_hash_table_create (bfd *abfd)
{
  struct m68hc11_elf_link_hash_table *ret;

  ret = m68hc11_elf_hash_table_create (abfd);
  if (ret == (struct m68hc11_elf_link_hash_table *) NULL)
    return NULL;

  ret->size_one_stub = m68hc12_elf_size_one_stub;
  ret->build_one_stub = m68hc12_elf_build_one_stub;

  return &ret->root.root;
}

static bfd_boolean
m68hc12_elf_set_mach_from_flags (bfd *abfd)
{
  flagword flags = elf_elfheader (abfd)->e_flags;

  switch (flags & EF_M68HC11_MACH_MASK)
    {
    case EF_M68HC12_MACH:
      bfd_default_set_arch_mach (abfd, bfd_arch_m68hc12, bfd_mach_m6812);
      break;
    case EF_M68HCS12_MACH:
      bfd_default_set_arch_mach (abfd, bfd_arch_m68hc12, bfd_mach_m6812s);
      break;
    case EF_M68HC11_GENERIC:
      bfd_default_set_arch_mach (abfd, bfd_arch_m68hc12,
                                 bfd_mach_m6812_default);
      break;
    default:
      return FALSE;
    }
  return TRUE;
}

/* Specific sections:
   - The .page0 is a data section that is mapped in [0x0000..0x00FF].
     Page0 accesses are faster on the M68HC12.
   - The .vectors is the section that represents the interrupt
     vectors.  */
static struct bfd_elf_special_section const elf32_m68hc12_special_sections[]=
{
  { ".eeprom",   7, 0, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE },
  { ".softregs", 9, 0, SHT_NOBITS,   SHF_ALLOC + SHF_WRITE },
  { ".page0",    6, 0, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE },
  { ".vectors",  8, 0, SHT_PROGBITS, SHF_ALLOC },
  { NULL,        0, 0, 0,            0 }
};

#define ELF_ARCH		bfd_arch_m68hc12
#define ELF_MACHINE_CODE	EM_68HC12
#define ELF_MAXPAGESIZE		0x1000

#define TARGET_BIG_SYM          bfd_elf32_m68hc12_vec
#define TARGET_BIG_NAME		"elf32-m68hc12"

#define elf_info_to_howto	0
#define elf_info_to_howto_rel	m68hc11_info_to_howto_rel
#define elf_backend_gc_mark_hook     elf32_m68hc11_gc_mark_hook
#define elf_backend_gc_sweep_hook    elf32_m68hc11_gc_sweep_hook
#define elf_backend_check_relocs     elf32_m68hc11_check_relocs
#define elf_backend_relocate_section elf32_m68hc11_relocate_section
#define elf_backend_object_p		m68hc12_elf_set_mach_from_flags
#define elf_backend_final_write_processing	0
#define elf_backend_can_gc_sections		1
#define elf_backend_special_sections elf32_m68hc12_special_sections
#define elf_backend_post_process_headers     elf32_m68hc11_post_process_headers
#define elf_backend_add_symbol_hook  elf32_m68hc11_add_symbol_hook

#define bfd_elf32_bfd_link_hash_table_create \
                                m68hc12_elf_bfd_link_hash_table_create
#define bfd_elf32_bfd_link_hash_table_free \
				m68hc11_elf_bfd_link_hash_table_free
#define bfd_elf32_bfd_merge_private_bfd_data \
					_bfd_m68hc11_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags	_bfd_m68hc11_elf_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data \
					_bfd_m68hc11_elf_print_private_bfd_data

#include "elf32-target.h"
