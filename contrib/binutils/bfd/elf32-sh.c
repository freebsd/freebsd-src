/* Hitachi SH specific support for 32-bit ELF
   Copyright 1996, 1997, 1998 Free Software Foundation, Inc.
   Contributed by Ian Lance Taylor, Cygnus Support.

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

static bfd_reloc_status_type sh_elf_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type sh_elf_ignore_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *sh_elf_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void sh_elf_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static boolean sh_elf_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, boolean *));
static boolean sh_elf_relax_delete_bytes
  PARAMS ((bfd *, asection *, bfd_vma, int));
static boolean sh_elf_align_loads
  PARAMS ((bfd *, asection *, Elf_Internal_Rela *, bfd_byte *, boolean *));
static boolean sh_elf_swap_insns
  PARAMS ((bfd *, asection *, PTR, bfd_byte *, bfd_vma));
static boolean sh_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static bfd_byte *sh_elf_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *, boolean, asymbol **));

enum sh_reloc_type
{
  R_SH_NONE = 0,
  R_SH_DIR32,
  R_SH_REL32,
  R_SH_DIR8WPN,
  R_SH_IND12W,
  R_SH_DIR8WPL,
  R_SH_DIR8WPZ,
  R_SH_DIR8BP,
  R_SH_DIR8W,
  R_SH_DIR8L,
  FIRST_INVALID_RELOC,
  LAST_INVALID_RELOC = 24,
  /* The remaining relocs are a GNU extension used for relaxation.  We
     use the same constants as COFF uses, not that it really matters.  */
  R_SH_SWITCH16 = 25,
  R_SH_SWITCH32,
  R_SH_USES,
  R_SH_COUNT,
  R_SH_ALIGN,
  R_SH_CODE,
  R_SH_DATA,
  R_SH_LABEL,
  R_SH_max
};

static reloc_howto_type sh_elf_howto_table[] =
{
  /* No relocation.  */
  HOWTO (R_SH_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit absolute relocation.  Setting partial_inplace to true and
     src_mask to a non-zero value is similar to the COFF toolchain.  */
  HOWTO (R_SH_DIR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_DIR32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit PC relative relocation.  */
  HOWTO (R_SH_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_REL32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 8 bit PC relative branch divided by 2.  */
  HOWTO (R_SH_DIR8WPN,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_DIR8WPN",	/* name */
	 true,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 12 bit PC relative branch divided by 2.  */
  HOWTO (R_SH_IND12W,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_IND12W",		/* name */
	 true,			/* partial_inplace */
	 0xfff,			/* src_mask */
	 0xfff,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 8 bit unsigned PC relative divided by 4.  */
  HOWTO (R_SH_DIR8WPL,		/* type */
	 2,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_DIR8WPL",	/* name */
	 true,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 8 bit unsigned PC relative divided by 2.  */
  HOWTO (R_SH_DIR8WPZ,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_DIR8WPZ",	/* name */
	 true,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 8 bit GBR relative.  FIXME: This only makes sense if we have some
     special symbol for the GBR relative area, and that is not
     implemented.  */
  HOWTO (R_SH_DIR8BP,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_DIR8BP",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 8 bit GBR relative divided by 2.  FIXME: This only makes sense if
     we have some special symbol for the GBR relative area, and that
     is not implemented.  */
  HOWTO (R_SH_DIR8W,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_DIR8W",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 8 bit GBR relative divided by 4.  FIXME: This only makes sense if
     we have some special symbol for the GBR relative area, and that
     is not implemented.  */
  HOWTO (R_SH_DIR8L,		/* type */
	 2,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_reloc,		/* special_function */
	 "R_SH_DIR8L",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */

  { 10 },
  { 11 },
  { 12 },
  { 13 },
  { 14 },
  { 15 },
  { 16 },
  { 17 },
  { 18 },
  { 19 },
  { 20 },
  { 21 },
  { 22 },
  { 23 },
  { 24 },

  /* The remaining relocs are a GNU extension used for relaxing.  The
     final pass of the linker never needs to do anything with any of
     these relocs.  Any required operations are handled by the
     relaxation code.  */

  /* A 16 bit switch table entry.  This is generated for an expression
     such as ``.word L1 - L2''.  The offset holds the difference
     between the reloc address and L2.  */
  HOWTO (R_SH_SWITCH16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_SWITCH16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 32 bit switch table entry.  This is generated for an expression
     such as ``.long L1 - L2''.  The offset holds the difference
     between the reloc address and L2.  */
  HOWTO (R_SH_SWITCH32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_SWITCH32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* Indicates a .uses pseudo-op.  The compiler will generate .uses
     pseudo-ops when it finds a function call which can be relaxed.
     The offset field holds the PC relative offset to the instruction
     which loads the register used in the function call.  */
  HOWTO (R_SH_USES,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_USES",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* The assembler will generate this reloc for addresses referred to
     by the register loads associated with USES relocs.  The offset
     field holds the number of times the address is referenced in the
     object file.  */
  HOWTO (R_SH_COUNT,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_COUNT",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* Indicates an alignment statement.  The offset field is the power
     of 2 to which subsequent portions of the object file must be
     aligned.  */
  HOWTO (R_SH_ALIGN,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_ALIGN",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* The assembler will generate this reloc before a block of
     instructions.  A section should be processed as assumining it
     contains data, unless this reloc is seen.  */
  HOWTO (R_SH_CODE,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_CODE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* The assembler will generate this reloc after a block of
     instructions when it sees data that is not instructions.  */
  HOWTO (R_SH_DATA,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_DATA",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* The assembler generates this reloc for each label within a block
     of instructions.  This permits the linker to avoid swapping
     instructions which are the targets of branches.  */
  HOWTO (R_SH_LABEL,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_LABEL",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true)			/* pcrel_offset */
};

/* This function is used for normal relocs.  This is like the COFF
   function, and is almost certainly incorrect for other ELF targets.  */

static bfd_reloc_status_type
sh_elf_reloc (abfd, reloc_entry, symbol_in, data, input_section, output_bfd,
	  error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol_in;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  unsigned long insn;
  bfd_vma sym_value;
  enum sh_reloc_type r_type;
  bfd_vma addr = reloc_entry->address;
  bfd_byte *hit_data = addr + (bfd_byte *) data;

  r_type = (enum sh_reloc_type) reloc_entry->howto->type;

  if (output_bfd != NULL)
    {
      /* Partial linking--do nothing.  */
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* Almost all relocs have to do with relaxing.  If any work must be
     done for them, it has been done in sh_relax_section.  */
  if (r_type != R_SH_DIR32
      && (r_type != R_SH_IND12W
	  || (symbol_in->flags & BSF_LOCAL) != 0))
    return bfd_reloc_ok;

  if (symbol_in != NULL
      && bfd_is_und_section (symbol_in->section))
    return bfd_reloc_undefined;

  if (bfd_is_com_section (symbol_in->section))
    sym_value = 0;                           
  else 
    sym_value = (symbol_in->value +
		 symbol_in->section->output_section->vma +
		 symbol_in->section->output_offset);

  switch (r_type)
    {
    case R_SH_DIR32:
      insn = bfd_get_32 (abfd, hit_data);
      insn += sym_value + reloc_entry->addend;
      bfd_put_32 (abfd, insn, hit_data);
      break;
    case R_SH_IND12W:
      insn = bfd_get_16 (abfd, hit_data);
      sym_value += reloc_entry->addend;
      sym_value -= (input_section->output_section->vma
		    + input_section->output_offset
		    + addr
		    + 4);
      sym_value += (insn & 0xfff) << 1;
      if (insn & 0x800)
	sym_value -= 0x1000;
      insn = (insn & 0xf000) | (sym_value & 0xfff);
      bfd_put_16 (abfd, insn, hit_data);
      if (sym_value < (bfd_vma) -0x1000 || sym_value >= 0x1000)
	return bfd_reloc_overflow;
      break;
    default:
      abort ();
      break;
    }

  return bfd_reloc_ok;
}

/* This function is used for relocs which are only used for relaxing,
   which the linker should otherwise ignore.  */

static bfd_reloc_status_type
sh_elf_ignore_reloc (abfd, reloc_entry, symbol, data, input_section,
		     output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  if (output_bfd != NULL)
    reloc_entry->address += input_section->output_offset;
  return bfd_reloc_ok;
}

/* This structure is used to map BFD reloc codes to SH ELF relocs.  */

struct elf_reloc_map
{
  unsigned char bfd_reloc_val;
  unsigned char elf_reloc_val;
};

/* An array mapping BFD reloc codes to SH ELF relocs.  */

static const struct elf_reloc_map sh_reloc_map[] =
{
  { BFD_RELOC_NONE, R_SH_NONE },
  { BFD_RELOC_32, R_SH_DIR32 },
  { BFD_RELOC_CTOR, R_SH_DIR32 },
  { BFD_RELOC_32_PCREL, R_SH_REL32 },
  { BFD_RELOC_SH_PCDISP8BY2, R_SH_DIR8WPN },
  { BFD_RELOC_SH_PCDISP12BY2, R_SH_IND12W },
  { BFD_RELOC_SH_PCRELIMM8BY2, R_SH_DIR8WPZ },
  { BFD_RELOC_SH_PCRELIMM8BY4, R_SH_DIR8WPL },
  { BFD_RELOC_SH_SWITCH16, R_SH_SWITCH16 },
  { BFD_RELOC_SH_SWITCH32, R_SH_SWITCH32 },
  { BFD_RELOC_SH_USES, R_SH_USES },
  { BFD_RELOC_SH_COUNT, R_SH_COUNT },
  { BFD_RELOC_SH_ALIGN, R_SH_ALIGN },
  { BFD_RELOC_SH_CODE, R_SH_CODE },
  { BFD_RELOC_SH_DATA, R_SH_DATA },
  { BFD_RELOC_SH_LABEL, R_SH_LABEL }
};

/* Given a BFD reloc code, return the howto structure for the
   corresponding SH ELf reloc.  */

static reloc_howto_type *
sh_elf_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0; i < sizeof (sh_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (sh_reloc_map[i].bfd_reloc_val == code)
	return &sh_elf_howto_table[(int) sh_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Given an ELF reloc, fill in the howto field of a relent.  */

static void
sh_elf_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r;

  r = ELF32_R_TYPE (dst->r_info);

  BFD_ASSERT (r < (unsigned int) R_SH_max);
  BFD_ASSERT (r < FIRST_INVALID_RELOC || r > LAST_INVALID_RELOC);

  cache_ptr->howto = &sh_elf_howto_table[r];
}

/* This function handles relaxing for SH ELF.  See the corresponding
   function in coff-sh.c for a description of what this does.  FIXME:
   There is a lot of duplication here between this code and the COFF
   specific code.  The format of relocs and symbols is wound deeply
   into this code, but it would still be better if the duplication
   could be eliminated somehow.  Note in particular that although both
   functions use symbols like R_SH_CODE, those symbols have different
   values; in coff-sh.c they come from include/coff/sh.h, whereas here
   they come from enum sh_reloc_type in this file.  */

static boolean 
sh_elf_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     boolean *again;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *free_relocs = NULL;
  boolean have_code;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  bfd_byte *free_contents = NULL;
  Elf32_External_Sym *extsyms = NULL;
  Elf32_External_Sym *free_extsyms = NULL;

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

  internal_relocs = (_bfd_elf32_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;
  if (! link_info->keep_memory)
    free_relocs = internal_relocs;

  have_code = false;

  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma laddr, paddr, symval;
      unsigned short insn;
      Elf_Internal_Rela *irelfn, *irelscan, *irelcount;
      bfd_signed_vma foff;

      if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_CODE)
	have_code = true;

      if (ELF32_R_TYPE (irel->r_info) != (int) R_SH_USES)
	continue;

      /* Get the section contents.  */
      if (contents == NULL)
	{
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
	}

      /* The r_addend field of the R_SH_USES reloc will point us to
         the register load.  The 4 is because the r_addend field is
         computed as though it were a jump offset, which are based
         from 4 bytes after the jump instruction.  */
      laddr = irel->r_offset + 4 + irel->r_addend;
      if (laddr >= sec->_raw_size)
	{
	  (*_bfd_error_handler) ("%s: 0x%lx: warning: bad R_SH_USES offset",
				 bfd_get_filename (abfd),
				 (unsigned long) irel->r_offset);
	  continue;
	}
      insn = bfd_get_16 (abfd, contents + laddr);

      /* If the instruction is not mov.l NN,rN, we don't know what to
         do.  */
      if ((insn & 0xf000) != 0xd000)
	{
	  ((*_bfd_error_handler)
	   ("%s: 0x%lx: warning: R_SH_USES points to unrecognized insn 0x%x",
	    bfd_get_filename (abfd), (unsigned long) irel->r_offset, insn));
	  continue;
	}

      /* Get the address from which the register is being loaded.  The
      	 displacement in the mov.l instruction is quadrupled.  It is a
      	 displacement from four bytes after the movl instruction, but,
      	 before adding in the PC address, two least significant bits
      	 of the PC are cleared.  We assume that the section is aligned
      	 on a four byte boundary.  */
      paddr = insn & 0xff;
      paddr *= 4;
      paddr += (laddr + 4) &~ 3;
      if (paddr >= sec->_raw_size)
	{
	  ((*_bfd_error_handler)
	   ("%s: 0x%lx: warning: bad R_SH_USES load offset",
	    bfd_get_filename (abfd), (unsigned long) irel->r_offset));
	  continue;
	}

      /* Get the reloc for the address from which the register is
         being loaded.  This reloc will tell us which function is
         actually being called.  */
      for (irelfn = internal_relocs; irelfn < irelend; irelfn++)
	if (irelfn->r_offset == paddr
	    && ELF32_R_TYPE (irelfn->r_info) == (int) R_SH_DIR32)
	  break;
      if (irelfn >= irelend)
	{
	  ((*_bfd_error_handler)
	   ("%s: 0x%lx: warning: could not find expected reloc",
	    bfd_get_filename (abfd), (unsigned long) paddr));
	  continue;
	}

      /* Read this BFD's symbols if we haven't done so already.  */
      if (extsyms == NULL)
	{
	  if (symtab_hdr->contents != NULL)
	    extsyms = (Elf32_External_Sym *) symtab_hdr->contents;
	  else
	    {
	      extsyms = ((Elf32_External_Sym *)
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
      if (ELF32_R_SYM (irelfn->r_info) < symtab_hdr->sh_info)
	{
	  Elf_Internal_Sym isym;

	  /* A local symbol.  */
	  bfd_elf32_swap_symbol_in (abfd,
				    extsyms + ELF32_R_SYM (irelfn->r_info),
				    &isym);

	  if (isym.st_shndx != _bfd_elf_section_from_bfd_section (abfd, sec))
	    {
	      ((*_bfd_error_handler)
	       ("%s: 0x%lx: warning: symbol in unexpected section",
		bfd_get_filename (abfd), (unsigned long) paddr));
	      continue;
	    }

	  symval = (isym.st_value
		    + sec->output_section->vma
		    + sec->output_offset);
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;

	  indx = ELF32_R_SYM (irelfn->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);
	  if (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	    {
	      /* This appears to be a reference to an undefined
                 symbol.  Just ignore it--it will be caught by the
                 regular reloc processing.  */
	      continue;
	    }

	  symval = (h->root.u.def.value
		    + h->root.u.def.section->output_section->vma
		    + h->root.u.def.section->output_offset);
	}

      symval += bfd_get_32 (abfd, contents + paddr);

      /* See if this function call can be shortened.  */
      foff = (symval
	      - (irel->r_offset
		 + sec->output_section->vma
		 + sec->output_offset
		 + 4));
      if (foff < -0x1000 || foff >= 0x1000)
	{
	  /* After all that work, we can't shorten this function call.  */
	  continue;
	}

      /* Shorten the function call.  */

      /* For simplicity of coding, we are going to modify the section
	 contents, the section relocs, and the BFD symbol table.  We
	 must tell the rest of the code not to free up this
	 information.  It would be possible to instead create a table
	 of changes which have to be made, as is done in coff-mips.c;
	 that would be more work, but would require less memory when
	 the linker is run.  */

      elf_section_data (sec)->relocs = internal_relocs;
      free_relocs = NULL;

      elf_section_data (sec)->this_hdr.contents = contents;
      free_contents = NULL;

      symtab_hdr->contents = (bfd_byte *) extsyms;
      free_extsyms = NULL;

      /* Replace the jsr with a bsr.  */

      /* Change the R_SH_USES reloc into an R_SH_IND12W reloc, and
         replace the jsr with a bsr.  */
      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irelfn->r_info), R_SH_IND12W);
      if (ELF32_R_SYM (irelfn->r_info) < symtab_hdr->sh_info)
	{
	  /* If this needs to be changed because of future relaxing,
             it will be handled here like other internal IND12W
             relocs.  */
	  bfd_put_16 (abfd,
		      0xb000 | ((foff >> 1) & 0xfff),
		      contents + irel->r_offset);
	}
      else
	{
	  /* We can't fully resolve this yet, because the external
             symbol value may be changed by future relaxing.  We let
             the final link phase handle it.  */
	  bfd_put_16 (abfd, 0xb000, contents + irel->r_offset);
	}

      /* See if there is another R_SH_USES reloc referring to the same
         register load.  */
      for (irelscan = internal_relocs; irelscan < irelend; irelscan++)
	if (ELF32_R_TYPE (irelscan->r_info) == (int) R_SH_USES
	    && laddr == irelscan->r_offset + 4 + irelscan->r_addend)
	  break;
      if (irelscan < irelend)
	{
	  /* Some other function call depends upon this register load,
	     and we have not yet converted that function call.
	     Indeed, we may never be able to convert it.  There is
	     nothing else we can do at this point.  */
	  continue;
	}

      /* Look for a R_SH_COUNT reloc on the location where the
         function address is stored.  Do this before deleting any
         bytes, to avoid confusion about the address.  */
      for (irelcount = internal_relocs; irelcount < irelend; irelcount++)
	if (irelcount->r_offset == paddr
	    && ELF32_R_TYPE (irelcount->r_info) == (int) R_SH_COUNT)
	  break;

      /* Delete the register load.  */
      if (! sh_elf_relax_delete_bytes (abfd, sec, laddr, 2))
	goto error_return;

      /* That will change things, so, just in case it permits some
         other function call to come within range, we should relax
         again.  Note that this is not required, and it may be slow.  */
      *again = true;

      /* Now check whether we got a COUNT reloc.  */
      if (irelcount >= irelend)
	{
	  ((*_bfd_error_handler)
	   ("%s: 0x%lx: warning: could not find expected COUNT reloc",
	    bfd_get_filename (abfd), (unsigned long) paddr));
	  continue;
	}

      /* The number of uses is stored in the r_addend field.  We've
         just deleted one.  */
      if (irelcount->r_addend == 0)
	{
	  ((*_bfd_error_handler) ("%s: 0x%lx: warning: bad count",
				  bfd_get_filename (abfd),
				  (unsigned long) paddr));
	  continue;
	}

      --irelcount->r_addend;

      /* If there are no more uses, we can delete the address.  Reload
         the address from irelfn, in case it was changed by the
         previous call to sh_elf_relax_delete_bytes.  */
      if (irelcount->r_addend == 0)
	{
	  if (! sh_elf_relax_delete_bytes (abfd, sec, irelfn->r_offset, 4))
	    goto error_return;
	}

      /* We've done all we can with that function call.  */
    }

  /* Look for load and store instructions that we can align on four
     byte boundaries.  */
  if (have_code)
    {
      boolean swapped;

      /* Get the section contents.  */
      if (contents == NULL)
	{
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
	}

      if (! sh_elf_align_loads (abfd, sec, internal_relocs, contents,
				&swapped))
	goto error_return;

      if (swapped)
	{
	  elf_section_data (sec)->relocs = internal_relocs;
	  free_relocs = NULL;

	  elf_section_data (sec)->this_hdr.contents = contents;
	  free_contents = NULL;

	  symtab_hdr->contents = (bfd_byte *) extsyms;
	  free_extsyms = NULL;
	}
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
	  symtab_hdr->contents = extsyms;
	}
      free_extsyms = NULL;
    }

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

/* Delete some bytes from a section while relaxing.  FIXME: There is a
   lot of duplication between this function and sh_relax_delete_bytes
   in coff-sh.c.  */

static boolean
sh_elf_relax_delete_bytes (abfd, sec, addr, count)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
     int count;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf32_External_Sym *extsyms;
  int shndx, index;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Rela *irelalign;
  bfd_vma toaddr;
  Elf32_External_Sym *esym, *esymend;
  struct elf_link_hash_entry *sym_hash;
  asection *o;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  extsyms = (Elf32_External_Sym *) symtab_hdr->contents;

  shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->_cooked_size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;
  for (; irel < irelend; irel++)
    {
      if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_ALIGN
	  && irel->r_offset > addr
	  && count < (1 << irel->r_addend))
	{
	  irelalign = irel;
	  toaddr = irel->r_offset;
	  break;
	}
    }

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count, toaddr - addr - count);
  if (irelalign == NULL)
    sec->_cooked_size -= count;
  else
    {
      int i;

#define NOP_OPCODE (0x0009)

      BFD_ASSERT ((count & 1) == 0);
      for (i = 0; i < count; i += 2)
	bfd_put_16 (abfd, NOP_OPCODE, contents + toaddr - count + i);
    }

  /* Adjust all the relocs.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      bfd_vma nraddr, stop;
      bfd_vma start = 0;
      int insn = 0;
      Elf_Internal_Sym sym;
      int off, adjust, oinsn;
      bfd_signed_vma voff = 0;
      boolean overflow;

      /* Get the new reloc address.  */
      nraddr = irel->r_offset;
      if ((irel->r_offset > addr
	   && irel->r_offset < toaddr)
	  || (ELF32_R_TYPE (irel->r_info) == (int) R_SH_ALIGN
	      && irel->r_offset == toaddr))
	nraddr -= count;

      /* See if this reloc was for the bytes we have deleted, in which
	 case we no longer care about it.  Don't delete relocs which
	 represent addresses, though.  */
      if (irel->r_offset >= addr
	  && irel->r_offset < addr + count
	  && ELF32_R_TYPE (irel->r_info) != (int) R_SH_ALIGN
	  && ELF32_R_TYPE (irel->r_info) != (int) R_SH_CODE
	  && ELF32_R_TYPE (irel->r_info) != (int) R_SH_DATA
	  && ELF32_R_TYPE (irel->r_info) != (int) R_SH_LABEL)
	irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
				     (int) R_SH_NONE);

      /* If this is a PC relative reloc, see if the range it covers
         includes the bytes we have deleted.  */
      switch ((enum sh_reloc_type) ELF32_R_TYPE (irel->r_info))
	{
	default:
	  break;

	case R_SH_DIR8WPN:
	case R_SH_IND12W:
	case R_SH_DIR8WPZ:
	case R_SH_DIR8WPL:
	  start = irel->r_offset;
	  insn = bfd_get_16 (abfd, contents + nraddr);
	  break;
	}

      switch ((enum sh_reloc_type) ELF32_R_TYPE (irel->r_info))
	{
	default:
	  start = stop = addr;
	  break;

	case R_SH_DIR32:
	  /* If this reloc is against a symbol defined in this
             section, and the symbol will not be adjusted below, we
             must check the addend to see it will put the value in
             range to be adjusted, and hence must be changed.  */
	  if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	    {
	      bfd_elf32_swap_symbol_in (abfd,
					extsyms + ELF32_R_SYM (irel->r_info),
					&sym);
	      if (sym.st_shndx == shndx
		  && (sym.st_value <= addr
		      || sym.st_value >= toaddr))
		{
		  bfd_vma val;

		  val = bfd_get_32 (abfd, contents + nraddr);
		  val += sym.st_value;
		  if (val >= addr && val < toaddr)
		    bfd_put_32 (abfd, val - count, contents + nraddr);
		}
	    }
	  start = stop = addr;
	  break;

	case R_SH_DIR8WPN:
	  off = insn & 0xff;
	  if (off & 0x80)
	    off -= 0x100;
	  stop = (bfd_vma) ((bfd_signed_vma) start + 4 + off * 2);
	  break;

	case R_SH_IND12W:
	  if (ELF32_R_SYM (irel->r_info) >= symtab_hdr->sh_info)
	    start = stop = addr;
	  else
	    {
	      off = insn & 0xfff;
	      if (off & 0x800)
		off -= 0x1000;
	      stop = (bfd_vma) ((bfd_signed_vma) start + 4 + off * 2);
	    }
	  break;

	case R_SH_DIR8WPZ:
	  off = insn & 0xff;
	  stop = start + 4 + off * 2;
	  break;

	case R_SH_DIR8WPL:
	  off = insn & 0xff;
	  stop = (start &~ (bfd_vma) 3) + 4 + off * 4;
	  break;

	case R_SH_SWITCH16:
	case R_SH_SWITCH32:
	  /* These relocs types represent
	       .word L2-L1
	     The r_offset field holds the difference between the reloc
	     address and L1.  That is the start of the reloc, and
	     adding in the contents gives us the top.  We must adjust
	     both the r_offset field and the section contents.  */

	  start = irel->r_offset;
	  stop = (bfd_vma) ((bfd_signed_vma) start - (long) irel->r_addend);

	  if (start > addr
	      && start < toaddr
	      && (stop <= addr || stop >= toaddr))
	    irel->r_addend += count;
	  else if (stop > addr
		   && stop < toaddr
		   && (start <= addr || start >= toaddr))
	    irel->r_addend -= count;

	  start = stop;

	  if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_SWITCH16)
	    voff = bfd_get_signed_16 (abfd, contents + nraddr);
	  else
	    voff = bfd_get_signed_32 (abfd, contents + nraddr);
	  stop = (bfd_vma) ((bfd_signed_vma) start + voff);

	  break;

	case R_SH_USES:
	  start = irel->r_offset;
	  stop = (bfd_vma) ((bfd_signed_vma) start
			    + (long) irel->r_addend
			    + 4);
	  break;
	}

      if (start > addr
	  && start < toaddr
	  && (stop <= addr || stop >= toaddr))
	adjust = count;
      else if (stop > addr
	       && stop < toaddr
	       && (start <= addr || start >= toaddr))
	adjust = - count;
      else
	adjust = 0;

      if (adjust != 0)
	{
	  oinsn = insn;
	  overflow = false;
	  switch ((enum sh_reloc_type) ELF32_R_TYPE (irel->r_info))
	    {
	    default:
	      abort ();
	      break;

	    case R_SH_DIR8WPN:
	    case R_SH_DIR8WPZ:
	      insn += adjust / 2;
	      if ((oinsn & 0xff00) != (insn & 0xff00))
		overflow = true;
	      bfd_put_16 (abfd, insn, contents + nraddr);
	      break;

	    case R_SH_IND12W:
	      insn += adjust / 2;
	      if ((oinsn & 0xf000) != (insn & 0xf000))
		overflow = true;
	      bfd_put_16 (abfd, insn, contents + nraddr);
	      break;

	    case R_SH_DIR8WPL:
	      BFD_ASSERT (adjust == count || count >= 4);
	      if (count >= 4)
		insn += adjust / 4;
	      else
		{
		  if ((irel->r_offset & 3) == 0)
		    ++insn;
		}
	      if ((oinsn & 0xff00) != (insn & 0xff00))
		overflow = true;
	      bfd_put_16 (abfd, insn, contents + nraddr);
	      break;

	    case R_SH_SWITCH16:
	      voff += adjust;
	      if (voff < - 0x8000 || voff >= 0x8000)
		overflow = true;
	      bfd_put_signed_16 (abfd, voff, contents + nraddr);
	      break;

	    case R_SH_SWITCH32:
	      voff += adjust;
	      bfd_put_signed_32 (abfd, voff, contents + nraddr);
	      break;

	    case R_SH_USES:
	      irel->r_addend += adjust;
	      break;
	    }

	  if (overflow)
	    {
	      ((*_bfd_error_handler)
	       ("%s: 0x%lx: fatal: reloc overflow while relaxing",
		bfd_get_filename (abfd), (unsigned long) irel->r_offset));
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	}

      irel->r_offset = nraddr;
    }

  /* Look through all the other sections.  If there contain any IMM32
     relocs against internal symbols which we are not going to adjust
     below, we may need to adjust the addends.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      Elf_Internal_Rela *internal_relocs;
      Elf_Internal_Rela *irelscan, *irelscanend;
      bfd_byte *ocontents;

      if (o == sec
	  || (o->flags & SEC_RELOC) == 0
	  || o->reloc_count == 0)
	continue;

      /* We always cache the relocs.  Perhaps, if info->keep_memory is
         false, we should free them, if we are permitted to, when we
         leave sh_coff_relax_section.  */
      internal_relocs = (_bfd_elf32_link_read_relocs
			 (abfd, o, (PTR) NULL, (Elf_Internal_Rela *) NULL,
			  true));
      if (internal_relocs == NULL)
	return false;

      ocontents = NULL;
      irelscanend = internal_relocs + o->reloc_count;
      for (irelscan = internal_relocs; irelscan < irelscanend; irelscan++)
	{
	  Elf_Internal_Sym sym;

	  if (ELF32_R_TYPE (irelscan->r_info) != (int) R_SH_DIR32)
	    continue;

	  if (ELF32_R_SYM (irelscan->r_info) >= symtab_hdr->sh_info)
	    continue;

	  bfd_elf32_swap_symbol_in (abfd,
				    extsyms + ELF32_R_SYM (irelscan->r_info),
				    &sym);

	  if (sym.st_shndx == shndx
	      && (sym.st_value <= addr
		  || sym.st_value >= toaddr))
	    {
	      bfd_vma val;

	      if (ocontents == NULL)
		{
		  if (elf_section_data (o)->this_hdr.contents != NULL)
		    ocontents = elf_section_data (o)->this_hdr.contents;
		  else
		    {
		      /* We always cache the section contents.
                         Perhaps, if info->keep_memory is false, we
                         should free them, if we are permitted to,
                         when we leave sh_coff_relax_section.  */
		      ocontents = (bfd_byte *) bfd_malloc (o->_raw_size);
		      if (ocontents == NULL)
			return false;
		      if (! bfd_get_section_contents (abfd, o, ocontents,
						      (file_ptr) 0,
						      o->_raw_size))
			return false;
		      elf_section_data (o)->this_hdr.contents = ocontents;
		    }
		}

	      val = bfd_get_32 (abfd, ocontents + irelscan->r_offset);
	      val += sym.st_value;
	      if (val >= addr && val < toaddr)
		bfd_put_32 (abfd, val - count,
			    ocontents + irelscan->r_offset);
	    }
	}
    }

  /* Adjust the local symbols defined in this section.  */
  esym = extsyms;
  esymend = esym + symtab_hdr->sh_info;
  for (; esym < esymend; esym++)
    {
      Elf_Internal_Sym isym;

      bfd_elf32_swap_symbol_in (abfd, esym, &isym);

      if (isym.st_shndx == shndx
	  && isym.st_value > addr
	  && isym.st_value < toaddr)
	{
	  isym.st_value -= count;
	  bfd_elf32_swap_symbol_out (abfd, &isym, esym);
	}
    }

  /* Now adjust the global symbols defined in this section.  */
  esym = extsyms + symtab_hdr->sh_info;
  esymend = extsyms + (symtab_hdr->sh_size / sizeof (Elf32_External_Sym));
  for (index = 0; esym < esymend; esym++, index++)
    {
      Elf_Internal_Sym isym;

      bfd_elf32_swap_symbol_in (abfd, esym, &isym);
      sym_hash = elf_sym_hashes (abfd)[index];
      if (isym.st_shndx == shndx
	  && ((sym_hash)->root.type == bfd_link_hash_defined
	      || (sym_hash)->root.type == bfd_link_hash_defweak)
	  && (sym_hash)->root.u.def.section == sec
	  && (sym_hash)->root.u.def.value > addr
	  && (sym_hash)->root.u.def.value < toaddr)
	{
	  (sym_hash)->root.u.def.value -= count;
	}
    }

  /* See if we can move the ALIGN reloc forward.  We have adjusted
     r_offset for it already.  */
  if (irelalign != NULL)
    {
      bfd_vma alignto, alignaddr;

      alignto = BFD_ALIGN (toaddr, 1 << irelalign->r_addend);
      alignaddr = BFD_ALIGN (irelalign->r_offset,
			     1 << irelalign->r_addend);
      if (alignto != alignaddr)
	{
	  /* Tail recursion.  */
	  return sh_elf_relax_delete_bytes (abfd, sec, alignaddr,
					    alignto - alignaddr);
	}
    }

  return true;
}

/* Look for loads and stores which we can align to four byte
   boundaries.  This is like sh_align_loads in coff-sh.c.  */

static boolean
sh_elf_align_loads (abfd, sec, internal_relocs, contents, pswapped)
     bfd *abfd;
     asection *sec;
     Elf_Internal_Rela *internal_relocs;
     bfd_byte *contents;
     boolean *pswapped;
{
  Elf_Internal_Rela *irel, *irelend;
  bfd_vma *labels = NULL;
  bfd_vma *label, *label_end;

  *pswapped = false;

  irelend = internal_relocs + sec->reloc_count;

  /* Get all the addresses with labels on them.  */
  labels = (bfd_vma *) bfd_malloc (sec->reloc_count * sizeof (bfd_vma));
  if (labels == NULL)
    goto error_return;
  label_end = labels;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_LABEL)
	{
	  *label_end = irel->r_offset;
	  ++label_end;
	}
    }

  /* Note that the assembler currently always outputs relocs in
     address order.  If that ever changes, this code will need to sort
     the label values and the relocs.  */

  label = labels;

  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma start, stop;

      if (ELF32_R_TYPE (irel->r_info) != (int) R_SH_CODE)
	continue;

      start = irel->r_offset;

      for (irel++; irel < irelend; irel++)
	if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_DATA)
	  break;
      if (irel < irelend)
	stop = irel->r_offset;
      else
	stop = sec->_cooked_size;

      if (! _bfd_sh_align_load_span (abfd, sec, contents, sh_elf_swap_insns,
				     (PTR) internal_relocs, &label,
				     label_end, start, stop, pswapped))
	goto error_return;
    }

  free (labels);

  return true;

 error_return:
  if (labels != NULL)
    free (labels);
  return false;
}

/* Swap two SH instructions.  This is like sh_swap_insns in coff-sh.c.  */

static boolean
sh_elf_swap_insns (abfd, sec, relocs, contents, addr)
     bfd *abfd;
     asection *sec;
     PTR relocs;
     bfd_byte *contents;
     bfd_vma addr;
{
  Elf_Internal_Rela *internal_relocs = (Elf_Internal_Rela *) relocs;
  unsigned short i1, i2;
  Elf_Internal_Rela *irel, *irelend;

  /* Swap the instructions themselves.  */
  i1 = bfd_get_16 (abfd, contents + addr);
  i2 = bfd_get_16 (abfd, contents + addr + 2);
  bfd_put_16 (abfd, i2, contents + addr);
  bfd_put_16 (abfd, i1, contents + addr + 2);

  /* Adjust all reloc addresses.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      enum sh_reloc_type type;
      int add;

      /* There are a few special types of relocs that we don't want to
         adjust.  These relocs do not apply to the instruction itself,
         but are only associated with the address.  */
      type = (enum sh_reloc_type) ELF32_R_TYPE (irel->r_info);
      if (type == R_SH_ALIGN
	  || type == R_SH_CODE
	  || type == R_SH_DATA
	  || type == R_SH_LABEL)
	continue;

      /* If an R_SH_USES reloc points to one of the addresses being
         swapped, we must adjust it.  It would be incorrect to do this
         for a jump, though, since we want to execute both
         instructions after the jump.  (We have avoided swapping
         around a label, so the jump will not wind up executing an
         instruction it shouldn't).  */
      if (type == R_SH_USES)
	{
	  bfd_vma off;

	  off = irel->r_offset + 4 + irel->r_addend;
	  if (off == addr)
	    irel->r_offset += 2;
	  else if (off == addr + 2)
	    irel->r_offset -= 2;
	}

      if (irel->r_offset == addr)
	{
	  irel->r_offset += 2;
	  add = -2;
	}
      else if (irel->r_offset == addr + 2)
	{
	  irel->r_offset -= 2;
	  add = 2;
	}
      else
	add = 0;

      if (add != 0)
	{
	  bfd_byte *loc;
	  unsigned short insn, oinsn;
	  boolean overflow;

	  loc = contents + irel->r_offset;
	  overflow = false;
	  switch (type)
	    {
	    default:
	      break;

	    case R_SH_DIR8WPN:
	    case R_SH_DIR8WPZ:
	      insn = bfd_get_16 (abfd, loc);
	      oinsn = insn;
	      insn += add / 2;
	      if ((oinsn & 0xff00) != (insn & 0xff00))
		overflow = true;
	      bfd_put_16 (abfd, insn, loc);
	      break;

	    case R_SH_IND12W:
	      insn = bfd_get_16 (abfd, loc);
	      oinsn = insn;
	      insn += add / 2;
	      if ((oinsn & 0xf000) != (insn & 0xf000))
		overflow = true;
	      bfd_put_16 (abfd, insn, loc);
	      break;

	    case R_SH_DIR8WPL:
	      /* This reloc ignores the least significant 3 bits of
                 the program counter before adding in the offset.
                 This means that if ADDR is at an even address, the
                 swap will not affect the offset.  If ADDR is an at an
                 odd address, then the instruction will be crossing a
                 four byte boundary, and must be adjusted.  */
	      if ((addr & 3) != 0)
		{
		  insn = bfd_get_16 (abfd, loc);
		  oinsn = insn;
		  insn += add / 2;
		  if ((oinsn & 0xff00) != (insn & 0xff00))
		    overflow = true;
		  bfd_put_16 (abfd, insn, loc);
		}

	      break;
	    }

	  if (overflow)
	    {
	      ((*_bfd_error_handler)
	       ("%s: 0x%lx: fatal: reloc overflow while relaxing",
		bfd_get_filename (abfd), (unsigned long) irel->r_offset));
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	}
    }

  return true;
}

/* Relocate an SH ELF section.  */

static boolean
sh_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel, *relend;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;

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

      r_type = ELF32_R_TYPE (rel->r_info);

      /* Many of the relocs are only used for relaxing, and are
         handled entirely by the relaxation code.  */
      if (r_type > (int) LAST_INVALID_RELOC)
	continue;

      if (r_type < 0
	  || r_type >= (int) FIRST_INVALID_RELOC)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      /* FIXME: This is certainly incorrect.  However, it is how the
         COFF linker works.  */
      if (r_type != (int) R_SH_DIR32
	  && r_type != (int) R_SH_IND12W)
	continue;

      howto = sh_elf_howto_table + r_type;

      /* This is a final link.  */
      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  /* There is nothing to be done for an internal IND12W
             relocation.  FIXME: This is probably wrong, but it's how
             the COFF relocations work.  */
	  if (r_type == (int) R_SH_IND12W)
	    continue;
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
	      relocation = (h->root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
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

      /* FIXME: This is how the COFF relocations work.  */
      if (r_type == (int) R_SH_IND12W)
	relocation -= 4;

      /* FIXME: We should use the addend, but the COFF relocations
         don't.  */
      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, 0);

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
	    }
	}
    }

  return true;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   which uses sh_elf_relocate_section.  */

static bfd_byte *
sh_elf_get_relocated_section_contents (output_bfd, link_info, link_order,
					data, relocateable, symbols)
     bfd *output_bfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  Elf_Internal_Shdr *symtab_hdr;
  asection *input_section = link_order->u.indirect.section;
  bfd *input_bfd = input_section->owner;
  asection **sections = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf32_External_Sym *external_syms = NULL;
  Elf_Internal_Sym *internal_syms = NULL;

  /* We only need to handle the case of relaxing, or of having a
     particular set of section contents, specially.  */
  if (relocateable
      || elf_section_data (input_section)->this_hdr.contents == NULL)
    return bfd_generic_get_relocated_section_contents (output_bfd, link_info,
						       link_order, data,
						       relocateable,
						       symbols);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  memcpy (data, elf_section_data (input_section)->this_hdr.contents,
	  input_section->_raw_size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      Elf_Internal_Sym *isymp;
      asection **secpp;
      Elf32_External_Sym *esym, *esymend;

      if (symtab_hdr->contents != NULL)
	external_syms = (Elf32_External_Sym *) symtab_hdr->contents;
      else
	{
	  external_syms = ((Elf32_External_Sym *)
			   bfd_malloc (symtab_hdr->sh_info
				       * sizeof (Elf32_External_Sym)));
	  if (external_syms == NULL && symtab_hdr->sh_info > 0)
	    goto error_return;
	  if (bfd_seek (input_bfd, symtab_hdr->sh_offset, SEEK_SET) != 0
	      || (bfd_read (external_syms, sizeof (Elf32_External_Sym),
			    symtab_hdr->sh_info, input_bfd)
		  != (symtab_hdr->sh_info * sizeof (Elf32_External_Sym))))
	    goto error_return;
	}

      internal_relocs = (_bfd_elf32_link_read_relocs
			 (input_bfd, input_section, (PTR) NULL,
			  (Elf_Internal_Rela *) NULL, false));
      if (internal_relocs == NULL)
	goto error_return;

      internal_syms = ((Elf_Internal_Sym *)
		       bfd_malloc (symtab_hdr->sh_info
				   * sizeof (Elf_Internal_Sym)));
      if (internal_syms == NULL && symtab_hdr->sh_info > 0)
	goto error_return;

      sections = (asection **) bfd_malloc (symtab_hdr->sh_info
					   * sizeof (asection *));
      if (sections == NULL && symtab_hdr->sh_info > 0)
	goto error_return;

      isymp = internal_syms;
      secpp = sections;
      esym = external_syms;
      esymend = esym + symtab_hdr->sh_info;
      for (; esym < esymend; ++esym, ++isymp, ++secpp)
	{
	  asection *isec;

	  bfd_elf32_swap_symbol_in (input_bfd, esym, isymp);

	  if (isymp->st_shndx == SHN_UNDEF)
	    isec = bfd_und_section_ptr;
	  else if (isymp->st_shndx > 0 && isymp->st_shndx < SHN_LORESERVE)
	    isec = bfd_section_from_elf_index (input_bfd, isymp->st_shndx);
	  else if (isymp->st_shndx == SHN_ABS)
	    isec = bfd_abs_section_ptr;
	  else if (isymp->st_shndx == SHN_COMMON)
	    isec = bfd_com_section_ptr;
	  else
	    {
	      /* Who knows?  */
	      isec = NULL;
	    }

	  *secpp = isec;
	}

      if (! sh_elf_relocate_section (output_bfd, link_info, input_bfd,
				     input_section, data, internal_relocs,
				     internal_syms, sections))
	goto error_return;

      if (sections != NULL)
	free (sections);
      sections = NULL;
      if (internal_syms != NULL)
	free (internal_syms);
      internal_syms = NULL;
      if (external_syms != NULL && symtab_hdr->contents == NULL)
	free (external_syms);
      external_syms = NULL;
      if (internal_relocs != elf_section_data (input_section)->relocs)
	free (internal_relocs);
      internal_relocs = NULL;
    }

  return data;

 error_return:
  if (internal_relocs != NULL
      && internal_relocs != elf_section_data (input_section)->relocs)
    free (internal_relocs);
  if (external_syms != NULL && symtab_hdr->contents == NULL)
    free (external_syms);
  if (internal_syms != NULL)
    free (internal_syms);
  if (sections != NULL)
    free (sections);
  return NULL;
}

#define TARGET_BIG_SYM		bfd_elf32_sh_vec
#define TARGET_BIG_NAME		"elf32-sh"
#define TARGET_LITTLE_SYM	bfd_elf32_shl_vec
#define TARGET_LITTLE_NAME	"elf32-shl"
#define ELF_ARCH		bfd_arch_sh
#define ELF_MACHINE_CODE	EM_SH
#define ELF_MAXPAGESIZE		0x1

#define elf_symbol_leading_char '_'

#define bfd_elf32_bfd_reloc_type_lookup	sh_elf_reloc_type_lookup
#define elf_info_to_howto		sh_elf_info_to_howto
#define bfd_elf32_bfd_relax_section	sh_elf_relax_section
#define elf_backend_relocate_section	sh_elf_relocate_section
#define bfd_elf32_bfd_get_relocated_section_contents \
					sh_elf_get_relocated_section_contents

#include "elf32-target.h"
