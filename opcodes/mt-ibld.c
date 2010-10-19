/* Instruction building/extraction support for mt. -*- C -*-

   THIS FILE IS MACHINE GENERATED WITH CGEN: Cpu tools GENerator.
   - the resultant file is machine generated, cgen-ibld.in isn't

   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2005
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
#include "mt-desc.h"
#include "mt-opc.h"
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
      
      if ((unsigned long) value > maxval)
	{
	  /* xgettext:c-format */
	  sprintf (errbuf,
		   _("operand out of range (%lu not between 0 and %lu)"),
		   value, maxval);
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
      if (word_offset == 0
	  && word_length > total_length)
	word_length = total_length;
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

const char * mt_cgen_insert_operand
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
mt_cgen_insert_operand (CGEN_CPU_DESC cd,
			     int opindex,
			     CGEN_FIELDS * fields,
			     CGEN_INSN_BYTES_PTR buffer,
			     bfd_vma pc ATTRIBUTE_UNUSED)
{
  const char * errmsg = NULL;
  unsigned int total_length = CGEN_FIELDS_BITSIZE (fields);

  switch (opindex)
    {
    case MT_OPERAND_A23 :
      errmsg = insert_normal (cd, fields->f_a23, 0, 0, 23, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_BALL :
      errmsg = insert_normal (cd, fields->f_ball, 0, 0, 19, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_BALL2 :
      errmsg = insert_normal (cd, fields->f_ball2, 0, 0, 15, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_BANKADDR :
      errmsg = insert_normal (cd, fields->f_bankaddr, 0, 0, 25, 13, 32, total_length, buffer);
      break;
    case MT_OPERAND_BRC :
      errmsg = insert_normal (cd, fields->f_brc, 0, 0, 18, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_BRC2 :
      errmsg = insert_normal (cd, fields->f_brc2, 0, 0, 14, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_CB1INCR :
      errmsg = insert_normal (cd, fields->f_cb1incr, 0|(1<<CGEN_IFLD_SIGNED), 0, 19, 6, 32, total_length, buffer);
      break;
    case MT_OPERAND_CB1SEL :
      errmsg = insert_normal (cd, fields->f_cb1sel, 0, 0, 25, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_CB2INCR :
      errmsg = insert_normal (cd, fields->f_cb2incr, 0|(1<<CGEN_IFLD_SIGNED), 0, 13, 6, 32, total_length, buffer);
      break;
    case MT_OPERAND_CB2SEL :
      errmsg = insert_normal (cd, fields->f_cb2sel, 0, 0, 22, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_CBRB :
      errmsg = insert_normal (cd, fields->f_cbrb, 0, 0, 10, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_CBS :
      errmsg = insert_normal (cd, fields->f_cbs, 0, 0, 19, 2, 32, total_length, buffer);
      break;
    case MT_OPERAND_CBX :
      errmsg = insert_normal (cd, fields->f_cbx, 0, 0, 14, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_CCB :
      errmsg = insert_normal (cd, fields->f_ccb, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_CDB :
      errmsg = insert_normal (cd, fields->f_cdb, 0, 0, 10, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_CELL :
      errmsg = insert_normal (cd, fields->f_cell, 0, 0, 9, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_COLNUM :
      errmsg = insert_normal (cd, fields->f_colnum, 0, 0, 18, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_CONTNUM :
      errmsg = insert_normal (cd, fields->f_contnum, 0, 0, 8, 9, 32, total_length, buffer);
      break;
    case MT_OPERAND_CR :
      errmsg = insert_normal (cd, fields->f_cr, 0, 0, 22, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_CTXDISP :
      errmsg = insert_normal (cd, fields->f_ctxdisp, 0, 0, 5, 6, 32, total_length, buffer);
      break;
    case MT_OPERAND_DUP :
      errmsg = insert_normal (cd, fields->f_dup, 0, 0, 6, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_FBDISP :
      errmsg = insert_normal (cd, fields->f_fbdisp, 0, 0, 15, 6, 32, total_length, buffer);
      break;
    case MT_OPERAND_FBINCR :
      errmsg = insert_normal (cd, fields->f_fbincr, 0, 0, 23, 4, 32, total_length, buffer);
      break;
    case MT_OPERAND_FRDR :
      errmsg = insert_normal (cd, fields->f_dr, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 19, 4, 32, total_length, buffer);
      break;
    case MT_OPERAND_FRDRRR :
      errmsg = insert_normal (cd, fields->f_drrr, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 15, 4, 32, total_length, buffer);
      break;
    case MT_OPERAND_FRSR1 :
      errmsg = insert_normal (cd, fields->f_sr1, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 23, 4, 32, total_length, buffer);
      break;
    case MT_OPERAND_FRSR2 :
      errmsg = insert_normal (cd, fields->f_sr2, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 19, 4, 32, total_length, buffer);
      break;
    case MT_OPERAND_ID :
      errmsg = insert_normal (cd, fields->f_id, 0, 0, 14, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_IMM16 :
      {
        long value = fields->f_imm16s;
        value = ((value) + (0));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 0, 15, 16, 32, total_length, buffer);
      }
      break;
    case MT_OPERAND_IMM16L :
      errmsg = insert_normal (cd, fields->f_imm16l, 0, 0, 23, 16, 32, total_length, buffer);
      break;
    case MT_OPERAND_IMM16O :
      {
        long value = fields->f_imm16s;
        value = ((value) + (0));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED), 0, 15, 16, 32, total_length, buffer);
      }
      break;
    case MT_OPERAND_IMM16Z :
      errmsg = insert_normal (cd, fields->f_imm16u, 0, 0, 15, 16, 32, total_length, buffer);
      break;
    case MT_OPERAND_INCAMT :
      errmsg = insert_normal (cd, fields->f_incamt, 0, 0, 19, 8, 32, total_length, buffer);
      break;
    case MT_OPERAND_INCR :
      errmsg = insert_normal (cd, fields->f_incr, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case MT_OPERAND_LENGTH :
      errmsg = insert_normal (cd, fields->f_length, 0, 0, 15, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_LOOPSIZE :
      {
        long value = fields->f_loopo;
        value = ((unsigned int) (value) >> (2));
        errmsg = insert_normal (cd, value, 0, 0, 7, 8, 32, total_length, buffer);
      }
      break;
    case MT_OPERAND_MASK :
      errmsg = insert_normal (cd, fields->f_mask, 0, 0, 25, 16, 32, total_length, buffer);
      break;
    case MT_OPERAND_MASK1 :
      errmsg = insert_normal (cd, fields->f_mask1, 0, 0, 22, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_MODE :
      errmsg = insert_normal (cd, fields->f_mode, 0, 0, 25, 2, 32, total_length, buffer);
      break;
    case MT_OPERAND_PERM :
      errmsg = insert_normal (cd, fields->f_perm, 0, 0, 25, 2, 32, total_length, buffer);
      break;
    case MT_OPERAND_RBBC :
      errmsg = insert_normal (cd, fields->f_rbbc, 0, 0, 25, 2, 32, total_length, buffer);
      break;
    case MT_OPERAND_RC :
      errmsg = insert_normal (cd, fields->f_rc, 0, 0, 15, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_RC1 :
      errmsg = insert_normal (cd, fields->f_rc1, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_RC2 :
      errmsg = insert_normal (cd, fields->f_rc2, 0, 0, 6, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_RC3 :
      errmsg = insert_normal (cd, fields->f_rc3, 0, 0, 7, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_RCNUM :
      errmsg = insert_normal (cd, fields->f_rcnum, 0, 0, 14, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_RDA :
      errmsg = insert_normal (cd, fields->f_rda, 0, 0, 25, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_ROWNUM :
      errmsg = insert_normal (cd, fields->f_rownum, 0, 0, 14, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_ROWNUM1 :
      errmsg = insert_normal (cd, fields->f_rownum1, 0, 0, 12, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_ROWNUM2 :
      errmsg = insert_normal (cd, fields->f_rownum2, 0, 0, 9, 3, 32, total_length, buffer);
      break;
    case MT_OPERAND_SIZE :
      errmsg = insert_normal (cd, fields->f_size, 0, 0, 13, 14, 32, total_length, buffer);
      break;
    case MT_OPERAND_TYPE :
      errmsg = insert_normal (cd, fields->f_type, 0, 0, 21, 2, 32, total_length, buffer);
      break;
    case MT_OPERAND_WR :
      errmsg = insert_normal (cd, fields->f_wr, 0, 0, 24, 1, 32, total_length, buffer);
      break;
    case MT_OPERAND_XMODE :
      errmsg = insert_normal (cd, fields->f_xmode, 0, 0, 23, 1, 32, total_length, buffer);
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while building insn.\n"),
	       opindex);
      abort ();
  }

  return errmsg;
}

int mt_cgen_extract_operand
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
mt_cgen_extract_operand (CGEN_CPU_DESC cd,
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
    case MT_OPERAND_A23 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 23, 1, 32, total_length, pc, & fields->f_a23);
      break;
    case MT_OPERAND_BALL :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 19, 1, 32, total_length, pc, & fields->f_ball);
      break;
    case MT_OPERAND_BALL2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 1, 32, total_length, pc, & fields->f_ball2);
      break;
    case MT_OPERAND_BANKADDR :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 13, 32, total_length, pc, & fields->f_bankaddr);
      break;
    case MT_OPERAND_BRC :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 18, 3, 32, total_length, pc, & fields->f_brc);
      break;
    case MT_OPERAND_BRC2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 3, 32, total_length, pc, & fields->f_brc2);
      break;
    case MT_OPERAND_CB1INCR :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 19, 6, 32, total_length, pc, & fields->f_cb1incr);
      break;
    case MT_OPERAND_CB1SEL :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 3, 32, total_length, pc, & fields->f_cb1sel);
      break;
    case MT_OPERAND_CB2INCR :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 13, 6, 32, total_length, pc, & fields->f_cb2incr);
      break;
    case MT_OPERAND_CB2SEL :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 22, 3, 32, total_length, pc, & fields->f_cb2sel);
      break;
    case MT_OPERAND_CBRB :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 10, 1, 32, total_length, pc, & fields->f_cbrb);
      break;
    case MT_OPERAND_CBS :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 19, 2, 32, total_length, pc, & fields->f_cbs);
      break;
    case MT_OPERAND_CBX :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 3, 32, total_length, pc, & fields->f_cbx);
      break;
    case MT_OPERAND_CCB :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_ccb);
      break;
    case MT_OPERAND_CDB :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 10, 1, 32, total_length, pc, & fields->f_cdb);
      break;
    case MT_OPERAND_CELL :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 3, 32, total_length, pc, & fields->f_cell);
      break;
    case MT_OPERAND_COLNUM :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 18, 3, 32, total_length, pc, & fields->f_colnum);
      break;
    case MT_OPERAND_CONTNUM :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 9, 32, total_length, pc, & fields->f_contnum);
      break;
    case MT_OPERAND_CR :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 22, 3, 32, total_length, pc, & fields->f_cr);
      break;
    case MT_OPERAND_CTXDISP :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 6, 32, total_length, pc, & fields->f_ctxdisp);
      break;
    case MT_OPERAND_DUP :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 6, 1, 32, total_length, pc, & fields->f_dup);
      break;
    case MT_OPERAND_FBDISP :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 6, 32, total_length, pc, & fields->f_fbdisp);
      break;
    case MT_OPERAND_FBINCR :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 23, 4, 32, total_length, pc, & fields->f_fbincr);
      break;
    case MT_OPERAND_FRDR :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 19, 4, 32, total_length, pc, & fields->f_dr);
      break;
    case MT_OPERAND_FRDRRR :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 15, 4, 32, total_length, pc, & fields->f_drrr);
      break;
    case MT_OPERAND_FRSR1 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 23, 4, 32, total_length, pc, & fields->f_sr1);
      break;
    case MT_OPERAND_FRSR2 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_ABS_ADDR), 0, 19, 4, 32, total_length, pc, & fields->f_sr2);
      break;
    case MT_OPERAND_ID :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 1, 32, total_length, pc, & fields->f_id);
      break;
    case MT_OPERAND_IMM16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 15, 16, 32, total_length, pc, & value);
        value = ((value) + (0));
        fields->f_imm16s = value;
      }
      break;
    case MT_OPERAND_IMM16L :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 23, 16, 32, total_length, pc, & fields->f_imm16l);
      break;
    case MT_OPERAND_IMM16O :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 15, 16, 32, total_length, pc, & value);
        value = ((value) + (0));
        fields->f_imm16s = value;
      }
      break;
    case MT_OPERAND_IMM16Z :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 16, 32, total_length, pc, & fields->f_imm16u);
      break;
    case MT_OPERAND_INCAMT :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 19, 8, 32, total_length, pc, & fields->f_incamt);
      break;
    case MT_OPERAND_INCR :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_incr);
      break;
    case MT_OPERAND_LENGTH :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 3, 32, total_length, pc, & fields->f_length);
      break;
    case MT_OPERAND_LOOPSIZE :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 7, 8, 32, total_length, pc, & value);
        value = ((((value) << (2))) + (8));
        fields->f_loopo = value;
      }
      break;
    case MT_OPERAND_MASK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 16, 32, total_length, pc, & fields->f_mask);
      break;
    case MT_OPERAND_MASK1 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 22, 3, 32, total_length, pc, & fields->f_mask1);
      break;
    case MT_OPERAND_MODE :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 2, 32, total_length, pc, & fields->f_mode);
      break;
    case MT_OPERAND_PERM :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 2, 32, total_length, pc, & fields->f_perm);
      break;
    case MT_OPERAND_RBBC :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 2, 32, total_length, pc, & fields->f_rbbc);
      break;
    case MT_OPERAND_RC :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 1, 32, total_length, pc, & fields->f_rc);
      break;
    case MT_OPERAND_RC1 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_rc1);
      break;
    case MT_OPERAND_RC2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 6, 1, 32, total_length, pc, & fields->f_rc2);
      break;
    case MT_OPERAND_RC3 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 7, 1, 32, total_length, pc, & fields->f_rc3);
      break;
    case MT_OPERAND_RCNUM :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 3, 32, total_length, pc, & fields->f_rcnum);
      break;
    case MT_OPERAND_RDA :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 1, 32, total_length, pc, & fields->f_rda);
      break;
    case MT_OPERAND_ROWNUM :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 3, 32, total_length, pc, & fields->f_rownum);
      break;
    case MT_OPERAND_ROWNUM1 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 12, 3, 32, total_length, pc, & fields->f_rownum1);
      break;
    case MT_OPERAND_ROWNUM2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 3, 32, total_length, pc, & fields->f_rownum2);
      break;
    case MT_OPERAND_SIZE :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 14, 32, total_length, pc, & fields->f_size);
      break;
    case MT_OPERAND_TYPE :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 21, 2, 32, total_length, pc, & fields->f_type);
      break;
    case MT_OPERAND_WR :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 24, 1, 32, total_length, pc, & fields->f_wr);
      break;
    case MT_OPERAND_XMODE :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 23, 1, 32, total_length, pc, & fields->f_xmode);
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while decoding insn.\n"),
	       opindex);
      abort ();
    }

  return length;
}

cgen_insert_fn * const mt_cgen_insert_handlers[] = 
{
  insert_insn_normal,
};

cgen_extract_fn * const mt_cgen_extract_handlers[] = 
{
  extract_insn_normal,
};

int mt_cgen_get_int_operand     (CGEN_CPU_DESC, int, const CGEN_FIELDS *);
bfd_vma mt_cgen_get_vma_operand (CGEN_CPU_DESC, int, const CGEN_FIELDS *);

/* Getting values from cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they return.
   TODO: floating point, inlining support, remove cases where result type
   not appropriate.  */

int
mt_cgen_get_int_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     const CGEN_FIELDS * fields)
{
  int value;

  switch (opindex)
    {
    case MT_OPERAND_A23 :
      value = fields->f_a23;
      break;
    case MT_OPERAND_BALL :
      value = fields->f_ball;
      break;
    case MT_OPERAND_BALL2 :
      value = fields->f_ball2;
      break;
    case MT_OPERAND_BANKADDR :
      value = fields->f_bankaddr;
      break;
    case MT_OPERAND_BRC :
      value = fields->f_brc;
      break;
    case MT_OPERAND_BRC2 :
      value = fields->f_brc2;
      break;
    case MT_OPERAND_CB1INCR :
      value = fields->f_cb1incr;
      break;
    case MT_OPERAND_CB1SEL :
      value = fields->f_cb1sel;
      break;
    case MT_OPERAND_CB2INCR :
      value = fields->f_cb2incr;
      break;
    case MT_OPERAND_CB2SEL :
      value = fields->f_cb2sel;
      break;
    case MT_OPERAND_CBRB :
      value = fields->f_cbrb;
      break;
    case MT_OPERAND_CBS :
      value = fields->f_cbs;
      break;
    case MT_OPERAND_CBX :
      value = fields->f_cbx;
      break;
    case MT_OPERAND_CCB :
      value = fields->f_ccb;
      break;
    case MT_OPERAND_CDB :
      value = fields->f_cdb;
      break;
    case MT_OPERAND_CELL :
      value = fields->f_cell;
      break;
    case MT_OPERAND_COLNUM :
      value = fields->f_colnum;
      break;
    case MT_OPERAND_CONTNUM :
      value = fields->f_contnum;
      break;
    case MT_OPERAND_CR :
      value = fields->f_cr;
      break;
    case MT_OPERAND_CTXDISP :
      value = fields->f_ctxdisp;
      break;
    case MT_OPERAND_DUP :
      value = fields->f_dup;
      break;
    case MT_OPERAND_FBDISP :
      value = fields->f_fbdisp;
      break;
    case MT_OPERAND_FBINCR :
      value = fields->f_fbincr;
      break;
    case MT_OPERAND_FRDR :
      value = fields->f_dr;
      break;
    case MT_OPERAND_FRDRRR :
      value = fields->f_drrr;
      break;
    case MT_OPERAND_FRSR1 :
      value = fields->f_sr1;
      break;
    case MT_OPERAND_FRSR2 :
      value = fields->f_sr2;
      break;
    case MT_OPERAND_ID :
      value = fields->f_id;
      break;
    case MT_OPERAND_IMM16 :
      value = fields->f_imm16s;
      break;
    case MT_OPERAND_IMM16L :
      value = fields->f_imm16l;
      break;
    case MT_OPERAND_IMM16O :
      value = fields->f_imm16s;
      break;
    case MT_OPERAND_IMM16Z :
      value = fields->f_imm16u;
      break;
    case MT_OPERAND_INCAMT :
      value = fields->f_incamt;
      break;
    case MT_OPERAND_INCR :
      value = fields->f_incr;
      break;
    case MT_OPERAND_LENGTH :
      value = fields->f_length;
      break;
    case MT_OPERAND_LOOPSIZE :
      value = fields->f_loopo;
      break;
    case MT_OPERAND_MASK :
      value = fields->f_mask;
      break;
    case MT_OPERAND_MASK1 :
      value = fields->f_mask1;
      break;
    case MT_OPERAND_MODE :
      value = fields->f_mode;
      break;
    case MT_OPERAND_PERM :
      value = fields->f_perm;
      break;
    case MT_OPERAND_RBBC :
      value = fields->f_rbbc;
      break;
    case MT_OPERAND_RC :
      value = fields->f_rc;
      break;
    case MT_OPERAND_RC1 :
      value = fields->f_rc1;
      break;
    case MT_OPERAND_RC2 :
      value = fields->f_rc2;
      break;
    case MT_OPERAND_RC3 :
      value = fields->f_rc3;
      break;
    case MT_OPERAND_RCNUM :
      value = fields->f_rcnum;
      break;
    case MT_OPERAND_RDA :
      value = fields->f_rda;
      break;
    case MT_OPERAND_ROWNUM :
      value = fields->f_rownum;
      break;
    case MT_OPERAND_ROWNUM1 :
      value = fields->f_rownum1;
      break;
    case MT_OPERAND_ROWNUM2 :
      value = fields->f_rownum2;
      break;
    case MT_OPERAND_SIZE :
      value = fields->f_size;
      break;
    case MT_OPERAND_TYPE :
      value = fields->f_type;
      break;
    case MT_OPERAND_WR :
      value = fields->f_wr;
      break;
    case MT_OPERAND_XMODE :
      value = fields->f_xmode;
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
mt_cgen_get_vma_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     const CGEN_FIELDS * fields)
{
  bfd_vma value;

  switch (opindex)
    {
    case MT_OPERAND_A23 :
      value = fields->f_a23;
      break;
    case MT_OPERAND_BALL :
      value = fields->f_ball;
      break;
    case MT_OPERAND_BALL2 :
      value = fields->f_ball2;
      break;
    case MT_OPERAND_BANKADDR :
      value = fields->f_bankaddr;
      break;
    case MT_OPERAND_BRC :
      value = fields->f_brc;
      break;
    case MT_OPERAND_BRC2 :
      value = fields->f_brc2;
      break;
    case MT_OPERAND_CB1INCR :
      value = fields->f_cb1incr;
      break;
    case MT_OPERAND_CB1SEL :
      value = fields->f_cb1sel;
      break;
    case MT_OPERAND_CB2INCR :
      value = fields->f_cb2incr;
      break;
    case MT_OPERAND_CB2SEL :
      value = fields->f_cb2sel;
      break;
    case MT_OPERAND_CBRB :
      value = fields->f_cbrb;
      break;
    case MT_OPERAND_CBS :
      value = fields->f_cbs;
      break;
    case MT_OPERAND_CBX :
      value = fields->f_cbx;
      break;
    case MT_OPERAND_CCB :
      value = fields->f_ccb;
      break;
    case MT_OPERAND_CDB :
      value = fields->f_cdb;
      break;
    case MT_OPERAND_CELL :
      value = fields->f_cell;
      break;
    case MT_OPERAND_COLNUM :
      value = fields->f_colnum;
      break;
    case MT_OPERAND_CONTNUM :
      value = fields->f_contnum;
      break;
    case MT_OPERAND_CR :
      value = fields->f_cr;
      break;
    case MT_OPERAND_CTXDISP :
      value = fields->f_ctxdisp;
      break;
    case MT_OPERAND_DUP :
      value = fields->f_dup;
      break;
    case MT_OPERAND_FBDISP :
      value = fields->f_fbdisp;
      break;
    case MT_OPERAND_FBINCR :
      value = fields->f_fbincr;
      break;
    case MT_OPERAND_FRDR :
      value = fields->f_dr;
      break;
    case MT_OPERAND_FRDRRR :
      value = fields->f_drrr;
      break;
    case MT_OPERAND_FRSR1 :
      value = fields->f_sr1;
      break;
    case MT_OPERAND_FRSR2 :
      value = fields->f_sr2;
      break;
    case MT_OPERAND_ID :
      value = fields->f_id;
      break;
    case MT_OPERAND_IMM16 :
      value = fields->f_imm16s;
      break;
    case MT_OPERAND_IMM16L :
      value = fields->f_imm16l;
      break;
    case MT_OPERAND_IMM16O :
      value = fields->f_imm16s;
      break;
    case MT_OPERAND_IMM16Z :
      value = fields->f_imm16u;
      break;
    case MT_OPERAND_INCAMT :
      value = fields->f_incamt;
      break;
    case MT_OPERAND_INCR :
      value = fields->f_incr;
      break;
    case MT_OPERAND_LENGTH :
      value = fields->f_length;
      break;
    case MT_OPERAND_LOOPSIZE :
      value = fields->f_loopo;
      break;
    case MT_OPERAND_MASK :
      value = fields->f_mask;
      break;
    case MT_OPERAND_MASK1 :
      value = fields->f_mask1;
      break;
    case MT_OPERAND_MODE :
      value = fields->f_mode;
      break;
    case MT_OPERAND_PERM :
      value = fields->f_perm;
      break;
    case MT_OPERAND_RBBC :
      value = fields->f_rbbc;
      break;
    case MT_OPERAND_RC :
      value = fields->f_rc;
      break;
    case MT_OPERAND_RC1 :
      value = fields->f_rc1;
      break;
    case MT_OPERAND_RC2 :
      value = fields->f_rc2;
      break;
    case MT_OPERAND_RC3 :
      value = fields->f_rc3;
      break;
    case MT_OPERAND_RCNUM :
      value = fields->f_rcnum;
      break;
    case MT_OPERAND_RDA :
      value = fields->f_rda;
      break;
    case MT_OPERAND_ROWNUM :
      value = fields->f_rownum;
      break;
    case MT_OPERAND_ROWNUM1 :
      value = fields->f_rownum1;
      break;
    case MT_OPERAND_ROWNUM2 :
      value = fields->f_rownum2;
      break;
    case MT_OPERAND_SIZE :
      value = fields->f_size;
      break;
    case MT_OPERAND_TYPE :
      value = fields->f_type;
      break;
    case MT_OPERAND_WR :
      value = fields->f_wr;
      break;
    case MT_OPERAND_XMODE :
      value = fields->f_xmode;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while getting vma operand.\n"),
		       opindex);
      abort ();
  }

  return value;
}

void mt_cgen_set_int_operand  (CGEN_CPU_DESC, int, CGEN_FIELDS *, int);
void mt_cgen_set_vma_operand  (CGEN_CPU_DESC, int, CGEN_FIELDS *, bfd_vma);

/* Stuffing values in cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they accept.
   TODO: floating point, inlining support, remove cases where argument type
   not appropriate.  */

void
mt_cgen_set_int_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     CGEN_FIELDS * fields,
			     int value)
{
  switch (opindex)
    {
    case MT_OPERAND_A23 :
      fields->f_a23 = value;
      break;
    case MT_OPERAND_BALL :
      fields->f_ball = value;
      break;
    case MT_OPERAND_BALL2 :
      fields->f_ball2 = value;
      break;
    case MT_OPERAND_BANKADDR :
      fields->f_bankaddr = value;
      break;
    case MT_OPERAND_BRC :
      fields->f_brc = value;
      break;
    case MT_OPERAND_BRC2 :
      fields->f_brc2 = value;
      break;
    case MT_OPERAND_CB1INCR :
      fields->f_cb1incr = value;
      break;
    case MT_OPERAND_CB1SEL :
      fields->f_cb1sel = value;
      break;
    case MT_OPERAND_CB2INCR :
      fields->f_cb2incr = value;
      break;
    case MT_OPERAND_CB2SEL :
      fields->f_cb2sel = value;
      break;
    case MT_OPERAND_CBRB :
      fields->f_cbrb = value;
      break;
    case MT_OPERAND_CBS :
      fields->f_cbs = value;
      break;
    case MT_OPERAND_CBX :
      fields->f_cbx = value;
      break;
    case MT_OPERAND_CCB :
      fields->f_ccb = value;
      break;
    case MT_OPERAND_CDB :
      fields->f_cdb = value;
      break;
    case MT_OPERAND_CELL :
      fields->f_cell = value;
      break;
    case MT_OPERAND_COLNUM :
      fields->f_colnum = value;
      break;
    case MT_OPERAND_CONTNUM :
      fields->f_contnum = value;
      break;
    case MT_OPERAND_CR :
      fields->f_cr = value;
      break;
    case MT_OPERAND_CTXDISP :
      fields->f_ctxdisp = value;
      break;
    case MT_OPERAND_DUP :
      fields->f_dup = value;
      break;
    case MT_OPERAND_FBDISP :
      fields->f_fbdisp = value;
      break;
    case MT_OPERAND_FBINCR :
      fields->f_fbincr = value;
      break;
    case MT_OPERAND_FRDR :
      fields->f_dr = value;
      break;
    case MT_OPERAND_FRDRRR :
      fields->f_drrr = value;
      break;
    case MT_OPERAND_FRSR1 :
      fields->f_sr1 = value;
      break;
    case MT_OPERAND_FRSR2 :
      fields->f_sr2 = value;
      break;
    case MT_OPERAND_ID :
      fields->f_id = value;
      break;
    case MT_OPERAND_IMM16 :
      fields->f_imm16s = value;
      break;
    case MT_OPERAND_IMM16L :
      fields->f_imm16l = value;
      break;
    case MT_OPERAND_IMM16O :
      fields->f_imm16s = value;
      break;
    case MT_OPERAND_IMM16Z :
      fields->f_imm16u = value;
      break;
    case MT_OPERAND_INCAMT :
      fields->f_incamt = value;
      break;
    case MT_OPERAND_INCR :
      fields->f_incr = value;
      break;
    case MT_OPERAND_LENGTH :
      fields->f_length = value;
      break;
    case MT_OPERAND_LOOPSIZE :
      fields->f_loopo = value;
      break;
    case MT_OPERAND_MASK :
      fields->f_mask = value;
      break;
    case MT_OPERAND_MASK1 :
      fields->f_mask1 = value;
      break;
    case MT_OPERAND_MODE :
      fields->f_mode = value;
      break;
    case MT_OPERAND_PERM :
      fields->f_perm = value;
      break;
    case MT_OPERAND_RBBC :
      fields->f_rbbc = value;
      break;
    case MT_OPERAND_RC :
      fields->f_rc = value;
      break;
    case MT_OPERAND_RC1 :
      fields->f_rc1 = value;
      break;
    case MT_OPERAND_RC2 :
      fields->f_rc2 = value;
      break;
    case MT_OPERAND_RC3 :
      fields->f_rc3 = value;
      break;
    case MT_OPERAND_RCNUM :
      fields->f_rcnum = value;
      break;
    case MT_OPERAND_RDA :
      fields->f_rda = value;
      break;
    case MT_OPERAND_ROWNUM :
      fields->f_rownum = value;
      break;
    case MT_OPERAND_ROWNUM1 :
      fields->f_rownum1 = value;
      break;
    case MT_OPERAND_ROWNUM2 :
      fields->f_rownum2 = value;
      break;
    case MT_OPERAND_SIZE :
      fields->f_size = value;
      break;
    case MT_OPERAND_TYPE :
      fields->f_type = value;
      break;
    case MT_OPERAND_WR :
      fields->f_wr = value;
      break;
    case MT_OPERAND_XMODE :
      fields->f_xmode = value;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while setting int operand.\n"),
		       opindex);
      abort ();
  }
}

void
mt_cgen_set_vma_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     CGEN_FIELDS * fields,
			     bfd_vma value)
{
  switch (opindex)
    {
    case MT_OPERAND_A23 :
      fields->f_a23 = value;
      break;
    case MT_OPERAND_BALL :
      fields->f_ball = value;
      break;
    case MT_OPERAND_BALL2 :
      fields->f_ball2 = value;
      break;
    case MT_OPERAND_BANKADDR :
      fields->f_bankaddr = value;
      break;
    case MT_OPERAND_BRC :
      fields->f_brc = value;
      break;
    case MT_OPERAND_BRC2 :
      fields->f_brc2 = value;
      break;
    case MT_OPERAND_CB1INCR :
      fields->f_cb1incr = value;
      break;
    case MT_OPERAND_CB1SEL :
      fields->f_cb1sel = value;
      break;
    case MT_OPERAND_CB2INCR :
      fields->f_cb2incr = value;
      break;
    case MT_OPERAND_CB2SEL :
      fields->f_cb2sel = value;
      break;
    case MT_OPERAND_CBRB :
      fields->f_cbrb = value;
      break;
    case MT_OPERAND_CBS :
      fields->f_cbs = value;
      break;
    case MT_OPERAND_CBX :
      fields->f_cbx = value;
      break;
    case MT_OPERAND_CCB :
      fields->f_ccb = value;
      break;
    case MT_OPERAND_CDB :
      fields->f_cdb = value;
      break;
    case MT_OPERAND_CELL :
      fields->f_cell = value;
      break;
    case MT_OPERAND_COLNUM :
      fields->f_colnum = value;
      break;
    case MT_OPERAND_CONTNUM :
      fields->f_contnum = value;
      break;
    case MT_OPERAND_CR :
      fields->f_cr = value;
      break;
    case MT_OPERAND_CTXDISP :
      fields->f_ctxdisp = value;
      break;
    case MT_OPERAND_DUP :
      fields->f_dup = value;
      break;
    case MT_OPERAND_FBDISP :
      fields->f_fbdisp = value;
      break;
    case MT_OPERAND_FBINCR :
      fields->f_fbincr = value;
      break;
    case MT_OPERAND_FRDR :
      fields->f_dr = value;
      break;
    case MT_OPERAND_FRDRRR :
      fields->f_drrr = value;
      break;
    case MT_OPERAND_FRSR1 :
      fields->f_sr1 = value;
      break;
    case MT_OPERAND_FRSR2 :
      fields->f_sr2 = value;
      break;
    case MT_OPERAND_ID :
      fields->f_id = value;
      break;
    case MT_OPERAND_IMM16 :
      fields->f_imm16s = value;
      break;
    case MT_OPERAND_IMM16L :
      fields->f_imm16l = value;
      break;
    case MT_OPERAND_IMM16O :
      fields->f_imm16s = value;
      break;
    case MT_OPERAND_IMM16Z :
      fields->f_imm16u = value;
      break;
    case MT_OPERAND_INCAMT :
      fields->f_incamt = value;
      break;
    case MT_OPERAND_INCR :
      fields->f_incr = value;
      break;
    case MT_OPERAND_LENGTH :
      fields->f_length = value;
      break;
    case MT_OPERAND_LOOPSIZE :
      fields->f_loopo = value;
      break;
    case MT_OPERAND_MASK :
      fields->f_mask = value;
      break;
    case MT_OPERAND_MASK1 :
      fields->f_mask1 = value;
      break;
    case MT_OPERAND_MODE :
      fields->f_mode = value;
      break;
    case MT_OPERAND_PERM :
      fields->f_perm = value;
      break;
    case MT_OPERAND_RBBC :
      fields->f_rbbc = value;
      break;
    case MT_OPERAND_RC :
      fields->f_rc = value;
      break;
    case MT_OPERAND_RC1 :
      fields->f_rc1 = value;
      break;
    case MT_OPERAND_RC2 :
      fields->f_rc2 = value;
      break;
    case MT_OPERAND_RC3 :
      fields->f_rc3 = value;
      break;
    case MT_OPERAND_RCNUM :
      fields->f_rcnum = value;
      break;
    case MT_OPERAND_RDA :
      fields->f_rda = value;
      break;
    case MT_OPERAND_ROWNUM :
      fields->f_rownum = value;
      break;
    case MT_OPERAND_ROWNUM1 :
      fields->f_rownum1 = value;
      break;
    case MT_OPERAND_ROWNUM2 :
      fields->f_rownum2 = value;
      break;
    case MT_OPERAND_SIZE :
      fields->f_size = value;
      break;
    case MT_OPERAND_TYPE :
      fields->f_type = value;
      break;
    case MT_OPERAND_WR :
      fields->f_wr = value;
      break;
    case MT_OPERAND_XMODE :
      fields->f_xmode = value;
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
mt_cgen_init_ibld_table (CGEN_CPU_DESC cd)
{
  cd->insert_handlers = & mt_cgen_insert_handlers[0];
  cd->extract_handlers = & mt_cgen_extract_handlers[0];

  cd->insert_operand = mt_cgen_insert_operand;
  cd->extract_operand = mt_cgen_extract_operand;

  cd->get_int_operand = mt_cgen_get_int_operand;
  cd->set_int_operand = mt_cgen_set_int_operand;
  cd->get_vma_operand = mt_cgen_get_vma_operand;
  cd->set_vma_operand = mt_cgen_set_vma_operand;
}
