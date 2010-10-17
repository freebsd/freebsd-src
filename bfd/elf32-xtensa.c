/* Xtensa-specific support for 32-bit ELF.
   Copyright 2003, 2004 Free Software Foundation, Inc.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"

#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <strings.h>

#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/xtensa.h"
#include "xtensa-isa.h"
#include "xtensa-config.h"

/* Main interface functions.  */
static void elf_xtensa_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static reloc_howto_type *elf_xtensa_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
extern int xtensa_read_table_entries
  PARAMS ((bfd *, asection *, property_table_entry **, const char *));
static bfd_boolean elf_xtensa_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static void elf_xtensa_hide_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *, bfd_boolean));
static asection *elf_xtensa_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static bfd_boolean elf_xtensa_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_boolean elf_xtensa_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean elf_xtensa_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static bfd_boolean elf_xtensa_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean elf_xtensa_modify_segment_map
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean elf_xtensa_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static bfd_boolean elf_xtensa_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, bfd_boolean *again));
static bfd_boolean elf_xtensa_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static bfd_boolean elf_xtensa_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean elf_xtensa_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_boolean elf_xtensa_set_private_flags
  PARAMS ((bfd *, flagword));
extern flagword elf_xtensa_get_private_bfd_flags
  PARAMS ((bfd *));
static bfd_boolean elf_xtensa_print_private_bfd_data
  PARAMS ((bfd *, PTR));
static bfd_boolean elf_xtensa_object_p
  PARAMS ((bfd *));
static void elf_xtensa_final_write_processing
  PARAMS ((bfd *, bfd_boolean));
static enum elf_reloc_type_class elf_xtensa_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));
static bfd_boolean elf_xtensa_discard_info
  PARAMS ((bfd *, struct elf_reloc_cookie *, struct bfd_link_info *));
static bfd_boolean elf_xtensa_ignore_discarded_relocs
  PARAMS ((asection *));
static bfd_boolean elf_xtensa_grok_prstatus
  PARAMS ((bfd *, Elf_Internal_Note *));
static bfd_boolean elf_xtensa_grok_psinfo
  PARAMS ((bfd *, Elf_Internal_Note *));
static bfd_boolean elf_xtensa_new_section_hook
  PARAMS ((bfd *, asection *));


/* Local helper functions.  */

static bfd_boolean xtensa_elf_dynamic_symbol_p
  PARAMS ((struct elf_link_hash_entry *, struct bfd_link_info *));
static int property_table_compare
  PARAMS ((const PTR, const PTR));
static bfd_boolean elf_xtensa_in_literal_pool
  PARAMS ((property_table_entry *, int, bfd_vma));
static void elf_xtensa_make_sym_local
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static bfd_boolean add_extra_plt_sections
  PARAMS ((bfd *, int));
static bfd_boolean elf_xtensa_fix_refcounts
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean elf_xtensa_allocate_plt_size
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean elf_xtensa_allocate_got_size
  PARAMS ((struct elf_link_hash_entry *, PTR));
static void elf_xtensa_allocate_local_got_size
  PARAMS ((struct bfd_link_info *, asection *));
static bfd_reloc_status_type elf_xtensa_do_reloc
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_vma, bfd_byte *,
	   bfd_vma, bfd_boolean, char **));
static char * vsprint_msg
  VPARAMS ((const char *, const char *, int, ...));
static char *build_encoding_error_message
  PARAMS ((xtensa_opcode, xtensa_encode_result));
static bfd_reloc_status_type bfd_elf_xtensa_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static void do_fix_for_relocatable_link
  PARAMS ((Elf_Internal_Rela *, bfd *, asection *));
static void do_fix_for_final_link
  PARAMS ((Elf_Internal_Rela *, asection *, bfd_vma *));
static bfd_vma elf_xtensa_create_plt_entry
  PARAMS ((bfd *, bfd *, unsigned));
static int elf_xtensa_combine_prop_entries
  PARAMS ((bfd *, asection *, asection *));
static bfd_boolean elf_xtensa_discard_info_for_section
  PARAMS ((bfd *, struct elf_reloc_cookie *, struct bfd_link_info *,
	   asection *));

/* Local functions to handle Xtensa configurability.  */

static void init_call_opcodes
  PARAMS ((void));
static bfd_boolean is_indirect_call_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_direct_call_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_windowed_call_opcode
  PARAMS ((xtensa_opcode));
static xtensa_opcode get_l32r_opcode
  PARAMS ((void));
static bfd_vma l32r_offset
  PARAMS ((bfd_vma, bfd_vma));
static int get_relocation_opnd
  PARAMS ((Elf_Internal_Rela *));
static xtensa_opcode get_relocation_opcode
  PARAMS ((asection *, bfd_byte *, Elf_Internal_Rela *));
static bfd_boolean is_l32r_relocation
  PARAMS ((asection *, bfd_byte *, Elf_Internal_Rela *));

/* Functions for link-time code simplifications.  */

static bfd_reloc_status_type elf_xtensa_do_asm_simplify 
  PARAMS ((bfd_byte *, bfd_vma, bfd_vma));
static bfd_reloc_status_type contract_asm_expansion
  PARAMS ((bfd_byte *, bfd_vma, Elf_Internal_Rela *));
static xtensa_opcode swap_callx_for_call_opcode
  PARAMS ((xtensa_opcode));
static xtensa_opcode get_expanded_call_opcode
  PARAMS ((bfd_byte *, int));

/* Access to internal relocations, section contents and symbols.  */

static Elf_Internal_Rela *retrieve_internal_relocs
  PARAMS ((bfd *, asection *, bfd_boolean));
static void pin_internal_relocs
  PARAMS ((asection *, Elf_Internal_Rela *));
static void release_internal_relocs
  PARAMS ((asection *, Elf_Internal_Rela *));
static bfd_byte *retrieve_contents
  PARAMS ((bfd *, asection *, bfd_boolean));
static void pin_contents
  PARAMS ((asection *, bfd_byte *));
static void release_contents
  PARAMS ((asection *, bfd_byte *));
static Elf_Internal_Sym *retrieve_local_syms
  PARAMS ((bfd *));

/* Miscellaneous utility functions.  */

static asection *elf_xtensa_get_plt_section
  PARAMS ((bfd *, int));
static asection *elf_xtensa_get_gotplt_section
  PARAMS ((bfd *, int));
static asection *get_elf_r_symndx_section
  PARAMS ((bfd *, unsigned long));
static struct elf_link_hash_entry *get_elf_r_symndx_hash_entry
  PARAMS ((bfd *, unsigned long));
static bfd_vma get_elf_r_symndx_offset
  PARAMS ((bfd *, unsigned long));
static bfd_boolean pcrel_reloc_fits
  PARAMS ((xtensa_operand, bfd_vma, bfd_vma));
static bfd_boolean xtensa_is_property_section
  PARAMS ((asection *));
static bfd_boolean xtensa_is_littable_section
  PARAMS ((asection *));
static bfd_boolean is_literal_section
  PARAMS ((asection *));
static int internal_reloc_compare
  PARAMS ((const PTR, const PTR));
extern char *xtensa_get_property_section_name
  PARAMS ((asection *, const char *));

/* Other functions called directly by the linker.  */

typedef void (*deps_callback_t)
  PARAMS ((asection *, bfd_vma, asection *, bfd_vma, PTR));
extern bfd_boolean xtensa_callback_required_dependence
  PARAMS ((bfd *, asection *, struct bfd_link_info *,
	   deps_callback_t, PTR));


typedef struct xtensa_relax_info_struct xtensa_relax_info;


/* Total count of PLT relocations seen during check_relocs.
   The actual PLT code must be split into multiple sections and all
   the sections have to be created before size_dynamic_sections,
   where we figure out the exact number of PLT entries that will be
   needed.  It is OK if this count is an overestimate, e.g., some
   relocations may be removed by GC.  */

static int plt_reloc_count = 0;


/* When this is true, relocations may have been modified to refer to
   symbols from other input files.  The per-section list of "fix"
   records needs to be checked when resolving relocations.  */

static bfd_boolean relaxing_section = FALSE;


static reloc_howto_type elf_howto_table[] =
{
  HOWTO (R_XTENSA_NONE, 0, 0, 0, FALSE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_NONE",
	 FALSE, 0x00000000, 0x00000000, FALSE),
  HOWTO (R_XTENSA_32, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_xtensa_reloc, "R_XTENSA_32",
	 TRUE, 0xffffffff, 0xffffffff, FALSE),
  /* Replace a 32-bit value with a value from the runtime linker (only
     used by linker-generated stub functions).  The r_addend value is
     special: 1 means to substitute a pointer to the runtime linker's
     dynamic resolver function; 2 means to substitute the link map for
     the shared object.  */
  HOWTO (R_XTENSA_RTLD, 0, 2, 32, FALSE, 0, complain_overflow_dont,
	 NULL, "R_XTENSA_RTLD",
	 FALSE, 0x00000000, 0x00000000, FALSE),
  HOWTO (R_XTENSA_GLOB_DAT, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_generic_reloc, "R_XTENSA_GLOB_DAT",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_XTENSA_JMP_SLOT, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_generic_reloc, "R_XTENSA_JMP_SLOT",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_XTENSA_RELATIVE, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_generic_reloc, "R_XTENSA_RELATIVE",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_XTENSA_PLT, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_xtensa_reloc, "R_XTENSA_PLT",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  EMPTY_HOWTO (7),
  HOWTO (R_XTENSA_OP0, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_OP0",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_OP1, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_OP1",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_OP2, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_OP2",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  /* Assembly auto-expansion.  */
  HOWTO (R_XTENSA_ASM_EXPAND, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_ASM_EXPAND",
	 FALSE, 0x00000000, 0x00000000, FALSE),
  /* Relax assembly auto-expansion.  */
  HOWTO (R_XTENSA_ASM_SIMPLIFY, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_ASM_SIMPLIFY",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  EMPTY_HOWTO (13),
  EMPTY_HOWTO (14),
  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_XTENSA_GNU_VTINHERIT, 0, 2, 0, FALSE, 0, complain_overflow_dont,
         NULL, "R_XTENSA_GNU_VTINHERIT",
	 FALSE, 0x00000000, 0x00000000, FALSE),
  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_XTENSA_GNU_VTENTRY, 0, 2, 0, FALSE, 0, complain_overflow_dont,
         _bfd_elf_rel_vtable_reloc_fn, "R_XTENSA_GNU_VTENTRY",
	 FALSE, 0x00000000, 0x00000000, FALSE)
};

#ifdef DEBUG_GEN_RELOC
#define TRACE(str) \
  fprintf (stderr, "Xtensa bfd reloc lookup %d (%s)\n", code, str)
#else
#define TRACE(str)
#endif

static reloc_howto_type *
elf_xtensa_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_NONE:
      TRACE ("BFD_RELOC_NONE");
      return &elf_howto_table[(unsigned) R_XTENSA_NONE ];

    case BFD_RELOC_32:
      TRACE ("BFD_RELOC_32");
      return &elf_howto_table[(unsigned) R_XTENSA_32 ];

    case BFD_RELOC_XTENSA_RTLD:
      TRACE ("BFD_RELOC_XTENSA_RTLD");
      return &elf_howto_table[(unsigned) R_XTENSA_RTLD ];

    case BFD_RELOC_XTENSA_GLOB_DAT:
      TRACE ("BFD_RELOC_XTENSA_GLOB_DAT");
      return &elf_howto_table[(unsigned) R_XTENSA_GLOB_DAT ];

    case BFD_RELOC_XTENSA_JMP_SLOT:
      TRACE ("BFD_RELOC_XTENSA_JMP_SLOT");
      return &elf_howto_table[(unsigned) R_XTENSA_JMP_SLOT ];

    case BFD_RELOC_XTENSA_RELATIVE:
      TRACE ("BFD_RELOC_XTENSA_RELATIVE");
      return &elf_howto_table[(unsigned) R_XTENSA_RELATIVE ];

    case BFD_RELOC_XTENSA_PLT:
      TRACE ("BFD_RELOC_XTENSA_PLT");
      return &elf_howto_table[(unsigned) R_XTENSA_PLT ];

    case BFD_RELOC_XTENSA_OP0:
      TRACE ("BFD_RELOC_XTENSA_OP0");
      return &elf_howto_table[(unsigned) R_XTENSA_OP0 ];

    case BFD_RELOC_XTENSA_OP1:
      TRACE ("BFD_RELOC_XTENSA_OP1");
      return &elf_howto_table[(unsigned) R_XTENSA_OP1 ];

    case BFD_RELOC_XTENSA_OP2:
      TRACE ("BFD_RELOC_XTENSA_OP2");
      return &elf_howto_table[(unsigned) R_XTENSA_OP2 ];

    case BFD_RELOC_XTENSA_ASM_EXPAND:
      TRACE ("BFD_RELOC_XTENSA_ASM_EXPAND");
      return &elf_howto_table[(unsigned) R_XTENSA_ASM_EXPAND ];

    case BFD_RELOC_XTENSA_ASM_SIMPLIFY:
      TRACE ("BFD_RELOC_XTENSA_ASM_SIMPLIFY");
      return &elf_howto_table[(unsigned) R_XTENSA_ASM_SIMPLIFY ];

    case BFD_RELOC_VTABLE_INHERIT:
      TRACE ("BFD_RELOC_VTABLE_INHERIT");
      return &elf_howto_table[(unsigned) R_XTENSA_GNU_VTINHERIT ];

    case BFD_RELOC_VTABLE_ENTRY:
      TRACE ("BFD_RELOC_VTABLE_ENTRY");
      return &elf_howto_table[(unsigned) R_XTENSA_GNU_VTENTRY ];

    default:
      break;
    }

  TRACE ("Unknown");
  return NULL;
}


/* Given an ELF "rela" relocation, find the corresponding howto and record
   it in the BFD internal arelent representation of the relocation.  */

static void
elf_xtensa_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r_type = ELF32_R_TYPE (dst->r_info);

  BFD_ASSERT (r_type < (unsigned int) R_XTENSA_max);
  cache_ptr->howto = &elf_howto_table[r_type];
}


/* Functions for the Xtensa ELF linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/lib/ld.so"

/* The size in bytes of an entry in the procedure linkage table.
   (This does _not_ include the space for the literals associated with
   the PLT entry.) */

#define PLT_ENTRY_SIZE 16

/* For _really_ large PLTs, we may need to alternate between literals
   and code to keep the literals within the 256K range of the L32R
   instructions in the code.  It's unlikely that anyone would ever need
   such a big PLT, but an arbitrary limit on the PLT size would be bad.
   Thus, we split the PLT into chunks.  Since there's very little
   overhead (2 extra literals) for each chunk, the chunk size is kept
   small so that the code for handling multiple chunks get used and
   tested regularly.  With 254 entries, there are 1K of literals for
   each chunk, and that seems like a nice round number.  */

#define PLT_ENTRIES_PER_CHUNK 254

/* PLT entries are actually used as stub functions for lazy symbol
   resolution.  Once the symbol is resolved, the stub function is never
   invoked.  Note: the 32-byte frame size used here cannot be changed
   without a corresponding change in the runtime linker.  */

static const bfd_byte elf_xtensa_be_plt_entry[PLT_ENTRY_SIZE] =
{
  0x6c, 0x10, 0x04,	/* entry sp, 32 */
  0x18, 0x00, 0x00,	/* l32r  a8, [got entry for rtld's resolver] */
  0x1a, 0x00, 0x00,	/* l32r  a10, [got entry for rtld's link map] */
  0x1b, 0x00, 0x00,	/* l32r  a11, [literal for reloc index] */
  0x0a, 0x80, 0x00,	/* jx    a8 */
  0			/* unused */
};

static const bfd_byte elf_xtensa_le_plt_entry[PLT_ENTRY_SIZE] =
{
  0x36, 0x41, 0x00,	/* entry sp, 32 */
  0x81, 0x00, 0x00,	/* l32r  a8, [got entry for rtld's resolver] */
  0xa1, 0x00, 0x00,	/* l32r  a10, [got entry for rtld's link map] */
  0xb1, 0x00, 0x00,	/* l32r  a11, [literal for reloc index] */
  0xa0, 0x08, 0x00,	/* jx    a8 */
  0			/* unused */
};


static inline bfd_boolean
xtensa_elf_dynamic_symbol_p (h, info)
     struct elf_link_hash_entry *h;
     struct bfd_link_info *info;
{
  /* Check if we should do dynamic things to this symbol.  The
     "ignore_protected" argument need not be set, because Xtensa code
     does not require special handling of STV_PROTECTED to make function
     pointer comparisons work properly.  The PLT addresses are never
     used for function pointers.  */

  return _bfd_elf_dynamic_symbol_p (h, info, 0);
}


static int
property_table_compare (ap, bp)
     const PTR ap;
     const PTR bp;
{
  const property_table_entry *a = (const property_table_entry *) ap;
  const property_table_entry *b = (const property_table_entry *) bp;

  /* Check if one entry overlaps with the other; this shouldn't happen
     except when searching for a match.  */
  if ((b->address >= a->address && b->address < (a->address + a->size))
      || (a->address >= b->address && a->address < (b->address + b->size)))
    return 0;

  return (a->address - b->address);
}


/* Get the literal table or instruction table entries for the given
   section.  Sets TABLE_P and returns the number of entries.  On error,
   returns a negative value.  */

int
xtensa_read_table_entries (abfd, section, table_p, sec_name)
     bfd *abfd;
     asection *section;
     property_table_entry **table_p;
     const char *sec_name;
{
  asection *table_section;
  char *table_section_name;
  bfd_size_type table_size = 0;
  bfd_byte *table_data;
  property_table_entry *blocks;
  int block_count;
  bfd_size_type num_records;
  Elf_Internal_Rela *internal_relocs;
  bfd_vma section_addr;

  table_section_name = 
    xtensa_get_property_section_name (section, sec_name);
  table_section = bfd_get_section_by_name (abfd, table_section_name);
  free (table_section_name);
  if (table_section != NULL)
    table_size = (table_section->_cooked_size
		  ? table_section->_cooked_size : table_section->_raw_size);
  
  if (table_size == 0) 
    {
      *table_p = NULL;
      return 0;
    }

  num_records = table_size / 8;
  table_data = retrieve_contents (abfd, table_section, TRUE);
  blocks = (property_table_entry *)
    bfd_malloc (num_records * sizeof (property_table_entry));
  block_count = 0;
  
  section_addr = section->output_section->vma + section->output_offset;

  /* If the file has not yet been relocated, process the relocations
     and sort out the table entries that apply to the specified section.  */
  internal_relocs = retrieve_internal_relocs (abfd, table_section, TRUE);
  if (internal_relocs && !table_section->reloc_done)
    {
      unsigned i;

      for (i = 0; i < table_section->reloc_count; i++)
	{
	  Elf_Internal_Rela *rel = &internal_relocs[i];
	  unsigned long r_symndx;

	  if (ELF32_R_TYPE (rel->r_info) == R_XTENSA_NONE)
	    continue;

	  BFD_ASSERT (ELF32_R_TYPE (rel->r_info) == R_XTENSA_32);
	  r_symndx = ELF32_R_SYM (rel->r_info);

	  if (get_elf_r_symndx_section (abfd, r_symndx) == section)
	    {
	      bfd_vma sym_off = get_elf_r_symndx_offset (abfd, r_symndx);
	      blocks[block_count].address =
		(section_addr + sym_off + rel->r_addend
		 + bfd_get_32 (abfd, table_data + rel->r_offset));
	      blocks[block_count].size =
		bfd_get_32 (abfd, table_data + rel->r_offset + 4);
	      block_count++;
	    }
	}
    }
  else
    {
      /* The file has already been relocated and the addresses are
	 already in the table.  */
      bfd_vma off;

      for (off = 0; off < table_size; off += 8) 
	{
	  bfd_vma address = bfd_get_32 (abfd, table_data + off);

	  if (address >= section_addr
	      && address < ( section_addr + section->_raw_size))
	    {
	      blocks[block_count].address = address;
	      blocks[block_count].size =
		bfd_get_32 (abfd, table_data + off + 4);
	      block_count++;
	    }
	}
    }

  release_contents (table_section, table_data);
  release_internal_relocs (table_section, internal_relocs);

  if (block_count > 0) 
    {
      /* Now sort them into address order for easy reference.  */
      qsort (blocks, block_count, sizeof (property_table_entry),
	     property_table_compare);
    }
    
  *table_p = blocks;
  return block_count;
}


static bfd_boolean
elf_xtensa_in_literal_pool (lit_table, lit_table_size, addr)
     property_table_entry *lit_table;
     int lit_table_size;
     bfd_vma addr;
{
  property_table_entry entry;

  if (lit_table_size == 0)
    return FALSE;

  entry.address = addr;
  entry.size = 1;

  if (bsearch (&entry, lit_table, lit_table_size,
	       sizeof (property_table_entry), property_table_compare))
    return TRUE;

  return FALSE;
}


/* Look through the relocs for a section during the first phase, and
   calculate needed space in the dynamic reloc sections.  */

static bfd_boolean
elf_xtensa_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned int r_type;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);

      if (r_symndx >= NUM_SHDR_ENTRIES (symtab_hdr))
	{
	  (*_bfd_error_handler) (_("%s: bad symbol index: %d"),
				 bfd_archive_filename (abfd),
				 r_symndx);
	  return FALSE;
	}

      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}

      switch (r_type)
	{
	case R_XTENSA_32:
	  if (h == NULL)
	    goto local_literal;

	  if ((sec->flags & SEC_ALLOC) != 0)
	    {
	      if (h->got.refcount <= 0)
		h->got.refcount = 1;
	      else
		h->got.refcount += 1;
	    }
	  break;

	case R_XTENSA_PLT:
	  /* If this relocation is against a local symbol, then it's
	     exactly the same as a normal local GOT entry.  */
	  if (h == NULL)
	    goto local_literal;

	  if ((sec->flags & SEC_ALLOC) != 0)
	    {
	      if (h->plt.refcount <= 0)
		{
		  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
		  h->plt.refcount = 1;
		}
	      else
		h->plt.refcount += 1;

	      /* Keep track of the total PLT relocation count even if we
		 don't yet know whether the dynamic sections will be
		 created.  */
	      plt_reloc_count += 1;

	      if (elf_hash_table (info)->dynamic_sections_created)
		{
		  if (!add_extra_plt_sections (elf_hash_table (info)->dynobj,
					       plt_reloc_count))
		    return FALSE;
		}
	    }
	  break;

	local_literal:
	  if ((sec->flags & SEC_ALLOC) != 0)
	    {
	      bfd_signed_vma *local_got_refcounts;

	      /* This is a global offset table entry for a local symbol.  */
	      local_got_refcounts = elf_local_got_refcounts (abfd);
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
	      local_got_refcounts[r_symndx] += 1;
	    }
	  break;

	case R_XTENSA_OP0:
	case R_XTENSA_OP1:
	case R_XTENSA_OP2:
	case R_XTENSA_ASM_EXPAND:
	case R_XTENSA_ASM_SIMPLIFY:
	  /* Nothing to do for these.  */
	  break;

	case R_XTENSA_GNU_VTINHERIT:
	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	  if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return FALSE;
	  break;

	case R_XTENSA_GNU_VTENTRY:
	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	  if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return FALSE;
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}


static void
elf_xtensa_hide_symbol (info, h, force_local)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     bfd_boolean force_local;
{
  /* For a shared link, move the plt refcount to the got refcount to leave
     space for RELATIVE relocs.  */
  elf_xtensa_make_sym_local (info, h);

  _bfd_elf_link_hash_hide_symbol (info, h, force_local);
}


/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
elf_xtensa_gc_mark_hook (sec, info, rel, h, sym)
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
	case R_XTENSA_GNU_VTINHERIT:
	case R_XTENSA_GNU_VTENTRY:
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

/* Update the GOT & PLT entry reference counts
   for the section being removed.  */

static bfd_boolean
elf_xtensa_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;

  if ((sec->flags & SEC_ALLOC) == 0)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    {
      unsigned long r_symndx;
      unsigned int r_type;
      struct elf_link_hash_entry *h = NULL;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx >= symtab_hdr->sh_info)
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      r_type = ELF32_R_TYPE (rel->r_info);
      switch (r_type)
	{
	case R_XTENSA_32:
	  if (h == NULL)
	    goto local_literal;
	  if (h->got.refcount > 0)
	    h->got.refcount--;
	  break;

	case R_XTENSA_PLT:
	  if (h == NULL)
	    goto local_literal;
	  if (h->plt.refcount > 0)
	    h->plt.refcount--;
	  break;

	local_literal:
	  if (local_got_refcounts[r_symndx] > 0)
	    local_got_refcounts[r_symndx] -= 1;
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}


/* Create all the dynamic sections.  */

static bfd_boolean
elf_xtensa_create_dynamic_sections (dynobj, info)
     bfd *dynobj;
     struct bfd_link_info *info;
{
  flagword flags, noalloc_flags;
  asection *s;

  /* First do all the standard stuff.  */
  if (! _bfd_elf_create_dynamic_sections (dynobj, info))
    return FALSE;

  /* Create any extra PLT sections in case check_relocs has already
     been called on all the non-dynamic input files.  */
  if (!add_extra_plt_sections (dynobj, plt_reloc_count))
    return FALSE;

  noalloc_flags = (SEC_HAS_CONTENTS | SEC_IN_MEMORY
		   | SEC_LINKER_CREATED | SEC_READONLY);
  flags = noalloc_flags | SEC_ALLOC | SEC_LOAD;

  /* Mark the ".got.plt" section READONLY.  */
  s = bfd_get_section_by_name (dynobj, ".got.plt");
  if (s == NULL
      || ! bfd_set_section_flags (dynobj, s, flags))
    return FALSE;

  /* Create ".rela.got".  */
  s = bfd_make_section (dynobj, ".rela.got");
  if (s == NULL
      || ! bfd_set_section_flags (dynobj, s, flags)
      || ! bfd_set_section_alignment (dynobj, s, 2))
    return FALSE;

  /* Create ".got.loc" (literal tables for use by dynamic linker).  */
  s = bfd_make_section (dynobj, ".got.loc");
  if (s == NULL
      || ! bfd_set_section_flags (dynobj, s, flags)
      || ! bfd_set_section_alignment (dynobj, s, 2))
    return FALSE;

  /* Create ".xt.lit.plt" (literal table for ".got.plt*").  */
  s = bfd_make_section (dynobj, ".xt.lit.plt");
  if (s == NULL
      || ! bfd_set_section_flags (dynobj, s, noalloc_flags)
      || ! bfd_set_section_alignment (dynobj, s, 2))
    return FALSE;

  return TRUE;
}


static bfd_boolean
add_extra_plt_sections (dynobj, count)
     bfd *dynobj;
     int count;
{
  int chunk;

  /* Iterate over all chunks except 0 which uses the standard ".plt" and
     ".got.plt" sections.  */
  for (chunk = count / PLT_ENTRIES_PER_CHUNK; chunk > 0; chunk--)
    {
      char *sname;
      flagword flags;
      asection *s;

      /* Stop when we find a section has already been created.  */
      if (elf_xtensa_get_plt_section (dynobj, chunk))
	break;

      flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	       | SEC_LINKER_CREATED | SEC_READONLY);

      sname = (char *) bfd_malloc (10);
      sprintf (sname, ".plt.%u", chunk);
      s = bfd_make_section (dynobj, sname);
      if (s == NULL
	  || ! bfd_set_section_flags (dynobj, s, flags | SEC_CODE)
	  || ! bfd_set_section_alignment (dynobj, s, 2))
	return FALSE;

      sname = (char *) bfd_malloc (14);
      sprintf (sname, ".got.plt.%u", chunk);
      s = bfd_make_section (dynobj, sname);
      if (s == NULL
	  || ! bfd_set_section_flags (dynobj, s, flags)
	  || ! bfd_set_section_alignment (dynobj, s, 2))
	return FALSE;
    }

  return TRUE;
}


/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static bfd_boolean
elf_xtensa_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elf_link_hash_entry *h;
{
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

  /* This is a reference to a symbol defined by a dynamic object.  The
     reference must go through the GOT, so there's no need for COPY relocs,
     .dynbss, etc.  */

  return TRUE;
}


static void
elf_xtensa_make_sym_local (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  if (info->shared)
    {
      if (h->plt.refcount > 0)
	{
	  /* Will use RELATIVE relocs instead of JMP_SLOT relocs.  */
	  if (h->got.refcount < 0)
	    h->got.refcount = 0;
	  h->got.refcount += h->plt.refcount;
	  h->plt.refcount = 0;
	}
    }
  else
    {
      /* Don't need any dynamic relocations at all.  */
      h->plt.refcount = 0;
      h->got.refcount = 0;
    }
}


static bfd_boolean
elf_xtensa_fix_refcounts (h, arg)
     struct elf_link_hash_entry *h;
     PTR arg;
{
  struct bfd_link_info *info = (struct bfd_link_info *) arg;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (! xtensa_elf_dynamic_symbol_p (h, info))
    elf_xtensa_make_sym_local (info, h);

  return TRUE;
}


static bfd_boolean
elf_xtensa_allocate_plt_size (h, arg)
     struct elf_link_hash_entry *h;
     PTR arg;
{
  asection *srelplt = (asection *) arg;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->plt.refcount > 0)
    srelplt->_raw_size += (h->plt.refcount * sizeof (Elf32_External_Rela));

  return TRUE;
}


static bfd_boolean
elf_xtensa_allocate_got_size (h, arg)
     struct elf_link_hash_entry *h;
     PTR arg;
{
  asection *srelgot = (asection *) arg;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->got.refcount > 0)
    srelgot->_raw_size += (h->got.refcount * sizeof (Elf32_External_Rela));

  return TRUE;
}


static void
elf_xtensa_allocate_local_got_size (info, srelgot)
     struct bfd_link_info *info;
     asection *srelgot;
{
  bfd *i;

  for (i = info->input_bfds; i; i = i->link_next)
    {
      bfd_signed_vma *local_got_refcounts;
      bfd_size_type j, cnt;
      Elf_Internal_Shdr *symtab_hdr;

      local_got_refcounts = elf_local_got_refcounts (i);
      if (!local_got_refcounts)
	continue;

      symtab_hdr = &elf_tdata (i)->symtab_hdr;
      cnt = symtab_hdr->sh_info;

      for (j = 0; j < cnt; ++j)
	{
	  if (local_got_refcounts[j] > 0)
	    srelgot->_raw_size += (local_got_refcounts[j]
				   * sizeof (Elf32_External_Rela));
	}
    }
}


/* Set the sizes of the dynamic sections.  */

static bfd_boolean
elf_xtensa_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  bfd *dynobj, *abfd;
  asection *s, *srelplt, *splt, *sgotplt, *srelgot, *spltlittbl, *sgotloc;
  bfd_boolean relplt, relgot;
  int plt_entries, plt_chunks, chunk;

  plt_entries = 0;
  plt_chunks = 0;
  srelgot = 0;

  dynobj = elf_hash_table (info)->dynobj;
  if (dynobj == NULL)
    abort ();

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (info->executable)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  if (s == NULL)
	    abort ();
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}

      /* Allocate room for one word in ".got".  */
      s = bfd_get_section_by_name (dynobj, ".got");
      if (s == NULL)
	abort ();
      s->_raw_size = 4;

      /* Adjust refcounts for symbols that we now know are not "dynamic".  */
      elf_link_hash_traverse (elf_hash_table (info),
			      elf_xtensa_fix_refcounts,
			      (PTR) info);

      /* Allocate space in ".rela.got" for literals that reference
	 global symbols.  */
      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
      if (srelgot == NULL)
	abort ();
      elf_link_hash_traverse (elf_hash_table (info),
			      elf_xtensa_allocate_got_size,
			      (PTR) srelgot);

      /* If we are generating a shared object, we also need space in
	 ".rela.got" for R_XTENSA_RELATIVE relocs for literals that
	 reference local symbols.  */
      if (info->shared)
	elf_xtensa_allocate_local_got_size (info, srelgot);

      /* Allocate space in ".rela.plt" for literals that have PLT entries.  */
      srelplt = bfd_get_section_by_name (dynobj, ".rela.plt");
      if (srelplt == NULL)
	abort ();
      elf_link_hash_traverse (elf_hash_table (info),
			      elf_xtensa_allocate_plt_size,
			      (PTR) srelplt);

      /* Allocate space in ".plt" to match the size of ".rela.plt".  For
	 each PLT entry, we need the PLT code plus a 4-byte literal.
	 For each chunk of ".plt", we also need two more 4-byte
	 literals, two corresponding entries in ".rela.got", and an
	 8-byte entry in ".xt.lit.plt".  */
      spltlittbl = bfd_get_section_by_name (dynobj, ".xt.lit.plt");
      if (spltlittbl == NULL)
	abort ();

      plt_entries = srelplt->_raw_size / sizeof (Elf32_External_Rela);
      plt_chunks =
	(plt_entries + PLT_ENTRIES_PER_CHUNK - 1) / PLT_ENTRIES_PER_CHUNK;

      /* Iterate over all the PLT chunks, including any extra sections
	 created earlier because the initial count of PLT relocations
	 was an overestimate.  */
      for (chunk = 0;
	   (splt = elf_xtensa_get_plt_section (dynobj, chunk)) != NULL;
	   chunk++)
	{
	  int chunk_entries;

	  sgotplt = elf_xtensa_get_gotplt_section (dynobj, chunk);
	  if (sgotplt == NULL)
	    abort ();

	  if (chunk < plt_chunks - 1)
	    chunk_entries = PLT_ENTRIES_PER_CHUNK;
	  else if (chunk == plt_chunks - 1)
	    chunk_entries = plt_entries - (chunk * PLT_ENTRIES_PER_CHUNK);
	  else
	    chunk_entries = 0;

	  if (chunk_entries != 0)
	    {
	      sgotplt->_raw_size = 4 * (chunk_entries + 2);
	      splt->_raw_size = PLT_ENTRY_SIZE * chunk_entries;
	      srelgot->_raw_size += 2 * sizeof (Elf32_External_Rela);
	      spltlittbl->_raw_size += 8;
	    }
	  else
	    {
	      sgotplt->_raw_size = 0;
	      splt->_raw_size = 0;
	    }
	}

      /* Allocate space in ".got.loc" to match the total size of all the
	 literal tables.  */
      sgotloc = bfd_get_section_by_name (dynobj, ".got.loc");
      if (sgotloc == NULL)
	abort ();
      sgotloc->_raw_size = spltlittbl->_raw_size;
      for (abfd = info->input_bfds; abfd != NULL; abfd = abfd->link_next)
	{
	  if (abfd->flags & DYNAMIC)
	    continue;
	  for (s = abfd->sections; s != NULL; s = s->next)
	    {
	      if (! elf_discarded_section (s)
		  && xtensa_is_littable_section (s)
		  && s != spltlittbl)
		sgotloc->_raw_size += s->_raw_size;
	    }
	}
    }

  /* Allocate memory for dynamic sections.  */
  relplt = FALSE;
  relgot = FALSE;
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

      if (strncmp (name, ".rela", 5) == 0)
	{
	  if (strcmp (name, ".rela.plt") == 0)
	    relplt = TRUE;
	  else if (strcmp (name, ".rela.got") == 0)
	    relgot = TRUE;

	  /* We use the reloc_count field as a counter if we need
	     to copy relocs into the output file.  */
	  s->reloc_count = 0;
	}
      else if (strncmp (name, ".plt.", 5) == 0
	       || strncmp (name, ".got.plt.", 9) == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* If we don't need this section, strip it from the output
		 file.  We must create the ".plt*" and ".got.plt*"
		 sections in create_dynamic_sections and/or check_relocs
		 based on a conservative estimate of the PLT relocation
		 count, because the sections must be created before the
		 linker maps input sections to output sections.  The
		 linker does that before size_dynamic_sections, where we
		 compute the exact size of the PLT, so there may be more
		 of these sections than are actually needed.  */
	      strip = TRUE;
	    }
	}
      else if (strcmp (name, ".got") != 0
	       && strcmp (name, ".plt") != 0
	       && strcmp (name, ".got.plt") != 0
	       && strcmp (name, ".xt.lit.plt") != 0
	       && strcmp (name, ".got.loc") != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	_bfd_strip_section_from_output (info, s);
      else
	{
	  /* Allocate memory for the section contents.  */
	  s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->_raw_size);
	  if (s->contents == NULL && s->_raw_size != 0)
	    return FALSE;
	}
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add the special XTENSA_RTLD relocations now.  The offsets won't be
	 known until finish_dynamic_sections, but we need to get the relocs
	 in place before they are sorted.  */
      if (srelgot == NULL)
	abort ();
      for (chunk = 0; chunk < plt_chunks; chunk++)
	{
	  Elf_Internal_Rela irela;
	  bfd_byte *loc;

	  irela.r_offset = 0;
	  irela.r_info = ELF32_R_INFO (0, R_XTENSA_RTLD);
	  irela.r_addend = 0;

	  loc = (srelgot->contents
		 + srelgot->reloc_count * sizeof (Elf32_External_Rela));
	  bfd_elf32_swap_reloca_out (output_bfd, &irela, loc);
	  bfd_elf32_swap_reloca_out (output_bfd, &irela,
				     loc + sizeof (Elf32_External_Rela));
	  srelgot->reloc_count += 2;
	}

      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf_xtensa_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  _bfd_elf_add_dynamic_entry (info, TAG, VAL)

      if (! info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return FALSE;
	}

      if (relplt)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return FALSE;
	}

      if (relgot)
	{
	  if (!add_dynamic_entry (DT_RELA, 0)
	      || !add_dynamic_entry (DT_RELASZ, 0)
	      || !add_dynamic_entry (DT_RELAENT, sizeof (Elf32_External_Rela)))
	    return FALSE;
	}

      if (!add_dynamic_entry (DT_XTENSA_GOT_LOC_OFF, 0)
	  || !add_dynamic_entry (DT_XTENSA_GOT_LOC_SZ, 0))
	return FALSE;
    }
#undef add_dynamic_entry

  return TRUE;
}


/* Remove any PT_LOAD segments with no allocated sections.  Prior to
   binutils 2.13, this function used to remove the non-SEC_ALLOC
   sections from PT_LOAD segments, but that task has now been moved
   into elf.c.  We still need this function to remove any empty
   segments that result, but there's nothing Xtensa-specific about
   this and it probably ought to be moved into elf.c as well.  */

static bfd_boolean
elf_xtensa_modify_segment_map (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
{
  struct elf_segment_map **m_p;

  m_p = &elf_tdata (abfd)->segment_map;
  while (*m_p != NULL)
    {
      if ((*m_p)->p_type == PT_LOAD && (*m_p)->count == 0)
	*m_p = (*m_p)->next;
      else
	m_p = &(*m_p)->next;
    }
  return TRUE;
}


/* Perform the specified relocation.  The instruction at (contents + address)
   is modified to set one operand to represent the value in "relocation".  The
   operand position is determined by the relocation type recorded in the
   howto.  */

#define CALL_SEGMENT_BITS (30)
#define CALL_SEGMENT_SIZE (1<<CALL_SEGMENT_BITS)

static bfd_reloc_status_type
elf_xtensa_do_reloc (howto, abfd, input_section, relocation,
		     contents, address, is_weak_undef, error_message)
     reloc_howto_type *howto;
     bfd *abfd;
     asection *input_section;
     bfd_vma relocation;
     bfd_byte *contents;
     bfd_vma address;
     bfd_boolean is_weak_undef;
     char **error_message;
{
  xtensa_opcode opcode;
  xtensa_operand operand;
  xtensa_encode_result encode_result;
  xtensa_isa isa = xtensa_default_isa;
  xtensa_insnbuf ibuff;
  bfd_vma self_address;
  int opnd;
  uint32 newval;

  switch (howto->type)
    {
    case R_XTENSA_NONE:
      return bfd_reloc_ok;

    case R_XTENSA_ASM_EXPAND:
      if (!is_weak_undef)
	{
	  /* Check for windowed CALL across a 1GB boundary.  */
	  xtensa_opcode opcode =
	    get_expanded_call_opcode (contents + address,
				      input_section->_raw_size - address);
	  if (is_windowed_call_opcode (opcode))
	    {
	      self_address = (input_section->output_section->vma
			      + input_section->output_offset
			      + address);
	      if ((self_address >> CALL_SEGMENT_BITS) !=
		  (relocation >> CALL_SEGMENT_BITS)) 
		{
		  *error_message = "windowed longcall crosses 1GB boundary; "
		    "return may fail";
		  return bfd_reloc_dangerous;
		}
	    }
	}
      return bfd_reloc_ok;

    case R_XTENSA_ASM_SIMPLIFY:
      { 
        /* Convert the L32R/CALLX to CALL.  */
	bfd_reloc_status_type retval = 
	  elf_xtensa_do_asm_simplify (contents, address,
				      input_section->_raw_size);
	if (retval != bfd_reloc_ok)
	  return retval;

	/* The CALL needs to be relocated.  Continue below for that part.  */
	address += 3;
	howto = &elf_howto_table[(unsigned) R_XTENSA_OP0 ];
      }
      break;

    case R_XTENSA_32:
    case R_XTENSA_PLT:
      {
	bfd_vma x;
	x = bfd_get_32 (abfd, contents + address);
	x = x + relocation;
	bfd_put_32 (abfd, x, contents + address);
      }
      return bfd_reloc_ok;
    }

  /* Read the instruction into a buffer and decode the opcode.  */
  ibuff = xtensa_insnbuf_alloc (isa);
  xtensa_insnbuf_from_chars (isa, ibuff, contents + address);
  opcode = xtensa_decode_insn (isa, ibuff);

  /* Determine which operand is being relocated.  */
  if (opcode == XTENSA_UNDEFINED)
    {
      *error_message = "cannot decode instruction";
      return bfd_reloc_dangerous;
    }

  if (howto->type < R_XTENSA_OP0 || howto->type > R_XTENSA_OP2)
    {
      *error_message = "unexpected relocation";
      return bfd_reloc_dangerous;
    }

  opnd = howto->type - R_XTENSA_OP0;

  /* Calculate the PC address for this instruction.  */
  if (!howto->pc_relative)
    {
      *error_message = "expected PC-relative relocation";
      return bfd_reloc_dangerous;
    }

  self_address = (input_section->output_section->vma
		  + input_section->output_offset
		  + address);

  /* Apply the relocation.  */
  operand = xtensa_get_operand (isa, opcode, opnd);
  newval = xtensa_operand_do_reloc (operand, relocation, self_address);
  encode_result = xtensa_operand_encode (operand, &newval);
  xtensa_operand_set_field (operand, ibuff, newval);

  /* Write the modified instruction back out of the buffer.  */
  xtensa_insnbuf_to_chars (isa, ibuff, contents + address);
  free (ibuff);

  if (encode_result != xtensa_encode_result_ok)
    {
      char *message = build_encoding_error_message (opcode, encode_result);
      *error_message = message;
      return bfd_reloc_dangerous;
    }

  /* Final check for call.  */
  if (is_direct_call_opcode (opcode)
      && is_windowed_call_opcode (opcode))
    {
      if ((self_address >> CALL_SEGMENT_BITS) !=
	  (relocation >> CALL_SEGMENT_BITS)) 
	{
	  *error_message = "windowed call crosses 1GB boundary; "
	    "return may fail";
	  return bfd_reloc_dangerous;
	}
    }

  return bfd_reloc_ok;
}


static char *
vsprint_msg VPARAMS ((const char *origmsg, const char *fmt, int arglen, ...))
{
  /* To reduce the size of the memory leak,
     we only use a single message buffer.  */
  static bfd_size_type alloc_size = 0;
  static char *message = NULL;
  bfd_size_type orig_len, len = 0;
  bfd_boolean is_append;

  VA_OPEN (ap, arglen);
  VA_FIXEDARG (ap, const char *, origmsg);
  
  is_append = (origmsg == message);  

  orig_len = strlen (origmsg);
  len = orig_len + strlen (fmt) + arglen + 20;
  if (len > alloc_size)
    {
      message = (char *) bfd_realloc (message, len);
      alloc_size = len;
    }
  if (!is_append)
    memcpy (message, origmsg, orig_len);
  vsprintf (message + orig_len, fmt, ap);
  VA_CLOSE (ap);
  return message;
}


static char *
build_encoding_error_message (opcode, encode_result)
     xtensa_opcode opcode;
     xtensa_encode_result encode_result;
{
  const char *opname = xtensa_opcode_name (xtensa_default_isa, opcode);
  const char *msg = NULL;

  switch (encode_result)
    {
    case xtensa_encode_result_ok:
      msg = "unexpected valid encoding";
      break;
    case xtensa_encode_result_align:
      msg = "misaligned encoding";
      break;
    case xtensa_encode_result_not_in_table:
      msg = "encoding not in lookup table";
      break;
    case xtensa_encode_result_too_low:
      msg = "encoding out of range: too low";
      break;
    case xtensa_encode_result_too_high:
      msg = "encoding out of range: too high";
      break;
    case xtensa_encode_result_not_ok:
    default:
      msg = "could not encode";
      break;
    }

  if (is_direct_call_opcode (opcode)
      && (encode_result == xtensa_encode_result_too_low
	  || encode_result == xtensa_encode_result_too_high))

    msg = "direct call out of range";

  else if (opcode == get_l32r_opcode ()) 
    {
      /* L32Rs have the strange interaction with encoding in that they
         have an unsigned immediate field, so libisa returns "too high"
         when the absolute value is out of range and never returns "too
         low", but I leave the "too low" message in case anything
         changes.  */
      if (encode_result == xtensa_encode_result_too_low)
	msg = "literal out of range";
      else if (encode_result == xtensa_encode_result_too_high)
	msg = "literal placed after use";
    }
  
  return vsprint_msg (opname, ": %s", strlen (msg) + 2, msg);
}


/* This function is registered as the "special_function" in the
   Xtensa howto for handling simplify operations.
   bfd_perform_relocation / bfd_install_relocation use it to
   perform (install) the specified relocation.  Since this replaces the code
   in bfd_perform_relocation, it is basically an Xtensa-specific,
   stripped-down version of bfd_perform_relocation.  */

static bfd_reloc_status_type
bfd_elf_xtensa_reloc (abfd, reloc_entry, symbol, data, input_section,
		      output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_vma relocation;
  bfd_reloc_status_type flag;
  bfd_size_type octets = reloc_entry->address * bfd_octets_per_byte (abfd);
  bfd_vma output_base = 0;
  reloc_howto_type *howto = reloc_entry->howto;
  asection *reloc_target_output_section;
  bfd_boolean is_weak_undef;

  /* ELF relocs are against symbols.  If we are producing relocatable
     output, and the reloc is against an external symbol, the resulting
     reloc will also be against the same symbol.  In such a case, we
     don't want to change anything about the way the reloc is handled,
     since it will all be done at final link time.  This test is similar
     to what bfd_elf_generic_reloc does except that it lets relocs with
     howto->partial_inplace go through even if the addend is non-zero.
     (The real problem is that partial_inplace is set for XTENSA_32
     relocs to begin with, but that's a long story and there's little we
     can do about it now....)  */

  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* Is the address of the relocation really within the section?  */
  if (reloc_entry->address > (input_section->_cooked_size
			      / bfd_octets_per_byte (abfd)))
    return bfd_reloc_outofrange;

  /* Work out which section the relocation is targeted at and the
     initial relocation command value.  */

  /* Get symbol value.  (Common symbols are special.)  */
  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  reloc_target_output_section = symbol->section->output_section;

  /* Convert input-section-relative symbol value to absolute.  */
  if ((output_bfd && !howto->partial_inplace)
      || reloc_target_output_section == NULL)
    output_base = 0;
  else
    output_base = reloc_target_output_section->vma;

  relocation += output_base + symbol->section->output_offset;

  /* Add in supplied addend.  */
  relocation += reloc_entry->addend;

  /* Here the variable relocation holds the final address of the
     symbol we are relocating against, plus any addend.  */
  if (output_bfd)
    {
      if (!howto->partial_inplace)
	{
	  /* This is a partial relocation, and we want to apply the relocation
	     to the reloc entry rather than the raw data.  Everything except
	     relocations against section symbols has already been handled
	     above.  */
         
	  BFD_ASSERT (symbol->flags & BSF_SECTION_SYM);
	  reloc_entry->addend = relocation;
	  reloc_entry->address += input_section->output_offset;
	  return bfd_reloc_ok;
	}
      else
	{
	  reloc_entry->address += input_section->output_offset;
	  reloc_entry->addend = 0;
	}
    }

  is_weak_undef = (bfd_is_und_section (symbol->section)
		   && (symbol->flags & BSF_WEAK) != 0);
  flag = elf_xtensa_do_reloc (howto, abfd, input_section, relocation,
			      (bfd_byte *) data, (bfd_vma) octets,
			      is_weak_undef, error_message);

  if (flag == bfd_reloc_dangerous)
    {
      /* Add the symbol name to the error message.  */
      if (! *error_message)
	*error_message = "";
      *error_message = vsprint_msg (*error_message, ": (%s + 0x%lx)",
				    strlen (symbol->name) + 17,
				    symbol->name, reloc_entry->addend);
    }

  return flag;
}


/* Set up an entry in the procedure linkage table.  */

static bfd_vma
elf_xtensa_create_plt_entry (dynobj, output_bfd, reloc_index)
      bfd *dynobj;
      bfd *output_bfd;
      unsigned reloc_index;
{
  asection *splt, *sgotplt;
  bfd_vma plt_base, got_base;
  bfd_vma code_offset, lit_offset;
  int chunk;

  chunk = reloc_index / PLT_ENTRIES_PER_CHUNK;
  splt = elf_xtensa_get_plt_section (dynobj, chunk);
  sgotplt = elf_xtensa_get_gotplt_section (dynobj, chunk);
  BFD_ASSERT (splt != NULL && sgotplt != NULL);

  plt_base = splt->output_section->vma + splt->output_offset;
  got_base = sgotplt->output_section->vma + sgotplt->output_offset;

  lit_offset = 8 + (reloc_index % PLT_ENTRIES_PER_CHUNK) * 4;
  code_offset = (reloc_index % PLT_ENTRIES_PER_CHUNK) * PLT_ENTRY_SIZE;

  /* Fill in the literal entry.  This is the offset of the dynamic
     relocation entry.  */
  bfd_put_32 (output_bfd, reloc_index * sizeof (Elf32_External_Rela),
	      sgotplt->contents + lit_offset);

  /* Fill in the entry in the procedure linkage table.  */
  memcpy (splt->contents + code_offset,
	  (bfd_big_endian (output_bfd)
	   ? elf_xtensa_be_plt_entry
	   : elf_xtensa_le_plt_entry),
	  PLT_ENTRY_SIZE);
  bfd_put_16 (output_bfd, l32r_offset (got_base + 0,
				       plt_base + code_offset + 3),
	      splt->contents + code_offset + 4);
  bfd_put_16 (output_bfd, l32r_offset (got_base + 4,
				       plt_base + code_offset + 6),
	      splt->contents + code_offset + 7);
  bfd_put_16 (output_bfd, l32r_offset (got_base + lit_offset,
				       plt_base + code_offset + 9),
	      splt->contents + code_offset + 10);

  return plt_base + code_offset;
}


/* Relocate an Xtensa ELF section.  This is invoked by the linker for
   both relocatable and final links.  */

static bfd_boolean
elf_xtensa_relocate_section (output_bfd, info, input_bfd,
			     input_section, contents, relocs,
			     local_syms, local_sections)
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
  struct elf_link_hash_entry **sym_hashes;
  asection *srelgot, *srelplt;
  bfd *dynobj;
  property_table_entry *lit_table = 0;
  int ltblsize = 0;
  char *error_message = NULL;

  if (xtensa_default_isa == NULL)
    xtensa_isa_init ();

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  srelgot = NULL;
  srelplt = NULL;
  if (dynobj != NULL)
    {
      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");;
      srelplt = bfd_get_section_by_name (dynobj, ".rela.plt");
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      ltblsize = xtensa_read_table_entries (input_bfd, input_section,
					    &lit_table, XTENSA_LIT_SEC_NAME);
      if (ltblsize < 0)
	return FALSE;
    }

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
      bfd_boolean is_weak_undef;
      bfd_boolean unresolved_reloc;
      bfd_boolean warned;

      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type == (int) R_XTENSA_GNU_VTINHERIT
	  || r_type == (int) R_XTENSA_GNU_VTENTRY)
	continue;

      if (r_type < 0 || r_type >= (int) R_XTENSA_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
      howto = &elf_howto_table[r_type];

      r_symndx = ELF32_R_SYM (rel->r_info);

      if (info->relocatable)
	{
	  /* This is a relocatable link. 
	     1) If the reloc is against a section symbol, adjust
	     according to the output section.
	     2) If there is a new target for this relocation,
	     the new target will be in the same output section.
	     We adjust the relocation by the output section
	     difference.  */

	  if (relaxing_section)
	    {
	      /* Check if this references a section in another input file.  */
	      do_fix_for_relocatable_link (rel, input_bfd, input_section);
	      r_type = ELF32_R_TYPE (rel->r_info);
	    }

	  if (r_type == R_XTENSA_ASM_SIMPLIFY) 
	    {
	      /* Convert ASM_SIMPLIFY into the simpler relocation
		 so that they never escape a relaxing link.  */
	      contract_asm_expansion (contents, input_section->_raw_size, rel);
	      r_type = ELF32_R_TYPE (rel->r_info);
	    }

	  /* This is a relocatable link, so we don't have to change
	     anything unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

	  /* If there is an addend with a partial_inplace howto,
	     then move the addend to the contents.  This is a hack
	     to work around problems with DWARF in relocatable links
	     with some previous version of BFD.  Now we can't easily get
	     rid of the hack without breaking backward compatibility.... */
	  if (rel->r_addend)
	    {
	      howto = &elf_howto_table[r_type];
	      if (howto->partial_inplace)
		{
		  r = elf_xtensa_do_reloc (howto, input_bfd, input_section,
					   rel->r_addend, contents,
					   rel->r_offset, FALSE,
					   &error_message);
		  if (r != bfd_reloc_ok)
		    {
		      if (!((*info->callbacks->reloc_dangerous)
			    (info, error_message, input_bfd, input_section,
			     rel->r_offset)))
			return FALSE;
		    }
		  rel->r_addend = 0;
		}
	    }

	  /* Done with work for relocatable link; continue with next reloc.  */
	  continue;
	}

      /* This is a final link.  */

      h = NULL;
      sym = NULL;
      sec = NULL;
      is_weak_undef = FALSE;
      unresolved_reloc = FALSE;
      warned = FALSE;

      if (howto->partial_inplace)
	{
	  /* Because R_XTENSA_32 was made partial_inplace to fix some
	     problems with DWARF info in partial links, there may be
	     an addend stored in the contents.  Take it out of there
	     and move it back into the addend field of the reloc.  */
	  rel->r_addend += bfd_get_32 (input_bfd, contents + rel->r_offset);
	  bfd_put_32 (input_bfd, 0, contents + rel->r_offset);
	}

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	}
      else
	{
	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);

	  if (relocation == 0
	      && !unresolved_reloc
	      && h->root.type == bfd_link_hash_undefweak)
	    is_weak_undef = TRUE;
	}

      if (relaxing_section)
	{
	  /* Check if this references a section in another input file.  */
	  do_fix_for_final_link (rel, input_section, &relocation);

	  /* Update some already cached values.  */
	  r_type = ELF32_R_TYPE (rel->r_info);
	  howto = &elf_howto_table[r_type];
	}

      /* Sanity check the address.  */
      if (rel->r_offset >= input_section->_raw_size
	  && ELF32_R_TYPE (rel->r_info) != R_XTENSA_NONE)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}

      /* Generate dynamic relocations.  */
      if (elf_hash_table (info)->dynamic_sections_created)
	{
	  bfd_boolean dynamic_symbol = xtensa_elf_dynamic_symbol_p (h, info);

	  if (dynamic_symbol && (r_type == R_XTENSA_OP0
				 || r_type == R_XTENSA_OP1
				 || r_type == R_XTENSA_OP2))
	    {
	      /* This is an error.  The symbol's real value won't be known
		 until runtime and it's likely to be out of range anyway.  */
	      const char *name = h->root.root.string;
	      error_message = vsprint_msg ("invalid relocation for dynamic "
					   "symbol", ": %s",
					   strlen (name) + 2, name);
	      if (!((*info->callbacks->reloc_dangerous)
		    (info, error_message, input_bfd, input_section,
		     rel->r_offset)))
		return FALSE;
	    }
	  else if ((r_type == R_XTENSA_32 || r_type == R_XTENSA_PLT)
		   && (input_section->flags & SEC_ALLOC) != 0
		   && (dynamic_symbol || info->shared))
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;
	      asection *srel;

	      if (dynamic_symbol && r_type == R_XTENSA_PLT)
		srel = srelplt;
	      else
		srel = srelgot;

	      BFD_ASSERT (srel != NULL);

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info,
					 input_section, rel->r_offset);

	      if ((outrel.r_offset | 1) == (bfd_vma) -1)
		memset (&outrel, 0, sizeof outrel);
	      else
		{
		  outrel.r_offset += (input_section->output_section->vma
				      + input_section->output_offset);

		  /* Complain if the relocation is in a read-only section
		     and not in a literal pool.  */
		  if ((input_section->flags & SEC_READONLY) != 0
		      && !elf_xtensa_in_literal_pool (lit_table, ltblsize,
						      outrel.r_offset))
		    {
		      error_message =
			_("dynamic relocation in read-only section");
		      if (!((*info->callbacks->reloc_dangerous)
			    (info, error_message, input_bfd, input_section,
			     rel->r_offset)))
			return FALSE;
		    }

		  if (dynamic_symbol)
		    {
		      outrel.r_addend = rel->r_addend;
		      rel->r_addend = 0;

		      if (r_type == R_XTENSA_32)
			{
			  outrel.r_info =
			    ELF32_R_INFO (h->dynindx, R_XTENSA_GLOB_DAT);
			  relocation = 0;
			}
		      else /* r_type == R_XTENSA_PLT */
			{
			  outrel.r_info =
			    ELF32_R_INFO (h->dynindx, R_XTENSA_JMP_SLOT);

			  /* Create the PLT entry and set the initial
			     contents of the literal entry to the address of
			     the PLT entry.  */
			  relocation = 
			    elf_xtensa_create_plt_entry (dynobj, output_bfd,
							 srel->reloc_count);
			}
		      unresolved_reloc = FALSE;
		    }
		  else
		    {
		      /* Generate a RELATIVE relocation.  */
		      outrel.r_info = ELF32_R_INFO (0, R_XTENSA_RELATIVE);
		      outrel.r_addend = 0;
		    }
		}

	      loc = (srel->contents
		     + srel->reloc_count++ * sizeof (Elf32_External_Rela));
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
	      BFD_ASSERT (sizeof (Elf32_External_Rela) * srel->reloc_count
			  <= srel->_cooked_size);
	    }
	}

      /* Dynamic relocs are not propagated for SEC_DEBUGGING sections
	 because such sections are not SEC_ALLOC and thus ld.so will
	 not process them.  */
      if (unresolved_reloc
	  && !((input_section->flags & SEC_DEBUGGING) != 0
	       && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0))
	(*_bfd_error_handler)
	  (_("%s(%s+0x%lx): unresolvable relocation against symbol `%s'"),
	   bfd_archive_filename (input_bfd),
	   bfd_get_section_name (input_bfd, input_section),
	   (long) rel->r_offset,
	   h->root.root.string);

      /* There's no point in calling bfd_perform_relocation here.
	 Just go directly to our "special function".  */
      r = elf_xtensa_do_reloc (howto, input_bfd, input_section,
			       relocation + rel->r_addend,
			       contents, rel->r_offset, is_weak_undef,
			       &error_message);
      
      if (r != bfd_reloc_ok && !warned)
	{
	  const char *name;

	  BFD_ASSERT (r == bfd_reloc_dangerous);
	  BFD_ASSERT (error_message != (char *) NULL);

	  if (h != NULL)
	    name = h->root.root.string;
	  else
	    {
	      name = bfd_elf_string_from_elf_section
		(input_bfd, symtab_hdr->sh_link, sym->st_name);
	      if (name && *name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }
	  if (name)
	    error_message = vsprint_msg (error_message, ": %s",
					 strlen (name), name);
	  if (!((*info->callbacks->reloc_dangerous)
		(info, error_message, input_bfd, input_section,
		 rel->r_offset)))
	    return FALSE;
	}
    }

  if (lit_table)
    free (lit_table);

  input_section->reloc_done = TRUE;

  return TRUE;
}


/* Finish up dynamic symbol handling.  There's not much to do here since
   the PLT and GOT entries are all set up by relocate_section.  */

static bfd_boolean
elf_xtensa_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    {
      /* Mark the symbol as undefined, rather than as defined in
	 the .plt section.  Leave the value alone.  */
      sym->st_shndx = SHN_UNDEF;
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}


/* Combine adjacent literal table entries in the output.  Adjacent
   entries within each input section may have been removed during
   relaxation, but we repeat the process here, even though it's too late
   to shrink the output section, because it's important to minimize the
   number of literal table entries to reduce the start-up work for the
   runtime linker.  Returns the number of remaining table entries or -1
   on error.  */

static int
elf_xtensa_combine_prop_entries (output_bfd, sxtlit, sgotloc)
     bfd *output_bfd;
     asection *sxtlit;
     asection *sgotloc;
{
  bfd_byte *contents;
  property_table_entry *table;
  bfd_size_type section_size, sgotloc_size;
  bfd_vma offset;
  int n, m, num;

  section_size = (sxtlit->_cooked_size != 0
		  ? sxtlit->_cooked_size : sxtlit->_raw_size);
  BFD_ASSERT (section_size % 8 == 0);
  num = section_size / 8;

  sgotloc_size = (sgotloc->_cooked_size != 0
		  ? sgotloc->_cooked_size : sgotloc->_raw_size);
  if (sgotloc_size != section_size)
    {
      (*_bfd_error_handler)
	("internal inconsistency in size of .got.loc section");
      return -1;
    }

  contents = (bfd_byte *) bfd_malloc (section_size);
  table = (property_table_entry *)
    bfd_malloc (num * sizeof (property_table_entry));
  if (contents == 0 || table == 0)
    return -1;

  /* The ".xt.lit.plt" section has the SEC_IN_MEMORY flag set and this
     propagates to the output section, where it doesn't really apply and
     where it breaks the following call to bfd_get_section_contents.  */
  sxtlit->flags &= ~SEC_IN_MEMORY;

  if (! bfd_get_section_contents (output_bfd, sxtlit, contents, 0,
				  section_size))
    return -1;

  /* There should never be any relocations left at this point, so this
     is quite a bit easier than what is done during relaxation.  */

  /* Copy the raw contents into a property table array and sort it.  */
  offset = 0;
  for (n = 0; n < num; n++)
    {
      table[n].address = bfd_get_32 (output_bfd, &contents[offset]);
      table[n].size = bfd_get_32 (output_bfd, &contents[offset + 4]);
      offset += 8;
    }
  qsort (table, num, sizeof (property_table_entry), property_table_compare);

  for (n = 0; n < num; n++)
    {
      bfd_boolean remove = FALSE;

      if (table[n].size == 0)
	remove = TRUE;
      else if (n > 0 &&
	       (table[n-1].address + table[n-1].size == table[n].address))
	{
	  table[n-1].size += table[n].size;
	  remove = TRUE;
	}

      if (remove)
	{
	  for (m = n; m < num - 1; m++)
	    {
	      table[m].address = table[m+1].address;
	      table[m].size = table[m+1].size;
	    }

	  n--;
	  num--;
	}
    }

  /* Copy the data back to the raw contents.  */
  offset = 0;
  for (n = 0; n < num; n++)
    {
      bfd_put_32 (output_bfd, table[n].address, &contents[offset]);
      bfd_put_32 (output_bfd, table[n].size, &contents[offset + 4]);
      offset += 8;
    }

  /* Clear the removed bytes.  */
  if ((bfd_size_type) (num * 8) < section_size)
    {
      memset (&contents[num * 8], 0, section_size - num * 8);
      sxtlit->_cooked_size = num * 8;
    }

  if (! bfd_set_section_contents (output_bfd, sxtlit, contents, 0,
				  section_size))
    return -1;

  /* Copy the contents to ".got.loc".  */
  memcpy (sgotloc->contents, contents, section_size);

  free (contents);
  free (table);
  return num;
}


/* Finish up the dynamic sections.  */

static bfd_boolean
elf_xtensa_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn, *srelplt, *sgot, *sxtlit, *sgotloc;
  Elf32_External_Dyn *dyncon, *dynconend;
  int num_xtlit_entries;

  if (! elf_hash_table (info)->dynamic_sections_created)
    return TRUE;

  dynobj = elf_hash_table (info)->dynobj;
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");
  BFD_ASSERT (sdyn != NULL);

  /* Set the first entry in the global offset table to the address of
     the dynamic section.  */
  sgot = bfd_get_section_by_name (dynobj, ".got");
  if (sgot)
    {
      BFD_ASSERT (sgot->_raw_size == 4);
      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);
    }

  srelplt = bfd_get_section_by_name (dynobj, ".rela.plt");
  if (srelplt != NULL && srelplt->_raw_size != 0)
    {
      asection *sgotplt, *srelgot, *spltlittbl;
      int chunk, plt_chunks, plt_entries;
      Elf_Internal_Rela irela;
      bfd_byte *loc;
      unsigned rtld_reloc;

      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");;
      BFD_ASSERT (srelgot != NULL);

      spltlittbl = bfd_get_section_by_name (dynobj, ".xt.lit.plt");
      BFD_ASSERT (spltlittbl != NULL);

      /* Find the first XTENSA_RTLD relocation.  Presumably the rest
	 of them follow immediately after....  */
      for (rtld_reloc = 0; rtld_reloc < srelgot->reloc_count; rtld_reloc++)
	{
	  loc = srelgot->contents + rtld_reloc * sizeof (Elf32_External_Rela);
	  bfd_elf32_swap_reloca_in (output_bfd, loc, &irela);
	  if (ELF32_R_TYPE (irela.r_info) == R_XTENSA_RTLD)
	    break;
	}
      BFD_ASSERT (rtld_reloc < srelgot->reloc_count);

      plt_entries = (srelplt->_raw_size / sizeof (Elf32_External_Rela));
      plt_chunks =
	(plt_entries + PLT_ENTRIES_PER_CHUNK - 1) / PLT_ENTRIES_PER_CHUNK;

      for (chunk = 0; chunk < plt_chunks; chunk++)
	{
	  int chunk_entries = 0;

	  sgotplt = elf_xtensa_get_gotplt_section (dynobj, chunk);
	  BFD_ASSERT (sgotplt != NULL);

	  /* Emit special RTLD relocations for the first two entries in
	     each chunk of the .got.plt section.  */

	  loc = srelgot->contents + rtld_reloc * sizeof (Elf32_External_Rela);
	  bfd_elf32_swap_reloca_in (output_bfd, loc, &irela);
	  BFD_ASSERT (ELF32_R_TYPE (irela.r_info) == R_XTENSA_RTLD);
	  irela.r_offset = (sgotplt->output_section->vma
			    + sgotplt->output_offset);
	  irela.r_addend = 1; /* tell rtld to set value to resolver function */
	  bfd_elf32_swap_reloca_out (output_bfd, &irela, loc);
	  rtld_reloc += 1;
	  BFD_ASSERT (rtld_reloc <= srelgot->reloc_count);

	  /* Next literal immediately follows the first.  */
	  loc += sizeof (Elf32_External_Rela);
	  bfd_elf32_swap_reloca_in (output_bfd, loc, &irela);
	  BFD_ASSERT (ELF32_R_TYPE (irela.r_info) == R_XTENSA_RTLD);
	  irela.r_offset = (sgotplt->output_section->vma
			    + sgotplt->output_offset + 4);
	  /* Tell rtld to set value to object's link map.  */
	  irela.r_addend = 2;
	  bfd_elf32_swap_reloca_out (output_bfd, &irela, loc);
	  rtld_reloc += 1;
	  BFD_ASSERT (rtld_reloc <= srelgot->reloc_count);

	  /* Fill in the literal table.  */
	  if (chunk < plt_chunks - 1)
	    chunk_entries = PLT_ENTRIES_PER_CHUNK;
	  else
	    chunk_entries = plt_entries - (chunk * PLT_ENTRIES_PER_CHUNK);

	  BFD_ASSERT ((unsigned) (chunk + 1) * 8 <= spltlittbl->_cooked_size);
	  bfd_put_32 (output_bfd,
		      sgotplt->output_section->vma + sgotplt->output_offset,
		      spltlittbl->contents + (chunk * 8) + 0);
	  bfd_put_32 (output_bfd,
		      8 + (chunk_entries * 4),
		      spltlittbl->contents + (chunk * 8) + 4);
	}

      /* All the dynamic relocations have been emitted at this point.
	 Make sure the relocation sections are the correct size.  */
      if (srelgot->_cooked_size != (sizeof (Elf32_External_Rela)
				    * srelgot->reloc_count)
	  || srelplt->_cooked_size != (sizeof (Elf32_External_Rela)
				       * srelplt->reloc_count))
	abort ();

     /* The .xt.lit.plt section has just been modified.  This must
	happen before the code below which combines adjacent literal
	table entries, and the .xt.lit.plt contents have to be forced to
	the output here.  */
      if (! bfd_set_section_contents (output_bfd,
				      spltlittbl->output_section,
				      spltlittbl->contents,
				      spltlittbl->output_offset,
				      spltlittbl->_raw_size))
	return FALSE;
      /* Clear SEC_HAS_CONTENTS so the contents won't be output again.  */
      spltlittbl->flags &= ~SEC_HAS_CONTENTS;
    }

  /* Combine adjacent literal table entries.  */
  BFD_ASSERT (! info->relocatable);
  sxtlit = bfd_get_section_by_name (output_bfd, ".xt.lit");
  sgotloc = bfd_get_section_by_name (dynobj, ".got.loc");
  BFD_ASSERT (sxtlit && sgotloc);
  num_xtlit_entries =
    elf_xtensa_combine_prop_entries (output_bfd, sxtlit, sgotloc);
  if (num_xtlit_entries < 0)
    return FALSE;

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

	case DT_XTENSA_GOT_LOC_SZ:
	  dyn.d_un.d_val = num_xtlit_entries;
	  break;

	case DT_XTENSA_GOT_LOC_OFF:
	  name = ".got.loc";
	  goto get_vma;
	case DT_PLTGOT:
	  name = ".got";
	  goto get_vma;
	case DT_JMPREL:
	  name = ".rela.plt";
	get_vma:
	  s = bfd_get_section_by_name (output_bfd, name);
	  BFD_ASSERT (s);
	  dyn.d_un.d_ptr = s->vma;
	  break;

	case DT_PLTRELSZ:
	  s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	  BFD_ASSERT (s);
	  dyn.d_un.d_val = (s->_cooked_size ? s->_cooked_size : s->_raw_size);
	  break;

	case DT_RELASZ:
	  /* Adjust RELASZ to not include JMPREL.  This matches what
	     glibc expects and what is done for several other ELF
	     targets (e.g., i386, alpha), but the "correct" behavior
	     seems to be unresolved.  Since the linker script arranges
	     for .rela.plt to follow all other relocation sections, we
	     don't have to worry about changing the DT_RELA entry.  */
	  s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	  if (s)
	    {
	      dyn.d_un.d_val -=
		(s->_cooked_size ? s->_cooked_size : s->_raw_size);
	    }
	  break;
	}

      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
    }

  return TRUE;
}


/* Functions for dealing with the e_flags field.  */

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static bfd_boolean
elf_xtensa_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  unsigned out_mach, in_mach;
  flagword out_flag, in_flag;

  /* Check if we have the same endianess.  */
  if (!_bfd_generic_verify_endian_match (ibfd, obfd))
    return FALSE;

  /* Don't even pretend to support mixed-format linking.  */
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return FALSE;

  out_flag = elf_elfheader (obfd)->e_flags;
  in_flag = elf_elfheader (ibfd)->e_flags;

  out_mach = out_flag & EF_XTENSA_MACH;
  in_mach = in_flag & EF_XTENSA_MACH;
  if (out_mach != in_mach) 
    {
      (*_bfd_error_handler)
	("%s: incompatible machine type. Output is 0x%x. Input is 0x%x",
	 bfd_archive_filename (ibfd), out_mach, in_mach);
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }

  if (! elf_flags_init (obfd))
    {
      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = in_flag;
      
      if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
	  && bfd_get_arch_info (obfd)->the_default)
	return bfd_set_arch_mach (obfd, bfd_get_arch (ibfd),
				  bfd_get_mach (ibfd));
      
      return TRUE;
    }

  if ((out_flag & EF_XTENSA_XT_INSN) !=
      (in_flag & EF_XTENSA_XT_INSN)) 
    elf_elfheader(obfd)->e_flags &= (~ EF_XTENSA_XT_INSN);

  if ((out_flag & EF_XTENSA_XT_LIT) !=
      (in_flag & EF_XTENSA_XT_LIT)) 
    elf_elfheader(obfd)->e_flags &= (~ EF_XTENSA_XT_LIT);

  return TRUE;
}


static bfd_boolean
elf_xtensa_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags |= flags;
  elf_flags_init (abfd) = TRUE;

  return TRUE;
}


extern flagword
elf_xtensa_get_private_bfd_flags (abfd)
     bfd *abfd;
{
  return elf_elfheader (abfd)->e_flags;
}


static bfd_boolean
elf_xtensa_print_private_bfd_data (abfd, farg)
     bfd *abfd;
     PTR farg;
{
  FILE *f = (FILE *) farg;
  flagword e_flags = elf_elfheader (abfd)->e_flags;

  fprintf (f, "\nXtensa header:\n");
  if ((e_flags & EF_XTENSA_MACH) == E_XTENSA_MACH) 
    fprintf (f, "\nMachine     = Base\n");
  else
    fprintf (f, "\nMachine Id  = 0x%x\n", e_flags & EF_XTENSA_MACH);

  fprintf (f, "Insn tables = %s\n",
	   (e_flags & EF_XTENSA_XT_INSN) ? "true" : "false");

  fprintf (f, "Literal tables = %s\n",
	   (e_flags & EF_XTENSA_XT_LIT) ? "true" : "false");

  return _bfd_elf_print_private_bfd_data (abfd, farg);
}


/* Set the right machine number for an Xtensa ELF file.  */

static bfd_boolean
elf_xtensa_object_p (abfd)
     bfd *abfd;
{
  int mach;
  unsigned long arch = elf_elfheader (abfd)->e_flags & EF_XTENSA_MACH;

  switch (arch)
    {
    case E_XTENSA_MACH:
      mach = bfd_mach_xtensa;
      break;
    default:
      return FALSE;
    }

  (void) bfd_default_set_arch_mach (abfd, bfd_arch_xtensa, mach);
  return TRUE;
}


/* The final processing done just before writing out an Xtensa ELF object
   file.  This gets the Xtensa architecture right based on the machine
   number.  */

static void
elf_xtensa_final_write_processing (abfd, linker)
     bfd *abfd;
     bfd_boolean linker ATTRIBUTE_UNUSED;
{
  int mach;
  unsigned long val;

  switch (mach = bfd_get_mach (abfd))
    {
    case bfd_mach_xtensa:
      val = E_XTENSA_MACH;
      break;
    default:
      return;
    }

  elf_elfheader (abfd)->e_flags &=  (~ EF_XTENSA_MACH);
  elf_elfheader (abfd)->e_flags |= val;
}


static enum elf_reloc_type_class
elf_xtensa_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  switch ((int) ELF32_R_TYPE (rela->r_info))
    {
    case R_XTENSA_RELATIVE:
      return reloc_class_relative;
    case R_XTENSA_JMP_SLOT:
      return reloc_class_plt;
    default:
      return reloc_class_normal;
    }
}


static bfd_boolean
elf_xtensa_discard_info_for_section (abfd, cookie, info, sec)
     bfd *abfd;
     struct elf_reloc_cookie *cookie;
     struct bfd_link_info *info;
     asection *sec;
{
  bfd_byte *contents;
  bfd_vma section_size;
  bfd_vma offset, actual_offset;
  size_t removed_bytes = 0;

  section_size = (sec->_cooked_size ? sec->_cooked_size : sec->_raw_size);
  if (section_size == 0 || section_size % 8 != 0)
    return FALSE;

  if (sec->output_section
      && bfd_is_abs_section (sec->output_section))
    return FALSE;

  contents = retrieve_contents (abfd, sec, info->keep_memory);
  if (!contents)
    return FALSE;

  cookie->rels = retrieve_internal_relocs (abfd, sec, info->keep_memory);
  if (!cookie->rels)
    {
      release_contents (sec, contents);
      return FALSE;
    }

  cookie->rel = cookie->rels;
  cookie->relend = cookie->rels + sec->reloc_count;

  for (offset = 0; offset < section_size; offset += 8)
    {
      actual_offset = offset - removed_bytes;

      /* The ...symbol_deleted_p function will skip over relocs but it
	 won't adjust their offsets, so do that here.  */
      while (cookie->rel < cookie->relend
	     && cookie->rel->r_offset < offset)
	{
	  cookie->rel->r_offset -= removed_bytes;
	  cookie->rel++;
	}

      while (cookie->rel < cookie->relend
	     && cookie->rel->r_offset == offset)
	{
	  if (bfd_elf_reloc_symbol_deleted_p (offset, cookie))
	    {
	      /* Remove the table entry.  (If the reloc type is NONE, then
		 the entry has already been merged with another and deleted
		 during relaxation.)  */
	      if (ELF32_R_TYPE (cookie->rel->r_info) != R_XTENSA_NONE)
		{
		  /* Shift the contents up.  */
		  if (offset + 8 < section_size)
		    memmove (&contents[actual_offset],
			     &contents[actual_offset+8],
			     section_size - offset - 8);
		  removed_bytes += 8;
		}

	      /* Remove this relocation.  */
	      cookie->rel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
	    }

	  /* Adjust the relocation offset for previous removals.  This
	     should not be done before calling ...symbol_deleted_p
	     because it might mess up the offset comparisons there.
	     Make sure the offset doesn't underflow in the case where
	     the first entry is removed.  */
	  if (cookie->rel->r_offset >= removed_bytes)
	    cookie->rel->r_offset -= removed_bytes;
	  else
	    cookie->rel->r_offset = 0;

	  cookie->rel++;
	}
    }

  if (removed_bytes != 0)
    {
      /* Adjust any remaining relocs (shouldn't be any).  */
      for (; cookie->rel < cookie->relend; cookie->rel++)
	{
	  if (cookie->rel->r_offset >= removed_bytes)
	    cookie->rel->r_offset -= removed_bytes;
	  else
	    cookie->rel->r_offset = 0;
	}

      /* Clear the removed bytes.  */
      memset (&contents[section_size - removed_bytes], 0, removed_bytes);

      pin_contents (sec, contents);
      pin_internal_relocs (sec, cookie->rels);

      sec->_cooked_size = section_size - removed_bytes;
      /* Also shrink _raw_size.  See comments in relax_property_section.  */
      sec->_raw_size = sec->_cooked_size;

      if (xtensa_is_littable_section (sec))
	{
	  bfd *dynobj = elf_hash_table (info)->dynobj;
	  if (dynobj)
	    {
	      asection *sgotloc =
		bfd_get_section_by_name (dynobj, ".got.loc");
	      if (sgotloc)
		{
		  bfd_size_type sgotloc_size =
		    (sgotloc->_cooked_size ? sgotloc->_cooked_size
		     : sgotloc->_raw_size);
		  sgotloc->_cooked_size = sgotloc_size - removed_bytes;
		  sgotloc->_raw_size = sgotloc_size - removed_bytes;
		}
	    }
	}
    }
  else
    {
      release_contents (sec, contents);
      release_internal_relocs (sec, cookie->rels);
    }

  return (removed_bytes != 0);
}


static bfd_boolean
elf_xtensa_discard_info (abfd, cookie, info)
     bfd *abfd;
     struct elf_reloc_cookie *cookie;
     struct bfd_link_info *info;
{
  asection *sec;
  bfd_boolean changed = FALSE;

  for (sec = abfd->sections; sec != NULL; sec = sec->next)
    {
      if (xtensa_is_property_section (sec))
	{
	  if (elf_xtensa_discard_info_for_section (abfd, cookie, info, sec))
	    changed = TRUE;
	}
    }

  return changed;
}


static bfd_boolean
elf_xtensa_ignore_discarded_relocs (sec)
     asection *sec;
{
  return xtensa_is_property_section (sec);
}


/* Support for core dump NOTE sections.  */

static bfd_boolean
elf_xtensa_grok_prstatus (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  int offset;
  unsigned int raw_size;

  /* The size for Xtensa is variable, so don't try to recognize the format
     based on the size.  Just assume this is GNU/Linux.  */

  /* pr_cursig */
  elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

  /* pr_pid */
  elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 24);

  /* pr_reg */
  offset = 72;
  raw_size = note->descsz - offset - 4;

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg",
					  raw_size, note->descpos + offset);
}


static bfd_boolean
elf_xtensa_grok_psinfo (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  switch (note->descsz)
    {
      default:
	return FALSE;

      case 128:		/* GNU/Linux elf_prpsinfo */
	elf_tdata (abfd)->core_program
	 = _bfd_elfcore_strndup (abfd, note->descdata + 32, 16);
	elf_tdata (abfd)->core_command
	 = _bfd_elfcore_strndup (abfd, note->descdata + 48, 80);
    }

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists.  */

  {
    char *command = elf_tdata (abfd)->core_command;
    int n = strlen (command);

    if (0 < n && command[n - 1] == ' ')
      command[n - 1] = '\0';
  }

  return TRUE;
}


/* Generic Xtensa configurability stuff.  */

static xtensa_opcode callx0_op = XTENSA_UNDEFINED;
static xtensa_opcode callx4_op = XTENSA_UNDEFINED;
static xtensa_opcode callx8_op = XTENSA_UNDEFINED;
static xtensa_opcode callx12_op = XTENSA_UNDEFINED;
static xtensa_opcode call0_op = XTENSA_UNDEFINED;
static xtensa_opcode call4_op = XTENSA_UNDEFINED;
static xtensa_opcode call8_op = XTENSA_UNDEFINED;
static xtensa_opcode call12_op = XTENSA_UNDEFINED;

static void
init_call_opcodes ()
{
  if (callx0_op == XTENSA_UNDEFINED)
    {
      callx0_op  = xtensa_opcode_lookup (xtensa_default_isa, "callx0");
      callx4_op  = xtensa_opcode_lookup (xtensa_default_isa, "callx4");
      callx8_op  = xtensa_opcode_lookup (xtensa_default_isa, "callx8");
      callx12_op = xtensa_opcode_lookup (xtensa_default_isa, "callx12");
      call0_op   = xtensa_opcode_lookup (xtensa_default_isa, "call0");
      call4_op   = xtensa_opcode_lookup (xtensa_default_isa, "call4");
      call8_op   = xtensa_opcode_lookup (xtensa_default_isa, "call8");
      call12_op  = xtensa_opcode_lookup (xtensa_default_isa, "call12");
    }
}


static bfd_boolean
is_indirect_call_opcode (opcode)
     xtensa_opcode opcode;
{
  init_call_opcodes ();
  return (opcode == callx0_op
	  || opcode == callx4_op
	  || opcode == callx8_op
	  || opcode == callx12_op);
}


static bfd_boolean
is_direct_call_opcode (opcode)
     xtensa_opcode opcode;
{
  init_call_opcodes ();
  return (opcode == call0_op
	  || opcode == call4_op
	  || opcode == call8_op
	  || opcode == call12_op);
}


static bfd_boolean
is_windowed_call_opcode (opcode)
     xtensa_opcode opcode;
{
  init_call_opcodes ();
  return (opcode == call4_op
	  || opcode == call8_op
	  || opcode == call12_op
	  || opcode == callx4_op
	  || opcode == callx8_op
	  || opcode == callx12_op);
}


static xtensa_opcode
get_l32r_opcode (void)
{
  static xtensa_opcode l32r_opcode = XTENSA_UNDEFINED;
  if (l32r_opcode == XTENSA_UNDEFINED)
    {
      l32r_opcode = xtensa_opcode_lookup (xtensa_default_isa, "l32r");
      BFD_ASSERT (l32r_opcode != XTENSA_UNDEFINED);
    }
  return l32r_opcode;
}


static bfd_vma
l32r_offset (addr, pc)
     bfd_vma addr;
     bfd_vma pc;
{
  bfd_vma offset;

  offset = addr - ((pc+3) & -4);
  BFD_ASSERT ((offset & ((1 << 2) - 1)) == 0);
  offset = (signed int) offset >> 2;
  BFD_ASSERT ((signed int) offset >> 16 == -1);
  return offset;
}


/* Get the operand number for a PC-relative relocation.
   If the relocation is not a PC-relative one, return (-1).  */

static int
get_relocation_opnd (irel)
     Elf_Internal_Rela *irel;
{
  if (ELF32_R_TYPE (irel->r_info) < R_XTENSA_OP0
      || ELF32_R_TYPE (irel->r_info) >= R_XTENSA_max)
    return -1;
  return ELF32_R_TYPE (irel->r_info) - R_XTENSA_OP0;
}


/* Get the opcode for a relocation.  */

static xtensa_opcode
get_relocation_opcode (sec, contents, irel)
     asection *sec;
     bfd_byte *contents;
     Elf_Internal_Rela *irel;
{
  static xtensa_insnbuf ibuff = NULL;
  xtensa_isa isa = xtensa_default_isa;

  if (get_relocation_opnd (irel) == -1)
    return XTENSA_UNDEFINED;

  if (contents == NULL)
    return XTENSA_UNDEFINED;

  if (sec->_raw_size <= irel->r_offset)
    return XTENSA_UNDEFINED;

  if (ibuff == NULL)
    ibuff = xtensa_insnbuf_alloc (isa);
      
  /* Decode the instruction.  */
  xtensa_insnbuf_from_chars (isa, ibuff, &contents[irel->r_offset]);
  return xtensa_decode_insn (isa, ibuff);
}


bfd_boolean
is_l32r_relocation (sec, contents, irel)
     asection *sec;
     bfd_byte *contents;
     Elf_Internal_Rela *irel;
{
  xtensa_opcode opcode;

  if (ELF32_R_TYPE (irel->r_info) != R_XTENSA_OP1)
    return FALSE;
  
  opcode = get_relocation_opcode (sec, contents, irel);
  return (opcode == get_l32r_opcode ());
}


/* Code for transforming CALLs at link-time.  */

static bfd_reloc_status_type
elf_xtensa_do_asm_simplify (contents, address, content_length)
     bfd_byte *contents;
     bfd_vma address;
     bfd_vma content_length;
{
  static xtensa_insnbuf insnbuf = NULL;
  xtensa_opcode opcode;
  xtensa_operand operand;
  xtensa_opcode direct_call_opcode;
  xtensa_isa isa = xtensa_default_isa;
  bfd_byte *chbuf = contents + address;
  int opn;

  if (insnbuf == NULL)
    insnbuf = xtensa_insnbuf_alloc (isa);

  if (content_length < address)
    {
      (*_bfd_error_handler)
	("Attempt to convert L32R/CALLX to CALL failed");
      return bfd_reloc_other;
    }

  opcode = get_expanded_call_opcode (chbuf, content_length - address);
  direct_call_opcode = swap_callx_for_call_opcode (opcode);
  if (direct_call_opcode == XTENSA_UNDEFINED)
    {
      (*_bfd_error_handler)
	("Attempt to convert L32R/CALLX to CALL failed");
      return bfd_reloc_other;
    }
  
  /* Assemble a NOP ("or a1, a1, a1") into the 0 byte offset.  */
  opcode = xtensa_opcode_lookup (isa, "or");
  xtensa_encode_insn (isa, opcode, insnbuf);
  for (opn = 0; opn < 3; opn++) 
    {
      operand = xtensa_get_operand (isa, opcode, opn);
      xtensa_operand_set_field (operand, insnbuf, 1);
    }
  xtensa_insnbuf_to_chars (isa, insnbuf, chbuf);

  /* Assemble a CALL ("callN 0") into the 3 byte offset.  */
  xtensa_encode_insn (isa, direct_call_opcode, insnbuf);
  operand = xtensa_get_operand (isa, opcode, 0);
  xtensa_operand_set_field (operand, insnbuf, 0);
  xtensa_insnbuf_to_chars (isa, insnbuf, chbuf + 3);

  return bfd_reloc_ok;
}


static bfd_reloc_status_type
contract_asm_expansion (contents, content_length, irel)
     bfd_byte *contents;
     bfd_vma content_length;
     Elf_Internal_Rela *irel;
{
  bfd_reloc_status_type retval =
    elf_xtensa_do_asm_simplify (contents, irel->r_offset, content_length);

  if (retval != bfd_reloc_ok)
    return retval;

  /* Update the irel->r_offset field so that the right immediate and
     the right instruction are modified during the relocation.  */
  irel->r_offset += 3;
  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info), R_XTENSA_OP0);
  return bfd_reloc_ok;
}


static xtensa_opcode
swap_callx_for_call_opcode (opcode)
     xtensa_opcode opcode;
{
  init_call_opcodes ();

  if (opcode == callx0_op) return call0_op;
  if (opcode == callx4_op) return call4_op;
  if (opcode == callx8_op) return call8_op;
  if (opcode == callx12_op) return call12_op;

  /* Return XTENSA_UNDEFINED if the opcode is not an indirect call.  */
  return XTENSA_UNDEFINED;
}


/* Check if "buf" is pointing to a "L32R aN; CALLX aN" sequence, and
   if so, return the CALLX opcode.  If not, return XTENSA_UNDEFINED.  */

#define L32R_TARGET_REG_OPERAND 0
#define CALLN_SOURCE_OPERAND 0

static xtensa_opcode 
get_expanded_call_opcode (buf, bufsize)
     bfd_byte *buf;
     int bufsize;
{
  static xtensa_insnbuf insnbuf = NULL;
  xtensa_opcode opcode;
  xtensa_operand operand;
  xtensa_isa isa = xtensa_default_isa;
  uint32 regno, call_regno;
  
  /* Buffer must be at least 6 bytes.  */
  if (bufsize < 6)
    return XTENSA_UNDEFINED;

  if (insnbuf == NULL)
    insnbuf = xtensa_insnbuf_alloc (isa);
      
  xtensa_insnbuf_from_chars (isa, insnbuf, buf);
  opcode = xtensa_decode_insn (isa, insnbuf);
  
  if (opcode != get_l32r_opcode ())
    return XTENSA_UNDEFINED;
  
  operand = xtensa_get_operand (isa, opcode, L32R_TARGET_REG_OPERAND);
  regno = xtensa_operand_decode
    (operand, xtensa_operand_get_field (operand, insnbuf));
  
  /* Next instruction should be an CALLXn with operand 0 == regno.  */
  xtensa_insnbuf_from_chars (isa, insnbuf, 
			     buf + xtensa_insn_length (isa, opcode));
  opcode = xtensa_decode_insn (isa, insnbuf);
  
  if (!is_indirect_call_opcode (opcode))
    return XTENSA_UNDEFINED;
  
  operand = xtensa_get_operand (isa, opcode, CALLN_SOURCE_OPERAND);
  call_regno = xtensa_operand_decode
    (operand, xtensa_operand_get_field (operand, insnbuf));
  if (call_regno != regno)
    return XTENSA_UNDEFINED;
  
  return opcode;
}


/* Data structures used during relaxation.  */

/* r_reloc: relocation values.  */

/* Through the relaxation process, we need to keep track of the values
   that will result from evaluating relocations.  The standard ELF
   relocation structure is not sufficient for this purpose because we're
   operating on multiple input files at once, so we need to know which
   input file a relocation refers to.  The r_reloc structure thus
   records both the input file (bfd) and ELF relocation.

   For efficiency, an r_reloc also contains a "target_offset" field to
   cache the target-section-relative offset value that is represented by
   the relocation.  */

typedef struct r_reloc_struct r_reloc;

struct r_reloc_struct
{
  bfd *abfd;
  Elf_Internal_Rela rela;
  bfd_vma target_offset;
};

static bfd_boolean r_reloc_is_const
  PARAMS ((const r_reloc *));
static void r_reloc_init
  PARAMS ((r_reloc *, bfd *, Elf_Internal_Rela *));
static bfd_vma r_reloc_get_target_offset
  PARAMS ((const r_reloc *));
static asection *r_reloc_get_section
  PARAMS ((const r_reloc *));
static bfd_boolean r_reloc_is_defined
  PARAMS ((const r_reloc *));
static struct elf_link_hash_entry *r_reloc_get_hash_entry
  PARAMS ((const r_reloc *));


/* The r_reloc structure is included by value in literal_value, but not
   every literal_value has an associated relocation -- some are simple
   constants.  In such cases, we set all the fields in the r_reloc
   struct to zero.  The r_reloc_is_const function should be used to
   detect this case.  */

static bfd_boolean
r_reloc_is_const (r_rel)
     const r_reloc *r_rel;
{
  return (r_rel->abfd == NULL);
}


static void
r_reloc_init (r_rel, abfd, irel) 
     r_reloc *r_rel;
     bfd *abfd;
     Elf_Internal_Rela *irel;
{
  if (irel != NULL)
    {
      r_rel->rela = *irel;
      r_rel->abfd = abfd;
      r_rel->target_offset = r_reloc_get_target_offset (r_rel);
    }
  else
    memset (r_rel, 0, sizeof (r_reloc));
}


static bfd_vma
r_reloc_get_target_offset (r_rel)
     const r_reloc *r_rel;
{
  bfd_vma target_offset;
  unsigned long r_symndx;

  BFD_ASSERT (!r_reloc_is_const (r_rel));
  r_symndx = ELF32_R_SYM (r_rel->rela.r_info);
  target_offset = get_elf_r_symndx_offset (r_rel->abfd, r_symndx);
  return (target_offset + r_rel->rela.r_addend);
}


static struct elf_link_hash_entry *
r_reloc_get_hash_entry (r_rel)
     const r_reloc *r_rel;
{
  unsigned long r_symndx = ELF32_R_SYM (r_rel->rela.r_info);
  return get_elf_r_symndx_hash_entry (r_rel->abfd, r_symndx);
}


static asection *
r_reloc_get_section (r_rel)
     const r_reloc *r_rel;
{
  unsigned long r_symndx = ELF32_R_SYM (r_rel->rela.r_info);
  return get_elf_r_symndx_section (r_rel->abfd, r_symndx);
}


static bfd_boolean
r_reloc_is_defined (r_rel)
     const r_reloc *r_rel;
{
  asection *sec = r_reloc_get_section (r_rel);
  if (sec == bfd_abs_section_ptr
      || sec == bfd_com_section_ptr
      || sec == bfd_und_section_ptr)
    return FALSE;
  return TRUE;
}


/* source_reloc: relocations that reference literal sections.  */

/* To determine whether literals can be coalesced, we need to first
   record all the relocations that reference the literals.  The
   source_reloc structure below is used for this purpose.  The
   source_reloc entries are kept in a per-literal-section array, sorted
   by offset within the literal section (i.e., target offset).

   The source_sec and r_rel.rela.r_offset fields identify the source of
   the relocation.  The r_rel field records the relocation value, i.e.,
   the offset of the literal being referenced.  The opnd field is needed
   to determine the range of the immediate field to which the relocation
   applies, so we can determine whether another literal with the same
   value is within range.  The is_null field is true when the relocation
   is being removed (e.g., when an L32R is being removed due to a CALLX
   that is converted to a direct CALL).  */

typedef struct source_reloc_struct source_reloc;

struct source_reloc_struct
{
  asection *source_sec;
  r_reloc r_rel;
  xtensa_operand opnd;
  bfd_boolean is_null;
};


static void init_source_reloc
  PARAMS ((source_reloc *, asection *, const r_reloc *, xtensa_operand));
static source_reloc *find_source_reloc
  PARAMS ((source_reloc *, int, asection *, Elf_Internal_Rela *));
static int source_reloc_compare
  PARAMS ((const PTR, const PTR));


static void
init_source_reloc (reloc, source_sec, r_rel, opnd)
     source_reloc *reloc;
     asection *source_sec;
     const r_reloc *r_rel;
     xtensa_operand opnd;
{
  reloc->source_sec = source_sec;
  reloc->r_rel = *r_rel;
  reloc->opnd = opnd;
  reloc->is_null = FALSE;
}


/* Find the source_reloc for a particular source offset and relocation
   type.  Note that the array is sorted by _target_ offset, so this is
   just a linear search.  */

static source_reloc *
find_source_reloc (src_relocs, src_count, sec, irel)
     source_reloc *src_relocs;
     int src_count;
     asection *sec;
     Elf_Internal_Rela *irel;
{
  int i;

  for (i = 0; i < src_count; i++)
    {
      if (src_relocs[i].source_sec == sec
	  && src_relocs[i].r_rel.rela.r_offset == irel->r_offset
	  && (ELF32_R_TYPE (src_relocs[i].r_rel.rela.r_info)
	      == ELF32_R_TYPE (irel->r_info)))
	return &src_relocs[i];
    }

  return NULL;
}


static int
source_reloc_compare (ap, bp)
     const PTR ap;
     const PTR bp;
{
  const source_reloc *a = (const source_reloc *) ap;
  const source_reloc *b = (const source_reloc *) bp;

  return (a->r_rel.target_offset - b->r_rel.target_offset);
}


/* Literal values and value hash tables.  */

/* Literals with the same value can be coalesced.  The literal_value
   structure records the value of a literal: the "r_rel" field holds the
   information from the relocation on the literal (if there is one) and
   the "value" field holds the contents of the literal word itself.

   The value_map structure records a literal value along with the
   location of a literal holding that value.  The value_map hash table
   is indexed by the literal value, so that we can quickly check if a
   particular literal value has been seen before and is thus a candidate
   for coalescing.  */

typedef struct literal_value_struct literal_value;
typedef struct value_map_struct value_map;
typedef struct value_map_hash_table_struct value_map_hash_table;

struct literal_value_struct
{
  r_reloc r_rel; 
  unsigned long value;
};

struct value_map_struct
{
  literal_value val;			/* The literal value.  */
  r_reloc loc;				/* Location of the literal.  */
  value_map *next;
};

struct value_map_hash_table_struct
{
  unsigned bucket_count;
  value_map **buckets;
  unsigned count;
};


static bfd_boolean is_same_value
  PARAMS ((const literal_value *, const literal_value *, bfd_boolean));
static value_map_hash_table *value_map_hash_table_init
  PARAMS ((void));
static unsigned hash_literal_value
  PARAMS ((const literal_value *));
static unsigned hash_bfd_vma
  PARAMS ((bfd_vma));
static value_map *get_cached_value
  PARAMS ((value_map_hash_table *, const literal_value *, bfd_boolean));
static value_map *add_value_map
  PARAMS ((value_map_hash_table *, const literal_value *, const r_reloc *,
	   bfd_boolean));


static bfd_boolean
is_same_value (src1, src2, final_static_link)
     const literal_value *src1;
     const literal_value *src2;
     bfd_boolean final_static_link;
{
  struct elf_link_hash_entry *h1, *h2;

  if (r_reloc_is_const (&src1->r_rel) != r_reloc_is_const (&src2->r_rel)) 
    return FALSE;

  if (r_reloc_is_const (&src1->r_rel))
    return (src1->value == src2->value);

  if (ELF32_R_TYPE (src1->r_rel.rela.r_info)
      != ELF32_R_TYPE (src2->r_rel.rela.r_info))
    return FALSE;

  if (r_reloc_get_target_offset (&src1->r_rel)
      != r_reloc_get_target_offset (&src2->r_rel))
    return FALSE;

  if (src1->value != src2->value)
    return FALSE;
  
  /* Now check for the same section (if defined) or the same elf_hash
     (if undefined or weak).  */
  h1 = r_reloc_get_hash_entry (&src1->r_rel);
  h2 = r_reloc_get_hash_entry (&src2->r_rel);
  if (r_reloc_is_defined (&src1->r_rel)
      && (final_static_link
	  || ((!h1 || h1->root.type != bfd_link_hash_defweak)
	      && (!h2 || h2->root.type != bfd_link_hash_defweak))))
    {
      if (r_reloc_get_section (&src1->r_rel)
	  != r_reloc_get_section (&src2->r_rel))
	return FALSE;
    }
  else
    {
      /* Require that the hash entries (i.e., symbols) be identical.  */
      if (h1 != h2 || h1 == 0)
	return FALSE;
    }

  return TRUE;
}


/* Must be power of 2.  */
#define INITIAL_HASH_RELOC_BUCKET_COUNT 1024

static value_map_hash_table *
value_map_hash_table_init ()
{
  value_map_hash_table *values;

  values = (value_map_hash_table *)
    bfd_malloc (sizeof (value_map_hash_table));

  values->bucket_count = INITIAL_HASH_RELOC_BUCKET_COUNT;
  values->count = 0;
  values->buckets = (value_map **)
    bfd_zmalloc (sizeof (value_map *) * values->bucket_count);

  return values;
}


static unsigned
hash_bfd_vma (val) 
     bfd_vma val;
{
  return (val >> 2) + (val >> 10);
}


static unsigned
hash_literal_value (src)
     const literal_value *src;
{
  unsigned hash_val;

  if (r_reloc_is_const (&src->r_rel))
    return hash_bfd_vma (src->value);

  hash_val = (hash_bfd_vma (r_reloc_get_target_offset (&src->r_rel))
	      + hash_bfd_vma (src->value));
  
  /* Now check for the same section and the same elf_hash.  */
  if (r_reloc_is_defined (&src->r_rel))
    hash_val += hash_bfd_vma ((bfd_vma) (unsigned) r_reloc_get_section (&src->r_rel));
  else
    hash_val += hash_bfd_vma ((bfd_vma) (unsigned) r_reloc_get_hash_entry (&src->r_rel));

  return hash_val;
}


/* Check if the specified literal_value has been seen before.  */

static value_map *
get_cached_value (map, val, final_static_link)
     value_map_hash_table *map;
     const literal_value *val;
     bfd_boolean final_static_link;
{
  value_map *map_e;
  value_map *bucket;
  unsigned idx;

  idx = hash_literal_value (val);
  idx = idx & (map->bucket_count - 1);
  bucket = map->buckets[idx];
  for (map_e = bucket; map_e; map_e = map_e->next)
    {
      if (is_same_value (&map_e->val, val, final_static_link))
	return map_e;
    }
  return NULL;
}


/* Record a new literal value.  It is illegal to call this if VALUE
   already has an entry here.  */

static value_map *
add_value_map (map, val, loc, final_static_link)
     value_map_hash_table *map;
     const literal_value *val;
     const r_reloc *loc;
     bfd_boolean final_static_link;
{
  value_map **bucket_p;
  unsigned idx;

  value_map *val_e = (value_map *) bfd_zmalloc (sizeof (value_map));

  BFD_ASSERT (get_cached_value (map, val, final_static_link) == NULL);
  val_e->val = *val;
  val_e->loc = *loc;

  idx = hash_literal_value (val);
  idx = idx & (map->bucket_count - 1);
  bucket_p = &map->buckets[idx];

  val_e->next = *bucket_p;
  *bucket_p = val_e;
  map->count++;
  /* FIXME: consider resizing the hash table if we get too many entries */
  
  return val_e;
}


/* Lists of literals being coalesced or removed.  */

/* In the usual case, the literal identified by "from" is being
   coalesced with another literal identified by "to".  If the literal is
   unused and is being removed altogether, "to.abfd" will be NULL.
   The removed_literal entries are kept on a per-section list, sorted
   by the "from" offset field.  */

typedef struct removed_literal_struct removed_literal;
typedef struct removed_literal_list_struct removed_literal_list;

struct removed_literal_struct
{
  r_reloc from;
  r_reloc to;
  removed_literal *next;
};

struct removed_literal_list_struct
{
  removed_literal *head;
  removed_literal *tail;
};


static void add_removed_literal
  PARAMS ((removed_literal_list *, const r_reloc *, const r_reloc *));
static removed_literal *find_removed_literal
  PARAMS ((removed_literal_list *, bfd_vma));
static bfd_vma offset_with_removed_literals
  PARAMS ((removed_literal_list *, bfd_vma));


/* Record that the literal at "from" is being removed.  If "to" is not
   NULL, the "from" literal is being coalesced with the "to" literal.  */

static void
add_removed_literal (removed_list, from, to)
     removed_literal_list *removed_list;
     const r_reloc *from;
     const r_reloc *to;
{
  removed_literal *r, *new_r, *next_r;

  new_r = (removed_literal *) bfd_zmalloc (sizeof (removed_literal));

  new_r->from = *from;
  if (to)
    new_r->to = *to;
  else
    new_r->to.abfd = NULL;
  new_r->next = NULL;
  
  r = removed_list->head;
  if (r == NULL) 
    {
      removed_list->head = new_r;
      removed_list->tail = new_r;
    }
  /* Special check for common case of append.  */
  else if (removed_list->tail->from.target_offset < from->target_offset)
    {
      removed_list->tail->next = new_r;
      removed_list->tail = new_r;
    }
  else
    {
      while (r->from.target_offset < from->target_offset
	     && r->next != NULL) 
	{
	  r = r->next;
	}
      next_r = r->next;
      r->next = new_r;
      new_r->next = next_r;
      if (next_r == NULL)
	removed_list->tail = new_r;
    }
}


/* Check if the list of removed literals contains an entry for the
   given address.  Return the entry if found.  */

static removed_literal *
find_removed_literal (removed_list, addr)
     removed_literal_list *removed_list;
     bfd_vma addr;
{
  removed_literal *r = removed_list->head;
  while (r && r->from.target_offset < addr)
    r = r->next;
  if (r && r->from.target_offset == addr)
    return r;
  return NULL;
}


/* Adjust an offset in a section to compensate for literals that are
   being removed.  Search the list of removed literals and subtract
   4 bytes for every removed literal prior to the given address.  */

static bfd_vma 
offset_with_removed_literals (removed_list, addr)
     removed_literal_list *removed_list;
     bfd_vma addr;
{
  removed_literal *r = removed_list->head;
  unsigned num_bytes = 0;

  if (r == NULL)
    return addr;

  while (r && r->from.target_offset <= addr)
    {
      num_bytes += 4;
      r = r->next;
    }
  if (num_bytes > addr)
    return 0;
  return (addr - num_bytes);
}


/* Coalescing literals may require a relocation to refer to a section in
   a different input file, but the standard relocation information
   cannot express that.  Instead, the reloc_bfd_fix structures are used
   to "fix" the relocations that refer to sections in other input files.
   These structures are kept on per-section lists.  The "src_type" field
   records the relocation type in case there are multiple relocations on
   the same location.  FIXME: This is ugly; an alternative might be to
   add new symbols with the "owner" field to some other input file.  */

typedef struct reloc_bfd_fix_struct reloc_bfd_fix;

struct reloc_bfd_fix_struct
{
  asection *src_sec;
  bfd_vma src_offset;
  unsigned src_type;			/* Relocation type.  */
  
  bfd *target_abfd;
  asection *target_sec;
  bfd_vma target_offset;
  
  reloc_bfd_fix *next;
};


static reloc_bfd_fix *reloc_bfd_fix_init
  PARAMS ((asection *, bfd_vma, unsigned, bfd *, asection *, bfd_vma));
static reloc_bfd_fix *get_bfd_fix
  PARAMS ((reloc_bfd_fix *, asection *, bfd_vma, unsigned));


static reloc_bfd_fix *
reloc_bfd_fix_init (src_sec, src_offset, src_type,
		    target_abfd, target_sec, target_offset)
     asection *src_sec;
     bfd_vma src_offset;
     unsigned src_type;
     bfd *target_abfd;
     asection *target_sec;
     bfd_vma target_offset;
{
  reloc_bfd_fix *fix;

  fix = (reloc_bfd_fix *) bfd_malloc (sizeof (reloc_bfd_fix));
  fix->src_sec = src_sec;
  fix->src_offset = src_offset;
  fix->src_type = src_type;
  fix->target_abfd = target_abfd;
  fix->target_sec = target_sec;
  fix->target_offset = target_offset;

  return fix;
}


static reloc_bfd_fix *
get_bfd_fix (fix_list, sec, offset, type)
     reloc_bfd_fix *fix_list;
     asection *sec;
     bfd_vma offset;
     unsigned type;
{
  reloc_bfd_fix *r;

  for (r = fix_list; r != NULL; r = r->next) 
    {
      if (r->src_sec == sec
	  && r->src_offset == offset
	  && r->src_type == type)
	return r;
    }
  return NULL;
}


/* Per-section data for relaxation.  */

struct xtensa_relax_info_struct
{
  bfd_boolean is_relaxable_literal_section;
  int visited;				/* Number of times visited.  */

  source_reloc *src_relocs;		/* Array[src_count].  */
  int src_count;
  int src_next;				/* Next src_relocs entry to assign.  */

  removed_literal_list removed_list;

  reloc_bfd_fix *fix_list;
};

struct elf_xtensa_section_data
{
  struct bfd_elf_section_data elf;
  xtensa_relax_info relax_info;
};

static void init_xtensa_relax_info
  PARAMS ((asection *));
static xtensa_relax_info *get_xtensa_relax_info
  PARAMS ((asection *));
static void add_fix
  PARAMS ((asection *, reloc_bfd_fix *));


static bfd_boolean
elf_xtensa_new_section_hook (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  struct elf_xtensa_section_data *sdata;
  bfd_size_type amt = sizeof (*sdata);

  sdata = (struct elf_xtensa_section_data *) bfd_zalloc (abfd, amt);
  if (sdata == NULL)
    return FALSE;
  sec->used_by_bfd = (PTR) sdata;

  return _bfd_elf_new_section_hook (abfd, sec);
}


static void
init_xtensa_relax_info (sec)
     asection *sec;
{
  xtensa_relax_info *relax_info = get_xtensa_relax_info (sec);

  relax_info->is_relaxable_literal_section = FALSE;
  relax_info->visited = 0;

  relax_info->src_relocs = NULL;
  relax_info->src_count = 0;
  relax_info->src_next = 0;

  relax_info->removed_list.head = NULL;
  relax_info->removed_list.tail = NULL;

  relax_info->fix_list = NULL;
}


static xtensa_relax_info *
get_xtensa_relax_info (sec)
     asection *sec;
{
  struct elf_xtensa_section_data *section_data;

  /* No info available if no section or if it is an output section.  */
  if (!sec || sec == sec->output_section)
    return NULL;

  section_data = (struct elf_xtensa_section_data *) elf_section_data (sec);
  return &section_data->relax_info;
}


static void
add_fix (src_sec, fix)
     asection *src_sec;
     reloc_bfd_fix *fix;
{
  xtensa_relax_info *relax_info;

  relax_info = get_xtensa_relax_info (src_sec);
  fix->next = relax_info->fix_list;
  relax_info->fix_list = fix;
}


/* Access to internal relocations, section contents and symbols.  */

/* During relaxation, we need to modify relocations, section contents,
   and symbol definitions, and we need to keep the original values from
   being reloaded from the input files, i.e., we need to "pin" the
   modified values in memory.  We also want to continue to observe the
   setting of the "keep-memory" flag.  The following functions wrap the
   standard BFD functions to take care of this for us.  */

static Elf_Internal_Rela *
retrieve_internal_relocs (abfd, sec, keep_memory)
     bfd *abfd;
     asection *sec;
     bfd_boolean keep_memory;
{
  Elf_Internal_Rela *internal_relocs;

  if ((sec->flags & SEC_LINKER_CREATED) != 0)
    return NULL;

  internal_relocs = elf_section_data (sec)->relocs;
  if (internal_relocs == NULL)
    internal_relocs = (_bfd_elf_link_read_relocs
		       (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
			keep_memory));
  return internal_relocs;
}


static void
pin_internal_relocs (sec, internal_relocs)
     asection *sec;
     Elf_Internal_Rela *internal_relocs;
{
  elf_section_data (sec)->relocs = internal_relocs;
}


static void
release_internal_relocs (sec, internal_relocs)
     asection *sec;
     Elf_Internal_Rela *internal_relocs;
{
  if (internal_relocs
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);
}


static bfd_byte *
retrieve_contents (abfd, sec, keep_memory)
     bfd *abfd;
     asection *sec;
     bfd_boolean keep_memory;
{
  bfd_byte *contents;

  contents = elf_section_data (sec)->this_hdr.contents;
  
  if (contents == NULL && sec->_raw_size != 0)
    {
      contents = (bfd_byte *) bfd_malloc (sec->_raw_size);
      if (contents != NULL)
	{
	  if (! bfd_get_section_contents (abfd, sec, contents,
					  (file_ptr) 0, sec->_raw_size))
	    {
	      free (contents);
	      return NULL;
	    }
	  if (keep_memory) 
	    elf_section_data (sec)->this_hdr.contents = contents;
	}
    }
  return contents;
}


static void
pin_contents (sec, contents)
     asection *sec;
     bfd_byte *contents;
{
  elf_section_data (sec)->this_hdr.contents = contents;
}


static void
release_contents (sec, contents)
     asection *sec;
     bfd_byte *contents;
{
  if (contents && 
      elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
}


static Elf_Internal_Sym *
retrieve_local_syms (input_bfd)
     bfd *input_bfd;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Sym *isymbuf;
  size_t locsymcount;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  locsymcount = symtab_hdr->sh_info;

  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
  if (isymbuf == NULL && locsymcount != 0)
    isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr, locsymcount, 0,
				    NULL, NULL, NULL);

  /* Save the symbols for this input file so they won't be read again.  */
  if (isymbuf && isymbuf != (Elf_Internal_Sym *) symtab_hdr->contents)
    symtab_hdr->contents = (unsigned char *) isymbuf;

  return isymbuf;
}


/* Code for link-time relaxation.  */

/* Local helper functions.  */
static bfd_boolean analyze_relocations
  PARAMS ((struct bfd_link_info *));
static bfd_boolean find_relaxable_sections
  PARAMS ((bfd *, asection *, struct bfd_link_info *, bfd_boolean *));
static bfd_boolean collect_source_relocs
  PARAMS ((bfd *, asection *, struct bfd_link_info *));
static bfd_boolean is_resolvable_asm_expansion
  PARAMS ((bfd *, asection *, bfd_byte *, Elf_Internal_Rela *,
	   struct bfd_link_info *, bfd_boolean *));
static bfd_boolean remove_literals
  PARAMS ((bfd *, asection *, struct bfd_link_info *, value_map_hash_table *));
static bfd_boolean relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *));
static bfd_boolean relax_property_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *));
static bfd_boolean relax_section_symbols
  PARAMS ((bfd *, asection *));
static bfd_boolean relocations_reach
  PARAMS ((source_reloc *, int, const r_reloc *));
static void translate_reloc
  PARAMS ((const r_reloc *, r_reloc *));
static Elf_Internal_Rela *get_irel_at_offset
  PARAMS ((asection *, Elf_Internal_Rela *, bfd_vma));
static Elf_Internal_Rela *find_associated_l32r_irel
  PARAMS ((asection *, bfd_byte *, Elf_Internal_Rela *,
	   Elf_Internal_Rela *));
static void shrink_dynamic_reloc_sections
  PARAMS ((struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *));


static bfd_boolean 
elf_xtensa_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     bfd_boolean *again;
{
  static value_map_hash_table *values = NULL;
  xtensa_relax_info *relax_info;

  if (!values)
    {
      /* Do some overall initialization for relaxation.  */
      values = value_map_hash_table_init ();
      relaxing_section = TRUE;
      if (!analyze_relocations (link_info))
	return FALSE;
    }
  *again = FALSE;

  /* Don't mess with linker-created sections.  */
  if ((sec->flags & SEC_LINKER_CREATED) != 0)
    return TRUE;

  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info != NULL);

  switch (relax_info->visited)
    {
    case 0:
      /* Note: It would be nice to fold this pass into
	 analyze_relocations, but it is important for this step that the
	 sections be examined in link order.  */
      if (!remove_literals (abfd, sec, link_info, values))
	return FALSE;
      *again = TRUE;
      break;

    case 1:
      if (!relax_section (abfd, sec, link_info))
	return FALSE;
      *again = TRUE;
      break;

    case 2:
      if (!relax_section_symbols (abfd, sec))
	return FALSE;
      break;
    }

  relax_info->visited++;
  return TRUE;
}

/* Initialization for relaxation.  */

/* This function is called once at the start of relaxation.  It scans
   all the input sections and marks the ones that are relaxable (i.e.,
   literal sections with L32R relocations against them).  It then
   collect source_reloc information for all the relocations against
   those relaxable sections.  */

static bfd_boolean
analyze_relocations (link_info)
     struct bfd_link_info *link_info;
{
  bfd *abfd;
  asection *sec;
  bfd_boolean is_relaxable = FALSE;

  /* Initialize the per-section relaxation info.  */
  for (abfd = link_info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (sec = abfd->sections; sec != NULL; sec = sec->next)
      {
	init_xtensa_relax_info (sec);
      }

  /* Mark relaxable sections (and count relocations against each one).  */
  for (abfd = link_info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (sec = abfd->sections; sec != NULL; sec = sec->next)
      {
	if (!find_relaxable_sections (abfd, sec, link_info, &is_relaxable))
	  return FALSE;
      }

  /* Bail out if there are no relaxable sections.  */
  if (!is_relaxable)
    return TRUE;

  /* Allocate space for source_relocs.  */
  for (abfd = link_info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (sec = abfd->sections; sec != NULL; sec = sec->next)
      {
	xtensa_relax_info *relax_info;

	relax_info = get_xtensa_relax_info (sec);
	if (relax_info->is_relaxable_literal_section)
	  {
	    relax_info->src_relocs = (source_reloc *)
	      bfd_malloc (relax_info->src_count * sizeof (source_reloc));
	  }
      }

  /* Collect info on relocations against each relaxable section.  */
  for (abfd = link_info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (sec = abfd->sections; sec != NULL; sec = sec->next)
      {
	if (!collect_source_relocs (abfd, sec, link_info))
	  return FALSE;
      }

  return TRUE;
}


/* Find all the literal sections that might be relaxed.  The motivation
   for this pass is that collect_source_relocs() needs to record _all_
   the relocations that target each relaxable section.  That is
   expensive and unnecessary unless the target section is actually going
   to be relaxed.  This pass identifies all such sections by checking if
   they have L32Rs pointing to them.  In the process, the total number
   of relocations targeting each section is also counted so that we
   know how much space to allocate for source_relocs against each
   relaxable literal section.  */

static bfd_boolean
find_relaxable_sections (abfd, sec, link_info, is_relaxable_p)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     bfd_boolean *is_relaxable_p;
{
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents;
  bfd_boolean ok = TRUE;
  unsigned i;

  internal_relocs = retrieve_internal_relocs (abfd, sec,
					      link_info->keep_memory);
  if (internal_relocs == NULL) 
    return ok;

  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec->_raw_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  for (i = 0; i < sec->reloc_count; i++) 
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      r_reloc r_rel;
      asection *target_sec;
      xtensa_relax_info *target_relax_info;

      r_reloc_init (&r_rel, abfd, irel);

      target_sec = r_reloc_get_section (&r_rel);
      target_relax_info = get_xtensa_relax_info (target_sec);
      if (!target_relax_info)
	continue;

      /* Count relocations against the target section.  */
      target_relax_info->src_count++;

      if (is_literal_section (target_sec)
	  && is_l32r_relocation (sec, contents, irel)
	  && r_reloc_is_defined (&r_rel))
	{
	  /* Mark the target section as relaxable.  */
	  target_relax_info->is_relaxable_literal_section = TRUE;
	  *is_relaxable_p = TRUE;
	}
    }

 error_return:
  release_contents (sec, contents);
  release_internal_relocs (sec, internal_relocs);
  return ok;
}


/* Record _all_ the relocations that point to relaxable literal
   sections, and get rid of ASM_EXPAND relocs by either converting them
   to ASM_SIMPLIFY or by removing them.  */

static bfd_boolean
collect_source_relocs (abfd, sec, link_info)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
{
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents;
  bfd_boolean ok = TRUE;
  unsigned i;

  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);
  if (internal_relocs == NULL) 
    return ok;

  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec->_raw_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  /* Record relocations against relaxable literal sections.  */
  for (i = 0; i < sec->reloc_count; i++) 
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      r_reloc r_rel;
      asection *target_sec;
      xtensa_relax_info *target_relax_info;

      r_reloc_init (&r_rel, abfd, irel);

      target_sec = r_reloc_get_section (&r_rel);
      target_relax_info = get_xtensa_relax_info (target_sec);

      if (target_relax_info
	  && target_relax_info->is_relaxable_literal_section)
	{
	  xtensa_opcode opcode;
	  xtensa_operand opnd;
	  source_reloc *s_reloc;
	  int src_next;

	  src_next = target_relax_info->src_next++;
	  s_reloc = &target_relax_info->src_relocs[src_next];

	  opcode = get_relocation_opcode (sec, contents, irel);
	  if (opcode == XTENSA_UNDEFINED)
	    opnd = NULL;
	  else
	    opnd = xtensa_get_operand (xtensa_default_isa, opcode,
				       get_relocation_opnd (irel));

	  init_source_reloc (s_reloc, sec, &r_rel, opnd);
	}
    }

  /* Now get rid of ASM_EXPAND relocations.  At this point, the
     src_relocs array for the target literal section may still be
     incomplete, but it must at least contain the entries for the L32R
     relocations associated with ASM_EXPANDs because they were just
     added in the preceding loop over the relocations.  */

  for (i = 0; i < sec->reloc_count; i++) 
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      bfd_boolean is_reachable;

      if (!is_resolvable_asm_expansion (abfd, sec, contents, irel, link_info,
					&is_reachable))
	continue;

      if (is_reachable)
	{
	  Elf_Internal_Rela *l32r_irel;
	  r_reloc r_rel;
	  asection *target_sec;
	  xtensa_relax_info *target_relax_info;

	  /* Mark the source_reloc for the L32R so that it will be
	     removed in remove_literals(), along with the associated
	     literal.  */
	  l32r_irel = find_associated_l32r_irel (sec, contents,
						 irel, internal_relocs);
	  if (l32r_irel == NULL)
	    continue;

	  r_reloc_init (&r_rel, abfd, l32r_irel);

	  target_sec = r_reloc_get_section (&r_rel);
	  target_relax_info = get_xtensa_relax_info (target_sec);

	  if (target_relax_info
	      && target_relax_info->is_relaxable_literal_section)
	    {
	      source_reloc *s_reloc;

	      /* Search the source_relocs for the entry corresponding to
		 the l32r_irel.  Note: The src_relocs array is not yet
		 sorted, but it wouldn't matter anyway because we're
		 searching by source offset instead of target offset.  */
	      s_reloc = find_source_reloc (target_relax_info->src_relocs, 
					   target_relax_info->src_next,
					   sec, l32r_irel);
	      BFD_ASSERT (s_reloc);
	      s_reloc->is_null = TRUE;
	    }

	  /* Convert this reloc to ASM_SIMPLIFY.  */
	  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
				       R_XTENSA_ASM_SIMPLIFY);
	  l32r_irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);

	  pin_internal_relocs (sec, internal_relocs);
	}
      else
	{
	  /* It is resolvable but doesn't reach.  We resolve now
	     by eliminating the relocation -- the call will remain
	     expanded into L32R/CALLX.  */
	  irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
	  pin_internal_relocs (sec, internal_relocs);
	}
    }

 error_return:
  release_contents (sec, contents);
  release_internal_relocs (sec, internal_relocs);
  return ok;
}


/* Return TRUE if the asm expansion can be resolved.  Generally it can
   be resolved on a final link or when a partial link locates it in the
   same section as the target.  Set "is_reachable" flag if the target of
   the call is within the range of a direct call, given the current VMA
   for this section and the target section.  */

bfd_boolean
is_resolvable_asm_expansion (abfd, sec, contents, irel, link_info,
			     is_reachable_p)
     bfd *abfd;
     asection *sec;
     bfd_byte *contents;
     Elf_Internal_Rela *irel;
     struct bfd_link_info *link_info;
     bfd_boolean *is_reachable_p;
{
  asection *target_sec;
  bfd_vma target_offset;
  r_reloc r_rel;
  xtensa_opcode opcode, direct_call_opcode;
  bfd_vma self_address;
  bfd_vma dest_address;

  *is_reachable_p = FALSE;

  if (contents == NULL)
    return FALSE;

  if (ELF32_R_TYPE (irel->r_info) != R_XTENSA_ASM_EXPAND) 
    return FALSE;
  
  opcode = get_expanded_call_opcode (contents + irel->r_offset,
				     sec->_raw_size - irel->r_offset);
  
  direct_call_opcode = swap_callx_for_call_opcode (opcode);
  if (direct_call_opcode == XTENSA_UNDEFINED)
    return FALSE;

  /* Check and see that the target resolves.  */
  r_reloc_init (&r_rel, abfd, irel);
  if (!r_reloc_is_defined (&r_rel))
    return FALSE;

  target_sec = r_reloc_get_section (&r_rel);
  target_offset = r_reloc_get_target_offset (&r_rel);

  /* If the target is in a shared library, then it doesn't reach.  This
     isn't supposed to come up because the compiler should never generate
     non-PIC calls on systems that use shared libraries, but the linker
     shouldn't crash regardless.  */
  if (!target_sec->output_section)
    return FALSE;
      
  /* For relocatable sections, we can only simplify when the output
     section of the target is the same as the output section of the
     source.  */
  if (link_info->relocatable
      && (target_sec->output_section != sec->output_section))
    return FALSE;

  self_address = (sec->output_section->vma
		  + sec->output_offset + irel->r_offset + 3);
  dest_address = (target_sec->output_section->vma
		  + target_sec->output_offset + target_offset);
      
  *is_reachable_p = pcrel_reloc_fits
    (xtensa_get_operand (xtensa_default_isa, direct_call_opcode, 0),
     self_address, dest_address);

  if ((self_address >> CALL_SEGMENT_BITS) !=
      (dest_address >> CALL_SEGMENT_BITS))
    return FALSE;

  return TRUE;
}


static Elf_Internal_Rela *
find_associated_l32r_irel (sec, contents, other_irel, internal_relocs)
     asection *sec;
     bfd_byte *contents;
     Elf_Internal_Rela *other_irel;
     Elf_Internal_Rela *internal_relocs;
{
  unsigned i;

  for (i = 0; i < sec->reloc_count; i++) 
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];

      if (irel == other_irel)
	continue;
      if (irel->r_offset != other_irel->r_offset)
	continue;
      if (is_l32r_relocation (sec, contents, irel))
	return irel;
    }

  return NULL;
}

/* First relaxation pass.  */

/* If the section is relaxable (i.e., a literal section), check each
   literal to see if it has the same value as another literal that has
   already been seen, either in the current section or a previous one.
   If so, add an entry to the per-section list of removed literals.  The
   actual changes are deferred until the next pass.  */

static bfd_boolean 
remove_literals (abfd, sec, link_info, values)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     value_map_hash_table *values;
{
  xtensa_relax_info *relax_info;
  bfd_byte *contents;
  Elf_Internal_Rela *internal_relocs;
  source_reloc *src_relocs;
  bfd_boolean final_static_link;
  bfd_boolean ok = TRUE;
  int i;

  /* Do nothing if it is not a relaxable literal section.  */
  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info);

  if (!relax_info->is_relaxable_literal_section)
    return ok;

  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);

  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec->_raw_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  final_static_link =
    (!link_info->relocatable
     && !elf_hash_table (link_info)->dynamic_sections_created);

  /* Sort the source_relocs by target offset.  */
  src_relocs = relax_info->src_relocs;
  qsort (src_relocs, relax_info->src_count,
	 sizeof (source_reloc), source_reloc_compare);

  for (i = 0; i < relax_info->src_count; i++)
    {
      source_reloc *rel;
      Elf_Internal_Rela *irel = NULL;
      literal_value val;
      value_map *val_map;

      rel = &src_relocs[i];
      irel = get_irel_at_offset (sec, internal_relocs,
				 rel->r_rel.target_offset);

      /* If the target_offset for this relocation is the same as the
	 previous relocation, then we've already considered whether the
	 literal can be coalesced.  Skip to the next one....  */
      if (i != 0 && (src_relocs[i-1].r_rel.target_offset
		     == rel->r_rel.target_offset))
	continue;

      /* Check if the relocation was from an L32R that is being removed
	 because a CALLX was converted to a direct CALL, and check if
	 there are no other relocations to the literal.  */
      if (rel->is_null
	  && (i == relax_info->src_count - 1
	      || (src_relocs[i+1].r_rel.target_offset
		  != rel->r_rel.target_offset)))
	{
	  /* Mark the unused literal so that it will be removed.  */
	  add_removed_literal (&relax_info->removed_list, &rel->r_rel, NULL);

	  /* Zero out the relocation on this literal location.  */
	  if (irel)
	    {
	      if (elf_hash_table (link_info)->dynamic_sections_created)
		shrink_dynamic_reloc_sections (link_info, abfd, sec, irel);

	      irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
	    }

	  continue;
	}

      /* Find the literal value.  */
      r_reloc_init (&val.r_rel, abfd, irel);
      BFD_ASSERT (rel->r_rel.target_offset < sec->_raw_size);
      val.value = bfd_get_32 (abfd, contents + rel->r_rel.target_offset);
          
      /* Check if we've seen another literal with the same value.  */
      val_map = get_cached_value (values, &val, final_static_link);
      if (val_map != NULL) 
	{
	  /* First check that THIS and all the other relocs to this
	     literal will FIT if we move them to the new address.  */

	  if (relocations_reach (rel, relax_info->src_count - i,
				 &val_map->loc))
	    {
	      /* Mark that the literal will be coalesced.  */
	      add_removed_literal (&relax_info->removed_list,
				   &rel->r_rel, &val_map->loc);
	    }
	  else
	    {
	      /* Relocations do not reach -- do not remove this literal.  */
	      val_map->loc = rel->r_rel;
	    }
	}
      else
	{
	  /* This is the first time we've seen this literal value.  */
	  BFD_ASSERT (sec == r_reloc_get_section (&rel->r_rel));
	  add_value_map (values, &val, &rel->r_rel, final_static_link);
	}
    }

error_return:
  release_contents (sec, contents);
  release_internal_relocs (sec, internal_relocs);
  return ok;
}


/* Check if the original relocations (presumably on L32R instructions)
   identified by reloc[0..N] can be changed to reference the literal
   identified by r_rel.  If r_rel is out of range for any of the
   original relocations, then we don't want to coalesce the original
   literal with the one at r_rel.  We only check reloc[0..N], where the
   offsets are all the same as for reloc[0] (i.e., they're all
   referencing the same literal) and where N is also bounded by the
   number of remaining entries in the "reloc" array.  The "reloc" array
   is sorted by target offset so we know all the entries for the same
   literal will be contiguous.  */

static bfd_boolean
relocations_reach (reloc, remaining_relocs, r_rel)
     source_reloc *reloc;
     int remaining_relocs;
     const r_reloc *r_rel;
{
  bfd_vma from_offset, source_address, dest_address;
  asection *sec;
  int i;

  if (!r_reloc_is_defined (r_rel))
    return FALSE;

  sec = r_reloc_get_section (r_rel);
  from_offset = reloc[0].r_rel.target_offset;

  for (i = 0; i < remaining_relocs; i++)
    {
      if (reloc[i].r_rel.target_offset != from_offset)
	break;

      /* Ignore relocations that have been removed.  */
      if (reloc[i].is_null)
	continue;

      /* The original and new output section for these must be the same
         in order to coalesce.  */
      if (r_reloc_get_section (&reloc[i].r_rel)->output_section
	  != sec->output_section)
	return FALSE;

      /* A NULL operand means it is not a PC-relative relocation, so
         the literal can be moved anywhere.  */
      if (reloc[i].opnd)
	{
	  /* Otherwise, check to see that it fits.  */
	  source_address = (reloc[i].source_sec->output_section->vma
			    + reloc[i].source_sec->output_offset
			    + reloc[i].r_rel.rela.r_offset);
	  dest_address = (sec->output_section->vma
			  + sec->output_offset
			  + r_rel->target_offset);

	  if (!pcrel_reloc_fits (reloc[i].opnd, source_address, dest_address))
	    return FALSE;
	}
    }

  return TRUE;
}


/* WARNING: linear search here.  If the relocation are in order by
   address, we can use a faster binary search.  ALSO, we assume that
   there is only 1 non-NONE relocation per address.  */

static Elf_Internal_Rela *
get_irel_at_offset (sec, internal_relocs, offset)
     asection *sec;
     Elf_Internal_Rela *internal_relocs;
     bfd_vma offset;
{
  unsigned i;
  if (!internal_relocs) 
    return NULL;
  for (i = 0; i < sec->reloc_count; i++)
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      if (irel->r_offset == offset
	  && ELF32_R_TYPE (irel->r_info) != R_XTENSA_NONE)
	return irel;
    }
  return NULL;
}


/* Second relaxation pass.  */

/* Modify all of the relocations to point to the right spot, and if this
   is a relaxable section, delete the unwanted literals and fix the
   cooked_size.  */

bfd_boolean 
relax_section (abfd, sec, link_info)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
{
  Elf_Internal_Rela *internal_relocs;
  xtensa_relax_info *relax_info;
  bfd_byte *contents;
  bfd_boolean ok = TRUE;
  unsigned i;

  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info);

  /* Handle property sections (e.g., literal tables) specially.  */
  if (xtensa_is_property_section (sec))
    {
      BFD_ASSERT (!relax_info->is_relaxable_literal_section);
      return relax_property_section (abfd, sec, link_info);
    }

  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec->_raw_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  if (internal_relocs)
    {
      for (i = 0; i < sec->reloc_count; i++) 
	{
	  Elf_Internal_Rela *irel;
	  xtensa_relax_info *target_relax_info;
	  bfd_vma source_offset;
	  r_reloc r_rel;
	  unsigned r_type;
	  asection *target_sec;

	  /* Locally change the source address.
	     Translate the target to the new target address.
	     If it points to this section and has been removed,
	     NULLify it.
	     Write it back.  */

	  irel = &internal_relocs[i];
	  source_offset = irel->r_offset;

	  r_type = ELF32_R_TYPE (irel->r_info);
	  r_reloc_init (&r_rel, abfd, irel);
	
	  if (relax_info->is_relaxable_literal_section)
	    {
	      if (r_type != R_XTENSA_NONE
		  && find_removed_literal (&relax_info->removed_list,
					   irel->r_offset))
		{
		  /* Remove this relocation.  */
		  if (elf_hash_table (link_info)->dynamic_sections_created)
		    shrink_dynamic_reloc_sections (link_info, abfd, sec, irel);
		  irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
		  irel->r_offset = offset_with_removed_literals
		    (&relax_info->removed_list, irel->r_offset);
		  continue;
		}
	      source_offset =
		offset_with_removed_literals (&relax_info->removed_list,
					      irel->r_offset);
	      irel->r_offset = source_offset;
	    }

	  target_sec = r_reloc_get_section (&r_rel);
	  target_relax_info = get_xtensa_relax_info (target_sec);

	  if (target_relax_info
	      && target_relax_info->is_relaxable_literal_section)
	    {
	      r_reloc new_rel;
	      reloc_bfd_fix *fix;

	      translate_reloc (&r_rel, &new_rel);

	      /* FIXME: If the relocation still references a section in
		 the same input file, the relocation should be modified
		 directly instead of adding a "fix" record.  */

	      fix = reloc_bfd_fix_init (sec, source_offset, r_type, 0,
					r_reloc_get_section (&new_rel),
					new_rel.target_offset);
	      add_fix (sec, fix);
	    }

	  pin_internal_relocs (sec, internal_relocs);
	}
    }

  if (relax_info->is_relaxable_literal_section)
    {
      /* Walk through the contents and delete literals that are not needed 
         anymore.  */

      unsigned long size = sec->_cooked_size;
      unsigned long removed = 0;

      removed_literal *reloc = relax_info->removed_list.head;
      for (; reloc; reloc = reloc->next) 
	{
	  unsigned long upper = sec->_raw_size;
	  bfd_vma start = reloc->from.target_offset + 4;
	  if (reloc->next)
	    upper = reloc->next->from.target_offset;
	  if (upper - start != 0) 
	    {
	      BFD_ASSERT (start <= upper);
	      memmove (contents + start - removed - 4,
		       contents + start,
		       upper - start );
	      pin_contents (sec, contents);
	    }
	  removed += 4;
	  size -= 4;
	}

      /* Change the section size.  */
      sec->_cooked_size = size;
      /* Also shrink _raw_size.  (The code in relocate_section that
	 checks that relocations are within the section must use
	 _raw_size because of the way the stabs sections are relaxed;
	 shrinking _raw_size means that these checks will not be
	 unnecessarily lax.)  */
      sec->_raw_size = size;
    }
  
 error_return:
  release_internal_relocs (sec, internal_relocs);
  release_contents (sec, contents);
  return ok;
}


/* Fix up a relocation to take account of removed literals.  */

static void
translate_reloc (orig_rel, new_rel)
     const r_reloc *orig_rel;
     r_reloc *new_rel;
{
  asection *sec;
  xtensa_relax_info *relax_info;
  removed_literal *removed;
  unsigned long new_offset;

  *new_rel = *orig_rel;

  if (!r_reloc_is_defined (orig_rel))
    return;
  sec = r_reloc_get_section (orig_rel);

  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info);

  if (!relax_info->is_relaxable_literal_section)
    return;

  /* Check if the original relocation is against a literal being removed.  */
  removed = find_removed_literal (&relax_info->removed_list,
				  orig_rel->target_offset);
  if (removed) 
    {
      asection *new_sec;

      /* The fact that there is still a relocation to this literal indicates
	 that the literal is being coalesced, not simply removed.  */
      BFD_ASSERT (removed->to.abfd != NULL);

      /* This was moved to some other address (possibly in another section). */
      *new_rel = removed->to;
      new_sec = r_reloc_get_section (new_rel);
      if (new_sec != sec) 
	{
	  sec = new_sec;
	  relax_info = get_xtensa_relax_info (sec);
	  if (!relax_info || !relax_info->is_relaxable_literal_section)
	    return;
	}
    }

  /* ...and the target address may have been moved within its section.  */
  new_offset = offset_with_removed_literals (&relax_info->removed_list,
					     new_rel->target_offset);

  /* Modify the offset and addend.  */
  new_rel->target_offset = new_offset;
  new_rel->rela.r_addend += (new_offset - new_rel->target_offset);
}


/* For dynamic links, there may be a dynamic relocation for each
   literal.  The number of dynamic relocations must be computed in
   size_dynamic_sections, which occurs before relaxation.  When a
   literal is removed, this function checks if there is a corresponding
   dynamic relocation and shrinks the size of the appropriate dynamic
   relocation section accordingly.  At this point, the contents of the
   dynamic relocation sections have not yet been filled in, so there's
   nothing else that needs to be done.  */

static void
shrink_dynamic_reloc_sections (info, abfd, input_section, rel)
     struct bfd_link_info *info;
     bfd *abfd;
     asection *input_section;
     Elf_Internal_Rela *rel;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  unsigned long r_symndx;
  int r_type;
  struct elf_link_hash_entry *h;
  bfd_boolean dynamic_symbol;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);

  r_type = ELF32_R_TYPE (rel->r_info);
  r_symndx = ELF32_R_SYM (rel->r_info);

  if (r_symndx < symtab_hdr->sh_info)
    h = NULL;
  else
    h = sym_hashes[r_symndx - symtab_hdr->sh_info];

  dynamic_symbol = xtensa_elf_dynamic_symbol_p (h, info);

  if ((r_type == R_XTENSA_32 || r_type == R_XTENSA_PLT)
      && (input_section->flags & SEC_ALLOC) != 0
      && (dynamic_symbol || info->shared))
    {
      bfd *dynobj;
      const char *srel_name;
      asection *srel;
      bfd_boolean is_plt = FALSE;

      dynobj = elf_hash_table (info)->dynobj;
      BFD_ASSERT (dynobj != NULL);

      if (dynamic_symbol && r_type == R_XTENSA_PLT)
	{
	  srel_name = ".rela.plt";
	  is_plt = TRUE;
	}
      else
	srel_name = ".rela.got";

      /* Reduce size of the .rela.* section by one reloc.  */
      srel = bfd_get_section_by_name (dynobj, srel_name);
      BFD_ASSERT (srel != NULL);
      BFD_ASSERT (srel->_cooked_size >= sizeof (Elf32_External_Rela));
      srel->_cooked_size -= sizeof (Elf32_External_Rela);

      /* Also shrink _raw_size.  (This seems wrong but other bfd code seems
	 to assume that linker-created sections will never be relaxed and
	 hence _raw_size must always equal _cooked_size.) */
      srel->_raw_size = srel->_cooked_size;

      if (is_plt)
	{
	  asection *splt, *sgotplt, *srelgot;
	  int reloc_index, chunk;

	  /* Find the PLT reloc index of the entry being removed.  This
	     is computed from the size of ".rela.plt".  It is needed to
	     figure out which PLT chunk to resize.  Usually "last index
	     = size - 1" since the index starts at zero, but in this
	     context, the size has just been decremented so there's no
	     need to subtract one.  */
	  reloc_index = srel->_cooked_size / sizeof (Elf32_External_Rela);

	  chunk = reloc_index / PLT_ENTRIES_PER_CHUNK;
	  splt = elf_xtensa_get_plt_section (dynobj, chunk);
	  sgotplt = elf_xtensa_get_gotplt_section (dynobj, chunk);
	  BFD_ASSERT (splt != NULL && sgotplt != NULL);

	  /* Check if an entire PLT chunk has just been eliminated.  */
	  if (reloc_index % PLT_ENTRIES_PER_CHUNK == 0)
	    {
	      /* The two magic GOT entries for that chunk can go away.  */
	      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
	      BFD_ASSERT (srelgot != NULL);
	      srelgot->reloc_count -= 2;
	      srelgot->_cooked_size -= 2 * sizeof (Elf32_External_Rela);
	      /* Shrink _raw_size (see comment above).  */
	      srelgot->_raw_size = srelgot->_cooked_size;

	      sgotplt->_cooked_size -= 8;

	      /* There should be only one entry left (and it will be
		 removed below).  */
	      BFD_ASSERT (sgotplt->_cooked_size == 4);
	      BFD_ASSERT (splt->_cooked_size == PLT_ENTRY_SIZE);
	    }

	  BFD_ASSERT (sgotplt->_cooked_size >= 4);
	  BFD_ASSERT (splt->_cooked_size >= PLT_ENTRY_SIZE);

	  sgotplt->_cooked_size -= 4;
	  splt->_cooked_size -= PLT_ENTRY_SIZE;

	  /* Shrink _raw_sizes (see comment above).  */
	  sgotplt->_raw_size = sgotplt->_cooked_size;
	  splt->_raw_size = splt->_cooked_size;
	}
    }
}


/* This is similar to relax_section except that when a target is moved,
   we shift addresses up.  We also need to modify the size.  This
   algorithm does NOT allow for relocations into the middle of the
   property sections.  */

static bfd_boolean 
relax_property_section (abfd, sec, link_info)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
{
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents;
  unsigned i, nexti;
  bfd_boolean ok = TRUE;

  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec->_raw_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  if (internal_relocs) 
    {
      for (i = 0; i < sec->reloc_count; i++) 
	{
	  Elf_Internal_Rela *irel;
	  xtensa_relax_info *target_relax_info;
	  r_reloc r_rel;
	  unsigned r_type;
	  asection *target_sec;

	  /* Locally change the source address.
	     Translate the target to the new target address.
	     If it points to this section and has been removed, MOVE IT.
	     Also, don't forget to modify the associated SIZE at
	     (offset + 4).  */

	  irel = &internal_relocs[i];
	  r_type = ELF32_R_TYPE (irel->r_info);
	  if (r_type == R_XTENSA_NONE)
	    continue;

	  r_reloc_init (&r_rel, abfd, irel);

	  target_sec = r_reloc_get_section (&r_rel);
	  target_relax_info = get_xtensa_relax_info (target_sec);

	  if (target_relax_info
	      && target_relax_info->is_relaxable_literal_section)
	    {
	      /* Translate the relocation's destination.  */
	      bfd_vma new_offset;
	      bfd_vma new_end_offset;
	      bfd_byte *size_p;
	      long old_size, new_size;

	      new_offset =
		offset_with_removed_literals (&target_relax_info->removed_list,
					      r_rel.target_offset);

	      /* Assert that we are not out of bounds.  */
	      size_p = &contents[irel->r_offset + 4];
	      old_size = bfd_get_32 (abfd, &contents[irel->r_offset + 4]);

	      new_end_offset =
		offset_with_removed_literals (&target_relax_info->removed_list,
					      r_rel.target_offset + old_size);
	      
	      new_size = new_end_offset - new_offset;
	      if (new_size != old_size)
		{
		  bfd_put_32 (abfd, new_size, size_p);
		  pin_contents (sec, contents);
		}
	      
	      if (new_offset != r_rel.target_offset)
		{
		  bfd_vma diff = new_offset - r_rel.target_offset;
		  irel->r_addend += diff;
		  pin_internal_relocs (sec, internal_relocs);
		}
	    }
	}
    }

  /* Combine adjacent property table entries.  This is also done in
     finish_dynamic_sections() but at that point it's too late to
     reclaim the space in the output section, so we do this twice.  */

  if (internal_relocs)
    {
      Elf_Internal_Rela *last_irel = NULL;
      int removed_bytes = 0;
      bfd_vma offset, last_irel_offset;
      bfd_vma section_size;

      /* Walk over memory and irels at the same time.
         This REQUIRES that the internal_relocs be sorted by offset.  */
      qsort (internal_relocs, sec->reloc_count, sizeof (Elf_Internal_Rela),
	     internal_reloc_compare);
      nexti = 0; /* Index into internal_relocs.  */

      pin_internal_relocs (sec, internal_relocs);
      pin_contents (sec, contents);

      last_irel_offset = (bfd_vma) -1;
      section_size = (sec->_cooked_size ? sec->_cooked_size : sec->_raw_size);
      BFD_ASSERT (section_size % 8 == 0);

      for (offset = 0; offset < section_size; offset += 8)
	{
	  Elf_Internal_Rela *irel, *next_irel;
	  bfd_vma bytes_to_remove, size, actual_offset;
	  bfd_boolean remove_this_irel;

	  irel = NULL;
	  next_irel = NULL;

	  /* Find the next two relocations (if there are that many left),
	     skipping over any R_XTENSA_NONE relocs.  On entry, "nexti" is
	     the starting reloc index.  After these two loops, "i"
	     is the index of the first non-NONE reloc past that starting
	     index, and "nexti" is the index for the next non-NONE reloc
	     after "i".  */

	  for (i = nexti; i < sec->reloc_count; i++)
	    {
	      if (ELF32_R_TYPE (internal_relocs[i].r_info) != R_XTENSA_NONE)
		{
		  irel = &internal_relocs[i];
		  break;
		}
	      internal_relocs[i].r_offset -= removed_bytes;
	    }

	  for (nexti = i + 1; nexti < sec->reloc_count; nexti++)
	    {
	      if (ELF32_R_TYPE (internal_relocs[nexti].r_info)
		  != R_XTENSA_NONE)
		{
		  next_irel = &internal_relocs[nexti];
		  break;
		}
	      internal_relocs[nexti].r_offset -= removed_bytes;
	    }

	  remove_this_irel = FALSE;
	  bytes_to_remove = 0;
	  actual_offset = offset - removed_bytes;
	  size = bfd_get_32 (abfd, &contents[actual_offset + 4]);

	  /* Check that the irels are sorted by offset,
	     with only one per address.  */
	  BFD_ASSERT (!irel || (int) irel->r_offset > (int) last_irel_offset); 
	  BFD_ASSERT (!next_irel || next_irel->r_offset > irel->r_offset);

	  /* Make sure there isn't a reloc on the size field.  */
	  if (irel && irel->r_offset == offset + 4)
	    {
	      irel->r_offset -= removed_bytes;
	      last_irel_offset = irel->r_offset;
	    }
	  else if (next_irel && next_irel->r_offset == offset + 4)
	    {
	      nexti += 1;
	      irel->r_offset -= removed_bytes;
	      next_irel->r_offset -= removed_bytes;
	      last_irel_offset = next_irel->r_offset;
	    }
	  else if (size == 0)
	    {
	      /* Always remove entries with zero size.  */
	      bytes_to_remove = 8;
	      if (irel && irel->r_offset == offset)
		{
		  remove_this_irel = TRUE;

		  irel->r_offset -= removed_bytes;
		  last_irel_offset = irel->r_offset;
		}
	    }
	  else if (irel && irel->r_offset == offset)
	    {
	      if (ELF32_R_TYPE (irel->r_info) == R_XTENSA_32)
		{
		  if (last_irel)
		    {
		      bfd_vma old_size = 
			bfd_get_32 (abfd, &contents[last_irel->r_offset + 4]);
		      bfd_vma old_address = 
			(last_irel->r_addend 
			 + bfd_get_32 (abfd, &contents[last_irel->r_offset]));
		      bfd_vma new_address = 
			(irel->r_addend 
			 + bfd_get_32 (abfd, &contents[actual_offset]));

		      if ((ELF32_R_SYM (irel->r_info) ==
			   ELF32_R_SYM (last_irel->r_info))
			  && (old_address + old_size == new_address)) 
			{
			  /* fix the old size */
			  bfd_put_32 (abfd, old_size + size,
				      &contents[last_irel->r_offset + 4]);
			  bytes_to_remove = 8;
			  remove_this_irel = TRUE;
			}
		      else
			last_irel = irel;
		    }
		  else
		    last_irel = irel;
		}

	      irel->r_offset -= removed_bytes;
	      last_irel_offset = irel->r_offset;
	    }

	  if (remove_this_irel)
	    {
	      irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
	      irel->r_offset -= bytes_to_remove;
	    }

	  if (bytes_to_remove != 0)
	    {
	      removed_bytes += bytes_to_remove;
	      if (offset + 8 < section_size)
		memmove (&contents[actual_offset],
			 &contents[actual_offset+8],
			 section_size - offset - 8);
	    }
	}

      if (removed_bytes) 
	{
	  /* Clear the removed bytes.  */
	  memset (&contents[section_size - removed_bytes], 0, removed_bytes);

	  sec->_cooked_size = section_size - removed_bytes;
	  /* Also shrink _raw_size.  (The code in relocate_section that
	     checks that relocations are within the section must use
	     _raw_size because of the way the stabs sections are
	     relaxed; shrinking _raw_size means that these checks will
	     not be unnecessarily lax.)  */
	  sec->_raw_size = sec->_cooked_size;

	  if (xtensa_is_littable_section (sec))
	    {
	      bfd *dynobj = elf_hash_table (link_info)->dynobj;
	      if (dynobj)
		{
		  asection *sgotloc =
		    bfd_get_section_by_name (dynobj, ".got.loc");
		  if (sgotloc)
		    {
		      bfd_size_type sgotloc_size =
			(sgotloc->_cooked_size ? sgotloc->_cooked_size
			 : sgotloc->_raw_size);
		      sgotloc->_cooked_size = sgotloc_size - removed_bytes;
		      sgotloc->_raw_size = sgotloc_size - removed_bytes;
		    }
		}
	    }
	}
    }

 error_return:
  release_internal_relocs (sec, internal_relocs);
  release_contents (sec, contents);
  return ok;
}


/* Third relaxation pass.  */

/* Change symbol values to account for removed literals.  */

bfd_boolean 
relax_section_symbols (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  xtensa_relax_info *relax_info;
  unsigned int sec_shndx;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Sym *isymbuf;
  unsigned i, num_syms, num_locals;

  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info);

  if (!relax_info->is_relaxable_literal_section)
    return TRUE;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isymbuf = retrieve_local_syms (abfd);

  num_syms = symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
  num_locals = symtab_hdr->sh_info;

  /* Adjust the local symbols defined in this section.  */
  for (i = 0; i < num_locals; i++)
    {
      Elf_Internal_Sym *isym = &isymbuf[i];

      if (isym->st_shndx == sec_shndx)
	{
	  bfd_vma new_address = offset_with_removed_literals
	    (&relax_info->removed_list, isym->st_value);
	  if (new_address != isym->st_value)
	    isym->st_value = new_address;
	}
    }

  /* Now adjust the global symbols defined in this section.  */
  for (i = 0; i < (num_syms - num_locals); i++)
    {
      struct elf_link_hash_entry *sym_hash;

      sym_hash = elf_sym_hashes (abfd)[i];

      if (sym_hash->root.type == bfd_link_hash_warning)
	sym_hash = (struct elf_link_hash_entry *) sym_hash->root.u.i.link;

      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec)
	{
	  bfd_vma new_address = offset_with_removed_literals
	    (&relax_info->removed_list, sym_hash->root.u.def.value);
	  if (new_address != sym_hash->root.u.def.value)
	    sym_hash->root.u.def.value = new_address;
	}
    }

  return TRUE;
}


/* "Fix" handling functions, called while performing relocations.  */

static void
do_fix_for_relocatable_link (rel, input_bfd, input_section)
     Elf_Internal_Rela *rel;
     bfd *input_bfd;
     asection *input_section;
{
  r_reloc r_rel;
  asection *sec, *old_sec;
  bfd_vma old_offset;
  int r_type = ELF32_R_TYPE (rel->r_info);
  reloc_bfd_fix *fix_list;
  reloc_bfd_fix *fix;

  if (r_type == R_XTENSA_NONE)
    return;

  fix_list = (get_xtensa_relax_info (input_section))->fix_list;
  if (fix_list == NULL)
    return;

  fix = get_bfd_fix (fix_list, input_section, rel->r_offset, r_type);
  if (fix == NULL)
    return;

  r_reloc_init (&r_rel, input_bfd, rel);
  old_sec = r_reloc_get_section (&r_rel);
  old_offset = r_reloc_get_target_offset (&r_rel);
	      
  if (old_sec == NULL || !r_reloc_is_defined (&r_rel))
    {
      BFD_ASSERT (r_type == R_XTENSA_ASM_EXPAND);
      /* Leave it be.  Resolution will happen in a later stage.  */
    }
  else
    {
      sec = fix->target_sec;
      rel->r_addend += ((sec->output_offset + fix->target_offset)
			- (old_sec->output_offset + old_offset));
    }
}


static void
do_fix_for_final_link (rel, input_section, relocationp)
     Elf_Internal_Rela *rel;
     asection *input_section;
     bfd_vma *relocationp;
{
  asection *sec;
  int r_type = ELF32_R_TYPE (rel->r_info);
  reloc_bfd_fix *fix_list;
  reloc_bfd_fix *fix;

  if (r_type == R_XTENSA_NONE)
    return;

  fix_list = (get_xtensa_relax_info (input_section))->fix_list;
  if (fix_list == NULL)
    return;

  fix = get_bfd_fix (fix_list, input_section, rel->r_offset, r_type);
  if (fix == NULL)
    return;

  sec = fix->target_sec;
  *relocationp = (sec->output_section->vma
		  + sec->output_offset
		  + fix->target_offset - rel->r_addend);
}


/* Miscellaneous utility functions....  */

static asection *
elf_xtensa_get_plt_section (dynobj, chunk)
     bfd *dynobj;
     int chunk;
{
  char plt_name[10];

  if (chunk == 0)
    return bfd_get_section_by_name (dynobj, ".plt");

  sprintf (plt_name, ".plt.%u", chunk);
  return bfd_get_section_by_name (dynobj, plt_name);
}


static asection *
elf_xtensa_get_gotplt_section (dynobj, chunk)
     bfd *dynobj;
     int chunk;
{
  char got_name[14];

  if (chunk == 0)
    return bfd_get_section_by_name (dynobj, ".got.plt");

  sprintf (got_name, ".got.plt.%u", chunk);
  return bfd_get_section_by_name (dynobj, got_name);
}


/* Get the input section for a given symbol index.
   If the symbol is:
   . a section symbol, return the section;
   . a common symbol, return the common section;
   . an undefined symbol, return the undefined section;
   . an indirect symbol, follow the links;
   . an absolute value, return the absolute section.  */

static asection *
get_elf_r_symndx_section (abfd, r_symndx)
     bfd *abfd;
     unsigned long r_symndx;
{
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  asection *target_sec = NULL;
  if (r_symndx < symtab_hdr->sh_info) 
    {
      Elf_Internal_Sym *isymbuf;
      unsigned int section_index;

      isymbuf = retrieve_local_syms (abfd);
      section_index = isymbuf[r_symndx].st_shndx;

      if (section_index == SHN_UNDEF)
	target_sec = bfd_und_section_ptr;
      else if (section_index > 0 && section_index < SHN_LORESERVE)
	target_sec = bfd_section_from_elf_index (abfd, section_index);
      else if (section_index == SHN_ABS)
	target_sec = bfd_abs_section_ptr;
      else if (section_index == SHN_COMMON)
	target_sec = bfd_com_section_ptr;
      else 
	/* Who knows?  */
	target_sec = NULL;
    }
  else
    {
      unsigned long indx = r_symndx - symtab_hdr->sh_info;
      struct elf_link_hash_entry *h = elf_sym_hashes (abfd)[indx];

      while (h->root.type == bfd_link_hash_indirect
             || h->root.type == bfd_link_hash_warning)
        h = (struct elf_link_hash_entry *) h->root.u.i.link;

      switch (h->root.type)
	{
	case bfd_link_hash_defined:
	case  bfd_link_hash_defweak:
	  target_sec = h->root.u.def.section;
	  break;
	case bfd_link_hash_common:
	  target_sec = bfd_com_section_ptr;
	  break;
	case bfd_link_hash_undefined:
	case bfd_link_hash_undefweak:
	  target_sec = bfd_und_section_ptr;
	  break;
	default: /* New indirect warning.  */
	  target_sec = bfd_und_section_ptr;
	  break;
	}
    }
  return target_sec;
}


static struct elf_link_hash_entry *
get_elf_r_symndx_hash_entry (abfd, r_symndx)
     bfd *abfd;
     unsigned long r_symndx;
{
  unsigned long indx;
  struct elf_link_hash_entry *h;
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  if (r_symndx < symtab_hdr->sh_info)
    return NULL;
  
  indx = r_symndx - symtab_hdr->sh_info;
  h = elf_sym_hashes (abfd)[indx];
  while (h->root.type == bfd_link_hash_indirect
	 || h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;
  return h;
}


/* Get the section-relative offset for a symbol number.  */

static bfd_vma
get_elf_r_symndx_offset (abfd, r_symndx)
     bfd *abfd;
     unsigned long r_symndx;
{
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  bfd_vma offset = 0;

  if (r_symndx < symtab_hdr->sh_info) 
    {
      Elf_Internal_Sym *isymbuf;
      isymbuf = retrieve_local_syms (abfd);
      offset = isymbuf[r_symndx].st_value;
    }
  else
    {
      unsigned long indx = r_symndx - symtab_hdr->sh_info;
      struct elf_link_hash_entry *h =
	elf_sym_hashes (abfd)[indx];

      while (h->root.type == bfd_link_hash_indirect
             || h->root.type == bfd_link_hash_warning)
	h = (struct elf_link_hash_entry *) h->root.u.i.link;
      if (h->root.type == bfd_link_hash_defined
          || h->root.type == bfd_link_hash_defweak)
	offset = h->root.u.def.value;
    }
  return offset;
}


static bfd_boolean
pcrel_reloc_fits (opnd, self_address, dest_address)
     xtensa_operand opnd;
     bfd_vma self_address;
     bfd_vma dest_address;
{
  uint32 new_address =
    xtensa_operand_do_reloc (opnd, dest_address, self_address);
  return (xtensa_operand_encode (opnd, &new_address)
	  == xtensa_encode_result_ok);
}


static int linkonce_len = sizeof (".gnu.linkonce.") - 1;
static int insn_sec_len = sizeof (XTENSA_INSN_SEC_NAME) - 1;
static int lit_sec_len = sizeof (XTENSA_LIT_SEC_NAME) - 1;


static bfd_boolean 
xtensa_is_property_section (sec)
     asection *sec;
{
  if (strncmp (XTENSA_INSN_SEC_NAME, sec->name, insn_sec_len) == 0
      || strncmp (XTENSA_LIT_SEC_NAME, sec->name, lit_sec_len) == 0)
    return TRUE;

  if (strncmp (".gnu.linkonce.", sec->name, linkonce_len) == 0
      && (sec->name[linkonce_len] == 'x'
	  || sec->name[linkonce_len] == 'p')
      && sec->name[linkonce_len + 1] == '.')
    return TRUE;

  return FALSE;
}


static bfd_boolean 
xtensa_is_littable_section (sec)
     asection *sec;
{
  if (strncmp (XTENSA_LIT_SEC_NAME, sec->name, lit_sec_len) == 0)
    return TRUE;

  if (strncmp (".gnu.linkonce.", sec->name, linkonce_len) == 0
      && sec->name[linkonce_len] == 'p'
      && sec->name[linkonce_len + 1] == '.')
    return TRUE;

  return FALSE;
}


static bfd_boolean
is_literal_section (sec)
     asection *sec;
{
  /* FIXME: the current definition of this leaves a lot to be desired....  */
  if (sec == NULL || sec->name == NULL)
    return FALSE;
  return (strstr (sec->name, "literal") != NULL);
}


static int
internal_reloc_compare (ap, bp)
     const PTR ap;
     const PTR bp;
{
  const Elf_Internal_Rela *a = (const Elf_Internal_Rela *) ap;
  const Elf_Internal_Rela *b = (const Elf_Internal_Rela *) bp;

  return (a->r_offset - b->r_offset);
}


char *
xtensa_get_property_section_name (sec, base_name)
     asection *sec;
     const char *base_name;
{
  if (strncmp (sec->name, ".gnu.linkonce.", linkonce_len) == 0)
    {
      char *prop_sec_name;
      const char *suffix;
      char linkonce_kind = 0;

      if (strcmp (base_name, XTENSA_INSN_SEC_NAME) == 0) 
	linkonce_kind = 'x';
      else if (strcmp (base_name, XTENSA_LIT_SEC_NAME) == 0) 
	linkonce_kind = 'p';
      else
	abort ();

      prop_sec_name = (char *) bfd_malloc (strlen (sec->name) + 1);
      memcpy (prop_sec_name, ".gnu.linkonce.", linkonce_len);
      prop_sec_name[linkonce_len] = linkonce_kind;
      prop_sec_name[linkonce_len + 1] = '.';

      suffix = sec->name + linkonce_len;
      while (*suffix)
	{
	  suffix += 1;
	  if (suffix[-1] == '.')
	    break;
	}
      strcpy (prop_sec_name + linkonce_len + 2, suffix);

      return prop_sec_name;
    }

  return strdup (base_name);
}


/* Other functions called directly by the linker.  */

bfd_boolean
xtensa_callback_required_dependence (abfd, sec, link_info, callback, closure)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     deps_callback_t callback;
     PTR closure;
{
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents;
  unsigned i;
  bfd_boolean ok = TRUE;

  /* ".plt*" sections have no explicit relocations but they contain L32R
     instructions that reference the corresponding ".got.plt*" sections.  */
  if ((sec->flags & SEC_LINKER_CREATED) != 0
      && strncmp (sec->name, ".plt", 4) == 0)
    {
      asection *sgotplt;

      /* Find the corresponding ".got.plt*" section.  */
      if (sec->name[4] == '\0')
	sgotplt = bfd_get_section_by_name (sec->owner, ".got.plt");
      else
	{
	  char got_name[14];
	  int chunk = 0;

	  BFD_ASSERT (sec->name[4] == '.');
	  chunk = strtol (&sec->name[5], NULL, 10);

	  sprintf (got_name, ".got.plt.%u", chunk);
	  sgotplt = bfd_get_section_by_name (sec->owner, got_name);
	}
      BFD_ASSERT (sgotplt);

      /* Assume worst-case offsets: L32R at the very end of the ".plt"
	 section referencing a literal at the very beginning of
	 ".got.plt".  This is very close to the real dependence, anyway.  */
      (*callback) (sec, sec->_raw_size, sgotplt, 0, closure);
    }

  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);
  if (internal_relocs == NULL
      || sec->reloc_count == 0) 
    return ok;

  /* Cache the contents for the duration of this scan.  */
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec->_raw_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  if (xtensa_default_isa == NULL)
    xtensa_isa_init ();

  for (i = 0; i < sec->reloc_count; i++) 
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      if (is_l32r_relocation (sec, contents, irel))
	{
	  r_reloc l32r_rel;
	  asection *target_sec;
	  bfd_vma target_offset;
	  
	  r_reloc_init (&l32r_rel, abfd, irel);
	  target_sec = NULL;
	  target_offset = 0;
	  /* L32Rs must be local to the input file.  */
	  if (r_reloc_is_defined (&l32r_rel))
	    {
	      target_sec = r_reloc_get_section (&l32r_rel);
	      target_offset = r_reloc_get_target_offset (&l32r_rel);
	    }
	  (*callback) (sec, irel->r_offset, target_sec, target_offset,
		       closure);
	}
    }

 error_return:
  release_internal_relocs (sec, internal_relocs);
  release_contents (sec, contents);
  return ok;
}

/* The default literal sections should always be marked as "code" (i.e.,
   SHF_EXECINSTR).  This is particularly important for the Linux kernel
   module loader so that the literals are not placed after the text.  */
static struct bfd_elf_special_section const elf_xtensa_special_sections[]=
{
  { ".literal",       8, 0, SHT_PROGBITS, SHF_ALLOC + SHF_EXECINSTR },
  { ".init.literal", 13, 0, SHT_PROGBITS, SHF_ALLOC + SHF_EXECINSTR },
  { ".fini.literal", 13, 0, SHT_PROGBITS, SHF_ALLOC + SHF_EXECINSTR },
  { NULL,             0, 0, 0,            0 }
};


#ifndef ELF_ARCH
#define TARGET_LITTLE_SYM		bfd_elf32_xtensa_le_vec
#define TARGET_LITTLE_NAME		"elf32-xtensa-le"
#define TARGET_BIG_SYM			bfd_elf32_xtensa_be_vec
#define TARGET_BIG_NAME			"elf32-xtensa-be"
#define ELF_ARCH			bfd_arch_xtensa

/* The new EM_XTENSA value will be recognized beginning in the Xtensa T1040
   release. However, we still have to generate files with the EM_XTENSA_OLD
   value so that pre-T1040 tools can read the files.  As soon as we stop
   caring about pre-T1040 tools, the following two values should be
   swapped. At the same time, any other code that uses EM_XTENSA_OLD
   (e.g., prep_headers() in elf.c) should be changed to use EM_XTENSA.  */
#define ELF_MACHINE_CODE		EM_XTENSA_OLD
#define ELF_MACHINE_ALT1		EM_XTENSA

#if XCHAL_HAVE_MMU
#define ELF_MAXPAGESIZE			(1 << XCHAL_MMU_MIN_PTE_PAGE_SIZE)
#else /* !XCHAL_HAVE_MMU */
#define ELF_MAXPAGESIZE			1
#endif /* !XCHAL_HAVE_MMU */
#endif /* ELF_ARCH */

#define elf_backend_can_gc_sections	1
#define elf_backend_can_refcount	1
#define elf_backend_plt_readonly	1
#define elf_backend_got_header_size	4
#define elf_backend_want_dynbss		0
#define elf_backend_want_got_plt	1

#define elf_info_to_howto		     elf_xtensa_info_to_howto_rela

#define bfd_elf32_bfd_merge_private_bfd_data elf_xtensa_merge_private_bfd_data
#define bfd_elf32_new_section_hook	     elf_xtensa_new_section_hook
#define bfd_elf32_bfd_print_private_bfd_data elf_xtensa_print_private_bfd_data
#define bfd_elf32_bfd_relax_section	     elf_xtensa_relax_section
#define bfd_elf32_bfd_reloc_type_lookup	     elf_xtensa_reloc_type_lookup
#define bfd_elf32_bfd_set_private_flags	     elf_xtensa_set_private_flags

#define elf_backend_adjust_dynamic_symbol    elf_xtensa_adjust_dynamic_symbol
#define elf_backend_check_relocs	     elf_xtensa_check_relocs
#define elf_backend_create_dynamic_sections  elf_xtensa_create_dynamic_sections
#define elf_backend_discard_info	     elf_xtensa_discard_info
#define elf_backend_ignore_discarded_relocs  elf_xtensa_ignore_discarded_relocs
#define elf_backend_final_write_processing   elf_xtensa_final_write_processing
#define elf_backend_finish_dynamic_sections  elf_xtensa_finish_dynamic_sections
#define elf_backend_finish_dynamic_symbol    elf_xtensa_finish_dynamic_symbol
#define elf_backend_gc_mark_hook	     elf_xtensa_gc_mark_hook
#define elf_backend_gc_sweep_hook	     elf_xtensa_gc_sweep_hook
#define elf_backend_grok_prstatus	     elf_xtensa_grok_prstatus
#define elf_backend_grok_psinfo		     elf_xtensa_grok_psinfo
#define elf_backend_hide_symbol		     elf_xtensa_hide_symbol
#define elf_backend_modify_segment_map	     elf_xtensa_modify_segment_map
#define elf_backend_object_p		     elf_xtensa_object_p
#define elf_backend_reloc_type_class	     elf_xtensa_reloc_type_class
#define elf_backend_relocate_section	     elf_xtensa_relocate_section
#define elf_backend_size_dynamic_sections    elf_xtensa_size_dynamic_sections
#define elf_backend_special_sections	     elf_xtensa_special_sections

#include "elf32-target.h"
