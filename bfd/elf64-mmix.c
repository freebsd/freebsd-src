/* MMIX-specific support for 64-bit ELF.
   Copyright 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
   Contributed by Hans-Peter Nilsson <hp@bitrange.com>

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

/* No specific ABI or "processor-specific supplement" defined.  */

/* TODO:
   - "Traditional" linker relaxation (shrinking whole sections).
   - Merge reloc stubs jumping to same location.
   - GETA stub relaxation (call a stub for out of range new
     R_MMIX_GETA_STUBBABLE).  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/mmix.h"
#include "opcode/mmix.h"

#define MINUS_ONE	(((bfd_vma) 0) - 1)

#define MAX_PUSHJ_STUB_SIZE (5 * 4)

/* Put these everywhere in new code.  */
#define FATAL_DEBUG						\
 _bfd_abort (__FILE__, __LINE__,				\
	     "Internal: Non-debugged code (test-case missing)")

#define BAD_CASE(x)				\
 _bfd_abort (__FILE__, __LINE__,		\
	     "bad case for " #x)

struct _mmix_elf_section_data
{
  struct bfd_elf_section_data elf;
  union
  {
    struct bpo_reloc_section_info *reloc;
    struct bpo_greg_section_info *greg;
  } bpo;

  struct pushj_stub_info
  {
    /* Maximum number of stubs needed for this section.  */
    bfd_size_type n_pushj_relocs;

    /* Size of stubs after a mmix_elf_relax_section round.  */
    bfd_size_type stubs_size_sum;

    /* Per-reloc stubs_size_sum information.  The stubs_size_sum member is the sum
       of these.  Allocated in mmix_elf_check_common_relocs.  */
    bfd_size_type *stub_size;

    /* Offset of next stub during relocation.  Somewhat redundant with the
       above: error coverage is easier and we don't have to reset the
       stubs_size_sum for relocation.  */
    bfd_size_type stub_offset;
  } pjs;
};

#define mmix_elf_section_data(sec) \
  ((struct _mmix_elf_section_data *) elf_section_data (sec))

/* For each section containing a base-plus-offset (BPO) reloc, we attach
   this struct as mmix_elf_section_data (section)->bpo, which is otherwise
   NULL.  */
struct bpo_reloc_section_info
  {
    /* The base is 1; this is the first number in this section.  */
    size_t first_base_plus_offset_reloc;

    /* Number of BPO-relocs in this section.  */
    size_t n_bpo_relocs_this_section;

    /* Running index, used at relocation time.  */
    size_t bpo_index;

    /* We don't have access to the bfd_link_info struct in
       mmix_final_link_relocate.  What we really want to get at is the
       global single struct greg_relocation, so we stash it here.  */
    asection *bpo_greg_section;
  };

/* Helper struct (in global context) for the one below.
   There's one of these created for every BPO reloc.  */
struct bpo_reloc_request
  {
    bfd_vma value;

    /* Valid after relaxation.  The base is 0; the first register number
       must be added.  The offset is in range 0..255.  */
    size_t regindex;
    size_t offset;

    /* The order number for this BPO reloc, corresponding to the order in
       which BPO relocs were found.  Used to create an index after reloc
       requests are sorted.  */
    size_t bpo_reloc_no;

    /* Set when the value is computed.  Better than coding "guard values"
       into the other members.  Is FALSE only for BPO relocs in a GC:ed
       section.  */
    bfd_boolean valid;
  };

/* We attach this as mmix_elf_section_data (sec)->bpo in the linker-allocated
   greg contents section (MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME),
   which is linked into the register contents section
   (MMIX_REG_CONTENTS_SECTION_NAME).  This section is created by the
   linker; using the same hook as for usual with BPO relocs does not
   collide.  */
struct bpo_greg_section_info
  {
    /* After GC, this reflects the number of remaining, non-excluded
       BPO-relocs.  */
    size_t n_bpo_relocs;

    /* This is the number of allocated bpo_reloc_requests; the size of
       sorted_indexes.  Valid after the check.*relocs functions are called
       for all incoming sections.  It includes the number of BPO relocs in
       sections that were GC:ed.  */
    size_t n_max_bpo_relocs;

    /* A counter used to find out when to fold the BPO gregs, since we
       don't have a single "after-relaxation" hook.  */
    size_t n_remaining_bpo_relocs_this_relaxation_round;

    /* The number of linker-allocated GREGs resulting from BPO relocs.
       This is an approximation after _bfd_mmix_before_linker_allocation
       and supposedly accurate after mmix_elf_relax_section is called for
       all incoming non-collected sections.  */
    size_t n_allocated_bpo_gregs;

    /* Index into reloc_request[], sorted on increasing "value", secondary
       by increasing index for strict sorting order.  */
    size_t *bpo_reloc_indexes;

    /* An array of all relocations, with the "value" member filled in by
       the relaxation function.  */
    struct bpo_reloc_request *reloc_request;
  };

static bfd_boolean mmix_elf_link_output_symbol_hook
  PARAMS ((struct bfd_link_info *, const char *, Elf_Internal_Sym *,
	   asection *, struct elf_link_hash_entry *));

static bfd_reloc_status_type mmix_elf_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

static reloc_howto_type *bfd_elf64_bfd_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));

static void mmix_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));

static int mmix_elf_sort_relocs PARAMS ((const PTR, const PTR));

static bfd_boolean mmix_elf_new_section_hook
  PARAMS ((bfd *, asection *));

static bfd_boolean mmix_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

static bfd_boolean mmix_elf_check_common_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

static bfd_boolean mmix_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));

static asection * mmix_elf_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));

static bfd_boolean mmix_elf_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

static bfd_reloc_status_type mmix_final_link_relocate
  PARAMS ((reloc_howto_type *, asection *, bfd_byte *,
	   bfd_vma, bfd_signed_vma, bfd_vma, const char *, asection *));

static bfd_reloc_status_type mmix_elf_perform_relocation
  PARAMS ((asection *, reloc_howto_type *, PTR, bfd_vma, bfd_vma));

static bfd_boolean mmix_elf_section_from_bfd_section
  PARAMS ((bfd *, asection *, int *));

static bfd_boolean mmix_elf_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));

static bfd_boolean mmix_elf_is_local_label_name
  PARAMS ((bfd *, const char *));

static int bpo_reloc_request_sort_fn PARAMS ((const PTR, const PTR));

static bfd_boolean mmix_elf_relax_section
  PARAMS ((bfd *abfd, asection *sec, struct bfd_link_info *link_info,
	   bfd_boolean *again));

extern bfd_boolean mmix_elf_final_link PARAMS ((bfd *, struct bfd_link_info *));

extern void mmix_elf_symbol_processing PARAMS ((bfd *, asymbol *));

/* Only intended to be called from a debugger.  */
extern void mmix_dump_bpo_gregs
  PARAMS ((struct bfd_link_info *, bfd_error_handler_type));

static void
mmix_set_relaxable_size
  PARAMS ((bfd *, asection *, void *));

static bfd_boolean
mmix_elf_get_section_contents
  PARAMS ((bfd *, sec_ptr, void *, file_ptr, bfd_size_type));


/* Watch out: this currently needs to have elements with the same index as
   their R_MMIX_ number.  */
static reloc_howto_type elf_mmix_howto_table[] =
 {
  /* This reloc does nothing.  */
  HOWTO (R_MMIX_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 8 bit absolute relocation.  */
  HOWTO (R_MMIX_8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_8",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 16 bit absolute relocation.  */
  HOWTO (R_MMIX_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 24 bit absolute relocation.  */
  HOWTO (R_MMIX_24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_24",		/* name */
	 FALSE,			/* partial_inplace */
	 ~0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_MMIX_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_32",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 64 bit relocation.  */
  HOWTO (R_MMIX_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_64",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 8 bit PC-relative relocation.  */
  HOWTO (R_MMIX_PC_8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_PC_8",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* An 16 bit PC-relative relocation.  */
  HOWTO (R_MMIX_PC_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_PC_16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* An 24 bit PC-relative relocation.  */
  HOWTO (R_MMIX_PC_24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_PC_24",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 32 bit absolute PC-relative relocation.  */
  HOWTO (R_MMIX_PC_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_PC_32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 64 bit PC-relative relocation.  */
  HOWTO (R_MMIX_PC_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MMIX_PC_64",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_MMIX_GNU_VTINHERIT, /* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_MMIX_GNU_VTINHERIT", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_MMIX_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_elf_rel_vtable_reloc_fn,	/* special_function */
	 "R_MMIX_GNU_VTENTRY", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* The GETA relocation is supposed to get any address that could
     possibly be reached by the GETA instruction.  It can silently expand
     to get a 64-bit operand, but will complain if any of the two least
     significant bits are set.  The howto members reflect a simple GETA.  */
  HOWTO (R_MMIX_GETA,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_GETA",		/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_GETA_1,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_GETA_1",		/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_GETA_2,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_GETA_2",		/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_GETA_3,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_GETA_3",		/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* The conditional branches are supposed to reach any (code) address.
     It can silently expand to a 64-bit operand, but will emit an error if
     any of the two least significant bits are set.  The howto members
     reflect a simple branch.  */
  HOWTO (R_MMIX_CBRANCH,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_CBRANCH",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),		       	/* pcrel_offset */

  HOWTO (R_MMIX_CBRANCH_J,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_CBRANCH_J",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_CBRANCH_1,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_CBRANCH_1",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_CBRANCH_2,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_CBRANCH_2",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_CBRANCH_3,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_CBRANCH_3",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* The PUSHJ instruction can reach any (code) address, as long as it's
     the beginning of a function (no usable restriction).  It can silently
     expand to a 64-bit operand, but will emit an error if any of the two
     least significant bits are set.  It can also expand into a call to a
     stub; see R_MMIX_PUSHJ_STUBBABLE.  The howto members reflect a simple
     PUSHJ.  */
  HOWTO (R_MMIX_PUSHJ,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_PUSHJ",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_PUSHJ_1,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_PUSHJ_1",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_PUSHJ_2,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_PUSHJ_2",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_PUSHJ_3,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_PUSHJ_3",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A JMP is supposed to reach any (code) address.  By itself, it can
     reach +-64M; the expansion can reach all 64 bits.  Note that the 64M
     limit is soon reached if you link the program in wildly different
     memory segments.  The howto members reflect a trivial JMP.  */
  HOWTO (R_MMIX_JMP,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 27,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_JMP",		/* name */
	 FALSE,			/* partial_inplace */
	 ~0x1ffffff,		/* src_mask */
	 0x1ffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_JMP_1,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 27,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_JMP_1",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x1ffffff,		/* src_mask */
	 0x1ffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_JMP_2,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 27,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_JMP_2",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x1ffffff,		/* src_mask */
	 0x1ffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_MMIX_JMP_3,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 27,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_JMP_3",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x1ffffff,		/* src_mask */
	 0x1ffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* When we don't emit link-time-relaxable code from the assembler, or
     when relaxation has done all it can do, these relocs are used.  For
     GETA/PUSHJ/branches.  */
  HOWTO (R_MMIX_ADDR19,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_ADDR19",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* For JMP.  */
  HOWTO (R_MMIX_ADDR27,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 27,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_ADDR27",	/* name */
	 FALSE,			/* partial_inplace */
	 ~0x1ffffff,		/* src_mask */
	 0x1ffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A general register or the value 0..255.  If a value, then the
     instruction (offset -3) needs adjusting.  */
  HOWTO (R_MMIX_REG_OR_BYTE,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_REG_OR_BYTE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A general register.  */
  HOWTO (R_MMIX_REG,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_REG",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A register plus an index, corresponding to the relocation expression.
     The sizes must correspond to the valid range of the expression, while
     the bitmasks correspond to what we store in the image.  */
  HOWTO (R_MMIX_BASE_PLUS_OFFSET,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_BASE_PLUS_OFFSET", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A "magic" relocation for a LOCAL expression, asserting that the
     expression is less than the number of global registers.  No actual
     modification of the contents is done.  Implementing this as a
     relocation was less intrusive than e.g. putting such expressions in a
     section to discard *after* relocation.  */
  HOWTO (R_MMIX_LOCAL,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_LOCAL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_MMIX_PUSHJ_STUBBABLE, /* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 19,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mmix_elf_reloc,	/* special_function */
	 "R_MMIX_PUSHJ_STUBBABLE", /* name */
	 FALSE,			/* partial_inplace */
	 ~0x0100ffff,		/* src_mask */
	 0x0100ffff,		/* dst_mask */
	 TRUE)			/* pcrel_offset */
 };


/* Map BFD reloc types to MMIX ELF reloc types.  */

struct mmix_reloc_map
  {
    bfd_reloc_code_real_type bfd_reloc_val;
    enum elf_mmix_reloc_type elf_reloc_val;
  };


static const struct mmix_reloc_map mmix_reloc_map[] =
  {
    {BFD_RELOC_NONE, R_MMIX_NONE},
    {BFD_RELOC_8, R_MMIX_8},
    {BFD_RELOC_16, R_MMIX_16},
    {BFD_RELOC_24, R_MMIX_24},
    {BFD_RELOC_32, R_MMIX_32},
    {BFD_RELOC_64, R_MMIX_64},
    {BFD_RELOC_8_PCREL, R_MMIX_PC_8},
    {BFD_RELOC_16_PCREL, R_MMIX_PC_16},
    {BFD_RELOC_24_PCREL, R_MMIX_PC_24},
    {BFD_RELOC_32_PCREL, R_MMIX_PC_32},
    {BFD_RELOC_64_PCREL, R_MMIX_PC_64},
    {BFD_RELOC_VTABLE_INHERIT, R_MMIX_GNU_VTINHERIT},
    {BFD_RELOC_VTABLE_ENTRY, R_MMIX_GNU_VTENTRY},
    {BFD_RELOC_MMIX_GETA, R_MMIX_GETA},
    {BFD_RELOC_MMIX_CBRANCH, R_MMIX_CBRANCH},
    {BFD_RELOC_MMIX_PUSHJ, R_MMIX_PUSHJ},
    {BFD_RELOC_MMIX_JMP, R_MMIX_JMP},
    {BFD_RELOC_MMIX_ADDR19, R_MMIX_ADDR19},
    {BFD_RELOC_MMIX_ADDR27, R_MMIX_ADDR27},
    {BFD_RELOC_MMIX_REG_OR_BYTE, R_MMIX_REG_OR_BYTE},
    {BFD_RELOC_MMIX_REG, R_MMIX_REG},
    {BFD_RELOC_MMIX_BASE_PLUS_OFFSET, R_MMIX_BASE_PLUS_OFFSET},
    {BFD_RELOC_MMIX_LOCAL, R_MMIX_LOCAL},
    {BFD_RELOC_MMIX_PUSHJ_STUBBABLE, R_MMIX_PUSHJ_STUBBABLE}
  };

static reloc_howto_type *
bfd_elf64_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0;
       i < sizeof (mmix_reloc_map) / sizeof (mmix_reloc_map[0]);
       i++)
    {
      if (mmix_reloc_map[i].bfd_reloc_val == code)
	return &elf_mmix_howto_table[mmix_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

static bfd_boolean
mmix_elf_new_section_hook (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  struct _mmix_elf_section_data *sdata;
  bfd_size_type amt = sizeof (*sdata);

  sdata = (struct _mmix_elf_section_data *) bfd_zalloc (abfd, amt);
  if (sdata == NULL)
    return FALSE;
  sec->used_by_bfd = (PTR) sdata;

  return _bfd_elf_new_section_hook (abfd, sec);
}


/* This function performs the actual bitfiddling and sanity check for a
   final relocation.  Each relocation gets its *worst*-case expansion
   in size when it arrives here; any reduction in size should have been
   caught in linker relaxation earlier.  When we get here, the relocation
   looks like the smallest instruction with SWYM:s (nop:s) appended to the
   max size.  We fill in those nop:s.

   R_MMIX_GETA: (FIXME: Relaxation should break this up in 1, 2, 3 tetra)
    GETA $N,foo
   ->
    SETL $N,foo & 0xffff
    INCML $N,(foo >> 16) & 0xffff
    INCMH $N,(foo >> 32) & 0xffff
    INCH $N,(foo >> 48) & 0xffff

   R_MMIX_CBRANCH: (FIXME: Relaxation should break this up, but
   condbranches needing relaxation might be rare enough to not be
   worthwhile.)
    [P]Bcc $N,foo
   ->
    [~P]B~cc $N,.+20
    SETL $255,foo & ...
    INCML ...
    INCMH ...
    INCH ...
    GO $255,$255,0

   R_MMIX_PUSHJ: (FIXME: Relaxation...)
    PUSHJ $N,foo
   ->
    SETL $255,foo & ...
    INCML ...
    INCMH ...
    INCH ...
    PUSHGO $N,$255,0

   R_MMIX_JMP: (FIXME: Relaxation...)
    JMP foo
   ->
    SETL $255,foo & ...
    INCML ...
    INCMH ...
    INCH ...
    GO $255,$255,0

   R_MMIX_ADDR19 and R_MMIX_ADDR27 are just filled in.  */

static bfd_reloc_status_type
mmix_elf_perform_relocation (isec, howto, datap, addr, value)
     asection *isec;
     reloc_howto_type *howto;
     PTR datap;
     bfd_vma addr;
     bfd_vma value;
{
  bfd *abfd = isec->owner;
  bfd_reloc_status_type flag = bfd_reloc_ok;
  bfd_reloc_status_type r;
  int offs = 0;
  int reg = 255;

  /* The worst case bits are all similar SETL/INCML/INCMH/INCH sequences.
     We handle the differences here and the common sequence later.  */
  switch (howto->type)
    {
    case R_MMIX_GETA:
      offs = 0;
      reg = bfd_get_8 (abfd, (bfd_byte *) datap + 1);

      /* We change to an absolute value.  */
      value += addr;
      break;

    case R_MMIX_CBRANCH:
      {
	int in1 = bfd_get_16 (abfd, (bfd_byte *) datap) << 16;

	/* Invert the condition and prediction bit, and set the offset
	   to five instructions ahead.

	   We *can* do better if we want to.  If the branch is found to be
	   within limits, we could leave the branch as is; there'll just
	   be a bunch of NOP:s after it.  But we shouldn't see this
	   sequence often enough that it's worth doing it.  */

	bfd_put_32 (abfd,
		    (((in1 ^ ((PRED_INV_BIT | COND_INV_BIT) << 24)) & ~0xffff)
		     | (24/4)),
		    (bfd_byte *) datap);

	/* Put a "GO $255,$255,0" after the common sequence.  */
	bfd_put_32 (abfd,
		    ((GO_INSN_BYTE | IMM_OFFSET_BIT) << 24) | 0xffff00,
		    (bfd_byte *) datap + 20);

	/* Common sequence starts at offset 4.  */
	offs = 4;

	/* We change to an absolute value.  */
	value += addr;
      }
      break;

    case R_MMIX_PUSHJ_STUBBABLE:
      /* If the address fits, we're fine.  */
      if ((value & 3) == 0
	  /* Note rightshift 0; see R_MMIX_JMP case below.  */
	  && (r = bfd_check_overflow (complain_overflow_signed,
				      howto->bitsize,
				      0,
				      bfd_arch_bits_per_address (abfd),
				      value)) == bfd_reloc_ok)
	goto pcrel_mmix_reloc_fits;
      else
	{
	  bfd_size_type raw_size
	    = (isec->_raw_size
	       - mmix_elf_section_data (isec)->pjs.n_pushj_relocs
	       * MAX_PUSHJ_STUB_SIZE);

	  /* We have the bytes at the PUSHJ insn and need to get the
	     position for the stub.  There's supposed to be room allocated
	     for the stub.  */
	  bfd_byte *stubcontents
	    = ((char *) datap
	       - (addr - (isec->output_section->vma + isec->output_offset))
	       + raw_size
	       + mmix_elf_section_data (isec)->pjs.stub_offset);
	  bfd_vma stubaddr;

	  /* The address doesn't fit, so redirect the PUSHJ to the
	     location of the stub.  */
	  r = mmix_elf_perform_relocation (isec,
					   &elf_mmix_howto_table
					   [R_MMIX_ADDR19],
					   datap,
					   addr,
					   isec->output_section->vma
					   + isec->output_offset
					   + raw_size
					   + (mmix_elf_section_data (isec)
					      ->pjs.stub_offset)
					   - addr);
	  if (r != bfd_reloc_ok)
	    return r;

	  stubaddr
	    = (isec->output_section->vma
	       + isec->output_offset
	       + raw_size
	       + mmix_elf_section_data (isec)->pjs.stub_offset);

	  /* We generate a simple JMP if that suffices, else the whole 5
	     insn stub.  */
	  if (bfd_check_overflow (complain_overflow_signed,
				  elf_mmix_howto_table[R_MMIX_ADDR27].bitsize,
				  0,
				  bfd_arch_bits_per_address (abfd),
				  addr + value - stubaddr) == bfd_reloc_ok)
	    {
	      bfd_put_32 (abfd, JMP_INSN_BYTE << 24, stubcontents);
	      r = mmix_elf_perform_relocation (isec,
					       &elf_mmix_howto_table
					       [R_MMIX_ADDR27],
					       stubcontents,
					       stubaddr,
					       value + addr - stubaddr);
	      mmix_elf_section_data (isec)->pjs.stub_offset += 4;

	      if (raw_size
		  + mmix_elf_section_data (isec)->pjs.stub_offset
		  > isec->_cooked_size)
		abort ();

	      return r;
	    }
	  else
	    {
	      /* Put a "GO $255,0" after the common sequence.  */
	      bfd_put_32 (abfd,
			  ((GO_INSN_BYTE | IMM_OFFSET_BIT) << 24)
			  | 0xff00, (bfd_byte *) stubcontents + 16);

	      /* Prepare for the general code to set the first part of the
		 linker stub, and */
	      value += addr;
	      datap = stubcontents;
	      mmix_elf_section_data (isec)->pjs.stub_offset
		+= MAX_PUSHJ_STUB_SIZE;
	    }
	}
      break;

    case R_MMIX_PUSHJ:
      {
	int inreg = bfd_get_8 (abfd, (bfd_byte *) datap + 1);

	/* Put a "PUSHGO $N,$255,0" after the common sequence.  */
	bfd_put_32 (abfd,
		    ((PUSHGO_INSN_BYTE | IMM_OFFSET_BIT) << 24)
		    | (inreg << 16)
		    | 0xff00,
		    (bfd_byte *) datap + 16);

	/* We change to an absolute value.  */
	value += addr;
      }
      break;

    case R_MMIX_JMP:
      /* This one is a little special.  If we get here on a non-relaxing
	 link, and the destination is actually in range, we don't need to
	 execute the nops.
	 If so, we fall through to the bit-fiddling relocs.

	 FIXME: bfd_check_overflow seems broken; the relocation is
	 rightshifted before testing, so supply a zero rightshift.  */

      if (! ((value & 3) == 0
	     && (r = bfd_check_overflow (complain_overflow_signed,
					 howto->bitsize,
					 0,
					 bfd_arch_bits_per_address (abfd),
					 value)) == bfd_reloc_ok))
	{
	  /* If the relocation doesn't fit in a JMP, we let the NOP:s be
	     modified below, and put a "GO $255,$255,0" after the
	     address-loading sequence.  */
	  bfd_put_32 (abfd,
		      ((GO_INSN_BYTE | IMM_OFFSET_BIT) << 24)
		      | 0xffff00,
		      (bfd_byte *) datap + 16);

	  /* We change to an absolute value.  */
	  value += addr;
	  break;
	}
      /* FALLTHROUGH.  */
    case R_MMIX_ADDR19:
    case R_MMIX_ADDR27:
    pcrel_mmix_reloc_fits:
      /* These must be in range, or else we emit an error.  */
      if ((value & 3) == 0
	  /* Note rightshift 0; see above.  */
	  && (r = bfd_check_overflow (complain_overflow_signed,
				      howto->bitsize,
				      0,
				      bfd_arch_bits_per_address (abfd),
				      value)) == bfd_reloc_ok)
	{
	  bfd_vma in1
	    = bfd_get_32 (abfd, (bfd_byte *) datap);
	  bfd_vma highbit;

	  if ((bfd_signed_vma) value < 0)
	    {
	      highbit = 1 << 24;
	      value += (1 << (howto->bitsize - 1));
	    }
	  else
	    highbit = 0;

	  value >>= 2;

	  bfd_put_32 (abfd,
		      (in1 & howto->src_mask)
		      | highbit
		      | (value & howto->dst_mask),
		      (bfd_byte *) datap);

	  return bfd_reloc_ok;
	}
      else
	return bfd_reloc_overflow;

    case R_MMIX_BASE_PLUS_OFFSET:
      {
	struct bpo_reloc_section_info *bpodata
	  = mmix_elf_section_data (isec)->bpo.reloc;
	asection *bpo_greg_section
	  = bpodata->bpo_greg_section;
	struct bpo_greg_section_info *gregdata
	  = mmix_elf_section_data (bpo_greg_section)->bpo.greg;
	size_t bpo_index
	  = gregdata->bpo_reloc_indexes[bpodata->bpo_index++];

	/* A consistency check: The value we now have in "relocation" must
	   be the same as the value we stored for that relocation.  It
	   doesn't cost much, so can be left in at all times.  */
	if (value != gregdata->reloc_request[bpo_index].value)
	  {
	    (*_bfd_error_handler)
	      (_("%s: Internal inconsistency error for value for\n\
 linker-allocated global register: linked: 0x%lx%08lx != relaxed: 0x%lx%08lx\n"),
	       bfd_get_filename (isec->owner),
	       (unsigned long) (value >> 32), (unsigned long) value,
	       (unsigned long) (gregdata->reloc_request[bpo_index].value
				>> 32),
	       (unsigned long) gregdata->reloc_request[bpo_index].value);
	    bfd_set_error (bfd_error_bad_value);
	    return bfd_reloc_overflow;
	  }

	/* Then store the register number and offset for that register
	   into datap and datap + 1 respectively.  */
	bfd_put_8 (abfd,
		   gregdata->reloc_request[bpo_index].regindex
		   + bpo_greg_section->output_section->vma / 8,
		   datap);
	bfd_put_8 (abfd,
		   gregdata->reloc_request[bpo_index].offset,
		   ((unsigned char *) datap) + 1);
	return bfd_reloc_ok;
      }

    case R_MMIX_REG_OR_BYTE:
    case R_MMIX_REG:
      if (value > 255)
	return bfd_reloc_overflow;
      bfd_put_8 (abfd, value, datap);
      return bfd_reloc_ok;

    default:
      BAD_CASE (howto->type);
    }

  /* This code adds the common SETL/INCML/INCMH/INCH worst-case
     sequence.  */

  /* Lowest two bits must be 0.  We return bfd_reloc_overflow for
     everything that looks strange.  */
  if (value & 3)
    flag = bfd_reloc_overflow;

  bfd_put_32 (abfd,
	      (SETL_INSN_BYTE << 24) | (value & 0xffff) | (reg << 16),
	      (bfd_byte *) datap + offs);
  bfd_put_32 (abfd,
	      (INCML_INSN_BYTE << 24) | ((value >> 16) & 0xffff) | (reg << 16),
	      (bfd_byte *) datap + offs + 4);
  bfd_put_32 (abfd,
	      (INCMH_INSN_BYTE << 24) | ((value >> 32) & 0xffff) | (reg << 16),
	      (bfd_byte *) datap + offs + 8);
  bfd_put_32 (abfd,
	      (INCH_INSN_BYTE << 24) | ((value >> 48) & 0xffff) | (reg << 16),
	      (bfd_byte *) datap + offs + 12);

  return flag;
}

/* Set the howto pointer for an MMIX ELF reloc (type RELA).  */

static void
mmix_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF64_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_MMIX_max);
  cache_ptr->howto = &elf_mmix_howto_table[r_type];
}

/* Any MMIX-specific relocation gets here at assembly time or when linking
   to other formats (such as mmo); this is the relocation function from
   the reloc_table.  We don't get here for final pure ELF linking.  */

static bfd_reloc_status_type
mmix_elf_reloc (abfd, reloc_entry, symbol, data, input_section,
		output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;
  bfd_reloc_status_type r;
  asection *reloc_target_output_section;
  bfd_reloc_status_type flag = bfd_reloc_ok;
  bfd_vma output_base = 0;
  bfd_vma addr;

  r = bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
			     input_section, output_bfd, error_message);

  /* If that was all that was needed (i.e. this isn't a final link, only
     some segment adjustments), we're done.  */
  if (r != bfd_reloc_continue)
    return r;

  if (bfd_is_und_section (symbol->section)
      && (symbol->flags & BSF_WEAK) == 0
      && output_bfd == (bfd *) NULL)
    return bfd_reloc_undefined;

  /* Is the address of the relocation really within the section?  */
  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Work out which section the relocation is targeted at and the
     initial relocation command value.  */

  /* Get symbol value.  (Common symbols are special.)  */
  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  reloc_target_output_section = bfd_get_output_section (symbol);

  /* Here the variable relocation holds the final address of the symbol we
     are relocating against, plus any addend.  */
  if (output_bfd)
    output_base = 0;
  else
    output_base = reloc_target_output_section->vma;

  relocation += output_base + symbol->section->output_offset;

  /* Get position of relocation.  */
  addr = (reloc_entry->address + input_section->output_section->vma
	  + input_section->output_offset);
  if (output_bfd != (bfd *) NULL)
    {
      /* Add in supplied addend.  */
      relocation += reloc_entry->addend;

      /* This is a partial relocation, and we want to apply the
	 relocation to the reloc entry rather than the raw data.
	 Modify the reloc inplace to reflect what we now know.  */
      reloc_entry->addend = relocation;
      reloc_entry->address += input_section->output_offset;
      return flag;
    }

  return mmix_final_link_relocate (reloc_entry->howto, input_section,
				   data, reloc_entry->address,
				   reloc_entry->addend, relocation,
				   bfd_asymbol_name (symbol),
				   reloc_target_output_section);
}

/* Relocate an MMIX ELF section.  Modified from elf32-fr30.c; look to it
   for guidance if you're thinking of copying this.  */

static bfd_boolean
mmix_elf_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, local_syms, local_sections)
     bfd *output_bfd ATTRIBUTE_UNUSED;
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
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;
  bfd_size_type raw_size
    = (input_section->_raw_size
       - mmix_elf_section_data (input_section)->pjs.n_pushj_relocs
       * MAX_PUSHJ_STUB_SIZE);
  size_t pjsno = 0;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
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
      bfd_boolean undefined_signalled = FALSE;

      r_type = ELF64_R_TYPE (rel->r_info);

      if (r_type == R_MMIX_GNU_VTINHERIT
	  || r_type == R_MMIX_GNU_VTENTRY)
	continue;

      r_symndx = ELF64_R_SYM (rel->r_info);

      if (info->relocatable)
	{
	  /* This is a relocatable link.  For most relocs we don't have to
	     change anything, unless the reloc is against a section
	     symbol, in which case we have to adjust according to where
	     the section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;

	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections [r_symndx];
		  rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

	  /* For PUSHJ stub relocs however, we may need to change the
	     reloc and the section contents, if the reloc doesn't reach
	     beyond the end of the output section and previous stubs.
	     Then we change the section contents to be a PUSHJ to the end
	     of the input section plus stubs (we can do that without using
	     a reloc), and then we change the reloc to be a R_MMIX_PUSHJ
	     at the stub location.  */
	  if (r_type == R_MMIX_PUSHJ_STUBBABLE)
	    {
	      /* We've already checked whether we need a stub; use that
		 knowledge.  */
	      if (mmix_elf_section_data (input_section)->pjs.stub_size[pjsno]
		  != 0)
		{
		  Elf_Internal_Rela relcpy;

		  if (mmix_elf_section_data (input_section)
		      ->pjs.stub_size[pjsno] != MAX_PUSHJ_STUB_SIZE)
		    abort ();

		  /* There's already a PUSHJ insn there, so just fill in
		     the offset bits to the stub.  */
		  if (mmix_final_link_relocate (elf_mmix_howto_table
						+ R_MMIX_ADDR19,
						input_section,
						contents,
						rel->r_offset,
						0,
						input_section
						->output_section->vma
						+ input_section->output_offset
						+ raw_size
						+ mmix_elf_section_data (input_section)
						->pjs.stub_offset,
						NULL, NULL) != bfd_reloc_ok)
		    return FALSE;

		  /* Put a JMP insn at the stub; it goes with the
		     R_MMIX_JMP reloc.  */
		  bfd_put_32 (output_bfd, JMP_INSN_BYTE << 24,
			      contents
			      + raw_size
			      + mmix_elf_section_data (input_section)
			      ->pjs.stub_offset);

		  /* Change the reloc to be at the stub, and to a full
		     R_MMIX_JMP reloc.  */
		  rel->r_info = ELF64_R_INFO (r_symndx, R_MMIX_JMP);
		  rel->r_offset
		    = (raw_size
		       + mmix_elf_section_data (input_section)
		       ->pjs.stub_offset);

		  mmix_elf_section_data (input_section)->pjs.stub_offset
		    += MAX_PUSHJ_STUB_SIZE;

		  /* Shift this reloc to the end of the relocs to maintain
		     the r_offset sorted reloc order.  */
		  relcpy = *rel;
		  memmove (rel, rel + 1, (char *) relend - (char *) rel);
		  relend[-1] = relcpy;

		  /* Back up one reloc, or else we'd skip the next reloc
		   in turn.  */
		  rel--;
		}

	      pjsno++;
	    }
	  continue;
	}

      /* This is a final link.  */
      howto = elf_mmix_howto_table + ELF64_R_TYPE (rel->r_info);
      h = NULL;
      sym = NULL;
      sec = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);

	  name = bfd_elf_string_from_elf_section (input_bfd,
						  symtab_hdr->sh_link,
						  sym->st_name);
	  if (name == NULL)
	    name = bfd_section_name (input_bfd, sec);
	}
      else
	{
	  bfd_boolean unresolved_reloc;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, undefined_signalled);
	  name = h->root.root.string;
	}

      r = mmix_final_link_relocate (howto, input_section,
				    contents, rel->r_offset,
				    rel->r_addend, relocation, name, sec);

      if (r != bfd_reloc_ok)
	{
	  bfd_boolean check_ok = TRUE;
	  const char * msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      check_ok = info->callbacks->reloc_overflow
		(info, name, howto->name, (bfd_vma) 0,
		 input_bfd, input_section, rel->r_offset);
	      break;

	    case bfd_reloc_undefined:
	      /* We may have sent this message above.  */
	      if (! undefined_signalled)
		check_ok = info->callbacks->undefined_symbol
		  (info, name, input_bfd, input_section, rel->r_offset,
		   TRUE);
	      undefined_signalled = TRUE;
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
	    check_ok = info->callbacks->warning
	      (info, msg, name, input_bfd, input_section, rel->r_offset);

	  if (! check_ok)
	    return FALSE;
	}
    }

  return TRUE;
}

/* Perform a single relocation.  By default we use the standard BFD
   routines.  A few relocs we have to do ourselves.  */

static bfd_reloc_status_type
mmix_final_link_relocate (howto, input_section, contents,
			  r_offset, r_addend, relocation, symname, symsec)
     reloc_howto_type *howto;
     asection *input_section;
     bfd_byte *contents;
     bfd_vma r_offset;
     bfd_signed_vma r_addend;
     bfd_vma relocation;
     const char *symname;
     asection *symsec;
{
  bfd_reloc_status_type r = bfd_reloc_ok;
  bfd_vma addr
    = (input_section->output_section->vma
       + input_section->output_offset
       + r_offset);
  bfd_signed_vma srel
    = (bfd_signed_vma) relocation + r_addend;

  switch (howto->type)
    {
      /* All these are PC-relative.  */
    case R_MMIX_PUSHJ_STUBBABLE:
    case R_MMIX_PUSHJ:
    case R_MMIX_CBRANCH:
    case R_MMIX_ADDR19:
    case R_MMIX_GETA:
    case R_MMIX_ADDR27:
    case R_MMIX_JMP:
      contents += r_offset;

      srel -= (input_section->output_section->vma
	       + input_section->output_offset
	       + r_offset);

      r = mmix_elf_perform_relocation (input_section, howto, contents,
				       addr, srel);
      break;

    case R_MMIX_BASE_PLUS_OFFSET:
      if (symsec == NULL)
	return bfd_reloc_undefined;

      /* Check that we're not relocating against a register symbol.  */
      if (strcmp (bfd_get_section_name (symsec->owner, symsec),
		  MMIX_REG_CONTENTS_SECTION_NAME) == 0
	  || strcmp (bfd_get_section_name (symsec->owner, symsec),
		     MMIX_REG_SECTION_NAME) == 0)
	{
	  /* Note: This is separated out into two messages in order
	     to ease the translation into other languages.  */
	  if (symname == NULL || *symname == 0)
	    (*_bfd_error_handler)
	      (_("%s: base-plus-offset relocation against register symbol: (unknown) in %s"),
	       bfd_get_filename (input_section->owner),
	       bfd_get_section_name (symsec->owner, symsec));
	  else
	    (*_bfd_error_handler)
	      (_("%s: base-plus-offset relocation against register symbol: %s in %s"),
	       bfd_get_filename (input_section->owner), symname,
	       bfd_get_section_name (symsec->owner, symsec));
	  return bfd_reloc_overflow;
	}
      goto do_mmix_reloc;

    case R_MMIX_REG_OR_BYTE:
    case R_MMIX_REG:
      /* For now, we handle these alike.  They must refer to an register
	 symbol, which is either relative to the register section and in
	 the range 0..255, or is in the register contents section with vma
	 regno * 8.  */

      /* FIXME: A better way to check for reg contents section?
	 FIXME: Postpone section->scaling to mmix_elf_perform_relocation? */
      if (symsec == NULL)
	return bfd_reloc_undefined;

      if (strcmp (bfd_get_section_name (symsec->owner, symsec),
		  MMIX_REG_CONTENTS_SECTION_NAME) == 0)
	{
	  if ((srel & 7) != 0 || srel < 32*8 || srel > 255*8)
	    {
	      /* The bfd_reloc_outofrange return value, though intuitively
		 a better value, will not get us an error.  */
	      return bfd_reloc_overflow;
	    }
	  srel /= 8;
	}
      else if (strcmp (bfd_get_section_name (symsec->owner, symsec),
		       MMIX_REG_SECTION_NAME) == 0)
	{
	  if (srel < 0 || srel > 255)
	    /* The bfd_reloc_outofrange return value, though intuitively a
	       better value, will not get us an error.  */
	    return bfd_reloc_overflow;
	}
      else
	{
	  /* Note: This is separated out into two messages in order
	     to ease the translation into other languages.  */
	  if (symname == NULL || *symname == 0)
	    (*_bfd_error_handler)
	      (_("%s: register relocation against non-register symbol: (unknown) in %s"),
	       bfd_get_filename (input_section->owner),
	       bfd_get_section_name (symsec->owner, symsec));
	  else
	    (*_bfd_error_handler)
	      (_("%s: register relocation against non-register symbol: %s in %s"),
	       bfd_get_filename (input_section->owner), symname,
	       bfd_get_section_name (symsec->owner, symsec));

	  /* The bfd_reloc_outofrange return value, though intuitively a
	     better value, will not get us an error.  */
	  return bfd_reloc_overflow;
	}
    do_mmix_reloc:
      contents += r_offset;
      r = mmix_elf_perform_relocation (input_section, howto, contents,
				       addr, srel);
      break;

    case R_MMIX_LOCAL:
      /* This isn't a real relocation, it's just an assertion that the
	 final relocation value corresponds to a local register.  We
	 ignore the actual relocation; nothing is changed.  */
      {
	asection *regsec
	  = bfd_get_section_by_name (input_section->output_section->owner,
				     MMIX_REG_CONTENTS_SECTION_NAME);
	bfd_vma first_global;

	/* Check that this is an absolute value, or a reference to the
	   register contents section or the register (symbol) section.
	   Absolute numbers can get here as undefined section.  Undefined
	   symbols are signalled elsewhere, so there's no conflict in us
	   accidentally handling it.  */
	if (!bfd_is_abs_section (symsec)
	    && !bfd_is_und_section (symsec)
	    && strcmp (bfd_get_section_name (symsec->owner, symsec),
		       MMIX_REG_CONTENTS_SECTION_NAME) != 0
	    && strcmp (bfd_get_section_name (symsec->owner, symsec),
		       MMIX_REG_SECTION_NAME) != 0)
	{
	  (*_bfd_error_handler)
	    (_("%s: directive LOCAL valid only with a register or absolute value"),
	     bfd_get_filename (input_section->owner));

	  return bfd_reloc_overflow;
	}

      /* If we don't have a register contents section, then $255 is the
	 first global register.  */
      if (regsec == NULL)
	first_global = 255;
      else
	{
	  first_global = bfd_get_section_vma (abfd, regsec) / 8;
	  if (strcmp (bfd_get_section_name (symsec->owner, symsec),
		      MMIX_REG_CONTENTS_SECTION_NAME) == 0)
	    {
	      if ((srel & 7) != 0 || srel < 32*8 || srel > 255*8)
		/* The bfd_reloc_outofrange return value, though
		   intuitively a better value, will not get us an error.  */
		return bfd_reloc_overflow;
	      srel /= 8;
	    }
	}

	if ((bfd_vma) srel >= first_global)
	  {
	    /* FIXME: Better error message.  */
	    (*_bfd_error_handler)
	      (_("%s: LOCAL directive: Register $%ld is not a local register.  First global register is $%ld."),
	       bfd_get_filename (input_section->owner), (long) srel, (long) first_global);

	    return bfd_reloc_overflow;
	  }
      }
      r = bfd_reloc_ok;
      break;

    default:
      r = _bfd_final_link_relocate (howto, input_section->owner, input_section,
				    contents, r_offset,
				    relocation, r_addend);
    }

  return r;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
mmix_elf_gc_mark_hook (sec, info, rel, h, sym)
     asection *sec;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      switch (ELF64_R_TYPE (rel->r_info))
	{
	case R_MMIX_GNU_VTINHERIT:
	case R_MMIX_GNU_VTENTRY:
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

/* Update relocation info for a GC-excluded section.  We could supposedly
   perform the allocation after GC, but there's no suitable hook between
   GC (or section merge) and the point when all input sections must be
   present.  Better to waste some memory and (perhaps) a little time.  */

static bfd_boolean
mmix_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  struct bpo_reloc_section_info *bpodata
    = mmix_elf_section_data (sec)->bpo.reloc;
  asection *allocated_gregs_section;

  /* If no bpodata here, we have nothing to do.  */
  if (bpodata == NULL)
    return TRUE;

  allocated_gregs_section = bpodata->bpo_greg_section;

  mmix_elf_section_data (allocated_gregs_section)->bpo.greg->n_bpo_relocs
    -= bpodata->n_bpo_relocs_this_section;

  return TRUE;
}

/* Sort register relocs to come before expanding relocs.  */

static int
mmix_elf_sort_relocs (p1, p2)
     const PTR p1;
     const PTR p2;
{
  const Elf_Internal_Rela *r1 = (const Elf_Internal_Rela *) p1;
  const Elf_Internal_Rela *r2 = (const Elf_Internal_Rela *) p2;
  int r1_is_reg, r2_is_reg;

  /* Sort primarily on r_offset & ~3, so relocs are done to consecutive
     insns.  */
  if ((r1->r_offset & ~(bfd_vma) 3) > (r2->r_offset & ~(bfd_vma) 3))
    return 1;
  else if ((r1->r_offset & ~(bfd_vma) 3) < (r2->r_offset & ~(bfd_vma) 3))
    return -1;

  r1_is_reg
    = (ELF64_R_TYPE (r1->r_info) == R_MMIX_REG_OR_BYTE
       || ELF64_R_TYPE (r1->r_info) == R_MMIX_REG);
  r2_is_reg
    = (ELF64_R_TYPE (r2->r_info) == R_MMIX_REG_OR_BYTE
       || ELF64_R_TYPE (r2->r_info) == R_MMIX_REG);
  if (r1_is_reg != r2_is_reg)
    return r2_is_reg - r1_is_reg;

  /* Neither or both are register relocs.  Then sort on full offset.  */
  if (r1->r_offset > r2->r_offset)
    return 1;
  else if (r1->r_offset < r2->r_offset)
    return -1;
  return 0;
}

/* Subset of mmix_elf_check_relocs, common to ELF and mmo linking.  */

static bfd_boolean
mmix_elf_check_common_relocs  (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  bfd *bpo_greg_owner = NULL;
  asection *allocated_gregs_section = NULL;
  struct bpo_greg_section_info *gregdata = NULL;
  struct bpo_reloc_section_info *bpodata = NULL;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  /* We currently have to abuse this COFF-specific member, since there's
     no target-machine-dedicated member.  There's no alternative outside
     the bfd_link_info struct; we can't specialize a hash-table since
     they're different between ELF and mmo.  */
  bpo_greg_owner = (bfd *) info->base_file;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      switch (ELF64_R_TYPE (rel->r_info))
        {
	  /* This relocation causes a GREG allocation.  We need to count
	     them, and we need to create a section for them, so we need an
	     object to fake as the owner of that section.  We can't use
	     the ELF dynobj for this, since the ELF bits assume lots of
	     DSO-related stuff if that member is non-NULL.  */
	case R_MMIX_BASE_PLUS_OFFSET:
	  /* We don't do anything with this reloc for a relocatable link.  */
	  if (info->relocatable)
	    break;

	  if (bpo_greg_owner == NULL)
	    {
	      bpo_greg_owner = abfd;
	      info->base_file = (PTR) bpo_greg_owner;
	    }

	  if (allocated_gregs_section == NULL)
	    allocated_gregs_section
	      = bfd_get_section_by_name (bpo_greg_owner,
					 MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME);

	  if (allocated_gregs_section == NULL)
	    {
	      allocated_gregs_section
		= bfd_make_section (bpo_greg_owner,
				    MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME);
	      /* Setting both SEC_ALLOC and SEC_LOAD means the section is
		 treated like any other section, and we'd get errors for
		 address overlap with the text section.  Let's set none of
		 those flags, as that is what currently happens for usual
		 GREG allocations, and that works.  */
	      if (allocated_gregs_section == NULL
		  || !bfd_set_section_flags (bpo_greg_owner,
					     allocated_gregs_section,
					     (SEC_HAS_CONTENTS
					      | SEC_IN_MEMORY
					      | SEC_LINKER_CREATED))
		  || !bfd_set_section_alignment (bpo_greg_owner,
						 allocated_gregs_section,
						 3))
		return FALSE;

	      gregdata = (struct bpo_greg_section_info *)
		bfd_zalloc (bpo_greg_owner, sizeof (struct bpo_greg_section_info));
	      if (gregdata == NULL)
		return FALSE;
	      mmix_elf_section_data (allocated_gregs_section)->bpo.greg
		= gregdata;
	    }
	  else if (gregdata == NULL)
	    gregdata
	      = mmix_elf_section_data (allocated_gregs_section)->bpo.greg;

	  /* Get ourselves some auxiliary info for the BPO-relocs.  */
	  if (bpodata == NULL)
	    {
	      /* No use doing a separate iteration pass to find the upper
		 limit - just use the number of relocs.  */
	      bpodata = (struct bpo_reloc_section_info *)
		bfd_alloc (bpo_greg_owner,
			   sizeof (struct bpo_reloc_section_info)
			   * (sec->reloc_count + 1));
	      if (bpodata == NULL)
		return FALSE;
	      mmix_elf_section_data (sec)->bpo.reloc = bpodata;
	      bpodata->first_base_plus_offset_reloc
		= bpodata->bpo_index
		= gregdata->n_max_bpo_relocs;
	      bpodata->bpo_greg_section
		= allocated_gregs_section;
	      bpodata->n_bpo_relocs_this_section = 0;
	    }

	  bpodata->n_bpo_relocs_this_section++;
	  gregdata->n_max_bpo_relocs++;

	  /* We don't get another chance to set this before GC; we've not
	     set up any hook that runs before GC.  */
	  gregdata->n_bpo_relocs
	    = gregdata->n_max_bpo_relocs;
	  break;

	case R_MMIX_PUSHJ_STUBBABLE:
	  mmix_elf_section_data (sec)->pjs.n_pushj_relocs++;
	  break;
	}
    }

  /* Allocate per-reloc stub storage and initialize it to the max stub
     size.  */
  if (mmix_elf_section_data (sec)->pjs.n_pushj_relocs != 0)
    {
      size_t i;

      mmix_elf_section_data (sec)->pjs.stub_size
	= bfd_alloc (abfd, mmix_elf_section_data (sec)->pjs.n_pushj_relocs
		     * sizeof (mmix_elf_section_data (sec)
			       ->pjs.stub_size[0]));
      if (mmix_elf_section_data (sec)->pjs.stub_size == NULL)
	return FALSE;

      for (i = 0; i < mmix_elf_section_data (sec)->pjs.n_pushj_relocs; i++)
	mmix_elf_section_data (sec)->pjs.stub_size[i] = MAX_PUSHJ_STUB_SIZE;
    }

  return TRUE;
}

/* Look through the relocs for a section during the first phase.  */

static bfd_boolean
mmix_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof(Elf64_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  /* First we sort the relocs so that any register relocs come before
     expansion-relocs to the same insn.  FIXME: Not done for mmo.  */
  qsort ((PTR) relocs, sec->reloc_count, sizeof (Elf_Internal_Rela),
	 mmix_elf_sort_relocs);

  /* Do the common part.  */
  if (!mmix_elf_check_common_relocs (abfd, info, sec, relocs))
    return FALSE;

  if (info->relocatable)
    return TRUE;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF64_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
        h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      switch (ELF64_R_TYPE (rel->r_info))
	{
        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_MMIX_GNU_VTINHERIT:
          if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_MMIX_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return FALSE;
          break;
	}
    }

  return TRUE;
}

/* Wrapper for mmix_elf_check_common_relocs, called when linking to mmo.
   Copied from elf_link_add_object_symbols.  */

bfd_boolean
_bfd_mmix_check_all_relocs (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection *o;

  for (o = abfd->sections; o != NULL; o = o->next)
    {
      Elf_Internal_Rela *internal_relocs;
      bfd_boolean ok;

      if ((o->flags & SEC_RELOC) == 0
	  || o->reloc_count == 0
	  || ((info->strip == strip_all || info->strip == strip_debugger)
	      && (o->flags & SEC_DEBUGGING) != 0)
	  || bfd_is_abs_section (o->output_section))
	continue;

      internal_relocs
	= _bfd_elf_link_read_relocs (abfd, o, (PTR) NULL,
				     (Elf_Internal_Rela *) NULL,
				     info->keep_memory);
      if (internal_relocs == NULL)
	return FALSE;

      ok = mmix_elf_check_common_relocs (abfd, info, o, internal_relocs);

      if (! info->keep_memory)
	free (internal_relocs);

      if (! ok)
	return FALSE;
    }

  return TRUE;
}

/* Change symbols relative to the reg contents section to instead be to
   the register section, and scale them down to correspond to the register
   number.  */

static bfd_boolean
mmix_elf_link_output_symbol_hook (info, name, sym, input_sec, h)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const char *name ATTRIBUTE_UNUSED;
     Elf_Internal_Sym *sym;
     asection *input_sec;
     struct elf_link_hash_entry *h ATTRIBUTE_UNUSED;
{
  if (input_sec != NULL
      && input_sec->name != NULL
      && ELF_ST_TYPE (sym->st_info) != STT_SECTION
      && strcmp (input_sec->name, MMIX_REG_CONTENTS_SECTION_NAME) == 0)
    {
      sym->st_value /= 8;
      sym->st_shndx = SHN_REGISTER;
    }

  return TRUE;
}

/* We fake a register section that holds values that are register numbers.
   Having a SHN_REGISTER and register section translates better to other
   formats (e.g. mmo) than for example a STT_REGISTER attribute.
   This section faking is based on a construct in elf32-mips.c.  */
static asection mmix_elf_reg_section;
static asymbol mmix_elf_reg_section_symbol;
static asymbol *mmix_elf_reg_section_symbol_ptr;

/* Handle the special section numbers that a symbol may use.  */

void
mmix_elf_symbol_processing (abfd, asym)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *asym;
{
  elf_symbol_type *elfsym;

  elfsym = (elf_symbol_type *) asym;
  switch (elfsym->internal_elf_sym.st_shndx)
    {
    case SHN_REGISTER:
      if (mmix_elf_reg_section.name == NULL)
	{
	  /* Initialize the register section.  */
	  mmix_elf_reg_section.name = MMIX_REG_SECTION_NAME;
	  mmix_elf_reg_section.flags = SEC_NO_FLAGS;
	  mmix_elf_reg_section.output_section = &mmix_elf_reg_section;
	  mmix_elf_reg_section.symbol = &mmix_elf_reg_section_symbol;
	  mmix_elf_reg_section.symbol_ptr_ptr = &mmix_elf_reg_section_symbol_ptr;
	  mmix_elf_reg_section_symbol.name = MMIX_REG_SECTION_NAME;
	  mmix_elf_reg_section_symbol.flags = BSF_SECTION_SYM;
	  mmix_elf_reg_section_symbol.section = &mmix_elf_reg_section;
	  mmix_elf_reg_section_symbol_ptr = &mmix_elf_reg_section_symbol;
	}
      asym->section = &mmix_elf_reg_section;
      break;

    default:
      break;
    }
}

/* Given a BFD section, try to locate the corresponding ELF section
   index.  */

static bfd_boolean
mmix_elf_section_from_bfd_section (abfd, sec, retval)
     bfd *                 abfd ATTRIBUTE_UNUSED;
     asection *            sec;
     int *                 retval;
{
  if (strcmp (bfd_get_section_name (abfd, sec), MMIX_REG_SECTION_NAME) == 0)
    *retval = SHN_REGISTER;
  else
    return FALSE;

  return TRUE;
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We must handle the special SHN_REGISTER section number here.

   We also check that we only have *one* each of the section-start
   symbols, since otherwise having two with the same value would cause
   them to be "merged", but with the contents serialized.  */

bfd_boolean
mmix_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Sym *sym;
     const char **namep ATTRIBUTE_UNUSED;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp ATTRIBUTE_UNUSED;
{
  if (sym->st_shndx == SHN_REGISTER)
    *secp = bfd_make_section_old_way (abfd, MMIX_REG_SECTION_NAME);
  else if ((*namep)[0] == '_' && (*namep)[1] == '_' && (*namep)[2] == '.'
	   && strncmp (*namep, MMIX_LOC_SECTION_START_SYMBOL_PREFIX,
		       strlen (MMIX_LOC_SECTION_START_SYMBOL_PREFIX)) == 0)
    {
      /* See if we have another one.  */
      struct bfd_link_hash_entry *h = bfd_link_hash_lookup (info->hash,
							    *namep,
							    FALSE,
							    FALSE,
							    FALSE);

      if (h != NULL && h->type != bfd_link_hash_undefined)
	{
	  /* How do we get the asymbol (or really: the filename) from h?
	     h->u.def.section->owner is NULL.  */
	  ((*_bfd_error_handler)
	   (_("%s: Error: multiple definition of `%s'; start of %s is set in a earlier linked file\n"),
	    bfd_get_filename (abfd), *namep,
	    *namep + strlen (MMIX_LOC_SECTION_START_SYMBOL_PREFIX)));
	   bfd_set_error (bfd_error_bad_value);
	   return FALSE;
	}
    }

  return TRUE;
}

/* We consider symbols matching "L.*:[0-9]+" to be local symbols.  */

bfd_boolean
mmix_elf_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  const char *colpos;
  int digits;

  /* Also include the default local-label definition.  */
  if (_bfd_elf_is_local_label_name (abfd, name))
    return TRUE;

  if (*name != 'L')
    return FALSE;

  /* If there's no ":", or more than one, it's not a local symbol.  */
  colpos = strchr (name, ':');
  if (colpos == NULL || strchr (colpos + 1, ':') != NULL)
    return FALSE;

  /* Check that there are remaining characters and that they are digits.  */
  if (colpos[1] == 0)
    return FALSE;

  digits = strspn (colpos + 1, "0123456789");
  return digits != 0 && colpos[1 + digits] == 0;
}

/* We get rid of the register section here.  */

bfd_boolean
mmix_elf_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  /* We never output a register section, though we create one for
     temporary measures.  Check that nobody entered contents into it.  */
  asection *reg_section;
  asection **secpp;

  reg_section = bfd_get_section_by_name (abfd, MMIX_REG_SECTION_NAME);

  if (reg_section != NULL)
    {
      /* FIXME: Pass error state gracefully.  */
      if (bfd_get_section_flags (abfd, reg_section) & SEC_HAS_CONTENTS)
	_bfd_abort (__FILE__, __LINE__, _("Register section has contents\n"));

      /* Really remove the section.  */
      for (secpp = &abfd->sections;
	   *secpp != reg_section;
	   secpp = &(*secpp)->next)
	;
      bfd_section_list_remove (abfd, secpp);
      --abfd->section_count;
    }

  if (! bfd_elf_final_link (abfd, info))
    return FALSE;

  /* Since this section is marked SEC_LINKER_CREATED, it isn't output by
     the regular linker machinery.  We do it here, like other targets with
     special sections.  */
  if (info->base_file != NULL)
    {
      asection *greg_section
	= bfd_get_section_by_name ((bfd *) info->base_file,
				   MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME);
      if (!bfd_set_section_contents (abfd,
				     greg_section->output_section,
				     greg_section->contents,
				     (file_ptr) greg_section->output_offset,
				     greg_section->_cooked_size))
	return FALSE;
    }
  return TRUE;
}

/* We need to include the maximum size of PUSHJ-stubs in the initial
   section size.  This is expected to shrink during linker relaxation.

   You might think that we should set *only* _cooked_size, but that won't
   work: section contents allocation will be using _raw_size in mixed
   format linking and not enough storage will be allocated.  FIXME: That's
   a major bug, including the name bfd_get_section_size_before_reloc; it
   should be bfd_get_section_size_before_relax.  The relaxation functions
   set _cooked size.  Relaxation happens before relocation.  All functions
   *after relaxation* should be using _cooked size.  */

static void
mmix_set_relaxable_size (abfd, sec, ptr)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     void *ptr;
{
  struct bfd_link_info *info = ptr;

  /* Make sure we only do this for section where we know we want this,
     otherwise we might end up resetting the size of COMMONs.  */
  if (mmix_elf_section_data (sec)->pjs.n_pushj_relocs == 0)
    return;

  sec->_cooked_size
    = (sec->_raw_size
       + mmix_elf_section_data (sec)->pjs.n_pushj_relocs
       * MAX_PUSHJ_STUB_SIZE);
  sec->_raw_size = sec->_cooked_size;

  /* For use in relocatable link, we start with a max stubs size.  See
     mmix_elf_relax_section.  */
  if (info->relocatable && sec->output_section)
    mmix_elf_section_data (sec->output_section)->pjs.stubs_size_sum
      += (mmix_elf_section_data (sec)->pjs.n_pushj_relocs
	  * MAX_PUSHJ_STUB_SIZE);
}

/* Initialize stuff for the linker-generated GREGs to match
   R_MMIX_BASE_PLUS_OFFSET relocs seen by the linker.  */

bfd_boolean
_bfd_mmix_before_linker_allocation (abfd, info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  asection *bpo_gregs_section;
  bfd *bpo_greg_owner;
  struct bpo_greg_section_info *gregdata;
  size_t n_gregs;
  bfd_vma gregs_size;
  size_t i;
  size_t *bpo_reloc_indexes;
  bfd *ibfd;

  /* Set the initial size of sections.  */
  for (ibfd = info->input_bfds; ibfd != NULL; ibfd = ibfd->link_next)
    bfd_map_over_sections (ibfd, mmix_set_relaxable_size, info);

  /* The bpo_greg_owner bfd is supposed to have been set by
     mmix_elf_check_relocs when the first R_MMIX_BASE_PLUS_OFFSET is seen.
     If there is no such object, there was no R_MMIX_BASE_PLUS_OFFSET.  */
  bpo_greg_owner = (bfd *) info->base_file;
  if (bpo_greg_owner == NULL)
    return TRUE;

  bpo_gregs_section
    = bfd_get_section_by_name (bpo_greg_owner,
			       MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME);

  if (bpo_gregs_section == NULL)
    return TRUE;

  /* We use the target-data handle in the ELF section data.  */
  gregdata = mmix_elf_section_data (bpo_gregs_section)->bpo.greg;
  if (gregdata == NULL)
    return FALSE;

  n_gregs = gregdata->n_bpo_relocs;
  gregdata->n_allocated_bpo_gregs = n_gregs;

  /* When this reaches zero during relaxation, all entries have been
     filled in and the size of the linker gregs can be calculated.  */
  gregdata->n_remaining_bpo_relocs_this_relaxation_round = n_gregs;

  /* Set the zeroth-order estimate for the GREGs size.  */
  gregs_size = n_gregs * 8;

  if (!bfd_set_section_size (bpo_greg_owner, bpo_gregs_section, gregs_size))
    return FALSE;

  /* Allocate and set up the GREG arrays.  They're filled in at relaxation
     time.  Note that we must use the max number ever noted for the array,
     since the index numbers were created before GC.  */
  gregdata->reloc_request
    = bfd_zalloc (bpo_greg_owner,
		  sizeof (struct bpo_reloc_request)
		  * gregdata->n_max_bpo_relocs);

  gregdata->bpo_reloc_indexes
    = bpo_reloc_indexes
    = bfd_alloc (bpo_greg_owner,
		 gregdata->n_max_bpo_relocs
		 * sizeof (size_t));
  if (bpo_reloc_indexes == NULL)
    return FALSE;

  /* The default order is an identity mapping.  */
  for (i = 0; i < gregdata->n_max_bpo_relocs; i++)
    {
      bpo_reloc_indexes[i] = i;
      gregdata->reloc_request[i].bpo_reloc_no = i;
    }

  return TRUE;
}

/* Fill in contents in the linker allocated gregs.  Everything is
   calculated at this point; we just move the contents into place here.  */

bfd_boolean
_bfd_mmix_after_linker_allocation (abfd, link_info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info;
{
  asection *bpo_gregs_section;
  bfd *bpo_greg_owner;
  struct bpo_greg_section_info *gregdata;
  size_t n_gregs;
  size_t i, j;
  size_t lastreg;
  bfd_byte *contents;

  /* The bpo_greg_owner bfd is supposed to have been set by mmix_elf_check_relocs
     when the first R_MMIX_BASE_PLUS_OFFSET is seen.  If there is no such
     object, there was no R_MMIX_BASE_PLUS_OFFSET.  */
  bpo_greg_owner = (bfd *) link_info->base_file;
  if (bpo_greg_owner == NULL)
    return TRUE;

  bpo_gregs_section
    = bfd_get_section_by_name (bpo_greg_owner,
			       MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME);

  /* This can't happen without DSO handling.  When DSOs are handled
     without any R_MMIX_BASE_PLUS_OFFSET seen, there will be no such
     section.  */
  if (bpo_gregs_section == NULL)
    return TRUE;

  /* We use the target-data handle in the ELF section data.  */

  gregdata = mmix_elf_section_data (bpo_gregs_section)->bpo.greg;
  if (gregdata == NULL)
    return FALSE;

  n_gregs = gregdata->n_allocated_bpo_gregs;

  /* We need to have a _raw_size contents even though there's only
     _cooked_size worth of data, since the generic relocation machinery
     will allocate and copy that much temporarily.  */
  bpo_gregs_section->contents
    = contents = bfd_alloc (bpo_greg_owner, bpo_gregs_section->_raw_size);
  if (contents == NULL)
    return FALSE;

  /* Sanity check: If these numbers mismatch, some relocation has not been
     accounted for and the rest of gregdata is probably inconsistent.
     It's a bug, but it's more helpful to identify it than segfaulting
     below.  */
  if (gregdata->n_remaining_bpo_relocs_this_relaxation_round
      != gregdata->n_bpo_relocs)
    {
      (*_bfd_error_handler)
	(_("Internal inconsistency: remaining %u != max %u.\n\
  Please report this bug."),
	 gregdata->n_remaining_bpo_relocs_this_relaxation_round,
	 gregdata->n_bpo_relocs);
      return FALSE;
    }

  for (lastreg = 255, i = 0, j = 0; j < n_gregs; i++)
    if (gregdata->reloc_request[i].regindex != lastreg)
      {
	bfd_put_64 (bpo_greg_owner, gregdata->reloc_request[i].value,
		    contents + j * 8);
	lastreg = gregdata->reloc_request[i].regindex;
	j++;
      }

  return TRUE;
}

/* Sort valid relocs to come before non-valid relocs, then on increasing
   value.  */

static int
bpo_reloc_request_sort_fn (p1, p2)
     const PTR p1;
     const PTR p2;
{
  const struct bpo_reloc_request *r1 = (const struct bpo_reloc_request *) p1;
  const struct bpo_reloc_request *r2 = (const struct bpo_reloc_request *) p2;

  /* Primary function is validity; non-valid relocs sorted after valid
     ones.  */
  if (r1->valid != r2->valid)
    return r2->valid - r1->valid;

  /* Then sort on value.  Don't simplify and return just the difference of
     the values: the upper bits of the 64-bit value would be truncated on
     a host with 32-bit ints.  */
  if (r1->value != r2->value)
    return r1->value > r2->value ? 1 : -1;

  /* As a last re-sort, use the relocation number, so we get a stable
     sort.  The *addresses* aren't stable since items are swapped during
     sorting.  It depends on the qsort implementation if this actually
     happens.  */
  return r1->bpo_reloc_no > r2->bpo_reloc_no
    ? 1 : (r1->bpo_reloc_no < r2->bpo_reloc_no ? -1 : 0);
}

/* For debug use only.  Dumps the global register allocations resulting
   from base-plus-offset relocs.  */

void
mmix_dump_bpo_gregs (link_info, pf)
     struct bfd_link_info *link_info;
     bfd_error_handler_type pf;
{
  bfd *bpo_greg_owner;
  asection *bpo_gregs_section;
  struct bpo_greg_section_info *gregdata;
  unsigned int i;

  if (link_info == NULL || link_info->base_file == NULL)
    return;

  bpo_greg_owner = (bfd *) link_info->base_file;

  bpo_gregs_section
    = bfd_get_section_by_name (bpo_greg_owner,
			       MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME);

  if (bpo_gregs_section == NULL)
    return;

  gregdata = mmix_elf_section_data (bpo_gregs_section)->bpo.greg;
  if (gregdata == NULL)
    return;

  if (pf == NULL)
    pf = _bfd_error_handler;

  /* These format strings are not translated.  They are for debug purposes
     only and never displayed to an end user.  Should they escape, we
     surely want them in original.  */
  (*pf) (" n_bpo_relocs: %u\n n_max_bpo_relocs: %u\n n_remain...round: %u\n\
 n_allocated_bpo_gregs: %u\n", gregdata->n_bpo_relocs,
     gregdata->n_max_bpo_relocs,
     gregdata->n_remaining_bpo_relocs_this_relaxation_round,
     gregdata->n_allocated_bpo_gregs);

  if (gregdata->reloc_request)
    for (i = 0; i < gregdata->n_max_bpo_relocs; i++)
      (*pf) ("%4u (%4u)/%4u#%u: 0x%08lx%08lx  r: %3u o: %3u\n",
	     i,
	     (gregdata->bpo_reloc_indexes != NULL
	      ? gregdata->bpo_reloc_indexes[i] : (size_t) -1),
	     gregdata->reloc_request[i].bpo_reloc_no,
	     gregdata->reloc_request[i].valid,

	     (unsigned long) (gregdata->reloc_request[i].value >> 32),
	     (unsigned long) gregdata->reloc_request[i].value,
	     gregdata->reloc_request[i].regindex,
	     gregdata->reloc_request[i].offset);
}

/* This links all R_MMIX_BASE_PLUS_OFFSET relocs into a special array, and
   when the last such reloc is done, an index-array is sorted according to
   the values and iterated over to produce register numbers (indexed by 0
   from the first allocated register number) and offsets for use in real
   relocation.

   PUSHJ stub accounting is also done here.

   Symbol- and reloc-reading infrastructure copied from elf-m10200.c.  */

static bfd_boolean
mmix_elf_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     bfd_boolean *again;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *irel, *irelend;
  asection *bpo_gregs_section = NULL;
  struct bpo_greg_section_info *gregdata;
  struct bpo_reloc_section_info *bpodata
    = mmix_elf_section_data (sec)->bpo.reloc;
  /* The initialization is to quiet compiler warnings.  The value is to
     spot a missing actual initialization.  */
  size_t bpono = (size_t) -1;
  size_t pjsno = 0;
  bfd *bpo_greg_owner;
  Elf_Internal_Sym *isymbuf = NULL;
  bfd_size_type raw_size
    = (sec->_raw_size
       - mmix_elf_section_data (sec)->pjs.n_pushj_relocs
       * MAX_PUSHJ_STUB_SIZE);

  mmix_elf_section_data (sec)->pjs.stubs_size_sum = 0;

  /* Assume nothing changes.  */
  *again = FALSE;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (sec->_cooked_size == 0 && sec->_raw_size != 0)
    abort ();

  /* We don't have to do anything if this section does not have relocs, or
     if this is not a code section.  */
  if ((sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0
      || (sec->flags & SEC_LINKER_CREATED) != 0
      /* If no R_MMIX_BASE_PLUS_OFFSET relocs and no PUSHJ-stub relocs,
         then nothing to do.  */
      || (bpodata == NULL
	  && mmix_elf_section_data (sec)->pjs.n_pushj_relocs == 0))
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  bpo_greg_owner = (bfd *) link_info->base_file;

  if (bpodata != NULL)
    {
      bpo_gregs_section = bpodata->bpo_greg_section;
      gregdata = mmix_elf_section_data (bpo_gregs_section)->bpo.greg;
      bpono = bpodata->first_base_plus_offset_reloc;
    }
  else
    gregdata = NULL;

  /* Get a copy of the native relocations.  */
  internal_relocs
    = _bfd_elf_link_read_relocs (abfd, sec, (PTR) NULL,
				 (Elf_Internal_Rela *) NULL,
				 link_info->keep_memory);
  if (internal_relocs == NULL)
    goto error_return;

  /* Walk through them looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;
      struct elf_link_hash_entry *h = NULL;

      /* We only process two relocs.  */
      if (ELF64_R_TYPE (irel->r_info) != (int) R_MMIX_BASE_PLUS_OFFSET
	  && ELF64_R_TYPE (irel->r_info) != (int) R_MMIX_PUSHJ_STUBBABLE)
	continue;

      /* We process relocs in a distinctly different way when this is a
	 relocatable link (for one, we don't look at symbols), so we avoid
	 mixing its code with that for the "normal" relaxation.  */
      if (link_info->relocatable)
	{
	  /* The only transformation in a relocatable link is to generate
	     a full stub at the location of the stub calculated for the
	     input section, if the relocated stub location, the end of the
	     output section plus earlier stubs, cannot be reached.  Thus
	     relocatable linking can only lead to worse code, but it still
	     works.  */
	  if (ELF64_R_TYPE (irel->r_info) == R_MMIX_PUSHJ_STUBBABLE)
	    {
	      /* If we can reach the end of the output-section and beyond
		 any current stubs, then we don't need a stub for this
		 reloc.  The relaxed order of output stub allocation may
		 not exactly match the straightforward order, so we always
		 assume presence of output stubs, which will allow
		 relaxation only on relocations indifferent to the
		 presence of output stub allocations for other relocations
		 and thus the order of output stub allocation.  */
	      if (bfd_check_overflow (complain_overflow_signed,
				      19,
				      0,
				      bfd_arch_bits_per_address (abfd),
				      /* Output-stub location.  */
				      sec->output_section->_cooked_size
				      + (mmix_elf_section_data (sec
							       ->output_section)
					 ->pjs.stubs_size_sum)
				      /* Location of this PUSHJ reloc.  */
				      - (sec->output_offset + irel->r_offset)
				      /* Don't count *this* stub twice.  */
				      - (mmix_elf_section_data (sec)
					 ->pjs.stub_size[pjsno]
					 + MAX_PUSHJ_STUB_SIZE))
		  == bfd_reloc_ok)
		mmix_elf_section_data (sec)->pjs.stub_size[pjsno] = 0;

	      mmix_elf_section_data (sec)->pjs.stubs_size_sum
		+= mmix_elf_section_data (sec)->pjs.stub_size[pjsno];

	      pjsno++;
	    }

	  continue;
	}

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF64_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  Elf_Internal_Sym *isym;
	  asection *sym_sec;

	  /* Read this BFD's local symbols if we haven't already.  */
	  if (isymbuf == NULL)
	    {
	      isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	      if (isymbuf == NULL)
		isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
						symtab_hdr->sh_info, 0,
						NULL, NULL, NULL);
	      if (isymbuf == 0)
		goto error_return;
	    }

	  isym = isymbuf + ELF64_R_SYM (irel->r_info);
	  if (isym->st_shndx == SHN_UNDEF)
	    sym_sec = bfd_und_section_ptr;
	  else if (isym->st_shndx == SHN_ABS)
	    sym_sec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    sym_sec = bfd_com_section_ptr;
	  else
	    sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
	  symval = (isym->st_value
		    + sym_sec->output_section->vma
		    + sym_sec->output_offset);
	}
      else
	{
	  unsigned long indx;

	  /* An external symbol.  */
	  indx = ELF64_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);
	  if (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	    {
	      /* This appears to be a reference to an undefined symbol.  Just
		 ignore it--it will be caught by the regular reloc processing.
		 We need to keep BPO reloc accounting consistent, though
		 else we'll abort instead of emitting an error message.  */
	      if (ELF64_R_TYPE (irel->r_info) == R_MMIX_BASE_PLUS_OFFSET
		  && gregdata != NULL)
		{
		  gregdata->n_remaining_bpo_relocs_this_relaxation_round--;
		  bpono++;
		}
	      continue;
	    }

	  symval = (h->root.u.def.value
		    + h->root.u.def.section->output_section->vma
		    + h->root.u.def.section->output_offset);
	}

      if (ELF64_R_TYPE (irel->r_info) == (int) R_MMIX_PUSHJ_STUBBABLE)
	{
	  bfd_vma value = symval + irel->r_addend;
	  bfd_vma dot
	    = (sec->output_section->vma
	       + sec->output_offset
	       + irel->r_offset);
	  bfd_vma stubaddr
	    = (sec->output_section->vma
	       + sec->output_offset
	       + raw_size
	       + mmix_elf_section_data (sec)->pjs.stubs_size_sum);

	  if ((value & 3) == 0
	      && bfd_check_overflow (complain_overflow_signed,
				     19,
				     0,
				     bfd_arch_bits_per_address (abfd),
				     value - dot
				     - (value > dot
					? mmix_elf_section_data (sec)
					->pjs.stub_size[pjsno]
					: 0))
	      == bfd_reloc_ok)
	    /* If the reloc fits, no stub is needed.  */
	    mmix_elf_section_data (sec)->pjs.stub_size[pjsno] = 0;
	  else
	    /* Maybe we can get away with just a JMP insn?  */
	    if ((value & 3) == 0
		&& bfd_check_overflow (complain_overflow_signed,
				       27,
				       0,
				       bfd_arch_bits_per_address (abfd),
				       value - stubaddr
				       - (value > dot
					  ? mmix_elf_section_data (sec)
					  ->pjs.stub_size[pjsno] - 4
					  : 0))
		== bfd_reloc_ok)
	      /* Yep, account for a stub consisting of a single JMP insn.  */
	      mmix_elf_section_data (sec)->pjs.stub_size[pjsno] = 4;
	  else
	    /* Nope, go for the full insn stub.  It doesn't seem useful to
	       emit the intermediate sizes; those will only be useful for
	       a >64M program assuming contiguous code.  */
	    mmix_elf_section_data (sec)->pjs.stub_size[pjsno]
	      = MAX_PUSHJ_STUB_SIZE;

	  mmix_elf_section_data (sec)->pjs.stubs_size_sum
	    += mmix_elf_section_data (sec)->pjs.stub_size[pjsno];
	  pjsno++;
	  continue;
	}

      /* We're looking at a R_MMIX_BASE_PLUS_OFFSET reloc.  */

      gregdata->reloc_request[gregdata->bpo_reloc_indexes[bpono]].value
	= symval + irel->r_addend;
      gregdata->reloc_request[gregdata->bpo_reloc_indexes[bpono++]].valid = TRUE;
      gregdata->n_remaining_bpo_relocs_this_relaxation_round--;
    }

  /* Check if that was the last BPO-reloc.  If so, sort the values and
     calculate how many registers we need to cover them.  Set the size of
     the linker gregs, and if the number of registers changed, indicate
     that we need to relax some more because we have more work to do.  */
  if (gregdata != NULL
      && gregdata->n_remaining_bpo_relocs_this_relaxation_round == 0)
    {
      size_t i;
      bfd_vma prev_base;
      size_t regindex;

      /* First, reset the remaining relocs for the next round.  */
      gregdata->n_remaining_bpo_relocs_this_relaxation_round
	= gregdata->n_bpo_relocs;

      qsort ((PTR) gregdata->reloc_request,
	     gregdata->n_max_bpo_relocs,
	     sizeof (struct bpo_reloc_request),
	     bpo_reloc_request_sort_fn);

      /* Recalculate indexes.  When we find a change (however unlikely
	 after the initial iteration), we know we need to relax again,
	 since items in the GREG-array are sorted by increasing value and
	 stored in the relaxation phase.  */
      for (i = 0; i < gregdata->n_max_bpo_relocs; i++)
	if (gregdata->bpo_reloc_indexes[gregdata->reloc_request[i].bpo_reloc_no]
	    != i)
	  {
	    gregdata->bpo_reloc_indexes[gregdata->reloc_request[i].bpo_reloc_no]
	      = i;
	    *again = TRUE;
	  }

      /* Allocate register numbers (indexing from 0).  Stop at the first
	 non-valid reloc.  */
      for (i = 0, regindex = 0, prev_base = gregdata->reloc_request[0].value;
	   i < gregdata->n_bpo_relocs;
	   i++)
	{
	  if (gregdata->reloc_request[i].value > prev_base + 255)
	    {
	      regindex++;
	      prev_base = gregdata->reloc_request[i].value;
	    }
	  gregdata->reloc_request[i].regindex = regindex;
	  gregdata->reloc_request[i].offset
	    = gregdata->reloc_request[i].value - prev_base;
	}

      /* If it's not the same as the last time, we need to relax again,
	 because the size of the section has changed.  I'm not sure we
	 actually need to do any adjustments since the shrinking happens
	 at the start of this section, but better safe than sorry.  */
      if (gregdata->n_allocated_bpo_gregs != regindex + 1)
	{
	  gregdata->n_allocated_bpo_gregs = regindex + 1;
	  *again = TRUE;
	}

      bpo_gregs_section->_cooked_size = (regindex + 1) * 8;
    }

  if (isymbuf != NULL && (unsigned char *) isymbuf != symtab_hdr->contents)
    {
      if (! link_info->keep_memory)
	free (isymbuf);
      else
	{
	  /* Cache the symbols for elf_link_input_bfd.  */
	  symtab_hdr->contents = (unsigned char *) isymbuf;
	}
    }

  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  if (sec->_cooked_size
      < raw_size + mmix_elf_section_data (sec)->pjs.stubs_size_sum)
    abort ();

  if (sec->_cooked_size
      > raw_size + mmix_elf_section_data (sec)->pjs.stubs_size_sum)
    {
      sec->_cooked_size
	= raw_size + mmix_elf_section_data (sec)->pjs.stubs_size_sum;
      *again = TRUE;
    }

  return TRUE;

 error_return:
  if (isymbuf != NULL && (unsigned char *) isymbuf != symtab_hdr->contents)
    free (isymbuf);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);
  return FALSE;
}

/* Because we set _raw_size to include the max size of pushj stubs,
   i.e. larger than the actual section input size (see
   mmix_set_relaxable_raw_size), we have to take care of that when reading
   the section.  */

static bfd_boolean
mmix_elf_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     void *location;
     file_ptr offset;
     bfd_size_type count;
{
  bfd_size_type raw_size
    = (section->_raw_size
       - mmix_elf_section_data (section)->pjs.n_pushj_relocs
       * MAX_PUSHJ_STUB_SIZE);

  if (offset + count > section->_raw_size)
    {
      abort();
      bfd_set_error (bfd_error_invalid_operation);
      return FALSE;
    }

  /* Check bounds against the faked raw_size.  */
  if (offset + count > raw_size)
    {
      /* Clear the part in the faked area.  */
      memset (location + raw_size - offset, 0, count - (raw_size - offset));

      /* If there's no initial part within the "real" contents, we're
         done.  */
      if ((bfd_size_type) offset >= raw_size)
	return TRUE;

      /* Else adjust the count and fall through to call the generic
         function.  */
      count = raw_size - offset;
    }

  return
    _bfd_generic_get_section_contents (abfd, section, location, offset,
				       count);
}


#define ELF_ARCH		bfd_arch_mmix
#define ELF_MACHINE_CODE 	EM_MMIX

/* According to mmix-doc page 36 (paragraph 45), this should be (1LL << 48LL).
   However, that's too much for something somewhere in the linker part of
   BFD; perhaps the start-address has to be a non-zero multiple of this
   number, or larger than this number.  The symptom is that the linker
   complains: "warning: allocated section `.text' not in segment".  We
   settle for 64k; the page-size used in examples is 8k.
   #define ELF_MAXPAGESIZE 0x10000

   Unfortunately, this causes excessive padding in the supposedly small
   for-education programs that are the expected usage (where people would
   inspect output).  We stick to 256 bytes just to have *some* default
   alignment.  */
#define ELF_MAXPAGESIZE 0x100

#define TARGET_BIG_SYM		bfd_elf64_mmix_vec
#define TARGET_BIG_NAME		"elf64-mmix"

#define elf_info_to_howto_rel		NULL
#define elf_info_to_howto		mmix_info_to_howto_rela
#define elf_backend_relocate_section	mmix_elf_relocate_section
#define elf_backend_gc_mark_hook	mmix_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook	mmix_elf_gc_sweep_hook

#define elf_backend_link_output_symbol_hook \
	mmix_elf_link_output_symbol_hook
#define elf_backend_add_symbol_hook	mmix_elf_add_symbol_hook

#define elf_backend_check_relocs	mmix_elf_check_relocs
#define elf_backend_symbol_processing	mmix_elf_symbol_processing

#define bfd_elf64_bfd_is_local_label_name \
	mmix_elf_is_local_label_name

#define elf_backend_may_use_rel_p	0
#define elf_backend_may_use_rela_p	1
#define elf_backend_default_use_rela_p	1

#define elf_backend_can_gc_sections	1
#define elf_backend_section_from_bfd_section \
	mmix_elf_section_from_bfd_section

#define bfd_elf64_new_section_hook	mmix_elf_new_section_hook
#define bfd_elf64_bfd_final_link	mmix_elf_final_link
#define bfd_elf64_bfd_relax_section	mmix_elf_relax_section
#define bfd_elf64_get_section_contents	mmix_elf_get_section_contents

#include "elf64-target.h"
