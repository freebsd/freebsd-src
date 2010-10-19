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
#include "frv-desc.h"
#include "frv-opc.h"
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
inline static const char *
parse_symbolic_address (CGEN_CPU_DESC cd,
			const char **strp,
			int opindex,
			int opinfo,
			enum cgen_parse_operand_result *resultp,
			bfd_vma *valuep)
{
  enum cgen_parse_operand_result result_type;
  const char *errmsg = (* cd->parse_operand_fn)
    (cd, CGEN_PARSE_OPERAND_SYMBOLIC, strp, opindex, opinfo,
     &result_type, valuep);

  if (errmsg == NULL
      && result_type != CGEN_PARSE_OPERAND_RESULT_QUEUED)
    return "symbolic expression required";

  if (resultp)
    *resultp = result_type;

  return errmsg;
}

static const char *
parse_ldd_annotation (CGEN_CPU_DESC cd,
		      const char **strp,
		      int opindex,
		      unsigned long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_vma value;

  if (**strp == '#' || **strp == '%')
    {
      if (strncasecmp (*strp + 1, "tlsdesc(", 8) == 0)
	{
	  *strp += 9;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_TLSDESC_RELAX,
					   &result_type, &value);
	  if (**strp != ')')
	    return "missing ')'";
	  if (valuep)
	    *valuep = value;
	  ++*strp;
	  if (errmsg)
	    return errmsg;
	}
    }
  
  while (**strp == ' ' || **strp == '\t')
    ++*strp;
  
  if (**strp != '@')
    return "missing `@'";

  ++*strp;

  return NULL;
}

static const char *
parse_call_annotation (CGEN_CPU_DESC cd,
		       const char **strp,
		       int opindex,
		       unsigned long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_vma value;

  if (**strp == '#' || **strp == '%')
    {
      if (strncasecmp (*strp + 1, "gettlsoff(", 10) == 0)
	{
	  *strp += 11;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GETTLSOFF_RELAX,
					   &result_type, &value);
	  if (**strp != ')')
	    return "missing ')'";
	  if (valuep)
	    *valuep = value;
	  ++*strp;
	  if (errmsg)
	    return errmsg;
	}
    }
  
  while (**strp == ' ' || **strp == '\t')
    ++*strp;
  
  if (**strp != '@')
    return "missing `@'";

  ++*strp;

  return NULL;
}

static const char *
parse_ld_annotation (CGEN_CPU_DESC cd,
		     const char **strp,
		     int opindex,
		     unsigned long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_vma value;

  if (**strp == '#' || **strp == '%')
    {
      if (strncasecmp (*strp + 1, "tlsoff(", 7) == 0)
	{
	  *strp += 8;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_TLSOFF_RELAX,
					   &result_type, &value);
	  if (**strp != ')')
	    return "missing ')'";
	  if (valuep)
	    *valuep = value;
	  ++*strp;
	  if (errmsg)
	    return errmsg;
	}
    }
  
  while (**strp == ' ' || **strp == '\t')
    ++*strp;
  
  if (**strp != '@')
    return "missing `@'";

  ++*strp;

  return NULL;
}

static const char *
parse_ulo16 (CGEN_CPU_DESC cd,
	     const char **strp,
	     int opindex,
	     unsigned long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_vma value;
 
  if (**strp == '#' || **strp == '%')
    {
      if (strncasecmp (*strp + 1, "lo(", 3) == 0)
	{
	  *strp += 4;
	  errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_FRV_LO16,
				       & result_type, & value);
	  if (**strp != ')')
	    return "missing `)'";
	  ++*strp;
	  if (errmsg == NULL
	      && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	    value &= 0xffff;
	  *valuep = value;
	  return errmsg;
	}
      if (strncasecmp (*strp + 1, "gprello(", 8) == 0)
	{
	  *strp += 9;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GPRELLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotlo(", 6) == 0)
	{
	  *strp += 7;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotfuncdesclo(", 14) == 0)
	{
	  *strp += 15;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOTLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotofflo(", 9) == 0)
	{
	  *strp += 10;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTOFFLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotofffuncdesclo(", 17) == 0)
	{
	  *strp += 18;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOTOFFLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsdesclo(", 13) == 0)
	{
	  *strp += 14;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSDESCLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "tlsmofflo(", 10) == 0)
	{
	  *strp += 11;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_TLSMOFFLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsofflo(", 12) == 0)
	{
	  *strp += 13;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSOFFLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
    }
  return cgen_parse_unsigned_integer (cd, strp, opindex, valuep);
}

static const char *
parse_uslo16 (CGEN_CPU_DESC cd,
	      const char **strp,
	      int opindex,
	      signed long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_vma value;
 
  if (**strp == '#' || **strp == '%')
    {
      if (strncasecmp (*strp + 1, "lo(", 3) == 0)
	{
	  *strp += 4;
	  errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_FRV_LO16,
				       & result_type, & value);
	  if (**strp != ')')
	    return "missing `)'";
	  ++*strp;
	  if (errmsg == NULL
	      && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	    value &= 0xffff;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gprello(", 8) == 0)
	{
	  *strp += 9;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GPRELLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotlo(", 6) == 0)
	{
	  *strp += 7;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotfuncdesclo(", 14) == 0)
	{
	  *strp += 15;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOTLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotofflo(", 9) == 0)
	{
	  *strp += 10;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTOFFLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotofffuncdesclo(", 17) == 0)
	{
	  *strp += 18;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOTOFFLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsdesclo(", 13) == 0)
	{
	  *strp += 14;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSDESCLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "tlsmofflo(", 10) == 0)
	{
	  *strp += 11;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_TLSMOFFLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsofflo(", 12) == 0)
	{
	  *strp += 13;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSOFFLO,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
    }
  return cgen_parse_signed_integer (cd, strp, opindex, valuep);
}

static const char *
parse_uhi16 (CGEN_CPU_DESC cd,
	     const char **strp,
	     int opindex,
	     unsigned long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_vma value;
 
  if (**strp == '#' || **strp == '%')
    {
      if (strncasecmp (*strp + 1, "hi(", 3) == 0)
	{
	  *strp += 4;
	  errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_FRV_HI16,
				       & result_type, & value);
	  if (**strp != ')')
	    return "missing `)'";
	  ++*strp;
	  if (errmsg == NULL
	      && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	    {
	      /* If bfd_vma is wider than 32 bits, but we have a sign-
		 or zero-extension, truncate it.  */
	      if (value >= - ((bfd_vma)1 << 31)
		  || value <= ((bfd_vma)1 << 31) - (bfd_vma)1)
		value &= (((bfd_vma)1 << 16) << 16) - 1;
	      value >>= 16;
	    }
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gprelhi(", 8) == 0)
	{
	  *strp += 9;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GPRELHI,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gothi(", 6) == 0)
	{
	  *strp += 7;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTHI,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotfuncdeschi(", 14) == 0)
	{
	  *strp += 15;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOTHI,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotoffhi(", 9) == 0)
	{
	  *strp += 10;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTOFFHI,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotofffuncdeschi(", 17) == 0)
	{
	  *strp += 18;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOTOFFHI,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsdeschi(", 13) == 0)
	{
	  *strp += 14;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSDESCHI,
					   &result_type, &value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "tlsmoffhi(", 10) == 0)
	{
	  *strp += 11;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_TLSMOFFHI,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsoffhi(", 12) == 0)
	{
	  *strp += 13;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSOFFHI,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
    }
  return cgen_parse_unsigned_integer (cd, strp, opindex, valuep);
}

static long
parse_register_number (const char **strp)
{
  int regno;

  if (**strp < '0' || **strp > '9')
    return -1; /* error */

  regno = **strp - '0';
  for (++*strp; **strp >= '0' && **strp <= '9'; ++*strp)
    regno = regno * 10 + (**strp - '0');

  return regno;
}

static const char *
parse_spr (CGEN_CPU_DESC cd,
	   const char **strp,
	   CGEN_KEYWORD * table,
	   long *valuep)
{
  const char *save_strp;
  long regno;

  /* Check for spr index notation.  */
  if (strncasecmp (*strp, "spr[", 4) == 0)
    {
      *strp += 4;
      regno = parse_register_number (strp);
      if (**strp != ']')
        return _("missing `]'");
      ++*strp;
      if (! spr_valid (regno))
	return _("Special purpose register number is out of range");
      *valuep = regno;
      return NULL;
    }

  save_strp = *strp;
  regno = parse_register_number (strp);
  if (regno != -1)
    {
      if (! spr_valid (regno))
	return _("Special purpose register number is out of range");
      *valuep = regno;
      return NULL;
    }

  *strp = save_strp;
  return cgen_parse_keyword (cd, strp, table, valuep);
}

static const char *
parse_d12 (CGEN_CPU_DESC cd,
	   const char **strp,
	   int opindex,
	   long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_vma value;
 
  /* Check for small data reference.  */
  if (**strp == '#' || **strp == '%')
    {
      if (strncasecmp (*strp + 1, "gprel12(", 8) == 0)
        {
          *strp += 9;
          errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GPREL12,
					   & result_type, & value);
          if (**strp != ')')
            return "missing `)'";
          ++*strp;
          *valuep = value;
          return errmsg;
        }
      else if (strncasecmp (*strp + 1, "got12(", 6) == 0)
	{
	  *strp += 7;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOT12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotfuncdesc12(", 14) == 0)
	{
	  *strp += 15;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOT12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotoff12(", 9) == 0)
	{
	  *strp += 10;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTOFF12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotofffuncdesc12(", 17) == 0)
	{
	  *strp += 18;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOTOFF12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsdesc12(", 13) == 0)
	{
	  *strp += 14;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSDESC12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "tlsmoff12(", 10) == 0)
	{
	  *strp += 11;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_TLSMOFF12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsoff12(", 12) == 0)
	{
	  *strp += 13;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSOFF12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
    }
  return cgen_parse_signed_integer (cd, strp, opindex, valuep);
}

static const char *
parse_s12 (CGEN_CPU_DESC cd,
	   const char **strp,
	   int opindex,
	   long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_vma value;
 
  /* Check for small data reference.  */
  if (**strp == '#' || **strp == '%')
    {
      if (strncasecmp (*strp + 1, "gprel12(", 8) == 0)
	{
	  *strp += 9;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GPREL12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing `)'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "got12(", 6) == 0)
	{
	  *strp += 7;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOT12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotfuncdesc12(", 14) == 0)
	{
	  *strp += 15;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOT12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotoff12(", 9) == 0)
	{
	  *strp += 10;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTOFF12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gotofffuncdesc12(", 17) == 0)
	{
	  *strp += 18;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_FUNCDESC_GOTOFF12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsdesc12(", 13) == 0)
	{
	  *strp += 14;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSDESC12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "tlsmoff12(", 10) == 0)
	{
	  *strp += 11;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_TLSMOFF12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
      else if (strncasecmp (*strp + 1, "gottlsoff12(", 12) == 0)
	{
	  *strp += 13;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GOTTLSOFF12,
					   & result_type, & value);
	  if (**strp != ')')
	    return "missing ')'";
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
    }

  if (**strp == '#')
    ++*strp;
  return cgen_parse_signed_integer (cd, strp, opindex, valuep);
}

static const char *
parse_u12 (CGEN_CPU_DESC cd,
	   const char **strp,
	   int opindex,
	   long *valuep)
{
  const char *errmsg;
  enum cgen_parse_operand_result result_type;
  bfd_vma value;
 
  /* Check for small data reference.  */
  if ((**strp == '#' || **strp == '%')
      && strncasecmp (*strp + 1, "gprel12(", 8) == 0)
    {
      *strp += 9;
      errmsg = parse_symbolic_address (cd, strp, opindex,
				       BFD_RELOC_FRV_GPRELU12,
				       & result_type, & value);
      if (**strp != ')')
        return "missing `)'";
      ++*strp;
      *valuep = value;
      return errmsg;
    }
  else
    {
      if (**strp == '#')
        ++*strp;
      return cgen_parse_signed_integer (cd, strp, opindex, valuep);
    }
}

static const char *
parse_A (CGEN_CPU_DESC cd,
	 const char **strp,
	 int opindex,
	 unsigned long *valuep,
	 unsigned long A)
{
  const char *errmsg;
 
  if (**strp == '#')
    ++*strp;

  errmsg = cgen_parse_unsigned_integer (cd, strp, opindex, valuep);
  if (errmsg)
    return errmsg;

  if (*valuep != A)
    return _("Value of A operand must be 0 or 1");

  return NULL;
}

static const char *
parse_A0 (CGEN_CPU_DESC cd,
	  const char **strp,
	  int opindex,
	  unsigned long *valuep)
{
  return parse_A (cd, strp, opindex, valuep, 0);
}

static const char *
parse_A1 (CGEN_CPU_DESC cd,
	  const char **strp,
	  int opindex,
	  unsigned long *valuep)
{
  return parse_A (cd, strp, opindex, valuep, 1);
}

static const char *
parse_even_register (CGEN_CPU_DESC  cd,
		     const char **  strP,
		     CGEN_KEYWORD * tableP,
		     long *         valueP)
{
  const char * errmsg;
  const char * saved_star_strP = * strP;

  errmsg = cgen_parse_keyword (cd, strP, tableP, valueP);

  if (errmsg == NULL && ((* valueP) & 1))
    {
      errmsg = _("register number must be even");
      * strP = saved_star_strP;
    }

  return errmsg;
}

static const char *
parse_call_label (CGEN_CPU_DESC cd,
		  const char **strp,
		  int opindex,
		  int opinfo,
		  enum cgen_parse_operand_result *resultp,
		  bfd_vma *valuep)
{
  const char *errmsg;
  bfd_vma value;
 
  /* Check for small data reference.  */
  if (opinfo == 0 && (**strp == '#' || **strp == '%'))
    {
      if (strncasecmp (*strp + 1, "gettlsoff(", 10) == 0)
	{
	  *strp += 11;
	  errmsg = parse_symbolic_address (cd, strp, opindex,
					   BFD_RELOC_FRV_GETTLSOFF,
					   resultp, &value);
	  if (**strp != ')')
	    return _("missing `)'");
	  ++*strp;
	  *valuep = value;
	  return errmsg;
	}
    }

  return cgen_parse_address (cd, strp, opindex, opinfo, resultp, valuep);
}

/* -- */

const char * frv_cgen_parse_operand
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
frv_cgen_parse_operand (CGEN_CPU_DESC cd,
			   int opindex,
			   const char ** strp,
			   CGEN_FIELDS * fields)
{
  const char * errmsg = NULL;
  /* Used by scalar operands that still need to be parsed.  */
  long junk ATTRIBUTE_UNUSED;

  switch (opindex)
    {
    case FRV_OPERAND_A0 :
      errmsg = parse_A0 (cd, strp, FRV_OPERAND_A0, (unsigned long *) (& fields->f_A));
      break;
    case FRV_OPERAND_A1 :
      errmsg = parse_A1 (cd, strp, FRV_OPERAND_A1, (unsigned long *) (& fields->f_A));
      break;
    case FRV_OPERAND_ACC40SI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_acc_names, & fields->f_ACC40Si);
      break;
    case FRV_OPERAND_ACC40SK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_acc_names, & fields->f_ACC40Sk);
      break;
    case FRV_OPERAND_ACC40UI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_acc_names, & fields->f_ACC40Ui);
      break;
    case FRV_OPERAND_ACC40UK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_acc_names, & fields->f_ACC40Uk);
      break;
    case FRV_OPERAND_ACCGI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_accg_names, & fields->f_ACCGi);
      break;
    case FRV_OPERAND_ACCGK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_accg_names, & fields->f_ACCGk);
      break;
    case FRV_OPERAND_CCI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_cccr_names, & fields->f_CCi);
      break;
    case FRV_OPERAND_CPRDOUBLEK :
      errmsg = parse_even_register (cd, strp, & frv_cgen_opval_cpr_names, & fields->f_CPRk);
      break;
    case FRV_OPERAND_CPRI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_cpr_names, & fields->f_CPRi);
      break;
    case FRV_OPERAND_CPRJ :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_cpr_names, & fields->f_CPRj);
      break;
    case FRV_OPERAND_CPRK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_cpr_names, & fields->f_CPRk);
      break;
    case FRV_OPERAND_CRI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_cccr_names, & fields->f_CRi);
      break;
    case FRV_OPERAND_CRJ :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_cccr_names, & fields->f_CRj);
      break;
    case FRV_OPERAND_CRJ_FLOAT :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_cccr_names, & fields->f_CRj_float);
      break;
    case FRV_OPERAND_CRJ_INT :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_cccr_names, & fields->f_CRj_int);
      break;
    case FRV_OPERAND_CRK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_cccr_names, & fields->f_CRk);
      break;
    case FRV_OPERAND_FCCI_1 :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fccr_names, & fields->f_FCCi_1);
      break;
    case FRV_OPERAND_FCCI_2 :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fccr_names, & fields->f_FCCi_2);
      break;
    case FRV_OPERAND_FCCI_3 :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fccr_names, & fields->f_FCCi_3);
      break;
    case FRV_OPERAND_FCCK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fccr_names, & fields->f_FCCk);
      break;
    case FRV_OPERAND_FRDOUBLEI :
      errmsg = parse_even_register (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRi);
      break;
    case FRV_OPERAND_FRDOUBLEJ :
      errmsg = parse_even_register (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRj);
      break;
    case FRV_OPERAND_FRDOUBLEK :
      errmsg = parse_even_register (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRi);
      break;
    case FRV_OPERAND_FRINTI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRi);
      break;
    case FRV_OPERAND_FRINTIEVEN :
      errmsg = parse_even_register (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRi);
      break;
    case FRV_OPERAND_FRINTJ :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRj);
      break;
    case FRV_OPERAND_FRINTJEVEN :
      errmsg = parse_even_register (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRj);
      break;
    case FRV_OPERAND_FRINTK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRINTKEVEN :
      errmsg = parse_even_register (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRJ :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRj);
      break;
    case FRV_OPERAND_FRK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRKHI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRk);
      break;
    case FRV_OPERAND_FRKLO :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_fr_names, & fields->f_FRk);
      break;
    case FRV_OPERAND_GRDOUBLEK :
      errmsg = parse_even_register (cd, strp, & frv_cgen_opval_gr_names, & fields->f_GRk);
      break;
    case FRV_OPERAND_GRI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_gr_names, & fields->f_GRi);
      break;
    case FRV_OPERAND_GRJ :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_gr_names, & fields->f_GRj);
      break;
    case FRV_OPERAND_GRK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_gr_names, & fields->f_GRk);
      break;
    case FRV_OPERAND_GRKHI :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_gr_names, & fields->f_GRk);
      break;
    case FRV_OPERAND_GRKLO :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_gr_names, & fields->f_GRk);
      break;
    case FRV_OPERAND_ICCI_1 :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_iccr_names, & fields->f_ICCi_1);
      break;
    case FRV_OPERAND_ICCI_2 :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_iccr_names, & fields->f_ICCi_2);
      break;
    case FRV_OPERAND_ICCI_3 :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_iccr_names, & fields->f_ICCi_3);
      break;
    case FRV_OPERAND_LI :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_LI, (unsigned long *) (& fields->f_LI));
      break;
    case FRV_OPERAND_LRAD :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_LRAD, (unsigned long *) (& fields->f_LRAD));
      break;
    case FRV_OPERAND_LRAE :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_LRAE, (unsigned long *) (& fields->f_LRAE));
      break;
    case FRV_OPERAND_LRAS :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_LRAS, (unsigned long *) (& fields->f_LRAS));
      break;
    case FRV_OPERAND_TLBPRL :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_TLBPRL, (unsigned long *) (& fields->f_TLBPRL));
      break;
    case FRV_OPERAND_TLBPROPX :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_TLBPROPX, (unsigned long *) (& fields->f_TLBPRopx));
      break;
    case FRV_OPERAND_AE :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_AE, (unsigned long *) (& fields->f_ae));
      break;
    case FRV_OPERAND_CALLANN :
      errmsg = parse_call_annotation (cd, strp, FRV_OPERAND_CALLANN, (unsigned long *) (& fields->f_reloc_ann));
      break;
    case FRV_OPERAND_CCOND :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_CCOND, (unsigned long *) (& fields->f_ccond));
      break;
    case FRV_OPERAND_COND :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_COND, (unsigned long *) (& fields->f_cond));
      break;
    case FRV_OPERAND_D12 :
      errmsg = parse_d12 (cd, strp, FRV_OPERAND_D12, (long *) (& fields->f_d12));
      break;
    case FRV_OPERAND_DEBUG :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_DEBUG, (unsigned long *) (& fields->f_debug));
      break;
    case FRV_OPERAND_EIR :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_EIR, (unsigned long *) (& fields->f_eir));
      break;
    case FRV_OPERAND_HINT :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_HINT, (unsigned long *) (& fields->f_hint));
      break;
    case FRV_OPERAND_HINT_NOT_TAKEN :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_h_hint_not_taken, & fields->f_hint);
      break;
    case FRV_OPERAND_HINT_TAKEN :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_h_hint_taken, & fields->f_hint);
      break;
    case FRV_OPERAND_LABEL16 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, FRV_OPERAND_LABEL16, 0, NULL,  & value);
        fields->f_label16 = value;
      }
      break;
    case FRV_OPERAND_LABEL24 :
      {
        bfd_vma value = 0;
        errmsg = parse_call_label (cd, strp, FRV_OPERAND_LABEL24, 0, NULL,  & value);
        fields->f_label24 = value;
      }
      break;
    case FRV_OPERAND_LDANN :
      errmsg = parse_ld_annotation (cd, strp, FRV_OPERAND_LDANN, (unsigned long *) (& fields->f_reloc_ann));
      break;
    case FRV_OPERAND_LDDANN :
      errmsg = parse_ldd_annotation (cd, strp, FRV_OPERAND_LDDANN, (unsigned long *) (& fields->f_reloc_ann));
      break;
    case FRV_OPERAND_LOCK :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_LOCK, (unsigned long *) (& fields->f_lock));
      break;
    case FRV_OPERAND_PACK :
      errmsg = cgen_parse_keyword (cd, strp, & frv_cgen_opval_h_pack, & fields->f_pack);
      break;
    case FRV_OPERAND_S10 :
      errmsg = cgen_parse_signed_integer (cd, strp, FRV_OPERAND_S10, (long *) (& fields->f_s10));
      break;
    case FRV_OPERAND_S12 :
      errmsg = parse_s12 (cd, strp, FRV_OPERAND_S12, (long *) (& fields->f_d12));
      break;
    case FRV_OPERAND_S16 :
      errmsg = cgen_parse_signed_integer (cd, strp, FRV_OPERAND_S16, (long *) (& fields->f_s16));
      break;
    case FRV_OPERAND_S5 :
      errmsg = cgen_parse_signed_integer (cd, strp, FRV_OPERAND_S5, (long *) (& fields->f_s5));
      break;
    case FRV_OPERAND_S6 :
      errmsg = cgen_parse_signed_integer (cd, strp, FRV_OPERAND_S6, (long *) (& fields->f_s6));
      break;
    case FRV_OPERAND_S6_1 :
      errmsg = cgen_parse_signed_integer (cd, strp, FRV_OPERAND_S6_1, (long *) (& fields->f_s6_1));
      break;
    case FRV_OPERAND_SLO16 :
      errmsg = parse_uslo16 (cd, strp, FRV_OPERAND_SLO16, (long *) (& fields->f_s16));
      break;
    case FRV_OPERAND_SPR :
      errmsg = parse_spr (cd, strp, & frv_cgen_opval_spr_names, & fields->f_spr);
      break;
    case FRV_OPERAND_U12 :
      errmsg = parse_u12 (cd, strp, FRV_OPERAND_U12, (long *) (& fields->f_u12));
      break;
    case FRV_OPERAND_U16 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_U16, (unsigned long *) (& fields->f_u16));
      break;
    case FRV_OPERAND_U6 :
      errmsg = cgen_parse_unsigned_integer (cd, strp, FRV_OPERAND_U6, (unsigned long *) (& fields->f_u6));
      break;
    case FRV_OPERAND_UHI16 :
      errmsg = parse_uhi16 (cd, strp, FRV_OPERAND_UHI16, (unsigned long *) (& fields->f_u16));
      break;
    case FRV_OPERAND_ULO16 :
      errmsg = parse_ulo16 (cd, strp, FRV_OPERAND_ULO16, (unsigned long *) (& fields->f_u16));
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while parsing.\n"), opindex);
      abort ();
  }

  return errmsg;
}

cgen_parse_fn * const frv_cgen_parse_handlers[] = 
{
  parse_insn_normal,
};

void
frv_cgen_init_asm (CGEN_CPU_DESC cd)
{
  frv_cgen_init_opcode_table (cd);
  frv_cgen_init_ibld_table (cd);
  cd->parse_handlers = & frv_cgen_parse_handlers[0];
  cd->parse_operand = frv_cgen_parse_operand;
}



/* Regex construction routine.

   This translates an opcode syntax string into a regex string,
   by replacing any non-character syntax element (such as an
   opcode) with the pattern '.*'

   It then compiles the regex and stores it in the opcode, for
   later use by frv_cgen_assemble_insn

   Returns NULL for success, an error message for failure.  */

char * 
frv_cgen_build_insn_regex (CGEN_INSN *insn)
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
frv_cgen_assemble_insn (CGEN_CPU_DESC cd,
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
      if (! frv_cgen_insn_supported (cd, insn))
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
