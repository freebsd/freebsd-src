/* BFD back-end for TMS320C30 a.out binaries.
   Copyright 1998, 1999, 2000 Free Software Foundation, Inc.
   Contributed by Steven Haworth (steve@pm.cse.rmit.edu.au)

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define TARGET_IS_BIG_ENDIAN_P
#define N_HEADER_IN_TEXT(x) 1
#define BYTES_IN_WORD 4
#define TEXT_START_ADDR 1024
#define TARGET_PAGE_SIZE 128
#define SEGMENT_SIZE TARGET_PAGE_SIZE
#define DEFAULT_ARCH bfd_arch_tic30
#define ARCH_SIZE 32

#define MY(OP) CAT(tic30_aout_,OP)
#define TARGETNAME "a.out-tic30"
#define NAME(x,y) CAT3(tic30_aout,_32_,y)

#include "bfd.h"
#include "sysdep.h"
#include "libaout.h"

#include "aout/aout64.h"
#include "aout/stab_gnu.h"
#include "aout/ar.h"

static bfd_reloc_status_type tic30_aout_fix_16
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type tic30_aout_fix_32
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type tic30_aout_fix_pcrel_16
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *tic30_aout_reloc_howto
  PARAMS ((bfd *, struct reloc_std_external *, int *, int *, int *));
static bfd_reloc_status_type tic30_aout_relocate_contents
  PARAMS ((reloc_howto_type *, bfd *, bfd_vma, bfd_byte *));
static bfd_reloc_status_type tic30_aout_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *, bfd_vma,
	   bfd_vma, bfd_vma));
static const bfd_target *tic30_aout_object_p PARAMS ((bfd *));
static boolean tic30_aout_write_object_contents PARAMS ((bfd *));
static boolean tic30_aout_set_sizes PARAMS ((bfd *));

#define MY_reloc_howto(BFD,REL,IN,EX,PC) tic30_aout_reloc_howto(BFD,REL,&IN,&EX,&PC)
#define MY_final_link_relocate tic30_aout_final_link_relocate
#define MY_object_p tic30_aout_object_p
#define MY_mkobject NAME(aout,mkobject)
#define MY_write_object_contents tic30_aout_write_object_contents
#define MY_set_sizes tic30_aout_set_sizes

#ifndef MY_exec_hdr_flags
#define MY_exec_hdr_flags 1
#endif

#ifndef MY_backend_data

#ifndef MY_zmagic_contiguous
#define MY_zmagic_contiguous 0
#endif
#ifndef MY_text_includes_header
#define MY_text_includes_header 0
#endif
#ifndef MY_entry_is_text_address
#define MY_entry_is_text_address 0
#endif
#ifndef MY_exec_header_not_counted
#define MY_exec_header_not_counted 1
#endif
#ifndef MY_add_dynamic_symbols
#define MY_add_dynamic_symbols 0
#endif
#ifndef MY_add_one_symbol
#define MY_add_one_symbol 0
#endif
#ifndef MY_link_dynamic_object
#define MY_link_dynamic_object 0
#endif
#ifndef MY_write_dynamic_symbol
#define MY_write_dynamic_symbol 0
#endif
#ifndef MY_check_dynamic_reloc
#define MY_check_dynamic_reloc 0
#endif
#ifndef MY_finish_dynamic_link
#define MY_finish_dynamic_link 0
#endif

static CONST struct aout_backend_data tic30_aout_backend_data =
{
  MY_zmagic_contiguous,
  MY_text_includes_header,
  MY_entry_is_text_address,
  MY_exec_hdr_flags,
  0,				/* text vma? */
  MY_set_sizes,
  MY_exec_header_not_counted,
  MY_add_dynamic_symbols,
  MY_add_one_symbol,
  MY_link_dynamic_object,
  MY_write_dynamic_symbol,
  MY_check_dynamic_reloc,
  MY_finish_dynamic_link
};
#define MY_backend_data &tic30_aout_backend_data
#endif

/* FIXME: This is wrong.  aoutx.h should really only be included by
   aout32.c.  */

#include "aoutx.h"

/* This table lists the relocation types for the TMS320C30.  There are
   only a few relocations required, and all must be divided by 4 (>>
   2) to get the 32-bit addresses in the format the TMS320C30 likes
   it.  */
reloc_howto_type tic30_aout_howto_table[] =
{
  EMPTY_HOWTO (-1),
  HOWTO (1, 2, 1, 16, false, 0, 0, tic30_aout_fix_16,
	 "16", false, 0x0000FFFF, 0x0000FFFF, false),
  HOWTO (2, 2, 2, 24, false, 0, complain_overflow_bitfield, NULL,
	 "24", false, 0x00FFFFFF, 0x00FFFFFF, false),
  HOWTO (3, 18, 3, 24, false, 0, complain_overflow_bitfield, NULL,
	 "LDP", false, 0x00FF0000, 0x000000FF, false),
  HOWTO (4, 2, 4, 32, false, 0, complain_overflow_bitfield, tic30_aout_fix_32,
	 "32", false, 0xFFFFFFFF, 0xFFFFFFFF, false),
  HOWTO (5, 2, 1, 16, true, 0, complain_overflow_signed,
	 tic30_aout_fix_pcrel_16, "PCREL", true, 0x0000FFFF, 0x0000FFFF, true),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1),
  EMPTY_HOWTO (-1)
};

extern reloc_howto_type *NAME (aout, reloc_type_lookup) ();

reloc_howto_type *
tic30_aout_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_8:
    case BFD_RELOC_TIC30_LDP:
      return &tic30_aout_howto_table[3];
    case BFD_RELOC_16:
      return &tic30_aout_howto_table[1];
    case BFD_RELOC_24:
      return &tic30_aout_howto_table[2];
    case BFD_RELOC_16_PCREL:
      return &tic30_aout_howto_table[5];
    case BFD_RELOC_32:
      return &tic30_aout_howto_table[4];
    default:
      return (reloc_howto_type *) NULL;
    }
}

static reloc_howto_type *
tic30_aout_reloc_howto (abfd, relocs, r_index, r_extern, r_pcrel)
     bfd *abfd;
     struct reloc_std_external *relocs;
     int *r_index;
     int *r_extern;
     int *r_pcrel;
{
  unsigned int r_length;
  unsigned int r_pcrel_done;
  int index;

  *r_pcrel = 0;
  if (bfd_header_big_endian (abfd))
    {
      *r_index = ((relocs->r_index[0] << 16) | (relocs->r_index[1] << 8) | relocs->r_index[2]);
      *r_extern = (0 != (relocs->r_type[0] & RELOC_STD_BITS_EXTERN_BIG));
      r_pcrel_done = (0 != (relocs->r_type[0] & RELOC_STD_BITS_PCREL_BIG));
      r_length = ((relocs->r_type[0] & RELOC_STD_BITS_LENGTH_BIG) >> RELOC_STD_BITS_LENGTH_SH_BIG);
    }
  else
    {
      *r_index = ((relocs->r_index[2] << 16) | (relocs->r_index[1] << 8) | relocs->r_index[0]);
      *r_extern = (0 != (relocs->r_type[0] & RELOC_STD_BITS_EXTERN_LITTLE));
      r_pcrel_done = (0 != (relocs->r_type[0] & RELOC_STD_BITS_PCREL_LITTLE));
      r_length = ((relocs->r_type[0] & RELOC_STD_BITS_LENGTH_LITTLE) >> RELOC_STD_BITS_LENGTH_SH_LITTLE);
    }
  index = r_length + 4 * r_pcrel_done;
  return tic30_aout_howto_table + index;
}

/* This function is used as a callback for 16-bit relocs.  This is
   required for relocations between segments.  A line in aoutx.h
   requires that any relocations for the data section should point to
   the end of the aligned text section, plus an offset.  By default,
   this does not happen, therefore this function takes care of
   that.  */

static bfd_reloc_status_type
tic30_aout_fix_16 (abfd, reloc_entry, symbol, data, input_section, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;

  /* Make sure that the symbol's section is defined.  */
  if (symbol->section == &bfd_und_section && (symbol->flags & BSF_WEAK) == 0)
    return output_bfd ? bfd_reloc_ok : bfd_reloc_undefined;
  /* Get the size of the input section and turn it into the TMS320C30
     32-bit address format.  */
  relocation = (symbol->section->vma >> 2);
  relocation += bfd_get_16 (abfd, (bfd_byte *) data + reloc_entry->address);
  bfd_put_16 (abfd, relocation, (bfd_byte *) data + reloc_entry->address);
  return bfd_reloc_ok;
}

/* This function does the same thing as tic30_aout_fix_16 except for 32
   bit relocations.  */

static bfd_reloc_status_type
tic30_aout_fix_32 (abfd, reloc_entry, symbol, data, input_section,
		   output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;

  /* Make sure that the symbol's section is defined.  */
  if (symbol->section == &bfd_und_section && (symbol->flags & BSF_WEAK) == 0)
    return output_bfd ? bfd_reloc_ok : bfd_reloc_undefined;
  /* Get the size of the input section and turn it into the TMS320C30
     32-bit address format.  */
  relocation = (symbol->section->vma >> 2);
  relocation += bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);
  bfd_put_32 (abfd, relocation, (bfd_byte *) data + reloc_entry->address);
  return bfd_reloc_ok;
}

/* This function is used to work out pc-relative offsets for the
   TMS320C30.  The data already placed by md_pcrel_from within gas is
   useless for a relocation, so we just get the offset value and place
   a version of this within the object code.
   tic30_aout_final_link_relocate will then calculate the required
   relocation to add on to the value in the object code.  */

static bfd_reloc_status_type
tic30_aout_fix_pcrel_16 (abfd, reloc_entry, symbol, data, input_section,
			 output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol ATTRIBUTE_UNUSED;
     PTR data;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation = 1;
  bfd_byte offset_data = bfd_get_8 (abfd, (bfd_byte *) data + reloc_entry->address - 1);

  /* The byte before the location of the fix contains bits 23-16 of
     the pcrel instruction.  Bit 21 is set for a delayed instruction
     which requires on offset of 3 instead of 1.  */
  if (offset_data & 0x20)
    relocation -= 3;
  else
    relocation -= 1;
  bfd_put_16 (abfd, relocation, (bfd_byte *) data + reloc_entry->address);
  return bfd_reloc_ok;
}

/* These macros will get 24-bit values from the bfd definition.
   Big-endian only.  */
#define bfd_getb_24(BFD,ADDR)			\
 (bfd_get_8 (BFD, ADDR    ) << 16) |		\
 (bfd_get_8 (BFD, ADDR + 1) <<  8) |		\
 (bfd_get_8 (BFD, ADDR + 2)      )

#define bfd_putb_24(BFD,DATA,ADDR)				\
 bfd_put_8 (BFD, (bfd_byte) ((DATA >> 16) & 0xFF), ADDR    );	\
 bfd_put_8 (BFD, (bfd_byte) ((DATA >>  8) & 0xFF), ADDR + 1);	\
 bfd_put_8 (BFD, (bfd_byte) ( DATA        & 0xFF), ADDR + 2)

/* Set parameters about this a.out file that are machine-dependent.
   This routine is called from some_aout_object_p just before it returns.  */

static const bfd_target *
tic30_aout_callback (abfd)
     bfd *abfd;
{
  struct internal_exec *execp = exec_hdr (abfd);
  unsigned int arch_align_power;
  unsigned long arch_align;

  /* Calculate the file positions of the parts of a newly read aout header */
  obj_textsec (abfd)->_raw_size = N_TXTSIZE (*execp);

  /* The virtual memory addresses of the sections */
  obj_textsec (abfd)->vma = N_TXTADDR (*execp);
  obj_datasec (abfd)->vma = N_DATADDR (*execp);
  obj_bsssec (abfd)->vma = N_BSSADDR (*execp);

  obj_textsec (abfd)->lma = obj_textsec (abfd)->vma;
  obj_datasec (abfd)->lma = obj_datasec (abfd)->vma;
  obj_bsssec (abfd)->lma = obj_bsssec (abfd)->vma;

  /* The file offsets of the sections */
  obj_textsec (abfd)->filepos = N_TXTOFF (*execp);
  obj_datasec (abfd)->filepos = N_DATOFF (*execp);

  /* The file offsets of the relocation info */
  obj_textsec (abfd)->rel_filepos = N_TRELOFF (*execp);
  obj_datasec (abfd)->rel_filepos = N_DRELOFF (*execp);

  /* The file offsets of the string table and symbol table.  */
  obj_sym_filepos (abfd) = N_SYMOFF (*execp);
  obj_str_filepos (abfd) = N_STROFF (*execp);

  /* Determine the architecture and machine type of the object file.  */
#ifdef SET_ARCH_MACH
  SET_ARCH_MACH (abfd, *execp);
#else
  bfd_default_set_arch_mach (abfd, DEFAULT_ARCH, 0);
#endif

  /* Now that we know the architecture, set the alignments of the
     sections.  This is normally done by NAME(aout,new_section_hook),
     but when the initial sections were created the architecture had
     not yet been set.  However, for backward compatibility, we don't
     set the alignment power any higher than as required by the size
     of the section.  */
  arch_align_power = bfd_get_arch_info (abfd)->section_align_power;
  arch_align = 1 << arch_align_power;
  if ((BFD_ALIGN (obj_textsec (abfd)->_raw_size, arch_align)
       == obj_textsec (abfd)->_raw_size)
      && (BFD_ALIGN (obj_datasec (abfd)->_raw_size, arch_align)
	  == obj_datasec (abfd)->_raw_size)
      && (BFD_ALIGN (obj_bsssec (abfd)->_raw_size, arch_align)
	  == obj_bsssec (abfd)->_raw_size))
    {
      obj_textsec (abfd)->alignment_power = arch_align_power;
      obj_datasec (abfd)->alignment_power = arch_align_power;
      obj_bsssec (abfd)->alignment_power = arch_align_power;
    }
  return abfd->xvec;
}

static bfd_reloc_status_type
tic30_aout_final_link_relocate (howto, input_bfd, input_section, contents,
				address, value, addend)
     reloc_howto_type *howto;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     bfd_vma address;
     bfd_vma value;
     bfd_vma addend;
{
  bfd_vma relocation;

  if (address > input_section->_raw_size)
    return bfd_reloc_outofrange;

  relocation = value + addend;
  if (howto->pc_relative)
    {
      relocation -= (input_section->output_section->vma + input_section->output_offset);
      if (howto->pcrel_offset)
	relocation -= address;
    }
  return tic30_aout_relocate_contents (howto, input_bfd, relocation,
				       contents + address);
}

bfd_reloc_status_type
tic30_aout_relocate_contents (howto, input_bfd, relocation, location)
     reloc_howto_type *howto;
     bfd *input_bfd;
     bfd_vma relocation;
     bfd_byte *location;
{
  bfd_vma x;
  boolean overflow;

  if (howto->size < 0)
    relocation = -relocation;

  switch (howto->size)
    {
    default:
    case 0:
      abort ();
      break;
    case 1:
      x = bfd_get_16 (input_bfd, location);
      break;
    case 2:
      x = bfd_getb_24 (input_bfd, location);
      break;
    case 3:
      x = bfd_get_8 (input_bfd, location);
      break;
    case 4:
      x = bfd_get_32 (input_bfd, location);
      break;
    }
  overflow = false;
  if (howto->complain_on_overflow != complain_overflow_dont)
    {
      bfd_vma check;
      bfd_signed_vma signed_check;
      bfd_vma add;
      bfd_signed_vma signed_add;

      if (howto->rightshift == 0)
	{
	  check = relocation;
	  signed_check = (bfd_signed_vma) relocation;
	}
      else
	{
	  check = relocation >> howto->rightshift;
	  if ((bfd_signed_vma) relocation >= 0)
	    signed_check = check;
	  else
	    signed_check = (check | ((bfd_vma) - 1 & ~((bfd_vma) - 1 >> howto->rightshift)));
	}
      add = x & howto->src_mask;
      signed_add = add;
      if ((add & (((~howto->src_mask) >> 1) & howto->src_mask)) != 0)
	signed_add -= (((~howto->src_mask) >> 1) & howto->src_mask) << 1;
      if (howto->bitpos == 0)
	{
	  check += add;
	  signed_check += signed_add;
	}
      else
	{
	  check += add >> howto->bitpos;
	  if (signed_add >= 0)
	    signed_check += add >> howto->bitpos;
	  else
	    signed_check += ((add >> howto->bitpos) | ((bfd_vma) - 1 & ~((bfd_vma) - 1 >> howto->bitpos)));
	}
      switch (howto->complain_on_overflow)
	{
	case complain_overflow_signed:
	  {
	    bfd_signed_vma reloc_signed_max = (1 << (howto->bitsize - 1)) - 1;
	    bfd_signed_vma reloc_signed_min = ~reloc_signed_max;
	    if (signed_check > reloc_signed_max || signed_check < reloc_signed_min)
	      overflow = true;
	  }
	  break;
	case complain_overflow_unsigned:
	  {
	    bfd_vma reloc_unsigned_max = (((1 << (howto->bitsize - 1)) - 1) << 1) | 1;
	    if (check > reloc_unsigned_max)
	      overflow = true;
	  }
	  break;
	case complain_overflow_bitfield:
	  {
	    bfd_vma reloc_bits = (((1 << (howto->bitsize - 1)) - 1) << 1) | 1;
	    if ((check & ~reloc_bits) != 0 && (((bfd_vma) signed_check & ~reloc_bits) != (-1 & ~reloc_bits)))
	      overflow = true;
	  }
	  break;
	default:
	  abort ();
	}
    }
  relocation >>= (bfd_vma) howto->rightshift;
  relocation <<= (bfd_vma) howto->bitpos;
  x = ((x & ~howto->dst_mask) | (((x & howto->src_mask) + relocation) & howto->dst_mask));
  switch (howto->size)
    {
    default:
    case 0:
      abort ();
      break;
    case 1:
      bfd_put_16 (input_bfd, x, location);
      break;
    case 2:
      bfd_putb_24 (input_bfd, x, location);
      break;
    case 3:
      bfd_put_8 (input_bfd, x, location);
      break;
    case 4:
      bfd_put_32 (input_bfd, x, location);
      break;
    }
  return overflow ? bfd_reloc_overflow : bfd_reloc_ok;
}

/* Finish up the reading of an a.out file header */

static const bfd_target *
tic30_aout_object_p (abfd)
     bfd *abfd;
{
  struct external_exec exec_bytes;	/* Raw exec header from file */
  struct internal_exec exec;	/* Cleaned-up exec header */
  const bfd_target *target;

  if (bfd_read ((PTR) & exec_bytes, 1, EXEC_BYTES_SIZE, abfd)
      != EXEC_BYTES_SIZE)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

#ifdef SWAP_MAGIC
  exec.a_info = SWAP_MAGIC (exec_bytes.e_info);
#else
  exec.a_info = bfd_h_get_32 (abfd, exec_bytes.e_info);
#endif /* SWAP_MAGIC */

  if (N_BADMAG (exec))
    return 0;
#ifdef MACHTYPE_OK
  if (!(MACHTYPE_OK (N_MACHTYPE (exec))))
    return 0;
#endif

  NAME (aout, swap_exec_header_in) (abfd, &exec_bytes, &exec);

#ifdef SWAP_MAGIC
  /* swap_exec_header_in read in a_info with the wrong byte order */
  exec.a_info = SWAP_MAGIC (exec_bytes.e_info);
#endif /* SWAP_MAGIC */

  target = NAME (aout, some_aout_object_p) (abfd, &exec, tic30_aout_callback);

#ifdef ENTRY_CAN_BE_ZERO
  /* The NEWSOS3 entry-point is/was 0, which (amongst other lossage)
   * means that it isn't obvious if EXEC_P should be set.
   * All of the following must be true for an executable:
   * There must be no relocations, the bfd can be neither an
   * archive nor an archive element, and the file must be executable.  */

  if (exec.a_trsize + exec.a_drsize == 0
      && bfd_get_format (abfd) == bfd_object && abfd->my_archive == NULL)
    {
      struct stat buf;
#ifndef S_IXUSR
#define S_IXUSR 0100		/* Execute by owner.  */
#endif
      if (stat (abfd->filename, &buf) == 0 && (buf.st_mode & S_IXUSR))
	abfd->flags |= EXEC_P;
    }
#endif /* ENTRY_CAN_BE_ZERO */

  return target;
}

/* Copy private section data.  This actually does nothing with the
   sections.  It copies the subformat field.  We copy it here, because
   we need to know whether this is a QMAGIC file before we set the
   section contents, and copy_private_bfd_data is not called until
   after the section contents have been set.  */

static boolean
MY_bfd_copy_private_section_data (ibfd, isec, obfd, osec)
     bfd *ibfd;
     asection *isec ATTRIBUTE_UNUSED;
     bfd *obfd;
     asection *osec ATTRIBUTE_UNUSED;
{
  if (bfd_get_flavour (obfd) == bfd_target_aout_flavour)
    obj_aout_subformat (obfd) = obj_aout_subformat (ibfd);
  return true;
}

/* Write an object file.
   Section contents have already been written.  We write the
   file header, symbols, and relocation.  */

static boolean
tic30_aout_write_object_contents (abfd)
     bfd *abfd;
{
  struct external_exec exec_bytes;
  struct internal_exec *execp = exec_hdr (abfd);

  obj_reloc_entry_size (abfd) = RELOC_STD_SIZE;

  {
    bfd_size_type text_size;	/* dummy vars */
    file_ptr text_end;
    if (adata (abfd).magic == undecided_magic)
      NAME (aout, adjust_sizes_and_vmas) (abfd, &text_size, &text_end);

    execp->a_syms = bfd_get_symcount (abfd) * EXTERNAL_NLIST_SIZE;
    execp->a_entry = bfd_get_start_address (abfd);

    execp->a_trsize = ((obj_textsec (abfd)->reloc_count) * obj_reloc_entry_size (abfd));
    execp->a_drsize = ((obj_datasec (abfd)->reloc_count) * obj_reloc_entry_size (abfd));
    NAME (aout, swap_exec_header_out) (abfd, execp, &exec_bytes);

    if (adata (abfd).exec_bytes_size > 0)
      {
	if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
	  return false;
	if (bfd_write ((PTR) & exec_bytes, 1, adata (abfd).exec_bytes_size, abfd) != adata (abfd).exec_bytes_size)
	  return false;
      }
    /* Now write out reloc info, followed by syms and strings */

    if (bfd_get_outsymbols (abfd) != (asymbol **) NULL
	&& bfd_get_symcount (abfd) != 0)
      {
	if (bfd_seek (abfd, (file_ptr) (N_SYMOFF (*execp)), SEEK_SET) != 0)
	  return false;

	if (!NAME (aout, write_syms) (abfd))
	  return false;
      }

    if (bfd_seek (abfd, (file_ptr) (N_TRELOFF (*execp)), SEEK_SET) != 0)
      return false;
    if (!NAME (aout, squirt_out_relocs) (abfd, obj_textsec (abfd)))
      return false;

    if (bfd_seek (abfd, (file_ptr) (N_DRELOFF (*execp)), SEEK_SET) != 0)
      return false;
    if (!NAME (aout, squirt_out_relocs) (abfd, obj_datasec (abfd)))
      return false;
  }

  return true;
}

static boolean
tic30_aout_set_sizes (abfd)
     bfd *abfd;
{
  adata (abfd).page_size = TARGET_PAGE_SIZE;

#ifdef SEGMENT_SIZE
  adata (abfd).segment_size = SEGMENT_SIZE;
#else
  adata (abfd).segment_size = TARGET_PAGE_SIZE;
#endif

#ifdef ZMAGIC_DISK_BLOCK_SIZE
  adata (abfd).zmagic_disk_block_size = ZMAGIC_DISK_BLOCK_SIZE;
#else
  adata (abfd).zmagic_disk_block_size = TARGET_PAGE_SIZE;
#endif

  adata (abfd).exec_bytes_size = EXEC_BYTES_SIZE;

  return true;
}

#ifndef MY_final_link_callback

/* Callback for the final_link routine to set the section offsets.  */

static void MY_final_link_callback
  PARAMS ((bfd *, file_ptr *, file_ptr *, file_ptr *));

static void
MY_final_link_callback (abfd, ptreloff, pdreloff, psymoff)
     bfd *abfd;
     file_ptr *ptreloff;
     file_ptr *pdreloff;
     file_ptr *psymoff;
{
  struct internal_exec *execp = exec_hdr (abfd);

  *ptreloff = obj_datasec (abfd)->filepos + execp->a_data;
  *pdreloff = *ptreloff + execp->a_trsize;
  *psymoff = *pdreloff + execp->a_drsize;;
}

#endif

#ifndef MY_bfd_final_link

/* Final link routine.  We need to use a call back to get the correct
   offsets in the output file.  */

static boolean
MY_bfd_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  struct internal_exec *execp = exec_hdr (abfd);
  file_ptr pos;
  bfd_vma vma = 0;
  int pad;

  /* Set the executable header size to 0, as we don't want one for an
     output.  */
  adata (abfd).exec_bytes_size = 0;
  pos = adata (abfd).exec_bytes_size;
  /* Text.  */
  vma = info->create_object_symbols_section->vma;
  pos += vma;
  obj_textsec (abfd)->filepos = pos;
  obj_textsec (abfd)->vma = vma;
  obj_textsec (abfd)->user_set_vma = 1;
  pos += obj_textsec (abfd)->_raw_size;
  vma += obj_textsec (abfd)->_raw_size;

  /* Data.  */
  if (abfd->flags & D_PAGED)
    {
      if (info->create_object_symbols_section->next->vma > 0)
	obj_datasec (abfd)->vma = info->create_object_symbols_section->next->vma;
      else
	obj_datasec (abfd)->vma = BFD_ALIGN (vma, adata (abfd).segment_size);
    }
  else
    {
      obj_datasec (abfd)->vma = BFD_ALIGN (vma, 4);
    }
  if (obj_datasec (abfd)->vma < vma)
    {
      obj_datasec (abfd)->vma = BFD_ALIGN (vma, 4);
    }
  obj_datasec (abfd)->user_set_vma = 1;
  vma = obj_datasec (abfd)->vma;
  obj_datasec (abfd)->filepos = vma + adata (abfd).exec_bytes_size;
  execp->a_text = vma - obj_textsec (abfd)->vma;
  obj_textsec (abfd)->_raw_size = execp->a_text;

  /* Since BSS follows data immediately, see if it needs alignment.  */
  vma += obj_datasec (abfd)->_raw_size;
  pad = align_power (vma, obj_bsssec (abfd)->alignment_power) - vma;
  obj_datasec (abfd)->_raw_size += pad;
  pos += obj_datasec (abfd)->_raw_size;
  execp->a_data = obj_datasec (abfd)->_raw_size;

  /* BSS.  */
  obj_bsssec (abfd)->vma = vma;
  obj_bsssec (abfd)->user_set_vma = 1;

  /* We are fully resized, so don't readjust in final_link.  */
  adata (abfd).magic = z_magic;

  return NAME (aout, final_link) (abfd, info, MY_final_link_callback);
}

#endif

enum machine_type
tic30_aout_machine_type (arch, machine, unknown)
     enum bfd_architecture arch;
     unsigned long machine ATTRIBUTE_UNUSED;
     boolean *unknown;
{
  enum machine_type arch_flags;

  arch_flags = M_UNKNOWN;
  *unknown = true;

  switch (arch)
    {
    case bfd_arch_tic30:
      *unknown = false;
      break;
    default:
      arch_flags = M_UNKNOWN;
    }
  if (arch_flags != M_UNKNOWN)
    *unknown = false;
  return arch_flags;
}

boolean
tic30_aout_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  if (!bfd_default_set_arch_mach (abfd, arch, machine))
    return false;
  if (arch != bfd_arch_unknown)
    {
      boolean unknown;
      tic30_aout_machine_type (arch, machine, &unknown);
      if (unknown)
	return false;
    }
  obj_reloc_entry_size (abfd) = RELOC_STD_SIZE;
  return (*aout_backend_info (abfd)->set_sizes) (abfd);
}

/* We assume BFD generic archive files.  */
#ifndef	MY_openr_next_archived_file
#define	MY_openr_next_archived_file	bfd_generic_openr_next_archived_file
#endif
#ifndef MY_get_elt_at_index
#define MY_get_elt_at_index		_bfd_generic_get_elt_at_index
#endif
#ifndef	MY_generic_stat_arch_elt
#define	MY_generic_stat_arch_elt	bfd_generic_stat_arch_elt
#endif
#ifndef	MY_slurp_armap
#define	MY_slurp_armap			bfd_slurp_bsd_armap
#endif
#ifndef	MY_slurp_extended_name_table
#define	MY_slurp_extended_name_table	_bfd_slurp_extended_name_table
#endif
#ifndef MY_construct_extended_name_table
#define MY_construct_extended_name_table \
  _bfd_archive_bsd_construct_extended_name_table
#endif
#ifndef	MY_write_armap
#define	MY_write_armap		bsd_write_armap
#endif
#ifndef MY_read_ar_hdr
#define MY_read_ar_hdr		_bfd_generic_read_ar_hdr
#endif
#ifndef	MY_truncate_arname
#define	MY_truncate_arname		bfd_bsd_truncate_arname
#endif
#ifndef MY_update_armap_timestamp
#define MY_update_armap_timestamp _bfd_archive_bsd_update_armap_timestamp
#endif

/* No core file defined here -- configure in trad-core.c separately.  */
#ifndef	MY_core_file_failing_command
#define	MY_core_file_failing_command _bfd_nocore_core_file_failing_command
#endif
#ifndef	MY_core_file_failing_signal
#define	MY_core_file_failing_signal	_bfd_nocore_core_file_failing_signal
#endif
#ifndef	MY_core_file_matches_executable_p
#define	MY_core_file_matches_executable_p	\
				_bfd_nocore_core_file_matches_executable_p
#endif
#ifndef	MY_core_file_p
#define	MY_core_file_p		_bfd_dummy_target
#endif

#ifndef MY_bfd_debug_info_start
#define MY_bfd_debug_info_start		bfd_void
#endif
#ifndef MY_bfd_debug_info_end
#define MY_bfd_debug_info_end		bfd_void
#endif
#ifndef MY_bfd_debug_info_accumulate
#define MY_bfd_debug_info_accumulate	\
			(void (*) PARAMS ((bfd*, struct sec *))) bfd_void
#endif

#ifndef MY_core_file_failing_command
#define MY_core_file_failing_command NAME(aout,core_file_failing_command)
#endif
#ifndef MY_core_file_failing_signal
#define MY_core_file_failing_signal NAME(aout,core_file_failing_signal)
#endif
#ifndef MY_core_file_matches_executable_p
#define MY_core_file_matches_executable_p NAME(aout,core_file_matches_executable_p)
#endif
#ifndef MY_set_section_contents
#define MY_set_section_contents NAME(aout,set_section_contents)
#endif
#ifndef MY_get_section_contents
#define MY_get_section_contents aout_32_get_section_contents
#endif
#ifndef MY_get_section_contents_in_window
#define MY_get_section_contents_in_window _bfd_generic_get_section_contents_in_window
#endif
#ifndef MY_new_section_hook
#define MY_new_section_hook NAME(aout,new_section_hook)
#endif
#ifndef MY_get_symtab_upper_bound
#define MY_get_symtab_upper_bound NAME(aout,get_symtab_upper_bound)
#endif
#ifndef MY_get_symtab
#define MY_get_symtab NAME(aout,get_symtab)
#endif
#ifndef MY_get_reloc_upper_bound
#define MY_get_reloc_upper_bound NAME(aout,get_reloc_upper_bound)
#endif
#ifndef MY_canonicalize_reloc
#define MY_canonicalize_reloc NAME(aout,canonicalize_reloc)
#endif
#ifndef MY_make_empty_symbol
#define MY_make_empty_symbol NAME(aout,make_empty_symbol)
#endif
#ifndef MY_print_symbol
#define MY_print_symbol NAME(aout,print_symbol)
#endif
#ifndef MY_get_symbol_info
#define MY_get_symbol_info NAME(aout,get_symbol_info)
#endif
#ifndef MY_get_lineno
#define MY_get_lineno NAME(aout,get_lineno)
#endif
#ifndef MY_set_arch_mach
#define MY_set_arch_mach tic30_aout_set_arch_mach
#endif
#ifndef MY_find_nearest_line
#define MY_find_nearest_line NAME(aout,find_nearest_line)
#endif
#ifndef MY_sizeof_headers
#define MY_sizeof_headers NAME(aout,sizeof_headers)
#endif
#ifndef MY_bfd_get_relocated_section_contents
#define MY_bfd_get_relocated_section_contents \
			bfd_generic_get_relocated_section_contents
#endif
#ifndef MY_bfd_relax_section
#define MY_bfd_relax_section bfd_generic_relax_section
#endif
#ifndef MY_bfd_gc_sections
#define MY_bfd_gc_sections bfd_generic_gc_sections
#endif
#ifndef MY_bfd_reloc_type_lookup
#define MY_bfd_reloc_type_lookup tic30_aout_reloc_type_lookup
#endif
#ifndef MY_bfd_make_debug_symbol
#define MY_bfd_make_debug_symbol 0
#endif
#ifndef MY_read_minisymbols
#define MY_read_minisymbols NAME(aout,read_minisymbols)
#endif
#ifndef MY_minisymbol_to_symbol
#define MY_minisymbol_to_symbol NAME(aout,minisymbol_to_symbol)
#endif
#ifndef MY_bfd_link_hash_table_create
#define MY_bfd_link_hash_table_create NAME(aout,link_hash_table_create)
#endif
#ifndef MY_bfd_link_add_symbols
#define MY_bfd_link_add_symbols NAME(aout,link_add_symbols)
#endif
#ifndef MY_bfd_link_split_section
#define MY_bfd_link_split_section  _bfd_generic_link_split_section
#endif

#ifndef MY_bfd_copy_private_bfd_data
#define MY_bfd_copy_private_bfd_data _bfd_generic_bfd_copy_private_bfd_data
#endif

#ifndef MY_bfd_merge_private_bfd_data
#define MY_bfd_merge_private_bfd_data _bfd_generic_bfd_merge_private_bfd_data
#endif

#ifndef MY_bfd_copy_private_symbol_data
#define MY_bfd_copy_private_symbol_data _bfd_generic_bfd_copy_private_symbol_data
#endif

#ifndef MY_bfd_print_private_bfd_data
#define MY_bfd_print_private_bfd_data _bfd_generic_bfd_print_private_bfd_data
#endif

#ifndef MY_bfd_set_private_flags
#define MY_bfd_set_private_flags _bfd_generic_bfd_set_private_flags
#endif

#ifndef MY_bfd_is_local_label_name
#define MY_bfd_is_local_label_name bfd_generic_is_local_label_name
#endif

#ifndef MY_bfd_free_cached_info
#define MY_bfd_free_cached_info NAME(aout,bfd_free_cached_info)
#endif

#ifndef MY_close_and_cleanup
#define MY_close_and_cleanup MY_bfd_free_cached_info
#endif

#ifndef MY_get_dynamic_symtab_upper_bound
#define MY_get_dynamic_symtab_upper_bound \
  _bfd_nodynamic_get_dynamic_symtab_upper_bound
#endif
#ifndef MY_canonicalize_dynamic_symtab
#define MY_canonicalize_dynamic_symtab \
  _bfd_nodynamic_canonicalize_dynamic_symtab
#endif
#ifndef MY_get_dynamic_reloc_upper_bound
#define MY_get_dynamic_reloc_upper_bound \
  _bfd_nodynamic_get_dynamic_reloc_upper_bound
#endif
#ifndef MY_canonicalize_dynamic_reloc
#define MY_canonicalize_dynamic_reloc \
  _bfd_nodynamic_canonicalize_dynamic_reloc
#endif

/* Aout symbols normally have leading underscores */
#ifndef MY_symbol_leading_char
#define MY_symbol_leading_char '_'
#endif

/* Aout archives normally use spaces for padding */
#ifndef AR_PAD_CHAR
#define AR_PAD_CHAR ' '
#endif

#ifndef MY_BFD_TARGET
const bfd_target tic30_aout_vec =
{
  TARGETNAME,			/* name */
  bfd_target_aout_flavour,
  BFD_ENDIAN_BIG,		/* target byte order (big) */
  BFD_ENDIAN_BIG,		/* target headers byte order (big) */
  (HAS_RELOC |			/* object flags */
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC),	/* section flags */
  MY_symbol_leading_char,
  AR_PAD_CHAR,			/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */
  {_bfd_dummy_target, MY_object_p,	/* bfd_check_format */
   bfd_generic_archive_p, MY_core_file_p},
  {bfd_false, MY_mkobject,	/* bfd_set_format */
   _bfd_generic_mkarchive, bfd_false},
  {bfd_false, MY_write_object_contents,		/* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (MY),
  BFD_JUMP_TABLE_COPY (MY),
  BFD_JUMP_TABLE_CORE (MY),
  BFD_JUMP_TABLE_ARCHIVE (MY),
  BFD_JUMP_TABLE_SYMBOLS (MY),
  BFD_JUMP_TABLE_RELOCS (MY),
  BFD_JUMP_TABLE_WRITE (MY),
  BFD_JUMP_TABLE_LINK (MY),
  BFD_JUMP_TABLE_DYNAMIC (MY),

  NULL,

  (PTR) MY_backend_data
};
#endif /* MY_BFD_TARGET */
