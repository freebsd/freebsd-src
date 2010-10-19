/* Instruction building/extraction support for m32c. -*- C -*-

   THIS FILE IS MACHINE GENERATED WITH CGEN: Cpu tools GENerator.
   - the resultant file is machine generated, cgen-ibld.in isn't

   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2005, 2006
   Free Software Foundation, Inc.

   This file is part of the GNU Binutils and GDB, the GNU debugger.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/* ??? Eventually more and more of this stuff can go to cpu-independent files.
   Keep that in mind.  */

#include "sysdep.h"
#include <stdio.h>
#include "ansidecl.h"
#include "dis-asm.h"
#include "bfd.h"
#include "symcat.h"
#include "m32c-desc.h"
#include "m32c-opc.h"
#include "opintl.h"
#include "safe-ctype.h"

#undef  min
#define min(a,b) ((a) < (b) ? (a) : (b))
#undef  max
#define max(a,b) ((a) > (b) ? (a) : (b))

/* Used by the ifield rtx function.  */
#define FLD(f) (fields->f)

static const char * insert_normal
  (CGEN_CPU_DESC, long, unsigned int, unsigned int, unsigned int,
   unsigned int, unsigned int, unsigned int, CGEN_INSN_BYTES_PTR);
static const char * insert_insn_normal
  (CGEN_CPU_DESC, const CGEN_INSN *,
   CGEN_FIELDS *, CGEN_INSN_BYTES_PTR, bfd_vma);
static int extract_normal
  (CGEN_CPU_DESC, CGEN_EXTRACT_INFO *, CGEN_INSN_INT,
   unsigned int, unsigned int, unsigned int, unsigned int,
   unsigned int, unsigned int, bfd_vma, long *);
static int extract_insn_normal
  (CGEN_CPU_DESC, const CGEN_INSN *, CGEN_EXTRACT_INFO *,
   CGEN_INSN_INT, CGEN_FIELDS *, bfd_vma);
#if CGEN_INT_INSN_P
static void put_insn_int_value
  (CGEN_CPU_DESC, CGEN_INSN_BYTES_PTR, int, int, CGEN_INSN_INT);
#endif
#if ! CGEN_INT_INSN_P
static CGEN_INLINE void insert_1
  (CGEN_CPU_DESC, unsigned long, int, int, int, unsigned char *);
static CGEN_INLINE int fill_cache
  (CGEN_CPU_DESC, CGEN_EXTRACT_INFO *,  int, int, bfd_vma);
static CGEN_INLINE long extract_1
  (CGEN_CPU_DESC, CGEN_EXTRACT_INFO *, int, int, int, unsigned char *, bfd_vma);
#endif

/* Operand insertion.  */

#if ! CGEN_INT_INSN_P

/* Subroutine of insert_normal.  */

static CGEN_INLINE void
insert_1 (CGEN_CPU_DESC cd,
	  unsigned long value,
	  int start,
	  int length,
	  int word_length,
	  unsigned char *bufp)
{
  unsigned long x,mask;
  int shift;

  x = cgen_get_insn_value (cd, bufp, word_length);

  /* Written this way to avoid undefined behaviour.  */
  mask = (((1L << (length - 1)) - 1) << 1) | 1;
  if (CGEN_INSN_LSB0_P)
    shift = (start + 1) - length;
  else
    shift = (word_length - (start + length));
  x = (x & ~(mask << shift)) | ((value & mask) << shift);

  cgen_put_insn_value (cd, bufp, word_length, (bfd_vma) x);
}

#endif /* ! CGEN_INT_INSN_P */

/* Default insertion routine.

   ATTRS is a mask of the boolean attributes.
   WORD_OFFSET is the offset in bits from the start of the insn of the value.
   WORD_LENGTH is the length of the word in bits in which the value resides.
   START is the starting bit number in the word, architecture origin.
   LENGTH is the length of VALUE in bits.
   TOTAL_LENGTH is the total length of the insn in bits.

   The result is an error message or NULL if success.  */

/* ??? This duplicates functionality with bfd's howto table and
   bfd_install_relocation.  */
/* ??? This doesn't handle bfd_vma's.  Create another function when
   necessary.  */

static const char *
insert_normal (CGEN_CPU_DESC cd,
	       long value,
	       unsigned int attrs,
	       unsigned int word_offset,
	       unsigned int start,
	       unsigned int length,
	       unsigned int word_length,
	       unsigned int total_length,
	       CGEN_INSN_BYTES_PTR buffer)
{
  static char errbuf[100];
  /* Written this way to avoid undefined behaviour.  */
  unsigned long mask = (((1L << (length - 1)) - 1) << 1) | 1;

  /* If LENGTH is zero, this operand doesn't contribute to the value.  */
  if (length == 0)
    return NULL;

  if (word_length > 32)
    abort ();

  /* For architectures with insns smaller than the base-insn-bitsize,
     word_length may be too big.  */
  if (cd->min_insn_bitsize < cd->base_insn_bitsize)
    {
      if (word_offset == 0
	  && word_length > total_length)
	word_length = total_length;
    }

  /* Ensure VALUE will fit.  */
  if (CGEN_BOOL_ATTR (attrs, CGEN_IFLD_SIGN_OPT))
    {
      long minval = - (1L << (length - 1));
      unsigned long maxval = mask;
      
      if ((value > 0 && (unsigned long) value > maxval)
	  || value < minval)
	{
	  /* xgettext:c-format */
	  sprintf (errbuf,
		   _("operand out of range (%ld not between %ld and %lu)"),
		   value, minval, maxval);
	  return errbuf;
	}
    }
  else if (! CGEN_BOOL_ATTR (attrs, CGEN_IFLD_SIGNED))
    {
      unsigned long maxval = mask;
      unsigned long val = (unsigned long) value;

      /* For hosts with a word size > 32 check to see if value has been sign
	 extended beyond 32 bits.  If so then ignore these higher sign bits
	 as the user is attempting to store a 32-bit signed value into an
	 unsigned 32-bit field which is allowed.  */
      if (sizeof (unsigned long) > 4 && ((value >> 32) == -1))
	val &= 0xFFFFFFFF;

      if (val > maxval)
	{
	  /* xgettext:c-format */
	  sprintf (errbuf,
		   _("operand out of range (0x%lx not between 0 and 0x%lx)"),
		   val, maxval);
	  return errbuf;
	}
    }
  else
    {
      if (! cgen_signed_overflow_ok_p (cd))
	{
	  long minval = - (1L << (length - 1));
	  long maxval =   (1L << (length - 1)) - 1;
	  
	  if (value < minval || value > maxval)
	    {
	      sprintf
		/* xgettext:c-format */
		(errbuf, _("operand out of range (%ld not between %ld and %ld)"),
		 value, minval, maxval);
	      return errbuf;
	    }
	}
    }

#if CGEN_INT_INSN_P

  {
    int shift;

    if (CGEN_INSN_LSB0_P)
      shift = (word_offset + start + 1) - length;
    else
      shift = total_length - (word_offset + start + length);
    *buffer = (*buffer & ~(mask << shift)) | ((value & mask) << shift);
  }

#else /* ! CGEN_INT_INSN_P */

  {
    unsigned char *bufp = (unsigned char *) buffer + word_offset / 8;

    insert_1 (cd, value, start, length, word_length, bufp);
  }

#endif /* ! CGEN_INT_INSN_P */

  return NULL;
}

/* Default insn builder (insert handler).
   The instruction is recorded in CGEN_INT_INSN_P byte order (meaning
   that if CGEN_INSN_BYTES_PTR is an int * and thus, the value is
   recorded in host byte order, otherwise BUFFER is an array of bytes
   and the value is recorded in target byte order).
   The result is an error message or NULL if success.  */

static const char *
insert_insn_normal (CGEN_CPU_DESC cd,
		    const CGEN_INSN * insn,
		    CGEN_FIELDS * fields,
		    CGEN_INSN_BYTES_PTR buffer,
		    bfd_vma pc)
{
  const CGEN_SYNTAX *syntax = CGEN_INSN_SYNTAX (insn);
  unsigned long value;
  const CGEN_SYNTAX_CHAR_TYPE * syn;

  CGEN_INIT_INSERT (cd);
  value = CGEN_INSN_BASE_VALUE (insn);

  /* If we're recording insns as numbers (rather than a string of bytes),
     target byte order handling is deferred until later.  */

#if CGEN_INT_INSN_P

  put_insn_int_value (cd, buffer, cd->base_insn_bitsize,
		      CGEN_FIELDS_BITSIZE (fields), value);

#else

  cgen_put_insn_value (cd, buffer, min ((unsigned) cd->base_insn_bitsize,
					(unsigned) CGEN_FIELDS_BITSIZE (fields)),
		       value);

#endif /* ! CGEN_INT_INSN_P */

  /* ??? It would be better to scan the format's fields.
     Still need to be able to insert a value based on the operand though;
     e.g. storing a branch displacement that got resolved later.
     Needs more thought first.  */

  for (syn = CGEN_SYNTAX_STRING (syntax); * syn; ++ syn)
    {
      const char *errmsg;

      if (CGEN_SYNTAX_CHAR_P (* syn))
	continue;

      errmsg = (* cd->insert_operand) (cd, CGEN_SYNTAX_FIELD (*syn),
				       fields, buffer, pc);
      if (errmsg)
	return errmsg;
    }

  return NULL;
}

#if CGEN_INT_INSN_P
/* Cover function to store an insn value into an integral insn.  Must go here
   because it needs <prefix>-desc.h for CGEN_INT_INSN_P.  */

static void
put_insn_int_value (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
		    CGEN_INSN_BYTES_PTR buf,
		    int length,
		    int insn_length,
		    CGEN_INSN_INT value)
{
  /* For architectures with insns smaller than the base-insn-bitsize,
     length may be too big.  */
  if (length > insn_length)
    *buf = value;
  else
    {
      int shift = insn_length - length;
      /* Written this way to avoid undefined behaviour.  */
      CGEN_INSN_INT mask = (((1L << (length - 1)) - 1) << 1) | 1;

      *buf = (*buf & ~(mask << shift)) | ((value & mask) << shift);
    }
}
#endif

/* Operand extraction.  */

#if ! CGEN_INT_INSN_P

/* Subroutine of extract_normal.
   Ensure sufficient bytes are cached in EX_INFO.
   OFFSET is the offset in bytes from the start of the insn of the value.
   BYTES is the length of the needed value.
   Returns 1 for success, 0 for failure.  */

static CGEN_INLINE int
fill_cache (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	    CGEN_EXTRACT_INFO *ex_info,
	    int offset,
	    int bytes,
	    bfd_vma pc)
{
  /* It's doubtful that the middle part has already been fetched so
     we don't optimize that case.  kiss.  */
  unsigned int mask;
  disassemble_info *info = (disassemble_info *) ex_info->dis_info;

  /* First do a quick check.  */
  mask = (1 << bytes) - 1;
  if (((ex_info->valid >> offset) & mask) == mask)
    return 1;

  /* Search for the first byte we need to read.  */
  for (mask = 1 << offset; bytes > 0; --bytes, ++offset, mask <<= 1)
    if (! (mask & ex_info->valid))
      break;

  if (bytes)
    {
      int status;

      pc += offset;
      status = (*info->read_memory_func)
	(pc, ex_info->insn_bytes + offset, bytes, info);

      if (status != 0)
	{
	  (*info->memory_error_func) (status, pc, info);
	  return 0;
	}

      ex_info->valid |= ((1 << bytes) - 1) << offset;
    }

  return 1;
}

/* Subroutine of extract_normal.  */

static CGEN_INLINE long
extract_1 (CGEN_CPU_DESC cd,
	   CGEN_EXTRACT_INFO *ex_info ATTRIBUTE_UNUSED,
	   int start,
	   int length,
	   int word_length,
	   unsigned char *bufp,
	   bfd_vma pc ATTRIBUTE_UNUSED)
{
  unsigned long x;
  int shift;

  x = cgen_get_insn_value (cd, bufp, word_length);

  if (CGEN_INSN_LSB0_P)
    shift = (start + 1) - length;
  else
    shift = (word_length - (start + length));
  return x >> shift;
}

#endif /* ! CGEN_INT_INSN_P */

/* Default extraction routine.

   INSN_VALUE is the first base_insn_bitsize bits of the insn in host order,
   or sometimes less for cases like the m32r where the base insn size is 32
   but some insns are 16 bits.
   ATTRS is a mask of the boolean attributes.  We only need `SIGNED',
   but for generality we take a bitmask of all of them.
   WORD_OFFSET is the offset in bits from the start of the insn of the value.
   WORD_LENGTH is the length of the word in bits in which the value resides.
   START is the starting bit number in the word, architecture origin.
   LENGTH is the length of VALUE in bits.
   TOTAL_LENGTH is the total length of the insn in bits.

   Returns 1 for success, 0 for failure.  */

/* ??? The return code isn't properly used.  wip.  */

/* ??? This doesn't handle bfd_vma's.  Create another function when
   necessary.  */

static int
extract_normal (CGEN_CPU_DESC cd,
#if ! CGEN_INT_INSN_P
		CGEN_EXTRACT_INFO *ex_info,
#else
		CGEN_EXTRACT_INFO *ex_info ATTRIBUTE_UNUSED,
#endif
		CGEN_INSN_INT insn_value,
		unsigned int attrs,
		unsigned int word_offset,
		unsigned int start,
		unsigned int length,
		unsigned int word_length,
		unsigned int total_length,
#if ! CGEN_INT_INSN_P
		bfd_vma pc,
#else
		bfd_vma pc ATTRIBUTE_UNUSED,
#endif
		long *valuep)
{
  long value, mask;

  /* If LENGTH is zero, this operand doesn't contribute to the value
     so give it a standard value of zero.  */
  if (length == 0)
    {
      *valuep = 0;
      return 1;
    }

  if (word_length > 32)
    abort ();

  /* For architectures with insns smaller than the insn-base-bitsize,
     word_length may be too big.  */
  if (cd->min_insn_bitsize < cd->base_insn_bitsize)
    {
      if (word_offset + word_length > total_length)
	word_length = total_length - word_offset;
    }

  /* Does the value reside in INSN_VALUE, and at the right alignment?  */

  if (CGEN_INT_INSN_P || (word_offset == 0 && word_length == total_length))
    {
      if (CGEN_INSN_LSB0_P)
	value = insn_value >> ((word_offset + start + 1) - length);
      else
	value = insn_value >> (total_length - ( word_offset + start + length));
    }

#if ! CGEN_INT_INSN_P

  else
    {
      unsigned char *bufp = ex_info->insn_bytes + word_offset / 8;

      if (word_length > 32)
	abort ();

      if (fill_cache (cd, ex_info, word_offset / 8, word_length / 8, pc) == 0)
	return 0;

      value = extract_1 (cd, ex_info, start, length, word_length, bufp, pc);
    }

#endif /* ! CGEN_INT_INSN_P */

  /* Written this way to avoid undefined behaviour.  */
  mask = (((1L << (length - 1)) - 1) << 1) | 1;

  value &= mask;
  /* sign extend? */
  if (CGEN_BOOL_ATTR (attrs, CGEN_IFLD_SIGNED)
      && (value & (1L << (length - 1))))
    value |= ~mask;

  *valuep = value;

  return 1;
}

/* Default insn extractor.

   INSN_VALUE is the first base_insn_bitsize bits, translated to host order.
   The extracted fields are stored in FIELDS.
   EX_INFO is used to handle reading variable length insns.
   Return the length of the insn in bits, or 0 if no match,
   or -1 if an error occurs fetching data (memory_error_func will have
   been called).  */

static int
extract_insn_normal (CGEN_CPU_DESC cd,
		     const CGEN_INSN *insn,
		     CGEN_EXTRACT_INFO *ex_info,
		     CGEN_INSN_INT insn_value,
		     CGEN_FIELDS *fields,
		     bfd_vma pc)
{
  const CGEN_SYNTAX *syntax = CGEN_INSN_SYNTAX (insn);
  const CGEN_SYNTAX_CHAR_TYPE *syn;

  CGEN_FIELDS_BITSIZE (fields) = CGEN_INSN_BITSIZE (insn);

  CGEN_INIT_EXTRACT (cd);

  for (syn = CGEN_SYNTAX_STRING (syntax); *syn; ++syn)
    {
      int length;

      if (CGEN_SYNTAX_CHAR_P (*syn))
	continue;

      length = (* cd->extract_operand) (cd, CGEN_SYNTAX_FIELD (*syn),
					ex_info, insn_value, fields, pc);
      if (length <= 0)
	return length;
    }

  /* We recognized and successfully extracted this insn.  */
  return CGEN_INSN_BITSIZE (insn);
}

/* Machine generated code added here.  */

const char * m32c_cgen_insert_operand
  (CGEN_CPU_DESC, int, CGEN_FIELDS *, CGEN_INSN_BYTES_PTR, bfd_vma);

/* Main entry point for operand insertion.

   This function is basically just a big switch statement.  Earlier versions
   used tables to look up the function to use, but
   - if the table contains both assembler and disassembler functions then
     the disassembler contains much of the assembler and vice-versa,
   - there's a lot of inlining possibilities as things grow,
   - using a switch statement avoids the function call overhead.

   This function could be moved into `parse_insn_normal', but keeping it
   separate makes clear the interface between `parse_insn_normal' and each of
   the handlers.  It's also needed by GAS to insert operands that couldn't be
   resolved during parsing.  */

const char *
m32c_cgen_insert_operand (CGEN_CPU_DESC cd,
			     int opindex,
			     CGEN_FIELDS * fields,
			     CGEN_INSN_BYTES_PTR buffer,
			     bfd_vma pc ATTRIBUTE_UNUSED)
{
  const char * errmsg = NULL;
  unsigned int total_length = CGEN_FIELDS_BITSIZE (fields);

  switch (opindex)
    {
    case M32C_OPERAND_A0 :
      break;
    case M32C_OPERAND_A1 :
      break;
    case M32C_OPERAND_AN16_PUSH_S :
      errmsg = insert_normal (cd, fields->f_4_1, 0, 0, 4, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_BIT16AN :
      errmsg = insert_normal (cd, fields->f_dst16_an, 0, 0, 15, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_BIT16RN :
      errmsg = insert_normal (cd, fields->f_dst16_rn, 0, 0, 14, 2, 32, total_length, buffer);
      break;
    case M32C_OPERAND_BIT3_S :
      {
{
  FLD (f_7_1) = ((((FLD (f_imm3_S)) - (1))) & (1));
  FLD (f_2_2) = ((((unsigned int) (((FLD (f_imm3_S)) - (1))) >> (1))) & (3));
}
        errmsg = insert_normal (cd, fields->f_2_2, 0, 0, 2, 2, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_7_1, 0, 0, 7, 1, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BIT32ANPREFIXED :
      errmsg = insert_normal (cd, fields->f_dst32_an_prefixed, 0, 0, 17, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_BIT32ANUNPREFIXED :
      errmsg = insert_normal (cd, fields->f_dst32_an_unprefixed, 0, 0, 9, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_BIT32RNPREFIXED :
      {
        long value = fields->f_dst32_rn_prefixed_QI;
        value = (((((((~ (value))) << (1))) & (2))) | (((((unsigned int) (value) >> (1))) & (1))));
        errmsg = insert_normal (cd, value, 0, 0, 16, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_BIT32RNUNPREFIXED :
      {
        long value = fields->f_dst32_rn_unprefixed_QI;
        value = (((((((~ (value))) << (1))) & (2))) | (((((unsigned int) (value) >> (1))) & (1))));
        errmsg = insert_normal (cd, value, 0, 0, 8, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_BITBASE16_16_S8 :
      errmsg = insert_normal (cd, fields->f_dsp_16_s8, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_BITBASE16_16_U16 :
      {
        long value = fields->f_dsp_16_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 0, 16, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_BITBASE16_16_U8 :
      errmsg = insert_normal (cd, fields->f_dsp_16_u8, 0, 0, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_BITBASE16_8_U11_S :
      {
{
  FLD (f_bitno16_S) = ((FLD (f_bitbase16_u11_S)) & (7));
  FLD (f_dsp_8_u8) = ((((unsigned int) (FLD (f_bitbase16_u11_S)) >> (3))) & (255));
}
        errmsg = insert_normal (cd, fields->f_bitno16_S, 0, 0, 5, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_8_u8, 0, 0, 8, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED :
      {
{
  FLD (f_bitno32_unprefixed) = ((FLD (f_bitbase32_16_s11_unprefixed)) & (7));
  FLD (f_dsp_16_s8) = ((int) (FLD (f_bitbase32_16_s11_unprefixed)) >> (3));
}
        errmsg = insert_normal (cd, fields->f_bitno32_unprefixed, 0, 0, 13, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_16_s8, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED :
      {
{
  FLD (f_bitno32_unprefixed) = ((FLD (f_bitbase32_16_s19_unprefixed)) & (7));
  FLD (f_dsp_16_s16) = ((int) (FLD (f_bitbase32_16_s19_unprefixed)) >> (3));
}
        errmsg = insert_normal (cd, fields->f_bitno32_unprefixed, 0, 0, 13, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        {
        long value = fields->f_dsp_16_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED :
      {
{
  FLD (f_bitno32_unprefixed) = ((FLD (f_bitbase32_16_u11_unprefixed)) & (7));
  FLD (f_dsp_16_u8) = ((((unsigned int) (FLD (f_bitbase32_16_u11_unprefixed)) >> (3))) & (255));
}
        errmsg = insert_normal (cd, fields->f_bitno32_unprefixed, 0, 0, 13, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_16_u8, 0, 0, 16, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED :
      {
{
  FLD (f_bitno32_unprefixed) = ((FLD (f_bitbase32_16_u19_unprefixed)) & (7));
  FLD (f_dsp_16_u16) = ((((unsigned int) (FLD (f_bitbase32_16_u19_unprefixed)) >> (3))) & (65535));
}
        errmsg = insert_normal (cd, fields->f_bitno32_unprefixed, 0, 0, 13, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        {
        long value = fields->f_dsp_16_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 0, 16, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED :
      {
{
  FLD (f_bitno32_unprefixed) = ((FLD (f_bitbase32_16_u27_unprefixed)) & (7));
  FLD (f_dsp_16_u16) = ((((unsigned int) (FLD (f_bitbase32_16_u27_unprefixed)) >> (3))) & (65535));
  FLD (f_dsp_32_u8) = ((((unsigned int) (FLD (f_bitbase32_16_u27_unprefixed)) >> (19))) & (255));
}
        errmsg = insert_normal (cd, fields->f_bitno32_unprefixed, 0, 0, 13, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        {
        long value = fields->f_dsp_16_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 0, 16, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_24_S11_PREFIXED :
      {
{
  FLD (f_bitno32_prefixed) = ((FLD (f_bitbase32_24_s11_prefixed)) & (7));
  FLD (f_dsp_24_s8) = ((int) (FLD (f_bitbase32_24_s11_prefixed)) >> (3));
}
        errmsg = insert_normal (cd, fields->f_bitno32_prefixed, 0, 0, 21, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_24_s8, 0|(1<<CGEN_IFLD_SIGNED), 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_24_S19_PREFIXED :
      {
{
  FLD (f_bitno32_prefixed) = ((FLD (f_bitbase32_24_s19_prefixed)) & (7));
  FLD (f_dsp_24_u8) = ((((unsigned int) (FLD (f_bitbase32_24_s19_prefixed)) >> (3))) & (255));
  FLD (f_dsp_32_s8) = ((int) (FLD (f_bitbase32_24_s19_prefixed)) >> (11));
}
        errmsg = insert_normal (cd, fields->f_bitno32_prefixed, 0, 0, 21, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_32_s8, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_24_U11_PREFIXED :
      {
{
  FLD (f_bitno32_prefixed) = ((FLD (f_bitbase32_24_u11_prefixed)) & (7));
  FLD (f_dsp_24_u8) = ((((unsigned int) (FLD (f_bitbase32_24_u11_prefixed)) >> (3))) & (255));
}
        errmsg = insert_normal (cd, fields->f_bitno32_prefixed, 0, 0, 21, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_24_U19_PREFIXED :
      {
{
  FLD (f_bitno32_prefixed) = ((FLD (f_bitbase32_24_u19_prefixed)) & (7));
  FLD (f_dsp_24_u8) = ((((unsigned int) (FLD (f_bitbase32_24_u19_prefixed)) >> (3))) & (255));
  FLD (f_dsp_32_u8) = ((((unsigned int) (FLD (f_bitbase32_24_u19_prefixed)) >> (11))) & (255));
}
        errmsg = insert_normal (cd, fields->f_bitno32_prefixed, 0, 0, 21, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITBASE32_24_U27_PREFIXED :
      {
{
  FLD (f_bitno32_prefixed) = ((FLD (f_bitbase32_24_u27_prefixed)) & (7));
  FLD (f_dsp_24_u8) = ((((unsigned int) (FLD (f_bitbase32_24_u27_prefixed)) >> (3))) & (255));
  FLD (f_dsp_32_u16) = ((((unsigned int) (FLD (f_bitbase32_24_u27_prefixed)) >> (11))) & (65535));
}
        errmsg = insert_normal (cd, fields->f_bitno32_prefixed, 0, 0, 21, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        {
        long value = fields->f_dsp_32_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 32, 0, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_BITNO16R :
      errmsg = insert_normal (cd, fields->f_dsp_16_u8, 0, 0, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_BITNO32PREFIXED :
      errmsg = insert_normal (cd, fields->f_bitno32_prefixed, 0, 0, 21, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_BITNO32UNPREFIXED :
      errmsg = insert_normal (cd, fields->f_bitno32_unprefixed, 0, 0, 13, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_10_U6 :
      errmsg = insert_normal (cd, fields->f_dsp_10_u6, 0, 0, 10, 6, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_16_S16 :
      {
        long value = fields->f_dsp_16_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_16_S8 :
      errmsg = insert_normal (cd, fields->f_dsp_16_s8, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_16_U16 :
      {
        long value = fields->f_dsp_16_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 0, 16, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_16_U20 :
      {
{
  FLD (f_dsp_16_u16) = ((FLD (f_dsp_16_u24)) & (65535));
  FLD (f_dsp_32_u8) = ((((unsigned int) (FLD (f_dsp_16_u24)) >> (16))) & (255));
}
        {
        long value = fields->f_dsp_16_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 0, 16, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_DSP_16_U24 :
      {
{
  FLD (f_dsp_16_u16) = ((FLD (f_dsp_16_u24)) & (65535));
  FLD (f_dsp_32_u8) = ((((unsigned int) (FLD (f_dsp_16_u24)) >> (16))) & (255));
}
        {
        long value = fields->f_dsp_16_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 0, 16, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_DSP_16_U8 :
      errmsg = insert_normal (cd, fields->f_dsp_16_u8, 0, 0, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_24_S16 :
      {
{
  FLD (f_dsp_24_u8) = ((FLD (f_dsp_24_s16)) & (255));
  FLD (f_dsp_32_u8) = ((((unsigned int) (FLD (f_dsp_24_s16)) >> (8))) & (255));
}
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_DSP_24_S8 :
      errmsg = insert_normal (cd, fields->f_dsp_24_s8, 0|(1<<CGEN_IFLD_SIGNED), 0, 24, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_24_U16 :
      {
{
  FLD (f_dsp_24_u8) = ((FLD (f_dsp_24_u16)) & (255));
  FLD (f_dsp_32_u8) = ((((unsigned int) (FLD (f_dsp_24_u16)) >> (8))) & (255));
}
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_DSP_24_U20 :
      {
{
  FLD (f_dsp_24_u8) = ((FLD (f_dsp_24_u24)) & (255));
  FLD (f_dsp_32_u16) = ((((unsigned int) (FLD (f_dsp_24_u24)) >> (8))) & (65535));
}
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        {
        long value = fields->f_dsp_32_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 32, 0, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_DSP_24_U24 :
      {
{
  FLD (f_dsp_24_u8) = ((FLD (f_dsp_24_u24)) & (255));
  FLD (f_dsp_32_u16) = ((((unsigned int) (FLD (f_dsp_24_u24)) >> (8))) & (65535));
}
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        {
        long value = fields->f_dsp_32_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 32, 0, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_DSP_24_U8 :
      errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_32_S16 :
      {
        long value = fields->f_dsp_32_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_32_S8 :
      errmsg = insert_normal (cd, fields->f_dsp_32_s8, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_32_U16 :
      {
        long value = fields->f_dsp_32_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 32, 0, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_32_U20 :
      {
        long value = fields->f_dsp_32_u24;
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        errmsg = insert_normal (cd, value, 0, 32, 0, 24, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_32_U24 :
      {
        long value = fields->f_dsp_32_u24;
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        errmsg = insert_normal (cd, value, 0, 32, 0, 24, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_32_U8 :
      errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_40_S16 :
      {
        long value = fields->f_dsp_40_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 32, 8, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_40_S8 :
      errmsg = insert_normal (cd, fields->f_dsp_40_s8, 0|(1<<CGEN_IFLD_SIGNED), 32, 8, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_40_U16 :
      {
        long value = fields->f_dsp_40_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 32, 8, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_40_U24 :
      {
        long value = fields->f_dsp_40_u24;
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        errmsg = insert_normal (cd, value, 0, 32, 8, 24, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_40_U8 :
      errmsg = insert_normal (cd, fields->f_dsp_40_u8, 0, 32, 8, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_48_S16 :
      {
        long value = fields->f_dsp_48_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 32, 16, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_48_S8 :
      errmsg = insert_normal (cd, fields->f_dsp_48_s8, 0|(1<<CGEN_IFLD_SIGNED), 32, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_48_U16 :
      {
        long value = fields->f_dsp_48_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 32, 16, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_48_U24 :
      {
{
  FLD (f_dsp_64_u8) = ((((unsigned int) (FLD (f_dsp_48_u24)) >> (16))) & (255));
  FLD (f_dsp_48_u16) = ((FLD (f_dsp_48_u24)) & (65535));
}
        {
        long value = fields->f_dsp_48_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 32, 16, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_64_u8, 0, 64, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_DSP_48_U8 :
      errmsg = insert_normal (cd, fields->f_dsp_48_u8, 0, 32, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_8_S24 :
      {
        long value = fields->f_dsp_8_s24;
        value = ((((((unsigned int) (value) >> (16))) | (((value) & (65280))))) | (((EXTQISI (TRUNCSIQI (((value) & (255))))) << (16))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 24, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_8_S8 :
      errmsg = insert_normal (cd, fields->f_dsp_8_s8, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_8_U16 :
      {
        long value = fields->f_dsp_8_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 0, 8, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_8_U24 :
      {
        long value = fields->f_dsp_8_u24;
        value = ((((((unsigned int) (value) >> (16))) | (((value) & (65280))))) | (((((value) & (255))) << (16))));
        errmsg = insert_normal (cd, value, 0, 0, 8, 24, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DSP_8_U6 :
      errmsg = insert_normal (cd, fields->f_dsp_8_u6, 0, 0, 8, 6, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DSP_8_U8 :
      errmsg = insert_normal (cd, fields->f_dsp_8_u8, 0, 0, 8, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16AN :
      errmsg = insert_normal (cd, fields->f_dst16_an, 0, 0, 15, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16AN_S :
      errmsg = insert_normal (cd, fields->f_dst16_an_s, 0, 0, 4, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16ANHI :
      errmsg = insert_normal (cd, fields->f_dst16_an, 0, 0, 15, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16ANQI :
      errmsg = insert_normal (cd, fields->f_dst16_an, 0, 0, 15, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16ANQI_S :
      errmsg = insert_normal (cd, fields->f_dst16_rn_QI_s, 0, 0, 5, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16ANSI :
      errmsg = insert_normal (cd, fields->f_dst16_an, 0, 0, 15, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16RNEXTQI :
      errmsg = insert_normal (cd, fields->f_dst16_rn_ext, 0, 0, 14, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16RNHI :
      errmsg = insert_normal (cd, fields->f_dst16_rn, 0, 0, 14, 2, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16RNQI :
      errmsg = insert_normal (cd, fields->f_dst16_rn, 0, 0, 14, 2, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16RNQI_S :
      errmsg = insert_normal (cd, fields->f_dst16_rn_QI_s, 0, 0, 5, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST16RNSI :
      errmsg = insert_normal (cd, fields->f_dst16_rn, 0, 0, 14, 2, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32ANEXTUNPREFIXED :
      errmsg = insert_normal (cd, fields->f_dst32_an_unprefixed, 0, 0, 9, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32ANPREFIXED :
      errmsg = insert_normal (cd, fields->f_dst32_an_prefixed, 0, 0, 17, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDHI :
      errmsg = insert_normal (cd, fields->f_dst32_an_prefixed, 0, 0, 17, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDQI :
      errmsg = insert_normal (cd, fields->f_dst32_an_prefixed, 0, 0, 17, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDSI :
      errmsg = insert_normal (cd, fields->f_dst32_an_prefixed, 0, 0, 17, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXED :
      errmsg = insert_normal (cd, fields->f_dst32_an_unprefixed, 0, 0, 9, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDHI :
      errmsg = insert_normal (cd, fields->f_dst32_an_unprefixed, 0, 0, 9, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDQI :
      errmsg = insert_normal (cd, fields->f_dst32_an_unprefixed, 0, 0, 9, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDSI :
      errmsg = insert_normal (cd, fields->f_dst32_an_unprefixed, 0, 0, 9, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32R0HI_S :
      break;
    case M32C_OPERAND_DST32R0QI_S :
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDHI :
      errmsg = insert_normal (cd, fields->f_dst32_rn_ext_unprefixed, 0, 0, 9, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDQI :
      errmsg = insert_normal (cd, fields->f_dst32_rn_ext_unprefixed, 0, 0, 9, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_DST32RNPREFIXEDHI :
      {
        long value = fields->f_dst32_rn_prefixed_HI;
        value = ((((value) + (2))) % (4));
        errmsg = insert_normal (cd, value, 0, 0, 16, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DST32RNPREFIXEDQI :
      {
        long value = fields->f_dst32_rn_prefixed_QI;
        value = (((((((~ (value))) << (1))) & (2))) | (((((unsigned int) (value) >> (1))) & (1))));
        errmsg = insert_normal (cd, value, 0, 0, 16, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DST32RNPREFIXEDSI :
      {
        long value = fields->f_dst32_rn_prefixed_SI;
        value = ((value) + (2));
        errmsg = insert_normal (cd, value, 0, 0, 16, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDHI :
      {
        long value = fields->f_dst32_rn_unprefixed_HI;
        value = ((((value) + (2))) % (4));
        errmsg = insert_normal (cd, value, 0, 0, 8, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDQI :
      {
        long value = fields->f_dst32_rn_unprefixed_QI;
        value = (((((((~ (value))) << (1))) & (2))) | (((((unsigned int) (value) >> (1))) & (1))));
        errmsg = insert_normal (cd, value, 0, 0, 8, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDSI :
      {
        long value = fields->f_dst32_rn_unprefixed_SI;
        value = ((value) + (2));
        errmsg = insert_normal (cd, value, 0, 0, 8, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_G :
      break;
    case M32C_OPERAND_IMM_12_S4 :
      errmsg = insert_normal (cd, fields->f_imm_12_s4, 0|(1<<CGEN_IFLD_SIGNED), 0, 12, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_12_S4N :
      errmsg = insert_normal (cd, fields->f_imm_12_s4, 0|(1<<CGEN_IFLD_SIGNED), 0, 12, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_13_U3 :
      errmsg = insert_normal (cd, fields->f_imm_13_u3, 0, 0, 13, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_16_HI :
      {
        long value = fields->f_dsp_16_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_IMM_16_QI :
      errmsg = insert_normal (cd, fields->f_dsp_16_s8, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_16_SI :
      {
{
  FLD (f_dsp_32_u16) = ((((unsigned int) (FLD (f_dsp_16_s32)) >> (16))) & (65535));
  FLD (f_dsp_16_u16) = ((FLD (f_dsp_16_s32)) & (65535));
}
        {
        long value = fields->f_dsp_16_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 0, 16, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
        {
        long value = fields->f_dsp_32_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 32, 0, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_IMM_20_S4 :
      errmsg = insert_normal (cd, fields->f_imm_20_s4, 0|(1<<CGEN_IFLD_SIGNED), 0, 20, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_24_HI :
      {
{
  FLD (f_dsp_24_u8) = ((FLD (f_dsp_24_s16)) & (255));
  FLD (f_dsp_32_u8) = ((((unsigned int) (FLD (f_dsp_24_s16)) >> (8))) & (255));
}
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_IMM_24_QI :
      errmsg = insert_normal (cd, fields->f_dsp_24_s8, 0|(1<<CGEN_IFLD_SIGNED), 0, 24, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_24_SI :
      {
{
  FLD (f_dsp_32_u24) = ((((unsigned int) (FLD (f_dsp_24_s32)) >> (8))) & (16777215));
  FLD (f_dsp_24_u8) = ((FLD (f_dsp_24_s32)) & (255));
}
        errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        {
        long value = fields->f_dsp_32_u24;
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        errmsg = insert_normal (cd, value, 0, 32, 0, 24, 32, total_length, buffer);
      }
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_IMM_32_HI :
      {
        long value = fields->f_dsp_32_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_IMM_32_QI :
      errmsg = insert_normal (cd, fields->f_dsp_32_s8, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_32_SI :
      {
        long value = fields->f_dsp_32_s32;
        value = EXTSISI (((((((((unsigned int) (value) >> (24))) & (255))) | (((((unsigned int) (value) >> (8))) & (65280))))) | (((((((value) << (8))) & (16711680))) | (((((value) << (24))) & (0xff000000)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 32, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_IMM_40_HI :
      {
        long value = fields->f_dsp_40_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 32, 8, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_IMM_40_QI :
      errmsg = insert_normal (cd, fields->f_dsp_40_s8, 0|(1<<CGEN_IFLD_SIGNED), 32, 8, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_40_SI :
      {
{
  FLD (f_dsp_64_u8) = ((((unsigned int) (FLD (f_dsp_40_s32)) >> (24))) & (255));
  FLD (f_dsp_40_u24) = ((FLD (f_dsp_40_s32)) & (16777215));
}
        {
        long value = fields->f_dsp_40_u24;
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        errmsg = insert_normal (cd, value, 0, 32, 8, 24, 32, total_length, buffer);
      }
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_64_u8, 0, 64, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_IMM_48_HI :
      {
        long value = fields->f_dsp_48_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 32, 16, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_IMM_48_QI :
      errmsg = insert_normal (cd, fields->f_dsp_48_s8, 0|(1<<CGEN_IFLD_SIGNED), 32, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_48_SI :
      {
{
  FLD (f_dsp_64_u16) = ((((unsigned int) (FLD (f_dsp_48_s32)) >> (16))) & (65535));
  FLD (f_dsp_48_u16) = ((FLD (f_dsp_48_s32)) & (65535));
}
        {
        long value = fields->f_dsp_48_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 32, 16, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
        {
        long value = fields->f_dsp_64_u16;
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        errmsg = insert_normal (cd, value, 0, 64, 0, 16, 32, total_length, buffer);
      }
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_IMM_56_HI :
      {
{
  FLD (f_dsp_56_u8) = ((FLD (f_dsp_56_s16)) & (255));
  FLD (f_dsp_64_u8) = ((((unsigned int) (FLD (f_dsp_56_s16)) >> (8))) & (255));
}
        errmsg = insert_normal (cd, fields->f_dsp_56_u8, 0, 32, 24, 8, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_dsp_64_u8, 0, 64, 0, 8, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_IMM_56_QI :
      errmsg = insert_normal (cd, fields->f_dsp_56_s8, 0|(1<<CGEN_IFLD_SIGNED), 32, 24, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_64_HI :
      {
        long value = fields->f_dsp_64_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 64, 0, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_IMM_8_HI :
      {
        long value = fields->f_dsp_8_s16;
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_IMM_8_QI :
      errmsg = insert_normal (cd, fields->f_dsp_8_s8, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_8_S4 :
      errmsg = insert_normal (cd, fields->f_imm_8_s4, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_8_S4N :
      errmsg = insert_normal (cd, fields->f_imm_8_s4, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_SH_12_S4 :
      errmsg = insert_normal (cd, fields->f_imm_12_s4, 0|(1<<CGEN_IFLD_SIGNED), 0, 12, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_SH_20_S4 :
      errmsg = insert_normal (cd, fields->f_imm_20_s4, 0|(1<<CGEN_IFLD_SIGNED), 0, 20, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM_SH_8_S4 :
      errmsg = insert_normal (cd, fields->f_imm_8_s4, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_IMM1_S :
      {
        long value = fields->f_imm1_S;
        value = ((value) - (1));
        errmsg = insert_normal (cd, value, 0, 0, 2, 1, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_IMM3_S :
      {
{
  FLD (f_7_1) = ((((FLD (f_imm3_S)) - (1))) & (1));
  FLD (f_2_2) = ((((unsigned int) (((FLD (f_imm3_S)) - (1))) >> (1))) & (3));
}
        errmsg = insert_normal (cd, fields->f_2_2, 0, 0, 2, 2, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_7_1, 0, 0, 7, 1, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_LAB_16_8 :
      {
        long value = fields->f_lab_16_8;
        value = ((value) - (((pc) + (2))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 16, 8, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_LAB_24_8 :
      {
        long value = fields->f_lab_24_8;
        value = ((value) - (((pc) + (2))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 24, 8, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_LAB_32_8 :
      {
        long value = fields->f_lab_32_8;
        value = ((value) - (((pc) + (2))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 32, 0, 8, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_LAB_40_8 :
      {
        long value = fields->f_lab_40_8;
        value = ((value) - (((pc) + (2))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 32, 8, 8, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_LAB_5_3 :
      {
        long value = fields->f_lab_5_3;
        value = ((value) - (((pc) + (2))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_PCREL_ADDR), 0, 5, 3, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_LAB_8_16 :
      {
        long value = fields->f_lab_8_16;
        value = ((((((((value) - (((pc) + (1))))) & (255))) << (8))) | (((unsigned int) (((((value) - (((pc) + (1))))) & (65535))) >> (8))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGN_OPT)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 8, 16, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_LAB_8_24 :
      {
        long value = fields->f_lab_8_24;
        value = ((((((unsigned int) (value) >> (16))) | (((value) & (65280))))) | (((((value) & (255))) << (16))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 8, 24, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_LAB_8_8 :
      {
        long value = fields->f_lab_8_8;
        value = ((value) - (((pc) + (1))));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 8, 8, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_LAB32_JMP_S :
      {
{
  SI tmp_val;
  tmp_val = ((((FLD (f_lab32_jmp_s)) - (pc))) - (2));
  FLD (f_7_1) = ((tmp_val) & (1));
  FLD (f_2_2) = ((unsigned int) (tmp_val) >> (1));
}
        errmsg = insert_normal (cd, fields->f_2_2, 0, 0, 2, 2, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_7_1, 0, 0, 7, 1, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_Q :
      break;
    case M32C_OPERAND_R0 :
      break;
    case M32C_OPERAND_R0H :
      break;
    case M32C_OPERAND_R0L :
      break;
    case M32C_OPERAND_R1 :
      break;
    case M32C_OPERAND_R1R2R0 :
      break;
    case M32C_OPERAND_R2 :
      break;
    case M32C_OPERAND_R2R0 :
      break;
    case M32C_OPERAND_R3 :
      break;
    case M32C_OPERAND_R3R1 :
      break;
    case M32C_OPERAND_REGSETPOP :
      errmsg = insert_normal (cd, fields->f_8_8, 0, 0, 8, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_REGSETPUSH :
      errmsg = insert_normal (cd, fields->f_8_8, 0, 0, 8, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_RN16_PUSH_S :
      errmsg = insert_normal (cd, fields->f_4_1, 0, 0, 4, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_S :
      break;
    case M32C_OPERAND_SRC16AN :
      errmsg = insert_normal (cd, fields->f_src16_an, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC16ANHI :
      errmsg = insert_normal (cd, fields->f_src16_an, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC16ANQI :
      errmsg = insert_normal (cd, fields->f_src16_an, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC16RNHI :
      errmsg = insert_normal (cd, fields->f_src16_rn, 0, 0, 10, 2, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC16RNQI :
      errmsg = insert_normal (cd, fields->f_src16_rn, 0, 0, 10, 2, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC32ANPREFIXED :
      errmsg = insert_normal (cd, fields->f_src32_an_prefixed, 0, 0, 19, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDHI :
      errmsg = insert_normal (cd, fields->f_src32_an_prefixed, 0, 0, 19, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDQI :
      errmsg = insert_normal (cd, fields->f_src32_an_prefixed, 0, 0, 19, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDSI :
      errmsg = insert_normal (cd, fields->f_src32_an_prefixed, 0, 0, 19, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXED :
      errmsg = insert_normal (cd, fields->f_src32_an_unprefixed, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDHI :
      errmsg = insert_normal (cd, fields->f_src32_an_unprefixed, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDQI :
      errmsg = insert_normal (cd, fields->f_src32_an_unprefixed, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDSI :
      errmsg = insert_normal (cd, fields->f_src32_an_unprefixed, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDHI :
      {
        long value = fields->f_src32_rn_prefixed_HI;
        value = ((((value) + (2))) % (4));
        errmsg = insert_normal (cd, value, 0, 0, 18, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDQI :
      {
        long value = fields->f_src32_rn_prefixed_QI;
        value = (((((((~ (value))) << (1))) & (2))) | (((((unsigned int) (value) >> (1))) & (1))));
        errmsg = insert_normal (cd, value, 0, 0, 18, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDSI :
      {
        long value = fields->f_src32_rn_prefixed_SI;
        value = ((value) + (2));
        errmsg = insert_normal (cd, value, 0, 0, 18, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDHI :
      {
        long value = fields->f_src32_rn_unprefixed_HI;
        value = ((((value) + (2))) % (4));
        errmsg = insert_normal (cd, value, 0, 0, 10, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDQI :
      {
        long value = fields->f_src32_rn_unprefixed_QI;
        value = (((((((~ (value))) << (1))) & (2))) | (((((unsigned int) (value) >> (1))) & (1))));
        errmsg = insert_normal (cd, value, 0, 0, 10, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDSI :
      {
        long value = fields->f_src32_rn_unprefixed_SI;
        value = ((value) + (2));
        errmsg = insert_normal (cd, value, 0, 0, 10, 2, 32, total_length, buffer);
      }
      break;
    case M32C_OPERAND_SRCDST16_R0L_R0H_S_NORMAL :
      errmsg = insert_normal (cd, fields->f_5_1, 0, 0, 5, 1, 32, total_length, buffer);
      break;
    case M32C_OPERAND_X :
      break;
    case M32C_OPERAND_Z :
      break;
    case M32C_OPERAND_COND16_16 :
      errmsg = insert_normal (cd, fields->f_dsp_16_u8, 0, 0, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND16_24 :
      errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND16_32 :
      errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND16C :
      errmsg = insert_normal (cd, fields->f_cond16, 0, 0, 12, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND16J :
      errmsg = insert_normal (cd, fields->f_cond16, 0, 0, 12, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND16J5 :
      errmsg = insert_normal (cd, fields->f_cond16j_5, 0, 0, 5, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND32 :
      {
{
  FLD (f_9_1) = ((((unsigned int) (FLD (f_cond32)) >> (3))) & (1));
  FLD (f_13_3) = ((FLD (f_cond32)) & (7));
}
        errmsg = insert_normal (cd, fields->f_9_1, 0, 0, 9, 1, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_13_3, 0, 0, 13, 3, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_COND32_16 :
      errmsg = insert_normal (cd, fields->f_dsp_16_u8, 0, 0, 16, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND32_24 :
      errmsg = insert_normal (cd, fields->f_dsp_24_u8, 0, 0, 24, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND32_32 :
      errmsg = insert_normal (cd, fields->f_dsp_32_u8, 0, 32, 0, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND32_40 :
      errmsg = insert_normal (cd, fields->f_dsp_40_u8, 0, 32, 8, 8, 32, total_length, buffer);
      break;
    case M32C_OPERAND_COND32J :
      {
{
  FLD (f_1_3) = ((((unsigned int) (FLD (f_cond32j)) >> (1))) & (7));
  FLD (f_7_1) = ((FLD (f_cond32j)) & (1));
}
        errmsg = insert_normal (cd, fields->f_1_3, 0, 0, 1, 3, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_7_1, 0, 0, 7, 1, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case M32C_OPERAND_CR1_PREFIXED_32 :
      errmsg = insert_normal (cd, fields->f_21_3, 0, 0, 21, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_CR1_UNPREFIXED_32 :
      errmsg = insert_normal (cd, fields->f_13_3, 0, 0, 13, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_CR16 :
      errmsg = insert_normal (cd, fields->f_9_3, 0, 0, 9, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_CR2_32 :
      errmsg = insert_normal (cd, fields->f_13_3, 0, 0, 13, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_CR3_PREFIXED_32 :
      errmsg = insert_normal (cd, fields->f_21_3, 0, 0, 21, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_CR3_UNPREFIXED_32 :
      errmsg = insert_normal (cd, fields->f_13_3, 0, 0, 13, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_FLAGS16 :
      errmsg = insert_normal (cd, fields->f_9_3, 0, 0, 9, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_FLAGS32 :
      errmsg = insert_normal (cd, fields->f_13_3, 0, 0, 13, 3, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SCCOND32 :
      errmsg = insert_normal (cd, fields->f_cond16, 0, 0, 12, 4, 32, total_length, buffer);
      break;
    case M32C_OPERAND_SIZE :
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while building insn.\n"),
	       opindex);
      abort ();
  }

  return errmsg;
}

int m32c_cgen_extract_operand
  (CGEN_CPU_DESC, int, CGEN_EXTRACT_INFO *, CGEN_INSN_INT, CGEN_FIELDS *, bfd_vma);

/* Main entry point for operand extraction.
   The result is <= 0 for error, >0 for success.
   ??? Actual values aren't well defined right now.

   This function is basically just a big switch statement.  Earlier versions
   used tables to look up the function to use, but
   - if the table contains both assembler and disassembler functions then
     the disassembler contains much of the assembler and vice-versa,
   - there's a lot of inlining possibilities as things grow,
   - using a switch statement avoids the function call overhead.

   This function could be moved into `print_insn_normal', but keeping it
   separate makes clear the interface between `print_insn_normal' and each of
   the handlers.  */

int
m32c_cgen_extract_operand (CGEN_CPU_DESC cd,
			     int opindex,
			     CGEN_EXTRACT_INFO *ex_info,
			     CGEN_INSN_INT insn_value,
			     CGEN_FIELDS * fields,
			     bfd_vma pc)
{
  /* Assume success (for those operands that are nops).  */
  int length = 1;
  unsigned int total_length = CGEN_FIELDS_BITSIZE (fields);

  switch (opindex)
    {
    case M32C_OPERAND_A0 :
      break;
    case M32C_OPERAND_A1 :
      break;
    case M32C_OPERAND_AN16_PUSH_S :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 4, 1, 32, total_length, pc, & fields->f_4_1);
      break;
    case M32C_OPERAND_BIT16AN :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 1, 32, total_length, pc, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_BIT16RN :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 2, 32, total_length, pc, & fields->f_dst16_rn);
      break;
    case M32C_OPERAND_BIT3_S :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 2, 2, 32, total_length, pc, & fields->f_2_2);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 7, 1, 32, total_length, pc, & fields->f_7_1);
        if (length <= 0) break;
{
  FLD (f_imm3_S) = ((((((FLD (f_2_2)) << (1))) | (FLD (f_7_1)))) + (1));
}
      }
      break;
    case M32C_OPERAND_BIT32ANPREFIXED :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 1, 32, total_length, pc, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_BIT32ANUNPREFIXED :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_BIT32RNPREFIXED :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 2, 32, total_length, pc, & value);
        value = (((((~ (((unsigned int) (value) >> (1))))) & (1))) | (((((value) << (1))) & (2))));
        fields->f_dst32_rn_prefixed_QI = value;
      }
      break;
    case M32C_OPERAND_BIT32RNUNPREFIXED :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 2, 32, total_length, pc, & value);
        value = (((((~ (((unsigned int) (value) >> (1))))) & (1))) | (((((value) << (1))) & (2))));
        fields->f_dst32_rn_unprefixed_QI = value;
      }
      break;
    case M32C_OPERAND_BITBASE16_16_S8 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_s8);
      break;
    case M32C_OPERAND_BITBASE16_16_U16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_16_u16 = value;
      }
      break;
    case M32C_OPERAND_BITBASE16_16_U8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_u8);
      break;
    case M32C_OPERAND_BITBASE16_8_U11_S :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 3, 32, total_length, pc, & fields->f_bitno16_S);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 8, 32, total_length, pc, & fields->f_dsp_8_u8);
        if (length <= 0) break;
{
  FLD (f_bitbase16_u11_S) = ((((FLD (f_dsp_8_u8)) << (3))) | (FLD (f_bitno16_S)));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_bitno32_unprefixed);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_s8);
        if (length <= 0) break;
{
  FLD (f_bitbase32_16_s11_unprefixed) = ((((FLD (f_dsp_16_s8)) << (3))) | (FLD (f_bitno32_unprefixed)));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_bitno32_unprefixed);
        if (length <= 0) break;
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_16_s16 = value;
      }
        if (length <= 0) break;
{
  FLD (f_bitbase32_16_s19_unprefixed) = ((((FLD (f_dsp_16_s16)) << (3))) | (FLD (f_bitno32_unprefixed)));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_bitno32_unprefixed);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_u8);
        if (length <= 0) break;
{
  FLD (f_bitbase32_16_u11_unprefixed) = ((((FLD (f_dsp_16_u8)) << (3))) | (FLD (f_bitno32_unprefixed)));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_bitno32_unprefixed);
        if (length <= 0) break;
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_16_u16 = value;
      }
        if (length <= 0) break;
{
  FLD (f_bitbase32_16_u19_unprefixed) = ((((FLD (f_dsp_16_u16)) << (3))) | (FLD (f_bitno32_unprefixed)));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_bitno32_unprefixed);
        if (length <= 0) break;
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_16_u16 = value;
      }
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
        if (length <= 0) break;
{
  FLD (f_bitbase32_16_u27_unprefixed) = ((((FLD (f_dsp_16_u16)) << (3))) | (((((FLD (f_dsp_32_u8)) << (19))) | (FLD (f_bitno32_unprefixed)))));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_24_S11_PREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 21, 3, 32, total_length, pc, & fields->f_bitno32_prefixed);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_s8);
        if (length <= 0) break;
{
  FLD (f_bitbase32_24_s11_prefixed) = ((((FLD (f_dsp_24_s8)) << (3))) | (FLD (f_bitno32_prefixed)));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_24_S19_PREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 21, 3, 32, total_length, pc, & fields->f_bitno32_prefixed);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_s8);
        if (length <= 0) break;
{
  FLD (f_bitbase32_24_s19_prefixed) = ((((FLD (f_dsp_24_u8)) << (3))) | (((((FLD (f_dsp_32_s8)) << (11))) | (FLD (f_bitno32_prefixed)))));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_24_U11_PREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 21, 3, 32, total_length, pc, & fields->f_bitno32_prefixed);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
{
  FLD (f_bitbase32_24_u11_prefixed) = ((((FLD (f_dsp_24_u8)) << (3))) | (FLD (f_bitno32_prefixed)));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_24_U19_PREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 21, 3, 32, total_length, pc, & fields->f_bitno32_prefixed);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
        if (length <= 0) break;
{
  FLD (f_bitbase32_24_u19_prefixed) = ((((FLD (f_dsp_24_u8)) << (3))) | (((((FLD (f_dsp_32_u8)) << (11))) | (FLD (f_bitno32_prefixed)))));
}
      }
      break;
    case M32C_OPERAND_BITBASE32_24_U27_PREFIXED :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 21, 3, 32, total_length, pc, & fields->f_bitno32_prefixed);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_32_u16 = value;
      }
        if (length <= 0) break;
{
  FLD (f_bitbase32_24_u27_prefixed) = ((((FLD (f_dsp_24_u8)) << (3))) | (((((FLD (f_dsp_32_u16)) << (11))) | (FLD (f_bitno32_prefixed)))));
}
      }
      break;
    case M32C_OPERAND_BITNO16R :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_u8);
      break;
    case M32C_OPERAND_BITNO32PREFIXED :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 21, 3, 32, total_length, pc, & fields->f_bitno32_prefixed);
      break;
    case M32C_OPERAND_BITNO32UNPREFIXED :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_bitno32_unprefixed);
      break;
    case M32C_OPERAND_DSP_10_U6 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 10, 6, 32, total_length, pc, & fields->f_dsp_10_u6);
      break;
    case M32C_OPERAND_DSP_16_S16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_16_s16 = value;
      }
      break;
    case M32C_OPERAND_DSP_16_S8 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_s8);
      break;
    case M32C_OPERAND_DSP_16_U16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_16_u16 = value;
      }
      break;
    case M32C_OPERAND_DSP_16_U20 :
      {
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_16_u16 = value;
      }
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
        if (length <= 0) break;
{
  FLD (f_dsp_16_u24) = ((((FLD (f_dsp_32_u8)) << (16))) | (FLD (f_dsp_16_u16)));
}
      }
      break;
    case M32C_OPERAND_DSP_16_U24 :
      {
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_16_u16 = value;
      }
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
        if (length <= 0) break;
{
  FLD (f_dsp_16_u24) = ((((FLD (f_dsp_32_u8)) << (16))) | (FLD (f_dsp_16_u16)));
}
      }
      break;
    case M32C_OPERAND_DSP_16_U8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_u8);
      break;
    case M32C_OPERAND_DSP_24_S16 :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
        if (length <= 0) break;
{
  FLD (f_dsp_24_s16) = EXTHISI (((HI) (UINT) (((((FLD (f_dsp_32_u8)) << (8))) | (FLD (f_dsp_24_u8))))));
}
      }
      break;
    case M32C_OPERAND_DSP_24_S8 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_s8);
      break;
    case M32C_OPERAND_DSP_24_U16 :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
        if (length <= 0) break;
{
  FLD (f_dsp_24_u16) = ((((FLD (f_dsp_32_u8)) << (8))) | (FLD (f_dsp_24_u8)));
}
      }
      break;
    case M32C_OPERAND_DSP_24_U20 :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_32_u16 = value;
      }
        if (length <= 0) break;
{
  FLD (f_dsp_24_u24) = ((((FLD (f_dsp_32_u16)) << (8))) | (FLD (f_dsp_24_u8)));
}
      }
      break;
    case M32C_OPERAND_DSP_24_U24 :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_32_u16 = value;
      }
        if (length <= 0) break;
{
  FLD (f_dsp_24_u24) = ((((FLD (f_dsp_32_u16)) << (8))) | (FLD (f_dsp_24_u8)));
}
      }
      break;
    case M32C_OPERAND_DSP_24_U8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
      break;
    case M32C_OPERAND_DSP_32_S16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_32_s16 = value;
      }
      break;
    case M32C_OPERAND_DSP_32_S8 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_s8);
      break;
    case M32C_OPERAND_DSP_32_U16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_32_u16 = value;
      }
      break;
    case M32C_OPERAND_DSP_32_U20 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 24, 32, total_length, pc, & value);
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        fields->f_dsp_32_u24 = value;
      }
      break;
    case M32C_OPERAND_DSP_32_U24 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 24, 32, total_length, pc, & value);
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        fields->f_dsp_32_u24 = value;
      }
      break;
    case M32C_OPERAND_DSP_32_U8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
      break;
    case M32C_OPERAND_DSP_40_S16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 8, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_40_s16 = value;
      }
      break;
    case M32C_OPERAND_DSP_40_S8 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 8, 8, 32, total_length, pc, & fields->f_dsp_40_s8);
      break;
    case M32C_OPERAND_DSP_40_U16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 8, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_40_u16 = value;
      }
      break;
    case M32C_OPERAND_DSP_40_U24 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 8, 24, 32, total_length, pc, & value);
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        fields->f_dsp_40_u24 = value;
      }
      break;
    case M32C_OPERAND_DSP_40_U8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 32, 8, 8, 32, total_length, pc, & fields->f_dsp_40_u8);
      break;
    case M32C_OPERAND_DSP_48_S16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 16, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_48_s16 = value;
      }
      break;
    case M32C_OPERAND_DSP_48_S8 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 16, 8, 32, total_length, pc, & fields->f_dsp_48_s8);
      break;
    case M32C_OPERAND_DSP_48_U16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_48_u16 = value;
      }
      break;
    case M32C_OPERAND_DSP_48_U24 :
      {
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_48_u16 = value;
      }
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 64, 0, 8, 32, total_length, pc, & fields->f_dsp_64_u8);
        if (length <= 0) break;
{
  FLD (f_dsp_48_u24) = ((((FLD (f_dsp_48_u16)) & (65535))) | (((((FLD (f_dsp_64_u8)) << (16))) & (16711680))));
}
      }
      break;
    case M32C_OPERAND_DSP_48_U8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 32, 16, 8, 32, total_length, pc, & fields->f_dsp_48_u8);
      break;
    case M32C_OPERAND_DSP_8_S24 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 24, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (16))) | (((value) & (65280))))) | (((EXTQISI (TRUNCSIQI (((value) & (255))))) << (16))));
        fields->f_dsp_8_s24 = value;
      }
      break;
    case M32C_OPERAND_DSP_8_S8 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 8, 32, total_length, pc, & fields->f_dsp_8_s8);
      break;
    case M32C_OPERAND_DSP_8_U16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_8_u16 = value;
      }
      break;
    case M32C_OPERAND_DSP_8_U24 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 24, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (16))) | (((value) & (65280))))) | (((((value) & (255))) << (16))));
        fields->f_dsp_8_u24 = value;
      }
      break;
    case M32C_OPERAND_DSP_8_U6 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 6, 32, total_length, pc, & fields->f_dsp_8_u6);
      break;
    case M32C_OPERAND_DSP_8_U8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 8, 32, total_length, pc, & fields->f_dsp_8_u8);
      break;
    case M32C_OPERAND_DST16AN :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 1, 32, total_length, pc, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_DST16AN_S :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 4, 1, 32, total_length, pc, & fields->f_dst16_an_s);
      break;
    case M32C_OPERAND_DST16ANHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 1, 32, total_length, pc, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_DST16ANQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 1, 32, total_length, pc, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_DST16ANQI_S :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 1, 32, total_length, pc, & fields->f_dst16_rn_QI_s);
      break;
    case M32C_OPERAND_DST16ANSI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 1, 32, total_length, pc, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_DST16RNEXTQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 1, 32, total_length, pc, & fields->f_dst16_rn_ext);
      break;
    case M32C_OPERAND_DST16RNHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 2, 32, total_length, pc, & fields->f_dst16_rn);
      break;
    case M32C_OPERAND_DST16RNQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 2, 32, total_length, pc, & fields->f_dst16_rn);
      break;
    case M32C_OPERAND_DST16RNQI_S :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 1, 32, total_length, pc, & fields->f_dst16_rn_QI_s);
      break;
    case M32C_OPERAND_DST16RNSI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 2, 32, total_length, pc, & fields->f_dst16_rn);
      break;
    case M32C_OPERAND_DST32ANEXTUNPREFIXED :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32ANPREFIXED :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 1, 32, total_length, pc, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 1, 32, total_length, pc, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 1, 32, total_length, pc, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDSI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 1, 32, total_length, pc, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXED :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDSI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32R0HI_S :
      break;
    case M32C_OPERAND_DST32R0QI_S :
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_dst32_rn_ext_unprefixed);
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_dst32_rn_ext_unprefixed);
      break;
    case M32C_OPERAND_DST32RNPREFIXEDHI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 2, 32, total_length, pc, & value);
        value = ((((value) + (2))) % (4));
        fields->f_dst32_rn_prefixed_HI = value;
      }
      break;
    case M32C_OPERAND_DST32RNPREFIXEDQI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 2, 32, total_length, pc, & value);
        value = (((((~ (((unsigned int) (value) >> (1))))) & (1))) | (((((value) << (1))) & (2))));
        fields->f_dst32_rn_prefixed_QI = value;
      }
      break;
    case M32C_OPERAND_DST32RNPREFIXEDSI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 2, 32, total_length, pc, & value);
        value = ((value) - (2));
        fields->f_dst32_rn_prefixed_SI = value;
      }
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDHI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 2, 32, total_length, pc, & value);
        value = ((((value) + (2))) % (4));
        fields->f_dst32_rn_unprefixed_HI = value;
      }
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDQI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 2, 32, total_length, pc, & value);
        value = (((((~ (((unsigned int) (value) >> (1))))) & (1))) | (((((value) << (1))) & (2))));
        fields->f_dst32_rn_unprefixed_QI = value;
      }
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDSI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 2, 32, total_length, pc, & value);
        value = ((value) - (2));
        fields->f_dst32_rn_unprefixed_SI = value;
      }
      break;
    case M32C_OPERAND_G :
      break;
    case M32C_OPERAND_IMM_12_S4 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 12, 4, 32, total_length, pc, & fields->f_imm_12_s4);
      break;
    case M32C_OPERAND_IMM_12_S4N :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 12, 4, 32, total_length, pc, & fields->f_imm_12_s4);
      break;
    case M32C_OPERAND_IMM_13_U3 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_imm_13_u3);
      break;
    case M32C_OPERAND_IMM_16_HI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_16_s16 = value;
      }
      break;
    case M32C_OPERAND_IMM_16_QI :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_s8);
      break;
    case M32C_OPERAND_IMM_16_SI :
      {
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_16_u16 = value;
      }
        if (length <= 0) break;
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_32_u16 = value;
      }
        if (length <= 0) break;
{
  FLD (f_dsp_16_s32) = ((((FLD (f_dsp_16_u16)) & (65535))) | (((((FLD (f_dsp_32_u16)) << (16))) & (0xffff0000))));
}
      }
      break;
    case M32C_OPERAND_IMM_20_S4 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 20, 4, 32, total_length, pc, & fields->f_imm_20_s4);
      break;
    case M32C_OPERAND_IMM_24_HI :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
        if (length <= 0) break;
{
  FLD (f_dsp_24_s16) = EXTHISI (((HI) (UINT) (((((FLD (f_dsp_32_u8)) << (8))) | (FLD (f_dsp_24_u8))))));
}
      }
      break;
    case M32C_OPERAND_IMM_24_QI :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_s8);
      break;
    case M32C_OPERAND_IMM_24_SI :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
        if (length <= 0) break;
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 24, 32, total_length, pc, & value);
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        fields->f_dsp_32_u24 = value;
      }
        if (length <= 0) break;
{
  FLD (f_dsp_24_s32) = ((((FLD (f_dsp_24_u8)) & (255))) | (((((FLD (f_dsp_32_u24)) << (8))) & (0xffffff00))));
}
      }
      break;
    case M32C_OPERAND_IMM_32_HI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_32_s16 = value;
      }
      break;
    case M32C_OPERAND_IMM_32_QI :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_s8);
      break;
    case M32C_OPERAND_IMM_32_SI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 0, 32, 32, total_length, pc, & value);
        value = EXTSISI (((((((((unsigned int) (value) >> (24))) & (255))) | (((((unsigned int) (value) >> (8))) & (65280))))) | (((((((value) << (8))) & (16711680))) | (((((value) << (24))) & (0xff000000)))))));
        fields->f_dsp_32_s32 = value;
      }
      break;
    case M32C_OPERAND_IMM_40_HI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 8, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_40_s16 = value;
      }
      break;
    case M32C_OPERAND_IMM_40_QI :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 8, 8, 32, total_length, pc, & fields->f_dsp_40_s8);
      break;
    case M32C_OPERAND_IMM_40_SI :
      {
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 8, 24, 32, total_length, pc, & value);
        value = ((((((((unsigned int) (value) >> (16))) & (255))) | (((value) & (65280))))) | (((((value) << (16))) & (16711680))));
        fields->f_dsp_40_u24 = value;
      }
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 64, 0, 8, 32, total_length, pc, & fields->f_dsp_64_u8);
        if (length <= 0) break;
{
  FLD (f_dsp_40_s32) = ((((FLD (f_dsp_40_u24)) & (16777215))) | (((((FLD (f_dsp_64_u8)) << (24))) & (0xff000000))));
}
      }
      break;
    case M32C_OPERAND_IMM_48_HI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 16, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_48_s16 = value;
      }
      break;
    case M32C_OPERAND_IMM_48_QI :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 16, 8, 32, total_length, pc, & fields->f_dsp_48_s8);
      break;
    case M32C_OPERAND_IMM_48_SI :
      {
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 16, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_48_u16 = value;
      }
        if (length <= 0) break;
        {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 64, 0, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280))));
        fields->f_dsp_64_u16 = value;
      }
        if (length <= 0) break;
{
  FLD (f_dsp_48_s32) = ((((FLD (f_dsp_48_u16)) & (65535))) | (((((FLD (f_dsp_64_u16)) << (16))) & (0xffff0000))));
}
      }
      break;
    case M32C_OPERAND_IMM_56_HI :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 32, 24, 8, 32, total_length, pc, & fields->f_dsp_56_u8);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 64, 0, 8, 32, total_length, pc, & fields->f_dsp_64_u8);
        if (length <= 0) break;
{
  FLD (f_dsp_56_s16) = EXTHISI (((HI) (UINT) (((((FLD (f_dsp_64_u8)) << (8))) | (FLD (f_dsp_56_u8))))));
}
      }
      break;
    case M32C_OPERAND_IMM_56_QI :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 32, 24, 8, 32, total_length, pc, & fields->f_dsp_56_s8);
      break;
    case M32C_OPERAND_IMM_64_HI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 64, 0, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_64_s16 = value;
      }
      break;
    case M32C_OPERAND_IMM_8_HI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 16, 32, total_length, pc, & value);
        value = EXTHISI (((HI) (INT) (((((((unsigned int) (value) >> (8))) & (255))) | (((((value) << (8))) & (65280)))))));
        fields->f_dsp_8_s16 = value;
      }
      break;
    case M32C_OPERAND_IMM_8_QI :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 8, 32, total_length, pc, & fields->f_dsp_8_s8);
      break;
    case M32C_OPERAND_IMM_8_S4 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 4, 32, total_length, pc, & fields->f_imm_8_s4);
      break;
    case M32C_OPERAND_IMM_8_S4N :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 4, 32, total_length, pc, & fields->f_imm_8_s4);
      break;
    case M32C_OPERAND_IMM_SH_12_S4 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 12, 4, 32, total_length, pc, & fields->f_imm_12_s4);
      break;
    case M32C_OPERAND_IMM_SH_20_S4 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 20, 4, 32, total_length, pc, & fields->f_imm_20_s4);
      break;
    case M32C_OPERAND_IMM_SH_8_S4 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 8, 4, 32, total_length, pc, & fields->f_imm_8_s4);
      break;
    case M32C_OPERAND_IMM1_S :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 2, 1, 32, total_length, pc, & value);
        value = ((value) + (1));
        fields->f_imm1_S = value;
      }
      break;
    case M32C_OPERAND_IMM3_S :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 2, 2, 32, total_length, pc, & fields->f_2_2);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 7, 1, 32, total_length, pc, & fields->f_7_1);
        if (length <= 0) break;
{
  FLD (f_imm3_S) = ((((((FLD (f_2_2)) << (1))) | (FLD (f_7_1)))) + (1));
}
      }
      break;
    case M32C_OPERAND_LAB_16_8 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 16, 8, 32, total_length, pc, & value);
        value = ((value) + (((pc) + (2))));
        fields->f_lab_16_8 = value;
      }
      break;
    case M32C_OPERAND_LAB_24_8 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 24, 8, 32, total_length, pc, & value);
        value = ((value) + (((pc) + (2))));
        fields->f_lab_24_8 = value;
      }
      break;
    case M32C_OPERAND_LAB_32_8 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 32, 0, 8, 32, total_length, pc, & value);
        value = ((value) + (((pc) + (2))));
        fields->f_lab_32_8 = value;
      }
      break;
    case M32C_OPERAND_LAB_40_8 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 32, 8, 8, 32, total_length, pc, & value);
        value = ((value) + (((pc) + (2))));
        fields->f_lab_40_8 = value;
      }
      break;
    case M32C_OPERAND_LAB_5_3 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_PCREL_ADDR), 0, 5, 3, 32, total_length, pc, & value);
        value = ((value) + (((pc) + (2))));
        fields->f_lab_5_3 = value;
      }
      break;
    case M32C_OPERAND_LAB_8_16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGN_OPT)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 8, 16, 32, total_length, pc, & value);
        value = ((((((unsigned int) (((value) & (65535))) >> (8))) | (((int) (((((value) & (255))) << (24))) >> (16))))) + (((pc) + (1))));
        fields->f_lab_8_16 = value;
      }
      break;
    case M32C_OPERAND_LAB_8_24 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 8, 24, 32, total_length, pc, & value);
        value = ((((((unsigned int) (value) >> (16))) | (((value) & (65280))))) | (((((value) & (255))) << (16))));
        fields->f_lab_8_24 = value;
      }
      break;
    case M32C_OPERAND_LAB_8_8 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 8, 8, 32, total_length, pc, & value);
        value = ((value) + (((pc) + (1))));
        fields->f_lab_8_8 = value;
      }
      break;
    case M32C_OPERAND_LAB32_JMP_S :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 2, 2, 32, total_length, pc, & fields->f_2_2);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 7, 1, 32, total_length, pc, & fields->f_7_1);
        if (length <= 0) break;
{
  FLD (f_lab32_jmp_s) = ((pc) + (((((((FLD (f_2_2)) << (1))) | (FLD (f_7_1)))) + (2))));
}
      }
      break;
    case M32C_OPERAND_Q :
      break;
    case M32C_OPERAND_R0 :
      break;
    case M32C_OPERAND_R0H :
      break;
    case M32C_OPERAND_R0L :
      break;
    case M32C_OPERAND_R1 :
      break;
    case M32C_OPERAND_R1R2R0 :
      break;
    case M32C_OPERAND_R2 :
      break;
    case M32C_OPERAND_R2R0 :
      break;
    case M32C_OPERAND_R3 :
      break;
    case M32C_OPERAND_R3R1 :
      break;
    case M32C_OPERAND_REGSETPOP :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 8, 32, total_length, pc, & fields->f_8_8);
      break;
    case M32C_OPERAND_REGSETPUSH :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 8, 32, total_length, pc, & fields->f_8_8);
      break;
    case M32C_OPERAND_RN16_PUSH_S :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 4, 1, 32, total_length, pc, & fields->f_4_1);
      break;
    case M32C_OPERAND_S :
      break;
    case M32C_OPERAND_SRC16AN :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_src16_an);
      break;
    case M32C_OPERAND_SRC16ANHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_src16_an);
      break;
    case M32C_OPERAND_SRC16ANQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_src16_an);
      break;
    case M32C_OPERAND_SRC16RNHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 10, 2, 32, total_length, pc, & fields->f_src16_rn);
      break;
    case M32C_OPERAND_SRC16RNQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 10, 2, 32, total_length, pc, & fields->f_src16_rn);
      break;
    case M32C_OPERAND_SRC32ANPREFIXED :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 19, 1, 32, total_length, pc, & fields->f_src32_an_prefixed);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 19, 1, 32, total_length, pc, & fields->f_src32_an_prefixed);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 19, 1, 32, total_length, pc, & fields->f_src32_an_prefixed);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDSI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 19, 1, 32, total_length, pc, & fields->f_src32_an_prefixed);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXED :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_src32_an_unprefixed);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_src32_an_unprefixed);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDQI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_src32_an_unprefixed);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDSI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_src32_an_unprefixed);
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDHI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 18, 2, 32, total_length, pc, & value);
        value = ((((value) + (2))) % (4));
        fields->f_src32_rn_prefixed_HI = value;
      }
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDQI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 18, 2, 32, total_length, pc, & value);
        value = (((((~ (((unsigned int) (value) >> (1))))) & (1))) | (((((value) << (1))) & (2))));
        fields->f_src32_rn_prefixed_QI = value;
      }
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDSI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 18, 2, 32, total_length, pc, & value);
        value = ((value) - (2));
        fields->f_src32_rn_prefixed_SI = value;
      }
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDHI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 10, 2, 32, total_length, pc, & value);
        value = ((((value) + (2))) % (4));
        fields->f_src32_rn_unprefixed_HI = value;
      }
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDQI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 10, 2, 32, total_length, pc, & value);
        value = (((((~ (((unsigned int) (value) >> (1))))) & (1))) | (((((value) << (1))) & (2))));
        fields->f_src32_rn_unprefixed_QI = value;
      }
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDSI :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 10, 2, 32, total_length, pc, & value);
        value = ((value) - (2));
        fields->f_src32_rn_unprefixed_SI = value;
      }
      break;
    case M32C_OPERAND_SRCDST16_R0L_R0H_S_NORMAL :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 1, 32, total_length, pc, & fields->f_5_1);
      break;
    case M32C_OPERAND_X :
      break;
    case M32C_OPERAND_Z :
      break;
    case M32C_OPERAND_COND16_16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_u8);
      break;
    case M32C_OPERAND_COND16_24 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
      break;
    case M32C_OPERAND_COND16_32 :
      length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
      break;
    case M32C_OPERAND_COND16C :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 12, 4, 32, total_length, pc, & fields->f_cond16);
      break;
    case M32C_OPERAND_COND16J :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 12, 4, 32, total_length, pc, & fields->f_cond16);
      break;
    case M32C_OPERAND_COND16J5 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 3, 32, total_length, pc, & fields->f_cond16j_5);
      break;
    case M32C_OPERAND_COND32 :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_9_1);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_13_3);
        if (length <= 0) break;
{
  FLD (f_cond32) = ((((FLD (f_9_1)) << (3))) | (FLD (f_13_3)));
}
      }
      break;
    case M32C_OPERAND_COND32_16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 16, 8, 32, total_length, pc, & fields->f_dsp_16_u8);
      break;
    case M32C_OPERAND_COND32_24 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 8, 32, total_length, pc, & fields->f_dsp_24_u8);
      break;
    case M32C_OPERAND_COND32_32 :
      length = extract_normal (cd, ex_info, insn_value, 0, 32, 0, 8, 32, total_length, pc, & fields->f_dsp_32_u8);
      break;
    case M32C_OPERAND_COND32_40 :
      length = extract_normal (cd, ex_info, insn_value, 0, 32, 8, 8, 32, total_length, pc, & fields->f_dsp_40_u8);
      break;
    case M32C_OPERAND_COND32J :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 1, 3, 32, total_length, pc, & fields->f_1_3);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 7, 1, 32, total_length, pc, & fields->f_7_1);
        if (length <= 0) break;
{
  FLD (f_cond32j) = ((((FLD (f_1_3)) << (1))) | (FLD (f_7_1)));
}
      }
      break;
    case M32C_OPERAND_CR1_PREFIXED_32 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 21, 3, 32, total_length, pc, & fields->f_21_3);
      break;
    case M32C_OPERAND_CR1_UNPREFIXED_32 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_13_3);
      break;
    case M32C_OPERAND_CR16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 3, 32, total_length, pc, & fields->f_9_3);
      break;
    case M32C_OPERAND_CR2_32 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_13_3);
      break;
    case M32C_OPERAND_CR3_PREFIXED_32 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 21, 3, 32, total_length, pc, & fields->f_21_3);
      break;
    case M32C_OPERAND_CR3_UNPREFIXED_32 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_13_3);
      break;
    case M32C_OPERAND_FLAGS16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 3, 32, total_length, pc, & fields->f_9_3);
      break;
    case M32C_OPERAND_FLAGS32 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 3, 32, total_length, pc, & fields->f_13_3);
      break;
    case M32C_OPERAND_SCCOND32 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 12, 4, 32, total_length, pc, & fields->f_cond16);
      break;
    case M32C_OPERAND_SIZE :
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while decoding insn.\n"),
	       opindex);
      abort ();
    }

  return length;
}

cgen_insert_fn * const m32c_cgen_insert_handlers[] = 
{
  insert_insn_normal,
};

cgen_extract_fn * const m32c_cgen_extract_handlers[] = 
{
  extract_insn_normal,
};

int m32c_cgen_get_int_operand     (CGEN_CPU_DESC, int, const CGEN_FIELDS *);
bfd_vma m32c_cgen_get_vma_operand (CGEN_CPU_DESC, int, const CGEN_FIELDS *);

/* Getting values from cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they return.
   TODO: floating point, inlining support, remove cases where result type
   not appropriate.  */

int
m32c_cgen_get_int_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     const CGEN_FIELDS * fields)
{
  int value;

  switch (opindex)
    {
    case M32C_OPERAND_A0 :
      value = 0;
      break;
    case M32C_OPERAND_A1 :
      value = 0;
      break;
    case M32C_OPERAND_AN16_PUSH_S :
      value = fields->f_4_1;
      break;
    case M32C_OPERAND_BIT16AN :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_BIT16RN :
      value = fields->f_dst16_rn;
      break;
    case M32C_OPERAND_BIT3_S :
      value = fields->f_imm3_S;
      break;
    case M32C_OPERAND_BIT32ANPREFIXED :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_BIT32ANUNPREFIXED :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_BIT32RNPREFIXED :
      value = fields->f_dst32_rn_prefixed_QI;
      break;
    case M32C_OPERAND_BIT32RNUNPREFIXED :
      value = fields->f_dst32_rn_unprefixed_QI;
      break;
    case M32C_OPERAND_BITBASE16_16_S8 :
      value = fields->f_dsp_16_s8;
      break;
    case M32C_OPERAND_BITBASE16_16_U16 :
      value = fields->f_dsp_16_u16;
      break;
    case M32C_OPERAND_BITBASE16_16_U8 :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_BITBASE16_8_U11_S :
      value = fields->f_bitbase16_u11_S;
      break;
    case M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED :
      value = fields->f_bitbase32_16_s11_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED :
      value = fields->f_bitbase32_16_s19_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED :
      value = fields->f_bitbase32_16_u11_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED :
      value = fields->f_bitbase32_16_u19_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED :
      value = fields->f_bitbase32_16_u27_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_S11_PREFIXED :
      value = fields->f_bitbase32_24_s11_prefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_S19_PREFIXED :
      value = fields->f_bitbase32_24_s19_prefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_U11_PREFIXED :
      value = fields->f_bitbase32_24_u11_prefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_U19_PREFIXED :
      value = fields->f_bitbase32_24_u19_prefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_U27_PREFIXED :
      value = fields->f_bitbase32_24_u27_prefixed;
      break;
    case M32C_OPERAND_BITNO16R :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_BITNO32PREFIXED :
      value = fields->f_bitno32_prefixed;
      break;
    case M32C_OPERAND_BITNO32UNPREFIXED :
      value = fields->f_bitno32_unprefixed;
      break;
    case M32C_OPERAND_DSP_10_U6 :
      value = fields->f_dsp_10_u6;
      break;
    case M32C_OPERAND_DSP_16_S16 :
      value = fields->f_dsp_16_s16;
      break;
    case M32C_OPERAND_DSP_16_S8 :
      value = fields->f_dsp_16_s8;
      break;
    case M32C_OPERAND_DSP_16_U16 :
      value = fields->f_dsp_16_u16;
      break;
    case M32C_OPERAND_DSP_16_U20 :
      value = fields->f_dsp_16_u24;
      break;
    case M32C_OPERAND_DSP_16_U24 :
      value = fields->f_dsp_16_u24;
      break;
    case M32C_OPERAND_DSP_16_U8 :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_DSP_24_S16 :
      value = fields->f_dsp_24_s16;
      break;
    case M32C_OPERAND_DSP_24_S8 :
      value = fields->f_dsp_24_s8;
      break;
    case M32C_OPERAND_DSP_24_U16 :
      value = fields->f_dsp_24_u16;
      break;
    case M32C_OPERAND_DSP_24_U20 :
      value = fields->f_dsp_24_u24;
      break;
    case M32C_OPERAND_DSP_24_U24 :
      value = fields->f_dsp_24_u24;
      break;
    case M32C_OPERAND_DSP_24_U8 :
      value = fields->f_dsp_24_u8;
      break;
    case M32C_OPERAND_DSP_32_S16 :
      value = fields->f_dsp_32_s16;
      break;
    case M32C_OPERAND_DSP_32_S8 :
      value = fields->f_dsp_32_s8;
      break;
    case M32C_OPERAND_DSP_32_U16 :
      value = fields->f_dsp_32_u16;
      break;
    case M32C_OPERAND_DSP_32_U20 :
      value = fields->f_dsp_32_u24;
      break;
    case M32C_OPERAND_DSP_32_U24 :
      value = fields->f_dsp_32_u24;
      break;
    case M32C_OPERAND_DSP_32_U8 :
      value = fields->f_dsp_32_u8;
      break;
    case M32C_OPERAND_DSP_40_S16 :
      value = fields->f_dsp_40_s16;
      break;
    case M32C_OPERAND_DSP_40_S8 :
      value = fields->f_dsp_40_s8;
      break;
    case M32C_OPERAND_DSP_40_U16 :
      value = fields->f_dsp_40_u16;
      break;
    case M32C_OPERAND_DSP_40_U24 :
      value = fields->f_dsp_40_u24;
      break;
    case M32C_OPERAND_DSP_40_U8 :
      value = fields->f_dsp_40_u8;
      break;
    case M32C_OPERAND_DSP_48_S16 :
      value = fields->f_dsp_48_s16;
      break;
    case M32C_OPERAND_DSP_48_S8 :
      value = fields->f_dsp_48_s8;
      break;
    case M32C_OPERAND_DSP_48_U16 :
      value = fields->f_dsp_48_u16;
      break;
    case M32C_OPERAND_DSP_48_U24 :
      value = fields->f_dsp_48_u24;
      break;
    case M32C_OPERAND_DSP_48_U8 :
      value = fields->f_dsp_48_u8;
      break;
    case M32C_OPERAND_DSP_8_S24 :
      value = fields->f_dsp_8_s24;
      break;
    case M32C_OPERAND_DSP_8_S8 :
      value = fields->f_dsp_8_s8;
      break;
    case M32C_OPERAND_DSP_8_U16 :
      value = fields->f_dsp_8_u16;
      break;
    case M32C_OPERAND_DSP_8_U24 :
      value = fields->f_dsp_8_u24;
      break;
    case M32C_OPERAND_DSP_8_U6 :
      value = fields->f_dsp_8_u6;
      break;
    case M32C_OPERAND_DSP_8_U8 :
      value = fields->f_dsp_8_u8;
      break;
    case M32C_OPERAND_DST16AN :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_DST16AN_S :
      value = fields->f_dst16_an_s;
      break;
    case M32C_OPERAND_DST16ANHI :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_DST16ANQI :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_DST16ANQI_S :
      value = fields->f_dst16_rn_QI_s;
      break;
    case M32C_OPERAND_DST16ANSI :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_DST16RNEXTQI :
      value = fields->f_dst16_rn_ext;
      break;
    case M32C_OPERAND_DST16RNHI :
      value = fields->f_dst16_rn;
      break;
    case M32C_OPERAND_DST16RNQI :
      value = fields->f_dst16_rn;
      break;
    case M32C_OPERAND_DST16RNQI_S :
      value = fields->f_dst16_rn_QI_s;
      break;
    case M32C_OPERAND_DST16RNSI :
      value = fields->f_dst16_rn;
      break;
    case M32C_OPERAND_DST32ANEXTUNPREFIXED :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32ANPREFIXED :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDHI :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDQI :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDSI :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXED :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDHI :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDQI :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDSI :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32R0HI_S :
      value = 0;
      break;
    case M32C_OPERAND_DST32R0QI_S :
      value = 0;
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDHI :
      value = fields->f_dst32_rn_ext_unprefixed;
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDQI :
      value = fields->f_dst32_rn_ext_unprefixed;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDHI :
      value = fields->f_dst32_rn_prefixed_HI;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDQI :
      value = fields->f_dst32_rn_prefixed_QI;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDSI :
      value = fields->f_dst32_rn_prefixed_SI;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDHI :
      value = fields->f_dst32_rn_unprefixed_HI;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDQI :
      value = fields->f_dst32_rn_unprefixed_QI;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDSI :
      value = fields->f_dst32_rn_unprefixed_SI;
      break;
    case M32C_OPERAND_G :
      value = 0;
      break;
    case M32C_OPERAND_IMM_12_S4 :
      value = fields->f_imm_12_s4;
      break;
    case M32C_OPERAND_IMM_12_S4N :
      value = fields->f_imm_12_s4;
      break;
    case M32C_OPERAND_IMM_13_U3 :
      value = fields->f_imm_13_u3;
      break;
    case M32C_OPERAND_IMM_16_HI :
      value = fields->f_dsp_16_s16;
      break;
    case M32C_OPERAND_IMM_16_QI :
      value = fields->f_dsp_16_s8;
      break;
    case M32C_OPERAND_IMM_16_SI :
      value = fields->f_dsp_16_s32;
      break;
    case M32C_OPERAND_IMM_20_S4 :
      value = fields->f_imm_20_s4;
      break;
    case M32C_OPERAND_IMM_24_HI :
      value = fields->f_dsp_24_s16;
      break;
    case M32C_OPERAND_IMM_24_QI :
      value = fields->f_dsp_24_s8;
      break;
    case M32C_OPERAND_IMM_24_SI :
      value = fields->f_dsp_24_s32;
      break;
    case M32C_OPERAND_IMM_32_HI :
      value = fields->f_dsp_32_s16;
      break;
    case M32C_OPERAND_IMM_32_QI :
      value = fields->f_dsp_32_s8;
      break;
    case M32C_OPERAND_IMM_32_SI :
      value = fields->f_dsp_32_s32;
      break;
    case M32C_OPERAND_IMM_40_HI :
      value = fields->f_dsp_40_s16;
      break;
    case M32C_OPERAND_IMM_40_QI :
      value = fields->f_dsp_40_s8;
      break;
    case M32C_OPERAND_IMM_40_SI :
      value = fields->f_dsp_40_s32;
      break;
    case M32C_OPERAND_IMM_48_HI :
      value = fields->f_dsp_48_s16;
      break;
    case M32C_OPERAND_IMM_48_QI :
      value = fields->f_dsp_48_s8;
      break;
    case M32C_OPERAND_IMM_48_SI :
      value = fields->f_dsp_48_s32;
      break;
    case M32C_OPERAND_IMM_56_HI :
      value = fields->f_dsp_56_s16;
      break;
    case M32C_OPERAND_IMM_56_QI :
      value = fields->f_dsp_56_s8;
      break;
    case M32C_OPERAND_IMM_64_HI :
      value = fields->f_dsp_64_s16;
      break;
    case M32C_OPERAND_IMM_8_HI :
      value = fields->f_dsp_8_s16;
      break;
    case M32C_OPERAND_IMM_8_QI :
      value = fields->f_dsp_8_s8;
      break;
    case M32C_OPERAND_IMM_8_S4 :
      value = fields->f_imm_8_s4;
      break;
    case M32C_OPERAND_IMM_8_S4N :
      value = fields->f_imm_8_s4;
      break;
    case M32C_OPERAND_IMM_SH_12_S4 :
      value = fields->f_imm_12_s4;
      break;
    case M32C_OPERAND_IMM_SH_20_S4 :
      value = fields->f_imm_20_s4;
      break;
    case M32C_OPERAND_IMM_SH_8_S4 :
      value = fields->f_imm_8_s4;
      break;
    case M32C_OPERAND_IMM1_S :
      value = fields->f_imm1_S;
      break;
    case M32C_OPERAND_IMM3_S :
      value = fields->f_imm3_S;
      break;
    case M32C_OPERAND_LAB_16_8 :
      value = fields->f_lab_16_8;
      break;
    case M32C_OPERAND_LAB_24_8 :
      value = fields->f_lab_24_8;
      break;
    case M32C_OPERAND_LAB_32_8 :
      value = fields->f_lab_32_8;
      break;
    case M32C_OPERAND_LAB_40_8 :
      value = fields->f_lab_40_8;
      break;
    case M32C_OPERAND_LAB_5_3 :
      value = fields->f_lab_5_3;
      break;
    case M32C_OPERAND_LAB_8_16 :
      value = fields->f_lab_8_16;
      break;
    case M32C_OPERAND_LAB_8_24 :
      value = fields->f_lab_8_24;
      break;
    case M32C_OPERAND_LAB_8_8 :
      value = fields->f_lab_8_8;
      break;
    case M32C_OPERAND_LAB32_JMP_S :
      value = fields->f_lab32_jmp_s;
      break;
    case M32C_OPERAND_Q :
      value = 0;
      break;
    case M32C_OPERAND_R0 :
      value = 0;
      break;
    case M32C_OPERAND_R0H :
      value = 0;
      break;
    case M32C_OPERAND_R0L :
      value = 0;
      break;
    case M32C_OPERAND_R1 :
      value = 0;
      break;
    case M32C_OPERAND_R1R2R0 :
      value = 0;
      break;
    case M32C_OPERAND_R2 :
      value = 0;
      break;
    case M32C_OPERAND_R2R0 :
      value = 0;
      break;
    case M32C_OPERAND_R3 :
      value = 0;
      break;
    case M32C_OPERAND_R3R1 :
      value = 0;
      break;
    case M32C_OPERAND_REGSETPOP :
      value = fields->f_8_8;
      break;
    case M32C_OPERAND_REGSETPUSH :
      value = fields->f_8_8;
      break;
    case M32C_OPERAND_RN16_PUSH_S :
      value = fields->f_4_1;
      break;
    case M32C_OPERAND_S :
      value = 0;
      break;
    case M32C_OPERAND_SRC16AN :
      value = fields->f_src16_an;
      break;
    case M32C_OPERAND_SRC16ANHI :
      value = fields->f_src16_an;
      break;
    case M32C_OPERAND_SRC16ANQI :
      value = fields->f_src16_an;
      break;
    case M32C_OPERAND_SRC16RNHI :
      value = fields->f_src16_rn;
      break;
    case M32C_OPERAND_SRC16RNQI :
      value = fields->f_src16_rn;
      break;
    case M32C_OPERAND_SRC32ANPREFIXED :
      value = fields->f_src32_an_prefixed;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDHI :
      value = fields->f_src32_an_prefixed;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDQI :
      value = fields->f_src32_an_prefixed;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDSI :
      value = fields->f_src32_an_prefixed;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXED :
      value = fields->f_src32_an_unprefixed;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDHI :
      value = fields->f_src32_an_unprefixed;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDQI :
      value = fields->f_src32_an_unprefixed;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDSI :
      value = fields->f_src32_an_unprefixed;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDHI :
      value = fields->f_src32_rn_prefixed_HI;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDQI :
      value = fields->f_src32_rn_prefixed_QI;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDSI :
      value = fields->f_src32_rn_prefixed_SI;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDHI :
      value = fields->f_src32_rn_unprefixed_HI;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDQI :
      value = fields->f_src32_rn_unprefixed_QI;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDSI :
      value = fields->f_src32_rn_unprefixed_SI;
      break;
    case M32C_OPERAND_SRCDST16_R0L_R0H_S_NORMAL :
      value = fields->f_5_1;
      break;
    case M32C_OPERAND_X :
      value = 0;
      break;
    case M32C_OPERAND_Z :
      value = 0;
      break;
    case M32C_OPERAND_COND16_16 :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_COND16_24 :
      value = fields->f_dsp_24_u8;
      break;
    case M32C_OPERAND_COND16_32 :
      value = fields->f_dsp_32_u8;
      break;
    case M32C_OPERAND_COND16C :
      value = fields->f_cond16;
      break;
    case M32C_OPERAND_COND16J :
      value = fields->f_cond16;
      break;
    case M32C_OPERAND_COND16J5 :
      value = fields->f_cond16j_5;
      break;
    case M32C_OPERAND_COND32 :
      value = fields->f_cond32;
      break;
    case M32C_OPERAND_COND32_16 :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_COND32_24 :
      value = fields->f_dsp_24_u8;
      break;
    case M32C_OPERAND_COND32_32 :
      value = fields->f_dsp_32_u8;
      break;
    case M32C_OPERAND_COND32_40 :
      value = fields->f_dsp_40_u8;
      break;
    case M32C_OPERAND_COND32J :
      value = fields->f_cond32j;
      break;
    case M32C_OPERAND_CR1_PREFIXED_32 :
      value = fields->f_21_3;
      break;
    case M32C_OPERAND_CR1_UNPREFIXED_32 :
      value = fields->f_13_3;
      break;
    case M32C_OPERAND_CR16 :
      value = fields->f_9_3;
      break;
    case M32C_OPERAND_CR2_32 :
      value = fields->f_13_3;
      break;
    case M32C_OPERAND_CR3_PREFIXED_32 :
      value = fields->f_21_3;
      break;
    case M32C_OPERAND_CR3_UNPREFIXED_32 :
      value = fields->f_13_3;
      break;
    case M32C_OPERAND_FLAGS16 :
      value = fields->f_9_3;
      break;
    case M32C_OPERAND_FLAGS32 :
      value = fields->f_13_3;
      break;
    case M32C_OPERAND_SCCOND32 :
      value = fields->f_cond16;
      break;
    case M32C_OPERAND_SIZE :
      value = 0;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while getting int operand.\n"),
		       opindex);
      abort ();
  }

  return value;
}

bfd_vma
m32c_cgen_get_vma_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     const CGEN_FIELDS * fields)
{
  bfd_vma value;

  switch (opindex)
    {
    case M32C_OPERAND_A0 :
      value = 0;
      break;
    case M32C_OPERAND_A1 :
      value = 0;
      break;
    case M32C_OPERAND_AN16_PUSH_S :
      value = fields->f_4_1;
      break;
    case M32C_OPERAND_BIT16AN :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_BIT16RN :
      value = fields->f_dst16_rn;
      break;
    case M32C_OPERAND_BIT3_S :
      value = fields->f_imm3_S;
      break;
    case M32C_OPERAND_BIT32ANPREFIXED :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_BIT32ANUNPREFIXED :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_BIT32RNPREFIXED :
      value = fields->f_dst32_rn_prefixed_QI;
      break;
    case M32C_OPERAND_BIT32RNUNPREFIXED :
      value = fields->f_dst32_rn_unprefixed_QI;
      break;
    case M32C_OPERAND_BITBASE16_16_S8 :
      value = fields->f_dsp_16_s8;
      break;
    case M32C_OPERAND_BITBASE16_16_U16 :
      value = fields->f_dsp_16_u16;
      break;
    case M32C_OPERAND_BITBASE16_16_U8 :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_BITBASE16_8_U11_S :
      value = fields->f_bitbase16_u11_S;
      break;
    case M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED :
      value = fields->f_bitbase32_16_s11_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED :
      value = fields->f_bitbase32_16_s19_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED :
      value = fields->f_bitbase32_16_u11_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED :
      value = fields->f_bitbase32_16_u19_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED :
      value = fields->f_bitbase32_16_u27_unprefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_S11_PREFIXED :
      value = fields->f_bitbase32_24_s11_prefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_S19_PREFIXED :
      value = fields->f_bitbase32_24_s19_prefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_U11_PREFIXED :
      value = fields->f_bitbase32_24_u11_prefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_U19_PREFIXED :
      value = fields->f_bitbase32_24_u19_prefixed;
      break;
    case M32C_OPERAND_BITBASE32_24_U27_PREFIXED :
      value = fields->f_bitbase32_24_u27_prefixed;
      break;
    case M32C_OPERAND_BITNO16R :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_BITNO32PREFIXED :
      value = fields->f_bitno32_prefixed;
      break;
    case M32C_OPERAND_BITNO32UNPREFIXED :
      value = fields->f_bitno32_unprefixed;
      break;
    case M32C_OPERAND_DSP_10_U6 :
      value = fields->f_dsp_10_u6;
      break;
    case M32C_OPERAND_DSP_16_S16 :
      value = fields->f_dsp_16_s16;
      break;
    case M32C_OPERAND_DSP_16_S8 :
      value = fields->f_dsp_16_s8;
      break;
    case M32C_OPERAND_DSP_16_U16 :
      value = fields->f_dsp_16_u16;
      break;
    case M32C_OPERAND_DSP_16_U20 :
      value = fields->f_dsp_16_u24;
      break;
    case M32C_OPERAND_DSP_16_U24 :
      value = fields->f_dsp_16_u24;
      break;
    case M32C_OPERAND_DSP_16_U8 :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_DSP_24_S16 :
      value = fields->f_dsp_24_s16;
      break;
    case M32C_OPERAND_DSP_24_S8 :
      value = fields->f_dsp_24_s8;
      break;
    case M32C_OPERAND_DSP_24_U16 :
      value = fields->f_dsp_24_u16;
      break;
    case M32C_OPERAND_DSP_24_U20 :
      value = fields->f_dsp_24_u24;
      break;
    case M32C_OPERAND_DSP_24_U24 :
      value = fields->f_dsp_24_u24;
      break;
    case M32C_OPERAND_DSP_24_U8 :
      value = fields->f_dsp_24_u8;
      break;
    case M32C_OPERAND_DSP_32_S16 :
      value = fields->f_dsp_32_s16;
      break;
    case M32C_OPERAND_DSP_32_S8 :
      value = fields->f_dsp_32_s8;
      break;
    case M32C_OPERAND_DSP_32_U16 :
      value = fields->f_dsp_32_u16;
      break;
    case M32C_OPERAND_DSP_32_U20 :
      value = fields->f_dsp_32_u24;
      break;
    case M32C_OPERAND_DSP_32_U24 :
      value = fields->f_dsp_32_u24;
      break;
    case M32C_OPERAND_DSP_32_U8 :
      value = fields->f_dsp_32_u8;
      break;
    case M32C_OPERAND_DSP_40_S16 :
      value = fields->f_dsp_40_s16;
      break;
    case M32C_OPERAND_DSP_40_S8 :
      value = fields->f_dsp_40_s8;
      break;
    case M32C_OPERAND_DSP_40_U16 :
      value = fields->f_dsp_40_u16;
      break;
    case M32C_OPERAND_DSP_40_U24 :
      value = fields->f_dsp_40_u24;
      break;
    case M32C_OPERAND_DSP_40_U8 :
      value = fields->f_dsp_40_u8;
      break;
    case M32C_OPERAND_DSP_48_S16 :
      value = fields->f_dsp_48_s16;
      break;
    case M32C_OPERAND_DSP_48_S8 :
      value = fields->f_dsp_48_s8;
      break;
    case M32C_OPERAND_DSP_48_U16 :
      value = fields->f_dsp_48_u16;
      break;
    case M32C_OPERAND_DSP_48_U24 :
      value = fields->f_dsp_48_u24;
      break;
    case M32C_OPERAND_DSP_48_U8 :
      value = fields->f_dsp_48_u8;
      break;
    case M32C_OPERAND_DSP_8_S24 :
      value = fields->f_dsp_8_s24;
      break;
    case M32C_OPERAND_DSP_8_S8 :
      value = fields->f_dsp_8_s8;
      break;
    case M32C_OPERAND_DSP_8_U16 :
      value = fields->f_dsp_8_u16;
      break;
    case M32C_OPERAND_DSP_8_U24 :
      value = fields->f_dsp_8_u24;
      break;
    case M32C_OPERAND_DSP_8_U6 :
      value = fields->f_dsp_8_u6;
      break;
    case M32C_OPERAND_DSP_8_U8 :
      value = fields->f_dsp_8_u8;
      break;
    case M32C_OPERAND_DST16AN :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_DST16AN_S :
      value = fields->f_dst16_an_s;
      break;
    case M32C_OPERAND_DST16ANHI :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_DST16ANQI :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_DST16ANQI_S :
      value = fields->f_dst16_rn_QI_s;
      break;
    case M32C_OPERAND_DST16ANSI :
      value = fields->f_dst16_an;
      break;
    case M32C_OPERAND_DST16RNEXTQI :
      value = fields->f_dst16_rn_ext;
      break;
    case M32C_OPERAND_DST16RNHI :
      value = fields->f_dst16_rn;
      break;
    case M32C_OPERAND_DST16RNQI :
      value = fields->f_dst16_rn;
      break;
    case M32C_OPERAND_DST16RNQI_S :
      value = fields->f_dst16_rn_QI_s;
      break;
    case M32C_OPERAND_DST16RNSI :
      value = fields->f_dst16_rn;
      break;
    case M32C_OPERAND_DST32ANEXTUNPREFIXED :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32ANPREFIXED :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDHI :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDQI :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDSI :
      value = fields->f_dst32_an_prefixed;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXED :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDHI :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDQI :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDSI :
      value = fields->f_dst32_an_unprefixed;
      break;
    case M32C_OPERAND_DST32R0HI_S :
      value = 0;
      break;
    case M32C_OPERAND_DST32R0QI_S :
      value = 0;
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDHI :
      value = fields->f_dst32_rn_ext_unprefixed;
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDQI :
      value = fields->f_dst32_rn_ext_unprefixed;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDHI :
      value = fields->f_dst32_rn_prefixed_HI;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDQI :
      value = fields->f_dst32_rn_prefixed_QI;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDSI :
      value = fields->f_dst32_rn_prefixed_SI;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDHI :
      value = fields->f_dst32_rn_unprefixed_HI;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDQI :
      value = fields->f_dst32_rn_unprefixed_QI;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDSI :
      value = fields->f_dst32_rn_unprefixed_SI;
      break;
    case M32C_OPERAND_G :
      value = 0;
      break;
    case M32C_OPERAND_IMM_12_S4 :
      value = fields->f_imm_12_s4;
      break;
    case M32C_OPERAND_IMM_12_S4N :
      value = fields->f_imm_12_s4;
      break;
    case M32C_OPERAND_IMM_13_U3 :
      value = fields->f_imm_13_u3;
      break;
    case M32C_OPERAND_IMM_16_HI :
      value = fields->f_dsp_16_s16;
      break;
    case M32C_OPERAND_IMM_16_QI :
      value = fields->f_dsp_16_s8;
      break;
    case M32C_OPERAND_IMM_16_SI :
      value = fields->f_dsp_16_s32;
      break;
    case M32C_OPERAND_IMM_20_S4 :
      value = fields->f_imm_20_s4;
      break;
    case M32C_OPERAND_IMM_24_HI :
      value = fields->f_dsp_24_s16;
      break;
    case M32C_OPERAND_IMM_24_QI :
      value = fields->f_dsp_24_s8;
      break;
    case M32C_OPERAND_IMM_24_SI :
      value = fields->f_dsp_24_s32;
      break;
    case M32C_OPERAND_IMM_32_HI :
      value = fields->f_dsp_32_s16;
      break;
    case M32C_OPERAND_IMM_32_QI :
      value = fields->f_dsp_32_s8;
      break;
    case M32C_OPERAND_IMM_32_SI :
      value = fields->f_dsp_32_s32;
      break;
    case M32C_OPERAND_IMM_40_HI :
      value = fields->f_dsp_40_s16;
      break;
    case M32C_OPERAND_IMM_40_QI :
      value = fields->f_dsp_40_s8;
      break;
    case M32C_OPERAND_IMM_40_SI :
      value = fields->f_dsp_40_s32;
      break;
    case M32C_OPERAND_IMM_48_HI :
      value = fields->f_dsp_48_s16;
      break;
    case M32C_OPERAND_IMM_48_QI :
      value = fields->f_dsp_48_s8;
      break;
    case M32C_OPERAND_IMM_48_SI :
      value = fields->f_dsp_48_s32;
      break;
    case M32C_OPERAND_IMM_56_HI :
      value = fields->f_dsp_56_s16;
      break;
    case M32C_OPERAND_IMM_56_QI :
      value = fields->f_dsp_56_s8;
      break;
    case M32C_OPERAND_IMM_64_HI :
      value = fields->f_dsp_64_s16;
      break;
    case M32C_OPERAND_IMM_8_HI :
      value = fields->f_dsp_8_s16;
      break;
    case M32C_OPERAND_IMM_8_QI :
      value = fields->f_dsp_8_s8;
      break;
    case M32C_OPERAND_IMM_8_S4 :
      value = fields->f_imm_8_s4;
      break;
    case M32C_OPERAND_IMM_8_S4N :
      value = fields->f_imm_8_s4;
      break;
    case M32C_OPERAND_IMM_SH_12_S4 :
      value = fields->f_imm_12_s4;
      break;
    case M32C_OPERAND_IMM_SH_20_S4 :
      value = fields->f_imm_20_s4;
      break;
    case M32C_OPERAND_IMM_SH_8_S4 :
      value = fields->f_imm_8_s4;
      break;
    case M32C_OPERAND_IMM1_S :
      value = fields->f_imm1_S;
      break;
    case M32C_OPERAND_IMM3_S :
      value = fields->f_imm3_S;
      break;
    case M32C_OPERAND_LAB_16_8 :
      value = fields->f_lab_16_8;
      break;
    case M32C_OPERAND_LAB_24_8 :
      value = fields->f_lab_24_8;
      break;
    case M32C_OPERAND_LAB_32_8 :
      value = fields->f_lab_32_8;
      break;
    case M32C_OPERAND_LAB_40_8 :
      value = fields->f_lab_40_8;
      break;
    case M32C_OPERAND_LAB_5_3 :
      value = fields->f_lab_5_3;
      break;
    case M32C_OPERAND_LAB_8_16 :
      value = fields->f_lab_8_16;
      break;
    case M32C_OPERAND_LAB_8_24 :
      value = fields->f_lab_8_24;
      break;
    case M32C_OPERAND_LAB_8_8 :
      value = fields->f_lab_8_8;
      break;
    case M32C_OPERAND_LAB32_JMP_S :
      value = fields->f_lab32_jmp_s;
      break;
    case M32C_OPERAND_Q :
      value = 0;
      break;
    case M32C_OPERAND_R0 :
      value = 0;
      break;
    case M32C_OPERAND_R0H :
      value = 0;
      break;
    case M32C_OPERAND_R0L :
      value = 0;
      break;
    case M32C_OPERAND_R1 :
      value = 0;
      break;
    case M32C_OPERAND_R1R2R0 :
      value = 0;
      break;
    case M32C_OPERAND_R2 :
      value = 0;
      break;
    case M32C_OPERAND_R2R0 :
      value = 0;
      break;
    case M32C_OPERAND_R3 :
      value = 0;
      break;
    case M32C_OPERAND_R3R1 :
      value = 0;
      break;
    case M32C_OPERAND_REGSETPOP :
      value = fields->f_8_8;
      break;
    case M32C_OPERAND_REGSETPUSH :
      value = fields->f_8_8;
      break;
    case M32C_OPERAND_RN16_PUSH_S :
      value = fields->f_4_1;
      break;
    case M32C_OPERAND_S :
      value = 0;
      break;
    case M32C_OPERAND_SRC16AN :
      value = fields->f_src16_an;
      break;
    case M32C_OPERAND_SRC16ANHI :
      value = fields->f_src16_an;
      break;
    case M32C_OPERAND_SRC16ANQI :
      value = fields->f_src16_an;
      break;
    case M32C_OPERAND_SRC16RNHI :
      value = fields->f_src16_rn;
      break;
    case M32C_OPERAND_SRC16RNQI :
      value = fields->f_src16_rn;
      break;
    case M32C_OPERAND_SRC32ANPREFIXED :
      value = fields->f_src32_an_prefixed;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDHI :
      value = fields->f_src32_an_prefixed;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDQI :
      value = fields->f_src32_an_prefixed;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDSI :
      value = fields->f_src32_an_prefixed;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXED :
      value = fields->f_src32_an_unprefixed;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDHI :
      value = fields->f_src32_an_unprefixed;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDQI :
      value = fields->f_src32_an_unprefixed;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDSI :
      value = fields->f_src32_an_unprefixed;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDHI :
      value = fields->f_src32_rn_prefixed_HI;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDQI :
      value = fields->f_src32_rn_prefixed_QI;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDSI :
      value = fields->f_src32_rn_prefixed_SI;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDHI :
      value = fields->f_src32_rn_unprefixed_HI;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDQI :
      value = fields->f_src32_rn_unprefixed_QI;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDSI :
      value = fields->f_src32_rn_unprefixed_SI;
      break;
    case M32C_OPERAND_SRCDST16_R0L_R0H_S_NORMAL :
      value = fields->f_5_1;
      break;
    case M32C_OPERAND_X :
      value = 0;
      break;
    case M32C_OPERAND_Z :
      value = 0;
      break;
    case M32C_OPERAND_COND16_16 :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_COND16_24 :
      value = fields->f_dsp_24_u8;
      break;
    case M32C_OPERAND_COND16_32 :
      value = fields->f_dsp_32_u8;
      break;
    case M32C_OPERAND_COND16C :
      value = fields->f_cond16;
      break;
    case M32C_OPERAND_COND16J :
      value = fields->f_cond16;
      break;
    case M32C_OPERAND_COND16J5 :
      value = fields->f_cond16j_5;
      break;
    case M32C_OPERAND_COND32 :
      value = fields->f_cond32;
      break;
    case M32C_OPERAND_COND32_16 :
      value = fields->f_dsp_16_u8;
      break;
    case M32C_OPERAND_COND32_24 :
      value = fields->f_dsp_24_u8;
      break;
    case M32C_OPERAND_COND32_32 :
      value = fields->f_dsp_32_u8;
      break;
    case M32C_OPERAND_COND32_40 :
      value = fields->f_dsp_40_u8;
      break;
    case M32C_OPERAND_COND32J :
      value = fields->f_cond32j;
      break;
    case M32C_OPERAND_CR1_PREFIXED_32 :
      value = fields->f_21_3;
      break;
    case M32C_OPERAND_CR1_UNPREFIXED_32 :
      value = fields->f_13_3;
      break;
    case M32C_OPERAND_CR16 :
      value = fields->f_9_3;
      break;
    case M32C_OPERAND_CR2_32 :
      value = fields->f_13_3;
      break;
    case M32C_OPERAND_CR3_PREFIXED_32 :
      value = fields->f_21_3;
      break;
    case M32C_OPERAND_CR3_UNPREFIXED_32 :
      value = fields->f_13_3;
      break;
    case M32C_OPERAND_FLAGS16 :
      value = fields->f_9_3;
      break;
    case M32C_OPERAND_FLAGS32 :
      value = fields->f_13_3;
      break;
    case M32C_OPERAND_SCCOND32 :
      value = fields->f_cond16;
      break;
    case M32C_OPERAND_SIZE :
      value = 0;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while getting vma operand.\n"),
		       opindex);
      abort ();
  }

  return value;
}

void m32c_cgen_set_int_operand  (CGEN_CPU_DESC, int, CGEN_FIELDS *, int);
void m32c_cgen_set_vma_operand  (CGEN_CPU_DESC, int, CGEN_FIELDS *, bfd_vma);

/* Stuffing values in cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they accept.
   TODO: floating point, inlining support, remove cases where argument type
   not appropriate.  */

void
m32c_cgen_set_int_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     CGEN_FIELDS * fields,
			     int value)
{
  switch (opindex)
    {
    case M32C_OPERAND_A0 :
      break;
    case M32C_OPERAND_A1 :
      break;
    case M32C_OPERAND_AN16_PUSH_S :
      fields->f_4_1 = value;
      break;
    case M32C_OPERAND_BIT16AN :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_BIT16RN :
      fields->f_dst16_rn = value;
      break;
    case M32C_OPERAND_BIT3_S :
      fields->f_imm3_S = value;
      break;
    case M32C_OPERAND_BIT32ANPREFIXED :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_BIT32ANUNPREFIXED :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_BIT32RNPREFIXED :
      fields->f_dst32_rn_prefixed_QI = value;
      break;
    case M32C_OPERAND_BIT32RNUNPREFIXED :
      fields->f_dst32_rn_unprefixed_QI = value;
      break;
    case M32C_OPERAND_BITBASE16_16_S8 :
      fields->f_dsp_16_s8 = value;
      break;
    case M32C_OPERAND_BITBASE16_16_U16 :
      fields->f_dsp_16_u16 = value;
      break;
    case M32C_OPERAND_BITBASE16_16_U8 :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_BITBASE16_8_U11_S :
      fields->f_bitbase16_u11_S = value;
      break;
    case M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED :
      fields->f_bitbase32_16_s11_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED :
      fields->f_bitbase32_16_s19_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED :
      fields->f_bitbase32_16_u11_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED :
      fields->f_bitbase32_16_u19_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED :
      fields->f_bitbase32_16_u27_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_S11_PREFIXED :
      fields->f_bitbase32_24_s11_prefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_S19_PREFIXED :
      fields->f_bitbase32_24_s19_prefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_U11_PREFIXED :
      fields->f_bitbase32_24_u11_prefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_U19_PREFIXED :
      fields->f_bitbase32_24_u19_prefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_U27_PREFIXED :
      fields->f_bitbase32_24_u27_prefixed = value;
      break;
    case M32C_OPERAND_BITNO16R :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_BITNO32PREFIXED :
      fields->f_bitno32_prefixed = value;
      break;
    case M32C_OPERAND_BITNO32UNPREFIXED :
      fields->f_bitno32_unprefixed = value;
      break;
    case M32C_OPERAND_DSP_10_U6 :
      fields->f_dsp_10_u6 = value;
      break;
    case M32C_OPERAND_DSP_16_S16 :
      fields->f_dsp_16_s16 = value;
      break;
    case M32C_OPERAND_DSP_16_S8 :
      fields->f_dsp_16_s8 = value;
      break;
    case M32C_OPERAND_DSP_16_U16 :
      fields->f_dsp_16_u16 = value;
      break;
    case M32C_OPERAND_DSP_16_U20 :
      fields->f_dsp_16_u24 = value;
      break;
    case M32C_OPERAND_DSP_16_U24 :
      fields->f_dsp_16_u24 = value;
      break;
    case M32C_OPERAND_DSP_16_U8 :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_DSP_24_S16 :
      fields->f_dsp_24_s16 = value;
      break;
    case M32C_OPERAND_DSP_24_S8 :
      fields->f_dsp_24_s8 = value;
      break;
    case M32C_OPERAND_DSP_24_U16 :
      fields->f_dsp_24_u16 = value;
      break;
    case M32C_OPERAND_DSP_24_U20 :
      fields->f_dsp_24_u24 = value;
      break;
    case M32C_OPERAND_DSP_24_U24 :
      fields->f_dsp_24_u24 = value;
      break;
    case M32C_OPERAND_DSP_24_U8 :
      fields->f_dsp_24_u8 = value;
      break;
    case M32C_OPERAND_DSP_32_S16 :
      fields->f_dsp_32_s16 = value;
      break;
    case M32C_OPERAND_DSP_32_S8 :
      fields->f_dsp_32_s8 = value;
      break;
    case M32C_OPERAND_DSP_32_U16 :
      fields->f_dsp_32_u16 = value;
      break;
    case M32C_OPERAND_DSP_32_U20 :
      fields->f_dsp_32_u24 = value;
      break;
    case M32C_OPERAND_DSP_32_U24 :
      fields->f_dsp_32_u24 = value;
      break;
    case M32C_OPERAND_DSP_32_U8 :
      fields->f_dsp_32_u8 = value;
      break;
    case M32C_OPERAND_DSP_40_S16 :
      fields->f_dsp_40_s16 = value;
      break;
    case M32C_OPERAND_DSP_40_S8 :
      fields->f_dsp_40_s8 = value;
      break;
    case M32C_OPERAND_DSP_40_U16 :
      fields->f_dsp_40_u16 = value;
      break;
    case M32C_OPERAND_DSP_40_U24 :
      fields->f_dsp_40_u24 = value;
      break;
    case M32C_OPERAND_DSP_40_U8 :
      fields->f_dsp_40_u8 = value;
      break;
    case M32C_OPERAND_DSP_48_S16 :
      fields->f_dsp_48_s16 = value;
      break;
    case M32C_OPERAND_DSP_48_S8 :
      fields->f_dsp_48_s8 = value;
      break;
    case M32C_OPERAND_DSP_48_U16 :
      fields->f_dsp_48_u16 = value;
      break;
    case M32C_OPERAND_DSP_48_U24 :
      fields->f_dsp_48_u24 = value;
      break;
    case M32C_OPERAND_DSP_48_U8 :
      fields->f_dsp_48_u8 = value;
      break;
    case M32C_OPERAND_DSP_8_S24 :
      fields->f_dsp_8_s24 = value;
      break;
    case M32C_OPERAND_DSP_8_S8 :
      fields->f_dsp_8_s8 = value;
      break;
    case M32C_OPERAND_DSP_8_U16 :
      fields->f_dsp_8_u16 = value;
      break;
    case M32C_OPERAND_DSP_8_U24 :
      fields->f_dsp_8_u24 = value;
      break;
    case M32C_OPERAND_DSP_8_U6 :
      fields->f_dsp_8_u6 = value;
      break;
    case M32C_OPERAND_DSP_8_U8 :
      fields->f_dsp_8_u8 = value;
      break;
    case M32C_OPERAND_DST16AN :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_DST16AN_S :
      fields->f_dst16_an_s = value;
      break;
    case M32C_OPERAND_DST16ANHI :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_DST16ANQI :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_DST16ANQI_S :
      fields->f_dst16_rn_QI_s = value;
      break;
    case M32C_OPERAND_DST16ANSI :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_DST16RNEXTQI :
      fields->f_dst16_rn_ext = value;
      break;
    case M32C_OPERAND_DST16RNHI :
      fields->f_dst16_rn = value;
      break;
    case M32C_OPERAND_DST16RNQI :
      fields->f_dst16_rn = value;
      break;
    case M32C_OPERAND_DST16RNQI_S :
      fields->f_dst16_rn_QI_s = value;
      break;
    case M32C_OPERAND_DST16RNSI :
      fields->f_dst16_rn = value;
      break;
    case M32C_OPERAND_DST32ANEXTUNPREFIXED :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32ANPREFIXED :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDHI :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDQI :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDSI :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXED :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDHI :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDQI :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDSI :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32R0HI_S :
      break;
    case M32C_OPERAND_DST32R0QI_S :
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDHI :
      fields->f_dst32_rn_ext_unprefixed = value;
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDQI :
      fields->f_dst32_rn_ext_unprefixed = value;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDHI :
      fields->f_dst32_rn_prefixed_HI = value;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDQI :
      fields->f_dst32_rn_prefixed_QI = value;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDSI :
      fields->f_dst32_rn_prefixed_SI = value;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDHI :
      fields->f_dst32_rn_unprefixed_HI = value;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDQI :
      fields->f_dst32_rn_unprefixed_QI = value;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDSI :
      fields->f_dst32_rn_unprefixed_SI = value;
      break;
    case M32C_OPERAND_G :
      break;
    case M32C_OPERAND_IMM_12_S4 :
      fields->f_imm_12_s4 = value;
      break;
    case M32C_OPERAND_IMM_12_S4N :
      fields->f_imm_12_s4 = value;
      break;
    case M32C_OPERAND_IMM_13_U3 :
      fields->f_imm_13_u3 = value;
      break;
    case M32C_OPERAND_IMM_16_HI :
      fields->f_dsp_16_s16 = value;
      break;
    case M32C_OPERAND_IMM_16_QI :
      fields->f_dsp_16_s8 = value;
      break;
    case M32C_OPERAND_IMM_16_SI :
      fields->f_dsp_16_s32 = value;
      break;
    case M32C_OPERAND_IMM_20_S4 :
      fields->f_imm_20_s4 = value;
      break;
    case M32C_OPERAND_IMM_24_HI :
      fields->f_dsp_24_s16 = value;
      break;
    case M32C_OPERAND_IMM_24_QI :
      fields->f_dsp_24_s8 = value;
      break;
    case M32C_OPERAND_IMM_24_SI :
      fields->f_dsp_24_s32 = value;
      break;
    case M32C_OPERAND_IMM_32_HI :
      fields->f_dsp_32_s16 = value;
      break;
    case M32C_OPERAND_IMM_32_QI :
      fields->f_dsp_32_s8 = value;
      break;
    case M32C_OPERAND_IMM_32_SI :
      fields->f_dsp_32_s32 = value;
      break;
    case M32C_OPERAND_IMM_40_HI :
      fields->f_dsp_40_s16 = value;
      break;
    case M32C_OPERAND_IMM_40_QI :
      fields->f_dsp_40_s8 = value;
      break;
    case M32C_OPERAND_IMM_40_SI :
      fields->f_dsp_40_s32 = value;
      break;
    case M32C_OPERAND_IMM_48_HI :
      fields->f_dsp_48_s16 = value;
      break;
    case M32C_OPERAND_IMM_48_QI :
      fields->f_dsp_48_s8 = value;
      break;
    case M32C_OPERAND_IMM_48_SI :
      fields->f_dsp_48_s32 = value;
      break;
    case M32C_OPERAND_IMM_56_HI :
      fields->f_dsp_56_s16 = value;
      break;
    case M32C_OPERAND_IMM_56_QI :
      fields->f_dsp_56_s8 = value;
      break;
    case M32C_OPERAND_IMM_64_HI :
      fields->f_dsp_64_s16 = value;
      break;
    case M32C_OPERAND_IMM_8_HI :
      fields->f_dsp_8_s16 = value;
      break;
    case M32C_OPERAND_IMM_8_QI :
      fields->f_dsp_8_s8 = value;
      break;
    case M32C_OPERAND_IMM_8_S4 :
      fields->f_imm_8_s4 = value;
      break;
    case M32C_OPERAND_IMM_8_S4N :
      fields->f_imm_8_s4 = value;
      break;
    case M32C_OPERAND_IMM_SH_12_S4 :
      fields->f_imm_12_s4 = value;
      break;
    case M32C_OPERAND_IMM_SH_20_S4 :
      fields->f_imm_20_s4 = value;
      break;
    case M32C_OPERAND_IMM_SH_8_S4 :
      fields->f_imm_8_s4 = value;
      break;
    case M32C_OPERAND_IMM1_S :
      fields->f_imm1_S = value;
      break;
    case M32C_OPERAND_IMM3_S :
      fields->f_imm3_S = value;
      break;
    case M32C_OPERAND_LAB_16_8 :
      fields->f_lab_16_8 = value;
      break;
    case M32C_OPERAND_LAB_24_8 :
      fields->f_lab_24_8 = value;
      break;
    case M32C_OPERAND_LAB_32_8 :
      fields->f_lab_32_8 = value;
      break;
    case M32C_OPERAND_LAB_40_8 :
      fields->f_lab_40_8 = value;
      break;
    case M32C_OPERAND_LAB_5_3 :
      fields->f_lab_5_3 = value;
      break;
    case M32C_OPERAND_LAB_8_16 :
      fields->f_lab_8_16 = value;
      break;
    case M32C_OPERAND_LAB_8_24 :
      fields->f_lab_8_24 = value;
      break;
    case M32C_OPERAND_LAB_8_8 :
      fields->f_lab_8_8 = value;
      break;
    case M32C_OPERAND_LAB32_JMP_S :
      fields->f_lab32_jmp_s = value;
      break;
    case M32C_OPERAND_Q :
      break;
    case M32C_OPERAND_R0 :
      break;
    case M32C_OPERAND_R0H :
      break;
    case M32C_OPERAND_R0L :
      break;
    case M32C_OPERAND_R1 :
      break;
    case M32C_OPERAND_R1R2R0 :
      break;
    case M32C_OPERAND_R2 :
      break;
    case M32C_OPERAND_R2R0 :
      break;
    case M32C_OPERAND_R3 :
      break;
    case M32C_OPERAND_R3R1 :
      break;
    case M32C_OPERAND_REGSETPOP :
      fields->f_8_8 = value;
      break;
    case M32C_OPERAND_REGSETPUSH :
      fields->f_8_8 = value;
      break;
    case M32C_OPERAND_RN16_PUSH_S :
      fields->f_4_1 = value;
      break;
    case M32C_OPERAND_S :
      break;
    case M32C_OPERAND_SRC16AN :
      fields->f_src16_an = value;
      break;
    case M32C_OPERAND_SRC16ANHI :
      fields->f_src16_an = value;
      break;
    case M32C_OPERAND_SRC16ANQI :
      fields->f_src16_an = value;
      break;
    case M32C_OPERAND_SRC16RNHI :
      fields->f_src16_rn = value;
      break;
    case M32C_OPERAND_SRC16RNQI :
      fields->f_src16_rn = value;
      break;
    case M32C_OPERAND_SRC32ANPREFIXED :
      fields->f_src32_an_prefixed = value;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDHI :
      fields->f_src32_an_prefixed = value;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDQI :
      fields->f_src32_an_prefixed = value;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDSI :
      fields->f_src32_an_prefixed = value;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXED :
      fields->f_src32_an_unprefixed = value;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDHI :
      fields->f_src32_an_unprefixed = value;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDQI :
      fields->f_src32_an_unprefixed = value;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDSI :
      fields->f_src32_an_unprefixed = value;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDHI :
      fields->f_src32_rn_prefixed_HI = value;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDQI :
      fields->f_src32_rn_prefixed_QI = value;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDSI :
      fields->f_src32_rn_prefixed_SI = value;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDHI :
      fields->f_src32_rn_unprefixed_HI = value;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDQI :
      fields->f_src32_rn_unprefixed_QI = value;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDSI :
      fields->f_src32_rn_unprefixed_SI = value;
      break;
    case M32C_OPERAND_SRCDST16_R0L_R0H_S_NORMAL :
      fields->f_5_1 = value;
      break;
    case M32C_OPERAND_X :
      break;
    case M32C_OPERAND_Z :
      break;
    case M32C_OPERAND_COND16_16 :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_COND16_24 :
      fields->f_dsp_24_u8 = value;
      break;
    case M32C_OPERAND_COND16_32 :
      fields->f_dsp_32_u8 = value;
      break;
    case M32C_OPERAND_COND16C :
      fields->f_cond16 = value;
      break;
    case M32C_OPERAND_COND16J :
      fields->f_cond16 = value;
      break;
    case M32C_OPERAND_COND16J5 :
      fields->f_cond16j_5 = value;
      break;
    case M32C_OPERAND_COND32 :
      fields->f_cond32 = value;
      break;
    case M32C_OPERAND_COND32_16 :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_COND32_24 :
      fields->f_dsp_24_u8 = value;
      break;
    case M32C_OPERAND_COND32_32 :
      fields->f_dsp_32_u8 = value;
      break;
    case M32C_OPERAND_COND32_40 :
      fields->f_dsp_40_u8 = value;
      break;
    case M32C_OPERAND_COND32J :
      fields->f_cond32j = value;
      break;
    case M32C_OPERAND_CR1_PREFIXED_32 :
      fields->f_21_3 = value;
      break;
    case M32C_OPERAND_CR1_UNPREFIXED_32 :
      fields->f_13_3 = value;
      break;
    case M32C_OPERAND_CR16 :
      fields->f_9_3 = value;
      break;
    case M32C_OPERAND_CR2_32 :
      fields->f_13_3 = value;
      break;
    case M32C_OPERAND_CR3_PREFIXED_32 :
      fields->f_21_3 = value;
      break;
    case M32C_OPERAND_CR3_UNPREFIXED_32 :
      fields->f_13_3 = value;
      break;
    case M32C_OPERAND_FLAGS16 :
      fields->f_9_3 = value;
      break;
    case M32C_OPERAND_FLAGS32 :
      fields->f_13_3 = value;
      break;
    case M32C_OPERAND_SCCOND32 :
      fields->f_cond16 = value;
      break;
    case M32C_OPERAND_SIZE :
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while setting int operand.\n"),
		       opindex);
      abort ();
  }
}

void
m32c_cgen_set_vma_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     CGEN_FIELDS * fields,
			     bfd_vma value)
{
  switch (opindex)
    {
    case M32C_OPERAND_A0 :
      break;
    case M32C_OPERAND_A1 :
      break;
    case M32C_OPERAND_AN16_PUSH_S :
      fields->f_4_1 = value;
      break;
    case M32C_OPERAND_BIT16AN :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_BIT16RN :
      fields->f_dst16_rn = value;
      break;
    case M32C_OPERAND_BIT3_S :
      fields->f_imm3_S = value;
      break;
    case M32C_OPERAND_BIT32ANPREFIXED :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_BIT32ANUNPREFIXED :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_BIT32RNPREFIXED :
      fields->f_dst32_rn_prefixed_QI = value;
      break;
    case M32C_OPERAND_BIT32RNUNPREFIXED :
      fields->f_dst32_rn_unprefixed_QI = value;
      break;
    case M32C_OPERAND_BITBASE16_16_S8 :
      fields->f_dsp_16_s8 = value;
      break;
    case M32C_OPERAND_BITBASE16_16_U16 :
      fields->f_dsp_16_u16 = value;
      break;
    case M32C_OPERAND_BITBASE16_16_U8 :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_BITBASE16_8_U11_S :
      fields->f_bitbase16_u11_S = value;
      break;
    case M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED :
      fields->f_bitbase32_16_s11_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED :
      fields->f_bitbase32_16_s19_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED :
      fields->f_bitbase32_16_u11_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED :
      fields->f_bitbase32_16_u19_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED :
      fields->f_bitbase32_16_u27_unprefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_S11_PREFIXED :
      fields->f_bitbase32_24_s11_prefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_S19_PREFIXED :
      fields->f_bitbase32_24_s19_prefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_U11_PREFIXED :
      fields->f_bitbase32_24_u11_prefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_U19_PREFIXED :
      fields->f_bitbase32_24_u19_prefixed = value;
      break;
    case M32C_OPERAND_BITBASE32_24_U27_PREFIXED :
      fields->f_bitbase32_24_u27_prefixed = value;
      break;
    case M32C_OPERAND_BITNO16R :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_BITNO32PREFIXED :
      fields->f_bitno32_prefixed = value;
      break;
    case M32C_OPERAND_BITNO32UNPREFIXED :
      fields->f_bitno32_unprefixed = value;
      break;
    case M32C_OPERAND_DSP_10_U6 :
      fields->f_dsp_10_u6 = value;
      break;
    case M32C_OPERAND_DSP_16_S16 :
      fields->f_dsp_16_s16 = value;
      break;
    case M32C_OPERAND_DSP_16_S8 :
      fields->f_dsp_16_s8 = value;
      break;
    case M32C_OPERAND_DSP_16_U16 :
      fields->f_dsp_16_u16 = value;
      break;
    case M32C_OPERAND_DSP_16_U20 :
      fields->f_dsp_16_u24 = value;
      break;
    case M32C_OPERAND_DSP_16_U24 :
      fields->f_dsp_16_u24 = value;
      break;
    case M32C_OPERAND_DSP_16_U8 :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_DSP_24_S16 :
      fields->f_dsp_24_s16 = value;
      break;
    case M32C_OPERAND_DSP_24_S8 :
      fields->f_dsp_24_s8 = value;
      break;
    case M32C_OPERAND_DSP_24_U16 :
      fields->f_dsp_24_u16 = value;
      break;
    case M32C_OPERAND_DSP_24_U20 :
      fields->f_dsp_24_u24 = value;
      break;
    case M32C_OPERAND_DSP_24_U24 :
      fields->f_dsp_24_u24 = value;
      break;
    case M32C_OPERAND_DSP_24_U8 :
      fields->f_dsp_24_u8 = value;
      break;
    case M32C_OPERAND_DSP_32_S16 :
      fields->f_dsp_32_s16 = value;
      break;
    case M32C_OPERAND_DSP_32_S8 :
      fields->f_dsp_32_s8 = value;
      break;
    case M32C_OPERAND_DSP_32_U16 :
      fields->f_dsp_32_u16 = value;
      break;
    case M32C_OPERAND_DSP_32_U20 :
      fields->f_dsp_32_u24 = value;
      break;
    case M32C_OPERAND_DSP_32_U24 :
      fields->f_dsp_32_u24 = value;
      break;
    case M32C_OPERAND_DSP_32_U8 :
      fields->f_dsp_32_u8 = value;
      break;
    case M32C_OPERAND_DSP_40_S16 :
      fields->f_dsp_40_s16 = value;
      break;
    case M32C_OPERAND_DSP_40_S8 :
      fields->f_dsp_40_s8 = value;
      break;
    case M32C_OPERAND_DSP_40_U16 :
      fields->f_dsp_40_u16 = value;
      break;
    case M32C_OPERAND_DSP_40_U24 :
      fields->f_dsp_40_u24 = value;
      break;
    case M32C_OPERAND_DSP_40_U8 :
      fields->f_dsp_40_u8 = value;
      break;
    case M32C_OPERAND_DSP_48_S16 :
      fields->f_dsp_48_s16 = value;
      break;
    case M32C_OPERAND_DSP_48_S8 :
      fields->f_dsp_48_s8 = value;
      break;
    case M32C_OPERAND_DSP_48_U16 :
      fields->f_dsp_48_u16 = value;
      break;
    case M32C_OPERAND_DSP_48_U24 :
      fields->f_dsp_48_u24 = value;
      break;
    case M32C_OPERAND_DSP_48_U8 :
      fields->f_dsp_48_u8 = value;
      break;
    case M32C_OPERAND_DSP_8_S24 :
      fields->f_dsp_8_s24 = value;
      break;
    case M32C_OPERAND_DSP_8_S8 :
      fields->f_dsp_8_s8 = value;
      break;
    case M32C_OPERAND_DSP_8_U16 :
      fields->f_dsp_8_u16 = value;
      break;
    case M32C_OPERAND_DSP_8_U24 :
      fields->f_dsp_8_u24 = value;
      break;
    case M32C_OPERAND_DSP_8_U6 :
      fields->f_dsp_8_u6 = value;
      break;
    case M32C_OPERAND_DSP_8_U8 :
      fields->f_dsp_8_u8 = value;
      break;
    case M32C_OPERAND_DST16AN :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_DST16AN_S :
      fields->f_dst16_an_s = value;
      break;
    case M32C_OPERAND_DST16ANHI :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_DST16ANQI :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_DST16ANQI_S :
      fields->f_dst16_rn_QI_s = value;
      break;
    case M32C_OPERAND_DST16ANSI :
      fields->f_dst16_an = value;
      break;
    case M32C_OPERAND_DST16RNEXTQI :
      fields->f_dst16_rn_ext = value;
      break;
    case M32C_OPERAND_DST16RNHI :
      fields->f_dst16_rn = value;
      break;
    case M32C_OPERAND_DST16RNQI :
      fields->f_dst16_rn = value;
      break;
    case M32C_OPERAND_DST16RNQI_S :
      fields->f_dst16_rn_QI_s = value;
      break;
    case M32C_OPERAND_DST16RNSI :
      fields->f_dst16_rn = value;
      break;
    case M32C_OPERAND_DST32ANEXTUNPREFIXED :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32ANPREFIXED :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDHI :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDQI :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_DST32ANPREFIXEDSI :
      fields->f_dst32_an_prefixed = value;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXED :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDHI :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDQI :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDSI :
      fields->f_dst32_an_unprefixed = value;
      break;
    case M32C_OPERAND_DST32R0HI_S :
      break;
    case M32C_OPERAND_DST32R0QI_S :
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDHI :
      fields->f_dst32_rn_ext_unprefixed = value;
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDQI :
      fields->f_dst32_rn_ext_unprefixed = value;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDHI :
      fields->f_dst32_rn_prefixed_HI = value;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDQI :
      fields->f_dst32_rn_prefixed_QI = value;
      break;
    case M32C_OPERAND_DST32RNPREFIXEDSI :
      fields->f_dst32_rn_prefixed_SI = value;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDHI :
      fields->f_dst32_rn_unprefixed_HI = value;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDQI :
      fields->f_dst32_rn_unprefixed_QI = value;
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDSI :
      fields->f_dst32_rn_unprefixed_SI = value;
      break;
    case M32C_OPERAND_G :
      break;
    case M32C_OPERAND_IMM_12_S4 :
      fields->f_imm_12_s4 = value;
      break;
    case M32C_OPERAND_IMM_12_S4N :
      fields->f_imm_12_s4 = value;
      break;
    case M32C_OPERAND_IMM_13_U3 :
      fields->f_imm_13_u3 = value;
      break;
    case M32C_OPERAND_IMM_16_HI :
      fields->f_dsp_16_s16 = value;
      break;
    case M32C_OPERAND_IMM_16_QI :
      fields->f_dsp_16_s8 = value;
      break;
    case M32C_OPERAND_IMM_16_SI :
      fields->f_dsp_16_s32 = value;
      break;
    case M32C_OPERAND_IMM_20_S4 :
      fields->f_imm_20_s4 = value;
      break;
    case M32C_OPERAND_IMM_24_HI :
      fields->f_dsp_24_s16 = value;
      break;
    case M32C_OPERAND_IMM_24_QI :
      fields->f_dsp_24_s8 = value;
      break;
    case M32C_OPERAND_IMM_24_SI :
      fields->f_dsp_24_s32 = value;
      break;
    case M32C_OPERAND_IMM_32_HI :
      fields->f_dsp_32_s16 = value;
      break;
    case M32C_OPERAND_IMM_32_QI :
      fields->f_dsp_32_s8 = value;
      break;
    case M32C_OPERAND_IMM_32_SI :
      fields->f_dsp_32_s32 = value;
      break;
    case M32C_OPERAND_IMM_40_HI :
      fields->f_dsp_40_s16 = value;
      break;
    case M32C_OPERAND_IMM_40_QI :
      fields->f_dsp_40_s8 = value;
      break;
    case M32C_OPERAND_IMM_40_SI :
      fields->f_dsp_40_s32 = value;
      break;
    case M32C_OPERAND_IMM_48_HI :
      fields->f_dsp_48_s16 = value;
      break;
    case M32C_OPERAND_IMM_48_QI :
      fields->f_dsp_48_s8 = value;
      break;
    case M32C_OPERAND_IMM_48_SI :
      fields->f_dsp_48_s32 = value;
      break;
    case M32C_OPERAND_IMM_56_HI :
      fields->f_dsp_56_s16 = value;
      break;
    case M32C_OPERAND_IMM_56_QI :
      fields->f_dsp_56_s8 = value;
      break;
    case M32C_OPERAND_IMM_64_HI :
      fields->f_dsp_64_s16 = value;
      break;
    case M32C_OPERAND_IMM_8_HI :
      fields->f_dsp_8_s16 = value;
      break;
    case M32C_OPERAND_IMM_8_QI :
      fields->f_dsp_8_s8 = value;
      break;
    case M32C_OPERAND_IMM_8_S4 :
      fields->f_imm_8_s4 = value;
      break;
    case M32C_OPERAND_IMM_8_S4N :
      fields->f_imm_8_s4 = value;
      break;
    case M32C_OPERAND_IMM_SH_12_S4 :
      fields->f_imm_12_s4 = value;
      break;
    case M32C_OPERAND_IMM_SH_20_S4 :
      fields->f_imm_20_s4 = value;
      break;
    case M32C_OPERAND_IMM_SH_8_S4 :
      fields->f_imm_8_s4 = value;
      break;
    case M32C_OPERAND_IMM1_S :
      fields->f_imm1_S = value;
      break;
    case M32C_OPERAND_IMM3_S :
      fields->f_imm3_S = value;
      break;
    case M32C_OPERAND_LAB_16_8 :
      fields->f_lab_16_8 = value;
      break;
    case M32C_OPERAND_LAB_24_8 :
      fields->f_lab_24_8 = value;
      break;
    case M32C_OPERAND_LAB_32_8 :
      fields->f_lab_32_8 = value;
      break;
    case M32C_OPERAND_LAB_40_8 :
      fields->f_lab_40_8 = value;
      break;
    case M32C_OPERAND_LAB_5_3 :
      fields->f_lab_5_3 = value;
      break;
    case M32C_OPERAND_LAB_8_16 :
      fields->f_lab_8_16 = value;
      break;
    case M32C_OPERAND_LAB_8_24 :
      fields->f_lab_8_24 = value;
      break;
    case M32C_OPERAND_LAB_8_8 :
      fields->f_lab_8_8 = value;
      break;
    case M32C_OPERAND_LAB32_JMP_S :
      fields->f_lab32_jmp_s = value;
      break;
    case M32C_OPERAND_Q :
      break;
    case M32C_OPERAND_R0 :
      break;
    case M32C_OPERAND_R0H :
      break;
    case M32C_OPERAND_R0L :
      break;
    case M32C_OPERAND_R1 :
      break;
    case M32C_OPERAND_R1R2R0 :
      break;
    case M32C_OPERAND_R2 :
      break;
    case M32C_OPERAND_R2R0 :
      break;
    case M32C_OPERAND_R3 :
      break;
    case M32C_OPERAND_R3R1 :
      break;
    case M32C_OPERAND_REGSETPOP :
      fields->f_8_8 = value;
      break;
    case M32C_OPERAND_REGSETPUSH :
      fields->f_8_8 = value;
      break;
    case M32C_OPERAND_RN16_PUSH_S :
      fields->f_4_1 = value;
      break;
    case M32C_OPERAND_S :
      break;
    case M32C_OPERAND_SRC16AN :
      fields->f_src16_an = value;
      break;
    case M32C_OPERAND_SRC16ANHI :
      fields->f_src16_an = value;
      break;
    case M32C_OPERAND_SRC16ANQI :
      fields->f_src16_an = value;
      break;
    case M32C_OPERAND_SRC16RNHI :
      fields->f_src16_rn = value;
      break;
    case M32C_OPERAND_SRC16RNQI :
      fields->f_src16_rn = value;
      break;
    case M32C_OPERAND_SRC32ANPREFIXED :
      fields->f_src32_an_prefixed = value;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDHI :
      fields->f_src32_an_prefixed = value;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDQI :
      fields->f_src32_an_prefixed = value;
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDSI :
      fields->f_src32_an_prefixed = value;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXED :
      fields->f_src32_an_unprefixed = value;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDHI :
      fields->f_src32_an_unprefixed = value;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDQI :
      fields->f_src32_an_unprefixed = value;
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDSI :
      fields->f_src32_an_unprefixed = value;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDHI :
      fields->f_src32_rn_prefixed_HI = value;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDQI :
      fields->f_src32_rn_prefixed_QI = value;
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDSI :
      fields->f_src32_rn_prefixed_SI = value;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDHI :
      fields->f_src32_rn_unprefixed_HI = value;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDQI :
      fields->f_src32_rn_unprefixed_QI = value;
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDSI :
      fields->f_src32_rn_unprefixed_SI = value;
      break;
    case M32C_OPERAND_SRCDST16_R0L_R0H_S_NORMAL :
      fields->f_5_1 = value;
      break;
    case M32C_OPERAND_X :
      break;
    case M32C_OPERAND_Z :
      break;
    case M32C_OPERAND_COND16_16 :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_COND16_24 :
      fields->f_dsp_24_u8 = value;
      break;
    case M32C_OPERAND_COND16_32 :
      fields->f_dsp_32_u8 = value;
      break;
    case M32C_OPERAND_COND16C :
      fields->f_cond16 = value;
      break;
    case M32C_OPERAND_COND16J :
      fields->f_cond16 = value;
      break;
    case M32C_OPERAND_COND16J5 :
      fields->f_cond16j_5 = value;
      break;
    case M32C_OPERAND_COND32 :
      fields->f_cond32 = value;
      break;
    case M32C_OPERAND_COND32_16 :
      fields->f_dsp_16_u8 = value;
      break;
    case M32C_OPERAND_COND32_24 :
      fields->f_dsp_24_u8 = value;
      break;
    case M32C_OPERAND_COND32_32 :
      fields->f_dsp_32_u8 = value;
      break;
    case M32C_OPERAND_COND32_40 :
      fields->f_dsp_40_u8 = value;
      break;
    case M32C_OPERAND_COND32J :
      fields->f_cond32j = value;
      break;
    case M32C_OPERAND_CR1_PREFIXED_32 :
      fields->f_21_3 = value;
      break;
    case M32C_OPERAND_CR1_UNPREFIXED_32 :
      fields->f_13_3 = value;
      break;
    case M32C_OPERAND_CR16 :
      fields->f_9_3 = value;
      break;
    case M32C_OPERAND_CR2_32 :
      fields->f_13_3 = value;
      break;
    case M32C_OPERAND_CR3_PREFIXED_32 :
      fields->f_21_3 = value;
      break;
    case M32C_OPERAND_CR3_UNPREFIXED_32 :
      fields->f_13_3 = value;
      break;
    case M32C_OPERAND_FLAGS16 :
      fields->f_9_3 = value;
      break;
    case M32C_OPERAND_FLAGS32 :
      fields->f_13_3 = value;
      break;
    case M32C_OPERAND_SCCOND32 :
      fields->f_cond16 = value;
      break;
    case M32C_OPERAND_SIZE :
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while setting vma operand.\n"),
		       opindex);
      abort ();
  }
}

/* Function to call before using the instruction builder tables.  */

void
m32c_cgen_init_ibld_table (CGEN_CPU_DESC cd)
{
  cd->insert_handlers = & m32c_cgen_insert_handlers[0];
  cd->extract_handlers = & m32c_cgen_extract_handlers[0];

  cd->insert_operand = m32c_cgen_insert_operand;
  cd->extract_operand = m32c_cgen_extract_operand;

  cd->get_int_operand = m32c_cgen_get_int_operand;
  cd->set_int_operand = m32c_cgen_set_int_operand;
  cd->get_vma_operand = m32c_cgen_get_vma_operand;
  cd->set_vma_operand = m32c_cgen_set_vma_operand;
}
