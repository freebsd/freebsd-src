/* IA-64 support for 64-bit ELF
   Copyright 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

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
#include "opcode/ia64.h"
#include "elf/ia64.h"

/*
 * THE RULES for all the stuff the linker creates --
 *
 * GOT		Entries created in response to LTOFF or LTOFF_FPTR
 *		relocations.  Dynamic relocs created for dynamic
 *		symbols in an application; REL relocs for locals
 *		in a shared library.
 *
 * FPTR		The canonical function descriptor.  Created for local
 *		symbols in applications.  Descriptors for dynamic symbols
 *		and local symbols in shared libraries are created by
 *		ld.so.  Thus there are no dynamic relocs against these
 *		objects.  The FPTR relocs for such _are_ passed through
 *		to the dynamic relocation tables.
 *
 * FULL_PLT	Created for a PCREL21B relocation against a dynamic symbol.
 *		Requires the creation of a PLTOFF entry.  This does not
 *		require any dynamic relocations.
 *
 * PLTOFF	Created by PLTOFF relocations.  For local symbols, this
 *		is an alternate function descriptor, and in shared libraries
 *		requires two REL relocations.  Note that this cannot be
 *		transformed into an FPTR relocation, since it must be in
 *		range of the GP.  For dynamic symbols, this is a function
 *		descriptor for a MIN_PLT entry, and requires one IPLT reloc.
 *
 * MIN_PLT	Created by PLTOFF entries against dynamic symbols.  This
 *		does not reqire dynamic relocations.
 */

#define USE_RELA		/* we want RELA relocs, not REL */

#define NELEMS(a)	((int) (sizeof (a) / sizeof ((a)[0])))

typedef struct bfd_hash_entry *(*new_hash_entry_func)
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));

/* In dynamically (linker-) created sections, we generally need to keep track
   of the place a symbol or expression got allocated to. This is done via hash
   tables that store entries of the following type.  */

struct elfNN_ia64_dyn_sym_info
{
  /* The addend for which this entry is relevant.  */
  bfd_vma addend;

  /* Next addend in the list.  */
  struct elfNN_ia64_dyn_sym_info *next;

  bfd_vma got_offset;
  bfd_vma fptr_offset;
  bfd_vma pltoff_offset;
  bfd_vma plt_offset;
  bfd_vma plt2_offset;

  /* The symbol table entry, if any, that this was derrived from.  */
  struct elf_link_hash_entry *h;

  /* Used to count non-got, non-plt relocations for delayed sizing
     of relocation sections.  */
  struct elfNN_ia64_dyn_reloc_entry
  {
    struct elfNN_ia64_dyn_reloc_entry *next;
    asection *srel;
    int type;
    int count;
  } *reloc_entries;

  /* True when the section contents have been updated.  */
  unsigned got_done : 1;
  unsigned fptr_done : 1;
  unsigned pltoff_done : 1;

  /* True for the different kinds of linker data we want created.  */
  unsigned want_got : 1;
  unsigned want_fptr : 1;
  unsigned want_ltoff_fptr : 1;
  unsigned want_plt : 1;
  unsigned want_plt2 : 1;
  unsigned want_pltoff : 1;
};

struct elfNN_ia64_local_hash_entry
{
  struct bfd_hash_entry root;
  struct elfNN_ia64_dyn_sym_info *info;
};

struct elfNN_ia64_local_hash_table
{
  struct bfd_hash_table root;
  /* No additional fields for now.  */
};

struct elfNN_ia64_link_hash_entry
{
  struct elf_link_hash_entry root;
  struct elfNN_ia64_dyn_sym_info *info;
};

struct elfNN_ia64_link_hash_table
{
  /* The main hash table */
  struct elf_link_hash_table root;

  asection *got_sec;		/* the linkage table section (or NULL) */
  asection *rel_got_sec;	/* dynamic relocation section for same */
  asection *fptr_sec;		/* function descriptor table (or NULL) */
  asection *plt_sec;		/* the primary plt section (or NULL) */
  asection *pltoff_sec;		/* private descriptors for plt (or NULL) */
  asection *rel_pltoff_sec;	/* dynamic relocation section for same */

  bfd_size_type minplt_entries;	/* number of minplt entries */

  struct elfNN_ia64_local_hash_table loc_hash_table;
};

#define elfNN_ia64_hash_table(p) \
  ((struct elfNN_ia64_link_hash_table *) ((p)->hash))

static bfd_reloc_status_type elfNN_ia64_reloc
  PARAMS ((bfd *abfd, arelent *reloc, asymbol *sym, PTR data,
	   asection *input_section, bfd *output_bfd, char **error_message));
static reloc_howto_type * lookup_howto
  PARAMS ((unsigned int rtype));
static reloc_howto_type *elfNN_ia64_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type bfd_code));
static void elfNN_ia64_info_to_howto
  PARAMS ((bfd *abfd, arelent *bfd_reloc, ElfNN_Internal_Rela *elf_reloc));
static boolean elfNN_ia64_relax_section
  PARAMS((bfd *abfd, asection *sec, struct bfd_link_info *link_info,
	  boolean *again));
static boolean is_unwind_section_name
  PARAMS ((const char *));
static boolean elfNN_ia64_section_from_shdr
  PARAMS ((bfd *, ElfNN_Internal_Shdr *, char *));
static boolean elfNN_ia64_fake_sections
  PARAMS ((bfd *abfd, ElfNN_Internal_Shdr *hdr, asection *sec));
static void elfNN_ia64_final_write_processing
  PARAMS ((bfd *abfd, boolean linker));
static boolean elfNN_ia64_add_symbol_hook
  PARAMS ((bfd *abfd, struct bfd_link_info *info, const Elf_Internal_Sym *sym,
	   const char **namep, flagword *flagsp, asection **secp,
	   bfd_vma *valp));
static int elfNN_ia64_additional_program_headers
  PARAMS ((bfd *abfd));
static boolean elfNN_ia64_is_local_label_name
  PARAMS ((bfd *abfd, const char *name));
static boolean elfNN_ia64_dynamic_symbol_p
  PARAMS ((struct elf_link_hash_entry *h, struct bfd_link_info *info));
static boolean elfNN_ia64_local_hash_table_init
  PARAMS ((struct elfNN_ia64_local_hash_table *ht, bfd *abfd,
	   new_hash_entry_func new));
static struct bfd_hash_entry *elfNN_ia64_new_loc_hash_entry
  PARAMS ((struct bfd_hash_entry *entry, struct bfd_hash_table *table,
	   const char *string));
static struct bfd_hash_entry *elfNN_ia64_new_elf_hash_entry
  PARAMS ((struct bfd_hash_entry *entry, struct bfd_hash_table *table,
	   const char *string));
static struct bfd_link_hash_table *elfNN_ia64_hash_table_create
  PARAMS ((bfd *abfd));
static struct elfNN_ia64_local_hash_entry *elfNN_ia64_local_hash_lookup
  PARAMS ((struct elfNN_ia64_local_hash_table *table, const char *string,
	   boolean create, boolean copy));
static void elfNN_ia64_dyn_sym_traverse
  PARAMS ((struct elfNN_ia64_link_hash_table *ia64_info,
	   boolean (*func) (struct elfNN_ia64_dyn_sym_info *, PTR),
	   PTR info));
static boolean elfNN_ia64_create_dynamic_sections
  PARAMS ((bfd *abfd, struct bfd_link_info *info));
static struct elfNN_ia64_dyn_sym_info * get_dyn_sym_info
  PARAMS ((struct elfNN_ia64_link_hash_table *ia64_info,
	   struct elf_link_hash_entry *h,
	   bfd *abfd, const Elf_Internal_Rela *rel, boolean create));
static asection *get_got
  PARAMS ((bfd *abfd, struct bfd_link_info *info,
	   struct elfNN_ia64_link_hash_table *ia64_info));
static asection *get_fptr
  PARAMS ((bfd *abfd, struct bfd_link_info *info,
	   struct elfNN_ia64_link_hash_table *ia64_info));
static asection *get_pltoff
  PARAMS ((bfd *abfd, struct bfd_link_info *info,
	   struct elfNN_ia64_link_hash_table *ia64_info));
static asection *get_reloc_section
  PARAMS ((bfd *abfd, struct elfNN_ia64_link_hash_table *ia64_info,
	   asection *sec, boolean create));
static boolean count_dyn_reloc
  PARAMS ((bfd *abfd, struct elfNN_ia64_dyn_sym_info *dyn_i,
	   asection *srel, int type));
static boolean elfNN_ia64_check_relocs
  PARAMS ((bfd *abfd, struct bfd_link_info *info, asection *sec,
	   const Elf_Internal_Rela *relocs));
static boolean elfNN_ia64_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *info, struct elf_link_hash_entry *h));
static unsigned long global_sym_index
  PARAMS ((struct elf_link_hash_entry *h));
static boolean allocate_fptr
  PARAMS ((struct elfNN_ia64_dyn_sym_info *dyn_i, PTR data));
static boolean allocate_global_data_got
  PARAMS ((struct elfNN_ia64_dyn_sym_info *dyn_i, PTR data));
static boolean allocate_global_fptr_got
  PARAMS ((struct elfNN_ia64_dyn_sym_info *dyn_i, PTR data));
static boolean allocate_local_got
  PARAMS ((struct elfNN_ia64_dyn_sym_info *dyn_i, PTR data));
static boolean allocate_pltoff_entries
  PARAMS ((struct elfNN_ia64_dyn_sym_info *dyn_i, PTR data));
static boolean allocate_plt_entries
  PARAMS ((struct elfNN_ia64_dyn_sym_info *dyn_i, PTR data));
static boolean allocate_plt2_entries
  PARAMS ((struct elfNN_ia64_dyn_sym_info *dyn_i, PTR data));
static boolean allocate_dynrel_entries
  PARAMS ((struct elfNN_ia64_dyn_sym_info *dyn_i, PTR data));
static boolean elfNN_ia64_size_dynamic_sections
  PARAMS ((bfd *output_bfd, struct bfd_link_info *info));
static bfd_reloc_status_type elfNN_ia64_install_value
  PARAMS ((bfd *abfd, bfd_byte *hit_addr, bfd_vma val, unsigned int r_type));
static void elfNN_ia64_install_dyn_reloc
  PARAMS ((bfd *abfd, struct bfd_link_info *info, asection *sec,
	   asection *srel, bfd_vma offset, unsigned int type,
	   long dynindx, bfd_vma addend));
static bfd_vma set_got_entry
  PARAMS ((bfd *abfd, struct bfd_link_info *info,
	   struct elfNN_ia64_dyn_sym_info *dyn_i, long dynindx,
	   bfd_vma addend, bfd_vma value, unsigned int dyn_r_type));
static bfd_vma set_fptr_entry
  PARAMS ((bfd *abfd, struct bfd_link_info *info,
	   struct elfNN_ia64_dyn_sym_info *dyn_i,
	   bfd_vma value));
static bfd_vma set_pltoff_entry
  PARAMS ((bfd *abfd, struct bfd_link_info *info,
	   struct elfNN_ia64_dyn_sym_info *dyn_i,
	   bfd_vma value, boolean));
static boolean elfNN_ia64_final_link
  PARAMS ((bfd *abfd, struct bfd_link_info *info));
static boolean elfNN_ia64_relocate_section
  PARAMS ((bfd *output_bfd, struct bfd_link_info *info, bfd *input_bfd,
	   asection *input_section, bfd_byte *contents,
	   Elf_Internal_Rela *relocs, Elf_Internal_Sym *local_syms,
	   asection **local_sections));
static boolean elfNN_ia64_finish_dynamic_symbol
  PARAMS ((bfd *output_bfd, struct bfd_link_info *info,
	   struct elf_link_hash_entry *h, Elf_Internal_Sym *sym));
static boolean elfNN_ia64_finish_dynamic_sections
  PARAMS ((bfd *abfd, struct bfd_link_info *info));
static boolean elfNN_ia64_set_private_flags
  PARAMS ((bfd *abfd, flagword flags));
static boolean elfNN_ia64_copy_private_bfd_data
  PARAMS ((bfd *ibfd, bfd *obfd));
static boolean elfNN_ia64_merge_private_bfd_data
  PARAMS ((bfd *ibfd, bfd *obfd));
static boolean elfNN_ia64_print_private_bfd_data
  PARAMS ((bfd *abfd, PTR ptr));

/* ia64-specific relocation */

/* Perform a relocation.  Not much to do here as all the hard work is
   done in elfNN_ia64_final_link_relocate.  */
static bfd_reloc_status_type
elfNN_ia64_reloc (abfd, reloc, sym, data, input_section,
		  output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc;
     asymbol *sym ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  if (output_bfd)
    {
      reloc->address += input_section->output_offset;
      return bfd_reloc_ok;
    }
  *error_message = "Unsupported call to elfNN_ia64_reloc";
  return bfd_reloc_notsupported;
}

#define IA64_HOWTO(TYPE, NAME, SIZE, PCREL, IN)			\
  HOWTO (TYPE, 0, SIZE, 0, PCREL, 0, complain_overflow_signed,	\
	 elfNN_ia64_reloc, NAME, false, 0, 0, IN)

/* This table has to be sorted according to increasing number of the
   TYPE field.  */
static reloc_howto_type ia64_howto_table[] =
  {
    IA64_HOWTO (R_IA64_NONE,	    "NONE",	   0, false, true),

    IA64_HOWTO (R_IA64_IMM14,	    "IMM14",	   0, false, true),
    IA64_HOWTO (R_IA64_IMM22,	    "IMM22",	   0, false, true),
    IA64_HOWTO (R_IA64_IMM64,	    "IMM64",	   0, false, true),
    IA64_HOWTO (R_IA64_DIR32MSB,    "DIR32MSB",	   2, false, true),
    IA64_HOWTO (R_IA64_DIR32LSB,    "DIR32LSB",	   2, false, true),
    IA64_HOWTO (R_IA64_DIR64MSB,    "DIR64MSB",	   4, false, true),
    IA64_HOWTO (R_IA64_DIR64LSB,    "DIR64LSB",	   4, false, true),

    IA64_HOWTO (R_IA64_GPREL22,	    "GPREL22",	   0, false, true),
    IA64_HOWTO (R_IA64_GPREL64I,    "GPREL64I",	   0, false, true),
    IA64_HOWTO (R_IA64_GPREL32MSB,  "GPREL32MSB",  2, false, true),
    IA64_HOWTO (R_IA64_GPREL32LSB,  "GPREL32LSB",  2, false, true),
    IA64_HOWTO (R_IA64_GPREL64MSB,  "GPREL64MSB",  4, false, true),
    IA64_HOWTO (R_IA64_GPREL64LSB,  "GPREL64LSB",  4, false, true),

    IA64_HOWTO (R_IA64_LTOFF22,	    "LTOFF22",	   0, false, true),
    IA64_HOWTO (R_IA64_LTOFF64I,    "LTOFF64I",	   0, false, true),

    IA64_HOWTO (R_IA64_PLTOFF22,    "PLTOFF22",	   0, false, true),
    IA64_HOWTO (R_IA64_PLTOFF64I,   "PLTOFF64I",   0, false, true),
    IA64_HOWTO (R_IA64_PLTOFF64MSB, "PLTOFF64MSB", 4, false, true),
    IA64_HOWTO (R_IA64_PLTOFF64LSB, "PLTOFF64LSB", 4, false, true),

    IA64_HOWTO (R_IA64_FPTR64I,	    "FPTR64I",	   0, false, true),
    IA64_HOWTO (R_IA64_FPTR32MSB,   "FPTR32MSB",   2, false, true),
    IA64_HOWTO (R_IA64_FPTR32LSB,   "FPTR32LSB",   2, false, true),
    IA64_HOWTO (R_IA64_FPTR64MSB,   "FPTR64MSB",   4, false, true),
    IA64_HOWTO (R_IA64_FPTR64LSB,   "FPTR64LSB",   4, false, true),

    IA64_HOWTO (R_IA64_PCREL60B,    "PCREL60B",	   0, true, true),
    IA64_HOWTO (R_IA64_PCREL21B,    "PCREL21B",	   0, true, true),
    IA64_HOWTO (R_IA64_PCREL21M,    "PCREL21M",	   0, true, true),
    IA64_HOWTO (R_IA64_PCREL21F,    "PCREL21F",	   0, true, true),
    IA64_HOWTO (R_IA64_PCREL32MSB,  "PCREL32MSB",  2, true, true),
    IA64_HOWTO (R_IA64_PCREL32LSB,  "PCREL32LSB",  2, true, true),
    IA64_HOWTO (R_IA64_PCREL64MSB,  "PCREL64MSB",  4, true, true),
    IA64_HOWTO (R_IA64_PCREL64LSB,  "PCREL64LSB",  4, true, true),

    IA64_HOWTO (R_IA64_LTOFF_FPTR22, "LTOFF_FPTR22", 0, false, true),
    IA64_HOWTO (R_IA64_LTOFF_FPTR64I, "LTOFF_FPTR64I", 0, false, true),
    IA64_HOWTO (R_IA64_LTOFF_FPTR64MSB, "LTOFF_FPTR64MSB", 4, false, true),
    IA64_HOWTO (R_IA64_LTOFF_FPTR64LSB, "LTOFF_FPTR64LSB", 4, false, true),

    IA64_HOWTO (R_IA64_SEGREL32MSB, "SEGREL32MSB", 2, false, true),
    IA64_HOWTO (R_IA64_SEGREL32LSB, "SEGREL32LSB", 2, false, true),
    IA64_HOWTO (R_IA64_SEGREL64MSB, "SEGREL64MSB", 4, false, true),
    IA64_HOWTO (R_IA64_SEGREL64LSB, "SEGREL64LSB", 4, false, true),

    IA64_HOWTO (R_IA64_SECREL32MSB, "SECREL32MSB", 2, false, true),
    IA64_HOWTO (R_IA64_SECREL32LSB, "SECREL32LSB", 2, false, true),
    IA64_HOWTO (R_IA64_SECREL64MSB, "SECREL64MSB", 4, false, true),
    IA64_HOWTO (R_IA64_SECREL64LSB, "SECREL64LSB", 4, false, true),

    IA64_HOWTO (R_IA64_REL32MSB,    "REL32MSB",	   2, false, true),
    IA64_HOWTO (R_IA64_REL32LSB,    "REL32LSB",	   2, false, true),
    IA64_HOWTO (R_IA64_REL64MSB,    "REL64MSB",	   4, false, true),
    IA64_HOWTO (R_IA64_REL64LSB,    "REL64LSB",	   4, false, true),

    IA64_HOWTO (R_IA64_LTV32MSB,    "LTV32MSB",	   2, false, true),
    IA64_HOWTO (R_IA64_LTV32LSB,    "LTV32LSB",	   2, false, true),
    IA64_HOWTO (R_IA64_LTV64MSB,    "LTV64MSB",	   4, false, true),
    IA64_HOWTO (R_IA64_LTV64LSB,    "LTV64LSB",	   4, false, true),

    IA64_HOWTO (R_IA64_PCREL21BI,   "PCREL21BI",   0, true, true),
    IA64_HOWTO (R_IA64_PCREL22,     "PCREL22",     0, true, true),
    IA64_HOWTO (R_IA64_PCREL64I,    "PCREL64I",    0, true, true),

    IA64_HOWTO (R_IA64_IPLTMSB,	    "IPLTMSB",	   4, false, true),
    IA64_HOWTO (R_IA64_IPLTLSB,	    "IPLTLSB",	   4, false, true),
    IA64_HOWTO (R_IA64_COPY,	    "COPY",	   4, false, true),
    IA64_HOWTO (R_IA64_LTOFF22X,    "LTOFF22X",	   0, false, true),
    IA64_HOWTO (R_IA64_LDXMOV,	    "LDXMOV",	   0, false, true),

    IA64_HOWTO (R_IA64_TPREL22,	    "TPREL22",	   0, false, false),
    IA64_HOWTO (R_IA64_TPREL64MSB,  "TPREL64MSB",  8, false, false),
    IA64_HOWTO (R_IA64_TPREL64LSB,  "TPREL64LSB",  8, false, false),
    IA64_HOWTO (R_IA64_LTOFF_TP22,  "LTOFF_TP22",  0, false, false),
  };

static unsigned char elf_code_to_howto_index[R_IA64_MAX_RELOC_CODE + 1];

/* Given a BFD reloc type, return the matching HOWTO structure.  */

static reloc_howto_type*
lookup_howto (rtype)
     unsigned int rtype;
{
  static int inited = 0;
  int i;

  if (!inited)
    {
      inited = 1;

      memset (elf_code_to_howto_index, 0xff, sizeof (elf_code_to_howto_index));
      for (i = 0; i < NELEMS (ia64_howto_table); ++i)
	elf_code_to_howto_index[ia64_howto_table[i].type] = i;
    }

  BFD_ASSERT (rtype <= R_IA64_MAX_RELOC_CODE);
  i = elf_code_to_howto_index[rtype];
  if (i >= NELEMS (ia64_howto_table))
    return 0;
  return ia64_howto_table + i;
}

static reloc_howto_type*
elfNN_ia64_reloc_type_lookup (abfd, bfd_code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type bfd_code;
{
  unsigned int rtype;

  switch (bfd_code)
    {
    case BFD_RELOC_NONE:		rtype = R_IA64_NONE; break;

    case BFD_RELOC_IA64_IMM14:		rtype = R_IA64_IMM14; break;
    case BFD_RELOC_IA64_IMM22:		rtype = R_IA64_IMM22; break;
    case BFD_RELOC_IA64_IMM64:		rtype = R_IA64_IMM64; break;

    case BFD_RELOC_IA64_DIR32MSB:	rtype = R_IA64_DIR32MSB; break;
    case BFD_RELOC_IA64_DIR32LSB:	rtype = R_IA64_DIR32LSB; break;
    case BFD_RELOC_IA64_DIR64MSB:	rtype = R_IA64_DIR64MSB; break;
    case BFD_RELOC_IA64_DIR64LSB:	rtype = R_IA64_DIR64LSB; break;

    case BFD_RELOC_IA64_GPREL22:	rtype = R_IA64_GPREL22; break;
    case BFD_RELOC_IA64_GPREL64I:	rtype = R_IA64_GPREL64I; break;
    case BFD_RELOC_IA64_GPREL32MSB:	rtype = R_IA64_GPREL32MSB; break;
    case BFD_RELOC_IA64_GPREL32LSB:	rtype = R_IA64_GPREL32LSB; break;
    case BFD_RELOC_IA64_GPREL64MSB:	rtype = R_IA64_GPREL64MSB; break;
    case BFD_RELOC_IA64_GPREL64LSB:	rtype = R_IA64_GPREL64LSB; break;

    case BFD_RELOC_IA64_LTOFF22:	rtype = R_IA64_LTOFF22; break;
    case BFD_RELOC_IA64_LTOFF64I:	rtype = R_IA64_LTOFF64I; break;

    case BFD_RELOC_IA64_PLTOFF22:	rtype = R_IA64_PLTOFF22; break;
    case BFD_RELOC_IA64_PLTOFF64I:	rtype = R_IA64_PLTOFF64I; break;
    case BFD_RELOC_IA64_PLTOFF64MSB:	rtype = R_IA64_PLTOFF64MSB; break;
    case BFD_RELOC_IA64_PLTOFF64LSB:	rtype = R_IA64_PLTOFF64LSB; break;
    case BFD_RELOC_IA64_FPTR64I:	rtype = R_IA64_FPTR64I; break;
    case BFD_RELOC_IA64_FPTR32MSB:	rtype = R_IA64_FPTR32MSB; break;
    case BFD_RELOC_IA64_FPTR32LSB:	rtype = R_IA64_FPTR32LSB; break;
    case BFD_RELOC_IA64_FPTR64MSB:	rtype = R_IA64_FPTR64MSB; break;
    case BFD_RELOC_IA64_FPTR64LSB:	rtype = R_IA64_FPTR64LSB; break;

    case BFD_RELOC_IA64_PCREL21B:	rtype = R_IA64_PCREL21B; break;
    case BFD_RELOC_IA64_PCREL21BI:	rtype = R_IA64_PCREL21BI; break;
    case BFD_RELOC_IA64_PCREL21M:	rtype = R_IA64_PCREL21M; break;
    case BFD_RELOC_IA64_PCREL21F:	rtype = R_IA64_PCREL21F; break;
    case BFD_RELOC_IA64_PCREL22:	rtype = R_IA64_PCREL22; break;
    case BFD_RELOC_IA64_PCREL60B:	rtype = R_IA64_PCREL60B; break;
    case BFD_RELOC_IA64_PCREL64I:	rtype = R_IA64_PCREL64I; break;
    case BFD_RELOC_IA64_PCREL32MSB:	rtype = R_IA64_PCREL32MSB; break;
    case BFD_RELOC_IA64_PCREL32LSB:	rtype = R_IA64_PCREL32LSB; break;
    case BFD_RELOC_IA64_PCREL64MSB:	rtype = R_IA64_PCREL64MSB; break;
    case BFD_RELOC_IA64_PCREL64LSB:	rtype = R_IA64_PCREL64LSB; break;

    case BFD_RELOC_IA64_LTOFF_FPTR22:	rtype = R_IA64_LTOFF_FPTR22; break;
    case BFD_RELOC_IA64_LTOFF_FPTR64I:	rtype = R_IA64_LTOFF_FPTR64I; break;
    case BFD_RELOC_IA64_LTOFF_FPTR64MSB: rtype = R_IA64_LTOFF_FPTR64MSB; break;
    case BFD_RELOC_IA64_LTOFF_FPTR64LSB: rtype = R_IA64_LTOFF_FPTR64LSB; break;

    case BFD_RELOC_IA64_SEGREL32MSB:	rtype = R_IA64_SEGREL32MSB; break;
    case BFD_RELOC_IA64_SEGREL32LSB:	rtype = R_IA64_SEGREL32LSB; break;
    case BFD_RELOC_IA64_SEGREL64MSB:	rtype = R_IA64_SEGREL64MSB; break;
    case BFD_RELOC_IA64_SEGREL64LSB:	rtype = R_IA64_SEGREL64LSB; break;

    case BFD_RELOC_IA64_SECREL32MSB:	rtype = R_IA64_SECREL32MSB; break;
    case BFD_RELOC_IA64_SECREL32LSB:	rtype = R_IA64_SECREL32LSB; break;
    case BFD_RELOC_IA64_SECREL64MSB:	rtype = R_IA64_SECREL64MSB; break;
    case BFD_RELOC_IA64_SECREL64LSB:	rtype = R_IA64_SECREL64LSB; break;

    case BFD_RELOC_IA64_REL32MSB:	rtype = R_IA64_REL32MSB; break;
    case BFD_RELOC_IA64_REL32LSB:	rtype = R_IA64_REL32LSB; break;
    case BFD_RELOC_IA64_REL64MSB:	rtype = R_IA64_REL64MSB; break;
    case BFD_RELOC_IA64_REL64LSB:	rtype = R_IA64_REL64LSB; break;

    case BFD_RELOC_IA64_LTV32MSB:	rtype = R_IA64_LTV32MSB; break;
    case BFD_RELOC_IA64_LTV32LSB:	rtype = R_IA64_LTV32LSB; break;
    case BFD_RELOC_IA64_LTV64MSB:	rtype = R_IA64_LTV64MSB; break;
    case BFD_RELOC_IA64_LTV64LSB:	rtype = R_IA64_LTV64LSB; break;

    case BFD_RELOC_IA64_IPLTMSB:	rtype = R_IA64_IPLTMSB; break;
    case BFD_RELOC_IA64_IPLTLSB:	rtype = R_IA64_IPLTLSB; break;
    case BFD_RELOC_IA64_COPY:		rtype = R_IA64_COPY; break;
    case BFD_RELOC_IA64_LTOFF22X:	rtype = R_IA64_LTOFF22X; break;
    case BFD_RELOC_IA64_LDXMOV:		rtype = R_IA64_LDXMOV; break;

    case BFD_RELOC_IA64_TPREL22:	rtype = R_IA64_TPREL22; break;
    case BFD_RELOC_IA64_TPREL64MSB:	rtype = R_IA64_TPREL64MSB; break;
    case BFD_RELOC_IA64_TPREL64LSB:	rtype = R_IA64_TPREL64LSB; break;
    case BFD_RELOC_IA64_LTOFF_TP22:	rtype = R_IA64_LTOFF_TP22; break;

    default: return 0;
    }
  return lookup_howto (rtype);
}

/* Given a ELF reloc, return the matching HOWTO structure.  */

static void
elfNN_ia64_info_to_howto (abfd, bfd_reloc, elf_reloc)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *bfd_reloc;
     ElfNN_Internal_Rela *elf_reloc;
{
  bfd_reloc->howto = lookup_howto (ELFNN_R_TYPE (elf_reloc->r_info));
}

#define PLT_HEADER_SIZE		(3 * 16)
#define PLT_MIN_ENTRY_SIZE	(1 * 16)
#define PLT_FULL_ENTRY_SIZE	(2 * 16)
#define PLT_RESERVED_WORDS	3

static const bfd_byte plt_header[PLT_HEADER_SIZE] =
{
  0x0b, 0x10, 0x00, 0x1c, 0x00, 0x21,  /*   [MMI]       mov r2=r14;;       */
  0xe0, 0x00, 0x08, 0x00, 0x48, 0x00,  /*               addl r14=0,r2      */
  0x00, 0x00, 0x04, 0x00,              /*               nop.i 0x0;;        */
  0x0b, 0x80, 0x20, 0x1c, 0x18, 0x14,  /*   [MMI]       ld8 r16=[r14],8;;  */
  0x10, 0x41, 0x38, 0x30, 0x28, 0x00,  /*               ld8 r17=[r14],8    */
  0x00, 0x00, 0x04, 0x00,              /*               nop.i 0x0;;        */
  0x11, 0x08, 0x00, 0x1c, 0x18, 0x10,  /*   [MIB]       ld8 r1=[r14]       */
  0x60, 0x88, 0x04, 0x80, 0x03, 0x00,  /*               mov b6=r17         */
  0x60, 0x00, 0x80, 0x00               /*               br.few b6;;        */
};

static const bfd_byte plt_min_entry[PLT_MIN_ENTRY_SIZE] =
{
  0x11, 0x78, 0x00, 0x00, 0x00, 0x24,  /*   [MIB]       mov r15=0          */
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00,  /*               nop.i 0x0          */
  0x00, 0x00, 0x00, 0x40               /*               br.few 0 <PLT0>;;  */
};

static const bfd_byte plt_full_entry[PLT_FULL_ENTRY_SIZE] =
{
  0x0b, 0x78, 0x00, 0x02, 0x00, 0x24,  /*   [MMI]       addl r15=0,r1;;    */
  0x00, 0x41, 0x3c, 0x30, 0x28, 0xc0,  /*               ld8 r16=[r15],8    */
  0x01, 0x08, 0x00, 0x84,              /*               mov r14=r1;;       */
  0x11, 0x08, 0x00, 0x1e, 0x18, 0x10,  /*   [MIB]       ld8 r1=[r15]       */
  0x60, 0x80, 0x04, 0x80, 0x03, 0x00,  /*               mov b6=r16         */
  0x60, 0x00, 0x80, 0x00               /*               br.few b6;;        */
};

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/ld.so.1"

/* Select out of range branch fixup type.  Note that Itanium does
   not support brl, and so it gets emulated by the kernel.  */
#undef USE_BRL

static const bfd_byte oor_brl[16] =
{
  0x05, 0x00, 0x00, 0x00, 0x01, 0x00,  /*  [MLX]        nop.m 0            */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /*               brl.sptk.few tgt;; */
  0x00, 0x00, 0x00, 0xc0
};

static const bfd_byte oor_ip[48] =
{
  0x04, 0x00, 0x00, 0x00, 0x01, 0x00,  /*  [MLX]        nop.m 0            */
  0x00, 0x00, 0x00, 0x00, 0x00, 0xe0,  /*               movl r15=0         */
  0x01, 0x00, 0x00, 0x60,
  0x03, 0x00, 0x00, 0x00, 0x01, 0x00,  /*  [MII]        nop.m 0            */
  0x00, 0x01, 0x00, 0x60, 0x00, 0x00,  /*               mov r16=ip;;       */
  0xf2, 0x80, 0x00, 0x80,              /*               add r16=r15,r16;;  */
  0x11, 0x00, 0x00, 0x00, 0x01, 0x00,  /*  [MIB]        nop.m 0            */
  0x60, 0x80, 0x04, 0x80, 0x03, 0x00,  /*               mov b6=r16         */
  0x60, 0x00, 0x80, 0x00               /*               br b6;;            */
};

/* These functions do relaxation for IA-64 ELF.

   This is primarily to support branches to targets out of range;
   relaxation of R_IA64_LTOFF22X and R_IA64_LDXMOV not yet supported.  */

static boolean
elfNN_ia64_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     boolean *again;
{
  struct one_fixup
    {
      struct one_fixup *next;
      asection *tsec;
      bfd_vma toff;
      bfd_vma trampoff;
    };

  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *free_relocs = NULL;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents;
  bfd_byte *free_contents = NULL;
  ElfNN_External_Sym *extsyms;
  ElfNN_External_Sym *free_extsyms = NULL;
  struct elfNN_ia64_link_hash_table *ia64_info;
  struct one_fixup *fixups = NULL;
  boolean changed_contents = false;
  boolean changed_relocs = false;

  /* Assume we're not going to change any sizes, and we'll only need
     one pass.  */
  *again = false;

  /* Nothing to do if there are no relocations.  */
  if ((sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0)
    return true;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (sec->_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Load the relocations for this section.  */
  internal_relocs = (_bfd_elfNN_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  if (! link_info->keep_memory)
    free_relocs = internal_relocs;

  ia64_info = elfNN_ia64_hash_table (link_info);
  irelend = internal_relocs + sec->reloc_count;

  for (irel = internal_relocs; irel < irelend; irel++)
    if (ELFNN_R_TYPE (irel->r_info) == (int) R_IA64_PCREL21B)
      break;

  /* No branch-type relocations.  */
  if (irel == irelend)
    {
      if (free_relocs != NULL)
	free (free_relocs);
      return true;
    }

  /* Get the section contents.  */
  if (elf_section_data (sec)->this_hdr.contents != NULL)
    contents = elf_section_data (sec)->this_hdr.contents;
  else
    {
      contents = (bfd_byte *) bfd_malloc (sec->_raw_size);
      if (contents == NULL)
	goto error_return;
      free_contents = contents;

      if (! bfd_get_section_contents (abfd, sec, contents,
				      (file_ptr) 0, sec->_raw_size))
	goto error_return;
    }

  /* Read this BFD's symbols.  */
  if (symtab_hdr->contents != NULL)
    extsyms = (ElfNN_External_Sym *) symtab_hdr->contents;
  else
    {
      extsyms = (ElfNN_External_Sym *) bfd_malloc (symtab_hdr->sh_size);
      if (extsyms == NULL)
	goto error_return;
      free_extsyms = extsyms;
      if (bfd_seek (abfd, symtab_hdr->sh_offset, SEEK_SET) != 0
	  || (bfd_read (extsyms, 1, symtab_hdr->sh_size, abfd)
	      != symtab_hdr->sh_size))
	goto error_return;
    }

  for (; irel < irelend; irel++)
    {
      bfd_vma symaddr, reladdr, trampoff, toff, roff;
      Elf_Internal_Sym isym;
      asection *tsec;
      struct one_fixup *f;

      if (ELFNN_R_TYPE (irel->r_info) != (int) R_IA64_PCREL21B)
	continue;

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELFNN_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  bfd_elfNN_swap_symbol_in (abfd,
				    extsyms + ELFNN_R_SYM (irel->r_info),
				    &isym);
	  if (isym.st_shndx == SHN_UNDEF)
	    continue;	/* We can't do anthing with undefined symbols.  */
	  else if (isym.st_shndx == SHN_ABS)
	    tsec = bfd_abs_section_ptr;
	  else if (isym.st_shndx == SHN_COMMON)
	    tsec = bfd_com_section_ptr;
	  else if (isym.st_shndx > 0 && isym.st_shndx < SHN_LORESERVE)
	    tsec = bfd_section_from_elf_index (abfd, isym.st_shndx);
	  else
	    continue;	/* who knows.  */

	  toff = isym.st_value;
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;
          struct elfNN_ia64_dyn_sym_info *dyn_i;

	  indx = ELFNN_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  dyn_i = get_dyn_sym_info (ia64_info, h, abfd, irel, false);

	  /* For branches to dynamic symbols, we're interested instead
	     in a branch to the PLT entry.  */
	  if (dyn_i && dyn_i->want_plt2)
	    {
	      tsec = ia64_info->plt_sec;
	      toff = dyn_i->plt2_offset;
	    }
	  else
	    {
	      /* We can't do anthing with undefined symbols.  */
	      if (h->root.type == bfd_link_hash_undefined
		  || h->root.type == bfd_link_hash_undefweak)
		continue;

	      tsec = h->root.u.def.section;
	      toff = h->root.u.def.value;
	    }
	}

      symaddr = (tsec->output_section->vma
		 + tsec->output_offset
		 + toff
		 + irel->r_addend);

      roff = irel->r_offset;
      reladdr = (sec->output_section->vma
		 + sec->output_offset
		 + roff) & -4;

      /* If the branch is in range, no need to do anything.  */
      if ((bfd_signed_vma) (symaddr - reladdr) >= -0x1000000
	  && (bfd_signed_vma) (symaddr - reladdr) <= 0x0FFFFF0)
	continue;

      /* If the branch and target are in the same section, you've
	 got one honking big section and we can't help you.  You'll
	 get an error message later.  */
      if (tsec == sec)
	continue;

      /* Look for an existing fixup to this address.  */
      for (f = fixups; f ; f = f->next)
	if (f->tsec == tsec && f->toff == toff)
	  break;

      if (f == NULL)
	{
	  /* Two alternatives: If it's a branch to a PLT entry, we can
	     make a copy of the FULL_PLT entry.  Otherwise, we'll have
	     to use a `brl' insn to get where we're going.  */

	  int size;

	  if (tsec == ia64_info->plt_sec)
	    size = sizeof (plt_full_entry);
	  else
	    {
#ifdef USE_BRL
	      size = sizeof (oor_brl);
#else
	      size = sizeof (oor_ip);
#endif
	    }

	  /* Resize the current section to make room for the new branch.  */
	  trampoff = (sec->_cooked_size + 15) & -16;
	  contents = (bfd_byte *) bfd_realloc (contents, trampoff + size);
	  if (contents == NULL)
	    goto error_return;
	  sec->_cooked_size = trampoff + size;

	  if (tsec == ia64_info->plt_sec)
	    {
	      memcpy (contents + trampoff, plt_full_entry, size);

	      /* Hijack the old relocation for use as the PLTOFF reloc.  */
	      irel->r_info = ELFNN_R_INFO (ELFNN_R_SYM (irel->r_info),
					   R_IA64_PLTOFF22);
	      irel->r_offset = trampoff;
	    }
	  else
	    {
#ifdef USE_BRL
	      memcpy (contents + trampoff, oor_brl, size);
	      irel->r_info = ELFNN_R_INFO (ELFNN_R_SYM (irel->r_info),
					   R_IA64_PCREL60B);
	      irel->r_offset = trampoff + 2;
#else
	      memcpy (contents + trampoff, oor_ip, size);
	      irel->r_info = ELFNN_R_INFO (ELFNN_R_SYM (irel->r_info),
					   R_IA64_PCREL64I);
	      irel->r_addend -= 16;
	      irel->r_offset = trampoff + 2;
#endif
	    }

	  /* Record the fixup so we don't do it again this section.  */
	  f = (struct one_fixup *) bfd_malloc (sizeof (*f));
	  f->next = fixups;
	  f->tsec = tsec;
	  f->toff = toff;
	  f->trampoff = trampoff;
	  fixups = f;
	}
      else
	{
	  /* Nop out the reloc, since we're finalizing things here.  */
	  irel->r_info = ELFNN_R_INFO (0, R_IA64_NONE);
	}

      /* Fix up the existing branch to hit the trampoline.  Hope like
	 hell this doesn't overflow too.  */
      if (elfNN_ia64_install_value (abfd, contents + roff,
				    f->trampoff - (roff & -4),
				    R_IA64_PCREL21B) != bfd_reloc_ok)
	goto error_return;

      changed_contents = true;
      changed_relocs = true;
    }

  /* Clean up and go home.  */
  while (fixups)
    {
      struct one_fixup *f = fixups;
      fixups = fixups->next;
      free (f);
    }

  if (changed_relocs)
    elf_section_data (sec)->relocs = internal_relocs;
  else if (free_relocs != NULL)
    free (free_relocs);

  if (changed_contents)
    elf_section_data (sec)->this_hdr.contents = contents;
  else if (free_contents != NULL)
    {
      if (! link_info->keep_memory)
	free (free_contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
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

  *again = changed_contents || changed_relocs;
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

/* Return true if NAME is an unwind table section name.  */

static inline boolean
is_unwind_section_name (name)
	const char *name;
{
  size_t len1, len2, len3;

  len1 = sizeof (ELF_STRING_ia64_unwind) - 1;
  len2 = sizeof (ELF_STRING_ia64_unwind_info) - 1;
  len3 = sizeof (ELF_STRING_ia64_unwind_once) - 1;
  return ((strncmp (name, ELF_STRING_ia64_unwind, len1) == 0
	   && strncmp (name, ELF_STRING_ia64_unwind_info, len2) != 0)
	  || strncmp (name, ELF_STRING_ia64_unwind_once, len3) == 0);
}

/* Handle an IA-64 specific section when reading an object file.  This
   is called when elfcode.h finds a section with an unknown type.  */

static boolean
elfNN_ia64_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     ElfNN_Internal_Shdr *hdr;
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
    case SHT_IA_64_UNWIND:
      break;

    case SHT_IA_64_EXT:
      if (strcmp (name, ELF_STRING_ia64_archext) != 0)
	return false;
      break;

    default:
      return false;
    }

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
    return false;
  newsect = hdr->bfd_section;

  return true;
}

/* Convert IA-64 specific section flags to bfd internal section flags.  */

/* ??? There is no bfd internal flag equivalent to the SHF_IA_64_NORECOV
   flag.  */

static boolean
elfNN_ia64_section_flags (flags, hdr)
     flagword *flags;
     ElfNN_Internal_Shdr *hdr;
{
  if (hdr->sh_flags & SHF_IA_64_SHORT)
    *flags |= SEC_SMALL_DATA;

  return true;
}

/* Set the correct type for an IA-64 ELF section.  We do this by the
   section name, which is a hack, but ought to work.  */

static boolean
elfNN_ia64_fake_sections (abfd, hdr, sec)
     bfd *abfd ATTRIBUTE_UNUSED;
     ElfNN_Internal_Shdr *hdr;
     asection *sec;
{
  register const char *name;

  name = bfd_get_section_name (abfd, sec);

  if (is_unwind_section_name (name))
    {
      /* We don't have the sections numbered at this point, so sh_info
	 is set later, in elfNN_ia64_final_write_processing.  */
      hdr->sh_type = SHT_IA_64_UNWIND;
      hdr->sh_flags |= SHF_LINK_ORDER;
    }
  else if (strcmp (name, ELF_STRING_ia64_archext) == 0)
    hdr->sh_type = SHT_IA_64_EXT;
  else if (strcmp (name, ".reloc") == 0)
    /*
     * This is an ugly, but unfortunately necessary hack that is
     * needed when producing EFI binaries on IA-64. It tells
     * elf.c:elf_fake_sections() not to consider ".reloc" as a section
     * containing ELF relocation info.  We need this hack in order to
     * be able to generate ELF binaries that can be translated into
     * EFI applications (which are essentially COFF objects).  Those
     * files contain a COFF ".reloc" section inside an ELFNN object,
     * which would normally cause BFD to segfault because it would
     * attempt to interpret this section as containing relocation
     * entries for section "oc".  With this hack enabled, ".reloc"
     * will be treated as a normal data section, which will avoid the
     * segfault.  However, you won't be able to create an ELFNN binary
     * with a section named "oc" that needs relocations, but that's
     * the kind of ugly side-effects you get when detecting section
     * types based on their names...  In practice, this limitation is
     * unlikely to bite.
     */
    hdr->sh_type = SHT_PROGBITS;

  if (sec->flags & SEC_SMALL_DATA)
    hdr->sh_flags |= SHF_IA_64_SHORT;

  return true;
}

/* The final processing done just before writing out an IA-64 ELF
   object file.  */

static void
elfNN_ia64_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker ATTRIBUTE_UNUSED;
{
  Elf_Internal_Shdr *hdr;
  const char *sname;
  asection *text_sect, *s;
  size_t len;

  for (s = abfd->sections; s; s = s->next)
    {
      hdr = &elf_section_data (s)->this_hdr;
      switch (hdr->sh_type)
	{
	case SHT_IA_64_UNWIND:
	  /* See comments in gas/config/tc-ia64.c:dot_endp on why we
	     have to do this.  */
	  sname = bfd_get_section_name (abfd, s);
	  len = sizeof (ELF_STRING_ia64_unwind) - 1;
	  if (sname && strncmp (sname, ELF_STRING_ia64_unwind, len) == 0)
	    {
	      sname += len;

	      if (sname[0] == '\0')
		/* .IA_64.unwind -> .text */
		text_sect = bfd_get_section_by_name (abfd, ".text");
	      else
		/* .IA_64.unwindFOO -> FOO */
		text_sect = bfd_get_section_by_name (abfd, sname);
	    }
	  else if (sname
		   && (len = sizeof (ELF_STRING_ia64_unwind_once) - 1,
		       strncmp (sname, ELF_STRING_ia64_unwind_once, len)) == 0)
	    {
	      /* .gnu.linkonce.ia64unw.FOO -> .gnu.linkonce.t.FOO */
	      size_t len2 = sizeof (".gnu.linkonce.t.") - 1;
	      char *once_name = alloca (len2 + strlen (sname) - len + 1);

	      memcpy (once_name, ".gnu.linkonce.t.", len2);
	      strcpy (once_name + len2, sname + len);
	      text_sect = bfd_get_section_by_name (abfd, once_name);
	    }
	  else
	    /* last resort: fall back on .text */
	    text_sect = bfd_get_section_by_name (abfd, ".text");

	  if (text_sect)
	    {
	      /* The IA-64 processor-specific ABI requires setting
		 sh_link to the unwind section, whereas HP-UX requires
		 sh_info to do so.  For maximum compatibility, we'll
		 set both for now... */
	      hdr->sh_link = elf_section_data (text_sect)->this_idx;
	      hdr->sh_info = elf_section_data (text_sect)->this_idx;
	    }
	  break;
	}
    }
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We use it to put .comm items in .sbss, and not .bss.  */

static boolean
elfNN_ia64_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
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
      && sym->st_size <= (unsigned) bfd_get_gp_size (abfd))
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

/* Return the number of additional phdrs we will need.  */

static int
elfNN_ia64_additional_program_headers (abfd)
     bfd *abfd;
{
  asection *s;
  int ret = 0;

  /* See if we need a PT_IA_64_ARCHEXT segment.  */
  s = bfd_get_section_by_name (abfd, ELF_STRING_ia64_archext);
  if (s && (s->flags & SEC_LOAD))
    ++ret;

  /* Count how many PT_IA_64_UNWIND segments we need.  */
  for (s = abfd->sections; s; s = s->next)
    if (is_unwind_section_name(s->name) && (s->flags & SEC_LOAD))
      ++ret;

  return ret;
}

static boolean
elfNN_ia64_modify_segment_map (abfd)
     bfd *abfd;
{
  struct elf_segment_map *m, **pm;
  Elf_Internal_Shdr *hdr;
  asection *s;

  /* If we need a PT_IA_64_ARCHEXT segment, it must come before
     all PT_LOAD segments.  */
  s = bfd_get_section_by_name (abfd, ELF_STRING_ia64_archext);
  if (s && (s->flags & SEC_LOAD))
    {
      for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
	if (m->p_type == PT_IA_64_ARCHEXT)
	  break;
      if (m == NULL)
	{
	  m = (struct elf_segment_map *) bfd_zalloc (abfd, sizeof *m);
	  if (m == NULL)
	    return false;

	  m->p_type = PT_IA_64_ARCHEXT;
	  m->count = 1;
	  m->sections[0] = s;

	  /* We want to put it after the PHDR and INTERP segments.  */
	  pm = &elf_tdata (abfd)->segment_map;
	  while (*pm != NULL
		 && ((*pm)->p_type == PT_PHDR
		     || (*pm)->p_type == PT_INTERP))
	    pm = &(*pm)->next;

	  m->next = *pm;
	  *pm = m;
	}
    }

  /* Install PT_IA_64_UNWIND segments, if needed.  */
  for (s = abfd->sections; s; s = s->next)
    {
      hdr = &elf_section_data (s)->this_hdr;
      if (hdr->sh_type != SHT_IA_64_UNWIND)
	continue;

      if (s && (s->flags & SEC_LOAD))
	{
	  for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
	    if (m->p_type == PT_IA_64_UNWIND && m->sections[0] == s)
	      break;

	  if (m == NULL)
	    {
	      m = (struct elf_segment_map *) bfd_zalloc (abfd, sizeof *m);
	      if (m == NULL)
		return false;

	      m->p_type = PT_IA_64_UNWIND;
	      m->count = 1;
	      m->sections[0] = s;
	      m->next = NULL;

	      /* We want to put it last.  */
	      pm = &elf_tdata (abfd)->segment_map;
	      while (*pm != NULL)
		pm = &(*pm)->next;
	      *pm = m;
	    }
	}
    }

  /* Turn on PF_IA_64_NORECOV if needed.  This involves traversing all of
     the input sections for each output section in the segment and testing
     for SHF_IA_64_NORECOV on each.  */
  for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
    if (m->p_type == PT_LOAD)
      {
	int i;
	for (i = m->count - 1; i >= 0; --i)
	  {
	    struct bfd_link_order *order = m->sections[i]->link_order_head;
	    while (order)
	      {
		if (order->type == bfd_indirect_link_order)
		  {
		    asection *is = order->u.indirect.section;
		    bfd_vma flags = elf_section_data(is)->this_hdr.sh_flags;
		    if (flags & SHF_IA_64_NORECOV)
		      {
			m->p_flags |= PF_IA_64_NORECOV;
			goto found;
		      }
		  }
		order = order->next;
	      }
	  }
      found:;
      }

  return true;
}

/* According to the Tahoe assembler spec, all labels starting with a
   '.' are local.  */

static boolean
elfNN_ia64_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
{
  return name[0] == '.';
}

/* Should we do dynamic things to this symbol?  */

static boolean
elfNN_ia64_dynamic_symbol_p (h, info)
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
  switch (ELF_ST_VISIBILITY (h->other))
    {
    case STV_INTERNAL:
    case STV_HIDDEN:
      return false;
    }

  if (h->root.type == bfd_link_hash_undefweak
      || h->root.type == bfd_link_hash_defweak)
    return true;

  if ((info->shared && !info->symbolic)
      || ((h->elf_link_hash_flags
	   & (ELF_LINK_HASH_DEF_DYNAMIC | ELF_LINK_HASH_REF_REGULAR))
	  == (ELF_LINK_HASH_DEF_DYNAMIC | ELF_LINK_HASH_REF_REGULAR)))
    return true;

  return false;
}

static boolean
elfNN_ia64_local_hash_table_init (ht, abfd, new)
     struct elfNN_ia64_local_hash_table *ht;
     bfd *abfd ATTRIBUTE_UNUSED;
     new_hash_entry_func new;
{
  memset (ht, 0, sizeof (*ht));
  return bfd_hash_table_init (&ht->root, new);
}

static struct bfd_hash_entry*
elfNN_ia64_new_loc_hash_entry (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elfNN_ia64_local_hash_entry *ret;
  ret = (struct elfNN_ia64_local_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (!ret)
    ret = bfd_hash_allocate (table, sizeof (*ret));

  if (!ret)
    return 0;

  /* Initialize our local data.  All zeros, and definitely easier
     than setting a handful of bit fields.  */
  memset (ret, 0, sizeof (*ret));

  /* Call the allocation method of the superclass.  */
  ret = ((struct elfNN_ia64_local_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));

  return (struct bfd_hash_entry *) ret;
}

static struct bfd_hash_entry*
elfNN_ia64_new_elf_hash_entry (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elfNN_ia64_link_hash_entry *ret;
  ret = (struct elfNN_ia64_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (!ret)
    ret = bfd_hash_allocate (table, sizeof (*ret));

  if (!ret)
    return 0;

  /* Initialize our local data.  All zeros, and definitely easier
     than setting a handful of bit fields.  */
  memset (ret, 0, sizeof (*ret));

  /* Call the allocation method of the superclass.  */
  ret = ((struct elfNN_ia64_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));

  return (struct bfd_hash_entry *) ret;
}

static void
elfNN_ia64_hash_copy_indirect (xdir, xind)
     struct elf_link_hash_entry *xdir, *xind;
{
  struct elfNN_ia64_link_hash_entry *dir, *ind;

  dir = (struct elfNN_ia64_link_hash_entry *)xdir;
  ind = (struct elfNN_ia64_link_hash_entry *)xind;

  /* Copy down any references that we may have already seen to the
     symbol which just became indirect.  */

  dir->root.elf_link_hash_flags |=
    (ind->root.elf_link_hash_flags
     & (ELF_LINK_HASH_REF_DYNAMIC
        | ELF_LINK_HASH_REF_REGULAR
        | ELF_LINK_HASH_REF_REGULAR_NONWEAK));

  /* Copy over the got and plt data.  This would have been done
     by check_relocs.  */

  if (dir->info == NULL)
    {
      struct elfNN_ia64_dyn_sym_info *dyn_i;

      dir->info = dyn_i = ind->info;
      ind->info = NULL;

      /* Fix up the dyn_sym_info pointers to the global symbol.  */
      for (; dyn_i; dyn_i = dyn_i->next)
	dyn_i->h = &dir->root;
    }
  BFD_ASSERT (ind->info == NULL);

  /* Copy over the dynindx.  */

  if (dir->root.dynindx == -1)
    {
      dir->root.dynindx = ind->root.dynindx;
      dir->root.dynstr_index = ind->root.dynstr_index;
      ind->root.dynindx = -1;
      ind->root.dynstr_index = 0;
    }
  BFD_ASSERT (ind->root.dynindx == -1);
}

static void
elfNN_ia64_hash_hide_symbol (info, xh)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elf_link_hash_entry *xh;
{
  struct elfNN_ia64_link_hash_entry *h;
  struct elfNN_ia64_dyn_sym_info *dyn_i;

  h = (struct elfNN_ia64_link_hash_entry *)xh;

  h->root.elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
  if ((h->root.elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
    h->root.dynindx = -1;

  for (dyn_i = h->info; dyn_i; dyn_i = dyn_i->next)
    dyn_i->want_plt2 = 0;
}

/* Create the derived linker hash table.  The IA-64 ELF port uses this
   derived hash table to keep information specific to the IA-64 ElF
   linker (without using static variables).  */

static struct bfd_link_hash_table*
elfNN_ia64_hash_table_create (abfd)
     bfd *abfd;
{
  struct elfNN_ia64_link_hash_table *ret;

  ret = bfd_alloc (abfd, sizeof (*ret));
  if (!ret)
    return 0;
  if (!_bfd_elf_link_hash_table_init (&ret->root, abfd,
				      elfNN_ia64_new_elf_hash_entry))
    {
      bfd_release (abfd, ret);
      return 0;
    }

  if (!elfNN_ia64_local_hash_table_init (&ret->loc_hash_table, abfd,
				         elfNN_ia64_new_loc_hash_entry))
    return 0;
  return &ret->root.root;
}

/* Look up an entry in a Alpha ELF linker hash table.  */

static INLINE struct elfNN_ia64_local_hash_entry *
elfNN_ia64_local_hash_lookup(table, string, create, copy)
     struct elfNN_ia64_local_hash_table *table;
     const char *string;
     boolean create, copy;
{
  return ((struct elfNN_ia64_local_hash_entry *)
	  bfd_hash_lookup (&table->root, string, create, copy));
}

/* Traverse both local and global hash tables.  */

struct elfNN_ia64_dyn_sym_traverse_data
{
  boolean (*func) PARAMS ((struct elfNN_ia64_dyn_sym_info *, PTR));
  PTR data;
};

static boolean
elfNN_ia64_global_dyn_sym_thunk (xentry, xdata)
     struct bfd_hash_entry *xentry;
     PTR xdata;
{
  struct elfNN_ia64_link_hash_entry *entry
    = (struct elfNN_ia64_link_hash_entry *) xentry;
  struct elfNN_ia64_dyn_sym_traverse_data *data
    = (struct elfNN_ia64_dyn_sym_traverse_data *) xdata;
  struct elfNN_ia64_dyn_sym_info *dyn_i;

  for (dyn_i = entry->info; dyn_i; dyn_i = dyn_i->next)
    if (! (*data->func) (dyn_i, data->data))
      return false;
  return true;
}

static boolean
elfNN_ia64_local_dyn_sym_thunk (xentry, xdata)
     struct bfd_hash_entry *xentry;
     PTR xdata;
{
  struct elfNN_ia64_local_hash_entry *entry
    = (struct elfNN_ia64_local_hash_entry *) xentry;
  struct elfNN_ia64_dyn_sym_traverse_data *data
    = (struct elfNN_ia64_dyn_sym_traverse_data *) xdata;
  struct elfNN_ia64_dyn_sym_info *dyn_i;

  for (dyn_i = entry->info; dyn_i; dyn_i = dyn_i->next)
    if (! (*data->func) (dyn_i, data->data))
      return false;
  return true;
}

static void
elfNN_ia64_dyn_sym_traverse (ia64_info, func, data)
     struct elfNN_ia64_link_hash_table *ia64_info;
     boolean (*func) PARAMS ((struct elfNN_ia64_dyn_sym_info *, PTR));
     PTR data;
{
  struct elfNN_ia64_dyn_sym_traverse_data xdata;

  xdata.func = func;
  xdata.data = data;

  elf_link_hash_traverse (&ia64_info->root,
			  elfNN_ia64_global_dyn_sym_thunk, &xdata);
  bfd_hash_traverse (&ia64_info->loc_hash_table.root,
		     elfNN_ia64_local_dyn_sym_thunk, &xdata);
}

static boolean
elfNN_ia64_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  struct elfNN_ia64_link_hash_table *ia64_info;
  asection *s;

  if (! _bfd_elf_create_dynamic_sections (abfd, info))
    return false;

  ia64_info = elfNN_ia64_hash_table (info);

  ia64_info->plt_sec = bfd_get_section_by_name (abfd, ".plt");
  ia64_info->got_sec = bfd_get_section_by_name (abfd, ".got");

  {
    flagword flags = bfd_get_section_flags (abfd, ia64_info->got_sec);
    bfd_set_section_flags (abfd, ia64_info->got_sec, SEC_SMALL_DATA | flags);
  }

  if (!get_pltoff (abfd, info, ia64_info))
    return false;

  s = bfd_make_section(abfd, ".rela.IA_64.pltoff");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					   | SEC_HAS_CONTENTS
					   | SEC_IN_MEMORY
					   | SEC_LINKER_CREATED
					   | SEC_READONLY))
      || !bfd_set_section_alignment (abfd, s, 3))
    return false;
  ia64_info->rel_pltoff_sec = s;

  s = bfd_make_section(abfd, ".rela.got");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, (SEC_ALLOC | SEC_LOAD
					   | SEC_HAS_CONTENTS
					   | SEC_IN_MEMORY
					   | SEC_LINKER_CREATED
					   | SEC_READONLY))
      || !bfd_set_section_alignment (abfd, s, 3))
    return false;
  ia64_info->rel_got_sec = s;

  return true;
}

/* Find and/or create a descriptor for dynamic symbol info.  This will
   vary based on global or local symbol, and the addend to the reloc.  */

static struct elfNN_ia64_dyn_sym_info *
get_dyn_sym_info (ia64_info, h, abfd, rel, create)
     struct elfNN_ia64_link_hash_table *ia64_info;
     struct elf_link_hash_entry *h;
     bfd *abfd;
     const Elf_Internal_Rela *rel;
     boolean create;
{
  struct elfNN_ia64_dyn_sym_info **pp;
  struct elfNN_ia64_dyn_sym_info *dyn_i;
  bfd_vma addend = rel ? rel->r_addend : 0;

  if (h)
    pp = &((struct elfNN_ia64_link_hash_entry *)h)->info;
  else
    {
      struct elfNN_ia64_local_hash_entry *loc_h;
      char *addr_name;
      size_t len;

      /* Construct a string for use in the elfNN_ia64_local_hash_table.
         The name describes what was once anonymous memory.  */

      len = sizeof (void*)*2 + 1 + sizeof (bfd_vma)*4 + 1 + 1;
      len += 10;	/* %p slop */

      addr_name = alloca (len);
      sprintf (addr_name, "%p:%lx", (void *) abfd, ELFNN_R_SYM (rel->r_info));

      /* Collect the canonical entry data for this address.  */
      loc_h = elfNN_ia64_local_hash_lookup (&ia64_info->loc_hash_table,
					    addr_name, create, create);
      BFD_ASSERT (loc_h);

      pp = &loc_h->info;
    }

  for (dyn_i = *pp; dyn_i && dyn_i->addend != addend; dyn_i = *pp)
    pp = &dyn_i->next;

  if (dyn_i == NULL && create)
    {
      dyn_i = (struct elfNN_ia64_dyn_sym_info *)
	bfd_zalloc (abfd, sizeof *dyn_i);
      *pp = dyn_i;
      dyn_i->addend = addend;
    }

  return dyn_i;
}

static asection *
get_got (abfd, info, ia64_info)
     bfd *abfd;
     struct bfd_link_info *info;
     struct elfNN_ia64_link_hash_table *ia64_info;
{
  asection *got;
  bfd *dynobj;

  got = ia64_info->got_sec;
  if (!got)
    {
      flagword flags;

      dynobj = ia64_info->root.dynobj;
      if (!dynobj)
	ia64_info->root.dynobj = dynobj = abfd;
      if (!_bfd_elf_create_got_section (dynobj, info))
	return 0;

      got = bfd_get_section_by_name (dynobj, ".got");
      BFD_ASSERT (got);
      ia64_info->got_sec = got;

      flags = bfd_get_section_flags (abfd, got);
      bfd_set_section_flags (abfd, got, SEC_SMALL_DATA | flags);
    }

  return got;
}

/* Create function descriptor section (.opd).  This section is called .opd
   because it contains "official prodecure descriptors".  The "official"
   refers to the fact that these descriptors are used when taking the address
   of a procedure, thus ensuring a unique address for each procedure.  */

static asection *
get_fptr (abfd, info, ia64_info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elfNN_ia64_link_hash_table *ia64_info;
{
  asection *fptr;
  bfd *dynobj;

  fptr = ia64_info->fptr_sec;
  if (!fptr)
    {
      dynobj = ia64_info->root.dynobj;
      if (!dynobj)
	ia64_info->root.dynobj = dynobj = abfd;

      fptr = bfd_make_section (dynobj, ".opd");
      if (!fptr
	  || !bfd_set_section_flags (dynobj, fptr,
				     (SEC_ALLOC
				      | SEC_LOAD
				      | SEC_HAS_CONTENTS
				      | SEC_IN_MEMORY
				      | SEC_READONLY
				      | SEC_LINKER_CREATED))
	  || !bfd_set_section_alignment (abfd, fptr, 4))
	{
	  BFD_ASSERT (0);
	  return NULL;
	}

      ia64_info->fptr_sec = fptr;
    }

  return fptr;
}

static asection *
get_pltoff (abfd, info, ia64_info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elfNN_ia64_link_hash_table *ia64_info;
{
  asection *pltoff;
  bfd *dynobj;

  pltoff = ia64_info->pltoff_sec;
  if (!pltoff)
    {
      dynobj = ia64_info->root.dynobj;
      if (!dynobj)
	ia64_info->root.dynobj = dynobj = abfd;

      pltoff = bfd_make_section (dynobj, ELF_STRING_ia64_pltoff);
      if (!pltoff
	  || !bfd_set_section_flags (dynobj, pltoff,
				     (SEC_ALLOC
				      | SEC_LOAD
				      | SEC_HAS_CONTENTS
				      | SEC_IN_MEMORY
				      | SEC_SMALL_DATA
				      | SEC_LINKER_CREATED))
	  || !bfd_set_section_alignment (abfd, pltoff, 4))
	{
	  BFD_ASSERT (0);
	  return NULL;
	}

      ia64_info->pltoff_sec = pltoff;
    }

  return pltoff;
}

static asection *
get_reloc_section (abfd, ia64_info, sec, create)
     bfd *abfd;
     struct elfNN_ia64_link_hash_table *ia64_info;
     asection *sec;
     boolean create;
{
  const char *srel_name;
  asection *srel;
  bfd *dynobj;

  srel_name = (bfd_elf_string_from_elf_section
	       (abfd, elf_elfheader(abfd)->e_shstrndx,
		elf_section_data(sec)->rel_hdr.sh_name));
  if (srel_name == NULL)
    return NULL;

  BFD_ASSERT ((strncmp (srel_name, ".rela", 5) == 0
	       && strcmp (bfd_get_section_name (abfd, sec),
			  srel_name+5) == 0)
	      || (strncmp (srel_name, ".rel", 4) == 0
		  && strcmp (bfd_get_section_name (abfd, sec),
			     srel_name+4) == 0));

  dynobj = ia64_info->root.dynobj;
  if (!dynobj)
    ia64_info->root.dynobj = dynobj = abfd;

  srel = bfd_get_section_by_name (dynobj, srel_name);
  if (srel == NULL && create)
    {
      srel = bfd_make_section (dynobj, srel_name);
      if (srel == NULL
	  || !bfd_set_section_flags (dynobj, srel,
				     (SEC_ALLOC
				      | SEC_LOAD
				      | SEC_HAS_CONTENTS
				      | SEC_IN_MEMORY
				      | SEC_LINKER_CREATED
				      | SEC_READONLY))
	  || !bfd_set_section_alignment (dynobj, srel, 3))
	return NULL;
    }

  return srel;
}

static boolean
count_dyn_reloc (abfd, dyn_i, srel, type)
     bfd *abfd;
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     asection *srel;
     int type;
{
  struct elfNN_ia64_dyn_reloc_entry *rent;

  for (rent = dyn_i->reloc_entries; rent; rent = rent->next)
    if (rent->srel == srel && rent->type == type)
      break;

  if (!rent)
    {
      rent = (struct elfNN_ia64_dyn_reloc_entry *)
	bfd_alloc (abfd, sizeof (*rent));
      if (!rent)
	return false;

      rent->next = dyn_i->reloc_entries;
      rent->srel = srel;
      rent->type = type;
      rent->count = 0;
      dyn_i->reloc_entries = rent;
    }
  rent->count++;

  return true;
}

static boolean
elfNN_ia64_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  struct elfNN_ia64_link_hash_table *ia64_info;
  const Elf_Internal_Rela *relend;
  Elf_Internal_Shdr *symtab_hdr;
  const Elf_Internal_Rela *rel;
  asection *got, *fptr, *srel;

  if (info->relocateable)
    return true;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  ia64_info = elfNN_ia64_hash_table (info);

  got = fptr = srel = NULL;

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; ++rel)
    {
      enum {
	NEED_GOT = 1,
	NEED_FPTR = 2,
	NEED_PLTOFF = 4,
	NEED_MIN_PLT = 8,
	NEED_FULL_PLT = 16,
	NEED_DYNREL = 32,
	NEED_LTOFF_FPTR = 64,
      };

      struct elf_link_hash_entry *h = NULL;
      unsigned long r_symndx = ELFNN_R_SYM (rel->r_info);
      struct elfNN_ia64_dyn_sym_info *dyn_i;
      int need_entry;
      boolean maybe_dynamic;
      int dynrel_type = R_IA64_NONE;

      if (r_symndx >= symtab_hdr->sh_info)
	{
	  /* We're dealing with a global symbol -- find its hash entry
	     and mark it as being referenced.  */
	  long indx = r_symndx - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  h->elf_link_hash_flags |= ELF_LINK_HASH_REF_REGULAR;
	}

      /* We can only get preliminary data on whether a symbol is
	 locally or externally defined, as not all of the input files
	 have yet been processed.  Do something with what we know, as
	 this may help reduce memory usage and processing time later.  */
      maybe_dynamic = false;
      if (h && ((info->shared && ! info->symbolic)
		|| ! (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)
		|| h->root.type == bfd_link_hash_defweak))
	maybe_dynamic = true;

      need_entry = 0;
      switch (ELFNN_R_TYPE (rel->r_info))
	{
	case R_IA64_TPREL22:
	case R_IA64_TPREL64MSB:
	case R_IA64_TPREL64LSB:
	case R_IA64_LTOFF_TP22:
	  return false;

	case R_IA64_LTOFF_FPTR22:
	case R_IA64_LTOFF_FPTR64I:
	case R_IA64_LTOFF_FPTR64MSB:
	case R_IA64_LTOFF_FPTR64LSB:
	  need_entry = NEED_FPTR | NEED_GOT | NEED_LTOFF_FPTR;
	  break;

	case R_IA64_FPTR64I:
	case R_IA64_FPTR32MSB:
	case R_IA64_FPTR32LSB:
	case R_IA64_FPTR64MSB:
	case R_IA64_FPTR64LSB:
	  if (info->shared || h)
	    need_entry = NEED_FPTR | NEED_DYNREL;
	  else
	    need_entry = NEED_FPTR;
	  dynrel_type = R_IA64_FPTR64LSB;
	  break;

	case R_IA64_LTOFF22:
	case R_IA64_LTOFF22X:
	case R_IA64_LTOFF64I:
	  need_entry = NEED_GOT;
	  break;

	case R_IA64_PLTOFF22:
	case R_IA64_PLTOFF64I:
	case R_IA64_PLTOFF64MSB:
	case R_IA64_PLTOFF64LSB:
	  need_entry = NEED_PLTOFF;
	  if (h)
	    {
	      if (maybe_dynamic)
		need_entry |= NEED_MIN_PLT;
	    }
	  else
	    {
	      (*info->callbacks->warning)
		(info, _("@pltoff reloc against local symbol"), 0,
		 abfd, 0, 0);
	    }
	  break;

	case R_IA64_PCREL21B:
        case R_IA64_PCREL60B:
	  /* Depending on where this symbol is defined, we may or may not
	     need a full plt entry.  Only skip if we know we'll not need
	     the entry -- static or symbolic, and the symbol definition
	     has already been seen.  */
	  if (maybe_dynamic && rel->r_addend == 0)
	    need_entry = NEED_FULL_PLT;
	  break;

	case R_IA64_IMM14:
	case R_IA64_IMM22:
	case R_IA64_IMM64:
	case R_IA64_DIR32MSB:
	case R_IA64_DIR32LSB:
	case R_IA64_DIR64MSB:
	case R_IA64_DIR64LSB:
	  /* Shared objects will always need at least a REL relocation.  */
	  if (info->shared || maybe_dynamic)
	    need_entry = NEED_DYNREL;
	  dynrel_type = R_IA64_DIR64LSB;
	  break;

	case R_IA64_IPLTMSB:
	case R_IA64_IPLTLSB:
	  /* Shared objects will always need at least a REL relocation.  */
	  if (info->shared || maybe_dynamic)
	    need_entry = NEED_DYNREL;
	  dynrel_type = R_IA64_IPLTLSB;
	  break;

	case R_IA64_PCREL22:
	case R_IA64_PCREL64I:
	case R_IA64_PCREL32MSB:
	case R_IA64_PCREL32LSB:
	case R_IA64_PCREL64MSB:
	case R_IA64_PCREL64LSB:
	  if (maybe_dynamic)
	    need_entry = NEED_DYNREL;
	  dynrel_type = R_IA64_PCREL64LSB;
	  break;
	}

      if (!need_entry)
	continue;

      if ((need_entry & NEED_FPTR) != 0
	  && rel->r_addend)
	{
	  (*info->callbacks->warning)
	    (info, _("non-zero addend in @fptr reloc"), 0,
	     abfd, 0, 0);
	}

      dyn_i = get_dyn_sym_info (ia64_info, h, abfd, rel, true);

      /* Record whether or not this is a local symbol.  */
      dyn_i->h = h;

      /* Create what's needed.  */
      if (need_entry & NEED_GOT)
	{
	  if (!got)
	    {
	      got = get_got (abfd, info, ia64_info);
	      if (!got)
		return false;
	    }
	  dyn_i->want_got = 1;
	}
      if (need_entry & NEED_FPTR)
	{
	  if (!fptr)
	    {
	      fptr = get_fptr (abfd, info, ia64_info);
	      if (!fptr)
		return false;
	    }

	  /* FPTRs for shared libraries are allocated by the dynamic
	     linker.  Make sure this local symbol will appear in the
	     dynamic symbol table.  */
	  if (!h && info->shared)
	    {
	      if (! (_bfd_elfNN_link_record_local_dynamic_symbol
		     (info, abfd, r_symndx)))
		return false;
	    }

	  dyn_i->want_fptr = 1;
	}
      if (need_entry & NEED_LTOFF_FPTR)
	dyn_i->want_ltoff_fptr = 1;
      if (need_entry & (NEED_MIN_PLT | NEED_FULL_PLT))
	{
          if (!ia64_info->root.dynobj)
	    ia64_info->root.dynobj = abfd;
	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  dyn_i->want_plt = 1;
	}
      if (need_entry & NEED_FULL_PLT)
	dyn_i->want_plt2 = 1;
      if (need_entry & NEED_PLTOFF)
	dyn_i->want_pltoff = 1;
      if ((need_entry & NEED_DYNREL) && (sec->flags & SEC_ALLOC))
	{
	  if (!srel)
	    {
	      srel = get_reloc_section (abfd, ia64_info, sec, true);
	      if (!srel)
		return false;
	    }
	  if (!count_dyn_reloc (abfd, dyn_i, srel, dynrel_type))
	    return false;
	}
    }

  return true;
}

struct elfNN_ia64_allocate_data
{
  struct bfd_link_info *info;
  bfd_size_type ofs;
};

/* For cleanliness, and potentially faster dynamic loading, allocate
   external GOT entries first.  */

static boolean
allocate_global_data_got (dyn_i, data)
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     PTR data;
{
  struct elfNN_ia64_allocate_data *x = (struct elfNN_ia64_allocate_data *)data;

  if (dyn_i->want_got
      && ! dyn_i->want_fptr
      && elfNN_ia64_dynamic_symbol_p (dyn_i->h, x->info))
     {
       dyn_i->got_offset = x->ofs;
       x->ofs += 8;
     }
  return true;
}

/* Next, allocate all the GOT entries used by LTOFF_FPTR relocs.  */

static boolean
allocate_global_fptr_got (dyn_i, data)
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     PTR data;
{
  struct elfNN_ia64_allocate_data *x = (struct elfNN_ia64_allocate_data *)data;

  if (dyn_i->want_got
      && dyn_i->want_fptr
      && elfNN_ia64_dynamic_symbol_p (dyn_i->h, x->info))
    {
      dyn_i->got_offset = x->ofs;
      x->ofs += 8;
    }
  return true;
}

/* Lastly, allocate all the GOT entries for local data.  */

static boolean
allocate_local_got (dyn_i, data)
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     PTR data;
{
  struct elfNN_ia64_allocate_data *x = (struct elfNN_ia64_allocate_data *)data;

  if (dyn_i->want_got
      && ! elfNN_ia64_dynamic_symbol_p (dyn_i->h, x->info))
    {
      dyn_i->got_offset = x->ofs;
      x->ofs += 8;
    }
  return true;
}

/* Search for the index of a global symbol in it's defining object file.  */

static unsigned long
global_sym_index (h)
     struct elf_link_hash_entry *h;
{
  struct elf_link_hash_entry **p;
  bfd *obj;

  BFD_ASSERT (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak);

  obj = h->root.u.def.section->owner;
  for (p = elf_sym_hashes (obj); *p != h; ++p)
    continue;

  return p - elf_sym_hashes (obj) + elf_tdata (obj)->symtab_hdr.sh_info;
}

/* Allocate function descriptors.  We can do these for every function
   in a main executable that is not exported.  */

static boolean
allocate_fptr (dyn_i, data)
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     PTR data;
{
  struct elfNN_ia64_allocate_data *x = (struct elfNN_ia64_allocate_data *)data;

  if (dyn_i->want_fptr)
    {
      struct elf_link_hash_entry *h = dyn_i->h;

      if (h)
	while (h->root.type == bfd_link_hash_indirect
	       || h->root.type == bfd_link_hash_warning)
	  h = (struct elf_link_hash_entry *) h->root.u.i.link;

      if (x->info->shared)
	{
	  if (h && h->dynindx == -1)
	    {
	      BFD_ASSERT ((h->root.type == bfd_link_hash_defined)
			  || (h->root.type == bfd_link_hash_defweak));

	      if (!_bfd_elfNN_link_record_local_dynamic_symbol
		    (x->info, h->root.u.def.section->owner,
		     global_sym_index (h)))
		return false;
	    }

	  dyn_i->want_fptr = 0;
	}
      else if (h == NULL || h->dynindx == -1)
	{
	  dyn_i->fptr_offset = x->ofs;
	  x->ofs += 16;
	}
      else
	dyn_i->want_fptr = 0;
    }
  return true;
}

/* Allocate all the minimal PLT entries.  */

static boolean
allocate_plt_entries (dyn_i, data)
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     PTR data;
{
  struct elfNN_ia64_allocate_data *x = (struct elfNN_ia64_allocate_data *)data;

  if (dyn_i->want_plt)
    {
      struct elf_link_hash_entry *h = dyn_i->h;

      if (h)
	while (h->root.type == bfd_link_hash_indirect
	       || h->root.type == bfd_link_hash_warning)
	  h = (struct elf_link_hash_entry *) h->root.u.i.link;

      /* ??? Versioned symbols seem to lose ELF_LINK_HASH_NEEDS_PLT.  */
      if (elfNN_ia64_dynamic_symbol_p (h, x->info))
	{
	  bfd_size_type offset = x->ofs;
	  if (offset == 0)
	    offset = PLT_HEADER_SIZE;
	  dyn_i->plt_offset = offset;
	  x->ofs = offset + PLT_MIN_ENTRY_SIZE;

	  dyn_i->want_pltoff = 1;
	}
      else
	{
	  dyn_i->want_plt = 0;
	  dyn_i->want_plt2 = 0;
	}
    }
  return true;
}

/* Allocate all the full PLT entries.  */

static boolean
allocate_plt2_entries (dyn_i, data)
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     PTR data;
{
  struct elfNN_ia64_allocate_data *x = (struct elfNN_ia64_allocate_data *)data;

  if (dyn_i->want_plt2)
    {
      struct elf_link_hash_entry *h = dyn_i->h;
      bfd_size_type ofs = x->ofs;

      dyn_i->plt2_offset = ofs;
      x->ofs = ofs + PLT_FULL_ENTRY_SIZE;

      while (h->root.type == bfd_link_hash_indirect
	     || h->root.type == bfd_link_hash_warning)
	h = (struct elf_link_hash_entry *) h->root.u.i.link;
      dyn_i->h->plt.offset = ofs;
    }
  return true;
}

/* Allocate all the PLTOFF entries requested by relocations and
   plt entries.  We can't share space with allocated FPTR entries,
   because the latter are not necessarily addressable by the GP.
   ??? Relaxation might be able to determine that they are.  */

static boolean
allocate_pltoff_entries (dyn_i, data)
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     PTR data;
{
  struct elfNN_ia64_allocate_data *x = (struct elfNN_ia64_allocate_data *)data;

  if (dyn_i->want_pltoff)
    {
      dyn_i->pltoff_offset = x->ofs;
      x->ofs += 16;
    }
  return true;
}

/* Allocate dynamic relocations for those symbols that turned out
   to be dynamic.  */

static boolean
allocate_dynrel_entries (dyn_i, data)
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     PTR data;
{
  struct elfNN_ia64_allocate_data *x = (struct elfNN_ia64_allocate_data *)data;
  struct elfNN_ia64_link_hash_table *ia64_info;
  struct elfNN_ia64_dyn_reloc_entry *rent;
  boolean dynamic_symbol, shared;

  ia64_info = elfNN_ia64_hash_table (x->info);
  dynamic_symbol = elfNN_ia64_dynamic_symbol_p (dyn_i->h, x->info);
  shared = x->info->shared;

  /* Take care of the normal data relocations.  */

  for (rent = dyn_i->reloc_entries; rent; rent = rent->next)
    {
      int count = rent->count;

      switch (rent->type)
	{
	case R_IA64_FPTR64LSB:
	  /* Allocate one iff !want_fptr, which by this point will
	     be true only if we're actually allocating one statically
	     in the main executable.  */
	  if (dyn_i->want_fptr)
	    continue;
	  break;
	case R_IA64_PCREL64LSB:
	  if (!dynamic_symbol)
	    continue;
	  break;
	case R_IA64_DIR64LSB:
	  if (!dynamic_symbol && !shared)
	    continue;
	  break;
	case R_IA64_IPLTLSB:
	  if (!dynamic_symbol && !shared)
	    continue;
	  /* Use two REL relocations for IPLT relocations
	     against local symbols.  */
	  if (!dynamic_symbol)
	    count *= 2;
	  break;
	default:
	  abort ();
	}
      rent->srel->_raw_size += sizeof (ElfNN_External_Rela) * count;
    }

  /* Take care of the GOT and PLT relocations.  */

  if (((dynamic_symbol || shared) && dyn_i->want_got)
      || (dyn_i->want_ltoff_fptr && dyn_i->h && dyn_i->h->dynindx != -1))
    ia64_info->rel_got_sec->_raw_size += sizeof (ElfNN_External_Rela);

  if (dyn_i->want_pltoff)
    {
      bfd_size_type t = 0;

      /* Dynamic symbols get one IPLT relocation.  Local symbols in
	 shared libraries get two REL relocations.  Local symbols in
	 main applications get nothing.  */
      if (dynamic_symbol)
	t = sizeof (ElfNN_External_Rela);
      else if (shared)
	t = 2 * sizeof (ElfNN_External_Rela);

      ia64_info->rel_pltoff_sec->_raw_size += t;
    }

  return true;
}

static boolean
elfNN_ia64_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elf_link_hash_entry *h;
{
  /* ??? Undefined symbols with PLT entries should be re-defined
     to be the PLT entry.  */

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

  /* If this is a reference to a symbol defined by a dynamic object which
     is not a function, we might allocate the symbol in our .dynbss section
     and allocate a COPY dynamic relocation.

     But IA-64 code is canonically PIC, so as a rule we can avoid this sort
     of hackery.  */

  return true;
}

static boolean
elfNN_ia64_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  struct elfNN_ia64_allocate_data data;
  struct elfNN_ia64_link_hash_table *ia64_info;
  asection *sec;
  bfd *dynobj;
  boolean reltext = false;
  boolean relplt = false;

  dynobj = elf_hash_table(info)->dynobj;
  ia64_info = elfNN_ia64_hash_table (info);
  BFD_ASSERT(dynobj != NULL);
  data.info = info;

  /* Set the contents of the .interp section to the interpreter.  */
  if (ia64_info->root.dynamic_sections_created
      && !info->shared)
    {
      sec = bfd_get_section_by_name (dynobj, ".interp");
      BFD_ASSERT (sec != NULL);
      sec->contents = (bfd_byte *) ELF_DYNAMIC_INTERPRETER;
      sec->_raw_size = strlen (ELF_DYNAMIC_INTERPRETER) + 1;
    }

  /* Allocate the GOT entries.  */

  if (ia64_info->got_sec)
    {
      data.ofs = 0;
      elfNN_ia64_dyn_sym_traverse (ia64_info, allocate_global_data_got, &data);
      elfNN_ia64_dyn_sym_traverse (ia64_info, allocate_global_fptr_got, &data);
      elfNN_ia64_dyn_sym_traverse (ia64_info, allocate_local_got, &data);
      ia64_info->got_sec->_raw_size = data.ofs;
    }

  /* Allocate the FPTR entries.  */

  if (ia64_info->fptr_sec)
    {
      data.ofs = 0;
      elfNN_ia64_dyn_sym_traverse (ia64_info, allocate_fptr, &data);
      ia64_info->fptr_sec->_raw_size = data.ofs;
    }

  /* Now that we've seen all of the input files, we can decide which
     symbols need plt entries.  Allocate the minimal PLT entries first.
     We do this even though dynamic_sections_created may be false, because
     this has the side-effect of clearing want_plt and want_plt2.  */

  data.ofs = 0;
  elfNN_ia64_dyn_sym_traverse (ia64_info, allocate_plt_entries, &data);

  ia64_info->minplt_entries = 0;
  if (data.ofs)
    {
      ia64_info->minplt_entries
	= (data.ofs - PLT_HEADER_SIZE) / PLT_MIN_ENTRY_SIZE;
    }

  /* Align the pointer for the plt2 entries.  */
  data.ofs = (data.ofs + 31) & -32;

  elfNN_ia64_dyn_sym_traverse (ia64_info, allocate_plt2_entries, &data);
  if (data.ofs != 0)
    {
      BFD_ASSERT (ia64_info->root.dynamic_sections_created);

      ia64_info->plt_sec->_raw_size = data.ofs;

      /* If we've got a .plt, we need some extra memory for the dynamic
	 linker.  We stuff these in .got.plt.  */
      sec = bfd_get_section_by_name (dynobj, ".got.plt");
      sec->_raw_size = 8 * PLT_RESERVED_WORDS;
    }

  /* Allocate the PLTOFF entries.  */

  if (ia64_info->pltoff_sec)
    {
      data.ofs = 0;
      elfNN_ia64_dyn_sym_traverse (ia64_info, allocate_pltoff_entries, &data);
      ia64_info->pltoff_sec->_raw_size = data.ofs;
    }

  if (ia64_info->root.dynamic_sections_created)
    {
      /* Allocate space for the dynamic relocations that turned out to be
	 required.  */

      elfNN_ia64_dyn_sym_traverse (ia64_info, allocate_dynrel_entries, &data);
    }

  /* We have now determined the sizes of the various dynamic sections.
     Allocate memory for them.  */
  for (sec = dynobj->sections; sec != NULL; sec = sec->next)
    {
      boolean strip;

      if (!(sec->flags & SEC_LINKER_CREATED))
	continue;

      /* If we don't need this section, strip it from the output file.
	 There were several sections primarily related to dynamic
	 linking that must be create before the linker maps input
	 sections to output sections.  The linker does that before
	 bfd_elf_size_dynamic_sections is called, and it is that
	 function which decides whether anything needs to go into
	 these sections.  */

      strip = (sec->_raw_size == 0);

      if (sec == ia64_info->got_sec)
	strip = false;
      else if (sec == ia64_info->rel_got_sec)
	{
	  if (strip)
	    ia64_info->rel_got_sec = NULL;
	  else
	    /* We use the reloc_count field as a counter if we need to
	       copy relocs into the output file.  */
	    sec->reloc_count = 0;
	}
      else if (sec == ia64_info->fptr_sec)
	{
	  if (strip)
	    ia64_info->fptr_sec = NULL;
	}
      else if (sec == ia64_info->plt_sec)
	{
	  if (strip)
	    ia64_info->plt_sec = NULL;
	}
      else if (sec == ia64_info->pltoff_sec)
	{
	  if (strip)
	    ia64_info->pltoff_sec = NULL;
	}
      else if (sec == ia64_info->rel_pltoff_sec)
	{
	  if (strip)
	    ia64_info->rel_pltoff_sec = NULL;
	  else
	    {
	      relplt = true;
	      /* We use the reloc_count field as a counter if we need to
		 copy relocs into the output file.  */
	      sec->reloc_count = 0;
	    }
	}
      else
	{
	  const char *name;

	  /* It's OK to base decisions on the section name, because none
	     of the dynobj section names depend upon the input files.  */
	  name = bfd_get_section_name (dynobj, sec);

	  if (strcmp (name, ".got.plt") == 0)
	    strip = false;
	  else if (strncmp (name, ".rel", 4) == 0)
	    {
	      if (!strip)
		{
		  const char *outname;
		  asection *target;

		  /* If this relocation section applies to a read only
		     section, then we probably need a DT_TEXTREL entry.  */
		  outname = bfd_get_section_name (output_bfd,
						  sec->output_section);
		  if (outname[4] == 'a')
		    outname += 5;
		  else
		    outname += 4;

		  target = bfd_get_section_by_name (output_bfd, outname);
		  if (target != NULL
		      && (target->flags & SEC_READONLY) != 0
		      && (target->flags & SEC_ALLOC) != 0)
		    reltext = true;

		  /* We use the reloc_count field as a counter if we need to
		     copy relocs into the output file.  */
		  sec->reloc_count = 0;
		}
	    }
	  else
	    continue;
	}

      if (strip)
	_bfd_strip_section_from_output (info, sec);
      else
	{
	  /* Allocate memory for the section contents.  */
	  sec->contents = (bfd_byte *) bfd_zalloc(dynobj, sec->_raw_size);
	  if (sec->contents == NULL && sec->_raw_size != 0)
	    return false;
	}
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the values
	 later (in finish_dynamic_sections) but we must add the entries now
	 so that we get the correct size for the .dynamic section.  */

      if (!info->shared)
	{
	  /* The DT_DEBUG entry is filled in by the dynamic linker and used
	     by the debugger.  */
	  if (!bfd_elfNN_add_dynamic_entry (info, DT_DEBUG, 0))
	    return false;
	}

      if (! bfd_elfNN_add_dynamic_entry (info, DT_IA_64_PLT_RESERVE, 0))
	return false;
      if (! bfd_elfNN_add_dynamic_entry (info, DT_PLTGOT, 0))
	return false;

      if (relplt)
	{
	  if (! bfd_elfNN_add_dynamic_entry (info, DT_PLTRELSZ, 0)
	      || ! bfd_elfNN_add_dynamic_entry (info, DT_PLTREL, DT_RELA)
	      || ! bfd_elfNN_add_dynamic_entry (info, DT_JMPREL, 0))
	    return false;
	}

      if (! bfd_elfNN_add_dynamic_entry (info, DT_RELA, 0)
	  || ! bfd_elfNN_add_dynamic_entry (info, DT_RELASZ, 0)
	  || ! bfd_elfNN_add_dynamic_entry (info, DT_RELAENT,
					    sizeof (ElfNN_External_Rela)))
	return false;

      if (reltext)
	{
	  if (! bfd_elfNN_add_dynamic_entry (info, DT_TEXTREL, 0))
	    return false;
	  info->flags |= DF_TEXTREL;
	}
    }

  /* ??? Perhaps force __gp local.  */

  return true;
}

static bfd_reloc_status_type
elfNN_ia64_install_value (abfd, hit_addr, val, r_type)
     bfd *abfd;
     bfd_byte *hit_addr;
     bfd_vma val;
     unsigned int r_type;
{
  const struct ia64_operand *op;
  int bigendian = 0, shift = 0;
  bfd_vma t0, t1, insn, dword;
  enum ia64_opnd opnd;
  const char *err;
  size_t size = 8;

  opnd = IA64_OPND_NIL;
  switch (r_type)
    {
    case R_IA64_NONE:
    case R_IA64_LDXMOV:
      return bfd_reloc_ok;

      /* Instruction relocations.  */

    case R_IA64_IMM14:		opnd = IA64_OPND_IMM14; break;

    case R_IA64_PCREL21F:	opnd = IA64_OPND_TGT25; break;
    case R_IA64_PCREL21M:	opnd = IA64_OPND_TGT25b; break;
    case R_IA64_PCREL60B:	opnd = IA64_OPND_TGT64; break;
    case R_IA64_PCREL21B:
    case R_IA64_PCREL21BI:
      opnd = IA64_OPND_TGT25c;
      break;

    case R_IA64_IMM22:
    case R_IA64_GPREL22:
    case R_IA64_LTOFF22:
    case R_IA64_LTOFF22X:
    case R_IA64_PLTOFF22:
    case R_IA64_PCREL22:
    case R_IA64_LTOFF_FPTR22:
      opnd = IA64_OPND_IMM22;
      break;

    case R_IA64_IMM64:
    case R_IA64_GPREL64I:
    case R_IA64_LTOFF64I:
    case R_IA64_PLTOFF64I:
    case R_IA64_PCREL64I:
    case R_IA64_FPTR64I:
    case R_IA64_LTOFF_FPTR64I:
      opnd = IA64_OPND_IMMU64;
      break;

      /* Data relocations.  */

    case R_IA64_DIR32MSB:
    case R_IA64_GPREL32MSB:
    case R_IA64_FPTR32MSB:
    case R_IA64_PCREL32MSB:
    case R_IA64_SEGREL32MSB:
    case R_IA64_SECREL32MSB:
    case R_IA64_LTV32MSB:
      size = 4; bigendian = 1;
      break;

    case R_IA64_DIR32LSB:
    case R_IA64_GPREL32LSB:
    case R_IA64_FPTR32LSB:
    case R_IA64_PCREL32LSB:
    case R_IA64_SEGREL32LSB:
    case R_IA64_SECREL32LSB:
    case R_IA64_LTV32LSB:
      size = 4; bigendian = 0;
      break;

    case R_IA64_DIR64MSB:
    case R_IA64_GPREL64MSB:
    case R_IA64_PLTOFF64MSB:
    case R_IA64_FPTR64MSB:
    case R_IA64_PCREL64MSB:
    case R_IA64_LTOFF_FPTR64MSB:
    case R_IA64_SEGREL64MSB:
    case R_IA64_SECREL64MSB:
    case R_IA64_LTV64MSB:
      size = 8; bigendian = 1;
      break;

    case R_IA64_DIR64LSB:
    case R_IA64_GPREL64LSB:
    case R_IA64_PLTOFF64LSB:
    case R_IA64_FPTR64LSB:
    case R_IA64_PCREL64LSB:
    case R_IA64_LTOFF_FPTR64LSB:
    case R_IA64_SEGREL64LSB:
    case R_IA64_SECREL64LSB:
    case R_IA64_LTV64LSB:
      size = 8; bigendian = 0;
      break;

      /* Unsupported / Dynamic relocations.  */
    default:
      return bfd_reloc_notsupported;
    }

  switch (opnd)
    {
    case IA64_OPND_IMMU64:
      hit_addr -= (long) hit_addr & 0x3;
      t0 = bfd_get_64 (abfd, hit_addr);
      t1 = bfd_get_64 (abfd, hit_addr + 8);

      /* tmpl/s: bits  0.. 5 in t0
	 slot 0: bits  5..45 in t0
	 slot 1: bits 46..63 in t0, bits 0..22 in t1
	 slot 2: bits 23..63 in t1 */

      /* First, clear the bits that form the 64 bit constant.  */
      t0 &= ~(0x3ffffLL << 46);
      t1 &= ~(0x7fffffLL
	      | ((  (0x07fLL << 13) | (0x1ffLL << 27)
		    | (0x01fLL << 22) | (0x001LL << 21)
		    | (0x001LL << 36)) << 23));

      t0 |= ((val >> 22) & 0x03ffffLL) << 46;		/* 18 lsbs of imm41 */
      t1 |= ((val >> 40) & 0x7fffffLL) <<  0;		/* 23 msbs of imm41 */
      t1 |= (  (((val >>  0) & 0x07f) << 13)		/* imm7b */
	       | (((val >>  7) & 0x1ff) << 27)		/* imm9d */
	       | (((val >> 16) & 0x01f) << 22)		/* imm5c */
	       | (((val >> 21) & 0x001) << 21)		/* ic */
	       | (((val >> 63) & 0x001) << 36)) << 23;	/* i */

      bfd_put_64 (abfd, t0, hit_addr);
      bfd_put_64 (abfd, t1, hit_addr + 8);
      break;

    case IA64_OPND_TGT64:
      hit_addr -= (long) hit_addr & 0x3;
      t0 = bfd_get_64 (abfd, hit_addr);
      t1 = bfd_get_64 (abfd, hit_addr + 8);

      /* tmpl/s: bits  0.. 5 in t0
	 slot 0: bits  5..45 in t0
	 slot 1: bits 46..63 in t0, bits 0..22 in t1
	 slot 2: bits 23..63 in t1 */

      /* First, clear the bits that form the 64 bit constant.  */
      t0 &= ~(0x3ffffLL << 46);
      t1 &= ~(0x7fffffLL
	      | ((1LL << 36 | 0xfffffLL << 13) << 23));

      val >>= 4;
      t0 |= ((val >> 20) & 0xffffLL) << 2 << 46;	/* 16 lsbs of imm39 */
      t1 |= ((val >> 36) & 0x7fffffLL) << 0;		/* 23 msbs of imm39 */
      t1 |= ((((val >> 0) & 0xfffffLL) << 13)		/* imm20b */
	      | (((val >> 59) & 0x1LL) << 36)) << 23;	/* i */

      bfd_put_64 (abfd, t0, hit_addr);
      bfd_put_64 (abfd, t1, hit_addr + 8);
      break;

    default:
      switch ((long) hit_addr & 0x3)
	{
	case 0: shift =  5; break;
	case 1: shift = 14; hit_addr += 3; break;
	case 2: shift = 23; hit_addr += 6; break;
	case 3: return bfd_reloc_notsupported; /* shouldn't happen...  */
	}
      dword = bfd_get_64 (abfd, hit_addr);
      insn = (dword >> shift) & 0x1ffffffffffLL;

      op = elf64_ia64_operands + opnd;
      err = (*op->insert) (op, val, &insn);
      if (err)
	return bfd_reloc_overflow;

      dword &= ~(0x1ffffffffffLL << shift);
      dword |= (insn << shift);
      bfd_put_64 (abfd, dword, hit_addr);
      break;

    case IA64_OPND_NIL:
      /* A data relocation.  */
      if (bigendian)
	if (size == 4)
	  bfd_putb32 (val, hit_addr);
	else
	  bfd_putb64 (val, hit_addr);
      else
	if (size == 4)
	  bfd_putl32 (val, hit_addr);
	else
	  bfd_putl64 (val, hit_addr);
      break;
    }

  return bfd_reloc_ok;
}

static void
elfNN_ia64_install_dyn_reloc (abfd, info, sec, srel, offset, type,
			      dynindx, addend)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     asection *srel;
     bfd_vma offset;
     unsigned int type;
     long dynindx;
     bfd_vma addend;
{
  Elf_Internal_Rela outrel;

  outrel.r_offset = (sec->output_section->vma
		     + sec->output_offset
		     + offset);

  BFD_ASSERT (dynindx != -1);
  outrel.r_info = ELFNN_R_INFO (dynindx, type);
  outrel.r_addend = addend;

  if (elf_section_data (sec)->stab_info != NULL)
    {
      /* This may be NULL for linker-generated relocations, as it is
	 inconvenient to pass all the bits around.  And this shouldn't
	 happen.  */
      BFD_ASSERT (info != NULL);

      offset = (_bfd_stab_section_offset
		(abfd, &elf_hash_table (info)->stab_info, sec,
		 &elf_section_data (sec)->stab_info, offset));
      if (offset == (bfd_vma) -1)
	{
	  /* Run for the hills.  We shouldn't be outputting a relocation
	     for this.  So do what everyone else does and output a no-op.  */
	  outrel.r_info = ELFNN_R_INFO (0, R_IA64_NONE);
	  outrel.r_addend = 0;
	  offset = 0;
	}
      outrel.r_offset = offset;
    }

  bfd_elfNN_swap_reloca_out (abfd, &outrel,
			     ((ElfNN_External_Rela *) srel->contents
			      + srel->reloc_count++));
  BFD_ASSERT (sizeof (ElfNN_External_Rela) * srel->reloc_count
	      <= srel->_cooked_size);
}

/* Store an entry for target address TARGET_ADDR in the linkage table
   and return the gp-relative address of the linkage table entry.  */

static bfd_vma
set_got_entry (abfd, info, dyn_i, dynindx, addend, value, dyn_r_type)
     bfd *abfd;
     struct bfd_link_info *info;
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     long dynindx;
     bfd_vma addend;
     bfd_vma value;
     unsigned int dyn_r_type;
{
  struct elfNN_ia64_link_hash_table *ia64_info;
  asection *got_sec;

  ia64_info = elfNN_ia64_hash_table (info);
  got_sec = ia64_info->got_sec;

  BFD_ASSERT ((dyn_i->got_offset & 7) == 0);

  if (! dyn_i->got_done)
    {
      dyn_i->got_done = true;

      /* Store the target address in the linkage table entry.  */
      bfd_put_64 (abfd, value, got_sec->contents + dyn_i->got_offset);

      /* Install a dynamic relocation if needed.  */
      if (info->shared
          || elfNN_ia64_dynamic_symbol_p (dyn_i->h, info)
	  || (dynindx != -1 && dyn_r_type == R_IA64_FPTR64LSB))
	{
	  if (dynindx == -1)
	    {
	      dyn_r_type = R_IA64_REL64LSB;
	      dynindx = 0;
	      addend = value;
	    }

	  if (bfd_big_endian (abfd))
	    {
	      switch (dyn_r_type)
		{
		case R_IA64_REL64LSB:
		  dyn_r_type = R_IA64_REL64MSB;
		  break;
		case R_IA64_DIR64LSB:
		  dyn_r_type = R_IA64_DIR64MSB;
		  break;
		case R_IA64_FPTR64LSB:
		  dyn_r_type = R_IA64_FPTR64MSB;
		  break;
		default:
		  BFD_ASSERT (false);
		  break;
		}
	    }

	  elfNN_ia64_install_dyn_reloc (abfd, NULL, got_sec,
					ia64_info->rel_got_sec,
					dyn_i->got_offset, dyn_r_type,
					dynindx, addend);
	}
    }

  /* Return the address of the linkage table entry.  */
  value = (got_sec->output_section->vma
	   + got_sec->output_offset
	   + dyn_i->got_offset);

  return value;
}

/* Fill in a function descriptor consisting of the function's code
   address and its global pointer.  Return the descriptor's address.  */

static bfd_vma
set_fptr_entry (abfd, info, dyn_i, value)
     bfd *abfd;
     struct bfd_link_info *info;
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     bfd_vma value;
{
  struct elfNN_ia64_link_hash_table *ia64_info;
  asection *fptr_sec;

  ia64_info = elfNN_ia64_hash_table (info);
  fptr_sec = ia64_info->fptr_sec;

  if (!dyn_i->fptr_done)
    {
      dyn_i->fptr_done = 1;

      /* Fill in the function descriptor.  */
      bfd_put_64 (abfd, value, fptr_sec->contents + dyn_i->fptr_offset);
      bfd_put_64 (abfd, _bfd_get_gp_value (abfd),
		  fptr_sec->contents + dyn_i->fptr_offset + 8);
    }

  /* Return the descriptor's address.  */
  value = (fptr_sec->output_section->vma
	   + fptr_sec->output_offset
	   + dyn_i->fptr_offset);

  return value;
}

/* Fill in a PLTOFF entry consisting of the function's code address
   and its global pointer.  Return the descriptor's address.  */

static bfd_vma
set_pltoff_entry (abfd, info, dyn_i, value, is_plt)
     bfd *abfd;
     struct bfd_link_info *info;
     struct elfNN_ia64_dyn_sym_info *dyn_i;
     bfd_vma value;
     boolean is_plt;
{
  struct elfNN_ia64_link_hash_table *ia64_info;
  asection *pltoff_sec;

  ia64_info = elfNN_ia64_hash_table (info);
  pltoff_sec = ia64_info->pltoff_sec;

  /* Don't do anything if this symbol uses a real PLT entry.  In
     that case, we'll fill this in during finish_dynamic_symbol.  */
  if ((! dyn_i->want_plt || is_plt)
      && !dyn_i->pltoff_done)
    {
      bfd_vma gp = _bfd_get_gp_value (abfd);

      /* Fill in the function descriptor.  */
      bfd_put_64 (abfd, value, pltoff_sec->contents + dyn_i->pltoff_offset);
      bfd_put_64 (abfd, gp, pltoff_sec->contents + dyn_i->pltoff_offset + 8);

      /* Install dynamic relocations if needed.  */
      if (!is_plt && info->shared)
	{
	  unsigned int dyn_r_type;

	  if (bfd_big_endian (abfd))
	    dyn_r_type = R_IA64_REL64MSB;
	  else
	    dyn_r_type = R_IA64_REL64LSB;

	  elfNN_ia64_install_dyn_reloc (abfd, NULL, pltoff_sec,
					ia64_info->rel_pltoff_sec,
					dyn_i->pltoff_offset,
					dyn_r_type, 0, value);
	  elfNN_ia64_install_dyn_reloc (abfd, NULL, pltoff_sec,
					ia64_info->rel_pltoff_sec,
					dyn_i->pltoff_offset + 8,
					dyn_r_type, 0, gp);
	}

      dyn_i->pltoff_done = 1;
    }

  /* Return the descriptor's address.  */
  value = (pltoff_sec->output_section->vma
	   + pltoff_sec->output_offset
	   + dyn_i->pltoff_offset);

  return value;
}

/* Called through qsort to sort the .IA_64.unwind section during a
   non-relocatable link.  Set elfNN_ia64_unwind_entry_compare_bfd
   to the output bfd so we can do proper endianness frobbing.  */

static bfd *elfNN_ia64_unwind_entry_compare_bfd;

static int
elfNN_ia64_unwind_entry_compare (a, b)
     PTR a;
     PTR b;
{
  bfd_vma av, bv;

  av = bfd_get_64 (elfNN_ia64_unwind_entry_compare_bfd, a);
  bv = bfd_get_64 (elfNN_ia64_unwind_entry_compare_bfd, b);

  return (av < bv ? -1 : av > bv ? 1 : 0);
}

static boolean
elfNN_ia64_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  struct elfNN_ia64_link_hash_table *ia64_info;
  asection *unwind_output_sec;

  ia64_info = elfNN_ia64_hash_table (info);

  /* Make sure we've got ourselves a nice fat __gp value.  */
  if (!info->relocateable)
    {
      bfd_vma min_vma = (bfd_vma) -1, max_vma = 0;
      bfd_vma min_short_vma = min_vma, max_short_vma = 0;
      struct elf_link_hash_entry *gp;
      bfd_vma gp_val;
      asection *os;

      /* Find the min and max vma of all sections marked short.  Also
	 collect min and max vma of any type, for use in selecting a
	 nice gp.  */
      for (os = abfd->sections; os ; os = os->next)
	{
	  bfd_vma lo, hi;

	  if ((os->flags & SEC_ALLOC) == 0)
	    continue;

	  lo = os->vma;
	  hi = os->vma + os->_raw_size;
	  if (hi < lo)
	    hi = (bfd_vma) -1;

	  if (min_vma > lo)
	    min_vma = lo;
	  if (max_vma < hi)
	    max_vma = hi;
	  if (os->flags & SEC_SMALL_DATA)
	    {
	      if (min_short_vma > lo)
		min_short_vma = lo;
	      if (max_short_vma < hi)
		max_short_vma = hi;
	    }
	}

      /* See if the user wants to force a value.  */
      gp = elf_link_hash_lookup (elf_hash_table (info), "__gp", false,
				 false, false);

      if (gp
	  && (gp->root.type == bfd_link_hash_defined
	      || gp->root.type == bfd_link_hash_defweak))
	{
	  asection *gp_sec = gp->root.u.def.section;
	  gp_val = (gp->root.u.def.value
		    + gp_sec->output_section->vma
		    + gp_sec->output_offset);
	}
      else
	{
	  /* Pick a sensible value.  */

	  asection *got_sec = ia64_info->got_sec;

	  /* Start with just the address of the .got.  */
	  if (got_sec)
	    gp_val = got_sec->output_section->vma;
	  else if (max_short_vma != 0)
	    gp_val = min_short_vma;
	  else
	    gp_val = min_vma;

	  /* If it is possible to address the entire image, but we
	     don't with the choice above, adjust.  */
	  if (max_vma - min_vma < 0x400000
	      && max_vma - gp_val <= 0x200000
	      && gp_val - min_vma > 0x200000)
	    gp_val = min_vma + 0x200000;
	  else if (max_short_vma != 0)
	    {
	      /* If we don't cover all the short data, adjust.  */
	      if (max_short_vma - gp_val >= 0x200000)
		gp_val = min_short_vma + 0x200000;

	      /* If we're addressing stuff past the end, adjust back.  */
	      if (gp_val > max_vma)
		gp_val = max_vma - 0x200000 + 8;
	    }
	}

      /* Validate whether all SHF_IA_64_SHORT sections are within
	 range of the chosen GP.  */

      if (max_short_vma != 0)
	{
	  if (max_short_vma - min_short_vma >= 0x400000)
	    {
	      (*_bfd_error_handler)
		(_("%s: short data segment overflowed (0x%lx >= 0x400000)"),
		 bfd_get_filename (abfd),
		 (unsigned long) (max_short_vma - min_short_vma));
	      return false;
	    }
	  else if ((gp_val > min_short_vma
		    && gp_val - min_short_vma > 0x200000)
		   || (gp_val < max_short_vma
		       && max_short_vma - gp_val >= 0x200000))
	    {
	      (*_bfd_error_handler)
		(_("%s: __gp does not cover short data segment"),
		 bfd_get_filename (abfd));
	      return false;
	    }
	}

      _bfd_set_gp_value (abfd, gp_val);

      if (gp)
	{
	  gp->root.type = bfd_link_hash_defined;
	  gp->root.u.def.value = gp_val;
	  gp->root.u.def.section = bfd_abs_section_ptr;
	}
    }

  /* If we're producing a final executable, we need to sort the contents
     of the .IA_64.unwind section.  Force this section to be relocated
     into memory rather than written immediately to the output file.  */
  unwind_output_sec = NULL;
  if (!info->relocateable)
    {
      asection *s = bfd_get_section_by_name (abfd, ELF_STRING_ia64_unwind);
      if (s)
	{
	  unwind_output_sec = s->output_section;
	  unwind_output_sec->contents
	    = bfd_malloc (unwind_output_sec->_raw_size);
	  if (unwind_output_sec->contents == NULL)
	    return false;
	}
    }

  /* Invoke the regular ELF backend linker to do all the work.  */
  if (!bfd_elfNN_bfd_final_link (abfd, info))
    return false;

  if (unwind_output_sec)
    {
      elfNN_ia64_unwind_entry_compare_bfd = abfd;
      qsort (unwind_output_sec->contents, unwind_output_sec->_raw_size / 24,
	     24, elfNN_ia64_unwind_entry_compare);

      if (! bfd_set_section_contents (abfd, unwind_output_sec,
				      unwind_output_sec->contents, 0,
				      unwind_output_sec->_raw_size))
	return false;
    }

  return true;
}

static boolean
elfNN_ia64_relocate_section (output_bfd, info, input_bfd, input_section,
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
  struct elfNN_ia64_link_hash_table *ia64_info;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;
  asection *srel;
  boolean ret_val = true;	/* for non-fatal errors */
  bfd_vma gp_val;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  ia64_info = elfNN_ia64_hash_table (info);

  /* Infect various flags from the input section to the output section.  */
  if (info->relocateable)
    {
      bfd_vma flags;

      flags = elf_section_data(input_section)->this_hdr.sh_flags;
      flags &= SHF_IA_64_NORECOV;

      elf_section_data(input_section->output_section)
	->this_hdr.sh_flags |= flags;
    }

  gp_val = _bfd_get_gp_value (output_bfd);
  srel = get_reloc_section (input_bfd, ia64_info, input_section, false);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; ++rel)
    {
      struct elf_link_hash_entry *h;
      struct elfNN_ia64_dyn_sym_info *dyn_i;
      bfd_reloc_status_type r;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      unsigned int r_type;
      bfd_vma value;
      asection *sym_sec;
      bfd_byte *hit_addr;
      boolean dynamic_symbol_p;
      boolean undef_weak_ref;

      r_type = ELFNN_R_TYPE (rel->r_info);
      if (r_type > R_IA64_MAX_RELOC_CODE)
	{
	  (*_bfd_error_handler)
	    (_("%s: unknown relocation type %d"),
	     bfd_get_filename (input_bfd), (int)r_type);
	  bfd_set_error (bfd_error_bad_value);
	  ret_val = false;
	  continue;
	}
      howto = lookup_howto (r_type);
      r_symndx = ELFNN_R_SYM (rel->r_info);

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
      undef_weak_ref = false;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  /* Reloc against local symbol.  */
	  sym = local_syms + r_symndx;
	  sym_sec = local_sections[r_symndx];
	  value  = (sym_sec->output_section->vma
		    + sym_sec->output_offset
		    + sym->st_value);
	}
      else
	{
	  long indx;

	  /* Reloc against global symbol.  */
	  indx = r_symndx - symtab_hdr->sh_info;
	  h = elf_sym_hashes (input_bfd)[indx];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  value = 0;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sym_sec = h->root.u.def.section;

	      /* Detect the cases that sym_sec->output_section is
		 expected to be NULL -- all cases in which the symbol
		 is defined in another shared module.  This includes
		 PLT relocs for which we've created a PLT entry and
		 other relocs for which we're prepared to create
		 dynamic relocations.  */
	      /* ??? Just accept it NULL and continue.  */

	      if (sym_sec->output_section != NULL)
		{
		  value = (h->root.u.def.value
			   + sym_sec->output_section->vma
			   + sym_sec->output_offset);
		}
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    undef_weak_ref = true;
	  else if (info->shared && !info->symbolic
		   && !info->no_undefined
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    ;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset,
		      (!info->shared || info->no_undefined
		       || ELF_ST_VISIBILITY (h->other)))))
		return false;
	      ret_val = false;
	      continue;
	    }
	}

      hit_addr = contents + rel->r_offset;
      value += rel->r_addend;
      dynamic_symbol_p = elfNN_ia64_dynamic_symbol_p (h, info);

      switch (r_type)
	{
	case R_IA64_NONE:
	case R_IA64_LDXMOV:
	  continue;

	case R_IA64_IMM14:
	case R_IA64_IMM22:
	case R_IA64_IMM64:
	case R_IA64_DIR32MSB:
	case R_IA64_DIR32LSB:
	case R_IA64_DIR64MSB:
	case R_IA64_DIR64LSB:
	  /* Install a dynamic relocation for this reloc.  */
	  if ((dynamic_symbol_p || info->shared)
	      && (input_section->flags & SEC_ALLOC) != 0)
	    {
	      unsigned int dyn_r_type;
	      long dynindx;
	      bfd_vma addend;

	      BFD_ASSERT (srel != NULL);

	      /* If we don't need dynamic symbol lookup, find a
		 matching RELATIVE relocation.  */
	      dyn_r_type = r_type;
	      if (dynamic_symbol_p)
		{
		  dynindx = h->dynindx;
		  addend = rel->r_addend;
		  value = 0;
		}
	      else
		{
		  switch (r_type)
		    {
		    case R_IA64_DIR32MSB:
		      dyn_r_type = R_IA64_REL32MSB;
		      break;
		    case R_IA64_DIR32LSB:
		      dyn_r_type = R_IA64_REL32LSB;
		      break;
		    case R_IA64_DIR64MSB:
		      dyn_r_type = R_IA64_REL64MSB;
		      break;
		    case R_IA64_DIR64LSB:
		      dyn_r_type = R_IA64_REL64LSB;
		      break;

		    default:
		      /* We can't represent this without a dynamic symbol.
			 Adjust the relocation to be against an output
			 section symbol, which are always present in the
			 dynamic symbol table.  */
		      /* ??? People shouldn't be doing non-pic code in
			 shared libraries.  Hork.  */
		      (*_bfd_error_handler)
			(_("%s: linking non-pic code in a shared library"),
			 bfd_get_filename (input_bfd));
		      ret_val = false;
		      continue;
		    }
		  dynindx = 0;
		  addend = value;
		}

	      elfNN_ia64_install_dyn_reloc (output_bfd, info, input_section,
					    srel, rel->r_offset, dyn_r_type,
					    dynindx, addend);
	    }
	  /* FALLTHRU */

	case R_IA64_LTV32MSB:
	case R_IA64_LTV32LSB:
	case R_IA64_LTV64MSB:
	case R_IA64_LTV64LSB:
	  r = elfNN_ia64_install_value (output_bfd, hit_addr, value, r_type);
	  break;

	case R_IA64_GPREL22:
	case R_IA64_GPREL64I:
	case R_IA64_GPREL32MSB:
	case R_IA64_GPREL32LSB:
	case R_IA64_GPREL64MSB:
	case R_IA64_GPREL64LSB:
	  if (dynamic_symbol_p)
	    {
	      (*_bfd_error_handler)
		(_("%s: @gprel relocation against dynamic symbol %s"),
		 bfd_get_filename (input_bfd), h->root.root.string);
	      ret_val = false;
	      continue;
	    }
	  value -= gp_val;
	  r = elfNN_ia64_install_value (output_bfd, hit_addr, value, r_type);
	  break;

	case R_IA64_LTOFF22:
	case R_IA64_LTOFF22X:
	case R_IA64_LTOFF64I:
          dyn_i = get_dyn_sym_info (ia64_info, h, input_bfd, rel, false);
	  value = set_got_entry (input_bfd, info, dyn_i, (h ? h->dynindx : -1),
				 rel->r_addend, value, R_IA64_DIR64LSB);
	  value -= gp_val;
	  r = elfNN_ia64_install_value (output_bfd, hit_addr, value, r_type);
	  break;

	case R_IA64_PLTOFF22:
	case R_IA64_PLTOFF64I:
	case R_IA64_PLTOFF64MSB:
	case R_IA64_PLTOFF64LSB:
          dyn_i = get_dyn_sym_info (ia64_info, h, input_bfd, rel, false);
	  value = set_pltoff_entry (output_bfd, info, dyn_i, value, false);
	  value -= gp_val;
	  r = elfNN_ia64_install_value (output_bfd, hit_addr, value, r_type);
	  break;

	case R_IA64_FPTR64I:
	case R_IA64_FPTR32MSB:
	case R_IA64_FPTR32LSB:
	case R_IA64_FPTR64MSB:
	case R_IA64_FPTR64LSB:
          dyn_i = get_dyn_sym_info (ia64_info, h, input_bfd, rel, false);
	  if (dyn_i->want_fptr)
	    {
	      if (!undef_weak_ref)
		value = set_fptr_entry (output_bfd, info, dyn_i, value);
	    }
	  else
	    {
	      long dynindx;

	      /* Otherwise, we expect the dynamic linker to create
		 the entry.  */

	      if (h)
		{
		  if (h->dynindx != -1)
		    dynindx = h->dynindx;
		  else
		    dynindx = (_bfd_elf_link_lookup_local_dynindx
			       (info, h->root.u.def.section->owner,
				global_sym_index (h)));
		}
	      else
		{
		  dynindx = (_bfd_elf_link_lookup_local_dynindx
			     (info, input_bfd, r_symndx));
		}

	      elfNN_ia64_install_dyn_reloc (output_bfd, info, input_section,
					    srel, rel->r_offset, r_type,
					    dynindx, rel->r_addend);
	      value = 0;
	    }

	  r = elfNN_ia64_install_value (output_bfd, hit_addr, value, r_type);
	  break;

	case R_IA64_LTOFF_FPTR22:
	case R_IA64_LTOFF_FPTR64I:
	case R_IA64_LTOFF_FPTR64MSB:
	case R_IA64_LTOFF_FPTR64LSB:
	  {
	    long dynindx;

	    dyn_i = get_dyn_sym_info (ia64_info, h, input_bfd, rel, false);
	    if (dyn_i->want_fptr)
	      {
		BFD_ASSERT (h == NULL || h->dynindx == -1)
	        if (!undef_weak_ref)
	          value = set_fptr_entry (output_bfd, info, dyn_i, value);
		dynindx = -1;
	      }
	    else
	      {
	        /* Otherwise, we expect the dynamic linker to create
		   the entry.  */
	        if (h)
		  {
		    if (h->dynindx != -1)
		      dynindx = h->dynindx;
		    else
		      dynindx = (_bfd_elf_link_lookup_local_dynindx
				 (info, h->root.u.def.section->owner,
				  global_sym_index (h)));
		  }
		else
		  dynindx = (_bfd_elf_link_lookup_local_dynindx
			     (info, input_bfd, r_symndx));
		value = 0;
	      }

	    value = set_got_entry (output_bfd, info, dyn_i, dynindx,
				   rel->r_addend, value, R_IA64_FPTR64LSB);
	    value -= gp_val;
	    r = elfNN_ia64_install_value (output_bfd, hit_addr, value, r_type);
	  }
	  break;

	case R_IA64_PCREL32MSB:
	case R_IA64_PCREL32LSB:
	case R_IA64_PCREL64MSB:
	case R_IA64_PCREL64LSB:
	  /* Install a dynamic relocation for this reloc.  */
	  if (dynamic_symbol_p)
	    {
	      BFD_ASSERT (srel != NULL);

	      elfNN_ia64_install_dyn_reloc (output_bfd, info, input_section,
					    srel, rel->r_offset, r_type,
					    h->dynindx, rel->r_addend);
	    }
	  goto finish_pcrel;

	case R_IA64_PCREL21BI:
	case R_IA64_PCREL21F:
	case R_IA64_PCREL21M:
	  /* ??? These two are only used for speculation fixup code.
	     They should never be dynamic.  */
	  if (dynamic_symbol_p)
	    {
	      (*_bfd_error_handler)
		(_("%s: dynamic relocation against speculation fixup"),
		 bfd_get_filename (input_bfd));
	      ret_val = false;
	      continue;
	    }
	  if (undef_weak_ref)
	    {
	      (*_bfd_error_handler)
		(_("%s: speculation fixup against undefined weak symbol"),
		 bfd_get_filename (input_bfd));
	      ret_val = false;
	      continue;
	    }
	  goto finish_pcrel;

	case R_IA64_PCREL21B:
	case R_IA64_PCREL60B:
	  /* We should have created a PLT entry for any dynamic symbol.  */
	  dyn_i = NULL;
	  if (h)
	    dyn_i = get_dyn_sym_info (ia64_info, h, NULL, NULL, false);

	  if (dyn_i && dyn_i->want_plt2)
	    {
	      /* Should have caught this earlier.  */
	      BFD_ASSERT (rel->r_addend == 0);

	      value = (ia64_info->plt_sec->output_section->vma
		       + ia64_info->plt_sec->output_offset
		       + dyn_i->plt2_offset);
	    }
	  else
	    {
	      /* Since there's no PLT entry, Validate that this is
		 locally defined.  */
	      BFD_ASSERT (undef_weak_ref || sym_sec->output_section != NULL);

	      /* If the symbol is undef_weak, we shouldn't be trying
		 to call it.  There's every chance that we'd wind up
		 with an out-of-range fixup here.  Don't bother setting
		 any value at all.  */
	      if (undef_weak_ref)
		continue;
	    }
	  goto finish_pcrel;

	case R_IA64_PCREL22:
	case R_IA64_PCREL64I:
	finish_pcrel:
	  /* Make pc-relative.  */
	  value -= (input_section->output_section->vma
		    + input_section->output_offset
		    + rel->r_offset) & ~ (bfd_vma) 0x3;
	  r = elfNN_ia64_install_value (output_bfd, hit_addr, value, r_type);
	  break;

	case R_IA64_SEGREL32MSB:
	case R_IA64_SEGREL32LSB:
	case R_IA64_SEGREL64MSB:
	case R_IA64_SEGREL64LSB:
	  {
	    struct elf_segment_map *m;
	    Elf_Internal_Phdr *p;

	    /* Find the segment that contains the output_section.  */
	    for (m = elf_tdata (output_bfd)->segment_map,
		   p = elf_tdata (output_bfd)->phdr;
		 m != NULL;
		 m = m->next, p++)
	      {
		int i;
		for (i = m->count - 1; i >= 0; i--)
		  if (m->sections[i] == sym_sec->output_section)
		    break;
		if (i >= 0)
		  break;
	      }

	    if (m == NULL)
	      {
		/* If the input section was discarded from the output, then
		   do nothing.  */

		if (bfd_is_abs_section (sym_sec->output_section))
		  r = bfd_reloc_ok;
		else
		  r = bfd_reloc_notsupported;
	      }
	    else
	      {
		/* The VMA of the segment is the vaddr of the associated
		   program header.  */
		if (value > p->p_vaddr)
		  value -= p->p_vaddr;
		else
		  value = 0;
		r = elfNN_ia64_install_value (output_bfd, hit_addr, value,
					      r_type);
	      }
	    break;
	  }

	case R_IA64_SECREL32MSB:
	case R_IA64_SECREL32LSB:
	case R_IA64_SECREL64MSB:
	case R_IA64_SECREL64LSB:
	  /* Make output-section relative.  */
	  if (value > input_section->output_section->vma)
	    value -= input_section->output_section->vma;
	  else
	    value = 0;
	  r = elfNN_ia64_install_value (output_bfd, hit_addr, value, r_type);
	  break;

	case R_IA64_IPLTMSB:
	case R_IA64_IPLTLSB:
	  /* Install a dynamic relocation for this reloc.  */
	  if ((dynamic_symbol_p || info->shared)
	      && (input_section->flags & SEC_ALLOC) != 0)
	    {
	      BFD_ASSERT (srel != NULL);

	      /* If we don't need dynamic symbol lookup, install two
		 RELATIVE relocations.  */
	      if (! dynamic_symbol_p)
		{
		  unsigned int dyn_r_type;

		  if (r_type == R_IA64_IPLTMSB)
		    dyn_r_type = R_IA64_REL64MSB;
		  else
		    dyn_r_type = R_IA64_REL64LSB;

		  elfNN_ia64_install_dyn_reloc (output_bfd, info,
						input_section,
						srel, rel->r_offset,
						dyn_r_type, 0, value);
		  elfNN_ia64_install_dyn_reloc (output_bfd, info,
						input_section,
						srel, rel->r_offset + 8,
						dyn_r_type, 0, gp_val);
		}
	      else
		elfNN_ia64_install_dyn_reloc (output_bfd, info, input_section,
					      srel, rel->r_offset, r_type,
					      h->dynindx, rel->r_addend);
	    }

	  if (r_type == R_IA64_IPLTMSB)
	    r_type = R_IA64_DIR64MSB;
	  else
	    r_type = R_IA64_DIR64LSB;
	  elfNN_ia64_install_value (output_bfd, hit_addr, value, r_type);
	  r = elfNN_ia64_install_value (output_bfd, hit_addr + 8, gp_val,
					r_type);
	  break;

	default:
	  r = bfd_reloc_notsupported;
	  break;
	}

      switch (r)
	{
	case bfd_reloc_ok:
	  break;

	case bfd_reloc_undefined:
	  /* This can happen for global table relative relocs if
	     __gp is undefined.  This is a panic situation so we
	     don't try to continue.  */
	  (*info->callbacks->undefined_symbol)
	    (info, "__gp", input_bfd, input_section, rel->r_offset, 1);
	  return false;

	case bfd_reloc_notsupported:
	  {
	    const char *name;

	    if (h)
	      name = h->root.root.string;
	    else
	      {
		name = bfd_elf_string_from_elf_section (input_bfd,
							symtab_hdr->sh_link,
							sym->st_name);
		if (name == NULL)
		  return false;
		if (*name == '\0')
		  name = bfd_section_name (input_bfd, input_section);
	      }
	    if (!(*info->callbacks->warning) (info, _("unsupported reloc"),
					      name, input_bfd,
					      input_section, rel->r_offset))
	      return false;
	    ret_val = false;
	  }
	  break;

	case bfd_reloc_dangerous:
	case bfd_reloc_outofrange:
	case bfd_reloc_overflow:
	default:
	  {
	    const char *name;

	    if (h)
	      name = h->root.root.string;
	    else
	      {
		name = bfd_elf_string_from_elf_section (input_bfd,
							symtab_hdr->sh_link,
							sym->st_name);
		if (name == NULL)
		  return false;
		if (*name == '\0')
		  name = bfd_section_name (input_bfd, input_section);
	      }
	    if (!(*info->callbacks->reloc_overflow) (info, name,
						     howto->name, 0,
						     input_bfd,
						     input_section,
						     rel->r_offset))
	      return false;
	    ret_val = false;
	  }
	  break;
	}
    }

  return ret_val;
}

static boolean
elfNN_ia64_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  struct elfNN_ia64_link_hash_table *ia64_info;
  struct elfNN_ia64_dyn_sym_info *dyn_i;

  ia64_info = elfNN_ia64_hash_table (info);
  dyn_i = get_dyn_sym_info (ia64_info, h, NULL, NULL, false);

  /* Fill in the PLT data, if required.  */
  if (dyn_i && dyn_i->want_plt)
    {
      Elf_Internal_Rela outrel;
      bfd_byte *loc;
      asection *plt_sec;
      bfd_vma plt_addr, pltoff_addr, gp_val, index;
      ElfNN_External_Rela *rel;

      gp_val = _bfd_get_gp_value (output_bfd);

      /* Initialize the minimal PLT entry.  */

      index = (dyn_i->plt_offset - PLT_HEADER_SIZE) / PLT_MIN_ENTRY_SIZE;
      plt_sec = ia64_info->plt_sec;
      loc = plt_sec->contents + dyn_i->plt_offset;

      memcpy (loc, plt_min_entry, PLT_MIN_ENTRY_SIZE);
      elfNN_ia64_install_value (output_bfd, loc, index, R_IA64_IMM22);
      elfNN_ia64_install_value (output_bfd, loc+2, -dyn_i->plt_offset,
				R_IA64_PCREL21B);

      plt_addr = (plt_sec->output_section->vma
		  + plt_sec->output_offset
		  + dyn_i->plt_offset);
      pltoff_addr = set_pltoff_entry (output_bfd, info, dyn_i, plt_addr, true);

      /* Initialize the FULL PLT entry, if needed.  */
      if (dyn_i->want_plt2)
	{
	  loc = plt_sec->contents + dyn_i->plt2_offset;

	  memcpy (loc, plt_full_entry, PLT_FULL_ENTRY_SIZE);
	  elfNN_ia64_install_value (output_bfd, loc, pltoff_addr - gp_val,
				    R_IA64_IMM22);

	  /* Mark the symbol as undefined, rather than as defined in the
	     plt section.  Leave the value alone.  */
	  /* ??? We didn't redefine it in adjust_dynamic_symbol in the
	     first place.  But perhaps elflink.h did some for us.  */
	  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	    sym->st_shndx = SHN_UNDEF;
	}

      /* Create the dynamic relocation.  */
      outrel.r_offset = pltoff_addr;
      if (bfd_little_endian (output_bfd))
	outrel.r_info = ELFNN_R_INFO (h->dynindx, R_IA64_IPLTLSB);
      else
	outrel.r_info = ELFNN_R_INFO (h->dynindx, R_IA64_IPLTMSB);
      outrel.r_addend = 0;

      /* This is fun.  In the .IA_64.pltoff section, we've got entries
	 that correspond both to real PLT entries, and those that
	 happened to resolve to local symbols but need to be created
	 to satisfy @pltoff relocations.  The .rela.IA_64.pltoff
	 relocations for the real PLT should come at the end of the
	 section, so that they can be indexed by plt entry at runtime.

	 We emitted all of the relocations for the non-PLT @pltoff
	 entries during relocate_section.  So we can consider the
	 existing sec->reloc_count to be the base of the array of
	 PLT relocations.  */

      rel = (ElfNN_External_Rela *)ia64_info->rel_pltoff_sec->contents;
      rel += ia64_info->rel_pltoff_sec->reloc_count;

      bfd_elfNN_swap_reloca_out (output_bfd, &outrel, rel + index);
    }

  /* Mark some specially defined symbols as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0
      || strcmp (h->root.root.string, "_PROCEDURE_LINKAGE_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return true;
}

static boolean
elfNN_ia64_finish_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  struct elfNN_ia64_link_hash_table *ia64_info;
  bfd *dynobj;

  ia64_info = elfNN_ia64_hash_table (info);
  dynobj = ia64_info->root.dynobj;

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      ElfNN_External_Dyn *dyncon, *dynconend;
      asection *sdyn, *sgotplt;
      bfd_vma gp_val;

      sdyn = bfd_get_section_by_name (dynobj, ".dynamic");
      sgotplt = bfd_get_section_by_name (dynobj, ".got.plt");
      BFD_ASSERT (sdyn != NULL);
      dyncon = (ElfNN_External_Dyn *) sdyn->contents;
      dynconend = (ElfNN_External_Dyn *) (sdyn->contents + sdyn->_raw_size);

      gp_val = _bfd_get_gp_value (abfd);

      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;

	  bfd_elfNN_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:
	      dyn.d_un.d_ptr = gp_val;
	      break;

	    case DT_PLTRELSZ:
	      dyn.d_un.d_val = (ia64_info->minplt_entries
				* sizeof (ElfNN_External_Rela));
	      break;

	    case DT_JMPREL:
	      /* See the comment above in finish_dynamic_symbol.  */
	      dyn.d_un.d_ptr = (ia64_info->rel_pltoff_sec->output_section->vma
				+ ia64_info->rel_pltoff_sec->output_offset
				+ (ia64_info->rel_pltoff_sec->reloc_count
				   * sizeof (ElfNN_External_Rela)));
	      break;

	    case DT_IA_64_PLT_RESERVE:
	      dyn.d_un.d_ptr = (sgotplt->output_section->vma
				+ sgotplt->output_offset);
	      break;

	    case DT_RELASZ:
	      /* Do not have RELASZ include JMPREL.  This makes things
		 easier on ld.so.  This is not what the rest of BFD set up.  */
	      dyn.d_un.d_val -= (ia64_info->minplt_entries
				 * sizeof (ElfNN_External_Rela));
	      break;
	    }

	  bfd_elfNN_swap_dyn_out (abfd, &dyn, dyncon);
	}

      /* Initialize the PLT0 entry */
      if (ia64_info->plt_sec)
	{
	  bfd_byte *loc = ia64_info->plt_sec->contents;
	  bfd_vma pltres;

	  memcpy (loc, plt_header, PLT_HEADER_SIZE);

	  pltres = (sgotplt->output_section->vma
		    + sgotplt->output_offset
		    - gp_val);

	  elfNN_ia64_install_value (abfd, loc+1, pltres, R_IA64_GPREL22);
	}
    }

  return true;
}

/* ELF file flag handling: */

/* Function to keep IA-64 specific file flags.  */
static boolean
elfNN_ia64_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = true;
  return true;
}

/* Copy backend specific data from one object module to another */
static boolean
elfNN_ia64_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd, *obfd;
{
  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  BFD_ASSERT (!elf_flags_init (obfd)
	      || (elf_elfheader (obfd)->e_flags
		  == elf_elfheader (ibfd)->e_flags));

  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_flags_init (obfd) = true;
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */
static boolean
elfNN_ia64_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd, *obfd;
{
  flagword out_flags;
  flagword in_flags;
  boolean ok = true;

  /* Don't even pretend to support mixed-format linking.  */
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return false;

  in_flags  = elf_elfheader (ibfd)->e_flags;
  out_flags = elf_elfheader (obfd)->e_flags;

  if (! elf_flags_init (obfd))
    {
      elf_flags_init (obfd) = true;
      elf_elfheader (obfd)->e_flags = in_flags;

      if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
	  && bfd_get_arch_info (obfd)->the_default)
	{
	  return bfd_set_arch_mach (obfd, bfd_get_arch (ibfd),
				    bfd_get_mach (ibfd));
	}

      return true;
    }

  /* Check flag compatibility.  */
  if (in_flags == out_flags)
    return true;

  /* Output has EF_IA_64_REDUCEDFP set only if all inputs have it set.  */
  if (!(in_flags & EF_IA_64_REDUCEDFP) && (out_flags & EF_IA_64_REDUCEDFP))
    elf_elfheader (obfd)->e_flags &= ~EF_IA_64_REDUCEDFP;

  if ((in_flags & EF_IA_64_TRAPNIL) != (out_flags & EF_IA_64_TRAPNIL))
    {
      (*_bfd_error_handler)
	(_("%s: linking trap-on-NULL-dereference with non-trapping files"),
	 bfd_get_filename (ibfd));

      bfd_set_error (bfd_error_bad_value);
      ok = false;
    }
  if ((in_flags & EF_IA_64_BE) != (out_flags & EF_IA_64_BE))
    {
      (*_bfd_error_handler)
	(_("%s: linking big-endian files with little-endian files"),
	 bfd_get_filename (ibfd));

      bfd_set_error (bfd_error_bad_value);
      ok = false;
    }
  if ((in_flags & EF_IA_64_ABI64) != (out_flags & EF_IA_64_ABI64))
    {
      (*_bfd_error_handler)
	(_("%s: linking 64-bit files with 32-bit files"),
	 bfd_get_filename (ibfd));

      bfd_set_error (bfd_error_bad_value);
      ok = false;
    }
  if ((in_flags & EF_IA_64_CONS_GP) != (out_flags & EF_IA_64_CONS_GP))
    {
      (*_bfd_error_handler)
	(_("%s: linking constant-gp files with non-constant-gp files"),
	 bfd_get_filename (ibfd));

      bfd_set_error (bfd_error_bad_value);
      ok = false;
    }
  if ((in_flags & EF_IA_64_NOFUNCDESC_CONS_GP)
      != (out_flags & EF_IA_64_NOFUNCDESC_CONS_GP))
    {
      (*_bfd_error_handler)
	(_("%s: linking auto-pic files with non-auto-pic files"),
	 bfd_get_filename (ibfd));

      bfd_set_error (bfd_error_bad_value);
      ok = false;
    }

  return ok;
}

static boolean
elfNN_ia64_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE *file = (FILE *) ptr;
  flagword flags = elf_elfheader (abfd)->e_flags;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  fprintf (file, "private flags = %s%s%s%s%s%s%s%s\n",
	   (flags & EF_IA_64_TRAPNIL) ? "TRAPNIL, " : "",
	   (flags & EF_IA_64_EXT) ? "EXT, " : "",
	   (flags & EF_IA_64_BE) ? "BE, " : "LE, ",
	   (flags & EF_IA_64_REDUCEDFP) ? "REDUCEDFP, " : "",
	   (flags & EF_IA_64_CONS_GP) ? "CONS_GP, " : "",
	   (flags & EF_IA_64_NOFUNCDESC_CONS_GP) ? "NOFUNCDESC_CONS_GP, " : "",
	   (flags & EF_IA_64_ABSOLUTE) ? "ABSOLUTE, " : "",
	   (flags & EF_IA_64_ABI64) ? "ABI64" : "ABI32");

  _bfd_elf_print_private_bfd_data (abfd, ptr);
  return true;
}

#define TARGET_LITTLE_SYM		bfd_elfNN_ia64_little_vec
#define TARGET_LITTLE_NAME		"elfNN-ia64-little"
#define TARGET_BIG_SYM			bfd_elfNN_ia64_big_vec
#define TARGET_BIG_NAME			"elfNN-ia64-big"
#define ELF_ARCH			bfd_arch_ia64
#define ELF_MACHINE_CODE		EM_IA_64
#define ELF_MACHINE_ALT1		1999	/* EAS2.3 */
#define ELF_MACHINE_ALT2		1998	/* EAS2.2 */
#define ELF_MAXPAGESIZE			0x10000	/* 64KB */

#define elf_backend_section_from_shdr \
	elfNN_ia64_section_from_shdr
#define elf_backend_section_flags \
	elfNN_ia64_section_flags
#define elf_backend_fake_sections \
	elfNN_ia64_fake_sections
#define elf_backend_final_write_processing \
	elfNN_ia64_final_write_processing
#define elf_backend_add_symbol_hook \
	elfNN_ia64_add_symbol_hook
#define elf_backend_additional_program_headers \
	elfNN_ia64_additional_program_headers
#define elf_backend_modify_segment_map \
	elfNN_ia64_modify_segment_map
#define elf_info_to_howto \
	elfNN_ia64_info_to_howto

#define bfd_elfNN_bfd_reloc_type_lookup \
	elfNN_ia64_reloc_type_lookup
#define bfd_elfNN_bfd_is_local_label_name \
	elfNN_ia64_is_local_label_name
#define bfd_elfNN_bfd_relax_section \
	elfNN_ia64_relax_section

/* Stuff for the BFD linker: */
#define bfd_elfNN_bfd_link_hash_table_create \
	elfNN_ia64_hash_table_create
#define elf_backend_create_dynamic_sections \
	elfNN_ia64_create_dynamic_sections
#define elf_backend_check_relocs \
	elfNN_ia64_check_relocs
#define elf_backend_adjust_dynamic_symbol \
	elfNN_ia64_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
	elfNN_ia64_size_dynamic_sections
#define elf_backend_relocate_section \
	elfNN_ia64_relocate_section
#define elf_backend_finish_dynamic_symbol \
	elfNN_ia64_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
	elfNN_ia64_finish_dynamic_sections
#define bfd_elfNN_bfd_final_link \
	elfNN_ia64_final_link

#define bfd_elfNN_bfd_copy_private_bfd_data \
	elfNN_ia64_copy_private_bfd_data
#define bfd_elfNN_bfd_merge_private_bfd_data \
	elfNN_ia64_merge_private_bfd_data
#define bfd_elfNN_bfd_set_private_flags \
	elfNN_ia64_set_private_flags
#define bfd_elfNN_bfd_print_private_bfd_data \
	elfNN_ia64_print_private_bfd_data

#define elf_backend_plt_readonly	1
#define elf_backend_want_plt_sym	0
#define elf_backend_plt_alignment	5
#define elf_backend_got_header_size	0
#define elf_backend_plt_header_size	PLT_HEADER_SIZE
#define elf_backend_want_got_plt	1
#define elf_backend_may_use_rel_p	1
#define elf_backend_may_use_rela_p	1
#define elf_backend_default_use_rela_p	1
#define elf_backend_want_dynbss		0
#define elf_backend_copy_indirect_symbol elfNN_ia64_hash_copy_indirect
#define elf_backend_hide_symbol		elfNN_ia64_hash_hide_symbol

#include "elfNN-target.h"
