/* BFD back-end for HP PA-RISC ELF files.
   Copyright (C) 1990, 91, 92, 93, 94, 1995 Free Software Foundation, Inc.

   Written by

	Center for Software Science
	Department of Computer Science
	University of Utah

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
#include "obstack.h"
#include "elf-bfd.h"

/* The internal type of a symbol table extension entry.  */
typedef unsigned long symext_entryS;

/* The external type of a symbol table extension entry.  */
#define ELF32_PARISC_SX_SIZE (4)
#define ELF32_PARISC_SX_GET(bfd, addr) bfd_h_get_32 ((bfd), (addr))
#define ELF32_PARISC_SX_PUT(bfd, val, addr) \
  bfd_h_put_32 ((bfd), (val), (addr))

/* HPPA symbol table extension entry types */
enum elf32_hppa_symextn_types
{
  PARISC_SXT_NULL,
  PARISC_SXT_SYMNDX,
  PARISC_SXT_ARG_RELOC,
};

/* These macros compose and decompose the value of a symextn entry:

   entry_type = ELF32_PARISC_SX_TYPE(word);
   entry_value = ELF32_PARISC_SX_VAL(word);
   word = ELF32_PARISC_SX_WORD(type,val);  */

#define ELF32_PARISC_SX_TYPE(p)		((p) >> 24)
#define ELF32_PARISC_SX_VAL(p)		((p) & 0xFFFFFF)
#define ELF32_PARISC_SX_WORD(type,val)	(((type) << 24) + (val & 0xFFFFFF))

/* The following was added facilitate implementation of the .hppa_symextn
   section.  This section is built after the symbol table is built in the
   elf_write_object_contents routine (called from bfd_close).  It is built
   so late because it requires information that is not known until
   the symbol and string table sections have been allocated, and
   the symbol table has been built. */

#define SYMEXTN_SECTION_NAME ".PARISC.symext"

struct symext_chain
  {
    symext_entryS entry;
    struct symext_chain *next;
  };

typedef struct symext_chain symext_chainS;

/* We use three different hash tables to hold information for
   linking PA ELF objects.

   The first is the elf32_hppa_link_hash_table which is derived
   from the standard ELF linker hash table.  We use this as a place to
   attach other hash tables and static information.

   The second is the stub hash table which is derived from the
   base BFD hash table.  The stub hash table holds the information
   necessary to build the linker stubs during a link.

   The last hash table keeps track of argument location information needed
   to build hash tables.  Each function with nonzero argument location
   bits will have an entry in this table.  */

/* Hash table for linker stubs.  */

struct elf32_hppa_stub_hash_entry
{
  /* Base hash table entry structure, we can get the name of the stub
     (and thus know exactly what actions it performs) from the base
     hash table entry.  */
  struct bfd_hash_entry root;

  /* Offset of the beginning of this stub.  */
  bfd_vma offset;

  /* Given the symbol's value and its section we can determine its final
     value when building the stubs (so the stub knows where to jump.  */
  symvalue target_value;
  asection *target_section;
};

struct elf32_hppa_stub_hash_table
{
  /* The hash table itself.  */
  struct bfd_hash_table root;

  /* The stub BFD.  */
  bfd *stub_bfd;

  /* Where to place the next stub.  */
  bfd_byte *location;

  /* Current offset in the stub section.  */
  unsigned int offset;

};

/* Hash table for argument location information.  */

struct elf32_hppa_args_hash_entry
{
  /* Base hash table entry structure.  */
  struct bfd_hash_entry root;

  /* The argument location bits for this entry.  */
  int arg_bits;
};

struct elf32_hppa_args_hash_table
{
  /* The hash table itself.  */
  struct bfd_hash_table root;
};

struct elf32_hppa_link_hash_entry
{
  struct elf_link_hash_entry root;
};

struct elf32_hppa_link_hash_table
{
  /* The main hash table.  */
  struct elf_link_hash_table root;

  /* The stub hash table.  */
  struct elf32_hppa_stub_hash_table *stub_hash_table;

  /* The argument relocation bits hash table.  */
  struct elf32_hppa_args_hash_table *args_hash_table;

  /* A count of the number of output symbols.  */
  unsigned int output_symbol_count;

  /* Stuff so we can handle DP relative relocations.  */
  long global_value;
  int global_sym_defined;
};

/* FIXME.  */
#define ARGUMENTS	0
#define RETURN_VALUE	1

/* The various argument relocations that may be performed.  */
typedef enum
{
  /* No relocation.  */
  NO,
  /* Relocate 32 bits from GR to FP register.  */
  GF,
  /* Relocate 64 bits from a GR pair to FP pair.  */
  GD,
  /* Relocate 32 bits from FP to GR.  */
  FG,
  /* Relocate 64 bits from FP pair to GR pair.  */
  DG,
} arg_reloc_type;

/* What is being relocated (eg which argument or the return value).  */
typedef enum
{
  ARG0, ARG1, ARG2, ARG3, RET,
} arg_reloc_location;


/* ELF32/HPPA relocation support

	This file contains ELF32/HPPA relocation support as specified
	in the Stratus FTX/Golf Object File Format (SED-1762) dated
	February 1994.  */

#include "elf32-hppa.h"
#include "hppa_stubs.h"

static bfd_reloc_status_type hppa_elf_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

static unsigned long hppa_elf_relocate_insn
  PARAMS ((bfd *, asection *, unsigned long, unsigned long, long,
	   long, unsigned long, unsigned long, unsigned long));

static bfd_reloc_status_type hppa_elf_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd*, char **));

static reloc_howto_type * elf_hppa_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));

static boolean elf32_hppa_set_section_contents
  PARAMS ((bfd *, sec_ptr, PTR, file_ptr, bfd_size_type));

static void elf_info_to_howto
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rela *));

static boolean elf32_hppa_backend_symbol_table_processing
  PARAMS ((bfd *, elf_symbol_type *, unsigned int));

static void elf32_hppa_backend_begin_write_processing
  PARAMS ((bfd *, struct bfd_link_info *));

static void elf32_hppa_backend_final_write_processing
  PARAMS ((bfd *, boolean));

static void add_entry_to_symext_chain
  PARAMS ((bfd *, unsigned int, unsigned int, symext_chainS **,
	   symext_chainS **));

static void
elf_hppa_tc_make_sections PARAMS ((bfd *, symext_chainS *));

static boolean hppa_elf_is_local_label PARAMS ((bfd *, asymbol *));

static boolean elf32_hppa_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));

static bfd_reloc_status_type elf32_hppa_bfd_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, bfd *, asection *,
	   bfd_byte *, bfd_vma, bfd_vma, bfd_vma, struct bfd_link_info *,
	   asection *, const char *, int));

static struct bfd_link_hash_table *elf32_hppa_link_hash_table_create
  PARAMS ((bfd *));

static struct bfd_hash_entry *
elf32_hppa_stub_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));

static struct bfd_hash_entry *
elf32_hppa_args_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));

static boolean
elf32_hppa_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *,
	   bfd_byte *, Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));

static boolean
elf32_hppa_stub_hash_table_init
  PARAMS ((struct elf32_hppa_stub_hash_table *, bfd *,
	   struct bfd_hash_entry *(*) PARAMS ((struct bfd_hash_entry *,
					       struct bfd_hash_table *,
					       const char *))));

static boolean
elf32_hppa_build_one_stub PARAMS ((struct bfd_hash_entry *, PTR));

static boolean
elf32_hppa_read_symext_info
  PARAMS ((bfd *, Elf_Internal_Shdr *, struct elf32_hppa_args_hash_table *,
	   Elf_Internal_Sym *));

static unsigned int elf32_hppa_size_of_stub
  PARAMS ((unsigned int, unsigned int, bfd_vma, bfd_vma, const char *));

static boolean elf32_hppa_arg_reloc_needed
  PARAMS ((unsigned int, unsigned int, arg_reloc_type []));

static void elf32_hppa_name_of_stub
  PARAMS ((unsigned int, unsigned int, bfd_vma, bfd_vma, char *));

static boolean elf32_hppa_size_symext PARAMS ((struct bfd_hash_entry *, PTR));

static boolean elf32_hppa_link_output_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const char *,
	   Elf_Internal_Sym *, asection *));

/* ELF/PA relocation howto entries.  */

static reloc_howto_type elf_hppa_howto_table[ELF_HOWTO_TABLE_SIZE] =
{
  {R_PARISC_NONE, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_NONE"},
  {R_PARISC_DIR32, 0, 0, 32, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DIR32"},
  {R_PARISC_DIR21L, 0, 0, 21, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DIR21L"},
  {R_PARISC_DIR17R, 0, 0, 17, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DIR17R"},
  {R_PARISC_DIR17F, 0, 0, 17, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DIR17F"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_DIR14R, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DIR14R"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_PCREL21L, 0, 0, 21, true, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PCREL21L"},
  {R_PARISC_PCREL17R, 0, 0, 17, true, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PCREL17R"},
  {R_PARISC_PCREL17F, 0, 0, 17, true, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PCREL17F"},
  {R_PARISC_PCREL17C, 0, 0, 17, true, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PCREL17C"},
  {R_PARISC_PCREL14R, 0, 0, 14, true, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PCREL14R"},
  {R_PARISC_PCREL14F, 0, 0, 14, true, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PCREL14F"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_DPREL21L, 0, 0, 21, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DPREL21L"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_DPREL14R, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DPREL14R"},
  {R_PARISC_DPREL14F, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DPREL14F"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_DLTREL21L, 0, 0, 21, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DLTREL21L"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_DLTREL14R, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DLTREL14R"},
  {R_PARISC_DLTREL14F, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DLTREL14F"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_DLTIND21L, 0, 0, 21, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DLTIND21L"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_DLTIND14R, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DLTIND14R"},
  {R_PARISC_DLTIND14F, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_DLTIND14F"},

  {R_PARISC_SETBASE, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_SETBASE"},
  {R_PARISC_BASEREL32, 0, 0, 32, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_BASEREL32"},
  {R_PARISC_BASEREL21L, 0, 0, 21, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_BASEREL21L"},
  {R_PARISC_BASEREL17R, 0, 0, 17, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_BASEREL17R"},
  {R_PARISC_BASEREL17F, 0, 0, 17, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_BASEREL17F"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_BASEREL14R, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_BASEREL14R"},
  {R_PARISC_BASEREL14F, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_BASEREL14F"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_TEXTREL32, 0, 0, 32, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_TEXTREL32"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_DATAREL32, 0, 0, 32, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},


  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_PLABEL32, 0, 0, 32, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PLABEL32"},
  {R_PARISC_PLABEL21L, 0, 0, 21, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PLABEL21L"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_PLABEL14R, 0, 0, 14, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PLABEL14R"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},


  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_PLTIND21L, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PLTIND21L"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_UNIMPLEMENTED"},
  {R_PARISC_PLTIND14R, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PLTIND14R"},
  {R_PARISC_PLTIND14F, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_PLTIND14F"},


  {R_PARISC_COPY, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_COPY"},
  {R_PARISC_GLOB_DAT, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_GLOB_DAT"},
  {R_PARISC_JMP_SLOT, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_JMP_SLOT"},
  {R_PARISC_RELATIVE, 0, 0, 0, false, 0, complain_overflow_bitfield, hppa_elf_reloc, "R_PARISC_RELATIVE"},

  {R_PARISC_UNIMPLEMENTED, 0, 0, 0, false, 0, complain_overflow_dont, NULL, "R_PARISC_UNIMPLEMENTED"},
};

/* Where (what register type) is an argument comming from?  */
typedef enum
{
  AR_NO,
  AR_GR,
  AR_FR,
  AR_FU,
  AR_FPDBL1,
  AR_FPDBL2,
} arg_location;

/* Horizontal represents the callee's argument location information,
   vertical represents caller's argument location information.  Value at a
   particular X,Y location represents what (if any) argument relocation
   needs to be performed to make caller and callee agree.  */

static CONST arg_reloc_type arg_mismatches[6][6] =
{
  {NO, NO, NO, NO, NO, NO},
  {NO, NO, GF, NO, GD, NO},
  {NO, FG, NO, NO, NO, NO},
  {NO, NO, NO, NO, NO, NO},
  {NO, DG, NO, NO, NO, NO},
  {NO, DG, NO, NO, NO, NO},
};

/* Likewise, but reversed for the return value.  */
static CONST arg_reloc_type ret_mismatches[6][6] =
{
  {NO, NO, NO, NO, NO, NO},
  {NO, NO, FG, NO, DG, NO},
  {NO, GF, NO, NO, NO, NO},
  {NO, NO, NO, NO, NO, NO},
  {NO, GD, NO, NO, NO, NO},
  {NO, GD, NO, NO, NO, NO},
};

/* Misc static crud for symbol extension records.  */
static symext_chainS *symext_rootP;
static symext_chainS *symext_lastP;
static bfd_size_type symext_chain_size;

/* FIXME: We should be able to try this static variable!  */
static bfd_byte *symextn_contents;


/* For linker stub hash tables.  */
#define elf32_hppa_stub_hash_lookup(table, string, create, copy) \
  ((struct elf32_hppa_stub_hash_entry *) \
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

#define elf32_hppa_stub_hash_traverse(table, func, info) \
  (bfd_hash_traverse \
   (&(table)->root, \
    (boolean (*) PARAMS ((struct bfd_hash_entry *, PTR))) (func), \
    (info)))

/* For linker args hash tables.  */
#define elf32_hppa_args_hash_lookup(table, string, create, copy) \
  ((struct elf32_hppa_args_hash_entry *) \
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

#define elf32_hppa_args_hash_traverse(table, func, info) \
  (bfd_hash_traverse \
   (&(table)->root, \
    (boolean (*) PARAMS ((struct bfd_hash_entry *, PTR))) (func), \
    (info)))

#define elf32_hppa_args_hash_table_init(table, newfunc) \
  (bfd_hash_table_init \
   (&(table)->root, \
    (struct bfd_hash_entry *(*) PARAMS ((struct bfd_hash_entry *, \
					 struct bfd_hash_table *, \
					 const char *))) (newfunc)))

/* For HPPA linker hash table.  */

#define elf32_hppa_link_hash_lookup(table, string, create, copy, follow)\
  ((struct elf32_hppa_link_hash_entry *)				\
   elf_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

#define elf32_hppa_link_hash_traverse(table, func, info)		\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the PA ELF linker hash table from a link_info structure.  */

#define elf32_hppa_hash_table(p) \
  ((struct elf32_hppa_link_hash_table *) ((p)->hash))


/* Extract specific argument location bits for WHICH from
   the full argument location in AR.  */
#define EXTRACT_ARBITS(ar, which) ((ar) >> (8 - ((which) * 2))) & 3

/* Assorted hash table functions.  */

/* Initialize an entry in the stub hash table.  */

static struct bfd_hash_entry *
elf32_hppa_stub_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf32_hppa_stub_hash_entry *ret;

  ret = (struct elf32_hppa_stub_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = ((struct elf32_hppa_stub_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct elf32_hppa_stub_hash_entry)));
  if (ret == NULL)
    return NULL;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf32_hppa_stub_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));

  if (ret)
    {
      /* Initialize the local fields.  */
      ret->offset = 0;
      ret->target_value = 0;
      ret->target_section = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Initialize a stub hash table.  */

static boolean
elf32_hppa_stub_hash_table_init (table, stub_bfd, newfunc)
     struct elf32_hppa_stub_hash_table *table;
     bfd *stub_bfd;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
{
  table->offset = 0;
  table->location = 0;
  table->stub_bfd = stub_bfd;
  return (bfd_hash_table_init (&table->root, newfunc));
}

/* Initialize an entry in the argument location hash table.  */

static struct bfd_hash_entry *
elf32_hppa_args_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf32_hppa_args_hash_entry *ret;

  ret = (struct elf32_hppa_args_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = ((struct elf32_hppa_args_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct elf32_hppa_args_hash_entry)));
  if (ret == NULL)
    return NULL;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf32_hppa_args_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));

  /* Initialize the local fields.  */
  if (ret)
    ret->arg_bits = 0;

  return (struct bfd_hash_entry *) ret;
}

/* Create the derived linker hash table.  The PA ELF port uses the derived
   hash table to keep information specific to the PA ELF linker (without
   using static variables).  */

static struct bfd_link_hash_table *
elf32_hppa_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf32_hppa_link_hash_table *ret;

  ret = ((struct elf32_hppa_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct elf32_hppa_link_hash_table)));
  if (ret == NULL)
    return NULL;
  if (!_bfd_elf_link_hash_table_init (&ret->root, abfd,
				      _bfd_elf_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }
  ret->stub_hash_table = NULL;
  ret->args_hash_table = NULL;
  ret->output_symbol_count = 0;
  ret->global_value = 0;
  ret->global_sym_defined = 0;

  return &ret->root.root;
}

/* Relocate the given INSN given the various input parameters.

   FIXME: endianness and sizeof (long) issues abound here.  */

static unsigned long
hppa_elf_relocate_insn (abfd, input_sect, insn, address, sym_value,
			r_addend, r_format, r_field, pcrel)
     bfd *abfd;
     asection *input_sect;
     unsigned long insn;
     unsigned long address;
     long sym_value;
     long r_addend;
     unsigned long r_format;
     unsigned long r_field;
     unsigned long pcrel;
{
  unsigned char opcode = get_opcode (insn);
  long constant_value;

  switch (opcode)
    {
    case LDO:
    case LDB:
    case LDH:
    case LDW:
    case LDWM:
    case STB:
    case STH:
    case STW:
    case STWM:
    case COMICLR:
    case SUBI:
    case ADDIT:
    case ADDI:
    case LDIL:
    case ADDIL:
      constant_value = HPPA_R_CONSTANT (r_addend);

      if (pcrel)
	sym_value -= address;

      sym_value = hppa_field_adjust (sym_value, constant_value, r_field);
      return hppa_rebuild_insn (abfd, insn, sym_value, r_format);

    case BL:
    case BE:
    case BLE:
      /* XXX computing constant_value is not needed??? */
      constant_value = assemble_17 ((insn & 0x001f0000) >> 16,
				    (insn & 0x00001ffc) >> 2,
				    insn & 1);

      constant_value = (constant_value << 15) >> 15;
      if (pcrel)
	{
	  sym_value -=
	    address + input_sect->output_offset
	    + input_sect->output_section->vma;
	  sym_value = hppa_field_adjust (sym_value, -8, r_field);
	}
      else
	sym_value = hppa_field_adjust (sym_value, constant_value, r_field);

      return hppa_rebuild_insn (abfd, insn, sym_value >> 2, r_format);

    default:
      if (opcode == 0)
	{
	  constant_value = HPPA_R_CONSTANT (r_addend);

	  if (pcrel)
	    sym_value -= address;

	  return hppa_field_adjust (sym_value, constant_value, r_field);
	}
      else
	abort ();
    }
}

/* Relocate an HPPA ELF section.  */

static boolean
elf32_hppa_relocate_section (output_bfd, info, input_bfd, input_section,
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

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sym_sec;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      const char *sym_name;

      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_PARISC_UNIMPLEMENTED)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = elf_hppa_howto_table + r_type;

      r_symndx = ELF32_R_SYM (rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sym_sec = local_sections[r_symndx];
		  rel->r_addend += sym_sec->output_offset;
		}
	    }

	  continue;
	}

      /* This is a final link.  */
      h = NULL;
      sym = NULL;
      sym_sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sym_sec = local_sections[r_symndx];
	  relocation = ((ELF_ST_TYPE (sym->st_info) == STT_SECTION
			   ? 0 : sym->st_value)
			 + sym_sec->output_offset
			 + sym_sec->output_section->vma);
	}
      else
	{
	  long indx;

	  indx = r_symndx - symtab_hdr->sh_info;
	  h = elf_sym_hashes (input_bfd)[indx];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sym_sec = h->root.u.def.section;
	      relocation = (h->root.u.def.value
			    + sym_sec->output_offset
			    + sym_sec->output_section->vma);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else
	    {
	      if (!((*info->callbacks->undefined_symbol)
		    (info, h->root.root.string, input_bfd,
		     input_section, rel->r_offset)))
		return false;
	      break;
	    }
	}

      if (h != NULL)
	sym_name = h->root.root.string;
      else
	{
	  sym_name = bfd_elf_string_from_elf_section (input_bfd,
						      symtab_hdr->sh_link,
						      sym->st_name);
	  if (sym_name == NULL)
	    return false;
	  if (*sym_name == '\0')
	    sym_name = bfd_section_name (input_bfd, sym_sec);
	}

      /* If args_hash_table is NULL, then we have encountered some
	 kind of link error (ex. undefined symbols).  Do not try to
	 apply any relocations, continue the loop so we can notify
	 the user of several errors in a single attempted link.  */
      if (elf32_hppa_hash_table (info)->args_hash_table == NULL)
	continue;

      r = elf32_hppa_bfd_final_link_relocate (howto, input_bfd, output_bfd,
					      input_section, contents,
					      rel->r_offset, relocation,
					      rel->r_addend, info, sym_sec,
					      sym_name, h == NULL);

      if (r != bfd_reloc_ok)
	{
	  switch (r)
	    {
	    /* This can happen for DP relative relocs if $global$ is
	       undefined.  This is a panic situation so we don't try
	       to continue.  */
	    case bfd_reloc_undefined:
	    case bfd_reloc_notsupported:
	      if (!((*info->callbacks->undefined_symbol)
		    (info, "$global$", input_bfd,
		     input_section, rel->r_offset)))
		return false;
	      return false;
	    case bfd_reloc_dangerous:
	      {
		/* We use this return value to indicate that we performed
		   a "dangerous" relocation.  This doesn't mean we did
		   the wrong thing, it just means there may be some cleanup
		   that needs to be done here.

		   In particular we had to swap the last call insn and its
		   delay slot.  If the delay slot insn needed a relocation,
		   then we'll need to adjust the next relocation entry's
		   offset to account for the fact that the insn moved.

		   This hair wouldn't be necessary if we inserted stubs
		   between procedures and used a "bl" to get to the stub.  */
		if (rel != relend)
		  {
		    Elf_Internal_Rela *next_rel = rel + 1;

		    if (rel->r_offset + 4 == next_rel->r_offset)
		      next_rel->r_offset -= 4;
		  }
		break;
	      }
	    default:
	    case bfd_reloc_outofrange:
	    case bfd_reloc_overflow:
	      {
		if (!((*info->callbacks->reloc_overflow)
		      (info, sym_name, howto->name, (bfd_vma) 0,
			input_bfd, input_section, rel->r_offset)))
		  return false;
	      }
	      break;
	    }
	}
    }

  return true;
}

/* Return one (or more) BFD relocations which implement the base
   relocation with modifications based on format and field.  */

elf32_hppa_reloc_type **
hppa_elf_gen_reloc_type (abfd, base_type, format, field, ignore)
     bfd *abfd;
     elf32_hppa_reloc_type base_type;
     int format;
     int field;
     int ignore;
{
  elf32_hppa_reloc_type *finaltype;
  elf32_hppa_reloc_type **final_types;

  /* Allocate slots for the BFD relocation.  */
  final_types = (elf32_hppa_reloc_type **)
    bfd_alloc_by_size_t (abfd, sizeof (elf32_hppa_reloc_type *) * 2);
  if (final_types == NULL)
    return NULL;

  /* Allocate space for the relocation itself.  */
  finaltype = (elf32_hppa_reloc_type *)
    bfd_alloc_by_size_t (abfd, sizeof (elf32_hppa_reloc_type));
  if (finaltype == NULL)
    return NULL;

  /* Some reasonable defaults.  */
  final_types[0] = finaltype;
  final_types[1] = NULL;

#define final_type finaltype[0]

  final_type = base_type;

  /* Just a tangle of nested switch statements to deal with the braindamage
     that a different field selector means a completely different relocation
     for PA ELF.  */
  switch (base_type)
    {
    case R_HPPA:
    case R_HPPA_ABS_CALL:
      switch (format)
	{
	case 14:
	  switch (field)
	    {
	    case e_rsel:
	    case e_rrsel:
	      final_type = R_PARISC_DIR14R;
	      break;
	    case e_rtsel:
	      final_type = R_PARISC_DLTREL14R;
	      break;
	    case e_tsel:
	      final_type = R_PARISC_DLTREL14F;
	      break;
	    case e_rpsel:
	      final_type = R_PARISC_PLABEL14R;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 17:
	  switch (field)
	    {
	    case e_fsel:
	      final_type = R_PARISC_DIR17F;
	      break;
	    case e_rsel:
	    case e_rrsel:
	      final_type = R_PARISC_DIR17R;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 21:
	  switch (field)
	    {
	    case e_lsel:
	    case e_lrsel:
	      final_type = R_PARISC_DIR21L;
	      break;
	    case e_ltsel:
	      final_type = R_PARISC_DLTREL21L;
	      break;
	    case e_lpsel:
	      final_type = R_PARISC_PLABEL21L;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 32:
	  switch (field)
	    {
	    case e_fsel:
	      final_type = R_PARISC_DIR32;
	      break;
	    case e_psel:
	      final_type = R_PARISC_PLABEL32;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	default:
	  return NULL;
	}
      break;


    case R_HPPA_GOTOFF:
      switch (format)
	{
	case 14:
	  switch (field)
	    {
	    case e_rsel:
	    case e_rrsel:
	      final_type = R_PARISC_DPREL14R;
	      break;
	    case e_fsel:
	      final_type = R_PARISC_DPREL14F;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 21:
	  switch (field)
	    {
	    case e_lrsel:
	    case e_lsel:
	      final_type = R_PARISC_DPREL21L;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	default:
	  return NULL;
	}
      break;


    case R_HPPA_PCREL_CALL:
      switch (format)
	{
	case 14:
	  switch (field)
	    {
	    case e_rsel:
	    case e_rrsel:
	      final_type = R_PARISC_PCREL14R;
	      break;
	    case e_fsel:
	      final_type = R_PARISC_PCREL14F;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 17:
	  switch (field)
	    {
	    case e_rsel:
	    case e_rrsel:
	      final_type = R_PARISC_PCREL17R;
	      break;
	    case e_fsel:
	      final_type = R_PARISC_PCREL17F;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	case 21:
	  switch (field)
	    {
	    case e_lsel:
	    case e_lrsel:
	      final_type = R_PARISC_PCREL21L;
	      break;
	    default:
	      return NULL;
	    }
	  break;

	default:
	  return NULL;
	}
      break;

    default:
      return NULL;
    }

  return final_types;
}

#undef final_type

/* Set the contents of a particular section at a particular location.  */

static boolean
elf32_hppa_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  /* Ignore write requests for the symbol extension section until we've
     had the chance to rebuild it ourselves.  */
  if (!strcmp (section->name, ".PARISC.symextn") && !symext_chain_size)
    return true;
  else
    return _bfd_elf_set_section_contents (abfd, section, location,
					  offset, count);
}

/* Translate from an elf into field into a howto relocation pointer.  */

static void
elf_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf32_Internal_Rela *dst;
{
  BFD_ASSERT (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_PARISC_UNIMPLEMENTED);
  cache_ptr->howto = &elf_hppa_howto_table[ELF32_R_TYPE (dst->r_info)];
}


/* Actually perform a relocation.  NOTE this is (mostly) superceeded
   by elf32_hppa_bfd_final_link_relocate which is called by the new
   fast linker.  */

static bfd_reloc_status_type
hppa_elf_reloc (abfd, reloc_entry, symbol_in, data, input_section, output_bfd,
		error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol_in;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* It is no longer valid to call hppa_elf_reloc when creating
     a final executable.  */
  if (output_bfd)
    {
      reloc_entry->address += input_section->output_offset;

      /* Work around lossage in generic elf code to write relocations.
	 (maps different section symbols into the same symbol index).  */
      if ((symbol_in->flags & BSF_SECTION_SYM)
	  && symbol_in->section)
	reloc_entry->addend += symbol_in->section->output_offset;
      return bfd_reloc_ok;
    }
  else
    {
      *error_message = (char *) "Unsupported call to hppa_elf_reloc";
      return bfd_reloc_notsupported;
    }
}

/* Actually perform a relocation as part of a final link.  This can get
   rather hairy when linker stubs are needed.  */

static bfd_reloc_status_type
elf32_hppa_bfd_final_link_relocate (howto, input_bfd, output_bfd,
				    input_section, contents, offset, value,
				    addend, info, sym_sec, sym_name, is_local)
     reloc_howto_type *howto;
     bfd *input_bfd;
     bfd *output_bfd;
     asection *input_section;
     bfd_byte *contents;
     bfd_vma offset;
     bfd_vma value;
     bfd_vma addend;
     struct bfd_link_info *info;
     asection *sym_sec;
     const char *sym_name;
     int is_local;
{
  unsigned long insn;
  unsigned long r_type = howto->type;
  unsigned long r_format = howto->bitsize;
  unsigned long r_field = e_fsel;
  bfd_byte *hit_data = contents + offset;
  boolean r_pcrel = howto->pc_relative;

  insn = bfd_get_32 (input_bfd, hit_data);

  /* Make sure we have a value for $global$.  FIXME isn't this effectively
     just like the gp pointer on MIPS?  Can we use those routines for this
     purpose?  */
  if (!elf32_hppa_hash_table (info)->global_sym_defined)
    {
      struct elf_link_hash_entry *h;
      asection *sec;

      h = elf_link_hash_lookup (elf_hash_table (info), "$global$", false,
				 false, false);

      /* If there isn't a $global$, then we're in deep trouble.  */
      if (h == NULL)
	return bfd_reloc_notsupported;

      /* If $global$ isn't a defined symbol, then we're still in deep
	 trouble.  */
      if (h->root.type != bfd_link_hash_defined)
	return bfd_reloc_undefined;

      sec = h->root.u.def.section;
      elf32_hppa_hash_table (info)->global_value = (h->root.u.def.value
						    + sec->output_section->vma
						    + sec->output_offset);
      elf32_hppa_hash_table (info)->global_sym_defined = 1;
    }

  switch (r_type)
    {
    case R_PARISC_NONE:
      break;

    case R_PARISC_DIR32:
    case R_PARISC_DIR17F:
    case R_PARISC_PCREL17C:
      r_field = e_fsel;
      goto do_basic_type_1;
    case R_PARISC_DIR21L:
    case R_PARISC_PCREL21L:
      r_field = e_lrsel;
      goto do_basic_type_1;
    case R_PARISC_DIR17R:
    case R_PARISC_PCREL17R:
    case R_PARISC_DIR14R:
    case R_PARISC_PCREL14R:
      r_field = e_rrsel;
      goto do_basic_type_1;

    /* For all the DP relative relocations, we need to examine the symbol's
       section.  If it's a code section, then "data pointer relative" makes
       no sense.  In that case we don't adjust the "value", and for 21 bit
       addil instructions, we change the source addend register from %dp to
       %r0.  */
    case R_PARISC_DPREL21L:
      r_field = e_lrsel;
      if (sym_sec->flags & SEC_CODE)
	{
	  if ((insn & 0xfc000000) >> 26 == 0xa
	       && (insn & 0x03e00000) >> 21 == 0x1b)
	    insn &= ~0x03e00000;
	}
      else
	value -= elf32_hppa_hash_table (info)->global_value;
      goto do_basic_type_1;
    case R_PARISC_DPREL14R:
      r_field = e_rrsel;
      if ((sym_sec->flags & SEC_CODE) == 0)
	value -= elf32_hppa_hash_table (info)->global_value;
      goto do_basic_type_1;
    case R_PARISC_DPREL14F:
      r_field = e_fsel;
      if ((sym_sec->flags & SEC_CODE) == 0)
	value -= elf32_hppa_hash_table (info)->global_value;
      goto do_basic_type_1;

    /* These cases are separate as they may involve a lot more work
       to deal with linker stubs.  */
    case R_PARISC_PLABEL32:
    case R_PARISC_PLABEL21L:
    case R_PARISC_PLABEL14R:
    case R_PARISC_PCREL17F:
      {
	bfd_vma location;
	unsigned int len, caller_args, callee_args;
	arg_reloc_type arg_reloc_types[5];
	struct elf32_hppa_args_hash_table *args_hash_table;
	struct elf32_hppa_args_hash_entry *args_hash;
	char *new_name, *stub_name;

	/* Get the field selector right.  We'll need it in a minute.  */
	if (r_type == R_PARISC_PCREL17F
	    || r_type == R_PARISC_PLABEL32)
	  r_field = e_fsel;
	else if (r_type == R_PARISC_PLABEL21L)
	  r_field = e_lrsel;
	else if (r_type == R_PARISC_PLABEL14R)
	  r_field = e_rrsel;

	/* Find out where we are and where we're going.  */
	location = (offset +
		    input_section->output_offset +
		    input_section->output_section->vma);

	/* Now look for the argument relocation bits associated with the
	   target.  */
	len = strlen (sym_name) + 1;
	if (is_local)
	  len += 9;
	new_name = bfd_malloc (len);
	if (!new_name)
	  return bfd_reloc_notsupported;
	strcpy (new_name, sym_name);

	/* Local symbols have unique IDs.  */
	if (is_local)
	  sprintf (new_name + len - 10, "_%08x", (int)sym_sec);

	args_hash_table = elf32_hppa_hash_table (info)->args_hash_table;

	args_hash = elf32_hppa_args_hash_lookup (args_hash_table,
						 new_name, false, false);
	if (args_hash == NULL)
	  callee_args = 0;
	else
	  callee_args = args_hash->arg_bits;

	/* If this is a CALL relocation, then get the caller's bits
	   from the addend.  Else use the magic 0x155 value for PLABELS.

	   Also we don't care about the destination (value) for PLABELS.  */
	if (r_type == R_PARISC_PCREL17F)
	  caller_args = HPPA_R_ARG_RELOC (addend);
	else
	  {
	    caller_args = 0x155;
	    location = value;
	  }

	/* Any kind of linker stub needed?  */
	if (((int)(value - location) > 0x3ffff)
	    || ((int)(value - location) < (int)0xfffc0000)
	    || elf32_hppa_arg_reloc_needed (caller_args, callee_args,
					    arg_reloc_types))
	  {
	    struct elf32_hppa_stub_hash_table *stub_hash_table;
	    struct elf32_hppa_stub_hash_entry *stub_hash;
	    asection *stub_section;

	    /* Build a name for the stub.  */

	    len = strlen (new_name);
	    len += 23;
	    stub_name = bfd_malloc (len);
	    if (!stub_name)
	      return bfd_reloc_notsupported;
	    elf32_hppa_name_of_stub (caller_args, callee_args,
				     location, value, stub_name);
	    strcat (stub_name, new_name);
	    free (new_name);

	    stub_hash_table = elf32_hppa_hash_table (info)->stub_hash_table;

	    stub_hash
	      = elf32_hppa_stub_hash_lookup (stub_hash_table, stub_name,
					     false, false);

	    /* We're done with that name.  */
	    free (stub_name);

	    /* The stub BFD only has one section.  */
	    stub_section = stub_hash_table->stub_bfd->sections;

	    if (stub_hash != NULL)
	      {

		if (r_type == R_PARISC_PCREL17F)
		  {
		    unsigned long delay_insn;
		    unsigned int opcode, rtn_reg, ldo_target_reg, ldo_src_reg;

		    /* We'll need to peek at the next insn.  */
		    delay_insn = bfd_get_32 (input_bfd, hit_data + 4);
		    opcode = get_opcode (delay_insn);

		    /* We also need to know the return register for this
		       call.  */
		    rtn_reg = (insn & 0x03e00000) >> 21;

		    ldo_src_reg = (delay_insn & 0x03e00000) >> 21;
		    ldo_target_reg = (delay_insn & 0x001f0000) >> 16;

		    /* Munge up the value and other parameters for
		       hppa_elf_relocate_insn.  */

		    value = (stub_hash->offset
			     + stub_section->output_offset
			     + stub_section->output_section->vma);

		    r_format = 17;
		    r_field = e_fsel;
		    r_pcrel = 0;
		    addend = 0;

		    /* We need to peek at the delay insn and determine if
		       we'll need to swap the branch and its delay insn.  */
		    if ((insn & 2)
			|| (opcode == LDO
			    && ldo_target_reg == rtn_reg)
			|| (delay_insn == 0x08000240))
		      {
			/* No need to swap the branch and its delay slot, but
			   we do need to make sure to jump past the return
			   pointer update in the stub.  */
			value += 4;

			/* If the delay insn does a return pointer adjustment,
			   then we have to make sure it stays valid.  */
			if (opcode == LDO
			    && ldo_target_reg == rtn_reg)
			  {
			    delay_insn &= 0xfc00ffff;
			    delay_insn |= ((31 << 21) | (31 << 16));
			    bfd_put_32 (input_bfd, delay_insn, hit_data + 4);
			  }
			/* Use a BLE to reach the stub.  */
			insn = BLE_SR4_R0;
		      }
		    else
		      {
			/* Wonderful, we have to swap the call insn and its
			   delay slot.  */
			bfd_put_32 (input_bfd, delay_insn, hit_data);
			/* Use a BLE,n to reach the stub.  */
			insn = (BLE_SR4_R0 | 0x2);
			bfd_put_32 (input_bfd, insn, hit_data + 4);
			insn = hppa_elf_relocate_insn (input_bfd,
						       input_section,
						       insn, offset + 4,
						       value, addend,
						       r_format, r_field,
						       r_pcrel);
			/* Update the instruction word.  */
			bfd_put_32 (input_bfd, insn, hit_data + 4);
			return bfd_reloc_dangerous;
		      }
		  }
		else
		  {
		    /* PLABEL stuff is easy.  */

		    value = (stub_hash->offset
			     + stub_section->output_offset
			     + stub_section->output_section->vma);
		    /* We don't need the RP adjustment for PLABELs.  */
		    value += 4;
		    if (r_type == R_PARISC_PLABEL32)
		      r_format = 32;
		    else if (r_type == R_PARISC_PLABEL21L)
		      r_format = 21;
		    else if (r_type == R_PARISC_PLABEL14R)
		      r_format = 14;

		    r_pcrel = 0;
		    addend = 0;
		  }
		}
	      else
		return bfd_reloc_notsupported;
	  }
	goto do_basic_type_1;
      }

do_basic_type_1:
      insn = hppa_elf_relocate_insn (input_bfd, input_section, insn,
				     offset, value, addend, r_format,
				     r_field, r_pcrel);
      break;

    /* Something we don't know how to handle.  */
    default:
      return bfd_reloc_notsupported;
    }

  /* Update the instruction word.  */
  bfd_put_32 (input_bfd, insn, hit_data);
  return (bfd_reloc_ok);
}

/* Return the address of the howto table entry to perform the CODE
   relocation for an ARCH machine.  */

static reloc_howto_type *
elf_hppa_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  if ((int) code < (int) R_PARISC_UNIMPLEMENTED)
    {
      BFD_ASSERT ((int) elf_hppa_howto_table[(int) code].type == (int) code);
      return &elf_hppa_howto_table[(int) code];
    }
  return NULL;
}

/* Return true if SYM represents a local label symbol.  */

static boolean
hppa_elf_is_local_label (abfd, sym)
     bfd *abfd;
     asymbol *sym;
{
  return (sym->name[0] == 'L' && sym->name[1] == '$');
}

/* Do any backend specific processing when beginning to write an object
   file.  For PA ELF we need to determine the size of the symbol extension
   section *before* any other output processing happens.  */

static void
elf32_hppa_backend_begin_write_processing (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  unsigned int i;
  asection *symextn_sec;

  /* Size up the symbol extension section.  */
  if ((abfd->outsymbols == NULL
       && info == NULL)
      || symext_chain_size != 0)
    return;

  if (info == NULL)
    {
      /* We were not called from the BFD ELF linker code, so we need
	 to examine the output BFD's outsymbols.

	 Note we can not build the symbol extensions now as the symbol
	 map hasn't been set up.  */
      for (i = 0; i < abfd->symcount; i++)
	{
	  elf_symbol_type *symbol = (elf_symbol_type *)abfd->outsymbols[i];

	  /* Only functions ever need an entry in the symbol extension
	     section.  */
	  if (!(symbol->symbol.flags & BSF_FUNCTION))
	    continue;

	  /* And only if they specify the locations of their arguments.  */
	  if (symbol->tc_data.hppa_arg_reloc == 0)
	    continue;

	  /* Yup.  This function symbol needs an entry.  */
	  symext_chain_size += 2 * ELF32_PARISC_SX_SIZE;
	}
    }
  else if (info->relocateable == true)
    {
      struct elf32_hppa_args_hash_table *table;
      table = elf32_hppa_hash_table (info)->args_hash_table;

      /* Determine the size of the symbol extension section.  */
      elf32_hppa_args_hash_traverse (table,
				     elf32_hppa_size_symext,
				     &symext_chain_size);
    }

  /* Now create the section and set its size.  We'll fill in the
     contents later.  */
  symextn_sec = bfd_get_section_by_name (abfd, SYMEXTN_SECTION_NAME);
  if (symextn_sec == NULL)
    symextn_sec = bfd_make_section (abfd, SYMEXTN_SECTION_NAME);

  bfd_set_section_flags (abfd, symextn_sec,
			 SEC_LOAD | SEC_HAS_CONTENTS | SEC_DATA);
  symextn_sec->output_section = symextn_sec;
  symextn_sec->output_offset = 0;
  bfd_set_section_alignment (abfd, symextn_sec, 2);
  bfd_set_section_size (abfd, symextn_sec, symext_chain_size);
}

/* Called for each entry in the args location hash table.  For each
   entry we bump the size pointer by 2 records (16 bytes).  */

static boolean
elf32_hppa_size_symext (gen_entry, in_args)
     struct bfd_hash_entry *gen_entry;
     PTR in_args;
{
  bfd_size_type *sizep = (bfd_size_type *)in_args;

  *sizep += 2 * ELF32_PARISC_SX_SIZE;
  return true;
}

/* Backend routine called by the linker for each output symbol.

   For PA ELF we use this opportunity to add an appropriate entry
   to the symbol extension chain for function symbols.  */

static boolean
elf32_hppa_link_output_symbol_hook (abfd, info, name, sym, section)
     bfd *abfd;
     struct bfd_link_info *info;
     const char *name;
     Elf_Internal_Sym *sym;
     asection *section;
{
  char *new_name;
  unsigned int len, index;
  struct elf32_hppa_args_hash_table *args_hash_table;
  struct elf32_hppa_args_hash_entry *args_hash;

  /* If the args hash table is NULL, then we've encountered an error
     of some sorts (for example, an undefined symbol).  In that case
     we've got nothing else to do.

     NOTE: elf_link_output_symbol will abort if we return false here!  */
  if (elf32_hppa_hash_table (info)->args_hash_table == NULL)
    return true;

  index = elf32_hppa_hash_table (info)->output_symbol_count++;

  /* We need to look up this symbol in the args hash table to see if
     it has argument relocation bits.  */
  if (ELF_ST_TYPE (sym->st_info) != STT_FUNC)
    return true;

  /* We know it's a function symbol of some kind.  */
  len = strlen (name) + 1;
  if (ELF_ST_BIND (sym->st_info) == STB_LOCAL)
    len += 9;

  new_name = bfd_malloc (len);
  if (new_name == NULL)
    return false;

  strcpy (new_name, name);
  if (ELF_ST_BIND (sym->st_info) == STB_LOCAL)
    sprintf (new_name + len - 10, "_%08x", (int)section);

  /* Now that we have the unique name, we can look it up in the
     args hash table.  */
  args_hash_table = elf32_hppa_hash_table (info)->args_hash_table;
  args_hash = elf32_hppa_args_hash_lookup (args_hash_table, new_name,
					   false, false);
  free (new_name);
  if (args_hash == NULL)
    return true;

  /* We know this symbol has arg reloc bits.  */
  add_entry_to_symext_chain (abfd, args_hash->arg_bits,
			     index, &symext_rootP, &symext_lastP);
  return true;
}

/* Perform any processing needed late in the object file writing process.
   For PA ELF we build and set the contents of the symbol extension
   section.  */

static void
elf32_hppa_backend_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker;
{
  asection *symextn_sec;
  unsigned int i;

  /* Now build the symbol extension section.  */
  if (symext_chain_size == 0)
    return;

  if (! linker)
    {
      /* We were not called from the backend linker, so we still need
	 to build the symbol extension chain.

         Look at each symbol, adding the appropriate information to the
	 symbol extension section list as necessary.  */
      for (i = 0; i < abfd->symcount; i++)
	{
	  elf_symbol_type *symbol = (elf_symbol_type *) abfd->outsymbols[i];

	  /* Only functions ever need an entry in the symbol extension
	     section.  */
	  if (!(symbol->symbol.flags & BSF_FUNCTION))
	    continue;

	  /* And only if they specify the locations of their arguments.  */
	  if (symbol->tc_data.hppa_arg_reloc == 0)
	    continue;

	  /* Add this symbol's information to the chain.  */
	  add_entry_to_symext_chain (abfd, symbol->tc_data.hppa_arg_reloc,
				     symbol->symbol.udata.i, &symext_rootP,
				     &symext_lastP);
	}
    }

  /* Now fill in the contents of the symbol extension section.  */
  elf_hppa_tc_make_sections (abfd, symext_rootP);

  /* And attach that as the section's contents.  */
  symextn_sec = bfd_get_section_by_name (abfd, SYMEXTN_SECTION_NAME);
  if (symextn_sec == (asection *) 0)
    abort();

  symextn_sec->contents = (void *)symextn_contents;

  bfd_set_section_contents (abfd, symextn_sec, symextn_sec->contents,
			    symextn_sec->output_offset, symextn_sec->_raw_size);
}

/* Update the symbol extention chain to include the symbol pointed to
   by SYMBOLP if SYMBOLP is a function symbol.  Used internally and by GAS.  */

static void
add_entry_to_symext_chain (abfd, arg_reloc, sym_idx, symext_root, symext_last)
     bfd *abfd;
     unsigned int arg_reloc;
     unsigned int sym_idx;
     symext_chainS **symext_root;
     symext_chainS **symext_last;
{
  symext_chainS *symextP;

  /* Allocate memory and initialize this entry.  */
  symextP = (symext_chainS *) bfd_alloc (abfd, sizeof (symext_chainS) * 2);
  if (!symextP)
    abort();			/* FIXME */

  symextP[0].entry = ELF32_PARISC_SX_WORD (PARISC_SXT_SYMNDX, sym_idx);
  symextP[0].next = &symextP[1];

  symextP[1].entry = ELF32_PARISC_SX_WORD (PARISC_SXT_ARG_RELOC, arg_reloc);
  symextP[1].next = NULL;

  /* Now update the chain itself so it can be walked later to build
     the symbol extension section.  */
  if (*symext_root == NULL)
    {
      *symext_root = &symextP[0];
      *symext_last = &symextP[1];
    }
  else
    {
      (*symext_last)->next = &symextP[0];
      *symext_last = &symextP[1];
    }
}

/* Build the symbol extension section.  */

static void
elf_hppa_tc_make_sections (abfd, symext_root)
     bfd *abfd;
     symext_chainS *symext_root;
{
  symext_chainS *symextP;
  unsigned int i;
  asection *symextn_sec;

  symextn_sec = bfd_get_section_by_name (abfd, SYMEXTN_SECTION_NAME);

  /* Grab some memory for the contents of the symbol extension section
     itself.  */
  symextn_contents = (bfd_byte *) bfd_zalloc (abfd,
					      symextn_sec->_raw_size);
  if (!symextn_contents)
    abort();			/* FIXME */

  /* Fill in the contents of the symbol extension chain.  */
  for (i = 0, symextP = symext_root; symextP; symextP = symextP->next, ++i)
    ELF32_PARISC_SX_PUT (abfd, (bfd_vma) symextP->entry,
			 symextn_contents + i * ELF32_PARISC_SX_SIZE);

  return;
}

/* Do some PA ELF specific work after reading in the symbol table.
   In particular attach the argument relocation from the
   symbol extension section to the appropriate symbols.  */

static boolean
elf32_hppa_backend_symbol_table_processing (abfd, esyms,symcnt)
     bfd *abfd;
     elf_symbol_type *esyms;
     unsigned int symcnt;
{
  Elf32_Internal_Shdr *symextn_hdr =
    bfd_elf_find_section (abfd, SYMEXTN_SECTION_NAME);
  unsigned int i, current_sym_idx = 0;

  /* If no symbol extension existed, then all symbol extension information
     is assumed to be zero.  */
  if (symextn_hdr == NULL)
    {
      for (i = 0; i < symcnt; i++)
	esyms[i].tc_data.hppa_arg_reloc = 0;
      return (true);
    }

  /* FIXME:  Why not use bfd_get_section_contents here?  Also should give
     memory back when we're done.  */
  /* Allocate a buffer of the appropriate size for the symextn section.  */
  symextn_hdr->contents = bfd_zalloc(abfd,symextn_hdr->sh_size);
  if (!symextn_hdr->contents)
    return false;

  /* Read in the symextn section.  */
  if (bfd_seek (abfd, symextn_hdr->sh_offset, SEEK_SET) == -1)
    return false;
  if (bfd_read ((PTR) symextn_hdr->contents, 1, symextn_hdr->sh_size, abfd)
      != symextn_hdr->sh_size)
    return false;

  /* Parse entries in the symbol extension section, updating the symtab
     entries as we go */
  for (i = 0; i < symextn_hdr->sh_size / ELF32_PARISC_SX_SIZE; i++)
    {
      symext_entryS se =
	ELF32_PARISC_SX_GET (abfd,
			     ((unsigned char *)symextn_hdr->contents
			      + i * ELF32_PARISC_SX_SIZE));
      unsigned int se_value = ELF32_PARISC_SX_VAL (se);
      unsigned int se_type = ELF32_PARISC_SX_TYPE (se);

      switch (se_type)
	{
	case PARISC_SXT_NULL:
	  break;

	case PARISC_SXT_SYMNDX:
	  if (se_value >= symcnt)
	    {
	      bfd_set_error (bfd_error_bad_value);
	      return (false);
	    }
	  current_sym_idx = se_value - 1;
	  break;

	case PARISC_SXT_ARG_RELOC:
	  esyms[current_sym_idx].tc_data.hppa_arg_reloc = se_value;
	  break;

	default:
	  bfd_set_error (bfd_error_bad_value);
	  return (false);
	}
    }
  return (true);
}

/* Read and attach the symbol extension information for the symbols
   in INPUT_BFD to the argument location hash table.  Handle locals
   if DO_LOCALS is true; likewise for globals when DO_GLOBALS is true.  */

static boolean
elf32_hppa_read_symext_info (input_bfd, symtab_hdr, args_hash_table, local_syms)
     bfd *input_bfd;
     Elf_Internal_Shdr *symtab_hdr;
     struct elf32_hppa_args_hash_table *args_hash_table;
     Elf_Internal_Sym *local_syms;
{
  asection *symextn_sec;
  bfd_byte *contents;
  unsigned int i, n_entries, current_index = 0;

  /* Get the symbol extension section for this BFD.  If no section exists
     then there's nothing to do.  Likewise if the section exists, but
     has no contents.  */
  symextn_sec = bfd_get_section_by_name (input_bfd, SYMEXTN_SECTION_NAME);
  if (symextn_sec == NULL)
    return true;

  /* Done separately so we can turn off SEC_HAS_CONTENTS (see below).  */
  if (symextn_sec->_raw_size == 0)
    {
      symextn_sec->flags &= ~SEC_HAS_CONTENTS;
      return true;
    }

  contents = (bfd_byte *) bfd_malloc ((size_t) symextn_sec->_raw_size);
  if (contents == NULL)
    return false;

  /* How gross.  We turn off SEC_HAS_CONTENTS for the input symbol extension
     sections to keep the generic ELF/BFD code from trying to do anything
     with them.  We have to undo that hack temporarily so that we can read
     in the contents with the generic code.  */
  symextn_sec->flags |= SEC_HAS_CONTENTS;
  if (bfd_get_section_contents (input_bfd, symextn_sec, contents,
				0, symextn_sec->_raw_size) == false)
    {
      symextn_sec->flags &= ~SEC_HAS_CONTENTS;
      free (contents);
      return false;
    }

  /* Gross.  Turn off SEC_HAS_CONTENTS for the input symbol extension
     sections (see above).  */
  symextn_sec->flags &= ~SEC_HAS_CONTENTS;

  n_entries = symextn_sec->_raw_size / ELF32_PARISC_SX_SIZE;
  for (i = 0; i < n_entries; i++)
    {
      symext_entryS entry =
	ELF32_PARISC_SX_GET (input_bfd, contents + i * ELF32_PARISC_SX_SIZE);
      unsigned int value = ELF32_PARISC_SX_VAL (entry);
      unsigned int type = ELF32_PARISC_SX_TYPE (entry);
      struct elf32_hppa_args_hash_entry *args_hash;

      switch (type)
	{
	case PARISC_SXT_NULL:
	  break;

	case PARISC_SXT_SYMNDX:
	  if (value >= symtab_hdr->sh_size / sizeof (Elf32_External_Sym))
	    {
	      bfd_set_error (bfd_error_bad_value);
	      free (contents);
	      return false;
	    }
	  current_index = value;
	  break;

	case PARISC_SXT_ARG_RELOC:
	  if (current_index < symtab_hdr->sh_info)
	    {
	      Elf_Internal_Shdr *hdr;
	      char *new_name;
	      const char *sym_name;
	      asection *sym_sec;
	      unsigned int len;

	      hdr = elf_elfsections (input_bfd)[local_syms[current_index].st_shndx];
	      sym_sec = hdr->bfd_section;
	      sym_name = bfd_elf_string_from_elf_section (input_bfd,
						      symtab_hdr->sh_link,
	 			        local_syms[current_index].st_name);
	      len = strlen (sym_name) + 10;
	      new_name = bfd_malloc (len);
	      if (new_name == NULL)
		{
		  free (contents);
		  return false;
		}
	      strcpy (new_name, sym_name);
	      sprintf (new_name + len - 10, "_%08x", (int)sym_sec);

	      /* This is a global symbol with argument location info.
		 We need to enter it into the hash table.  */
	      args_hash = elf32_hppa_args_hash_lookup (args_hash_table,
						       new_name, true,
						       true);
	      free (new_name);
	      if (args_hash == NULL)
		{
		  free (contents);
		  return false;
		}
	      args_hash->arg_bits = value;
	      break;
	    }
	  else if (current_index >= symtab_hdr->sh_info)
	    {
	      struct elf_link_hash_entry *h;

	      current_index -= symtab_hdr->sh_info;
	      h = elf_sym_hashes(input_bfd)[current_index];
	      /* This is a global symbol with argument location
		 information.  We need to enter it into the hash table.  */
	      args_hash = elf32_hppa_args_hash_lookup (args_hash_table,
						       h->root.root.string,
						       true, true);
	      if (args_hash == NULL)
		{
		  bfd_set_error (bfd_error_bad_value);
		  free (contents);
		  return false;
		}
	      args_hash->arg_bits = value;
	      break;
	    }
	  else
	    break;

	default:
	  bfd_set_error (bfd_error_bad_value);
	  free (contents);
	  return false;
	}
    }
  free (contents);
  return true;
}

/* Undo the generic ELF code's subtraction of section->vma from the
   value of each external symbol.  */

static boolean
elf32_hppa_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     const Elf_Internal_Sym *sym;
     const char **namep;
     flagword *flagsp;
     asection **secp;
     bfd_vma *valp;
{
  *valp += (*secp)->vma;
  return true;
}

/* Determine the name of the stub needed to perform a call assuming the
   argument relocation bits for caller and callee are in CALLER and CALLEE
   for a call from LOCATION to DESTINATION.  Copy the name into STUB_NAME.  */

static void
elf32_hppa_name_of_stub (caller, callee, location, destination, stub_name)
     unsigned int caller, callee;
     bfd_vma location, destination;
     char *stub_name;
{
  arg_reloc_type arg_reloc_types[5];

  if (elf32_hppa_arg_reloc_needed (caller, callee, arg_reloc_types))
    {
      arg_reloc_location i;
      /* Fill in the basic template.  */
      strcpy (stub_name, "__XX_XX_XX_XX_XX_stub_");

      /* Now fix the specifics.  */
      for (i = ARG0; i <= RET; i++)
	switch (arg_reloc_types[i])
	  {
	    case NO:
	      stub_name[3 * i + 2] = 'N';
	      stub_name[3 * i + 3] = 'O';
	      break;
	    case GF:
	      stub_name[3 * i + 2] = 'G';
	      stub_name[3 * i + 3] = 'F';
	      break;
	    case FG:
	      stub_name[3 * i + 2] = 'F';
	      stub_name[3 * i + 3] = 'G';
	      break;
	    case GD:
	      stub_name[3 * i + 2] = 'G';
	      stub_name[3 * i + 3] = 'D';
	      break;
	    case DG:
	      stub_name[3 * i + 2] = 'D';
	      stub_name[3 * i + 3] = 'G';
	      break;
	  }
    }
  else
    strcpy (stub_name, "_____long_branch_stub_");
}

/* Determine if an argument relocation stub is needed to perform a
   call assuming the argument relocation bits for caller and callee
   are in CALLER and CALLEE.  Place the type of relocations (if any)
   into stub_types_p.  */

static boolean
elf32_hppa_arg_reloc_needed (caller, callee, stub_types)
     unsigned int caller, callee;
     arg_reloc_type stub_types[5];
{
  /* Special case for no relocations.  */
  if (caller == 0 || callee == 0)
    return 0;
  else
    {
      arg_location caller_loc[5];
      arg_location callee_loc[5];

      /* Extract the location information for the argument and return
	 value on both the caller and callee sides.  */
      caller_loc[ARG0] = EXTRACT_ARBITS (caller, ARG0);
      callee_loc[ARG0] = EXTRACT_ARBITS (callee, ARG0);
      caller_loc[ARG1] = EXTRACT_ARBITS (caller, ARG1);
      callee_loc[ARG1] = EXTRACT_ARBITS (callee, ARG1);
      caller_loc[ARG2] = EXTRACT_ARBITS (caller, ARG2);
      callee_loc[ARG2] = EXTRACT_ARBITS (callee, ARG2);
      caller_loc[ARG3] = EXTRACT_ARBITS (caller, ARG3);
      callee_loc[ARG3] = EXTRACT_ARBITS (callee, ARG3);
      caller_loc[RET] = EXTRACT_ARBITS (caller, RET);
      callee_loc[RET] = EXTRACT_ARBITS (callee, RET);

      /* Check some special combinations.  This is necessary to
	 deal with double precision FP arguments.  */
      if (caller_loc[ARG0] == AR_FU || caller_loc[ARG1] == AR_FU)
	{
	  caller_loc[ARG0] = AR_FPDBL1;
	  caller_loc[ARG1] = AR_NO;
	}
      if (caller_loc[ARG2] == AR_FU || caller_loc[ARG3] == AR_FU)
	{
	  caller_loc[ARG2] = AR_FPDBL2;
	  caller_loc[ARG3] = AR_NO;
	}
      if (callee_loc[ARG0] == AR_FU || callee_loc[ARG1] == AR_FU)
	{
	  callee_loc[ARG0] = AR_FPDBL1;
	  callee_loc[ARG1] = AR_NO;
	}
      if (callee_loc[ARG2] == AR_FU || callee_loc[ARG3] == AR_FU)
	{
	  callee_loc[ARG2] = AR_FPDBL2;
	  callee_loc[ARG3] = AR_NO;
	}

      /* Now look up any relocation needed for each argument and the
	 return value.  */
      stub_types[ARG0] = arg_mismatches[caller_loc[ARG0]][callee_loc[ARG0]];
      stub_types[ARG1] = arg_mismatches[caller_loc[ARG1]][callee_loc[ARG1]];
      stub_types[ARG2] = arg_mismatches[caller_loc[ARG2]][callee_loc[ARG2]];
      stub_types[ARG3] = arg_mismatches[caller_loc[ARG3]][callee_loc[ARG3]];
      stub_types[RET] = ret_mismatches[caller_loc[RET]][callee_loc[RET]];

      return (stub_types[ARG0] != NO
	      || stub_types[ARG1] != NO
	      || stub_types[ARG2] != NO
	      || stub_types[ARG3] != NO
	      || stub_types[RET] != NO);
    }
}

/* Compute the size of the stub needed to call from LOCATION to DESTINATION
   (a function named SYM_NAME), with argument relocation bits CALLER and
   CALLEE.  Return zero if no stub is needed to perform such a call.  */

static unsigned int
elf32_hppa_size_of_stub (callee, caller, location, destination, sym_name)
     unsigned int callee, caller;
     bfd_vma location, destination;
     const char *sym_name;
{
  arg_reloc_type arg_reloc_types[5];

  /* Determine if a long branch or argument relocation stub is needed.
     If an argument relocation stub is needed, the relocation will be
     stored into arg_reloc_types.  */
  if (!(((int)(location - destination) > 0x3ffff)
	|| ((int)(location - destination) < (int)0xfffc0000)
	|| elf32_hppa_arg_reloc_needed (caller, callee, arg_reloc_types)))
    return 0;

  /* Some kind of stub is needed.  Determine how big it needs to be.
     First check for argument relocation stubs as they also handle
     long calls.  Then check for long calls to millicode and finally
     the normal long calls.  */
  if (arg_reloc_types[ARG0] != NO
      || arg_reloc_types[ARG1] != NO
      || arg_reloc_types[ARG2] != NO
      || arg_reloc_types[ARG3] != NO
      || arg_reloc_types[RET] != NO)
    {
      /* Some kind of argument relocation stub is needed.  */
      unsigned int len = 16;
      arg_reloc_location i;

      /* Each GR or FG relocation takes 2 insns, each GD or DG
	 relocation takes 3 insns.  Plus 4 more insns for the
         RP adjustment, ldil & (be | ble) and copy.  */
      for (i = ARG0; i <= RET; i++)
	switch (arg_reloc_types[i])
	  {
	    case GF:
	    case FG:
	      len += 8;
	      break;

	    case GD:
	    case DG:
	      len += 12;
	      break;

	    default:
	      break;
	  }

      /* Extra instructions are needed if we're relocating a return value.  */
      if (arg_reloc_types[RET] != NO)
	len += 12;

      return len;
    }
  else if (!strncmp ("$$", sym_name, 2)
      && strcmp ("$$dyncall", sym_name))
    return 12;
  else
    return 16;
}

/* Build one linker stub as defined by the stub hash table entry GEN_ENTRY.
   IN_ARGS contains the stub BFD and link info pointers.  */

static boolean
elf32_hppa_build_one_stub (gen_entry, in_args)
     struct bfd_hash_entry *gen_entry;
     PTR in_args;
{
  void **args = (void **)in_args;
  bfd *stub_bfd = (bfd *)args[0];
  struct bfd_link_info *info = (struct bfd_link_info *)args[1];
  struct elf32_hppa_stub_hash_entry *entry;
  struct elf32_hppa_stub_hash_table *stub_hash_table;
  bfd_byte *loc;
  symvalue sym_value;
  const char *sym_name;

  /* Initialize pointers to the stub hash table, the particular entry we
     are building a stub for, and where (in memory) we should place the stub
     instructions.  */
  entry = (struct elf32_hppa_stub_hash_entry *)gen_entry;
  stub_hash_table = elf32_hppa_hash_table(info)->stub_hash_table;
  loc = stub_hash_table->location;

  /* Make a note of the offset within the stubs for this entry.  */
  entry->offset = stub_hash_table->offset;

  /* The symbol's name starts at offset 22.  */
  sym_name = entry->root.string + 22;

  sym_value = (entry->target_value
	       + entry->target_section->output_offset
	       + entry->target_section->output_section->vma);

  if (strncmp ("_____long_branch_stub_", entry->root.string, 22))
    {
      /* This must be an argument or return value relocation stub.  */
      unsigned long insn;
      arg_reloc_location i;
      bfd_byte *begin_loc = loc;

      /* First the return pointer adjustment.  Depending on exact calling
	 sequence this instruction may be skipped.  */
      bfd_put_32 (stub_bfd, LDO_M4_R31_R31, loc);
      loc += 4;

      /* If we are relocating a return value, then we're going to have
	 to return into the stub.  So we have to save off the user's
	 return pointer into the stack at RP'.  */
      if (strncmp (entry->root.string + 14, "NO", 2))
	{
	  bfd_put_32 (stub_bfd, STW_R31_M8R30, loc);
	  loc += 4;
	}

      /* Iterate over the argument relocations, emitting instructions
	 to move them around as necessary.  */
      for (i = ARG0; i <= ARG3; i++)
	{
	  if (!strncmp (entry->root.string + 3 * i + 2, "GF", 2))
	    {
	      bfd_put_32 (stub_bfd, STW_ARG_M16R30 | ((26 - i) << 16), loc);
	      bfd_put_32 (stub_bfd, FLDW_M16R30_FARG | (4 + i), loc + 4);
	      loc += 8;
	    }
	  else if (!strncmp (entry->root.string + 3 * i + 2, "FG", 2))
	    {
	      bfd_put_32 (stub_bfd, FSTW_FARG_M16R30 | (4 + i), loc);
	      bfd_put_32 (stub_bfd, LDW_M16R30_ARG | ((26 - i) << 16), loc + 4);
	      loc += 8;
	    }
	  else if (!strncmp (entry->root.string + 3 * i + 2, "GD", 2))
	    {
	      bfd_put_32 (stub_bfd, STW_ARG_M12R30 | ((26 - i) << 16), loc);
	      bfd_put_32 (stub_bfd, STW_ARG_M16R30 | ((25 - i) << 16), loc + 4);
	      bfd_put_32 (stub_bfd, FLDD_M16R30_FARG | (5 + i), loc + 8);
	      loc += 12;
	    }
	  else if (!strncmp (entry->root.string + 3 * i + 2, "DG", 2))
	    {
	      bfd_put_32 (stub_bfd, FSTD_FARG_M16R30 | (5 + i), loc);
	      bfd_put_32 (stub_bfd, LDW_M12R30_ARG | ((26 - i) << 16), loc + 4);
	      bfd_put_32 (stub_bfd, LDW_M16R30_ARG | ((25 - i) << 16), loc + 8);
	      loc += 12;
	    }
	}

      /* Load the high bits of the target address into %r1.  */
      insn = hppa_rebuild_insn (stub_bfd, LDIL_R1,
				hppa_field_adjust (sym_value, 0, e_lrsel), 21);
      bfd_put_32 (stub_bfd, insn, loc);
      loc += 4;

      /* If we are relocating a return value, then we're going to have
	 to return into the stub, then perform the return value relocation.  */
      if (strncmp (entry->root.string + 14, "NO", 2))
	{
	  /* To return to the stub we "ble" to the target and copy the return
	     pointer from %r31 into %r2.  */
	  insn = hppa_rebuild_insn (stub_bfd,
				    BLE_SR4_R1,
				    hppa_field_adjust (sym_value, 0,
						       e_rrsel) >> 2,
				    17);
	  bfd_put_32 (stub_bfd, insn, loc);
	  bfd_put_32 (stub_bfd, COPY_R31_R2, loc + 4);

	  /* Reload the return pointer for our caller from the stack.  */
	  bfd_put_32 (stub_bfd, LDW_M8R30_R31, loc + 8);
	  loc += 12;

	  /* Perform the return value relocation.  */
	  if (!strncmp (entry->root.string + 14, "GF", 2))
	    {
	      bfd_put_32 (stub_bfd, STW_ARG_M16R30 | (28 << 16), loc);
	      bfd_put_32 (stub_bfd, FLDW_M16R30_FARG | 4, loc + 4);
	      loc += 8;
	    }
	  else if (!strncmp (entry->root.string + 14, "FG", 2))
	    {
	      bfd_put_32 (stub_bfd, FSTW_FARG_M16R30 | 4, loc);
	      bfd_put_32 (stub_bfd, LDW_M16R30_ARG | (28 << 16), loc + 4);
	      loc += 8;
	    }
	  else if (!strncmp (entry->root.string + 2, "GD", 2))
	    {
	      bfd_put_32 (stub_bfd, STW_ARG_M12R30 | (28 << 16), loc);
	      bfd_put_32 (stub_bfd, STW_ARG_M16R30 | (29 << 16), loc + 4);
	      bfd_put_32 (stub_bfd, FLDD_M16R30_FARG | 4, loc + 8);
	      loc += 12;
	    }
	  else if (!strncmp (entry->root.string + 2, "DG", 2))
	    {
	      bfd_put_32 (stub_bfd, FSTD_FARG_M16R30 | 4, loc);
	      bfd_put_32 (stub_bfd, LDW_M12R30_ARG | (28 << 16), loc + 4);
	      bfd_put_32 (stub_bfd, LDW_M16R30_ARG | (29 << 16), loc + 8);
	      loc += 12;
	    }
	  /* Branch back to the user's code now.  */
	  bfd_put_32 (stub_bfd, BV_N_0_R31, loc);
	  loc += 4;
	}
      else
	{
	  /* No return value relocation, so we can simply "be" to the
	     target and copy out return pointer into %r2.  */
	  insn = hppa_rebuild_insn (stub_bfd, BE_SR4_R1,
				    hppa_field_adjust (sym_value, 0,
						       e_rrsel) >> 2, 17);
	  bfd_put_32 (stub_bfd, insn, loc);
	  bfd_put_32 (stub_bfd, COPY_R31_R2, loc + 4);
	  loc += 8;
	}

      /* Update the location and offsets.  */
      stub_hash_table->location += (loc - begin_loc);
      stub_hash_table->offset += (loc - begin_loc);
    }
  else
    {
      /* Create one of two variant long branch stubs.  One for $$dyncall and
	 normal calls, the other for calls to millicode.  */
      unsigned long insn;
      int millicode_call = 0;

      if (!strncmp ("$$", sym_name, 2) && strcmp ("$$dyncall", sym_name))
	millicode_call = 1;

      /* First the return pointer adjustment.  Depending on exact calling
	 sequence this instruction may be skipped.  */
      bfd_put_32 (stub_bfd, LDO_M4_R31_R31, loc);

      /* The next two instructions are the long branch itself.  A long branch
	 is formed with "ldil" loading the upper bits of the target address
	 into a register, then branching with "be" which adds in the lower bits.
	 Long branches to millicode nullify the delay slot of the "be".  */
      insn = hppa_rebuild_insn (stub_bfd, LDIL_R1,
				hppa_field_adjust (sym_value, 0, e_lrsel), 21);
      bfd_put_32 (stub_bfd, insn, loc + 4);
      insn = hppa_rebuild_insn (stub_bfd, BE_SR4_R1 | (millicode_call ? 2 : 0),
				hppa_field_adjust (sym_value, 0, e_rrsel) >> 2,
				17);
      bfd_put_32 (stub_bfd, insn, loc + 8);

      if (!millicode_call)
	{
	  /* The sequence to call this stub places the return pointer into %r31,
	     the final target expects the return pointer in %r2, so copy the
	      return pointer into the proper register.  */
	  bfd_put_32 (stub_bfd, COPY_R31_R2, loc + 12);

	  /* Update the location and offsets.  */
	  stub_hash_table->location += 16;
	  stub_hash_table->offset += 16;
	}
      else
	{
	  /* Update the location and offsets.  */
	  stub_hash_table->location += 12;
	  stub_hash_table->offset += 12;
	}

    }
  return true;
}

/* External entry points for sizing and building linker stubs.  */

/* Build all the stubs associated with the current output file.  The
   stubs are kept in a hash table attached to the main linker hash
   table.  This is called via hppaelf_finish in the linker.  */

boolean
elf32_hppa_build_stubs (stub_bfd, info)
     bfd *stub_bfd;
     struct bfd_link_info *info;
{
  /* The stub BFD only has one section.  */
  asection *stub_sec = stub_bfd->sections;
  struct elf32_hppa_stub_hash_table *table;
  unsigned int size;
  void *args[2];

  /* So we can pass both the BFD for the stubs and the link info
     structure to the routine which actually builds stubs.  */
  args[0] = stub_bfd;
  args[1] = info;

  /* Allocate memory to hold the linker stubs.  */
  size = bfd_section_size (stub_bfd, stub_sec);
  stub_sec->contents = (unsigned char *) bfd_zalloc (stub_bfd, size);
  if (stub_sec->contents == NULL)
    return false;
  table = elf32_hppa_hash_table(info)->stub_hash_table;
  table->location = stub_sec->contents;

  /* Build the stubs as directed by the stub hash table.  */
  elf32_hppa_stub_hash_traverse (table, elf32_hppa_build_one_stub, args);

  return true;
}

/* Determine and set the size of the stub section for a final link.

   The basic idea here is to examine all the relocations looking for
   PC-relative calls to a target that is unreachable with a "bl"
   instruction or calls where the caller and callee disagree on the
   location of their arguments or return value.  */

boolean
elf32_hppa_size_stubs (stub_bfd, output_bfd, link_info)
     bfd *stub_bfd;
     bfd *output_bfd;
     struct bfd_link_info *link_info;
{
  bfd *input_bfd;
  asection *section, *stub_sec = 0;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Sym *local_syms, *isym, **all_local_syms;
  Elf32_External_Sym *ext_syms, *esym;
  unsigned int i, index, bfd_count = 0;
  struct elf32_hppa_stub_hash_table *stub_hash_table = 0;
  struct elf32_hppa_args_hash_table *args_hash_table = 0;

  /* Create and initialize the stub hash table.  */
  stub_hash_table = ((struct elf32_hppa_stub_hash_table *)
		     bfd_malloc (sizeof (struct elf32_hppa_stub_hash_table)));
  if (!stub_hash_table)
    goto error_return;

  if (!elf32_hppa_stub_hash_table_init (stub_hash_table, stub_bfd,
					elf32_hppa_stub_hash_newfunc))
    goto error_return;

  /* Likewise for the argument location hash table.  */
  args_hash_table = ((struct elf32_hppa_args_hash_table *)
		     bfd_malloc (sizeof (struct elf32_hppa_args_hash_table)));
  if (!args_hash_table)
    goto error_return;

  if (!elf32_hppa_args_hash_table_init (args_hash_table,
					elf32_hppa_args_hash_newfunc))
    goto error_return;

  /* Attach the hash tables to the main hash table.  */
  elf32_hppa_hash_table(link_info)->stub_hash_table = stub_hash_table;
  elf32_hppa_hash_table(link_info)->args_hash_table = args_hash_table;

  /* Count the number of input BFDs.  */
  for (input_bfd = link_info->input_bfds;
       input_bfd != NULL;
       input_bfd = input_bfd->link_next)
     bfd_count++;

  /* We want to read in symbol extension records only once.  To do this
     we need to read in the local symbols in parallel and save them for
     later use; so hold pointers to the local symbols in an array.  */
  all_local_syms
    = (Elf_Internal_Sym **) bfd_malloc (sizeof (Elf_Internal_Sym *)
					* bfd_count);
  if (all_local_syms == NULL)
    goto error_return;
  memset (all_local_syms, 0, sizeof (Elf_Internal_Sym *) * bfd_count);

  /* Walk over all the input BFDs adding entries to the args hash table
     for all the external functions.  */
  for (input_bfd = link_info->input_bfds, index = 0;
       input_bfd != NULL;
       input_bfd = input_bfd->link_next, index++)
    {
      /* We'll need the symbol table in a second.  */
      symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
      if (symtab_hdr->sh_info == 0)
	continue;

      /* We need an array of the local symbols attached to the input bfd.
	 Unfortunately, we're going to have to read & swap them in.  */
      local_syms
	= (Elf_Internal_Sym *) bfd_malloc (symtab_hdr->sh_info
					   * sizeof (Elf_Internal_Sym));
      if (local_syms == NULL)
	{
	  for (i = 0; i < bfd_count; i++)
	    if (all_local_syms[i])
	      free (all_local_syms[i]);
	  free (all_local_syms);
	  goto error_return;
	}
      all_local_syms[index] = local_syms;

      ext_syms
	= (Elf32_External_Sym *) bfd_malloc (symtab_hdr->sh_info
					     * sizeof (Elf32_External_Sym));
      if (ext_syms == NULL)
	{
	  for (i = 0; i < bfd_count; i++)
	    if (all_local_syms[i])
	      free (all_local_syms[i]);
	  free (all_local_syms);
	  goto error_return;
	}

      if (bfd_seek (input_bfd, symtab_hdr->sh_offset, SEEK_SET) != 0
	  || bfd_read (ext_syms, 1,
		       (symtab_hdr->sh_info
			* sizeof (Elf32_External_Sym)), input_bfd)
	  != (symtab_hdr->sh_info * sizeof (Elf32_External_Sym)))
	{
	  for (i = 0; i < bfd_count; i++)
	    if (all_local_syms[i])
	      free (all_local_syms[i]);
	  free (all_local_syms);
	  free (ext_syms);
	  goto error_return;
	}

      /* Swap the local symbols in.  */
      isym = local_syms;
      esym = ext_syms;
      for (i = 0; i < symtab_hdr->sh_info; i++, esym++, isym++)
	 bfd_elf32_swap_symbol_in (input_bfd, esym, isym);

      /* Now we can free the external symbols.  */
      free (ext_syms);

      if (elf32_hppa_read_symext_info (input_bfd, symtab_hdr, args_hash_table,
				       local_syms) == false)
	{
	  for (i = 0; i < bfd_count; i++)
	    if (all_local_syms[i])
	      free (all_local_syms[i]);
	  free (all_local_syms);
	  goto error_return;
	}
    }

  /* Magic as we know the stub bfd only has one section.  */
  stub_sec = stub_bfd->sections;

  /* If generating a relocateable output file, then we don't
     have to examine the relocs.  */
  if (link_info->relocateable)
    {
      for (i = 0; i < bfd_count; i++)
	if (all_local_syms[i])
	  free (all_local_syms[i]);
      free (all_local_syms);
      return true;
    }

  /* Now that we have argument location information for all the global
     functions we can start looking for stubs.  */
  for (input_bfd = link_info->input_bfds, index = 0;
       input_bfd != NULL;
       input_bfd = input_bfd->link_next, index++)
    {
      /* We'll need the symbol table in a second.  */
      symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
      if (symtab_hdr->sh_info == 0)
	continue;

      local_syms = all_local_syms[index];

      /* Walk over each section attached to the input bfd.  */
      for (section = input_bfd->sections;
	   section != NULL;
	   section = section->next)
	{
	  Elf_Internal_Shdr *input_rel_hdr;
	  Elf32_External_Rela *external_relocs, *erelaend, *erela;
	  Elf_Internal_Rela *internal_relocs, *irelaend, *irela;

	  /* If there aren't any relocs, then there's nothing to do.  */
	  if ((section->flags & SEC_RELOC) == 0
	      || section->reloc_count == 0)
	    continue;

	  /* Allocate space for the external relocations.  */
	  external_relocs
	    = ((Elf32_External_Rela *)
	       bfd_malloc (section->reloc_count
			   * sizeof (Elf32_External_Rela)));
	  if (external_relocs == NULL)
	    {
	      for (i = 0; i < bfd_count; i++)
		if (all_local_syms[i])
		  free (all_local_syms[i]);
	      free (all_local_syms);
	      goto error_return;
	    }

	  /* Likewise for the internal relocations.  */
	  internal_relocs
	    = ((Elf_Internal_Rela *)
	       bfd_malloc (section->reloc_count * sizeof (Elf_Internal_Rela)));
	  if (internal_relocs == NULL)
	    {
	      free (external_relocs);
	      for (i = 0; i < bfd_count; i++)
		if (all_local_syms[i])
		  free (all_local_syms[i]);
	      free (all_local_syms);
	      goto error_return;
	    }

	  /* Read in the external relocs.  */
	  input_rel_hdr = &elf_section_data (section)->rel_hdr;
	  if (bfd_seek (input_bfd, input_rel_hdr->sh_offset, SEEK_SET) != 0
	      || bfd_read (external_relocs, 1, input_rel_hdr->sh_size,
			   input_bfd) != input_rel_hdr->sh_size)
	    {
	      free (external_relocs);
	      free (internal_relocs);
	      for (i = 0; i < bfd_count; i++)
		if (all_local_syms[i])
		  free (all_local_syms[i]);
	      free (all_local_syms);
	      goto error_return;
	    }

	  /* Swap in the relocs.  */
	  erela = external_relocs;
	  erelaend = erela + section->reloc_count;
	  irela = internal_relocs;
	  for (; erela < erelaend; erela++, irela++)
	    bfd_elf32_swap_reloca_in (input_bfd, erela, irela);

	  /* We're done with the external relocs, free them.  */
	  free (external_relocs);

	  /* Now examine each relocation.  */
	  irela = internal_relocs;
	  irelaend = irela + section->reloc_count;
	  for (; irela < irelaend; irela++)
	    {
	      long r_type, callee_args, caller_args, size_of_stub;
	      unsigned long r_index;
	      struct elf_link_hash_entry *hash;
	      struct elf32_hppa_stub_hash_entry *stub_hash;
	      struct elf32_hppa_args_hash_entry *args_hash;
	      Elf_Internal_Sym *sym;
	      asection *sym_sec;
	      const char *sym_name;
	      symvalue sym_value;
	      bfd_vma location, destination;
	      char *new_name = NULL;

	      r_type = ELF32_R_TYPE (irela->r_info);
	      r_index = ELF32_R_SYM (irela->r_info);

	      if (r_type < 0 || r_type >= (int) R_PARISC_UNIMPLEMENTED)
		{
		  bfd_set_error (bfd_error_bad_value);
		  free (internal_relocs);
		  for (i = 0; i < bfd_count; i++)
		    if (all_local_syms[i])
		      free (all_local_syms[i]);
		  free (all_local_syms);
		  goto error_return;
		}

	      /* Only look for stubs on call instructions or plabel
		 references.  */
	      if (r_type != R_PARISC_PCREL17F
		  && r_type != R_PARISC_PLABEL32
		  && r_type != R_PARISC_PLABEL21L
		  && r_type != R_PARISC_PLABEL14R)
		continue;

	      /* Now determine the call target, its name, value, section
		 and argument relocation bits.  */
	      hash = NULL;
	      sym = NULL;
	      sym_sec = NULL;
	      if (r_index < symtab_hdr->sh_info)
		{
		  /* It's a local symbol.  */
		  Elf_Internal_Shdr *hdr;

		  sym = local_syms + r_index;
		  hdr = elf_elfsections (input_bfd)[sym->st_shndx];
		  sym_sec = hdr->bfd_section;
		  sym_name = bfd_elf_string_from_elf_section (input_bfd,
							      symtab_hdr->sh_link,
							      sym->st_name);
		  sym_value = (ELF_ST_TYPE (sym->st_info) == STT_SECTION
			       ? 0 : sym->st_value);
		  destination = (sym_value
				 + sym_sec->output_offset
				 + sym_sec->output_section->vma);

		  /* Tack on an ID so we can uniquely identify this local
		     symbol in the stub or arg info hash tables.  */
		  new_name = bfd_malloc (strlen (sym_name) + 10);
		  if (new_name == 0)
		    {
		      free (internal_relocs);
		      for (i = 0; i < bfd_count; i++)
			if (all_local_syms[i])
			  free (all_local_syms[i]);
		      free (all_local_syms);
		      goto error_return;
		    }
		  sprintf (new_name, "%s_%08x", sym_name, (int)sym_sec);
		  sym_name = new_name;
		}
	      else
		{
		  /* It's an external symbol.  */
		  long index;

		  index = r_index - symtab_hdr->sh_info;
		  hash = elf_sym_hashes (input_bfd)[index];
		  if (hash->root.type == bfd_link_hash_defined
		      || hash->root.type == bfd_link_hash_defweak)
		    {
		      sym_sec = hash->root.u.def.section;
		      sym_name = hash->root.root.string;
		      sym_value = hash->root.u.def.value;
		      destination = (sym_value
				     + sym_sec->output_offset
				     + sym_sec->output_section->vma);
		    }
		  else
		    {
		      bfd_set_error (bfd_error_bad_value);
		      free (internal_relocs);
		      for (i = 0; i < bfd_count; i++)
			if (all_local_syms[i])
			  free (all_local_syms[i]);
		      free (all_local_syms);
		      goto error_return;
		    }
		}

	      args_hash = elf32_hppa_args_hash_lookup (args_hash_table,
						       sym_name, false, false);

	      /* Get both caller and callee argument information.  */
	      if (args_hash == NULL)
		callee_args = 0;
	      else
		callee_args = args_hash->arg_bits;

	      /* For calls get the caller's bits from the addend of
		 the call relocation.  For PLABELS the caller's bits
		 are assumed to have all args & return values in general
		 registers (0x155).  */
	      if (r_type == R_PARISC_PCREL17F)
		caller_args = HPPA_R_ARG_RELOC (irela->r_addend);
	      else
		caller_args = 0x155;

	      /* Now determine where the call point is.  */
	      location = (section->output_offset
			  + section->output_section->vma
			  + irela->r_offset);

	      /* We only care about the destination for PCREL function
		 calls (eg. we don't care for PLABELS).  */
	      if (r_type != R_PARISC_PCREL17F)
		location = destination;

	      /* Determine what (if any) linker stub is needed and its
		 size (in bytes).  */
	      size_of_stub = elf32_hppa_size_of_stub (callee_args,
						      caller_args,
						      location,
						      destination,
						      sym_name);
	      if (size_of_stub != 0)
		{
		  char *stub_name;
		  unsigned int len;

		  /* Get the name of this stub.  */
		  len = strlen (sym_name);
		  len += 23;

		  stub_name = bfd_malloc (len);
		  if (!stub_name)
		    {
		      /* Because sym_name was mallocd above for local
			 symbols.  */
		      if (r_index < symtab_hdr->sh_info)
			free (new_name);

		      free (internal_relocs);
		      for (i = 0; i < bfd_count; i++)
			if (all_local_syms[i])
			  free (all_local_syms[i]);
		      free (all_local_syms);
		      goto error_return;
		    }
		  elf32_hppa_name_of_stub (caller_args, callee_args,
					   location, destination, stub_name);
		  strcat (stub_name + 22, sym_name);

		  /* Because sym_name was malloced above for local symbols.  */
		  if (r_index < symtab_hdr->sh_info)
		    free (new_name);

		  stub_hash
		    = elf32_hppa_stub_hash_lookup (stub_hash_table, stub_name,
						   false, false);
		  if (stub_hash != NULL)
		    {
		      /* The proper stub has already been created, nothing
			 else to do.  */
		      free (stub_name);
		    }
		  else
		    {
		      bfd_set_section_size (stub_bfd, stub_sec,
					    (bfd_section_size (stub_bfd,
							       stub_sec)
					     + size_of_stub));

		      /* Enter this entry into the linker stub hash table.  */
		      stub_hash
			= elf32_hppa_stub_hash_lookup (stub_hash_table,
						       stub_name, true, true);
		      if (stub_hash == NULL)
			{
			  free (stub_name);
			  free (internal_relocs);
			  for (i = 0; i < bfd_count; i++)
			    if (all_local_syms[i])
			      free (all_local_syms[i]);
			  free (all_local_syms);
			  goto error_return;
			}

		      /* We'll need these to determine the address that the
			 stub will branch to.  */
		      stub_hash->target_value = sym_value;
		      stub_hash->target_section = sym_sec;
		    }
		  free (stub_name);
		}
	    }
	  /* We're done with the internal relocs, free them.  */
	  free (internal_relocs);
	}
    }
  /* We're done with the local symbols, free them.  */
  for (i = 0; i < bfd_count; i++)
    if (all_local_syms[i])
      free (all_local_syms[i]);
  free (all_local_syms);
  return true;

error_return:
  /* Return gracefully, avoiding dangling references to the hash tables.  */
  if (stub_hash_table)
    {
      elf32_hppa_hash_table(link_info)->stub_hash_table = NULL;
      free (stub_hash_table);
    }
  if (args_hash_table)
    {
      elf32_hppa_hash_table(link_info)->args_hash_table = NULL;
      free (args_hash_table);
    }
  /* Set the size of the stub section to zero since we're never going
     to create them.   Avoids losing when we try to get its contents
     too.  */
  bfd_set_section_size (stub_bfd, stub_sec, 0);
  return false;
}

/* Misc BFD support code.  */
#define bfd_elf32_bfd_reloc_type_lookup		elf_hppa_reloc_type_lookup
#define bfd_elf32_bfd_is_local_label		hppa_elf_is_local_label

/* Symbol extension stuff.  */
#define bfd_elf32_set_section_contents		elf32_hppa_set_section_contents
#define elf_backend_symbol_table_processing \
  elf32_hppa_backend_symbol_table_processing
#define elf_backend_begin_write_processing \
  elf32_hppa_backend_begin_write_processing
#define elf_backend_final_write_processing \
  elf32_hppa_backend_final_write_processing

/* Stuff for the BFD linker.  */
#define elf_backend_relocate_section		elf32_hppa_relocate_section
#define elf_backend_add_symbol_hook		elf32_hppa_add_symbol_hook
#define elf_backend_link_output_symbol_hook \
  elf32_hppa_link_output_symbol_hook
#define bfd_elf32_bfd_link_hash_table_create \
  elf32_hppa_link_hash_table_create

#define TARGET_BIG_SYM		bfd_elf32_hppa_vec
#define TARGET_BIG_NAME		"elf32-hppa"
#define ELF_ARCH		bfd_arch_hppa
#define ELF_MACHINE_CODE	EM_PARISC
#define ELF_MAXPAGESIZE		0x1000

#include "elf32-target.h"
