/* BFD back-end for Hitachi Super-H COFF binaries.
   Copyright 1993, 1994, 1995, 1996 Free Software Foundation, Inc.
   Contributed by Cygnus Support.
   Written by Steve Chamberlain, <sac@cygnus.com>.
   Relaxing code written by Ian Lance Taylor, <ian@cygnus.com>.

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
#include "obstack.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/sh.h"
#include "coff/internal.h"
#include "libcoff.h"

/* Internal functions.  */
static bfd_reloc_status_type sh_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static long get_symbol_value PARAMS ((asymbol *));
static boolean sh_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, boolean *));
static boolean sh_relax_delete_bytes
  PARAMS ((bfd *, asection *, bfd_vma, int));
static boolean sh_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));
static bfd_byte *sh_coff_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *, boolean, asymbol **));

/* Default section alignment to 2**2.  */
#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)

/* Generate long file names.  */
#define COFF_LONG_FILENAMES

/* The supported relocations.  There are a lot of relocations defined
   in coff/internal.h which we do not expect to ever see.  */
static reloc_howto_type sh_coff_howtos[] =
{
  { 0 },
  { 1 },
  { 2 },
  { 3 }, /* R_SH_PCREL8 */
  { 4 }, /* R_SH_PCREL16 */
  { 5 }, /* R_SH_HIGH8 */
  { 6 }, /* R_SH_IMM24 */
  { 7 }, /* R_SH_LOW16 */
  { 8 },
  { 9 }, /* R_SH_PCDISP8BY4 */

  HOWTO (R_SH_PCDISP8BY2,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_pcdisp8by2",	/* name */
	 true,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */

  { 11 }, /* R_SH_PCDISP8 */

  HOWTO (R_SH_PCDISP,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_pcdisp12by2",	/* name */
	 true,			/* partial_inplace */
	 0xfff,			/* src_mask */
	 0xfff,			/* dst_mask */
	 true),			/* pcrel_offset */

  { 13 },

  HOWTO (R_SH_IMM32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_imm32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  { 15 },
  { 16 }, /* R_SH_IMM8 */
  { 17 }, /* R_SH_IMM8BY2 */
  { 18 }, /* R_SH_IMM8BY4 */
  { 19 }, /* R_SH_IMM4 */
  { 20 }, /* R_SH_IMM4BY2 */
  { 21 }, /* R_SH_IMM4BY4 */

  HOWTO (R_SH_PCRELIMM8BY2,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_pcrelimm8by2",	/* name */
	 true,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */

  HOWTO (R_SH_PCRELIMM8BY4,	/* type */
	 2,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_pcrelimm8by4",	/* name */
	 true,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */

  HOWTO (R_SH_IMM16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_imm16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_SH_SWITCH16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_switch16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_SH_SWITCH32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_switch32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_SH_USES,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_uses",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_SH_COUNT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_count",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_SH_ALIGN,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 sh_reloc,		/* special_function */
	 "r_align",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false)			/* pcrel_offset */
};

#define SH_COFF_HOWTO_COUNT (sizeof sh_coff_howtos / sizeof sh_coff_howtos[0])

/* Check for a bad magic number.  */
#define BADMAG(x) SHBADMAG(x)

/* Customize coffcode.h (this is not currently used).  */
#define SH 1

/* FIXME: This should not be set here.  */
#define __A_MAGIC_SET__

/* Swap the r_offset field in and out.  */
#define SWAP_IN_RELOC_OFFSET  bfd_h_get_32
#define SWAP_OUT_RELOC_OFFSET bfd_h_put_32

/* Swap out extra information in the reloc structure.  */
#define SWAP_OUT_RELOC_EXTRA(abfd, src, dst)	\
  do						\
    {						\
      dst->r_stuff[0] = 'S';			\
      dst->r_stuff[1] = 'C';			\
    }						\
  while (0)

/* Get the value of a symbol, when performing a relocation.  */

static long
get_symbol_value (symbol)       
     asymbol *symbol;
{                                             
  bfd_vma relocation;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;                           
  else 
    relocation = (symbol->value +
		  symbol->section->output_section->vma +
		  symbol->section->output_offset);

  return relocation;
}

/* This macro is used in coffcode.h to get the howto corresponding to
   an internal reloc.  */

#define RTYPE2HOWTO(relent, internal)		\
  ((relent)->howto =				\
   ((internal)->r_type < SH_COFF_HOWTO_COUNT	\
    ? &sh_coff_howtos[(internal)->r_type]	\
    : (reloc_howto_type *) NULL))

/* This is the same as the macro in coffcode.h, except that it copies
   r_offset into reloc_entry->addend for some relocs.  */
#define CALC_ADDEND(abfd, ptr, reloc, cache_ptr)                \
  {                                                             \
    coff_symbol_type *coffsym = (coff_symbol_type *) NULL;      \
    if (ptr && bfd_asymbol_bfd (ptr) != abfd)                   \
      coffsym = (obj_symbols (abfd)                             \
                 + (cache_ptr->sym_ptr_ptr - symbols));         \
    else if (ptr)                                               \
      coffsym = coff_symbol_from (abfd, ptr);                   \
    if (coffsym != (coff_symbol_type *) NULL                    \
        && coffsym->native->u.syment.n_scnum == 0)              \
      cache_ptr->addend = 0;                                    \
    else if (ptr && bfd_asymbol_bfd (ptr) == abfd               \
             && ptr->section != (asection *) NULL)              \
      cache_ptr->addend = - (ptr->section->vma + ptr->value);   \
    else                                                        \
      cache_ptr->addend = 0;                                    \
    if ((reloc).r_type == R_SH_SWITCH16				\
	|| (reloc).r_type == R_SH_SWITCH32			\
	|| (reloc).r_type == R_SH_USES				\
	|| (reloc).r_type == R_SH_COUNT				\
	|| (reloc).r_type == R_SH_ALIGN)			\
      cache_ptr->addend = (reloc).r_offset;			\
  }

/* This is the howto function for the SH relocations.  */

static bfd_reloc_status_type
sh_reloc (abfd, reloc_entry, symbol_in, data, input_section, output_bfd,
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
  unsigned short r_type;
  bfd_vma addr = reloc_entry->address;
  bfd_byte *hit_data = addr + (bfd_byte *) data;

  r_type = reloc_entry->howto->type;

  if (output_bfd != NULL)
    {
      /* Partial linking--do nothing.  */
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* Almost all relocs have to do with relaxing.  If any work must be
     done for them, it has been done in sh_relax_section.  */
  if (r_type != R_SH_IMM32
      && (r_type != R_SH_PCDISP
	  || (symbol_in->flags & BSF_LOCAL) != 0))
    return bfd_reloc_ok;

  if (symbol_in != NULL
      && bfd_is_und_section (symbol_in->section))
    return bfd_reloc_undefined;

  sym_value = get_symbol_value (symbol_in);

  switch (r_type)
    {
    case R_SH_IMM32:
      insn = bfd_get_32 (abfd, hit_data);
      insn += sym_value + reloc_entry->addend;
      bfd_put_32 (abfd, insn, hit_data);
      break;
    case R_SH_PCDISP:
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

/* We can do relaxing.  */
#define coff_bfd_relax_section sh_relax_section

/* We use the special COFF backend linker.  */
#define coff_relocate_section sh_relocate_section

/* When relaxing, we need to use special code to get the relocated
   section contents.  */
#define coff_bfd_get_relocated_section_contents \
  sh_coff_get_relocated_section_contents

#include "coffcode.h"

/* This function handles relaxing on the SH.

   Function calls on the SH look like this:

       movl  L1,r0
       ...
       jsr   @r0
       ...
     L1:
       .long function

   The compiler and assembler will cooperate to create R_SH_USES
   relocs on the jsr instructions.  The r_offset field of the
   R_SH_USES reloc is the PC relative offset to the instruction which
   loads the register (the r_offset field is computed as though it
   were a jump instruction, so the offset value is actually from four
   bytes past the instruction).  The linker can use this reloc to
   determine just which function is being called, and thus decide
   whether it is possible to replace the jsr with a bsr.

   If multiple function calls are all based on a single register load
   (i.e., the same function is called multiple times), the compiler
   guarantees that each function call will have an R_SH_USES reloc.
   Therefore, if the linker is able to convert each R_SH_USES reloc
   which refers to that address, it can safely eliminate the register
   load.

   When the assembler creates an R_SH_USES reloc, it examines it to
   determine which address is being loaded (L1 in the above example).
   It then counts the number of references to that address, and
   creates an R_SH_COUNT reloc at that address.  The r_offset field of
   the R_SH_COUNT reloc will be the number of references.  If the
   linker is able to eliminate a register load, it can use the
   R_SH_COUNT reloc to see whether it can also eliminate the function
   address.  */

static boolean 
sh_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     boolean *again;
{
  struct internal_reloc *internal_relocs;
  struct internal_reloc *free_relocs = NULL;
  struct internal_reloc *irel, *irelend;
  bfd_byte *contents = NULL;
  bfd_byte *free_contents = NULL;

  *again = false;

  if (link_info->relocateable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0)
    return true;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (sec->_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  internal_relocs = (_bfd_coff_read_internal_relocs
		     (abfd, sec, link_info->keep_memory,
		      (bfd_byte *) NULL, false,
		      (struct internal_reloc *) NULL));
  if (internal_relocs == NULL)
    goto error_return;
  if (! link_info->keep_memory)
    free_relocs = internal_relocs;

  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma laddr, paddr, symval;
      unsigned short insn;
      struct internal_reloc *irelfn, *irelscan, *irelcount;
      struct internal_syment sym;
      bfd_signed_vma foff;

      if (irel->r_type != R_SH_USES)
	continue;

      /* Get the section contents.  */
      if (contents == NULL)
	{
	  if (coff_section_data (abfd, sec) != NULL
	      && coff_section_data (abfd, sec)->contents != NULL)
	    contents = coff_section_data (abfd, sec)->contents;
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

      /* The r_offset field of the R_SH_USES reloc will point us to
         the register load.  The 4 is because the r_offset field is
         computed as though it were a jump offset, which are based
         from 4 bytes after the jump instruction.  */
      laddr = irel->r_vaddr - sec->vma + 4 + irel->r_offset;
      if (laddr >= sec->_raw_size)
	{
	  (*_bfd_error_handler) ("%s: 0x%lx: warning: bad R_SH_USES offset",
				 bfd_get_filename (abfd),
				 (unsigned long) irel->r_vaddr);
	  continue;
	}
      insn = bfd_get_16 (abfd, contents + laddr);

      /* If the instruction is not mov.l NN,rN, we don't know what to
         do.  */
      if ((insn & 0xf000) != 0xd000)
	{
	  ((*_bfd_error_handler)
	   ("%s: 0x%lx: warning: R_SH_USES points to unrecognized insn 0x%x",
	    bfd_get_filename (abfd), (unsigned long) irel->r_vaddr, insn));
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
	    bfd_get_filename (abfd), (unsigned long) irel->r_vaddr));
	  continue;
	}

      /* Get the reloc for the address from which the register is
         being loaded.  This reloc will tell us which function is
         actually being called.  */
      paddr += sec->vma;
      for (irelfn = internal_relocs; irelfn < irelend; irelfn++)
	if (irelfn->r_vaddr == paddr
	    && irelfn->r_type == R_SH_IMM32)
	  break;
      if (irelfn >= irelend)
	{
	  ((*_bfd_error_handler)
	   ("%s: 0x%lx: warning: could not find expected reloc",
	    bfd_get_filename (abfd), (unsigned long) paddr));
	  continue;
	}

      /* Get the value of the symbol referred to by the reloc.  */
      if (! _bfd_coff_get_external_symbols (abfd))
	goto error_return;
      bfd_coff_swap_sym_in (abfd,
			    ((bfd_byte *) obj_coff_external_syms (abfd)
			     + (irelfn->r_symndx
				* bfd_coff_symesz (abfd))),
			    &sym);
      if (sym.n_scnum != 0 && sym.n_scnum != sec->target_index)
	{
	  ((*_bfd_error_handler)
	   ("%s: 0x%lx: warning: symbol in unexpected section",
	    bfd_get_filename (abfd), (unsigned long) paddr));
	  continue;
	}

      if (sym.n_sclass != C_EXT)
	{
	  symval = (sym.n_value
		    - sec->vma
		    + sec->output_section->vma
		    + sec->output_offset);
	}
      else
	{
	  struct coff_link_hash_entry *h;

	  h = obj_coff_sym_hashes (abfd)[irelfn->r_symndx];
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

      symval += bfd_get_32 (abfd, contents + paddr - sec->vma);

      /* See if this function call can be shortened.  */
      foff = (symval
	      - (irel->r_vaddr
		 - sec->vma
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

      if (coff_section_data (abfd, sec) == NULL)
	{
	  sec->used_by_bfd =
	    ((PTR) bfd_zalloc (abfd, sizeof (struct coff_section_tdata)));
	  if (sec->used_by_bfd == NULL)
	    goto error_return;
	}

      coff_section_data (abfd, sec)->relocs = internal_relocs;
      coff_section_data (abfd, sec)->keep_relocs = true;
      free_relocs = NULL;

      coff_section_data (abfd, sec)->contents = contents;
      coff_section_data (abfd, sec)->keep_contents = true;
      free_contents = NULL;

      obj_coff_keep_syms (abfd) = true;

      /* Replace the jsr with a bsr.  */

      /* Change the R_SH_USES reloc into an R_SH_PCDISP reloc, and
         replace the jsr with a bsr.  */
      irel->r_type = R_SH_PCDISP;
      irel->r_symndx = irelfn->r_symndx;
      if (sym.n_sclass != C_EXT)
	{
	  /* If this needs to be changed because of future relaxing,
             it will be handled here like other internal PCDISP
             relocs.  */
	  bfd_put_16 (abfd,
		      0xb000 | ((foff >> 1) & 0xfff),
		      contents + irel->r_vaddr - sec->vma);
	}
      else
	{
	  /* We can't fully resolve this yet, because the external
             symbol value may be changed by future relaxing.  We let
             the final link phase handle it.  */
	  bfd_put_16 (abfd, 0xb000, contents + irel->r_vaddr - sec->vma);
	}

      /* See if there is another R_SH_USES reloc referring to the same
         register load.  */
      for (irelscan = internal_relocs; irelscan < irelend; irelscan++)
	if (irelscan->r_type == R_SH_USES
	    && laddr == irelscan->r_vaddr - sec->vma + 4 + irelscan->r_offset)
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
	if (irelcount->r_vaddr == paddr
	    && irelcount->r_type == R_SH_COUNT)
	  break;

      /* Delete the register load.  */
      if (! sh_relax_delete_bytes (abfd, sec, laddr, 2))
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

      /* The number of uses is stored in the r_offset field.  We've
         just deleted one.  */
      if (irelcount->r_offset == 0)
	{
	  ((*_bfd_error_handler) ("%s: 0x%lx: warning: bad count",
				  bfd_get_filename (abfd),
				  (unsigned long) paddr));
	  continue;
	}

      --irelcount->r_offset;

      /* If there are no more uses, we can delete the address.  Reload
         the address from irelfn, in case it was changed by the
         previous call to sh_relax_delete_bytes.  */
      if (irelcount->r_offset == 0)
	{
	  if (! sh_relax_delete_bytes (abfd, sec,
				       irelfn->r_vaddr - sec->vma, 4))
	    goto error_return;
	}

      /* We've done all we can with that function call.  */
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
	  /* Cache the section contents for coff_link_input_bfd.  */
	  if (coff_section_data (abfd, sec) == NULL)
	    {
	      sec->used_by_bfd =
		((PTR) bfd_zalloc (abfd, sizeof (struct coff_section_tdata)));
	      if (sec->used_by_bfd == NULL)
		goto error_return;
	      coff_section_data (abfd, sec)->relocs = NULL;
	    }
	  coff_section_data (abfd, sec)->contents = contents;
	}
    }

  return true;

 error_return:
  if (free_relocs != NULL)
    free (free_relocs);
  if (free_contents != NULL)
    free (free_contents);
  return false;
}

/* Delete some bytes from a section while relaxing.  */

static boolean
sh_relax_delete_bytes (abfd, sec, addr, count)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
     int count;
{
  bfd_byte *contents;
  struct internal_reloc *irel, *irelend;
  struct internal_reloc *irelalign;
  bfd_vma toaddr;
  bfd_byte *esym, *esymend;
  bfd_size_type symesz;
  struct coff_link_hash_entry **sym_hash;
  asection *o;

  contents = coff_section_data (abfd, sec)->contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->_cooked_size;

  irel = coff_section_data (abfd, sec)->relocs;
  irelend = irel + sec->reloc_count;
  for (; irel < irelend; irel++)
    {
      if (irel->r_type == R_SH_ALIGN
	  && irel->r_vaddr - sec->vma > addr
	  && count < (1 << irel->r_offset))
	{
	  irelalign = irel;
	  toaddr = irel->r_vaddr - sec->vma;
	  break;
	}
    }

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count, toaddr - addr - count);
  if (irelalign == NULL)
    sec->_cooked_size -= count;
  else
    memset (contents + toaddr - count, 0, count);

  /* Adjust all the relocs.  */
  for (irel = coff_section_data (abfd, sec)->relocs; irel < irelend; irel++)
    {
      bfd_vma nraddr, start, stop;
      int insn = 0;
      struct internal_syment sym;
      int off, adjust, oinsn;
      bfd_signed_vma voff;
      boolean overflow;

      /* Get the new reloc address.  */
      nraddr = irel->r_vaddr - sec->vma;
      if ((irel->r_vaddr - sec->vma > addr
	   && irel->r_vaddr - sec->vma < toaddr)
	  || (irel->r_type == R_SH_ALIGN
	      && irel->r_vaddr - sec->vma == toaddr))
	nraddr -= count;

      /* See if this reloc was for the bytes we have deleted, in which
	 case we no longer care about it.  */
      if (irel->r_vaddr - sec->vma >= addr
	  && irel->r_vaddr - sec->vma < addr + count
	  && irel->r_type != R_SH_ALIGN)
	irel->r_type = R_SH_UNUSED;

      /* If this is a PC relative reloc, see if the range it covers
         includes the bytes we have deleted.  */
      switch (irel->r_type)
	{
	default:
	  break;

	case R_SH_PCDISP8BY2:
	case R_SH_PCDISP:
	case R_SH_PCRELIMM8BY2:
	case R_SH_PCRELIMM8BY4:
	  start = irel->r_vaddr - sec->vma;
	  insn = bfd_get_16 (abfd, contents + nraddr);
	  break;
	}

      switch (irel->r_type)
	{
	default:
	  start = stop = addr;
	  break;

	case R_SH_IMM32:
	  /* If this reloc is against a symbol defined in this
             section, and the symbol will not be adjusted below, we
             must check the addend to see it will put the value in
             range to be adjusted, and hence must be changed.  */
	  bfd_coff_swap_sym_in (abfd,
				((bfd_byte *) obj_coff_external_syms (abfd)
				 + (irel->r_symndx
				    * bfd_coff_symesz (abfd))),
				&sym);
	  if (sym.n_sclass != C_EXT
	      && sym.n_scnum == sec->target_index
	      && ((bfd_vma) sym.n_value <= addr
		  || (bfd_vma) sym.n_value >= toaddr))
	    {
	      bfd_vma val;

	      val = bfd_get_32 (abfd, contents + nraddr);
	      val += sym.n_value;
	      if (val >= addr && val < toaddr)
		bfd_put_32 (abfd, val - count, contents + nraddr);
	    }
	  start = stop = addr;
	  break;

	case R_SH_PCDISP8BY2:
	  off = insn & 0xff;
	  if (off & 0x80)
	    off -= 0x100;
	  stop = (bfd_vma) ((bfd_signed_vma) start + 4 + off * 2);
	  break;

	case R_SH_PCDISP:
	  bfd_coff_swap_sym_in (abfd,
				((bfd_byte *) obj_coff_external_syms (abfd)
				 + (irel->r_symndx
				    * bfd_coff_symesz (abfd))),
				&sym);
	  if (sym.n_sclass == C_EXT)
	    start = stop = addr;
	  else
	    {
	      off = insn & 0xfff;
	      if (off & 0x800)
		off -= 0x1000;
	      stop = (bfd_vma) ((bfd_signed_vma) start + 4 + off * 2);
	    }
	  break;

	case R_SH_PCRELIMM8BY2:
	  off = insn & 0xff;
	  stop = start + 4 + off * 2;
	  break;

	case R_SH_PCRELIMM8BY4:
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

	  start = irel->r_vaddr - sec->vma;
	  stop = (bfd_vma) ((bfd_signed_vma) start - (long) irel->r_offset);

	  if (start > addr
	      && start < toaddr
	      && (stop <= addr || stop >= toaddr))
	    irel->r_offset += count;
	  else if (stop > addr
		   && stop < toaddr
		   && (start <= addr || start >= toaddr))
	    irel->r_offset -= count;

	  start = stop;

	  if (irel->r_type == R_SH_SWITCH16)
	    voff = bfd_get_signed_16 (abfd, contents + nraddr);
	  else
	    voff = bfd_get_signed_32 (abfd, contents + nraddr);
	  stop = (bfd_vma) ((bfd_signed_vma) start + voff);

	  break;

	case R_SH_USES:
	  start = irel->r_vaddr - sec->vma;
	  stop = (bfd_vma) ((bfd_signed_vma) start
			    + (long) irel->r_offset
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
	  switch (irel->r_type)
	    {
	    default:
	      abort ();
	      break;

	    case R_SH_PCDISP8BY2:
	    case R_SH_PCRELIMM8BY2:
	      insn += adjust / 2;
	      if ((oinsn & 0xff00) != (insn & 0xff00))
		overflow = true;
	      bfd_put_16 (abfd, insn, contents + nraddr);
	      break;

	    case R_SH_PCDISP:
	      insn += adjust / 2;
	      if ((oinsn & 0xf000) != (insn & 0xf000))
		overflow = true;
	      bfd_put_16 (abfd, insn, contents + nraddr);
	      break;

	    case R_SH_PCRELIMM8BY4:
	      BFD_ASSERT (adjust == count || count >= 4);
	      if (count >= 4)
		insn += adjust / 4;
	      else
		{
		  if ((irel->r_vaddr & 3) == 0)
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
	      irel->r_offset += adjust;
	      break;
	    }

	  if (overflow)
	    {
	      ((*_bfd_error_handler)
	       ("%s: 0x%lx: fatal: reloc overflow while relaxing",
		bfd_get_filename (abfd), (unsigned long) irel->r_vaddr));
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	}

      irel->r_vaddr = nraddr + sec->vma;
    }

  /* Look through all the other sections.  If there contain any IMM32
     relocs against internal symbols which we are not going to adjust
     below, we may need to adjust the addends.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      struct internal_reloc *internal_relocs;
      struct internal_reloc *irelscan, *irelscanend;
      bfd_byte *ocontents;

      if (o == sec
	  || (o->flags & SEC_RELOC) == 0
	  || o->reloc_count == 0)
	continue;

      /* We always cache the relocs.  Perhaps, if info->keep_memory is
         false, we should free them, if we are permitted to, when we
         leave sh_coff_relax_section.  */
      internal_relocs = (_bfd_coff_read_internal_relocs
			 (abfd, o, true, (bfd_byte *) NULL, false,
			  (struct internal_reloc *) NULL));
      if (internal_relocs == NULL)
	return false;

      ocontents = NULL;
      irelscanend = internal_relocs + o->reloc_count;
      for (irelscan = internal_relocs; irelscan < irelscanend; irelscan++)
	{
	  struct internal_syment sym;

	  if (irelscan->r_type != R_SH_IMM32)
	    continue;

	  bfd_coff_swap_sym_in (abfd,
				((bfd_byte *) obj_coff_external_syms (abfd)
				 + (irelscan->r_symndx
				    * bfd_coff_symesz (abfd))),
				&sym);
	  if (sym.n_sclass != C_EXT
	      && sym.n_scnum == sec->target_index
	      && ((bfd_vma) sym.n_value <= addr
		  || (bfd_vma) sym.n_value >= toaddr))
	    {
	      bfd_vma val;

	      if (ocontents == NULL)
		{
		  if (coff_section_data (abfd, o)->contents != NULL)
		    ocontents = coff_section_data (abfd, o)->contents;
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
		      coff_section_data (abfd, o)->contents = ocontents;
		    }
		}

	      val = bfd_get_32 (abfd, ocontents + irelscan->r_vaddr - o->vma);
	      val += sym.n_value;
	      if (val >= addr && val < toaddr)
		bfd_put_32 (abfd, val - count,
			    ocontents + irelscan->r_vaddr - o->vma);

	      coff_section_data (abfd, o)->keep_contents = true;
	    }
	}
    }

  /* Adjusting the internal symbols will not work if something has
     already retrieved the generic symbols.  It would be possible to
     make this work by adjusting the generic symbols at the same time.
     However, this case should not arise in normal usage.  */
  if (obj_symbols (abfd) != NULL
      || obj_raw_syments (abfd) != NULL)
    {
      ((*_bfd_error_handler)
       ("%s: fatal: generic symbols retrieved before relaxing",
	bfd_get_filename (abfd)));
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  /* Adjust all the symbols.  */
  sym_hash = obj_coff_sym_hashes (abfd);
  symesz = bfd_coff_symesz (abfd);
  esym = (bfd_byte *) obj_coff_external_syms (abfd);
  esymend = esym + obj_raw_syment_count (abfd) * symesz;
  while (esym < esymend)
    {
      struct internal_syment isym;

      bfd_coff_swap_sym_in (abfd, (PTR) esym, (PTR) &isym);

      if (isym.n_scnum == sec->target_index
	  && (bfd_vma) isym.n_value > addr
	  && (bfd_vma) isym.n_value < toaddr)
	{
	  isym.n_value -= count;

	  bfd_coff_swap_sym_out (abfd, (PTR) &isym, (PTR) esym);

	  if (*sym_hash != NULL)
	    {
	      BFD_ASSERT ((*sym_hash)->root.type == bfd_link_hash_defined
			  || (*sym_hash)->root.type == bfd_link_hash_defweak);
	      BFD_ASSERT ((*sym_hash)->root.u.def.value >= addr
			  && (*sym_hash)->root.u.def.value < toaddr);
	      (*sym_hash)->root.u.def.value -= count;
	    }
	}

      esym += (isym.n_numaux + 1) * symesz;
      sym_hash += isym.n_numaux + 1;
    }

  /* See if we can move the ALIGN reloc forward.  We have adjusted
     r_vaddr for it already.  */
  if (irelalign != NULL)
    {
      bfd_vma alignaddr;

      alignaddr = BFD_ALIGN (irelalign->r_vaddr - sec->vma,
			     1 << irelalign->r_offset);
      if (alignaddr != toaddr)
	{
	  /* Tail recursion.  */
	  return sh_relax_delete_bytes (abfd, sec,
					irelalign->r_vaddr - sec->vma,
					1 << irelalign->r_offset);
	}
    }

  return true;
}

/* This is a modification of _bfd_coff_generic_relocate_section, which
   will handle SH relaxing.  */

static boolean
sh_relocate_section (output_bfd, info, input_bfd, input_section, contents,
		     relocs, syms, sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     struct internal_reloc *relocs;
     struct internal_syment *syms;
     asection **sections;
{
  struct internal_reloc *rel;
  struct internal_reloc *relend;

  rel = relocs;
  relend = rel + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      long symndx;
      struct coff_link_hash_entry *h;
      struct internal_syment *sym;
      bfd_vma addend;
      bfd_vma val;
      reloc_howto_type *howto;
      bfd_reloc_status_type rstat;

      /* Almost all relocs have to do with relaxing.  If any work must
         be done for them, it has been done in sh_relax_section.  */
      if (rel->r_type != R_SH_IMM32
	  && rel->r_type != R_SH_PCDISP)
	continue;

      symndx = rel->r_symndx;

      if (symndx == -1)
	{
	  h = NULL;
	  sym = NULL;
	}
      else
	{    
	  h = obj_coff_sym_hashes (input_bfd)[symndx];
	  sym = syms + symndx;
	}

      if (sym != NULL && sym->n_scnum != 0)
	addend = - sym->n_value;
      else
	addend = 0;

      if (rel->r_type == R_SH_PCDISP)
	addend -= 4;

      if (rel->r_type >= SH_COFF_HOWTO_COUNT)
	howto = NULL;
      else
	howto = &sh_coff_howtos[rel->r_type];

      if (howto == NULL)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      val = 0;

      if (h == NULL)
	{
	  asection *sec;

	  /* There is nothing to do for an internal PCDISP reloc.  */
	  if (rel->r_type == R_SH_PCDISP)
	    continue;

	  if (symndx == -1)
	    {
	      sec = bfd_abs_section_ptr;
	      val = 0;
	    }
	  else
	    {
	      sec = sections[symndx];
              val = (sec->output_section->vma
		     + sec->output_offset
		     + sym->n_value
		     - sec->vma);
	    }
	}
      else
	{
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      asection *sec;

	      sec = h->root.u.def.section;
	      val = (h->root.u.def.value
		     + sec->output_section->vma
		     + sec->output_offset);
	    }
	  else if (! info->relocateable)
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd, input_section,
		      rel->r_vaddr - input_section->vma)))
		return false;
	    }
	}

      rstat = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents,
					rel->r_vaddr - input_section->vma,
					val, addend);

      switch (rstat)
	{
	default:
	  abort ();
	case bfd_reloc_ok:
	  break;
	case bfd_reloc_overflow:
	  {
	    const char *name;
	    char buf[SYMNMLEN + 1];

	    if (symndx == -1)
	      name = "*ABS*";
	    else if (h != NULL)
	      name = h->root.root.string;
	    else if (sym->_n._n_n._n_zeroes == 0
		     && sym->_n._n_n._n_offset != 0)
	      name = obj_coff_strings (input_bfd) + sym->_n._n_n._n_offset;
	    else
	      {
 		strncpy (buf, sym->_n._n_name, SYMNMLEN);
		buf[SYMNMLEN] = '\0';
		name = buf;
	      }

	    if (! ((*info->callbacks->reloc_overflow)
		   (info, name, howto->name, (bfd_vma) 0, input_bfd,
		    input_section, rel->r_vaddr - input_section->vma)))
	      return false;
	  }
	}
    }

  return true;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   which uses sh_relocate_section.  */

static bfd_byte *
sh_coff_get_relocated_section_contents (output_bfd, link_info, link_order,
					data, relocateable, symbols)
     bfd *output_bfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  asection *input_section = link_order->u.indirect.section;
  bfd *input_bfd = input_section->owner;
  asection **sections = NULL;
  struct internal_reloc *internal_relocs = NULL;
  struct internal_syment *internal_syms = NULL;

  /* We only need to handle the case of relaxing, or of having a
     particular set of section contents, specially.  */
  if (relocateable
      || coff_section_data (input_bfd, input_section) == NULL
      || coff_section_data (input_bfd, input_section)->contents == NULL)
    return bfd_generic_get_relocated_section_contents (output_bfd, link_info,
						       link_order, data,
						       relocateable,
						       symbols);

  memcpy (data, coff_section_data (input_bfd, input_section)->contents,
	  input_section->_raw_size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      bfd_size_type symesz = bfd_coff_symesz (input_bfd);
      bfd_byte *esym, *esymend;
      struct internal_syment *isymp;
      asection **secpp;

      if (! _bfd_coff_get_external_symbols (input_bfd))
	goto error_return;

      internal_relocs = (_bfd_coff_read_internal_relocs
			 (input_bfd, input_section, false, (bfd_byte *) NULL,
			  false, (struct internal_reloc *) NULL));
      if (internal_relocs == NULL)
	goto error_return;

      internal_syms = ((struct internal_syment *)
		       bfd_malloc (obj_raw_syment_count (input_bfd)
				   * sizeof (struct internal_syment)));
      if (internal_syms == NULL)
	goto error_return;

      sections = (asection **) bfd_malloc (obj_raw_syment_count (input_bfd)
					   * sizeof (asection *));
      if (sections == NULL)
	goto error_return;

      isymp = internal_syms;
      secpp = sections;
      esym = (bfd_byte *) obj_coff_external_syms (input_bfd);
      esymend = esym + obj_raw_syment_count (input_bfd) * symesz;
      while (esym < esymend)
	{
	  bfd_coff_swap_sym_in (input_bfd, (PTR) esym, (PTR) isymp);

	  if (isymp->n_scnum != 0)
	    *secpp = coff_section_from_bfd_index (input_bfd, isymp->n_scnum);
	  else
	    {
	      if (isymp->n_value == 0)
		*secpp = bfd_und_section_ptr;
	      else
		*secpp = bfd_com_section_ptr;
	    }

	  esym += (isymp->n_numaux + 1) * symesz;
	  secpp += isymp->n_numaux + 1;
	  isymp += isymp->n_numaux + 1;
	}

      if (! sh_relocate_section (output_bfd, link_info, input_bfd,
				 input_section, data, internal_relocs,
				 internal_syms, sections))
	goto error_return;

      free (sections);
      sections = NULL;
      free (internal_syms);
      internal_syms = NULL;
      free (internal_relocs);
      internal_relocs = NULL;
    }

  return data;

 error_return:
  if (internal_relocs != NULL)
    free (internal_relocs);
  if (internal_syms != NULL)
    free (internal_syms);
  if (sections != NULL)
    free (sections);
  return NULL;
}

/* The target vectors.  */

const bfd_target shcoff_vec =
{
  "coff-sh",			/* name */
  bfd_target_coff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | BFD_IS_RELAXABLE),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC),
  '_',				/* leading symbol underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

  {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
     bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive, /* bfd_set_format */
     bfd_false},
  {bfd_false, coff_write_object_contents, /* bfd_write_contents */
     _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (coff),
  BFD_JUMP_TABLE_COPY (coff),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
  BFD_JUMP_TABLE_SYMBOLS (coff),
  BFD_JUMP_TABLE_RELOCS (coff),
  BFD_JUMP_TABLE_WRITE (coff),
  BFD_JUMP_TABLE_LINK (coff),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  COFF_SWAP_TABLE,
};

const bfd_target shlcoff_vec =
{
  "coff-shl",			/* name */
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little endian too*/

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | BFD_IS_RELAXABLE),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC),
  '_',				/* leading symbol underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

  {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
     bfd_generic_archive_p, _bfd_dummy_target},   
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive, /* bfd_set_format */
     bfd_false},
  {bfd_false, coff_write_object_contents, /* bfd_write_contents */
     _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (coff),
  BFD_JUMP_TABLE_COPY (coff),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
  BFD_JUMP_TABLE_SYMBOLS (coff),
  BFD_JUMP_TABLE_RELOCS (coff),
  BFD_JUMP_TABLE_WRITE (coff),
  BFD_JUMP_TABLE_LINK (coff),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  COFF_SWAP_TABLE,
};
