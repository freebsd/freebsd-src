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
#include "fr30-desc.h"
#include "fr30-opc.h"
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
/* Handle register lists for LDMx and STMx.  */

static int
parse_register_number (const char **strp)
{
  int regno;

  if (**strp < '0' || **strp > '9')
    return -1; /* Error.  */
  regno = **strp - '0';
  ++*strp;

  if (**strp >= '0' && **strp <= '9')
    {
      regno = regno * 10 + (**strp - '0');
      ++*strp;
    }

  return regno;
}

static const char *
parse_register_list (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
		     const char **strp,
		     int opindex ATTRIBUTE_UNUSED,
		     unsigned long *valuep,
		     int high_low,   /* 0 == high, 1 == low.  */
		     int load_store) /* 0 == load, 1 == store.  */
{
  *valuep = 0;
  while (**strp && **strp != ')')
    {
      int regno;

      if (**strp != 'R' && **strp != 'r')
	break;
      ++*strp;

      regno = parse_register_number (strp);
      if (regno == -1)
	return _("Register number is not valid");
      if (regno > 7 && !high_low)
	return _("Register must be between r0 and r7");
      if (regno < 8 && high_low)
	return _("Register must be between r8 and r15");

      if (high_low)
	regno -= 8;

      if (load_store) /* Mask is reversed for store.  */
	*valuep |= 0x80 >> regno;
      else
	*valuep |= 1 << regno;

      if (**strp == ',')
	{
	  if (*(*strp + 1) == ')')
	    break;
	  ++*strp;
	}
    }

  if (!*strp || **strp != ')')
    return _("Register list is not valid");

  return NULL;
}

static const char *
parse_low_register_list_ld (CGEN_CPU_DESC cd,
			    const char **strp,
			    int opindex,
			    unsigned long *valuep)
{
  return parse_register_list (cd, strp, opindex, valuep,
			      0 /* Low.  */, 0 /* Load.  */);
}

static const char *
parse_hi_register_list_ld (CGEN_CPU_DESC cd,
			   const char **strp,
			   int opindex,
			   unsigned long *valuep)
{
  return parse_register_list (cd, strp, opindex, valuep,
			      1 /* High.  */, 0 /* Load.  */);
}

static const char *
parse_low_register_list_st (CGEN_CPU_DESC cd,
			    const char **strp,
			    int opindex,
			    unsigned long *valuep)
{
  return parse_register_list (cd, strp, opindex, valuep,
			      0 /* Low.  */, 1 /* Store.  */);
}

static const char *
parse_hi_register_list_st (CGEN_CPU_DESC cd,
			   const char **strp,
			   int opindex,
			   unsigned long *valuep)
{
  return parse_register_list (cd, strp, opindex, valuep,
			      1 /* High.  */, 1 /* Store.  */);
}

/* -- */

const char * fr30_cgen_parse_operand
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
fr30_cgen_parse_operand (CGEN_CPU_DESC cd,
			   int opindex,
			   const char ** strp,
			   CGEN_FIELDS * fields)
{
  const char * errmsg = NULL;
  /* Used by scalar operands that still need to be parsed.  */
  long junk ATTRIBUTE_UNUSED;

  switch (opindex)
    {
    case FR30_OPERAND_CRI :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_cr_names, & fields->f_CRi);
      break;
    case FR30_OPERAND_CRJ :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_cr_names, & fields->f_CRj);
      break;
    case FR30_OPERAND_R13 :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_h_r13, & junk);
      break;
    case FR30_OPERAND_R14 :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_h_r14, & junk);
      break;
    case FR30_OPERAND_R15 :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_h_r15, & junk);
      break;
    case FR30_OPERAND_RI :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_gr_names, & fields->f_Ri);
      break;
    case FR30_OPERAND_RIC :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_gr_names, & fields->f_Ric);
      break;
    case FR30_OPERAND_RJ :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_gr_names, & fields->f_Rj);
      break;
    case FR30_OPERAND_RJC :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_gr_names, & fields->f_Rjc);
      break;
    case FR30_OPERAND_RS1 :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_dr_names, & fields->f_Rs1);
      break;
    case FR30_OPERAND_RS2 :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_dr_names, & fields->f_Rs2);
      break;
    case FR30_OPERAND_CC :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_CC, (unsigned long *) (& fields->f_cc));
      break;
    case FR30_OPERAND_CCC :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_CCC, (unsigned long *) (& fields->f_ccc));
      break;
    case FR30_OPERAND_DIR10 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_DIR10, (unsigned long *) (& fields->f_dir10));
      break;
    case FR30_OPERAND_DIR8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_DIR8, (unsigned long *) (& fields->f_dir8));
      break;
    case FR30_OPERAND_DIR9 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_DIR9, (unsigned long *) (& fields->f_dir9));
      break;
    case FR30_OPERAND_DISP10 :
      errmsg = cgen_parse_signed_integer (cd, strp, FR30_OPERAND_DISP10, (long *) (& fields->f_disp10));
      break;
    case FR30_OPERAND_DISP8 :
      errmsg = cgen_parse_signed_integer (cd, strp, FR30_OPERAND_DISP8, (long *) (& fields->f_disp8));
      break;
    case FR30_OPERAND_DISP9 :
      errmsg = cgen_parse_signed_integer (cd, strp, FR30_OPERAND_DISP9, (long *) (& fields->f_disp9));
      break;
    case FR30_OPERAND_I20 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_I20, (unsigned long *) (& fields->f_i20));
      break;
    case FR30_OPERAND_I32 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_I32, (unsigned long *) (& fields->f_i32));
      break;
    case FR30_OPERAND_I8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_I8, (unsigned long *) (& fields->f_i8));
      break;
    case FR30_OPERAND_LABEL12 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, FR30_OPERAND_LABEL12, 0, NULL,  & value);
        fields->f_rel12 = value;
      }
      break;
    case FR30_OPERAND_LABEL9 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, FR30_OPERAND_LABEL9, 0, NULL,  & value);
        fields->f_rel9 = value;
      }
      break;
    case FR30_OPERAND_M4 :
      errmsg = cgen_parse_signed_integer (cd, strp, FR30_OPERAND_M4, (long *) (& fields->f_m4));
      break;
    case FR30_OPERAND_PS :
      errmsg = cgen_parse_keyword (cd, strp, & fr30_cgen_opval_h_ps, & junk);
      break;
    case FR30_OPERAND_REGLIST_HI_LD :
      errmsg = parse_hi_register_list_ld (cd, strp, FR30_OPERAND_REGLIST_HI_LD, (unsigned long *) (& fields->f_reglist_hi_ld));
      break;
    case FR30_OPERAND_REGLIST_HI_ST :
      errmsg = parse_hi_register_list_st (cd, strp, FR30_OPERAND_REGLIST_HI_ST, (unsigned long *) (& fields->f_reglist_hi_st));
      break;
    case FR30_OPERAND_REGLIST_LOW_LD :
      errmsg = parse_low_register_list_ld (cd, strp, FR30_OPERAND_REGLIST_LOW_LD, (unsigned long *) (& fields->f_reglist_low_ld));
      break;
    case FR30_OPERAND_REGLIST_LOW_ST :
      errmsg = parse_low_register_list_st (cd, strp, FR30_OPERAND_REGLIST_LOW_ST, (unsigned long *) (& fields->f_reglist_low_st));
      break;
    case FR30_OPERAND_S10 :
      errmsg = cgen_parse_signed_integer (cd, strp, FR30_OPERAND_S10, (long *) (& fields->f_s10));
      break;
    case FR30_OPERAND_U10 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_U10, (unsigned long *) (& fields->f_u10));
      break;
    case FR30_OPERAND_U4 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_U4, (unsigned long *) (& fields->f_u4));
      break;
    case FR30_OPERAND_U4C :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_U4C, (unsigned long *) (& fields->f_u4c));
      break;
    case FR30_OPERAND_U8 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_U8, (unsigned long *) (& fields->f_u8));
      break;
    case FR30_OPERAND_UDISP6 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FR30_OPERAND_UDISP6, (unsigned long *) (& fields->f_udisp6));
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while parsing.\n"), opindex);
      abort ();
  }

  return errmsg;
}

cgen_parse_fn * const fr30_cgen_parse_handlers[] = 
{
  parse_insn_normal,
};

void
fr30_cgen_init_asm (CGEN_CPU_DESC cd)
{
  fr30_cgen_init_opcode_table (cd);
  fr30_cgen_init_ibld_table (cd);
  cd->parse_handlers = & fr30_cgen_parse_handlers[0];
  cd->parse_operand = fr30_cgen_parse_operand;
}



/* Regex construction routine.

   This translates an opcode syntax string into a regex string,
   by replacing any non-character syntax element (such as an
   opcode) with the pattern '.*'

   It then compiles the regex and stores it in the opcode, for
   later use by fr30_cgen_assemble_insn

   Returns NULL for success, an error message for failure.  */

char * 
fr30_cgen_build_insn_regex (CGEN_INSN *insn)
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
fr30_cgen_assemble_insn (CGEN_CPU_DESC cd,
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
      if (! fr30_cgen_insn_supported (cd, insn))
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
