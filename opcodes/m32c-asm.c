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
#include "m32c-desc.h"
#include "m32c-opc.h"
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
#include "safe-ctype.h"

#define MACH_M32C 5		/* Must match md_begin.  */

static int
m32c_cgen_isa_register (const char **strp)
 {
   int u;
   const char *s = *strp;
   static char * m32c_register_names [] = 
     {
       "r0", "r1", "r2", "r3", "r0l", "r0h", "r1l", "r1h",
       "a0", "a1", "r2r0", "r3r1", "sp", "fb", "dct0", "dct1", "flg", "svf",
       "drc0", "drc1", "dmd0", "dmd1", "intb", "svp", "vct", "isp", "dma0",
       "dma1", "dra0", "dra1", "dsa0", "dsa1", 0
     };
 
   for (u = 0; m32c_register_names[u]; u++)
     {
       int len = strlen (m32c_register_names[u]);

       if (memcmp (m32c_register_names[u], s, len) == 0
	   && (s[len] == 0 || ! ISALNUM (s[len])))
        return 1;
     }
   return 0;
}

#define PARSE_UNSIGNED							\
  do									\
    {									\
      /* Don't successfully parse literals beginning with '['.  */	\
      if (**strp == '[')						\
	return "Invalid literal"; /* Anything -- will not be seen.  */	\
									\
      errmsg = cgen_parse_unsigned_integer (cd, strp, opindex, & value);\
      if (errmsg)							\
	return errmsg;							\
    }									\
  while (0)

#define PARSE_SIGNED							\
  do									\
    {									\
      /* Don't successfully parse literals beginning with '['.  */	\
      if (**strp == '[')						\
	return "Invalid literal"; /* Anything -- will not be seen.  */	\
									\
      errmsg = cgen_parse_signed_integer (cd, strp, opindex, & value);  \
      if (errmsg)							\
	return errmsg;							\
    }									\
  while (0)

static const char *
parse_unsigned6 (CGEN_CPU_DESC cd, const char **strp,
		 int opindex, unsigned long *valuep)
{
  const char *errmsg = 0;
  unsigned long value;

  PARSE_UNSIGNED;

  if (value > 0x3f)
    return _("imm:6 immediate is out of range");

  *valuep = value;
  return 0;
}

static const char *
parse_unsigned8 (CGEN_CPU_DESC cd, const char **strp,
		 int opindex, unsigned long *valuep)
{
  const char *errmsg = 0;
  unsigned long value;
  long have_zero = 0;

  if (strncasecmp (*strp, "%dsp8(", 6) == 0)
    {
      enum cgen_parse_operand_result result_type;
      bfd_vma value;
      const char *errmsg;

      *strp += 6;
      errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_8,
				   & result_type, & value);
      if (**strp != ')')
	return _("missing `)'");
      (*strp) ++;

      if (errmsg == NULL
  	  && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  return _("%dsp8() takes a symbolic address, not a number");
	}
      *valuep = value;
      return errmsg;
    }

  if (strncmp (*strp, "0x0", 3) == 0 
      || (**strp == '0' && *(*strp + 1) != 'x'))
    have_zero = 1;

  PARSE_UNSIGNED;

  if (value > 0xff)
    return _("dsp:8 immediate is out of range");

  /* If this field may require a relocation then use larger dsp16.  */
  if (! have_zero && value == 0)
    return _("dsp:8 immediate is out of range");

  *valuep = value;
  return 0;
}

static const char *
parse_signed4 (CGEN_CPU_DESC cd, const char **strp,
	       int opindex, signed long *valuep)
{
  const char *errmsg = 0;
  signed long value;
  long have_zero = 0;

  if (strncmp (*strp, "0x0", 3) == 0 
      || (**strp == '0' && *(*strp + 1) != 'x'))
    have_zero = 1;

  PARSE_SIGNED;

  if (value < -8 || value > 7)
    return _("Immediate is out of range -8 to 7");

  /* If this field may require a relocation then use larger dsp16.  */
  if (! have_zero && value == 0)
    return _("Immediate is out of range -8 to 7");

  *valuep = value;
  return 0;
}

static const char *
parse_signed4n (CGEN_CPU_DESC cd, const char **strp,
		int opindex, signed long *valuep)
{
  const char *errmsg = 0;
  signed long value;
  long have_zero = 0;

  if (strncmp (*strp, "0x0", 3) == 0 
      || (**strp == '0' && *(*strp + 1) != 'x'))
    have_zero = 1;

  PARSE_SIGNED;

  if (value < -7 || value > 8)
    return _("Immediate is out of range -7 to 8");

  /* If this field may require a relocation then use larger dsp16.  */
  if (! have_zero && value == 0)
    return _("Immediate is out of range -7 to 8");

  *valuep = -value;
  return 0;
}

static const char *
parse_signed8 (CGEN_CPU_DESC cd, const char **strp,
	       int opindex, signed long *valuep)
{
  const char *errmsg = 0;
  signed long value;

  if (strncasecmp (*strp, "%hi8(", 5) == 0)
    {
      enum cgen_parse_operand_result result_type;
      bfd_vma value;
      const char *errmsg;

      *strp += 5;
      errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_M32C_HI8,
				   & result_type, & value);
      if (**strp != ')')
	return _("missing `)'");
      (*strp) ++;

      if (errmsg == NULL
  	  && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  value >>= 16;
	}
      *valuep = value;
      return errmsg;
    }

  PARSE_SIGNED;

  if (value <= 255 && value > 127)
    value -= 0x100;

  if (value < -128 || value > 127)
    return _("dsp:8 immediate is out of range");

  *valuep = value;
  return 0;
}

static const char *
parse_unsigned16 (CGEN_CPU_DESC cd, const char **strp,
		 int opindex, unsigned long *valuep)
{
  const char *errmsg = 0;
  unsigned long value;
  long have_zero = 0;

  if (strncasecmp (*strp, "%dsp16(", 7) == 0)
    {
      enum cgen_parse_operand_result result_type;
      bfd_vma value;
      const char *errmsg;

      *strp += 7;
      errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_16,
				   & result_type, & value);
      if (**strp != ')')
	return _("missing `)'");
      (*strp) ++;

      if (errmsg == NULL
  	  && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  return _("%dsp16() takes a symbolic address, not a number");
	}
      *valuep = value;
      return errmsg;
    }

  /* Don't successfully parse literals beginning with '['.  */
  if (**strp == '[')
    return "Invalid literal"; /* Anything -- will not be seen.  */

  /* Don't successfully parse register names.  */
  if (m32c_cgen_isa_register (strp))
    return "Invalid literal"; /* Anything -- will not be seen.  */

  if (strncmp (*strp, "0x0", 3) == 0 
      || (**strp == '0' && *(*strp + 1) != 'x'))
    have_zero = 1;
  
  errmsg = cgen_parse_unsigned_integer (cd, strp, opindex, & value);
  if (errmsg)
    return errmsg;

  if (value > 0xffff)
    return _("dsp:16 immediate is out of range");

  /* If this field may require a relocation then use larger dsp24.  */
  if (cd->machs == MACH_M32C && ! have_zero && value == 0
      && (strncmp (*strp, "[a", 2) == 0
	  || **strp == ','
	  || **strp == 0))
    return _("dsp:16 immediate is out of range");

  *valuep = value;
  return 0;
}

static const char *
parse_signed16 (CGEN_CPU_DESC cd, const char **strp,
	       int opindex, signed long *valuep)
{
  const char *errmsg = 0;
  signed long value;

  if (strncasecmp (*strp, "%lo16(", 6) == 0)
    {
      enum cgen_parse_operand_result result_type;
      bfd_vma value;
      const char *errmsg;

      *strp += 6;
      errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_LO16,
				   & result_type, & value);
      if (**strp != ')')
	return _("missing `)'");
      (*strp) ++;

      if (errmsg == NULL
  	  && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  value &= 0xffff;
	}
      *valuep = value;
      return errmsg;
    }

  if (strncasecmp (*strp, "%hi16(", 6) == 0)
    {
      enum cgen_parse_operand_result result_type;
      bfd_vma value;
      const char *errmsg;

      *strp += 6;
      errmsg = cgen_parse_address (cd, strp, opindex, BFD_RELOC_HI16,
				   & result_type, & value);
      if (**strp != ')')
	return _("missing `)'");
      (*strp) ++;

      if (errmsg == NULL
  	  && result_type == CGEN_PARSE_OPERAND_RESULT_NUMBER)
	{
	  value >>= 16;
	}
      *valuep = value;
      return errmsg;
    }

  PARSE_SIGNED;

  if (value <= 65535 && value > 32767)
    value -= 0x10000;

  if (value < -32768 || value > 32767)
    return _("dsp:16 immediate is out of range");

  *valuep = value;
  return 0;
}

static const char *
parse_unsigned20 (CGEN_CPU_DESC cd, const char **strp,
		 int opindex, unsigned long *valuep)
{
  const char *errmsg = 0;
  unsigned long value;
  
  /* Don't successfully parse literals beginning with '['.  */
  if (**strp == '[')
    return "Invalid literal"; /* Anything -- will not be seen.  */

  /* Don't successfully parse register names.  */
  if (m32c_cgen_isa_register (strp))
    return "Invalid literal"; /* Anything -- will not be seen.  */

  errmsg = cgen_parse_unsigned_integer (cd, strp, opindex, & value);
  if (errmsg)
    return errmsg;

  if (value > 0xfffff)
    return _("dsp:20 immediate is out of range");

  *valuep = value;
  return 0;
}

static const char *
parse_unsigned24 (CGEN_CPU_DESC cd, const char **strp,
		 int opindex, unsigned long *valuep)
{
  const char *errmsg = 0;
  unsigned long value;
  
  /* Don't successfully parse literals beginning with '['.  */
  if (**strp == '[')
    return "Invalid literal"; /* Anything -- will not be seen.  */

  /* Don't successfully parse register names.  */
  if (m32c_cgen_isa_register (strp))
    return "Invalid literal"; /* Anything -- will not be seen.  */

  errmsg = cgen_parse_unsigned_integer (cd, strp, opindex, & value);
  if (errmsg)
    return errmsg;

  if (value > 0xffffff)
    return _("dsp:24 immediate is out of range");

  *valuep = value;
  return 0;
}

/* This should only be used for #imm->reg.  */
static const char *
parse_signed24 (CGEN_CPU_DESC cd, const char **strp,
		 int opindex, signed long *valuep)
{
  const char *errmsg = 0;
  signed long value;

  PARSE_SIGNED;

  if (value <= 0xffffff && value > 0x7fffff)
    value -= 0x1000000;

  if (value > 0xffffff)
    return _("dsp:24 immediate is out of range");

  *valuep = value;
  return 0;
}

static const char *
parse_signed32 (CGEN_CPU_DESC cd, const char **strp,
		int opindex, signed long *valuep)
{
  const char *errmsg = 0;
  signed long value;
  
  errmsg = cgen_parse_signed_integer (cd, strp, opindex, & value);
  if (errmsg)
    return errmsg;

  *valuep = value;
  return 0;
}

static const char *
parse_imm1_S (CGEN_CPU_DESC cd, const char **strp,
	     int opindex, signed long *valuep)
{
  const char *errmsg = 0;
  signed long value;

  errmsg = cgen_parse_signed_integer (cd, strp, opindex, & value);
  if (errmsg)
    return errmsg;

  if (value < 1 || value > 2)
    return _("immediate is out of range 1-2");

  *valuep = value;
  return 0;
}

static const char *
parse_imm3_S (CGEN_CPU_DESC cd, const char **strp,
	     int opindex, signed long *valuep)
{
  const char *errmsg = 0;
  signed long value;
  
  errmsg = cgen_parse_signed_integer (cd, strp, opindex, & value);
  if (errmsg)
    return errmsg;

  if (value < 1 || value > 8)
    return _("immediate is out of range 1-8");

  *valuep = value;
  return 0;
}

static const char *
parse_bit3_S (CGEN_CPU_DESC cd, const char **strp,
	     int opindex, signed long *valuep)
{
  const char *errmsg = 0;
  signed long value;
  
  errmsg = cgen_parse_signed_integer (cd, strp, opindex, & value);
  if (errmsg)
    return errmsg;

  if (value < 0 || value > 7)
    return _("immediate is out of range 0-7");

  *valuep = value;
  return 0;
}

static const char *
parse_lab_5_3 (CGEN_CPU_DESC cd,
	       const char **strp,
	       int opindex ATTRIBUTE_UNUSED,
	       int opinfo,
	       enum cgen_parse_operand_result *type_addr,
	       bfd_vma *valuep)
{
  const char *errmsg = 0;
  bfd_vma value;
  enum cgen_parse_operand_result op_res;

  errmsg = cgen_parse_address (cd, strp, M32C_OPERAND_LAB_5_3,
			       opinfo, & op_res, & value);

  if (type_addr)
    *type_addr = op_res;

  if (op_res == CGEN_PARSE_OPERAND_ADDRESS)
    {
      /* This is a hack; the field cannot handle near-zero signed
	 offsets that CGEN wants to put in to indicate an "empty"
	 operand at first.  */
      *valuep = 2;
      return 0;
    }
  if (errmsg)
    return errmsg;

  if (value < 2 || value > 9)
    return _("immediate is out of range 2-9");

  *valuep = value;
  return 0;
}

static const char *
parse_Bitno16R (CGEN_CPU_DESC cd, const char **strp,
		int opindex, unsigned long *valuep)
{
  const char *errmsg = 0;
  unsigned long value;

  errmsg = cgen_parse_unsigned_integer (cd, strp, opindex, & value);
  if (errmsg)
    return errmsg;

  if (value > 15)
    return _("Bit number for indexing general register is out of range 0-15");

  *valuep = value;
  return 0;
}

static const char *
parse_unsigned_bitbase (CGEN_CPU_DESC cd, const char **strp,
			int opindex, unsigned long *valuep,
			unsigned bits, int allow_syms)
{
  const char *errmsg = 0;
  unsigned long bit;
  unsigned long base;
  const char *newp = *strp;
  unsigned long long bitbase;
  long have_zero = 0;

  errmsg = cgen_parse_unsigned_integer (cd, & newp, opindex, & bit);
  if (errmsg)
    return errmsg;

  if (*newp != ',')
    return "Missing base for bit,base:8";

  ++newp;

  if (strncmp (newp, "0x0", 3) == 0 
      || (newp[0] == '0' && newp[1] != 'x'))
    have_zero = 1;

  errmsg = cgen_parse_unsigned_integer (cd, & newp, opindex, & base);
  if (errmsg)
    return errmsg;

  bitbase = (unsigned long long) bit + ((unsigned long long) base * 8);

  if (bitbase >= (1ull << bits))
    return _("bit,base is out of range");

  /* If this field may require a relocation then use larger displacement.  */
  if (! have_zero && base == 0)
    {
      switch (allow_syms) {
      case 0:
	return _("bit,base out of range for symbol");
      case 1:
	break;
      case 2:
	if (strncmp (newp, "[sb]", 4) != 0)
	  return _("bit,base out of range for symbol");
	break;
      }
    }

  *valuep = bitbase;
  *strp = newp;
  return 0;
}

static const char *
parse_signed_bitbase (CGEN_CPU_DESC cd, const char **strp,
		      int opindex, signed long *valuep,
		      unsigned bits, int allow_syms)
{
  const char *errmsg = 0;
  unsigned long bit;
  signed long base;
  const char *newp = *strp;
  long long bitbase;
  long long limit;
  long have_zero = 0;

  errmsg = cgen_parse_unsigned_integer (cd, & newp, opindex, & bit);
  if (errmsg)
    return errmsg;

  if (*newp != ',')
    return "Missing base for bit,base:8";

  ++newp;

  if (strncmp (newp, "0x0", 3) == 0 
      || (newp[0] == '0' && newp[1] != 'x'))
    have_zero = 1;

  errmsg = cgen_parse_signed_integer (cd, & newp, opindex, & base);
  if (errmsg)
    return errmsg;

  bitbase = (long long)bit + ((long long)base * 8);

  limit = 1ll << (bits - 1);
  if (bitbase < -limit || bitbase >= limit)
    return _("bit,base is out of range");

  /* If this field may require a relocation then use larger displacement.  */
  if (! have_zero && base == 0 && ! allow_syms)
    return _("bit,base out of range for symbol");

  *valuep = bitbase;
  *strp = newp;
  return 0;
}

static const char *
parse_unsigned_bitbase8 (CGEN_CPU_DESC cd, const char **strp,
			 int opindex, unsigned long *valuep)
{
  return parse_unsigned_bitbase (cd, strp, opindex, valuep, 8, 0);
}

static const char *
parse_unsigned_bitbase11 (CGEN_CPU_DESC cd, const char **strp,
			 int opindex, unsigned long *valuep)
{
  return parse_unsigned_bitbase (cd, strp, opindex, valuep, 11, 0);
}

static const char *
parse_unsigned_bitbase16 (CGEN_CPU_DESC cd, const char **strp,
			  int opindex, unsigned long *valuep)
{
  return parse_unsigned_bitbase (cd, strp, opindex, valuep, 16, 1);
}

static const char *
parse_unsigned_bitbase19 (CGEN_CPU_DESC cd, const char **strp,
			 int opindex, unsigned long *valuep)
{
  return parse_unsigned_bitbase (cd, strp, opindex, valuep, 19, 2);
}

static const char *
parse_unsigned_bitbase27 (CGEN_CPU_DESC cd, const char **strp,
			 int opindex, unsigned long *valuep)
{
  return parse_unsigned_bitbase (cd, strp, opindex, valuep, 27, 1);
}

static const char *
parse_signed_bitbase8 (CGEN_CPU_DESC cd, const char **strp,
		       int opindex, signed long *valuep)
{
  return parse_signed_bitbase (cd, strp, opindex, valuep, 8, 1);
}

static const char *
parse_signed_bitbase11 (CGEN_CPU_DESC cd, const char **strp,
		       int opindex, signed long *valuep)
{
  return parse_signed_bitbase (cd, strp, opindex, valuep, 11, 0);
}

static const char *
parse_signed_bitbase19 (CGEN_CPU_DESC cd, const char **strp,
		       int opindex, signed long *valuep)
{
  return parse_signed_bitbase (cd, strp, opindex, valuep, 19, 1);
}

/* Parse the suffix as :<char> or as nothing followed by a whitespace.  */

static const char *
parse_suffix (const char **strp, char suffix)
{
  const char *newp = *strp;
  
  if (**strp == ':' && TOLOWER (*(*strp + 1)) == suffix)
    newp = *strp + 2;

  if (ISSPACE (*newp))
    {
      *strp = newp;
      return 0;
    }
	
  return "Invalid suffix"; /* Anything -- will not be seen.  */
}

static const char *
parse_S (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED, const char **strp,
	 int opindex ATTRIBUTE_UNUSED, signed long *valuep ATTRIBUTE_UNUSED)
{
  return parse_suffix (strp, 's');
}

static const char *
parse_G (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED, const char **strp,
	 int opindex ATTRIBUTE_UNUSED, signed long *valuep ATTRIBUTE_UNUSED)
{
  return parse_suffix (strp, 'g');
}

static const char *
parse_Q (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED, const char **strp,
	 int opindex ATTRIBUTE_UNUSED, signed long *valuep ATTRIBUTE_UNUSED)
{
  return parse_suffix (strp, 'q');
}

static const char *
parse_Z (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED, const char **strp,
	 int opindex ATTRIBUTE_UNUSED, signed long *valuep ATTRIBUTE_UNUSED)
{
  return parse_suffix (strp, 'z');
}

/* Parse an empty suffix. Fail if the next char is ':'.  */

static const char *
parse_X (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED, const char **strp,
	 int opindex ATTRIBUTE_UNUSED, signed long *valuep ATTRIBUTE_UNUSED)
{
  if (**strp == ':')
    return "Unexpected suffix";
  return 0;
}

static const char *
parse_r0l_r0h (CGEN_CPU_DESC cd, const char **strp,
	       int opindex ATTRIBUTE_UNUSED, signed long *valuep)
{
  const char *errmsg;
  signed long value;
  signed long junk;
  const char *newp = *strp;

  /* Parse r0[hl].  */
  errmsg = cgen_parse_keyword (cd, & newp, & m32c_cgen_opval_h_r0l_r0h, & value);
  if (errmsg)
    return errmsg;

  if (*newp != ',')
    return _("not a valid r0l/r0h pair");
  ++newp;

  /* Parse the second register in the pair.  */
  if (value == 0) /* r0l */
    errmsg = cgen_parse_keyword (cd, & newp, & m32c_cgen_opval_h_r0h, & junk);
  else
    errmsg = cgen_parse_keyword (cd, & newp, & m32c_cgen_opval_h_r0l, & junk);
  if (errmsg)
    return errmsg;

  *strp = newp;
  *valuep = ! value;
  return 0;
}

/* Accept .b or .w in any case.  */

static const char *
parse_size (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED, const char **strp,
	    int opindex ATTRIBUTE_UNUSED, signed long *valuep ATTRIBUTE_UNUSED)
{
  if (**strp == '.'
      && (*(*strp + 1) == 'b' || *(*strp + 1) == 'B'
	  || *(*strp + 1) == 'w' || *(*strp + 1) == 'W'))
    {
      *strp += 2;
      return NULL;
    }

  return _("Invalid size specifier");
}

/* Special check to ensure that instruction exists for given machine.  */

int
m32c_cgen_insn_supported (CGEN_CPU_DESC cd,
			  const CGEN_INSN *insn)
{
  int machs = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_MACH);
  CGEN_BITSET isas = CGEN_INSN_BITSET_ATTR_VALUE (insn, CGEN_INSN_ISA);

  /* If attributes are absent, assume no restriction.  */
  if (machs == 0)
    machs = ~0;

  return ((machs & cd->machs)
          && cgen_bitset_intersect_p (& isas, cd->isas));
}

/* Parse a set of registers, R0,R1,A0,A1,SB,FB.  */

static const char *
parse_regset (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	      const char **strp,
	      int opindex ATTRIBUTE_UNUSED,
	      unsigned long *valuep,
	      int push)
{
  const char *errmsg = 0;
  int regno = 0;
 
  *valuep = 0;
  while (**strp && **strp != ')')
    {
      if (**strp == 'r' || **strp == 'R')
	{
	  ++*strp;
	  regno = **strp - '0';
	  if (regno > 4)
	    errmsg = _("Register number is not valid");
	}
      else if (**strp == 'a' || **strp == 'A')
	{
	  ++*strp;
	  regno = **strp - '0';
	  if (regno > 2)
	    errmsg = _("Register number is not valid");
	  regno = **strp - '0' + 4;
	}
      
      else if (strncasecmp (*strp, "sb", 2) == 0 || strncasecmp (*strp, "SB", 2) == 0)
	{
	  regno = 6;
	  ++*strp;
	}
      
      else if (strncasecmp (*strp, "fb", 2) == 0 || strncasecmp (*strp, "FB", 2) == 0)
	{
	  regno = 7;
	  ++*strp;
	}
      
      if (push) /* Mask is reversed for push.  */
	*valuep |= 0x80 >> regno;
      else
	*valuep |= 1 << regno;

      ++*strp;
      if (**strp == ',')
        {
          if (*(*strp + 1) == ')')
            break;
          ++*strp;
        }
    }

  if (!*strp)
    errmsg = _("Register list is not valid");

  return errmsg;
}

#define POP  0
#define PUSH 1

static const char *
parse_pop_regset (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
		  const char **strp,
		  int opindex ATTRIBUTE_UNUSED,
		  unsigned long *valuep)
{
  return parse_regset (cd, strp, opindex, valuep, POP);
}

static const char *
parse_push_regset (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
		   const char **strp,
		   int opindex ATTRIBUTE_UNUSED,
		   unsigned long *valuep)
{
  return parse_regset (cd, strp, opindex, valuep, PUSH);
}

/* -- dis.c */

const char * m32c_cgen_parse_operand
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
m32c_cgen_parse_operand (CGEN_CPU_DESC cd,
			   int opindex,
			   const char ** strp,
			   CGEN_FIELDS * fields)
{
  const char * errmsg = NULL;
  /* Used by scalar operands that still need to be parsed.  */
  long junk ATTRIBUTE_UNUSED;

  switch (opindex)
    {
    case M32C_OPERAND_A0 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_a0, & junk);
      break;
    case M32C_OPERAND_A1 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_a1, & junk);
      break;
    case M32C_OPERAND_AN16_PUSH_S :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_HI, & fields->f_4_1);
      break;
    case M32C_OPERAND_BIT16AN :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_BIT16RN :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_HI, & fields->f_dst16_rn);
      break;
    case M32C_OPERAND_BIT3_S :
      errmsg = parse_bit3_S (cd, strp, M32C_OPERAND_BIT3_S, (long *) (& fields->f_imm3_S));
      break;
    case M32C_OPERAND_BIT32ANPREFIXED :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_BIT32ANUNPREFIXED :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_BIT32RNPREFIXED :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_QI, & fields->f_dst32_rn_prefixed_QI);
      break;
    case M32C_OPERAND_BIT32RNUNPREFIXED :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_QI, & fields->f_dst32_rn_unprefixed_QI);
      break;
    case M32C_OPERAND_BITBASE16_16_S8 :
      errmsg = parse_signed_bitbase8 (cd, strp, M32C_OPERAND_BITBASE16_16_S8, (long *) (& fields->f_dsp_16_s8));
      break;
    case M32C_OPERAND_BITBASE16_16_U16 :
      errmsg = parse_unsigned_bitbase16 (cd, strp, M32C_OPERAND_BITBASE16_16_U16, (unsigned long *) (& fields->f_dsp_16_u16));
      break;
    case M32C_OPERAND_BITBASE16_16_U8 :
      errmsg = parse_unsigned_bitbase8 (cd, strp, M32C_OPERAND_BITBASE16_16_U8, (unsigned long *) (& fields->f_dsp_16_u8));
      break;
    case M32C_OPERAND_BITBASE16_8_U11_S :
      errmsg = parse_unsigned_bitbase11 (cd, strp, M32C_OPERAND_BITBASE16_8_U11_S, (unsigned long *) (& fields->f_bitbase16_u11_S));
      break;
    case M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED :
      errmsg = parse_signed_bitbase11 (cd, strp, M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED, (long *) (& fields->f_bitbase32_16_s11_unprefixed));
      break;
    case M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED :
      errmsg = parse_signed_bitbase19 (cd, strp, M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED, (long *) (& fields->f_bitbase32_16_s19_unprefixed));
      break;
    case M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED :
      errmsg = parse_unsigned_bitbase11 (cd, strp, M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED, (unsigned long *) (& fields->f_bitbase32_16_u11_unprefixed));
      break;
    case M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED :
      errmsg = parse_unsigned_bitbase19 (cd, strp, M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED, (unsigned long *) (& fields->f_bitbase32_16_u19_unprefixed));
      break;
    case M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED :
      errmsg = parse_unsigned_bitbase27 (cd, strp, M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED, (unsigned long *) (& fields->f_bitbase32_16_u27_unprefixed));
      break;
    case M32C_OPERAND_BITBASE32_24_S11_PREFIXED :
      errmsg = parse_signed_bitbase11 (cd, strp, M32C_OPERAND_BITBASE32_24_S11_PREFIXED, (long *) (& fields->f_bitbase32_24_s11_prefixed));
      break;
    case M32C_OPERAND_BITBASE32_24_S19_PREFIXED :
      errmsg = parse_signed_bitbase19 (cd, strp, M32C_OPERAND_BITBASE32_24_S19_PREFIXED, (long *) (& fields->f_bitbase32_24_s19_prefixed));
      break;
    case M32C_OPERAND_BITBASE32_24_U11_PREFIXED :
      errmsg = parse_unsigned_bitbase11 (cd, strp, M32C_OPERAND_BITBASE32_24_U11_PREFIXED, (unsigned long *) (& fields->f_bitbase32_24_u11_prefixed));
      break;
    case M32C_OPERAND_BITBASE32_24_U19_PREFIXED :
      errmsg = parse_unsigned_bitbase19 (cd, strp, M32C_OPERAND_BITBASE32_24_U19_PREFIXED, (unsigned long *) (& fields->f_bitbase32_24_u19_prefixed));
      break;
    case M32C_OPERAND_BITBASE32_24_U27_PREFIXED :
      errmsg = parse_unsigned_bitbase27 (cd, strp, M32C_OPERAND_BITBASE32_24_U27_PREFIXED, (unsigned long *) (& fields->f_bitbase32_24_u27_prefixed));
      break;
    case M32C_OPERAND_BITNO16R :
      errmsg = parse_Bitno16R (cd, strp, M32C_OPERAND_BITNO16R, (unsigned long *) (& fields->f_dsp_16_u8));
      break;
    case M32C_OPERAND_BITNO32PREFIXED :
      errmsg = cgen_parse_unsigned_integer (cd, strp, M32C_OPERAND_BITNO32PREFIXED, (unsigned long *) (& fields->f_bitno32_prefixed));
      break;
    case M32C_OPERAND_BITNO32UNPREFIXED :
      errmsg = cgen_parse_unsigned_integer (cd, strp, M32C_OPERAND_BITNO32UNPREFIXED, (unsigned long *) (& fields->f_bitno32_unprefixed));
      break;
    case M32C_OPERAND_DSP_10_U6 :
      errmsg = parse_unsigned6 (cd, strp, M32C_OPERAND_DSP_10_U6, (unsigned long *) (& fields->f_dsp_10_u6));
      break;
    case M32C_OPERAND_DSP_16_S16 :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_DSP_16_S16, (long *) (& fields->f_dsp_16_s16));
      break;
    case M32C_OPERAND_DSP_16_S8 :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_DSP_16_S8, (long *) (& fields->f_dsp_16_s8));
      break;
    case M32C_OPERAND_DSP_16_U16 :
      errmsg = parse_unsigned16 (cd, strp, M32C_OPERAND_DSP_16_U16, (unsigned long *) (& fields->f_dsp_16_u16));
      break;
    case M32C_OPERAND_DSP_16_U20 :
      errmsg = parse_unsigned20 (cd, strp, M32C_OPERAND_DSP_16_U20, (unsigned long *) (& fields->f_dsp_16_u24));
      break;
    case M32C_OPERAND_DSP_16_U24 :
      errmsg = parse_unsigned24 (cd, strp, M32C_OPERAND_DSP_16_U24, (unsigned long *) (& fields->f_dsp_16_u24));
      break;
    case M32C_OPERAND_DSP_16_U8 :
      errmsg = parse_unsigned8 (cd, strp, M32C_OPERAND_DSP_16_U8, (unsigned long *) (& fields->f_dsp_16_u8));
      break;
    case M32C_OPERAND_DSP_24_S16 :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_DSP_24_S16, (long *) (& fields->f_dsp_24_s16));
      break;
    case M32C_OPERAND_DSP_24_S8 :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_DSP_24_S8, (long *) (& fields->f_dsp_24_s8));
      break;
    case M32C_OPERAND_DSP_24_U16 :
      errmsg = parse_unsigned16 (cd, strp, M32C_OPERAND_DSP_24_U16, (unsigned long *) (& fields->f_dsp_24_u16));
      break;
    case M32C_OPERAND_DSP_24_U20 :
      errmsg = parse_unsigned20 (cd, strp, M32C_OPERAND_DSP_24_U20, (unsigned long *) (& fields->f_dsp_24_u24));
      break;
    case M32C_OPERAND_DSP_24_U24 :
      errmsg = parse_unsigned24 (cd, strp, M32C_OPERAND_DSP_24_U24, (unsigned long *) (& fields->f_dsp_24_u24));
      break;
    case M32C_OPERAND_DSP_24_U8 :
      errmsg = parse_unsigned8 (cd, strp, M32C_OPERAND_DSP_24_U8, (unsigned long *) (& fields->f_dsp_24_u8));
      break;
    case M32C_OPERAND_DSP_32_S16 :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_DSP_32_S16, (long *) (& fields->f_dsp_32_s16));
      break;
    case M32C_OPERAND_DSP_32_S8 :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_DSP_32_S8, (long *) (& fields->f_dsp_32_s8));
      break;
    case M32C_OPERAND_DSP_32_U16 :
      errmsg = parse_unsigned16 (cd, strp, M32C_OPERAND_DSP_32_U16, (unsigned long *) (& fields->f_dsp_32_u16));
      break;
    case M32C_OPERAND_DSP_32_U20 :
      errmsg = parse_unsigned20 (cd, strp, M32C_OPERAND_DSP_32_U20, (unsigned long *) (& fields->f_dsp_32_u24));
      break;
    case M32C_OPERAND_DSP_32_U24 :
      errmsg = parse_unsigned24 (cd, strp, M32C_OPERAND_DSP_32_U24, (unsigned long *) (& fields->f_dsp_32_u24));
      break;
    case M32C_OPERAND_DSP_32_U8 :
      errmsg = parse_unsigned8 (cd, strp, M32C_OPERAND_DSP_32_U8, (unsigned long *) (& fields->f_dsp_32_u8));
      break;
    case M32C_OPERAND_DSP_40_S16 :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_DSP_40_S16, (long *) (& fields->f_dsp_40_s16));
      break;
    case M32C_OPERAND_DSP_40_S8 :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_DSP_40_S8, (long *) (& fields->f_dsp_40_s8));
      break;
    case M32C_OPERAND_DSP_40_U16 :
      errmsg = parse_unsigned16 (cd, strp, M32C_OPERAND_DSP_40_U16, (unsigned long *) (& fields->f_dsp_40_u16));
      break;
    case M32C_OPERAND_DSP_40_U24 :
      errmsg = parse_unsigned24 (cd, strp, M32C_OPERAND_DSP_40_U24, (unsigned long *) (& fields->f_dsp_40_u24));
      break;
    case M32C_OPERAND_DSP_40_U8 :
      errmsg = parse_unsigned8 (cd, strp, M32C_OPERAND_DSP_40_U8, (unsigned long *) (& fields->f_dsp_40_u8));
      break;
    case M32C_OPERAND_DSP_48_S16 :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_DSP_48_S16, (long *) (& fields->f_dsp_48_s16));
      break;
    case M32C_OPERAND_DSP_48_S8 :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_DSP_48_S8, (long *) (& fields->f_dsp_48_s8));
      break;
    case M32C_OPERAND_DSP_48_U16 :
      errmsg = parse_unsigned16 (cd, strp, M32C_OPERAND_DSP_48_U16, (unsigned long *) (& fields->f_dsp_48_u16));
      break;
    case M32C_OPERAND_DSP_48_U24 :
      errmsg = parse_unsigned24 (cd, strp, M32C_OPERAND_DSP_48_U24, (unsigned long *) (& fields->f_dsp_48_u24));
      break;
    case M32C_OPERAND_DSP_48_U8 :
      errmsg = parse_unsigned8 (cd, strp, M32C_OPERAND_DSP_48_U8, (unsigned long *) (& fields->f_dsp_48_u8));
      break;
    case M32C_OPERAND_DSP_8_S24 :
      errmsg = parse_signed24 (cd, strp, M32C_OPERAND_DSP_8_S24, (long *) (& fields->f_dsp_8_s24));
      break;
    case M32C_OPERAND_DSP_8_S8 :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_DSP_8_S8, (long *) (& fields->f_dsp_8_s8));
      break;
    case M32C_OPERAND_DSP_8_U16 :
      errmsg = parse_unsigned16 (cd, strp, M32C_OPERAND_DSP_8_U16, (unsigned long *) (& fields->f_dsp_8_u16));
      break;
    case M32C_OPERAND_DSP_8_U24 :
      errmsg = parse_unsigned24 (cd, strp, M32C_OPERAND_DSP_8_U24, (unsigned long *) (& fields->f_dsp_8_u24));
      break;
    case M32C_OPERAND_DSP_8_U6 :
      errmsg = parse_unsigned6 (cd, strp, M32C_OPERAND_DSP_8_U6, (unsigned long *) (& fields->f_dsp_8_u6));
      break;
    case M32C_OPERAND_DSP_8_U8 :
      errmsg = parse_unsigned8 (cd, strp, M32C_OPERAND_DSP_8_U8, (unsigned long *) (& fields->f_dsp_8_u8));
      break;
    case M32C_OPERAND_DST16AN :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_DST16AN_S :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_HI, & fields->f_dst16_an_s);
      break;
    case M32C_OPERAND_DST16ANHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_HI, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_DST16ANQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_QI, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_DST16ANQI_S :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_QI, & fields->f_dst16_rn_QI_s);
      break;
    case M32C_OPERAND_DST16ANSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_SI, & fields->f_dst16_an);
      break;
    case M32C_OPERAND_DST16RNEXTQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_ext_QI, & fields->f_dst16_rn_ext);
      break;
    case M32C_OPERAND_DST16RNHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_HI, & fields->f_dst16_rn);
      break;
    case M32C_OPERAND_DST16RNQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_QI, & fields->f_dst16_rn);
      break;
    case M32C_OPERAND_DST16RNQI_S :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r0l_r0h, & fields->f_dst16_rn_QI_s);
      break;
    case M32C_OPERAND_DST16RNSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_SI, & fields->f_dst16_rn);
      break;
    case M32C_OPERAND_DST32ANEXTUNPREFIXED :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32ANPREFIXED :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_HI, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_QI, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_dst32_an_prefixed);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXED :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_HI, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_QI, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_dst32_an_unprefixed);
      break;
    case M32C_OPERAND_DST32R0HI_S :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r0, & junk);
      break;
    case M32C_OPERAND_DST32R0QI_S :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r0l, & junk);
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_ext_HI, & fields->f_dst32_rn_ext_unprefixed);
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_ext_QI, & fields->f_dst32_rn_ext_unprefixed);
      break;
    case M32C_OPERAND_DST32RNPREFIXEDHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_HI, & fields->f_dst32_rn_prefixed_HI);
      break;
    case M32C_OPERAND_DST32RNPREFIXEDQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_QI, & fields->f_dst32_rn_prefixed_QI);
      break;
    case M32C_OPERAND_DST32RNPREFIXEDSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_SI, & fields->f_dst32_rn_prefixed_SI);
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_HI, & fields->f_dst32_rn_unprefixed_HI);
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_QI, & fields->f_dst32_rn_unprefixed_QI);
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_SI, & fields->f_dst32_rn_unprefixed_SI);
      break;
    case M32C_OPERAND_G :
      errmsg = parse_G (cd, strp, M32C_OPERAND_G, (long *) (& junk));
      break;
    case M32C_OPERAND_IMM_12_S4 :
      errmsg = parse_signed4 (cd, strp, M32C_OPERAND_IMM_12_S4, (long *) (& fields->f_imm_12_s4));
      break;
    case M32C_OPERAND_IMM_12_S4N :
      errmsg = parse_signed4n (cd, strp, M32C_OPERAND_IMM_12_S4N, (long *) (& fields->f_imm_12_s4));
      break;
    case M32C_OPERAND_IMM_13_U3 :
      errmsg = parse_signed4 (cd, strp, M32C_OPERAND_IMM_13_U3, (long *) (& fields->f_imm_13_u3));
      break;
    case M32C_OPERAND_IMM_16_HI :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_IMM_16_HI, (long *) (& fields->f_dsp_16_s16));
      break;
    case M32C_OPERAND_IMM_16_QI :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_IMM_16_QI, (long *) (& fields->f_dsp_16_s8));
      break;
    case M32C_OPERAND_IMM_16_SI :
      errmsg = parse_signed32 (cd, strp, M32C_OPERAND_IMM_16_SI, (long *) (& fields->f_dsp_16_s32));
      break;
    case M32C_OPERAND_IMM_20_S4 :
      errmsg = parse_signed4 (cd, strp, M32C_OPERAND_IMM_20_S4, (long *) (& fields->f_imm_20_s4));
      break;
    case M32C_OPERAND_IMM_24_HI :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_IMM_24_HI, (long *) (& fields->f_dsp_24_s16));
      break;
    case M32C_OPERAND_IMM_24_QI :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_IMM_24_QI, (long *) (& fields->f_dsp_24_s8));
      break;
    case M32C_OPERAND_IMM_24_SI :
      errmsg = parse_signed32 (cd, strp, M32C_OPERAND_IMM_24_SI, (long *) (& fields->f_dsp_24_s32));
      break;
    case M32C_OPERAND_IMM_32_HI :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_IMM_32_HI, (long *) (& fields->f_dsp_32_s16));
      break;
    case M32C_OPERAND_IMM_32_QI :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_IMM_32_QI, (long *) (& fields->f_dsp_32_s8));
      break;
    case M32C_OPERAND_IMM_32_SI :
      errmsg = parse_signed32 (cd, strp, M32C_OPERAND_IMM_32_SI, (long *) (& fields->f_dsp_32_s32));
      break;
    case M32C_OPERAND_IMM_40_HI :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_IMM_40_HI, (long *) (& fields->f_dsp_40_s16));
      break;
    case M32C_OPERAND_IMM_40_QI :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_IMM_40_QI, (long *) (& fields->f_dsp_40_s8));
      break;
    case M32C_OPERAND_IMM_40_SI :
      errmsg = parse_signed32 (cd, strp, M32C_OPERAND_IMM_40_SI, (long *) (& fields->f_dsp_40_s32));
      break;
    case M32C_OPERAND_IMM_48_HI :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_IMM_48_HI, (long *) (& fields->f_dsp_48_s16));
      break;
    case M32C_OPERAND_IMM_48_QI :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_IMM_48_QI, (long *) (& fields->f_dsp_48_s8));
      break;
    case M32C_OPERAND_IMM_48_SI :
      errmsg = parse_signed32 (cd, strp, M32C_OPERAND_IMM_48_SI, (long *) (& fields->f_dsp_48_s32));
      break;
    case M32C_OPERAND_IMM_56_HI :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_IMM_56_HI, (long *) (& fields->f_dsp_56_s16));
      break;
    case M32C_OPERAND_IMM_56_QI :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_IMM_56_QI, (long *) (& fields->f_dsp_56_s8));
      break;
    case M32C_OPERAND_IMM_64_HI :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_IMM_64_HI, (long *) (& fields->f_dsp_64_s16));
      break;
    case M32C_OPERAND_IMM_8_HI :
      errmsg = parse_signed16 (cd, strp, M32C_OPERAND_IMM_8_HI, (long *) (& fields->f_dsp_8_s16));
      break;
    case M32C_OPERAND_IMM_8_QI :
      errmsg = parse_signed8 (cd, strp, M32C_OPERAND_IMM_8_QI, (long *) (& fields->f_dsp_8_s8));
      break;
    case M32C_OPERAND_IMM_8_S4 :
      errmsg = parse_signed4 (cd, strp, M32C_OPERAND_IMM_8_S4, (long *) (& fields->f_imm_8_s4));
      break;
    case M32C_OPERAND_IMM_8_S4N :
      errmsg = parse_signed4n (cd, strp, M32C_OPERAND_IMM_8_S4N, (long *) (& fields->f_imm_8_s4));
      break;
    case M32C_OPERAND_IMM_SH_12_S4 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_shimm, & fields->f_imm_12_s4);
      break;
    case M32C_OPERAND_IMM_SH_20_S4 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_shimm, & fields->f_imm_20_s4);
      break;
    case M32C_OPERAND_IMM_SH_8_S4 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_shimm, & fields->f_imm_8_s4);
      break;
    case M32C_OPERAND_IMM1_S :
      errmsg = parse_imm1_S (cd, strp, M32C_OPERAND_IMM1_S, (long *) (& fields->f_imm1_S));
      break;
    case M32C_OPERAND_IMM3_S :
      errmsg = parse_imm3_S (cd, strp, M32C_OPERAND_IMM3_S, (long *) (& fields->f_imm3_S));
      break;
    case M32C_OPERAND_LAB_16_8 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, M32C_OPERAND_LAB_16_8, 0, NULL,  & value);
        fields->f_lab_16_8 = value;
      }
      break;
    case M32C_OPERAND_LAB_24_8 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, M32C_OPERAND_LAB_24_8, 0, NULL,  & value);
        fields->f_lab_24_8 = value;
      }
      break;
    case M32C_OPERAND_LAB_32_8 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, M32C_OPERAND_LAB_32_8, 0, NULL,  & value);
        fields->f_lab_32_8 = value;
      }
      break;
    case M32C_OPERAND_LAB_40_8 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, M32C_OPERAND_LAB_40_8, 0, NULL,  & value);
        fields->f_lab_40_8 = value;
      }
      break;
    case M32C_OPERAND_LAB_5_3 :
      {
        bfd_vma value = 0;
        errmsg = parse_lab_5_3 (cd, strp, M32C_OPERAND_LAB_5_3, 0, NULL,  & value);
        fields->f_lab_5_3 = value;
      }
      break;
    case M32C_OPERAND_LAB_8_16 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, M32C_OPERAND_LAB_8_16, 0, NULL,  & value);
        fields->f_lab_8_16 = value;
      }
      break;
    case M32C_OPERAND_LAB_8_24 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, M32C_OPERAND_LAB_8_24, 0, NULL,  & value);
        fields->f_lab_8_24 = value;
      }
      break;
    case M32C_OPERAND_LAB_8_8 :
      {
        bfd_vma value = 0;
        errmsg = cgen_parse_address (cd, strp, M32C_OPERAND_LAB_8_8, 0, NULL,  & value);
        fields->f_lab_8_8 = value;
      }
      break;
    case M32C_OPERAND_LAB32_JMP_S :
      {
        bfd_vma value = 0;
        errmsg = parse_lab_5_3 (cd, strp, M32C_OPERAND_LAB32_JMP_S, 0, NULL,  & value);
        fields->f_lab32_jmp_s = value;
      }
      break;
    case M32C_OPERAND_Q :
      errmsg = parse_Q (cd, strp, M32C_OPERAND_Q, (long *) (& junk));
      break;
    case M32C_OPERAND_R0 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r0, & junk);
      break;
    case M32C_OPERAND_R0H :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r0h, & junk);
      break;
    case M32C_OPERAND_R0L :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r0l, & junk);
      break;
    case M32C_OPERAND_R1 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r1, & junk);
      break;
    case M32C_OPERAND_R1R2R0 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r1r2r0, & junk);
      break;
    case M32C_OPERAND_R2 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r2, & junk);
      break;
    case M32C_OPERAND_R2R0 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r2r0, & junk);
      break;
    case M32C_OPERAND_R3 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r3, & junk);
      break;
    case M32C_OPERAND_R3R1 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_r3r1, & junk);
      break;
    case M32C_OPERAND_REGSETPOP :
      errmsg = parse_pop_regset (cd, strp, M32C_OPERAND_REGSETPOP, (unsigned long *) (& fields->f_8_8));
      break;
    case M32C_OPERAND_REGSETPUSH :
      errmsg = parse_push_regset (cd, strp, M32C_OPERAND_REGSETPUSH, (unsigned long *) (& fields->f_8_8));
      break;
    case M32C_OPERAND_RN16_PUSH_S :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_QI, & fields->f_4_1);
      break;
    case M32C_OPERAND_S :
      errmsg = parse_S (cd, strp, M32C_OPERAND_S, (long *) (& junk));
      break;
    case M32C_OPERAND_SRC16AN :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_src16_an);
      break;
    case M32C_OPERAND_SRC16ANHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_HI, & fields->f_src16_an);
      break;
    case M32C_OPERAND_SRC16ANQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_QI, & fields->f_src16_an);
      break;
    case M32C_OPERAND_SRC16RNHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_HI, & fields->f_src16_rn);
      break;
    case M32C_OPERAND_SRC16RNQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_QI, & fields->f_src16_rn);
      break;
    case M32C_OPERAND_SRC32ANPREFIXED :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_src32_an_prefixed);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_HI, & fields->f_src32_an_prefixed);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_QI, & fields->f_src32_an_prefixed);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_src32_an_prefixed);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXED :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_src32_an_unprefixed);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_HI, & fields->f_src32_an_unprefixed);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar_QI, & fields->f_src32_an_unprefixed);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_ar, & fields->f_src32_an_unprefixed);
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_HI, & fields->f_src32_rn_prefixed_HI);
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_QI, & fields->f_src32_rn_prefixed_QI);
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_SI, & fields->f_src32_rn_prefixed_SI);
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDHI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_HI, & fields->f_src32_rn_unprefixed_HI);
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDQI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_QI, & fields->f_src32_rn_unprefixed_QI);
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDSI :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_gr_SI, & fields->f_src32_rn_unprefixed_SI);
      break;
    case M32C_OPERAND_SRCDST16_R0L_R0H_S_NORMAL :
      errmsg = parse_r0l_r0h (cd, strp, M32C_OPERAND_SRCDST16_R0L_R0H_S_NORMAL, (long *) (& fields->f_5_1));
      break;
    case M32C_OPERAND_X :
      errmsg = parse_X (cd, strp, M32C_OPERAND_X, (long *) (& junk));
      break;
    case M32C_OPERAND_Z :
      errmsg = parse_Z (cd, strp, M32C_OPERAND_Z, (long *) (& junk));
      break;
    case M32C_OPERAND_COND16_16 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond16, & fields->f_dsp_16_u8);
      break;
    case M32C_OPERAND_COND16_24 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond16, & fields->f_dsp_24_u8);
      break;
    case M32C_OPERAND_COND16_32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond16, & fields->f_dsp_32_u8);
      break;
    case M32C_OPERAND_COND16C :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond16c, & fields->f_cond16);
      break;
    case M32C_OPERAND_COND16J :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond16j, & fields->f_cond16);
      break;
    case M32C_OPERAND_COND16J5 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond16j_5, & fields->f_cond16j_5);
      break;
    case M32C_OPERAND_COND32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond32, & fields->f_cond32);
      break;
    case M32C_OPERAND_COND32_16 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond32, & fields->f_dsp_16_u8);
      break;
    case M32C_OPERAND_COND32_24 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond32, & fields->f_dsp_24_u8);
      break;
    case M32C_OPERAND_COND32_32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond32, & fields->f_dsp_32_u8);
      break;
    case M32C_OPERAND_COND32_40 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond32, & fields->f_dsp_40_u8);
      break;
    case M32C_OPERAND_COND32J :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond32, & fields->f_cond32j);
      break;
    case M32C_OPERAND_CR1_PREFIXED_32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cr1_32, & fields->f_21_3);
      break;
    case M32C_OPERAND_CR1_UNPREFIXED_32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cr1_32, & fields->f_13_3);
      break;
    case M32C_OPERAND_CR16 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cr_16, & fields->f_9_3);
      break;
    case M32C_OPERAND_CR2_32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cr2_32, & fields->f_13_3);
      break;
    case M32C_OPERAND_CR3_PREFIXED_32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cr3_32, & fields->f_21_3);
      break;
    case M32C_OPERAND_CR3_UNPREFIXED_32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cr3_32, & fields->f_13_3);
      break;
    case M32C_OPERAND_FLAGS16 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_flags, & fields->f_9_3);
      break;
    case M32C_OPERAND_FLAGS32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_flags, & fields->f_13_3);
      break;
    case M32C_OPERAND_SCCOND32 :
      errmsg = cgen_parse_keyword (cd, strp, & m32c_cgen_opval_h_cond32, & fields->f_cond16);
      break;
    case M32C_OPERAND_SIZE :
      errmsg = parse_size (cd, strp, M32C_OPERAND_SIZE, (long *) (& junk));
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while parsing.\n"), opindex);
      abort ();
  }

  return errmsg;
}

cgen_parse_fn * const m32c_cgen_parse_handlers[] = 
{
  parse_insn_normal,
};

void
m32c_cgen_init_asm (CGEN_CPU_DESC cd)
{
  m32c_cgen_init_opcode_table (cd);
  m32c_cgen_init_ibld_table (cd);
  cd->parse_handlers = & m32c_cgen_parse_handlers[0];
  cd->parse_operand = m32c_cgen_parse_operand;
}



/* Regex construction routine.

   This translates an opcode syntax string into a regex string,
   by replacing any non-character syntax element (such as an
   opcode) with the pattern '.*'

   It then compiles the regex and stores it in the opcode, for
   later use by m32c_cgen_assemble_insn

   Returns NULL for success, an error message for failure.  */

char * 
m32c_cgen_build_insn_regex (CGEN_INSN *insn)
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
m32c_cgen_assemble_insn (CGEN_CPU_DESC cd,
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
      if (! m32c_cgen_insn_supported (cd, insn))
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
