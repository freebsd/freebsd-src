/* BFD back-end for Renesas H8/300 COFF binaries.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   Written by Steve Chamberlain, <sac@cygnus.com>.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "genlink.h"
#include "coff/h8300.h"
#include "coff/internal.h"
#include "libcoff.h"
#include "libiberty.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (1)

/* We derive a hash table from the basic BFD hash table to
   hold entries in the function vector.  Aside from the
   info stored by the basic hash table, we need the offset
   of a particular entry within the hash table as well as
   the offset where we'll add the next entry.  */

struct funcvec_hash_entry
  {
    /* The basic hash table entry.  */
    struct bfd_hash_entry root;

    /* The offset within the vectors section where
       this entry lives.  */
    bfd_vma offset;
  };

struct funcvec_hash_table
  {
    /* The basic hash table.  */
    struct bfd_hash_table root;

    bfd *abfd;

    /* Offset at which we'll add the next entry.  */
    unsigned int offset;
  };

static struct bfd_hash_entry *
funcvec_hash_newfunc
  (struct bfd_hash_entry *, struct bfd_hash_table *, const char *);

static bfd_reloc_status_type special
  (bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **);
static int select_reloc
  (reloc_howto_type *);
static void rtype2howto
  (arelent *, struct internal_reloc *);
static void reloc_processing
  (arelent *, struct internal_reloc *, asymbol **, bfd *, asection *);
static bfd_boolean h8300_symbol_address_p
  (bfd *, asection *, bfd_vma);
static int h8300_reloc16_estimate
  (bfd *, asection *, arelent *, unsigned int,
   struct bfd_link_info *);
static void h8300_reloc16_extra_cases
  (bfd *, struct bfd_link_info *, struct bfd_link_order *, arelent *,
   bfd_byte *, unsigned int *, unsigned int *);
static bfd_boolean h8300_bfd_link_add_symbols
  (bfd *, struct bfd_link_info *);

/* To lookup a value in the function vector hash table.  */
#define funcvec_hash_lookup(table, string, create, copy) \
  ((struct funcvec_hash_entry *) \
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

/* The derived h8300 COFF linker table.  Note it's derived from
   the generic linker hash table, not the COFF backend linker hash
   table!  We use this to attach additional data structures we
   need while linking on the h8300.  */
struct h8300_coff_link_hash_table {
  /* The main hash table.  */
  struct generic_link_hash_table root;

  /* Section for the vectors table.  This gets attached to a
     random input bfd, we keep it here for easy access.  */
  asection *vectors_sec;

  /* Hash table of the functions we need to enter into the function
     vector.  */
  struct funcvec_hash_table *funcvec_hash_table;
};

static struct bfd_link_hash_table *h8300_coff_link_hash_table_create (bfd *);

/* Get the H8/300 COFF linker hash table from a link_info structure.  */

#define h8300_coff_hash_table(p) \
  ((struct h8300_coff_link_hash_table *) ((coff_hash_table (p))))

/* Initialize fields within a funcvec hash table entry.  Called whenever
   a new entry is added to the funcvec hash table.  */

static struct bfd_hash_entry *
funcvec_hash_newfunc (struct bfd_hash_entry *entry,
		      struct bfd_hash_table *gen_table,
		      const char *string)
{
  struct funcvec_hash_entry *ret;
  struct funcvec_hash_table *table;

  ret = (struct funcvec_hash_entry *) entry;
  table = (struct funcvec_hash_table *) gen_table;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = ((struct funcvec_hash_entry *)
	   bfd_hash_allocate (gen_table,
			      sizeof (struct funcvec_hash_entry)));
  if (ret == NULL)
    return NULL;

  /* Call the allocation method of the superclass.  */
  ret = ((struct funcvec_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, gen_table, string));

  if (ret == NULL)
    return NULL;

  /* Note where this entry will reside in the function vector table.  */
  ret->offset = table->offset;

  /* Bump the offset at which we store entries in the function
     vector.  We'd like to bump up the size of the vectors section,
     but it's not easily available here.  */
 switch (bfd_get_mach (table->abfd))
   {
   case bfd_mach_h8300:
   case bfd_mach_h8300hn:
   case bfd_mach_h8300sn:
     table->offset += 2;
     break;
   case bfd_mach_h8300h:
   case bfd_mach_h8300s:
     table->offset += 4;
     break;
   default:
     return NULL;
   }

  /* Everything went OK.  */
  return (struct bfd_hash_entry *) ret;
}

/* Initialize the function vector hash table.  */

static bfd_boolean
funcvec_hash_table_init (struct funcvec_hash_table *table,
			 bfd *abfd,
			 struct bfd_hash_entry *(*newfunc)
			   (struct bfd_hash_entry *,
			    struct bfd_hash_table *,
			    const char *),
			 unsigned int entsize)
{
  /* Initialize our local fields, then call the generic initialization
     routine.  */
  table->offset = 0;
  table->abfd = abfd;
  return (bfd_hash_table_init (&table->root, newfunc, entsize));
}

/* Create the derived linker hash table.  We use a derived hash table
   basically to hold "static" information during an H8/300 coff link
   without using static variables.  */

static struct bfd_link_hash_table *
h8300_coff_link_hash_table_create (bfd *abfd)
{
  struct h8300_coff_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct h8300_coff_link_hash_table);

  ret = (struct h8300_coff_link_hash_table *) bfd_malloc (amt);
  if (ret == NULL)
    return NULL;
  if (!_bfd_link_hash_table_init (&ret->root.root, abfd,
				  _bfd_generic_link_hash_newfunc,
				  sizeof (struct generic_link_hash_entry)))
    {
      free (ret);
      return NULL;
    }

  /* Initialize our data.  */
  ret->vectors_sec = NULL;
  ret->funcvec_hash_table = NULL;

  /* OK.  Everything's initialized, return the base pointer.  */
  return &ret->root.root;
}

/* Special handling for H8/300 relocs.
   We only come here for pcrel stuff and return normally if not an -r link.
   When doing -r, we can't do any arithmetic for the pcrel stuff, because
   the code in reloc.c assumes that we can manipulate the targets of
   the pcrel branches.  This isn't so, since the H8/300 can do relaxing,
   which means that the gap after the instruction may not be enough to
   contain the offset required for the branch, so we have to use only
   the addend until the final link.  */

static bfd_reloc_status_type
special (bfd *abfd ATTRIBUTE_UNUSED,
	 arelent *reloc_entry ATTRIBUTE_UNUSED,
	 asymbol *symbol ATTRIBUTE_UNUSED,
	 PTR data ATTRIBUTE_UNUSED,
	 asection *input_section ATTRIBUTE_UNUSED,
	 bfd *output_bfd,
	 char **error_message ATTRIBUTE_UNUSED)
{
  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;

  /* Adjust the reloc address to that in the output section.  */
  reloc_entry->address += input_section->output_offset;
  return bfd_reloc_ok;
}

static reloc_howto_type howto_table[] = {
  HOWTO (R_RELBYTE, 0, 0, 8, FALSE, 0, complain_overflow_bitfield, special, "8", FALSE, 0x000000ff, 0x000000ff, FALSE),
  HOWTO (R_RELWORD, 0, 1, 16, FALSE, 0, complain_overflow_bitfield, special, "16", FALSE, 0x0000ffff, 0x0000ffff, FALSE),
  HOWTO (R_RELLONG, 0, 2, 32, FALSE, 0, complain_overflow_bitfield, special, "32", FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_PCRBYTE, 0, 0, 8, TRUE, 0, complain_overflow_signed, special, "DISP8", FALSE, 0x000000ff, 0x000000ff, TRUE),
  HOWTO (R_PCRWORD, 0, 1, 16, TRUE, 0, complain_overflow_signed, special, "DISP16", FALSE, 0x0000ffff, 0x0000ffff, TRUE),
  HOWTO (R_PCRLONG, 0, 2, 32, TRUE, 0, complain_overflow_signed, special, "DISP32", FALSE, 0xffffffff, 0xffffffff, TRUE),
  HOWTO (R_MOV16B1, 0, 1, 16, FALSE, 0, complain_overflow_bitfield, special, "relaxable mov.b:16", FALSE, 0x0000ffff, 0x0000ffff, FALSE),
  HOWTO (R_MOV16B2, 0, 1, 8, FALSE, 0, complain_overflow_bitfield, special, "relaxed mov.b:16", FALSE, 0x000000ff, 0x000000ff, FALSE),
  HOWTO (R_JMP1, 0, 1, 16, FALSE, 0, complain_overflow_bitfield, special, "16/pcrel", FALSE, 0x0000ffff, 0x0000ffff, FALSE),
  HOWTO (R_JMP2, 0, 0, 8, FALSE, 0, complain_overflow_bitfield, special, "pcrecl/16", FALSE, 0x000000ff, 0x000000ff, FALSE),
  HOWTO (R_JMPL1, 0, 2, 32, FALSE, 0, complain_overflow_bitfield, special, "24/pcrell", FALSE, 0x00ffffff, 0x00ffffff, FALSE),
  HOWTO (R_JMPL2, 0, 0, 8, FALSE, 0, complain_overflow_bitfield, special, "pc8/24", FALSE, 0x000000ff, 0x000000ff, FALSE),
  HOWTO (R_MOV24B1, 0, 1, 32, FALSE, 0, complain_overflow_bitfield, special, "relaxable mov.b:24", FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_MOV24B2, 0, 1, 8, FALSE, 0, complain_overflow_bitfield, special, "relaxed mov.b:24", FALSE, 0x0000ffff, 0x0000ffff, FALSE),

  /* An indirect reference to a function.  This causes the function's address
     to be added to the function vector in lo-mem and puts the address of
     the function vector's entry in the jsr instruction.  */
  HOWTO (R_MEM_INDIRECT, 0, 0, 8, FALSE, 0, complain_overflow_bitfield, special, "8/indirect", FALSE, 0x000000ff, 0x000000ff, FALSE),

  /* Internal reloc for relaxing.  This is created when a 16-bit pc-relative
     branch is turned into an 8-bit pc-relative branch.  */
  HOWTO (R_PCRWORD_B, 0, 0, 8, TRUE, 0, complain_overflow_bitfield, special, "relaxed bCC:16", FALSE, 0x000000ff, 0x000000ff, FALSE),

  HOWTO (R_MOVL1, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,special, "32/24 relaxable move", FALSE, 0xffffffff, 0xffffffff, FALSE),

  HOWTO (R_MOVL2, 0, 1, 16, FALSE, 0, complain_overflow_bitfield, special, "32/24 relaxed move", FALSE, 0x0000ffff, 0x0000ffff, FALSE),

  HOWTO (R_BCC_INV, 0, 0, 8, TRUE, 0, complain_overflow_signed, special, "DISP8 inverted", FALSE, 0x000000ff, 0x000000ff, TRUE),

  HOWTO (R_JMP_DEL, 0, 0, 8, TRUE, 0, complain_overflow_signed, special, "Deleted jump", FALSE, 0x000000ff, 0x000000ff, TRUE),
};

/* Turn a howto into a reloc number.  */

#define SELECT_RELOC(x,howto) \
  { x.r_type = select_reloc (howto); }

#define BADMAG(x) (H8300BADMAG (x) && H8300HBADMAG (x) && H8300SBADMAG (x) \
				   && H8300HNBADMAG(x) && H8300SNBADMAG(x))
#define H8300 1			/* Customize coffcode.h  */
#define __A_MAGIC_SET__

/* Code to swap in the reloc.  */
#define SWAP_IN_RELOC_OFFSET	H_GET_32
#define SWAP_OUT_RELOC_OFFSET	H_PUT_32
#define SWAP_OUT_RELOC_EXTRA(abfd, src, dst) \
  dst->r_stuff[0] = 'S'; \
  dst->r_stuff[1] = 'C';

static int
select_reloc (reloc_howto_type *howto)
{
  return howto->type;
}

/* Code to turn a r_type into a howto ptr, uses the above howto table.  */

static void
rtype2howto (arelent *internal, struct internal_reloc *dst)
{
  switch (dst->r_type)
    {
    case R_RELBYTE:
      internal->howto = howto_table + 0;
      break;
    case R_RELWORD:
      internal->howto = howto_table + 1;
      break;
    case R_RELLONG:
      internal->howto = howto_table + 2;
      break;
    case R_PCRBYTE:
      internal->howto = howto_table + 3;
      break;
    case R_PCRWORD:
      internal->howto = howto_table + 4;
      break;
    case R_PCRLONG:
      internal->howto = howto_table + 5;
      break;
    case R_MOV16B1:
      internal->howto = howto_table + 6;
      break;
    case R_MOV16B2:
      internal->howto = howto_table + 7;
      break;
    case R_JMP1:
      internal->howto = howto_table + 8;
      break;
    case R_JMP2:
      internal->howto = howto_table + 9;
      break;
    case R_JMPL1:
      internal->howto = howto_table + 10;
      break;
    case R_JMPL2:
      internal->howto = howto_table + 11;
      break;
    case R_MOV24B1:
      internal->howto = howto_table + 12;
      break;
    case R_MOV24B2:
      internal->howto = howto_table + 13;
      break;
    case R_MEM_INDIRECT:
      internal->howto = howto_table + 14;
      break;
    case R_PCRWORD_B:
      internal->howto = howto_table + 15;
      break;
    case R_MOVL1:
      internal->howto = howto_table + 16;
      break;
    case R_MOVL2:
      internal->howto = howto_table + 17;
      break;
    case R_BCC_INV:
      internal->howto = howto_table + 18;
      break;
    case R_JMP_DEL:
      internal->howto = howto_table + 19;
      break;
    default:
      abort ();
      break;
    }
}

#define RTYPE2HOWTO(internal, relocentry) rtype2howto (internal, relocentry)

/* Perform any necessary magic to the addend in a reloc entry.  */

#define CALC_ADDEND(abfd, symbol, ext_reloc, cache_ptr) \
 cache_ptr->addend = ext_reloc.r_offset;

#define RELOC_PROCESSING(relent,reloc,symbols,abfd,section) \
 reloc_processing (relent, reloc, symbols, abfd, section)

static void
reloc_processing (arelent *relent, struct internal_reloc *reloc,
		  asymbol **symbols, bfd *abfd, asection *section)
{
  relent->address = reloc->r_vaddr;
  rtype2howto (relent, reloc);

  if (((int) reloc->r_symndx) > 0)
    relent->sym_ptr_ptr = symbols + obj_convert (abfd)[reloc->r_symndx];
  else
    relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;

  relent->addend = reloc->r_offset;
  relent->address -= section->vma;
}

static bfd_boolean
h8300_symbol_address_p (bfd *abfd, asection *input_section, bfd_vma address)
{
  asymbol **s;

  s = _bfd_generic_link_get_symbols (abfd);
  BFD_ASSERT (s != (asymbol **) NULL);

  /* Search all the symbols for one in INPUT_SECTION with
     address ADDRESS.  */
  while (*s)
    {
      asymbol *p = *s;

      if (p->section == input_section
	  && (input_section->output_section->vma
	      + input_section->output_offset
	      + p->value) == address)
	return TRUE;
      s++;
    }
  return FALSE;
}

/* If RELOC represents a relaxable instruction/reloc, change it into
   the relaxed reloc, notify the linker that symbol addresses
   have changed (bfd_perform_slip) and return how much the current
   section has shrunk by.

   FIXME: Much of this code has knowledge of the ordering of entries
   in the howto table.  This needs to be fixed.  */

static int
h8300_reloc16_estimate (bfd *abfd, asection *input_section, arelent *reloc,
			unsigned int shrink, struct bfd_link_info *link_info)
{
  bfd_vma value;
  bfd_vma dot;
  bfd_vma gap;
  static asection *last_input_section = NULL;
  static arelent *last_reloc = NULL;

  /* The address of the thing to be relocated will have moved back by
     the size of the shrink - but we don't change reloc->address here,
     since we need it to know where the relocation lives in the source
     uncooked section.  */
  bfd_vma address = reloc->address - shrink;

  if (input_section != last_input_section)
    last_reloc = NULL;

  /* Only examine the relocs which might be relaxable.  */
  switch (reloc->howto->type)
    {
      /* This is the 16-/24-bit absolute branch which could become an
	 8-bit pc-relative branch.  */
    case R_JMP1:
    case R_JMPL1:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Get the address of the next instruction (not the reloc).  */
      dot = (input_section->output_section->vma
	     + input_section->output_offset + address);

      /* Adjust for R_JMP1 vs R_JMPL1.  */
      dot += (reloc->howto->type == R_JMP1 ? 1 : 2);

      /* Compute the distance from this insn to the branch target.  */
      gap = value - dot;

      /* If the distance is within -128..+128 inclusive, then we can relax
	 this jump.  +128 is valid since the target will move two bytes
	 closer if we do relax this branch.  */
      if ((int) gap >= -128 && (int) gap <= 128)
	{
	  bfd_byte code;

	  if (!bfd_get_section_contents (abfd, input_section, & code,
					 reloc->address, 1))
	    break;
	  code = bfd_get_8 (abfd, & code);

	  /* It's possible we may be able to eliminate this branch entirely;
	     if the previous instruction is a branch around this instruction,
	     and there's no label at this instruction, then we can reverse
	     the condition on the previous branch and eliminate this jump.

	       original:			new:
		 bCC lab1			bCC' lab2
		 jmp lab2
		lab1:				lab1:

	     This saves 4 bytes instead of two, and should be relatively
	     common.

	     Only perform this optimisation for jumps (code 0x5a) not
	     subroutine calls, as otherwise it could transform:

			     mov.w   r0,r0
			     beq     .L1
			     jsr     @_bar
		      .L1:   rts
		      _bar:  rts
	     into:
			     mov.w   r0,r0
			     bne     _bar
			     rts
		      _bar:  rts

	     which changes the call (jsr) into a branch (bne).  */
	  if (code == 0x5a
	      && gap <= 126
	      && last_reloc
	      && last_reloc->howto->type == R_PCRBYTE)
	    {
	      bfd_vma last_value;
	      last_value = bfd_coff_reloc16_get_value (last_reloc, link_info,
						       input_section) + 1;

	      if (last_value == dot + 2
		  && last_reloc->address + 1 == reloc->address
		  && !h8300_symbol_address_p (abfd, input_section, dot - 2))
		{
		  reloc->howto = howto_table + 19;
		  last_reloc->howto = howto_table + 18;
		  last_reloc->sym_ptr_ptr = reloc->sym_ptr_ptr;
		  last_reloc->addend = reloc->addend;
		  shrink += 4;
		  bfd_perform_slip (abfd, 4, input_section, address);
		  break;
		}
	    }

	  /* Change the reloc type.  */
	  reloc->howto = reloc->howto + 1;

	  /* This shrinks this section by two bytes.  */
	  shrink += 2;
	  bfd_perform_slip (abfd, 2, input_section, address);
	}
      break;

    /* This is the 16-bit pc-relative branch which could become an 8-bit
       pc-relative branch.  */
    case R_PCRWORD:
      /* Get the address of the target of this branch, add one to the value
	 because the addend field in PCrel jumps is off by -1.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section) + 1;

      /* Get the address of the next instruction if we were to relax.  */
      dot = input_section->output_section->vma +
	input_section->output_offset + address;

      /* Compute the distance from this insn to the branch target.  */
      gap = value - dot;

      /* If the distance is within -128..+128 inclusive, then we can relax
	 this jump.  +128 is valid since the target will move two bytes
	 closer if we do relax this branch.  */
      if ((int) gap >= -128 && (int) gap <= 128)
	{
	  /* Change the reloc type.  */
	  reloc->howto = howto_table + 15;

	  /* This shrinks this section by two bytes.  */
	  shrink += 2;
	  bfd_perform_slip (abfd, 2, input_section, address);
	}
      break;

    /* This is a 16-bit absolute address in a mov.b insn, which can
       become an 8-bit absolute address if it's in the right range.  */
    case R_MOV16B1:
      /* Get the address of the data referenced by this mov.b insn.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      value = bfd_h8300_pad_address (abfd, value);

      /* If the address is in the top 256 bytes of the address space
	 then we can relax this instruction.  */
      if (value >= 0xffffff00u)
	{
	  /* Change the reloc type.  */
	  reloc->howto = reloc->howto + 1;

	  /* This shrinks this section by two bytes.  */
	  shrink += 2;
	  bfd_perform_slip (abfd, 2, input_section, address);
	}
      break;

    /* Similarly for a 24-bit absolute address in a mov.b.  Note that
       if we can't relax this into an 8-bit absolute, we'll fall through
       and try to relax it into a 16-bit absolute.  */
    case R_MOV24B1:
      /* Get the address of the data referenced by this mov.b insn.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      value = bfd_h8300_pad_address (abfd, value);

      if (value >= 0xffffff00u)
	{
	  /* Change the reloc type.  */
	  reloc->howto = reloc->howto + 1;

	  /* This shrinks this section by four bytes.  */
	  shrink += 4;
	  bfd_perform_slip (abfd, 4, input_section, address);

	  /* Done with this reloc.  */
	  break;
	}

      /* FALLTHROUGH and try to turn the 24-/32-bit reloc into a 16-bit
	 reloc.  */

    /* This is a 24-/32-bit absolute address in a mov insn, which can
       become an 16-bit absolute address if it's in the right range.  */
    case R_MOVL1:
      /* Get the address of the data referenced by this mov insn.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      value = bfd_h8300_pad_address (abfd, value);

      /* If the address is a sign-extended 16-bit value then we can
         relax this instruction.  */
      if (value <= 0x7fff || value >= 0xffff8000u)
	{
	  /* Change the reloc type.  */
	  reloc->howto = howto_table + 17;

	  /* This shrinks this section by two bytes.  */
	  shrink += 2;
	  bfd_perform_slip (abfd, 2, input_section, address);
	}
      break;

      /* No other reloc types represent relaxing opportunities.  */
    default:
      break;
    }

  last_reloc = reloc;
  last_input_section = input_section;
  return shrink;
}

/* Handle relocations for the H8/300, including relocs for relaxed
   instructions.

   FIXME: Not all relocations check for overflow!  */

static void
h8300_reloc16_extra_cases (bfd *abfd, struct bfd_link_info *link_info,
			   struct bfd_link_order *link_order, arelent *reloc,
			   bfd_byte *data, unsigned int *src_ptr,
			   unsigned int *dst_ptr)
{
  unsigned int src_address = *src_ptr;
  unsigned int dst_address = *dst_ptr;
  asection *input_section = link_order->u.indirect.section;
  bfd_vma value;
  bfd_vma dot;
  int gap, tmp;
  unsigned char temp_code;

  switch (reloc->howto->type)
    {
    /* Generic 8-bit pc-relative relocation.  */
    case R_PCRBYTE:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      dot = (input_section->output_offset
	     + dst_address
	     + link_order->u.indirect.section->output_section->vma);

      gap = value - dot;

      /* Sanity check.  */
      if (gap < -128 || gap > 126)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, NULL,
		  bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Everything looks OK.  Apply the relocation and update the
	 src/dst address appropriately.  */
      bfd_put_8 (abfd, gap, data + dst_address);
      dst_address++;
      src_address++;

      /* All done.  */
      break;

    /* Generic 16-bit pc-relative relocation.  */
    case R_PCRWORD:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Get the address of the instruction (not the reloc).  */
      dot = (input_section->output_offset
	     + dst_address
	     + link_order->u.indirect.section->output_section->vma + 1);

      gap = value - dot;

      /* Sanity check.  */
      if (gap > 32766 || gap < -32768)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, NULL,
		  bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Everything looks OK.  Apply the relocation and update the
	 src/dst address appropriately.  */
      bfd_put_16 (abfd, (bfd_vma) gap, data + dst_address);
      dst_address += 2;
      src_address += 2;

      /* All done.  */
      break;

    /* Generic 8-bit absolute relocation.  */
    case R_RELBYTE:
      /* Get the address of the object referenced by this insn.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      bfd_put_8 (abfd, value & 0xff, data + dst_address);
      dst_address += 1;
      src_address += 1;

      /* All done.  */
      break;

    /* Various simple 16-bit absolute relocations.  */
    case R_MOV16B1:
    case R_JMP1:
    case R_RELWORD:
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      bfd_put_16 (abfd, value, data + dst_address);
      dst_address += 2;
      src_address += 2;
      break;

    /* Various simple 24-/32-bit absolute relocations.  */
    case R_MOV24B1:
    case R_MOVL1:
    case R_RELLONG:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      bfd_put_32 (abfd, value, data + dst_address);
      dst_address += 4;
      src_address += 4;
      break;

    /* Another 24-/32-bit absolute relocation.  */
    case R_JMPL1:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      value = ((value & 0x00ffffff)
	       | (bfd_get_32 (abfd, data + src_address) & 0xff000000));
      bfd_put_32 (abfd, value, data + dst_address);
      dst_address += 4;
      src_address += 4;
      break;

      /* This is a 24-/32-bit absolute address in one of the following
	 instructions:

	   "band", "bclr", "biand", "bild", "bior", "bist", "bixor",
	   "bld", "bnot", "bor", "bset", "bst", "btst", "bxor", "ldc.w",
	   "stc.w" and "mov.[bwl]"

	 We may relax this into an 16-bit absolute address if it's in
	 the right range.  */
    case R_MOVL2:
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      value = bfd_h8300_pad_address (abfd, value);

      /* Sanity check.  */
      if (value <= 0x7fff || value >= 0xffff8000u)
	{
	  /* Insert the 16-bit value into the proper location.  */
	  bfd_put_16 (abfd, value, data + dst_address);

	  /* Fix the opcode.  For all the instructions that belong to
	     this relaxation, we simply need to turn off bit 0x20 in
	     the previous byte.  */
	  data[dst_address - 1] &= ~0x20;
	  dst_address += 2;
	  src_address += 4;
	}
      else
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, NULL,
		  bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}
      break;

    /* A 16-bit absolute branch that is now an 8-bit pc-relative branch.  */
    case R_JMP2:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Get the address of the next instruction.  */
      dot = (input_section->output_offset
	     + dst_address
	     + link_order->u.indirect.section->output_section->vma + 1);

      gap = value - dot;

      /* Sanity check.  */
      if (gap < -128 || gap > 126)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, NULL,
		  bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Now fix the instruction itself.  */
      switch (data[dst_address - 1])
	{
	case 0x5e:
	  /* jsr -> bsr */
	  bfd_put_8 (abfd, 0x55, data + dst_address - 1);
	  break;
	case 0x5a:
	  /* jmp -> bra */
	  bfd_put_8 (abfd, 0x40, data + dst_address - 1);
	  break;

	default:
	  abort ();
	}

      /* Write out the 8-bit value.  */
      bfd_put_8 (abfd, gap, data + dst_address);

      dst_address += 1;
      src_address += 3;

      break;

    /* A 16-bit pc-relative branch that is now an 8-bit pc-relative branch.  */
    case R_PCRWORD_B:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Get the address of the instruction (not the reloc).  */
      dot = (input_section->output_offset
	     + dst_address
	     + link_order->u.indirect.section->output_section->vma - 1);

      gap = value - dot;

      /* Sanity check.  */
      if (gap < -128 || gap > 126)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, NULL,
		  bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Now fix the instruction.  */
      switch (data[dst_address - 2])
	{
	case 0x58:
	  /* bCC:16 -> bCC:8 */
	  /* Get the second byte of the original insn, which contains
	     the condition code.  */
	  tmp = data[dst_address - 1];

	  /* Compute the fisrt byte of the relaxed instruction.  The
	     original sequence 0x58 0xX0 is relaxed to 0x4X, where X
	     represents the condition code.  */
	  tmp &= 0xf0;
	  tmp >>= 4;
	  tmp |= 0x40;

	  /* Write it.  */
	  bfd_put_8 (abfd, tmp, data + dst_address - 2);
	  break;

	case 0x5c:
	  /* bsr:16 -> bsr:8 */
	  bfd_put_8 (abfd, 0x55, data + dst_address - 2);
	  break;

	default:
	  abort ();
	}

      /* Output the target.  */
      bfd_put_8 (abfd, gap, data + dst_address - 1);

      /* We don't advance dst_address -- the 8-bit reloc is applied at
	 dst_address - 1, so the next insn should begin at dst_address.  */
      src_address += 2;

      break;

    /* Similarly for a 24-bit absolute that is now 8 bits.  */
    case R_JMPL2:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* Get the address of the instruction (not the reloc).  */
      dot = (input_section->output_offset
	     + dst_address
	     + link_order->u.indirect.section->output_section->vma + 2);

      gap = value - dot;

      /* Fix the instruction.  */
      switch (data[src_address])
	{
	case 0x5e:
	  /* jsr -> bsr */
	  bfd_put_8 (abfd, 0x55, data + dst_address);
	  break;
	case 0x5a:
	  /* jmp ->bra */
	  bfd_put_8 (abfd, 0x40, data + dst_address);
	  break;
	default:
	  abort ();
	}

      bfd_put_8 (abfd, gap, data + dst_address + 1);
      dst_address += 2;
      src_address += 4;

      break;

      /* This is a 16-bit absolute address in one of the following
	 instructions:

	   "band", "bclr", "biand", "bild", "bior", "bist", "bixor",
	   "bld", "bnot", "bor", "bset", "bst", "btst", "bxor", and
	   "mov.b"

	 We may relax this into an 8-bit absolute address if it's in
	 the right range.  */
    case R_MOV16B2:
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* All instructions with R_H8_DIR16B2 start with 0x6a.  */
      if (data[dst_address - 2] != 0x6a)
	abort ();

      temp_code = data[src_address - 1];

      /* If this is a mov.b instruction, clear the lower nibble, which
	 contains the source/destination register number.  */
      if ((temp_code & 0x10) != 0x10)
	temp_code &= 0xf0;

      /* Fix up the opcode.  */
      switch (temp_code)
	{
	case 0x00:
	  /* This is mov.b @aa:16,Rd.  */
	  data[dst_address - 2] = (data[src_address - 1] & 0xf) | 0x20;
	  break;
	case 0x80:
	  /* This is mov.b Rs,@aa:16.  */
	  data[dst_address - 2] = (data[src_address - 1] & 0xf) | 0x30;
	  break;
	case 0x18:
	  /* This is a bit-maniputation instruction that stores one
	     bit into memory, one of "bclr", "bist", "bnot", "bset",
	     and "bst".  */
	  data[dst_address - 2] = 0x7f;
	  break;
	case 0x10:
	  /* This is a bit-maniputation instruction that loads one bit
	     from memory, one of "band", "biand", "bild", "bior",
	     "bixor", "bld", "bor", "btst", and "bxor".  */
	  data[dst_address - 2] = 0x7e;
	  break;
	default:
	  abort ();
	}

      bfd_put_8 (abfd, value & 0xff, data + dst_address - 1);
      src_address += 2;
      break;

      /* This is a 24-bit absolute address in one of the following
	 instructions:

	   "band", "bclr", "biand", "bild", "bior", "bist", "bixor",
	   "bld", "bnot", "bor", "bset", "bst", "btst", "bxor", and
	   "mov.b"

	 We may relax this into an 8-bit absolute address if it's in
	 the right range.  */
    case R_MOV24B2:
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      /* All instructions with R_MOV24B2 start with 0x6a.  */
      if (data[dst_address - 2] != 0x6a)
	abort ();

      temp_code = data[src_address - 1];

      /* If this is a mov.b instruction, clear the lower nibble, which
	 contains the source/destination register number.  */
      if ((temp_code & 0x30) != 0x30)
	temp_code &= 0xf0;

      /* Fix up the opcode.  */
      switch (temp_code)
	{
	case 0x20:
	  /* This is mov.b @aa:24/32,Rd.  */
	  data[dst_address - 2] = (data[src_address - 1] & 0xf) | 0x20;
	  break;
	case 0xa0:
	  /* This is mov.b Rs,@aa:24/32.  */
	  data[dst_address - 2] = (data[src_address - 1] & 0xf) | 0x30;
	  break;
	case 0x38:
	  /* This is a bit-maniputation instruction that stores one
	     bit into memory, one of "bclr", "bist", "bnot", "bset",
	     and "bst".  */
	  data[dst_address - 2] = 0x7f;
	  break;
	case 0x30:
	  /* This is a bit-maniputation instruction that loads one bit
	     from memory, one of "band", "biand", "bild", "bior",
	     "bixor", "bld", "bor", "btst", and "bxor".  */
	  data[dst_address - 2] = 0x7e;
	  break;
	default:
	  abort ();
	}

      bfd_put_8 (abfd, value & 0xff, data + dst_address - 1);
      src_address += 4;
      break;

    case R_BCC_INV:
      /* Get the address of the target of this branch.  */
      value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);

      dot = (input_section->output_offset
	     + dst_address
	     + link_order->u.indirect.section->output_section->vma) + 1;

      gap = value - dot;

      /* Sanity check.  */
      if (gap < -128 || gap > 126)
	{
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, NULL,
		  bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort ();
	}

      /* Everything looks OK.  Fix the condition in the instruction, apply
	 the relocation, and update the src/dst address appropriately.  */

      bfd_put_8 (abfd, bfd_get_8 (abfd, data + dst_address - 1) ^ 1,
		 data + dst_address - 1);
      bfd_put_8 (abfd, gap, data + dst_address);
      dst_address++;
      src_address++;

      /* All done.  */
      break;

    case R_JMP_DEL:
      src_address += 4;
      break;

    /* An 8-bit memory indirect instruction (jmp/jsr).

       There's several things that need to be done to handle
       this relocation.

       If this is a reloc against the absolute symbol, then
       we should handle it just R_RELBYTE.  Likewise if it's
       for a symbol with a value ge 0 and le 0xff.

       Otherwise it's a jump/call through the function vector,
       and the linker is expected to set up the function vector
       and put the right value into the jump/call instruction.  */
    case R_MEM_INDIRECT:
      {
	/* We need to find the symbol so we can determine it's
	   address in the function vector table.  */
	asymbol *symbol;
	const char *name;
	struct funcvec_hash_table *ftab;
	struct funcvec_hash_entry *h;
	struct h8300_coff_link_hash_table *htab;
	asection *vectors_sec;

	if (link_info->hash->creator != abfd->xvec)
	  {
	    (*_bfd_error_handler)
	      (_("cannot handle R_MEM_INDIRECT reloc when using %s output"),
	       link_info->hash->creator->name);

	    /* What else can we do?  This function doesn't allow return
	       of an error, and we don't want to call abort as that
	       indicates an internal error.  */
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif
	    xexit (EXIT_FAILURE);
	  }
	htab = h8300_coff_hash_table (link_info);
	vectors_sec = htab->vectors_sec;

	/* First see if this is a reloc against the absolute symbol
	   or against a symbol with a nonnegative value <= 0xff.  */
	symbol = *(reloc->sym_ptr_ptr);
	value = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
	if (symbol == bfd_abs_section_ptr->symbol
	    || value <= 0xff)
	  {
	    /* This should be handled in a manner very similar to
	       R_RELBYTES.   If the value is in range, then just slam
	       the value into the right location.  Else trigger a
	       reloc overflow callback.  */
	    if (value <= 0xff)
	      {
		bfd_put_8 (abfd, value, data + dst_address);
		dst_address += 1;
		src_address += 1;
	      }
	    else
	      {
		if (! ((*link_info->callbacks->reloc_overflow)
		       (link_info, NULL,
			bfd_asymbol_name (*reloc->sym_ptr_ptr),
			reloc->howto->name, reloc->addend, input_section->owner,
			input_section, reloc->address)))
		  abort ();
	      }
	    break;
	  }

	/* This is a jump/call through a function vector, and we're
	   expected to create the function vector ourselves.

	   First look up this symbol in the linker hash table -- we need
	   the derived linker symbol which holds this symbol's index
	   in the function vector.  */
	name = symbol->name;
	if (symbol->flags & BSF_LOCAL)
	  {
	    char *new_name = bfd_malloc ((bfd_size_type) strlen (name) + 10);

	    if (new_name == NULL)
	      abort ();

	    sprintf (new_name, "%s_%08x", name, symbol->section->id);
	    name = new_name;
	  }

	ftab = htab->funcvec_hash_table;
	h = funcvec_hash_lookup (ftab, name, FALSE, FALSE);

	/* This shouldn't ever happen.  If it does that means we've got
	   data corruption of some kind.  Aborting seems like a reasonable
	   thing to do here.  */
	if (h == NULL || vectors_sec == NULL)
	  abort ();

	/* Place the address of the function vector entry into the
	   reloc's address.  */
	bfd_put_8 (abfd,
		   vectors_sec->output_offset + h->offset,
		   data + dst_address);

	dst_address++;
	src_address++;

	/* Now create an entry in the function vector itself.  */
	switch (bfd_get_mach (input_section->owner))
	  {
	  case bfd_mach_h8300:
	  case bfd_mach_h8300hn:
	  case bfd_mach_h8300sn:
	    bfd_put_16 (abfd,
			bfd_coff_reloc16_get_value (reloc,
						    link_info,
						    input_section),
			vectors_sec->contents + h->offset);
	    break;
	  case bfd_mach_h8300h:
	  case bfd_mach_h8300s:
	    bfd_put_32 (abfd,
			bfd_coff_reloc16_get_value (reloc,
						    link_info,
						    input_section),
			vectors_sec->contents + h->offset);
	    break;
	  default:
	    abort ();
	  }

	/* Gross.  We've already written the contents of the vector section
	   before we get here...  So we write it again with the new data.  */
	bfd_set_section_contents (vectors_sec->output_section->owner,
				  vectors_sec->output_section,
				  vectors_sec->contents,
				  (file_ptr) vectors_sec->output_offset,
				  vectors_sec->size);
	break;
      }

    default:
      abort ();
      break;

    }

  *src_ptr = src_address;
  *dst_ptr = dst_address;
}

/* Routine for the h8300 linker.

   This routine is necessary to handle the special R_MEM_INDIRECT
   relocs on the h8300.  It's responsible for generating a vectors
   section and attaching it to an input bfd as well as sizing
   the vectors section.  It also creates our vectors hash table.

   It uses the generic linker routines to actually add the symbols.
   from this BFD to the bfd linker hash table.  It may add a few
   selected static symbols to the bfd linker hash table.  */

static bfd_boolean
h8300_bfd_link_add_symbols (bfd *abfd, struct bfd_link_info *info)
{
  asection *sec;
  struct funcvec_hash_table *funcvec_hash_table;
  bfd_size_type amt;
  struct h8300_coff_link_hash_table *htab;

  /* Add the symbols using the generic code.  */
  _bfd_generic_link_add_symbols (abfd, info);

  if (info->hash->creator != abfd->xvec)
    return TRUE;

  htab = h8300_coff_hash_table (info);

  /* If we haven't created a vectors section, do so now.  */
  if (!htab->vectors_sec)
    {
      flagword flags;

      /* Make sure the appropriate flags are set, including SEC_IN_MEMORY.  */
      flags = (SEC_ALLOC | SEC_LOAD
	       | SEC_HAS_CONTENTS | SEC_IN_MEMORY | SEC_READONLY);
      htab->vectors_sec = bfd_make_section (abfd, ".vectors");

      /* If the section wasn't created, or we couldn't set the flags,
	 quit quickly now, rather than dying a painful death later.  */
      if (!htab->vectors_sec
	  || !bfd_set_section_flags (abfd, htab->vectors_sec, flags))
	return FALSE;

      /* Also create the vector hash table.  */
      amt = sizeof (struct funcvec_hash_table);
      funcvec_hash_table = (struct funcvec_hash_table *) bfd_alloc (abfd, amt);

      if (!funcvec_hash_table)
	return FALSE;

      /* And initialize the funcvec hash table.  */
      if (!funcvec_hash_table_init (funcvec_hash_table, abfd,
				    funcvec_hash_newfunc,
				    sizeof (struct funcvec_hash_entry)))
	{
	  bfd_release (abfd, funcvec_hash_table);
	  return FALSE;
	}

      /* Store away a pointer to the funcvec hash table.  */
      htab->funcvec_hash_table = funcvec_hash_table;
    }

  /* Load up the function vector hash table.  */
  funcvec_hash_table = htab->funcvec_hash_table;

  /* Now scan the relocs for all the sections in this bfd; create
     additional space in the .vectors section as needed.  */
  for (sec = abfd->sections; sec; sec = sec->next)
    {
      long reloc_size, reloc_count, i;
      asymbol **symbols;
      arelent **relocs;

      /* Suck in the relocs, symbols & canonicalize them.  */
      reloc_size = bfd_get_reloc_upper_bound (abfd, sec);
      if (reloc_size <= 0)
	continue;

      relocs = (arelent **) bfd_malloc ((bfd_size_type) reloc_size);
      if (!relocs)
	return FALSE;

      /* The symbols should have been read in by _bfd_generic link_add_symbols
	 call abovec, so we can cheat and use the pointer to them that was
	 saved in the above call.  */
      symbols = _bfd_generic_link_get_symbols(abfd);
      reloc_count = bfd_canonicalize_reloc (abfd, sec, relocs, symbols);
      if (reloc_count <= 0)
	{
	  free (relocs);
	  continue;
	}

      /* Now walk through all the relocations in this section.  */
      for (i = 0; i < reloc_count; i++)
	{
	  arelent *reloc = relocs[i];
	  asymbol *symbol = *(reloc->sym_ptr_ptr);
	  const char *name;

	  /* We've got an indirect reloc.  See if we need to add it
	     to the function vector table.   At this point, we have
	     to add a new entry for each unique symbol referenced
	     by an R_MEM_INDIRECT relocation except for a reloc
	     against the absolute section symbol.  */
	  if (reloc->howto->type == R_MEM_INDIRECT
	      && symbol != bfd_abs_section_ptr->symbol)

	    {
	      struct funcvec_hash_table *ftab;
	      struct funcvec_hash_entry *h;

	      name = symbol->name;
	      if (symbol->flags & BSF_LOCAL)
		{
		  char *new_name;

		  new_name = bfd_malloc ((bfd_size_type) strlen (name) + 10);
		  if (new_name == NULL)
		    abort ();

		  sprintf (new_name, "%s_%08x", name, symbol->section->id);
		  name = new_name;
		}

	      /* Look this symbol up in the function vector hash table.  */
	      ftab = htab->funcvec_hash_table;
	      h = funcvec_hash_lookup (ftab, name, FALSE, FALSE);

	      /* If this symbol isn't already in the hash table, add
		 it and bump up the size of the hash table.  */
	      if (h == NULL)
		{
		  h = funcvec_hash_lookup (ftab, name, TRUE, TRUE);
		  if (h == NULL)
		    {
		      free (relocs);
		      return FALSE;
		    }

		  /* Bump the size of the vectors section.  Each vector
		     takes 2 bytes on the h8300 and 4 bytes on the h8300h.  */
		  switch (bfd_get_mach (abfd))
		    {
		    case bfd_mach_h8300:
		    case bfd_mach_h8300hn:
		    case bfd_mach_h8300sn:
		      htab->vectors_sec->size += 2;
		      break;
		    case bfd_mach_h8300h:
		    case bfd_mach_h8300s:
		      htab->vectors_sec->size += 4;
		      break;
		    default:
		      abort ();
		    }
		}
	    }
	}

      /* We're done with the relocations, release them.  */
      free (relocs);
    }

  /* Now actually allocate some space for the function vector.  It's
     wasteful to do this more than once, but this is easier.  */
  sec = htab->vectors_sec;
  if (sec->size != 0)
    {
      /* Free the old contents.  */
      if (sec->contents)
	free (sec->contents);

      /* Allocate new contents.  */
      sec->contents = bfd_malloc (sec->size);
    }

  return TRUE;
}

#define coff_reloc16_extra_cases h8300_reloc16_extra_cases
#define coff_reloc16_estimate h8300_reloc16_estimate
#define coff_bfd_link_add_symbols h8300_bfd_link_add_symbols
#define coff_bfd_link_hash_table_create h8300_coff_link_hash_table_create

#define COFF_LONG_FILENAMES
#include "coffcode.h"

#undef coff_bfd_get_relocated_section_contents
#undef coff_bfd_relax_section
#define coff_bfd_get_relocated_section_contents \
  bfd_coff_reloc16_get_relocated_section_contents
#define coff_bfd_relax_section bfd_coff_reloc16_relax_section

CREATE_BIG_COFF_TARGET_VEC (h8300coff_vec, "coff-h8300", BFD_IS_RELAXABLE, 0, '_', NULL, COFF_SWAP_TABLE)
