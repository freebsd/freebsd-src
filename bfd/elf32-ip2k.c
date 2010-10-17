/* Ubicom IP2xxx specific support for 32-bit ELF
   Copyright 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "elf/ip2k.h"

/* Struct used to pass miscellaneous paramaters which
   helps to avoid overly long parameter lists.  */
struct misc
{
  Elf_Internal_Shdr *  symtab_hdr;
  Elf_Internal_Rela *  irelbase;
  bfd_byte *           contents;
  Elf_Internal_Sym *   isymbuf;
};

struct ip2k_opcode
{
  unsigned short opcode;
  unsigned short mask;
};
  
/* Prototypes.  */
static reloc_howto_type *ip2k_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static int ip2k_is_opcode
  PARAMS ((bfd_byte *, const struct ip2k_opcode *));
static bfd_vma symbol_value
  PARAMS ((bfd *, Elf_Internal_Shdr *, Elf_Internal_Sym *,
	   Elf_Internal_Rela *));
static void ip2k_get_mem
  PARAMS ((bfd *, bfd_byte *, int, bfd_byte *));
static bfd_vma ip2k_nominal_page_bits
  PARAMS ((bfd *, asection *, bfd_vma, bfd_byte *));
static bfd_boolean ip2k_test_page_insn
  PARAMS ((bfd *, asection *, Elf_Internal_Rela *, struct misc *));
static bfd_boolean ip2k_delete_page_insn
  PARAMS ((bfd *, asection *, Elf_Internal_Rela *, bfd_boolean *, struct misc *));
static int ip2k_is_switch_table_128
  PARAMS ((bfd *, asection *, bfd_vma, bfd_byte *));
static bfd_boolean ip2k_relax_switch_table_128
  PARAMS ((bfd *, asection *, Elf_Internal_Rela *, bfd_boolean *, struct misc *));
static int ip2k_is_switch_table_256
  PARAMS ((bfd *, asection *, bfd_vma, bfd_byte *));
static bfd_boolean ip2k_relax_switch_table_256
  PARAMS ((bfd *, asection *, Elf_Internal_Rela *, bfd_boolean *, struct misc *));
static bfd_boolean ip2k_elf_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, bfd_boolean *));
static bfd_boolean ip2k_elf_relax_section_page
  PARAMS ((bfd *, asection *, bfd_boolean *, struct misc *, unsigned long, unsigned long));
static void adjust_all_relocations
  PARAMS ((bfd *, asection *, bfd_vma, bfd_vma, int, int));
static bfd_boolean ip2k_elf_relax_delete_bytes
  PARAMS ((bfd *, asection *, bfd_vma, int));
static void ip2k_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static bfd_reloc_status_type ip2k_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, bfd_vma));
static bfd_boolean ip2k_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static asection *ip2k_elf_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static bfd_boolean ip2k_elf_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

static bfd_boolean ip2k_relaxed = FALSE;

static const struct ip2k_opcode ip2k_page_opcode[] =
{
  {0x0010, 0xFFF8},	/* page */
  {0x0000, 0x0000},
};

#define IS_PAGE_OPCODE(code) \
  ip2k_is_opcode (code, ip2k_page_opcode)

static const struct ip2k_opcode ip2k_jmp_opcode[] =
{
  {0xE000, 0xE000},	/* jmp */
  {0x0000, 0x0000},
};

#define IS_JMP_OPCODE(code) \
  ip2k_is_opcode (code, ip2k_jmp_opcode)

static const struct ip2k_opcode ip2k_call_opcode[] =
{
  {0xC000, 0xE000},	/* call */
  {0x0000, 0x0000},
};

#define IS_CALL_OPCODE(code) \
  ip2k_is_opcode (code, ip2k_call_opcode)

static const struct ip2k_opcode ip2k_snc_opcode[] =
{
  {0xA00B, 0xFFFF},	/* snc */
  {0x0000, 0x0000},
};

#define IS_SNC_OPCODE(code) \
  ip2k_is_opcode (code, ip2k_snc_opcode)

static const struct ip2k_opcode ip2k_inc_1sp_opcode[] =
{
  {0x2B81, 0xFFFF},	/* inc 1(SP) */
  {0x0000, 0x0000},
};

#define IS_INC_1SP_OPCODE(code) \
  ip2k_is_opcode (code, ip2k_inc_1sp_opcode)

static const struct ip2k_opcode ip2k_add_2sp_w_opcode[] =
{
  {0x1F82, 0xFFFF},	/* add 2(SP),w */
  {0x0000, 0x0000},
};

#define IS_ADD_2SP_W_OPCODE(code) \
  ip2k_is_opcode (code, ip2k_add_2sp_w_opcode)

static const struct ip2k_opcode ip2k_add_w_wreg_opcode[] =
{
  {0x1C0A, 0xFFFF},	/* add w,wreg */
  {0x1E0A, 0xFFFF},	/* add wreg,w */
  {0x0000, 0x0000},
};

#define IS_ADD_W_WREG_OPCODE(code) \
  ip2k_is_opcode (code, ip2k_add_w_wreg_opcode)

static const struct ip2k_opcode ip2k_add_pcl_w_opcode[] =
{
  {0x1E09, 0xFFFF},	/* add pcl,w */
  {0x0000, 0x0000},
};

#define IS_ADD_PCL_W_OPCODE(code) \
  ip2k_is_opcode (code, ip2k_add_pcl_w_opcode)

static const struct ip2k_opcode ip2k_skip_opcodes[] =
{
  {0xB000, 0xF000},	/* sb */
  {0xA000, 0xF000},	/* snb */
  {0x7600, 0xFE00},	/* cse/csne #lit */
  {0x5800, 0xFC00},	/* incsnz */
  {0x4C00, 0xFC00},	/* decsnz */
  {0x4000, 0xFC00},	/* cse/csne */
  {0x3C00, 0xFC00},	/* incsz */
  {0x2C00, 0xFC00},	/* decsz */
  {0x0000, 0x0000},
};

#define IS_SKIP_OPCODE(code) \
  ip2k_is_opcode (code, ip2k_skip_opcodes)

/* Relocation tables.  */
static reloc_howto_type ip2k_elf_howto_table [] =
{
#define IP2K_HOWTO(t,rs,s,bs,pr,bp,name,sm,dm) \
    HOWTO(t,                    /* type */ \
          rs,                   /* rightshift */ \
          s,                    /* size (0 = byte, 1 = short, 2 = long) */ \
          bs,                   /* bitsize */ \
          pr,                   /* pc_relative */ \
          bp,                   /* bitpos */ \
          complain_overflow_dont,/* complain_on_overflow */ \
          bfd_elf_generic_reloc,/* special_function */ \
          name,                 /* name */ \
          FALSE,                /* partial_inplace */ \
          sm,                   /* src_mask */ \
          dm,                   /* dst_mask */ \
          pr)                   /* pcrel_offset */

  /* This reloc does nothing.  */
  IP2K_HOWTO (R_IP2K_NONE, 0,2,32, FALSE, 0, "R_IP2K_NONE", 0, 0),
  /* A 16 bit absolute relocation.  */
  IP2K_HOWTO (R_IP2K_16, 0,1,16, FALSE, 0, "R_IP2K_16", 0, 0xffff),
  /* A 32 bit absolute relocation.  */
  IP2K_HOWTO (R_IP2K_32, 0,2,32, FALSE, 0, "R_IP2K_32", 0, 0xffffffff),
  /* A 8-bit data relocation for the FR9 field.  Ninth bit is computed specially.  */
  IP2K_HOWTO (R_IP2K_FR9, 0,1,9, FALSE, 0, "R_IP2K_FR9", 0, 0x00ff),
  /* A 4-bit data relocation.  */
  IP2K_HOWTO (R_IP2K_BANK, 8,1,4, FALSE, 0, "R_IP2K_BANK", 0, 0x000f),
  /* A 13-bit insn relocation - word address => right-shift 1 bit extra.  */
  IP2K_HOWTO (R_IP2K_ADDR16CJP, 1,1,13, FALSE, 0, "R_IP2K_ADDR16CJP", 0, 0x1fff),
  /* A 3-bit insn relocation - word address => right-shift 1 bit extra.  */
  IP2K_HOWTO (R_IP2K_PAGE3, 14,1,3, FALSE, 0, "R_IP2K_PAGE3", 0, 0x0007),
  /* Two 8-bit data relocations.  */
  IP2K_HOWTO (R_IP2K_LO8DATA, 0,1,8, FALSE, 0, "R_IP2K_LO8DATA", 0, 0x00ff),
  IP2K_HOWTO (R_IP2K_HI8DATA, 8,1,8, FALSE, 0, "R_IP2K_HI8DATA", 0, 0x00ff),
  /* Two 8-bit insn relocations.  word address => right-shift 1 bit extra.  */
  IP2K_HOWTO (R_IP2K_LO8INSN, 1,1,8, FALSE, 0, "R_IP2K_LO8INSN", 0, 0x00ff),
  IP2K_HOWTO (R_IP2K_HI8INSN, 9,1,8, FALSE, 0, "R_IP2K_HI8INSN", 0, 0x00ff),

  /* Special 1 bit relocation for SKIP instructions.  */
  IP2K_HOWTO (R_IP2K_PC_SKIP, 1,1,1, FALSE, 12, "R_IP2K_PC_SKIP", 0xfffe, 0x1000),
  /* 16 bit word address.  */
  IP2K_HOWTO (R_IP2K_TEXT, 1,1,16, FALSE, 0, "R_IP2K_TEXT", 0, 0xffff),
  /* A 7-bit offset relocation for the FR9 field.  Eigth and ninth bit comes from insn.  */
  IP2K_HOWTO (R_IP2K_FR_OFFSET, 0,1,9, FALSE, 0, "R_IP2K_FR_OFFSET", 0x180, 0x007f),
  /* Bits 23:16 of an address.  */
  IP2K_HOWTO (R_IP2K_EX8DATA, 16,1,8, FALSE, 0, "R_IP2K_EX8DATA", 0, 0x00ff),
};


/* Map BFD reloc types to IP2K ELF reloc types.  */
static reloc_howto_type *
ip2k_reloc_type_lookup (abfd, code)
     bfd * abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  /* Note that the ip2k_elf_howto_table is indxed by the R_
     constants.  Thus, the order that the howto records appear in the
     table *must* match the order of the relocation types defined in
     include/elf/ip2k.h.  */

  switch (code)
    {
    case BFD_RELOC_NONE:
      return &ip2k_elf_howto_table[ (int) R_IP2K_NONE];
    case BFD_RELOC_16:
      return &ip2k_elf_howto_table[ (int) R_IP2K_16];
    case BFD_RELOC_32:
      return &ip2k_elf_howto_table[ (int) R_IP2K_32];
    case BFD_RELOC_IP2K_FR9:
      return &ip2k_elf_howto_table[ (int) R_IP2K_FR9];
    case BFD_RELOC_IP2K_BANK:
      return &ip2k_elf_howto_table[ (int) R_IP2K_BANK];
    case BFD_RELOC_IP2K_ADDR16CJP:
      return &ip2k_elf_howto_table[ (int) R_IP2K_ADDR16CJP];
    case BFD_RELOC_IP2K_PAGE3:
      return &ip2k_elf_howto_table[ (int) R_IP2K_PAGE3];
    case BFD_RELOC_IP2K_LO8DATA:
      return &ip2k_elf_howto_table[ (int) R_IP2K_LO8DATA];
    case BFD_RELOC_IP2K_HI8DATA:
      return &ip2k_elf_howto_table[ (int) R_IP2K_HI8DATA];
    case BFD_RELOC_IP2K_LO8INSN:
      return &ip2k_elf_howto_table[ (int) R_IP2K_LO8INSN];
    case BFD_RELOC_IP2K_HI8INSN:
      return &ip2k_elf_howto_table[ (int) R_IP2K_HI8INSN];
    case BFD_RELOC_IP2K_PC_SKIP:
      return &ip2k_elf_howto_table[ (int) R_IP2K_PC_SKIP];
    case BFD_RELOC_IP2K_TEXT:
      return &ip2k_elf_howto_table[ (int) R_IP2K_TEXT];
    case BFD_RELOC_IP2K_FR_OFFSET:
      return &ip2k_elf_howto_table[ (int) R_IP2K_FR_OFFSET];
    case BFD_RELOC_IP2K_EX8DATA:
      return &ip2k_elf_howto_table[ (int) R_IP2K_EX8DATA];
    default:
      /* Pacify gcc -Wall.  */
      return NULL;
    }
  return NULL;
}

static void
ip2k_get_mem (abfd, addr, length, ptr)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_byte *addr;
     int length;
     bfd_byte *ptr;
{
  while (length --)
    * ptr ++ = bfd_get_8 (abfd, addr ++);
}

static bfd_boolean
ip2k_is_opcode (code, opcodes)
     bfd_byte *code;
     const struct ip2k_opcode *opcodes;
{
  unsigned short insn = (code[0] << 8) | code[1];

  while (opcodes->mask != 0)
    {
      if ((insn & opcodes->mask) == opcodes->opcode)
	return TRUE;

      opcodes ++;
    }

  return FALSE;
}

#define PAGENO(ABSADDR) ((ABSADDR) & 0xFFFFC000)
#define BASEADDR(SEC)	((SEC)->output_section->vma + (SEC)->output_offset)

#define UNDEFINED_SYMBOL (~(bfd_vma)0)

/* Return the value of the symbol associated with the relocation IREL.  */

static bfd_vma
symbol_value (abfd, symtab_hdr, isymbuf, irel)
     bfd *abfd;
     Elf_Internal_Shdr *symtab_hdr;
     Elf_Internal_Sym *isymbuf;
     Elf_Internal_Rela *irel;
{
  if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
    {
      Elf_Internal_Sym *isym;
      asection *sym_sec;

      isym = isymbuf + ELF32_R_SYM (irel->r_info);
      if (isym->st_shndx == SHN_UNDEF)
	sym_sec = bfd_und_section_ptr;
      else if (isym->st_shndx == SHN_ABS)
	sym_sec = bfd_abs_section_ptr;
      else if (isym->st_shndx == SHN_COMMON)
	sym_sec = bfd_com_section_ptr;
      else
	sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);

      return isym->st_value + BASEADDR (sym_sec);
    }
  else
    {
      unsigned long indx;
      struct elf_link_hash_entry *h;

      indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
      h = elf_sym_hashes (abfd)[indx];
      BFD_ASSERT (h != NULL);

      if (h->root.type != bfd_link_hash_defined
	  && h->root.type != bfd_link_hash_defweak)
	return UNDEFINED_SYMBOL;

      return (h->root.u.def.value + BASEADDR (h->root.u.def.section));
    }
}

/* Returns the expected page state for the given instruction not including
   the effect of page instructions.  */

static bfd_vma
ip2k_nominal_page_bits (abfd, sec, addr, contents)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     bfd_vma addr;
     bfd_byte *contents;
{
  bfd_vma page = PAGENO (BASEADDR (sec) + addr);

  /* Check if section flows into this page. If not then the page
     bits are assumed to match the PC. This will be true unless
     the user has a page instruction without a call/jump, in which
     case they are on their own.  */
  if (PAGENO (BASEADDR (sec)) == page)
    return page;

  /* Section flows across page boundary. The page bits should match
     the PC unless there is a possible flow from the previous page,
     in which case it is not possible to determine the value of the
     page bits.  */
  while (PAGENO (BASEADDR (sec) + addr - 2) == page)
    {
      bfd_byte code[2];

      addr -= 2;
      ip2k_get_mem (abfd, contents + addr, 2, code);
      if (!IS_PAGE_OPCODE (code))
	continue;

      /* Found a page instruction, check if jump table.  */
      if (ip2k_is_switch_table_128 (abfd, sec, addr, contents) != -1)
	/* Jump table => page is conditional.  */
	continue;

      if (ip2k_is_switch_table_256 (abfd, sec, addr, contents) != -1)
	/* Jump table => page is conditional.  */
	continue;

      /* Found a page instruction, check if conditional.  */
      if (addr >= 2)
        {
	  ip2k_get_mem (abfd, contents + addr - 2, 2, code);
          if (IS_SKIP_OPCODE (code))
	    /* Page is conditional.  */
	    continue;
        }

      /* Unconditional page instruction => page bits should be correct.  */
      return page;
    }

  /* Flow from previous page => page bits are impossible to determine.  */
  return 0;
}

static bfd_boolean
ip2k_test_page_insn (abfd, sec, irel, misc)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     Elf_Internal_Rela *irel;
     struct misc *misc;
{
  bfd_vma symval;

  /* Get the value of the symbol referred to by the reloc.  */
  symval = symbol_value (abfd, misc->symtab_hdr, misc->isymbuf, irel);
  if (symval == UNDEFINED_SYMBOL)
    /* This appears to be a reference to an undefined
       symbol.  Just ignore it--it will be caught by the
       regular reloc processing.  */
    return FALSE;

  /* Test if we can delete this page instruction.  */
  if (PAGENO (symval + irel->r_addend) !=
      ip2k_nominal_page_bits (abfd, sec, irel->r_offset, misc->contents))
    return FALSE;

  return TRUE;
}

static bfd_boolean
ip2k_delete_page_insn (abfd, sec, irel, again, misc)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     Elf_Internal_Rela *irel;
     bfd_boolean *again;
     struct misc *misc;
{
  /* Note that we've changed the relocs, section contents, etc.  */
  elf_section_data (sec)->relocs = misc->irelbase;
  elf_section_data (sec)->this_hdr.contents = misc->contents;
  misc->symtab_hdr->contents = (bfd_byte *) misc->isymbuf;

  /* Fix the relocation's type.  */
  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info), R_IP2K_NONE);

  /* Delete the PAGE insn.  */
  if (!ip2k_elf_relax_delete_bytes (abfd, sec, irel->r_offset, 2))
    return FALSE;
	
  /* Modified => will need to iterate relaxation again.  */
  *again = TRUE;
  
  return TRUE;
}

/* Determine if the instruction sequence matches that for
   the prologue of a switch dispatch table with fewer than
   128 entries.

          sc
          page    $nnn0
          jmp     $nnn0
          add     w,wreg
          add     pcl,w
  addr=>
          page    $nnn1
          jmp     $nnn1
 	   page    $nnn2
 	   jmp     $nnn2
 	   ...
 	   page    $nnnN
 	   jmp     $nnnN

  After relaxation.
  	   sc
 	   page    $nnn0
  	   jmp     $nnn0
 	   add     pcl,w
  addr=>
  	   jmp     $nnn1
 	   jmp     $nnn2
 	   ...
          jmp     $nnnN  */

static int
ip2k_is_switch_table_128 (abfd, sec, addr, contents)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     bfd_vma addr;
     bfd_byte *contents;
{
  bfd_byte code[4];
  int index = 0;
  
  /* Check current page-jmp.  */
  if (addr + 4 > sec->_cooked_size)
    return -1;

  ip2k_get_mem (abfd, contents + addr, 4, code);

  if ((! IS_PAGE_OPCODE (code + 0))
      || (! IS_JMP_OPCODE (code + 2)))
    return -1;
  
  /* Search back.  */
  while (1)
    {
      if (addr < 4)
	return -1;

      /* Check previous 2 instructions.  */
      ip2k_get_mem (abfd, contents + addr - 4, 4, code);
      if ((IS_ADD_W_WREG_OPCODE (code + 0))
	  && (IS_ADD_PCL_W_OPCODE (code + 2)))
	return index;

      if ((! IS_PAGE_OPCODE (code + 0))
	  || (! IS_JMP_OPCODE (code + 2)))
	return -1;

      index++;
      addr -= 4;
    }
}

static bfd_boolean
ip2k_relax_switch_table_128 (abfd, sec, irel, again, misc)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     Elf_Internal_Rela *irel;
     bfd_boolean *again;
     struct misc *misc;
{
  Elf_Internal_Rela *irelend = misc->irelbase + sec->reloc_count;
  Elf_Internal_Rela *ireltest = irel;
  bfd_byte code[4];
  bfd_vma addr;
  
  /* Test all page instructions.  */
  addr = irel->r_offset;
  while (1)
    {
      if (addr + 4 > sec->_cooked_size)
	break;

      ip2k_get_mem (abfd, misc->contents + addr, 4, code);
      if ((! IS_PAGE_OPCODE (code + 0))
	  || (! IS_JMP_OPCODE (code + 2)))
	break;

      /* Validate relocation entry (every entry should have a matching
          relocation entry).  */
      if (ireltest >= irelend)
        {
	  _bfd_error_handler (_("ip2k relaxer: switch table without complete matching relocation information."));
          return FALSE;
        }

      if (ireltest->r_offset != addr)
        {
	  _bfd_error_handler (_("ip2k relaxer: switch table without complete matching relocation information."));
          return FALSE;
        }

      if (! ip2k_test_page_insn (abfd, sec, ireltest, misc))
	/* Un-removable page insn => nothing can be done.  */
	return TRUE;

      addr += 4;
      ireltest += 2;
    }

  /* Relaxable. Adjust table header.  */
  ip2k_get_mem (abfd, misc->contents + irel->r_offset - 4, 4, code);
  if ((! IS_ADD_W_WREG_OPCODE (code + 0))
      || (! IS_ADD_PCL_W_OPCODE (code + 2)))
    {
      _bfd_error_handler (_("ip2k relaxer: switch table header corrupt."));
      return FALSE;
    }

  if (!ip2k_elf_relax_delete_bytes (abfd, sec, irel->r_offset - 4, 2))
    return FALSE;

  *again = TRUE;

  /* Delete all page instructions in table.  */
  while (irel < ireltest)
    {
      if (!ip2k_delete_page_insn (abfd, sec, irel, again, misc))
	return FALSE;
      irel += 2;
    }

  return TRUE;
}

/* Determine if the instruction sequence matches that for
   the prologue switch dispatch table with fewer than
   256 entries but more than 127.

   Before relaxation.
          push    %lo8insn(label) ; Push address of table
          push    %hi8insn(label)
          add     w,wreg          ; index*2 => offset
          snc                     ; CARRY SET?
          inc     1(sp)           ; Propagate MSB into table address
          add     2(sp),w         ; Add low bits of offset to table address
          snc                     ; and handle any carry-out
          inc     1(sp)
   addr=>
          page    __indjmp        ; Do an indirect jump to that location
          jmp     __indjmp
   label:                         ; case dispatch table starts here
 	   page    $nnn1
 	   jmp	   $nnn1
 	   page	   $nnn2
 	   jmp     $nnn2
 	   ...
 	   page    $nnnN
 	   jmp	   $nnnN

  After relaxation.
          push    %lo8insn(label) ; Push address of table
          push    %hi8insn(label)
          add     2(sp),w         ; Add low bits of offset to table address
          snc                     ; and handle any carry-out
          inc     1(sp)
  addr=>
          page    __indjmp        ; Do an indirect jump to that location
          jmp     __indjmp
   label:                         ; case dispatch table starts here
          jmp     $nnn1
          jmp     $nnn2
          ...
          jmp     $nnnN  */

static int
ip2k_is_switch_table_256 (abfd, sec, addr, contents)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     bfd_vma addr;
     bfd_byte *contents;
{
  bfd_byte code[16];
  int index = 0;
  
  /* Check current page-jmp.  */
  if (addr + 4 > sec->_cooked_size)
    return -1;

  ip2k_get_mem (abfd, contents + addr, 4, code);
  if ((! IS_PAGE_OPCODE (code + 0))
      || (! IS_JMP_OPCODE (code + 2)))
    return -1;
  
  /* Search back.  */
  while (1)
    {
      if (addr < 16)
	return -1;

      /* Check previous 8 instructions.  */
      ip2k_get_mem (abfd, contents + addr - 16, 16, code);
      if ((IS_ADD_W_WREG_OPCODE (code + 0))
	  && (IS_SNC_OPCODE (code + 2))
	  && (IS_INC_1SP_OPCODE (code + 4))
	  && (IS_ADD_2SP_W_OPCODE (code + 6))
	  && (IS_SNC_OPCODE (code + 8))
	  && (IS_INC_1SP_OPCODE (code + 10))
	  && (IS_PAGE_OPCODE (code + 12))
	  && (IS_JMP_OPCODE (code + 14)))
	return index;

      if ((IS_ADD_W_WREG_OPCODE (code + 2))
	  && (IS_SNC_OPCODE (code + 4))
	  && (IS_INC_1SP_OPCODE (code + 6))
	  && (IS_ADD_2SP_W_OPCODE (code + 8))
	  && (IS_SNC_OPCODE (code + 10))
	  && (IS_INC_1SP_OPCODE (code + 12))
	  && (IS_JMP_OPCODE (code + 14)))
	return index;
      
      if ((! IS_PAGE_OPCODE (code + 0))
	  || (! IS_JMP_OPCODE (code + 2)))
	return -1;

      index++;
      addr -= 4;
    }
}

static bfd_boolean
ip2k_relax_switch_table_256 (abfd, sec, irel, again, misc)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     Elf_Internal_Rela *irel;
     bfd_boolean *again;
     struct misc *misc;
{
  Elf_Internal_Rela *irelend = misc->irelbase + sec->reloc_count;
  Elf_Internal_Rela *ireltest = irel;
  bfd_byte code[12];
  bfd_vma addr;
  
  /* Test all page instructions.  */
  addr = irel->r_offset;

  while (1)
    {
      if (addr + 4 > sec->_cooked_size)
	break;

      ip2k_get_mem (abfd, misc->contents + addr, 4, code);

      if ((! IS_PAGE_OPCODE (code + 0))
	  || (! IS_JMP_OPCODE (code + 2)))
	break;

      /* Validate relocation entry (every entry should have a matching
          relocation entry).  */
      if (ireltest >= irelend)
        {
          _bfd_error_handler (_("ip2k relaxer: switch table without complete matching relocation information."));
          return FALSE;
        }

      if (ireltest->r_offset != addr)
        {
          _bfd_error_handler (_("ip2k relaxer: switch table without complete matching relocation information."));
          return FALSE;
        }

      if (!ip2k_test_page_insn (abfd, sec, ireltest, misc))
	/* Un-removable page insn => nothing can be done.  */
	return TRUE;

      addr += 4;
      ireltest += 2;
    }

  /* Relaxable. Adjust table header.  */
  ip2k_get_mem (abfd, misc->contents + irel->r_offset - 4, 2, code);
  if (IS_PAGE_OPCODE (code))
    addr = irel->r_offset - 16;
  else
    addr = irel->r_offset - 14;

  ip2k_get_mem (abfd, misc->contents + addr, 12, code);
  if ((!IS_ADD_W_WREG_OPCODE (code + 0))
      || (!IS_SNC_OPCODE (code + 2))
      || (!IS_INC_1SP_OPCODE (code + 4))
      || (!IS_ADD_2SP_W_OPCODE (code + 6))
      || (!IS_SNC_OPCODE (code + 8))
      || (!IS_INC_1SP_OPCODE (code + 10)))
    {
      _bfd_error_handler (_("ip2k relaxer: switch table header corrupt."));
      return FALSE;
    }

  /* Delete first 3 opcodes.  */
  if (!ip2k_elf_relax_delete_bytes (abfd, sec, addr + 0, 6))
    return FALSE;

  *again = TRUE;

  /* Delete all page instructions in table.  */
  while (irel < ireltest)
    {
      if (!ip2k_delete_page_insn (abfd, sec, irel, again, misc))
	return FALSE;
      irel += 2;
    }

  return TRUE;
}

/* This function handles relaxing for the ip2k.

   Principle: Start with the first page and remove page instructions that
   are not require on this first page. By removing page instructions more
   code will fit into this page - repeat until nothing more can be achieved
   for this page. Move on to the next page.

   Processing the pages one at a time from the lowest page allows a removal
   only policy to be used - pages can be removed but are never reinserted.  */

static bfd_boolean
ip2k_elf_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     bfd_boolean *again;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents = NULL;
  Elf_Internal_Sym *isymbuf = NULL;
  static asection * first_section = NULL;
  static unsigned long search_addr;
  static unsigned long page_start = 0;
  static unsigned long page_end = 0;
  static unsigned int pass = 0;
  static bfd_boolean new_pass = FALSE;
  static bfd_boolean changed = FALSE;
  struct misc misc;
  asection *stab;

  /* Assume nothing changes.  */
  *again = FALSE;

  if (first_section == NULL)
    {
      ip2k_relaxed = TRUE;
      first_section = sec;
    }

  if (first_section == sec)
    {
      pass++;
      new_pass = TRUE;
    }

  /* We don't have to do anything for a relocatable link,
     if this section does not have relocs, or if this is
     not a code section.  */
  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0)
    return TRUE;

  /* If this is the first time we have been called
      for this section, initialise the cooked size.  */
  if (sec->_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  internal_relocs = _bfd_elf_link_read_relocs (abfd, sec, NULL,
					       (Elf_Internal_Rela *)NULL,
					       link_info->keep_memory);
  if (internal_relocs == NULL)
    goto error_return;

  /* Make sure the stac.rela stuff gets read in.  */
  stab = bfd_get_section_by_name (abfd, ".stab");

  if (stab)
    {
      /* So stab does exits.  */
      Elf_Internal_Rela * irelbase;

      irelbase = _bfd_elf_link_read_relocs (abfd, stab, NULL,
					    (Elf_Internal_Rela *)NULL,
					    link_info->keep_memory);
    }

  /* Get section contents cached copy if it exists.  */
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

	  if (! bfd_get_section_contents (abfd, sec, contents,
					  (file_ptr) 0, sec->_raw_size))
	    goto error_return;
	}
    }

  /* Read this BFD's symbols cached copy if it exists.  */
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

  misc.symtab_hdr = symtab_hdr;
  misc.isymbuf = isymbuf;
  misc.irelbase = internal_relocs;
  misc.contents = contents;

  /* This is where all the relaxation actually get done.  */
  if ((pass == 1) || (new_pass && !changed))
    {
      /* On the first pass we simply search for the lowest page that
         we havn't relaxed yet. Note that the pass count is reset
         each time a page is complete in order to move on to the next page.
         If we can't find any more pages then we are finished.  */
      if (new_pass)
	{
	  pass = 1;
	  new_pass = FALSE;
	  changed = TRUE; /* Pre-initialize to break out of pass 1.  */
	  search_addr = 0xFFFFFFFF;
	}

      if ((BASEADDR (sec) + sec->_cooked_size < search_addr)
	  && (BASEADDR (sec) + sec->_cooked_size > page_end))
	{
	  if (BASEADDR (sec) <= page_end)
	    search_addr = page_end + 1;
	  else
	    search_addr = BASEADDR (sec);

	  /* Found a page => more work to do.  */
	  *again = TRUE;
	}
    }
  else
    {
      if (new_pass)
	{
	  new_pass = FALSE;
	  changed = FALSE;
	  page_start = PAGENO (search_addr);
	  page_end = page_start | 0x00003FFF;
	}

      /* Only process sections in range.  */
      if ((BASEADDR (sec) + sec->_cooked_size >= page_start)
	  && (BASEADDR (sec) <= page_end))
	{
          if (!ip2k_elf_relax_section_page (abfd, sec, &changed, &misc, page_start, page_end))
	    return FALSE;
	}
      *again = TRUE;
    }

  /* Perform some house keeping after relaxing the section.  */

  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    {
      if (! link_info->keep_memory)
	free (isymbuf);
      else
	symtab_hdr->contents = (unsigned char *) isymbuf;
    }

  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    {
      if (! link_info->keep_memory)
	free (contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
	}
    }

  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return TRUE;

 error_return:
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);
  return FALSE;
}

/* This function handles relaxation of a section in a specific page.  */

static bfd_boolean
ip2k_elf_relax_section_page (abfd, sec, again, misc, page_start, page_end)
     bfd *abfd;
     asection *sec;
     bfd_boolean *again;
     struct misc *misc;
     unsigned long page_start;
     unsigned long page_end;
{
  Elf_Internal_Rela *irelend = misc->irelbase + sec->reloc_count;
  Elf_Internal_Rela *irel;
  int switch_table_128;
  int switch_table_256;
  
  /* Walk thru the section looking for relaxation opportunities.  */
  for (irel = misc->irelbase; irel < irelend; irel++)
    {
      if (ELF32_R_TYPE (irel->r_info) != (int) R_IP2K_PAGE3)
	/* Ignore non page instructions.  */
	continue;

      if (BASEADDR (sec) + irel->r_offset < page_start)
	/* Ignore page instructions on earlier page - they have
	   already been processed. Remember that there is code flow
	   that crosses a page boundary.  */
	continue;

      if (BASEADDR (sec) + irel->r_offset > page_end)
	/* Flow beyond end of page => nothing more to do for this page.  */
	return TRUE;

      /* Detect switch tables.  */
      switch_table_128 = ip2k_is_switch_table_128 (abfd, sec, irel->r_offset, misc->contents);
      switch_table_256 = ip2k_is_switch_table_256 (abfd, sec, irel->r_offset, misc->contents);

      if ((switch_table_128 > 0) || (switch_table_256 > 0))
	/* If the index is greater than 0 then it has already been processed.  */
	continue;

      if (switch_table_128 == 0)
	{
	  if (!ip2k_relax_switch_table_128 (abfd, sec, irel, again, misc))
	    return FALSE;

	  continue;
	}

      if (switch_table_256 == 0)
	{
	  if (!ip2k_relax_switch_table_256 (abfd, sec, irel, again, misc))
	    return FALSE;

	  continue;
	}

      /* Simple relax.  */
      if (ip2k_test_page_insn (abfd, sec, irel, misc))
	{
	  if (!ip2k_delete_page_insn (abfd, sec, irel, again, misc))
	    return FALSE;

	  continue;
	}
    }

  return TRUE;
}

/* Parts of a Stabs entry.  */

#define STRDXOFF  (0)
#define TYPEOFF   (4)
#define OTHEROFF  (5)
#define DESCOFF   (6)
#define VALOFF    (8)
#define STABSIZE  (12)

/* Adjust all the relocations entries after adding or inserting instructions.  */

static void
adjust_all_relocations (abfd, sec, addr, endaddr, count, noadj)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
     bfd_vma endaddr;
     int count;
     int noadj;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Sym *isymbuf, *isym, *isymend;
  unsigned int shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend, *irelbase;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;
  asection *stab;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;

  shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  irelbase = elf_section_data (sec)->relocs;
  irelend = irelbase + sec->reloc_count;

  for (irel = irelbase; irel < irelend; irel++)
    {
      if (ELF32_R_TYPE (irel->r_info) != R_IP2K_NONE)
        {
          /* Get the value of the symbol referred to by the reloc.  */
          if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
            {
              asection *sym_sec;

              /* A local symbol.  */
	      isym = isymbuf + ELF32_R_SYM (irel->r_info);
              sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);

              if (isym->st_shndx == shndx)
                {
                  bfd_vma baseaddr = BASEADDR (sec);
                  bfd_vma symval = BASEADDR (sym_sec) + isym->st_value
                                   + irel->r_addend;

                  if ((baseaddr + addr + noadj) <= symval
                      && symval < (baseaddr + endaddr))
                    irel->r_addend += count;
                }
            }
        }

      /* Do this only for PC space relocations.  */
      if (addr <= irel->r_offset && irel->r_offset < endaddr)
        irel->r_offset += count;
    }

  /* Now fix the stab relocations.  */
  stab = bfd_get_section_by_name (abfd, ".stab");
  if (stab)
    {
      bfd_byte *stabcontents, *stabend, *stabp;

      irelbase = elf_section_data (stab)->relocs;
      irelend = irelbase + stab->reloc_count;

      /* Pull out the contents of the stab section.  */
      if (elf_section_data (stab)->this_hdr.contents != NULL)
	stabcontents = elf_section_data (stab)->this_hdr.contents;
      else
	{
	  stabcontents = (bfd_byte *) bfd_alloc (abfd, stab->_raw_size);
	  if (stabcontents == NULL)
	    return;

	  if (! bfd_get_section_contents (abfd, stab, stabcontents,
					  (file_ptr) 0, stab->_raw_size))
	    return;

	  /* We need to remember this.  */
	  elf_section_data (stab)->this_hdr.contents = stabcontents;
	}

      stabend = stabcontents + stab->_raw_size;

      for (irel = irelbase; irel < irelend; irel++)
	{
	  if (ELF32_R_TYPE (irel->r_info) != R_IP2K_NONE)
	    {
	      /* Get the value of the symbol referred to by the reloc.  */
	      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
		{
		  asection *sym_sec;
		  
		  /* A local symbol.  */
		  isym = isymbuf + ELF32_R_SYM (irel->r_info);
		  sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
		  
		  if (sym_sec == sec)
		    {
		      const char *name;
		      unsigned long strx;
		      unsigned char type, other;
		      unsigned short desc;
		      bfd_vma value;
		      bfd_vma baseaddr = BASEADDR (sec);
		      bfd_vma symval = BASEADDR (sym_sec) + isym->st_value
			+ irel->r_addend;
		      
		      if ((baseaddr + addr) <= symval
			  && symval <= (baseaddr + endaddr))
			irel->r_addend += count;

		      /* Go hunt up a function and fix its line info if needed.  */
		      stabp = stabcontents + irel->r_offset - 8; 

		      /* Go pullout the stab entry.  */
		      strx  = bfd_h_get_32 (abfd, stabp + STRDXOFF);
		      type  = bfd_h_get_8 (abfd, stabp + TYPEOFF);
		      other = bfd_h_get_8 (abfd, stabp + OTHEROFF);
		      desc  = bfd_h_get_16 (abfd, stabp + DESCOFF);
		      value = bfd_h_get_32 (abfd, stabp + VALOFF);
		      
		      name = bfd_get_stab_name (type);
		      
		      if (strcmp (name, "FUN") == 0)
			{
			  int function_adjusted = 0;

			  if (symval > (baseaddr + addr))
			    /* Not in this function.  */
			    continue;

			  /* Hey we got a function hit.  */
			  stabp += STABSIZE;
			  for (;stabp < stabend; stabp += STABSIZE)
			    {
			      /* Go pullout the stab entry.  */
			      strx  = bfd_h_get_32 (abfd, stabp + STRDXOFF);
			      type  = bfd_h_get_8 (abfd, stabp + TYPEOFF);
			      other = bfd_h_get_8 (abfd, stabp + OTHEROFF);
			      desc  = bfd_h_get_16 (abfd, stabp + DESCOFF);
			      value = bfd_h_get_32 (abfd, stabp + VALOFF);

			      name = bfd_get_stab_name (type);

			      if (strcmp (name, "FUN") == 0)
				{
				  /* Hit another function entry.  */
				  if (function_adjusted)
				    {
				      /* Adjust the value.  */
				      value += count;
				  
				      /* We need to put it back.  */
				      bfd_h_put_32 (abfd, value,stabp + VALOFF);
				    }

				  /* And then bale out.  */
				  break;
				}

			      if (strcmp (name, "SLINE") == 0)
				{
				  /* Got a line entry.  */
				  if ((baseaddr + addr) <= (symval + value))
				    {
				      /* Adjust the line entry.  */
				      value += count;

				      /* We need to put it back.  */
				      bfd_h_put_32 (abfd, value,stabp + VALOFF);
				      function_adjusted = 1;
				    }
				}
			    }
			}
		    }
		}
	    }
	}
    }

  /* When adding an instruction back it is sometimes necessary to move any
     global or local symbol that was referencing the first instruction of
     the moved block to refer to the first instruction of the inserted block.

     For example adding a PAGE instruction before a CALL or JMP requires
     that any label on the CALL or JMP is moved to the PAGE insn.  */
  addr += noadj;

  /* Adjust the local symbols defined in this section.  */
  isymend = isymbuf + symtab_hdr->sh_info;
  for (isym = isymbuf; isym < isymend; isym++)
    {
      if (isym->st_shndx == shndx
	  && addr <= isym->st_value
	  && isym->st_value < endaddr)
	isym->st_value += count;
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
	  && sym_hash->root.u.def.section == sec)
	{
          if (addr <= sym_hash->root.u.def.value
              && sym_hash->root.u.def.value < endaddr)
	    sym_hash->root.u.def.value += count;
	}
    }

  return;
}

/* Delete some bytes from a section while relaxing.  */

static bfd_boolean
ip2k_elf_relax_delete_bytes (abfd, sec, addr, count)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
     int count;
{
  bfd_byte *contents = elf_section_data (sec)->this_hdr.contents;
  bfd_vma endaddr = sec->_cooked_size;

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count,
	   endaddr - addr - count);

  sec->_cooked_size -= count;

  adjust_all_relocations (abfd, sec, addr + count, endaddr, -count, 0);
  return TRUE;
}

/* -------------------------------------------------------------------- */

/* XXX: The following code is the result of a cut&paste.  This unfortunate
   practice is very widespread in the various target back-end files.  */

/* Set the howto pointer for a IP2K ELF reloc.  */

static void
ip2k_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd * abfd ATTRIBUTE_UNUSED;
     arelent * cache_ptr;
     Elf_Internal_Rela * dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  switch (r_type)
    {
    default:
      cache_ptr->howto = & ip2k_elf_howto_table [r_type];
      break;
    }
}

/* Perform a single relocation.
   By default we use the standard BFD routines.  */

static bfd_reloc_status_type
ip2k_final_link_relocate (howto, input_bfd, input_section, contents, rel,
			  relocation)
     reloc_howto_type *  howto;
     bfd *               input_bfd;
     asection *          input_section;
     bfd_byte *          contents;
     Elf_Internal_Rela * rel;
     bfd_vma             relocation;
{
  static bfd_vma page_addr = 0;

  bfd_reloc_status_type r = bfd_reloc_ok;
  switch (howto->type)
    {
      /* Handle data space relocations.  */
    case R_IP2K_FR9:
    case R_IP2K_BANK:
      if ((relocation & IP2K_DATA_MASK) == IP2K_DATA_VALUE)
	relocation &= ~IP2K_DATA_MASK;
      else
	r = bfd_reloc_notsupported;
      break;

    case R_IP2K_LO8DATA:
    case R_IP2K_HI8DATA:
    case R_IP2K_EX8DATA:
      break;

      /* Handle insn space relocations.  */
    case R_IP2K_PAGE3:
      page_addr = BASEADDR (input_section) + rel->r_offset;
      if ((relocation & IP2K_INSN_MASK) == IP2K_INSN_VALUE)
	relocation &= ~IP2K_INSN_MASK;
      else
	r = bfd_reloc_notsupported;
      break;

    case R_IP2K_ADDR16CJP:
      if (BASEADDR (input_section) + rel->r_offset != page_addr + 2)
	{
	  /* No preceding page instruction, verify that it isn't needed.  */
	  if (PAGENO (relocation + rel->r_addend) !=
	      ip2k_nominal_page_bits (input_bfd, input_section,
	      			      rel->r_offset, contents))
	    _bfd_error_handler (_("ip2k linker: missing page instruction at 0x%08lx (dest = 0x%08lx)."),
				BASEADDR (input_section) + rel->r_offset,
				relocation + rel->r_addend);
        }
      else if (ip2k_relaxed)
        {
          /* Preceding page instruction. Verify that the page instruction is
             really needed. One reason for the relaxation to miss a page is if
             the section is not marked as executable.  */
	  if (!ip2k_is_switch_table_128 (input_bfd, input_section, rel->r_offset - 2, contents) &&
	      !ip2k_is_switch_table_256 (input_bfd, input_section, rel->r_offset - 2, contents) &&
	      (PAGENO (relocation + rel->r_addend) ==
	       ip2k_nominal_page_bits (input_bfd, input_section,
	      			      rel->r_offset - 2, contents)))
	    _bfd_error_handler (_("ip2k linker: redundant page instruction at 0x%08lx (dest = 0x%08lx)."),
				page_addr,
				relocation + rel->r_addend);
        }
      if ((relocation & IP2K_INSN_MASK) == IP2K_INSN_VALUE)
	relocation &= ~IP2K_INSN_MASK;
      else
	r = bfd_reloc_notsupported;
      break;

    case R_IP2K_LO8INSN:
    case R_IP2K_HI8INSN:
    case R_IP2K_PC_SKIP:
      if ((relocation & IP2K_INSN_MASK) == IP2K_INSN_VALUE)
	relocation &= ~IP2K_INSN_MASK;
      else
	r = bfd_reloc_notsupported;
      break;

    case R_IP2K_16:
      /* If this is a relocation involving a TEXT
	 symbol, reduce it to a word address.  */
      if ((relocation & IP2K_INSN_MASK) == IP2K_INSN_VALUE)
	howto = &ip2k_elf_howto_table[ (int) R_IP2K_TEXT];
      break;

      /* Pass others through.  */
    default:
      break;
    }

  /* Only install relocation if above tests did not disqualify it.  */
  if (r == bfd_reloc_ok)
    r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				  contents, rel->r_offset,
				  relocation, rel->r_addend);

  return r;
}

/* Relocate a IP2K ELF section.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjusting the section contents as
   necessary, and (if using Rela relocs and generating a relocatable
   output file) adjusting the reloc addend as necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocatable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static bfd_boolean
ip2k_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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

  if (info->relocatable)
    return TRUE;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *           howto;
      unsigned long                r_symndx;
      Elf_Internal_Sym *           sym;
      asection *                   sec;
      struct elf_link_hash_entry * h;
      bfd_vma                      relocation;
      bfd_reloc_status_type        r;
      const char *                 name = NULL;
      int                          r_type;

      /* This is a final link.  */
      r_type = ELF32_R_TYPE (rel->r_info);
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto  = ip2k_elf_howto_table + ELF32_R_TYPE (rel->r_info);
      h      = NULL;
      sym    = NULL;
      sec    = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = BASEADDR (sec) + sym->st_value;

	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, sec) : name;
	}
      else
	{
	  bfd_boolean warned;
	  bfd_boolean unresolved_reloc;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);

	  name = h->root.root.string;
	}

      /* Finally, the sole IP2K-specific part.  */
      r = ip2k_final_link_relocate (howto, input_bfd, input_section,
				     contents, rel, relocation);

      if (r != bfd_reloc_ok)
	{
	  const char * msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, name, howto->name, (bfd_vma) 0,
		 input_bfd, input_section, rel->r_offset);
	      break;

	    case bfd_reloc_undefined:
	      r = info->callbacks->undefined_symbol
		(info, name, input_bfd, input_section, rel->r_offset, TRUE);
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      break;

	      /* This is how ip2k_final_link_relocate tells us of a non-kosher
                 reference between insn & data address spaces.  */
	    case bfd_reloc_notsupported:
              if (sym != NULL) /* Only if it's not an unresolved symbol.  */
	         msg = _("unsupported relocation between data/insn address spaces");
	      break;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous relocation");
	      break;

	    default:
	      msg = _("internal error: unknown error");
	      break;
	    }

	  if (msg)
	    r = info->callbacks->warning
	      (info, msg, name, input_bfd, input_section, rel->r_offset);

	  if (! r)
	    return FALSE;
	}
    }

  return TRUE;
}

static asection *
ip2k_elf_gc_mark_hook (sec, info, rel, h, sym)
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
#if 0
      case R_IP2K_GNU_VTINHERIT:
      case R_IP2K_GNU_VTENTRY:
        break;
#endif

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
     {
       if (!(elf_bad_symtab (sec->owner)
	     && ELF_ST_BIND (sym->st_info) != STB_LOCAL)
	   && ! ((sym->st_shndx <= 0 || sym->st_shndx >= SHN_LORESERVE)
		 && sym->st_shndx != SHN_COMMON))
	 return bfd_section_from_elf_index (sec->owner, sym->st_shndx);
      }
  return NULL;
}

static bfd_boolean
ip2k_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  /* We don't use got and plt entries for ip2k.  */
  return TRUE;
}

#define TARGET_BIG_SYM	 bfd_elf32_ip2k_vec
#define TARGET_BIG_NAME  "elf32-ip2k"

#define ELF_ARCH	 bfd_arch_ip2k
#define ELF_MACHINE_CODE EM_IP2K
#define ELF_MACHINE_ALT1 EM_IP2K_OLD
#define ELF_MAXPAGESIZE  1 /* No pages on the IP2K.  */

#define elf_info_to_howto_rel			NULL
#define elf_info_to_howto			ip2k_info_to_howto_rela

#define elf_backend_can_gc_sections     	1
#define elf_backend_rela_normal			1
#define elf_backend_gc_mark_hook                ip2k_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook               ip2k_elf_gc_sweep_hook
#define elf_backend_relocate_section		ip2k_elf_relocate_section

#define elf_symbol_leading_char			'_'
#define bfd_elf32_bfd_reloc_type_lookup		ip2k_reloc_type_lookup
#define bfd_elf32_bfd_relax_section		ip2k_elf_relax_section

#include "elf32-target.h"
