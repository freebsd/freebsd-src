/* Instruction building/extraction support for frv. -*- C -*-

THIS FILE IS MACHINE GENERATED WITH CGEN: Cpu tools GENerator.
- the resultant file is machine generated, cgen-ibld.in isn't

Copyright 1996, 1997, 1998, 1999, 2000, 2001 Free Software Foundation, Inc.

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
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* ??? Eventually more and more of this stuff can go to cpu-independent files.
   Keep that in mind.  */

#include "sysdep.h"
#include <stdio.h>
#include "ansidecl.h"
#include "dis-asm.h"
#include "bfd.h"
#include "symcat.h"
#include "frv-desc.h"
#include "frv-opc.h"
#include "opintl.h"
#include "safe-ctype.h"

#undef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#undef max
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

#if 0
  if (CGEN_INT_INSN_P
      && word_offset != 0)
    abort ();
#endif

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
#if 0
  int big_p = CGEN_CPU_INSN_ENDIAN (cd) == CGEN_ENDIAN_BIG;
#endif
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

#if 0
  if (CGEN_INT_INSN_P
      && word_offset != 0)
    abort ();
#endif

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

/* machine generated code added here */

const char * frv_cgen_insert_operand
  PARAMS ((CGEN_CPU_DESC, int, CGEN_FIELDS *, CGEN_INSN_BYTES_PTR, bfd_vma));

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
frv_cgen_insert_operand (cd, opindex, fields, buffer, pc)
     CGEN_CPU_DESC cd;
     int opindex;
     CGEN_FIELDS * fields;
     CGEN_INSN_BYTES_PTR buffer;
     bfd_vma pc ATTRIBUTE_UNUSED;
{
  const char * errmsg = NULL;
  unsigned int total_length = CGEN_FIELDS_BITSIZE (fields);

  switch (opindex)
    {
    case FRV_OPERAND_A0 :
      errmsg = insert_normal (cd, fields->f_A, 0, 0, 17, 1, 32, total_length, buffer);
      break;
    case FRV_OPERAND_A1 :
      errmsg = insert_normal (cd, fields->f_A, 0, 0, 17, 1, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ACC40SI :
      errmsg = insert_normal (cd, fields->f_ACC40Si, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ACC40SK :
      errmsg = insert_normal (cd, fields->f_ACC40Sk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ACC40UI :
      errmsg = insert_normal (cd, fields->f_ACC40Ui, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ACC40UK :
      errmsg = insert_normal (cd, fields->f_ACC40Uk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ACCGI :
      errmsg = insert_normal (cd, fields->f_ACCGi, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ACCGK :
      errmsg = insert_normal (cd, fields->f_ACCGk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CCI :
      errmsg = insert_normal (cd, fields->f_CCi, 0, 0, 11, 3, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CPRDOUBLEK :
      errmsg = insert_normal (cd, fields->f_CPRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CPRI :
      errmsg = insert_normal (cd, fields->f_CPRi, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CPRJ :
      errmsg = insert_normal (cd, fields->f_CPRj, 0, 0, 5, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CPRK :
      errmsg = insert_normal (cd, fields->f_CPRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CRI :
      errmsg = insert_normal (cd, fields->f_CRi, 0, 0, 14, 3, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CRJ :
      errmsg = insert_normal (cd, fields->f_CRj, 0, 0, 2, 3, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CRJ_FLOAT :
      errmsg = insert_normal (cd, fields->f_CRj_float, 0, 0, 26, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CRJ_INT :
      {
        long value = fields->f_CRj_int;
        value = ((value) - (4));
        errmsg = insert_normal (cd, value, 0, 0, 26, 2, 32, total_length, buffer);
      }
      break;
    case FRV_OPERAND_CRK :
      errmsg = insert_normal (cd, fields->f_CRk, 0, 0, 27, 3, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FCCI_1 :
      errmsg = insert_normal (cd, fields->f_FCCi_1, 0, 0, 11, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FCCI_2 :
      errmsg = insert_normal (cd, fields->f_FCCi_2, 0, 0, 26, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FCCI_3 :
      errmsg = insert_normal (cd, fields->f_FCCi_3, 0, 0, 1, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FCCK :
      errmsg = insert_normal (cd, fields->f_FCCk, 0, 0, 26, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRDOUBLEI :
      errmsg = insert_normal (cd, fields->f_FRi, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRDOUBLEJ :
      errmsg = insert_normal (cd, fields->f_FRj, 0, 0, 5, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRDOUBLEK :
      errmsg = insert_normal (cd, fields->f_FRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRI :
      errmsg = insert_normal (cd, fields->f_FRi, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRINTI :
      errmsg = insert_normal (cd, fields->f_FRi, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRINTIEVEN :
      errmsg = insert_normal (cd, fields->f_FRi, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRINTJ :
      errmsg = insert_normal (cd, fields->f_FRj, 0, 0, 5, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRINTJEVEN :
      errmsg = insert_normal (cd, fields->f_FRj, 0, 0, 5, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRINTK :
      errmsg = insert_normal (cd, fields->f_FRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRINTKEVEN :
      errmsg = insert_normal (cd, fields->f_FRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRJ :
      errmsg = insert_normal (cd, fields->f_FRj, 0, 0, 5, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRK :
      errmsg = insert_normal (cd, fields->f_FRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRKHI :
      errmsg = insert_normal (cd, fields->f_FRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_FRKLO :
      errmsg = insert_normal (cd, fields->f_FRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_GRDOUBLEK :
      errmsg = insert_normal (cd, fields->f_GRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_GRI :
      errmsg = insert_normal (cd, fields->f_GRi, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_GRJ :
      errmsg = insert_normal (cd, fields->f_GRj, 0, 0, 5, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_GRK :
      errmsg = insert_normal (cd, fields->f_GRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_GRKHI :
      errmsg = insert_normal (cd, fields->f_GRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_GRKLO :
      errmsg = insert_normal (cd, fields->f_GRk, 0, 0, 30, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ICCI_1 :
      errmsg = insert_normal (cd, fields->f_ICCi_1, 0, 0, 11, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ICCI_2 :
      errmsg = insert_normal (cd, fields->f_ICCi_2, 0, 0, 26, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ICCI_3 :
      errmsg = insert_normal (cd, fields->f_ICCi_3, 0, 0, 1, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_LI :
      errmsg = insert_normal (cd, fields->f_LI, 0, 0, 25, 1, 32, total_length, buffer);
      break;
    case FRV_OPERAND_AE :
      errmsg = insert_normal (cd, fields->f_ae, 0, 0, 25, 1, 32, total_length, buffer);
      break;
    case FRV_OPERAND_CCOND :
      errmsg = insert_normal (cd, fields->f_ccond, 0, 0, 12, 1, 32, total_length, buffer);
      break;
    case FRV_OPERAND_COND :
      errmsg = insert_normal (cd, fields->f_cond, 0, 0, 8, 1, 32, total_length, buffer);
      break;
    case FRV_OPERAND_D12 :
      errmsg = insert_normal (cd, fields->f_d12, 0|(1<<CGEN_IFLD_SIGNED), 0, 11, 12, 32, total_length, buffer);
      break;
    case FRV_OPERAND_DEBUG :
      errmsg = insert_normal (cd, fields->f_debug, 0, 0, 25, 1, 32, total_length, buffer);
      break;
    case FRV_OPERAND_EIR :
      errmsg = insert_normal (cd, fields->f_eir, 0, 0, 17, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_HINT :
      errmsg = insert_normal (cd, fields->f_hint, 0, 0, 17, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_HINT_NOT_TAKEN :
      errmsg = insert_normal (cd, fields->f_hint, 0, 0, 17, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_HINT_TAKEN :
      errmsg = insert_normal (cd, fields->f_hint, 0, 0, 17, 2, 32, total_length, buffer);
      break;
    case FRV_OPERAND_LABEL16 :
      {
        long value = fields->f_label16;
        value = ((int) (((value) - (pc))) >> (2));
        errmsg = insert_normal (cd, value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 15, 16, 32, total_length, buffer);
      }
      break;
    case FRV_OPERAND_LABEL24 :
      {
{
  FLD (f_labelH6) = ((int) (((FLD (f_label24)) - (pc))) >> (20));
  FLD (f_labelL18) = ((((unsigned int) (((FLD (f_label24)) - (pc))) >> (2))) & (262143));
}
        errmsg = insert_normal (cd, fields->f_labelH6, 0|(1<<CGEN_IFLD_SIGNED), 0, 30, 6, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_labelL18, 0, 0, 17, 18, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case FRV_OPERAND_LOCK :
      errmsg = insert_normal (cd, fields->f_lock, 0, 0, 25, 1, 32, total_length, buffer);
      break;
    case FRV_OPERAND_PACK :
      errmsg = insert_normal (cd, fields->f_pack, 0, 0, 31, 1, 32, total_length, buffer);
      break;
    case FRV_OPERAND_S10 :
      errmsg = insert_normal (cd, fields->f_s10, 0|(1<<CGEN_IFLD_SIGNED), 0, 9, 10, 32, total_length, buffer);
      break;
    case FRV_OPERAND_S12 :
      errmsg = insert_normal (cd, fields->f_d12, 0|(1<<CGEN_IFLD_SIGNED), 0, 11, 12, 32, total_length, buffer);
      break;
    case FRV_OPERAND_S16 :
      errmsg = insert_normal (cd, fields->f_s16, 0|(1<<CGEN_IFLD_SIGNED), 0, 15, 16, 32, total_length, buffer);
      break;
    case FRV_OPERAND_S5 :
      errmsg = insert_normal (cd, fields->f_s5, 0|(1<<CGEN_IFLD_SIGNED), 0, 4, 5, 32, total_length, buffer);
      break;
    case FRV_OPERAND_S6 :
      errmsg = insert_normal (cd, fields->f_s6, 0|(1<<CGEN_IFLD_SIGNED), 0, 5, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_S6_1 :
      errmsg = insert_normal (cd, fields->f_s6_1, 0|(1<<CGEN_IFLD_SIGNED), 0, 11, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_SLO16 :
      errmsg = insert_normal (cd, fields->f_s16, 0|(1<<CGEN_IFLD_SIGNED), 0, 15, 16, 32, total_length, buffer);
      break;
    case FRV_OPERAND_SPR :
      {
{
  FLD (f_spr_h) = ((unsigned int) (FLD (f_spr)) >> (6));
  FLD (f_spr_l) = ((FLD (f_spr)) & (63));
}
        errmsg = insert_normal (cd, fields->f_spr_h, 0, 0, 30, 6, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_spr_l, 0, 0, 17, 6, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case FRV_OPERAND_U12 :
      {
{
  FLD (f_u12_h) = ((int) (FLD (f_u12)) >> (6));
  FLD (f_u12_l) = ((FLD (f_u12)) & (63));
}
        errmsg = insert_normal (cd, fields->f_u12_h, 0|(1<<CGEN_IFLD_SIGNED), 0, 17, 6, 32, total_length, buffer);
        if (errmsg)
          break;
        errmsg = insert_normal (cd, fields->f_u12_l, 0, 0, 5, 6, 32, total_length, buffer);
        if (errmsg)
          break;
      }
      break;
    case FRV_OPERAND_U16 :
      errmsg = insert_normal (cd, fields->f_u16, 0, 0, 15, 16, 32, total_length, buffer);
      break;
    case FRV_OPERAND_U6 :
      errmsg = insert_normal (cd, fields->f_u6, 0, 0, 5, 6, 32, total_length, buffer);
      break;
    case FRV_OPERAND_UHI16 :
      errmsg = insert_normal (cd, fields->f_u16, 0, 0, 15, 16, 32, total_length, buffer);
      break;
    case FRV_OPERAND_ULO16 :
      errmsg = insert_normal (cd, fields->f_u16, 0, 0, 15, 16, 32, total_length, buffer);
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while building insn.\n"),
	       opindex);
      abort ();
  }

  return errmsg;
}

int frv_cgen_extract_operand
  PARAMS ((CGEN_CPU_DESC, int, CGEN_EXTRACT_INFO *, CGEN_INSN_INT,
           CGEN_FIELDS *, bfd_vma));

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
frv_cgen_extract_operand (cd, opindex, ex_info, insn_value, fields, pc)
     CGEN_CPU_DESC cd;
     int opindex;
     CGEN_EXTRACT_INFO *ex_info;
     CGEN_INSN_INT insn_value;
     CGEN_FIELDS * fields;
     bfd_vma pc;
{
  /* Assume success (for those operands that are nops).  */
  int length = 1;
  unsigned int total_length = CGEN_FIELDS_BITSIZE (fields);

  switch (opindex)
    {
    case FRV_OPERAND_A0 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 1, 32, total_length, pc, & fields->f_A);
      break;
    case FRV_OPERAND_A1 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 1, 32, total_length, pc, & fields->f_A);
      break;
    case FRV_OPERAND_ACC40SI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_ACC40Si);
      break;
    case FRV_OPERAND_ACC40SK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_ACC40Sk);
      break;
    case FRV_OPERAND_ACC40UI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_ACC40Ui);
      break;
    case FRV_OPERAND_ACC40UK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_ACC40Uk);
      break;
    case FRV_OPERAND_ACCGI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_ACCGi);
      break;
    case FRV_OPERAND_ACCGK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_ACCGk);
      break;
    case FRV_OPERAND_CCI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 3, 32, total_length, pc, & fields->f_CCi);
      break;
    case FRV_OPERAND_CPRDOUBLEK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_CPRk);
      break;
    case FRV_OPERAND_CPRI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_CPRi);
      break;
    case FRV_OPERAND_CPRJ :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 6, 32, total_length, pc, & fields->f_CPRj);
      break;
    case FRV_OPERAND_CPRK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_CPRk);
      break;
    case FRV_OPERAND_CRI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 14, 3, 32, total_length, pc, & fields->f_CRi);
      break;
    case FRV_OPERAND_CRJ :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 2, 3, 32, total_length, pc, & fields->f_CRj);
      break;
    case FRV_OPERAND_CRJ_FLOAT :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 26, 2, 32, total_length, pc, & fields->f_CRj_float);
      break;
    case FRV_OPERAND_CRJ_INT :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 26, 2, 32, total_length, pc, & value);
        value = ((value) + (4));
        fields->f_CRj_int = value;
      }
      break;
    case FRV_OPERAND_CRK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 27, 3, 32, total_length, pc, & fields->f_CRk);
      break;
    case FRV_OPERAND_FCCI_1 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 2, 32, total_length, pc, & fields->f_FCCi_1);
      break;
    case FRV_OPERAND_FCCI_2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 26, 2, 32, total_length, pc, & fields->f_FCCi_2);
      break;
    case FRV_OPERAND_FCCI_3 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 1, 2, 32, total_length, pc, & fields->f_FCCi_3);
      break;
    case FRV_OPERAND_FCCK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 26, 2, 32, total_length, pc, & fields->f_FCCk);
      break;
    case FRV_OPERAND_FRDOUBLEI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_FRi);
      break;
    case FRV_OPERAND_FRDOUBLEJ :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 6, 32, total_length, pc, & fields->f_FRj);
      break;
    case FRV_OPERAND_FRDOUBLEK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_FRi);
      break;
    case FRV_OPERAND_FRINTI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_FRi);
      break;
    case FRV_OPERAND_FRINTIEVEN :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_FRi);
      break;
    case FRV_OPERAND_FRINTJ :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 6, 32, total_length, pc, & fields->f_FRj);
      break;
    case FRV_OPERAND_FRINTJEVEN :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 6, 32, total_length, pc, & fields->f_FRj);
      break;
    case FRV_OPERAND_FRINTK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRINTKEVEN :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRJ :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 6, 32, total_length, pc, & fields->f_FRj);
      break;
    case FRV_OPERAND_FRK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRKHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRKLO :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_FRk);
      break;
    case FRV_OPERAND_GRDOUBLEK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_GRk);
      break;
    case FRV_OPERAND_GRI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_GRi);
      break;
    case FRV_OPERAND_GRJ :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 6, 32, total_length, pc, & fields->f_GRj);
      break;
    case FRV_OPERAND_GRK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_GRk);
      break;
    case FRV_OPERAND_GRKHI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_GRk);
      break;
    case FRV_OPERAND_GRKLO :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_GRk);
      break;
    case FRV_OPERAND_ICCI_1 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 11, 2, 32, total_length, pc, & fields->f_ICCi_1);
      break;
    case FRV_OPERAND_ICCI_2 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 26, 2, 32, total_length, pc, & fields->f_ICCi_2);
      break;
    case FRV_OPERAND_ICCI_3 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 1, 2, 32, total_length, pc, & fields->f_ICCi_3);
      break;
    case FRV_OPERAND_LI :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 1, 32, total_length, pc, & fields->f_LI);
      break;
    case FRV_OPERAND_AE :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 1, 32, total_length, pc, & fields->f_ae);
      break;
    case FRV_OPERAND_CCOND :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 12, 1, 32, total_length, pc, & fields->f_ccond);
      break;
    case FRV_OPERAND_COND :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 8, 1, 32, total_length, pc, & fields->f_cond);
      break;
    case FRV_OPERAND_D12 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 11, 12, 32, total_length, pc, & fields->f_d12);
      break;
    case FRV_OPERAND_DEBUG :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 1, 32, total_length, pc, & fields->f_debug);
      break;
    case FRV_OPERAND_EIR :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_eir);
      break;
    case FRV_OPERAND_HINT :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 2, 32, total_length, pc, & fields->f_hint);
      break;
    case FRV_OPERAND_HINT_NOT_TAKEN :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 2, 32, total_length, pc, & fields->f_hint);
      break;
    case FRV_OPERAND_HINT_TAKEN :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 2, 32, total_length, pc, & fields->f_hint);
      break;
    case FRV_OPERAND_LABEL16 :
      {
        long value;
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED)|(1<<CGEN_IFLD_PCREL_ADDR), 0, 15, 16, 32, total_length, pc, & value);
        value = ((((value) << (2))) + (pc));
        fields->f_label16 = value;
      }
      break;
    case FRV_OPERAND_LABEL24 :
      {
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 30, 6, 32, total_length, pc, & fields->f_labelH6);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 18, 32, total_length, pc, & fields->f_labelL18);
        if (length <= 0) break;
{
  FLD (f_label24) = ((((((((FLD (f_labelH6)) << (18))) | (FLD (f_labelL18)))) << (2))) + (pc));
}
      }
      break;
    case FRV_OPERAND_LOCK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 25, 1, 32, total_length, pc, & fields->f_lock);
      break;
    case FRV_OPERAND_PACK :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 31, 1, 32, total_length, pc, & fields->f_pack);
      break;
    case FRV_OPERAND_S10 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 9, 10, 32, total_length, pc, & fields->f_s10);
      break;
    case FRV_OPERAND_S12 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 11, 12, 32, total_length, pc, & fields->f_d12);
      break;
    case FRV_OPERAND_S16 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 15, 16, 32, total_length, pc, & fields->f_s16);
      break;
    case FRV_OPERAND_S5 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 4, 5, 32, total_length, pc, & fields->f_s5);
      break;
    case FRV_OPERAND_S6 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 5, 6, 32, total_length, pc, & fields->f_s6);
      break;
    case FRV_OPERAND_S6_1 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 11, 6, 32, total_length, pc, & fields->f_s6_1);
      break;
    case FRV_OPERAND_SLO16 :
      length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 15, 16, 32, total_length, pc, & fields->f_s16);
      break;
    case FRV_OPERAND_SPR :
      {
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 30, 6, 32, total_length, pc, & fields->f_spr_h);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 17, 6, 32, total_length, pc, & fields->f_spr_l);
        if (length <= 0) break;
{
  FLD (f_spr) = ((((FLD (f_spr_h)) << (6))) | (FLD (f_spr_l)));
}
      }
      break;
    case FRV_OPERAND_U12 :
      {
        length = extract_normal (cd, ex_info, insn_value, 0|(1<<CGEN_IFLD_SIGNED), 0, 17, 6, 32, total_length, pc, & fields->f_u12_h);
        if (length <= 0) break;
        length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 6, 32, total_length, pc, & fields->f_u12_l);
        if (length <= 0) break;
{
  FLD (f_u12) = ((((FLD (f_u12_h)) << (6))) | (FLD (f_u12_l)));
}
      }
      break;
    case FRV_OPERAND_U16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 16, 32, total_length, pc, & fields->f_u16);
      break;
    case FRV_OPERAND_U6 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 5, 6, 32, total_length, pc, & fields->f_u6);
      break;
    case FRV_OPERAND_UHI16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 16, 32, total_length, pc, & fields->f_u16);
      break;
    case FRV_OPERAND_ULO16 :
      length = extract_normal (cd, ex_info, insn_value, 0, 0, 15, 16, 32, total_length, pc, & fields->f_u16);
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while decoding insn.\n"),
	       opindex);
      abort ();
    }

  return length;
}

cgen_insert_fn * const frv_cgen_insert_handlers[] = 
{
  insert_insn_normal,
};

cgen_extract_fn * const frv_cgen_extract_handlers[] = 
{
  extract_insn_normal,
};

int frv_cgen_get_int_operand
  PARAMS ((CGEN_CPU_DESC, int, const CGEN_FIELDS *));
bfd_vma frv_cgen_get_vma_operand
  PARAMS ((CGEN_CPU_DESC, int, const CGEN_FIELDS *));

/* Getting values from cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they return.
   TODO: floating point, inlining support, remove cases where result type
   not appropriate.  */

int
frv_cgen_get_int_operand (cd, opindex, fields)
     CGEN_CPU_DESC cd ATTRIBUTE_UNUSED;
     int opindex;
     const CGEN_FIELDS * fields;
{
  int value;

  switch (opindex)
    {
    case FRV_OPERAND_A0 :
      value = fields->f_A;
      break;
    case FRV_OPERAND_A1 :
      value = fields->f_A;
      break;
    case FRV_OPERAND_ACC40SI :
      value = fields->f_ACC40Si;
      break;
    case FRV_OPERAND_ACC40SK :
      value = fields->f_ACC40Sk;
      break;
    case FRV_OPERAND_ACC40UI :
      value = fields->f_ACC40Ui;
      break;
    case FRV_OPERAND_ACC40UK :
      value = fields->f_ACC40Uk;
      break;
    case FRV_OPERAND_ACCGI :
      value = fields->f_ACCGi;
      break;
    case FRV_OPERAND_ACCGK :
      value = fields->f_ACCGk;
      break;
    case FRV_OPERAND_CCI :
      value = fields->f_CCi;
      break;
    case FRV_OPERAND_CPRDOUBLEK :
      value = fields->f_CPRk;
      break;
    case FRV_OPERAND_CPRI :
      value = fields->f_CPRi;
      break;
    case FRV_OPERAND_CPRJ :
      value = fields->f_CPRj;
      break;
    case FRV_OPERAND_CPRK :
      value = fields->f_CPRk;
      break;
    case FRV_OPERAND_CRI :
      value = fields->f_CRi;
      break;
    case FRV_OPERAND_CRJ :
      value = fields->f_CRj;
      break;
    case FRV_OPERAND_CRJ_FLOAT :
      value = fields->f_CRj_float;
      break;
    case FRV_OPERAND_CRJ_INT :
      value = fields->f_CRj_int;
      break;
    case FRV_OPERAND_CRK :
      value = fields->f_CRk;
      break;
    case FRV_OPERAND_FCCI_1 :
      value = fields->f_FCCi_1;
      break;
    case FRV_OPERAND_FCCI_2 :
      value = fields->f_FCCi_2;
      break;
    case FRV_OPERAND_FCCI_3 :
      value = fields->f_FCCi_3;
      break;
    case FRV_OPERAND_FCCK :
      value = fields->f_FCCk;
      break;
    case FRV_OPERAND_FRDOUBLEI :
      value = fields->f_FRi;
      break;
    case FRV_OPERAND_FRDOUBLEJ :
      value = fields->f_FRj;
      break;
    case FRV_OPERAND_FRDOUBLEK :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRI :
      value = fields->f_FRi;
      break;
    case FRV_OPERAND_FRINTI :
      value = fields->f_FRi;
      break;
    case FRV_OPERAND_FRINTIEVEN :
      value = fields->f_FRi;
      break;
    case FRV_OPERAND_FRINTJ :
      value = fields->f_FRj;
      break;
    case FRV_OPERAND_FRINTJEVEN :
      value = fields->f_FRj;
      break;
    case FRV_OPERAND_FRINTK :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRINTKEVEN :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRJ :
      value = fields->f_FRj;
      break;
    case FRV_OPERAND_FRK :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRKHI :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRKLO :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_GRDOUBLEK :
      value = fields->f_GRk;
      break;
    case FRV_OPERAND_GRI :
      value = fields->f_GRi;
      break;
    case FRV_OPERAND_GRJ :
      value = fields->f_GRj;
      break;
    case FRV_OPERAND_GRK :
      value = fields->f_GRk;
      break;
    case FRV_OPERAND_GRKHI :
      value = fields->f_GRk;
      break;
    case FRV_OPERAND_GRKLO :
      value = fields->f_GRk;
      break;
    case FRV_OPERAND_ICCI_1 :
      value = fields->f_ICCi_1;
      break;
    case FRV_OPERAND_ICCI_2 :
      value = fields->f_ICCi_2;
      break;
    case FRV_OPERAND_ICCI_3 :
      value = fields->f_ICCi_3;
      break;
    case FRV_OPERAND_LI :
      value = fields->f_LI;
      break;
    case FRV_OPERAND_AE :
      value = fields->f_ae;
      break;
    case FRV_OPERAND_CCOND :
      value = fields->f_ccond;
      break;
    case FRV_OPERAND_COND :
      value = fields->f_cond;
      break;
    case FRV_OPERAND_D12 :
      value = fields->f_d12;
      break;
    case FRV_OPERAND_DEBUG :
      value = fields->f_debug;
      break;
    case FRV_OPERAND_EIR :
      value = fields->f_eir;
      break;
    case FRV_OPERAND_HINT :
      value = fields->f_hint;
      break;
    case FRV_OPERAND_HINT_NOT_TAKEN :
      value = fields->f_hint;
      break;
    case FRV_OPERAND_HINT_TAKEN :
      value = fields->f_hint;
      break;
    case FRV_OPERAND_LABEL16 :
      value = fields->f_label16;
      break;
    case FRV_OPERAND_LABEL24 :
      value = fields->f_label24;
      break;
    case FRV_OPERAND_LOCK :
      value = fields->f_lock;
      break;
    case FRV_OPERAND_PACK :
      value = fields->f_pack;
      break;
    case FRV_OPERAND_S10 :
      value = fields->f_s10;
      break;
    case FRV_OPERAND_S12 :
      value = fields->f_d12;
      break;
    case FRV_OPERAND_S16 :
      value = fields->f_s16;
      break;
    case FRV_OPERAND_S5 :
      value = fields->f_s5;
      break;
    case FRV_OPERAND_S6 :
      value = fields->f_s6;
      break;
    case FRV_OPERAND_S6_1 :
      value = fields->f_s6_1;
      break;
    case FRV_OPERAND_SLO16 :
      value = fields->f_s16;
      break;
    case FRV_OPERAND_SPR :
      value = fields->f_spr;
      break;
    case FRV_OPERAND_U12 :
      value = fields->f_u12;
      break;
    case FRV_OPERAND_U16 :
      value = fields->f_u16;
      break;
    case FRV_OPERAND_U6 :
      value = fields->f_u6;
      break;
    case FRV_OPERAND_UHI16 :
      value = fields->f_u16;
      break;
    case FRV_OPERAND_ULO16 :
      value = fields->f_u16;
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
frv_cgen_get_vma_operand (cd, opindex, fields)
     CGEN_CPU_DESC cd ATTRIBUTE_UNUSED;
     int opindex;
     const CGEN_FIELDS * fields;
{
  bfd_vma value;

  switch (opindex)
    {
    case FRV_OPERAND_A0 :
      value = fields->f_A;
      break;
    case FRV_OPERAND_A1 :
      value = fields->f_A;
      break;
    case FRV_OPERAND_ACC40SI :
      value = fields->f_ACC40Si;
      break;
    case FRV_OPERAND_ACC40SK :
      value = fields->f_ACC40Sk;
      break;
    case FRV_OPERAND_ACC40UI :
      value = fields->f_ACC40Ui;
      break;
    case FRV_OPERAND_ACC40UK :
      value = fields->f_ACC40Uk;
      break;
    case FRV_OPERAND_ACCGI :
      value = fields->f_ACCGi;
      break;
    case FRV_OPERAND_ACCGK :
      value = fields->f_ACCGk;
      break;
    case FRV_OPERAND_CCI :
      value = fields->f_CCi;
      break;
    case FRV_OPERAND_CPRDOUBLEK :
      value = fields->f_CPRk;
      break;
    case FRV_OPERAND_CPRI :
      value = fields->f_CPRi;
      break;
    case FRV_OPERAND_CPRJ :
      value = fields->f_CPRj;
      break;
    case FRV_OPERAND_CPRK :
      value = fields->f_CPRk;
      break;
    case FRV_OPERAND_CRI :
      value = fields->f_CRi;
      break;
    case FRV_OPERAND_CRJ :
      value = fields->f_CRj;
      break;
    case FRV_OPERAND_CRJ_FLOAT :
      value = fields->f_CRj_float;
      break;
    case FRV_OPERAND_CRJ_INT :
      value = fields->f_CRj_int;
      break;
    case FRV_OPERAND_CRK :
      value = fields->f_CRk;
      break;
    case FRV_OPERAND_FCCI_1 :
      value = fields->f_FCCi_1;
      break;
    case FRV_OPERAND_FCCI_2 :
      value = fields->f_FCCi_2;
      break;
    case FRV_OPERAND_FCCI_3 :
      value = fields->f_FCCi_3;
      break;
    case FRV_OPERAND_FCCK :
      value = fields->f_FCCk;
      break;
    case FRV_OPERAND_FRDOUBLEI :
      value = fields->f_FRi;
      break;
    case FRV_OPERAND_FRDOUBLEJ :
      value = fields->f_FRj;
      break;
    case FRV_OPERAND_FRDOUBLEK :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRI :
      value = fields->f_FRi;
      break;
    case FRV_OPERAND_FRINTI :
      value = fields->f_FRi;
      break;
    case FRV_OPERAND_FRINTIEVEN :
      value = fields->f_FRi;
      break;
    case FRV_OPERAND_FRINTJ :
      value = fields->f_FRj;
      break;
    case FRV_OPERAND_FRINTJEVEN :
      value = fields->f_FRj;
      break;
    case FRV_OPERAND_FRINTK :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRINTKEVEN :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRJ :
      value = fields->f_FRj;
      break;
    case FRV_OPERAND_FRK :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRKHI :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_FRKLO :
      value = fields->f_FRk;
      break;
    case FRV_OPERAND_GRDOUBLEK :
      value = fields->f_GRk;
      break;
    case FRV_OPERAND_GRI :
      value = fields->f_GRi;
      break;
    case FRV_OPERAND_GRJ :
      value = fields->f_GRj;
      break;
    case FRV_OPERAND_GRK :
      value = fields->f_GRk;
      break;
    case FRV_OPERAND_GRKHI :
      value = fields->f_GRk;
      break;
    case FRV_OPERAND_GRKLO :
      value = fields->f_GRk;
      break;
    case FRV_OPERAND_ICCI_1 :
      value = fields->f_ICCi_1;
      break;
    case FRV_OPERAND_ICCI_2 :
      value = fields->f_ICCi_2;
      break;
    case FRV_OPERAND_ICCI_3 :
      value = fields->f_ICCi_3;
      break;
    case FRV_OPERAND_LI :
      value = fields->f_LI;
      break;
    case FRV_OPERAND_AE :
      value = fields->f_ae;
      break;
    case FRV_OPERAND_CCOND :
      value = fields->f_ccond;
      break;
    case FRV_OPERAND_COND :
      value = fields->f_cond;
      break;
    case FRV_OPERAND_D12 :
      value = fields->f_d12;
      break;
    case FRV_OPERAND_DEBUG :
      value = fields->f_debug;
      break;
    case FRV_OPERAND_EIR :
      value = fields->f_eir;
      break;
    case FRV_OPERAND_HINT :
      value = fields->f_hint;
      break;
    case FRV_OPERAND_HINT_NOT_TAKEN :
      value = fields->f_hint;
      break;
    case FRV_OPERAND_HINT_TAKEN :
      value = fields->f_hint;
      break;
    case FRV_OPERAND_LABEL16 :
      value = fields->f_label16;
      break;
    case FRV_OPERAND_LABEL24 :
      value = fields->f_label24;
      break;
    case FRV_OPERAND_LOCK :
      value = fields->f_lock;
      break;
    case FRV_OPERAND_PACK :
      value = fields->f_pack;
      break;
    case FRV_OPERAND_S10 :
      value = fields->f_s10;
      break;
    case FRV_OPERAND_S12 :
      value = fields->f_d12;
      break;
    case FRV_OPERAND_S16 :
      value = fields->f_s16;
      break;
    case FRV_OPERAND_S5 :
      value = fields->f_s5;
      break;
    case FRV_OPERAND_S6 :
      value = fields->f_s6;
      break;
    case FRV_OPERAND_S6_1 :
      value = fields->f_s6_1;
      break;
    case FRV_OPERAND_SLO16 :
      value = fields->f_s16;
      break;
    case FRV_OPERAND_SPR :
      value = fields->f_spr;
      break;
    case FRV_OPERAND_U12 :
      value = fields->f_u12;
      break;
    case FRV_OPERAND_U16 :
      value = fields->f_u16;
      break;
    case FRV_OPERAND_U6 :
      value = fields->f_u6;
      break;
    case FRV_OPERAND_UHI16 :
      value = fields->f_u16;
      break;
    case FRV_OPERAND_ULO16 :
      value = fields->f_u16;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while getting vma operand.\n"),
		       opindex);
      abort ();
  }

  return value;
}

void frv_cgen_set_int_operand
  PARAMS ((CGEN_CPU_DESC, int, CGEN_FIELDS *, int));
void frv_cgen_set_vma_operand
  PARAMS ((CGEN_CPU_DESC, int, CGEN_FIELDS *, bfd_vma));

/* Stuffing values in cgen_fields is handled by a collection of functions.
   They are distinguished by the type of the VALUE argument they accept.
   TODO: floating point, inlining support, remove cases where argument type
   not appropriate.  */

void
frv_cgen_set_int_operand (cd, opindex, fields, value)
     CGEN_CPU_DESC cd ATTRIBUTE_UNUSED;
     int opindex;
     CGEN_FIELDS * fields;
     int value;
{
  switch (opindex)
    {
    case FRV_OPERAND_A0 :
      fields->f_A = value;
      break;
    case FRV_OPERAND_A1 :
      fields->f_A = value;
      break;
    case FRV_OPERAND_ACC40SI :
      fields->f_ACC40Si = value;
      break;
    case FRV_OPERAND_ACC40SK :
      fields->f_ACC40Sk = value;
      break;
    case FRV_OPERAND_ACC40UI :
      fields->f_ACC40Ui = value;
      break;
    case FRV_OPERAND_ACC40UK :
      fields->f_ACC40Uk = value;
      break;
    case FRV_OPERAND_ACCGI :
      fields->f_ACCGi = value;
      break;
    case FRV_OPERAND_ACCGK :
      fields->f_ACCGk = value;
      break;
    case FRV_OPERAND_CCI :
      fields->f_CCi = value;
      break;
    case FRV_OPERAND_CPRDOUBLEK :
      fields->f_CPRk = value;
      break;
    case FRV_OPERAND_CPRI :
      fields->f_CPRi = value;
      break;
    case FRV_OPERAND_CPRJ :
      fields->f_CPRj = value;
      break;
    case FRV_OPERAND_CPRK :
      fields->f_CPRk = value;
      break;
    case FRV_OPERAND_CRI :
      fields->f_CRi = value;
      break;
    case FRV_OPERAND_CRJ :
      fields->f_CRj = value;
      break;
    case FRV_OPERAND_CRJ_FLOAT :
      fields->f_CRj_float = value;
      break;
    case FRV_OPERAND_CRJ_INT :
      fields->f_CRj_int = value;
      break;
    case FRV_OPERAND_CRK :
      fields->f_CRk = value;
      break;
    case FRV_OPERAND_FCCI_1 :
      fields->f_FCCi_1 = value;
      break;
    case FRV_OPERAND_FCCI_2 :
      fields->f_FCCi_2 = value;
      break;
    case FRV_OPERAND_FCCI_3 :
      fields->f_FCCi_3 = value;
      break;
    case FRV_OPERAND_FCCK :
      fields->f_FCCk = value;
      break;
    case FRV_OPERAND_FRDOUBLEI :
      fields->f_FRi = value;
      break;
    case FRV_OPERAND_FRDOUBLEJ :
      fields->f_FRj = value;
      break;
    case FRV_OPERAND_FRDOUBLEK :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRI :
      fields->f_FRi = value;
      break;
    case FRV_OPERAND_FRINTI :
      fields->f_FRi = value;
      break;
    case FRV_OPERAND_FRINTIEVEN :
      fields->f_FRi = value;
      break;
    case FRV_OPERAND_FRINTJ :
      fields->f_FRj = value;
      break;
    case FRV_OPERAND_FRINTJEVEN :
      fields->f_FRj = value;
      break;
    case FRV_OPERAND_FRINTK :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRINTKEVEN :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRJ :
      fields->f_FRj = value;
      break;
    case FRV_OPERAND_FRK :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRKHI :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRKLO :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_GRDOUBLEK :
      fields->f_GRk = value;
      break;
    case FRV_OPERAND_GRI :
      fields->f_GRi = value;
      break;
    case FRV_OPERAND_GRJ :
      fields->f_GRj = value;
      break;
    case FRV_OPERAND_GRK :
      fields->f_GRk = value;
      break;
    case FRV_OPERAND_GRKHI :
      fields->f_GRk = value;
      break;
    case FRV_OPERAND_GRKLO :
      fields->f_GRk = value;
      break;
    case FRV_OPERAND_ICCI_1 :
      fields->f_ICCi_1 = value;
      break;
    case FRV_OPERAND_ICCI_2 :
      fields->f_ICCi_2 = value;
      break;
    case FRV_OPERAND_ICCI_3 :
      fields->f_ICCi_3 = value;
      break;
    case FRV_OPERAND_LI :
      fields->f_LI = value;
      break;
    case FRV_OPERAND_AE :
      fields->f_ae = value;
      break;
    case FRV_OPERAND_CCOND :
      fields->f_ccond = value;
      break;
    case FRV_OPERAND_COND :
      fields->f_cond = value;
      break;
    case FRV_OPERAND_D12 :
      fields->f_d12 = value;
      break;
    case FRV_OPERAND_DEBUG :
      fields->f_debug = value;
      break;
    case FRV_OPERAND_EIR :
      fields->f_eir = value;
      break;
    case FRV_OPERAND_HINT :
      fields->f_hint = value;
      break;
    case FRV_OPERAND_HINT_NOT_TAKEN :
      fields->f_hint = value;
      break;
    case FRV_OPERAND_HINT_TAKEN :
      fields->f_hint = value;
      break;
    case FRV_OPERAND_LABEL16 :
      fields->f_label16 = value;
      break;
    case FRV_OPERAND_LABEL24 :
      fields->f_label24 = value;
      break;
    case FRV_OPERAND_LOCK :
      fields->f_lock = value;
      break;
    case FRV_OPERAND_PACK :
      fields->f_pack = value;
      break;
    case FRV_OPERAND_S10 :
      fields->f_s10 = value;
      break;
    case FRV_OPERAND_S12 :
      fields->f_d12 = value;
      break;
    case FRV_OPERAND_S16 :
      fields->f_s16 = value;
      break;
    case FRV_OPERAND_S5 :
      fields->f_s5 = value;
      break;
    case FRV_OPERAND_S6 :
      fields->f_s6 = value;
      break;
    case FRV_OPERAND_S6_1 :
      fields->f_s6_1 = value;
      break;
    case FRV_OPERAND_SLO16 :
      fields->f_s16 = value;
      break;
    case FRV_OPERAND_SPR :
      fields->f_spr = value;
      break;
    case FRV_OPERAND_U12 :
      fields->f_u12 = value;
      break;
    case FRV_OPERAND_U16 :
      fields->f_u16 = value;
      break;
    case FRV_OPERAND_U6 :
      fields->f_u6 = value;
      break;
    case FRV_OPERAND_UHI16 :
      fields->f_u16 = value;
      break;
    case FRV_OPERAND_ULO16 :
      fields->f_u16 = value;
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while setting int operand.\n"),
		       opindex);
      abort ();
  }
}

void
frv_cgen_set_vma_operand (cd, opindex, fields, value)
     CGEN_CPU_DESC cd ATTRIBUTE_UNUSED;
     int opindex;
     CGEN_FIELDS * fields;
     bfd_vma value;
{
  switch (opindex)
    {
    case FRV_OPERAND_A0 :
      fields->f_A = value;
      break;
    case FRV_OPERAND_A1 :
      fields->f_A = value;
      break;
    case FRV_OPERAND_ACC40SI :
      fields->f_ACC40Si = value;
      break;
    case FRV_OPERAND_ACC40SK :
      fields->f_ACC40Sk = value;
      break;
    case FRV_OPERAND_ACC40UI :
      fields->f_ACC40Ui = value;
      break;
    case FRV_OPERAND_ACC40UK :
      fields->f_ACC40Uk = value;
      break;
    case FRV_OPERAND_ACCGI :
      fields->f_ACCGi = value;
      break;
    case FRV_OPERAND_ACCGK :
      fields->f_ACCGk = value;
      break;
    case FRV_OPERAND_CCI :
      fields->f_CCi = value;
      break;
    case FRV_OPERAND_CPRDOUBLEK :
      fields->f_CPRk = value;
      break;
    case FRV_OPERAND_CPRI :
      fields->f_CPRi = value;
      break;
    case FRV_OPERAND_CPRJ :
      fields->f_CPRj = value;
      break;
    case FRV_OPERAND_CPRK :
      fields->f_CPRk = value;
      break;
    case FRV_OPERAND_CRI :
      fields->f_CRi = value;
      break;
    case FRV_OPERAND_CRJ :
      fields->f_CRj = value;
      break;
    case FRV_OPERAND_CRJ_FLOAT :
      fields->f_CRj_float = value;
      break;
    case FRV_OPERAND_CRJ_INT :
      fields->f_CRj_int = value;
      break;
    case FRV_OPERAND_CRK :
      fields->f_CRk = value;
      break;
    case FRV_OPERAND_FCCI_1 :
      fields->f_FCCi_1 = value;
      break;
    case FRV_OPERAND_FCCI_2 :
      fields->f_FCCi_2 = value;
      break;
    case FRV_OPERAND_FCCI_3 :
      fields->f_FCCi_3 = value;
      break;
    case FRV_OPERAND_FCCK :
      fields->f_FCCk = value;
      break;
    case FRV_OPERAND_FRDOUBLEI :
      fields->f_FRi = value;
      break;
    case FRV_OPERAND_FRDOUBLEJ :
      fields->f_FRj = value;
      break;
    case FRV_OPERAND_FRDOUBLEK :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRI :
      fields->f_FRi = value;
      break;
    case FRV_OPERAND_FRINTI :
      fields->f_FRi = value;
      break;
    case FRV_OPERAND_FRINTIEVEN :
      fields->f_FRi = value;
      break;
    case FRV_OPERAND_FRINTJ :
      fields->f_FRj = value;
      break;
    case FRV_OPERAND_FRINTJEVEN :
      fields->f_FRj = value;
      break;
    case FRV_OPERAND_FRINTK :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRINTKEVEN :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRJ :
      fields->f_FRj = value;
      break;
    case FRV_OPERAND_FRK :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRKHI :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_FRKLO :
      fields->f_FRk = value;
      break;
    case FRV_OPERAND_GRDOUBLEK :
      fields->f_GRk = value;
      break;
    case FRV_OPERAND_GRI :
      fields->f_GRi = value;
      break;
    case FRV_OPERAND_GRJ :
      fields->f_GRj = value;
      break;
    case FRV_OPERAND_GRK :
      fields->f_GRk = value;
      break;
    case FRV_OPERAND_GRKHI :
      fields->f_GRk = value;
      break;
    case FRV_OPERAND_GRKLO :
      fields->f_GRk = value;
      break;
    case FRV_OPERAND_ICCI_1 :
      fields->f_ICCi_1 = value;
      break;
    case FRV_OPERAND_ICCI_2 :
      fields->f_ICCi_2 = value;
      break;
    case FRV_OPERAND_ICCI_3 :
      fields->f_ICCi_3 = value;
      break;
    case FRV_OPERAND_LI :
      fields->f_LI = value;
      break;
    case FRV_OPERAND_AE :
      fields->f_ae = value;
      break;
    case FRV_OPERAND_CCOND :
      fields->f_ccond = value;
      break;
    case FRV_OPERAND_COND :
      fields->f_cond = value;
      break;
    case FRV_OPERAND_D12 :
      fields->f_d12 = value;
      break;
    case FRV_OPERAND_DEBUG :
      fields->f_debug = value;
      break;
    case FRV_OPERAND_EIR :
      fields->f_eir = value;
      break;
    case FRV_OPERAND_HINT :
      fields->f_hint = value;
      break;
    case FRV_OPERAND_HINT_NOT_TAKEN :
      fields->f_hint = value;
      break;
    case FRV_OPERAND_HINT_TAKEN :
      fields->f_hint = value;
      break;
    case FRV_OPERAND_LABEL16 :
      fields->f_label16 = value;
      break;
    case FRV_OPERAND_LABEL24 :
      fields->f_label24 = value;
      break;
    case FRV_OPERAND_LOCK :
      fields->f_lock = value;
      break;
    case FRV_OPERAND_PACK :
      fields->f_pack = value;
      break;
    case FRV_OPERAND_S10 :
      fields->f_s10 = value;
      break;
    case FRV_OPERAND_S12 :
      fields->f_d12 = value;
      break;
    case FRV_OPERAND_S16 :
      fields->f_s16 = value;
      break;
    case FRV_OPERAND_S5 :
      fields->f_s5 = value;
      break;
    case FRV_OPERAND_S6 :
      fields->f_s6 = value;
      break;
    case FRV_OPERAND_S6_1 :
      fields->f_s6_1 = value;
      break;
    case FRV_OPERAND_SLO16 :
      fields->f_s16 = value;
      break;
    case FRV_OPERAND_SPR :
      fields->f_spr = value;
      break;
    case FRV_OPERAND_U12 :
      fields->f_u12 = value;
      break;
    case FRV_OPERAND_U16 :
      fields->f_u16 = value;
      break;
    case FRV_OPERAND_U6 :
      fields->f_u6 = value;
      break;
    case FRV_OPERAND_UHI16 :
      fields->f_u16 = value;
      break;
    case FRV_OPERAND_ULO16 :
      fields->f_u16 = value;
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
frv_cgen_init_ibld_table (cd)
     CGEN_CPU_DESC cd;
{
  cd->insert_handlers = & frv_cgen_insert_handlers[0];
  cd->extract_handlers = & frv_cgen_extract_handlers[0];

  cd->insert_operand = frv_cgen_insert_operand;
  cd->extract_operand = frv_cgen_extract_operand;

  cd->get_int_operand = frv_cgen_get_int_operand;
  cd->set_int_operand = frv_cgen_set_int_operand;
  cd->get_vma_operand = frv_cgen_get_vma_operand;
  cd->set_vma_operand = frv_cgen_set_vma_operand;
}
