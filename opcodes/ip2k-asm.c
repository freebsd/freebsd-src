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
#include "ip2k-desc.h"
#include "ip2k-opc.h"
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

static const char *
parse_fr (CGEN_CPU_DESC cd,
	  const char **strp,
	  int opindex,
	  unsigned long *valuep)
{
  const char *errmsg;
  const char *old_strp;
  char *afteroffset; 
  enum cgen_parse_operand_result result_type;
  bfd_vma value;
  extern CGEN_KEYWORD ip2k_cgen_opval_register_names;
  bfd_vma tempvalue;

  old_strp = *strp;
  afteroffset = NULL;

  /* Check here to see if you're about to try parsing a w as the first arg
     and return an error if you are.  */
  if ((strncmp (*strp, "w", 1) == 0) || (strncmp (*strp, "W", 1) == 0))
    {
      (*strp)++;

      if ((strncmp (*strp, ",", 1) == 0) || ISSPACE (**strp))
	{
	  /* We've been passed a w.  Return with an error message so that
	     cgen will try the next parsing option.  */
	  errmsg = _("W keyword invalid in FR operand slot.");
	  return errmsg;
	}
      *strp = old_strp;
    }

  /* Attempt parse as register keyword. */
  errmsg = cgen_parse_keyword (cd, strp, & ip2k_cgen_opval_register_names,
			       (long *) valuep);
  if (*strp != NULL
      && errmsg == NULL)
    return errmsg;

  /* Attempt to parse for "(IP)".  */
  afteroffset = strstr (*strp, "(IP)");

  if (afteroffset == NULL)
    /* Make sure it's not in lower case.  */
    afteroffset = strstr (*strp, "(ip)");

  if (afteroffset != NULL)
    {
      if (afteroffset != *strp)
	{
	  /* Invalid offset present.  */
	  errmsg = _("offset(IP) is not a valid form");
	  return errmsg;
	}
      else
	{
	  *strp += 4; 
	  *valuep = 0;
	  errmsg = NULL;
	  return errmsg;
	}
    }

  /* Attempt to parse for DP. ex: mov w, offset(DP)
                                  mov offset(DP),w   */

  /* Try parsing it as an address and see what comes back.  */
  afteroffset = strstr (*strp, "(DP)");

  if (afteroffset == NULL)
    /* Maybe it's in lower case.  */
    afteroffset = strstr (*strp, "(dp)");

  if (afteroffset != NULL)
    {
      if (afteroffset == *strp)
	{
	  /* No offset present. Use 0 by default.  */
	  tempvalue = 0;
	  errmsg = NULL;
	}
      else
	errmsg = cgen_parse_address (cd, strp, opindex,
				     BFD_RELOC_IP2K_FR_OFFSET,
				     & result_type, & tempvalue);

      if (errmsg == NULL)
	{
	  if (tempvalue <= 127)
	    {
	      /* Value is ok.  Fix up the first 2 bits and return.  */
	      *valuep = 0x0100 | tempvalue;
	      *strp += 4; /* Skip over the (DP) in *strp.  */
	      return errmsg;
	    }
	  else
	    {
	      /* Found something there in front of (DP) but it's out
		 of range.  */
	      errmsg = _("(DP) offset out of range.");
	      return errmsg;
	    }
	}
    }


  /* Attempt to parse for SP. ex: mov w, offset(SP)
                                  mov offset(SP), w.  */
  afteroffset = strstr (*strp, "(SP)");

  if (afteroffset == NULL)
    /* Maybe it's in lower case.  */
    afteroffset = strstr (*strp, "(sp)");

  if (afteroffset != NULL)
    {
      if (afteroffset == *strp)
	{
	  /* No offset present. Use 0 by default.  */
	  tempvalue = 0;
	  errmsg = NULL;
	}
      else
	errmsg = cgen_parse_address (cd, strp, opindex,
				     BFD_RELOC_IP2K_FR_OFFSET,
				     & result_type, & tempvalue);

      if (errmsg == NULL)
	{
	  if (tempvalue <= 127)
	    {
	      /* Value is ok.  Fix up the first 2 bits and return.  */
	      *valuep = 0x0180 | tempvalue;
	      *strp += 4; /* Skip over the (SP) in *strp.  */
	      return errmsg;
	    }
	  else
	    {
	      /* Found something there in front of (SP) but it's out
		 of range.  */
	      errmsg = _("(SP) offset out of range.");
	      return errmsg;
	    }
	}
    }

  /* Attempt to parse as an address.  */
  *strp = old_strp;
  errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_IP2K_FR9,
			       & result_type, & value);
  if (errmsg == NULL)
    {
      *valuep = value;

      /* If a parenthesis is found, warn about invalid form.  */
      if (**strp == '(')
	errmsg = _("illegal use of parentheses");

      /* If a numeric value is specified, ensure that it is between
	 1 and 255.  */
      else if (result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  if (value < 0x1 || value > 0xff)
	    errmsg = _("operand out of range (not between 1 and 255)");
	}
    }
  return errmsg;
}

static const char *
parse_addr16 (CGEN_CPU_DESC cd,
	      const char **strp,
	      int opindex,
	      unsigned long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_reloc_code_real_type code = BFD_RELOC_NONE;
  bfd_vma value;

  if (opindex == (CGEN_OPERAND_TYPE) IP2K_OPERAND_ADDR16H)
    code = BFD_RELOC_IP2K_HI8DATA;
  else if (opindex == (CGEN_OPERAND_TYPE) IP2K_OPERAND_ADDR16L)
    code = BFD_RELOC_IP2K_LO8DATA;
  else
    {
      /* Something is very wrong. opindex has to be one of the above.  */
      errmsg = _("parse_addr16: invalid opindex.");
      return errmsg;
    }
  
  errmsg = cgen_parse_address (cd, strp, opindex, code,
			       & result_type, & value);
  if (errmsg == NULL)
    {
      /* We either have a relocation or a number now.  */
      if (result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  /* We got a number back.  */
	  if (code == BFD_RELOC_IP2K_HI8DATA)
            value >>= 8;
	  else
	    /* code = BFD_RELOC_IP2K_LOW8DATA.  */
	    value &= 0x00FF;
	}   
      *valuep = value;
    }

  return errmsg;
}

static const char *
parse_addr16_cjp (CGEN_CPU_DESC cd,
		  const char **strp,
		  int opindex,
		  unsigned long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_reloc_code_real_type code = BFD_RELOC_NONE;
  bfd_vma value;
 
  if (opindex == (CGEN_OPERAND_TYPE) IP2K_OPERAND_ADDR16CJP)
    code = BFD_RELOC_IP2K_ADDR16CJP;
  else if (opindex == (CGEN_OPERAND_TYPE) IP2K_OPERAND_ADDR16P)
    code = BFD_RELOC_IP2K_PAGE3;

  errmsg = cgen_parse_address (cd, strp, opindex, code,
			       & result_type, & value);
  if (errmsg == NULL)
    {
      if (result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  if ((value & 0x1) == 0)  /* If the address is even .... */
	    {
	      if (opindex == (CGEN_OPERAND_TYPE) IP2K_OPERAND_ADDR16CJP)
                *valuep = (value >> 1) & 0x1FFF;  /* Should mask be 1FFF?  */
	      else if (opindex == (CGEN_OPERAND_TYPE) IP2K_OPERAND_ADDR16P)
                *valuep = (value >> 14) & 0x7;
	    }
          else
 	    errmsg = _("Byte address required. - must be even.");
	}
      else if (result_type == CGEN_PARSE_OPERAND_RESULT_QUEUED)
	{
	  /* This will happen for things like (s2-s1) where s2 and s1
	     are labels.  */
	  *valuep = value;
	}
      else 
        errmsg = _("cgen_parse_address returned a symbol. Literal required.");
    }
  return errmsg; 
}

static const char *
parse_lit8 (CGEN_CPU_DESC cd,
	    const char **strp,
	    int opindex,
	    long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_reloc_code_real_type code = BFD_RELOC_NONE;
  bfd_vma value;

  /* Parse %OP relocating operators.  */
  if (strncmp (*strp, "%bank", 5) == 0)
    {
      *strp += 5;
      code = BFD_RELOC_IP2K_BANK;
    }
  else if (strncmp (*strp, "%lo8data", 8) == 0)
    {
      *strp += 8;
      code = BFD_RELOC_IP2K_LO8DATA;
    }
  else if (strncmp (*strp, "%hi8data", 8) == 0)
    {
      *strp += 8;
      code = BFD_RELOC_IP2K_HI8DATA;
    }
  else if (strncmp (*strp, "%ex8data", 8) == 0)
    {
      *strp += 8;
      code = BFD_RELOC_IP2K_EX8DATA;
    }
  else if (strncmp (*strp, "%lo8insn", 8) == 0)
    {
      *strp += 8;
      code = BFD_RELOC_IP2K_LO8INSN;
    }
  else if (strncmp (*strp, "%hi8insn", 8) == 0)
    {
      *strp += 8;
      code = BFD_RELOC_IP2K_HI8INSN;
    }

  /* Parse %op operand.  */
  if (code != BFD_RELOC_NONE)
    {
      errmsg = cgen_parse_address (cd, strp, opindex, code, 
				   & result_type, & value);
      if ((errmsg == NULL) &&
	  (result_type != CGEN_PARSE_OPERAND_RESULT_QUEUED))
	errmsg = _("percent-operator operand is not a symbol");

      *valuep = value;
    }
  /* Parse as a number.  */
  else
    {
      errmsg = cgen_parse_signed_integer (cd, strp, opindex, valuep);

      /* Truncate to eight bits to accept both signed and unsigned input.  */
      if (errmsg == NULL)
	*valuep &= 0xFF;
    }

  return errmsg;
}

static const char *
parse_bit3 (CGEN_CPU_DESC cd,
	    const char **strp,
	    int opindex,
	    unsigned long *valuep)
{
  const char *errmsg;
  char mode = 0;
  long count = 0;
  unsigned long value;

  if (strncmp (*strp, "%bit", 4) == 0)
    {
      *strp += 4;
      mode = 1;
    }
  else if (strncmp (*strp, "%msbbit", 7) == 0)
    {
      *strp += 7;
      mode = 1;
    }
  else if (strncmp (*strp, "%lsbbit", 7) == 0)
    {
      *strp += 7;
      mode = 2;
    }

  errmsg = cgen_parse_unsigned_integer (cd, strp, opindex, valuep);
  if (errmsg)
    return errmsg;

  if (mode)
    {
      value = * valuep;
      if (value == 0)
	{
	  errmsg = _("Attempt to find bit index of 0");
	  return errmsg;
	}
    
      if (mode == 1)
	{
	  count = 31;
	  while ((value & 0x80000000) == 0)
	    {
	      count--;
	      value <<= 1;
	    }
	}
      else if (mode == 2)
	{
	  count = 0;
	  while ((value & 0x00000001) == 0)
	    {
	      count++;
	      value >>= 1;
	    }
	}
    
      *valuep = count;
    }

  return errmsg;
}

/* -- dis.c */

const char * ip2k_cgen_parse_operand
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
ip2k_cgen_parse_operand (CGEN_CPU_DESC cd,
			   int opindex,
			   const char ** strp,
			   CGEN_FIELDS * fields)
{
  const char * errmsg = NULL;
  /* Used by scalar operands that still need to be parsed.  */
  long junk ATTRIBUTE_UNUSED;

  switch (opindex)
    {
    case IP2K_OPERAND_ADDR16CJP :
      errmsg = parse_addr16_cjp (cd, strp, IP2K_OPERAND_ADDR16CJP, (unsigned long *) (& fields->f_addr16cjp));
      break;
    case IP2K_OPERAND_ADDR16H :
      errmsg = parse_addr16 (cd, strp, IP2K_OPERAND_ADDR16H, (unsigned long *) (& fields->f_imm8));
      break;
    case IP2K_OPERAND_ADDR16L :
      errmsg = parse_addr16 (cd, strp, IP2K_OPERAND_ADDR16L, (unsigned long *) (& fields->f_imm8));
      break;
    case IP2K_OPERAND_ADDR16P :
      errmsg = parse_addr16_cjp (cd, strp, IP2K_OPERAND_ADDR16P, (unsigned long *) (& fields->f_page3));
      break;
    case IP2K_OPERAND_BITNO :
      errmsg = parse_bit3 (cd, strp, IP2K_OPERAND_BITNO, (unsigned long *) (& fields->f_bitno));
      break;
    case IP2K_OPERAND_CBIT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IP2K_OPERAND_CBIT, (unsigned long *) (& junk));
      break;
    case IP2K_OPERAND_DCBIT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IP2K_OPERAND_DCBIT, (unsigned long *) (& junk));
      break;
    case IP2K_OPERAND_FR :
      errmsg = parse_fr (cd, strp, IP2K_OPERAND_FR, (unsigned long *) (& fields->f_reg));
      break;
    case IP2K_OPERAND_LIT8 :
      errmsg = parse_lit8 (cd, strp, IP2K_OPERAND_LIT8, (long *) (& fields->f_imm8));
      break;
    case IP2K_OPERAND_PABITS :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IP2K_OPERAND_PABITS, (unsigned long *) (& junk));
      break;
    case IP2K_OPERAND_RETI3 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IP2K_OPERAND_RETI3, (unsigned long *) (& fields->f_reti3));
      break;
    case IP2K_OPERAND_ZBIT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, IP2K_OPERAND_ZBIT, (unsigned long *) (& junk));
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while parsing.\n"), opindex);
      abort ();
  }

  return errmsg;
}

cgen_parse_fn * const ip2k_cgen_parse_handlers[] = 
{
  parse_insn_normal,
};

void
ip2k_cgen_init_asm (CGEN_CPU_DESC cd)
{
  ip2k_cgen_init_opcode_table (cd);
  ip2k_cgen_init_ibld_table (cd);
  cd->parse_handlers = & ip2k_cgen_parse_handlers[0];
  cd->parse_operand = ip2k_cgen_parse_operand;
}



/* Regex construction routine.

   This translates an opcode syntax string into a regex string,
   by replacing any non-character syntax element (such as an
   opcode) with the pattern '.*'

   It then compiles the regex and stores it in the opcode, for
   later use by ip2k_cgen_assemble_insn

   Returns NULL for success, an error message for failure.  */

char * 
ip2k_cgen_build_insn_regex (CGEN_INSN *insn)
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
ip2k_cgen_assemble_insn (CGEN_CPU_DESC cd,
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
      if (! ip2k_cgen_insn_supported (cd, insn))
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
