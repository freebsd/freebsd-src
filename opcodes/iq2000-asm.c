/* Assembler interface for targets using CGEN. -*- C -*-
   CGEN: Cpu tools GENerator

THIS FILE IS MACHINE GENERATED WITH CGEN.
- the resultant file is machine generated, cgen-asm.in isn't

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
#include "bfd.h"
#include "symcat.h"
#include "iq2000-desc.h"
#include "iq2000-opc.h"
#include "opintl.h"
#include "xregex.h"
#include "libiberty.h"
#include "safe-ctype.h"

#undef  min
#define min(a,b) ((a) < (b) ? (a) : (b))
#undef  max
#define max(a,b) ((a) > (b) ? (a) : (b))

static const char * parse_insn_normal
  (CGEN_CPU_DESC, const CGEN_INSN *, const char **, CGEN_FIELDS *);

/* -- assembler routines inserted here.  */

/* -- asm.c */
static const char * parse_mimm PARAMS ((CGEN_CPU_DESC, const char **, int, long *));
static const char * parse_imm  PARAMS ((CGEN_CPU_DESC, const char **, int, unsigned long *));
static const char * parse_hi16 PARAMS ((CGEN_CPU_DESC, const char **, int, unsigned long *));
static const char * parse_lo16 PARAMS ((CGEN_CPU_DESC, const char **, int, long *));

/* Special check to ensure that instruction exists for given machine */
int
iq2000_cgen_insn_supported (cd, insn)
     CGEN_CPU_DESC cd;
     CGEN_INSN *insn;
{
  int machs = cd->machs;

  return ((CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_MACH) & machs) != 0);
}

static int iq2000_cgen_isa_register (strp)
     const char **strp;
{
  int len;
  int ch1, ch2;
  if (**strp == 'r' || **strp == 'R') 
    {
      len = strlen (*strp);
      if (len == 2) 
        {
          ch1 = (*strp)[1];
          if ('0' <= ch1 && ch1 <= '9')
            return 1;
        } 
      else if (len == 3) 
        {
	  ch1 = (*strp)[1];
          ch2 = (*strp)[2];
          if (('1' <= ch1 && ch1 <= '2') && ('0' <= ch2 && ch2 <= '9'))
            return 1;
          if ('3' == ch1 && (ch2 == '0' || ch2 == '1'))
            return 1;
        }
    }
  if (**strp == '%' && tolower((*strp)[1]) != 'l' && tolower((*strp)[1]) != 'h')
    return 1;
  return 0;
}

/* Handle negated literal.  */

static const char *
parse_mimm (cd, strp, opindex, valuep)
     CGEN_CPU_DESC cd;
     const char **strp;
     int opindex;
     long *valuep;
{
  const char *errmsg;
  long value;

  /* Verify this isn't a register */
  if (iq2000_cgen_isa_register (strp))
    errmsg = _("immediate value cannot be register");
  else
    {
      long value;
      
      errmsg = cgen_parse_signed_integer (cd, strp, opindex, & value);
      if (errmsg == NULL)
	{
	  long x = (-value) & 0xFFFF0000;
	  if (x != 0 && x != 0xFFFF0000)
	    errmsg = _("immediate value out of range");
	  else
	    *valuep = (-value & 0xFFFF);
	}
    }
  return errmsg;
}

/* Handle signed/unsigned literal.  */

static const char *
parse_imm (cd, strp, opindex, valuep)
     CGEN_CPU_DESC cd;
     const char **strp;
     int opindex;
     unsigned long *valuep;
{
  const char *errmsg;
  long value;

  if (iq2000_cgen_isa_register (strp))
    errmsg = _("immediate value cannot be register");
  else
    {
      long value;

      errmsg = cgen_parse_signed_integer (cd, strp, opindex, & value);
      if (errmsg == NULL)
	{
	  long x = value & 0xFFFF0000;
	  if (x != 0 && x != 0xFFFF0000)
	    errmsg = _("immediate value out of range");
	  else
	    *valuep = (value & 0xFFFF);
	}
    }
  return errmsg;
}

/* Handle iq10 21-bit jmp offset.  */

static const char *
parse_jtargq10 (cd, strp, opindex, reloc, type_addr, valuep)
     CGEN_CPU_DESC cd;
     const char **strp;
     int opindex;
     int reloc;
     enum cgen_parse_operand_result *type_addr;
     unsigned long *valuep;
{
  const char *errmsg;
  bfd_vma value;
  enum cgen_parse_operand_result result_type = CGEN_PARSE_OPERAND_RESULT_NUMBER;

  errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_IQ2000_OFFSET_21,
			       &result_type, &value);
  if (errmsg == NULL && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
    {
      /* check value is within 23-bits (remembering that 2-bit shift right will occur) */
      if (value > 0x7fffff)
        return _("21-bit offset out of range");
    }
  *valuep = (value & 0x7FFFFF);
  return errmsg;
}

/* Handle high().  */

static const char *
parse_hi16 (cd, strp, opindex, valuep)
     CGEN_CPU_DESC cd;
     const char **strp;
     int opindex;
     unsigned long *valuep;
{
  if (strncasecmp (*strp, "%hi(", 4) == 0)
    {
      enum cgen_parse_operand_result result_type;
      bfd_vma value;
      const char *errmsg;

      *strp += 4;
      errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_HI16,
				   &result_type, &value);
      if (**strp != ')')
	return _("missing `)'");

      ++*strp;
      if (errmsg == NULL
  	  && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  /* if value has top-bit of %lo on, then it will
	     sign-propagate and so we compensate by adding
	     1 to the resultant %hi value */
	  if (value & 0x8000)
	    value += 0x10000;
	  value >>= 16;
	}
      *valuep = value;

      return errmsg;
    }

  /* we add %uhi in case a user just wants the high 16-bits or is using
     an insn like ori for %lo which does not sign-propagate */
  if (strncasecmp (*strp, "%uhi(", 5) == 0)
    {
      enum cgen_parse_operand_result result_type;
      bfd_vma value;
      const char *errmsg;

      *strp += 5;
      errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_IQ2000_UHI16,
				   &result_type, &value);
      if (**strp != ')')
	return _("missing `)'");

      ++*strp;
      if (errmsg == NULL
  	  && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  value >>= 16;
	}
      *valuep = value;

      return errmsg;
    }

  return parse_imm (cd, strp, opindex, valuep);
}

/* Handle %lo in a signed context.
   The signedness of the value doesn't matter to %lo(), but this also
   handles the case where %lo() isn't present.  */

static const char *
parse_lo16 (cd, strp, opindex, valuep)
     CGEN_CPU_DESC cd;
     const char **strp;
     int opindex;
     long *valuep;
{
  if (strncasecmp (*strp, "%lo(", 4) == 0)
    {
      const char *errmsg;
      enum cgen_parse_operand_result result_type;
      bfd_vma value;

      *strp += 4;
      errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_LO16,
				   &result_type, &value);
      if (**strp != ')')
	return _("missing `)'");
      ++*strp;
      if (errmsg == NULL
	  && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	value &= 0xffff;
      *valuep = value;
      return errmsg;
    }

  return parse_imm (cd, strp, opindex, valuep);
}

/* Handle %lo in a negated signed context.
   The signedness of the value doesn't matter to %lo(), but this also
   handles the case where %lo() isn't present.  */

static const char *
parse_mlo16 (cd, strp, opindex, valuep)
     CGEN_CPU_DESC cd;
     const char **strp;
     int opindex;
     long *valuep;
{
  if (strncasecmp (*strp, "%lo(", 4) == 0)
    {
      const char *errmsg;
      enum cgen_parse_operand_result result_type;
      bfd_vma value;

      *strp += 4;
      errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_LO16,
				   &result_type, &value);
      if (**strp != ')')
	return _("missing `)'");
      ++*strp;
      if (errmsg == NULL
	  && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	value = (-value) & 0xffff;
      *valuep = value;
      return errmsg;
    }

  return parse_mimm (cd, strp, opindex, valuep);
}

/* -- */

const char * iq2000_cgen_parse_operand
  PARAMS ((CGEN_CPU_DESC, int, const char **, CGEN_FIELDS *));

/* Main entry point for operand parsing.

   This function is basically just a big switch statement.  Earlier versions
   used tables to look up the function to use, but
   - if the table contains both assembler and disassembler functions then
     the disassembler contains much of the assembler and vice-versa,
   - there's a lot of inlining possibilities as things grow,
   - using a switch statement avoids the function call overhead.

   This function could be moved into `parse_insn_normal', but keeping it
   separate makes clear the interface between `parse_insn_normal' and each of
   the handlers.  */

const char *
iq2000_cgen_parse_operand (cd, opindex, strp, fields)
     CGEN_CPU_DESC cd;
     int opindex;
     const char ** strp;
     CGEN_FIELDS * fields;
{
  const char * errmsg = NULL;
  /* Used by scalar operands that still need to be parsed.  */
  long junk ATTRIBUTE_UNUSED;

  switch (opindex)
    {
    case IQ2000_OPERAND_BASE :
      errmsg = cgen_parse_keyword (cd, strp, & iq2000_cgen_opval_gr_names, & fields->f_rs);
      break;
    case IQ2000_OPERAND_BASEOFF :
      {
        bfd_vma value;
        errmsg = cgen_parse_address (cd, strp, IQ2000_OPERAND_BASEOFF, 0, NULL,  & value);
        fields->f_imm = value;
      }
      break;
    case IQ2000_OPERAND_BITNUM :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_BITNUM, &fields->f_rt);
      break;
    case IQ2000_OPERAND_BYTECOUNT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_BYTECOUNT, &fields->f_bytecount);
      break;
    case IQ2000_OPERAND_CAM_Y :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_CAM_Y, &fields->f_cam_y);
      break;
    case IQ2000_OPERAND_CAM_Z :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_CAM_Z, &fields->f_cam_z);
      break;
    case IQ2000_OPERAND_CM_3FUNC :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_CM_3FUNC, &fields->f_cm_3func);
      break;
    case IQ2000_OPERAND_CM_3Z :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_CM_3Z, &fields->f_cm_3z);
      break;
    case IQ2000_OPERAND_CM_4FUNC :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_CM_4FUNC, &fields->f_cm_4func);
      break;
    case IQ2000_OPERAND_CM_4Z :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_CM_4Z, &fields->f_cm_4z);
      break;
    case IQ2000_OPERAND_COUNT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_COUNT, &fields->f_count);
      break;
    case IQ2000_OPERAND_EXECODE :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_EXECODE, &fields->f_excode);
      break;
    case IQ2000_OPERAND_F_INDEX :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_F_INDEX, &fields->f_index);
      break;
    case IQ2000_OPERAND_HI16 :
      errmsg = parse_hi16 (cd, strp, IQ2000_OPERAND_HI16, &fields->f_imm);
      break;
    case IQ2000_OPERAND_IMM :
      errmsg = parse_imm (cd, strp, IQ2000_OPERAND_IMM, &fields->f_imm);
      break;
    case IQ2000_OPERAND_JMPTARG :
      {
        bfd_vma value;
        errmsg = cgen_parse_address (cd, strp, IQ2000_OPERAND_JMPTARG, 0, NULL,  & value);
        fields->f_jtarg = value;
      }
      break;
    case IQ2000_OPERAND_JMPTARGQ10 :
      {
        bfd_vma value;
        errmsg = parse_jtargq10 (cd, strp, IQ2000_OPERAND_JMPTARGQ10, 0, NULL,  & value);
        fields->f_jtargq10 = value;
      }
      break;
    case IQ2000_OPERAND_LO16 :
      errmsg = parse_lo16 (cd, strp, IQ2000_OPERAND_LO16, &fields->f_imm);
      break;
    case IQ2000_OPERAND_MASK :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_MASK, &fields->f_mask);
      break;
    case IQ2000_OPERAND_MASKL :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_MASKL, &fields->f_maskl);
      break;
    case IQ2000_OPERAND_MASKQ10 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_MASKQ10, &fields->f_maskq10);
      break;
    case IQ2000_OPERAND_MASKR :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_MASKR, &fields->f_rs);
      break;
    case IQ2000_OPERAND_MLO16 :
      errmsg = parse_mlo16 (cd, strp, IQ2000_OPERAND_MLO16, &fields->f_imm);
      break;
    case IQ2000_OPERAND_OFFSET :
      {
        bfd_vma value;
        errmsg = cgen_parse_address (cd, strp, IQ2000_OPERAND_OFFSET, 0, NULL,  & value);
        fields->f_offset = value;
      }
      break;
    case IQ2000_OPERAND_RD :
      errmsg = cgen_parse_keyword (cd, strp, & iq2000_cgen_opval_gr_names, & fields->f_rd);
      break;
    case IQ2000_OPERAND_RD_RS :
      errmsg = cgen_parse_keyword (cd, strp, & iq2000_cgen_opval_gr_names, & fields->f_rd_rs);
      break;
    case IQ2000_OPERAND_RD_RT :
      errmsg = cgen_parse_keyword (cd, strp, & iq2000_cgen_opval_gr_names, & fields->f_rd_rt);
      break;
    case IQ2000_OPERAND_RS :
      errmsg = cgen_parse_keyword (cd, strp, & iq2000_cgen_opval_gr_names, & fields->f_rs);
      break;
    case IQ2000_OPERAND_RT :
      errmsg = cgen_parse_keyword (cd, strp, & iq2000_cgen_opval_gr_names, & fields->f_rt);
      break;
    case IQ2000_OPERAND_RT_RS :
      errmsg = cgen_parse_keyword (cd, strp, & iq2000_cgen_opval_gr_names, & fields->f_rt_rs);
      break;
    case IQ2000_OPERAND_SHAMT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IQ2000_OPERAND_SHAMT, &fields->f_shamt);
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while parsing.\n"), opindex);
      abort ();
  }

  return errmsg;
}

cgen_parse_fn * const iq2000_cgen_parse_handlers[] = 
{
  parse_insn_normal,
};

void
iq2000_cgen_init_asm (cd)
     CGEN_CPU_DESC cd;
{
  iq2000_cgen_init_opcode_table (cd);
  iq2000_cgen_init_ibld_table (cd);
  cd->parse_handlers = & iq2000_cgen_parse_handlers[0];
  cd->parse_operand = iq2000_cgen_parse_operand;
}



/* Regex construction routine.

   This translates an opcode syntax string into a regex string,
   by replacing any non-character syntax element (such as an
   opcode) with the pattern '.*'

   It then compiles the regex and stores it in the opcode, for
   later use by iq2000_cgen_assemble_insn

   Returns NULL for success, an error message for failure.  */

char * 
iq2000_cgen_build_insn_regex (CGEN_INSN *insn)
{  
  CGEN_OPCODE *opc = (CGEN_OPCODE *) CGEN_INSN_OPCODE (insn);
  const char *mnem = CGEN_INSN_MNEMONIC (insn);
  char rxbuf[CGEN_MAX_RX_ELEMENTS];
  char *rx = rxbuf;
  const CGEN_SYNTAX_CHAR_TYPE *syn;
  int reg_err;

  syn = CGEN_SYNTAX_STRING (CGEN_OPCODE_SYNTAX (opc));

  /* Mnemonics come first in the syntax string.  */
  if (! CGEN_SYNTAX_MNEMONIC_P (* syn))
    return _("missing mnemonic in syntax string");
  ++syn;

  /* Generate a case sensitive regular expression that emulates case
     insensitive matching in the "C" locale.  We cannot generate a case
     insensitive regular expression because in Turkish locales, 'i' and 'I'
     are not equal modulo case conversion.  */

  /* Copy the literal mnemonic out of the insn.  */
  for (; *mnem; mnem++)
    {
      char c = *mnem;

      if (ISALPHA (c))
	{
	  *rx++ = '[';
	  *rx++ = TOLOWER (c);
	  *rx++ = TOUPPER (c);
	  *rx++ = ']';
	}
      else
	*rx++ = c;
    }

  /* Copy any remaining literals from the syntax string into the rx.  */
  for(; * syn != 0 && rx <= rxbuf + (CGEN_MAX_RX_ELEMENTS - 7 - 4); ++syn)
    {
      if (CGEN_SYNTAX_CHAR_P (* syn)) 
	{
	  char c = CGEN_SYNTAX_CHAR (* syn);

	  switch (c) 
	    {
	      /* Escape any regex metacharacters in the syntax.  */
	    case '.': case '[': case '\\': 
	    case '*': case '^': case '$': 

#ifdef CGEN_ESCAPE_EXTENDED_REGEX
	    case '?': case '{': case '}': 
	    case '(': case ')': case '*':
	    case '|': case '+': case ']':
#endif
	      *rx++ = '\\';
	      *rx++ = c;
	      break;

	    default:
	      if (ISALPHA (c))
		{
		  *rx++ = '[';
		  *rx++ = TOLOWER (c);
		  *rx++ = TOUPPER (c);
		  *rx++ = ']';
		}
	      else
		*rx++ = c;
	      break;
	    }
	}
      else
	{
	  /* Replace non-syntax fields with globs.  */
	  *rx++ = '.';
	  *rx++ = '*';
	}
    }

  /* Trailing whitespace ok.  */
  * rx++ = '['; 
  * rx++ = ' '; 
  * rx++ = '\t'; 
  * rx++ = ']'; 
  * rx++ = '*'; 

  /* But anchor it after that.  */
  * rx++ = '$'; 
  * rx = '\0';

  CGEN_INSN_RX (insn) = xmalloc (sizeof (regex_t));
  reg_err = regcomp ((regex_t *) CGEN_INSN_RX (insn), rxbuf, REG_NOSUB);

  if (reg_err == 0) 
    return NULL;
  else
    {
      static char msg[80];

      regerror (reg_err, (regex_t *) CGEN_INSN_RX (insn), msg, 80);
      regfree ((regex_t *) CGEN_INSN_RX (insn));
      free (CGEN_INSN_RX (insn));
      (CGEN_INSN_RX (insn)) = NULL;
      return msg;
    }
}


/* Default insn parser.

   The syntax string is scanned and operands are parsed and stored in FIELDS.
   Relocs are queued as we go via other callbacks.

   ??? Note that this is currently an all-or-nothing parser.  If we fail to
   parse the instruction, we return 0 and the caller will start over from
   the beginning.  Backtracking will be necessary in parsing subexpressions,
   but that can be handled there.  Not handling backtracking here may get
   expensive in the case of the m68k.  Deal with later.

   Returns NULL for success, an error message for failure.  */

static const char *
parse_insn_normal (CGEN_CPU_DESC cd,
		   const CGEN_INSN *insn,
		   const char **strp,
		   CGEN_FIELDS *fields)
{
  /* ??? Runtime added insns not handled yet.  */
  const CGEN_SYNTAX *syntax = CGEN_INSN_SYNTAX (insn);
  const char *str = *strp;
  const char *errmsg;
  const char *p;
  const CGEN_SYNTAX_CHAR_TYPE * syn;
#ifdef CGEN_MNEMONIC_OPERANDS
  /* FIXME: wip */
  int past_opcode_p;
#endif

  /* For now we assume the mnemonic is first (there are no leading operands).
     We can parse it without needing to set up operand parsing.
     GAS's input scrubber will ensure mnemonics are lowercase, but we may
     not be called from GAS.  */
  p = CGEN_INSN_MNEMONIC (insn);
  while (*p && TOLOWER (*p) == TOLOWER (*str))
    ++p, ++str;

  if (* p)
    return _("unrecognized instruction");

#ifndef CGEN_MNEMONIC_OPERANDS
  if (* str && ! ISSPACE (* str))
    return _("unrecognized instruction");
#endif

  CGEN_INIT_PARSE (cd);
  cgen_init_parse_operand (cd);
#ifdef CGEN_MNEMONIC_OPERANDS
  past_opcode_p = 0;
#endif

  /* We don't check for (*str != '\0') here because we want to parse
     any trailing fake arguments in the syntax string.  */
  syn = CGEN_SYNTAX_STRING (syntax);

  /* Mnemonics come first for now, ensure valid string.  */
  if (! CGEN_SYNTAX_MNEMONIC_P (* syn))
    abort ();

  ++syn;

  while (* syn != 0)
    {
      /* Non operand chars must match exactly.  */
      if (CGEN_SYNTAX_CHAR_P (* syn))
	{
	  /* FIXME: While we allow for non-GAS callers above, we assume the
	     first char after the mnemonic part is a space.  */
	  /* FIXME: We also take inappropriate advantage of the fact that
	     GAS's input scrubber will remove extraneous blanks.  */
	  if (TOLOWER (*str) == TOLOWER (CGEN_SYNTAX_CHAR (* syn)))
	    {
#ifdef CGEN_MNEMONIC_OPERANDS
	      if (CGEN_SYNTAX_CHAR(* syn) == ' ')
		past_opcode_p = 1;
#endif
	      ++ syn;
	      ++ str;
	    }
	  else if (*str)
	    {
	      /* Syntax char didn't match.  Can't be this insn.  */
	      static char msg [80];

	      /* xgettext:c-format */
	      sprintf (msg, _("syntax error (expected char `%c', found `%c')"),
		       CGEN_SYNTAX_CHAR(*syn), *str);
	      return msg;
	    }
	  else
	    {
	      /* Ran out of input.  */
	      static char msg [80];

	      /* xgettext:c-format */
	      sprintf (msg, _("syntax error (expected char `%c', found end of instruction)"),
		       CGEN_SYNTAX_CHAR(*syn));
	      return msg;
	    }
	  continue;
	}

      /* We have an operand of some sort.  */
      errmsg = cd->parse_operand (cd, CGEN_SYNTAX_FIELD (*syn),
					  &str, fields);
      if (errmsg)
	return errmsg;

      /* Done with this operand, continue with next one.  */
      ++ syn;
    }

  /* If we're at the end of the syntax string, we're done.  */
  if (* syn == 0)
    {
      /* FIXME: For the moment we assume a valid `str' can only contain
	 blanks now.  IE: We needn't try again with a longer version of
	 the insn and it is assumed that longer versions of insns appear
	 before shorter ones (eg: lsr r2,r3,1 vs lsr r2,r3).  */
      while (ISSPACE (* str))
	++ str;

      if (* str != '\0')
	return _("junk at end of line"); /* FIXME: would like to include `str' */

      return NULL;
    }

  /* We couldn't parse it.  */
  return _("unrecognized instruction");
}

/* Main entry point.
   This routine is called for each instruction to be assembled.
   STR points to the insn to be assembled.
   We assume all necessary tables have been initialized.
   The assembled instruction, less any fixups, is stored in BUF.
   Remember that if CGEN_INT_INSN_P then BUF is an int and thus the value
   still needs to be converted to target byte order, otherwise BUF is an array
   of bytes in target byte order.
   The result is a pointer to the insn's entry in the opcode table,
   or NULL if an error occured (an error message will have already been
   printed).

   Note that when processing (non-alias) macro-insns,
   this function recurses.

   ??? It's possible to make this cpu-independent.
   One would have to deal with a few minor things.
   At this point in time doing so would be more of a curiosity than useful
   [for example this file isn't _that_ big], but keeping the possibility in
   mind helps keep the design clean.  */

const CGEN_INSN *
iq2000_cgen_assemble_insn (CGEN_CPU_DESC cd,
			   const char *str,
			   CGEN_FIELDS *fields,
			   CGEN_INSN_BYTES_PTR buf,
			   char **errmsg)
{
  const char *start;
  CGEN_INSN_LIST *ilist;
  const char *parse_errmsg = NULL;
  const char *insert_errmsg = NULL;
  int recognized_mnemonic = 0;

  /* Skip leading white space.  */
  while (ISSPACE (* str))
    ++ str;

  /* The instructions are stored in hashed lists.
     Get the first in the list.  */
  ilist = CGEN_ASM_LOOKUP_INSN (cd, str);

  /* Keep looking until we find a match.  */
  start = str;
  for ( ; ilist != NULL ; ilist = CGEN_ASM_NEXT_INSN (ilist))
    {
      const CGEN_INSN *insn = ilist->insn;
      recognized_mnemonic = 1;

#ifdef CGEN_VALIDATE_INSN_SUPPORTED 
      /* Not usually needed as unsupported opcodes
	 shouldn't be in the hash lists.  */
      /* Is this insn supported by the selected cpu?  */
      if (! iq2000_cgen_insn_supported (cd, insn))
	continue;
#endif
      /* If the RELAXED attribute is set, this is an insn that shouldn't be
	 chosen immediately.  Instead, it is used during assembler/linker
	 relaxation if possible.  */
      if (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_RELAXED) != 0)
	continue;

      str = start;

      /* Skip this insn if str doesn't look right lexically.  */
      if (CGEN_INSN_RX (insn) != NULL &&
	  regexec ((regex_t *) CGEN_INSN_RX (insn), str, 0, NULL, 0) == REG_NOMATCH)
	continue;

      /* Allow parse/insert handlers to obtain length of insn.  */
      CGEN_FIELDS_BITSIZE (fields) = CGEN_INSN_BITSIZE (insn);

      parse_errmsg = CGEN_PARSE_FN (cd, insn) (cd, insn, & str, fields);
      if (parse_errmsg != NULL)
	continue;

      /* ??? 0 is passed for `pc'.  */
      insert_errmsg = CGEN_INSERT_FN (cd, insn) (cd, insn, fields, buf,
						 (bfd_vma) 0);
      if (insert_errmsg != NULL)
        continue;

      /* It is up to the caller to actually output the insn and any
         queued relocs.  */
      return insn;
    }

  {
    static char errbuf[150];
#ifdef CGEN_VERBOSE_ASSEMBLER_ERRORS
    const char *tmp_errmsg;

    /* If requesting verbose error messages, use insert_errmsg.
       Failing that, use parse_errmsg.  */
    tmp_errmsg = (insert_errmsg ? insert_errmsg :
		  parse_errmsg ? parse_errmsg :
		  recognized_mnemonic ?
		  _("unrecognized form of instruction") :
		  _("unrecognized instruction"));

    if (strlen (start) > 50)
      /* xgettext:c-format */
      sprintf (errbuf, "%s `%.50s...'", tmp_errmsg, start);
    else 
      /* xgettext:c-format */
      sprintf (errbuf, "%s `%.50s'", tmp_errmsg, start);
#else
    if (strlen (start) > 50)
      /* xgettext:c-format */
      sprintf (errbuf, _("bad instruction `%.50s...'"), start);
    else 
      /* xgettext:c-format */
      sprintf (errbuf, _("bad instruction `%.50s'"), start);
#endif
      
    *errmsg = errbuf;
    return NULL;
  }
}

#if 0 /* This calls back to GAS which we can't do without care.  */

/* Record each member of OPVALS in the assembler's symbol table.
   This lets GAS parse registers for us.
   ??? Interesting idea but not currently used.  */

/* Record each member of OPVALS in the assembler's symbol table.
   FIXME: Not currently used.  */

void
iq2000_cgen_asm_hash_keywords (CGEN_CPU_DESC cd, CGEN_KEYWORD *opvals)
{
  CGEN_KEYWORD_SEARCH search = cgen_keyword_search_init (opvals, NULL);
  const CGEN_KEYWORD_ENTRY * ke;

  while ((ke = cgen_keyword_search_next (& search)) != NULL)
    {
#if 0 /* Unnecessary, should be done in the search routine.  */
      if (! iq2000_cgen_opval_supported (ke))
	continue;
#endif
      cgen_asm_record_register (cd, ke->name, ke->value);
    }
}

#endif /* 0 */
