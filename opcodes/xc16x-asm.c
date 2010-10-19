/* Assembler interface for targets using CGEN. -*- C -*-
   CGEN: Cpu tools GENerator

   THIS FILE IS MACHINE GENERATED WITH CGEN.
   - the resultant file is machine generated, cgen-asm.in isn't

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
#include "bfd.h"
#include "symcat.h"
#include "xc16x-desc.h"
#include "xc16x-opc.h"
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
/* Handle '#' prefixes (i.e. skip over them).  */

static const char *
parse_hash (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	    const char **strp,
	    int opindex ATTRIBUTE_UNUSED,
	    long *valuep ATTRIBUTE_UNUSED)
{
  if (**strp == '#')
    {
      ++*strp;
      return NULL;
    }
  return _("Missing '#' prefix");
}

/* Handle '.' prefixes (i.e. skip over them).  */

static const char *
parse_dot (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	   const char **strp,
	   int opindex ATTRIBUTE_UNUSED,
	   long *valuep ATTRIBUTE_UNUSED)
{
  if (**strp == '.')
    {
      ++*strp;
      return NULL;
    }
  return _("Missing '.' prefix");
}

/* Handle 'pof:' prefixes (i.e. skip over them).  */

static const char *
parse_pof (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	   const char **strp,
	   int opindex ATTRIBUTE_UNUSED,
	   long *valuep ATTRIBUTE_UNUSED)
{
  if (strncasecmp (*strp, "pof:", 4) == 0)
    {
      *strp += 4;
      return NULL;
    }
  return _("Missing 'pof:' prefix");  
}

/* Handle 'pag:' prefixes (i.e. skip over them).  */

static const char *
parse_pag (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	   const char **strp,
	   int opindex ATTRIBUTE_UNUSED,
	   long *valuep ATTRIBUTE_UNUSED)
{
  if (strncasecmp (*strp, "pag:", 4) == 0)
    {
      *strp += 4;
      return NULL;
    }
  return _("Missing 'pag:' prefix");
}

/* Handle 'sof' prefixes (i.e. skip over them).  */

static const char *
parse_sof (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	   const char **strp,
	   int opindex ATTRIBUTE_UNUSED,
	   long *valuep ATTRIBUTE_UNUSED)
{
  if (strncasecmp (*strp, "sof:", 4) == 0)
    {
      *strp += 4;
      return NULL;
    }
  return _("Missing 'sof:' prefix");
}

/* Handle 'seg' prefixes (i.e. skip over them).  */

static const char *
parse_seg (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	   const char **strp,
	   int opindex ATTRIBUTE_UNUSED,
	   long *valuep ATTRIBUTE_UNUSED)
{
  if (strncasecmp (*strp, "seg:", 4) == 0)
    {
      *strp += 4;
      return NULL;
    }
  return _("Missing 'seg:' prefix");
}
/* -- */

const char * xc16x_cgen_parse_operand
  (CGEN_CPU_DESC, int, const char **, CGEN_FIELDS *);

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
xc16x_cgen_parse_operand (CGEN_CPU_DESC cd,
			   int opindex,
			   const char ** strp,
			   CGEN_FIELDS * fields)
{
  const char * errmsg = NULL;
  /* Used by scalar operands that still need to be parsed.  */
  long junk ATTRIBUTE_UNUSED;

  switch (opindex)
    {
    case XC16X_OPERAND_REGNAM :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_psw_names, & fields->f_reg8);
      break;
    case XC16X_OPERAND_BIT01 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_BIT01, (unsigned long *) (& fields->f_op_1bit));
      break;
    case XC16X_OPERAND_BIT1 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_BIT1, (unsigned long *) (& fields->f_op_bit1));
      break;
    case XC16X_OPERAND_BIT2 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_BIT2, (unsigned long *) (& fields->f_op_bit2));
      break;
    case XC16X_OPERAND_BIT4 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_BIT4, (unsigned long *) (& fields->f_op_bit4));
      break;
    case XC16X_OPERAND_BIT8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_BIT8, (unsigned long *) (& fields->f_op_bit8));
      break;
    case XC16X_OPERAND_BITONE :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_BITONE, (unsigned long *) (& fields->f_op_onebit));
      break;
    case XC16X_OPERAND_CADDR :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, XC16X_OPERAND_CADDR, 0, NULL,  & value);
        fields->f_offset16 = value;
      }
      break;
    case XC16X_OPERAND_COND :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_conditioncode_names, & fields->f_condcode);
      break;
    case XC16X_OPERAND_DATA8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_DATA8, (unsigned long *) (& fields->f_data8));
      break;
    case XC16X_OPERAND_DATAHI8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_DATAHI8, (unsigned long *) (& fields->f_datahi8));
      break;
    case XC16X_OPERAND_DOT :
      errmsg = parse_dot (cd, strp, XC16X_OPERAND_DOT, (long *) (& junk));
      break;
    case XC16X_OPERAND_DR :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_gr_names, & fields->f_r1);
      break;
    case XC16X_OPERAND_DRB :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_grb_names, & fields->f_r1);
      break;
    case XC16X_OPERAND_DRI :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_gr_names, & fields->f_r4);
      break;
    case XC16X_OPERAND_EXTCOND :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_extconditioncode_names, & fields->f_extccode);
      break;
    case XC16X_OPERAND_GENREG :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_r8_names, & fields->f_regb8);
      break;
    case XC16X_OPERAND_HASH :
      errmsg = parse_hash (cd, strp, XC16X_OPERAND_HASH, (long *) (& junk));
      break;
    case XC16X_OPERAND_ICOND :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_conditioncode_names, & fields->f_icondcode);
      break;
    case XC16X_OPERAND_LBIT2 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_LBIT2, (unsigned long *) (& fields->f_op_lbit2));
      break;
    case XC16X_OPERAND_LBIT4 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_LBIT4, (unsigned long *) (& fields->f_op_lbit4));
      break;
    case XC16X_OPERAND_MASK8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_MASK8, (unsigned long *) (& fields->f_mask8));
      break;
    case XC16X_OPERAND_MASKLO8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_MASKLO8, (unsigned long *) (& fields->f_datahi8));
      break;
    case XC16X_OPERAND_MEMGR8 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_memgr8_names, & fields->f_memgr8);
      break;
    case XC16X_OPERAND_MEMORY :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, XC16X_OPERAND_MEMORY, 0, NULL,  & value);
        fields->f_memory = value;
      }
      break;
    case XC16X_OPERAND_PAG :
      errmsg = parse_pag (cd, strp, XC16X_OPERAND_PAG, (long *) (& junk));
      break;
    case XC16X_OPERAND_PAGENUM :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_PAGENUM, (unsigned long *) (& fields->f_pagenum));
      break;
    case XC16X_OPERAND_POF :
      errmsg = parse_pof (cd, strp, XC16X_OPERAND_POF, (long *) (& junk));
      break;
    case XC16X_OPERAND_QBIT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_QBIT, (unsigned long *) (& fields->f_qbit));
      break;
    case XC16X_OPERAND_QHIBIT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_QHIBIT, (unsigned long *) (& fields->f_qhibit));
      break;
    case XC16X_OPERAND_QLOBIT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_QLOBIT, (unsigned long *) (& fields->f_qlobit));
      break;
    case XC16X_OPERAND_REG8 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_r8_names, & fields->f_reg8);
      break;
    case XC16X_OPERAND_REGB8 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_grb8_names, & fields->f_regb8);
      break;
    case XC16X_OPERAND_REGBMEM8 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_regbmem8_names, & fields->f_regmem8);
      break;
    case XC16X_OPERAND_REGHI8 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_r8_names, & fields->f_reghi8);
      break;
    case XC16X_OPERAND_REGMEM8 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_regmem8_names, & fields->f_regmem8);
      break;
    case XC16X_OPERAND_REGOFF8 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_r8_names, & fields->f_regoff8);
      break;
    case XC16X_OPERAND_REL :
      errmsg = cgen_parse_signed_integer (cd, strp, XC16X_OPERAND_REL, (long *) (& fields->f_rel8));
      break;
    case XC16X_OPERAND_RELHI :
      errmsg = cgen_parse_signed_integer (cd, strp, XC16X_OPERAND_RELHI, (long *) (& fields->f_relhi8));
      break;
    case XC16X_OPERAND_SEG :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_SEG, (unsigned long *) (& fields->f_seg8));
      break;
    case XC16X_OPERAND_SEGHI8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_SEGHI8, (unsigned long *) (& fields->f_segnum8));
      break;
    case XC16X_OPERAND_SEGM :
      errmsg = parse_seg (cd, strp, XC16X_OPERAND_SEGM, (long *) (& junk));
      break;
    case XC16X_OPERAND_SOF :
      errmsg = parse_sof (cd, strp, XC16X_OPERAND_SOF, (long *) (& junk));
      break;
    case XC16X_OPERAND_SR :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_gr_names, & fields->f_r2);
      break;
    case XC16X_OPERAND_SR2 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_gr_names, & fields->f_r0);
      break;
    case XC16X_OPERAND_SRB :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_grb_names, & fields->f_r2);
      break;
    case XC16X_OPERAND_SRC1 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_gr_names, & fields->f_r1);
      break;
    case XC16X_OPERAND_SRC2 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_gr_names, & fields->f_r2);
      break;
    case XC16X_OPERAND_SRDIV :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_regdiv8_names, & fields->f_reg8);
      break;
    case XC16X_OPERAND_U4 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_reg0_name, & fields->f_uimm4);
      break;
    case XC16X_OPERAND_UIMM16 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_UIMM16, (unsigned long *) (& fields->f_uimm16));
      break;
    case XC16X_OPERAND_UIMM2 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_ext_names, & fields->f_uimm2);
      break;
    case XC16X_OPERAND_UIMM3 :
      errmsg = cgen_parse_keyword (cd, strp, & xc16x_cgen_opval_reg0_name1, & fields->f_uimm3);
      break;
    case XC16X_OPERAND_UIMM4 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_UIMM4, (unsigned long *) (& fields->f_uimm4));
      break;
    case XC16X_OPERAND_UIMM7 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_UIMM7, (unsigned long *) (& fields->f_uimm7));
      break;
    case XC16X_OPERAND_UIMM8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_UIMM8, (unsigned long *) (& fields->f_uimm8));
      break;
    case XC16X_OPERAND_UPAG16 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_UPAG16, (unsigned long *) (& fields->f_uimm16));
      break;
    case XC16X_OPERAND_UPOF16 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, XC16X_OPERAND_UPOF16, 0, NULL,  & value);
        fields->f_memory = value;
      }
      break;
    case XC16X_OPERAND_USEG16 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_USEG16, (unsigned long *) (& fields->f_offset16));
      break;
    case XC16X_OPERAND_USEG8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_USEG8, (unsigned long *) (& fields->f_seg8));
      break;
    case XC16X_OPERAND_USOF16 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, XC16X_OPERAND_USOF16, (unsigned long *) (& fields->f_offset16));
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while parsing.\n"), opindex);
      abort ();
  }

  return errmsg;
}

cgen_parse_fn * const xc16x_cgen_parse_handlers[] = 
{
  parse_insn_normal,
};

void
xc16x_cgen_init_asm (CGEN_CPU_DESC cd)
{
  xc16x_cgen_init_opcode_table (cd);
  xc16x_cgen_init_ibld_table (cd);
  cd->parse_handlers = & xc16x_cgen_parse_handlers[0];
  cd->parse_operand = xc16x_cgen_parse_operand;
}



/* Regex construction routine.

   This translates an opcode syntax string into a regex string,
   by replacing any non-character syntax element (such as an
   opcode) with the pattern '.*'

   It then compiles the regex and stores it in the opcode, for
   later use by xc16x_cgen_assemble_insn

   Returns NULL for success, an error message for failure.  */

char * 
xc16x_cgen_build_insn_regex (CGEN_INSN *insn)
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
xc16x_cgen_assemble_insn (CGEN_CPU_DESC cd,
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
      if (! xc16x_cgen_insn_supported (cd, insn))
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
