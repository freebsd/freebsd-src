/* SPARC-specific support for 32-bit ELF
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004 Free Software Foundation, Inc.

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
#include "opcode/sparc.h"

static reloc_howto_type *elf32_sparc_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void elf32_sparc_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static bfd_boolean elf32_sparc_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_boolean elf32_sparc_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static bfd_boolean allocate_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean readonly_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean elf32_sparc_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean elf32_sparc_new_section_hook
  PARAMS ((bfd *, asection *));
static bfd_boolean elf32_sparc_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, bfd_boolean *));
static bfd_vma dtpoff_base
  PARAMS ((struct bfd_link_info *));
static bfd_vma tpoff
  PARAMS ((struct bfd_link_info *, bfd_vma));
static bfd_boolean elf32_sparc_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static bfd_boolean elf32_sparc_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static bfd_boolean elf32_sparc_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean elf32_sparc_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static struct bfd_hash_entry *link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct bfd_link_hash_table *elf32_sparc_link_hash_table_create
  PARAMS ((bfd *));
static bfd_boolean create_got_section
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean elf32_sparc_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static void elf32_sparc_copy_indirect_symbol
  PARAMS ((const struct elf_backend_data *, struct elf_link_hash_entry *,
	  struct elf_link_hash_entry *));
static int elf32_sparc_tls_transition
  PARAMS ((struct bfd_link_info *, bfd *, int, int));

static bfd_boolean elf32_sparc_mkobject
  PARAMS ((bfd *));
static bfd_boolean elf32_sparc_object_p
  PARAMS ((bfd *));
static void elf32_sparc_final_write_processing
  PARAMS ((bfd *, bfd_boolean));
static enum elf_reloc_type_class elf32_sparc_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));
static asection * elf32_sparc_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static bfd_boolean elf32_sparc_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

/* The relocation "howto" table.  */

static bfd_reloc_status_type sparc_elf_notsupported_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type sparc_elf_wdisp16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type sparc_elf_hix22_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type sparc_elf_lox10_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

reloc_howto_type _bfd_sparc_elf_howto_table[] =
{
  HOWTO(R_SPARC_NONE,      0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_8,         0,0, 8,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_8",       FALSE,0,0x000000ff,TRUE),
  HOWTO(R_SPARC_16,        0,1,16,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_16",      FALSE,0,0x0000ffff,TRUE),
  HOWTO(R_SPARC_32,        0,2,32,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_32",      FALSE,0,0xffffffff,TRUE),
  HOWTO(R_SPARC_DISP8,     0,0, 8,TRUE, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP8",   FALSE,0,0x000000ff,TRUE),
  HOWTO(R_SPARC_DISP16,    0,1,16,TRUE, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP16",  FALSE,0,0x0000ffff,TRUE),
  HOWTO(R_SPARC_DISP32,    0,2,32,TRUE, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP32",  FALSE,0,0xffffffff,TRUE),
  HOWTO(R_SPARC_WDISP30,   2,2,30,TRUE, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP30", FALSE,0,0x3fffffff,TRUE),
  HOWTO(R_SPARC_WDISP22,   2,2,22,TRUE, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP22", FALSE,0,0x003fffff,TRUE),
  HOWTO(R_SPARC_HI22,     10,2,22,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HI22",    FALSE,0,0x003fffff,TRUE),
  HOWTO(R_SPARC_22,        0,2,22,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_22",      FALSE,0,0x003fffff,TRUE),
  HOWTO(R_SPARC_13,        0,2,13,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_13",      FALSE,0,0x00001fff,TRUE),
  HOWTO(R_SPARC_LO10,      0,2,10,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_LO10",    FALSE,0,0x000003ff,TRUE),
  HOWTO(R_SPARC_GOT10,     0,2,10,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GOT10",   FALSE,0,0x000003ff,TRUE),
  HOWTO(R_SPARC_GOT13,     0,2,13,FALSE,0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_GOT13",   FALSE,0,0x00001fff,TRUE),
  HOWTO(R_SPARC_GOT22,    10,2,22,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GOT22",   FALSE,0,0x003fffff,TRUE),
  HOWTO(R_SPARC_PC10,      0,2,10,TRUE, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_PC10",    FALSE,0,0x000003ff,TRUE),
  HOWTO(R_SPARC_PC22,     10,2,22,TRUE, 0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_PC22",    FALSE,0,0x003fffff,TRUE),
  HOWTO(R_SPARC_WPLT30,    2,2,30,TRUE, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WPLT30",  FALSE,0,0x3fffffff,TRUE),
  HOWTO(R_SPARC_COPY,      0,0,00,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_COPY",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_GLOB_DAT,  0,0,00,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GLOB_DAT",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_JMP_SLOT,  0,0,00,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_JMP_SLOT",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_RELATIVE,  0,0,00,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_RELATIVE",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_UA32,      0,2,32,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_UA32",    FALSE,0,0xffffffff,TRUE),
  HOWTO(R_SPARC_PLT32,     0,0,00,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_PLT32",   FALSE,0,0xffffffff,TRUE),
  HOWTO(R_SPARC_HIPLT22,   0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HIPLT22",  FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_LOPLT10,   0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_LOPLT10",  FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_PCPLT32,   0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT32",  FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_PCPLT22,   0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT22",  FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_PCPLT10,   0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PCPLT10",  FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_10,        0,2,10,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_10",      FALSE,0,0x000003ff,TRUE),
  HOWTO(R_SPARC_11,        0,2,11,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_11",      FALSE,0,0x000007ff,TRUE),
  /* These are for sparc64 in a 64 bit environment.
     Values need to be here because the table is indexed by reloc number.  */
  HOWTO(R_SPARC_64,        0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_64",      FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_OLO10,     0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_OLO10",   FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_HH22,      0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HH22",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_HM10,      0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_HM10",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_LM22,      0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_LM22",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_PC_HH22,   0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PC_HH22", FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_PC_HM10,   0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PC_HM10", FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_PC_LM22,   0,0,00,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_PC_LM22", FALSE,0,0x00000000,TRUE),
  /* End sparc64 in 64 bit environment values.
     The following are for sparc64 in a 32 bit environment.  */
  HOWTO(R_SPARC_WDISP16,   2,2,16,TRUE, 0,complain_overflow_signed,  sparc_elf_wdisp16_reloc,"R_SPARC_WDISP16", FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_WDISP19,   2,2,19,TRUE, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP19", FALSE,0,0x0007ffff,TRUE),
  HOWTO(R_SPARC_UNUSED_42, 0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_UNUSED_42",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_7,         0,2, 7,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_7",       FALSE,0,0x0000007f,TRUE),
  HOWTO(R_SPARC_5,         0,2, 5,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_5",       FALSE,0,0x0000001f,TRUE),
  HOWTO(R_SPARC_6,         0,2, 6,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_6",       FALSE,0,0x0000003f,TRUE),
  HOWTO(R_SPARC_NONE,      0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_NONE,      0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_NONE,      0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_NONE,      0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_NONE,      0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_NONE,      0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_NONE,      0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_NONE,      0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_UA64,      0,0, 0,FALSE,0,complain_overflow_dont,    sparc_elf_notsupported_reloc,  "R_SPARC_UA64",    FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_UA16,      0,1,16,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_UA16",    FALSE,0,0x0000ffff,TRUE),
  HOWTO(R_SPARC_TLS_GD_HI22,10,2,22,FALSE,0,complain_overflow_dont,  bfd_elf_generic_reloc,  "R_SPARC_TLS_GD_HI22",FALSE,0,0x003fffff,TRUE),
  HOWTO(R_SPARC_TLS_GD_LO10,0,2,10,FALSE,0,complain_overflow_dont,   bfd_elf_generic_reloc,  "R_SPARC_TLS_GD_LO10",FALSE,0,0x000003ff,TRUE),
  HOWTO(R_SPARC_TLS_GD_ADD,0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_TLS_GD_ADD",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_GD_CALL,2,2,30,TRUE,0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_TLS_GD_CALL",FALSE,0,0x3fffffff,TRUE),
  HOWTO(R_SPARC_TLS_LDM_HI22,10,2,22,FALSE,0,complain_overflow_dont, bfd_elf_generic_reloc,  "R_SPARC_TLS_LDM_HI22",FALSE,0,0x003fffff,TRUE),
  HOWTO(R_SPARC_TLS_LDM_LO10,0,2,10,FALSE,0,complain_overflow_dont,  bfd_elf_generic_reloc,  "R_SPARC_TLS_LDM_LO10",FALSE,0,0x000003ff,TRUE),
  HOWTO(R_SPARC_TLS_LDM_ADD,0,0, 0,FALSE,0,complain_overflow_dont,   bfd_elf_generic_reloc,  "R_SPARC_TLS_LDM_ADD",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_LDM_CALL,2,2,30,TRUE,0,complain_overflow_signed, bfd_elf_generic_reloc,  "R_SPARC_TLS_LDM_CALL",FALSE,0,0x3fffffff,TRUE),
  HOWTO(R_SPARC_TLS_LDO_HIX22,0,2,0,FALSE,0,complain_overflow_bitfield,sparc_elf_hix22_reloc,"R_SPARC_TLS_LDO_HIX22",FALSE,0,0x003fffff, FALSE),
  HOWTO(R_SPARC_TLS_LDO_LOX10,0,2,0,FALSE,0,complain_overflow_dont,  sparc_elf_lox10_reloc,  "R_SPARC_TLS_LDO_LOX10",FALSE,0,0x000003ff, FALSE),
  HOWTO(R_SPARC_TLS_LDO_ADD,0,0, 0,FALSE,0,complain_overflow_dont,   bfd_elf_generic_reloc,  "R_SPARC_TLS_LDO_ADD",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_IE_HI22,10,2,22,FALSE,0,complain_overflow_dont,  bfd_elf_generic_reloc,  "R_SPARC_TLS_IE_HI22",FALSE,0,0x003fffff,TRUE),
  HOWTO(R_SPARC_TLS_IE_LO10,0,2,10,FALSE,0,complain_overflow_dont,   bfd_elf_generic_reloc,  "R_SPARC_TLS_IE_LO10",FALSE,0,0x000003ff,TRUE),
  HOWTO(R_SPARC_TLS_IE_LD,0,0, 0,FALSE,0,complain_overflow_dont,     bfd_elf_generic_reloc,  "R_SPARC_TLS_IE_LD",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_IE_LDX,0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_TLS_IE_LDX",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_IE_ADD,0,0, 0,FALSE,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_TLS_IE_ADD",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_LE_HIX22,0,2,0,FALSE,0,complain_overflow_bitfield,sparc_elf_hix22_reloc, "R_SPARC_TLS_LE_HIX22",FALSE,0,0x003fffff, FALSE),
  HOWTO(R_SPARC_TLS_LE_LOX10,0,2,0,FALSE,0,complain_overflow_dont,   sparc_elf_lox10_reloc,  "R_SPARC_TLS_LE_LOX10",FALSE,0,0x000003ff, FALSE),
  HOWTO(R_SPARC_TLS_DTPMOD32,0,0, 0,FALSE,0,complain_overflow_dont,  bfd_elf_generic_reloc,  "R_SPARC_TLS_DTPMOD32",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_DTPMOD64,0,0, 0,FALSE,0,complain_overflow_dont,  bfd_elf_generic_reloc,  "R_SPARC_TLS_DTPMOD64",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_DTPOFF32,0,2,32,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc,"R_SPARC_TLS_DTPOFF32",FALSE,0,0xffffffff,TRUE),
  HOWTO(R_SPARC_TLS_DTPOFF64,0,0, 0,FALSE,0,complain_overflow_dont,  bfd_elf_generic_reloc,  "R_SPARC_TLS_DTPOFF64",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_TPOFF32,0,0, 0,FALSE,0,complain_overflow_dont,   bfd_elf_generic_reloc,  "R_SPARC_TLS_TPOFF32",FALSE,0,0x00000000,TRUE),
  HOWTO(R_SPARC_TLS_TPOFF64,0,0, 0,FALSE,0,complain_overflow_dont,   bfd_elf_generic_reloc,  "R_SPARC_TLS_TPOFF64",FALSE,0,0x00000000,TRUE)
};
static reloc_howto_type elf32_sparc_vtinherit_howto =
  HOWTO (R_SPARC_GNU_VTINHERIT, 0,2,0,FALSE,0,complain_overflow_dont, NULL, "R_SPARC_GNU_VTINHERIT", FALSE,0, 0, FALSE);
static reloc_howto_type elf32_sparc_vtentry_howto =
  HOWTO (R_SPARC_GNU_VTENTRY, 0,2,0,FALSE,0,complain_overflow_dont, _bfd_elf_rel_vtable_reloc_fn,"R_SPARC_GNU_VTENTRY", FALSE,0,0, FALSE);
static reloc_howto_type elf32_sparc_rev32_howto =
  HOWTO(R_SPARC_REV32, 0,2,32,FALSE,0,complain_overflow_bitfield,bfd_elf_generic_reloc, "R_SPARC_REV32", FALSE,0,0xffffffff,TRUE);

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct elf_reloc_map sparc_reloc_map[] =
{
  { BFD_RELOC_NONE, R_SPARC_NONE, },
  { BFD_RELOC_16, R_SPARC_16, },
  { BFD_RELOC_16_PCREL, R_SPARC_DISP16 },
  { BFD_RELOC_8, R_SPARC_8 },
  { BFD_RELOC_8_PCREL, R_SPARC_DISP8 },
  { BFD_RELOC_CTOR, R_SPARC_32 },
  { BFD_RELOC_32, R_SPARC_32 },
  { BFD_RELOC_32_PCREL, R_SPARC_DISP32 },
  { BFD_RELOC_HI22, R_SPARC_HI22 },
  { BFD_RELOC_LO10, R_SPARC_LO10, },
  { BFD_RELOC_32_PCREL_S2, R_SPARC_WDISP30 },
  { BFD_RELOC_SPARC_PLT32, R_SPARC_PLT32 },
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
  { BFD_RELOC_SPARC_UA16, R_SPARC_UA16 },
  { BFD_RELOC_SPARC_UA32, R_SPARC_UA32 },
  { BFD_RELOC_SPARC_UA64, R_SPARC_UA64 },
  { BFD_RELOC_SPARC_10, R_SPARC_10 },
  { BFD_RELOC_SPARC_11, R_SPARC_11 },
  { BFD_RELOC_SPARC_64, R_SPARC_64 },
  { BFD_RELOC_SPARC_OLO10, R_SPARC_OLO10 },
  { BFD_RELOC_SPARC_HH22, R_SPARC_HH22 },
  { BFD_RELOC_SPARC_HM10, R_SPARC_HM10 },
  { BFD_RELOC_SPARC_LM22, R_SPARC_LM22 },
  { BFD_RELOC_SPARC_PC_HH22, R_SPARC_PC_HH22 },
  { BFD_RELOC_SPARC_PC_HM10, R_SPARC_PC_HM10 },
  { BFD_RELOC_SPARC_PC_LM22, R_SPARC_PC_LM22 },
  { BFD_RELOC_SPARC_WDISP16, R_SPARC_WDISP16 },
  { BFD_RELOC_SPARC_WDISP19, R_SPARC_WDISP19 },
  { BFD_RELOC_SPARC_7, R_SPARC_7 },
  { BFD_RELOC_SPARC_5, R_SPARC_5 },
  { BFD_RELOC_SPARC_6, R_SPARC_6 },
  { BFD_RELOC_SPARC_TLS_GD_HI22, R_SPARC_TLS_GD_HI22 },
  { BFD_RELOC_SPARC_TLS_GD_LO10, R_SPARC_TLS_GD_LO10 },
  { BFD_RELOC_SPARC_TLS_GD_ADD, R_SPARC_TLS_GD_ADD },
  { BFD_RELOC_SPARC_TLS_GD_CALL, R_SPARC_TLS_GD_CALL },
  { BFD_RELOC_SPARC_TLS_LDM_HI22, R_SPARC_TLS_LDM_HI22 },
  { BFD_RELOC_SPARC_TLS_LDM_LO10, R_SPARC_TLS_LDM_LO10 },
  { BFD_RELOC_SPARC_TLS_LDM_ADD, R_SPARC_TLS_LDM_ADD },
  { BFD_RELOC_SPARC_TLS_LDM_CALL, R_SPARC_TLS_LDM_CALL },
  { BFD_RELOC_SPARC_TLS_LDO_HIX22, R_SPARC_TLS_LDO_HIX22 },
  { BFD_RELOC_SPARC_TLS_LDO_LOX10, R_SPARC_TLS_LDO_LOX10 },
  { BFD_RELOC_SPARC_TLS_LDO_ADD, R_SPARC_TLS_LDO_ADD },
  { BFD_RELOC_SPARC_TLS_IE_HI22, R_SPARC_TLS_IE_HI22 },
  { BFD_RELOC_SPARC_TLS_IE_LO10, R_SPARC_TLS_IE_LO10 },
  { BFD_RELOC_SPARC_TLS_IE_LD, R_SPARC_TLS_IE_LD },
  { BFD_RELOC_SPARC_TLS_IE_LDX, R_SPARC_TLS_IE_LDX },
  { BFD_RELOC_SPARC_TLS_IE_ADD, R_SPARC_TLS_IE_ADD },
  { BFD_RELOC_SPARC_TLS_LE_HIX22, R_SPARC_TLS_LE_HIX22 },
  { BFD_RELOC_SPARC_TLS_LE_LOX10, R_SPARC_TLS_LE_LOX10 },
  { BFD_RELOC_SPARC_TLS_DTPMOD32, R_SPARC_TLS_DTPMOD32 },
  { BFD_RELOC_SPARC_TLS_DTPMOD64, R_SPARC_TLS_DTPMOD64 },
  { BFD_RELOC_SPARC_TLS_DTPOFF32, R_SPARC_TLS_DTPOFF32 },
  { BFD_RELOC_SPARC_TLS_DTPOFF64, R_SPARC_TLS_DTPOFF64 },
  { BFD_RELOC_SPARC_TLS_TPOFF32, R_SPARC_TLS_TPOFF32 },
  { BFD_RELOC_SPARC_TLS_TPOFF64, R_SPARC_TLS_TPOFF64 },
  { BFD_RELOC_VTABLE_INHERIT, R_SPARC_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_SPARC_GNU_VTENTRY },
  { BFD_RELOC_SPARC_REV32, R_SPARC_REV32 }
};

static reloc_howto_type *
elf32_sparc_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  switch (code)
    {
    case BFD_RELOC_VTABLE_INHERIT:
      return &elf32_sparc_vtinherit_howto;

    case BFD_RELOC_VTABLE_ENTRY:
      return &elf32_sparc_vtentry_howto;

    case BFD_RELOC_SPARC_REV32:
      return &elf32_sparc_rev32_howto;

    default:
      for (i = 0;
	   i < sizeof (sparc_reloc_map) / sizeof (struct elf_reloc_map);
	   i++)
	{
	  if (sparc_reloc_map[i].bfd_reloc_val == code)
	    return (_bfd_sparc_elf_howto_table
		    + (int) sparc_reloc_map[i].elf_reloc_val);
	}
    }
    bfd_set_error (bfd_error_bad_value);
    return NULL;
}

/* We need to use ELF32_R_TYPE so we have our own copy of this function,
   and elf64-sparc.c has its own copy.  */

static void
elf32_sparc_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  switch (ELF32_R_TYPE(dst->r_info))
    {
    case R_SPARC_GNU_VTINHERIT:
      cache_ptr->howto = &elf32_sparc_vtinherit_howto;
      break;

    case R_SPARC_GNU_VTENTRY:
      cache_ptr->howto = &elf32_sparc_vtentry_howto;
      break;

    case R_SPARC_REV32:
      cache_ptr->howto = &elf32_sparc_rev32_howto;
      break;

    default:
      BFD_ASSERT (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_SPARC_max_std);
      cache_ptr->howto = &_bfd_sparc_elf_howto_table[ELF32_R_TYPE(dst->r_info)];
    }
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
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
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
     char **error_message ATTRIBUTE_UNUSED;
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

  x = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);
  x |= ((((relocation >> 2) & 0xc000) << 6)
	| ((relocation >> 2) & 0x3fff));
  bfd_put_32 (abfd, x, (bfd_byte *) data + reloc_entry->address);

  if ((bfd_signed_vma) relocation < - 0x40000
      || (bfd_signed_vma) relocation > 0x3ffff)
    return bfd_reloc_overflow;
  else
    return bfd_reloc_ok;
}

/* Handle the HIX22 reloc.  */

static bfd_reloc_status_type
sparc_elf_hix22_reloc (abfd,
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
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;
  bfd_vma insn;

  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0)
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
  insn = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  insn = (insn &~ (bfd_vma) 0x3fffff) | (((~relocation) >> 10) & 0x3fffff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  return bfd_reloc_ok;
}

/* Handle the LOX10 reloc.  */

static bfd_reloc_status_type
sparc_elf_lox10_reloc (abfd,
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
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;
  bfd_vma insn;

  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0)
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
  insn = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  insn = (insn &~ (bfd_vma) 0x1fff) | 0x1c00 | (relocation & 0x3ff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  return bfd_reloc_ok;
}

/* Support for core dump NOTE sections.  */

static bfd_boolean
elf32_sparc_grok_psinfo (bfd *abfd, Elf_Internal_Note *note)
{
  switch (note->descsz)
    {
    default:
      return FALSE;

    case 260:			/* Solaris prpsinfo_t.  */
      elf_tdata (abfd)->core_program
	= _bfd_elfcore_strndup (abfd, note->descdata + 84, 16);
      elf_tdata (abfd)->core_command
	= _bfd_elfcore_strndup (abfd, note->descdata + 100, 80);
      break;

    case 336:			/* Solaris psinfo_t.  */
      elf_tdata (abfd)->core_program
	= _bfd_elfcore_strndup (abfd, note->descdata + 88, 16);
      elf_tdata (abfd)->core_command
	= _bfd_elfcore_strndup (abfd, note->descdata + 104, 80);
      break;
    }

  return TRUE;
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

/* The SPARC linker needs to keep track of the number of relocs that it
   decides to copy as dynamic relocs in check_relocs for each symbol.
   This is so that it can later discard them if they are found to be
   unnecessary.  We store the information in a field extending the
   regular ELF linker hash table.  */

struct elf32_sparc_dyn_relocs
{
  struct elf32_sparc_dyn_relocs *next;

  /* The input section of the reloc.  */
  asection *sec;

  /* Total number of relocs copied for the input section.  */
  bfd_size_type count;

  /* Number of pc-relative relocs copied for the input section.  */
  bfd_size_type pc_count;
};

/* SPARC ELF linker hash entry.  */

struct elf32_sparc_link_hash_entry
{
  struct elf_link_hash_entry elf;

  /* Track dynamic relocs copied for this symbol.  */
  struct elf32_sparc_dyn_relocs *dyn_relocs;

#define GOT_UNKNOWN     0
#define GOT_NORMAL      1
#define GOT_TLS_GD      2
#define GOT_TLS_IE      3
  unsigned char tls_type;
};

#define elf32_sparc_hash_entry(ent) ((struct elf32_sparc_link_hash_entry *)(ent))

struct elf32_sparc_obj_tdata
{
  struct elf_obj_tdata root;

  /* tls_type for each local got entry.  */
  char *local_got_tls_type;

  /* TRUE if TLS GD relocs has been seen for this object.  */
  bfd_boolean has_tlsgd;
};

#define elf32_sparc_tdata(abfd) \
  ((struct elf32_sparc_obj_tdata *) (abfd)->tdata.any)

#define elf32_sparc_local_got_tls_type(abfd) \
  (elf32_sparc_tdata (abfd)->local_got_tls_type)

static bfd_boolean
elf32_sparc_mkobject (abfd)
     bfd *abfd;
{
  bfd_size_type amt = sizeof (struct elf32_sparc_obj_tdata);
  abfd->tdata.any = bfd_zalloc (abfd, amt);
  if (abfd->tdata.any == NULL)
    return FALSE;
  return TRUE;
}

/* SPARC ELF linker hash table.  */

struct elf32_sparc_link_hash_table
{
  struct elf_link_hash_table elf;

  /* Short-cuts to get to dynamic linker sections.  */
  asection *sgot;
  asection *srelgot;
  asection *splt;
  asection *srelplt;
  asection *sdynbss;
  asection *srelbss;

  union {
    bfd_signed_vma refcount;
    bfd_vma offset;
  } tls_ldm_got;

  /* Small local sym to section mapping cache.  */
  struct sym_sec_cache sym_sec;
};

/* Get the SPARC ELF linker hash table from a link_info structure.  */

#define elf32_sparc_hash_table(p) \
  ((struct elf32_sparc_link_hash_table *) ((p)->hash))

/* Create an entry in an i386 ELF linker hash table.  */

static struct bfd_hash_entry *
link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (entry == NULL)
    {
      entry = bfd_hash_allocate (table,
				 sizeof (struct elf32_sparc_link_hash_entry));
      if (entry == NULL)
	return entry;
    }

  /* Call the allocation method of the superclass.  */
  entry = _bfd_elf_link_hash_newfunc (entry, table, string);
  if (entry != NULL)
    {
      struct elf32_sparc_link_hash_entry *eh;

      eh = (struct elf32_sparc_link_hash_entry *) entry;
      eh->dyn_relocs = NULL;
      eh->tls_type = GOT_UNKNOWN;
    }

  return entry;
}

/* Create a SPARC ELF linker hash table.  */

static struct bfd_link_hash_table *
elf32_sparc_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf32_sparc_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct elf32_sparc_link_hash_table);

  ret = (struct elf32_sparc_link_hash_table *) bfd_malloc (amt);
  if (ret == NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->elf, abfd, link_hash_newfunc))
    {
      free (ret);
      return NULL;
    }

  ret->sgot = NULL;
  ret->srelgot = NULL;
  ret->splt = NULL;
  ret->srelplt = NULL;
  ret->sdynbss = NULL;
  ret->srelbss = NULL;
  ret->tls_ldm_got.refcount = 0;
  ret->sym_sec.abfd = NULL;

  return &ret->elf.root;
}

/* Create .got and .rela.got sections in DYNOBJ, and set up
   shortcuts to them in our hash table.  */

static bfd_boolean
create_got_section (dynobj, info)
     bfd *dynobj;
     struct bfd_link_info *info;
{
  struct elf32_sparc_link_hash_table *htab;

  if (! _bfd_elf_create_got_section (dynobj, info))
    return FALSE;

  htab = elf32_sparc_hash_table (info);
  htab->sgot = bfd_get_section_by_name (dynobj, ".got");
  if (!htab->sgot)
    abort ();

  htab->srelgot = bfd_make_section (dynobj, ".rela.got");
  if (htab->srelgot == NULL
      || ! bfd_set_section_flags (dynobj, htab->srelgot,
				  (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
				   | SEC_IN_MEMORY | SEC_LINKER_CREATED
				   | SEC_READONLY))
      || ! bfd_set_section_alignment (dynobj, htab->srelgot, 2))
    return FALSE;
  return TRUE;
}

/* Create .plt, .rela.plt, .got, .rela.got, .dynbss, and
   .rela.bss sections in DYNOBJ, and set up shortcuts to them in our
   hash table.  */

static bfd_boolean
elf32_sparc_create_dynamic_sections (dynobj, info)
     bfd *dynobj;
     struct bfd_link_info *info;
{
  struct elf32_sparc_link_hash_table *htab;

  htab = elf32_sparc_hash_table (info);
  if (!htab->sgot && !create_got_section (dynobj, info))
    return FALSE;

  if (!_bfd_elf_create_dynamic_sections (dynobj, info))
    return FALSE;

  htab->splt = bfd_get_section_by_name (dynobj, ".plt");
  htab->srelplt = bfd_get_section_by_name (dynobj, ".rela.plt");
  htab->sdynbss = bfd_get_section_by_name (dynobj, ".dynbss");
  if (!info->shared)
    htab->srelbss = bfd_get_section_by_name (dynobj, ".rela.bss");

  if (!htab->splt || !htab->srelplt || !htab->sdynbss
      || (!info->shared && !htab->srelbss))
    abort ();

  return TRUE;
}

/* Copy the extra info we tack onto an elf_link_hash_entry.  */

static void
elf32_sparc_copy_indirect_symbol (bed, dir, ind)
     const struct elf_backend_data *bed;
     struct elf_link_hash_entry *dir, *ind;
{
  struct elf32_sparc_link_hash_entry *edir, *eind;

  edir = (struct elf32_sparc_link_hash_entry *) dir;
  eind = (struct elf32_sparc_link_hash_entry *) ind;

  if (eind->dyn_relocs != NULL)
    {
      if (edir->dyn_relocs != NULL)
	{
	  struct elf32_sparc_dyn_relocs **pp;
	  struct elf32_sparc_dyn_relocs *p;

	  if (ind->root.type == bfd_link_hash_indirect)
	    abort ();

	  /* Add reloc counts against the weak sym to the strong sym
	     list.  Merge any entries against the same section.  */
	  for (pp = &eind->dyn_relocs; (p = *pp) != NULL; )
	    {
	      struct elf32_sparc_dyn_relocs *q;

	      for (q = edir->dyn_relocs; q != NULL; q = q->next)
		if (q->sec == p->sec)
		  {
		    q->pc_count += p->pc_count;
		    q->count += p->count;
		    *pp = p->next;
		    break;
		  }
	      if (q == NULL)
		pp = &p->next;
	    }
	  *pp = edir->dyn_relocs;
	}

      edir->dyn_relocs = eind->dyn_relocs;
      eind->dyn_relocs = NULL;
    }

  if (ind->root.type == bfd_link_hash_indirect
      && dir->got.refcount <= 0)
    {
      edir->tls_type = eind->tls_type;
      eind->tls_type = GOT_UNKNOWN;
    }
  _bfd_elf_link_hash_copy_indirect (bed, dir, ind);
}

static int
elf32_sparc_tls_transition (info, abfd, r_type, is_local)
     struct bfd_link_info *info;
     bfd *abfd;
     int r_type;
     int is_local;
{
  if (r_type == R_SPARC_TLS_GD_HI22
      && ! elf32_sparc_tdata (abfd)->has_tlsgd)
    r_type = R_SPARC_REV32;

  if (info->shared)
    return r_type;

  switch (r_type)
    {
    case R_SPARC_TLS_GD_HI22:
      if (is_local)
	return R_SPARC_TLS_LE_HIX22;
      return R_SPARC_TLS_IE_HI22;
    case R_SPARC_TLS_GD_LO10:
      if (is_local)
	return R_SPARC_TLS_LE_LOX10;
      return R_SPARC_TLS_IE_LO10;
    case R_SPARC_TLS_IE_HI22:
      if (is_local)
	return R_SPARC_TLS_LE_HIX22;
      return r_type;
    case R_SPARC_TLS_IE_LO10:
      if (is_local)
	return R_SPARC_TLS_LE_LOX10;
      return r_type;
    case R_SPARC_TLS_LDM_HI22:
      return R_SPARC_TLS_LE_HIX22;
    case R_SPARC_TLS_LDM_LO10:
      return R_SPARC_TLS_LE_LOX10;
    }

  return r_type;
}

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static bfd_boolean
elf32_sparc_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  struct elf32_sparc_link_hash_table *htab;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sreloc;
  bfd_boolean checked_tlsgd = FALSE;

  if (info->relocatable)
    return TRUE;

  htab = elf32_sparc_hash_table (info);
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_offsets = elf_local_got_offsets (abfd);

  sreloc = NULL;

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
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      /* Compatibility with old R_SPARC_REV32 reloc conflicting
	 with R_SPARC_TLS_GD_HI22.  */
      if (! checked_tlsgd)
	switch (r_type)
	  {
	  case R_SPARC_TLS_GD_HI22:
	    {
	      const Elf_Internal_Rela *relt;

	      for (relt = rel + 1; relt < rel_end; relt++)
		if (ELF32_R_TYPE (relt->r_info) == R_SPARC_TLS_GD_LO10
		    || ELF32_R_TYPE (relt->r_info) == R_SPARC_TLS_GD_ADD
		    || ELF32_R_TYPE (relt->r_info) == R_SPARC_TLS_GD_CALL)
		  break;
	      checked_tlsgd = TRUE;
	      elf32_sparc_tdata (abfd)->has_tlsgd = relt < rel_end;
	    }
	    break;
	  case R_SPARC_TLS_GD_LO10:
	  case R_SPARC_TLS_GD_ADD:
	  case R_SPARC_TLS_GD_CALL:
	    checked_tlsgd = TRUE;
	    elf32_sparc_tdata (abfd)->has_tlsgd = TRUE;
	    break;
	  }

      r_type = elf32_sparc_tls_transition (info, abfd, r_type, h == NULL);
      switch (r_type)
	{
	case R_SPARC_TLS_LDM_HI22:
	case R_SPARC_TLS_LDM_LO10:
	  htab->tls_ldm_got.refcount += 1;
	  break;

	case R_SPARC_TLS_LE_HIX22:
	case R_SPARC_TLS_LE_LOX10:
	  if (info->shared)
	    goto r_sparc_plt32;
	  break;

	case R_SPARC_TLS_IE_HI22:
	case R_SPARC_TLS_IE_LO10:
	  if (info->shared)
	    info->flags |= DF_STATIC_TLS;
	  /* Fall through */

	case R_SPARC_GOT10:
	case R_SPARC_GOT13:
	case R_SPARC_GOT22:
	case R_SPARC_TLS_GD_HI22:
	case R_SPARC_TLS_GD_LO10:
	  /* This symbol requires a global offset table entry.  */
	  {
	    int tls_type, old_tls_type;

	    switch (r_type)
	      {
	      default:
	      case R_SPARC_GOT10:
	      case R_SPARC_GOT13:
	      case R_SPARC_GOT22:
		tls_type = GOT_NORMAL;
		break;
	      case R_SPARC_TLS_GD_HI22:
	      case R_SPARC_TLS_GD_LO10:
		tls_type = GOT_TLS_GD;
		break;
	      case R_SPARC_TLS_IE_HI22:
	      case R_SPARC_TLS_IE_LO10:
		tls_type = GOT_TLS_IE;
		break;
	      }

	    if (h != NULL)
	      {
		h->got.refcount += 1;
		old_tls_type = elf32_sparc_hash_entry(h)->tls_type;
	      }
	    else
	      {
		bfd_signed_vma *local_got_refcounts;

		/* This is a global offset table entry for a local symbol.  */
		local_got_refcounts = elf_local_got_refcounts (abfd);
		if (local_got_refcounts == NULL)
		  {
		    bfd_size_type size;

		    size = symtab_hdr->sh_info;
		    size *= (sizeof (bfd_signed_vma) + sizeof(char));
		    local_got_refcounts = ((bfd_signed_vma *)
					   bfd_zalloc (abfd, size));
		    if (local_got_refcounts == NULL)
		      return FALSE;
		    elf_local_got_refcounts (abfd) = local_got_refcounts;
		    elf32_sparc_local_got_tls_type (abfd)
		      = (char *) (local_got_refcounts + symtab_hdr->sh_info);
		  }
		local_got_refcounts[r_symndx] += 1;
		old_tls_type = elf32_sparc_local_got_tls_type (abfd) [r_symndx];
	      }

	    /* If a TLS symbol is accessed using IE at least once,
	       there is no point to use dynamic model for it.  */
	    if (old_tls_type != tls_type && old_tls_type != GOT_UNKNOWN
		&& (old_tls_type != GOT_TLS_GD
		    || tls_type != GOT_TLS_IE))
	      {
		if (old_tls_type == GOT_TLS_IE && tls_type == GOT_TLS_GD)
		  tls_type = old_tls_type;
		else
		  {
		    (*_bfd_error_handler)
		      (_("%s: `%s' accessed both as normal and thread local symbol"),
		       bfd_archive_filename (abfd),
		       h ? h->root.root.string : "<local>");
		    return FALSE;
		  }
	      }

	    if (old_tls_type != tls_type)
	      {
		if (h != NULL)
		  elf32_sparc_hash_entry (h)->tls_type = tls_type;
		else
		  elf32_sparc_local_got_tls_type (abfd) [r_symndx] = tls_type;
	      }
	  }

	  if (htab->sgot == NULL)
	    {
	      if (htab->elf.dynobj == NULL)
		htab->elf.dynobj = abfd;
	      if (!create_got_section (htab->elf.dynobj, info))
		return FALSE;
	    }
	  break;

	case R_SPARC_TLS_GD_CALL:
	case R_SPARC_TLS_LDM_CALL:
	  if (info->shared)
	    {
	      /* These are basically R_SPARC_TLS_WPLT30 relocs against
		 __tls_get_addr.  */
	      struct bfd_link_hash_entry *bh = NULL;
	      if (! _bfd_generic_link_add_one_symbol (info, abfd,
						      "__tls_get_addr", 0,
						      bfd_und_section_ptr, 0,
						      NULL, FALSE, FALSE,
						      &bh))
		return FALSE;
	      h = (struct elf_link_hash_entry *) bh;
	    }
	  else
	    break;
	  /* Fall through */

	case R_SPARC_PLT32:
	case R_SPARC_WPLT30:
	  /* This symbol requires a procedure linkage table entry.  We
	     actually build the entry in adjust_dynamic_symbol,
	     because this might be a case of linking PIC code without
	     linking in any dynamic objects, in which case we don't
	     need to generate a procedure linkage table after all.  */

	  if (h == NULL)
	    {
	      /* The Solaris native assembler will generate a WPLT30
		 reloc for a local symbol if you assemble a call from
		 one section to another when using -K pic.  We treat
		 it as WDISP30.  */
	      if (ELF32_R_TYPE (rel->r_info) == R_SPARC_PLT32)
		goto r_sparc_plt32;
	      break;
	    }

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;

	  if (ELF32_R_TYPE (rel->r_info) == R_SPARC_PLT32)
	    goto r_sparc_plt32;
	  h->plt.refcount += 1;
	  break;

	case R_SPARC_PC10:
	case R_SPARC_PC22:
	  if (h != NULL)
	    h->elf_link_hash_flags |= ELF_LINK_NON_GOT_REF;

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
	case R_SPARC_8:
	case R_SPARC_16:
	case R_SPARC_32:
	case R_SPARC_HI22:
	case R_SPARC_22:
	case R_SPARC_13:
	case R_SPARC_LO10:
	case R_SPARC_UA16:
	case R_SPARC_UA32:
	  if (h != NULL)
	    h->elf_link_hash_flags |= ELF_LINK_NON_GOT_REF;

	r_sparc_plt32:
	  if (h != NULL && !info->shared)
	    {
	      /* We may need a .plt entry if the function this reloc
		 refers to is in a shared lib.  */
	      h->plt.refcount += 1;
	    }

	  /* If we are creating a shared library, and this is a reloc
	     against a global symbol, or a non PC relative reloc
	     against a local symbol, then we need to copy the reloc
	     into the shared library.  However, if we are linking with
	     -Bsymbolic, we do not need to copy a reloc against a
	     global symbol which is defined in an object we are
	     including in the link (i.e., DEF_REGULAR is set).  At
	     this point we have not seen all the input files, so it is
	     possible that DEF_REGULAR is not set now but will be set
	     later (it is never cleared).  In case of a weak definition,
	     DEF_REGULAR may be cleared later by a strong definition in
	     a shared library.  We account for that possibility below by
	     storing information in the relocs_copied field of the hash
	     table entry.  A similar situation occurs when creating
	     shared libraries and symbol visibility changes render the
	     symbol local.

	     If on the other hand, we are creating an executable, we
	     may need to keep relocations for symbols satisfied by a
	     dynamic library if we manage to avoid copy relocs for the
	     symbol.  */
	  if ((info->shared
	       && (sec->flags & SEC_ALLOC) != 0
	       && (! _bfd_sparc_elf_howto_table[r_type].pc_relative
		   || (h != NULL
		       && (! info->symbolic
			   || h->root.type == bfd_link_hash_defweak
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))))
	      || (!info->shared
		  && (sec->flags & SEC_ALLOC) != 0
		  && h != NULL
		  && (h->root.type == bfd_link_hash_defweak
		      || (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)))
	    {
	      struct elf32_sparc_dyn_relocs *p;
	      struct elf32_sparc_dyn_relocs **head;

	      /* When creating a shared object, we must copy these
		 relocs into the output file.  We create a reloc
		 section in dynobj and make room for the reloc.  */
	      if (sreloc == NULL)
		{
		  const char *name;
		  bfd *dynobj;

		  name = (bfd_elf_string_from_elf_section
			  (abfd,
			   elf_elfheader (abfd)->e_shstrndx,
			   elf_section_data (sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return FALSE;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (abfd, sec),
					 name + 5) == 0);

		  if (htab->elf.dynobj == NULL)
		    htab->elf.dynobj = abfd;
		  dynobj = htab->elf.dynobj;

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      flagword flags;

		      sreloc = bfd_make_section (dynobj, name);
		      flags = (SEC_HAS_CONTENTS | SEC_READONLY
			       | SEC_IN_MEMORY | SEC_LINKER_CREATED);
		      if ((sec->flags & SEC_ALLOC) != 0)
			flags |= SEC_ALLOC | SEC_LOAD;
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (dynobj, sreloc, flags)
			  || ! bfd_set_section_alignment (dynobj, sreloc, 2))
			return FALSE;
		    }
		  elf_section_data (sec)->sreloc = sreloc;
		}

	      /* If this is a global symbol, we count the number of
		 relocations we need for this symbol.  */
	      if (h != NULL)
		head = &((struct elf32_sparc_link_hash_entry *) h)->dyn_relocs;
	      else
		{
		  /* Track dynamic relocs needed for local syms too.
		     We really need local syms available to do this
		     easily.  Oh well.  */

		  asection *s;
		  s = bfd_section_from_r_symndx (abfd, &htab->sym_sec,
						 sec, r_symndx);
		  if (s == NULL)
		    return FALSE;

		  head = ((struct elf32_sparc_dyn_relocs **)
			  &elf_section_data (s)->local_dynrel);
		}

	      p = *head;
	      if (p == NULL || p->sec != sec)
		{
		  bfd_size_type amt = sizeof *p;
		  p = ((struct elf32_sparc_dyn_relocs *)
		       bfd_alloc (htab->elf.dynobj, amt));
		  if (p == NULL)
		    return FALSE;
		  p->next = *head;
		  *head = p;
		  p->sec = sec;
		  p->count = 0;
		  p->pc_count = 0;
		}

	      p->count += 1;
	      if (_bfd_sparc_elf_howto_table[r_type].pc_relative)
		p->pc_count += 1;
	    }

	  break;

	case R_SPARC_GNU_VTINHERIT:
	  if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return FALSE;
	  break;

	case R_SPARC_GNU_VTENTRY:
	  if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return FALSE;
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}

static asection *
elf32_sparc_gc_mark_hook (sec, info, rel, h, sym)
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
      case R_SPARC_GNU_VTINHERIT:
      case R_SPARC_GNU_VTENTRY:
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

/* Update the got entry reference counts for the section being removed.  */
static bfd_boolean
elf32_sparc_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;

  elf_section_data (sec)->local_dynrel = NULL;

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
	{
	  struct elf32_sparc_link_hash_entry *eh;
	  struct elf32_sparc_dyn_relocs **pp;
	  struct elf32_sparc_dyn_relocs *p;

	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  eh = (struct elf32_sparc_link_hash_entry *) h;
	  for (pp = &eh->dyn_relocs; (p = *pp) != NULL; pp = &p->next)
	    if (p->sec == sec)
	      {
		/* Everything must go for SEC.  */
		*pp = p->next;
		break;
	      }
	}

      r_type = ELF32_R_TYPE (rel->r_info);
      r_type = elf32_sparc_tls_transition (info, abfd, r_type, h != NULL);
      switch (r_type)
	{
	case R_SPARC_TLS_LDM_HI22:
	case R_SPARC_TLS_LDM_LO10:
	  if (elf32_sparc_hash_table (info)->tls_ldm_got.refcount > 0)
	    elf32_sparc_hash_table (info)->tls_ldm_got.refcount -= 1;
	  break;

	case R_SPARC_TLS_GD_HI22:
	case R_SPARC_TLS_GD_LO10:
	case R_SPARC_TLS_IE_HI22:
	case R_SPARC_TLS_IE_LO10:
	case R_SPARC_GOT10:
	case R_SPARC_GOT13:
	case R_SPARC_GOT22:
	  if (h != NULL)
	    {
	      if (h->got.refcount > 0)
		h->got.refcount--;
	    }
	  else
	    {
	      if (local_got_refcounts[r_symndx] > 0)
		local_got_refcounts[r_symndx]--;
	    }
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
	case R_SPARC_8:
	case R_SPARC_16:
	case R_SPARC_32:
	case R_SPARC_HI22:
	case R_SPARC_22:
	case R_SPARC_13:
	case R_SPARC_LO10:
	case R_SPARC_UA16:
	case R_SPARC_UA32:
	case R_SPARC_PLT32:
	  if (info->shared)
	    break;
	  /* Fall through.  */

	case R_SPARC_WPLT30:
	  if (h != NULL)
	    {
	      if (h->plt.refcount > 0)
		h->plt.refcount--;
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
elf32_sparc_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  struct elf32_sparc_link_hash_table *htab;
  struct elf32_sparc_link_hash_entry * eh;
  struct elf32_sparc_dyn_relocs *p;
  asection *s;
  unsigned int power_of_two;

  htab = elf32_sparc_hash_table (info);

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (htab->elf.dynobj != NULL
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
     (although we could actually do it here).  The STT_NOTYPE
     condition is a hack specifically for the Oracle libraries
     delivered for Solaris; for some inexplicable reason, they define
     some of their functions as STT_NOTYPE when they really should be
     STT_FUNC.  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0
      || (h->type == STT_NOTYPE
	  && (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	  && (h->root.u.def.section->flags & SEC_CODE) != 0))
    {
      if (h->plt.refcount <= 0
	  || (! info->shared
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) == 0
	      && h->root.type != bfd_link_hash_undefweak
	      && h->root.type != bfd_link_hash_undefined))
	{
	  /* This case can occur if we saw a WPLT30 reloc in an input
	     file, but the symbol was never referred to by a dynamic
	     object, or if all references were garbage collected.  In
	     such a case, we don't actually need to build a procedure
	     linkage table, and we can just do a WDISP30 reloc instead.  */
	  h->plt.offset = (bfd_vma) -1;
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	}

      return TRUE;
    }
  else
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

  /* If there are no references to this symbol that do not use the
     GOT, we don't need to generate a copy reloc.  */
  if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0)
    return TRUE;

  eh = (struct elf32_sparc_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      s = p->sec->output_section;
      if (s != NULL && (s->flags & SEC_READONLY) != 0)
	break;
    }

  /* If we didn't find any dynamic relocs in read-only sections, then
     we'll be keeping the dynamic relocs and avoiding the copy reloc.  */
  if (p == NULL)
    {
      h->elf_link_hash_flags &= ~ELF_LINK_NON_GOT_REF;
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

  /* We must generate a R_SPARC_COPY reloc to tell the dynamic linker
     to copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rel.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      htab->srelbss->_raw_size += sizeof (Elf32_External_Rela);
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_COPY;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 3)
    power_of_two = 3;

  /* Apply the required alignment.  */
  s = htab->sdynbss;
  s->_raw_size = BFD_ALIGN (s->_raw_size,
			    (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (dynobj, s))
    {
      if (! bfd_set_section_alignment (dynobj, s, power_of_two))
	return FALSE;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return TRUE;
}

/* Allocate space in .plt, .got and associated reloc sections for
   dynamic relocs.  */

static bfd_boolean
allocate_dynrelocs (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct bfd_link_info *info;
  struct elf32_sparc_link_hash_table *htab;
  struct elf32_sparc_link_hash_entry *eh;
  struct elf32_sparc_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_indirect)
    return TRUE;

  if (h->root.type == bfd_link_hash_warning)
    /* When warning symbols are created, they **replace** the "real"
       entry in the hash table, thus we never get to see the real
       symbol in a hash traversal.  So look at it now.  */
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  info = (struct bfd_link_info *) inf;
  htab = elf32_sparc_hash_table (info);

  if (htab->elf.dynamic_sections_created
      && h->plt.refcount > 0)
    {
      /* Make sure this symbol is output as a dynamic symbol.
	 Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
	  && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	{
	  if (! bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      if (WILL_CALL_FINISH_DYNAMIC_SYMBOL (1, info->shared, h))
	{
	  asection *s = htab->splt;

	  /* The first four entries in .plt are reserved.  */
	  if (s->_raw_size == 0)
	    s->_raw_size = 4 * PLT_ENTRY_SIZE;

	  /* The procedure linkage table has a maximum size.  */
	  if (s->_raw_size >= 0x400000)
	    {
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }

	  h->plt.offset = s->_raw_size;

	  /* If this symbol is not defined in a regular file, and we are
	     not generating a shared library, then set the symbol to this
	     location in the .plt.  This is required to make function
	     pointers compare as equal between the normal executable and
	     the shared library.  */
	  if (! info->shared
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	    {
	      h->root.u.def.section = s;
	      h->root.u.def.value = h->plt.offset;
	    }

	  /* Make room for this entry.  */
	  s->_raw_size += PLT_ENTRY_SIZE;

	  /* We also need to make an entry in the .rela.plt section.  */
	  htab->srelplt->_raw_size += sizeof (Elf32_External_Rela);
	}
      else
	{
	  h->plt.offset = (bfd_vma) -1;
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	}
    }
  else
    {
      h->plt.offset = (bfd_vma) -1;
      h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
    }

  /* If R_SPARC_TLS_IE_{HI22,LO10} symbol is now local to the binary,
     make it a R_SPARC_TLS_LE_{HI22,LO10} requiring no TLS entry.  */
  if (h->got.refcount > 0
      && !info->shared
      && h->dynindx == -1
      && elf32_sparc_hash_entry(h)->tls_type == GOT_TLS_IE)
    h->got.offset = (bfd_vma) -1;
  else if (h->got.refcount > 0)
    {
      asection *s;
      bfd_boolean dyn;
      int tls_type = elf32_sparc_hash_entry(h)->tls_type;

      /* Make sure this symbol is output as a dynamic symbol.
	 Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
	  && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	{
	  if (! bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      s = htab->sgot;
      h->got.offset = s->_raw_size;
      s->_raw_size += 4;
      /* R_SPARC_TLS_GD_HI{22,LO10} needs 2 consecutive GOT slots.  */
      if (tls_type == GOT_TLS_GD)
	s->_raw_size += 4;
      dyn = htab->elf.dynamic_sections_created;
      /* R_SPARC_TLS_IE_{HI22,LO10} needs one dynamic relocation,
	 R_SPARC_TLS_GD_{HI22,LO10} needs one if local symbol and two if
	 global.  */
      if ((tls_type == GOT_TLS_GD && h->dynindx == -1)
	  || tls_type == GOT_TLS_IE)
	htab->srelgot->_raw_size += sizeof (Elf32_External_Rela);
      else if (tls_type == GOT_TLS_GD)
	htab->srelgot->_raw_size += 2 * sizeof (Elf32_External_Rela);
      else if (WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info->shared, h))
	htab->srelgot->_raw_size += sizeof (Elf32_External_Rela);
    }
  else
    h->got.offset = (bfd_vma) -1;

  eh = (struct elf32_sparc_link_hash_entry *) h;
  if (eh->dyn_relocs == NULL)
    return TRUE;

  /* In the shared -Bsymbolic case, discard space allocated for
     dynamic pc-relative relocs against symbols which turn out to be
     defined in regular objects.  For the normal shared case, discard
     space for pc-relative relocs that have become local due to symbol
     visibility changes.  */

  if (info->shared)
    {
      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
	  && ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0
	      || info->symbolic))
	{
	  struct elf32_sparc_dyn_relocs **pp;

	  for (pp = &eh->dyn_relocs; (p = *pp) != NULL; )
	    {
	      p->count -= p->pc_count;
	      p->pc_count = 0;
	      if (p->count == 0)
		*pp = p->next;
	      else
		pp = &p->next;
	    }
	}
    }
  else
    {
      /* For the non-shared case, discard space for relocs against
	 symbols which turn out to need copy relocs or are not
	 dynamic.  */

      if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0
	  && (((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	       && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	      || (htab->elf.dynamic_sections_created
		  && (h->root.type == bfd_link_hash_undefweak
		      || h->root.type == bfd_link_hash_undefined))))
	{
	  /* Make sure this symbol is output as a dynamic symbol.
	     Undefined weak syms won't yet be marked as dynamic.  */
	  if (h->dynindx == -1
	      && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	    {
	      if (! bfd_elf_link_record_dynamic_symbol (info, h))
		return FALSE;
	    }

	  /* If that succeeded, we know we'll be keeping all the
	     relocs.  */
	  if (h->dynindx != -1)
	    goto keep;
	}

      eh->dyn_relocs = NULL;

    keep: ;
    }

  /* Finally, allocate space.  */
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *sreloc = elf_section_data (p->sec)->sreloc;
      sreloc->_raw_size += p->count * sizeof (Elf32_External_Rela);
    }

  return TRUE;
}

/* Find any dynamic relocs that apply to read-only sections.  */

static bfd_boolean
readonly_dynrelocs (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct elf32_sparc_link_hash_entry *eh;
  struct elf32_sparc_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  eh = (struct elf32_sparc_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *s = p->sec->output_section;

      if (s != NULL && (s->flags & SEC_READONLY) != 0)
	{
	  struct bfd_link_info *info = (struct bfd_link_info *) inf;

	  info->flags |= DF_TEXTREL;

	  /* Not an error, just cut short the traversal.  */
	  return FALSE;
	}
    }
  return TRUE;
}

/* Set the sizes of the dynamic sections.  */

static bfd_boolean
elf32_sparc_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  struct elf32_sparc_link_hash_table *htab;
  bfd *dynobj;
  asection *s;
  bfd *ibfd;

  htab = elf32_sparc_hash_table (info);
  dynobj = htab->elf.dynobj;
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

  /* Set up .got offsets for local syms, and space for local dynamic
     relocs.  */
  for (ibfd = info->input_bfds; ibfd != NULL; ibfd = ibfd->link_next)
    {
      bfd_signed_vma *local_got;
      bfd_signed_vma *end_local_got;
      char *local_tls_type;
      bfd_size_type locsymcount;
      Elf_Internal_Shdr *symtab_hdr;
      asection *srel;

      if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour)
	continue;

      for (s = ibfd->sections; s != NULL; s = s->next)
	{
	  struct elf32_sparc_dyn_relocs *p;

	  for (p = *((struct elf32_sparc_dyn_relocs **)
		     &elf_section_data (s)->local_dynrel);
	       p != NULL;
	       p = p->next)
	    {
	      if (!bfd_is_abs_section (p->sec)
		  && bfd_is_abs_section (p->sec->output_section))
		{
		  /* Input section has been discarded, either because
		     it is a copy of a linkonce section or due to
		     linker script /DISCARD/, so we'll be discarding
		     the relocs too.  */
		}
	      else if (p->count != 0)
		{
		  srel = elf_section_data (p->sec)->sreloc;
		  srel->_raw_size += p->count * sizeof (Elf32_External_Rela);
		  if ((p->sec->output_section->flags & SEC_READONLY) != 0)
		    info->flags |= DF_TEXTREL;
		}
	    }
	}

      local_got = elf_local_got_refcounts (ibfd);
      if (!local_got)
	continue;

      symtab_hdr = &elf_tdata (ibfd)->symtab_hdr;
      locsymcount = symtab_hdr->sh_info;
      end_local_got = local_got + locsymcount;
      local_tls_type = elf32_sparc_local_got_tls_type (ibfd);
      s = htab->sgot;
      srel = htab->srelgot;
      for (; local_got < end_local_got; ++local_got, ++local_tls_type)
	{
	  if (*local_got > 0)
	    {
	      *local_got = s->_raw_size;
	      s->_raw_size += 4;
	      if (*local_tls_type == GOT_TLS_GD)
		s->_raw_size += 4;
	      if (info->shared
		  || *local_tls_type == GOT_TLS_GD
		  || *local_tls_type == GOT_TLS_IE)
		srel->_raw_size += sizeof (Elf32_External_Rela);
	    }
	  else
	    *local_got = (bfd_vma) -1;
	}
    }

  if (htab->tls_ldm_got.refcount > 0)
    {
      /* Allocate 2 got entries and 1 dynamic reloc for
	 R_SPARC_TLS_LDM_{HI22,LO10} relocs.  */
      htab->tls_ldm_got.offset = htab->sgot->_raw_size;
      htab->sgot->_raw_size += 8;
      htab->srelgot->_raw_size += sizeof (Elf32_External_Rela);
    }
  else
    htab->tls_ldm_got.offset = -1;

  /* Allocate global sym .plt and .got entries, and space for global
     sym dynamic relocs.  */
  elf_link_hash_traverse (&htab->elf, allocate_dynrelocs, (PTR) info);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Make space for the trailing nop in .plt.  */
      if (htab->splt->_raw_size > 0)
	htab->splt->_raw_size += 4;

      /* If the .got section is more than 0x1000 bytes, we add
	 0x1000 to the value of _GLOBAL_OFFSET_TABLE_, so that 13
	 bit relocations have a greater chance of working.  */
      if (htab->sgot->_raw_size >= 0x1000
	  && elf_hash_table (info)->hgot->root.u.def.value == 0)
	elf_hash_table (info)->hgot->root.u.def.value = 0x1000;
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      bfd_boolean strip = FALSE;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

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
	      strip = TRUE;
	    }
	  else
	    {
	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (s != htab->splt && s != htab->sgot)
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
	 values later, in elf32_sparc_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  _bfd_elf_add_dynamic_entry (info, TAG, VAL)

      if (info->executable)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return FALSE;
	}

      if (htab->srelplt->_raw_size != 0)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return FALSE;
	}

      if (!add_dynamic_entry (DT_RELA, 0)
	  || !add_dynamic_entry (DT_RELASZ, 0)
	  || !add_dynamic_entry (DT_RELAENT, sizeof (Elf32_External_Rela)))
	return FALSE;

      /* If any dynamic relocs apply to a read-only section,
	 then we need a DT_TEXTREL entry.  */
      if ((info->flags & DF_TEXTREL) == 0)
	elf_link_hash_traverse (&htab->elf, readonly_dynrelocs,
				(PTR) info);

      if (info->flags & DF_TEXTREL)
	{
	  if (!add_dynamic_entry (DT_TEXTREL, 0))
	    return FALSE;
	}
    }
#undef add_dynamic_entry

  return TRUE;
}

struct elf32_sparc_section_data
{
  struct bfd_elf_section_data elf;
  unsigned int do_relax;
};

#define sec_do_relax(sec) \
  ((struct elf32_sparc_section_data *) elf_section_data (sec))->do_relax

static bfd_boolean
elf32_sparc_new_section_hook (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  struct elf32_sparc_section_data *sdata;
  bfd_size_type amt = sizeof (*sdata);

  sdata = (struct elf32_sparc_section_data *) bfd_zalloc (abfd, amt);
  if (sdata == NULL)
    return FALSE;
  sec->used_by_bfd = (PTR) sdata;

  return _bfd_elf_new_section_hook (abfd, sec);
}

static bfd_boolean
elf32_sparc_relax_section (abfd, section, link_info, again)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
     bfd_boolean *again;
{
  *again = FALSE;
  sec_do_relax (section) = 1;
  return TRUE;
}

/* Return the base VMA address which should be subtracted from real addresses
   when resolving @dtpoff relocation.
   This is PT_TLS segment p_vaddr.  */

static bfd_vma
dtpoff_base (info)
     struct bfd_link_info *info;
{
  /* If tls_sec is NULL, we should have signalled an error already.  */
  if (elf_hash_table (info)->tls_sec == NULL)
    return 0;
  return elf_hash_table (info)->tls_sec->vma;
}

/* Return the relocation value for @tpoff relocation
   if STT_TLS virtual address is ADDRESS.  */

static bfd_vma
tpoff (info, address)
     struct bfd_link_info *info;
     bfd_vma address;
{
  struct elf_link_hash_table *htab = elf_hash_table (info);

  /* If tls_sec is NULL, we should have signalled an error already.  */
  if (htab->tls_sec == NULL)
    return 0;
  return address - htab->tls_size - htab->tls_sec->vma;
}

/* Relocate a SPARC ELF section.  */

static bfd_boolean
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
  struct elf32_sparc_link_hash_table *htab;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  bfd_vma got_base;
  asection *sreloc;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  if (info->relocatable)
    return TRUE;

  htab = elf32_sparc_hash_table (info);
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  local_got_offsets = elf_local_got_offsets (input_bfd);

  if (elf_hash_table (info)->hgot == NULL)
    got_base = 0;
  else
    got_base = elf_hash_table (info)->hgot->root.u.def.value;

  sreloc = elf_section_data (input_section)->sreloc;

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type, tls_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sec;
      bfd_vma relocation, off;
      bfd_reloc_status_type r;
      bfd_boolean is_plt = FALSE;
      bfd_boolean unresolved_reloc;

      r_type = ELF32_R_TYPE (rel->r_info);

      if (r_type == R_SPARC_GNU_VTINHERIT
	  || r_type == R_SPARC_GNU_VTENTRY)
	continue;

      if (r_type < 0 || r_type >= (int) R_SPARC_max_std)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
      howto = _bfd_sparc_elf_howto_table + r_type;

      /* This is a final link.  */
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
	  bfd_boolean warned ATTRIBUTE_UNUSED;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);
	}

      switch (r_type)
	{
	case R_SPARC_GOT10:
	case R_SPARC_GOT13:
	case R_SPARC_GOT22:
	  /* Relocation is to the entry for this symbol in the global
	     offset table.  */
	  if (htab->sgot == NULL)
	    abort ();

	  if (h != NULL)
	    {
	      bfd_boolean dyn;

	      off = h->got.offset;
	      BFD_ASSERT (off != (bfd_vma) -1);
	      dyn = elf_hash_table (info)->dynamic_sections_created;

	      if (! WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info->shared, h)
		  || (info->shared
		      && (info->symbolic
			  || h->dynindx == -1
			  || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL))
		      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
		{
		  /* This is actually a static link, or it is a
		     -Bsymbolic link and the symbol is defined
		     locally, or the symbol was forced to be local
		     because of a version file.  We must initialize
		     this entry in the global offset table.  Since the
		     offset must always be a multiple of 4, we use the
		     least significant bit to record whether we have
		     initialized it already.

		     When doing a dynamic link, we create a .rela.got
		     relocation entry to initialize the value.  This
		     is done in the finish_dynamic_symbol routine.  */
		  if ((off & 1) != 0)
		    off &= ~1;
		  else
		    {
		      bfd_put_32 (output_bfd, relocation,
				  htab->sgot->contents + off);
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
		 already processed this entry.  */
	      if ((off & 1) != 0)
		off &= ~1;
	      else
		{

		  if (info->shared)
		    {
		      asection *s;
		      Elf_Internal_Rela outrel;
		      bfd_byte *loc;

		      /* We need to generate a R_SPARC_RELATIVE reloc
			 for the dynamic linker.  */
		      s = htab->srelgot;
		      BFD_ASSERT (s != NULL);

		      outrel.r_offset = (htab->sgot->output_section->vma
					 + htab->sgot->output_offset
					 + off);
		      outrel.r_info = ELF32_R_INFO (0, R_SPARC_RELATIVE);
		      outrel.r_addend = relocation;
		      relocation = 0;
		      loc = s->contents;
		      loc += s->reloc_count++ * sizeof (Elf32_External_Rela);
		      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
		    }

		  bfd_put_32 (output_bfd, relocation,
			      htab->sgot->contents + off);
		  local_got_offsets[r_symndx] |= 1;
		}
	    }
	  relocation = htab->sgot->output_offset + off - got_base;
	  break;

	case R_SPARC_PLT32:
	  if (h == NULL || h->plt.offset == (bfd_vma) -1)
	    {
	      r_type = R_SPARC_32;
	      goto r_sparc_plt32;
	    }
	  /* Fall through.  */
	case R_SPARC_WPLT30:
	r_sparc_wplt30:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  /* The Solaris native assembler will generate a WPLT30 reloc
	     for a local symbol if you assemble a call from one
	     section to another when using -K pic.  We treat it as
	     WDISP30.  */
	  if (h == NULL)
	    break;

	  if (h->plt.offset == (bfd_vma) -1 || htab->splt == NULL)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
		 happens when statically linking PIC code, or when
		 using -Bsymbolic.  */
	      break;
	    }

	  relocation = (htab->splt->output_section->vma
			+ htab->splt->output_offset
			+ h->plt.offset);
	  unresolved_reloc = FALSE;
	  if (r_type == R_SPARC_PLT32)
	    {
	      r_type = R_SPARC_32;
	      is_plt = TRUE;
	      goto r_sparc_plt32;
	    }
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
	case R_SPARC_8:
	case R_SPARC_16:
	case R_SPARC_32:
	case R_SPARC_HI22:
	case R_SPARC_22:
	case R_SPARC_13:
	case R_SPARC_LO10:
	case R_SPARC_UA16:
	case R_SPARC_UA32:
	r_sparc_plt32:
	  /* r_symndx will be zero only for relocs against symbols
	     from removed linkonce sections, or sections discarded by
	     a linker script.  */
	  if (r_symndx == 0
	      || (input_section->flags & SEC_ALLOC) == 0)
	    break;

	  if ((info->shared
	       && (! howto->pc_relative
		   || (h != NULL
		       && h->dynindx != -1
		       && (! info->symbolic
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))))
	      || (!info->shared
		  && h != NULL
		  && h->dynindx != -1
		  && (h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0
		  && (((h->elf_link_hash_flags
			& ELF_LINK_HASH_DEF_DYNAMIC) != 0
		       && (h->elf_link_hash_flags
			   & ELF_LINK_HASH_DEF_REGULAR) == 0)
		      || h->root.type == bfd_link_hash_undefweak
		      || h->root.type == bfd_link_hash_undefined)))
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;
	      bfd_boolean skip, relocate = FALSE;

	      /* When generating a shared object, these relocations
		 are copied into the output file to be resolved at run
		 time.  */

	      BFD_ASSERT (sreloc != NULL);

	      skip = FALSE;

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info, input_section,
					 rel->r_offset);
	      if (outrel.r_offset == (bfd_vma) -1)
		skip = TRUE;
	      else if (outrel.r_offset == (bfd_vma) -2)
		skip = TRUE, relocate = TRUE;
	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      /* Optimize unaligned reloc usage now that we know where
		 it finally resides.  */
	      switch (r_type)
		{
		case R_SPARC_16:
		  if (outrel.r_offset & 1)
		    r_type = R_SPARC_UA16;
		  break;
		case R_SPARC_UA16:
		  if (!(outrel.r_offset & 1))
		    r_type = R_SPARC_16;
		  break;
		case R_SPARC_32:
		  if (outrel.r_offset & 3)
		    r_type = R_SPARC_UA32;
		  break;
		case R_SPARC_UA32:
		  if (!(outrel.r_offset & 3))
		    r_type = R_SPARC_32;
		  break;
	  	case R_SPARC_DISP8:
		case R_SPARC_DISP16:
	  	case R_SPARC_DISP32:
		  /* If the symbol is not dynamic, we should not keep
		     a dynamic relocation.  But an .rela.* slot has been
		     allocated for it, output R_SPARC_NONE.
		     FIXME: Add code tracking needed dynamic relocs as
		     e.g. i386 has.  */
		  if (h->dynindx == -1)
		    skip = TRUE, relocate = TRUE;
		  break;
		}

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      /* h->dynindx may be -1 if the symbol was marked to
		 become local.  */
	      else if (h != NULL && ! is_plt
		       && ((! info->symbolic && h->dynindx != -1)
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

		      if (is_plt)
			sec = htab->splt;

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

			  /* FIXME: we really should be able to link non-pic
			     shared libraries.  */
			  if (indx == 0)
			    {
			      BFD_FAIL ();
			      (*_bfd_error_handler)
				(_("%s: probably compiled without -fPIC?"),
				 bfd_archive_filename (input_bfd));
			      bfd_set_error (bfd_error_bad_value);
			      return FALSE;
			    }
			}

		      outrel.r_info = ELF32_R_INFO (indx, r_type);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		}

	      loc = sreloc->contents;
	      loc += sreloc->reloc_count++ * sizeof (Elf32_External_Rela);
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);

	      /* This reloc will be computed at runtime, so there's no
		 need to do anything now.  */
	      if (! relocate)
		continue;
	    }
	  break;

	case R_SPARC_TLS_GD_HI22:
	  if (! elf32_sparc_tdata (input_bfd)->has_tlsgd)
	    {
	      /* R_SPARC_REV32 used the same reloc number as
		 R_SPARC_TLS_GD_HI22.  */
	      r_type = R_SPARC_REV32;
	      break;
	    }
	  /* Fall through */

	case R_SPARC_TLS_GD_LO10:
	case R_SPARC_TLS_IE_HI22:
	case R_SPARC_TLS_IE_LO10:
	  r_type = elf32_sparc_tls_transition (info, input_bfd, r_type,
					       h == NULL);
	  tls_type = GOT_UNKNOWN;
	  if (h == NULL && local_got_offsets)
	    tls_type = elf32_sparc_local_got_tls_type (input_bfd) [r_symndx];
	  else if (h != NULL)
	    {
	      tls_type = elf32_sparc_hash_entry(h)->tls_type;
	      if (!info->shared && h->dynindx == -1 && tls_type == GOT_TLS_IE)
		switch (ELF32_R_TYPE (rel->r_info))
		  {
		  case R_SPARC_TLS_GD_HI22:
		  case R_SPARC_TLS_IE_HI22:
		    r_type = R_SPARC_TLS_LE_HIX22;
		    break;
		  default:
		    r_type = R_SPARC_TLS_LE_LOX10;
		    break;
		  }
	    }
	  if (tls_type == GOT_TLS_IE)
	    switch (r_type)
	      {
	      case R_SPARC_TLS_GD_HI22:
		r_type = R_SPARC_TLS_IE_HI22;
		break;
	      case R_SPARC_TLS_GD_LO10:
		r_type = R_SPARC_TLS_IE_LO10;
		break;
	      }

	  if (r_type == R_SPARC_TLS_LE_HIX22)
	    {
	      relocation = tpoff (info, relocation);
	      break;
	    }
	  if (r_type == R_SPARC_TLS_LE_LOX10)
	    {
	      /* Change add into xor.  */
	      relocation = tpoff (info, relocation);
	      bfd_put_32 (output_bfd, (bfd_get_32 (input_bfd,
						   contents + rel->r_offset)
				       | 0x80182000), contents + rel->r_offset);
	      break;
	    }

	  if (h != NULL)
	    {
	      off = h->got.offset;
	      h->got.offset |= 1;
	    }
	  else
	    {
	      BFD_ASSERT (local_got_offsets != NULL);
	      off = local_got_offsets[r_symndx];
	      local_got_offsets[r_symndx] |= 1;
	    }

	r_sparc_tlsldm:
	  if (htab->sgot == NULL)
	    abort ();

	  if ((off & 1) != 0)
	    off &= ~1;
	  else
	    {
	      Elf_Internal_Rela outrel;
	      Elf32_External_Rela *loc;
	      int dr_type, indx;

	      if (htab->srelgot == NULL)
		abort ();

	      bfd_put_32 (output_bfd, 0, htab->sgot->contents + off);
	      outrel.r_offset = (htab->sgot->output_section->vma
				 + htab->sgot->output_offset + off);
	      indx = h && h->dynindx != -1 ? h->dynindx : 0;
	      if (r_type == R_SPARC_TLS_IE_HI22
		|| r_type == R_SPARC_TLS_IE_LO10)
		dr_type = R_SPARC_TLS_TPOFF32;
	      else
		dr_type = R_SPARC_TLS_DTPMOD32;
	      if (dr_type == R_SPARC_TLS_TPOFF32 && indx == 0)
		outrel.r_addend = relocation - dtpoff_base (info);
	      else
		outrel.r_addend = 0;
	      outrel.r_info = ELF32_R_INFO (indx, dr_type);
	      loc = (Elf32_External_Rela *) htab->srelgot->contents;
	      loc += htab->srelgot->reloc_count++;
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel,
					 (bfd_byte *) loc);

	      if (r_type == R_SPARC_TLS_GD_HI22
		  || r_type == R_SPARC_TLS_GD_LO10)
		{
		  if (indx == 0)
		    {
	    	      BFD_ASSERT (! unresolved_reloc);
		      bfd_put_32 (output_bfd,
				  relocation - dtpoff_base (info),
				  htab->sgot->contents + off + 4);
		    }
		  else
		    {
		      bfd_put_32 (output_bfd, 0,
				  htab->sgot->contents + off + 4);
		      outrel.r_info = ELF32_R_INFO (indx,
						    R_SPARC_TLS_DTPOFF32);
		      outrel.r_offset += 4;
		      htab->srelgot->reloc_count++;
		      loc++;
		      bfd_elf32_swap_reloca_out (output_bfd, &outrel,
						 (bfd_byte *) loc);
		    }
		}
	      else if (dr_type == R_SPARC_TLS_DTPMOD32)
		{
		  bfd_put_32 (output_bfd, 0,
			      htab->sgot->contents + off + 4);
		}
	    }

	  if (off >= (bfd_vma) -2)
	    abort ();

	  relocation = htab->sgot->output_offset + off - got_base;
	  unresolved_reloc = FALSE;
	  howto = _bfd_sparc_elf_howto_table + r_type;
	  break;

	case R_SPARC_TLS_LDM_HI22:
	case R_SPARC_TLS_LDM_LO10:
	  if (! info->shared)
	    {
	      bfd_put_32 (output_bfd, SPARC_NOP, contents + rel->r_offset);
	      continue;
	    }
	  off = htab->tls_ldm_got.offset;
	  htab->tls_ldm_got.offset |= 1;
	  goto r_sparc_tlsldm;

	case R_SPARC_TLS_LDO_HIX22:
	case R_SPARC_TLS_LDO_LOX10:
	  if (info->shared)
	    relocation -= dtpoff_base (info);
	  else
	    relocation = tpoff (info, relocation);
	  break;

	case R_SPARC_TLS_LE_HIX22:
	case R_SPARC_TLS_LE_LOX10:
	  if (info->shared)
	    {
	      Elf_Internal_Rela outrel;
	      bfd_boolean skip, relocate = FALSE;

	      BFD_ASSERT (sreloc != NULL);
	      skip = FALSE;
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
	      else
		{
		  outrel.r_info = ELF32_R_INFO (0, r_type);
		  outrel.r_addend = relocation - dtpoff_base (info)
				    + rel->r_addend;
		}

	      bfd_elf32_swap_reloca_out (output_bfd, &outrel,
					 (bfd_byte *) (((Elf32_External_Rela *)
							sreloc->contents)
						       + sreloc->reloc_count));
	      ++sreloc->reloc_count;
	      continue;
	    }
	  relocation = tpoff (info, relocation);
	  break;

	case R_SPARC_TLS_LDM_CALL:
	  if (! info->shared)
	    {
	      /* mov %g0, %o0 */
	      bfd_put_32 (output_bfd, 0x90100000, contents + rel->r_offset);
	      continue;
	    }
	  /* Fall through */

	case R_SPARC_TLS_GD_CALL:
	  tls_type = GOT_UNKNOWN;
	  if (h == NULL && local_got_offsets)
	    tls_type = elf32_sparc_local_got_tls_type (input_bfd) [r_symndx];
	  else if (h != NULL)
	    tls_type = elf32_sparc_hash_entry(h)->tls_type;
	  if (! info->shared
	      || (r_type == R_SPARC_TLS_GD_CALL && tls_type == GOT_TLS_IE))
	    {
	      bfd_vma insn;

	      if (!info->shared && (h == NULL || h->dynindx == -1))
		{
		  /* GD -> LE */
		  bfd_put_32 (output_bfd, SPARC_NOP, contents + rel->r_offset);
		  continue;
		}

	      /* GD -> IE */
	      if (rel + 1 < relend
		  && ELF32_R_TYPE (rel[1].r_info) == R_SPARC_TLS_GD_ADD
		  && rel[1].r_offset == rel->r_offset + 4
		  && ELF32_R_SYM (rel[1].r_info) == r_symndx
		  && (((insn = bfd_get_32 (input_bfd,
					   contents + rel[1].r_offset))
		       >> 25) & 0x1f) == 8)
		{
		  /* We have
		     call __tls_get_addr, %tgd_call(foo)
		      add %reg1, %reg2, %o0, %tgd_add(foo)
		     and change it into IE:
		     ld [%reg1 + %reg2], %o0, %tie_ld(foo)
		     add %g7, %o0, %o0, %tie_add(foo).
		     add is 0x80000000 | (rd << 25) | (rs1 << 14) | rs2,
		     ld is 0xc0000000 | (rd << 25) | (rs1 << 14) | rs2.  */
		  bfd_put_32 (output_bfd, insn | 0xc0000000,
			      contents + rel->r_offset);
		  bfd_put_32 (output_bfd, 0x9001c008,
			      contents + rel->r_offset + 4);
		  rel++;
		  continue;
		}

	      bfd_put_32 (output_bfd, 0x9001c008, contents + rel->r_offset);
	      continue;
	    }

	  h = (struct elf_link_hash_entry *)
	      bfd_link_hash_lookup (info->hash, "__tls_get_addr", FALSE,
				    FALSE, TRUE);
	  BFD_ASSERT (h != NULL);
	  r_type = R_SPARC_WPLT30;
	  howto = _bfd_sparc_elf_howto_table + r_type;
	  goto r_sparc_wplt30;

	case R_SPARC_TLS_GD_ADD:
	  tls_type = GOT_UNKNOWN;
	  if (h == NULL && local_got_offsets)
	    tls_type = elf32_sparc_local_got_tls_type (input_bfd) [r_symndx];
	  else if (h != NULL)
	    tls_type = elf32_sparc_hash_entry(h)->tls_type;
	  if (! info->shared || tls_type == GOT_TLS_IE)
	    {
	      /* add %reg1, %reg2, %reg3, %tgd_add(foo)
		 changed into IE:
		 ld [%reg1 + %reg2], %reg3, %tie_ld(foo)
		 or LE:
		 add %g7, %reg2, %reg3.  */
	      bfd_vma insn = bfd_get_32 (input_bfd, contents + rel->r_offset);
	      if ((h != NULL && h->dynindx != -1) || info->shared)
		relocation = insn | 0xc0000000;
	      else
		relocation = (insn & ~0x7c000) | 0x1c000;
	      bfd_put_32 (output_bfd, relocation, contents + rel->r_offset);
	    }
	  continue;

	case R_SPARC_TLS_LDM_ADD:
	  if (! info->shared)
	    bfd_put_32 (output_bfd, SPARC_NOP, contents + rel->r_offset);
	  continue;

	case R_SPARC_TLS_LDO_ADD:
	  if (! info->shared)
	    {
	      /* Change rs1 into %g7.  */
	      bfd_vma insn = bfd_get_32 (input_bfd, contents + rel->r_offset);
	      insn = (insn & ~0x7c000) | 0x1c000;
	      bfd_put_32 (output_bfd, insn, contents + rel->r_offset);
	    }
	  continue;

	case R_SPARC_TLS_IE_LD:
	case R_SPARC_TLS_IE_LDX:
	  if (! info->shared && (h == NULL || h->dynindx == -1))
	    {
	      bfd_vma insn = bfd_get_32 (input_bfd, contents + rel->r_offset);
	      int rs2 = insn & 0x1f;
	      int rd = (insn >> 25) & 0x1f;

	      if (rs2 == rd)
		relocation = SPARC_NOP;
	      else
		relocation = 0x80100000 | (insn & 0x3e00001f);
	      bfd_put_32 (output_bfd, relocation, contents + rel->r_offset);
	    }
	  continue;

	case R_SPARC_TLS_IE_ADD:
	  /* Totally useless relocation.  */
	  continue;

	case R_SPARC_TLS_DTPOFF32:
	  relocation -= dtpoff_base (info);
	  break;

	default:
	  break;
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

      r = bfd_reloc_continue;
      if (r_type == R_SPARC_WDISP16)
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
      else if (r_type == R_SPARC_REV32)
	{
	  bfd_vma x;

	  relocation = relocation + rel->r_addend;

	  x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	  x = x + relocation;
	  bfd_putl32 (/*input_bfd,*/ x, contents + rel->r_offset);
	  r = bfd_reloc_ok;
	}
      else if (r_type == R_SPARC_TLS_LDO_HIX22
	       || r_type == R_SPARC_TLS_LE_HIX22)
	{
	  bfd_vma x;

	  relocation += rel->r_addend;
	  relocation = relocation ^ 0xffffffff;

	  x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	  x = (x & ~(bfd_vma) 0x3fffff) | ((relocation >> 10) & 0x3fffff);
	  bfd_put_32 (input_bfd, x, contents + rel->r_offset);
	  r = bfd_reloc_ok;
	}
      else if (r_type == R_SPARC_TLS_LDO_LOX10
	       || r_type == R_SPARC_TLS_LE_LOX10)
	{
	  bfd_vma x;

	  relocation += rel->r_addend;
	  relocation = (relocation & 0x3ff) | 0x1c00;

	  x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	  x = (x & ~(bfd_vma) 0x1fff) | relocation;
	  bfd_put_32 (input_bfd, x, contents + rel->r_offset);

	  r = bfd_reloc_ok;
	}
      else if ((r_type == R_SPARC_WDISP30 || r_type == R_SPARC_WPLT30)
	       && sec_do_relax (input_section)
	       && rel->r_offset + 4 < input_section->_raw_size)
	{
#define G0		0
#define O7		15
#define XCC		(2 << 20)
#define COND(x)		(((x)&0xf)<<25)
#define CONDA		COND(0x8)
#define INSN_BPA	(F2(0,1) | CONDA | BPRED | XCC)
#define INSN_BA		(F2(0,2) | CONDA)
#define INSN_OR		F3(2, 0x2, 0)
#define INSN_NOP	F2(0,4)

	  bfd_vma x, y;

	  /* If the instruction is a call with either:
	     restore
	     arithmetic instruction with rd == %o7
	     where rs1 != %o7 and rs2 if it is register != %o7
	     then we can optimize if the call destination is near
	     by changing the call into a branch always.  */
	  x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	  y = bfd_get_32 (input_bfd, contents + rel->r_offset + 4);
	  if ((x & OP(~0)) == OP(1) && (y & OP(~0)) == OP(2))
	    {
	      if (((y & OP3(~0)) == OP3(0x3d) /* restore */
		   || ((y & OP3(0x28)) == 0 /* arithmetic */
		       && (y & RD(~0)) == RD(O7)))
		  && (y & RS1(~0)) != RS1(O7)
		  && ((y & F3I(~0))
		      || (y & RS2(~0)) != RS2(O7)))
		{
		  bfd_vma reloc;

		  reloc = relocation + rel->r_addend - rel->r_offset;
		  reloc -= (input_section->output_section->vma
			   + input_section->output_offset);

		  /* Ensure the reloc fits into simm22.  */
		  if ((reloc & 3) == 0
		      && ((reloc & ~(bfd_vma)0x7fffff) == 0
			  || ((reloc | 0x7fffff) == ~(bfd_vma)0)))
		    {
		      reloc >>= 2;

		      /* Check whether it fits into simm19 on v9.  */
		      if (((reloc & 0x3c0000) == 0
			   || (reloc & 0x3c0000) == 0x3c0000)
			  && (elf_elfheader (output_bfd)->e_flags & EF_SPARC_32PLUS))
			x = INSN_BPA | (reloc & 0x7ffff); /* ba,pt %xcc */
		      else
			x = INSN_BA | (reloc & 0x3fffff); /* ba */
		      bfd_put_32 (input_bfd, x, contents + rel->r_offset);
		      r = bfd_reloc_ok;
		      if (rel->r_offset >= 4
			  && (y & (0xffffffff ^ RS1(~0)))
			      == (INSN_OR | RD(O7) | RS2(G0)))
			{
			  bfd_vma z;
			  unsigned int reg;

			  z = bfd_get_32 (input_bfd,
					  contents + rel->r_offset - 4);
			  if ((z & (0xffffffff ^ RD(~0)))
			      != (INSN_OR | RS1(O7) | RS2(G0)))
			    break;

			  /* The sequence was
			     or %o7, %g0, %rN
			     call foo
			     or %rN, %g0, %o7

			     If call foo was replaced with ba, replace
			     or %rN, %g0, %o7 with nop.  */

			  reg = (y & RS1(~0)) >> 14;
			  if (reg != ((z & RD(~0)) >> 25)
			      || reg == G0 || reg == O7)
			    break;

			  bfd_put_32 (input_bfd, (bfd_vma) INSN_NOP,
				      contents + rel->r_offset + 4);
			}

		    }
		}
	    }
	}

      if (r == bfd_reloc_continue)
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
		if (! ((*info->callbacks->reloc_overflow)
		       (info, name, howto->name, (bfd_vma) 0,
			input_bfd, input_section, rel->r_offset)))
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
elf32_sparc_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;
  struct elf32_sparc_link_hash_table *htab;

  htab = elf32_sparc_hash_table (info);
  dynobj = htab->elf.dynobj;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *srela;
      Elf_Internal_Rela rela;
      bfd_byte *loc;

      /* This symbol has an entry in the procedure linkage table.  Set
	 it up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = htab->splt;
      srela = htab->srelplt;
      BFD_ASSERT (splt != NULL && srela != NULL);

      /* Fill in the entry in the procedure linkage table.  */
      bfd_put_32 (output_bfd,
		  PLT_ENTRY_WORD0 + h->plt.offset,
		  splt->contents + h->plt.offset);
      bfd_put_32 (output_bfd,
		  (PLT_ENTRY_WORD1
		   + (((- (h->plt.offset + 4)) >> 2) & 0x3fffff)),
		  splt->contents + h->plt.offset + 4);
      bfd_put_32 (output_bfd, (bfd_vma) PLT_ENTRY_WORD2,
		  splt->contents + h->plt.offset + 8);

      /* Fill in the entry in the .rela.plt section.  */
      rela.r_offset = (splt->output_section->vma
		       + splt->output_offset
		       + h->plt.offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_SPARC_JMP_SLOT);
      rela.r_addend = 0;
      loc = srela->contents;
      loc += (h->plt.offset / PLT_ENTRY_SIZE - 4) * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	  /* If the symbol is weak, we do need to clear the value.
	     Otherwise, the PLT entry would provide a definition for
	     the symbol even if the symbol wasn't defined anywhere,
	     and so the symbol would never be NULL.  */
	  if ((h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR_NONWEAK)
	      == 0)
	    sym->st_value = 0;
	}
    }

  if (h->got.offset != (bfd_vma) -1
      && elf32_sparc_hash_entry(h)->tls_type != GOT_TLS_GD
      && elf32_sparc_hash_entry(h)->tls_type != GOT_TLS_IE)
    {
      asection *sgot;
      asection *srela;
      Elf_Internal_Rela rela;
      bfd_byte *loc;

      /* This symbol has an entry in the global offset table.  Set it
	 up.  */

      sgot = htab->sgot;
      srela = htab->srelgot;
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
	  && (info->symbolic || h->dynindx == -1)
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	{
	  asection *sec = h->root.u.def.section;
	  rela.r_info = ELF32_R_INFO (0, R_SPARC_RELATIVE);
	  rela.r_addend = (h->root.u.def.value
			   + sec->output_section->vma
			   + sec->output_offset);
	}
      else
	{
	  rela.r_info = ELF32_R_INFO (h->dynindx, R_SPARC_GLOB_DAT);
	  rela.r_addend = 0;
	}

      bfd_put_32 (output_bfd, (bfd_vma) 0,
		  sgot->contents + (h->got.offset &~ (bfd_vma) 1));
      loc = srela->contents;
      loc += srela->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      asection *s;
      Elf_Internal_Rela rela;
      bfd_byte *loc;

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
      loc = s->contents + s->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
    }

  /* Mark some specially defined symbols as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0
      || strcmp (h->root.root.string, "_PROCEDURE_LINKAGE_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}

/* Finish up the dynamic sections.  */

static bfd_boolean
elf32_sparc_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;
  struct elf32_sparc_link_hash_table *htab;

  htab = elf32_sparc_hash_table (info);
  dynobj = htab->elf.dynobj;

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
	  bfd_boolean size;

	  bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:   name = ".plt"; size = FALSE; break;
	    case DT_PLTRELSZ: name = ".rela.plt"; size = TRUE; break;
	    case DT_JMPREL:   name = ".rela.plt"; size = FALSE; break;
	    default:	  name = NULL; size = FALSE; break;
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
	  bfd_put_32 (output_bfd, (bfd_vma) SPARC_NOP,
		      splt->contents + splt->_raw_size - 4);
	}

      elf_section_data (splt->output_section)->this_hdr.sh_entsize = 0;
    }

  /* Set the first entry in the global offset table to the address of
     the dynamic section.  */
  if (htab->sgot && htab->sgot->_raw_size > 0)
    {
      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, htab->sgot->contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    htab->sgot->contents);
    }

  if (htab->sgot)
    elf_section_data (htab->sgot->output_section)->this_hdr.sh_entsize = 4;

  return TRUE;
}

/* Functions for dealing with the e_flags field.

   We don't define set_private_flags or copy_private_bfd_data because
   the only currently defined values are based on the bfd mach number,
   so we use the latter instead and defer setting e_flags until the
   file is written out.  */

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static bfd_boolean
elf32_sparc_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  bfd_boolean error;
  /* FIXME: This should not be static.  */
  static unsigned long previous_ibfd_e_flags = (unsigned long) -1;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  error = FALSE;

  if (bfd_get_mach (ibfd) >= bfd_mach_sparc_v9)
    {
      error = TRUE;
      (*_bfd_error_handler)
	(_("%s: compiled for a 64 bit system and target is 32 bit"),
	 bfd_archive_filename (ibfd));
    }
  else if ((ibfd->flags & DYNAMIC) == 0)
    {
      if (bfd_get_mach (obfd) < bfd_get_mach (ibfd))
	bfd_set_arch_mach (obfd, bfd_arch_sparc, bfd_get_mach (ibfd));
    }

  if (((elf_elfheader (ibfd)->e_flags & EF_SPARC_LEDATA)
       != previous_ibfd_e_flags)
      && previous_ibfd_e_flags != (unsigned long) -1)
    {
      (*_bfd_error_handler)
	(_("%s: linking little endian files with big endian files"),
	 bfd_archive_filename (ibfd));
      error = TRUE;
    }
  previous_ibfd_e_flags = elf_elfheader (ibfd)->e_flags & EF_SPARC_LEDATA;

  if (error)
    {
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  return TRUE;
}

/* Set the right machine number.  */

static bfd_boolean
elf32_sparc_object_p (abfd)
     bfd *abfd;
{
  if (elf_elfheader (abfd)->e_machine == EM_SPARC32PLUS)
    {
      if (elf_elfheader (abfd)->e_flags & EF_SPARC_SUN_US3)
	return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
					  bfd_mach_sparc_v8plusb);
      else if (elf_elfheader (abfd)->e_flags & EF_SPARC_SUN_US1)
	return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
					  bfd_mach_sparc_v8plusa);
      else if (elf_elfheader (abfd)->e_flags & EF_SPARC_32PLUS)
	return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
					  bfd_mach_sparc_v8plus);
      else
	return FALSE;
    }
  else if (elf_elfheader (abfd)->e_flags & EF_SPARC_LEDATA)
    return bfd_default_set_arch_mach (abfd, bfd_arch_sparc,
				      bfd_mach_sparc_sparclite_le);
  else
    return bfd_default_set_arch_mach (abfd, bfd_arch_sparc, bfd_mach_sparc);
}

/* The final processing done just before writing out the object file.
   We need to set the e_machine field appropriately.  */

static void
elf32_sparc_final_write_processing (abfd, linker)
     bfd *abfd;
     bfd_boolean linker ATTRIBUTE_UNUSED;
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
    case bfd_mach_sparc_v8plusb :
      elf_elfheader (abfd)->e_machine = EM_SPARC32PLUS;
      elf_elfheader (abfd)->e_flags &=~ EF_SPARC_32PLUS_MASK;
      elf_elfheader (abfd)->e_flags |= EF_SPARC_32PLUS | EF_SPARC_SUN_US1
				       | EF_SPARC_SUN_US3;
      break;
    case bfd_mach_sparc_sparclite_le :
      elf_elfheader (abfd)->e_machine = EM_SPARC;
      elf_elfheader (abfd)->e_flags |= EF_SPARC_LEDATA;
      break;
    default :
      abort ();
      break;
    }
}

static enum elf_reloc_type_class
elf32_sparc_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  switch ((int) ELF32_R_TYPE (rela->r_info))
    {
    case R_SPARC_RELATIVE:
      return reloc_class_relative;
    case R_SPARC_JMP_SLOT:
      return reloc_class_plt;
    case R_SPARC_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

#define TARGET_BIG_SYM	bfd_elf32_sparc_vec
#define TARGET_BIG_NAME	"elf32-sparc"
#define ELF_ARCH	bfd_arch_sparc
#define ELF_MACHINE_CODE EM_SPARC
#define ELF_MACHINE_ALT1 EM_SPARC32PLUS
#define ELF_MAXPAGESIZE 0x10000

#define bfd_elf32_bfd_reloc_type_lookup	elf32_sparc_reloc_type_lookup
#define bfd_elf32_bfd_link_hash_table_create \
					elf32_sparc_link_hash_table_create
#define bfd_elf32_bfd_relax_section	elf32_sparc_relax_section
#define bfd_elf32_new_section_hook	elf32_sparc_new_section_hook
#define elf_info_to_howto		elf32_sparc_info_to_howto
#define elf_backend_copy_indirect_symbol \
					elf32_sparc_copy_indirect_symbol
#define elf_backend_create_dynamic_sections \
					elf32_sparc_create_dynamic_sections
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
#define bfd_elf32_mkobject		elf32_sparc_mkobject
#define elf_backend_object_p		elf32_sparc_object_p
#define elf_backend_final_write_processing \
					elf32_sparc_final_write_processing
#define elf_backend_gc_mark_hook	elf32_sparc_gc_mark_hook
#define elf_backend_gc_sweep_hook       elf32_sparc_gc_sweep_hook
#define elf_backend_grok_psinfo		elf32_sparc_grok_psinfo
#define elf_backend_reloc_type_class	elf32_sparc_reloc_type_class

#define elf_backend_can_gc_sections 1
#define elf_backend_can_refcount 1
#define elf_backend_want_got_plt 0
#define elf_backend_plt_readonly 0
#define elf_backend_want_plt_sym 1
#define elf_backend_got_header_size 4
#define elf_backend_rela_normal 1

#include "elf32-target.h"
