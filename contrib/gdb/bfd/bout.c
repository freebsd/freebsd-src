/* BFD back-end for Intel 960 b.out binaries.
   Copyright 1990, 91, 92, 93, 94, 95, 1996 Free Software Foundation, Inc.
   Written by Cygnus Support.

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
#include "bfdlink.h"
#include "genlink.h"
#include "bout.h"

#include "aout/stab_gnu.h"
#include "libaout.h"		/* BFD a.out internal data structures */


static int aligncode PARAMS ((bfd *abfd, asection *input_section,
			      arelent *r, unsigned int shrink));
static void perform_slip PARAMS ((bfd *abfd, unsigned int slip,
				  asection *input_section, bfd_vma value));
static boolean b_out_squirt_out_relocs PARAMS ((bfd *abfd, asection *section));
static const bfd_target *b_out_callback PARAMS ((bfd *));
static bfd_reloc_status_type calljx_callback
  PARAMS ((bfd *, struct bfd_link_info *, arelent *, PTR src, PTR dst,
	   asection *));
static bfd_reloc_status_type callj_callback
  PARAMS ((bfd *, struct bfd_link_info *, arelent *, PTR data,
	   unsigned int srcidx, unsigned int dstidx, asection *, boolean));
static bfd_vma get_value PARAMS ((arelent *, struct bfd_link_info *,
				  asection *));
static int abs32code PARAMS ((bfd *, asection *, arelent *,
			      unsigned int, struct bfd_link_info *));
static boolean b_out_bfd_relax_section PARAMS ((bfd *, asection *,
						struct bfd_link_info *,
						boolean *));
static bfd_byte *b_out_bfd_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *, boolean, asymbol **));

/* Swaps the information in an executable header taken from a raw byte
   stream memory image, into the internal exec_header structure.  */

void
bout_swap_exec_header_in (abfd, raw_bytes, execp)
     bfd *abfd;
     struct external_exec *raw_bytes;
     struct internal_exec *execp;
{
  struct external_exec *bytes = (struct external_exec *)raw_bytes;

  /* Now fill in fields in the execp, from the bytes in the raw data.  */
  execp->a_info   = bfd_h_get_32 (abfd, bytes->e_info);
  execp->a_text   = GET_WORD (abfd, bytes->e_text);
  execp->a_data   = GET_WORD (abfd, bytes->e_data);
  execp->a_bss    = GET_WORD (abfd, bytes->e_bss);
  execp->a_syms   = GET_WORD (abfd, bytes->e_syms);
  execp->a_entry  = GET_WORD (abfd, bytes->e_entry);
  execp->a_trsize = GET_WORD (abfd, bytes->e_trsize);
  execp->a_drsize = GET_WORD (abfd, bytes->e_drsize);
  execp->a_tload  = GET_WORD (abfd, bytes->e_tload);
  execp->a_dload  = GET_WORD (abfd, bytes->e_dload);
  execp->a_talign = bytes->e_talign[0];
  execp->a_dalign = bytes->e_dalign[0];
  execp->a_balign = bytes->e_balign[0];
  execp->a_relaxable = bytes->e_relaxable[0];
}

/* Swaps the information in an internal exec header structure into the
   supplied buffer ready for writing to disk.  */

PROTO(void, bout_swap_exec_header_out,
	  (bfd *abfd,
	   struct internal_exec *execp,
	   struct external_exec *raw_bytes));
void
bout_swap_exec_header_out (abfd, execp, raw_bytes)
     bfd *abfd;
     struct internal_exec *execp;
     struct external_exec *raw_bytes;
{
  struct external_exec *bytes = (struct external_exec *)raw_bytes;

  /* Now fill in fields in the raw data, from the fields in the exec struct. */
  bfd_h_put_32 (abfd, execp->a_info  , bytes->e_info);
  PUT_WORD (abfd, execp->a_text  , bytes->e_text);
  PUT_WORD (abfd, execp->a_data  , bytes->e_data);
  PUT_WORD (abfd, execp->a_bss   , bytes->e_bss);
  PUT_WORD (abfd, execp->a_syms  , bytes->e_syms);
  PUT_WORD (abfd, execp->a_entry , bytes->e_entry);
  PUT_WORD (abfd, execp->a_trsize, bytes->e_trsize);
  PUT_WORD (abfd, execp->a_drsize, bytes->e_drsize);
  PUT_WORD (abfd, execp->a_tload , bytes->e_tload);
  PUT_WORD (abfd, execp->a_dload , bytes->e_dload);
  bytes->e_talign[0] = execp->a_talign;
  bytes->e_dalign[0] = execp->a_dalign;
  bytes->e_balign[0] = execp->a_balign;
  bytes->e_relaxable[0] = execp->a_relaxable;
}


static const bfd_target *
b_out_object_p (abfd)
     bfd *abfd;
{
  struct internal_exec anexec;
  struct external_exec exec_bytes;

  if (bfd_read ((PTR) &exec_bytes, 1, EXEC_BYTES_SIZE, abfd)
      != EXEC_BYTES_SIZE) {
    if (bfd_get_error () != bfd_error_system_call)
      bfd_set_error (bfd_error_wrong_format);
    return 0;
  }

  anexec.a_info = bfd_h_get_32 (abfd, exec_bytes.e_info);

  if (N_BADMAG (anexec)) {
    bfd_set_error (bfd_error_wrong_format);
    return 0;
  }

  bout_swap_exec_header_in (abfd, &exec_bytes, &anexec);
  return aout_32_some_aout_object_p (abfd, &anexec, b_out_callback);
}


/* Finish up the opening of a b.out file for reading.  Fill in all the
   fields that are not handled by common code.  */

static const bfd_target *
b_out_callback (abfd)
     bfd *abfd;
{
  struct internal_exec *execp = exec_hdr (abfd);
  unsigned long bss_start;

  /* Architecture and machine type */
  bfd_set_arch_mach(abfd,
		    bfd_arch_i960, /* B.out only used on i960 */
		    bfd_mach_i960_core /* Default */
		    );

  /* The positions of the string table and symbol table.  */
  obj_str_filepos (abfd) = N_STROFF (*execp);
  obj_sym_filepos (abfd) = N_SYMOFF (*execp);

  /* The alignments of the sections */
  obj_textsec (abfd)->alignment_power = execp->a_talign;
  obj_datasec (abfd)->alignment_power = execp->a_dalign;
  obj_bsssec  (abfd)->alignment_power = execp->a_balign;

  /* The starting addresses of the sections.  */
  obj_textsec (abfd)->vma = execp->a_tload;
  obj_datasec (abfd)->vma = execp->a_dload;

  /* And reload the sizes, since the aout module zaps them */
  obj_textsec (abfd)->_raw_size = execp->a_text;

  bss_start = execp->a_dload + execp->a_data; /* BSS = end of data section */
  obj_bsssec (abfd)->vma = align_power (bss_start, execp->a_balign);

  /* The file positions of the sections */
  obj_textsec (abfd)->filepos = N_TXTOFF(*execp);
  obj_datasec (abfd)->filepos = N_DATOFF(*execp);

  /* The file positions of the relocation info */
  obj_textsec (abfd)->rel_filepos = N_TROFF(*execp);
  obj_datasec (abfd)->rel_filepos =  N_DROFF(*execp);

  adata(abfd).page_size = 1;	/* Not applicable. */
  adata(abfd).segment_size = 1; /* Not applicable. */
  adata(abfd).exec_bytes_size = EXEC_BYTES_SIZE;

  if (execp->a_relaxable)
   abfd->flags |= BFD_IS_RELAXABLE;
  return abfd->xvec;
}

struct bout_data_struct {
    struct aoutdata a;
    struct internal_exec e;
};

static boolean
b_out_mkobject (abfd)
     bfd *abfd;
{
  struct bout_data_struct *rawptr;

  rawptr = (struct bout_data_struct *) bfd_zalloc (abfd, sizeof (struct bout_data_struct));
  if (rawptr == NULL)
    return false;

  abfd->tdata.bout_data = rawptr;
  exec_hdr (abfd) = &rawptr->e;

  obj_textsec (abfd) = (asection *)NULL;
  obj_datasec (abfd) = (asection *)NULL;
  obj_bsssec (abfd) = (asection *)NULL;

  return true;
}

static boolean
b_out_write_object_contents (abfd)
     bfd *abfd;
{
  struct external_exec swapped_hdr;

  if (! aout_32_make_sections (abfd))
    return false;

  exec_hdr (abfd)->a_info = BMAGIC;

  exec_hdr (abfd)->a_text = obj_textsec (abfd)->_raw_size;
  exec_hdr (abfd)->a_data = obj_datasec (abfd)->_raw_size;
  exec_hdr (abfd)->a_bss = obj_bsssec (abfd)->_raw_size;
  exec_hdr (abfd)->a_syms = bfd_get_symcount (abfd) * sizeof (struct nlist);
  exec_hdr (abfd)->a_entry = bfd_get_start_address (abfd);
  exec_hdr (abfd)->a_trsize = ((obj_textsec (abfd)->reloc_count) *
                               sizeof (struct relocation_info));
  exec_hdr (abfd)->a_drsize = ((obj_datasec (abfd)->reloc_count) *
                               sizeof (struct relocation_info));

  exec_hdr (abfd)->a_talign = obj_textsec (abfd)->alignment_power;
  exec_hdr (abfd)->a_dalign = obj_datasec (abfd)->alignment_power;
  exec_hdr (abfd)->a_balign = obj_bsssec (abfd)->alignment_power;

  exec_hdr (abfd)->a_tload = obj_textsec (abfd)->vma;
  exec_hdr (abfd)->a_dload = obj_datasec (abfd)->vma;

  bout_swap_exec_header_out (abfd, exec_hdr (abfd), &swapped_hdr);

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0
      || (bfd_write ((PTR) &swapped_hdr, 1, EXEC_BYTES_SIZE, abfd)
	  != EXEC_BYTES_SIZE))
    return false;

  /* Now write out reloc info, followed by syms and strings */
  if (bfd_get_symcount (abfd) != 0)
    {
      if (bfd_seek (abfd, (file_ptr)(N_SYMOFF(*exec_hdr(abfd))), SEEK_SET)
	  != 0)
	return false;

      if (! aout_32_write_syms (abfd))
	return false;

      if (bfd_seek (abfd, (file_ptr)(N_TROFF(*exec_hdr(abfd))), SEEK_SET) != 0)
	return false;

      if (!b_out_squirt_out_relocs (abfd, obj_textsec (abfd))) return false;
      if (bfd_seek (abfd, (file_ptr)(N_DROFF(*exec_hdr(abfd))), SEEK_SET)
	  != 0)
	return false;

      if (!b_out_squirt_out_relocs (abfd, obj_datasec (abfd))) return false;
    }
  return true;
}

/** Some reloc hackery */

#define CALLS	 0x66003800	/* Template for 'calls' instruction	*/
#define BAL	 0x0b000000	/* Template for 'bal' instruction */
#define BALX	 0x85000000	/* Template for 'balx' instruction	*/
#define BAL_MASK 0x00ffffff
#define CALL     0x09000000
#define PCREL13_MASK 0x1fff


#define output_addr(sec) ((sec)->output_offset+(sec)->output_section->vma)

/* Magic to turn callx into calljx */
static bfd_reloc_status_type
calljx_callback (abfd, link_info, reloc_entry, src, dst, input_section)
     bfd *abfd;
     struct bfd_link_info *link_info;
     arelent *reloc_entry;
     PTR src;
     PTR dst;
     asection *input_section;
{
  int word = bfd_get_32 (abfd, src);
  asymbol *symbol_in = *(reloc_entry->sym_ptr_ptr);
  aout_symbol_type *symbol = aout_symbol (symbol_in);
  bfd_vma value;

  value = get_value (reloc_entry, link_info, input_section);

  if (IS_CALLNAME (symbol->other))
    {
      aout_symbol_type *balsym = symbol+1;
      int inst = bfd_get_32 (abfd, (bfd_byte *) src-4);
      /* The next symbol should be an N_BALNAME */
      BFD_ASSERT (IS_BALNAME (balsym->other));
      inst &= BAL_MASK;
      inst |= BALX;
      bfd_put_32 (abfd, inst, (bfd_byte *) dst-4);
      symbol = balsym;
      value = (symbol->symbol.value
	       + output_addr (symbol->symbol.section));
    }

  word += value + reloc_entry->addend;

  bfd_put_32(abfd, word, dst);
  return bfd_reloc_ok;
}


/* Magic to turn call into callj */
static bfd_reloc_status_type
callj_callback (abfd, link_info, reloc_entry,  data, srcidx, dstidx,
		input_section, shrinking)
     bfd *abfd;
     struct bfd_link_info *link_info;
     arelent *reloc_entry;
     PTR data;
     unsigned int srcidx;
     unsigned int dstidx;
     asection *input_section;
     boolean shrinking;
{
  int word = bfd_get_32 (abfd, (bfd_byte *) data + srcidx);
  asymbol *symbol_in = *(reloc_entry->sym_ptr_ptr);
  aout_symbol_type *symbol = aout_symbol (symbol_in);
  bfd_vma value;

  value = get_value (reloc_entry, link_info, input_section);

  if (IS_OTHER(symbol->other))
    {
      /* Call to a system procedure - replace code with system
	 procedure number.  */
      word = CALLS | (symbol->other - 1);
    }
  else if (IS_CALLNAME(symbol->other))
    {
      aout_symbol_type *balsym = symbol+1;

      /* The next symbol should be an N_BALNAME.  */
      BFD_ASSERT(IS_BALNAME(balsym->other));
      
      /* We are calling a leaf, so replace the call instruction with a
	 bal.  */
      word = BAL | ((word
		     + output_addr (balsym->symbol.section)
		     + balsym->symbol.value + reloc_entry->addend
		     - dstidx
		     - output_addr (input_section))
		    & BAL_MASK);
    }
  else if ((symbol->symbol.flags & BSF_SECTION_SYM) != 0)
    {
      /* A callj against a symbol in the same section is a fully
         resolved relative call.  We don't need to do anything here.
         If the symbol is not in the same section, I'm not sure what
         to do; fortunately, this case will probably never arise.  */
      BFD_ASSERT (! shrinking);
      BFD_ASSERT (symbol->symbol.section == input_section);
    }
  else
    {
      word = CALL | (((word & BAL_MASK)
		      + value
		      + reloc_entry->addend
		      - (shrinking ? dstidx : 0)
		      - output_addr (input_section))
		     & BAL_MASK);
    }
  bfd_put_32(abfd, word, (bfd_byte *) data + dstidx);
  return bfd_reloc_ok;
}

/* type rshift size  bitsize  	pcrel	bitpos  absolute overflow check*/

#define ABS32CODE 0
#define ABS32CODE_SHRUNK 1
#define PCREL24 2
#define CALLJ 3
#define ABS32 4
#define PCREL13 5
#define ABS32_MAYBE_RELAXABLE 1
#define ABS32_WAS_RELAXABLE 2

#define ALIGNER 10
#define ALIGNDONE 11
static reloc_howto_type howto_reloc_callj =
HOWTO(CALLJ, 0, 2, 24, true, 0, complain_overflow_signed, 0,"callj", true, 0x00ffffff, 0x00ffffff,false);
static  reloc_howto_type howto_reloc_abs32 =
HOWTO(ABS32, 0, 2, 32, false, 0, complain_overflow_bitfield,0,"abs32", true, 0xffffffff,0xffffffff,false);
static reloc_howto_type howto_reloc_pcrel24 =
HOWTO(PCREL24, 0, 2, 24, true, 0, complain_overflow_signed,0,"pcrel24", true, 0x00ffffff,0x00ffffff,false);

static reloc_howto_type howto_reloc_pcrel13 =
HOWTO(PCREL13, 0, 2, 13, true, 0, complain_overflow_signed,0,"pcrel13", true, 0x00001fff,0x00001fff,false);


static reloc_howto_type howto_reloc_abs32codeshrunk =
HOWTO(ABS32CODE_SHRUNK, 0, 2, 24, true, 0, complain_overflow_signed, 0,"callx->callj", true, 0x00ffffff, 0x00ffffff,false);

static  reloc_howto_type howto_reloc_abs32code =
HOWTO(ABS32CODE, 0, 2, 32, false, 0, complain_overflow_bitfield,0,"callx", true, 0xffffffff,0xffffffff,false);

static reloc_howto_type howto_align_table[] = {
  HOWTO (ALIGNER, 0, 0x1, 0, false, 0, complain_overflow_dont, 0, "align16", false, 0, 0, false),
  HOWTO (ALIGNER, 0, 0x3, 0, false, 0, complain_overflow_dont, 0, "align32", false, 0, 0, false),
  HOWTO (ALIGNER, 0, 0x7, 0, false, 0, complain_overflow_dont, 0, "align64", false, 0, 0, false),
  HOWTO (ALIGNER, 0, 0xf, 0, false, 0, complain_overflow_dont, 0, "align128", false, 0, 0, false),
};

static reloc_howto_type howto_done_align_table[] = {
  HOWTO (ALIGNDONE, 0x1, 0x1, 0, false, 0, complain_overflow_dont, 0, "donealign16", false, 0, 0, false),
  HOWTO (ALIGNDONE, 0x3, 0x3, 0, false, 0, complain_overflow_dont, 0, "donealign32", false, 0, 0, false),
  HOWTO (ALIGNDONE, 0x7, 0x7, 0, false, 0, complain_overflow_dont, 0, "donealign64", false, 0, 0, false),
  HOWTO (ALIGNDONE, 0xf, 0xf, 0, false, 0, complain_overflow_dont, 0, "donealign128", false, 0, 0, false),
};

static reloc_howto_type *
b_out_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    default:
      return 0;
    case BFD_RELOC_I960_CALLJ:
      return &howto_reloc_callj;
    case BFD_RELOC_32:
    case BFD_RELOC_CTOR:
      return &howto_reloc_abs32;
    case BFD_RELOC_24_PCREL:
      return &howto_reloc_pcrel24;
    }
}

/* Allocate enough room for all the reloc entries, plus pointers to them all */

static boolean
b_out_slurp_reloc_table (abfd, asect, symbols)
     bfd *abfd;
     sec_ptr asect;
     asymbol **symbols;
{
  register struct relocation_info *rptr;
  unsigned int counter ;
  arelent *cache_ptr ;
  int extern_mask, pcrel_mask, callj_mask, length_shift;
  int incode_mask;
  int size_mask;
  bfd_vma prev_addr = 0;
  unsigned int count;
  size_t  reloc_size;
  struct relocation_info *relocs;
  arelent *reloc_cache;

  if (asect->relocation)
    return true;
  if (!aout_32_slurp_symbol_table (abfd))
    return false;

  if (asect == obj_datasec (abfd)) {
    reloc_size = exec_hdr(abfd)->a_drsize;
    goto doit;
  }

  if (asect == obj_textsec (abfd)) {
    reloc_size = exec_hdr(abfd)->a_trsize;
    goto doit;
  }

  if (asect == obj_bsssec (abfd)) {
    reloc_size = 0;
    goto doit;
  }

  bfd_set_error (bfd_error_invalid_operation);
  return false;

 doit:
  if (bfd_seek (abfd, (file_ptr)(asect->rel_filepos),  SEEK_SET) != 0)
    return false;
  count = reloc_size / sizeof (struct relocation_info);

  relocs = (struct relocation_info *) bfd_malloc (reloc_size);
  if (!relocs && reloc_size != 0)
    return false;
  reloc_cache = (arelent *) bfd_malloc ((count+1) * sizeof (arelent));
  if (!reloc_cache) {
    if (relocs != NULL)
      free ((char*)relocs);
    return false;
  }

  if (bfd_read ((PTR) relocs, 1, reloc_size, abfd) != reloc_size) {
    free (reloc_cache);
    if (relocs != NULL)
      free (relocs);
    return false;
  }



  if (bfd_header_big_endian (abfd)) {
    /* big-endian bit field allocation order */
    pcrel_mask  = 0x80;
    extern_mask = 0x10;
    incode_mask = 0x08;
    callj_mask  = 0x02;
    size_mask =   0x20;
    length_shift = 5;
  } else {
    /* little-endian bit field allocation order */
    pcrel_mask  = 0x01;
    extern_mask = 0x08;
    incode_mask = 0x10;
    callj_mask  = 0x40;
    size_mask   = 0x02;
    length_shift = 1;
  }

  for (rptr = relocs, cache_ptr = reloc_cache, counter = 0;
       counter < count;
       counter++, rptr++, cache_ptr++)
  {
    unsigned char *raw = (unsigned char *)rptr;
    unsigned int symnum;
    cache_ptr->address = bfd_h_get_32 (abfd, raw + 0);
    cache_ptr->howto = 0;
    if (bfd_header_big_endian (abfd))
    {
      symnum = (raw[4] << 16) | (raw[5] << 8) | raw[6];
    }
    else
    {
      symnum = (raw[6] << 16) | (raw[5] << 8) | raw[4];
    }

    if (raw[7] & extern_mask)
    {
      /* if this is set then the r_index is a index into the symbol table;
       * if the bit is not set then r_index contains a section map.
       * we either fill in the sym entry with a pointer to the symbol,
       * or point to the correct section
       */
      cache_ptr->sym_ptr_ptr = symbols + symnum;
      cache_ptr->addend = 0;
    } else
    {
      /* in a.out symbols are relative to the beginning of the
       * file rather than sections ?
       * (look in translate_from_native_sym_flags)
       * the reloc entry addend has added to it the offset into the
       * file of the data, so subtract the base to make the reloc
       * section relative */
      int s;
      {
	/* sign-extend symnum from 24 bits to whatever host uses */
	s = symnum;
	if (s & (1 << 23))
	  s |= (~0) << 24;
      }
      cache_ptr->sym_ptr_ptr = (asymbol **)NULL;
      switch (s)
      {
       case N_TEXT:
       case N_TEXT | N_EXT:
	cache_ptr->sym_ptr_ptr = obj_textsec(abfd)->symbol_ptr_ptr;
	cache_ptr->addend = - obj_textsec(abfd)->vma;
	break;
       case N_DATA:
       case N_DATA | N_EXT:
	cache_ptr->sym_ptr_ptr = obj_datasec(abfd)->symbol_ptr_ptr;
	cache_ptr->addend = - obj_datasec(abfd)->vma;
	break;
       case N_BSS:
       case N_BSS | N_EXT:
	cache_ptr->sym_ptr_ptr = obj_bsssec(abfd)->symbol_ptr_ptr;
	cache_ptr->addend =  - obj_bsssec(abfd)->vma;
	break;
       case N_ABS:
       case N_ABS | N_EXT:
	cache_ptr->sym_ptr_ptr = obj_bsssec(abfd)->symbol_ptr_ptr;
	cache_ptr->addend = 0;
	break;
      case -2: /* .align */
	if (raw[7] & pcrel_mask)
	  {
	    cache_ptr->howto = &howto_align_table[(raw[7] >> length_shift) & 3];
	    cache_ptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	  }
	else
	  {
	    /* .org? */
	    abort ();
	  }
	cache_ptr->addend = 0;
	break;
       default:
	BFD_ASSERT(0);
	break;
      }

    }

    /* the i960 only has a few relocation types:
       abs 32-bit and pcrel 24bit.   except for callj's!  */
    if (cache_ptr->howto != 0)
      ;
    else if (raw[7] & callj_mask)
    {
      cache_ptr->howto = &howto_reloc_callj;
    }
    else if ( raw[7] & pcrel_mask)
    {
      if (raw[7] & size_mask)
       cache_ptr->howto = &howto_reloc_pcrel13;
      else
       cache_ptr->howto = &howto_reloc_pcrel24;
    }
    else
    {
      if (raw[7] & incode_mask)
      {
	cache_ptr->howto = &howto_reloc_abs32code;
      }
      else
      {
	cache_ptr->howto = &howto_reloc_abs32;
      }
    }
    if (cache_ptr->address < prev_addr)
    {
      /* Ouch! this reloc is out of order, insert into the right place
       */
      arelent tmp;
      arelent *cursor = cache_ptr-1;
      bfd_vma stop = cache_ptr->address;
      tmp  = *cache_ptr;
      while (cursor->address > stop && cursor >= reloc_cache)
      {
	cursor[1] = cursor[0];
	cursor--;
      }
      cursor[1] = tmp;
    }
    else
    {
      prev_addr = cache_ptr->address;
    }
  }


  if (relocs != NULL)
    free (relocs);
  asect->relocation = reloc_cache;
  asect->reloc_count = count;


  return true;
}


static boolean
b_out_squirt_out_relocs (abfd, section)
     bfd *abfd;
     asection *section;
{
  arelent **generic;
  int r_extern = 0;
  int r_idx;
  int incode_mask;
  int len_1;
  unsigned int count = section->reloc_count;
  struct relocation_info *native, *natptr;
  size_t natsize = count * sizeof (struct relocation_info);
  int extern_mask, pcrel_mask,  len_2, callj_mask;
  if (count == 0) return true;
  generic   = section->orelocation;
  native = ((struct relocation_info *) bfd_malloc (natsize));
  if (!native && natsize != 0)
    return false;

  if (bfd_header_big_endian (abfd))
  {
    /* Big-endian bit field allocation order */
    pcrel_mask  = 0x80;
    extern_mask = 0x10;
    len_2       = 0x40;
    len_1       = 0x20;
    callj_mask  = 0x02;
    incode_mask = 0x08;
  }
  else
  {
    /* Little-endian bit field allocation order */
    pcrel_mask  = 0x01;
    extern_mask = 0x08;
    len_2       = 0x04;
    len_1       = 0x02;
    callj_mask  = 0x40;
    incode_mask = 0x10;
  }

  for (natptr = native; count > 0; --count, ++natptr, ++generic)
  {
    arelent *g = *generic;
    unsigned char *raw = (unsigned char *)natptr;
    asymbol *sym = *(g->sym_ptr_ptr);

    asection *output_section = sym->section->output_section;

    bfd_h_put_32(abfd, g->address, raw);
    /* Find a type in the output format which matches the input howto -
     * at the moment we assume input format == output format FIXME!!
     */
    r_idx = 0;
    /* FIXME:  Need callj stuff here, and to check the howto entries to
       be sure they are real for this architecture.  */
    if (g->howto== &howto_reloc_callj)
    {
      raw[7] = callj_mask + pcrel_mask + len_2;
    }
    else if (g->howto == &howto_reloc_pcrel24)
    {
      raw[7] = pcrel_mask + len_2;
    }
    else if (g->howto == &howto_reloc_pcrel13)
    {
      raw[7] = pcrel_mask + len_1;
    }
    else if (g->howto == &howto_reloc_abs32code)
    {
      raw[7] = len_2 + incode_mask;
    }
    else if (g->howto >= howto_align_table
	     && g->howto <= (howto_align_table
			    + sizeof (howto_align_table) / sizeof (howto_align_table[0])
			    - 1))
      {
	/* symnum == -2; extern_mask not set, pcrel_mask set */
	r_idx = -2;
	r_extern = 0;
	raw[7] = (pcrel_mask
		  | ((g->howto - howto_align_table) << 1));
      }
    else {
      raw[7] = len_2;
    }

    if (r_idx != 0)
      /* already mucked with r_extern, r_idx */;
    else if (bfd_is_com_section (output_section)
	     || bfd_is_abs_section (output_section)
	     || bfd_is_und_section (output_section))
    {

      if (bfd_abs_section_ptr->symbol == sym)
      {
	/* Whoops, looked like an abs symbol, but is really an offset
	   from the abs section */
	r_idx = 0;
	r_extern = 0;
       }
      else
      {
	/* Fill in symbol */

	r_extern = 1;
	r_idx = (*g->sym_ptr_ptr)->udata.i;
      }
    }
    else
    {
      /* Just an ordinary section */
      r_extern = 0;
      r_idx  = output_section->target_index;
    }

    if (bfd_header_big_endian (abfd)) {
      raw[4] = (unsigned char) (r_idx >> 16);
      raw[5] = (unsigned char) (r_idx >>  8);
      raw[6] = (unsigned char) (r_idx     );
    } else {
      raw[6] = (unsigned char) (r_idx >> 16);
      raw[5] = (unsigned char) (r_idx>>  8);
      raw[4] = (unsigned char) (r_idx     );
    }
    if (r_extern)
     raw[7] |= extern_mask;
  }

  if (bfd_write ((PTR) native, 1, natsize, abfd) != natsize) {
    free((PTR)native);
    return false;
  }
  free ((PTR)native);

  return true;
}

/* This is stupid.  This function should be a boolean predicate */
static long
b_out_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd *abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
  arelent *tblptr;
  unsigned int count;

  if ((section->flags & SEC_CONSTRUCTOR) != 0)
    {
      arelent_chain *chain = section->constructor_chain;
      for (count = 0; count < section->reloc_count; count++)
	{
	  *relptr++ = &chain->relent;
	  chain = chain->next;
	}
    }
  else
    {
      if (section->relocation == NULL
	  && ! b_out_slurp_reloc_table (abfd, section, symbols))
	return -1;

      tblptr = section->relocation;
      for (count = 0; count++ < section->reloc_count;)
	*relptr++ = tblptr++;
    }

  *relptr = NULL;

  return section->reloc_count;
}

static long
b_out_get_reloc_upper_bound (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  if (bfd_get_format (abfd) != bfd_object) {
    bfd_set_error (bfd_error_invalid_operation);
    return -1;
  }

  if (asect->flags & SEC_CONSTRUCTOR)
    return sizeof (arelent *) * (asect->reloc_count + 1);

  if (asect == obj_datasec (abfd))
    return (sizeof (arelent *) *
	    ((exec_hdr(abfd)->a_drsize / sizeof (struct relocation_info))
	     +1));

  if (asect == obj_textsec (abfd))
    return (sizeof (arelent *) *
	    ((exec_hdr(abfd)->a_trsize / sizeof (struct relocation_info))
	     +1));

  if (asect == obj_bsssec (abfd))
    return 0;

  bfd_set_error (bfd_error_invalid_operation);
  return -1;
}

static boolean
b_out_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{

  if (abfd->output_has_begun == false) { /* set by bfd.c handler */
    if (! aout_32_make_sections (abfd))
      return false;

    obj_textsec (abfd)->filepos = sizeof(struct internal_exec);
    obj_datasec(abfd)->filepos = obj_textsec(abfd)->filepos
                                 +  obj_textsec (abfd)->_raw_size;

  }
  /* regardless, once we know what we're doing, we might as well get going */
  if (bfd_seek (abfd, section->filepos + offset, SEEK_SET) != 0)
    return false;

  if (count != 0) {
    return (bfd_write ((PTR)location, 1, count, abfd) == count) ?true:false;
  }
  return true;
}

static boolean
b_out_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  bfd_default_set_arch_mach(abfd, arch, machine);

  if (arch == bfd_arch_unknown)	/* Unknown machine arch is OK */
    return true;
  if (arch == bfd_arch_i960)	/* i960 default is OK */
    switch (machine) {
    case bfd_mach_i960_core:
    case bfd_mach_i960_kb_sb:
    case bfd_mach_i960_mc:
    case bfd_mach_i960_xa:
    case bfd_mach_i960_ca:
    case bfd_mach_i960_ka_sa:
    case bfd_mach_i960_jx:
    case bfd_mach_i960_hx:
    case 0:
      return true;
    default:
      return false;
    }

  return false;
}

static int
b_out_sizeof_headers (ignore_abfd, ignore)
     bfd *ignore_abfd;
     boolean ignore;
{
  return sizeof(struct internal_exec);
}



/************************************************************************/
static bfd_vma
get_value (reloc, link_info, input_section)
     arelent *reloc;
     struct bfd_link_info *link_info;
     asection *input_section;
{
  bfd_vma value;
  asymbol *symbol = *(reloc->sym_ptr_ptr);

  /* A symbol holds a pointer to a section, and an offset from the
     base of the section.  To relocate, we find where the section will
     live in the output and add that in */

  if (bfd_is_und_section (symbol->section))
    {
      struct bfd_link_hash_entry *h;

      /* The symbol is undefined in this BFD.  Look it up in the
	 global linker hash table.  FIXME: This should be changed when
	 we convert b.out to use a specific final_link function and
	 change the interface to bfd_relax_section to not require the
	 generic symbols.  */
      h = bfd_wrapped_link_hash_lookup (input_section->owner, link_info,
					bfd_asymbol_name (symbol),
					false, false, true);
      if (h != (struct bfd_link_hash_entry *) NULL
	  && (h->type == bfd_link_hash_defined
	      || h->type == bfd_link_hash_defweak))
	value = h->u.def.value + output_addr (h->u.def.section);
      else if (h != (struct bfd_link_hash_entry *) NULL
	       && h->type == bfd_link_hash_common)
	value = h->u.c.size;
      else
	{
	  if (! ((*link_info->callbacks->undefined_symbol)
		 (link_info, bfd_asymbol_name (symbol),
		  input_section->owner, input_section, reloc->address)))
	    abort ();
	  value = 0;
	}
    }
  else
    {
      value = symbol->value + output_addr (symbol->section);
    }

  /* Add the value contained in the relocation */
  value += reloc->addend;

  return value;
}

static void
perform_slip (abfd, slip, input_section, value)
     bfd *abfd;
     unsigned int slip;
     asection *input_section;
     bfd_vma value;
{
  asymbol **s;

  s = _bfd_generic_link_get_symbols (abfd);
  BFD_ASSERT (s != (asymbol **) NULL);

  /* Find all symbols past this point, and make them know
     what's happened */
  while (*s)
  {
    asymbol *p = *s;
    if (p->section == input_section)
    {
      /* This was pointing into this section, so mangle it */
      if (p->value > value)
      {
	p->value -=slip;
	if (p->udata.p != NULL)
	  {
	    struct generic_link_hash_entry *h;

	    h = (struct generic_link_hash_entry *) p->udata.p;
	    BFD_ASSERT (h->root.type == bfd_link_hash_defined);
	    h->root.u.def.value -= slip;
	    BFD_ASSERT (h->root.u.def.value == p->value);
	  }
      }
    }
    s++;

  }
}

/* This routine works out if the thing we want to get to can be
   reached with a 24bit offset instead of a 32 bit one.
   If it can, then it changes the amode */

static int
abs32code (abfd, input_section, r, shrink, link_info)
     bfd *abfd;
     asection *input_section;
     arelent *r;
     unsigned int shrink;
     struct bfd_link_info *link_info;
{
  bfd_vma value = get_value (r, link_info, input_section);
  bfd_vma dot = output_addr (input_section) + r->address;
  bfd_vma gap;

  /* See if the address we're looking at within 2^23 bytes of where
     we are, if so then we can use a small branch rather than the
     jump we were going to */

  gap = value - (dot - shrink);


  if (-1<<23 < (long)gap && (long)gap < 1<<23 )
  {
    /* Change the reloc type from 32bitcode possible 24, to 24bit
       possible 32 */

    r->howto = &howto_reloc_abs32codeshrunk;
    /* The place to relc moves back by four bytes */
    r->address -=4;

    /* This will be four bytes smaller in the long run */
    shrink += 4 ;
    perform_slip (abfd, 4, input_section, r->address-shrink + 4);
  }
  return shrink;
}

static int
aligncode (abfd, input_section, r, shrink)
     bfd *abfd;
     asection *input_section;
     arelent *r;
     unsigned int shrink;
{
  bfd_vma dot = output_addr (input_section) + r->address;
  bfd_vma gap;
  bfd_vma old_end;
  bfd_vma new_end;
  int shrink_delta;
  int size = r->howto->size;

  /* Reduce the size of the alignment so that it's still aligned but
     smaller  - the current size is already the same size as or bigger
     than the alignment required.  */

  /* calculate the first byte following the padding before we optimize */
  old_end = ((dot + size ) & ~size) + size+1;
  /* work out where the new end will be - remember that we're smaller
     than we used to be */
  new_end = ((dot - shrink + size) & ~size);

  /* This is the new end */
  gap = old_end - ((dot + size) & ~size);

  shrink_delta = (old_end - new_end) - shrink;

  if (shrink_delta)
  {
    /* Change the reloc so that it knows how far to align to */
    r->howto = howto_done_align_table + (r->howto - howto_align_table);

    /* Encode the stuff into the addend - for future use we need to
       know how big the reloc used to be */
    r->addend = old_end - dot + r->address;

    /* This will be N bytes smaller in the long run, adjust all the symbols */
    perform_slip (abfd, shrink_delta, input_section, r->address - shrink);
    shrink += shrink_delta;
  }
  return shrink;
}

static boolean
b_out_bfd_relax_section (abfd, i, link_info, again)
     bfd *abfd;
     asection *i;
     struct bfd_link_info *link_info;
     boolean *again;
{
  /* Get enough memory to hold the stuff */
  bfd *input_bfd = i->owner;
  asection *input_section = i;
  int shrink = 0 ;
  arelent **reloc_vector = NULL;
  long reloc_size = bfd_get_reloc_upper_bound(input_bfd,
					      input_section);

  if (reloc_size < 0)
    return false;

  /* We only run this relaxation once.  It might work to run it
     multiple times, but it hasn't been tested.  */
  *again = false;

  if (reloc_size)
    {
      long reloc_count;

      reloc_vector = (arelent **) bfd_malloc (reloc_size);
      if (reloc_vector == NULL && reloc_size != 0)
	goto error_return;

      /* Get the relocs and think about them */
      reloc_count =
	bfd_canonicalize_reloc (input_bfd, input_section, reloc_vector,
				_bfd_generic_link_get_symbols (input_bfd));
      if (reloc_count < 0)
	goto error_return;
      if (reloc_count > 0)
	{
	  arelent **parent;
	  for (parent = reloc_vector; *parent; parent++)
	    {
	      arelent *r = *parent;
	      switch (r->howto->type)
		{
		case ALIGNER:
		  /* An alignment reloc */
		  shrink = aligncode (abfd, input_section, r, shrink);
		  break;
		case ABS32CODE:
		  /* A 32bit reloc in an addressing mode */
		  shrink = abs32code (input_bfd, input_section, r, shrink,
				      link_info);
		  break;
		case ABS32CODE_SHRUNK:
		  shrink+=4;
		  break;
		}
	    }
	}
    }
  input_section->_cooked_size = input_section->_raw_size - shrink;

  if (reloc_vector != NULL)
    free (reloc_vector);
  return true;
 error_return:
  if (reloc_vector != NULL)
    free (reloc_vector);
  return false;
}

static bfd_byte *
b_out_bfd_get_relocated_section_contents (in_abfd, link_info, link_order,
					  data, relocateable, symbols)
     bfd *in_abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  /* Get enough memory to hold the stuff */
  bfd *input_bfd = link_order->u.indirect.section->owner;
  asection *input_section = link_order->u.indirect.section;
  long reloc_size = bfd_get_reloc_upper_bound(input_bfd,
					      input_section);
  arelent **reloc_vector = NULL;
  long reloc_count;

  if (reloc_size < 0)
    goto error_return;

  /* If producing relocateable output, don't bother to relax.  */
  if (relocateable)
    return bfd_generic_get_relocated_section_contents (in_abfd, link_info,
						       link_order,
						       data, relocateable,
						       symbols);

  reloc_vector = (arelent **) bfd_malloc (reloc_size);
  if (reloc_vector == NULL && reloc_size != 0)
    goto error_return;

  input_section->reloc_done = 1;

  /* read in the section */
  BFD_ASSERT (true == bfd_get_section_contents(input_bfd,
					       input_section,
					       data,
					       0,
					       input_section->_raw_size));

  reloc_count = bfd_canonicalize_reloc (input_bfd,
					input_section,
					reloc_vector,
					symbols);
  if (reloc_count < 0)
    goto error_return;
  if (reloc_count > 0)
    {
      arelent **parent = reloc_vector;
      arelent *reloc ;

      unsigned int dst_address = 0;
      unsigned int src_address = 0;
      unsigned int run;
      unsigned int idx;

      /* Find how long a run we can do */
      while (dst_address < link_order->size)
	{
	  reloc = *parent;
	  if (reloc)
	    {
	      /* Note that the relaxing didn't tie up the addresses in the
		 relocation, so we use the original address to work out the
		 run of non-relocated data */
	      BFD_ASSERT (reloc->address >= src_address);
	      run = reloc->address - src_address;
	      parent++;
	    }
	  else
	    {
	      run = link_order->size - dst_address;
	    }
	  /* Copy the bytes */
	  for (idx = 0; idx < run; idx++)
	    {
	      data[dst_address++] = data[src_address++];
	    }

	  /* Now do the relocation */

	  if (reloc)
	    {
	      switch (reloc->howto->type)
		{
		case ABS32CODE:
		  calljx_callback (in_abfd, link_info, reloc,
				   src_address + data, dst_address + data,
				   input_section);
		  src_address+=4;
		  dst_address+=4;
		  break;
		case ABS32:
		  bfd_put_32(in_abfd,
			     (bfd_get_32 (in_abfd, data+src_address)
			      + get_value (reloc, link_info, input_section)),
			     data+dst_address);
		  src_address+=4;
		  dst_address+=4;
		  break;
		case CALLJ:
		  callj_callback (in_abfd, link_info, reloc, data, src_address,
				  dst_address, input_section, false);
		  src_address+=4;
		  dst_address+=4;
		  break;
		case ALIGNDONE:
		  BFD_ASSERT (reloc->addend >= src_address);
		  BFD_ASSERT (reloc->addend <= input_section->_raw_size);
		  src_address = reloc->addend;
		  dst_address = ((dst_address + reloc->howto->size)
				 & ~reloc->howto->size);
		  break;
		case ABS32CODE_SHRUNK:
		  /* This used to be a callx, but we've found out that a
		     callj will reach, so do the right thing.  */
		  callj_callback (in_abfd, link_info, reloc, data,
				  src_address + 4, dst_address, input_section,
				  true);
		  dst_address+=4;
		  src_address+=8;
		  break;
		case PCREL24:
		  {
		    long int word = bfd_get_32(in_abfd, data+src_address);
		    bfd_vma value;

		    value = get_value (reloc, link_info, input_section);
		    word = ((word & ~BAL_MASK)
			    | (((word & BAL_MASK)
				+ value
				- output_addr (input_section)
				+ reloc->addend)
			       & BAL_MASK));

		    bfd_put_32(in_abfd,word,  data+dst_address);
		    dst_address+=4;
		    src_address+=4;

		  }
		  break;

		case PCREL13:
		  {
		    long int word = bfd_get_32(in_abfd, data+src_address);
		    bfd_vma value;

		    value = get_value (reloc, link_info, input_section);
		    word = ((word & ~PCREL13_MASK)
			    | (((word & PCREL13_MASK)
				+ value
				+ reloc->addend
				- output_addr (input_section))
			       & PCREL13_MASK));

		    bfd_put_32(in_abfd,word,  data+dst_address);
		    dst_address+=4;
		    src_address+=4;

		  }
		  break;

		default:
		  abort();
		}
	    }
	}
    }
  if (reloc_vector != NULL)
    free (reloc_vector);
  return data;
 error_return:
  if (reloc_vector != NULL)
    free (reloc_vector);
  return NULL;
}
/***********************************************************************/

/* Build the transfer vectors for Big and Little-Endian B.OUT files.  */

#define aout_32_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define aout_32_close_and_cleanup aout_32_bfd_free_cached_info

#define b_out_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define b_out_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define b_out_bfd_final_link _bfd_generic_final_link
#define b_out_bfd_link_split_section  _bfd_generic_link_split_section

#define aout_32_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

const bfd_target b_out_vec_big_host =
{
  "b.out.big",			/* name */
  bfd_target_aout_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_BIG,		/* hdr byte order is big */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | BFD_IS_RELAXABLE ),
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  '_',				/* symbol leading char */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */

  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */
 {_bfd_dummy_target, b_out_object_p, /* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
 {bfd_false, b_out_mkobject,	/* bfd_set_format */
   _bfd_generic_mkarchive, bfd_false},
 {bfd_false, b_out_write_object_contents, /* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (aout_32),
     BFD_JUMP_TABLE_COPY (_bfd_generic),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_bsd),
     BFD_JUMP_TABLE_SYMBOLS (aout_32),
     BFD_JUMP_TABLE_RELOCS (b_out),
     BFD_JUMP_TABLE_WRITE (b_out),
     BFD_JUMP_TABLE_LINK (b_out),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  (PTR) 0,
};


const bfd_target b_out_vec_little_host =
{
  "b.out.little",		/* name */
  bfd_target_aout_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | BFD_IS_RELAXABLE ),
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  '_',				/* symbol leading char */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

  {_bfd_dummy_target, b_out_object_p, /* bfd_check_format */
     bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, b_out_mkobject,	/* bfd_set_format */
     _bfd_generic_mkarchive, bfd_false},
  {bfd_false, b_out_write_object_contents, /* bfd_write_contents */
     _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (aout_32),
     BFD_JUMP_TABLE_COPY (_bfd_generic),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_bsd),
     BFD_JUMP_TABLE_SYMBOLS (aout_32),
     BFD_JUMP_TABLE_RELOCS (b_out),
     BFD_JUMP_TABLE_WRITE (b_out),
     BFD_JUMP_TABLE_LINK (b_out),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  (PTR) 0
};
