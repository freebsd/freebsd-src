/* SPARC-specific support for 32-bit ELF
   Copyright (C) 1993, 1994, 1995, 1996 Free Software Foundation, Inc.

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
#include "elf/sparc.h"

static reloc_howto_type *elf32_sparc_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void elf_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static boolean elf32_sparc_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static boolean elf32_sparc_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static boolean elf32_sparc_adjust_dynindx
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf32_sparc_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf32_sparc_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static boolean elf32_sparc_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static boolean elf32_sparc_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf32_sparc_merge_private_bfd_data PARAMS ((bfd *, bfd *));
static boolean elf32_sparc_object_p
  PARAMS ((bfd *));
static void elf32_sparc_final_write_processing
  PARAMS ((bfd *, boolean));

/* The howto table and associated functions.
   ??? elf64-sparc.c has its own copy for the moment to ease transition
   since some of the relocation values have changed.  At some point we'll
   want elf64-sparc.c to switch over and use this table.
   ??? Do we want to recognize (or flag as errors) some of the 64 bit entries
   if the target is elf32-sparc.
*/

static bfd_reloc_status_type sparc_elf_notsupported_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type sparc_elf_wdisp16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

reloc_howto_type _bfd_sparc_elf_howto_table[] = 
{
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_8,         0,0, 8,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_8",       false,0,0x000000ff,true),
  HOWTO(R_SPARC_16,        0,1,16,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_16",      false,0,0x0000ffff,true),
  HOWTO(R_SPARC_32,        0,2,32,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_32",      false,0,0xffffffff,true),
  HOWTO(R_SPARC_DISP8,     0,0, 8,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP8",   false,0,0x000000ff,true),
  HOWTO(R_SPARC_DISP16,    0,1,16,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP16",  false,0,0x0000ffff,true),
  HOWTO(R_SPARC_DISP32,    0,2,32,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP32",  false,0,0x00ffffff,true),
  HOWTO(R_SPARC_WDISP30,   2,2,30,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP30", false,0,0x3fffffff,true),
  HOWTO(R_SPARC_WDISP22,   2,2,22,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP22", false,0,0x003fffff,true),
  HOWTO(R_SPARC_HI22,     10,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HI22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_22,        0,2,22,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_22",      false,0,0x003fffff,true),
  HOWTO(R_SPARC_13,        0,2,13,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_13",      false,0,0x00001fff,true),
  HOWTO(R_SPARC_LO10,      0,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_LO10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT10,     0,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GOT10",   false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT13,     0,2,13,false,0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_GOT13",   false,0,0x00001fff,true),
  HOWTO(R_SPARC_GOT22,    10,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GOT22",   false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC10,      0,2,10,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_PC10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_PC22,     10,2,22,true, 0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_PC22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_WPLT30,    2,2,30,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WPLT30",  false,0,0x3fffffff,true),
  HOWTO(R_SPARC_COPY,      0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_COPY",    false,0,0x00000000,true),
  HOWTO(R_SPARC_GLOB_DAT,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GLOB_DAT",false,0,0x00000000,true),
  HOWTO(R_SPARC_JMP_SLOT,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_JMP_SLOT",false,0,0x00000000,true),
  HOWTO(R_SPARC_RELATIVE,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_RELATIVE",false,0,0x00000000,true),
  HOWTO(R_SPARC_UA32,      0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_UA32",    false,0,0x00000000,true),
  HOWTO(R_SPARC_PLT32,     0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PLT32",    false,0,0x00000000,true),
  HOWTO(R_SPARC_HIPLT22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HIPLT22",  false,0,0x00000000,true),
  HOWTO(R_SPARC_LOPLT10,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_LOPLT10",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT32,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT32",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT22",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT10,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT10",  false,0,0x00000000,true),
  HOWTO(R_SPARC_10,        0,2,10,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_10",      false,0,0x000003ff,true),
  HOWTO(R_SPARC_11,        0,2,11,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_11",      false,0,0x000007ff,true),
  /* ??? If we need to handle R_SPARC_64 then we need (figuratively)
     --enable-64-bit-bfd.  That causes objdump to print address as 64 bits
     which we really don't want on an elf32-sparc system.  There may be other
     consequences which we may not want (at least not until it's proven they're
     necessary) so for now these are only enabled ifdef BFD64.  */
#ifdef BFD64
  HOWTO(R_SPARC_64,        0,4,00,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_64",      false,0,~ (bfd_vma) 0, true),
  /* ??? These don't make sense except in 64 bit systems so they're disabled
     ifndef BFD64 too (for now).  */
  HOWTO(R_SPARC_OLO10,     0,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_OLO10",   false,0,0x000003ff,true),
  HOWTO(R_SPARC_HH22,     42,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HH22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_HM10,     32,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HM10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_LM22,     10,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_LM22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC_HH22,  42,2,22,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HH22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC_HM10,  32,2,10,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HM10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_PC_LM22,  10,2,22,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_LM22",    false,0,0x003fffff,true),
#else
  HOWTO(R_SPARC_64,      0,0,00,false,0,complain_overflow_dont,      sparc_elf_notsupported_reloc,  "R_SPARC_64",      false,0,0x00000000,true),
  HOWTO(R_SPARC_OLO10,      0,0,00,false,0,complain_overflow_dont,   sparc_elf_notsupported_reloc,  "R_SPARC_OLO10",   false,0,0x00000000,true),
  HOWTO(R_SPARC_HH22,      0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HH22",    false,0,0x00000000,true),
  HOWTO(R_SPARC_HM10,      0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HM10",    false,0,0x00000000,true),
  HOWTO(R_SPARC_LM22,      0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_LM22",    false,0,0x00000000,true),
  HOWTO(R_SPARC_PC_HH22,      0,0,00,false,0,complain_overflow_dont, sparc_elf_notsupported_reloc,  "R_SPARC_PC_HH22", false,0,0x00000000,true),
  HOWTO(R_SPARC_PC_HM10,      0,0,00,false,0,complain_overflow_dont, sparc_elf_notsupported_reloc,  "R_SPARC_PC_HM10", false,0,0x00000000,true),
  HOWTO(R_SPARC_PC_LM22,      0,0,00,false,0,complain_overflow_dont, sparc_elf_notsupported_reloc,  "R_SPARC_PC_LM22", false,0,0x00000000,true),
#endif
  HOWTO(R_SPARC_WDISP16,   2,2,16,true, 0,complain_overflow_signed,  sparc_elf_wdisp16_reloc,"R_SPARC_WDISP16", false,0,0x00000000,true),
  HOWTO(R_SPARC_WDISP19,   2,2,22,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP19", false,0,0x0007ffff,true),
  HOWTO(R_SPARC_GLOB_JMP,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GLOB_DAT",false,0,0x00000000,true),
  HOWTO(R_SPARC_7,         0,2, 7,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_7",       false,0,0x0000007f,true),
  HOWTO(R_SPARC_5,         0,2, 5,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_5",       false,0,0x0000001f,true),
  HOWTO(R_SPARC_6,         0,2, 6,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_6",       false,0,0x0000003f,true),
};

struct elf_reloc_map {
  unsigned char bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static CONST struct elf_reloc_map sparc_reloc_map[] =
{
  { BFD_RELOC_NONE, R_SPARC_NONE, },
  { BFD_RELOC_16, R_SPARC_16, },
  { BFD_RELOC_8, R_SPARC_8 },
  { BFD_RELOC_8_PCREL, R_SPARC_DISP8 },
  /* ??? This might cause us to need separate functions in elf{32,64}-sparc.c
     (we could still have just one table), but is this reloc ever used?  */
  { BFD_RELOC_CTOR, R_SPARC_32 }, /* @@ Assumes 32 bits.  */
  { BFD_RELOC_32, R_SPARC_32 },
  { BFD_RELOC_32_PCREL, R_SPARC_DISP32 },
  { BFD_RELOC_HI22, R_SPARC_HI22 },
  { BFD_RELOC_LO10, R_SPARC_LO10, },
  { BFD_RELOC_32_PCREL_S2, R_SPARC_WDISP30 },
  { BFD_RELOC_SPARC22, R_SPARC_22 },
  { BFD_RELOC_SPARC13, R_SPARC_13 },
  { BFD_RELOC_SPARC_GOT10, R_SPARC_GOT10 },
  { BFD_RELOC_SPARC_GOT13, R_SPARC_GOT13 },
  { BFD_RELOC_SPARC_GOT22, R_SPARC_GOT22 },
  { BFD_RELOC_SPARC_PC10, R_SPARC_PC10 },
  { BFD_RELOC_SPARC_PC22, R_SPARC_PC22 },
  { BFD_RELOC_SPARC_WPLT30, R_SPARC_WPLT30 },
  { BFD_RELOC_SPARC_COPY, R_SPARC_COPY },
  { BFD_RELOC_SPARC_GLOB_DAT, R_SPARC_GLOB_DAT },
  { BFD_RELOC_SPARC_JMP_SLOT, R_SPARC_JMP_SLOT },
  { BFD_RELOC_SPARC_RELATIVE, R_SPARC_RELATIVE },
  { BFD_RELOC_SPARC_WDISP22, R_SPARC_WDISP22 },
  /* ??? Doesn't dwarf use this?  */
/*{ BFD_RELOC_SPARC_UA32, R_SPARC_UA32 }, not used?? */
  {BFD_RELOC_SPARC_10, R_SPARC_10},
  {BFD_RELOC_SPARC_11, R_SPARC_11},
  {BFD_RELOC_SPARC_64, R_SPARC_64},
  {BFD_RELOC_SPARC_OLO10, R_SPARC_OLO10},
  {BFD_RELOC_SPARC_HH22, R_SPARC_HH22},
  {BFD_RELOC_SPARC_HM10, R_SPARC_HM10},
  {BFD_RELOC_SPARC_LM22, R_SPARC_LM22},
  {BFD_RELOC_SPARC_PC_HH22, R_SPARC_PC_HH22},
  {BFD_RELOC_SPARC_PC_HM10, R_SPARC_PC_HM10},
  {BFD_RELOC_SPARC_PC_LM22, R_SPARC_PC_LM22},
  {BFD_RELOC_SPARC_WDISP16, R_SPARC_WDISP16},
  {BFD_RELOC_SPARC_WDISP19, R_SPARC_WDISP19},
  {BFD_RELOC_SPARC_GLOB_JMP, R_SPARC_GLOB_JMP},
  {BFD_RELOC_SPARC_7, R_SPARC_7},
  {BFD_RELOC_SPARC_5, R_SPARC_5},
  {BFD_RELOC_SPARC_6, R_SPARC_6},
};

static reloc_howto_type *
elf32_sparc_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;
  for (i = 0; i < sizeof (sparc_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (sparc_reloc_map[i].bfd_reloc_val == code)
	return &_bfd_sparc_elf_howto_table[(int) sparc_reloc_map[i].elf_reloc_val];
    }
  return 0;
}

/* We need to use ELF32_R_TYPE so we have our own copy of this function,
   and elf64-sparc.c has its own copy.  */

static void
elf_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  BFD_ASSERT (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_SPARC_max);
  cache_ptr->howto = &_bfd_sparc_elf_howto_table[ELF32_R_TYPE(dst->r_info)];
}

/* For unsupported relocs.  */

static bfd_reloc_status_type
sparc_elf_notsupported_reloc (abfd,
			     reloc_entry,
			     symbol,
			     data,
			     input_section,
			     output_bfd,
			     error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  return bfd_reloc_notsupported;
}

/* Handle the WDISP16 reloc.  */

static bfd_reloc_status_type
sparc_elf_wdisp16_reloc (abfd,
			 reloc_entry,
			 symbol,
			 data,
			 input_section,
			 output_bfd,
			 error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_vma relocation;
  bfd_vma x;

  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != NULL)
    return bfd_reloc_continue;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  relocation = (symbol->value
		+ symbol->section->output_section->vma
		+ symbol->section->output_offset);
  relocation += reloc_entry->addend;
  relocation -=	(input_section->output_section->vma
		 + input_section->output_offset);
  relocation -= reloc_entry->address;

  x = bfd_get_32 (abfd, (char *) data + reloc_entry->address);
  x |= ((((relocation >> 2) & 0xc000) << 6)
	| ((relocation >> 2) & 0x3fff));
  bfd_put_32 (abfd, x, (char *) data + reloc_entry->address);

  if ((bfd_signed_vma) relocation < - 0x40000
      || (bfd_signed_vma) relocation > 0x3ffff)
    return bfd_reloc_overflow;
  else
    return bfd_reloc_ok;
}

/* Functions for the SPARC ELF linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/ld.so.1"

/* The nop opcode we use.  */

#define SPARC_NOP 0x01000000

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 12

/* The first four entries in a procedure linkage table are reserved,
   and the initial contents are unimportant (we zero them out).
   Subsequent entries look like this.  See the SVR4 ABI SPARC
   supplement to see how this works.  */

/* sethi %hi(.-.plt0),%g1.  We fill in the address later.  */
#define PLT_ENTRY_WORD0 0x03000000
/* b,a .plt0.  We fill in the offset later.  */
#define PLT_ENTRY_WORD1 0x30800000
/* nop.  */
#define PLT_ENTRY_WORD2 SPARC_NOP

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static boolean
elf32_sparc_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sgot;
  asection *srelgot;
  asection *sreloc;

  if (info->relocateable)
    return true;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_offsets = elf_local_got_offsets (abfd);

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
	case R_SPARC_GOT10:
	case R_SPARC_GOT13:
	case R_SPARC_GOT22:
	  /* This symbol requires a global offset table entry.  */

	  if (dynobj == NULL)
	    {
	      /* Create the .got section.  */
	      elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! _bfd_elf_create_got_section (dynobj, info))
		return false;
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
		      || ! bfd_set_section_flags (dynobj, srelgot,
						  (SEC_ALLOC
						   | SEC_LOAD
						   | SEC_HAS_CONTENTS
						   | SEC_IN_MEMORY
						   | SEC_READONLY))
		      || ! bfd_set_section_alignment (dynobj, srelgot, 2))
		    return false;
		}
	    }

	  if (h != NULL)
	    {
	      if (h->got_offset != (bfd_vma) -1)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      h->got_offset = sgot->_raw_size;

	      /* Make sure this symbol is output as a dynamic symbol.  */
	      if (h->dynindx == -1)
		{
		  if (! bfd_elf32_link_record_dynamic_symbol (info, h))
		    return false;
		}

	      srelgot->_raw_size += sizeof (Elf32_External_Rela);
	    }
	  else
	    {
	      /* This is a global offset table entry for a local
                 symbol.  */
	      if (local_got_offsets == NULL)
		{
		  size_t size;
		  register unsigned int i;

		  size = symtab_hdr->sh_info * sizeof (bfd_vma);
		  local_got_offsets = (bfd_vma *) bfd_alloc (abfd, size);
		  if (local_got_offsets == NULL)
		    return false;
		  elf_local_got_offsets (abfd) = local_got_offsets;
		  for (i = 0; i < symtab_hdr->sh_info; i++)
		    local_got_offsets[i] = (bfd_vma) -1;
		}
	      if (local_got_offsets[r_symndx] != (bfd_vma) -1)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      local_got_offsets[r_symndx] = sgot->_raw_size;

	      if (info->shared)
		{
		  /* If we are generating a shared object, we need to
                     output a R_SPARC_RELATIVE reloc so that the
                     dynamic linker can adjust this GOT entry.  */
		  srelgot->_raw_size += sizeof (Elf32_External_Rela);
		}
	    }

	  sgot->_raw_size += 4;

	  break;

	case R_SPARC_WPLT30:
	  /* This symbol requires a procedure linkage table entry.  We
             actually build the entry in adjust_dynamic_symbol,
             because this might be a case of linking PIC code without
             linking in any dynamic objects, in which case we don't
             need to generate a procedure linkage table after all.  */

	  if (h == NULL)
	    {
	      /* It does not make sense to have a procedure linkage
                 table entry for a local symbol.  */
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }

	  /* Make sure this symbol is output as a dynamic symbol.  */
	  if (h->dynindx == -1)
	    {
	      if (! bfd_elf32_link_record_dynamic_symbol (info, h))
		return false;
	    }

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;

	  break;

	case R_SPARC_PC10:
	case R_SPARC_PC22:
	  if (h != NULL
	      && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* Fall through.  */
	case R_SPARC_DISP8:
	case R_SPARC_DISP16:
	case R_SPARC_DISP32:
	case R_SPARC_WDISP30:
	case R_SPARC_WDISP22:
	case R_SPARC_WDISP19:
	case R_SPARC_WDISP16:
	  if (h == NULL)
	    break;
	  /* Fall through.  */
	case R_SPARC_8:
	case R_SPARC_16:
	case R_SPARC_32:
	case R_SPARC_HI22:
	case R_SPARC_22:
	case R_SPARC_13:
	case R_SPARC_LO10:
	case R_SPARC_UA32:
	  if (info->shared
	      && (sec->flags & SEC_ALLOC) != 0)
	    {
	      /* When creating a shared object, we must copy these
                 relocs into the output file.  We create a reloc
                 section in dynobj and make room for the reloc.  */
	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (abfd,
			   elf_elfheader (abfd)->e_shstrndx,
			   elf_section_data (sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return false;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (abfd, sec),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      sreloc = bfd_make_section (dynobj, name);
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (dynobj, sreloc,
						      (SEC_ALLOC
						       | SEC_LOAD
						       | SEC_HAS_CONTENTS
						       | SEC_IN_MEMORY
						       | SEC_READONLY))
			  || ! bfd_set_section_alignment (dynobj, sreloc, 2))
			return false;
		    }
		}

	      sreloc->_raw_size += sizeof (Elf32_External_Rela);
	    }

	  break;

	default:
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
elf32_sparc_adjust_dynamic_symbol (info, h)
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
     will fill in the contents of the procedure linkage table later
     (although we could actually do it here).  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (! elf_hash_table (info)->dynamic_sections_created)
	{
	  /* This case can occur if we saw a WPLT30 reloc in an input
             file, but none of the input files were dynamic objects.
             In such a case, we don't actually need to build a
             procedure linkage table, and we can just do a WDISP30
             reloc instead.  */
	  BFD_ASSERT ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0);
	  return true;
	}

      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);

      /* The first four entries in .plt are reserved.  */
      if (s->_raw_size == 0)
	s->_raw_size = 4 * PLT_ENTRY_SIZE;

      /* The procedure linkage table has a maximum size.  */
      if (s->_raw_size >= 0x400000)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      /* If this symbol is not defined in a regular file, and we are
	 not generating a shared library, then set the symbol to this
	 location in the .plt.  This is required to make function
	 pointers compare as equal between the normal executable and
	 the shared library.  */
      if (! info->shared
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = s->_raw_size;
	}

      h->plt_offset = s->_raw_size;

      /* Make room for this entry.  */
      s->_raw_size += PLT_ENTRY_SIZE;

      /* We also need to make an entry in the .rela.plt section.  */

      s = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (s != NULL);
      s->_raw_size += sizeof (Elf32_External_Rela);

      return true;
    }

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
     is not a function.  */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.  */
  if (info->shared)
    return true;

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

  /* If the symbol is currently defined in the .bss section of the
     dynamic object, then it is OK to simply initialize it to zero.
     If the symbol is in some other section, we must generate a
     R_SPARC_COPY reloc to tell the dynamic linker to copy the initial
     value out of the dynamic object and into the runtime process
     image.  We need to remember the offset into the .rel.bss section
     we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_LOAD) != 0)
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
      if (! bfd_set_section_alignment (dynobj, s, power_of_two))
	return false;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return true;
}

/* Set the sizes of the dynamic sections.  */

static boolean
elf32_sparc_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean reltext;
  boolean relplt;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}

      /* Make space for the trailing nop in .plt.  */
      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);
      if (s->_raw_size > 0)
	s->_raw_size += 4;
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

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  reltext = false;
  relplt = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      boolean strip;

      if ((s->flags & SEC_IN_MEMORY) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      strip = false;

      if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* If we don't need this section, strip it from the
		 output file.  This is to handle .rela.bss and
		 .rel.plt.  We must create it in
		 create_dynamic_sections, because it must be created
		 before the linker maps input sections to output
		 sections.  The linker does that before
		 adjust_dynamic_symbol is called, and it is that
		 function which decides whether anything needs to go
		 into these sections.  */
	      strip = true;
	    }
	  else
	    {
	      asection *target;

	      /* If this relocation section applies to a read only
		 section, then we probably need a DT_TEXTREL entry.  */
	      target = bfd_get_section_by_name (output_bfd, name + 5);
	      if (target != NULL
		  && (target->flags & SEC_READONLY) != 0)
		reltext = true;

	      if (strcmp (name, ".rela.plt") == 0)
		relplt = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strcmp (name, ".plt") != 0
	       && strcmp (name, ".got") != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	{
	  asection **spp;

	  for (spp = &s->output_section->owner->sections;
	       *spp != s->output_section;
	       spp = &(*spp)->next)
	    ;
	  *spp = s->output_section->next;
	  --s->output_section->owner->section_count;

	  continue;
	}

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_alloc (dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return false;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf32_sparc_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (! info->shared)
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_DEBUG, 0))
	    return false;
	}

      if (! bfd_elf32_add_dynamic_entry (info, DT_PLTGOT, 0))
	return false;

      if (relplt)
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_PLTRELSZ, 0)
	      || ! bfd_elf32_add_dynamic_entry (info, DT_PLTREL, DT_RELA)
	      || ! bfd_elf32_add_dynamic_entry (info, DT_JMPREL, 0))
	    return false;
	}

      if (! bfd_elf32_add_dynamic_entry (info, DT_RELA, 0)
	  || ! bfd_elf32_add_dynamic_entry (info, DT_RELASZ, 0)
	  || ! bfd_elf32_add_dynamic_entry (info, DT_RELAENT,
					    sizeof (Elf32_External_Rela)))
	return false;

      if (reltext)
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_TEXTREL, 0))
	    return false;
	}
    }

  /* If we are generating a shared library, we generate a section
     symbol for each output section.  These are local symbols, which
     means that they must come first in the dynamic symbol table.
     That means we must increment the dynamic symbol index of every
     other dynamic symbol.  */
  if (info->shared)
    {
      int c, i;

      c = bfd_count_sections (output_bfd);
      elf_link_hash_traverse (elf_hash_table (info),
			      elf32_sparc_adjust_dynindx,
			      (PTR) &c);
      elf_hash_table (info)->dynsymcount += c;

      for (i = 1, s = output_bfd->sections; s != NULL; s = s->next, i++)
	{
	  elf_section_data (s)->dynindx = i;
	  /* These symbols will have no names, so we don't need to
             fiddle with dynstr_index.  */
	}
    }

  return true;
}

/* Increment the index of a dynamic symbol by a given amount.  Called
   via elf_link_hash_traverse.  */

static boolean
elf32_sparc_adjust_dynindx (h, cparg)
     struct elf_link_hash_entry *h;
     PTR cparg;
{
  int *cp = (int *) cparg;

  if (h->dynindx != -1)
    h->dynindx += *cp;
  return true;
}

/* Relocate a SPARC ELF section.  */

static boolean
elf32_sparc_relocate_section (output_bfd, info, input_bfd, input_section,
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
      bfd_reloc_status_type r;

      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_SPARC_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = _bfd_sparc_elf_howto_table + r_type;

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
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      if ((r_type == R_SPARC_WPLT30
		   && h->plt_offset != (bfd_vma) -1)
		  || ((r_type == R_SPARC_GOT10
		       || r_type == R_SPARC_GOT13
		       || r_type == R_SPARC_GOT22)
		      && elf_hash_table (info)->dynamic_sections_created
		      && (! info->shared
			  || ! info->symbolic
			  || (h->elf_link_hash_flags
			      & ELF_LINK_HASH_DEF_REGULAR) == 0))
		  || (info->shared
		      && (! info->symbolic
			  || (h->elf_link_hash_flags
			      & ELF_LINK_HASH_DEF_REGULAR) == 0)
		      && (input_section->flags & SEC_ALLOC) != 0
		      && (r_type == R_SPARC_8
			  || r_type == R_SPARC_16
			  || r_type == R_SPARC_32
			  || r_type == R_SPARC_DISP8
			  || r_type == R_SPARC_DISP16
			  || r_type == R_SPARC_DISP32
			  || r_type == R_SPARC_WDISP30
			  || r_type == R_SPARC_WDISP22
			  || r_type == R_SPARC_WDISP19
			  || r_type == R_SPARC_WDISP16
			  || r_type == R_SPARC_HI22
			  || r_type == R_SPARC_22
			  || r_type == R_SPARC_13
			  || r_type == R_SPARC_LO10
			  || r_type == R_SPARC_UA32
			  || ((r_type == R_SPARC_PC10
			       || r_type == R_SPARC_PC22)
			      && strcmp (h->root.root.string,
					 "_GLOBAL_OFFSET_TABLE_") != 0))))
		{
		  /* In these cases, we don't need the relocation
                     value.  We check specially because in some
                     obscure cases sec->output_section will be NULL.  */
		  relocation = 0;
		}
	      else
		relocation = (h->root.u.def.value
			      + sec->output_section->vma
			      + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else if (info->shared && !info->symbolic)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset)))
		return false;
	      relocation = 0;
	    }
	}

      switch (r_type)
	{
	case R_SPARC_GOT10:
	case R_SPARC_GOT13:
	case R_SPARC_GOT22:
	  /* Relocation is to the entry for this symbol in the global
             offset table.  */
	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  if (h != NULL)
	    {
	      bfd_vma off;

	      off = h->got_offset;
	      BFD_ASSERT (off != (bfd_vma) -1);

	      if (! elf_hash_table (info)->dynamic_sections_created
		  || (info->shared
		      && info->symbolic
		      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
		{
		  /* This is actually a static link, or it is a
                     -Bsymbolic link and the symbol is defined
                     locally.  We must initialize this entry in the
                     global offset table.  Since the offset must
                     always be a multiple of 4, we use the least
                     significant bit to record whether we have
                     initialized it already.

		     When doing a dynamic link, we create a .rela.got
		     relocation entry to initialize the value.  This
		     is done in the finish_dynamic_symbol routine.  */
		  if ((off & 1) != 0)
		    off &= ~1;
		  else
		    {
		      bfd_put_32 (output_bfd, relocation,
				  sgot->contents + off);
		      h->got_offset |= 1;
		    }
		}

	      relocation = sgot->output_offset + off;
	    }
	  else
	    {
	      bfd_vma off;

	      BFD_ASSERT (local_got_offsets != NULL
			  && local_got_offsets[r_symndx] != (bfd_vma) -1);

	      off = local_got_offsets[r_symndx];

	      /* The offset must always be a multiple of 4.  We use
		 the least significant bit to record whether we have
		 already processed this entry.  */
	      if ((off & 1) != 0)
		off &= ~1;
	      else
		{
		  bfd_put_32 (output_bfd, relocation, sgot->contents + off);

		  if (info->shared)
		    {
		      asection *srelgot;
		      Elf_Internal_Rela outrel;

		      /* We need to generate a R_SPARC_RELATIVE reloc
			 for the dynamic linker.  */
		      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
		      BFD_ASSERT (srelgot != NULL);

		      outrel.r_offset = (sgot->output_section->vma
					 + sgot->output_offset
					 + off);
		      outrel.r_info = ELF32_R_INFO (0, R_SPARC_RELATIVE);
		      outrel.r_addend = 0;
		      bfd_elf32_swap_reloca_out (output_bfd, &outrel,
						 (((Elf32_External_Rela *)
						   srelgot->contents)
						  + srelgot->reloc_count));
		      ++srelgot->reloc_count;
		    }

		  local_got_offsets[r_symndx] |= 1;
		}

	      relocation = sgot->output_offset + off;
	    }

	  break;

	case R_SPARC_WPLT30:
	  /* Relocation is to the entry for this symbol in the
             procedure linkage table.  */
	  BFD_ASSERT (h != NULL);

	  if (h->plt_offset == (bfd_vma) -1)
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
			+ h->plt_offset);
	  break;

	case R_SPARC_PC10:
	case R_SPARC_PC22:
	  if (h != NULL
	      && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* Fall through.  */
	case R_SPARC_DISP8:
	case R_SPARC_DISP16:
	case R_SPARC_DISP32:
	case R_SPARC_WDISP30:
	case R_SPARC_WDISP22:
	case R_SPARC_WDISP19:
	case R_SPARC_WDISP16:
	  if (h == NULL)
	    break;
	  /* Fall through.  */
	case R_SPARC_8:
	case R_SPARC_16:
	case R_SPARC_32:
	case R_SPARC_HI22:
	case R_SPARC_22:
	case R_SPARC_13:
	case R_SPARC_LO10:
	case R_SPARC_UA32:
	  if (info->shared
	      && (input_section->flags & SEC_ALLOC) != 0)
	    {
	      Elf_Internal_Rela outrel;

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
		    return false;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (input_bfd,
							       input_section),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  BFD_ASSERT (sreloc != NULL);
		}

	      outrel.r_offset = (rel->r_offset
				 + input_section->output_section->vma
				 + input_section->output_offset);
	      if (h != NULL
		  && (! info->symbolic
		      || (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0))
		{
		  BFD_ASSERT (h->dynindx != -1);
		  outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = rel->r_addend;
		}
	      else
		{
		  if (r_type == R_SPARC_32)
		    {
		      outrel.r_info = ELF32_R_INFO (0, R_SPARC_RELATIVE);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		  else
		    {
		      long indx;

		      if (h == NULL)
			sec = local_sections[r_symndx];
		      else
			{
			  BFD_ASSERT (h->root.type == bfd_link_hash_defined
				      || (h->root.type
					  == bfd_link_hash_defweak));
			  sec = h->root.u.def.section;
			}
		      if (sec != NULL && bfd_is_abs_section (sec))
			indx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return false;
			}
		      else
			{
			  asection *osec;

			  osec = sec->output_section;
			  indx = elf_section_data (osec)->dynindx;
			  if (indx == 0)
			    abort ();
			}

		      outrel.r_info = ELF32_R_INFO (indx, r_type);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		}

	      bfd_elf32_swap_reloca_out (output_bfd, &outrel,
					 (((Elf32_External_Rela *)
					   sreloc->contents)
					  + sreloc->reloc_count));
	      ++sreloc->reloc_count;

	      /* This reloc will be computed at runtime, so there's no
                 need to do anything now.  */
	      continue;
	    }

	default:
	  break;
	}		

      if (r_type != R_SPARC_WDISP16)
	r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				      contents, rel->r_offset,
				      relocation, rel->r_addend);
      else
	{
	  bfd_vma x;

	  relocation += rel->r_addend;
	  relocation -= (input_section->output_section->vma
			 + input_section->output_offset);
	  relocation -= rel->r_offset;

	  x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	  x |= ((((relocation >> 2) & 0xc000) << 6)
		| ((relocation >> 2) & 0x3fff));
	  bfd_put_32 (input_bfd, x, contents + rel->r_offset);

	  if ((bfd_signed_vma) relocation < - 0x40000
	      || (bfd_signed_vma) relocation > 0x3ffff)
	    r = bfd_reloc_overflow;
	  else
	    r = bfd_reloc_ok;
	}

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
		  name = h->root.root.string;
		else
		  {
		    name = bfd_elf_string_from_elf_section (input_bfd,
							    symtab_hdr->sh_link,
							    sym->st_name);
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
	    }
	}
    }

  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
elf32_sparc_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;

  dynobj = elf_hash_table (info)->dynobj;

  if (h->plt_offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *srela;
      Elf_Internal_Rela rela;

      /* This symbol has an entry in the procedure linkage table.  Set
         it up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = bfd_get_section_by_name (dynobj, ".plt");
      srela = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (splt != NULL && srela != NULL);

      /* Fill in the entry in the procedure linkage table.  */
      bfd_put_32 (output_bfd,
		  PLT_ENTRY_WORD0 + h->plt_offset,
		  splt->contents + h->plt_offset);
      bfd_put_32 (output_bfd,
		  (PLT_ENTRY_WORD1
		   + (((- (h->plt_offset + 4)) >> 2) & 0x3fffff)),
		  splt->contents + h->plt_offset + 4);
      bfd_put_32 (output_bfd, PLT_ENTRY_WORD2,
		  splt->contents + h->plt_offset + 8);

      /* Fill in the entry in the .rela.plt section.  */
      rela.r_offset = (splt->output_section->vma
		       + splt->output_offset
		       + h->plt_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_SPARC_JMP_SLOT);
      rela.r_addend = 0;
      bfd_elf32_swap_reloca_out (output_bfd, &rela,
				 ((Elf32_External_Rela *) srela->contents
				  + h->plt_offset / PLT_ENTRY_SIZE - 4));

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	}
    }

  if (h->got_offset != (bfd_vma) -1)
    {
      asection *sgot;
      asection *srela;
      Elf_Internal_Rela rela;

      /* This symbol has an entry in the global offset table.  Set it
         up.  */

      BFD_ASSERT (h->dynindx != -1);

      sgot = bfd_get_section_by_name (dynobj, ".got");
      srela = bfd_get_section_by_name (dynobj, ".rela.got");
      BFD_ASSERT (sgot != NULL && srela != NULL);

      rela.r_offset = (sgot->output_section->vma
		       + sgot->output_offset
		       + (h->got_offset &~ 1));

      /* If this is a -Bsymbolic link, and the symbol is defined
	 locally, we just want to emit a RELATIVE reloc.  The entry in
	 the global offset table will already have been initialized in
	 the relocate_section function.  */
      if (info->shared
	  && info->symbolic
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	rela.r_info = ELF32_R_INFO (0, R_SPARC_RELATIVE);
      else
	{
	  bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + h->got_offset);
	  rela.r_info = ELF32_R_INFO (h->dynindx, R_SPARC_GLOB_DAT);
	}

      rela.r_addend = 0;
      bfd_elf32_swap_reloca_out (output_bfd, &rela,
				 ((Elf32_External_Rela *) srela->contents
				  + srela->reloc_count));
      ++srela->reloc_count;
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      asection *s;
      Elf_Internal_Rela rela;

      /* This symbols needs a copy reloc.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1);

      s = bfd_get_section_by_name (h->root.u.def.section->owner,
				   ".rela.bss");
      BFD_ASSERT (s != NULL);

      rela.r_offset = (h->root.u.def.value
		       + h->root.u.def.section->output_section->vma
		       + h->root.u.def.section->output_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_SPARC_COPY);
      rela.r_addend = 0;
      bfd_elf32_swap_reloca_out (output_bfd, &rela,
				 ((Elf32_External_Rela *) s->contents
				  + s->reloc_count));
      ++s->reloc_count;
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
elf32_sparc_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;
  asection *sgot;

  dynobj = elf_hash_table (info)->dynobj;

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
	  boolean size;

	  bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:   name = ".plt"; size = false; break;
	    case DT_PLTRELSZ: name = ".rela.plt"; size = true; break;
	    case DT_JMPREL:   name = ".rela.plt"; size = false; break;
	    default:	  name = NULL; size = false; break;
	    }

	  if (name != NULL)
	    {
	      asection *s;

	      s = bfd_get_section_by_name (output_bfd, name);
	      if (s == NULL)
		dyn.d_un.d_val = 0;
	      else
		{
		  if (! size)
		    dyn.d_un.d_ptr = s->vma;
		  else
		    {
		      if (s->_cooked_size != 0)
			dyn.d_un.d_val = s->_cooked_size;
		      else
			dyn.d_un.d_val = s->_raw_size;
		    }
		}
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	    }
	}

      /* Clear the first four entries in the procedure linkage table,
	 and put a nop in the last four bytes.  */
      if (splt->_raw_size > 0)
	{
	  memset (splt->contents, 0, 4 * PLT_ENTRY_SIZE);
	  bfd_put_32 (output_bfd, SPARC_NOP,
		      splt->contents + splt->_raw_size - 4);
	}

      elf_section_data (splt->output_section)->this_hdr.sh_entsize =
	PLT_ENTRY_SIZE;
    }

  /* Set the first entry in the global offset table to the address of
     the dynamic section.  */
  sgot = bfd_get_section_by_name (dynobj, ".got");
  BFD_ASSERT (sgot != NULL);
  if (sgot->_raw_size > 0)
    {
      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);
    }

  elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;

  if (info->shared)
    {
      asection *sdynsym;
      asection *s;
      Elf_Internal_Sym sym;

      /* Set up the section symbols for the output sections.  */

      sdynsym = bfd_get_section_by_name (dynobj, ".dynsym");
      BFD_ASSERT (sdynsym != NULL);

      sym.st_size = 0;
      sym.st_name = 0;
      sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
      sym.st_other = 0;

      for (s = output_bfd->sections; s != NULL; s = s->next)
	{
	  int indx;

	  sym.st_value = s->vma;

	  indx = elf_section_data (s)->this_idx;
	  BFD_ASSERT (indx > 0);
	  sym.st_shndx = indx;

	  bfd_elf32_swap_symbol_out (output_bfd, &sym,
				     (PTR) (((Elf32_External_Sym *)
					     sdynsym->contents)
					    + elf_section_data (s)->dynindx));
	}

      /* Set the sh_info field of the output .dynsym section to the
         index of the first global symbol.  */
      elf_section_data (sdynsym->output_section)->this_hdr.sh_info =
	bfd_count_sections (output_bfd) + 1;
    }

  return true;
}

/* Functions for dealing with the e_flags field.

   We don't define set_private_flags or copy_private_bfd_data because
   the only currently defined values are based on the bfd mach number,
   so we use the latter instead and defer setting e_flags until the
   file is written out.  */

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static boolean
elf32_sparc_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  boolean error;

  /* This function is selected based on the input vector.  We only
     want to copy information over if the output BFD also uses Elf
     format.  */
  if (bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  error = false;

#if 0
  /* ??? The native linker doesn't do this so we can't (otherwise gcc would
     have to know which linker is being used).  Instead, the native linker
     bumps up the architecture level when it has to.  However, I still think
     warnings like these are good, so it would be nice to have them turned on
     by some option.  */

  /* If the output machine is normal sparc, we can't allow v9 input files.  */
  if (bfd_get_mach (obfd) == bfd_mach_sparc
      && (bfd_get_mach (ibfd) == bfd_mach_sparc_v8plus
	  || bfd_get_mach (ibfd) == bfd_mach_sparc_v8plusa))
    {
      error = true;
      (*_bfd_error_handler)
	("%s: compiled for a v8plus system and target is v8",
	 bfd_get_filename (ibfd));
    }
  /* If the output machine is v9, we can't allow v9+vis input files.  */
  if (bfd_get_mach (obfd) == bfd_mach_sparc_v8plus
      && bfd_get_mach (ibfd) == bfd_mach_sparc_v8plusa)
    {
      error = true;
      (*_bfd_error_handler)
	("%s: compiled for a v8plusa system and target is v8plus",
	 bfd_get_filename (ibfd));
    }
#else
  if (bfd_get_mach (ibfd) >= bfd_mach_sparc_v9)
    {
      error = true;
      (*_bfd_error_handler)
	("%s: compiled for a 64 bit system and target is 32 bit",
	 bfd_get_filename (ibfd));
    }
  else if (bfd_get_mach (obfd) < bfd_get_mach (ibfd))
    bfd_set_arch_mach (obfd, bfd_arch_sparc, bfd_get_mach (ibfd));
#endif

  if (error)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  return true;
}

/* Set the right machine number.  */

static boolean
elf32_sparc_object_p (abfd)
     bfd *abfd;
{
  if (elf_elfheader (abfd)->e_machine == EM_SPARC32PLUS)
    {
      if (elf_elfheader (abfd)->e_flags & EF_SPARC_SUN_US1)
	return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
					  bfd_mach_sparc_v8plusa);
      else if (elf_elfheader (abfd)->e_flags & EF_SPARC_32PLUS)
	return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
					  bfd_mach_sparc_v8plus);
      else
	return false;
    }
  else
    return bfd_default_set_arch_mach (abfd, bfd_arch_sparc, bfd_mach_sparc);
}

/* The final processing done just before writing out the object file.
   We need to set the e_machine field appropriately.  */

static void
elf32_sparc_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker;
{
  switch (bfd_get_mach (abfd))
    {
    case bfd_mach_sparc :
      break; /* nothing to do */
    case bfd_mach_sparc_v8plus :
      elf_elfheader (abfd)->e_machine = EM_SPARC32PLUS;
      elf_elfheader (abfd)->e_flags &=~ EF_SPARC_32PLUS_MASK;
      elf_elfheader (abfd)->e_flags |= EF_SPARC_32PLUS;
      break;
    case bfd_mach_sparc_v8plusa :
      elf_elfheader (abfd)->e_machine = EM_SPARC32PLUS;
      elf_elfheader (abfd)->e_flags &=~ EF_SPARC_32PLUS_MASK;
      elf_elfheader (abfd)->e_flags |= EF_SPARC_32PLUS | EF_SPARC_SUN_US1;
      break;
    default :
      abort ();
    }
}

#define TARGET_BIG_SYM	bfd_elf32_sparc_vec
#define TARGET_BIG_NAME	"elf32-sparc"
#define ELF_ARCH	bfd_arch_sparc
#define ELF_MACHINE_CODE EM_SPARC
#define ELF_MACHINE_ALT1 EM_SPARC32PLUS
#define ELF_MAXPAGESIZE 0x10000

#define bfd_elf32_bfd_reloc_type_lookup	elf32_sparc_reloc_type_lookup
#define elf_backend_create_dynamic_sections \
					_bfd_elf_create_dynamic_sections
#define elf_backend_check_relocs	elf32_sparc_check_relocs
#define elf_backend_adjust_dynamic_symbol \
					elf32_sparc_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
					elf32_sparc_size_dynamic_sections
#define elf_backend_relocate_section	elf32_sparc_relocate_section
#define elf_backend_finish_dynamic_symbol \
					elf32_sparc_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					elf32_sparc_finish_dynamic_sections
#define bfd_elf32_bfd_merge_private_bfd_data \
					elf32_sparc_merge_private_bfd_data
#define elf_backend_object_p		elf32_sparc_object_p
#define elf_backend_final_write_processing \
					elf32_sparc_final_write_processing
#define elf_backend_want_got_plt 0
#define elf_backend_plt_readonly 0
#define elf_backend_want_plt_sym 1

#include "elf32-target.h"
