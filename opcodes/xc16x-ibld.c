/* Instruction building/extraction support for xc16x. -*- C -*-

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
#include "xc16x-desc.h"
#include "xc16x-opc.h"
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

const char * xc16x_cgen_insert_operand
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
xc16x_cgen_insert_operand (CGEN_CPU_DESC cd,
			     int opindex,
			     CGEN_FIELDS * fields,
			     CGEN_INSN_BYTES_PTR buffer,
			     bfd_vma pc ATTRIBUTE_UNUSED)
{
  const char * errmsg = NULL;
  unsigned int total_length = CGEN_FIELDS_BITSIZE (fields);

  switch (opindex)
    {
    case XC16X_OPERAND_REGNAM :
      errmsg = insert_normal (cd, fields->f_reg8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_BIT01 :
      errmsg = insert_normal (cd, fields->f_op_1bit, 0, 0, 8, 1, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_BIT1 :
      errmsg = insert_normal (cd, fields->f_op_bit1, 0, 0, 11, 1, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_BIT2 :
      errmsg = insert_normal (cd, fields->f_op_bit2, 0, 0, 11, 2, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_BIT4 :
      errmsg = insert_normal (cd, fields->f_op_bit4, 0, 0, 11, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_BIT8 :
      errmsg = insert_normal (cd, fields->f_op_bit8, 0, 0, 31, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_BITONE :
      errmsg = insert_normal (cd, fields->f_op_onebit, 0, 0, 9, 1, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_CADDR :
      errmsg = insert_normal (cd, fields->f_offset16, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_ABS_ADDR), 0, 31, 16, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_COND :
      errmsg = insert_normal (cd, fields->f_condcode, 0, 0, 7, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_DATA8 :
      errmsg = insert_normal (cd, fields->f_data8, 0, 0, 23, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_DATAHI8 :
      errmsg = insert_normal (cd, fields->f_datahi8, 0, 0, 31, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_DOT :
      break;
    case XC16X_OPERAND_DR :
      errmsg = insert_normal (cd, fields->f_r1, 0, 0, 15, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_DRB :
      errmsg = insert_normal (cd, fields->f_r1, 0, 0, 15, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_DRI :
      errmsg = insert_normal (cd, fields->f_r4, 0, 0, 11, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_EXTCOND :
      errmsg = insert_normal (cd, fields->f_extccode, 0, 0, 15, 5, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_GENREG :
      errmsg = insert_normal (cd, fields->f_regb8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_HASH :
      break;
    case XC16X_OPERAND_ICOND :
      errmsg = insert_normal (cd, fields->f_icondcode, 0, 0, 15, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_LBIT2 :
      errmsg = insert_normal (cd, fields->f_op_lbit2, 0, 0, 15, 2, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_LBIT4 :
      errmsg = insert_normal (cd, fields->f_op_lbit4, 0, 0, 15, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_MASK8 :
      errmsg = insert_normal (cd, fields->f_mask8, 0, 0, 23, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_MASKLO8 :
      errmsg = insert_normal (cd, fields->f_datahi8, 0, 0, 31, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_MEMGR8 :
      errmsg = insert_normal (cd, fields->f_memgr8, 0, 0, 31, 16, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_MEMORY :
      errmsg = insert_normal (cd, fields->f_memory, 0, 0, 31, 16, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_PAG :
      break;
    case XC16X_OPERAND_PAGENUM :
      errmsg = insert_normal (cd, fields->f_pagenum, 0, 0, 25, 10, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_POF :
      break;
    case XC16X_OPERAND_QBIT :
      errmsg = insert_normal (cd, fields->f_qbit, 0, 0, 7, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_QHIBIT :
      errmsg = insert_normal (cd, fields->f_qhibit, 0, 0, 27, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_QLOBIT :
      errmsg = insert_normal (cd, fields->f_qlobit, 0, 0, 31, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_REG8 :
      errmsg = insert_normal (cd, fields->f_reg8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_REGB8 :
      errmsg = insert_normal (cd, fields->f_regb8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_REGBMEM8 :
      errmsg = insert_normal (cd, fields->f_regmem8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_REGHI8 :
      errmsg = insert_normal (cd, fields->f_reghi8, 0, 0, 23, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_REGMEM8 :
      errmsg = insert_normal (cd, fields->f_regmem8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_REGOFF8 :
      errmsg = insert_normal (cd, fields->f_regoff8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_REL :
      errmsg = insert_normal (cd, fields->f_rel8, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_RELHI :
      errmsg = insert_normal (cd, fields->f_relhi8, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 23, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_SEG :
      errmsg = insert_normal (cd, fields->f_seg8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_SEGHI8 :
      errmsg = insert_normal (cd, fields->f_segnum8, 0, 0, 23, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_SEGM :
      break;
    case XC16X_OPERAND_SOF :
      break;
    case XC16X_OPERAND_SR :
      errmsg = insert_normal (cd, fields->f_r2, 0, 0, 11, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_SR2 :
      errmsg = insert_normal (cd, fields->f_r0, 0, 0, 9, 2, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_SRB :
      errmsg = insert_normal (cd, fields->f_r2, 0, 0, 11, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_SRC1 :
      errmsg = insert_normal (cd, fields->f_r1, 0, 0, 15, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_SRC2 :
      errmsg = insert_normal (cd, fields->f_r2, 0, 0, 11, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_SRDIV :
      errmsg = insert_normal (cd, fields->f_reg8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_U4 :
      errmsg = insert_normal (cd, fields->f_uimm4, 0, 0, 15, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_UIMM16 :
      errmsg = insert_normal (cd, fields->f_uimm16, 0, 0, 31, 16, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_UIMM2 :
      errmsg = insert_normal (cd, fields->f_uimm2, 0, 0, 13, 2, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_UIMM3 :
      errmsg = insert_normal (cd, fields->f_uimm3, 0, 0, 10, 3, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_UIMM4 :
      errmsg = insert_normal (cd, fields->f_uimm4, 0, 0, 15, 4, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_UIMM7 :
      errmsg = insert_normal (cd, fields->f_uimm7, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 15, 7, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_UIMM8 :
      errmsg = insert_normal (cd, fields->f_uimm8, 0, 0, 23, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_UPAG16 :
      errmsg = insert_normal (cd, fields->f_uimm16, 0, 0, 31, 16, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_UPOF16 :
      errmsg = insert_normal (cd, fields->f_memory, 0, 0, 31, 16, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_USEG16 :
      errmsg = insert_normal (cd, fields->f_offset16, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_ABS_ADDR), 0, 31, 16, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_USEG8 :
      errmsg = insert_normal (cd, fields->f_seg8, 0, 0, 15, 8, 32, total_length, buffer);
      break;
    case XC16X_OPERAND_USOF16 :
      errmsg = insert_normal (cd, fields->f_offset16, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_ABS_ADDR), 0, 31, 16, 32, total_length, buffer);
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while building insn.\n"),
	       opindex);
      abort ();
  }

  return errmsg;
}

int xc16x_cgen_extract_operand
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
xc16x_cgen_extract_operand (CGEN_CPU_DESC cd,
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
    case XC16X_OPERAND_REGNAM :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_reg8);
      break;
    case XC16X_OPERAND_BIT01 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 1, 32, total_length, pc, & fields->f_op_1bit);
      break;
    case XC16X_OPERAND_BIT1 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 1, 32, total_length, pc, & fields->f_op_bit1);
      break;
    case XC16X_OPERAND_BIT2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 2, 32, total_length, pc, & fields->f_op_bit2);
      break;
    case XC16X_OPERAND_BIT4 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 4, 32, total_length, pc, & fields->f_op_bit4);
      break;
    case XC16X_OPERAND_BIT8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 8, 32, total_length, pc, & fields->f_op_bit8);
      break;
    case XC16X_OPERAND_BITONE :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 1, 32, total_length, pc, & fields->f_op_onebit);
      break;
    case XC16X_OPERAND_CADDR :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_ABS_ADDR), 0, 31, 16, 32, total_length, pc, & fields->f_offset16);
      break;
    case XC16X_OPERAND_COND :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 7, 4, 32, total_length, pc, & fields->f_condcode);
      break;
    case XC16X_OPERAND_DATA8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 23, 8, 32, total_length, pc, & fields->f_data8);
      break;
    case XC16X_OPERAND_DATAHI8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 8, 32, total_length, pc, & fields->f_datahi8);
      break;
    case XC16X_OPERAND_DOT :
      break;
    case XC16X_OPERAND_DR :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 4, 32, total_length, pc, & fields->f_r1);
      break;
    case XC16X_OPERAND_DRB :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 4, 32, total_length, pc, & fields->f_r1);
      break;
    case XC16X_OPERAND_DRI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 4, 32, total_length, pc, & fields->f_r4);
      break;
    case XC16X_OPERAND_EXTCOND :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 5, 32, total_length, pc, & fields->f_extccode);
      break;
    case XC16X_OPERAND_GENREG :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_regb8);
      break;
    case XC16X_OPERAND_HASH :
      break;
    case XC16X_OPERAND_ICOND :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 4, 32, total_length, pc, & fields->f_icondcode);
      break;
    case XC16X_OPERAND_LBIT2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 2, 32, total_length, pc, & fields->f_op_lbit2);
      break;
    case XC16X_OPERAND_LBIT4 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 4, 32, total_length, pc, & fields->f_op_lbit4);
      break;
    case XC16X_OPERAND_MASK8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 23, 8, 32, total_length, pc, & fields->f_mask8);
      break;
    case XC16X_OPERAND_MASKLO8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 8, 32, total_length, pc, & fields->f_datahi8);
      break;
    case XC16X_OPERAND_MEMGR8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 16, 32, total_length, pc, & fields->f_memgr8);
      break;
    case XC16X_OPERAND_MEMORY :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 16, 32, total_length, pc, & fields->f_memory);
      break;
    case XC16X_OPERAND_PAG :
      break;
    case XC16X_OPERAND_PAGENUM :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 10, 32, total_length, pc, & fields->f_pagenum);
      break;
    case XC16X_OPERAND_POF :
      break;
    case XC16X_OPERAND_QBIT :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 7, 4, 32, total_length, pc, & fields->f_qbit);
      break;
    case XC16X_OPERAND_QHIBIT :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 27, 4, 32, total_length, pc, & fields->f_qhibit);
      break;
    case XC16X_OPERAND_QLOBIT :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 4, 32, total_length, pc, & fields->f_qlobit);
      break;
    case XC16X_OPERAND_REG8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_reg8);
      break;
    case XC16X_OPERAND_REGB8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_regb8);
      break;
    case XC16X_OPERAND_REGBMEM8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_regmem8);
      break;
    case XC16X_OPERAND_REGHI8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 23, 8, 32, total_length, pc, & fields->f_reghi8);
      break;
    case XC16X_OPERAND_REGMEM8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_regmem8);
      break;
    case XC16X_OPERAND_REGOFF8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_regoff8);
      break;
    case XC16X_OPERAND_REL :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 15, 8, 32, total_length, pc, & fields->f_rel8);
      break;
    case XC16X_OPERAND_RELHI :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 23, 8, 32, total_length, pc, & fields->f_relhi8);
      break;
    case XC16X_OPERAND_SEG :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_seg8);
      break;
    case XC16X_OPERAND_SEGHI8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 23, 8, 32, total_length, pc, & fields->f_segnum8);
      break;
    case XC16X_OPERAND_SEGM :
      break;
    case XC16X_OPERAND_SOF :
      break;
    case XC16X_OPERAND_SR :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 4, 32, total_length, pc, & fields->f_r2);
      break;
    case XC16X_OPERAND_SR2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 9, 2, 32, total_length, pc, & fields->f_r0);
      break;
    case XC16X_OPERAND_SRB :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 4, 32, total_length, pc, & fields->f_r2);
      break;
    case XC16X_OPERAND_SRC1 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 4, 32, total_length, pc, & fields->f_r1);
      break;
    case XC16X_OPERAND_SRC2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 4, 32, total_length, pc, & fields->f_r2);
      break;
    case XC16X_OPERAND_SRDIV :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_reg8);
      break;
    case XC16X_OPERAND_U4 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 4, 32, total_length, pc, & fields->f_uimm4);
      break;
    case XC16X_OPERAND_UIMM16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 16, 32, total_length, pc, & fields->f_uimm16);
      break;
    case XC16X_OPERAND_UIMM2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 13, 2, 32, total_length, pc, & fields->f_uimm2);
      break;
    case XC16X_OPERAND_UIMM3 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 10, 3, 32, total_length, pc, & fields->f_uimm3);
      break;
    case XC16X_OPERAND_UIMM4 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 4, 32, total_length, pc, & fields->f_uimm4);
      break;
    case XC16X_OPERAND_UIMM7 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 15, 7, 32, total_length, pc, & fields->f_uimm7);
      break;
    case XC16X_OPERAND_UIMM8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 23, 8, 32, total_length, pc, & fields->f_uimm8);
      break;
    case XC16X_OPERAND_UPAG16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 16, 32, total_length, pc, & fields->f_uimm16);
      break;
    case XC16X_OPERAND_UPOF16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 16, 32, total_length, pc, & fields->f_memory);
      break;
    case XC16X_OPERAND_USEG16 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_ABS_ADDR), 0, 31, 16, 32, total_length, pc, & fields->f_offset16);
      break;
    case XC16X_OPERAND_USEG8 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 8, 32, total_length, pc, & fields->f_seg8);
      break;
    case XC16X_OPERAND_USOF16 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_RELOC)|(1<<CGEN_IFLD_ABS_ADDR), 0, 31, 16, 32, total_length, pc, & fields->f_offset16);
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while decoding insn.\n"),
	       opindex);
      abort ();
    }

  return length;
}

cgen_insert_fn * const xc16x_cgen_insert_handlers[] = 
{
  insert_insn_normal,
};

cgen_extract_fn * const xc16x_cgen_extract_handlers[] = 
{
  extract_insn_normal,
};

int xc16x_cgen_get_int_operand     (CGEN_CPU_DESC, int, const CGEN_FIELDS *);
bfd_vma xc16x_cgen_get_vma_operand (CGEN_CPU_DESC, int, const CGEN_FIELDS *);

/* Getting values from cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they return.
   TODO: floating point, inlining support, remove cases where result type
   not appropriate.  */

int
xc16x_cgen_get_int_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     const CGEN_FIELDS * fields)
{
  int value;

  switch (opindex)
    {
    case XC16X_OPERAND_REGNAM :
      value = fields->f_reg8;
      break;
    case XC16X_OPERAND_BIT01 :
      value = fields->f_op_1bit;
      break;
    case XC16X_OPERAND_BIT1 :
      value = fields->f_op_bit1;
      break;
    case XC16X_OPERAND_BIT2 :
      value = fields->f_op_bit2;
      break;
    case XC16X_OPERAND_BIT4 :
      value = fields->f_op_bit4;
      break;
    case XC16X_OPERAND_BIT8 :
      value = fields->f_op_bit8;
      break;
    case XC16X_OPERAND_BITONE :
      value = fields->f_op_onebit;
      break;
    case XC16X_OPERAND_CADDR :
      value = fields->f_offset16;
      break;
    case XC16X_OPERAND_COND :
      value = fields->f_condcode;
      break;
    case XC16X_OPERAND_DATA8 :
      value = fields->f_data8;
      break;
    case XC16X_OPERAND_DATAHI8 :
      value = fields->f_datahi8;
      break;
    case XC16X_OPERAND_DOT :
      value = 0;
      break;
    case XC16X_OPERAND_DR :
      value = fields->f_r1;
      break;
    case XC16X_OPERAND_DRB :
      value = fields->f_r1;
      break;
    case XC16X_OPERAND_DRI :
      value = fields->f_r4;
      break;
    case XC16X_OPERAND_EXTCOND :
      value = fields->f_extccode;
      break;
    case XC16X_OPERAND_GENREG :
      value = fields->f_regb8;
      break;
    case XC16X_OPERAND_HASH :
      value = 0;
      break;
    case XC16X_OPERAND_ICOND :
      value = fields->f_icondcode;
      break;
    case XC16X_OPERAND_LBIT2 :
      value = fields->f_op_lbit2;
      break;
    case XC16X_OPERAND_LBIT4 :
      value = fields->f_op_lbit4;
      break;
    case XC16X_OPERAND_MASK8 :
      value = fields->f_mask8;
      break;
    case XC16X_OPERAND_MASKLO8 :
      value = fields->f_datahi8;
      break;
    case XC16X_OPERAND_MEMGR8 :
      value = fields->f_memgr8;
      break;
    case XC16X_OPERAND_MEMORY :
      value = fields->f_memory;
      break;
    case XC16X_OPERAND_PAG :
      value = 0;
      break;
    case XC16X_OPERAND_PAGENUM :
      value = fields->f_pagenum;
      break;
    case XC16X_OPERAND_POF :
      value = 0;
      break;
    case XC16X_OPERAND_QBIT :
      value = fields->f_qbit;
      break;
    case XC16X_OPERAND_QHIBIT :
      value = fields->f_qhibit;
      break;
    case XC16X_OPERAND_QLOBIT :
      value = fields->f_qlobit;
      break;
    case XC16X_OPERAND_REG8 :
      value = fields->f_reg8;
      break;
    case XC16X_OPERAND_REGB8 :
      value = fields->f_regb8;
      break;
    case XC16X_OPERAND_REGBMEM8 :
      value = fields->f_regmem8;
      break;
    case XC16X_OPERAND_REGHI8 :
      value = fields->f_reghi8;
      break;
    case XC16X_OPERAND_REGMEM8 :
      value = fields->f_regmem8;
      break;
    case XC16X_OPERAND_REGOFF8 :
      value = fields->f_regoff8;
      break;
    case XC16X_OPERAND_REL :
      value = fields->f_rel8;
      break;
    case XC16X_OPERAND_RELHI :
      value = fields->f_relhi8;
      break;
    case XC16X_OPERAND_SEG :
      value = fields->f_seg8;
      break;
    case XC16X_OPERAND_SEGHI8 :
      value = fields->f_segnum8;
      break;
    case XC16X_OPERAND_SEGM :
      value = 0;
      break;
    case XC16X_OPERAND_SOF :
      value = 0;
      break;
    case XC16X_OPERAND_SR :
      value = fields->f_r2;
      break;
    case XC16X_OPERAND_SR2 :
      value = fields->f_r0;
      break;
    case XC16X_OPERAND_SRB :
      value = fields->f_r2;
      break;
    case XC16X_OPERAND_SRC1 :
      value = fields->f_r1;
      break;
    case XC16X_OPERAND_SRC2 :
      value = fields->f_r2;
      break;
    case XC16X_OPERAND_SRDIV :
      value = fields->f_reg8;
      break;
    case XC16X_OPERAND_U4 :
      value = fields->f_uimm4;
      break;
    case XC16X_OPERAND_UIMM16 :
      value = fields->f_uimm16;
      break;
    case XC16X_OPERAND_UIMM2 :
      value = fields->f_uimm2;
      break;
    case XC16X_OPERAND_UIMM3 :
      value = fields->f_uimm3;
      break;
    case XC16X_OPERAND_UIMM4 :
      value = fields->f_uimm4;
      break;
    case XC16X_OPERAND_UIMM7 :
      value = fields->f_uimm7;
      break;
    case XC16X_OPERAND_UIMM8 :
      value = fields->f_uimm8;
      break;
    case XC16X_OPERAND_UPAG16 :
      value = fields->f_uimm16;
      break;
    case XC16X_OPERAND_UPOF16 :
      value = fields->f_memory;
      break;
    case XC16X_OPERAND_USEG16 :
      value = fields->f_offset16;
      break;
    case XC16X_OPERAND_USEG8 :
      value = fields->f_seg8;
      break;
    case XC16X_OPERAND_USOF16 :
      value = fields->f_offset16;
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
xc16x_cgen_get_vma_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     const CGEN_FIELDS * fields)
{
  bfd_vma value;

  switch (opindex)
    {
    case XC16X_OPERAND_REGNAM :
      value = fields->f_reg8;
      break;
    case XC16X_OPERAND_BIT01 :
      value = fields->f_op_1bit;
      break;
    case XC16X_OPERAND_BIT1 :
      value = fields->f_op_bit1;
      break;
    case XC16X_OPERAND_BIT2 :
      value = fields->f_op_bit2;
      break;
    case XC16X_OPERAND_BIT4 :
      value = fields->f_op_bit4;
      break;
    case XC16X_OPERAND_BIT8 :
      value = fields->f_op_bit8;
      break;
    case XC16X_OPERAND_BITONE :
      value = fields->f_op_onebit;
      break;
    case XC16X_OPERAND_CADDR :
      value = fields->f_offset16;
      break;
    case XC16X_OPERAND_COND :
      value = fields->f_condcode;
      break;
    case XC16X_OPERAND_DATA8 :
      value = fields->f_data8;
      break;
    case XC16X_OPERAND_DATAHI8 :
      value = fields->f_datahi8;
      break;
    case XC16X_OPERAND_DOT :
      value = 0;
      break;
    case XC16X_OPERAND_DR :
      value = fields->f_r1;
      break;
    case XC16X_OPERAND_DRB :
      value = fields->f_r1;
      break;
    case XC16X_OPERAND_DRI :
      value = fields->f_r4;
      break;
    case XC16X_OPERAND_EXTCOND :
      value = fields->f_extccode;
      break;
    case XC16X_OPERAND_GENREG :
      value = fields->f_regb8;
      break;
    case XC16X_OPERAND_HASH :
      value = 0;
      break;
    case XC16X_OPERAND_ICOND :
      value = fields->f_icondcode;
      break;
    case XC16X_OPERAND_LBIT2 :
      value = fields->f_op_lbit2;
      break;
    case XC16X_OPERAND_LBIT4 :
      value = fields->f_op_lbit4;
      break;
    case XC16X_OPERAND_MASK8 :
      value = fields->f_mask8;
      break;
    case XC16X_OPERAND_MASKLO8 :
      value = fields->f_datahi8;
      break;
    case XC16X_OPERAND_MEMGR8 :
      value = fields->f_memgr8;
      break;
    case XC16X_OPERAND_MEMORY :
      value = fields->f_memory;
      break;
    case XC16X_OPERAND_PAG :
      value = 0;
      break;
    case XC16X_OPERAND_PAGENUM :
      value = fields->f_pagenum;
      break;
    case XC16X_OPERAND_POF :
      value = 0;
      break;
    case XC16X_OPERAND_QBIT :
      value = fields->f_qbit;
      break;
    case XC16X_OPERAND_QHIBIT :
      value = fields->f_qhibit;
      break;
    case XC16X_OPERAND_QLOBIT :
      value = fields->f_qlobit;
      break;
    case XC16X_OPERAND_REG8 :
      value = fields->f_reg8;
      break;
    case XC16X_OPERAND_REGB8 :
      value = fields->f_regb8;
      break;
    case XC16X_OPERAND_REGBMEM8 :
      value = fields->f_regmem8;
      break;
    case XC16X_OPERAND_REGHI8 :
      value = fields->f_reghi8;
      break;
    case XC16X_OPERAND_REGMEM8 :
      value = fields->f_regmem8;
      break;
    case XC16X_OPERAND_REGOFF8 :
      value = fields->f_regoff8;
      break;
    case XC16X_OPERAND_REL :
      value = fields->f_rel8;
      break;
    case XC16X_OPERAND_RELHI :
      value = fields->f_relhi8;
      break;
    case XC16X_OPERAND_SEG :
      value = fields->f_seg8;
      break;
    case XC16X_OPERAND_SEGHI8 :
      value = fields->f_segnum8;
      break;
    case XC16X_OPERAND_SEGM :
      value = 0;
      break;
    case XC16X_OPERAND_SOF :
      value = 0;
      break;
    case XC16X_OPERAND_SR :
      value = fields->f_r2;
      break;
    case XC16X_OPERAND_SR2 :
      value = fields->f_r0;
      break;
    case XC16X_OPERAND_SRB :
      value = fields->f_r2;
      break;
    case XC16X_OPERAND_SRC1 :
      value = fields->f_r1;
      break;
    case XC16X_OPERAND_SRC2 :
      value = fields->f_r2;
      break;
    case XC16X_OPERAND_SRDIV :
      value = fields->f_reg8;
      break;
    case XC16X_OPERAND_U4 :
      value = fields->f_uimm4;
      break;
    case XC16X_OPERAND_UIMM16 :
      value = fields->f_uimm16;
      break;
    case XC16X_OPERAND_UIMM2 :
      value = fields->f_uimm2;
      break;
    case XC16X_OPERAND_UIMM3 :
      value = fields->f_uimm3;
      break;
    case XC16X_OPERAND_UIMM4 :
      value = fields->f_uimm4;
      break;
    case XC16X_OPERAND_UIMM7 :
      value = fields->f_uimm7;
      break;
    case XC16X_OPERAND_UIMM8 :
      value = fields->f_uimm8;
      break;
    case XC16X_OPERAND_UPAG16 :
      value = fields->f_uimm16;
      break;
    case XC16X_OPERAND_UPOF16 :
      value = fields->f_memory;
      break;
    case XC16X_OPERAND_USEG16 :
      value = fields->f_offset16;
      break;
    case XC16X_OPERAND_USEG8 :
      value = fields->f_seg8;
      break;
    case XC16X_OPERAND_USOF16 :
      value = fields->f_offset16;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while getting vma operand.\n"),
		       opindex);
      abort ();
  }

  return value;
}

void xc16x_cgen_set_int_operand  (CGEN_CPU_DESC, int, CGEN_FIELDS *, int);
void xc16x_cgen_set_vma_operand  (CGEN_CPU_DESC, int, CGEN_FIELDS *, bfd_vma);

/* Stuffing values in cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they accept.
   TODO: floating point, inlining support, remove cases where argument type
   not appropriate.  */

void
xc16x_cgen_set_int_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     CGEN_FIELDS * fields,
			     int value)
{
  switch (opindex)
    {
    case XC16X_OPERAND_REGNAM :
      fields->f_reg8 = value;
      break;
    case XC16X_OPERAND_BIT01 :
      fields->f_op_1bit = value;
      break;
    case XC16X_OPERAND_BIT1 :
      fields->f_op_bit1 = value;
      break;
    case XC16X_OPERAND_BIT2 :
      fields->f_op_bit2 = value;
      break;
    case XC16X_OPERAND_BIT4 :
      fields->f_op_bit4 = value;
      break;
    case XC16X_OPERAND_BIT8 :
      fields->f_op_bit8 = value;
      break;
    case XC16X_OPERAND_BITONE :
      fields->f_op_onebit = value;
      break;
    case XC16X_OPERAND_CADDR :
      fields->f_offset16 = value;
      break;
    case XC16X_OPERAND_COND :
      fields->f_condcode = value;
      break;
    case XC16X_OPERAND_DATA8 :
      fields->f_data8 = value;
      break;
    case XC16X_OPERAND_DATAHI8 :
      fields->f_datahi8 = value;
      break;
    case XC16X_OPERAND_DOT :
      break;
    case XC16X_OPERAND_DR :
      fields->f_r1 = value;
      break;
    case XC16X_OPERAND_DRB :
      fields->f_r1 = value;
      break;
    case XC16X_OPERAND_DRI :
      fields->f_r4 = value;
      break;
    case XC16X_OPERAND_EXTCOND :
      fields->f_extccode = value;
      break;
    case XC16X_OPERAND_GENREG :
      fields->f_regb8 = value;
      break;
    case XC16X_OPERAND_HASH :
      break;
    case XC16X_OPERAND_ICOND :
      fields->f_icondcode = value;
      break;
    case XC16X_OPERAND_LBIT2 :
      fields->f_op_lbit2 = value;
      break;
    case XC16X_OPERAND_LBIT4 :
      fields->f_op_lbit4 = value;
      break;
    case XC16X_OPERAND_MASK8 :
      fields->f_mask8 = value;
      break;
    case XC16X_OPERAND_MASKLO8 :
      fields->f_datahi8 = value;
      break;
    case XC16X_OPERAND_MEMGR8 :
      fields->f_memgr8 = value;
      break;
    case XC16X_OPERAND_MEMORY :
      fields->f_memory = value;
      break;
    case XC16X_OPERAND_PAG :
      break;
    case XC16X_OPERAND_PAGENUM :
      fields->f_pagenum = value;
      break;
    case XC16X_OPERAND_POF :
      break;
    case XC16X_OPERAND_QBIT :
      fields->f_qbit = value;
      break;
    case XC16X_OPERAND_QHIBIT :
      fields->f_qhibit = value;
      break;
    case XC16X_OPERAND_QLOBIT :
      fields->f_qlobit = value;
      break;
    case XC16X_OPERAND_REG8 :
      fields->f_reg8 = value;
      break;
    case XC16X_OPERAND_REGB8 :
      fields->f_regb8 = value;
      break;
    case XC16X_OPERAND_REGBMEM8 :
      fields->f_regmem8 = value;
      break;
    case XC16X_OPERAND_REGHI8 :
      fields->f_reghi8 = value;
      break;
    case XC16X_OPERAND_REGMEM8 :
      fields->f_regmem8 = value;
      break;
    case XC16X_OPERAND_REGOFF8 :
      fields->f_regoff8 = value;
      break;
    case XC16X_OPERAND_REL :
      fields->f_rel8 = value;
      break;
    case XC16X_OPERAND_RELHI :
      fields->f_relhi8 = value;
      break;
    case XC16X_OPERAND_SEG :
      fields->f_seg8 = value;
      break;
    case XC16X_OPERAND_SEGHI8 :
      fields->f_segnum8 = value;
      break;
    case XC16X_OPERAND_SEGM :
      break;
    case XC16X_OPERAND_SOF :
      break;
    case XC16X_OPERAND_SR :
      fields->f_r2 = value;
      break;
    case XC16X_OPERAND_SR2 :
      fields->f_r0 = value;
      break;
    case XC16X_OPERAND_SRB :
      fields->f_r2 = value;
      break;
    case XC16X_OPERAND_SRC1 :
      fields->f_r1 = value;
      break;
    case XC16X_OPERAND_SRC2 :
      fields->f_r2 = value;
      break;
    case XC16X_OPERAND_SRDIV :
      fields->f_reg8 = value;
      break;
    case XC16X_OPERAND_U4 :
      fields->f_uimm4 = value;
      break;
    case XC16X_OPERAND_UIMM16 :
      fields->f_uimm16 = value;
      break;
    case XC16X_OPERAND_UIMM2 :
      fields->f_uimm2 = value;
      break;
    case XC16X_OPERAND_UIMM3 :
      fields->f_uimm3 = value;
      break;
    case XC16X_OPERAND_UIMM4 :
      fields->f_uimm4 = value;
      break;
    case XC16X_OPERAND_UIMM7 :
      fields->f_uimm7 = value;
      break;
    case XC16X_OPERAND_UIMM8 :
      fields->f_uimm8 = value;
      break;
    case XC16X_OPERAND_UPAG16 :
      fields->f_uimm16 = value;
      break;
    case XC16X_OPERAND_UPOF16 :
      fields->f_memory = value;
      break;
    case XC16X_OPERAND_USEG16 :
      fields->f_offset16 = value;
      break;
    case XC16X_OPERAND_USEG8 :
      fields->f_seg8 = value;
      break;
    case XC16X_OPERAND_USOF16 :
      fields->f_offset16 = value;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while setting int operand.\n"),
		       opindex);
      abort ();
  }
}

void
xc16x_cgen_set_vma_operand (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			     int opindex,
			     CGEN_FIELDS * fields,
			     bfd_vma value)
{
  switch (opindex)
    {
    case XC16X_OPERAND_REGNAM :
      fields->f_reg8 = value;
      break;
    case XC16X_OPERAND_BIT01 :
      fields->f_op_1bit = value;
      break;
    case XC16X_OPERAND_BIT1 :
      fields->f_op_bit1 = value;
      break;
    case XC16X_OPERAND_BIT2 :
      fields->f_op_bit2 = value;
      break;
    case XC16X_OPERAND_BIT4 :
      fields->f_op_bit4 = value;
      break;
    case XC16X_OPERAND_BIT8 :
      fields->f_op_bit8 = value;
      break;
    case XC16X_OPERAND_BITONE :
      fields->f_op_onebit = value;
      break;
    case XC16X_OPERAND_CADDR :
      fields->f_offset16 = value;
      break;
    case XC16X_OPERAND_COND :
      fields->f_condcode = value;
      break;
    case XC16X_OPERAND_DATA8 :
      fields->f_data8 = value;
      break;
    case XC16X_OPERAND_DATAHI8 :
      fields->f_datahi8 = value;
      break;
    case XC16X_OPERAND_DOT :
      break;
    case XC16X_OPERAND_DR :
      fields->f_r1 = value;
      break;
    case XC16X_OPERAND_DRB :
      fields->f_r1 = value;
      break;
    case XC16X_OPERAND_DRI :
      fields->f_r4 = value;
      break;
    case XC16X_OPERAND_EXTCOND :
      fields->f_extccode = value;
      break;
    case XC16X_OPERAND_GENREG :
      fields->f_regb8 = value;
      break;
    case XC16X_OPERAND_HASH :
      break;
    case XC16X_OPERAND_ICOND :
      fields->f_icondcode = value;
      break;
    case XC16X_OPERAND_LBIT2 :
      fields->f_op_lbit2 = value;
      break;
    case XC16X_OPERAND_LBIT4 :
      fields->f_op_lbit4 = value;
      break;
    case XC16X_OPERAND_MASK8 :
      fields->f_mask8 = value;
      break;
    case XC16X_OPERAND_MASKLO8 :
      fields->f_datahi8 = value;
      break;
    case XC16X_OPERAND_MEMGR8 :
      fields->f_memgr8 = value;
      break;
    case XC16X_OPERAND_MEMORY :
      fields->f_memory = value;
      break;
    case XC16X_OPERAND_PAG :
      break;
    case XC16X_OPERAND_PAGENUM :
      fields->f_pagenum = value;
      break;
    case XC16X_OPERAND_POF :
      break;
    case XC16X_OPERAND_QBIT :
      fields->f_qbit = value;
      break;
    case XC16X_OPERAND_QHIBIT :
      fields->f_qhibit = value;
      break;
    case XC16X_OPERAND_QLOBIT :
      fields->f_qlobit = value;
      break;
    case XC16X_OPERAND_REG8 :
      fields->f_reg8 = value;
      break;
    case XC16X_OPERAND_REGB8 :
      fields->f_regb8 = value;
      break;
    case XC16X_OPERAND_REGBMEM8 :
      fields->f_regmem8 = value;
      break;
    case XC16X_OPERAND_REGHI8 :
      fields->f_reghi8 = value;
      break;
    case XC16X_OPERAND_REGMEM8 :
      fields->f_regmem8 = value;
      break;
    case XC16X_OPERAND_REGOFF8 :
      fields->f_regoff8 = value;
      break;
    case XC16X_OPERAND_REL :
      fields->f_rel8 = value;
      break;
    case XC16X_OPERAND_RELHI :
      fields->f_relhi8 = value;
      break;
    case XC16X_OPERAND_SEG :
      fields->f_seg8 = value;
      break;
    case XC16X_OPERAND_SEGHI8 :
      fields->f_segnum8 = value;
      break;
    case XC16X_OPERAND_SEGM :
      break;
    case XC16X_OPERAND_SOF :
      break;
    case XC16X_OPERAND_SR :
      fields->f_r2 = value;
      break;
    case XC16X_OPERAND_SR2 :
      fields->f_r0 = value;
      break;
    case XC16X_OPERAND_SRB :
      fields->f_r2 = value;
      break;
    case XC16X_OPERAND_SRC1 :
      fields->f_r1 = value;
      break;
    case XC16X_OPERAND_SRC2 :
      fields->f_r2 = value;
      break;
    case XC16X_OPERAND_SRDIV :
      fields->f_reg8 = value;
      break;
    case XC16X_OPERAND_U4 :
      fields->f_uimm4 = value;
      break;
    case XC16X_OPERAND_UIMM16 :
      fields->f_uimm16 = value;
      break;
    case XC16X_OPERAND_UIMM2 :
      fields->f_uimm2 = value;
      break;
    case XC16X_OPERAND_UIMM3 :
      fields->f_uimm3 = value;
      break;
    case XC16X_OPERAND_UIMM4 :
      fields->f_uimm4 = value;
      break;
    case XC16X_OPERAND_UIMM7 :
      fields->f_uimm7 = value;
      break;
    case XC16X_OPERAND_UIMM8 :
      fields->f_uimm8 = value;
      break;
    case XC16X_OPERAND_UPAG16 :
      fields->f_uimm16 = value;
      break;
    case XC16X_OPERAND_UPOF16 :
      fields->f_memory = value;
      break;
    case XC16X_OPERAND_USEG16 :
      fields->f_offset16 = value;
      break;
    case XC16X_OPERAND_USEG8 :
      fields->f_seg8 = value;
      break;
    case XC16X_OPERAND_USOF16 :
      fields->f_offset16 = value;
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
xc16x_cgen_init_ibld_table (CGEN_CPU_DESC cd)
{
  cd->insert_handlers = & xc16x_cgen_insert_handlers[0];
  cd->extract_handlers = & xc16x_cgen_extract_handlers[0];

  cd->insert_operand = xc16x_cgen_insert_operand;
  cd->extract_operand = xc16x_cgen_extract_operand;

  cd->get_int_operand = xc16x_cgen_get_int_operand;
  cd->set_int_operand = xc16x_cgen_set_int_operand;
  cd->get_vma_operand = xc16x_cgen_get_vma_operand;
  cd->set_vma_operand = xc16x_cgen_set_vma_operand;
}
